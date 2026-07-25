/* File: fs/save-player.c -- carved from save.c (shares state via fs/save-internal.h) */

#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "fs/save-internal.h"
#include <stdio.h>

/*
 * Write some "extra" info
 */
void wr_extra(void)
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

    // Compatibility block: persistent fields stored in 15 legacy reserved bytes.
    wr_byte(p_ptr->morgoth_hall_entered ? 1 : 0);
    wr_byte(p_ptr->morgoth_second_wind ? 1 : 0);
    wr_byte(p_ptr->discovery_lore_flags);
    wr_s16b(p_ptr->lamp_oil);
    wr_byte((byte)player_active_weapon_mode());
    wr_byte(p_ptr->quick_access_prompt_flags & QUICK_ACCESS_PROMPT_MASK);
    {
        byte morgoth_call_state =
            p_ptr->morgoth_call_state
            & (SAVEFILE_MORGOTH_CALL_SEEN
                | SAVEFILE_MORGOTH_CALL_ESCALATION_MASK);
        wr_byte(morgoth_call_state);
    }
    wr_u32b(0L);
    wr_u16b(0U);
    wr_byte(0);

    /* Reserved: legacy item-quality squelch array (now unused) */
    for (i = 0; i < LEGACY_ITEM_QUALITY_BYTES; i++)
        wr_byte(0);

    /* Store the name of the current greater vault */
    wr_string(g_vault_name);

    /* Save the current number of special item types */
    wr_u16b(z_info->e_max);

    /* Save special item "seen"/"aware" flags (legacy squelch bit unused) */
    for (i = 0; i < z_info->e_max; i++)
    {
        ego_item_type* e_ptr = &e_info[i];
        byte tmp8u = 0;

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
    /* Aulë quest fields */
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
    /* Nienna quest fields */
    wr_byte(p_ptr->niena_quest);
    wr_byte(p_ptr->niena_monsters_seen);
    wr_byte(p_ptr->niena_monsters_killed);
    wr_byte(p_ptr->niena_reserved);
    wr_s16b(p_ptr->niena_level);
    wr_s16b(p_ptr->niena_reserved2);
    /* Oromë quest fields */
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
        wr_u32b(sn_state.hint_used_mask);
        for (i = 0; i < SKEL_HINT_MAX; i++)
            wr_byte(sn_state.hint_use_counts[i]);
        wr_byte(sn_state.seen_count);
        for (i = 0; i < SKELETON_NOTE_SEEN_MAX; i++)
            wr_s16b(sn_state.seen_ids[i]);
    }

    /* Partition generation metadata (grid + per-partition modes) */
    {
        partition_meta_save pm;
        level_partition_meta_get(&pm);
        wr_byte(0x53);
        wr_s16b(pm.grid_rows);
        wr_s16b(pm.grid_cols);
        wr_s16b(pm.partition_count);
        for (i = 0; i < PARTITION_META_MAX; ++i)
            wr_byte(pm.modes[i]);
        for (i = 0; i < PARTITION_META_MAX; ++i)
            wr_byte(pm.big_cave_types[i]);
    }

    /* Hint message log (per-level skeleton note archive) */
    {
        wr_byte(0x54);
        wr_s16b(hint_messages_level_depth_for_save());
        wr_s16b(hint_messages_map_wid_for_save());
        wr_s16b(hint_messages_map_hgt_for_save());

        byte count = hint_messages_count_for_save();
        wr_byte(count);
        for (i = 0; i < count; ++i)
        {
            hint_message_meta meta;
            byte line_count = hint_messages_message_line_count(i);
            wr_byte(line_count);
            for (int li = 0; li < line_count; ++li)
                wr_string(hint_messages_message_line(i, li));

            hint_messages_message_meta(i, &meta);
            wr_s16b(meta.source_y);
            wr_s16b(meta.source_x);
            wr_byte(meta.cue_count);
            for (int cue = 0; cue < HINT_MESSAGE_CUE_MAX; ++cue)
            {
                wr_string(meta.cue_dists[cue]);
                wr_string(meta.cue_dirs[cue]);
            }
        }
    }

    wr_byte(0x55);
    wr_byte((byte)run_mode_current());
    {
        int8_t *stacks = blitz_runtime_curse_stacks();
        u64b seen = *blitz_runtime_curses_seen();
        for (i = 0; i < METAR_CURSE_SLOTS; ++i)
            wr_byte((byte)(stacks ? stacks[i] : 0));
        wr_u32b((u32b)(seen & 0xFFFFFFFFULL));
        wr_u32b((u32b)(seen >> 32));
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
void wr_randarts(void)
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
        wr_u32b(a_ptr->flags4);

        wr_byte(a_ptr->level);
        wr_byte(a_ptr->rarity);

        wr_byte(a_ptr->activation);
        wr_u16b(a_ptr->time);
        wr_u16b(a_ptr->randtime);

        for (int bi = 0; bi < A_MAX; bi++)
            wr_s16b(a_ptr->stat_bonus[bi]);
        for (int bi = 0; bi < S_MAX; bi++)
            wr_s16b(a_ptr->skill_bonus[bi]);
        for (int bi = 0; bi < A_MAX; bi++)
            wr_byte(a_ptr->stat_bonus_set[bi] ? 1 : 0);
        for (int bi = 0; bi < S_MAX; bi++)
            wr_byte(a_ptr->skill_bonus_set[bi] ? 1 : 0);
    }
}

