#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "pane.h"
#include "supplies.h"
#include "item_set.h"
#include "ui/status/status-internal.h"

/*
 * Display basic info (mostly left of map)
 */
void prt_frame_basic(void)
{
    int i;

    if (ui_hide_left_panel())
    {
        prt_depth();
        return;
    }

    for (i = ROW_MAP; Term && i < ROW_MAP + SCREEN_HGT; i++)
        Term_erase(0, i, LEFT_PANEL_WID);

    /* Name */
    prt_player_name();

    /* Small monospace health graphic under the name */
    prt_char_health_graphic();

    /* Level/Experience */
    prt_exp();

    /* All Stats */
    for (i = 0; i < A_MAX; i++)
        prt_stat(i);

    /* Hitpoints */
    prt_hp();

    /* Spellpoints */
    prt_sp();

    /* Light */
    prt_light();

    /* Melee */
    prt_mel();

    /* Archery */
    prt_arc();

    /* Quiver */
    prt_quiver();

    /* Evasion */
    prt_evn();

    /* Song */
    prt_song();

    /* Current depth */
    prt_depth();

    /* redraw monster health */
    health_redraw();
}

/*
 * Display extra info (mostly below map)
 */
static void prt_frame_extra(void)
{
    if (ui_status_system_compact())
    {
        /* Compact status mode: render a single packed line. */
        if (!ui_compact_status_line_handles_wounds())
        {
            prt_poisoned();
            prt_cut();
        }
        prt_status_line_compact();
        return;
    }

    /* Stun */
    prt_stun();

    /* Food */
    prt_hunger();

    /* Various */
    prt_blind();
    prt_confused();
    prt_afraid();
    prt_poisoned();
    prt_cut();
    prt_terrain();

    /* State */
    prt_state();

    /* Speed */
    prt_speed();
}

/*
 * Hack -- display inventory in sub-windows
 */
static void fix_inven(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_INVEN)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Display inventory */
        display_inven();

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- display supplies in sub-windows
 */
static void fix_supplies(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_SUPPLY)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Display supplies */
        display_supplies();

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- display monsters in sub-windows
 */
static void fix_monlist(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_MONLIST)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Display visible monsters */
        display_monlist();

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- display combat rolls in sub-windows
 */
typedef enum pane_log_display_entry_kind
{
    PANE_LOG_DISPLAY_MESSAGE,
    PANE_LOG_DISPLAY_COMBAT
} pane_log_display_entry_kind;

typedef struct pane_log_display_entry
{
    pane_log_display_entry_kind kind;
    u32b sequence;
    int tie_breaker;
    s16b message_age;
    int history_idx;
    int roll_idx;
    combat_roll* roll;
} pane_log_display_entry;

#define PANE_LOG_DISPLAY_MAX_ENTRIES \
    (MESSAGE_MAX + ((MAX_COMBAT_HISTORY + 1) * MAX_COMBAT_ROLLS))

static pane_log_display_entry
    pane_log_display_entries[PANE_LOG_DISPLAY_MAX_ENTRIES];

static bool pane_log_filter_includes_messages(int filter)
{
    return filter == LOG_HISTORY_FILTER_ALL
        || filter == LOG_HISTORY_FILTER_MESSAGES;
}

static bool pane_log_filter_includes_combat(int filter)
{
    return filter == LOG_HISTORY_FILTER_ALL
        || filter == LOG_HISTORY_FILTER_COMBAT;
}

static int pane_log_display_entry_compare_newest_first(
    const void* a, const void* b)
{
    const pane_log_display_entry* entry_a = (const pane_log_display_entry*)a;
    const pane_log_display_entry* entry_b = (const pane_log_display_entry*)b;

    if (entry_a->sequence < entry_b->sequence)
        return 1;
    if (entry_a->sequence > entry_b->sequence)
        return -1;

    if (entry_a->tie_breaker < entry_b->tie_breaker)
        return 1;
    if (entry_a->tie_breaker > entry_b->tie_breaker)
        return -1;

    return (int)entry_a->kind - (int)entry_b->kind;
}

