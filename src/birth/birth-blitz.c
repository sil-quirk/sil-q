/* File: birth/birth-blitz.c */

#include "angband.h"
#include "birth/birth-internal.h"

static cptr blitz_character_mode_name(byte mode)
{
    switch (mode)
    {
    case BLITZ_CHARACTER_RANDOM_STATS: return "Random with stats";
    case BLITZ_CHARACTER_SELECTED: return "Selected";
    default: return "Random";
    }
}

static cptr blitz_effect_mode_name(byte mode)
{
    switch (mode)
    {
    case BLITZ_EFFECT_SELECTED: return "Selected";
    case BLITZ_EFFECT_SELECTED_DESCR: return "Selected + descriptions";
    default: return "Random";
    }
}

static void blitz_setup_clamp(blitz_setup* setup)
{
    if (!setup)
        return;

    if (setup->character_mode > BLITZ_CHARACTER_SELECTED)
        setup->character_mode = BLITZ_CHARACTER_RANDOM;
    if (setup->effect_mode > BLITZ_EFFECT_SELECTED_DESCR)
        setup->effect_mode = BLITZ_EFFECT_RANDOM;
    if (setup->blessing_count > BLITZ_MAX_EFFECT_COUNT)
        setup->blessing_count = BLITZ_MAX_EFFECT_COUNT;
    if (setup->curse_count > BLITZ_MAX_EFFECT_COUNT)
        setup->curse_count = BLITZ_MAX_EFFECT_COUNT;
    if (setup->curse_count < setup->blessing_count)
        setup->curse_count = setup->blessing_count;
}

static void blitz_pick_random_race_and_character(void)
{
    int race = 0;
    int available[64];
    int available_count = 0;

    if (!z_info)
        return;

    race = rand_int(z_info->p_max);
    p_ptr->prace = race;
    rp_ptr = &p_info[p_ptr->prace];

    for (int i = 0; i < z_info->c_max && available_count < (int)N_ELEMENTS(available); i++)
    {
        if (birth_character_is_set(i))
            available[available_count++] = i;
    }

    if (available_count <= 0)
        p_ptr->pcharacter = 0;
    else
        p_ptr->pcharacter = available[rand_int(available_count)];

    current_character_profile = &c_info[p_ptr->pcharacter];
}

static void blitz_setup_draw(const blitz_setup* setup, int selected)
{
    char buf[160];
    int wid = 80;
    int hgt = 24;
    bool steamdeck = steamdeck_controls_active();

    Term_get_size(&wid, &hgt);
    Term_clear();
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);

    c_put_str(TERM_YELLOW, "Blitz Setup", 1, MAX((wid - 11) / 2, 0));
    birth_put_wrapped_text(TERM_SLATE,
        "Configure a self-contained Blitz run. Story progress stays untouched.",
        3, 2);

    strnfmt(buf, sizeof(buf), "Character: %s", blitz_character_mode_name(setup->character_mode));
    birth_put_str_fit(selected == 0 ? TERM_L_BLUE : TERM_WHITE, buf, 6, 4);
    ui_menu_click_add(0, 4, 6, wid - 8);

    strnfmt(buf, sizeof(buf), "Oaths: %s", setup->oaths_enabled ? "Yes" : "No");
    birth_put_str_fit(selected == 1 ? TERM_L_BLUE : TERM_WHITE, buf, 7, 4);
    ui_menu_click_add(1, 4, 7, wid - 8);

    strnfmt(buf, sizeof(buf), "Blessings: %d", setup->blessing_count);
    birth_put_str_fit(selected == 2 ? TERM_L_BLUE : TERM_WHITE, buf, 8, 4);
    ui_menu_click_add(2, 4, 8, wid - 8);

    strnfmt(buf, sizeof(buf), "Curses: %d", setup->curse_count);
    birth_put_str_fit(selected == 3 ? TERM_L_BLUE : TERM_WHITE, buf, 9, 4);
    ui_menu_click_add(3, 4, 9, wid - 8);

    strnfmt(buf, sizeof(buf), "Effect picks: %s", blitz_effect_mode_name(setup->effect_mode));
    birth_put_str_fit(selected == 4 ? TERM_L_BLUE : TERM_WHITE, buf, 10, 4);
    ui_menu_click_add(4, 4, 10, wid - 8);

    if (steamdeck)
    {
        char confirm_label[16];
        char back_label[16];
        char prompt_buf[96];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        {
            char prompt_full[96];
            char prompt_short[80];
            const char* variants[2];

            strnfmt(prompt_full, sizeof(prompt_full),
                "D-pad navigate/change  %s begin  %s back",
                confirm_label, back_label);
            strnfmt(prompt_short, sizeof(prompt_short),
                "%s begin  %s back", confirm_label, back_label);
            variants[0] = prompt_full;
            variants[1] = prompt_short;
            terminal_prompt_pick_variant(prompt_buf, sizeof(prompt_buf),
                wid - 4, false, variants, N_ELEMENTS(variants));
        }
        birth_put_str_fit(TERM_L_DARK, prompt_buf, 13, 2);
        ui_menu_click_add_text_token(-2, 2, 13, prompt_buf, "begin");
        ui_menu_click_add_text_token(-1, 2, 13, prompt_buf, "back");
    }
    else
    {
        char prompt_text[96];
        const char* variants[] = {
            "Dir navigate/change  Enter begin  Esc back",
            "Dir change  Enter begin  Esc back",
            "Enter begin  Esc back"
        };
        terminal_prompt_pick_variant(prompt_text, sizeof(prompt_text),
            wid - 4, false, variants, N_ELEMENTS(variants));
        birth_put_str_fit(TERM_L_DARK, prompt_text, 13, 2);
        ui_menu_click_add_text_token(-2, 2, 13, prompt_text, "begin");
        ui_menu_click_add_text_token(-1, 2, 13, prompt_text, "back");
    }
}

