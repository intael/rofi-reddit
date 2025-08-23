#include "reddit.h"
#include "curl_wrappers.h"
#include "memory.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/urlapi.h>
#include <glib.h>
#include <jansson.h>
#include <pwd.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <tomlc17.h>
#include <unistd.h>

static const char* const HTTPS_SCHEME = "https://";
static const char* const REDDIT_HOST = "www.reddit.com";
static const char* const REDDIT_API_HOST = "oauth.reddit.com";

static const uint16_t* const ACCESS_TOKEN_MAX_SIZE = &(const uint16_t){1024};

static bool is_auth_filled(const struct app_auth* auth) {
    if (!auth || !auth->client_id || !auth->client_secret)
        return false;
    // client_name is not critical for auth purposes
    return auth->client_id[0] != '\0' && auth->client_secret[0] != '\0';
}

static struct app_auth* new_app_auth(toml_result_t toml) {
    struct app_auth* auth = (struct app_auth*)LOG_ERR_MALLOC(struct app_auth, 1);
    auth->client_name = strdup(toml_seek(toml.toptab, "reddit.client_name").u.s);
    auth->client_id = strdup(toml_seek(toml.toptab, "reddit.client_id").u.s);
    auth->client_secret = strdup(toml_seek(toml.toptab, "reddit.client_secret").u.s);
    return auth;
}

static void free_app_auth(struct app_auth* auth) {
    if (!auth)
        return;
    free(auth->client_name);
    free(auth->client_id);
    free(auth->client_secret);
    free(auth);
}

static int create_dir_if_not_exists(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, S_IRWXU) != 0) {
            return errno;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        return ENOTDIR;
    }
    return 0;
}

struct rofi_reddit_paths* new_rofi_reddit_paths() {
    char* plugin_cfg_dir = g_build_filename("/", "usr", "share", "rofi-reddit", NULL);
    struct stat cfg_dir_stat;
    if (stat(plugin_cfg_dir, &cfg_dir_stat) != 0 || !S_ISDIR(cfg_dir_stat.st_mode)) {
        fprintf(stderr, "Rofi Reddit config directory does not exist. This is a symptom of the installing process "
                        "having gone wrong. Try reinstalling. Aborting.\n");
        free(plugin_cfg_dir);
        exit(EXIT_FAILURE);
    }
    char* config_file_path = g_build_filename(plugin_cfg_dir, "config.toml", NULL);
    free(plugin_cfg_dir);
    if (access(config_file_path, F_OK) != 0 || access(config_file_path, R_OK) != 0) {
        fprintf(
            stderr,
            "Rofi Reddit config file does not exist or lacks read permissions at %s. Check permissions. Aborting.\n",
            config_file_path);
        free(config_file_path);
        exit(EXIT_FAILURE);
    }
    struct rofi_reddit_paths* paths = LOG_ERR_MALLOC(struct rofi_reddit_paths, 1);
    paths->config_path = config_file_path;
    char* xdg_cache = getenv("XDG_CACHE_HOME");
    char* user_cache_dir =
        xdg_cache && xdg_cache[0] != '\0' ? xdg_cache : g_build_filename(getenv("HOME"), ".cache", NULL);
    char* plugin_cache_dir = g_build_filename(user_cache_dir, "rofi-reddit", NULL);
    free(user_cache_dir);
    if (create_dir_if_not_exists(plugin_cache_dir) != 0) {
        fprintf(stderr,
                "Failed to create or access cache directory at %s. Check permissions. This will lead to more Reddit "
                "API calls than necessary.\n",
                plugin_cache_dir);
        free(plugin_cache_dir);
        free_rofi_reddit_paths(paths);
        exit(EXIT_FAILURE);
    }
    paths->access_token_cache_path = g_build_filename(plugin_cache_dir, "access_token", NULL);
    struct stat access_token_cache_stat;
    // access token exists, process has read permissions and file is nonempty
    paths->access_token_cache_exists = stat(paths->access_token_cache_path, &access_token_cache_stat) == 0 &&
                                       access(paths->access_token_cache_path, R_OK) == 0 && cfg_dir_stat.st_size > 0;

    free(plugin_cache_dir);
    free(xdg_cache);
    return paths;
}

void free_rofi_reddit_paths(const struct rofi_reddit_paths* paths) {
    if (!paths)
        return;
    free((void*)paths->config_path);
    free((void*)paths->access_token_cache_path);
    free((void*)paths);
}

