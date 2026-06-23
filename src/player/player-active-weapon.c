#include "angband.h"
#include "externs.h"
#include "log/log.h"

static byte last_ranged_weapon_mode = PLAYER_ACTIVE_WEAPON_RANGED_1;
static byte pending_active_weapon_mode = PLAYER_ACTIVE_WEAPON_NONE;

bool player_active_weapon_mode_is_ranged(int mode)
{
    return mode == PLAYER_ACTIVE_WEAPON_RANGED_1
        || mode == PLAYER_ACTIVE_WEAPON_RANGED_2;
}

static int normalize_active_weapon_mode(int mode)
{
    switch (mode)
    {
    case PLAYER_ACTIVE_WEAPON_MELEE:
    case PLAYER_ACTIVE_WEAPON_RANGED_1:
    case PLAYER_ACTIVE_WEAPON_RANGED_2:
        return mode;
    default:
        return PLAYER_ACTIVE_WEAPON_MELEE;
    }
}

int player_active_weapon_mode(void)
{
    int mode = normalize_active_weapon_mode(p_ptr->active_weapon_mode);

    if (player_active_weapon_mode_is_ranged(mode))
        last_ranged_weapon_mode = (byte)mode;

    return mode;
}

bool player_active_weapon_is_melee(void)
{
    return player_active_weapon_mode() == PLAYER_ACTIVE_WEAPON_MELEE;
}

bool player_active_weapon_is_ranged(void)
{
    return player_active_weapon_mode_is_ranged(player_active_weapon_mode());
}

int player_active_weapon_mode_for_quiver(int quiver)
{
    return (quiver == 2) ? PLAYER_ACTIVE_WEAPON_RANGED_2
                         : PLAYER_ACTIVE_WEAPON_RANGED_1;
}

int player_active_weapon_quiver_slot(void)
{
    switch (player_active_weapon_mode())
    {
    case PLAYER_ACTIVE_WEAPON_RANGED_1:
        return INVEN_QUIVER1;
    case PLAYER_ACTIVE_WEAPON_RANGED_2:
        return INVEN_QUIVER2;
    default:
        return 0;
    }
}

int player_active_weapon_quiver_number(void)
{
    return (player_active_weapon_mode() == PLAYER_ACTIVE_WEAPON_RANGED_2)
        ? 2 : 1;
}

static bool player_switch_needs_turn(int old_mode, int new_mode)
{
    bool old_ranged = player_active_weapon_mode_is_ranged(old_mode);
    bool new_ranged = player_active_weapon_mode_is_ranged(new_mode);

    return old_ranged != new_ranged;
}

static bool player_switch_is_free(int old_mode, int new_mode)
{
    /* Versatility: readying the melee weapon after ranged is free. */
    if (player_active_weapon_mode_is_ranged(old_mode)
        && new_mode == PLAYER_ACTIVE_WEAPON_MELEE
        && p_ptr->active_ability[S_ARC][ARC_VERSATILITY])
        return true;

    /* Warden: readying a ranged weapon after melee is free, whether
     * loosing arrows or throwing.  The reverse still takes a turn. */
    if (old_mode == PLAYER_ACTIVE_WEAPON_MELEE
        && player_active_weapon_mode_is_ranged(new_mode)
        && p_ptr->active_ability[S_MEL][MEL_WARDEN])
        return true;

    return false;
}

static const char* active_weapon_mode_log_name(int mode)
{
    switch (mode)
    {
    case PLAYER_ACTIVE_WEAPON_MELEE:
        return "melee";
    case PLAYER_ACTIVE_WEAPON_RANGED_1:
        return "ranged quiver 1";
    case PLAYER_ACTIVE_WEAPON_RANGED_2:
        return "ranged quiver 2";
    default:
        return "unknown";
    }
}

static bool confirm_active_weapon_switch(int new_mode)
{
    (void)new_mode;

    msg_print("Switching between melee and ranged takes one turn; inactive weapon attack, damage, evasion, and armour bonuses do not apply.");
    msg_print("This confirmation can be removed in Gameplay Options.");
    return get_check("Change active weapon? ");
}

static void mark_active_weapon_changed(void)
{
    p_ptr->update |= PU_BONUS;
    p_ptr->redraw |= PR_BASIC | PR_MEL | PR_ARC | PR_ARMOR | PR_EQUIPPY
        | PR_QUIVER;
    p_ptr->window |= PW_PLAYER_0 | PW_EQUIP;
}

static bool object_is_one_handed_weapon(const object_type* o_ptr)
{
    u32b f1 = 0, f2 = 0, f3 = 0;

    if (!o_ptr || !o_ptr->k_idx)
        return false;
    if (o_ptr->tval != TV_SWORD && o_ptr->tval != TV_POLEARM
        && o_ptr->tval != TV_HAFTED && o_ptr->tval != TV_DIGGING)
    {
        return false;
    }

    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & TR3_TWO_HANDED)
        return false;
    if (hand_and_a_half_bonus(o_ptr))
        return false;

    return true;
}