static NavResult blitz_setup_menu(void)
{
    blitz_setup* setup = blitz_current_setup_mutable();
    int selected = 0;
    bool steamdeck = steamdeck_controls_active();
    NavResult result = NAV_TO_MAIN;

    blitz_setup_clamp(setup);

    /* Render at the inventory/supply scale: hide panes BEFORE pushing the menu
     * scale so it is calculated from the full screen. */
    screen_save();
    screen_push_supporting_panes_hidden();
    screen_push_touch_pane_hidden();
    sdl_push_terminal_menu_scale();

    while (1)
    {
        char key;

        blitz_setup_draw(setup, selected);
        key = inkey();
        bool click_generated_command = false;
        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice < 5)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != selected)
                    {
                        selected = clicked_choice;
                        continue;
                    }
                    key = '6';
                    click_generated_command = true;
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                else if (clicked_choice == -1) {
                    key = ESCAPE;
                    click_generated_command = true;
                } else if (clicked_choice == -2) {
                    key = '\r';
                    click_generated_command = true;
                }
            }
        }

        if (!click_generated_command)
            key = (char)steamdeck_menu_key(key, 0, 0);

        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key()))
        {
            ui_menu_click_clear();
            result = NAV_TO_MAIN;
            break;
        }

        if (key == '\n' || key == '\r' || key == ' '
            || (steamdeck && key == steamdeck_confirm_key()))
        {
            ui_menu_click_clear();
            result = NAV_OK;
            break;
        }

        if (key == '8')
        {
            selected = (selected + 4) % 5;
            continue;
        }

        if (key == '2')
        {
            selected = (selected + 1) % 5;
            continue;
        }

        if (key != '4' && key != '6')
            continue;

        switch (selected)
        {
        case 0:
            if (key == '4')
                setup->character_mode = (setup->character_mode == BLITZ_CHARACTER_RANDOM)
                    ? BLITZ_CHARACTER_SELECTED
                    : setup->character_mode - 1;
            else
                setup->character_mode = (setup->character_mode == BLITZ_CHARACTER_SELECTED)
                    ? BLITZ_CHARACTER_RANDOM
                    : setup->character_mode + 1;
            break;
        case 1:
            setup->oaths_enabled = !setup->oaths_enabled;
            break;
        case 2:
            if (key == '4' && setup->blessing_count > 0)
                setup->blessing_count--;
            else if (key == '6' && setup->blessing_count < BLITZ_MAX_EFFECT_COUNT)
                setup->blessing_count++;
            break;
        case 3:
            if (key == '4' && setup->curse_count > 0)
                setup->curse_count--;
            else if (key == '6' && setup->curse_count < BLITZ_MAX_EFFECT_COUNT)
                setup->curse_count++;
            break;
        case 4:
            if (key == '4')
                setup->effect_mode = (setup->effect_mode == BLITZ_EFFECT_RANDOM)
                    ? BLITZ_EFFECT_SELECTED_DESCR
                    : setup->effect_mode - 1;
            else
                setup->effect_mode = (setup->effect_mode == BLITZ_EFFECT_SELECTED_DESCR)
                    ? BLITZ_EFFECT_RANDOM
                    : setup->effect_mode + 1;
            break;
        default:
            break;
        }

        blitz_setup_clamp(setup);
    }

    sdl_pop_terminal_menu_scale();
    screen_pop_touch_pane_hidden();
    screen_pop_supporting_panes_hidden();
    screen_load();
    return result;
}

