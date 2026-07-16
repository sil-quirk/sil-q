/* File: cave-map-ui.c */

#include "cave-internal.h"

static bool hidden_left_panel_mask_span_at(int vy, int* start_col,
    int* width)
{
    int row_index;

    if (start_col)
        *start_col = 0;
    if (width)
        *width = 0;

    if (!g_hide_left_panel)
        return false;

    row_index = vy - g_hidden_left_panel_overlay_start_row;
    if (row_index < 0 || row_index >= g_hidden_left_panel_overlay_rows)
        return false;

    if (start_col)
        *start_col = g_hidden_left_panel_overlay_start_cols[row_index];
    if (width)
        *width = g_hidden_left_panel_overlay_widths[row_index];
    return g_hidden_left_panel_overlay_widths[row_index] > 0;
}

static bool hidden_left_panel_masked_span(int vy, int vx, int width)
{
    int mask_start = 0;
    int mask_width = 0;

    if (width <= 0)
        return false;
    if (!hidden_left_panel_mask_span_at(vy, &mask_start, &mask_width))
        return false;

    return (vx < mask_start + mask_width) && (vx + width > mask_start);
}

/* SDL map underlays can change even when the terminal glyph is unchanged. */
static void force_term_cell_redraw(int vx, int vy, int width)
{
    int x;
    int x2;

    if (!Term || !Term->old)
        return;

    if ((vy < 0) || (vy >= Term->hgt) || (vx >= Term->wid))
        return;

    if (vx < 0)
        vx = 0;

    if (width < 1)
        width = 1;

    x2 = vx + width - 1;
    if (x2 >= Term->wid)
        x2 = Term->wid - 1;

    for (x = vx; x <= x2; x++)
    {
        Term->old->a[vy][x] = 255;
        Term->old->c[vy][x] = 0;
        Term->old->ta[vy][x] = 255;
        Term->old->tc[vy][x] = 0;
        Term->old->story[vy][x] = 255;
    }

    if (vy < Term->y1)
        Term->y1 = vy;
    if (vy > Term->y2)
        Term->y2 = vy;
    if (vx < Term->x1[vy])
        Term->x1[vy] = vx;
    if (x2 > Term->x2[vy])
        Term->x2[vy] = x2;
}

/*
 * Move the cursor to a given map location.
 *
 * The main screen will always be at least 24x80 in size.
 */
void move_cursor_relative(int y, int x)
{
    int ky, kx;
    int vy, vx;
    int cell_w;

    /* Location relative to panel */
    ky = y - p_ptr->wy;

    /* Verify location */
    if ((ky < 0) || (ky >= SCREEN_HGT))
        return;

    /* Location relative to panel */
    kx = x - p_ptr->wx;

    /* Verify location */
    if ((kx < 0) || (kx >= SCREEN_WID))
        return;

    /* Location in window */
    vy = ky + ROW_MAP;

    /* Location in window */
    vx = kx + COL_MAP;

    if (use_bigtile)
        vx += kx;

    cell_w = use_bigtile ? 2 : 1;
    if (hidden_left_panel_masked_span(vy, vx, cell_w))
        return;

    /* Go there */
    if (use_bigtile)
        (void)Term_gotoxy_big(vx, vy);
    else
        (void)Term_gotoxy(vx, vy);
}

/*
 * Display an attr/char pair at the given map location
 *
 * Note the inline use of "panel_contains()" for efficiency.
 *
 * Note the use of "Term_queue_char()" for efficiency.
 *
 * The main screen will always be at least 24x80 in size.
 */
