/* File: object/object-knowledge.c */

#include "angband.h"
#include "externs.h"
#include "object/object-knowledge.h"
#include "object/object-internal.h"
#include "log/log.h"


void object_known(object_type* o_ptr)
{
    /* Remove special inscription, if any */
    if (o_ptr->discount >= INSCRIP_NULL)
        o_ptr->discount = 0;

    /* The object is not "sensed" */
    o_ptr->ident &= ~(IDENT_SENSE);

    /* Now we know about the item */
    o_ptr->ident |= (IDENT_KNOWN);
}

/*
 * The player is now aware of the effects of the given object.
 */
void object_aware(object_type* o_ptr)
{
    bool flag = k_info[o_ptr->k_idx].aware;
    bool quiet_awareness = !character_generated || character_xtra || character_icky;

    /* Fully aware of the effects */
    k_info[o_ptr->k_idx].aware = true;

    // If newly aware
    if (!flag && !p_ptr->leaving)
    {
        if (!quiet_awareness)
        {
            char o_name[120];

            // gain experience for identification
            int new_exp = 75;
            gain_exp(new_exp);
            p_ptr->ident_exp += new_exp;

            object_desc(o_name, sizeof(o_name), o_ptr, true, 0);
            msg_format("The true virtue of %s is unveiled to you, and 75 experience is won.",
                o_name);
        }

        // remove any autoinscription
        obliterate_autoinscription(o_ptr->k_idx);
    }
}

/*
 * Something has been "sampled"
 */
void object_tried(object_type* o_ptr)
{
    /* Mark it as tried (even if "aware") */
    k_info[o_ptr->k_idx].tried = true;
}

/*
 * Return the "value" of an "unknown" item
 * Make a guess at the value of non-aware items
 */
static s32b object_value_base(const object_type* o_ptr)
{
    int value = 0;

    object_kind* k_ptr = &k_info[o_ptr->k_idx];

    /* Use template cost for aware objects */
    if (object_aware_p(o_ptr))
    {
        /* Give credit for hit bonus */
        value += ((o_ptr->att - k_ptr->att) * 100L);

        /* Give credit for evasion bonus */
        value += ((o_ptr->evn - k_ptr->evn) * 100L);

        /* Give credit for sides bonus */
        value += ((o_ptr->ps - k_ptr->ps) * o_ptr->pd * 100L);

        /* Give credit for dice bonus */
        value += ((o_ptr->pd - k_ptr->pd) * o_ptr->ps * 100L);

        /* Give credit for sides bonus */
        value += ((o_ptr->ds - k_ptr->ds) * 100L);

        /* Give credit for dice bonus */
        value += ((o_ptr->dd - k_ptr->dd) * o_ptr->ds * 100L);

        // Arrows are worth less since they are perishable
        if (o_ptr->tval == TV_ARROW)
            value /= 10;

        // add in the base cost from the template
        value += k_ptr->cost;
    }

    else
    {
        /* Analyze the type */
        switch (o_ptr->tval)
        {
        /* Un-aware Food */
        case TV_FOOD:
            return (5L);

        /* Un-aware Potions */
        case TV_POTION:
            return (20L);

        /* Un-aware Staffs */
        case TV_STAFF:
            return (70L);

        /* Un-aware Rods */
        case TV_HORN:
            return (90L);

        /* Un-aware Rings */
        case TV_RING:
            return (45L);

        /* Un-aware Amulets */
        case TV_AMULET:
            return (45L);
        }
    }

    return (value);
}

/*
 * Return the "real" price of a "known" item, not including discounts.
 *
 * Wand and staffs get cost for each charge.
 *
 * Armor is worth an extra 100 gold per bonus point to armor class.
 *
 * Weapons are worth an extra 100 gold per bonus point (AC,TH,TD).
 *
 * Missiles are only worth 5 gold per bonus point, since they
 * usually appear in groups of 20, and we want the player to get
 * the same amount of cash for any "equivalent" item.  Note that
 * missiles never have any of the "pval" flags, and in fact, they
 * only have a few of the available flags, primarily of the "slay"
 * and "brand" and "ignore" variety.
 *
 * Weapons with negative hit+damage bonuses are worthless.
 *
 * Every wearable item with a "pval" bonus is worth extra (see below).
 */
