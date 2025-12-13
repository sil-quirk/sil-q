/* File: save.c */

/*
 * Copyright (c) 1997 Ben Harrison, and others
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include <stdio.h>

void updatecharinfoS(void)
{
    char tmp_Path[1024];
    char parsed_dir_user[1024];

    int curDepth = p_ptr->max_depth * 50;

    log_debug("Creating character output file");

    if (!path_parse(parsed_dir_user, sizeof(parsed_dir_user), ANGBAND_DIR_USER))
    {
        log_warn("updatecharinfoS: unable to resolve user directory");
        return;
    }

    if (!path_build(tmp_Path, sizeof(tmp_Path), parsed_dir_user, "CharOutput.txt"))
    {
        log_warn("updatecharinfoS: unable to build character output path");
        return;
    }

    FILE* oFile = fopen(tmp_Path, "w");
    if (!oFile) {
        log_warn("Failed to open character output file: %s", tmp_Path);
        return;
    }

    fprintf(oFile, "{\n");

    const char* race_str = p_name + p_info[p_ptr->prace].name;
    fprintf(oFile, "race: \"%s\",\n", race_str);

    const char* class_str = c_name + c_info[p_ptr->pcharacter].name;
    fprintf(oFile, "class: \"%s\",\n", class_str);

    fprintf(oFile, "mDepth: \"%i\",\n", curDepth);
    fprintf(oFile, "isDead: \"%i\",\n", p_ptr->is_dead);
    fprintf(oFile, "killedBy: \"%s\",\n", p_ptr->died_from);
    fprintf(oFile, "onTheRun: \"%i\",\n", p_ptr->on_the_run);
    fprintf(oFile, "morgothDead: \"%i\"\n", p_ptr->morgoth_slain);

    fprintf(oFile, "}");
    fclose(oFile);
    log_debug("Character output file written successfully: %s", tmp_Path);
}



#ifdef FUTURE_SAVEFILES

/*
 * XXX XXX XXX Ignore this for now...
 *
 * The basic format of Angband 2.8.0 (and later) savefiles is simple.
 *
 * The savefile itself is a "header" (4 bytes) plus a series of "blocks",
 * plus, perhaps, some form of "footer" at the end.
 *
 * The "header" contains information about the "version" of the savefile.
 * Conveniently, pre-2.8.0 savefiles also use a 4 byte header, though the
 * interpretation of the "sf_extra" byte is very different.  Unfortunately,
 * savefiles from Angband 2.5.X reverse the sf_major and sf_minor fields,
 * and must be handled specially, until we decide to start ignoring them.
 *
 * Each "block" is a "type" (2 bytes), plus a "size" (2 bytes), plus "data",
 * plus a "check" (2 bytes), plus a "stamp" (2 bytes).  The format of the
 * "check" and "stamp" bytes is still being contemplated, but it would be
 * nice for one to be a simple byte-checksum, and the other to be a complex
 * additive checksum of some kind.  Both should be zero if the block is empty.
 *
 * Standard types:
 *   TYPE_BIRTH --> creation info
 *   TYPE_OPTIONS --> option settings
 *   TYPE_MESSAGES --> message recall
 *   TYPE_PLAYER --> player information
 *   TYPE_INVEN --> player inven/equip
 *   TYPE_RACES --> monster race data
 *   TYPE_KINDS --> object kind data
 *   TYPE_UNIQUES --> unique info
 *   TYPE_ARTEFACTS --> artefact info
 *
 * Dungeon information:
 *   TYPE_DUNGEON --> dungeon info
 *   TYPE_FEATURES --> dungeon features
 *   TYPE_OBJECTS --> dungeon objects
 *   TYPE_MONSTERS --> dungeon monsters
 *
 * Conversions:
 *   Break old "races" into normals/uniques
 *   Extract info about the "unique" monsters
 *
 * Question:
 *   Should the normals/uniques be broken for 2.8.0, or should 2.8.0 simply
 *   be a "fixed point" into which older savefiles are converted, and then
 *   future versions could ignore older savefiles, and the "conversions"
 *   would be much simpler.
 */

/*
 * XXX XXX XXX
 */
#define TYPE_OPTIONS 17362

/*
 * Hack -- current savefile
 */
static int data_fd = -1;

/*
 * Hack -- current block type
 */
static u16b data_type;

/*
 * Hack -- current block size
 */
static u16b data_size;

/*
 * Hack -- pointer to the data buffer
 */
static byte* data_head;

/*
 * Hack -- pointer into the data buffer
 */
static byte* data_next;

/*
 * Hack -- write the current "block" to the savefile
 */
static errr wr_block(void)
{
    errr err;

    byte fake[4];

    /* Save the type and size */
    fake[0] = (byte)(data_type);
    fake[1] = (byte)(data_type >> 8);
    fake[2] = (byte)(data_size);
    fake[3] = (byte)(data_size >> 8);

    /* Dump the head */
    err = sdl_write(data_fd, (cptr)&fake, sizeof(fake));

    /* Dump the actual data */
    err = sdl_write(data_fd, (cptr)data_head, data_size);

    /* XXX XXX XXX */
    fake[0] = 0;
    fake[1] = 0;
    fake[2] = 0;
    fake[3] = 0;

    /* Dump the tail */
    err = sdl_write(data_fd, (cptr)&fake, sizeof(fake));

    /* Hack -- reset */
    data_next = data_head;

    /* Wipe the data block */
    memset(data_head, 0, sizeof(byte) * 65535);

    /* Success */
    return (0);
}

/*
 * Hack -- add data to the current block
 */
static void put_byte(byte v) { *data_next++ = v; }

/*
 * Hack -- add data to the current block
 */
static void put_char(char v) { put_byte((byte)(v)); }

/*
 * Hack -- add data to the current block
 */
static void put_u16b(u16b v)
{
    *data_next++ = (byte)(v);
    *data_next++ = (byte)(v >> 8);
}

/*
 * Hack -- add data to the current block
 */
static void put_s16b(s16b v) { put_u16b((u16b)(v)); }

/*
 * Hack -- add data to the current block
 */
static void put_u32b(u32b v)
{
    *data_next++ = (byte)(v);
    *data_next++ = (byte)(v >> 8);
    *data_next++ = (byte)(v >> 16);
    *data_next++ = (byte)(v >> 24);
}

/*
 * Hack -- add data to the current block
 */
static void put_s32b(s32b v) { put_u32b((u32b)(v)); }

/*
 * Hack -- add data to the current block
 */
static void put_string(char* str)
{
    while ((*data_next++ = *str++) != '\0')
        ;
}

/*
 * Hack -- read the next "block" from the savefile
 */
