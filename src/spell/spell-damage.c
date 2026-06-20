/* File: spell/spell-damage.c */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "supplies.h"
#include <math.h>

void attempt_to_cheat_death(void)
{
    char o_name[80];

    /* Scan the equipment */
    for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        u32b f1, f2, f3;

        object_type* o_ptr = &inventory[i];
        object_flags(o_ptr, &f1, &f2, &f3);

        /* If player is dead, save them at the cost of the item */
        if (f3 & TR3_CHEAT_DEATH && p_ptr->chp <= 0)
        {
            p_ptr->chp = MAX(1, (p_ptr->mhp + 3) / 4);
            p_ptr->energy += 100;
            set_blind(0);
            set_confused(0);
            set_poisoned(0);
            set_afraid(0);
            set_entranced(0);
            set_image(0);
            set_stun(0);
            set_cut(0);
            set_slow(0);

            /* Get a description */
            object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

            msg_format("Your %s breaks into two pieces!", o_name);
            ident_f3(TR3_CHEAT_DEATH, o_ptr);

            inven_item_increase(i, -1);
            inven_item_optimize(i);
        }
    }
}

/*
 * Decreases players hit points and sets death flag if necessary
 *
 * Invulnerability needs to be changed into a "shield" XXX XXX XXX
 *
 * Hack -- this function allows the user to save (or quit) the game
 * when he dies, since the "You die." message is shown before setting
 * the player to "dead".
 */
void take_hit(int dam, cptr kb_str)
{
    int old_chp = p_ptr->chp;

    int warning = (p_ptr->mhp * op_ptr->hitpoint_warn / 10);

    time_t ct = time((time_t*)0);
    char long_day[40];
    char buf[120];

    /* Paranoia */
    if (p_ptr->is_dead)
        return;

    /* Disturb */
    disturb(1, 0);

    /* Hurt the player */
    p_ptr->chp -= dam;

    attempt_to_cheat_death();

    /* Display the hitpoints */
    p_ptr->redraw |= (PR_HP);

    /* Window stuff */
    p_ptr->window |= (PW_PLAYER_0);

    if (p_ptr->chp <= 0)
    {
        /* Hack -- Note death */
        message(MSG_DEATH, 0, "You die.");
        message_flush();

        /* Note cause of death */
        if (p_ptr->image == 0)
        {
            SDL_strlcpy(p_ptr->died_from, kb_str, sizeof(p_ptr->died_from));
        }
        else
        {
            strnfmt(p_ptr->died_from, sizeof(p_ptr->died_from),
                "%s (while hallucinating)", kb_str);
        }

        killer_commit(kb_str);

        /* Note death */
        p_ptr->is_dead = true;

        /* Leaving */
        p_ptr->leaving = true;

        /* Write a note */

        /* Get time */
        (void)strftime(long_day, 40, "%d %B %Y", localtime(&ct));

        /* Add note */
        SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

        /*killed by */
        sprintf(buf, "Slain by %s.", p_ptr->died_from);

        /* Write message */
        do_cmd_note(buf, p_ptr->depth);

        /* date and time*/
        sprintf(buf, "Died on %s.", long_day);

        /* Write message */
        do_cmd_note(buf, p_ptr->depth);

        SDL_strlcat(notes_buffer, "\n", sizeof(notes_buffer));

        /* Dead */
        return;
    }

    /* Hitpoint warning */
    if (p_ptr->chp < warning)
    {
        /* Hack -- bell on first notice */
        if (old_chp > warning)
        {
            bell("Low hitpoint warning!");
        }

        /* Message */
        message(MSG_HITPOINT_WARN, 0, "*** LOW HITPOINT WARNING! ***");
        message_flush();
    }

    // Cancel entrancement
    set_entranced(0);
}

/*
 * Does a given class of objects (usually) hate acid?
 * Note that acid can either melt or corrode something.
 */
bool hates_acid(const object_type* o_ptr)
{
    /* Analyze the type */
    switch (o_ptr->tval)
    {
    /* Wearable items */
    case TV_ARROW:
    case TV_BOW:
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        return (true);
    }

    /* Staffs are wood */
    case TV_STAFF:
    {
        return (true);
    }

    /* Ouch */
    case TV_CHEST:
    {
        return (true);
    }

    /* Skeleton */
    case TV_SKELETON:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Does a given object (usually) hate electricity?
 */
bool hates_elec(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_RING:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Does a given object (usually) hate fire?
 * Hafted/Polearm weapons have wooden shafts.
 * Arrows/Bows are mostly wooden.
 */
bool hates_fire(const object_type* o_ptr)
{
    /* Analyze the type */
    switch (o_ptr->tval)
    {
    /* Wearable */
    case TV_ARROW:
    case TV_BOW:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    {
        return (true);
    }

    /* Chests */
    case TV_CHEST:
    {
        return (true);
    }

    /* Torches */
    case TV_LIGHT:
    {
        if (o_ptr->sval == SV_LIGHT_TORCH || o_ptr->sval == SV_LIGHT_MALLORN)
            return (true);
        else
            return (false);
    }

    /* Notes burn */
    case TV_NOTE:
    {
        return (true);
    }

    /* Staffs burn */
    case TV_STAFF:
    {
        return (true);
    }
    }

    return (false);
}

/*
 * Does a given object (usually) hate cold?
 */
bool hates_cold(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_POTION:
    case TV_GEM:
    case TV_FLASK:
    {
        return (true);
    }
    case TV_LIGHT:
    {
        return (o_ptr->sval == SV_LIGHT_LANTERN)
            || (o_ptr->sval == SV_LIGHT_LESSER_JEWEL);
    }
    }

    return (false);
}

/*
 * Melt something
 */
static int set_acid_destroy(const object_type* o_ptr)
{
    u32b f1, f2, f3;
    if (!hates_acid(o_ptr))
        return (false);
    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & (TR3_IGNORE_ACID))
        return (false);
    return (true);
}

/*
 * Electrical damage
 */
static int set_elec_destroy(const object_type* o_ptr)
{
    u32b f1, f2, f3;
    if (!hates_elec(o_ptr))
        return (false);
    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & (TR3_IGNORE_ELEC))
        return (false);
    return (true);
}

/*
 * Burn something
 */
static int set_fire_destroy(const object_type* o_ptr)
{
    u32b f1, f2, f3;
    if (!hates_fire(o_ptr))
        return (false);
    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & (TR3_IGNORE_FIRE))
        return (false);
    return (true);
}

/*
 * Freeze things
 */
static int set_cold_destroy(const object_type* o_ptr)
{
    u32b f1, f2, f3;
    if (!hates_cold(o_ptr))
        return (false);
    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & (TR3_IGNORE_COLD))
        return (false);
    return (true);
}

/*
 * Loud concussive force shatters fragile carried items like cold does.
 */
static int set_sound_destroy(const object_type* o_ptr)
{
    return hates_cold(o_ptr);
}

/*
 * This seems like a pretty standard "typedef"
 */
typedef int (*inven_func)(const object_type*);

typedef enum elemental_item_candidate_location
{
    ELEMENTAL_CANDIDATE_INVENTORY = 0,
    ELEMENTAL_CANDIDATE_SUPPLY = 1
} elemental_item_candidate_location;

typedef struct elemental_item_candidate
{
    elemental_item_candidate_location location;
    int index;
    object_type* o_ptr;
    long weight;
    int units;
    int unit_size;
    int quantity_per_unit;
} elemental_item_candidate;

typedef struct elemental_item_debug_info
{
    bool enabled;
    int attack_type;
    int raw_dam;
    int min_raw;
    int max_raw;
    int hp_dam;
    double roll_cdf;
    double q_factor;
    double hurt_factor;
    int threshold;
    bool gate_roll_made;
    int gate_roll;
    int candidate_count;
    long total_weight;
    bool candidate_selected;
    int selection_roll;
    elemental_item_candidate_location selected_location;
    int selected_index;
    long selected_weight;
    double slot_factor;
    double material_factor;
    double stack_factor;
    bool acid_roll_made;
    int acid_roll;
    char selected_name[80];
    cptr outcome;
} elemental_item_debug_info;

enum
{
    ELEMENTAL_PERCENT_ROLL = 100,
    ELEMENTAL_ACID_REDUCTION_PERCENT = 70
};

static cptr elemental_attack_name(int attack_type)
{
    switch (attack_type)
    {
    case GF_ACID:
        return "acid";
    case GF_ELEC:
        return "elec";
    case GF_FIRE:
        return "fire";
    case GF_COLD:
        return "cold";
    case GF_SOUND:
        return "sound";
    default:
        return "unknown";
    }
}

static void elemental_debug_init(elemental_item_debug_info* debug,
    int attack_type, int raw_dam, int min_raw, int max_raw, int hp_dam)
{
    if (!debug)
        return;

    memset(debug, 0, sizeof(*debug));
    debug->enabled = show_elemental_item_rolls;
    debug->attack_type = attack_type;
    debug->raw_dam = raw_dam;
    debug->min_raw = min_raw;
    debug->max_raw = max_raw;
    debug->hp_dam = hp_dam;
}

