#include "angband.h"
#include "externs.h"
#include "item_set.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "cmd/world/cmd-interact-chest.h"
#include "ui/question.h"

s16b chest_check(int y, int x)
{
    s16b this_o_idx, next_o_idx = 0;

    /* Scan all objects in the grid */
    for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr;

        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Skip unknown chests XXX XXX */
        /* if (!o_ptr->marked) continue; */

        /* Check for chest */
        if (o_ptr->tval == TV_CHEST)
            return (this_o_idx);
    }

    /* No chest */
    return (0);
}

typedef enum
{
    CHEST_ALIGNMENT_STANDARD = 0,
    CHEST_ALIGNMENT_NOBLE = 1,
    CHEST_ALIGNMENT_EVIL = 2,
    CHEST_ALIGNMENT_INVALID = 3
} chest_alignment_type;

static void chest_apply_drop_alignment(chest_alignment_type alignment)
{
    switch (alignment)
    {
    case CHEST_ALIGNMENT_NOBLE:
        drop_allow_noble = true;
        drop_allow_evil = false;
        break;

    case CHEST_ALIGNMENT_EVIL:
        drop_allow_noble = false;
        drop_allow_evil = true;
        break;

    case CHEST_ALIGNMENT_STANDARD:
    default:
        /* Until a themed item appears, chest rolls may pick either alignment. */
        drop_allow_noble = true;
        drop_allow_evil = true;
        break;
    }
}

static chest_alignment_type chest_item_alignment(const object_type* o_ptr)
{
    u32b f1, f2, f3, f4;
    bool noble;
    bool evil;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    noble = (f4 & TR4_NOBLE_ITEM) != 0;
    evil = (f4 & TR4_EVIL_ITEM) != 0;

    if (noble && evil)
        return CHEST_ALIGNMENT_INVALID;
    if (noble)
        return CHEST_ALIGNMENT_NOBLE;
    if (evil)
        return CHEST_ALIGNMENT_EVIL;

    return CHEST_ALIGNMENT_STANDARD;
}

/*
 * Allocate objects upon opening or destroying a chest.
 *
 * Disperse treasures from the given chest, centered at (x,y).  If
 * destroy_typ is an elemental attack, any generated contents vulnerable to
 * that element are destroyed instead of being dropped.
 */
void chest_release_contents(object_type* o_ptr, int y, int x, int destroy_typ)
{
    int number;
    bool generated_an_item = false;
    bool dropped_an_item = false;
    chest_alignment_type chest_alignment = CHEST_ALIGNMENT_STANDARD;
    int destroyed_contents = 0;
    int old_generation_mode = object_generation_mode;
    bool old_allow_noble = drop_allow_noble;
    bool old_allow_evil = drop_allow_evil;

    object_type* i_ptr;

    object_type object_type_body;

    if (!o_ptr || o_ptr->tval != TV_CHEST)
        return;

    /* Determine how much to drop (see above) */
    number = (o_ptr->sval >= SV_CHEST_MIN_LARGE) ? 4 : rand_range(2, 3);

    /* Zero pval means empty chest */
    if (!o_ptr->pval)
        return;

    /* Opening a chest */
    object_generation_mode = OB_GEN_MODE_CHEST;
    chest_apply_drop_alignment(chest_alignment);

    /* Determine the "value" of the items */
    int base_depth = ABS(o_ptr->pval);
    if (base_depth < 1)
        base_depth = 1;

    /* Chest contents are generated at the chest's stored depth. */
    int gen_depth = base_depth;

    /* Min-depth penalties are reduced by +5 from the chest level, so items
     * appearing below their minimum depth have less penalty. */
    int penalty_depth = base_depth + 5;

    level_partition_kind part_kind = LEVEL_PART_NONE;
    if (o_ptr->xtra1 & 0x80)
        part_kind = (level_partition_kind)(o_ptr->xtra1 & 0x7F);
    if (part_kind <= LEVEL_PART_NONE || part_kind >= LEVEL_PART_MAX)
        part_kind = level_partition_kind_for_point(y, x);
    drop_profile part_profile;
    drop_profile_for_partition_kind_source(
        part_kind, PARTITION_DROP_SOURCE_CHEST, &part_profile);

    if (o_ptr->sval == SV_CHEST_PRESENT)
        number = 1;

    /* Chest-specific difficulty bonus */
    drop_quality chest_quality = DROP_QUALITY_NORMAL;
    if ((o_ptr->sval == SV_CHEST_SMALL_WOODEN)
        || (o_ptr->sval == SV_CHEST_LARGE_WOODEN))
        chest_quality = DROP_QUALITY_GOOD;
    else if ((o_ptr->sval == SV_CHEST_SMALL_STEEL)
        || (o_ptr->sval == SV_CHEST_LARGE_STEEL))
        chest_quality = DROP_QUALITY_GREAT;
    else if ((o_ptr->sval == SV_CHEST_SMALL_JEWELLED)
        || (o_ptr->sval == SV_CHEST_LARGE_JEWELLED)
        || (o_ptr->sval == SV_CHEST_PRESENT))
        chest_quality = DROP_QUALITY_SUPERB;

    /* Drop some objects (non-chests) */
    for (; number > 0; --number)
    {
        bool accepted = false;

        for (int attempt = 0; attempt < 64 && !accepted; attempt++)
        {
            /* Get local object */
            i_ptr = &object_type_body;

            /* Wipe the object */
            object_wipe(i_ptr);

            bool ok = drop_generate_object_profiled_depths(gen_depth, penalty_depth,
                chest_quality, DROP_TYPE_UNTHEMED, 0, true, &part_profile, i_ptr);

            if (!ok)
                continue;

            chest_alignment_type item_alignment = chest_item_alignment(i_ptr);

            if (item_alignment == CHEST_ALIGNMENT_INVALID)
                continue;

            if (item_alignment == CHEST_ALIGNMENT_NOBLE
                || item_alignment == CHEST_ALIGNMENT_EVIL)
            {
                if (chest_alignment == CHEST_ALIGNMENT_STANDARD)
                {
                    chest_alignment = item_alignment;
                    chest_apply_drop_alignment(chest_alignment);
                }
                else if (chest_alignment != item_alignment)
                {
                    continue;
                }
            }

            generated_an_item = true;

            if ((destroy_typ >= 0)
                && elemental_attack_destroys_object(destroy_typ, i_ptr))
            {
                destroyed_contents++;
                accepted = true;
                continue;
            }

            drop_near(i_ptr, -1, y, x);
            dropped_an_item = true;
            accepted = true;
        }
    }

    /* No longer opening a chest */
    object_generation_mode = old_generation_mode;
    drop_allow_noble = old_allow_noble;
    drop_allow_evil = old_allow_evil;

    /* Empty */
    o_ptr->pval = 0;

    /*Paranoia, delete chest theme*/
    o_ptr->xtra1 = 0;

    /* Known */
    object_known(o_ptr);

    if (!generated_an_item)
    {
        msg_print("The chest is empty.");
    }
    else if (!dropped_an_item && destroyed_contents > 0)
    {
        msg_print("The chest's contents are ruined.");
    }
    else if (destroyed_contents > 0)
    {
        msg_print("Some of the chest's contents are ruined.");
    }
}

/*
 * Allocate objects upon opening a chest
 *
 * Disperse treasures from the given chest, centered at (x,y).
 *
 */
static void chest_death(int y, int x, s16b o_idx)
{
    chest_release_contents(&o_list[o_idx], y, x, -1);
}

/*
 * Chests have traps too.
 *
 * Exploding chest destroys contents (and traps).
 * Note that the chest itself is never destroyed.
 */
static void chest_trap(int y, int x, s16b o_idx)
{
    int trap, dam;
    int level, bonus, needle_skill;

    object_type* o_ptr = &o_list[o_idx];

    (void)x; // casting to soothe compilation warnings
    (void)y;

    /* Ignore disarmed chests */
    if (o_ptr->pval <= 0)
        return;

    /* The chest's level (pval, 1--25) drives how nasty its trap is: deeper
     * chests deal more damage and their needles strike more accurately. */
    level = o_ptr->pval;
    bonus = level / 6;          /* extra damage dice */
    needle_skill = 2 + level / 4; /* needle accuracy vs the player's DEX */

    /* Obtain the traps */
    trap = object_chest_trap_flags(o_ptr);

    // Store information for the combat rolls window
    combat_roll_special_char = object_char(o_ptr);
    combat_roll_special_attr = object_attr(o_ptr);

    /* Needle - Hallucination */
    if (trap & (CHEST_NEEDLE_HALLU))
    {
        sound(MSG_TRAP_NEEDLE);

        if (skill_check(NULL, needle_skill, p_ptr->stat_use[A_DEX], PLAYER) > 0)
        {
            msg_print("A small needle has pricked you!");
            if (allow_player_image(NULL))
            {
                set_image(p_ptr->image + damroll(80, 4));
            }
            else
            {
                msg_print("You resist the effects!");
            }
        }
        else
        {
            msg_print("A small needle just misses you.");
        }
    }

    /* Needle - Entrancement */
    if (trap & (CHEST_NEEDLE_ENTRANCE))
    {
        sound(MSG_TRAP_NEEDLE);

        if (skill_check(NULL, needle_skill, p_ptr->stat_use[A_DEX], PLAYER) > 0)
        {
            msg_print("A small needle has pricked you!");
            if (allow_player_entrancement(NULL))
            {
                set_entranced(damroll(10, 4));
            }
            else
            {
                msg_print("You resist the effects!");
            }
        }
        else
        {
            msg_print("A small needle just misses you.");
        }
    }

    /* Needle - Lose strength */
    if (trap & (CHEST_NEEDLE_LOSE_STR))
    {
        sound(MSG_TRAP_NEEDLE);

        if (skill_check(NULL, needle_skill, p_ptr->stat_use[A_DEX], PLAYER) > 0)
        {
            msg_print("A small needle has pricked you!");
            (void)do_dec_stat(A_STR, NULL);
        }
        else
        {
            msg_print("A small needle just misses you.");
        }
    }

    /* Confusion Gas */
    if (trap & (CHEST_GAS_CONF))
    {
        sound(MSG_TRAP_GAS);

        msg_print("A noxious vapour escapes from the chest!");
        if (allow_player_confusion(NULL))
        {
            (void)set_confused(p_ptr->confused + damroll(4, 4));
        }
        else
        {
            msg_print("You resist the effects.");
        }
    }

    /* Acrid Smoke */
    if (trap & (CHEST_GAS_STUN))
    {
        sound(MSG_TRAP_GAS);

        msg_print("Acrid smoke pours from the chest!");
        if (allow_player_stun(NULL))
        {
            msg_print("It fills your lungs and your mind reels.");

            dam = damroll(3 + bonus, 4);

            update_combat_rolls1b(NULL, PLAYER, true);
            update_combat_rolls2(
                3 + bonus, 4, dam, -1, -1, 0, 0, GF_HURT, false);

            killer_mark_other(SCORE_KILLER_TRAP);
            take_hit(dam, "a trapped chest");

            set_stun(p_ptr->stun + damroll(30, 4));
        }
        else
        {
            msg_print("You resist the effects.");
        }
    }

    /* Poison Gas */
    if (trap & (CHEST_GAS_POISON))
    {
        sound(MSG_TRAP_GAS);

        msg_print("A noxious vapour escapes from the chest!");

        update_combat_rolls1b(NULL, PLAYER, true);

        (void)pois_dam_pure(10 + bonus * 2, 4, true);
    }

    /* Flame */
    if (trap & (CHEST_FLAME))
    {
        sound(MSG_TRAP_FIRE);

        msg_print("There is a sudden burst of flame!");

        update_combat_rolls1b(NULL, PLAYER, true);

        fire_dam_pure(10 + bonus * 2, 4, true, "a trapped chest");

        /* Make some noise */
        monster_perception(true, false, -5);
    }
}

static void prep_skeleton_food(object_type* o_ptr, byte skeleton_sval)
{
    switch (skeleton_sval)
    {
    case SV_SKELETON_ELF:
        object_prep(o_ptr, lookup_kind(TV_FOOD,
            one_in_(2) ? SV_FOOD_LEMBAS : SV_FOOD_BREAD));
        break;
    case SV_SKELETON_ORC:
        object_prep(o_ptr, lookup_kind(TV_FOOD, SV_FOOD_MEAT));
        break;
    case SV_SKELETON_HUMAN:
    default:
        object_prep(o_ptr, lookup_kind(TV_FOOD,
            one_in_(2) ? SV_FOOD_BREAD : SV_FOOD_MEAT));
        break;
    }

    object_known(o_ptr);
}

static bool prep_skeleton_light(object_type* o_ptr)
{
    int depth = 1;

    if (p_ptr && p_ptr->depth > 0)
        depth = p_ptr->depth;

    object_wipe(o_ptr);
    if (!drop_generate_object(
            depth, DROP_QUALITY_NORMAL, DROP_TYPE_SIMPLE_LIGHTS, false, o_ptr))
        return false;

    object_known(o_ptr);
    return true;
}

static bool skeleton_damaged_item_allowed(byte skeleton_sval, const object_type* o_ptr)
{
    chest_alignment_type alignment = chest_item_alignment(o_ptr);

    if (alignment == CHEST_ALIGNMENT_INVALID)
        return false;
    if (skeleton_sval == SV_SKELETON_ELF && alignment == CHEST_ALIGNMENT_EVIL)
        return false;
    if (skeleton_sval == SV_SKELETON_ORC && alignment == CHEST_ALIGNMENT_NOBLE)
        return false;

    return true;
}

#define SKELETON_GEAR_DIFFICULTY_BONUS 5

static bool generate_skeleton_damaged_item(object_type* o_ptr, byte skeleton_sval,
    bool* no_item_generated)
{
    bool old_allow_noble = drop_allow_noble;
    bool old_allow_evil = drop_allow_evil;
    bool generated_any = false;

    if (no_item_generated)
        *no_item_generated = false;

    drop_allow_noble = (skeleton_sval == SV_SKELETON_HUMAN
        || skeleton_sval == SV_SKELETON_ELF);
    drop_allow_evil = (skeleton_sval == SV_SKELETON_HUMAN
        || skeleton_sval == SV_SKELETON_ORC);

    for (int attempt = 0; attempt < 50; attempt++)
    {
        object_wipe(o_ptr);
        if (!drop_generate_object_with_bonus(object_level, DROP_QUALITY_NORMAL,
                DROP_TYPE_DAMAGED, SKELETON_GEAR_DIFFICULTY_BONUS, false, o_ptr))
            continue;

        generated_any = true;

        if (skeleton_damaged_item_allowed(skeleton_sval, o_ptr))
        {
            drop_allow_noble = old_allow_noble;
            drop_allow_evil = old_allow_evil;
            return false;
        }
    }

    object_wipe(o_ptr);
    drop_allow_noble = old_allow_noble;
    drop_allow_evil = old_allow_evil;
    if (no_item_generated)
        *no_item_generated = !generated_any;
    return true;
}

typedef struct skeleton_note_profile {
    int note_chance;
    int weight_scale[SKEL_HINT_MAX];
} skeleton_note_profile;

typedef struct skeleton_note_state {
    int level_depth;
    int note_cap;
    int notes_shown;
    int map_wid;
    int map_hgt;
    u32b hint_used_mask;
    byte hint_use_counts[SKEL_HINT_MAX];
    byte seen_count;
    s16b seen_ids[SKELETON_NOTE_SEEN_MAX];
} skeleton_note_state;

#define HINT_MESSAGE_MAX 32
#define HINT_MESSAGE_LINES_MAX 16

typedef struct hint_message_state {
    s16b level_depth;
    s16b map_wid;
    s16b map_hgt;
    byte message_count;
    byte line_counts[HINT_MESSAGE_MAX];
    char lines[HINT_MESSAGE_MAX][HINT_MESSAGE_LINES_MAX][100];
    hint_message_meta meta[HINT_MESSAGE_MAX];
} hint_message_state;

static hint_message_state g_hint_message_state = {
    .level_depth = -1
};

#define SKELETON_TIP_MAX_DEPTH 7
#define SKELETON_NOTE_LEVEL_BASE_BLOCKS 9
#define SKELETON_NOTE_LEVEL_RANDOM_ROLL1 17
#define SKELETON_NOTE_LEVEL_RANDOM_ROLL2 14
#define SKELETON_NOTE_LEVEL_MIN_BLOCKS 8
#define SKELETON_NOTE_SMALLER_LEVEL_DELTA 3
#define SKELETON_NOTE_SMALLER_LEVEL_MIN_BLOCKS 6
#define SKELETON_NOTE_HOARD_GUARD_RADIUS 10
#define SKELETON_NOTE_UNBOUNDED_CAP 32767
#define SKEL_HINT_LIMIT_UNLIMITED -1

static skeleton_note_state g_skeleton_note_state = { .level_depth = -1 };
static int g_skeleton_note_entry_count = -1;
static const int skeleton_hint_base_weight[SKEL_HINT_MAX]
    = {
        0,  /* NONE */
        113, /* GREAT_VAULT (was 75 -> 1.5x rounded) */
        60, /* VAULT_ARTIFACT */
        55, /* STAIRS */
        0,  /* PARTITION_PRESENCE (deprecated) */
        45, /* FORGE */
        70, /* UNIQUE */
        180, /* TIP */
        35, /* SIZE */
        180, /* QUEST (doubled from 90) */
        40, /* PART_LABYRINTH */
        40, /* PART_CHASM */
        40, /* PART_CAVE */
        40, /* PART_CAVE_ICE */
        40, /* PART_CAVE_FIRE */
        40, /* PART_CAVE_POIS */
        35, /* PART_ROOMY */
        35, /* PART_RUINED */
        35  /* PART_CAVEY */
    };

/* Legacy count fields kept so old saves can migrate into hint_use_counts. */
#define SKEL_HINT_STAIRS_COUNT_SHIFT 24
#define SKEL_HINT_FORGE_COUNT_SHIFT 26
#define SKEL_HINT_UNIQUE_COUNT_SHIFT 28
#define SKEL_HINT_ARTIFACT_COUNT_SHIFT 30

static void skeleton_note_ensure_level_state(void);
static bool skeleton_note_has_unseen_template(
    byte sval, skeleton_note_role role, skeleton_hint_kind hint);
static int skeleton_note_map_distance(int y1, int x1, int y2, int x2);
static const char* skeleton_note_direction_phrase(int from_y, int from_x, int to_y, int to_x);
static const char* skeleton_note_distance_phrase(int dist,
    const level_layout_info* layout, char* buf, size_t buf_sz);
static int skeleton_note_append_wrapped_text(
    const char* text, char lines[][100], int idx, int limit, int wrap);
static void skeleton_note_recount_templates(void)
{
    g_skeleton_note_entry_count = 0;

    if (!skeleton_note_info || !z_info)
        return;

    for (int i = 0; i < z_info->skeleton_note_max; ++i)
    {
        skeleton_note_template* t = &skeleton_note_info[i];
        if (t->role == SKELETON_NOTE_ROLE_NONE)
            continue;
        if (t->text == 0 || t->weight == 0)
            continue;
        g_skeleton_note_entry_count++;
    }
}

static int skeleton_note_entry_count(void)
{
    if (g_skeleton_note_entry_count < 0)
        skeleton_note_recount_templates();
    return g_skeleton_note_entry_count;
}

static void skeleton_note_reset_seen(void)
{
    g_skeleton_note_state.seen_count = 0;
    for (int i = 0; i < SKELETON_NOTE_SEEN_MAX; ++i)
        g_skeleton_note_state.seen_ids[i] = -1;
}

static bool skeleton_note_seen_id(s16b id)
{
    for (int i = 0; i < g_skeleton_note_state.seen_count; ++i)
    {
        if (g_skeleton_note_state.seen_ids[i] == id)
            return true;
    }
    return false;
}

static void skeleton_note_record_seen(s16b id)
{
    if (id < 0)
        return;
    if (skeleton_note_seen_id(id))
        return;

    if (g_skeleton_note_state.seen_count < SKELETON_NOTE_SEEN_MAX)
    {
        g_skeleton_note_state.seen_ids[g_skeleton_note_state.seen_count++] = id;
        return;
    }

    /* Keep a simple rolling window to reduce repetition across levels. */
    for (int i = 1; i < SKELETON_NOTE_SEEN_MAX; ++i)
        g_skeleton_note_state.seen_ids[i - 1] = g_skeleton_note_state.seen_ids[i];
    g_skeleton_note_state.seen_ids[SKELETON_NOTE_SEEN_MAX - 1] = id;
}