void print_rel(char c, byte a, int y, int x)
{
    int ky, kx;
    int vy, vx;
    int cell_w;

    /* Location relative to panel */
    ky = y - p_ptr->wy;

    /* Verify location */
    if ((ky < 0) || (ky >= SCREEN_HGT))
        return;

    /* Location relative to panel */
    kx = x - p_ptr->wx;

    /* Verify location */
    if ((kx < 0) || (kx >= SCREEN_WID))
        return;

    /* Location in window */
    vy = ky + ROW_MAP;

    /* Location in window */
    vx = kx + COL_MAP;

    if (use_bigtile)
        vx += kx;

    cell_w = use_bigtile ? 2 : 1;
    if (hidden_left_panel_masked_span(vy, vx, cell_w))
        return;

    /* Hack -- Queue it */
    Term_queue_char(vx, vy, a, c, 0, 0);

    if (use_bigtile)
    {
        /* Mega-Hack : Queue dummy char */
        if (a & 0x80)
            Term_queue_char(vx + 1, vy, 255, -1, 0, 0);
        else
            Term_queue_char(vx + 1, vy, TERM_WHITE, ' ', 0, 0);
    }
}

/*
 * Memorize interesting viewable object/features in the given grid
 *
 * This function should only be called on "legal" grids.
 *
 * This function will memorize the object and/or feature in the given grid,
 * if they are (1) see-able and (2) interesting.  Note that all objects are
 * interesting, all terrain features except floors (and invisible traps) are
 * interesting, and floors (and invisible traps) are interesting sometimes
 * (depending on various options involving the illumination of floor grids).
 *
 * The automatic memorization of all objects and non-floor terrain features
 * as soon as they are displayed allows incredible amounts of optimization
 * in various places, especially "map_info()" and this function itself.
 *
 * Note that the memorization of objects is completely separate from the
 * memorization of terrain features, preventing annoying floor memorization
 * when a detected object is picked up from a dark floor, and object
 * memorization when an object is dropped into a floor grid which is
 * memorized but out-of-sight.
 *
 * This function should be called every time the "memorization" of a grid
 * (or the object in a grid) is called into question, such as when an object
 * is created in a grid, when a terrain feature "changes" from "floor" to
 * "non-floor", and when any grid becomes "see-able" for any reason.
 *
 * This function is called primarily from the "update_view()" function, for
 * each grid which becomes newly "see-able".
 */
void note_spot(int y, int x)
{
    u16b info;

    object_type* o_ptr;

    /* Get cave info */
    info = cave_info[y][x];

    /* Require "seen" flag */
    if (!(info & (CAVE_SEEN)))
        return;

    /* Hack -- memorize objects */
    for (o_ptr = get_first_object(y, x); o_ptr; o_ptr = get_next_object(o_ptr))
    {
        /* Memorize objects */
        o_ptr->marked = true;
    }

    /* Hack -- memorize grids */
    if (!(info & (CAVE_MARK)))
    {
        /* Memorize some "boring" grids */
        if (cave_floorlike_bold(y, x))
        {
            /* Option -- memorize certain floors */
            if (info & (CAVE_GLOW))
            {
                /* Memorize */
                cave_info[y][x] |= (CAVE_MARK);
            }
        }

        /* Memorize all "interesting" grids */
        else
        {
            /* Memorize */
            cave_info[y][x] |= (CAVE_MARK);
        }
    }
}

/*
 * Redraw (on the screen) a given map location
 *
 * This function should only be called on "legal" grids.
 *
 * Note the inline use of "print_rel()" for efficiency.
 *
 * The main screen will always be at least 24x80 in size.
 */
