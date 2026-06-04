/* File: player/player-song-effects.c */

#include "angband.h"
#include "externs.h"
#include "player/player-song-internal.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"
#include "supplies.h"
#include <math.h>

void sing_song_of_freedom(int score)
{
    int y, x;
    int base_difficulty, difficulty;
    int result;
    int new_feat;
    object_type* o_ptr;
    bool closed_chasm = false;

    // set the base difficulty
    if (p_ptr->depth > 0)
    {
        base_difficulty = p_ptr->depth / 2;
    }
    else
    {
        base_difficulty = 10;
    }

    /* Scan the map */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            // get the object present (if any)
            o_ptr = &o_list[cave_o_idx[y][x]];

            /* Locked/trapped chest */
            if (o_ptr->tval == TV_CHEST)
            {
                /* Disarm/Unlock traps */
                if (o_ptr->pval > 0)
                {
                    difficulty = base_difficulty + 5
                        + flow_dist(FLOW_PLAYER_NOISE, y, x);
                    if (skill_check(PLAYER, score, difficulty, NULL) > 0)
                    {
                        /* Disarm or Unlock */
                        o_ptr->pval = (0 - o_ptr->pval);

                        /* Identify */
                        object_known(o_ptr);
                    }
                }
            }

            // Chasm
            else if (cave_feat[y][x] == FEAT_CHASM)
            {
                closed_chasm |= close_chasm(
                    y, x, score - flow_dist(FLOW_PLAYER_NOISE, y, x) - 5);
            }

            /* Invisible trap */
            else if (cave_trap_bold(y, x) && (cave_info[y][x] & (CAVE_HIDDEN)))
            {
                difficulty
                    = base_difficulty + 5 + flow_dist(FLOW_PLAYER_NOISE, y, x);
                if (skill_check(PLAYER, score, difficulty, NULL) > 0)
                {
                    /* Remove the trap */
                    cave_feat[y][x] = FEAT_FLOOR;
                }
            }

            /* Visible trap */
            else if (cave_trap_bold(y, x))
            {
                difficulty
                    = base_difficulty + 5 + flow_dist(FLOW_PLAYER_NOISE, y, x);
                if (skill_check(PLAYER, score, difficulty, NULL) > 0)
                {
                    /* Remove the trap */
                    cave_feat[y][x] = FEAT_FLOOR;

                    if (cave_info[y][x] & (CAVE_SEEN))
                    {
                        lite_spot(y, x);
                    }
                }
            }

            /* Secret door */
            else if (cave_feat[y][x] == FEAT_SECRET)
            {
                difficulty
                    = base_difficulty + 0 + flow_dist(FLOW_PLAYER_NOISE, y, x);
                if (skill_check(PLAYER, score, difficulty, NULL) > 0)
                {
                    /* Pick a door */
                    place_closed_door(y, x);

                    if (cave_info[y][x] & (CAVE_SEEN))
                    {
                        /* Message */
                        msg_print("You have found a secret door.");

                        /* Disturb */
                        disturb(0, 0);
                    }
                }
            }

            /* Stuck door */
            else if ((cave_feat[y][x] >= FEAT_DOOR_HEAD + 0x08)
                && (cave_feat[y][x] <= FEAT_DOOR_TAIL))
            {
                difficulty
                    = base_difficulty + 0 + flow_dist(FLOW_PLAYER_NOISE, y, x);
                result = skill_check(PLAYER, score, difficulty, NULL);
                if (result > 0)
                {
                    new_feat = cave_feat[y][x] - result;

                    if (new_feat <= FEAT_DOOR_HEAD + 0x08)
                        new_feat = FEAT_DOOR_HEAD;

                    cave_feat[y][x] = new_feat;
                }
            }

            /* Locked door */
            else if ((cave_feat[y][x] >= FEAT_DOOR_HEAD + 0x01)
                && (cave_feat[y][x] <= FEAT_DOOR_HEAD + 0x07))
            {
                difficulty
                    = base_difficulty + 0 + flow_dist(FLOW_PLAYER_NOISE, y, x);
                result = skill_check(PLAYER, score, difficulty, NULL);
                if (result > 0)
                {
                    new_feat = cave_feat[y][x] - result;

                    if (new_feat < FEAT_DOOR_HEAD)
                        new_feat = FEAT_DOOR_HEAD;

                    cave_feat[y][x] = new_feat;
                }
            }

            /* Rubble */
            else if (cave_feat[y][x] == FEAT_RUBBLE)
            {
                int noise_dist = 100;
                int d, dir;

                // check adjacent squares for valid noise distances, since
                // rubble is impervious to sound
                for (d = 0; d < 8; d++)
                {
                    dir = cycle[d];
                    noise_dist = MIN(noise_dist,
                        flow_dist(
                            FLOW_PLAYER_NOISE, y + ddy[dir], x + ddx[dir]));
                }
                noise_dist++;

                difficulty = base_difficulty + 5 + noise_dist;
                result = skill_check(PLAYER, score, difficulty, NULL);
                if (result > 0)
                {
                    /* Disperse the rubble */
                    cave_set_feat(y, x, FEAT_FLOOR);

                    /* Update the flow code */
                    p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
                }
            }
        }
    }

    // then, if any chasms were marked to be closed, do the closing
    if (closed_chasm)
    {
        for (y = 0; y < p_ptr->cur_map_hgt; y++)
        {
            for (x = 0; x < p_ptr->cur_map_wid; x++)
            {
                if ((cave_feat[y][x] == FEAT_CHASM)
                    && (cave_info[y][x] & (CAVE_TEMP)))
                {
                    // remove the temporary marking
                    cave_info[y][x] &= ~(CAVE_TEMP);

                    // close the chasm
                    cave_set_feat(y, x, FEAT_FLOOR);

                    // update the visuals
                    p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
                }
            }
        }
    }
}