static u32b skeleton_hint_bit(skeleton_hint_kind kind)
{
    return ((u32b)1u << (u32b)kind);
}

static int skeleton_hint_limit(skeleton_hint_kind kind)
{
    switch (kind)
    {
    case SKEL_HINT_NONE:
    case SKEL_HINT_TIP:
        return SKEL_HINT_LIMIT_UNLIMITED;
    case SKEL_HINT_GREAT_VAULT:
    case SKEL_HINT_VAULT_ARTIFACT:
    case SKEL_HINT_QUEST:
        return SKEL_HINT_LIMIT_UNLIMITED;
    case SKEL_HINT_STAIRS:
    case SKEL_HINT_FORGE:
    case SKEL_HINT_UNIQUE_MONSTER:
    case SKEL_HINT_PART_LABYRINTH:
    case SKEL_HINT_PART_CHASM:
    case SKEL_HINT_PART_CAVE:
    case SKEL_HINT_PART_CAVE_ICE:
    case SKEL_HINT_PART_CAVE_FIRE:
    case SKEL_HINT_PART_CAVE_POIS:
        return 5;
    case SKEL_HINT_PART_ROOMY:
    case SKEL_HINT_PART_RUINED:
    case SKEL_HINT_PART_CAVEY:
        return 3;
    case SKEL_HINT_LEVEL_SIZE:
        return 1;
    default:
        return 1;
    }
}

static int skeleton_hint_repeat_shift(skeleton_hint_kind kind)
{
    switch (kind)
    {
    case SKEL_HINT_STAIRS:
        return SKEL_HINT_STAIRS_COUNT_SHIFT;
    case SKEL_HINT_FORGE:
        return SKEL_HINT_FORGE_COUNT_SHIFT;
    case SKEL_HINT_UNIQUE_MONSTER:
        return SKEL_HINT_UNIQUE_COUNT_SHIFT;
    case SKEL_HINT_VAULT_ARTIFACT:
        return SKEL_HINT_ARTIFACT_COUNT_SHIFT;
    default:
        return -1;
    }
}

static int skeleton_hint_count_from_legacy_mask(skeleton_hint_kind kind, u32b state_mask)
{
    u32b bit = skeleton_hint_bit(kind);

    int shift = skeleton_hint_repeat_shift(kind);
    int count = (shift >= 0) ? (int)((state_mask >> shift) & 0x3u) : 0;

    /*
     * Backward compatibility: older saves only persisted the base bit, so
     * treat that as "seen once" if no explicit repeat count is encoded.
     */
    if (count == 0 && (state_mask & bit))
        count = 1;

    return count;
}

static int skeleton_hint_use_count(
    skeleton_hint_kind kind, const byte hint_counts[SKEL_HINT_MAX])
{
    if (!hint_counts || kind <= SKEL_HINT_NONE || kind >= SKEL_HINT_MAX)
        return 0;

    return hint_counts[kind];
}

static bool skeleton_hint_reached_limit(
    skeleton_hint_kind kind, const byte hint_counts[SKEL_HINT_MAX])
{
    int limit = skeleton_hint_limit(kind);

    if (limit == SKEL_HINT_LIMIT_UNLIMITED)
        return false;

    if (limit <= 0)
        return true;

    return (skeleton_hint_use_count(kind, hint_counts) >= limit);
}

static void skeleton_hint_count_mark_used(
    skeleton_hint_kind kind, byte hint_counts[SKEL_HINT_MAX])
{
    int limit = skeleton_hint_limit(kind);

    if (!hint_counts || limit == SKEL_HINT_LIMIT_UNLIMITED || limit <= 0)
        return;

    if (hint_counts[kind] < 255)
        hint_counts[kind]++;
}

static void skeleton_hint_record_used(skeleton_hint_kind kind)
{
    if (kind == SKEL_HINT_TIP || kind == SKEL_HINT_NONE)
        return;

    if (skeleton_hint_limit(kind) != SKEL_HINT_LIMIT_UNLIMITED)
        skeleton_hint_count_mark_used(kind, g_skeleton_note_state.hint_use_counts);

    g_skeleton_note_state.hint_used_mask |= skeleton_hint_bit(kind);
}

static int skeleton_note_generated_side_for_depth_rolls(int depth, int roll1,
    int roll2)
{
    int bonus1;
    int bonus2;
    int blocks;

    if (depth < 0)
        depth = 0;
    if (roll1 < 1)
        roll1 = 1;
    if (roll2 < 1)
        roll2 = 1;

    /* Keep this in sync with cave_gen()'s depth-based level-size formula. */
    bonus1 = (depth + roll1) / 3;
    bonus2 = (depth + roll2) / 3;
    blocks = SKELETON_NOTE_LEVEL_BASE_BLOCKS + MAX(bonus1, bonus2);

    if (blocks > MAX_LEVEL_BLOCKS)
        blocks = MAX_LEVEL_BLOCKS;
    if (blocks < SKELETON_NOTE_LEVEL_MIN_BLOCKS)
        blocks = SKELETON_NOTE_LEVEL_MIN_BLOCKS;

    if (smaller_level_size)
    {
        blocks -= SKELETON_NOTE_SMALLER_LEVEL_DELTA;
        if (blocks < SKELETON_NOTE_SMALLER_LEVEL_MIN_BLOCKS)
            blocks = SKELETON_NOTE_SMALLER_LEVEL_MIN_BLOCKS;
    }

    return blocks * PANEL_HGT;
}

static int skeleton_note_generated_min_side(void)
{
    int depth = p_ptr ? p_ptr->depth : 0;
    return skeleton_note_generated_side_for_depth_rolls(depth, 1, 1);
}

static int skeleton_note_generated_max_side(void)
{
    int depth = p_ptr ? p_ptr->depth : 0;
    return skeleton_note_generated_side_for_depth_rolls(depth,
        SKELETON_NOTE_LEVEL_RANDOM_ROLL1, SKELETON_NOTE_LEVEL_RANDOM_ROLL2);
}

static int skeleton_note_size_bucket(const level_layout_info* layout)
{
    if (!layout)
        return 0;

    /*
     * Size buckets for skeleton-note pacing.
     *
     * Bucket against this depth's actual legal size span rather than the
     * absolute MAX_DUNGEON_* ceiling. Otherwise the generator quickly
     * collapses into "large" descriptors once the depth-scaled floor rises.
     */
    int side = MAX(layout->map_wid, layout->map_hgt);
    int min_side = skeleton_note_generated_min_side();
    int max_side = skeleton_note_generated_max_side();
    int span = max_side - min_side;

    if (side <= 0 || min_side <= 0 || max_side <= 0)
        return 0;
    if (max_side <= min_side || span <= 0)
        return (side >= max_side) ? 3 : 0;

    if (side <= min_side)
        return 0;
    if (side >= max_side)
        return 3;

    int bucket = ((side - min_side) * 4) / (span + 1);
    if (bucket < 0)
        bucket = 0;
    if (bucket > 3)
        bucket = 3;
    return bucket;
}

static int skeleton_note_size_word_bucket(const level_layout_info* layout)
{
    int depth = p_ptr ? p_ptr->depth : 0;
    int side = 0;
    int total = 0;
    int lower = 0;
    int equal = 0;

    if (!layout)
        return 0;

    side = MAX(layout->map_wid, layout->map_hgt);
    if (side <= 0)
        return 0;

    /*
     * For note text, compare the generated size against the generator's
     * actual roll distribution rather than evenly slicing the legal span.
     * cave_gen() deliberately biases upward by taking the max of two rolls,
     * so width-based buckets overstate how often a level is "huge".
     */
    for (int roll1 = 1; roll1 <= SKELETON_NOTE_LEVEL_RANDOM_ROLL1; ++roll1)
    {
        for (int roll2 = 1; roll2 <= SKELETON_NOTE_LEVEL_RANDOM_ROLL2; ++roll2)
        {
            int generated_side
                = skeleton_note_generated_side_for_depth_rolls(depth, roll1, roll2);

            ++total;
            if (generated_side < side)
                ++lower;
            else if (generated_side == side)
                ++equal;
        }
    }

    if (total <= 0)
        return 0;

    /*
     * Use the midpoint percentile for this exact generated size so the most
     * common roll cluster reads as a middle descriptor instead of "vast".
     */
    if (equal > 0)
    {
        s64b numerator = (s64b)(2 * lower + equal) * 4;
        s64b denominator = (s64b)2 * total;
        int bucket = (int)(numerator / denominator);

        if (bucket < 0)
            bucket = 0;
        if (bucket > 3)
            bucket = 3;
        return bucket;
    }

    return skeleton_note_size_bucket(layout);
}

static int skeleton_note_cap_from_layout(const level_layout_info* layout)
{
    (void)layout;
    return SKELETON_NOTE_UNBOUNDED_CAP;
}

static skeleton_note_profile skeleton_note_profile_for_sval(byte sval)
{
    skeleton_note_profile prof;
    memset(&prof, 0, sizeof(prof));
    
    /* Default all hint scales to 100 */
    for (int i = 0; i < SKEL_HINT_MAX; ++i)
        prof.weight_scale[i] = 100;

    switch (sval)
    {
    case SV_SKELETON_ELF:
        prof.note_chance = 83;
        prof.weight_scale[SKEL_HINT_GREAT_VAULT] = 110;
        prof.weight_scale[SKEL_HINT_VAULT_ARTIFACT] = 120;
        prof.weight_scale[SKEL_HINT_STAIRS] = 90;
        prof.weight_scale[SKEL_HINT_PARTITION_PRESENCE] = 0;
        prof.weight_scale[SKEL_HINT_PART_LABYRINTH] = 155;
        prof.weight_scale[SKEL_HINT_PART_CHASM] = 155;
        prof.weight_scale[SKEL_HINT_PART_CAVE] = 155;
        prof.weight_scale[SKEL_HINT_PART_CAVE_ICE] = 155;
        prof.weight_scale[SKEL_HINT_PART_CAVE_FIRE] = 155;
        prof.weight_scale[SKEL_HINT_PART_CAVE_POIS] = 155;
        prof.weight_scale[SKEL_HINT_FORGE] = 110;
        prof.weight_scale[SKEL_HINT_UNIQUE_MONSTER] = 120;
        prof.weight_scale[SKEL_HINT_TIP] = 120;
        break;
    case SV_SKELETON_HUMAN:
        prof.note_chance = 60;
        prof.weight_scale[SKEL_HINT_GREAT_VAULT] = 120;
        prof.weight_scale[SKEL_HINT_VAULT_ARTIFACT] = 105;
        prof.weight_scale[SKEL_HINT_STAIRS] = 140;
        prof.weight_scale[SKEL_HINT_PARTITION_PRESENCE] = 0;
        prof.weight_scale[SKEL_HINT_PART_LABYRINTH] = 95;
        prof.weight_scale[SKEL_HINT_PART_CHASM] = 95;
        prof.weight_scale[SKEL_HINT_PART_CAVE] = 95;
        prof.weight_scale[SKEL_HINT_PART_CAVE_ICE] = 95;
        prof.weight_scale[SKEL_HINT_PART_CAVE_FIRE] = 95;
        prof.weight_scale[SKEL_HINT_PART_CAVE_POIS] = 95;
        prof.weight_scale[SKEL_HINT_FORGE] = 120;
        prof.weight_scale[SKEL_HINT_UNIQUE_MONSTER] = 120;
        prof.weight_scale[SKEL_HINT_TIP] = 240;
        break;
    case SV_SKELETON_ORC:
        prof.note_chance = 38;
        prof.weight_scale[SKEL_HINT_GREAT_VAULT] = 170;
        prof.weight_scale[SKEL_HINT_VAULT_ARTIFACT] = 180;
        prof.weight_scale[SKEL_HINT_STAIRS] = 120;
        prof.weight_scale[SKEL_HINT_PARTITION_PRESENCE] = 0;
        prof.weight_scale[SKEL_HINT_PART_LABYRINTH] = 65;
        prof.weight_scale[SKEL_HINT_PART_CHASM] = 65;
        prof.weight_scale[SKEL_HINT_PART_CAVE] = 65;
        prof.weight_scale[SKEL_HINT_PART_CAVE_ICE] = 65;
        prof.weight_scale[SKEL_HINT_PART_CAVE_FIRE] = 65;
        prof.weight_scale[SKEL_HINT_PART_CAVE_POIS] = 65;
        prof.weight_scale[SKEL_HINT_FORGE] = 160;
        prof.weight_scale[SKEL_HINT_UNIQUE_MONSTER] = 90;
        prof.weight_scale[SKEL_HINT_TIP] = 100;
        break;
    default:
        break;
    }

    return prof;
}

static int skeleton_note_tip_override_chance(byte sval, int depth)
{
    if (depth < 1)
        depth = 1;
    if (depth > SKELETON_TIP_MAX_DEPTH)
        return 0;

    int t = SKELETON_TIP_MAX_DEPTH - depth; /* 0.. */
    int chance = 0;
    switch (sval)
    {
    case SV_SKELETON_HUMAN:
        chance = 35 + t * 6;
        break;
    case SV_SKELETON_ELF:
        chance = 25 + t * 5;
        break;
    case SV_SKELETON_ORC:
        chance = 20 + t * 4;
        break;
    default:
        chance = 0;
        break;
    }

    if (chance > 90)
        chance = 90;
    if (chance < 0)
        chance = 0;
    return chance;
}

static const char* partition_label(level_partition_kind kind, big_cave_type_t cave_type)
{
    switch (kind)
    {
    case LEVEL_PART_LABYRINTH:
        return "a maze of hewn stone";
    case LEVEL_PART_BIG_CAVE:
        switch (cave_type)
        {
        case BIG_CAVE_ICE:
            return "a vast ice cavern";
        case BIG_CAVE_FIRE:
            return "a vast cavern of fire";
        case BIG_CAVE_POIS:
            return "a cavern of poisonous vapours";
        default:
            return "a vast cavern";
        }
    case LEVEL_PART_CHASM:
        return "a chasm with perilous bridges";
    case LEVEL_PART_RUINED:
        return "ruined halls";
    case LEVEL_PART_CAVEY:
        return "natural caves";
    case LEVEL_PART_ROOMY:
        return "long halls of wrought stone";
    default:
        return "wandering tunnels";
    }
}

static const char* partition_hazard_label(level_partition_kind kind, big_cave_type_t cave_type)
{
    switch (kind)
    {
    case LEVEL_PART_LABYRINTH:
        return "its turns and dead ends will unmake your bearings";
    case LEVEL_PART_CHASM:
        return "bridges are narrow, and a single misstep is death";
    case LEVEL_PART_BIG_CAVE:
        switch (cave_type)
        {
        case BIG_CAVE_ICE:
            return "the floor is slick and the cold bites";
        case BIG_CAVE_FIRE:
            return "the air burns and smoke chokes";
        case BIG_CAVE_POIS:
            return "green fumes cling low and sting the lungs";
        default:
            return "sound carries far, and there is little cover";
        }
    default:
        return "";
    }
}

static const char* size_word_for_bucket(int bucket)
{
    switch (bucket)
    {
    case 0:
        return "narrow";
    case 1:
        return "broad";
    case 2:
        return "sprawling";
    default:
        return "vast";
    }
}

static const char* skeleton_note_pick_size_word(const level_layout_info* layout)
{
    int actual = layout ? skeleton_note_size_word_bucket(layout) : 0;
    if (actual < 0)
        actual = 0;
    if (actual > 3)
        actual = 3;

    int weights[4] = {0};
    switch (actual)
    {
    case 0:
        weights[0] = 70;
        weights[1] = 20;
        weights[2] = 8;
        weights[3] = 2;
        break;
    case 1:
        weights[0] = 15;
        weights[1] = 55;
        weights[2] = 25;
        weights[3] = 5;
        break;
    case 2:
        weights[0] = 5;
        weights[1] = 20;
        weights[2] = 55;
        weights[3] = 20;
        break;
    default:
        weights[0] = 2;
        weights[1] = 8;
        weights[2] = 25;
        weights[3] = 65;
        break;
    }

    int total = 0;
    for (int i = 0; i < 4; ++i)
        total += weights[i];

    if (total <= 0)
        return size_word_for_bucket(actual);

    int roll = rand_int(total);
    for (int i = 0; i < 4; ++i)
    {
        if (roll < weights[i])
            return size_word_for_bucket(i);
        roll -= weights[i];
    }

    return size_word_for_bucket(actual);
}

static const char* skeleton_note_fallback_opening(byte sval)
{
    switch (sval)
    {
    case SV_SKELETON_ELF:
        return "Flowing script, penned in calmer hours:";
    case SV_SKELETON_HUMAN:
        return "A hurried note from steadier hands:";
    case SV_SKELETON_ORC:
        return "Jagged scrawl on greasy hide:";
    default:
        return "A brittle note clutched by the bones:";
    }
}

static const char* skeleton_note_fallback_signoff(byte sval)
{
    switch (sval)
    {
    case SV_SKELETON_ELF:
        return "If you endure, tread softly.";
    case SV_SKELETON_HUMAN:
        return "Maybe you'll fare better.";
    case SV_SKELETON_ORC:
        return "Take what we couldn't.";
    default:
        return "";
    }
}

static void hint_messages_clear_for_level(s16b level_depth, s16b map_wid, s16b map_hgt)
{
    g_hint_message_state.level_depth = level_depth;
    g_hint_message_state.map_wid = map_wid;
    g_hint_message_state.map_hgt = map_hgt;
    g_hint_message_state.message_count = 0;
    for (int i = 0; i < HINT_MESSAGE_MAX; ++i)
    {
        g_hint_message_state.line_counts[i] = 0;
        g_hint_message_state.meta[i].source_y = -1;
        g_hint_message_state.meta[i].source_x = -1;
        g_hint_message_state.meta[i].cue_count = 0;
        for (int cue = 0; cue < HINT_MESSAGE_CUE_MAX; ++cue)
        {
            g_hint_message_state.meta[i].cue_dirs[cue][0] = '\0';
            g_hint_message_state.meta[i].cue_dists[cue][0] = '\0';
        }
    }
}

static void hint_message_meta_copy(hint_message_meta* dst, const hint_message_meta* src)
{
    if (!dst)
        return;

    dst->source_y = -1;
    dst->source_x = -1;
    dst->cue_count = 0;
    for (int cue = 0; cue < HINT_MESSAGE_CUE_MAX; ++cue)
    {
        dst->cue_dirs[cue][0] = '\0';
        dst->cue_dists[cue][0] = '\0';
    }

    if (!src)
        return;

    dst->source_y = src->source_y;
    dst->source_x = src->source_x;
    dst->cue_count = MIN(src->cue_count, HINT_MESSAGE_CUE_MAX);
    for (int cue = 0; cue < HINT_MESSAGE_CUE_MAX; ++cue)
    {
        strnfmt(dst->cue_dirs[cue], HINT_MESSAGE_CUE_TEXT_MAX, "%s",
            (cue < dst->cue_count) ? src->cue_dirs[cue] : "");
        strnfmt(dst->cue_dists[cue], HINT_MESSAGE_CUE_TEXT_MAX, "%s",
            (cue < dst->cue_count) ? src->cue_dists[cue] : "");
    }
}

static int hint_messages_push_internal(const char lines[][100], int line_count,
    const hint_message_meta* meta)
{
    if (line_count <= 0)
        return -1;
    if (line_count > HINT_MESSAGE_LINES_MAX)
        line_count = HINT_MESSAGE_LINES_MAX;

    int slot = g_hint_message_state.message_count;
    if (slot >= HINT_MESSAGE_MAX)
    {
        for (int i = 1; i < HINT_MESSAGE_MAX; ++i)
        {
            g_hint_message_state.line_counts[i - 1] = g_hint_message_state.line_counts[i];
            hint_message_meta_copy(&g_hint_message_state.meta[i - 1],
                &g_hint_message_state.meta[i]);
            for (int j = 0; j < HINT_MESSAGE_LINES_MAX; ++j)
                strnfmt(g_hint_message_state.lines[i - 1][j], 100, "%s",
                    g_hint_message_state.lines[i][j]);
        }
        slot = HINT_MESSAGE_MAX - 1;
    }
    else
    {
        g_hint_message_state.message_count++;
    }

    g_hint_message_state.line_counts[slot] = (byte)line_count;
    hint_message_meta_copy(&g_hint_message_state.meta[slot], meta);
    for (int i = 0; i < line_count; ++i)
        strnfmt(g_hint_message_state.lines[slot][i], 100, "%s", lines[i]);
    for (int i = line_count; i < HINT_MESSAGE_LINES_MAX; ++i)
        g_hint_message_state.lines[slot][i][0] = '\0';

    return slot;
}

