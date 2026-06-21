#include "angband.h"
#include "metarun-internal.h"

#if 0 /* Previous one-page dashboard helpers; retained for historical context. */
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
#endif

static void metarun_truncate_for_width(char *buf, int max_width)
{
    int total;
    int width;
    int dots;
    int prefix;
    int w;

    if (!buf) return;
    if (max_width <= 0) {
        buf[0] = '\0';
        return;
    }

    /* Measure in display columns, not bytes, so a UTF-8 line is only cut when
     * it is genuinely too wide on screen. */
    total = (int)strlen(buf);
    width = utf8_display_width_n(buf, total);
    if (width <= max_width) return;

    /* Reserve up to three columns for an ellipsis, then keep whole UTF-8
     * sequences until those columns are filled and cut on that boundary, so a
     * multibyte glyph is never split in half. */
    dots = (max_width >= 4) ? 3 : 0;
    prefix = 0;
    w = 0;
    while (prefix < total) {
        int clen = utf8_sequence_len_n(buf + prefix, total - prefix);
        int cw;

        if (clen <= 0) break;
        cw = utf8_display_width_n(buf + prefix, clen);
        if (w + cw > max_width - dots) break;
        prefix += clen;
        w += cw;
    }
    if (dots) {
        buf[prefix++] = '.';
        buf[prefix++] = '.';
        buf[prefix++] = '.';
    }
    buf[prefix] = '\0';
}

#if 0 /* Previous one-page dashboard input and compact-layout helpers. */
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

/* Input-specific phrase telling the player how to open the full effects list. */
static const char *metarun_view_all_hint(bool steamdeck, const char *full_label)
{
    static char buf[48];

    if (steamdeck) {
        strnfmt(buf, sizeof(buf), "press [%s]",
                (full_label && full_label[0]) ? full_label : "Start");
        return buf;
    }
    if (sdl_touch_only_device_active())
        return "tap Full list";
    return "press 'u'";
}

/*
 * Touch-only action bar for the story-statistics screen.  Replaces the
 * keyboard-letter prompt with tappable command buttons laid out across the
 * two reserved bottom rows.  Each button maps to the same choice the keyboard
 * handler already understands ('b','f','c','u','s','x') plus a Done button
 * (ESCAPE) that exits.  Register these before metarun_register_continue_clicks
 * so the buttons win over the tap-anywhere-to-exit full rows.
 */
static void metarun_register_touch_action_buttons(int term_width,
    int term_height, bool blitz_enabled)
{
    struct { int choice; const char *label; } items[8];
    int n = 0;
    int row;
    int col;

    if (term_height < 2 || term_width < 8)
        return;

    items[n].choice = 'b'; items[n].label = "Spend blessings"; n++;
    items[n].choice = 'f'; items[n].label = "Threshold"; n++;
    items[n].choice = 'c'; items[n].label = "Difficulty"; n++;
    items[n].choice = 'u'; items[n].label = "Full list"; n++;
    items[n].choice = 's'; items[n].label = "History"; n++;
    if (blitz_enabled) { items[n].choice = 'x'; items[n].label = "Blitz"; n++; }
    items[n].choice = ESCAPE; items[n].label = "Done"; n++;

    row = term_height - 2;
    col = 0;
    for (int i = 0; i < n; i++) {
        int w = (int)strlen(items[i].label) + 4; /* "[ " + label + " ]" */
        if (col != 0 && col + w > term_width) {
            if (row < term_height - 1) {
                row++;
                col = 0;
            } else {
                break; /* no more room to lay out buttons */
            }
        }
        col = ui_menu_click_put_button(items[i].choice, row, col,
            TERM_L_WHITE, items[i].label);
    }
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
#endif

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
        int term_width = 80;
        int term_height = 24;

        Term_get_size(&term_width, &term_height);

        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_outside_cancel_enabled(true);
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
            char prompt_full[96];
            char prompt_short[80];
            const char* variants[2];

            strnfmt(prompt_full, sizeof(prompt_full),
                "D-pad choose  [%s] accept  [%s] cancel", accept_label,
                back_label);
            strnfmt(prompt_short, sizeof(prompt_short),
                "[%s] accept  [%s] cancel", accept_label, back_label);
            variants[0] = prompt_full;
            variants[1] = prompt_short;
            terminal_prompt_pick_variant(hint_buf, sizeof(hint_buf),
                term_width - 2, false, variants, N_ELEMENTS(variants));
            Term_putstr(2, row + 1, -1, TERM_L_DARK, hint_buf);
            ui_menu_click_add_text_token(-1, 2, row + 1, hint_buf, "cancel");
        } else if (sdl_touch_only_device_active()) {
            char prompt_text[96];
            const char* variants[] = {
                "Tap a row to select, tap away to exit",
                "Tap to select, tap away to exit",
                "Tap to select"
            };
            terminal_prompt_pick_variant(prompt_text, sizeof(prompt_text),
                term_width - 2, false, variants, N_ELEMENTS(variants));
            Term_putstr(2, row + 1, -1, TERM_L_DARK, prompt_text);
        } else if (menu_letters) {
            char prompt_text[96];
            const char* variants[] = {
                "Dir choose  a/b/c select  Enter accept  Esc cancel",
                "a/b/c select  Enter accept  Esc cancel",
                "Enter accept  Esc cancel"
            };
            terminal_prompt_pick_variant(prompt_text, sizeof(prompt_text),
                term_width - 2, false, variants, N_ELEMENTS(variants));
            Term_putstr(2, row + 1, -1, TERM_L_DARK, prompt_text);
            ui_menu_click_add_text_token(-1, 2, row + 1, prompt_text,
                "Esc cancels");
            ui_menu_click_add_text_token(-1, 2, row + 1, prompt_text,
                "Esc cancel");
        } else {
            char prompt_text[96];
            const char* variants[] = {
                "Dir choose  Enter accept  Esc cancel",
                "Enter accept  Esc cancel"
            };
            terminal_prompt_pick_variant(prompt_text, sizeof(prompt_text),
                term_width - 2, false, variants, N_ELEMENTS(variants));
            Term_putstr(2, row + 1, -1, TERM_L_DARK, prompt_text);
            ui_menu_click_add_text_token(-1, 2, row + 1, prompt_text,
                "Esc cancels");
            ui_menu_click_add_text_token(-1, 2, row + 1, prompt_text,
                "Esc cancel");
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

/*
 * Run a story-statistics sub-menu without flashing the underlying gameplay
 * screen.  print_metarun_stats() keeps its own screen_save() for the whole
 * session; each sub-menu saves and restores its own screen on top of the stats
 * view.  We deliberately do NOT screen_load() here - restoring the main screen
 * between the stats view and the sub-menu made the gameplay screen blink.  We
 * only drop the pane-hide / menu scale (which the sub-menus render without) and
 * restore them when the sub-menu returns.
 */
static void metarun_run_substats_menu(bool startup_scene, void (*fn)(void))
{
    screen_pop_touch_pane_hidden();
    screen_pop_supporting_panes_hidden();
    if (!startup_scene)
        sdl_pop_terminal_menu_scale();

    fn();

    if (!startup_scene)
        sdl_push_terminal_menu_scale();
    screen_push_supporting_panes_hidden();
    screen_push_touch_pane_hidden();
}

#if 0 /* Previous one-page dashboard, superseded by the story book below. */
/* Updated print_metarun_stats(): prettier layout, star & death bars, curses list */
static void print_metarun_stats_legacy(void)
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
        } else if (sdl_touch_only_device_active()) {
            Term_putstr(2, 8, -1, TERM_L_DARK, "Tap to return");
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

    bool startup_scene = (!character_generated || !p_ptr || !p_ptr->playing);

    /* Save the screen and hide panes ONCE for the whole stats session.  Each
     * sub-menu saves/restores its own screen on top of this view, so we must
     * not restore the underlying main screen between them (that flashed the
     * gameplay screen).  Loop in place instead, recomputing after each
     * sub-menu returns. */
    if (!startup_scene) {
        screen_save();
        sdl_push_terminal_menu_scale();
    }
    screen_push_supporting_panes_hidden();
    screen_push_touch_pane_hidden();

    bool exit_stats = false;
    bool launch_blitz = false;

    while (!exit_stats)
    {
    /* Re-read live score/state each pass so changes made in a sub-menu
     * (spending blessings, difficulty, threshold) show up on return. */
    refresh_current_metar_score();
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
        } else if (sdl_touch_only_device_active()) {
            snprintf(buf, sizeof buf, "Blessing Pool  : %lu total (mode: %s, tap Threshold to change)",
                     (unsigned long)total_pool, threshold_mode);
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
            } else if (sdl_touch_only_device_active()) {
                strnfmt(line1, sizeof(line1), "Blessing Pool:%lu/%lu (%s, tap Threshold)",
                        (unsigned long)remainder, (unsigned long)threshold, threshold_mode);
                strnfmt(line2, sizeof(line2), "Pool:%lu/%lu  %s",
                        (unsigned long)remainder, (unsigned long)threshold, threshold_mode);
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
            snprintf(buf, sizeof buf, "List truncated - %s to view all effects",
                     metarun_view_all_hint(steamdeck, full_label));
            Term_putstr(col + 2, row++, -1, TERM_L_DARK, buf);
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
                char padded[96];
                metarun_display_pad(padded, sizeof padded, name, 28);
                if (effect && *effect) {
                    snprintf(buf, sizeof buf, "  %s %-5s %d - %s", padded,
                             is_blessing ? "Bless" : "Curse", magnitude, effect);
                } else {
                    snprintf(buf, sizeof buf, "  %s %-5s %d", padded,
                             is_blessing ? "Bless" : "Curse", magnitude);
                }

                /* Truncate to the display width (UTF-8 aware). */
                metarun_truncate_for_width(buf, max_display_width);

                Term_putstr(col, row++, -1, attr, buf);
                lines_remaining--;

                if (lines_remaining <= 0 && entries_remaining > 0) {
                    curses_truncated = true;
                    break;
                }
            }

            if (curses_truncated && lines_remaining > 0) {
                if (entries_remaining > 0) {
                    snprintf(buf, sizeof buf, "... and %d more effect%s (%s to view all)",
                             entries_remaining, (entries_remaining == 1) ? "" : "s",
                             metarun_view_all_hint(steamdeck, full_label));
                } else {
                    snprintf(buf, sizeof buf, "List truncated - %s to view all effects",
                             metarun_view_all_hint(steamdeck, full_label));
                }
                Term_putstr(col, row++, -1, TERM_L_DARK, buf);
            }
        }

        /* Prompt line (full): dynamically packed to width */
        if (sdl_touch_only_device_active()) {
            metarun_register_touch_action_buttons(term_width, term_height,
                                                  blitz_enabled);
        } else {
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
        }
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
            snprintf(buf, sizeof buf, "List truncated - %s to view all effects",
                     metarun_view_all_hint(steamdeck, full_label));
            metarun_truncate_for_width(buf, term_width - col - 1);
            Term_putstr(col + 2, row++, -1, TERM_L_DARK, buf);
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
                    } else if (sdl_touch_only_device_active()) {
                        snprintf(line1, sizeof line1, "... and %d more effects (tap Full list)", entries_remaining);
                        snprintf(line2, sizeof line2, "... and %d more (tap Full list)", entries_remaining);
                        snprintf(line3, sizeof line3, "... %d more", entries_remaining);
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
                    } else if (sdl_touch_only_device_active()) {
                        SDL_strlcpy(line1, "List truncated - tap Full list to view all effects", sizeof(line1));
                        SDL_strlcpy(line2, "List truncated - tap Full list", sizeof(line2));
                        SDL_strlcpy(line3, "Truncated", sizeof(line3));
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

        if (sdl_touch_only_device_active()) {
            metarun_register_touch_action_buttons(term_width, term_height,
                                                  blitz_enabled);
        } else {
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
        }
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
            exit_stats = true;
        } else if (key == confirm_key || key == ' ' || key == '\r' || key == '\n') {
            /* A button = continue (exit) */
            exit_stats = true;
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

    if (exit_stats) {
        /* Leave the loop and tear the stats screen down once below. */
    } else if (key == 'b' || key == 'B') {
        metarun_run_substats_menu(startup_scene, open_blessing_exchange);
        continue;
    } else if (key == 'c' || key == 'C') {
        metarun_run_substats_menu(startup_scene, choose_difficulty_menu);
        continue;
    } else if (key == 'f' || key == 'F') {
        metarun_run_substats_menu(startup_scene, adjust_blessing_threshold_menu);
        continue;
    } else if (key == 'u' || key == 'U') {
        /* Show the full list of active curses/blessings separately */
        metarun_run_substats_menu(startup_scene, show_all_active_curses);
        continue;
    } else if (key == 's' || key == 'S') {
        /* Show history only */
        metarun_run_substats_menu(startup_scene, list_metaruns);
        continue;
    } else if ((key == 'x' || key == 'X') && blitz_enabled) {
        launch_blitz = true;
        exit_stats = true;
    } else {
        /* Any other key (including Esc / Done) exits the stats screen. */
        exit_stats = true;
    }
    } /* end while (!exit_stats) */

    screen_pop_touch_pane_hidden();
    screen_pop_supporting_panes_hidden();
    if (!startup_scene) {
        sdl_pop_terminal_menu_scale();
        screen_load();
    }

    if (launch_blitz) {
        run_mode_set_pending(RUN_MODE_BLITZ);
        run_mode_set_current(RUN_MODE_BLITZ);
    }
}
#endif

