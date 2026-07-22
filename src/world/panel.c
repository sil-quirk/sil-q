#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"

/* Screen-cell rectangle [x1,x2) x [y1,y2) of an SDL overlay over the map. */
struct map_pane_span {
    int x1;
    int y1;
    int x2;
    int y2;
};

#define MAP_OVERLAY_SPAN_MAX (MAX_PANE_CONFIGS + 4)

static int map_center_clearance(void)
{
#ifdef USE_SDL
    return get_sdl_camera_center_clearance();
#else
    return SDL_CAMERA_CENTER_CLEARANCE_DEFAULT;
#endif
}

/* Collect every live SDL overlay which currently obscures the map. */
static int map_overlay_spans(struct map_pane_span* spans, int max_spans)
{
    int count = 0;

#ifdef USE_SDL
    int start_cols[MAP_OVERLAY_SPAN_MAX];
    int cols[MAP_OVERLAY_SPAN_MAX];
    int start_rows[MAP_OVERLAY_SPAN_MAX];
    int rows[MAP_OVERLAY_SPAN_MAX];

    count = sdl_map_overlay_map_coverages(max_spans, start_cols, cols,
        start_rows, rows);
    for (int i = 0; i < count; i++) {
        spans[i].x1 = start_cols[i];
        spans[i].y1 = start_rows[i];
        spans[i].x2 = start_cols[i] + cols[i];
        spans[i].y2 = start_rows[i] + rows[i];
    }
#else
    (void)spans;
    (void)max_spans;
#endif

    return count;
}

static int clamp_screen_center(int value, int size)
{
    if (size <= 1)
        return 0;
    if (value < 0)
        return 0;
    if (value >= size)
        return size - 1;
    return value;
}

static bool map_pane_span_contains(const struct map_pane_span* s, int y,
    int x, int clearance)
{
    return s && x >= s->x1 - clearance && x < s->x2 + clearance
        && y >= s->y1 - clearance && y < s->y2 + clearance;
}

static bool map_center_clear(int y, int x,
    const struct map_pane_span* spans, int span_count, int clearance)
{
    for (int i = 0; i < span_count; i++)
    {
        if (map_pane_span_contains(&spans[i], y, x, clearance))
            return false;
    }

    return true;
}

static bool map_zone_cell_clear(int y, int x, int screen_h, int screen_w,
    const struct map_pane_span* spans, int span_count, int clearance)
{
    if (x < clearance || x >= screen_w - clearance
        || y < clearance || y >= screen_h - clearance)
        return false;

    return map_center_clear(y, x, spans, span_count, clearance);
}

static int* map_zone_labels = NULL;
static int* map_zone_queue = NULL;
static int map_zone_capacity = 0;
static int map_zone_cached_h = 0;
static int map_zone_cached_w = 0;
static int map_zone_cached_clearance = 0;
static int map_zone_cached_span_count = 0;
static struct map_pane_span map_zone_cached_spans[MAP_OVERLAY_SPAN_MAX];
static bool map_zone_cache_valid = false;

static bool map_zone_cache_matches(int screen_h, int screen_w, int clearance,
    const struct map_pane_span* spans, int span_count)
{
    if (!map_zone_cache_valid || screen_h != map_zone_cached_h
        || screen_w != map_zone_cached_w
        || clearance != map_zone_cached_clearance
        || span_count != map_zone_cached_span_count)
        return false;

    for (int i = 0; i < span_count; i++)
    {
        if (spans[i].x1 != map_zone_cached_spans[i].x1
            || spans[i].y1 != map_zone_cached_spans[i].y1
            || spans[i].x2 != map_zone_cached_spans[i].x2
            || spans[i].y2 != map_zone_cached_spans[i].y2)
            return false;
    }

    return true;
}

static bool map_zone_cache_reserve(int cell_count)
{
    int* labels;
    int* queue;

    if (cell_count <= map_zone_capacity)
        return true;

    labels = mem_alloc_array(cell_count, int);
    queue = mem_alloc_array(cell_count, int);
    if (!labels || !queue)
    {
        mem_free_null(labels);
        mem_free_null(queue);
        return false;
    }

    mem_free_null(map_zone_labels);
    mem_free_null(map_zone_queue);
    map_zone_labels = labels;
    map_zone_queue = queue;
    map_zone_capacity = cell_count;
    return true;
}

/* Label each connected region left after screen edges and live overlays have
 * been expanded by the configured recenter distance. */