static errr rd_block(void)
{
    errr err;

    byte fake[4];

    /* Read the head data */
    err = sdl_read(data_fd, (char*)&fake, sizeof(fake));

    /* Extract the type and size */
    data_type = (fake[0] | ((u16b)fake[1] << 8));
    data_size = (fake[2] | ((u16b)fake[3] << 8));

    /* Wipe the data block */
    memset(data_head, 0, sizeof(byte) * 65535);

    /* Read the actual data */
    err = sdl_read(data_fd, (char*)data_head, data_size);

    /* Read the tail data */
    err = sdl_read(data_fd, (char*)&fake, sizeof(fake));

    /* XXX XXX XXX Verify */

    /* Hack -- reset */
    data_next = data_head;

    /* Success */
    return (0);
}

/*
 * Hack -- get data from the current block
 */
static void get_byte(byte* ip)
{
    byte d1;
    d1 = (*data_next++);
    (*ip) = (d1);
}

/*
 * Hack -- get data from the current block
 */
static void get_char(char* ip) { get_byte((byte*)ip); }

/*
 * Hack -- get data from the current block
 */
static void get_u16b(u16b* ip)
{
    u16b d0, d1;
    d0 = (*data_next++);
    d1 = (*data_next++);
    (*ip) = (d0 | (d1 << 8));
}

/*
 * Hack -- get data from the current block
 */
static void get_s16b(s16b* ip) { get_u16b((u16b*)ip); }

/*
 * Hack -- get data from the current block
 */
static void get_u32b(u32b* ip)
{
    u32b d0, d1, d2, d3;
    d0 = (*data_next++);
    d1 = (*data_next++);
    d2 = (*data_next++);
    d3 = (*data_next++);
    (*ip) = (d0 | (d1 << 8) | (d2 << 16) | (d3 << 24));
}

/*
 * Hack -- get data from the current block
 */
static void get_s32b(s32b* ip) { get_u32b((u32b*)ip); }

/*
 * Read a savefile for Angband 2.8.0
 */
static errr rd_savefile(void)
{
    bool done = false;

    byte fake[4];

    /* Open the savefile */
    data_fd = sdl_fopen(savefile, "rb");

    /* No file */
    if (data_fd < 0)
        return (1);

    /* Strip the first four bytes (see below) */
    if (sdl_read(data_fd, (char*)(fake), sizeof(fake)))
        return (1);

    /* Make array XXX XXX XXX */
    data_head = mem_alloc_array(65535, byte);

    /* Hack -- reset */
    data_next = data_head;

    /* Read blocks */
    while (!done)
    {
        /* Read the block */
        if (rd_block())
            break;

        /* Analyze the type */
        switch (data_type)
        {
        /* Done XXX XXX XXX */
        case 0:
        {
            done = true;
            break;
        }

        /* Grab the options */
        case TYPE_OPTIONS:
        {
            if (get_options())
                err = -1;
            break;
        }
        }

        /* XXX XXX XXX verify "data_next" */
        if (data_next - data_head > data_size)
            break;
    }

    /* XXX XXX XXX Check for errors */

    /* Kill array XXX XXX XXX */
    mem_free_null(data_head);

    /* Success */
    return (0);
}

#endif /* FUTURE_SAVEFILES */

/*
 * Some "local" parameters, used to help write savefiles
 */

static SDL_IOStream* fff; /* Current save "file" */

static byte xor_byte; /* Simple encryption */

static u32b v_stamp = 0L; /* A simple "checksum" on the actual values */
static u32b x_stamp = 0L; /* A simple "checksum" on the encoded bytes */

/* Track if a write error has occurred */
static bool write_error = false;

/* Track save byte offset for detailed logging */
static u32b save_byte_offset = 0;

/*
 * These functions place information into a savefile a byte at a time
 */

static void sf_put(byte v)
{
    size_t result;
    
    /* Encode the value */
    xor_byte ^= v;
    
    /* Write the byte directly */
    result = SDL_WriteIO(fff, &xor_byte, 1);
    
    /* Check for write error */
    if (result != 1)
    {
        if (!write_error)
        {
            log_error("sf_put: Write error detected (SDL_WriteIO failed) - SDL Error: %s", SDL_GetError());
            log_error("sf_put: fff pointer = %p, attempted to write byte 0x%02x at offset %u", 
                     (void*)fff, xor_byte, save_byte_offset);
            write_error = true;
        }
        return;
    }

    /* Track byte offset */
    save_byte_offset++;

    /* Maintain the checksum info */
    v_stamp += v;
    x_stamp += xor_byte;
}

static void wr_byte(byte v) { 
    sf_put(v);
}

static void wr_u16b(u16b v)
{
    log_trace("[save:%06u] wr_u16b: 0x%04X (%u)", (unsigned)save_byte_offset, (unsigned)v, (unsigned)v);
    sf_put((byte)(v & 0xFF));
    sf_put((byte)((v >> 8) & 0xFF));
    save_byte_offset += 2;
}

static void wr_s16b(s16b v) { 
    log_trace("[save:%06u] wr_s16b: %d", (unsigned)save_byte_offset, (int)v);
    wr_u16b((u16b)v); 
}

static void wr_u32b(u32b v)
{
    log_trace("[save:%06u] wr_u32b: 0x%08X (%u)", (unsigned)save_byte_offset, (unsigned)v, (unsigned)v);
    sf_put((byte)(v & 0xFF));
    sf_put((byte)((v >> 8) & 0xFF));
    sf_put((byte)((v >> 16) & 0xFF));
    sf_put((byte)((v >> 24) & 0xFF));
    save_byte_offset += 4;
}

static void wr_s32b(s32b v) { 
    log_trace("[save:%06u] wr_s32b: %d", (unsigned)save_byte_offset, (int)v);
    wr_u32b((u32b)v); 
}

static void wr_string(cptr str)
{
    while (*str)
    {
        wr_byte(*str);
        str++;
    }
    wr_byte(*str);
}

/*
 * These functions write info in larger logical records
 */

/*
 * Write an "item" record
 */
static void wr_item(const object_type* o_ptr)
{
    int i;

    wr_s16b(o_ptr->k_idx);

    wr_s16b(o_ptr->image_k_idx);

    /* Location */
    wr_byte(o_ptr->iy);
    wr_byte(o_ptr->ix);

    wr_byte(o_ptr->tval);
    wr_byte(o_ptr->sval);
    wr_s16b(o_ptr->pval);

    wr_byte(o_ptr->discount);

    wr_byte(o_ptr->number);
    wr_s16b(o_ptr->weight);

    wr_byte(o_ptr->name1);
    wr_byte(o_ptr->name2);

    wr_s16b(o_ptr->timeout);

    wr_s16b(o_ptr->att);
    wr_byte(o_ptr->dd);
    wr_byte(o_ptr->ds);
    wr_s16b(o_ptr->evn);
    wr_byte(o_ptr->pd);
    wr_byte(o_ptr->ps);
    wr_byte(o_ptr->pickup);
    wr_s16b(o_ptr->pickup_slot);

    wr_u32b(o_ptr->ident);

    wr_byte(o_ptr->marked);

    /* Held by monster index */
    wr_s16b(o_ptr->held_m_idx);

    /* Extra information */
    wr_byte(o_ptr->xtra1);

    // granted abilities
    wr_byte(o_ptr->abilities);
    for (i = 0; i < 8; i++)
    {
        wr_byte(o_ptr->skilltype[i]);
        wr_byte(o_ptr->abilitynum[i]);
    }

    wr_s32b(o_ptr->unused1);
    wr_s32b(o_ptr->unused2);
    wr_s32b(o_ptr->unused3);
    wr_s32b(o_ptr->unused4);

    // 8 spare bytes
    wr_u32b(0L);
    wr_u32b(0L);

    /* Save the inscription (if any) */
    if (o_ptr->obj_note)
    {
        wr_string(quark_str(o_ptr->obj_note));
    }
    else
    {
        wr_string("");
    }
}

