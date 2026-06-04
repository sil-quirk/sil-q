#include "angband.h"
#include "metarun-internal.h"

void metarun_save_persistent_settings(void)
{
    log_info("Saving persistent settings to metarun");

    /* Save options */
    for (int i = 0; i < 8; i++) {
        metar.persistent_options[i] = 0;
    }

    /* Pack options into the persistent storage */
    for (int i = 0; i < OPT_MAX; i++) {
        int word_idx = i / 32;
        int bit_idx = i % 32;

        if (word_idx < 8 && option_text[i] && !option_is_app_persistent(i)
            && op_ptr->opt[i]) {
            metar.persistent_options[word_idx] |= (1UL << bit_idx);
        }
    }

    /* Save window flags */
    for (int i = 0; i < SAVE_WINDOW_TERM_MAX; i++) {
        metar.persistent_window_flags[i] = op_ptr->window_flag[i];
    }

    /* Mark as initialized */
    metar.persistent_options_initialized = 1;

    /* Save the metarun data */
    save_metaruns();

    log_info("Persistent settings saved successfully");
}

/*
 * Load metarun persistent settings to current game options
 */
void metarun_load_persistent_settings(void)
{
    /* Only load if settings have been previously saved */
    if (!metar.persistent_options_initialized) {
        log_info("No persistent settings found, using defaults");
        return;
    }

    log_info("Loading persistent settings from metarun");

    /* Load options */
    for (int i = 0; i < OPT_MAX; i++) {
        int word_idx = i / 32;
        int bit_idx = i % 32;

        if (word_idx < 8 && option_text[i] && !option_is_app_persistent(i)) {
            op_ptr->opt[i] = (metar.persistent_options[word_idx] & (1UL << bit_idx)) != 0;
        }
    }

    /* Load window flags */
    for (int i = 0; i < ANGBAND_TERM_MAX; i++)
        op_ptr->window_flag[i] = 0;
    for (int i = 0; i < SAVE_WINDOW_TERM_MAX; i++)
        op_ptr->window_flag[i] = metar.persistent_window_flags[i];
    op_ptr->window_flag[WINDOW_SUPPLY] |= PW_SUPPLY;

    log_info("Persistent settings loaded successfully");
}
