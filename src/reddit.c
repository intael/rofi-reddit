#include "src/reddit.h"
#include "src/curl_wrappers.h"
#include "src/memory.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/urlapi.h>
#include <glib.h>
#include <jansson.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tomlc17.h>
#include <unistd.h>

static const char* const REDDIT_HOST = "www.reddit.com";
static const char* const REDDIT_API_HOST = "oauth.reddit.com";

static const uint16_t* const ACCESS_TOKEN_MAX_SIZE = &(const uint16_t){1024};

const struct rofi_reddit_paths* new_rofi_reddit_paths() {
    const char* home_path = getenv("HOME");
    if (!home_path) {
        fprintf(stderr, "HOME env variable is not available.");
        return NULL;
    }
    struct rofi_reddit_paths* paths =
        (struct rofi_reddit_paths*)malloc(sizeof(struct rofi_reddit_paths));
    paths->dir_path = g_build_filename(home_path, ".config", "rofi_reddit", NULL);
    paths->dir_exists = access(paths->dir_path, F_OK) == 0;
    paths->config_path = g_build_filename(paths->dir_path, "config.toml", NULL);
    paths->config_path_exists = access(paths->config_path, F_OK) == 0;
    paths->access_token_cache_path = g_build_filename(paths->dir_path, "access_token", NULL);
    paths->access_token_cache_exists = access(paths->access_token_cache_path, F_OK) == 0;
    return paths;
}

void free_rofi_reddit_paths(const struct rofi_reddit_paths* paths) {
    if (!paths)
        return;
    free((void*)paths->dir_path);
    free((void*)paths->config_path);
    free((void*)paths->access_token_cache_path);
    free((void*)paths);
}

const struct rofi_reddit_cfg* new_rofi_reddit_cfg(const struct rofi_reddit_paths* paths) {
    struct rofi_reddit_cfg* cfg =
        (struct rofi_reddit_cfg*)LOG_ERR_MALLOC(struct rofi_reddit_cfg, 2);
    if (!paths->config_path_exists) {
        fprintf(stderr, "Could not find rofi_reddit config file at %s\n", paths->config_path);
        return NULL;
    }
    if (access(paths->config_path, F_OK) == -1) {
        fprintf(stderr,
                "Could not read rofi_reddit config file due to missing read permissions at %s\n",
                paths->config_path);
        return NULL;
    }
    toml_result_t parsed_toml = toml_parse_file_ex(paths->config_path);
    if (!parsed_toml.ok) {
        fprintf(stderr, "Failed to parse config file: %s\n", parsed_toml.errmsg);
        free(cfg);
        return NULL;
    }
    struct app_auth* auth = (struct app_auth*)LOG_ERR_MALLOC(struct app_auth, 2);
    auth->client_name = strdup(toml_seek(parsed_toml.toptab, "reddit.client_name").u.s);
    auth->client_id = strdup(toml_seek(parsed_toml.toptab, "reddit.client_id").u.s);
    auth->client_secret = strdup(toml_seek(parsed_toml.toptab, "reddit.client_secret").u.s);
    toml_free(parsed_toml);
    cfg->auth = auth;
    return cfg;
}

void free_rofi_reddit_cfg(const struct rofi_reddit_cfg* cfg) {
    if (!cfg)
        return;
    if (cfg->auth) {
        free((void*)cfg->auth->client_name);
        free((void*)cfg->auth->client_id);
        free((void*)cfg->auth->client_secret);
        free((void*)cfg->auth);
    }
    free((void*)cfg);
}

const RedditApp* new_reddit_app(const struct rofi_reddit_cfg* config) {
    RedditApp* app = (RedditApp*)LOG_ERR_MALLOC(RedditApp, 1);
    app->auth = config->auth;
    app->http_client = curl_easy_init();
    if (!app->http_client) {
        fprintf(stderr, "Failed to initialize CURL.\n");
        free(app);
        return NULL;
    }
    return app;
}
void free_reddit_app(RedditApp* app) {
    if (!app)
        return;
    curl_easy_cleanup(app->http_client);
    free(app);
}