struct rofi_reddit_cfg* new_rofi_reddit_cfg(struct rofi_reddit_paths* paths) {
    if (access(paths->config_path, F_OK) != 0 || access(paths->config_path, R_OK) != 0) {
        fprintf(
            stderr,
            "Could not read rofi-reddit config file at %s. Either it doesn't exist or permissions are insufficient.\n",
            paths->config_path);
        return NULL;
    }
    struct rofi_reddit_cfg* cfg = (struct rofi_reddit_cfg*)LOG_ERR_MALLOC(struct rofi_reddit_cfg, 1);
    toml_result_t parsed_toml = toml_parse_file_ex(paths->config_path);
    if (!parsed_toml.ok) {
        fprintf(stderr, "Failed to parse config file: %s\n", parsed_toml.errmsg);
        free_rofi_reddit_cfg(cfg);
        return NULL;
    }
    struct app_auth* auth = new_app_auth(parsed_toml);
    if (!is_auth_filled(auth)) {
        fprintf(
            stderr,
            "Rofi Reddit config file is missing critical fields. Please fill in client_id and client_secret at %s\n",
            paths->config_path);
        free_app_auth(auth);
        free_rofi_reddit_cfg(cfg);
        toml_free(parsed_toml);
        return NULL;
    }
    toml_free(parsed_toml);
    cfg->auth = auth;
    cfg->paths = paths;
    return cfg;
}

void free_rofi_reddit_cfg(const struct rofi_reddit_cfg* cfg) {
    if (!cfg)
        return;
    if (!cfg->paths)
        free_rofi_reddit_paths(cfg->paths);
    if (!cfg->auth)
        free_app_auth(cfg->auth);
    free((void*)cfg);
}

RedditApp* new_reddit_app(struct rofi_reddit_cfg* config) {
    RedditApp* app = (RedditApp*)LOG_ERR_MALLOC(RedditApp, 1);
    app->config = config;
    app->http_client = curl_easy_init();
    if (!app->http_client) {
        fprintf(stderr, "Failed to initialize CURL.\n");
        free_reddit_app(app);
        return NULL;
    }
    return app;
}
void free_reddit_app(RedditApp* app) {
    if (!app)
        return;
    curl_easy_cleanup(app->http_client);
    free_rofi_reddit_cfg(app->config);
    free(app);
}

static size_t write_callback(char* buffer, size_t chunks, size_t chunk_size, void* stream) {
    struct response_buffer* resp = (struct response_buffer*)stream;
    size_t realsize = chunks * chunk_size;
    char* ptr = realloc(resp->buffer, resp->size + realsize + 1);
    resp->buffer = ptr;
    memcpy(&(resp->buffer[resp->size]), buffer, realsize);
    resp->size += realsize;
    resp->buffer[resp->size] = 0;
    return realsize;
}

static struct curl_slist* user_agent_header(const RedditApp* const app) {
    const char* ua_header_key = "User-Agent";
    size_t header_size = strlen(ua_header_key) + strlen(app->config->auth->client_name) + 1;
    char* ua_header = (char*)LOG_ERR_MALLOC(char, header_size);
    snprintf(ua_header, header_size, "%s: %s", ua_header_key, app->config->auth->client_name);
    struct curl_slist* headers = curl_slist_append(NULL, ua_header);
    return headers;
}

static json_t* deserialize_json_response(const struct response_buffer* resp) {
    json_error_t error;
    json_t* root = json_loads(resp->buffer, 0, &error);
    if (!root) {
        fprintf(stderr, "Error deserializing JSON: %s\n", error.text);
        return NULL;
    }
    return root;
}

static RedditAccessToken* deserialize_access_token(const struct response_buffer* resp) {
    json_t* payload = deserialize_json_response(resp);
    if (payload) {
        RedditAccessToken* token = (RedditAccessToken*)LOG_ERR_MALLOC(RedditAccessToken, 1);
        token->token = strdup(json_string_value(json_object_get(payload, "access_token")));
        json_decref(payload);
        return token;
    }
    return NULL;
}