static s32b object_value_real(const object_type* o_ptr)
{
    s32b value;

    u32b f1, f2, f3;

    object_kind* k_ptr = &k_info[o_ptr->k_idx];

    /* Hack -- "worthless" items */
    if (!k_ptr->cost)
        return (0L);

    /* Base cost */
    value = k_ptr->cost;

    /* Extract some flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    /* Artefact */
    if (o_ptr->name1)
    {
        artefact_type* a_ptr = &a_info[o_ptr->name1];

        /* Hack -- "worthless" artefacts */
        if (!a_ptr->cost)
            return (0L);

        /* Hack -- Use the artefact cost instead */
        value = a_ptr->cost;
    }

    /* Ego-Items (prefix and/or suffix) */
    else if (object_has_ego(o_ptr))
    {
        byte ego_prefix = object_ego_prefix(o_ptr);
        if (ego_prefix)
        {
            ego_item_type* e_ptr = &e_info[ego_prefix];
            if (!e_ptr->cost)
                return (0L);
            value += e_ptr->cost;
        }

        byte ego_suffix = object_ego_suffix(o_ptr);
        if (ego_suffix)
        {
            ego_item_type* e_ptr = &e_info[ego_suffix];
            if (!e_ptr->cost)
                return (0L);
            value += e_ptr->cost;
        }
    }

    /* Analyze pval bonus */
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
        /* Hack -- Negative "pval" is always bad */
        if (o_ptr->pval < 0)
            return (0L);

        /* Give credit for stat bonuses */
        if (f1 & (TR1_STR | TR1_NEG_STR))
            value += ((s32b)o_ptr->stat_bonus[A_STR] * 300L);
        if (f1 & (TR1_DEX | TR1_NEG_DEX))
            value += ((s32b)o_ptr->stat_bonus[A_DEX] * 300L);
        if (f1 & (TR1_CON | TR1_NEG_CON))
            value += ((s32b)o_ptr->stat_bonus[A_CON] * 300L);
        if (f1 & (TR1_GRA | TR1_NEG_GRA))
            value += ((s32b)o_ptr->stat_bonus[A_GRA] * 300L);

        /* Give credit for skills */
        if (f1 & (TR1_MEL))
            value += ((s32b)o_ptr->skill_bonus[S_MEL] * 100L);
        if (f1 & (TR1_ARC))
            value += ((s32b)o_ptr->skill_bonus[S_ARC] * 100L);
        if (f1 & (TR1_STL))
            value += ((s32b)o_ptr->skill_bonus[S_STL] * 100L);
        if (f1 & (TR1_PER))
            value += ((s32b)o_ptr->skill_bonus[S_PER] * 100L);
        if (f1 & (TR1_WIL))
            value += ((s32b)o_ptr->skill_bonus[S_WIL] * 100L);
        if (f1 & (TR1_SMT))
            value += ((s32b)o_ptr->skill_bonus[S_SMT] * 100L);
        if (f1 & (TR1_SNG))
            value += ((s32b)o_ptr->skill_bonus[S_SNG] * 100L);

        /* Give credit for tunneling */
        if (f1 & (TR1_TUNNEL))
            value += (o_ptr->pval * 50L);

        /* Give credit for speed bonus */
        if (f2 & (TR2_SPEED))
            value += 1000L;

        break;
    }
    }

    /* Analyze the item */
    switch (o_ptr->tval)
    {
    /* Staffs and Gems */
    case TV_STAFF:
    case TV_GEM:
    {
        /* Pay extra for charges, depending on standard number of
         * charges.  Handle new-style wands correctly.
         */
        value += ((value / 20) * (o_ptr->pval / o_ptr->number));

        /* Done */
        break;
    }

    /* Rings/Amulets */
    case TV_RING:
    case TV_AMULET:
    {
        /* Hack -- negative bonuses are bad */
        if (o_ptr->att < 0)
            return (0L);
        if (o_ptr->evn < 0)
            return (0L);

        /* Give credit for bonuses */
        value += ((o_ptr->att + o_ptr->evn + o_ptr->ps) * 100L);

        /* Done */
        break;
    }

    /* Armor */
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_CLOAK:
    case TV_CROWN:
    case TV_HELM:
    case TV_SHIELD:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        /* Give credit for hit bonus */
        value += ((o_ptr->att - k_ptr->att) * 100L);

        /* Give credit for evasion bonus */
        value += ((o_ptr->evn - k_ptr->evn) * 100L);

        /* Give credit for sides bonus */
        value += ((o_ptr->ps - k_ptr->ps) * o_ptr->pd * 50L);

        /* Give credit for dice bonus */
        value += ((o_ptr->pd - k_ptr->pd) * o_ptr->ps * 50L);

        /* Done */
        break;
    }

    /* Bows/Weapons */
    case TV_BOW:
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_SWORD:
    case TV_POLEARM:
    {
        /* Give credit for hit bonus */
        value += ((o_ptr->att - k_ptr->att) * 100L);

        /* Give credit for evasion bonus */
        value += ((o_ptr->evn - k_ptr->evn) * 100L);

        /* Give credit for sides bonus */
        value += ((o_ptr->ds - k_ptr->ds) * o_ptr->dd * 51L);

        /* Give credit for dice bonus */
        value += ((o_ptr->dd - k_ptr->dd) * o_ptr->ds * 51L);

        /* Done */
        break;
    }

    /* Arrows */
    case TV_ARROW:
    {
        /* Give credit for hit bonus */
        value += ((o_ptr->att - k_ptr->att) * 10L);

        /* Done */
        break;
    }
    }

    /* No negative value */
    if (value < 0)
        value = 0;

    /* Return the value */
    return (value);
}