/* ------------------------------------------------------------------------
 * Story book
 *
 * This replaces the old single statistics dashboard with five persistent
 * pages.  The book deliberately delegates all mutations to the existing
 * blessing, threshold, and difficulty menus so their validation and save
 * behavior remain the single source of truth.
 * ------------------------------------------------------------------------ */

enum story_book_page {
    STORY_BOOK_STATISTICS = 0,
    STORY_BOOK_BLESSINGS,
    STORY_BOOK_CURSES,
    STORY_BOOK_DIFFICULTY,
    STORY_BOOK_METARUNS,
    STORY_BOOK_PAGE_MAX
};

enum story_book_action {
    STORY_BOOK_PREVIOUS = 100,
    STORY_BOOK_NEXT,
    STORY_BOOK_CLOSE,
    STORY_BOOK_EXCHANGE,
    STORY_BOOK_THRESHOLD,
    STORY_BOOK_CHANGE_DIFFICULTY,
    STORY_BOOK_BLITZ,
    STORY_BOOK_CURSES_EARLIER,
    STORY_BOOK_CURSES_LATER,
    STORY_BOOK_RUNS_NEWER,
    STORY_BOOK_RUNS_OLDER,
    STORY_BOOK_DIFFICULTY_CONFIRM,
    STORY_BOOK_DIFFICULTY_CANCEL,
    STORY_BOOK_PAGE_BASE = 200,
    STORY_BOOK_CURSE_BASE = 10000,
    STORY_BOOK_RUN_BASE = 20000,
    STORY_BOOK_MINOR_BASE = 30000,
    STORY_BOOK_MAJOR_BASE = 31000,
    STORY_BOOK_REMOVE_CURSE_BASE = 32000,
    STORY_BOOK_THRESHOLD_BASE = 33000,
    STORY_BOOK_DIFFICULTY_BASE = 34000
};

static void story_book_put_line(int row, int col, int width, byte attr,
    cptr text)
{
    char line[512];

    if (!text || row < 0 || width <= 0)
        return;

    SDL_strlcpy(line, text, sizeof(line));
    for (char *p = line; *p; p++) {
        if (*p == '\n' || *p == '\r')
            *p = ' ';
    }
    metarun_truncate_for_width(line, width);
    Term_putstr(col, row, -1, attr, line);
}

/* Print no more than the available number of proportional-font lines. */
static void story_book_put_wrapped(int *row, int row_limit, int col,
    int width, byte attr, cptr text)
{
    char copy[1024];
    int available;
    int wrap_col;
    int lines;

    if (!row || !text || !text[0] || width <= 0 || *row >= row_limit)
        return;

    available = row_limit - *row;
    wrap_col = col + width;
    SDL_strlcpy(copy, text, sizeof(copy));
    lines = count_wrapped_lines_story(copy, wrap_col, col);

    while (lines > available) {
        char *cut = strrchr(copy, ' ');
        size_t len;

        if (!cut)
            break;
        *cut = '\0';
        len = strlen(copy);
        while (len > 0 && copy[len - 1] == ' ')
            copy[--len] = '\0';
        if (len + 4 < sizeof(copy))
            SDL_strlcat(copy, "...", sizeof(copy));
        lines = count_wrapped_lines_story(copy, wrap_col, col);
        if (lines > available && len > 3)
            copy[len] = '\0'; /* remove the ellipsis before cutting again */
    }

    if (lines > available)
        lines = available;
    story_print_text(*row, col, width, attr, copy);
    *row += lines;
}

static void story_book_draw_header(enum story_book_page page, int term_width)
{
    const char *labels_full[STORY_BOOK_PAGE_MAX] = {
        "1 Statistics", "2 Blessings", "3 Curses", "4 Difficulty",
        "5 Meta-runs"
    };
    const char *labels_short[STORY_BOOK_PAGE_MAX] = {
        "1 Stats", "2 Bless", "3 Curse", "4 Diff", "5 Runs"
    };
    const char *const *labels = (term_width >= 74) ? labels_full : labels_short;
    int col = 1;

    story_book_put_line(0, 2, term_width - 4, TERM_YELLOW,
        "The Chronicle of the Long Defiance");

    for (int i = 0; i < STORY_BOOK_PAGE_MAX; i++) {
        char label[48];
        int width;
        byte attr = (i == (int)page) ? TERM_L_BLUE : TERM_SLATE;

        strnfmt(label, sizeof(label), (i == (int)page) ? "[%s]" : " %s ",
            labels[i]);
        width = (int)strlen(label);
        if (col + width >= term_width)
            break;
        Term_putstr(col, 2, -1, attr, label);
        ui_menu_click_add(STORY_BOOK_PAGE_BASE + i, col, 2, width);
        col += width + 1;
    }
}

