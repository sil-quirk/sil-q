#include "angband.h"
#include "metarun-internal.h"

static void build_symbol_bar(char *out, size_t out_len, int current, int maximum, char filled)
{
    if (!out || out_len == 0) return;
    if (maximum <= 0) {
        strnfmt(out, out_len, "[]");
        return;
    }

    const int MAX_BAR_SLOTS = 20;
    int slots = maximum;
    if (slots > MAX_BAR_SLOTS) slots = MAX_BAR_SLOTS;
    if (slots < 1) slots = 1;

    char buffer[MAX_BAR_SLOTS + 1];
    for (int i = 0; i < slots; i++) {
        buffer[i] = (i < current) ? filled : '.';
    }
    buffer[slots] = '\0';

    strnfmt(out, out_len, "[%s]", buffer);
}

static void build_death_marks(char *out, size_t out_len, int deaths)
{
    if (!out || out_len == 0) return;
    if (deaths <= 0) {
        strnfmt(out, out_len, "none");
        return;
    }

    int max_marks = (int)out_len - 1;
    if (max_marks <= 0) {
        if (out_len > 0) out[0] = '\0';
        return;
    }

    if (deaths <= max_marks) {
        for (int i = 0; i < deaths; i++) out[i] = 'x';
        out[deaths] = '\0';
    } else {
        int marks = max_marks - 1;
        if (marks < 0) marks = 0;
        for (int i = 0; i < marks; i++) out[i] = 'x';
        out[marks] = '+';
        out[marks + 1] = '\0';
    }
}

static void draw_blessing_meter(int col, int start_row, int height, u32b current, u32b threshold)
{
    if (height < 5 || threshold == 0) return;

    /* Calculate fill percentage */
    int percent = (int)((current * 100) / threshold);
    if (percent > 100) percent = 100;

    /* Draw title */
    Term_putstr(col, start_row, -1, TERM_L_BLUE, "Blessing Pool");

    /* Draw top border */
    Term_putstr(col, start_row + 1, -1, TERM_L_BLUE, "+----------+");

    /* Draw the meter from bottom to top using simple ASCII */
    int meter_start = start_row + 2;
    int meter_end = start_row + height - 1;
    int meter_height = meter_end - meter_start;
    int filled_height = (meter_height * percent) / 100;

    for (int row = meter_start; row < meter_end; row++) {
        int rows_from_bottom = meter_end - row - 1;
        if (rows_from_bottom < filled_height) {
            /* Filled portion - use # for filled */
            Term_putstr(col, row, -1, TERM_L_BLUE, "|##########|");
        } else {
            /* Empty portion */
            Term_putstr(col, row, -1, TERM_L_BLUE, "|          |");
        }
    }

    /* Draw bottom border */
    Term_putstr(col, meter_end, -1, TERM_L_BLUE, "+----------+");

    /* Draw progress text below the meter */
    char progress_buf[20];
    snprintf(progress_buf, sizeof progress_buf, "%lu/%lu",
             (unsigned long)current, (unsigned long)threshold);
    int text_col = col + (12 - (int)strlen(progress_buf)) / 2;
    if (text_col < col) text_col = col;
    Term_putstr(text_col, meter_end + 1, -1, TERM_L_BLUE, progress_buf);
}

static void metarun_truncate_for_width(char *buf, int max_width)
{
    if (!buf) return;
    if (max_width <= 0) {
        buf[0] = '\0';
        return;
    }

    int len = (int)strlen(buf);
    if (len <= max_width) return;

    if (max_width >= 4) {
        buf[max_width - 3] = '.';
        buf[max_width - 2] = '.';
        buf[max_width - 1] = '.';
        buf[max_width] = '\0';
    } else {
        buf[max_width] = '\0';
    }
}

static void metarun_put_prompt_line(int term_width, int term_height, byte attr, const char *text)
{
    if (term_width <= 0 || term_height <= 0) return;

    char line[512];
    int line_width = term_width;
    if (line_width > (int)sizeof(line) - 1) line_width = (int)sizeof(line) - 1;

    memset(line, ' ', line_width);
    line[line_width] = '\0';

    if (text && *text) {
        size_t tlen = strlen(text);
        if ((int)tlen > line_width) tlen = (size_t)line_width;
        memcpy(line, text, tlen);
    }

    Term_putstr(0, term_height - 1, -1, attr, line);
}

static void metarun_register_prompt_label_click(int choice, int row,
    const char *prompt, const char *label)
{
    char token[48];

    if (!label || !label[0])
        return;

    strnfmt(token, sizeof(token), "[%s]", label);
    ui_menu_click_add_text_token(choice, 0, row, prompt, token);
}

static void metarun_register_prompt_action_click(int choice, int row,
    const char *prompt, const char *label, const char *fallback_label,
    const char *action_text)
{
    char token[96];
    const char *key_label = (label && label[0]) ? label : fallback_label;

    if (!prompt || !key_label || !key_label[0]
        || !action_text || !action_text[0])
    {
        return;
    }

    strnfmt(token, sizeof(token), "[%s] %s", key_label, action_text);
    ui_menu_click_add_text_token(choice, 0, row, prompt, token);
    ui_menu_click_add_text_token(choice, 0, row, prompt, action_text);
}

static void metarun_register_stats_prompt_clicks(const char *prompt, int row,
    const char *spend_label, const char *threshold_label,
    const char *diff_label, const char *full_label,
    const char *history_label, const char *blitz_label, bool blitz_enabled)
{
    if (!prompt || row < 0)
        return;

    metarun_register_prompt_label_click('b', row, prompt, spend_label);
    metarun_register_prompt_label_click('f', row, prompt, threshold_label);
    metarun_register_prompt_label_click('c', row, prompt, diff_label);
    metarun_register_prompt_label_click('u', row, prompt, full_label);
    metarun_register_prompt_label_click('s', row, prompt, history_label);
    if (blitz_enabled)
        metarun_register_prompt_label_click('x', row, prompt, blitz_label);

    metarun_register_prompt_action_click('b', row, prompt, spend_label, "b",
        "Spend blessings");
    metarun_register_prompt_action_click('u', row, prompt, full_label, "u",
        "Full list");

    ui_menu_click_add_text_token('b', 0, row, prompt, "[b]");
    ui_menu_click_add_text_token('b', 0, row, prompt, "Spend");
    ui_menu_click_add_text_token('b', 0, row, prompt, "Bless");
    ui_menu_click_add_text_token('f', 0, row, prompt, "[f]");
    ui_menu_click_add_text_token('f', 0, row, prompt, "Threshold");
    ui_menu_click_add_text_token('f', 0, row, prompt, "Thresh");
    ui_menu_click_add_text_token('f', 0, row, prompt, "Thr");
    ui_menu_click_add_text_token('c', 0, row, prompt, "[c]");
    ui_menu_click_add_text_token('c', 0, row, prompt, "Difficulty");
    ui_menu_click_add_text_token('c', 0, row, prompt, "Diff");
    ui_menu_click_add_text_token('u', 0, row, prompt, "[u]");
    ui_menu_click_add_text_token('u', 0, row, prompt, "Full");
    ui_menu_click_add_text_token('u', 0, row, prompt, "List");
    ui_menu_click_add_text_token('s', 0, row, prompt, "[s]");
    ui_menu_click_add_text_token('s', 0, row, prompt, "History");
    ui_menu_click_add_text_token('s', 0, row, prompt, "Hist");

    if (blitz_enabled) {
        ui_menu_click_add_text_token('x', 0, row, prompt, "[x]");
        ui_menu_click_add_text_token('x', 0, row, prompt, "Blitz");
        ui_menu_click_add_text_token('x', 0, row, prompt, "Bz");
    }
}

