#include "angband.h"
#include "externs.h"
#include "mem/alloc.h"
#include "fs/path.h"
#include "fs/io_sdl.h"
#include "gen-log.h"
#include "log/log.h"
#include <string.h>

/*
 * New drop generation system.
 *
 * Builds a catalog of all droppable items (base, ego variants, artefacts)
 * with precomputed smithing difficulties and depth metadata. The catalog is
 * cached to lib/data/drops.raw and regenerated when any relevant edit file
 * changes. All selection happens from this catalog using the difficulty-band
 * rules described in the design brief.
 */

typedef enum
{
    DROP_CAT_WEAPON = 0,
    DROP_CAT_ARMOR = 1,
    DROP_CAT_JEWELRY = 2,
    DROP_CAT_SUPPLY = 3,
    DROP_CAT_MAX = 4
} drop_category;

typedef enum
{
    DROP_GROUP_NORMAL = 0,
    DROP_GROUP_EGO = 1,
    DROP_GROUP_ARTIFACT = 2
} drop_group_kind;

#define DROP_ALLOC_MAX 8

typedef struct
{
    object_type obj; /* fully specified object template */
    drop_category category;
    drop_group_kind group_kind;
    s16b group_id; /* k_idx for normal, e_idx for ego, a_idx for artifact */
    s16b difficulty; /* smithing difficulty (baseline, penalised separately) */
    s16b min_depth;
    s16b max_depth;
    byte num_allocations; /* number of depth/rarity allocation pairs */
    byte alloc_depth[DROP_ALLOC_MAX]; /* depth thresholds where rarity changes */
    byte alloc_rarity[DROP_ALLOC_MAX]; /* rarity value from this depth onward (0 allowed) */
} drop_entry;

static drop_entry* g_drop_entries = NULL;
static size_t g_drop_count = 0;
static size_t g_drop_capacity = 0;

/* Jinx egos are excluded from normal drops but applied probabilistically */
static const s16b jinx_egos[] = {
    /* EGO_FLICKERING_SHADOW,  -- temporarily disabled */
    -1  /* sentinel */
};

static const char* DROP_RAW_FILE = "drops";
static const u32b DROP_RAW_MAGIC = 0x44525053; /* 'DRPS' */
static const u32b DROP_RAW_VERSION = 6;

typedef struct
{
    u32b magic;
    u32b version;
    u32b count;
} drop_raw_header;

static const char* drop_quality_name(drop_quality quality)
{
    switch (quality)
    {
    case DROP_QUALITY_GOOD:
        return "good";
    case DROP_QUALITY_GREAT:
        return "great";
    case DROP_QUALITY_SUPERB:
        return "superb";
    case DROP_QUALITY_NORMAL:
    default:
        return "normal";
    }
}

static int drop_quality_bonus(drop_quality quality)
{
    switch (quality)
    {
    case DROP_QUALITY_GOOD:
        return DROP_BONUS_GOOD;
    case DROP_QUALITY_GREAT:
        return DROP_BONUS_GREAT;
    case DROP_QUALITY_SUPERB:
        return DROP_BONUS_SUPERB;
    case DROP_QUALITY_NORMAL:
    default:
        return 0;
    }
}

drop_quality drop_quality_from_flags(bool good, bool great)
{
    if (great)
        return DROP_QUALITY_GREAT;
    if (good)
        return DROP_QUALITY_GOOD;
    return DROP_QUALITY_NORMAL;
}

#define DROP_DEFAULT_CAT_WEIGHT 25
#define DROP_DEFAULT_SUPPLY_WEIGHT 1

void drop_profile_default(drop_profile* profile)
{
    if (!profile)
        return;

    profile->weight_weapon = DROP_DEFAULT_CAT_WEIGHT;
    profile->weight_armor = DROP_DEFAULT_CAT_WEIGHT;
    profile->weight_jewelry = DROP_DEFAULT_CAT_WEIGHT;
    profile->weight_supply = DROP_DEFAULT_CAT_WEIGHT;
    profile->supply_potion = DROP_DEFAULT_SUPPLY_WEIGHT;
    profile->supply_herb = DROP_DEFAULT_SUPPLY_WEIGHT;
    profile->supply_gem = DROP_DEFAULT_SUPPLY_WEIGHT;
    profile->supply_staff = DROP_DEFAULT_SUPPLY_WEIGHT;
    profile->supply_misc = DROP_DEFAULT_SUPPLY_WEIGHT;
}

/* ------------------------------------------------------------------------ */
/* Helpers                                                                  */
/* ------------------------------------------------------------------------ */

static drop_category drop_category_for_kind(const object_kind* k_ptr)
{
    switch (k_ptr->tval)
    {
    case TV_ARROW:
        /* Base category is weapon (for ego arrows); normal arrows overridden to supply in add_drop_entry */
        return DROP_CAT_WEAPON;
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_BOW:
    case TV_DIGGING:
        return DROP_CAT_WEAPON;
    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
        return DROP_CAT_ARMOR;
    case TV_RING:
    case TV_AMULET:
        return DROP_CAT_JEWELRY;
    case TV_LIGHT:
        /* All non-Feanorian light sources are treated as supply (misc/torches). */
        if (k_ptr->sval == SV_LIGHT_FEANORIAN || k_ptr->sval == SV_LIGHT_SILMARIL)
            return DROP_CAT_JEWELRY;
        if (k_ptr->sval == SV_LIGHT_TORCH || k_ptr->sval == SV_LIGHT_MALLORN
            || k_ptr->sval == SV_LIGHT_LANTERN || k_ptr->sval == SV_LIGHT_LESSER_JEWEL)
            return DROP_CAT_SUPPLY;
        return DROP_CAT_MAX;
    case TV_HORN:
        return DROP_CAT_JEWELRY;
    case TV_POTION:
    case TV_STAFF:
    case TV_GEM:
    case TV_FOOD: /* Herbs */
    case TV_FLASK:
        return DROP_CAT_SUPPLY;
    default:
        return DROP_CAT_MAX;
    }
}

static int min_locale_depth(const object_kind* k_ptr)
{
    int min_depth = k_ptr->level;
    for (int i = 0; i < 4; i++)
    {
        if (k_ptr->locale[i] && (k_ptr->locale[i] < min_depth || min_depth == 0))
            min_depth = k_ptr->locale[i];
    }
    if (min_depth <= 0)
        min_depth = 1;
    return min_depth;
}

static int max_locale_depth(const object_kind* k_ptr)
{
    /*
     * CRITICAL: Return 0 to indicate NO max depth restriction.
     * The locale array indicates where items spawn naturally via allocation,
     * but the drop system should NOT be limited by these depths.
     */
    (void)k_ptr;
    return 0;
}

static void sort_allocations(byte* depths, byte* rarities, int count)
{
    for (int i = 1; i < count; i++)
    {
        for (int j = i; j > 0; j--)
        {
            if (depths[j] < depths[j - 1])
            {
                byte tmp_d = depths[j];
                byte tmp_r = rarities[j];
                depths[j] = depths[j - 1];
                rarities[j] = rarities[j - 1];
                depths[j - 1] = tmp_d;
                rarities[j - 1] = tmp_r;
            }
            else
            {
                break;
            }
        }
    }
}

static int collect_kind_allocations(const object_kind* k_ptr, byte* depths, byte* rarities)
{
    int count = 0;

    if (k_ptr->alloc_count > 0)
    {
        for (int i = 0; i < k_ptr->alloc_count && i < 4; i++)
        {
            depths[count] = k_ptr->alloc_depth[i];
            rarities[count] = k_ptr->alloc_prob[i];
            count++;
        }
    }
    else
    {
        for (int i = 0; i < 4; i++)
        {
            if (k_ptr->chance[i])
            {
                depths[count] = k_ptr->locale[i];
                rarities[count] = k_ptr->chance[i];
                count++;
            }
        }
    }

    sort_allocations(depths, rarities, count);
    return count;
}

static int collect_ego_allocations(const ego_item_type* e_ptr, byte* depths, byte* rarities)
{
    int count = 0;

    if (e_ptr->alloc_count > 0)
    {
        for (int i = 0; i < e_ptr->alloc_count && i < 4; i++)
        {
            depths[count] = e_ptr->alloc_depth[i];
            rarities[count] = e_ptr->alloc_prob[i];
            count++;
        }
    }

    sort_allocations(depths, rarities, count);
    return count;
}

static int schedule_min_depth(const byte* depths, int count, int fallback)
{
    if (count <= 0)
        return fallback;
    int min_depth = depths[0];
    for (int i = 1; i < count; i++)
    {
        if (depths[i] < min_depth)
            min_depth = depths[i];
    }
    return (min_depth > 0) ? min_depth : fallback;
}

static int schedule_max_depth_cap(const byte* depths, const byte* rarities, int count)
{
    int last_positive = -1;
    for (int i = 0; i < count; i++)
    {
        if (rarities[i] > 0)
            last_positive = i;
    }
    if (last_positive >= 0 && last_positive + 1 < count && rarities[last_positive + 1] == 0)
        return depths[last_positive + 1] - 1;
    if (last_positive < 0)
        return -1; /* all zero rarities */
    return 0; /* no cap */
}

static int rarity_from_schedule(const byte* depths, const byte* rarities, int count,
    int depth, int default_rarity)
{
    if (count <= 0)
        return default_rarity;

    int first_depth = depths[0];
    if (first_depth > 0 && depth < first_depth)
        return 0;

    int rarity = rarities[0];
    for (int i = 1; i < count; i++)
    {
        if (depth >= depths[i])
            rarity = rarities[i];
        else
            break;
    }
    return rarity;
}

static int combine_allocations(const byte* base_depths, const byte* base_rarities, int base_count,
    const byte* ego_depths, const byte* ego_rarities, int ego_count,
    byte* out_depths, byte* out_rarities)
{
    byte merged[DROP_ALLOC_MAX];
    int merged_count = 0;

    for (int i = 0; i < base_count && merged_count < DROP_ALLOC_MAX; i++)
    {
        bool exists = false;
        for (int j = 0; j < merged_count; j++)
        {
            if (merged[j] == base_depths[i])
            {
                exists = true;
                break;
            }
        }
        if (!exists)
            merged[merged_count++] = base_depths[i];
    }

    for (int i = 0; i < ego_count && merged_count < DROP_ALLOC_MAX; i++)
    {
        bool exists = false;
        for (int j = 0; j < merged_count; j++)
        {
            if (merged[j] == ego_depths[i])
            {
                exists = true;
                break;
            }
        }
        if (!exists)
            merged[merged_count++] = ego_depths[i];
    }

    for (int i = 0; i < merged_count; i++)
    {
        for (int j = i + 1; j < merged_count; j++)
        {
            if (merged[j] < merged[i])
            {
                byte tmp = merged[i];
                merged[i] = merged[j];
                merged[j] = tmp;
            }
        }
    }

    int out_count = 0;
    for (int i = 0; i < merged_count && out_count < DROP_ALLOC_MAX; i++)
    {
        int depth = merged[i];
        int base_r = rarity_from_schedule(base_depths, base_rarities, base_count, depth, 1);
        int ego_r = rarity_from_schedule(ego_depths, ego_rarities, ego_count, depth, 1);
        int combined = base_r * ego_r;
        if (combined < 0)
            combined = 0;
        if (out_count == 0 || combined != out_rarities[out_count - 1])
        {
            out_depths[out_count] = (byte)depth;
            out_rarities[out_count] = (byte)MIN(combined, 255);
            out_count++;
        }
    }

    return out_count;
}