void hint_messages_level_reset(void)
{
    hint_messages_clear_for_level(p_ptr->depth, p_ptr->cur_map_wid, p_ptr->cur_map_hgt);
}

void hint_messages_ensure_level_state(void)
{
    if (g_hint_message_state.level_depth != p_ptr->depth
        || g_hint_message_state.map_wid != p_ptr->cur_map_wid
        || g_hint_message_state.map_hgt != p_ptr->cur_map_hgt)
    {
        hint_messages_level_reset();
    }
}

byte hint_messages_count_for_save(void)
{
    hint_messages_ensure_level_state();
    return g_hint_message_state.message_count;
}

s16b hint_messages_level_depth_for_save(void)
{
    hint_messages_ensure_level_state();
    return g_hint_message_state.level_depth;
}

s16b hint_messages_map_wid_for_save(void)
{
    hint_messages_ensure_level_state();
    return g_hint_message_state.map_wid;
}

s16b hint_messages_map_hgt_for_save(void)
{
    hint_messages_ensure_level_state();
    return g_hint_message_state.map_hgt;
}

byte hint_messages_message_line_count(int index)
{
    if (index < 0 || index >= g_hint_message_state.message_count)
        return 0;
    return g_hint_message_state.line_counts[index];
}

const char* hint_messages_message_line(int index, int line)
{
    if (index < 0 || index >= g_hint_message_state.message_count)
        return "";
    if (line < 0 || line >= g_hint_message_state.line_counts[index])
        return "";
    return g_hint_message_state.lines[index][line];
}

typedef struct hint_platform_text_rule {
    const char* keyboard;
    const char* controller;
    const char* touch;
} hint_platform_text_rule;

/*
 * Skeleton-note text is stored in a platform-neutral save format: the
 * keyboard wording remains the canonical source text, then this presentation
 * helper adapts control instructions to the active UI.  That also updates
 * hints restored from older saves rather than leaving archived Alt-key text
 * visible on mobile.
 */
static const hint_platform_text_rule hint_platform_text_rules[] = {
    {
        "You can move more quietly using stealth mode with capital 'S'.",
        "Open the character action wheel and choose Stealth to move more "
            "quietly.",
        "Tap your character to open the action wheel, then tap Stealth to "
            "move more quietly."
    },
    {
        "You can reassign controls in the options.",
        "You can reassign controller controls in Options > Input Options > "
            "Controller Settings.",
        "You can reassign touch controls in Options > Input Options > Touch "
            "Settings."
    },
    {
        "You can zoom the main map by Alt+'+' or Alt+'-'.",
        "You can change the main-map zoom in Options > Window Options.",
        "Pinch the main map with two fingers to zoom in or out."
    },
    {
        "You can hide or unhide the right panel by Alt+'i'.",
        "You can show or hide side panes in Options > Window Options.",
        "Tap a Quick Access button to use it; hold the button to change its "
            "command."
    },
    {
        "You can hide or unhide the bottom panel by Alt+'l'.",
        "You can show or hide bottom panes in Options > Window Options.",
        "Tap the combat, status, depth, or rolls regions to open their "
            "overlays."
    }
};

static void hint_text_replace_all(const char* src, const char* from,
    const char* to, char* out, size_t out_sz)
{
    const char* cursor;
    const char* match;
    size_t from_len;
    size_t used = 0;

    if (!out || out_sz == 0)
        return;
    out[0] = '\0';
    if (!src || !from || !from[0] || !to)
        return;

    cursor = src;
    from_len = strlen(from);
    while ((match = strstr(cursor, from)) != NULL)
    {
        size_t prefix_len = (size_t)(match - cursor);
        size_t available = out_sz - used - 1;
        size_t copy_len = MIN(prefix_len, available);

        if (copy_len > 0)
        {
            memcpy(out + used, cursor, copy_len);
            used += copy_len;
            out[used] = '\0';
        }
        if (copy_len < prefix_len)
            return;

        SDL_strlcat(out, to, out_sz);
        used = strlen(out);
        if (used >= out_sz - 1)
            return;
        cursor = match + from_len;
    }

    if (used < out_sz - 1)
        SDL_strlcat(out, cursor, out_sz);
}

void hint_text_for_current_platform(const char* src, char* out,
    size_t out_sz)
{
    char current[2048];
    char replaced[2048];
    bool touch = sdl_touch_tutorial_device_available();
    bool controller = !touch && steamdeck_controls_active();

    if (!out || out_sz == 0)
        return;

    SDL_strlcpy(current, src ? src : "", sizeof(current));
    if (!touch && !controller)
    {
        SDL_strlcpy(out, current, out_sz);
        return;
    }

    for (int i = 0; i < (int)N_ELEMENTS(hint_platform_text_rules); ++i)
    {
        const hint_platform_text_rule* rule = &hint_platform_text_rules[i];
        const char* replacement = touch ? rule->touch : rule->controller;

        hint_text_replace_all(current, rule->keyboard, replacement, replaced,
            sizeof(replaced));
        SDL_strlcpy(current, replaced, sizeof(current));
    }

    SDL_strlcpy(out, current, out_sz);
}

void hint_messages_message_meta(int index, hint_message_meta* out)
{
    if (!out)
        return;

    if (index < 0 || index >= g_hint_message_state.message_count)
    {
        hint_message_meta_copy(out, NULL);
        return;
    }

    hint_message_meta_copy(out, &g_hint_message_state.meta[index]);
}

static void hint_messages_trim_copy(const char* src, char* out, size_t out_sz)
{
    const char* start = src ? src : "";
    size_t len;

    if (!out || out_sz == 0)
        return;

    while (*start == ' ')
        start++;

    len = strlen(start);
    while (len > 0 && start[len - 1] == ' ')
        len--;

    if (len >= out_sz)
        len = out_sz - 1;

    memcpy(out, start, len);
    out[len] = '\0';
}

static bool hint_messages_contains_ci(const char* haystack, const char* needle)
{
    size_t needle_len;

    if (!haystack || !needle || !needle[0])
        return false;

    needle_len = strlen(needle);
    for (const char* p = haystack; *p; p++)
    {
        if (SDL_strncasecmp(p, needle, needle_len) == 0)
            return true;
    }

    return false;
}

static bool hint_messages_title_part_is_tutorial(const char* part)
{
    return hint_messages_contains_ci(part, "Survival Tip")
        || hint_messages_contains_ci(part, "Tutorial");
}

static void hint_messages_append_part(char* out, size_t out_sz,
    const char* part)
{
    size_t cur;

    if (!out || out_sz == 0 || !part || !part[0])
        return;

    cur = strlen(out);
    if (cur >= out_sz - 1)
        return;

    if (cur > 0)
        cur += strnfmt(out + cur, out_sz - cur, " & ");
    if (cur < out_sz - 1)
        (void)strnfmt(out + cur, out_sz - cur, "%s", part);
}

static void hint_messages_filtered_title(int index, char* out, size_t out_sz)
{
    char work[128];
    char* title;
    char* segment;

    if (!out || out_sz == 0)
        return;

    out[0] = '\0';
    work[0] = '\0';
    if (hint_messages_message_line_count(index) <= 0)
        return;

    for (int li = 0; li < hint_messages_message_line_count(index); ++li)
    {
        const char* line = hint_messages_message_line(index, li);

        if (line && line[0])
        {
            strnfmt(work, sizeof(work), "%s", line);
            break;
        }

        if (li + 1 >= hint_messages_message_line_count(index))
            return;
    }

    title = work;
    while (*title == ' ')
        title++;
    if (SDL_strncasecmp(title, "Hint:", 5) == 0)
        title += 5;

    segment = title;
    while (segment && *segment)
    {
        char part[80];
        char* next = strchr(segment, '&');

        if (next)
        {
            *next = '\0';
            next++;
        }

        hint_messages_trim_copy(segment, part, sizeof(part));
        if (part[0] && !hint_messages_title_part_is_tutorial(part))
            hint_messages_append_part(out, out_sz, part);

        segment = next;
    }
}

static bool hint_messages_first_title_is_tutorial(int index)
{
    if (hint_messages_message_line_count(index) <= 0)
        return false;

    for (int li = 0; li < hint_messages_message_line_count(index); ++li)
    {
        char title[128];
        const char* line = hint_messages_message_line(index, li);

        hint_messages_trim_copy(line, title, sizeof(title));
        if (!title[0])
            continue;

        return hint_messages_contains_ci(title, "Survival Tip")
            || hint_messages_contains_ci(title, "Tutorial");
    }

    return false;
}

static void hint_messages_first_body_line(int index, char* out, size_t out_sz)
{
    bool skipped_title = false;

    if (!out || out_sz == 0)
        return;

    out[0] = '\0';
    if (hint_messages_message_line_count(index) <= 0)
        return;

    for (int li = 0; li < hint_messages_message_line_count(index); ++li)
    {
        char line[128];

        hint_messages_trim_copy(
            hint_messages_message_line(index, li), line, sizeof(line));
        if (!line[0])
            continue;

        if (!skipped_title && SDL_strncasecmp(line, "Hint:", 5) == 0)
        {
            skipped_title = true;
            continue;
        }

        strnfmt(out, out_sz, "%s", line);
        return;
    }
}

static void hint_messages_format_cues(const hint_message_meta* meta, char* out,
    size_t out_sz)
{
    size_t cur = 0;

    if (!out || out_sz == 0)
        return;

    out[0] = '\0';
    if (!meta)
        return;

    for (int cue = 0; cue < meta->cue_count; ++cue)
    {
        const char* dist = meta->cue_dists[cue];
        const char* dir = meta->cue_dirs[cue];

        if ((!dist || !dist[0]) && (!dir || !dir[0]))
            continue;

        if (cur > 0)
            cur += strnfmt(out + cur, out_sz - cur, "; ");
        if (cur >= out_sz - 1)
            return;

        if (dist && dist[0] && dir && dir[0])
            cur += strnfmt(out + cur, out_sz - cur, "%s %s", dist, dir);
        else if (dist && dist[0])
            cur += strnfmt(out + cur, out_sz - cur, "%s", dist);
        else
            cur += strnfmt(out + cur, out_sz - cur, "%s", dir);

        if (cur >= out_sz - 1)
            return;
    }
}

bool hint_messages_short_tip(int index, char* out, size_t out_sz)
{
    hint_message_meta meta;
    char title[96];
    char cues[128];
    char platform_text[512];

    if (!out || out_sz == 0)
        return false;

    out[0] = '\0';
    hint_messages_ensure_level_state();
    if (index < 0 || index >= g_hint_message_state.message_count)
        return false;

    hint_messages_filtered_title(index, title, sizeof(title));
    hint_messages_message_meta(index, &meta);
    hint_messages_format_cues(&meta, cues, sizeof(cues));
    if (!title[0])
    {
        char body[96];

        hint_messages_first_body_line(index, body, sizeof(body));
        if (body[0])
        {
            if (hint_messages_first_title_is_tutorial(index))
                strnfmt(title, sizeof(title), "Survival Tip: %s", body);
            else
                strnfmt(title, sizeof(title), "%s", body);
        }
    }

    if (!title[0] && !cues[0])
        return false;

    if (title[0] && cues[0])
        strnfmt(out, out_sz, "%s - %s", title, cues);
    else if (title[0])
        strnfmt(out, out_sz, "%s", title);
    else
        strnfmt(out, out_sz, "%s", cues);

    hint_text_for_current_platform(out, platform_text, sizeof(platform_text));
    SDL_strlcpy(out, platform_text, out_sz);
    return out[0] != '\0';
}

bool hint_messages_short_tip_for_source(int y, int x, char* out, size_t out_sz)
{
    if (!out || out_sz == 0)
        return false;

    out[0] = '\0';
    hint_messages_ensure_level_state();

    for (int i = g_hint_message_state.message_count - 1; i >= 0; --i)
    {
        hint_message_meta meta;

        hint_messages_message_meta(i, &meta);
        if (meta.source_y != y || meta.source_x != x)
            continue;

        if (hint_messages_short_tip(i, out, out_sz))
            return true;
    }

    return false;
}

void hint_messages_clear_for_load(s16b level_depth, s16b map_wid, s16b map_hgt)
{
    hint_messages_clear_for_level(level_depth, map_wid, map_hgt);
}

int hint_messages_add_for_load(const char lines[][100], int line_count,
    const hint_message_meta* meta)
{
    return hint_messages_push_internal(lines, line_count, meta);
}

int hint_messages_add_note_lines(const char note_lines[][100],
    const hint_message_meta* meta)
{
    hint_messages_ensure_level_state();

    int line_count = 0;
    while (line_count < HINT_MESSAGE_LINES_MAX && note_lines[line_count][0])
        line_count++;

    return hint_messages_push_internal(note_lines, line_count, meta);
}

static bool level_has_greater_vault(void)
{
    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            if (cave_info[y][x] & CAVE_G_VAULT)
                return true;
        }
    }
    return false;
}

static const vault_type* skeleton_note_current_greater_vault(void)
{
    if (!p_ptr || !z_info || !v_info || !v_name)
        return NULL;

    /*
     * Before first entry, g_vault_name is the authoritative identity for the
     * current level's vault.
     */
    if (g_vault_name[0])
    {
        for (int i = 0; i < z_info->v_max; ++i)
        {
            const vault_type* v_ptr = &v_info[i];
            if (v_ptr->typ != 8 || !v_ptr->name)
                continue;
            if (streq(v_name + v_ptr->name, g_vault_name))
                return v_ptr;
        }
    }

    /*
     * Entry clears g_vault_name, but greater_vaults retains generated vault
     * indices for the run.  The newest one is the vault on this level.  Do
     * not apply that fallback on the Gates or Morgoth's special level, whose
     * CAVE_G_VAULT areas are not type-8 greater vaults.
     */
    if (p_ptr->depth == 0 || p_ptr->depth == MORGOTH_DEPTH)
        return NULL;

    for (int i = MAX_GREATER_VAULTS - 1; i >= 0; --i)
    {
        int v_idx = p_ptr->greater_vaults[i];
        if (v_idx <= 0 || v_idx >= z_info->v_max)
            continue;
        if (v_info[v_idx].typ == 8)
            return &v_info[v_idx];
    }

    return NULL;
}

static cptr skeleton_note_current_greater_vault_hint(void)
{
    const vault_type* v_ptr = skeleton_note_current_greater_vault();

    if (!v_ptr || !v_text || !v_ptr->skeleton_hint)
        return NULL;
    return v_text + v_ptr->skeleton_hint;
}

static bool skeleton_note_artefact_seen_and_identified(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->name1 || !z_info || o_ptr->name1 >= z_info->art_max)
        return false;

    artefact_type* a_ptr = &a_info[o_ptr->name1];
    bool seen = ((o_ptr->ident & IDENT_ARTIFACT_SEEN) != 0)
        || ((a_ptr->seen & ART_SEEN_PHYSICAL) != 0);
    bool identified = object_known_p(o_ptr) || (a_ptr->found_num > 0);

    return seen && identified;
}

static bool skeleton_note_artefact_hint_target_ok(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;
    if (o_ptr->held_m_idx)
        return false;
    if (!artefact_p(o_ptr))
        return false;
    if (o_ptr->iy >= p_ptr->cur_map_hgt || o_ptr->ix >= p_ptr->cur_map_wid)
        return false;
    if (skeleton_note_artefact_seen_and_identified(o_ptr))
        return false;

    return true;
}

static bool level_has_artefact_hint_target(void)
{
    for (int i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];
        if (skeleton_note_artefact_hint_target_ok(o_ptr))
            return true;
    }
    return false;
}

static bool level_has_stairs_down(void)
{
    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            int feat = cave_feat[y][x];
            if (feat == FEAT_MORE || feat == FEAT_MORE_SHAFT)
                return true;
        }
    }
    return false;
}

static bool level_has_stairs_up(void)
{
    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            int feat = cave_feat[y][x];
            if (feat == FEAT_LESS || feat == FEAT_LESS_SHAFT)
                return true;
        }
    }
    return false;
}

static bool level_has_forge(void)
{
    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            if (cave_forge_bold(y, x))
                return true;
        }
    }
    return false;
}

static bool level_has_partition_kind(level_partition_kind kind)
{
    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            if (level_partition_kind_for_point(y, x) == kind)
                return true;
        }
    }
    return false;
}

static bool skeleton_note_is_quest_giver_r_idx(int r_idx)
{
    switch (r_idx)
    {
    case R_IDX_TULKAS:
    case R_IDX_AULE:
    case R_IDX_MANDOS:
    case R_IDX_NIENA:
    case R_IDX_OROME:
    case R_IDX_VARDA:
        return true;
    default:
        return false;
    }
}

static const char* skeleton_note_quest_site_name(int r_idx)
{
    switch (r_idx)
    {
    case R_IDX_TULKAS:
        return "a strong Power";
    case R_IDX_AULE:
        return "a smith of the West";
    case R_IDX_MANDOS:
        return "a doomsman in shadow";
    case R_IDX_NIENA:
        return "a lady of pity";
    case R_IDX_OROME:
        return "a hunter of the West";
    case R_IDX_VARDA:
        return "a lady of the stars";
    case R_IDX_DURUIN:
        return "a bastion of shadow";
    default:
        return "a Power";
    }
}

static bool level_has_quest_giver(void)
{
    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        if (!m_ptr->r_idx)
            continue;
        if (skeleton_note_is_quest_giver_r_idx(m_ptr->r_idx))
            return true;
    }
    return false;
}

static bool level_has_quest_vault(void)
{
    if (p_ptr->aule_level == p_ptr->depth && p_ptr->aule_quest != AULE_QUEST_NOT_STARTED)
        return true;
    if (p_ptr->mandos_level == p_ptr->depth && p_ptr->mandos_quest != MANDOS_QUEST_NOT_STARTED)
        return true;
    if (p_ptr->varda_level == p_ptr->depth && p_ptr->varda_vault_placed)
        return true;

    /* Fallback: detect Duruin's bastion by its guardian. */
    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        if (!m_ptr->r_idx)
            continue;
        if (m_ptr->r_idx == R_IDX_DURUIN)
            return true;
    }

    return false;
}

static void skeleton_note_big_cave_counts(
    const level_layout_info* layout, int counts[BIG_CAVE_TYPE_MAX], int* out_none)
{
    for (int i = 0; i < BIG_CAVE_TYPE_MAX; ++i)
        counts[i] = 0;

    if (out_none)
        *out_none = 0;

    if (!layout || layout->partition_count <= 0 || layout->big_cave_parts <= 0)
        return;

    for (int pi = 0; pi < layout->partition_count; ++pi)
    {
        big_cave_type_t cave_type = level_partition_big_cave_type_for_index(pi);
        if (cave_type > BIG_CAVE_NONE && cave_type < BIG_CAVE_TYPE_MAX)
            counts[cave_type]++;
    }

    int typed = 0;
    for (int t = 1; t < BIG_CAVE_TYPE_MAX; ++t)
        typed += counts[t];

    int none = layout->big_cave_parts - typed;
    if (none < 0)
        none = 0;
    if (out_none)
        *out_none = none;
}

void skeleton_note_level_reset(void)
{
    level_layout_info layout;
    level_layout_info_current(&layout);

    g_skeleton_note_state.level_depth = p_ptr->depth;
    g_skeleton_note_state.map_wid = layout.map_wid;
    g_skeleton_note_state.map_hgt = layout.map_hgt;
    g_skeleton_note_state.note_cap = skeleton_note_cap_from_layout(&layout);
    g_skeleton_note_state.notes_shown = 0;
    g_skeleton_note_state.hint_used_mask = 0;
    memset(g_skeleton_note_state.hint_use_counts, 0,
        sizeof(g_skeleton_note_state.hint_use_counts));
    g_skeleton_note_entry_count = -1;

    if (g_skeleton_note_state.note_cap < 1)
        g_skeleton_note_state.note_cap = 1;

    hint_messages_level_reset();
}

/*
 * Reset hint/skeleton note state completely for a new game.
 * Called from re_init_some_things() to ensure clean state
 * when starting a new game after death without restarting the app.
 */