bool object_has_ego_flag4(const object_type* o_ptr, u32b flag)
{
    byte ego_prefix;
    byte ego_suffix;

    if (!o_ptr || !flag)
        return false;

    ego_prefix = object_ego_prefix(o_ptr);
    if (ego_prefix && (e_info[ego_prefix].flags4 & flag))
        return true;

    ego_suffix = object_ego_suffix(o_ptr);
    if (ego_suffix && (e_info[ego_suffix].flags4 & flag))
        return true;

    return false;
}

/*
 * Return the price of an item including plusses (and charges).
 *
 * This function returns the "value" of the given item (qty one).
 *
 * Never notice "unknown" bonuses or properties, including "curses",
 * since that would give the player information he did not have.
 *
 * Note that discounted items stay discounted forever.
 */
s32b object_value(const object_type* o_ptr)
{
    s32b value;

    /* Known items -- acquire the actual value */
    if (object_known_p(o_ptr))
    {
        /* Broken items -- worthless */
        if (broken_p(o_ptr))
            return (0L);

        /* Cursed items -- worthless */
        if (cursed_p(o_ptr))
            return (0L);

        /* Real value (see above) */
        value = object_value_real(o_ptr);
    }

    /* Unknown items -- acquire the base value */
    else
    {
        /* Hack -- Felt broken items */
        if ((o_ptr->ident & (IDENT_SENSE)) && broken_p(o_ptr))
            return (0L);

        /* Hack -- Felt cursed items */
        if ((o_ptr->ident & (IDENT_SENSE)) && cursed_p(o_ptr))
            return (0L);

        /* Base value (see above) */
        value = object_value_base(o_ptr);
    }

    /* Return the final value */
    return (value);
}

/*
 * Determine if an item can "absorb" a second item
 *
 * See "object_absorb()" for the actual "absorption" code.
 *
 * If permitted, we allow wands/staffs (if they are known to have equal
 * charges) and rods (if fully charged) to combine.  They will unstack
 * (if necessary) when they are used.
 *
 * If permitted, we allow weapons/armor to stack, if fully "known".
 *
 * Missiles will combine if both stacks have the same "known" status.
 * This is done to make unidentified stacks of missiles useful.
 *
 * Food, potions, and "easy know" items always stack.
 *
 * Chests, and activatable items, except rods, never stack (for various
 * reasons).
 */
