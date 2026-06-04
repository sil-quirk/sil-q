#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "pane.h"
#include "supplies.h"
#include "item_set.h"
#include "player/player-upkeep-internal.h"

/*
 * Determine the radius of possibly flickering lights
 */
int light_up_to(int base_radius, object_type* o_ptr)
{
    int radius = base_radius;
    u32b f1, f2, f3, f4;

    /* Extract the flags */
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    // Some lights flicker (DARKNESS and UNLIGHT items cause flickering)
    if ((f2 & (TR2_DARKNESS)) || (f4 & (TR4_UNLIGHT)))
    {
        while ((radius > -2) && one_in_(3))
        {
            radius--;
        }
    }
    else if (player_light_fuel(o_ptr) <= player_light_sputter_threshold(o_ptr))
    {
        while ((radius > 0) && one_in_(3))
        {
            radius--;
        }
    }

    return (radius);
}

/*
 *  Determines how much an enemy in a given location should make the sword glow
 */
int hate_level(int y, int x, int multiplier)
{
    int dist;

    // check distance of monster from player (by noise)
    dist = flow_dist(FLOW_MONSTER_NOISE, y, x);

    // Avoid a division by zero
    if (dist == 0)
        dist = 1;

    // determine the danger level
    return ((50 * multiplier) / dist);
}

/*
 * Determine whether a melee weapon is glowing in response to nearby enemies
 */
bool weapon_glows(const object_type* o_ptr)
{
    int total_hate = 0;
    int i;
    int iy = o_ptr->iy; // weapon location
    int ix = o_ptr->ix;
    int py = p_ptr->py; // player location
    int px = p_ptr->px;
    int y, x; // generic location
    u32b f1, f2, f3, f4;
    bool viewable = false;

    bool glows = false;

    if (!character_dungeon)
        return (false);

    // Must be a melee weapon
    if (wield_slot(o_ptr) != INVEN_WIELD)
        return (false);

    // use the player's position where needed
    if ((iy == 0) && (ix == 0))
    {
        iy = py;
        ix = px;
    }

    // out of LOS objects don't glow (or it can't be seen)
    if (cave_info[iy - 1][ix - 1] & (CAVE_VIEW))
        viewable = true;
    if (cave_info[iy - 1][ix] & (CAVE_VIEW))
        viewable = true;
    if (cave_info[iy - 1][ix + 1] & (CAVE_VIEW))
        viewable = true;
    if (cave_info[iy][ix - 1] & (CAVE_VIEW))
        viewable = true;
    if (cave_info[iy][ix] & (CAVE_VIEW))
        viewable = true;
    if (cave_info[iy][ix + 1] & (CAVE_VIEW))
        viewable = true;
    if (cave_info[iy + 1][ix - 1] & (CAVE_VIEW))
        viewable = true;
    if (cave_info[iy + 1][ix] & (CAVE_VIEW))
        viewable = true;
    if (cave_info[iy + 1][ix + 1] & (CAVE_VIEW))
        viewable = true;

    if (!viewable)
        return (false);

    // create a 'flow' around the object
    update_flow(iy, ix, FLOW_MONSTER_NOISE);

    /* Extract the flags */
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    /* Add up the total of creatures vulnerable to the weapon's slays */
    for (i = 1; i < mon_max; i++)
    {
        bool target = false;
        int multiplier = 1;
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        // Determine if a slay is applicable
        if ((f1 & (TR1_SLAY_WOLF)) && (r_ptr->flags3 & (RF3_WOLF)))
            target = true;
        if ((f1 & (TR1_SLAY_SPIDER)) && (r_ptr->flags3 & (RF3_SPIDER)))
            target = true;
        if ((f1 & (TR1_SLAY_UNDEAD)) && (r_ptr->flags3 & (RF3_UNDEAD)))
            target = true;
        if ((f1 & (TR1_SLAY_RAUKO)) && (r_ptr->flags3 & (RF3_RAUKO)))
            target = true;
        if ((f1 & (TR1_SLAY_ORC)) && (r_ptr->flags3 & (RF3_ORC)))
            target = true;
        if ((f1 & (TR1_SLAY_TROLL)) && (r_ptr->flags3 & (RF3_TROLL)))
            target = true;
        if ((f1 & (TR1_SLAY_DRAGON)) && (r_ptr->flags3 & (RF3_DRAGON)))
            target = true;
        if ((f4 & (TR4_SLAY_SERPENT)) && (r_ptr->flags3 & (RF3_SERPENT)))
            target = true;
        if ((f4 & (TR4_SLAY_VAMPIRE)) && (r_ptr->flags3 & (RF3_VAMPIRE)))
            target = true;
        if ((f4 & (TR4_SLAY_HORROR)) && (r_ptr->flags3 & (RF3_HORROR)))
            target = true;
        if ((f4 & (TR4_SLAY_CAT)) && (r_ptr->flags3 & (RF3_CAT)))
            target = true;
        if ((f4 & (TR4_SLAY_GIANT)) && (r_ptr->flags3 & (RF3_GIANT)))
            target = true;
        // No glow for Morgoth's weapons that slay men and elves

        // skip inapplicable monsters
        if (!target)
            continue;

        // increase the effect for uniques
        if (r_ptr->flags1 & (RF1_UNIQUE))
            multiplier *= 2;

        // increase the effect for individually occuring creatures
        if (!(r_ptr->flags1 & (RF1_FRIENDS)) && !(r_ptr->flags1 & (RF1_FRIEND))
            && !(r_ptr->flags1 & (RF1_ESCORTS))
            && !(r_ptr->flags1 & (RF1_ESCORT)))
            multiplier *= 2;

        // add up the 'hate'
        total_hate += hate_level(m_ptr->fy, m_ptr->fx, multiplier);
    }

    /* Add a similar effect for very nearby webs for spider slaying wearpons */
    if (f1 & (TR1_SLAY_SPIDER))
    {
        for (y = (iy - 2); y <= (iy + 2); y++)
        {
            for (x = (ix - 2); x <= (ix + 2); x++)
            {
                if (in_bounds(y, x))
                {
                    // skip inapplicable squares
                    if (cave_feat[y][x] != FEAT_TRAP_WEB)
                        continue;

                    // add up the 'hate'
                    total_hate += hate_level(y, x, 1);
                }
            }
        }
    }

    if (total_hate >= 15)
        glows = true;

    return (glows);
}

