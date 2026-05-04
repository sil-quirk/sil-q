/* File: load.c */

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
#include "log/log.h"
#include "player/killer.h"
#include "score/score_guid.h"
#include <string.h> /* memset, strstr */
#include <stdio.h>  /* FILE, getc, ftell, fseek, ferror */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>  /* O_RDONLY */
#include <errno.h>
#include <stdbool.h>

/* #include "init.h"  not required directly here after refactor */
#include "metarun.h"

/*
 * This file loads savefiles from Sil.
 *
 * We attempt to prevent corrupt savefiles from inducing memory errors.
 *
 * Note that this file should not use the random number generator, the
 * object flavors, the visual attr/char mappings, or anything else which
 * is initialized *after* or *during* the "load character" function.
 *
 * This file assumes that the monster/object records are initialized
 * to zero, and the race/kind tables have been loaded correctly.  The
 * order of object stacks is currently not saved in the savefiles, but
 * the "next" pointers are saved, so all necessary knowledge is present.
 *
 * We should implement simple "savefile extenders" using some form of
 * "sized" chunks of bytes, with a {size,type,data} format, so everyone
 * can know the size, interested people can know the type, and the actual
 * data is available to the parsing routines that acknowledge the type.
 *
 * Consider changing the "globe of invulnerability" code so that it
 * takes some form of "maximum damage to protect from" in addition to
 * the existing "number of turns to protect for", and where each hit
 * by a monster will reduce the shield by that amount.  XXX XXX XXX
 */

/*
 * Local "savefile" pointer
 */
static SDL_IOStream* fff;

/*
 * Hack -- old "encryption" byte
 */
static byte xor_byte;

/*
 * Hack -- simple "checksum" on the actual values
 */
static u32b v_check = 0L;

/*
 * Hack -- simple "checksum" on the encoded bytes
 */
static u32b x_check = 0L;

/* Debug: count bytes consumed from save stream (post-decode) */
static u32b load_byte_offset = 0;

/* Helper macros for concise load logging (use DEBUG level so always visible in user logs) */
#define LOAD_LOG(fmt, ...) log_trace("[load:%06u] " fmt, (unsigned)load_byte_offset, __VA_ARGS__)
#define LOAD_LOG0(msg)      log_trace("[load:%06u] %s", (unsigned)load_byte_offset, msg)

/* Track feature availability for the currently loaded savefile. */
static bool savefile_has_runtime_overrides = false;
static bool savefile_has_monster_shatter = false;
static bool savefile_has_song_duels = false;
static bool savefile_has_ability_timeline = false;
static bool savefile_has_varda_quest = false;
static bool savefile_has_artifact_seen = false;
static bool savefile_has_skeleton_notes = false;
static bool savefile_has_skeleton_hint_mask = false;
static bool savefile_has_skeleton_hint_mask32 = false;
static bool savefile_has_partition_meta = false;
static bool savefile_has_partition_meta_types = false;
static bool savefile_has_cave_info_hi = false;
static bool savefile_has_hint_messages = false;
static bool savefile_has_hint_message_meta = false;
static bool savefile_has_thrall_quest = false;
static bool savefile_has_thrall_quest_requested = false;
static bool savefile_has_randart_flags4 = false;
static bool savefile_has_item_bonuses = false;
static bool savefile_has_randart_bonuses = false;
static bool savefile_has_morgoth_call_state = false;

/* Version comparison helpers: update these when bumping savefile semantics. */
static int savefile_version_compare(byte major, byte minor, byte patch, byte extra)
{
    if (sf_major != major)
        return (sf_major > major) ? 1 : -1;
    if (sf_minor != minor)
        return (sf_minor > minor) ? 1 : -1;
    if (sf_patch != patch)
        return (sf_patch > patch) ? 1 : -1;
    if (sf_extra != extra)
        return (sf_extra > extra) ? 1 : -1;
    return 0;
}

static bool savefile_version_at_least(byte major, byte minor, byte patch, byte extra)
{
    return savefile_version_compare(major, minor, patch, extra) >= 0;
}

static bool savefile_version_at_most(byte major, byte minor, byte patch, byte extra)
{
    return savefile_version_compare(major, minor, patch, extra) <= 0;
}

static bool savefile_version_supported(void)
{
    /* Reject savefiles older than our documented floor. */
    if (!savefile_version_at_least(OLD_VERSION_MAJOR, OLD_VERSION_MINOR, OLD_VERSION_PATCH, 0))
        return false;

    /* Reject savefiles from the future. */
    if (!savefile_version_at_most(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_EXTRA))
        return false;

    /* Enforce the minimum extra value for the current release series. */
    if (sf_major == VERSION_MAJOR && sf_minor == VERSION_MINOR && sf_patch == VERSION_PATCH)
    {
#if MIN_VERSION_EXTRA > 0
        if (sf_extra < MIN_VERSION_EXTRA)
            return false;
#endif
    }

    return true;
}
/* For backward-compatible reading: if the door-choices block is absent,
 * we prefetch the next u16 (objects count) here after probing. */
static u16b objects_count_prefetch = 0xFFFF;
/* Back-compat: some intermediate builds wrote door-choices block before
 * cave_color (i.e., between cave_feat and cave_color). We'll probe there; if
 * no magic, treat the two bytes as the first (count,value) pair for the
 * cave_color RLE, staging them here. */
static bool color_rle_pair_prefetched = false;
static byte color_rle_count_prefetch = 0;
static byte color_rle_value_prefetch = 0;

static u16b new_artefacts;
static u16b art_norm_count;
static bool load_note_screen_enabled = false;
static bool startup_loading_overlay_armed = false;
static bool startup_loading_overlay_shown = false;

static void maybe_show_startup_loading_overlay(void)
{
    static const char msg[] = "Loading...";
    int term_wid = 80;
    int term_hgt = 24;
    int row;
    int col;

    if (!startup_loading_overlay_armed || startup_loading_overlay_shown)
        return;

    Term_get_size(&term_wid, &term_hgt);
    row = term_hgt - 1;
    col = MAX(0, (term_wid - (int)strlen(msg)) / 2);

    Term_erase(0, row, 255);
    Term_putstr(col, row, term_wid - col, TERM_SLATE, msg);
    Term_fresh();

    startup_loading_overlay_shown = true;
}

void startup_loading_overlay_arm(void)
{
    startup_loading_overlay_armed = true;
    startup_loading_overlay_shown = false;
}

void startup_loading_overlay_disarm(void)
{
    startup_loading_overlay_armed = false;
    startup_loading_overlay_shown = false;
}

/*
 * Hack -- Show information on the screen, one line at a time.
 *
 * Avoid the top two lines, to avoid interference with "msg_print()".
 */
static void note(cptr msg)
{
    static int y = 2;

    if (msg && msg[0])
        log_debug("load note: %s", msg);

    maybe_show_startup_loading_overlay();

    if (!load_note_screen_enabled)
        return;

    /* Draw the message */
    prt(msg, y, 0);

    /* Advance one line (wrap if needed) */
    if (++y >= 24)
        y = 2;

    /* Flush it */
    Term_fresh();
}

/*
 * This counts the artefacts generated so far
 */
static int artefact_count(void)
{
    int i, count = 0;

    // note that it only counts through the fixed and random artefacts, not the
    // self-made ones
    for (i = 0; i < z_info->art_rand_max; i++)
    {
        if (((&a_info[i])->cur_num > 0)
            && !((a_info[i].flags3 & (TR3_INSTA_ART))))
        {
            count++;
        }
    }

    return (count);
}

/*
 * Hack -- determine if an item is "wearable" (or a missile)
 */
static bool wearable_p(const object_type* o_ptr)
{
    /* Valid "tval" codes */
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    case TV_BOW:
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    case TV_LIGHT:
    case TV_AMULET:
    case TV_RING:
    {
        return (true);
    }
    }

    /* Nope */
    return (false);
}

/*
 * The following functions are used to load the basic building blocks
 * of savefiles.  They also maintain the "checksum" info.
 */

static byte sf_get(void)
{
    byte c, v;

    /* Read a byte from the stream */
    if (SDL_ReadIO(fff, &c, 1) != 1)
    {
        log_error("sf_get: Failed to read byte at offset %ld", load_byte_offset);
        return (0);
    }
    
    /* Decode the value */
    v = c ^ xor_byte;
    xor_byte = c;

    /* Maintain the checksum info */
    v_check += v;
    x_check += xor_byte;

    /* Track offset (decoded payload byte) */
    load_byte_offset++;
    
    /* Return the value */
    return (v);
}

static void rd_byte(byte* ip) { 
    *ip = sf_get();
    /* load_byte_offset already incremented by sf_get() */
}

static void rd_bool(bool* bp) {
    *bp = sf_get() != 0;  // Any non-zero value becomes true
    /* load_byte_offset already incremented by sf_get() */
}

static void rd_u16b(u16b* ip)
{
    (*ip) = sf_get();
    (*ip) |= ((u16b)(sf_get()) << 8);
    /* load_byte_offset already incremented by sf_get() calls */
    log_trace("[load:%06u] rd_u16b: 0x%04X (%u)", (unsigned)(load_byte_offset - 2), (unsigned)*ip, (unsigned)*ip);
}

static void rd_s16b(s16b* ip) { 
    rd_u16b((u16b*)ip);
    log_trace("[load:%06u] rd_s16b: %d", (unsigned)(load_byte_offset - 2), (int)*ip);
}

static void rd_u32b(u32b* ip)
{
    (*ip) = sf_get();
    (*ip) |= ((u32b)(sf_get()) << 8);
    (*ip) |= ((u32b)(sf_get()) << 16);
    (*ip) |= ((u32b)(sf_get()) << 24);
    /* load_byte_offset already incremented by sf_get() calls */
    log_trace("[load:%06u] rd_u32b: 0x%08X (%u)", (unsigned)(load_byte_offset - 4), (unsigned)*ip, (unsigned)*ip);
}

static void rd_s32b(s32b* ip) { 
    rd_u32b((u32b*)ip);
    log_trace("[load:%06u] rd_s32b: %d", (unsigned)(load_byte_offset - 4), (int)*ip);
}

/*
 * Hack -- read a string
 */
static void rd_string(char* str, int max)
{
    int i;

    /* Read the string */
    for (i = 0; true; i++)
    {
        byte tmp8u;

        /* Read a byte */
        rd_byte(&tmp8u);

        /* Collect string while legal */
        if (i < max)
            str[i] = tmp8u;

        /* End of string */
        if (!tmp8u)
            break;
    }

    /* Terminate */
    str[max - 1] = '\0';
}

/*
 * Hack -- strip some bytes
 */
static void strip_bytes(int n)
{
    byte tmp8u;

    /* Strip the bytes */
    while (n--)
        rd_byte(&tmp8u);
}

/*
 * Read an object
 *
 * This function attempts to "repair" old savefiles, and to extract
 * the most up to date values for various object fields.
 */
static void convert_old_staff_of_warding(object_type* o_ptr)
{
    if (!o_ptr || o_ptr->tval != TV_STAFF || o_ptr->sval != SV_STAFF_WARDING)
        return;

    int uses = 0;
    if (o_ptr->pval > 0)
    {
        uses = o_ptr->pval / CHANNELING_CHARGE_MULTIPLIER;
        if (uses <= 0)
            uses = 1;
    }
    else if (o_ptr->number > 0)
    {
        uses = o_ptr->number;
    }

    if (uses < 0)
        uses = 0;

    o_ptr->tval = TV_GEM;
    o_ptr->sval = SV_GEM_WARDING;
    o_ptr->pval = 0;
    o_ptr->timeout = 0;

    if (uses > MAX_STACK_SIZE - 1)
        uses = MAX_STACK_SIZE - 1;
    o_ptr->number = uses;

    object_kind* k_ptr = &k_info[o_ptr->k_idx];
    if (k_ptr)
    {
        o_ptr->weight = k_ptr->weight;
    }

    if (o_ptr->number <= 0)
        o_ptr->ident |= IDENT_EMPTY;
    else
        o_ptr->ident &= ~(IDENT_EMPTY);

    memset(o_ptr->stat_bonus, 0, sizeof(o_ptr->stat_bonus));
    memset(o_ptr->skill_bonus, 0, sizeof(o_ptr->skill_bonus));
}

static void object_derive_stat_skill_bonuses_from_pval(object_type* o_ptr)
{
    if (!o_ptr)
        return;

    memset(o_ptr->stat_bonus, 0, sizeof(o_ptr->stat_bonus));
    memset(o_ptr->skill_bonus, 0, sizeof(o_ptr->skill_bonus));

    u32b f1, f2, f3;
    object_flags(o_ptr, &f1, &f2, &f3);

    if (f1 & TR1_STR)
        o_ptr->stat_bonus[A_STR] += o_ptr->pval;
    if (f1 & TR1_NEG_STR)
        o_ptr->stat_bonus[A_STR] -= o_ptr->pval;

    if (f1 & TR1_DEX)
        o_ptr->stat_bonus[A_DEX] += o_ptr->pval;
    if (f1 & TR1_NEG_DEX)
        o_ptr->stat_bonus[A_DEX] -= o_ptr->pval;

    if (f1 & TR1_CON)
        o_ptr->stat_bonus[A_CON] += o_ptr->pval;
    if (f1 & TR1_NEG_CON)
        o_ptr->stat_bonus[A_CON] -= o_ptr->pval;

    if (f1 & TR1_GRA)
        o_ptr->stat_bonus[A_GRA] += o_ptr->pval;
    if (f1 & TR1_NEG_GRA)
        o_ptr->stat_bonus[A_GRA] -= o_ptr->pval;

    if (f1 & TR1_MEL)
        o_ptr->skill_bonus[S_MEL] += o_ptr->pval;
    if (f1 & TR1_ARC)
        o_ptr->skill_bonus[S_ARC] += o_ptr->pval;
    if (f1 & TR1_STL)
        o_ptr->skill_bonus[S_STL] += o_ptr->pval;
    if (f1 & TR1_PER)
        o_ptr->skill_bonus[S_PER] += o_ptr->pval;
    if (f1 & TR1_WIL)
        o_ptr->skill_bonus[S_WIL] += o_ptr->pval;
    if (f1 & TR1_SMT)
        o_ptr->skill_bonus[S_SMT] += o_ptr->pval;
    if (f1 & TR1_SNG)
        o_ptr->skill_bonus[S_SNG] += o_ptr->pval;
}