/*
 * Special monster flags that get saved in the savefile
 */
#define SAVE_MON_FLAGS                                                         \
    (MFLAG_ACTV | MFLAG_ALWAYS_CAST | MFLAG_AGGRESSIVE | MFLAG_SUMMONED        \
        | MFLAG_HIT_BY_RANGED | MFLAG_HIT_BY_MELEE | MFLAG_CHARGED)

/*
 * Write a "monster" record
 */
static void wr_monster(const monster_type* m_ptr)
{
    u32b tmp32u;
    int i;

    wr_s16b(m_ptr->r_idx);
    wr_s16b(m_ptr->image_r_idx);
    wr_byte(m_ptr->fy);
    wr_byte(m_ptr->fx);
    wr_s16b(m_ptr->hp);
    wr_s16b(m_ptr->maxhp);
    wr_s16b(m_ptr->alertness);
    wr_byte(m_ptr->skip_next_turn);
    wr_byte(m_ptr->mspeed);
    wr_byte(m_ptr->energy);
    wr_byte(m_ptr->stunned);
    wr_byte(m_ptr->confused);
    wr_s16b(m_ptr->hasted);
    wr_s16b(m_ptr->slowed);
    wr_byte(m_ptr->stance);
    wr_s16b(m_ptr->morale);
    wr_s16b(m_ptr->tmp_morale);
    wr_byte(m_ptr->noise);
    wr_byte(m_ptr->encountered);
    wr_byte(m_ptr->target_y);
    wr_byte(m_ptr->target_x);
    wr_s16b(m_ptr->wandering_idx);
    wr_byte(m_ptr->wandering_dist);
    wr_byte(m_ptr->mana);
    wr_byte(m_ptr->song);
    wr_byte(m_ptr->skip_this_turn);

    wr_byte(m_ptr->song_contest_stacks);
    wr_byte(m_ptr->song_lament_stacks);
    wr_byte(m_ptr->song_lockout_timer);
    wr_byte(m_ptr->song_hp_loss_lo);
    wr_s32b(m_ptr->song_contest_last_turn);
    wr_s32b(m_ptr->song_lament_last_turn);
    wr_s16b(m_ptr->song_will_penalty);
    wr_s16b(m_ptr->song_stealth_penalty);
    wr_s16b(m_ptr->song_evasion_penalty);
    wr_byte(m_ptr->song_armor_dice_penalty);
    wr_byte(m_ptr->song_hp_loss_hi);
    wr_byte(m_ptr->song_contest_completed);
    wr_byte(m_ptr->song_lament_completed);

    wr_s16b(m_ptr->consecutive_attacks);
    wr_s16b(m_ptr->turns_stationary);

    /*save the temporary flags*/
    tmp32u = m_ptr->mflag & (SAVE_MON_FLAGS);
    wr_u32b(tmp32u);

    for (i = 0; i < ACTION_MAX; i++)
    {
        wr_byte(m_ptr->previous_action[i]);
    }

    for (i = 0; i < MONSTER_BLOW_MAX; i++)
    {
        wr_byte(m_ptr->blow_dd_reduction[i]);
    }

    for (i = 0; i < MONSTER_BLOW_MAX; i++)
    {
        wr_byte(m_ptr->blow_ds_reduction[i]);
    }

    wr_byte(m_ptr->armor_ps_reduction);
    wr_byte(m_ptr->shatter_padding[0]);
    wr_byte(m_ptr->shatter_padding[1]);
    wr_byte(m_ptr->shatter_padding[2]);
}

/*
 * Write a "lore" record
 */
static void wr_lore(int r_idx)
{
    int i;

    monster_race* r_ptr = &r_info[r_idx];
    monster_lore* l_ptr = &l_list[r_idx];

    /* Count deaths/sights/kills */
    wr_s16b(l_ptr->deaths);
    wr_s16b(l_ptr->psights);
    wr_s16b(l_ptr->tsights);
    wr_s16b(l_ptr->pkills);
    wr_s16b(l_ptr->tkills);

    /* Count wakes and ignores */
    wr_byte(l_ptr->notice);
    wr_byte(l_ptr->ignore);

    /* Count drops */
    wr_byte(l_ptr->drop_item);

    /* Count spells */
    wr_byte(l_ptr->ranged);

    /* Count blows of each type */
    for (i = 0; i < MONSTER_BLOW_MAX; i++)
        wr_byte(l_ptr->blows[i]);

    /* Memorize flags */
    wr_u32b(l_ptr->flags1);
    wr_u32b(l_ptr->flags2);
    wr_u32b(l_ptr->flags3);
    wr_u32b(l_ptr->flags4);

    /* Monster limit per level */
    wr_byte(r_ptr->max_num);

    // 8 spare bytes
    wr_u32b(0L);
    wr_u32b(0L);
}

static bool monster_race_stats_changed(int r_idx)
{
    if (!r_base)
        return false;

    const monster_race* base = &r_base[r_idx];
    const monster_race* cur = &r_info[r_idx];

    if (base->hdice != cur->hdice) return true;
    if (base->hside != cur->hside) return true;
    if (base->evn != cur->evn) return true;
    if (base->pd != cur->pd) return true;
    if (base->ps != cur->ps) return true;
    if (base->speed != cur->speed) return true;
    if (base->light != cur->light) return true;
    if (base->sleep != cur->sleep) return true;
    if (base->per != cur->per) return true;
    if (base->stl != cur->stl) return true;
    if (base->wil != cur->wil) return true;
    if (base->extra != cur->extra) return true;
    if (base->freq_ranged != cur->freq_ranged) return true;
    if (base->spell_power != cur->spell_power) return true;
    if (base->mon_power != cur->mon_power) return true;
    if (base->flags1 != cur->flags1) return true;
    if (base->flags2 != cur->flags2) return true;
    if (base->flags3 != cur->flags3) return true;
    if (base->flags4 != cur->flags4) return true;

    for (int i = 0; i < MONSTER_BLOW_MAX; i++)
    {
        const monster_blow* b_base = &base->blow[i];
        const monster_blow* b_cur = &cur->blow[i];
        if (b_base->method != b_cur->method) return true;
        if (b_base->effect != b_cur->effect) return true;
        if (b_base->att != b_cur->att) return true;
        if (b_base->dd != b_cur->dd) return true;
        if (b_base->ds != b_cur->ds) return true;
    }

    if (base->level != cur->level) return true;
    if (base->rarity != cur->rarity) return true;
    if (base->d_attr != cur->d_attr) return true;
    if (base->d_char != cur->d_char) return true;
    if (base->x_attr != cur->x_attr) return true;
    if (base->x_char != cur->x_char) return true;

    return false;
}

