/* File: fs/load-dungeon.c -- carved from load.c (shares state via fs/load-internal.h) */

#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "log/log.h"
#include "player/killer.h"
#include "score/score_guid.h"
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include "metarun.h"
#include "fs/load-internal.h"

/*
 * Read the dungeon
 *
 * The monsters/objects must be loaded in the same order
 * that they were stored, since the actual indexes matter.
 *
 * Note that the size of the dungeon is now hard-coded to
 * DUNGEON_HGT by DUNGEON_WID, and any dungeon with another
 * size will be silently discarded by this routine.
 *
 * Note that dungeon objects, including objects held by monsters, are
 * placed directly into the dungeon, using "object_copy()", which will
 * copy "iy", "ix", and "held_m_idx", leaving "next_o_idx" blank for
 * objects held by monsters, since it is not saved in the savefile.
 *
 * After loading the monsters, the objects being held by monsters are
 * linked directly into those monsters.
 */
errr rd_dungeon(void)
{
    int i, y, x;

    s16b depth;
    s16b py, px;

    byte count;
    byte tmp8u;

    u16b limit;
    bool defer_player_placement = false;
    s16b header_py = 0;
    s16b header_px = 0;

    log_debug("rd_dungeon: ENTRY");
    log_trace("[load:%06u] === BEGIN DUNGEON ===", (unsigned)load_byte_offset);
    maybe_show_startup_loading_overlay();

    /*** Basic info ***/

    log_trace("[load:%06u] Reading dungeon header", (unsigned)load_byte_offset);
    /* Header info */
    rd_s16b(&depth);
    rd_s16b(&py);
    rd_s16b(&px);
    rd_byte(&p_ptr->cur_map_hgt);
    rd_byte(&p_ptr->cur_map_wid);

    log_debug("rd_dungeon: Read header - depth=%d, py=%d, px=%d, map=%dx%d", 
             depth, py, px, p_ptr->cur_map_hgt, p_ptr->cur_map_wid);
    header_py = py;
    header_px = px;

    /* Ignore illegal dungeons */
    if ((depth < 0) || (depth > MORGOTH_DEPTH))
    {
        note(format("Ignoring illegal dungeon depth (%d)", depth));
        return (0);
    }

    /* Ignore illegal dungeons */
    if ((p_ptr->cur_map_hgt > MAX_DUNGEON_HGT)
        || (p_ptr->cur_map_wid > MAX_DUNGEON_WID))
    {
        /* XXX XXX XXX */
        note(format("Ignoring illegal dungeon size (%d,%d).",
            p_ptr->cur_map_hgt, p_ptr->cur_map_wid));
        return (0);
    }

    /* The savefile header's player position is used later for player placement.
     *
     * The FOV/view code assumes the player is fully inside the outer walls.
     * If the save was written mid-crash/mid-transition, this can be (0,0) or
     * otherwise unsafe, which would crash on the first update_view().
     *
     * Treat unsafe positions as "repairable corruption" and defer placement
     * until after monsters are loaded, so we can pick an unoccupied grid. */
    if ((px < 0) || (py < 0) || (px >= p_ptr->cur_map_wid)
        || (py >= p_ptr->cur_map_hgt))
    {
        log_warn("rd_dungeon: Savefile has out-of-bounds player location py=%d px=%d (map=%dx%d); will repair",
                 py, px, p_ptr->cur_map_hgt, p_ptr->cur_map_wid);
        defer_player_placement = true;
    }
    else if (!in_bounds_fully(py, px))
    {
        log_warn("rd_dungeon: Savefile has boundary player location py=%d px=%d (map=%dx%d); will repair",
                 py, px, p_ptr->cur_map_hgt, p_ptr->cur_map_wid);
        defer_player_placement = true;
    }
    else
    {
        log_debug("rd_dungeon: Player position valid");
    }

    /* Clear per-grid entity maps; savefiles store entities separately. */
    for (y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            cave_o_idx[y][x] = 0;
            cave_m_idx[y][x] = 0;
        }
    }

    /*** Run length decoding of cave_info ***/

    log_trace("[load:%06u] === BEGIN CAVE_INFO RLE ===", (unsigned)load_byte_offset);
    /* Load the dungeon data */
    for (x = y = 0; y < p_ptr->cur_map_hgt;)
    {
        maybe_show_startup_loading_overlay();
        /* Grab RLE info */
        rd_byte(&count);
        rd_byte(&tmp8u);

        /* Apply the RLE info */
        for (i = count; i > 0; i--)
        {
            /* Extract "info" */
            cave_info[y][x] = tmp8u;

            /* Advance/Wrap */
            if (++x >= p_ptr->cur_map_wid)
            {
                /* Wrap */
                x = 0;

                /* Advance/Wrap */
                if (++y >= p_ptr->cur_map_hgt)
                    break;
            }
        }
    }
    log_trace("[load:%06u] === END CAVE_INFO RLE ===", (unsigned)load_byte_offset);

    /* Persisted high-bit cave_info flags (e.g. CAVE_CHASM_AREA).
     *
     * - New saves (0.9.1 extra>=8): mandatory block.
     * - Older saves: no block. */
    if (savefile_has_cave_info_hi)
    {
        const u16b CAVE_INFO_HI_MAGIC = 0xC1F0;
        u16b magic = 0;
        rd_u16b(&magic);
        if (magic != CAVE_INFO_HI_MAGIC)
        {
            note(format("Invalid cave_info_hi marker 0x%04X", magic));
            return (-1);
        }

        log_trace("[load:%06u] === BEGIN CAVE_INFO_HI RLE ===", (unsigned)load_byte_offset);
        for (x = y = 0; y < p_ptr->cur_map_hgt;)
        {
            maybe_show_startup_loading_overlay();
            rd_byte(&count);
            rd_byte(&tmp8u);

            for (i = count; i > 0; i--)
            {
                cave_info[y][x] |= ((u16b)tmp8u) << 8;

                if (++x >= p_ptr->cur_map_wid)
                {
                    x = 0;
                    if (++y >= p_ptr->cur_map_hgt)
                        break;
                }
            }
        }
        log_trace("[load:%06u] === END CAVE_INFO_HI RLE ===", (unsigned)load_byte_offset);
    }

    /* Ensure per-tile chasm partition tag exists across the entire partition bounds.
     * Older saves (and some generation paths) may not have stored/covered this. */
    {
        level_layout_info layout;
        level_layout_info_current(&layout);
        if (layout.partition_count > 0)
        {
            for (int yy = 0; yy < p_ptr->cur_map_hgt; ++yy)
            {
                for (int xx = 0; xx < p_ptr->cur_map_wid; ++xx)
                {
                    if (level_partition_kind_for_point(yy, xx) == LEVEL_PART_CHASM)
                        cave_info[yy][xx] |= CAVE_CHASM_AREA;
                }
            }
        }
    }

    /* Note: door-choices are only probed after cave_color for current saves. */

    /*** Run length decoding of cave_feat ***/

    log_trace("[load:%06u] === BEGIN CAVE_FEAT RLE ===", (unsigned)load_byte_offset);
    /* Load the dungeon data */
    for (x = y = 0; y < p_ptr->cur_map_hgt;)
    {
        maybe_show_startup_loading_overlay();
        /* Grab RLE info */
        rd_byte(&count);
        rd_byte(&tmp8u);

        /* Apply the RLE info */
        for (i = count; i > 0; i--)
        {
            /* Extract "feat" */
            cave_set_feat(y, x, tmp8u);

            /* Advance/Wrap */
            if (++x >= p_ptr->cur_map_wid)
            {
                /* Wrap */
                x = 0;

                /* Advance/Wrap */
                if (++y >= p_ptr->cur_map_hgt)
                    break;
            }
        }
    }
    log_trace("[load:%06u] === END CAVE_FEAT RLE ===", (unsigned)load_byte_offset);

    /* Back-compat probe: some saves wrote door-choices here, before cave_color. */
    log_trace("[load:%06u] === PROBE FOR DOOR_CHOICES (BEFORE CAVE_COLOR) ===", (unsigned)load_byte_offset);
    {
        const u16b DOOR_CHOICES_MAGIC = 0xD00D;
        u16b maybe_magic;
        rd_u16b(&maybe_magic);
        log_trace("[load:%06u] Probe value: 0x%04X (expecting 0x%04X for magic)", 
                 (unsigned)(load_byte_offset - 2), (unsigned)maybe_magic, (unsigned)DOOR_CHOICES_MAGIC);
        if (maybe_magic == DOOR_CHOICES_MAGIC) {
            byte n = 0;
            rd_byte(&n);
            byte buf[64];
            int to_read = (n > 64) ? 64 : n;
            for (int i2 = 0; i2 < to_read; ++i2) rd_byte(&buf[i2]);
            for (int i2 = to_read; i2 < n; ++i2) { byte skip; rd_byte(&skip); }
            log_debug("Read door-choices block before cave_color (compat): magic=0x%04X, len=%d (used=%d)", DOOR_CHOICES_MAGIC, n, to_read);
            styles_load_level_door_choices(buf, to_read);
        } else {
            /* Not magic: this was actually the first (count,value) pair for cave_color RLE. */
            color_rle_pair_prefetched = true;
            color_rle_count_prefetch = (byte)(maybe_magic & 0x00FF);
            color_rle_value_prefetch = (byte)((maybe_magic >> 8) & 0x00FF);
            log_debug("No door-choices before cave_color; staged RLE pair count=%u value=0x%02X", (unsigned)color_rle_count_prefetch, (unsigned)color_rle_value_prefetch);
        }
    }

    /*** Run length decoding of cave_color (style encoding) ***/
    log_trace("[load:%06u] === BEGIN CAVE_COLOR RLE ===", (unsigned)load_byte_offset);
    for (x = y = 0; y < p_ptr->cur_map_hgt;) {
        maybe_show_startup_loading_overlay();
        /* Grab RLE info, using prefetched pair if available first */
        if (color_rle_pair_prefetched) {
            count = color_rle_count_prefetch;
            tmp8u = color_rle_value_prefetch;
            color_rle_pair_prefetched = false;
            log_trace("[load:%06u] Using prefetched cave_color RLE pair: count=%u value=0x%02X", 
                     (unsigned)load_byte_offset, (unsigned)count, (unsigned)tmp8u);
        } else {
            rd_byte(&count);
            rd_byte(&tmp8u);
        }
        /* Apply the RLE info */
        for (i = count; i > 0; i--) {
            cave_color[y][x] = tmp8u;
            if (++x >= p_ptr->cur_map_wid) {
                x = 0;
                if (++y >= p_ptr->cur_map_hgt) break;
            }
        }
    }
    log_trace("[load:%06u] === END CAVE_COLOR RLE ===", (unsigned)load_byte_offset);

    /* Optional extension: persisted door style variant choices (new saves, after cave_color) */
    log_trace("[load:%06u] === PROBE FOR DOOR_CHOICES (AFTER CAVE_COLOR) ===", (unsigned)load_byte_offset);
    {
        const u16b DOOR_CHOICES_MAGIC = 0xD00D;
        u16b maybe_magic;
        rd_u16b(&maybe_magic);
        log_debug("Probe after cave_color: 0x%04X", maybe_magic);
        if (maybe_magic == DOOR_CHOICES_MAGIC) {
            byte n = 0;
            rd_byte(&n);
            byte buf[64];
            int to_read = (n > 64) ? 64 : n;
            for (int i = 0; i < to_read; ++i) rd_byte(&buf[i]);
            /* If payload in file was larger than buffer, skip extras */
            for (int i = to_read; i < n; ++i) { byte skip; rd_byte(&skip); }
            log_debug("Read door-choices block after cave_color: magic=0x%04X, len=%d (used=%d)", DOOR_CHOICES_MAGIC, n, to_read);
            styles_load_level_door_choices(buf, to_read);
        } else {
            /* Not our magic; interpret it as the first two bytes of the next section. */
            objects_count_prefetch = maybe_magic;
            log_debug("No door-choices after cave_color; staged objects count prefetch=%u", (unsigned)objects_count_prefetch);
        }
    }

    /* Optional extension: rewired-trap difficulty (0.9.7.2+).  New saves always
     * write the door-choices block above, so the stream is positioned exactly
     * here; older saves lack this block and skip it via the version gate. */
    if (savefile_has_cave_rewired)
    {
        const u16b CAVE_REWIRED_MAGIC = 0xC2F0;
        u16b magic = 0;
        rd_u16b(&magic);
        if (magic != CAVE_REWIRED_MAGIC)
        {
            note(format("Invalid cave_rewired marker 0x%04X", magic));
            return (-1);
        }

        log_trace("[load:%06u] === BEGIN CAVE_REWIRED RLE ===", (unsigned)load_byte_offset);
        for (x = y = 0; y < p_ptr->cur_map_hgt;)
        {
            maybe_show_startup_loading_overlay();
            rd_byte(&count);
            rd_byte(&tmp8u);

            for (i = count; i > 0; i--)
            {
                cave_rewired[y][x] = tmp8u;

                if (++x >= p_ptr->cur_map_wid)
                {
                    x = 0;
                    if (++y >= p_ptr->cur_map_hgt)
                        break;
                }
            }
        }
        log_trace("[load:%06u] === END CAVE_REWIRED RLE ===", (unsigned)load_byte_offset);
    }

    /*** Player ***/

    /* Load depth */
    p_ptr->depth = depth;

    /* Place player in dungeon (unless we need to repair after loading monsters) */
    if (!defer_player_placement)
    {
        if (!player_place(py, px))
        {
            log_error("Failed to place player at (%d,%d) in dungeon (depth=%d, map=%dx%d)", py, px, depth, p_ptr->cur_map_hgt, p_ptr->cur_map_wid);
            note(format("Cannot place player (%d,%d)!", py, px));
            return (-1);
        }
        log_debug("Player placed successfully at (%d,%d)", py, px);
    }
    else
    {
        note("Repairing invalid player location in savefile...");
    }

    /*** Objects ***/

    log_trace("[load:%06u] === BEGIN OBJECTS ===", (unsigned)load_byte_offset);
    /* Read the item count (possibly pre-fetched if no door choices block) */
    {
        u16b probe = 0;
        bool used_prefetch = false;
        if (objects_count_prefetch != 0xFFFF) {
            probe = objects_count_prefetch;
            objects_count_prefetch = 0xFFFF; /* reset */
            used_prefetch = true;
            log_trace("[load:%06u] Using prefetched objects count: %u", (unsigned)load_byte_offset, (unsigned)probe);
        } else {
            rd_u16b(&probe);
        }
        /* Validate the count; fall back to a direct read if suspicious */
        if (probe == 0 || (z_info && probe > z_info->o_max)) {
            if (used_prefetch) {
                log_warn("Prefetched objects count looked invalid (%u); reading directly", (unsigned)probe);
                rd_u16b(&limit);
            } else {
                limit = probe;
            }
        } else {
            limit = probe;
        }
    }
    log_debug("Loading %d objects from dungeon (limit=%u)", limit - 1, (unsigned)limit);

    /* Verify maximum */
    if (limit > z_info->o_max)
    {
        log_error("Too many objects in savefile: limit=%d, z_info->o_max=%d", limit, z_info->o_max);
        note(format("Too many (%d) object entries!", limit));
        return (-1);
    }

    /* Read the dungeon items */
    for (i = 1; i < limit; i++)
    {
        if ((i & 31) == 0)
            maybe_show_startup_loading_overlay();
        object_type* i_ptr;
        object_type object_type_body;

        s16b o_idx;
        object_type* o_ptr;

        /* Get the object */
        i_ptr = &object_type_body;

        /* Wipe the object */
        object_wipe(i_ptr);

        /* Read the item */
        if (rd_item(i_ptr))
        {
            log_error("Error reading dungeon item %d of %d", i, limit - 1);
            note("Error reading item");
            return (-1);
        }

        /* Make an object */
        o_idx = o_pop();

        /* Paranoia */
        if (o_idx != i)
        {
            log_error("Cannot place object %d: o_pop() returned %d (expected match)", i, o_idx);
            note(format("Cannot place object %d!", i));
            return (-1);
        }

        /* Get the object */
        o_ptr = &o_list[o_idx];

        /* Structure Copy */
        object_copy(o_ptr, i_ptr);

        /* Dungeon floor */
        if (!i_ptr->held_m_idx)
        {
            int x = i_ptr->ix;
            int y = i_ptr->iy;

            /* ToDo: Verify coordinates */

            /* Link the object to the pile */
            o_ptr->next_o_idx = cave_o_idx[y][x];

            /* Link the floor to the object */
            cave_o_idx[y][x] = o_idx;
        }
    }
    log_trace("[load:%06u] === END OBJECTS ===", (unsigned)load_byte_offset);

    /*** Monsters ***/

    log_trace("[load:%06u] === BEGIN MONSTERS ===", (unsigned)load_byte_offset);
    /* Read the monster count */
    rd_u16b(&limit);
    if (limit == 0) {
        u16b retry_m = 0;
        rd_u16b(&retry_m);
        if (retry_m > 0 && retry_m <= MAX_MONSTERS) {
            log_warn("Monsters count was 0; using recovery read=%u", (unsigned)retry_m);
            limit = retry_m;
        }
    }
    log_debug("Loading %d monsters from dungeon (limit=%u)", limit - 1, (unsigned)limit);

    /* Hack -- verify */
    if (limit > MAX_MONSTERS)
    {
        note(format("Too many (%d) monster entries!", limit));
        return (-1);
    }

    /* Read the monsters */
    for (i = 1; i < limit; i++)
    {
        if ((i & 31) == 0)
            maybe_show_startup_loading_overlay();
        monster_type* n_ptr;
        monster_type monster_type_body;

        /* Get local monster */
        n_ptr = &monster_type_body;

        /* Clear the monster */
        memset(n_ptr, 0, sizeof(monster_type));

    /* Read the monster */
    rd_monster(n_ptr);

        /* Place monster in dungeon */
        if (monster_place(n_ptr->fy, n_ptr->fx, n_ptr) != i)
        {
            log_warn("Cannot place monster %d at (%d,%d)", i, n_ptr->fy, n_ptr->fx);
            note(format("Cannot place monster %d", i));
            return (-1);
        }
    }
    log_trace("[load:%06u] === END MONSTERS ===", (unsigned)load_byte_offset);

    /*** Player (repair path) ***/

    if (defer_player_placement)
    {
        int ry = -1, rx = -1;
        bool found = false;

        /* Enforce a permanent wall boundary to keep view/FOV safe. */
        if ((p_ptr->cur_map_hgt >= 2) && (p_ptr->cur_map_wid >= 2))
        {
            int yy, xx;
            for (xx = 0; xx < p_ptr->cur_map_wid; ++xx)
            {
                cave_set_feat(0, xx, FEAT_WALL_PERM);
                cave_set_feat(p_ptr->cur_map_hgt - 1, xx, FEAT_WALL_PERM);
            }
            for (yy = 0; yy < p_ptr->cur_map_hgt; ++yy)
            {
                cave_set_feat(yy, 0, FEAT_WALL_PERM);
                cave_set_feat(yy, p_ptr->cur_map_wid - 1, FEAT_WALL_PERM);
            }
        }

        /* Prefer a staircase for recovery. */
        if (depth == 0)
        {
            for (y = 1; y < p_ptr->cur_map_hgt - 1 && !found; ++y)
            {
                for (x = 1; x < p_ptr->cur_map_wid - 1; ++x)
                {
                    if (cave_down_stairs_bold(y, x) && (cave_m_idx[y][x] == 0))
                    {
                        ry = y;
                        rx = x;
                        found = true;
                        break;
                    }
                }
            }
        }
        else
        {
            for (y = 1; y < p_ptr->cur_map_hgt - 1 && !found; ++y)
            {
                for (x = 1; x < p_ptr->cur_map_wid - 1; ++x)
                {
                    if (cave_up_stairs_bold(y, x) && (cave_m_idx[y][x] == 0))
                    {
                        ry = y;
                        rx = x;
                        found = true;
                        break;
                    }
                }
            }
            for (y = 1; y < p_ptr->cur_map_hgt - 1 && !found; ++y)
            {
                for (x = 1; x < p_ptr->cur_map_wid - 1; ++x)
                {
                    if (cave_down_stairs_bold(y, x) && (cave_m_idx[y][x] == 0))
                    {
                        ry = y;
                        rx = x;
                        found = true;
                        break;
                    }
                }
            }
        }

        /* Fallback: any empty, non-chasm, unoccupied floor. */
        if (!found)
        {
            for (y = 1; y < p_ptr->cur_map_hgt - 1 && !found; ++y)
            {
                for (x = 1; x < p_ptr->cur_map_wid - 1; ++x)
                {
                    if (cave_empty_bold(y, x))
                    {
                        ry = y;
                        rx = x;
                        found = true;
                        break;
                    }
                }
            }
        }

        /* Final fallback: clamp to center and search outward. */
        if (!found)
        {
            int cy = p_ptr->cur_map_hgt / 2;
            int cx = p_ptr->cur_map_wid / 2;
            int max_r = MAX(p_ptr->cur_map_hgt, p_ptr->cur_map_wid);

            if (cy < 1)
                cy = 1;
            if (cx < 1)
                cx = 1;
            if (cy > p_ptr->cur_map_hgt - 2)
                cy = p_ptr->cur_map_hgt - 2;
            if (cx > p_ptr->cur_map_wid - 2)
                cx = p_ptr->cur_map_wid - 2;

            for (int r = 0; (r <= max_r) && !found; ++r)
            {
                int y1 = cy - r;
                int y2 = cy + r;
                int x1 = cx - r;
                int x2 = cx + r;

                for (int yy = y1; (yy <= y2) && !found; ++yy)
                {
                    for (int xx = x1; xx <= x2; ++xx)
                    {
                        if (!in_bounds_fully(yy, xx))
                            continue;
                        if (cave_m_idx[yy][xx] != 0)
                            continue;
                        if (!cave_floor_bold(yy, xx))
                            continue;
                        if (cave_feat[yy][xx] == FEAT_CHASM)
                            continue;
                        ry = yy;
                        rx = xx;
                        found = true;
                        break;
                    }
                }
            }
        }

        if (!found)
        {
            log_error("rd_dungeon: Could not repair player location (header py=%d px=%d, depth=%d, map=%dx%d)",
                      header_py, header_px, depth, p_ptr->cur_map_hgt, p_ptr->cur_map_wid);
            note("Savefile recovery failed (no safe grid found).");
            return (-1);
        }

        /* Place the player now that the map and monsters are known. */
        if (!player_place(ry, rx))
        {
            log_error("rd_dungeon: Failed to place repaired player at (%d,%d) (header py=%d px=%d, depth=%d)",
                      ry, rx, header_py, header_px, depth);
            note("Savefile recovery failed (cannot place player).");
            return (-1);
        }

        log_warn("rd_dungeon: Repaired player location from (%d,%d) to (%d,%d) at depth %d",
                 header_py, header_px, ry, rx, depth);
    }

    /*** Holding ***/

    log_trace("[load:%06u] === BEGIN MONSTER OBJECT HOLDING ===", (unsigned)load_byte_offset);
    /* Reacquire objects */
    for (i = 1; i < o_max; ++i)
    {
        object_type* o_ptr;

        monster_type* m_ptr;

        /* Get the object */
        o_ptr = &o_list[i];

        /* Ignore dungeon objects */
        if (!o_ptr->held_m_idx)
            continue;

        /* Verify monster index */
        if (o_ptr->held_m_idx > MAX_MONSTERS)
        {
            note("Invalid monster index");
            return (-1);
        }

        /* Get the monster */
        m_ptr = &mon_list[o_ptr->held_m_idx];

        /* Link the object to the pile */
        o_ptr->next_o_idx = m_ptr->hold_o_idx;

        /* Link the monster to the object */
        m_ptr->hold_o_idx = i;
    }
    log_trace("[load:%06u] === END MONSTER OBJECT HOLDING ===", (unsigned)load_byte_offset);

    // dump the wandering monster information
    log_trace("[load:%06u] === BEGIN WANDERING MONSTERS ===", (unsigned)load_byte_offset);
    for (i = FLOW_WANDERING_HEAD; i <= FLOW_WANDERING_TAIL; i++)
    {
        maybe_show_startup_loading_overlay();
        rd_byte(&flow_center_y[i]);
        rd_byte(&flow_center_x[i]);
        rd_s16b(&wandering_pause[i]);

        update_flow(flow_center_y[i], flow_center_x[i], i);
    }
    log_trace("[load:%06u] === END WANDERING MONSTERS ===", (unsigned)load_byte_offset);

    /*** Success ***/

    /* After loading the level, pick the level primary style based on the
     * majority style encoded in cave_color, if any. */
    {
        int counts[256];
        int max_count = 0, best = -1;
        memset(counts, 0, sizeof(counts));
        for (y = 0; y < p_ptr->cur_map_hgt; ++y) {
            for (x = 0; x < p_ptr->cur_map_wid; ++x) {
                byte c = cave_color[y][x];
                if (c >= COLOR_STYLE_BASE) {
                    int idx = c - COLOR_STYLE_BASE;
                    if (idx >= 0 && idx < 256) {
                        int v = ++counts[idx];
                        if (v > max_count) { max_count = v; best = idx; }
                    }
                }
            }
        }
        if (best >= 0) styles_set_loaded_level_primary(best);
    }

    /* The dungeon is ready */
    character_dungeon = true;

    log_trace("[load:%06u] === END DUNGEON ===", (unsigned)load_byte_offset);
    log_debug("rd_dungeon: SUCCESS - dungeon loaded completely");

    /* Success */
    return (0);
}
