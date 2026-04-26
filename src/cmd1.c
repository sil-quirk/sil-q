/* File: cmd1.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include <math.h>

static bool valorous_oath_blocks_auto_attack(monster_type* m_ptr);
static bool queue_deferred_pickup_pack_drop(int item, int amount,
    bool refill_oil_pool);

static bool weapon_has_attack_confirmation_inscription(const object_type* o_ptr)
{
    cptr s;

    if (!o_ptr || !o_ptr->obj_note)
        return false;

    s = strchr(quark_str(o_ptr->obj_note), '!');
    while (s)
    {
        if (s[1] == 'a')
            return true;

        s = strchr(s + 1, '!');
    }

    return false;
}

static bool polearm_is_axe(const object_type* weapon)
{
    if (!weapon)
        return false;

    if (weapon->tval != TV_POLEARM)
        return false;

    switch (weapon->sval)
    {
    case SV_BATTLE_AXE:
    case SV_GREAT_AXE:
        return true;
    default:
        return false;
    }
}

static bool sword_is_medium(const object_type* weapon)
{
    if (!weapon || weapon->tval != TV_SWORD)
        return false;

    switch (weapon->sval)
    {
    case SV_LONG_SWORD:
    case SV_BASTARD_SWORD:
        return true;
    default:
        return false;
    }
}

static bool sword_is_great(const object_type* weapon)
{
    if (!weapon || weapon->tval != TV_SWORD)
        return false;

    switch (weapon->sval)
    {
    case SV_GREAT_SWORD:
    case SV_STAR_IRON_GREAT_SWORD:
        return true;
    default:
        return false;
    }
}

static u16b weapon_sound_message_type(const object_type* weapon, bool hit)
{
    u16b fallback = hit ? MSG_HIT : MSG_MISS;

    if (!weapon || weapon->k_idx == 0)
        return MSG_WEAPON_UNARMED;

    if (weapon->weight == 0)
        return MSG_WEAPON_UNARMED;

    switch (weapon->tval)
    {
    case TV_SWORD:
        if (sword_is_great(weapon))
            return MSG_WEAPON_SLASH_HEAVY;
        else if (sword_is_medium(weapon))
            return MSG_WEAPON_SLASH_MEDIUM;
        else
            return MSG_WEAPON_SLASH_LIGHT;
    case TV_POLEARM:
        if (weapon->sval == SV_HAND_AXE)
            return MSG_WEAPON_SLASH_LIGHT;
        else if (weapon->sval == SV_BATTLE_AXE)
            return MSG_WEAPON_SLASH_MEDIUM;
        else if (polearm_is_axe(weapon))
            return MSG_WEAPON_SLASH_HEAVY;
        else
            return MSG_WEAPON_THRUST;
    case TV_HAFTED:
    case TV_DIGGING:
    case TV_STAFF:
    case TV_LIGHT:
    case TV_HORN:
        return MSG_WEAPON_BLUNT;
    default:
        break;
    }

    return fallback;
}

static int weapon_animation_delay(u16b weapon_sound_type)
{
    switch (weapon_sound_type)
    {
    case MSG_WEAPON_SLASH_LIGHT:
    case MSG_WEAPON_UNARMED:
        return 300;
    case MSG_WEAPON_SLASH_MEDIUM:
    case MSG_WEAPON_THRUST:
        return 350;
    case MSG_WEAPON_SLASH_HEAVY:
    case MSG_WEAPON_BLUNT:
        return 400;
    default:
        return 350; // fallback to medium
    }
}

bool graphics_are_ascii()
{
    return use_graphics == GRAPHICS_NONE || use_graphics == GRAPHICS_PSEUDO;
}

/*
 * Puts an item in the player's inventory.
 * If the inventory would overflow, this is handled at the start of the next
 * player turn.
 */
static void strip_brass_lantern_turns_suffix(char* o_name, const object_type* o_ptr)
{
    char* fuel_suffix;

    if (!o_name || !o_ptr)
        return;

    if (o_ptr->tval != TV_LIGHT || o_ptr->sval != SV_LIGHT_LANTERN)
        return;

    fuel_suffix = strstr(o_name, " (");
    if (fuel_suffix && strstr(fuel_suffix, " turns)"))
        *fuel_suffix = '\0';
}

void give_player_item(object_type * o_ptr)
{
    char o_name[80];
    object_type copy = *o_ptr;

    int slot = inven_carry(o_ptr, true);

    if (slot == SUPPLIES_INDEX)
    {
        object_desc(o_name, sizeof(o_name), &copy, true, 3);
        strip_brass_lantern_turns_suffix(o_name, &copy);
        char label = supplies_label_char();
        if (!label)
            label = 'a';
        msg_format("You add %s to your supplies (%c).", o_name, label);
        sound(MSG_PICK);
        return;
    }

    if (slot < 0)
        return;
    
    /* Play pickup sound */
    sound(MSG_PICK);

    /* reset the pointer to the new location to pick up the count of the item
       in the inventory */
    o_ptr = &inventory[slot];

    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    msg_format("You have %s (%c).", o_name, index_to_label(slot));

    /* Update quiver display if this was a throwing weapon or arrow */
    if ((slot == INVEN_QUIVER1 || slot == INVEN_QUIVER2) ||
        (copy.tval == TV_ARROW))
    {
        p_ptr->redraw |= (PR_QUIVER);
    }
}

void new_wandering_flow(monster_type* m_ptr, int ty, int tx)
{
    int y, x, i;
    int wandering_idx = m_ptr->wandering_idx;
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    if (wandering_idx < FLOW_WANDERING_HEAD)
    {
        return;
    }

    // territorial monsters target their creation location
    // same with the tutorial
    if ((r_ptr->flags2 & (RF2_TERRITORIAL)) || (p_ptr->game_type < 0))
    {
        // they only pick a new location on creation
        // Sil-y: Hack: using the fact that speed hasn't been determined yet on
        // creation
        if (m_ptr->mspeed == 0)
        {
            // update the flow
            update_flow(m_ptr->fy, m_ptr->fx, wandering_idx);
        }
    }

    // if a location was requested, use that
    else if (in_bounds_fully(ty, tx))
    {
        y = ty;
        x = tx;

        // update the flow
        update_flow(y, x, wandering_idx);
    }

    // otherwise choose a location
    else
    {
        // sometimes intelligent monsters want to pick a staircase and leave the
        // level
        if ((r_ptr->flags2 & (RF2_SMART))
            && !(r_ptr->flags2 & (RF2_TERRITORIAL))
            && (p_ptr->depth != MORGOTH_DEPTH) && one_in_(5)
            && random_stair_location(&y, &x) && (cave_m_idx[y][x] >= 0)
            && !(cave_info[y][x] & (CAVE_ICKY)))
        {
            // update the flow
            update_flow(y, x, wandering_idx);
        }

        // otherwise pick a random location (on a floor, in a room, and not in a
        // vault)
        else
        {
            // give up after 100 tries
            for (i = 0; i < 100; i++)
            {
                y = rand_int(p_ptr->cur_map_hgt);
                x = rand_int(p_ptr->cur_map_wid);
                if (in_bounds_fully(y, x) && (cave_feat[y][x] == FEAT_FLOOR)
                    && (cave_info[y][x] & (CAVE_ROOM))
                    && !(cave_info[y][x] & (CAVE_ICKY)))
                {
                    // update the flow
                    update_flow(y, x, wandering_idx);
                    break;
                }
            }
        }
    }

    // reset the pause (if any)
    wandering_pause[wandering_idx] = 0;
}

/*
 * Determines a wandering-destination for a monster.
 * default_idx_ptr is the wandering index to use by default, and gets updated by
 * this function.
 */
void new_wandering_destination(monster_type* m_ptr, monster_type* leader_ptr)
{
    int i;
    bool wandering_indices[FLOW_WANDERING_TAIL + 1];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    // many monsters don't get wandering destinations:
    if ((r_ptr->flags1 & (RF1_NEVER_MOVE))
        || (r_ptr->flags1 & (RF1_HIDDEN_MOVE))
        || !((r_ptr->flags2 & (RF2_SMART)) || (r_ptr->flags4 & (RF4_SHRIEK))))
    {
        return;
    }

    // there is a special way of finding indices at the Gates level
    // as otherwise we run out too quickly
    if (p_ptr->depth == 0)
    {
        // mark the used indices
        for (i = 1; i < mon_max; i++)
        {
            monster_type* n_ptr = &mon_list[i];

            /* Skip dead monsters */
            if (!n_ptr->r_idx)
                continue;

            if ((n_ptr->r_idx == m_ptr->r_idx) && one_in_(2))
                leader_ptr = n_ptr;
        }
    }

    // find a new index if one is not specified
    if (leader_ptr != NULL)
    {
        i = leader_ptr->wandering_idx;
    }
    else
    {
        // clear the index array
        for (i = 0; i <= FLOW_WANDERING_TAIL; i++)
        {
            wandering_indices[i] = false;
        }

        // mark the used indices
        for (i = 1; i < mon_max; i++)
        {
            monster_type* n_ptr = &mon_list[i];

            /* Skip dead monsters */
            if (!n_ptr->r_idx)
                continue;

            wandering_indices[n_ptr->wandering_idx] = true;
        }

        // find the smallest unused index
        for (i = FLOW_WANDERING_HEAD; i <= FLOW_WANDERING_TAIL; i++)
        {
            if (!wandering_indices[i])
                break;
        }
    }

    // if we have a valid index, then find a location and build the noise flow
    if (i <= FLOW_WANDERING_TAIL)
    {
        m_ptr->wandering_idx = i;
        m_ptr->wandering_dist = MON_WANDER_RANGE;
        new_wandering_flow(m_ptr, 0, 0);
    }

    // if we can't store any more indices, then just set it to zero, which means
    // that the monster will just move randomly and won't wander properly
    // this is very rare, but does occasionally happen (1 in 100 deep levels?)
    else
    {
        // msg_debug("Out of wandering monster indices.");
        m_ptr->wandering_idx = 0;
        m_ptr->wandering_dist = MON_WANDER_RANGE;
    }
}

/*
 * Makes Morgoth drop his Iron Crown with an appropriate message.
 */

void drop_iron_crown(monster_type* m_ptr, const char* msg)
{
    int i, near_y, near_x;

    log_debug("drop_iron_crown: called, ART_MORGOTH_3 cur_num=%d", 
              (&a_info[ART_MORGOTH_3])->cur_num);

    if ((&a_info[ART_MORGOTH_3])->cur_num == 0)
    {
        log_debug("drop_iron_crown: crown not yet dropped, dropping now");
        msg_print(msg);

        // choose a nearby location, but not his own square
        for (i = 0; i < 1000; i++)
        {
            near_y = m_ptr->fy + rand_range(-1, 1);
            near_x = m_ptr->fx + rand_range(-1, 1);

            if (((near_y != m_ptr->fy) || (near_x != m_ptr->fx))
                && cave_floor_bold(near_y, near_x))
                break;
        }

        log_debug("drop_iron_crown: dropping crown at (%d, %d)", near_y, near_x);
        
        // drop it there
        create_chosen_artefact(ART_MORGOTH_3, near_y, near_x, true);

        log_debug("drop_iron_crown: calling anger_morgoth(1) - crown lost");
        // lower Morgoth's protection, remove his light source, increase his
        // will and perception and evasion
        anger_morgoth(1);
    }
    else
    {
        log_debug("drop_iron_crown: crown already dropped, skipping");
    }
}

void make_alert(monster_type* m_ptr)
{
    int random_level = rand_range(ALERTNESS_ALERT, ALERTNESS_QUITE_ALERT);
    set_alertness(m_ptr, MAX(m_ptr->alertness, random_level));
}

/*
 * Changes a monster's alertness value and displays any appropriate messages
 */
void set_alertness(monster_type* m_ptr, int alertness)
{
    char m_name[80];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    bool redisplay = false;
    bool is_non_alert_thrall =
        m_ptr->r_idx == R_IDX_HUMAN_THRALL || m_ptr->r_idx == R_IDX_ELF_THRALL;

    // Nothing to be done...
    if (m_ptr->alertness == alertness)
        return;

    // cap the alertness value
    if (alertness < ALERTNESS_MIN)
        alertness = ALERTNESS_MIN;
    if (alertness > ALERTNESS_MAX)
        alertness = ALERTNESS_MAX;

    // Can't alert non-alert thralls so cap alertness lower for them
    if (is_non_alert_thrall && alertness >= ALERTNESS_UNWARY)
    {
        alertness = ALERTNESS_UNWARY;
    }

    /* Get the monster name */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    // First deal with cases where the monster becomes more alert
    if (m_ptr->alertness < alertness)
    {
        if ((m_ptr->alertness < ALERTNESS_UNWARY)
            && (alertness >= ALERTNESS_ALERT))
        {
            // Monster must spend its next turn noticing you
            m_ptr->skip_next_turn = true;

            // Notice the "waking up and noticing"
            if (m_ptr->ml)
            {
                // Dump a message
                msg_format("%^s wakes up and notices you.", m_name);

                // disturb the player
                disturb(1, 0);

                // redisplay the monster
                redisplay = true;
            }
        }
        else if ((m_ptr->alertness < ALERTNESS_UNWARY)
            && (alertness >= ALERTNESS_UNWARY))
        {
            // Notice the "waking up"
            if (m_ptr->ml)
            {
                // Dump a message
                msg_format("%^s wakes up.", m_name);

                // disturb the player
                disturb(1, 0);

                // redisplay the monster
                redisplay = true;
            }
        }
        else if ((m_ptr->alertness < ALERTNESS_ALERT)
            && (alertness >= ALERTNESS_ALERT))
        {
            // Monster must spend its next turn noticing you
            m_ptr->skip_next_turn = true;

            // Notice the "noticing" (!)
            if (m_ptr->ml)
            {
                // Dump a message
                msg_format("%^s notices you.", m_name);

                // disturb the player
                disturb(1, 0);

                // redisplay the monster
                redisplay = true;
            }
        }
        else if ((m_ptr->alertness < ALERTNESS_UNWARY)
            && (alertness < ALERTNESS_UNWARY)
            && (alertness >= ALERTNESS_UNWARY - 2))
        {
            // Notice the "stirring"
            if (m_ptr->ml)
            {
                // Dump a message
                msg_format("%^s stirs.", m_name);
            }
        }
        else if ((m_ptr->alertness < ALERTNESS_ALERT)
            && (alertness < ALERTNESS_ALERT)
            && (alertness >= ALERTNESS_ALERT - 2))
        {
            // Notice the "looking around"
            if (m_ptr->ml)
            {
                // Dump a message
                msg_format("%^s looks around.", m_name);
            }
        }
    }
    // First deal with cases where the monster becomes less alert
    else
    {
        if ((m_ptr->alertness >= ALERTNESS_UNWARY)
            && (alertness < ALERTNESS_UNWARY))
        {
            // Notice the falling asleep
            if (m_ptr->ml)
            {
                // Dump a message
                msg_format("%^s falls asleep.", m_name);

                // redisplay the monster
                redisplay = true;
            }
            if (m_ptr->r_idx == R_IDX_MORGOTH)
            {
                // Dump a message
                msg_format("%^s falls asleep.", m_name);

                // redisplay the monster
                redisplay = true;

                drop_iron_crown(m_ptr,
                    "His crown slips from off his brow and falls to the "
                    "ground nearby.");
            }
        }
        else if ((m_ptr->alertness >= ALERTNESS_ALERT)
            && (alertness < ALERTNESS_ALERT))
        {
            // Notice the becoming unwary
            if (m_ptr->ml)
            {
                // Dump a message
                msg_format("%^s becomes unwary.", m_name);

                // redisplay the monster
                redisplay = true;

                // give the monster a new place to wander towards
                if (!(r_ptr->flags2 & (RF2_TERRITORIAL)))
                    new_wandering_flow(m_ptr, p_ptr->py, p_ptr->px);
            }
        }
        else if (alertness < ALERTNESS_UNWARY)
        {
            // Notice the deepening sleep
            if (m_ptr->ml)
            {
                // Dump a message
                // msg_format("%^s's sleep deepens.", m_name);
            }
        }
        else if (alertness < ALERTNESS_ALERT)
        {
            // Notice the increasing unwariness
            if (m_ptr->ml)
            {
                // Dump a message
                // msg_format("%^s becomes more unwary.", m_name);
            }
        }
        else
        {
            // Notice the decreasing alertness
            if (m_ptr->ml)
            {
                // Dump a message
                // msg_format("%^s looks less alert.", m_name);
            }
        }
    }

    // do the actual alerting
    m_ptr->alertness = alertness;

    // redisplay the monster
    if (redisplay)
        lite_spot(m_ptr->fy, m_ptr->fx);
}

/*
 * Determines the chance of a skill or hit roll succeeding.
 * (1 d sides + skill) - (1 d sides + difficulty)
 * Results <= 0 count as fails.
 * Results > 0 are successes.
 *
 * returns the number of ways you could succeed
 * (i.e. number of chances out of sides*sides
 *
 * note that this will be a percentage for normal skills (10 sides)
 * but will be out of 400 for hit rolls
 */
extern int success_chance(int sides, int skill, int difficulty)
{
    int i, j;
    int ways = 0;

    for (i = 1; i <= sides; i++)
        for (j = 1; j <= sides; j++)
            if (i + skill > j + difficulty)
                ways++;

    return ways;
}

/*
 * Determine the result of a skill check.
 * (1d10 + skill) - (1d10 + difficulty)
 * Results <= 0 count as fails.
 * Results > 0 are successes.
 *
 * There is a fake skill check in monster_perception (where player roll is used
 * once for all monsters) so if something changes here, remember to change it
 * there.
 */
int skill_check(
    monster_type* m_ptr1, int skill, int difficulty, monster_type* m_ptr2)
{
    int skill_total;
    int difficulty_total;
    int skill_total_alt;
    int difficulty_total_alt;

    // bonuses against your enemy of choice
    if ((m_ptr1 == PLAYER) && (m_ptr2 != NULL))
        skill += bane_bonus(m_ptr2);
    if ((m_ptr2 == PLAYER) && (m_ptr1 != NULL))
        difficulty += bane_bonus(m_ptr1);

    // monster racial bane bonus against you
    if ((m_ptr1 == PLAYER) && (m_ptr2 != NULL))
        difficulty += elf_bane_bonus(m_ptr2) + dwarf_bane_bonus(m_ptr2)
            + edain_bane_bonus(m_ptr2);
    if ((m_ptr2 == PLAYER) && (m_ptr1 != NULL))
        skill += elf_bane_bonus(m_ptr1) + dwarf_bane_bonus(m_ptr1)
            + edain_bane_bonus(m_ptr1);

    // the basic rolls
    skill_total = dieroll(10) + skill;
    difficulty_total = dieroll(10) + difficulty;

    // alternate rolls for dealing with the curse
    skill_total_alt = dieroll(10) + skill;
    difficulty_total_alt = dieroll(10) + difficulty;

    // player curse?
    if (p_ptr->cursed)
    {
        if (m_ptr1 == PLAYER)
            skill_total = MIN(skill_total, skill_total_alt);
        if (m_ptr2 == PLAYER)
            difficulty_total = MIN(difficulty_total, difficulty_total_alt);
    }

    /* Debugging message */
    if (cheat_skill_rolls)
    {
        msg_format("{%d+%d v %d+%d = %d}.", skill_total - skill, skill,
            difficulty_total - difficulty, difficulty,
            skill_total - difficulty_total);
    }

    return (skill_total - difficulty_total);
}

/*
 * Light hating monsters get a penalty to hit/evn if the player's
 * square is too bright.
 */

extern int light_penalty(const monster_type* m_ptr)
{
    int penalty = 0;
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    if (r_ptr->flags3 & (RF3_HURT_LITE))
    {
        penalty = (cave_light[m_ptr->fy][m_ptr->fx] - 2);

        if (penalty < 0)
            penalty = 0;
    }

    return (penalty);
}

/*
 * Determine the result of an attempt to hit an opponent.
 * Results <= 0 count as misses.
 * Results > 0 are hits and, if high enough, are criticals.
 *
 * The monster is the creature doing the attacking.
 * This is used in displaying the attack roll details.
 * attacker_vis is whether the attacker is visible.
 * this is used in displaying the attack roll details.
 */
int hit_roll(int att, int evn, const monster_type* m_ptr1,
    const monster_type* m_ptr2, bool display_roll)
{
    int attack_score, attack_score_alt;
    int evasion_score, evasion_score_alt;
    bool non_player_visible;

    // determine the visibility for  the combat roll window
    if (m_ptr1 == PLAYER)
    {
        if (m_ptr2 == NULL)
            non_player_visible = true;
        else
            non_player_visible = m_ptr2->ml;
    }
    else
    {
        if (m_ptr1 == NULL)
            non_player_visible = true;
        else
            non_player_visible = m_ptr1->ml;
    }

    // roll the dice...
    attack_score = dieroll(20) + att;
    attack_score_alt = dieroll(20) + att;
    evasion_score = dieroll(20) + evn;
    evasion_score_alt = dieroll(20) + evn;

    // take the worst of two rolls for cursed players
    if (p_ptr->cursed)
    {
        if (m_ptr1 == PLAYER)
        {
            attack_score = MIN(attack_score, attack_score_alt);
        }
        else
        {
            evasion_score = MIN(evasion_score, evasion_score_alt);
        }
    }

    // set the information for the combat roll window
    if (display_roll)
    {
        update_combat_rolls1(m_ptr1, m_ptr2, non_player_visible, att,
            attack_score - att, evn, evasion_score - evn);
    }

    return (attack_score - evasion_score);
}

/*
 * Determines the player's evasion based on all the relevant attributes and
 * modifiers.
 */

int total_player_attack(monster_type* m_ptr, int base)
{
    int att = base;

    // reward concentration ability (if applicable)
    att += concentration_bonus(m_ptr->fy, m_ptr->fx);

    // reward focused attack ability (if applicable)
    att += focused_attack_bonus();

    // reward bane ability (if applicable)
    att += bane_bonus(m_ptr);

    // reward artifact-granted bane (if applicable)
    att += artifact_bane_bonus(m_ptr);

    // reward unique bane ability (if applicable)
    att += unique_bane_bonus(m_ptr);

    // reward master hunter ability (if applicable)
    att += master_hunter_bonus(m_ptr);

    // penalise distance -- note that this penalty will equal 0 in melee
    att -= distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx) / 5;

    // halve attack score for certain situations (and only halve positive
    // scores!)
    if (att > 0)
    {
        // penalise the player if (s)he can't see the monster
        if (!m_ptr->ml)
            att /= 2;

        // penalise the player if (s)he is in a pit or web
        if (cave_pit_bold(p_ptr->py, p_ptr->px)
            || (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB))
        {
            att /= 2;
        }
    }

    return (att);
}

/*
 * Determines the player's evasion based on all the relevant attributes and
 * modifiers.
 */

int total_player_evasion(monster_type* m_ptr, bool archery)
{
    int evn = p_ptr->skill_use[S_EVN];

    // reward successful use of the dodging ability
    evn += dodging_bonus();

    // reward successful use of the bane ability
    evn += bane_bonus(m_ptr);

    // reward artifact-granted bane (if applicable)
    evn += artifact_bane_bonus(m_ptr);

    // reward unique bane ability (if applicable)
    evn += unique_bane_bonus(m_ptr);

    // halve evasion for certain situations (and only halve positive evasion!)
    if (evn > 0)
    {
        // penalise the player if (s)he can't see the monster
        if (!m_ptr->ml)
            evn /= 2;

        // penalise targets of archery attacks
        if (archery)
            evn /= 2;

        // penalise the player if (s)he is in a pit or web
        if (cave_pit_bold(p_ptr->py, p_ptr->px)
            || (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB))
        {
            evn /= 2;
        }
    }

    return (evn);
}

/*
 * Determines a monster's attack score based on all the relevant attributes and
 * modifiers.
 */

int total_monster_attack(monster_type* m_ptr, int base)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int att = base;
    bool unseen = false;

    // penalise stunning
    if (m_ptr->stunned)
        att -= 2;

    // penalise being in bright light for light-averse monsters
    att -= light_penalty(m_ptr);

    // reward surrounding the player
    att += overwhelming_att_mod(m_ptr);

    // penalise distance
    att -= distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx) / 5;

    // racial bane bonus
    att += elf_bane_bonus(m_ptr);
    att += dwarf_bane_bonus(m_ptr);
    att += edain_bane_bonus(m_ptr);

    // unique bane penalty (player ability affecting monster)
    att -= unique_bane_bonus(m_ptr);

    // halve attack score for certain situations (and only halve positive
    // scores!)
    if (att > 0)
    {
        // check if player is unseen
        if ((r_ptr->light > 0) && strchr("@G", r_ptr->d_char)
            && (cave_light[p_ptr->py][p_ptr->px] <= 0))
            unseen = true;

        // penalise monsters who can't see the player
        if (unseen)
            att /= 2;
    }

    return (att);
}

/*
 * Determines a monster's evasion based on all the relevant attributes and
 * modifiers.
 */

int total_monster_evasion(monster_type* m_ptr, bool archery)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    int evn = r_ptr->evn;
    evn -= m_ptr->song_evasion_penalty;
    bool unseen = false;

    // penalise stunning
    if (m_ptr->stunned)
        evn -= 2;

    // penalise being in bright light for light-averse monsters
    evn -= light_penalty(m_ptr);

    // racial bane bonus
    evn += elf_bane_bonus(m_ptr);
    evn += dwarf_bane_bonus(m_ptr);
    evn += edain_bane_bonus(m_ptr);

    // unique bane penalty (player ability affecting monster)
    evn -= unique_bane_bonus(m_ptr);

    // halve evasion for certain situations (and only halve positive evasion!)
    if (evn > 0)
    {
        // check if player is unseen
        if ((r_ptr->light > 0) && strchr("@G", r_ptr->d_char)
            && (cave_light[p_ptr->py][p_ptr->px] <= 0))
            unseen = true;

        // penalise unwary monsters, or those who can't see the player
        if (unseen || (m_ptr->alertness < ALERTNESS_ALERT))
            evn /= 2;

        // penalise targets of archery attacks
        if (archery)
            evn /= 2;
    }

    // finally, all sleeping monsters have -5 total evasion
    if (m_ptr->alertness < ALERTNESS_UNWARY)
        evn = -5;

    return (evn);
}

/*
 * Monsters are already given a large set penalty for being asleep
 * (total evasion mod of -5) and unwary (evasion score / 2),
 * but we also give a bonus for high stealth characters who have ASSASSINATION.
 */

int stealth_melee_bonus(const monster_type* m_ptr, bool allow_unseen)
{
    int stealth_bonus = 0;

    if (p_ptr->active_ability[S_STL][STL_ASSASSINATION])
    {
        bool visible_target = allow_unseen || m_ptr->ml;
        bool unaware_target = (m_ptr->alertness < ALERTNESS_ALERT)
            || song_disguise_monster_is_fooled(m_ptr);

        if (unaware_target && visible_target && !(p_ptr->confused))
        {
            stealth_bonus = p_ptr->skill_use[S_STL];
        }
    }
    return (stealth_bonus);
}