void reset_hint_skeleton_state(void)
{
    /* Reset hint message state to initial values */
    hint_messages_clear_for_level(-1, 0, 0);
    
    /* Reset skeleton note state to initial values */
    g_skeleton_note_state.level_depth = -1;
    g_skeleton_note_state.map_wid = 0;
    g_skeleton_note_state.map_hgt = 0;
    g_skeleton_note_state.note_cap = 0;
    g_skeleton_note_state.notes_shown = 0;
    g_skeleton_note_state.hint_used_mask = 0;
    memset(g_skeleton_note_state.hint_use_counts, 0,
        sizeof(g_skeleton_note_state.hint_use_counts));
    skeleton_note_reset_seen();
    g_skeleton_note_entry_count = -1;
}

static void skeleton_note_ensure_level_state(void)
{
    if (g_skeleton_note_state.level_depth != p_ptr->depth
        || g_skeleton_note_state.map_wid != p_ptr->cur_map_wid
        || g_skeleton_note_state.map_hgt != p_ptr->cur_map_hgt)
    {
        skeleton_note_level_reset();
    }
}

void skeleton_note_get_state(skeleton_note_state_save* out)
{
    if (!out)
        return;
    skeleton_note_ensure_level_state();
    out->level_depth = (s16b)g_skeleton_note_state.level_depth;
    out->note_cap = (s16b)g_skeleton_note_state.note_cap;
    out->notes_shown = (s16b)g_skeleton_note_state.notes_shown;
    out->map_wid = (s16b)g_skeleton_note_state.map_wid;
    out->map_hgt = (s16b)g_skeleton_note_state.map_hgt;
    out->hint_used_mask = g_skeleton_note_state.hint_used_mask;
    for (int i = 0; i < SKEL_HINT_MAX; ++i)
        out->hint_use_counts[i] = g_skeleton_note_state.hint_use_counts[i];
    out->seen_count = g_skeleton_note_state.seen_count;
    for (int i = 0; i < SKELETON_NOTE_SEEN_MAX; ++i)
        out->seen_ids[i] = g_skeleton_note_state.seen_ids[i];
}

void skeleton_note_set_state(const skeleton_note_state_save* in)
{
    skeleton_note_reset_seen();
    if (!in)
    {
        skeleton_note_level_reset();
        return;
    }
    g_skeleton_note_state.level_depth = in->level_depth;
    g_skeleton_note_state.note_cap = SKELETON_NOTE_UNBOUNDED_CAP;
    g_skeleton_note_state.notes_shown = (in->notes_shown >= 0)
        ? in->notes_shown
        : 0;
    g_skeleton_note_state.map_wid
        = (in->map_wid > 0) ? in->map_wid : p_ptr->cur_map_wid;
    g_skeleton_note_state.map_hgt
        = (in->map_hgt > 0) ? in->map_hgt : p_ptr->cur_map_hgt;
    g_skeleton_note_state.hint_used_mask = (u32b)in->hint_used_mask;
    for (int i = 0; i < SKEL_HINT_MAX; ++i)
    {
        skeleton_hint_kind kind = (skeleton_hint_kind)i;
        int count = in->hint_use_counts[i];
        int limit = skeleton_hint_limit(kind);

        if (count == 0)
            count = skeleton_hint_count_from_legacy_mask(
                kind, g_skeleton_note_state.hint_used_mask);
        if (limit > 0 && count > limit)
            count = limit;
        if (count < 0)
            count = 0;
        if (count > 255)
            count = 255;

        g_skeleton_note_state.hint_use_counts[i] = (byte)count;
    }
    g_skeleton_note_state.seen_count = MIN(in->seen_count, SKELETON_NOTE_SEEN_MAX);
    for (int i = 0; i < SKELETON_NOTE_SEEN_MAX; ++i)
        g_skeleton_note_state.seen_ids[i] = in->seen_ids[i];
    if (g_skeleton_note_state.notes_shown > g_skeleton_note_state.note_cap)
        g_skeleton_note_state.notes_shown = g_skeleton_note_state.note_cap;
}

static bool skeleton_hint_available(skeleton_hint_kind kind,
    const level_layout_info* layout, bool vault_present,
    bool artefact_present, byte sval)
{
    if (kind == SKEL_HINT_TIP && disable_skeleton_note_tutorial)
        return false;

    bool ok = false;

    switch (kind)
    {
    case SKEL_HINT_GREAT_VAULT:
        ok = vault_present;
        break;
    case SKEL_HINT_VAULT_ARTIFACT:
        ok = artefact_present;
        break;
    case SKEL_HINT_STAIRS:
        ok = level_has_stairs_down() || level_has_stairs_up();
        break;
    case SKEL_HINT_PARTITION_PRESENCE:
        ok = false;
        break;
    case SKEL_HINT_FORGE:
        ok = level_has_forge();
        break;
    case SKEL_HINT_UNIQUE_MONSTER:
    {
        for (int i = 1; i < mon_max; i++)
        {
            monster_type *m_ptr = &mon_list[i];
            if (!m_ptr->r_idx) continue;
            monster_race *r_ptr = &r_info[m_ptr->r_idx];
            if (r_ptr->flags1 & RF1_UNIQUE)
            {
                ok = true;
                break;
            }
        }
        break;
    }
    case SKEL_HINT_TIP:
        ok = (p_ptr->depth <= SKELETON_TIP_MAX_DEPTH)
            && (sval == SV_SKELETON_HUMAN || sval == SV_SKELETON_ELF
                || sval == SV_SKELETON_ORC);
        break;
    case SKEL_HINT_LEVEL_SIZE:
        ok = (layout != NULL);
        break;
    case SKEL_HINT_QUEST:
        ok = level_has_quest_giver() || level_has_quest_vault();
        break;
    case SKEL_HINT_PART_LABYRINTH:
        ok = layout && (layout->labyrinth_parts > 0);
        break;
    case SKEL_HINT_PART_CHASM:
        ok = layout && (layout->chasm_parts > 0);
        break;
    case SKEL_HINT_PART_CAVE:
    {
        int counts[BIG_CAVE_TYPE_MAX];
        int none = 0;
        skeleton_note_big_cave_counts(layout, counts, &none);
        ok = (layout && none > 0);
        break;
    }
    case SKEL_HINT_PART_CAVE_ICE:
    {
        int counts[BIG_CAVE_TYPE_MAX];
        int none = 0;
        skeleton_note_big_cave_counts(layout, counts, &none);
        ok = (counts[BIG_CAVE_ICE] > 0);
        break;
    }
    case SKEL_HINT_PART_CAVE_FIRE:
    {
        int counts[BIG_CAVE_TYPE_MAX];
        int none = 0;
        skeleton_note_big_cave_counts(layout, counts, &none);
        ok = (counts[BIG_CAVE_FIRE] > 0);
        break;
    }
    case SKEL_HINT_PART_CAVE_POIS:
    {
        int counts[BIG_CAVE_TYPE_MAX];
        int none = 0;
        skeleton_note_big_cave_counts(layout, counts, &none);
        ok = (counts[BIG_CAVE_POIS] > 0);
        break;
    }
    case SKEL_HINT_PART_ROOMY:
        ok = level_has_partition_kind(LEVEL_PART_ROOMY);
        break;
    case SKEL_HINT_PART_RUINED:
        ok = level_has_partition_kind(LEVEL_PART_RUINED);
        break;
    case SKEL_HINT_PART_CAVEY:
        ok = level_has_partition_kind(LEVEL_PART_CAVEY);
        break;
    default:
        ok = false;
        break;
    }

    if (!ok)
        return false;

    /*
     * Tutorial tips should be repeatable: don't hide them just because we've
     * recently shown every TIP template.
     */
    if (kind != SKEL_HINT_TIP
        && skeleton_hint_limit(kind) != SKEL_HINT_LIMIT_UNLIMITED)
    {
        if (!skeleton_note_has_unseen_template(
                sval, SKELETON_NOTE_ROLE_HINT, kind))
            return false;
    }

    return true;
}

typedef struct
{
    level_partition_kind kind;
    big_cave_type_t big_cave_type;
} skeleton_partition_focus;

static skeleton_partition_focus skeleton_pick_partition_presence(
    const level_layout_info* layout)
{
    skeleton_partition_focus focus;
    focus.kind = LEVEL_PART_NONE;
    focus.big_cave_type = BIG_CAVE_NONE;

    if (!layout)
        return focus;

    int big_cave_counts[BIG_CAVE_TYPE_MAX] = {0};
    for (int pi = 0; pi < layout->partition_count; ++pi)
    {
        big_cave_type_t cave_type = level_partition_big_cave_type_for_index(pi);
        if (cave_type > BIG_CAVE_NONE && cave_type < BIG_CAVE_TYPE_MAX)
            big_cave_counts[cave_type]++;
    }

    int elemental = big_cave_counts[BIG_CAVE_ICE] + big_cave_counts[BIG_CAVE_FIRE]
        + big_cave_counts[BIG_CAVE_POIS];
    int big_none = layout->big_cave_parts - elemental;
    if (big_none < 0)
        big_none = 0;

    struct {
        level_partition_kind kind;
        big_cave_type_t big_cave_type;
        int weight;
    } options[8];
    int n_options = 0;

    if (layout->labyrinth_parts > 0)
    {
        options[n_options].kind = LEVEL_PART_LABYRINTH;
        options[n_options].big_cave_type = BIG_CAVE_NONE;
        options[n_options].weight = layout->labyrinth_parts;
        n_options++;
    }
    if (layout->chasm_parts > 0)
    {
        options[n_options].kind = LEVEL_PART_CHASM;
        options[n_options].big_cave_type = BIG_CAVE_NONE;
        options[n_options].weight = layout->chasm_parts;
        n_options++;
    }
    if (big_none > 0)
    {
        options[n_options].kind = LEVEL_PART_BIG_CAVE;
        options[n_options].big_cave_type = BIG_CAVE_NONE;
        options[n_options].weight = big_none;
        n_options++;
    }
    if (big_cave_counts[BIG_CAVE_ICE] > 0)
    {
        options[n_options].kind = LEVEL_PART_BIG_CAVE;
        options[n_options].big_cave_type = BIG_CAVE_ICE;
        options[n_options].weight = big_cave_counts[BIG_CAVE_ICE];
        n_options++;
    }
    if (big_cave_counts[BIG_CAVE_FIRE] > 0)
    {
        options[n_options].kind = LEVEL_PART_BIG_CAVE;
        options[n_options].big_cave_type = BIG_CAVE_FIRE;
        options[n_options].weight = big_cave_counts[BIG_CAVE_FIRE];
        n_options++;
    }
    if (big_cave_counts[BIG_CAVE_POIS] > 0)
    {
        options[n_options].kind = LEVEL_PART_BIG_CAVE;
        options[n_options].big_cave_type = BIG_CAVE_POIS;
        options[n_options].weight = big_cave_counts[BIG_CAVE_POIS];
        n_options++;
    }

    int total = 0;
    for (int i = 0; i < n_options; ++i)
        total += options[i].weight;

    if (total <= 0)
        return focus;

    int roll = rand_int(total);
    for (int i = 0; i < n_options; ++i)
    {
        if (roll < options[i].weight)
        {
            focus.kind = options[i].kind;
            focus.big_cave_type = options[i].big_cave_type;
            return focus;
        }
        roll -= options[i].weight;
    }

    return focus;
}

static skeleton_hint_kind skeleton_note_choose_hint(
    const skeleton_note_profile* profile, const level_layout_info* layout,
    bool vault_present, bool artefact_present, byte sval,
    const byte hint_counts[SKEL_HINT_MAX], u32b exclude_mask)
{
    int weights[SKEL_HINT_MAX] = {0};
    int total = 0;

    for (int k = 1; k < SKEL_HINT_MAX; ++k)
    {
        skeleton_hint_kind kind = (skeleton_hint_kind)k;
        if (exclude_mask & skeleton_hint_bit(kind))
            continue;

        if (skeleton_hint_reached_limit(kind, hint_counts))
            continue;

        if (!skeleton_hint_available(
                kind, layout, vault_present, artefact_present, sval))
            continue;

        int base = skeleton_hint_base_weight[k];
        
        if (kind == SKEL_HINT_TIP)
        {
            int scaled = SKELETON_TIP_MAX_DEPTH + 1 - p_ptr->depth;
            if (scaled <= 0)
                base = 0;
            else
            {
                if (scaled > SKELETON_TIP_MAX_DEPTH)
                    scaled = SKELETON_TIP_MAX_DEPTH;
                base = (base * scaled) / SKELETON_TIP_MAX_DEPTH;
            }
        }

        int scale = profile->weight_scale[k];

        if (base <= 0 || scale <= 0)
            continue;

        int weight = (base * scale) / 100;
        if (weight < 1)
            weight = 1;

        weights[k] = weight;
        total += weight;
    }

    if (total <= 0)
        return SKEL_HINT_NONE;

    int roll = rand_int(total);
    for (int k = 1; k < SKEL_HINT_MAX; ++k)
    {
        if (weights[k] == 0)
            continue;

        if (roll < weights[k])
            return (skeleton_hint_kind)k;

        roll -= weights[k];
    }

    return SKEL_HINT_NONE;
}

static bool skeleton_note_has_unseen_template(
    byte sval, skeleton_note_role role, skeleton_hint_kind hint)
{
    if (!skeleton_note_info || !z_info)
        return false;

    for (int i = 0; i < z_info->skeleton_note_max; ++i)
    {
        skeleton_note_template* t = &skeleton_note_info[i];
        if (t->role != role || t->weight == 0 || t->text == 0)
            continue;
        if (t->sval != SV_SKELETON_NOTE_ANY && t->sval != sval)
            continue;
        if (role == SKELETON_NOTE_ROLE_HINT && t->hint != hint)
            continue;
        if (!skeleton_note_seen_id((s16b)i))
            return true;
    }
    return false;
}

static s16b skeleton_note_pick_entry_internal(
    byte sval, skeleton_note_role role, skeleton_hint_kind hint, bool allow_seen,
    s16b exclude_id)
{
    if (!skeleton_note_info || !z_info)
        return -1;

    int total = 0;
    int max = z_info->skeleton_note_max;
    for (int i = 0; i < max; ++i)
    {
        skeleton_note_template* t = &skeleton_note_info[i];
        if (t->role != role || t->weight == 0 || t->text == 0)
            continue;
        if (t->sval != SV_SKELETON_NOTE_ANY && t->sval != sval)
            continue;
        if ((s16b)i == exclude_id)
            continue;
        if (role == SKELETON_NOTE_ROLE_HINT && t->hint != hint)
            continue;
        if (!allow_seen && skeleton_note_seen_id((s16b)i))
            continue;
        total += t->weight;
    }

    if (total <= 0)
        return -1;

    int roll = rand_int(total);
    for (int i = 0; i < max; ++i)
    {
        skeleton_note_template* t = &skeleton_note_info[i];
        if (t->role != role || t->weight == 0 || t->text == 0)
            continue;
        if (t->sval != SV_SKELETON_NOTE_ANY && t->sval != sval)
            continue;
        if ((s16b)i == exclude_id)
            continue;
        if (role == SKELETON_NOTE_ROLE_HINT && t->hint != hint)
            continue;
        if (!allow_seen && skeleton_note_seen_id((s16b)i))
            continue;
        if (roll < t->weight)
            return (s16b)i;
        roll -= t->weight;
    }

    return -1;
}

static s16b skeleton_note_pick_entry(
    byte sval, skeleton_note_role role, skeleton_hint_kind hint)
{
    return skeleton_note_pick_entry_internal(sval, role, hint, false, -1);
}

typedef struct skeleton_note_line
{
    const char* tpl;
    level_partition_kind presence_kind;
    big_cave_type_t big_cave_type;
    const char* unique_type;
    const char* dir;
    const char* dist;
    const char* site;
    const char* artefact_kind;
    const char* size_word;
} skeleton_note_line;

static void skeleton_note_normalize_spaces(char* s)
{
    if (!s)
        return;

    /* Trim leading spaces */
    char* p = s;
    while (*p == ' ')
        ++p;
    if (p != s)
        memmove(s, p, strlen(p) + 1);

    /* Collapse multiple spaces */
    char* r = s;
    char* w = s;
    bool prev_space = false;
    while (*r)
    {
        if (*r == ' ')
        {
            if (!prev_space)
                *w++ = ' ';
            prev_space = true;
        }
        else
        {
            *w++ = *r;
            prev_space = false;
        }
        ++r;
    }
    *w = '\0';

    /* Trim trailing spaces */
    size_t len = strlen(s);
    while (len > 0 && s[len - 1] == ' ')
        s[--len] = '\0';
}

static int skeleton_note_append_wrapped_segment_mono(
    const char* seg, char lines[][100], int idx, int limit, int wrap)
{
    if (!seg || !seg[0] || limit <= idx)
        return idx;

    if (wrap < 10)
        wrap = 10;
    if (wrap > 95)
        wrap = 95;

    int len = (int)strlen(seg);
    int pos = 0;
    while (pos < len && idx < limit)
    {
        while (pos < len && seg[pos] == ' ')
            pos++;
        if (pos >= len)
            break;

        int remaining = len - pos;
        int take = (remaining <= wrap) ? remaining : wrap;

        if (remaining > wrap)
        {
            int end = pos + take;
            int split = -1;
            for (int j = end - 1; j > pos; --j)
            {
                if (seg[j] == ' ')
                {
                    split = j;
                    break;
                }
            }
            if (split > pos)
                take = split - pos;
        }

        while (take > 0 && seg[pos + take - 1] == ' ')
            take--;

        if (take <= 0)
            break;

        strnfmt(lines[idx++], 100, "%.*s", take, seg + pos);
        pos += take;
    }

    return idx;
}

static int skeleton_note_max_chars_fit_pixels(const char* text, int max_chars, int max_px, int cell_width)
{
    if (!text || max_chars <= 0)
        return 0;

    if (max_px <= 0 || cell_width <= 0)
        return max_chars;

    int lo = 1;
    int hi = max_chars;
    int best = 1;
    while (lo <= hi)
    {
        int mid = (lo + hi) / 2;
        int w = sdl_story_font_text_width(text, mid);
        if (w <= 0)
            w = mid * cell_width;

        if (w <= max_px)
        {
            best = mid;
            lo = mid + 1;
        }
        else
        {
            hi = mid - 1;
        }
    }

    return best;
}

static void skeleton_note_pad_line_for_story(char line[100], int wrap_cols, int cell_width)
{
    if (!line || wrap_cols <= 0 || cell_width <= 0)
        return;

    int len = (int)strlen(line);
    if (len <= 0)
        return;

    if (wrap_cols > 99)
        wrap_cols = 99;

    int px = sdl_story_font_text_width(line, len);
    if (px <= 0)
        px = len * cell_width;

    int cells_needed = (px + cell_width - 1) / cell_width;
    if (cells_needed < len)
        cells_needed = len;

    /* Safety margin for measurement/render mismatches. */
    if (cells_needed < wrap_cols)
        cells_needed++;

    if (cells_needed > wrap_cols)
        cells_needed = wrap_cols;

    while (len < cells_needed && len < 99)
        line[len++] = ' ';
    line[len] = '\0';
}

static int skeleton_note_append_wrapped_segment_story(
    const char* seg, char lines[][100], int idx, int limit, int wrap_cols)
{
    if (!seg || !seg[0] || limit <= idx)
        return idx;

    if (wrap_cols < 10)
        wrap_cols = 10;
    if (wrap_cols > 95)
        wrap_cols = 95;

    int cell_width = sdl_get_cell_width();
    if (cell_width <= 0)
        return skeleton_note_append_wrapped_segment_mono(seg, lines, idx, limit, wrap_cols);

    int wrap_px = wrap_cols * cell_width;
    int space_px = sdl_story_font_text_width(" ", 1);
    if (space_px <= 0)
        space_px = cell_width;

    int max_line_chars = wrap_cols;
    if (max_line_chars > 99)
        max_line_chars = 99;

    const char* s = seg;
    while (*s && idx < limit)
    {
        while (*s == ' ')
            s++;
        if (!*s)
            break;

        char out[100];
        int out_len = 0;
        int line_px = 0;
        bool first_word = true;

        while (*s)
        {
            while (*s == ' ')
                s++;
            if (!*s)
                break;

            const char* word = s;
            int word_len = 0;
            while (word[word_len] && word[word_len] != ' ')
                word_len++;

            int word_px = sdl_story_font_text_width(word, word_len);
            if (word_px <= 0)
                word_px = word_len * cell_width;

            int add_px = word_px + (first_word ? 0 : space_px);
            int add_chars = word_len + (first_word ? 0 : 1);

            if (!first_word && ((line_px + add_px) > wrap_px || (out_len + add_chars) > max_line_chars))
                break;

            if (first_word && (word_px > wrap_px || word_len > max_line_chars))
            {
                int remaining_chars = max_line_chars - out_len;
                int max_chars = word_len;
                if (max_chars > remaining_chars)
                    max_chars = remaining_chars;
                int fit = skeleton_note_max_chars_fit_pixels(word, max_chars, wrap_px, cell_width);
                if (fit <= 0)
                    fit = 1;
                memcpy(out + out_len, word, fit);
                out_len += fit;
                out[out_len] = '\0';
                s += fit;
                break;
            }

            if (!first_word)
            {
                out[out_len++] = ' ';
                line_px += space_px;
            }

            int copy = word_len;
            if (copy > 99 - out_len)
                copy = 99 - out_len;
            if (copy > max_line_chars - out_len)
                copy = max_line_chars - out_len;
            memcpy(out + out_len, word, copy);
            out_len += copy;
            out[out_len] = '\0';
            line_px += word_px;

            s += word_len;
            first_word = false;
        }

        if (out_len > 0)
        {
            strnfmt(lines[idx], 100, "%s", out);
            skeleton_note_pad_line_for_story(lines[idx], wrap_cols, cell_width);
            idx++;
        }

        while (*s == ' ')
            s++;
    }

    return idx;
}

