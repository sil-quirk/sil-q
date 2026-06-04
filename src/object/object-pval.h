/* File: object/object-pval.h */

#ifndef INCLUDED_OBJECT_PVAL_H
#define INCLUDED_OBJECT_PVAL_H

#include "angband.h"

u32b object_kind_pval_flags1(const object_kind* k_ptr);
u32b artefact_pval_flags1(const artefact_type* a_ptr);
u32b ego_item_pval_flags1(const ego_item_type* e_ptr);
u32b object_pval_flags1(const object_type* o_ptr);
void object_apply_pval_delta_with_mask(object_type* o_ptr, u32b mask,
    int delta);

#endif /* INCLUDED_OBJECT_PVAL_H */