/*
 * Give a bonus to attack the player depending on the number of adjacent
 * monsters. This is +1 for monsters near the attacker or to the sides, and +2
 * for monsters in the three positions behind the player:
 *
 * 1M1  M11
 * 1@1  1@2
 * 222  122
 *
 * We should lessen this with the crowd fighting ability
 */
int overwhelming_att_mod(monster_type* m_ptr)
{
    int mod = 0;
    int dir;
    int dy, dx;
    int py = p_ptr->py;
    int px = p_ptr->px;

    // determine the main direction from the player to the monster
    dir = rough_direction(py, px, m_ptr->fy, m_ptr->fx);

    // extract the deltas from the direction
    dy = ddy[dir];
    dx = ddx[dir];

    // if monster in an orthogonal direction   753
    //                                         8@M
    //                                         642
    if (dy * dx == 0)
    {
        // increase modifier for monsters engaged with the player...
        if (attacker_at(py + dx + dy, px - dy + dx))
            mod++; // direction 2
        if (attacker_at(py - dx + dy, px + dy + dx))
            mod++; // direction 3
        if (attacker_at(py + dx, px - dy))
            mod++; // direction 4
        if (attacker_at(py - dx, px + dy))
            mod++; // direction 5

        // ...especially if they are behind the player
        if (attacker_at(py + dx - dy, px - dy - dx))
            mod += 2; // direction 6
        if (attacker_at(py - dx - dy, px + dy - dx))
            mod += 2; // direction 7
        if (attacker_at(py - dy, px - dx))
            mod += 2; // direction 8
    }
    // if monster in a diagonal direction   875
    //                                      6@3
    //                                      42M
    else
    {
        // increase modifier for monsters engaged with the player...
        if (attacker_at(py + dy, px))
            mod++; // direction 2
        if (attacker_at(py, px + dx))
            mod++; // direction 3
        if (attacker_at(py + dx, px - dy))
            mod++; // direction 4
        if (attacker_at(py - dx, px + dy))
            mod++; // direction 5

        // ...especially if they are behind the player
        if (attacker_at(py - dy, px))
            mod += 2; // direction 6
        if (attacker_at(py, px - dx))
            mod += 2; // direction 7
        if (attacker_at(py - dy, px - dx))
            mod += 2; // direction 8
    }

    // adjust for crowd fighting ability
    if (p_ptr->active_ability[S_EVN][EVN_CROWD_FIGHTING])
    {
        mod /= 2;
    }

    return (mod);
}

/*
 * Determines the number of bonus dice from a (potentially) critical hit
 *
 * bonus of 1 die for every (6 + weight_in_pounds) over what is needed.
 * (using rounding at 0.5 instead of always rounding up)
 *
 * Thus for a Dagger (0.8lb):         7, 14, 20, 27...  (6+weight)
 *            Short Sword (1.5lb):    8, 15, 23, 30...
 *            Long Sword (3lb):       9, 18, 27, 35...
 *            Bastard Sword (4lb):   10, 20, 30, 40...
 *            Great Sword (7lb):     13, 26, 39, 52...
 *            Shortbow (2lb):         8, 16, 24, 32...
 *            Longbow (3lb):          9, 18, 27, 36...
 *            m 1dX (2lb):            8, 16, 24, 32...
 *            m 2dX (4lb):           10, 20, 30, 40...
 *            m 3dX (6lb):           12, 24, 36, 48...
 *
 * (old versions)
 * Thus for a Dagger (0.8lb):         9, 13, 17, 21...  5 then (3+weight)
 *            Short Sword (1.5lb):   10, 14, 19, 23...
 *            Long Sword (3lb):      11, 17, 23, 29...
 *            Bastard Sword (4lb):   12, 19, 26, 33...
 *            Great Sword (7lb):     15, 25, 35, 45...
 *            Shortbow (2lb):        10, 15, 20, 25...
 *            Longbow (3lb):         11, 17, 23, 29...
 *            m 1dX (2lb):           10, 15, 20, 25...
 *            m 2dX (4lb):           12, 19, 26, 33...
 *            m 3dX (6lb):           14, 23, 32, 41...
 * Thus for a Dagger (0.8lb):        11, 12, 13, 14...  (10 then weightx)
 *            Short Sword (1.5lb):   12, 13, 15, 16...
 *            Long Sword (3lb):      13, 16, 19, 22...
 *            Bastard Sword (4lb):   14, 18, 22, 26...
 *            Great Sword (7lb):     17, 24, 31, 38...
 *            Shortbow (2lb):        12, 14, 16, 18...
 *            Longbow (3lb):         13, 16, 19, 22...
 * Thus for a Dagger (0.8lb):         6, 12, 18, 24...  (5+weight)
 *            Short Sword (1.5lb):    7, 13, 20, 26...
 *            Long Sword (3lb):       8, 16, 24, 32...
 *            Bastard Sword (4lb):    9, 18, 27, 36...
 *            Great Sword (7lb):     12, 24, 36, 48...
 *            Shortbow (2lb):         7, 14, 21, 28...
 *            Longbow (3lb):          8, 16, 24, 32...
 * Thus for a Dagger (0.8lb):         4,  8, 12, 16...  (3+weight)
 *            Short Sword (1.5lb):    5,  9, 14, 18...
 *            Long Sword (3lb):       6, 12, 18, 25...
 *            Bastard Sword (4lb):    7, 14, 21, 28...
 *            Great Sword (7lb):     10, 20, 30, 40...
 *            Shortbow (2lb):         5, 10, 15, 20...
 *            Longbow (3lb):          6, 12, 18, 24...
 * Thus for a Dagger (0.8lb):         8, 12, 15, 18...  (old1)
 *            Short Sword (1.5lb):    9, 14, 18, 23...
 *            Long Sword (3lb):      11, 17, 23, 29...
 *            Bastard Sword (3.5lb): 11, 18, 24, 31...
 *            Great Sword (7lb):     15, 25, 35, 45...
 * Thus for a Dagger (0.8lb):         7, 10, 12, 14...  (old2)
 *            Short Sword (1.5lb):    8, 12, 15, 19...
 *            Long Sword (3lb):      10, 15, 20, 25...
 *            Bastard Sword (3.5lb): 10, 16, 21, 27...
 *            Great Sword (7lb):     14, 23, 32, 41...
 */
int crit_bonus(int hit_result, int weight, const monster_race* r_ptr,
    int skill_type, bool thrown, monster_type* attacker, const object_type* o_ptr)
{
    monster_type* m_ptr = attacker;
    int crit_bonus_dice;
    int crit_seperation = 70;

    if (attacker != NULL && attacker != PLAYER)
    {
        int shift = curse_flag_delta_cur(CUR_CRIT_THRESH_SHIFT);
        if (shift) hit_result += shift;
    }

    // When attacking a monster...
    if (r_ptr->level != 0)
    {
        // Can have improved criticals for melee
        if ((skill_type == S_MEL) && p_ptr->active_ability[S_MEL][MEL_FINESSE])
            crit_seperation -= 20;

        if ((skill_type == S_MEL) && thrown && o_ptr
            && (p_ptr->active_ability[S_MEL][MEL_THROWING]
                || object_grants_ability(o_ptr, S_MEL, MEL_THROWING))
            && player_can_treat_as_throwing(o_ptr))
        {
            crit_seperation -= 10;
        }

        // Can have improved criticals for melee with one handed weapons
        // Special case: Maedhros character can use Subtlety with hand-and-a-half weapons
        bool maedhros_hand_and_half = (c_info[p_ptr->pcharacter].flags_u & UNQ_MEL_MAEDHROS)
            && (k_info[(&inventory[INVEN_WIELD])->k_idx].flags3 & (TR3_HAND_AND_A_HALF))
            && (!inventory[INVEN_ARM].k_idx);
        
        if ((skill_type == S_MEL) && p_ptr->active_ability[S_MEL][MEL_CONTROL]
            && !thrown && (!two_handed_melee() || maedhros_hand_and_half) && !inventory[INVEN_ARM].k_idx)
            crit_seperation -= 20;

        // Subtlety can work with throwing if the weapon has TR4_SUBTLETY_THROW flag.
        // The flag extends an existing Subtlety ability; it does not grant one.
        if ((skill_type == S_MEL) && thrown && o_ptr
            && p_ptr->active_ability[S_MEL][MEL_CONTROL])
        {
            u32b st_f1, st_f2, st_f3, st_f4;
            object_flags4(o_ptr, &st_f1, &st_f2, &st_f3, &st_f4);
            if (st_f4 & TR4_SUBTLETY_THROW)
                crit_seperation -= 20;
        }

        // Can have inferior criticals for melee
        if ((skill_type == S_MEL) && p_ptr->active_ability[S_MEL][MEL_POWER])
            crit_seperation += 10;
    }

    // note: the +4 in this calculation is for rounding purposes
    crit_bonus_dice = (hit_result * 10 + 4) / (crit_seperation + weight);

    // When attacking a monster...
    if (r_ptr->level != 0)
    {
        // Resistance to criticals doubles what you need for each bonus die
        if (r_ptr->flags1 & (RF1_RES_CRIT))
            crit_bonus_dice /= 2;

        // certain creatures cannot suffer crits as they have no vulnerable
        // areas
        if (r_ptr->flags1 & (RF1_NO_CRIT))
            crit_bonus_dice = 0;
    }
    else if (m_ptr && p_ptr->active_ability[S_PER][PER_OUTWIT]
        && skill_check(PLAYER, p_ptr->skill_use[S_PER],
               monster_skill(m_ptr, S_PER), m_ptr)
            > 0)
    {
        crit_bonus_dice = 0;
    }

    // can't have fewer than zero dice
    if (crit_bonus_dice < 0)
        crit_bonus_dice = 0;

    return crit_bonus_dice;
}

/*
 * Describes the effect of a slay
 */
void slay_desc(char* description, u32b flag, const monster_type* m_ptr)
{
    char m_name[80];
    char m_poss[80];

    /* Monster description */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);
    monster_desc(m_poss, sizeof(m_poss), m_ptr, 0x22);

    if (flag == TR3_WILL_DRAIN)
    {
        sprintf(description, "drains %s will", m_poss);
        return;
    }

    switch (flag)
    {
    case TR1_SHARPNESS:
        sprintf(description, "cuts deeply");
        break;
    case TR1_SHARPNESS2:
        sprintf(description, "cuts effortlessly");
        break;
    case TR1_VAMPIRIC:
        sprintf(description, "drains life from %s", m_name);
        break;
    case TR1_SLAY_ORC:
        sprintf(description, "strikes truly");
        break;
    case TR1_SLAY_WOLF:
        sprintf(description, "strikes truly");
        break;
    case TR1_SLAY_SPIDER:
        sprintf(description, "strikes truly");
        break;
    case TR1_SLAY_UNDEAD:
        sprintf(description, "strikes truly");
        break;
    case TR1_SLAY_RAUKO:
        sprintf(description, "strikes truly");
        break;
    case TR1_SLAY_DRAGON:
        sprintf(description, "strikes truly");
        break;
    case TR1_SLAY_TROLL:
        sprintf(description, "strikes truly");
        break;
    case TR1_SLAY_MAN_OR_ELF:
        sprintf(description, "strikes truly");
        break;
    case TR4_SLAY_SERPENT:
    case TR4_SLAY_VAMPIRE:
    case TR4_SLAY_HORROR:
    case TR4_SLAY_CAT:
    case TR4_SLAY_GIANT:
        sprintf(description, "strikes truly");
        break;
    case TR1_BRAND_ELEC:
        sprintf(description, "shocks %s with the force of lightning", m_name);
        break;
    case TR1_BRAND_FIRE:
        sprintf(description, "burns %s with an inner fire", m_name);
        break;
    case TR1_BRAND_COLD:
        sprintf(description, "freezes %s", m_name);
        break;
    case TR1_BRAND_POIS:
        sprintf(description, "poisons %s", m_name);
        break;
    case TR4_ARMOR_SHATTER:
        sprintf(description, "shatters %s armor", m_poss);
        break;
    }

    return;
}

extern void ident(object_type* o_ptr)
{
    /* Identify it */
    object_aware(o_ptr);
    object_known(o_ptr);

    /* Apply an autoinscription, if necessary */
    apply_autoinscription(o_ptr);

    /* Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Combine / Reorder the pack (later) */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

    return;
}

extern void ident_on_wield(object_type* o_ptr)
{
    u32b f1, f2, f3, f4;
    u32b orig_f1;

    bool notice = false;

    char o_full_name[80];

    object_kind* k_ptr = &k_info[o_ptr->k_idx];

    /* Get the flags */
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    orig_f1 = f1;

    // Ignore previously identified items
    if (object_known_p(o_ptr))
    {
        return;
    }

    // identify the special item types that do nothing much
    // (since they have no hidden abilities, they must already be obvious)
    if (object_has_ego(o_ptr))
    {
        bool all_trivial = true;
        byte ego_pfx = object_ego_prefix(o_ptr);
        byte ego_sfx = object_ego_suffix(o_ptr);

        if (ego_pfx)
        {
            ego_item_type* e_ptr = &e_info[ego_pfx];
            if ((e_ptr->flags1 != 0L) || (e_ptr->flags2 != 0L)
                || ((e_ptr->flags3 | (TR3_IGNORE_ALL)) != (TR3_IGNORE_ALL))
                || (e_ptr->flags4 != 0L)
                || (e_ptr->abilities != 0))
            {
                all_trivial = false;
            }
        }

        if (ego_sfx)
        {
            ego_item_type* e_ptr = &e_info[ego_sfx];
            if ((e_ptr->flags1 != 0L) || (e_ptr->flags2 != 0L)
                || ((e_ptr->flags3 | (TR3_IGNORE_ALL)) != (TR3_IGNORE_ALL))
                || (e_ptr->flags4 != 0L)
                || (e_ptr->abilities != 0))
            {
                all_trivial = false;
            }
        }

        if (all_trivial)
            notice = true;
    }

    // identify true sight if it cures blindness
    if (p_ptr->blind && (f2 & (TR2_SEE_INVIS)))
    {
        notice = true;
    }

    // Currently tunneling is an unambiguous ego on mattocks, so auto-ID
    if (f1 & TR1_TUNNEL)
    {
        notice = true;
    }

    if (f3 & TR3_ACCURATE)
    {
        notice = true;
    }

    if (f3 & TR3_CUMBERSOME)
    {
        notice = true;
    }

    if (o_ptr->name1 || object_has_ego(o_ptr))
    {
        // For special items and artefacts, we need to ignore the flags that are
        // basic to the object type and focus on the special/artefact ones. We
        // can do this by subtracting out the basic flags

        f1 &= ~(k_ptr->flags1);
        f2 &= ~(k_ptr->flags2);
        f3 &= ~(k_ptr->flags3);

        /*
         * If a special/artefact modifies pval on a base that already has a pval
         * flag (e.g. Shadow Cloak has STEALTH), stripping base flags would hide
         * the effect and prevent auto-identification on wear.
         */
        {
            u32b base_pval_flags = (orig_f1 & k_ptr->flags1);

            if ((base_pval_flags & (TR1_TUNNEL | TR1_DAMAGE_SIDES))
                && (o_ptr->pval != k_ptr->pval))
            {
                f1 |= (base_pval_flags & (TR1_TUNNEL | TR1_DAMAGE_SIDES));
            }

            if ((base_pval_flags & (TR1_STR | TR1_NEG_STR))
                && (o_ptr->stat_bonus[A_STR] != k_ptr->stat_bonus[A_STR]))
                f1 |= (base_pval_flags & (TR1_STR | TR1_NEG_STR));
            if ((base_pval_flags & (TR1_DEX | TR1_NEG_DEX))
                && (o_ptr->stat_bonus[A_DEX] != k_ptr->stat_bonus[A_DEX]))
                f1 |= (base_pval_flags & (TR1_DEX | TR1_NEG_DEX));
            if ((base_pval_flags & (TR1_CON | TR1_NEG_CON))
                && (o_ptr->stat_bonus[A_CON] != k_ptr->stat_bonus[A_CON]))
                f1 |= (base_pval_flags & (TR1_CON | TR1_NEG_CON));
            if ((base_pval_flags & (TR1_GRA | TR1_NEG_GRA))
                && (o_ptr->stat_bonus[A_GRA] != k_ptr->stat_bonus[A_GRA]))
                f1 |= (base_pval_flags & (TR1_GRA | TR1_NEG_GRA));

            if ((base_pval_flags & TR1_MEL)
                && (o_ptr->skill_bonus[S_MEL] != k_ptr->skill_bonus[S_MEL]))
                f1 |= (base_pval_flags & TR1_MEL);
            if ((base_pval_flags & TR1_ARC)
                && (o_ptr->skill_bonus[S_ARC] != k_ptr->skill_bonus[S_ARC]))
                f1 |= (base_pval_flags & TR1_ARC);
            if ((base_pval_flags & TR1_STL)
                && (o_ptr->skill_bonus[S_STL] != k_ptr->skill_bonus[S_STL]))
                f1 |= (base_pval_flags & TR1_STL);
            if ((base_pval_flags & TR1_PER)
                && (o_ptr->skill_bonus[S_PER] != k_ptr->skill_bonus[S_PER]))
                f1 |= (base_pval_flags & TR1_PER);
            if ((base_pval_flags & TR1_WIL)
                && (o_ptr->skill_bonus[S_WIL] != k_ptr->skill_bonus[S_WIL]))
                f1 |= (base_pval_flags & TR1_WIL);
            if ((base_pval_flags & TR1_SMT)
                && (o_ptr->skill_bonus[S_SMT] != k_ptr->skill_bonus[S_SMT]))
                f1 |= (base_pval_flags & TR1_SMT);
            if ((base_pval_flags & TR1_SNG)
                && (o_ptr->skill_bonus[S_SNG] != k_ptr->skill_bonus[S_SNG]))
                f1 |= (base_pval_flags & TR1_SNG);
        }
    }

    if (f2 & (TR2_DARKNESS))
    {
        notice = true;
        msg_print("It reduces your light radius, but concentrates the light that remains.");
    }
    else if (f4 & (TR4_UNLIGHT))
    {
        notice = true;
        msg_print("It reduces your light radius without concentrating the light that remains.");
    }
    else if (f2 & (TR2_LIGHT))
    {
        if (o_ptr->tval != TV_LIGHT)
        {
            notice = true;
            msg_print("It glows with a wondrous light.");
        }
        else if ((o_ptr->sval == SV_LIGHT_FEANORIAN)
            || (o_ptr->sval == SV_LIGHT_LESSER_JEWEL) || (o_ptr->timeout > 0))
        {
            notice = true;
            msg_print("It glows very brightly.");
        }
    }
    else if (f2 & (TR2_SLOWNESS))
    {
        notice = true;
        msg_print("It slows your movement.");
    }
    else if (f2 & (TR2_SPEED))
    {
        notice = true;
        msg_print("It speeds your movement.");
    }

    else if (f1 & (TR1_DAMAGE_SIDES))
    {
        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (o_ptr->pval > 0)
        {
            notice = true;
            msg_print("You feel more forceful in melee.");
        }
        else if (o_ptr->pval < 0)
        {
            notice = true;
            msg_print("You feel less forceful in melee.");
        }
    }
    else if ((f1 & (TR1_STR)) || (f1 & (TR1_NEG_STR)))
    {
        int bonus = o_ptr->stat_bonus[A_STR];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel stronger.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less strong.");
        }
    }
    else if ((f1 & (TR1_DEX)) || (f1 & (TR1_NEG_DEX)))
    {
        int bonus = o_ptr->stat_bonus[A_DEX];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more agile.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less agile.");
        }
    }
    else if ((f1 & (TR1_CON)) || (f1 & (TR1_NEG_CON)))
    {
        int bonus = o_ptr->stat_bonus[A_CON];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more resilient.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less resilient.");
        }
    }
    else if ((f1 & (TR1_GRA)) || (f1 & (TR1_NEG_GRA)))
    {
        int bonus = o_ptr->stat_bonus[A_GRA];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more attuned to the world.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less attuned to the world.");
        }
    }
    else if (f1 & (TR1_MEL))
    {
        int bonus = o_ptr->skill_bonus[S_MEL];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more in control of your weapon.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less in control of your weapon.");
        }
    }
    else if (f1 & (TR1_ARC))
    {
        int bonus = o_ptr->skill_bonus[S_ARC];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more accurate at archery.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less accurate at archery.");
        }
    }
    else if (f1 & (TR1_STL))
    {
        int bonus = o_ptr->skill_bonus[S_STL];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("Your movements become quieter.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("Your movements less quiet.");
        }
    }
    else if (f1 & (TR1_PER))
    {
        int bonus = o_ptr->skill_bonus[S_PER];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more perceptive.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less perceptive.");
        }
    }
    else if (f1 & (TR1_WIL))
    {
        int bonus = o_ptr->skill_bonus[S_WIL];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel more firm of will.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less firm of will.");
        }
    }
    else if (f1 & (TR1_SMT))
    {
        int bonus = o_ptr->skill_bonus[S_SMT];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You feel a desire to craft things with your hands.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel less able to craft things.");
        }
    }
    else if (f1 & (TR1_SNG))
    {
        int bonus = o_ptr->skill_bonus[S_SNG];

        // can identify <+0> items if you already know the flavour
        if ((k_info[o_ptr->k_idx].flavor) && object_aware_p(o_ptr))
        {
            notice = true;
        }
        else if (bonus > 0)
        {
            notice = true;
            msg_print("You are filled with inspiration.");
        }
        else if (bonus < 0)
        {
            notice = true;
            msg_print("You feel a loss of inspiration.");
        }
    }

    // identify the item types that grant abilities
    else if (k_ptr->abilities > 0)
    {
        notice = true;
        msg_format("You have gained the ability '%s'.",
            b_name
                + (&b_info[ability_index(
                       k_ptr->skilltype[0], k_ptr->abilitynum[0])])
                      ->name);
    }

    // identify the special item types that grant abilities
    else if (object_has_ego(o_ptr))
    {
        byte ego_pfx = object_ego_prefix(o_ptr);
        byte ego_sfx = object_ego_suffix(o_ptr);

        ego_item_type* e_ptr = NULL;
        if (ego_pfx && e_info[ego_pfx].abilities > 0)
            e_ptr = &e_info[ego_pfx];
        else if (ego_sfx && e_info[ego_sfx].abilities > 0)
            e_ptr = &e_info[ego_sfx];

        if (e_ptr && e_ptr->abilities > 0)
        {
            notice = true;
            msg_format("You have gained the ability '%s'.",
                b_name
                    + (&b_info[ability_index(
                           e_ptr->skilltype[0], e_ptr->abilitynum[0])])
                          ->name);
        }
    }

    // identify the artefacts that grant abilities
    else if (o_ptr->name1)
    {
        artefact_type* a_ptr = &a_info[o_ptr->name1];

        if (a_ptr->abilities > 0)
        {
            notice = true;
            msg_format("You have gained the ability '%s'.",
                b_name
                    + (&b_info[ability_index(
                           a_ptr->skilltype[0], a_ptr->abilitynum[0])])
                          ->name);
        }
    }

    // can identify <+0> items if you already know the flavour
    else if (k_info[o_ptr->k_idx].flavor)
    {
        if (object_aware_p(o_ptr))
        {
            if (o_ptr->tval != TV_STAFF)
                notice = true;
        }
        else if (o_ptr->att > 0)
        {
            notice = true;
            msg_print("You somehow feel more accurate in combat.");
        }
        else if (o_ptr->att < 0)
        {
            notice = true;
            msg_print("You somehow feel less accurate in combat.");
        }
        else if (o_ptr->evn > 0)
        {
            notice = true;
            msg_print("You somehow feel harder to hit.");
        }
        else if (o_ptr->evn < 0)
        {
            notice = true;
            msg_print("You somehow feel more vulnerable.");
        }
        else if (o_ptr->pd > 0)
        {
            notice = true;
            msg_print("You somehow feel more protected.");
        }
    }

    if (notice)
    {
        if (object_uses_smithing_difficulty(o_ptr))
        {
            player_mark_object_experienced(o_ptr);
        }
        else
        {
            /* identify the object */
            ident(o_ptr);

            /* Full object description */
            object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

            /* Print the messages */
            msg_format("You recognize it as %s.", o_full_name);
        }
    }

    return;
}