static void elemental_debug_slot_desc(
    elemental_item_candidate_location location, int index, char* buf,
    size_t buf_size)
{
    if (!buf || (buf_size == 0))
        return;

    if (location == ELEMENTAL_CANDIDATE_SUPPLY)
    {
        strnfmt(buf, buf_size, "supplies[%d]", index);
        return;
    }

    if (index < INVEN_PACK)
    {
        strnfmt(buf, buf_size, "pack(%c)", index_to_label(index));
        return;
    }

    switch (index)
    {
    case INVEN_WIELD:
        strnfmt(buf, buf_size, "wield");
        return;
    case INVEN_BOW:
        strnfmt(buf, buf_size, "bow");
        return;
    case INVEN_STAFF:
        strnfmt(buf, buf_size, "staff");
        return;
    case INVEN_LEFT:
        strnfmt(buf, buf_size, "left");
        return;
    case INVEN_RIGHT:
        strnfmt(buf, buf_size, "right");
        return;
    case INVEN_NECK:
        strnfmt(buf, buf_size, "neck");
        return;
    case INVEN_LITE:
        strnfmt(buf, buf_size, "light");
        return;
    case INVEN_BODY:
        strnfmt(buf, buf_size, "body");
        return;
    case INVEN_OUTER:
        strnfmt(buf, buf_size, "outer");
        return;
    case INVEN_ARM:
        strnfmt(buf, buf_size, "arm");
        return;
    case INVEN_HEAD:
        strnfmt(buf, buf_size, "head");
        return;
    case INVEN_HANDS:
        strnfmt(buf, buf_size, "hands");
        return;
    case INVEN_FEET:
        strnfmt(buf, buf_size, "feet");
        return;
    case INVEN_QUIVER1:
        strnfmt(buf, buf_size, "quiver1");
        return;
    case INVEN_QUIVER2:
        strnfmt(buf, buf_size, "quiver2");
        return;
    case INVEN_HORN:
        strnfmt(buf, buf_size, "horn");
        return;
    default:
        strnfmt(buf, buf_size, "slot[%d]", index);
        return;
    }
}

static bool elemental_slot_uses_pack_like_factor(int slot,
    elemental_item_candidate_location location);
static double elemental_item_slot_factor(int slot,
    elemental_item_candidate_location location);
static double elemental_item_material_factor(int attack_type,
    const object_type* o_ptr);
static void elemental_mark_inventory_item_changed(void);
static bool acid_can_corrode_object(const object_type* o_ptr);
static cptr elemental_corrode_candidate(
    const elemental_item_candidate* candidate, elemental_item_debug_info* debug);

static void elemental_debug_record_candidate(
    elemental_item_debug_info* debug, int attack_type,
    const elemental_item_candidate* candidate, int selection_roll,
    int candidate_count, long total_weight)
{
    double stack_factor = 1.0;

    if (!debug || !candidate || !candidate->o_ptr)
        return;

    debug->candidate_count = candidate_count;
    debug->total_weight = total_weight;
    debug->candidate_selected = true;
    debug->selection_roll = selection_roll;
    debug->selected_location = candidate->location;
    debug->selected_index = candidate->index;
    debug->selected_weight = candidate->weight;
    debug->slot_factor = elemental_item_slot_factor(candidate->index,
        candidate->location);
    debug->material_factor = elemental_item_material_factor(attack_type,
        candidate->o_ptr);

    if (elemental_slot_uses_pack_like_factor(candidate->index,
        candidate->location))
    {
        stack_factor = sqrt((double)MAX(candidate->o_ptr->number, 1));
    }

    debug->stack_factor = stack_factor;
    object_desc(debug->selected_name, sizeof(debug->selected_name),
        candidate->o_ptr, false, 3);
}

static void elemental_debug_emit(const elemental_item_debug_info* debug)
{
    char gate_buf[64];
    char target_buf[256];
    char slot_buf[32];
    char acid_buf[32];
    char buf[768];
    double threshold_pct;
    double candidate_pct = 0.0;

    if (!debug || !debug->enabled || !debug->outcome)
        return;

    if (!debug->gate_roll_made)
    {
        strnfmt(gate_buf, sizeof(gate_buf), "gate=skip");
    }
    else
    {
        strnfmt(gate_buf, sizeof(gate_buf), "gate=%d%s%d",
            debug->gate_roll, (debug->gate_roll < debug->threshold) ? "<" : ">=",
            debug->threshold);
    }

    target_buf[0] = '\0';
    if (debug->candidate_selected && (debug->total_weight > 0))
    {
        elemental_debug_slot_desc(debug->selected_location, debug->selected_index,
            slot_buf, sizeof(slot_buf));
        candidate_pct = ((double)debug->selected_weight * 100.0)
            / (double)debug->total_weight;
        strnfmt(target_buf, sizeof(target_buf),
            " pick=%d/%ld target=%s@%s w=%ld(%.2f%%) sf=%.2f mf=%.2f st=%.2f",
            debug->selection_roll, debug->total_weight, debug->selected_name,
            slot_buf, debug->selected_weight, candidate_pct,
            debug->slot_factor, debug->material_factor, debug->stack_factor);
    }

    acid_buf[0] = '\0';
    if (debug->acid_roll_made)
    {
        strnfmt(acid_buf, sizeof(acid_buf), " acid=%d/100->%s",
            debug->acid_roll + 1,
            (debug->acid_roll < ELEMENTAL_ACID_REDUCTION_PERCENT) ? "corrode"
                                                                   : "destroy");
    }

    threshold_pct = (double)debug->threshold / 10000.0;
    strnfmt(buf, sizeof(buf),
        "[Elem %s] raw=%d/%d..%d hp=%d cdf=%.1f%% q=%.3f hurt=%.3f chance=%.4f%% %s cand=%d%s%s -> %s",
        elemental_attack_name(debug->attack_type), debug->raw_dam, debug->min_raw,
        debug->max_raw, debug->hp_dam, debug->roll_cdf * 100.0,
        debug->q_factor, debug->hurt_factor,
        threshold_pct, gate_buf, debug->candidate_count, target_buf, acid_buf,
        debug->outcome);
    msg_print(buf);
}

bool elemental_attack_destroys_object(int attack_type, const object_type* o_ptr)
{
    inven_func typ = NULL;

    switch (attack_type)
    {
    case GF_ACID:
        typ = set_acid_destroy;
        break;
    case GF_ELEC:
        typ = set_elec_destroy;
        break;
    case GF_FIRE:
        typ = set_fire_destroy;
        break;
    case GF_COLD:
        typ = set_cold_destroy;
        break;
    case GF_SOUND:
        typ = set_sound_destroy;
        break;
    }

    if (!typ || !o_ptr)
        return false;

    return (*typ)(o_ptr) ? true : false;
}

static bool elemental_attack_can_target_object(int attack_type,
    const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (artefact_p(o_ptr))
        return false;

    if (!elemental_attack_destroys_object(attack_type, o_ptr))
        return false;

    if ((attack_type == GF_FIRE) && object_is_fire_broken(o_ptr))
        return false;

    if (((attack_type == GF_COLD) || (attack_type == GF_SOUND))
        && (o_ptr->tval == TV_LIGHT) && (o_ptr->sval == SV_LIGHT_LANTERN)
        && (object_ego_prefix(o_ptr) == EGO_BROKEN_BRASS_LANTERN))
    {
        return false;
    }

    return true;
}

static double elemental_clamp01(double value)
{
    if (value < 0.0)
        return 0.0;
    if (value > 1.0)
        return 1.0;
    return value;
}

static double elemental_hurt_scale(int attack_type)
{
    switch (attack_type)
    {
    case GF_FIRE:
        return 60.0 / 3.0;
    case GF_ACID:
        return 60.0 / 3.0;
    case GF_COLD:
        return 35.0 / 3.0;
    default:
        return 80.0 / 3.0;
    }
}

static double elemental_linear_damage_percentile(int raw_dam, int min_raw,
    int max_raw)
{
    double percentile;

    if (max_raw < min_raw)
    {
        int tmp = max_raw;
        max_raw = min_raw;
        min_raw = tmp;
    }

    if (max_raw == min_raw)
        return (raw_dam >= max_raw) ? 1.0 : 0.0;

    percentile = ((double)raw_dam - (double)min_raw)
        / ((double)max_raw - (double)min_raw);
    return elemental_clamp01(percentile);
}

static bool elemental_damage_roll_shape(int min_raw, int max_raw, int* dice,
    int* sides)
{
    if (!dice || !sides)
        return false;

    if ((min_raw <= 0) || (max_raw < min_raw))
        return false;

    if ((max_raw % min_raw) != 0)
        return false;

    *dice = min_raw;
    *sides = max_raw / min_raw;
    return (*sides >= 1);
}