bool player_has_equipped_flag3(u32b flag3)
{
    for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];
        if (!o_ptr->k_idx) continue;

        u32b f1, f2, f3;
        object_flags(o_ptr, &f1, &f2, &f3);
        if (f3 & flag3) return true;
    }

    return false;
}

bool player_has_inventory_flag3(u32b flag3)
{
    /* Check entire inventory (pack + equipment) */
    for (int i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];
        if (!o_ptr->k_idx) continue;

        u32b f1, f2, f3;
        object_flags(o_ptr, &f1, &f2, &f3);
        if (f3 & flag3) return true;
    }

    return false;
}

int oath_special_ability_from_oath_num(int oath_num)
{
    switch (oath_num)
    {
        case OATH_MERCY: return SPC_OATH_MERCY;
        case OATH_SILENCE: return SPC_OATH_SILENCE;
        case OATH_IRON: return SPC_OATH_IRON;
        case OATH_SMITH: return SPC_OATH_SMITH;
        case OATH_VALOROUS: return SPC_OATH_VALOROUS;
        case OATH_LIGHT: return SPC_OATH_LIGHT;
        default: return -1;
    }
}

static bool player_has_active_oath(void)
{
    if (p_ptr->oath_type <= 0) return false;
    if (oath_invalid(p_ptr->oath_type)) return false;

    int special_ability = oath_special_ability_from_oath_num(p_ptr->oath_type);
    if (special_ability < 0) return false;

    return p_ptr->active_ability[S_SPC][special_ability];
}

/*
 * Extract and set the current "lite radius"
 */
