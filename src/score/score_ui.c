#include "score/score_ui.h"

#include "angband.h"
#include "externs.h"
#include "fs/path.h"
#include "log/log.h"
#include "score/score_io.h"
#include "score/score_logic.h"
#include "score/score_runs.h"
#include "sdl-config.h"
#include "metarun.h"

#include <SDL3/SDL.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Helper to build score/meta file path correctly for both portable and normal builds */
static bool build_meta_path(char* buf, size_t len, const char* filename)
{
#ifdef SIL_USE_LOCAL_DATA
    /* Portable build: in apex directory */
    return path_build(buf, len, ANGBAND_DIR_APEX, filename);
#else
    /* Normal build: in meta directory (parent of metaruns) */
    if (ANGBAND_DIR_METARUN && *ANGBAND_DIR_METARUN) {
        char meta_dir[1024];
        SDL_strlcpy(meta_dir, ANGBAND_DIR_METARUN, sizeof(meta_dir));
        char* last_sep = strrchr(meta_dir, PATH_SEP[0]);
        if (last_sep) *last_sep = '\0';
        return path_build(buf, len, meta_dir, filename);
    } else {
        return path_build(buf, len, ANGBAND_DIR_APEX, filename);
    }
#endif
}

#define RUN_HISTORY_MAX       256
#define RUN_HISTORY_ROWS       15

typedef struct run_history_entry {
    score_record_v1 record;
    s64b detail_offset;
    int rating;
} run_history_entry;

typedef enum run_detail_panel {
    RUN_PANEL_GENERAL = 0,
    RUN_PANEL_STATS,
    RUN_PANEL_ABILITIES,
    RUN_PANEL_MILESTONES,
    RUN_PANEL_ARTEFACTS,
    RUN_PANEL_MONSTERS,
    RUN_PANEL_COUNT
} run_detail_panel;

typedef enum run_monster_sort_mode {
    RUN_MON_SORT_APPEARANCE = 0,
    RUN_MON_SORT_DEPTH,
    RUN_MON_SORT_COUNT
} run_monster_sort_mode;

typedef struct run_detail_list_state {
    int top;
    int highlight;
} run_detail_list_state;

typedef struct run_detail_view_state {
    int general_top;
    int stats_top;
    run_detail_list_state abilities;
    run_detail_list_state milestones;
    run_detail_list_state artefacts;
    run_detail_list_state monsters;
    run_monster_sort_mode monster_sort_mode;
} run_detail_view_state;

typedef struct run_detail_text_view {
    int first_row;
    int visible_rows;
    int scroll_top;
    int logical_row;
    int term_wid;
} run_detail_text_view;

static void run_history_show_detail(const run_history_entry* entry);
static int collect_run_history(run_history_entry* out, int capacity);
static bool show_run_history_detail_for_score(const high_score* score);
static bool run_history_prepare_artefact_object(
    const score_run_artefact_v1* entry, object_type* out);

static void run_history_refresh_active_run(void)
{
    if (!character_generated || !p_ptr || p_ptr->is_dead)
        return;

    high_score preview;
    if (!build_live_preview_score(&preview))
        return;

    time_t now = time(NULL);
    if (now == (time_t)-1)
        now = 0;

    if (!score_runs_record_current_run(&preview, now, SCORE_RECORD_ALIVE)) {
        log_warn("run_history: unable to refresh live snapshot before viewing");
    }
}

static bool run_history_is_current(const run_history_entry* entry)
{
    if (!entry)
        return false;
    if (!character_generated || !p_ptr || p_ptr->is_dead)
        return false;
    if (entry->record.status != SCORE_RECORD_ALIVE)
        return false;
    return (entry->record.metarun_id == metar.id);
}

static high_score forced_highlight_entry;
static bool forced_highlight_active = false;
static bool force_interactive_scores = false;
static bool score_last_layout_short = true;

static void score_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

typedef enum
{
    SCORE_VIEW_ORDER_SCORE = 0,
    SCORE_VIEW_ORDER_CHRONOLOGY = 1
} score_view_order;

typedef enum
{
    RUN_HISTORY_SORT_DATE = 0,
    RUN_HISTORY_SORT_RATING = 1
} run_history_sort_order;

static const char* run_history_sort_label(run_history_sort_order order)
{
    return (order == RUN_HISTORY_SORT_RATING) ? "Rating" : "Date";
}

static void score_ui_get_term_size(int* wid, int* hgt)
{
    int local_wid = 80;
    int local_hgt = 24;

    if (Term)
        Term_get_size(&local_wid, &local_hgt);

    if (local_wid < 1)
        local_wid = 80;
    if (local_hgt < 1)
        local_hgt = 24;

    if (wid)
        *wid = local_wid;
    if (hgt)
        *hgt = local_hgt;
}

static bool score_ui_compact_width(int term_wid)
{
    return (term_wid < 70);
}

static void score_ui_put_fit(byte attr, cptr text, int row, int col, int term_wid)
{
    int max = term_wid - col;

    if (!text || row < 0 || col < 0 || max <= 0)
        return;

    Term_putstr(col, row, max, attr, text);
}

static char run_history_status_short(score_record_status status)
{
    switch (status) {
    case SCORE_RECORD_ALIVE:
        return 'A';
    case SCORE_RECORD_DEAD:
        return 'D';
    case SCORE_RECORD_ESCAPED:
        return 'E';
    default:
        return '?';
    }
}

static void run_history_build_high_score(const score_record_v1* rec, high_score* out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!rec)
        return;

    strnfmt(out->what, sizeof(out->what), "%s", VERSION_STRING);
    strnfmt(out->pts, sizeof(out->pts), "%4d", rec->net_curses);
    strnfmt(out->turns, sizeof(out->turns), "%9lu", (unsigned long)rec->turns_spent);

    if (rec->completed_utc) {
        time_t ts = (time_t)rec->completed_utc;
        struct tm* tm_info = localtime(&ts);
        if (tm_info)
            strftime(out->day, sizeof(out->day), "@%Y%m%d", tm_info);
    }

    const char* who = rec->player_name[0] ? rec->player_name :
        (rec->savefile_hint[0] ? rec->savefile_hint : "<unknown>");
    strnfmt(out->who, sizeof(out->who), "%-.15s", who);

    strnfmt(out->p_r, sizeof(out->p_r), "%02u", (unsigned)rec->race_id);
    strnfmt(out->p_h, sizeof(out->p_h), "%02u", (unsigned)rec->character_id);
    strnfmt(out->cur_dun, sizeof(out->cur_dun), "%3u", (unsigned)rec->exit_depth);
    strnfmt(out->max_dun, sizeof(out->max_dun), "%3u", (unsigned)rec->max_depth);
    strnfmt(out->cur_lev, sizeof(out->cur_lev), "%3u", (unsigned)rec->uniques_killed);

    const char* how = (rec->status == SCORE_RECORD_ALIVE)
        ? "(alive and well)"
        : (rec->cause_of_death[0] ? rec->cause_of_death : "(unknown)");
    strnfmt(out->how, sizeof(out->how), "%-.49s", how);

    int sils = (rec->silmarils > 9) ? 9 : (int)rec->silmarils;
    strnfmt(out->silmarils, sizeof(out->silmarils), "%1d", sils);
    out->morgoth_slain[0] = (rec->run_flags & SCORE_RUN_FLAG_MORGOTH_SLAIN) ? 't' : 'f';
    out->escaped[0] = (rec->run_flags & SCORE_RUN_FLAG_ANGBAND_ESCAPED) ? 't' : 'f';
}

static int run_history_compute_rating(const score_record_v1* rec)
{
    if (!rec)
        return 0;
    high_score temp;
    run_history_build_high_score(rec, &temp);
    return score_points(&temp);
}

static int compare_scores_qsort(const void* va, const void* vb)
{
    const high_score* a = (const high_score*)va;
    const high_score* b = (const high_score*)vb;
    return score_compare(a, b);
}

static long score_day_key(const high_score* entry)
{
    if (!entry)
        return LONG_MIN;

    if (streq(entry->how, "(alive and well)"))
        return LONG_MAX;

    if (entry->day[0] != '@')
        return LONG_MIN + 1;

    char buf[32];
    SDL_strlcpy(buf, entry->day + 1, sizeof(buf));
    char* end = NULL;
    long value = strtol(buf, &end, 10);
    if (value <= 0 || !end || *end != '\0')
        return LONG_MIN + 1;

    return value;
}

static int compare_scores_chronological(const void* va, const void* vb)
{
    const high_score* a = (const high_score*)va;
    const high_score* b = (const high_score*)vb;

    long day_a = score_day_key(a);
    long day_b = score_day_key(b);
    if (day_a != day_b)
        return (day_a > day_b) ? -1 : 1;

    int cmp = strcmp(a->who, b->who);
    if (cmp != 0)
        return cmp;

    cmp = strcmp(a->how, b->how);
    if (cmp != 0)
        return cmp;

    return score_compare(a, b);
}

static int deduplicate_scores_by_name(high_score* entries, int count)
{
    if (count <= 1)
        return count;

    high_score unique[MAX_HISCORES + 1];
    int unique_scores[MAX_HISCORES + 1];
    int unique_count = 0;

    for (int i = 0; i < count; i++)
    {
        int pts = score_points(&entries[i]);
        bool merged = false;

        for (int j = 0; j < unique_count; j++)
        {
            if (streq(entries[i].who, unique[j].who))
            {
                if (pts > unique_scores[j]
                    || (pts == unique_scores[j] && strcmp(entries[i].day, unique[j].day) > 0)
                    || (pts == unique_scores[j] && streq(entries[i].day, unique[j].day)
                        && strcmp(entries[i].how, unique[j].how) > 0))
                {
                    unique[j] = entries[i];
                    unique_scores[j] = pts;
                }
                merged = true;
                break;
            }
        }

        if (!merged)
        {
            unique[unique_count] = entries[i];
            unique_scores[unique_count] = pts;
            unique_count++;
        }
    }

    for (int i = 0; i < unique_count; i++)
    {
        entries[i] = unique[i];
    }

    return unique_count;
}

static const char* score_view_order_label(score_view_order order)
{
    return (order == SCORE_VIEW_ORDER_CHRONOLOGY)
        ? "Date (newest first)"
        : "Score (highest first)";
}
static bool score_identity_matches(const high_score* a, const high_score* b)
{
    if (!a || !b)
        return false;
    return streq(a->who, b->who)
        && streq(a->day, b->day)
        && streq(a->how, b->how);
}
static int find_score_index(const high_score* entries, int count, const high_score* target)
{
    if (!target)
        return -1;
    for (int i = 0; i < count; i++)
    {
        if (score_identity_matches(&entries[i], target))
            return i;
    }
    return -1;
}
static void set_forced_highlight_entry(const high_score* entry)
{
    if (entry) {
        forced_highlight_entry = *entry;
        forced_highlight_active = true;
    } else {
        forced_highlight_active = false;
    }
}
static byte score_entry_color(const high_score* entry, bool highlight)
{
    if (highlight) return TERM_YELLOW;

    if (!entry) return TERM_SLATE;

    if (streq(entry->how, "(alive and well)"))
        return TERM_L_GREEN;

    if (entry->escaped[0] == 't')
        return TERM_GREEN;

    if (entry->morgoth_slain[0] == 't')
        return TERM_L_RED;

    int sil = atoi(entry->silmarils);
    if (sil > 0)
        return TERM_ORANGE;

    int depth = atoi(entry->max_dun);
    if (depth >= 10)
        return TERM_WHITE;
    if (depth >= 5)
        return TERM_L_WHITE;

    return TERM_SLATE;
}
static void truncate_preserving_words(const char* src, char* dst, size_t dst_size, int max_width)
{
    if (!dst || dst_size == 0)
        return;
    if (!src)
        src = "";
    if (max_width <= 0)
    {
        dst[0] = '\0';
        return;
    }

    size_t limit = dst_size - 1;
    if ((size_t)max_width > limit)
        max_width = (int)limit;

    int len = (int)strlen(src);
    if (len <= max_width)
    {
        strnfmt(dst, dst_size, "%s", src);
        return;
    }

    if (max_width <= 3)
    {
        int fill = MIN(max_width, (int)limit);
        for (int i = 0; i < fill; i++) dst[i] = '.';
        dst[fill] = '\0';
        return;
    }

    int cut = max_width - 3;
    int candidate = cut;
    while (candidate > 0 && !isspace((unsigned char)src[candidate - 1]))
        candidate--;
    if (candidate >= 3)
        cut = candidate;

    char head[64];
    strnfmt(head, sizeof(head), "%.*s", cut, src);
    int head_len = (int)strlen(head);
    while (head_len > 0 && isspace((unsigned char)head[head_len - 1]))
        head[--head_len] = '\0';

    strnfmt(dst, dst_size, "%s...", head);
}
static void truncate_with_ellipsis(const char* src, char* dst, size_t dst_size,
                                   int max_width)
{
    if (!dst || dst_size == 0)
        return;
    if (!src)
        src = "";
    if (max_width <= 0) {
        dst[0] = '\0';
        return;
    }

    size_t limit = dst_size - 1;
    if ((size_t)max_width > limit)
        max_width = (int)limit;

    int len = (int)strlen(src);
    if (len <= max_width) {
        strnfmt(dst, dst_size, "%s", src);
        return;
    }

    if (max_width <= 3) {
        int fill = MIN(max_width, (int)limit);
        for (int i = 0; i < fill; i++)
            dst[i] = '.';
        dst[fill] = '\0';
        return;
    }

    strnfmt(dst, dst_size, "%.*s...", max_width - 3, src);
}

static void score_ui_choose_fit(char* out, size_t out_len, int term_wid,
                                int col, const char** candidates, int count)
{
    int max_width = term_wid - col;

    if (!out || out_len == 0)
        return;

    out[0] = '\0';

    if (!candidates || count <= 0 || max_width <= 0)
        return;

    for (int i = 0; i < count; i++)
    {
        if (!candidates[i])
            continue;
        if ((int)strlen(candidates[i]) <= max_width)
        {
            SDL_strlcpy(out, candidates[i], out_len);
            return;
        }
    }

    truncate_with_ellipsis(candidates[count - 1], out, out_len, max_width);
}