bool object_similar(const object_type* o_ptr, const object_type* j_ptr)
{
    /* Require identical object types */
    if (o_ptr->k_idx != j_ptr->k_idx)
        return (false);

    /* Require identical weight */
    if (!(o_ptr->weight == j_ptr->weight))
        return (false);

    /* Analyze the items */
    switch (o_ptr->tval)
    {
    /* Chests */
    case TV_SKELETON:
    case TV_CHEST:
    {
        /* Never okay */
        return (false);
    }

    /* Food, Potions, and Gems */
    case TV_FOOD:
    case TV_POTION:
    case TV_GEM:
    {
        /* Assume okay */
        break;
    }

    /* Staves */
    case TV_STAFF:
    {
        /* Don't merge as it messes with charges etc. */
        return (false);
    }

    /* Horns */
    case TV_HORN:
    {
        /* Assume okay */
        break;
    }

    /* Rings, Amulets, Lites and Books */
    case TV_RING:
    case TV_AMULET:
    case TV_LIGHT:
    {
        /* Require both items to be known */
        if (!object_known_p(o_ptr) || !object_known_p(j_ptr))
            return (false);

        __attribute__((fallthrough));
    }

    /* Weapons and Armor */
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
    {
        __attribute__((fallthrough));
    }

    /* Missiles & most things from above */
    case TV_ARROW:
    {
        /* Require identical knowledge of both items */
        if (object_known_p(o_ptr) != object_known_p(j_ptr))
            return (false);

        /* Require identical "bonuses" */
        if (o_ptr->att != j_ptr->att)
            return (false);
        if (o_ptr->evn != j_ptr->evn)
            return (false);
        if (o_ptr->ds != j_ptr->ds)
            return (false);
        if (o_ptr->dd != j_ptr->dd)
            return (false);

        // only check protection if at least one item has it
        if ((o_ptr->pd * o_ptr->ps > 0) || (j_ptr->pd * j_ptr->ps > 0))
        {
            if (o_ptr->ps != j_ptr->ps)
                return (false);
            if (o_ptr->pd != j_ptr->pd)
                return (false);
        }

        /* Require identical "pval" code */
        if (o_ptr->pval != j_ptr->pval)
            return (false);

        /* Require identical per-stat/skill bonuses */
        if (memcmp(o_ptr->stat_bonus, j_ptr->stat_bonus, sizeof(o_ptr->stat_bonus)) != 0)
            return (false);
        if (memcmp(o_ptr->skill_bonus, j_ptr->skill_bonus, sizeof(o_ptr->skill_bonus)) != 0)
            return (false);

        /* Require identical "artefact" names */
        if (o_ptr->name1 != j_ptr->name1)
            return (false);

        log_trace("object_similar: checking egos - o_ptr prefix=%d suffix=%d, j_ptr prefix=%d suffix=%d",
                  (int)object_ego_prefix(o_ptr), (int)object_ego_suffix(o_ptr),
                  (int)object_ego_prefix(j_ptr), (int)object_ego_suffix(j_ptr));

        /* Require identical ego affixes */
        if (object_ego_prefix(o_ptr) != object_ego_prefix(j_ptr)
            || object_ego_suffix(o_ptr) != object_ego_suffix(j_ptr))
        {
            log_trace("object_similar: DIFFERENT egos, returning false");
            return (false);
        }

        log_trace("object_similar: checking timeout - o_ptr=%d, j_ptr=%d",
                  o_ptr->timeout, j_ptr->timeout);

        /* Mega-Hack -- Handle lights */
        if (fuelable_light_p(o_ptr))
        {
            if (o_ptr->timeout != j_ptr->timeout)
            {
                log_debug("object_similar: DIFFERENT timeout, returning false");
                return (false);
            }
        }

        /* Hack -- Never stack recharging items */
        else if (o_ptr->timeout || j_ptr->timeout)
            return (false);

        /* Probably okay */
        break;
    }

    /* Various */
    default:
    {
        /* Require knowledge */
        if (!object_known_p(o_ptr) || !object_known_p(j_ptr))
            return (false);

        /* Probably okay */
        break;
    }
    }

    /* Runtime-state items carry per-item repair data and must never stack. */
    if (object_runtime_state(o_ptr) || object_runtime_state(j_ptr))
    {
        return (false);
    }

    /* Hack -- Require identical "cursed" and "broken" status */
    if (((o_ptr->ident & (IDENT_CURSED)) != (j_ptr->ident & (IDENT_CURSED)))
        || ((o_ptr->ident & (IDENT_BROKEN)) != (j_ptr->ident & (IDENT_BROKEN))))
    {
        return (false);
    }

    /* Hack -- Require compatible inscriptions */
    if (o_ptr->obj_note != j_ptr->obj_note)
    {
        /* Normally require matching inscriptions */
        return (false);
    }

    /* Hack -- Require compatible "discount" fields */
    if (o_ptr->discount != j_ptr->discount)
    {
        bool o_uncursed_only = (o_ptr->discount == INSCRIP_UNCURSED)
            && !cursed_p(o_ptr);
        bool j_uncursed_only = (j_ptr->discount == INSCRIP_UNCURSED)
            && !cursed_p(j_ptr);

        /* Allow {uncursed} to stack with an otherwise identical clean item. */
        if ((o_uncursed_only && (j_ptr->discount == 0))
            || (j_uncursed_only && (o_ptr->discount == 0)))
        {
        }
        /* Both are (different) special inscriptions */
        else if ((o_ptr->discount >= INSCRIP_NULL)
            && (j_ptr->discount >= INSCRIP_NULL))
        {
            /* Normally require matching inscriptions */
            return (false);
        }

        /* One is a special inscription, one is a discount or nothing */
        else if ((o_ptr->discount >= INSCRIP_NULL)
            || (j_ptr->discount >= INSCRIP_NULL))
        {
            /* Normally require matching inscriptions */
            return (false);
        }

        /* One is a discount, one is a (different) discount or nothing */
        else
        {
            /* require matching discounts */
            return (false);
        }
    }

    /* Maximal "stacking" limit */
    if (o_ptr->number >= object_stack_limit(o_ptr))
        return (false);
    if (j_ptr->number >= object_stack_limit(j_ptr))
        return (false);

    /* They match, so they must be similar */
    return (true);
}