static void artefact_derive_stat_skill_bonuses_from_pval(artefact_type* a_ptr)
{
    if (!a_ptr)
        return;

    memset(a_ptr->stat_bonus, 0, sizeof(a_ptr->stat_bonus));
    memset(a_ptr->skill_bonus, 0, sizeof(a_ptr->skill_bonus));
    memset(a_ptr->stat_bonus_set, 0, sizeof(a_ptr->stat_bonus_set));
    memset(a_ptr->skill_bonus_set, 0, sizeof(a_ptr->skill_bonus_set));

    if (a_ptr->flags1 & TR1_STR)
        a_ptr->stat_bonus[A_STR] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_NEG_STR)
        a_ptr->stat_bonus[A_STR] -= a_ptr->pval;

    if (a_ptr->flags1 & TR1_DEX)
        a_ptr->stat_bonus[A_DEX] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_NEG_DEX)
        a_ptr->stat_bonus[A_DEX] -= a_ptr->pval;

    if (a_ptr->flags1 & TR1_CON)
        a_ptr->stat_bonus[A_CON] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_NEG_CON)
        a_ptr->stat_bonus[A_CON] -= a_ptr->pval;

    if (a_ptr->flags1 & TR1_GRA)
        a_ptr->stat_bonus[A_GRA] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_NEG_GRA)
        a_ptr->stat_bonus[A_GRA] -= a_ptr->pval;

    if (a_ptr->flags1 & TR1_MEL)
        a_ptr->skill_bonus[S_MEL] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_ARC)
        a_ptr->skill_bonus[S_ARC] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_STL)
        a_ptr->skill_bonus[S_STL] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_PER)
        a_ptr->skill_bonus[S_PER] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_WIL)
        a_ptr->skill_bonus[S_WIL] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_SMT)
        a_ptr->skill_bonus[S_SMT] += a_ptr->pval;
    if (a_ptr->flags1 & TR1_SNG)
        a_ptr->skill_bonus[S_SNG] += a_ptr->pval;
}