static int skeleton_note_append_wrapped_segment(
    const char* seg, char lines[][100], int idx, int limit, int wrap_cols)
{
    if (!seg || !seg[0] || limit <= idx)
        return idx;

    if (sdl_story_font_text_width(" ", 1) > 0 && sdl_get_cell_width() > 0)
        return skeleton_note_append_wrapped_segment_story(seg, lines, idx, limit, wrap_cols);

    return skeleton_note_append_wrapped_segment_mono(seg, lines, idx, limit, wrap_cols);
}

static void skeleton_note_expand_template(const char* tpl,
    const level_layout_info* layout, level_partition_kind presence_kind,
    big_cave_type_t big_cave_type, const char* unique_type, const char* dir,
    const char* dist, const char* site, const char* artefact_kind,
    const char* size_word, char* out, size_t out_sz)
{
    const char* part = partition_label(presence_kind, big_cave_type);
    const char* part_hazard = partition_hazard_label(presence_kind, big_cave_type);
    const char* size_word_text = size_word
        ? size_word
        : size_word_for_bucket(layout ? skeleton_note_size_word_bucket(layout) : 0);
    int width = layout ? layout->map_wid : 0;
    int height = layout ? layout->map_hgt : 0;
    const char* dir_text = dir ? dir : "";
    const char* dist_text = dist ? dist : "";
    const char* site_text = site ? site : "";
    const char* art_text = artefact_kind ? artefact_kind : "an artefact";

    size_t w = 0;
    const char* p = tpl ? tpl : "";
    while (*p && w + 1 < out_sz)
    {
        if (*p == '{')
        {
            if (strncmp(p, "{PART}", 6) == 0)
            {
                w += strnfmt(out + w, out_sz - w, "%s", part);
                p += 6;
                continue;
            }
            if (strncmp(p, "{PART_HAZARD}", 13) == 0)
            {
                w += strnfmt(out + w, out_sz - w, "%s", part_hazard);
                p += 13;
                continue;
            }
            if (strncmp(p, "{SIZEWORD}", 10) == 0)
            {
                w += strnfmt(out + w, out_sz - w, "%s", size_word_text);
                p += 10;
                continue;
            }
            if (strncmp(p, "{WIDTH}", 7) == 0)
            {
                w += strnfmt(out + w, out_sz - w, "%d", width);
                p += 7;
                continue;
            }
            if (strncmp(p, "{HEIGHT}", 8) == 0)
            {
                w += strnfmt(out + w, out_sz - w, "%d", height);
                p += 8;
                continue;
            }
            if (strncmp(p, "{UNIQUE_TYPE}", 13) == 0)
            {
                w += strnfmt(out + w, out_sz - w, "%s", unique_type ? unique_type : "creature");
                p += 13;
                continue;
            }
            if (strncmp(p, "{DIR}", 5) == 0)
            {
                w += strnfmt(out + w, out_sz - w, "%s", dir_text);
                p += 5;
                continue;
            }
            if (strncmp(p, "{DIST}", 6) == 0)
            {
                w += strnfmt(out + w, out_sz - w, "%s", dist_text);
                p += 6;
                continue;
            }
            if (strncmp(p, "{SITE}", 6) == 0)
            {
                w += strnfmt(out + w, out_sz - w, "%s", site_text);
                p += 6;
                continue;
            }
            if (strncmp(p, "{SITE_CAP}", 10) == 0)
            {
                char site_cap[64];
                strnfmt(site_cap, sizeof(site_cap), "%s", site_text);
                if (site_cap[0] >= 'a' && site_cap[0] <= 'z')
                    site_cap[0] = (char)(site_cap[0] - 'a' + 'A');
                w += strnfmt(out + w, out_sz - w, "%s", site_cap);
                p += 10;
                continue;
            }
            if (strncmp(p, "{ART_CAP}", 9) == 0)
            {
                char art_cap[64];
                strnfmt(art_cap, sizeof(art_cap), "%s", art_text);
                if (art_cap[0] >= 'a' && art_cap[0] <= 'z')
                    art_cap[0] = (char)(art_cap[0] - 'a' + 'A');
                w += strnfmt(out + w, out_sz - w, "%s", art_cap);
                p += 9;
                continue;
            }
            if (strncmp(p, "{ART}", 5) == 0)
            {
                w += strnfmt(out + w, out_sz - w, "%s", art_text);
                p += 5;
                continue;
            }
            if (strncmp(p, "{MITHRIL}", 9) == 0)
            {
                if (p_ptr && p_ptr->depth >= MITHRIL_VEIN_MIN_DEPTH)
                    w += strnfmt(out + w, out_sz - w, " or mithril");
                p += 9;
                continue;
            }
        }
        out[w++] = *p++;
    }
    out[w] = '\0';
}

static int skeleton_note_append_expanded_lines(const skeleton_note_line* line,
    const level_layout_info* layout, char lines[][100], int idx, int limit, int wrap)
{
    if (!line || !line->tpl || limit <= idx)
        return idx;

    char expanded[512];
    skeleton_note_expand_template(line->tpl, layout, line->presence_kind,
        line->big_cave_type, line->unique_type, line->dir, line->dist,
        line->site, line->artefact_kind, line->size_word, expanded,
        sizeof(expanded));

    char* seg = expanded;
    while (seg && *seg && idx < limit)
    {
        char* next = strchr(seg, '|');
        if (next)
        {
            *next = '\0';
            next++;
        }

        skeleton_note_normalize_spaces(seg);
        if (seg[0])
            idx = skeleton_note_append_wrapped_segment(seg, lines, idx, limit, wrap);

        seg = next;
    }

    return idx;
}

static int skeleton_note_append_wrapped_text(
    const char* text, char lines[][100], int idx, int limit, int wrap)
{
    if (!text || !text[0] || limit <= idx)
        return idx;

    char expanded[512];
    strnfmt(expanded, sizeof(expanded), "%s", text);

    char* seg = expanded;
    while (seg && *seg && idx < limit)
    {
        char* next = strchr(seg, '|');
        if (next)
        {
            *next = '\0';
            next++;
        }

        skeleton_note_normalize_spaces(seg);
        if (seg[0])
            idx = skeleton_note_append_wrapped_segment(seg, lines, idx, limit, wrap);

        seg = next;
    }

    return idx;
}

static const char* skeleton_note_body_separator(byte sval)
{
    switch (sval)
    {
    case SV_SKELETON_ELF:
        return "A second warning follows:";
    case SV_SKELETON_HUMAN:
        return "Another line follows:";
    case SV_SKELETON_ORC:
        return "More scratched below:";
    default:
        return "Another warning follows:";
    }
}

static void skeleton_note_build_lines(const char* opening,
    const skeleton_note_line* body_lines, int body_count, const char* closing,
    const level_layout_info* layout, char lines[][100],
    const char* body_separator)
{
    /* These lines are archived content, not terminal rows.  The native SDL
     * surface wraps them to its measured pixel width when displayed.  Using
     * the narrow portrait Term width here can exhaust the archive before a
     * second hint is appended. */
    const int max_lines = HINT_MESSAGE_LINES_MAX - 2; /* Title + terminator. */
    const int wrap = 95;

    int idx = 0;
    idx = skeleton_note_append_wrapped_text(opening, lines, idx, max_lines, wrap);

    for (int i = 0; i < body_count && idx < max_lines; ++i)
    {
        if (i > 0)
        {
            idx = skeleton_note_append_wrapped_text(body_separator, lines,
                idx, max_lines, wrap);
        }
        idx = skeleton_note_append_expanded_lines(&body_lines[i], layout,
            lines, idx, max_lines, wrap);
    }

    idx = skeleton_note_append_wrapped_text(closing, lines, idx, max_lines, wrap);

    lines[idx][0] = '\0';
}

static const char* skeleton_get_unique_type_name(const monster_race* r_ptr)
{
    if (!r_ptr) return "creature";
    
    if (r_ptr->flags3 & RF3_DRAGON) return "dragon";
    if (r_ptr->flags3 & RF3_RAUKO) return "demon";
    if (r_ptr->flags3 & RF3_UNDEAD) return "spirit";
    if (r_ptr->flags3 & RF3_ORC) return "orc";
    if (r_ptr->flags3 & RF3_TROLL) return "troll";
    if (r_ptr->flags3 & RF3_SPIDER) return "spider";
    if (r_ptr->flags3 & RF3_WOLF) return "wolf";
    if (r_ptr->d_char == 'C') return "hound";
    if (r_ptr->flags3 & RF3_MAN) return "human";
    if (r_ptr->flags3 & RF3_ELF) return "elf";

    return "horror";
}

static int skeleton_note_map_distance(int y1, int x1, int y2, int x2)
{
    return distance(y1, x1, y2, x2);
}

static const char* skeleton_note_direction_phrase(int from_y, int from_x, int to_y, int to_x)
{
    int dy = to_y - from_y;
    int dx = to_x - from_x;

    int sy = (dy > 0) ? 1 : ((dy < 0) ? -1 : 0);
    int sx = (dx > 0) ? 1 : ((dx < 0) ? -1 : 0);

    if (sy == 0 && sx == 0)
        return "here";
    if (sy < 0 && sx == 0)
        return "to the north";
    if (sy < 0 && sx > 0)
        return "to the north-east";
    if (sy == 0 && sx > 0)
        return "to the east";
    if (sy > 0 && sx > 0)
        return "to the south-east";
    if (sy > 0 && sx == 0)
        return "to the south";
    if (sy > 0 && sx < 0)
        return "to the south-west";
    if (sy == 0 && sx < 0)
        return "to the west";
    return "to the north-west";
}

static const char* skeleton_note_distance_phrase(int dist,
    const level_layout_info* layout, char* buf, size_t buf_sz)
{
    int side = layout ? MAX(layout->map_wid, layout->map_hgt) : 0;
    int near_limit = 10;
    int mid_limit = 24;
    int far_limit = 48;
    int max_dist = 0;

    if (side > 0)
    {
        near_limit = MAX(8, side / 8);
        mid_limit = MAX(near_limit + 8, side / 4);
        far_limit = MAX(mid_limit + 8, side / 2);
    }

    if (layout && layout->map_hgt > 0 && layout->map_wid > 0)
    {
        max_dist = skeleton_note_map_distance(
            0, 0, layout->map_hgt - 1, layout->map_wid - 1);
    }

    if (!buf || buf_sz == 0)
        return "";
    if (dist < 0)
        dist = 0;

    if (dist < near_limit)
    {
        strnfmt(buf, buf_sz, "a short way (less than %d squares)",
            near_limit);
        return buf;
    }
    if (dist <= mid_limit)
    {
        int hi = (max_dist > 0) ? MIN(mid_limit, max_dist) : mid_limit;
        strnfmt(buf, buf_sz, "some distance (%d to %d squares)",
            near_limit, hi);
        return buf;
    }
    if (dist <= far_limit)
    {
        int hi = (max_dist > 0) ? MIN(far_limit, max_dist) : far_limit;
        strnfmt(buf, buf_sz, "a long way (%d to %d squares)",
            mid_limit + 1, hi);
        return buf;
    }

    strnfmt(buf, buf_sz, "a very long way (more than %d squares)",
        far_limit);
    return buf;
}

static bool skeleton_note_find_nearest_stairs_kind(
    bool want_down, int from_y, int from_x, int* out_y, int* out_x, int* out_feat, int* out_dist)
{
    int best_y = -1;
    int best_x = -1;
    int best_feat = 0;
    int best_dist = 0;
    int seen = 0;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            int feat = cave_feat[y][x];
            bool ok = want_down ? (feat == FEAT_MORE || feat == FEAT_MORE_SHAFT)
                                : (feat == FEAT_LESS || feat == FEAT_LESS_SHAFT);
            if (!ok)
                continue;

            int dist = skeleton_note_map_distance(from_y, from_x, y, x);
            if (best_y < 0 || dist < best_dist)
            {
                best_y = y;
                best_x = x;
                best_feat = feat;
                best_dist = dist;
                seen = 1;
                continue;
            }

            if (dist == best_dist)
            {
                ++seen;
                if (one_in_(seen))
                {
                    best_y = y;
                    best_x = x;
                    best_feat = feat;
                }
            }
        }
    }

    if (best_y < 0)
        return false;

    if (out_y) *out_y = best_y;
    if (out_x) *out_x = best_x;
    if (out_feat) *out_feat = best_feat;
    if (out_dist) *out_dist = best_dist;
    return true;
}

static bool skeleton_note_find_nearest_stairs(
    byte sval, int from_y, int from_x, int* out_y, int* out_x, int* out_feat, int* out_dist)
{
    int prefer_down = 0;
    switch (sval)
    {
    case SV_SKELETON_ORC:
        prefer_down = 80;
        break;
    case SV_SKELETON_ELF:
        prefer_down = 65;
        break;
    default:
        prefer_down = 55;
        break;
    }

    bool want_down = percent_chance(prefer_down);
    if (want_down)
    {
        if (skeleton_note_find_nearest_stairs_kind(
                true, from_y, from_x, out_y, out_x, out_feat, out_dist))
            return true;
        return skeleton_note_find_nearest_stairs_kind(
            false, from_y, from_x, out_y, out_x, out_feat, out_dist);
    }

    if (skeleton_note_find_nearest_stairs_kind(
            false, from_y, from_x, out_y, out_x, out_feat, out_dist))
        return true;
    return skeleton_note_find_nearest_stairs_kind(
        true, from_y, from_x, out_y, out_x, out_feat, out_dist);
}

static bool skeleton_note_find_nearest_forge(
    int from_y, int from_x, int* out_y, int* out_x, int* out_feat, int* out_dist)
{
    int best_y = -1;
    int best_x = -1;
    int best_feat = 0;
    int best_dist = 0;
    int seen = 0;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            if (!cave_forge_bold(y, x))
                continue;

            int feat = cave_feat[y][x];
            int dist = skeleton_note_map_distance(from_y, from_x, y, x);
            if (best_y < 0 || dist < best_dist)
            {
                best_y = y;
                best_x = x;
                best_feat = feat;
                best_dist = dist;
                seen = 1;
                continue;
            }

            if (dist == best_dist)
            {
                ++seen;
                if (one_in_(seen))
                {
                    best_y = y;
                    best_x = x;
                    best_feat = feat;
                }
            }
        }
    }

    if (best_y < 0)
        return false;

    if (out_y) *out_y = best_y;
    if (out_x) *out_x = best_x;
    if (out_feat) *out_feat = best_feat;
    if (out_dist) *out_dist = best_dist;
    return true;
}

static bool skeleton_note_find_nearest_quest_site(
    int from_y, int from_x, int* out_y, int* out_x, int* out_dist, const char** out_site)
{
    int best_y = -1;
    int best_x = -1;
    int best_dist = 0;
    const char* best_site = NULL;
    int seen = 0;

    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        if (!m_ptr->r_idx)
            continue;

        int r_idx = m_ptr->r_idx;
        if (!skeleton_note_is_quest_giver_r_idx(r_idx) && r_idx != R_IDX_DURUIN)
            continue;

        int dist = skeleton_note_map_distance(from_y, from_x, m_ptr->fy, m_ptr->fx);
        if (best_y < 0 || dist < best_dist)
        {
            best_y = m_ptr->fy;
            best_x = m_ptr->fx;
            best_dist = dist;
            best_site = skeleton_note_quest_site_name(r_idx);
            seen = 1;
            continue;
        }

        if (dist == best_dist)
        {
            ++seen;
            if (one_in_(seen))
            {
                best_y = m_ptr->fy;
                best_x = m_ptr->fx;
                best_site = skeleton_note_quest_site_name(r_idx);
            }
        }
    }

    if (p_ptr->aule_level == p_ptr->depth && p_ptr->aule_quest != AULE_QUEST_NOT_STARTED)
    {
        int y = p_ptr->aule_forge_y;
        int x = p_ptr->aule_forge_x;
        if (in_bounds(y, x) && cave_forge_bold(y, x))
        {
            int dist = skeleton_note_map_distance(from_y, from_x, y, x);
            if (best_y < 0 || dist < best_dist)
            {
                best_y = y;
                best_x = x;
                best_dist = dist;
                best_site = "a forge of strange craft";
                seen = 1;
            }
            else if (dist == best_dist)
            {
                ++seen;
                if (one_in_(seen))
                {
                    best_y = y;
                    best_x = x;
                    best_site = "a forge of strange craft";
                }
            }
        }
    }

    if (p_ptr->mandos_level == p_ptr->depth && p_ptr->mandos_quest != MANDOS_QUEST_NOT_STARTED)
    {
        int y = p_ptr->mandos_vault_y;
        int x = p_ptr->mandos_vault_x;
        if (in_bounds(y, x))
        {
            int dist = skeleton_note_map_distance(from_y, from_x, y, x);
            if (best_y < 0 || dist < best_dist)
            {
                best_y = y;
                best_x = x;
                best_dist = dist;
                best_site = "a hall of doom";
                seen = 1;
            }
            else if (dist == best_dist)
            {
                ++seen;
                if (one_in_(seen))
                {
                    best_y = y;
                    best_x = x;
                    best_site = "a hall of doom";
                }
            }
        }
    }

    if (best_y < 0)
        return false;

    if (out_y) *out_y = best_y;
    if (out_x) *out_x = best_x;
    if (out_dist) *out_dist = best_dist;
    if (out_site) *out_site = best_site ? best_site : "a Power";
    return true;
}

static bool skeleton_note_find_nearest_great_vault(
    int from_y, int from_x, int* out_y, int* out_x, int* out_dist)
{
    int best_y = -1;
    int best_x = -1;
    int best_dist = 0;
    int seen = 0;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            if (!(cave_info[y][x] & CAVE_G_VAULT))
                continue;

            int dist = skeleton_note_map_distance(from_y, from_x, y, x);
            if (best_y < 0 || dist < best_dist)
            {
                best_y = y;
                best_x = x;
                best_dist = dist;
                seen = 1;
                continue;
            }

            if (dist == best_dist)
            {
                ++seen;
                if (one_in_(seen))
                {
                    best_y = y;
                    best_x = x;
                }
            }
        }
    }

    if (best_y < 0)
        return false;

    if (out_y) *out_y = best_y;
    if (out_x) *out_x = best_x;
    if (out_dist) *out_dist = best_dist;
    return true;
}

static const char* skeleton_note_artefact_kind_name(const object_type* o_ptr)
{
    if (!o_ptr)
        return "artefact";

    switch (o_ptr->tval)
    {
    case TV_SWORD:
        return "sword";
    case TV_POLEARM:
        return "spear";
    case TV_HAFTED:
        return "hammer";
    case TV_BOW:
        return "bow";
    case TV_ARROW:
        return "arrow";
    case TV_SOFT_ARMOR:
        return "suit of armour";
    case TV_MAIL:
        return "mail shirt";
    case TV_CLOAK:
        return "cloak";
    case TV_SHIELD:
        return "shield";
    case TV_HELM:
        return "helm";
    case TV_CROWN:
        return "crown";
    case TV_GLOVES:
        return "pair of gloves";
    case TV_BOOTS:
        return "pair of boots";
    case TV_RING:
        return "ring";
    case TV_AMULET:
        return "amulet";
    case TV_LIGHT:
        return "lamp";
    case TV_HORN:
        return "horn";
    case TV_STAFF:
        return "staff";
    case TV_DIGGING:
        return "mattock";
    case TV_GEM:
        return "jewel";
    default:
        return "artefact";
    }
}

