/* File: fs/load-internal.h */

/*
 * Private helpers shared across the carved savefile-reader translation units
 * (src/fs/load.c + load-player.c + load-notes-inventory.c + load-dungeon.c).
 *
 * This is intentionally NOT a public subsystem header: nothing here belongs in
 * externs.h. Include it AFTER angband.h / externs.h / log/log.h so the basic
 * types and log_trace() are already visible.
 */

#ifndef INCLUDED_FS_LOAD_INTERNAL_H
#define INCLUDED_FS_LOAD_INTERNAL_H

/* Debug cursor: count of bytes consumed from the save stream (post-decode). */
extern u32b load_byte_offset;

/* Feature availability for the savefile currently being read. */
extern bool savefile_has_runtime_overrides;
extern bool savefile_has_monster_shatter;
extern bool savefile_has_song_duels;
extern bool savefile_has_ability_timeline;
extern bool savefile_has_varda_quest;
extern bool savefile_has_artifact_seen;
extern bool savefile_has_skeleton_notes;
extern bool savefile_has_skeleton_hint_mask;
extern bool savefile_has_skeleton_hint_mask32;
extern bool savefile_has_skeleton_hint_counts;
extern bool savefile_has_partition_meta;
extern bool savefile_has_partition_meta_types;
extern bool savefile_has_cave_info_hi;
extern bool savefile_has_cave_rewired;
extern bool savefile_has_cave_natural;
extern bool savefile_has_hint_messages;
extern bool savefile_has_hint_message_meta;
extern bool savefile_has_thrall_quest;
extern bool savefile_has_thrall_quest_requested;
extern bool savefile_has_randart_flags4;
extern bool savefile_has_item_bonuses;
extern bool savefile_has_randart_bonuses;
extern bool savefile_has_morgoth_call_state;
extern bool savefile_has_combat_history;

/* Prefetch / counters shared between the orchestrator and the lanes. */
extern u16b objects_count_prefetch;
extern bool color_rle_pair_prefetched;
extern byte color_rle_count_prefetch;
extern byte color_rle_value_prefetch;
extern u16b new_artefacts;
extern u16b art_norm_count;
extern u32b randart_version;

/* Low-level readers and shared helpers (defined in fs/load.c). */
void load_note(cptr msg);
void load_rd_byte(byte* ip);
void load_rd_bool(bool* bp);
void load_rd_u16b(u16b* ip);
void load_rd_s16b(s16b* ip);
void load_rd_u32b(u32b* ip);
void load_rd_s32b(s32b* ip);
void load_rd_string(char* str, int max);
void load_strip_bytes(int n);
errr load_rd_item(object_type* o_ptr);
void load_rd_monster(monster_type* m_ptr);
bool load_savefile_version_at_least(byte major, byte minor, byte patch, byte extra);
void load_artefact_derive_stat_skill_bonuses_from_pval(artefact_type* a_ptr);
void load_maybe_show_startup_loading_overlay(void);

/*
 * Legacy supply/oil migration lives in fs/load.c (it peeks the raw stream and
 * is orchestrated from load_player), but rd_inventory in the notes-inventory
 * lane needs this one probe.
 */
bool load_legacy_savefile_has_supply_block(void);

/* Lane entry points (each defined in its own fs/load-*.c). */
errr load_read_extra(void);
errr load_read_randarts(void);
bool load_read_notes(void);
errr load_read_inventory(void);
errr load_read_dungeon(void);

/*
 * Keep the terse in-file names working so the moved function bodies need no
 * edits. The real symbols carry a load_ prefix to avoid colliding with save.c
 * (and any other translation unit) in the global namespace.
 */
#define note load_note
#define rd_byte load_rd_byte
#define rd_bool load_rd_bool
#define rd_u16b load_rd_u16b
#define rd_s16b load_rd_s16b
#define rd_u32b load_rd_u32b
#define rd_s32b load_rd_s32b
#define rd_string load_rd_string
#define strip_bytes load_strip_bytes
#define rd_item load_rd_item
#define rd_monster load_rd_monster
#define savefile_version_at_least load_savefile_version_at_least
#define artefact_derive_stat_skill_bonuses_from_pval load_artefact_derive_stat_skill_bonuses_from_pval
#define maybe_show_startup_loading_overlay load_maybe_show_startup_loading_overlay
#define legacy_savefile_has_supply_block load_legacy_savefile_has_supply_block
#define rd_extra load_read_extra
#define rd_randarts load_read_randarts
#define rd_notes load_read_notes
#define rd_inventory load_read_inventory
#define rd_dungeon load_read_dungeon

/* Concise load logging (previously defined in the monolithic load.c). */
#define LOAD_LOG(fmt, ...)                                                      \
    log_trace("[load:%06u] " fmt, (unsigned)load_byte_offset, __VA_ARGS__)
#define LOAD_LOG0(msg)                                                          \
    log_trace("[load:%06u] %s", (unsigned)load_byte_offset, msg)

#endif /* INCLUDED_FS_LOAD_INTERNAL_H */