static void wr_monster_race_stats(const monster_race* r_ptr)
{
    wr_byte(r_ptr->hdice);
    wr_byte(r_ptr->hside);
    wr_s16b(r_ptr->evn);
    wr_byte(r_ptr->pd);
    wr_byte(r_ptr->ps);
    wr_byte(r_ptr->speed);
    wr_s16b(r_ptr->light);
    wr_s16b(r_ptr->sleep);
    wr_s16b(r_ptr->per);
    wr_s16b(r_ptr->stl);
    wr_s16b(r_ptr->wil);
    wr_s16b(r_ptr->extra);
    wr_byte(r_ptr->freq_ranged);
    wr_byte(r_ptr->spell_power);
    wr_u32b(r_ptr->mon_power);
    wr_u32b(r_ptr->flags1);
    wr_u32b(r_ptr->flags2);
    wr_u32b(r_ptr->flags3);
    wr_u32b(r_ptr->flags4);

    for (int i = 0; i < MONSTER_BLOW_MAX; i++)
    {
        wr_byte(r_ptr->blow[i].method);
        wr_byte(r_ptr->blow[i].effect);
        wr_s16b(r_ptr->blow[i].att);
        wr_byte(r_ptr->blow[i].dd);
        wr_byte(r_ptr->blow[i].ds);
    }

    wr_byte(r_ptr->level);
    wr_byte(r_ptr->rarity);
    wr_byte(r_ptr->d_attr);
    wr_byte((byte)r_ptr->d_char);
    wr_byte(r_ptr->x_attr);
    wr_byte((byte)r_ptr->x_char);
}

static void wr_monster_runtime_overrides(void)
{
    if (!r_base)
    {
        wr_u16b(0);
        return;
    }

    u16b count = 0;

    for (int i = 0; i < z_info->r_max; i++)
    {
        if (monster_race_stats_changed(i))
            count++;
    }

    wr_u16b(count);

    if (!count)
        return;

    log_debug("Writing %u monster race runtime overrides", (unsigned)count);

    for (int i = 0; i < z_info->r_max; i++)
    {
        if (!monster_race_stats_changed(i))
            continue;

        wr_u16b((u16b)i);
        wr_monster_race_stats(&r_info[i]);
    }
}

/*
 * Write an "xtra" record
 */
static void wr_xtra(int k_idx)
{
    byte tmp8u = 0;

    object_kind* k_ptr = &k_info[k_idx];

    if (k_ptr->aware)
        tmp8u |= 0x01;
    if (k_ptr->tried)
        tmp8u |= 0x02;
    if (k_ptr->everseen)
        tmp8u |= 0x08;

    wr_byte(tmp8u);

    /*write the squelch settings*/
    tmp8u = k_ptr->squelch;

    wr_byte(tmp8u);
}

/*
 * Write RNG state
 */
static errr wr_randomizer(void)
{
    int i;
    u64b state = Rand_state_export();

    /* Preserve legacy padding */
    wr_u32b(0L);
    wr_u32b(0L);

    /* Legacy "place" slot (unused now) */
    wr_u16b(0);

    for (i = 0; i < 63; i++)
    {
        u32b word = 0;
        if (i == 0)
            word = (u32b)(state & 0xFFFFFFFFu);
        else if (i == 1)
            word = (u32b)(state >> 32);
        wr_u32b(word);
    }

    return (0);
}

/*
 * Write the "options"
 */
static void wr_options(void)
{
    int i, k;

    u32b flag[8];
    u32b mask[8];
    u32b window_flag[ANGBAND_TERM_MAX];
    u32b window_mask[ANGBAND_TERM_MAX];

    /*** Special Options ***/

    /* Write "delay_factor" */
    wr_byte(op_ptr->delay_factor);

    /* Write "hitpoint_warn" */
    wr_byte(op_ptr->hitpoint_warn);

    /* Write "main_combat_rolls" */
    wr_byte(op_ptr->main_combat_rolls);

    // 7 spare bytes
    wr_byte(0);
    wr_u32b(0L);
    wr_u16b(0);

    /*** Normal options ***/

    /* Reset */
    for (i = 0; i < 8; i++)
    {
        flag[i] = 0L;
        mask[i] = 0L;
    }

    /* Analyze the options */
    for (i = 0; i < OPT_MAX; i++)
    {
        int os = i / 32;
        int ob = i % 32;

        /* Process real entries */
        if (option_text[i])
        {
            /* Set flag */
            if (op_ptr->opt[i])
            {
                /* Set */
                flag[os] |= (1L << ob);
            }

            /* Set mask */
            mask[os] |= (1L << ob);
        }
    }

    /* Dump the flags */
    for (i = 0; i < 8; i++)
        wr_u32b(flag[i]);

    /* Dump the masks */
    for (i = 0; i < 8; i++)
        wr_u32b(mask[i]);

    /*** Window options ***/

    /* Reset */
    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        /* Flags */
        window_flag[i] = op_ptr->window_flag[i];

        /* Mask */
        window_mask[i] = 0L;

        /* Build the mask */
        for (k = 0; k < 32; k++)
        {
            /* Set mask */
            if (window_flag_desc[k])
            {
                window_mask[i] |= (1L << k);
            }
        }
    }

    /* Dump the flags */
    for (i = 0; i < ANGBAND_TERM_MAX; i++)
        wr_u32b(window_flag[i]);

    /* Dump the masks */
    for (i = 0; i < ANGBAND_TERM_MAX; i++)
        wr_u32b(window_mask[i]);
}

/*
 * Write some "extra" info
 */
