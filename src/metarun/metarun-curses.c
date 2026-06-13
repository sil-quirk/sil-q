#include "angband.h"
#include "metarun-internal.h"

int any_curse_flag_active(u32b flag)
{
    /* Intended for CUR flags such as CUR_NOCHOICE (curse-only, not blessings). */
    if (!z_info || !cu_info) return 0;
    int count = 0;
    for (int id = 0; id < z_info->cu_max; id++) {
        int stacks = CURSE_GET(id);
        if (stacks > 0 && (cu_info[id].flags_u & flag)) count += stacks;
    }
    return count;
}

static int weighted_random_curse(void)
{
    long total = 0;
    int  w_max = 1;

    /* Does the hero's lineage carry the flag? */
    bool tilt = (p_info[p_ptr->prace].flags  & RHF_CURSE) ||
                (c_info[p_ptr->pcharacter].flags & RHF_CURSE);

    /* Pass 1 - find the largest weight and (later) build the total */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* <- unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        if (w > w_max) w_max = w;
    }

    /* Pass 2 - sum effective weights */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* <- unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        int  cnt = CURSE_CURSE_STACK(i);
        byte cap = (byte)CURSE_CURSE_CAP(i);
        if (cap && cnt >= cap) continue;           /* cap reached */

        /* RHF_CURSE excludes the most weighted choices */
        if (tilt && w == w_max) continue;

        long base = tilt
            ? w + ((w_max + 1 - w) >> 1)           /* lift the tail */
            : w;

        total += base / (cnt + 1);
    }

    if (!total) return rand_int(z_info->cu_max);    /* safety net */

    /* Pass 3 - roulette wheel */
    long pick = rand_int(total), run = 0;
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* <- unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        int  cnt = CURSE_CURSE_STACK(i);
        byte cap = (byte)CURSE_CURSE_CAP(i);
        if (cap && cnt >= cap) continue;

        /* RHF_CURSE excludes the most weighted choices */
        if (tilt && w == w_max) continue;

        long base = tilt
            ? w + ((w_max + 1 - w) >> 1)
            : w;

        long eff = base / (cnt + 1);
        run += eff;
        if (pick < run) return i;
    }

    return rand_int(z_info->cu_max);                /* unreachable */
}

void add_curse_stack(int idx)
{
    /* respect per-curse stack cap */
    if (CURSE_CURSE_CAP(idx) &&
        CURSE_CURSE_STACK(idx) >= CURSE_CURSE_CAP(idx))
    {
        log_debug("Curse %d (%s) already at max stacks", idx, cu_name + cu_info[idx].name);
        return;
    }

    CURSE_ADD(idx, 1);
    log_info("Added curse stack: %s (now %d stacks)", cu_name + cu_info[idx].name, CURSE_GET(idx));
    save_metaruns();
}

int menu_choose_one_curse(int n)
{
    /* if any active curse has the "no-choice" flag, skip the menu */
    if (any_curse_flag_active(CUR_NOCHOICE))
        return weighted_random_curse();

    int pick[CURSE_MENU_LINES], sel;
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();

    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        bool dup;
        do {
            dup     = false;
            pick[i] = weighted_random_curse();
            for (int j = 0; j < i; j++)
                if (pick[i] == pick[j]) { dup = true; break; }

            byte cap = (byte)CURSE_CURSE_CAP(pick[i]);
            if (cap && CURSE_CURSE_STACK(pick[i]) >= cap) { dup = true; continue; }

        } while (dup);
    }

    screen_save();
    screen_push_supporting_panes_hidden();
    Term_clear();

    /* Fade in the title */
    char str[60];
    const char* seq[] = { "a", "the second", "the third" };
    strnfmt(str, sizeof(str), "Dark powers demand their price - choose %s curse:", seq[n]);
    print_heading_fade(str, TERM_YELLOW);

    /* dynamic vertical layout - ask support/text-output.c to count wrapped lines */
    int row = 4;                                     /* first free row */
    text_out_hook = text_out_to_screen;
    text_out_wrap = metarun_term_width() - 2;        /* full width     */

    /* Show each curse one by one with fade-in effect */
    bool fast_forward = false;

    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        curse_type *cu = &cu_info[pick[i]];
        char name_buf[128];
        if (!menu_letters)
            strnfmt(name_buf, sizeof name_buf, "   %s", cu_name + cu->name);
        else
            strnfmt(name_buf, sizeof name_buf, "%c) %s", 'a'+i, cu_name + cu->name);

        const char *txt = cu_text + cu->text;
        int need_lines = count_wrapped_lines(txt, text_out_wrap, 4);