static int pane_log_display_collect_entries(
    pane_log_display_entry* entries, int max_entries, int filter)
{
    int count = 0;

    if (pane_log_filter_includes_messages(filter))
    {
        s16b num = message_num();

        for (s16b age = 0; (age < num) && (count < max_entries); age++)
        {
            entries[count].kind = PANE_LOG_DISPLAY_MESSAGE;
            entries[count].sequence = message_sequence(age);
            if (entries[count].sequence == 0)
                entries[count].sequence = (u32b)(num - age);
            entries[count].tie_breaker = num - age;
            entries[count].message_age = age;
            entries[count].history_idx = -1;
            entries[count].roll_idx = -1;
            entries[count].roll = NULL;
            count++;
        }
    }

    if (pane_log_filter_includes_combat(filter))
    {
        for (int r = 0; (r < combat_number) && (count < max_entries)
             && (r < MAX_COMBAT_ROLLS); r++)
        {
            combat_roll* roll = &combat_rolls[0][r];

            if (roll->att_type == COMBAT_ROLL_NONE)
                continue;

            entries[count].kind = PANE_LOG_DISPLAY_COMBAT;
            entries[count].sequence = roll->sequence;
            if (entries[count].sequence == 0)
                entries[count].sequence = (u32b)turn;
            entries[count].tie_breaker = r;
            entries[count].message_age = -1;
            entries[count].history_idx = -1;
            entries[count].roll_idx = r;
            entries[count].roll = roll;
            count++;
        }

        for (int h = 0; (h < combat_history_count) && (count < max_entries);
             h++)
        {
            int hist_idx =
                (combat_history_head - h + MAX_COMBAT_HISTORY)
                % MAX_COMBAT_HISTORY;
            combat_history_round* round = &combat_history[hist_idx];
            int rolls = round->num_rolls;

            if (rolls > MAX_COMBAT_ROLLS)
                rolls = MAX_COMBAT_ROLLS;

            for (int r = 0; (r < rolls) && (count < max_entries); r++)
            {
                combat_roll* roll = &round->rolls[r];

                if (roll->att_type == COMBAT_ROLL_NONE)
                    continue;

                entries[count].kind = PANE_LOG_DISPLAY_COMBAT;
                entries[count].sequence = roll->sequence;
                if (entries[count].sequence == 0)
                    entries[count].sequence = (u32b)round->turn_count;
                entries[count].tie_breaker = r;
                entries[count].message_age = -1;
                entries[count].history_idx = hist_idx;
                entries[count].roll_idx = r;
                entries[count].roll = roll;
                count++;
            }
        }
    }

    qsort(entries, count, sizeof(pane_log_display_entry),
        pane_log_display_entry_compare_newest_first);

    return count;
}

static void display_messages_in_pane(void)
{
    int i;
    int w, h;
    int x, y;

    Term_get_size(&w, &h);

    for (i = 0; i < h; i++)
    {
        byte color = message_color((s16b)i);

        Term_putstr(0, (h - 1) - i, -1, color, message_str((s16b)i));
        Term_locate(&x, &y);
        Term_erase(x, y, 255);
    }
}

#define PANE_LOG_WRAP_MAX_SEGS 8
#define PANE_LOG_WRAP_SEG_MAX 96

/*
 * Split a message into segments that each fit `avail` cells, breaking on
 * spaces where possible.  Returns the number of segments produced.
 */
static int pane_log_wrap_message(cptr msg, int avail,
    char segs[PANE_LOG_WRAP_MAX_SEGS][PANE_LOG_WRAP_SEG_MAX])
{
    int len = msg ? (int)strlen(msg) : 0;
    int pos = 0;
    int n = 0;

    if (avail < 1)
        avail = 1;

    while (pos < len && n < PANE_LOG_WRAP_MAX_SEGS)
    {
        int remain = len - pos;
        int take = (remain <= avail) ? remain : avail;
        int copy;

        if (take < remain)
        {
            /* Prefer to break at the last space inside the window. */
            for (int k = take; k > 0; k--)
            {
                if (msg[pos + k] == ' ')
                {
                    take = k;
                    break;
                }
            }
        }

        copy = take;
        if (copy >= PANE_LOG_WRAP_SEG_MAX)
            copy = PANE_LOG_WRAP_SEG_MAX - 1;
        memcpy(segs[n], msg + pos, copy);
        segs[n][copy] = '\0';
        n++;

        pos += take;
        while (pos < len && msg[pos] == ' ')
            pos++;
    }

    return n;
}