static void score_ui_build_halls_footer(char* footer, size_t footer_len,
                                        bool steamdeck, int term_wid,
                                        int page, int total_pages,
                                        const char* open_label,
                                        const char* order_label,
                                        const char* layout_label,
                                        const char* exit_label)
{
    char full[192];
    char medium[160];
    char short1[128];
    char short2[96];
    char tiny[64];
    char minimum[32];
    const char* candidates[6];
    int count = 0;

    if (!footer || footer_len == 0)
        return;

    footer[0] = '\0';

    if (steamdeck)
    {
        strnfmt(full, sizeof(full),
            "[%s] Open  [%s] Order  [%s] Layout  [%s] Exit  D-pad Move  N/P Page %d/%d",
            open_label, order_label, layout_label, exit_label, page + 1,
            total_pages);
        strnfmt(medium, sizeof(medium),
            "[%s] Open  [%s] Order  [%s] Layout  [%s] Exit  D-pad  %d/%d",
            open_label, order_label, layout_label, exit_label, page + 1,
            total_pages);
        strnfmt(short1, sizeof(short1),
            "[%s] Open  [%s] Ord  [%s] Lay  [%s] Exit  %d/%d",
            open_label, order_label, layout_label, exit_label, page + 1,
            total_pages);
        strnfmt(short2, sizeof(short2), "[%s] Open  [%s] Exit  %d/%d",
            open_label, exit_label, page + 1, total_pages);
        strnfmt(tiny, sizeof(tiny), "[%s] Open  %d/%d", open_label, page + 1,
            total_pages);
        strnfmt(minimum, sizeof(minimum), "%d/%d", page + 1, total_pages);
    }
    else
    {
        strnfmt(full, sizeof(full),
            "[Enter/Right] Open  [S] Order  [L] Layout  [Esc] Exit  [Up/Down] Move  [N] Next  [P] Prev %d/%d",
            page + 1, total_pages);
        strnfmt(medium, sizeof(medium),
            "Enter Open  S Order  L Layout  Esc Exit  Up/Down Move  N/P Page %d/%d",
            page + 1, total_pages);
        strnfmt(short1, sizeof(short1),
            "Enter Open  S Order  L Layout  Esc Exit  Up/Down  N/P %d/%d",
            page + 1, total_pages);
        strnfmt(short2, sizeof(short2),
            "Enter Open  S Order  L Layout  Esc Exit  %d/%d",
            page + 1, total_pages);
        strnfmt(tiny, sizeof(tiny), "Enter Open  Esc Exit  %d/%d", page + 1,
            total_pages);
        strnfmt(minimum, sizeof(minimum), "%d/%d", page + 1, total_pages);
    }

    candidates[count++] = full;
    candidates[count++] = medium;
    candidates[count++] = short1;
    candidates[count++] = short2;
    candidates[count++] = tiny;
    candidates[count++] = minimum;

    score_ui_choose_fit(footer, footer_len, term_wid, 1, candidates, count);
}

static int run_history_detail_body_rows(int term_hgt, int first_row)
{
    int footer_row = term_hgt - 2;
    int last_content_row = footer_row - 1;
    int rows = last_content_row - first_row + 1;

    if (rows < 1)
        rows = 1;

    return rows;
}

static void run_history_clamp_scroll(int* top, int rows, int total_lines)
{
    int max_top;

    if (!top)
        return;

    if (rows < 1)
        rows = 1;
    if (total_lines < 0)
        total_lines = 0;

    max_top = total_lines - rows;
    if (max_top < 0)
        max_top = 0;

    if (*top < 0)
        *top = 0;
    if (*top > max_top)
        *top = max_top;
}

static bool run_history_handle_scroll_key(int* top, int ch, int rows,
                                          int total_lines)
{
    int delta = 0;

    if (!top)
        return false;

    if (rows < 1)
        rows = 1;

    switch (ch) {
    case '2':
    case 'j':
    case 'J':
#ifdef ARROW_DOWN
    case ARROW_DOWN:
#endif
        delta = 1;
        break;
    case '8':
    case 'k':
    case 'K':
#ifdef ARROW_UP
    case ARROW_UP:
#endif
        delta = -1;
        break;
    case '3':
    case 'n':
    case 'N':
        delta = rows;
        break;
    case '-':
    case '7':
    case 'p':
    case 'P':
        delta = -rows;
        break;
    default:
        return false;
    }

    *top += delta;
    run_history_clamp_scroll(top, rows, total_lines);
    return true;
}

static void run_detail_text_view_init(run_detail_text_view* view, int first_row,
                                      int visible_rows, int scroll_top,
                                      int term_wid)
{
    if (!view)
        return;

    view->first_row = first_row;
    view->visible_rows = (visible_rows < 1) ? 1 : visible_rows;
    view->scroll_top = (scroll_top < 0) ? 0 : scroll_top;
    view->logical_row = 0;
    view->term_wid = (term_wid < 1) ? 80 : term_wid;
}

static void run_detail_text_view_put(run_detail_text_view* view, byte attr,
                                     const char* text)
{
    int relative_row;

    if (!view)
        return;

    relative_row = view->logical_row - view->scroll_top;
    if (relative_row >= 0 && relative_row < view->visible_rows) {
        Term_putstr(0, view->first_row + relative_row, view->term_wid, attr,
            text ? text : "");
    }

    view->logical_row++;
}

static int run_detail_text_view_row_y(const run_detail_text_view* view)
{
    int relative_row;

    if (!view)
        return -1;

    relative_row = view->logical_row - view->scroll_top;
    if (relative_row < 0 || relative_row >= view->visible_rows)
        return -1;

    return view->first_row + relative_row;
}

static void run_detail_text_view_advance(run_detail_text_view* view)
{
    if (!view)
        return;
    view->logical_row++;
}