static void story_book_draw_footer(enum story_book_page page, int term_width,
    int term_height)
{
    char footer[160];
    int row = term_height - 1;

    if (sdl_touch_only_device_active()) {
        strnfmt(footer, sizeof(footer),
            "[ Previous ]   Page %d of %d   [ Next ]   [ Close ]",
            page + 1, STORY_BOOK_PAGE_MAX);
    } else {
        strnfmt(footer, sizeof(footer),
            "Left/Right turn page   1-5 open page   Esc close   (%d/%d)",
            page + 1, STORY_BOOK_PAGE_MAX);
    }
    story_book_put_line(row, 1, term_width - 2, TERM_L_DARK, footer);

    ui_menu_click_add(STORY_BOOK_PREVIOUS, 0, row, MAX(12, term_width / 4));
    ui_menu_click_add(STORY_BOOK_NEXT, term_width / 2, row,
        MAX(8, term_width / 4));
    ui_menu_click_add(STORY_BOOK_CLOSE, MAX(0, term_width - 14), row, 14);
}

static void story_book_draw_statistics(int term_width, int row_limit,
    bool startup_scene)
{
    char buf[256];
    int row = 4;
    const char *difficulty = "Unknown";
    int win_goal = WINCON_SILMARILS;
    int required_survivors;
    int available;
    u32b threshold = metarun_threshold_value(&metar);

    if (runtype_info && metar.type < z_info->rt_max
        && runtype_info[metar.type].name[0])
    {
        difficulty = runtype_info[metar.type].name;
        if (runtype_info[metar.type].win_con)
            win_goal = runtype_info[metar.type].win_con;
    }
    required_survivors = required_survivor_target(win_goal);
    available = blessing_points_available();

    story_book_put_line(row++, 2, term_width - 4, TERM_YELLOW,
        "Chapter I - The Measure of the Tale");
    strnfmt(buf, sizeof(buf), "Story run %u, undertaken on %s difficulty.",
        (unsigned)metar.id, difficulty);
    story_book_put_line(row++, 4, term_width - 6, TERM_L_BLUE, buf);
    row++;

    strnfmt(buf, sizeof(buf), "Meta-run score: %lu", (unsigned long)metar.score);
    story_book_put_line(row++, 4, term_width - 6, TERM_WHITE, buf);
    strnfmt(buf, sizeof(buf), "Best single run: %lu",
        (unsigned long)get_best_run_score_from_highscores());
    story_book_put_line(row++, 4, term_width - 6, TERM_WHITE, buf);
    strnfmt(buf, sizeof(buf), "Silmarils recovered: %d of %d", metar.silmarils,
        win_goal);
    story_book_put_line(row++, 4, term_width - 6, TERM_L_GREEN, buf);
    strnfmt(buf, sizeof(buf), "Living heroes: %d (the tale requires %d)",
        metar.alive_characters, required_survivors);
    story_book_put_line(row++, 4, term_width - 6,
        (metar.alive_characters < required_survivors) ? TERM_L_RED : TERM_L_GREEN,
        buf);
    strnfmt(buf, sizeof(buf), "Deaths recorded: %d", metar.deaths);
    story_book_put_line(row++, 4, term_width - 6, TERM_L_RED, buf);
    row++;

    strnfmt(buf, sizeof(buf),
        "Blessing points: %d available, %d spent, %d earned.", available,
        metar.blessing_points_spent, metar.blessing_points);
    story_book_put_line(row++, 4, term_width - 6,
        available > 0 ? TERM_L_GREEN : TERM_WHITE, buf);
    strnfmt(buf, sizeof(buf), "Fallen score: %lu; next blessing: %lu of %lu (%s).",
        (unsigned long)metar.fallen_score_total,
        (unsigned long)metar.fallen_score_pool, (unsigned long)threshold,
        threshold_mode_name(metarun_get_threshold_mode(&metar)));
    story_book_put_wrapped(&row, row_limit, 4, term_width - 6, TERM_WHITE, buf);

    if (startup_scene && row < row_limit) {
        row++;
        story_book_put_line(row, 4, term_width - 6, TERM_L_BLUE,
            "[ Begin Blitz Mode ]");
        ui_menu_click_add(STORY_BOOK_BLITZ, 4, row, 24);
    }
}

static void story_book_draw_blessings(int term_width, int row_limit)
{
    char buf[512];
    int row = 4;
    int available = blessing_points_available();
    int minor_count = 0;
    int major_count = 0;

    story_book_put_line(row++, 2, term_width - 4, TERM_YELLOW,
        "Chapter II - Blessings of the West");
    strnfmt(buf, sizeof(buf), "%d blessing point%s remain to be bestowed.",
        available, (available == 1) ? "" : "s");
    story_book_put_line(row++, 4, term_width - 6,
        available > 0 ? TERM_L_GREEN : TERM_WHITE, buf);

    story_book_put_line(row, 4, term_width - 6,
        available > 0 ? TERM_L_BLUE : TERM_L_DARK,
        "[ Add a blessing or remove a curse ]");
    ui_menu_click_add(STORY_BOOK_EXCHANGE, 4, row++,
        MIN(term_width - 6, 40));
    row++;

    story_book_put_line(row++, 4, term_width - 6, TERM_L_BLUE,
        "Minor blessings");
    for (int id = 0; id < z_info->cu_max && row < row_limit; id++) {
        int stacks = CURSE_GET(id);
        if (stacks >= 0)
            continue;
        strnfmt(buf, sizeof(buf), "- %s (strength %d)",
            blessing_display_name(id), -stacks);
        story_book_put_line(row++, 6, term_width - 8, TERM_L_GREEN, buf);
        minor_count++;
    }
    if (!minor_count && row < row_limit)
        story_book_put_line(row++, 6, term_width - 8, TERM_L_DARK,
            "None have yet been granted.");

    if (row < row_limit)
        story_book_put_line(row++, 4, term_width - 6, TERM_L_BLUE,
            "Major blessings");
    for (int i = 0; i < metarun_major_blessing_count() && row < row_limit; i++) {
        if (!metarun_has_major_blessing_index(i))
            continue;
        const char *desc = major_blessing_short_desc(i);
        strnfmt(buf, sizeof(buf), "- %s%s%s", major_blessing_name_str(i),
            (desc && desc[0]) ? ": " : "", (desc && desc[0]) ? desc : "");
        story_book_put_line(row++, 6, term_width - 8, TERM_YELLOW, buf);
        major_count++;
    }
    if (!major_count && row < row_limit)
        story_book_put_line(row++, 6, term_width - 8, TERM_L_DARK,
            "None have yet been unlocked.");
}

static int story_book_known_curse_count(void)
{
    int count = 0;
    for (int id = 0; id < z_info->cu_max; id++) {
        if (CURSE_SEEN(id))
            count++;
    }
    return count;
}

static int story_book_next_known_curse(int selected, int direction)
{
    int limit = MIN(z_info->cu_max, METAR_CURSE_SLOTS);

    if (limit <= 0 || story_book_known_curse_count() == 0)
        return -1;
    for (int step = 1; step <= limit; step++) {
        int id = (selected + direction * step) % limit;
        if (id < 0)
            id += limit;
        if (CURSE_SEEN(id))
            return id;
    }
    return -1;
}

static void story_book_draw_curses(int term_width, int row_limit,
    int *selected_curse)
{
    char buf[1024];
    int known_ids[METAR_CURSE_SLOTS];
    int known_count = 0;
    int selected_pos = 0;
    int row = 4;
    int list_rows;
    int first;

    for (int id = 0; id < z_info->cu_max && id < METAR_CURSE_SLOTS; id++) {
        if (CURSE_SEEN(id))
            known_ids[known_count++] = id;
    }
    if (known_count && (*selected_curse < 0 || !CURSE_SEEN(*selected_curse)))
        *selected_curse = known_ids[0];
    for (int i = 0; i < known_count; i++) {
        if (known_ids[i] == *selected_curse) {
            selected_pos = i;
            break;
        }
    }

    story_book_put_line(row++, 2, term_width - 4, TERM_YELLOW,
        "Chapter III - The Curses Made Known");
    if (!known_count) {
        story_book_put_wrapped(&row, row_limit, 4, term_width - 6,
            TERM_L_DARK,
            "No curse has yet revealed its full nature in this story run.");
        return;
    }

    list_rows = MIN(6, MAX(2, (row_limit - row) / 2));
    first = selected_pos - list_rows / 2;
    if (first < 0) first = 0;
    if (first + list_rows > known_count)
        first = MAX(0, known_count - list_rows);

    for (int i = first; i < known_count && i < first + list_rows; i++) {
        int id = known_ids[i];
        int stacks = CURSE_GET(id);
        byte attr = (id == *selected_curse) ? TERM_YELLOW
            : (stacks > 0 ? TERM_L_RED : TERM_SLATE);
        strnfmt(buf, sizeof(buf), "%c %s%s", (id == *selected_curse) ? '>' : ' ',
            curse_display_name(id), (stacks > 0) ? format(" (active x%d)", stacks) : "");
        story_book_put_line(row, 4, term_width - 6, attr, buf);
        ui_menu_click_add(STORY_BOOK_CURSE_BASE + id, 3, row++, term_width - 5);
    }

    row++;
    if (row >= row_limit || *selected_curse < 0)
        return;

    const curse_type *curse = &cu_info[*selected_curse];
    story_book_put_line(row++, 4, term_width - 6, TERM_L_RED,
        curse_display_name(*selected_curse));
    if (curse->text) {
        story_book_put_wrapped(&row, row_limit, 6, term_width - 8, TERM_SLATE,
            cu_text + curse->text);
    }
    if (curse->power && row < row_limit) {
        strnfmt(buf, sizeof(buf), "Known effect: %s", cu_text + curse->power);
        story_book_put_wrapped(&row, row_limit, 6, term_width - 8,
            TERM_L_RED, buf);
    }
}

