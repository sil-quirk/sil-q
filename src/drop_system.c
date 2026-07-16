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
    bool noble; /* NOBLE_ITEM flag: only drops from vault *&!~ tokens and chest contents */
    bool evil; /* EVIL_ITEM alignment flag used for ego/chest composition rules */
} drop_entry;

static drop_entry* g_drop_entries = NULL;
static size_t g_drop_count = 0;
static size_t g_drop_capacity = 0;

static int ego_s8(byte v)
{
    return (int)(int8_t)v;
}

static int smithing_step_from_ego_bonus(int bonus)
{
    if (bonus == 0)
        return 0;
    return (bonus > 0) ? 1 : -1;
}

static bool drop_kind_is_protection_amulet(const object_kind* k_ptr)
{
    return k_ptr && k_ptr->tval == TV_AMULET
        && k_ptr->sval == SV_AMULET_PROTECTION;
}

static int drop_kind_base_pd_min(const object_kind* k_ptr)
{
    return k_ptr ? k_ptr->pd : 0;
}

static int drop_kind_base_pd_max(const object_kind* k_ptr)
{
    if (drop_kind_is_protection_amulet(k_ptr))
        return 2;

    return k_ptr ? k_ptr->pd : 0;
}

static bool drop_object_is_damaged(const object_type* o_ptr)
{
    u32b f1, f2, f3;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    object_flags(o_ptr, &f1, &f2, &f3);
    return (f3 & TR3_DAMAGED) != 0;
}

static const char* DROP_RAW_FILE = "drops";
static const u32b DROP_RAW_MAGIC = 0x44525053; /* 'DRPS' */
static const u32b DROP_RAW_VERSION = 22;
static const int DROP_MIN_DIFFICULTY = -15;

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
    case DROP_QUALITY_ARTEFACT:
        return "artefact";
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
    case DROP_QUALITY_ARTEFACT:
        return DROP_BONUS_ARTEFACT;
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

