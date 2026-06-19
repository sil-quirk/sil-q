#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"

/* Screen-cell rectangle [x1,x2) x [y1,y2) of an SDL pane floating over the
 * map. */
struct map_pane_span {
    int x1;
    int y1;
    int x2;
    int y2;
};

#define MAP_PANE_SPAN_MAX 3

/* Collect the panes currently obscuring the map: the styled left panel pane
 * the combat overlay, and the overlay log band. */
static int map_pane_spans(struct map_pane_span* spans, int max_spans)
{
    int count = 0;

#ifdef USE_SDL
    int pane_start_col = 0;
    int pane_cols = 0;
    int pane_start_row = 0;
    int pane_rows = 0;

    if (count < max_spans
        && sdl_left_panel_pane_map_coverage(&pane_start_col, &pane_cols,
            &pane_start_row, &pane_rows))
    {
        spans[count].x1 = pane_start_col;
        spans[count].y1 = pane_start_row;
        spans[count].x2 = pane_start_col + pane_cols;
        spans[count].y2 = pane_start_row + pane_rows;
        count++;
    }

    if (count < max_spans
        && sdl_combat_overlay_pane_map_coverage(&pane_start_col, &pane_cols,
            &pane_start_row, &pane_rows))
    {
        spans[count].x1 = pane_start_col;
        spans[count].y1 = pane_start_row;
        spans[count].x2 = pane_start_col + pane_cols;
        spans[count].y2 = pane_start_row + pane_rows;
        count++;
    }

    if (count < max_spans
        && sdl_overlay_log_pane_map_coverage(&pane_start_col, &pane_cols,
            &pane_start_row, &pane_rows))
    {
        spans[count].x1 = pane_start_col;
        spans[count].y1 = pane_start_row;
        spans[count].x2 = pane_start_col + pane_cols;
        spans[count].y2 = pane_start_row + pane_rows;
        count++;
    }
#else
    (void)spans;
    (void)max_spans;
#endif

    return count;
}

/* Center the player in the unobscured map area when SDL panes float over it. */
static void map_safe_center(int* center_y, int* center_x,
    const struct map_pane_span* spans, int span_count)
{
    int cy = SCREEN_HGT / 2;
    int cx = SCREEN_WID / 2;
    int y1 = 0;
    int y2 = SCREEN_HGT;
    int x1 = 0;
    int x2 = SCREEN_WID;

    for (int i = 0; i < span_count; i++)
    {
        const struct map_pane_span* s = &spans[i];

        if (s->y1 <= cy && s->y2 > cy)
        {
            if (s->x1 <= cx && s->x2 > x1)
                x1 = s->x2;
            else if (s->x1 > cx && s->x1 < x2)
                x2 = s->x1;
        }

        if (s->x1 <= cx && s->x2 > cx)
        {
            if (s->y1 <= cy && s->y2 > y1)
                y1 = s->y2;
            else if (s->y1 > cy && s->y1 < y2)
                y2 = s->y1;
        }
    }

    if (span_count > 0)
    {
        if (x1 < 0)
            x1 = 0;
        if (x2 > SCREEN_WID)
            x2 = SCREEN_WID;
        if (y1 < 0)
            y1 = 0;
        if (y2 > SCREEN_HGT)
            y2 = SCREEN_HGT;

        if (x2 > x1)
            cx = x1 + (x2 - x1) / 2;
        if (y2 > y1)
            cy = y1 + (y2 - y1) / 2;
    }

    if (center_y)
        *center_y = cy;
    if (center_x)
        *center_x = cx;
}

static int floor_div_int(int value, int divisor)
{
    int quot;
    int rem;

    if (divisor <= 0)
        return 0;

    quot = value / divisor;
    rem = value % divisor;
    if (rem != 0 && value < 0)
        quot--;
    return quot;
}

/*
 * How close (as a fraction of the viewport) the player may drift from the
 * centre of a small viewport before the view recentres on them.  Smaller =>
 * the player is kept nearer the middle and the view recentres more often;
 * larger => a bigger "dead zone" and less frequent recentring.  At 2 the dead
 * zone spans half the viewport; tend toward 3-4 for a tighter, more centred
 * feel.
 */
#define SCROLL_RECENTER_DIVISOR 3