static void wr_extra(void)
{
    int i, j;

    wr_string(op_ptr->full_name);

    wr_string(p_ptr->died_from);

    wr_string(p_ptr->history);

    /* Race/Character/Sex */
    wr_byte(p_ptr->prace);
    wr_byte (p_ptr->pcharacter);
    wr_byte(p_ptr->unused1);

    wr_s16b(p_ptr->game_type);



    /* Age/Height/Weight */
    wr_s16b(p_ptr->age);
    wr_s16b(p_ptr->ht);
    wr_s16b(p_ptr->wt);

    /* Dump the stats (maximum and current) */
    for (i = 0; i < A_MAX; ++i)
        wr_s16b(p_ptr->stat_base[i]);
    for (i = 0; i < A_MAX; ++i)
        wr_s16b(p_ptr->stat_drain[i]);

    /* Dump the skill bases */
    for (i = 0; i < S_MAX; ++i)
        wr_s16b(p_ptr->skill_base[i]);

    for (i = 0; i < S_MAX; ++i)
    {
        for (j = 0; j < ABILITIES_MAX; ++j)
        {
            wr_byte(p_ptr->innate_ability[i][j]);
            wr_byte(p_ptr->active_ability[i][j]);
            wr_byte(p_ptr->have_ability[i][j]);
            
            /* Debug special abilities save */
            if (i == S_SPC && p_ptr->have_ability[i][j] != 0) {
                log_trace("Save: Special ability %d has value %d", j, p_ptr->have_ability[i][j]);
            }
        }
    }

    ability_log_sync_missing();
    u16b ability_events = p_ptr->ability_timeline_count;
    if (ability_events > ABILITY_TIMELINE_MAX)
        ability_events = ABILITY_TIMELINE_MAX;
    wr_u16b(ability_events);
    for (u16b idx = 0; idx < ability_events; idx++) {
        wr_byte(p_ptr->ability_timeline_skill[idx]);
        wr_byte(p_ptr->ability_timeline_ability[idx]);
        wr_u32b(p_ptr->ability_timeline_turn[idx]);
        wr_s16b(p_ptr->ability_timeline_depth[idx]);
    }

    wr_s16b(p_ptr->last_attack_m_idx);
    wr_s16b(p_ptr->consecutive_attacks);
    wr_s16b(p_ptr->bane_type);

    for (i = 0; i < ACTION_MAX; ++i)
    {
        wr_byte(p_ptr->previous_action[i]);
    }
    wr_byte(p_ptr->focused);

    wr_s32b(p_ptr->new_exp);
    wr_s32b(p_ptr->exp);

    wr_s32b(p_ptr->encounter_exp);
    wr_s32b(p_ptr->kill_exp);
    wr_s32b(p_ptr->descent_exp);
    wr_s32b(p_ptr->ident_exp);

    wr_s16b(p_ptr->mhp);
    wr_s16b(p_ptr->chp);
    wr_u16b(p_ptr->chp_frac);

    wr_s16b(p_ptr->msp);
    wr_s16b(p_ptr->csp);
    wr_u16b(p_ptr->csp_frac);

    /* Max Dungeon Level */
    wr_s16b(p_ptr->max_depth);

    wr_u16b(p_ptr->staircasiness);

    /* More info */
    wr_s16b(p_ptr->morgoth_state);

    wr_byte(p_ptr->song1);
    wr_byte(p_ptr->song2);
    wr_s16b(p_ptr->song_duration);
    wr_s16b(p_ptr->song_target_idx);
    wr_byte(p_ptr->song_target_song);
    wr_byte(p_ptr->song_lockout_timer);
    wr_byte(p_ptr->song_contest_player_stacks);
    wr_byte(p_ptr->song_duel_pad);
    wr_s32b(p_ptr->song_contest_last_turn);
    wr_s16b(p_ptr->vengeance);
    wr_s16b(p_ptr->blind);
    wr_s16b(p_ptr->entranced);
    wr_s16b(p_ptr->confused);
    wr_s16b(p_ptr->food);
    wr_u16b(p_ptr->stairs_taken);
    wr_u16b(p_ptr->fixed_forge_count);
    wr_u16b(p_ptr->forge_count);
    wr_s16b(p_ptr->energy);
    wr_s16b(p_ptr->fast);
    wr_s16b(p_ptr->slow);
    wr_s16b(p_ptr->afraid);
    wr_s16b(p_ptr->cut);
    wr_s16b(p_ptr->stun);
    wr_s16b(p_ptr->poisoned);
    wr_s16b(p_ptr->image);
    wr_s16b(p_ptr->rage);
    wr_s16b(p_ptr->tmp_str);
    wr_s16b(p_ptr->tmp_dex);
    wr_s16b(p_ptr->tmp_con);
    wr_s16b(p_ptr->tmp_gra);
    wr_s16b(p_ptr->tim_invis);
    wr_s16b(p_ptr->tmp_per);
    wr_s16b(p_ptr->darkened);
    wr_s16b(p_ptr->oppose_fire);
    wr_s16b(p_ptr->oppose_cold);
    wr_s16b(p_ptr->oppose_pois);

    wr_s16b(p_ptr->song_challenge_effect);
    wr_s16b(p_ptr->song_elbereth_effect);

    wr_byte(p_ptr->stealth_mode);
    wr_byte(p_ptr->self_made_arts);

    wr_byte(p_ptr->climbing);

    // 15 spare bytes (was 19, used 4 for song debuff counters)
    wr_byte(0);
    wr_byte(0);
    wr_byte(0);
    wr_u32b(0L);
    wr_u32b(0L);
    wr_u32b(0L);

    /* Save item-quality squelch sub-menu */
    for (i = 0; i < SQUELCH_BYTES; i++)
        wr_byte(squelch_level[i]);

    /* Store the name of the current greater vault */
    wr_string(g_vault_name);

    /* Save the current number of special item types */
    wr_u16b(z_info->e_max);

    /* Save special item squelch settings */
    for (i = 0; i < z_info->e_max; i++)
    {
        ego_item_type* e_ptr = &e_info[i];
        byte tmp8u = 0;

        if (e_ptr->squelch)
            tmp8u |= 0x01;
        if (e_ptr->everseen)
            tmp8u |= 0x02;
        if (e_ptr->aware)
            tmp8u |= 0x04;

        wr_byte(tmp8u);
    }

    /*Write the current number of auto-inscriptions*/
    wr_u16b(inscriptionsCount);

    /*Write the autoinscriptions array*/
    for (i = 0; i < inscriptionsCount; i++)
    {
        wr_s16b(inscriptions[i].kindIdx);
        wr_string(quark_str(inscriptions[i].inscriptionIdx));
    }

    // Greater vaults seen
    for (i = 0; i < MAX_GREATER_VAULTS; i++)
    {
        wr_s16b(p_ptr->greater_vaults[i]);
    }

    /* Random artefact version */
    wr_u32b(RANDART_VERSION);

    /* Random artefact seed */
    wr_u32b(seed_randart);

    /* Write the "object seeds" */
    wr_u32b(seed_flavor);

    /* Special stuff */
    wr_u16b(p_ptr->panic_save);
    wr_byte(p_ptr->truce);
    wr_byte(p_ptr->morgoth_hits);
    wr_byte(p_ptr->crown_hint);
    wr_byte(p_ptr->crown_shatter);
    wr_byte(p_ptr->cursed);
    wr_byte(p_ptr->on_the_run);
    wr_byte(p_ptr->morgoth_slain);
    wr_u16b(p_ptr->escaped);
    wr_u16b(p_ptr->noscore);
    wr_u16b(p_ptr->smithing_leftover);
    wr_byte(p_ptr->unique_forge_made ? 1 : 0);
    wr_byte(p_ptr->unique_forge_seen ? 1 : 0);

    /* Write death */
    wr_byte(p_ptr->is_dead ? 1 : 0);
    log_trace("Player is dead: %d", p_ptr->is_dead);

    /* Write feeling */
    wr_byte(feeling);

    /* Turn of last "feeling" */
    wr_byte(do_feeling);

    /* Current turn */
    wr_s32b(turn);
    log_trace("Current turn: %d", turn);

    /* Current player turn */
    wr_s32b(playerturn);
    log_trace("Player turn: %d", playerturn);

    /* Crown shatter flags (Sil 2 / Sil 3) */
    wr_byte(p_ptr->crown_shatter_sil2);
    wr_byte(p_ptr->crown_shatter_sil3);

    wr_byte(p_ptr->killed_enemy_with_arrow ? 1 : 0);

    wr_byte(p_ptr->oath_type);
    wr_byte(p_ptr->oaths_broken);
    log_info("SAVE: About to write quest marker 0x51, oath_type=%d, oaths_broken=%d", 
             p_ptr->oath_type, p_ptr->oaths_broken);

    /* From 0.8.6 onward, write quest block preceded by marker 0x51 */
#if (VERSION_MAJOR > 0) || (VERSION_MAJOR == 0 && (VERSION_MINOR > 8 || (VERSION_MINOR == 8 && VERSION_PATCH >= 6)))
    log_trace("Writing quest block marker 0x51 (version %s)", VERSION_STRING);
    wr_byte(0x51); /* quest block marker */
    wr_byte(p_ptr->tulkas_quest);
    wr_s16b(p_ptr->tulkas_target_r_idx);
    wr_s16b(p_ptr->tulkas_prize_a_idx);
    wr_byte(p_ptr->tulkas_quest_complete);
    /* Aule quest fields */
    wr_byte(p_ptr->aule_quest);
    wr_byte(p_ptr->aule_forge_y);
    wr_byte(p_ptr->aule_forge_x);
    wr_byte(p_ptr->aule_reserved);
    wr_s16b(p_ptr->aule_level);
    wr_s16b(p_ptr->aule_last_object_diff);
    /* Mandos quest fields */
    wr_byte(p_ptr->mandos_quest);
    wr_byte(p_ptr->mandos_vault_y);
    wr_byte(p_ptr->mandos_vault_x);
    wr_byte(p_ptr->mandos_monsters_remaining);
    wr_s16b(p_ptr->mandos_level);
    wr_s16b(p_ptr->mandos_reserved);
    /* Niena quest fields */
    wr_byte(p_ptr->niena_quest);
    wr_byte(p_ptr->niena_monsters_seen);
    wr_byte(p_ptr->niena_monsters_killed);
    wr_byte(p_ptr->niena_reserved);
    wr_s16b(p_ptr->niena_level);
    wr_s16b(p_ptr->niena_reserved2);
    /* Orome quest fields */
    wr_byte(p_ptr->orome_quest);
    wr_byte(p_ptr->orome_target_type);
    wr_s16b(p_ptr->orome_target_count);
    wr_s16b(p_ptr->orome_killed_count);
    wr_s16b(p_ptr->orome_wolves_killed);
    wr_s16b(p_ptr->orome_spiders_killed);
    wr_s16b(p_ptr->orome_serpents_killed);
    wr_s16b(p_ptr->orome_vampires_killed);
    /* Varda quest fields */
    wr_byte(p_ptr->varda_quest);
    wr_byte(p_ptr->varda_vault_ready);
    wr_byte(p_ptr->varda_vault_placed);
    wr_byte(p_ptr->varda_reserved);
    wr_s16b(p_ptr->varda_level);
    wr_byte(p_ptr->quest_vault_used);
    for (i = 0; i < 15; i++) wr_byte(p_ptr->quest_reserved[i]);
#else
    /* Older versions (<=0.8.5) had no quest block; do not write marker */
#endif

    /* Skeleton note state (per-level tutorial-style messages) */
    {
        skeleton_note_state_save sn_state;
        skeleton_note_get_state(&sn_state);
        wr_byte(0x52);
        wr_s16b(sn_state.level_depth);
        wr_s16b(sn_state.note_cap);
        wr_s16b(sn_state.notes_shown);
        wr_s16b(sn_state.map_wid);
        wr_s16b(sn_state.map_hgt);
        wr_byte(sn_state.hint_used_mask);
        wr_byte(sn_state.seen_count);
        for (i = 0; i < SKELETON_NOTE_SEEN_MAX; i++)
            wr_s16b(sn_state.seen_ids[i]);
    }

    wr_s32b(min_depth_counter);
    log_info("SAVE: min_depth_counter=%d, current depth=%d, calculated min_depth()=%d", 
             min_depth_counter, p_ptr->depth, min_depth());

    log_debug("Updating character info output file");
    updatecharinfoS();
}