static const char* skeleton_note_indefinite_article(const char* noun)
{
    char c = (noun && noun[0]) ? noun[0] : 'a';

    if (c >= 'A' && c <= 'Z')
        c = (char)(c - 'A' + 'a');

    if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u')
        return "an";
    return "a";
}

static const char* skeleton_note_format_artefact_kind(
    const object_type* o_ptr, char* buf, size_t buf_sz)
{
    const char* kind = skeleton_note_artefact_kind_name(o_ptr);

    if (!buf || buf_sz == 0)
        return "an artefact";

    if (!kind || !kind[0])
        kind = "artefact";

    strnfmt(buf, buf_sz, "%s %s",
        skeleton_note_indefinite_article(kind), kind);
    return buf;
}

static u32b skeleton_note_nearest_guardian_source_ident(int y, int x)
{
    int best_dist = 0;
    int seen = 0;
    u32b best_ident = 0;

    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr;
        u32b source_ident = 0;
        int dist;

        if (!m_ptr->r_idx)
            continue;

        dist = skeleton_note_map_distance(y, x, m_ptr->fy, m_ptr->fx);
        if (dist > SKELETON_NOTE_HOARD_GUARD_RADIUS)
            continue;

        r_ptr = &r_info[m_ptr->r_idx];
        if (r_ptr->flags3 & RF3_DRAGON)
            source_ident |= IDENT_DRAGON_DROP;
        if (r_ptr->flags1 & RF1_UNIQUE)
            source_ident |= IDENT_UNIQUE_DROP;
        if (!source_ident)
            continue;

        if (!best_ident || dist < best_dist)
        {
            best_ident = source_ident;
            best_dist = dist;
            seen = 1;
            continue;
        }

        if (dist == best_dist)
        {
            ++seen;
            if (one_in_(seen))
                best_ident = source_ident;
        }
    }

    return best_ident;
}

static const char* skeleton_note_hoard_site_for_source_ident(u32b source_ident)
{
    if ((source_ident & IDENT_DRAGON_DROP)
        && (source_ident & IDENT_UNIQUE_DROP))
    {
        return "a dragon-lord's hoard";
    }
    if (source_ident & IDENT_DRAGON_DROP)
        return "a dragon's hoard";
    if (source_ident & IDENT_UNIQUE_DROP)
        return "a unique foe's hoard";
    return NULL;
}

static const char* skeleton_note_hoard_site_for_object(
    const object_type* o_ptr, char* vault_site, size_t vault_site_sz)
{
    u32b source_ident = 0;
    const char* source_site = NULL;

    if (!o_ptr)
        return "a hidden cache";

    if (o_ptr->ident
        & (IDENT_CHASM_SANCTUM_ITEM | IDENT_CHASM_SANCTUM_DROP))
    {
        return "a chasm sanctum";
    }

    source_ident = o_ptr->ident & (IDENT_DRAGON_DROP | IDENT_UNIQUE_DROP);
    source_site = skeleton_note_hoard_site_for_source_ident(source_ident);
    if (source_site)
        return source_site;

    if (in_bounds_fully(o_ptr->iy, o_ptr->ix)
        && (cave_info[o_ptr->iy][o_ptr->ix] & CAVE_G_VAULT))
    {
        const vault_type* v_ptr = skeleton_note_current_greater_vault();
        cptr vault_name = NULL;

        if (v_ptr && v_ptr->name)
            vault_name = v_name + v_ptr->name;
        else if (g_vault_name[0])
            vault_name = g_vault_name;

        if (vault_name && vault_site && vault_site_sz > 0)
        {
            strnfmt(vault_site, vault_site_sz, "a cache within %s",
                vault_name);
            return vault_site;
        }
        return "a great vault";
    }

    if (o_ptr->ident & IDENT_HOARD_DROP)
    {
        source_ident =
            skeleton_note_nearest_guardian_source_ident(o_ptr->iy, o_ptr->ix);
        source_site = skeleton_note_hoard_site_for_source_ident(source_ident);
        if (source_site)
            return source_site;
        return "a treasure hoard";
    }
    return "a hidden cache";
}

static bool skeleton_note_find_nearest_artefact(
    int from_y, int from_x, int* out_y, int* out_x, int* out_dist,
    char* out_site, size_t out_site_sz, char* out_artefact_kind,
    size_t out_artefact_kind_sz)
{
    int best_y = -1;
    int best_x = -1;
    int best_dist = 0;
    char best_site[96];
    char best_artefact_kind[64];
    int seen = 0;

    best_site[0] = '\0';
    best_artefact_kind[0] = '\0';

    for (int i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];
        if (!skeleton_note_artefact_hint_target_ok(o_ptr))
            continue;

        int dist = skeleton_note_map_distance(from_y, from_x, o_ptr->iy, o_ptr->ix);
        if (best_y < 0 || dist < best_dist)
        {
            char vault_site[96];
            const char* site;

            best_y = o_ptr->iy;
            best_x = o_ptr->ix;
            best_dist = dist;
            vault_site[0] = '\0';
            site = skeleton_note_hoard_site_for_object(
                o_ptr, vault_site, sizeof(vault_site));
            strnfmt(best_site, sizeof(best_site), "%s",
                site ? site : "a hidden cache");
            (void)skeleton_note_format_artefact_kind(
                o_ptr, best_artefact_kind, sizeof(best_artefact_kind));
            seen = 1;
            continue;
        }

        if (dist == best_dist)
        {
            ++seen;
            if (one_in_(seen))
            {
                char vault_site[96];
                const char* site;

                best_y = o_ptr->iy;
                best_x = o_ptr->ix;
                vault_site[0] = '\0';
                site = skeleton_note_hoard_site_for_object(
                    o_ptr, vault_site, sizeof(vault_site));
                strnfmt(best_site, sizeof(best_site), "%s",
                    site ? site : "a hidden cache");
                (void)skeleton_note_format_artefact_kind(
                    o_ptr, best_artefact_kind, sizeof(best_artefact_kind));
            }
        }
    }

    if (best_y < 0)
        return false;

    if (out_y) *out_y = best_y;
    if (out_x) *out_x = best_x;
    if (out_dist) *out_dist = best_dist;
    if (out_site && out_site_sz > 0)
    {
        strnfmt(out_site, out_site_sz, "%s",
            best_site[0] ? best_site : "a hidden cache");
    }
    if (out_artefact_kind && out_artefact_kind_sz > 0)
    {
        strnfmt(out_artefact_kind, out_artefact_kind_sz, "%s",
            best_artefact_kind[0] ? best_artefact_kind : "an artefact");
    }
    return true;
}

static bool skeleton_note_find_nearest_unique(
    int from_y, int from_x, int* out_r_idx, int* out_y, int* out_x, int* out_dist)
{
    int best_r_idx = 0;
    int best_y = -1;
    int best_x = -1;
    int best_dist = 0;
    int seen = 0;

    for (int i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];
        if (!m_ptr->r_idx)
            continue;
        monster_race* r_ptr = &r_info[m_ptr->r_idx];
        if (!(r_ptr->flags1 & RF1_UNIQUE))
            continue;

        int dist = skeleton_note_map_distance(from_y, from_x, m_ptr->fy, m_ptr->fx);
        if (best_y < 0 || dist < best_dist)
        {
            best_r_idx = m_ptr->r_idx;
            best_y = m_ptr->fy;
            best_x = m_ptr->fx;
            best_dist = dist;
            seen = 1;
            continue;
        }

        if (dist == best_dist)
        {
            ++seen;
            if (one_in_(seen))
            {
                best_r_idx = m_ptr->r_idx;
                best_y = m_ptr->fy;
                best_x = m_ptr->fx;
            }
        }
    }

    if (best_y < 0)
        return false;

    if (out_r_idx) *out_r_idx = best_r_idx;
    if (out_y) *out_y = best_y;
    if (out_x) *out_x = best_x;
    if (out_dist) *out_dist = best_dist;
    return true;
}

static bool skeleton_note_find_nearest_partition_site(level_partition_kind kind,
    big_cave_type_t cave_type, int from_y, int from_x, int* out_y, int* out_x, int* out_dist)
{
    int source_pi = level_partition_index_for_point(from_y, from_x);
    level_partition_kind source_kind = level_partition_kind_for_point(from_y, from_x);
    big_cave_type_t source_cave_type =
        level_partition_big_cave_type_for_point(from_y, from_x);
    bool skip_source_partition = (source_pi >= 0) && (source_kind == kind)
        && ((kind != LEVEL_PART_BIG_CAVE) || (cave_type == BIG_CAVE_NONE)
            || (source_cave_type == cave_type));
    int best_y = -1;
    int best_x = -1;
    int best_dist = 0;
    int seen = 0;

    for (int y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            if (level_partition_kind_for_point(y, x) != kind)
                continue;
            if (skip_source_partition
                && level_partition_index_for_point(y, x) == source_pi)
            {
                continue;
            }
            if (kind == LEVEL_PART_BIG_CAVE && cave_type != BIG_CAVE_NONE)
            {
                if (level_partition_big_cave_type_for_point(y, x) != cave_type)
                    continue;
            }

            int dist = skeleton_note_map_distance(from_y, from_x, y, x);
            if (best_y < 0 || dist < best_dist)
            {
                best_y = y;
                best_x = x;
                best_dist = dist;
                seen = 1;
                continue;
            }

            if (dist == best_dist)
            {
                ++seen;
                if (one_in_(seen))
                {
                    best_y = y;
                    best_x = x;
                }
            }
        }
    }

    if (best_y < 0)
        return false;

    if (out_y) *out_y = best_y;
    if (out_x) *out_x = best_x;
    if (out_dist) *out_dist = best_dist;
    return true;
}

static const char* skeleton_note_stair_site(int feat)
{
    switch (feat)
    {
    case FEAT_MORE:
        return "down staircase";
    case FEAT_MORE_SHAFT:
        return "down shaft";
    case FEAT_LESS:
        return "up staircase";
    case FEAT_LESS_SHAFT:
        return "up shaft";
    default:
        return "stairs";
    }
}

static const char* skeleton_note_stair_title(int feat)
{
    switch (feat)
    {
    case FEAT_MORE:
        return "Hint: Down Stairs";
    case FEAT_MORE_SHAFT:
        return "Hint: Down Shaft";
    case FEAT_LESS:
        return "Hint: Up Stairs";
    case FEAT_LESS_SHAFT:
        return "Hint: Up Shaft";
    default:
        return "Hint: Stairs";
    }
}

static void skeleton_note_partition_meta_for_hint(
    skeleton_hint_kind hint, level_partition_kind* out_kind, big_cave_type_t* out_type)
{
    if (out_kind)
        *out_kind = LEVEL_PART_NONE;
    if (out_type)
        *out_type = BIG_CAVE_NONE;

    switch (hint)
    {
    case SKEL_HINT_PART_LABYRINTH:
        if (out_kind) *out_kind = LEVEL_PART_LABYRINTH;
        break;
    case SKEL_HINT_PART_CHASM:
        if (out_kind) *out_kind = LEVEL_PART_CHASM;
        break;
    case SKEL_HINT_PART_CAVE:
        if (out_kind) *out_kind = LEVEL_PART_BIG_CAVE;
        break;
    case SKEL_HINT_PART_CAVE_ICE:
        if (out_kind) *out_kind = LEVEL_PART_BIG_CAVE;
        if (out_type) *out_type = BIG_CAVE_ICE;
        break;
    case SKEL_HINT_PART_CAVE_FIRE:
        if (out_kind) *out_kind = LEVEL_PART_BIG_CAVE;
        if (out_type) *out_type = BIG_CAVE_FIRE;
        break;
    case SKEL_HINT_PART_CAVE_POIS:
        if (out_kind) *out_kind = LEVEL_PART_BIG_CAVE;
        if (out_type) *out_type = BIG_CAVE_POIS;
        break;
    case SKEL_HINT_PART_ROOMY:
        if (out_kind) *out_kind = LEVEL_PART_ROOMY;
        break;
    case SKEL_HINT_PART_RUINED:
        if (out_kind) *out_kind = LEVEL_PART_RUINED;
        break;
    case SKEL_HINT_PART_CAVEY:
        if (out_kind) *out_kind = LEVEL_PART_CAVEY;
        break;
    default:
        break;
    }
}

static const char* skeleton_note_forge_site(int feat, char* buf, size_t buf_sz)
{
    (void)buf;
    (void)buf_sz;

    if (feat >= FEAT_FORGE_UNIQUE_HEAD && feat <= FEAT_FORGE_UNIQUE_TAIL)
        return "unique forge";

    if (feat >= FEAT_FORGE_GOOD_HEAD && feat <= FEAT_FORGE_GOOD_TAIL)
        return "enchanted forge";

    return "forge";
}

static void hint_message_meta_init(hint_message_meta* meta, int source_y, int source_x)
{
    if (!meta)
        return;

    memset(meta, 0, sizeof(*meta));
    meta->source_y = (s16b)source_y;
    meta->source_x = (s16b)source_x;
}

static bool hint_message_cue_is_specific(const char* dist, const char* dir)
{
    if ((dist && streq(dist, "somewhere")) || (dir && streq(dir, "on this level")))
        return false;

    return ((dist && dist[0]) || (dir && dir[0]));
}

static void hint_message_meta_add_cue(hint_message_meta* meta, const char* dist,
    const char* dir)
{
    if (!meta || !hint_message_cue_is_specific(dist, dir))
        return;

    for (int i = 0; i < meta->cue_count; ++i)
    {
        if (streq(meta->cue_dists[i], dist ? dist : "")
            && streq(meta->cue_dirs[i], dir ? dir : ""))
        {
            return;
        }
    }

    if (meta->cue_count >= HINT_MESSAGE_CUE_MAX)
        return;

    int slot = meta->cue_count++;
    strnfmt(meta->cue_dists[slot], HINT_MESSAGE_CUE_TEXT_MAX, "%s", dist ? dist : "");
    strnfmt(meta->cue_dirs[slot], HINT_MESSAGE_CUE_TEXT_MAX, "%s", dir ? dir : "");
}

static const char* skeleton_hint_title(skeleton_hint_kind hint, int stairs_feat)
{
    switch (hint)
    {
    case SKEL_HINT_GREAT_VAULT:
        return "Hint: Great Vault";
    case SKEL_HINT_VAULT_ARTIFACT:
        return "Hint: Hidden Artefact";
    case SKEL_HINT_STAIRS:
        return skeleton_note_stair_title(stairs_feat);
    case SKEL_HINT_PARTITION_PRESENCE:
        return "Hint: Layout";
    case SKEL_HINT_FORGE:
        return "Hint: Forge";
    case SKEL_HINT_UNIQUE_MONSTER:
        return "Hint: Unique Monster";
    case SKEL_HINT_TIP:
        return "Hint: Survival Tip";
    case SKEL_HINT_LEVEL_SIZE:
        return "Hint: Level Size";
    case SKEL_HINT_QUEST:
        return "Hint: Quest";
    case SKEL_HINT_PART_LABYRINTH:
        return "Hint: Labyrinth";
    case SKEL_HINT_PART_CHASM:
        return "Hint: Chasm";
    case SKEL_HINT_PART_CAVE:
    case SKEL_HINT_PART_CAVEY:
        return "Hint: Caves";
    case SKEL_HINT_PART_CAVE_ICE:
        return "Hint: Ice Cave";
    case SKEL_HINT_PART_CAVE_FIRE:
        return "Hint: Fire Cave";
    case SKEL_HINT_PART_CAVE_POIS:
        return "Hint: Poison Cave";
    case SKEL_HINT_PART_ROOMY:
        return "Hint: Rooms";
    case SKEL_HINT_PART_RUINED:
        return "Hint: Ruins";
    default:
        return "Hint: Note";
    }
}