static void metarun_register_continue_clicks(int term_height)
{
    if (term_height < 1)
        return;

    for (int row = 0; row < term_height; row++)
        ui_menu_click_add_full_row('\r', row);
}

typedef struct {
    char variants[4][64];
    int variant_count;
    int variant_idx;
    bool enabled;
    int drop_priority;
} metarun_prompt_action;

static size_t metarun_render_action_prompt(const metarun_prompt_action *actions,
                                           int action_count,
                                           char *out,
                                           size_t out_size)
{
    if (!out || out_size == 0) return 0;

    out[0] = '\0';
    bool first = true;

    for (int i = 0; i < action_count; i++) {
        if (!actions[i].enabled) continue;
        if (actions[i].variant_idx < 0 || actions[i].variant_idx >= actions[i].variant_count) continue;

        if (!first) SDL_strlcat(out, "  ", out_size);
        SDL_strlcat(out, actions[i].variants[actions[i].variant_idx], out_size);
        first = false;
    }

    return strlen(out);
}

static void metarun_build_action_prompt(int term_width,
                                        bool steamdeck,
                                        const char *spend_label,
                                        const char *threshold_label,
                                        const char *diff_label,
                                        const char *full_label,
                                        const char *history_label,
                                        const char *blitz_label,
                                        bool blitz_enabled,
                                        char *out,
                                        size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';

    const char *spend = (spend_label && *spend_label) ? spend_label : "X";
    const char *thr = (threshold_label && *threshold_label) ? threshold_label : "R1";
    const char *diff = (diff_label && *diff_label) ? diff_label : "L1";
    const char *full = (full_label && *full_label) ? full_label : "Start";
    const char *hist = (history_label && *history_label) ? history_label : "Y";
    const char *blitz = (blitz_label && *blitz_label) ? blitz_label : "Back";

    metarun_prompt_action actions[6];
    memset(actions, 0, sizeof(actions));

    for (int i = 0; i < 6; i++) {
        actions[i].variant_count = 4;
        actions[i].variant_idx = 0;
        actions[i].enabled = (i < 5) ? true : blitz_enabled;
    }

    /* Lower number means dropped earlier if space is too tight. */
    actions[0].drop_priority = 5; /* Blessings */
    actions[1].drop_priority = 4; /* Threshold */
    actions[2].drop_priority = 3; /* Difficulty */
    actions[3].drop_priority = 2; /* Full list */
    actions[4].drop_priority = 1; /* History */
    actions[5].drop_priority = 4; /* Blitz */

    if (steamdeck) {
        strnfmt(actions[0].variants[0], sizeof(actions[0].variants[0]), "[%s] Spend blessings", spend);
        strnfmt(actions[0].variants[1], sizeof(actions[0].variants[1]), "[%s] Blessings", spend);
        strnfmt(actions[0].variants[2], sizeof(actions[0].variants[2]), "[%s] Bless", spend);
        strnfmt(actions[0].variants[3], sizeof(actions[0].variants[3]), "[%s]", spend);

        strnfmt(actions[1].variants[0], sizeof(actions[1].variants[0]), "[%s] Threshold", thr);
        strnfmt(actions[1].variants[1], sizeof(actions[1].variants[1]), "[%s] Thresh", thr);
        strnfmt(actions[1].variants[2], sizeof(actions[1].variants[2]), "[%s] Thr", thr);
        strnfmt(actions[1].variants[3], sizeof(actions[1].variants[3]), "[%s]", thr);

        strnfmt(actions[2].variants[0], sizeof(actions[2].variants[0]), "[%s] Difficulty", diff);
        strnfmt(actions[2].variants[1], sizeof(actions[2].variants[1]), "[%s] Diff", diff);
        strnfmt(actions[2].variants[2], sizeof(actions[2].variants[2]), "[%s] D", diff);
        strnfmt(actions[2].variants[3], sizeof(actions[2].variants[3]), "[%s]", diff);

        strnfmt(actions[3].variants[0], sizeof(actions[3].variants[0]), "[%s] Full list", full);
        strnfmt(actions[3].variants[1], sizeof(actions[3].variants[1]), "[%s] List", full);
        strnfmt(actions[3].variants[2], sizeof(actions[3].variants[2]), "[%s] L", full);
        strnfmt(actions[3].variants[3], sizeof(actions[3].variants[3]), "[%s]", full);

        strnfmt(actions[4].variants[0], sizeof(actions[4].variants[0]), "[%s] History", hist);
        strnfmt(actions[4].variants[1], sizeof(actions[4].variants[1]), "[%s] Hist", hist);
        strnfmt(actions[4].variants[2], sizeof(actions[4].variants[2]), "[%s] H", hist);
        strnfmt(actions[4].variants[3], sizeof(actions[4].variants[3]), "[%s]", hist);

        strnfmt(actions[5].variants[0], sizeof(actions[5].variants[0]), "[%s] Blitz", blitz);
        strnfmt(actions[5].variants[1], sizeof(actions[5].variants[1]), "[%s] Blitz", blitz);
        strnfmt(actions[5].variants[2], sizeof(actions[5].variants[2]), "[%s] Bz", blitz);
        strnfmt(actions[5].variants[3], sizeof(actions[5].variants[3]), "[%s]", blitz);

    } else {
        SDL_strlcpy(actions[0].variants[0], "[b] Spend blessings", sizeof(actions[0].variants[0]));
        SDL_strlcpy(actions[0].variants[1], "[b] Blessings", sizeof(actions[0].variants[1]));
        SDL_strlcpy(actions[0].variants[2], "[b] Bless", sizeof(actions[0].variants[2]));
        SDL_strlcpy(actions[0].variants[3], "[b]", sizeof(actions[0].variants[3]));

        SDL_strlcpy(actions[1].variants[0], "[f] Threshold", sizeof(actions[1].variants[0]));
        SDL_strlcpy(actions[1].variants[1], "[f] Thresh", sizeof(actions[1].variants[1]));
        SDL_strlcpy(actions[1].variants[2], "[f] Thr", sizeof(actions[1].variants[2]));
        SDL_strlcpy(actions[1].variants[3], "[f]", sizeof(actions[1].variants[3]));

        SDL_strlcpy(actions[2].variants[0], "[c] Difficulty", sizeof(actions[2].variants[0]));
        SDL_strlcpy(actions[2].variants[1], "[c] Diff", sizeof(actions[2].variants[1]));
        SDL_strlcpy(actions[2].variants[2], "[c] D", sizeof(actions[2].variants[2]));
        SDL_strlcpy(actions[2].variants[3], "[c]", sizeof(actions[2].variants[3]));

        SDL_strlcpy(actions[3].variants[0], "[u] Full list", sizeof(actions[3].variants[0]));
        SDL_strlcpy(actions[3].variants[1], "[u] List", sizeof(actions[3].variants[1]));
        SDL_strlcpy(actions[3].variants[2], "[u] L", sizeof(actions[3].variants[2]));
        SDL_strlcpy(actions[3].variants[3], "[u]", sizeof(actions[3].variants[3]));

        SDL_strlcpy(actions[4].variants[0], "[s] History", sizeof(actions[4].variants[0]));
        SDL_strlcpy(actions[4].variants[1], "[s] Hist", sizeof(actions[4].variants[1]));
        SDL_strlcpy(actions[4].variants[2], "[s] H", sizeof(actions[4].variants[2]));
        SDL_strlcpy(actions[4].variants[3], "[s]", sizeof(actions[4].variants[3]));

        SDL_strlcpy(actions[5].variants[0], "[x] Blitz", sizeof(actions[5].variants[0]));
        SDL_strlcpy(actions[5].variants[1], "[x] Blitz", sizeof(actions[5].variants[1]));
        SDL_strlcpy(actions[5].variants[2], "[x]", sizeof(actions[5].variants[2]));
        SDL_strlcpy(actions[5].variants[3], "[x]", sizeof(actions[5].variants[3]));
    }

    for (;;) {
        size_t len = metarun_render_action_prompt(actions, 6, out, out_size);
        if ((int)len <= term_width) break;

        int best_shrink = -1;
        int best_save = 0;
        for (int i = 0; i < 6; i++) {
            if (!actions[i].enabled) continue;
            if (actions[i].variant_idx + 1 >= actions[i].variant_count) continue;

            int cur_len = (int)strlen(actions[i].variants[actions[i].variant_idx]);
            int next_len = (int)strlen(actions[i].variants[actions[i].variant_idx + 1]);
            int save = cur_len - next_len;
            if (save > best_save) {
                best_save = save;
                best_shrink = i;
            }
        }

        if (best_shrink >= 0) {
            actions[best_shrink].variant_idx++;
            continue;
        }

        int drop_idx = -1;
        int drop_priority = INT_MAX;
        for (int i = 0; i < 6; i++) {
            if (!actions[i].enabled) continue;
            if (actions[i].drop_priority < drop_priority) {
                drop_priority = actions[i].drop_priority;
                drop_idx = i;
            }
        }

        if (drop_idx < 0) break;
        actions[drop_idx].enabled = false;
    }

    if (out[0] == '\0') {
        if (steamdeck) {
            strnfmt(out, out_size, "[%s]", spend);
        } else {
            SDL_strlcpy(out, blitz_enabled ? "[x]" : "[b]", out_size);
        }
    }
}

