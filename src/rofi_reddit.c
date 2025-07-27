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
    char* selected_subreddit;
    enum subreddit_access subreddit_access;
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
        pd->selected_subreddit = NULL;
        pd->subreddit_access = SUBREDDIT_ACCESS_UNINITIALIZED;
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

static enum subreddit_access
subreddit_access_denied_reason(const struct reddit_api_response* response) {
    json_error_t error;
    json_t* root = json_loads((const char*)response->response_buffer->buffer, 0, &error);
    enum subreddit_access access_status = SUBREDDIT_ACCESS_EXPIRED_TOKEN;
    if (response->status_code == HTTP_FORBIDDEN && root && json_is_object(root)) {
        access_status = SUBREDDIT_ACCESS_UNKNOWN;
        json_t* reason = json_object_get(root, "reason");
        if (reason && json_is_string(reason)) {
            const char* reason_str = json_string_value(reason);
            if (strcmp(reason_str, "private") == 0) {
                access_status = SUBREDDIT_ACCESS_PRIVATE;
            } else if (strcmp(reason_str, "quarantined") == 0) {
                access_status = SUBREDDIT_ACCESS_QUARANTINED;
            }
        }
        json_decref(root);
    }
    return access_status;
}

static const char* sanitize_subrredit_name(const char* subreddit) {
    if (!subreddit || strlen(subreddit) == 0)
        return NULL;
    char* trimmed = g_strstrip(g_strdup(subreddit));
    GString* result = g_string_new(NULL);
    for (const char* p = trimmed; *p; ++p) {
        if (!g_ascii_isspace(*p)) {
            g_string_append_c(result, *p);
        }
    }
    g_free(trimmed);
    char* final = g_strdup(result->str);
    g_string_free(result, TRUE);
    return final;
}

static ModeMode rofi_reddit_mode_result(Mode* sw, int mretv, char** input,
                                        unsigned int selected_line) {
    ModeMode retv = MODE_EXIT;
    RofiRedditModePrivateData* pd = (RofiRedditModePrivateData*)mode_get_private_data(sw);
    if (mretv & MENU_NEXT) {
        retv = NEXT_DIALOG;
    } else if (mretv & MENU_OK) {
        if (!pd->listings || selected_line >= pd->listings->count)
            return MODE_EXIT;
        char* url = pd->listings->items[selected_line].url;
        char* xdg_command = "xdg-open";
        char cmd[strlen(xdg_command) + strlen(url) + 4];
        snprintf(cmd, sizeof(cmd), "%s '%s' &", xdg_command, url);
        system(cmd);
        retv = MODE_EXIT;
    } else if (mretv & MENU_PREVIOUS) {
        retv = PREVIOUS_DIALOG;
    } else if ((mretv & MENU_CUSTOM_INPUT)) {
        const char* subreddit = sanitize_subrredit_name(*input);
        if (!subreddit || strlen(subreddit) == 0) {
            pd->subreddit_access = SUBREDDIT_ACCESS_UNKNOWN;
            return RELOAD_DIALOG;
        }
        pd->selected_subreddit = subreddit;
        fprintf(stdout, "Fetching subreddit=%s listings.\n", subreddit);
        const struct reddit_api_response* response =
            fetch_hot_listings(pd->app, pd->token, subreddit);
        switch (response->status_code) {
        case HTTP_OK:
            pd->listings = deserialize_listings(response->response_buffer);
            pd->subreddit_access = SUBREDDIT_ACCESS_OK;
            break;
        case HTTP_UNAUTHORIZED:
        case HTTP_FORBIDDEN:
            enum subreddit_access denied_reason = subreddit_access_denied_reason(response);
            switch (denied_reason) {
            case SUBREDDIT_ACCESS_EXPIRED_TOKEN:
                free_reddit_access_token(pd->token);
                pd->token = fetch_and_cache_token(pd->app);
                retv = rofi_reddit_mode_result(sw, mretv, (char**)&subreddit, selected_line);
                break;
            case SUBREDDIT_ACCESS_QUARANTINED:
            case SUBREDDIT_ACCESS_UNKNOWN:
            case SUBREDDIT_ACCESS_PRIVATE:
                pd->subreddit_access = denied_reason;
                break;
            }
            break;
        case HTTP_NOT_FOUND:
            pd->subreddit_access = SUBREDDIT_ACCESS_DOESNT_EXIST;
            retv = RELOAD_DIALOG;
        default:
            break;
        }
        if (pd->listings && pd->listings->count > 0) {
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
        free(pd->selected_subreddit);
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
    char* message = NULL;
    switch (pd->subreddit_access) {
    case SUBREDDIT_ACCESS_UNINITIALIZED:
        message = "Type a subreddit to fetch threads for!";
        break;
    case SUBREDDIT_ACCESS_OK:
        if (pd->listings && pd->listings->count > 0) {
            message = g_strdup_printf(
                "Found %zu threads for subreddit '%s'. Now select a thread to open in your browser!",
                pd->listings->count, pd->selected_subreddit);
        } else {
            message = "No threads available on this subreddit. Type another subreddit to fetch "
                      "threads for!";
        }
        break;
    case SUBREDDIT_ACCESS_DOESNT_EXIST:
        message = "This subreddit does not exist. Please try another one.";
        break;
    case SUBREDDIT_ACCESS_PRIVATE:
        message = "This subreddit is private. You do not have access.";
        break;
    case SUBREDDIT_ACCESS_QUARANTINED:
        message = "This subreddit is quarantined. Can't fetch threads.";
        break;
    case SUBREDDIT_ACCESS_UNKNOWN:
        message = "Unknown access status for subreddit. Can't fetch threads.";
        break;
    default:
        message = "An unknown error occurred. Please try again.";
        break;
    }
    return g_strdup(message);
}

Mode mode = {
    .abi_version = ABI_VERSION,
    .name = "rofi-reddit",
    .cfg_name_key = "display-rofi-reddit",
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