static void skeleton_note_maybe_show(byte sval, int skel_y, int skel_x)
{
    if (skeleton_note_entry_count() == 0)
        return;

    skeleton_note_ensure_level_state();

    if (g_skeleton_note_state.note_cap <= 0)
        return;

    skeleton_note_profile profile = skeleton_note_profile_for_sval(sval);

    level_layout_info layout;
    level_layout_info_current(&layout);

    bool vault_present = level_has_greater_vault();
    bool artefact_present = level_has_artefact_hint_target();
    bool tutorial_note_forced = skeleton_hint_available(
        SKEL_HINT_TIP, &layout, vault_present, artefact_present, sval);

    if (profile.note_chance <= 0 && !tutorial_note_forced)
        return;

    if (!tutorial_note_forced && !percent_chance(profile.note_chance))
        return;

    skeleton_hint_kind hint1 = SKEL_HINT_NONE;
    skeleton_hint_kind hint2 = SKEL_HINT_NONE;
    if (tutorial_note_forced)
    {
        hint1 = SKEL_HINT_TIP;

        if (profile.note_chance > 0 && percent_chance(profile.note_chance))
        {
            hint2 = skeleton_note_choose_hint(
                &profile, &layout, vault_present, artefact_present, sval,
                g_skeleton_note_state.hint_use_counts,
                skeleton_hint_bit(SKEL_HINT_TIP));
        }

    }
    else
    {
        bool can_tip = skeleton_hint_available(
            SKEL_HINT_TIP, &layout, vault_present, artefact_present, sval);
        int tip_chance = skeleton_note_tip_override_chance(sval, p_ptr->depth);
        if (can_tip && tip_chance > 0 && percent_chance(tip_chance))
        {
            hint1 = SKEL_HINT_TIP;
        }
        else
        {
            hint1 = skeleton_note_choose_hint(
                &profile, &layout, vault_present, artefact_present, sval,
                g_skeleton_note_state.hint_use_counts, 0);
        }
    }
    if (hint1 == SKEL_HINT_NONE)
        return;

    if (hint2 == SKEL_HINT_NONE && hint1 != SKEL_HINT_TIP)
    {
        int size_bucket = skeleton_note_size_bucket(&layout);
        int second_chance = 0;

        switch (sval)
        {
        case SV_SKELETON_ELF:
            second_chance = 50 + 10 * size_bucket;
            break;
        case SV_SKELETON_ORC:
            second_chance = 30 + 5 * size_bucket;
            break;
        default:
            second_chance = 45 + 10 * size_bucket;
            break;
        }

        if (second_chance > 0 && percent_chance(second_chance))
        {
            byte counts_after_hint1[SKEL_HINT_MAX];
            u32b exclude_mask2 = skeleton_hint_bit(hint1);

            memcpy(counts_after_hint1, g_skeleton_note_state.hint_use_counts,
                sizeof(counts_after_hint1));
            skeleton_hint_count_mark_used(hint1, counts_after_hint1);

            hint2 = skeleton_note_choose_hint(
                &profile, &layout, vault_present, artefact_present, sval,
                counts_after_hint1, exclude_mask2);
        }
    }

    /*
     * When a tutorial tip drove the note, fill the second slot with another
     * tip if no regular hint was rolled.
     */
    if (hint2 == SKEL_HINT_NONE && hint1 == SKEL_HINT_TIP)
        hint2 = SKEL_HINT_TIP;

    skeleton_hint_record_used(hint1);
    skeleton_hint_record_used(hint2);

    const char* unique_type = NULL;
    int unique_y = 0;
    int unique_x = 0;
    int unique_dist = 0;
    bool unique_found = false;
    if (hint1 == SKEL_HINT_UNIQUE_MONSTER || hint2 == SKEL_HINT_UNIQUE_MONSTER)
    {
        int r_idx = 0;
        if (skeleton_note_find_nearest_unique(skel_y, skel_x, &r_idx, &unique_y, &unique_x, &unique_dist))
        {
            unique_type = skeleton_get_unique_type_name(&r_info[r_idx]);
            unique_found = true;
        }
    }

    skeleton_partition_focus focus_part;
    focus_part.kind = LEVEL_PART_NONE;
    focus_part.big_cave_type = BIG_CAVE_NONE;
    int stairs_feat = -1;
    if (hint1 == SKEL_HINT_PARTITION_PRESENCE
        || hint2 == SKEL_HINT_PARTITION_PRESENCE)
    {
        focus_part = skeleton_pick_partition_presence(&layout);
    }

    bool note_has_tip = (hint1 == SKEL_HINT_TIP || hint2 == SKEL_HINT_TIP);
    s16b opening_id = -1;
    s16b signoff_id = -1;
    const char* opening = NULL;
    const char* signoff = NULL;

    if (!note_has_tip)
    {
        opening_id = skeleton_note_pick_entry(
            sval, SKELETON_NOTE_ROLE_OPENING, SKEL_HINT_NONE);
        signoff_id = skeleton_note_pick_entry(
            sval, SKELETON_NOTE_ROLE_SIGNOFF, SKEL_HINT_NONE);
        if (opening_id < 0)
        {
            opening_id = skeleton_note_pick_entry_internal(
                sval, SKELETON_NOTE_ROLE_OPENING, SKEL_HINT_NONE, true, -1);
        }
        if (signoff_id < 0)
        {
            signoff_id = skeleton_note_pick_entry_internal(
                sval, SKELETON_NOTE_ROLE_SIGNOFF, SKEL_HINT_NONE, true, -1);
        }

        opening = opening_id >= 0
            ? (skeleton_note_text + skeleton_note_info[opening_id].text)
            : skeleton_note_fallback_opening(sval);
        signoff = signoff_id >= 0
            ? (skeleton_note_text + skeleton_note_info[signoff_id].text)
            : skeleton_note_fallback_signoff(sval);
    }

    skeleton_note_line body_lines[2];
    s16b body_ids[2] = {-1, -1};
    int body_count = 0;
    hint_message_meta hint_meta;
    hint_message_meta_init(&hint_meta, skel_y, skel_x);

    skeleton_hint_kind hints[2] = {hint1, hint2};
    int hint_count = (hint2 != SKEL_HINT_NONE) ? 2 : 1;
    cptr unique_vault_hint = skeleton_note_current_greater_vault_hint();

    char forge_site_buf[64];
    char artefact_site_buf[2][96];
    char artefact_kind_buf[2][64];
    char distance_buf[2][64];
    char tutorial_tip_buf[2][256];
    forge_site_buf[0] = '\0';
    artefact_site_buf[0][0] = '\0';
    artefact_site_buf[1][0] = '\0';
    artefact_kind_buf[0][0] = '\0';
    artefact_kind_buf[1][0] = '\0';
    distance_buf[0][0] = '\0';
    distance_buf[1][0] = '\0';
    tutorial_tip_buf[0][0] = '\0';
    tutorial_tip_buf[1][0] = '\0';

    for (int i = 0; i < hint_count; ++i)
    {
        skeleton_hint_kind hint = hints[i];
        s16b note_id = -1;
        const char* extra_tpl = NULL;
        const char* tpl = NULL;

        if (hint == SKEL_HINT_GREAT_VAULT && unique_vault_hint)
        {
            tpl = unique_vault_hint;
        }
        else
        {
            note_id = skeleton_note_pick_entry(
                sval, SKELETON_NOTE_ROLE_HINT, hint);

            if (hint == SKEL_HINT_TIP && body_count > 0
                && note_id == body_ids[body_count - 1])
            {
                note_id = skeleton_note_pick_entry_internal(
                    sval, SKELETON_NOTE_ROLE_HINT, hint, false,
                    body_ids[body_count - 1]);
            }

            if (note_id < 0)
            {
                note_id = skeleton_note_pick_entry_internal(
                    sval, SKELETON_NOTE_ROLE_HINT, hint, true,
                    (hint == SKEL_HINT_TIP && body_count > 0)
                        ? body_ids[body_count - 1]
                        : -1);
            }

            tpl = (note_id >= 0)
                ? (skeleton_note_text + skeleton_note_info[note_id].text)
                : NULL;
            if (note_id >= 0 && skeleton_note_info[note_id].extra_text)
            {
                extra_tpl =
                    skeleton_note_text + skeleton_note_info[note_id].extra_text;
            }
        }

        if (!tpl)
        {
            switch (hint)
            {
            case SKEL_HINT_GREAT_VAULT:
                tpl = "A gate of black stone stands {DIST} {DIR}; the warding is unbroken.";
                break;
            case SKEL_HINT_VAULT_ARTIFACT:
                tpl = "{ART_CAP} lies in {SITE} {DIST} {DIR}.";
                break;
            case SKEL_HINT_STAIRS:
                tpl = "The {SITE} lies {DIST} {DIR}.";
                break;
            case SKEL_HINT_PARTITION_PRESENCE:
                tpl = "Beware {PART} {DIST} {DIR}; {PART_HAZARD}.";
                break;
            case SKEL_HINT_PART_LABYRINTH:
                tpl = "A maze of hewn stone lies {DIST} {DIR}; its turns will unmake your bearings.";
                break;
            case SKEL_HINT_PART_CHASM:
                tpl = "There is a chasm {DIST} {DIR}. The dark below drinks both light and courage.";
                break;
            case SKEL_HINT_PART_CAVE:
                tpl = "A great cavern opens {DIST} {DIR}. Sound carries far, and there is little cover.";
                break;
            case SKEL_HINT_PART_CAVE_ICE:
                tpl = "A great ice cavern lies {DIST} {DIR}. The cold bites, and the floor is slick.";
                break;
            case SKEL_HINT_PART_CAVE_FIRE:
                tpl = "A cavern of fire lies {DIST} {DIR}. The air burns and smoke chokes.";
                break;
            case SKEL_HINT_PART_CAVE_POIS:
                tpl = "A cavern of poisonous vapours lies {DIST} {DIR}. The reek clings low and stings the lungs.";
                break;
            case SKEL_HINT_FORGE:
                tpl = "Smoke and hammer-sound: the {SITE} lies {DIST} {DIR}.";
                break;
            case SKEL_HINT_UNIQUE_MONSTER:
                tpl = "A {UNIQUE_TYPE} walks these halls {DIST} {DIR}. Hide or flee.";
                break;
            case SKEL_HINT_TIP:
                tpl = "Bones clutch a faded scrap of text.";
                break;
            case SKEL_HINT_LEVEL_SIZE:
                tpl = "This place is {SIZEWORD}; do not expect a short road to anywhere.";
                break;
            case SKEL_HINT_QUEST:
                tpl = "A power stirs in these deeps; I saw {SITE} {DIST} {DIR}.";
                break;
            default:
                tpl = "Bones clutch a faded scrap of text.";
                break;
            }
        }

        if (hint == SKEL_HINT_TIP && extra_tpl && extra_tpl[0])
        {
            strnfmt(tutorial_tip_buf[body_count], sizeof(tutorial_tip_buf[body_count]),
                "%s %s",
                tpl, extra_tpl);
            tpl = tutorial_tip_buf[body_count];
        }

        body_lines[body_count].tpl = tpl;
        if (hint == SKEL_HINT_PARTITION_PRESENCE)
        {
            body_lines[body_count].presence_kind = focus_part.kind;
            body_lines[body_count].big_cave_type = focus_part.big_cave_type;
        }
        else
        {
            skeleton_note_partition_meta_for_hint(
                hint, &body_lines[body_count].presence_kind,
                &body_lines[body_count].big_cave_type);
        }
        body_lines[body_count].unique_type
            = (hint == SKEL_HINT_UNIQUE_MONSTER) ? unique_type : NULL;
        body_lines[body_count].dir = NULL;
        body_lines[body_count].dist = NULL;
        body_lines[body_count].site = NULL;
        body_lines[body_count].artefact_kind = NULL;
        body_lines[body_count].size_word = NULL;

        if (hint == SKEL_HINT_STAIRS)
        {
            int ty = 0, tx = 0, feat = 0, dist = 0;
            if (skeleton_note_find_nearest_stairs(
                    sval, skel_y, skel_x, &ty, &tx, &feat, &dist))
            {
                stairs_feat = feat;
                body_lines[body_count].dir
                    = skeleton_note_direction_phrase(skel_y, skel_x, ty, tx);
                body_lines[body_count].dist = skeleton_note_distance_phrase(
                    dist, &layout, distance_buf[body_count],
                    sizeof(distance_buf[body_count]));
                body_lines[body_count].site = skeleton_note_stair_site(feat);
            }
        }
        else if (hint == SKEL_HINT_FORGE)
        {
            int ty = 0, tx = 0, feat = 0, dist = 0;
            if (skeleton_note_find_nearest_forge(
                    skel_y, skel_x, &ty, &tx, &feat, &dist))
            {
                body_lines[body_count].dir
                    = skeleton_note_direction_phrase(skel_y, skel_x, ty, tx);
                body_lines[body_count].dist = skeleton_note_distance_phrase(
                    dist, &layout, distance_buf[body_count],
                    sizeof(distance_buf[body_count]));
                body_lines[body_count].site
                    = skeleton_note_forge_site(feat, forge_site_buf, sizeof(forge_site_buf));
            }
        }
        else if (hint == SKEL_HINT_LEVEL_SIZE)
        {
            body_lines[body_count].size_word = skeleton_note_pick_size_word(&layout);
        }
        else if (hint == SKEL_HINT_QUEST)
        {
            int ty = 0, tx = 0, dist = 0;
            const char* site = NULL;
            if (skeleton_note_find_nearest_quest_site(skel_y, skel_x, &ty, &tx, &dist, &site))
            {
                body_lines[body_count].dir
                    = skeleton_note_direction_phrase(skel_y, skel_x, ty, tx);
                body_lines[body_count].dist = skeleton_note_distance_phrase(
                    dist, &layout, distance_buf[body_count],
                    sizeof(distance_buf[body_count]));
                body_lines[body_count].site = site;
            }
            else
            {
                body_lines[body_count].dist = "somewhere";
                body_lines[body_count].dir = "on this level";
                body_lines[body_count].site = "a warded place";
            }
        }
        else if (hint == SKEL_HINT_GREAT_VAULT)
        {
            int ty = 0, tx = 0, dist = 0;
            if (skeleton_note_find_nearest_great_vault(skel_y, skel_x, &ty, &tx, &dist))
            {
                body_lines[body_count].dir
                    = skeleton_note_direction_phrase(skel_y, skel_x, ty, tx);
                body_lines[body_count].dist = skeleton_note_distance_phrase(
                    dist, &layout, distance_buf[body_count],
                    sizeof(distance_buf[body_count]));
            }
            else
            {
                body_lines[body_count].dist = "somewhere";
                body_lines[body_count].dir = "on this level";
            }
        }
        else if (hint == SKEL_HINT_VAULT_ARTIFACT)
        {
            int ty = 0, tx = 0, dist = 0;
            if (skeleton_note_find_nearest_artefact(
                    skel_y, skel_x, &ty, &tx, &dist,
                    artefact_site_buf[body_count],
                    sizeof(artefact_site_buf[body_count]),
                    artefact_kind_buf[body_count],
                    sizeof(artefact_kind_buf[body_count])))
            {
                body_lines[body_count].dir
                    = skeleton_note_direction_phrase(skel_y, skel_x, ty, tx);
                body_lines[body_count].dist = skeleton_note_distance_phrase(
                    dist, &layout, distance_buf[body_count],
                    sizeof(distance_buf[body_count]));
                body_lines[body_count].site =
                    artefact_site_buf[body_count];
                body_lines[body_count].artefact_kind =
                    artefact_kind_buf[body_count];
            }
            else
            {
                body_lines[body_count].dist = "somewhere";
                body_lines[body_count].dir = "on this level";
                body_lines[body_count].site = "a hidden cache";
                body_lines[body_count].artefact_kind = "an artefact";
            }
        }
        else if (hint == SKEL_HINT_UNIQUE_MONSTER)
        {
            if (unique_found)
            {
                body_lines[body_count].dir = skeleton_note_direction_phrase(
                    skel_y, skel_x, unique_y, unique_x);
                body_lines[body_count].dist = skeleton_note_distance_phrase(
                    unique_dist, &layout, distance_buf[body_count],
                    sizeof(distance_buf[body_count]));
            }
            else
            {
                body_lines[body_count].dist = "somewhere";
                body_lines[body_count].dir = "on this level";
            }
        }
        else if (body_lines[body_count].presence_kind != LEVEL_PART_NONE)
        {
            int ty = 0, tx = 0, dist = 0;
            if (skeleton_note_find_nearest_partition_site(
                    body_lines[body_count].presence_kind,
                    body_lines[body_count].big_cave_type,
                    skel_y, skel_x, &ty, &tx, &dist))
            {
                body_lines[body_count].dir
                    = skeleton_note_direction_phrase(skel_y, skel_x, ty, tx);
                body_lines[body_count].dist = skeleton_note_distance_phrase(
                    dist, &layout, distance_buf[body_count],
                    sizeof(distance_buf[body_count]));
            }
            else
            {
                body_lines[body_count].dist = "somewhere";
                body_lines[body_count].dir = "on this level";
            }
        }

        hint_message_meta_add_cue(&hint_meta, body_lines[body_count].dist,
            body_lines[body_count].dir);
        body_ids[body_count] = note_id;
        body_count++;
    }

    char note_lines[HINT_MESSAGE_LINES_MAX][100] = {{0}};
    skeleton_note_build_lines(
        opening, body_lines, body_count, signoff, &layout, note_lines,
        skeleton_note_body_separator(sval));

    /* Prepend title */
    char title_buf[100];
    if (hint2 != SKEL_HINT_NONE)
    {
        const char* title1 = skeleton_hint_title(hint1,
            (hint1 == SKEL_HINT_STAIRS) ? stairs_feat : -1);
        const char* title2 = skeleton_hint_title(hint2,
            (hint2 == SKEL_HINT_STAIRS) ? stairs_feat : -1);
        if (streq(title1, title2))
        {
            strnfmt(title_buf, sizeof(title_buf), "%s", title1);
        }
        else
        {
            strnfmt(title_buf, sizeof(title_buf), "Hint: %s & %s",
                title1 + 6,
                title2 + 6);
        }
    }
    else
    {
        strnfmt(title_buf, sizeof(title_buf), "%s",
            skeleton_hint_title(hint1,
                (hint1 == SKEL_HINT_STAIRS) ? stairs_feat : -1));
    }

    for (int i = HINT_MESSAGE_LINES_MAX - 2; i >= 0; --i)
        strnfmt(note_lines[i+1], 100, "%s", note_lines[i]);
    strnfmt(note_lines[0], 100, "%s", title_buf);

    {
        int message_index = hint_messages_add_note_lines(note_lines, &hint_meta);
        if (message_index >= 0)
            show_hint_message_screen(message_index);
        else
            pause_with_text(note_lines, 4, 8, NULL, 0);
    }
    if ((hint1 != SKEL_HINT_NONE && hint1 != SKEL_HINT_TIP)
        || (hint2 != SKEL_HINT_NONE && hint2 != SKEL_HINT_TIP))
    {
        g_skeleton_note_state.notes_shown++;
    }
    skeleton_note_record_seen(opening_id);
    for (int i = 0; i < body_count; ++i)
        skeleton_note_record_seen(body_ids[i]);
    skeleton_note_record_seen(signoff_id);
}

/*
 * Attempt to search the given skeleton at the given location
 *
 * Assumes there is no monster blocking the destination
 */
void do_cmd_search_skeleton(int y, int x, s16b o_idx)
{
    bool search_failed = true;
    bool auto_carry_food = false;
    bool no_item_generated = false;
    object_type* o_ptr = &o_list[o_idx];

    // Searched already
    if (o_ptr->pval == 0)
    {
        return;
    }

    object_generation_mode = OB_GEN_MODE_SKELETON;

    skeleton_note_maybe_show(o_ptr->sval, y, x);

    object_type* i_ptr;
    object_type object_type_body;
    i_ptr = &object_type_body;

    int roll = rand_int(100);

    if (roll < 20)
    {
        prep_skeleton_food(i_ptr, o_ptr->sval);
        auto_carry_food = true;
        search_failed = false;
    }
    else if (roll < 40)
    {
        search_failed = !prep_skeleton_light(i_ptr);
    }
    else if (roll < 50)
    {
        search_failed = generate_skeleton_damaged_item(
            i_ptr, o_ptr->sval, &no_item_generated);
    }
    else
    {
        search_failed = true;
    }

    o_ptr->pval = 0;

    object_generation_mode = OB_GEN_MODE_NORMAL;

    if (search_failed)
    {
        if (no_item_generated)
            msg_print("You sift the bones, but they yield only dust.");
        else
            msg_print("You failed to find anything among the bones.");
    }
    else
    {
        if (i_ptr->k_idx)
        {
            int slot = -1;
            char o_name[80];

            if (i_ptr->tval != TV_ARROW)
            {
                i_ptr->number = 1;
            }
            else
            {
                i_ptr->number = dieroll(4) + 2;
                msg_format("You gather up %d arrows.", i_ptr->number);
            }

            object_desc(o_name, sizeof(o_name), i_ptr, true, 0);

            if (auto_carry_food)
            {
                slot = inven_carry(i_ptr, true);

                if (slot == SUPPLIES_INDEX)
                {
                    char label = supplies_label_char();
                    if (!label)
                        label = 'a';
                    msg_format("You recover %s and add it to your supplies (%c).", o_name, label);
                }
                else if (slot >= 0)
                {
                    msg_format("You recover %s (%c).", o_name, index_to_label(slot));
                }
                else
                {
                    msg_format("You recover %s from the bones.", o_name);
                    drop_near(i_ptr, -1, y, x);
                }
            }
            else
            {
                msg_format("You find %s among the bones.", o_name);
                drop_near(i_ptr, -1, y, x);
            }

            /* Break the truce if creatures see */
            break_truce(false);
        }
    }
}

typedef struct chest_minigame_retry_state
{
    bool active;
    bool pause_before_prompt;
    int y;
    int x;
    char previous[240];
} chest_minigame_retry_state;

static chest_minigame_retry_state chest_retry;

#define CHEST_LOOK_DIE_SIDES 5
#define CHEST_FULL_DETECTION_DISARM_BONUS 5
#define CHEST_MINIGAME_RETRY_DELAY_MS 1000
#define CHEST_LOCK_BASE_DIFFICULTY 8
#define CHEST_LOOK_BASE_DIFFICULTY 8
#define CHEST_DISARM_BASE_DIFFICULTY 6
#define CHEST_CONCEALED_TRAP_BASE_DIFFICULTY 18

static bool chest_was_inspected(const object_type* o_ptr)
{
    return o_ptr && ((o_ptr->ident & IDENT_CHEST_LOOKED) != 0);
}

bool chest_trap_fully_known(const object_type* o_ptr)
{
    return o_ptr && (object_known_p(o_ptr)
        || ((o_ptr->ident & IDENT_CHEST_TRAP_FULL) != 0));
}

bool chest_trap_presence_known(const object_type* o_ptr)
{
    if (!o_ptr || !object_chest_trap_flags(o_ptr))
        return false;

    return chest_trap_fully_known(o_ptr)
        || ((o_ptr->ident & IDENT_CHEST_TRAP_PRESENT) != 0);
}

bool chest_minigame_retry_target(int* y, int* x)
{
    if (!chest_retry.active)
        return false;

    if (y)
        *y = chest_retry.y;
    if (x)
        *x = chest_retry.x;
    return true;
}

void chest_minigame_clear_retry(void)
{
    memset(&chest_retry, 0, sizeof(chest_retry));
}

static void chest_minigame_schedule_retry(int y, int x, cptr previous,
    bool pause_before_prompt)
{
    chest_retry.active = true;
    chest_retry.pause_before_prompt = pause_before_prompt;
    chest_retry.y = y;
    chest_retry.x = x;
    SDL_strlcpy(chest_retry.previous, previous ? previous : "",
        sizeof(chest_retry.previous));
    p_ptr->command_new = 'o';
}

static void chest_desc_append(char* desc, size_t desc_size, cptr text)
{
    if (!desc || !desc_size || !text || !text[0])
        return;

    if (desc[0])
        SDL_strlcat(desc, " ", desc_size);
    SDL_strlcat(desc, text, desc_size);
}

static int chest_condition_penalty(void)
{
    int penalty = 0;

    if (p_ptr->blind || no_light() || p_ptr->image)
        penalty += 5;
    if (p_ptr->confused)
        penalty += 5;
    return penalty;
}

static int chest_lock_difficulty(const object_type* o_ptr)
{
    int level = o_ptr ? ABS(o_ptr->pval) : 0;
    int power = 1 + (level / 4);

    return power + CHEST_LOCK_BASE_DIFFICULTY
        + chest_condition_penalty();
}

static int chest_look_difficulty(const object_type* o_ptr)
{
    int level = o_ptr ? ABS(o_ptr->pval) : 0;

    /* A deliberate close inspection is substantially easier than noticing a
     * chest trap in passing.  The d5 opposed throw keeps its variance low. */
    return (level / 2) + CHEST_LOOK_BASE_DIFFICULTY
        + chest_condition_penalty();
}

