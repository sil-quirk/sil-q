#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include <math.h>

static bool valorous_oath_blocks_auto_attack(monster_type* m_ptr);

static void player_face_grid_before_attack(int y, int x)
{
    player_set_visual_facing_target_immediate(y, x);
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
int skill_check_details(monster_type* m_ptr1, int skill, int difficulty,
    monster_type* m_ptr2, skill_roll_details* details)
{
    int skill_total;
    int difficulty_total;
    int skill_total_alt;
    int difficulty_total_alt;
    int skill_die;
    int difficulty_die;
    int skill_die_alt;
    int difficulty_die_alt;
    bool skill_curse_active = false;
    bool difficulty_curse_active = false;
    bool skill_alt_used = false;
    bool difficulty_alt_used = false;

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

    skill_die = skill_total - skill;
    difficulty_die = difficulty_total - difficulty;
    skill_die_alt = skill_total_alt - skill;
    difficulty_die_alt = difficulty_total_alt - difficulty;

    // player curse?
    if (p_ptr->cursed)
    {
        skill_curse_active = (m_ptr1 == PLAYER);
        difficulty_curse_active = (m_ptr2 == PLAYER);
        if (m_ptr1 == PLAYER)
        {
            skill_alt_used = (skill_total_alt < skill_total);
            skill_total = MIN(skill_total, skill_total_alt);
        }
        if (m_ptr2 == PLAYER)
        {
            difficulty_alt_used = (difficulty_total_alt < difficulty_total);
            difficulty_total = MIN(difficulty_total, difficulty_total_alt);
        }
    }

    if (details)
    {
        memset(details, 0, sizeof(*details));
        details->skill = skill;
        details->difficulty = difficulty;
        details->skill_die = skill_total - skill;
        details->difficulty_die = difficulty_total - difficulty;
        details->skill_die_primary = skill_die;
        details->difficulty_die_primary = difficulty_die;
        details->skill_die_alt = skill_die_alt;
        details->difficulty_die_alt = difficulty_die_alt;
        details->skill_total = skill_total;
        details->difficulty_total = difficulty_total;
        details->result = skill_total - difficulty_total;
        details->skill_curse_active = skill_curse_active;
        details->difficulty_curse_active = difficulty_curse_active;
        details->skill_alt_used = skill_alt_used;
        details->difficulty_alt_used = difficulty_alt_used;
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

int skill_check(
    monster_type* m_ptr1, int skill, int difficulty, monster_type* m_ptr2)
{
    return skill_check_details(m_ptr1, skill, difficulty, m_ptr2, NULL);
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
int hit_roll_details(int att, int evn, const monster_type* m_ptr1,
    const monster_type* m_ptr2, bool display_roll, int* attack_die,
    int* evasion_die)
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

    if (attack_die)
        *attack_die = attack_score - att;
    if (evasion_die)
        *evasion_die = evasion_score - evn;

    return (attack_score - evasion_score);
}

int hit_roll(int att, int evn, const monster_type* m_ptr1,
    const monster_type* m_ptr2, bool display_roll)
{
    return hit_roll_details(
        att, evn, m_ptr1, m_ptr2, display_roll, NULL, NULL);
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

void ident_weapon_by_use_context(object_type* o_ptr,
    const monster_type* m_ptr, u32b flag, cptr context)
{
    char o_short_name[80];
    char o_full_name[80];
    char slay_description[160];

    /* Short, pre-identification object description */
    object_desc(o_short_name, sizeof(o_short_name), o_ptr, false, 0);

    /* Description of the 'slay' */
    slay_desc(slay_description, flag, m_ptr);

    /* Print the messages */
    if (context && context[0])
        msg_format(
            "Your %s %s %s.", context, o_short_name, slay_description);
    else
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

void ident_weapon_by_use(
    object_type* o_ptr, const monster_type* m_ptr, u32b flag)
{
    ident_weapon_by_use_context(o_ptr, m_ptr, flag, NULL);
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
 * Extra damage dice that depth grants to a dungeon trap.
 *
 * Trap damage is otherwise fixed for the whole game, so traps lose all bite
 * once the player's HP and armour outscale them.  This grants roughly one
 * extra die for every `feet` levels of depth (moderate scaling: a small bump
 * from the start, steeper deep down).  Returns 0 in the town.
 */
static int trap_depth_dice(int feet)
{
    if ((feet <= 0) || (p_ptr->depth <= 0))
        return 0;
    return p_ptr->depth / feet;
}

/*
 * Handle player hitting a real trap
 */
void hit_trap(int y, int x)
{
    int i, dam, prt, net_dam;
    int feat = cave_feat[y][x];

    cptr name = "a trap";

    /* A trap the player has rewired is keyed to spare them -- they know the
     * altered mechanism and pass over it freely.  It still catches monsters.
     * Return before disturbing so it behaves like safe floor underfoot. */
    if (cave_rewired[y][x])
        return;

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

        /* Falling damage (deeper pits hit harder) */
        dam = damroll(2 + trap_depth_dice(7), 4);

        update_combat_rolls1b(NULL, PLAYER, true);
        update_combat_rolls2(
            2 + trap_depth_dice(7), 4, dam, -1, -1, 0, 0, GF_HURT, false);

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

        /* Falling damage (deeper pits hit harder) */
        dam = damroll(2 + trap_depth_dice(7), 4);

        update_combat_rolls1b(NULL, PLAYER, true);
        update_combat_rolls2(
            2 + trap_depth_dice(7), 4, dam, -1, -1, 0, 0, GF_HURT, false);

        /* Take the damage */
        killer_mark_other(SCORE_KILLER_FALL);
        take_hit(dam, name);

        /* Extra spike damage (more spikes deeper down) */
        dam = damroll(4 + trap_depth_dice(6), 5);

        /* Protection */
        prt = protection_roll(GF_HURT, true);

        net_dam = (dam - prt > 0) ? (dam - prt) : 0;

        update_combat_rolls1b(NULL, PLAYER, true);
        update_combat_rolls2(
            4 + trap_depth_dice(6), 5, dam, -1, -1, prt, 100, GF_HURT, true);

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
            /* An envenomed dart: real (depth-scaled) damage, and it saps
             * strength even through armour -- the poison, not the impact,
             * does the draining.  The drain still allows a Will save / is
             * stopped by Sustain Strength inside do_dec_stat(). */
            dam = damroll(1 + trap_depth_dice(8), 6);
            prt = protection_roll(GF_HURT, false);

            net_dam = (dam - prt > 0) ? (dam - prt) : 0;

            update_combat_rolls1b(NULL, PLAYER, true);
            update_combat_rolls2(1 + trap_depth_dice(8), 6, dam, -1, -1, prt,
                100, GF_HURT, false);

            if (net_dam > 0)
            {
                msg_print("A small dart hits you!");

                killer_mark_other(SCORE_KILLER_TRAP);
                take_hit(net_dam, name);

                /* The venom only enters if the dart broke the skin -- armour
                 * that soaks all the damage also stops the poison.  The drain
                 * still allows a Will save / is stopped by Sustain Strength. */
                (void)do_dec_stat(A_STR, NULL);
            }
            else
            {
                msg_print(
                    "A small dart hits you, but is deflected by your armour.");
            }
        }
        else
        {
            msg_print("A small dart barely misses you.");
            update_combat_rolls_no_damage();
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

        /* Acid damage (stronger deeper down) */
        dam = damroll(4 + trap_depth_dice(7), 4);

        /* Protection */
        prt = protection_roll(GF_HURT, false);

        net_dam = (dam - prt > 0) ? (dam - prt) : 0;

        update_combat_rolls1b(NULL, PLAYER, true);
        update_combat_rolls2(
            4 + trap_depth_dice(7), 4, dam, -1, -1, prt, 100, GF_HURT, false);

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

            dam = damroll(1 + trap_depth_dice(12), 4);

            update_combat_rolls1b(NULL, PLAYER, true);
            update_combat_rolls2(
                1 + trap_depth_dice(12), 4, dam, -1, -1, 0, 0, GF_HURT, true);

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
            dam = damroll(6 + trap_depth_dice(6), 8);

            /* Protection */
            prt = protection_roll(GF_HURT, false);

            net_dam = (dam - prt > 0) ? (dam - prt) : 0;

            update_combat_rolls1b(NULL, PLAYER, true);
            update_combat_rolls2(
                6 + trap_depth_dice(6), 8, dam, -1, -1, prt, 100, GF_HURT,
                false);

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
                dam = damroll(4 + trap_depth_dice(6), 8);

                /* Protection */
                prt = protection_roll(GF_HURT, false);

                update_combat_rolls2(4 + trap_depth_dice(6), 8, dam, -1, -1,
                    prt, 100, GF_HURT, false);

                net_dam = (dam - prt > 0) ? (dam - prt) : 0;

                if (allow_player_stun(NULL))
                {
                    (void)set_stun(p_ptr->stun + dam * 4);
                }
            }
            else
            {
                msg_print("You nimbly dodge the falling rock!");
                update_combat_rolls_no_damage();
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

    if (player_active_weapon_is_melee()
        && p_ptr->active_ability[S_MEL][MEL_CHARGE] && (p_ptr->pspeed > 1)
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

    if (!player_active_weapon_is_melee())
        return;

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

    if (!player_active_weapon_is_melee())
        return;

    /* Get the monster */
    m_idx = cave_m_idx[y][x];
    m_ptr = &mon_list[m_idx];
    r_ptr = &r_info[m_ptr->r_idx];

    player_face_grid_before_attack(y, x);

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

    // The pacifist gameplay option prompts before attacking.
    if (pacifist_attack_warning && !p_ptr->truce && m_ptr->ml)
    {
        if (!get_check_near(m_ptr->fy, m_ptr->fx,
                "Are you sure you wish to attack? "))
            abort_attack = true;
    }

    // Warning about breaking the truce
    if ((p_ptr->truce)
        && !get_check_near(m_ptr->fy, m_ptr->fx,
            "Are you sure you wish to attack? "))
    {
        abort_attack = true;
    }

    // Warn about fighting with fists
    if ((o_ptr->weight == 0)
        && !get_check_near(m_ptr->fy, m_ptr->fx,
            "Are you sure you wish to attack with no weapon? "))
    {
        abort_attack = true;
    }

    // Warn about fighting with shovel
    if ((o_ptr->tval == TV_DIGGING) && (o_ptr->sval == SV_SHOVEL)
        && !get_check_near(m_ptr->fy, m_ptr->fx,
            "Are you sure you wish to attack with your shovel? "))
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

            bool skip_take_hit = false;

            if (singing(SNG_SLAYING) && crit_bonus_dice > 0)
            {
                int kill_threshold = ability_bonus(S_SNG, SNG_SLAYING);
                if (m_ptr->hp <= kill_threshold)
                {
                    if ((m_ptr->r_idx == R_IDX_MORGOTH)
                        && !p_ptr->morgoth_second_wind)
                    {
                        int song_dam = m_ptr->hp;

                        msg_format("Your song soars as %s reels at the edge "
                                   "of doom.", m_name);

                        /* Sort out combat rolls window */
                        total_dice = 0;
                        mds = 0;
                        dam = song_dam;
                        net_dam = song_dam;
                        prt = 0;
                        prt_percent = 0;

                        if (morgoth_enter_final_stage(m_idx))
                        {
                            skip_take_hit = true;
                        }
                    }
                    else
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

                    if (skip_take_hit)
                    {
                        p_ptr->vengeance = 0;
                    }
                }
            }

            update_combat_rolls2(total_dice, mds, dam, armor_dice, armor_sides,
                prt, prt_percent, damage_type, true);

            // Stamp the roll before mon_take_hit() emits a death message, so
            // combined history reads: hit message, roll, slain/destroyed.
            if (!fatal_blow && !skip_take_hit)
            {
                // damage, check for death
                fatal_blow = mon_take_hit(m_idx, net_dam, NULL, -1);
                p_ptr->vengeance = 0;
            }

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

            if (skip_take_hit)
            {
                break;
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
                        if (!monster_race_is_vala(m_ptr->r_idx)
                            && !(r_ptr->flags3 & (RF3_NO_CONF)))
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
            update_combat_rolls_no_damage();

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
    if (!player_active_weapon_is_melee()
        || !p_ptr->active_ability[S_MEL][MEL_WHIRLWIND_ATTACK])
    {
        return (false);
    }

    return (true);
}

bool can_impale()
{
    bool has_impale_skill = player_active_weapon_is_melee()
        && p_ptr->active_ability[S_MEL][MEL_IMPALE];

    object_type* o_ptr = &inventory[INVEN_WIELD];

    return has_impale_skill && weapon_is_impale_eligible(o_ptr);
}

void py_attack(int y, int x, int attack_type)
{
    int dir, dir0, yy, xx;

    dir = dir_from_delta(y - p_ptr->py, x - p_ptr->px);
    player_set_visual_facing_dir(dir);

    if (!player_active_weapon_is_melee())
    {
        if (attack_type == ATT_MAIN)
            (void)do_cmd_fire_at_adjacent(y, x);
        return;
    }

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

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

    bool flanking = p_ptr->active_ability[S_EVN][EVN_FLANKING]
        && wearing_only_light_armour();
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
