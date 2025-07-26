#include "reddit.h"
#include <assert.h>
#include <stdio.h>
#include <unity.h>

void setUp(void) {
}

void tearDown(void) {
}

void test_hot_listings_fetch_end_to_end(void) {
    struct rofi_reddit_paths* paths = new_rofi_reddit_paths();
    struct rofi_reddit_cfg* cfg = new_rofi_reddit_cfg(paths);
    RedditApp* app = new_reddit_app(cfg);

    const RedditAccessToken* token = new_reddit_access_token(app);
    struct reddit_api_response* response = fetch_hot_listings(app, token, "spainfire");
    if (response->status_code != HTTP_OK) {
        fprintf(stdout, "Access token is invalid or expired. Trying to fetch new one.\n");
        fetch_and_cache_token(app);
        paths = new_rofi_reddit_paths();
    }

    struct listings* listings = (struct listings*)response->data;
    fprintf(stdout, "Fetched %zu threads from subreddit 'spainfire'.\n", listings->count);
    for (int i = 0; i < listings->count; i++) {
        fprintf(stdout, "Thread %d: \n", i + 1);
        fprintf(stdout, "%s\n", listings->items[i].title);
        TEST_ASSERT_NOT_NULL(listings->items[i].title);
        TEST_ASSERT_TRUE(*(listings->items[i].title) != '\0');
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