/*
 * Use the actual NdS distribution when min/max describe one; otherwise,
 * fall back to the previous linear percentile.
 */
static double elemental_damage_cdf_percentile(int raw_dam, int min_raw,
    int max_raw)
{
    int dice;
    int sides;
    int max_sum;
    int capped_raw;
    int prev_max = 0;
    double cdf = 0.0;
    double* prev;
    double* next;

    if (max_raw < min_raw)
    {
        int tmp = max_raw;
        max_raw = min_raw;
        min_raw = tmp;
    }

    if (raw_dam < min_raw)
        return 0.0;

    if (raw_dam >= max_raw)
        return 1.0;

    if (!elemental_damage_roll_shape(min_raw, max_raw, &dice, &sides))
    {
        return elemental_linear_damage_percentile(raw_dam, min_raw, max_raw);
    }

    if (sides == 1)
        return (raw_dam >= dice) ? 1.0 : 0.0;

    max_sum = dice * sides;
    prev = mem_alloc_array(max_sum + 1, double);
    next = mem_alloc_array(max_sum + 1, double);
    if (!prev || !next)
    {
        prev = mem_free(prev);
        next = mem_free(next);
        return elemental_linear_damage_percentile(raw_dam, min_raw, max_raw);
    }

    prev[0] = 1.0;

    for (int die = 0; die < dice; die++)
    {
        memset(next, 0, (size_t)(max_sum + 1) * sizeof(*next));

        for (int sum = 0; sum <= prev_max; sum++)
        {
            double probability = prev[sum];

            if (probability <= 0.0)
                continue;

            probability /= (double)sides;
            for (int face = 1; face <= sides; face++)
            {
                next[sum + face] += probability;
            }
        }

        {
            double* tmp = prev;
            prev = next;
            next = tmp;
        }

        prev_max += sides;
    }

    capped_raw = MIN(raw_dam, max_sum);
    for (int sum = min_raw; sum <= capped_raw; sum++)
    {
        cdf += prev[sum];
    }

    prev = mem_free(prev);
    next = mem_free(next);
    return elemental_clamp01(cdf);
}

static int elemental_attack_probability_per_million(int attack_type,
    int raw_dam, int min_raw, int max_raw, int hp_dam,
    elemental_item_debug_info* debug)
{
    double percentile;
    double q;
    double hp = (double)hp_dam;
    const double hurt_scale = elemental_hurt_scale(attack_type);
    double hurt;
    double chance;
    int threshold;

    if (debug)
    {
        debug->roll_cdf = 0.0;
        debug->q_factor = 0.0;
        debug->hurt_factor = 0.0;
        debug->threshold = 0;
    }

    if (hp_dam <= 0)
        return 0;

    percentile = elemental_damage_cdf_percentile(raw_dam, min_raw, max_raw);
    q = elemental_clamp01((percentile - 0.50) / 0.50);
    hurt = (hp * hp) / ((hp * hp) + (hurt_scale * hurt_scale));
    chance = q * q * hurt;
    threshold = (int)(chance * 1000000.0 + 0.5);

    if (threshold < 0)
        threshold = 0;
    if (threshold > 1000000)
        threshold = 1000000;

    if (debug)
    {
        debug->roll_cdf = percentile;
        debug->q_factor = q;
        debug->hurt_factor = hurt;
        debug->threshold = threshold;
    }

    return threshold;
}

static void elemental_debug_emit_size_summary(int attack_type, int raw_dam,
    int min_raw, int max_raw, int hp_dam, double cdf, double q_squared,
    double hurt, double chance, int total, int remaining, cptr outcome)
{
    char buf1[192];
    char buf2[192];

    if (!show_elemental_item_rolls || !outcome)
        return;

    strnfmt(buf1, sizeof(buf1),
        "[Elem %s] raw=%d/%d..%d hp=%d cdf=%.1f%% q2=%.3f hurt=%.3f chance=%.4f%%",
        elemental_attack_name(attack_type), raw_dam, min_raw, max_raw, hp_dam,
        cdf * 100.0, q_squared, hurt, chance * 100.0);
    strnfmt(buf2, sizeof(buf2), "[Elem %s] total=%d remaining=%d -> %s",
        elemental_attack_name(attack_type), total, remaining, outcome);
    msg_print(buf1);
    msg_print(buf2);
}

static bool elemental_attack_matches_object_material(int attack_type,
    const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    switch (attack_type)
    {
    case GF_ACID:
        return hates_acid(o_ptr);
    case GF_ELEC:
        return hates_elec(o_ptr);
    case GF_FIRE:
        return hates_fire(o_ptr);
    case GF_COLD:
        return hates_cold(o_ptr);
    default:
        return false;
    }
}

static bool elemental_attack_allows_size_location(int attack_type,
    elemental_item_candidate_location location, int index,
    const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    switch (attack_type)
    {
    case GF_ACID:
    case GF_ELEC:
    case GF_FIRE:
        return (location == ELEMENTAL_CANDIDATE_INVENTORY)
            && (index >= INVEN_WIELD) && (index < INVEN_TOTAL);

    case GF_COLD:
        if (location == ELEMENTAL_CANDIDATE_SUPPLY)
            return o_ptr->tval != TV_LIGHT;

        return (location == ELEMENTAL_CANDIDATE_INVENTORY)
            && (index == INVEN_LITE);

    default:
        return false;
    }
}

static bool elemental_object_has_attack_resistance(const object_type* o_ptr,
    int attack_type)
{
    u32b f1, f2, f3, f4;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    (void)f1;
    (void)f4;

    switch (attack_type)
    {
    case GF_ACID:
        return (f3 & TR3_IGNORE_ACID) ? true : false;
    case GF_ELEC:
        return ((f2 & TR2_RES_ELEC) || (f3 & TR3_IGNORE_ELEC)) ? true : false;
    case GF_FIRE:
        return ((f2 & TR2_RES_FIRE) || (f3 & TR3_IGNORE_FIRE)) ? true : false;
    case GF_COLD:
        return ((f2 & TR2_RES_COLD) || (f3 & TR3_IGNORE_COLD)) ? true : false;
    default:
        return false;
    }
}

static bool elemental_shield_has_attack_protection(const object_type* o_ptr,
    int attack_type)
{
    u32b f1, f2, f3, f4;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    (void)f1;
    (void)f2;

    switch (attack_type)
    {
    case GF_ACID:
        return (f3 & TR3_IGNORE_ACID) ? true : false;
    case GF_ELEC:
        return (f3 & TR3_IGNORE_ELEC) ? true : false;
    case GF_FIRE:
        return ((f4 & TR4_PROT_FIRE) || (f3 & TR3_IGNORE_FIRE)) ? true : false;
    case GF_COLD:
        return ((f4 & TR4_PROT_COLD) || (f3 & TR3_IGNORE_COLD)) ? true : false;
    default:
        return false;
    }
}

static int elemental_shield_block_base(const object_type* o_ptr)
{
    int chance = 0;
    byte ego_idx;

    if (!o_ptr || !o_ptr->k_idx || (o_ptr->tval != TV_SHIELD))
        return 0;

    chance += k_info[o_ptr->k_idx].elemental_block;

    if (o_ptr->name1)
        chance += a_info[o_ptr->name1].elemental_block;

    ego_idx = object_ego_prefix(o_ptr);
    if (ego_idx)
        chance += e_info[ego_idx].elemental_block;

    ego_idx = object_ego_suffix(o_ptr);
    if (ego_idx)
        chance += e_info[ego_idx].elemental_block;

    return chance;
}

static int elemental_shield_block_chance(const object_type* o_ptr,
    int attack_type)
{
    int chance = elemental_shield_block_base(o_ptr);

    if (!o_ptr || !o_ptr->k_idx || (o_ptr->tval != TV_SHIELD))
        return 0;

    if (elemental_object_has_attack_resistance(o_ptr, attack_type))
        chance += 25;

    if (blocking_bonus_active())
        chance += 25;

    if (chance < 0)
        chance = 0;
    if (chance > 100)
        chance = 100;

    return chance;
}

static object_type* elemental_equipped_shield(void)
{
    object_type* o_ptr = &inventory[INVEN_ARM];

    if (!o_ptr->k_idx || (o_ptr->tval != TV_SHIELD))
        return NULL;
    if (!player_shield_counts_for_active_weapon(o_ptr))
        return NULL;

    return o_ptr;
}

static bool elemental_damage_blocking_shield(object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx || (o_ptr->ps <= 0))
        return false;

    o_ptr->ps--;
    elemental_mark_inventory_item_changed();
    return true;
}

