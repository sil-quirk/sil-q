/* File: birth/birth-skills.c */

#include "angband.h"
#include "birth/birth-internal.h"

/*
 * Skill point costs.
 *
 * The nth skill point costs (100*n) experience points
 */
int birth_skill_cost(int base, int points)
{
    int total_cost = (points + base) * (points + base + 1) / 2;
    int prev_cost = (base) * (base + 1) / 2;
    return ((total_cost - prev_cost) * 100);
}

static int gain_skills_initial_skill = -1;

void gain_skills_set_initial_skill(int skill)
{
    if (skill < 0 || skill >= S_MAX || skill == S_SPC)
        gain_skills_initial_skill = -1;
    else
        gain_skills_initial_skill = skill;
}

/*
 * Increase your skills by spending experience points.
 */
extern NavResult gain_skills(void)
{
    int i;

    int skill = ((gain_skills_initial_skill >= 0
        && gain_skills_initial_skill < S_MAX
        && gain_skills_initial_skill != S_SPC)
        ? gain_skills_initial_skill
        : 0);

    int old_base[S_MAX];
    int skill_gain[S_MAX];
    int skill_costs[S_MAX];

    int old_new_exp = p_ptr->new_exp;
    int total_cost = 0;

    char ch;

    NavResult result = NAV_OK;

    bool death_view = death_spectator_active();

    log_debug("Starting skills allocation with %d experience points", p_ptr->new_exp);
    gain_skills_initial_skill = -1;

    // hack global variable
    skill_gain_in_progress = true;

    /* save the old skills */
    for (i = 0; i < S_MAX; i++)
        old_base[i] = p_ptr->skill_base[i];

    /* initialise the skill gains */
    for (i = 0; i < S_MAX; i++)
        skill_gain[i] = 0;

    /* Interact */
    while (1)
    {
        bool steamdeck = steamdeck_controls_active();

        /* Recompute points/costs and apply the temporary skill increases */
        total_cost = 0;

        for (i = 0; i < S_MAX; i++)
        {
            /* Skip Special abilities skill - not trainable */
            if (i == S_SPC)
            {
                skill_costs[i] = 0;
                continue;
            }
            skill_costs[i] = birth_skill_cost(old_base[i], skill_gain[i]);
            total_cost += skill_costs[i];
        }

        p_ptr->new_exp = old_new_exp - total_cost;

        if (p_ptr->new_exp < 0)
        {
            bell("Excessive skills!");
            skill_gain[skill]--;
            continue;
        }

        p_ptr->update |= (PU_BONUS);
        p_ptr->redraw |= (PR_EXP | PR_BASIC);

        for (i = 0; i < S_MAX; i++)
        {
            if (i == S_SPC) continue;
            p_ptr->skill_base[i] = old_base[i] + skill_gain[i];
        }

        update_stuff();

        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);

        sdl_character_sheet_screen_show_birth_skills(old_base,
            skill_gain, skill_costs, skill, p_ptr->new_exp);

        /* First-time players: guided callouts over the real skills screen. */
        birth_coach_show_once(BIRTH_COACH_SKILLS);

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
                if (clicked_choice >= 0 && clicked_choice < S_MAX
                    && clicked_choice != S_SPC)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != skill)
                    {
                        skill = clicked_choice;
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
        if (((ch == 'Q') || (ch == 'q')) && (turn == 0)) {
            /* restore state before leaving */
            p_ptr->new_exp = old_new_exp;
            for (i = 0; i < S_MAX; i++) {
                if (i != S_SPC) /* Don't restore Special abilities skill */
                    p_ptr->skill_base[i] = old_base[i];
            }
            skill_gain_in_progress = false;
            ui_menu_click_clear();
            sdl_character_sheet_screen_hide();
            return NAV_TO_CHARACTER;
        }

        /* Done */
        if (birth_confirm_input(ch, steamdeck))
        {
            ui_menu_click_clear();
            result = NAV_OK;
            break;
        }

        /* Abort */
        if (steamdeck && ch == steamdeck_back_key())
            ch = ESCAPE;
        if (ch == ESCAPE)
        {
            p_ptr->new_exp = old_new_exp;
            for (i = 0; i < S_MAX; i++) {
                if (i != S_SPC) /* Don't restore Special abilities skill */
                    p_ptr->skill_base[i] = old_base[i];
            }
            ui_menu_click_clear();
            result = NAV_BACK;   /* go back to stat allocation */
            break;
        }

        /* Prev skill */
        if (ch == '8')
        {
            do {
                skill = (skill + S_MAX - 1) % S_MAX;
            } while (skill == S_SPC); /* Skip Special abilities skill */
        }

        /* Next skill */
        if (ch == '2')
        {
            do {
                skill = (skill + 1) % S_MAX;
            } while (skill == S_SPC); /* Skip Special abilities skill */
        }

        /* Final Look retains this browser but not skill purchases. */
        if ((ch == '4') || (ch == '6'))
        {
            if (death_view)
            {
                msg_print("You cannot do that during this final look.");
                continue;
            }
        }

        /* Decrease skill */
        if ((ch == '4') && (skill_gain[skill] > 0))
            skill_gain[skill]--;

        /* Increase stat */
        if (ch == '6')
        {
            /* Don't allow increasing Special abilities skill */
            if (skill != S_SPC) {
                skill_gain[skill]++;
            }
        }
    }

    // reset hack global variable
    ui_menu_click_clear();
    sdl_character_sheet_screen_hide();
    skill_gain_in_progress = false;

    /* Calculate the bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Update stuff */
    update_stuff();

    /* Offer newly relevant shortcuts only after the allocation is committed;
     * backing out of this screen must never mutate the SDL layout. */
    if (result == NAV_OK && character_generated && old_base[S_SNG] == 0
        && p_ptr->skill_base[S_SNG] > 0)
    {
        sdl_quick_access_suggest_skill_shortcut(S_SNG);
    }
    if (result == NAV_OK && character_generated && old_base[S_SMT] == 0
        && p_ptr->skill_base[S_SMT] > 0)
    {
        sdl_quick_access_suggest_skill_shortcut(S_SMT);
    }

    log_debug("Skills allocation completed, spent %d experience", old_new_exp - p_ptr->new_exp);

    /* Done */
    return result;
}