/* Restore runtime quantities (fuel, charges, stacks) that were handled by apply_magic previously. */
static void drop_apply_spawn_quantities(object_type* o_ptr)
{
    object_kind* k_ptr = &k_info[o_ptr->k_idx];

    switch (o_ptr->tval)
    {
    case TV_LIGHT:
    {
        /* Only adjust empty/zero-fuel lights */
        if (o_ptr->timeout <= 0)
        {
            if (o_ptr->sval == SV_LIGHT_TORCH)
            {
                o_ptr->timeout = one_in_(3) ? rand_range(500, 2000) : 2000;
            }
            else if (o_ptr->sval == SV_LIGHT_LANTERN)
            {
                o_ptr->timeout = one_in_(3) ? rand_range(500, 3000) : 3000;
            }
            else if (o_ptr->sval == SV_LIGHT_MALLORN)
            {
                o_ptr->timeout = one_in_(3) ? rand_range(20, 50) : 50;
            }
        }
        break;
    }
    case TV_STAFF:
    {
        int mult = CHANNELING_CHARGE_MULTIPLIER;
        switch (o_ptr->sval)
        {
        case SV_STAFF_SECRETS:
        case SV_STAFF_IMPRISONMENT:
        case SV_STAFF_FREEDOM:
        case SV_STAFF_LIGHT:
        case SV_STAFF_REVELATIONS:
        case SV_STAFF_FOES:
        case SV_STAFF_SLUMBER:
        case SV_STAFF_MAJESTY:
            o_ptr->pval = mult * damroll(4, 2);
            break;
        case SV_STAFF_SANCTITY:
        case SV_STAFF_UNDERSTANDING:
        case SV_STAFF_TREASURES:
        case SV_STAFF_SELF_KNOWLEDGE:
        case SV_STAFF_DISMAY:
        case SV_STAFF_RECHARGING:
            o_ptr->pval = mult * damroll(2, 2);
            break;
        case SV_STAFF_SUMMONING:
            o_ptr->pval = mult * damroll(6, 2);
            break;
        default:
            o_ptr->pval = mult * damroll(2, 2);
            break;
        }
        break;
    }
    case TV_GEM:
    {
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
        o_ptr->pval = 0;
        break;
    }
    default:
        break;
    }

    /* Throwing weapons can spawn in small stacks and should use base weight for stacking. */
    if ((k_ptr->flags3 & TR3_THROWING) && o_ptr->tval != TV_ARROW && !o_ptr->name1)
    {
        o_ptr->weight = k_ptr->weight;
        if (one_in_(2))
        {
            int stack_limit = object_stack_limit(o_ptr);
            int max_spawn = (stack_limit < 5) ? stack_limit : 5;
            int min_spawn = (max_spawn < 2) ? 1 : 2;
            o_ptr->number = rand_range(min_spawn, max_spawn);
        }
    }
}

/* Baseline smithing difficulty (player-neutral). */
static void drop_dif_mod(int value, int positive_base, int* dif_inc)
{
    int mod = 1 + ((positive_base - 1) / 5);
    if (value > 0)
        *dif_inc += positive_base * value + mod * (value * (value - 1) / 2);
}

/* Slot determination without using inventory state (only for difficulty multiplier) */
static s16b neutral_wield_slot(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
        return INVEN_WIELD;
    case TV_BOW:
        return INVEN_BOW;
    case TV_STAFF:
        return INVEN_STAFF;
    case TV_RING:
        return INVEN_LEFT;
    case TV_AMULET:
        return INVEN_NECK;
    case TV_LIGHT:
        return INVEN_LITE;
    case TV_MAIL:
    case TV_SOFT_ARMOR:
        return INVEN_BODY;
    case TV_CLOAK:
        return INVEN_OUTER;
    case TV_SHIELD:
        return INVEN_ARM;
    case TV_CROWN:
    case TV_HELM:
        return INVEN_HEAD;
    case TV_GLOVES:
        return INVEN_HANDS;
    case TV_BOOTS:
        return INVEN_FEET;
    case TV_ARROW:
        return INVEN_QUIVER1;
    default:
        break;
    }
    return -1;
}

static int smithing_difficulty_baseline(object_type* o_ptr)
{
    object_kind* k_ptr = &k_info[o_ptr->k_idx];
    int x, newv, base;
    int dif_inc = 0;
    int dif_dec = 0;
    int weight_factor;
    u32b f1, f2, f3;
    int brands = 0;
    int dif_mult = 100;

    /* Extract flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    /* Base handling for non-jewelry: add base level */
    if (o_ptr->tval != TV_RING && o_ptr->tval != TV_AMULET)
    {
        /* Note: We do NOT strip base flags anymore. 
           We want the total difficulty/value of the item, including its intrinsic properties.
           This ensures high-tier base items (like Mithril) have appropriate difficulty. */

        dif_inc += k_ptr->level / 2;
    }

    /* Weight variance */
    if (o_ptr->weight == 0)
        weight_factor = 1100;
    else if (o_ptr->weight > k_ptr->weight)
        weight_factor = 100 * o_ptr->weight / k_ptr->weight;
    else
        weight_factor = 100 * k_ptr->weight / o_ptr->weight;
    dif_inc += (weight_factor - 100) / 20;

    /* Attack bonus */
    x = o_ptr->att - k_ptr->att;
    if ((o_ptr->tval == TV_ARROW) && (x > 0))
    {
        int old_di = dif_inc;
        drop_dif_mod(x, 5, &dif_inc);
        dif_inc = (dif_inc - old_di) / 2;
    }
    else if ((o_ptr->tval == TV_BOW || o_ptr->tval == TV_SWORD
                 || o_ptr->tval == TV_POLEARM || o_ptr->tval == TV_HAFTED)
        && (x > 0))
    {
        drop_dif_mod(x, 3, &dif_inc);
    }
    else
    {
        drop_dif_mod(x, 6, &dif_inc);
        if (x > 0)
            dif_inc -= 1;
    }

    /* Evasion bonus */
    x = o_ptr->evn - k_ptr->evn;
    drop_dif_mod(x, 6, &dif_inc);
    if (x > 0)
        dif_inc -= 1;

    /* Damage bonus */
    x = (o_ptr->ds - k_ptr->ds);
    drop_dif_mod(x, 3 * x + 2, &dif_inc);

    /* Protection bonus */
    base = (k_ptr->ps > 0) ? ((k_ptr->ps + 1) * k_ptr->pd) : 0;
    newv = (o_ptr->ps > 0) ? ((o_ptr->ps + 1) * o_ptr->pd) : 0;
    x = newv - base;

    if ((o_ptr->tval == TV_MAIL) && (o_ptr->sval == SV_LONG_CORSLET) && (x > 0))
    {
        drop_dif_mod(x, 1, &dif_inc);
        dif_inc += 2;
    }
    else if ((o_ptr->tval == TV_RING) && (x > 0))
    {
        drop_dif_mod(x, 1, &dif_inc);
        dif_inc += 4;
    }
    else
    {
        drop_dif_mod(x, 3, &dif_inc);
    }

    /* Slays and brands */
    if (f1 & TR1_SLAY_ORC)
        dif_inc += 3;
    if (f1 & TR1_SLAY_TROLL)
        dif_inc += 3;
    if (f1 & TR1_SLAY_WOLF)
        dif_inc += 3;
    if (f1 & TR1_SLAY_SPIDER)
        dif_inc += 4;
    if (f1 & TR1_SLAY_UNDEAD)
        dif_inc += 3;
    if (f1 & TR1_SLAY_RAUKO)
        dif_inc += 4;
    if (f1 & TR1_SLAY_DRAGON)
        dif_inc += 4;
    if (f1 & TR1_SLAY_MAN_OR_ELF)
        dif_inc += 5;

    if (f1 & TR1_BRAND_COLD)
    {
        dif_inc += 18;
        brands++;
    }
    if (f1 & TR1_BRAND_FIRE)
    {
        dif_inc += 14;
        brands++;
    }
    if (f1 & TR1_BRAND_POIS)
    {
        if (o_ptr->tval == TV_ARROW)
            dif_inc += 12;
        else
        {
            dif_inc += 16;
            brands++;
        }
    }
    if (brands > 1)
        dif_inc += (brands - 1) * 20;

    if (f1 & TR1_SHARPNESS)
        dif_inc += (o_ptr->tval == TV_ARROW) ? 14 : 24;
    if (f1 & TR1_SHARPNESS2)
        dif_inc += 40;
    if (f1 & TR1_VAMPIRIC)
        dif_inc += 6;
    if (f3 & TR3_ACCURATE)
        dif_inc += 15;

    /* pval-based bonuses */
    if (f1 & TR1_TUNNEL)
    {
        x = o_ptr->pval;
        drop_dif_mod(x, 8, &dif_inc);
    }
    if (o_ptr->pval != 0)
    {
        x = (o_ptr->pval > 0) ? o_ptr->pval : 0;

        if (f1 & TR1_DAMAGE_SIDES)
            drop_dif_mod(x, 18, &dif_inc);
        if (f1 & TR1_STR)
            drop_dif_mod(x, 14, &dif_inc);
        if (f1 & TR1_DEX)
            drop_dif_mod(x, 14, &dif_inc);
        if (f1 & TR1_CON)
            drop_dif_mod(x, 14, &dif_inc);
        if (f1 & TR1_GRA)
            drop_dif_mod(x, 14, &dif_inc);
        if (f1 & TR1_ARC)
            drop_dif_mod(x, 4, &dif_inc);
        if (f1 & TR1_STL)
            drop_dif_mod(x, 4, &dif_inc);
        if (f1 & TR1_PER)
            drop_dif_mod(x, 3, &dif_inc);
        if (f1 & TR1_WIL)
            drop_dif_mod(x, 3, &dif_inc);
        if (f1 & TR1_SNG)
            drop_dif_mod(x, 4, &dif_inc);

        x = (o_ptr->pval < 0) ? o_ptr->pval : 0;

        if (f1 & TR1_NEG_STR)
            drop_dif_mod(-x, 12, &dif_inc);
        if (f1 & TR1_NEG_DEX)
            drop_dif_mod(-x, 12, &dif_inc);
        if (f1 & TR1_NEG_CON)
            drop_dif_mod(-x, 12, &dif_inc);
        if (f1 & TR1_NEG_GRA)
            drop_dif_mod(-x, 12, &dif_inc);
    }

    /* Sustains */
    if (f2 & TR2_SUST_STR)
        dif_inc += 2;
    if (f2 & TR2_SUST_DEX)
        dif_inc += 2;
    if (f2 & TR2_SUST_CON)
        dif_inc += 2;
    if (f2 & TR2_SUST_GRA)
        dif_inc += 2;

    /* Abilities / misc flags */
    if (f2 & TR2_SLOW_DIGEST)
        dif_inc += 2;
    if (f2 & TR2_RADIANCE)
        dif_inc += 6;
    if (f2 & TR2_LIGHT)
        dif_inc += 8;
    if (f2 & TR2_REGEN)
        dif_inc += 4;
    if (f2 & TR2_SEE_INVIS)
        dif_inc += 4;
    if (f2 & TR2_FREE_ACT)
        dif_inc += 7;
    if (f2 & TR2_SPEED)
        dif_inc += 40;
    if (f3 & TR3_CHEAT_DEATH)
        dif_inc += 13;
    if (f3 & TR3_STAND_FAST)
        dif_inc += 2;
    if (f3 & TR3_AVOID_TRAPS)
        dif_inc += 6;
    if (f3 & TR3_MEDIC)
        dif_inc += 4;

    if (f2 & TR2_RES_COLD)
        dif_inc += 5;
    if (f2 & TR2_RES_FIRE)
        dif_inc += 5;
    if (f2 & TR2_RES_POIS)
        dif_inc += 5;

    if (f2 & TR2_RES_BLEED)
        dif_inc += 1;
    if (f2 & TR2_RES_BLIND)
        dif_inc += 2;
    if (f2 & TR2_RES_CONFU)
        dif_inc += 2;
    if (f2 & TR2_RES_STUN)
        dif_inc += 2;
    if (f2 & TR2_RES_FEAR)
        dif_inc += 2;
    if (f2 & TR2_RES_HALLU)
        dif_inc += 1;

    /* Penalties (only for non-artefact) */
    if (!o_ptr->name1)
    {
        if (f2 & TR2_DANGER)
            dif_dec += 5;
        if (f2 & TR2_DARKNESS)
            dif_dec += 3;
        if (f2 & TR2_AGGRAVATE)
            dif_dec += 3;
        if (f2 & TR2_HAUNTED)
            dif_dec += 5;
        if (f2 & TR2_VUL_COLD)
            dif_dec += 4;
        if (f2 & TR2_VUL_FIRE)
            dif_dec += 4;
        if (f2 & TR2_VUL_POIS)
            dif_dec += 4;
        if (f3 & TR2_TRAITOR)
            dif_dec += 2;
        if (f3 & TR3_LIGHT_CURSE)
            dif_dec += 2;
        if (f3 & TR3_CUMBERSOME)
            dif_dec += 3;
    }

    /* Abilities */
    for (int i = 0; i < o_ptr->abilities; i++)
    {
        int level = (&b_info[ability_index(
                         o_ptr->skilltype[i], o_ptr->abilitynum[i])])
                        ->level;
        dif_inc += 5 + (level / 3);
    }

    int dif = dif_inc - dif_dec;

    /* Minor slot multiplier */
    switch (neutral_wield_slot(o_ptr))
    {
    case INVEN_LEFT:
    case INVEN_RIGHT:
    case INVEN_LITE:
    case INVEN_OUTER:
    case INVEN_HANDS:
    case INVEN_FEET:
    case INVEN_QUIVER1:
    case INVEN_QUIVER2:
        dif_mult += 20;
        break;
    default:
        break;
    }

    if (k_ptr->flags3 & TR3_ENCHANTABLE)
        dif_mult -= 30;

    dif = dif * dif_mult / 100;

    if ((o_ptr->tval == TV_ARROW) && (o_ptr->name1))
        dif /= 2;

    if (dif < 0)
        dif = 0;
    if (dif > 255)
        dif = 255;
    return dif;
}