static void story_book_draw_difficulty(int term_width, int row_limit)
{
    char buf[256];
    int row = 4;
    const char *difficulty = "Unknown";
    u32b threshold = metarun_threshold_value(&metar);

    if (runtype_info && metar.type < z_info->rt_max
        && runtype_info[metar.type].name[0])
        difficulty = runtype_info[metar.type].name;

    story_book_put_line(row++, 2, term_width - 4, TERM_YELLOW,
        "Chapter IV - The Weight of Doom");
    strnfmt(buf, sizeof(buf), "Current difficulty: %s", difficulty);
    story_book_put_line(row++, 4, term_width - 6, TERM_L_BLUE, buf);
    story_book_put_line(row, 4, term_width - 6, TERM_L_BLUE,
        "[ Change difficulty ]");
    ui_menu_click_add(STORY_BOOK_CHANGE_DIFFICULTY, 4, row++, 26);
    row++;

    strnfmt(buf, sizeof(buf), "Blessing threshold: %s - %lu points",
        threshold_mode_name(metarun_get_threshold_mode(&metar)),
        (unsigned long)threshold);
    story_book_put_line(row++, 4, term_width - 6, TERM_L_GREEN, buf);
    story_book_put_line(row, 4, term_width - 6, TERM_L_BLUE,
        "[ Change blessing threshold ]");
    ui_menu_click_add(STORY_BOOK_THRESHOLD, 4, row++, 34);
    row++;

    story_book_put_line(row++, 4, term_width - 6, TERM_L_BLUE,
        "Thresholds for this difficulty");
    const metarun_blessing_threshold_mode modes[] = {
        METARUN_BLESSING_THRESHOLD_EASIER,
        METARUN_BLESSING_THRESHOLD_NORMAL,
        METARUN_BLESSING_THRESHOLD_HARDER
    };
    for (int i = 0; i < (int)N_ELEMENTS(modes) && row < row_limit; i++) {
        strnfmt(buf, sizeof(buf), "- %s: %lu points per blessing",
            threshold_mode_name(modes[i]),
            (unsigned long)runtype_threshold_for_mode(metar.type, modes[i]));
        story_book_put_line(row++, 6, term_width - 8,
            modes[i] == metarun_get_threshold_mode(&metar) ? TERM_L_GREEN : TERM_WHITE,
            buf);
    }

    if (row < row_limit)
        story_book_put_line(row++, 4, term_width - 6, TERM_L_BLUE,
            "Paths of difficulty");
    for (int i = 0; runtype_info && i < z_info->rt_max && row < row_limit; i++) {
        if (!runtype_info[i].name[0])
            continue;
        strnfmt(buf, sizeof(buf), "%c %s%s", (i == metar.type) ? '>' : '-',
            runtype_info[i].name,
            (i < metar.max_difficulty_reached) ? " (locked)" : "");
        story_book_put_line(row++, 6, term_width - 8,
            (i == metar.type) ? TERM_YELLOW
                : ((i < metar.max_difficulty_reached) ? TERM_L_DARK
                                                       : runtype_info[i].colour),
            buf);
    }
}

static int story_book_compare_metaruns(const void *a, const void *b)
{
    s16b ia = *(const s16b *)a;
    s16b ib = *(const s16b *)b;
    const metarun *ma = &metaruns[ia];
    const metarun *mb = &metaruns[ib];

    if (ma->last_played != mb->last_played)
        return (ma->last_played < mb->last_played) ? 1 : -1;
    if (ma->id != mb->id)
        return (ma->id < mb->id) ? 1 : -1;
    return 0;
}

static void story_book_draw_metaruns(int term_width, int row_limit,
    int *selected_position)
{
    char buf[512];
    s16b *order;
    int row = 4;
    int list_rows;
    int first;

    story_book_put_line(row++, 2, term_width - 4, TERM_YELLOW,
        "Chapter V - The Chronicle of Story Runs");
    if (!metaruns || metarun_max <= 0) {
        story_book_put_line(row, 4, term_width - 6, TERM_L_DARK,
            "No story runs have been recorded.");
        return;
    }

    order = mem_alloc_array(metarun_max, s16b);
    for (s16b i = 0; i < metarun_max; i++) {
        metaruns[i].score = compute_metarun_score(&metaruns[i]);
        order[i] = i;
    }
    qsort(order, metarun_max, sizeof(*order), story_book_compare_metaruns);
    if (*selected_position < 0) *selected_position = 0;
    if (*selected_position >= metarun_max) *selected_position = metarun_max - 1;

    list_rows = MIN(7, MAX(2, (row_limit - row) / 2));
    first = *selected_position - list_rows / 2;
    if (first < 0) first = 0;
    if (first + list_rows > metarun_max)
        first = MAX(0, metarun_max - list_rows);

    for (int pos = first; pos < metarun_max && pos < first + list_rows; pos++) {
        s16b idx = order[pos];
        const metarun *m = &metaruns[idx];
        time_t played = (time_t)m->last_played;
        char date[24] = "unknown date";
        struct tm *when = localtime(&played);
        if (when)
            strftime(date, sizeof(date), "%Y-%m-%d", when);
        strnfmt(buf, sizeof(buf), "%c Story %u - score %lu - %s%s",
            (pos == *selected_position) ? '>' : ' ', (unsigned)m->id,
            (unsigned long)m->score, date, (idx == current_run) ? " (current)" : "");
        story_book_put_line(row, 4, term_width - 6,
            (pos == *selected_position) ? TERM_YELLOW
                : ((idx == current_run) ? TERM_L_BLUE : TERM_WHITE), buf);
        ui_menu_click_add(STORY_BOOK_RUN_BASE + pos, 3, row++, term_width - 5);
    }

    row++;
    if (row < row_limit) {
        s16b idx = order[*selected_position];
        const metarun *m = &metaruns[idx];
        const char *difficulty = "Unknown";
        int win_goal = WINCON_SILMARILS;
        int curses = 0;
        int blessings = 0;
        int majors = 0;
        const char *result = "In progress";

        if (runtype_info && m->type < z_info->rt_max
            && runtype_info[m->type].name[0])
        {
            difficulty = runtype_info[m->type].name;
            if (runtype_info[m->type].win_con)
                win_goal = runtype_info[m->type].win_con;
        }
        if (m->silmarils >= win_goal) result = "Victory";
        else if (m->deaths >= LOSECON_DEATHS) result = "Defeat";
        for (int id = 0; id < METAR_CURSE_SLOTS; id++) {
            if (m->curse_stacks[id] > 0) curses += m->curse_stacks[id];
            else blessings -= m->curse_stacks[id];
        }
        for (int i = 0; i < major_blessing_capacity() && i < 16; i++) {
            if (m->major_blessings & (1U << i)) majors++;
        }

        strnfmt(buf, sizeof(buf), "Story %u - %s", (unsigned)m->id, result);
        story_book_put_line(row++, 4, term_width - 6, TERM_L_BLUE, buf);
        strnfmt(buf, sizeof(buf),
            "%s difficulty; %d Silmarils; %d deaths; best run %lu.",
            difficulty, m->silmarils, m->deaths,
            (unsigned long)m->best_run_score);
        story_book_put_wrapped(&row, row_limit, 6, term_width - 8, TERM_WHITE, buf);
        strnfmt(buf, sizeof(buf),
            "%d curse stack%s, %d minor blessing stack%s, %d major blessing%s.",
            curses, curses == 1 ? "" : "s", blessings,
            blessings == 1 ? "" : "s", majors, majors == 1 ? "" : "s");
        story_book_put_wrapped(&row, row_limit, 6, term_width - 8, TERM_SLATE, buf);
    }
    order = mem_free(order);
}

typedef struct story_book_sdl_state {
    int selected_curse;
    int curse_offset;
    int selected_run;
    int run_offset;
    int pending_difficulty;
} story_book_sdl_state;

static void story_book_sdl_heading(cptr text, bool new_page)
{
    if (new_page)
        sdl_character_sheet_screen_break_book_page();
    sdl_character_sheet_screen_add_book_paragraph_colored(text, TERM_YELLOW);
}

static void story_book_sdl_append(char *buf, size_t size, cptr text)
{
    if (!buf || size == 0 || !text || !text[0])
        return;
    if (buf[0])
        SDL_strlcat(buf, "\n", size);
    SDL_strlcat(buf, text, size);
}

