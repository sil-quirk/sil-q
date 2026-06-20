#include "angband.h"
#include "metarun-internal.h"

static int blessing_points_remaining(void)
{
    int earned = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
    int spent = metar.blessing_points_spent;
    if (spent > earned) spent = earned;
    int available = earned - spent;
    if (available < 0) available = 0;
    return available;
}

static void blessing_spend_points(int cost)
{
    if (cost <= 0) return;
    int earned = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
    int spent = metar.blessing_points_spent + cost;
    if (spent > earned) spent = earned;
    if (spent < 0) spent = 0;
    metar.blessing_points_spent = (u16b)spent;
}

static void blessing_put_choice_prompt(int row, bool steamdeck,
    bool menu_letters, cptr accept_label, cptr back_label, cptr back_word)
{
    char prompt[128];
    int width = metarun_term_width() - 2;

    if (width < 1)
        width = 1;

    if (steamdeck)
    {
        char prompt_full[128];
        char prompt_short[96];
        const char* variants[2];

        strnfmt(prompt_full, sizeof(prompt_full),
            "D-pad navigate  [%s] accept  [%s] %s", accept_label,
            back_label, back_word ? back_word : "cancel");
        strnfmt(prompt_short, sizeof(prompt_short), "[%s] accept  [%s] %s",
            accept_label, back_label, back_word ? back_word : "cancel");
        variants[0] = prompt_full;
        variants[1] = prompt_short;
        terminal_prompt_pick_variant(prompt, sizeof(prompt), width, false,
            variants, N_ELEMENTS(variants));
    }
    else if (sdl_touch_only_device_active())
    {
        const char* variants[] = {
            "Tap a row to select, tap away to exit",
            "Tap to select, tap away to exit",
            "Tap to select"
        };
        terminal_prompt_pick_variant(prompt, sizeof(prompt), width, false,
            variants, N_ELEMENTS(variants));
    }
    else if (menu_letters)
    {
        char prompt_full[128];
        char prompt_mid[96];
        char prompt_short[80];
        const char* variants[3];

        strnfmt(prompt_full, sizeof(prompt_full),
            "Dir navigate  Enter accept  Letter select  Esc %s",
            back_word ? back_word : "cancel");
        strnfmt(prompt_mid, sizeof(prompt_mid),
            "Dir navigate  Enter accept  Esc %s",
            back_word ? back_word : "cancel");
        strnfmt(prompt_short, sizeof(prompt_short), "Enter accept  Esc %s",
            back_word ? back_word : "cancel");
        variants[0] = prompt_full;
        variants[1] = prompt_mid;
        variants[2] = prompt_short;
        terminal_prompt_pick_variant(prompt, sizeof(prompt), width, false,
            variants, N_ELEMENTS(variants));
    }
    else
    {
        char prompt_full[96];
        char prompt_short[80];
        const char* variants[2];

        strnfmt(prompt_full, sizeof(prompt_full),
            "Dir navigate  Enter accept  Esc %s",
            back_word ? back_word : "cancel");
        strnfmt(prompt_short, sizeof(prompt_short), "Enter accept  Esc %s",
            back_word ? back_word : "cancel");
        variants[0] = prompt_full;
        variants[1] = prompt_short;
        terminal_prompt_pick_variant(prompt, sizeof(prompt), width, false,
            variants, N_ELEMENTS(variants));
    }

    Term_putstr(2, row, -1, TERM_L_DARK, prompt);
    ui_menu_click_add_text_token(-1, 2, row, prompt,
        back_word ? back_word : "cancel");
    ui_menu_click_add_text_token(-1, 2, row, prompt, "Esc cancel");
}

static void blessing_commit_changes(bool apply_runtime)
{
    if (!sync_current_metarun_slot(false)) {
        log_warn("blessing_commit_changes: unable to sync current slot (idx=%d, max=%d)",
                 current_run, metarun_max);
    }
    refresh_current_metar_score();
    if (apply_runtime) {
        metarun_apply_runtime_effects();
    }
    save_metaruns();
}

bool metarun_inline_remove_curse(int id)
{
    int stacks;

    if (blessing_points_remaining() < 1 || id < 0 || id >= z_info->cu_max)
        return false;
    stacks = CURSE_CURSE_STACK(id);
    if (stacks <= 0)
        return false;
    CURSE_SET(id, stacks - 1);
    CURSE_SEEN_SET(id);
    blessing_spend_points(1);
    metar.pending_blessing_count = 0;
    for (int i = 0; i < 3; i++)
        metar.pending_blessing_choices[i] = 255;
    blessing_commit_changes(true);
    return true;
}