static void run_detail_text_view_blank(run_detail_text_view* view)
{
    if (!view)
        return;
    view->logical_row++;
}
static void display_single_score_short(byte attr, int place, int row, const high_score* entry)
{
    char depth_commas[16];
    char verdict_buf[96];
    const char* verdict;
    int wid, hgt;

    /* Get actual terminal width */
    score_ui_get_term_size(&wid, &hgt);
    const int line_width = wid;

    int depth_ft = atoi(entry->cur_dun) * 50;
    comma_number(depth_commas, depth_ft);

    int pts = score_points(entry);
    int silmarils = parse_score_int(entry->silmarils, sizeof(entry->silmarils), 0);
    bool morgoth = (entry->morgoth_slain[0] == 't');

    /* Build indicators string */
    char indicators[8] = "";
    int ind_pos = 0;
    
    /* Add Silmaril indicators */
    for (int i = 0; i < silmarils && i < 3; i++) {
        indicators[ind_pos++] = '*';
    }
    
    /* Add Morgoth indicator */
    if (morgoth) {
        indicators[ind_pos++] = 'V';
    }
    indicators[ind_pos] = '\0';

    /* Build verdict with appropriate formatting */
    if (entry->escaped[0] == 't') {
        if (indicators[0]) {
            strnfmt(verdict_buf, sizeof(verdict_buf), "Escaped with %s", indicators);
        } else {
            strnfmt(verdict_buf, sizeof(verdict_buf), "Escaped Angband");
        }
        verdict = verdict_buf;
    } else if (streq(entry->how, "(alive and well)")) {
        verdict = "Alive";
    } else if (morgoth) {
        /* Morgoth victory is a special end state; avoid "Slain by ..." wording. */
        if (indicators[0]) {
            strnfmt(verdict_buf, sizeof(verdict_buf),
                    "Victorious over Morgoth's illusion (%s) at %sft %s",
                    entry->how, depth_commas, indicators);
        } else {
            strnfmt(verdict_buf, sizeof(verdict_buf),
                    "Victorious over Morgoth's illusion (%s) at %sft",
                    entry->how, depth_commas);
        }
        verdict = verdict_buf;
    } else {
        /* For deaths, include depth and indicators - keep ft visible */
        if (indicators[0]) {
            strnfmt(verdict_buf, sizeof(verdict_buf), "Slain by %s at %sft %s", 
                    entry->how, depth_commas, indicators);
        } else {
            strnfmt(verdict_buf, sizeof(verdict_buf), "Slain by %s at %sft", 
                    entry->how, depth_commas);
        }
        verdict = verdict_buf;
    }

    const char* name_src = entry->who[0] ? entry->who : "(unknown)";

    /* Column layout with maximum verdict display:
     * "1. Maedhros   777  Slain by a Young fire-drake at 800ft with indicators"
     *  ^^^ ^^^^^^^^ ^^^  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
     *  Pl  Name(12) Scr  Verdict (uses all remaining terminal width)
     */
    const int place_width = 4;      /* "1. " */
    const int name_width = 15;      /* Fixed minimum name column */
    const int score_width = 5;      /* Right-aligned score */
    const int gap = 2;              /* Spaces between score and verdict */
    
    /* Verdict gets all remaining space on the line, minus 1 for cleaner right margin */
    int verdict_start = place_width + name_width + score_width + gap;
    int verdict_width = line_width - verdict_start - 1;  /* -1 for right margin */
    if (verdict_width < 1) verdict_width = 1;

    /* Build the line */
    char line[256];
    for (size_t i = 0; i < sizeof(line); i++) line[i] = ' ';
    
    int pos = 0;
    
    /* Place number: "1. " */
    char place_buf[8];
    strnfmt(place_buf, sizeof(place_buf), "%2d. ", place);
    memcpy(line + pos, place_buf, strlen(place_buf));
    pos = place_width;  /* Jump to fixed position */
    
    /* Name field: left-aligned in 20-char column */
    char name_field[64];
    truncate_preserving_words(name_src, name_field, sizeof(name_field), name_width);
    int name_len = (int)strlen(name_field);
    if (name_len > name_width) name_len = name_width;
    memcpy(line + pos, name_field, name_len);
    pos = place_width + name_width;  /* Jump to fixed position */
    
    /* Score field: right-aligned in 5-char column */
    char score_buf[16];
    strnfmt(score_buf, sizeof(score_buf), "%d", pts);
    int score_len = (int)strlen(score_buf);
    if (score_len > score_width) {
        /* Truncate from left if too long */
        memcpy(line + pos + score_width - score_len, score_buf + (score_len - score_width), score_width);
    } else {
        memcpy(line + pos + score_width - score_len, score_buf, score_len);
    }
    pos = place_width + name_width + score_width + gap;  /* Jump past score + gap */
    
    /* Verdict field: keep "at XXft" visible at end if truncating needed */
    const char* verdict_str = verdict;
    int verdict_len = (int)strlen(verdict_str);
    
    if (verdict_len > verdict_width) {
        /* Find the " at " part which contains the depth info - keep it visible */
        const char* at_pos = strstr(verdict_str, " at ");
        if (at_pos) {
            int at_offset = (int)(at_pos - verdict_str);
            int tail_len = verdict_len - at_offset;  /* Length from " at " onward */
            
            if (tail_len < verdict_width) {
                /* We can fit the tail, so truncate the beginning */
                int prefix_len = verdict_width - tail_len;
                memcpy(line + pos, verdict_str, prefix_len);
                memcpy(line + pos + prefix_len, at_pos, tail_len);
                pos += verdict_width;
            } else {
                /* Even the tail is too long, just show what fits starting from beginning */
                memcpy(line + pos, verdict_str, verdict_width);
                pos += verdict_width;
            }
        } else {
            /* No " at " found, just show beginning of verdict */
            memcpy(line + pos, verdict_str, verdict_width);
            pos += verdict_width;
        }
    } else {
        /* Verdict fits completely */
        memcpy(line + pos, verdict_str, verdict_len);
        pos += verdict_len;
    }
    
    line[pos] = '\0';

    c_put_str(attr, line, 3 + row, 0);
}
extern void display_single_score(
    byte attr, int row, int col, int place, int fake, high_score* the_score)
{
    int ph;
    int aged, depth;

    cptr user, when;

    char out_val[160];
    char tmp_val[160];

    char aged_commas[15];
    char depth_commas[15];

    /* Extract the race/character */
    ph = atoi(the_score->p_h);

    /* Hack -- extract the turns and such */
    for (user = the_score->uid; isspace((unsigned char)*user);
         user++) /* loop */
        ;
    for (when = the_score->day; isspace((unsigned char)*when);
         when++) /* loop */
        ;

    aged = atoi(the_score->turns);
    depth = atoi(the_score->cur_dun) * 50;

    comma_number(aged_commas, aged);
    comma_number(depth_commas, depth);

    /* Clean up standard encoded form of "when" */
    if ((*when == '@') && strlen(when) == 9)
    {
        char month[4];

        sprintf(month, "%.2s", when + 5);
        atomonth(atoi(month), month);

        if (*(when + 7) == '0')
            sprintf(tmp_val, "%.1s %.3s %.4s", when + 8, month, when + 1);
        else
            sprintf(tmp_val, "%.2s %.3s %.4s", when + 7, month, when + 1);

        when = tmp_val;
    }

    /* if not displayed in a place, then don't write the place number */
    /* show the score as human-readable commas, e.g. "123 456"            */
    char score_commas[16];
    int calculated_score = score_points(the_score);
    
    log_debug("display_single_score: '%s' calculated_score=%d", the_score->who, calculated_score);
    log_debug("  pts field raw: '%.*s'", (int)sizeof(the_score->pts), the_score->pts);

    const score_file_ctx* active_ctx = score_file_active_ctx();
    byte ver_major = active_ctx ? active_ctx->version_major : 0;
    byte ver_minor = active_ctx ? active_ctx->version_minor : 0;
    byte ver_patch = active_ctx ? active_ctx->version_patch : 0;
    byte ver_extra = active_ctx ? active_ctx->version_extra : 0;

    log_debug("  version: %d.%d.%d.%d (has_curses=%s)",
              ver_major, ver_minor, ver_patch, ver_extra,
              scores_version_has_curses(score_file_global_ctx()) ? "yes" : "no");
    
    comma_number(score_commas, calculated_score);

    /* Build curse/blessing text if applicable */
    char curse_text[32] = "";
    byte curse_color = TERM_WHITE;
    if (scores_version_has_curses(score_file_global_ctx()))
    {
        int curses = parse_score_int(the_score->pts, sizeof(the_score->pts), 0);
        log_debug("display_single_score: Building curse display for '%s', curses=%d", the_score->who, curses);
        
        if (curses > 0)
        {
            strnfmt(curse_text, sizeof(curse_text), " (%d curse%s)", curses, (curses == 1) ? "" : "s");
            curse_color = TERM_L_RED;
            log_debug("  curse_text='%s', color=%d", curse_text, curse_color);
        }
        else if (curses < 0)
        {
            strnfmt(curse_text, sizeof(curse_text), " (%d blessing%s)", -curses, (curses == -1) ? "" : "s");
            curse_color = TERM_L_GREEN;
            log_debug("  curse_text='%s', color=%d", curse_text, curse_color);
        }
        else
        {
            log_debug("  curses=0, not displaying");
        }
    }

    /* Build a fixed-width prefix so the name column is aligned for all entries */
    {
        char prefix[32];
        if (the_score->escaped[0] == 't')
        {
            if (place == 0)
                strnfmt(prefix, sizeof(prefix), "     escaped  ");
            else
                strnfmt(prefix, sizeof(prefix), "%3d. escaped  ", place);
        }
        else
        {
            if (place == 0)
                strnfmt(prefix, sizeof(prefix), "     %5s ft  ", depth_commas);
            else
                strnfmt(prefix, sizeof(prefix), "%3d. %5s ft  ", place, depth_commas);
        }

        /* Pad the prefix to a fixed width (15 chars) to guarantee the name column */
        while ((int)strlen(prefix) < 15)
        {
            SDL_strlcat(prefix, " ", sizeof(prefix));
        }
        /* Truncate if somehow longer */
        prefix[15] = '\0';

        /* Now build the line with the fixed prefix */
        strnfmt(out_val, sizeof(out_val), "%s%s%s  [%s pts]",
                prefix, the_score->who, c_name + c_info[ph].alt_name, score_commas);
    }

    /* Add curse text to string (we'll display it in color later by finding it) */
    size_t pre_curse_len = strlen(out_val);
    if (curse_text[0] != '\0')
    {
        SDL_strlcat(out_val, curse_text, sizeof(out_val));
    }

    /* Possibly ammend the first line */
    if (the_score->morgoth_slain[0] == 't')
    {
        SDL_strlcat(out_val, ", hailed as the Slayer of Morgoth's shadow",
            sizeof(out_val));
    }
    else
    {
        if (the_score->silmarils[0] == '1')
        {
            SDL_strlcat(out_val, ", who freed a Silmaril", sizeof(out_val));
        }
        if (the_score->silmarils[0] == '2')
        {
            SDL_strlcat(out_val, ", who freed two Silmarils", sizeof(out_val));
        }
        if (the_score->silmarils[0] == '3')
        {
            SDL_strlcat(
                out_val, ", who freed all three Silmarils", sizeof(out_val));
        }
        if (the_score->silmarils[0] > '3')
        {
            SDL_strlcat(out_val, ", who freed suspiciously many Silmarils",
                sizeof(out_val));
        }
    }

    /* Dump the first line */
    c_put_str(attr, out_val, row + 3, col);

    /* Overlay curse/blessing count in color at the position we added it */
    if (curse_text[0] != '\0')
    {
        int curse_col = col + pre_curse_len;
        log_debug("  Displaying curse_text='%s' at row=%d, col=%d", 
                  curse_text, row + 3, curse_col);
        c_put_str(curse_color, curse_text, row + 3, curse_col);
    }
    else
    {
        log_debug("  curse_text is empty, not displaying");
    }

    /* Prepare the second line for escapees */
    if (the_score->escaped[0] == 't')
    {
        strnfmt(
            out_val, sizeof(out_val), "               Escaped the iron hells");

        if ((the_score->morgoth_slain[0] == 't')
            || (the_score->silmarils[0] > '0'))
        {
            SDL_strlcat(out_val, " and brought back the light of Valinor",
                sizeof(out_val));
        }
        else
        {
            SDL_strlcat(out_val, " empty-handed", sizeof(out_val));
        }
    }

    /* "Alive" entry: either the synthetic/fake score or a real one whose
       cause-of-death text is literally "(alive and well)"                */
    else if (fake || streq(the_score->how, "(alive and well)"))
    {
        strnfmt(out_val, sizeof(out_val),
            "               Lives still, deep within Angband's vaults");
    }

    /* Prepare the second line for those slain */
    else if (the_score->morgoth_slain[0] == 't')
    {
        strnfmt(out_val, sizeof(out_val),
            "               Victorious over Morgoth's illusion (%s)",
            the_score->how);
    }
    else
    {
        strnfmt(out_val, sizeof(out_val), "               Slain by %s",
            the_score->how);

        /* Mark those with a silmaril */
        if (the_score->silmarils[0] > '0')
        {
            SDL_strlcat(out_val, " during a daring escape", sizeof(out_val));
        }
    }

    /* Dump the info */
    c_put_str(attr, out_val, row + 4, col);

    /* Don't print date for living characters */
    if (fake)
    {
        strnfmt(out_val, sizeof(out_val), "               after %s turns.",
            aged_commas);
        c_put_str(attr, out_val, row + 5, col);
    }
    else
    {
        strnfmt(out_val, sizeof(out_val),
            "               after %s turns.  (%s)", aged_commas, when);
        c_put_str(attr, out_val, row + 5, col);
    }

    /* Print symbols for silmarils / slaying Morgoth */
    if (the_score->silmarils[0] == '1')
    {
        c_put_str(attr, "         *", row + 5, col);
    }
    if (the_score->silmarils[0] == '2')
    {
        c_put_str(attr, "        * *", row + 5, col);
    }
    if (the_score->silmarils[0] > '2')
    {
        c_put_str(attr, "       * * *", row + 5, col);
    }
    if (the_score->morgoth_slain[0] == 't')
    {
        c_put_str(TERM_L_DARK, "         V", row + 4, col);
    }
}
static char display_scores_pages(const high_score* entries, int count,
                                 int* highlight_index, score_view_order order,
                                 bool detailed, int page_size)
{
    enum {
        SCORE_CLICK_ORDER = -1,
        SCORE_CLICK_LAYOUT = -2,
        SCORE_CLICK_EXIT = -3,
        SCORE_CLICK_OPEN = -4,
        SCORE_CLICK_PREV = -5,
        SCORE_CLICK_NEXT = -6
    };
    bool steamdeck = steamdeck_controls_active();
    char order_label[16] = "";
    char layout_label[16] = "";
    char exit_label[16] = "";
    char open_label[16] = "";
    int term_wid = 80;
    int term_hgt = 24;
    int footer_row;
    bool compact;
    int start_index = 0;

    if (steamdeck) {
        /* Steam Deck UI: Y=order, X=layout, B=exit, A=open */
        score_prompt_label(steamdeck_secondary_key(), "Y", order_label, sizeof(order_label));
        score_prompt_label(steamdeck_alt_action_key(), "X", layout_label, sizeof(layout_label));
        score_prompt_label(steamdeck_back_key(), "B", exit_label, sizeof(exit_label));
        score_prompt_label(steamdeck_confirm_key(), "A", open_label, sizeof(open_label));
    }

    score_ui_get_term_size(&term_wid, &term_hgt);
    footer_row = term_hgt - 1;
    if (footer_row < 4)
        footer_row = 4;
    compact = score_ui_compact_width(term_wid);

    Term_clear();

    if (!entries || count <= 0)
    {
        c_put_str(TERM_L_BLUE, "               Halls of Mandos", 1, 0);
        c_put_str(TERM_SLATE, "No recorded heroes yet.", 3, 0);
        if (steamdeck) {
            char hint_buf[48];
            strnfmt(hint_buf, sizeof(hint_buf), "(press %s)", exit_label);
            score_ui_put_fit(TERM_L_WHITE, hint_buf, footer_row, 2, term_wid);
        } else {
            score_ui_put_fit(TERM_L_WHITE, "(press any key)", footer_row, 2,
                term_wid);
        }
        ui_menu_click_begin();
        ui_menu_click_set_outside_cancel_enabled(true);
        ui_menu_click_add_full_row(SCORE_CLICK_EXIT, footer_row);
        while (true)
        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;
            bool saved_hide_cursor = hide_cursor;
            hide_cursor = true;
            (void)inkey();
            hide_cursor = saved_hide_cursor;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                break;
            }

            ui_menu_click_clear();
            break;
        }
        ui_menu_click_clear();
        return 0;
    }

    if (!highlight_index)
        return 0;
    if (*highlight_index < 0 || *highlight_index >= count)
        *highlight_index = 0;

    while (true)
    {
        int body_rows;
        int entries_per_page;
        int layout_col;
        int max_start;
        int page;
        int total_pages;
        bool has_more;
        bool has_prev;

        score_ui_get_term_size(&term_wid, &term_hgt);
        footer_row = term_hgt - 1;
        if (footer_row < 4)
            footer_row = 4;
        compact = score_ui_compact_width(term_wid);

        body_rows = footer_row - 3;
        if (body_rows < 1)
            body_rows = 1;

        entries_per_page = detailed ? (body_rows / 4) : body_rows;
        if (detailed && entries_per_page > page_size)
            entries_per_page = page_size;
        if (entries_per_page < 1) entries_per_page = 1;

        max_start = ((count - 1) / entries_per_page) * entries_per_page;
        if (max_start < 0)
            max_start = 0;
        if (start_index > max_start)
            start_index = max_start;
        if (*highlight_index < start_index)
            start_index = (*highlight_index / entries_per_page) * entries_per_page;
        if (*highlight_index >= start_index + entries_per_page)
            start_index = (*highlight_index / entries_per_page) * entries_per_page;
        if (start_index > max_start)
            start_index = max_start;

        page = (entries_per_page > 0) ? (start_index / entries_per_page) : 0;
        total_pages = (count + entries_per_page - 1) / entries_per_page;
        if (total_pages < 1)
            total_pages = 1;
        has_more = (start_index + entries_per_page < count);
        has_prev = (start_index > 0);

        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_outside_cancel_enabled(true);
        c_put_str(TERM_L_BLUE, "               Halls of Mandos", 1, 0);

        char order_buf[64];
        strnfmt(order_buf, sizeof(order_buf), "%s", score_view_order_label(order));
        c_put_str(TERM_L_WHITE, order_buf, 2, 0);

        char layout_buf[32];
        strnfmt(layout_buf, sizeof(layout_buf), "Layout: %s", detailed ? "Full" : "Short");
        layout_col = term_wid - (int)strlen(layout_buf) - 1;
        if (!compact && layout_col > (int)strlen(order_buf) + 2)
            c_put_str(TERM_SLATE, layout_buf, 2, layout_col);

        for (int row = 0; row < entries_per_page && (start_index + row) < count; row++)
        {
            int idx = start_index + row;
            bool is_highlight = (idx == *highlight_index);
            byte attr = score_entry_color(&entries[idx], is_highlight);

            if (detailed)
            {
                display_single_score(attr, row * 4, 0, start_index + row + 1, false, (high_score*)&entries[idx]);
                ui_menu_click_add_full_row(idx, row * 4 + 3);
                ui_menu_click_add_full_row(idx, row * 4 + 4);
                ui_menu_click_add_full_row(idx, row * 4 + 5);
            }
            else
            {
                display_single_score_short(attr, start_index + row + 1, row, &entries[idx]);
                ui_menu_click_add_full_row(idx, row + 3);
            }
        }

        char footer[192];
        score_ui_build_halls_footer(footer, sizeof(footer), steamdeck,
            term_wid, page, total_pages, open_label, order_label,
            layout_label, exit_label);
        score_ui_put_fit(TERM_L_WHITE, footer, footer_row, 1, term_wid);
        ui_menu_click_add_text_token(SCORE_CLICK_OPEN, 1, footer_row, footer,
            "Open");
        ui_menu_click_add_text_token(SCORE_CLICK_OPEN, 1, footer_row, footer,
            "open");
        ui_menu_click_add_text_token(SCORE_CLICK_OPEN, 1, footer_row, footer,
            "Enter");
        ui_menu_click_add_text_token(SCORE_CLICK_ORDER, 1, footer_row, footer,
            "Order");
        ui_menu_click_add_text_token(SCORE_CLICK_ORDER, 1, footer_row, footer,
            "order");
        ui_menu_click_add_text_token(SCORE_CLICK_ORDER, 1, footer_row, footer,
            "Ord");
        ui_menu_click_add_text_token(SCORE_CLICK_ORDER, 1, footer_row, footer,
            "[S]");
        ui_menu_click_add_text_token(SCORE_CLICK_LAYOUT, 1, footer_row, footer,
            "Layout");
        ui_menu_click_add_text_token(SCORE_CLICK_LAYOUT, 1, footer_row, footer,
            "layout");
        ui_menu_click_add_text_token(SCORE_CLICK_LAYOUT, 1, footer_row, footer,
            "Lay");
        ui_menu_click_add_text_token(SCORE_CLICK_LAYOUT, 1, footer_row, footer,
            "[L]");
        ui_menu_click_add_text_token(SCORE_CLICK_EXIT, 1, footer_row, footer,
            "Exit");
        ui_menu_click_add_text_token(SCORE_CLICK_EXIT, 1, footer_row, footer,
            "exit");
        ui_menu_click_add_text_token(SCORE_CLICK_EXIT, 1, footer_row, footer,
            "Esc");
        ui_menu_click_add_text_token(SCORE_CLICK_EXIT, 1, footer_row, footer,
            "ESC");
        ui_menu_click_add_text_token(SCORE_CLICK_NEXT, 1, footer_row, footer,
            "Next");
        ui_menu_click_add_text_token(SCORE_CLICK_NEXT, 1, footer_row, footer,
            "next");
        ui_menu_click_add_text_token(SCORE_CLICK_NEXT, 1, footer_row, footer,
            "[N]");
        ui_menu_click_add_text_token(SCORE_CLICK_PREV, 1, footer_row, footer,
            "Prev");
        ui_menu_click_add_text_token(SCORE_CLICK_PREV, 1, footer_row, footer,
            "prev");
        ui_menu_click_add_text_token(SCORE_CLICK_PREV, 1, footer_row, footer,
            "[P]");

        bool saved_hide_cursor = hide_cursor;
        hide_cursor = true;
        char ch = inkey();
        hide_cursor = saved_hide_cursor;
        prt("", footer_row, 0);

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice < count)
                {
                    *highlight_index = clicked_choice;
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    if (!show_run_history_detail_for_score(&entries[*highlight_index]))
                        bell("No run history is available for that character.");
                    continue;
                }

                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;

                switch (clicked_choice)
                {
                case SCORE_CLICK_ORDER: ch = 's'; break;
                case SCORE_CLICK_LAYOUT: ch = 'l'; break;
                case SCORE_CLICK_EXIT: ch = ESCAPE; break;
                case SCORE_CLICK_OPEN: ch = '\r'; break;
                case SCORE_CLICK_PREV: ch = 'p'; break;
                case SCORE_CLICK_NEXT: ch = 'n'; break;
                default: break;
                }
            }
            else
                ui_menu_click_clear();
        }

        if (ch == UI_MENU_CLICK_WAKE_KEY)
            continue;

        if (steamdeck) {
            int back_key = steamdeck_back_key();
            int confirm_key = steamdeck_confirm_key();
            int alt_key = steamdeck_alt_action_key();
            int secondary_key = steamdeck_secondary_key();
            
            if (ch == back_key)
                return ESCAPE;  /* B = back */
            if (ch == confirm_key)
                ch = '\r';  /* A = open */
            if (ch == alt_key)
                ch = 'l';  /* X = layout toggle */
            if (ch == secondary_key)
                ch = 's';  /* Y = order toggle */
        }

        if (ch == ESCAPE)
            return ESCAPE;
        if (ch == 's' || ch == 'S' || ch == 'o' || ch == 'O')
            return ch;
        if (ch == 'l' || ch == 'L')
            return ch;

        switch (ch)
        {
        case '8':
        case 'k':
        case 'K':
#ifdef ARROW_UP
        case ARROW_UP:
#endif
            if (*highlight_index > 0)
                (*highlight_index)--;
            else
                bell("Already at top entry.");
            break;

        case '2':
        case 'j':
        case 'J':
#ifdef ARROW_DOWN
        case ARROW_DOWN:
#endif
            if (*highlight_index + 1 < count)
                (*highlight_index)++;
            else
                bell("Already at last entry.");
            break;

        case '3':
        case 'n':
        case 'N':
            if (has_more)
            {
                start_index += entries_per_page;
                if (start_index > max_start)
                    start_index = max_start;
                *highlight_index = start_index;
            }
            else
                bell("Already at last page.");
            break;

        case '4':
#ifdef ARROW_LEFT
        case ARROW_LEFT:
#endif
        case '7':
        case '-':
        case 'p':
        case 'P':
            if (has_prev)
            {
                start_index -= entries_per_page;
                if (start_index < 0)
                    start_index = 0;
                *highlight_index = start_index;
            }
            else
                bell("Already at first page.");
            break;

        case ' ':
        case '6':
        case '\r':
        case '\n':
#ifdef ARROW_RIGHT
        case ARROW_RIGHT:
#endif
            if (!show_run_history_detail_for_score(&entries[*highlight_index]))
                bell("No run history is available for that character.");
            break;

        default:
            return 0;
        }
    }

    return 0;
}
void display_scores(int from, int to)
{
    (void)from;
    (void)to;

    log_info("Displaying high scores with interactive controls");
    show_scores_interactive(true);
    quit(NULL);
}
void display_scores_short(int from, int to)
{
    (void)from;
    (void)to;

    bool previous_layout = score_last_layout_short;
    score_last_layout_short = true;
    show_scores_interactive(true);
    score_last_layout_short = previous_layout;
}
static bool ensure_entry_visible(high_score* entries, int* count, int capacity,
                                 const high_score* target, bool sort_by_score, int* highlight_index)
{
    if (!entries || !count || !target || capacity <= 0)
        return false;

    bool score_file_has_links =
        scores_version_has_run_links(score_file_active_ctx());
    int idx = find_score_index(entries, *count, target);
    if (idx >= 0)
    {
        u32b target_record_id;
        if (score_file_has_links
            && score_runs_get_legacy_link(target, &target_record_id))
        {
            u32b existing_record_id;
            if (!score_runs_get_legacy_link(&entries[idx], &existing_record_id)
                || existing_record_id != target_record_id)
            {
                entries[idx] = *target;
            }
        }
        if (highlight_index) *highlight_index = idx;
        return true;
    }

    if (*count < capacity)
    {
        entries[*count] = *target;
        (*count)++;
    }
    else
    {
        entries[capacity - 1] = *target;
        *count = capacity;
    }

    if (sort_by_score)
        qsort(entries, *count, sizeof(high_score), compare_scores_qsort);
    else
        qsort(entries, *count, sizeof(high_score), compare_scores_chronological);

    *count = deduplicate_scores_by_name(entries, *count);
    if (*count > MAX_HISCORES)
        *count = MAX_HISCORES;

    idx = find_score_index(entries, *count, target);

    if (idx < 0)
    {
        for (int i = 0; i < *count; i++)
        {
            if (streq(entries[i].who, target->who))
            {
                entries[i] = *target;
                if (sort_by_score)
                    qsort(entries, *count, sizeof(high_score), compare_scores_qsort);
                else
                    qsort(entries, *count, sizeof(high_score), compare_scores_chronological);
                idx = find_score_index(entries, *count, target);
                break;
            }
        }
    }

    if (highlight_index && idx >= 0)
        *highlight_index = idx;

    return idx >= 0;
}
void show_scores(bool longscore)
{
    bool preview_allowed = (!force_interactive_scores && !forced_highlight_active && character_generated && !p_ptr->is_dead);
    log_info("show_scores: longscore=%d force_interactive=%d generated=%d dead=%d preview=%d",
             longscore ? 1 : 0,
             force_interactive_scores ? 1 : 0,
             character_generated ? 1 : 0,
             p_ptr->is_dead ? 1 : 0,
             preview_allowed ? 1 : 0);

    high_score ordered_by_score[MAX_HISCORES + 1];
    high_score ordered_by_time[MAX_HISCORES + 1];

    int count_score = collect_high_scores(ordered_by_score, MAX_HISCORES, true);
    int count_time = collect_high_scores(ordered_by_time, MAX_HISCORES, false);

    const int capacity = MAX_HISCORES + 1;
    int page_size = 5;
    bool detailed = !score_last_layout_short;
    score_view_order order = SCORE_VIEW_ORDER_SCORE;
    int term_wid = 80;
    int term_hgt = 24;

    score_ui_get_term_size(&term_wid, &term_hgt);
    if (score_ui_compact_width(term_wid) || term_hgt < 20)
        detailed = false;

    high_score highlight_buffer;
    const high_score* highlight_entry = NULL;
    if (forced_highlight_active)
    {
        highlight_entry = &forced_highlight_entry;
    }
    else if (character_generated)
    {
        if (p_ptr->is_dead)
        {
            if (create_score(&highlight_buffer) == 0)
                highlight_entry = &highlight_buffer;
        }
        else if (build_live_preview_score(&highlight_buffer))
        {
            highlight_entry = &highlight_buffer;
        }
    }

    int highlight_score = -1;
    int highlight_time = -1;
    if (highlight_entry)
    {
        highlight_score = find_score_index(ordered_by_score, count_score, highlight_entry);
        highlight_time = find_score_index(ordered_by_time, count_time, highlight_entry);
        log_debug("show_scores: highlight indices score=%d time=%d for %s",
                  highlight_score, highlight_time, highlight_entry->who);

        if (highlight_score < 0)
            ensure_entry_visible(ordered_by_score, &count_score, capacity, highlight_entry, true, &highlight_score);
        if (highlight_time < 0)
            ensure_entry_visible(ordered_by_time, &count_time, capacity, highlight_entry, false, &highlight_time);
    }

    screen_push_supporting_panes_hidden();
    while (true)
    {
        const high_score* list = (order == SCORE_VIEW_ORDER_SCORE) ? ordered_by_score : ordered_by_time;
        int count = (order == SCORE_VIEW_ORDER_SCORE) ? count_score : count_time;
        int* highlight = (order == SCORE_VIEW_ORDER_SCORE) ? &highlight_score : &highlight_time;

        log_debug("show_scores: rendering page (order=%s count=%d highlight=%d)",
                  (order == SCORE_VIEW_ORDER_SCORE) ? "score" : "time",
                  count, highlight ? *highlight : -1);

        char response = display_scores_pages(list, count, highlight, order, detailed, page_size);
        if (response == 's' || response == 'S' || response == 'o' || response == 'O')
        {
            high_score selected;
            bool have_selected = false;

            if (highlight && *highlight >= 0 && *highlight < count)
            {
                selected = list[*highlight];
                have_selected = true;
            }

            order = (order == SCORE_VIEW_ORDER_SCORE) ? SCORE_VIEW_ORDER_CHRONOLOGY : SCORE_VIEW_ORDER_SCORE;

            if (have_selected)
            {
                if (order == SCORE_VIEW_ORDER_SCORE)
                {
                    int idx = find_score_index(ordered_by_score, count_score,
                        &selected);
                    if (idx >= 0)
                        highlight_score = idx;
                }
                else
                {
                    int idx = find_score_index(ordered_by_time, count_time,
                        &selected);
                    if (idx >= 0)
                        highlight_time = idx;
                }
            }
            continue;
        }
        if (response == 'l' || response == 'L')
        {
            detailed = !detailed;
            score_last_layout_short = !detailed;
            continue;
        }
        break;
    }
    screen_pop_supporting_panes_hidden();

    forced_highlight_active = false;
    score_last_layout_short = !detailed;
}
void show_scores_interactive(bool longscore)
{
    bool previous = force_interactive_scores;
    force_interactive_scores = true;
    log_debug("show_scores_interactive: forcing interactive display (longscore=%d)", longscore ? 1 : 0);
    show_scores(longscore);
    force_interactive_scores = previous;
}
void show_scores_interactive_highlight(bool longscore, const high_score* entry)
{
    high_score saved_entry;
    bool had_forced = forced_highlight_active;
    if (had_forced) saved_entry = forced_highlight_entry;

    if (entry) {
        set_forced_highlight_entry(entry);
    } else {
        forced_highlight_active = false;
    }

    show_scores_interactive(longscore);

    if (had_forced) {
        forced_highlight_entry = saved_entry;
        forced_highlight_active = true;
    } else {
        forced_highlight_active = false;
    }
}