static int elemental_item_unit_size(const object_type* o_ptr,
    elemental_item_candidate_location location, int index)
{
    u32b f1, f2, f3, f4;

    if (!o_ptr || !o_ptr->k_idx)
        return 0;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    (void)f1;
    (void)f2;
    (void)f4;

    if ((location == ELEMENTAL_CANDIDATE_INVENTORY)
        && (index >= INVEN_WIELD) && (index < INVEN_TOTAL)
        && ((f3 & TR3_THROWING) || (index == INVEN_QUIVER1)
            || (index == INVEN_QUIVER2)))
    {
        return 1;
    }

    switch (o_ptr->tval)
    {
    case TV_ARROW:
        return 1;
    case TV_SHIELD:
        return 2;
    case TV_SWORD:
    case TV_HAFTED:
    case TV_POLEARM:
        return 2;
    case TV_BOW:
        return 2;
    case TV_STAFF:
        return 1;
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_CLOAK:
        return 1;
    case TV_SOFT_ARMOR:
    case TV_MAIL:
        return 2;
    case TV_POTION:
    case TV_GEM:
        return 1;
    case TV_LIGHT:
        return ((o_ptr->sval == SV_LIGHT_TORCH)
            || (o_ptr->sval == SV_LIGHT_MALLORN))
            ? 1
            : 2;
    default:
        return 1;
    }
}

static int elemental_item_unit_count(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx || (o_ptr->number <= 0))
        return 0;

    if (o_ptr->tval == TV_ARROW)
        return (o_ptr->number + 11) / 12;

    return o_ptr->number;
}

static int elemental_item_quantity_per_unit(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx || (o_ptr->number <= 0))
        return 0;

    if (o_ptr->tval == TV_ARROW)
        return MIN(o_ptr->number, 12);

    return 1;
}

static void elemental_describe_quantity(char* buf, size_t buf_size,
    const elemental_item_candidate* candidate, int amount)
{
    object_type desc_obj;

    if (!buf || (buf_size == 0) || !candidate || !candidate->o_ptr)
        return;

    object_copy(&desc_obj, candidate->o_ptr);
    desc_obj.number = (byte)amount;
    object_desc(buf, buf_size, &desc_obj, false, 3);
}

static void elemental_message_amount(const elemental_item_candidate* candidate,
    int original_number, int amount, cptr o_name, cptr singular_action,
    cptr plural_action)
{
    cptr owner;
    cptr action = (amount > 1) ? plural_action : singular_action;

    if (original_number > 1)
    {
        if (amount >= original_number)
            owner = "All of your";
        else if (amount > 1)
            owner = "Some of your";
        else
            owner = "One of your";
    }
    else
    {
        owner = "Your";
    }

    if (candidate->location == ELEMENTAL_CANDIDATE_SUPPLY)
    {
        msg_format("%s %s in your supplies %s", owner, o_name, action);
    }
    else if (candidate->index < INVEN_PACK)
    {
        msg_format("%s %s (%c) %s", owner, o_name,
            index_to_label(candidate->index), action);
    }
    else
    {
        msg_format("%s %s %s", owner, o_name, action);
    }
}

static void elemental_remove_quantity_from_candidate(
    const elemental_item_candidate* candidate, int amount)
{
    object_type* o_ptr;
    int total_before;
    int charges_lost;

    if (!candidate || !candidate->o_ptr || (amount <= 0))
        return;

    o_ptr = candidate->o_ptr;
    amount = MIN(amount, o_ptr->number);

    if (candidate->location == ELEMENTAL_CANDIDATE_SUPPLY)
    {
        (void)supplies_consume_quantity(candidate->index, amount);
        return;
    }

    if (((o_ptr->tval == TV_STAFF) || (o_ptr->tval == TV_HORN))
        && (o_ptr->number > amount))
    {
        total_before = o_ptr->number;
        charges_lost = (o_ptr->pval * amount) / total_before;
        if ((charges_lost <= 0) && (o_ptr->pval > 0))
            charges_lost = 1;
        if (charges_lost > o_ptr->pval)
            charges_lost = o_ptr->pval;
        o_ptr->pval -= charges_lost;
    }

    inven_item_increase(candidate->index, -amount);
    inven_item_optimize(candidate->index);
}

static void elemental_destroy_candidate_quantity(
    const elemental_item_candidate* candidate, int attack_type, int amount)
{
    char o_name[80];
    int original_number;

    if (!candidate || !candidate->o_ptr || (amount <= 0))
        return;

    original_number = candidate->o_ptr->number;
    amount = MIN(amount, original_number);
    elemental_describe_quantity(o_name, sizeof(o_name), candidate, amount);

    if ((candidate->o_ptr->tval == TV_CHEST) && (amount > 0))
    {
        chest_release_contents(candidate->o_ptr, p_ptr->py, p_ptr->px,
            attack_type);
    }

    elemental_remove_quantity_from_candidate(candidate, amount);
    elemental_message_amount(candidate, original_number, amount, o_name,
        "was destroyed!", "were destroyed!");
}

static int elemental_damage_quota_divisor(int attack_type)
{
    switch (attack_type)
    {
    case GF_ACID:
        return 20;
    case GF_ELEC:
        return 10;
    case GF_FIRE:
        return 10;
    case GF_COLD:
        return 5;
    default:
        return 0;
    }
}

static int elemental_damage_total(int attack_type, int hp_dam)
{
    int divisor = elemental_damage_quota_divisor(attack_type);
    int groups;
    int total = 1;

    if ((hp_dam <= 0) || (divisor <= 0))
        return 0;

    groups = hp_dam / divisor;
    if (groups < 1)
        groups = 1;

    for (int i = 2; i <= groups; i++)
    {
        if (one_in_(i))
            total++;
    }

    return total;
}

static bool elemental_acid_roll_reduces(int* roll)
{
    int value = rand_int(ELEMENTAL_PERCENT_ROLL);

    if (roll)
        *roll = value;

    return value < ELEMENTAL_ACID_REDUCTION_PERCENT;
}

static bool elemental_select_size_candidate(int attack_type, int total,
    elemental_item_candidate* out, int* candidate_count, long* total_units,
    int* selection_roll)
{
    int supply_count = supplies_entry_count();
    int capacity = INVEN_TOTAL + supply_count;
    elemental_item_candidate* candidates;
    int count = 0;
    long available_units = 0;
    int pick;
    int allowed_size = total;

retry_with_size:

    if (!out || (capacity <= 0) || (total <= 0))
        return false;

    candidates = mem_alloc_array(capacity, elemental_item_candidate);
    if (!candidates)
        return false;

    for (int slot = 0; slot < INVEN_TOTAL; slot++)
    {
        object_type* o_ptr = &inventory[slot];
        int unit_size;
        int units;

        if (!o_ptr->k_idx)
            continue;

        if (!elemental_attack_allows_size_location(attack_type,
                ELEMENTAL_CANDIDATE_INVENTORY, slot, o_ptr))
            continue;

        if (!elemental_attack_matches_object_material(attack_type, o_ptr))
            continue;

        unit_size = elemental_item_unit_size(o_ptr,
            ELEMENTAL_CANDIDATE_INVENTORY, slot);
        if ((unit_size <= 0) || (unit_size > allowed_size))
            continue;

        units = elemental_item_unit_count(o_ptr);
        if (units <= 0)
            continue;

        candidates[count].location = ELEMENTAL_CANDIDATE_INVENTORY;
        candidates[count].index = slot;
        candidates[count].o_ptr = o_ptr;
        candidates[count].weight = units;
        candidates[count].units = units;
        candidates[count].unit_size = unit_size;
        candidates[count].quantity_per_unit = elemental_item_quantity_per_unit(o_ptr);
        available_units += units;
        count++;
    }

    for (int idx = 0; idx < supply_count; idx++)
    {
        object_type* o_ptr = supplies_entry_at(idx);
        int unit_size;
        int units;

        if (!o_ptr || !o_ptr->k_idx)
            continue;

        if (!elemental_attack_allows_size_location(attack_type,
                ELEMENTAL_CANDIDATE_SUPPLY, idx, o_ptr))
            continue;

        if (!elemental_attack_matches_object_material(attack_type, o_ptr))
            continue;

        unit_size = elemental_item_unit_size(o_ptr,
            ELEMENTAL_CANDIDATE_SUPPLY, idx);
        if ((unit_size <= 0) || (unit_size > allowed_size))
            continue;

        units = elemental_item_unit_count(o_ptr);
        if (units <= 0)
            continue;

        candidates[count].location = ELEMENTAL_CANDIDATE_SUPPLY;
        candidates[count].index = idx;
        candidates[count].o_ptr = o_ptr;
        candidates[count].weight = units;
        candidates[count].units = units;
        candidates[count].unit_size = unit_size;
        candidates[count].quantity_per_unit = elemental_item_quantity_per_unit(o_ptr);
        available_units += units;
        count++;
    }

    if (candidate_count)
        *candidate_count = count;
    if (total_units)
        *total_units = available_units;

    if ((count <= 0) || (available_units <= 0))
    {
        if ((allowed_size == total) && (total == 1))
        {
            allowed_size = total + 1;
            goto retry_with_size;
        }

        mem_free(candidates);
        return false;
    }

    pick = rand_int((int)available_units);
    if (selection_roll)
        *selection_roll = pick;

    for (int i = 0; i < count; i++)
    {
        if (pick < candidates[i].units)
        {
            *out = candidates[i];
            mem_free(candidates);
            return true;
        }

        pick -= candidates[i].units;
    }

    *out = candidates[count - 1];
    mem_free(candidates);
    return true;
}

