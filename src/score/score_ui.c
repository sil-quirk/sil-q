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

/* Final Look deliberately presents live-game UI flags.  Score and run-history
 * code must use the finalized run state instead of those presentation flags. */
static bool score_ui_character_finalized(void)
{
    return p_ptr && (p_ptr->is_dead || death_spectator_active());
}

static void run_history_refresh_active_run(void)
{
    if (!character_generated || !p_ptr || score_ui_character_finalized())
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
    if (!character_generated || !p_ptr || score_ui_character_finalized())
        return false;
    if (entry->record.status != SCORE_RECORD_ALIVE)
        return false;
    return (entry->record.metarun_id == metar.id);
}

static high_score forced_highlight_entry;
static bool forced_highlight_active = false;
static bool score_last_layout_short = false;

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
static void score_ui_halls_date(const high_score* entry, char* out,
    size_t out_len)
{
    const char* when;

    if (!out || out_len == 0)
        return;
    out[0] = '\0';
    if (!entry)
        return;

    when = entry->day;
    while (*when && isspace((unsigned char)*when))
        when++;
    if (*when == '@' && strlen(when) == 9)
    {
        char month[4];

        strnfmt(month, sizeof(month), "%.2s", when + 5);
        atomonth(atoi(month), month);
        strnfmt(out, out_len, "%d %.3s %.4s", atoi(when + 7), month,
            when + 1);
        return;
    }
    SDL_strlcpy(out, when, out_len);
}

static void score_ui_append_points_factor(char* out, size_t out_len,
    bool* have_factor, cptr label, int points)
{
    char factor[96];

    if (!out || out_len == 0 || !have_factor || !label || points <= 0)
        return;
    strnfmt(factor, sizeof(factor), "%s%s +%d pts",
        *have_factor ? "  |  " : "", label, points);
    SDL_strlcat(out, factor, out_len);
    *have_factor = true;
}

static void score_ui_append_percent_factor(char* out, size_t out_len,
    bool* have_factor, cptr label, int delta_bp)
{
    char factor[96];
    int magnitude;

    if (!out || out_len == 0 || !have_factor || !label || delta_bp == 0)
        return;
    magnitude = ABS(delta_bp);
    strnfmt(factor, sizeof(factor), "%s%s %c%d.%d%%",
        *have_factor ? "  |  " : "", label, delta_bp > 0 ? '+' : '-',
        magnitude / 10, magnitude % 10);
    SDL_strlcat(out, factor, out_len);
    *have_factor = true;
}

static void score_ui_build_score_factors(const score_breakdown* breakdown,
    char* increases, size_t increases_len, char* decreases,
    size_t decreases_len)
{
    bool have_increase = false;
    bool have_decrease = false;
    char label[64];
    char formula[96];

    if (!breakdown || !increases || increases_len == 0 || !decreases
        || decreases_len == 0)
    {
        return;
    }

    SDL_strlcpy(increases, "Score increases: ", increases_len);
    score_ui_append_points_factor(increases, increases_len, &have_increase,
        "descent", breakdown->descent_points);
    score_ui_append_points_factor(increases, increases_len, &have_increase,
        "unique kills", breakdown->unique_points);
    score_ui_append_points_factor(increases, increases_len, &have_increase,
        "ascent", breakdown->ascent_points);
    score_ui_append_points_factor(increases, increases_len, &have_increase,
        "Silmarils", breakdown->silmaril_points);
    score_ui_append_points_factor(increases, increases_len, &have_increase,
        "Morgoth", breakdown->morgoth_points);
    score_ui_append_points_factor(increases, increases_len, &have_increase,
        breakdown->morgoth_slain && !breakdown->escaped
            ? "victory" : "escape",
        breakdown->escape_points);
    if (breakdown->character_mult_bp > 0)
        score_ui_append_percent_factor(increases, increases_len,
            &have_increase, "character challenge",
            breakdown->character_mult_bp);
    if (breakdown->curse_mult_bp > 0)
    {
        strnfmt(label, sizeof(label), "%d net curse%s", breakdown->curses,
            breakdown->curses == 1 ? "" : "s");
        score_ui_append_percent_factor(increases, increases_len,
            &have_increase, label, breakdown->curse_mult_bp);
    }
    if (!have_increase)
        SDL_strlcat(increases, "none", increases_len);

    SDL_strlcpy(decreases, "Score decreases: ", decreases_len);
    if (breakdown->character_mult_bp < 0)
        score_ui_append_percent_factor(decreases, decreases_len,
            &have_decrease, "character power",
            breakdown->character_mult_bp);
    if (breakdown->curse_mult_bp < 0)
    {
        int blessings = -breakdown->curses;

        strnfmt(label, sizeof(label), "%d net blessing%s", blessings,
            blessings == 1 ? "" : "s");
        score_ui_append_percent_factor(decreases, decreases_len,
            &have_decrease, label, breakdown->curse_mult_bp);
    }
    if (!have_decrease)
        SDL_strlcat(decreases, "none", decreases_len);

    strnfmt(formula, sizeof(formula),
        "  |  Formula: %d base x %d.%03d = %d pts",
        breakdown->base_score, breakdown->mult_bp / 1000,
        breakdown->mult_bp % 1000, breakdown->total_score);
    SDL_strlcat(decreases, formula, decreases_len);
}