#ifdef DEBUG_CURSES
        const char *pow = cu_text + cu->power;
        int need_pow_lines = 0;
        if (*pow) {
            need_pow_lines = count_wrapped_lines(pow, text_out_wrap, 4);
        }
#endif

        /* Fade in all text for this curse simultaneously */
        const byte fade_cols[] = { TERM_L_DARK, TERM_SLATE, TERM_L_WHITE };
        const int steps = (int)(sizeof(fade_cols) / sizeof(fade_cols[0]));

        for (int s = 0; s < steps && !fast_forward; s++)
        {
            /* Check for ESC key to skip fade */
            char ch;
            if (Term_inkey(&ch, false, false) == 0 && ch == ESCAPE)
            {
                fast_forward = true;
                break;
            }

            /* Name line */
            c_put_str(s == steps - 1 ? TERM_L_RED : fade_cols[s], name_buf, row, 2);

            /* Poem text */
            Term_gotoxy(4, row + 2);
            text_out_c(s == steps - 1 ? TERM_SLATE : fade_cols[s], txt);

#ifdef DEBUG_CURSES
            /* Power text if present */
            if (*pow) {
                Term_gotoxy(4, row + need_lines + 2);
                text_out_c(s == steps - 1 ? TERM_L_RED : fade_cols[s], pow);
            }
#endif

            Term_fresh();
            Term_xtra(TERM_XTRA_DELAY, 200);
        }

        /* If fade was interrupted, show final state immediately */
        if (fast_forward) {
            c_put_str(TERM_L_RED, name_buf, row, 2);
            Term_gotoxy(4, row + 2);
            text_out_c(TERM_SLATE, txt);

#ifdef DEBUG_CURSES
            if (*pow) {
                Term_gotoxy(4, row + need_lines + 2);
                text_out_c(TERM_L_RED, pow);
            }
#endif
        }

        /* Move to next curse position */
#ifdef DEBUG_CURSES
        if (*pow)
            row += need_lines + need_pow_lines + 3;
        else
