#include "tomlc17.h"
#include <curl/curl.h>
#include <jansson.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef _REDDIT_H
#define _REDDIT_H

struct rofi_reddit_paths {
  const char *config_path;
  const char *access_token_cache_path;
  bool access_token_cache_exists;
};

const struct rofi_reddit_paths *new_rofi_reddit_paths();
void free_rofi_reddit_paths(const struct rofi_reddit_paths *paths);

struct app_auth {
  const char *client_name;
  const char *client_id;
  const char *client_secret;
};

struct rofi_reddit_cfg {
  const struct app_auth *auth;
  const struct rofi_reddit_paths *paths;
};

const struct rofi_reddit_cfg *
new_rofi_reddit_cfg(const struct rofi_reddit_paths *paths);
void free_rofi_reddit_cfg(const struct rofi_reddit_cfg *cfg);

typedef struct {
  const struct rofi_reddit_cfg *config;
  CURL *http_client;
} RedditApp;

const RedditApp *new_reddit_app(const struct rofi_reddit_cfg *config);

void free_reddit_app(RedditApp *app);

typedef struct {
  const char *token;
} RedditAccessToken;

const RedditAccessToken *
fetch_reddit_access_token_from_api(const RedditApp *app);
RedditAccessToken *fetch_and_cache_token(const RedditApp *app);

struct listing {
  char *title;
  char *selftext;
  char *permalink;
  uint32_t ups;
};
void free_listing(const struct listing *listing);

struct listings {
  const struct listing *items;
  size_t count;
};

void free_listings(const struct listings *listings);

const struct reddit_api_response *
fetch_hot_listings(const RedditApp *app, const RedditAccessToken *token,
                   const char *subreddit);

const RedditAccessToken *new_reddit_access_token(const RedditApp *app);

void free_reddit_access_token(const RedditAccessToken *token);

struct reddit_api_response {
  long *http_status_code;
  const void *data;
};

struct reddit_api_response *new_reddit_api_response(void *data,
                                                    long *status_code);
void free_reddit_api_response(const struct reddit_api_response *response);

#endif