int metarun_inline_minor_blessing_choices(int out[3])
{
    int picks = 0;

    if (!out)
        return 0;
    for (int i = 0; i < metar.pending_blessing_count && i < 3; i++) {
        int id = metar.pending_blessing_choices[i];
        int stacks;
        if (id < 0 || id >= z_info->cu_max || id == 255
            || !cu_info[id].blessing_name)
            continue;
        stacks = CURSE_GET(id);
        if (stacks > 0)
            continue;
        if (CURSE_BLESSING_CAP(id) > 0
            && -MIN(stacks, 0) >= CURSE_BLESSING_CAP(id))
            continue;
        out[picks++] = id;
    }
    if (picks > 0)
        return picks;

    int eligible[METAR_CURSE_SLOTS];
    int weights[METAR_CURSE_SLOTS];
    int count = 0;
    int total_weight = 0;
    for (int id = 0; id < z_info->cu_max && count < METAR_CURSE_SLOTS; id++) {
        int stacks;
        int blessing_stacks;
        int base_weight;
        if (!cu_info[id].blessing_name)
            continue;
        stacks = CURSE_GET(id);
        if (stacks > 0)
            continue;
        blessing_stacks = -MIN(stacks, 0);
        if (CURSE_BLESSING_CAP(id) > 0
            && blessing_stacks >= CURSE_BLESSING_CAP(id))
            continue;
        eligible[count] = id;
        base_weight = cu_info[id].weight > 0 ? cu_info[id].weight : 1;
        weights[count] = MAX(1, base_weight / (blessing_stacks + 1));
        total_weight += weights[count];
        count++;
    }
    picks = MIN(3, count);
    for (int i = 0; i < picks; i++) {
        int roll = rand_int(total_weight);
        int sum = 0;
        int selected = 0;
        for (int j = 0; j < count; j++) {
            sum += weights[j];
            if (roll < sum) {
                selected = j;
                break;
            }
        }
        out[i] = eligible[selected];
        total_weight -= weights[selected];
        eligible[selected] = eligible[count - 1];
        weights[selected] = weights[count - 1];
        count--;
    }
    metar.pending_blessing_count = picks;
    for (int i = 0; i < 3; i++)
        metar.pending_blessing_choices[i] = (i < picks) ? out[i] : 255;
    if (picks > 0)
        save_metaruns();
    return picks;
}

bool metarun_inline_choose_minor_blessing(int id)
{
    int choices[3];
    int count;
    int stacks;
    bool offered = false;

    if (blessing_points_remaining() < 1)
        return false;
    count = metarun_inline_minor_blessing_choices(choices);
    for (int i = 0; i < count; i++)
        if (choices[i] == id) offered = true;
    if (!offered)
        return false;
    stacks = CURSE_GET(id);
    if (stacks > 0 || (CURSE_BLESSING_CAP(id) > 0
            && -MIN(stacks, 0) >= CURSE_BLESSING_CAP(id)))
        return false;
    CURSE_ADD(id, -1);
    CURSE_SEEN_SET(id);
    blessing_spend_points(1);
    metar.pending_blessing_count = 0;
    for (int i = 0; i < 3; i++)
        metar.pending_blessing_choices[i] = 255;
    blessing_commit_changes(true);
    return true;
}

bool metarun_inline_choose_major_blessing(int idx)
{
    int cost;

    if (idx < 0 || idx >= major_blessing_capacity()
        || !major_blessing_def(idx) || metarun_has_major_blessing_index(idx))
        return false;
    cost = major_blessing_cost(idx);
    if (cost < 0 || cost > blessing_points_remaining())
        return false;
    metar.major_blessings |= (1U << idx);
    blessing_spend_points(cost);
    blessing_commit_changes(true);
    return true;
}