void calc_torch(void)
{
    int i;
    object_type* o_ptr;
    u32b f1, f2, f3, f4;
    int old_light;
    bool has_oath_boost = false;
    bool has_active_oath = false;
    int oath_reward_mult = 1;

    /* Store old value */
    old_light = p_ptr->cur_light;

    bool has_oath_negate = player_has_inventory_flag3(TR3_OATH_NEGATE);
    has_oath_boost = player_has_equipped_flag3(TR3_OATH_BOOST);
    has_active_oath = player_has_active_oath();
    oath_reward_mult = has_oath_negate ? 0 : ((has_oath_boost && has_active_oath) ? 2 : 1);

    /* Assume no light */
    p_ptr->cur_light = 0;

    /* Loop through all wielded items */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Skip empty slots */
        if (!o_ptr->k_idx)
            continue;

        /* Extract the flags */
        object_flags4(o_ptr, &f1, &f2, &f3, &f4);

        /* Skip quiver 1 entirely - it provides no bonuses */
        if (i == INVEN_QUIVER1)
            continue;

        /* Skip quiver 2 unless item is an arrow or throwing item */
        if (i == INVEN_QUIVER2)
        {
            bool is_throwing = player_can_treat_as_throwing_flags(o_ptr, f3);
            bool is_arrow = (o_ptr->tval == TV_ARROW);
            if (!is_throwing && !is_arrow)
                continue;
        }

        /* Does this item glow? */
        if ((f2 & TR2_LIGHT) && (i != INVEN_LITE))
            p_ptr->cur_light++;

        /* Does this item create darkness? */
        if ((f2 & TR2_DARKNESS) && (i != INVEN_LITE))
            p_ptr->cur_light--;

        /* Does this item create unlight? (dims light without power bonus) */
        if ((f4 & TR4_UNLIGHT) && (i != INVEN_LITE))
            p_ptr->cur_light--;

        /* Examine actual light */
        if (o_ptr->tval == TV_LIGHT)
        {
            bool extinguished = false;

            /* Some items provide permanent, bright, light */
            if (o_ptr->sval == SV_LIGHT_LESSER_JEWEL)
                p_ptr->cur_light += RADIUS_LESSER_JEWEL;
            else if (o_ptr->sval == SV_LIGHT_FEANORIAN)
                p_ptr->cur_light += RADIUS_FEANORIAN;
            else if (o_ptr->sval == SV_LIGHT_SILMARIL)
                p_ptr->cur_light += RADIUS_SILMARIL;

            /* Torches (with fuel) provide some light */
            else if ((o_ptr->sval == SV_LIGHT_TORCH) && player_light_has_fuel(o_ptr))
            {
                p_ptr->cur_light += light_up_to(RADIUS_TORCH, o_ptr);
            }

            /* Broken lanterns can still hold oil, but give no light until repaired. */
            else if ((o_ptr->sval == SV_LIGHT_LANTERN)
                && (object_ego_prefix(o_ptr) == EGO_BROKEN_BRASS_LANTERN))
            {
                extinguished = true;
            }

            /* Lanterns (with fuel) provide more light */
            else if ((o_ptr->sval == SV_LIGHT_LANTERN) && player_light_has_fuel(o_ptr))
            {
                p_ptr->cur_light += light_up_to(RADIUS_LANTERN, o_ptr);
            }

            /* Mallorn torches (with fuel) provide even more light */
            else if ((o_ptr->sval == SV_LIGHT_MALLORN) && player_light_has_fuel(o_ptr))
            {
                p_ptr->cur_light += light_up_to(RADIUS_MALLORN, o_ptr);
            }

            else
            {
                extinguished = true;
            }

            if (!extinguished && (f2 & TR2_LIGHT))
            {
                p_ptr->cur_light++;
            }
        }
    }

    // increase radius when the player's weapon glows
    if (weapon_glows(&inventory[INVEN_WIELD]))
        p_ptr->cur_light++;
    if (weapon_glows(&inventory[INVEN_ARM]))
        p_ptr->cur_light++;

    /* Player is darkened */
    if (p_ptr->darkened && (p_ptr->cur_light > 0))
        p_ptr->cur_light--;

    // Smithing brightens the room a bit
    if (p_ptr->smithing)
        p_ptr->cur_light += 2;

    // Song of the trees
    if (singing(SNG_TREES))
    {
        p_ptr->cur_light += ability_bonus(S_SNG, SNG_TREES);
    }

    /* Oath of Light reward */
    if (p_ptr->active_ability[S_SPC][SPC_OATH_LIGHT] && !oath_invalid(OATH_LIGHT))
    {
        p_ptr->cur_light += 1 * oath_reward_mult;
    }
    /* Ring of Barahir: +1 light when no oath is active */
    else if (has_oath_boost && !has_active_oath)
    {
        p_ptr->cur_light += 1;
    }

    /* Update the visuals */
    p_ptr->update |= (PU_UPDATE_VIEW);
    p_ptr->update |= (PU_MONSTERS);

    /* Apply light radius curses/blessings */
    {
        int r = curse_flag_delta_cur(CUR_LIGHTR);

        /* radius penalty/bonus: +/-1 per stack, never below zero */
        if (r != 0)
            p_ptr->cur_light = MAX(0, p_ptr->cur_light - r);
    }

    /* Notice changes in the "lite radius" */
    if (old_light != p_ptr->cur_light)
    {
        /* Update the visuals */
        p_ptr->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
    }

    p_ptr->redraw |= (PR_LIGHT);
}
