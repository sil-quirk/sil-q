/* File: object/object-make.c */

#include "angband.h"
#include "externs.h"
#include "object/object-make.h"
#include "object/object-internal.h"
#include "log/log.h"
#include <stdint.h>


static void object_mention(const object_type* o_ptr)
{
    char o_name[80];

    /* Describe */
    object_desc_spoil(o_name, sizeof(o_name), o_ptr, false, 0);

    /* Artefact */
    if (artefact_p(o_ptr))
    {
        /* Silly message */
        msg_format("Artefact (%s)", o_name);
    }

    /* Ego-item */
    else if (ego_item_p(o_ptr))
    {
        /* Silly message */
        msg_format("Ego-item (%s)", o_name);
    }

    /* Normal item */
    else
    {
        /* Silly message */
        msg_format("Object (%s)", o_name);
    }
}

/*
 * Attempt to change an object into an special item -MWK-
 * Better only called by apply_magic().
 * The return value says if we picked a cursed item (if allowed) and is
 * passed on to a_m_aux1/2().
 * If no legal ego item is found, this routine returns 0, resulting in
 * an unenchanted item.
 */
static int make_special_item(object_type* o_ptr, bool only_good)
{
    int i, j, level;

    int e_idx;

    long value, total;

    ego_item_type* e_ptr;
    object_kind* k_ptr = &k_info[o_ptr->k_idx];

    alloc_entry* table = alloc_ego_table;

    /* Fail if object already is ego or artefact */
    if (o_ptr->name1)
        return (false);
    if (object_has_ego(o_ptr))
        return (false);

    level = object_level;

    /* Boost level (like with object base types) */
    if (level > 0)
    {
        /* Occasional "boost" */
        if (one_in_(GREAT_SPECIAL))
        {
            // most of the time, choose a new deeper depth, weighted towards the
            // current depth
            if (level < MORGOTH_DEPTH)
            {
                int x = rand_range(level + 1, MORGOTH_DEPTH);
                int y = rand_range(level + 1, MORGOTH_DEPTH);

                level = MIN(x, y);
            }

            // but if it was already very deep, just increment it
            else
            {
                level++;
            }
        }
    }

    /* Reset total */
    total = 0L;

    /* Process probabilities */
    for (i = 0; i < alloc_ego_size; i++)
    {
        /* Default */
        table[i].prob3 = 0;

        /* Objects are sorted by depth */
        if (table[i].level > level)
            continue;

        /* Get the index */
        e_idx = table[i].index;

        /* Get the actual kind */
        e_ptr = &e_info[e_idx];

        /* Some special items can't be generated too deep */
        if ((e_ptr->max_level > 0) && (p_ptr->depth > e_ptr->max_level))
            continue;
        if (e_ptr->flags3 & TR3_DAMAGED)
            continue; /* Damaged drops are reserved for explicit damaged-drop paths. */

        /* If we force fine/special, don't create cursed */
        if (only_good && (e_ptr->flags3 & TR3_LIGHT_CURSE))
            continue;

        /* If we force fine/special, don't useless */
        if (only_good && (e_ptr->cost == 0))
            continue;

        /* Don't mix opposing alignment flags on ego creations. */
        if ((k_ptr->flags4 & TR4_NOBLE_ITEM) && (e_ptr->flags4 & TR4_EVIL_ITEM))
            continue;
        if ((k_ptr->flags4 & TR4_EVIL_ITEM) && (e_ptr->flags4 & TR4_NOBLE_ITEM))
            continue;
        if ((k_ptr->flags3 & (TR3_MITHRIL | TR3_STAR_IRON))
            && (e_ptr->flags4 & TR4_EVIL_ITEM))
            continue;

        /* Test if this is a legal special item type for this object */
        for (j = 0; j < EGO_TVALS_MAX; j++)
        {
            /* Require identical base type */
            if (o_ptr->tval == e_ptr->tval[j])
            {
                /* Require sval in bounds, lower */
                if (o_ptr->sval >= e_ptr->min_sval[j])
                {
                    /* Require sval in bounds, upper */
                    if (o_ptr->sval <= e_ptr->max_sval[j])
                    {
                        /* Accept */
                        table[i].prob3 = table[i].prob2;
                    }
                }
            }
        }

        /* Total */
        total += table[i].prob3;
    }

    // If there aren't *any* valid items to choose from give up
    if (total == 0)
    {
        return (0);
    }

    /* Pick an special item */
    value = rand_int(total);

    /* Find the object */
    for (i = 0; i < alloc_ego_size; i++)
    {
        /* Found the entry */
        if (value < table[i].prob3)
            break;

        /* Decrement */
        value = value - table[i].prob3;
    }

    /* We have one */
    e_idx = (byte)table[i].index;
    {
        ego_item_type* chosen = &e_info[e_idx];
        const char* raw = e_name + chosen->name;
        if (ego_name_is_prefix(raw))
            object_set_ego_prefix(o_ptr, e_idx);
        else
            object_set_ego_suffix(o_ptr, e_idx);
    }

    return ((e_info[e_idx].flags3 & TR3_LIGHT_CURSE) ? -2 : 2);
}

/*
 * As artefacts are generated, there is an increasing chance to fail to make the
 * next one
 */
static bool too_many_artefacts(void)
{
    int i;

    for (i = 0; i < p_ptr->artefacts; i++)
    {
        if (percent_chance(10))
            return (true);
    }

    return (false);
}

#if 0
/*
 * Mega-Hack -- Attempt to create one of the "Special Objects".
 *
 * We are only called from "make_object()", and we assume that
 * "apply_magic()" is called immediately after we return.
 *
 * Note -- see "make_artefact()" and "apply_magic()".
 *
 * We *prefer* to create the special artefacts in order, but this is
 * normally outweighed by the "rarity" rolls for those artefacts.
 */
static bool make_artefact_special(object_type* o_ptr)
{
    int i;

    int k_idx;

    int depth_check = ((object_generation_mode) ? object_level : p_ptr->depth);

    /* No artefacts, do nothing */
    if (adult_no_artefacts)
        return (false);

    // as more artefacts are generated, the chance for another decreases
    if (too_many_artefacts())
        return (false);

    /* Check the special artefacts */
    for (i = 0; i < z_info->art_spec_max; ++i)
    {
        artefact_type* a_ptr = &a_info[i];

        /* Skip "empty" artefacts */
        if (a_ptr->tval + a_ptr->sval == 0)
            continue;

        /* Cannot make an artefact twice */
        if (a_ptr->cur_num)
            continue;

        /* Cannot make an artefact reserved for Valar quest */
        if (valar_reserved_artifacts && valar_reserved_artifacts[i])
            continue;

        /* Enforce minimum "depth" (loosely) */
        if (a_ptr->level > depth_check)
        {
            /* Get the "out-of-depth factor" */
            int d = (a_ptr->level - depth_check) * 2;

            /* Roll for out-of-depth creation */
            if (rand_int(d) != 0)
                continue;
        }

        /* Artefact "rarity roll" */
        if (rand_int(a_ptr->rarity) != 0)
            continue;

        /* Find the base object */
        k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);

        /* Enforce minimum "object" level (loosely) */
        if (k_info[k_idx].level > depth_check)
        {
            /* Get the "out-of-depth factor" */
            int d = (k_info[k_idx].level - depth_check) * 5;

            /* Roll for out-of-depth creation */
            if (rand_int(d) != 0)
                continue;
        }

        /* Assign the template */
        object_prep(o_ptr, k_idx);

        /* Mark the item as an artefact */
        o_ptr->name1 = i;

        /* Success */
        return (true);
    }

    /* Failure */
    return (false);
}
#endif