static void add_drop_entry(const object_type* proto, drop_category cat,
    drop_group_kind group_kind, int group_id, int min_depth, int max_depth,
    const byte* alloc_depths, const byte* alloc_rarities, int num_allocs)
{
    object_kind* k_ptr = &k_info[proto->k_idx];

    /* Never allow INSTA_ART templates except as true artefacts */
    if ((k_ptr->flags3 & TR3_INSTA_ART) && group_kind != DROP_GROUP_ARTIFACT)
        return;

    /* Override category: simple arrows go to supply (egos go to weapon) */
    if (group_kind == DROP_GROUP_NORMAL && k_ptr->tval == TV_ARROW)
        cat = DROP_CAT_SUPPLY;

    /* Special case: Lesser Jewel of Grace stays in jewelry */
    if (k_ptr->tval == TV_LIGHT && k_ptr->sval == SV_LIGHT_LESSER_JEWEL
        && proto->name2 == EGO_GRACE)
    {
        cat = DROP_CAT_JEWELRY;
    }

    if (g_drop_count + 1 > g_drop_capacity)
    {
        size_t new_cap = (g_drop_capacity == 0) ? 1024 : g_drop_capacity * 2;
        if (new_cap < g_drop_count + 1)
            new_cap = g_drop_count + 1;
        drop_entry* new_buf = mem_alloc_array(new_cap, drop_entry);
        if (g_drop_entries && g_drop_count)
            memcpy(new_buf, g_drop_entries, g_drop_count * sizeof(drop_entry));
        mem_free_null(g_drop_entries);
        g_drop_entries = new_buf;
        g_drop_capacity = new_cap;
    }

    drop_entry* entry = &g_drop_entries[g_drop_count++];
    object_copy(&entry->obj, proto);
    entry->category = cat;
    entry->group_kind = group_kind;
    entry->group_id = (s16b)group_id;
    entry->min_depth = (s16b)min_depth;
    entry->max_depth = (s16b)max_depth;
    entry->num_allocations = (byte)MIN(num_allocs, DROP_ALLOC_MAX);
    for (int i = 0; i < entry->num_allocations; i++)
    {
        entry->alloc_depth[i] = alloc_depths[i];
        entry->alloc_rarity[i] = alloc_rarities[i];
    }
    if (cat == DROP_CAT_SUPPLY)
        entry->difficulty = 0;
    else
        entry->difficulty = (s16b)smithing_difficulty_baseline(&entry->obj);
}

/* Apply ego flag data (abilities and curses) without randomness */
/* Check if an ego is a jinx ego */
static bool is_jinx_ego(int e_idx)
{
    for (int i = 0; jinx_egos[i] >= 0; i++)
    {
        if (jinx_egos[i] == e_idx)
            return true;
    }
    return false;
}

static void apply_ego_static(object_type* o_ptr, ego_item_type* e_ptr)
{
    // abilities
    for (int i = 0; i < e_ptr->abilities; i++)
    {
        o_ptr->skilltype[i + o_ptr->abilities] = e_ptr->skilltype[i];
        o_ptr->abilitynum[i + o_ptr->abilities] = e_ptr->abilitynum[i];
    }
    o_ptr->abilities += e_ptr->abilities;

    // cursed / broken flags
    if (!e_ptr->cost)
        o_ptr->ident |= (IDENT_BROKEN);
    if (e_ptr->flags3 & (TR3_LIGHT_CURSE))
        o_ptr->ident |= (IDENT_CURSED);
}

/* Build variants for a base object (normal item). */
static void build_normal_variants(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];
    /* Skip pure artifact templates; they should only appear via artefact entries */
    if (k_ptr->flags3 & TR3_INSTA_ART)
        return;

    drop_category cat = drop_category_for_kind(k_ptr);
    if (cat == DROP_CAT_MAX)
        return;

    object_type base;
    object_prep(&base, k_idx);
    base.weight = k_ptr->weight;

    byte alloc_depths[DROP_ALLOC_MAX];
    byte alloc_rarities[DROP_ALLOC_MAX];
    int num_allocations = collect_kind_allocations(k_ptr, alloc_depths, alloc_rarities);
    int fallback_min = min_locale_depth(k_ptr);
    if (fallback_min <= 0)
        fallback_min = 1;
    if (num_allocations == 0)
    {
        alloc_depths[0] = (byte)fallback_min;
        alloc_rarities[0] = 1;
        num_allocations = 1;
    }

    bool has_positive_rarity = false;
    for (int i = 0; i < num_allocations; i++)
    {
        if (alloc_rarities[i] > 0)
        {
            has_positive_rarity = true;
            break;
        }
    }
    int rarity_cap_depth = schedule_max_depth_cap(alloc_depths, alloc_rarities, num_allocations);
    if (!has_positive_rarity && rarity_cap_depth < 0)
        return; /* never spawns */

    int min_depth = schedule_min_depth(alloc_depths, num_allocations, fallback_min);
    int max_depth = max_locale_depth(k_ptr);
    if (min_depth <= 0)
        min_depth = 1;
    if (rarity_cap_depth > 0 && (max_depth == 0 || rarity_cap_depth < max_depth))
        max_depth = rarity_cap_depth;

    drop_group_kind group_kind = (cat == DROP_CAT_JEWELRY) ? DROP_GROUP_EGO : DROP_GROUP_NORMAL;

    /* Supply items: no smithing variants, use new allocation semantics */
    if (cat == DROP_CAT_SUPPLY)
    {
        add_drop_entry(&base, cat, DROP_GROUP_NORMAL, k_idx, min_depth, max_depth,
            alloc_depths, alloc_rarities, num_allocations);
        return;
    }

    /* Smithing caps (no ego/artefact) taken from smithing menu logic */
    int att_min = k_ptr->att;
    int att_max = k_ptr->att;
    int ds_min = k_ptr->ds;
    int ds_max = k_ptr->ds;
    int evn_min = k_ptr->evn;
    int evn_max = k_ptr->evn;
    int ps_min = k_ptr->ps;
    int ps_max = k_ptr->ps;
    int pval_min = k_ptr->pval;
    int pval_max = k_ptr->pval;
    bool pval_allowed = (k_ptr->flags1 & TR1_PVAL_MASK) != 0 || k_ptr->pval != 0;

    switch (k_ptr->tval)
    {
    case TV_SWORD:
    case TV_POLEARM:
    case TV_HAFTED:
    case TV_DIGGING:
    case TV_BOW:
        att_max = k_ptr->att + 1;
        ds_max = k_ptr->ds + 1;
        break;
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
        att_max = k_ptr->att + 1;
        if (att_max > 0)
            att_max = 0;
        evn_max = k_ptr->evn + 1;
        ps_max = k_ptr->ps + 1;
        if ((k_ptr->tval == TV_CLOAK)
            || (k_ptr->tval == TV_SOFT_ARMOR && k_ptr->sval == SV_ROBE))
            ps_max = 0;
        if (k_ptr->tval == TV_MAIL && k_ptr->sval == SV_LONG_CORSLET)
            ps_max = k_ptr->ps + 2;
        break;
    case TV_RING:
        if (k_ptr->sval == SV_RING_ACCURACY)
        {
            att_max = 4;
            att_min = 1;
        }
        else
        {
            att_max = k_ptr->att;
        }
        if (k_ptr->sval == SV_RING_EVASION)
        {
            evn_max = 4;
            evn_min = 1;
        }
        else
        {
            evn_max = k_ptr->evn;
        }
        if (k_ptr->sval == SV_RING_PROTECTION)
        {
            /* Protection rings are always 1dX (never 0dX). */
            base.pd = 1;
            ps_max = 3;
            ps_min = 1;
        }
        if (pval_allowed)
        {
            if (pval_min < 1)
                pval_min = 1;
            pval_max = 4; /* smithing caps ring/amulet pval at 4 */
        }
        break;
    case TV_AMULET:
        if (pval_allowed)
        {
            if (pval_min < 1)
                pval_min = 1;
            pval_max = 4; /* smithing caps ring/amulet pval at 4 */
        }
        break;
    default:
        break;
    }

    // Variant list (all combinations within smithing caps)
    // Use combined rarity and minimum depth for the entire item
    for (int att = att_min; att <= att_max; att++)
    {
        for (int ds = ds_min; ds <= ds_max; ds++)
        {
            for (int evn = evn_min; evn <= evn_max; evn++)
            {
                for (int ps = ps_min; ps <= ps_max; ps++)
                {
                    int pval_hi = pval_allowed ? pval_max : pval_min;
                    for (int pval = pval_min; pval <= pval_hi; pval++)
                    {
                        object_type v = base;
                        v.att = att;
                        v.ds = ds;
                        v.evn = evn;
                        v.ps = ps;
                        v.pval = pval;
                        add_drop_entry(&v, cat, group_kind, k_idx,
                            min_depth, max_depth,
                            alloc_depths, alloc_rarities, num_allocations);
                    }
                }
            }
        }
    }
}