/*
 *  Do the effects of (the monster song) Song of Binding
 */

bool known_to_delvings(int y, int x)
{
    if (!in_bounds(y, x))
        return false;
    return ((cave_info[y][x] & CAVE_MARK) || (cave_info[y][x] & CAVE_SEEN));
}

void sing_song_of_challenge(int score)
{
    int i;

    /* Scan all other monsters */
    for (i = mon_max - 1; i >= 1; i--)
    {
        int resistance;
        int result;

        /* Access the monster */
        monster_type* m_ptr = &mon_list[i];

        /* Ignore dead monsters */
        if (!m_ptr->r_idx)
            continue;

        resistance = monster_skill(m_ptr, S_WIL);

        // Adjust to work best against lower-will monsters.
        resistance = (resistance * resistance) / 10;

        // adjust difficulty by the distance to the monster
        result = skill_check(PLAYER, score,
            resistance + flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx),
            m_ptr);

        /* If successful, alert the monster and make it more aggressive */
        if (result > 0)
        {
            set_alertness(m_ptr, m_ptr->alertness + result);
            // boost morale and check for the monster turning aggressive
            m_ptr->tmp_morale = MAX(m_ptr->tmp_morale, 30);
            calc_morale(m_ptr);
            calc_stance(m_ptr);
        }
    }
}