static void story_book_sdl_copy_excerpt(char *out, size_t size, cptr text,
    size_t max_chars)
{
    size_t len;

    if (!out || size == 0)
        return;
    SDL_strlcpy(out, text ? text : "", size);
    len = strlen(out);
    if (len <= max_chars)
        return;
    len = MIN(max_chars, size - 1);
    while (len > 0 && out[len] != ' ')
        len--;
    /* No break space within reach: fall back to a hard cut, but keep it on a
     * UTF-8 sequence boundary so a multibyte symbol is never split. */
    if (len < 8)
        len = (size_t)utf8_safe_prefix_len(out, (int)MIN(max_chars, size - 1));
    out[len] = '\0';
    if (len + 4 < size)
        SDL_strlcat(out, "...", size);
}

static int story_book_sdl_collect_known_curses(int *ids, int capacity)
{
    int count = 0;

    for (int id = 0; id < z_info->cu_max && id < METAR_CURSE_SLOTS; id++) {
        if (!CURSE_SEEN(id))
            continue;
        if (count < capacity)
            ids[count] = id;
        count++;
    }
    return MIN(count, capacity);
}

static int story_book_sdl_difficulty_curse_stacks(int runtype_id,
    int *distinct)
{
    int count = 0;
    int kinds = 0;

    if (runtype_info && runtype_id >= 0 && runtype_id < z_info->rt_max) {
        int limit = MIN(METAR_CURSE_SLOTS, z_info->cu_max);
        for (int id = 0; id < limit; id++) {
            int stacks = runtype_info[runtype_id].curse_stacks[id];
            if (stacks <= 0)
                continue;
            count += stacks;
            kinds++;
        }
    }
    if (distinct)
        *distinct = kinds;
    return count;
}

static void story_book_sdl_difficulty_effects(int runtype_id, char *out,
    size_t size)
{
    int kinds = 0;
    int stacks = story_book_sdl_difficulty_curse_stacks(runtype_id, &kinds);
    int win_goal = WINCON_SILMARILS;
    u32b easier;
    u32b normal;
    u32b harder;

    if (!out || size == 0)
        return;
    out[0] = '\0';
    if (!runtype_info || runtype_id < 0 || runtype_id >= z_info->rt_max)
        return;
    if (runtype_info[runtype_id].win_con)
        win_goal = runtype_info[runtype_id].win_con;
    easier = runtype_threshold_for_mode(runtype_id,
        METARUN_BLESSING_THRESHOLD_EASIER);
    normal = runtype_threshold_for_mode(runtype_id,
        METARUN_BLESSING_THRESHOLD_NORMAL);
    harder = runtype_threshold_for_mode(runtype_id,
        METARUN_BLESSING_THRESHOLD_HARDER);

    if (stacks > 0) {
        strnfmt(out, size,
            "%d starting curse stack%s across %d curse%s. Win at %d "
            "Silmarils. Blessings require %lu/%lu/%lu fallen-score points "
            "on easier/normal/harder thresholds.",
            stacks, stacks == 1 ? "" : "s", kinds, kinds == 1 ? "" : "s",
            win_goal, (unsigned long)easier, (unsigned long)normal,
            (unsigned long)harder);
    } else {
        strnfmt(out, size,
            "No starting curses. Win at %d Silmarils. Blessings require "
            "%lu/%lu/%lu fallen-score points on easier/normal/harder "
            "thresholds.", win_goal, (unsigned long)easier,
            (unsigned long)normal, (unsigned long)harder);
    }
}

static s16b *story_book_sdl_metarun_order(void)
{
    s16b *order;

    if (!metaruns || metarun_max <= 0)
        return NULL;
    order = mem_alloc_array(metarun_max, s16b);
    for (s16b i = 0; i < metarun_max; i++) {
        metaruns[i].score = compute_metarun_score(&metaruns[i]);
        order[i] = i;
    }
    qsort(order, metarun_max, sizeof(*order), story_book_compare_metaruns);
    return order;
}