NavResult blitz_character_creation(void)
{
    blitz_runtime_reset();

    if (blitz_setup_menu() != NAV_OK)
        return NAV_TO_MAIN;

    if (blitz_current_setup()->character_mode == BLITZ_CHARACTER_SELECTED)
        return character_creation();

    blitz_pick_random_race_and_character();
    finalize_character_creation_selection();
    return NAV_OK;
}

static cptr blitz_curse_name_str(int id)
{
    cptr raw = cu_name + cu_info[id].name;
    if (strncmp(raw, "Curse of ", 9) == 0)
        raw += 9;
    return raw;
}

static cptr blitz_blessing_name_str(int id)
{
    if (cu_info[id].blessing_name)
    {
        cptr raw = cu_name + cu_info[id].blessing_name;
        if (strncmp(raw, "Blessing of ", 12) == 0)
            raw += 12;
        return raw;
    }

    return blitz_curse_name_str(id);
}

static int blitz_collect_eligible_effect_ids(bool blessing, int ids[], int max_ids)
{
    int count = 0;

    for (int id = 0; z_info && id < z_info->cu_max && count < max_ids; id++)
    {
        int stacks = CURSE_GET(id);
        int blessing_stacks = (stacks < 0) ? -stacks : 0;
        int curse_stacks = (stacks > 0) ? stacks : 0;
        byte curse_cap = (byte)CURSE_CURSE_CAP(id);
        byte blessing_cap = (byte)CURSE_BLESSING_CAP(id);

        if (blessing)
        {
            if (!cu_info[id].blessing_name)
                continue;
            if (stacks > 0)
                continue;
            if (blessing_cap > 0 && blessing_stacks >= blessing_cap)
                continue;
        }
        else
        {
            if (!cu_info[id].name)
                continue;
            if (curse_cap > 0 && curse_stacks >= curse_cap)
                continue;
        }

        ids[count++] = id;
    }

    return count;
}

static int blitz_weighted_random_curse_pick(void)
{
    long total = 0;
    int w_max = 1;
    bool tilt = (p_info[p_ptr->prace].flags & RHF_CURSE)
        || (c_info[p_ptr->pcharacter].flags & RHF_CURSE);

    for (int i = 0; z_info && i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name)
            continue;
        if (cu_info[i].weight > w_max)
            w_max = cu_info[i].weight;
    }

    for (int i = 0; z_info && i < z_info->cu_max; i++)
    {
        byte w = cu_info[i].weight ? cu_info[i].weight : 1;
        int cnt = CURSE_CURSE_STACK(i);
        byte cap = (byte)CURSE_CURSE_CAP(i);
        long base;

        if (!cu_info[i].name)
            continue;
        if (cap && cnt >= cap)
            continue;
        if (tilt && w == w_max)
            continue;

        base = tilt ? w + ((w_max + 1 - w) >> 1) : w;
        total += base / (cnt + 1);
    }

    if (!total)
        return -1;

    long pick = rand_int(total);
    long run = 0;
    for (int i = 0; z_info && i < z_info->cu_max; i++)
    {
        byte w = cu_info[i].weight ? cu_info[i].weight : 1;
        int cnt = CURSE_CURSE_STACK(i);
        byte cap = (byte)CURSE_CURSE_CAP(i);
        long base;
        long eff;

        if (!cu_info[i].name)
            continue;
        if (cap && cnt >= cap)
            continue;
        if (tilt && w == w_max)
            continue;

        base = tilt ? w + ((w_max + 1 - w) >> 1) : w;
        eff = base / (cnt + 1);
        run += eff;
        if (pick < run)
            return i;
    }

    return -1;
}