/*
 * Scroll one axis so the player sits comfortably inside a viewport that spans
 * screen cells [lo, hi).  Parameterised on a sub-window so it can be applied to
 * just the map area the styled pane leaves clear: passing the reduced range
 * keeps the player out of the pane while still scrolling normally, instead of
 * being pinned against the pane edge.
 *
 *   p    = player map coordinate on this axis (py or px)
 *   w    = current map offset on this axis (p_ptr->wy or ->wx)
 *   lo   = first usable screen cell (0, or just past a top/left pane)
 *   hi   = one-past the last usable screen cell (SCREEN_*, or a bottom/right pane)
 *   panel= PANEL_HGT / PANEL_WID
 *   big  = margin used on a large viewport (the classic 13 / 17)
 *
 * Small viewports recentre the player on the middle once they wander past the
 * margin (so the camera keeps up with them); large viewports keep the classic
 * panel-aligned scroll.  When [lo,hi) is the whole screen and the viewport is
 * large this reduces to the original logic, so the no-pane case is unchanged.
 */
static int scroll_axis_within(int p, int w, int lo, int hi, int panel, int big)
{
    int screen = hi - lo;
    bool compact;
    int margin;
    int wv;

    if (screen < 1)
        screen = 1;

    compact = (screen < panel * 2);

    /* Work in the virtual frame where the viewport starts at screen cell 0. */
    wv = w + lo;

    if (compact)
    {
        margin = screen / SCROLL_RECENTER_DIVISOR;
        if (margin < 1)
            margin = 1;
        if (margin > (screen - 1) / 2)
            margin = (screen - 1) / 2;

        /* Recentre on the player once they drift past the margin. */
        if (p < wv + margin || p >= wv + screen - margin)
            wv = p - screen / 2;
    }
    else
    {
        margin = big;
        if (margin > (screen - panel) / 2)
            margin = (screen - panel) / 2;
        if (margin < 1)
            margin = 1;

        /* Classic panel-aligned jump near the edges. */
        if (p < wv + margin || p >= wv + screen - margin)
        {
            wv = floor_div_int(p - panel / 2, panel) * panel;

            /* The panel-aligned jump knows nothing of the [lo,hi) window; if
             * it would still leave the player outside it (i.e. under a pane),
             * recentre on the player instead.  The jump always lands well
             * inside a full-size viewport, so the classic feel is unchanged. */
            if (p < wv || p >= wv + screen)
                wv = p - screen / 2;
        }
    }

    return wv - lo;
}

/*
 * Modify the current panel to the given coordinates, adjusting only to
 * keep the viewport within the useful off-map scroll range, and return true
 * if anything done.
 *
 * Hack -- The surface should never be scrolled around.
 *
 * Note that monsters are no longer affected in any way by panel changes.
 *
 * As a total hack, whenever the current panel changes, we assume that
 * the "overhead view" window should be updated.
 */
bool modify_panel(int wy, int wx)
{
    struct map_pane_span spans[MAP_PANE_SPAN_MAX];
    int span_count = map_pane_spans(spans, MAP_PANE_SPAN_MAX);
    int center_y;
    int center_x;
    int min_wy;
    int min_wx;
    int max_wy;
    int max_wx;

    map_safe_center(&center_y, &center_x, spans, span_count);

    min_wy = -center_y;
    min_wx = -center_x;
    max_wy = p_ptr->cur_map_hgt - 1 - center_y;
    max_wx = p_ptr->cur_map_wid - 1 - center_x;

    if (max_wy < min_wy)
        max_wy = min_wy;
    if (max_wx < min_wx)
        max_wx = min_wx;

    /* Verify wy, adjust if needed */
    if (wy < min_wy)
        wy = min_wy;
    else if (wy > max_wy)
        wy = max_wy;

    /* Verify wx, adjust if needed */
    if (wx < min_wx)
        wx = min_wx;
    else if (wx > max_wx)
        wx = max_wx;

    /* React to changes */
    if ((p_ptr->wy != wy) || (p_ptr->wx != wx))
    {
        /* Save wy, wx */
        p_ptr->wy = wy;
        p_ptr->wx = wx;

        /* Redraw map */
        p_ptr->redraw |= (PR_MAP);

        /* Hack -- Window stuff */
        p_ptr->window |= (PW_OVERHEAD);

        /* Changed */
        return (true);
    }

    /* No change */
    return (false);
}

/*
 * Perform the minimum "whole panel" adjustment to ensure that the given
 * location is contained inside the current panel, and return true if any
 * such adjustment was performed.
 */
bool adjust_panel(int y, int x)
{
    int wy = p_ptr->wy;
    int wx = p_ptr->wx;

    /* Adjust as needed */
    while (y >= wy + SCREEN_HGT)
        wy += SCREEN_HGT;
    while (y < wy)
        wy -= SCREEN_HGT;

    /* Adjust as needed */
    while (x >= wx + SCREEN_WID)
        wx += SCREEN_WID;
    while (x < wx)
        wx -= SCREEN_WID;

    /* Use "modify_panel" */
    return (modify_panel(wy, wx));
}