void sing_song_of_delvings(int score)
{
    int y, x, yy, xx;
    int min_x, max_x, min_y, max_y, y_range, x_range;

    int px = p_ptr->px;
    int py = p_ptr->py;

    int range = score + 8;

    min_y = MAX(1, py - range);
    max_y = MIN(MAX_DUNGEON_HGT, py + range + 1);
    min_x = MAX(1, px - range);
    max_x = MIN(MAX_DUNGEON_WID, px + range + 1);
    y_range = max_y - min_y;
    x_range = max_x - min_x;

    char* delvings;
    delvings = mem_alloc_array(y_range * x_range * 4, char);

    for (y = min_y; y < max_y; ++y)
    {
        for (x = min_x; x < max_x; ++x)
        {
            bool neighbour_known = false;
            int distance_from_player = (distance(py, px, y, x));
            int adjusted_score = score - distance_from_player;

            for (yy = y - 1; yy <= y + 1; ++yy)
            {
                for (xx = x - 1; xx <= x + 1; ++xx)
                {
                    int chance = damroll(1, 6);
                    if (known_to_delvings(yy, xx) && chance < adjusted_score)
                        neighbour_known = true;
                }
            }

            if (neighbour_known)
            {
                int dy = y - min_y;
                int dx = x - min_x;

                delvings[(dy * x_range) + dx] = true;
            }
        }
    }

    for (y = min_y; y < max_y; ++y)
    {
        for (x = min_x; x < max_x; ++x)
        {
            int dy = y - min_y;
            int dx = x - min_x;

            if (delvings[(dy * x_range) + dx] == true)
            {
                map_feature(y, x);
                if (cave_feat[y][x] == FEAT_SECRET && known_to_delvings(y, x))
                {
                    place_closed_door(y, x);
                }
            }
            if (cave_stair_bold(y, x) || cave_forge_bold(y, x)
                || cave_trap_bold(y, x))
            {
                // Special case for stairs and forges - if we know a square
                // within a distance of 5 along an axis, we spot them.
                int i, j;
                int start_y = MAX(min_y, y - 5);
                int end_y = MIN(max_y, y + 5);
                int start_x = MAX(min_x, x - 5);
                int end_x = MIN(max_x, x + 5);

                for (j = start_y; j < end_y; ++j)
                {
                    if (delvings[(j * x_range) + dx] == true)
                    {
                        if (cave_trap_bold(y, x))
                            reveal_trap(y, x);
                        else
                            map_feature(y, x);
                    }
                }

                for (i = start_x; i < end_x; ++i)
                {
                    if (delvings[(dy * x_range) + i] == true)
                    {
                        if (cave_trap_bold(y, x))
                            reveal_trap(y, x);
                        else
                            map_feature(y, x);
                    }
                }
            }
        }
    }

    /* Redraw map */
    p_ptr->redraw |= (PR_MAP);

    /* Window stuff */
    p_ptr->window |= (PW_OVERHEAD);

    mem_free_null(delvings);
}

static bool object_is_monster_weapon(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_DIGGING:
        return true;
    default:
        return false;
    }
}

static bool object_is_monster_armour(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
        return true;
    default:
        return false;
    }
}

static void find_monster_equipment(
    monster_type* m_ptr, object_type** weapon, object_type** armour)
{
    s16b this_o_idx;

    *weapon = NULL;
    *armour = NULL;

    for (this_o_idx = m_ptr->hold_o_idx; this_o_idx;
         this_o_idx = o_list[this_o_idx].next_o_idx)
    {
        object_type* o_ptr = &o_list[this_o_idx];

        if (!*weapon && object_is_monster_weapon(o_ptr))
            *weapon = o_ptr;

        if (!*armour && object_is_monster_armour(o_ptr))
            *armour = o_ptr;

        if (*weapon && *armour)
            break;
    }
}

static bool object_is_indestructible(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (artefact_p(o_ptr))
        return true;

    if (o_ptr->discount == INSCRIP_INDESTRUCTIBLE)
        return true;

    return false;
}

static bool object_is_weapon(const object_type* o_ptr)
{
    if (!o_ptr)
        return false;
    return object_is_monster_weapon(o_ptr);
}

static bool object_is_armour(const object_type* o_ptr)
{
    if (!o_ptr)
        return false;
    return object_is_monster_armour(o_ptr);
}

static bool object_can_be_shattered(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (object_is_indestructible(o_ptr))
        return false;

    object_kind* k_ptr = &k_info[o_ptr->k_idx];

    if (k_ptr->flags3 & TR3_IGNORE_ALL)
        return false;

    return true;
}

