#include <stdio.h>
#include <unistd.h>

#include "glib.h"
#include "reddit.h"
#include <rofi/helper.h>
#include <rofi/mode-private.h>
#include <rofi/mode.h>

#include <stdint.h>

G_MODULE_EXPORT Mode mode;

typedef struct {
    const RedditApp* app;
    const RedditAccessToken* token;
    struct listings* listings;
} RofiRedditModePrivateData;

static int rofi_reddit_mode_init(Mode* sw) {
    /**
     * Called on startup when enabled (in modi list)
     */
    if (mode_get_private_data(sw) == NULL) {
        const struct rofi_reddit_paths* paths = new_rofi_reddit_paths();
        const struct rofi_reddit_cfg* config = new_rofi_reddit_cfg(paths);
        const RedditApp* app = new_reddit_app(config);
        RofiRedditModePrivateData* pd = g_malloc0(sizeof(*pd));
        mode_set_private_data(sw, (void*)pd);
        // free_rofi_reddit_cfg(config); // TODO: need to deepcopy auth when instantiating app
        pd->app = app;
        pd->token = new_reddit_access_token(app, paths);
        pd->listings = NULL;
        fprintf(stdout, "Initialized Rofi Reddit Mode with app: %s\n", app->auth->client_id);
    }
    return TRUE;
}

static unsigned int rofi_reddit_mode_get_num_entries(const Mode* sw) {
    const RofiRedditModePrivateData* pd =
        (const RofiRedditModePrivateData*)mode_get_private_data(sw);
    if (pd->listings) {
        fprintf(stdout, "Number of listings: %zu\n", pd->listings->count);
        return pd->listings->count;
    }
    return 0;
}

static ModeMode rofi_reddit_mode_result(Mode* sw, int mretv, char** input,
                                        unsigned int selected_line) {
    ModeMode retv = MODE_EXIT;
    RofiRedditModePrivateData* pd = (RofiRedditModePrivateData*)mode_get_private_data(sw);
    fprintf(stdout, "Result!\n");
    if (mretv & MENU_NEXT) {
        retv = NEXT_DIALOG;
    } else if (mretv & MENU_OK) { // TODO: spawn browser with open tab at url
        if (pd->listings == NULL) {
            fprintf(stderr, "No listings available.\n");
            return MODE_EXIT;
        }
        if (selected_line >= pd->listings->count) {
            fprintf(stderr, "Selected line out of range.\n");
            return MODE_EXIT;
        }
        const struct listing* selected_listing = &pd->listings->items[selected_line];
        printf("Selected listing: %s\n", selected_listing->title);
        printf("Ups: %u\n", selected_listing->ups);
        retv = MODE_EXIT;
    } else if (mretv & MENU_PREVIOUS) {
        retv = PREVIOUS_DIALOG;
    } else if ((mretv & MENU_CUSTOM_INPUT)) {
        fprintf(stdout, "Fetching subreddit=%s listings.\n", *input);
        const struct reddit_api_response* response = fetch_hot_listings(pd->app, pd->token, *input);
        if (*response->http_status_code != 200) { // TODO: handle 302 redirects
            fprintf(stdout, "Access token is invalid or expired. Trying to fetch new one.\n");
            pd->token = new_reddit_access_token(pd->app, new_rofi_reddit_paths());
        }
        pd->listings = (struct listings*)fetch_hot_listings(pd->app, pd->token, *input)->data;
        retv = RELOAD_DIALOG;
    }
    return retv;
}

static void rofi_reddit_mode_destroy(Mode* sw) {
    RofiRedditModePrivateData* pd = (RofiRedditModePrivateData*)mode_get_private_data(sw);
    if (pd != NULL) {
        fprintf(stdout, "Destroying Rofi Reddit Mode.\n");
        free_reddit_access_token(pd->token);
        free_reddit_app(pd->app);
        g_free(pd);
        mode_set_private_data(sw, NULL);
    }
}

static char* _get_display_value(const Mode* sw, unsigned int selected_line,
                                G_GNUC_UNUSED int* state, G_GNUC_UNUSED GList** attr_list,
                                int get_entry) {
    RofiRedditModePrivateData* pd = (RofiRedditModePrivateData*)mode_get_private_data(sw);
    if (!pd->listings) {
        return g_strdup("OOPS!"); // TODO: implement history of subreddits
    }
    if (selected_line >= pd->listings->count) {
        fprintf(stderr, "Selected line out of range.\n");
        return NULL;
    }
    const struct listing* entry = &pd->listings->items[selected_line];
    return g_strdup_printf("%s", entry->title);
}

static int rofi_reddit_token_match(const Mode* sw, rofi_int_matcher** tokens, unsigned int index) {
    return true;
}

static char* get_message(const Mode* sw) {
    return g_strdup("Subreddit:");
}

Mode mode = {
    .abi_version = ABI_VERSION,
    .name = "rofi_reddit",
    .cfg_name_key = "display-rofi_reddit",
    ._init = rofi_reddit_mode_init,
    ._get_num_entries = rofi_reddit_mode_get_num_entries,
    ._result = rofi_reddit_mode_result,
    ._destroy = rofi_reddit_mode_destroy,
    ._token_match = rofi_reddit_token_match,
    ._get_display_value = _get_display_value,
    ._get_message = get_message,
    ._get_completion = NULL,
    ._preprocess_input = NULL,
    .private_data = NULL,
    .free = NULL,
};