static errr rd_item(object_type* o_ptr)
{
    u32b f1, f2, f3;

    object_kind* k_ptr;

    char buf[128];

    int i;

    /* Kind */
    rd_s16b(&o_ptr->k_idx);

    /* Paranoia */
    if ((o_ptr->k_idx < 0) || (o_ptr->k_idx >= z_info->k_max))
    {
        return (-1);
    }

    /* Hallucinatory Kind */
    rd_s16b(&o_ptr->image_k_idx);

    /* Location */
    rd_byte(&o_ptr->iy);
    rd_byte(&o_ptr->ix);

    /* Type/Subtype */
    rd_byte(&o_ptr->tval);
    rd_byte(&o_ptr->sval);

    /* Special pval */
    rd_s16b(&o_ptr->pval);

    rd_byte(&o_ptr->discount);

    rd_byte(&o_ptr->number);
    rd_s16b(&o_ptr->weight);

    rd_byte(&o_ptr->name1);
    rd_byte(&o_ptr->name2);

    rd_s16b(&o_ptr->timeout);

    rd_s16b(&o_ptr->att);
    rd_byte(&o_ptr->dd);
    rd_byte(&o_ptr->ds);
    rd_s16b(&o_ptr->evn);
    rd_byte(&o_ptr->pd);
    rd_byte(&o_ptr->ps);
    rd_byte(&o_ptr->pickup);
    rd_s16b(&o_ptr->pickup_slot);

    rd_u32b(&o_ptr->ident);

    rd_byte(&o_ptr->marked);

    /* Monster holding object */
    rd_s16b(&o_ptr->held_m_idx);

    /* Special powers */
    rd_byte(&o_ptr->xtra1);

    // granted abilities
    rd_byte(&o_ptr->abilities);
    for (i = 0; i < 8; i++)
    {
        rd_byte(&o_ptr->skilltype[i]);
        rd_byte(&o_ptr->abilitynum[i]);
    }

    rd_s32b(&o_ptr->unused1);
    rd_s32b(&o_ptr->unused2);
    rd_s32b(&o_ptr->unused3);
    rd_s32b(&o_ptr->unused4);

    // bane_type for each ability slot (8 bytes)
    for (i = 0; i < 8; i++)
    {
        rd_byte(&o_ptr->bane_type[i]);
    }

    /* Per-stat/skill modifiers */
    if (savefile_has_item_bonuses)
    {
        for (i = 0; i < A_MAX; i++)
        {
            rd_s16b(&o_ptr->stat_bonus[i]);
        }
        for (i = 0; i < S_MAX; i++)
        {
            rd_s16b(&o_ptr->skill_bonus[i]);
        }
    }
    else
    {
        memset(o_ptr->stat_bonus, 0, sizeof(o_ptr->stat_bonus));
        memset(o_ptr->skill_bonus, 0, sizeof(o_ptr->skill_bonus));
    }

    /* Inscription */
    rd_string(buf, sizeof(buf));

    /* Save the inscription */
    if (buf[0])
        o_ptr->obj_note = quark_add(buf);

    /* Obtain the "kind" template */
    k_ptr = &k_info[o_ptr->k_idx];

    /* Obtain tval/sval from k_info */
    o_ptr->tval = k_ptr->tval;
    o_ptr->sval = k_ptr->sval;

    /* Hack -- notice "broken" items */
    if (k_ptr->cost <= 0)
        o_ptr->ident |= (IDENT_BROKEN);

    /* Repair non "wearable" items */
    if (!wearable_p(o_ptr))
    {
        /* Get the correct fields */
        o_ptr->att = k_ptr->att;
        o_ptr->evn = k_ptr->evn;

        /* Get the correct fields */
        o_ptr->dd = k_ptr->dd;
        o_ptr->ds = k_ptr->ds;
        o_ptr->pd = k_ptr->pd;
        o_ptr->ps = k_ptr->ps;

        /* Get the correct weight */
        o_ptr->weight = k_ptr->weight;

        /* Paranoia */
        o_ptr->name1 = 0;
        object_set_ego_suffix(o_ptr, 0);
        object_set_ego_prefix(o_ptr, 0);

        /* All done */
        return (0);
    }

    /* Extract the flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    /* Migrate legacy visible {uncursed} to a hidden persisted marker. */
    if (o_ptr->discount == INSCRIP_UNCURSED)
    {
        o_ptr->ident |= IDENT_UNCURSED;
        o_ptr->discount = 0;
    }

    /* Preserve cleansed curse state without showing {uncursed}. */
    if ((f3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        && !(o_ptr->ident & IDENT_UNCURSED))
        o_ptr->ident |= (IDENT_CURSED);

    /* Paranoia */
    if (o_ptr->name1)
    {
        artefact_type* a_ptr;

        /*hack - adjust if new artefact*/
        if (o_ptr->name1 >= art_norm_count)
        {
            o_ptr->name1 += new_artefacts;
        }

        /* Paranoia */
        if (o_ptr->name1 >= z_info->art_max)
        {
            return (-1);
        }

        /* Obtain the artefact info */
        a_ptr = &a_info[o_ptr->name1];

        /* Verify that artefact */
        if (a_ptr->tval + a_ptr->sval == 0)
        {
            o_ptr->name1 = 0;
        }
    }

    /*
     * Ego items
     *
     * - Prefix ego is stored in o_ptr->unused2 (see object_ego_prefix()).
     * - Suffix ego is stored in o_ptr->name2 (see object_ego_suffix()).
     *
     * For compatibility with older saves (which only had one ego in name2),
     * migrate prefix-type egos from suffix->prefix based on the ego name.
     */
    if (o_ptr->unused2 < 0 || o_ptr->unused2 > 255)
        o_ptr->unused2 = 0;

    if (object_ego_prefix(o_ptr))
    {
        byte e_idx = object_ego_prefix(o_ptr);
        ego_item_type* e_ptr;

        if (e_idx >= z_info->e_max)
        {
            object_set_ego_prefix(o_ptr, 0);
        }
        else
        {
            e_ptr = &e_info[e_idx];
            if (!e_ptr->name)
            {
                object_set_ego_prefix(o_ptr, 0);
            }
            else if (!ego_name_is_prefix(e_name + e_ptr->name))
            {
                /* Corrupted/legacy: move to suffix slot if possible. */
                if (!object_ego_suffix(o_ptr))
                    object_set_ego_suffix(o_ptr, e_idx);
                object_set_ego_prefix(o_ptr, 0);
            }
        }
    }

    if (object_ego_suffix(o_ptr))
    {
        byte e_idx = object_ego_suffix(o_ptr);
        ego_item_type* e_ptr;

        if (e_idx >= z_info->e_max)
        {
            return (-1);
        }

        e_ptr = &e_info[e_idx];
        if (!e_ptr->name)
        {
            object_set_ego_suffix(o_ptr, 0);
        }
        else if (ego_name_is_prefix(e_name + e_ptr->name))
        {
            /* Legacy: prefix egos used to live in name2. */
            if (!object_ego_prefix(o_ptr))
                object_set_ego_prefix(o_ptr, e_idx);
            object_set_ego_suffix(o_ptr, 0);
        }
    }

    /* Hack -- extract the "broken" flag */
    if (o_ptr->pval < 0)
        o_ptr->ident |= (IDENT_BROKEN);

    /* Artefacts */
    if (o_ptr->name1)
    {
        artefact_type* a_ptr;

        /* Obtain the artefact info */
        a_ptr = &a_info[o_ptr->name1];

        /* Get the new artefact "pval" */
        o_ptr->pval = a_ptr->pval;

        /* Get the new artefact fields */
        o_ptr->dd = a_ptr->dd;
        o_ptr->ds = a_ptr->ds;
        o_ptr->pd = a_ptr->pd;
        o_ptr->ps = a_ptr->ps;
        o_ptr->evn = a_ptr->evn;

        /* Get the new artefact weight */
        o_ptr->weight = a_ptr->weight;

        /* Ensure artefact-granted abilities are present (some generators may omit them). */
        for (int ai = 0; ai < a_ptr->abilities && o_ptr->abilities < (int)N_ELEMENTS(o_ptr->skilltype); ai++)
        {
            bool found = false;
            for (int oi = 0; oi < o_ptr->abilities; oi++)
            {
                if (o_ptr->skilltype[oi] == a_ptr->skilltype[ai]
                    && o_ptr->abilitynum[oi] == a_ptr->abilitynum[ai])
                {
                    found = true;
                    /* Also copy bane_type if this is a Bane ability */
                    o_ptr->bane_type[oi] = a_ptr->bane_type[ai];
                    break;
                }
            }
            if (!found)
            {
                int idx = o_ptr->abilities;
                o_ptr->skilltype[idx] = a_ptr->skilltype[ai];
                o_ptr->abilitynum[idx] = a_ptr->abilitynum[ai];
                o_ptr->bane_type[idx] = a_ptr->bane_type[ai];
                o_ptr->abilities++;
            }
        }

        /* Hack -- extract the "broken" flag */
        if (!a_ptr->cost)
            o_ptr->ident |= (IDENT_BROKEN);
    }

    /* Ego items */
    if (object_ego_prefix(o_ptr))
    {
        ego_item_type* e_ptr = &e_info[object_ego_prefix(o_ptr)];
        if (!e_ptr->cost)
            o_ptr->ident |= (IDENT_BROKEN);
    }
    if (object_ego_suffix(o_ptr))
    {
        ego_item_type* e_ptr = &e_info[object_ego_suffix(o_ptr)];
        if (!e_ptr->cost)
            o_ptr->ident |= (IDENT_BROKEN);
    }

    convert_old_staff_of_warding(o_ptr);

    /* Back-compat: derive bonuses from pval+flags for older saves. */
    if (o_ptr->name1 && !savefile_has_item_bonuses)
    {
        artefact_type* a_ptr = &a_info[o_ptr->name1];
        memcpy(o_ptr->stat_bonus, a_ptr->stat_bonus, sizeof(o_ptr->stat_bonus));
        memcpy(o_ptr->skill_bonus, a_ptr->skill_bonus, sizeof(o_ptr->skill_bonus));
    }
    else if (!savefile_has_item_bonuses)
    {
        object_derive_stat_skill_bonuses_from_pval(o_ptr);
    }

    /* Log staff loading for debugging disappearing staff bug */
    if (o_ptr->tval == TV_STAFF)
    {
        log_debug("Loaded staff: k_idx=%d sval=%d pval=%d number=%d", 
                  o_ptr->k_idx, o_ptr->sval, o_ptr->pval, o_ptr->number);
    }

    /* Success */
    return (0);
}

/*
 * Read a monster
 */
static void rd_monster(monster_type* m_ptr)
{
    int i;

    /* Read the monster race */
    rd_s16b(&m_ptr->r_idx);

    /* Read the other information */
    rd_s16b(&m_ptr->image_r_idx);
    rd_byte(&m_ptr->fy);
    rd_byte(&m_ptr->fx);
    rd_s16b(&m_ptr->hp);
    rd_s16b(&m_ptr->maxhp);
    rd_s16b(&m_ptr->alertness);
    rd_byte(&m_ptr->skip_next_turn);
    rd_byte(&m_ptr->mspeed);
    rd_byte(&m_ptr->energy);
    rd_byte(&m_ptr->stunned);
    rd_byte(&m_ptr->confused);
    rd_s16b(&m_ptr->hasted);
    rd_s16b(&m_ptr->slowed);
    rd_byte(&m_ptr->stance);
    rd_s16b(&m_ptr->morale);
    rd_s16b(&m_ptr->tmp_morale);
    rd_byte(&m_ptr->noise);
    rd_byte(&m_ptr->encountered);
    rd_byte(&m_ptr->target_y);
    rd_byte(&m_ptr->target_x);
    rd_s16b(&m_ptr->wandering_idx);
    rd_byte(&m_ptr->wandering_dist);
    rd_byte(&m_ptr->mana);
    rd_byte(&m_ptr->song);
    rd_byte(&m_ptr->skip_this_turn);

    if (savefile_has_song_duels)
    {
        rd_byte(&m_ptr->song_contest_stacks);
        rd_byte(&m_ptr->song_lament_stacks);
        rd_byte(&m_ptr->song_lockout_timer);
        rd_byte(&m_ptr->song_hp_loss_lo);
        rd_s32b(&m_ptr->song_contest_last_turn);
        rd_s32b(&m_ptr->song_lament_last_turn);
        rd_s16b(&m_ptr->song_will_penalty);
        rd_s16b(&m_ptr->song_stealth_penalty);
        rd_s16b(&m_ptr->song_evasion_penalty);
        rd_byte(&m_ptr->song_armor_dice_penalty);
        rd_byte(&m_ptr->song_hp_loss_hi);
        rd_byte(&m_ptr->song_contest_completed);
        rd_byte(&m_ptr->song_lament_completed);
    }
    else
    {
        // legacy spare byte
        strip_bytes(1);
        m_ptr->song_contest_stacks = 0;
        m_ptr->song_lament_stacks = 0;
        m_ptr->song_lockout_timer = 0;
        m_ptr->song_hp_loss_lo = 0;
        m_ptr->song_contest_last_turn = 0;
        m_ptr->song_lament_last_turn = 0;
        m_ptr->song_will_penalty = 0;
        m_ptr->song_stealth_penalty = 0;
        m_ptr->song_evasion_penalty = 0;
        m_ptr->song_armor_dice_penalty = 0;
        m_ptr->song_hp_loss_hi = 0;
        m_ptr->song_contest_completed = 0;
        m_ptr->song_lament_completed = 0;
    }

    rd_s16b(&m_ptr->consecutive_attacks);
    rd_s16b(&m_ptr->turns_stationary);
    rd_u32b(&m_ptr->mflag);

    for (i = 0; i < ACTION_MAX; i++)
    {
        rd_byte(&m_ptr->previous_action[i]);
    }

    if (savefile_has_song_duels)
    {
        for (i = 0; i < MONSTER_BLOW_MAX; i++)
        {
            rd_byte(&m_ptr->blow_dd_reduction[i]);
        }
    }
    else
    {
        for (i = 0; i < MONSTER_BLOW_MAX; i++)
        {
            m_ptr->blow_dd_reduction[i] = 0;
        }
    }

    if (savefile_has_monster_shatter)
    {
        for (i = 0; i < MONSTER_BLOW_MAX; i++)
        {
            rd_byte(&m_ptr->blow_ds_reduction[i]);
        }

        rd_byte(&m_ptr->armor_ps_reduction);
        rd_byte(&m_ptr->shatter_padding[0]);
        rd_byte(&m_ptr->shatter_padding[1]);
        rd_byte(&m_ptr->shatter_padding[2]);
    }
    else
    {
        for (i = 0; i < MONSTER_BLOW_MAX; i++)
        {
            m_ptr->blow_ds_reduction[i] = 0;
        }

        m_ptr->armor_ps_reduction = 0;
        memset(m_ptr->shatter_padding, 0, sizeof(m_ptr->shatter_padding));
        strip_bytes(8);
    }

    /* Thrall quest data */
    if (savefile_has_thrall_quest)
    {
        rd_byte(&m_ptr->thrall_quest_item);
        if (savefile_has_thrall_quest_requested)
        {
            rd_byte(&m_ptr->thrall_quest_requested);
        }
        else
        {
            m_ptr->thrall_quest_requested = 0;
        }
        rd_byte(&m_ptr->thrall_quest_completed);
    }
    else
    {
        m_ptr->thrall_quest_item = THRALL_QUEST_NONE;
        m_ptr->thrall_quest_requested = 0;
        m_ptr->thrall_quest_completed = 0;
    }
}

/*
 * Read the monster lore
 */
static void rd_lore(int r_idx)
{
    int i;

    monster_race* r_ptr = &r_info[r_idx];
    monster_lore* l_ptr = &l_list[r_idx];

    /* Count deaths/sights/kills */
    rd_s16b(&l_ptr->deaths);
    rd_s16b(&l_ptr->psights);
    rd_s16b(&l_ptr->tsights);
    rd_s16b(&l_ptr->pkills);
    rd_s16b(&l_ptr->tkills);

    /* Count notices and ignores */
    rd_byte(&l_ptr->notice);
    rd_byte(&l_ptr->ignore);

    rd_byte(&l_ptr->drop_item);

    rd_byte(&l_ptr->ranged);

    /* Count blows of each type */
    for (i = 0; i < MONSTER_BLOW_MAX; i++)
        rd_byte(&l_ptr->blows[i]);

    /* Memorize flags */
    rd_u32b(&l_ptr->flags1);
    rd_u32b(&l_ptr->flags2);
    rd_u32b(&l_ptr->flags3);
    rd_u32b(&l_ptr->flags4);

    /* Read the "Racial" monster limit per level */
    rd_byte(&r_ptr->max_num);

    /* Song-revealed lore plus spare bytes */
    rd_byte(&l_ptr->song_lore_flags);
    strip_bytes(7);

    /* Repair the lore flags */
    l_ptr->flags1 &= r_ptr->flags1;
    l_ptr->flags2 &= r_ptr->flags2;
    l_ptr->flags3 &= r_ptr->flags3;
    l_ptr->flags4 &= r_ptr->flags4;
    l_ptr->song_lore_flags
        &= (MONSTER_LORE_SONG_CONTEST | MONSTER_LORE_SONG_LAMENT);
}

static void rd_monster_race_stats(monster_race* r_ptr)
{
    byte tmp8u;
    s16b tmp16s;
    u32b tmp32u;

    rd_byte(&tmp8u);
    r_ptr->hdice = tmp8u;
    rd_byte(&tmp8u);
    r_ptr->hside = tmp8u;
    rd_s16b(&tmp16s);
    r_ptr->evn = tmp16s;
    rd_byte(&tmp8u);
    r_ptr->pd = tmp8u;
    rd_byte(&tmp8u);
    r_ptr->ps = tmp8u;
    rd_byte(&tmp8u);
    r_ptr->speed = tmp8u;
    rd_s16b(&tmp16s);
    r_ptr->light = tmp16s;
    rd_s16b(&tmp16s);
    r_ptr->sleep = tmp16s;
    rd_s16b(&tmp16s);
    r_ptr->per = tmp16s;
    rd_s16b(&tmp16s);
    r_ptr->stl = tmp16s;
    rd_s16b(&tmp16s);
    r_ptr->wil = tmp16s;
    rd_s16b(&tmp16s);
    r_ptr->extra = tmp16s;
    rd_byte(&tmp8u);
    r_ptr->freq_ranged = tmp8u;
    rd_byte(&tmp8u);
    r_ptr->spell_power = tmp8u;
    rd_u32b(&tmp32u);
    r_ptr->mon_power = tmp32u;
    rd_u32b(&tmp32u);
    r_ptr->flags1 = tmp32u;
    rd_u32b(&tmp32u);
    r_ptr->flags2 = tmp32u;
    rd_u32b(&tmp32u);
    r_ptr->flags3 = tmp32u;
    rd_u32b(&tmp32u);
    r_ptr->flags4 = tmp32u;

    for (int i = 0; i < MONSTER_BLOW_MAX; i++)
    {
        rd_byte(&tmp8u);
        r_ptr->blow[i].method = tmp8u;
        rd_byte(&tmp8u);
        r_ptr->blow[i].effect = tmp8u;
        rd_s16b(&tmp16s);
        r_ptr->blow[i].att = tmp16s;
        rd_byte(&tmp8u);
        r_ptr->blow[i].dd = tmp8u;
        rd_byte(&tmp8u);
        r_ptr->blow[i].ds = tmp8u;
    }

    rd_byte(&tmp8u);
    r_ptr->level = tmp8u;
    rd_byte(&tmp8u);
    r_ptr->rarity = tmp8u;
    rd_byte(&tmp8u);
    r_ptr->d_attr = tmp8u;
    rd_byte(&tmp8u);
    r_ptr->d_char = (char)tmp8u;
    rd_byte(&tmp8u);
    r_ptr->x_attr = tmp8u;
    rd_byte(&tmp8u);
    r_ptr->x_char = (char)tmp8u;
}

static void restore_monster_races_from_base(void)
{
    if (!r_base)
        return;

    for (int r = 0; r < z_info->r_max; r++)
    {
        byte saved_max_num = r_info[r].max_num;

        r_info[r] = r_base[r];
        r_info[r].max_num = saved_max_num;
    }
}

static void rd_monster_runtime_overrides(void)
{
    u16b count = 0;

    rd_u16b(&count);

    if (!count)
        return;

    log_debug(
        "Discarding %u legacy monster race runtime overrides",
        (unsigned)count);

    for (u16b n = 0; n < count; n++)
    {
        u16b r_idx = 0;
        monster_race scratch;

        rd_u16b(&r_idx);
        memset(&scratch, 0, sizeof(scratch));
        rd_monster_race_stats(&scratch);

        if (r_idx >= z_info->r_max)
        {
            log_error(
                "Invalid monster race index %u in override block (max %u)",
                (unsigned)r_idx, (unsigned)z_info->r_max);
        }
    }
}
/*
 * Read RNG state
 */
static void rd_randomizer(void)
{
    int i;
    u16b dummy;
    u32b tmp32;
    u32b lo = 0;
    u32b hi = 0;

    strip_bytes(8);
    rd_u16b(&dummy);

    for (i = 0; i < 63; i++)
    {
        rd_u32b(&tmp32);
        if (i == 0)
            lo = tmp32;
        else if (i == 1)
            hi = tmp32;
    }

    Rand_state_import(((u64b)hi << 32) | lo);
}

/*
 * Read options
 *
 * Note that the normal options are stored as a set of 256 bit flags,
 * plus a set of 256 bit masks to indicate which bit flags were defined
 * at the time the savefile was created.  This will allow new options
 * to be added, and old options to be removed, at any time, without
 * hurting old savefiles.
 *
 * The window options are stored in the same way, but note that each
 * window gets 32 options, and their order is fixed by certain defines.
 */
static void rd_options(void)
{
    int i, n;

    byte b;

    u32b flag[8];
    u32b mask[8];
    u32b window_flag[ANGBAND_TERM_MAX];
    u32b window_mask[ANGBAND_TERM_MAX];

    /*** Special info */

    /* Read "delay_factor" */
    rd_byte(&b);
    op_ptr->delay_factor = b;

    /* Read "hitpoint_warn" */
    rd_byte(&b);
    op_ptr->hitpoint_warn = b;

    /* Read "main_combat_rolls" */
    rd_byte(&b);
    op_ptr->main_combat_rolls = b;
    /* Ensure it's in valid range */
    if (op_ptr->main_combat_rolls > 3)
        op_ptr->main_combat_rolls = 0;

    /* Read "ability_desc_mode" */
    rd_byte(&b);
    op_ptr->ability_desc_mode = b;
    if (op_ptr->ability_desc_mode > 2)
        op_ptr->ability_desc_mode = 0;

    /* Read "vault_drop_frequency" */
    rd_byte(&b);
    op_ptr->vault_drop_frequency = b;
    if (op_ptr->vault_drop_frequency > VDF_PLENTIFUL)
        op_ptr->vault_drop_frequency = VDF_NORMAL;

    /* Read "intro_style" */
    rd_byte(&b);
    op_ptr->intro_style = b;
    if (op_ptr->intro_style > INTRO_STYLE_RANDOM)
        op_ptr->intro_style = INTRO_STYLE_FLAME;

    if (savefile_version_at_least(0, 9, 5, 5))
    {
        rd_byte(&b);
        op_ptr->level_entry_narrative_mode = b;
        if (op_ptr->level_entry_narrative_mode > LEVEL_ENTRY_NARRATIVE_OFF)
            op_ptr->level_entry_narrative_mode = LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;

        rd_byte(&b);
        op_ptr->partition_narrative_mode = b;
        if (op_ptr->partition_narrative_mode > PARTITION_NARRATIVE_OFF)
            op_ptr->partition_narrative_mode = PARTITION_NARRATIVE_BANNER;

        rd_byte(&b);
        op_ptr->noble_item_spawn_mode = b;
        if (op_ptr->noble_item_spawn_mode > NOBLE_ITEM_SPAWN_INCLUDE_VAULTS)
            op_ptr->noble_item_spawn_mode = NOBLE_ITEM_SPAWN_RESTRICTED;

        rd_byte(&b);
        if ((b >= 1) && (b <= (NARRATIVE_BANNER_TURNS_MAX + 1)))
            op_ptr->narrative_banner_turns = (byte)(b - 1);
        else
            op_ptr->narrative_banner_turns = DEFAULT_NARRATIVE_BANNER_TURNS;

        if (savefile_version_at_least(0, 9, 6, 1))
        {
            rd_byte(&b);
            op_ptr->min_depth_timer_mode = b;
            if (op_ptr->min_depth_timer_mode > MIN_DEPTH_TIMER_MODE_MAX)
                op_ptr->min_depth_timer_mode = MIN_DEPTH_TIMER_MODE_NORMAL;
        }
        else
        {
            op_ptr->min_depth_timer_mode = MIN_DEPTH_TIMER_MODE_NORMAL;
        }
    }
    else
    {
        /* Old savefiles used the boolean show_level_entry_banner option. */
        op_ptr->level_entry_narrative_mode = LEVEL_ENTRY_NARRATIVE_BANNER_DELAY;
        op_ptr->partition_narrative_mode = PARTITION_NARRATIVE_BANNER;
        op_ptr->narrative_banner_turns = DEFAULT_NARRATIVE_BANNER_TURNS;
        op_ptr->noble_item_spawn_mode = NOBLE_ITEM_SPAWN_RESTRICTED;
        op_ptr->min_depth_timer_mode = MIN_DEPTH_TIMER_MODE_NORMAL;
        strip_bytes(4);
    }

    /*** Normal Options ***/

    /* Read the option flags */
    for (n = 0; n < 8; n++)
        rd_u32b(&flag[n]);

    /* Read the option masks */
    for (n = 0; n < 8; n++)
        rd_u32b(&mask[n]);

    /* Analyze the options */
    for (i = 0; i < OPT_MAX; i++)
    {
        int os = i / 32;
        int ob = i % 32;

        /* Process real entries */
        if (option_text[i])
        {
            /* Process saved entries */
            if (mask[os] & (1L << ob))
            {
                /* Set flag */
                if (flag[os] & (1L << ob))
                {
                    /* Set */
                    op_ptr->opt[i] = true;
                }

                /* Clear flag */
                else
                {
                    /* Set */
                    op_ptr->opt[i] = false;
                }
            }
        }
    }

    if (!savefile_version_at_least(0, 9, 5, 5))
    {
        op_ptr->level_entry_narrative_mode = op_ptr->opt[OPT_show_level_entry_banner]
            ? LEVEL_ENTRY_NARRATIVE_BANNER_DELAY
            : LEVEL_ENTRY_NARRATIVE_OFF;
        op_ptr->partition_narrative_mode = op_ptr->opt[OPT_show_partition_narrative]
            ? PARTITION_NARRATIVE_BANNER
            : PARTITION_NARRATIVE_OFF;
        op_ptr->narrative_banner_turns = DEFAULT_NARRATIVE_BANNER_TURNS;
        op_ptr->noble_item_spawn_mode = NOBLE_ITEM_SPAWN_RESTRICTED;
        op_ptr->min_depth_timer_mode = MIN_DEPTH_TIMER_MODE_NORMAL;
    }

    /*** Window Options ***/

    /* Read the window flags */
    for (n = 0; n < ANGBAND_TERM_MAX; n++)
    {
        rd_u32b(&window_flag[n]);
    }

    /* Read the window masks */
    for (n = 0; n < ANGBAND_TERM_MAX; n++)
    {
        rd_u32b(&window_mask[n]);
    }

    /* Analyze the options */
    for (n = 0; n < ANGBAND_TERM_MAX; n++)
    {
        op_ptr->window_flag[n] = 0;

        /* Analyze the options */
        for (i = 0; i < 32; i++)
        {
            /* Process valid flags */
            if (window_flag_desc[i])
            {
                /* Process valid flags */
                if (window_mask[n] & (1L << i))
                {
                    /* Set */
                    if (window_flag[n] & (1L << i))
                    {
                        /* Set */
                        op_ptr->window_flag[n] |= (1L << i);
                    }
                }
            }
        }
    }
}

static u32b randart_version;

/*
 * Read the "extra" information
 */
static errr rd_extra(void)
{
    int i, j;

    byte tmp8u;
    u16b file_e_max;

    log_debug("rd_extra: begin (byte_ofs=%u)", (unsigned)load_byte_offset);

    rd_string(op_ptr->full_name, sizeof(op_ptr->full_name));

    rd_string(p_ptr->died_from, 80);

    rd_string(p_ptr->history, 550);

    /* Player race */
    rd_byte(&p_ptr->prace);

    /* Verify player race */
    if (p_ptr->prace >= z_info->p_max)
    {
        note(format("Invalid player race (%d).", p_ptr->prace));
        return (-1);
    }

    /* Player character */
    rd_byte(&p_ptr->pcharacter);

    /* Verify player character */
    if (p_ptr->pcharacter >= z_info->c_max)
    {
        note(format("Invalid player character (%d).", p_ptr->pcharacter));
        return (-1);
    }

    /* Player sex */
    rd_byte(&p_ptr->unused1);

    /* Tutorial? */
    rd_s16b(&p_ptr->game_type);

    /* Age/Height/Weight */
    rd_s16b(&p_ptr->age);
    rd_s16b(&p_ptr->ht);
    rd_s16b(&p_ptr->wt);

    /* Read the stat info */
    for (i = 0; i < A_MAX; i++)
        rd_s16b(&p_ptr->stat_base[i]);
    for (i = 0; i < A_MAX; i++)
        rd_s16b(&p_ptr->stat_drain[i]);

    /* Read the skill info - all skills including S_SPC (Special) present in 0.9.0 */
    for (i = 0; i < S_MAX; i++)
        rd_s16b(&p_ptr->skill_base[i]);

    /* Read the abilities info (innate + active flags) */
    for (i = 0; i < S_MAX; i++)
    {
        for (j = 0; j < ABILITIES_MAX; j++)
        {
            rd_byte(&p_ptr->innate_ability[i][j]);
            rd_byte(&p_ptr->active_ability[i][j]);
            rd_byte(&p_ptr->have_ability[i][j]);
            
            /* Debug special abilities load */
            if (i == S_SPC && p_ptr->have_ability[i][j] != 0) {
                log_trace("Load: Special ability %d loaded with value %d", j, p_ptr->have_ability[i][j]);
            }
        }
    }

    p_ptr->ability_timeline_count = 0;
    if (savefile_has_ability_timeline)
    {
        u16b ability_events = 0;
        rd_u16b(&ability_events);
        if (ability_events > ABILITY_TIMELINE_MAX)
            ability_events = ABILITY_TIMELINE_MAX;

        for (u16b idx = 0; idx < ability_events; idx++)
        {
            byte skill = 0;
            byte abil = 0;
            u32b turn = 0;
            s16b depth = 0;

            rd_byte(&skill);
            rd_byte(&abil);
            rd_u32b(&turn);
            rd_s16b(&depth);

            if (idx < ABILITY_TIMELINE_MAX)
            {
                p_ptr->ability_timeline_skill[idx] = skill;
                p_ptr->ability_timeline_ability[idx] = abil;
                p_ptr->ability_timeline_turn[idx] = turn;
                p_ptr->ability_timeline_depth[idx] = depth;
                p_ptr->ability_timeline_count = idx + 1;
            }
        }
    }
    else
    {
        ability_log_reset();
    }

    rd_s16b(&p_ptr->last_attack_m_idx);
    rd_s16b(&p_ptr->consecutive_attacks);
    rd_s16b(&p_ptr->bane_type);

    for (i = 0; i < ACTION_MAX; ++i)
    {
        rd_byte(&(p_ptr->previous_action[i]));
    }
    rd_byte(&p_ptr->focused);

    rd_s32b(&p_ptr->new_exp);
    rd_s32b(&p_ptr->exp);

    rd_s32b(&p_ptr->encounter_exp);
    rd_s32b(&p_ptr->kill_exp);
    rd_s32b(&p_ptr->descent_exp);
    rd_s32b(&p_ptr->ident_exp);

    rd_s16b(&p_ptr->mhp);
    rd_s16b(&p_ptr->chp);
    rd_u16b(&p_ptr->chp_frac);

    rd_s16b(&p_ptr->msp);
    rd_s16b(&p_ptr->csp);
    rd_u16b(&p_ptr->csp_frac);

    rd_s16b(&p_ptr->max_depth);

    /* Hack -- Repair maximum dungeon level */
    if (p_ptr->max_depth < 0)
        p_ptr->max_depth = 1;

    rd_u16b(&p_ptr->staircasiness);

    /* More info */
    rd_s16b(&p_ptr->morgoth_state);
    
    log_debug("load: morgoth_state loaded as %d", p_ptr->morgoth_state);

    /* Read the flags */
    rd_byte(&p_ptr->song1);
    rd_byte(&p_ptr->song2);
    rd_s16b(&p_ptr->song_duration);
    if (savefile_has_song_duels)
    {
        rd_s16b(&p_ptr->song_target_idx);
        rd_byte(&p_ptr->song_target_song);
        rd_byte(&p_ptr->song_lockout_timer);
        rd_byte(&p_ptr->song_contest_player_stacks);
        rd_byte(&p_ptr->song_duel_pad);
        rd_s32b(&p_ptr->song_contest_last_turn);
    }
    else
    {
        p_ptr->song_target_idx = 0;
        p_ptr->song_target_song = SNG_NOTHING;
        p_ptr->song_lockout_timer = 0;
        p_ptr->song_contest_player_stacks = 0;
        p_ptr->song_duel_pad = 0;
        p_ptr->song_contest_last_turn = 0;
    }
    rd_s16b(&p_ptr->vengeance);
    rd_s16b(&p_ptr->blind);
    rd_s16b(&p_ptr->entranced);
    rd_s16b(&p_ptr->confused);
    rd_s16b(&p_ptr->food);
    rd_u16b(&p_ptr->stairs_taken);
    rd_u16b(&p_ptr->fixed_forge_count);
    rd_u16b(&p_ptr->forge_count);
    rd_s16b(&p_ptr->energy);
    rd_s16b(&p_ptr->fast);
    rd_s16b(&p_ptr->slow);
    rd_s16b(&p_ptr->afraid);
    rd_s16b(&p_ptr->cut);
    rd_s16b(&p_ptr->stun);
    rd_s16b(&p_ptr->poisoned);
    rd_s16b(&p_ptr->image);
    rd_s16b(&p_ptr->rage);
    rd_s16b(&p_ptr->tmp_str);
    rd_s16b(&p_ptr->tmp_dex);
    rd_s16b(&p_ptr->tmp_con);
    rd_s16b(&p_ptr->tmp_gra);
    rd_s16b(&p_ptr->tim_invis);
    rd_s16b(&p_ptr->tmp_per);
    rd_s16b(&p_ptr->darkened);
    rd_s16b(&p_ptr->oppose_fire);
    rd_s16b(&p_ptr->oppose_cold);
    rd_s16b(&p_ptr->oppose_pois);

    rd_s16b(&p_ptr->song_challenge_effect);
    rd_s16b(&p_ptr->song_elbereth_effect);

    rd_byte(&p_ptr->stealth_mode);
    rd_byte(&p_ptr->self_made_arts);
    rd_byte(&p_ptr->climbing);

    // Reserved block: 7 legacy bytes, 1 summons byte, 7 spare bytes.
    {
        byte morgoth_hall_entered = 0;
        byte morgoth_second_wind = 0;
        byte discovery_lore_flags = 0;
        s16b lamp_oil = 0;
        byte morgoth_call_state = 0;
        rd_byte(&morgoth_hall_entered);
        rd_byte(&morgoth_second_wind);
        rd_byte(&discovery_lore_flags);
        rd_s16b(&lamp_oil);
        strip_bytes(2);
        if (savefile_has_morgoth_call_state)
        {
            rd_byte(&morgoth_call_state);
            strip_bytes(7);
        }
        else
        {
            strip_bytes(8);
        }
        p_ptr->morgoth_hall_entered = morgoth_hall_entered ? 1 : 0;
        p_ptr->morgoth_second_wind = morgoth_second_wind ? 1 : 0;
        p_ptr->discovery_lore_flags = discovery_lore_flags;
        p_ptr->lamp_oil = lamp_oil;
        if (savefile_has_morgoth_call_state)
        {
            p_ptr->morgoth_call_state =
                morgoth_call_state
                & (SAVEFILE_MORGOTH_CALL_SEEN
                    | SAVEFILE_MORGOTH_CALL_ESCALATION_MASK);
        }
    }

    /* Read item-quality squelch sub-menu */
    for (i = 0; i < SQUELCH_BYTES; i++)
        rd_byte(&squelch_level[i]);

    /* Load the name of the current greater vault */
    rd_string(g_vault_name, sizeof(g_vault_name));

    /* Read the number of saved special item types */
    rd_u16b(&file_e_max);

    /* Read special item squelch settings */
    for (i = 0; i < z_info->e_max; i++)
    {
        ego_item_type* e_ptr = &e_info[i];

        tmp8u = 0;

        if (i < file_e_max)
            rd_byte(&tmp8u);

        e_ptr->squelch |= (tmp8u & 0x01);
        e_ptr->everseen |= (tmp8u & 0x02);
        e_ptr->aware |= (tmp8u & 0x04);

        /* Hack - Repair the savefile */
        if (!e_ptr->everseen)
            e_ptr->squelch = false;
    }

    /* Read possible extra elements */
    while (i < file_e_max)
    {
        rd_byte(&tmp8u);
        i++;
    }

    /*Write the current number of auto-inscriptions*/
    rd_u16b(&inscriptionsCount);

    /*Write the autoinscriptions array*/
    for (i = 0; i < inscriptionsCount; i++)
    {
        char tmp[80];

        rd_s16b(&inscriptions[i].kindIdx);

        rd_string(tmp, 80);

        inscriptions[i].inscriptionIdx = quark_add(tmp);
    }

    for (i = 0; i < MAX_GREATER_VAULTS; i++)
    {
        s16b n;

        rd_s16b(&n);
        p_ptr->greater_vaults[i] = n;
    }

    /* Read the randart version */
    rd_u32b(&randart_version);

    /* Read the randart seed */
    rd_u32b(&seed_randart);

    /* Hack -- the two "special seeds" */
    rd_u32b(&seed_flavor);

    /* Special stuff */
    rd_u16b(&p_ptr->panic_save);
    rd_byte(&p_ptr->truce);
    rd_byte(&p_ptr->morgoth_hits);
    rd_byte(&p_ptr->crown_hint);
    rd_byte(&p_ptr->crown_shatter);
    rd_byte(&p_ptr->cursed);
    rd_byte(&p_ptr->on_the_run);
    rd_byte(&p_ptr->morgoth_slain);
    rd_u16b(&p_ptr->escaped);
    rd_u16b(&p_ptr->noscore);
    rd_s16b(&p_ptr->smithing_leftover);

    // rd_byte(&tmp8u);
    // p_ptr->unique_forge_made = tmp8u;
    // rd_byte(&tmp8u);
    // p_ptr->unique_forge_seen = tmp8u;

    // /* Read "death" */
    // rd_byte(&tmp8u);
    // p_ptr->is_dead = tmp8u;
    rd_bool(&p_ptr->unique_forge_made);
    rd_bool(&p_ptr->unique_forge_seen);
    rd_bool(&p_ptr->is_dead);

    /* Read "feeling" */
    rd_byte(&tmp8u);
    feeling = tmp8u;

    /*read the level feeling*/
    rd_byte(&tmp8u);
    do_feeling = tmp8u;

    /* Current turn */
    rd_s32b(&turn);

    /* For 0.9.0+ (sf_extra >= 1), format is: turn, playerturn, crown_shatter_sil2, crown_shatter_sil3 */
    /* Legacy formats (< 0.8.9) are no longer supported */
    rd_s32b(&playerturn);
    rd_byte(&p_ptr->crown_shatter_sil2);
    rd_byte(&p_ptr->crown_shatter_sil3);

    rd_bool(&p_ptr->killed_enemy_with_arrow);

    rd_byte(&p_ptr->oath_type);
    rd_byte(&p_ptr->oaths_broken);
    log_info("LOAD: Read oath_type=%d, oaths_broken=%d at byte_ofs=%u", 
             p_ptr->oath_type, p_ptr->oaths_broken, (unsigned)load_byte_offset);

    /* Quest fields (0.8.6+ format with marker 0x51) */
    /* Legacy versions < 0.8.9 are not supported */
    log_debug("rd_extra: reading quest block (byte_ofs=%u) version=%u.%u.%u", 
             (unsigned)load_byte_offset, (unsigned)sf_major, (unsigned)sf_minor, (unsigned)sf_patch);
    
    byte marker;
    rd_byte(&marker);
    if (marker != 0x51) {
        note(format("Invalid quest marker 0x%02X (expected 0x51). Savefile too old (< 0.8.9).", marker));
        return (-1);
    }
    
    log_debug("QUEST: marker 0x51 detected, reading quest block");
    rd_byte(&p_ptr->tulkas_quest);
    rd_s16b(&p_ptr->tulkas_target_r_idx);
    rd_s16b(&p_ptr->tulkas_prize_a_idx);
    rd_byte(&p_ptr->tulkas_quest_complete);
    rd_byte(&p_ptr->aule_quest);
    rd_byte(&p_ptr->aule_forge_y);
    rd_byte(&p_ptr->aule_forge_x);
    rd_byte(&p_ptr->aule_reserved);
    rd_s16b(&p_ptr->aule_level);
    rd_s16b(&p_ptr->aule_last_object_diff);
    rd_byte(&p_ptr->mandos_quest);
    rd_byte(&p_ptr->mandos_vault_y);
    rd_byte(&p_ptr->mandos_vault_x);
    rd_byte(&p_ptr->mandos_monsters_remaining);
    rd_s16b(&p_ptr->mandos_level);
    rd_s16b(&p_ptr->mandos_reserved);
    rd_byte(&p_ptr->niena_quest);
    rd_byte(&p_ptr->niena_monsters_seen);
    rd_byte(&p_ptr->niena_monsters_killed);
    rd_byte(&p_ptr->niena_reserved);
    rd_s16b(&p_ptr->niena_level);
    rd_s16b(&p_ptr->niena_reserved2);
    rd_byte(&p_ptr->orome_quest);
    rd_byte(&p_ptr->orome_target_type);
    rd_s16b(&p_ptr->orome_target_count);
    rd_s16b(&p_ptr->orome_killed_count);
    rd_s16b(&p_ptr->orome_wolves_killed);
    rd_s16b(&p_ptr->orome_spiders_killed);
    rd_s16b(&p_ptr->orome_serpents_killed);
    rd_s16b(&p_ptr->orome_vampires_killed);
    if (savefile_has_varda_quest) {
        rd_byte(&p_ptr->varda_quest);
        rd_byte(&p_ptr->varda_vault_ready);
        rd_byte(&p_ptr->varda_vault_placed);
        rd_byte(&p_ptr->varda_reserved);
        rd_s16b(&p_ptr->varda_level);
    } else {
        p_ptr->varda_quest = VARDA_QUEST_NOT_STARTED;
        p_ptr->varda_vault_ready = 0;
        p_ptr->varda_vault_placed = 0;
        p_ptr->varda_reserved = 0;
        p_ptr->varda_level = 0;
    }
    rd_byte(&p_ptr->quest_vault_used);
    /* quest_reserved array grew in 0.9.1.3; read available bytes safely */
    int quest_reserved_len = savefile_version_at_least(0, 9, 1, 3) ? 15 : 12;
    for (int qi = 0; qi < quest_reserved_len && qi < (int)N_ELEMENTS(p_ptr->quest_reserved); qi++) {
        rd_byte(&p_ptr->quest_reserved[qi]);
    }
    for (int qi = quest_reserved_len; qi < (int)N_ELEMENTS(p_ptr->quest_reserved); qi++) {
        p_ptr->quest_reserved[qi] = 0;
    }

    /* If Varda has claimed this run's quest slot, ensure reservation persists after load. */
    if (p_ptr->varda_quest >= VARDA_QUEST_ACTIVE) {
        p_ptr->quest_reserved[0] = 1;
    }

    /* Skeleton note state (per-level tutorial-style messages) */
    if (savefile_has_skeleton_notes)
    {
        byte marker = 0;
        rd_byte(&marker);
        if (marker != 0x52)
        {
            note(format("Invalid skeleton note marker 0x%02X", marker));
            return (-1);
        }
        skeleton_note_state_save sn_state;
        rd_s16b(&sn_state.level_depth);
        rd_s16b(&sn_state.note_cap);
        rd_s16b(&sn_state.notes_shown);
        rd_s16b(&sn_state.map_wid);
        rd_s16b(&sn_state.map_hgt);
        if (savefile_has_skeleton_hint_mask32)
        {
            rd_u32b(&sn_state.hint_used_mask);
        }
        else if (savefile_has_skeleton_hint_mask)
        {
            byte tmp_mask = 0;
            rd_byte(&tmp_mask);
            sn_state.hint_used_mask = tmp_mask;
        }
        else
        {
            sn_state.hint_used_mask = 0;
        }
        rd_byte(&sn_state.seen_count);
        for (int i = 0; i < SKELETON_NOTE_SEEN_MAX; ++i)
            rd_s16b(&sn_state.seen_ids[i]);
        skeleton_note_set_state(&sn_state);
    }
    else
    {
        skeleton_note_set_state(NULL);
    }

    /* Partition generation metadata (grid + per-partition modes) */
    if (savefile_has_partition_meta)
    {
        byte marker = 0;
        rd_byte(&marker);
        if (marker != 0x53)
        {
            note(format("Invalid partition meta marker 0x%02X", marker));
            return (-1);
        }

        partition_meta_save pm;
        memset(&pm, 0, sizeof(pm));
        rd_s16b(&pm.grid_rows);
        rd_s16b(&pm.grid_cols);
        rd_s16b(&pm.partition_count);
        for (int i = 0; i < PARTITION_META_MAX; ++i)
            rd_byte(&pm.modes[i]);
        if (savefile_has_partition_meta_types)
        {
            for (int i = 0; i < PARTITION_META_MAX; ++i)
                rd_byte(&pm.big_cave_types[i]);
        }

        level_partition_meta_set(&pm);
    }

    /* Hint message log (per-level skeleton note archive) */
    if (savefile_has_hint_messages)
    {
        byte marker = 0;
        rd_byte(&marker);
        if (marker != 0x54)
        {
            note(format("Invalid hint message marker 0x%02X", marker));
            return (-1);
        }

        s16b level_depth = 0;
        s16b map_wid = 0;
        s16b map_hgt = 0;
        rd_s16b(&level_depth);
        rd_s16b(&map_wid);
        rd_s16b(&map_hgt);

        byte count = 0;
        rd_byte(&count);

        hint_messages_clear_for_load(level_depth, map_wid, map_hgt);

        for (int mi = 0; mi < count; ++mi)
        {
            hint_message_meta meta;
            byte line_count = 0;
            rd_byte(&line_count);

            char lines[16][100];
            int keep = (line_count > 16) ? 16 : line_count;
            for (int li = 0; li < keep; ++li)
                rd_string(lines[li], sizeof(lines[li]));
            for (int li = keep; li < 16; ++li)
                lines[li][0] = '\0';

            for (int li = keep; li < line_count; ++li)
            {
                char discard[100];
                rd_string(discard, sizeof(discard));
            }

            memset(&meta, 0, sizeof(meta));
            meta.source_y = -1;
            meta.source_x = -1;

            if (savefile_has_hint_message_meta)
            {
                rd_s16b(&meta.source_y);
                rd_s16b(&meta.source_x);
                rd_byte(&meta.cue_count);
                meta.cue_count = MIN(meta.cue_count, HINT_MESSAGE_CUE_MAX);
                for (int cue = 0; cue < HINT_MESSAGE_CUE_MAX; ++cue)
                {
                    rd_string(meta.cue_dists[cue], sizeof(meta.cue_dists[cue]));
                    rd_string(meta.cue_dirs[cue], sizeof(meta.cue_dirs[cue]));
                }
            }

            hint_messages_add_for_load(lines, keep, &meta);
        }
    }
    else
    {
        hint_messages_level_reset();
    }

    if (savefile_version_at_least(0, 9, 5, 6))
    {
        byte marker = 0;
        byte mode = RUN_MODE_STORY;
        int8_t stacks[METAR_CURSE_SLOTS];
        u32b seen_lo = 0;
        u32b seen_hi = 0;

        rd_byte(&marker);
        if (marker != 0x55)
        {
            note(format("Invalid blitz marker 0x%02X", marker));
            return (-1);
        }

        rd_byte(&mode);
        if (mode != RUN_MODE_BLITZ)
            mode = RUN_MODE_STORY;

        for (int bi = 0; bi < METAR_CURSE_SLOTS; ++bi)
        {
            byte raw = 0;
            rd_byte(&raw);
            stacks[bi] = (int8_t)raw;
        }
        rd_u32b(&seen_lo);
        rd_u32b(&seen_hi);

        run_mode_set_current((run_mode)mode);
        run_mode_set_pending((run_mode)mode);
        if (mode == RUN_MODE_BLITZ)
            blitz_runtime_restore(stacks, ((u64b)seen_hi << 32) | seen_lo);
        else
            blitz_runtime_reset();
    }
    else
    {
        run_mode_set_current(RUN_MODE_STORY);
        run_mode_set_pending(RUN_MODE_STORY);
        blitz_runtime_reset();
    }

    /* Min depth counter */
    rd_s32b(&min_depth_counter);
    morgoth_call_sync_loaded_stage();
    log_info("LOAD: min_depth_counter=%d, calculated min_depth()=%d", min_depth_counter, min_depth());

    /* Quest states loaded from save should remain as-is for this character */
    /* Metarun completion is checked separately via metarun_is_quest_completed() */

    ability_log_sync_missing();

    return (0);
}

/*
 * Read the random artefacts
 */
static errr rd_randarts(void)
{
    int i;
    byte tmp8u;
    s16b tmp16s;
    u16b tmp16u;
    u16b artefact_count, begin;
    s32b tmp32s;
    u32b tmp32u;

    LOAD_LOG0("enter rd_randarts");
    /* Read the number of artefacts */
    rd_u16b(&begin);
    rd_u16b(&artefact_count);
    rd_u16b(&art_norm_count);

    bool header_valid = true;
    if ((artefact_count > z_info->art_max) || (art_norm_count > z_info->art_norm_max)) header_valid = false;
    if (!(begin == 0 || begin == z_info->art_norm_max)) header_valid = false;

    log_debug("randarts header: begin=%u artefact_count=%u art_norm_count=%u (limits: art_max=%d art_norm_max=%d) valid=%d", begin, artefact_count, art_norm_count,
              z_info->art_max, z_info->art_norm_max, header_valid ? 1 : 0);

    /* Alive or cheating death */
    if (!p_ptr->is_dead || arg_wizard)
    {
        /* Incompatible save files */
        if ((artefact_count > z_info->art_max) || (art_norm_count > z_info->art_norm_max))
        {
            if (artefact_count > z_info->art_max)
                note(format("Too many artefacts in save (%u > %d)", artefact_count, z_info->art_max));
            else
                note(format("Too many normal artefacts in save (%u > %d)", art_norm_count, z_info->art_norm_max));
            log_error("randarts mismatch: begin=%u art=%u/%d norm=%u/%d", begin,
                      artefact_count, z_info->art_max, art_norm_count, z_info->art_norm_max);
            return (-1);
        }
        /*Mark any new added artefacts*/
        if (art_norm_count < z_info->art_norm_max)
        {
            new_artefacts = z_info->art_norm_max - art_norm_count;
        }
        else
            new_artefacts = 0;

        /* Mark the old artefacts as "empty" */
        for (i = begin; i < z_info->art_max; i++)
        {
            artefact_type* a_ptr = &a_info[i];

            /*hack - if a new "normal artefact has been added in mid-game, don't
             * erase it*/
            if ((i >= art_norm_count) && (i < z_info->art_norm_max))
                continue;

            a_ptr->tval = 0;
            a_ptr->sval = 0;
            a_ptr->name[0] = '\0';
            memset(a_ptr->stat_bonus, 0, sizeof(a_ptr->stat_bonus));
            memset(a_ptr->skill_bonus, 0, sizeof(a_ptr->skill_bonus));
            memset(a_ptr->stat_bonus_set, 0, sizeof(a_ptr->stat_bonus_set));
            memset(a_ptr->skill_bonus_set, 0, sizeof(a_ptr->skill_bonus_set));
        }

        /* Read the artefacts */
        for (i = begin; i < artefact_count; i++)
        {
            artefact_type* a_ptr = &a_info[i];

            /*hack - if a new "normal artefact has been added in mid-game, don't
             * erase it*/
            if ((i >= art_norm_count) && (i < z_info->art_norm_max))
                continue;

            rd_string(a_ptr->name, MAX_LEN_ART_NAME);
            if (randart_version >= 63)
            {
                rd_u32b(&a_ptr->guid.hi);
                rd_u32b(&a_ptr->guid.lo);
            }
            else
            {
                a_ptr->guid = score_guid_from_string(
                    a_ptr->name[0] ? a_ptr->name : "randart", (u32b)i);
            }

            rd_byte(&a_ptr->tval);
            rd_byte(&a_ptr->sval);
            rd_s16b(&a_ptr->pval);

            rd_s16b(&a_ptr->att);
            rd_byte(&a_ptr->dd);
            rd_byte(&a_ptr->ds);
            rd_s16b(&a_ptr->evn);
            rd_byte(&a_ptr->pd);
            rd_byte(&a_ptr->ps);

            rd_s16b(&a_ptr->weight);
            rd_s32b(&a_ptr->cost);

            rd_u32b(&a_ptr->flags1);
            rd_u32b(&a_ptr->flags2);
            rd_u32b(&a_ptr->flags3);
            if (savefile_has_randart_flags4)
                rd_u32b(&a_ptr->flags4);
            else
                a_ptr->flags4 = 0;
            rd_byte(&a_ptr->level);
            rd_byte(&a_ptr->rarity);
            rd_byte(&a_ptr->activation);
            rd_u16b(&a_ptr->time);
            rd_u16b(&a_ptr->randtime);

            if (savefile_has_randart_bonuses)
            {
                for (int bi = 0; bi < A_MAX; bi++)
                    rd_s16b(&a_ptr->stat_bonus[bi]);
                for (int bi = 0; bi < S_MAX; bi++)
                    rd_s16b(&a_ptr->skill_bonus[bi]);
                for (int bi = 0; bi < A_MAX; bi++)
                {
                    rd_byte(&tmp8u);
                    a_ptr->stat_bonus_set[bi] = tmp8u ? true : false;
                }
                for (int bi = 0; bi < S_MAX; bi++)
                {
                    rd_byte(&tmp8u);
                    a_ptr->skill_bonus_set[bi] = tmp8u ? true : false;
                }
            }
            else
            {
                artefact_derive_stat_skill_bonuses_from_pval(a_ptr);
            }
        }
    }
    else
    {
        /* Strip the the artefacts for a dead/new character*/
        for (i = begin; i < artefact_count; i++)
        {
            char tmpstr[MAX_LEN_ART_NAME];
            rd_string(tmpstr, sizeof(tmpstr)); /*a_ptr->name*/
            if (randart_version >= 63)
            {
                rd_u32b(&tmp32u);
                rd_u32b(&tmp32u);
            }
            rd_byte(&tmp8u); /* a_ptr->tval */
            rd_byte(&tmp8u); /* a_ptr->sval */
            rd_s16b(&tmp16s); /* a_ptr->pval */

            /* Added for Sil */
            rd_s16b(&tmp16s); // a_ptr->att
            rd_byte(&tmp8u); // a_ptr->dd
            rd_byte(&tmp8u); // a_ptr->ds
            rd_s16b(&tmp16s); // a_ptr->evn
            rd_byte(&tmp8u); // a_ptr->pd
            rd_byte(&tmp8u); // a_ptr->ps

            rd_s16b(&tmp16s); /* a_ptr->weight */
            rd_s32b(&tmp32s); /* a_ptr->cost */

            rd_u32b(&tmp32u); /* a_ptr->flags1 */
            rd_u32b(&tmp32u); /* a_ptr->flags2 */
            rd_u32b(&tmp32u); /* a_ptr->flags3 */
            if (savefile_has_randart_flags4)
                rd_u32b(&tmp32u); /* a_ptr->flags4 */
            rd_byte(&tmp8u); /* a_ptr->level */
            rd_byte(&tmp8u); /* a_ptr->rarity */

            rd_byte(&tmp8u); /* a_ptr->activation */
            rd_u16b(&tmp16u); /* a_ptr->time */
            rd_u16b(&tmp16u); /* a_ptr->randtime */

            if (savefile_has_randart_bonuses)
            {
                for (int bi = 0; bi < A_MAX; bi++)
                    rd_s16b(&tmp16s);
                for (int bi = 0; bi < S_MAX; bi++)
                    rd_s16b(&tmp16s);
                for (int bi = 0; bi < A_MAX; bi++)
                    rd_byte(&tmp8u);
                for (int bi = 0; bi < S_MAX; bi++)
                    rd_byte(&tmp8u);
            }
        }
    }

    return (0);
}

/*
 * Read the notes. Every new savefile has at least NOTES_MARK.
 */
static bool rd_notes(void)
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

typedef enum legacy_supply_kind
{
    LEGACY_SUPPLY_KIND_FOOD = 0,
    LEGACY_SUPPLY_KIND_POTIONS,
    LEGACY_SUPPLY_KIND_GEMS,
    LEGACY_SUPPLY_KIND_WOODEN_TORCHES,
    LEGACY_SUPPLY_KIND_MALLORN_TORCHES,
    LEGACY_SUPPLY_KIND_BRASS_LANTERNS,
    LEGACY_SUPPLY_KIND_LESSER_JEWELS,
    LEGACY_SUPPLY_KIND_FEANORIAN_LAMPS,
    LEGACY_SUPPLY_KIND_OIL_FLASKS,
    LEGACY_SUPPLY_KIND_MAX
} legacy_supply_kind;

typedef struct legacy_supply_migration_report
{
    int moved[LEGACY_SUPPLY_KIND_MAX];
    int dropped[LEGACY_SUPPLY_KIND_MAX];
    int discarded[LEGACY_SUPPLY_KIND_MAX];
    int oil_added;
    int oil_lost;
    int lanterns_drained;
    int flasks_converted;
} legacy_supply_migration_report;

static int legacy_supply_kind_for_object(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return -1;

    switch (o_ptr->tval)
    {
    case TV_FOOD:
        return LEGACY_SUPPLY_KIND_FOOD;
    case TV_POTION:
        return LEGACY_SUPPLY_KIND_POTIONS;
    case TV_GEM:
        return LEGACY_SUPPLY_KIND_GEMS;
    case TV_FLASK:
        return LEGACY_SUPPLY_KIND_OIL_FLASKS;
    case TV_LIGHT:
        switch (o_ptr->sval)
        {
        case SV_LIGHT_TORCH:
            return LEGACY_SUPPLY_KIND_WOODEN_TORCHES;
        case SV_LIGHT_MALLORN:
            return LEGACY_SUPPLY_KIND_MALLORN_TORCHES;
        case SV_LIGHT_LANTERN:
            return LEGACY_SUPPLY_KIND_BRASS_LANTERNS;
        case SV_LIGHT_LESSER_JEWEL:
            return LEGACY_SUPPLY_KIND_LESSER_JEWELS;
        case SV_LIGHT_FEANORIAN:
            return LEGACY_SUPPLY_KIND_FEANORIAN_LAMPS;
        default:
            break;
        }
        break;
    default:
        break;
    }

    return -1;
}

static cptr legacy_supply_kind_name(int kind, int count)
{
    bool plural = (count != 1);

    switch (kind)
    {
    case LEGACY_SUPPLY_KIND_FOOD:
        return plural ? "food items" : "food item";
    case LEGACY_SUPPLY_KIND_POTIONS:
        return plural ? "potions" : "potion";
    case LEGACY_SUPPLY_KIND_GEMS:
        return plural ? "gems" : "gem";
    case LEGACY_SUPPLY_KIND_WOODEN_TORCHES:
        return plural ? "wooden torches" : "wooden torch";
    case LEGACY_SUPPLY_KIND_MALLORN_TORCHES:
        return plural ? "mallorn torches" : "mallorn torch";
    case LEGACY_SUPPLY_KIND_BRASS_LANTERNS:
        return plural ? "brass lanterns" : "brass lantern";
    case LEGACY_SUPPLY_KIND_LESSER_JEWELS:
        return plural ? "lesser jewels" : "lesser jewel";
    case LEGACY_SUPPLY_KIND_FEANORIAN_LAMPS:
        return plural ? "Feanorian lamps" : "Feanorian lamp";
    case LEGACY_SUPPLY_KIND_OIL_FLASKS:
        return plural ? "oil flasks" : "oil flask";
    default:
        return plural ? "items" : "item";
    }
}

static void legacy_supply_record_counts(int counts[LEGACY_SUPPLY_KIND_MAX],
    const object_type* o_ptr, int amount)
{
    int kind = legacy_supply_kind_for_object(o_ptr);

    if (!counts || amount <= 0 || kind < 0 || kind >= LEGACY_SUPPLY_KIND_MAX)
        return;

    counts[kind] += amount;
}

static void legacy_supply_append_count(char* buf, size_t len, bool* first,
    int count, int kind)
{
    char part[64];

    if (!buf || !first || count <= 0)
        return;

    if (!*first)
        SDL_strlcat(buf, ", ", len);

    strnfmt(part, sizeof(part), "%d %s", count,
        legacy_supply_kind_name(kind, count));
    SDL_strlcat(buf, part, len);
    *first = false;
}

static bool legacy_supply_counts_present(const int counts[LEGACY_SUPPLY_KIND_MAX])
{
    for (int i = 0; i < LEGACY_SUPPLY_KIND_MAX; i++)
    {
        if (counts[i] > 0)
            return true;
    }

    return false;
}

static bool legacy_supply_migration_changed(
    const legacy_supply_migration_report* report)
{
    if (!report)
        return false;

    if (report->oil_added > 0 || report->oil_lost > 0
        || report->lanterns_drained > 0 || report->flasks_converted > 0)
    {
        return true;
    }

    return legacy_supply_counts_present(report->moved)
        || legacy_supply_counts_present(report->dropped)
        || legacy_supply_counts_present(report->discarded);
}

static void legacy_supply_add_oil(legacy_supply_migration_report* report,
    int amount)
{
    int before;

    if (!report || amount <= 0)
        return;

    before = (p_ptr && p_ptr->lamp_oil > 0) ? p_ptr->lamp_oil : 0;
    if (p_ptr)
        p_ptr->lamp_oil = before + amount;

    report->oil_added += amount;
}

static void legacy_supply_drop_object(object_type* drop,
    legacy_supply_migration_report* report)
{
    if (!drop || !drop->k_idx || drop->number <= 0)
        return;

    if (player_light_destroyed_on_drop(drop))
    {
        legacy_supply_record_counts(report->discarded, drop, drop->number);
        return;
    }

    drop_near(drop, 0, p_ptr->py, p_ptr->px);
    legacy_supply_record_counts(report->dropped, drop, drop->number);
}

static bool legacy_pack_permanent_light_matches(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx && o_ptr->tval == TV_LIGHT
        && ((o_ptr->sval == SV_LIGHT_LESSER_JEWEL)
            || (o_ptr->sval == SV_LIGHT_FEANORIAN));
}

static bool legacy_savefile_has_supply_block(void)
{
    Sint64 saved_pos;
    byte saved_xor;
    u32b saved_v_check;
    u32b saved_x_check;
    u32b saved_offset;
    u16b maybe_supply_count = 0;
    bool has_block = false;
    bool ok = true;

    saved_pos = sdl_tell(fff);
    if (saved_pos < 0)
        return false;

    saved_xor = xor_byte;
    saved_v_check = v_check;
    saved_x_check = x_check;
    saved_offset = load_byte_offset;

    rd_u16b(&maybe_supply_count);

    if (maybe_supply_count == 0 || maybe_supply_count > 512)
        goto restore;

    for (u16b i = 0; i < MIN((u16b)2, maybe_supply_count); i++)
    {
        object_type probe;
        s32b stored_units = 0;

        object_wipe(&probe);
        if (rd_item(&probe))
        {
            ok = false;
            break;
        }

        rd_s32b(&stored_units);

        if (!probe.k_idx || !supplies_is_supply_object(&probe)
            || stored_units < 0)
        {
            ok = false;
            break;
        }
    }

    has_block = ok;

restore:
    sdl_seek(fff, saved_pos);
    xor_byte = saved_xor;
    v_check = saved_v_check;
    x_check = saved_x_check;
    load_byte_offset = saved_offset;

    log_debug("Legacy supply block probe: %s (raw count=%u)",
        has_block ? "present" : "absent", (unsigned)maybe_supply_count);

    return has_block;
}

static void legacy_supply_trim_pack_permanent_lights(
    legacy_supply_migration_report* report)
{
    while ((player_carried_light_count_for_sval(SV_LIGHT_LESSER_JEWEL)
            + player_carried_light_count_for_sval(SV_LIGHT_FEANORIAN))
        > PLAYER_PERMANENT_LIGHT_CAP)
    {
        bool removed = false;

        for (int i = INVEN_PACK - 1; i >= 0; i--)
        {
            object_type* o_ptr = &inventory[i];
            object_type drop;

            if (!legacy_pack_permanent_light_matches(o_ptr))
                continue;

            object_copy(&drop, o_ptr);
            drop.number = 1;
            inven_item_increase(i, -1);
            inven_item_optimize(i);
            legacy_supply_drop_object(&drop, report);
            removed = true;
            break;
        }

        if (!removed)
        {
            log_warn("Legacy light migration could not trim excess permanent lights");
            break;
        }
    }
}

typedef struct oil_container_unit
{
    object_type obj;
    bool keep;
} oil_container_unit;

typedef struct oil_container_unit_list
{
    oil_container_unit* units;
    int count;
    int capacity;
} oil_container_unit_list;

static int oil_container_saved_fuel(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return 0;

    if (o_ptr->tval == TV_LIGHT && o_ptr->sval == SV_LIGHT_LANTERN)
        return MIN(o_ptr->timeout, FUEL_LAMP);
    if (o_ptr->tval == TV_FLASK)
        return MIN(o_ptr->pval, FUEL_FLASK);

    return 0;
}

static bool oil_container_is_lamp(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx && o_ptr->tval == TV_LIGHT
        && o_ptr->sval == SV_LIGHT_LANTERN;
}

static bool oil_container_is_flask(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx && o_ptr->tval == TV_FLASK;
}

static void oil_container_units_reserve(oil_container_unit_list* list,
    int minimum)
{
    int new_capacity;
    oil_container_unit* new_units;

    if (!list || list->capacity >= minimum)
        return;

    new_capacity = (list->capacity > 0) ? list->capacity : 8;
    while (new_capacity < minimum)
        new_capacity *= 2;

    new_units = mem_alloc_array(new_capacity, oil_container_unit);
    if (list->units && list->count > 0)
    {
        memcpy(new_units, list->units, list->count * sizeof(oil_container_unit));
        mem_free_null(list->units);
    }

    list->units = new_units;
    list->capacity = new_capacity;
}

static void oil_container_units_add(oil_container_unit_list* list,
    const object_type* src, int amount)
{
    if (!list || !src || !src->k_idx || amount <= 0)
        return;

    oil_container_units_reserve(list, list->count + amount);
    for (int i = 0; i < amount; i++)
    {
        object_copy(&list->units[list->count].obj, src);
        list->units[list->count].obj.number = 1;
        list->units[list->count].keep = false;
        list->count++;
    }
}

static void oil_container_report_source(
    legacy_supply_migration_report* report, const object_type* o_ptr, int amount)
{
    if (!report || !o_ptr || amount <= 0)
        return;

    if (oil_container_is_lamp(o_ptr))
        report->lanterns_drained += amount;
    else if (oil_container_is_flask(o_ptr))
        report->flasks_converted += amount;
}

static void oil_container_drop_unit(object_type* unit, int* excess_oil,
    legacy_supply_migration_report* report)
{
    int fuel = 0;
    int capacity;

    if (!unit || !unit->k_idx)
        return;

    capacity = player_oil_container_unit_capacity(unit);
    if (excess_oil && *excess_oil > 0 && capacity > 0)
    {
        fuel = MIN(*excess_oil, capacity);
        *excess_oil -= fuel;
    }

    player_oil_container_set_fuel(unit, fuel);
    legacy_supply_drop_object(unit, report);
}

static void migrate_oil_container_system(
    legacy_supply_migration_report* report)
{
    oil_container_unit_list list;
    object_type* lite;
    bool keep_equipped_lamp = false;
    int total_oil;
    int used_slots = 0;
    int stored_oil;
    int excess_oil;

    if (!report)
        return;

    memset(report, 0, sizeof(*report));
    memset(&list, 0, sizeof(list));

    total_oil = (p_ptr && p_ptr->lamp_oil > 0) ? p_ptr->lamp_oil : 0;

    lite = &inventory[INVEN_LITE];
    if (player_oil_container_object(lite))
    {
        int amount = MAX(lite->number, 1);
        int fuel = oil_container_saved_fuel(lite);

        if (fuel > 0)
        {
            total_oil += fuel * amount;
            oil_container_report_source(report, lite, amount);
        }

        keep_equipped_lamp = true;
        used_slots += player_oil_container_slot_cost(lite) * amount;
        player_oil_container_set_fuel(lite, 0);
    }

    for (int i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];
        int amount;
        int fuel;

        if (!player_oil_container_object(o_ptr))
            continue;

        amount = MAX(o_ptr->number, 1);
        fuel = oil_container_saved_fuel(o_ptr);
        if (fuel > 0)
        {
            total_oil += fuel * amount;
            oil_container_report_source(report, o_ptr, amount);
        }

        oil_container_units_add(&list, o_ptr, amount);
        inven_item_increase(i, -o_ptr->number);
        inven_item_optimize(i);
        i--;
    }

    for (int i = 0; i < supplies_entry_count(); i++)
    {
        object_type* s_ptr = supplies_entry_at(i);
        int amount;
        int fuel;

        if (!player_oil_container_object(s_ptr))
            continue;

        amount = MAX(s_ptr->number, 1);
        fuel = oil_container_saved_fuel(s_ptr);
        if (fuel > 0)
        {
            total_oil += fuel * amount;
            oil_container_report_source(report, s_ptr, amount);
        }

        oil_container_units_add(&list, s_ptr, amount);
        supplies_consume_quantity(i, amount);
        i--;
    }

    for (int pass = 0; pass < 2; pass++)
    {
        bool want_lamps = (pass == 0);

        for (int i = 0; i < list.count; i++)
        {
            object_type* unit = &list.units[i].obj;
            int cost;

            if (want_lamps != oil_container_is_lamp(unit))
                continue;

            cost = player_oil_container_slot_cost(unit);
            if (cost <= 0)
                continue;

            if (used_slots + cost > PLAYER_OIL_CONTAINER_SLOT_CAP)
                continue;

            list.units[i].keep = true;
            used_slots += cost;
        }
    }

    if (p_ptr)
        p_ptr->lamp_oil = 0;

    if (keep_equipped_lamp)
        player_oil_container_set_fuel(lite, 0);

    for (int i = 0; i < list.count; i++)
    {
        object_type keep;

        if (!list.units[i].keep)
            continue;

        object_copy(&keep, &list.units[i].obj);
        keep.number = 1;
        player_oil_container_set_fuel(&keep, 0);
        if (supplies_absorb_object(&keep))
        {
            legacy_supply_record_counts(report->moved, &list.units[i].obj, 1);
        }
        else
        {
            list.units[i].keep = false;
        }
    }

    stored_oil = MIN(total_oil, player_lamp_oil_capacity());
    player_set_lamp_oil(stored_oil);
    report->oil_added = stored_oil;
    excess_oil = total_oil - stored_oil;
    if (excess_oil < 0)
        excess_oil = 0;

    for (int i = 0; i < list.count; i++)
    {
        object_type drop;

        if (list.units[i].keep)
            continue;

        object_copy(&drop, &list.units[i].obj);
        drop.number = 1;
        oil_container_drop_unit(&drop, &excess_oil, report);
    }

    if (excess_oil > 0)
        report->oil_lost += excess_oil;

    if (list.units)
        mem_free_null(list.units);
}