static int base_weapon_ds(const object_type* o_ptr)
{
    int base = k_info[o_ptr->k_idx].ds;

    if (o_ptr->name1)
    {
        artefact_type* a_ptr = &a_info[o_ptr->name1];
        if (a_ptr->ds > 0)
            base = a_ptr->ds;
    }

    return base;
}

static int base_armour_ps(const object_type* o_ptr)
{
    int base = k_info[o_ptr->k_idx].ps;

    if (o_ptr->name1)
    {
        artefact_type* a_ptr = &a_info[o_ptr->name1];
        if (a_ptr->ps > 0)
            base = a_ptr->ps;
    }

    return base;
}

static bool shatter_weapon_object(object_type* o_ptr, int amount)
{
    if (!object_is_weapon(o_ptr))
        return false;

    if (!object_can_be_shattered(o_ptr))
        return false;

    int base = base_weapon_ds(o_ptr);

    if (o_ptr->ds <= base)
        return false;

    int new_ds = MAX(base, o_ptr->ds - amount);

    if (new_ds < o_ptr->ds)
    {
        o_ptr->ds = (byte)new_ds;
        return true;
    }

    return false;
}

static bool shatter_armour_object(object_type* o_ptr, int amount)
{
    if (!object_is_armour(o_ptr))
        return false;

    if (!object_can_be_shattered(o_ptr))
        return false;

    int base = base_armour_ps(o_ptr);

    if (o_ptr->ps <= base)
        return false;

    int new_ps = MAX(base, o_ptr->ps - amount);

    if (new_ps < o_ptr->ps)
    {
        o_ptr->ps = (byte)new_ps;
        return true;
    }

    return false;
}

static void shatter_floor_items(int score);

void sing_song_of_elbereth(int score)
{
    int i;

    /* Scan all other monsters */
    for (i = mon_max - 1; i >= 1; i--)
    {
        int resistance;
        int result;

        /* Access the monster */
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        /* Ignore dead monsters */
        if (!m_ptr->r_idx)
            continue;

        resistance = monster_skill(m_ptr, S_WIL);

        // only intelligent monsters are affected
        if (!(r_ptr->flags2 & (RF2_SMART)))
            resistance += 100;

        // Morgoth is not affected
        if (m_ptr->r_idx == R_IDX_MORGOTH)
            resistance += 100;

        // adjust difficulty by the distance to the monster
        result = skill_check(PLAYER, score,
            resistance + flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx),
            m_ptr);

        /* If successful, cause fear in the monster */
        if (result > 0)
        {
            /* Decrease temporary morale */
            m_ptr->tmp_morale -= result * 10;
        }
    }
}

void sing_song_of_trees(int score)
{
    int py = p_ptr->py;
    int px = p_ptr->px;
    int rad = ability_bonus(S_SNG, SNG_TREES); // Stepped light radius bonus
    int dd = 1; // Always 1 die
    int ds = score;            // Not used for GF_LIGHT damage; kept for debugging
    int dif = score;           // Song score for GF_LIGHT resistance checks
    
    log_debug("sing_song_of_trees: score=%d rad=%d dd=%d ds=%d", score, rad, dd, ds);
    
    /* Song of Trees damages/stuns light-sensitive monsters without visual flash */
    /* Uses PROJECT_KILL to affect monsters, but NOT PROJECT_GRID (no visual light squares effect) */
    /* PROJECT_HIDE prevents the projectile animation */
    /* Light radius increase is handled separately in player-light.c calc_torch() */
    /* Shows damage messages to provide feedback when monsters are affected */
    /* IMPORTANT: Use uniform=true so dd doesn't decay with distance (damage is based on light_level at monster's position) */
    u32b flg = PROJECT_BOOM | PROJECT_KILL | PROJECT_PASS | PROJECT_HIDE;
    
    (void)project(-1, rad, py, px, py, px, dd, ds, dif, GF_LIGHT, flg, 0, true);
}

