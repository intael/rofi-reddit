#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "curl_wrappers.h"
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
    if (mode_get_private_data(sw) == NULL) {
        const struct rofi_reddit_paths* paths = new_rofi_reddit_paths();
        const struct rofi_reddit_cfg* config = new_rofi_reddit_cfg(paths);
        const RedditApp* app = new_reddit_app(config);
        RofiRedditModePrivateData* pd = g_malloc0(sizeof(*pd));
        mode_set_private_data(sw, (void*)pd);
        pd->app = app;
        pd->token = new_reddit_access_token(app);
        pd->listings = NULL;
        fprintf(stdout, "Initialized Rofi Reddit Mode with app: %s\n",
                app->config->auth->client_name);
    }
    return TRUE;
}

static unsigned int rofi_reddit_mode_get_num_entries(const Mode* sw) {
    const RofiRedditModePrivateData* pd =
        (const RofiRedditModePrivateData*)mode_get_private_data(sw);
    if (pd->listings)
        return pd->listings->count;
    return 0;
}

static bool is_bad_response_due_to_access_token(const struct reddit_api_response* response) {
    json_error_t error;
    json_t* root = json_loads((const char*)response->response_buffer->buffer, 0, &error);
    if (response->status_code == HTTP_FORBIDDEN && root && json_is_object(root)) {
        json_t* reason = json_object_get(root, "reason");
        if (reason && json_is_string(reason)) {
            const char* reason_str = json_string_value(reason);
            if (strcmp(reason_str, "private") == 0) {
                fprintf(stderr, "Subreddit is private. You do not have access.\n");
            } else if (strcmp(reason_str, "quarantined") == 0) {
                fprintf(stderr, "Subreddit is quarantined. Special access required.\n");
            } else {
                fprintf(stderr, "%s.\n", reason_str);
            }
        } else {
            fprintf(stderr, "Access to subreddit is forbidden for an unknown reason.\n");
        }
        json_decref(root);
        return false;
    }
    fprintf(stdout, "Detected expired or invalid access token.\n");
    return true;
}

static ModeMode rofi_reddit_mode_result(Mode* sw, int mretv, char** input,
                                        unsigned int selected_line) {
    ModeMode retv = MODE_EXIT;
    RofiRedditModePrivateData* pd = (RofiRedditModePrivateData*)mode_get_private_data(sw);
    if (mretv & MENU_NEXT) {
        retv = NEXT_DIALOG;
    } else if (mretv & MENU_OK) {
        if (pd->listings == NULL) {
            // TODO: create valid empty state and reload with it here
            fprintf(stderr, "No listings available.\n");
            return MODE_EXIT;
        }
        if (selected_line >= pd->listings->count) {
            fprintf(stderr, "Selected line out of range.\n");
            return MODE_EXIT;
        }
        char cmd[500];
        snprintf(cmd, sizeof(cmd), "xdg-open '%s' &", pd->listings->items[selected_line].url);
        system(cmd);
        retv = MODE_EXIT;
    } else if (mretv & MENU_PREVIOUS) {
        retv = PREVIOUS_DIALOG;
    } else if ((mretv & MENU_CUSTOM_INPUT)) {
        fprintf(stdout, "Fetching subreddit=%s listings.\n", *input);
        const struct reddit_api_response* response = fetch_hot_listings(pd->app, pd->token, *input);
        switch (response->status_code) {
        case HTTP_OK:
            pd->listings = deserialize_listings(response->response_buffer);
            break;
        case HTTP_UNAUTHORIZED:
        case HTTP_FORBIDDEN:
            if (is_bad_response_due_to_access_token(response)) {
                free_reddit_api_response(response);
                free_reddit_access_token(pd->token);
                pd->token = fetch_and_cache_token(pd->app);
                retv = rofi_reddit_mode_result(sw, mretv, input, selected_line);
            }
            break;
        case HTTP_NOT_FOUND:
            fprintf(stdout, "Subreddit=%s doesn't seem to exist.\n", *input);
            return MODE_EXIT;
        default:
            break;
        }
        if (pd->listings) {
            fprintf(stdout, "Collected listings: %zu\n", pd->listings->count);
        }
        retv = RELOAD_DIALOG;
        if (response)
            free_reddit_api_response(response);
    }
    return retv;
}

static void rofi_reddit_mode_destroy(Mode* sw) {
    RofiRedditModePrivateData* pd = (RofiRedditModePrivateData*)mode_get_private_data(sw);
    if (pd != NULL) {
        fprintf(stdout, "Destroying Rofi Reddit Mode.\n");
        free_reddit_access_token(pd->token);
        free_reddit_app(pd->app);
        free_listings(pd->listings);
        g_free(pd);
        mode_set_private_data(sw, NULL);
    }
}

static char* get_display_value(const Mode* sw, unsigned int selected_line, G_GNUC_UNUSED int* state,
                               G_GNUC_UNUSED GList** attr_list, int get_entry) {
    RofiRedditModePrivateData* pd = (RofiRedditModePrivateData*)mode_get_private_data(sw);
    if (!pd->listings) {
        return g_strdup("OOPS!"); // TODO: implement history of subreddits
    }
    if (selected_line >= pd->listings->count) {
        fprintf(stderr, "Selected line out of range.\n");
        return NULL;
    }
    return g_strdup_printf("%s", pd->listings->items[selected_line].title);
}

static int rofi_reddit_token_match(const Mode* sw, rofi_int_matcher** tokens, unsigned int index) {
    return true;
}

static char* get_message(const Mode* sw) {
    RofiRedditModePrivateData* pd = (RofiRedditModePrivateData*)mode_get_private_data(sw);
    if (pd->listings && pd->listings->count > 0) {
        return g_strdup("Now select a thread to open in your browser!");
    }
    return g_strdup("Type a subreddit to fetch threads for!");
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
    ._get_display_value = get_display_value,
    ._get_message = get_message,
    ._get_completion = NULL,
    ._preprocess_input = NULL,
    .private_data = NULL,
    .free = NULL,
};
