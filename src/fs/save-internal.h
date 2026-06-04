/* File: fs/save-internal.h */

/*
 * Private helpers shared across the carved savefile-writer translation units
 * (src/fs/save.c + save-player.c + save-notes-inventory.c + save-dungeon.c).
 *
 * Like fs/load-internal.h, this is NOT a public subsystem header: nothing here
 * belongs in externs.h. Include it AFTER angband.h / externs.h so the basic
 * types are already visible.
 */

#ifndef INCLUDED_FS_SAVE_INTERNAL_H
#define INCLUDED_FS_SAVE_INTERNAL_H

/* Running count of bytes written (used in the inline "[save:%06u]" log lines). */
extern u32b save_byte_offset;

/* Dumps the character sheet to a text file; defined in fs/save.c, called by wr_extra. */
void updatecharinfoS(void);

/* Low-level writers shared between the orchestrator and the lanes. */
void save_wr_byte(byte v);
void save_wr_u16b(u16b v);
void save_wr_s16b(s16b v);
void save_wr_u32b(u32b v);
void save_wr_s32b(s32b v);
void save_wr_string(cptr str);
void save_wr_item(const object_type* o_ptr);
void save_wr_monster(const monster_type* m_ptr);

/* Lane entry points (each defined in its own fs/save-*.c). */
void save_write_extra(void);
void save_write_randarts(void);
void save_write_notes(void);
void save_write_dungeon(void);

/*
 * Keep the terse in-file names working so the moved function bodies need no
 * edits. The real symbols carry a save_ prefix to avoid colliding with load.c
 * (e.g. its rd_savefile) and any other translation unit.
 */
#define wr_byte save_wr_byte
#define wr_u16b save_wr_u16b
#define wr_s16b save_wr_s16b
#define wr_u32b save_wr_u32b
#define wr_s32b save_wr_s32b
#define wr_string save_wr_string
#define wr_item save_wr_item
#define wr_monster save_wr_monster
#define wr_extra save_write_extra
#define wr_randarts save_write_randarts
#define wr_notes save_write_notes
#define wr_dungeon save_write_dungeon

#endif /* INCLUDED_FS_SAVE_INTERNAL_H */
