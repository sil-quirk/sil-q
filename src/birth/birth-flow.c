/* File: birth/birth-flow.c */

#include "angband.h"
#include "birth/birth-internal.h"

#define BASE_COLUMN 7
#define STAT_TITLE_ROW 14
#define BASE_STAT_ROW 16

/*
 * Helper function for 'player_birth()'.
 *
 * See "display_player" for screen layout code.
 */
static NavResult player_birth_aux(void)
{

    log_debug("Initializing character data and history");
    birth_pending_compact_description_confirm = true;

    SDL_strlcpy(op_ptr->full_name, c_name + c_info[p_ptr->pcharacter].name, sizeof(op_ptr->full_name));
    process_player_name(true);  /* CRITICAL: Must pass true to update savefile path! */
    /* Clear the previous history strings */
    p_ptr->history[0] = '\0';
    SDL_strlcat(
                p_ptr->history, (c_text + c_info[p_ptr->pcharacter].text), sizeof(p_ptr->history));

    p_ptr->wt = 0;
    p_ptr->ht = 0;
    p_ptr->age = 0;

    /* Oath selection (after character creation, before tutorial/stats) */
    if (run_mode_is_blitz() && !blitz_oaths_enabled())
    {
        p_ptr->oath_type = 0;
    }
    else
    {
        log_debug("Entering oath selection");
        NavResult oath_result = select_oath();
        if (oath_result != NAV_OK) return oath_result;
        log_debug("Oath selection completed");
    }

    if (run_mode_is_blitz())
    {
        NavResult blitz_effects = blitz_configure_effects();
        if (blitz_effects != NAV_OK)
            return blitz_effects;
    }

    /* Point-based flow */
    if (blitz_auto_allocates_stats())
    {
        NavResult auto_result = blitz_auto_build_character();
        if (auto_result != NAV_OK)
            return auto_result;
    }
    else
    {
        int stat_alloc[A_MAX];

        for (int i = 0; i < A_MAX; i++)
            stat_alloc[i] = p_ptr->stat_base[i];

        /* Show the complete character sheet once ("full at first") before
         * dropping into stat allocation, then skills. */
        character_sheet_show_birth_preview();

        for (;;)
        {
            if (!sdl_character_sheet_screen_active())
                display_player(0);

            /* Stats allocation screen */
            log_debug("Entering stats allocation");
            NavResult s = player_birth_aux_2(stat_alloc);
            if (s == NAV_OK) {
                /* Skill allocation: Esc returns to stats; q returns to character selection. */
                log_debug("Stats accepted, entering skills allocation");
                screen_push_touch_pane_proto();
                NavResult g = gain_skills();
                screen_pop_touch_pane_proto();
                if (g == NAV_BACK) continue;
                if (g == NAV_TO_CHARACTER) return NAV_BACK;
                if (g != NAV_OK) return g;
                log_debug("Skills allocation completed");
                break; /* accepted */
            }
            if (s == NAV_BACK)   return NAV_BACK;    /* back to Character Selection */
            if (s == NAV_TO_CHARACTER) return NAV_BACK; /* back to Character Selection */
            if (s == NAV_TO_MAIN) return NAV_TO_MAIN;/* back to main menu */
            if (s == NAV_QUIT)   return NAV_QUIT;    /* hard exit */
            /* any other value: loop again */
        }
    }

    // Reset the number of artefacts
    p_ptr->artefacts = 0;

    log_trace("Final character stats: Str=%d Dex=%d Con=%d Gra=%d",
              p_ptr->stat_base[A_STR], p_ptr->stat_base[A_DEX],
              p_ptr->stat_base[A_CON], p_ptr->stat_base[A_GRA]);

    /* Accept */
    return NAV_OK;
}

/*
 * Create a new character.
 *
 * Note that we may be called with "junk" leftover in the various
 * fields, so we must be sure to clear them first.
 */
NavResult player_birth()
{
    int i;

    char raw_date[25];
    char clean_date[25];
    char month[4];
    time_t ct = time((time_t*)0);

    log_info("Starting character creation process");
    killer_reset();

    /* Create a new character */
    while (1)
    {
        NavResult r = player_birth_aux();
        if (r == NAV_OK) break;
        if (r == NAV_BACK) return NAV_BACK;         /* back to character_selection */
        if (r == NAV_TO_CHARACTER) return NAV_BACK; /* back to character_selection */
        if (r == NAV_TO_MAIN) return NAV_TO_MAIN;   /* back to main menu */
        if (r == NAV_QUIT) return NAV_QUIT;         /* hard exit */
        /* Any other value -> retry loop */
    }

    for (i = 0; i < NOTES_LENGTH; i++)
    {
        notes_buffer[i] = '\0';
    }

    /* Get date */
    (void)strftime(raw_date, sizeof(raw_date), "@%Y%m%d", localtime(&ct));

    sprintf(month, "%.2s", raw_date + 5);
    atomonth(atoi(month), month);

    if (*(raw_date + 7) == '0')
        sprintf(
            clean_date, "%.1s %.3s %.4s", raw_date + 8, month, raw_date + 1);
    else
        sprintf(
            clean_date, "%.2s %.3s %.4s", raw_date + 7, month, raw_date + 1);

    /* Add in "character start" information */
    SDL_strlcat(notes_buffer,
        format("%s of the %s\n", op_ptr->full_name, p_name + rp_ptr->name),
        sizeof(notes_buffer));
    SDL_strlcat(notes_buffer, format("Entered Angband on %s\n", clean_date),
        sizeof(notes_buffer));
    SDL_strlcat(
        notes_buffer, "\n   Turn     Depth   Note\n\n", sizeof(notes_buffer));

    /* Note player birth in the message recall */
    message_add(" ", MSG_GENERIC);
    message_add("  ", MSG_GENERIC);
    message_add("====================", MSG_GENERIC);
    message_add("  ", MSG_GENERIC);
    message_add(" ", MSG_GENERIC);

    /* Hack -- outfit the player */
    player_outfit();

    /* Load persistent settings from metarun if this is a continuing metarun */
    if (!run_mode_is_blitz())
        metarun_load_persistent_settings();

    /* Reapply app-wide settings after character creation so UI preferences are
     * not sourced from the metarun or savefile. */
    sdl_config_load_app_options(get_sdl_config_path());

    log_info("Character creation completed: %s the %s", op_ptr->full_name, p_name + rp_ptr->name);

    return NAV_OK;
}













