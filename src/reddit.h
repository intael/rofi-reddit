#include "curl_wrappers.h"
#include <curl/curl.h>
#include <jansson.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef _REDDIT_H
#define _REDDIT_H

struct rofi_reddit_paths {
    const char* config_path;
    const char* access_token_cache_path;
    bool access_token_cache_exists;
};

struct rofi_reddit_paths* new_rofi_reddit_paths();
void free_rofi_reddit_paths(const struct rofi_reddit_paths* paths);

struct app_auth {
    const char* client_name;
    const char* client_id;
    const char* client_secret;
};

struct rofi_reddit_cfg {
    const struct app_auth* auth;
    struct rofi_reddit_paths* paths;
};

struct rofi_reddit_cfg* new_rofi_reddit_cfg(struct rofi_reddit_paths* paths);
void free_rofi_reddit_cfg(const struct rofi_reddit_cfg* cfg);

typedef struct {
    struct rofi_reddit_cfg* config;
    CURL* http_client;
} RedditApp;

RedditApp* new_reddit_app(struct rofi_reddit_cfg* config);

void free_reddit_app(RedditApp* app);

typedef struct {
    const char* token;
} RedditAccessToken;

const struct reddit_api_response* fetch_reddit_access_token_from_api(const RedditApp* app);
RedditAccessToken* fetch_and_cache_token(RedditApp* app);

struct listing {
    char* title;
    char* selftext;
    char* url;
    uint32_t ups;
};
void free_listing(const struct listing* listing);

struct listings {
    const struct listing* items;
    size_t count;
};

struct listings* deserialize_listings(const struct response_buffer* resp);
void deserialize_listing(json_t* listing_json, struct listing* deserialize_to, size_t index);

void free_listings(const struct listings* listings);

const struct reddit_api_response* fetch_hot_listings(const RedditApp* app, const RedditAccessToken* token,
                                                     const char* subreddit);

RedditAccessToken* new_reddit_access_token(RedditApp* app);

void free_reddit_access_token(const RedditAccessToken* token);

struct reddit_api_response {
    enum http_status_code status_code;
    const struct response_buffer* response_buffer;
};

struct reddit_api_response* new_reddit_api_response(struct response_buffer* response, long* status_code);
void free_reddit_api_response(const struct reddit_api_response* response);

enum subreddit_access {
    SUBREDDIT_ACCESS_UNINITIALIZED,
    SUBREDDIT_ACCESS_OK,
    SUBREDDIT_ACCESS_DOESNT_EXIST,
    SUBREDDIT_ACCESS_PRIVATE,
    SUBREDDIT_ACCESS_QUARANTINED,
    SUBREDDIT_ACCESS_EXPIRED_TOKEN,
    SUBREDDIT_ACCESS_UNKNOWN
};

#endif
