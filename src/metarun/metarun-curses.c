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

static void curse_menu_fade_title(void)
{
    const int duration_ms = 450;
    const int frame_ms = 16;

    sdl_poetry_screen_set_alpha(0, 0, 0, 0);
    for (int elapsed = 0; elapsed < duration_ms; elapsed += frame_ms)
    {
        byte alpha = (byte)((elapsed * 255) / duration_ms);

        sdl_poetry_screen_set_alpha(alpha, 0, 0, 0);
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY,
            MIN(frame_ms, duration_ms - elapsed));
    }
    sdl_poetry_screen_set_alpha(255, 0, 0, 0);
    Term_fresh();
}

static bool curse_menu_fade_choice(int choice)
{
    const int duration_ms = 600;
    const int frame_ms = 16;

    sdl_poetry_screen_set_choice_visible(choice, true,
        TERM_L_RED, TERM_SLATE);
    sdl_poetry_screen_set_choice_alpha(choice, 0);
    for (int elapsed = 0; elapsed < duration_ms; elapsed += frame_ms)
    {
        char ch;
        byte alpha = (byte)((elapsed * 255) / duration_ms);

        if (Term_inkey(&ch, false, false) == 0 && ch == ESCAPE)
        {
            sdl_poetry_screen_set_choice_alpha(choice, 255);
            Term_fresh();
            return false;
        }
        sdl_poetry_screen_set_choice_alpha(choice, alpha);
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY,
            MIN(frame_ms, duration_ms - elapsed));
    }
    sdl_poetry_screen_set_choice_alpha(choice, 255);
    Term_fresh();
    return true;
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

static int menu_choose_one_curse_aux(int n, bool force_menu)
{
    /* if any active curse has the "no-choice" flag, skip the menu */
    if (!force_menu && any_curse_flag_active(CUR_NOCHOICE))
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
    char str[96];
    cptr ordinal;

    switch (n)
    {
        case 1: ordinal = "the second"; break;
        case 2: ordinal = "the third"; break;
        case 3: ordinal = "the fourth"; break;
        default: ordinal = "a"; break;
    }
    strnfmt(str, sizeof(str),
        "Dark powers demand their price - choose %s curse:", ordinal);
    sdl_poetry_screen_begin_choices(str);
    if (!sdl_poetry_screen_active())
        quit("Mandatory SDL curse menu renderer is unavailable");
    for (int i = 0; i < CURSE_MENU_LINES; i++)
    {
        curse_type* cu = &cu_info[pick[i]];
        char name_buf[128];

        if (menu_letters)
            strnfmt(name_buf, sizeof(name_buf), "%c) %s", 'a' + i,
                cu_name + cu->name);
        else
            SDL_strlcpy(name_buf, cu_name + cu->name, sizeof(name_buf));
        sdl_poetry_screen_add_choice(i, name_buf, cu_text + cu->text);
    }
    curse_menu_fade_title();
    Term_xtra(TERM_XTRA_DELAY, 1000);

    /* Show each curse one by one with fade-in effect */
    bool fast_forward = false;

    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        if (fast_forward)
        {
            sdl_poetry_screen_set_choice_visible(i, true,
                TERM_L_RED, TERM_SLATE);
            sdl_poetry_screen_set_choice_alpha(i, 255);
        }
        else if (!curse_menu_fade_choice(i))
            fast_forward = true;

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
    }

    sdl_poetry_screen_set_prompt(curse_prompt, true);

    /* Menu navigation variables */
    int highlight = 0;  /* Currently highlighted option (0, 1, 2) */
    bool menu_done = false;

    while (!menu_done) {
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        sdl_poetry_screen_set_highlight(highlight);
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
    sdl_poetry_screen_hide();
    screen_pop_supporting_panes_hidden();
    screen_load();
    return pick[sel];
}

int menu_choose_one_curse(int n)
{
    return menu_choose_one_curse_aux(n, false);
}

int metarun_debug_preview_curse_menu(void)
{
    return menu_choose_one_curse_aux(0, true);
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

static int choose_escape_curses_ui_aux(int n, int out[4], bool preview)
{
    // int rolls = any_curse_flag_active(CUR_NOCHOICE) ? 1 : n;
    int taken = 0;

    /* Display intro with fade-in effect */
    screen_save();
    screen_push_supporting_panes_hidden();
    Term_clear();

    char intro_text[512];
    strnfmt(intro_text, sizeof(intro_text),
            "The Valar watch silently as Morgoth's malice reaches out from shadow-"
            "Your triumph has drawn his wrath. His dark will twists fate, "
            "forcing upon you the final choice-%s curse%s you must bear.",
            (n == 1) ? "a" : (n == 2) ? "two" : (n == 3) ? "three" : "four",
            (n == 1) ? "" : "s");

    metarun_show_poetry_scene("The Valar's Judgment", TERM_L_BLUE,
        intro_text, TERM_L_WHITE, "", TERM_SLATE,
        "[Press any key to face your destiny]");
    Term_clear();

    for (int i = 0; i < n; i++)
    {
        int idx = preview ? menu_choose_one_curse_aux(i, true)
                          : menu_choose_one_curse(i);

        if (idx < 0)
            break;
        log_debug("Player selected curse %d: %s", idx, cu_name + cu_info[idx].name);
        if (!preview)
            add_curse_stack(idx);            /* gameplay side-effect */
        if (taken < 4) out[taken++] = idx;
    }

    /* Wipe the menu clutter so narrative starts clean */
    Term_clear();

    /* Restore screen state to fix character_icky imbalance */
    screen_pop_supporting_panes_hidden();
    screen_load();

    return taken;
}

int choose_escape_curses_ui(int n, int out[4])
{
    return choose_escape_curses_ui_aux(n, out, false);
}

int metarun_debug_choose_escape_curses(int n, int out[4])
{
    return choose_escape_curses_ui_aux(n, out, true);
}

/****************  Oath-breaking curse chooser with fade ************/

/*
 * Shows the oath-specific consequence and Morgoth's response with fade-in,
 * then opens the permanent curse selection menu.
 * Returns the selected curse index.
 */
int choose_oath_breaking_curse_ui(int oath_id)
{
    /* Display curse message with fade-in effect */
    screen_save();
    screen_push_supporting_panes_hidden();
    Term_clear();

    /* Get oath-specific permanent message (E: field from oath.txt) */
    char* perm_msg = oath_permanent_message(oath_id);
    cptr consequence = (perm_msg && perm_msg[0])
        ? perm_msg : "Your oath is forever broken in this age.";

    /* Show Morgoth's attention text with fade in red */
    char intro_text[256];
    strnfmt(intro_text, sizeof(intro_text),
            "The breach of your sacred vow has drawn Morgoth's attention. "
            "His malice reaches out to compound your suffering with a curse you must bear.");

    metarun_show_poetry_scene("The Sundering of Sacred Vows", TERM_L_RED,
        consequence, TERM_L_RED, intro_text, TERM_RED,
        "[Press any key to face your judgment]");
    Term_clear();

    /* Let the player choose 1 curse from 3 options */
    int idx = menu_choose_one_curse(0);
    log_debug("Player selected curse %d for oath breaking", idx);

    /* Wipe the menu clutter so narrative starts clean */
    Term_clear();

    /* Restore screen state */
    screen_pop_supporting_panes_hidden();
    screen_load();

    return idx;
}