static void elemental_attack_affect_multiple_items(int attack_type,
    int raw_dam, int min_raw, int max_raw, int hp_dam)
{
    double cdf;
    double q;
    double q_squared;
    double hp = (double)hp_dam;
    const double hurt_scale = elemental_hurt_scale(attack_type);
    double hurt;
    double chance;
    int threshold;
    int total;
    int total_budget;
    int destroyed = 0;
    int destroyed_size = 0;
    int reduced = 0;
    int reduced_size = 0;
    int resisted = 0;
    int resisted_size = 0;
    char outcome[80];

    if (hp_dam <= 0)
        return;

    cdf = elemental_damage_cdf_percentile(raw_dam, min_raw, max_raw);
    if (cdf <= 0.50)
    {
        elemental_debug_emit_size_summary(attack_type, raw_dam, min_raw,
            max_raw, hp_dam, cdf, 0.0, 0.0, 0.0, 0, 0, "cdf<=50%");
        return;
    }

    q = elemental_clamp01((cdf - 0.50) / 0.50);
    q_squared = q * q;

    if (q_squared <= 0.50)
        msg_print("The elemental assault was furious.");
    else
        msg_print("The elemental assault was devastating.");

    hurt = (hp * hp) / ((hp * hp) + (hurt_scale * hurt_scale));
    chance = q_squared * hurt;
    threshold = (int)(chance * 1000000.0 + 0.5);

    if (threshold <= 0)
    {
        elemental_debug_emit_size_summary(attack_type, raw_dam, min_raw,
            max_raw, hp_dam, cdf, q_squared, hurt, chance, 0, 0,
            "no trigger");
        return;
    }

    if (rand_int(1000000) >= threshold)
    {
        msg_print("Luckily, nothing was damaged.");
        elemental_debug_emit_size_summary(attack_type, raw_dam, min_raw,
            max_raw, hp_dam, cdf, q_squared, hurt, chance, 0, 0,
            "hurt roll failed");
        return;
    }

    {
        object_type* shield = elemental_equipped_shield();

        if (shield)
        {
            int block_chance = elemental_shield_block_chance(shield,
                attack_type);

            if (block_chance > 0)
            {
                if (rand_int(100) < block_chance)
                {
                    msg_print("Your shield blocked the attack.");
                    if (!elemental_shield_has_attack_protection(shield,
                            attack_type)
                        && elemental_damage_blocking_shield(shield))
                    {
                        msg_print("Your shield lost one side of protection.");
                    }

                    elemental_debug_emit_size_summary(attack_type, raw_dam,
                        min_raw, max_raw, hp_dam, cdf, q_squared, hurt, chance,
                        0, 0, "shield blocked");
                    return;
                }

                msg_print("Your shield could not block the attack.");
            }
        }
    }

    total = elemental_damage_total(attack_type, hp_dam);
    total_budget = total;
    if (total <= 0)
    {
        elemental_debug_emit_size_summary(attack_type, raw_dam, min_raw,
            max_raw, hp_dam, cdf, q_squared, hurt, chance, 0, 0, "no total");
        return;
    }

    while (total > 0)
    {
        elemental_item_candidate candidate;
        int amount;
        char o_name[80];

        if (!elemental_select_size_candidate(attack_type, total, &candidate,
                NULL, NULL, NULL))
        {
            break;
        }

        total -= candidate.unit_size;
        amount = candidate.quantity_per_unit;
        if (candidate.o_ptr && (candidate.o_ptr->number > 0))
            amount = MIN(amount, candidate.o_ptr->number);

        elemental_describe_quantity(o_name, sizeof(o_name), &candidate, amount);

        if (elemental_object_has_attack_resistance(candidate.o_ptr, attack_type))
        {
            elemental_message_amount(&candidate, candidate.o_ptr->number, amount,
                o_name, "resisted the attack.", "resisted the attack.");
            resisted++;
            resisted_size += candidate.unit_size;
            continue;
        }

        if ((attack_type == GF_ACID) && acid_can_corrode_object(candidate.o_ptr)
            && elemental_acid_roll_reduces(NULL))
        {
            cptr acid_outcome = elemental_corrode_candidate(&candidate, NULL);

            if (streq(acid_outcome, "was destroyed!"))
            {
                destroyed++;
                destroyed_size += candidate.unit_size;
            }
            else
            {
                reduced++;
                reduced_size += candidate.unit_size;
            }

            continue;
        }

        elemental_destroy_candidate_quantity(&candidate, attack_type, amount);
        destroyed++;
        destroyed_size += candidate.unit_size;
    }

    strnfmt(outcome, sizeof(outcome),
        "destroyed=%d(size=%d) reduced=%d(size=%d) resisted=%d(size=%d)",
        destroyed, destroyed_size, reduced, reduced_size, resisted,
        resisted_size);
    elemental_debug_emit_size_summary(attack_type, raw_dam, min_raw, max_raw,
        hp_dam, cdf, q_squared, hurt, chance, total_budget, total, outcome);
}

static bool elemental_slot_uses_pack_like_factor(int slot,
    elemental_item_candidate_location location)
{
    if (location == ELEMENTAL_CANDIDATE_SUPPLY)
        return true;

    return (slot < INVEN_PACK) || (slot == INVEN_LITE)
        || (slot == INVEN_QUIVER1) || (slot == INVEN_QUIVER2);
}

static double elemental_item_slot_factor(int slot,
    elemental_item_candidate_location location)
{
    if (location == ELEMENTAL_CANDIDATE_SUPPLY)
        return 0.70;

    if (slot < INVEN_PACK)
        return 0.70;

    switch (slot)
    {
    case INVEN_WIELD:
        return 1.40;
    case INVEN_BOW:
        return 1.20;
    case INVEN_ARM:
        return 1.10;
    case INVEN_BODY:
        return 1.00;
    case INVEN_OUTER:
        return 1.15;
    case INVEN_HANDS:
        return 0.90;
    case INVEN_FEET:
        return 0.85;
    case INVEN_HEAD:
        return 0.85;
    case INVEN_LITE:
    case INVEN_QUIVER1:
    case INVEN_QUIVER2:
        return 0.70;
    default:
        return 1.00;
    }
}

static double elemental_item_material_factor(int attack_type,
    const object_type* o_ptr)
{
    switch (attack_type)
    {
    case GF_FIRE:
        if ((o_ptr->tval == TV_ARROW) || (o_ptr->tval == TV_HAFTED)
            || (o_ptr->tval == TV_POLEARM))
        {
            return 1.20;
        }

        if ((o_ptr->tval == TV_BOW) || (o_ptr->tval == TV_STAFF))
            return 1.60;

        if ((o_ptr->tval == TV_CLOAK) || (o_ptr->tval == TV_BOOTS)
            || (o_ptr->tval == TV_GLOVES)
            || ((o_ptr->tval == TV_SOFT_ARMOR) && (o_ptr->sval == SV_ROBE)))
        {
            return 1.40;
        }

        return 1.00;

    case GF_COLD:
    case GF_SOUND:
        if ((o_ptr->tval == TV_POTION) || (o_ptr->tval == TV_GEM)
            || (o_ptr->tval == TV_FLASK))
        {
            return 1.50;
        }

        if ((o_ptr->tval == TV_LIGHT) && (o_ptr->sval == SV_LIGHT_LANTERN))
            return 1.20;

        return 1.00;

    case GF_ACID:
        if ((o_ptr->tval == TV_MAIL) || (o_ptr->tval == TV_SHIELD)
            || (o_ptr->tval == TV_HELM) || (o_ptr->tval == TV_CROWN))
        {
            return 1.40;
        }

        if ((o_ptr->tval == TV_ARROW) || (o_ptr->tval == TV_BOW)
            || (o_ptr->tval == TV_SWORD) || (o_ptr->tval == TV_HAFTED)
            || (o_ptr->tval == TV_POLEARM))
        {
            return 1.10;
        }

        return 1.00;

    default:
        return 1.00;
    }
}

static bool elemental_attack_allows_candidate_location(int attack_type,
    elemental_item_candidate_location location, int index)
{
    switch (attack_type)
    {
    case GF_FIRE:
    case GF_ACID:
    case GF_ELEC:
        return (location == ELEMENTAL_CANDIDATE_INVENTORY)
            && (index >= INVEN_WIELD) && (index < INVEN_TOTAL);

    case GF_COLD:
        return location == ELEMENTAL_CANDIDATE_SUPPLY;

    case GF_SOUND:
        return true;

    default:
        return false;
    }
}