static bool story_book_sdl_build(bool startup_scene,
    story_book_sdl_state *state, int restore_page)
{
    char buf[1024];
    char line[256];
    const char *difficulty = "Unknown";
    int win_goal = WINCON_SILMARILS;
    int required_survivors;
    int available;
    u32b threshold;

    if (!state || !sdl_character_sheet_screen_begin_book(
            "The Chronicle of the Long Defiance"))
        return false;

    /* Let the reader leave from any page with the mouse (or a touch tap). */
    sdl_character_sheet_screen_set_book_close_button(true);

    sdl_character_sheet_screen_add_book_contents("I. Statistics",
        STORY_BOOK_PAGE_BASE + STORY_BOOK_STATISTICS, STORY_BOOK_STATISTICS);
    sdl_character_sheet_screen_add_book_contents("II. Blessings",
        STORY_BOOK_PAGE_BASE + STORY_BOOK_BLESSINGS, STORY_BOOK_BLESSINGS);
    sdl_character_sheet_screen_add_book_contents("III. Curses",
        STORY_BOOK_PAGE_BASE + STORY_BOOK_CURSES, STORY_BOOK_CURSES);
    sdl_character_sheet_screen_add_book_contents("IV. Difficulty",
        STORY_BOOK_PAGE_BASE + STORY_BOOK_DIFFICULTY, STORY_BOOK_DIFFICULTY);
    sdl_character_sheet_screen_add_book_contents("V. Story Runs",
        STORY_BOOK_PAGE_BASE + STORY_BOOK_METARUNS, STORY_BOOK_METARUNS);

    refresh_current_metar_score();
    compute_blessing_pool();
    metarun_sanitize_major_blessing_bits(&metar);

    if (runtype_info && metar.type < z_info->rt_max
        && runtype_info[metar.type].name[0])
    {
        difficulty = runtype_info[metar.type].name;
        if (runtype_info[metar.type].win_con)
            win_goal = runtype_info[metar.type].win_con;
    }
    required_survivors = required_survivor_target(win_goal);
    available = blessing_points_available();
    threshold = metarun_threshold_value(&metar);

    /* Page I: statistics.  The tale is told in the warm tones of the Jar of
     * Light that stands beside it -- tan for the framing, gold for glory and
     * the Silmarils, cream for the count of the living and the dead, and amber
     * (the colour of the held light) for the road to the next blessing. */
    story_book_sdl_heading("Chapter I - The Measure of the Tale", false);

    strnfmt(line, sizeof(line), "Story run %u on %s difficulty.",
        (unsigned)metar.id, difficulty);
    sdl_character_sheet_screen_add_book_paragraph_colored(line, TERM_L_UMBER);

    strnfmt(line, sizeof(line),
        "Meta-run score: %lu  (best single run: %lu).   Silmarils: %d of %d.",
        (unsigned long)metar.score,
        (unsigned long)get_best_run_score_from_highscores(),
        metar.silmarils, win_goal);
    sdl_character_sheet_screen_add_book_paragraph_colored(line, TERM_YELLOW);

    strnfmt(line, sizeof(line), "Living heroes: %d (need %d).   Deaths: %d.",
        metar.alive_characters, required_survivors, metar.deaths);
    sdl_character_sheet_screen_add_book_paragraph_colored(line, TERM_L_WHITE);

    strnfmt(line, sizeof(line),
        "Blessing points: %d available, %d spent, %d earned.",
        available, metar.blessing_points_spent, metar.blessing_points);
    sdl_character_sheet_screen_add_book_paragraph_colored(line, TERM_L_UMBER);

    strnfmt(line, sizeof(line),
        "Fallen score: %lu.   Next blessing: %lu of %lu (%s).",
        (unsigned long)metar.fallen_score_total,
        (unsigned long)metar.fallen_score_pool, (unsigned long)threshold,
        threshold_mode_name(metarun_get_threshold_mode(&metar)));
    sdl_character_sheet_screen_add_book_paragraph_colored(line, TERM_ORANGE);

    sdl_character_sheet_screen_set_book_lamp(metar.fallen_score_pool,
        threshold, STORY_BOOK_STATISTICS);
    if (startup_scene)
        sdl_character_sheet_screen_add_book_action_colored("Begin Blitz Mode",
            STORY_BOOK_BLITZ, TERM_L_BLUE);

    /* Page II: blessings and the exchange. */
    story_book_sdl_heading("Chapter II - Blessings of the West", true);
    buf[0] = '\0';
    strnfmt(line, sizeof(line), "%d blessing point%s remain to be bestowed.",
        available, available == 1 ? "" : "s");
    story_book_sdl_append(buf, sizeof(buf), line);
    char minor_line[512] = "Minor blessings: ";
    int minor_count = 0;
    for (int id = 0; id < z_info->cu_max; id++) {
        int stacks = CURSE_GET(id);
        if (stacks >= 0)
            continue;
        if (minor_count > 0)
            SDL_strlcat(minor_line, ", ", sizeof(minor_line));
        SDL_strlcat(minor_line, blessing_display_name(id), sizeof(minor_line));
        minor_count++;
        if (minor_count >= 3)
            break;
    }
    if (!minor_count)
        SDL_strlcat(minor_line, "none", sizeof(minor_line));
    story_book_sdl_append(buf, sizeof(buf), minor_line);
    char major_line[512] = "Major blessings: ";
    int major_count = 0;
    for (int i = 0; i < metarun_major_blessing_count(); i++) {
        if (!metarun_has_major_blessing_index(i))
            continue;
        if (major_count > 0)
            SDL_strlcat(major_line, ", ", sizeof(major_line));
        SDL_strlcat(major_line, major_blessing_name_str(i), sizeof(major_line));
        major_count++;
        if (major_count >= 3)
            break;
    }
    if (!major_count)
        SDL_strlcat(major_line, "none", sizeof(major_line));
    story_book_sdl_append(buf, sizeof(buf), major_line);
    sdl_character_sheet_screen_add_book_paragraph_colored(buf, TERM_L_GREEN);

    if (available > 0) {
        int minor_choices[3];
        int minor_choices_count = metarun_inline_minor_blessing_choices(
            minor_choices);

        for (int i = 0; i < minor_choices_count; i++) {
            int id = minor_choices[i];
            strnfmt(line, sizeof(line), "Receive %s (cost 1)",
                blessing_display_name(id));
            sdl_character_sheet_screen_add_book_action_colored(line,
                STORY_BOOK_MINOR_BASE + id, TERM_L_GREEN);
        }
        int shown_curses = 0;
        for (int id = 0; id < z_info->cu_max && shown_curses < 3; id++) {
            int stacks = CURSE_CURSE_STACK(id);
            if (stacks <= 0)
                continue;
            strnfmt(line, sizeof(line), "Lift one stack of %s (cost 1)",
                curse_display_name(id));
            sdl_character_sheet_screen_add_book_action_colored(line,
                STORY_BOOK_REMOVE_CURSE_BASE + id, TERM_L_RED);
            shown_curses++;
        }
        int shown_major = 0;
        for (int i = 0; i < major_blessing_capacity() && shown_major < 3; i++) {
            int cost;
            if (!major_blessing_def(i) || metarun_has_major_blessing_index(i))
                continue;
            cost = major_blessing_cost(i);
            strnfmt(line, sizeof(line), "Seal %s (cost %d)",
                major_blessing_name_str(i), cost);
            if (cost <= available) {
                sdl_character_sheet_screen_add_book_action_colored(line,
                    STORY_BOOK_MAJOR_BASE + i, TERM_YELLOW);
            } else {
                sdl_character_sheet_screen_add_book_paragraph_colored(line,
                    TERM_L_DARK);
            }
            shown_major++;
        }
    } else {
        sdl_character_sheet_screen_add_book_paragraph_colored(
            "Earn another blessing point to alter the gifts of this tale.",
            TERM_L_DARK);
    }

    /* Page III: known curses, with a bounded live list and full selected lore. */
    story_book_sdl_heading("Chapter III - The Curses Made Known", true);
    int curse_ids[METAR_CURSE_SLOTS];
    int curse_count = story_book_sdl_collect_known_curses(curse_ids,
        N_ELEMENTS(curse_ids));
    if (curse_count <= 0) {
        state->selected_curse = -1;
        state->curse_offset = 0;
        sdl_character_sheet_screen_add_book_paragraph(
            "No curse has yet revealed its full nature in this story run.");
    } else {
        int selected_pos = 0;

        if (state->curse_offset < 0) state->curse_offset = 0;
        if (state->curse_offset >= curse_count)
            state->curse_offset = MAX(0, curse_count - 5);
        for (int i = 0; i < curse_count; i++) {
            if (curse_ids[i] == state->selected_curse) {
                selected_pos = i;
                break;
            }
        }
        if (state->selected_curse < 0 || !CURSE_SEEN(state->selected_curse)) {
            state->selected_curse = curse_ids[state->curse_offset];
            selected_pos = state->curse_offset;
        }
        if (selected_pos < state->curse_offset
            || selected_pos >= state->curse_offset + 5)
            state->curse_offset = (selected_pos / 5) * 5;

        int end = MIN(curse_count, state->curse_offset + 5);
        for (int i = state->curse_offset; i < end; i++) {
            int id = curse_ids[i];
            int stacks = CURSE_GET(id);
            char suffix[32] = "";
            if (stacks > 0)
                strnfmt(suffix, sizeof(suffix), " (active x%d)", stacks);
            strnfmt(line, sizeof(line), "%c %s%s",
                id == state->selected_curse ? '>' : '-',
                curse_display_name(id), suffix);
            sdl_character_sheet_screen_add_book_action_colored(line,
                STORY_BOOK_CURSE_BASE + id,
                stacks > 0 ? TERM_L_RED : TERM_SLATE);
        }

        const curse_type *curse = &cu_info[state->selected_curse];
        char desc[384];
        char power[384];
        story_book_sdl_copy_excerpt(desc, sizeof(desc),
            curse->text ? cu_text + curse->text : "No description recorded.",
            300);
        story_book_sdl_copy_excerpt(power, sizeof(power),
            curse->power ? cu_text + curse->power : "Unknown", 260);
        strnfmt(buf, sizeof(buf), "%s\n%s",
            curse_display_name(state->selected_curse), desc);
        sdl_character_sheet_screen_add_book_paragraph_colored(buf, TERM_SLATE);
        strnfmt(buf, sizeof(buf), "Known effect: %s", power);
        sdl_character_sheet_screen_add_book_paragraph_colored(buf, TERM_L_RED);
        if (state->curse_offset > 0)
            sdl_character_sheet_screen_add_book_action_colored(
                "Earlier known curses", STORY_BOOK_CURSES_EARLIER,
                TERM_L_BLUE);
        if (end < curse_count)
            sdl_character_sheet_screen_add_book_action_colored(
                "Later known curses", STORY_BOOK_CURSES_LATER,
                TERM_L_BLUE);
    }

    /* Page IV: difficulty changes are previewed and confirmed on this page. */
    story_book_sdl_heading("Chapter IV - The Weight of Doom", true);
    if (state->pending_difficulty >= 0 && runtype_info
        && state->pending_difficulty < z_info->rt_max
        && runtype_info[state->pending_difficulty].name[0])
    {
        int pending = state->pending_difficulty;

        strnfmt(buf, sizeof(buf), "Change %s to %s?", difficulty,
            runtype_info[pending].name);
        sdl_character_sheet_screen_add_book_paragraph_colored(buf,
            runtype_info[pending].colour);
        story_book_sdl_difficulty_effects(pending, buf, sizeof(buf));
        sdl_character_sheet_screen_add_book_paragraph_colored(buf, TERM_WHITE);
        sdl_character_sheet_screen_add_book_paragraph_colored(
            "WARNING: This difficulty increase is permanent for this story "
            "run. You cannot return to the current difficulty after "
            "confirming.", TERM_L_RED);
        sdl_character_sheet_screen_add_book_action_colored(
            "Confirm permanent difficulty change",
            STORY_BOOK_DIFFICULTY_CONFIRM, TERM_L_RED);
        sdl_character_sheet_screen_add_book_action_colored(
            "Cancel - keep current difficulty", STORY_BOOK_DIFFICULTY_CANCEL,
            TERM_L_GREEN);
    } else {
        const metarun_blessing_threshold_mode modes[] = {
            METARUN_BLESSING_THRESHOLD_EASIER,
            METARUN_BLESSING_THRESHOLD_NORMAL,
            METARUN_BLESSING_THRESHOLD_HARDER
        };

        strnfmt(buf, sizeof(buf),
            "Current: %s. Blessing threshold: %s - %lu points.", difficulty,
            threshold_mode_name(metarun_get_threshold_mode(&metar)),
            (unsigned long)threshold);
        sdl_character_sheet_screen_add_book_paragraph_colored(buf,
            TERM_L_BLUE);
        for (int i = 0; i < (int)N_ELEMENTS(modes); i++) {
            byte attr = modes[i] == METARUN_BLESSING_THRESHOLD_EASIER
                ? TERM_L_GREEN
                : (modes[i] == METARUN_BLESSING_THRESHOLD_HARDER
                    ? TERM_ORANGE : TERM_WHITE);
            strnfmt(line, sizeof(line), "%s threshold - %lu points",
                threshold_mode_name(modes[i]),
                (unsigned long)runtype_threshold_for_mode(metar.type,
                    modes[i]));
            sdl_character_sheet_screen_add_book_action_colored(line,
                STORY_BOOK_THRESHOLD_BASE + modes[i], attr);
        }
        sdl_character_sheet_screen_add_book_paragraph_colored(
            "Difficulty levels (select one to review its effects):",
            TERM_SLATE);
        for (int i = 0; runtype_info && i < z_info->rt_max; i++) {
            int stacks;
            byte attr;

            if (!runtype_info[i].name[0])
                continue;
            stacks = story_book_sdl_difficulty_curse_stacks(i, NULL);
            strnfmt(line, sizeof(line), "%c %s - %s", i == metar.type ? '>' : '-',
                runtype_info[i].name,
                stacks > 0 ? format("%d starting curse stack%s", stacks,
                    stacks == 1 ? "" : "s") : "no starting curses");
            attr = i == metar.type ? TERM_YELLOW
                : (i < metar.max_difficulty_reached ? TERM_L_DARK
                                                     : runtype_info[i].colour);
            if (i != metar.type && i >= metar.max_difficulty_reached) {
                sdl_character_sheet_screen_add_book_action_colored(line,
                    STORY_BOOK_DIFFICULTY_BASE + i, attr);
            } else {
                if (i < metar.max_difficulty_reached)
                    SDL_strlcat(line, " (locked)", sizeof(line));
                sdl_character_sheet_screen_add_book_paragraph_colored(line,
                    attr);
            }
        }
    }

    /* Page V: click a run to replace the detail paragraph in place. */
    story_book_sdl_heading("Chapter V - The Chronicle of Story Runs", true);
    s16b *order = story_book_sdl_metarun_order();
    if (!order) {
        state->selected_run = 0;
        state->run_offset = 0;
        sdl_character_sheet_screen_add_book_paragraph(
            "No story runs have been recorded.");
    } else {
        if (state->selected_run < 0) state->selected_run = 0;
        if (state->selected_run >= metarun_max)
            state->selected_run = metarun_max - 1;
        if (state->run_offset < 0) state->run_offset = 0;
        if (state->run_offset >= metarun_max)
            state->run_offset = MAX(0, metarun_max - 5);
        if (state->selected_run < state->run_offset
            || state->selected_run >= state->run_offset + 5)
            state->run_offset = (state->selected_run / 5) * 5;

        int end = MIN(metarun_max, state->run_offset + 5);
        for (int pos = state->run_offset; pos < end; pos++) {
            s16b idx = order[pos];
            const metarun *m = &metaruns[idx];
            time_t played = (time_t)m->last_played;
            char date[24] = "unknown date";
            struct tm *when = localtime(&played);
            if (when)
                strftime(date, sizeof(date), "%Y-%m-%d", when);
            strnfmt(line, sizeof(line), "%c Story %u - score %lu - %s%s",
                pos == state->selected_run ? '>' : '-', (unsigned)m->id,
                (unsigned long)m->score, date,
                idx == current_run ? " (current)" : "");
            sdl_character_sheet_screen_add_book_action_colored(line,
                STORY_BOOK_RUN_BASE + pos,
                idx == current_run ? TERM_YELLOW : TERM_WHITE);
        }

        s16b idx = order[state->selected_run];
        const metarun *m = &metaruns[idx];
        const char *run_difficulty = "Unknown";
        int run_win_goal = WINCON_SILMARILS;
        int curses = 0;
        int blessings = 0;
        int majors = 0;
        const char *result = "In progress";
        if (runtype_info && m->type < z_info->rt_max
            && runtype_info[m->type].name[0])
        {
            run_difficulty = runtype_info[m->type].name;
            if (runtype_info[m->type].win_con)
                run_win_goal = runtype_info[m->type].win_con;
        }
        if (m->silmarils >= run_win_goal) result = "Victory";
        else if (m->deaths >= LOSECON_DEATHS) result = "Defeat";
        for (int id = 0; id < METAR_CURSE_SLOTS; id++) {
            if (m->curse_stacks[id] > 0) curses += m->curse_stacks[id];
            else blessings -= m->curse_stacks[id];
        }
        for (int i = 0; i < major_blessing_capacity() && i < 16; i++) {
            if (m->major_blessings & (1U << i)) majors++;
        }
        strnfmt(buf, sizeof(buf),
            "Story %u - %s.\n%s difficulty; %d Silmarils; %d deaths; best run %lu.\n"
            "%d curse stacks, %d minor blessing stacks, %d major blessings.",
            (unsigned)m->id, result, run_difficulty, m->silmarils, m->deaths,
            (unsigned long)m->best_run_score, curses, blessings, majors);
        sdl_character_sheet_screen_add_book_paragraph_colored(buf, TERM_SLATE);
        if (state->run_offset > 0)
            sdl_character_sheet_screen_add_book_action_colored(
                "Newer story runs", STORY_BOOK_RUNS_NEWER, TERM_L_BLUE);
        if (end < metarun_max)
            sdl_character_sheet_screen_add_book_action_colored(
                "Older story runs", STORY_BOOK_RUNS_OLDER, TERM_L_BLUE);
        order = mem_free(order);
    }

    sdl_character_sheet_screen_commit_book();
    sdl_character_sheet_screen_set_book_page(restore_page);
    return true;
}