void lite_spot(int y, int x)
{
    byte a;
    char c;
    byte ta;
    char tc;

    int ky, kx;
    int vy, vx;
    int cell_w;

#ifdef USE_SDL
    /* The retained side minimap includes grids outside the main viewport.
     * Notify it before the viewport checks below so off-screen monsters,
     * objects, and terrain do not leave stale cached cells. */
    sdl_side_map_pane_invalidate_cell(y, x);
#endif

    /* Location relative to panel */
    ky = y - p_ptr->wy;

    /* Verify location */
    if ((ky < 0) || (ky >= SCREEN_HGT))
        return;

    /* Location relative to panel */
    kx = x - p_ptr->wx;

    /* Verify location */
    if ((kx < 0) || (kx >= SCREEN_WID))
        return;

    /* Location in window */
    vy = ky + ROW_MAP;

    /* Location in window */
    vx = kx + COL_MAP;

    if (use_bigtile)
        vx += kx;

    cell_w = use_bigtile ? 2 : 1;
    if (hidden_left_panel_masked_span(vy, vx, cell_w))
        return;

    /* Hack -- redraw the grid */
    map_info(y, x, &a, &c, &ta, &tc);

    /* Hack -- Queue it */
    Term_queue_char(vx, vy, a, c, ta, tc);

    if (use_bigtile)
    {
        vx++;

        /* Mega-Hack : Queue dummy char */
        if (a & 0x80)
            Term_queue_char(vx, vy, 255, -1, 0, 0);
        else
            Term_queue_char(vx, vy, TERM_WHITE, ' ', TERM_WHITE, ' ');
    }

    if (!graphics_are_ascii())
    {
        bool force_visual_redraw = (cave_m_idx[y][x] < 0);

        if (!force_visual_redraw && mirror_monster_tile_facing
            && (cave_m_idx[y][x] > 0))
        {
            monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
            monster_race* r_ptr = &r_info[m_ptr->r_idx];

            force_visual_redraw =
                m_ptr->ml && (r_ptr->tile_facing != MONSTER_TILE_FACING_NONE);
        }

        if (force_visual_redraw)
            force_term_cell_redraw(vx - (use_bigtile ? 1 : 0), vy, cell_w);
    }
}

/*
 * Redraw (on the screen) the current map panel
 *
 * Note the inline use of "lite_spot()" for efficiency.
 *
 * The main screen will always be at least 24x80 in size.
 */
void prt_map(void)
{
    byte a;
    char c;
    byte ta;
    char tc;

    int y, x;
    int vy, vx;
    int ty, tx;
    int cell_w = use_bigtile ? 2 : 1;
    bool rage_map_filter_active = (!graphics_are_ascii() && p_ptr
        && !p_ptr->is_dead && p_ptr->rage);
    static bool last_rage_map_filter_active = false;
    bool force_rage_map_filter_refresh =
        (rage_map_filter_active != last_rage_map_filter_active);

    /* Assume screen */
    ty = p_ptr->wy + SCREEN_HGT;
    tx = p_ptr->wx + SCREEN_WID;

    /* Dump the map */
    for (y = p_ptr->wy, vy = ROW_MAP; y < ty; vy++, y++)
    {
        for (x = p_ptr->wx, vx = COL_MAP; x < tx; vx += cell_w, x++)
        {
            if (hidden_left_panel_masked_span(vy, vx, cell_w))
                continue;

            /* Check bounds */
            if (!in_bounds(y, x))
            {
                /* Outside the generated map must look exactly like unexplored
                 * space, or the viewport reveals the map boundary. */
                cave_feature_visual(&f_info[FEAT_NONE], &a, &c);
                Term_queue_char(vx, vy, a, c, a, c);
                if (use_bigtile)
                {
                    if (a & 0x80)
                        Term_queue_char(vx + 1, vy, 255, -1, 0, 0);
                    else
                        Term_queue_char(vx + 1, vy, TERM_WHITE, ' ',
                            TERM_WHITE, ' ');
                }
                continue;
            }

            /* Determine what is there */
            map_info(y, x, &a, &c, &ta, &tc);

            /* Hack -- Queue it */
            Term_queue_char(vx, vy, a, c, ta, tc);

            if (use_bigtile)
            {
                /* Mega-Hack : Queue dummy char */
                if (a & 0x80)
                    Term_queue_char(vx + 1, vy, 255, -1, 0, 0);
                else
                    Term_queue_char(vx + 1, vy, TERM_WHITE, ' ', TERM_WHITE,
                        ' ');
            }

            if (force_rage_map_filter_refresh
                || (!graphics_are_ascii() && (cave_m_idx[y][x] < 0)))
                force_term_cell_redraw(vx, vy, cell_w);
        }
    }

    last_rage_map_filter_active = rage_map_filter_active;
}