void sing_song_of_lorien(int score)
{
    int i;

    /* Scan all other monsters */
    for (i = mon_max - 1; i >= 1; i--)
    {
        int resistance;
        int result;

        /* Access the monster */
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];
        monster_lore* l_ptr = &l_list[m_ptr->r_idx];

        /* Ignore dead monsters */
        if (!m_ptr->r_idx)
            continue;

        resistance = monster_skill(m_ptr, S_WIL);

        // Deal with sleep resistance
        if (r_ptr->flags3 & (RF3_NO_SLEEP))
        {
            resistance += 100;
            if (m_ptr->ml)
                l_ptr->flags3 |= (RF3_NO_SLEEP);
        }

        // adjust difficulty by the distance to the monster
        if (c_info[p_ptr->pcharacter].flags_u & UNQ_SNG_LUT) result = skill_check(PLAYER, (3*score)/2,
            resistance + 5 + flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx),
            m_ptr);
        else result = skill_check(PLAYER, score,
            resistance + 5 + flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx),
            m_ptr);

        /* If successful, (partially) put the monster to sleep */
        if (result > 0)
        {
            set_alertness(m_ptr, m_ptr->alertness - result);
        }
    }
}

/*
 * Apply shattering effect to monsters in an arc (like Horn of Blasting)
 * This affects monsters within a 90-degree arc, radius 3
 */
void shatter_in_arc(int dir, int score)
{
    int i, j;
    int direction;
    extern const byte cycle[];
    extern const byte chome[];

    /* Handle special directions */
    if (dir == DIRECTION_UP || dir == DIRECTION_DOWN || dir == 5)
        return;

    direction = chome[dir];

    /* Scan arc: 3 forward, in 3 directions (left, center, right) */
    for (i = -1; i < 2; ++i)
    {
        for (j = 1; j <= 3; ++j)
        {
            int arc_dir = cycle[direction + i];
            int y = p_ptr->py + j * ddy[arc_dir];
            int x = p_ptr->px + j * ddx[arc_dir];

            /* Check bounds */
            if (!in_bounds_fully(y, x))
                continue;

            /* Check for monster */
            if (cave_m_idx[y][x] <= 0)
                continue;

            monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
            monster_race* r_ptr = &r_info[m_ptr->r_idx];
            object_type* weapon;
            object_type* armour;
            int resistance;
            int result;
            bool weapon_possible = false;
            bool armour_possible = false;
            int weapon_blow = -1;
            int best_ds = 0;

            /* Ignore dead monsters */
            if (!m_ptr->r_idx)
                continue;

            bool has_weapon_flag = (r_ptr->flags3 & RF3_HAS_WEAPON) != 0;
            bool has_armour_flag = (r_ptr->flags3 & RF3_HAS_ARMOUR) != 0;

            if (!has_weapon_flag && !has_armour_flag)
                continue;

            /* Identify items carried by the monster */
            find_monster_equipment(m_ptr, &weapon, &armour);

            /* Determine resistance (no distance scaling for arc effect) */
            resistance = monster_skill(m_ptr, S_WIL);

            result = skill_check(PLAYER, score, resistance, m_ptr);

            if (result <= 0)
                continue;

            /* Check for weapon possibility */
            if (has_weapon_flag)
            {
                for (int b = 0; b < MONSTER_BLOW_MAX; b++)
                {
                    if (!r_ptr->blow[b].method)
                        break;

                    int ds = r_ptr->blow[b].ds;
                    if (ds <= 1)
                        continue;

                    int max_reduction = ds - 1;
                    int current = m_ptr->blow_ds_reduction[b];

                    if (current >= max_reduction)
                        continue;

                    if (ds > best_ds)
                    {
                        best_ds = ds;
                        weapon_blow = b;
                    }
                }

                weapon_possible = (weapon_blow != -1);
            }

            /* Check for armour possibility */
            if (has_armour_flag && r_ptr->ps > 0)
            {
                if (m_ptr->armor_ps_reduction < r_ptr->ps)
                    armour_possible = true;
            }

            if (!weapon_possible && !armour_possible)
                continue;

            /* 50/50 chance to target weapon or armour */
            bool target_weapon = weapon_possible
                && (!armour_possible || one_in_(2));

            if (target_weapon && weapon_possible)
            {
                /* Probability to weaken: score/3 percent */
                int weaken_chance = score / 3;
                
                if (percent_chance(weaken_chance))
                {
                    /* Reduce by exactly 1 */
                    m_ptr->blow_ds_reduction[weapon_blow] += 1;

                    if (weapon)
                        shatter_weapon_object(weapon, 1);

                    if (m_ptr->ml)
                    {
                        char m_name[80];
                        monster_desc(m_name, sizeof(m_name), m_ptr, 0);
                        msg_format("The blast splinters %s's weapon.", m_name);
                    }
                }
            }
            else if (!target_weapon && armour_possible)
            {
                /* Probability to weaken: score/3 percent */
                int weaken_chance = score / 3;
                
                if (percent_chance(weaken_chance))
                {
                    /* Reduce by exactly 1 */
                    m_ptr->armor_ps_reduction += 1;

                    if (armour)
                        shatter_armour_object(armour, 1);

                    if (m_ptr->ml)
                    {
                        char m_name[80];
                        monster_desc(m_name, sizeof(m_name), m_ptr, 0);
                        msg_format("The blast warps %s's armour.", m_name);
                    }
                }
            }
        }
    }
}