extern void ident_resist(u32b flag)
{
    u32b f1, f2, f3;

    int i;

    bool notice = false;

    char effect_string[80];
    char o_full_name[80];
    char o_short_name[80];

    object_type* o_ptr;
    object_kind* k_ptr;

    /* Scan the equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];
        k_ptr = &k_info[o_ptr->k_idx];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Extract the item flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        {
            bool is_quiver1 = (i == INVEN_QUIVER1);
            bool is_quiver2 = (i == INVEN_QUIVER2);
            bool is_throwing_item = player_can_treat_as_throwing_flags(o_ptr, f3);

            if (is_quiver1)
                continue;
            if (is_quiver2 && !is_throwing_item)
                continue;
        }

        if (o_ptr->name1 || object_has_ego(o_ptr))
        {
            // For special items and artefacts, we need to ignore the flags that
            // are basic to the object type and focus on the special/artefact
            // ones. We can do this by subtracting out the basic flags

            f1 &= ~(k_ptr->flags1);
            f2 &= ~(k_ptr->flags2);
            f3 &= ~(k_ptr->flags3);
        }

        if (!object_known_p(o_ptr))
        {
            /* Short, pre-identification object description */
            object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

            if ((flag == TR2_RES_COLD) && (f2 & (TR2_RES_COLD)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s partly protects you from the chill.",
                    o_short_name);
            }
            else if ((flag == TR2_RES_FIRE) && (f2 & (TR2_RES_FIRE)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s partly protects you from the flame.",
                    o_short_name);
            }
            else if ((flag == TR2_RES_POIS) && (f2 & (TR2_RES_POIS)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s partly protects you from the poison.",
                    o_short_name);
            }
            else if ((flag == TR2_RES_BLEED) && (f2 & (TR2_RES_BLEED)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your bleeding is slowed by your %s.", o_short_name);
            }
            else if ((flag == TR2_RES_COLD) && (f2 & (TR2_VUL_COLD)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s intensifies the chill.", o_short_name);
            }
            else if ((flag == TR2_RES_FIRE) && (f2 & (TR2_VUL_FIRE)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s intensifies the flame.", o_short_name);
            }
            else if ((flag == TR2_RES_POIS) && (f2 & (TR2_VUL_POIS)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s intensifies the poison.", o_short_name);
            }
            else if ((flag == TR2_RES_FEAR) && (f2 & (TR2_RES_FEAR)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s fills you with courage.", o_short_name);
            }
            else if ((flag == TR2_RES_BLIND) && (f2 & (TR2_RES_BLIND)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s protects your sight.", o_short_name);
            }
            else if ((flag == TR2_RES_HALLU) && (f2 & (TR2_RES_HALLU)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s protects your sight.", o_short_name);
            }
            else if ((flag == TR2_RES_CONFU) && (f2 & (TR2_RES_CONFU)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s fills you with calm.", o_short_name);
            }
            else if ((flag == TR2_RES_STUN) && (f2 & (TR2_RES_STUN)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s fills you with calm.", o_short_name);
            }
            else if ((flag == TR2_FREE_ACT) && (f2 & (TR2_FREE_ACT)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s glows softly.", o_short_name);
            }
            else if ((flag == TR2_SUST_STR) && (f2 & (TR2_SUST_STR)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s sustains your strength.", o_short_name);
            }
            else if ((flag == TR2_SUST_DEX) && (f2 & (TR2_SUST_DEX)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s sustains your dexterity.", o_short_name);
            }
            else if ((flag == TR2_SUST_CON) && (f2 & (TR2_SUST_CON)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s sustains your constitution.", o_short_name);
            }
            else if ((flag == TR2_SUST_GRA) && (f2 & (TR2_SUST_GRA)))
            {
                notice = true;
                strnfmt(effect_string, sizeof(effect_string),
                    "Your %s sustains your grace.", o_short_name);
            }
        }

        if (notice)
        {
            if (object_uses_smithing_difficulty(o_ptr))
            {
                player_mark_object_experienced(o_ptr);
                msg_format("%s", effect_string);
            }
            else
            {
                /* identify the object */
                ident(o_ptr);

                /* Full object description */
                object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                /* Print the messages */
                msg_format("%s", effect_string);
                msg_format("You realize that it is %s.", o_full_name);
            }

            return;
        }
    }

    return;
}

extern void ident_passive(void)
{
    u32b f1, f2, f3;

    int i;

    bool notice = false;

    char effect_string[80];
    char o_full_name[80];
    char o_short_name[80];

    object_type* o_ptr;

    /* Scan the equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Extract the item flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        if (!object_known_p(o_ptr))
        {
            if ((f2 & (TR2_REGEN)) && (p_ptr->chp < p_ptr->mhp))
            {
                notice = true;
                SDL_strlcpy(effect_string,
                    "You notice that you are recovering much faster than "
                    "usual.",
                    sizeof(effect_string));
            }
            else if ((f2 & (TR2_AGGRAVATE)))
            {
                notice = true;
                SDL_strlcpy(effect_string,
                    "You notice that you are enraging your enemies.",
                    sizeof(effect_string));
            }
            else if ((f2 & (TR2_DANGER)))
            {
                notice = true;
                SDL_strlcpy(effect_string,
                    "You notice that you are attracting more powerful enemies.",
                    sizeof(effect_string));
            }
        }

        if (notice)
        {
            /* Short, pre-identification object description */
            object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

            /* Print the messages */
            msg_format("%s", effect_string);

            if (object_uses_smithing_difficulty(o_ptr))
            {
                player_mark_object_experienced(o_ptr);
            }
            else
            {
                /* identify the object */
                ident(o_ptr);

                /* Full object description */
                object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                msg_format("You realize that your %s is %s.", o_short_name,
                    o_full_name);
            }

            return;
        }
    }

    return;
}

extern void ident_see_invisible(const monster_type* m_ptr)
{
    u32b f1, f2, f3;

    int i;

    bool notice = false;

    char m_name[80];
    char o_full_name[80];
    char o_short_name[80];

    object_type* o_ptr;

    /* Scan the equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Extract the item flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        if (!object_known_p(o_ptr))
        {
            if ((f2 & (TR2_SEE_INVIS)))
            {
                notice = true;
            }
        }

        if (notice)
        {
            /* Get the monster name */
            monster_desc(m_name, sizeof(m_name), m_ptr, 0);

            /* Short, pre-identification object description */
            object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

            /* Print the messages */
            msg_format("You notice that you can see %s very clearly.", m_name);

            if (object_uses_smithing_difficulty(o_ptr))
            {
                player_mark_object_experienced(o_ptr);
            }
            else
            {
                /* identify the object */
                ident(o_ptr);

                /* Full object description */
                object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                msg_format("You realize that your %s is %s.", o_short_name,
                    o_full_name);
            }

            return;
        }
    }

    return;
}

extern void ident_haunted(void)
{
    u32b f1, f2, f3;

    int i;

    bool notice = false;

    char o_full_name[80];
    char o_short_name[80];

    object_type* o_ptr;

    /* Scan the equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Extract the item flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        if (!object_known_p(o_ptr))
        {
            if ((f2 & (TR2_HAUNTED)))
            {
                notice = true;
            }
        }

        if (notice)
        {
            /* Short, pre-identification object description */
            object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

            /* Print the messages */
            msg_print("You notice that wraiths are being drawn to you.");

            if (object_uses_smithing_difficulty(o_ptr))
            {
                player_mark_object_experienced(o_ptr);
            }
            else
            {
                /* identify the object */
                ident(o_ptr);

                /* Full object description */
                object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                msg_format("You realize that your %s is %s.", o_short_name,
                    o_full_name);
            }

            return;
        }
    }

    return;
}

/*
 * Identifies a hunger or sustenance item and prints a message
 */
void ident_hunger(void)
{
    u32b f1, f2, f3;
    int i;
    bool notice = false;
    char o_full_name[80];
    char o_short_name[80];
    object_type* o_ptr;

    /* Scan the equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Extract the item flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        if (!object_known_p(o_ptr))
        {
            if ((f2 & (TR2_HUNGER)) && (p_ptr->hunger > 0))
            {
                notice = true;
            }

            if ((f2 & (TR2_SLOW_DIGEST)) && (p_ptr->hunger < 0))
            {
                notice = true;
            }
        }

        if (notice)
        {
            /* Short, pre-identification object description */
            object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

            /* Print the messages */
            if (f2 & (TR2_HUNGER))
                msg_print("You notice that you are growing hungry much faster "
                          "than before.");
            else if (f2 & (TR2_SLOW_DIGEST))
                msg_print("You notice that you are growing hungry slower than "
                          "before.");

            if (object_uses_smithing_difficulty(o_ptr))
            {
                player_mark_object_experienced(o_ptr);
            }
            else
            {
                /* identify the object */
                ident(o_ptr);

                /* Full object description */
                object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                msg_format("You realize that your %s is %s.", o_short_name,
                    o_full_name);
            }

            return;
        }
    }

    return;
}

extern void ident_f2(u32b flag, object_type* supplied_object)
{
    u32b f1, f2, f3;

    int i;

    bool notice = false;

    char o_full_name[80];
    char o_short_name[80];

    object_type* o_ptr = supplied_object;

    if (!o_ptr)
    {
        /* Scan the equipment */
        for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            o_ptr = &inventory[i];

            /* Skip non-objects */
            if (!o_ptr->k_idx)
                continue;

            /* Extract the item flags */
            object_flags(o_ptr, &f1, &f2, &f3);

            if (!object_known_p(o_ptr) && (f2 & (flag)))
            {
                notice = true;
                break;
            }
        }
    }
    else if (!object_known_p(o_ptr))
    {
        object_flags(o_ptr, &f1, &f2, &f3);
        if (f2 & flag)
        {
            notice = true;
        }
    }

    if (notice && o_ptr)
    {
        /* Short, pre-identification object description */
        object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

        if (object_uses_smithing_difficulty(o_ptr))
        {
            player_mark_object_experienced(o_ptr);
            msg_format("You learn more about your %s.", o_short_name);
        }
        else
        {
            /* identify the object */
            ident(o_ptr);

            /* Full object description */
            object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

            msg_format(
                "You realize that your %s is %s.", o_short_name, o_full_name);
        }
    }
}

extern void ident_f3(u32b flag, object_type* supplied_object)
{
    u32b f1, f2, f3;

    int i;

    bool notice = false;

    char o_full_name[80];
    char o_short_name[80];

    object_type* o_ptr = supplied_object;

    if (!o_ptr)
    {
        /* Scan the equipment */
        for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            o_ptr = &inventory[i];

            /* Skip non-objects */
            if (!o_ptr->k_idx)
                continue;

            /* Extract the item flags */
            object_flags(o_ptr, &f1, &f2, &f3);

            if (!object_known_p(o_ptr) && (f3 & (flag)))
            {
                notice = true;
                break;
            }
        }
    }
    else if (!object_known_p(o_ptr))
    {
        object_flags(o_ptr, &f1, &f2, &f3);
        if (f3 & flag)
        {
            notice = true;
        }
    }

    if (notice && o_ptr)
    {
        /* Short, pre-identification object description */
        object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

        if (object_uses_smithing_difficulty(o_ptr))
        {
            player_mark_object_experienced(o_ptr);
            msg_format("You learn more about your %s.", o_short_name);
        }
        else
        {
            /* identify the object */
            ident(o_ptr);

            /* Full object description */
            object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

            msg_format(
                "You realize that your %s is %s.", o_short_name, o_full_name);
        }
    }
}

/*
 * Identifies a weapon from one of its slays being active and prints a message
 */
void ident_weapon_by_use(
    object_type* o_ptr, const monster_type* m_ptr, u32b flag)
{
    char o_short_name[80];
    char o_full_name[80];
    char slay_description[160];

    /* Short, pre-identification object description */
    object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

    /* Description of the 'slay' */
    slay_desc(slay_description, flag, m_ptr);

    /* Print the messages */
    msg_format("Your %s %s.", o_short_name, slay_description);
    if (object_uses_smithing_difficulty(o_ptr))
    {
        player_mark_object_experienced(o_ptr);
    }
    else
    {
        /* identify the object */
        ident(o_ptr);

        /* Full object description */
        object_desc(o_full_name, sizeof(o_full_name), o_ptr, true, 3);

        msg_format("You recognize it as %s.", o_full_name);
    }

    return;
}

void ident_bow_arrow_by_use(object_type* j_ptr, object_type* i_ptr,
    object_type* o_ptr, const monster_type* m_ptr, u32b bow_flag,
    u32b arrow_flag)
{
    char i_short_name[80];
    char i_full_name[80];
    char j_short_name[80];
    char j_full_name[80];
    char slay_description[160];

    /* Short, pre-identification bow and arrow description */
    object_desc(j_short_name, sizeof(j_short_name), j_ptr, false, 0);
    object_desc(i_short_name, sizeof(i_short_name), i_ptr, false, 0);

    if (arrow_flag)
    {
        slay_desc(slay_description, arrow_flag, m_ptr);

        msg_format("Your %s %s.", i_short_name, slay_description);
        if (object_uses_smithing_difficulty(i_ptr))
        {
            player_mark_object_experienced(i_ptr);
            player_mark_object_experienced(o_ptr);
        }
        else
        {
            /* Identify the arrow and remaining arrows */
            object_aware(i_ptr);
            object_known(i_ptr);
            object_aware(o_ptr);
            object_known(o_ptr);

            /* Apply an autoinscription, if necessary */
            apply_autoinscription(i_ptr);
            apply_autoinscription(o_ptr);

            /* Recalculate bonuses */
            p_ptr->update |= (PU_BONUS);

            /* Combine / Reorder the pack (later) */
            p_ptr->notice |= (PN_COMBINE | PN_REORDER);

            /* Window stuff */
            p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

            /* Full arrow description */
            object_desc(i_full_name, sizeof(i_full_name), i_ptr, true, 3);

            msg_format("You recognize it as %s.", i_full_name);
        }

        // don't carry on to identify the bow on the same shot
        return;
    }

    if (bow_flag)
    {
        slay_desc(slay_description, bow_flag, m_ptr);

        msg_format("Your shot %s.", slay_description);
        if (object_uses_smithing_difficulty(j_ptr))
        {
            player_mark_object_experienced(j_ptr);
        }
        else
        {
            /* Identify the bow */
            object_aware(j_ptr);
            object_known(j_ptr);

            /* Apply an autoinscription, if necessary */
            apply_autoinscription(j_ptr);

            /* Recalculate bonuses */
            p_ptr->update |= (PU_BONUS);

            /* Combine / Reorder the pack (later) */
            p_ptr->notice |= (PN_COMBINE | PN_REORDER);

            /* Window stuff */
            p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

            /* Full bow description */
            object_desc(j_full_name, sizeof(j_full_name), j_ptr, true, 3);

            msg_format("You recognize your %s to be %s.", j_short_name,
                j_full_name);
        }
    }

    return;
}

void apply_weapon_combat_effects(object_type* o_ptr, monster_type* m_ptr,
    int skill_type, int net_dam, bool fatal_blow, cptr armor_shatter_noun)
{
    monster_race* r_ptr;
    u32b f1 = 0, f2 = 0, f3 = 0, f4 = 0;

    if (!o_ptr || !o_ptr->k_idx || !m_ptr)
        return;

    r_ptr = &r_info[m_ptr->r_idx];
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    if (!fatal_blow && (net_dam > 0))
    {
        if ((f4 & TR4_ARMOR_SHATTER) && (r_ptr->flags3 & RF3_HAS_ARMOUR))
        {
            int shatter_skill = p_ptr->skill_use[skill_type];
            int resist_skill = monster_skill(m_ptr, S_WIL);

            if (skill_check(NULL, shatter_skill, resist_skill, m_ptr) > 0)
            {
                if (m_ptr->armor_ps_reduction < r_ptr->ps)
                {
                    m_ptr->armor_ps_reduction++;

                    if (m_ptr->ml)
                    {
                        char m_poss[80];
                        monster_desc(m_poss, sizeof(m_poss), m_ptr, 0x22);
                        msg_format("Your %s shatters %s armor!",
                            armor_shatter_noun ? armor_shatter_noun : "attack",
                            m_poss);
                    }

                    if (!object_known_p(o_ptr))
                    {
                        ident_weapon_by_use(o_ptr, m_ptr, TR4_ARMOR_SHATTER);
                    }
                }
            }
        }

        if ((f3 & TR3_WILL_DRAIN) && !(r_ptr->flags2 & RF2_MINDLESS))
        {
            int drain_skill = p_ptr->skill_use[skill_type];
            int resist_skill = monster_skill(m_ptr, S_WIL);

            if (skill_check(NULL, drain_skill, resist_skill, m_ptr) > 0)
            {
                m_ptr->song_will_penalty++;

                if (m_ptr->ml)
                {
                    char m_poss[80];
                    monster_desc(m_poss, sizeof(m_poss), m_ptr, 0x22);
                    msg_format("You drain %s will!", m_poss);
                }

                if (!object_known_p(o_ptr))
                {
                    ident_weapon_by_use(o_ptr, m_ptr, TR3_WILL_DRAIN);
                }
            }
        }
    }

    if (fatal_blow && (f1 & TR1_VAMPIRIC) && !monster_nonliving(r_ptr))
    {
        if (hp_player(7, false, false) && !object_known_p(o_ptr))
        {
            ident_weapon_by_use(o_ptr, m_ptr, TR1_VAMPIRIC);
        }
    }
}

/*
 * Makes checks against perception to see if the weapon becomes identified
 *
 * Returns the flag that was noticed, the calling function can send this to
 * ident_weapon_by_use
 */

u32b maybe_notice_slay(const object_type* o_ptr, u32b flag)
{
    u32b noticed_flag = 0L;

    if (!object_known_p(o_ptr))
    {
        noticed_flag = flag;
    }

    return noticed_flag;
}

/*
 * Determines the number of bonus dice from slays/brands
 *
 * Note that "flasks of oil" do NOT do fire damage, although they
 * certainly could be made to do so.  XXX XXX
 *
 * All 'slays' and 'brands' do one additional die (these are cumulative)
 * 'kills' do an additional two dice.
 */
int slay_bonus(
    const object_type* o_ptr, const monster_type* m_ptr, u32b* noticed_flag)
{
    int slay_bonus_dice = 0;
    int brand_bonus_dice = 0;

    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    monster_lore* l_ptr = &l_list[m_ptr->r_idx];

    u32b f1, f2, f3, f4;

    /* Extract the flags */
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    /* Some "weapons" and "arrows" do extra damage */
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    case TV_BOW:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_DIGGING:
    {
        /* Slay Wolf */
        if ((f1 & (TR1_SLAY_WOLF)) && (r_ptr->flags3 & (RF3_WOLF)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_WOLF);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_WOLF);
        }

        /* Slay Spider */
        if ((f1 & (TR1_SLAY_SPIDER)) && (r_ptr->flags3 & (RF3_SPIDER)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_SPIDER);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_SPIDER);
        }

        /* Slay Undead */
        if ((f1 & (TR1_SLAY_UNDEAD)) && (r_ptr->flags3 & (RF3_UNDEAD)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_UNDEAD);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_UNDEAD);
        }

        /* Slay Rauko */
        if ((f1 & (TR1_SLAY_RAUKO)) && (r_ptr->flags3 & (RF3_RAUKO)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_RAUKO);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_RAUKO);
        }

        /* Slay Orc */
        if ((f1 & (TR1_SLAY_ORC)) && (r_ptr->flags3 & (RF3_ORC)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_ORC);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_ORC);
        }

        /* Slay Troll */
        if ((f1 & (TR1_SLAY_TROLL)) && (r_ptr->flags3 & (RF3_TROLL)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_TROLL);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_TROLL);
        }

        /* Slay Dragon */
        if ((f1 & (TR1_SLAY_DRAGON)) && (r_ptr->flags3 & (RF3_DRAGON)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_DRAGON);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_DRAGON);
        }

        /* Slay Serpent */
        if ((f4 & (TR4_SLAY_SERPENT)) && (r_ptr->flags3 & (RF3_SERPENT)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_SERPENT);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR4_SLAY_SERPENT);
        }

        /* Slay Vampire */
        if ((f4 & (TR4_SLAY_VAMPIRE)) && (r_ptr->flags3 & (RF3_VAMPIRE)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_VAMPIRE);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR4_SLAY_VAMPIRE);
        }

        /* Slay Horror */
        if ((f4 & (TR4_SLAY_HORROR)) && (r_ptr->flags3 & (RF3_HORROR)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_HORROR);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR4_SLAY_HORROR);
        }

        /* Slay Cat */
        if ((f4 & (TR4_SLAY_CAT)) && (r_ptr->flags3 & (RF3_CAT)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_CAT);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR4_SLAY_CAT);
        }

        /* Slay Giant */
        if ((f4 & (TR4_SLAY_GIANT)) && (r_ptr->flags3 & (RF3_GIANT)))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_GIANT);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR4_SLAY_GIANT);
        }

        /* Slay Men and Elves */
        if ((f1 & (TR1_SLAY_MAN_OR_ELF))
            && ((r_ptr->flags3 & (RF3_MAN)) || (r_ptr->flags3 & (RF3_ELF))))
        {
            if (m_ptr->ml)
            {
                l_ptr->flags3 |= (RF3_MAN);
                l_ptr->flags3 |= (RF3_ELF);
            }

            slay_bonus_dice += 1;

            *noticed_flag = maybe_notice_slay(o_ptr, TR1_SLAY_MAN_OR_ELF);
        }

        /* Brand (Elec) */
        if (f1 & (TR1_BRAND_ELEC))
        {
            /* Notice immunity */
            if (r_ptr->flags3 & (RF3_RES_ELEC))
            {
                if (m_ptr->ml)
                {
                    l_ptr->flags3 |= (RF3_RES_ELEC);
                }
            }

            /* Otherwise, take the damage */
            else
            {
                brand_bonus_dice += 1;

                *noticed_flag = maybe_notice_slay(o_ptr, TR1_BRAND_ELEC);
            }
        }

        /* Brand (Fire) */
        if (f1 & (TR1_BRAND_FIRE))
        {
            /* Notice immunity */
            if (r_ptr->flags3 & (RF3_RES_FIRE))
            {
                if (m_ptr->ml)
                {
                    l_ptr->flags3 |= (RF3_RES_FIRE);
                }
            }

            /* Otherwise, take the damage */
            else
            {
                brand_bonus_dice += 1;

                *noticed_flag = maybe_notice_slay(o_ptr, TR1_BRAND_FIRE);

                // extra bonus against vulnerable creatures
                if (r_ptr->flags3 & (RF3_HURT_FIRE))
                {
                    brand_bonus_dice += 1;

                    /* Memorize the effects */
                    l_ptr->flags3 |= (RF3_HURT_FIRE);
                }
            }
        }

        /* Brand (Cold) */
        if (f1 & (TR1_BRAND_COLD))
        {
            /* Notice immunity */
            if (r_ptr->flags3 & (RF3_RES_COLD))
            {
                if (m_ptr->ml)
                {
                    l_ptr->flags3 |= (RF3_RES_COLD);
                }
            }

            /* Otherwise, take the damage */
            else
            {
                brand_bonus_dice += 1;

                *noticed_flag = maybe_notice_slay(o_ptr, TR1_BRAND_COLD);

                // extra bonus against vulnerable creatures
                if (r_ptr->flags3 & (RF3_HURT_COLD))
                {
                    brand_bonus_dice += 1;

                    /* Memorize the effects */
                    l_ptr->flags3 |= (RF3_HURT_COLD);
                }
            }
        }

        /* Brand (Poison) */
        if (f1 & (TR1_BRAND_POIS))
        {
            /* Notice immunity */
            if (r_ptr->flags3 & (RF3_RES_POIS))
            {
                if (m_ptr->ml)
                {
                    l_ptr->flags3 |= (RF3_RES_POIS);
                }
            }

            /* Otherwise, take the damage */
            else
            {
                brand_bonus_dice += 1;

                *noticed_flag = maybe_notice_slay(o_ptr, TR1_BRAND_POIS);
            }
        }

        break;
    }
    }

    if ((slay_bonus_dice > 0) || (brand_bonus_dice > 1))
    {
        // cause a temporary morale penalty
        scare_onlooking_friends(m_ptr, -20);
    }

    return (slay_bonus_dice + brand_bonus_dice);
}

/*
 * Determines the protection percentage
 */
extern int prt_after_sharpness(const object_type* o_ptr, u32b* noticed_flag)
{
    int protection = 100;

    u32b f1, f2, f3;

    /* Extract the flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    /* Sharpness */
    if (f1 & (TR1_SHARPNESS))
    {
        *noticed_flag = maybe_notice_slay(o_ptr, TR1_SHARPNESS);
        protection = 50;
    }

    /* Sharpness 2 */
    if (f1 & (TR1_SHARPNESS2))
    {
        *noticed_flag = maybe_notice_slay(o_ptr, TR1_SHARPNESS2);
        protection = 0;
    }

    if (protection < 0)
        protection = 0;

    return protection;
}

bool is_normal_attack(int attack_type)
{
    return (attack_type == ATT_MAIN) || (attack_type == ATT_FLANKING)
        || (attack_type == ATT_CONTROLLED_RETREAT)
        || (attack_type == ATT_IMPALE);
}

/*
 * Search a single square for hidden things
 * (a utility function called by 'search' and 'perceive')
 */
void search_square(int y, int x, int dist, int searching)
{
    int score = 0;
    int difficulty = 0;
    int chest_level = 0;

    object_type* o_ptr;
    int chest_trap_present = false;

    // determine if a trap is present
    for (o_ptr = get_first_object(y, x); o_ptr; o_ptr = get_next_object(o_ptr))
    {
        if ((o_ptr->tval == TV_CHEST) && (o_ptr->pval > 0)
            && object_chest_trap_flags(o_ptr) && !object_known_p(o_ptr))
        {
            chest_trap_present = true;
            chest_level = o_ptr->pval;
            break;
        }
    }

    // if searching, discover unknown adjacent squares of interest
    if (searching)
    {
        if ((dist == 1) && !(cave_info[y][x] & (CAVE_MARK)))
        {
            // mark all non-floor non-trap squares
            if (!cave_floorlike_bold(y, x))
            {
                cave_info[y][x] |= (CAVE_MARK);
            }

            // mark an object, but not the square it is in
            if (cave_o_idx[y][x] != 0)
            {
                (&o_list[cave_o_idx[y][x]])->marked = true;
            }

            /* Redraw */
            lite_spot(y, x);
        }
    }

    // if there is anything to notice...
    if ((cave_trap_bold(y, x) && (cave_info[y][x] & (CAVE_HIDDEN)))
        || (cave_feat[y][x] == FEAT_SECRET) || chest_trap_present)
    {
        // give up if the square is unseen and not adjacent
        if ((dist > 1) && !(cave_info[y][x] & (CAVE_SEEN)))
            return;

        // no bonus for searching on your own square
        if (dist < 1)
        {
            dist = 1;
        }

        // Determine the base score
        score = p_ptr->skill_use[S_PER] + cave_light[y][x];

        // If using the search command give a score bonus
        if (searching)
            score += 5;

        // Determine the base difficulty
        if (chest_trap_present)
        {
            difficulty = chest_level / 2;
        }
        else
        {
            if (p_ptr->depth > 0)
            {
                difficulty = p_ptr->depth / 2;
            }
            else
            {
                difficulty = 10;
            }
        }

        // Give various penalties
        if (p_ptr->blind || no_light() || p_ptr->image)
            difficulty += 5; // can't see properly
        if (p_ptr->confused)
            difficulty += 5; // confused
        if (dist == 2)
            difficulty += 2; // distance 2
        if (dist == 3)
            difficulty += 4; // distance 3
        if (dist == 4)
            difficulty += 6; // distance 4
        if cave_trap_bold (y, x)
            difficulty += 5; // dungeon trap
        if (cave_feat[y][x] == FEAT_SECRET)
            difficulty += 10; // secret door
        if (chest_trap_present)
            difficulty += 15; // chest trap
        // if (cave_info[y][x] & (CAVE_ICKY)) difficulty
        // += 2;   // inside least/lesser/greater vaults

        // Spider bane bonus helps to find webs
        if (cave_feat[y][x] == FEAT_TRAP_WEB)
        {
            difficulty -= spider_bane_bonus();
            difficulty -= artifact_spider_bane_bonus();
        }

        /* Sometimes, notice things */
        if (skill_check(PLAYER, score, difficulty, NULL) > 0)
        {
            /* Dungeon trap */
            if (cave_trap_bold(y, x))
            {
                /* Reveal the trap */
                reveal_trap(y, x);

                /* Message */
                msg_print("You have found a trap.");

                /* Disturb */
                disturb(0, 0);
            }

            /* Secret door */
            if (cave_feat[y][x] == FEAT_SECRET)
            {
                /* Message */
                msg_print("You have found a secret door.");

                /* Pick a door */
                place_closed_door(y, x);

                /* Disturb */
                disturb(0, 0);
            }

            if (chest_trap_present)
            {
                /* Message */
                msg_print("You have discovered a trap on the chest!");

                /* Know the trap */
                object_known(o_ptr);

                /* Notice it */
                disturb(0, 0);
            }
        }
    }
}

/*
 * Search for adjacent hidden things
 */
void search(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int y, x;

    /* Search the adjacent grids */
    for (y = (py - 1); y <= (py + 1); y++)
    {
        for (x = (px - 1); x <= (px + 1); x++)
        {
            if ((x != px) || (y != py))
                search_square(y, x, 1, true);
        }
    }

    // also make the normal perception check
    perceive();
}

/*
 * Maybe notice hidden things nearby
 */
extern void perceive(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int y, x, dist;

    /* Search nearby grids */
    for (y = (py - 4); y <= (py + 4); y++)
    {
        for (x = (px - 4); x <= (px + 4); x++)
        {
            if (in_bounds(y, x))
            {
                dist = distance(py, px, y, x);

                /* Search only if adjacent, player lit or permanently lit */
                if ((dist <= 1) || (p_ptr->cur_light >= dist)
                    || (cave_info[y][x] & (CAVE_GLOW)))
                {
                    /* Search only if also within four grids and in line of
                     * sight*/
                    if ((dist <= 4) && los(py, px, y, x))
                    {
                        search_square(y, x, dist, false);
                    }
                }
            }
        }
    }
}

/*
 * Check if an object is a weapon or armor that would violate the Oath of the Smith
 */
