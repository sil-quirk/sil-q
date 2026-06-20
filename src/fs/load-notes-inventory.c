/* File: fs/load-notes-inventory.c -- carved from load.c (shares state via fs/load-internal.h) */

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
 * Read the notes. Every new savefile has at least NOTES_MARK.
 */
bool rd_notes(void)
{
    int alive = (!p_ptr->is_dead || arg_wizard);
    char tmpstr[100];
    int i;

    // reset the notes buffer
    for (i = 0; i < NOTES_LENGTH; i++)
    {
        notes_buffer[i] = '\0';
    }

    if (alive)
    {
        /* Append the notes in the savefile to the buffer */
        while (true)
        {
            rd_string(tmpstr, sizeof(tmpstr));
            /* Found the end? */
            if (strstr(tmpstr, NOTES_MARK))
                break;
            SDL_strlcat(
                notes_buffer, format("%s\n", tmpstr), sizeof(notes_buffer));
        }
    }
    /* Ignore the notes */
    else
    {
        while (true)
        {
            rd_string(tmpstr, sizeof(tmpstr));

            /* Found the end? */
            if (strstr(tmpstr, NOTES_MARK))
            {
                break;
            }
        }
    }

    return 0;
}


/*
 * Read the player inventory (and the smithing object)
 *
 * Note that the inventory is "re-sorted" later by "dungeon()".
 */