static int chest_disarm_difficulty(const object_type* o_ptr)
{
    int level = o_ptr ? ABS(o_ptr->pval) : 0;
    int power = 1 + (level / 4);

    return power + (level / 4) + CHEST_DISARM_BASE_DIFFICULTY
        + chest_condition_penalty();
}

static void chest_trap_effect_desc(char* buf, size_t buf_size,
    const object_type* o_ptr)
{
    byte trap = object_chest_trap_flags(o_ptr);
    int level = ABS(o_ptr->pval);
    int bonus = level / 6;
    int needle_skill = 2 + level / 4;
    char part[192];

    buf[0] = '\0';

#define CHEST_EFFECT_ADD(text_) chest_desc_append(buf, buf_size, (text_))
    if (trap & CHEST_GAS_CONF)
        CHEST_EFFECT_ADD("Confusion gas: 4d4 turns of confusion.");
    if (trap & CHEST_GAS_STUN)
    {
        strnfmt(part, sizeof(part),
            "Acrid smoke: %dd4 damage and 30d4 turns of stun.", 3 + bonus);
        CHEST_EFFECT_ADD(part);
    }
    if (trap & CHEST_GAS_POISON)
    {
        strnfmt(part, sizeof(part),
            "Poison gas: %dd4 pure poison damage.", 10 + bonus * 2);
        CHEST_EFFECT_ADD(part);
    }
    if (trap & CHEST_NEEDLE_HALLU)
    {
        strnfmt(part, sizeof(part),
            "Hallucinatory needle: attack %d vs Dexterity; 80d4 turns of "
            "hallucination.",
            needle_skill);
        CHEST_EFFECT_ADD(part);
    }
    if (trap & CHEST_NEEDLE_ENTRANCE)
    {
        strnfmt(part, sizeof(part),
            "Entrancing needle: attack %d vs Dexterity; 10d4 turns of "
            "entrancement.",
            needle_skill);
        CHEST_EFFECT_ADD(part);
    }
    if (trap & CHEST_NEEDLE_LOSE_STR)
    {
        strnfmt(part, sizeof(part),
            "Weakening needle: attack %d vs Dexterity; drains Strength.",
            needle_skill);
        CHEST_EFFECT_ADD(part);
    }
    if (trap & CHEST_FLAME)
    {
        strnfmt(part, sizeof(part),
            "Flame trap: %dd4 pure fire damage.", 10 + bonus * 2);
        CHEST_EFFECT_ADD(part);
    }
#undef CHEST_EFFECT_ADD
}

static bool chest_can_look(const object_type* o_ptr)
{
    if (!o_ptr || chest_trap_fully_known(o_ptr))
        return false;

    return !chest_was_inspected(o_ptr)
        || (p_ptr->skill_base[S_PER] > object_runtime_payload(o_ptr));
}

static void chest_mark_looked(object_type* o_ptr, bool full)
{
    byte trap;

    if (!o_ptr)
        return;

    trap = object_chest_trap_flags(o_ptr);
    o_ptr->ident |= IDENT_CHEST_LOOKED;
    if (trap)
        o_ptr->ident |= IDENT_CHEST_TRAP_PRESENT;
    else
        o_ptr->ident &= ~IDENT_CHEST_TRAP_PRESENT;
    object_set_runtime_payload(o_ptr, p_ptr->skill_base[S_PER]);

    if (full)
    {
        o_ptr->ident |= IDENT_CHEST_TRAP_FULL;
        object_known(o_ptr);
    }
}

static bool do_cmd_chest_minigame(int y, int x, s16b o_idx)
{
    enum
    {
        CHEST_ACTION_LOOK,
        CHEST_ACTION_DISARM,
        CHEST_ACTION_OPEN
    };
    ui_question_option options[3];
    int actions[3];
    int count = 0;
    int choice;
    int action;
    int result;
    int score;
    int difficulty;
    bool fully_known;
    bool trap_known;
    bool looked;
    bool pause_before_prompt = false;
    byte trap;
    char title[96];
    char desc[768];
    char line[240];
    char effects[480];
    char previous[240];
    char look_label[64];
    char disarm_label[64];
    char open_label[64];
    object_type* o_ptr;
    skill_roll_details roll;

    if (o_idx <= 0 || o_idx >= o_max)
    {
        chest_minigame_clear_retry();
        return false;
    }

    o_ptr = &o_list[o_idx];
    if (!o_ptr->k_idx || o_ptr->tval != TV_CHEST || o_ptr->pval == 0)
    {
        chest_minigame_clear_retry();
        return false;
    }

    previous[0] = '\0';
    if (chest_retry.active && chest_retry.y == y && chest_retry.x == x)
    {
        /* Command repetition may consume the retry before request_command()
         * sees its queued open command.  Avoid leaving that command stale if
         * this attempt finishes the interaction. */
        if (p_ptr->command_new == 'o')
            p_ptr->command_new = 0;
        pause_before_prompt = chest_retry.pause_before_prompt;
        SDL_strlcpy(previous, chest_retry.previous, sizeof(previous));
    }
    chest_minigame_clear_retry();

    trap = object_chest_trap_flags(o_ptr);
    fully_known = chest_trap_fully_known(o_ptr);
    trap_known = chest_trap_presence_known(o_ptr);
    looked = chest_was_inspected(o_ptr);

    object_desc(title, sizeof(title), o_ptr, true, 3);
    desc[0] = '\0';
    if (previous[0])
        chest_desc_append(desc, sizeof(desc), previous);

    if (o_ptr->pval < 0)
    {
        chest_desc_append(desc, sizeof(desc),
            "The trap and lock are disabled.");
    }
    else if (fully_known && trap)
    {
        chest_desc_append(desc, sizeof(desc),
            "You know how this trap works, making it easier to disarm.");
        chest_trap_effect_desc(effects, sizeof(effects), o_ptr);
        chest_desc_append(desc, sizeof(desc), effects);
    }
    else if (trap_known)
    {
        chest_desc_append(desc, sizeof(desc),
            "The chest is trapped, but you do not know how the trap works.");
    }
    else if (fully_known)
    {
        chest_desc_append(desc, sizeof(desc),
            "This chest is not trapped.");
    }
    else if (looked && !trap)
    {
        chest_desc_append(desc, sizeof(desc),
            "This chest is not trapped.");
    }
    else
    {
        chest_desc_append(desc, sizeof(desc),
            "This chest may be trapped. You can inspect it before opening.");
    }

    if (!fully_known && looked && !chest_can_look(o_ptr))
    {
        chest_desc_append(desc, sizeof(desc),
            "You need higher base Perception to inspect it again.");
    }

    if (trap_known && o_ptr->pval > 0)
    {
        chest_desc_append(desc, sizeof(desc),
            "The Open anyway percentage is only the chance to pick the lock. "
            "If the lock is picked, the trap triggers with 100% certainty, "
            "then the chest opens.");
    }

    strnfmt(look_label, sizeof(look_label), "Look for trap: %d%%",
        player_skill_check_success_percent(p_ptr->skill_use[S_PER],
            chest_look_difficulty(o_ptr), CHEST_LOOK_DIE_SIDES,
            CHEST_LOOK_DIE_SIDES));

    score = p_ptr->skill_use[S_PER];
    if (p_ptr->active_ability[S_PER][PER_REWIRE_TRAPS])
        score += 5;
    if (fully_known)
        score += CHEST_FULL_DETECTION_DISARM_BONUS;
    strnfmt(disarm_label, sizeof(disarm_label), "Disarm: %d%%",
        player_skill_check_success_percent(
            score, chest_disarm_difficulty(o_ptr), 10, 10));

    if (o_ptr->pval > 0)
    {
        strnfmt(open_label, sizeof(open_label),
            trap_known ? "Open anyway (lock): %d%%" : "Open: %d%%",
            player_skill_check_success_percent(p_ptr->skill_use[S_PER],
                chest_lock_difficulty(o_ptr), 10, 10));
    }
    else
    {
        SDL_strlcpy(open_label, "Open: 100%", sizeof(open_label));
    }

#define CHEST_MENU_ADD(action_, key_, label_, attr_)                          \
    do                                                                        \
    {                                                                         \
        options[count] = (ui_question_option){                                \
            (key_), (label_), (attr_), false                                  \
        };                                                                    \
        actions[count] = (action_);                                            \
        count++;                                                              \
    } while (0)

    if (o_ptr->pval > 0 && chest_can_look(o_ptr))
        CHEST_MENU_ADD(CHEST_ACTION_LOOK, 'l', look_label, TERM_L_BLUE);
    if (o_ptr->pval > 0 && trap_known)
        CHEST_MENU_ADD(CHEST_ACTION_DISARM, 'd', disarm_label, TERM_L_GREEN);
    CHEST_MENU_ADD(CHEST_ACTION_OPEN, 'o', open_label, TERM_ORANGE);
#undef CHEST_MENU_ADD

    if (pause_before_prompt && Term && !character_icky)
    {
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, CHEST_MINIGAME_RETRY_DELAY_MS);
    }

    choice = ui_question_ask(title, desc, options, count, y, x, 0);
    if (choice < 0 || choice >= count)
    {
        p_ptr->energy_use = 0;
        return false;
    }

    action = actions[choice];
    p_ptr->energy_use = 100;
    p_ptr->previous_action[0] = ACTION_MISC;

    if (action == CHEST_ACTION_LOOK)
    {
        score = p_ptr->skill_use[S_PER];
        difficulty = chest_look_difficulty(o_ptr);
        result = show_interaction_skill_roll_animation_sided(
            "Inspecting the chest", "Looking for a trap", y, x, score,
            difficulty, CHEST_LOOK_DIE_SIDES, CHEST_LOOK_DIE_SIDES, &roll);
        chest_mark_looked(o_ptr, result > 0);

        if (result > 0 && trap)
        {
            msg_print("You identify the chest's trap and understand its mechanism.");
            SDL_strlcpy(line,
                "Inspection succeeded. You identified the trap.",
                sizeof(line));
        }
        else if (result > 0)
        {
            msg_print("You confirm that the chest has no trap.");
            SDL_strlcpy(line,
                "Inspection succeeded. The chest is not trapped.",
                sizeof(line));
        }
        else if (trap)
        {
            msg_print("You find signs that the chest is trapped.");
            SDL_strlcpy(line,
                "Inspection failed. You only learned that a trap is present.",
                sizeof(line));
        }
        else
        {
            msg_print("You find no trap on the chest.");
            SDL_strlcpy(line,
                "Inspection failed. You only learned that no trap is present.",
                sizeof(line));
        }

        flush();
        chest_minigame_schedule_retry(y, x, line, result <= 0);
        return true;
    }

    if (action == CHEST_ACTION_DISARM)
    {
        score = p_ptr->skill_use[S_PER];
        if (p_ptr->active_ability[S_PER][PER_REWIRE_TRAPS])
            score += 5;
        if (fully_known)
            score += CHEST_FULL_DETECTION_DISARM_BONUS;
        difficulty = chest_disarm_difficulty(o_ptr);
        result = show_interaction_skill_roll_animation("Disarming the chest",
            "Testing the trap mechanism", y, x, score, difficulty, &roll);

        if (result > 0)
        {
            msg_print("You have disarmed the chest.");
            o_ptr->pval = 0 - o_ptr->pval;
            SDL_strlcpy(line,
                "Disarm succeeded. The trap and lock are disabled.",
                sizeof(line));
        }
        else if (result > -3)
        {
            msg_print("You failed to disarm the chest.");
            SDL_strlcpy(line,
                "Disarm failed. The trap remains armed.", sizeof(line));
        }
        else
        {
            msg_print("You set off the trap!");
            chest_trap(y, x, o_idx);
            SDL_strlcpy(line,
                "Disarm failed. The trap was triggered.", sizeof(line));
        }

        flush();
        if (!p_ptr->is_dead && o_ptr->pval != 0)
            chest_minigame_schedule_retry(y, x, line, result <= 0);
        return !p_ptr->is_dead && o_ptr->pval != 0;
    }

    /* Opening is always available.  Unlike the legacy flow, choosing it does
     * not silently make a separate disarm attempt first. */
    if (o_ptr->pval > 0)
    {
        score = p_ptr->skill_use[S_PER];
        difficulty = chest_lock_difficulty(o_ptr);
        result = show_interaction_skill_roll_animation("Picking the chest lock",
            "Working the lockpick", y, x, score, difficulty, &roll);
        if (result <= 0)
        {
            flush();
            message(MSG_LOCKPICK_FAIL, 0, "You failed to pick the lock.");
            SDL_strlcpy(line,
                "Lockpick failed. The chest remains locked.", sizeof(line));
            chest_minigame_schedule_retry(y, x, line, true);
            return true;
        }

        message(MSG_LOCKPICK_FAIL, 0, "You have picked the lock.");
    }

    sound(MSG_CHEST_OPEN);
    chest_trap(y, x, o_idx);
    chest_death(y, x, o_idx);
    chest_minigame_clear_retry();
    return false;
}

/*
 * Legacy chest opening used when the chest minigame is disabled.
 */
static bool do_cmd_open_chest_legacy(int y, int x, s16b o_idx)
{
    int score, power, difficulty, result;
    skill_roll_details lock_roll;
    skill_roll_details disarm_roll;

    bool flag = true;

    bool more = false;

    object_type* o_ptr = &o_list[o_idx];

    /* Attempt to unlock it */
    if (o_ptr->pval > 0)
    {
        /* Assume locked, and thus not open */
        flag = false;

        /* Get the score in favour (=perception) */
        score = p_ptr->skill_use[S_PER];

        /* Determine trap power based on the chest pval (power is 1--7)*/
        power = 1 + (o_ptr->pval / 4);

        // Base difficulty is the lock power plus the lockpick baseline.
        difficulty = power + CHEST_LOCK_BASE_DIFFICULTY;

        /* Penalize some conditions */
        if (p_ptr->blind || no_light() || p_ptr->image)
            difficulty += 5;
        if (p_ptr->confused)
            difficulty += 5;

        result = show_interaction_skill_roll_animation("Picking the chest lock",
            "Working the lockpick", y, x, score, difficulty, &lock_roll);

        /* Success -- May still have traps */
        if (result > 0)
        {
            message(MSG_LOCKPICK_FAIL, 0, "You have picked the lock.");

            /* A known trap gets a committed disarm attempt after the lock.
             * An undiscovered trap cannot be disarmed and fires when the
             * chest opens; report that consequence alongside the lock roll. */
            if (object_chest_trap_flags(o_ptr) && object_known_p(o_ptr))
            {
                /* Chest traps are concealed and fiddly: harder to disarm than
                 * a floor trap of the same depth, and harder still on better
                 * (higher-pval) chests. */
                difficulty = power + (o_ptr->pval / 4)
                    + CHEST_DISARM_BASE_DIFFICULTY;
                if (p_ptr->blind || no_light() || p_ptr->image)
                    difficulty += 5;
                if (p_ptr->confused)
                    difficulty += 5;
                /* Rewire Traps: +5 to the disarm only (score here is shared
                 * with the lock-pick, so apply it as -5 difficulty instead). */
                if (p_ptr->active_ability[S_PER][PER_REWIRE_TRAPS])
                    difficulty -= 5;

                result = show_interaction_skill_roll_animation(
                    "Disarming the chest", "Testing the trap mechanism", y, x,
                    score, difficulty, &disarm_roll);
                show_interaction_skill_roll_pair("Opening the chest", y, x,
                    "Lockpick", &lock_roll, "Disarm", &disarm_roll);

                if (result > 0)
                {
                    msg_print("You have disarmed the chest.");
                    o_ptr->pval = (0 - o_ptr->pval);
                }
                else
                {
                    msg_print("You fail to disarm the chest and set off its trap!");
                }
            }
            else if (object_chest_trap_flags(o_ptr))
            {
                show_interaction_skill_roll_status("Opening the chest", y, x,
                    "Lockpick", &lock_roll,
                    "Trap       undiscovered trap triggered", TERM_L_RED);
            }
            flag = true;
        }

        /* Failure -- Keep trying */
        else
        {
            /* We may continue repeating */
            more = true;
            flush();
            message(MSG_LOCKPICK_FAIL, 0, "You failed to pick the lock.");
        }
    }

    /* Allowed to open */
    if (flag)
    {
        sound(MSG_CHEST_OPEN);

        /* Apply chest traps, if any */
        chest_trap(y, x, o_idx);

        /* Let the Chest drop items */
        chest_death(y, x, o_idx);
    }

    /* Result */
    return (more);
}

/*
 * Attempt to disarm the chest at the given location
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
static bool do_cmd_disarm_chest_legacy(int y, int x, s16b o_idx)
{
    int score, power, difficulty, result;
    skill_roll_details roll;

    bool more = false;

    object_type* o_ptr = &o_list[o_idx];

    /* Get the score in favour (=perception) */
    score = p_ptr->skill_use[S_PER];

    /* If the chest's trap has not yet been discovered, a disarm attempt is
     * instead a deliberate inspection: roll Perception against the trap's
     * concealment.  Find it and a follow-up attempt may disarm it; fumble the
     * search badly and your clumsy probing springs the trap. */
    if (!object_known_p(o_ptr))
    {
        if ((o_ptr->pval > 0) && object_chest_trap_flags(o_ptr))
        {
            int find_diff = (o_ptr->pval / 2)
                + CHEST_CONCEALED_TRAP_BASE_DIFFICULTY;

            if (p_ptr->blind || no_light() || p_ptr->image)
                find_diff += 5;
            if (p_ptr->confused)
                find_diff += 5;

            result = show_interaction_skill_roll_animation("Inspecting chest",
                "Searching for traps", y, x, score, find_diff, &roll);

            if (result > 0)
            {
                msg_print("You discover a trap on the chest!");
                object_known(o_ptr);
                more = true; /* a follow-up attempt can now disarm it */
            }
            else if (result > -5)
            {
                msg_print("You find no sign of a trap... yet.");
                more = true;
            }
            else
            {
                msg_print("Your clumsy probing springs a hidden trap!");
                chest_trap(y, x, o_idx);
            }
        }
        else
        {
            msg_print("You don't see any traps.");
        }

        return (more);
    }

    /* Mastery of trap mechanisms (Rewire Traps) makes disarming easier.  Added
     * after the inspection step above so it aids the disarm, not the search. */
    if (p_ptr->active_ability[S_PER][PER_REWIRE_TRAPS])
        score += 5;

    /* Determine trap power based on the trap pval (power is 1--7)*/
    power = 1 + (o_ptr->pval / 4);

    // Chest traps are concealed and fiddly: harder to disarm than a floor trap
    // of the same depth, and harder still on better (higher-pval) chests.
    difficulty = power + (o_ptr->pval / 4)
        + CHEST_DISARM_BASE_DIFFICULTY;

    /* Penalize some conditions */
    if (p_ptr->blind || no_light() || p_ptr->image)
        difficulty += 5;
    if (p_ptr->confused)
        difficulty += 5;

    // perform the check
    result = show_interaction_skill_roll_animation("Disarming chest",
        "Testing the mechanism", y, x, score, difficulty, &roll);

    /* Already disarmed/unlocked (the trap is known by now -- an undiscovered
     * trap is handled by the inspection step above). */
    if (o_ptr->pval <= 0)
    {
        msg_print("The chest is not trapped.");
    }

    /* No traps to find. */
    else if (!object_chest_trap_flags(o_ptr))
    {
        msg_print("The chest is not trapped.");
    }

    /* Success (get a lot of experience) */
    else if (result > 0)
    {
        msg_print("You have disarmed the chest.");
        o_ptr->pval = (0 - o_ptr->pval);
    }

    /* Failure -- Keep trying */
    else if (result > -3)
    {
        /* We may keep trying */
        more = true;
        flush();
        msg_print("You failed to disarm the chest.");
    }

    /* Failure -- Set off the trap */
    else
    {
        msg_print("You set off a trap!");
        chest_trap(y, x, o_idx);
    }

    /* Result */
    return (more);
}

bool do_cmd_open_chest(int y, int x, s16b o_idx)
{
    if (chest_trap_minigame)
        return do_cmd_chest_minigame(y, x, o_idx);

    chest_minigame_clear_retry();
    return do_cmd_open_chest_legacy(y, x, o_idx);
}

bool do_cmd_disarm_chest(int y, int x, s16b o_idx)
{
    if (chest_trap_minigame)
        return do_cmd_chest_minigame(y, x, o_idx);

    chest_minigame_clear_retry();
    return do_cmd_disarm_chest_legacy(y, x, o_idx);
}