/*
 * Allow one item to "absorb" another, assuming they are similar.
 *
 * The blending of the "note" field assumes that either (1) one has an
 * inscription and the other does not, or (2) neither has an inscription.
 * In both these cases, we can simply use the existing note, unless the
 * blending object has a note, in which case we use that note.
 *
 * The blending of the "discount" field assumes that either (1) one is a
 * special inscription and one is nothing, or (2) one is a discount and
 * one is a smaller discount, or (3) one is a discount and one is nothing,
 * or (4) both are nothing.  In all of these cases, we can simply use the
 * "maximum" of the two "discount" fields.
 *
 * These assumptions are enforced by the "object_similar()" code.
 */
void object_absorb(object_type* o_ptr, object_type* j_ptr)
{
    /* Log staff absorption attempts - this should never happen! */
    if (o_ptr->tval == TV_STAFF || j_ptr->tval == TV_STAFF)
    {
        log_error("BUG: object_absorb called on staff! o_ptr: tval=%d k_idx=%d number=%d, j_ptr: tval=%d k_idx=%d number=%d",
                  o_ptr->tval, o_ptr->k_idx, o_ptr->number, 
                  j_ptr->tval, j_ptr->k_idx, j_ptr->number);
    }

    int total = o_ptr->number + j_ptr->number;
    int limit = object_stack_limit(o_ptr);

    if (limit > object_stack_limit(j_ptr))
        limit = object_stack_limit(j_ptr);

    if (total > limit)
    {
        o_ptr->number = limit;
        j_ptr->number = total - limit;
    }
    else
    {
        o_ptr->number = total;
        j_ptr->number = 0;
    }

    /* Preserve auto-recovery intent across stack merges and partial absorbs. */
    {
        bool o_pickup = o_ptr->pickup ? true : false;
        bool j_pickup = j_ptr->pickup ? true : false;
        bool pickup = o_pickup || j_pickup;
        bool o_slot_valid = o_pickup
            && ((o_ptr->pickup_slot == INVEN_QUIVER1)
                || (o_ptr->pickup_slot == INVEN_QUIVER2));
        bool j_slot_valid = j_pickup
            && ((j_ptr->pickup_slot == INVEN_QUIVER1)
                || (j_ptr->pickup_slot == INVEN_QUIVER2));
        s16b pickup_slot = -1;

        if (o_slot_valid && j_slot_valid)
        {
            if (o_ptr->pickup_slot == j_ptr->pickup_slot)
                pickup_slot = o_ptr->pickup_slot;
        }
        else if (o_slot_valid)
        {
            pickup_slot = o_ptr->pickup_slot;
        }
        else if (j_slot_valid)
        {
            pickup_slot = j_ptr->pickup_slot;
        }

        o_ptr->pickup = pickup;
        j_ptr->pickup = pickup;
        o_ptr->pickup_slot = pickup ? pickup_slot : -1;
        j_ptr->pickup_slot = pickup ? pickup_slot : -1;
    }

    /* Hack -- Blend "known" status */
    if (object_known_p(j_ptr))
        object_known(o_ptr);
    if (object_known_p(o_ptr))
        object_known(j_ptr);

    /* Blend "handled" status (combat stats stay visible after dropping). */
    if (j_ptr->ident & IDENT_HANDLED)
        o_ptr->ident |= IDENT_HANDLED;
    if (o_ptr->ident & IDENT_HANDLED)
        j_ptr->ident |= IDENT_HANDLED;

    /* Hack -- Blend "notes" */
    if (j_ptr->obj_note != 0)
        o_ptr->obj_note = j_ptr->obj_note;
    if (o_ptr->obj_note != 0)
        j_ptr->obj_note = o_ptr->obj_note;

    /* Mega-Hack -- Blend "discounts" */
    if (o_ptr->discount < j_ptr->discount)
        o_ptr->discount = j_ptr->discount;
    if (j_ptr->discount < o_ptr->discount)
        j_ptr->discount = o_ptr->discount;
}