static void score_ui_build_halls_card(const high_score* entry, int place,
    char* rank, size_t rank_len, char* name, size_t name_len,
    char* score, size_t score_len, char* outcome, size_t outcome_len,
    char* details, size_t details_len, char* honors, size_t honors_len,
    char* increases, size_t increases_len, char* decreases,
    size_t decreases_len)
{
    char score_commas[24];
    char turn_commas[24];
    char depth_commas[24];
    char deepest_commas[24];
    char date[32];
    int character = entry ? atoi(entry->p_h) : -1;
    int turns = entry ? atoi(entry->turns) : 0;
    int depth = entry ? atoi(entry->cur_dun) * 50 : 0;
    int deepest = entry ? atoi(entry->max_dun) * 50 : 0;
    int silmarils = entry
        ? parse_score_int(entry->silmarils, sizeof(entry->silmarils), 0) : 0;
    bool morgoth = entry && entry->morgoth_slain[0] == 't';
    bool escaped = entry && entry->escaped[0] == 't';
    bool alive = entry && streq(entry->how, "(alive and well)");
    cptr hero = (entry && entry->who[0]) ? entry->who : "Unknown hero";
    score_breakdown breakdown = score_calculate_breakdown(entry);

    strnfmt(rank, rank_len, "#%d", place);
    if (entry && z_info && c_info && c_name
        && character >= 0 && character < z_info->c_max)
    {
        strnfmt(name, name_len, "%s%s", hero,
            c_name + c_info[character].alt_name);
    }
    else
        SDL_strlcpy(name, hero, name_len);

    comma_number(score_commas, breakdown.total_score);
    strnfmt(score, score_len, "%s pts", score_commas);
    comma_number(turn_commas, turns);
    comma_number(depth_commas, depth);
    comma_number(deepest_commas, deepest);
    score_ui_halls_date(entry, date, sizeof(date));

    if (escaped)
    {
        if (silmarils > 0 || morgoth)
            SDL_strlcpy(outcome,
                "Escaped the iron hells and bore the light back to Valinor.",
                outcome_len);
        else
            SDL_strlcpy(outcome,
                "Escaped the iron hells of Angband empty-handed.",
                outcome_len);
    }
    else if (alive)
        SDL_strlcpy(outcome, "Lives still, deep within Angband's vaults.",
            outcome_len);
    else if (morgoth)
        strnfmt(outcome, outcome_len,
            "Victorious over Morgoth's illusion; fate: %s.",
            (entry && entry->how[0]) ? entry->how : "unknown");
    else
        strnfmt(outcome, outcome_len, "Slain by %s at %s ft.",
            (entry && entry->how[0]) ? entry->how : "an unknown doom",
            depth_commas);

    if (date[0])
        strnfmt(details, details_len,
            "%s turns  |  deepest descent %s ft  |  %s", turn_commas,
            deepest_commas, date);
    else
        strnfmt(details, details_len,
            "%s turns  |  deepest descent %s ft", turn_commas,
            deepest_commas);

    honors[0] = '\0';
    if (silmarils > 0)
        strnfmt(honors, honors_len, "%d Silmaril%s", silmarils,
            silmarils == 1 ? "" : "s");
    if (morgoth)
    {
        if (honors[0])
            SDL_strlcat(honors, "  |  Morgoth", honors_len);
        else
            SDL_strlcpy(honors, "Morgoth", honors_len);
    }
    if (escaped && !honors[0])
        SDL_strlcpy(honors, "Escaped", honors_len);
    else if (alive && !honors[0])
        SDL_strlcpy(honors, "Living", honors_len);

    score_ui_build_score_factors(&breakdown, increases, increases_len,
        decreases, decreases_len);
}