static bool story_book_show_sdl(bool startup_scene)
{
    story_book_sdl_state state = { -1, 0, 0, 0, -1 };
    bool done = false;
    bool launch_blitz = false;

    if (!startup_scene)
        screen_save();
    screen_push_supporting_panes_hidden();

    if (!story_book_sdl_build(startup_scene, &state, 0)) {
        screen_pop_supporting_panes_hidden();
        if (!startup_scene)
            screen_load();
        return false;
    }

    while (!done) {
        int key;
        int clicked = 0;
        int click_action = UI_MENU_CLICK_PRIMARY;
        int page;
        int page_count;

        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        key = metarun_inkey_hidden();

        if (ui_menu_click_take_action(&clicked, &click_action)) {
            ui_menu_click_clear();
            if (click_action == UI_MENU_CLICK_HOVER)
                continue;

            /* The on-screen Close button (mouse/touch) leaves the book. */
            if (clicked == SDL_SELECT_CLICK_CLOSE) {
                done = true;
                continue;
            }

            page = sdl_character_sheet_screen_select_page();
            page_count = sdl_character_sheet_screen_select_page_count();
            if (clicked == SDL_SELECT_CLICK_PAGE_PREV) {
                if (page > 0 && !sdl_character_sheet_screen_page_turning())
                    sdl_character_sheet_screen_begin_page_turn(-1);
                continue;
            }
            if (clicked == SDL_SELECT_CLICK_PAGE_NEXT) {
                if (page < page_count - 1
                    && !sdl_character_sheet_screen_page_turning())
                    sdl_character_sheet_screen_begin_page_turn(+1);
                else if (page >= page_count - 1)
                    done = true;
                continue;
            }

            if (clicked >= STORY_BOOK_PAGE_BASE
                && clicked < STORY_BOOK_PAGE_BASE + STORY_BOOK_PAGE_MAX)
            {
                int target = clicked - STORY_BOOK_PAGE_BASE;
                if (target >= 0 && target < page_count && target != page
                    && !sdl_character_sheet_screen_page_turning())
                    sdl_character_sheet_screen_begin_page_turn_to(target);
                continue;
            }

            if (clicked >= STORY_BOOK_CURSE_BASE
                && clicked < STORY_BOOK_CURSE_BASE + METAR_CURSE_SLOTS)
            {
                state.selected_curse = clicked - STORY_BOOK_CURSE_BASE;
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked >= STORY_BOOK_RUN_BASE
                && clicked < STORY_BOOK_RUN_BASE + metarun_max)
            {
                state.selected_run = clicked - STORY_BOOK_RUN_BASE;
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }

            if (clicked >= STORY_BOOK_MINOR_BASE
                && clicked < STORY_BOOK_MINOR_BASE + METAR_CURSE_SLOTS)
            {
                (void)metarun_inline_choose_minor_blessing(
                    clicked - STORY_BOOK_MINOR_BASE);
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked >= STORY_BOOK_MAJOR_BASE
                && clicked < STORY_BOOK_MAJOR_BASE + major_blessing_capacity())
            {
                (void)metarun_inline_choose_major_blessing(
                    clicked - STORY_BOOK_MAJOR_BASE);
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked >= STORY_BOOK_REMOVE_CURSE_BASE
                && clicked < STORY_BOOK_REMOVE_CURSE_BASE + METAR_CURSE_SLOTS)
            {
                (void)metarun_inline_remove_curse(
                    clicked - STORY_BOOK_REMOVE_CURSE_BASE);
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked >= STORY_BOOK_THRESHOLD_BASE
                && clicked < STORY_BOOK_THRESHOLD_BASE
                    + METARUN_BLESSING_THRESHOLD_MODE_MAX)
            {
                metarun_blessing_threshold_mode mode =
                    (metarun_blessing_threshold_mode)(clicked
                        - STORY_BOOK_THRESHOLD_BASE);
                metarun_set_threshold_mode(&metar, mode);
                update_blessing_ledger(&metar);
                if (!sync_current_metarun_slot(false))
                    log_warn("Inline threshold change failed to sync metarun");
                save_metaruns();
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked >= STORY_BOOK_DIFFICULTY_BASE
                && clicked < STORY_BOOK_DIFFICULTY_BASE + z_info->rt_max)
            {
                state.pending_difficulty = clicked
                    - STORY_BOOK_DIFFICULTY_BASE;
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked == STORY_BOOK_DIFFICULTY_CONFIRM) {
                if (state.pending_difficulty >= 0)
                    (void)metarun_set_difficulty_inline(
                        state.pending_difficulty);
                state.pending_difficulty = -1;
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked == STORY_BOOK_DIFFICULTY_CANCEL) {
                state.pending_difficulty = -1;
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked == STORY_BOOK_CURSES_EARLIER) {
                state.curse_offset = MAX(0, state.curse_offset - 5);
                state.selected_curse = -1;
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked == STORY_BOOK_CURSES_LATER) {
                state.curse_offset += 5;
                state.selected_curse = -1;
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked == STORY_BOOK_RUNS_NEWER) {
                state.run_offset = MAX(0, state.run_offset - 5);
                state.selected_run = state.run_offset;
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked == STORY_BOOK_RUNS_OLDER) {
                state.run_offset += 5;
                state.selected_run = state.run_offset;
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked == STORY_BOOK_BLITZ) {
                launch_blitz = true;
                done = true;
                continue;
            }
        } else if (key == UI_MENU_CLICK_WAKE_KEY) {
            ui_menu_click_clear();
            continue;
        }
        ui_menu_click_clear();

        if (steamdeck_controls_active())
            key = steamdeck_menu_key(key, '4', '6');
        if (sdl_character_sheet_screen_page_turning())
            continue;
        page = sdl_character_sheet_screen_select_page();
        page_count = sdl_character_sheet_screen_select_page_count();
        if (key == ESCAPE || key == 'q' || key == 'Q')
            done = true;
        else if (key == '4' && page > 0)
            sdl_character_sheet_screen_begin_page_turn(-1);
        else if (key == '6' || key == ' ' || key == '\r' || key == '\n') {
            if (page < page_count - 1)
                sdl_character_sheet_screen_begin_page_turn(+1);
            else
                done = true;
        }
    }

    ui_menu_click_clear();
    sdl_character_sheet_screen_hide();
    screen_pop_supporting_panes_hidden();
    if (!startup_scene)
        screen_load();
    if (launch_blitz) {
        run_mode_set_pending(RUN_MODE_BLITZ);
        run_mode_set_current(RUN_MODE_BLITZ);
    }
    return true;
}