static void metarun_pick_best_variant(char *out,
                                      size_t out_size,
                                      int max_width,
                                      const char *v1,
                                      const char *v2,
                                      const char *v3,
                                      const char *v4)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';

    const char *variants[4] = { v1, v2, v3, v4 };
    const char *fallback = "";

    for (int i = 0; i < 4; i++) {
        const char *candidate = variants[i];
        if (!candidate || !*candidate) continue;
        fallback = candidate;
        if ((int)strlen(candidate) <= max_width) {
            SDL_strlcpy(out, candidate, out_size);
            return;
        }
    }

    SDL_strlcpy(out, fallback, out_size);
    metarun_truncate_for_width(out, max_width);
}

static void metarun_put_adaptive_line(int col,
                                      int *row,
                                      int term_width,
                                      byte attr,
                                      const char *v1,
                                      const char *v2,
                                      const char *v3,
                                      const char *v4)
{
    if (!row) return;
    int max_width = term_width - col - 1;
    if (max_width <= 0) return;

    char line[256];
    metarun_pick_best_variant(line, sizeof(line), max_width, v1, v2, v3, v4);
    Term_putstr(col, (*row)++, -1, attr, line);
}

/*
 * Enhanced print_metarun_stats():
 * - Draws a bracketed progress bar for Silmarils using '*'
 * - Renders deaths as a string of 'x' markers without a fixed limit
 * - Aligns labels & values for a cleaner layout
 * - Lists active curses with D: and (optionally) P: details
 * - Shows a blessing meter on the right side
 */