static int pane_log_wrap_message_pixels(cptr msg, int fallback_avail,
    char segs[PANE_LOG_WRAP_MAX_SEGS][PANE_LOG_WRAP_SEG_MAX])
{
    int offsets[PANE_LOG_WRAP_MAX_SEGS];
    int lengths[PANE_LOG_WRAP_MAX_SEGS];
    int nseg;

    nseg = sdl_overlay_log_wrap(msg, PANE_LOG_WRAP_MAX_SEGS, offsets,
        lengths);
    if (nseg < 1)
        return pane_log_wrap_message(msg, fallback_avail, segs);

    for (int i = 0; i < nseg; i++)
    {
        int copy = lengths[i];

        if (copy >= PANE_LOG_WRAP_SEG_MAX)
            copy = PANE_LOG_WRAP_SEG_MAX - 1;
        if (copy < 0)
            copy = 0;
        memcpy(segs[i], msg + offsets[i], copy);
        segs[i][copy] = '\0';
    }

    return nseg;
}

/*
 * Per-row combat-roll tokens for the overlay log, captured independently of the
 * cell grid so the SDL renderer can re-pack the full line in pixels even when
 * the panel is too narrow in cells to hold it.
 */
#define PANE_LOG_COMBAT_ROW_MAX 128
static combat_roll_token
    g_pane_log_row_tokens[PANE_LOG_COMBAT_ROW_MAX][COMBAT_ROLL_MAX_TOKENS];
static int g_pane_log_row_token_count[PANE_LOG_COMBAT_ROW_MAX];
static bool g_pane_log_row_message_active[PANE_LOG_COMBAT_ROW_MAX];
static byte g_pane_log_row_message_attr[PANE_LOG_COMBAT_ROW_MAX];
static char
    g_pane_log_row_message_text[PANE_LOG_COMBAT_ROW_MAX][PANE_LOG_WRAP_SEG_MAX];

static void pane_log_row_state_reset(int h)
{
    for (int r = 0; (r < h) && (r < PANE_LOG_COMBAT_ROW_MAX); r++)
    {
        g_pane_log_row_token_count[r] = 0;
        g_pane_log_row_message_active[r] = false;
        g_pane_log_row_message_text[r][0] = '\0';
    }
}

static void pane_log_store_overlay_message_row(int row, byte attr, cptr text)
{
    if (row < 0 || row >= PANE_LOG_COMBAT_ROW_MAX)
        return;

    g_pane_log_row_message_active[row] = true;
    g_pane_log_row_message_attr[row] = attr;
    SDL_strlcpy(g_pane_log_row_message_text[row], text ? text : "",
        sizeof(g_pane_log_row_message_text[row]));
}

static void pane_log_mark_overlay_message_row(int row, int margin, byte attr)
{
    bool prev_pixel_pack;
    char fill[256];
    int n;

    if (!Term)
        return;
    if (row < 0 || row >= Term->hgt)
        return;
    if (margin < 0)
        margin = 0;
    if (margin >= Term->wid)
        margin = Term->wid - 1;

    /*
     * Flag the whole visible band (not just one cell) with spaces so it forms a
     * single story span.  Otherwise the blank cells after a lone marker are
     * wiped to background *after* the pixel renderer draws the full message,
     * erasing everything past the marker.
     */
    n = Term->wid - margin;
    if (n < 1)
        n = 1;
    if (n > (int)sizeof(fill) - 1)
        n = (int)sizeof(fill) - 1;
    for (int i = 0; i < n; i++)
        fill[i] = ' ';
    fill[n] = '\0';

    prev_pixel_pack = Term->story_pixel_pack;
    Term->story_pixel_pack = true;
    Term_putstr(margin, row, n, attr, fill);
    Term->story_pixel_pack = prev_pixel_pack;
}

int pane_log_combat_row_tokens(int row, const combat_roll_token** out)
{
    if (row < 0 || row >= PANE_LOG_COMBAT_ROW_MAX
        || g_pane_log_row_token_count[row] <= 0)
    {
        if (out)
            *out = NULL;
        return 0;
    }

    if (out)
        *out = g_pane_log_row_tokens[row];
    return g_pane_log_row_token_count[row];
}