static long elemental_item_weight(int attack_type,
    elemental_item_candidate_location location, int index,
    const object_type* o_ptr)
{
    double slot_factor;
    double material_factor;
    double stack_factor = 1.0;
    long scaled;

    if (!elemental_attack_can_target_object(attack_type, o_ptr))
        return 0;

    if (!elemental_attack_allows_candidate_location(attack_type, location, index))
        return 0;

    slot_factor = elemental_item_slot_factor(index, location);
    material_factor = elemental_item_material_factor(attack_type, o_ptr);

    if (elemental_slot_uses_pack_like_factor(index, location))
    {
        stack_factor = sqrt((double)MAX(o_ptr->number, 1));
    }

    scaled = (long)((slot_factor * material_factor * stack_factor * 1000.0)
        + 0.5);

    if (scaled < 1)
        scaled = 1;

    return scaled;
}

static bool elemental_select_candidate(int attack_type,
    elemental_item_candidate* out, elemental_item_debug_info* debug)
{
    int supply_count = supplies_entry_count();
    int capacity = INVEN_TOTAL + supply_count;
    elemental_item_candidate* candidates;
    int count = 0;
    long total_weight = 0;
    int pick;
    int selection_roll;

    if (!out || (capacity <= 0))
        return false;

    candidates = mem_alloc_array(capacity, elemental_item_candidate);

    for (int slot = 0; slot < INVEN_TOTAL; slot++)
    {
        object_type* o_ptr = &inventory[slot];
        long weight;

        if (!o_ptr->k_idx)
            continue;

        weight = elemental_item_weight(attack_type,
            ELEMENTAL_CANDIDATE_INVENTORY, slot, o_ptr);
        if (weight <= 0)
            continue;

        candidates[count].location = ELEMENTAL_CANDIDATE_INVENTORY;
        candidates[count].index = slot;
        candidates[count].o_ptr = o_ptr;
        candidates[count].weight = weight;
        total_weight += weight;
        count++;
    }

    for (int idx = 0; idx < supply_count; idx++)
    {
        object_type* o_ptr = supplies_entry_at(idx);
        long weight;

        if (!o_ptr || !o_ptr->k_idx)
            continue;

        weight = elemental_item_weight(attack_type,
            ELEMENTAL_CANDIDATE_SUPPLY, idx, o_ptr);
        if (weight <= 0)
            continue;

        candidates[count].location = ELEMENTAL_CANDIDATE_SUPPLY;
        candidates[count].index = idx;
        candidates[count].o_ptr = o_ptr;
        candidates[count].weight = weight;
        total_weight += weight;
        count++;
    }

    if ((count <= 0) || (total_weight <= 0))
    {
        if (debug)
        {
            debug->candidate_count = count;
            debug->total_weight = total_weight;
        }
        mem_free(candidates);
        return false;
    }

    pick = rand_int((int)total_weight);
    selection_roll = pick;
    for (int i = 0; i < count; i++)
    {
        if (pick < candidates[i].weight)
        {
            *out = candidates[i];
            elemental_debug_record_candidate(debug, attack_type, out,
                selection_roll, count, total_weight);
            mem_free(candidates);
            return true;
        }

        pick -= (int)candidates[i].weight;
    }

    *out = candidates[count - 1];
    elemental_debug_record_candidate(debug, attack_type, out, selection_roll,
        count, total_weight);
    mem_free(candidates);
    return true;
}

static void elemental_message(const elemental_item_candidate* candidate,
    int original_number, cptr o_name, cptr action)
{
    cptr owner = (original_number > 1) ? "One of your" : "Your";

    if (candidate->location == ELEMENTAL_CANDIDATE_SUPPLY)
    {
        msg_format("%s %s in your supplies %s", owner, o_name, action);
    }
    else if (candidate->index < INVEN_PACK)
    {
        msg_format("%s %s (%c) %s", owner, o_name,
            index_to_label(candidate->index), action);
    }
    else
    {
        msg_format("%s %s %s", owner, o_name, action);
    }
}

static void elemental_mark_inventory_item_changed(void)
{
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);
    p_ptr->update |= (PU_BONUS | PU_MANA);
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
}

static void elemental_prepare_split_item(const object_type* src,
    object_type* split)
{
    object_copy(split, src);
    split->number = 1;
    split->pickup = false;
    split->pickup_slot = -1;
}

static void elemental_remove_one_from_candidate(
    const elemental_item_candidate* candidate)
{
    object_type* o_ptr = candidate->o_ptr;

    if (candidate->location == ELEMENTAL_CANDIDATE_SUPPLY)
    {
        (void)supplies_consume_quantity(candidate->index, 1);
        return;
    }

    if (((o_ptr->tval == TV_STAFF) || (o_ptr->tval == TV_HORN))
        && (o_ptr->number > 1))
    {
        o_ptr->pval -= o_ptr->pval / o_ptr->number;
    }

    inven_item_increase(candidate->index, -1);
    inven_item_optimize(candidate->index);
}

static void elemental_reinsert_split_item(
    const elemental_item_candidate* candidate, object_type* split)
{
    if (candidate->location == ELEMENTAL_CANDIDATE_SUPPLY)
    {
        if (!supplies_absorb_object(split))
            drop_near(split, 0, p_ptr->py, p_ptr->px);
        return;
    }

    if (inven_carry(split, false) < 0)
        drop_near(split, 0, p_ptr->py, p_ptr->px);
}

static bool acid_can_corrode_object(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
        return true;

    default:
        return false;
    }
}

static bool acid_corrode_object(object_type* o_ptr)
{
    if ((o_ptr->ps <= 0) && (o_ptr->evn <= 0))
        return true;

    if (o_ptr->evn >= 0)
        o_ptr->evn--;
    else
        o_ptr->ps--;

    return false;
}

static cptr elemental_corrode_candidate(const elemental_item_candidate* candidate,
    elemental_item_debug_info* debug)
{
    object_type* source = candidate->o_ptr;
    object_type split;
    int original_number = source->number;
    char o_name[80];

    object_desc(o_name, sizeof(o_name), source, false, 3);

    if (original_number > 1)
    {
        elemental_prepare_split_item(source, &split);
        elemental_remove_one_from_candidate(candidate);

        if (acid_corrode_object(&split))
        {
            if (debug)
            {
                debug->outcome = "was destroyed!";
                elemental_debug_emit(debug);
            }
            elemental_message(candidate, original_number, o_name,
                "was destroyed!");
            return "was destroyed!";
        }

        elemental_reinsert_split_item(candidate, &split);
        if (debug)
        {
            debug->outcome = "was damaged!";
            elemental_debug_emit(debug);
        }
        elemental_message(candidate, original_number, o_name, "was damaged!");
        return "was damaged!";
    }

    if (acid_corrode_object(source))
    {
        elemental_remove_one_from_candidate(candidate);
        if (debug)
        {
            debug->outcome = "was destroyed!";
            elemental_debug_emit(debug);
        }
        elemental_message(candidate, original_number, o_name, "was destroyed!");
        return "was destroyed!";
    }

    if (candidate->location == ELEMENTAL_CANDIDATE_SUPPLY)
        supplies_refresh_entry(candidate->index);
    else
        elemental_mark_inventory_item_changed();

    if (debug)
    {
        debug->outcome = "was damaged!";
        elemental_debug_emit(debug);
    }
    elemental_message(candidate, original_number, o_name, "was damaged!");
    return "was damaged!";
}

static cptr elemental_destroy_candidate(
    const elemental_item_candidate* candidate, int attack_type,
    elemental_item_debug_info* debug)
{
    object_type* source = candidate->o_ptr;
    object_type split;
    object_type* target = source;
    int original_number = source->number;
    char o_name[80];

    object_desc(o_name, sizeof(o_name), source, false, 3);

    if (original_number > 1)
    {
        elemental_prepare_split_item(source, &split);
        target = &split;
    }

    if ((target->tval == TV_CHEST) && (attack_type != GF_SOUND))
    {
        chest_release_contents(target, p_ptr->py, p_ptr->px, attack_type);
        elemental_remove_one_from_candidate(candidate);
        if (debug)
        {
            debug->outcome = "was destroyed!";
            elemental_debug_emit(debug);
        }
        elemental_message(candidate, original_number, o_name,
            "was destroyed!");
        return "was destroyed!";
    }

    if ((attack_type == GF_FIRE) && object_break_shafted_weapon_by_fire(target))
    {
        if (original_number > 1)
        {
            elemental_remove_one_from_candidate(candidate);
            elemental_reinsert_split_item(candidate, target);
        }
        else if (candidate->location == ELEMENTAL_CANDIDATE_SUPPLY)
        {
            supplies_refresh_entry(candidate->index);
        }
        else
        {
            elemental_mark_inventory_item_changed();
        }

        if (debug)
        {
            debug->outcome = "was broken!";
            elemental_debug_emit(debug);
        }
        elemental_message(candidate, original_number, o_name, "was broken!");
        return "was broken!";
    }

    if (((attack_type == GF_COLD) || (attack_type == GF_SOUND))
        && (target->tval == TV_LIGHT) && (target->sval == SV_LIGHT_LANTERN)
        && object_break_brass_lantern(target))
    {
        if (original_number > 1)
        {
            elemental_remove_one_from_candidate(candidate);
            elemental_reinsert_split_item(candidate, target);
        }
        else if (candidate->location == ELEMENTAL_CANDIDATE_SUPPLY)
        {
            supplies_refresh_entry(candidate->index);
        }
        else
        {
            elemental_mark_inventory_item_changed();
        }

        if (debug)
        {
            debug->outcome = "was broken!";
            elemental_debug_emit(debug);
        }
        elemental_message(candidate, original_number, o_name, "was broken!");
        return "was broken!";
    }

    elemental_remove_one_from_candidate(candidate);
    if (debug)
    {
        debug->outcome = "was destroyed!";
        elemental_debug_emit(debug);
    }
    elemental_message(candidate, original_number, o_name, "was destroyed!");
    return "was destroyed!";
}

