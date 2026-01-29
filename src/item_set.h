#ifndef INCLUDED_ITEM_SET_H
#define INCLUDED_ITEM_SET_H

#include "h-basic.h"
#include "init.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Item set system (data-driven, from lib/edit/set.txt).
 *
 * - Supports "paired weapons" sets (no off-hand penalty; no Two Weapon ability needed)
 * - Supports simple stat/skill bonuses when the full set is equipped.
 */

void item_sets_reset(void);

#ifdef ALLOW_TEMPLATES
errr parse_set_info(char* buf, header* head);
#endif

errr item_sets_finalize(void);

int item_sets_get_paired_artefact(int art_idx);

void item_sets_apply_player_bonuses(void);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDED_ITEM_SET_H */