/*
 * Force every visible map grid to be repainted on the next Term_fresh().
 *
 * prt_map()/lite_spot() route through Term_queue_char(), which skips terminal
 * cells whose glyph is unchanged.  That lets the SDL map canvas keep stale
 * pixels across a layout change that reflows the map area without changing the
 * glyphs themselves -- e.g. the look command hiding/showing the left panel.
 * (This used to be papered over as a side effect of the look toggling the
 * terminal story font, whose per-cell flag transition dirtied every cell.)
 * Invalidate the cached glyph for each non-masked viewport cell so the flush
 * repaints it from scr, then leave the actual refresh to the caller.
 */
void force_map_redraw(void)
{
    int y, x, vy, vx;
    int cell_w = use_bigtile ? 2 : 1;
    int ty;
    int tx;

    if (!Term || !Term->old)
        return;

    ty = p_ptr->wy + SCREEN_HGT;
    tx = p_ptr->wx + SCREEN_WID;

    for (y = p_ptr->wy, vy = ROW_MAP; y < ty; vy++, y++)
    {
        for (x = p_ptr->wx, vx = COL_MAP; x < tx; vx += cell_w, x++)
        {
            if (hidden_left_panel_masked_span(vy, vx, cell_w))
                continue;

            force_term_cell_redraw(vx, vy, cell_w);
        }
    }
}

/*
 * Hack -- priority array (see below)
 *
 * Note that all "walls" always look like "secret doors" (see "map_info()").
 */
static const int priority_table[13][2] = {
    /* Dark */
    { FEAT_NONE, 2 },

    /* Floors */
    { FEAT_FLOOR, 5 },

    /* Walls */
    { FEAT_SECRET, 10 },

    /* Quartz */
    { FEAT_QUARTZ, 11 },

    /* Rubble */
    { FEAT_RUBBLE, 13 },

    /* Open doors */
    { FEAT_OPEN, 15 }, { FEAT_BROKEN, 15 },

    /* Closed doors */
    { FEAT_DOOR_HEAD + 0x00, 17 },

    /* Stairs */
    { FEAT_LESS, 25 }, { FEAT_MORE, 25 }, { FEAT_LESS_SHAFT, 25 },
    { FEAT_MORE_SHAFT, 25 },

    /* End */
    { 0, 0 }
};

/*
 * Hack -- a priority function (see below)
 */
static byte priority(byte a, char c)
{
    int i, p0, p1;

    feature_type* f_ptr;

    /* Scan the table */
    for (i = 0; true; i++)
    {
        /* Priority level */
        p1 = priority_table[i][1];

        /* End of table */
        if (!p1)
            break;

        /* Feature index */
        p0 = priority_table[i][0];

        /* Get the feature */
        f_ptr = &f_info[p0];

        /* Check character and attribute, accept matches */
        {
            byte fa;
            char fc;

            cave_feature_visual(f_ptr, &fa, &fc);
            if ((fc == c) && (fa == a))
                return (p1);
        }
    }

    /* Default */
    return (20);
}

/*
 * Display a "small-scale" map of the dungeon in the active Term.
 *
 * Note that this function must "disable" the special lighting effects so
 * that the "priority" function will work.
 *
 * Note the use of a specialized "priority" function to allow this function
 * to work with any graphic attr/char mappings, and the attempts to optimize
 * this function where possible.
 *
 * If "cy" and "cx" are not NULL, then returns the screen location at which
 * the player was displayed, so the cursor can be moved to that location,
 * and restricts the horizontal map size to SCREEN_WID.  Otherwise, nothing
 * is returned (obviously), and no restrictions are enforced.
 */