static void elemental_attack_affect_one_item(int attack_type, int raw_dam,
    int min_raw, int max_raw, int hp_dam)
{
    elemental_item_candidate candidate;
    elemental_item_debug_info debug;
    int gate_roll;

    elemental_debug_init(&debug, attack_type, raw_dam, min_raw, max_raw, hp_dam);
    int threshold = elemental_attack_probability_per_million(attack_type,
        raw_dam, min_raw, max_raw, hp_dam, debug.enabled ? &debug : NULL);

    if (threshold <= 0)
    {
        if (debug.enabled)
        {
            debug.outcome = "no trigger chance";
            elemental_debug_emit(&debug);
        }
        return;
    }

    gate_roll = rand_int(1000000);
    if (debug.enabled)
    {
        debug.gate_roll_made = true;
        debug.gate_roll = gate_roll;
    }

    if (gate_roll >= threshold)
    {
        if (debug.enabled)
        {
            debug.outcome = "no item";
            elemental_debug_emit(&debug);
        }
        return;
    }

    if (!elemental_select_candidate(attack_type, &candidate,
        debug.enabled ? &debug : NULL))
    {
        if (debug.enabled)
        {
            debug.outcome = "no eligible target";
            elemental_debug_emit(&debug);
        }
        return;
    }

    if ((attack_type == GF_ACID) && acid_can_corrode_object(candidate.o_ptr)
        )
    {
        int acid_roll;
        bool reduce = elemental_acid_roll_reduces(&acid_roll);

        if (debug.enabled)
        {
            debug.acid_roll_made = true;
            debug.acid_roll = acid_roll;
        }

        if (reduce)
        {
            (void)elemental_corrode_candidate(&candidate,
                debug.enabled ? &debug : NULL);
            return;
        }
    }

    (void)elemental_destroy_candidate(&candidate, attack_type,
        debug.enabled ? &debug : NULL);
}

void sound_dam(int raw_dam, int min_raw, int max_raw, int hp_dam)
{
    if (raw_dam <= 0)
        return;

    elemental_attack_affect_one_item(GF_SOUND, raw_dam, min_raw, max_raw,
        hp_dam);
}

/*
 * Hurt the player with Acid
 */
void acid_dam(int raw_dam, int min_raw, int max_raw, int hp_dam, cptr kb_str)
{
    /* Abort if no damage to receive */
    if (hp_dam <= 0)
        return;

    /* Take damage */
    take_hit(hp_dam, kb_str);

    /* Elemental item damage */
    elemental_attack_affect_multiple_items(GF_ACID, raw_dam, min_raw, max_raw,
        hp_dam);
}

/*
 * Hurt the player with electricity
 */
void elec_dam(int raw_dam, int min_raw, int max_raw, int hp_dam, cptr kb_str)
{
    /* Abort if no damage to receive */
    if (hp_dam <= 0)
        return;

    /* Take damage */
    take_hit(hp_dam, kb_str);

    /* Elemental item damage */
    elemental_attack_affect_multiple_items(GF_ELEC, raw_dam, min_raw, max_raw,
        hp_dam);
}

/*
 * The player's fire resistance depends on equipment and temporary effects
 */
extern int resist_fire(void)
{
    int res = p_ptr->resist_fire;

    if (p_ptr->oppose_fire)
        res++;

    // represent overall vulnerabilities as negatives of the normal range
    if (res < 1)
        res -= 2;

    return (res);
}

/*
 * The player's cold resistance depends on equipment and temporary effects
 */
extern int resist_cold(void)
{
    int res = p_ptr->resist_cold;

    if (p_ptr->oppose_cold)
        res++;

    // represent overall vulnerabilities as negatives of the normal range
    if (res < 1)
        res -= 2;

    return (res);
}

/*
 * The player's poison resistance depends on equipment and temporary effects
 */
extern int resist_pois(void)
{
    int res = p_ptr->resist_pois;

    if (p_ptr->oppose_pois)
        res++;

    // represent overall vulnerabilities as negatives of the normal range
    if (res < 1)
        res -= 2;

    return (res);
}

/*
 * The player's dark resistance is strictly dependent
 * on the brightness of their square
 */
extern int resist_dark(void)
{
    int res = cave_light[p_ptr->py][p_ptr->px];

    if (res < 1)
        res = 1;

    return (res);
}

static int elemental_resisted_damage(int dam, int resistance)
{
    if (resistance > 0)
    {
        int stacks = resistance - 1;
        return (dam * 2) / (2 + stacks);
    }

    return (dam * (-resistance));
}

static void log_elemental_damage_context(const char* tag, cptr kb_str, int dam,
    int prt, int resistance, int net_dam)
{
    bool should_log = level_partition_big_cave_type_for_point(p_ptr->py, p_ptr->px)
        != BIG_CAVE_NONE;

    if (!should_log)
    {
        should_log = (cave_info[p_ptr->py][p_ptr->px]
            & (CAVE_G_VAULT | CAVE_MORGOTH_TUNNEL)) != 0;
    }

    if (!should_log)
        return;

    log_partition_debug_for_point(tag, p_ptr->py, p_ptr->px);
    log_debug(
        "%s: killer=%s raw=%d prt=%d net=%d base_fire=%d base_cold=%d base_pois=%d oppose_fire=%d oppose_cold=%d oppose_pois=%d effective_resistance=%d",
        tag, kb_str ? kb_str : "(none)", dam, prt, net_dam,
        p_ptr->resist_fire, p_ptr->resist_cold, p_ptr->resist_pois,
        p_ptr->oppose_fire, p_ptr->oppose_cold, p_ptr->oppose_pois,
        resistance);
}

/*
 * Hurt the player with Fire
 */
void fire_dam_mixed(int raw_dam, int min_raw, int max_raw, int hp_dam,
    cptr kb_str)
{
    /* Abort if no damage to receive */
    if (hp_dam <= 0)
        return;

    /* Take damage */
    take_hit(hp_dam, kb_str);

    /* Elemental item damage */
    elemental_attack_affect_multiple_items(GF_FIRE, raw_dam, min_raw, max_raw,
        hp_dam);

    // possibly identify relevant items
    ident_resist(TR2_RES_FIRE);
}

/*
 * Hurt the player with Fire
 */
void fire_dam_pure(int dd, int ds, bool update_rolls, cptr kb_str)
{
    int dam = damroll(dd, ds);
    int net_dam;
    int prt = protection_roll(GF_FIRE, false);
    int resistance = resist_fire();

    net_dam = elemental_resisted_damage(dam, resistance);
    net_dam = net_dam > prt ? net_dam - prt : 0;

    if (update_rolls)
    {
        update_combat_rolls2(dd, ds, dam, -1, -1, prt, 100, GF_FIRE, false);
    }

    log_elemental_damage_context("fire_dam_pure", kb_str, dam, prt,
        resistance, net_dam);

    /* Abort if no damage to receive */
    if (net_dam <= 0)
        return;

    /* Take damage */
    take_hit(net_dam, kb_str);

    /* Elemental item damage */
    elemental_attack_affect_multiple_items(GF_FIRE, dam, dd, dd * ds,
        net_dam);

    // possibly identify relevant items
    ident_resist(TR2_RES_FIRE);
}

/*
 * Hurt the player with Cold
 */
void cold_dam_mixed(int raw_dam, int min_raw, int max_raw, int hp_dam,
    cptr kb_str)
{
    /* Abort if no damage to receive */
    if (hp_dam <= 0)
        return;

    /* Take damage */
    take_hit(hp_dam, kb_str);

    /* Elemental item damage */
    elemental_attack_affect_multiple_items(GF_COLD, raw_dam, min_raw, max_raw,
        hp_dam);

    // possibly identify relevant items
    ident_resist(TR2_RES_COLD);
}

/*
 * Hurt the player with Cold
 */