/*
 * Dump the random artefacts
 */
static void wr_randarts(void)
{
    int i, begin;

    if (adult_rand_artefacts)
        begin = 0;
    else
        begin = z_info->art_norm_max;

    wr_u16b(begin);
    wr_u16b(z_info->art_max);
    wr_u16b(z_info->art_norm_max);

    for (i = begin; i < z_info->art_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];

        wr_string(a_ptr->name);
        wr_u32b(a_ptr->guid.hi);
        wr_u32b(a_ptr->guid.lo);

        wr_byte(a_ptr->tval);
        wr_byte(a_ptr->sval);
        wr_s16b(a_ptr->pval);

        wr_s16b(a_ptr->att);
        wr_byte(a_ptr->dd);
        wr_byte(a_ptr->ds);
        wr_s16b(a_ptr->evn);
        wr_byte(a_ptr->pd);
        wr_byte(a_ptr->ps);

        wr_s16b(a_ptr->weight);
        wr_s32b(a_ptr->cost);

        wr_u32b(a_ptr->flags1);
        wr_u32b(a_ptr->flags2);
        wr_u32b(a_ptr->flags3);

        wr_byte(a_ptr->level);
        wr_byte(a_ptr->rarity);

        wr_byte(a_ptr->activation);
        wr_u16b(a_ptr->time);
        wr_u16b(a_ptr->randtime);
    }
}

/*
 * Write the notes into the savefile. Every savefile has at least NOTES_MARK.
 */
static void wr_notes(void)
{
    char end_note[80];
    char tmpstr[100];
    char ch;
    bool done = false;

    int i = 0;
    int j = 0;

    // Sil: I've had to re-do this with the removal of the notes file
    //      The code below is pretty verbose and surely there was a better way!
    while (!done)
    {
        j = 0;

        while (true)
        {
            ch = notes_buffer[i];

            tmpstr[j] = ch;

            i++;
            j++;

            if (ch == '\n')
            {
                tmpstr[j - 1] = '\0';

                wr_string(tmpstr);
                break;
            }

            if (ch == '\0')
            {
                done = true;
                break;
            }
        }
    }

    // copy the special notes marker into a string
    SDL_strlcpy(end_note, NOTES_MARK, sizeof(end_note));

    /* Always write NOTES_MARK */
    wr_string(end_note);
}

/*
 * The cave grid flags that get saved in the savefile
 */
#define IMPORTANT_FLAGS                                                        \
    (CAVE_MARK | CAVE_GLOW | CAVE_ICKY | CAVE_ROOM | CAVE_G_VAULT | CAVE_HIDDEN)

/*
 * Write the current dungeon
 */