/*
 * Attempt to change an object into an artefact
 *
 * This routine should only be called by "apply_magic()"
 *
 * Note -- see "make_artefact_special()" and "apply_magic()"
 */
static bool make_artefact(object_type* o_ptr, bool allow_insta)
{
    int i;

    int depth_check = ((object_generation_mode) ? object_level : p_ptr->depth);

    /* No artefacts, do nothing */
    if (adult_no_artefacts)
        return (false);

    // as more artefacts are generated, the chance for another decreases
    if (too_many_artefacts())
        return (false);

    /* Check the artefact list (skip the "specials" and randoms) */
    for (i = z_info->art_spec_max; i < z_info->art_norm_max; i++)
    {
        artefact_type* a_ptr = &a_info[i];

        /* Skip "empty" items */
        if (a_ptr->tval + a_ptr->sval == 0)
            continue;

        /* Cannot make an artefact twice */
        if (a_ptr->cur_num)
            continue;

        /* Cannot make an artefact reserved for Valar quest */
        if (valar_reserved_artifacts && valar_reserved_artifacts[i])
            continue;

        /* Must have the correct fields */
        if (a_ptr->tval != o_ptr->tval)
            continue;
        if (a_ptr->sval != o_ptr->sval)
            continue;

        /* Can only generate 'insta-arts' in certain situations */
        if ((a_ptr->flags3 & (TR3_INSTA_ART)) && !allow_insta)
        {
            continue;
        }

        /* XXX XXX Enforce minimum "depth" (loosely) */
        if (a_ptr->level > depth_check)
        {
            /* Get the "out-of-depth factor" */
            int d = (a_ptr->level - depth_check) * 2;

            /* Roll for out-of-depth creation */
            if (rand_int(d) != 0)
                continue;
        }

        /* We must make the "rarity roll" */
        if (!one_in_(a_ptr->rarity))
            continue;

        /* Mark the item as an artefact */
        o_ptr->name1 = i;

        /* Set stack size for stackable artefacts (arrows, throwing weapons) */
        {
            const object_kind* k_ptr = (o_ptr->k_idx ? &k_info[o_ptr->k_idx] : NULL);
            bool allow_stack = (o_ptr->tval == TV_ARROW)
                || (k_ptr && (k_ptr->flags3 & TR3_THROWING));
            if (allow_stack)
            {
                artefact_type* art_ptr = &a_info[i];
                int desired = art_ptr->spawn_num ? (int)art_ptr->spawn_num : 1;
                int limit = object_stack_limit(o_ptr);
                if (limit > 0 && desired > limit)
                    desired = limit;
                if (desired < 1)
                    desired = 1;
                o_ptr->number = (byte)desired;
            }
            else if (o_ptr->number < 1)
            {
                o_ptr->number = 1;
            }
        }

        /* Success */
        return (true);
    }

    /* Failure */
    return (false);
}

/*
 * Charge a new staff.
 */
static void charge_staff(object_type* o_ptr)
{
    int mult = CHANNELING_CHARGE_MULTIPLIER;

    switch (o_ptr->sval)
    {
    case SV_STAFF_SECRETS:
        o_ptr->pval = mult * damroll(4, 2);
        break;
    case SV_STAFF_IMPRISONMENT:
        o_ptr->pval = mult * damroll(4, 2);
        break;
    case SV_STAFF_FREEDOM:
        o_ptr->pval = mult * damroll(4, 2);
        break;
    case SV_STAFF_LIGHT:
        o_ptr->pval = mult * damroll(4, 2);
        break;
    case SV_STAFF_SANCTITY:
        o_ptr->pval = mult * damroll(2, 2);
        break;
    case SV_STAFF_UNDERSTANDING:
        o_ptr->pval = mult * damroll(2, 2);
        break;
    case SV_STAFF_REVELATIONS:
        o_ptr->pval = mult * damroll(4, 2);
        break;
    case SV_STAFF_TREASURES:
        o_ptr->pval = mult * damroll(2, 2);
        break;
    case SV_STAFF_FOES:
        o_ptr->pval = mult * damroll(4, 2);
        break;
    case SV_STAFF_SLUMBER:
        o_ptr->pval = mult * damroll(4, 2);
        break;
    case SV_STAFF_MAJESTY:
        o_ptr->pval = mult * damroll(4, 2);
        break;
    case SV_STAFF_SELF_KNOWLEDGE:
        o_ptr->pval = mult * damroll(2, 2);
        break;
    case SV_STAFF_DISMAY:
        o_ptr->pval = mult * damroll(2, 2);
        break;
    case SV_STAFF_RECHARGING:
        o_ptr->pval = mult * damroll(2, 2);
        break;

    case SV_STAFF_SUMMONING:
        o_ptr->pval = mult * damroll(6, 2);
        break;
    case SV_STAFF_SHADOWS:
        o_ptr->pval = mult * damroll(4, 2);
        break;
    }
}

/*
 *
 * Determines the theme of a chest.  This function is called
 * from chest_death when the chest is being opened. JG
 *
 */
static int choose_chest_contents(void)
{
    /*
     * chest theme # 2 is potions  (+ herbs of restoring)
     * chest theme # 3 is staffs
     * chest theme # 4 is shields
     * chest theme # 5 is weapons
     * chest theme # 6 is armor
     * chest theme # 7 is boots
     * chest theme # 8 is bow
     * chest theme # 9 is cloak
     * chest theme #10 is gloves
     * chest theme #11 is edged weapons
     * chest theme #12 is polearms
     * chest theme #13 is helms and crowns
     * chest theme #14 is jewellery
     */

    return (dieroll(13) + 1);
}

/*
 * Apply magic to an item known to be a "weapon"
 *
 */
static void a_m_aux_1(object_type* o_ptr, int level)
{
    bool boost_dam = false;
    bool boost_att = false;

    // arrows can only have increased attack value
    if (o_ptr->tval == TV_ARROW)
    {
        o_ptr->att += 3;
        return;
    }

    else
    {
        // small chance of boosting both
        if (percent_chance(level))
        {
            boost_dam = true;
            boost_att = true;
        }
        // otherwise 50/50 chance of dam or att
        else if (one_in_(2))
        {
            boost_dam = true;
        }
        else
        {
            boost_att = true;
        }
    }

    if (boost_dam)
    {
        o_ptr->ds++;
    }
    if (boost_att)
    {
        o_ptr->att++;
    }
}

/*
 * Apply magic to an item known to be "armor"
 *
 */
static void a_m_aux_2(object_type* o_ptr, int level)
{
    bool boost_prot = false;
    bool boost_other = false;

    // for cloaks and robes and filthy rags go for evasion only
    if ((o_ptr->tval == TV_CLOAK)
        || ((o_ptr->tval == TV_SOFT_ARMOR) && (o_ptr->sval == SV_ROBE)))
    {
        boost_other = true;
    }
    // otherwise if there are no penalties to fix, then go for protection only
    else if ((o_ptr->att >= 0) && (o_ptr->evn >= 0))
    {
        boost_prot = true;
    }
    // otherwise choose randomly (protection, other, or both)
    else
    {
        // small chance of boosting both
        if (percent_chance(level))
        {
            boost_prot = true;
            boost_other = true;
        }
        // otherwise 50/50 chance of dam or att
        else if (one_in_(2))
        {
            boost_prot = true;
        }
        else
        {
            boost_other = true;
        }
    }

    if (boost_other)
    {
        if ((o_ptr->att < 0) && (o_ptr->evn < 0))
        {
            if (one_in_(2))
                o_ptr->evn++;
            else
                o_ptr->att++;
        }
        else if (o_ptr->att < 0)
        {
            o_ptr->att++;
        }
        else
        {
            o_ptr->evn++;
        }
    }

    if (boost_prot)
    {
        o_ptr->ps++;
    }
}

