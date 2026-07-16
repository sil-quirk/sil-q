/* File: birth/birth-allocation.c */

#include "angband.h"
#include "birth/birth-internal.h"

NavResult player_birth_aux_2(int stats[A_MAX])
{
    int i;

    int stat = 0;

    int cost;
    int stat_costs[A_MAX];

    char ch;

    NavResult result = NAV_BACK;

    /* Determine experience and things */
    get_extra();

    /*
     * First-time players are coached inline: the guided callout overlay for the
     * attributes screen is shown once, from within the loop below, after the
     * real stats sheet has been built (see birth_coach_show_once).
     */

    log_trace("Starting stats allocation interface");
    screen_push_touch_pane_hidden();

    /* Interact */
    while (1)
    {
        bool steamdeck = steamdeck_controls_active();
        /* Reset cost */
        cost = 0;

        /* Process stats */
        for (i = 0; i < A_MAX; i++)
        {
            /* Obtain a "bonus" for "race" */
            int bonus = rp_ptr->r_adj[i] + current_character_profile->h_adj[i] + curses_stat_adj(i);

            /* Apply the racial bonuses */
            p_ptr->stat_base[i] = stats[i] + bonus;
            p_ptr->stat_drain[i] = 0;

            /* Total cost */
            cost += birth_stat_costs[stats[i] + 4];
        }

        /* Restrict cost */
        if (cost > MAX_COST)
        {
            /* Warning */
            bell("Excessive stats!");

            /* Reduce stat */
            stats[stat]--;

            /* Recompute costs */
            continue;
        }

        for (i = 0; i < A_MAX; i++)
            stat_costs[i] = birth_stat_increase_cost(stats[i]);

        p_ptr->new_exp = p_ptr->exp = get_start_xp();

        /* Calculate the bonuses and hitpoints */
        p_ptr->update |= (PU_BONUS | PU_HP);

        /* Update stuff */
        update_stuff();

        /* Fully healed */
        p_ptr->chp = p_ptr->mhp;

        /* Fully rested */
        calc_voice();
        p_ptr->csp = p_ptr->msp;

        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);

        sdl_character_sheet_screen_show_birth_stats(stats, stat_costs,
            stat, MAX_COST - cost);

        /* First-time players: guided callouts over the real attributes screen. */
        birth_coach_show_once(BIRTH_COACH_STATS);

        /* Get key */
        hide_cursor = true;
        ch = inkey();
        hide_cursor = false;
        bool click_generated_command = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice < A_MAX)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != stat)
                    {
                        stat = clicked_choice;
                        continue;
                    }
                    ch = (click_action == UI_MENU_CLICK_SECONDARY) ? '4' : '6';
                    click_generated_command = true;
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                else if (clicked_choice == -1) {
                    ch = ESCAPE;
                    click_generated_command = true;
                } else if (clicked_choice == -2) {
                    ch = '\r';
                    click_generated_command = true;
                } else if (clicked_choice == -3) {
                    ch = 'q';
                    click_generated_command = true;
                }
            }
        }

        if (!click_generated_command)
            ch = (char)steamdeck_menu_key(ch, 0, 0);

        /* Return to character selection before the game starts */
        if ((ch == 'Q') || (ch == 'q')) {
            ui_menu_click_clear();
            result = (turn == 0) ? NAV_BACK : NAV_QUIT;
            goto cleanup;
        }

        /* Back to Character Selection */
        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;
        if (ch == ESCAPE)
        {
            ui_menu_click_clear();
            result = NAV_BACK;
            goto cleanup;
        }

        /* Done */
        if (birth_confirm_input(ch, steamdeck))
        {
            if (!birth_confirm_unspent_stat_points(MAX_COST - cost, steamdeck))
                continue;
            ui_menu_click_clear();
            result = NAV_OK;
            goto cleanup;
        }

        /* Prev stat */
        if (ch == '8')
        {
            stat = (stat + A_MAX - 1) % A_MAX;
        }

        /* Next stat */
        if (ch == '2')
        {
            stat = (stat + 1) % A_MAX;
        }

        /* Decrease stat */
        if ((ch == '4') && (stats[stat] > 0))
        {
            stats[stat]--;
        }

        /* Increase stat */
        if (ch == '6')
        {
            stats[stat]++;
        }
    }

    /* Shouldn't reach; default to back */
cleanup:
    /*
     * A successful allocation proceeds directly to the skills overlay.
     * Leave the stats overlay active until that overlay replaces it so no
     * uncovered frame can be presented during the handoff.
     */
    if (result != NAV_OK)
        sdl_character_sheet_screen_hide();
    screen_pop_touch_pane_hidden();
    return result;
}