static int blitz_weighted_random_blessing_pick(void)
{
    int eligible[METAR_CURSE_SLOTS];
    int weights[METAR_CURSE_SLOTS];
    int count = 0;
    int total_weight = 0;

    for (int id = 0; z_info && id < z_info->cu_max && count < METAR_CURSE_SLOTS; id++)
    {
        int stacks = CURSE_GET(id);
        int blessing_stacks = (stacks < 0) ? -stacks : 0;
        int base_weight;
        int effective_weight;

        if (!cu_info[id].blessing_name)
            continue;
        if (stacks > 0)
            continue;
        if (CURSE_BLESSING_CAP(id) > 0
            && blessing_stacks >= CURSE_BLESSING_CAP(id))
            continue;

        eligible[count] = id;
        base_weight = cu_info[id].weight > 0 ? cu_info[id].weight : 1;
        effective_weight = base_weight / (blessing_stacks + 1);
        weights[count] = (effective_weight > 0) ? effective_weight : 1;
        total_weight += weights[count];
        count++;
    }

    if (count <= 0 || total_weight <= 0)
        return -1;

    int roll = rand_int(total_weight);
    int sum = 0;
    for (int i = 0; i < count; i++)
    {
        sum += weights[i];
        if (roll < sum)
            return eligible[i];
    }

    return eligible[0];
}

static void blitz_add_effect_detail_line(cptr text, byte attr)
{
    if (!text || !text[0])
        return;
    sdl_character_sheet_screen_add_select_detail(text, attr, "");
}

static void blitz_add_effect_details(int id, bool blessing,
    bool show_effects)
{
    curse_type* cu;
    cptr name;
    cptr desc;
    cptr power;
    bool reveal_power;

    if (!z_info || id < 0 || id >= z_info->cu_max)
        return;

    cu = &cu_info[id];
    name = blessing ? blitz_blessing_name_str(id) : blitz_curse_name_str(id);
    desc = blessing
        ? (cu->blessing_text ? cu_text + cu->blessing_text : "")
        : (cu->text ? cu_text + cu->text : "");
    power = blessing
        ? (cu->blessing_power ? cu_text + cu->blessing_power : "")
        : (cu->power ? cu_text + cu->power : "");
    /* Birth Blitz selection is a deliberate pick, so "+ descriptions" reveals
     * the effect outright -- the poetical description always shows, and the
     * mechanical effect is added on top when descriptions are enabled (no
     * CURSE_SEEN gate, which would otherwise hide every effect at birth). */
    reveal_power = show_effects && power && power[0];

    blitz_add_effect_detail_line(name, TERM_WHITE);
    blitz_add_effect_detail_line(desc, TERM_SLATE);
    if (reveal_power)
    {
        char power_line[1024];

        strnfmt(power_line, sizeof(power_line), "Effect: %s", power);
        blitz_add_effect_detail_line(power_line,
            blessing ? TERM_L_GREEN : TERM_L_RED);
    }
}