static bool player_has_exactly_one_one_handed_weapon_equipped(void)
{
    object_type* main_hand = &inventory[INVEN_WIELD];
    object_type* off_hand = &inventory[INVEN_ARM];

    if (!object_is_one_handed_weapon(main_hand))
        return false;
    if (off_hand->k_idx && off_hand->tval != TV_SHIELD)
        return false;

    return true;
}

static bool object_is_dagger(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx && o_ptr->tval == TV_SWORD
        && (o_ptr->sval == SV_DAGGER || o_ptr->sval == SV_CHIPPED_DAGGER);
}

bool player_power_throw_weapon_eligible(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx || o_ptr->tval != TV_POLEARM)
        return false;

    return o_ptr->sval == SV_SPEAR || o_ptr->sval == SV_HAND_AXE;
}

static bool player_has_melee_weapon_equipped(void)
{
    const object_type* o_ptr = &inventory[INVEN_WIELD];

    if (!o_ptr->k_idx)
        return false;

    return o_ptr->tval == TV_SWORD || o_ptr->tval == TV_POLEARM
        || o_ptr->tval == TV_HAFTED || o_ptr->tval == TV_DIGGING;
}

bool player_power_throw_ready(void)
{
    int previous_action;

    if (!p_ptr)
        return false;
    if (!p_ptr->active_ability[S_MEL][MEL_POWER_THROW])
        return false;
    if (!player_active_weapon_is_melee())
        return false;
    if (!player_has_melee_weapon_equipped())
        return false;

    previous_action = p_ptr->previous_action[1];
    return previous_action == 5 || previous_action == ACTION_READY_MELEE;
}

bool player_can_power_throw_from_quiver(int slot)
{
    if (slot != INVEN_QUIVER1 && slot != INVEN_QUIVER2)
        return false;
    if (!player_power_throw_ready())
        return false;

    return player_power_throw_weapon_eligible(&inventory[slot]);
}

int player_power_throw_quiver_slot(void)
{
    if (player_can_power_throw_from_quiver(INVEN_QUIVER1))
        return INVEN_QUIVER1;
    if (player_can_power_throw_from_quiver(INVEN_QUIVER2))
        return INVEN_QUIVER2;
    return 0;
}

bool player_can_quick_throw_from_quiver(int slot)
{
    object_type* o_ptr;

    if (slot != INVEN_QUIVER1 && slot != INVEN_QUIVER2)
        return false;
    if (!p_ptr->active_ability[S_MEL][MEL_THROWING])
        return false;
    if (!player_has_exactly_one_one_handed_weapon_equipped())
        return false;

    o_ptr = &inventory[slot];
    return object_is_dagger(o_ptr);
}

int player_quick_throw_quiver_slot(void)
{
    if (player_can_quick_throw_from_quiver(INVEN_QUIVER1))
        return INVEN_QUIVER1;
    if (player_can_quick_throw_from_quiver(INVEN_QUIVER2))
        return INVEN_QUIVER2;
    return 0;
}

/*
 * Alchemy lets the player hurl potions at any time, with no requirement to
 * be wielding a single one-handed weapon (unlike quick-throwing daggers).
 */
bool player_can_throw_potions(void)
{
    return p_ptr->active_ability[S_PER][PER_ALCHEMY] != 0;
}

/* True if Alchemy is known and at least one potion is carried in the pack. */
bool player_has_throwable_potion(void)
{
    int i;

    if (!player_can_throw_potions())
        return false;

    for (i = 0; i < INVEN_PACK; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (o_ptr->k_idx && o_ptr->tval == TV_POTION)
            return true;
    }

    return false;
}

/*
 * Whether a quick-throw affordance should be offered at all: either a
 * quick-throwable dagger sits in a quiver, or Alchemy allows throwing a
 * carried potion.
 */
bool player_quick_throw_available(void)
{
    return player_quick_throw_quiver_slot() != 0
        || player_power_throw_quiver_slot() != 0
        || player_has_throwable_potion();
}

bool player_shield_counts_for_active_weapon(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx || o_ptr->tval != TV_SHIELD)
        return false;
    if (player_active_weapon_is_melee())
        return true;

    return player_active_weapon_is_ranged()
        && o_ptr->sval == SV_ROUND_SHIELD
        && p_ptr->active_ability[S_ARC][ARC_POINT_BLANK];
}

bool player_weapon_slot_combat_bonuses_active(int slot,
    const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    switch (slot)
    {
    case INVEN_WIELD:
        return player_active_weapon_is_melee();
    case INVEN_BOW:
        return player_active_weapon_is_ranged();
    case INVEN_ARM:
        if (o_ptr->tval == TV_SHIELD)
            return player_shield_counts_for_active_weapon(o_ptr);
        return player_active_weapon_is_melee();
    case INVEN_QUIVER1:
        return player_active_weapon_mode() == PLAYER_ACTIVE_WEAPON_RANGED_1;
    case INVEN_QUIVER2:
        return player_active_weapon_mode() == PLAYER_ACTIVE_WEAPON_RANGED_2;
    default:
        return true;
    }
}