static void migrate_legacy_supply_system(
    legacy_supply_migration_report* report)
{
    object_type detached[INVEN_PACK];
    int detached_count = 0;
    object_type* lite;

    if (!report)
        return;

    memset(report, 0, sizeof(*report));

    lite = &inventory[INVEN_LITE];
    if (lite->k_idx && lite->tval == TV_LIGHT
        && lite->sval == SV_LIGHT_LANTERN && lite->timeout > 0)
    {
        int units = MAX(lite->number, 1);
        report->lanterns_drained += units;
        legacy_supply_add_oil(report, lite->timeout * units);
        lite->timeout = 0;
    }

    for (int i = 0; i < supplies_entry_count(); i++)
    {
        object_type* supply = supplies_entry_at(i);
        if (!supply || !supply->k_idx)
            continue;
        if (supply->tval != TV_LIGHT || supply->sval != SV_LIGHT_LANTERN
            || supply->timeout <= 0)
        {
            continue;
        }

        report->lanterns_drained += MAX(supply->number, 1);
        legacy_supply_add_oil(report, supply->timeout * MAX(supply->number, 1));
        supply->timeout = 0;
    }

    for (int i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx)
            continue;
        if (!supplies_is_supply_object(o_ptr))
            continue;
        if (player_oil_container_object(o_ptr))
            continue;

        if (detached_count < INVEN_PACK)
        {
            object_copy(&detached[detached_count], o_ptr);
            detached_count++;
        }

        inven_item_increase(i, -o_ptr->number);
        inven_item_optimize(i);
        i--;
    }

    for (int i = 0; i < detached_count; i++)
    {
        object_type* src = &detached[i];

        if (!src->k_idx || src->number <= 0)
            continue;

        if (!supplies_is_supply_object(src))
            continue;

        for (int n = 0; n < src->number; n++)
        {
            object_type unit;

            object_copy(&unit, src);
            unit.number = 1;

            int source_oil = 0;
            if (unit.tval == TV_LIGHT && unit.sval == SV_LIGHT_LANTERN)
            {
                report->lanterns_drained++;
                source_oil = unit.timeout;
            }
            else if (unit.tval == TV_FLASK)
            {
                report->flasks_converted++;
                source_oil = unit.pval;
            }

            int before_oil = player_lamp_oil();
            if (supplies_absorb_object(&unit))
            {
                int gained = player_lamp_oil() - before_oil;
                if (gained < 0)
                    gained = 0;
                if (gained > source_oil)
                    gained = source_oil;
                report->oil_added += gained;
                report->oil_lost += source_oil - gained;
                legacy_supply_record_counts(report->moved, src, 1);
            }
            else
            {
                legacy_supply_drop_object(&unit, report);
            }
        }
    }

    legacy_supply_trim_pack_permanent_lights(report);
}