void sing_song_of_shattering(int score)
{
    int i;
    int monsters_checked = 0;
    int monsters_with_flags = 0;
    int skill_check_passed = 0;

    log_debug("Song of Shattering: starting with score=%d", score);

    /* Scan all other monsters */
    for (i = mon_max - 1; i >= 1; i--)
    {
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];
        object_type* weapon;
        object_type* armour;
        int resistance;
        int result;
        bool weapon_possible = false;
        bool armour_possible = false;
        int weapon_blow = -1;
        int best_ds = 0;

        /* Ignore dead monsters */
        if (!m_ptr->r_idx)
            continue;

        monsters_checked++;

        bool has_weapon_flag = (r_ptr->flags3 & RF3_HAS_WEAPON) != 0;
        bool has_armour_flag = (r_ptr->flags3 & RF3_HAS_ARMOUR) != 0;
        bool has_stone_body = (r_ptr->flags3 & RF3_STONE) != 0;

        if (!has_weapon_flag && !has_armour_flag && !has_stone_body)
            continue;

        monsters_with_flags++;
        log_debug("Song of Shattering: Monster %s has flags (weapon=%d, armour=%d, stone=%d)", 
                  r_name + r_ptr->name, has_weapon_flag, has_armour_flag,
                  has_stone_body);

        /* Identify items carried by the monster (for secondary effects) */
        find_monster_equipment(m_ptr, &weapon, &armour);

        /* Determine resistance, scaling with distance */
        resistance = monster_skill(m_ptr, S_WIL);
        resistance += flow_dist(FLOW_PLAYER_NOISE, m_ptr->fy, m_ptr->fx);

        result = skill_check(PLAYER, score, resistance, m_ptr);

        log_debug("Song of Shattering: skill_check result=%d (score=%d, resistance=%d)", 
                  result, score, resistance);

        if (result <= 0)
            continue;

        skill_check_passed++;

        /* Check for weapon possibility */
        if (has_weapon_flag)
        {
            for (int b = 0; b < MONSTER_BLOW_MAX; b++)
            {
                if (!r_ptr->blow[b].method)
                    break;

                int ds = r_ptr->blow[b].ds;
                if (ds <= 1)
                    continue;

                int max_reduction = ds - 1;
                int current = m_ptr->blow_ds_reduction[b];

                if (current >= max_reduction)
                    continue;

                if (ds > best_ds)
                {
                    best_ds = ds;
                    weapon_blow = b;
                }
            }

            weapon_possible = (weapon_blow != -1);
        }

        /* Check for armour possibility */
        if ((has_armour_flag || has_stone_body) && r_ptr->ps > 0)
        {
            if (m_ptr->armor_ps_reduction < r_ptr->ps)
                armour_possible = true;
        }

        if (!weapon_possible && !armour_possible)
        {
            log_debug("Song of Shattering: No valid targets for this monster");
            continue;
        }

        /* 50/50 chance to target weapon or armour (no fallthrough) */
        bool target_weapon = weapon_possible
            && (!armour_possible || one_in_(2));

        log_debug("Song of Shattering: target_weapon=%d, weapon_possible=%d, armour_possible=%d", 
                  target_weapon, weapon_possible, armour_possible);

        if (target_weapon && weapon_possible)
        {
            /* Probability to weaken: score/3 percent (6.7% at score 20) */
            int weaken_chance = score / 3;
            
            log_debug("Song of Shattering: Attempting weapon damage, weaken_chance=%d%%", weaken_chance);
            
            if (percent_chance(weaken_chance))
            {
                /* Reduce by exactly 1 */
                m_ptr->blow_ds_reduction[weapon_blow] += 1;

                if (weapon)
                    shatter_weapon_object(weapon, 1);

                if (m_ptr->ml)
                {
                    char m_name[80];
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0);
                    msg_format("Your song of sundering rends %s's weapon.", m_name);
                }
                
                log_debug("Song of Shattering: Weapon damage SUCCESS");
            }
            else
            {
                log_debug("Song of Shattering: Weapon damage FAILED probability check");
            }
        }
        else if (!target_weapon && armour_possible)
        {
            /* Probability to weaken: score/3 percent (6.7% at score 20) */
            int weaken_chance = score / 3;
            
            log_debug("Song of Shattering: Attempting armour damage, weaken_chance=%d%%", weaken_chance);
            
            if (percent_chance(weaken_chance))
            {
                /* Reduce by exactly 1 */
                m_ptr->armor_ps_reduction += 1;

                if (armour)
                    shatter_armour_object(armour, 1);

                if (m_ptr->ml)
                {
                    char m_name[80];
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0);
                    msg_format("Your song of sundering mars %s's armour.", m_name);
                }
                
                log_debug("Song of Shattering: Armour damage SUCCESS");
            }
            else
            {
                log_debug("Song of Shattering: Armour damage FAILED probability check");
            }
        }
    }

    log_debug("Song of Shattering: Summary - checked=%d, with_flags=%d, passed_skill_check=%d", 
              monsters_checked, monsters_with_flags, skill_check_passed);

    shatter_floor_items(score);
}