void deserialize_listing(json_t* listing_json, struct listing* deserialize_to, size_t index) {
    json_t* data = json_object_get(listing_json, "data");
    if (!data || !json_is_object(data) || !json_string_value(json_object_get(data, "title"))) {
        fprintf(stderr, "No data found for listing.\n");
        return;
    }
    struct listing* item = deserialize_to + index;
    item->title = strdup(json_string_value(json_object_get(data, "title")));

    const char* selftext_val = json_string_value(json_object_get(data, "selftext"));
    item->selftext = selftext_val ? strdup(selftext_val) : NULL;

    json_t* ups_json = json_object_get(data, "ups");
    item->ups = (ups_json && json_is_integer(ups_json)) ? (uint32_t)json_integer_value(ups_json) : 0;

    const char* permalink_val = json_string_value(json_object_get(data, "permalink"));
    const char* path_val = permalink_val ? permalink_val : json_string_value(json_object_get(data, "url"));

    item->url = NULL;
    if (path_val) {
        size_t len = strlen(HTTPS_SCHEME) + strlen(REDDIT_HOST) + strlen(path_val) + 1;
        char* parsed_url = LOG_ERR_MALLOC(char, len);
        snprintf(parsed_url, len, "%s%s%s", HTTPS_SCHEME, REDDIT_HOST, path_val);
        item->url = parsed_url;
    } else {
        fprintf(stderr, "No URL or permalink found for listing.\n");
    }
}

struct listings* deserialize_listings(const struct response_buffer* resp) {
    json_t* payload = deserialize_json_response(resp);
    if (payload) {
        json_t* listing_payloads = json_object_get(json_object_get(payload, "data"), "children");
        size_t count = json_array_size(listing_payloads);
        struct listings* reddit_listings = (struct listings*)LOG_ERR_MALLOC(struct listings, 1);
        struct listing* items = (struct listing*)LOG_ERR_MALLOC(struct listing, count);
        for (size_t i = 0; i < count; i++) {
            json_t* listing_json = json_array_get(listing_payloads, i);
            deserialize_listing(listing_json, items, i);
        }
        reddit_listings->count = count;
        reddit_listings->items = items;
        json_decref(payload);
        return reddit_listings;
    }
    return NULL;
}

void free_listing(const struct listing* listing) {
    if (!listing)
        return;
    free(listing->title);
    free(listing->selftext);
    free(listing->url);
}

void free_listings(const struct listings* listings) {
    if (!listings)
        return;
    for (size_t i = 0; i < listings->count; i++) {
        free_listing(&listings->items[i]);
    }
    free((void*)listings->items);
    free((void*)listings);
}