static void queue_legacy_supply_migration_messages(
    const legacy_supply_migration_report* report)
{
    char buf[256];
    char sources[128];
    bool first = true;

    if (!report)
        return;

    message_add("Your save was converted to the 0.9.6 supply system.",
        MSG_GENERIC);

    if (legacy_supply_counts_present(report->moved))
    {
        SDL_strlcpy(buf, "Moved to supplies: ", sizeof(buf));
        first = true;
        for (int i = 0; i < LEGACY_SUPPLY_KIND_MAX; i++)
            legacy_supply_append_count(buf, sizeof(buf), &first,
                report->moved[i], i);
        message_add(buf, MSG_GENERIC);
    }

    if (report->oil_added > 0)
    {
        sources[0] = '\0';
        first = true;
        legacy_supply_append_count(sources, sizeof(sources), &first,
            report->lanterns_drained, LEGACY_SUPPLY_KIND_BRASS_LANTERNS);
        legacy_supply_append_count(sources, sizeof(sources), &first,
            report->flasks_converted, LEGACY_SUPPLY_KIND_OIL_FLASKS);

        if (sources[0] != '\0')
        {
            strnfmt(buf, sizeof(buf), "Lamp oil pooled: %d turns from %s.",
                report->oil_added, sources);
        }
        else
        {
            strnfmt(buf, sizeof(buf), "Lamp oil pooled: %d turns added.",
                report->oil_added);
        }
        message_add(buf, MSG_GENERIC);
    }

    if (report->oil_lost > 0)
    {
        strnfmt(buf, sizeof(buf),
            "%d turns of excess lamp oil could not be stored.",
            report->oil_lost);
        message_add(buf, MSG_GENERIC);
    }

    if (legacy_supply_counts_present(report->dropped))
    {
        SDL_strlcpy(buf, "Dropped nearby: ", sizeof(buf));
        first = true;
        for (int i = 0; i < LEGACY_SUPPLY_KIND_MAX; i++)
            legacy_supply_append_count(buf, sizeof(buf), &first,
                report->dropped[i], i);
        message_add(buf, MSG_GENERIC);
    }

    if (legacy_supply_counts_present(report->discarded))
    {
        SDL_strlcpy(buf, "Discarded as spent: ", sizeof(buf));
        first = true;
        for (int i = 0; i < LEGACY_SUPPLY_KIND_MAX; i++)
            legacy_supply_append_count(buf, sizeof(buf), &first,
                report->discarded[i], i);
        message_add(buf, MSG_GENERIC);
    }
}