static bool blessing_remove_curse(char *result_msg, size_t msg_size, byte *result_attr)
{
    int ids[METAR_CURSE_SLOTS];
    int count = 0;

    for (int id = 0; id < z_info->cu_max; id++) {
        if (CURSE_CURSE_STACK(id) > 0) {
            if (count < METAR_CURSE_SLOTS) {
                ids[count++] = id;
            }
        }
    }

    if (count == 0) {
        if (result_msg && msg_size > 0) {
            SDL_strlcpy(result_msg, "No curses cling to this saga.", msg_size);
            if (result_attr) *result_attr = TERM_L_DARK;
        }
        return false;
    }

    int selected = 0;
    int choice = -1;
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();
    char accept_label[16] = "";
    char back_label[16] = "";

    if (steamdeck) {
        /* Steam Deck UI: A=accept, B=cancel */
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label, sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
    }

    /* Setup text wrapping */
    text_out_hook = text_out_to_screen;
    text_out_indent = 6;  /* Indent wrapped lines to match description column */
    int wrap_width = metarun_term_width() - 8;  /* Leave margin for indentation */
    text_out_wrap = wrap_width;

    while (choice < 0) {
        screen_save();
        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_outside_cancel_enabled(true);

        Term_putstr(2, 1, -1, TERM_YELLOW, "Remove a Curse (cost 1 blessing point)");
        Term_putstr(2, 3, -1, TERM_L_WHITE, "Choose which curse to lift:");

        int click_width = (metarun_term_width() > 6) ? metarun_term_width() - 4 : 76;
        int line = 5;
        for (int i = 0; i < count; i++) {
            int id = ids[i];
            curse_type *c = &cu_info[id];
            int stacks = CURSE_CURSE_STACK(id);
            int option_line = line;
            /* Display curse name and stacks */
            char buf[128];
            char padded[96];
            metarun_display_pad(padded, sizeof padded,
                                curse_display_name(id), 28);
            if (!menu_letters)
                snprintf(buf, sizeof buf, "   %s stacks: %d", padded, stacks);
            else
                snprintf(buf, sizeof buf, "%c) %s stacks: %d",
                         'a' + i, padded, stacks);

            if (i == selected) {
                Term_putstr(2, line, -1, TERM_L_BLUE, ">");
                Term_putstr(4, line++, -1, TERM_L_RED, buf);

                /* Always show description (D:) with wrapping */
                if (c->text) {
                    cptr desc = cu_text + c->text;
                    Term_gotoxy(6, line);
                    text_out_c(TERM_L_WHITE, desc);
                    line += count_wrapped_lines(desc, wrap_width, 6);
                }

                /* Show power (P:) ONLY if curse is identified, with wrapping */
                bool is_seen = CURSE_SEEN(id);
                log_debug("blessing_remove_curse: curse %d (%s) seen=%d power=%d",
                          id, curse_display_name(id), is_seen, (c->power != 0));
                if (is_seen && c->power) {
                    cptr power = cu_text + c->power;
                    Term_gotoxy(6, line);
                    text_out_c(TERM_SLATE, power);
                    line += count_wrapped_lines(power, wrap_width, 6);
                }
            } else {
                Term_putstr(2, line, -1, TERM_L_DARK, " ");
                Term_putstr(4, line++, -1, TERM_RED, buf);
            }
            ui_menu_click_add(i, 2, option_line, click_width);
        }

        blessing_put_choice_prompt(line + 1, steamdeck, menu_letters,
            accept_label, back_label, "cancel");
        char key = metarun_inkey_hidden();

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice < count)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != selected)
                    {
                        selected = clicked_choice;
                        screen_load();
                        continue;
                    }
                    key = '\r';
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                {
                    screen_load();
                    continue;
                }
                else if (clicked_choice == -1)
                    key = ESCAPE;
                else if (clicked_choice == -2)
                    key = '\r';
            }
        }

        screen_load();
        if (key == UI_MENU_CLICK_WAKE_KEY)
            continue;

        /* Handle back/cancel - ESC or B button in Steam Deck mode */
        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key()) || (!steamdeck && (key == 'h' || key == 'H'))) {
            ui_menu_click_clear();
            /* Reset text wrapping */
            text_out_wrap = 0;
            text_out_indent = 0;
            return false;
        } else if (key == '\r' || key == '\n' || (steamdeck && key == steamdeck_confirm_key()) || key == '6') {
            choice = selected;
            break;
        } else if (key == '8' || key == 'k' || key == '-') {
            selected = (selected + count - 1) % count;
            continue;
        } else if (key == '2' || key == 'j' || key == '+') {
            selected = (selected + 1) % count;
            continue;
        }

        int idx = key - 'a';
        if (menu_letters && idx >= 0 && idx < count) {
            choice = idx;
        } else if (menu_letters && key >= 'A' && key <= 'Z') {
            idx = key - 'A';
            if (idx >= 0 && idx < count) {
                choice = idx;
            } else {
                bell("Invalid selection.");
            }
        } else {
            bell("Invalid selection.");
        }
    }

    ui_menu_click_clear();

    int curse_id = ids[choice];
    int current_stacks = CURSE_CURSE_STACK(curse_id);

    /* Remove only one stack instead of all stacks */
    if (current_stacks > 1) {
        CURSE_SET(curse_id, current_stacks - 1);
    } else {
        CURSE_SET(curse_id, 0);
    }
    CURSE_SEEN_SET(curse_id);

    /* Reset text wrapping */
    text_out_wrap = 0;
    text_out_indent = 0;

    blessing_spend_points(1);

    /* Clear pending blessing choices when removing a curse */
    /* (removing a curse might make new blessings available) */
    metar.pending_blessing_count = 0;
    for (int i = 0; i < 3; i++) {
        metar.pending_blessing_choices[i] = 255;
    }

    blessing_commit_changes(true);

    if (result_msg && msg_size > 0) {
        if (current_stacks > 1) {
            snprintf(result_msg, msg_size, "One stack of %s is lifted. (%d remain%s)",
                     curse_display_name(curse_id),
                     current_stacks - 1,
                     (current_stacks - 1 == 1) ? "s" : "");
        } else {
            snprintf(result_msg, msg_size, "The curse of %s is lifted.", curse_display_name(curse_id));
        }
        if (result_attr) *result_attr = TERM_L_BLUE;
    }
    return true;
}