bool is_weapon_or_armor(const object_type* o_ptr)
{
    /* Check if it's a weapon */
    if (o_ptr->tval == TV_SWORD || o_ptr->tval == TV_POLEARM || 
        o_ptr->tval == TV_HAFTED || o_ptr->tval == TV_BOW)
        return true;
        
    /* Check if it's armor */
    if (o_ptr->tval == TV_SOFT_ARMOR || o_ptr->tval == TV_MAIL || 
        o_ptr->tval == TV_SHIELD || o_ptr->tval == TV_HELM || 
        o_ptr->tval == TV_CROWN || o_ptr->tval == TV_CLOAK || 
        o_ptr->tval == TV_GLOVES || o_ptr->tval == TV_BOOTS)
        return true;
        
    return false;
}

bool smith_oath_forbids_object(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    return chosen_oath(OATH_SMITH) && !oath_invalid(OATH_SMITH)
        && is_weapon_or_armor(o_ptr) && !is_smithed_by_player(o_ptr);
}

bool smith_oath_confirm_break(void)
{
    char* prompt;

    if (!chosen_oath(OATH_SMITH) || oath_invalid(OATH_SMITH))
        return true;

    prompt = oath_confirmation_prompt(OATH_SMITH);
    if (!prompt || !prompt[0])
        prompt = "Are you certain you wish to break your Oath of the Smith?";

    if (!get_check_oath_multiline(prompt))
        return false;

    p_ptr->oaths_broken |= OATH_SMITH_FLAG;
    apply_oath_breaking_curse(OATH_SMITH);
    return true;
}

/*
 * Check if an object was smithed by the player
 */
static const object_type* replacement_filter_incoming = NULL;
static bool item_tester_limit_group(const object_type* o_ptr);

static bool pack_item_matches_replacement_type(const object_type* incoming,
                                               const object_type* candidate)
{
    if (!incoming || !candidate || !candidate->k_idx)
        return false;

    if (player_oil_container_object(incoming)
        && player_oil_container_object(candidate))
    {
        return true;
    }

    if (incoming->tval == candidate->tval)
        return true;

    int incoming_slot = wield_slot(incoming);
    if (incoming_slot >= INVEN_WIELD && incoming_slot < INVEN_TOTAL)
    {
        int candidate_slot = wield_slot(candidate);
        if (candidate_slot == incoming_slot)
            return true;
    }

    return false;
}

static void format_staff_prompt_name(char* buf, size_t max,
                                     const object_type* o_ptr, bool pref)
{
    char full[80];
    const char* staff_of;

    if (!buf || max == 0)
        return;

    buf[0] = '\0';

    if (!o_ptr || !o_ptr->k_idx)
        return;

    object_desc(full, sizeof(full), o_ptr, pref, 0);

    if (o_ptr->tval != TV_STAFF)
    {
        SDL_strlcpy(buf, full, max);
        return;
    }

    staff_of = strstr(full, "Staff of ");
    if (!staff_of)
    {
        SDL_strlcpy(buf, full, max);
        return;
    }

    if (!pref)
    {
        SDL_strlcpy(buf, staff_of, max);
        return;
    }

    if (!strncmp(full, "The ", 4))
        strnfmt(buf, max, "The %s", staff_of);
    else if (!strncmp(full, "no more ", 8))
        strnfmt(buf, max, "no more %s", staff_of);
    else
        strnfmt(buf, max, "a %s", staff_of);
}

bool is_smithed_by_player(const object_type* o_ptr)
{
    return (o_ptr->unused1 != 0);
}

/*
 * Prompt the player to drop an inventory item so a new object can be picked up.
 * Returns true if an item was dropped, false if the player declined or nothing was dropped.
 */
static bool prompt_replace_pack_item(const object_type* incoming)
{
    char incoming_name[80];
    char prompt[160];

    /* Ensure story font is disabled before showing messages */
    extern bool sdl_is_story_font_enabled(void);
    extern void sdl_story_font_disable(void);
    if (sdl_is_story_font_enabled())
        sdl_story_font_disable();

    object_desc(incoming_name, sizeof(incoming_name), incoming, true, 3);
    msg_format("No room for %s.", incoming_name);
    msg_print("Choose an item to replace.");

    strnfmt(prompt, sizeof(prompt), "Replace which item to pick up %s? ", incoming_name);

    while (true)
    {
        int item;

        if (!get_item(&item, prompt,
                "You have nothing to replace.", (USE_INVEN)))
        {
            return false;
        }

        if ((item < 0) || (item >= INVEN_PACK))
        {
            bell("Illegal object choice!");
            continue;
        }

        object_type* drop_ptr = &inventory[item];

        if (!drop_ptr->k_idx)
        {
            bell("That slot is empty.");
            continue;
        }

        if (!queue_deferred_pickup_pack_drop(item, drop_ptr->number,
                player_oil_container_object(incoming)
                    && player_oil_container_object(drop_ptr)))
            continue;

        /* Let inventory housekeeping run before we attempt the pickup again */
        p_ptr->notice |= (PN_COMBINE | PN_REORDER);
        notice_stuff();

        return true;
    }
}

static bool object_is_brass_lamp(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx && o_ptr->tval == TV_LIGHT
        && o_ptr->sval == SV_LIGHT_LANTERN;
}

static bool object_is_oil_flask(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx && o_ptr->tval == TV_FLASK;
}

static bool object_uses_light_pickup_limit(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx && player_light_carry_cap(o_ptr) > 0;
}

typedef enum pickup_failure_result
{
    PICKUP_FAILURE_ABORT = 0,
    PICKUP_FAILURE_RETRY,
    PICKUP_FAILURE_EQUIPPED
} pickup_failure_result;

static bool deferred_pickup_drop_pending = false;
static object_type deferred_pickup_drop;
static int deferred_pickup_drop_oil = 0;
static bool deferred_pickup_refill_oil_pool = false;
static bool brass_lamp_pickup_overflow_checked = false;

static void clear_deferred_pickup_drop(void)
{
    deferred_pickup_drop_pending = false;
    object_wipe(&deferred_pickup_drop);
    deferred_pickup_drop_oil = 0;
    deferred_pickup_refill_oil_pool = false;
}

static void drop_object_at_player_feet_or_nearby(object_type* drop)
{
    bool can_drop_here;

    if (!drop || !drop->k_idx || drop->number <= 0)
        return;

    can_drop_here = (cave_feat[p_ptr->py][p_ptr->px] == FEAT_FLOOR
        || cave_feat[p_ptr->py][p_ptr->px] == FEAT_SUNLIGHT);

    if (can_drop_here && floor_carry(p_ptr->py, p_ptr->px, drop) > 0)
        return;

    (void)drop_near(drop, 0, p_ptr->py, p_ptr->px);
}

static int refill_lamp_oil_from_deferred_drop(void)
{
    int current_oil;
    int free_capacity;
    int oil_to_transfer;

    if (!deferred_pickup_refill_oil_pool || deferred_pickup_drop_oil <= 0)
        return 0;

    current_oil = player_lamp_oil();
    free_capacity = player_lamp_oil_capacity() - current_oil;
    if (free_capacity <= 0)
        return 0;

    oil_to_transfer = MIN(deferred_pickup_drop_oil, free_capacity);
    if (oil_to_transfer <= 0)
        return 0;

    if (!player_gain_lamp_oil(oil_to_transfer, false))
        return 0;

    deferred_pickup_drop_oil -= oil_to_transfer;
    return oil_to_transfer;
}

static void flush_deferred_pickup_drop(void)
{
    if (!deferred_pickup_drop_pending)
        return;

    (void)refill_lamp_oil_from_deferred_drop();

    log_debug("pickup replace: flushing deferred drop at (%d,%d) "
        "cave_o_idx=%d tval=%d sval=%d number=%d oil=%d",
        p_ptr->py, p_ptr->px, cave_o_idx[p_ptr->py][p_ptr->px],
        deferred_pickup_drop.tval, deferred_pickup_drop.sval,
        deferred_pickup_drop.number,
        deferred_pickup_drop_oil);

    if (player_oil_container_object(&deferred_pickup_drop)
        && deferred_pickup_drop_oil > 0)
    {
        int oil_remaining = deferred_pickup_drop_oil;
        int unit_capacity =
            player_oil_container_unit_capacity(&deferred_pickup_drop);

        for (int n = 0; n < deferred_pickup_drop.number; n++)
        {
            object_type single_drop;
            object_wipe(&single_drop);
            object_copy(&single_drop, &deferred_pickup_drop);
            single_drop.number = 1;
            player_oil_container_set_fuel(&single_drop,
                MIN(oil_remaining, unit_capacity));
            oil_remaining -= MIN(oil_remaining, unit_capacity);
            drop_object_at_player_feet_or_nearby(&single_drop);
        }
    }
    else
    {
        drop_object_at_player_feet_or_nearby(&deferred_pickup_drop);
    }

    clear_deferred_pickup_drop();
}

static bool queue_deferred_pickup_drop(const object_type* src, int amount,
    int oil_to_drop, bool refill_oil_pool)
{
    if (!src || !src->k_idx || amount <= 0)
        return false;

    if (amount > src->number)
        amount = src->number;

    if (deferred_pickup_drop_pending)
    {
        log_warn("pickup replace: flushing unexpected pre-existing deferred "
            "drop before queueing another");
        flush_deferred_pickup_drop();
    }

    object_wipe(&deferred_pickup_drop);
    object_copy(&deferred_pickup_drop, src);
    deferred_pickup_drop.number = amount;
    deferred_pickup_drop_oil = oil_to_drop;
    deferred_pickup_refill_oil_pool = refill_oil_pool;
    deferred_pickup_drop_pending = true;

    return true;
}

static bool queue_deferred_pickup_supply_drop(int supply_idx, int amount,
    bool refill_oil_pool)
{
    object_type* supply_obj = supplies_entry_at(supply_idx);
    object_type deferred;
    char o_name[80];
    int oil_to_drop = 0;

    if (!supply_obj || !supply_obj->k_idx || amount <= 0)
        return false;

    if (amount > supply_obj->number)
        amount = supply_obj->number;

    if (player_oil_container_object(supply_obj))
    {
        if (!player_prepare_oil_container_drop(supply_obj, amount,
                &oil_to_drop, NULL))
            return false;
    }

    object_wipe(&deferred);
    object_copy(&deferred, supply_obj);
    deferred.number = amount;

    object_desc(o_name, sizeof(o_name), &deferred, true, 3);

    if (player_light_destroyed_on_drop(&deferred))
    {
        msg_format("You discard %s; %s too spent to keep.",
            o_name, (deferred.number > 1) ? "they are" : "it is");
        (void)supplies_consume_quantity(supply_idx, amount);
        p_ptr->redraw |= (PR_MAP | PR_LIGHT);
        p_ptr->window |= (PW_MESSAGE);
        handle_stuff();
        return true;
    }

    if (!queue_deferred_pickup_drop(supply_obj, amount, oil_to_drop,
            refill_oil_pool))
        return false;

    (void)supplies_consume_quantity(supply_idx, amount);

    log_debug("pickup replace: queued deferred supply drop supply_idx=%d "
        "amount=%d tval=%d sval=%d oil=%d",
        supply_idx, amount, deferred_pickup_drop.tval,
        deferred_pickup_drop.sval, deferred_pickup_drop_oil);

    return true;
}

static bool queue_deferred_pickup_pack_drop(int item, int amount,
    bool refill_oil_pool)
{
    object_type* drop_ptr;
    object_type deferred;
    char o_name[80];
    int oil_to_drop = 0;

    if ((item < 0) || (item >= INVEN_PACK) || amount <= 0)
        return false;

    drop_ptr = &inventory[item];
    if (!drop_ptr->k_idx)
        return false;

    if (amount > drop_ptr->number)
        amount = drop_ptr->number;

    object_wipe(&deferred);
    object_copy(&deferred, drop_ptr);
    deferred.number = amount;

    if (player_oil_container_object(&deferred))
    {
        if (!player_prepare_oil_container_drop(&deferred, amount,
                &oil_to_drop, NULL))
            return false;
    }

    object_desc(o_name, sizeof(o_name), &deferred, true, 3);

    if (player_light_destroyed_on_drop(&deferred))
    {
        msg_format("You discard %s; %s too spent to keep.",
            o_name, (deferred.number > 1) ? "they are" : "it is");

        inven_item_increase(item, -amount);
        inven_item_describe(item);
        inven_item_optimize(item);
        p_ptr->redraw |= (PR_MAP | PR_LIGHT);
        p_ptr->window |= (PW_MESSAGE);
        handle_stuff();
        return true;
    }

    if (!queue_deferred_pickup_drop(drop_ptr, amount, oil_to_drop,
            refill_oil_pool))
        return false;

    inven_item_increase(item, -amount);
    inven_item_describe(item);
    inven_item_optimize(item);

    return true;
}

static bool confirm_oil_pickup_overflow_with_bonus(const object_type* o_ptr,
    int oil_amount, int lantern_bonus)
{
    char o_name[80];
    char prompt[160];

    if (!o_ptr || oil_amount <= 0
        || !player_lamp_oil_would_overflow_with_bonus(oil_amount,
            lantern_bonus))
        return true;

    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
    strnfmt(prompt, sizeof(prompt),
        "Adding the oil from %s will waste some oil. Proceed? ", o_name);
    return get_check(prompt);
}

static bool confirm_oil_pickup_overflow(const object_type* o_ptr, int oil_amount)
{
    return confirm_oil_pickup_overflow_with_bonus(o_ptr, oil_amount, 0);
}

static pickup_failure_result prompt_replace_light_limit_item(
    object_type* incoming, int floor_o_idx, const char* incoming_name)
{
    char prompt[160];
    cptr label = inven_carry_limit_label();
    int limit = inven_carry_limit_value();
    bool replaced = false;
    bool old_item_tester_full = item_tester_full;
    byte old_item_tester_tval = item_tester_tval;
    bool (*old_item_tester_hook)(const object_type*) = item_tester_hook;
    const object_type* old_filter = replacement_filter_incoming;
    bool old_expand_supplies = inventory_menu_set_expand_supplies(true);

    extern bool sdl_is_story_font_enabled(void);
    extern void sdl_story_font_disable(void);
    if (sdl_is_story_font_enabled())
        sdl_story_font_disable();

    if (label)
        msg_format("You already carry %s (limit %d).", label, limit);
    else
        msg_print("You cannot carry any more of those.");

    msg_print("Choose an item to replace.");

    strnfmt(prompt, sizeof(prompt),
            "Replace which item to pick up %s? ", incoming_name);

    replacement_filter_incoming = incoming;
    item_tester_tval = 0;
    item_tester_hook = item_tester_limit_group;
    item_tester_full = false;

    while (true)
    {
        int item;
        object_type* drop_ptr = NULL;
        int remove_amt = 1;

        if (!get_item(&item, prompt, "You have nothing to replace.",
            USE_INVEN | USE_EQUIP))
        {
            break;
        }

        if (item >= SUPPLIES_INDEX)
        {
            int supply_idx = item - SUPPLIES_INDEX;
            drop_ptr = supplies_entry_at(supply_idx);

            if (!drop_ptr || !drop_ptr->k_idx)
            {
                bell("That supply entry is empty.");
                continue;
            }
        }
        else
        {
            if ((item < 0) || (item >= INVEN_TOTAL))
            {
                bell("Illegal object choice!");
                continue;
            }

            drop_ptr = &inventory[item];
            if (!drop_ptr->k_idx)
            {
                bell("That slot is empty.");
                continue;
            }

            if ((item >= INVEN_WIELD) && cursed_p(drop_ptr))
            {
                char equipped_name[80];
                object_desc(equipped_name, sizeof(equipped_name), drop_ptr, true,
                    3);
                msg_format("You cannot remove %s.", equipped_name);
                continue;
            }
        }

        if (!inven_carry_limit_can_replace(drop_ptr)
            || !pack_item_matches_replacement_type(incoming, drop_ptr))
        {
            msg_print("That will not make enough room.");
            continue;
        }

        if ((item >= INVEN_WIELD) && (item == wield_slot(incoming)))
        {
            log_debug("pickup light replace: equipping floor item %d directly "
                "into slot %d instead of dropping first", floor_o_idx, item);
            inventory_menu_set_expand_supplies(old_expand_supplies);
            replacement_filter_incoming = old_filter;
            item_tester_hook = old_item_tester_hook;
            item_tester_tval = old_item_tester_tval;
            item_tester_full = old_item_tester_full;
            do_cmd_wield(incoming, 0 - floor_o_idx);
            return PICKUP_FAILURE_EQUIPPED;
        }

        if (player_oil_container_object(incoming)
            && player_oil_container_object(drop_ptr))
        {
            int incoming_cost = player_oil_container_slot_cost(incoming);
            int drop_cost = player_oil_container_slot_cost(drop_ptr);
            int free_slots = player_oil_container_slot_capacity()
                - player_oil_container_slots_used();
            int needed_slots = incoming_cost * MAX(incoming->number, 1)
                - MAX(free_slots, 0);

            remove_amt = MAX(1, (needed_slots + drop_cost - 1) / drop_cost);
        }
        else
        {
            remove_amt = MAX(1,
                incoming->number - player_light_available_capacity(incoming));
        }
        remove_amt = MIN(remove_amt, MAX(drop_ptr->number, 1));

        if (item >= SUPPLIES_INDEX)
        {
            int supply_idx = item - SUPPLIES_INDEX;
            bool refill_oil_pool = player_oil_container_object(incoming)
                && player_oil_container_object(drop_ptr);

            if (floor_o_idx > 0)
            {
                if (!queue_deferred_pickup_supply_drop(supply_idx, remove_amt,
                        refill_oil_pool))
                    continue;
            }
            else
            {
                if (!supplies_drop_amount(supply_idx, remove_amt))
                    continue;
            }
        }
        else
        {
            if ((item < INVEN_WIELD)
                && !queue_deferred_pickup_pack_drop(item, remove_amt,
                    player_oil_container_object(incoming)
                        && player_oil_container_object(drop_ptr)))
            {
                continue;
            }

            if (item >= INVEN_WIELD)
                inven_drop(item, remove_amt);
        }

        p_ptr->notice |= (PN_COMBINE | PN_REORDER);
        notice_stuff();
        replaced = true;
        break;
    }

    inventory_menu_set_expand_supplies(old_expand_supplies);
    replacement_filter_incoming = old_filter;
    item_tester_hook = old_item_tester_hook;
    item_tester_tval = old_item_tester_tval;
    item_tester_full = old_item_tester_full;

    return replaced ? PICKUP_FAILURE_RETRY : PICKUP_FAILURE_ABORT;
}

static bool pickup_brass_lamp(int o_idx, object_type* o_ptr)
{
    int oil_amount;
    int pickup_y;
    int pickup_x;

    if (!object_is_brass_lamp(o_ptr))
        return false;

    if (o_ptr->number != 1)
        return false;

    pickup_y = o_ptr->iy;
    pickup_x = o_ptr->ix;
    oil_amount = MIN(o_ptr->timeout, FUEL_LAMP);

    if (player_light_available_capacity(o_ptr) <= 0)
        return false;

    if (!brass_lamp_pickup_overflow_checked
        && !confirm_oil_pickup_overflow_with_bonus(o_ptr, oil_amount, 1))
    {
        msg_print("You leave it on the ground.");
        return true;
    }

    brass_lamp_pickup_overflow_checked = false;
    player_gain_lamp_oil_with_bonus(oil_amount, true, 1);
    o_ptr->timeout = 0;
    give_player_item(o_ptr);
    (void)player_lamp_oil();

    if (!o_ptr->k_idx || o_ptr->number <= 0)
    {
        if (!o_ptr->k_idx)
        {
            log_debug("pickup_brass_lamp: restoring wiped floor object %d to "
                "(%d,%d) before delete", o_idx, pickup_y, pickup_x);
            o_ptr->iy = pickup_y;
            o_ptr->ix = pickup_x;
        }
        delete_object_idx(o_idx);
    }

    flush_deferred_pickup_drop();

    return true;
}

static bool pickup_brass_lamp_oil_only(object_type* o_ptr)
{
    int oil_amount;

    if (!object_is_brass_lamp(o_ptr) || (o_ptr->number != 1))
        return false;

    oil_amount = MIN(o_ptr->timeout, FUEL_LAMP);
    if ((oil_amount <= 0) || !get_check("Take only the oil? "))
        return false;

    if (!confirm_oil_pickup_overflow(o_ptr, oil_amount))
    {
        msg_print("You leave it on the ground.");
        return true;
    }

    player_gain_lamp_oil(oil_amount, true);
    o_ptr->timeout = 0;
    msg_print("You siphon the oil and leave the lamp behind.");
    return true;
}

static int carried_oil_flask_count(void)
{
    int count = 0;

    for (int i = 0; i < supplies_entry_count(); i++)
    {
        object_type* s_ptr = supplies_entry_at(i);
        if (s_ptr && s_ptr->k_idx && s_ptr->tval == TV_FLASK)
            count += MAX(s_ptr->number, 1);
    }

    for (int i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];
        if (o_ptr->k_idx && o_ptr->tval == TV_FLASK)
            count += MAX(o_ptr->number, 1);
    }

    return count;
}

static int discard_oil_flasks_for_lamp(int needed_slots)
{
    int discarded = 0;

    while (needed_slots > 0)
    {
        bool removed = false;

        for (int i = 0; i < supplies_entry_count(); i++)
        {
            object_type* s_ptr = supplies_entry_at(i);
            if (!s_ptr || !s_ptr->k_idx || s_ptr->tval != TV_FLASK)
                continue;

            (void)supplies_consume_quantity(i, 1);
            discarded++;
            needed_slots--;
            removed = true;
            break;
        }

        if (removed)
            continue;

        for (int i = 0; i < INVEN_PACK; i++)
        {
            object_type* o_ptr = &inventory[i];
            if (!o_ptr->k_idx || o_ptr->tval != TV_FLASK)
                continue;

            inven_item_increase(i, -1);
            inven_item_optimize(i);
            discarded++;
            needed_slots--;
            removed = true;
            break;
        }

        if (!removed)
            break;
    }

    return discarded;
}

static bool carried_brass_lamps_fill_oil_storage(void)
{
    return player_carried_light_count_for_sval(SV_LIGHT_LANTERN)
        * PLAYER_BRASS_LAMP_SLOT_COST >= PLAYER_OIL_CONTAINER_SLOT_CAP;
}

static int oil_container_pickup_oil_amount(const object_type* o_ptr)
{
    int unit_oil;

    if (!o_ptr || !o_ptr->k_idx)
        return 0;

    if (object_is_brass_lamp(o_ptr))
        unit_oil = MIN(o_ptr->timeout, FUEL_LAMP);
    else if (object_is_oil_flask(o_ptr))
        unit_oil = MIN(o_ptr->pval, FUEL_FLASK);
    else
        return 0;

    if (unit_oil < 0)
        unit_oil = 0;

    return unit_oil * MAX(o_ptr->number, 1);
}

static int oil_flask_units_oil_amount(const object_type* o_ptr, int amount)
{
    int unit_oil;

    if (!object_is_oil_flask(o_ptr) || amount <= 0)
        return 0;

    unit_oil = MIN(o_ptr->pval, FUEL_FLASK);
    if (unit_oil < 0)
        unit_oil = 0;

    return unit_oil * MIN(amount, MAX(o_ptr->number, 1));
}

static bool pickup_oil_flask_oil_only(int o_idx, object_type* o_ptr)
{
    int oil_amount;

    if (!object_is_oil_flask(o_ptr))
        return false;

    if (!carried_brass_lamps_fill_oil_storage())
        return false;

    oil_amount = oil_container_pickup_oil_amount(o_ptr);
    if (oil_amount > 0 && !confirm_oil_pickup_overflow(o_ptr, oil_amount))
    {
        msg_print("You leave it on the ground.");
        return true;
    }

    if (oil_amount > 0)
    {
        player_gain_lamp_oil(oil_amount, true);
        (void)player_lamp_oil();
        msg_format("You pour the oil into your lamp stores and discard the "
            "flask%s.", (MAX(o_ptr->number, 1) == 1) ? "" : "s");
    }
    else
    {
        msg_format("You discard the empty oil flask%s.",
            (MAX(o_ptr->number, 1) == 1) ? "" : "s");
    }

    delete_object_idx(o_idx);
    p_ptr->redraw |= (PR_MAP | PR_LIGHT);
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    return true;
}

static int oil_flasks_replaced_for_lamp_oil_amount(int needed_slots)
{
    int oil_amount = 0;

    if (needed_slots <= 0)
        return 0;

    for (int i = 0; i < supplies_entry_count() && needed_slots > 0; i++)
    {
        object_type* s_ptr = supplies_entry_at(i);
        int amount;

        if (!object_is_oil_flask(s_ptr))
            continue;

        amount = MIN(needed_slots, MAX(s_ptr->number, 1));
        oil_amount += oil_flask_units_oil_amount(s_ptr, amount);
        needed_slots -= amount;
    }

    for (int i = 0; i < INVEN_PACK && needed_slots > 0; i++)
    {
        object_type* o_ptr = &inventory[i];
        int amount;

        if (!object_is_oil_flask(o_ptr))
            continue;

        amount = MIN(needed_slots, MAX(o_ptr->number, 1));
        oil_amount += oil_flask_units_oil_amount(o_ptr, amount);
        needed_slots -= amount;
    }

    return oil_amount;
}

static bool brass_lamp_pickup_oil_would_overflow_after_discard(
    const object_type* incoming, int discarded_flasks, int discarded_flask_oil)
{
    int oil_amount;
    int resulting_slots;
    int resulting_capacity;
    int current_oil;

    if (!object_is_brass_lamp(incoming))
        return false;

    oil_amount = oil_container_pickup_oil_amount(incoming);
    oil_amount += discarded_flask_oil;
    if (oil_amount <= 0)
        return false;

    resulting_slots = player_oil_container_slots_used()
        - discarded_flasks * PLAYER_OIL_FLASK_SLOT_COST
        + player_oil_container_slot_cost(incoming) * MAX(incoming->number, 1);
    if (resulting_slots < 0)
        resulting_slots = 0;
    if (resulting_slots > PLAYER_OIL_CONTAINER_SLOT_CAP)
        resulting_slots = PLAYER_OIL_CONTAINER_SLOT_CAP;

    resulting_capacity = resulting_slots * FUEL_FLASK;
    current_oil = p_ptr ? p_ptr->lamp_oil : 0;
    if (current_oil < 0)
        current_oil = 0;

    return current_oil + oil_amount > resulting_capacity;
}