static void queue_oil_container_migration_messages(
    const legacy_supply_migration_report* report)
{
    char buf[256];
    bool first = true;

    if (!legacy_supply_migration_changed(report))
        return;

    message_add("Your save was converted to the 0.9.6.2 oil system.",
        MSG_GENERIC);

    if (legacy_supply_counts_present(report->moved))
    {
        SDL_strlcpy(buf, "Oil containers kept: ", sizeof(buf));
        first = true;
        legacy_supply_append_count(buf, sizeof(buf), &first,
            report->moved[LEGACY_SUPPLY_KIND_BRASS_LANTERNS],
            LEGACY_SUPPLY_KIND_BRASS_LANTERNS);
        legacy_supply_append_count(buf, sizeof(buf), &first,
            report->moved[LEGACY_SUPPLY_KIND_OIL_FLASKS],
            LEGACY_SUPPLY_KIND_OIL_FLASKS);
        message_add(buf, MSG_GENERIC);
    }

    if (report->oil_added > 0)
    {
        strnfmt(buf, sizeof(buf), "Lamp oil stored: %d/%d turns.",
            player_lamp_oil(), player_lamp_oil_capacity());
        message_add(buf, MSG_GENERIC);
    }

    if (legacy_supply_counts_present(report->dropped))
    {
        SDL_strlcpy(buf, "Oil containers dropped: ", sizeof(buf));
        first = true;
        legacy_supply_append_count(buf, sizeof(buf), &first,
            report->dropped[LEGACY_SUPPLY_KIND_BRASS_LANTERNS],
            LEGACY_SUPPLY_KIND_BRASS_LANTERNS);
        legacy_supply_append_count(buf, sizeof(buf), &first,
            report->dropped[LEGACY_SUPPLY_KIND_OIL_FLASKS],
            LEGACY_SUPPLY_KIND_OIL_FLASKS);
        message_add(buf, MSG_GENERIC);
    }

    if (report->oil_lost > 0)
    {
        strnfmt(buf, sizeof(buf), "%d turns of excess oil could not be stored.",
            report->oil_lost);
        message_add(buf, MSG_GENERIC);
    }
}