static bool blessing_gain_minor(char *result_msg, size_t msg_size, byte *result_attr)
{
    int options[3];
    int picks = 0;

    /* Check if we have pending choices that are still valid */
    bool have_valid_pending = false;
    if (metar.pending_blessing_count > 0) {
        /* Validate pending choices - make sure they're still eligible */
        for (int i = 0; i < metar.pending_blessing_count && i < 3; i++) {
            int id = metar.pending_blessing_choices[i];
            if (id == 255) continue; /* Empty slot */

            curse_type *c = &cu_info[id];
            if (!c->blessing_name) continue; /* No longer has blessing */

            int stacks = CURSE_GET(id);
            if (stacks > 0) continue; /* Currently cursed */

            int blessing_stacks = (stacks < 0) ? -stacks : 0;
            if (CURSE_BLESSING_CAP(id) > 0
                && blessing_stacks >= CURSE_BLESSING_CAP(id))
                continue; /* At max */

            /* This pending choice is still valid */
            options[picks++] = id;
        }

        if (picks > 0) {
            have_valid_pending = true;
        }
    }

    /* If we don't have valid pending choices, generate new ones */
    if (!have_valid_pending) {
        int eligible[METAR_CURSE_SLOTS];
        int weights[METAR_CURSE_SLOTS];
        int count = 0;
        int total_weight = 0;

        /* Build list of eligible blessings with their weights */
        for (int id = 0; id < z_info->cu_max; id++) {
            curse_type *c = &cu_info[id];
            if (!c->blessing_name) continue;

            int stacks = CURSE_GET(id);
            if (stacks > 0) continue; /* currently cursed */

            int blessing_stacks = (stacks < 0) ? -stacks : 0;
            if (CURSE_BLESSING_CAP(id) > 0
                && blessing_stacks >= CURSE_BLESSING_CAP(id))
                continue;

            if (count < METAR_CURSE_SLOTS) {
                eligible[count] = id;
                /* Apply weight with diminishing returns for existing stacks (same as curse system) */
                int base_weight = c->weight > 0 ? c->weight : 1;
                int effective_weight = base_weight / (blessing_stacks + 1);
                weights[count] = (effective_weight > 0) ? effective_weight : 1;  /* Minimum weight of 1 */
                total_weight += weights[count];
                count++;
            }
        }

        if (count == 0) {
            if (result_msg && msg_size > 0) {
                SDL_strlcpy(result_msg, "No blessings are presently available.", msg_size);
                if (result_attr) *result_attr = TERM_L_DARK;
            }
            return false;
        }

        /* Select up to 3 blessings using weighted random selection */
        picks = MIN(3, count);

        for (int i = 0; i < picks; i++) {
            /* Weighted random selection from remaining eligible blessings */
            int roll = rand_int(total_weight);
            int sum = 0;
            int selected = 0;

            for (int j = 0; j < count; j++) {
                sum += weights[j];
                if (roll < sum) {
                    selected = j;
                    break;
                }
            }

            options[i] = eligible[selected];

            /* Remove selected blessing from pool for next iteration */
            total_weight -= weights[selected];
            eligible[selected] = eligible[count - 1];
            weights[selected] = weights[count - 1];
            count--;
        }

        /* Store these choices as pending */
        metar.pending_blessing_count = picks;
        for (int i = 0; i < 3; i++) {
            if (i < picks) {
                metar.pending_blessing_choices[i] = options[i];
            } else {
                metar.pending_blessing_choices[i] = 255; /* Empty */
            }
        }
        save_metaruns();
    }

    int selected = 0;
    int choice = -1;
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();
    char accept_label[16] = "";
    char back_label[16] = "";

    if (steamdeck) {
        /* Steam Deck UI: A=accept, B=cancel */
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label, sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
    }
    while (choice < 0) {
        screen_save();
        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_outside_cancel_enabled(true);

        Term_putstr(2, 1, -1, TERM_YELLOW, "Receive a Blessing (cost 1 blessing point)");
        Term_putstr(2, 3, -1, TERM_L_WHITE, "Select a gift to accept:");

        int click_width = (metarun_term_width() > 6) ? metarun_term_width() - 4 : 76;
        int line = 5;
        for (int i = 0; i < picks; i++) {
            int id = options[i];
            curse_type *c = &cu_info[id];
            cptr name = blessing_display_name(id);
            int option_line = line;
            char buf[160];
            if (!menu_letters)
                snprintf(buf, sizeof buf, "   %-30s", name);
            else
                snprintf(buf, sizeof buf, "%c) %-30s", 'a' + i, name);
            if (i == selected) {
                Term_putstr(2, line, -1, TERM_L_BLUE, ">");
                Term_putstr(4, line++, -1, TERM_L_GREEN, buf);

                /* Show both poetic description (E:) and mechanical effect (H:) for selected item */
                if (c->blessing_text) {
                    cptr desc = cu_text + c->blessing_text;
                    Term_putstr(6, line++, -1, TERM_L_WHITE, desc);
                }
                if (c->blessing_power) {
                    cptr power = cu_text + c->blessing_power;
                    Term_putstr(6, line++, -1, TERM_L_GREEN, power);
                }
            } else {
                Term_putstr(2, line, -1, TERM_L_DARK, " ");
                Term_putstr(4, line++, -1, TERM_L_GREEN, buf);
            }
            ui_menu_click_add(i, 2, option_line, click_width);
        }

        blessing_put_choice_prompt(line + 1, steamdeck, menu_letters,
            accept_label, back_label, "cancel");
        char key = metarun_inkey_hidden();

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice < picks)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != selected)
                    {
                        selected = clicked_choice;
                        screen_load();
                        continue;
                    }
                    key = '\r';
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                {
                    screen_load();
                    continue;
                }
                else if (clicked_choice == -1)
                    key = ESCAPE;
                else if (clicked_choice == -2)
                    key = '\r';
            }
        }

        screen_load();
        if (key == UI_MENU_CLICK_WAKE_KEY)
            continue;

        /* Handle back/cancel - ESC or B button in Steam Deck mode */
        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key()) || (!steamdeck && (key == 'h' || key == 'H'))) {
            ui_menu_click_clear();
            return false;
        } else if (key == '\r' || key == '\n' || (steamdeck && key == steamdeck_confirm_key()) || key == '6') {
            choice = selected;
            break;
        } else if (key == '8' || key == 'k' || key == '-') {
            selected = (selected + picks - 1) % picks;
            continue;
        } else if (key == '2' || key == 'j' || key == '+') {
            selected = (selected + 1) % picks;
            continue;
        }

        int idx = key - 'a';
        if (menu_letters && idx >= 0 && idx < picks) {
            choice = idx;
        } else if (menu_letters && key >= 'A' && key <= 'Z') {
            idx = key - 'A';
            if (idx >= 0 && idx < picks) {
                choice = idx;
            } else {
                bell("Invalid selection.");
            }
        } else {
            bell("Invalid selection.");
        }
    }

    ui_menu_click_clear();

    int blessing_id = options[choice];
    int stacks = CURSE_GET(blessing_id);
    int blessing_stacks = (stacks < 0) ? -stacks : 0;

    if (CURSE_BLESSING_CAP(blessing_id) > 0
        && blessing_stacks >= CURSE_BLESSING_CAP(blessing_id)) {
        if (result_msg && msg_size > 0) {
            SDL_strlcpy(result_msg, "That blessing cannot grow any stronger.", msg_size);
            if (result_attr) *result_attr = TERM_L_DARK;
        }
        return false;
    }

    CURSE_ADD(blessing_id, -1);
    CURSE_SEEN_SET(blessing_id);

    blessing_spend_points(1);

    /* Clear pending choices after selection */
    metar.pending_blessing_count = 0;
    for (int i = 0; i < 3; i++) {
        metar.pending_blessing_choices[i] = 255;
    }

    blessing_commit_changes(true);

    if (result_msg && msg_size > 0) {
        snprintf(result_msg, msg_size, "You receive the %s.", blessing_display_name(blessing_id));
        if (result_attr) *result_attr = TERM_L_GREEN;
    }
    return true;
}