static char score_ui_finish_halls(char response)
{
    ui_menu_click_clear();
    sdl_halls_screen_hide();
    return response;
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
        SCORE_CLICK_NEXT = -6,
        SCORE_CLICK_HISTORY = -7
    };
    bool steamdeck = steamdeck_controls_active();
    int start_index = 0;

    if (!entries || count <= 0)
    {
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_outside_cancel_enabled(true);
        sdl_halls_screen_begin(
            "Here are remembered the fates of those who entered Angband.",
            "No memorials have yet been inscribed.", false,
            SCORE_CLICK_EXIT);
        sdl_halls_screen_set_empty("No recorded heroes yet.");
        sdl_halls_screen_add_action(SCORE_CLICK_HISTORY, "Run History",
            TERM_L_BLUE, true);
        sdl_halls_screen_add_action(SCORE_CLICK_EXIT, "Back", TERM_L_WHITE,
            true);
        Term_fresh();
        while (true)
        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;
            char ch;
            bool saved_hide_cursor = hide_cursor;
            hide_cursor = true;
            ch = inkey();
            hide_cursor = saved_hide_cursor;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                if (clicked_choice == SCORE_CLICK_HISTORY)
                    return score_ui_finish_halls('r');
                return score_ui_finish_halls(0);
            }

            if (ch == UI_MENU_CLICK_WAKE_KEY)
                continue;
            ch = (char)steamdeck_menu_key(ch, 0, 0);
            if ((steamdeck && ch == steamdeck_info_key())
                || ch == 'r' || ch == 'R' || ch == 'h' || ch == 'H')
                return score_ui_finish_halls('r');
            return score_ui_finish_halls(0);
        }
    }

    if (!highlight_index)
        return score_ui_finish_halls(0);
    if (*highlight_index < 0 || *highlight_index >= count)
        *highlight_index = 0;

    while (true)
    {
        int entries_per_page;
        int max_start;
        int page;
        int total_pages;
        bool has_more;
        bool has_prev;

        entries_per_page = sdl_halls_screen_page_capacity(detailed);
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

        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_outside_cancel_enabled(true);
        {
            char page_status[160];

            strnfmt(page_status, sizeof(page_status),
                "%s  |  %s memorials  |  page %d of %d",
                score_view_order_label(order), detailed ? "full" : "brief",
                page + 1, total_pages);
            sdl_halls_screen_begin(
                "Here are remembered the fates of those who entered Angband.",
                page_status, detailed, SCORE_CLICK_EXIT);
        }

        for (int row = 0; row < entries_per_page && (start_index + row) < count; row++)
        {
            int idx = start_index + row;
            bool is_highlight = (idx == *highlight_index);
            byte attr = score_entry_color(&entries[idx], false);
            char rank[16];
            char name[64];
            char score[32];
            char outcome[256];
            char details[256];
            char honors[96];
            char increases[256];
            char decreases[256];

            score_ui_build_halls_card(&entries[idx], idx + 1, rank,
                sizeof(rank), name, sizeof(name), score, sizeof(score),
                outcome, sizeof(outcome), details, sizeof(details), honors,
                sizeof(honors), increases, sizeof(increases), decreases,
                sizeof(decreases));
            sdl_halls_screen_add_entry(idx, rank, name, score, outcome,
                details, honors, increases, decreases, attr, is_highlight);
        }

        sdl_halls_screen_add_action(SCORE_CLICK_EXIT, "Back", TERM_L_WHITE,
            true);
        sdl_halls_screen_add_action(SCORE_CLICK_HISTORY, "Run History",
            TERM_L_BLUE, true);
        sdl_halls_screen_add_action(SCORE_CLICK_ORDER,
            order == SCORE_VIEW_ORDER_SCORE ? "Order: Score" : "Order: Date",
            TERM_L_WHITE, true);
        sdl_halls_screen_add_action(SCORE_CLICK_LAYOUT,
            detailed ? "View: Full" : "View: Brief", TERM_L_WHITE, true);
        if (!sdl_touch_only_device_active())
            sdl_halls_screen_add_action(SCORE_CLICK_OPEN, "Open Hero",
                TERM_L_BLUE, true);
        sdl_halls_screen_add_action(SCORE_CLICK_PREV, "Previous", TERM_SLATE,
            has_prev);
        sdl_halls_screen_add_action(SCORE_CLICK_NEXT, "Next", TERM_SLATE,
            has_more);
        Term_fresh();

        bool saved_hide_cursor = hide_cursor;
        hide_cursor = true;
        char ch = inkey();
        hide_cursor = saved_hide_cursor;
        bool click_generated_command = false;

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
                    sdl_halls_screen_hide();
                    if (!show_run_history_detail_for_score(&entries[*highlight_index]))
                        bell("No run history is available for that character.");
                    continue;
                }

                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;

                switch (clicked_choice)
                {
                case SCORE_CLICK_ORDER: ch = 's'; click_generated_command = true; break;
                case SCORE_CLICK_LAYOUT: ch = 'l'; click_generated_command = true; break;
                case SCORE_CLICK_EXIT: ch = ESCAPE; click_generated_command = true; break;
                case SCORE_CLICK_OPEN: ch = '\r'; click_generated_command = true; break;
                case SCORE_CLICK_HISTORY: ch = 'r'; click_generated_command = true; break;
                case SCORE_CLICK_PREV: ch = 'p'; click_generated_command = true; break;
                case SCORE_CLICK_NEXT: ch = 'n'; click_generated_command = true; break;
                default: break;
                }
            }
            else
                ui_menu_click_clear();
        }

        if (ch == UI_MENU_CLICK_WAKE_KEY)
            continue;

        if (!click_generated_command)
            ch = (char)steamdeck_menu_key(ch, 'p', 'n');

        if (steamdeck) {
            int back_key = steamdeck_back_key();
            int confirm_key = steamdeck_confirm_key();
            int alt_key = steamdeck_alt_action_key();
            int secondary_key = steamdeck_secondary_key();
            
            if (ch == back_key)
                return score_ui_finish_halls(ESCAPE);  /* B = back */
            if (ch == confirm_key)
                ch = '\r';  /* A = open */
            if (ch == steamdeck_info_key())
                ch = 'r';  /* RS = run history */
            if (ch == alt_key)
                ch = 'l';  /* X = layout toggle */
            if (ch == secondary_key)
                ch = 's';  /* Y = order toggle */
        }

        if (ch == ESCAPE)
            return score_ui_finish_halls(ESCAPE);
        if (ch == 'r' || ch == 'R' || ch == 'h' || ch == 'H')
            return score_ui_finish_halls('r');
        if (ch == 's' || ch == 'S' || ch == 'o' || ch == 'O')
            return score_ui_finish_halls(ch);
        if (ch == 'l' || ch == 'L')
            return score_ui_finish_halls(ch);

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
            sdl_halls_screen_hide();
            if (!show_run_history_detail_for_score(&entries[*highlight_index]))
                bell("No run history is available for that character.");
            break;

        default:
            return score_ui_finish_halls(0);
        }
    }

    return score_ui_finish_halls(0);
}
void display_scores(void)
{
    log_info("Displaying high scores with interactive controls");
    show_scores_interactive();
    quit(NULL);
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
static void show_scores(void)
{
    bool finalized = score_ui_character_finalized();
    log_info("show_scores: generated=%d dead=%d",
             character_generated ? 1 : 0,
             finalized ? 1 : 0);

    sdl_suspend_main_view_zoom_for_saved_screen();

    high_score ordered_by_score[MAX_HISCORES + 1];
    high_score ordered_by_time[MAX_HISCORES + 1];

    int count_score = collect_high_scores(ordered_by_score, MAX_HISCORES, true);
    int count_time = collect_high_scores(ordered_by_time, MAX_HISCORES, false);

    const int capacity = MAX_HISCORES + 1;
    int page_size = 5;
    bool detailed = !score_last_layout_short;
    score_view_order order = SCORE_VIEW_ORDER_SCORE;

    high_score highlight_buffer;
    const high_score* highlight_entry = NULL;
    if (forced_highlight_active)
    {
        highlight_entry = &forced_highlight_entry;
    }
    else if (character_generated)
    {
        if (finalized)
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
    screen_push_touch_pane_hidden();
    while (true)
    {
        const high_score* list = (order == SCORE_VIEW_ORDER_SCORE) ? ordered_by_score : ordered_by_time;
        int count = (order == SCORE_VIEW_ORDER_SCORE) ? count_score : count_time;
        int* highlight = (order == SCORE_VIEW_ORDER_SCORE) ? &highlight_score : &highlight_time;

        log_debug("show_scores: rendering page (order=%s count=%d highlight=%d)",
                  (order == SCORE_VIEW_ORDER_SCORE) ? "score" : "time",
                  count, highlight ? *highlight : -1);

        char response = display_scores_pages(list, count, highlight, order, detailed, page_size);
        if (response == 'r' || response == 'R')
        {
            log_info("Halls of Mandos: opening Run History");
            do_cmd_run_history();
            continue;
        }
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
    /* If the player is quitting the program from in-game (leaving, no longer
     * playing, not dead, not returning to the title menu), the teardown below
     * restores and repaints the dungeon view, which flashes on screen for a
     * frame just before the window closes.  Suppress presentation so the score
     * screen stays visible until the process exits. */
    if (p_ptr->leaving && !p_ptr->playing && !p_ptr->is_dead
        && !p_ptr->quit_to_menu)
    {
        sdl_set_present_suppressed(true);
    }
    screen_pop_touch_pane_hidden();
    screen_pop_supporting_panes_hidden();
    if (sdl_resume_main_view_zoom_for_saved_screen()
        && character_generated && p_ptr && p_ptr->playing
        && character_dungeon)
    {
        do_cmd_redraw();
    }

    forced_highlight_active = false;
    score_last_layout_short = !detailed;
}
void show_scores_interactive(void)
{
    show_scores();
}
void show_scores_interactive_highlight(const high_score* entry)
{
    high_score saved_entry;
    bool had_forced = forced_highlight_active;
    if (had_forced) saved_entry = forced_highlight_entry;

    if (entry) {
        set_forced_highlight_entry(entry);
    } else {
        forced_highlight_active = false;
    }

    show_scores_interactive();

    if (had_forced) {
        forced_highlight_entry = saved_entry;
        forced_highlight_active = true;
    } else {
        forced_highlight_active = false;
    }
}

void show_scores_interactive_highlight_from_file(const char* filepath,
                                                 const high_score* entry)
{
    if (!filepath || !filepath[0]) {
        show_scores_interactive_highlight(entry);
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
        show_scores_interactive_highlight(entry);
        return;
    }

    log_debug("show_scores_interactive_highlight_from_file: rendering %s",
              filepath);
    score_file_ctx* previous_ctx = score_file_set_active_ctx(&temp_ctx);
    show_scores_interactive_highlight(entry);
    score_file_set_active_ctx(previous_ctx);

    SDL_CloseIO(temp_ctx.fd);
    score_file_reset_ctx(&temp_ctx);
}


static const char* score_run_status_label(score_record_status status)
{
    switch (status) {
    case SCORE_RECORD_ALIVE: return "Alive";
    case SCORE_RECORD_DEAD: return "Dead";
    case SCORE_RECORD_ESCAPED: return "Escaped";
    case SCORE_RECORD_REMOVED: return "Removed";
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
        if (temp.status == SCORE_RECORD_REMOVED)
            continue;
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
    sdl_push_terminal_menu_scale();

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
        int list_header_row = 3;
        int first_entry_row = 4;

        Term_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_outside_cancel_enabled(true);
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
        rows = footer_row - first_entry_row;
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
            c_prt(TERM_WHITE, "", list_header_row - 1, 0);
            c_prt(TERM_L_UMBER, "Date", list_header_row, 2);
            c_prt(TERM_L_UMBER, "S", list_header_row, col_status);
            c_prt(TERM_L_UMBER, "Depth", list_header_row, col_depth);
            c_prt(TERM_L_UMBER, "Score", list_header_row, col_score);
            if (show_sils)
                c_prt(TERM_L_UMBER, "Sil", list_header_row, col_sils);
            if (summary_width > 0)
                score_ui_put_fit(TERM_L_UMBER, "Player / Fate",
                    list_header_row, col_player, term_wid);
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
            c_prt(TERM_WHITE, "", list_header_row - 1, 0);
            c_prt(TERM_L_UMBER, "Date", list_header_row, col_date);
            c_prt(TERM_L_UMBER, "Status", list_header_row, col_status);
            c_prt(TERM_L_UMBER, "Depth", list_header_row, col_depth);
            c_prt(TERM_L_UMBER, "Score", list_header_row, col_score);
            c_prt(TERM_L_UMBER, "Sils", list_header_row, col_sils);
            if (player_width > 0)
                c_prt(TERM_L_UMBER, "Player", list_header_row, col_player);
            if (fate_width > 0)
                c_prt(TERM_L_UMBER, "Fate", list_header_row, col_fate);
        }

        for (int i = 0; i < rows; i++) {
            int idx = page_offset + i;
            if (idx >= count)
                break;

            const score_record_v1* rec = &entries[idx].record;
            int row_y = first_entry_row + i;

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
            char footer_full[160];
            char footer_short[120];
            const char* variants[2];

            strnfmt(footer_full, sizeof(footer_full),
                "D-pad move/page  [%s] details  [%s] sort  [%s] back",
                confirm_label, sort_label, back_label);
            strnfmt(footer_short, sizeof(footer_short),
                "[%s] details  [%s] sort  [%s] back",
                confirm_label, sort_label, back_label);
            variants[0] = footer_full;
            variants[1] = footer_short;
            terminal_prompt_pick_variant(footer, sizeof(footer), term_wid,
                false, variants, N_ELEMENTS(variants));
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
        } else if (sdl_touch_only_device_active()) {
            char footer[160];
            const char* variants[] = {
                "Tap a row to view, tap away to exit",
                "Tap to view, tap away to exit",
                "Tap to view"
            };

            terminal_prompt_pick_variant(footer, sizeof(footer), term_wid,
                false, variants, N_ELEMENTS(variants));
            score_ui_put_fit(TERM_L_DARK, footer, footer_row, 0, term_wid);
        } else {
            char footer[160];
            const char* variants[] = {
                "Dir move/page  Enter details  R sort  Esc back",
                "Enter details  R sort  Esc back",
                "Enter details  Esc back"
            };

            terminal_prompt_pick_variant(footer, sizeof(footer), term_wid,
                false, variants, N_ELEMENTS(variants));
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

        ui_scroll_area_begin(first_entry_row, footer_row - 1,
            SDL_TOUCH_MENU_CATEGORY_OTHER);
        ui_scroll_area_set_keys('8', '2', 'n', 'p');

        (void)Term_set_cursor(false);
        Term_fresh();
        bool saved_hide_cursor = hide_cursor;
        hide_cursor = true;
        int ch = inkey();
        hide_cursor = saved_hide_cursor;
        bool click_generated_command = false;

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
                    click_generated_command = true;
                }
            }
            else {
                ui_menu_click_clear();
                ui_scroll_area_clear();
            }
        }

        if (!click_generated_command)
            ch = steamdeck_menu_key(ch, 'p', 'n');

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

    sdl_pop_terminal_menu_scale();
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
        (void)screen_roff(entry->r_idx, NULL);
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
    sdl_push_terminal_menu_scale();

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
        ui_menu_click_set_outside_cancel_enabled(true);
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
                    ? "D-pad scroll/view  [%s] back"
                    : "D-pad view  [%s] back")
                : ((general_total_lines > text_rows)
                    ? "Dir scroll/view  Esc back"
                    : "Left/Right view  Esc back");
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
                    ? "D-pad scroll/view  [%s] back"
                    : "D-pad view  [%s] back")
                : ((stats_total_lines > text_rows)
                    ? "Dir scroll/view  Esc back"
                    : "Left/Right view  Esc back");
            scroll_first_row = 3;
            scroll_rows = text_rows;
            enable_scroll_area = true;
            break;
        case RUN_PANEL_ABILITIES:
            ability_rows = run_history_draw_abilities_panel(&details, &view.abilities, term_hgt);
            footer = steamdeck
                ? "D-pad navigate/view  [%s] back"
                : "Dir navigate/view  Esc back";
            active_list_rows = ability_rows;
            active_list_total = run_history_detail_panel_total(&details, panel);
            scroll_first_row = 5;
            scroll_rows = ability_rows;
            enable_scroll_area = active_list_total > 0;
            break;
        case RUN_PANEL_MILESTONES:
            milestone_rows = run_history_draw_milestones_panel(&details, &view.milestones, term_hgt);
            footer = steamdeck
                ? "D-pad navigate/view  [%s] back"
                : "Dir navigate/view  Esc back";
            active_list_rows = milestone_rows;
            active_list_total = run_history_detail_panel_total(&details, panel);
            scroll_first_row = 5;
            scroll_rows = milestone_rows;
            enable_scroll_area = active_list_total > 0;
            break;
        case RUN_PANEL_ARTEFACTS:
            artefact_rows = run_history_draw_artefact_panel(&details, &view.artefacts, term_hgt);
            footer = steamdeck
                ? "D-pad navigate/view  [%s] inspect  [%s] back"
                : "Dir navigate/view  Enter inspect  Esc back";
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
                ? "D-pad navigate/view  [%s] inspect  [%s] sort  [%s] back"
                : "Dir navigate/view  Enter inspect  S sort  Esc back";
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
            if (sdl_touch_only_device_active()) {
                SDL_strlcpy(footer_buf,
                    "Swipe to scroll, tap a tab to switch, tap away to close",
                    sizeof(footer_buf));
            } else if (steamdeck) {
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
        bool click_generated_command = false;

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
                        click_generated_command = true;
                        break;
                    case RUN_DETAIL_CLICK_PREV_PANEL:
                        ch = '4';
                        click_generated_command = true;
                        break;
                    case RUN_DETAIL_CLICK_NEXT_PANEL:
                        ch = '6';
                        click_generated_command = true;
                        break;
                    case RUN_DETAIL_CLICK_INSPECT:
                        ch = '\r';
                        click_generated_command = true;
                        break;
                    case RUN_DETAIL_CLICK_SORT:
                        ch = 's';
                        click_generated_command = true;
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

        if (!click_generated_command)
            ch = steamdeck_menu_key(ch, '4', '6');

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

    sdl_pop_terminal_menu_scale();
    screen_load();
}