/*
 * Read the player inventory (and the smithing object)
 *
 * Note that the inventory is "re-sorted" later by "dungeon()".
 */
static errr rd_inventory(void)
{
    int slot = 0;

    object_type* i_ptr;
    object_type object_type_body;

    log_debug("Loading smithing object and player inventory");
    log_trace("[load:%06u] === BEGIN SMITHING ITEM ===", (unsigned)load_byte_offset);

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

    /* Success */
    return (0);
}

/*
 * Read the saved messages
 */
static void rd_messages(void)
{
    int i;
    char buf[128];
    u16b tmp16u;

    s16b num;

    /* Total */
    rd_s16b(&num);
    log_debug("Loading %d message history entries", num);

    /* Read the messages */
    for (i = 0; i < num; i++)
    {
        /* Read the message */
        rd_string(buf, sizeof(buf));

        /* Read the message type */
        rd_u16b(&tmp16u);

        /* Save the message */
        message_add(buf, tmp16u);
    }
}

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
static errr rd_dungeon(void)
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

            /* Rearrange stack if needed */
            rearrange_stack(y, x);
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

/*
 * Actually read the savefile
 */
static errr rd_savefile_new_aux(void)
{
    int i;

    byte tmp8u;
    u16b tmp16u;

    u32b n_x_check, n_v_check;
    u32b o_x_check, o_v_check;

    savefile_has_runtime_overrides = savefile_version_at_least(0, 9, 0, 3);
    savefile_has_monster_shatter = savefile_version_at_least(0, 9, 0, 4);
    savefile_has_song_duels = savefile_version_at_least(0, 9, 0, 5);
    savefile_has_ability_timeline = savefile_version_at_least(0, 9, 1, 1);
    savefile_has_artifact_seen = savefile_version_at_least(0, 9, 1, 4);
    savefile_has_skeleton_notes = savefile_version_at_least(0, 9, 1, 5);
    savefile_has_skeleton_hint_mask = savefile_version_at_least(0, 9, 1, 6);
    savefile_has_skeleton_hint_mask32 = savefile_version_at_least(0, 9, 1, 13);
    savefile_has_partition_meta = savefile_version_at_least(0, 9, 1, 7);
    savefile_has_partition_meta_types = savefile_version_at_least(0, 9, 1, 9);
    savefile_has_cave_info_hi = savefile_version_at_least(0, 9, 1, 8);
    savefile_has_hint_messages = savefile_version_at_least(0, 9, 1, 10);
    savefile_has_hint_message_meta = savefile_version_at_least(0, 9, 5, 7);
    savefile_has_thrall_quest = savefile_version_at_least(0, 9, 1, 11);
    savefile_has_thrall_quest_requested = savefile_version_at_least(0, 9, 1, 12);
    savefile_has_randart_flags4 = savefile_version_at_least(0, 9, 5, 1);
    savefile_has_item_bonuses = savefile_version_at_least(0, 9, 5, 2);
    savefile_has_randart_bonuses = savefile_version_at_least(0, 9, 5, 3);
    savefile_has_morgoth_call_state = savefile_version_at_least(0, 9, 6, 4);

    /* Reset load byte offset counter */
    load_byte_offset = 0;
    log_trace("=== LOAD: Reset byte offset counter ===");

    /* Mention the savefile version */
    note(
        format("Loading a %d.%d.%d savefile...", sf_major, sf_minor, sf_patch));

    /* Strip the version bytes */
    strip_bytes(4);

    /* Hack -- decrypt */
    xor_byte = sf_extra;

    /* Clear the checksums */
    v_check = 0L;
    x_check = 0L;

    /* Operating system info */
    rd_u32b(&sf_xtra);

    /* Time of savefile creation */
    rd_u32b(&sf_when);

    /* Number of resurrections */
    rd_u16b(&sf_lives);

    /* Number of times played */
    rd_u16b(&sf_saves);

    // 8 spare bytes
    strip_bytes(8);

    /* Read RNG state */
    rd_randomizer();
    if (arg_fiddle)
        note("Loaded Randomizer Info");

    /* Then the options */
    rd_options();
    if (arg_fiddle)
        note("Loaded Option Flags");

    /* Then the "messages" */
    rd_messages();
    if (arg_fiddle)
        note("Loaded Messages");

    /* Monster Memory */
    rd_u16b(&tmp16u);
    log_debug("Loading %d monster race records", tmp16u);

    /* Incompatible save files */
    if (tmp16u > z_info->r_max)
    {
        note(format("Too many (%u) monster races!", tmp16u));
        return (-1);
    }

    /* Read the available records */
    for (i = 0; i < tmp16u; i++)
    {
        /* Read the lore */
        rd_lore(i);
    }
    restore_monster_races_from_base();

    if (savefile_has_runtime_overrides)
    {
        rd_monster_runtime_overrides();
    }
    if (arg_fiddle)
        note("Loaded Monster Memory");

    /* Object Memory */
    rd_u16b(&tmp16u);
    log_debug("Loading %d object kind records", tmp16u);

    /* Incompatible save files */
    if (tmp16u > z_info->k_max)
    {
        note(format("Too many (%u) object kinds!", tmp16u));
        return (-1);
    }

    /* Read the object memory */
    for (i = 0; i < tmp16u; i++)
    {
        byte tmp8u;

        object_kind* k_ptr = &k_info[i];

        rd_byte(&tmp8u);

        k_ptr->aware = (tmp8u & 0x01) ? true : false;
        k_ptr->tried = (tmp8u & 0x02) ? true : false;
        k_ptr->everseen = (tmp8u & 0x08) ? true : false;

        rd_byte(&k_ptr->squelch);
    }
    if (arg_fiddle)
        note("Loaded Object Memory");

    /* Load the Artefacts */
    rd_u16b(&tmp16u);
    log_debug("Loading %d artefact records", tmp16u);

    /* Incompatible save files */
    if (tmp16u > z_info->art_max)
    {
        note(format("Too many (%u) artefacts!", tmp16u));
        return (-1);
    }

    /* Read the artefact flags */
    for (i = 0; i < tmp16u; i++)
    {
        rd_byte(&tmp8u);
        a_info[i].cur_num = tmp8u;
        rd_byte(&tmp8u);
        a_info[i].found_num = tmp8u;
        if (savefile_has_artifact_seen)
        {
            rd_byte(&tmp8u);
            a_info[i].seen = tmp8u;
        }
        else
        {
            /* Older saves don't have seen field - default to 0 */
            a_info[i].seen = 0;
        }
    }
    if (arg_fiddle)
        note("Loaded Artefacts");

    /* Read the extra stuff */
    log_debug("Loading extra player information");
    if (rd_extra())
        return (-1);
    if (arg_fiddle)
        note("Loaded extra information");

    log_debug("Loading random artefacts");
    if (rd_randarts())
        return (-1);
    if (arg_fiddle)
        note("Loaded Random Artefacts");

    log_debug("Loading notes");
    if (rd_notes())
        return (-1);
    if (arg_fiddle)
        note("Loaded Notes");

    /* Important -- Initialize the race/character */
    rp_ptr = &p_info[p_ptr->prace];
    current_character_profile = &c_info[p_ptr->pcharacter];

    /* Read the inventory */
    log_debug("Loading player inventory");
    if (rd_inventory())
    {
        note("Unable to read inventory");
        return (-1);
    }

    /* I'm not dead yet... */
    if (!p_ptr->is_dead)
    {
        /* Dead players have no dungeon */
        note("Restoring Dungeon...");
        log_debug("Loading dungeon data");
        if (rd_dungeon())
        {
            note("Error reading dungeon data");
            return (-1);
        }
    }

    /* Save the checksum */
    n_v_check = v_check;

    /* Read the old checksum */
    rd_u32b(&o_v_check);

    log_debug("Checksum validation: expected=%u, file=%u", n_v_check, o_v_check);

    /* Verify */
    if (o_v_check != n_v_check)
    {
        log_error("Invalid checksum: expected %u, got %u", n_v_check, o_v_check);
        note("Invalid checksum");
        return (-1);
    }

    /* Save the encoded checksum */
    n_x_check = x_check;

    /* Read the checksum */
    rd_u32b(&o_x_check);

    log_debug("Encoded checksum validation: expected=%u, file=%u", n_x_check, o_x_check);

    /* Verify */
    if (o_x_check != n_x_check)
    {
        log_error("Invalid encoded checksum: expected %u, got %u", n_x_check, o_x_check);
        note("Invalid encoded checksum");
        return (-1);
    }

    /* Success */
    return (0);
}