static bool blessing_unlock_major(char *result_msg, size_t msg_size, byte *result_attr)
{
    metarun_sanitize_major_blessing_bits(&metar);

    int cap = major_blessing_capacity();
    if (cap <= 0 || !mb_info) {
        if (result_msg && msg_size > 0) {
            SDL_strlcpy(result_msg, "No major blessings are currently defined.", msg_size);
            if (result_attr) *result_attr = TERM_L_DARK;
        }
        return false;
    }

    struct {
        int idx;
        char key;
    } options[16];

    int option_count = 0;
    for (int i = 0; i < cap && option_count < 16; i++) {
        if (metarun_has_major_blessing_index(i)) continue;
        if (!major_blessing_def(i)) continue;
        options[option_count].idx = i;
        options[option_count].key = (char)('a' + option_count);
        option_count++;
    }

    if (option_count == 0) {
        if (result_msg && msg_size > 0) {
            SDL_strlcpy(result_msg, "All major blessings are already sealed.", msg_size);
            if (result_attr) *result_attr = TERM_L_DARK;
        }
        return false;
    }

    int available = blessing_points_remaining();
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();
    char accept_label[16] = "";
    char back_label[16] = "";

    if (steamdeck) {
        /* Steam Deck UI: A=accept, B=cancel */
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label, sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
    }

    /* Find first affordable option as initial selection */
    int selected = -1;
    for (int i = 0; i < option_count; i++) {
        int cost = major_blessing_cost(options[i].idx);
        if (cost <= available) {
            selected = i;
            break;
        }
    }

    /* If no affordable options, show message and return */
    if (selected < 0) {
        if (result_msg && msg_size > 0) {
            snprintf(result_msg, msg_size, "You need %d blessing points to unlock any major blessing.",
                   major_blessing_cost(options[0].idx));
            if (result_attr) *result_attr = TERM_L_DARK;
        }
        return false;
    }

    while (true) {
        screen_save();
        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_outside_cancel_enabled(true);

        Term_putstr(2, 1, -1, TERM_YELLOW, "Unlock a Major Blessing");
        Term_putstr(2, 3, -1, TERM_L_WHITE, "Select which covenant to forge:");

        /* Recalculate in case something changed (shouldn't happen but safe) */
        available = blessing_points_remaining();

        int click_width = (metarun_term_width() > 6) ? metarun_term_width() - 4 : 76;
        int line = 5;
        for (int i = 0; i < option_count; i++) {
            int idx = options[i].idx;
            char key = options[i].key;
            const char *name = major_blessing_name_str(idx);
            const char *detail = major_blessing_detail_desc(idx);
            int cost = major_blessing_cost(idx);
            bool affordable = (cost <= available);
            int option_line = line;

            char buf[160];
            if (!menu_letters)
                snprintf(buf, sizeof buf, "   %s (cost %d)", name, cost);
            else
                snprintf(buf, sizeof buf, "%c) %s (cost %d)", key, name, cost);

            if (i == selected) {
                Term_putstr(2, line, -1, TERM_L_BLUE, ">");
                if (affordable) {
                    Term_putstr(4, line++, -1, TERM_L_GREEN, buf);
                } else {
                    Term_putstr(4, line++, -1, TERM_L_DARK, buf);
                }

                /* Show description only for selected item */
                if (detail && *detail) {
                    byte desc_color = affordable ? TERM_L_WHITE : TERM_SLATE;
                    Term_putstr(6, line++, -1, desc_color, detail);
                }
            } else {
                Term_putstr(2, line, -1, TERM_L_DARK, " ");
                if (affordable) {
                    Term_putstr(4, line++, -1, TERM_L_GREEN, buf);
                } else {
                    Term_putstr(4, line++, -1, TERM_L_DARK, buf);
                }
            }
            ui_menu_click_add(i, 2, option_line, click_width);

            line++;
        }

        /* Show available points */
        char points_msg[80];
        snprintf(points_msg, sizeof points_msg, "Available blessing points: %d", available);
        Term_putstr(2, line++, -1, TERM_L_BLUE, points_msg);

        blessing_put_choice_prompt(line + 1, steamdeck, menu_letters,
            accept_label, back_label, "cancel");

        char key = metarun_inkey_hidden();
        bool selected_from_confirm = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice < option_count)
                {
                    int cost = major_blessing_cost(options[clicked_choice].idx);
                    bool affordable = (cost <= available);

                    if (click_action == UI_MENU_CLICK_HOVER)
                    {
                        if (affordable && clicked_choice != selected)
                            selected = clicked_choice;
                        screen_load();
                        continue;
                    }

                    if (!affordable)
                    {
                        key = options[clicked_choice].key;
                        selected_from_confirm = true;
                    }
                    else if (clicked_choice != selected)
                    {
                        selected = clicked_choice;
                        screen_load();
                        continue;
                    }
                    else
                    {
                        key = '\r';
                        selected_from_confirm = true;
                    }
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                {
                    screen_load();
                    continue;
                }
                else if (clicked_choice == -1)
                    key = ESCAPE;
                else if (clicked_choice == -2)
                {
                    key = '\r';
                    selected_from_confirm = true;
                }
            }
        }

        screen_load();
        if (key == UI_MENU_CLICK_WAKE_KEY)
            continue;

        /* Handle back/cancel - ESC or B button in Steam Deck mode */
        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key()) || (!steamdeck && (key == 'h' || key == 'H'))) {
            ui_menu_click_clear();
            return false;
        }

        if (key == '\r' || key == '\n' || (steamdeck && key == steamdeck_confirm_key()) || key == '6') {
            key = options[selected].key;
            selected_from_confirm = true;
        } else if (key == '8' || key == 'k' || key == '-') {
            /* Navigate up, skipping unaffordable options */
            int start = selected;
            do {
                selected = (selected + option_count - 1) % option_count;
                int cost = major_blessing_cost(options[selected].idx);
                if (cost <= available) break;
            } while (selected != start);
            continue;
        } else if (key == '2' || key == 'j' || key == '+') {
            /* Navigate down, skipping unaffordable options */
            int start = selected;
            do {
                selected = (selected + 1) % option_count;
                int cost = major_blessing_cost(options[selected].idx);
                if (cost <= available) break;
            } while (selected != start);
            continue;
        }

        int choice_idx = -1;
        char lowered = tolower((unsigned char)key);
        if ((menu_letters || selected_from_confirm) && lowered >= 'a' && lowered <= 'z') {
            for (int i = 0; i < option_count; i++) {
                if (lowered == options[i].key) {
                    int cost = major_blessing_cost(options[i].idx);
                    if (cost > available) {
                        bell("Not enough blessing points for that covenant.");
                        choice_idx = -2; /* Special marker for unaffordable */
                        break;
                    }
                    choice_idx = options[i].idx;
                    selected = i;
                    break;
                }
            }
        }

        if (choice_idx == -2) {
            /* Was unaffordable, already showed bell */
            continue;
        }

        if (choice_idx < 0) {
            bell("Invalid selection.");
            continue;
        }

        /* At this point choice is valid and affordable */
        int cost = major_blessing_cost(choice_idx);

        metar.major_blessings |= (1U << choice_idx);
        blessing_spend_points(cost);
        blessing_commit_changes(true);

        if (result_msg && msg_size > 0) {
            const char *msg = major_blessing_unlock_msg(choice_idx);
            if (msg && *msg) {
                SDL_strlcpy(result_msg, msg, msg_size);
            } else {
                snprintf(result_msg, msg_size, "You seal the %s.", major_blessing_name_str(choice_idx));
            }
            if (result_attr) *result_attr = TERM_YELLOW;
        }
        ui_menu_click_clear();
        return true;
    }
}