static bool map_zone_cache_build(int screen_h, int screen_w, int clearance,
    const struct map_pane_span* spans, int span_count)
{
    static const int step_y[4] = { -1, 0, 1, 0 };
    static const int step_x[4] = { 0, 1, 0, -1 };
    int cell_count = screen_h * screen_w;
    int next_label = 1;

    if (screen_h <= 0 || screen_w <= 0 || cell_count <= 0
        || !map_zone_cache_reserve(cell_count))
        return false;

    for (int y = 0; y < screen_h; y++)
    {
        for (int x = 0; x < screen_w; x++)
        {
            int index = y * screen_w + x;

            map_zone_labels[index] = map_zone_cell_clear(y, x, screen_h,
                screen_w, spans, span_count, clearance) ? -1 : 0;
        }
    }

    for (int start = 0; start < cell_count; start++)
    {
        int head = 0;
        int tail = 0;

        if (map_zone_labels[start] != -1)
            continue;

        map_zone_labels[start] = next_label;
        map_zone_queue[tail++] = start;

        while (head < tail)
        {
            int index = map_zone_queue[head++];
            int y = index / screen_w;
            int x = index % screen_w;

            for (int direction = 0; direction < 4; direction++)
            {
                int near_y = y + step_y[direction];
                int near_x = x + step_x[direction];
                int near_index;

                if (near_y < 0 || near_y >= screen_h
                    || near_x < 0 || near_x >= screen_w)
                    continue;

                near_index = near_y * screen_w + near_x;
                if (map_zone_labels[near_index] != -1)
                    continue;

                map_zone_labels[near_index] = next_label;
                map_zone_queue[tail++] = near_index;
            }
        }

        next_label++;
    }

    map_zone_cached_h = screen_h;
    map_zone_cached_w = screen_w;
    map_zone_cached_clearance = clearance;
    map_zone_cached_span_count = span_count;
    for (int i = 0; i < span_count; i++)
        map_zone_cached_spans[i] = spans[i];
    map_zone_cache_valid = true;
    return true;
}

/* Select the visible zone nearest the player's present screen position.  A
 * boundary-triggered recenter can move the target behind the direction of
 * travel, leaving more of the zone visible ahead of the player. */
static void map_safe_center(int* center_y, int* center_x,
    const struct map_pane_span* spans, int span_count, int anchor_y,
    int anchor_x, int travel_y, int travel_x)
{
    int screen_h = SCREEN_HGT;
    int screen_w = SCREEN_WID;
    int clearance = map_center_clearance();
    int cy = clamp_screen_center(screen_h / 2, screen_h);
    int cx = clamp_screen_center(screen_w / 2, screen_w);
    int zone_label = 0;
    long long nearest_distance = 0;
    long long sum_y = 0;
    long long sum_x = 0;
    int zone_cells = 0;
    long long center_distance = 0;
    bool have_zone_center = false;

    if (!map_zone_cache_matches(screen_h, screen_w, clearance, spans,
            span_count)
        && !map_zone_cache_build(screen_h, screen_w, clearance, spans,
            span_count))
        goto finish;

    if (anchor_y >= 0 && anchor_y < screen_h
        && anchor_x >= 0 && anchor_x < screen_w)
        zone_label = map_zone_labels[anchor_y * screen_w + anchor_x];

    if (zone_label <= 0)
    {
        for (int y = 0; y < screen_h; y++)
        {
            for (int x = 0; x < screen_w; x++)
            {
                int label = map_zone_labels[y * screen_w + x];
                long long dy;
                long long dx;
                long long distance;

                if (label <= 0)
                    continue;

                dy = y - anchor_y;
                dx = x - anchor_x;
                distance = dy * dy + dx * dx;
                if (zone_label <= 0 || distance < nearest_distance)
                {
                    zone_label = label;
                    nearest_distance = distance;
                }
            }
        }
    }

    if (zone_label <= 0)
        goto finish;

    for (int y = 0; y < screen_h; y++)
    {
        for (int x = 0; x < screen_w; x++)
        {
            if (map_zone_labels[y * screen_w + x] != zone_label)
                continue;

            sum_y += y;
            sum_x += x;
            zone_cells++;
        }
    }

    for (int y = 0; y < screen_h; y++)
    {
        for (int x = 0; x < screen_w; x++)
        {
            long long dy;
            long long dx;
            long long distance;

            if (map_zone_labels[y * screen_w + x] != zone_label)
                continue;

            /* Compare to the exact rational lead target without rounding it
             * into an overlay or another disconnected zone. */
            dy = (long long)y * zone_cells - sum_y
                + (long long)travel_y * clearance * zone_cells;
            dx = (long long)x * zone_cells - sum_x
                + (long long)travel_x * clearance * zone_cells;
            distance = dy * dy + dx * dx;
            if (!have_zone_center || distance < center_distance)
            {
                cy = y;
                cx = x;
                center_distance = distance;
                have_zone_center = true;
            }
        }
    }

finish:
    if (center_y)
        *center_y = cy;
    if (center_x)
        *center_x = cx;
}

/* Screen edges and every overlay are the same kind of hidden space. */
static bool map_cell_near_hidden(int y, int x,
    const struct map_pane_span* spans, int span_count)
{
    int clearance = map_center_clearance();

    if (x < clearance
        || x >= SCREEN_WID - clearance
        || y < clearance
        || y >= SCREEN_HGT - clearance)
    {
        return true;
    }

    return !map_center_clear(y, x, spans, span_count, clearance);
}