errr rd_inventory(void)
{
    int slot = 0;

    object_type* i_ptr;
    object_type object_type_body;

    log_debug("Loading smithing object and player inventory");
    log_trace("[load:%06u] === BEGIN SMITHING ITEM ===", (unsigned)load_byte_offset);

    /*
     * Start from a clean inventory.  The metarun autoload reads several
     * savefiles through this same path into the shared inventory[] globals,
     * and this function only ever writes the slots a savefile actually
     * stored.  Without clearing first, a slot that the current character
     * left empty (e.g. a quiver emptied by throwing its last weapon) keeps
     * whatever a previously loaded character had there, duplicating the
     * thrown weapon (one phantom in the quiver, one real copy on the floor).
     */
    for (int wipe_slot = 0; wipe_slot < INVEN_TOTAL; wipe_slot++)
        object_wipe(&inventory[wipe_slot]);
    p_ptr->inven_cnt = 0;
    p_ptr->equip_cnt = 0;

    /* Wipe the smithing object */
    object_wipe(smith_o_ptr);

    /* Read the smithing object */
    if (rd_item(smith_o_ptr))
    {
        note("Error reading smithing item");
        return (-1);
    }
    log_trace("[load:%06u] === END SMITHING ITEM ===", (unsigned)load_byte_offset);

    log_trace("[load:%06u] === BEGIN INVENTORY ===", (unsigned)load_byte_offset);
    /* Read until done */
    while (1)
    {
        u16b n;

        /* Get the next item index */
        rd_u16b(&n);

        /* Nope, we reached the end */
        if (n == 0xFFFF)
        {
            log_trace("[load:%06u] Found inventory sentinel 0xFFFF", (unsigned)(load_byte_offset - 2));
            break;
        }

        log_trace("[load:%06u] Loading inventory slot %u", (unsigned)(load_byte_offset - 2), (unsigned)n);

        /* Get local object */
        i_ptr = &object_type_body;

        /* Wipe the object */
        object_wipe(i_ptr);

        /* Read the item */
        if (rd_item(i_ptr))
        {
            log_warn("Error reading inventory item");
            note("Error reading item");
            return (-1);
        }

        /* Hack -- verify item */
        if (!i_ptr->k_idx)
            return (-1);

        if (i_ptr->number <= 0)
        {
            log_warn("Skipping stale empty inventory slot %u while loading",
                (unsigned)n);
            continue;
        }

        /* Verify slot */
        if (n >= INVEN_TOTAL)
            return (-1);

        /* Wield equipment */
        if (n >= INVEN_WIELD)
        {
            /* Copy object */
            object_copy(&inventory[n], i_ptr);

            /* Log equipped staff loading */
            if (i_ptr->tval == TV_STAFF)
            {
                log_debug("Loaded equipped staff at slot %d: k_idx=%d sval=%d pval=%d number=%d",
                          n, i_ptr->k_idx, i_ptr->sval, i_ptr->pval, i_ptr->number);
            }

            /* One more item */
            p_ptr->equip_cnt++;
        }

        /* Warning -- backpack is full */
        else if (p_ptr->inven_cnt == INVEN_PACK)
        {
            /* Oops */
            note("Too many items in the inventory!");

            /* Fail */
            return (-1);
        }

        /* Carry inventory */
        else
        {
            /* Get a slot */
            n = slot++;

            /* Copy object */
            object_copy(&inventory[n], i_ptr);

            /* Log pack staff loading */
            if (i_ptr->tval == TV_STAFF)
            {
                log_debug("Loaded pack staff at slot %d: k_idx=%d sval=%d pval=%d number=%d",
                          n, i_ptr->k_idx, i_ptr->sval, i_ptr->pval, i_ptr->number);
            }

            /* One more item */
            p_ptr->inven_cnt++;
        }
    }
    log_trace("[load:%06u] === END INVENTORY ===", (unsigned)load_byte_offset);

    log_debug("Inventory loaded: %d items carried, %d items equipped", p_ptr->inven_cnt, p_ptr->equip_cnt);

    log_trace("[load:%06u] === BEGIN SUPPLIES ===", (unsigned)load_byte_offset);
    supplies_reset_store();
    bool has_supply_block = false;

    if (savefile_version_at_least(0, 9, 6, 0))
    {
        u16b supply_magic = 0;
        rd_u16b(&supply_magic);
        if (supply_magic != SAVEFILE_SUPPLY_BLOCK_MAGIC)
        {
            log_warn("Invalid supplies block marker 0x%04X",
                (unsigned)supply_magic);
            note("Error reading supplies");
            return (-1);
        }
        has_supply_block = true;
    }
    else
    {
        has_supply_block = legacy_savefile_has_supply_block();
    }

    if (has_supply_block)
    {
        u16b supply_count = 0;
        rd_u16b(&supply_count);
        log_debug("Loading %u supply entries", (unsigned)supply_count);
        supplies_set_allow_overflow(true);
        for (u16b si = 0; si < supply_count; si++)
        {
            object_type supply;
            object_wipe(&supply);
            if (rd_item(&supply))
            {
                log_warn("Error reading supply entry");
                note("Error reading supplies");
                supplies_set_allow_overflow(false);
                return (-1);
            }

            s32b stored_units = 0;
            rd_s32b(&stored_units);

            if (supply.tval == TV_GEM)
            {
                int count = (int)stored_units;
                if (count <= 0)
                    count = supply.number;
                if (count < 0)
                    count = 0;

                supply.pval = 0;
                supply.ident &= ~(IDENT_EMPTY);

                int stack_limit = object_stack_limit(&supply);
                while (count > 0)
                {
                    int chunk = MIN(count, stack_limit);
                    if (chunk <= 0)
                        break;
                    object_type part;
                    object_copy(&part, &supply);
                    part.number = (byte)chunk;
                    count -= chunk;
                    supplies_absorb_object(&part);
                }
                continue;
            }

            if (supply.k_idx)
                supplies_absorb_object(&supply);
        }
        supplies_set_allow_overflow(false);
    }
    else
    {
        log_debug("No legacy supplies block present; migration will rebuild supplies if needed");
    }

    supplies_set_allow_overflow(false);

    log_trace("[load:%06u] === END SUPPLIES ===", (unsigned)load_byte_offset);

    if (savefile_version_at_least(0, 9, 6, 0))
        supplies_ingest_pack();

    jewelry_presets_reset();
    if (savefile_version_at_least(0, 9, 6, 7))
    {
        u16b preset_magic = 0;
        byte preset_count = 0;

        log_trace("[load:%06u] === BEGIN JEWELRY PRESETS ===",
            (unsigned)load_byte_offset);
        rd_u16b(&preset_magic);
        if (preset_magic != SAVEFILE_JEWELRY_PRESET_BLOCK_MAGIC)
        {
            log_warn("Invalid jewelry preset block marker 0x%04X",
                (unsigned)preset_magic);
            note("Error reading jewelry presets");
            return (-1);
        }

        rd_byte(&preset_count);
        for (byte preset = 0; preset < preset_count; preset++)
        {
            byte is_set = 0;
            rd_byte(&is_set);

            for (byte slot_idx = 0; slot_idx < JEWELRY_PRESET_SLOT_MAX;
                 slot_idx++)
            {
                object_type preset_obj;
                object_wipe(&preset_obj);

                if (rd_item(&preset_obj))
                {
                    log_warn("Error reading jewelry preset entry");
                    note("Error reading jewelry presets");
                    return (-1);
                }

                if (is_set && preset < JEWELRY_PRESET_MAX
                    && preset_obj.k_idx)
                {
                    jewelry_preset_set_object(preset, slot_idx, &preset_obj);
                }
            }
        }
        log_trace("[load:%06u] === END JEWELRY PRESETS ===",
            (unsigned)load_byte_offset);
    }

    /* Success */
    return (0);
}