bool pane_log_overlay_message_row(int row, cptr* out_text, byte* out_attr)
{
    if (row < 0 || row >= PANE_LOG_COMBAT_ROW_MAX
        || !g_pane_log_row_message_active[row])
    {
        if (out_text)
            *out_text = NULL;
        return false;
    }

    if (out_text)
        *out_text = g_pane_log_row_message_text[row];
    if (out_attr)
        *out_attr = g_pane_log_row_message_attr[row];
    return true;
}

static bool pane_log_current_term_is_overlay(void)
{
    return (PANE_ROLLS < ANGBAND_TERM_MAX)
        && (Term == angband_term[PANE_ROLLS]);
}

static void display_overlay_messages_in_pane(void)
{
    int w, h;
    int margin;
    int avail;
    int row;
    s16b num;

    Term_get_size(&w, &h);
    for (int r = 0; r < h; r++)
        Term_erase(0, r, 255);
    pane_log_row_state_reset(h);

    margin = pane_log_overlay_left_margin(w);
    avail = w - margin;
    if (avail < 1)
        avail = 1;

    num = message_num();
    row = h - 1;
    for (s16b age = 0; (age < num) && (row >= 0); age++)
    {
        byte attr = message_color(age);
        char segs[PANE_LOG_WRAP_MAX_SEGS][PANE_LOG_WRAP_SEG_MAX];
        int nseg = pane_log_wrap_message_pixels(message_str(age), avail,
            segs);
        int top;

        if (nseg < 1)
        {
            row--;
            continue;
        }

        top = row - nseg + 1;
        for (int k = 0; k < nseg; k++)
        {
            int ry = top + k;

            if (ry < 0 || ry >= h)
                continue;
            pane_log_store_overlay_message_row(ry, attr, segs[k]);
            pane_log_mark_overlay_message_row(ry, margin, attr);
        }
        row = top - 1;
    }
}

static void display_combined_log_in_pane(int filter)
{
    int w, h;
    int count;
    int margin;
    int avail;
    bool overlay;
    int row;

    Term_get_size(&w, &h);
    for (int r = 0; r < h; r++)
        Term_erase(0, r, 255);

    /*
     * In the overlay log the visible panel is a narrow right-hand band, so
     * indent text to that band and wrap long messages within it.  The wide
     * cell grid behind it still carries the full combat-roll line.
     */
    overlay = (PANE_ROLLS < ANGBAND_TERM_MAX) && (Term == angband_term[PANE_ROLLS]);

    /*
     * Only the overlay log paints from the shared out-of-band row buffers; the
     * bottom log pane writes real cells.  Resetting the buffers here for the
     * non-overlay pane would wipe the overlay's stored rows, so a later generic
     * Term_redraw of the overlay (e.g. Ctrl-R) would repaint it from empty
     * buffers and blank it.  Keep the overlay's content intact by only the
     * overlay resetting (and then repopulating) the buffers.
     */
    if (overlay)
        pane_log_row_state_reset(h);
    margin = overlay ? pane_log_overlay_left_margin(w) : 0;
    avail = w - margin;
    if (avail < 1)
        avail = 1;

    count = pane_log_display_collect_entries(pane_log_display_entries,
        PANE_LOG_DISPLAY_MAX_ENTRIES, filter);

    row = h - 1;
    for (int i = 0; (i < count) && (row >= 0); i++)
    {
        pane_log_display_entry* entry = &pane_log_display_entries[i];
        int x, y;

        if (entry->kind == PANE_LOG_DISPLAY_MESSAGE)
        {
            cptr msg = message_str(entry->message_age);
            byte attr = message_color(entry->message_age);
            char segs[PANE_LOG_WRAP_MAX_SEGS][PANE_LOG_WRAP_SEG_MAX];
            int nseg;
            int top;

            if (!overlay)
            {
                Term_putstr(margin, row, -1, attr, msg ? msg : "");
                Term_locate(&x, &y);
                Term_erase(x, y, 255);
                row--;
                continue;
            }

            nseg = pane_log_wrap_message_pixels(msg, avail, segs);
            if (nseg < 1)
            {
                row--;
                continue;
            }

            /* Lay the wrapped lines top-to-bottom ending at the current row. */
            top = row - nseg + 1;
            for (int k = 0; k < nseg; k++)
            {
                int ry = top + k;

                if (ry < 0 || ry >= h)
                    continue;
                pane_log_store_overlay_message_row(ry, attr, segs[k]);
                pane_log_mark_overlay_message_row(ry, margin, attr);
            }
            row = top - 1;
        }
        else
        {
            combat_roll* roll = entry->roll;

            display_combat_roll_line_at(row, 0, roll);
            if (overlay && row >= 0 && row < PANE_LOG_COMBAT_ROW_MAX)
                g_pane_log_row_token_count[row] = combat_roll_emit_tokens(
                    roll, g_pane_log_row_tokens[row], COMBAT_ROLL_MAX_TOKENS);
            Term_locate(&x, &y);
            Term_erase(x, y, 255);
            row--;
        }
    }
}