void show_scores_interactive_highlight_from_file(bool longscore,
                                                 const char* filepath,
                                                 const high_score* entry)
{
    if (!filepath || !filepath[0]) {
        show_scores_interactive_highlight(longscore, entry);
        return;
    }

    score_file_ctx temp_ctx;
    score_file_reset_ctx(&temp_ctx);

    safe_setuid_grab();
    temp_ctx.fd = score_file_open(filepath, O_RDONLY);
    safe_setuid_drop();
    if (!temp_ctx.fd) {
        log_warn("show_scores_interactive_highlight_from_file: unable to open %s",
                 filepath);
        show_scores_interactive_highlight(longscore, entry);
        return;
    }

    log_debug("show_scores_interactive_highlight_from_file: rendering %s",
              filepath);
    score_file_ctx* previous_ctx = score_file_set_active_ctx(&temp_ctx);
    show_scores_interactive_highlight(longscore, entry);
    score_file_set_active_ctx(previous_ctx);

    SDL_CloseIO(temp_ctx.fd);
    score_file_reset_ctx(&temp_ctx);
}

#if 0
static const char* score_run_killer_kind_label(score_killer_kind kind)
{
    switch (kind) {
    case SCORE_KILLER_MONSTER: return "Monster";
    case SCORE_KILLER_TRAP: return "Trap";
    case SCORE_KILLER_FALL: return "Fall";
    case SCORE_KILLER_SELF: return "Self";
    case SCORE_KILLER_OTHER: return "Other";
    default: return "Unknown";
    }
}
#endif

static const char* score_run_status_label(score_record_status status)
{
    switch (status) {
    case SCORE_RECORD_ALIVE: return "Alive";
    case SCORE_RECORD_DEAD: return "Dead";
    case SCORE_RECORD_ESCAPED: return "Escaped";
    default: return "Unknown";
    }
}

static bool run_history_skip_details(SDL_IOStream* file, s64b* detail_offset)
{
    if (!file)
        return false;

    Sint64 header_pos = SDL_TellIO(file);
    score_run_detail_header_v1 header;
    if (SDL_ReadIO(file, &header, sizeof(header)) != sizeof(header))
        return false;

    if (detail_offset)
        *detail_offset = (s64b)header_pos;

    return score_runs_skip_detail_payload(file, &header);
}
static const char* run_history_race_name(byte idx)
{
    if (!p_info || !p_name || !z_info || idx >= z_info->p_max)
        return "<unknown>";
    return p_name + p_info[idx].name;
}
#if 0
static const char* run_history_character_name(byte idx)
{
    if (!c_info || !c_name || !z_info || idx >= z_info->c_max)
        return "<unknown>";
    return c_name + c_info[idx].name;
}
#endif
static const char* run_history_monster_name(u16b r_idx)
{
    if (!r_info || !r_name || !z_info || r_idx == 0 || r_idx >= z_info->r_max)
        return "<unknown>";
    return r_name + r_info[r_idx].name;
}
static void run_history_format_timestamp(u32b utc, bool include_time,
                                         char* out, size_t out_len)
{
    if (!out || out_len == 0)
        return;

    if (!utc) {
        SDL_strlcpy(out, "----", out_len);
        return;
    }

    time_t ts = (time_t)utc;
    struct tm* tm_info = localtime(&ts);
    if (!tm_info) {
        SDL_strlcpy(out, "----", out_len);
        return;
    }

    const char* fmt = include_time ? "%Y-%m-%d %H:%M" : "%Y-%m-%d";
    if (strftime(out, out_len, fmt, tm_info) == 0) {
        SDL_strlcpy(out, "----", out_len);
    }
}

static void run_history_build_summary(const char* player,
                                      const score_record_v1* rec,
                                      char* out, size_t out_len)
{
    if (!out || out_len == 0) {
        return;
    }

    if (!rec) {
        out[0] = '\0';
        return;
    }

    const char* name = (player && *player) ? player : NULL;
    const char* cause = rec->cause_of_death[0] ? rec->cause_of_death : NULL;

    if (name && cause) {
        strnfmt(out, out_len, "%s: %s", name, cause);
    } else if (name) {
        SDL_strlcpy(out, name, out_len);
    } else if (cause) {
        SDL_strlcpy(out, cause, out_len);
    } else {
        SDL_strlcpy(out, "<unknown>", out_len);
    }
}
#if 0
static void run_history_format_flags(byte run_flags, char* out, size_t out_len)
{
    if (!out || out_len == 0)
        return;

    out[0] = '\0';
    bool first = true;

    #define APPEND_FLAG(label) \
        do { \
            if (!first) SDL_strlcat(out, ", ", out_len); \
            SDL_strlcat(out, (label), out_len); \
            first = false; \
        } while (0)

    if (run_flags & SCORE_RUN_FLAG_MORGOTH_SLAIN)
        APPEND_FLAG("Morgoth slain");
    if (run_flags & SCORE_RUN_FLAG_ANGBAND_ESCAPED)
        APPEND_FLAG("Escaped");
    if (run_flags & SCORE_RUN_FLAG_NOSCORE)
        APPEND_FLAG("No score");
    if (run_flags & SCORE_RUN_FLAG_CHEAT)
        APPEND_FLAG("Cheat");
    if (run_flags & SCORE_RUN_FLAG_BLITZ)
        APPEND_FLAG("Blitz");

    #undef APPEND_FLAG

    if (first)
        SDL_strlcpy(out, "(none)", out_len);
}
#endif

static int run_history_compare_date_desc(const void* a, const void* b)
{
    const run_history_entry* ea = (const run_history_entry*)a;
    const run_history_entry* eb = (const run_history_entry*)b;
    const score_record_v1* ra = &ea->record;
    const score_record_v1* rb = &eb->record;

    if (ra->completed_utc > rb->completed_utc)
        return -1;
    if (ra->completed_utc < rb->completed_utc)
        return 1;

    if (ra->record_id > rb->record_id)
        return -1;
    if (ra->record_id < rb->record_id)
        return 1;

    if (ra->created_utc > rb->created_utc)
        return -1;
    if (ra->created_utc < rb->created_utc)
        return 1;

    return 0;
}

static int run_history_compare_rating_desc(const void* a, const void* b)
{
    const run_history_entry* ea = (const run_history_entry*)a;
    const run_history_entry* eb = (const run_history_entry*)b;
    if (ea->rating > eb->rating)
        return -1;
    if (ea->rating < eb->rating)
        return 1;
    return run_history_compare_date_desc(a, b);
}

static void run_history_sort_entries(run_history_entry* entries,
                                     int count,
                                     run_history_sort_order order)
{
    if (!entries || count <= 1)
        return;
    if (order == RUN_HISTORY_SORT_RATING)
        qsort(entries, count, sizeof(run_history_entry),
              run_history_compare_rating_desc);
    else
        qsort(entries, count, sizeof(run_history_entry),
              run_history_compare_date_desc);
}

static int collect_run_history(run_history_entry* out, int capacity)
{
    if (capacity <= 0 || !out)
        return 0;

    char path[1024];
    if (!build_meta_path(path, sizeof(path), SCORE_RUNS_DB_FILENAME))
        return 0;

    safe_setuid_grab();
    SDL_IOStream* file = SDL_IOFromFile(path, "rb");
    safe_setuid_drop();

    if (!file)
        return 0;

    score_db_header header;
    if (SDL_ReadIO(file, &header, sizeof(header)) != sizeof(header) ||
        memcmp(header.magic, SCORE_DB_MAGIC, sizeof(header.magic)) != 0) {
        SDL_CloseIO(file);
        return 0;
    }

    run_history_entry* ring = mem_alloc_array(capacity, run_history_entry);
    if (!ring) {
        SDL_CloseIO(file);
        return 0;
    }

    int stored = 0;
    score_record_v1 temp;
    while (SDL_ReadIO(file, &temp, sizeof(temp)) == sizeof(temp)) {
        s64b detail_offset = (s64b)SDL_TellIO(file);
        if (!run_history_skip_details(file, &detail_offset))
            break;
        run_history_entry* slot = &ring[stored % capacity];
        slot->record = temp;
        slot->detail_offset = detail_offset;
        slot->rating = run_history_compute_rating(&temp);
        stored++;
    }

    SDL_CloseIO(file);

    int count = (stored < capacity) ? stored : capacity;
    if (count <= 0) {
        mem_free(ring);
        return 0;
    }

    int start = (stored > capacity) ? (stored % capacity) : 0;
    for (int i = 0; i < count; i++) {
        int idx = (start + i) % capacity;
        out[i] = ring[idx];
    }

    mem_free(ring);
    return count;
}