/*
 * Apply magic to an item known to be "boring"
 *
 * Hack -- note the special code for various items
 */
static void a_m_aux_4(object_type* o_ptr, int level, bool fine, bool special)
{
    object_kind* k_ptr = &k_info[o_ptr->k_idx];

    /* Unused parameters */
    (void)level;

    /* Apply magic (good or bad) according to type */
    switch (o_ptr->tval)
    {
    case TV_LIGHT:
    {
        /* Hack -- Torches -- random fuel */
        if (o_ptr->sval == SV_LIGHT_TORCH)
        {
            int spawn_fuel = 1000;
            int min_fuel = 250;

            if (one_in_(3))
            {
                o_ptr->timeout = rand_range(min_fuel, spawn_fuel);
            }
            else
            {
                o_ptr->timeout = spawn_fuel;
            }
        }

        /* Hack -- Lanterns -- random fuel */
        else if (o_ptr->sval == SV_LIGHT_LANTERN)
        {
            int spawn_fuel = (FUEL_LAMP * 2) / 5;
            int min_fuel = FUEL_LAMP / 15;

            if (one_in_(3))
            {
                o_ptr->timeout = rand_range(min_fuel, spawn_fuel);
            }
            else
            {
                o_ptr->timeout = spawn_fuel;
            }
        }

        /* Mallorn torches -- random fuel */
        if (o_ptr->sval == SV_LIGHT_MALLORN)
        {
            if (one_in_(3))
            {
                o_ptr->timeout = rand_range(40, 100);
            }
            else
            {
                o_ptr->timeout = 100;
            }
        }
        break;
    }

    case TV_STAFF:
    {
        /* Hack -- charge staffs */
        charge_staff(o_ptr);

        break;
    }

    case TV_GEM:
    {
        /* Gems use number instead of charges - spawn same quantity as charge_staff would have given */
        int charges = 0;
        
        switch (o_ptr->sval)
        {
        case SV_GEM_FREEDOM:
        case SV_GEM_LIGHT:
        case SV_GEM_REVELATIONS:
        case SV_GEM_FOES:
            charges = damroll(4, 2);
            break;
        case SV_GEM_SANCTITY:
        case SV_GEM_UNDERSTANDING:
        case SV_GEM_TREASURES:
        case SV_GEM_SELF_KNOWLEDGE:
        case SV_GEM_RECHARGING:
        case SV_GEM_SHADOWS:
            charges = damroll(2, 2);
            break;
        default:
            charges = damroll(2, 2);
            break;
        }
        
        o_ptr->number = charges;
        o_ptr->pval = 0;  /* Gems don't use pval */
        
        break;
    }

    case TV_HORN:
    {
        /* Transfer the pval. */
        o_ptr->pval = k_ptr->pval;
        break;
    }

    case TV_SKELETON:
    {
        /* Not searched. */
        o_ptr->pval = 1;
        break;
    }

    case TV_CHEST:
    {
        /* Hack -- chest level is fixed at player level at time of generation */
        o_ptr->pval = object_level;

        /*chest created with fine flag get a level boost*/
        if (fine)
            o_ptr->pval += 2;

        /*chest created with special flag also gets a level boost*/
        if (special)
            o_ptr->pval += 2;

        /*chests now increase level rating*/
        rating += 5;

        /* Don't exceed "chest level" of 25 */
        if (o_ptr->pval > 25)
            o_ptr->pval = 25;

        /*a minimum pval of 1, or else it will be empty on the surface*/
        if (o_ptr->pval < 1)
            o_ptr->pval = 1;

        /*save the chest theme in xtra1, used in chest death*/
        o_ptr->xtra1 = choose_chest_contents();

        break;
    }
    }
}