static void story_book_run_existing_menu(bool startup_scene, void (*fn)(void),
    story_font_term_state *font_state)
{
    story_font_term_pop(font_state);
    metarun_run_substats_menu(startup_scene, fn);
    story_font_term_push_slot(true, false, STORY_FONT_SLOT_SECONDARY,
        font_state);
}

void print_metarun_stats(void)
{
    bool startup_scene;
    bool done = false;
    bool launch_blitz = false;
    enum story_book_page page = STORY_BOOK_STATISTICS;
    int selected_curse = -1;
    int selected_run = 0;
    story_font_term_state font_state;

    refresh_current_metar_score();
    if (current_run < 0 || current_run >= metarun_max) {
        screen_save();
        Term_clear();
        Term_putstr(2, 5, -1, TERM_RED, "No story-run data is available.");
        Term_putstr(2, 7, -1, TERM_L_DARK, "Press any key to return.");
        metarun_wait_hidden();
        screen_load();
        return;
    }

    startup_scene = (!character_generated || !p_ptr || !p_ptr->playing);
    if (story_book_show_sdl(startup_scene))
        return;

    /* Non-SDL fallback: retain the terminal book below. */
    if (!startup_scene) {
        screen_save();
        sdl_push_terminal_menu_scale();
    }
    screen_push_supporting_panes_hidden();
    screen_push_touch_pane_hidden();
    story_font_term_push_slot(true, false, STORY_FONT_SLOT_SECONDARY,
        &font_state);

    while (!done) {
        int term_width = 80;
        int term_height = 24;
        int row_limit;
        int action = 0;
        int click_action = UI_MENU_CLICK_PRIMARY;
        char key;

        refresh_current_metar_score();
        compute_blessing_pool();
        metarun_sanitize_major_blessing_bits(&metar);
        Term_get_size(&term_width, &term_height);
        row_limit = MAX(5, term_height - 2);

        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        story_book_draw_header(page, term_width);
        switch (page) {
        case STORY_BOOK_STATISTICS:
            story_book_draw_statistics(term_width, row_limit, startup_scene);
            break;
        case STORY_BOOK_BLESSINGS:
            story_book_draw_blessings(term_width, row_limit);
            break;
        case STORY_BOOK_CURSES:
            story_book_draw_curses(term_width, row_limit, &selected_curse);
            break;
        case STORY_BOOK_DIFFICULTY:
            story_book_draw_difficulty(term_width, row_limit);
            break;
        case STORY_BOOK_METARUNS:
            story_book_draw_metaruns(term_width, row_limit, &selected_run);
            break;
        default:
            break;
        }
        story_book_draw_footer(page, term_width, term_height);
        Term_fresh();

        key = metarun_inkey_hidden();
        if (ui_menu_click_take_action(&action, &click_action)) {
            if (click_action == UI_MENU_CLICK_HOVER) {
                if (action >= STORY_BOOK_CURSE_BASE
                    && action < STORY_BOOK_CURSE_BASE + METAR_CURSE_SLOTS)
                {
                    selected_curse = action - STORY_BOOK_CURSE_BASE;
                } else if (action >= STORY_BOOK_RUN_BASE
                    && action < STORY_BOOK_RUN_BASE + metarun_max)
                {
                    selected_run = action - STORY_BOOK_RUN_BASE;
                } else {
                    ui_menu_click_clear();
                    continue;
                }
                ui_menu_click_clear();
                continue;
            }
        } else if (key == UI_MENU_CLICK_WAKE_KEY) {
            ui_menu_click_clear();
            continue;
        }
        ui_menu_click_clear();

        if (action >= STORY_BOOK_PAGE_BASE
            && action < STORY_BOOK_PAGE_BASE + STORY_BOOK_PAGE_MAX)
        {
            page = (enum story_book_page)(action - STORY_BOOK_PAGE_BASE);
            continue;
        }
        if (action >= STORY_BOOK_CURSE_BASE
            && action < STORY_BOOK_CURSE_BASE + METAR_CURSE_SLOTS)
        {
            selected_curse = action - STORY_BOOK_CURSE_BASE;
            continue;
        }
        if (action >= STORY_BOOK_RUN_BASE
            && action < STORY_BOOK_RUN_BASE + metarun_max)
        {
            selected_run = action - STORY_BOOK_RUN_BASE;
            continue;
        }
        if (action == STORY_BOOK_PREVIOUS) key = '4';
        else if (action == STORY_BOOK_NEXT) key = '6';
        else if (action == STORY_BOOK_CLOSE) key = ESCAPE;
        else if (action == STORY_BOOK_EXCHANGE) key = 'b';
        else if (action == STORY_BOOK_THRESHOLD) key = 't';
        else if (action == STORY_BOOK_CHANGE_DIFFICULTY) key = 'd';
        else if (action == STORY_BOOK_BLITZ) key = 'x';

        if (key >= '1' && key <= '5') {
            page = (enum story_book_page)(key - '1');
        } else if (key == ESCAPE
            || (steamdeck_controls_active() && key == steamdeck_back_key())) {
            done = true;
        } else if (key == '4' || key == 'h' || key == 'H') {
            page = (enum story_book_page)((page + STORY_BOOK_PAGE_MAX - 1)
                % STORY_BOOK_PAGE_MAX);
        } else if (key == '6' || key == 'l' || key == 'L') {
            page = (enum story_book_page)((page + 1) % STORY_BOOK_PAGE_MAX);
        } else if (page == STORY_BOOK_BLESSINGS
            && (key == 'b' || key == 'B' || key == '\r' || key == '\n'))
        {
            story_book_run_existing_menu(startup_scene,
                open_blessing_exchange, &font_state);
        } else if (page == STORY_BOOK_DIFFICULTY
            && (key == 't' || key == 'T'))
        {
            story_book_run_existing_menu(startup_scene,
                adjust_blessing_threshold_menu, &font_state);
        } else if (page == STORY_BOOK_DIFFICULTY
            && (key == 'd' || key == 'D' || key == '\r' || key == '\n'))
        {
            story_book_run_existing_menu(startup_scene,
                choose_difficulty_menu, &font_state);
        } else if (page == STORY_BOOK_STATISTICS && startup_scene
            && (key == 'x' || key == 'X'))
        {
            launch_blitz = true;
            done = true;
        } else if (page == STORY_BOOK_CURSES
            && (key == '8' || key == 'k' || key == 'K' || key == '-'))
        {
            selected_curse = story_book_next_known_curse(selected_curse, -1);
        } else if (page == STORY_BOOK_CURSES
            && (key == '2' || key == 'j' || key == 'J' || key == '+'))
        {
            selected_curse = story_book_next_known_curse(selected_curse, 1);
        } else if (page == STORY_BOOK_METARUNS
            && (key == '8' || key == 'k' || key == 'K' || key == '-'))
        {
            selected_run = MAX(0, selected_run - 1);
        } else if (page == STORY_BOOK_METARUNS
            && (key == '2' || key == 'j' || key == 'J' || key == '+'))
        {
            selected_run = MIN(metarun_max - 1, selected_run + 1);
        }
    }

    ui_menu_click_clear();
    story_font_term_pop(&font_state);
    screen_pop_touch_pane_hidden();
    screen_pop_supporting_panes_hidden();
    if (!startup_scene) {
        sdl_pop_terminal_menu_scale();
        screen_load();
    }
    if (launch_blitz) {
        run_mode_set_pending(RUN_MODE_BLITZ);
        run_mode_set_current(RUN_MODE_BLITZ);
    }
}
