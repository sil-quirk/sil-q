/* File: fs/load-player.c -- carved from load.c (shares state via fs/load-internal.h) */

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
 * Read the "extra" information
 */
errr rd_extra(void)
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

    // Compatibility block: persistent fields stored in 15 legacy reserved bytes.
    {
        byte morgoth_hall_entered = 0;
        byte morgoth_second_wind = 0;
        byte discovery_lore_flags = 0;
        byte quick_access_prompt_flags = 0;
        s16b lamp_oil = 0;
        byte active_weapon_mode = PLAYER_ACTIVE_WEAPON_MELEE;
        byte morgoth_call_state = 0;
        rd_byte(&morgoth_hall_entered);
        rd_byte(&morgoth_second_wind);
        rd_byte(&discovery_lore_flags);
        rd_s16b(&lamp_oil);
        if (savefile_version_at_least(0, 9, 7, 1))
        {
            rd_byte(&active_weapon_mode);
        }
        else
        {
            active_weapon_mode = PLAYER_ACTIVE_WEAPON_MELEE;
            strip_bytes(1);
        }
        if (savefile_version_at_least(0, 9, 7, 5))
            rd_byte(&quick_access_prompt_flags);
        else
            strip_bytes(1);
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
        p_ptr->quick_access_prompt_flags =
            quick_access_prompt_flags & QUICK_ACCESS_PROMPT_MASK;
        p_ptr->lamp_oil = lamp_oil;
        p_ptr->active_weapon_mode = active_weapon_mode;
        if (savefile_has_morgoth_call_state)
        {
            p_ptr->morgoth_call_state =
                morgoth_call_state
                & (SAVEFILE_MORGOTH_CALL_SEEN
                    | SAVEFILE_MORGOTH_CALL_ESCALATION_MASK);
        }
    }

    /* Reserved: legacy item-quality squelch array (now unused) */
    {
        byte legacy_squelch;
        for (i = 0; i < LEGACY_ITEM_QUALITY_BYTES; i++)
            rd_byte(&legacy_squelch);
    }

    /* Load the name of the current greater vault */
    rd_string(g_vault_name, sizeof(g_vault_name));

    /* Read the number of saved special item types */
    rd_u16b(&file_e_max);

    /* Read special item "seen"/"aware" flags (legacy squelch bit ignored) */
    for (i = 0; i < z_info->e_max; i++)
    {
        ego_item_type* e_ptr = &e_info[i];

        tmp8u = 0;

        if (i < file_e_max)
            rd_byte(&tmp8u);

        e_ptr->everseen |= (tmp8u & 0x02);
        e_ptr->aware |= (tmp8u & 0x04);
    }

    /* Read possible extra elements */
    while (i < file_e_max)
    {
        rd_byte(&tmp8u);
        i++;
    }

    /* Read the current number of auto-inscriptions */
    {
        u16b saved_inscriptions_count;

        rd_u16b(&saved_inscriptions_count);
        inscriptionsCount = MIN(saved_inscriptions_count,
            (u16b)AUTOINSCRIPTIONS_MAX);
        if (saved_inscriptions_count > AUTOINSCRIPTIONS_MAX)
        {
            log_warn("Savefile contains %u auto-inscriptions; keeping first %u",
                (unsigned)saved_inscriptions_count,
                (unsigned)AUTOINSCRIPTIONS_MAX);
        }

        /* Read the autoinscriptions array */
        for (i = 0; i < saved_inscriptions_count; i++)
        {
            char tmp[80];
            s16b kind_idx;

            rd_s16b(&kind_idx);
            rd_string(tmp, 80);

            if (i < inscriptionsCount)
            {
                inscriptions[i].kindIdx = kind_idx;
                inscriptions[i].inscriptionIdx = quark_add(tmp);
            }
        }
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

    if (p_ptr->quest_reserved[0] > QUEST_MAX_INITIATED_PER_RUN) {
        p_ptr->quest_reserved[0] = QUEST_MAX_INITIATED_PER_RUN;
    }

    /* Older saves may have an active Varda quest without the initiated counter. */
    if (p_ptr->varda_quest >= VARDA_QUEST_ACTIVE && p_ptr->quest_reserved[0] == 0) {
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
        memset(&sn_state, 0, sizeof(sn_state));
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
        if (savefile_has_skeleton_hint_counts)
        {
            for (int i = 0; i < SKEL_HINT_MAX; ++i)
                rd_byte(&sn_state.hint_use_counts[i]);
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
errr rd_randarts(void)
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