void object_into_artefact(object_type* o_ptr, artefact_type* a_ptr)
{
    int i;

    /* Extract the other fields */
    o_ptr->pval = a_ptr->pval;
    for (i = 0; i < A_MAX; i++)
        o_ptr->stat_bonus[i] = a_ptr->stat_bonus[i];
    for (i = 0; i < S_MAX; i++)
        o_ptr->skill_bonus[i] = a_ptr->skill_bonus[i];
    o_ptr->att = a_ptr->att;
    o_ptr->dd = a_ptr->dd;
    o_ptr->ds = a_ptr->ds;
    o_ptr->evn = a_ptr->evn;
    o_ptr->pd = a_ptr->pd;
    o_ptr->ps = a_ptr->ps;
    o_ptr->weight = a_ptr->weight;

    // add the abilities
    for (i = 0; i < a_ptr->abilities; i++)
    {
        o_ptr->skilltype[i + o_ptr->abilities] = a_ptr->skilltype[i];
        o_ptr->abilitynum[i + o_ptr->abilities] = a_ptr->abilitynum[i];
        o_ptr->bane_type[i + o_ptr->abilities] = a_ptr->bane_type[i];
    }
    o_ptr->abilities += a_ptr->abilities;

    /* Hack - mark the depth of artefact creation for the notes function
     * probably a bad idea to use this flag.  It is used when making special
     * items, which currently fails when an item is an artefact.  If this was
     * changed this would be the cause of some major bugs.
     */
    if (p_ptr->depth)
    {
        o_ptr->xtra1 = p_ptr->depth;
    }

    /*hack - mark chest items with a special level so the notes patch
     * knows where it is coming from.
     */
    else if (object_generation_mode == OB_GEN_MODE_CHEST)
        o_ptr->xtra1 = CHEST_LEVEL;
    else if (object_generation_mode == OB_GEN_MODE_SKELETON)
        o_ptr->xtra1 = SKELETON_LEVEL;

    /* Hack -- extract the "broken" flag */
    if (!a_ptr->cost)
        o_ptr->ident |= (IDENT_BROKEN);

    /* Hack -- extract the "cursed" flag */
    if (a_ptr->flags3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        o_ptr->ident |= (IDENT_CURSED);
}

static void apply_delta_byte_clamped(byte* v, int delta)
{
    if (!v)
        return;

    int next = (int)(*v) + delta;
    if (next < 0)
        next = 0;
    if (next > 255)
        next = 255;
    *v = (byte)next;
}

static s16b roll_ego_bonus_range(s16b min_bonus, s16b max_bonus, bool smithing)
{
    if (smithing || min_bonus == max_bonus)
        return min_bonus;

    return (s16b)rand_range(min_bonus, max_bonus);
}

static void apply_ego_explicit_bonus_ranges(object_type* o_ptr,
    const ego_item_type* e_ptr, bool smithing)
{
    if (!o_ptr || !e_ptr)
        return;

    for (int i = 0; i < A_MAX; i++)
    {
        if (!e_ptr->stat_bonus_set[i])
            continue;

        o_ptr->stat_bonus[i] += roll_ego_bonus_range(
            e_ptr->stat_bonus_min[i], e_ptr->stat_bonus[i], smithing);
    }

    for (int i = 0; i < S_MAX; i++)
    {
        if (!e_ptr->skill_bonus_set[i])
            continue;

        o_ptr->skill_bonus[i] += roll_ego_bonus_range(
            e_ptr->skill_bonus_min[i], e_ptr->skill_bonus[i], smithing);
    }
}

static bool ego_affix_has_only_flag_effects(const ego_item_type* e_ptr)
{
    if (!e_ptr)
        return false;

    if (e_ptr->abilities != 0 || e_ptr->max_pval != 0 || e_ptr->min_pval != 0
        || e_ptr->max_att != 0 || e_ptr->to_dd != 0 || e_ptr->to_ds != 0
        || e_ptr->max_evn != 0 || e_ptr->to_pd != 0 || e_ptr->to_ps != 0)
    {
        return false;
    }

    for (int i = 0; i < A_MAX; i++)
    {
        if (e_ptr->stat_bonus_set[i] || e_ptr->stat_bonus_min[i] != 0
            || e_ptr->stat_bonus[i] != 0)
        {
            return false;
        }
    }

    for (int i = 0; i < S_MAX; i++)
    {
        if (e_ptr->skill_bonus_set[i] || e_ptr->skill_bonus_min[i] != 0
            || e_ptr->skill_bonus[i] != 0)
        {
            return false;
        }
    }

    return true;
}

static bool object_is_fire_breakable_weapon(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (o_ptr->tval == TV_HAFTED)
        return true;

    if (o_ptr->tval == TV_POLEARM)
        return true;

    return false;
}

static s32b pack_fire_broken_weapon_payload(s16b att, byte dd, byte ds)
{
    u32b payload = (u32b)(u16b)att;
    payload |= ((u32b)dd << 16);
    payload |= ((u32b)ds << 24);
    return (s32b)payload;
}

static void unpack_fire_broken_weapon_payload(s32b payload, s16b* att, byte* dd,
    byte* ds)
{
    u32b bits = (u32b)payload;

    if (att)
        *att = (s16b)(bits & 0xFFFFU);
    if (dd)
        *dd = (byte)((bits >> 16) & 0xFFU);
    if (ds)
        *ds = (byte)((bits >> 24) & 0xFFU);
}

bool object_is_fire_broken(const object_type* o_ptr)
{
    return object_runtime_state(o_ptr) == OBJECT_RUNTIME_STATE_FIRE_BROKEN;
}

bool object_break_shafted_weapon_by_fire(object_type* o_ptr)
{
    if (!object_is_fire_breakable_weapon(o_ptr))
        return false;

    if (object_is_fire_broken(o_ptr))
        return true;

    object_set_runtime_payload(
        o_ptr, pack_fire_broken_weapon_payload(o_ptr->att, o_ptr->dd, o_ptr->ds));
    object_set_runtime_state(o_ptr, OBJECT_RUNTIME_STATE_FIRE_BROKEN);

    if (o_ptr->att > SHRT_MIN)
        o_ptr->att--;

    if (o_ptr->ds > 1)
        o_ptr->ds--;
    else if (o_ptr->dd > 1)
        o_ptr->dd--;

    pseudo_id(o_ptr);
    return true;
}

bool object_repair_fire_broken_weapon(object_type* o_ptr)
{
    s16b att = 0;
    byte dd = 0;
    byte ds = 0;

    if (!object_is_fire_broken(o_ptr))
        return false;

    unpack_fire_broken_weapon_payload(
        object_runtime_payload(o_ptr), &att, &dd, &ds);

    o_ptr->att = att;
    o_ptr->dd = dd;
    o_ptr->ds = ds;
    object_set_runtime_state(o_ptr, OBJECT_RUNTIME_STATE_NONE);
    object_set_runtime_payload(o_ptr, 0);

    pseudo_id(o_ptr);
    return true;
}

bool object_break_brass_lantern(object_type* o_ptr)
{
    byte old_prefix;
    bool old_prefix_carried_intrinsic_curse = false;
    bool new_state_is_intrinsically_cursed = false;

    if (!o_ptr || !o_ptr->k_idx || o_ptr->tval != TV_LIGHT
        || o_ptr->sval != SV_LIGHT_LANTERN)
    {
        return false;
    }

    old_prefix = object_ego_prefix(o_ptr);
    if (old_prefix == EGO_BROKEN_BRASS_LANTERN)
    {
        o_ptr->ident |= IDENT_BROKEN;
        return true;
    }

    if (old_prefix)
    {
        if (old_prefix >= z_info->e_max)
            return false;

        if (!ego_affix_has_only_flag_effects(&e_info[old_prefix]))
        {
            log_warn(
                "object_break_brass_lantern: unsupported lantern prefix %d",
                old_prefix);
            return false;
        }

        old_prefix_carried_intrinsic_curse
            = (e_info[old_prefix].flags3
                & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
            != 0;
    }

    object_set_ego_prefix(o_ptr, EGO_BROKEN_BRASS_LANTERN);
    o_ptr->ident |= IDENT_BROKEN;

    if (o_ptr->name1
        && (a_info[o_ptr->name1].flags3
            & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE)))
    {
        new_state_is_intrinsically_cursed = true;
    }

    if (k_info[o_ptr->k_idx].flags3
        & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
    {
        new_state_is_intrinsically_cursed = true;
    }

    if (object_ego_suffix(o_ptr)
        && (e_info[object_ego_suffix(o_ptr)].flags3
            & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE)))
    {
        new_state_is_intrinsically_cursed = true;
    }

    if (new_state_is_intrinsically_cursed)
        o_ptr->ident |= IDENT_CURSED;
    else if (old_prefix_carried_intrinsic_curse)
        o_ptr->ident &= ~IDENT_CURSED;

    pseudo_id(o_ptr);
    return true;
}

bool object_apply_ego_affix(object_type* o_ptr, int e_idx, bool smithing)
{
    const object_kind* k_ptr = NULL;
    ego_item_type* e_ptr;
    u32b ef3, ef4;
    int i;
    int max_att;
    int to_dd;
    int to_ds;
    int max_evn;
    int to_pd;
    int to_ps;
    bool enforce_positive_protection;

    if (!o_ptr || !o_ptr->k_idx || e_idx <= 0 || e_idx >= z_info->e_max)
        return false;

    k_ptr = &k_info[o_ptr->k_idx];
    e_ptr = &e_info[e_idx];
    ef3 = e_ptr->flags3;
    ef4 = e_ptr->flags4;
    u32b pval_mask = object_kind_pval_flags1(k_ptr) | ego_item_pval_flags1(e_ptr);
    max_att = (int)(int8_t)e_ptr->max_att;
    to_dd = (int)(int8_t)e_ptr->to_dd;
    to_ds = (int)(int8_t)e_ptr->to_ds;
    max_evn = (int)(int8_t)e_ptr->max_evn;
    to_pd = (int)(int8_t)e_ptr->to_pd;
    to_ps = (int)(int8_t)e_ptr->to_ps;
    enforce_positive_protection = ((k_ptr->pd > 0) && (k_ptr->ps > 0))
        || (to_pd > 0) || (to_ps > 0);

    for (i = 0; i < e_ptr->abilities && o_ptr->abilities < (int)N_ELEMENTS(o_ptr->skilltype); i++)
    {
        int idx = o_ptr->abilities;
        o_ptr->skilltype[idx] = e_ptr->skilltype[i];
        o_ptr->abilitynum[idx] = e_ptr->abilitynum[i];
        o_ptr->abilities++;
    }

    if (!e_ptr->cost)
        o_ptr->ident |= (IDENT_BROKEN);
    if (ef3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        o_ptr->ident |= (IDENT_CURSED);

    if (smithing)
    {
        if (max_att)
            o_ptr->att += (max_att > 0) ? 1 : -1;
        if (max_evn)
            o_ptr->evn += (max_evn > 0) ? 1 : -1;
        if (to_dd)
            apply_delta_byte_clamped(&o_ptr->dd, (to_dd > 0) ? 1 : -1);
        if (to_ds)
            apply_delta_byte_clamped(&o_ptr->ds, (to_ds > 0) ? 1 : -1);
        if (to_pd)
            apply_delta_byte_clamped(&o_ptr->pd, (to_pd > 0) ? 1 : -1);
        if (to_ps)
            apply_delta_byte_clamped(&o_ptr->ps, (to_ps > 0) ? 1 : -1);

        if (e_ptr->max_pval > 0)
        {
            int delta = 1;
            o_ptr->pval += (s16b)delta;
            object_apply_pval_delta_with_mask(o_ptr, pval_mask, delta);
        }
    }
    else
    {
        if (max_att)
            o_ptr->att += (max_att > 0) ? dieroll(max_att) : -dieroll(-max_att);
        if (max_evn)
            o_ptr->evn += (max_evn > 0) ? dieroll(max_evn) : -dieroll(-max_evn);
        if (to_dd)
            apply_delta_byte_clamped(&o_ptr->dd, (to_dd > 0) ? dieroll(to_dd) : -dieroll(-to_dd));
        if (to_ds)
            apply_delta_byte_clamped(&o_ptr->ds, (to_ds > 0) ? dieroll(to_ds) : -dieroll(-to_ds));
        if (to_pd)
            apply_delta_byte_clamped(&o_ptr->pd, (to_pd > 0) ? dieroll(to_pd) : -dieroll(-to_pd));
        if (to_ps)
            apply_delta_byte_clamped(&o_ptr->ps, (to_ps > 0) ? dieroll(to_ps) : -dieroll(-to_ps));

        if (e_ptr->max_pval > 0)
        {
            int delta = dieroll(e_ptr->max_pval);
            o_ptr->pval += (s16b)delta;
            object_apply_pval_delta_with_mask(o_ptr, pval_mask, delta);
        }
    }

    apply_ego_explicit_bonus_ranges(o_ptr, e_ptr, smithing);

    if (k_ptr->dd > 0)
    {
        if (o_ptr->dd < 1)
            o_ptr->dd = 1;

        if (o_ptr->ds < 1)
        {
            int deficit = 1 - (int)o_ptr->ds;
            o_ptr->ds = 1;
            if ((int)o_ptr->dd > deficit)
                o_ptr->dd = (byte)((int)o_ptr->dd - deficit);
            else
                o_ptr->dd = 1;
        }
    }

    if (enforce_positive_protection && ((k_ptr->pd > 0) || (o_ptr->pd > 0)))
    {
        if (o_ptr->pd < 1)
            o_ptr->pd = 1;

        if (o_ptr->ps < 1)
        {
            int deficit = 1 - (int)o_ptr->ps;
            o_ptr->ps = 1;
            if ((int)o_ptr->pd > deficit)
                o_ptr->pd = (byte)((int)o_ptr->pd - deficit);
            else
                o_ptr->pd = 1;
        }
    }

    apply_object_weight_flags(o_ptr, k_ptr->weight, ef4);

    pseudo_id(o_ptr);
    return true;
}

void object_into_special(object_type* o_ptr, int lev, bool smithing)
{
    u32b f1, f2, f3, f4;
    int i;
    const object_kind* k_ptr = NULL;
    bool enforce_positive_protection = false;

    (void)
        lev; // Cast to soothe compilation warnings (currently unused variable)

    if (o_ptr && o_ptr->k_idx)
        k_ptr = &k_info[o_ptr->k_idx];
    if (k_ptr && (k_ptr->pd > 0) && (k_ptr->ps > 0))
        enforce_positive_protection = true;

    /* Examine the item */
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    /* Ensure overall curse state is set before applying pval deltas. */
    if (f3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        o_ptr->ident |= (IDENT_CURSED);

    /* Apply each ego present (prefix then suffix). */
    byte ego_ids[2] = { object_ego_prefix(o_ptr), object_ego_suffix(o_ptr) };
    for (int ego_slot = 0; ego_slot < 2; ego_slot++)
    {
        byte e_idx = ego_ids[ego_slot];
        if (!e_idx)
            continue;

        ego_item_type* e_ptr = &e_info[e_idx];
        u32b pval_mask = object_kind_pval_flags1(k_ptr) | ego_item_pval_flags1(e_ptr);
        int max_att = (int)(int8_t)e_ptr->max_att;
        int to_dd = (int)(int8_t)e_ptr->to_dd;
        int to_ds = (int)(int8_t)e_ptr->to_ds;
        int max_evn = (int)(int8_t)e_ptr->max_evn;
        int to_pd = (int)(int8_t)e_ptr->to_pd;
        int to_ps = (int)(int8_t)e_ptr->to_ps;
        if ((to_pd > 0) || (to_ps > 0))
            enforce_positive_protection = true;

        /* Add the abilities (bounded by object ability storage). */
        for (i = 0; i < e_ptr->abilities && o_ptr->abilities < (int)N_ELEMENTS(o_ptr->skilltype); i++)
        {
            int idx = o_ptr->abilities;
            o_ptr->skilltype[idx] = e_ptr->skilltype[i];
            o_ptr->abilitynum[idx] = e_ptr->abilitynum[i];
            o_ptr->abilities++;
        }

        /* Acquire "broken"/"cursed" flags. */
        if (!e_ptr->cost)
            o_ptr->ident |= (IDENT_BROKEN);
        if (e_ptr->flags3 & (TR3_LIGHT_CURSE))
            o_ptr->ident |= (IDENT_CURSED);

        /* Apply numeric bonuses. */
        if (smithing)
        {
            if (max_att)
                o_ptr->att += (max_att > 0) ? 1 : -1;
            if (max_evn)
                o_ptr->evn += (max_evn > 0) ? 1 : -1;
            if (to_dd)
                apply_delta_byte_clamped(&o_ptr->dd, (to_dd > 0) ? 1 : -1);
            if (to_ds)
                apply_delta_byte_clamped(&o_ptr->ds, (to_ds > 0) ? 1 : -1);
            if (to_pd)
                apply_delta_byte_clamped(&o_ptr->pd, (to_pd > 0) ? 1 : -1);
            if (to_ps)
                apply_delta_byte_clamped(&o_ptr->ps, (to_ps > 0) ? 1 : -1);

            if (e_ptr->max_pval > 0)
            {
                int delta = 1;
                o_ptr->pval += (s16b)delta;
                object_apply_pval_delta_with_mask(o_ptr, pval_mask, delta);
            }
        }
        else
        {
            if (max_att)
                o_ptr->att += (max_att > 0) ? dieroll(max_att) : -dieroll(-max_att);
            if (max_evn)
                o_ptr->evn += (max_evn > 0) ? dieroll(max_evn) : -dieroll(-max_evn);
            if (to_dd)
                apply_delta_byte_clamped(&o_ptr->dd, (to_dd > 0) ? dieroll(to_dd) : -dieroll(-to_dd));
            if (to_ds)
                apply_delta_byte_clamped(&o_ptr->ds, (to_ds > 0) ? dieroll(to_ds) : -dieroll(-to_ds));
            if (to_pd)
                apply_delta_byte_clamped(&o_ptr->pd, (to_pd > 0) ? dieroll(to_pd) : -dieroll(-to_pd));
            if (to_ps)
                apply_delta_byte_clamped(&o_ptr->ps, (to_ps > 0) ? dieroll(to_ps) : -dieroll(-to_ps));

            if (e_ptr->max_pval > 0)
            {
                int delta = dieroll(e_ptr->max_pval);
                o_ptr->pval += (s16b)delta;
                object_apply_pval_delta_with_mask(o_ptr, pval_mask, delta);
            }
        }

        /* Apply ego-specific M: rolls after pval-based bonuses. */
        apply_ego_explicit_bonus_ranges(o_ptr, e_ptr, smithing);

    }

    /* Never allow invalid dice/sides on items that normally have them. */
    if (k_ptr && k_ptr->dd > 0)
    {
        if (o_ptr->dd < 1)
            o_ptr->dd = 1;

        if (o_ptr->ds < 1)
        {
            int deficit = 1 - (int)o_ptr->ds;
            o_ptr->ds = 1;
            if ((int)o_ptr->dd > deficit)
                o_ptr->dd = (byte)((int)o_ptr->dd - deficit);
            else
                o_ptr->dd = 1;
        }
    }

    if (enforce_positive_protection && k_ptr
        && ((k_ptr->pd > 0) || (o_ptr->pd > 0)))
    {
        if (o_ptr->pd < 1)
            o_ptr->pd = 1;

        if (o_ptr->ps < 1)
        {
            int deficit = 1 - (int)o_ptr->ps;
            o_ptr->ps = 1;
            if ((int)o_ptr->pd > deficit)
                o_ptr->pd = (byte)((int)o_ptr->pd - deficit);
            else
                o_ptr->pd = 1;
        }
    }

    /* Apply explicit weight flags relative to base kind weight. */
    if (k_ptr)
        apply_object_weight_flags(o_ptr, k_ptr->weight, f4);

    /* Cheat -- describe the item */
    if (cheat_peek)
        object_mention(o_ptr);

    // pseudo-id the item
    pseudo_id(o_ptr);
}

/*
 * Complete the "creation" of an object by applying "magic" to the item
 *
 * This includes not only rolling for random bonuses, but also putting the
 * finishing touches on special items and artefacts, giving charges to wands and
 * staffs, giving fuel to lites, and placing traps on chests.
 *
 * In particular, note that "Instant Artefacts", if "created" by an external
 * routine, must pass through this function to complete the actual creation.
 *
 * The base chance of the item being "fine" increases with the "level"
 * parameter, which is usually derived from the dungeon level, being equal
 * to (level)%.
 * The chance that the object will be "special" (special item or artefact),
 * is also (level)%.
 * If "good" is true, then
 * the object is guaranteed to be either "fine" or "special".
 * If "great" is true, then the object is guaranteed to be
 * both "fine" and "special".
 *
 * If "okay" is true, and the object is going to be "special", then there is
 * a chance that an artefact will be created.  This is true even if both the
 * "good" and "great" arguments are false.  Objects which have both "good" and
 * "great" flags get three extra "attempts" to become an artefact.
 *
 * If "allow_insta" is true, then INSTA_ART artefacts can be generated
 *
 * Note that in the above we are using the new terminology of 'fine' and
 * 'special' where Vanilla Angband used 'good' and 'great'. A big change is that
 * these are now independent: you can have ego items that don't have extra
 * mundane bonuses
 * (+att, +evn, +sides...)
 */
void apply_magic(object_type* o_ptr, int lev, bool okay, bool good, bool great,
    bool allow_insta)
{
    int i, artefact_rolls;

    bool fine = false;
    bool special = false;

    /* Maximum "level" for various things */
    if (lev > MAX_DEPTH - 1)
        lev = MAX_DEPTH - 1;

    /* Roll for "fine" */
    if (percent_chance(lev * 2))
        fine = true;

    /* Roll for "special" */
    if (percent_chance(lev))
        special = true;

    /* guarantee "fine" or "special" for "good" drops */
    if (good)
    {
        if (one_in_(2))
            fine = true;
        else
            special = true;
    }

    /* guarantee "fine" and "special" for "great" drops */
    if (great)
    {
        fine = true;
        special = true;
    }

    /* Assume no rolls */
    artefact_rolls = 0;

    if (special)
        artefact_rolls = 1;

    if (great)
        artefact_rolls = 3;

    /* Get 8 rolls if good and great are both set */
    if ((good) && (great))
        artefact_rolls = 8;

    /* Get no rolls if not allowed */
    if (!okay || o_ptr->name1)
        artefact_rolls = 0;

    /* Roll for artefacts if allowed */
    for (i = 0; i < artefact_rolls; i++)
    {
        /* Roll for an artefact */
        if (make_artefact(o_ptr, allow_insta))
            break;
    }

    /* Hack -- analyze artefacts */
    if (o_ptr->name1)
    {
        artefact_type* a_ptr = &a_info[o_ptr->name1];

        /* Artifact tracking based on generation context:
         * - Monsters: Mark as created (INSTA_ARTs are monster-specific, won't spawn elsewhere)
         * - Chests: Mark as created+seen immediately (prevents chest scumming)
         * - Ground: Mark as created but NOT seen (check visibility later)
         */
        if (object_generation_mode == OB_GEN_MODE_CHEST)
        {
            /* Chest artifacts: mark as created and seen immediately */
            a_ptr->cur_num = 1;
            o_ptr->ident |= IDENT_ARTIFACT_SEEN;
        }
        else if (object_generation_mode == OB_GEN_MODE_NORMAL)
        {
            /* Ground artifacts: mark as created but not yet seen */
            a_ptr->cur_num = 1;
            /* Don't set IDENT_ARTIFACT_SEEN yet - wait for player visibility */
        }
        else
        {
            /* Monster/special artifacts: mark as created (INSTA_ART are unique to monsters) */
            a_ptr->cur_num = 1;
            /* Don't mark as seen - player may not encounter the monster */
        }

        object_into_artefact(o_ptr, a_ptr);

        /* Mega-Hack -- increase the rating */
        rating += 10;

        /* Set the good item flag */
        good_item_flag = true;

        /* Cheat -- peek at the item */
        if (cheat_peek)
            object_mention(o_ptr);

        // pseudo-id the item
        pseudo_id(o_ptr);

        // keep count of artefacts generated (not including insta-arts)
        if (!(a_ptr->flags3 & (TR3_INSTA_ART)))
            p_ptr->artefacts++;

        /* Done */
        return;
    }

    /* Apply magic */
    switch (o_ptr->tval)
    {
    case TV_SWORD:
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_BOW:
    {
        // deal with special items
        if (special)
        {
            int ego_power;

            ego_power = make_special_item(o_ptr, (bool)(good || great));

            // if we were unlucky enough to have no valid special types
            // then at least let it be a fine item
            if (ego_power == 0)
                fine = true;
        }

        // deal with fine items
        if (fine)
        {
            a_m_aux_1(o_ptr, lev);
        }

        // Throwing weapons keep their rolled weight; a generated multi-item
        // stack shares that one roll.
        if ((k_info[o_ptr->k_idx].flags3 & (TR3_THROWING))
            && !artefact_p(o_ptr))
        {
            // often come in multiples, but limited to quiver stack size
            if (one_in_(2))
            {
                int stack_limit = object_stack_limit(o_ptr);
                int max_spawn = (stack_limit < 5) ? stack_limit : 5;
                int min_spawn = (max_spawn < 2) ? 1 : 2;
                o_ptr->number = rand_range(min_spawn, max_spawn);
            }
        }

        break;
    }
    case TV_ARROW:
    {
        // note that arrows can't be both fine and special

        if (special)
        {
            // More special arrows lower down
            make_special_item(o_ptr, (bool)(good || great));
            if (o_ptr->number > 1)
                o_ptr->number /= 2;
        }

        else if (fine)
        {
            a_m_aux_1(o_ptr, lev);
            if (o_ptr->number > 1)
                o_ptr->number /= 2;
        }

        break;
    }

    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_HELM:
    case TV_CROWN:
    case TV_CLOAK:
    case TV_GLOVES:
    case TV_BOOTS:
    {
        if (special)
        {
            int ego_power;

            ego_power = make_special_item(o_ptr, (bool)(good || great));

            // if we were unlucky enough to have no valid special types
            // then at least let it be a fine item
            if (ego_power == 0)
                fine = true;
        }

        if (fine)
        {
            a_m_aux_2(o_ptr, lev);
        }

        break;
    }

    case TV_LIGHT:
    {
        if (special)
        {
            make_special_item(o_ptr, (bool)(good || great));
        }

        /* Fuel it */
        a_m_aux_4(o_ptr, lev, fine, special);
        break;
    }

    default:
    {
        a_m_aux_4(o_ptr, lev, fine, special);
        break;
    }
    }

    /* Hack -- analyze special items */
    if (object_has_ego(o_ptr))
    {
        // apply all the bonuses for the given special item type
        object_into_special(o_ptr, lev, false);

        /* Done */
        return;
    }

    /* Examine real objects */
    if (o_ptr->k_idx)
    {
        object_kind* k_ptr = &k_info[o_ptr->k_idx];

        /* Hack -- acquire "broken" flag */
        if (!k_ptr->cost)
            o_ptr->ident |= (IDENT_BROKEN);

        /* Hack -- acquire "cursed" flag */
        if (k_ptr->flags3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
            o_ptr->ident |= (IDENT_CURSED);

        // identify non-special non-artefact weapons/armour
        switch (o_ptr->tval)
        {
        case TV_DIGGING:
        case TV_HAFTED:
        case TV_POLEARM:
        case TV_SWORD:
        case TV_BOW:
        case TV_ARROW:
        case TV_MAIL:
        case TV_SOFT_ARMOR:
        case TV_SHIELD:
        case TV_HELM:
        case TV_CROWN:
        case TV_CLOAK:
        case TV_GLOVES:
        case TV_BOOTS:
        case TV_LIGHT:
        {
            /* Identify it */
            object_aware(o_ptr);
            object_known(o_ptr);
        }
        }
    }
}

#if 0
/*
 * Hack -- determine if a template is "great".
 *
 * Note that this test only applies to the object *kind*, so it is
 * possible to choose a kind which is "great", and then later cause
 * the actual object to be cursed.  We do explicitly forbid objects
 * which are known to be boring or which start out somewhat damaged.
 */
static bool kind_is_great(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Armor -- great */
    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    {
        return (true);
    }

    /* Weapons -- great */
    case TV_BOW:
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    {
        return (true);
    }

    /* Chests -- great */
    case TV_CHEST:
    {
        return (true);
    }
    }

    /* Assume not great */
    return (false);
}
#endif

#if 0
/*
 * Hack -- determine if a template is a chest.
 *
 */
static bool kind_is_chest(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Chests -- */
    case TV_CHEST:
    {
        return (true);
    }
    }

    /* Assume not chest */
    return (false);
}
#endif

/*
 * Hack -- determine if a template is footwear.
 *
 */
static bool kind_is_boots(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* footwear -- */
    case TV_BOOTS:
    {
        return (true);
    }
    }

    /* Assume not footwear */
    return (false);
}

/*
 * Hack -- determine if a template is headgear.
 *
 */
static bool kind_is_headgear(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Headgear -- Suitable */
    case TV_HELM:
    case TV_CROWN:
    {
        return (true);
    }
    }

    /* Assume not headgear */
    return (false);
}