static bool auto_replace_flasks_for_brass_lamp(const object_type* incoming,
    bool* aborted)
{
    int incoming_slots;
    int needed_slots;
    int discarded_flask_oil;
    int discarded;

    if (aborted)
        *aborted = false;

    if (!object_is_brass_lamp(incoming))
        return false;

    incoming_slots = player_oil_container_slot_cost(incoming)
        * MAX(incoming->number, 1);
    needed_slots = player_oil_container_slots_used() + incoming_slots
        - player_oil_container_slot_capacity();

    if (needed_slots <= 0)
        return false;

    if (carried_oil_flask_count() < needed_slots)
        return false;

    discarded_flask_oil = oil_flasks_replaced_for_lamp_oil_amount(needed_slots);

    if (brass_lamp_pickup_oil_would_overflow_after_discard(incoming,
            needed_slots, discarded_flask_oil))
    {
        if (!confirm_oil_pickup_overflow(incoming,
                oil_container_pickup_oil_amount(incoming)
                    + discarded_flask_oil))
        {
            if (aborted)
                *aborted = true;
            msg_print("You leave it on the ground.");
            return false;
        }

        brass_lamp_pickup_overflow_checked = true;
    }

    discarded = discard_oil_flasks_for_lamp(needed_slots);
    if (discarded > 0)
    {
        brass_lamp_pickup_overflow_checked = true;
        if (discarded_flask_oil > 0)
            player_gain_lamp_oil(discarded_flask_oil, true);
        msg_format("Your lamp replaces %d oil flask%s, keeping the oil in "
            "your lamp stores.",
            discarded, (discarded == 1) ? "" : "s");
        p_ptr->notice |= (PN_COMBINE | PN_REORDER);
        p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    }

    return discarded > 0;
}

/*
 * Helper routine for py_pickup() and py_pickup_floor().
 *
 * Add the given dungeon object to the character's inventory.
 *
 * Delete the object afterwards.
 */
static bool prepare_floor_object_for_pickup(int o_idx, object_type* o_ptr);

void py_pickup_aux(int o_idx)
{
    object_type* o_ptr;
    char o_name[120];
    
    o_ptr = &o_list[o_idx];

    if (object_is_searched_skeleton(o_ptr))
        return;

    // Remember the floor position even if give_player_item wipes the object
    int pickup_y = o_ptr->iy;
    int pickup_x = o_ptr->ix;

    /*hack - don't pickup &nothings*/
    if (o_ptr->k_idx)
    {
        if (!prepare_floor_object_for_pickup(o_idx, o_ptr))
        {
            flush_deferred_pickup_drop();
            return;
        }

        if (pickup_brass_lamp(o_idx, o_ptr))
            return;

        /* Check for Oath of the Smith violation */
        if (smith_oath_forbids_object(o_ptr))
        {
            if (!smith_oath_confirm_break())
            {
                flush_deferred_pickup_drop();
                return;
            }
        }

        /* Check for supply items with partial pickup option */
        if (supplies_is_supply_object(o_ptr) && o_ptr->number > 1)
        {
            int max_qty = supplies_max_absorbable_quantity(o_ptr);
            
            /* If we can't absorb all of it but can absorb some, offer partial pickup */
            if (max_qty > 0 && max_qty < o_ptr->number)
            {
                object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
                
                char prompt[160];
                strnfmt(prompt, sizeof(prompt), 
                        "Your supply cache can only hold %d of %d. Pick up how many? (0-%d): ",
                        max_qty, o_ptr->number, max_qty);
                
                int qty = get_quantity(prompt, max_qty);
                
                if (qty <= 0)
                {
                    msg_print("You leave it on the ground.");
                    flush_deferred_pickup_drop();
                    return;
                }
                
                /* Create a partial object to pick up */
                object_type partial;
                object_copy(&partial, o_ptr);
                partial.number = qty;
                
                give_player_item(&partial);
                
                /* Reduce the floor object */
                o_ptr->number -= qty;
                
                /* Break the truce if creatures see */
                break_truce(false);

                flush_deferred_pickup_drop();
                
                return;
            }
        }
        
        give_player_item(o_ptr);

        // Break the truce if creatures see
        break_truce(false);

        if (!o_ptr->k_idx || o_ptr->number <= 0)
        {
            if (!o_ptr->k_idx)
            {
                o_ptr->iy = pickup_y;
                o_ptr->ix = pickup_x;
            }
            delete_object_idx(o_idx);
        }

        flush_deferred_pickup_drop();

        return;
    }

    /* Delete the object */
    o_ptr->iy = pickup_y;
    o_ptr->ix = pickup_x;
    delete_object_idx(o_idx);
    flush_deferred_pickup_drop();
}

/*
 * Allow the player to sort through items in a pile and
 * pickup what they want.  This command does not use
 * any energy because it costs a player no extra energy
 * to walk into a grid and automatically pick up items
 */
void do_cmd_pickup_from_pile(void)
{
    bool picked_up_item = false;

    /*
     * Loop through and pick up objects until escape is hit or the backpack
     * can't hold anything else.
     */
    while (true)
    {
        int item;

        char prompt[80];

        int floor_list[MAX_FLOOR_STACK];

        int floor_num;

        /*start with everything updated*/
        handle_stuff();

        /* Scan for floor objects */
        floor_num = scan_floor(
            floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px, 0x01);

        /* No pile */
        if (floor_num < 1)
        {
            if (picked_up_item)
                msg_format("There are no more objects where you are standing.");
            else
                msg_format("There are no objects where you are standing.");
            break;
        }

        /* Save screen */
        screen_save();

        /* Display */
        show_floor(floor_list, floor_num);

        SDL_strlcpy(
            prompt, "Pick up which object? (ESC to cancel):", sizeof(prompt));

        /* Get the object number to be bought */
        item = get_menu_choice(floor_num, prompt);

        /*player chose escape*/
        if (item == -1)
        {
            screen_load();
            break;
        }

        /* Pick up the object */
        py_pickup_aux(floor_list[item]);

        /*Mark that we picked something up*/
        picked_up_item = true;

        /* Load screen */
        screen_load();
    }

    /* Combine / Reorder the pack */
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Update quiver display if needed */
    p_ptr->redraw |= (PR_QUIVER);

    /* Just be sure all inventory management is done. */
    notice_stuff();
}

static void report_pack_limit_failure(const char* o_name, bool still)
{
    if (inven_carry_limit_failed())
    {
        cptr label = inven_carry_limit_label();
        int limit = inven_carry_limit_value();

        if (label)
        {
            /* Special message for supply weight limit */
            if (strcmp(label, "supply weight") == 0)
            {
                msg_format("Your supply cache cannot carry any more weight (limit %d lbs).",
                           limit);
                return;
            }

            if (still)
                msg_format("Your pack still cannot hold more %s (limit %d).", label,
                           limit);
            else
                msg_format("Your pack cannot hold more %s (limit %d).", label, limit);
            return;
        }
    }

    if (still)
        msg_format("You still have no room for %s.", o_name);
    else
        msg_format("You have no room for %s.", o_name);
}

static bool item_tester_limit_group(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (inven_carry_limit_is_supply_weight())
        return inven_carry_limit_can_replace(o_ptr);

    if (replacement_filter_incoming
        && !pack_item_matches_replacement_type(replacement_filter_incoming, o_ptr))
        return false;

    return inven_carry_limit_can_replace(o_ptr);
}

static int supply_weight_replacement_amount(const object_type* incoming,
                                            const object_type* candidate)
{
    int incoming_weight;
    int over_limit;
    int amount;

    if (!incoming || !candidate || !candidate->k_idx)
        return 0;

    if (!supplies_weight_counts_to_limit(incoming)
        || !supplies_weight_counts_to_limit(candidate))
    {
        return 0;
    }

    if (incoming->weight <= 0 || candidate->weight <= 0)
        return 0;

    incoming_weight = incoming->weight * MAX(incoming->number, 1);
    over_limit = supplies_limit_weight() + incoming_weight
        - supplies_current_weight_cap();

    if (over_limit <= 0)
        return 1;

    amount = (over_limit + candidate->weight - 1) / candidate->weight;
    return MIN(amount, MAX(candidate->number, 1));
}

static bool prompt_replace_pack_item_limit(const object_type* incoming,
                                           const char* incoming_name)
{
    char prompt[160];
    cptr label = inven_carry_limit_label();
    int limit = inven_carry_limit_value();
    bool replaced = false;
    bool supply_weight_limit = inven_carry_limit_is_supply_weight();

    bool old_item_tester_full = item_tester_full;
    byte old_item_tester_tval = item_tester_tval;
    bool (*old_item_tester_hook)(const object_type*) = item_tester_hook;
    const object_type* old_filter = replacement_filter_incoming;
    bool old_expand_supplies =
        inventory_menu_set_expand_supplies(supply_weight_limit);

    /* Ensure story font is disabled before showing messages */
    extern bool sdl_is_story_font_enabled(void);
    extern void sdl_story_font_disable(void);
    if (sdl_is_story_font_enabled())
        sdl_story_font_disable();

    if (label)
        msg_format("You already carry %s (limit %d).", label, limit);
    else
        msg_print("You cannot carry any more of those.");

    msg_print("Choose an item to replace.");

    strnfmt(prompt, sizeof(prompt),
            "Replace which item to pick up %s? ", incoming_name);

    replacement_filter_incoming = incoming;
    item_tester_tval = 0;
    item_tester_hook = item_tester_limit_group;
    item_tester_full = false;

    while (true)
    {
        int item;
        object_type* drop_ptr = NULL;
        int remove_amt = 0;

        if (!get_item(&item, prompt, "You have nothing to replace.", USE_INVEN))
            break;

        if (item >= SUPPLIES_INDEX)
        {
            int supply_idx = item - SUPPLIES_INDEX;
            drop_ptr = supplies_entry_at(supply_idx);

            if (!drop_ptr || !drop_ptr->k_idx)
            {
                bell("That supply entry is empty.");
                continue;
            }
        }
        else if ((item < 0) || (item >= INVEN_PACK))
        {
            bell("Illegal object choice!");
            continue;
        }
        else
        {
            drop_ptr = &inventory[item];

            if (!drop_ptr->k_idx)
            {
                bell("That slot is empty.");
                continue;
            }
        }

        if (!inven_carry_limit_can_replace(drop_ptr))
        {
            msg_print("That will not make enough room.");
            continue;
        }

        if (supply_weight_limit)
        {
            if (item < SUPPLIES_INDEX)
            {
                msg_print("That will not make enough room.");
                continue;
            }

            remove_amt = supply_weight_replacement_amount(incoming, drop_ptr);
            if (remove_amt <= 0)
            {
                msg_print("That will not make enough room.");
                continue;
            }

            if (!queue_deferred_pickup_supply_drop(item - SUPPLIES_INDEX,
                    remove_amt, false))
            {
                continue;
            }
        }
        else
        {
            if (item >= SUPPLIES_INDEX)
            {
                msg_print("That will not make enough room.");
                continue;
            }

            if (!queue_deferred_pickup_pack_drop(item, drop_ptr->number, false))
                continue;
        }

        p_ptr->notice |= (PN_COMBINE | PN_REORDER);
        notice_stuff();

        replaced = true;
        break;
    }

    replacement_filter_incoming = old_filter;
    item_tester_hook = old_item_tester_hook;
    item_tester_tval = old_item_tester_tval;
    item_tester_full = old_item_tester_full;
    inventory_menu_set_expand_supplies(old_expand_supplies);

    return replaced;
}

static pickup_failure_result handle_zero_limit_pickup(object_type* incoming,
                                                      int floor_o_idx,
                                                      const char* incoming_name)
{
    int slot = wield_slot(incoming);

    msg_format("You cannot carry %s in your pack.", incoming_name);

    if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
    {
        msg_print("It does not fit anywhere on your body.");
        return PICKUP_FAILURE_ABORT;
    }

    object_type* equip_ptr = &inventory[slot];

    if (!equip_ptr->k_idx)
    {
        if (get_check("Wear it now? "))
        {
            do_cmd_wield(incoming, 0 - floor_o_idx);
            return PICKUP_FAILURE_EQUIPPED;
        }

        msg_print("You leave it on the ground.");
        return PICKUP_FAILURE_ABORT;
    }

    if (cursed_p(equip_ptr))
    {
        char equipped_name[80];
        object_desc(equipped_name, sizeof(equipped_name), equip_ptr, true, 3);
        msg_format("You cannot remove %s.", equipped_name);
        return PICKUP_FAILURE_ABORT;
    }

    screen_save();
    show_equip();
    msg_print(NULL);
    screen_load();

    char equipped_name[80];
    object_desc(equipped_name, sizeof(equipped_name), equip_ptr, true, 3);

    char prompt[160];
    strnfmt(prompt, sizeof(prompt), "Replace %s with %s? ", equipped_name,
            incoming_name);

    if (get_check(prompt))
    {
        do_cmd_wield(incoming, 0 - floor_o_idx);
        return PICKUP_FAILURE_EQUIPPED;
    }

    msg_print("You decide to keep your current equipment.");
    return PICKUP_FAILURE_ABORT;
}

static pickup_failure_result handle_group_limit_pickup(object_type* incoming,
                                                       int floor_o_idx,
                                                       const char* incoming_name)
{
    if (object_uses_light_pickup_limit(incoming))
        return prompt_replace_light_limit_item(incoming, floor_o_idx,
            incoming_name);

    if (!prompt_replace_pack_item_limit(incoming, incoming_name))
        return PICKUP_FAILURE_ABORT;

    return PICKUP_FAILURE_RETRY;
}

static pickup_failure_result resolve_pickup_failure(object_type* incoming,
                                                    int floor_o_idx,
                                                    const char* incoming_name,
                                                    bool attempted_replacement)
{
    bool has_lamp_oil_fallback = object_is_brass_lamp(incoming)
        && (incoming->number == 1) && (incoming->timeout > 0);

    if (inven_carry_limit_failed())
    {
        if (inven_carry_limit_value() <= 0)
            return handle_zero_limit_pickup(incoming, floor_o_idx,
                                            incoming_name);

        pickup_failure_result limit_result =
            handle_group_limit_pickup(incoming, floor_o_idx, incoming_name);

        if ((limit_result == PICKUP_FAILURE_ABORT) && !has_lamp_oil_fallback)
            report_pack_limit_failure(incoming_name, attempted_replacement);

        return limit_result;
    }

    if (prompt_replace_pack_item(incoming))
        return PICKUP_FAILURE_RETRY;

    if (!has_lamp_oil_fallback)
        report_pack_limit_failure(incoming_name, attempted_replacement);

    return PICKUP_FAILURE_ABORT;
}

static bool prepare_floor_object_for_pickup(int o_idx, object_type* o_ptr)
{
    char o_name[120];
    bool attempted_replacement = false;
    bool pickup_aborted = false;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    brass_lamp_pickup_overflow_checked = false;
    object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    if (pickup_oil_flask_oil_only(o_idx, o_ptr))
        return false;

    auto_replace_flasks_for_brass_lamp(o_ptr, &pickup_aborted);
    if (pickup_aborted)
        return false;

    while (!inven_carry_okay(o_ptr))
    {
        pickup_failure_result failure = resolve_pickup_failure(
            o_ptr, o_idx, o_name, attempted_replacement);

        if (failure == PICKUP_FAILURE_RETRY)
        {
            attempted_replacement = true;
            continue;
        }

        if (failure == PICKUP_FAILURE_EQUIPPED)
            return false;

        if (pickup_brass_lamp_oil_only(o_ptr))
        {
            flush_deferred_pickup_drop();
            return false;
        }

        flush_deferred_pickup_drop();
        return false;
    }

    return true;
}

void py_pickup(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;
    bool done_pickup = false;

    s16b this_o_idx, next_o_idx = 0;

    object_type* o_ptr;

    char o_name[80];

    /* Automatically destroy squelched items in pile if necessary */
    do_squelch_pile(py, px);

    /* Scan the pile of objects */
    for (this_o_idx = cave_o_idx[py][px]; this_o_idx; this_o_idx = next_o_idx)
    {
        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        if (object_is_searched_skeleton(o_ptr))
            continue;

        /* Describe the object */
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

        /* Hack -- disturb */
        disturb(0, 0);

        /* End loop if squelched stuff reached */
        if ((k_info[o_ptr->k_idx].squelch == SQUELCH_ALWAYS)
            && (k_info[o_ptr->k_idx].aware))
        {
            next_o_idx = 0;
            continue;
        }

        bool skip_current_item = false;

        if (p_ptr->active_ability[S_WIL][WIL_CHANNELING] && o_ptr->tval == TV_STAFF && o_ptr->pval > 0)
        {
            int target_slot = -1;
            object_type* target = NULL;

            object_type* wielded = &inventory[INVEN_STAFF];
            if (wielded->k_idx && wielded->k_idx == o_ptr->k_idx)
            {
                target = wielded;
                target_slot = INVEN_STAFF;
            }

            if (!target)
            {
                for (int i = 0; i < INVEN_PACK; i++)
                {
                    object_type* pack_obj = &inventory[i];
                    if (!pack_obj->k_idx)
                        continue;
                    if (pack_obj->tval != TV_STAFF)
                        continue;
                    if (pack_obj->k_idx != o_ptr->k_idx)
                        continue;
                    target = pack_obj;
                    target_slot = i;
                    break;
                }
            }

            if (target)
            {
                int mult = CHANNELING_CHARGE_MULTIPLIER;
                int existing_raw = MAX(target->pval, 0);
                int donor_raw = MAX(o_ptr->pval, 0);
                int existing_uses = existing_raw / mult;
                int donor_uses = donor_raw / mult;
                if (donor_uses > 0)
                {
                    double existing_term = pow((double)existing_uses, 1.5);
                    double donor_term = pow((double)donor_uses, 1.5);
                    double combined_uses_raw = 0.0;
                    double sum_terms = existing_term + donor_term;
                    if (sum_terms > 0.0)
                        combined_uses_raw = pow(sum_terms, 2.0 / 3.0);
                    int combined_uses = (int)(combined_uses_raw + 0.5);
                    long combined_pval = (long)combined_uses * mult;
                    long max_pval = (long)(32767 / mult) * mult;
                    if (combined_pval > max_pval)
                        combined_pval = max_pval;
                    combined_uses = (int)(combined_pval / mult);
                    int gain_uses = combined_uses - existing_uses;
                    if (gain_uses > 0)
                    {
                        char target_name[80];
                        char donor_name[80];
                        char prompt[120];
                        format_staff_prompt_name(
                            target_name, sizeof(target_name), target, false);
                        format_staff_prompt_name(
                            donor_name, sizeof(donor_name), o_ptr, true);
                        
                        log_debug("Channeling: donor floor staff k_idx=%d pval=%d number=%d, target inv slot %d k_idx=%d pval=%d number=%d",
                                  o_ptr->k_idx, o_ptr->pval, o_ptr->number,
                                  target_slot, target->k_idx, target->pval, target->number);
                        
                        strnfmt(prompt, sizeof(prompt),
                            "Channel %s into your %s (%d charges)?",
                            donor_name, target_name, combined_uses);
                        if (get_check(prompt))
                        {
                            target->pval = (s16b)combined_pval;
                            target->ident &= ~(IDENT_EMPTY);
                            o_ptr->pval = 0;
                            o_ptr->ident |= IDENT_EMPTY;
                            
                            log_debug("Channeling complete: target now has pval=%d number=%d, donor has pval=%d number=%d",
                                      target->pval, target->number, o_ptr->pval, o_ptr->number);
                            
                            if (target_slot >= 0 && target_slot < INVEN_TOTAL)
                                inven_item_charges(target_slot);
                            p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST);
                            p_ptr->window |= (PW_EQUIP | PW_PLAYER_0 | PW_INVEN);
                            msg_format("You channel %d charge%s into your %s (now %d).",
                                gain_uses, (gain_uses == 1) ? "" : "s",
                                target_name, combined_uses);
                            delete_object_idx(this_o_idx);
                            
                            log_debug("Channeling: deleted floor object idx %d", this_o_idx);
                            
                            done_pickup = true;
                            p_ptr->previous_action[0] = ACTION_MISC;
                            p_ptr->energy_use = 100;
                            skip_current_item = true;
                        }
                    }
                }
            }
        }

        if (skip_current_item)
            continue;

        // Check whether it would be too heavy
        if (p_ptr->total_weight + o_ptr->weight > weight_limit() * 3 / 2)
        {
            if (o_ptr->k_idx)
                msg_format("You cannot lift %s.", o_name);

            /* Check the next object */
            continue;
        }

        // store the action type
        p_ptr->previous_action[0] = ACTION_MISC;

        /* Take a turn */
        p_ptr->energy_use = 100;

        /* Pick up the object */
        py_pickup_aux(this_o_idx);

        done_pickup = true;
    }

    if (!done_pickup)
    {
        p_ptr->previous_action[0] = ACTION_NOTHING;
        p_ptr->energy_use = 0;
    }
}

/*
 * Determine if a trap affects the player.
 * Based on player's evasion.
 */
extern bool check_hit(int power, bool display_roll)
{
    if (hit_roll(power, p_ptr->skill_use[S_EVN] + dodging_bonus(), NULL, PLAYER,
            display_roll)
        > 0)
        return (true);
    else
        return (false);
}

/*
 * Handle player hitting a real trap
 */
void hit_trap(int y, int x)
{
    int i, dam, prt, net_dam;
    int feat = cave_feat[y][x];

    cptr name = "a trap";

    /* Disturb the player */
    disturb(0, 0);

    // Store information for the combat rolls window
    combat_roll_special_char = (&f_info[feat])->d_char;
    combat_roll_special_attr = (&f_info[feat])->d_attr;

    if (p_ptr->avoid_traps && feat != FEAT_CHASM && feat != FEAT_TRAP_ROOST
        && feat != FEAT_TRAP_WEB && feat != FEAT_TRAP_PIT
        && feat != FEAT_TRAP_SPIKED_PIT)
    {
        msg_print("You carefully avoid a trap.");
        reveal_trap(y, x);
        ident_f3(TR3_AVOID_TRAPS, NULL);
        return;
    }

    /* Analyze XXX XXX XXX */
    switch (feat)
    {
        // not really a trap, but handled here due to similarities
    case FEAT_CHASM:
    {
        // give several messages so the player has a chance to see it happen
        msg_print("You fall into the darkness!");
        message_flush();
        if (p_ptr->depth >= MORGOTH_DEPTH)
        {
            msg_print("...and plunge into the abyss.");
            message_flush();

            // add to the notes file
            do_cmd_note("Fell into a chasm", p_ptr->depth);

            // chasms on the final level are fatal
            killer_mark_other(SCORE_KILLER_FALL);
            take_hit(p_ptr->chp + 1000, "falling into the abyss");
        }
        else
        {
            msg_print("...and land somewhere deeper in the Iron Hells.");
            message_flush();

            // add to the notes file
            do_cmd_note("Fell into a chasm", p_ptr->depth);

            // take some damage
            falling_damage(false);

            varda_quest_fail_if_bastion_missed();

            // make a note if the player loses a greater vault
            note_lost_greater_vault();

            /* New depth */
            p_ptr->depth = MIN(p_ptr->depth + 2, MORGOTH_DEPTH);

            /* Leaving */
            p_ptr->leaving = true;
        }

        break;
    }

    case FEAT_TRAP_false_FLOOR:
    {
        // give several messages so the player has a chance to see it happen
        msg_print("The floor crumbles beneath you!");
        message_flush();
        msg_print("You fall through...");
        message_flush();
        msg_print("...and land somewhere deeper in the Iron Hells.");
        message_flush();

        // add to the notes file
        do_cmd_note("Fell through a false floor", p_ptr->depth);

        // take some damage
        falling_damage(false);

        varda_quest_fail_if_bastion_missed();

        // make a note if the player loses a greater vault
        note_lost_greater_vault();

        /* New depth */
        p_ptr->depth++;

        /* Leaving */
        p_ptr->leaving = true;

        break;
    }

    case FEAT_TRAP_PIT:
    {
        msg_print("You fall into a pit!");

        /* Falling damage */
        dam = damroll(2, 4);

        update_combat_rolls1b(NULL, PLAYER, true);
        update_combat_rolls2(2, 4, dam, -1, -1, 0, 0, GF_HURT, false);

        /* Take the damage */
        killer_mark_other(SCORE_KILLER_FALL);
        take_hit(dam, name);

        /* Make some noise */
        stealth_score -= 5;

        break;
    }

    case FEAT_TRAP_SPIKED_PIT:
    {
        msg_print("You fall into a spiked pit!");

        /* Falling damage */
        dam = damroll(2, 4);

        update_combat_rolls1b(NULL, PLAYER, true);
        update_combat_rolls2(2, 4, dam, -1, -1, 0, 0, GF_HURT, false);

        /* Take the damage */
        killer_mark_other(SCORE_KILLER_FALL);
        take_hit(dam, name);

        /* Extra spike damage */
        dam = damroll(4, 5);

        /* Protection */
        prt = protection_roll(GF_HURT, true);

        net_dam = (dam - prt > 0) ? (dam - prt) : 0;

        update_combat_rolls1b(NULL, PLAYER, true);
        update_combat_rolls2(4, 5, dam, -1, -1, prt, 100, GF_HURT, true);

        if (net_dam > 0)
        {
            msg_print("You are impaled!");

            /* Take the damage */
            killer_mark_other(SCORE_KILLER_TRAP);
            take_hit(net_dam, name);

            (void)set_cut(p_ptr->cut + (net_dam + 1) / 2);
        }
        else
        {
            msg_print("Your armour protects you.");
        }

        /* Make some noise */
        stealth_score -= 10;

        break;
    }

    case FEAT_TRAP_DART:
    {
        sound(MSG_TRAP_NEEDLE);

        if (check_hit(15, true))
        {
            dam = damroll(1, 15);
            prt = protection_roll(GF_HURT, false);

            if (dam > prt)
            {
                msg_print("A small dart hits you!");

                // do a tiny amount of damage
                killer_mark_other(SCORE_KILLER_TRAP);
                take_hit(1, name);

                update_combat_rolls2(
                    1, 15, prt + 1, -1, -1, prt, 100, GF_HURT, false);

                (void)do_dec_stat(A_STR, NULL);
            }
            else
            {
                msg_print(
                    "A small dart hits you, but is deflected by your armour.");

                update_combat_rolls2(
                    1, 15, dam, -1, -1, prt, 100, GF_HURT, false);
            }
        }
        else
        {
            msg_print("A small dart barely misses you.");
        }

        /* Make a small amount of noise */
        monster_perception(true, false, 5);

        break;
    }

    case FEAT_TRAP_FLASH:
    {
        if (!p_ptr->blind)
        {
            msg_print("There is a searing flash of light!");
            if (allow_player_blind(NULL))
            {
                (void)set_blind(p_ptr->blind + damroll(5, 4));
            }
            else
            {
                msg_print("Your vision quickly clears.");
            }
        }

        /* Make a small amount of noise */
        monster_perception(true, false, 5);

        break;
    }

    case FEAT_TRAP_GAS_CONF:
    {
        sound(MSG_TRAP_GAS);

        msg_print("A vapor fills the air and you feel yourself becoming "
                  "lightheaded.");
        if (allow_player_confusion(NULL))
        {
            (void)set_confused(p_ptr->confused + damroll(4, 4));
        }
        else
        {
            msg_print("You resist the effects!");
        }
        explosion(-1, 1, y, x, 3, 4, 10, GF_CONFUSION);

        /* Make a small amount of noise */
        monster_perception(true, false, 10);

        break;
    }

    case FEAT_TRAP_GAS_MEMORY:
    {
        sound(MSG_TRAP_GAS);

        msg_print("You are surrounded by a strange mist!");
        if (saving_throw(NULL, 0))
        {
            msg_print("You resist the effects!");
        }
        else
        {
            msg_print("Your memories fade away.");
            wiz_dark();
        }

        // Aesthetic explosion that does nothing
        explosion(-1, 1, y, x, 0, 0, 0, GF_NOTHING);

        /* Make a small amount of noise */
        monster_perception(true, false, 10);

        break;
    }

    case FEAT_TRAP_ACID:
    {
        msg_print("You are splashed with acid!");

        /* Acid damage */
        dam = damroll(4, 4);

        /* Protection */
        prt = protection_roll(GF_HURT, false);

        net_dam = (dam - prt > 0) ? (dam - prt) : 0;

        update_combat_rolls1b(NULL, PLAYER, true);
        update_combat_rolls2(4, 4, dam, -1, -1, prt, 100, GF_HURT, false);

        acid_dam(dam, 4, 16, net_dam, "an acid trap");

        /* Make a small amount of noise */
        monster_perception(true, false, 10);

        break;
    }

    case FEAT_TRAP_IMPRISONMENT:
    {
        msg_print("Words of imprisonment echo through the halls!");
        (void)lock_doors_radius(y, x, 10, 10 + (p_ptr->depth / 2));

        break;
    }

    case FEAT_TRAP_ALARM:
    {
        if (singing(SNG_SILENCE))
        {
            msg_print("You hear the muffled toll of a bell above your head.");
        }
        else
        {
            msg_print("You hear a bell toll loudly above your head.");
        }

        /* Make a lot of noise */
        monster_perception(true, false, -20);

        break;
    }

    case FEAT_TRAP_CALTROPS:
    {
        if (skill_check(PLAYER, p_ptr->skill_use[S_PER], 10, NULL) > 0)
        {
            msg_print("You step carefully amidst a field of caltrops.");
        }
        else
        {
            msg_print("You step on a caltrop.");

            dam = damroll(1, 4);

            update_combat_rolls1b(NULL, PLAYER, true);
            update_combat_rolls2(1, 4, dam, -1, -1, 0, 0, GF_HURT, true);

            killer_mark_other(SCORE_KILLER_TRAP);
            take_hit(dam, name);

            if (allow_player_slow(NULL))
            {
                msg_print("It pierces your foot.");
                set_slow(p_ptr->slow + damroll(4, 4));
            }
        }

        /* Make some noise */
        stealth_score -= 10;

        break;
    }

    case FEAT_TRAP_ROOST:
    {
        int count = 0;

        for (i = 0; i < 1000; i++)
        {
            if (count < 2)
            {
                count += summon_specific(y, x,
                    p_ptr->depth + damroll(2, 2) - damroll(2, 2),
                    SUMMON_BIRD_BAT);
            }
        }

        if (count >= 1)
        {
            msg_print("There is a flutter of wings from high above.");

            /* Forget the trap */
            cave_info[y][x] &= ~(CAVE_MARK);

            /* Remove the trap */
            cave_set_feat(y, x, FEAT_FLOOR);
        }

        break;
    }

    case FEAT_TRAP_WEB:
    {
        int count = 0;

        msg_print("You are caught in a vast black web.");

        for (i = 0; i < 1000; i++)
        {
            if (count < 1)
            {
                count += summon_specific(y, x,
                    p_ptr->depth + damroll(2, 2) - damroll(2, 2),
                    SUMMON_SPIDER);
            }
        }

        if (count >= 1)
        {
            msg_print("A spider descends from the gloom.");
        }

        break;
    }

    case FEAT_TRAP_DEADFALL:
    {
        int yy, xx;
        int sy = y; // to soothe compiler warnings
        int sx = x; // to soothe compiler warnings
        int sn = 0;

        msg_print("The ceiling collapses!");

        /* Check around the player */
        for (i = 0; i < 8; i++)
        {
            /* Get the location */
            yy = p_ptr->py + ddy_ddd[i];
            xx = p_ptr->px + ddx_ddd[i];

            /* Skip non-empty grids */
            if (!cave_empty_bold(yy, xx))
                continue;

            /* Count "safe" grids, apply the randomizer */
            if ((++sn > 1) && (rand_int(sn) != 0))
                continue;

            /* Save the safe location */
            sy = yy;
            sx = xx;
        }

        /* Hurt the player a lot */
        if (!sn)
        {
            /* Message and damage */
            msg_print("You are severely crushed!");
            dam = damroll(6, 8);

            /* Protection */
            prt = protection_roll(GF_HURT, false);

            net_dam = (dam - prt > 0) ? (dam - prt) : 0;

            update_combat_rolls1b(NULL, PLAYER, true);
            update_combat_rolls2(6, 8, dam, -1, -1, prt, 100, GF_HURT, false);

            if (allow_player_stun(NULL))
            {
                (void)set_stun(p_ptr->stun + dam * 4);
            }
        }

        /* Destroy the grid, and push the player to safety */
        else
        {
            /* Calculate results */
            if (check_hit(20, true))
            {
                msg_print("You are struck by rubble!");
                dam = damroll(4, 8);

                /* Protection */
                prt = protection_roll(GF_HURT, false);

                update_combat_rolls2(
                    4, 8, dam, -1, -1, prt, 100, GF_HURT, false);

                net_dam = (dam - prt > 0) ? (dam - prt) : 0;

                if (allow_player_stun(NULL))
                {
                    (void)set_stun(p_ptr->stun + dam * 4);
                }
            }
            else
            {
                msg_print("You nimbly dodge the falling rock!");
                net_dam = 0;
            }

            /* Move player */
            monster_swap(p_ptr->py, p_ptr->px, sy, sx);
        }

        /* Take the damage */
        killer_mark_other(SCORE_KILLER_TRAP);
        take_hit(net_dam, name);

        /* Forget the trap */
        cave_info[y][x] &= ~(CAVE_MARK);

        /* Replace the trap with rubble */
        cave_set_feat(y, x, FEAT_RUBBLE);

        /* Make a lot of noise */
        monster_perception(true, false, -20);

        break;
    }
    }
}

