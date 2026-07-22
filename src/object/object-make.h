/* File: object/object-make.h */

#ifndef INCLUDED_OBJECT_MAKE_H
#define INCLUDED_OBJECT_MAKE_H

#include "angband.h"

#ifndef DROP_QUALITY_T_DEFINED
#define DROP_QUALITY_T_DEFINED
typedef enum
{
    DROP_QUALITY_NORMAL = 0,
    DROP_QUALITY_GOOD = 1,
    DROP_QUALITY_GREAT = 2,
    DROP_QUALITY_SUPERB = 3,
    DROP_QUALITY_ARTEFACT = 4
} drop_quality;
#endif

#define DROP_BONUS_GOOD 5
#define DROP_BONUS_GREAT 10
#define DROP_BONUS_SUPERB 15
#define DROP_BONUS_ARTEFACT 20
#define DROP_GREAT_ARTEFACT_WEIGHT_MULTIPLIER 5
#define DROP_CHEST_NOBLE_RARITY_BONUS 20

#ifndef DROP_PROFILE_T_DEFINED
#define DROP_PROFILE_T_DEFINED
typedef struct
{
    int weight_weapon;
    int weight_armor;
    int weight_jewelry;
    int weight_supply;
    int supply_potion;
    int supply_herb;
    int supply_gem;
    int supply_staff;
    int supply_light;
    int supply_arrows;
    int supply_tunneling;
    bool allow_damaged;
} drop_profile;
#endif

void object_into_artefact(object_type* o_ptr, artefact_type* a_ptr);
bool object_apply_ego_affix(object_type* o_ptr, int e_idx, bool smithing);
bool object_break_brass_lantern(object_type* o_ptr);
bool object_is_fire_broken(const object_type* o_ptr);
bool object_has_broken_prefix(const object_type* o_ptr);
bool object_break_shafted_weapon_by_fire(object_type* o_ptr);
bool object_repair_fire_broken_weapon(object_type* o_ptr);
void object_into_special(object_type* o_ptr, int lev, bool smithing);
void apply_magic(object_type* o_ptr, int lev, bool okay, bool good, bool great,
    bool allow_insta);
bool make_object_with_profile(object_type* j_ptr, drop_quality quality,
    int objecttype, const drop_profile* profile);
bool make_object(object_type* j_ptr, drop_quality quality, int objecttype);
bool make_guaranteed_artefact_with_profile(object_type* j_ptr,
    drop_quality quality, int objecttype, const drop_profile* profile);
bool make_guaranteed_artefact(object_type* j_ptr, drop_quality quality,
    int objecttype);
bool prep_object_theme(int themetype);

#endif /* INCLUDED_OBJECT_MAKE_H */