/*
 * Hack -- determine if a template is armor.
 *
 */
static bool kind_is_armor(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Armor -- suitable */
    case TV_MAIL:
    case TV_SOFT_ARMOR:
    {
        return (true);
    }
    }

    /* Assume not armor */
    return (false);
}

/*
 * Hack -- determine if a template is gloves.
 *
 */
static bool kind_is_gloves(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Gloves -- suitable */
    case TV_GLOVES:
    {
        return (true);
    }
    }

    /* Assume not suitable  */
    return (false);
}

/*
 * Hack -- determine if a template is a cloak.
 *
 */
static bool kind_is_cloak(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
        /* Cloaks -- suitable */

    case TV_CLOAK:
    {
        return (true);
    }
    }

    /* Assume not a suitable  */
    return (false);
}

/*
 * Hack -- determine if a template is a shield.
 *
 */
static bool kind_is_shield(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* shield -- suitable */
    case TV_SHIELD:
    {
        return (true);
    }
    }

    /* Assume not suitable */
    return (false);
}

/*
 * Hack -- determine if a template is a bow/arrow.
 */

static bool kind_is_bow(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* All bows and arrows are suitable  */
    case TV_BOW:
    {
        return (true);
    }
    }

    /* Assume not suitable  */
    return (false);
}