const struct reddit_api_response* fetch_reddit_access_token_from_api(const RedditApp* app) {
    curl_easy_reset(app->http_client);
    struct response_buffer* buffer = new_response_buffer();
    struct curl_slist* ua_header = user_agent_header(app);

    CURL* url = curl_url();
    curl_url_set(url, CURLUPART_SCHEME, "https", 0);
    curl_url_set(url, CURLUPART_HOST, REDDIT_HOST, 0);
    curl_url_set(url, CURLUPART_PATH, "api/v1/access_token/", 0);
    char* url_str = NULL;
    curl_url_get(url, CURLUPART_URL, &url_str, 0);

    curl_easy_setopt(app->http_client, CURLOPT_POST, 1L);
    curl_easy_setopt(app->http_client, CURLOPT_USERNAME, app->config->auth->client_id);
    curl_easy_setopt(app->http_client, CURLOPT_PASSWORD, app->config->auth->client_secret);
    curl_easy_setopt(app->http_client, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(app->http_client, CURLOPT_WRITEDATA, buffer);
    curl_easy_setopt(app->http_client, CURLOPT_URL, url_str);
    curl_easy_setopt(app->http_client, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(app->http_client, CURLOPT_HTTPHEADER, ua_header);
    curl_easy_setopt(app->http_client, CURLOPT_POSTFIELDS, "scope=read&grant_type=client_credentials");
    // curl_easy_setopt(app->http_client, CURLOPT_VERBOSE, 1L);

    curl_easy_perform(app->http_client);
    long* resp_status = get_response_status(app->http_client);
    curl_slist_free_all(ua_header);
    curl_url_cleanup(url);
    curl_free(url_str);
    return new_reddit_api_response(buffer, resp_status);
}

RedditAccessToken* fetch_and_cache_token(RedditApp* app) {
    const struct reddit_api_response* response = fetch_reddit_access_token_from_api(app);
    if (response->status_code == HTTP_OK) {
        RedditAccessToken* token = deserialize_access_token(response->response_buffer);
        fprintf(stdout, "Obtained access token from API of size: %zu. Caching to %s\n", strlen(token->token),
                app->config->paths->access_token_cache_path);
        FILE* const CACHE = fopen(app->config->paths->access_token_cache_path, "w+");
        if (!CACHE) {
            fprintf(stderr, "Failed to create or open access token cache file for writing at: %s\n",
                    app->config->paths->access_token_cache_path);
            free_reddit_api_response(response);
            free_reddit_access_token(token);
            free_reddit_app(app);
            exit(EXIT_FAILURE);
        }
        fputs(token->token, CACHE);
        fclose(CACHE);
        app->config->paths = new_rofi_reddit_paths();
        return token;
    }
    return NULL;
}

RedditAccessToken* new_reddit_access_token(RedditApp* app) {
    RedditAccessToken* reddit_token = NULL;
    if (app->config->paths->access_token_cache_exists) {
        FILE* const CACHE = fopen(app->config->paths->access_token_cache_path, "r");
        if (!CACHE) {
            fprintf(stderr, "Failed to open access token cache file for reading: %s\n",
                    app->config->paths->access_token_cache_path);
            free_reddit_app(app);
            exit(EXIT_FAILURE);
        }
        char* buffer = malloc(*ACCESS_TOKEN_MAX_SIZE);
        fgets(buffer, *ACCESS_TOKEN_MAX_SIZE, CACHE);
        RedditAccessToken* cached_token = LOG_ERR_MALLOC(RedditAccessToken, 1);
        cached_token->token = buffer;
        reddit_token = cached_token;
        fprintf(stdout, "Access token cache hit.\n");
        fclose(CACHE);
    } else {
        fprintf(stdout, "Access token cache miss. Fetching from API.\n");
        reddit_token = fetch_and_cache_token(app);
    }
    if (!reddit_token)
        perror("Failed to obtain Reddit access token.\n");
    return reddit_token;
}

void free_reddit_access_token(const RedditAccessToken* token) {
    if (!token)
        return;
    free((void*)token->token);
    free((void*)token);
}

const struct reddit_api_response* fetch_hot_listings(const RedditApp* app, const RedditAccessToken* token,
                                                     const char* subreddit) {
    curl_easy_reset(app->http_client);
    struct response_buffer* response_buffer = new_response_buffer();
    struct curl_slist* ua_header = user_agent_header(app);

    CURL* url = curl_url();
    curl_url_set(url, CURLUPART_SCHEME, "https", 0);
    curl_url_set(url, CURLUPART_HOST, REDDIT_API_HOST, 0);
    char url_path[100];
    snprintf(url_path, 100, "r/%s/hot/", subreddit);
    curl_url_set(url, CURLUPART_PATH, url_path, 0);
    curl_url_set(url, CURLUPART_QUERY, "limit=15", 0); // TODO: make configurable
    char* url_str = NULL;
    curl_url_get(url, CURLUPART_URL, &url_str, 0);

    curl_easy_setopt(app->http_client, CURLOPT_POST, 0L);
    curl_easy_setopt(app->http_client, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(app->http_client, CURLOPT_WRITEDATA, response_buffer);
    curl_easy_setopt(app->http_client, CURLOPT_URL, url_str);
    curl_easy_setopt(app->http_client, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
    curl_easy_setopt(app->http_client, CURLOPT_XOAUTH2_BEARER, token->token);
    curl_easy_setopt(app->http_client, CURLOPT_HTTPHEADER, ua_header);
    curl_easy_setopt(app->http_client, CURLOPT_FOLLOWLOCATION, 1L);
    // curl_easy_setopt(app->http_client, CURLOPT_VERBOSE, 1L);

    curl_easy_perform(app->http_client);

    long* resp_status = get_response_status(app->http_client);

    curl_slist_free_all(ua_header);
    curl_url_cleanup(url);
    curl_free(url_str);
    return new_reddit_api_response(response_buffer, resp_status);
}

struct reddit_api_response* new_reddit_api_response(struct response_buffer* response, long* status_code) {
    struct reddit_api_response* reddit_response =
        (struct reddit_api_response*)LOG_ERR_MALLOC(struct reddit_api_response, 1);
    reddit_response->status_code = http_status_code_from(*status_code);
    reddit_response->response_buffer = response;
    return reddit_response;
}

void free_reddit_api_response(const struct reddit_api_response* response) {
    if (!response)
        return;
    // pointer to data is owned by the caller, so we don't free it here
    free((void*)response);
}
