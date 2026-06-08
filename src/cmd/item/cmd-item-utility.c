#include "angband.h"
#include "externs.h"
#include "item_set.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"

void do_cmd_exchange(void)
{
    int y, x, dir;

    monster_type* m_ptr;
    monster_race* r_ptr;
    char m_name[80];

    if (!p_ptr->active_ability[S_STL][STL_EXCHANGE_PLACES])
    {
        msg_print(
            "You need the ability 'exchange places' to use this command.");
        return;
    }

    /*
     * Let the SDL frontend show and handle one-click exchange targets while
     * the classic direction prompt is active. Keyboard direction input still
     * follows the normal get_rep_dir() path.
     */
    sdl_player_exchange_begin_direction_prompt();

    /* Get a direction (or abort) */
    if (!get_rep_dir(&dir))
    {
        sdl_player_exchange_cancel_direction_prompt();
        return;
    }

    sdl_player_exchange_cancel_direction_prompt();

    /* Get location */
    y = p_ptr->py + ddy[dir];
    x = p_ptr->px + ddx[dir];

    // deal with overburdened characters
    if (p_ptr->total_weight > weight_limit() * 3 / 2)
    {
        /* Abort */
        msg_print("You are too burdened to move.");

        return;
    }

    // Can't exchange from within pits
    if (cave_pit_bold(p_ptr->py, p_ptr->px))
    {
        /* Message */
        msg_print(
            "You would have to escape the pit before being able to exchange "
            "places.");

        return;
    }
    // Can't exchange from within webs
    else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB)
    {
        /* Message */
        msg_print(
            "You would have to escape the web before being able to exchange "
            "places.");

        return;
    }
    else if ((cave_m_idx[y][x] <= 0) || !(&mon_list[cave_m_idx[y][x]])->ml)
    {
        /* Message */
        msg_print("You cannot see a monster there to exchange places with.");

        return;
    }
    else if (cave_wall_bold(y, x))
    {
        /* Message */
        msg_print("You cannot enter the wall.");

        return;
    }
    else if (cave_any_closed_door_bold(y, x))
    {
        /* Message */
        msg_print("You cannot enter the closed door.");

        return;
    }
    else if (cave_feat[y][x] == FEAT_RUBBLE)
    {
        /* Message */
        msg_print("You cannot enter the rubble.");

        return;
    }
    else
    {
        m_ptr = &mon_list[cave_m_idx[y][x]];
        r_ptr = &r_info[m_ptr->r_idx];

        if ((r_ptr->flags1 & (RF1_NEVER_MOVE))
            || (r_ptr->flags1 & (RF1_HIDDEN_MOVE)))
        {
            monster_desc(m_name, sizeof(m_name), m_ptr, 0);

            /* Message */
            msg_format("You cannot get past %s.", m_name);

            return;
        }
    }

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Apply confusion */
    if (confuse_dir(&dir))
    {
        /* Get location */
        y = p_ptr->py + ddy[dir];
        x = p_ptr->px + ddx[dir];
    }

    // re-check for a visible monster (in case confusion changed the move)
    if ((cave_m_idx[y][x] <= 0) || !(&mon_list[cave_m_idx[y][x]])->ml)
    {
        /* Message */
        msg_print("You cannot see a monster there to exchange places with.");

        return;
    }

    else if (cave_wall_bold(y, x))
    {
        /* Message */
        msg_print("There is a wall in the way.");

        return;
    }
    else if (cave_any_closed_door_bold(y, x))
    {
        /* Message */
        msg_print("There is a door in the way.");

        return;
    }
    else if (cave_feat[y][x] == FEAT_RUBBLE)
    {
        /* Message */
        msg_print("There is a pile of rubble in the way.");

        return;
    }
    else if (cave_feat[y][x] == FEAT_CHASM)
    {
        /* Message */
        msg_print("You cannot exchange places over the chasm.");

        return;
    }

    // recalculate the monster info (in case confusion changed the move)
    m_ptr = &mon_list[cave_m_idx[y][x]];
    r_ptr = &r_info[m_ptr->r_idx];
    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

    /* Message */
    msg_format("You exchange places with %s.", m_name);

    // attack of opportunity
    if ((m_ptr->alertness >= ALERTNESS_ALERT) && !m_ptr->confused
        && !(r_ptr->flags2 & (RF2_MINDLESS)))
    {
        msg_print("It attacks you as you slip past.");
        make_attack_normal(m_ptr);
    }

    // Alert the monster
    make_alert(m_ptr);

    // Swap positions with the monster
    monster_swap(p_ptr->py, p_ptr->px, y, x);

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
}