/*
 * Hack -- determine if a template is a "good" digging tool
 *
 */
static bool kind_is_digging_tool(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Diggers -- Good */
    case TV_DIGGING:
    {
        return (true);
    }
    }

    /* Assume not good */
    return (false);
}

/*
 * Hack -- determine if a template is a edged weapon.
 */
static bool kind_is_edged(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Edged Weapons -- suitable */
    case TV_SWORD:
    {
        return (true);
    }
    }

    /* Assume not suitable */
    return (false);
}

/*
 * Hack -- determine if a template is a polearm.
 */
static bool kind_is_polearm(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Weapons -- suitable */
    case TV_POLEARM:
    {
        return (true);
    }
    }

    /* Assume not suitable */
    return (false);
}

/*
 * Hack -- determine if a template is a weapon.
 */
static bool kind_is_weapon(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Weapons -- suitable */
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    {
        return (true);
    }
    }

    /* Assume not suitable */
    return (false);
}

#if 0
/*
 * Hack -- determine if a potion is good for a chest.
 * includes herb of restoring
 *
 */
static bool kind_is_potion(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /*potions suitable for a chest*/
    case TV_POTION:
    {
        if (k_ptr->sval == SV_POTION_QUICKNESS)
            return (true);
        if (k_ptr->sval == SV_POTION_MIRUVOR)
            return (true);
        if (k_ptr->sval == SV_POTION_HEALING)
            return (true);
        return (false);
    }

    case TV_FOOD:
        /* HACK -  herbs of restoring can be with potions */
        {
            if ((k_ptr->sval == SV_FOOD_RESTORATION)
                && ((k_ptr->level + 5) >= object_level))
                return (true);
            return (false);
        }
    }

    /* Assume not suitable */
    return (false);
}
#endif