/*
 * Change the current panel to the panel lying in the given direction.
 *
 * Return true if the panel was changed.
 */
bool change_panel(int dir)
{
    int wy = p_ptr->wy + ddy[dir] * PANEL_HGT;
    int wx = p_ptr->wx + ddx[dir] * PANEL_WID;

    /* Use "modify_panel" */
    return (modify_panel(wy, wx));
}

/*
 * Verify the current panel (relative to the player location).
 *
 * By default, when the player gets "too close" to the edge of the current
 * panel, the map scrolls one panel in that direction so that the player
 * is no longer so close to the edge.
 *
 * The "center_player" option allows the current panel to always be centered
 * around the player, which is very expensive, and also has some interesting
 * gameplay ramifications.
 */
void verify_panel(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;
    int center_y = SCREEN_HGT / 2;
    int center_x = SCREEN_WID / 2;
    struct map_pane_span spans[MAP_PANE_SPAN_MAX];
    int span_count = map_pane_spans(spans, MAP_PANE_SPAN_MAX);

    int wy = p_ptr->wy;
    int wx = p_ptr->wx;

    map_safe_center(&center_y, &center_x, spans, span_count);

    /*
     * Treat each floating pane like a sidebar: shrink the map playfield to
     * the screen cells the panes leave clear, then scroll the player inside
     * that reduced window with the normal logic (see scroll_axis_within).
     * This keeps the player tile from ever sliding under a pane without
     * pinning it to the pane edge -- the old clamp did the latter, which felt
     * like the camera was permanently centred.  Each pane hugs one screen
     * edge or corner; reserve the band along whichever edge it is nearest,
     * and for a corner pane reserve the axis that hides the fewest map cells
     * (a tall, narrow panel costs a column band; a short, wide one a row
     * band).
     */
    int play_y_lo = 0;
    int play_y_hi = SCREEN_HGT;
    int play_x_lo = 0;
    int play_x_hi = SCREEN_WID;

    for (int i = 0; i < span_count; i++)
    {
        const struct map_pane_span* s = &spans[i];
        int gap_left = s->x1;
        int gap_right = SCREEN_WID - s->x2;
        int gap_top = s->y1;
        int gap_bottom = SCREEN_HGT - s->y2;

        /* The pane hugs the edge whose gap is smaller; equal gaps mean it is
         * centred on that axis and so cannot be cleared by scrolling. */
        bool anchor_left = (gap_left < gap_right);
        bool anchor_top = (gap_top < gap_bottom);
        bool reserve_horizontal = (gap_left != gap_right);
        bool reserve_vertical = (gap_top != gap_bottom);

        /* Corner pane: reserve only the cheaper axis. */
        if (reserve_horizontal && reserve_vertical)
        {
            int horiz_cost = (anchor_left ? s->x2 : (SCREEN_WID - s->x1))
                * SCREEN_HGT;
            int vert_cost = (anchor_top ? s->y2 : (SCREEN_HGT - s->y1))
                * SCREEN_WID;

            if (horiz_cost <= vert_cost)
                reserve_vertical = false;
            else
                reserve_horizontal = false;
        }

        if (reserve_horizontal)
        {
            if (anchor_left)
            {
                if (s->x2 > play_x_lo)
                    play_x_lo = s->x2;
            }
            else if (s->x1 < play_x_hi)
                play_x_hi = s->x1;
        }
        else if (reserve_vertical)
        {
            if (anchor_top)
            {
                if (s->y2 > play_y_lo)
                    play_y_lo = s->y2;
            }
            else if (s->y1 < play_y_hi)
                play_y_hi = s->y1;
        }
    }

    bool do_center = center_player && (!p_ptr->running || !run_avoid_center);

    /* Scroll vertically: centre on demand, else keep within the playfield. */
    if (do_center)
        wy = py - center_y;
    else
        wy = scroll_axis_within(py, wy, play_y_lo, play_y_hi, PANEL_HGT, 13);

    /* Scroll horizontally: centre on demand, else keep within the playfield. */
    if (do_center)
    {
        if (px != wx + center_x)
            wx = px - center_x;
    }
    else
        wx = scroll_axis_within(px, wx, play_x_lo, play_x_hi, PANEL_WID, 17);

    /* Scroll if needed */
    bool panel_changed = modify_panel(wy, wx);

    /* Safety net: never allow the player to remain outside the visible panel. */
    if (!panel_contains(py, px))
    {
        if (adjust_panel(py, px))
            panel_changed = true;
    }

    if (panel_changed)
    {
        /* Optional disturb on "panel change" */
        if (!center_player)
            disturb(0, 0);
    }
}