static void adjust_blessing_threshold_menu(void)
{
    const metarun_blessing_threshold_mode order[] = {
        METARUN_BLESSING_THRESHOLD_EASIER,
        METARUN_BLESSING_THRESHOLD_NORMAL,
        METARUN_BLESSING_THRESHOLD_HARDER
    };
    const char *labels[] = { "Easier", "Normal", "Harder" };
    const char *descs[] = {
        "If the game feels too hard, use this to earn blessings sooner.",
        "Default level.",
        "Pick this if you want fewer blessings by raising the threshold."
    };
    const int option_count = (int)N_ELEMENTS(order);

    if (current_run < 0 || current_run >= metarun_max) return;

    metarun_blessing_threshold_mode current_mode = metarun_get_threshold_mode(&metar);
    bool steamdeck = steamdeck_controls_active();
    bool menu_letters = sdl_menu_letters_enabled();
    char accept_label[16] = "";
    char back_label[16] = "";

    if (steamdeck) {
        /* Steam Deck UI: A=accept, B=back */
        metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label, sizeof(accept_label));
        metarun_prompt_label(steamdeck_back_key(), "B", back_label, sizeof(back_label));
    }
    int selection = 0;
    for (int i = 0; i < option_count; i++) {
        if (order[i] == current_mode) {
            selection = i;
            break;
        }
    }

    bool accepted = false;
    metarun_blessing_threshold_mode chosen_mode = current_mode;

    screen_save();

    while (true) {
        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        Term_putstr(2, 1, -1, TERM_YELLOW, "=== Blessing Threshold ===");

        char buf[160];
        u32b current_threshold = metarun_threshold_value(&metar);
        snprintf(buf, sizeof buf, "Current: %s (%lu points per blessing)",
                 threshold_mode_name(current_mode), (unsigned long)current_threshold);
        Term_putstr(2, 3, -1, TERM_L_BLUE, buf);

        int row = 5;
        for (int i = 0; i < option_count; i++) {
            metarun_blessing_threshold_mode mode = order[i];
            u32b mode_threshold = runtype_threshold_for_mode(metar.type, mode);
            bool is_highlighted = (i == selection);
            bool is_current = (mode == current_mode);

            /* Color scheme: Green for Easier, White for Normal, Orange for Harder */
            byte base_color = (mode == METARUN_BLESSING_THRESHOLD_EASIER) ? TERM_L_GREEN :
                             (mode == METARUN_BLESSING_THRESHOLD_HARDER) ? TERM_ORANGE :
                             TERM_WHITE;

            char option_buf[80];
            if (!menu_letters)
                snprintf(option_buf, sizeof option_buf, "%c  %s",
                         is_highlighted ? '>' : ' ', labels[i]);
            else
                snprintf(option_buf, sizeof option_buf, "%c%c) %s",
                         is_highlighted ? '>' : ' ', 'a' + i, labels[i]);

            byte name_attr = is_highlighted ? TERM_YELLOW : (is_current ? base_color : base_color);
            Term_putstr(2, row, -1, name_attr, option_buf);
            ui_menu_click_add(i, 2, row, 60);
            row++;

            snprintf(option_buf, sizeof option_buf, "    Requires %lu points per blessing",
                     (unsigned long)mode_threshold);
            byte threshold_attr = is_highlighted ? TERM_L_WHITE : TERM_L_DARK;
            Term_putstr(2, row, -1, threshold_attr, option_buf);
            ui_menu_click_add(i, 2, row, 60);
            row++;

            byte desc_attr = is_highlighted ? TERM_L_WHITE : TERM_SLATE;
            Term_putstr(4, row, -1, desc_attr, descs[i]);
            ui_menu_click_add(i, 2, row, 60);
            row++;
            row++;
        }

        if (steamdeck) {
            char hint_buf[96];
            strnfmt(hint_buf, sizeof(hint_buf),
                    "D-pad to choose  [%s] accept  [%s] cancel", accept_label, back_label);
            Term_putstr(2, row + 1, -1, TERM_L_DARK, hint_buf);
            ui_menu_click_add_text_token(-1, 2, row + 1, hint_buf, "cancel");
        } else if (menu_letters) {
            cptr prompt_text =
                "Use arrows or a/b/c to choose. Enter accepts, Esc cancels.";
            Term_putstr(2, row + 1, -1, TERM_L_DARK, prompt_text);
            ui_menu_click_add_text_token(-1, 2, row + 1, prompt_text,
                "Esc cancels");
        } else {
            cptr prompt_text =
                "Use arrows to choose. Enter accepts, Esc cancels.";
            Term_putstr(2, row + 1, -1, TERM_L_DARK, prompt_text);
            ui_menu_click_add_text_token(-1, 2, row + 1, prompt_text,
                "Esc cancels");
        }

        char key = metarun_inkey_hidden();

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice < option_count)
                {
                    if (click_action == UI_MENU_CLICK_HOVER
                        || clicked_choice != selection)
                    {
                        selection = clicked_choice;
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

        /* Handle back/cancel - ESC, B button in Steam Deck mode, or 'h' key */
        if (key == ESCAPE || (steamdeck && key == steamdeck_back_key()) || (!steamdeck && (key == 'h' || key == 'H'))) {
            ui_menu_click_clear();
            break;
        } else if (key == '\r' || key == '\n' || (steamdeck && key == steamdeck_confirm_key()) || key == '6') {
            accepted = true;
            chosen_mode = order[selection];
            break;
        } else if (key == '8' || key == 'k' || key == '-') {
            selection = (selection + option_count - 1) % option_count;
            continue;
        } else if (key == '2' || key == 'j' || key == '+') {
            selection = (selection + 1) % option_count;
            continue;
        } else if (menu_letters && key >= 'a' && key < 'a' + option_count) {
            selection = key - 'a';
            continue;
        } else if (menu_letters && key >= 'A' && key < 'A' + option_count) {
            selection = key - 'A';
            continue;
        }
    }

    bool changed = false;
    u32b new_threshold = 0;

    ui_menu_click_clear();

    if (accepted && chosen_mode != current_mode) {
        metarun_set_threshold_mode(&metar, chosen_mode);
        update_blessing_ledger(&metar);
        if (!sync_current_metarun_slot(false)) {
            log_warn("Threshold change: unable to sync metarun slot (idx=%d, max=%d)", current_run, metarun_max);
        }
        save_metaruns();
        changed = true;
        new_threshold = metarun_threshold_value(&metar);
    }

    if (accepted) {
        Term_clear();
        if (changed) {
            char msg[160];
            snprintf(msg, sizeof msg, "Blessing threshold set to %s.",
                     threshold_mode_name(chosen_mode));
            Term_putstr(2, 2, -1, TERM_L_GREEN, msg);
            snprintf(msg, sizeof msg, "New requirement: %lu points per blessing.",
                     (unsigned long)new_threshold);
            Term_putstr(2, 4, -1, TERM_WHITE, msg);
        } else {
            Term_putstr(2, 2, -1, TERM_L_DARK,
                        "Blessing threshold remains unchanged.");
        }
        if (steamdeck) {
            char hint_buf[64];
            metarun_prompt_label(steamdeck_confirm_key(), "A", accept_label, sizeof(accept_label));
            strnfmt(hint_buf, sizeof(hint_buf), "Press [%s] to continue.", accept_label);
            Term_putstr(2, 6, -1, TERM_L_DARK, hint_buf);
        } else {
            Term_putstr(2, 6, -1, TERM_L_DARK, "Press any key to continue.");
        }
        Term_fresh();
        ui_menu_click_begin();
        ui_menu_click_add_full_row('\r', 6);
        metarun_wait_hidden();
        ui_menu_click_clear();
    }

    ui_menu_click_clear();
    screen_load();
}