#if 0
/*
 * Hack -- determine if a staff is good for a chest.
 *
 */
static bool kind_is_staff(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    if (k_ptr->tval == TV_STAFF || k_ptr->tval == TV_GEM)
    {
        /*staffs suitable for a chest*/
        if (k_ptr->sval == SV_STAFF_UNDERSTANDING)
            return (true);
        if (k_ptr->sval == SV_STAFF_TREASURES)
            return (true);
        if (k_ptr->sval == SV_STAFF_SLUMBER)
            return (true);
        if (k_ptr->sval == SV_STAFF_WARDING || k_ptr->sval == SV_GEM_WARDING)
            return (true);
        if (k_ptr->sval == SV_STAFF_RECHARGING)
            return (true);
    }

    /* Assume not suitable for a chest */
    return (false);
}
#endif

#if 0
/*
 * Hack -- determine if a template is "jewelry for chests".
 *
 */
static bool kind_is_jewelry(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Crowns are suitable for a chest */
    case TV_CROWN:
    {
        return (true);
    }

    case TV_RING:
    {
        if (k_ptr->sval == SV_RING_STR)
            return (true);
        if (k_ptr->sval == SV_RING_DEX)
            return (true);
        if (k_ptr->sval == SV_RING_EVASION)
            return (true);
        if (k_ptr->sval == SV_RING_ERED_LUIN)
            return (true);
        if (k_ptr->sval == SV_RING_ACCURACY)
            return (true);
        if (k_ptr->sval == SV_RING_BARAHIR)
            return (true);
        if (k_ptr->sval == SV_RING_MELIAN)
            return (true);
        return (false);
    }

    case TV_AMULET:
    {
        if (k_ptr->sval == SV_AMULET_TINFANG_GELION)
            return (true);
        if (k_ptr->sval == SV_AMULET_NIMPHELOS)
            return (true);
        if (k_ptr->sval == SV_AMULET_ELESSAR)
            return (true);
        if (k_ptr->sval == SV_AMULET_DWARVES)
            return (true);
        if (k_ptr->sval == SV_AMULET_BLESSED_REALM)
            return (true);
        if (k_ptr->sval == SV_AMULET_CON)
            return (true);
        if (k_ptr->sval == SV_AMULET_GRA)
            return (true);
        if (k_ptr->sval == SV_AMULET_PROTECTION)
            return (true);
        if (k_ptr->sval == SV_AMULET_VIGILANT_EYE)
            return (true);
        if (k_ptr->sval == SV_AMULET_LAST_CHANCES)
            return (true);
        return (false);
    }
    }

    /* Assume not suitable for a chest */
    return (false);
}
#endif