void open_blessing_exchange(void)
{
    bool done = false;
    int selected = 0;  /* Track highlighted option: 0=remove curse, 1=minor blessing, 2=major blessing */
    char status_msg[256] = "";
    byte status_attr = TERM_WHITE;
    bool clear_status_on_next_key = false;
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();
    char accept_label[16] = "";
    char back_label[16] = "";

    if (steamdeck) {
        /* Steam Deck UI: A=accept, B=back */
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label, sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
    }

    while (!done) {
        compute_blessing_pool();
        int available = blessing_points_remaining();
        int earned = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
        int spent = metar.blessing_points_spent;

        bool major_available = false;
        int min_major_cost = INT_MAX;
        int major_cap = major_blessing_capacity();
        for (int i = 0; i < major_cap; i++) {
            if (metarun_has_major_blessing_index(i)) continue;
            if (!major_blessing_def(i)) continue;
            major_available = true;
            int cost = major_blessing_cost(i);
            if (cost < 0) cost = 0;
            if (cost < min_major_cost) min_major_cost = cost;
        }
        if (!major_available || min_major_cost == INT_MAX) {
            min_major_cost = 0;
        }

        /* Check if major blessing option is actually affordable */
        bool major_affordable = major_available && (min_major_cost <= available);

        int option_count = major_available ? 3 : 2;
        if (selected < 0) selected = 0;
        if (selected >= option_count) selected = option_count - 1;

        screen_save();
        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_outside_cancel_enabled(true);
        int click_width = (metarun_term_width() > 6) ? metarun_term_width() - 4 : 76;

        Term_putstr(2, 1, -1, TERM_YELLOW, "Blessing Exchange");
        char buf[160];
        snprintf(buf, sizeof buf, "Blessing Points Available: %d (spent %d / earned %d)",
                 available, spent, earned);
        Term_putstr(2, 3, -1, TERM_L_WHITE, buf);

        /* Get blessing point threshold from runtype data */
        u32b threshold = metarun_threshold_value(&metar);
        if (threshold == 0) threshold = 1;

        snprintf(buf, sizeof buf, "Fallen Score Pool: %lu (progress %lu / %lu)",
                 (unsigned long)metar.fallen_score_total,
                 (unsigned long)metar.fallen_score_pool,
                 (unsigned long)threshold);
        Term_putstr(2, 4, -1, TERM_L_WHITE, buf);

        Term_putstr(2, 6, -1, TERM_L_GREEN, "Options:");

        /* Option 0: Remove curse */
        cptr marker0 = (selected == 0) ? ">" : " ";
        byte attr0 = (selected == 0) ? TERM_L_WHITE : TERM_WHITE;
        Term_putstr(2, 8, -1, TERM_L_BLUE, marker0);
        Term_putstr(4, 8, -1, attr0,
            menu_letters ? "r) Remove a curse (cost 1)"
                         : "Remove a curse (cost 1)");
        ui_menu_click_add(0, 2, 8, click_width);

        /* Option 1: Minor blessing */
        cptr marker1 = (selected == 1) ? ">" : " ";
        byte attr1 = (selected == 1) ? TERM_L_WHITE : TERM_WHITE;
        Term_putstr(2, 9, -1, TERM_L_BLUE, marker1);
        Term_putstr(4, 9, -1, attr1,
            menu_letters ? "m) Gain a minor blessing (cost 1)"
                         : "Gain a minor blessing (cost 1)");
        ui_menu_click_add(1, 2, 9, click_width);

        /* Option 2: Major blessing */
        if (major_available) {
            cptr marker2 = (selected == 2) ? ">" : " ";
            byte attr2;
            if (major_affordable) {
                attr2 = (selected == 2) ? TERM_L_WHITE : TERM_WHITE;
            } else {
                attr2 = TERM_L_DARK; /* Grey out if unaffordable */
            }
            snprintf(buf, sizeof buf, menu_letters
                     ? "u) Unlock a major blessing (cost %d)"
                     : "Unlock a major blessing (cost %d)",
                     min_major_cost);
            Term_putstr(2, 10, -1, TERM_L_BLUE, marker2);
            Term_putstr(4, 10, -1, attr2, buf);
            ui_menu_click_add(2, 2, 10, click_width);
        } else {
            Term_putstr(4,10, -1, TERM_L_DARK,
                menu_letters ? "u) Unlock a major blessing (none available)"
                             : "Unlock a major blessing (none available)");
        }
        blessing_put_choice_prompt(12, steamdeck, menu_letters, accept_label,
            back_label, "leave");

        /* Display status message if present */
        if (status_msg[0] != '\0') {
            Term_putstr(2, 14, -1, status_attr, status_msg);
        }

        char key = metarun_inkey_hidden();
        bool selected_from_confirm = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice < option_count)
                {
                    bool selectable = (clicked_choice != 2) || major_affordable;

                    if (click_action == UI_MENU_CLICK_HOVER)
                    {
                        if (selectable && clicked_choice != selected)
                        {
                            selected = clicked_choice;
                            status_msg[0] = '\0';
                            clear_status_on_next_key = false;
                        }
                        screen_load();
                        continue;
                    }

                    if (selectable && clicked_choice != selected)
                    {
                        selected = clicked_choice;
                        status_msg[0] = '\0';
                        clear_status_on_next_key = false;
                        screen_load();
                        continue;
                    }

                    key = (clicked_choice == 0) ? 'r'
                        : (clicked_choice == 1) ? 'm' : 'u';
                    selected_from_confirm = true;
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                {
                    screen_load();
                    continue;
                }
                else if (clicked_choice == -1)
                    key = ESCAPE;
                else if (clicked_choice == -2)
                    key = '\r';
            }
        }

        screen_load();
        if (key == UI_MENU_CLICK_WAKE_KEY)
            continue;

        /* Clear status message on navigation or if flagged */
        if (clear_status_on_next_key || key == '8' || key == 'k' || key == '-' ||
            key == '2' || key == 'j' || key == '+') {
            status_msg[0] = '\0';
            clear_status_on_next_key = false;
        }

        /* Handle navigation */
        if (key == '8' || key == 'k' || key == '-') {
            /* Navigate up, skipping unaffordable major blessing */
            int start = selected;
            do {
                selected = (selected + option_count - 1) % option_count;
                if (selected == 2 && !major_affordable) continue; /* Skip unaffordable major */
                break;
            } while (selected != start);
            continue;
        } else if (key == '2' || key == 'j' || key == '+') {
            /* Navigate down, skipping unaffordable major blessing */
            int start = selected;
            do {
                selected = (selected + 1) % option_count;
                if (selected == 2 && !major_affordable) continue; /* Skip unaffordable major */
                break;
            } while (selected != start);
            continue;
        } else if (key == '\r' || key == '\n' || (steamdeck && key == steamdeck_confirm_key()) || key == '6') {
            /* A button/Space/Enter activates highlighted option */
            if (selected == 0) key = 'r';
            else if (selected == 1) key = 'm';
            else if (selected == 2) key = 'u';
            selected_from_confirm = true;
        }

        /* Handle back/cancel - ESC, B button in Steam Deck mode, or 'h' key */
        if (key == ESCAPE || key == '4' || (steamdeck && key == steamdeck_back_key()) || (!steamdeck && (key == 'h' || key == 'H'))) {
            done = true;
            continue;
        }

        if (!menu_letters && !selected_from_confirm) {
            bell("Use navigation and confirm to select in this mode.");
            continue;
        }

        switch (key) {
        case 'r':
        case 'R':
            if (available < 1) {
                SDL_strlcpy(status_msg, "You need at least one blessing point to lift a curse.", sizeof(status_msg));
                status_attr = TERM_ORANGE;
                clear_status_on_next_key = true;
            } else if (blessing_remove_curse(status_msg, sizeof(status_msg), &status_attr)) {
                compute_blessing_pool();
                clear_status_on_next_key = true;
            } else {
                clear_status_on_next_key = true;
            }
            break;
        case 'm':
        case 'M':
            if (available < 1) {
                SDL_strlcpy(status_msg, "You need at least one blessing point to receive a gift.", sizeof(status_msg));
                status_attr = TERM_ORANGE;
                clear_status_on_next_key = true;
            } else if (blessing_gain_minor(status_msg, sizeof(status_msg), &status_attr)) {
                compute_blessing_pool();
                clear_status_on_next_key = true;
            } else {
                clear_status_on_next_key = true;
            }
            break;
        case 'u':
        case 'U':
            if (!major_available) {
                SDL_strlcpy(status_msg, "All major blessings have already been sealed.", sizeof(status_msg));
                status_attr = TERM_L_DARK;
                clear_status_on_next_key = true;
            } else if (!major_affordable) {
                snprintf(status_msg, sizeof(status_msg), "You need %d blessing points to unlock a major blessing.", min_major_cost);
                status_attr = TERM_ORANGE;
                clear_status_on_next_key = true;
            } else if (blessing_unlock_major(status_msg, sizeof(status_msg), &status_attr)) {
                compute_blessing_pool();
                clear_status_on_next_key = true;
            } else {
                clear_status_on_next_key = true;
            }
            break;
        default:
            bell("Unrecognised option.");
            break;
        }
    }

    ui_menu_click_clear();
}
