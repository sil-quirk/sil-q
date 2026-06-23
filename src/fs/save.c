/* File: save.c */

/*
 * Copyright (c) 1997 Ben Harrison, and others
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "fs/save-internal.h"
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
u32b save_byte_offset = 0;

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

void wr_byte(byte v) { 
    sf_put(v);
}

void wr_u16b(u16b v)
{
    log_trace("[save:%06u] wr_u16b: 0x%04X (%u)", (unsigned)save_byte_offset, (unsigned)v, (unsigned)v);
    sf_put((byte)(v & 0xFF));
    sf_put((byte)((v >> 8) & 0xFF));
    save_byte_offset += 2;
}

void wr_s16b(s16b v) { 
    log_trace("[save:%06u] wr_s16b: %d", (unsigned)save_byte_offset, (int)v);
    wr_u16b((u16b)v); 
}

void wr_u32b(u32b v)
{
    log_trace("[save:%06u] wr_u32b: 0x%08X (%u)", (unsigned)save_byte_offset, (unsigned)v, (unsigned)v);
    sf_put((byte)(v & 0xFF));
    sf_put((byte)((v >> 8) & 0xFF));
    sf_put((byte)((v >> 16) & 0xFF));
    sf_put((byte)((v >> 24) & 0xFF));
    save_byte_offset += 4;
}

void wr_s32b(s32b v) { 
    log_trace("[save:%06u] wr_s32b: %d", (unsigned)save_byte_offset, (int)v);
    wr_u32b((u32b)v); 
}

void wr_string(cptr str)
{
    while (*str)
    {
        wr_byte(*str);
        str++;
    }
    wr_byte(*str);
}

static void wr_combat_roll(const combat_roll* roll)
{
    wr_u32b(roll->sequence);
    wr_byte((byte)roll->att_type);
    wr_s16b((s16b)roll->dam_type);
    wr_byte((byte)roll->attacker_char);
    wr_byte(roll->attacker_attr);
    wr_byte((byte)roll->defender_char);
    wr_byte(roll->defender_attr);
    wr_byte(roll->is_attacker_player ? 1 : 0);
    wr_byte(roll->is_defender_player ? 1 : 0);
    wr_s16b((s16b)roll->att);
    wr_s16b((s16b)roll->att_roll);
    wr_s16b((s16b)roll->evn);
    wr_s16b((s16b)roll->evn_roll);
    wr_s16b((s16b)roll->dd);
    wr_s16b((s16b)roll->ds);
    wr_s16b((s16b)roll->dam);
    wr_s16b((s16b)roll->pd);
    wr_s16b((s16b)roll->ps);
    wr_s16b((s16b)roll->prot);
    wr_s16b((s16b)roll->prt_percent);
    wr_byte(roll->melee ? 1 : 0);
}

static void wr_combat_history(void)
{
    bool has_current_round = (combat_number > 0);
    int history_to_write = combat_history_count;
    u16b count;

    if (history_to_write > MAX_COMBAT_HISTORY)
        history_to_write = MAX_COMBAT_HISTORY;

    if (has_current_round && history_to_write >= MAX_COMBAT_HISTORY)
        history_to_write = MAX_COMBAT_HISTORY - 1;

    count = (u16b)(history_to_write + (has_current_round ? 1 : 0));

    wr_u16b(count);
    log_trace("Writing %u combat history rounds", (unsigned)count);

    /* Dump the combat history oldest first, matching message history. */
    for (int h = history_to_write - 1; h >= 0; h--)
    {
        int hist_idx =
            (combat_history_head - h + MAX_COMBAT_HISTORY)
            % MAX_COMBAT_HISTORY;
        combat_history_round* round = &combat_history[hist_idx];
        u16b rolls = (u16b)round->num_rolls;

        if (rolls > MAX_COMBAT_ROLLS)
            rolls = MAX_COMBAT_ROLLS;

        wr_s32b((s32b)round->turn_count);
        wr_u16b(rolls);

        for (u16b r = 0; r < rolls; r++)
            wr_combat_roll(&round->rolls[r]);
    }

    if (has_current_round)
    {
        u16b rolls = (u16b)combat_number;

        if (rolls > MAX_COMBAT_ROLLS)
            rolls = MAX_COMBAT_ROLLS;

        wr_s32b((s32b)turn);
        wr_u16b(rolls);

        for (u16b r = 0; r < rolls; r++)
            wr_combat_roll(&combat_rolls[0][r]);
    }
}

/*
 * These functions write info in larger logical records
 */

/*
 * Write an "item" record
 */