static bool pane_index_has_log_display_mode(int pane)
{
    return pane == WINDOW_MESSAGE || pane == WINDOW_COMBAT_ROLLS;
}

static void display_log_pane_with_filter(int pane, int default_filter)
{
    int filter = default_filter;
    story_font_term_state story_prev;

    if (pane_index_has_log_display_mode(pane))
        filter = sdl_log_pane_display_filter(pane);

    /* Render the log/combat pane with the secondary story font. */
    story_font_term_push_slot(true, false, STORY_FONT_SLOT_SECONDARY,
        &story_prev);

    if (filter == LOG_HISTORY_FILTER_MESSAGES)
    {
        if (pane_log_current_term_is_overlay())
            display_overlay_messages_in_pane();
        else
            display_messages_in_pane();
    }
    else if (filter == LOG_HISTORY_FILTER_COMBAT)
    {
        /* Only the overlay owns the shared out-of-band buffers; see the note in
         * display_combined_log_in_pane().  Don't let a non-overlay combat pane
         * wipe them out from under the overlay. */
        if (Term && pane_log_current_term_is_overlay())
            pane_log_row_state_reset(Term->hgt);
        display_combat_rolls();
    }
    else
        display_combined_log_in_pane(filter);

    story_font_term_pop(&story_prev);

    /*
     * The overlay log keeps its message text and combat tokens out-of-band
     * (the term cells are only spaces carrying a pixel-pack flag), so the term's
     * per-cell change detection cannot tell when a row's actual content changed.
     * Unchanged cells would be skipped on the next Term_fresh, leaving the
     * canvas showing stale or never-drawn rows (a half-rendered combat line, or
     * an empty upper band).  Force a full re-emit so every populated row is
     * repainted from the current out-of-band content.
     */
    if (pane_log_current_term_is_overlay() && Term)
        Term->total_erase = true;
}

static void fix_log_panes(u32b requested_flags)
{
    int j;

    /*
     * Message and combat panes can each display Messages, Combat, or Combined,
     * and both legacy window slots subscribe to both PW flags. The old code
     * scanned and rendered every subscribed term once in fix_combat_rolls()
     * and then again in fix_message(). Render each term exactly once.
     */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;
        int default_filter;

        if (!angband_term[j])
            continue;
        if (!(op_ptr->window_flag[j] & requested_flags
                & (PW_COMBAT_ROLLS | PW_MESSAGE)))
        {
            continue;
        }

        Term_activate(angband_term[j]);
        default_filter = (j == WINDOW_MESSAGE)
            ? LOG_HISTORY_FILTER_MESSAGES
            : LOG_HISTORY_FILTER_COMBAT;
        display_log_pane_with_filter(j, default_filter);
        Term_fresh();
        Term_activate(old);
    }
}

/*
 * Hack -- display equipment in sub-windows
 */
static void fix_equip(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_EQUIP)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Display equipment */
        display_equip();

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- display player in sub-windows (mode 0)
 */
static void fix_player_0(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_PLAYER_0)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Display player */
        display_player(0);

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Hack -- display monster recall in sub-windows
 */
static void fix_monster(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        term* old = Term;

        /* No window */
        if (!angband_term[j])
            continue;

        /* No relevant flags */
        if (!(op_ptr->window_flag[j] & (PW_MONSTER)))
            continue;

        /* Activate */
        Term_activate(angband_term[j]);

        /* Display monster race info */
        if (p_ptr->monster_race_idx)
            display_roff(p_ptr->monster_race_idx, NULL);

        /* Fresh */
        Term_fresh();

        /* Restore */
        Term_activate(old);
    }
}

/*
 * Handle "p_ptr->redraw"
 */