static int blitz_select_effect_from_list(bool blessing, bool show_effects, int ordinal, int total)
{
    int ids[METAR_CURSE_SLOTS];
    int count = blitz_collect_eligible_effect_ids(blessing, ids, METAR_CURSE_SLOTS);
    int selected = 0;
    bool steamdeck = steamdeck_controls_active();

    if (count <= 0)
        return -1;

    while (1)
    {
        int selected_id;
        char key;
        char title[80];

        selected_id = ids[selected];
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);

        strnfmt(title, sizeof(title), "Choose %s %d of %d",
            blessing ? "Blessing" : "Curse", ordinal, total);
        sdl_character_sheet_screen_begin_select(selected, title);
        sdl_character_sheet_screen_set_select_menu_style(true);
        sdl_character_sheet_screen_set_select_dynamic_description(true);
        for (int idx = 0; idx < count; idx++)
        {
            /* No letter prefix: there are more curses than the 26 letters
             * cover, so letter shortcuts are inconsistent -- the list is
             * navigated by direction keys / pointer instead. */
            cptr name = blessing ? blitz_blessing_name_str(ids[idx])
                                 : blitz_curse_name_str(ids[idx]);

            sdl_character_sheet_screen_add_select_row(idx, name,
                blessing ? TERM_L_GREEN : TERM_L_RED, "");
        }
        blitz_add_effect_details(selected_id, blessing, show_effects);
        sdl_character_sheet_screen_commit_select(selected);
        key = inkey();
        bool click_generated_command = false;

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
                        continue;
                    }
                    key = '\r';
                    click_generated_command = true;
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                else if (clicked_choice == -1) {
                    key = ESCAPE;
                    click_generated_command = true;
                } else if (clicked_choice == -2) {
                    key = '\r';
                    click_generated_command = true;
                }
            }
        }

        if (!click_generated_command)
            key = (char)steamdeck_menu_key(key, 0, 0);

        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key()))
        {
            sdl_character_sheet_screen_hide();
            ui_menu_click_clear();
            return -1;
        }
        if (key == '\n' || key == '\r' || key == ' '
            || (steamdeck && key == steamdeck_confirm_key()))
        {
            sdl_character_sheet_screen_hide();
            ui_menu_click_clear();
            return selected_id;
        }
        if (key == '8')
        {
            selected = (selected + count - 1) % count;
            continue;
        }
        if (key == '2')
        {
            selected = (selected + 1) % count;
            continue;
        }
        if (key == '4' || key == '6')
        {
            int rows_per_column =
                sdl_character_sheet_screen_select_menu_rows_per_column();
            int next = selected
                + ((key == '6') ? rows_per_column : -rows_per_column);

            if (rows_per_column > 0 && next >= 0 && next < count)
                selected = next;
            continue;
        }
    }
}

static void blitz_apply_effect_pick(int id, bool blessing)
{
    CURSE_ADD(id, blessing ? -1 : 1);
    CURSE_SEEN_SET(id);
}

static void blitz_show_effect_summary(void)
{
    int wid = 80;
    int hgt = 24;
    int row = 3;

    Term_get_size(&wid, &hgt);
    Term_clear();
    c_put_str(TERM_YELLOW, "Blitz Effects", 1, MAX((wid - 13) / 2, 0));

    for (int id = 0; z_info && id < z_info->cu_max; id++)
    {
        int stacks = CURSE_GET(id);
        char line[128];

        if (stacks == 0)
            continue;

        strnfmt(line, sizeof(line), "%s x%d",
            (stacks < 0) ? blitz_blessing_name_str(id) : blitz_curse_name_str(id),
            (stacks < 0) ? -stacks : stacks);
        birth_put_str_fit(stacks < 0 ? TERM_L_GREEN : TERM_L_RED, line, row++, 4);
    }

    if (row == 3)
        birth_put_str_fit(TERM_SLATE, "No blessings or curses selected.", row++, 4);

    if (steamdeck_controls_active())
    {
        char confirm_label[16];
        char prompt_buf[48];

        birth_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        strnfmt(prompt_buf, sizeof(prompt_buf), "[%s] continue", confirm_label);
        birth_put_str_fit(TERM_L_BLUE, prompt_buf, MIN(row + 1, hgt - 1), 2);
    }
    else
    {
        birth_put_str_fit(TERM_L_BLUE, "Press any key to continue.",
            MIN(row + 1, hgt - 1), 2);
    }
    (void)inkey();
}

NavResult blitz_configure_effects(void)
{
    const blitz_setup* setup = blitz_current_setup();
    NavResult result = NAV_OK;

    blitz_runtime_reset();

    /* Match the inventory/supply scale for the effect-selection and summary
     * screens: hide panes first, then push the configured menu scale. */
    screen_save();
    screen_push_supporting_panes_hidden();
    screen_push_touch_pane_hidden();
    sdl_push_terminal_menu_scale();

    for (int i = 0; i < setup->curse_count; i++)
    {
        int id = (setup->effect_mode == BLITZ_EFFECT_RANDOM)
            ? blitz_weighted_random_curse_pick()
            : blitz_select_effect_from_list(false,
                setup->effect_mode == BLITZ_EFFECT_SELECTED_DESCR, i + 1, setup->curse_count);
        if (id < 0)
        {
            result = NAV_BACK;
            goto done;
        }
        blitz_apply_effect_pick(id, false);
    }

    for (int i = 0; i < setup->blessing_count; i++)
    {
        int id = (setup->effect_mode == BLITZ_EFFECT_RANDOM)
            ? blitz_weighted_random_blessing_pick()
            : blitz_select_effect_from_list(true,
                setup->effect_mode == BLITZ_EFFECT_SELECTED_DESCR, i + 1, setup->blessing_count);
        if (id < 0)
        {
            result = NAV_BACK;
            goto done;
        }
        blitz_apply_effect_pick(id, true);
    }

    if (setup->curse_count > 0 || setup->blessing_count > 0)
        blitz_show_effect_summary();

done:
    sdl_pop_terminal_menu_scale();
    screen_pop_touch_pane_hidden();
    screen_pop_supporting_panes_hidden();
    screen_load();
    return result;
}