#if 0
/*
 * Hack -- determine if a template is "good".
 *
 * Note that this test only applies to the object *kind*, so it is
 * possible to choose a kind which is "good", and then later cause
 * the actual object to be cursed.  We do explicitly forbid objects
 * which are known to be boring or which start out somewhat damaged.
 */
static bool kind_is_good(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];

    if (kind_is_damaged_item(k_idx))
        return false;

    /* Analyze the item type */
    switch (k_ptr->tval)
    {
    /* Armor -- Good */
    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    {
        return (true);
    }

    /* Weapons -- Good */
    case TV_BOW:
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_DIGGING:
    {
        return (true);
    }

    /* Arrows -- Good */
    case TV_ARROW:
    {
        return (true);
    }

    /* Rings -- Rings of Speed are good */
    case TV_RING:
    {
        return (false);
    }

    /*the very powerful healing potions can be good*/
    case TV_POTION:
    {
        if (k_ptr->sval == SV_POTION_MIRUVOR)
            return (true);
        if (k_ptr->sval == SV_POTION_QUICKNESS)
            return (true);
        if (k_ptr->sval == SV_POTION_HEALING)
            return (true);
        return (false);
    }

    /* Chests -- Chests are good. */
    case TV_CHEST:
    {
        return (true);
    }
    }

    /* Assume not good */
    return (false);
}
#endif

/*
 * Attempt to make an object (normal or weighted quality)
 *
 * This routine plays nasty games to generate the "special artefacts".
 *
 * This routine uses "object_level" for the "generation level".
 *
 * We assume that the given object has been "wiped".
 */
static void apply_generated_object_rating(object_type* j_ptr, bool* mentioned)
{
    if (!cursed_p(j_ptr) && !broken_p(j_ptr)
        && (k_info[j_ptr->k_idx].level > p_ptr->depth))
    {
        rating += (k_info[j_ptr->k_idx].level - p_ptr->depth);
        if (cheat_peek)
        {
            object_mention(j_ptr);
            if (mentioned)
                *mentioned = true;
        }
    }
}

bool make_object_with_profile(object_type* j_ptr, drop_quality quality,
    int objecttype, const drop_profile* profile)
{
    int depth = object_level;
    bool allow_artefacts = (object_generation_mode == OB_GEN_MODE_CHEST)
        || (object_generation_mode == OB_GEN_MODE_MONSTER_DROP);
    if (!drop_generate_object_profiled(
            depth, quality, objecttype, 0, allow_artefacts, profile, j_ptr))
        return false;

    apply_generated_object_rating(j_ptr, NULL);

    return true;
}

bool make_object(object_type* j_ptr, drop_quality quality, int objecttype)
{
    return make_object_with_profile(j_ptr, quality, objecttype, NULL);
}

bool make_guaranteed_artefact_with_profile(object_type* j_ptr,
    drop_quality quality, int objecttype, const drop_profile* profile)
{
    bool allow_artefacts = (object_generation_mode == OB_GEN_MODE_CHEST)
        || (object_generation_mode == OB_GEN_MODE_MONSTER_DROP);

    if (!allow_artefacts || adult_no_artefacts)
        return false;

    bool mentioned = false;

    if (!drop_generate_guaranteed_artefact(
            object_level, object_level, quality, objecttype, profile, j_ptr))
    {
        return false;
    }

    apply_generated_object_rating(j_ptr, &mentioned);

    rating += 10;
    good_item_flag = true;

    if (cheat_peek && !mentioned)
        object_mention(j_ptr);

    pseudo_id(j_ptr);

    if (j_ptr->name1)
    {
        artefact_type* a_ptr = &a_info[j_ptr->name1];
        if (!(a_ptr->flags3 & TR3_INSTA_ART))
            p_ptr->artefacts++;
    }

    return true;
}

bool make_guaranteed_artefact(object_type* j_ptr, drop_quality quality, int objecttype)
{
    return make_guaranteed_artefact_with_profile(
        j_ptr, quality, objecttype, NULL);
}

/*
 * Set the object theme
 */

/*
 * This is an imcomplete list of themes.  Returns false if theme not found.
 * Used primarily for Randarts
 */
bool prep_object_theme(int themetype)
{
    /*get the store creation mode*/
    switch (themetype)
    {
    case DROP_TYPE_SHIELD:
    {
        get_obj_num_hook = kind_is_shield;
        break;
    }
    case DROP_TYPE_WEAPON:
    {
        get_obj_num_hook = kind_is_weapon;
        break;
    }
    case DROP_TYPE_EDGED:
    {
        get_obj_num_hook = kind_is_edged;
        break;
    }
    case DROP_TYPE_POLEARM:
    {
        get_obj_num_hook = kind_is_polearm;
        break;
    }
    case DROP_TYPE_ARMOR:
    {
        get_obj_num_hook = kind_is_armor;
        break;
    }
    case DROP_TYPE_BOOTS:
    {
        get_obj_num_hook = kind_is_boots;
        break;
    }
    case DROP_TYPE_BOW:
    {
        get_obj_num_hook = kind_is_bow;
        break;
    }
    case DROP_TYPE_CLOAK:
    {
        get_obj_num_hook = kind_is_cloak;
        break;
    }
    case DROP_TYPE_GLOVES:
    {
        get_obj_num_hook = kind_is_gloves;
        break;
    }
    case DROP_TYPE_HEADGEAR:
    {
        get_obj_num_hook = kind_is_headgear;
        break;
    }
    case DROP_TYPE_DIGGING:
    {
        get_obj_num_hook = kind_is_digging_tool;

        break;
    }
    case DROP_TYPE_DAMAGED:
    {
        get_obj_num_hook = kind_is_damaged_item;

        break;
    }

    default:
        return (false);
    }

    /*prepare the allocation table*/
    get_obj_num_prep();

    return (true);
}

/*
 * Let the floor carry an object
 */