static void player_polearm_switch_attack(void)
{
    object_type* o_ptr = &inventory[INVEN_WIELD];
    u32b f1 = 0, f2 = 0, f3 = 0;
    char o_name[80];

    if (!p_ptr->active_ability[S_MEL][MEL_POLEARMS])
        return;
    if (!o_ptr->k_idx)
        return;
    if (!p_ptr->focused || p_ptr->truce || p_ptr->confused || p_ptr->afraid
        || p_ptr->entranced || p_ptr->stun > 100)
    {
        return;
    }

    object_flags(o_ptr, &f1, &f2, &f3);
    if (!(f3 & TR3_POLEARM))
        return;

    object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

    for (int d = 0; d < 8; d++)
    {
        int y = p_ptr->py + ddy_ddd[d];
        int x = p_ptr->px + ddx_ddd[d];
        int m_idx;
        monster_type* m_ptr;
        char m_name[80];

        if (!in_bounds(y, x))
            continue;

        m_idx = cave_m_idx[y][x];
        if (m_idx <= 0)
            continue;

        m_ptr = &mon_list[m_idx];
        if (!m_ptr->ml)
            continue;
        if (forgo_attacking_unwary && m_ptr->alertness < ALERTNESS_ALERT)
            continue;

        monster_desc(m_name, sizeof(m_name), m_ptr, 0);
        if (valorous_oath_auto_attack_safety && chosen_oath(OATH_VALOROUS)
            && !oath_invalid(OATH_VALOROUS)
            && (m_ptr->stance == STANCE_FLEEING))
        {
            msg_format("As you ready your %s, %s comes into reach, but you hold back.", o_name, m_name);
            return;
        }

        msg_format("As you ready your %s, %s comes into reach.", o_name, m_name);
        py_attack_aux(y, x, ATT_POLEARM);
        return;
    }
}

bool player_set_active_weapon_mode(int mode, bool confirm, bool take_turn)
{
    int old_mode = player_active_weapon_mode();
    bool needs_turn;
    bool free_switch;

    mode = normalize_active_weapon_mode(mode);
    if (old_mode == mode)
        return true;

    needs_turn = player_switch_needs_turn(old_mode, mode);
    free_switch = player_switch_is_free(old_mode, mode);

    if (confirm && needs_turn && active_weapon_switch_confirm
        && !confirm_active_weapon_switch(mode))
    {
        return false;
    }

    p_ptr->active_weapon_mode = (byte)mode;
    if (player_active_weapon_mode_is_ranged(mode))
        last_ranged_weapon_mode = (byte)mode;

    if (mode == PLAYER_ACTIVE_WEAPON_MELEE)
        msg_print("Your active weapon is now your melee weapon.");
    else if (mode == PLAYER_ACTIVE_WEAPON_RANGED_2)
        msg_print("Your active weapon is now your ranged weapon with the 2nd quiver.");
    else
        msg_print("Your active weapon is now your ranged weapon with the 1st quiver.");

    log_info("Active weapon changed: %s -> %s",
        active_weapon_mode_log_name(old_mode), active_weapon_mode_log_name(mode));

    mark_active_weapon_changed();
    update_stuff();

    if (mode == PLAYER_ACTIVE_WEAPON_MELEE
        && player_active_weapon_mode_is_ranged(old_mode))
    {
        player_polearm_switch_attack();
    }

    if (take_turn && needs_turn && !free_switch)
    {
        p_ptr->energy_use = 100;
        p_ptr->previous_action[0]
            = (mode == PLAYER_ACTIVE_WEAPON_MELEE)
            ? ACTION_READY_MELEE : ACTION_MISC;
    }
    else if (take_turn && free_switch)
    {
        p_ptr->energy_use = 0;
        p_ptr->previous_action[0] = ACTION_NOTHING;
    }

    return true;
}

void do_cmd_toggle_active_weapon(void)
{
    int old_mode = player_active_weapon_mode();
    int new_mode;

    /* Tab toggles between melee and ranged. */
    if (player_active_weapon_mode_is_ranged(old_mode))
    {
        new_mode = PLAYER_ACTIVE_WEAPON_MELEE;
    }
    else
    {
        new_mode = normalize_active_weapon_mode(last_ranged_weapon_mode);
        if (!player_active_weapon_mode_is_ranged(new_mode))
            new_mode = PLAYER_ACTIVE_WEAPON_RANGED_1;
    }

    (void)player_set_active_weapon_mode(new_mode, true, true);
}

void player_queue_active_weapon_mode(int mode)
{
    mode = normalize_active_weapon_mode(mode);
    pending_active_weapon_mode = (byte)mode;
    if (player_active_weapon_mode_is_ranged(mode))
        last_ranged_weapon_mode = (byte)mode;
}

void do_cmd_pending_active_weapon_mode(void)
{
    int mode;

    if (pending_active_weapon_mode == PLAYER_ACTIVE_WEAPON_NONE)
        return;

    mode = normalize_active_weapon_mode(pending_active_weapon_mode);
    pending_active_weapon_mode = PLAYER_ACTIVE_WEAPON_NONE;
    (void)player_set_active_weapon_mode(mode, true, true);
}