void do_cmd_swap_quivers(void)
{
    object_type* q1_ptr = &inventory[INVEN_QUIVER1];
    object_type* q2_ptr = &inventory[INVEN_QUIVER2];
    object_type tmp;
    int i;

    if (!q1_ptr->k_idx && !q2_ptr->k_idx)
    {
        msg_print("Both quivers are empty.");
        return;
    }

    if ((q1_ptr->k_idx && cursed_p(q1_ptr))
        || (q2_ptr->k_idx && cursed_p(q2_ptr)))
    {
        msg_print("You cannot bear to rearrange your quiver.");
        return;
    }

    p_ptr->energy_use = 100;
    p_ptr->previous_action[0] = ACTION_MISC;

    object_copy(&tmp, q1_ptr);
    object_copy(q1_ptr, q2_ptr);
    object_copy(q2_ptr, &tmp);

    if (!q1_ptr->k_idx)
        object_wipe(q1_ptr);
    if (!q2_ptr->k_idx)
        object_wipe(q2_ptr);

    if (q2_ptr->k_idx)
    {
        if (player_can_treat_as_throwing(q2_ptr))
        {
            ident_on_wield(q2_ptr);

            for (i = 0; i < q2_ptr->abilities; i++)
            {
                int skill = q2_ptr->skilltype[i];
                int ability = q2_ptr->abilitynum[i];

                if (!p_ptr->have_ability[skill][ability])
                {
                    p_ptr->have_ability[skill][ability] = true;
                    p_ptr->active_ability[skill][ability] = true;
                }
            }
        }
    }

    msg_print("You swap your first and second quivers.");

    p_ptr->update |= (PU_BONUS | PU_MANA);
    p_ptr->redraw |= (PR_EQUIPPY | PR_RESIST | PR_MAP | PR_QUIVER | PR_ARC);
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

    handle_stuff();
}


static bool item_tester_hook_pack_staff(const object_type* o_ptr)
{
    return o_ptr && (o_ptr >= inventory) && (o_ptr < inventory + INVEN_PACK)
        && (o_ptr->tval == TV_STAFF);
}


static int choose_pack_staff_for_swap(void)
{
    int count = 0;
    int slot = -1;

    for (int i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx || o_ptr->tval != TV_STAFF)
            continue;

        count++;
        slot = i;
    }

    if (count == 0)
    {
        msg_print("You have no staff in your pack to swap with.");
        return -1;
    }

    if (count == 1)
        return slot;

    {
        byte old_item_tester_tval = item_tester_tval;
        bool (*old_item_tester_hook)(const object_type*) = item_tester_hook;
        bool old_item_tester_full = item_tester_full;
        bool picked;

        item_tester_tval = 0;
        item_tester_hook = item_tester_hook_pack_staff;
        item_tester_full = false;

        picked = open_inventory_item_select_menu(USE_INVEN,
            "Swap with which staff? ",
            "You have no staff in your pack to swap with.", &slot);

        item_tester_tval = old_item_tester_tval;
        item_tester_hook = old_item_tester_hook;
        item_tester_full = old_item_tester_full;

        if (!picked)
            return -1;
    }

    if ((slot < 0) || (slot >= INVEN_PACK) || inventory[slot].tval != TV_STAFF)
    {
        msg_print("That is not a staff in your pack.");
        return -1;
    }

    return slot;
}


void do_cmd_swap_staff(void)
{
    object_type* staff_ptr = &inventory[INVEN_STAFF];
    int item;

    if (!staff_ptr->k_idx || staff_ptr->tval != TV_STAFF)
    {
        msg_print("You are not wielding a walking staff.");
        return;
    }

    if (cursed_p(staff_ptr))
    {
        msg_print("You cannot bear to part with it.");
        return;
    }

    item = choose_pack_staff_for_swap();
    if (item < 0)
        return;

    do_cmd_wield(&inventory[item], item);
}