static bool run_history_find_by_record_id(u32b record_id,
                                          run_history_entry* out)
{
    if (!out)
        return false;

    char path[1024];
    if (!build_meta_path(path, sizeof(path), SCORE_RUNS_DB_FILENAME))
        return false;

    safe_setuid_grab();
    SDL_IOStream* file = SDL_IOFromFile(path, "rb");
    safe_setuid_drop();

    if (!file)
        return false;

    score_db_header header;
    if (SDL_ReadIO(file, &header, sizeof(header)) != sizeof(header) ||
        memcmp(header.magic, SCORE_DB_MAGIC, sizeof(header.magic)) != 0) {
        SDL_CloseIO(file);
        return false;
    }

    score_record_v1 temp;
    while (SDL_ReadIO(file, &temp, sizeof(temp)) == sizeof(temp)) {
        s64b detail_offset = (s64b)SDL_TellIO(file);

        if (temp.record_id == record_id) {
            out->record = temp;
            out->detail_offset = detail_offset;
            out->rating = run_history_compute_rating(&temp);
            SDL_CloseIO(file);
            return true;
        }

        if (!run_history_skip_details(file, &detail_offset))
            break;
    }

    SDL_CloseIO(file);
    return false;
}

static int score_run_history_match_quality(const high_score* score,
                                           const run_history_entry* entry)
{
    if (!score || !entry)
        return -1;

    high_score record_score;
    run_history_build_high_score(&entry->record, &record_score);

    if (score_identity_matches(score, &record_score))
        return 1000;

    if (!streq(score->who, record_score.who))
        return -1;

    int quality = 10;

    if (streq(score->how, record_score.how))
        quality += 200;
    if (streq(score->day, record_score.day))
        quality += 100;
    if (score_points(score) == score_points(&record_score))
        quality += 60;
    if (atoi(score->p_h) == atoi(record_score.p_h))
        quality += 30;
    if (atoi(score->p_r) == atoi(record_score.p_r))
        quality += 10;
    if (atoi(score->turns) == atoi(record_score.turns))
        quality += 20;
    if (atoi(score->cur_dun) == atoi(record_score.cur_dun))
        quality += 10;
    if (atoi(score->max_dun) == atoi(record_score.max_dun))
        quality += 10;
    if (score->escaped[0] == record_score.escaped[0])
        quality += 10;
    if (score->morgoth_slain[0] == record_score.morgoth_slain[0])
        quality += 10;

    return quality;
}

static bool show_run_history_detail_for_score(const high_score* score)
{
    if (!score)
        return false;

    const int minimum_quality = 150;

    run_history_refresh_active_run();

    u32b linked_record_id;
    if (scores_version_has_run_links(score_file_active_ctx())
        && score_runs_get_legacy_link(score, &linked_record_id))
    {
        run_history_entry linked_entry;
        if (run_history_find_by_record_id(linked_record_id, &linked_entry))
        {
            int quality = score_run_history_match_quality(score,
                &linked_entry);
            if (quality >= minimum_quality)
            {
                run_history_show_detail(&linked_entry);
                return true;
            }
        }
    }

    run_history_entry entries[RUN_HISTORY_MAX];
    int count = collect_run_history(entries, RUN_HISTORY_MAX);
    int best = -1;
    int best_quality = -1;

    for (int i = 0; i < count; i++)
    {
        int quality = score_run_history_match_quality(score, &entries[i]);
        if (quality < 0)
            continue;

        if (quality > best_quality
            || (quality == best_quality
                && entries[i].rating > entries[best].rating)
            || (quality == best_quality
                && entries[i].rating == entries[best].rating
                && entries[i].record.completed_utc
                    > entries[best].record.completed_utc))
        {
            best = i;
            best_quality = quality;
        }
    }

    if (best < 0 || best_quality < minimum_quality)
        return false;

    run_history_show_detail(&entries[best]);
    return true;
}

void do_cmd_run_history(void)
{
    enum {
        RUN_HISTORY_CLICK_SORT = -1,
        RUN_HISTORY_CLICK_BACK = -2,
        RUN_HISTORY_CLICK_DETAILS = -3,
        RUN_HISTORY_CLICK_PREV = -4,
        RUN_HISTORY_CLICK_NEXT = -5
    };
    run_history_refresh_active_run();

    run_history_entry entries[RUN_HISTORY_MAX];
    int count = collect_run_history(entries, RUN_HISTORY_MAX);
    if (count <= 0) {
        msg_print("No run history is available.");
        return;
    }
    run_history_sort_order sort_order = RUN_HISTORY_SORT_DATE;
    run_history_sort_entries(entries, count, sort_order);

    int page_offset = 0;
    int highlight = 0;
    bool done = false;

    screen_save();
    screen_push_supporting_panes_hidden();

    while (!done) {
        int term_wid = 80;
        int term_hgt = 24;
        int footer_row;
        int rows;
        int total_pages;
        int last_page_offset;
        bool compact;
        int col_date = 2;
        int col_status = 0;
        int col_depth = 0;
        int col_score = 0;
        int col_sils = -1;
        int col_player = 0;
        int col_fate = -1;
        int player_width = 0;
        int fate_width = 0;
        int summary_width = 0;
        bool show_sils = false;
        bool steamdeck = steamdeck_controls_active();
        char confirm_label[16] = "";
        char back_label[16] = "";
        char sort_label[16] = "";
        int page_label_row = 1;

        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_scroll_area_clear();

        if (steamdeck) {
            score_prompt_label(steamdeck_confirm_key(), "A",
                confirm_label, sizeof(confirm_label));
            score_prompt_label(steamdeck_back_key(), "B",
                back_label, sizeof(back_label));
            score_prompt_label(steamdeck_secondary_key(), "Y",
                sort_label, sizeof(sort_label));
        }

        score_ui_get_term_size(&term_wid, &term_hgt);
        footer_row = term_hgt - 1;
        if (footer_row < 5)
            footer_row = 5;
        rows = footer_row - 3;
        if (rows < 4)
            rows = 4;
        total_pages = (count + rows - 1) / rows;
        last_page_offset = ((count - 1) / rows) * rows;
        if (last_page_offset < 0)
            last_page_offset = 0;
        compact = score_ui_compact_width(term_wid);

        if (page_offset < 0)
            page_offset = 0;
        if (page_offset > last_page_offset)
            page_offset = last_page_offset;

        int page = (rows > 0) ? (page_offset / rows) : 0;

        if (compact) {
            const int date_width = 10;
            const int status_width = 1;
            const int depth_width = 5;
            const int score_width = 6;
            const int sils_width = 1;

            col_status = col_date + date_width + 1;
            col_depth = col_status + status_width + 1;
            col_score = col_depth + depth_width + 1;
            show_sils = (term_wid >= 48);
            if (show_sils) {
                col_sils = col_score + score_width + 1;
                col_player = col_sils + sils_width + 2;
            } else {
                col_player = col_score + score_width + 2;
            }
            summary_width = term_wid - col_player;
            if (summary_width < 0)
                summary_width = 0;

            score_ui_put_fit(TERM_L_BLUE,
                format("Run History %d/%d", page + 1, total_pages), 0, 0,
                term_wid);
            page_label_row = 1;
            if (steamdeck) {
                score_ui_put_fit(TERM_SLATE,
                    format("Sort: %s [%s]", run_history_sort_label(sort_order),
                        sort_label),
                    page_label_row, 2, term_wid);
            } else {
                score_ui_put_fit(TERM_SLATE,
                    format("Sort: %s [R]", run_history_sort_label(sort_order)),
                    page_label_row, 2, term_wid);
            }
            ui_menu_click_add_full_row(RUN_HISTORY_CLICK_SORT, page_label_row);
            c_prt(TERM_L_UMBER, "Date", 2, 2);
            c_prt(TERM_L_UMBER, "S", 2, col_status);
            c_prt(TERM_L_UMBER, "Depth", 2, col_depth);
            c_prt(TERM_L_UMBER, "Score", 2, col_score);
            if (show_sils)
                c_prt(TERM_L_UMBER, "Sil", 2, col_sils);
            if (summary_width > 0)
                score_ui_put_fit(TERM_L_UMBER, "Player / Fate", 2, col_player,
                    term_wid);
        } else {
            const int date_width = 10;
            const int status_width = 7;
            const int depth_width = 6;
            const int score_width = 7;
            const int sils_width = 4;
            int remaining;

            col_status = col_date + date_width + 1;
            col_depth = col_status + status_width + 1;
            col_score = col_depth + depth_width + 1;
            col_sils = col_score + score_width + 1;
            col_player = col_sils + sils_width + 2;
            remaining = term_wid - col_player;
            if (remaining < 0)
                remaining = 0;

            player_width = remaining / 3;
            if (player_width < 10)
                player_width = 10;
            if (player_width > 18)
                player_width = 18;
            if (player_width > remaining - 8)
                player_width = MAX(8, remaining - 8);
            fate_width = remaining - player_width - 1;
            if (fate_width < 6 && player_width > 8) {
                int give = MIN(player_width - 8, 6 - fate_width);
                player_width -= give;
                fate_width += give;
            }
            if (player_width < 0)
                player_width = 0;
            if (fate_width < 0)
                fate_width = 0;
            if (fate_width > 0)
                col_fate = col_player + player_width + 1;

            score_ui_put_fit(TERM_L_BLUE,
                format("=== Run History (%d entries) === Page %d of %d ===",
                    count, page + 1, total_pages),
                0, 0, term_wid);
            page_label_row = 1;
            if (steamdeck) {
                score_ui_put_fit(TERM_SLATE,
                    format("Sort: %s (press [%s] to toggle)",
                        run_history_sort_label(sort_order), sort_label),
                    page_label_row, 2, term_wid);
            } else {
                score_ui_put_fit(TERM_SLATE,
                    format("Sort: %s (press [R] to toggle)",
                        run_history_sort_label(sort_order)),
                    page_label_row, 2, term_wid);
            }
            ui_menu_click_add_full_row(RUN_HISTORY_CLICK_SORT, page_label_row);
            c_prt(TERM_L_UMBER, "Date", 2, col_date);
            c_prt(TERM_L_UMBER, "Status", 2, col_status);
            c_prt(TERM_L_UMBER, "Depth", 2, col_depth);
            c_prt(TERM_L_UMBER, "Score", 2, col_score);
            c_prt(TERM_L_UMBER, "Sils", 2, col_sils);
            if (player_width > 0)
                c_prt(TERM_L_UMBER, "Player", 2, col_player);
            if (fate_width > 0)
                c_prt(TERM_L_UMBER, "Fate", 2, col_fate);
        }

        for (int i = 0; i < rows; i++) {
            int idx = page_offset + i;
            if (idx >= count)
                break;

            const score_record_v1* rec = &entries[idx].record;
            int row_y = 3 + i;

            char date[16];
            run_history_format_timestamp(rec->completed_utc, false, date, sizeof(date));

            char player[21];
            if (rec->player_name[0]) {
                SDL_strlcpy(player, rec->player_name, sizeof(player));
            } else if (rec->savefile_hint[0]) {
                SDL_strlcpy(player, rec->savefile_hint, sizeof(player));
            } else {
                SDL_strlcpy(player, "<unknown>", sizeof(player));
            }

            /* Convert depth to feet */
            int depth_ft = rec->exit_depth * 50;

            bool selected = (idx == highlight);
            byte row_color = selected ? TERM_YELLOW : 
                           (rec->status == SCORE_RECORD_ALIVE) ? TERM_L_GREEN :
                           (rec->silmarils > 0) ? TERM_VIOLET : TERM_WHITE;

            c_prt(row_color, selected ? ">" : " ", row_y, 0);
            ui_menu_click_add_full_row(idx, row_y);
            c_prt(row_color, date, row_y, 2);
            if (compact) {
                char summary[160];
                char summary_fit[160];
                char status_buf[2] = { run_history_status_short(rec->status), '\0' };
                c_prt(row_color, status_buf, row_y, col_status);
                c_prt(row_color, format("%4d'", depth_ft), row_y, col_depth);
                c_prt(row_color, format("%6d", entries[idx].rating), row_y,
                    col_score);
                if (show_sils)
                    c_prt(row_color, format("%1u", (unsigned)rec->silmarils),
                        row_y, col_sils);
                if (summary_width > 0) {
                    run_history_build_summary(player, rec, summary,
                        sizeof(summary));
                    truncate_with_ellipsis(summary, summary_fit,
                        sizeof(summary_fit), summary_width);
                    Term_putstr(col_player, row_y, summary_width, row_color,
                        summary_fit);
                }
            } else {
                char cause[160];
                char player_fit[32];

                c_prt(row_color, score_run_status_label(rec->status), row_y,
                    col_status);
                c_prt(row_color, format("%6d'", depth_ft), row_y, col_depth);
                c_prt(row_color, format("%7d", entries[idx].rating), row_y,
                    col_score);
                c_prt(row_color, format("%3u", (unsigned)rec->silmarils),
                    row_y, col_sils);
                if (player_width > 0) {
                    truncate_preserving_words(player, player_fit,
                        sizeof(player_fit), player_width);
                    Term_putstr(col_player, row_y, player_width, row_color,
                        player_fit);
                }
                if (fate_width > 0) {
                    truncate_with_ellipsis(rec->cause_of_death, cause,
                        sizeof(cause), fate_width);
                    Term_putstr(col_fate, row_y, fate_width, row_color, cause);
                }
            }
        }

        if (steamdeck) {
            char footer[160];

            strnfmt(footer, sizeof(footer),
                "[%s] details  [%s] sort  [%s] back  [Up/Down] move  [Left/Right] page",
                confirm_label, sort_label, back_label);
            score_ui_put_fit(TERM_L_DARK, footer, footer_row, 0, term_wid);
            ui_menu_click_add_text_token(RUN_HISTORY_CLICK_DETAILS, 0,
                footer_row, footer, "details");
            ui_menu_click_add_text_token(RUN_HISTORY_CLICK_SORT, 0,
                footer_row, footer, "sort");
            ui_menu_click_add_text_token(RUN_HISTORY_CLICK_BACK, 0,
                footer_row, footer, "back");
            ui_menu_click_add_text_token(RUN_HISTORY_CLICK_PREV, 0,
                footer_row, footer, "Left");
            ui_menu_click_add_text_token(RUN_HISTORY_CLICK_NEXT, 0,
                footer_row, footer, "Right");
        } else {
            const char *footer =
                "[Space/Enter/Right] details  [R] sort  [Esc] back  [Up/Down] move  [N/P/3/7] page";

            score_ui_put_fit(TERM_L_DARK, footer, footer_row, 0, term_wid);
            ui_menu_click_add_text_token(RUN_HISTORY_CLICK_DETAILS, 0,
                footer_row, footer, "details");
            ui_menu_click_add_text_token(RUN_HISTORY_CLICK_SORT, 0,
                footer_row, footer, "sort");
            ui_menu_click_add_text_token(RUN_HISTORY_CLICK_BACK, 0,
                footer_row, footer, "back");
            ui_menu_click_add_text_token(RUN_HISTORY_CLICK_NEXT, 0,
                footer_row, footer, "N/P");
        }

        ui_scroll_area_begin(3, footer_row - 1, SDL_TOUCH_MENU_CATEGORY_OTHER);
        ui_scroll_area_set_keys('8', '2', 'n', 'p');

        (void)Term_set_cursor(false);
        Term_fresh();
        bool saved_hide_cursor = hide_cursor;
        hide_cursor = true;
        int ch = inkey();
        hide_cursor = saved_hide_cursor;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                ui_scroll_area_clear();
                if (clicked_choice >= 0 && clicked_choice < count)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                    {
                        highlight = clicked_choice;
                        continue;
                    }
                    if (highlight != clicked_choice)
                    {
                        highlight = clicked_choice;
                        continue;
                    }
                    ch = '\r';
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                else
                {
                    switch (clicked_choice)
                    {
                    case RUN_HISTORY_CLICK_SORT: ch = 'r'; break;
                    case RUN_HISTORY_CLICK_BACK: ch = ESCAPE; break;
                    case RUN_HISTORY_CLICK_DETAILS: ch = '\r'; break;
                    case RUN_HISTORY_CLICK_PREV: ch = 'p'; break;
                    case RUN_HISTORY_CLICK_NEXT: ch = 'n'; break;
                    default: break;
                    }
                }
            }
            else {
                ui_menu_click_clear();
                ui_scroll_area_clear();
            }
        }

        if (steamdeck) {
            if (ch == steamdeck_back_key())
                ch = ESCAPE;
            else if (ch == steamdeck_confirm_key())
                ch = '\r';
            else if (ch == steamdeck_secondary_key())
                ch = 'r';
        }

        switch (ch) {
        case ESCAPE:
        case 'q':
            done = true;
            break;

        case 'r':
        case 'R':
            sort_order = (sort_order == RUN_HISTORY_SORT_DATE)
                ? RUN_HISTORY_SORT_RATING : RUN_HISTORY_SORT_DATE;
            run_history_sort_entries(entries, count, sort_order);
            page_offset = 0;
            highlight = 0;
            break;

        case 'y':
        case 'Y':
        case ' ':
        case '6':
        case '\r':
        case '\n':
#ifdef ARROW_RIGHT
        case ARROW_RIGHT:
#endif
            run_history_show_detail(&entries[highlight]);
            break;

        case '3':
        case 'n':
        case 'N':
            if (page_offset + rows < count) {
                page_offset += rows;
                if (page_offset > last_page_offset)
                    page_offset = last_page_offset;
                highlight += rows;
                if (highlight >= count)
                    highlight = count - 1;
            } else {
                bell("Already at last page.");
            }
            break;

        case '4':
#ifdef ARROW_LEFT
        case ARROW_LEFT:
#endif
        case '7':
        case '-':
        case 'p':
        case 'P':
            if (page_offset > 0) {
                page_offset -= rows;
                if (page_offset < 0)
                    page_offset = 0;
                if (highlight < page_offset)
                    highlight = page_offset;
            } else {
                bell("Already at first page.");
            }
            break;

        case '8':
        case 'k':
        case 'K':
            if (highlight > 0) {
                highlight--;
                if (highlight < page_offset)
                    page_offset = (highlight / rows) * rows;
            } else {
                bell("Already at top entry.");
            }
            break;

        case '2':
        case 'j':
        case 'J':
            if (highlight + 1 < count) {
                highlight++;
                if (highlight >= page_offset + rows)
                    page_offset = (highlight / rows) * rows;
            } else {
                bell("Already at last entry.");
            }
            break;

        default:
            break;
        }
    }

    screen_pop_supporting_panes_hidden();
    ui_menu_click_clear();
    ui_scroll_area_clear();
    screen_load();
}
static bool run_history_prepare_artefact_object(
    const score_run_artefact_v1* entry, object_type* out)
{
    if (!entry || !out || !z_info)
        return false;
    if (entry->a_idx <= 0 || entry->a_idx >= z_info->art_max)
        return false;

    object_wipe(out);

#ifdef ALLOW_SPOILERS
    if (make_fake_artefact(out, (byte)entry->a_idx))
        goto prepared;
#endif

    artefact_type* art = &a_info[entry->a_idx];
    if (!art || (art->tval == 0 && art->sval == 0))
        return false;

    s16b k_idx = lookup_kind(art->tval, art->sval);
    if (k_idx <= 0)
        return false;

    object_prep(out, k_idx);
    out->name1 = (byte)entry->a_idx;
    out->pval = art->pval;
    out->att = art->att;
    out->dd = art->dd;
    out->ds = art->ds;
    out->evn = art->evn;
    out->pd = art->pd;
    out->ps = art->ps;
    out->weight = art->weight;

    for (int i = 0; i < art->abilities; i++)
    {
        out->skilltype[i + out->abilities] = art->skilltype[i];
        out->abilitynum[i + out->abilities] = art->abilitynum[i];
    }
    out->abilities += art->abilities;

    if (art->flags3 & (TR3_LIGHT_CURSE))
        out->ident |= IDENT_CURSED;

prepared:
    out->ident |= IDENT_KNOWN | IDENT_SENSE;
    object_known(out);
    return true;
}