static void wr_dungeon(void)
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
            /* Extract the important cave_info flags */
            tmp8u = (cave_info[y][x] & (IMPORTANT_FLAGS));

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

/*
 * Actually write a save-file
 */
static bool wr_savefile(void)
{
    int i;

    u32b now;

    u16b tmp16u;

    log_trace("Starting wr_savefile");
    
    /* Reset write error flag */
    write_error = false;

    /* Reset byte offset counter */
    save_byte_offset = 0;
    log_trace("=== SAVE: Reset byte offset counter ===");

    /* Guess at the current time */
    now = time((time_t*)0);

    /* Note the operating system */
    sf_xtra = 0L;

    /* Note when the file was saved */
    sf_when = now;

    /* Note the number of saves */
    sf_saves++;

    /*** Actually write the file ***/

    /* Dump the file header */
    xor_byte = 0;
    wr_byte(VERSION_MAJOR);
    xor_byte = 0;
    wr_byte(VERSION_MINOR);
    xor_byte = 0;
    wr_byte(VERSION_PATCH);
    xor_byte = 0;
    wr_byte(VERSION_EXTRA);

    /* Reset the checksum */
    v_stamp = 0L;
    x_stamp = 0L;

    /* Operating system */
    wr_u32b(sf_xtra);

    /* Time file last saved */
    wr_u32b(sf_when);

    /* Number of past lives */
    wr_u16b(sf_lives);

    /* Number of times saved */
    wr_u16b(sf_saves);

    // 8 spare bytes
    wr_u32b(0L);
    wr_u32b(0L);

    /* Write the RNG state */
    log_trace("Writing RNG state");
    wr_randomizer();

    /* Write the boolean "options" */
    log_trace("Writing options");
    wr_options();

    /* Dump the number of "messages" */
    tmp16u = message_num();
    wr_u16b(tmp16u);

    log_trace("Writing %d messages", tmp16u);

    /* Dump the messages (oldest first!) */
    for (i = tmp16u - 1; i >= 0; i--)
    {
        wr_string(message_str((s16b)i));
        wr_u16b(message_type((s16b)i));
    }

    /* Dump the monster lore */
    tmp16u = z_info->r_max;
    wr_u16b(tmp16u);
    log_debug("Writing %d monster lore entries", tmp16u);
    for (i = 0; i < tmp16u; i++)
        wr_lore(i);

    /* Dump runtime monster stat overrides (supports dynamic buffs/debuffs) */
    wr_monster_runtime_overrides();

    /* Dump the object memory */
    tmp16u = z_info->k_max;
    wr_u16b(tmp16u);
    log_debug("Writing %d object memory entries", tmp16u);
    for (i = 0; i < tmp16u; i++)
        wr_xtra(i);

    /* Hack -- Dump the artefacts */
    tmp16u = z_info->art_max;
    wr_u16b(tmp16u);
    log_debug("Writing %d artefact entries", tmp16u);
    for (i = 0; i < tmp16u; i++)
    {
        artefact_type* a_ptr = &a_info[i];
        wr_byte(a_ptr->cur_num);
        wr_byte(a_ptr->found_num);
        wr_byte(a_ptr->seen);
    }

    /* Write the "extra" information */
    log_trace("Writing extra information...\n");

    wr_extra();



    /*Write the randarts*/
    log_trace("Writing random artefacts...");
    wr_randarts();

    /*Copy the notes file into the savefile*/
    log_trace("Writing notes...");
    wr_notes();

    // Write the smithing item
    log_debug("Writing smithing item");
    log_trace("[save:%06u] === BEGIN SMITHING ITEM ===", (unsigned)save_byte_offset);
    wr_item(smith_o_ptr);
    log_trace("[save:%06u] === END SMITHING ITEM ===", (unsigned)save_byte_offset);

    /* Write the inventory */
    log_debug("Writing player inventory");
    log_trace("[save:%06u] === BEGIN INVENTORY ===", (unsigned)save_byte_offset);
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Dump index */
        log_trace("[save:%06u] Writing inventory slot %d", (unsigned)save_byte_offset, i);
        wr_u16b((u16b)i);

        /* Dump object */
        log_trace("Writing inventory item %d", i);
        wr_item(o_ptr);
    }

    /* Add a sentinel */
    log_trace("[save:%06u] Writing inventory sentinel 0xFFFF", (unsigned)save_byte_offset);
    wr_u16b(0xFFFF);
    log_trace("[save:%06u] === END INVENTORY ===", (unsigned)save_byte_offset);

    /* Write supplies cache */
    log_trace("[save:%06u] === BEGIN SUPPLIES ===", (unsigned)save_byte_offset);
    {
        u16b supply_count = (u16b)supplies_entry_count();
        wr_u16b(supply_count);
        log_debug("Writing %u supply entries", (unsigned)supply_count);
        for (u16b si = 0; si < supply_count; si++) {
            object_type* supply_obj = supplies_entry_at(si);
            s32b stored_units = 0;
            if (supply_obj && supply_obj->k_idx) {
                log_trace("[save:%06u] Writing supply entry %u", (unsigned)save_byte_offset, (unsigned)si);
                wr_item(supply_obj);
                stored_units = supplies_entry_units(si);
            } else {
                object_type blank;
                object_wipe(&blank);
                log_trace("[save:%06u] Writing blank supply entry %u", (unsigned)save_byte_offset, (unsigned)si);
                wr_item(&blank);
            }
            wr_s32b(stored_units);
        }
    }
    log_trace("[save:%06u] === END SUPPLIES ===", (unsigned)save_byte_offset);

    /* Player is not dead, write the dungeon */
    log_debug("save: p_ptr->is_dead = %d, will %s dungeon", 
             p_ptr->is_dead, p_ptr->is_dead ? "SKIP" : "write");
    
    if (!p_ptr->is_dead)
    {
        /* Check for invalid dungeon state */
        if (p_ptr->cur_map_wid == 0 || p_ptr->cur_map_hgt == 0)
        {
            log_error("CRITICAL: Attempting to save with invalid dungeon dimensions: %dx%d at depth %d",
                     p_ptr->cur_map_hgt, p_ptr->cur_map_wid, p_ptr->depth);
            log_error("  Player position: (%d, %d)", p_ptr->py, p_ptr->px);
            log_error("  character_generated: %d", character_generated);
            /* This is a critical error - dungeon should always be valid for alive characters */
        }
        
        /* Dump the dungeon */
        log_trace("Writing dungeon...");
        wr_dungeon();
    }
    else
    {
        log_warn("SKIPPING dungeon write because p_ptr->is_dead = TRUE");
    }

    /* Write the "value check-sum" */
    wr_u32b(v_stamp);

    /* Write the "encoded checksum" */
    wr_u32b(x_stamp);

    /* Check for write errors */
    if (write_error)
    {
        log_error("Save aborted: write_error flag was set during save");
        return false;
    }

    /* Flush the stream to ensure all data is written */
    int flush_result = SDL_FlushIO(fff);
    if (flush_result != 0)
    {
        /* SDL_FlushIO returns 0 on success or negative on error */
        log_warn("Save file flush returned %d (SDL Error: '%s') - attempting to continue anyway", 
                 flush_result, SDL_GetError());
        /* Don't fail the save just because flush failed - the data might still be written */
        /* return false; */
    }

    /* Successful save */
    log_debug("Savefile write completed successfully");
    return true;
}