void redraw_stuff(void)
{
    bool hidden_overlay_needs_refresh = false;
    u32b requested_redraw;

    /* Redraw stuff */
    if (!p_ptr->redraw) {
        // log_trace("redraw_stuff: no redraws needed");
        return;
    }

    // log_trace("redraw_stuff: processing redraws 0x%08X", p_ptr->redraw);

    /* Character is not ready yet, no screen updates */
    if (!character_generated)
        return;

    // log_trace("redraw_stuff: character_icky=%d, character_generated=%s",
            //   character_icky, character_generated ? "true" : "false");

    /* Character is in "icky" mode, no screen updates */
    if (character_icky && !p_ptr->is_dead) {
        // log_trace("redraw_stuff: character in icky mode (value=%d), skipping screen updates", character_icky);
        return;
    }

    requested_redraw = p_ptr->redraw;
    /*
     * The styled SDL left pane is rendered from its own retained status term.
     * Map-only redraws do not affect it; any status redraw marks that source
     * dirty so multiple native presents can reuse one generated frame.
     */
    if (requested_redraw & ~PR_MAP)
        sdl_left_panel_source_invalidate();

    if (ui_hide_left_panel())
    {
        hidden_overlay_line hidden_lines[16];
        int hidden_line_count = 0;
        bool hidden_mask_changed = false;

        if (hidden_left_panel_visible())
        {
            hidden_line_count = hidden_left_panel_build_lines(hidden_lines, 16);
            hidden_mask_changed
                = hidden_left_panel_sync_mask(hidden_lines, hidden_line_count);
        }
        else
            hidden_mask_changed = hidden_left_panel_sync_mask(NULL, 0);

        if (hidden_mask_changed)
        {
            p_ptr->redraw |= PR_MAP;
            hidden_overlay_needs_refresh = true;
        }
    }

    if (p_ptr->redraw & (PR_MAP))
    {
        p_ptr->redraw &= ~(PR_MAP);
        log_trace("redraw_stuff: redrawing map");
        prt_map();
        if (ui_hide_left_panel())
            hidden_overlay_needs_refresh = true;
    }

    if (p_ptr->redraw & (PR_BASIC))
    {
        p_ptr->redraw &= ~(PR_BASIC);
        p_ptr->redraw &= ~(PR_STATS);
        p_ptr->redraw &= ~(PR_MEL | PR_EXP | PR_ARC | PR_QUIVER);
        p_ptr->redraw &= ~(PR_ARMOR | PR_HP | PR_VOICE | PR_SONG | PR_LIGHT);
        p_ptr->redraw &= ~(PR_DEPTH | PR_HEALTHBAR);
        p_ptr->redraw &= ~(PR_RESIST);
        prt_frame_basic();
        if (ui_hide_left_panel())
            hidden_overlay_needs_refresh = true;
    }

    if (p_ptr->redraw & (PR_MISC))
    {
        p_ptr->redraw &= ~(PR_MISC);

        if (!ui_hide_left_panel())
        {
            /* Name */
            prt_player_name();
        }
    }

    if (p_ptr->redraw & (PR_EXP))
    {
        p_ptr->redraw &= ~(PR_EXP);
        if (!ui_hide_left_panel())
            prt_exp();
    }

    if (p_ptr->redraw & (PR_STATS))
    {
        p_ptr->redraw &= ~(PR_STATS);
        if (!ui_hide_left_panel())
        {
            prt_stat(A_STR);
            prt_stat(A_DEX);
            prt_stat(A_CON);
            prt_stat(A_GRA);
        }
    }

    if (p_ptr->redraw & (PR_MEL))
    {
        p_ptr->redraw &= ~(PR_MEL);
        if (!ui_hide_left_panel())
            prt_mel();
        else
            hidden_overlay_needs_refresh = true;
    }

    if (p_ptr->redraw & (PR_ARC))
    {
        p_ptr->redraw &= ~(PR_ARC);
        if (!ui_hide_left_panel())
            prt_arc();
        else
            hidden_overlay_needs_refresh = true;
    }

    if (p_ptr->redraw & (PR_QUIVER))
    {
        p_ptr->redraw &= ~(PR_QUIVER);
        if (!ui_hide_left_panel())
        {
            prt_quiver();
            prt_arc();
        }
        else
            hidden_overlay_needs_refresh = true;
    }

    if (p_ptr->redraw & (PR_ARMOR))
    {
        p_ptr->redraw &= ~(PR_ARMOR);
        if (!ui_hide_left_panel())
            prt_evn();
    }

    if (p_ptr->redraw & (PR_HP))
    {
        p_ptr->redraw &= ~(PR_HP);
        if (ui_hide_left_panel())
            hidden_overlay_needs_refresh = true;
        else
            prt_hp();

        /*
         * hack:  redraw player, since the player's color
         * now indicates approximate health.
         */
        if (arg_graphics == GRAPHICS_NONE)
        {
            lite_spot(p_ptr->py, p_ptr->px);
        }

        if (!ui_hide_left_panel())
        {
            /* Also update the monospace character health graphic */
            prt_char_health_graphic();
        }
    }

    if (p_ptr->redraw & (PR_VOICE))
    {
        p_ptr->redraw &= ~(PR_VOICE);
        if (ui_hide_left_panel())
            hidden_overlay_needs_refresh = true;
        else
            prt_sp();
    }

    if (p_ptr->redraw & (PR_LIGHT))
    {
        p_ptr->redraw &= ~(PR_LIGHT);
        if (!ui_hide_left_panel())
            prt_light();
        else
            hidden_overlay_needs_refresh = true;
    }

    /* Sil - Hack: always redraw song (really should invent redraw flag for it
     * etc. */
    if (p_ptr->redraw & (PR_SONG))
    {
        p_ptr->redraw &= ~(PR_SONG);
        if (!ui_hide_left_panel())
            prt_song();
        else
        {
            if (ui_compact_status_line_handles_song())
                prt_song();
            hidden_overlay_needs_refresh = true;
        }
    }

    if (p_ptr->redraw & (PR_DEPTH))
    {
        p_ptr->redraw &= ~(PR_DEPTH);
        prt_depth();
    }

    if (p_ptr->redraw & (PR_HEALTHBAR))
    {
        p_ptr->redraw &= ~(PR_HEALTHBAR);
        if (!ui_hide_left_panel())
            health_redraw();
        else
            hidden_overlay_needs_refresh = true;
    }

    if (p_ptr->redraw & (PR_EXTRA))
    {
        p_ptr->redraw &= ~(PR_EXTRA);
        p_ptr->redraw &= ~(PR_CUT | PR_STUN);
        p_ptr->redraw &= ~(PR_HUNGER);
        p_ptr->redraw &= ~(PR_BLIND | PR_CONFUSED);
        p_ptr->redraw &= ~(PR_AFRAID | PR_POISONED);
        p_ptr->redraw &= ~(PR_STATE | PR_SPEED);
        prt_frame_extra();
        if (ui_hide_left_panel())
            hidden_overlay_needs_refresh = true;
    }

    if (p_ptr->redraw & (PR_CUT))
    {
        p_ptr->redraw &= ~(PR_CUT);
        if (!ui_hide_left_panel() || ui_compact_status_line_handles_wounds())
            prt_cut();
        if (ui_hide_left_panel())
            hidden_overlay_needs_refresh = true;
    }

    if (p_ptr->redraw & (PR_STUN))
    {
        p_ptr->redraw &= ~(PR_STUN);
        prt_stun();
    }

    if (p_ptr->redraw & (PR_HUNGER))
    {
        p_ptr->redraw &= ~(PR_HUNGER);
        prt_hunger();
    }

    if (p_ptr->redraw & (PR_BLIND))
    {
        p_ptr->redraw &= ~(PR_BLIND);
        prt_blind();
    }

    if (p_ptr->redraw & (PR_CONFUSED))
    {
        p_ptr->redraw &= ~(PR_CONFUSED);
        prt_confused();
    }

    if (p_ptr->redraw & (PR_AFRAID))
    {
        p_ptr->redraw &= ~(PR_AFRAID);
        prt_afraid();
    }

    if (p_ptr->redraw & (PR_POISONED))
    {
        p_ptr->redraw &= ~(PR_POISONED);
        if (!ui_hide_left_panel() || ui_compact_status_line_handles_wounds())
            prt_poisoned();
        if (ui_hide_left_panel())
            hidden_overlay_needs_refresh = true;
    }

    if (p_ptr->redraw & (PR_STATE))
    {
        p_ptr->redraw &= ~(PR_STATE);
        prt_state();
    }

    if (p_ptr->redraw & (PR_SPEED))
    {
        p_ptr->redraw &= ~(PR_SPEED);
        prt_speed();
    }

    if (p_ptr->redraw & (PR_TERRAIN))
    {
        p_ptr->redraw &= ~(PR_TERRAIN);
        prt_terrain();
    }

    if (ui_hide_left_panel() && hidden_overlay_needs_refresh)
        prt_hidden_top_vitals();

    redraw_hidden_left_panel_overlay();

    // log_trace("redraw_stuff: completed all redraws");
}