drop_quality drop_quality_from_flags(bool good, bool great, bool superb)
{
    if (superb)
        return DROP_QUALITY_SUPERB;
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
    profile->supply_potion = DROP_DEFAULT_SUPPLY_WEIGHT * 2;
    profile->supply_herb = DROP_DEFAULT_SUPPLY_WEIGHT * 2;
    profile->supply_gem = DROP_DEFAULT_SUPPLY_WEIGHT * 2;
    profile->supply_staff = DROP_DEFAULT_SUPPLY_WEIGHT * 2;
    profile->supply_light = DROP_DEFAULT_SUPPLY_WEIGHT;
    profile->supply_arrows = DROP_DEFAULT_SUPPLY_WEIGHT;
    profile->supply_tunneling = 0; /* Disabled by default */
    profile->allow_damaged = false;
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
        /* All non-Fëanorian light sources are treated as supply. */
        if (k_ptr->sval == SV_LIGHT_FEANORIAN || k_ptr->sval == SV_LIGHT_SILMARIL)
            return DROP_CAT_JEWELRY;
        if (k_ptr->sval == SV_LIGHT_TORCH || k_ptr->sval == SV_LIGHT_MALLORN
            || k_ptr->sval == SV_LIGHT_LANTERN || k_ptr->sval == SV_LIGHT_LESSER_JEWEL)
            return DROP_CAT_SUPPLY;
        return DROP_CAT_MAX;
    case TV_DIGGING:
        /* Simple shovels/mattocks go to supply (tunneling); egos stay in weapon via add_drop_entry */
        if (k_ptr->sval == SV_SHOVEL || k_ptr->sval == SV_MATTOCK)
            return DROP_CAT_SUPPLY;
        return DROP_CAT_WEAPON;
    case TV_POTION:
    case TV_STAFF:
    case TV_HORN:
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

static int schedule_min_depth(const byte* depths, const byte* rarities,
    int count, int fallback)
{
    if (count <= 0)
        return fallback;

    for (int i = 0; i < count; i++)
    {
        if (rarities[i] > 0 && depths[i] > 0)
            return depths[i];
    }

    return fallback;
}

static int schedule_max_depth_cap(const byte* depths, const byte* rarities, int count)
{
    int last_positive = -1;
    for (int i = 0; i < count; i++)
    {
        if (rarities[i] > 0)
            last_positive = i;
    }

    if (last_positive < 0)
        return -1; /* all zero rarities */

    /* If the schedule transitions to zero after the last positive entry,
     * treat that as an inclusive max-depth marker (i.e. cap at that depth). */
    for (int i = last_positive + 1; i < count; i++)
    {
        if (rarities[i] == 0)
            return depths[i];
    }

    return 0; /* no cap */
}

static int schedule_leading_zero_floor(const byte* depths, const byte* rarities,
    int count)
{
    int effective_count = count;

    while (effective_count > 1 && rarities[effective_count - 1] == 0)
        effective_count--;

    if (effective_count <= 1 || rarities[0] != 0)
        return 0;

    for (int i = 1; i < effective_count; i++)
    {
        if (rarities[i] > 0)
            return depths[0];
    }

    return 0;
}

static int rarity_from_schedule(const byte* depths, const byte* rarities, int count,
    int depth, int default_rarity)
{
    int effective_count = count;
    int first_positive = -1;

    if (count <= 0)
        return default_rarity;

    /* Trailing zero-rarity entries are treated as max-depth markers, not an
     * in-band rarity override at that exact depth. */
    while (effective_count > 1 && rarities[effective_count - 1] == 0)
        effective_count--;

    for (int i = 0; i < effective_count; i++)
    {
        if (rarities[i] > 0)
        {
            first_positive = i;
            break;
        }
    }

    if (first_positive < 0)
        return 0;

    if (rarities[0] == 0 && first_positive > 0)
    {
        if (depth < depths[0])
            return 0;
        if (depth < depths[first_positive])
            return rarities[first_positive];
    }

    int rarity = rarities[0];
    for (int i = 1; i < effective_count; i++)
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
    int base_cap = schedule_max_depth_cap(base_depths, base_rarities, base_count);
    int ego_cap = schedule_max_depth_cap(ego_depths, ego_rarities, ego_count);
    int combined_cap = 0;
    if (base_cap > 0 && ego_cap > 0)
        combined_cap = MIN(base_cap, ego_cap);
    else if (base_cap > 0)
        combined_cap = base_cap;
    else if (ego_cap > 0)
        combined_cap = ego_cap;

    int combined_floor = MAX(
        schedule_leading_zero_floor(base_depths, base_rarities, base_count),
        schedule_leading_zero_floor(ego_depths, ego_rarities, ego_count));

    byte merged[DROP_ALLOC_MAX];
    int merged_count = 0;

    for (int i = 0; i < base_count && merged_count < DROP_ALLOC_MAX; i++)
    {
        if (combined_cap > 0 && base_depths[i] > combined_cap)
            continue;
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
        if (combined_cap > 0 && ego_depths[i] > combined_cap)
            continue;
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
    if (combined_floor > 0
        && (combined_cap <= 0 || combined_floor <= combined_cap))
    {
        out_depths[out_count] = (byte)combined_floor;
        out_rarities[out_count] = 0;
        out_count++;
    }

    for (int i = 0; i < merged_count && out_count < DROP_ALLOC_MAX; i++)
    {
        int depth = merged[i];
        if (combined_cap > 0 && depth > combined_cap)
            continue;
        if (combined_floor > 0 && depth < combined_floor)
            continue;

        int base_r = rarity_from_schedule(base_depths, base_rarities, base_count, depth, 1);
        int ego_r = rarity_from_schedule(ego_depths, ego_rarities, ego_count, depth, 1);
        /* Allocation weights are treated as 0-100-ish rarity/weight values.
         * When combining base and ego schedules, scale back down and round up
         * so low-percentage egos don't truncate to zero (which would make
         * valid combos impossible).
         * e.g. base=15 and ego=33 yields 5 (rounded up from 4.95).
         */
        int combined = 0;
        if (base_r > 0 && ego_r > 0)
            combined = (base_r * ego_r + 99) / 100;
        if (out_count == 0 || combined != out_rarities[out_count - 1])
        {
            out_depths[out_count] = (byte)depth;
            out_rarities[out_count] = (byte)MIN(combined, 255);
            out_count++;
        }
    }

    /* Preserve inclusive max-depth markers (A:.../0) so max_depth caps apply to combined entries.
     * This intentionally allows duplicate depths (e.g. depth=6 rarity=X then depth=6 rarity=0),
     * with the trailing 0 treated as a cap marker by schedule_max_depth_cap(). */
    if (combined_cap > 0 && out_count < DROP_ALLOC_MAX)
    {
        out_depths[out_count] = (byte)combined_cap;
        out_rarities[out_count] = 0;
        out_count++;
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
                int spawn_fuel = 1000;
                int min_fuel = 250;
                o_ptr->timeout = one_in_(3) ? rand_range(min_fuel, spawn_fuel) : spawn_fuel;
            }
            else if (o_ptr->sval == SV_LIGHT_LANTERN)
            {
                int spawn_fuel = (FUEL_LAMP * 2) / 5;
                int min_fuel = FUEL_LAMP / 15;
                o_ptr->timeout = one_in_(3)
                    ? rand_range(min_fuel, spawn_fuel)
                    : spawn_fuel;
            }
            else if (o_ptr->sval == SV_LIGHT_MALLORN)
            {
                o_ptr->timeout = one_in_(3) ? rand_range(30, 100) : 100;
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

    /* Throwing weapons can spawn in small stacks; identical weights still gate stacking. */
    if ((k_ptr->flags3 & TR3_THROWING) && o_ptr->tval != TV_ARROW && !o_ptr->name1)
    {
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
    if (value > 0)
    {
        int mod = 1 + ((positive_base - 1) / 5);
        *dif_inc += positive_base * value + mod * (value * (value - 1) / 2);
    }
    else if (value < 0)
    {
        int abs_value = -value;
        int negative_base = (positive_base + 1) / 2;
        int negative_mod = 1 + ((negative_base - 1) / 5);
        *dif_inc -= negative_base * abs_value + negative_mod * (abs_value * (abs_value - 1) / 2);
    }
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
    case TV_HORN:
        return INVEN_HORN;
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

bool object_uses_smithing_difficulty(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    switch (o_ptr->tval)
    {
    case TV_ARROW:
        /* Simple arrows are treated as supply; ego/artifact arrows use difficulty. */
        return (o_ptr->name1 != 0) || object_has_ego(o_ptr);

    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_BOW:
    case TV_DIGGING:
        return true;

    case TV_MAIL:
    case TV_SOFT_ARMOR:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
        return true;

    case TV_RING:
    case TV_AMULET:
        return true;

    case TV_LIGHT:
        /* Non-Fëanorian lights are treated as supply, except Grace lesser jewels. */
        if (o_ptr->sval == SV_LIGHT_FEANORIAN || o_ptr->sval == SV_LIGHT_SILMARIL)
            return true;
        if (o_ptr->sval == SV_LIGHT_LESSER_JEWEL && object_has_ego_idx(o_ptr, EGO_GRACE))
            return true;
        return false;

    default:
        return false;
    }
}

static int smithing_difficulty_baseline(const object_type* o_ptr)
{
    object_kind* k_ptr = &k_info[o_ptr->k_idx];
    int x, newv, base;
    int dif_inc = 0;
    int dif_dec = 0;
    int weight_factor;
    u32b f1, f2, f3, f4;
    int brands = 0;
    int dif_mult = 100;

    /* Extract flags */
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

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
    if (f4 & (TR4_WEIGHT | TR4_NEG_WEIGHT))
        dif_inc += 5;

    int smith_base_att = (o_ptr->tval == TV_RING || o_ptr->tval == TV_AMULET)
        ? 0
        : k_ptr->att;
    int smith_base_evn = (o_ptr->tval == TV_RING || o_ptr->tval == TV_AMULET)
        ? 0
        : k_ptr->evn;
    int smith_base_ds = (o_ptr->tval == TV_RING || o_ptr->tval == TV_AMULET)
        ? 0
        : k_ptr->ds;
    int smith_base_prot = (o_ptr->tval == TV_RING || o_ptr->tval == TV_AMULET)
        ? 0
        : ((k_ptr->ps > 0) ? ((k_ptr->ps + 1) * k_ptr->pd) : 0);

    /* Attack bonus */
    x = o_ptr->att - smith_base_att;
    if ((o_ptr->tval == TV_ARROW || o_ptr->tval == TV_BOW
            || o_ptr->tval == TV_SWORD || o_ptr->tval == TV_POLEARM
            || o_ptr->tval == TV_HAFTED)
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
    x = o_ptr->evn - smith_base_evn;
    if (o_ptr->tval == TV_MAIL || o_ptr->tval == TV_SOFT_ARMOR
        || o_ptr->tval == TV_SHIELD || o_ptr->tval == TV_CLOAK
        || o_ptr->tval == TV_BOOTS || o_ptr->tval == TV_GLOVES
        || o_ptr->tval == TV_HELM || o_ptr->tval == TV_CROWN)
    {
        drop_dif_mod(x, 6, &dif_inc);
        if (x > 0)
            dif_inc -= 1;
    }
    else
    {
        drop_dif_mod(x, 9, &dif_inc);
        if (x > 0)
            dif_inc -= 2;
    }

    /* Damage bonus */
    x = (o_ptr->ds - smith_base_ds);
    drop_dif_mod(x, 3 * ABS(x) + 2, &dif_inc);

    /* Protection bonus */
    base = smith_base_prot;
    newv = (o_ptr->ps > 0) ? ((o_ptr->ps + 1) * o_ptr->pd) : 0;
    x = newv - base;

    if ((o_ptr->tval == TV_MAIL) && (o_ptr->sval == SV_LONG_CORSLET) && (x > 0))
    {
        drop_dif_mod(x, 1, &dif_inc);
        dif_inc += 2;
    }
    else if ((o_ptr->tval == TV_AMULET) && (x > 0))
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

    if (f4 & TR4_SLAY_SERPENT)
        dif_inc += 4;
    if (f4 & TR4_SLAY_VAMPIRE)
        dif_inc += 4;
    if (f4 & TR4_SLAY_HORROR)
        dif_inc += 4;
    if (f4 & TR4_SLAY_CAT)
        dif_inc += 3;
    if (f4 & TR4_SLAY_GIANT)
        dif_inc += 3;

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
    if (f1 & TR1_BRAND_ELEC)
    {
        dif_inc += 16;  /* No monsters have HURT_ELEC, same as poison */
        brands++;
    }
    if (brands > 1)
        dif_inc += (brands - 1) * 20;

    if (f1 & TR1_SHARPNESS)
        dif_inc += (o_ptr->tval == TV_ARROW) ? 14 : 24;
    if (f1 & TR1_SHARPNESS2)
        dif_inc += 40;
    if (f1 & TR1_VAMPIRIC)
        dif_inc += 6;
    if (f3 & TR3_WILL_DRAIN)
        dif_inc += 8;  /* Like VAMPIRIC+2 */
    if (f3 & TR3_ACCURATE)
        dif_inc += 15;
    if (f4 & TR4_ARMOR_SHATTER)
        dif_inc += 15;  /* Like ACCURATE */
    if (f4 & TR4_DEPTH_SCALE_PS)
        dif_inc += 5;  /* Situational */
    if (f4 & TR4_PAIRED)
        dif_inc += 3;  /* Paired weapon bonus */
    if (f4 & TR4_SUBTLETY_THROW)
        dif_inc += 15;
    if (f4 & TR4_LIGHT_ARMOR)
        dif_inc += 2;  /* Light armour tag (e.g. the (Light) ego) */

    /* pval-based bonuses */
    if (f1 & TR1_TUNNEL)
    {
        x = o_ptr->pval - k_ptr->pval;
        drop_dif_mod(x, 8, &dif_inc);
    }
    {
        if (f1 & TR1_DAMAGE_SIDES)
        {
            int v = o_ptr->pval;
            if (v > 0)
                drop_dif_mod(v, 18, &dif_inc);
        }

        if (f1 & (TR1_STR | TR1_NEG_STR))
        {
            int v = o_ptr->stat_bonus[A_STR];
            if (v > 0)
                drop_dif_mod(v, 14, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 10, &dif_dec);
        }
        if (f1 & (TR1_DEX | TR1_NEG_DEX))
        {
            int v = o_ptr->stat_bonus[A_DEX];
            if (v > 0)
                drop_dif_mod(v, 14, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 10, &dif_dec);
        }
        if (f1 & (TR1_CON | TR1_NEG_CON))
        {
            int v = o_ptr->stat_bonus[A_CON];
            if (v > 0)
                drop_dif_mod(v, 14, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 10, &dif_dec);
        }
        if (f1 & (TR1_GRA | TR1_NEG_GRA))
        {
            int v = o_ptr->stat_bonus[A_GRA];
            if (v > 0)
                drop_dif_mod(v, 14, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 10, &dif_dec);
        }

        if (f1 & TR1_ARC)
        {
            int v = o_ptr->skill_bonus[S_ARC];
            if (v > 0)
                drop_dif_mod(v, 4, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 3, &dif_dec);
        }
        if (f1 & TR1_STL)
        {
            int v = o_ptr->skill_bonus[S_STL];
            if (v > 0)
                drop_dif_mod(v, 4, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 3, &dif_dec);
        }
        if (f1 & TR1_PER)
        {
            int v = o_ptr->skill_bonus[S_PER];
            if (v > 0)
                drop_dif_mod(v, 3, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 2, &dif_dec);
        }
        if (f1 & TR1_WIL)
        {
            int v = o_ptr->skill_bonus[S_WIL];
            if (v > 0)
                drop_dif_mod(v, 3, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 2, &dif_dec);
        }
        if (f1 & TR1_SMT)
        {
            int v = o_ptr->skill_bonus[S_SMT];
            if (v > 0)
                drop_dif_mod(v, 4, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 3, &dif_dec);
        }
        if (f1 & TR1_SNG)
        {
            int v = o_ptr->skill_bonus[S_SNG];
            if (v > 0)
                drop_dif_mod(v, 4, &dif_inc);
            else if (v < 0)
                drop_dif_mod(-v, 3, &dif_dec);
        }
    }

    /*
     * Extra difficulty for multiple distinct stat/skill bonuses.
     * First bonus is "free" (already covered by the per-bonus scaling above).
     */
    {
        int stat_count = 0;
        int skill_count = 0;

        if ((f1 & TR1_STR) && o_ptr->stat_bonus[A_STR] > 0)
            stat_count++;
        if ((f1 & TR1_DEX) && o_ptr->stat_bonus[A_DEX] > 0)
            stat_count++;
        if ((f1 & TR1_CON) && o_ptr->stat_bonus[A_CON] > 0)
            stat_count++;
        if ((f1 & TR1_GRA) && o_ptr->stat_bonus[A_GRA] > 0)
            stat_count++;

        if ((f1 & TR1_ARC) && o_ptr->skill_bonus[S_ARC] > 0)
            skill_count++;
        if ((f1 & TR1_STL) && o_ptr->skill_bonus[S_STL] > 0)
            skill_count++;
        if ((f1 & TR1_PER) && o_ptr->skill_bonus[S_PER] > 0)
            skill_count++;
        if ((f1 & TR1_WIL) && o_ptr->skill_bonus[S_WIL] > 0)
            skill_count++;
        if ((f1 & TR1_SMT) && o_ptr->skill_bonus[S_SMT] > 0)
            skill_count++;
        if ((f1 & TR1_SNG) && o_ptr->skill_bonus[S_SNG] > 0)
            skill_count++;

        if (stat_count > 1)
            dif_inc += (stat_count - 1) * 7;
        if (skill_count > 1)
            dif_inc += (skill_count - 1) * 3;
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
    if (f3 & TR3_OATH_BOOST)
        dif_inc += 5;
    if (f3 & TR3_OATH_NEGATE)
        dif_dec += 5;

    if (f2 & TR2_RES_COLD)
        dif_inc += 5;
    if (f2 & TR2_RES_FIRE)
        dif_inc += 5;
    if (f2 & TR2_RES_POIS)
        dif_inc += 5;
    if (f2 & TR2_RES_ELEC)
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

    /* Penalty flags - now apply to all items including artefacts */
    if (f2 & TR2_DANGER)
        dif_dec += 5;
    if (f2 & TR2_DARKNESS)
        dif_dec += 2;  /* Changed from 3 to match Python */
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
    if (f2 & TR2_TRAITOR)
        dif_dec += 2;
    if (f3 & TR3_CUMBERSOME)
        dif_dec += 3;
    if (f4 & TR4_UNLIGHT)
        dif_dec += 5;  /* Worse than DARKNESS - pure negative, no light bonus */
    if (f2 & TR2_SLOWNESS)
        dif_dec += 15;
    if (f2 & TR2_HUNGER)
        dif_dec += 3;
    if (f2 & TR2_FEAR)
        dif_dec += 5;

    /* Curse penalties */
    if (f3 & TR3_LIGHT_CURSE)
        dif_dec += 3;
    if (f3 & TR3_HEAVY_CURSE)
        dif_dec += 4;
    if (f3 & TR3_PERMA_CURSE)
        dif_dec += 8;

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
    case INVEN_HORN:
        dif_mult += 20;
        break;
    default:
        break;
    }

    if ((k_ptr->flags3 & TR3_ENCHANTABLE) || (f3 & TR3_ENCHANTABLE))
        dif_mult -= 30;

    dif = dif * dif_mult / 100;

    if ((o_ptr->tval == TV_ARROW) && (o_ptr->name1))
        dif /= 2;

    return dif;
}

int object_smithing_difficulty(const object_type* o_ptr)
{
    if (!object_uses_smithing_difficulty(o_ptr))
        return 0;

    return smithing_difficulty_baseline(o_ptr);
}

static int more_special_rarity_bonus(int rarity_percent)
{
    if (rarity_percent <= 0)
        return 0;

    rarity_percent += 20;
    if (rarity_percent > 255)
        rarity_percent = 255;

    return rarity_percent;
}

static int less_special_rarity_penalty(int rarity_percent)
{
    if (rarity_percent <= 0)
        return 0;

    rarity_percent -= 20;
    if (rarity_percent < 0)
        rarity_percent = 0;

    return rarity_percent;
}

static byte scale_arrow_supply_rarity(byte rarity, int att_bonus)
{
    int scaled = rarity;

    while (att_bonus > 0 && scaled > 0)
    {
        scaled /= 3;
        att_bonus--;
    }

    return (byte)scaled;
}

typedef enum
{
    DROP_ALIGNMENT_STANDARD = 0,
    DROP_ALIGNMENT_NOBLE = 1,
    DROP_ALIGNMENT_EVIL = 2
} drop_alignment;

typedef enum
{
    DROP_ALIGNMENT_FILTER_ANY = 0,
    DROP_ALIGNMENT_FILTER_NOBLE = 1,
    DROP_ALIGNMENT_FILTER_EVIL = 2
} drop_alignment_filter;

static bool merge_drop_alignment_from_flags4(drop_alignment* alignment, u32b flags4)
{
    bool noble = (flags4 & TR4_NOBLE_ITEM) != 0;
    bool evil = (flags4 & TR4_EVIL_ITEM) != 0;
    drop_alignment item_alignment;

    if (noble && evil)
        return false;

    if (!noble && !evil)
        return true;

    item_alignment = noble ? DROP_ALIGNMENT_NOBLE : DROP_ALIGNMENT_EVIL;

    if (*alignment == DROP_ALIGNMENT_STANDARD || *alignment == item_alignment)
    {
        *alignment = item_alignment;
        return true;
    }

    return false;
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

    /* Override category: ego digging tools go to weapon (normals stay in supply) */
    if (group_kind == DROP_GROUP_EGO && k_ptr->tval == TV_DIGGING)
        cat = DROP_CAT_WEAPON;

    /* Override category: artefact digging tools go to weapon (simple tools are supply-only) */
    if (group_kind == DROP_GROUP_ARTIFACT && k_ptr->tval == TV_DIGGING)
        cat = DROP_CAT_WEAPON;

    /* Special case: Lesser Jewel of Grace stays in jewelry */
    if (k_ptr->tval == TV_LIGHT && k_ptr->sval == SV_LIGHT_LESSER_JEWEL
        && object_has_ego_idx(proto, EGO_GRACE))
    {
        cat = DROP_CAT_JEWELRY;
    }

    drop_alignment alignment = DROP_ALIGNMENT_STANDARD;

    if (!merge_drop_alignment_from_flags4(&alignment, k_ptr->flags4))
        return;

    /* Check ego suffix (name2) flags4 */
    if (proto->name2 > 0 && (int)proto->name2 < z_info->e_max)
    {
        if (!merge_drop_alignment_from_flags4(&alignment, e_info[(int)proto->name2].flags4))
            return;
    }

    /* Check ego prefix (unused2) flags4 */
    if (proto->unused2 > 0 && (int)proto->unused2 < z_info->e_max)
    {
        if (!merge_drop_alignment_from_flags4(&alignment, e_info[(int)proto->unused2].flags4))
            return;
    }

    /* Check artefact flags4 */
    if (group_kind == DROP_GROUP_ARTIFACT
        && group_id > 0 && group_id < z_info->art_max)
    {
        if (!merge_drop_alignment_from_flags4(&alignment, a_info[group_id].flags4))
            return;
    }

    if (g_drop_count + 1 > g_drop_capacity)
    {
        size_t new_cap = (g_drop_capacity == 0) ? 1024 : g_drop_capacity * 2;
        if (new_cap < g_drop_count + 1)
            new_cap = g_drop_count + 1;
        drop_entry* new_buf = mem_alloc_array(new_cap, drop_entry);
        if (!new_buf)
        {
            log_error("drop_system: failed to grow drop catalog to %zu entries",
                new_cap);
            return;
        }
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
    entry->noble = (alignment == DROP_ALIGNMENT_NOBLE);
    entry->evil = (alignment == DROP_ALIGNMENT_EVIL);
}

/* Apply ego flag data (abilities and curses) without randomness */
static void apply_ego_static(object_type* o_ptr, ego_item_type* e_ptr)
{
    // abilities
    for (int i = 0; i < e_ptr->abilities && o_ptr->abilities < (int)N_ELEMENTS(o_ptr->skilltype); i++)
    {
        int idx = o_ptr->abilities;
        o_ptr->skilltype[idx] = e_ptr->skilltype[i];
        o_ptr->abilitynum[idx] = e_ptr->abilitynum[i];
        o_ptr->abilities++;
    }

    // cursed / broken flags
    if (!e_ptr->cost)
        o_ptr->ident |= (IDENT_BROKEN);
    if (e_ptr->flags3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        o_ptr->ident |= (IDENT_CURSED);

    for (int i = 0; i < A_MAX; i++)
    {
        if (e_ptr->stat_bonus_set[i])
            o_ptr->stat_bonus[i] += e_ptr->stat_bonus_min[i];
    }

    for (int i = 0; i < S_MAX; i++)
    {
        if (e_ptr->skill_bonus_set[i])
            o_ptr->skill_bonus[i] += e_ptr->skill_bonus_min[i];
    }
}

typedef struct
{
    bool is_stat;
    byte index;
    s16b min_value;
    s16b max_value;
} drop_bonus_range;

static int collect_ego_bonus_ranges(const ego_item_type* first,
    const ego_item_type* second, drop_bonus_range* out, int max_out)
{
    s16b stat_min[A_MAX] = { 0 };
    s16b stat_max[A_MAX] = { 0 };
    bool stat_used[A_MAX] = { false };
    s16b skill_min[S_MAX] = { 0 };
    s16b skill_max[S_MAX] = { 0 };
    bool skill_used[S_MAX] = { false };
    const ego_item_type* egos[2] = { first, second };
    int count = 0;

    for (int ego_idx = 0; ego_idx < (int)N_ELEMENTS(egos); ego_idx++)
    {
        const ego_item_type* e_ptr = egos[ego_idx];
        if (!e_ptr)
            continue;

        for (int i = 0; i < A_MAX; i++)
        {
            if (!e_ptr->stat_bonus_set[i])
                continue;

            stat_used[i] = true;
            stat_min[i] += e_ptr->stat_bonus_min[i];
            stat_max[i] += e_ptr->stat_bonus[i];
        }

        for (int i = 0; i < S_MAX; i++)
        {
            if (!e_ptr->skill_bonus_set[i])
                continue;

            skill_used[i] = true;
            skill_min[i] += e_ptr->skill_bonus_min[i];
            skill_max[i] += e_ptr->skill_bonus[i];
        }
    }

    for (int i = 0; i < A_MAX && count < max_out; i++)
    {
        if (!stat_used[i] || stat_max[i] <= stat_min[i])
            continue;

        out[count].is_stat = true;
        out[count].index = (byte)i;
        out[count].min_value = stat_min[i];
        out[count].max_value = stat_max[i];
        count++;
    }

    for (int i = 0; i < S_MAX && count < max_out; i++)
    {
        if (!skill_used[i] || skill_max[i] <= skill_min[i])
            continue;

        out[count].is_stat = false;
        out[count].index = (byte)i;
        out[count].min_value = skill_min[i];
        out[count].max_value = skill_max[i];
        count++;
    }

    return count;
}

static void add_drop_entry_with_bonus_ranges_recursive(const object_type* proto,
    drop_category cat, drop_group_kind group_kind, int group_id, int min_depth,
    int max_depth, const byte* alloc_depths, const byte* alloc_rarities,
    int num_allocs, const drop_bonus_range* ranges, int range_count, int range_idx)
{
    if (range_idx >= range_count)
    {
        add_drop_entry(proto, cat, group_kind, group_id, min_depth, max_depth,
            alloc_depths, alloc_rarities, num_allocs);
        return;
    }

    const drop_bonus_range* range = &ranges[range_idx];
    for (int value = range->min_value; value <= range->max_value; value++)
    {
        object_type v = *proto;
        int delta = value - range->min_value;

        if (range->is_stat)
            v.stat_bonus[range->index] += (s16b)delta;
        else
            v.skill_bonus[range->index] += (s16b)delta;

        add_drop_entry_with_bonus_ranges_recursive(&v, cat, group_kind, group_id,
            min_depth, max_depth, alloc_depths, alloc_rarities, num_allocs,
            ranges, range_count, range_idx + 1);
    }
}

static void add_drop_entry_with_bonus_ranges(const object_type* proto,
    drop_category cat, drop_group_kind group_kind, int group_id, int min_depth,
    int max_depth, const byte* alloc_depths, const byte* alloc_rarities,
    int num_allocs, const ego_item_type* first, const ego_item_type* second)
{
    drop_bonus_range ranges[A_MAX + S_MAX];
    int range_count = collect_ego_bonus_ranges(first, second, ranges,
        (int)N_ELEMENTS(ranges));

    if (range_count <= 0)
    {
        add_drop_entry(proto, cat, group_kind, group_id, min_depth, max_depth,
            alloc_depths, alloc_rarities, num_allocs);
        return;
    }

    add_drop_entry_with_bonus_ranges_recursive(proto, cat, group_kind, group_id,
        min_depth, max_depth, alloc_depths, alloc_rarities, num_allocs,
        ranges, range_count, 0);
}

static bool ego_applies_to_kind(const ego_item_type* e_ptr, const object_kind* k_ptr)
{
    if (!e_ptr || !k_ptr)
        return false;

    for (int t = 0; t < EGO_TVALS_MAX; t++)
    {
        if (!e_ptr->tval[t])
            continue;
        if (k_ptr->tval != e_ptr->tval[t])
            continue;
        if (k_ptr->sval < e_ptr->min_sval[t] || k_ptr->sval > e_ptr->max_sval[t])
            continue;
        return true;
    }

    return false;
}

static int ego_combo_group_id(int prefix_idx, int suffix_idx)
{
    return ((prefix_idx & 0xFF) << 8) | (suffix_idx & 0xFF);
}

/* Build variants for a base object (normal item). */
static void build_normal_variants(int k_idx)
{
    object_kind* k_ptr = &k_info[k_idx];
    /* Skip pure artifact templates; they should only appear via artefact entries */
    if (k_ptr->flags3 & TR3_INSTA_ART)
        return;
    if (k_ptr->flags3 & TR3_DAMAGED)
        return; /* Damaged drops come from explicit damaged-drop paths. */

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

    int min_depth = schedule_min_depth(alloc_depths, alloc_rarities,
        num_allocations, fallback_min);
    int max_depth = max_locale_depth(k_ptr);
    if (min_depth <= 0)
        min_depth = 1;
    if (rarity_cap_depth > 0 && (max_depth == 0 || rarity_cap_depth < max_depth))
        max_depth = rarity_cap_depth;

    drop_group_kind group_kind = (cat == DROP_CAT_JEWELRY) ? DROP_GROUP_EGO : DROP_GROUP_NORMAL;

    /* Supply items: no smithing variants, use new allocation semantics. */
    if (cat == DROP_CAT_SUPPLY)
    {
        if (k_ptr->tval == TV_ARROW)
        {
            int att_min = k_ptr->att;
            int att_max = MAX(k_ptr->att, k_ptr->max_att);

            for (int att = att_min; att <= att_max; att++)
            {
                object_type v = base;
                byte arrow_alloc_rarities[DROP_ALLOC_MAX];
                int att_bonus = MAX(0, att - k_ptr->att);

                memcpy(arrow_alloc_rarities, alloc_rarities,
                    sizeof(arrow_alloc_rarities));
                for (int i = 0; i < num_allocations; i++)
                    arrow_alloc_rarities[i] = scale_arrow_supply_rarity(
                        alloc_rarities[i], att_bonus);

                v.att = att;
                add_drop_entry(&v, cat, DROP_GROUP_NORMAL, k_idx, min_depth,
                    max_depth, alloc_depths, arrow_alloc_rarities,
                    num_allocations);
            }
        }
        else
        {
            add_drop_entry(&base, cat, DROP_GROUP_NORMAL, k_idx, min_depth,
                max_depth, alloc_depths, alloc_rarities, num_allocations);
        }
        return;
    }

    /* Ranges from data-driven R: lines in object.txt */
    int att_min = k_ptr->att;
    int att_max = k_ptr->max_att;
    int ds_min = k_ptr->ds;
    int ds_max = k_ptr->max_ds;
    int evn_min = k_ptr->evn;
    int evn_max = k_ptr->max_evn;
    int pd_min = drop_kind_base_pd_min(k_ptr);
    int pd_max = drop_kind_base_pd_max(k_ptr);
    int ps_min = k_ptr->ps;
    int ps_max = k_ptr->max_ps;
    u32b kind_pval_mask = object_kind_pval_flags1(k_ptr);
    int pval_min = k_ptr->pval;
    int pval_max = k_ptr->max_pval;
    bool pval_allowed = kind_pval_mask != 0 || k_ptr->pval != 0;

    // Variant list (all combinations within smithing caps)
    // Use combined rarity and minimum depth for the entire item
    for (int att = att_min; att <= att_max; att++)
    {
        for (int ds = ds_min; ds <= ds_max; ds++)
        {
            for (int evn = evn_min; evn <= evn_max; evn++)
            {
                for (int pd = pd_min; pd <= pd_max; pd++)
                {
                    for (int ps = ps_min; ps <= ps_max; ps++)
                    {
                        int pval_hi = pval_allowed ? pval_max : pval_min;
                        for (int pval = pval_min; pval <= pval_hi; pval++)
                        {
                            object_type v = base;
                            int delta = pval - base.pval;
                            v.att = att;
                            v.ds = ds;
                            v.evn = evn;
                            v.pd = pd;
                            v.ps = ps;
                            v.pval = pval;

                            if (delta != 0)
                                object_apply_pval_delta_with_mask(&v, kind_pval_mask, delta);
                            add_drop_entry(&v, cat, group_kind, k_idx,
                                min_depth, max_depth,
                                alloc_depths, alloc_rarities, num_allocations);
                        }
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

    const char* ego_name = e_name + e_ptr->name;
    bool is_prefix = ego_name_is_prefix(ego_name);

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
            if ((k_ptr->flags3 & (TR3_MITHRIL | TR3_STAR_IRON))
                && (e_ptr->flags4 & TR4_EVIL_ITEM))
                continue;

            drop_category cat = drop_category_for_kind(k_ptr);
            if (cat == DROP_CAT_MAX)
                continue;

            object_type base;
            object_prep(&base, k_idx);
            base.weight = k_ptr->weight;
            if (is_prefix)
                object_set_ego_prefix(&base, e_idx);
            else
                object_set_ego_suffix(&base, e_idx);
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

            /* MORE_SPECIAL: boost base item rarities by one tier (+20) so that
               e.g. Dagger (85) becomes 100, grouping with Spear/Shortsword. */
            if (k_ptr->flags3 & TR3_MORE_SPECIAL)
            {
                for (int i = 0; i < base_allocs; i++)
                    base_rarities[i] = (byte)more_special_rarity_bonus(base_rarities[i]);
            }

            /* LESS_SPECIAL: reduce base item rarities by one tier (-20), making
               ego combinations rarer. Bottoms out at 0. */
            if (k_ptr->flags4 & TR4_LESS_SPECIAL)
            {
                for (int i = 0; i < base_allocs; i++)
                    base_rarities[i] = (byte)less_special_rarity_penalty(base_rarities[i]);
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

            int base_min_depth = schedule_min_depth(base_depths, base_rarities,
                base_allocs, base_fallback_depth);
            int min_depth = schedule_min_depth(ego_depths, ego_rarities,
                ego_allocs, ego_fallback_depth);
            if (base_min_depth <= 0)
                base_min_depth = 1;
            if (min_depth < base_min_depth)
                min_depth = base_min_depth;
            int rarity_cap_depth = schedule_max_depth_cap(alloc_depths, alloc_rarities, num_allocations);
            if (min_depth <= 0)
                min_depth = 1;
            if (rarity_cap_depth > 0 && (max_depth == 0 || rarity_cap_depth < max_depth))
                max_depth = rarity_cap_depth;

            /* Ranges from data-driven R: lines + ego C: line contributions */
            int ego_max_att = ego_s8(e_ptr->max_att);
            int ego_to_ds = ego_s8(e_ptr->to_ds);
            int ego_max_evn = ego_s8(e_ptr->max_evn);
            int ego_to_ps = ego_s8(e_ptr->to_ps);
            int ego_to_dd = ego_s8(e_ptr->to_dd);
            int ego_to_pd = ego_s8(e_ptr->to_pd);

            int att_min = k_ptr->att + smithing_step_from_ego_bonus(ego_max_att);
            int att_max = k_ptr->max_att + ego_max_att;
            int ds_min = k_ptr->ds + smithing_step_from_ego_bonus(ego_to_ds);
            int ds_max = k_ptr->max_ds + ego_to_ds;
            int evn_min = k_ptr->evn + smithing_step_from_ego_bonus(ego_max_evn);
            int evn_max = k_ptr->max_evn + ego_max_evn;
            int ps_min = k_ptr->ps + smithing_step_from_ego_bonus(ego_to_ps);
            int ps_max = k_ptr->max_ps + ego_to_ps;
            int dd_min = k_ptr->dd + smithing_step_from_ego_bonus(ego_to_dd);
            int dd_max = k_ptr->dd + ego_to_dd;
            int pd_min = drop_kind_base_pd_min(k_ptr)
                + smithing_step_from_ego_bonus(ego_to_pd);
            int pd_max = drop_kind_base_pd_max(k_ptr) + ego_to_pd;
            u32b kind_pval_mask = object_kind_pval_flags1(k_ptr);
            u32b ego_pval_mask = ego_item_pval_flags1(e_ptr);
            int kind_pval_min = k_ptr->pval;
            int kind_pval_max = k_ptr->max_pval;
            int ego_pval_min = (e_ptr->max_pval > 0) ? 1 : 0;
            int ego_pval_max = e_ptr->max_pval;
            bool kind_pval_allowed = (kind_pval_mask != 0)
                || (k_ptr->pval != 0) || (k_ptr->max_pval != k_ptr->pval);

            if (ds_min < 0)
                ds_min = 0;
            if (ds_max < 0)
                ds_max = 0;
            if (dd_min < 0)
                dd_min = 0;
            if (dd_max < 0)
                dd_max = 0;
            if (pd_min < 0)
                pd_min = 0;
            if (pd_max < 0)
                pd_max = 0;
            if (ps_min < 0)
                ps_min = 0;
            if (ps_max < 0)
                ps_max = 0;

            if (att_min > att_max)
                att_min = att_max;
            if (ds_min > ds_max)
                ds_min = ds_max;
            if (evn_min > evn_max)
                evn_min = evn_max;
            if (ps_min > ps_max)
                ps_min = ps_max;
            if (dd_min > dd_max)
                dd_min = dd_max;
            if (pd_min > pd_max)
                pd_min = pd_max;
            if (kind_pval_min > kind_pval_max)
                kind_pval_max = kind_pval_min;
            if (ego_pval_min > ego_pval_max)
                ego_pval_min = ego_pval_max;

            /* Generate variants using combined rarity and effective min depth */
            for (int att = att_min; att <= att_max; att++)
            {
                for (int ds = ds_min; ds <= ds_max; ds++)
                {
                    for (int evn = evn_min; evn <= evn_max; evn++)
                    {
                        for (int ps = ps_min; ps <= ps_max; ps++)
                        {
                            int kind_pval_hi = kind_pval_allowed ? kind_pval_max : kind_pval_min;
                            for (int kind_pval = kind_pval_min;
                                 kind_pval <= kind_pval_hi; kind_pval++)
                            {
                                for (int ego_pval = ego_pval_min; ego_pval <= ego_pval_max; ego_pval++)
                                {
                                    for (int dd = dd_min; dd <= dd_max; dd++)
                                    {
                                        for (int pd = pd_min; pd <= pd_max; pd++)
                                        {
                                            object_type v = base;
                                            int kind_delta = kind_pval - base.pval;
                                            /* Keep catalog prototypes aligned with object_into_special():
                                             * cursed egos remain cursed, but their pval is not inverted. */
                                            int ego_delta = ego_pval;
                                            v.att = att;
                                            v.ds = ds;
                                            v.dd = dd;
                                            v.evn = evn;
                                            v.ps = ps;
                                            v.pd = pd;
                                            v.pval = kind_pval + ego_delta;

                                            if (kind_delta != 0)
                                                object_apply_pval_delta_with_mask(&v, kind_pval_mask, kind_delta);
                                            if (ego_delta != 0)
                                                object_apply_pval_delta_with_mask(&v, ego_pval_mask, ego_delta);

                                            add_drop_entry_with_bonus_ranges(&v, cat,
                                                DROP_GROUP_EGO, e_idx, min_depth,
                                                max_depth, alloc_depths,
                                                alloc_rarities, num_allocations,
                                                e_ptr, NULL);
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
}

static void build_ego_combo_variants(int prefix_idx, int suffix_idx)
{
    if (prefix_idx <= 0 || suffix_idx <= 0)
        return;

    ego_item_type* prefix_ptr = &e_info[prefix_idx];
    ego_item_type* suffix_ptr = &e_info[suffix_idx];
    if (suffix_ptr->flags4 & TR4_NO_PREFIX)
        return;
    if (!prefix_ptr->tval[0] || !suffix_ptr->tval[0])
        return;

    const char* prefix_name = e_name + prefix_ptr->name;
    const char* suffix_name = e_name + suffix_ptr->name;
    if (!ego_name_is_prefix(prefix_name) || ego_name_is_prefix(suffix_name))
        return;

    if ((prefix_ptr->flags4 & TR4_NOBLE_ITEM) && (suffix_ptr->flags4 & TR4_EVIL_ITEM))
        return;
    if ((prefix_ptr->flags4 & TR4_EVIL_ITEM) && (suffix_ptr->flags4 & TR4_NOBLE_ITEM))
        return;

    int group_id = ego_combo_group_id(prefix_idx, suffix_idx);

    for (int k_idx = 1; k_idx < z_info->k_max; k_idx++)
    {
        object_kind* k_ptr = &k_info[k_idx];
        if (k_ptr->flags3 & TR3_INSTA_ART)
            continue;
        if (!ego_applies_to_kind(prefix_ptr, k_ptr) || !ego_applies_to_kind(suffix_ptr, k_ptr))
            continue;
        if ((k_ptr->flags3 & (TR3_MITHRIL | TR3_STAR_IRON))
            && ((prefix_ptr->flags4 & TR4_EVIL_ITEM)
                || (suffix_ptr->flags4 & TR4_EVIL_ITEM)))
            continue;

        drop_category cat = drop_category_for_kind(k_ptr);
        if (cat == DROP_CAT_MAX)
            continue;

        object_type base;
        object_prep(&base, k_idx);
        base.weight = k_ptr->weight;
        object_set_ego_prefix(&base, prefix_idx);
        object_set_ego_suffix(&base, suffix_idx);
        apply_ego_static(&base, prefix_ptr);
        apply_ego_static(&base, suffix_ptr);

        /* Max depth: apply the strictest max-level restriction among the two egos. */
        int max_depth = max_locale_depth(k_ptr);
        if (prefix_ptr->max_level > 0 && (max_depth == 0 || prefix_ptr->max_level < max_depth))
            max_depth = prefix_ptr->max_level;
        if (suffix_ptr->max_level > 0 && (max_depth == 0 || suffix_ptr->max_level < max_depth))
            max_depth = suffix_ptr->max_level;

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

        int prefix_fallback_depth = (prefix_ptr->level > 0) ? prefix_ptr->level : 1;
        int suffix_fallback_depth = (suffix_ptr->level > 0) ? suffix_ptr->level : 1;

        byte prefix_depths[DROP_ALLOC_MAX];
        byte prefix_rarities[DROP_ALLOC_MAX];
        int prefix_allocs = collect_ego_allocations(prefix_ptr, prefix_depths, prefix_rarities);
        int prefix_default_rarity = (prefix_ptr->rarity > 0) ? prefix_ptr->rarity : 1;
        if (prefix_allocs == 0)
        {
            prefix_depths[0] = (byte)prefix_fallback_depth;
            prefix_rarities[0] = (byte)prefix_default_rarity;
            prefix_allocs = 1;
        }

        byte suffix_depths[DROP_ALLOC_MAX];
        byte suffix_rarities[DROP_ALLOC_MAX];
        int suffix_allocs = collect_ego_allocations(suffix_ptr, suffix_depths, suffix_rarities);
        int suffix_default_rarity = (suffix_ptr->rarity > 0) ? suffix_ptr->rarity : 1;
        if (suffix_allocs == 0)
        {
            suffix_depths[0] = (byte)suffix_fallback_depth;
            suffix_rarities[0] = (byte)suffix_default_rarity;
            suffix_allocs = 1;
        }

        byte tmp_depths[DROP_ALLOC_MAX];
        byte tmp_rarities[DROP_ALLOC_MAX];
        int tmp_allocs = combine_allocations(
            base_depths, base_rarities, base_allocs,
            prefix_depths, prefix_rarities, prefix_allocs,
            tmp_depths, tmp_rarities);

        byte alloc_depths[DROP_ALLOC_MAX];
        byte alloc_rarities[DROP_ALLOC_MAX];
        int num_allocations = combine_allocations(
            tmp_depths, tmp_rarities, tmp_allocs,
            suffix_depths, suffix_rarities, suffix_allocs,
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

        int base_min_depth = schedule_min_depth(base_depths, base_rarities,
            base_allocs, base_fallback_depth);
        int prefix_min_depth = schedule_min_depth(prefix_depths, prefix_rarities,
            prefix_allocs, prefix_fallback_depth);
        int suffix_min_depth = schedule_min_depth(suffix_depths, suffix_rarities,
            suffix_allocs, suffix_fallback_depth);

        if (base_min_depth <= 0)
            base_min_depth = 1;
        if (prefix_min_depth <= 0)
            prefix_min_depth = 1;
        if (suffix_min_depth <= 0)
            suffix_min_depth = 1;

        int min_depth = base_min_depth;
        if (prefix_min_depth > min_depth)
            min_depth = prefix_min_depth;
        if (suffix_min_depth > min_depth)
            min_depth = suffix_min_depth;

        int rarity_cap_depth = schedule_max_depth_cap(alloc_depths, alloc_rarities, num_allocations);
        if (min_depth <= 0)
            min_depth = 1;
        if (rarity_cap_depth > 0 && (max_depth == 0 || rarity_cap_depth < max_depth))
            max_depth = rarity_cap_depth;

        /* Combined ego numeric contributions */
        int max_att_bonus = ego_s8(prefix_ptr->max_att) + ego_s8(suffix_ptr->max_att);
        int max_evn_bonus = ego_s8(prefix_ptr->max_evn) + ego_s8(suffix_ptr->max_evn);
        int to_dd_bonus = ego_s8(prefix_ptr->to_dd) + ego_s8(suffix_ptr->to_dd);
        int to_ds_bonus = ego_s8(prefix_ptr->to_ds) + ego_s8(suffix_ptr->to_ds);
        int to_pd_bonus = ego_s8(prefix_ptr->to_pd) + ego_s8(suffix_ptr->to_pd);
        int to_ps_bonus = ego_s8(prefix_ptr->to_ps) + ego_s8(suffix_ptr->to_ps);
        int att_min = k_ptr->att
            + smithing_step_from_ego_bonus(ego_s8(prefix_ptr->max_att))
            + smithing_step_from_ego_bonus(ego_s8(suffix_ptr->max_att));
        int att_max = k_ptr->max_att + max_att_bonus;
        int ds_min = k_ptr->ds
            + smithing_step_from_ego_bonus(ego_s8(prefix_ptr->to_ds))
            + smithing_step_from_ego_bonus(ego_s8(suffix_ptr->to_ds));
        int ds_max = k_ptr->max_ds + to_ds_bonus;
        int evn_min = k_ptr->evn
            + smithing_step_from_ego_bonus(ego_s8(prefix_ptr->max_evn))
            + smithing_step_from_ego_bonus(ego_s8(suffix_ptr->max_evn));
        int evn_max = k_ptr->max_evn + max_evn_bonus;
        int ps_min = k_ptr->ps
            + smithing_step_from_ego_bonus(ego_s8(prefix_ptr->to_ps))
            + smithing_step_from_ego_bonus(ego_s8(suffix_ptr->to_ps));
        int ps_max = k_ptr->max_ps + to_ps_bonus;
        int dd_min = k_ptr->dd
            + smithing_step_from_ego_bonus(ego_s8(prefix_ptr->to_dd))
            + smithing_step_from_ego_bonus(ego_s8(suffix_ptr->to_dd));
        int dd_max = k_ptr->dd + to_dd_bonus;
        int pd_min = drop_kind_base_pd_min(k_ptr)
            + smithing_step_from_ego_bonus(ego_s8(prefix_ptr->to_pd))
            + smithing_step_from_ego_bonus(ego_s8(suffix_ptr->to_pd));
        int pd_max = drop_kind_base_pd_max(k_ptr) + to_pd_bonus;
        u32b kind_pval_mask = object_kind_pval_flags1(k_ptr);
        u32b prefix_pval_mask = ego_item_pval_flags1(prefix_ptr);
        u32b suffix_pval_mask = ego_item_pval_flags1(suffix_ptr);
        int kind_pval_min = k_ptr->pval;
        int kind_pval_max = k_ptr->max_pval;
        int prefix_pval_min = (prefix_ptr->max_pval > 0) ? 1 : 0;
        int prefix_pval_max = prefix_ptr->max_pval;
        int suffix_pval_min = (suffix_ptr->max_pval > 0) ? 1 : 0;
        int suffix_pval_max = suffix_ptr->max_pval;
        bool kind_pval_allowed = (kind_pval_mask != 0)
            || (k_ptr->pval != 0) || (k_ptr->max_pval != k_ptr->pval);

        if (ds_min < 0)
            ds_min = 0;
        if (ds_max < 0)
            ds_max = 0;
        if (dd_min < 0)
            dd_min = 0;
        if (dd_max < 0)
            dd_max = 0;
        if (pd_min < 0)
            pd_min = 0;
        if (pd_max < 0)
            pd_max = 0;
        if (ps_min < 0)
            ps_min = 0;
        if (ps_max < 0)
            ps_max = 0;

        if (att_min > att_max)
            att_min = att_max;
        if (ds_min > ds_max)
            ds_min = ds_max;
        if (evn_min > evn_max)
            evn_min = evn_max;
        if (ps_min > ps_max)
            ps_min = ps_max;
        if (dd_min > dd_max)
            dd_min = dd_max;
        if (pd_min > pd_max)
            pd_min = pd_max;
        if (kind_pval_min > kind_pval_max)
            kind_pval_max = kind_pval_min;
        if (prefix_pval_min > prefix_pval_max)
            prefix_pval_min = prefix_pval_max;
        if (suffix_pval_min > suffix_pval_max)
            suffix_pval_min = suffix_pval_max;

        for (int att = att_min; att <= att_max; att++)
        {
            for (int ds = ds_min; ds <= ds_max; ds++)
            {
                for (int evn = evn_min; evn <= evn_max; evn++)
                {
                    for (int ps = ps_min; ps <= ps_max; ps++)
                    {
                        int kind_pval_hi = kind_pval_allowed ? kind_pval_max : kind_pval_min;
                        for (int kind_pval = kind_pval_min; kind_pval <= kind_pval_hi; kind_pval++)
                        {
                            for (int prefix_pval = prefix_pval_min; prefix_pval <= prefix_pval_max; prefix_pval++)
                            {
                                for (int suffix_pval = suffix_pval_min; suffix_pval <= suffix_pval_max; suffix_pval++)
                                {
                                    for (int dd = dd_min; dd <= dd_max; dd++)
                                    {
                                        for (int pd = pd_min; pd <= pd_max; pd++)
                                        {
                                            object_type v = base;
                                            int kind_delta = kind_pval - base.pval;
                                            int prefix_delta = prefix_pval;
                                            int suffix_delta = suffix_pval;
                                            v.att = att;
                                            v.ds = ds;
                                            v.dd = dd;
                                            v.evn = evn;
                                            v.ps = ps;
                                            v.pd = pd;
                                            v.pval = kind_pval + prefix_delta + suffix_delta;

                                            if (kind_delta != 0)
                                                object_apply_pval_delta_with_mask(&v, kind_pval_mask, kind_delta);
                                            if (prefix_delta != 0)
                                                object_apply_pval_delta_with_mask(&v, prefix_pval_mask, prefix_delta);
                                            if (suffix_delta != 0)
                                                object_apply_pval_delta_with_mask(&v, suffix_pval_mask, suffix_delta);

                                            add_drop_entry_with_bonus_ranges(&v, cat,
                                                DROP_GROUP_EGO, group_id, min_depth,
                                                max_depth, alloc_depths,
                                                alloc_rarities, num_allocations,
                                                prefix_ptr, suffix_ptr);
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
    for (int i = 0; i < A_MAX; i++)
        v.stat_bonus[i] = a_ptr->stat_bonus[i];
    for (int i = 0; i < S_MAX; i++)
        v.skill_bonus[i] = a_ptr->skill_bonus[i];
    v.att = a_ptr->att;
    v.evn = a_ptr->evn;
    v.dd = a_ptr->dd;
    v.ds = a_ptr->ds;
    v.pd = a_ptr->pd;
    v.ps = a_ptr->ps;
    v.weight = a_ptr->weight;

    /* For stackable artefacts (arrows, throwing weapons), use spawn_num */
    {
        object_kind* ak_ptr = &k_info[k_idx];
        if ((v.tval == TV_ARROW) || (ak_ptr->flags3 & TR3_THROWING))
        {
            int desired = a_ptr->spawn_num ? (int)a_ptr->spawn_num : 1;
            int limit = object_stack_limit(&v);
            if (limit > 0 && desired > limit)
                desired = limit;
            if (desired < 1)
                desired = 1;
            v.number = (byte)desired;
        }
    }

    v.ident = 0;
    if (!a_ptr->cost)
        v.ident |= (IDENT_BROKEN);
    if (a_ptr->flags3 & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        v.ident |= (IDENT_CURSED);

    /* Copy artefact-granted abilities (mirrors object_into_artefact()). */
    for (int i = 0; i < a_ptr->abilities && v.abilities < (int)N_ELEMENTS(v.skilltype); i++)
    {
        int idx = v.abilities;
        v.skilltype[idx] = a_ptr->skilltype[i];
        v.abilitynum[idx] = a_ptr->abilitynum[i];
        v.bane_type[idx] = a_ptr->bane_type[i];
        v.abilities++;
    }

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
    
    path_build(txt_path, sizeof(txt_path), ANGBAND_DIR_EDIT, "artefact.txt");
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

    /* Ego prefix+suffix combos */
    for (int p_idx = 1; p_idx < z_info->e_max; p_idx++)
    {
        ego_item_type* p_ptr = &e_info[p_idx];
        if (!p_ptr->tval[0])
            continue;
        if (!ego_name_is_prefix(e_name + p_ptr->name))
            continue;

        for (int s_idx = 1; s_idx < z_info->e_max; s_idx++)
        {
            ego_item_type* s_ptr = &e_info[s_idx];
            if (!s_ptr->tval[0])
                continue;
            if (ego_name_is_prefix(e_name + s_ptr->name))
                continue;

            build_ego_combo_variants(p_idx, s_idx);
        }
    }

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
    DROP_SUPPLY_LIGHT = 4,
    DROP_SUPPLY_ARROWS = 5,
    DROP_SUPPLY_TUNNELING = 6,
    DROP_SUPPLY_GROUP_MAX = 7
} drop_supply_group_id;

typedef struct
{
    drop_category cat;
    u32b cat_mask; /* allowed categories (bitmask of DROP_CAT_*) */
    drop_quality quality;
    int depth;        /* Generation depth (object_level) */
    int legal_depth;  /* Depth cap for allocation legality */
    int min_depth_penalty_depth; /* Depth used only for min-depth difficulty penalty */
    int difficulty_bonus;
    bool is_supply;
    int droptype;
    int base_roll;
    int lower;
    int upper;
    bool allow_artefacts; /* whether artefacts can be selected */
    bool artefacts_only; /* whether only artefact entries are allowed */
    bool allow_noble; /* explicit override for noble-tagged entries */
    bool allow_evil; /* explicit override for evil-tagged entries */
    bool allow_noble_from_quality; /* whether GOOD+ quality may include noble-tagged entries */
    drop_alignment_filter alignment_filter; /* restrict to a specific alignment-tagged pool */
    bool allow_damaged; /* whether damaged items may participate in non-damaged profiles */
    int artefact_weight_multiplier; /* group weight multiplier for artefacts */
    int noble_rarity_bonus; /* additive rarity bonus for noble entries */
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
    req->artefact_weight_multiplier = 1;
    req->noble_rarity_bonus = 0;
    req->allow_damaged = false;
    for (int i = 0; i < DROP_CAT_MAX; ++i)
        req->cat_weights[i] = DROP_DEFAULT_CAT_WEIGHT;
    req->supply_weights[DROP_SUPPLY_POTION] = DROP_DEFAULT_SUPPLY_WEIGHT * 2;
    req->supply_weights[DROP_SUPPLY_HERB] = DROP_DEFAULT_SUPPLY_WEIGHT * 2;
    req->supply_weights[DROP_SUPPLY_GEM] = DROP_DEFAULT_SUPPLY_WEIGHT * 2;
    req->supply_weights[DROP_SUPPLY_STAFF] = DROP_DEFAULT_SUPPLY_WEIGHT * 2;
    req->supply_weights[DROP_SUPPLY_LIGHT] = DROP_DEFAULT_SUPPLY_WEIGHT;
    req->supply_weights[DROP_SUPPLY_ARROWS] = DROP_DEFAULT_SUPPLY_WEIGHT;
    req->supply_weights[DROP_SUPPLY_TUNNELING] = DROP_DEFAULT_SUPPLY_WEIGHT * 2;
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
    req->supply_weights[DROP_SUPPLY_LIGHT] = MAX(0, profile->supply_light);
    req->supply_weights[DROP_SUPPLY_ARROWS] = MAX(0, profile->supply_arrows);
    req->supply_weights[DROP_SUPPLY_TUNNELING] = MAX(0, profile->supply_tunneling);
    req->allow_damaged = profile->allow_damaged;
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
    case TV_HORN:
        return DROP_SUPPLY_STAFF;
    case TV_LIGHT:
    case TV_FLASK:
        return DROP_SUPPLY_LIGHT;
    case TV_DIGGING:
        return DROP_SUPPLY_TUNNELING;
    case TV_ARROW:
        return DROP_SUPPLY_ARROWS;
    default:
        return DROP_SUPPLY_ARROWS;
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

static int drop_entry_pick_weight(const drop_entry* e, int base_rarity,
    int group_size, const drop_request* req)
{
    int weight = base_rarity;

    if (weight <= 0)
        return 0;

    if (e && req && e->noble && req->noble_rarity_bonus > 0 && group_size > 0)
        weight += group_size * req->noble_rarity_bonus;

    return weight;
}

static int drop_group_pick_bonus(const drop_group* grp, drop_entry* entries,
    const drop_request* req)
{
    int noble_count = 0;

    if (!grp || grp->entry_count <= 0)
        return 0;
    if (!req || req->noble_rarity_bonus <= 0)
        return 0;

    for (int i = 0; i < grp->entry_count; i++)
    {
        if (entries[grp->entry_indices[i]].noble)
            noble_count++;
    }

    return noble_count * req->noble_rarity_bonus;
}

/* Forward declarations */
static int drop_entry_rarity_at_depth(const drop_entry* e, int depth);
static int group_rarity_at_depth(const drop_entry* e, int depth);
static const drop_entry* find_drop_entry_for_object(const object_type* o_ptr,
    byte ego_prefix, byte ego_suffix);

static const drop_entry* find_drop_entry_for_object(const object_type* o_ptr,
    byte ego_prefix, byte ego_suffix)
{
    if (!o_ptr || !o_ptr->k_idx)
        return NULL;

    for (size_t i = 0; i < g_drop_count; i++)
    {
        const drop_entry* e = &g_drop_entries[i];
        if (e->obj.k_idx != o_ptr->k_idx)
            continue;
        if (e->obj.name1 != o_ptr->name1)
            continue;
        if (object_ego_prefix(&e->obj) != ego_prefix)
            continue;
        if (object_ego_suffix(&e->obj) != ego_suffix)
            continue;

        return e;
    }

    return NULL;
}

int object_weight_rarity(const object_type* o_ptr, int depth)
{
    if (!o_ptr || !o_ptr->k_idx)
        return 0;
    if (depth < 1)
        depth = 1;
    if (!g_drop_entries || g_drop_count == 0)
        return 0;

    byte ego_prefix = object_ego_prefix(o_ptr);
    byte ego_suffix = object_ego_suffix(o_ptr);
    const drop_entry* match = NULL;

    /* Match by base kind + artifact id + ego affixes.
     * We intentionally ignore stat rolls because allocation schedule is shared
     * across variants within the same kind+ego combo.
     */
    match = find_drop_entry_for_object(o_ptr, ego_prefix, ego_suffix);

    if (match)
        return group_rarity_at_depth(match, depth);

    return 0;
}

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
    bool damaged = drop_object_is_damaged(&e->obj);

    if (damaged && req->droptype != DROP_TYPE_DAMAGED && !req->allow_damaged)
        return false;

    switch (req->droptype)
    {
    case DROP_TYPE_NOT_DAMAGED:
        return !damaged;
    case DROP_TYPE_DAMAGED:
        return damaged;
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
            || e->obj.tval == TV_LIGHT);
    case DROP_TYPE_POTION:
        return e->obj.tval == TV_POTION;
    case DROP_TYPE_STAFF:
        return (e->obj.tval == TV_STAFF || e->obj.tval == TV_HORN
            || e->obj.tval == TV_GEM);
    case DROP_TYPE_SIMPLE_LIGHTS:
        return e->group_kind == DROP_GROUP_NORMAL
            && e->obj.tval == TV_LIGHT
            && (e->obj.sval == SV_LIGHT_TORCH || e->obj.sval == SV_LIGHT_MALLORN
                || e->obj.sval == SV_LIGHT_LANTERN);
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
    int penalty_depth = req->min_depth_penalty_depth;
    
    /* DEBUG: Count what filters are rejecting items */
    int filter_artifact = 0, filter_droptype = 0, filter_category = 0;
    int filter_maxdepth = 0, filter_difficulty = 0, filter_total = 0;

    for (size_t i = 0; i < g_drop_count; i++)
    {
        drop_entry e = g_drop_entries[i];
        filter_total++;

        if (req->artefacts_only && e.group_kind != DROP_GROUP_ARTIFACT)
            continue;

        if (e.group_kind == DROP_GROUP_ARTIFACT)
        {
            /* Skip artefacts if not allowed by the drop request */
            if (!req->allow_artefacts || req->quality < DROP_QUALITY_GOOD) {
                filter_artifact++;
                continue;
            }
            
            artefact_type* a_ptr = &a_info[e.group_id];
            /* Skip if the artefact is already present this run, or if the
             * player has already physically seen it and preserved it. */
            if (a_ptr->cur_num || (a_ptr->seen & ART_SEEN_PHYSICAL)) {
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

        if (req->alignment_filter == DROP_ALIGNMENT_FILTER_NOBLE && !e.noble)
            continue;

        if (req->alignment_filter == DROP_ALIGNMENT_FILTER_EVIL && !e.evil)
            continue;

        if (e.noble)
        {
            bool noble_allowed = req->allow_noble
                || ((object_generation_mode != OB_GEN_MODE_CHEST)
                    && req->allow_noble_from_quality
                    && req->quality >= DROP_QUALITY_GOOD);
            if (!noble_allowed)
                continue;
        }

        if (e.evil)
        {
            bool evil_allowed = req->allow_evil
                || ((object_generation_mode != OB_GEN_MODE_CHEST)
                    && req->quality >= DROP_QUALITY_GOOD);
            if (!evil_allowed)
                continue;
        }

        if (!droptype_matches(req, &e)) {
            filter_droptype++;
            continue;
        }

        if ((req->cat_mask & (1U << e.category)) == 0) {
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
        if (penalty_depth < e.min_depth)
            effective_dif += 2 * (e.min_depth - penalty_depth);

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
            if (penalty_depth < e->min_depth)
                effective_dif += 2 * (e->min_depth - penalty_depth);
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

    return rarity_from_schedule(e->alloc_depth, e->alloc_rarity,
        e->num_allocations, depth, 1);
}

static int group_rarity_at_depth(const drop_entry* e, int depth)
{
    int rarity = drop_entry_rarity_at_depth(e, depth);
    if (rarity <= 0)
        return 0;
    return rarity;
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

static drop_group* choose_group(drop_group* groups, int group_count,
    drop_entry* entries, int depth, const drop_request* req)
{
    if (group_count <= 0)
        return NULL;

    /* Use dynamic weights to avoid fixed-buffer overflow when many groups exist */
    int* weights = mem_alloc_array(group_count, int);
    int total = 0;
    for (int i = 0; i < group_count; i++)
    {
        int entry_idx = groups[i].entry_indices[0];
        int w = group_rarity_at_depth(&entries[entry_idx], depth);
        int group_bonus = drop_group_pick_bonus(&groups[i], entries, req);
        if (w > 0)
            w += group_bonus;
        if (req && groups[i].kind == DROP_GROUP_ARTIFACT
            && req->artefact_weight_multiplier > 1)
        {
            w *= req->artefact_weight_multiplier;
        }
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
            int group_bonus = drop_group_pick_bonus(&groups[i], entries, req);
            if (weight > 0)
                weight += group_bonus;
            if (req && groups[i].kind == DROP_GROUP_ARTIFACT
                && req->artefact_weight_multiplier > 1)
            {
                weight *= req->artefact_weight_multiplier;
            }
            gen_log_write("DROP_GROUP",
                "idx=%d kind=%d group_id=%d weight=%d total=%d "
                "entries=%d noble_bonus=%d chosen=%s",
                i, groups[i].kind, groups[i].group_id,
                weight, total,
                groups[i].entry_count, group_bonus,
                (i == chosen) ? "YES" : "no");
        }
        gen_log_write("DROP_GROUP_PICK",
            "pick=%d total=%d chosen_idx=%d", pick, total, chosen);
    }

    mem_free_null(weights);
    return &groups[chosen];
}

static drop_entry* choose_entry_from_group(drop_entry* entries,
    const drop_group* grp, int depth, const drop_request* req)
{
    int base_rarity;
    if (grp->entry_count <= 0)
        return NULL;

    base_rarity = group_rarity_at_depth(&entries[grp->entry_indices[0]], depth);

    int* weights = mem_alloc_array(grp->entry_count, int);
    int total = 0;
    int chosen_slot = grp->entry_count - 1;

    for (int i = 0; i < grp->entry_count; i++)
    {
        int entry_idx = grp->entry_indices[i];
        int weight = drop_entry_pick_weight(&entries[entry_idx], base_rarity,
            grp->entry_count, req);
        weights[i] = weight;
        total += weight;
    }

    if (total <= 0)
    {
        mem_free_null(weights);
        return &entries[grp->entry_indices[0]];
    }

    int pick = rand_int(total);
    for (int i = 0, acc = 0; i < grp->entry_count; i++)
    {
        acc += weights[i];
        if (pick < acc)
        {
            chosen_slot = i;
            break;
        }
    }
    drop_entry* chosen = &entries[grp->entry_indices[chosen_slot]];

    if (gen_log_initialized)
    {
        gen_log_write("DROP_ITEM_SELECT",
            "group_kind=%d group_id=%d entry_count=%d pick=%d total=%d "
            "chosen_slot=%d chosen_weight=%d noble=%s k_idx=%d att=%d ds=%d evn=%d ps=%d",
            grp->kind, grp->group_id, grp->entry_count, pick, total,
            chosen_slot, weights[chosen_slot], chosen->noble ? "yes" : "no",
            chosen->obj.k_idx, chosen->obj.att, chosen->obj.ds,
            chosen->obj.evn, chosen->obj.ps);
    }

    mem_free_null(weights);
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
        if (req->min_depth_penalty_depth < chosen->min_depth)
            effective_dif = chosen->difficulty
                + 2 * (chosen->min_depth - req->min_depth_penalty_depth);
        else
            effective_dif = chosen->difficulty;

        if (chosen->group_kind == DROP_GROUP_ARTIFACT)
            a_idx = chosen->group_id;
        else if (chosen->group_kind == DROP_GROUP_EGO)
            e_idx = chosen->group_id;
        group_kind = chosen->group_kind;
    }

    gen_log_write("DROP",
        "depth=%d cat=%s droptype=%d supply=%s target=%d band=%d..%d bonus=%d "
        "legal_depth=%d penalty_depth=%d cat_mask=0x%x "
        "strict=%zu relaxed=%zu used_relaxed=%s fallback=%s "
        "chosen_k=%d a_idx=%d e_idx=%d base_dif=%d eff_dif=%d min_depth=%d "
        "max_depth=%d rarity_at_depth=%d group_kind=%d",
        req->depth,
        chosen ? drop_category_name(chosen->category) : drop_category_name(req->cat),
        req->droptype,
        req->is_supply ? "yes" : "no", req->base_roll, req->lower, req->upper,
        req->difficulty_bonus, req->legal_depth, req->min_depth_penalty_depth,
        (unsigned)req->cat_mask, strict_count, relaxed_count,
        used_relaxed ? "yes" : "no", fallback ? "yes" : "no",
        chosen ? chosen->obj.k_idx : -1, a_idx, e_idx,
        chosen ? chosen->difficulty : -1, effective_dif,
        chosen ? chosen->min_depth : -1, chosen ? chosen->max_depth : -1,
        chosen ? group_rarity_at_depth(chosen, req->legal_depth) : 0, group_kind);
}

/*
 * Global chest generation context - set by callers (generate.c) before chest generation.
 * Reset to defaults after each generation.
 */
static int g_chest_vault_type = 0;  /* -1=labyrinth jewelled, 0=default/partition, 6=type6, 7=type7, 8=type8, 9=type9 */
static int g_chest_mode = 0;        /* 0=default 50/50, 1=always small, 2=always large */
static int g_chest_material_wood_pct = -1;
static int g_chest_material_steel_pct = -1;
static int g_chest_material_jewel_pct = -1;

static void reset_chest_generation_context(void)
{
    g_chest_vault_type = 0;
    g_chest_mode = 0;
    g_chest_material_wood_pct = -1;
    g_chest_material_steel_pct = -1;
    g_chest_material_jewel_pct = -1;
}

static bool chest_has_custom_material_weights(void)
{
    return g_chest_material_wood_pct >= 0
        && g_chest_material_steel_pct >= 0
        && g_chest_material_jewel_pct >= 0;
}

static void normalize_chest_material_weights(
    int* wooden_pct, int* steel_pct, int* jewelled_pct)
{
    int total;

    if (!wooden_pct || !steel_pct || !jewelled_pct)
        return;

    if (*wooden_pct < 0) *wooden_pct = 0;
    if (*steel_pct < 0) *steel_pct = 0;
    if (*jewelled_pct < 0) *jewelled_pct = 0;

    total = *wooden_pct + *steel_pct + *jewelled_pct;
    if (total <= 0)
    {
        *wooden_pct = 50;
        *steel_pct = 35;
        *jewelled_pct = 15;
        return;
    }

    if (total > 100)
    {
        int overflow = total - 100;

        if (*jewelled_pct >= overflow)
            *jewelled_pct -= overflow;
        else if (*steel_pct >= overflow)
            *steel_pct -= overflow;
        else
            *wooden_pct = MAX(0, *wooden_pct - overflow);
    }
    else if (total < 100)
    {
        *jewelled_pct += (100 - total);
    }
}

static drop_quality chest_material_quality_for_index(int material_index)
{
    if (material_index <= 0)
        return DROP_QUALITY_GOOD;
    if (material_index == 1)
        return DROP_QUALITY_GREAT;
    return DROP_QUALITY_SUPERB;
}

/*
 * Generate a chest according to game design specifications:
 * Material distribution controlled by g_chest_vault_type:
 *   - Labyrinth guaranteed chest: 100% jewelled
 *   - Type 6 vaults: 65% wooden, 35% steel
 *   - Type 7 vaults: 35% wooden, 65% steel
 *   - Type 8 vaults: 100% jewelled
 *   - Type 9 vaults: 100% jewelled
 *   - Default partitions and other vaults: 50% wooden, 35% steel, 15% jewelled
 * Size distribution controlled by g_chest_mode:
 *   - Mode 0 (default): 50/50 small or large
 *   - Mode 1: always small
 *   - Mode 2: always large
 * Chest contents add +5 levels when opened (handled in chest_death())
 */
static bool generate_chest(int depth, const drop_profile* profile, object_type* out)
{
    /* Size distribution based on mode */
    bool is_large;
    bool upgraded = false;
    bool force_steel = p_ptr && (p_ptr->depth == 0);
    if (g_chest_mode == 1)
        is_large = false;
    else if (g_chest_mode == 2)
        is_large = true;
    else
        is_large = one_in_(2);
    
    const int small_svals[] = {
        SV_CHEST_SMALL_WOODEN, SV_CHEST_SMALL_STEEL, SV_CHEST_SMALL_JEWELLED};
    const int large_svals[] = {
        SV_CHEST_LARGE_WOODEN, SV_CHEST_LARGE_STEEL, SV_CHEST_LARGE_JEWELLED};
    
    /* Determine material based on vault type context */
    int material_roll = rand_int(100);  /* 0-99 */
    int material_index;
    drop_quality material_quality;
    
    if (force_steel)
    {
        material_index = 1;
        material_quality = DROP_QUALITY_GREAT;
    }
    else if (chest_has_custom_material_weights())
    {
        int wooden_pct = g_chest_material_wood_pct;
        int steel_pct = g_chest_material_steel_pct;
        int jewelled_pct = g_chest_material_jewel_pct;

        normalize_chest_material_weights(&wooden_pct, &steel_pct, &jewelled_pct);

        if (material_roll < wooden_pct)
            material_index = 0;
        else if (material_roll < wooden_pct + steel_pct)
            material_index = 1;
        else
            material_index = 2;

        material_quality = chest_material_quality_for_index(material_index);
    }
    else if (g_chest_vault_type == -1)  /* Labyrinth guaranteed chest: jewelled only */
    {
        material_index = 2;
        material_quality = DROP_QUALITY_SUPERB;
    }
    else
    {
        /* Base percentages by vault type */
        int wooden_pct, steel_pct;
        
        if (g_chest_vault_type == 6)       /* Type 6: 65 wooden / 35 steel */
        { wooden_pct = 65; steel_pct = 35; }
        else if (g_chest_vault_type == 7)  /* Type 7: 35 wooden / 65 steel */
        { wooden_pct = 35; steel_pct = 65; }
        else if (g_chest_vault_type == 8)  /* Type 8: jewelled only */
        { wooden_pct = 0; steel_pct = 0; }
        else if (g_chest_vault_type == 9)  /* Type 9: jewelled only */
        { wooden_pct = 0; steel_pct = 0; }
        else                               /* Default: 50/35/15 */
        { wooden_pct = 50; steel_pct = 35; }
        
        int jewelled_pct = 100 - wooden_pct - steel_pct;
        
        /* CUR_CHEST_WOOD curse/blessing: shift wooden probability */
        int chest_delta = curse_flag_delta_cur(CUR_CHEST_WOOD);
        if (chest_delta != 0)
        {
            int shift = chest_delta * 20;
            int half = shift / 2;
            wooden_pct += shift;
            steel_pct -= half;
            jewelled_pct -= (shift - half);
            
            /* Clamp to [0, 100] */
            if (wooden_pct < 0) wooden_pct = 0;
            if (wooden_pct > 100) wooden_pct = 100;
            if (steel_pct < 0) steel_pct = 0;
            if (jewelled_pct < 0) jewelled_pct = 0;
            
            /* Renormalize: ensure sum is exactly 100 */
            int total = wooden_pct + steel_pct + jewelled_pct;
            if (total > 100)
            {
                /* Trim from wooden (the shifted value) */
                wooden_pct -= (total - 100);
                if (wooden_pct < 0) wooden_pct = 0;
            }
            else if (total < 100)
            {
                /* Pad jewelled with the remainder */
                jewelled_pct += (100 - total);
            }
        }
        
        /* Roll material from adjusted percentages */
        if (material_roll < wooden_pct)
        {
            material_index = 0;
        }
        else if (material_roll < wooden_pct + steel_pct)
        {
            material_index = 1;
        }
        else
        {
            material_index = 2;
        }

        material_quality = chest_material_quality_for_index(material_index);
    }

    if (rand_int(100) < 10)
    {
        if (material_index < 2)
        {
            material_index++;
            upgraded = true;
        }
        else if (!is_large)
        {
            is_large = true;
            upgraded = true;
        }

        material_quality = chest_material_quality_for_index(material_index);
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
        reset_chest_generation_context();
        return false;
    }
    
    /* Create the chest object */
    object_prep(out, k_idx);
    
    /* Set chest level (pval) at generation time; opening generates contents at +5. */
    out->pval = depth;
    if (out->pval > 25)
        out->pval = 25;
    if (out->pval < 1)
        out->pval = 1;
    
    /* Chest contents are generated at open time, not stored as a theme. */
    (void)profile;
    out->xtra1 = 0;
    
    if (gen_log_initialized)
    {
        gen_log_write("CHEST_GENERATED",
            "depth=%d vault_type=%d mode=%d size=%s material=%s quality=%s difficulty_bonus=%d chest_level=%d sval=%d upgraded=%s",
            depth, g_chest_vault_type, g_chest_mode, is_large ? "large" : "small",
            material_index == 0 ? "wooden" : (material_index == 1 ? "steel" : "jewelled"),
            drop_quality_name(material_quality), difficulty_bonus, out->pval,
            chest_sval, upgraded ? "yes" : "no");
    }
    
    /* Reset context to defaults after generation */
    reset_chest_generation_context();
    
    return true;
}

/*
 * Set chest generation context for vault-specific material distributions.
 * vault_type: -1=labyrinth jewelled, 0=default/partition,
 * 6=65/35 wood/steel, 7=35/65 wood/steel, 8=100 jewelled, 9=100 jewelled
 */
void drop_set_chest_vault_type(int vault_type)
{
    g_chest_vault_type = vault_type;
}

/*
 * Set chest generation context for mode-specific size distributions.
 * mode: 0=default 50/50, 1=always small, 2=always large
 */
void drop_set_chest_mode(int mode)
{
    g_chest_mode = mode;
}

void drop_set_chest_material_weights(int wooden_pct, int steel_pct, int jewelled_pct)
{
    g_chest_material_wood_pct = wooden_pct;
    g_chest_material_steel_pct = steel_pct;
    g_chest_material_jewel_pct = jewelled_pct;
}

void drop_clear_chest_material_weights(void)
{
    g_chest_material_wood_pct = -1;
    g_chest_material_steel_pct = -1;
    g_chest_material_jewel_pct = -1;
}

bool drop_generate_object(int depth, drop_quality quality, int droptype,
    bool allow_artefacts, object_type* out)
{
    return drop_generate_object_profiled(
        depth, quality, droptype, 0, allow_artefacts, NULL, out);
}

static drop_entry* drop_try_pick(drop_request* req, int legal_depth,
    drop_entry** candidates, size_t* cand_count, size_t* strict_count,
    size_t* relaxed_count, bool relaxed, bool fallback)
{
    mem_free_null(*candidates);
    *candidates = NULL;
    *cand_count = 0;
    if (!relaxed)
        *strict_count = 0;
    if (relaxed_count)
        *relaxed_count = 0;

    if (!collect_candidate_entries(req, relaxed, candidates, cand_count))
    {
        if (relaxed)
        {
            if (relaxed_count)
                *relaxed_count = *cand_count;
        }
        else
        {
            *strict_count = *cand_count;
        }

        log_drop_attempt(req, *strict_count,
            relaxed_count ? *relaxed_count : 0, NULL, relaxed, fallback);
        return NULL;
    }

    if (relaxed)
    {
        if (relaxed_count)
            *relaxed_count = *cand_count;
    }
    else
    {
        *strict_count = *cand_count;
    }

    drop_entry* chosen = NULL;
    if (*cand_count > 0)
    {
        if (req->is_supply)
        {
            chosen = choose_supply_entry(*candidates, *cand_count, legal_depth, req);
        }
        else
        {
            drop_group* groups = mem_alloc_array(*cand_count, drop_group);
            int group_cap = (int)(*cand_count);
            int group_count = group_cap;
            if (build_groups(*candidates, *cand_count, groups, &group_count))
            {
                drop_group* grp = choose_group(groups, group_count, *candidates, legal_depth, req);
                if (grp)
                    chosen = choose_entry_from_group(*candidates, grp, legal_depth, req);
            }
            mem_free_null(groups);
        }
    }

    log_drop_attempt(req, *strict_count,
        relaxed_count ? *relaxed_count : 0, chosen, relaxed, fallback);
    return chosen;
}

static void drop_apply_chosen_entry(const drop_entry* chosen, int depth,
    object_type* out)
{
    object_wipe(out);
    object_copy(out, &chosen->obj);

    /* Catalog entries pin baseline weight for stable difficulty bands.
     * Restore the normal live spawn roll once the final affixes are known. */
    object_refresh_weight(out);

    /* Restore runtime quantities (fuel/charges/stacks) that are not baked into templates */
    drop_apply_spawn_quantities(out);
    if (drop_object_is_damaged(out))
    {
        /* Damage is visible wear-and-tear rather than hidden magic. */
        object_aware(out);
        object_known(out);
    }

    if (chosen->group_kind == DROP_GROUP_ARTIFACT)
    {
        artefact_type* a_ptr = &a_info[chosen->group_id];
        if (!a_ptr->cur_num)
            a_ptr->cur_num = 1;
    }
    if (out->tval == TV_ARROW && !artefact_p(out))
    {
        int depth_adjust = MORGOTH_DEPTH - depth;
        out->number = 20 + damroll(1, 10 + MAX(0, depth_adjust));
        if (out->number > 48)
            out->number = 48;
    }
    apply_autoinscription(out);
}

static bool drop_generate_object_internal(int depth, drop_quality quality,
    int min_depth_penalty_depth, int droptype, int extra_bonus, bool allow_artefacts,
    int artefact_weight_multiplier, bool artefacts_only,
    const drop_profile* profile, drop_alignment_filter alignment_filter,
    object_type* out)
{
    if (min_depth_penalty_depth < 1)
        min_depth_penalty_depth = 1;

    /* Handle chest generation specially */
    if (droptype == DROP_TYPE_CHEST)
    {
        return generate_chest(depth, profile, out);
    }
    
    drop_request req = { 0 };
    drop_request_apply_profile(&req, profile);
    int gen_depth = depth;
    int legal_depth = gen_depth;
    if (p_ptr)
    {
        int current_depth = player_generation_depth();
        if (legal_depth > current_depth)
            legal_depth = current_depth;
    }

    req.depth = gen_depth;
    req.quality = quality;
    req.legal_depth = legal_depth;
    req.min_depth_penalty_depth = min_depth_penalty_depth;
    req.difficulty_bonus = extra_bonus + drop_quality_bonus(quality);
    req.is_supply = false;
    req.droptype = droptype;
    req.allow_artefacts = allow_artefacts;
    req.artefacts_only = artefacts_only;
    req.allow_noble = drop_allow_noble;
    req.allow_evil = drop_allow_evil;
    req.allow_noble_from_quality = drop_allow_noble_from_quality;
    req.alignment_filter = alignment_filter;
    req.artefact_weight_multiplier
        = (allow_artefacts && artefact_weight_multiplier > 1)
        ? artefact_weight_multiplier
        : 1;
    if (req.artefact_weight_multiplier > 100)
        req.artefact_weight_multiplier = 100;
    req.noble_rarity_bonus = 0;
    /* Chest contents add a flat rarity bonus to eligible noble items. */
    if (object_generation_mode == OB_GEN_MODE_CHEST && req.allow_noble)
        req.noble_rarity_bonus = DROP_CHEST_NOBLE_RARITY_BONUS;

    /* Supply is normally restricted to normal-quality drops, but chest
     * profiles may opt back in explicitly through partition.txt. */
    bool chest_profile_allows_supply = (object_generation_mode == OB_GEN_MODE_CHEST)
        && profile && (profile->weight_supply > 0);
    bool disallow_supply = (quality > DROP_QUALITY_NORMAL)
        || (object_generation_mode == OB_GEN_MODE_CHEST);
    if (chest_profile_allows_supply)
        disallow_supply = false;
    if (disallow_supply)
    {
        req.cat_weights[DROP_CAT_SUPPLY] = 0;
        req.supply_weights[DROP_SUPPLY_POTION] = 0;
        req.supply_weights[DROP_SUPPLY_HERB] = 0;
        req.supply_weights[DROP_SUPPLY_GEM] = 0;
        req.supply_weights[DROP_SUPPLY_STAFF] = 0;
        req.supply_weights[DROP_SUPPLY_LIGHT] = 0;
        req.supply_weights[DROP_SUPPLY_ARROWS] = 0;
        req.supply_weights[DROP_SUPPLY_TUNNELING] = 0;

        if (req.cat_weights[DROP_CAT_WEAPON] <= 0
            && req.cat_weights[DROP_CAT_ARMOR] <= 0
            && req.cat_weights[DROP_CAT_JEWELRY] <= 0)
        {
            req.cat_weights[DROP_CAT_WEAPON] = DROP_DEFAULT_CAT_WEIGHT;
            req.cat_weights[DROP_CAT_ARMOR] = DROP_DEFAULT_CAT_WEIGHT;
            req.cat_weights[DROP_CAT_JEWELRY] = DROP_DEFAULT_CAT_WEIGHT;
        }
    }

    /* New difficulty formula: 1.25*Depth - 19 + min(1d(25+3D/4),1d(25+3D/4)) */
    /* Use legal_depth (actual dungeon depth) for difficulty, NOT chest bonus depth */
    int sides = 25 + (3 * legal_depth) / 4;
    if (sides < 1) sides = 1;
    int roll1 = dieroll(sides);
    int roll2 = dieroll(sides);
    int min_roll = MIN(roll1, roll2);
    int base_calc = (int)(1.25 * legal_depth) - 19 + min_roll;
    req.base_roll = base_calc + req.difficulty_bonus;
    req.lower = req.base_roll - 2;
    req.upper = req.base_roll + 2;
    req.cat_mask = 0;
    bool negative_target = (req.base_roll < 0);

    if (gen_log_initialized)
    {
        gen_log_write("DROP_TARGET",
            "depth=%d legal_depth=%d min_penalty_depth=%d quality=%s bonus=%d art_mult=%d noble_bonus=%d sides=%d roll1=%d roll2=%d min=%d "
            "base_calc=%d target=%d band=%d..%d",
            depth, legal_depth, min_depth_penalty_depth, drop_quality_name(quality),
            req.difficulty_bonus, req.artefact_weight_multiplier,
            req.noble_rarity_bonus, sides, roll1, roll2, min_roll,
            base_calc, req.base_roll, req.lower, req.upper);
    }

    /* Map droptype to category if provided */
    switch (droptype)
    {
    case DROP_TYPE_WEAPON:
    case DROP_TYPE_EDGED:
    case DROP_TYPE_POLEARM:
    case DROP_TYPE_BOW:
        req.cat = DROP_CAT_WEAPON;
        break;
    case DROP_TYPE_DIGGING:
        if (disallow_supply)
            return false;
        req.cat = DROP_CAT_SUPPLY;
        req.is_supply = true;
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
    case DROP_TYPE_SIMPLE_LIGHTS:
    case DROP_TYPE_TORCHES:
        if (disallow_supply)
            return false;
        req.cat = DROP_CAT_SUPPLY;
        req.is_supply = true;
        break;
    case DROP_TYPE_DAMAGED:
        /* Damaged items can be weapons or armor; never roll supply for this request. */
        req.cat = DROP_CAT_ARMOR;
        req.cat_mask = (1U << DROP_CAT_WEAPON) | (1U << DROP_CAT_ARMOR);
        break;
    default:
        req.cat = roll_category(&req);
        req.cat_mask = (1U << req.cat);
        break;
    }
    if (req.cat == DROP_CAT_SUPPLY)
        req.is_supply = true;
    if (req.cat_mask == 0)
        req.cat_mask = (1U << req.cat);

    if (droptype == DROP_TYPE_TORCHES || droptype == DROP_TYPE_SIMPLE_LIGHTS)
    {
        req.supply_weights[DROP_SUPPLY_POTION] = 0;
        req.supply_weights[DROP_SUPPLY_HERB] = 0;
        req.supply_weights[DROP_SUPPLY_GEM] = 0;
        req.supply_weights[DROP_SUPPLY_STAFF] = 0;
        req.supply_weights[DROP_SUPPLY_LIGHT] = 100;
        req.supply_weights[DROP_SUPPLY_ARROWS] = 0;
        req.supply_weights[DROP_SUPPLY_TUNNELING] = 0;
    }

    if (droptype == DROP_TYPE_DIGGING)
    {
        req.supply_weights[DROP_SUPPLY_POTION] = 0;
        req.supply_weights[DROP_SUPPLY_HERB] = 0;
        req.supply_weights[DROP_SUPPLY_GEM] = 0;
        req.supply_weights[DROP_SUPPLY_STAFF] = 0;
        req.supply_weights[DROP_SUPPLY_LIGHT] = 0;
        req.supply_weights[DROP_SUPPLY_ARROWS] = 0;
        req.supply_weights[DROP_SUPPLY_TUNNELING] = 100;
    }

    if (!req.is_supply && req.upper < DROP_MIN_DIFFICULTY && droptype != DROP_TYPE_DAMAGED)
    {
        if (gen_log_initialized)
        {
            gen_log_write("DROP_SKIP",
                "depth=%d droptype=%d target=%d band=%d..%d (upper<%d)",
                depth, droptype, req.base_roll, req.lower, req.upper, DROP_MIN_DIFFICULTY);
        }
        return false;
    }

    drop_entry* candidates = NULL;
    size_t cand_count = 0;
    size_t strict_count = 0;
    size_t relaxed_count = 0;
    drop_entry* chosen = NULL;

    bool partition_driven_cat = false;
    switch (droptype)
    {
    case DROP_TYPE_WEAPON:
    case DROP_TYPE_EDGED:
    case DROP_TYPE_POLEARM:
    case DROP_TYPE_BOW:
    case DROP_TYPE_DIGGING:
    case DROP_TYPE_ARMOR:
    case DROP_TYPE_SHIELD:
    case DROP_TYPE_BOOTS:
    case DROP_TYPE_CLOAK:
    case DROP_TYPE_GLOVES:
    case DROP_TYPE_HEADGEAR:
    case DROP_TYPE_JEWELRY:
    case DROP_TYPE_POTION:
    case DROP_TYPE_STAFF:
    case DROP_TYPE_SIMPLE_LIGHTS:
    case DROP_TYPE_TORCHES:
    case DROP_TYPE_NOT_DAMAGED:
    case DROP_TYPE_DAMAGED:
        partition_driven_cat = false;
        break;
    default:
        partition_driven_cat = true;
        break;
    }

    /* Initial attempt */
    chosen = drop_try_pick(&req, legal_depth, &candidates, &cand_count,
        &strict_count, &relaxed_count, false, false);

    /* If the band is empty, add categories by partition probability (difficulty categories only). */
    if (!negative_target && !chosen && !req.is_supply && partition_driven_cat
        && (req.cat == DROP_CAT_WEAPON || req.cat == DROP_CAT_ARMOR
            || req.cat == DROP_CAT_JEWELRY))
    {
        drop_category cats[3] = { DROP_CAT_WEAPON, DROP_CAT_ARMOR, DROP_CAT_JEWELRY };
        for (int i = 0; i < 3; i++)
        {
            for (int j = i + 1; j < 3; j++)
            {
                int wi = MAX(0, req.cat_weights[cats[i]]);
                int wj = MAX(0, req.cat_weights[cats[j]]);
                if (wj > wi)
                {
                    drop_category tmp = cats[i];
                    cats[i] = cats[j];
                    cats[j] = tmp;
                }
            }
        }

        for (int i = 0; i < 3 && !chosen; i++)
        {
            drop_category cat = cats[i];
            int w = MAX(0, req.cat_weights[cat]);
            if (w <= 0)
                continue;
            if (req.cat_mask & (1U << cat))
                continue;

            req.cat_mask |= (1U << cat);
            chosen = drop_try_pick(&req, legal_depth, &candidates, &cand_count,
                &strict_count, &relaxed_count, false, true);
        }
    }

    /* If still empty, relax the band downward until something exists. */
    /* Keep widening into negative difficulty bands, but stop at the floor. */
    while (!negative_target && !chosen && !req.is_supply && req.lower > DROP_MIN_DIFFICULTY)
    {
        req.lower--;
        chosen = drop_try_pick(&req, legal_depth, &candidates, &cand_count,
            &strict_count, &relaxed_count, false, true);
    }

    /*
     * Guaranteed artefact drops should exhaust the remaining legal artefact pool
     * before reporting failure. Strict banding is still preferred first so
     * normal difficulty shaping remains intact when it succeeds.
     */
    if (req.artefacts_only && !chosen && !req.is_supply)
    {
        chosen = drop_try_pick(&req, legal_depth, &candidates, &cand_count,
            &strict_count, &relaxed_count, true, true);
    }

    bool ok = (chosen != NULL);

    /* No fallback - if we can't find anything after widening bands, just fail */
    if (!ok && gen_log_initialized)
    {
        gen_log_write("DROP_FAILED",
            "depth=%d cat=%d droptype=%d target=%d - no valid items after retries",
            depth, req.cat, droptype, req.base_roll);
    }

    if (ok)
        drop_apply_chosen_entry(chosen, depth, out);

    mem_free_null(candidates);
    return ok;
}

bool drop_generate_object_with_bonus(int depth, drop_quality quality,
    int droptype, int extra_bonus, bool allow_artefacts, object_type* out)
{
    return drop_generate_object_internal(
        depth, quality, depth, droptype, extra_bonus, allow_artefacts, 1, false,
        NULL, DROP_ALIGNMENT_FILTER_ANY, out);
}

bool drop_generate_object_profiled(int depth, drop_quality quality,
    int droptype, int extra_bonus, bool allow_artefacts,
    const drop_profile* profile, object_type* out)
{
    return drop_generate_object_internal(
        depth, quality, depth, droptype, extra_bonus, allow_artefacts, 1, false,
        profile, DROP_ALIGNMENT_FILTER_ANY, out);
}

bool drop_generate_object_with_bonus_depths(int depth, int min_depth_penalty_depth,
    drop_quality quality, int droptype, int extra_bonus, bool allow_artefacts,
    object_type* out)
{
    return drop_generate_object_internal(depth, quality, min_depth_penalty_depth,
        droptype, extra_bonus, allow_artefacts, 1, false, NULL,
        DROP_ALIGNMENT_FILTER_ANY, out);
}

bool drop_generate_object_profiled_depths(int depth, int min_depth_penalty_depth,
    drop_quality quality, int droptype, int extra_bonus, bool allow_artefacts,
    const drop_profile* profile, object_type* out)
{
    return drop_generate_object_internal(depth, quality, min_depth_penalty_depth,
        droptype, extra_bonus, allow_artefacts, 1, false, profile,
        DROP_ALIGNMENT_FILTER_ANY, out);
}

bool drop_generate_object_profiled_depths_biased(int depth,
    int min_depth_penalty_depth, drop_quality quality, int droptype, int extra_bonus,
    bool allow_artefacts, int artefact_weight_multiplier,
    const drop_profile* profile, object_type* out)
{
    return drop_generate_object_internal(depth, quality, min_depth_penalty_depth,
        droptype, extra_bonus, allow_artefacts, artefact_weight_multiplier, false,
        profile, DROP_ALIGNMENT_FILTER_ANY, out);
}

bool drop_generate_guaranteed_artefact(int depth,
    int min_depth_penalty_depth, drop_quality quality, int droptype,
    const drop_profile* profile, object_type* out)
{
    return drop_generate_object_internal(depth, quality, min_depth_penalty_depth,
        droptype, 0, true, 1, true, profile, DROP_ALIGNMENT_FILTER_ANY, out);
}

bool drop_generate_chasm_sanctum_object(int depth, object_type* out)
{
    drop_request req = { 0 };
    drop_entry* candidates = NULL;
    size_t cand_count = 0;
    size_t strict_count = 0;
    size_t relaxed_count = 0;
    drop_entry* chosen = NULL;
    int legal_depth;
    int penalty_depth;
    bool allow_artefacts = !(adult_no_artefacts || birth_no_artefacts);

    if (!out)
        return false;

    if (depth < 1)
        depth = 1;

    legal_depth = depth;
    penalty_depth = depth + 5;

    if (p_ptr)
    {
        int current_depth = player_generation_depth();
        if (legal_depth > current_depth)
            legal_depth = current_depth;
    }

    drop_request_apply_profile(&req, NULL);
    req.depth = depth;
    req.quality = DROP_QUALITY_SUPERB;
    req.cat = DROP_CAT_WEAPON;
    req.cat_mask = (1U << DROP_CAT_WEAPON)
        | (1U << DROP_CAT_ARMOR)
        | (1U << DROP_CAT_JEWELRY);
    req.legal_depth = legal_depth;
    req.min_depth_penalty_depth = penalty_depth;
    req.difficulty_bonus = DROP_BONUS_SUPERB;
    req.is_supply = false;
    req.droptype = DROP_TYPE_UNTHEMED;
    req.allow_artefacts = allow_artefacts;
    req.artefacts_only = true;
    req.allow_noble = false;
    req.allow_evil = true;
    req.allow_noble_from_quality = false;
    req.alignment_filter = DROP_ALIGNMENT_FILTER_EVIL;
    req.artefact_weight_multiplier = 1;
    req.noble_rarity_bonus = 0;

    /* Chasm sanctums prefer EVIL artefacts in a wider +-5 band around the
     * jewelled-chest style +15 difficulty roll. */
    {
        int sides = 25 + (3 * legal_depth) / 4;
        int roll1;
        int roll2;
        int min_roll;
        int base_calc;

        if (sides < 1)
            sides = 1;
        roll1 = dieroll(sides);
        roll2 = dieroll(sides);
        min_roll = MIN(roll1, roll2);
        base_calc = (int)(1.25 * legal_depth) - 19 + min_roll;
        req.base_roll = base_calc + req.difficulty_bonus;
        req.lower = req.base_roll - 5;
        req.upper = req.base_roll + 5;

        if (gen_log_initialized)
        {
            gen_log_write("DROP_SANCTUM",
                "artefact_pass depth=%d legal_depth=%d target=%d band=%d..%d sides=%d roll1=%d roll2=%d min=%d",
                depth, legal_depth, req.base_roll, req.lower, req.upper,
                sides, roll1, roll2, min_roll);
        }
    }

    if (allow_artefacts && req.upper >= DROP_MIN_DIFFICULTY)
    {
        chosen = drop_try_pick(&req, legal_depth, &candidates, &cand_count,
            &strict_count, &relaxed_count, false, false);
    }

    if (chosen)
    {
        drop_apply_chosen_entry(chosen, depth, out);
        mem_free_null(candidates);
        return true;
    }

    mem_free_null(candidates);

    if (gen_log_initialized)
    {
        gen_log_write("DROP_SANCTUM",
            "fallback_pass depth=%d legal_depth=%d penalty_depth=%d quality=%s alignment=evil_only",
            depth, legal_depth, penalty_depth,
            drop_quality_name(DROP_QUALITY_SUPERB));
    }

    return drop_generate_object_internal(depth, DROP_QUALITY_SUPERB, penalty_depth,
        DROP_TYPE_UNTHEMED, 0, allow_artefacts, 1, false, NULL,
        DROP_ALIGNMENT_FILTER_EVIL, out);
}
