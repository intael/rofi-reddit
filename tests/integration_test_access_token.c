#include "reddit.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <unity.h>

void setUp(void) {
}

void tearDown(void) {
}

void test_hot_listings_fetch_end_to_end(void) {
    struct rofi_reddit_paths* paths = new_rofi_reddit_paths();
    if (access(paths->config_path, F_OK) == -1 || access(paths->config_path, R_OK) == -1) {
        fprintf(stdout, "Config file not found or not readable. Run the setup script to create it.\n");
        TEST_FAIL_MESSAGE("Config file not found.");
    }
    struct rofi_reddit_cfg* cfg = new_rofi_reddit_cfg(paths);
    RedditApp* app = new_reddit_app(cfg);

    const RedditAccessToken* token = new_reddit_access_token(app);
    struct reddit_api_response* response = fetch_hot_listings(app, token, "libertarian");
    if (response->status_code != HTTP_OK) {
        fprintf(stdout, "Access token is invalid or expired. Trying to fetch new one.\n");
        fetch_and_cache_token(app);
        paths = new_rofi_reddit_paths();
    }

    const struct listings* listings = deserialize_listings(response->response_buffer);
    fprintf(stdout, "Fetched %zu threads from subreddit 'libertarian'.\n", listings->count);
    for (int i = 0; i < listings->count; i++) {
        char* title = listings->items[i].title;
        TEST_ASSERT_NOT_NULL(title);
        TEST_ASSERT_TRUE(strlen(title) > 0);
    }

    free_reddit_app(app);
    free_reddit_access_token(token);
    free_listings(listings);
    free_reddit_api_response(response);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_hot_listings_fetch_end_to_end);
    return UNITY_END();
}