static const char* run_detail_panel_names[RUN_PANEL_COUNT] = {
    "General", "Stats", "Abilities", "Milestones", "Artefacts", "Monsters"
};

static const char* run_detail_panel_names_compact[RUN_PANEL_COUNT] = {
    "Gen", "Stat", "Abil", "Mile", "Art", "Mon"
};

enum {
    RUN_DETAIL_CLICK_BACK = -1,
    RUN_DETAIL_CLICK_PREV_PANEL = -2,
    RUN_DETAIL_CLICK_NEXT_PANEL = -3,
    RUN_DETAIL_CLICK_INSPECT = -4,
    RUN_DETAIL_CLICK_SORT = -5,
    RUN_DETAIL_CLICK_TAB_BASE = -100
};

static int run_history_detail_tab_choice(run_detail_panel panel)
{
    return RUN_DETAIL_CLICK_TAB_BASE - (int)panel;
}

static bool run_history_detail_choice_to_tab(int choice,
                                             run_detail_panel* panel)
{
    int idx;

    if (choice > RUN_DETAIL_CLICK_TAB_BASE)
        return false;

    idx = RUN_DETAIL_CLICK_TAB_BASE - choice;
    if (idx < 0 || idx >= RUN_PANEL_COUNT)
        return false;

    if (panel)
        *panel = (run_detail_panel)idx;
    return true;
}

static void run_history_draw_panel_tabs(run_detail_panel active,
                                        const bool available[RUN_PANEL_COUNT])
{
    int col = 0;
    int active_idx = (int)active;
    int term_wid = 80;
    int term_hgt = 24;
    bool compact;

    score_ui_get_term_size(&term_wid, &term_hgt);
    compact = (term_wid < 72);
    for (int i = 0; i < RUN_PANEL_COUNT; i++) {
        char buffer[32];
        strnfmt(buffer, sizeof(buffer), "%s%s%s",
            (i == active_idx) ? "[" : " ",
            compact ? run_detail_panel_names_compact[i]
                    : run_detail_panel_names[i],
            (i == active_idx) ? "]" : " ");
        byte color = available[i] ? TERM_L_WHITE : TERM_SLATE;
        if (i == active_idx)
            color = available[i] ? TERM_WHITE : TERM_SLATE;
        score_ui_put_fit(color, buffer, 0, col, term_wid);
        ui_menu_click_add(run_history_detail_tab_choice((run_detail_panel)i),
            col, 0, (int)strlen(buffer));
        col += (int)strlen(buffer) + 1;
    }
    score_ui_put_fit(TERM_L_DARK, "Left/Right change views", 1, 0, term_wid);
    ui_menu_click_add_text_token(RUN_DETAIL_CLICK_PREV_PANEL, 0, 1,
        "Left/Right change views", "Left");
    ui_menu_click_add_text_token(RUN_DETAIL_CLICK_NEXT_PANEL, 0, 1,
        "Left/Right change views", "Right");
}

static const char* run_history_monster_sort_labels[RUN_MON_SORT_COUNT] = {
    "First met",
    "Depth (uniques first)"
};

static const score_run_detail_block* g_monster_sort_details = NULL;
static run_monster_sort_mode g_monster_sort_mode = RUN_MON_SORT_APPEARANCE;

static bool run_history_monster_is_unique(const score_run_monster_v1* entry)
{
    if (!entry || !r_info || !z_info)
        return false;
    if (entry->r_idx <= 0 || entry->r_idx >= z_info->r_max)
        return false;
    const monster_race* r_ptr = &r_info[entry->r_idx];
    return (r_ptr->flags1 & RF1_UNIQUE) != 0;
}

static int run_history_monster_level(const score_run_monster_v1* entry)
{
    if (!entry || !r_info || !z_info)
        return -1;
    if (entry->r_idx <= 0 || entry->r_idx >= z_info->r_max)
        return -1;
    return r_info[entry->r_idx].level;
}

static int run_history_compare_monsters(const void* va, const void* vb)
{
    int ia = *(const int*)va;
    int ib = *(const int*)vb;
    const score_run_monster_v1* ma = &g_monster_sort_details->monsters[ia];
    const score_run_monster_v1* mb = &g_monster_sort_details->monsters[ib];

    if (g_monster_sort_mode == RUN_MON_SORT_DEPTH) {
        int a_unique = run_history_monster_is_unique(ma) ? 1 : 0;
        int b_unique = run_history_monster_is_unique(mb) ? 1 : 0;
        if (a_unique != b_unique)
            return (b_unique - a_unique);

        int a_level = run_history_monster_level(ma);
        int b_level = run_history_monster_level(mb);
        if (a_level != b_level)
            return (b_level - a_level);
    }

    if (ia != ib)
        return (ia < ib) ? -1 : 1;
    return 0;
}

static int* run_history_build_monster_order(const score_run_detail_block* details,
                                            run_monster_sort_mode mode,
                                            int total)
{
    if (total <= 0)
        return NULL;
    int* order = mem_alloc_array(total, int);
    if (!order)
        return NULL;
    for (int i = 0; i < total; i++)
        order[i] = i;
    if (mode == RUN_MON_SORT_APPEARANCE)
        return order;

    g_monster_sort_details = details;
    g_monster_sort_mode = mode;
    qsort(order, total, sizeof(int), run_history_compare_monsters);
    g_monster_sort_details = NULL;
    return order;
}

static int run_history_draw_general_panel(const score_record_v1* rec,
                                          const run_history_entry* entry,
                                          const char* player,
                                          const char* race_name,
                                          const char* status_label,
                                          const char* created,
                                          const char* completed,
                                          bool current_run,
                                          int scroll_top,
                                          int term_hgt,
                                          int term_wid)
{
    char line[160];
    run_detail_text_view view;

    byte status_color = (rec->status == SCORE_RECORD_ALIVE) ? TERM_L_GREEN :
                        (rec->status == SCORE_RECORD_DEAD) ? TERM_L_RED : TERM_ORANGE;

    run_detail_text_view_init(&view, 3, run_history_detail_body_rows(term_hgt, 3),
        scroll_top, term_wid);

    strnfmt(line, sizeof(line), "Player:      %s", player);
    run_detail_text_view_put(&view, TERM_L_WHITE, line);

    strnfmt(line, sizeof(line), "Race:        %s", race_name);
    run_detail_text_view_put(&view, TERM_L_WHITE, line);

    strnfmt(line, sizeof(line), "Status:      %s", status_label);
    run_detail_text_view_put(&view, status_color, line);

    strnfmt(line, sizeof(line), "Rating:      %d points", entry->rating);
    run_detail_text_view_put(&view, TERM_SLATE, line);
    run_detail_text_view_blank(&view);

    if (!current_run) {
        strnfmt(line, sizeof(line), "Started:     %s", created);
        run_detail_text_view_put(&view, TERM_L_DARK, line);
        strnfmt(line, sizeof(line), "Completed:   %s", completed);
        run_detail_text_view_put(&view, TERM_L_DARK, line);
    } else {
        strnfmt(line, sizeof(line), "Started:     %s  (Run in progress)", created);
        run_detail_text_view_put(&view, TERM_L_GREEN, line);
    }
    run_detail_text_view_blank(&view);

    strnfmt(line, sizeof(line), "Max depth:      %d ft", rec->max_depth * 50);
    run_detail_text_view_put(&view, TERM_L_WHITE, line);

    strnfmt(line, sizeof(line), "Current depth:  %d ft", rec->exit_depth * 50);
    run_detail_text_view_put(&view, TERM_L_WHITE, line);

    byte sil_color = (rec->silmarils > 0) ? TERM_VIOLET : TERM_L_DARK;
    strnfmt(line, sizeof(line), "Silmarils:      %u", (unsigned)rec->silmarils);
    run_detail_text_view_put(&view, sil_color, line);
    run_detail_text_view_blank(&view);

    strnfmt(line, sizeof(line), "Quests completed:     %u", (unsigned)rec->quests_completed);
    run_detail_text_view_put(&view, TERM_L_WHITE, line);

    byte unique_color = (rec->uniques_killed > 0) ? TERM_YELLOW : TERM_L_DARK;
    strnfmt(line, sizeof(line), "Uniques defeated:     %u", (unsigned)rec->uniques_killed);
    run_detail_text_view_put(&view, unique_color, line);

    byte art_color = (rec->artefacts_found > 0) ? TERM_YELLOW : TERM_L_DARK;
    strnfmt(line, sizeof(line), "Artefacts found:      %u", (unsigned)rec->artefacts_found);
    run_detail_text_view_put(&view, art_color, line);

    strnfmt(line, sizeof(line), "Skills learned:       %u", (unsigned)rec->skills_learned);
    run_detail_text_view_put(&view, TERM_L_WHITE, line);

    strnfmt(line, sizeof(line), "Abilities learned:    %u", (unsigned)rec->abilities_learned);
    run_detail_text_view_put(&view, TERM_L_WHITE, line);
    run_detail_text_view_blank(&view);

    strnfmt(line, sizeof(line), "Monsters seen:        %lu", (unsigned long)rec->kills_seen);
    run_detail_text_view_put(&view, TERM_L_WHITE, line);

    strnfmt(line, sizeof(line), "Monsters killed:      %lu", (unsigned long)rec->kills_total);
    run_detail_text_view_put(&view, TERM_L_WHITE, line);

    strnfmt(line, sizeof(line), "Experience gained:    %lu", (unsigned long)rec->xp_earned);
    run_detail_text_view_put(&view, TERM_L_WHITE, line);

    strnfmt(line, sizeof(line), "Turns spent:          %lu", (unsigned long)rec->turns_spent);
    run_detail_text_view_put(&view, TERM_L_WHITE, line);

    if (rec->status == SCORE_RECORD_DEAD) {
        run_detail_text_view_blank(&view);
        strnfmt(line, sizeof(line), "Cause of death:  %s", rec->cause_of_death);
        run_detail_text_view_put(&view, TERM_L_RED, line);
    }

    return view.logical_row;
}