/*
 * Actually read the savefile
 */
static errr rd_savefile(void)
{
    errr err;

    log_debug("Opening savefile for reading");

    /* Grab permissions */
    safe_setuid_grab();

    /* The savefile is a binary file */
    fff = sdl_fopen(savefile, "rb");

    /* Drop permissions */
    safe_setuid_drop();

    /* Paranoia */
    if (!fff)
    {
        log_error("Failed to open savefile: %s", savefile);
        return (-1);
    }

    /* Call the sub-function */
    err = rd_savefile_new_aux();
    log_debug("rd_savefile_new_aux returned: %d", err);

    /* Note: SDL doesn't have ferror equivalent - errors are caught during read operations */
    
    /* Close the file */
    sdl_fclose(fff);
    log_debug("Savefile closed");

    /* Result */
    return (err);
}

/*
 * Attempt to Load a "savefile"
 *
 * On multi-user systems, you may only "read" a savefile if you will be
 * allowed to "write" it later, this prevents painful situations in which
 * the player loads a savefile belonging to someone else, and then is not
 * allowed to save his game when he quits.
 *
 * We return "true" if the savefile was usable, and we set the global
 * flag "character_loaded" if a real, living, character was loaded.
 *
 * Note that we always try to load the "current" savefile, even if
 * there is no such file, so we must check for "empty" savefile names.
 */
bool load_player(void)
{
    SDL_IOStream* fd = NULL;

    errr err = 0;

    byte vvv[4];

#ifdef VERIFY_TIMESTAMP
    struct stat statbuf;
#endif /* VERIFY_TIMESTAMP */

    cptr what = "generic";

    log_debug("Loading savefile '%s'", savefile);
    load_note_screen_enabled = arg_fiddle;

    /* Paranoia */
    turn = 0;

    /* Paranoia */
    p_ptr->is_dead = false;
    killer_reset();

    // Set a flag to show that we are restoring a game
    p_ptr->restoring = true;

    /* Allow empty savefile name */
    if (!savefile[0])
        return (true);

    /* Grab permissions */
    safe_setuid_grab();

    /* Open the savefile */
    fd = sdl_fopen(savefile, "rb");

    /* Drop permissions */
    safe_setuid_drop();

    /* No file */
    if (!fd)
    {
        /* Give a message */
        // msg_format("Savefile \"%s\" does not exist.", savefile);
        // message_flush();

        /* Allow this */
        log_debug("Savefile '%s' does not exist", savefile);
        p_ptr->restoring = false;
        return (false);
    }

    log_debug("Savefile exists, proceeding with load");

    /* Close the file */
    sdl_fclose(fd);

#ifdef VERIFY_SAVEFILE

    /* Verify savefile usage */
    if (!err)
    {
        SDL_IOStream* fkk;

        char temp[1024];

        /* Extract name of lock file */
        SDL_strlcpy(temp, savefile, sizeof(temp));
        SDL_strlcat(temp, ".lok", sizeof(temp));

        /* Grab permissions */
        safe_setuid_grab();

        /* Check for lock */
        fkk = sdl_fopen(temp, "r");

        /* Drop permissions */
        safe_setuid_drop();

        /* Oops, lock exists */
        if (fkk)
        {
            /* Close the file */
            sdl_fclose(fkk);

            /* Message */
            msg_print("Savefile is currently in use.");
            message_flush();

            /* Oops */
            return (false);
        }

        /* Grab permissions */
        safe_setuid_grab();

        /* Create a lock file */
        fkk = sdl_fopen(temp, "w");

        /* Drop permissions */
        safe_setuid_drop();

        /* Dump a line of info */
        fprintf(fkk, "Lock file for savefile '%s'\n", savefile);

        /* Close the lock file */
        sdl_fclose(fkk);
    }

#endif /* VERIFY_SAVEFILE */

    /* Okay */
    if (!err)
    {
        /* Grab permissions */
        safe_setuid_grab();

        /* Open the savefile */
        fd = sdl_fopen(savefile, "rb");

        /* Drop permissions */
        safe_setuid_drop();

        /* No file */
        if (!fd)
            err = -1;

        /* Message (below) */
        if (err)
            what = "Cannot open savefile";
    }

    /* Process file */
    if (!err)
    {
#ifdef VERIFY_TIMESTAMP
        /* Note: fstat requires integer file descriptor, not available with SDL_IOStream */
        /* Timestamp verification disabled for SDL builds */
        log_debug("Timestamp verification skipped (not supported with SDL_IOStream)");
#endif /* VERIFY_TIMESTAMP */

        /* Read the first four bytes */
        if (sdl_read(fd, (char*)(vvv), sizeof(vvv)))
            err = -1;

        /* What */
        if (err)
            what = "Cannot read savefile";

        /* Close the file */
        sdl_fclose(fd);
    }

    /* Process file */
    if (!err)
    {
        /* Extract version */
        sf_major = vvv[0];
        sf_minor = vvv[1];
        sf_patch = vvv[2];
        sf_extra = vvv[3];
        log_debug("Version bytes read: %u.%u.%u extra=%u", (unsigned)sf_major, (unsigned)sf_minor, (unsigned)sf_patch, (unsigned)sf_extra);

        if (!savefile_version_supported())
        {
            err = -1;
            what = "Incompatible savefile version";
            log_error("Savefile version %u.%u.%u extra=%u is outside supported range [%u.%u.%u extra>=0 .. %u.%u.%u extra<=%u] (current release requires extra >= %u)",
                (unsigned)sf_major, (unsigned)sf_minor, (unsigned)sf_patch, (unsigned)sf_extra,
                (unsigned)OLD_VERSION_MAJOR, (unsigned)OLD_VERSION_MINOR, (unsigned)OLD_VERSION_PATCH,
                (unsigned)VERSION_MAJOR, (unsigned)VERSION_MINOR, (unsigned)VERSION_PATCH, (unsigned)VERSION_EXTRA,
                (unsigned)MIN_VERSION_EXTRA);
        }
        else
        {
            savefile_has_runtime_overrides = savefile_version_at_least(0, 9, 0, 3);
            savefile_has_monster_shatter = savefile_version_at_least(0, 9, 0, 4);
            savefile_has_song_duels = savefile_version_at_least(0, 9, 0, 5);
            savefile_has_ability_timeline = savefile_version_at_least(0, 9, 1, 1);
            savefile_has_varda_quest = savefile_version_at_least(0, 9, 1, 3);
            savefile_has_artifact_seen = savefile_version_at_least(0, 9, 1, 4);
            savefile_has_skeleton_notes = savefile_version_at_least(0, 9, 1, 5);
            savefile_has_skeleton_hint_mask = savefile_version_at_least(0, 9, 1, 6);
            savefile_has_skeleton_hint_mask32 = savefile_version_at_least(0, 9, 1, 13);
            savefile_has_partition_meta = savefile_version_at_least(0, 9, 1, 7);
            savefile_has_partition_meta_types = savefile_version_at_least(0, 9, 1, 9);
            savefile_has_cave_info_hi = savefile_version_at_least(0, 9, 1, 8);
            savefile_has_hint_messages = savefile_version_at_least(0, 9, 1, 10);
            savefile_has_hint_message_meta = savefile_version_at_least(0, 9, 5, 7);
            savefile_has_thrall_quest = savefile_version_at_least(0, 9, 1, 11);
            savefile_has_thrall_quest_requested = savefile_version_at_least(0, 9, 1, 12);
            savefile_has_randart_flags4 = savefile_version_at_least(0, 9, 5, 1);
            savefile_has_item_bonuses = savefile_version_at_least(0, 9, 5, 2);
            savefile_has_randart_bonuses = savefile_version_at_least(0, 9, 5, 3);
            savefile_has_morgoth_call_state = savefile_version_at_least(0, 9, 6, 4);
        }

        load_byte_offset = 0; /* reset counter before decoding stream */

        /* Keep the current startup screen visible until the delayed
         * overlay is actually ready to show. Otherwise the unconditional clear
         * erases the very frame we want to reuse. */
        if (startup_loading_overlay_armed)
            maybe_show_startup_loading_overlay();
        else
            Term_clear();

        if (!err)
        {
            /* Attempt to load */
            err = rd_savefile();
            if (err) {
                log_error("Read savefile failed");
            } else {
                log_debug("Read savefile success");
            }
            if (!err) {
                log_debug("load: post-read flags (is_dead=%d, wizard=%d, noscore=0x%04X)",
                         p_ptr->is_dead, p_ptr->wizard ? 1 : 0, (unsigned)p_ptr->noscore);
            }

            /* Message (below) */
            if (err)
                what = "Cannot parse savefile";
        }
    }

    /* Paranoia */
    if (!err)
    {
        /* Invalid turn */
        if (!turn)
            err = -1;

        /* Message (below) */
        if (err)
            what = "Broken savefile";
    }

#ifdef VERIFY_TIMESTAMP
    /* Verify timestamp */
    if (!err && !arg_wizard)
    {
        /* Hack -- Verify the timestamp */
        if (sf_when > (statbuf.st_ctime + 100)
            || sf_when < (statbuf.st_ctime - 100))
        {
            /* Message */
            what = "Invalid timestamp";

            /* Oops */
            err = -1;
        }
    }
#endif /* VERIFY_TIMESTAMP */

    /* Okay */
    if (!err)
    {
        /* App-wide settings live in the SDL JSON config, so they must win over
         * any copies serialized in the savefile. */
        sdl_config_load_app_options(get_sdl_config_path());

        // if Morgoth has lost his crown...
        if ((&a_info[ART_MORGOTH_3])->cur_num == 1)
        {
            // lower Morgoth's protection, remove his light source, increase his
            // will and perception
            (&r_info[R_IDX_MORGOTH])->pd -= 1;
            (&r_info[R_IDX_MORGOTH])->light = 0;
            (&r_info[R_IDX_MORGOTH])->wil += 5;
            (&r_info[R_IDX_MORGOTH])->per += 5;
        }

        /* Player is dead */
        if (p_ptr->is_dead)
        {
            log_info("Loading a dead character");
            /* Cheat death (unless the character retired) */
            if (arg_wizard)
            {
                log_info("Wizard mode: resurrecting dead character");
                /*heal the player*/
                hp_player(100, true, true);

                /* Forget death */
                p_ptr->is_dead = false;

                /* A character was loaded */
                character_loaded = true;

                // put the character somewhere sensible
                p_ptr->depth = min_depth();

                // Mark savefile
                p_ptr->noscore |= 0x0001;

                /* Done */
                return (true);
            }

            /* Forget death */
            p_ptr->is_dead = false;

            /* Count lives */
            sf_lives++;

            /* Forget turns */
            turn = 0;
            playerturn = 0;

            /* A dead character was loaded */
            character_loaded_dead = true;
            log_info("Character loaded dead");

            /* Done */
            return (false);
        }

        /* A character was loaded */
        character_loaded = true;
        log_info("%s", character_loaded ? "Character loaded" : "Character not loaded");

        /* Still alive */
        if (p_ptr->chp >= 0)
        {
            /* Reset cause of death */
            SDL_strlcpy(
                p_ptr->died_from, "(alive and well)", sizeof(p_ptr->died_from));
        }

        // count the artefacts seen for the player
        p_ptr->artefacts = artefact_count();
        log_debug("Character has seen %d artefacts", p_ptr->artefacts);

        /* Process player name to update base_name and savefile path */
        process_player_name(true);
        log_debug("Processed player name after load: base_name='%s', savefile='%s'", 
                 op_ptr->base_name, savefile);

        /* Reapply Morgoth's anger state to the r_info template */
        if (p_ptr->morgoth_state > 0)
        {
            log_debug("load: reapplying morgoth_state %d to r_info template", 
                     p_ptr->morgoth_state);
            
            /* Save current state, then reset to 0 and reapply */
            s16b saved_state = p_ptr->morgoth_state;
            p_ptr->morgoth_state = 0;
            anger_morgoth(saved_state);
        }
        else
        {
            log_debug("load: morgoth_state is 0, no reapplication needed");
        }

        if (!savefile_version_at_least(0, 9, 6, 0))
        {
            legacy_supply_migration_report migration_report;

            migrate_legacy_supply_system(&migration_report);
            if (legacy_supply_migration_changed(&migration_report))
            {
                p_ptr->notice |= (PN_COMBINE | PN_REORDER);
                p_ptr->update |= (PU_BONUS);
                p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
            }
            queue_legacy_supply_migration_messages(&migration_report);
        }

        if (!savefile_version_at_least(0, 9, 6, 2))
        {
            legacy_supply_migration_report oil_report;

            migrate_oil_container_system(&oil_report);
            if (legacy_supply_migration_changed(&oil_report))
            {
                p_ptr->notice |= (PN_COMBINE | PN_REORDER);
                p_ptr->update |= (PU_BONUS);
                p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
            }
            queue_oil_container_migration_messages(&oil_report);
        }

        /* Success */
        return (true);
    }

#ifdef VERIFY_SAVEFILE

    /* Verify savefile usage */
    if (true)
    {
        char temp[1024];

        /* Extract name of lock file */
        SDL_strlcpy(temp, savefile, sizeof(temp));
        SDL_strlcat(temp, ".lok", sizeof(temp));

        /* Grab permissions */
        safe_setuid_grab();

        /* Remove lock */
        fd_kill(temp);

        /* Drop permissions */
        safe_setuid_drop();
    }

#endif /* VERIFY_SAVEFILE */

    /* Message */
    if (load_note_screen_enabled)
    {
        msg_format("Error (%s) reading %d.%d.%d savefile.", what, sf_major,
            sf_minor, sf_patch);
        message_flush();
    }
    else
    {
        log_warn("Error (%s) reading %d.%d.%d savefile.", what, sf_major,
            sf_minor, sf_patch);
    }

    /* Oops */
    return (false);
}
