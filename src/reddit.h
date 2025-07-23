#include <curl/curl.h>
#include <jansson.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef _REDDIT_H
#define _REDDIT_H

struct rofi_reddit_paths {
  const char *dir_path;
  bool dir_exists;
  const char *config_path;
  bool config_path_exists;
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
};

const struct rofi_reddit_cfg *
new_rofi_reddit_cfg(const struct rofi_reddit_paths *paths);
void free_rofi_reddit_cfg(const struct rofi_reddit_cfg *cfg);

typedef struct {
  const struct app_auth *auth;
  CURL *http_client;
} RedditApp;

const RedditApp *new_reddit_app(const struct rofi_reddit_cfg *config);

void free_reddit_app(RedditApp *app);

typedef struct {
  const char *token;
} RedditAccessToken;

const RedditAccessToken *
fetch_reddit_access_token_from_api(const RedditApp *app);
RedditAccessToken *fetch_and_cache_token(const RedditApp *app,
                                         const struct rofi_reddit_paths *paths);

struct listing {
  char *title;
  char *selftext;
  uint32_t ups;
};

struct listings {
  const struct listing *items;
  size_t count;
};

const struct reddit_api_response *
fetch_hot_listings(const RedditApp *app, const RedditAccessToken *token,
                   const char *subreddit);

const RedditAccessToken *
new_reddit_access_token(const RedditApp *app,
                        const struct rofi_reddit_paths *paths);

void free_reddit_access_token(const RedditAccessToken *token);

struct reddit_api_response {
  long *http_status_code;
  const void *data;
};

struct reddit_api_response *new_reddit_api_response(void *data,
                                                    long *status_code);
void free_reddit_api_response(const struct reddit_api_response *response);

#endif