/* Build variants for ego items over applicable base kinds. */
static void build_ego_variants(int e_idx)
{
    ego_item_type* e_ptr = &e_info[e_idx];
    if (!e_ptr->tval[0])
        return;
    
    /* Skip jinx egos - they are applied separately, not in normal drops */
    if (is_jinx_ego(e_idx))
        return;

    for (int t = 0; t < EGO_TVALS_MAX; t++)
    {
        if (!e_ptr->tval[t])
            continue;
        for (int k_idx = 1; k_idx < z_info->k_max; k_idx++)
        {
            object_kind* k_ptr = &k_info[k_idx];
            if (k_ptr->tval != e_ptr->tval[t])
                continue;
            if (k_ptr->sval < e_ptr->min_sval[t] || k_ptr->sval > e_ptr->max_sval[t])
                continue;
            if (k_ptr->flags3 & TR3_INSTA_ART)
                continue;

            drop_category cat = drop_category_for_kind(k_ptr);
            if (cat == DROP_CAT_MAX)
                continue;

            object_type base;
            object_prep(&base, k_idx);
            base.weight = k_ptr->weight;
            base.name2 = e_idx;
            apply_ego_static(&base, e_ptr);

            /* Ego items: use ego W: depth for min_depth (for difficulty penalty) */
            int ego_fallback_depth = (e_ptr->level > 0) ? e_ptr->level : 1;
            int max_depth = (e_ptr->max_level > 0) ? e_ptr->max_level
                                                   : max_locale_depth(k_ptr);

            byte base_depths[DROP_ALLOC_MAX];
            byte base_rarities[DROP_ALLOC_MAX];
            int base_allocs = collect_kind_allocations(k_ptr, base_depths, base_rarities);
            int base_fallback_depth = min_locale_depth(k_ptr);
            if (base_fallback_depth <= 0)
                base_fallback_depth = 1;
            if (base_allocs == 0)
            {
                base_depths[0] = (byte)base_fallback_depth;
                base_rarities[0] = 1;
                base_allocs = 1;
            }

            byte ego_depths[DROP_ALLOC_MAX];
            byte ego_rarities[DROP_ALLOC_MAX];
            int ego_allocs = collect_ego_allocations(e_ptr, ego_depths, ego_rarities);
            int ego_default_rarity = (e_ptr->rarity > 0) ? e_ptr->rarity : 1;
            if (ego_allocs == 0)
            {
                ego_depths[0] = (byte)ego_fallback_depth;
                ego_rarities[0] = (byte)ego_default_rarity;
                ego_allocs = 1;
            }

            byte alloc_depths[DROP_ALLOC_MAX];
            byte alloc_rarities[DROP_ALLOC_MAX];
            int num_allocations = combine_allocations(
                base_depths, base_rarities, base_allocs,
                ego_depths, ego_rarities, ego_allocs,
                alloc_depths, alloc_rarities);

            if (num_allocations == 0)
                continue;

            bool has_positive_rarity = false;
            for (int i = 0; i < num_allocations; i++)
            {
                if (alloc_rarities[i] > 0)
                {
                    has_positive_rarity = true;
                    break;
                }
            }
            if (!has_positive_rarity && num_allocations > 0
                && schedule_max_depth_cap(alloc_depths, alloc_rarities, num_allocations) < 0)
            {
                continue;
            }

            int base_min_depth = schedule_min_depth(base_depths, base_allocs, base_fallback_depth);
            int min_depth = schedule_min_depth(ego_depths, ego_allocs, ego_fallback_depth);
            if (base_min_depth <= 0)
                base_min_depth = 1;
            if (min_depth < base_min_depth)
                min_depth = base_min_depth;
            int rarity_cap_depth = schedule_max_depth_cap(alloc_depths, alloc_rarities, num_allocations);
            if (min_depth <= 0)
                min_depth = 1;
            if (rarity_cap_depth > 0 && (max_depth == 0 || rarity_cap_depth < max_depth))
                max_depth = rarity_cap_depth;

            /* Smithing bounds with ego applied (match smithing UI logic) */
            int att_min = k_ptr->att + ((e_ptr->max_att > 0) ? 1 : 0);
            int att_max = k_ptr->att;
            int ds_min = k_ptr->ds + ((e_ptr->to_ds > 0) ? 1 : 0);
            int ds_max = k_ptr->ds;
            int evn_min = k_ptr->evn + ((e_ptr->max_evn > 0) ? 1 : 0);
            int evn_max = k_ptr->evn;
            int ps_min = k_ptr->ps + ((e_ptr->to_ps > 0) ? 1 : 0);
            int ps_max = k_ptr->ps;
            int pval_min = k_ptr->pval + ((e_ptr->max_pval > 0) ? 1 : 0);
            int pval_max = k_ptr->pval + e_ptr->max_pval;
            int dd_min = k_ptr->dd + ((e_ptr->to_dd > 0) ? 1 : 0);
            int dd_max = k_ptr->dd + e_ptr->to_dd;
            int pd_min = k_ptr->pd + ((e_ptr->to_pd > 0) ? 1 : 0);
            int pd_max = k_ptr->pd + e_ptr->to_pd;
            bool pval_allowed = ((k_ptr->flags1 & TR1_PVAL_MASK) != 0)
                || (k_ptr->pval != 0) || (e_ptr->max_pval > 0);

            switch (k_ptr->tval)
            {
            case TV_SWORD:
            case TV_POLEARM:
            case TV_HAFTED:
            case TV_DIGGING:
            case TV_BOW:
                att_max = k_ptr->att + 1 + e_ptr->max_att;
                ds_max = k_ptr->ds + 1 + e_ptr->to_ds;
                evn_max = k_ptr->evn + e_ptr->max_evn;
                break;
            case TV_BOOTS:
            case TV_GLOVES:
            case TV_HELM:
            case TV_CROWN:
            case TV_SHIELD:
            case TV_CLOAK:
            case TV_SOFT_ARMOR:
            case TV_MAIL:
                att_max = k_ptr->att + 1 + e_ptr->max_att;
                if (att_max > 0)
                    att_max = 0;
                evn_max = k_ptr->evn + 1 + e_ptr->max_evn;
                ps_max = k_ptr->ps + 1 + e_ptr->to_ps;
                if ((k_ptr->tval == TV_CLOAK)
                    || (k_ptr->tval == TV_SOFT_ARMOR && k_ptr->sval == SV_ROBE))
                    ps_max = 0;
                if (k_ptr->tval == TV_MAIL && k_ptr->sval == SV_LONG_CORSLET)
                    ps_max = k_ptr->ps + 2 + e_ptr->to_ps;
                break;
            case TV_RING:
                if (k_ptr->sval == SV_RING_ACCURACY)
                {
                    att_max = 4;
                    att_min = 1;
                }
                else
                {
                    att_max = k_ptr->att + e_ptr->max_att;
                }
                if (k_ptr->sval == SV_RING_EVASION)
                {
                    evn_max = 4;
                    evn_min = 1;
                }
                else
                {
                    evn_max = k_ptr->evn + e_ptr->max_evn;
                }
                if (k_ptr->sval == SV_RING_PROTECTION)
                {
                    /* Protection rings are always 1dX (never 0dX). */
                    if (pd_min < 1)
                        pd_min = 1;
                    if (pd_max < 1)
                        pd_max = 1;
                    ps_max = 3;
                    ps_min = 1;
                }
                if (pval_allowed)
                {
                    if (k_ptr->flags1 & TR1_PVAL_MASK)
                    {
                        if (pval_min < 1)
                            pval_min = 1;
                        if (pval_max < 4)
                            pval_max = 4;
                    }
                    if (pval_max > 4)
                        pval_max = 4;
                }
                break;
            case TV_AMULET:
                if (pval_allowed)
                {
                    if (k_ptr->flags1 & TR1_PVAL_MASK)
                    {
                        if (pval_min < 1)
                            pval_min = 1;
                        if (pval_max < 4)
                            pval_max = 4;
                    }
                    if (pval_max > 4)
                        pval_max = 4;
                }
                break;
            default:
                att_max = k_ptr->att + e_ptr->max_att;
                ds_max = k_ptr->ds + e_ptr->to_ds;
                evn_max = k_ptr->evn + e_ptr->max_evn;
                ps_max = k_ptr->ps + e_ptr->to_ps;
                break;
            }

            if (att_min > att_max)
                att_min = att_max;
            if (ds_min > ds_max)
                ds_min = ds_max;
            if (evn_min > evn_max)
                evn_min = evn_max;
            if (ps_min > ps_max)
                ps_min = ps_max;
            if (pval_min > pval_max)
                pval_min = pval_max;
            if (dd_min > dd_max)
                dd_min = dd_max;
            if (pd_min > pd_max)
                pd_min = pd_max;

            /* Generate variants using combined rarity and effective min depth */
            for (int att = att_min; att <= att_max; att++)
            {
                for (int ds = ds_min; ds <= ds_max; ds++)
                {
                    for (int evn = evn_min; evn <= evn_max; evn++)
                    {
                        for (int ps = ps_min; ps <= ps_max; ps++)
                        {
                            for (int pval = pval_min;
                                 pval <= (pval_allowed ? pval_max : pval_min); pval++)
                            {
                                for (int dd = dd_min; dd <= dd_max; dd++)
                                {
                                    for (int pd = pd_min; pd <= pd_max; pd++)
                                    {
                                        object_type v = base;
                                        v.att = att;
                                        v.ds = ds;
                                        v.dd = dd;
                                        v.evn = evn;
                                        v.ps = ps;
                                        v.pd = pd;
                                        v.pval = pval;
                                        add_drop_entry(&v, cat, DROP_GROUP_EGO, e_idx,
                                            min_depth, max_depth,
                                            alloc_depths, alloc_rarities, num_allocations);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/* Build artefact entries (single variants). */
static void build_artifact_variants(int a_idx)
{
    artefact_type* a_ptr = &a_info[a_idx];
    if (!a_ptr->tval || !a_ptr->sval)
        return;
    int k_idx = lookup_kind(a_ptr->tval, a_ptr->sval);
    if (!k_idx)
        return;

    object_type v;
    object_prep(&v, k_idx);
    v.name1 = a_idx;

    /* Copy artefact stats */
    v.pval = a_ptr->pval;
    v.att = a_ptr->att;
    v.evn = a_ptr->evn;
    v.dd = a_ptr->dd;
    v.ds = a_ptr->ds;
    v.pd = a_ptr->pd;
    v.ps = a_ptr->ps;
    v.weight = a_ptr->weight;
    v.ident = 0;
    if (!a_ptr->cost)
        v.ident |= (IDENT_BROKEN);
    if (a_ptr->flags3 & (TR3_LIGHT_CURSE))
        v.ident |= (IDENT_CURSED);

    object_kind* k_ptr = &k_info[k_idx];
    drop_category cat = drop_category_for_kind(k_ptr);
    if (cat == DROP_CAT_MAX)
        return;

    int rarity = (a_ptr->rarity > 0) ? a_ptr->rarity : 1;
    int level = (a_ptr->level > 0) ? a_ptr->level : 1;
    byte depth_arr[1] = {(byte)level};
    byte rarity_arr[1] = {(byte)rarity};
    add_drop_entry(&v, cat, DROP_GROUP_ARTIFACT, a_idx, level, MORGOTH_DEPTH,
        depth_arr, rarity_arr, 1);
}

static void clear_drop_entries(void)
{
    mem_free_null(g_drop_entries);
    g_drop_entries = NULL;
    g_drop_count = 0;
    g_drop_capacity = 0;
}

/* ------------------------------------------------------------------------ */
/* Raw file load/save                                                       */
/* ------------------------------------------------------------------------ */

static bool load_drop_raw(void)
{
    char path[1024];
    path_build(path, sizeof(path), ANGBAND_DIR_DATA, format("%s.raw", DROP_RAW_FILE));

    SDL_IOStream* fd = sdl_fopen(path, "rb");
    if (!fd)
        return false;

    drop_raw_header hdr;
    if (sdl_read(fd, (char*)&hdr, sizeof(hdr)))
    {
        sdl_fclose(fd);
        return false;
    }
    if (hdr.magic != DROP_RAW_MAGIC || hdr.version != DROP_RAW_VERSION)
    {
        sdl_fclose(fd);
        return false;
    }

    size_t bytes = hdr.count * sizeof(drop_entry);
    drop_entry* buf = mem_alloc_array(hdr.count, drop_entry);
    if (sdl_read(fd, (char*)buf, bytes))
    {
        mem_free_null(buf);
        sdl_fclose(fd);
        return false;
    }

    sdl_fclose(fd);
    clear_drop_entries();
    g_drop_entries = buf;
    g_drop_count = hdr.count;
    return true;
}

static bool save_drop_raw(void)
{
    char path[1024];
    path_build(path, sizeof(path), ANGBAND_DIR_DATA, format("%s.raw", DROP_RAW_FILE));

    SDL_IOStream* fd = sdl_fopen(path, "wb");
    if (!fd)
        return false;

    drop_raw_header hdr;
    hdr.magic = DROP_RAW_MAGIC;
    hdr.version = DROP_RAW_VERSION;
    hdr.count = (u32b)g_drop_count;

    bool ok = true;
    if (sdl_write(fd, (cptr)&hdr, sizeof(hdr)))
        ok = false;
    if (ok && sdl_write(fd, (cptr)g_drop_entries, g_drop_count * sizeof(drop_entry)))
        ok = false;

    sdl_fclose(fd);
    return ok;
}

/* ------------------------------------------------------------------------ */
/* Public init                                                              */
/* ------------------------------------------------------------------------ */

void drop_system_init(void)
{
    /* Try to use cached raw if up to date */
#ifdef CHECK_MODIFICATION_TIME
    char raw_path[1024];
    char txt_path[1024];
    path_build(raw_path, sizeof(raw_path), ANGBAND_DIR_DATA, format("%s.raw", DROP_RAW_FILE));
    
    log_debug("drop_system_init: Checking modification times for drops.raw");
    log_debug("drop_system_init: raw_path='%s'", raw_path);
    
    path_build(txt_path, sizeof(txt_path), ANGBAND_DIR_EDIT, "object.txt");
    log_debug("drop_system_init: checking against '%s'", txt_path);
    bool need_rebuild = check_modification_date_sdl(raw_path, txt_path) != 0;
    
    path_build(txt_path, sizeof(txt_path), ANGBAND_DIR_EDIT, "special.txt");
    log_debug("drop_system_init: checking against '%s'", txt_path);
    need_rebuild |= (check_modification_date_sdl(raw_path, txt_path) != 0);
    
    path_build(txt_path, sizeof(txt_path), ANGBAND_DIR_EDIT, "artifact.txt");
    log_debug("drop_system_init: checking against '%s'", txt_path);
    need_rebuild |= (check_modification_date_sdl(raw_path, txt_path) != 0);
    
    log_debug("drop_system_init: need_rebuild=%d", need_rebuild);
#else
    bool need_rebuild = true;
#endif

    if (!need_rebuild && load_drop_raw())
    {
        log_info("Loaded drop catalog from drops.raw (%zu entries)", g_drop_count);
        return;
    }

    clear_drop_entries();
    log_info("Rebuilding drop catalog...");

    /* Normal items */
    for (int k_idx = 1; k_idx < z_info->k_max; k_idx++)
        build_normal_variants(k_idx);

    /* Ego items */
    for (int e_idx = 1; e_idx < z_info->e_max; e_idx++)
        build_ego_variants(e_idx);

    /* Artefacts */
    for (int a_idx = 1; a_idx < z_info->art_max; a_idx++)
        build_artifact_variants(a_idx);

    /* Log catalog size by category/group for diagnostics */
    size_t cat_counts[DROP_CAT_MAX] = { 0 };
    size_t group_kind_counts[3] = { 0 };
    for (size_t i = 0; i < g_drop_count; i++)
    {
        if (g_drop_entries[i].category < DROP_CAT_MAX)
            cat_counts[g_drop_entries[i].category]++;
        if (g_drop_entries[i].group_kind >= 0
            && g_drop_entries[i].group_kind <= 2)
            group_kind_counts[g_drop_entries[i].group_kind]++;
    }

    save_drop_raw();
    log_info("Drop catalog rebuilt: %zu entries (weapon=%zu armor=%zu jewelry=%zu supply=%zu | normal=%zu ego=%zu art=%zu)",
        g_drop_count, cat_counts[DROP_CAT_WEAPON], cat_counts[DROP_CAT_ARMOR],
        cat_counts[DROP_CAT_JEWELRY], cat_counts[DROP_CAT_SUPPLY],
        group_kind_counts[DROP_GROUP_NORMAL], group_kind_counts[DROP_GROUP_EGO],
        group_kind_counts[DROP_GROUP_ARTIFACT]);
}

/* ------------------------------------------------------------------------ */
/* Selection logic                                                          */
/* ------------------------------------------------------------------------ */

typedef enum
{
    DROP_SUPPLY_POTION = 0,
    DROP_SUPPLY_HERB = 1,
    DROP_SUPPLY_GEM = 2,
    DROP_SUPPLY_STAFF = 3,
    DROP_SUPPLY_MISC = 4,
    DROP_SUPPLY_GROUP_MAX = 5
} drop_supply_group_id;

typedef struct
{
    drop_category cat;
    int depth;        /* Generation depth (object_level) */
    int legal_depth;  /* Depth cap for allocation legality */
    int difficulty_bonus;
    bool is_supply;
    int droptype;
    int base_roll;
    int lower;
    int upper;
    bool allow_artefacts; /* whether artefacts can be selected */
    int cat_weights[DROP_CAT_MAX];
    int supply_weights[DROP_SUPPLY_GROUP_MAX];
} drop_request;

typedef struct
{
    s16b entry_indices[4096];
    int entry_count;
    drop_group_kind kind;
    s16b group_id;
    int rarity;
} drop_group;

static void drop_request_set_default_weights(drop_request* req)
{
    for (int i = 0; i < DROP_CAT_MAX; ++i)
        req->cat_weights[i] = DROP_DEFAULT_CAT_WEIGHT;
    for (int i = 0; i < DROP_SUPPLY_GROUP_MAX; ++i)
        req->supply_weights[i] = DROP_DEFAULT_SUPPLY_WEIGHT;
}

static void drop_request_apply_profile(
    drop_request* req, const drop_profile* profile)
{
    drop_request_set_default_weights(req);
    if (!profile)
        return;

    req->cat_weights[DROP_CAT_WEAPON] = MAX(0, profile->weight_weapon);
    req->cat_weights[DROP_CAT_ARMOR] = MAX(0, profile->weight_armor);
    req->cat_weights[DROP_CAT_JEWELRY] = MAX(0, profile->weight_jewelry);
    req->cat_weights[DROP_CAT_SUPPLY] = MAX(0, profile->weight_supply);

    req->supply_weights[DROP_SUPPLY_POTION] = MAX(0, profile->supply_potion);
    req->supply_weights[DROP_SUPPLY_HERB] = MAX(0, profile->supply_herb);
    req->supply_weights[DROP_SUPPLY_GEM] = MAX(0, profile->supply_gem);
    req->supply_weights[DROP_SUPPLY_STAFF] = MAX(0, profile->supply_staff);
    req->supply_weights[DROP_SUPPLY_MISC] = MAX(0, profile->supply_misc);
}

static drop_supply_group_id supply_group_for_entry(const drop_entry* e)
{
    switch (e->obj.tval)
    {
    case TV_POTION:
        return DROP_SUPPLY_POTION;
    case TV_FOOD:
        return DROP_SUPPLY_HERB;
    case TV_GEM:
        return DROP_SUPPLY_GEM;
    case TV_STAFF:
        return DROP_SUPPLY_STAFF;
    case TV_ARROW:
    case TV_LIGHT:
    case TV_FLASK:
        return DROP_SUPPLY_MISC;
    default:
        return DROP_SUPPLY_MISC;
    }
}

static int supply_entry_weight(const drop_entry* e, int depth)
{
    int diff = e->min_depth - depth;
    if (diff <= 0)
        return 10;
    int w = 10 - diff;
    if (w < 1)
        w = 1;
    return w;
}

/* Forward declarations */
static int drop_entry_rarity_at_depth(const drop_entry* e, int depth);
static int group_rarity_at_depth(const drop_entry* e, int depth);

static const char* drop_category_name(drop_category cat)
{
    switch (cat)
    {
    case DROP_CAT_WEAPON:
        return "weapon";
    case DROP_CAT_ARMOR:
        return "armor";
    case DROP_CAT_JEWELRY:
        return "jewelry";
    case DROP_CAT_SUPPLY:
        return "supply";
    default:
        return "unknown";
    }
}

/* Category roll: defaults 25/25/25/25 */
static drop_category roll_category(const drop_request* req)
{
    int weights[DROP_CAT_MAX];
    int total = 0;
    for (int i = 0; i < DROP_CAT_MAX; ++i)
    {
        weights[i] = DROP_DEFAULT_CAT_WEIGHT;
        if (req)
            weights[i] = req->cat_weights[i];
        if (weights[i] < 0)
            weights[i] = 0;
        total += weights[i];
    }

    /* Fallback to equal weights if everything is disabled */
    if (total <= 0)
    {
        for (int i = 0; i < DROP_CAT_MAX; ++i)
            weights[i] = DROP_DEFAULT_CAT_WEIGHT;
        total = DROP_DEFAULT_CAT_WEIGHT * DROP_CAT_MAX;
    }

    int roll = rand_int(total);
    int accum = 0;
    for (int i = 0; i < DROP_CAT_MAX; ++i)
    {
        accum += weights[i];
        if (roll < accum)
            return (drop_category)i;
    }

    return DROP_CAT_SUPPLY;
}

static bool droptype_matches(const drop_request* req, const drop_entry* e)
{
    switch (req->droptype)
    {
    case DROP_TYPE_EDGED:
        return e->obj.tval == TV_SWORD;
    case DROP_TYPE_POLEARM:
        return e->obj.tval == TV_POLEARM;
    case DROP_TYPE_BOW:
        return e->obj.tval == TV_BOW;
    case DROP_TYPE_DIGGING:
        return e->obj.tval == TV_DIGGING;
    case DROP_TYPE_SHIELD:
        return e->obj.tval == TV_SHIELD;
    case DROP_TYPE_ARMOR:
        return (e->obj.tval == TV_MAIL || e->obj.tval == TV_SOFT_ARMOR);
    case DROP_TYPE_BOOTS:
        return e->obj.tval == TV_BOOTS;
    case DROP_TYPE_CLOAK:
        return e->obj.tval == TV_CLOAK;
    case DROP_TYPE_GLOVES:
        return e->obj.tval == TV_GLOVES;
    case DROP_TYPE_HEADGEAR:
        return (e->obj.tval == TV_HELM || e->obj.tval == TV_CROWN);
    case DROP_TYPE_JEWELRY:
        return (e->obj.tval == TV_RING || e->obj.tval == TV_AMULET
            || e->obj.tval == TV_LIGHT || e->obj.tval == TV_HORN);
    case DROP_TYPE_POTION:
        return e->obj.tval == TV_POTION;
    case DROP_TYPE_STAFF:
        return (e->obj.tval == TV_STAFF || e->obj.tval == TV_GEM);
    case DROP_TYPE_TORCHES:
        return e->obj.tval == TV_LIGHT
            && (e->obj.sval == SV_LIGHT_TORCH || e->obj.sval == SV_LIGHT_MALLORN
                || e->obj.sval == SV_LIGHT_LANTERN || e->obj.sval == SV_LIGHT_LESSER_JEWEL);
    default:
        return true;
    }
}

static bool collect_candidate_entries(
    const drop_request* req, bool relaxed, drop_entry** out, size_t* out_count)
{
    if (!g_drop_entries || g_drop_count == 0)
        return false;

    drop_entry* buf = mem_alloc_array(g_drop_count, drop_entry);
    size_t count = 0;
    int gen_depth = req->depth;
    int depth = req->legal_depth;
    
    /* DEBUG: Count what filters are rejecting items */
    int filter_artifact = 0, filter_droptype = 0, filter_category = 0;
    int filter_maxdepth = 0, filter_difficulty = 0, filter_total = 0;

    for (size_t i = 0; i < g_drop_count; i++)
    {
        drop_entry e = g_drop_entries[i];
        filter_total++;

        if (e.group_kind == DROP_GROUP_ARTIFACT)
        {
            /* Skip artefacts if not allowed by the drop request */
            if (!req->allow_artefacts) {
                filter_artifact++;
                continue;
            }
            
            artefact_type* a_ptr = &a_info[e.group_id];
            /* Skip if already created OR already seen by player */
            if (a_ptr->cur_num || a_ptr->seen) {
                filter_artifact++;
                continue;
            }
            /* Skip monster-specific automatic drop artefacts (indexes 20+, weapons/armor only)
             * Jewelry artefacts (1-19) have INSTA_ART for flavor system but should still drop normally */
            if ((a_ptr->flags3 & TR3_INSTA_ART) && e.group_id >= 20) {
                filter_artifact++;
                continue;
            }
        }

        if (!droptype_matches(req, &e)) {
            filter_droptype++;
            continue;
        }

        if (e.category != req->cat) {
            filter_category++;
            continue;
        }

            /* Only apply max_depth filter if explicitly set (non-zero) */
        if (e.max_depth > 0 && depth > e.max_depth) {
            filter_maxdepth++;
            /* DEBUG: Log first few maxdepth rejections */
            if (filter_maxdepth <= 3 && gen_log_initialized && gen_depth >= 19) {
                gen_log_write("DROP_MAXDEPTH_REJECT",
                    "depth=%d item_maxdepth=%d k_idx=%d group_kind=%d",
                    gen_depth, e.max_depth, e.obj.k_idx, e.group_kind);
            }
            continue;
        }

        int rarity_weight = group_rarity_at_depth(&e, depth);
        if (rarity_weight <= 0)
            continue;

        int effective_dif = e.difficulty;
        if (depth < e.min_depth)
            effective_dif += 2 * (e.min_depth - depth);

        if (req->is_supply)
        {
            buf[count++] = e;
            continue;
        }

        if (!relaxed && (effective_dif < req->lower || effective_dif > req->upper)) {
            filter_difficulty++;
            continue;
        }

        buf[count++] = e;
    }
    
    /* DEBUG: Log filtering stats */
    if (gen_log_initialized && gen_depth >= 19)
    {
        gen_log_write("DROP_FILTER",
            "depth=%d cat=%s relaxed=%s total=%d artifact_used=%d droptype=%d "
            "category=%d maxdepth=%d difficulty=%d passed=%zu",
            gen_depth, drop_category_name(req->cat), relaxed ? "yes" : "no",
            filter_total, filter_artifact, filter_droptype, filter_category,
            filter_maxdepth, filter_difficulty, count);
    }

    *out = buf;
    *out_count = count;

    if (gen_log_initialized && count > 0)
    {
        /* Log first few candidates for debugging */
        int samples = (count < 5) ? count : 5;
        for (int i = 0; i < samples; i++)
        {
            drop_entry* e = &buf[i];
            int effective_dif = e->difficulty;
            if (depth < e->min_depth)
                effective_dif += 2 * (e->min_depth - depth);
            int rarity_at_depth = drop_entry_rarity_at_depth(e, depth);
            int weight_at_depth = group_rarity_at_depth(e, depth);
            
            gen_log_write("DROP_CANDIDATE",
                "relaxed=%s k_idx=%d cat=%s group_kind=%d group_id=%d "
                "base_dif=%d eff_dif=%d min_depth=%d max_depth=%d rarity_at_depth=%d weight=%d",
                relaxed ? "yes" : "no", e->obj.k_idx,
                drop_category_name(e->category), e->group_kind, e->group_id,
                e->difficulty, effective_dif, e->min_depth, e->max_depth,
                rarity_at_depth, weight_at_depth);
        }
    }

    return (count > 0);
}

/* Calculate weight for a group at a specific depth (step-based rarity). */
static int drop_entry_rarity_at_depth(const drop_entry* e, int depth)
{
    if (!e || e->num_allocations == 0)
        return 1;

    int first_depth = e->alloc_depth[0];
    if (first_depth > 0 && depth < first_depth)
        return 0;

    int rarity = e->alloc_rarity[0];
    for (int i = 1; i < e->num_allocations; i++)
    {
        if (depth >= e->alloc_depth[i])
            rarity = e->alloc_rarity[i];
        else
            break;
    }
    return rarity;
}

static int group_rarity_at_depth(const drop_entry* e, int depth)
{
    int rarity = drop_entry_rarity_at_depth(e, depth);
    if (rarity <= 0)
        return 0;
    return MAX(1, 100 / rarity);
}

static bool build_groups(drop_entry* entries, size_t count, drop_group* groups,
    int* group_count)
{
    int gcount = 0;
    for (size_t i = 0; i < count; i++)
    {
        drop_entry* e = &entries[i];
        bool found = false;
        for (int g = 0; g < gcount; g++)
        {
            drop_group* grp = &groups[g];
            if (grp->kind == e->group_kind && grp->group_id == e->group_id)
            {
                if (grp->entry_count < (int)(sizeof(grp->entry_indices) / sizeof(grp->entry_indices[0])))
                    grp->entry_indices[grp->entry_count++] = (s16b)i;
                found = true;
                break;
            }
        }
        if (!found)
        {
            if (gcount >= *group_count)
                break;
            drop_group* grp = &groups[gcount++];
            grp->kind = e->group_kind;
            grp->group_id = e->group_id;
            grp->entry_count = 0;
            grp->entry_indices[grp->entry_count++] = (s16b)i;
        }
    }
    *group_count = gcount;
    return (gcount > 0);
}

static drop_group* choose_group(drop_group* groups, int group_count, drop_entry* entries, int depth)
{
    if (group_count <= 0)
        return NULL;

    /* Use dynamic weights to avoid fixed-buffer overflow when many groups exist */
    int* weights = mem_alloc_array(group_count, int);
    int total = 0;
    for (int i = 0; i < group_count; i++)
    {
        /* Use first entry in group to calculate depth-dependent weight */
        int entry_idx = groups[i].entry_indices[0];
        int w = group_rarity_at_depth(&entries[entry_idx], depth);
        weights[i] = w;
        total += w;
    }
    if (total <= 0)
    {
        mem_free_null(weights);
        return NULL;
    }
    int pick = rand_int(total);
    int accum = 0;
    int chosen = group_count - 1;
    for (int i = 0; i < group_count; i++)
    {
        accum += weights[i];
        if (pick < accum)
        {
            chosen = i;
            break;
        }
    }

    if (gen_log_initialized)
    {
        /* Log group selection details */
        int samples = (group_count < 10) ? group_count : 10;
        for (int i = 0; i < samples; i++)
        {
            int entry_idx = groups[i].entry_indices[0];
            int weight = group_rarity_at_depth(&entries[entry_idx], depth);
            gen_log_write("DROP_GROUP",
                "idx=%d kind=%d group_id=%d weight=%d total=%d "
                "entries=%d chosen=%s",
                i, groups[i].kind, groups[i].group_id,
                weight, total,
                groups[i].entry_count, (i == chosen) ? "YES" : "no");
        }
        gen_log_write("DROP_GROUP_PICK",
            "pick=%d total=%d chosen_idx=%d", pick, total, chosen);
    }

    mem_free_null(weights);
    return &groups[chosen];
}

static drop_entry* choose_entry_from_group(drop_entry* entries,
    const drop_group* grp)
{
    if (grp->entry_count <= 0)
        return NULL;
    int pick = rand_int(grp->entry_count);
    drop_entry* chosen = &entries[grp->entry_indices[pick]];

    if (gen_log_initialized)
    {
        gen_log_write("DROP_ITEM_SELECT",
            "group_kind=%d group_id=%d entry_count=%d pick=%d "
            "k_idx=%d att=%d ds=%d evn=%d ps=%d",
            grp->kind, grp->group_id, grp->entry_count, pick,
            chosen->obj.k_idx, chosen->obj.att, chosen->obj.ds,
            chosen->obj.evn, chosen->obj.ps);
    }

    return chosen;
}

static drop_entry* choose_supply_entry(drop_entry* entries, size_t count,
    int depth, const drop_request* req)
{
    typedef struct
    {
        drop_entry** items;
        int count;
        int cap;
    } supply_bucket;

    supply_bucket buckets[DROP_SUPPLY_GROUP_MAX];
    int bucket_weights[DROP_SUPPLY_GROUP_MAX] = { 0 };
    for (int gid = 0; gid < DROP_SUPPLY_GROUP_MAX; gid++)
    {
        buckets[gid].items = NULL;
        buckets[gid].count = 0;
        buckets[gid].cap = 0;
    }

    for (size_t i = 0; i < count; i++)
    {
        drop_entry* e = &entries[i];
        drop_supply_group_id gid = supply_group_for_entry(e);
        supply_bucket* b = &buckets[gid];
        if (b->count + 1 > b->cap)
        {
            int new_cap = (b->cap == 0) ? 64 : b->cap * 2;
            if (new_cap < b->count + 1)
                new_cap = b->count + 1;
            drop_entry** new_items = (drop_entry**)SDL_realloc(b->items, new_cap * sizeof(*new_items));
            if (!new_items)
            {
                for (int j = 0; j < DROP_SUPPLY_GROUP_MAX; j++)
                    mem_free_null(buckets[j].items);
                return NULL;
            }
            b->items = new_items;
            b->cap = new_cap;
        }
        b->items[b->count++] = e;
    }

    int total_group_weight = 0;
    for (int gid = 0; gid < DROP_SUPPLY_GROUP_MAX; gid++)
    {
        if (buckets[gid].count == 0)
            continue;
        int w = (req) ? req->supply_weights[gid] : DROP_DEFAULT_SUPPLY_WEIGHT;
        if (w <= 0)
            continue;
        bucket_weights[gid] = w;
        total_group_weight += w;
    }
    if (total_group_weight == 0)
    {
        for (int gid = 0; gid < DROP_SUPPLY_GROUP_MAX; gid++)
            mem_free_null(buckets[gid].items);
        return NULL;
    }

    int pick_group = rand_int(total_group_weight);
    int chosen_gid = DROP_SUPPLY_GROUP_MAX - 1;
    for (int gid = 0, acc = 0; gid < DROP_SUPPLY_GROUP_MAX; gid++)
    {
        if (buckets[gid].count == 0)
            continue;
        acc += bucket_weights[gid];
        if (pick_group < acc)
        {
            chosen_gid = gid;
            break;
        }
    }

    supply_bucket* chosen_bucket = &buckets[chosen_gid];
    int* item_weights = mem_alloc_array(chosen_bucket->count, int);
    if (!item_weights)
    {
        for (int gid = 0; gid < DROP_SUPPLY_GROUP_MAX; gid++)
            mem_free_null(buckets[gid].items);
        return NULL;
    }

    int total_item_weight = 0;
    for (int i = 0; i < chosen_bucket->count; i++)
    {
        drop_entry* e = chosen_bucket->items[i];
        int rarity_weight = group_rarity_at_depth(e, depth);
        if (rarity_weight <= 0)
        {
            item_weights[i] = 0;
            continue;
        }
        int w = rarity_weight * supply_entry_weight(e, depth);
        item_weights[i] = w;
        total_item_weight += w;
    }
    if (total_item_weight <= 0)
    {
        mem_free_null(item_weights);
        for (int gid = 0; gid < DROP_SUPPLY_GROUP_MAX; gid++)
            mem_free_null(buckets[gid].items);
        return NULL;
    }

    int pick_item = rand_int(total_item_weight);
    drop_entry* chosen = chosen_bucket->items[chosen_bucket->count - 1];
    for (int i = 0, acc = 0; i < chosen_bucket->count; i++)
    {
        acc += item_weights[i];
        if (pick_item < acc)
        {
            chosen = chosen_bucket->items[i];
            break;
        }
    }

    mem_free_null(item_weights);
    for (int gid = 0; gid < DROP_SUPPLY_GROUP_MAX; gid++)
        mem_free_null(buckets[gid].items);
    return chosen;
}

static void log_drop_attempt(const drop_request* req, size_t strict_count,
    size_t relaxed_count, const drop_entry* chosen, bool used_relaxed,
    bool fallback)
{
    if (!gen_log_initialized)
        return;

    int effective_dif = -1;
    int a_idx = -1;
    int e_idx = -1;
    int group_kind = -1;
    if (chosen)
    {
        if (req->depth < chosen->min_depth)
            effective_dif = chosen->difficulty + 2 * (chosen->min_depth - req->depth);
        else
            effective_dif = chosen->difficulty;

        if (chosen->group_kind == DROP_GROUP_ARTIFACT)
            a_idx = chosen->group_id;
        else if (chosen->group_kind == DROP_GROUP_EGO)
            e_idx = chosen->group_id;
        group_kind = chosen->group_kind;
    }

    gen_log_write("DROP",
        "depth=%d cat=%s droptype=%d supply=%s roll=%d band=%d..%d bonus=%d "
        "strict=%zu relaxed=%zu used_relaxed=%s fallback=%s "
        "chosen_k=%d a_idx=%d e_idx=%d base_dif=%d eff_dif=%d min_depth=%d "
        "max_depth=%d rarity_at_depth=%d group_kind=%d",
        req->depth, drop_category_name(req->cat), req->droptype,
        req->is_supply ? "yes" : "no", req->base_roll, req->lower, req->upper,
        req->difficulty_bonus, strict_count, relaxed_count,
        used_relaxed ? "yes" : "no", fallback ? "yes" : "no",
        chosen ? chosen->obj.k_idx : -1, a_idx, e_idx,
        chosen ? chosen->difficulty : -1, effective_dif,
        chosen ? chosen->min_depth : -1, chosen ? chosen->max_depth : -1,
        chosen ? group_rarity_at_depth(chosen, req->depth) : 0, group_kind);
}

/*
 * Apply jinx ego to normal items based on difficulty.
 * Jinx probability is inversely proportional to item difficulty:
 * - Easy items (low difficulty) have high jinx chance
 * - Difficult items (high difficulty) have low jinx chance
 * - Artefacts are never jinxed
 * Returns true if jinx was applied.
 */
static bool try_apply_jinx(object_type* o_ptr, int depth)
{
    /* Never jinx artefacts */
    if (o_ptr->name1)
        return false;
    
    /* Only jinx normal items (not already ego items) */
    if (o_ptr->name2)
        return false;
    
    /* Calculate item difficulty */
    int difficulty = smithing_difficulty_baseline(o_ptr);
    
    /* Base jinx probability: 10% at difficulty 0, scaling down */
    /* Formula: max(1%, 10% - (difficulty / 10)) */
    int base_prob = 1000; /* 10% in 0.1% units */
    int diff_penalty = difficulty * 10; /* 1% per point of difficulty */
    int jinx_prob = MAX(100, base_prob - diff_penalty); /* minimum 1% */
    
    /* Roll for jinx */
    if (rand_int(10000) >= jinx_prob)
        return false;
    
    /* Try each jinx ego to see if it applies to this item type */
    for (int i = 0; jinx_egos[i] >= 0; i++)
    {
        int e_idx = jinx_egos[i];
        ego_item_type* e_ptr = &e_info[e_idx];
        
        /* Check if this jinx ego applies to this object type */
        bool matches = false;
        for (int t = 0; t < EGO_TVALS_MAX && e_ptr->tval[t]; t++)
        {
            if (e_ptr->tval[t] == o_ptr->tval &&
                o_ptr->sval >= e_ptr->min_sval[t] &&
                o_ptr->sval <= e_ptr->max_sval[t])
            {
                matches = true;
                break;
            }
        }
        
        if (matches)
        {
            /* Apply the jinx ego */
            o_ptr->name2 = e_idx;
            apply_ego_static(o_ptr, e_ptr);
            
            if (gen_log_initialized)
            {
                char oname[120];
                object_desc(oname, sizeof(oname), o_ptr, false, 0);
                gen_log_write("DROP_JINXED",
                    "depth=%d dif=%d prob=%d.%02d%% obj=\"%s\"",
                    depth, difficulty, jinx_prob / 100, jinx_prob % 100, oname);
            }
            
            return true;
        }
    }
    
    return false;
}

/*
 * Generate a chest according to game design specifications:
 * - 50/50 chance small or large
 * - 50% wooden (good), 35% steel (great), 15% jewelled (superb)  
 * - Chests add 4 levels to depth for drop calculation
 */
static bool generate_chest(int depth, object_type* out)
{
    /* 50/50 chance for small vs large */
    bool is_large = one_in_(2);
    const int small_svals[] = {
        SV_CHEST_SMALL_WOODEN, SV_CHEST_SMALL_STEEL, SV_CHEST_SMALL_JEWELLED};
    const int large_svals[] = {
        SV_CHEST_LARGE_WOODEN, SV_CHEST_LARGE_STEEL, SV_CHEST_LARGE_JEWELLED};
    
    /* Determine material: 50% wood, 35% steel, 15% jewelled */
    int material_roll = rand_int(100);  /* 0-99 */
    int material_index;
    drop_quality material_quality;
    
    if (material_roll < 50)
    {
        /* Wooden chest: 0-49 = 50% */
        material_index = 0;
        material_quality = DROP_QUALITY_GOOD;
    }
    else if (material_roll < 85)
    {
        /* Steel chest: 50-84 = 35% */
        material_index = 1;
        material_quality = DROP_QUALITY_GREAT;
    }
    else
    {
        /* Jewelled chest: 85-99 = 15% */
        material_index = 2;
        material_quality = DROP_QUALITY_SUPERB;
    }
    
    int chest_sval = is_large ? large_svals[material_index]
        : small_svals[material_index];
    int difficulty_bonus = drop_quality_bonus(material_quality);
    
    /* Look up the chest k_idx by tval=TV_CHEST and sval */
    int k_idx = lookup_kind(TV_CHEST, chest_sval);
    if (!k_idx)
    {
        if (gen_log_initialized)
            gen_log_write("CHEST_ERROR", "Failed to find chest k_idx for sval=%d", chest_sval);
        return false;
    }
    
    /* Create the chest object */
    object_prep(out, k_idx);
    
    /* Set chest level (pval) = depth + 4 as per specification */
    out->pval = depth + 4;
    if (out->pval > 25)
        out->pval = 25;
    if (out->pval < 1)
        out->pval = 1;
    
    /* Set chest theme for contents (matching logic from object2.c) */
    int theme_roll = rand_int(100);
    if (theme_roll < 5)
        out->xtra1 = 1;  /* CHEST_ARMOUR */
    else if (theme_roll < 10)
        out->xtra1 = 2;  /* CHEST_WEAPONS */
    else if (theme_roll < 15)
        out->xtra1 = 3;  /* CHEST_POTIONS */
    else if (theme_roll < 20)
        out->xtra1 = 4;  /* CHEST_STAVES */
    else if (theme_roll < 25)
        out->xtra1 = 5;  /* CHEST_JEWELLERY */
    else
        out->xtra1 = 0;  /* CHEST_MIXED = default */
    
    if (gen_log_initialized)
    {
        gen_log_write("CHEST_GENERATED",
            "depth=%d size=%s material=%s quality=%s difficulty_bonus=%d chest_level=%d sval=%d",
            depth, is_large ? "large" : "small",
            material_index == 0 ? "wooden" : (material_index == 1 ? "steel" : "jewelled"),
            drop_quality_name(material_quality), difficulty_bonus, out->pval,
            chest_sval);
    }
    
    return true;
}

bool drop_generate_object(int depth, drop_quality quality, int droptype,
    bool allow_artefacts, object_type* out)
{
    return drop_generate_object_profiled(
        depth, quality, droptype, 0, allow_artefacts, NULL, out);
}

static bool drop_generate_object_internal(int depth, drop_quality quality,
    int droptype, int extra_bonus, bool allow_artefacts,
    const drop_profile* profile, object_type* out)
{
    /* Handle chest generation specially */
    if (droptype == DROP_TYPE_CHEST)
    {
        /* Chests also respect legal depth cap */
        int legal_depth = depth;
        if (p_ptr)
        {
            int current_depth = (p_ptr->depth > 0) ? p_ptr->depth : 1;
            if (legal_depth > current_depth)
                legal_depth = current_depth;
        }
        return generate_chest(legal_depth, out);
    }
    
    drop_request req;
    drop_request_apply_profile(&req, profile);
    int gen_depth = depth;
    int legal_depth = gen_depth;
    if (p_ptr)
    {
        int current_depth = (p_ptr->depth > 0) ? p_ptr->depth : 1;
        if (legal_depth > current_depth)
            legal_depth = current_depth;
    }

    req.depth = gen_depth;
    req.legal_depth = legal_depth;
    req.difficulty_bonus = extra_bonus + drop_quality_bonus(quality);
    req.is_supply = false;
    req.droptype = droptype;
    req.allow_artefacts = allow_artefacts;
    int roll1 = dieroll(30);
    int roll2 = dieroll(30);
    int min_roll = MIN(roll1, roll2);
    int base_calc = (int)(1.70 * depth) + min_roll - 23;
    req.base_roll = base_calc + req.difficulty_bonus;
    req.lower = req.base_roll - 2;
    req.upper = req.base_roll + 2;

    if (gen_log_initialized)
    {
        gen_log_write("DROP_TARGET",
            "depth=%d quality=%s bonus=%d roll1=%d roll2=%d min=%d "
            "base_calc=%d target=%d band=%d..%d",
            depth, drop_quality_name(quality),
            req.difficulty_bonus, roll1, roll2, min_roll,
            base_calc, req.base_roll, req.lower, req.upper);
    }

    /* Map droptype to category if provided */
    switch (droptype)
    {
    case DROP_TYPE_WEAPON:
    case DROP_TYPE_EDGED:
    case DROP_TYPE_POLEARM:
    case DROP_TYPE_BOW:
    case DROP_TYPE_DIGGING:
        req.cat = DROP_CAT_WEAPON;
        break;
    case DROP_TYPE_ARMOR:
    case DROP_TYPE_SHIELD:
    case DROP_TYPE_BOOTS:
    case DROP_TYPE_CLOAK:
    case DROP_TYPE_GLOVES:
    case DROP_TYPE_HEADGEAR:
        req.cat = DROP_CAT_ARMOR;
        break;
    case DROP_TYPE_JEWELRY:
        req.cat = DROP_CAT_JEWELRY;
        break;
    case DROP_TYPE_POTION:
    case DROP_TYPE_STAFF:
    case DROP_TYPE_TORCHES:
        req.cat = DROP_CAT_SUPPLY;
        req.is_supply = true;
        break;
    default:
        req.cat = roll_category(&req);
        break;
    }
    if (req.cat == DROP_CAT_SUPPLY)
        req.is_supply = true;

    if (droptype == DROP_TYPE_TORCHES)
    {
        req.supply_weights[DROP_SUPPLY_POTION] = 0;
        req.supply_weights[DROP_SUPPLY_HERB] = 0;
        req.supply_weights[DROP_SUPPLY_GEM] = 0;
        req.supply_weights[DROP_SUPPLY_STAFF] = 0;
        req.supply_weights[DROP_SUPPLY_MISC] = 100;
    }

    drop_entry* candidates = NULL;
    size_t cand_count = 0;
    size_t strict_count = 0;
    size_t relaxed_count = 0;
    bool used_relaxed = false;
    bool attempted_fallback = false;
    drop_entry* chosen = NULL;

    /* Try widening difficulty bands across 5 attempts */
    for (int attempt = 0; attempt < 5 && !chosen; attempt++)
    {
        if (attempt > 0)
        {
            /* Widen the band by 1 each attempt */
            req.lower = req.base_roll - 2 - attempt;
            req.upper = req.base_roll + 2 + attempt;
        }

        mem_free_null(candidates);
        candidates = NULL;
        cand_count = 0;
        strict_count = 0;
        relaxed_count = 0;
        used_relaxed = false;

        /* Always use strict filtering - never fall back to relaxed mode */
        if (collect_candidate_entries(&req, false, &candidates, &cand_count))
        {
            strict_count = cand_count;
        }
        else
        {
            strict_count = cand_count;
            /* No candidates found with this band width - continue to next attempt */
            continue;
        }

        if (cand_count > 0)
        {
            if (req.is_supply || req.cat == DROP_CAT_SUPPLY)
            {
                chosen = choose_supply_entry(candidates, cand_count, legal_depth, &req);
            }
            else
            {
                drop_group* groups = mem_alloc_array(cand_count, drop_group);
                int group_cap = (int)cand_count;
                int group_count = group_cap;
                if (build_groups(candidates, cand_count, groups, &group_count))
                {
                    drop_group* grp = choose_group(groups, group_count, candidates, legal_depth);
                    chosen = choose_entry_from_group(candidates, grp);
                }
                mem_free_null(groups);
            }
        }

        log_drop_attempt(&req, strict_count, relaxed_count, chosen, used_relaxed,
            attempted_fallback && attempt == 1);
    }

    bool ok = (chosen != NULL);

    /* No fallback - if we can't find anything after widening bands, just fail */
    if (!ok && gen_log_initialized)
    {
        gen_log_write("DROP_FAILED",
            "depth=%d cat=%d droptype=%d target=%d - no valid items after 5 attempts",
            depth, req.cat, droptype, req.base_roll);
    }

    if (ok)
    {
        object_wipe(out);
        object_copy(out, &chosen->obj);
        
        /* Try to apply jinx to normal items */
        try_apply_jinx(out, depth);

        /* Restore runtime quantities (fuel/charges/stacks) that are not baked into templates */
        drop_apply_spawn_quantities(out);
        
        if (chosen->group_kind == DROP_GROUP_ARTIFACT)
        {
            artefact_type* a_ptr = &a_info[chosen->group_id];
            if (!a_ptr->cur_num)
                a_ptr->cur_num = 1;
        }
        if (out->tval == TV_ARROW)
        {
            int depth_adjust = MORGOTH_DEPTH - depth;
            out->number = 20 + damroll(1, 10 + MAX(0, depth_adjust));
            if (out->number > 48)
                out->number = 48;
        }
        apply_autoinscription(out);
    }

    mem_free_null(candidates);
    return ok;
}

bool drop_generate_object_with_bonus(int depth, drop_quality quality,
    int droptype, int extra_bonus, bool allow_artefacts, object_type* out)
{
    return drop_generate_object_internal(
        depth, quality, droptype, extra_bonus, allow_artefacts, NULL, out);
}

bool drop_generate_object_profiled(int depth, drop_quality quality,
    int droptype, int extra_bonus, bool allow_artefacts,
    const drop_profile* profile, object_type* out)
{
    return drop_generate_object_internal(
        depth, quality, droptype, extra_bonus, allow_artefacts, profile, out);
}