#endif
            row += need_lines + 3;

        /* 1 second delay between curses (except for the last one) */
        if (i < CURSE_MENU_LINES - 1) {
            Term_xtra(TERM_XTRA_DELAY, 1000);
        }
    }

    /* Show the prompt immediately without fade */
    char curse_prompt[96];
    curse_prompt[0] = '\0';
    if (steamdeck)
    {
        char accept_label[16];
        char back_label[16];
        char hint_buf[96];

        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label,
            sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        {
            char prompt_full[96];
            char prompt_short[80];
            const char* variants[2];

            strnfmt(prompt_full, sizeof(prompt_full),
                "D-pad navigate  [%s] accept  [%s] cancel",
                accept_label, back_label);
            strnfmt(prompt_short, sizeof(prompt_short),
                "[%s] accept  [%s] cancel", accept_label, back_label);
            variants[0] = prompt_full;
            variants[1] = prompt_short;
            terminal_prompt_pick_variant(hint_buf, sizeof(hint_buf),
                metarun_term_width() - 2, false, variants,
                N_ELEMENTS(variants));
        }
        SDL_strlcpy(curse_prompt, hint_buf, sizeof(curse_prompt));
        c_put_str(TERM_L_DARK, hint_buf, row + 1, 2);
    }
    else if (sdl_touch_only_device_active())
    {
        char prompt[96];
        const char* variants[] = {
            "Tap a row to choose your curse",
            "Tap a row to choose",
            "Tap to choose"
        };
        terminal_prompt_pick_variant(prompt, sizeof(prompt),
            metarun_term_width() - 2, false, variants, N_ELEMENTS(variants));
        SDL_strlcpy(curse_prompt, prompt, sizeof(curse_prompt));
        c_put_str(TERM_L_DARK, prompt, row + 1, 2);
    }
    else if (menu_letters)
    {
        char prompt[96];
        const char* variants[] = {
            "Dir navigate  Enter accept  a/b/c select",
            "Enter accept  a/b/c select"
        };
        terminal_prompt_pick_variant(prompt, sizeof(prompt),
            metarun_term_width() - 2, false, variants, N_ELEMENTS(variants));
        SDL_strlcpy(curse_prompt, prompt, sizeof(curse_prompt));
        c_put_str(TERM_L_DARK, prompt, row + 1, 2);
    }
    else
    {
        char prompt[96];
        const char* variants[] = {
            "Dir navigate  Enter accept",
            "Enter accept"
        };
        terminal_prompt_pick_variant(prompt, sizeof(prompt),
            metarun_term_width() - 2, false, variants, N_ELEMENTS(variants));
        SDL_strlcpy(curse_prompt, prompt, sizeof(curse_prompt));
        c_put_str(TERM_L_DARK, prompt, row + 1, 2);
    }

    /* Menu navigation variables */
    int highlight = 0;  /* Currently highlighted option (0, 1, 2) */
    bool menu_done = false;
    int option_rows[CURSE_MENU_LINES];  /* Store the row for each option */

    /* Calculate row positions for each option */
    int calc_row = 4;
    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        option_rows[i] = calc_row;
        curse_type *cu = &cu_info[pick[i]];
        const char *txt = cu_text + cu->text;
        int need_lines = count_wrapped_lines(txt, text_out_wrap, 4);
        calc_row += need_lines + 3;
    }

    while (!menu_done) {
        /* Ensure text output settings are consistent */
        text_out_hook = text_out_to_screen;
        text_out_wrap = metarun_term_width() - 2;
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);

        /* Update highlight display for each option */
        for (int i = 0; i < CURSE_MENU_LINES; i++) {
            curse_type *cu = &cu_info[pick[i]];
            char name_buf[128];
            if (!menu_letters)
                strnfmt(name_buf, sizeof name_buf, "   %s", cu_name + cu->name);
            else
                strnfmt(name_buf, sizeof name_buf, "%c) %s", 'a'+i, cu_name + cu->name);

            /* Clear the line first to remove any previous highlighting */
            Term_erase(2, option_rows[i], strlen(name_buf));

            /* Display the option with highlighting */
            if (i == highlight) {
                c_put_str(TERM_RED, name_buf, option_rows[i], 2);     /* Highlighted - red */
            } else {
                c_put_str(TERM_L_RED, name_buf, option_rows[i], 2);   /* Normal - light red */
            }
            ui_menu_click_add(i, 2, option_rows[i], metarun_term_width() - 4);
        }
        ui_menu_click_add_text_token(-1, 2, row + 1,
            curse_prompt, "cancel");

        /* Position cursor at the end of the highlighted option text */
        curse_type *highlighted_cu = &cu_info[pick[highlight]];
        char highlighted_name_buf[128];
        if (!menu_letters)
            strnfmt(highlighted_name_buf, sizeof highlighted_name_buf, "   %s",
                cu_name + highlighted_cu->name);
        else
            strnfmt(highlighted_name_buf, sizeof highlighted_name_buf, "%c) %s",
                'a'+highlight, cu_name + highlighted_cu->name);
        int cursor_col = 2 + strlen(highlighted_name_buf);
        Term_gotoxy(cursor_col, option_rows[highlight]);
        Term_fresh();
        char key = metarun_inkey_hidden();

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice < CURSE_MENU_LINES)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != highlight)
                    {
                        highlight = clicked_choice;
                        continue;
                    }
                    key = '\r';
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                else if (clicked_choice == -1)
                    key = ESCAPE;
                else if (clicked_choice == -2)
                    key = '\r';
            }
        }

        /* Handle input */
        if (menu_letters && key >= 'a' && key < 'a' + CURSE_MENU_LINES) {
            /* Letter shortcuts */
            sel = key - 'a';
            menu_done = true;
        }
        else if (menu_letters && key >= 'A' && key < 'A' + CURSE_MENU_LINES) {
            /* Capital letter shortcuts */
            sel = key - 'A';
            menu_done = true;
        }
        else if (key == '\r' || key == '\n' || key == ' ' || key == '6'
            || (steamdeck && key == steamdeck_confirm_key())) {
            /* Enter, Space, or numpad 6 - select current highlight */
            sel = highlight;
            menu_done = true;
        }
        else if (key == '8' || key == 'k') {
            /* Up navigation */
            highlight = (highlight + CURSE_MENU_LINES - 1) % CURSE_MENU_LINES;
        }
        else if (key == '2' || key == 'j') {
            /* Down navigation */
            highlight = (highlight + 1) % CURSE_MENU_LINES;
        }
        else if (key == ESCAPE || (steamdeck && key == steamdeck_back_key())) {
            /* Escape - default to first option */
            sel = 0;
            menu_done = true;
        }
    }
    ui_menu_click_clear();
    screen_pop_supporting_panes_hidden();
    screen_load();
    return pick[sel];
}


