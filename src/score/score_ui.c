#include "score/score_ui.h"

#include "angband.h"
#include "externs.h"
#include "fs/path.h"
#include "log/log.h"
#include "score/score_io.h"
#include "score/score_logic.h"
#include "score/score_runs.h"

#include <SDL3/SDL.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RUN_HISTORY_MAX       256
#define RUN_HISTORY_ROWS       15

static high_score forced_highlight_entry;
static bool forced_highlight_active = false;
static bool force_interactive_scores = false;
static bool score_last_layout_short = true;

typedef enum
{
    SCORE_VIEW_ORDER_SCORE = 0,
    SCORE_VIEW_ORDER_CHRONOLOGY = 1
} score_view_order;

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
static void truncate_preserving_tail(const char* src, char* dst, size_t dst_size, int max_width)
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

    if (max_width <= 4)
    {
        int fill = MIN(max_width, (int)limit);
        for (int i = 0; i < fill; i++) dst[i] = '.';
        dst[fill] = '\0';
        return;
    }

    int end = len;
    while (end > 0 && isspace((unsigned char)src[end - 1]))
        end--;
    int tail_start = end;
    while (tail_start > 0 && !isspace((unsigned char)src[tail_start - 1]))
        tail_start--;
    int tail_len = end - tail_start;

    if (tail_len >= max_width - 3)
    {
        const char* tail_ptr = src + len - (max_width - 3);
        strnfmt(dst, dst_size, "...%.*s", max_width - 3, tail_ptr);
        return;
    }

    int remaining = max_width - 3 - tail_len;
    if (remaining < 3)
        remaining = 3;

    int head_end = tail_start;
    if (head_end > remaining)
        head_end = remaining;
    int candidate = head_end;
    while (candidate > 0 && !isspace((unsigned char)src[candidate - 1]))
        candidate--;
    if (candidate >= 3)
        head_end = candidate;

    char head[96];
    strnfmt(head, sizeof(head), "%.*s", head_end, src);
    int head_len = (int)strlen(head);
    while (head_len > 0 && isspace((unsigned char)head[head_len - 1]))
        head[--head_len] = '\0';

    const char* tail_ptr = src + tail_start;
    while (*tail_ptr && isspace((unsigned char)*tail_ptr))
        tail_ptr++;

    size_t pos = 0;
    dst[0] = '\0';

    if (head_len > 0)
    {
        size_t copy = (size_t)head_len;
        if (copy > (size_t)max_width) copy = (size_t)max_width;
        memcpy(dst + pos, head, copy);
        pos += copy;
    }

    if (pos + 3 > (size_t)max_width)
    {
        int fill = MIN(max_width, (int)limit);
        for (int i = 0; i < fill; i++) dst[i] = '.';
        dst[fill] = '\0';
        return;
    }

    memcpy(dst + pos, "...", 3);
    pos += 3;

    size_t tail_copy = strlen(tail_ptr);
    if (tail_copy > (size_t)max_width - pos)
        tail_copy = (size_t)max_width - pos;
    memcpy(dst + pos, tail_ptr, tail_copy);
    pos += tail_copy;
    dst[pos] = '\0';
}
static void display_single_score_short(byte attr, int place, int row, const high_score* entry)
{
    char depth_commas[16];
    char verdict_buf[96];
    const char* verdict;
    int wid, hgt;

    /* Get actual terminal width */
    Term_get_size(&wid, &hgt);
    const int line_width = MAX(80, wid);

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

    if (place == 0)
        strnfmt(out_val, sizeof(out_val), "     %5s ft  %s%s  [%s pts]",
                depth_commas, the_score->who,
                c_name + c_info[ph].alt_name, score_commas);
    else
        strnfmt(out_val, sizeof(out_val), "%3d. %5s ft  %s%s  [%s pts]",
                place, depth_commas, the_score->who,
                c_name + c_info[ph].alt_name, score_commas);

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
    if (the_score->escaped[0] == 't')
    {
        c_put_str(attr, "  escaped", row + 3, col + 4);
    }
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
static char display_scores_pages(const high_score* entries, int count, int highlight_index,
                                 score_view_order order, bool detailed, int page_size)
{
    Term_clear();

    if (!entries || count <= 0)
    {
        c_put_str(TERM_L_BLUE, "               Halls of Mandos", 1, 0);
        c_put_str(TERM_SLATE, "No recorded heroes yet.", 3, 0);
        Term_putstr(2, 23, -1, TERM_L_WHITE, "(press any key)");
        (void)inkey();
        return 0;
    }

    int start_index = 0;
    bool highlight_pending = true;

    while (start_index < count)
    {
        int entries_per_page = detailed ? page_size : (page_size * 4);
        if (entries_per_page < 1) entries_per_page = 1;

        if (highlight_pending && highlight_index >= 0)
        {
            int max_start = (count - entries_per_page);
            if (max_start < 0) max_start = 0;
            start_index = (highlight_index / entries_per_page) * entries_per_page;
            if (start_index > max_start) start_index = max_start;
            highlight_pending = false;
        }

        Term_clear();
        c_put_str(TERM_L_BLUE, "               Halls of Mandos", 1, 0);

        char order_buf[64];
        strnfmt(order_buf, sizeof(order_buf), "%s", score_view_order_label(order));
        c_put_str(TERM_L_WHITE, order_buf, 2, 0);

        char layout_buf[32];
        strnfmt(layout_buf, sizeof(layout_buf), "Layout: %s", detailed ? "Full" : "Short");
        c_put_str(TERM_SLATE, layout_buf, 2, 40);

        for (int row = 0; row < entries_per_page && (start_index + row) < count; row++)
        {
            int idx = start_index + row;
            bool is_highlight = (idx == highlight_index);
            byte attr = score_entry_color(&entries[idx], is_highlight);

            if (detailed)
            {
                display_single_score(attr, row * 4, 0, start_index + row + 1, false, (high_score*)&entries[idx]);
            }
            else
            {
                display_single_score_short(attr, start_index + row + 1, row, &entries[idx]);
            }
        }

        bool has_more = (start_index + entries_per_page < count);

        char footer[80];
        strnfmt(footer, sizeof(footer), "[S] Toggle order   [L] Layout   [ESC] Exit   (press any other key to %s)",
                has_more ? "continue" : "close");
        Term_putstr(1, 23, -1, TERM_L_WHITE, footer);

        char ch = inkey();
        prt("", 23, 0);

        if (ch == ESCAPE)
            return ESCAPE;
        if (ch == 's' || ch == 'S' || ch == 'o' || ch == 'O')
            return ch;
        if (ch == 'l' || ch == 'L')
            return ch;

        if (!has_more)
            break;

        start_index += entries_per_page;
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

    int idx = find_score_index(entries, *count, target);
    if (idx >= 0)
    {
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

    screen_save();
    while (true)
    {
        const high_score* list = (order == SCORE_VIEW_ORDER_SCORE) ? ordered_by_score : ordered_by_time;
        int count = (order == SCORE_VIEW_ORDER_SCORE) ? count_score : count_time;
        int highlight = (order == SCORE_VIEW_ORDER_SCORE) ? highlight_score : highlight_time;

        log_debug("show_scores: rendering page (order=%s count=%d highlight=%d)",
                  (order == SCORE_VIEW_ORDER_SCORE) ? "score" : "time",
                  count, highlight);

        char response = display_scores_pages(list, count, highlight, order, detailed, page_size);
        if (response == 's' || response == 'S' || response == 'o' || response == 'O')
        {
            order = (order == SCORE_VIEW_ORDER_SCORE) ? SCORE_VIEW_ORDER_CHRONOLOGY : SCORE_VIEW_ORDER_SCORE;
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
    screen_load();
    Term_fresh();

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

static const char* score_run_status_label(score_record_status status)
{
    switch (status) {
    case SCORE_RECORD_ALIVE: return "Alive";
    case SCORE_RECORD_DEAD: return "Dead";
    case SCORE_RECORD_ESCAPED: return "Escaped";
    default: return "Unknown";
    }
}
static const char* run_history_race_name(byte idx)
{
    if (!p_info || !p_name || !z_info || idx >= z_info->p_max)
        return "<unknown>";
    return p_name + p_info[idx].name;
}
static const char* run_history_character_name(byte idx)
{
    if (!c_info || !c_name || !z_info || idx >= z_info->c_max)
        return "<unknown>";
    return c_name + c_info[idx].name;
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

    #undef APPEND_FLAG

    if (first)
        SDL_strlcpy(out, "(none)", out_len);
}
static void run_history_format_guid(const score_guid64* guid,
                                    char* out, size_t out_len)
{
    if (!out || out_len == 0) return;
    if (!guid || (guid->hi == 0 && guid->lo == 0)) {
        SDL_strlcpy(out, "00000000-00000000", out_len);
        return;
    }
    strnfmt(out, out_len, "%08X-%08X",
            (unsigned int)guid->hi, (unsigned int)guid->lo);
}
static void run_history_show_detail(const score_record_v1* rec)
{
    if (!rec)
        return;

    screen_save();
    Term_clear();

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

    char flags[80];
    run_history_format_flags(rec->run_flags, flags, sizeof(flags));

    char killer_guid[32];
    run_history_format_guid(&rec->killer_guid, killer_guid, sizeof(killer_guid));

    const char* status = score_run_status_label(rec->status);
    const char* race_name = run_history_race_name(rec->race_id);
    const char* char_name = run_history_character_name(rec->character_id);
    const char* killer_kind = score_run_killer_kind_label(rec->killer_kind);
    const char* killer_race = run_history_monster_name(rec->killer_race_index);

    int row = 0;
    char line[160];

    strnfmt(line, sizeof(line), "Run #%u detail", rec->record_id);
    c_prt(TERM_L_BLUE, line, row++, 0);
    row++;

    strnfmt(line, sizeof(line),
            "Metarun %u  Chronological #%u  Character ID 0x%08X",
            rec->metarun_id, rec->chronological_idx, rec->character_id);
    c_prt(TERM_L_WHITE, line, row++, 0);

    strnfmt(line, sizeof(line), "Status: %s  Flags: %s", status, flags);
    c_prt(TERM_L_WHITE, line, row++, 0);

    strnfmt(line, sizeof(line), "Player: %s  Save hint: %s",
            player,
            rec->savefile_hint[0] ? rec->savefile_hint : "<none>");
    c_prt(TERM_L_WHITE, line, row++, 0);

    strnfmt(line, sizeof(line), "Race: %s (%u)  Character: %s (%u)  Power: %d",
            race_name, rec->race_id, char_name, rec->character_id, rec->character_power);
    c_prt(TERM_L_WHITE, line, row++, 0);

    strnfmt(line, sizeof(line), "Started: %s  Completed: %s", created, completed);
    c_prt(TERM_L_WHITE, line, row++, 0);

    strnfmt(line, sizeof(line),
            "Depth reached: %u'  Exit depth: %u'  Silmarils: %u",
            (unsigned)rec->max_depth, (unsigned)rec->exit_depth,
            (unsigned)rec->silmarils);
    c_prt(TERM_L_WHITE, line, row++, 0);

    strnfmt(line, sizeof(line),
            "Quests: %u  Uniques defeated: %u  Artefacts: %u",
            (unsigned)rec->quests_completed,
            (unsigned)rec->uniques_killed,
            (unsigned)rec->artefacts_found);
    c_prt(TERM_L_WHITE, line, row++, 0);

    strnfmt(line, sizeof(line),
            "Skills learned: %u  Abilities learned: %u  Net curses: %d",
            (unsigned)rec->skills_learned,
            (unsigned)rec->abilities_learned,
            rec->net_curses);
    c_prt(TERM_L_WHITE, line, row++, 0);

    strnfmt(line, sizeof(line),
            "Turns: %lu  XP: %lu  Kills: %lu  Seen: %lu",
            (unsigned long)rec->turns_spent,
            (unsigned long)rec->xp_earned,
            (unsigned long)rec->kills_total,
            (unsigned long)rec->kills_seen);
    c_prt(TERM_L_WHITE, line, row++, 0);

    row++;

    strnfmt(line, sizeof(line), "Killer: %s (%s)", rec->killer_name, killer_kind);
    c_prt(TERM_L_WHITE, line, row++, 0);

    strnfmt(line, sizeof(line), "Killer race: %s (%u)", killer_race, rec->killer_race_index);
    c_prt(TERM_L_WHITE, line, row++, 0);

    strnfmt(line, sizeof(line), "Cause: %s",
            rec->cause_of_death[0] ? rec->cause_of_death : "(unknown)");
    c_prt(TERM_L_WHITE, line, row++, 0);

    strnfmt(line, sizeof(line), "Killer GUID: %s", killer_guid);
    c_prt(TERM_L_DARK, line, row++, 0);

    row++;
    c_prt(TERM_L_WHITE, "Press any key to return", row++, 0);
    Term_fresh();
    (void)inkey();

    screen_load();
}

static int compare_run_records_desc(const void* a, const void* b)
{
    const score_record_v1* ra = (const score_record_v1*)a;
    const score_record_v1* rb = (const score_record_v1*)b;
    if (ra->record_id > rb->record_id)
        return -1;
    if (ra->record_id < rb->record_id)
        return 1;
    if (ra->completed_utc > rb->completed_utc)
        return -1;
    if (ra->completed_utc < rb->completed_utc)
        return 1;
    return 0;
}

static int collect_run_history(score_record_v1* out, int capacity)
{
    if (capacity <= 0 || !out)
        return 0;

    char path[1024];
    if (!path_build(path, sizeof(path), ANGBAND_DIR_APEX, SCORE_RUNS_DB_FILENAME))
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

    score_record_v1* ring = mem_alloc_array(capacity, score_record_v1);
    if (!ring) {
        SDL_CloseIO(file);
        return 0;
    }

    int stored = 0;
    score_record_v1 temp;
    while (SDL_ReadIO(file, &temp, sizeof(temp)) == sizeof(temp)) {
        ring[stored % capacity] = temp;
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

    qsort(out, count, sizeof(score_record_v1), compare_run_records_desc);
    return count;
}
void do_cmd_run_history(void)
{
    score_record_v1 entries[RUN_HISTORY_MAX];
    int count = collect_run_history(entries, RUN_HISTORY_MAX);
    if (count <= 0) {
        msg_print("No run history is available.");
        return;
    }

    int rows = RUN_HISTORY_ROWS;
    int total_pages = (count + rows - 1) / rows;
    int last_page_offset = ((count - 1) / rows) * rows;
    if (last_page_offset < 0)
        last_page_offset = 0;
    int page_offset = 0;
    int highlight = 0;
    bool done = false;

    while (!done) {
        screen_save();
        Term_clear();

        if (page_offset < 0)
            page_offset = 0;
        if (page_offset > last_page_offset)
            page_offset = last_page_offset;

        int page = (rows > 0) ? (page_offset / rows) : 0;

        c_prt(TERM_L_BLUE,
              format("Run history (%d entries)  page %d/%d", count, page + 1, total_pages),
              0, 0);
        c_prt(TERM_L_UMBER,
              "ID    Date       Status    Dpth Sil Player            Cause of death",
              2, 0);

        for (int i = 0; i < rows; i++) {
            int idx = page_offset + i;
            if (idx >= count)
                break;

            const score_record_v1* rec = &entries[idx];

            char date[16];
            run_history_format_timestamp(rec->completed_utc, false, date, sizeof(date));

            char cause[64];
            truncate_preserving_tail(rec->cause_of_death, cause, sizeof(cause), 60);

            char player[33];
            if (rec->player_name[0]) {
                SDL_strlcpy(player, rec->player_name, sizeof(player));
            } else if (rec->savefile_hint[0]) {
                SDL_strlcpy(player, rec->savefile_hint, sizeof(player));
            } else {
                SDL_strlcpy(player, "<unknown>", sizeof(player));
            }

            char line[160];
            bool selected = (idx == highlight);
            strnfmt(line, sizeof(line), "%c%-4u %-10s %-8s %4u %3u %-16.16s %-60s",
                    selected ? '>' : ' ',
                    rec->record_id,
                    date,
                    score_run_status_label(rec->status),
                    (unsigned)rec->exit_depth,
                    (unsigned)rec->silmarils,
                    player,
                    cause);
            c_prt(selected ? TERM_YELLOW : TERM_WHITE, line, 3 + i, 0);
        }

        c_prt(TERM_L_DARK,
              "[Esc] exit  [Up/Down] move  [Space/Right] next  [Left/-] prev  [Y/Enter] show details",
              4 + rows, 0);

        Term_fresh();
        int ch = inkey();
        screen_load();

        switch (ch) {
        case ESCAPE:
        case 'q':
            done = true;
            break;

        case 'y':
        case 'Y':
        case '\r':
        case '\n':
            run_history_show_detail(&entries[highlight]);
            break;

        case ' ':
        case '6':
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
}