/*
 * Find the attr/char pair to use for a visual hit effect
 *
 */
static u16b hit_pict(int net_dam, int dam_type, bool fatal_blow)
{
    int base;

    byte k;

    byte a;
    char c;

    if (!graphics_are_ascii())
    {
        a = misc_to_attr[net_dam];
        c = misc_to_char[net_dam];
    }
    else
    {
        /* Base graphic '*' */
        base = 0x30;

        /* Basic hit color */
        if (fatal_blow)
        {
            k = TERM_RED;
        }
        else if (net_dam == 0)
        {
            // only knock back overrides the default for zero damage hits
            if (dam_type == GF_SOUND)
            {
                k = TERM_L_UMBER;
            }
            else
            {
                k = TERM_L_WHITE;
            }
        }
        else
        {
            if (dam_type == GF_POIS)
            {
                k = TERM_GREEN;
            }
            else if (dam_type == GF_SOUND)
            {
                k = TERM_L_UMBER;
            }
            else
            {
                k = TERM_L_RED;
            }
        }

        /* Obtain attr/char */
        a = misc_to_attr[base + k];
        c = misc_to_char[base + k];

        if (net_dam > 0)
        {
            // if (net_dam < 20)	c = 48 + (net_dam % 10);
            c = 48 + (net_dam % 10);
        }
    }

    /* Create pict */
    return (PICT(a, c));
}

void display_hit(int y, int x, int net_dam, int dam_type, bool fatal_blow)
{
    u16b p1;
    u16b p2;

    int tens = net_dam / 10;
    int units = net_dam % 10;
    if (tens > 9)
    {
        tens = 9;
        units = 9;
    }

    // do nothing unless the appropriate option is set
    if (!display_hits)
        return;

    /* Obtain the hit pict */
    p1 = hit_pict(units, dam_type, fatal_blow);
    p2 = hit_pict(tens, dam_type, fatal_blow);

    /* Display the visual effects */
    print_rel(PICT_C(p1), PICT_A(p1), y, x);
    move_cursor_relative(y, x);

    if (net_dam >= 10)
    {
        print_rel(PICT_C(p2), PICT_A(p2), y, x - 1);
        move_cursor_relative(y, x - 1);
    }

    Term_fresh();

    /* Delay */
    Term_xtra(TERM_XTRA_DELAY, 25 * op_ptr->delay_factor);

    /* Erase the visual effects */
    lite_spot(y, x);
    lite_spot(y, x - 1);
    Term_fresh();
}

/*
 *  Determines whether an attack is a charge attack
 */

bool valid_charge(int fy, int fx, int attack_type)
{
    int d, i;

    int deltay = fy - p_ptr->py;
    int deltax = fx - p_ptr->px;

    if (p_ptr->active_ability[S_MEL][MEL_CHARGE] && (p_ptr->pspeed > 1)
        && is_normal_attack(attack_type))
    {
        // try all three directions
        for (i = -1; i <= 1; i++)
        {
            d = cycle[chome[dir_from_delta(deltay, deltax)] + i];

            if (p_ptr->previous_action[1] == d)
            {
                return (true);
            }
        }
    }

    return (false);
}

/*
 *  Attacks a new monster with 'follow through' if applicable
 */

void possible_follow_through(int fy, int fx, int attack_type)
{
    int d, i;

    int y, x;

    int deltay = fy - p_ptr->py;
    int deltax = fx - p_ptr->px;

    // clamp impale kills
    if (deltax > 1)
        deltax = 1;
    else if (deltax < -1)
        deltax = -1;

    if (deltay > 1)
        deltay = 1;
    else if (deltay < -1)
        deltay = -1;

    if (p_ptr->active_ability[S_MEL][MEL_FOLLOW_THROUGH] && !(p_ptr->confused)
        && (is_normal_attack(attack_type) || (attack_type == ATT_FOLLOW_THROUGH)
            || (attack_type == ATT_WHIRLWIND)))
    {
        // look through adjacent squares in an anticlockwise direction
        for (i = 1; i < 8; i++)
        {
            d = cycle[chome[dir_from_delta(deltay, deltax)] + i];

            y = p_ptr->py + ddy[d];
            x = p_ptr->px + ddx[d];

            if (cave_m_idx[y][x] > 0)
            {
                monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];

                if (m_ptr->ml
                    && (!forgo_attacking_unwary
                        || (m_ptr->alertness >= ALERTNESS_ALERT)))
                {
                    if (valorous_oath_blocks_auto_attack(m_ptr))
                    {
                        msg_print("You stop your follow-through to avoid striking a fleeing foe.");
                        return;
                    }

                    msg_print("You continue your attack!");
                    py_attack_aux(y, x, ATT_FOLLOW_THROUGH);
                    return;
                }
            }
        }
    }
}

/*
 *  Determines the bonus for the ability 'concentration' and updates some
 * related variables.
 */

int concentration_bonus(int y, int x)
{
    int bonus = 0;

    // deal with 'concentration' ability
    if (p_ptr->active_ability[S_PER][PER_CONCENTRATION]
        && (p_ptr->last_attack_m_idx == cave_m_idx[y][x]))
    {
        bonus = MIN(p_ptr->consecutive_attacks, p_ptr->skill_use[S_PER] / 2);
    }

    // If the player is not engaged with this monster, reset the attack count
    // and mosnter
    if ((p_ptr->last_attack_m_idx != cave_m_idx[y][x]))
    {
        p_ptr->consecutive_attacks = 0;
        p_ptr->last_attack_m_idx = cave_m_idx[y][x];
    }

    return (bonus);
}

/*
 *  Determines the bonus for the ability 'focused attack'.
 */

int focused_attack_bonus(void)
{
    // focused attack
    if (p_ptr->focused)
    {
        p_ptr->focused = false;

        if (p_ptr->active_ability[S_PER][PER_FOCUSED_ATTACK])
        {
            return (p_ptr->skill_use[S_PER] / 2);
        }
    }

    return (0);
}

/*
 *  Determines the bonus for the ability 'master hunter'.
 */

int master_hunter_bonus(monster_type* m_ptr)
{
    // master hunter bonus
    if (p_ptr->active_ability[S_PER][PER_MASTER_HUNTER])
    {
        return (
            MIN((&l_list[m_ptr->r_idx])->pkills, p_ptr->skill_use[S_PER] / 2));
    }
    else
    {
        return (0);
    }
}

void attack_punctuation(char* punctuation, int net_dam, int crit_bonus_dice)
{
    int i;

    if (net_dam == 0)
    {
        SDL_strlcpy(punctuation, "...", sizeof(punctuation));
    }
    else if (crit_bonus_dice == 0)
    {
        SDL_strlcpy(punctuation, ".", sizeof(punctuation));
    }
    else
    {
        for (i = 0; (i < crit_bonus_dice) && (i < 20); i++)
        {
            punctuation[i] = '!';
        }
        punctuation[i] = '\0';
    }
}

bool knock_back(int y1, int x1, int y2, int x2)
{
    bool knocked = false;

    bool monster_target = false;

    int mod, d, i;
    int y3, x3; // the location to get knocked to
    int dir;

    int dy, dx;

    // default to there being no monster
    monster_type* m_ptr = NULL;

    // determine the main direction from the source to the target
    dir = rough_direction(y1, x1, y2, x2);

    // extract the deltas from the direction
    dy = ddy[dir];
    dx = ddx[dir];

    // knocking a monster back...
    if (cave_m_idx[y2][x2] > 0)
    {
        monster_target = true;
        m_ptr = &mon_list[cave_m_idx[y2][x2]];
    }

    // first try to knock it straight back
    if (cave_floor_bold(y2 + dy, x2 + dx)
        && (cave_m_idx[y2 + dy][x2 + dx] == 0))
    {
        y3 = y2 + dy;
        x3 = x2 + dx;
        knocked = true;
    }

    // then try the adjacent directions
    else
    {
        // randomize clockwise or anticlockwise
        if (one_in_(2))
            mod = -1;
        else
            mod = +1;

        // try both directions
        for (i = 0; i < 2; i++)
        {
            d = cycle[chome[dir_from_delta(dy, dx)] + mod];
            y3 = y2 + ddy[d];
            x3 = x2 + ddx[d];
            if (cave_floor_bold(y3, x3) && (cave_m_idx[y3][x3] == 0))
            {
                knocked = true;
                break;
            }

            // switch direction
            mod *= -1;
        }
    }

    // make the target skip a turn
    if (knocked)
    {
        if (monster_target)
        {
            m_ptr->skip_next_turn = true;

            // actually move the monster
            monster_swap(y2, x2, y3, x3);
        }
        else
        {
            msg_print("You are knocked back.");
            p_ptr->knocked_back = true;

            p_ptr->skip_next_turn = true;

            // actually move the player
            monster_swap(y2, x2, y3, x3);

            // cannot stay in the air
            p_ptr->leaping = false;

            // make some noise when landing
            stealth_score -= 5;

            /* Set off traps */
            if (cave_trap_bold(p_ptr->py, p_ptr->px)
                || ((cave_feat[p_ptr->py][p_ptr->px] == FEAT_CHASM)))
            {
                // If it is hidden
                if (cave_info[p_ptr->py][p_ptr->px] & (CAVE_HIDDEN))
                {
                    /* Reveal the trap */
                    reveal_trap(p_ptr->py, p_ptr->px);
                }

                /* Hit the trap */
                hit_trap(p_ptr->py, p_ptr->px);
            }
        }
    }

    return (knocked);
}

bool merciless_attack(monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    return (chosen_oath(OATH_MERCY) && !oath_invalid(OATH_MERCY)
        && ((r_ptr->flags3 & (RF3_MAN)) || (r_ptr->flags3 & (RF3_ELF))));
}

bool cowardly_attack(monster_type* m_ptr)
{
    return (chosen_oath(OATH_VALOROUS) && !oath_invalid(OATH_VALOROUS)
        && m_ptr->stance == STANCE_FLEEING);  /* Monster is fleeing in terror */
}

static bool valorous_oath_blocks_auto_attack(monster_type* m_ptr)
{
    if (!valorous_oath_auto_attack_safety)
        return false;

    if (!chosen_oath(OATH_VALOROUS) || oath_invalid(OATH_VALOROUS))
        return false;

    if (!m_ptr || !m_ptr->ml)
        return false;

    return (m_ptr->stance == STANCE_FLEEING);
}

bool abort_for_mercy(monster_type* m_ptr)
{
    // Unseen enemies are okay to kill
    if (!m_ptr->ml)
        return false;

    if (merciless_attack(m_ptr))
    {
        /* Use oath-specific confirmation prompt */
        char* prompt = oath_confirmation_prompt(OATH_MERCY);
        if (!prompt || !prompt[0]) prompt = "Are you sure you wish to break your oath?";
        
        if (!get_check_oath_multiline(prompt))
        {
            return true;
        }
    }

    return false;
}

bool abort_for_valorous(monster_type* m_ptr)
{
    // Unseen enemies are okay to kill  
    if (!m_ptr->ml)
        return false;

    if (cowardly_attack(m_ptr))
    {
        /* Use oath-specific confirmation prompt */
        char* prompt = oath_confirmation_prompt(OATH_VALOROUS);
        if (!prompt || !prompt[0]) prompt = "Are you sure you wish to break your oath?";
        
        if (!get_check_oath_multiline(prompt))
        {
            return true;
        }
    }

    return false;
}

/*
 * Check if an attack type is an Area of Effect (AoE) attack
 * vs a direct targeted attack
 */
bool is_aoe_attack_type(int attack_type)
{
    switch (attack_type)
    {
        case ATT_MAIN:
        case ATT_FLANKING:
        case ATT_CONTROLLED_RETREAT:
        case ATT_POLEARM:
        case ATT_RIPOSTE:
        case ATT_OPPORTUNIST:
        case ATT_ZONE_OF_CONTROL:
        case ATT_OPPORTUNITY:
        case ATT_IMPALE:
            return false;  // Direct targeted attacks
            
        case ATT_WHIRLWIND:
        case ATT_RAGE:
        case ATT_FOLLOW_THROUGH:
            return true;   // AoE attacks
            
        default:
            return false;  // Default to direct attack
    }
}

/*
 * Apply consequences when an oath is broken:
 * 1. Remove oath bonuses (recalculate stats)
 * 2. Apply a random metarun curse
 * 3. Ban the oath for the rest of this metarun
 */
void apply_oath_breaking_curse(int oath_id)
{
    cptr oath_name;
    
    if (oath_id < 1 || !z_info || oath_id >= z_info->oath_max) return;
    
    /* Get oath name for logging - use static fallback names to avoid dangling pointer */
    static const char* fallback_oath_names[] = {"", "Mercy", "Silence", "Iron", "Smith", "Valorous", "Light"};
    if (oath_id <= z_info->oath_max && oath_info[oath_id].name) {
        oath_name = oath_name_text + oath_info[oath_id].name;
    } else if (oath_id < 7) {
        oath_name = fallback_oath_names[oath_id];
    } else {
        oath_name = "Unknown";
    }
    
    log_trace("Applying oath breaking consequences for oath %d (%s)", oath_id, oath_name);
    
    /* Disable the corresponding special ability */
    if (oath_id == OATH_MERCY) {
        p_ptr->active_ability[S_SPC][SPC_OATH_MERCY] = false;
    }
    else if (oath_id == OATH_SILENCE) {
        p_ptr->active_ability[S_SPC][SPC_OATH_SILENCE] = false;
    }
    else if (oath_id == OATH_IRON) {
        p_ptr->active_ability[S_SPC][SPC_OATH_IRON] = false;
    }
    else if (oath_id == OATH_SMITH) {
        p_ptr->active_ability[S_SPC][SPC_OATH_SMITH] = false;
    }
    else if (oath_id == OATH_VALOROUS) {
        p_ptr->active_ability[S_SPC][SPC_OATH_VALOROUS] = false;
    }
    else if (oath_id == OATH_LIGHT) {
        p_ptr->active_ability[S_SPC][SPC_OATH_LIGHT] = false;
    }
    
    /* Remove oath bonuses by recalculating */
    p_ptr->update |= (PU_BONUS);
    p_ptr->redraw |= (PR_STATE);
    
    /* Show oath-specific curse message and let player choose curse */
    int chosen_curse = choose_oath_breaking_curse_ui(oath_id);
    
    if (chosen_curse >= 0) {
        /* Apply the chosen curse */
        add_curse_stack(chosen_curse);
        log_trace("Applied chosen curse %d for breaking oath", chosen_curse);
    } else {
        /* Fallback to random curse if UI failed */
        int selected_curse = 0;
        if (z_info && z_info->cu_max > 0) selected_curse = rand_int(z_info->cu_max);
        add_curse_stack(selected_curse);
        log_trace("Applied fallback random curse %d for breaking oath", selected_curse);
    }
    
    /* Ban this oath for the rest of the metarun */
    metarun_ban_oath(oath_id);
    
    log_trace("Banned oath %d (%s) from future selection in this metarun", oath_id, oath_name);
}

void break_mercy_oath(monster_type* m_ptr, int damage)
{
    // Unseen enemies are okay to kill
    if (!m_ptr->ml)
        return;

    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    if (damage > 0
        && ((r_ptr->flags3 & (RF3_MAN)) || (r_ptr->flags3 & (RF3_ELF))))
    {
        if (merciless_attack(m_ptr))
        {
            /* Curse message and selection handled by apply_oath_breaking_curse */
            do_cmd_note("Broke your oath", p_ptr->depth);
            
            /* Apply oath breaking consequences */
            apply_oath_breaking_curse(OATH_MERCY);
            
            /* Only mark oath as broken if player actually has it */
            p_ptr->oaths_broken |= OATH_MERCY_FLAG;
        }
    }
}

void break_valorous_oath(monster_type* m_ptr, int damage, int attack_type, int damage_source)
{
    // Unseen enemies are okay to kill
    if (!m_ptr->ml)
        return;

    // Only break oath for player-caused damage  
    // damage_source: -1 = player, 0+ = monster index
    if (damage_source != -1)
        return;

    /* All player-caused attacks break Valor on hit */
    (void)attack_type;
    if (damage <= 0)
        return;

    if (!cowardly_attack(m_ptr))
        return;

    do_cmd_note("Broke your oath", p_ptr->depth);
    apply_oath_breaking_curse(OATH_VALOROUS);
    p_ptr->oaths_broken |= OATH_VALOROUS_FLAG;
}

/*
 * Attack the monster at the given location
 *
 * If no "weapon" is available, then "punch" the monster one time.
 */
