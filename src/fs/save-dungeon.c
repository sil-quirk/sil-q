/* File: fs/save-dungeon.c -- carved from save.c (shares state via fs/save-internal.h) */

#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "fs/save-internal.h"
#include <stdio.h>

/*
 * The cave grid flags that get saved in the savefile
 */
#define IMPORTANT_FLAGS_LO                                                     \
    (CAVE_MARK | CAVE_GLOW | CAVE_ICKY | CAVE_ROOM | CAVE_G_VAULT | CAVE_HIDDEN)
#define IMPORTANT_FLAGS_HI (CAVE_CHASM_AREA)
#define IMPORTANT_FLAGS_16 (IMPORTANT_FLAGS_LO | IMPORTANT_FLAGS_HI)

/*
 * Write the current dungeon
 */
void wr_dungeon(void)
{
    int i, y, x;

    byte tmp8u;

    byte count;
    byte prev_char;

    log_debug("Writing dungeon level %d (%dx%d)", p_ptr->depth, p_ptr->cur_map_hgt, p_ptr->cur_map_wid);
    log_trace("[save:%06u] === BEGIN DUNGEON ===", (unsigned)save_byte_offset);

    /*** Basic info ***/

    log_trace("[save:%06u] Writing dungeon header", (unsigned)save_byte_offset);
    /* Dungeon specific info follows */
    wr_s16b(p_ptr->depth);
    wr_s16b(p_ptr->py);
    wr_s16b(p_ptr->px);
    wr_byte(p_ptr->cur_map_hgt);
    wr_byte(p_ptr->cur_map_wid);
    log_trace("[save:%06u] Dungeon header written: depth=%d, py=%d, px=%d, hgt=%d, wid=%d", 
             (unsigned)save_byte_offset, p_ptr->depth, p_ptr->py, p_ptr->px, 
             p_ptr->cur_map_hgt, p_ptr->cur_map_wid);

    /*** Simple "Run-Length-Encoding" of cave_info ***/

    log_trace("[save:%06u] === BEGIN CAVE_INFO RLE ===", (unsigned)save_byte_offset);
    /* Note that this will induce two wasted bytes */
    count = 0;
    prev_char = 0;

    /* Dump the cave */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            /* Extract the important cave_info flags (low byte only). */
            u16b info = (u16b)(cave_info[y][x] & IMPORTANT_FLAGS_16);
            tmp8u = (byte)(info & 0x00FF);

            /* If the run is broken, or too full, flush it */
            if ((tmp8u != prev_char) || (count == MAX_UCHAR))
            {
                wr_byte((byte)count);
                wr_byte((byte)prev_char);
                prev_char = tmp8u;
                count = 1;
            }

            /* Continue the run */
            else
            {
                count++;
            }
        }
    }

    /* Flush the data (if any) */
    if (count)
    {
        wr_byte((byte)count);
        wr_byte((byte)prev_char);
    }
    log_trace("[save:%06u] === END CAVE_INFO RLE ===", (unsigned)save_byte_offset);

    /* Optional extension: save high-bit cave_info flags (e.g. CAVE_CHASM_AREA).
     *
     * This keeps the core cave_info RLE byte-sized for back-compat, while
     * allowing persistence of any "important" flags in bits 8..15. */
    log_trace("[save:%06u] === BEGIN CAVE_INFO_HI RLE ===", (unsigned)save_byte_offset);
    {
        const u16b CAVE_INFO_HI_MAGIC = 0xC1F0;

        wr_u16b(CAVE_INFO_HI_MAGIC);

        count = 0;
        prev_char = 0;

        for (y = 0; y < p_ptr->cur_map_hgt; y++)
        {
            for (x = 0; x < p_ptr->cur_map_wid; x++)
            {
                u16b info = (u16b)(cave_info[y][x] & IMPORTANT_FLAGS_16);
                tmp8u = (byte)((info >> 8) & 0x00FF);

                if ((tmp8u != prev_char) || (count == MAX_UCHAR))
                {
                    wr_byte((byte)count);
                    wr_byte((byte)prev_char);
                    prev_char = tmp8u;
                    count = 1;
                }
                else
                {
                    count++;
                }
            }
        }

        if (count)
        {
            wr_byte((byte)count);
            wr_byte((byte)prev_char);
        }
    }
    log_trace("[save:%06u] === END CAVE_INFO_HI RLE ===", (unsigned)save_byte_offset);

    /*** Simple "Run-Length-Encoding" of cave_feat ***/

    log_trace("[save:%06u] === BEGIN CAVE_FEAT RLE ===", (unsigned)save_byte_offset);
    /* Note that this will induce two wasted bytes */
    count = 0;
    prev_char = 0;

    /* Dump the cave */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            /* Extract a byte */
            tmp8u = cave_feat[y][x];

            /* If the run is broken, or too full, flush it */
            if ((tmp8u != prev_char) || (count == MAX_UCHAR))
            {
                wr_byte((byte)count);
                wr_byte((byte)prev_char);
                prev_char = tmp8u;
                count = 1;
            }

            /* Continue the run */
            else
            {
                count++;
            }
        }
    }

    /* Flush the data (if any) */
    if (count)
    {
        wr_byte((byte)count);
        wr_byte((byte)prev_char);
    }
    log_trace("[save:%06u] === END CAVE_FEAT RLE ===", (unsigned)save_byte_offset);

    /*** Simple "Run-Length-Encoding" of cave_color (style encoding) ***/

    log_trace("[save:%06u] === BEGIN CAVE_COLOR RLE ===", (unsigned)save_byte_offset);
    /* Note that this will induce two wasted bytes */
    count = 0;
    prev_char = 0;

    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            /* Extract the encoded color/style byte */
            tmp8u = cave_color[y][x];

            /* If the run is broken, or too full, flush it */
            if ((tmp8u != prev_char) || (count == MAX_UCHAR))
            {
                wr_byte((byte)count);
                wr_byte((byte)prev_char);
                prev_char = tmp8u;
                count = 1;
            }
            else
            {
                count++;
            }
        }
    }

    /* Flush the data (if any) */
    if (count)
    {
        wr_byte((byte)count);
        wr_byte((byte)prev_char);
    }
    log_trace("[save:%06u] === END CAVE_COLOR RLE ===", (unsigned)save_byte_offset);

    /* Persist door style variant choices so door visuals survive save/load.
     * Note: This must come AFTER cave_color so load.c can read it there. */
    log_trace("[save:%06u] === BEGIN DOOR_CHOICES ===", (unsigned)save_byte_offset);
    {
        /* Magic identifier for the door-choices block (0xD00D) */
        const u16b DOOR_CHOICES_MAGIC = 0xD00D;
        byte buf[64];
        int cap = (z_info && z_info->style_max > 0) ? z_info->style_max : 0;
        if (cap > 64) cap = 64;
        styles_copy_level_door_choices(buf, cap);
        log_debug("Writing door-choices block: magic=0x%04X, len=%d", DOOR_CHOICES_MAGIC, cap);
        wr_u16b(DOOR_CHOICES_MAGIC);
        wr_byte((byte)cap);
        for (int i = 0; i < cap; ++i) wr_byte(buf[i]);
    }
    log_trace("[save:%06u] === END DOOR_CHOICES ===", (unsigned)save_byte_offset);

    /*** Run-Length-Encoding of cave_rewired (rewired-trap difficulty) ***/
    /* New in 0.9.7.2; read on load only when savefile_has_cave_rewired. */
    log_trace("[save:%06u] === BEGIN CAVE_REWIRED RLE ===", (unsigned)save_byte_offset);
    {
        const u16b CAVE_REWIRED_MAGIC = 0xC2F0;

        wr_u16b(CAVE_REWIRED_MAGIC);

        count = 0;
        prev_char = 0;

        for (y = 0; y < p_ptr->cur_map_hgt; y++)
        {
            for (x = 0; x < p_ptr->cur_map_wid; x++)
            {
                /* Extract the rewired-trap difficulty byte */
                tmp8u = cave_rewired[y][x];

                /* If the run is broken, or too full, flush it */
                if ((tmp8u != prev_char) || (count == MAX_UCHAR))
                {
                    wr_byte((byte)count);
                    wr_byte((byte)prev_char);
                    prev_char = tmp8u;
                    count = 1;
                }
                else
                {
                    count++;
                }
            }
        }

        /* Flush the data (if any) */
        if (count)
        {
            wr_byte((byte)count);
            wr_byte((byte)prev_char);
        }
    }
    log_trace("[save:%06u] === END CAVE_REWIRED RLE ===", (unsigned)save_byte_offset);

    /*** Run-Length-Encoding of the natural CA-cave footprint ***/
    /* New in 0.9.7.4; read on load only when savefile_has_cave_natural. */
    log_trace("[save:%06u] === BEGIN CAVE_NATURAL RLE ===", (unsigned)save_byte_offset);
    {
        const u16b CAVE_NATURAL_MAGIC = 0xC3F0;

        wr_u16b(CAVE_NATURAL_MAGIC);

        count = 0;
        prev_char = 0;

        for (y = 0; y < p_ptr->cur_map_hgt; y++)
        {
            for (x = 0; x < p_ptr->cur_map_wid; x++)
            {
                tmp8u = cave_natural[y][x] ? 1 : 0;

                if ((tmp8u != prev_char) || (count == MAX_UCHAR))
                {
                    wr_byte((byte)count);
                    wr_byte((byte)prev_char);
                    prev_char = tmp8u;
                    count = 1;
                }
                else
                {
                    count++;
                }
            }
        }

        if (count)
        {
            wr_byte((byte)count);
            wr_byte((byte)prev_char);
        }
    }
    log_trace("[save:%06u] === END CAVE_NATURAL RLE ===", (unsigned)save_byte_offset);

    /*** Compact ***/

    log_trace("[save:%06u] Compacting objects and monsters", (unsigned)save_byte_offset);
    /* Compact the objects */
    compact_objects(0);

    /* Compact the monsters */
    compact_monsters(0);

    /*** Dump objects ***/

    log_trace("[save:%06u] === BEGIN OBJECTS ===", (unsigned)save_byte_offset);
    /* Total objects */
    if (o_max == 0) {
        log_warn("o_max was 0; clamping to 1 to avoid invalid object count");
        wr_u16b(1);
    } else {
        wr_u16b(o_max);
    }
    log_debug("Writing %d objects to savefile (o_max=%u)", o_max ? (o_max - 1) : 0, (unsigned)o_max);

    /* Dump the objects */
    for (i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];

        /* Dump it */
        wr_item(o_ptr);
    }
    log_trace("[save:%06u] === END OBJECTS ===", (unsigned)save_byte_offset);

    /*** Dump the monsters ***/

    log_trace("[save:%06u] === BEGIN MONSTERS ===", (unsigned)save_byte_offset);
    /* Total monsters */
    wr_u16b(mon_max);
    log_debug("Writing %d monsters to savefile", mon_max - 1);
    log_live_special_vault_only_monsters("save wr_dungeon");

    /* Dump the monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Dump it */
        wr_monster(m_ptr);
    }
    log_trace("[save:%06u] === END MONSTERS ===", (unsigned)save_byte_offset);

    // dump the wandering monster information
    log_trace("[save:%06u] === BEGIN WANDERING MONSTERS ===", (unsigned)save_byte_offset);
    for (i = FLOW_WANDERING_HEAD; i <= FLOW_WANDERING_TAIL; i++)
    {
        wr_byte(flow_center_y[i]);
        wr_byte(flow_center_x[i]);
        wr_s16b(wandering_pause[i]);
    }
    log_trace("[save:%06u] === END WANDERING MONSTERS ===", (unsigned)save_byte_offset);

    log_debug("Dungeon data write completed - %d objects, %d monsters", o_max - 1, mon_max - 1);
    log_trace("[save:%06u] === END DUNGEON ===", (unsigned)save_byte_offset);
}