static int object_weight_flag_adjustment(int base_weight, u32b flags4)
{
    int quarter_basis;

    if (base_weight <= 0)
        return 0;

    quarter_basis = div_round(base_weight, 4);

    /* Weight affixes should move the item by at least 0.5 lb. */
    if (quarter_basis < 5)
        quarter_basis = 5;

    if ((flags4 & TR4_WEIGHT) && !(flags4 & TR4_NEG_WEIGHT))
        return quarter_basis;

    if ((flags4 & TR4_NEG_WEIGHT) && !(flags4 & TR4_WEIGHT))
        return -quarter_basis;

    return 0;
}

s16b object_roll_base_weight(const object_kind* k_ptr)
{
    int weight;

    if (!k_ptr)
        return 0;

    /* Exact weight for most items, approximate weight for weapons and armour. */
    switch (k_ptr->tval)
    {
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
    {
        weight = Rand_normal(k_ptr->weight, k_ptr->weight / 6 + 1);

        /* Round to the nearest multiple of 0.5 lb. */
        weight = div_round(weight * 2, 10);
        weight *= 5;

        /* Restrict weight to within [2/3, 3/2] of the standard. */
        while (weight * 3 < k_ptr->weight * 2)
            weight += 5;
        while (weight * 2 > k_ptr->weight * 3)
            weight -= 5;

        break;
    }
    default:
        weight = k_ptr->weight;
        break;
    }

    return (s16b)weight;
}

void apply_object_weight_flags(object_type* o_ptr, int base_weight,
    u32b flags4)
{
    int adjusted_weight;
    int weight_adjust = object_weight_flag_adjustment(base_weight, flags4);

    if (weight_adjust == 0)
        return;

    adjusted_weight = o_ptr->weight + weight_adjust;
    /* Weight is stored in tenths of a pound, so the minimum is 0.5 lb. */
    if (adjusted_weight < 5)
        adjusted_weight = 5;

    o_ptr->weight = (s16b)adjusted_weight;
}

void object_refresh_weight(object_type* o_ptr)
{
    object_kind* k_ptr;
    u32b f1, f2, f3, f4;

    if (!o_ptr || !o_ptr->k_idx || artefact_p(o_ptr))
        return;

    k_ptr = &k_info[o_ptr->k_idx];
    o_ptr->weight = object_roll_base_weight(k_ptr);

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    apply_object_weight_flags(o_ptr, k_ptr->weight, f4);
}
