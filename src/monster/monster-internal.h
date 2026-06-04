/* File: monster-internal.h */

#ifndef INCLUDED_MONSTER_INTERNAL_H
#define INCLUDED_MONSTER_INTERNAL_H

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "sdl-config.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
 * Pluralizer.  Args(count, singular, plural)
 */
#define plural(c, s, p) (((c) == 1) ? (s) : (p))

/*
 * Pronoun arrays, by gender.  Defined in monster-lore.c and shared with
 * monster-lore-combat.c.
 */
extern cptr wd_he[3];
extern cptr wd_his[3];
extern cptr wd_him[3];

/*
 * Monster-lore helpers shared between monster-lore.c and
 * monster-lore-combat.c (the recall description is split across both files).
 */
bool know_damage(const monster_lore* l_ptr, int i);
bool lore_knows_lament_stats(const monster_lore* l_ptr);
void describe_monster_spells(int r_idx, const monster_lore* l_ptr);
void describe_monster_drop(int r_idx, const monster_lore* l_ptr);
void describe_monster_attack(
    int r_idx, const monster_lore* l_ptr, const monster_type* m_ptr);
void describe_monster_abilities(int r_idx, const monster_lore* l_ptr);

/*
 * Hallucination guard for the recall screen.  Defined in monster-lore.c and
 * used by monster-recall.c.
 */
bool monster_recall_blocked_by_hallucination(void);
void monster_recall_show_hallucination_block(void);

/*
 * Pick a random hallucinatory monster race.  Defined in monster-list.c and
 * used by monster-spawn.c when stocking image_r_idx.
 */
int random_r_idx(void);

#endif /* INCLUDED_MONSTER_INTERNAL_H */