void display_map(int* cy, int* cx)
{
    int map_hgt, map_wid;
    int row, col;

    int x, y;
    int min_x, max_x, min_y, max_y;
    int explored_wid, explored_hgt;

    byte ta;
    char tc;

    byte tp;

    /* Large array on the stack */
    byte mp[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];

    monster_race* r_ptr = &r_info[0];

    /* Find the bounding box of explored areas */
    min_x = p_ptr->cur_map_wid;
    max_x = 0;
    min_y = p_ptr->cur_map_hgt;
    max_y = 0;

    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            /* Check if this grid has been seen */
            if (cave_info[y][x] & (CAVE_MARK))
            {
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
            }
        }
    }

    /* Calculate explored dimensions */
    explored_wid = (max_x - min_x + 1);
    explored_hgt = (max_y - min_y + 1);

    /* If nothing explored, fall back to full map */
    if (explored_wid < 1 || explored_hgt < 1)
    {
        min_x = 0;
        max_x = p_ptr->cur_map_wid - 1;
        min_y = 0;
        max_y = p_ptr->cur_map_hgt - 1;
        explored_wid = p_ptr->cur_map_wid;
        explored_hgt = p_ptr->cur_map_hgt;
    }

    /* Desired map height */
    map_hgt = Term->hgt - 2;
    map_wid = Term->wid - 2;

    /* Prevent accidents */
    if (map_hgt > explored_hgt)
        map_hgt = explored_hgt;
    if (map_wid > explored_wid)
        map_wid = explored_wid;

    /* Prevent accidents */
    if ((map_wid < 1) || (map_hgt < 1))
        return;

    /* Nothing here */
    ta = TERM_WHITE;
    tc = ' ';

    /* Clear the priorities */
    for (y = 0; y < map_hgt; ++y)
    {
        for (x = 0; x < map_wid; ++x)
        {
            /* No priority */
            mp[y][x] = 0;
        }
    }

    /* Clear the screen (but don't force a redraw) */
    clear_from(0);

    /* Corners */
    x = map_wid + 1;
    y = map_hgt + 1;

    /* Draw the corners */
    Term_putch(0, 0, ta, '+');
    Term_putch(x, 0, ta, '+');
    Term_putch(0, y, ta, '+');
    Term_putch(x, y, ta, '+');

    /* Draw the horizontal edges */
    for (x = 1; x <= map_wid; x++)
    {
        Term_putch(x, 0, ta, '-');
        Term_putch(x, y, ta, '-');
    }

    /* Draw the vertical edges */
    for (y = 1; y <= map_hgt; y++)
    {
        Term_putch(0, y, ta, '|');
        Term_putch(x, y, ta, '|');
    }

    /* Analyze the actual map (only explored area) */
    for (y = min_y; y <= max_y; y++)
    {
        for (x = min_x; x <= max_x; x++)
        {
            /* Scale based on explored area */
            row = ((y - min_y) * map_hgt / explored_hgt);
            col = ((x - min_x) * map_wid / explored_wid);

            if (use_bigtile)
                col = col & ~1;

            /* Get the attr/char at that map location */
            map_info(y, x, &ta, &tc, &ta, &tc);

            /* Get the priority of that attr/char */
            tp = priority(ta, tc);

            /* Examine boring grids */
            if ((tp == 20) && (cave_m_idx[y][x] > 0))
            {
                monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
                monster_race* r_ptr = &r_info[m_ptr->r_idx];

                /* Notice dangerous monsters */
                /* Sil-y: this may need some tweaking */
                tp = MAX(20, (int)r_ptr->level - p_ptr->depth + 20);

                /* Ignore invisible monsters */
                if (!m_ptr->ml)
                    tp = 20;
            }

            /* Save "best" */
            if (mp[row][col] < tp)
            {
                /* Add the character */
                Term_putch(col + 1, row + 1, ta, tc);

                if (use_bigtile)
                {
                    if (ta & 0x80)
                        Term_putch(col + 2, row + 1, 255, -1);
                    else
                        Term_putch(col + 2, row + 1, TERM_WHITE, ' ');
                }

                /* Save priority */
                mp[row][col] = tp;
            }
        }
    }

    /* Player location (scaled relative to explored area) */
    row = ((p_ptr->py - min_y) * map_hgt / explored_hgt);
    col = ((p_ptr->px - min_x) * map_wid / explored_wid);

    if (use_bigtile)
        col = col & ~1;

    /*** Make sure the player is visible ***/

    /* Get the "player" attr */
    cave_monster_visual(r_ptr, &ta, &tc);

    /* Draw the player */
    Term_putch(col + 1, row + 1, ta, tc);

    /* Return player location */
    if (cy != NULL)
        (*cy) = row + 1;
    if (cx != NULL)
        (*cx) = col + 1;
}