/*
 * Medium level player saver
 *
 * XXX XXX XXX Angband 2.8.0 will use "fd" instead of "fff" if possible
 */
static bool save_player_aux(cptr name)
{
    bool ok = false;

    SDL_IOStream* fd;

    int mode = 0644;

    /* No file yet */
    fff = NULL;

    /* File type is "SAVE" */
    FILE_TYPE(FILE_TYPE_SAVE);

    /* Grab permissions */
    safe_setuid_grab();

    /* Create the savefile */
    log_debug("save_player_aux: calling sdl_fmake(%s, %d)", name, mode);
    fd = sdl_fmake(name, mode);
    if (!fd)
    {
        log_error("save_player_aux: sdl_fmake FAILED for %s - SDL Error: %s", name, SDL_GetError());
    }
    else
    {
        log_debug("save_player_aux: sdl_fmake succeeded for %s", name);
    }

    /* Drop permissions */
    safe_setuid_drop();

    /* File is okay */
    if (fd)
    {
        /* Close the "fd" */
        sdl_fclose(fd);

        /* Grab permissions */
        safe_setuid_grab();

        /* Open the savefile */
        fff = sdl_fopen(name, "wb");

        /* Drop permissions */
        safe_setuid_drop();

        /* Successful open */
        if (fff)
        {
            /* Write the savefile */
            log_trace("Writing savefile %s", name);
            bool write_ok = wr_savefile();
            if (write_ok)
            {
                ok = true;
                log_debug("wr_savefile() succeeded");
            }
            else
            {
                log_error("wr_savefile() failed");
            }

            /* Attempt to close it */
            if (sdl_fclose(fff))
            {
                ok = false;
                log_error("sdl_fclose() failed");
            }
        }
        else
        {
            log_error("Failed to open savefile for writing: %s", name);
        }

        /* Grab permissions */
        safe_setuid_grab();

        /* Remove "broken" files */
        if (!ok)
        {
            log_debug("Removing broken savefile: %s", name);
            fd_kill(name);
        }

        /* Drop permissions */
        safe_setuid_drop();
    }
    else
    {
        log_error("Failed to create savefile: %s", name);
    }

    /* Failure */
    if (!ok)
    {
        log_error("save_player_aux failed for file: %s", name);
        return (false);
    }

    /* Successful save */
    character_saved = true;
    log_debug("Character saved successfully to: %s", name);

    /* Success */
    return (true);
}

/*
 * Attempt to save the player in a savefile
 */
bool save_player(void)
{
    int result = false;

    char safe[1024];

    log_info("Starting game save...");

    // in final deployment versions, you cannot save in the tutorial
    if (DEPLOYMENT && p_ptr->game_type != 0)
    {
        log_info("Save blocked: tutorial mode in deployment build");
        return (false);
    }

    /* New savefile */
    SDL_strlcpy(safe, savefile, sizeof(safe));
    SDL_strlcat(safe, ".new", sizeof(safe));

#ifdef VM
    /* Hack -- support "flat directory" usage on VM/ESA */
    SDL_strlcpy(safe, savefile, sizeof(safe));
    SDL_strlcat(safe, "n", sizeof(safe));
#endif /* VM */

    /* Grab permissions */
    safe_setuid_grab();

    /* Remove it */
    log_debug("save_player: attempting to remove existing .new file: %s", safe);
    if (!fd_kill(safe))
    {
        log_warn("save_player: fd_kill failed for %s (file may not exist, which is OK)", safe);
    }
    else
    {
        log_debug("save_player: successfully removed %s", safe);
    }

    /* Drop permissions */
    safe_setuid_drop();

    /* Attempt to save the player */
    log_debug("save_player: calling save_player_aux(%s)", safe);
    if (save_player_aux(safe))
    {
        char temp[1024];

        log_info("Save successful - activating new savefile");

        /* Old savefile */
        SDL_strlcpy(temp, savefile, sizeof(temp));
        SDL_strlcat(temp, ".old", sizeof(temp));

#ifdef VM
        /* Hack -- support "flat directory" usage on VM/ESA */
        SDL_strlcpy(temp, savefile, sizeof(temp));
        SDL_strlcat(temp, "o", sizeof(temp));
#endif /* VM */

        /* Grab permissions */
        safe_setuid_grab();

        /* Remove it */
        fd_kill(temp);

        /* Preserve old savefile if it exists */
        /* Check if old savefile exists first (important for first-time saves) */
        bool had_old_savefile = false;
        SDL_IOStream* old_fd = sdl_fopen(savefile, "rb");
        if (old_fd)
        {
            /* Old file exists, close it and preserve it */
            had_old_savefile = true;
            sdl_fclose(old_fd);
            log_debug("Old savefile exists, preserving it as .old");
            
            if (!fd_move(savefile, temp))
            {
                log_error("Failed to preserve old savefile - aborting activation");
                safe_setuid_drop();
                return (false);
            }
        }
        else
        {
            /* No old savefile - this is a first-time save, which is fine */
            log_debug("No old savefile found (first-time save) - skipping preserve step");
        }

        /* Activate new savefile */
        if (!fd_move(safe, savefile))
        {
            log_error("Failed to activate new savefile - attempting to restore old");
            /* Try to restore the old file if it existed */
            if (had_old_savefile)
            {
                fd_move(temp, savefile);
            }
            safe_setuid_drop();
            return (false);
        }

        /* Remove preserved savefile */
        fd_kill(temp);

        /* Drop permissions */
        safe_setuid_drop();

        /* Hack -- Pretend the character was loaded */
        character_loaded = true;

#ifdef VERIFY_SAVEFILE

        /* Lock on savefile */
        SDL_strlcpy(temp, savefile, sizeof(temp));
        SDL_strlcat(temp, ".lok", sizeof(temp));

        /* Grab permissions */
        safe_setuid_grab();

        /* Remove lock file */
        fd_kill(temp);

        /* Drop permissions */
        safe_setuid_drop();

#endif /* VERIFY_SAVEFILE */

        /* Success */
        result = true;
    }
    else
    {
        log_error("Save failed - could not write savefile");
    }

    char buf[1024];
    /* Build the filename */
    path_build(buf, sizeof(buf), ANGBAND_DIR_APEX, "meta.raw");

    // /* Attempt to open the meta file */
    // meta_fd = sdl_fopen(buf, O_RDWR);

    // // Save Metarun
    // strcpy(meta.name,"F");
    // if (!meta_seek(atoi(meta.id))) meta_write(&meta);

    // /* Close it */
    // sdl_fclose(meta_fd);

    /* Return the result */
    if (result) {
        log_info("Game save completed successfully");
    }
    return (result);
}