/*
 * Handle "p_ptr->window"
 */
void window_stuff(void)
{
    int j;

    u32b mask = 0L;

    /* Nothing to do */
    if (!p_ptr->window) {
        // log_trace("window_stuff: no window updates needed");
        return;
    }

    log_trace("window_stuff: processing windows 0x%08X", p_ptr->window);

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        /* Save usable flags */
        if (angband_term[j])
        {
            /* Build the mask */
            mask |= op_ptr->window_flag[j];
        }
    }

    /* Apply usable flags */
    p_ptr->window &= (mask);

    /* Nothing to do */
    if (!p_ptr->window)
        return;

    /*
     * Every fix_* routine refreshes its terminal. On SDL all terminals share
     * one native window, so presenting after each auxiliary terminal redraw
     * repaints that whole window several times. Batch those refreshes into one
     * presentation after all requested panes have been updated.
     */
    sdl_present_batch_begin();

    /* Display inventory */
    if (p_ptr->window & (PW_INVEN))
    {
        sdl_refresh_supporting_panes_layout_deferred();
        if (p_ptr->update)
            update_stuff();
        if (p_ptr->redraw)
            redraw_stuff();
        p_ptr->window &= ~(PW_INVEN);
        fix_inven();
    }

    /* Display supplies */
    if (p_ptr->window & (PW_SUPPLY))
    {
        sdl_refresh_supporting_panes_layout_deferred();
        if (p_ptr->update)
            update_stuff();
        if (p_ptr->redraw)
            redraw_stuff();
        p_ptr->window &= ~(PW_SUPPLY);
        fix_supplies();
    }

    /* Display monster list */
    if (p_ptr->window & (PW_MONLIST))
    {
        p_ptr->window &= ~(PW_MONLIST);
        fix_monlist();
    }

    /* Display equipment */
    if (p_ptr->window & (PW_EQUIP))
    {
        log_trace("window_stuff: PW_EQUIP flag set, calling fix_equip()");
        p_ptr->window &= ~(PW_EQUIP);
        fix_equip();
        log_trace("window_stuff: fix_equip() completed");

        /* Also trigger quiver redraw since quiver is part of equipment */
        p_ptr->redraw |= (PR_QUIVER);
    }

    /* Display player (mode 0) */
    if (p_ptr->window & (PW_PLAYER_0))
    {
        p_ptr->window &= ~(PW_PLAYER_0);
        fix_player_0();
    }

    /* Display message/combat panes.  Either source can feed either pane when
     * its in-pane filter is set to Combined, Log only, or Combat only. */
    if (p_ptr->window & (PW_COMBAT_ROLLS | PW_MESSAGE))
    {
        u32b requested_log_flags =
            p_ptr->window & (PW_COMBAT_ROLLS | PW_MESSAGE);

        p_ptr->window &= ~(PW_COMBAT_ROLLS | PW_MESSAGE);
        fix_log_panes(requested_log_flags);
    }

    /* Display monster recall */
    if (p_ptr->window & (PW_MONSTER))
    {
        p_ptr->window &= ~(PW_MONSTER);
        fix_monster();
    }

    sdl_present_batch_end();

    // log_trace("window_stuff: completed all window updates");
}

/*
 * Handle "p_ptr->update" and "p_ptr->redraw" and "p_ptr->window"
 */
void handle_stuff(void)
{
    log_trace("handle_stuff: starting (update=0x%08X, redraw=0x%08X, window=0x%08X)",
              p_ptr->update, p_ptr->redraw, p_ptr->window);

    /* Update stuff */
    if (p_ptr->update)
        update_stuff();

    /* Redraw stuff */
    if (p_ptr->redraw)
        redraw_stuff();

    /* Window stuff */
    if (p_ptr->window)
        window_stuff();

    log_trace("handle_stuff: completed");
}