static bool map_travel_points_toward_hidden(int y, int x, int travel_y,
    int travel_x, const struct map_pane_span* spans, int span_count)
{
    int clearance = map_center_clearance();

    if ((travel_x < 0 && x < clearance)
        || (travel_x > 0 && x >= SCREEN_WID - clearance)
        || (travel_y < 0 && y < clearance)
        || (travel_y > 0 && y >= SCREEN_HGT - clearance))
    {
        return true;
    }

    for (int i = 0; i < span_count; i++)
    {
        const struct map_pane_span* s = &spans[i];

        if (!map_pane_span_contains(s, y, x, clearance))
            continue;

        if ((x < s->x1 && travel_x > 0)
            || (x >= s->x2 && travel_x < 0)
            || (y < s->y1 && travel_y > 0)
            || (y >= s->y2 && travel_y < 0))
        {
            return true;
        }

        /* This can happen briefly when an overlay appears over the player. */
        if (x >= s->x1 && x < s->x2 && y >= s->y1 && y < s->y2
            && (travel_y != 0 || travel_x != 0))
        {
            return true;
        }
    }

    return false;
}

/* Infer travel from actual player displacement rather than the last requested
 * command, which may have failed or may no longer describe forced movement. */
static void map_player_travel_direction(int py, int px, int* travel_y,
    int* travel_x)
{
    static bool have_previous = false;
    static int previous_y = 0;
    static int previous_x = 0;
    static int previous_depth = 0;
    static int previous_map_hgt = 0;
    static int previous_map_wid = 0;
    int delta_y = 0;
    int delta_x = 0;

    if (have_previous && p_ptr->depth == previous_depth
        && p_ptr->cur_map_hgt == previous_map_hgt
        && p_ptr->cur_map_wid == previous_map_wid)
    {
        delta_y = py - previous_y;
        delta_x = px - previous_x;

        /* Normal movement and leaps are local.  Do not turn teleports or a
         * newly generated level into a directional camera lead. */
        if (ABS(delta_y) > 2 || ABS(delta_x) > 2)
        {
            delta_y = 0;
            delta_x = 0;
        }
    }

    previous_y = py;
    previous_x = px;
    previous_depth = p_ptr->depth;
    previous_map_hgt = p_ptr->cur_map_hgt;
    previous_map_wid = p_ptr->cur_map_wid;
    have_previous = true;

    if (travel_y)
        *travel_y = SGN(delta_y);
    if (travel_x)
        *travel_x = SGN(delta_x);
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
    struct map_pane_span spans[MAP_OVERLAY_SPAN_MAX];
    int span_count = map_overlay_spans(spans, MAP_OVERLAY_SPAN_MAX);
    int center_y;
    int center_x;
    int min_wy;
    int min_wx;
    int max_wy;
    int max_wx;

    map_safe_center(&center_y, &center_x, spans, span_count,
        p_ptr->py - wy, p_ptr->px - wx, 0, 0);

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
 * By default, when the player comes within the configured clearance of hidden
 * space (the screen edge or an overlay), move the player behind the center of
 * the visible zone to leave more map visible in the direction of travel.
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
    int travel_y = 0;
    int travel_x = 0;
    struct map_pane_span spans[MAP_OVERLAY_SPAN_MAX];
    int span_count = map_overlay_spans(spans, MAP_OVERLAY_SPAN_MAX);

    int wy = p_ptr->wy;
    int wx = p_ptr->wx;
    bool do_center = center_player && (!p_ptr->running || !run_avoid_center);
    bool near_hidden = map_cell_near_hidden(
        py - wy, px - wx, spans, span_count);
    bool lead_recenter;

    map_player_travel_direction(py, px, &travel_y, &travel_x);
    lead_recenter = near_hidden && !do_center
        && map_travel_points_toward_hidden(py - wy, px - wx,
            travel_y, travel_x, spans, span_count);

    /* Always-center mode retains its stable meaning.  Normal boundary
     * recentering instead leads in the actual travel direction. */
    map_safe_center(&center_y, &center_x, spans, span_count,
        py - wy, px - wx, lead_recenter ? travel_y : 0,
        lead_recenter ? travel_x : 0);

    /* One rule for every camera boundary: once the player is within the
     * configured distance of a screen edge or live overlay, shift both axes. */
    if (do_center || near_hidden)
    {
        wy = py - center_y;
        wx = px - center_x;
    }

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
        /* A panel adjustment normally interrupts automatic actions.  When it
         * is only following active movement, however, stopping here truncates
         * both a distant SDL path and a normal run at the next camera shift.
         * Each movement mode already stops itself for monsters, objects,
         * terrain, and other real disturbances, so let it survive camera
         * maintenance. */
        if (!center_player && !p_ptr->running
            && !sdl_mouse_path_is_following())
        {
            disturb(0, 0);
        }
    }
}