static int run_history_draw_stats_panel(const score_run_detail_block* details,
                                        int scroll_top,
                                        int term_hgt,
                                        int term_wid)
{
    const int stat_base_col = 14;
    const int stat_drain_col = 22;
    const int stat_current_col = 30;
    const int skill_base_col = 18;
    const int skill_current_col = 28;
    const int skill_stat_col = 38;
    const int skill_other_col = 46;
    run_detail_text_view view;

    run_detail_text_view_init(&view, 3, run_history_detail_body_rows(term_hgt, 3),
        scroll_top, term_wid);

    if (!details->stats || details->stats_count == 0) {
        run_detail_text_view_put(&view, TERM_L_DARK,
            "No stat data recorded for this run.");
        run_detail_text_view_blank(&view);
    } else {
        int y;

        run_detail_text_view_put(&view, TERM_L_BLUE, "=== Stats ===");
        y = run_detail_text_view_row_y(&view);
        if (y >= 0) {
            c_prt(TERM_L_UMBER, "Stat", y, 0);
            c_prt(TERM_L_UMBER, "Base", y, stat_base_col);
            c_prt(TERM_L_UMBER, "Drain", y, stat_drain_col);
            c_prt(TERM_L_UMBER, "Current", y, stat_current_col);
        }
        run_detail_text_view_advance(&view);
        for (u16b i = 0; i < details->stats_count; i++) {
            const score_run_stat_v1* entry = &details->stats[i];
            const char* label = (entry->stat_index < A_MAX)
                ? stat_names_full[entry->stat_index] : "<unknown>";
            y = run_detail_text_view_row_y(&view);
            if (y >= 0) {
                c_prt(TERM_L_WHITE, label, y, 0);
                c_prt(TERM_L_WHITE, format("%5d", entry->base), y,
                    stat_base_col);
                c_prt(TERM_L_WHITE, format("%5d", entry->drain), y,
                    stat_drain_col);
                c_prt(TERM_L_WHITE, format("%7d", entry->current), y,
                    stat_current_col);
            }
            run_detail_text_view_advance(&view);
        }
        run_detail_text_view_blank(&view);
    }

    if (!details->skills || details->skills_count == 0) {
        run_detail_text_view_put(&view, TERM_L_DARK,
            "No skill data recorded for this run.");
    } else {
        int y;

        run_detail_text_view_put(&view, TERM_L_BLUE, "=== Skills ===");
        y = run_detail_text_view_row_y(&view);
        if (y >= 0) {
            c_prt(TERM_L_UMBER, "Skill", y, 0);
            c_prt(TERM_L_UMBER, "Base", y, skill_base_col);
            c_prt(TERM_L_UMBER, "Current", y, skill_current_col);
            c_prt(TERM_L_UMBER, "Stat", y, skill_stat_col);
            c_prt(TERM_L_UMBER, "Other", y, skill_other_col);
        }
        run_detail_text_view_advance(&view);
        for (u16b i = 0; i < details->skills_count; i++) {
            const score_run_skill_v1* entry = &details->skills[i];
            const char* label = (entry->skill_index < S_MAX)
                ? skill_names_full[entry->skill_index] : "<unknown>";
            y = run_detail_text_view_row_y(&view);
            if (y >= 0) {
                c_prt(TERM_L_WHITE, label, y, 0);
                c_prt(TERM_L_WHITE, format("%5d", entry->base), y,
                    skill_base_col);
                c_prt(TERM_L_WHITE, format("%7d", entry->current), y,
                    skill_current_col);
                c_prt(TERM_L_WHITE, format("%5d", entry->stat_bonus), y,
                    skill_stat_col);
                c_prt(TERM_L_WHITE, format("%5d", entry->item_bonus), y,
                    skill_other_col);
            }
            run_detail_text_view_advance(&view);
        }
    }

    return view.logical_row;
}

static void run_history_clamp_list_state(run_detail_list_state* state,
                                         int rows, int total)
{
    if (total <= 0) {
        state->top = 0;
        state->highlight = 0;
        return;
    }
    if (rows < 1)
        rows = 1;
    if (state->highlight < 0)
        state->highlight = 0;
    if (state->highlight >= total)
        state->highlight = total - 1;
    if (state->top < 0)
        state->top = 0;
    if (state->top > total - rows)
        state->top = MAX(total - rows, 0);
    if (state->highlight < state->top)
        state->top = state->highlight;
    if (state->highlight >= state->top + rows)
        state->top = state->highlight - rows + 1;
}

static bool run_history_handle_list_key(run_detail_list_state* state,
                                        int ch, int rows, int total)
{
    if (total <= 0)
        return false;

    switch (ch) {
    case '2':
    case 'j':
    case 'J':
#ifdef ARROW_DOWN
    case ARROW_DOWN:
#endif
        state->highlight++;
        break;
    case '8':
    case 'k':
    case 'K':
#ifdef ARROW_UP
    case ARROW_UP:
#endif
        state->highlight--;
        break;
    case '3':
    case 'n':
    case 'N':
        state->highlight += rows;
        break;
    case '-':
    case '7':
    case 'p':
    case 'P':
        state->highlight -= rows;
        break;
    default:
        return false;
    }

    run_history_clamp_list_state(state, rows, total);
    return true;
}

static int run_history_detail_panel_total(const score_run_detail_block* details,
                                          run_detail_panel panel)
{
    if (!details)
        return 0;

    switch (panel) {
    case RUN_PANEL_ABILITIES:
        return details->ability_count;
    case RUN_PANEL_MILESTONES:
        return details->milestone_count;
    case RUN_PANEL_ARTEFACTS:
        return MIN(details->header.artefact_count,
            details->header.artefact_capacity);
    case RUN_PANEL_MONSTERS:
        return MIN(details->header.monster_count,
            details->header.monster_capacity);
    default:
        return 0;
    }
}

static run_detail_list_state* run_history_detail_panel_state(
    run_detail_view_state* view, run_detail_panel panel)
{
    if (!view)
        return NULL;

    switch (panel) {
    case RUN_PANEL_ABILITIES:
        return &view->abilities;
    case RUN_PANEL_MILESTONES:
        return &view->milestones;
    case RUN_PANEL_ARTEFACTS:
        return &view->artefacts;
    case RUN_PANEL_MONSTERS:
        return &view->monsters;
    default:
        return NULL;
    }
}

static const char* run_history_format_depth_label(const score_run_milestone_v1* entry,
                                                  char* buffer, size_t len)
{
    if (entry->depth_label[0]) {
        SDL_strlcpy(buffer, entry->depth_label, len);
        return buffer;
    }
    int feet = entry->depth * 50;
    if (feet <= 0) {
        SDL_strlcpy(buffer, "-", len);
        return buffer;
    }
    strnfmt(buffer, len, "%5d ft", feet);
    return buffer;
}

static int run_history_draw_abilities_panel(const score_run_detail_block* details,
                                            run_detail_list_state* state,
                                            int term_hgt)
{
    int total = details->ability_count;
    int rows = run_history_detail_body_rows(term_hgt, 5);

    if (total <= 0) {
        c_prt(TERM_L_DARK, "No ability timeline recorded for this run.", 3, 0);
        return rows;
    }

    run_history_clamp_list_state(state, rows, total);

    c_prt(TERM_L_BLUE, format("=== Abilities Acquired (%d total) ===", total), 2, 0);
    c_prt(TERM_L_UMBER, "Seq  Turn     Depth    Ability", 4, 0);

    for (int row = 0; row < rows; row++) {
        int idx = state->top + row;
        if (idx >= total)
            break;
        const score_run_ability_v1* entry = &details->abilities[idx];
        const char* skill = (entry->skill_index < S_MAX)
            ? skill_names_full[entry->skill_index] : "<unknown skill>";
        const char* ability_name = "<unknown ability>";
        if (entry->skill_index < S_MAX && entry->ability_index < ABILITIES_MAX) {
            ability_type* b_ptr = &b_info[ability_index(entry->skill_index, entry->ability_index)];
            if (b_ptr && b_ptr->name && b_name)
                ability_name = b_name + b_ptr->name;
        }
        byte color = (idx == state->highlight) ? TERM_L_GREEN : TERM_L_WHITE;
        char depth_buf[16];
        strnfmt(depth_buf, sizeof(depth_buf), "%5d ft", entry->depth * 50);
        ui_menu_click_add_full_row(idx, 5 + row);
        c_prt(color,
            format("%3u  %7lu  %-7s  %s - %s",
                entry->order,
                (unsigned long)entry->player_turn,
                depth_buf,
                skill,
                ability_name),
            5 + row, 0);
    }

    return rows;
}

static int run_history_draw_milestones_panel(const score_run_detail_block* details,
                                             run_detail_list_state* state,
                                             int term_hgt)
{
    int total = details->milestone_count;
    int rows = run_history_detail_body_rows(term_hgt, 5);

    if (total <= 0 || !details->milestones) {
        c_prt(TERM_L_DARK, "No milestone log recorded for this run.", 3, 0);
        return rows;
    }

    run_history_clamp_list_state(state, rows, total);
    c_prt(TERM_L_BLUE, "=== Milestones ===", 2, 0);
    c_prt(TERM_L_UMBER, "Turn      Depth    Note", 4, 0);

    for (int row = 0; row < rows; row++) {
        int idx = state->top + row;
        if (idx >= total)
            break;
        const score_run_milestone_v1* entry = &details->milestones[idx];
        char depth_buf[16];
        const char* depth = run_history_format_depth_label(entry, depth_buf, sizeof(depth_buf));
        byte color = (idx == state->highlight) ? TERM_L_GREEN : TERM_L_WHITE;
        ui_menu_click_add_full_row(idx, 5 + row);
        c_prt(color,
            format("%7lu  %-8s  %-60.60s",
                (unsigned long)entry->player_turn,
                depth,
                entry->note[0] ? entry->note : "(no note)"),
            5 + row, 0);
    }

    return rows;
}

static void run_history_examine_artefact(const score_run_detail_block* details,
                                         const run_detail_list_state* state)
{
    int total = details->header.artefact_count;
    if (total > details->header.artefact_capacity)
        total = details->header.artefact_capacity;
    int idx = state->highlight;
    if (idx < 0 || idx >= total)
        return;
    const score_run_artefact_v1* entry = &details->artefacts[idx];
    object_type fake_obj;
    if (run_history_prepare_artefact_object(entry, &fake_obj)) {
        object_info_screen(&fake_obj);
    } else {
        bell("Artefact information not available.");
    }
}

static void run_history_examine_monster(const score_run_detail_block* details,
                                        const run_detail_list_state* state,
                                        run_monster_sort_mode mode)
{
    int total = details->header.monster_count;
    if (total > details->header.monster_capacity)
        total = details->header.monster_capacity;
    int idx = state->highlight;
    if (idx < 0 || idx >= total)
        return;

    int* order = run_history_build_monster_order(details, mode, total);
    if (!order)
        return;
    const score_run_monster_v1* entry = &details->monsters[order[idx]];
    mem_free(order);

    if (z_info && entry->r_idx > 0 && entry->r_idx < z_info->r_max) {
        screen_save();
        if (!screen_roff(entry->r_idx, NULL))
            (void)inkey();
        screen_load();
    } else {
        bell("Monster information not available.");
    }
}

static int run_history_draw_artefact_panel(const score_run_detail_block* details,
                                           run_detail_list_state* state,
                                           int term_hgt)
{
    int total = details->header.artefact_count;
    if (total > details->header.artefact_capacity)
        total = details->header.artefact_capacity;
    int rows = run_history_detail_body_rows(term_hgt, 5);

    if (total <= 0 || !details->artefacts) {
        c_prt(TERM_L_DARK, "No artefact data recorded for this run.", 3, 0);
        return rows;
    }

    run_history_clamp_list_state(state, rows, total);
    c_prt(TERM_L_BLUE, format("=== Artefacts Recovered (%d total) ===", total), 2, 0);
    c_prt(TERM_L_UMBER, "Artefact", 4, 4);

    for (int row = 0; row < rows; row++) {
        int idx = state->top + row;
        if (idx >= total)
            break;
        const score_run_artefact_v1* entry = &details->artefacts[idx];
        char full_desc[120] = "<unknown artefact>";
        byte color = TERM_WHITE;
        byte pict_attr = TERM_WHITE;
        char pict_char = '?';
        object_type temp_obj;
        if (run_history_prepare_artefact_object(entry, &temp_obj)) {
            color = TERM_YELLOW;
            object_desc(full_desc, sizeof(full_desc), &temp_obj, true, 0);
            pict_attr = object_attr(&temp_obj);
            pict_char = object_char(&temp_obj);
        } else if (z_info && entry->a_idx > 0 && entry->a_idx < z_info->art_max) {
            artefact_type* art = &a_info[entry->a_idx];
            if (art && art->name[0])
                SDL_strlcpy(full_desc, art->name, sizeof(full_desc));
        }

        bool selected = (idx == state->highlight);
        int y = 5 + row;
        c_prt(selected ? TERM_L_GREEN : color, selected ? ">" : " ", y, 0);
        ui_menu_click_add_full_row(idx, y);
        Term_putch(2, y, pict_attr, pict_char);
        if (use_bigtile)
            Term_putch(3, y, 255, -1);
        c_prt(selected ? TERM_L_GREEN : color,
            format("%-72.72s", full_desc), y, 4);
    }

    return rows;
}

static int run_history_draw_monster_panel(const score_run_detail_block* details,
                                          run_detail_list_state* state,
                                          run_monster_sort_mode sort_mode,
                                          int term_hgt)
{
    int term_wid = 80;
    int total = details->header.monster_count;
    if (total > details->header.monster_capacity)
        total = details->header.monster_capacity;
    int rows = run_history_detail_body_rows(term_hgt, 5);

    if (total <= 0 || !details->monsters) {
        c_prt(TERM_L_DARK, "No monster encounters were tracked for this run.", 3, 0);
        return rows;
    }

    run_history_clamp_list_state(state, rows, total);
    int* order = run_history_build_monster_order(details, sort_mode, total);
    if (!order) {
        c_prt(TERM_L_DARK, "Unable to build monster list.", 3, 0);
        return rows;
    }

    score_ui_get_term_size(&term_wid, NULL);

    c_prt(TERM_L_BLUE,
          format("=== Monster Encounters (%d total, %s) ===",
                 total, run_history_monster_sort_labels[sort_mode]),
          2, 0);
    c_prt(TERM_L_UMBER, "Monster", 4, 2);
    if (score_ui_compact_width(term_wid)) {
        int seen_col = term_wid - 10;
        int slain_col = term_wid - 5;
        c_prt(TERM_L_UMBER, "Seen", 4, seen_col);
        c_prt(TERM_L_UMBER, "Slay", 4, slain_col);
    } else {
        c_prt(TERM_L_UMBER, "Seen", 4, 58);
        c_prt(TERM_L_UMBER, "Slain", 4, 68);
    }

    for (int row = 0; row < rows; row++) {
        int idx = state->top + row;
        if (idx >= total)
            break;
        const score_run_monster_v1* entry = &details->monsters[order[idx]];
        const char* name = run_history_monster_name(entry->r_idx);
        byte pic_color = TERM_WHITE;
        char pic_char = '?';
        if (z_info && entry->r_idx > 0 && entry->r_idx < z_info->r_max) {
            monster_race* r_ptr = &r_info[entry->r_idx];
            pic_char = monster_char(r_ptr);
            pic_color = monster_attr(r_ptr);
        }

        byte color = (entry->killed > 0) ? TERM_L_GREEN : TERM_L_WHITE;
        byte display_color = (idx == state->highlight) ? TERM_YELLOW : color;
        int y = 5 + row;
        bool compact = score_ui_compact_width(term_wid);
        c_prt(display_color, (idx == state->highlight) ? ">" : " ", y, 0);
        ui_menu_click_add_full_row(idx, y);
        Term_putch(2, y, pic_color, pic_char);
        if (use_bigtile) {
            Term_putch(3, y, 255, -1);
        }
        if (compact) {
            int seen_col = term_wid - 10;
            int slain_col = term_wid - 5;
            int name_width = seen_col - 5;
            if (name_width < 8)
                name_width = 8;
            Term_putstr(4, y, name_width, display_color, name);
            c_prt(display_color, format("%4u", (unsigned)entry->seen), y,
                seen_col);
            c_prt(display_color, format("%4u", (unsigned)entry->killed), y,
                slain_col);
        } else {
            c_prt(display_color, format("%-52.52s", name), y, 4);
            c_prt(display_color, format("%5u", (unsigned)entry->seen), y, 58);
            c_prt(display_color, format("%5u", (unsigned)entry->killed), y, 68);
        }
    }

    mem_free(order);
    return rows;
}