/* Updated print_metarun_stats(): prettier layout, star & death bars, curses list */
void print_metarun_stats(void)
{
    int row = 1;
    int col = 2;
    char buf[160];
    int term_height, term_width;

    refresh_current_metar_score();

    if (current_run < 0 || current_run >= metarun_max) {
        screen_save();
        screen_push_supporting_panes_hidden();
        screen_push_touch_pane_hidden();
        sdl_push_terminal_menu_scale();
        Term_clear();
        Term_putstr(2, 5, -1, TERM_RED, "Error: No metarun data available.");
        Term_putstr(2, 6, -1, TERM_L_WHITE, "Please start a new game first.");
        if (steamdeck_controls_active()) {
            char label[16];
            metarun_prompt_label(steamdeck_confirm_key(), "A", label, sizeof(label));
            strnfmt(buf, sizeof(buf), "Press %s to return.", label);
            Term_putstr(2, 8, -1, TERM_L_DARK, buf);
        } else {
            Term_putstr(2, 8, -1, TERM_L_DARK, "Press any key to return.");
        }
        ui_menu_click_begin();
        metarun_register_continue_clicks(Term ? Term->hgt : 24);
        metarun_wait_hidden();
        ui_menu_click_clear();
        sdl_pop_terminal_menu_scale();
        screen_pop_touch_pane_hidden();
        screen_pop_supporting_panes_hidden();
        screen_load();
        return;
    }

    compute_blessing_pool();
    metarun_sanitize_major_blessing_bits(&metar);

    const char *diff_name = "Unknown";
    int win_goal = WINCON_SILMARILS;

    if (runtype_info && metar.type < z_info->rt_max && runtype_info[metar.type].name[0])
    {
        diff_name = runtype_info[metar.type].name;
        win_goal = runtype_info[metar.type].win_con ? runtype_info[metar.type].win_con : WINCON_SILMARILS;
    }

    if (win_goal <= 0) win_goal = WINCON_SILMARILS;

    int remaining_silmarils = win_goal - metar.silmarils;
    if (remaining_silmarils < 0) remaining_silmarils = 0;

    char sil_bar[32];
    build_symbol_bar(sil_bar, sizeof sil_bar, metar.silmarils, win_goal, '*');
    char death_marks[32];
    build_death_marks(death_marks, sizeof death_marks, metar.deaths);

    int required_survivors = required_survivor_target(win_goal);
    int alive = metar.alive_characters;

    u32b best_run = get_best_run_score_from_highscores();
    u32b total_pool = metar.fallen_score_total;
    u32b remainder = metar.fallen_score_pool;

    /* Get blessing point threshold from runtype data */
    u32b threshold = metarun_threshold_value(&metar);
    if (threshold == 0) threshold = 1;
    const char *threshold_mode = threshold_mode_name(metarun_get_threshold_mode(&metar));

    int earned_points = metar.blessing_points;
    int spent_points = metar.blessing_points_spent;
    int available_points = earned_points - spent_points;
    bool startup_scene = (!character_generated || !p_ptr || !p_ptr->playing);

    if (!startup_scene) {
        screen_save();
        sdl_push_terminal_menu_scale();
    }
    screen_push_supporting_panes_hidden();
    screen_push_touch_pane_hidden();

redraw_story_stats:
    row = 1;
    col = 2;
    Term_clear();
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    Term_get_size(&term_width, &term_height);
    bool steamdeck = steamdeck_controls_active();
    char spend_label[16] = "";
    char threshold_label[16] = "";
    char diff_label[16] = "";
    char full_label[16] = "";
    char history_label[16] = "";
    char back_label[16] = "";
    char continue_label[16] = "";
    char blitz_label[16] = "";

    if (steamdeck) {
        /* Steam Deck UI: A=Continue, B=Back, X=Spend, Y=History,
         * L1=Diff, R1=Threshold, Start=Full list, RS Right=Blitz */
        int confirm_key = steamdeck_confirm_key();
        int back_key = steamdeck_back_key();
        int alt_key = steamdeck_alt_action_key();
        int secondary_key = steamdeck_secondary_key();
        int l1_key = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        int r1_key = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        int start_key = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_START);

        metarun_prompt_label(confirm_key, "A", continue_label, sizeof(continue_label));
        metarun_prompt_label(back_key, "B", back_label, sizeof(back_label));
        metarun_prompt_label(alt_key, "X", spend_label, sizeof(spend_label));
        metarun_prompt_label(secondary_key, "Y", history_label, sizeof(history_label));
        metarun_prompt_label(l1_key, "L1", diff_label, sizeof(diff_label));
        metarun_prompt_label(r1_key, "R1", threshold_label, sizeof(threshold_label));
        metarun_prompt_label(start_key, "Start", full_label, sizeof(full_label));
        metarun_prompt_label('x', "RS Right", blitz_label, sizeof(blitz_label));
    }

    /* Blitz can be launched from the in-game main menu; this quick-entry on
     * the metarun stats screen only applies before a character exists. */
    bool blitz_enabled = startup_scene;

    bool full_layout = (term_width >= 80 && term_height >= 24);
    int meter_col = 0;

    /* Count major blessings once (used by both layouts) */
    int unlocked_major = 0;
    int major_total = metarun_major_blessing_count();
    for (int i = 0; i < major_total; i++) {
        if (metarun_has_major_blessing_index(i)) unlocked_major++;
    }

    if (full_layout) {
        /* Calculate blessing meter position (right side) */
        meter_col = term_width - 16;
        if (meter_col < 60) meter_col = 60; /* Keep some space for main content */
        if (meter_col > term_width - 13) meter_col = term_width - 13; /* "Blessing Pool" is 13 chars */

        int meter_height = 15;
        int max_meter_height = term_height - 7;
        if (max_meter_height < 5) max_meter_height = 5;
        if (meter_height > max_meter_height) meter_height = max_meter_height;

        /* Draw blessing meter on the right side */
        draw_blessing_meter(meter_col, 2, meter_height, remainder, threshold);

        Term_putstr(col, row++, -1, TERM_YELLOW, "=== Current Story Statistics ===");

        snprintf(buf, sizeof buf, "Run-ID         : %u", metar.id);
        Term_putstr(col, row++, -1, TERM_WHITE, buf);

        snprintf(buf, sizeof buf, "Difficulty     : %s", diff_name);
        Term_putstr(col, row++, -1, TERM_L_BLUE, buf);

        snprintf(buf, sizeof buf, "Meta Score     : %lu", (unsigned long)metar.score);
        Term_putstr(col, row++, -1, TERM_WHITE, buf);

        snprintf(buf, sizeof buf, "Best Run Score : %lu", (unsigned long)best_run);
        Term_putstr(col, row++, -1, TERM_WHITE, buf);

        snprintf(buf, sizeof buf, "Silmarils      : %-22s %2d / %d (remaining %d)",
                 sil_bar, metar.silmarils, win_goal, remaining_silmarils);
        Term_putstr(col, row++, -1, TERM_WHITE, buf);

        byte alive_attr = (alive < required_survivors) ? TERM_RED : TERM_L_GREEN;
        snprintf(buf, sizeof buf, "Living Heroes  : %d (need >= %d)", alive, required_survivors);
        Term_putstr(col, row++, -1, alive_attr, buf);

        snprintf(buf, sizeof buf, "Deaths         : %-22s (%d total)",
                 death_marks, metar.deaths);
        Term_putstr(col, row++, -1, TERM_WHITE, buf);

        byte blessing_attr = (available_points > 0) ? TERM_L_GREEN : TERM_WHITE;
        snprintf(buf, sizeof buf, "Blessing Points: %d available (%d spent / %d earned)",
                 available_points, spent_points, earned_points);
        Term_putstr(col, row++, -1, blessing_attr, buf);

        if (steamdeck) {
            snprintf(buf, sizeof buf, "Blessing Pool  : %lu total (mode: %s, [%s] to change)",
                     (unsigned long)total_pool, threshold_mode, threshold_label);
        } else {
            snprintf(buf, sizeof buf, "Blessing Pool  : %lu total (mode: %s, press 'f' to change)",
                     (unsigned long)total_pool, threshold_mode);
        }
        Term_putstr(col, row++, -1, TERM_WHITE, buf);

        Term_putstr(col, row++, -1, TERM_YELLOW, "Major Blessings:");
        for (int i = 0; i < major_total; i++) {
            if (!metarun_has_major_blessing_index(i)) continue;
            const char *name = major_blessing_name_str(i);
            const char *desc = major_blessing_short_desc(i);
            char desc_buf[80];
            if (desc && *desc) {
                SDL_strlcpy(desc_buf, desc, sizeof desc_buf);
                char *nl = strchr(desc_buf, '\n');
                if (nl) *nl = '\0';
                snprintf(buf, sizeof buf, "  [X] %s (%s)", name, desc_buf);
            } else {
                snprintf(buf, sizeof buf, "  [X] %s", name);
            }
            Term_putstr(col, row++, -1, TERM_L_GREEN, buf);
        }
        if (unlocked_major == 0) {
            Term_putstr(col + 2, row++, -1, TERM_L_DARK, "None unlocked yet");
        }

        row++; /* spacing before lists */

        Term_putstr(col, row++, -1, TERM_YELLOW, "Active Curses & Blessings:");
    } else {
        int compact_width = term_width - col - 1;
        if (compact_width < 10) compact_width = 10;
        int summary_row_limit = term_height - 4;

        metarun_put_adaptive_line(col, &row, term_width, TERM_YELLOW,
                                  "=== Story Statistics ===",
                                  "=== Story Stats ===",
                                  "== Story Stats ==",
                                  "== Stats ==");

        char line1[192], line2[192], line3[192], line4[192];

        if (row < summary_row_limit) {
            strnfmt(line1, sizeof(line1), "Run-ID:%u  Difficulty:%s", metar.id, diff_name);
            strnfmt(line2, sizeof(line2), "ID:%u  Difficulty:%s", metar.id, diff_name);
            strnfmt(line3, sizeof(line3), "ID:%u  Diff:%s", metar.id, diff_name);
            strnfmt(line4, sizeof(line4), "ID:%u %s", metar.id, diff_name);
            metarun_put_adaptive_line(col, &row, term_width, TERM_L_BLUE, line1, line2, line3, line4);
        }

        if (row < summary_row_limit) {
            strnfmt(line1, sizeof(line1), "Meta Score:%lu  Best Run:%lu", (unsigned long)metar.score, (unsigned long)best_run);
            strnfmt(line2, sizeof(line2), "Meta:%lu  Best:%lu", (unsigned long)metar.score, (unsigned long)best_run);
            strnfmt(line3, sizeof(line3), "Score:%lu  Best:%lu", (unsigned long)metar.score, (unsigned long)best_run);
            strnfmt(line4, sizeof(line4), "M:%lu B:%lu", (unsigned long)metar.score, (unsigned long)best_run);
            metarun_put_adaptive_line(col, &row, term_width, TERM_WHITE, line1, line2, line3, line4);
        }

        if (row < summary_row_limit) {
            strnfmt(line1, sizeof(line1), "Silmarils:%d/%d (rem %d)  Alive:%d/%d", metar.silmarils, win_goal, remaining_silmarils, alive, required_survivors);
            strnfmt(line2, sizeof(line2), "Sil:%d/%d rem %d  Alive:%d/%d", metar.silmarils, win_goal, remaining_silmarils, alive, required_survivors);
            strnfmt(line3, sizeof(line3), "Sil:%d/%d  Alive:%d/%d", metar.silmarils, win_goal, alive, required_survivors);
            strnfmt(line4, sizeof(line4), "S:%d/%d A:%d/%d", metar.silmarils, win_goal, alive, required_survivors);
            byte sil_alive_attr = (alive < required_survivors) ? TERM_RED : TERM_L_GREEN;
            metarun_put_adaptive_line(col, &row, term_width, sil_alive_attr, line1, line2, line3, line4);
        }

        if (row < summary_row_limit) {
            strnfmt(line1, sizeof(line1), "Deaths:%d  Blessing Points:%d (%d/%d)", metar.deaths, available_points, spent_points, earned_points);
            strnfmt(line2, sizeof(line2), "Deaths:%d  BPoints:%d (%d/%d)", metar.deaths, available_points, spent_points, earned_points);
            strnfmt(line3, sizeof(line3), "Deaths:%d  BP:%d", metar.deaths, available_points);
            strnfmt(line4, sizeof(line4), "D:%d BP:%d", metar.deaths, available_points);
            byte bp_attr = (available_points > 0) ? TERM_L_GREEN : TERM_WHITE;
            metarun_put_adaptive_line(col, &row, term_width, bp_attr, line1, line2, line3, line4);
        }

        if (row < summary_row_limit) {
            if (steamdeck) {
                strnfmt(line1, sizeof(line1), "Blessing Pool:%lu/%lu (%s, [%s] change)",
                        (unsigned long)remainder, (unsigned long)threshold, threshold_mode, threshold_label);
                strnfmt(line2, sizeof(line2), "Pool:%lu/%lu  %s  [%s]",
                        (unsigned long)remainder, (unsigned long)threshold, threshold_mode, threshold_label);
            } else {
                strnfmt(line1, sizeof(line1), "Blessing Pool:%lu/%lu (%s, press 'f')",
                        (unsigned long)remainder, (unsigned long)threshold, threshold_mode);
                strnfmt(line2, sizeof(line2), "Pool:%lu/%lu  %s (f)",
                        (unsigned long)remainder, (unsigned long)threshold, threshold_mode);
            }
            strnfmt(line3, sizeof(line3), "Pool:%lu/%lu  %s",
                    (unsigned long)remainder, (unsigned long)threshold, threshold_mode);
            strnfmt(line4, sizeof(line4), "P:%lu/%lu", (unsigned long)remainder, (unsigned long)threshold);
            metarun_put_adaptive_line(col, &row, term_width, TERM_WHITE, line1, line2, line3, line4);
        }

        if (row < summary_row_limit) {
            strnfmt(line1, sizeof(line1), "Major Blessings:%d", unlocked_major);
            strnfmt(line2, sizeof(line2), "Major:%d", unlocked_major);
            strnfmt(line3, sizeof(line3), "Major:%d", unlocked_major);
            strnfmt(line4, sizeof(line4), "M:%d", unlocked_major);
            metarun_put_adaptive_line(col, &row, term_width, TERM_YELLOW, line1, line2, line3, line4);
        }

        if (unlocked_major > 0 && row < summary_row_limit) {
            char majors_line[192];
            majors_line[0] = '\0';
            SDL_strlcpy(majors_line, "  ", sizeof(majors_line));
            bool first = true;
            for (int i = 0; i < major_total; i++) {
                if (!metarun_has_major_blessing_index(i)) continue;
                const char *name = major_blessing_name_str(i);
                char tmp[96];
                if (first) {
                    strnfmt(tmp, sizeof(tmp), "%s", name);
                    first = false;
                } else {
                    strnfmt(tmp, sizeof(tmp), ", %s", name);
                }
                if ((int)strlen(majors_line) + (int)strlen(tmp) > compact_width) break;
                SDL_strlcat(majors_line, tmp, sizeof(majors_line));
            }
            if (majors_line[2] != '\0') {
                metarun_truncate_for_width(majors_line, compact_width);
                Term_putstr(col, row++, -1, TERM_L_GREEN, majors_line);
            }
        }

        if (row < term_height - 2) {
            metarun_put_adaptive_line(col, &row, term_width, TERM_YELLOW,
                                      "Active Curses & Blessings:",
                                      "Curses & Blessings:",
                                      "Effects:",
                                      "Fx:");
        }
    }

    if (full_layout) {
        /* --- Full (>=80x24) layout: keep existing rendering exactly --- */

        /* Calculate max width for effect display (left side only, meter is separate) */
        int max_display_width = (meter_col > 60) ? meter_col - 4 : 56;

        int available_lines = term_height - row - 2;
        if (available_lines < 0) available_lines = 0;

        int active_count = 0;
        for (int id = 0; id < z_info->cu_max; id++) {
            if (CURSE_GET(id) != 0) active_count++;
        }

        bool curses_truncated = false;
        if (active_count == 0) {
            Term_putstr(col + 2, row++, -1, TERM_L_DARK, "None active");
        } else if (available_lines <= 0) {
            curses_truncated = true;
            if (steamdeck) {
                snprintf(buf, sizeof buf, "List truncated - press [%s] to view all effects", full_label);
                Term_putstr(col + 2, row++, -1, TERM_L_DARK, buf);
            } else {
                Term_putstr(col + 2, row++, -1, TERM_L_DARK,
                            "List truncated - press 'u' to view all effects");
            }
        } else {
            int lines_remaining = available_lines;
            int entries_remaining = active_count;

            for (int id = 0; id < z_info->cu_max; id++) {
                int stacks = CURSE_GET(id);
                if (!stacks) continue;

                if (lines_remaining <= 0) {
                    curses_truncated = true;
                    break;
                }

                entries_remaining--;
                bool is_blessing = (stacks < 0);
                int magnitude = is_blessing ? -stacks : stacks;
                cptr name = is_blessing ? blessing_display_name(id) : curse_display_name(id);
                byte attr = is_blessing ? TERM_L_GREEN : TERM_RED;
                bool seen = CURSE_SEEN(id);

                const curse_type *cu = &cu_info[id];
                cptr effect = NULL;

                /* Only show H:/P: effect if identified */
                if (seen) {
                    effect = is_blessing
                        ? (cu->blessing_power ? cu_text + cu->blessing_power : NULL)
                        : (cu->power ? cu_text + cu->power : NULL);
                }

                /* Format with proper alignment - shorten type labels and move closer to left */
                /* Use shortened labels: "Bless" and "Curse" instead of full words */
                if (effect && *effect) {
                    snprintf(buf, sizeof buf, "  %-28s %-5s %d - %s", name,
                             is_blessing ? "Bless" : "Curse", magnitude, effect);
                } else {
                    snprintf(buf, sizeof buf, "  %-28s %-5s %d", name,
                             is_blessing ? "Bless" : "Curse", magnitude);
                }

                /* Truncate if too long */
                if ((int)strlen(buf) > max_display_width) {
                    buf[max_display_width - 3] = '.';
                    buf[max_display_width - 2] = '.';
                    buf[max_display_width - 1] = '.';
                    buf[max_display_width] = '\0';
                }

                Term_putstr(col, row++, -1, attr, buf);
                lines_remaining--;

                if (lines_remaining <= 0 && entries_remaining > 0) {
                    curses_truncated = true;
                    break;
                }
            }

            if (curses_truncated && lines_remaining > 0) {
                if (entries_remaining > 0) {
                    if (steamdeck) {
                        snprintf(buf, sizeof buf, "... and %d more effect%s (press [%s] to view all)",
                                 entries_remaining, (entries_remaining == 1) ? "" : "s", full_label);
                    } else {
                        snprintf(buf, sizeof buf, "... and %d more effect%s (press 'u' to view all)",
                                 entries_remaining, (entries_remaining == 1) ? "" : "s");
                    }
                } else {
                    if (steamdeck) {
                        snprintf(buf, sizeof buf, "List truncated - press [%s] to view all effects", full_label);
                    } else {
                        SDL_strlcpy(buf, "List truncated - press 'u' to view all effects",
                                  sizeof buf);
                    }
                }
                Term_putstr(col, row++, -1, TERM_L_DARK, buf);
            }
        }

        /* Prompt line (full): dynamically packed to width */
        char prompt_buf[256];
        metarun_build_action_prompt(term_width, steamdeck,
                                    spend_label, threshold_label, diff_label,
                                    full_label, history_label, blitz_label,
                                    blitz_enabled,
                                    prompt_buf, sizeof(prompt_buf));
        metarun_truncate_for_width(prompt_buf, term_width);
        metarun_put_prompt_line(term_width, term_height, TERM_L_DARK, prompt_buf);
        metarun_register_stats_prompt_clicks(prompt_buf, term_height - 1,
                                             spend_label, threshold_label,
                                             diff_label, full_label,
                                             history_label, blitz_label,
                                             blitz_enabled);
        metarun_register_continue_clicks(term_height);
    } else {
        /* --- Compact layout --- */
        int max_display_width = term_width - col - 1;
        if (max_display_width < 10) max_display_width = 10;
        if (max_display_width > (int)sizeof(buf) - 1) max_display_width = (int)sizeof(buf) - 1;

        int available_lines = term_height - row - 2;
        if (available_lines < 0) available_lines = 0;

        int active_count = 0;
        for (int id = 0; id < z_info->cu_max; id++) {
            if (CURSE_GET(id) != 0) active_count++;
        }

        bool curses_truncated = false;
        if (active_count == 0) {
            Term_putstr(col + 2, row++, -1, TERM_L_DARK, "None active");
        } else if (available_lines <= 0) {
            curses_truncated = true;
            if (steamdeck) {
                snprintf(buf, sizeof buf, "List truncated - press [%s] to view all effects", full_label);
                metarun_truncate_for_width(buf, term_width - col - 1);
                Term_putstr(col + 2, row++, -1, TERM_L_DARK, buf);
            } else {
                Term_putstr(col + 2, row++, -1, TERM_L_DARK,
                            "List truncated - press 'u' to view all effects");
            }
        } else {
            int lines_remaining = available_lines;
            int entries_remaining = active_count;
            bool show_effects = (max_display_width >= 36);
            int value_width = 4;
            int name_width = max_display_width - 2 - 1 - value_width; /* "  " + name + " " + value */
            if (show_effects) {
                int reserve_effect = max_display_width / 3;
                if (reserve_effect < 10) reserve_effect = 10;
                int with_effect = max_display_width - 2 - 1 - value_width - 3 - reserve_effect; /* " - " + effect */
                if (with_effect >= 8) name_width = with_effect;
            }
            if (name_width > 26) name_width = 26;
            if (name_width < 8) name_width = 8;

            for (int id = 0; id < z_info->cu_max; id++) {
                int stacks = CURSE_GET(id);
                if (!stacks) continue;

                if (lines_remaining <= 0) {
                    curses_truncated = true;
                    break;
                }

                entries_remaining--;
                bool is_blessing = (stacks < 0);
                int magnitude = is_blessing ? -stacks : stacks;
                cptr name = is_blessing ? blessing_display_name(id) : curse_display_name(id);
                byte attr = is_blessing ? TERM_L_GREEN : TERM_RED;
                bool seen = CURSE_SEEN(id);

                const curse_type *cu = &cu_info[id];
                cptr effect = NULL;

                if (seen && show_effects) {
                    effect = is_blessing
                        ? (cu->blessing_power ? cu_text + cu->blessing_power : NULL)
                        : (cu->power ? cu_text + cu->power : NULL);
                }

                char sign = is_blessing ? '+' : '-';
                char value_buf[16];
                strnfmt(value_buf, sizeof(value_buf), "%c%d", sign, magnitude);

                if (effect && *effect && show_effects) {
                    snprintf(buf, sizeof buf, "  %-*.*s %*s - %s",
                             name_width, name_width, name,
                             value_width, value_buf, effect);
                } else {
                    snprintf(buf, sizeof buf, "  %-*.*s %*s",
                             name_width, name_width, name,
                             value_width, value_buf);
                }

                metarun_truncate_for_width(buf, max_display_width);
                Term_putstr(col, row++, -1, attr, buf);
                lines_remaining--;

                if (lines_remaining <= 0 && entries_remaining > 0) {
                    curses_truncated = true;
                    break;
                }
            }

            if (curses_truncated && lines_remaining > 0) {
                char line1[160], line2[160], line3[160], line4[160];
                if (entries_remaining > 0) {
                    if (steamdeck) {
                        snprintf(line1, sizeof line1, "... and %d more effects (press [%s] for full list)",
                                 entries_remaining, full_label);
                        snprintf(line2, sizeof line2, "... and %d more (press [%s] for list)",
                                 entries_remaining, full_label);
                        snprintf(line3, sizeof line3, "... %d more [%s]", entries_remaining, full_label);
                        snprintf(line4, sizeof line4, "... %d more", entries_remaining);
                    } else {
                        snprintf(line1, sizeof line1, "... and %d more effects (press 'u' for full list)", entries_remaining);
                        snprintf(line2, sizeof line2, "... and %d more (press 'u' for list)", entries_remaining);
                        snprintf(line3, sizeof line3, "... %d more (u)", entries_remaining);
                        snprintf(line4, sizeof line4, "... %d more", entries_remaining);
                    }
                } else {
                    if (steamdeck) {
                        snprintf(line1, sizeof line1, "List truncated - press [%s] to view all effects", full_label);
                        snprintf(line2, sizeof line2, "List truncated - press [%s]", full_label);
                        snprintf(line3, sizeof line3, "Truncated [%s]", full_label);
                        SDL_strlcpy(line4, "Truncated", sizeof(line4));
                    } else {
                        SDL_strlcpy(line1, "List truncated - press 'u' to view all effects", sizeof(line1));
                        SDL_strlcpy(line2, "List truncated - press 'u' for list", sizeof(line2));
                        SDL_strlcpy(line3, "Truncated (u)", sizeof(line3));
                        SDL_strlcpy(line4, "Truncated", sizeof(line4));
                    }
                }
                metarun_pick_best_variant(buf, sizeof(buf), term_width - col - 1,
                                          line1, line2, line3, line4);
                Term_putstr(col, row++, -1, TERM_L_DARK, buf);
            }
        }

        char prompt_buf[256];
        metarun_build_action_prompt(term_width, steamdeck,
                                    spend_label, threshold_label, diff_label,
                                    full_label, history_label, blitz_label,
                                    blitz_enabled,
                                    prompt_buf, sizeof(prompt_buf));
        metarun_truncate_for_width(prompt_buf, term_width);
        metarun_put_prompt_line(term_width, term_height, TERM_L_DARK, prompt_buf);
        metarun_register_stats_prompt_clicks(prompt_buf, term_height - 1,
                                             spend_label, threshold_label,
                                             diff_label, full_label,
                                             history_label, blitz_label,
                                             blitz_enabled);
        metarun_register_continue_clicks(term_height);
    }

    char key = metarun_inkey_hidden();
    bool redraw_for_hover = false;
    {
        int clicked_choice = 0;
        int click_action = UI_MENU_CLICK_PRIMARY;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if (click_action == UI_MENU_CLICK_HOVER)
                redraw_for_hover = true;
            else
                key = (char)clicked_choice;
        }
        else if (key == UI_MENU_CLICK_WAKE_KEY)
            redraw_for_hover = true;
    }
    ui_menu_click_clear();
    if (redraw_for_hover)
        goto redraw_story_stats;

    if (steamdeck) {
        int back_key = steamdeck_back_key();
        int confirm_key = steamdeck_confirm_key();
        int alt_key = steamdeck_alt_action_key();
        int secondary_key = steamdeck_secondary_key();
        int l1_key = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        int r1_key = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        int start_key = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_START);

        if (key == back_key) {
            /* B button = exit/back */
            screen_pop_touch_pane_hidden();
            screen_pop_supporting_panes_hidden();
            if (!startup_scene) {
                sdl_pop_terminal_menu_scale();
                screen_load();
            }
            return;
        } else if (key == confirm_key || key == ' ' || key == '\r' || key == '\n') {
            /* A button = continue (exit) */
            screen_pop_touch_pane_hidden();
            screen_pop_supporting_panes_hidden();
            if (!startup_scene) {
                sdl_pop_terminal_menu_scale();
                screen_load();
            }
            return;
        } else if (key == alt_key) {
            /* X button = spend blessings */
            key = 'b';
        } else if (key == secondary_key) {
            /* Y button = history */
            key = 's';
        } else if (key == l1_key) {
            /* L1 = difficulty */
            key = 'c';
        } else if (key == r1_key) {
            /* R1 = threshold */
            key = 'f';
        } else if (key == start_key) {
            /* Start = full list */
            key = 'u';
        }
    }
    if (key == 'b' || key == 'B') {
        screen_pop_touch_pane_hidden();
        screen_pop_supporting_panes_hidden();
        if (!startup_scene) {
            sdl_pop_terminal_menu_scale();
            screen_load();
        }
        open_blessing_exchange();
        print_metarun_stats();
        return;
    } else if (key == 'c' || key == 'C') {
        screen_pop_touch_pane_hidden();
        screen_pop_supporting_panes_hidden();
        if (!startup_scene) {
            sdl_pop_terminal_menu_scale();
            screen_load();
        }
        choose_difficulty_menu();
        return;
    } else if (key == 'f' || key == 'F') {
        screen_pop_touch_pane_hidden();
        screen_pop_supporting_panes_hidden();
        if (!startup_scene) {
            sdl_pop_terminal_menu_scale();
            screen_load();
        }
        adjust_blessing_threshold_menu();
        print_metarun_stats();
        return;
    } else if (key == 'u' || key == 'U') {
        /* Show the full list of active curses/blessings separately */
        screen_pop_touch_pane_hidden();
        screen_pop_supporting_panes_hidden();
        if (!startup_scene) {
            sdl_pop_terminal_menu_scale();
            screen_load();
        }
        show_all_active_curses();
        print_metarun_stats();
        return;
    } else if (key == 's' || key == 'S') {
        /* Show history only */
        screen_pop_touch_pane_hidden();
        screen_pop_supporting_panes_hidden();
        if (!startup_scene) {
            sdl_pop_terminal_menu_scale();
            screen_load();
        }
        list_metaruns();
        print_metarun_stats();
        return;
    } else if ((key == 'x' || key == 'X') && blitz_enabled) {
        screen_pop_touch_pane_hidden();
        screen_pop_supporting_panes_hidden();
        if (!startup_scene) {
            sdl_pop_terminal_menu_scale();
            screen_load();
        }
        run_mode_set_pending(RUN_MODE_BLITZ);
        run_mode_set_current(RUN_MODE_BLITZ);
        return;
    }

    screen_pop_touch_pane_hidden();
    screen_pop_supporting_panes_hidden();
    if (!startup_scene) {
        sdl_pop_terminal_menu_scale();
        screen_load();
    }
}