/* ------------------------------------------------------------------ *
 *  Debug helper - wipe every active curse for the current meta-run.  *
 * ------------------------------------------------------------------ */
void metarun_clear_all_curses(void)
{
    log_info("Clearing all curses for current metarun");
    memset(metar.curse_stacks, 0, sizeof(metar.curse_stacks));
    metar.curses_seen = 0;
    if (!sync_current_metarun_slot(false)) {
        log_warn("metarun_clear_all_curses: unable to sync current slot");
    }
    save_metaruns();
}

int choose_escape_curses_ui(int n, int out[4])
{
    // int rolls = any_curse_flag_active(CUR_NOCHOICE) ? 1 : n;
    int taken = 0;
    bool fast_forward = false;

    /* Display intro with fade-in effect */
    screen_save();
    screen_push_supporting_panes_hidden();
    Term_clear();

    print_heading_fade("The Valar's Judgment", TERM_L_BLUE);

    char intro_text[512];
    strnfmt(intro_text, sizeof(intro_text),
            "The Valar watch silently as Morgoth's malice reaches out from shadow-"
            "Your triumph has drawn his wrath. His dark will twists fate, "
            "forcing upon you the final choice-%s curse%s you must bear.",
            (n == 1) ? "a" : (n == 2) ? "two" : (n == 3) ? "three" : "four",
            (n == 1) ? "" : "s");

    if (!print_paragraph_fade(intro_text, TERM_L_WHITE, 4))
        fast_forward = true;

    wait_for_keypress_with_prompt("[Press any key to face your destiny]");
    Term_clear();

    for (int i = 0; i < n; i++)
    {
        int idx = menu_choose_one_curse(i);   /* weighted picker, UI */
        log_debug("Player selected curse %d: %s", idx, cu_name + cu_info[idx].name);
        add_curse_stack(idx);                /* gameplay side-effect */
        if (taken < 4) out[taken++] = idx;
    }

    /* Wipe the menu clutter so narrative starts clean */
    Term_clear();

    /* Restore screen state to fix character_icky imbalance */
    screen_pop_supporting_panes_hidden();
    screen_load();

    /* Avoid unused variable warning */
    (void)fast_forward;

    return taken;
}

/****************  Oath-breaking curse chooser with fade ************/

/*
 * Shows the oath-specific curse message with fade-in, waits 3 seconds,
 * then shows the permanent consequence message and curse selection menu.
 * Returns the selected curse index.
 */
int choose_oath_breaking_curse_ui(int oath_id)
{
    bool fast_forward = false;

    /* Display curse message with fade-in effect */
    screen_save();
    screen_push_supporting_panes_hidden();
    Term_clear();

    /* Add Tolkien-style heading */
    print_heading_fade("The Sundering of Sacred Vows", TERM_L_RED);

    /* Get oath-specific permanent message (E: field from oath.txt) */
    char* perm_msg = oath_permanent_message(oath_id);

    /* Add empty line before E: text */
    Term_putstr(2, 4, -1, TERM_SLATE, "");

    /* Show only the permanent message (E: field) with fade */
    if (perm_msg && perm_msg[0]) {
        if (!print_paragraph_fade(perm_msg, TERM_L_RED, 5))
            fast_forward = true;
    } else {
        if (!print_paragraph_fade("Your oath is forever broken in this age.", TERM_L_RED, 5))
            fast_forward = true;
    }

    /* Hold the message for 3 seconds if not fast-forwarded */
    if (!fast_forward) {
        Term_xtra(TERM_XTRA_DELAY, 3000);
    }

    /* Add empty line before attention text */
    Term_putstr(2, 8, -1, TERM_SLATE, "");

    /* Show Morgoth's attention text with fade in red */
    char intro_text[256];
    strnfmt(intro_text, sizeof(intro_text),
            "The breach of your sacred vow has drawn Morgoth's attention. "
            "His malice reaches out to compound your suffering with a curse you must bear.");

    if (!print_paragraph_fade(intro_text, TERM_RED, 9))
        fast_forward = true;

    wait_for_keypress_with_prompt("[Press any key to face your judgment]");
    Term_clear();

    /* Let the player choose 1 curse from 3 options */
    int idx = menu_choose_one_curse(0);
    log_debug("Player selected curse %d for oath breaking", idx);

    /* Wipe the menu clutter so narrative starts clean */
    Term_clear();

    /* Restore screen state */
    screen_pop_supporting_panes_hidden();
    screen_load();

    /* Avoid unused variable warning */
    (void)fast_forward;

    return idx;
}