static size_t write_callback(char* buffer, size_t chunks, size_t chunk_size, void* stream) {
    struct response* resp = (struct response*)stream;
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
    size_t header_size = strlen(ua_header_key) + strlen(app->auth->client_name) + 1;
    char* ua_header = (char*)LOG_ERR_MALLOC(char, header_size);
    snprintf(ua_header, header_size, "%s: %s", ua_header_key, app->auth->client_name);
    struct curl_slist* headers = curl_slist_append(NULL, ua_header);
    return headers;
}

static json_t* deserialize_json_response(const struct response* resp) {
    json_error_t error;
    json_t* root = json_loads(resp->buffer, 0, &error);
    if (!root) {
        fprintf(stderr, "Error deserializing JSON: %s\n", error.text);
        return NULL;
    }
    return root;
}

static const RedditAccessToken* deserialize_access_token(struct response* resp) {
    json_t* payload = deserialize_json_response(resp);
    if (payload) {
        RedditAccessToken* token = (RedditAccessToken*)LOG_ERR_MALLOC(RedditAccessToken, 1);
        token->token = strdup(json_string_value(json_object_get(payload, "access_token")));
        json_decref(payload);
        return token;
    }
    return NULL;
}

static const struct listings* deserialize_listings(const struct response* resp) {
    json_t* payload = deserialize_json_response(resp);
    if (payload) {
        json_t* listing_payloads = json_object_get(json_object_get(payload, "data"), "children");
        size_t count = json_array_size(listing_payloads);
        struct listings* reddit_listings = (struct listings*)LOG_ERR_MALLOC(struct listings, 1);
        struct listing* items = (struct listing*)LOG_ERR_MALLOC(struct listing, count);
        for (size_t i = 0; i < count; i++) {
            json_t* listing_json = json_array_get(listing_payloads, i);
            json_t* data = json_object_get(listing_json, "data");
            struct listing* item = (struct listing*)LOG_ERR_MALLOC(struct listing, 1);
            item->title = strdup(json_string_value(json_object_get(data, "title")));
            item->selftext = strdup(json_string_value(json_object_get(data, "selftext")));
            item->ups = (uint32_t)json_integer_value(json_object_get(data, "ups"));
            items[i] = *item;
            free(item);
        }
        reddit_listings->count = count;
        reddit_listings->items = items;
        json_decref(payload);
        return reddit_listings;
    }
    return NULL;
}

void free_listings(const struct listings* listings) {
    if (!listings)
        return;
    for (size_t i = 0; i < listings->count; i++) {
        free((void*)listings->items[i].title);
        free((void*)listings->items[i].selftext);
    }
    free((void*)listings->items);
    free((void*)listings);
}