void py_attack_aux(int y, int x, int attack_type)
{
    int num = 0;

    int attack_mod = 0, total_attack_mod = 0, total_evasion_mod = 0;
    int hit_result = 0;
    int crit_bonus_dice = 0, slay_bonus_dice = 0;
    int cruel_blow_multiplier = 0;
    int dam = 0, prt = 0;
    int net_dam = 0;
    int prt_percent = 100;
    int hits = 0;
    int weapon_weight;
    int total_dice;
    int blows;
    int mdd, mds;
    int stealth_bonus = 0;
    int assassination_bonus = 0;
    int monster_ripostes = 0;
    int effective_strength;
    int damage_type = GF_HURT;

    int m_idx;
    monster_type* m_ptr;
    monster_race* r_ptr;

    object_type* o_ptr;

    char m_name[80];
    char punctuation[20];

    bool abort_attack = false;
    bool do_knock_back = false;
    bool knocked = false;
    bool charge = false;
    bool rapid_attack = false;
    bool off_hand_blow = false;
    bool fatal_blow = false;
    bool smite = false;

    u32b f1, f2, f3, f4; // the weapon's flags

    u32b noticed_flag = 0; // if any slay is observed and the weapon thus
                           // identified it goes here

    /* Get the monster */
    m_idx = cave_m_idx[y][x];
    m_ptr = &mon_list[m_idx];
    r_ptr = &r_info[m_ptr->r_idx];

    /*possibly update the monster health bar*/
    if (p_ptr->health_who == cave_m_idx[y][x])
        p_ptr->redraw |= (PR_HEALTHBAR);

    /* Disturb the player */
    disturb(0, 0);

    /* Extract monster name (or "it") */
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /* Auto-Recall if possible and visible */
    if (m_ptr->ml)
        monster_race_track(m_ptr->r_idx);

    /* Track a new monster */
    if (m_ptr->ml)
        health_track(cave_m_idx[y][x]);

    /* Target this monster */
    if (m_ptr->ml)
        target_set_monster(cave_m_idx[y][x]);

    if (r_ptr->flags1 & (RF1_PEACEFUL))
    {
        if (attack_type == ATT_MAIN)
        {
            /* Handle alert thrall quest interaction */
            if (is_alert_thrall(m_ptr))
            {
                handle_thrall_interaction(m_ptr);
            }
            else
            {
                msg_format("You stop before you bump into %s.", m_name);
            }
        }

        if (!player_attacked)
        {
            p_ptr->previous_action[0] = ACTION_NOTHING;
            p_ptr->energy_use = 0;
        }

        return;
    }

    /* Get the weapon */
    o_ptr = &inventory[INVEN_WIELD];

    /* Handle player fear */
    if (p_ptr->afraid)
    {
        /* Message */
        msg_format("You are too afraid to attack %s!", m_name);

        abort_attack = true;
    }

    // "!a" on the weapon, or the gameplay option, prompts before attacking.
    if ((pacifist_attack_warning
            || weapon_has_attack_confirmation_inscription(o_ptr))
        && !p_ptr->truce && m_ptr->ml)
    {
        if (!get_check("Are you sure you wish to attack? "))
            abort_attack = true;
    }

    // Warning about breaking the truce
    if ((p_ptr->truce) && !get_check("Are you sure you wish to attack? "))
    {
        abort_attack = true;
    }

    // Warn about fighting with fists
    if ((o_ptr->weight == 0)
        && !get_check("Are you sure you wish to attack with no weapon? "))
    {
        abort_attack = true;
    }

    // Warn about fighting with shovel
    if ((o_ptr->tval == TV_DIGGING) && (o_ptr->sval == SV_SHOVEL)
        && !get_check("Are you sure you wish to attack with your shovel? "))
    {
        abort_attack = true;
    }

    // Don't make the player deal with Oath warnings on free attacks - pass them
    // up
    if (!is_normal_attack(attack_type) && merciless_attack(m_ptr))
    {
        abort_attack = true;
    }
    else if (abort_for_mercy(m_ptr))
    {
        abort_attack = true;
    }
    else if (!is_aoe_attack_type(attack_type) && abort_for_valorous(m_ptr))
    {
        // Only show valorous oath warning for direct attacks
        // AoE attacks will break oath immediately without warning
        abort_attack = true;
    }

    // Cancel the attack if needed
    if (abort_attack)
    {
        if (!player_attacked)
        {
            // reset the action type
            p_ptr->previous_action[0] = ACTION_NOTHING;

            // don't take a turn
            p_ptr->energy_use = 0;
        }

        /* Done */
        return;
    }

    // fighting with fists is equivalent to a 4 lb weapon for the purpose of
    // criticals
    weapon_weight = o_ptr->weight ? o_ptr->weight : 40;

    mdd = p_ptr->mdd;
    mds = p_ptr->mds;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    // determine the base for the attack_mod
    attack_mod = p_ptr->skill_use[S_MEL];

    /* Monsters might notice */
    player_attacked = true;

    // Determine the number of attacks
    blows = 1;
    if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
    {
        blows++;
        rapid_attack = true;
    }
    if (p_ptr->mds2 > 0 && attack_type != ATT_IMPALE)
    {
        blows++;
    }

    // Attack types that take place in the opponents' turns only allow a single
    // attack
    if (!is_normal_attack(attack_type) && attack_type != ATT_WHIRLWIND)
    {
        blows = 1;

        // undo strength adjustment to the attack (if any)
        mds = total_mds(o_ptr, 0);

        // undo the dexterity adjustment to the attack (if any)
        if (rapid_attack)
        {
            rapid_attack = false;
            attack_mod += 3;
        }
    }

    /* Attack once for each legal blow */
    while (num++ < blows)
    {
        smite = two_handed_melee() && p_ptr->active_ability[S_MEL][MEL_SMITE]
            && num == 1
            && (attack_type == ATT_MAIN || attack_type == ATT_FLANKING
                || attack_type == ATT_IMPALE
                || attack_type == ATT_FOLLOW_THROUGH
                || attack_type == ATT_WHIRLWIND);

        do_knock_back = false;
        knocked = false;

        if (smite)
            p_ptr->skip_next_turn = true;

        // if the previous blow was a charge, undo the charge effects for later
        // blows
        if (charge)
        {
            charge = false;
            attack_mod -= 3;
            mds = p_ptr->mds;
        }

        // adjust for off-hand weapon if it is being used
        if ((num == blows) && (num != 1) && (p_ptr->mds2 > 0)
            && attack_type != ATT_IMPALE)
        {
            off_hand_blow = true;
            rapid_attack = false;

            attack_mod += p_ptr->offhand_mel_mod;
            mdd = p_ptr->mdd2;
            mds = p_ptr->mds2;
            o_ptr = &inventory[INVEN_ARM];
            weapon_weight = o_ptr->weight;
            object_flags4(o_ptr, &f1, &f2, &f3, &f4);
        }

        if (is_normal_attack(attack_type))
        {
            assassination_bonus = stealth_melee_bonus(m_ptr, false);
        }
        else
        {
            assassination_bonus = 0;
        }

        // +3 Str/Dex on first blow when charging
        if ((num == 1) && valid_charge(y, x, attack_type))
        {
            if (!(assassination_over_charge && assassination_bonus > 0))
            {
                int str_adjustment = 3;

                if (rapid_attack)
                    str_adjustment -= 3;

                charge = true;
                attack_mod += 3;

                // undo strength adjustment to the attack (if any)
                mds = total_mds(o_ptr, str_adjustment);

                if (assassination_bonus > 0)
                {
                    msg_print(
                        "(Assassination did not apply because this was a charge attack.)");
                }
            }
        }

        // reward attacks on unaware monsters for characters with the
        // assassination ability, unless charge takes priority
        if (is_normal_attack(attack_type) && !charge)
        {
            stealth_bonus = assassination_bonus;
        }
        else
        {
            stealth_bonus = 0;
        }

        // Determine the player's attack score after all modifiers
        total_attack_mod
            = total_player_attack(m_ptr, attack_mod + stealth_bonus);

        // Determine the monster's evasion score after all modifiers
        total_evasion_mod = total_monster_evasion(m_ptr, false);

        song_disguise_note_player_attack(cave_m_idx[m_ptr->fy][m_ptr->fx]);

        hit_result = hit_roll(
            total_attack_mod, total_evasion_mod, PLAYER, m_ptr, true);

        if (hit_result <= 0 && (f3 & TR3_ACCURATE))
        {
            char m_name[80];
            monster_desc(m_name, sizeof(m_name), m_ptr, 0x00);

            hit_result = hit_roll(
                total_attack_mod, total_evasion_mod, PLAYER, m_ptr, true);
            if (hit_result > 0)
                msg_format("%^s tries and fails to dodge your blow.", m_name);
        }

        /* If the attack connects... */
        if (hit_result > 0)
        {
            hits++;

            /* Mark the monster as attacked */
            m_ptr->mflag |= (MFLAG_HIT_BY_MELEE);

            /* Mark the monster as charged */
            if (charge)
                m_ptr->mflag |= (MFLAG_CHARGED);

            /* Calculate the damage */
            crit_bonus_dice = crit_bonus(
                hit_result, weapon_weight, r_ptr, S_MEL, false, NULL, o_ptr);
            slay_bonus_dice = slay_bonus(o_ptr, m_ptr, &noticed_flag);

            if (f3 & TR3_CUMBERSOME)
            {
                crit_bonus_dice = 0;
            }

            total_dice = mdd + slay_bonus_dice + crit_bonus_dice;

            dam = damroll(total_dice, mds);
            if (smite)
                dam = total_dice * mds;

            /* Apply armor dice/sides curses/blessings */
            int armor_dice_base = r_ptr->pd - m_ptr->song_armor_dice_penalty;
            if (armor_dice_base < 0)
                armor_dice_base = 0;
            int armor_dice = armor_dice_base + curse_flag_delta_cur(CUR_MON_ARM_DICE);
            int armor_sides = monster_base_armour_sides(m_ptr) + curse_flag_delta_cur(CUR_MON_ARM_SIDE);
            if (armor_dice < 0) armor_dice = 0;
            if (armor_sides < 1) armor_sides = 1;
            prt = damroll(armor_dice, armor_sides);
            prt_percent = prt_after_sharpness(o_ptr, &noticed_flag);

            if (prt_percent < 0)
            {
                prt_percent = 0;
            }

            prt = (prt * prt_percent) / 100;

            net_dam = dam - prt;

            /* No negative damage */
            if (net_dam < 0)
                net_dam = 0;

            break_mercy_oath(m_ptr, net_dam);
            break_valorous_oath(m_ptr, net_dam, attack_type, -1);  // -1 indicates player damage

            // Play weapon swing sound first (layered sound system)
            u16b weapon_swing_type = weapon_sound_message_type(o_ptr, false);
            sound(weapon_swing_type);

            // Schedule the result sound (armor or hit) anchored to the swing
            // onset, so weapon_animation_delay is "ms from swing start" rather
            // than drifting with display_hit's internal pause.
            u16b result_sound = (net_dam > 0) ? MSG_HIT : MSG_ARMOR;
            sound_delayed(result_sound, weapon_animation_delay(weapon_swing_type));

            // determine the punctuation for the attack ("...", ".", "!" etc)
            attack_punctuation(punctuation, net_dam, crit_bonus_dice);

            /* Special message for visible unalert creatures */
            if (stealth_bonus)
            {
                msg_format("You stealthily attack %s%s", m_name, punctuation);
            }
            else
            {
                /* Message */
                if (charge)
                {
                    msg_format("You charge %s%s", m_name, punctuation);
                }
                else if (smite)
                {
                    msg_format("You smite %s%s", m_name, punctuation);
                }
                else if (attack_type == ATT_IMPALE)
                {
                    msg_format("You impale %s%s", m_name, punctuation);
                }
                else
                {
                    msg_format("You hit %s%s", m_name, punctuation);
                }
            }

            // determine the player's score for knocking an opponent backwards
            // if they have the ability first calculate their strength including
            // modifiers for this attack
            effective_strength = p_ptr->stat_use[A_STR];
            if (charge)
                effective_strength += 3;
            if (rapid_attack)
                effective_strength -= 3;
            if (off_hand_blow)
                effective_strength -= 3;

            // cap the value by the weapon weight
            if (effective_strength > weapon_weight / 10)
                effective_strength = weapon_weight / 10;
            if ((effective_strength < 0)
                && (-effective_strength > weapon_weight / 10))
                effective_strength = -(weapon_weight / 10);

            // give an extra +2 bonus for using a weapon two-handed
            if (two_handed_melee())
                effective_strength += 2;

            // check whether the effect triggers
            if (p_ptr->active_ability[S_MEL][MEL_KNOCK_BACK]
                && (attack_type != ATT_OPPORTUNIST)
                && !(r_ptr->flags1 & (RF1_NEVER_MOVE))
                && !(r_ptr->flags1 & (RF1_HIDDEN_MOVE))
                && (skill_check(PLAYER, effective_strength * 2,
                        monster_stat(m_ptr, A_CON) * 2, m_ptr)
                    > 0))
            {
                // remember this for later when the effect is applied
                do_knock_back = true;
            }

            if (singing(SNG_SLAYING) && crit_bonus_dice > 0)
            {
                int kill_threshold = ability_bonus(S_SNG, SNG_SLAYING);
                if (m_ptr->hp <= kill_threshold)
                {
                    msg_format("Your song soars as %s falls before you.", m_name);

                    /* Sort out combat rolls window */
                    total_dice = 0;
                    mds = 0;
                    dam = m_ptr->hp;
                    prt = 0;
                    prt_percent = 0;

                    /* Generate treasure */
                    monster_death(m_idx);

                    /* Auto-recall only if visible or unique */
                    if (m_ptr->ml || (r_ptr->flags1 & (RF1_UNIQUE)))
                    {
                        monster_race_track(m_ptr->r_idx);
                    }

                    /* Delete the monster */
                    delete_monster_idx(m_idx);
                    
                    fatal_blow = true;
                }
            }

            // Take hit only if monster has not been killed by an ability already
            if (!fatal_blow)
            {
                // damage, check for death
                fatal_blow = mon_take_hit(m_idx, net_dam, NULL, -1);
                p_ptr->vengeance = 0;
            }

            update_combat_rolls2(total_dice, mds, dam, armor_dice, armor_sides,
                prt, prt_percent, damage_type, true);

            // use different colours depending on whether knock back triggered
            if (do_knock_back)
            {
                display_hit(y, x, net_dam, GF_SOUND, fatal_blow);
            }
            else
            {
                display_hit(y, x, net_dam, GF_HURT, fatal_blow);
            }

            apply_weapon_combat_effects(
                o_ptr, m_ptr, S_MEL, net_dam, fatal_blow, "blow");

            // if a slay was noticed, then identify the weapon
            if (noticed_flag)
            {
                ident_weapon_by_use(o_ptr, m_ptr, noticed_flag);
                noticed_flag = false;
            }

            // deal with killing blows
            if (fatal_blow)
            {
                // deal with 'follow_through' ability
                possible_follow_through(y, x, attack_type);

                if (p_ptr->active_ability[S_WIL][WIL_FORMIDABLE])
                {
                    int will_score = p_ptr->skill_use[S_WIL];
                    if (project_los(GF_FEAR, 0, 0, will_score, true))
                        msg_print("Your foes are daunted!");
                }

                // stop attacking
                break;
            }

            // if the monster didn't die...
            else
            {
                // deal with knock back ability if it triggered
                if (do_knock_back)
                {
                    knocked = knock_back(p_ptr->py, p_ptr->px, y, x);
                }

                // Morgoth drops his iron crown if he is hit for 10 or more net
                // damage twice
                if ((m_ptr->r_idx == R_IDX_MORGOTH)
                    && ((&a_info[ART_MORGOTH_3])->cur_num == 0))
                {
                    if (net_dam >= 10)
                    {
                        if (p_ptr->morgoth_hits == 0)
                        {
                            msg_print("The force of your blow knocks the Iron "
                                      "Crown off balance.");
                            p_ptr->morgoth_hits++;
                        }
                        else if (p_ptr->morgoth_hits == 1)
                        {
                            drop_iron_crown(m_ptr,
                                "You knock his crown from off his brow, and it "
                                "falls to the ground nearby.");
                            p_ptr->morgoth_hits++;
                        }
                    }
                }

                // Deal with cruel blow ability
                if (p_ptr->active_ability[S_STL][STL_CRUEL_BLOW]
                    && (crit_bonus_dice > 0) && (net_dam > 0)
                    && !(r_ptr->flags1 & (RF1_RES_CRIT)))
                {
                    // Slightly magical. Function that caps out before 30
                    // but grows quickly early on, and doesn't need math.h
                    cruel_blow_multiplier = (30 - (60 / (crit_bonus_dice + 2)));
                    if (skill_check(PLAYER, cruel_blow_multiplier,
                            monster_skill(m_ptr, S_WIL), m_ptr)
                        > 0)
                    {
                        msg_format("%^s reels in pain!", m_name);

                        // confuse the monster (if possible)
                        if (!(r_ptr->flags3 & (RF3_NO_CONF)))
                        {
                            // The +1 is needed as a turn of this wears off
                            // immediately
                            m_ptr->confused += crit_bonus_dice + 1;
                        }

                        // cause a temporary morale penalty
                        scare_onlooking_friends(m_ptr, -20);
                    }
                }
            }
        }

        /* Player misses */
        else
        {
            // Play weapon swing sound (no result sound for misses, so no
            // scheduling needed)
            u16b weapon_swing_type = weapon_sound_message_type(o_ptr, false);
            sound(weapon_swing_type);

            /* Message - no additional sound for miss */
            msg_format("You miss %s.", m_name);

            // Occasional warning about fighting from within a pit
            if (cave_pit_bold(p_ptr->py, p_ptr->px) && one_in_(3))
            {
                msg_print(
                    "(It is very hard to dodge or attack from within a pit.)");
            }

            // Occasional warning about fighting from within a web
            if ((cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB)
                && one_in_(3))
            {
                msg_print(
                    "(It is very hard to dodge or attack from within a web.)");
            }

            // allow for ripostes
            // treats attack a weapon weighing 2 pounds per damage die
            if ((r_ptr->flags2 & (RF2_RIPOSTE)) && (monster_ripostes == 0)
                && !m_ptr->confused && (m_ptr->stance != STANCE_FLEEING)
                && !m_ptr->skip_this_turn && !m_ptr->skip_next_turn
                && (hit_result <= -10 - (2 * r_ptr->blow[0].dd)))
            {
                msg_format("%^s ripostes!", m_name);
                make_attack_normal(m_ptr);
                monster_ripostes++;

                if (m_ptr->ml)
                {
                    monster_lore* l_ptr = &l_list[m_ptr->r_idx];
                    l_ptr->flags2 |= (RF2_RIPOSTE);
                }
            }
        }

        // alert the monster, even if no damage was done or the player missed
        make_alert(m_ptr);

        // stop attacking if you displace the creature
        if (knocked)
            break;
    }

    // Break the truce if creatures see
    break_truce(false);
}

/*
 * Count the maximum number of continuous passable adjacent squares 
 * (not walls, not rubble, not closed doors)
 * Returns the longest sequence of adjacent passable squares
 */
int count_open_adjacent_squares(int y, int x)
{
    bool passable[8];
    int i;
    int max_continuous = 0;
    int current_continuous = 0;
    
    /* First, check which adjacent squares are passable */
    for (i = 0; i < 8; i++)
    {
        int adj_y = y + ddy_ddd[i];
        int adj_x = x + ddx_ddd[i];
        
        /* Check bounds */
        if (!in_bounds(adj_y, adj_x))
        {
            passable[i] = false;
            continue;
        }
            
        /* Check if square is passable (not wall, not rubble, not closed door) */
        if (cave_floor_bold(adj_y, adj_x) || 
            cave_feat[adj_y][adj_x] == FEAT_OPEN ||
            (cave_feat[adj_y][adj_x] >= FEAT_TRAP_HEAD && cave_feat[adj_y][adj_x] <= FEAT_TRAP_TAIL))
        {
            passable[i] = true;
        }
        else
        {
            passable[i] = false;
        }
        log_trace("Adjacent square %d: (%d,%d) feat=%d passable=%d", i, adj_y, adj_x, cave_feat[adj_y][adj_x], passable[i]);
    }
    
    /* Now find the longest continuous sequence of passable squares */
    /* We need to check sequences that are actually adjacent in the game world */
    /* Direction mapping: 0=S, 1=N, 2=E, 3=W, 4=SE, 5=SW, 6=NE, 7=NW */
    /* Clockwise order in game world: N(1), NE(6), E(2), SE(4), S(0), SW(5), W(3), NW(7) */
    int clockwise_order[8] = {1, 6, 2, 4, 0, 5, 3, 7};
    
    for (int start = 0; start < 8; start++)
    {
        current_continuous = 0;
        /* Count consecutive passable squares going clockwise from start */
        for (int offset = 0; offset < 8; offset++)
        {
            int idx = clockwise_order[(start + offset) % 8];
            if (passable[idx])
            {
                current_continuous++;
            }
            else
            {
                break; /* Stop at first non-passable square */
            }
        }
        if (current_continuous > max_continuous)
            max_continuous = current_continuous;
    }
    
    log_trace("count_open_adjacent_squares result: max_continuous=%d", max_continuous);
    return max_continuous;
}

bool whirlwind_possible(void)
{
    if (!p_ptr->active_ability[S_MEL][MEL_WHIRLWIND_ATTACK])
    {
        return (false);
    }

    return (true);
}

bool can_impale()
{
    bool has_impale_skill = p_ptr->active_ability[S_MEL][MEL_IMPALE];

    object_type* o_ptr = &inventory[INVEN_WIELD];

    return has_impale_skill && weapon_is_impale_eligible(o_ptr);
}

void py_attack(int y, int x, int attack_type)
{
    int dir, dir0, yy, xx;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    dir = dir_from_delta(y - p_ptr->py, x - p_ptr->px);
    dir0 = chome[dir];

    // Debug logging for whirlwind
    int open_squares = count_open_adjacent_squares(p_ptr->py, p_ptr->px);
    int adjacent_monsters = adj_mon_count(p_ptr->py, p_ptr->px);
    bool whirlwind_poss = whirlwind_possible();
    
    log_trace("Whirlwind debug: rage=%d, whirlwind_possible=%d, open_squares=%d, adj_monsters=%d, afraid=%d", 
              p_ptr->rage, whirlwind_poss, open_squares, adjacent_monsters, p_ptr->afraid);
    
    bool do_rage_attack = p_ptr->rage && (adjacent_monsters > 1) && !p_ptr->afraid;
    bool do_whirlwind_attack = !p_ptr->rage && whirlwind_poss
        && (open_squares >= 5) && (adjacent_monsters > 1) && !p_ptr->afraid;

    if (do_whirlwind_attack && valorous_oath_auto_attack_safety
        && chosen_oath(OATH_VALOROUS) && !oath_invalid(OATH_VALOROUS))
    {
        for (int check_dir = 1; check_dir <= 9; check_dir++)
        {
            int cy, cx;
            int m_idx;
            monster_type* m_ptr;

            if (check_dir == 5)
                continue;

            cy = p_ptr->py + ddy[check_dir];
            cx = p_ptr->px + ddx[check_dir];
            if (!in_bounds(cy, cx))
                continue;

            m_idx = cave_m_idx[cy][cx];
            if (m_idx <= 0)
                continue;

            m_ptr = &mon_list[m_idx];
            if (m_ptr->ml && (m_ptr->stance == STANCE_FLEEING))
            {
                msg_print("You hold back your whirlwind to avoid striking a fleeing foe.");
                do_whirlwind_attack = false;
                break;
            }
        }
    }

    if (do_rage_attack || do_whirlwind_attack)
    {
        int i;
        bool clockwise = one_in_(2);

        // message only for rage (too annoying otherwise)
        if (do_rage_attack)
        {
            msg_print("You strike out at everything around you!");
        }
        else
        {
            msg_print("You whirl around, striking at everything nearby!");
        }

        // attack the adjacent squares in sequence
        for (i = 0; i < 8; i++)
        {
            if (clockwise)
                dir = cycle[dir0 + i];
            else
                dir = cycle[dir0 - i];

            yy = p_ptr->py + ddy[dir];
            xx = p_ptr->px + ddx[dir];

            if (cave_m_idx[yy][xx] > 0)
            {
                monster_type* m_ptr = &mon_list[cave_m_idx[yy][xx]];

                if (do_rage_attack)
                {
                    py_attack_aux(yy, xx, ATT_RAGE);
                }
                else if ((i == 0) || !forgo_attacking_unwary
                    || (m_ptr->alertness >= ALERTNESS_ALERT))
                {
                    py_attack_aux(yy, xx, ATT_WHIRLWIND);
                }
            }
        }
    }
    else if (can_impale())
    {
        yy = y + ddy[dir];
        xx = x + ddx[dir];

        if (cave_m_idx[yy][xx] > 0)
        {
            py_attack_aux(y, x, ATT_IMPALE);
            py_attack_aux(yy, xx, ATT_IMPALE);
        }
        else
        {
            py_attack_aux(y, x, attack_type);
        }
    }
    else
    {
        py_attack_aux(y, x, attack_type);
    }
}

/*
 *  Does any flanking or controlled retreat attack necessary when player moves
 * to square y,x
 */
void flanking_or_retreat(int y, int x)
{
    int py = p_ptr->py;
    int px = p_ptr->px;
    int d;
    int fy, fx;
    int start;
    monster_type* m_ptr;

    bool flanking = p_ptr->active_ability[S_EVN][EVN_FLANKING];
    bool controlled_retreat = false;
    bool moved_last_turn = false;

    if (((p_ptr->previous_action[1] >= 1) && (p_ptr->previous_action[1] <= 9)
            && (p_ptr->previous_action[1] != 5))
        || (p_ptr->previous_action[1] == ACTION_BASH))
    {
        moved_last_turn = true;
    }

    // need to have the ability, and to have not moved last round
    if (p_ptr->active_ability[S_EVN][EVN_CONTROLLED_RETREAT]
        && !moved_last_turn)
    {
        controlled_retreat = true;
    }

    if (singing(SNG_DISGUISE))
        return;

    if (!p_ptr->confused && (flanking || controlled_retreat))
    {
        fy = p_ptr->target_row;
        fx = p_ptr->target_col;

        // first see if the targetted monster is eligible and attack it if so
        if ((cave_m_idx[fy][fx] > 0) && !p_ptr->confused && !p_ptr->afraid
            && !p_ptr->truce)
        {
            m_ptr = &mon_list[cave_m_idx[fy][fx]];

            if (!merciless_attack(m_ptr)
                && m_ptr->ml
                && (!forgo_attacking_unwary
                    || (m_ptr->alertness >= ALERTNESS_ALERT)))
            {
                // try a flanking attack
                if (flanking && (distance(py, px, fy, fx) == 1)
                    && (distance(y, x, fy, fx) == 1))
                {
                    if (valorous_oath_blocks_auto_attack(m_ptr))
                    {
                        msg_print("You forgo a flanking attack to avoid striking a fleeing foe.");
                        return;
                    }

                    py_attack(fy, fx, ATT_FLANKING);
                    return;
                }
                // try a controlled retreat attack
                if (controlled_retreat && (distance(py, px, fy, fx) == 1)
                    && (distance(y, x, fy, fx) > 1))
                {
                    if (valorous_oath_blocks_auto_attack(m_ptr))
                    {
                        msg_print("You forgo a controlled retreat attack to avoid striking a fleeing foe.");
                        return;
                    }

                    py_attack(fy, fx, ATT_CONTROLLED_RETREAT);
                    return;
                }
            }
        }

        // otherwise we will look through the eligible monsters and choose one
        // randomly
        start = rand_int(8);

        /* Look for adjacent monsters */
        for (d = start; d < 8 + start; d++)
        {
            fy = py + ddy_ddd[d % 8];
            fx = px + ddx_ddd[d % 8];

            /* Check Bounds */
            if (!in_bounds(fy, fx))
                continue;

            if ((cave_m_idx[fy][fx] > 0) && !p_ptr->confused && !p_ptr->afraid
                && !p_ptr->truce)
            {
                m_ptr = &mon_list[cave_m_idx[fy][fx]];

                // base conditions for an attack
                if (!merciless_attack(m_ptr)
                    && m_ptr->ml
                    && (!forgo_attacking_unwary
                        || (m_ptr->alertness >= ALERTNESS_ALERT)))
                {
                    // try a flanking attack
                    if (flanking && (distance(py, px, fy, fx) == 1)
                        && (distance(y, x, fy, fx) == 1))
                    {
                        if (valorous_oath_blocks_auto_attack(m_ptr))
                        {
                            msg_print("You forgo a flanking attack to avoid striking a fleeing foe.");
                            return;
                        }

                        py_attack(fy, fx, ATT_FLANKING);
                        return;
                    }
                    // try a controlled retreat attack
                    if (controlled_retreat && (distance(py, px, fy, fx) == 1)
                        && (distance(y, x, fy, fx) > 1))
                    {
                        if (valorous_oath_blocks_auto_attack(m_ptr))
                        {
                            msg_print("You forgo a controlled retreat attack to avoid striking a fleeing foe.");
                            return;
                        }

                        py_attack(fy, fx, ATT_CONTROLLED_RETREAT);
                        return;
                    }
                }
            }
        }
    }
}

/*
 * Move player in the given direction, with the given "pickup" flag.
 *
 * This routine should only be called when energy has been expended.
 *
 * Note that this routine handles monsters in the destination grid,
 * and also handles attempting to move into walls/doors/rubble/etc.
 */