static void shatter_floor_items(int score)
{
    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (int x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (!in_bounds_fully(y, x))
                continue;

            if (!cave_o_idx[y][x])
                continue;

            int dist = flow_dist(FLOW_PLAYER_NOISE, y, x);
            if (dist >= FLOW_MAX_DIST)
                continue;

            int difficulty = 10 + dist;
            int result = skill_check(PLAYER, score, difficulty, NULL);
            if (result <= 0)
                continue;

            s16b this_o_idx = cave_o_idx[y][x];

            while (this_o_idx)
            {
                object_type* o_ptr = &o_list[this_o_idx];
                bool changed = false;

                /* Probability to weaken: score/3 percent (6.7% at score 20) */
                int weaken_chance = score / 3;

                if (percent_chance(weaken_chance))
                {
                    if (object_is_weapon(o_ptr))
                    {
                        changed = shatter_weapon_object(o_ptr, 1);
                    }
                    else if (object_is_armour(o_ptr))
                    {
                        changed = shatter_armour_object(o_ptr, 1);
                    }

                    if (changed)
                    {
                        if (panel_contains(y, x) && player_can_see_bold(y, x))
                        {
                            char o_name[80];
                            object_desc(o_name, sizeof(o_name), o_ptr, false, 0);
                            msg_format("%s answers your song with a bitter crack.", o_name);
                        }

                        lite_spot(y, x);
                    }
                }

                this_o_idx = o_ptr->next_o_idx;
            }
        }
    }
}