static void run_history_show_detail(const run_history_entry* entry)
{
    if (!entry)
        return;

    const score_record_v1* rec = &entry->record;
    score_run_detail_block details;
    memset(&details, 0, sizeof(details));
    bool have_details = (entry->detail_offset >= 0)
        && score_runs_load_details(entry->detail_offset, &details);

    bool current_run = run_history_is_current(entry);
    if ((!have_details || details.header.monster_count == 0
            || details.header.artefact_count == 0)
        && current_run) {
        score_runs_free_details(&details);
        memset(&details, 0, sizeof(details));
        have_details = score_runs_snapshot_details(&details);
        if (!have_details)
            log_warn("run_history: unable to hydrate live detail payload");
    }

    char player[33];
    if (rec->player_name[0]) {
        SDL_strlcpy(player, rec->player_name, sizeof(player));
    } else if (rec->savefile_hint[0]) {
        SDL_strlcpy(player, rec->savefile_hint, sizeof(player));
    } else {
        SDL_strlcpy(player, "<unknown>", sizeof(player));
    }

    char created[32], completed[32];
    run_history_format_timestamp(rec->created_utc, true, created, sizeof(created));
    run_history_format_timestamp(rec->completed_utc, true, completed, sizeof(completed));

    const char* status = score_run_status_label(rec->status);
    const char* race_name = run_history_race_name(rec->race_id);

    bool panel_has_data[RUN_PANEL_COUNT];
    panel_has_data[RUN_PANEL_GENERAL] = true;
    panel_has_data[RUN_PANEL_STATS] = true;
    panel_has_data[RUN_PANEL_ABILITIES] = have_details && details.ability_count > 0;
    panel_has_data[RUN_PANEL_MILESTONES] = have_details && details.milestone_count > 0;
    panel_has_data[RUN_PANEL_ARTEFACTS] = have_details && details.header.artefact_count > 0;
    panel_has_data[RUN_PANEL_MONSTERS] = have_details && details.header.monster_count > 0;

    run_detail_panel panel = RUN_PANEL_GENERAL;
    run_detail_view_state view = {0};
    bool done = false;
    int term_hgt = 24;
    int term_wid = 80;
    int general_total_lines = 0;
    int stats_total_lines = 0;

    screen_save();

    while (!done) {
        bool steamdeck = steamdeck_controls_active();
        char confirm_label[16] = "";
        char back_label[16] = "";
        char sort_label[16] = "";
        char footer_buf[192] = "";
        int active_list_rows = 0;
        int active_list_total = 0;
        int scroll_first_row = 3;
        int scroll_rows = 0;
        bool enable_scroll_area = false;

        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_scroll_area_clear();

        if (steamdeck) {
            score_prompt_label(steamdeck_confirm_key(), "A",
                confirm_label, sizeof(confirm_label));
            score_prompt_label(steamdeck_back_key(), "B",
                back_label, sizeof(back_label));
            score_prompt_label(steamdeck_secondary_key(), "Y",
                sort_label, sizeof(sort_label));
        }

        score_ui_get_term_size(&term_wid, &term_hgt);
        run_history_draw_panel_tabs(panel, panel_has_data);

        const char* footer = NULL;
        int text_rows = run_history_detail_body_rows(term_hgt, 3);
        int artefact_rows = 0;
        int monster_rows = 0;
        int ability_rows = 0;
        int milestone_rows = 0;

        switch (panel) {
        case RUN_PANEL_GENERAL:
            c_prt(TERM_L_BLUE, format("=== Run #%u Details ===", rec->record_id), 2, 0);
            run_history_clamp_scroll(&view.general_top, text_rows,
                general_total_lines);
            general_total_lines = run_history_draw_general_panel(rec, entry,
                player, race_name, status, created, completed, current_run,
                view.general_top, term_hgt, term_wid);
            footer = steamdeck
                ? ((general_total_lines > text_rows)
                    ? "[Up/Down] scroll  [Left/Right] view  [%s] back"
                    : "[Left/Right] view  [%s] back")
                : ((general_total_lines > text_rows)
                    ? "[Up/Down] scroll  [Left/Right] view  [Esc] back"
                    : "[Left/Right] view  [Esc] back");
            scroll_first_row = 3;
            scroll_rows = text_rows;
            enable_scroll_area = true;
            break;
        case RUN_PANEL_STATS:
            c_prt(TERM_L_BLUE, format("=== Run #%u Details ===", rec->record_id), 2, 0);
            run_history_clamp_scroll(&view.stats_top, text_rows,
                stats_total_lines);
            stats_total_lines = run_history_draw_stats_panel(&details,
                view.stats_top, term_hgt, term_wid);
            footer = steamdeck
                ? ((stats_total_lines > text_rows)
                    ? "[Up/Down] scroll  [Left/Right] view  [%s] back"
                    : "[Left/Right] view  [%s] back")
                : ((stats_total_lines > text_rows)
                    ? "[Up/Down] scroll  [Left/Right] view  [Esc] back"
                    : "[Left/Right] view  [Esc] back");
            scroll_first_row = 3;
            scroll_rows = text_rows;
            enable_scroll_area = true;
            break;
        case RUN_PANEL_ABILITIES:
            ability_rows = run_history_draw_abilities_panel(&details, &view.abilities, term_hgt);
            footer = steamdeck
                ? "[Up/Down] navigate  [Left/Right] view  [%s] back"
                : "[Up/Down] navigate  [Left/Right] view  [Esc] back";
            active_list_rows = ability_rows;
            active_list_total = run_history_detail_panel_total(&details, panel);
            scroll_first_row = 5;
            scroll_rows = ability_rows;
            enable_scroll_area = active_list_total > 0;
            break;
        case RUN_PANEL_MILESTONES:
            milestone_rows = run_history_draw_milestones_panel(&details, &view.milestones, term_hgt);
            footer = steamdeck
                ? "[Up/Down] navigate  [Left/Right] view  [%s] back"
                : "[Up/Down] navigate  [Left/Right] view  [Esc] back";
            active_list_rows = milestone_rows;
            active_list_total = run_history_detail_panel_total(&details, panel);
            scroll_first_row = 5;
            scroll_rows = milestone_rows;
            enable_scroll_area = active_list_total > 0;
            break;
        case RUN_PANEL_ARTEFACTS:
            artefact_rows = run_history_draw_artefact_panel(&details, &view.artefacts, term_hgt);
            footer = steamdeck
                ? "[Up/Down] navigate  [Left/Right] view  [%s] inspect  [%s] back"
                : "[Up/Down] navigate  [Space/Enter] inspect  [Left/Right] view  [Esc] back";
            active_list_rows = artefact_rows;
            active_list_total = run_history_detail_panel_total(&details, panel);
            scroll_first_row = 5;
            scroll_rows = artefact_rows;
            enable_scroll_area = active_list_total > 0;
            break;
        case RUN_PANEL_MONSTERS:
            monster_rows = run_history_draw_monster_panel(&details, &view.monsters,
                view.monster_sort_mode, term_hgt);
            footer = steamdeck
                ? "[Up/Down] navigate  [Left/Right] view  [%s] inspect  [%s] sort  [%s] back"
                : "[Up/Down] navigate  [Space/Enter] inspect  [S] sort  [Left/Right] view  [Esc] back";
            active_list_rows = monster_rows;
            active_list_total = run_history_detail_panel_total(&details, panel);
            scroll_first_row = 5;
            scroll_rows = monster_rows;
            enable_scroll_area = active_list_total > 0;
            break;
        default:
            break;
        }

        if (footer) {
            if (steamdeck) {
                if (panel == RUN_PANEL_GENERAL || panel == RUN_PANEL_STATS ||
                    panel == RUN_PANEL_ABILITIES || panel == RUN_PANEL_MILESTONES) {
                    strnfmt(footer_buf, sizeof(footer_buf), footer, back_label);
                } else if (panel == RUN_PANEL_ARTEFACTS) {
                    strnfmt(footer_buf, sizeof(footer_buf), footer,
                        confirm_label, back_label);
                } else if (panel == RUN_PANEL_MONSTERS) {
                    strnfmt(footer_buf, sizeof(footer_buf), footer,
                        confirm_label, sort_label, back_label);
                }
            } else {
                SDL_strlcpy(footer_buf, footer, sizeof(footer_buf));
            }

            score_ui_put_fit(TERM_L_DARK, footer_buf, term_hgt - 2, 0,
                term_wid);
            ui_menu_click_add_text_token(RUN_DETAIL_CLICK_PREV_PANEL, 0,
                term_hgt - 2, footer_buf, "Left");
            ui_menu_click_add_text_token(RUN_DETAIL_CLICK_NEXT_PANEL, 0,
                term_hgt - 2, footer_buf, "Right");
            ui_menu_click_add_text_token(RUN_DETAIL_CLICK_BACK, 0,
                term_hgt - 2, footer_buf, "back");
            if (panel == RUN_PANEL_ARTEFACTS || panel == RUN_PANEL_MONSTERS) {
                ui_menu_click_add_text_token(RUN_DETAIL_CLICK_INSPECT, 0,
                    term_hgt - 2, footer_buf, "inspect");
            }
            if (panel == RUN_PANEL_MONSTERS) {
                ui_menu_click_add_text_token(RUN_DETAIL_CLICK_SORT, 0,
                    term_hgt - 2, footer_buf, "sort");
            }
        }

        if (enable_scroll_area) {
            ui_scroll_area_begin(scroll_first_row,
                scroll_first_row + scroll_rows - 1,
                SDL_TOUCH_MENU_CATEGORY_OTHER);
            ui_scroll_area_set_keys('8', '2', '6', '4');
        }
        (void)Term_set_cursor(false);
        Term_fresh();

        bool saved_hide_cursor = hide_cursor;
        hide_cursor = true;
        int ch = inkey();
        hide_cursor = saved_hide_cursor;
        bool skip_command = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action)) {
                run_detail_panel clicked_panel;
                run_detail_list_state* clicked_state =
                    run_history_detail_panel_state(&view, panel);

                if (click_action == UI_MENU_CLICK_HOVER) {
                    if (clicked_choice >= 0 && clicked_state
                        && clicked_choice < active_list_total)
                    {
                        clicked_state->highlight = clicked_choice;
                        run_history_clamp_list_state(clicked_state,
                            active_list_rows, active_list_total);
                    }
                    skip_command = true;
                } else if (run_history_detail_choice_to_tab(clicked_choice,
                           &clicked_panel)) {
                    panel = clicked_panel;
                    skip_command = true;
                } else if (clicked_choice >= 0 && clicked_state
                           && clicked_choice < active_list_total) {
                    if (click_action == UI_MENU_CLICK_SECONDARY
                        && (panel == RUN_PANEL_ARTEFACTS ||
                            panel == RUN_PANEL_MONSTERS))
                    {
                        clicked_state->highlight = clicked_choice;
                        run_history_clamp_list_state(clicked_state,
                            active_list_rows, active_list_total);
                        ch = '\r';
                    } else if (clicked_state->highlight != clicked_choice) {
                        clicked_state->highlight = clicked_choice;
                        run_history_clamp_list_state(clicked_state,
                            active_list_rows, active_list_total);
                        skip_command = true;
                    } else if (panel == RUN_PANEL_ARTEFACTS ||
                               panel == RUN_PANEL_MONSTERS) {
                        ch = '\r';
                    } else {
                        skip_command = true;
                    }
                } else {
                    switch (clicked_choice) {
                    case RUN_DETAIL_CLICK_BACK:
                        ch = ESCAPE;
                        break;
                    case RUN_DETAIL_CLICK_PREV_PANEL:
                        ch = '4';
                        break;
                    case RUN_DETAIL_CLICK_NEXT_PANEL:
                        ch = '6';
                        break;
                    case RUN_DETAIL_CLICK_INSPECT:
                        ch = '\r';
                        break;
                    case RUN_DETAIL_CLICK_SORT:
                        ch = 's';
                        break;
                    default:
                        skip_command = true;
                        break;
                    }
                }
            } else if (ch == UI_MENU_CLICK_WAKE_KEY) {
                skip_command = true;
            }

            ui_menu_click_clear();
            ui_scroll_area_clear();
        }

        if (skip_command)
            continue;

        if (steamdeck) {
            if (ch == steamdeck_back_key())
                ch = ESCAPE;
            else if (ch == steamdeck_confirm_key())
                ch = '\r';
            else if (ch == steamdeck_secondary_key())
                ch = 's';
        }

        switch (ch) {
        case ESCAPE:
        case 'q':
        case 'Q':
            done = true;
            break;
        case '4':
#ifdef ARROW_LEFT
        case ARROW_LEFT:
#endif
        case 'h':
        case 'H':
            panel = (run_detail_panel)((panel + RUN_PANEL_COUNT - 1) % RUN_PANEL_COUNT);
            break;
        case '6':
#ifdef ARROW_RIGHT
        case ARROW_RIGHT:
#endif
        case 'l':
        case 'L':
            panel = (run_detail_panel)((panel + 1) % RUN_PANEL_COUNT);
            break;
        default: {
            bool handled = false;
            switch (panel) {
            case RUN_PANEL_GENERAL:
                handled = run_history_handle_scroll_key(&view.general_top, ch,
                    text_rows, general_total_lines);
                break;
            case RUN_PANEL_STATS:
                handled = run_history_handle_scroll_key(&view.stats_top, ch,
                    text_rows, stats_total_lines);
                break;
            case RUN_PANEL_ABILITIES:
                handled = run_history_handle_list_key(&view.abilities, ch,
                    ability_rows, details.ability_count);
                break;
            case RUN_PANEL_MILESTONES:
                handled = run_history_handle_list_key(&view.milestones, ch,
                    milestone_rows, details.milestone_count);
                break;
            case RUN_PANEL_ARTEFACTS:
                if (ch == ' ' || ch == '\r' || ch == '\n' ||
                    ch == 'x' || ch == 'X' || ch == 'r' || ch == 'R') {
                    run_history_examine_artefact(&details, &view.artefacts);
                    handled = true;
                } else {
                    int total = details.header.artefact_count;
                    if (total > details.header.artefact_capacity)
                        total = details.header.artefact_capacity;
                    handled = run_history_handle_list_key(&view.artefacts, ch,
                        artefact_rows, total);
                }
                break;
            case RUN_PANEL_MONSTERS:
                if (ch == ' ' || ch == '\r' || ch == '\n' ||
                    ch == 'x' || ch == 'X' || ch == 'r' || ch == 'R') {
                    run_history_examine_monster(&details, &view.monsters,
                        view.monster_sort_mode);
                    handled = true;
                } else if (ch == 's' || ch == 'S') {
                    view.monster_sort_mode =
                        (run_monster_sort_mode)((view.monster_sort_mode + 1) % RUN_MON_SORT_COUNT);
                    run_history_clamp_list_state(&view.monsters, monster_rows,
                        MIN(details.header.monster_count, details.header.monster_capacity));
                    handled = true;
                } else {
                    int total = details.header.monster_count;
                    if (total > details.header.monster_capacity)
                        total = details.header.monster_capacity;
                    handled = run_history_handle_list_key(&view.monsters, ch,
                        monster_rows, total);
                }
                break;
            default:
                handled = false;
                break;
            }
            if (!handled)
                bell("Unknown command.");
            break;
        }
        }
    }

    ui_menu_click_clear();
    ui_scroll_area_clear();

    if (have_details)
        score_runs_free_details(&details);

    screen_load();
}