void move_player(int dir)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int y, x;

    /* Find the result of moving */
    y = py + ddy[dir];
    x = px + ddx[dir];

    /* deal with leaving the map */
    if ((y < 0) || (x < 0) || (y >= p_ptr->cur_map_hgt)
        || (x >= p_ptr->cur_map_wid))
    {
        do_cmd_escape(silmarils_possessed());
        return;
    }

    /* Hack -- attack visible monsters */
    if ((cave_m_idx[y][x] > 0) && mon_list[cave_m_idx[y][x]].ml)
    {
        /* Attack */
        py_attack(y, x, ATT_MAIN);
    }

    /* open known doors on movement */
    else if ((cave_info[y][x] & (CAVE_MARK))
        && cave_known_closed_door_bold(y, x))
    {
        /* Open */
        do_cmd_open_aux(y, x);
    }

    /* Player can not walk through "walls", but can go through traps */
    else if (!cave_floor_bold(y, x))
    {
        log_debug("cmd_walk: Hit wall at (%d, %d)", y, x);
        /* Disturb the player */
        disturb(0, 0);

        /* Notice unknown obstacles */
        if (!(cave_info[y][x] & (CAVE_MARK)))
        {
            /* Rubble */
            if (cave_feat[y][x] == FEAT_RUBBLE)
            {
                message(MSG_HITWALL, 0,
                    "You feel a pile of rubble blocking your way.");
                cave_info[y][x] |= (CAVE_MARK);
                lite_spot(y, x);
            }

            /* Closed door */
            else if (cave_known_closed_door_bold(y, x))
            {
                message(MSG_HITWALL, 0, "You feel a door blocking your way.");
                cave_info[y][x] |= (CAVE_MARK);
                lite_spot(y, x);
            }

            /* Wall (or secret door) */
            else
            {
                message(MSG_HITWALL, 0, "You feel a wall blocking your way.");
                cave_info[y][x] |= (CAVE_MARK);
                lite_spot(y, x);
            }
        }

        /* Mention known obstacles */
        else
        {
            /* Rubble */
            if (cave_feat[y][x] == FEAT_RUBBLE)
            {
                message(MSG_HITWALL, 0,
                    "There is a pile of rubble blocking your way.");
            }

            /* Closed door */
            else if (cave_known_closed_door_bold(y, x))
            {
                message(MSG_HITWALL, 0, "There is a door blocking your way.");
            }

            /* Wall (or secret door) */
            else
            {
                message(MSG_HITWALL, 0, "There is a wall blocking your way.");
            }
        }

        // store the action type
        p_ptr->previous_action[0] = ACTION_MISC;
    }

    /* Normal movement */
    else
    {
        // deal with overburdened characters
        if (p_ptr->total_weight > weight_limit() * 3 / 2)
        {
            /* Abort */
            msg_print("You are too burdened to move.");

            /* Disturb the player */
            disturb(0, 0);

            // don't take a turn...
            p_ptr->energy_use = 0;

            return;
        }

        /* Check before walking on known traps/chasms on movement */
        if ((!p_ptr->confused) && (cave_info[y][x] & (CAVE_MARK)))
        {
            // leapable things: chasms, traps (except roosts and webs)
            if ((cave_feat[y][x] == FEAT_CHASM)
                || (((cave_trap_bold(y, x)) && !cave_floorlike_bold(y, x))
                    && !(cave_feat[y][x] == FEAT_TRAP_ROOST
                        || cave_feat[y][x] == FEAT_TRAP_WEB)))
            {
                char prompt[80];
                int i;
                int d;
                bool run_up = false;
                bool confirm = true;

                // test all three directions roughly towards the chasm/pit
                for (i = -1; i <= 1; i++)
                {
                    d = cycle[chome[dir_from_delta(
                                  y - p_ptr->py, x - p_ptr->px)]
                        + i];

                    // if the last action was a move in this direction, we have
                    // a valid run_up
                    if (p_ptr->previous_action[1] == d)
                        run_up = true;
                }

                if (p_ptr->active_ability[S_EVN][EVN_LEAPING])
                {
                    int y_mid, x_mid; // the midpoint of the leap
                    int y_end, x_end; // the endpoint of the leap

                    /* Get location */
                    y_mid = p_ptr->py + ddy[dir];
                    x_mid = p_ptr->px + ddx[dir];
                    y_end = y_mid + ddy[dir];
                    x_end = x_mid + ddx[dir];

                    /* Disturb the player */
                    disturb(0, 0);

                    /* Flush input */
                    flush();

                    // Can't jump from within pits
                    if (cave_pit_bold(p_ptr->py, p_ptr->px))
                    {
                        msg_print("You cannot leap from within a pit.");
                    }

                    // Can't jump from within webs
                    else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB)
                    {
                        msg_print("You cannot leap from within a web.");
                    }

                    // Can't jump without a run up
                    else if (!run_up)
                    {
                        msg_print("You cannot leap without a run up.");
                    }

                    // need room to land
                    else if ((cave_info[y_end][x_end] & (CAVE_MARK))
                        && (cave_wall_bold(y_end, x_end)
                            || cave_any_closed_door_bold(y_end, x_end)))
                    {
                        msg_print("You cannot leap over as there is no room to "
                                  "land.");
                    }

                    else
                    {
                        // confirm if the destination is unknown
                        if (!(cave_info[y_end][x_end] & (CAVE_SEEN))
                            && !(cave_info[y_end][x_end] & (CAVE_MARK)))
                        {
                            strnfmt(prompt, sizeof(prompt),
                                "Are you sure you wish to leap into the "
                                "unknown? ");
                        }

                        // confirm if the destination is in the chasm
                        else if (cave_feat[y_end][x_end] == FEAT_CHASM)
                        {
                            if (p_ptr->depth >= MORGOTH_DEPTH)
                            {
                                strnfmt(prompt, sizeof(prompt),
                                    "Are you sure you wish to leap into the "
                                    "abyss? You will surely die. ");
                            }
                            else
                            {
                                strnfmt(prompt, sizeof(prompt),
                                    "Are you sure you wish to leap into the "
                                    "abyss? ");
                            }
                        }

                        // confirm if the destination has a visible monster
                        else if ((cave_m_idx[y_end][x_end] > 0)
                            && (&mon_list[cave_m_idx[y_end][x_end]])->ml)
                        {
                            monster_type* m_ptr
                                = &mon_list[cave_m_idx[y_end][x_end]];
                            char m_name[80];

                            /* Get the monster name */
                            monster_desc(m_name, sizeof(m_name), m_ptr, 0);

                            strnfmt(prompt, sizeof(prompt),
                                "Are you sure you wish to leap into %s? ",
                                m_name);
                        }

                        // default confirmation
                        else
                        {
                            confirm = false;
                            // strnfmt(prompt, sizeof(prompt), "Leap over the
                            // %s? ", f_name +
                            // f_info[cave_feat[y_mid][x_mid]].name);
                        }

                        // if you say 'yes' to the prompt, then try to leap
                        if (!confirm || get_check(prompt))
                        {
                            // at this point attack any invisible monster that
                            // may be there
                            if (cave_m_idx[y][x] > 0)
                            {
                                msg_print("An unseen foe blocks your way.");

                                /* Attack */
                                py_attack(y, x, ATT_MAIN);

                                return;
                            }

                            // otherwise do the leap!
                            else
                            {
                                // we generously give you your free flanking
                                // attack...
                                flanking_or_retreat(y_mid, x_mid);

                                /* Take a turn */
                                p_ptr->energy_use = 100;

                                // store the action type
                                p_ptr->previous_action[0] = dir;

                                // move player to the new position
                                monster_swap(
                                    p_ptr->py, p_ptr->px, y_mid, x_mid);

                                // remember that the player is in the air now
                                p_ptr->leaping = true;

                                return;
                            }
                        }
                    }
                }

                // if the player hasn't already leapt
                if (!p_ptr->leaping && (cave_feat[y][x] == FEAT_CHASM))
                {
                    /* Disturb the player */
                    disturb(0, 0);

                    /* Flush input */
                    flush();

                    cptr prompt = "Step into the chasm? ";
                    if (p_ptr->depth >= MORGOTH_DEPTH)
                        prompt = "Step into the chasm? You will surely die. ";

                    if (!get_check(prompt))
                    {
                        // don't take a turn...
                        p_ptr->energy_use = 0;

                        return;
                    }
                }
            }

            // traps
            if ((cave_trap_bold(y, x) && !cave_floorlike_bold(y, x)))
            {
                /* Disturb the player */
                disturb(0, 0);

                /* Flush input */
                flush();

                if (!get_check("Are you sure you want to step on the trap? "))
                {
                    // don't take a turn...
                    p_ptr->energy_use = 0;

                    return;
                }
            }
        }

        // if there is an invisible monster present and you haven't yet
        // attacked, do so now
        if (cave_m_idx[y][x] > 0)
        {
            msg_print("An unseen foe blocks your way.");

            /* Attack */
            py_attack(y, x, ATT_MAIN);

            return;
        }

        // It is hard to get out of a pit
        if (cave_pit_bold(py, px))
        {
            int difficulty;

            if (cave_feat[py][px] == FEAT_TRAP_PIT)
                difficulty = 10;
            else
                difficulty = 15;

            /* Disturb the player */
            disturb(0, 0);

            if (check_hit(difficulty, false))
            {
                msg_print("You try to climb out of the pit, but fail.");

                /* Take a turn */
                p_ptr->energy_use = 100;

                // store the action type
                p_ptr->previous_action[0] = ACTION_MISC;

                return;
            }
            else
            {
                msg_print("You climb out of the pit.");
            }
        }

        // It is hard to get out of a web
        if (cave_feat[py][px] == FEAT_TRAP_WEB)
        {
            if (!break_free_of_web())
                return;
        }

        if ((p_ptr->depth == MORGOTH_DEPTH) && !p_ptr->morgoth_hall_entered
            && (cave_info[y][x] & CAVE_G_VAULT))
        {
            if (!preconfirm_enter_morgoth_hall())
            {
                disturb(0, 0);
                p_ptr->energy_use = 0;
                return;
            }
        }

        /* Sound */
        sound(MSG_WALK);

        // do flanking or controlled retreat attack if any
        flanking_or_retreat(y, x);

        /* Move player */
        monster_swap(py, px, y, x);

        /* Check for Mandos quest interaction after movement */
        check_mandos_quest_interaction();
        
        /* Check for Niena quest completion after movement */
        check_niena_quest_completion();

        if (cave_feat[y][x] == FEAT_SUNLIGHT
            && cave_feat[py][px] != FEAT_SUNLIGHT)
        {
            msg_print("You step into a patch of sunlight.");
        }
        else if (cave_feat[y][x] != FEAT_SUNLIGHT
            && cave_feat[py][px] == FEAT_SUNLIGHT)
        {
            msg_print("You step out of the sunlight.");
        }

        /* New location */
        y = py = p_ptr->py;
        x = px = p_ptr->px;

        /* Chasm sanctum EVIL drops trigger their ambush on entry. */
        trigger_chasm_sanctum_ambush_if_needed(y, x);

        /* Spontaneous Searching */
        perceive();

        // remember this direction of movement
        p_ptr->previous_action[0] = dir;

        /* Discover stairs if blind */
        if (cave_stair_bold(y, x))
        {
            cave_info[y][x] |= (CAVE_MARK);
            lite_spot(y, x);
        }

        /* Remark on Forge and discover it if blind */
        if (cave_forge_bold(p_ptr->py, p_ptr->px))
        {
            if ((cave_feat[p_ptr->py][p_ptr->px] >= FEAT_FORGE_UNIQUE_HEAD)
                && !p_ptr->unique_forge_seen)
            {
                msg_print("You enter the forge 'Orodruth' - the Mountain's "
                          "Anger - where "
                          "Grond was made in days of old.");
                msg_print("The fires burn still.");
                p_ptr->unique_forge_seen = true;
                do_cmd_note("Entered the forge 'Orodruth'", p_ptr->depth);
            }

            else
            {
                char* article;

                if (cave_feat[p_ptr->py][p_ptr->px] >= FEAT_FORGE_UNIQUE_HEAD)
                    article = "the";
                else if (cave_feat[p_ptr->py][p_ptr->px]
                    >= FEAT_FORGE_GOOD_HEAD)
                    article = "an";
                else
                    article = "a";

                msg_format("You enter %s %s.", article,
                    f_name + f_info[cave_feat[p_ptr->py][p_ptr->px]].name);
            }

            cave_info[y][x] |= (CAVE_MARK);
            lite_spot(y, x);
        }

        /* Set off traps */
        if (cave_trap_bold(y, x) || (cave_feat[y][x] == FEAT_CHASM))
        {
            // If it is hidden
            if (cave_info[y][x] & (CAVE_HIDDEN))
            {
                /* Reveal the trap */
                reveal_trap(y, x);
            }

            /* Hit the trap */
            hit_trap(y, x);
        }

        // read any notes the player stumbles upon
        if ((cave_o_idx[p_ptr->py][p_ptr->px] != 0))
        {
            object_type* o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
            if (o_ptr->tval == TV_NOTE)
            {
                note_info_screen(o_ptr);
            }
        }
    }
}

/*
 * Hack -- Check for a "known wall" (see below)
 */
static int see_wall(int dir, int y, int x)
{
    /* Get the new location */
    y += ddy[dir];
    x += ddx[dir];

    /* Illegal grids are not known walls XXX XXX XXX */
    if (!in_bounds(y, x))
        return (false);

    /* Non-wall grids are not known walls */
    if (!cave_wall_bold(y, x))
        return (false);

    /* Unknown walls are not known walls */
    if (!(cave_info[y][x] & (CAVE_MARK)))
        return (false);

    /* Default */
    return (true);
}

/*
 * Hack -- Check for an "unknown corner" (see below)
 */
// static int see_nothing(int dir, int y, int x)
//{
//	/* Get the new location */
//	y += ddy[dir];
//	x += ddx[dir];

//	/* Illegal grids are unknown XXX XXX XXX */
//	if (!in_bounds(y, x)) return (true);

//	/* Memorized grids are always known */
//	if (cave_info[y][x] & (CAVE_MARK)) return (false);

//	/* Default */
//	return (true);
//}

/*
 * The running algorithm  -CJS-
 *
 * Basically, once you start running, you keep moving until something
 * interesting happens.  In an enclosed space, you run straight, but
 * you follow corners as needed (i.e. hallways).  In an open space,
 * you run straight, but you stop before entering an enclosed space
 * (i.e. a room with a doorway).  In a semi-open space (with walls on
 * one side only), you run straight, but you stop before entering an
 * enclosed space or an open space (i.e. running along side a wall).
 *
 * All discussions below refer to what the player can see, that is,
 * an unknown wall is just like a normal floor.  This means that we
 * must be careful when dealing with "illegal" grids.
 *
 * No assumptions are made about the layout of the dungeon, so this
 * algorithm works in hallways, rooms, destroyed areas, etc.
 *
 * In the diagrams below, the player has just arrived in the grid
 * marked as '@', and he has just come from a grid marked as 'o',
 * and he is about to enter the grid marked as 'x'.
 *
 * Running while confused is not allowed, and so running into a wall
 * is only possible when the wall is not seen by the player.  This
 * will take a turn and stop the running.
 *
 * Several conditions are tracked by the running variables.
 *
 *   p_ptr->run_open_area (in the open on at least one side)
 *   p_ptr->run_break_left (wall on the left, stop if it opens)
 *   p_ptr->run_break_right (wall on the right, stop if it opens)
 *
 * When running begins, these conditions are initialized by examining
 * the grids adjacent to the requested destination grid (marked 'x'),
 * two on each side (marked 'L' and 'R').  If either one of the two
 * grids on a given side is a wall, then that side is considered to
 * be "closed".  Both sides enclosed yields a hallway.
 *
 *    LL                     @L
 *    @x      (normal)       RxL   (diagonal)
 *    RR      (east)          R    (south-east)
 *
 * In the diagram below, in which the player is running east along a
 * hallway, he will stop as indicated before attempting to enter the
 * intersection (marked 'x').  Starting a new run in any direction
 * will begin a new hallway run.
 *
 *  #.#
 * ##.##
 * o@x..
 * ##.##
 *  #.#
 *
 * Note that a minor hack is inserted to make the angled corridor
 * entry (with one side blocked near and the other side blocked
 * further away from the runner) work correctly. The runner moves
 * diagonally, but then saves the previous direction as being
 * straight into the gap. Otherwise, the tail end of the other
 * entry would be perceived as an alternative on the next move.
 *
 * In the diagram below, the player is running east down a hallway,
 * and will stop in the grid (marked '1') before the intersection.
 * Continuing the run to the south-east would result in a long run
 * stopping at the end of the hallway (marked '2').
 *
 * ##################
 * o@x       1
 * ########### ######
 * #2          #
 * #############
 *
 * After each step, the surroundings are examined to determine if
 * the running should stop, and to determine if the running should
 * change direction.  We examine the new current player location
 * (at which the runner has just arrived) and the direction from
 * which the runner is considered to have come.
 *
 * Moving one grid in some direction places you adjacent to three
 * or five new grids (for straight and diagonal moves respectively)
 * to which you were not previously adjacent (marked as '!').
 *
 *   ...!              ...
 *   .o@!  (normal)    .o.!  (diagonal)
 *   ...!  (east)      ..@!  (south east)
 *                      !!!
 *
 * If any of the newly adjacent grids are "interesting" (monsters,
 * objects, some terrain features) then running stops.
 *
 * If any of the newly adjacent grids seem to be open, and you are
 * looking for a break on that side, then running stops.
 *
 * If any of the newly adjacent grids do not seem to be open, and
 * you are in an open area, and the non-open side was previously
 * entirely open, then running stops.
 *
 * If you are in a hallway, then the algorithm must determine if
 * the running should continue, turn, or stop.  If only one of the
 * newly adjacent grids appears to be open, then running continues
 * in that direction, turning if necessary.  If there are more than
 * two possible choices, then running stops.  If there are exactly
 * two possible choices, separated by a grid which does not seem
 * to be open, then running stops.  Otherwise, as shown below, the
 * player has probably reached a "corner".
 *
 *    ###             o##
 *    o@x  (normal)   #@!   (diagonal)
 *    ##!  (east)     ##x   (south east)
 *
 * In this situation, there will be two newly adjacent open grids,
 * one touching the player on a diagonal, and one directly adjacent.
 * We must consider the two "option" grids further out (marked '?').
 * We assign "option" to the straight-on grid, and "option2" to the
 * diagonal grid.
 *
 *    ###s
 *    o@x?   (may be incorrect diagram!)
 *    ##!?
 *
 * If both "option" grids are closed, then there is no reason to enter
 * the corner, and so we can cut the corner, by moving into the other
 * grid (diagonally).  If we choose not to cut the corner, then we may
 * go straight, but we pretend that we got there by moving diagonally.
 * Below, we avoid the obvious grid (marked 'x') and cut the corner
 * instead (marked 'n').
 *
 *    ###:               o##
 *    o@x#   (normal)    #@n    (maybe?)
 *    ##n#   (east)      ##x#
 *                       ####
 *
 * If one of the "option" grids is open, then we may have a choice, so
 * we check to see whether it is a potential corner or an intersection
 * (or room entrance).  If the grid two spaces straight ahead, and the
 * space marked with 's' are both open, then it is a potential corner
 * and we enter it if requested.  Otherwise, we stop, because it is
 * not a corner, and is instead an intersection or a room entrance.
 *
 *    ###
 *    o@x
 *    ##!#
 *
 * (This documentation may no longer be correct)
 */

/*
 * Hack -- allow quick "cycling" through the legal directions
 */
const byte cycle[] = { 1, 2, 3, 6, 9, 8, 7, 4, 1, 2, 3, 6, 9, 8, 7, 4, 1, 2, 3,
    6, 9, 8, 7, 4 };

/*
 * Hack -- map each direction into the "middle" of the "cycle[]" array
 */
const byte chome[] = { 0, 8, 9, 10, 15, 0, 11, 14, 13, 12 };

/*
 * Initialize the running algorithm for a new direction.
 *
 * Diagonal Corridor -- allow diaginal entry into corridors.
 *
 * Blunt Corridor -- If there is a wall two spaces ahead and
 * we seem to be in a corridor, then force a turn into the side
 * corridor, must be moving straight into a corridor here. (?)
 *
 * Diagonal Corridor    Blunt Corridor (?)
 *       # #                  #
 *       #x#                 @x#
 *       @p.                  p
 */
static void run_init(int dir)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int i, row, col;

    bool deepleft, deepright;
    bool shortleft, shortright;

    /* Save the direction */
    p_ptr->run_cur_dir = dir;

    /* Assume running straight */
    p_ptr->run_old_dir = dir;

    /* Assume looking for open area */
    p_ptr->run_open_area = true;

    /* Assume not looking for breaks */
    p_ptr->run_break_right = false;
    p_ptr->run_break_left = false;

    /* Assume no nearby walls */
    deepleft = deepright = false;
    shortright = shortleft = false;

    /* Find the destination grid */
    row = py + ddy[dir];
    col = px + ddx[dir];

    /* Extract cycle index */
    i = chome[dir];

    /* Check for nearby wall */
    if (see_wall(cycle[i + 1], py, px))
    {
        p_ptr->run_break_left = true;
        shortleft = true;
    }

    /* Check for distant wall */
    else if (see_wall(cycle[i + 1], row, col))
    {
        p_ptr->run_break_left = true;
        deepleft = true;
    }

    /* Check for nearby wall */
    if (see_wall(cycle[i - 1], py, px))
    {
        p_ptr->run_break_right = true;
        shortright = true;
    }

    /* Check for distant wall */
    else if (see_wall(cycle[i - 1], row, col))
    {
        p_ptr->run_break_right = true;
        deepright = true;
    }

    /* Looking for a break */
    if (p_ptr->run_break_left && p_ptr->run_break_right)
    {
        /* Not looking for open area */
        p_ptr->run_open_area = false;

        /* Hack -- allow angled corridor entry */
        if (dir & 0x01)
        {
            if (deepleft && !deepright)
            {
                p_ptr->run_old_dir = cycle[i - 1];
            }
            else if (deepright && !deepleft)
            {
                p_ptr->run_old_dir = cycle[i + 1];
            }
        }

        /* Hack -- allow blunt corridor entry */
        else if (see_wall(cycle[i], row, col))
        {
            if (shortleft && !shortright)
            {
                p_ptr->run_old_dir = cycle[i - 2];
            }
            else if (shortright && !shortleft)
            {
                p_ptr->run_old_dir = cycle[i + 2];
            }
        }
    }
}

/*
 * Update the current "run" path
 *
 * Return true if the running should be stopped
 */
static bool run_test(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;

    int prev_dir;
    int new_dir;

    int row, col;
    int i, max, inv;
    int option, option2;

    /* No options yet */
    option = 0;
    option2 = 0;

    /* Where we came from */
    prev_dir = p_ptr->run_old_dir;

    /* Range of newly adjacent grids */
    max = (prev_dir & 0x01) + 1;

    /* Look at every newly adjacent square. */
    for (i = -max; i <= max; i++)
    {
        object_type* o_ptr;

        /* New direction */
        new_dir = cycle[chome[prev_dir] + i];

        /* New location */
        row = py + ddy[new_dir];
        col = px + ddx[new_dir];

        /* Visible monsters abort running */
        if (cave_m_idx[row][col] > 0)
        {
            monster_type* m_ptr = &mon_list[cave_m_idx[row][col]];

            /* Visible monster */
            if (m_ptr->ml)
                return (true);
        }

        /* Visible objects abort running */
        for (o_ptr = get_first_object(row, col); o_ptr;
             o_ptr = get_next_object(o_ptr))
        {
            /* Visible object */
            if (o_ptr->marked && !object_is_searched_skeleton(o_ptr))
                return (true);
        }

        /* Assume unknown */
        inv = true;

        /* Check memorized grids */
        if (cave_info[row][col] & (CAVE_MARK))
        {
            bool notice = true;

            /* Examine the terrain */
            switch (cave_feat[row][col])
            {
            /* Floors */
            case FEAT_FLOOR:

            /* Secret doors */
            case FEAT_SECRET:

            /* Walls */
            case FEAT_QUARTZ:
            case FEAT_WALL_EXTRA:
            case FEAT_WALL_INNER:
            case FEAT_WALL_OUTER:
            case FEAT_WALL_SOLID:
            case FEAT_WALL_PERM:
            {
                /* Ignore */
                notice = false;

                /* Done */
                break;
            }

            /* Open doors */
            case FEAT_OPEN:
            case FEAT_BROKEN:
            {
                /* ignore */
                notice = false;

                /* Done */
                break;
            }

            /* Stairs */
            case FEAT_LESS:
            case FEAT_MORE:
            case FEAT_LESS_SHAFT:
            case FEAT_MORE_SHAFT:
            {
                /* Done */
                break;
            }

            /* Deal with traps */
            default:
            {
                // ignore hidden traps
                if (cave_floorlike_bold(row, col))
                {
                    /* ignore */
                    notice = false;

                    /* Done */
                    break;
                }
            }
            }

            /* Interesting feature */
            if (notice)
                return (true);

            /* The grid is "visible" */
            inv = false;
        }

        /* Analyze unknown grids and floors */
        if (inv || cave_floor_bold(row, col))
        {
            /* Looking for open area */
            if (p_ptr->run_open_area)
            {
                /* Nothing */
            }

            /* The first new direction. */
            else if (!option)
            {
                option = new_dir;
            }

            /* Three new directions. Stop running. */
            else if (option2)
            {
                return (true);
            }

            /* Two non-adjacent new directions.  Stop running. */
            else if (option != cycle[chome[prev_dir] + i - 1])
            {
                return (true);
            }

            /* Two new (adjacent) directions (case 1) */
            else if (new_dir & 0x01)
            {
                option2 = new_dir;
            }

            /* Two new (adjacent) directions (case 2) */
            else
            {
                option2 = option;
                option = new_dir;
            }
        }

        /* Obstacle, while looking for open area */
        else
        {
            if (p_ptr->run_open_area)
            {
                if (i < 0)
                {
                    /* Break to the right */
                    p_ptr->run_break_right = true;
                }

                else if (i > 0)
                {
                    /* Break to the left */
                    p_ptr->run_break_left = true;
                }
            }
        }
    }

    // Now check to see if running another step would bring us next to an
    // immobile monster (such as a mold).
    /* Look at every soon to be newly adjacent square. */
    for (i = -max; i <= max; i++)
    {
        /* New direction */
        new_dir = cycle[chome[prev_dir] + i];

        /* New location */
        row = py + ddy[prev_dir] + ddy[new_dir];
        col = px + ddx[prev_dir] + ddx[new_dir];

        /* Visible immovable monsters abort running */
        if (cave_m_idx[row][col] > 0)
        {
            monster_type* m_ptr = &mon_list[cave_m_idx[row][col]];
            monster_race* r_ptr = &r_info[m_ptr->r_idx];

            /* Visible monster */
            if (m_ptr->ml && (r_ptr->flags1 & (RF1_NEVER_MOVE)))
                return (true);
        }
    }

    /* Looking for open area */
    if (p_ptr->run_open_area)
    {
        /* Hack -- look again */
        for (i = -max; i < 0; i++)
        {
            new_dir = cycle[chome[prev_dir] + i];

            row = py + ddy[new_dir];
            col = px + ddx[new_dir];

            /* Unknown grid or non-wall */
            /* Was: cave_floor_bold(row, col) */
            if (!(cave_info[row][col] & (CAVE_MARK))
                || (!cave_wall_bold(row, col)))
            {
                /* Looking to break right */
                if (p_ptr->run_break_right)
                {
                    return (true);
                }
            }

            /* Obstacle */
            else
            {
                /* Looking to break left */
                if (p_ptr->run_break_left)
                {
                    return (true);
                }
            }
        }

        /* Hack -- look again */
        for (i = max; i > 0; i--)
        {
            new_dir = cycle[chome[prev_dir] + i];

            row = py + ddy[new_dir];
            col = px + ddx[new_dir];

            /* Unknown grid or non-wall */
            /* Was: cave_floor_bold(row, col) */
            if (!(cave_info[row][col] & (CAVE_MARK))
                || (!cave_wall_bold(row, col)))
            {
                /* Looking to break left */
                if (p_ptr->run_break_left)
                {
                    return (true);
                }
            }

            /* Obstacle */
            else
            {
                /* Looking to break right */
                if (p_ptr->run_break_right)
                {
                    return (true);
                }
            }
        }
    }

    /* Not looking for open area */
    else
    {
        /* No options */
        if (!option)
        {
            return (true);
        }

        /* One option */
        else if (!option2)
        {
            /* Primary option */
            p_ptr->run_cur_dir = option;

            /* No other options */
            p_ptr->run_old_dir = option;
        }

        /* Two options, examining corners */
        else
        {
            /* Primary option */
            p_ptr->run_cur_dir = option;

            /* Stop in the doorway of a room */
            row = py + 2 * ddy[option];
            col = px + 2 * ddx[option];
            if ((cave_info[row][col] & CAVE_MARK) && !cave_wall_bold(row, col))
            {
                return (true);
            }

            /* Hack -- allow curving */
            p_ptr->run_old_dir = option2;
        }
    }

    /* About to hit a known wall, stop */
    if (see_wall(p_ptr->run_cur_dir, py, px))
    {
        return (true);
    }

    /* Failure */
    return (false);
}

/*
 * Take one step along the current "run" path
 *
 * Called with a real direction to begin a new run, and with zero
 * to continue a run in progress.
 */
void run_step(int dir)
{
    /* Start run */
    if (dir)
    {
        /* Initialize */
        run_init(dir);

        /* Hack -- Set the run counter */
        p_ptr->running = (p_ptr->command_arg ? p_ptr->command_arg : 1000);
    }

    /* Continue run */
    else
    {
        /* Update run */
        if (run_test())
        {
            /* Disturb */
            disturb(0, 0);

            /* Done */
            return;
        }
    }

    /* Decrease counter */
    p_ptr->running--;

    /* Take time */
    p_ptr->energy_use = 100;

    /* Move the player */
    move_player(p_ptr->run_cur_dir);
}