void wr_item(const object_type* o_ptr)
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

    // bane_type for each ability slot (8 bytes)
    for (i = 0; i < 8; i++)
    {
        wr_byte(o_ptr->bane_type[i]);
    }

    /* Per-stat/skill modifiers */
    for (i = 0; i < A_MAX; i++)
    {
        wr_s16b(o_ptr->stat_bonus[i]);
    }
    for (i = 0; i < S_MAX; i++)
    {
        wr_s16b(o_ptr->skill_bonus[i]);
    }

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
void wr_monster(const monster_type* m_ptr)
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

    /* Thrall quest data */
    wr_byte(m_ptr->thrall_quest_item);
    wr_byte(m_ptr->thrall_quest_requested);
    wr_byte(m_ptr->thrall_quest_completed);
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

    /* Song-revealed lore plus spare bytes */
    wr_byte(l_ptr->song_lore_flags);
    wr_byte(0);
    wr_u16b(0);
    wr_u32b(0L);
}

static void wr_monster_runtime_overrides(void)
{
    /* Monster race template overrides are reconstructed from dedicated save
     * fields after load. Persisting the whole template blob can replay stale
     * edit-file data into newer builds, so new saves intentionally write none.
     */
    wr_u16b(0);
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

    /* Reserved: legacy per-kind squelch byte (now unused) */
    wr_byte(0);
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
    u32b window_flag[SAVE_WINDOW_TERM_MAX];
    u32b window_mask[SAVE_WINDOW_TERM_MAX];

    /*** Special Options ***/

    /* Write "delay_factor" */
    wr_byte(op_ptr->delay_factor);

    /* Write "hitpoint_warn" */
    wr_byte(op_ptr->hitpoint_warn);

    /* Legacy main-terminal combat row count; panes own combat display now. */
    wr_byte(0);

    /* Legacy "ability_desc_mode" slot. */
    wr_byte(0);

    /* Write "vault_drop_frequency" */
    wr_byte(op_ptr->vault_drop_frequency);

    /* Write "intro_style" */
    wr_byte(op_ptr->intro_style);

    /* Write "level_entry_narrative_mode" */
    wr_byte(op_ptr->level_entry_narrative_mode);

    /* Write "partition_narrative_mode" */
    wr_byte(op_ptr->partition_narrative_mode);

    /* Write "noble_item_spawn_mode" */
    wr_byte(op_ptr->noble_item_spawn_mode);

    /* Persist banner turns as value+1 so old saves' zero spare byte means "use default". */
    wr_byte((byte)(MIN(op_ptr->narrative_banner_turns,
        NARRATIVE_BANNER_TURNS_MAX) + 1));

    /* Write "min_depth_timer_mode" */
    wr_byte((byte)MIN(op_ptr->min_depth_timer_mode, MIN_DEPTH_TIMER_MODE_MAX));

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
    for (i = 0; i < SAVE_WINDOW_TERM_MAX; i++)
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
    for (i = 0; i < SAVE_WINDOW_TERM_MAX; i++)
        wr_u32b(window_flag[i]);

    /* Dump the masks */
    for (i = 0; i < SAVE_WINDOW_TERM_MAX; i++)
        wr_u32b(window_mask[i]);
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
        wr_u32b(message_sequence((s16b)i));
    }

    /* Dump the combat history (oldest first) */
    wr_combat_history();

    /* Dump the monster lore */
    tmp16u = z_info->r_max;
    wr_u16b(tmp16u);
    log_debug("Writing %d monster lore entries", tmp16u);
    for (i = 0; i < tmp16u; i++)
        wr_lore(i);

    /* Legacy monster template override block (intentionally empty) */
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

        /* Skip non-objects and stale empty stacks. */
        if (!o_ptr->k_idx || o_ptr->number <= 0)
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
        wr_u16b(SAVEFILE_SUPPLY_BLOCK_MAGIC);
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

    /* Write jewelry presets */
    log_trace("[save:%06u] === BEGIN JEWELRY PRESETS ===", (unsigned)save_byte_offset);
    {
        wr_u16b(SAVEFILE_JEWELRY_PRESET_BLOCK_MAGIC);
        wr_byte(JEWELRY_PRESET_MAX);
        for (byte preset = 0; preset < JEWELRY_PRESET_MAX; preset++)
        {
            bool set = jewelry_preset_is_set(preset);
            wr_byte(set ? 1 : 0);
            for (byte slot = 0; slot < JEWELRY_PRESET_SLOT_MAX; slot++)
            {
                const object_type* preset_obj =
                    jewelry_preset_object(preset, slot);
                if (preset_obj && preset_obj->k_idx)
                {
                    wr_item(preset_obj);
                }
                else
                {
                    object_type blank;
                    object_wipe(&blank);
                    wr_item(&blank);
                }
            }
        }
    }
    log_trace("[save:%06u] === END JEWELRY PRESETS ===", (unsigned)save_byte_offset);

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