void cold_dam_pure(int dd, int ds, bool update_rolls, cptr kb_str)
{
    int dam = damroll(dd, ds);
    int net_dam;
    int prt = protection_roll(GF_COLD, false);
    int resistance = resist_cold();

    net_dam = elemental_resisted_damage(dam, resistance);
    net_dam = net_dam > prt ? net_dam - prt : 0;

    if (update_rolls)
    {
        update_combat_rolls2(dd, ds, dam, -1, -1, prt, 100, GF_COLD, false);
    }

    log_elemental_damage_context("cold_dam_pure", kb_str, dam, prt,
        resistance, net_dam);

    /* Abort if no damage to receive */
    if (net_dam <= 0)
        return;

    /* Take damage */
    take_hit(net_dam, kb_str);

    /* Elemental item damage */
    elemental_attack_affect_multiple_items(GF_COLD, dam, dd, dd * ds,
        net_dam);

    // possibly identify relevant items
    ident_resist(TR2_RES_COLD);
}

/*
 * Hurt the player with Darkness from melee
 */
void dark_dam_mixed(int dam, cptr kb_str)
{
    /* Abort if no damage to receive */
    if (dam <= 0)
        return;

    /* Take damage */
    take_hit(dam, kb_str);
}

/*
 * Hurt the player with Darkness from breaths
 */
void dark_dam_pure(int dd, int ds, bool update_rolls, cptr kb_str)
{
    int dam = damroll(dd, ds);
    int net_dam;
    int prt = protection_roll(GF_DARK, false);
    int resistance = resist_dark();

    net_dam = dam / resistance;
    net_dam = net_dam > prt ? net_dam - prt : 0;

    if (update_rolls)
    {
        update_combat_rolls2(dd, ds, dam, -1, -1, prt, 100, GF_DARK, false);
    }

    // 'pure' darkness attacks can also blind
    if (one_in_(resistance) && allow_player_blind(NULL))
    {
        (void)set_blind(p_ptr->blind + damroll(2, 4));
    }

    /* Abort if no damage to receive */
    if (net_dam <= 0)
        return;

    /* Take damage */
    take_hit(net_dam, kb_str);
}

/*
 * Poison the player from melee
 */
void pois_dam_mixed(int dam)
{
    /* Abort if no damage to receive */
    if (dam <= 0)
        return;

    /* Set poison counter */
    set_poisoned(p_ptr->poisoned + dam);

    // possibly identify relevant items
    ident_resist(TR2_RES_POIS);
}

/*
 * Poison the player from breaths etc
 */
void pois_dam_pure(int dd, int ds, bool update_rolls)
{
    int dam = damroll(dd, ds);
    int net_dam;
    int prt = protection_roll(GF_POIS, false);
    int resistance = resist_pois();

    net_dam = elemental_resisted_damage(dam, resistance);
    net_dam = net_dam > prt ? net_dam - prt : 0;

    if (update_rolls)
    {
        update_combat_rolls2(dd, ds, dam, -1, -1, prt, 100, GF_POIS, false);
    }

    log_elemental_damage_context("pois_dam_pure", "poison", dam, prt,
        resistance, net_dam);

    /* Abort if no damage to receive */
    if (net_dam <= 0)
        return;

    /* Set poison counter */
    set_poisoned(p_ptr->poisoned + net_dam);

    // possibly identify relevant items
    ident_resist(TR2_RES_POIS);
}

/*
 * Increase a stat by one randomized level
 *
 * Most code will "restore" a stat before calling this function,
 * in particular, stat potions will always restore the stat and
 * then increase the fully restored value.
 */
bool inc_stat(int stat)
{
    /* Cannot go above BASE_STAT_MAX */
    if (p_ptr->stat_base[stat] < BASE_STAT_MAX)
    {
        p_ptr->stat_base[stat]++;

        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Redisplay the stats later */
        p_ptr->redraw |= (PR_STATS);

        /* Success */
        return (true);
    }

    /* Nothing to gain */
    return (false);
}

/*
 * Decreases a stat by a number of points.
 *
 * Note that "permanent" means that the *given* amount is permanent,
 * not that the new value becomes permanent.
 */
bool dec_stat(int stat, int amount, bool permanent)
{
    int result = false;

    /* Temporary damage */
    if (!permanent)
    {
        p_ptr->stat_drain[stat] -= amount;
        result = true;
    }

    /* Permanent damage */
    if (permanent && (p_ptr->stat_base[stat] > 0))
    {
        if (amount > p_ptr->stat_base[stat])
            p_ptr->stat_base[stat] = 0;
        else
            p_ptr->stat_base[stat] -= amount;

        result = true;
    }

    /* Apply changes */
    if (result)
    {
        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Redisplay the stats later */
        p_ptr->redraw |= (PR_STATS);
    }

    /* Done */
    return (result);
}

/*
 * Restore a stat by the number of points.
 * Return true only if this actually makes a difference.
 */
bool res_stat(int stat, int points)
{
    /* Restore if needed */
    if (p_ptr->stat_drain[stat] < 0)
    {
        /* Restore */
        p_ptr->stat_drain[stat] += points;

        if (p_ptr->stat_drain[stat] > 0)
            p_ptr->stat_drain[stat] = 0;

        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Redisplay the stats later */
        p_ptr->redraw |= (PR_STATS);

        /* Success */
        return (true);
    }

    /* Nothing to restore */
    return (false);
}

/*
 * Inflict disease on the character.
 */
void disease(int* damage)
{
    int con, attempts;
    int i;

    /* Get current constitution */
    con = p_ptr->stat_use[A_CON];

    /* Adjust damage and choose message based on constitution */
    if (con < -2)
    {
        msg_print("You feel deathly ill.");
        *damage *= 2;
    }

    else if (con < 0)
    {
        msg_print("You feel seriously ill.");
    }

    else if (con < 2)
    {
        msg_print("You feel quite ill.");
        *damage = *damage * 2 / 3;
    }

    else if (con < 5)
    {
        msg_print("You feel ill.");
        *damage /= 2;
    }

    else if (con < 7)
    {
        msg_print("You feel sick.");
        *damage /= 3;
    }

    else
    {
        msg_print("You feel a bit sick.");
        *damage /= 4;
    }

    /* Infect the character (fully cumulative) */
    set_poisoned(p_ptr->poisoned + *damage + 1);

    /* Determine # of stat-reduction attempts */
    attempts = (5 + *damage) / 5;

    /* Attack stats */
    for (i = 0; i < attempts; i++)
    {
        /* Each attempt has a 10% chance of success */
        if (one_in_(10))
        {
            /* Damage a random stat */
            (void)do_dec_stat(rand_int(A_MAX), NULL);
        }
    }
}

/*
 * Apply disenchantment to the player's stuff
 *
 * This function is also called from the "melee" code.
 *
 * The "mode" is currently unused.
 *
 * Return "true" if the player notices anything.
 *
 * Sil-y: this presently brings att, evn, dd, ds, pd, ds down towards their base
 * values by one point each
 */
bool apply_disenchant(int mode)
{
    int t = 0;

    object_type* o_ptr;

    object_kind* k_ptr;

    char o_name[80];

    /* Unused parameter */
    (void)mode;

    /* Pick a random slot */
    switch (dieroll(8))
    {
    case 1:
        t = INVEN_WIELD;
        break;
    case 2:
        t = INVEN_BOW;
        break;
    case 3:
        t = INVEN_BODY;
        break;
    case 4:
        t = INVEN_OUTER;
        break;
    case 5:
        t = INVEN_ARM;
        break;
    case 6:
        t = INVEN_HEAD;
        break;
    case 7:
        t = INVEN_HANDS;
        break;
    case 8:
        t = INVEN_FEET;
        break;
    }

    /* Get the item */
    o_ptr = &inventory[t];

    k_ptr = &k_info[o_ptr->k_idx];

    /* No item, nothing happens */
    if (!o_ptr->k_idx)
        return (false);

    /* Check to see if it is disenchantable */

    /* Describe the object */
    object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

    /* Artefacts have 60% chance to resist */
    if (artefact_p(o_ptr) && percent_chance(60))
    {
        /* Message */
        msg_format("Your %s (%c) resist%s disenchantment!", o_name,
            index_to_label(t), ((o_ptr->number != 1) ? "" : "s"));

        /* Notice */
        return (true);
    }

    /* Do the disenchanting */
    if (o_ptr->att > k_ptr->att)
        o_ptr->att--;
    if (o_ptr->evn > k_ptr->evn)
        o_ptr->evn--;
    if (o_ptr->ds > k_ptr->ds)
        o_ptr->ds--;
    if (o_ptr->dd > k_ptr->dd)
        o_ptr->dd--;
    if (o_ptr->ps > k_ptr->ps)
        o_ptr->ps--;
    if (o_ptr->pd > k_ptr->pd)
        o_ptr->pd--;

    msg_format("Your %s (%c) %s disenchanted!", o_name, index_to_label(t),
        ((o_ptr->number != 1) ? "were" : "was"));

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Window stuff */
    p_ptr->window |= (PW_EQUIP | PW_PLAYER_0);

    /* Notice */
    return (true);
}