static void blitz_auto_assign_stats(int stats[A_MAX])
{
    int cost = 0;

    for (int i = 0; i < A_MAX; i++)
        stats[i] = 0;

    while (cost < MAX_COST)
    {
        int choices[A_MAX];
        int choice_count = 0;

        for (int i = 0; i < A_MAX; i++)
        {
            int next = stats[i] + 1;
            int next_cost;

            if (next > 6)
                continue;
            next_cost = cost - birth_stat_costs[stats[i] + 4]
                + birth_stat_costs[next + 4];
            if (next_cost <= MAX_COST)
                choices[choice_count++] = i;
        }

        if (choice_count <= 0)
            break;

        int pick = choices[rand_int(choice_count)];
        cost -= birth_stat_costs[stats[pick] + 4];
        stats[pick]++;
        cost += birth_stat_costs[stats[pick] + 4];
    }
}

static void blitz_auto_assign_skills(void)
{
    int old_base[S_MAX];
    int gains[S_MAX];
    int budget;

    for (int i = 0; i < S_MAX; i++)
    {
        old_base[i] = p_ptr->skill_base[i];
        gains[i] = 0;
    }

    budget = p_ptr->new_exp;

    while (budget > 0)
    {
        int choices[S_MAX];
        int weights[S_MAX];
        int choice_count = 0;
        int total_weight = 0;

        for (int i = 0; i < S_MAX; i++)
        {
            int delta;
            int weight = 2;

            if (i == S_SPC)
                continue;

            delta = birth_skill_cost(old_base[i], gains[i] + 1)
                - birth_skill_cost(old_base[i], gains[i]);
            if (delta <= 0 || delta > budget)
                continue;

            for (int slot = 0; slot < CHARACTER_ABILITY_MAX; slot++)
            {
                int skill_idx = c_info[p_ptr->pcharacter].a_adj[slot][0];
                if (skill_idx < 0)
                    break;
                if (skill_idx == i)
                    weight += 3;
            }

            choices[choice_count] = i;
            weights[choice_count] = weight;
            total_weight += weight;
            choice_count++;
        }

        if (choice_count <= 0)
            break;

        int roll = rand_int(total_weight);
        int sum = 0;
        int chosen = choices[0];
        for (int i = 0; i < choice_count; i++)
        {
            sum += weights[i];
            if (roll < sum)
            {
                chosen = choices[i];
                break;
            }
        }

        budget -= birth_skill_cost(old_base[chosen], gains[chosen] + 1)
            - birth_skill_cost(old_base[chosen], gains[chosen]);
        gains[chosen]++;
    }

    for (int i = 0; i < S_MAX; i++)
    {
        if (i == S_SPC)
            continue;
        p_ptr->skill_base[i] = old_base[i] + gains[i];
    }

    p_ptr->new_exp = budget;
}

NavResult blitz_auto_build_character(void)
{
    int stats[A_MAX];

    get_extra();
    blitz_auto_assign_stats(stats);

    for (int i = 0; i < A_MAX; i++)
    {
        int bonus = rp_ptr->r_adj[i] + current_character_profile->h_adj[i] + curses_stat_adj(i);
        p_ptr->stat_base[i] = stats[i] + bonus;
        p_ptr->stat_drain[i] = 0;
    }

    p_ptr->update |= (PU_BONUS | PU_HP);
    update_stuff();
    p_ptr->chp = p_ptr->mhp;
    calc_voice();
    p_ptr->csp = p_ptr->msp;

    blitz_auto_assign_skills();
    p_ptr->update |= (PU_BONUS);
    update_stuff();
    p_ptr->chp = p_ptr->mhp;
    calc_voice();
    p_ptr->csp = p_ptr->msp;

    return NAV_OK;
}