/*
 * Display a "small-scale" map of the dungeon.
 *
 * Note that the "player" is always displayed on the map.
 */
void do_cmd_view_map(void)
{
    int cy = 0;
    int cx = 0;
    cptr prompt = "Hit any key to continue";

    if (!p_ptr->is_dead && g_labyrinth_view_active)
    {
        msg_print("The labyrinth confounds your map.");
        return;
    }

    /* Save screen */
    screen_save();
    screen_push_supporting_panes_hidden();

    /* Note */
    prt("Please wait...", 0, 0);

    /* Flush */
    Term_fresh();

    /* Clear the screen */
    Term_clear();

    /* Display the map */
#ifdef USE_SDL
    {
        bool sdl_map = false;
        bool saved_hide_cursor = hide_cursor;

        sdl_minimap_begin();
        Term_fresh();
        sdl_map = sdl_display_pixel_map(&cy, &cx);
        if (sdl_map)
        {
            hide_cursor = true;
            (void)Term_set_cursor(false);
            Term_fresh();

            while (true)
            {
                char ch = inkey();
                int pan_dx = 0;
                int pan_dy = 0;
                int hint_index = -1;

                if (ch == UI_MENU_CLICK_WAKE_KEY
                    && sdl_minimap_take_hint_click(&hint_index))
                {
                    if (hint_index >= 0)
                        show_hint_message_screen(hint_index);
                    (void)sdl_display_pixel_map(&cy, &cx);
                    Term_fresh();
                    continue;
                }

                if (ch == '+' || ch == '=')
                {
                    (void)sdl_minimap_adjust_zoom(1);
                    Term_fresh();
                    continue;
                }

                if (ch == '-' || ch == '_')
                {
                    (void)sdl_minimap_adjust_zoom(-1);
                    Term_fresh();
                    continue;
                }

                switch (ch)
                {
                case '1': pan_dx = -1; pan_dy = 1; break;
                case '2': pan_dy = 1; break;
                case '3': pan_dx = 1; pan_dy = 1; break;
                case '4': pan_dx = -1; break;
                case '6': pan_dx = 1; break;
                case '7': pan_dx = -1; pan_dy = -1; break;
                case '8': pan_dy = -1; break;
                case '9': pan_dx = 1; pan_dy = -1; break;
                default: break;
                }

                if (pan_dx || pan_dy)
                {
                    (void)sdl_minimap_pan(pan_dx, pan_dy);
                    Term_fresh();
                    continue;
                }

                break;
            }

            hide_cursor = saved_hide_cursor;
            (void)Term_set_cursor(false);
            sdl_minimap_end();
            screen_pop_supporting_panes_hidden();
            screen_load();
            return;
        }

        hide_cursor = saved_hide_cursor;
        sdl_minimap_end();
    }
#endif
    {
        display_map(&cy, &cx);
    }

    /* Show the prompt */
    put_str(prompt, Term->hgt - 1, Term->wid / 2 - strlen(prompt) / 2);

    /* Hilite the player */
    Term_gotoxy(cx, cy);

    /* Flush the pixel map/prompt before waiting for input. */
    Term_fresh();

    /* Get any key */
    (void)inkey();

    /* Load screen */
    screen_pop_supporting_panes_hidden();
    screen_load();
}
