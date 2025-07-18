#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "glib.h"
#include "reddit.h"
#include <rofi/helper.h>
#include <rofi/mode-private.h>
#include <rofi/mode.h>

#include <stdint.h>

G_MODULE_EXPORT Mode mode;

/**
 * The internal data structure holding the private data of the TEST Mode.
 */
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
        free_rofi_reddit_cfg(config);
        pd->app = app;
        pd->token = new_reddit_access_token(app, paths);
        pd->listings = NULL;
        mode_set_private_data(sw, (void*)pd);
    }
    return TRUE;
}
static unsigned int rofi_reddit_mode_get_num_entries(const Mode* sw) {
    const RofiRedditModePrivateData* pd =
        (const RofiRedditModePrivateData*)mode_get_private_data(sw);
    return pd->listings->count;
}

static ModeMode rofi_reddit_mode_result(Mode* sw, int mretv, char** input,
                                        unsigned int selected_line) {
    ModeMode retv = MODE_EXIT;
    RofiRedditModePrivateData* pd = (RofiRedditModePrivateData*)mode_get_private_data(sw);
    if (mretv & MENU_NEXT) {
        retv = NEXT_DIALOG;
    } else if (mretv & MENU_PREVIOUS) {
        retv = PREVIOUS_DIALOG;
    } else if ((mretv & MENU_CUSTOM_INPUT)) {
        retv = RELOAD_DIALOG;
    }
    return retv;
}

static void rofi_reddit_mode_destroy(Mode* sw) {
    RofiRedditModePrivateData* pd = (RofiRedditModePrivateData*)mode_get_private_data(sw);
    if (pd != NULL) {
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
    return g_strdup(pd->listings->items[selected_line].title);
}

/**
 * @param sw The mode object.
 * @param tokens The tokens to match against.
 * @param index  The index in this plugin to match against.
 *
 * Match the entry.
 *
 * @param returns try when a match.
 */
static int myplugin_token_match(const Mode* sw, rofi_int_matcher** tokens, unsigned int index) {
    RofiRedditModePrivateData* pd = (RofiRedditModePrivateData*)mode_get_private_data(sw);

    // Call default matching function.
    return helper_token_match(tokens, pd->listings->items[index].title);
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
    ._token_match = myplugin_token_match,
    ._get_display_value = _get_display_value,
    ._get_message = get_message,
    ._get_completion = NULL,
    ._preprocess_input = NULL,
    .private_data = NULL,
    .free = NULL,
};
