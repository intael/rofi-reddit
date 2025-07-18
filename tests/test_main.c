#include "reddit.h"
#include <stdio.h>

int main(void) {
    struct rofi_reddit_paths* paths = new_rofi_reddit_paths();
    struct rofi_reddit_cfg* cfg = new_rofi_reddit_cfg(paths);

    fprintf(stdout, "Reddit client name: %s\n", cfg->auth->client_name);
    fprintf(stdout, "Reddit client id: %s\n", cfg->auth->client_id);
    fprintf(stdout, "Reddit client secret: %s\n", cfg->auth->client_secret);

    RedditApp* app = new_reddit_app(cfg);

    const RedditAccessToken *token = new_reddit_access_token(app, paths);
    struct listings *threads = fetch_hot_listings(app, token, "spainfire");

    fprintf(stdout, "Fetched %zu threads from subreddit 'spainfire'.\n", threads->count);
    for(int i = 0; i < threads->count; i++) {
        fprintf(stdout, "--------------------------------------\n");
        fprintf(stdout, "Title: %s\n", threads->items[i].title);
        fprintf(stdout, "Selftext: %s\n", threads->items[i].selftext);
        fprintf(stdout, "Ups: %u\n", threads->items[i].ups);
    }

    free_reddit_app(app);
    free_rofi_reddit_cfg(cfg);
    free_rofi_reddit_paths(paths);
    free_reddit_access_token(token);
}