const RedditAccessToken* fetch_reddit_access_token_from_api(const RedditApp* const app) {
    struct response* response_buffer = new_response();
    struct curl_slist* ua_header = user_agent_header(app);

    CURL* url = curl_url();
    curl_url_set(url, CURLUPART_SCHEME, "https", 0);
    curl_url_set(url, CURLUPART_HOST, REDDIT_HOST, 0);
    curl_url_set(url, CURLUPART_PATH, "api/v1/access_token/", 0);
    char* url_str = NULL;
    curl_url_get(url, CURLUPART_URL, &url_str, 0);

    curl_easy_setopt(app->http_client, CURLOPT_POST, 1L);
    curl_easy_setopt(app->http_client, CURLOPT_USERNAME, app->auth->client_id);
    curl_easy_setopt(app->http_client, CURLOPT_PASSWORD, app->auth->client_secret);
    curl_easy_setopt(app->http_client, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(app->http_client, CURLOPT_WRITEDATA, response_buffer);
    curl_easy_setopt(app->http_client, CURLOPT_URL, url_str);
    curl_easy_setopt(app->http_client, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(app->http_client, CURLOPT_HTTPHEADER, ua_header);
    curl_easy_setopt(app->http_client, CURLOPT_POSTFIELDS,
                     "scope=read&grant_type=client_credentials");
    // curl_easy_setopt(app->http_client, CURLOPT_VERBOSE, 1L);

    CURLcode status = curl_easy_perform(app->http_client);

    curl_slist_free_all(ua_header);

    long* resp_status = get_response_status(app->http_client);
    fprintf(stderr, "Reddit access token request status code: %ld\n", *resp_status);
    const RedditAccessToken* reddit_token = NULL;
    if (status == CURLE_OK && *(resp_status) == 200) {
        reddit_token = deserialize_access_token(response_buffer);
    } else {
        fprintf(stderr, "Reddit access token request failed or returned non-200 code.\n");
    }
    free_response(response_buffer);
    free(resp_status);
    curl_url_cleanup(url);
    curl_free(url_str);
    return reddit_token;
}

static void cache_token(const RedditAccessToken* token, const struct rofi_reddit_paths* paths) {
    FILE* const CACHE = fopen(paths->access_token_cache_path, "w+");
    fputs(token->token, CACHE);
}

const RedditAccessToken* new_reddit_access_token(const RedditApp* const app,
                                                 const struct rofi_reddit_paths* paths) {
    const RedditAccessToken* reddit_token = NULL;
    if (!paths->access_token_cache_exists) {
        fprintf(stdout, "Access token cache miss. Fetching from API.\n");
        reddit_token = fetch_reddit_access_token_from_api(app);
        if (reddit_token) {
            fprintf(stdout, "Obtained access token from API of size: %zu. Caching to %s\n",
                    strlen(reddit_token->token), paths->access_token_cache_path);
            cache_token(reddit_token, paths);
        }
    }
    if (!reddit_token && access(paths->access_token_cache_path, R_OK) == 0) {
        FILE* const CACHE = fopen(paths->access_token_cache_path, "r");
        char* buffer = malloc(*ACCESS_TOKEN_MAX_SIZE);
        fgets(buffer, *ACCESS_TOKEN_MAX_SIZE, CACHE);
        RedditAccessToken* cached_token = malloc(sizeof(RedditAccessToken));
        cached_token->token = buffer;
        reddit_token = cached_token;
        fprintf(stdout, "Access token cache hit.\n");
        fclose(CACHE);
    }
    if (!reddit_token)
        fprintf(stderr, "Failed to obtain Reddit access token.\n");
    return reddit_token;
}

void free_reddit_access_token(const RedditAccessToken* token) {
    if (!token)
        return;
    free((void*)token->token);
    free((void*)token);
}

const struct listings* fetch_hot_listings(const RedditApp* app, const RedditAccessToken* token,
                                          const char* subreddit) {
    struct response* response_buffer = new_response();
    struct curl_slist* ua_header = user_agent_header(app);

    curl_easy_reset(app->http_client);
    CURL* url = curl_url();
    curl_url_set(url, CURLUPART_SCHEME, "https", 0);
    curl_url_set(url, CURLUPART_HOST, REDDIT_API_HOST, 0);
    char url_path[100];
    snprintf(url_path, 100, "r/%s/hot/", subreddit);
    curl_url_set(url, CURLUPART_PATH, url_path, 0);
    curl_url_set(url, CURLUPART_QUERY, "limit=1", 0);
    char* url_str = NULL;
    curl_url_get(url, CURLUPART_URL, &url_str, 0);

    curl_easy_setopt(app->http_client, CURLOPT_POST, 0L);
    curl_easy_setopt(app->http_client, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(app->http_client, CURLOPT_WRITEDATA, response_buffer);
    curl_easy_setopt(app->http_client, CURLOPT_URL, url_str);
    curl_easy_setopt(app->http_client, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
    curl_easy_setopt(app->http_client, CURLOPT_XOAUTH2_BEARER, token->token);
    curl_easy_setopt(app->http_client, CURLOPT_HTTPHEADER, ua_header);
    // curl_easy_setopt(app->http_client, CURLOPT_VERBOSE, 1L);

    CURLcode status = curl_easy_perform(app->http_client);

    curl_slist_free_all(ua_header);
    curl_url_cleanup(url);
    curl_free(url_str);

    long* resp_status = get_response_status(app->http_client);
    fprintf(stdout, "Fetch hot listings status code: %ld\n", *resp_status);
    // fprintf(stdout, "%s\n", json_dumps(json_object_get(response_payload, "data"),
    // JSON_INDENT(2)));

    const struct listings* listings = NULL;
    if (status == CURLE_OK && *(resp_status) == 200) {
        listings = deserialize_listings(response_buffer);
    } else {
        fprintf(stderr, "Reddit hot listings request failed or returned non-200 code.\n");
    }

    free(resp_status);
    return listings;
}
