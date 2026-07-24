#include "angband.h"
#include "externs.h"
#include "item_set.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "ui/question.h"

#define THROW_PENDING_NONE -9999
#define THROW_CANDIDATE_MAX 26
static int throw_pending_slot = THROW_PENDING_NONE;

static int breakage_chance(const object_type* o_ptr, bool hit_wall)
{
    int p = 0;

    /* Examine the item type */
    switch (o_ptr->tval)
    {
    /* Always break */
    case TV_FLASK:
    case TV_POTION:
    {
        p = 100;
        break;
    }

    /* Often break */
    case TV_LIGHT:
    {
        /* Jewels don't break */
        if ((o_ptr->tval == TV_LIGHT)
            && ((o_ptr->sval == SV_LIGHT_SILMARIL)
                || (o_ptr->sval == SV_LIGHT_LESSER_JEWEL)))
        {
            p = 0;
        }
        else
        {
            p = 20;
        }
        break;
    }

    /* Sometimes break */
    case TV_ARROW:
    {
        p = 10;

        break;
    }

    /* Rarely break */
    default:
    {
        p = 5;

        break;
    }
    }

    /* double breakage chance if it hit a wall */
    if (hit_wall)
    {
        p *= 2;
        if (p > 100)
            p = 100;
    }
    // Unless they hit a wall, items designed for throwing won't break
    else if (player_can_treat_as_throwing(o_ptr))
    {
        p = 0;
    }

    return (p);
}

/*
 *  Determines if a bow shoots radiant arrows and lights the current grid if so
 */
bool do_radiance(int y, int x, const object_type* j_ptr)
{
    bool radiance = false;
    u32b f1 = 0, f2 = 0, f3 = 0, f4 = 0;

    if (!j_ptr || !j_ptr->k_idx)
        return false;

    object_flags4(j_ptr, &f1, &f2, &f3, &f4);
    radiance = (f2 & TR2_RADIANCE) != 0;

    // If the bow has 'radiance' and the square is dark, then light it
    if (radiance && !(cave_info[y][x] & (CAVE_GLOW)))
    {
        // Give it light
        cave_info[y][x] |= (CAVE_GLOW);

        // Remember the grid
        cave_info[y][x] |= (CAVE_MARK);

        // Fully update the visuals
        p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);

        // Update stuff
        update_stuff();

        return (true);
    }
    else
    {
        return (false);
    }
}

extern int archery_range(const object_type* j_ptr)
{
    int range;

    range = (j_ptr->dd * total_ads(j_ptr) * 3) / 2;
    if (range > MAX_RANGE)
        range = MAX_RANGE;

    return (range);
}

/*
 * Convert a thrown attack's distance into one of four penalty bands
 * across the item's maximum range.
 */
static int throwing_range_band(int dist, int max_range)
{
    int band;

    if (max_range <= 1)
        return 1;

    if (dist < 1)
        dist = 1;
    if (dist > max_range)
        dist = max_range;

    band = (((dist - 1) * 4) / (max_range - 1)) + 1;

    if (band < 1)
        band = 1;
    if (band > 4)
        band = 4;

    return band;
}

static int throwing_range_attack_penalty(int dist, int max_range)
{
    return throwing_range_band(dist, max_range);
}

static int throwing_range_ds_penalty(int dist, int max_range)
{
    static const int ds_penalties[4] = { 0, 1, 1, 2 };

    return ds_penalties[throwing_range_band(dist, max_range) - 1];
}

extern int throwing_range(const object_type* i_ptr)
{
    int div;
    int range;
    u32b f1, f2, f3;

    object_flags(i_ptr, &f1, &f2, &f3);

    /* the divisor is the weight + 2lb */
    div = i_ptr->weight + 20;

    /* Restore the original throw-range scaling. */
    range = (weight_limit() / 5) / div;

    /* Max distance of MAX_RANGE */
    if (range > MAX_RANGE)
        range = MAX_RANGE;

    /* Min distance of 1 */
    if (range < 1)
        range = 1;

    return (range);
}

/*
 * Give all adjacent, alert, non-mindless opponents (except one whose
 * coordinates are supplied) a free attack on the player.
 */
void attacks_of_opportunity(int neutralized_y, int neutralized_x)
{
    int i;
    int y, x;
    int start;
    int opportunity_attacks = 0;

    monster_type* m_ptr;
    monster_race* r_ptr;

    start = rand_int(8);

    /* Look for adjacent monsters */
    for (i = start; i < 8 + start; i++)
    {
        y = p_ptr->py + ddy_ddd[i % 8];
        x = p_ptr->px + ddx_ddd[i % 8];

        /* Check Bounds */
        if (!in_bounds(y, x))
            continue;

        // 'Point blank archery' avoids attacks of opportunity from the monster
        // shot at
        if (p_ptr->active_ability[S_ARC][ARC_POINT_BLANK]
            && (neutralized_y == y) && (neutralized_x == x))
            continue;

        // if it is occupied by a monster
        if (cave_m_idx[y][x] > 0)
        {
            m_ptr = &mon_list[cave_m_idx[y][x]];
            r_ptr = &r_info[m_ptr->r_idx];

            // the monster must be alert, not confused, and not fleeing or
            // peaceful
            if ((m_ptr->alertness >= ALERTNESS_ALERT) && !m_ptr->confused
                && (m_ptr->stance != STANCE_FLEEING)
                && !(r_ptr->flags2 & (RF2_MINDLESS))
                && !(r_ptr->flags1 & (RF1_PEACEFUL)) && !m_ptr->skip_next_turn
                && !m_ptr->skip_this_turn)
            {
                int evn = p_ptr->skill_use[S_EVN];
                opportunity_attacks++;

                if (opportunity_attacks == 1)
                {
                    msg_print("You provoke attacks of opportunity from "
                              "adjacent enemies!");
                }

                p_ptr->skill_use[S_EVN] = evn / 2;
                make_attack_normal(m_ptr);
                p_ptr->skill_use[S_EVN] = evn;
            }
        }
    }

    return;
}

/*
 * Fire an object from the pack or floor.
 *
 * See "calc_bonuses()" for more calculations and such.
 *
 * Note that "firing" a missile is MUCH better than "throwing" it.
 *
 * Note: "unseen" monsters are very hard to hit.
 *
 * Objects are more likely to break if they "attempt" to hit a monster.
 *
 * The "extra shot" code works by decreasing the amount of energy
 * required to make each shot, spreading the shots out over time.
 *
 * Note that when firing missiles, the launcher multiplier is applied
 * after all the bonuses are added in, making multipliers very useful.
 */
static bool abort_for_valorous_ranged_path(int range, int ty, int tx)
{
    u16b path_g[256];
    int path_n;
    int ty2 = ty;
    int tx2 = tx;

    if (range <= 0)
        return false;

    if (!chosen_oath(OATH_VALOROUS) || oath_invalid(OATH_VALOROUS))
        return false;

    path_n = project_path(
        path_g, range, p_ptr->py, p_ptr->px, &ty2, &tx2, PROJECT_THRU);

    for (int i = 0; i < path_n; i++)
    {
        int y = GRID_Y(path_g[i]);
        int x = GRID_X(path_g[i]);

        /* Stop before hitting walls */
        if (!cave_floor_bold(y, x))
            break;

        if (cave_m_idx[y][x] > 0)
        {
            monster_type* m_ptr = &mon_list[cave_m_idx[y][x]];
            if (m_ptr->ml && (m_ptr->stance == STANCE_FLEEING))
                return abort_for_valorous(m_ptr);
        }
    }

    return false;
}

static bool warding_girdle_can_place_glyph(int y, int x)
{
    if (!in_bounds_fully(y, x))
        return false;
    if ((y == p_ptr->py) && (x == p_ptr->px))
        return false;
    if (cave_m_idx[y][x] != 0)
        return false;
    if (!cave_clean_bold(y, x))
        return false;

    return true;
}

static void warding_girdle_spawn_glyphs(
    int impact_y, int impact_x, int step_dy, int step_dx)
{
    if (step_dy == 0 && step_dx == 0)
        return;

    int pre_y = impact_y - step_dy;
    int pre_x = impact_x - step_dx;
    if (!warding_girdle_can_place_glyph(pre_y, pre_x))
        return;

    int perp_dy = step_dx;
    int perp_dx = -step_dy;

    cave_set_feat(pre_y, pre_x, FEAT_GLYPH);

    int side1_y = pre_y + perp_dy;
    int side1_x = pre_x + perp_dx;
    if (warding_girdle_can_place_glyph(side1_y, side1_x))
        cave_set_feat(side1_y, side1_x, FEAT_GLYPH);

    int side2_y = pre_y - perp_dy;
    int side2_x = pre_x - perp_dx;
    if (warding_girdle_can_place_glyph(side2_y, side2_x))
        cave_set_feat(side2_y, side2_x, FEAT_GLYPH);

    msg_print("A warding girdle flares into being!");
}

/* During command processing, index 1 is the preceding player action because
 * process_player() has already shifted the history and cleared index 0. */
static bool player_moved_on_previous_turn(void)
{
    int action = p_ptr->previous_action[1];

    return (action >= 1) && (action <= 9) && (action != 5);
}

static void restore_target_after_implicit_fire(
    bool target_set, int target_who, int target_row, int target_col)
{
    if (!target_set)
    {
        target_set_monster(0);
        return;
    }

    if (target_who > 0)
    {
        if ((target_who < mon_max) && target_able(target_who))
            target_set_monster(target_who);
        else
            target_set_monster(0);
        return;
    }

    if (in_bounds_fully(target_row, target_col))
        target_set_location(target_row, target_col);
    else
        target_set_monster(0);
}

void do_cmd_fire(int quiver)
{
    int dir, item;
    int i, y, x, ty, tx;
    int ty2,
        tx2; // dummy variables needed to pass to the path projection function
    int first_y = 0, first_x = 0;
    int tdis;

    u32b f1 = 0, f2 = 0, f3 = 0, f4 = 0; /* combined projectile/bow flags */
    u32b bow_f1 = 0, bow_f2 = 0, bow_f3 = 0, bow_f4 = 0;
    u32b ammo_f1 = 0, ammo_f2 = 0, ammo_f3 = 0, ammo_f4 = 0;

    int attack_mod = 0, total_attack_mod = 0;
    int total_evasion_mod = 0;
    int hit_result = 0;
    int crit_bonus_dice = 0, slay_bonus_dice = 0;
    int crippling_blow_multiplier = 0;
    int total_dd = 0, total_ds = 0;
    int dam = 0, prt = 0, prt_percent = 100;
    int net_dam = 0;

    int shot;
    int shots = 1;

    object_type* o_ptr;
    object_type* j_ptr;

    object_type* i_ptr;
    object_type object_type_body;

    monster_type* m_ptr;
    monster_race* r_ptr;

    char o_name[80];
    char punctuation[20];

    int path_n;
    u16b path_g[256];

    int msec = op_ptr->delay_factor * op_ptr->delay_factor;

    u32b noticed_arrow_flag
        = 0L; // if a slay is noticed on the arrow/bow it is recorded here
    u32b noticed_bow_flag = 0L; // and the arrow/bow will be identified.

    bool noticed_radiance = false;

    bool pierce = false;
    bool targets_remaining = false;
    bool deadly_hail_bonus = false;
    bool puncture = false;
    bool returning_arrow = false;
    bool warding_girdle_active = false;
    bool clear_location_target = false;

    // Determine the projectile in the requested quiver
    if (quiver == 1)
    {
        o_ptr = &inventory[INVEN_QUIVER1];
        item = INVEN_QUIVER1;

        if (!o_ptr->k_idx)
        {
            msg_print("You have nothing in your 1st quiver.");
            return;
        }
    }
    else
    {
        o_ptr = &inventory[INVEN_QUIVER2];
        item = INVEN_QUIVER2;

        if (!o_ptr->k_idx)
        {
            msg_print("You have nothing in your 2nd quiver.");
            return;
        }
    }

    returning_arrow = false;

    /* Determine whether the item should be thrown directly */
    object_flags4(o_ptr, &ammo_f1, &ammo_f2, &ammo_f3, &ammo_f4);

    if (player_can_power_throw_from_quiver(item))
    {
        do_cmd_throw_from_slot(item);
        return;
    }

    {
        int requested_mode = player_active_weapon_mode_for_quiver(quiver);
        bool quick_throw = player_can_quick_throw_from_quiver(item)
            && player_can_treat_as_throwing_flags(o_ptr, ammo_f3);

        if (player_active_weapon_mode() != requested_mode)
        {
            if (player_active_weapon_is_melee() && quick_throw)
            {
                do_cmd_throw_from_slot(item);
                return;
            }

            if (!player_set_active_weapon_mode(requested_mode, true, true))
                return;

            if (p_ptr->energy_use > 0)
                return;
        }
    }

    if (player_can_treat_as_throwing_flags(o_ptr, ammo_f3))
    {
        do_cmd_throw_from_slot(item);
        return;
    }

    /* Get the "bow" (if any) */
    j_ptr = &inventory[INVEN_BOW];

    /* Require a usable launcher */
    if (!j_ptr->tval || !p_ptr->ammo_tval)
    {
        msg_print("You have nothing to fire with.");
        return;
    }

    /* Base range */
    tdis = archery_range(j_ptr);

    // bow flags
    object_flags4(j_ptr, &bow_f1, &bow_f2, &bow_f3, &bow_f4);

    /* Handle player fear */
    if (p_ptr->afraid)
    {
        /* Message */
        msg_print("You are too afraid to aim properly!");

        /* Done */
        return;
    }

    /* Get a direction (or cancel) */
    if (!get_aim_dir(&dir, tdis))
        return;

    /* Start at the player */
    y = p_ptr->py;
    x = p_ptr->px;

    /* Predict the "target" location */
    ty = p_ptr->py + 99 * ddy[dir];
    tx = p_ptr->px + 99 * ddx[dir];

    if ((dir == DIRECTION_UP) || (dir == DIRECTION_DOWN))
    {
        ty = p_ptr->py;
        tx = p_ptr->px;
    }

    /* Check for "target request" */
    if ((dir == 5) && target_okay(tdis))
    {
        ty = p_ptr->target_row;
        tx = p_ptr->target_col;
        clear_location_target = (p_ptr->target_who == 0);

        m_ptr = &mon_list[cave_m_idx[ty][tx]];
        r_ptr = &r_info[m_ptr->r_idx];

        if (abort_for_mercy(m_ptr))
        {
            return;
        }
    }

    /* Warn before ranged attacks that might hit fleeing enemies (Oath of Valor) */
    if (abort_for_valorous_ranged_path(tdis, ty, tx))
        return;

    /* Get local object */
    i_ptr = &object_type_body;

    /* Obtain a local object */
    object_copy(i_ptr, o_ptr);

    /* Combine the bow and ammunition flags for generic shot effects. */
    f1 = bow_f1 | ammo_f1;
    f2 = bow_f2 | ammo_f2;
    f3 = bow_f3 | ammo_f3;
    f4 = bow_f4 | ammo_f4;

    /* Determine the base attack score */
    attack_mod = (p_ptr->skill_use[S_ARC] + i_ptr->att);

    /* Single object */
    i_ptr->number = 1;

    /* Set pickup on fired arrow */
    i_ptr->pickup = true;
    i_ptr->pickup_slot = item;

    /* Set bonus: warding girdle glyphs on impact */
    warding_girdle_active
        = item_sets_warding_girdle_active_for_artefact(i_ptr->name1);

    /* Sound */
    sound(MSG_SHOOT);
    if (use_sound) {
        Term_xtra(TERM_XTRA_DELAY, 350);
    }

    /* Describe the object */
    object_desc(o_name, sizeof(o_name), i_ptr, false, 3);

    /* Skirmishing makes the shot a half-energy action after movement. */
    if (p_ptr->active_ability[S_ARC][ARC_SKIRMISHING]
        && wearing_only_light_armour() && player_moved_on_previous_turn())
    {
        p_ptr->energy_use = 50;
    }
    else
    {
        p_ptr->energy_use = 100;
    }

    // store the action type
    p_ptr->previous_action[0] = ACTION_ARCHERY;

    if (p_ptr->active_ability[S_ARC][ARC_DEADLY_HAIL])
    {
        deadly_hail_bonus = p_ptr->killed_enemy_with_arrow;
    }

    p_ptr->killed_enemy_with_arrow = false;

    // set dummy variables to pass to project_path (so it doesn't clobber the
    // real ones)
    ty2 = ty;
    tx2 = tx;

    /* Calculate the path */
    path_n = project_path(
        path_g, tdis, p_ptr->py, p_ptr->px, &ty2, &tx2, PROJECT_THRU);

    /* Hack -- Handle stuff */
    handle_stuff();

    /* If the bow has 'radiance', then light the starting square */
    noticed_radiance = do_radiance(y, x, j_ptr) || do_radiance(y, x, i_ptr);

    for (shot = 0; shot < shots; shot++)
    {
        bool hit_wall = false;
        bool ghost_arrow = false;
        bool warding_girdle_triggered = false;
        int missed_monsters = 0;
        int final_y = GRID_Y(path_g[path_n - 1]);
        int final_x = GRID_X(path_g[path_n - 1]);
        int last_step_dy = 0;
        int last_step_dx = 0;

        // abort the later shot(s) if there is no target on the trajectory
        if ((shot > 0) && !targets_remaining)
            break;
        targets_remaining = false;

        /* Reduce and describe inventory */
        if (!returning_arrow && item >= 0)
        {
            inven_item_increase(item, -1);
            inven_item_describe(item);
            inven_item_optimize(item);
        }

        /* Reduce and describe floor item */
        else if (!returning_arrow)
        {
            floor_item_increase(0 - item, -1);
            floor_item_optimize(0 - item);
        }

        /* Project along the path */
        for (i = 0; i < path_n; ++i)
        {
            int oy = y;
            int ox = x;

            int ny = GRID_Y(path_g[i]);
            int nx = GRID_X(path_g[i]);
            int step_dy = ny - oy;
            int step_dx = nx - ox;

            last_step_dy = step_dy;
            last_step_dx = step_dx;

            /* Hack -- Stop before hitting walls */
            if (!cave_floor_bold(ny, nx))
            {
                // if the arrow hasn't already stopped, do some things...
                if (!ghost_arrow)
                {
                    hit_wall = true;

                    // record resting place of arrow
                    final_y = y;
                    final_x = x;

                    if (warding_girdle_active && !warding_girdle_triggered)
                    {
                        warding_girdle_spawn_glyphs(ny, nx, step_dy, step_dx);
                        warding_girdle_triggered = true;
                    }

                    // Show collision
                    /* Only do visuals if the player can "see" the missile */
                    if (panel_contains(ny, nx))
                    {
                        /* Obtain the bolt pict */
                        u16b p = bolt_pict(y, x, y, x, GF_ARROW);

                        /* Extract attr/char */
                        byte a = PICT_A(p);
                        char c = PICT_C(p);

                        /* Display the visual effects */
                        print_rel(c, a, ny, nx);

                        move_cursor_relative(ny, nx);
                        Term_fresh();
                        Term_xtra(TERM_XTRA_DELAY, 25 * op_ptr->delay_factor);
                        lite_spot(ny, nx);
                        Term_fresh();
                    }

                    /* Delay anyway for consistency */
                    else
                    {
                        /* Pause anyway, for consistancy */
                        Term_xtra(TERM_XTRA_DELAY, 25 * op_ptr->delay_factor);
                    }
                }

                break;
            }

            /* Advance */
            x = nx;
            y = ny;

            // after an arrow has stopped, keep looking along the path,
            // but don't attempt to hit creatures, or display graphics or
            // anything
            if (ghost_arrow)
            {
                if (cave_m_idx[y][x] > 0)
                {
                    if (!forgo_attacking_unwary
                        || ((&mon_list[cave_m_idx[y][x]])->alertness
                            >= ALERTNESS_ALERT))
                        targets_remaining = true;
                }

                continue;
            }

            /* If the bow has 'radiance', then light the square being passed
             * over */
            noticed_radiance
                = do_radiance(y, x, j_ptr) || do_radiance(y, x, i_ptr)
                || noticed_radiance;

            /* Only do visuals if the player can "see" the missile */
            if (panel_contains(y, x) && player_can_see_bold(y, x))
            {
                /* Obtain the bolt pict */
                u16b p = bolt_pict(oy, ox, y, x, GF_ARROW);

                /* Extract attr/char */
                byte a = PICT_A(p);
                char c = PICT_C(p);

                /* Display the visual effects */
                print_rel(c, a, y, x);

                move_cursor_relative(y, x);
                Term_fresh();
                Term_xtra(TERM_XTRA_DELAY, msec);
                lite_spot(y, x);
                Term_fresh();
            }

            /* Delay anyway for consistency */
            else
            {
                /* Pause anyway, for consistancy */
                Term_xtra(TERM_XTRA_DELAY, msec);
            }

            /* Handle monster */
            if (cave_m_idx[y][x] > 0)
            {
                m_ptr = &mon_list[cave_m_idx[y][x]];
                r_ptr = &r_info[m_ptr->r_idx];

                if (abort_for_mercy(m_ptr))
                {
                    return;
                }

                // record the co-ordinates of the first monster in line of fire
                if (first_y == 0)
                    first_y = y;
                if (first_x == 0)
                    first_x = x;

                // Determine the player's attack score after all modifiers
                total_attack_mod = total_player_attack(m_ptr, attack_mod);

                /* Monsters might notice */
                player_attacked = true;

                // Modifications for shots that go past the target or strike
                // things before the target...
                if ((dir == 5) && target_okay(tdis))
                {
                    // if there is a specific target and this is not it, then
                    // massively penalise
                    if ((ty != y) || (tx != x))
                    {
                        total_attack_mod = 0;
                    }
                }
                // if it is just a shot in a direction and has already missed
                // something, then massively penalise
                else if (missed_monsters > 0)
                {
                    total_attack_mod = 0;
                }
                // if it is a shot in a direction and this is the first monster
                else
                {
                    /* Hack -- Track this monster race */
                    if (m_ptr->ml)
                        monster_race_track(m_ptr->r_idx);

                    /* Hack -- Track this monster */
                    if (m_ptr->ml)
                        health_track(cave_m_idx[y][x]);

                    /* Hack -- Target this monster */
                    if (m_ptr->ml)
                        target_set_monster(cave_m_idx[y][x]);
                }

                // Aim improved if monster is fleeing. If firing into several
                // fleeing monsters, the chance of hitting one is higher.
                if (p_ptr->active_ability[S_ARC][ARC_ROUT]
                    && m_ptr->stance == STANCE_FLEEING)
                {
                    total_attack_mod += 5;
                }

                // Determine the monster's evasion after all modifiers
                total_evasion_mod = total_monster_evasion(m_ptr, true);

                // No killing peaceful creatures
                if (r_ptr->flags1 & (RF1_PEACEFUL))
                {
                    hit_result = 0;
                }
                else
                {
                    /* Test for hit */
                    hit_result = hit_roll(total_attack_mod, total_evasion_mod,
                        PLAYER, m_ptr, true);
                }

                if (hit_result <= 0 && !(r_ptr->flags1 & (RF1_PEACEFUL))
                    && (f3 & TR3_ACCURATE))
                {
                    hit_result = hit_roll(total_attack_mod, total_evasion_mod,
                        PLAYER, m_ptr, true);
                    if (hit_result > 0)
                        msg_print("Your arrow flies true.");
                }

                song_disguise_note_player_attack(cave_m_idx[y][x]);

                /* If it hit */
                if (hit_result > 0)
                {
                    /* Assume a default death */
                    cptr note_dies = " dies.";

                    char m_name[80];

                    /* Get the monster name (or "it") */
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

                    /*Mark the monster as attacked by the player*/
                    m_ptr->mflag |= (MFLAG_HIT_BY_RANGED);

                    if (monster_nonliving(r_ptr))
                    {
                        /* Special note at death */
                        note_dies = " is destroyed.";
                    }

                    // Handle sharpness (which can change 'hit' message)
                    prt_percent
                        = prt_after_sharpness(i_ptr, &noticed_arrow_flag);
                    if (percent_chance(100 - prt_percent))
                    {
                        pierce = true;
                    }

                    /* Add 'critical hit' dice based on bow weight */
                    crit_bonus_dice = crit_bonus(
                        hit_result, j_ptr->weight, r_ptr, S_ARC, false, NULL, NULL);

                    if (f3 & TR3_CUMBERSOME)
                    {
                        crit_bonus_dice = 0;
                    }

                    if (p_ptr->active_ability[S_ARC][ARC_AMBUSH]
                        && m_ptr->alertness < ALERTNESS_ALERT)
                    {
                        crit_bonus_dice++;
                    }

                    /* Add slay (or brand) dice based on both arrow and bow */
                    slay_bonus_dice
                        = slay_bonus(i_ptr, m_ptr, &noticed_arrow_flag);
                    slay_bonus_dice
                        += slay_bonus(j_ptr, m_ptr, &noticed_bow_flag);

                    /* Calculate the damage done */
                    total_dd = j_ptr->dd + crit_bonus_dice + slay_bonus_dice;

                    if (deadly_hail_bonus)
                        total_dd *= 2;

                    // Sil-y: debugging test case
                    if (p_ptr->ads <= 0)
                    {
                        msg_format("BUG: Your damage sides for archery are %d.",
                            p_ptr->ads);
                        msg_format("BUG: Recalculating them would give %d.",
                            total_ads(j_ptr));
                        msg_format("BUG: j_ptr->ds is %d.", j_ptr->ds);
                    }

                    total_ds = total_ads(j_ptr);

                    /* Can't have a negative number of sides */
                    if (total_ds < 0)
                        total_ds = 0;

                    dam = damroll(total_dd, total_ds);
                    
                    /* Apply armor dice/sides curses/blessings */
                    int armor_dice_base = r_ptr->pd - m_ptr->song_armor_dice_penalty;
                    if (armor_dice_base < 0)
                        armor_dice_base = 0;
                    int armor_dice = armor_dice_base + curse_flag_delta_cur(CUR_MON_ARM_DICE);
                    int armor_sides = monster_base_armour_sides(m_ptr) + curse_flag_delta_cur(CUR_MON_ARM_SIDE);
                    if (armor_dice < 0) armor_dice = 0;
                    if (armor_sides < 1) armor_sides = 1;
                    prt = damroll(armor_dice, armor_sides);

                    prt = (prt * prt_percent) / 100;

                    if (prt >= dam
                        && p_ptr->active_ability[S_ARC][ARC_PUNCTURE])
                    {
                        puncture = true;
                        dam = 5;
                        prt = 0;
                    }

                    net_dam = dam - prt;

                    // no negative damage
                    if (net_dam < 0)
                        net_dam = 0;

                    break_mercy_oath(m_ptr, net_dam);
                    break_valorous_oath(m_ptr, net_dam, ATT_MAIN, -1);  // Direct archery shot

                    /* Handle unseen monster */
                    if (!(m_ptr->ml))
                    {
                        /* Unseen monster */
                        msg_format("The %s finds a mark.", o_name);
                    }

                    /* Handle visible monster */
                    else
                    {
                        char m_name[80];

                        /* Get "the monster" or "it" */
                        monster_desc(m_name, sizeof(m_name), m_ptr, 0);

                        // determine the punctuation for the attack ("...", ".",
                        // "!" etc)
                        attack_punctuation(
                            punctuation, net_dam, crit_bonus_dice);

                        /* Message */
                        if (pierce)
                            msg_format("The %s pierces %s%s", o_name, m_name,
                                punctuation);
                        else if (deadly_hail_bonus)
                            msg_format("The %s tears into %s!", o_name, m_name);
                        else if (puncture)
                            msg_format("The %s hits %s in a vulnerable spot%s",
                                o_name, m_name, punctuation);
                        else
                            msg_format("The %s hits %s%s", o_name, m_name,
                                punctuation);
                    }

                    if (net_dam > 0)
                    {
                        if ((f4 & TR4_ARMOR_SHATTER)
                            && (r_ptr->flags3 & RF3_HAS_ARMOUR))
                        {
                            int shatter_skill = p_ptr->skill_use[S_ARC];
                            int resist_skill = monster_skill(m_ptr, S_WIL);

                            if (skill_check(NULL, shatter_skill, resist_skill,
                                    m_ptr)
                                > 0)
                            {
                                if (m_ptr->armor_ps_reduction < r_ptr->ps)
                                {
                                    m_ptr->armor_ps_reduction++;

                                    if (m_ptr->ml)
                                    {
                                        char m_poss[80];
                                        monster_desc(
                                            m_poss, sizeof(m_poss), m_ptr, 0x22);
                                        msg_format(
                                            "Your shot shatters %s armor!",
                                            m_poss);
                                    }

                                    if (!noticed_arrow_flag
                                        && (ammo_f4 & TR4_ARMOR_SHATTER)
                                        && !object_known_p(i_ptr))
                                    {
                                        noticed_arrow_flag = TR4_ARMOR_SHATTER;
                                    }
                                    else if (!noticed_bow_flag
                                        && (bow_f4 & TR4_ARMOR_SHATTER)
                                        && !object_known_p(j_ptr))
                                    {
                                        noticed_bow_flag = TR4_ARMOR_SHATTER;
                                    }
                                }
                            }
                        }

                        if ((f3 & TR3_WILL_DRAIN)
                            && !(r_ptr->flags2 & RF2_MINDLESS))
                        {
                            int drain_skill = p_ptr->skill_use[S_ARC];
                            int resist_skill = monster_skill(m_ptr, S_WIL);

                            if (skill_check(NULL, drain_skill, resist_skill,
                                    m_ptr)
                                > 0)
                            {
                                m_ptr->song_will_penalty++;

                                if (m_ptr->ml)
                                {
                                    char m_poss[80];
                                    monster_desc(
                                        m_poss, sizeof(m_poss), m_ptr, 0x22);
                                    msg_format("Your shot drains %s will!",
                                        m_poss);
                                }

                                if (!noticed_arrow_flag
                                    && (ammo_f3 & TR3_WILL_DRAIN)
                                    && !object_known_p(i_ptr))
                                {
                                    noticed_arrow_flag = TR3_WILL_DRAIN;
                                }
                                else if (!noticed_bow_flag
                                    && (bow_f3 & TR3_WILL_DRAIN)
                                    && !object_known_p(j_ptr))
                                {
                                    noticed_bow_flag = TR3_WILL_DRAIN;
                                }
                            }
                        }
                    }

                    // if a slay or other combat effect was noticed, then
                    // identify the bow/arrow
                    if (noticed_arrow_flag || noticed_bow_flag)
                    {
                        ident_bow_arrow_by_use(j_ptr, i_ptr, o_ptr, m_ptr,
                            noticed_bow_flag, noticed_arrow_flag);
                    }

                    /* No negative damage */
                    if (net_dam < 0)
                        net_dam = 0;

                    update_combat_rolls2(total_dd, total_ds, dam, armor_dice,
                        armor_sides, prt, prt_percent, GF_HURT, false);

                    // hit the monster, check for death
                    p_ptr->killed_enemy_with_arrow = mon_take_hit(
                        cave_m_idx[y][x], net_dam, note_dies, -1);

                    if (p_ptr->killed_enemy_with_arrow
                        && (f1 & TR1_VAMPIRIC) && !monster_nonliving(r_ptr))
                    {
                        if (hp_player(7, false, false))
                        {
                            if ((ammo_f1 & TR1_VAMPIRIC)
                                && !object_known_p(i_ptr))
                            {
                                ident_bow_arrow_by_use(j_ptr, i_ptr, o_ptr,
                                    m_ptr, 0, TR1_VAMPIRIC);
                            }
                            else if ((bow_f1 & TR1_VAMPIRIC)
                                && !object_known_p(j_ptr))
                            {
                                ident_bow_arrow_by_use(j_ptr, i_ptr, o_ptr,
                                    m_ptr, TR1_VAMPIRIC, 0);
                            }
                        }
                    }

                    display_hit(
                        y, x, net_dam, GF_HURT, p_ptr->killed_enemy_with_arrow);

                    // if this wasn't the killing shot
                    if (!p_ptr->killed_enemy_with_arrow)
                    {
                        // there is at least one target left on the trajectory
                        targets_remaining = true;

                        // alert the monster, even if no damage was done
                        // (if damage was done, then it was alerted by
                        // mon_take_hit() )
                        if (net_dam == 0)
                        {
                            make_alert(m_ptr);
                        }

                        if ((f2 & (TR2_RADIANCE))
                            && r_ptr->flags3 & RF3_HURT_LITE && net_dam > 5
                            && one_in_(monster_skill(m_ptr, S_WIL)))
                        {
                            bool known_radiance
                                = object_known_p(j_ptr) || object_known_p(i_ptr)
                                || object_known_p(o_ptr) || noticed_radiance;
                            if (m_ptr->ml && known_radiance)
                            {
                                msg_format("%^s contorts as the shining arrow "
                                           "strikes it!",
                                    m_name);
                            }

                            stun_monster(m_ptr, net_dam);
                        }

                        // Morgoth drops his iron crown if he is hit for 10 or
                        // more net damage twice
                        if (m_ptr->r_idx == R_IDX_MORGOTH)
                        {
                            if (net_dam >= 10
                                && ((&a_info[ART_MORGOTH_3])->cur_num == 0))
                            {
                                if (p_ptr->morgoth_hits == 0)
                                {
                                    msg_print("The force of your shot knocks "
                                              "the Iron Crown off "
                                              "balance.");
                                    p_ptr->morgoth_hits++;
                                }
                                else if (p_ptr->morgoth_hits == 1)
                                {
                                    drop_iron_crown(m_ptr,
                                        "You knock his crown from off his "
                                        "brow, and "
                                        "it falls to the ground nearby.");
                                    p_ptr->morgoth_hits++;
                                }
                            }
                        }

                        /* Message */
                        message_pain(cave_m_idx[y][x], net_dam);

                        // Deal with crippling shot ability
                        if (p_ptr->active_ability[S_ARC][ARC_CRIPPLING]
                            && (crit_bonus_dice >= 1) && (net_dam > 0)
                            && !(r_ptr->flags1 & (RF1_RES_CRIT)))
                        {
                            // Slightly magical. Function that caps out before
                            // 30 but grows quickly early on, and
                            // doesn't need math.h
                            crippling_blow_multiplier
                                = (30 - (60 / (crit_bonus_dice + 2)));
                            if (skill_check(PLAYER, crippling_blow_multiplier,
                                    monster_skill(m_ptr, S_WIL), m_ptr)
                                > 0)
                            {
                                msg_format("Your shot cripples %^s!", m_name);

                                // slow the monster
                                // The +1 is needed as a turn of this wears off
                                // immediately
                                set_monster_slow(
                                    cave_m_idx[m_ptr->fy][m_ptr->fx],
                                    m_ptr->slowed + crit_bonus_dice + 1, false);
                            }
                        }
                    }

                    /* Stop looking if a monster was hit but not pierced */
                    if (!pierce)
                    {
                        if (warding_girdle_active && !warding_girdle_triggered)
                        {
                            warding_girdle_spawn_glyphs(
                                y, x, step_dy, step_dx);
                            warding_girdle_triggered = true;
                        }

                        // continue checking trajectory, but without affecting
                        // things
                        ghost_arrow = true;

                        // record resting place of arrow
                        final_y = y;
                        final_x = x;
                    }
                    else
                    {
                        pierce = false;
                    }
                }

                // if it misses the monster...
                else
                {
                    // there is at least one target left on the trajectory
                    targets_remaining = true;
                }

                /* we have missed a target, but could still hit something (with
                 * a penalty) */
                missed_monsters++;
            }
        }

        if (warding_girdle_active && !warding_girdle_triggered)
        {
            warding_girdle_spawn_glyphs(y, x, last_step_dy, last_step_dx);
            warding_girdle_triggered = true;
        }

        if (!object_known_p(j_ptr) && noticed_radiance)
        {
            char j_short_name[80];
            char j_full_name[80];

            object_desc(j_short_name, sizeof(j_short_name), j_ptr, false, 0);
            object_aware(j_ptr);
            object_known(j_ptr);
            object_desc(j_full_name, sizeof(j_full_name), j_ptr, true, 3);

            msg_print("The arrow leaves behind a trail of light!");
            msg_format(
                "You recognize your %s to be %s", j_short_name, j_full_name);
        }

        // Break the truce if creatures see
        break_truce(false);

        /* Drop (or break) near that location */
        if (!returning_arrow)
            drop_near(i_ptr, breakage_chance(i_ptr, hit_wall), final_y, final_x);
    }

    /* Have to set this here as well, just in case... */
    /* Monsters might notice */
    player_attacked = true;

    p_ptr->redraw |= (PR_ARC | PR_QUIVER);

    // provoke attacks of opportunity
    if (p_ptr->active_ability[S_ARC][ARC_POINT_BLANK])
        attacks_of_opportunity(first_y, first_x);
    else
        attacks_of_opportunity(0, 0);

    /* A manually aimed empty square is an input for this shot, not a lasting
     * target.  Location targets never invalidate by themselves, so retaining
     * one leaves the target cursor highlighted indefinitely.  Monster targets
     * remain tracked as before. */
    if (clear_location_target)
    {
        target_set_monster(0);
        health_track(0);
        (void)Term_set_cursor(false);
        Term_fresh();
    }
}

bool do_cmd_fire_at_adjacent(int y, int x)
{
    int dir;
    int quiver;
    bool old_target_set;
    int old_target_who;
    int old_target_row;
    int old_target_col;

    if (!player_active_weapon_is_ranged())
        return false;
    if (distance(p_ptr->py, p_ptr->px, y, x) > 1)
        return false;

    dir = motion_dir(p_ptr->py, p_ptr->px, y, x);
    if (dir == 5)
        return false;

    quiver = player_active_weapon_quiver_number();
    old_target_set = p_ptr->target_set ? true : false;
    old_target_who = p_ptr->target_who;
    old_target_row = p_ptr->target_row;
    old_target_col = p_ptr->target_col;

    p_ptr->command_dir = dir;
    do_cmd_fire(quiver);
    restore_target_after_implicit_fire(
        old_target_set, old_target_who, old_target_row, old_target_col);
    return true;
}

static cptr thrown_potion_effect_log_message(const object_type* o_ptr)
{
    if (!o_ptr || o_ptr->tval != TV_POTION)
        return NULL;

    switch (o_ptr->sval)
    {
    case SV_POTION_SLOWNESS:
        return "A slowing vapour bursts from the bottle.";
    case SV_POTION_CONFUSION:
        return "A bewildering haze bursts from the bottle.";
    case SV_POTION_true_SIGHT:
        return "A clarifying mist bursts from the bottle.";
    case SV_POTION_POISON:
        return "A poisonous cloud bursts from the bottle.";
    case SV_POTION_ORCISH_LIQUOR:
        return "The liquor bursts into flame.";
    case SV_POTION_BLINDNESS:
        return "A blinding spray bursts from the bottle.";
    case SV_POTION_DEC_DEX:
        return "A slick haze bursts from the bottle.";
    case SV_POTION_DEC_GRA:
        return "A spirit-severing mist bursts from the bottle.";
    default:
        return NULL;
    }
}

/* Apply a supported potion to the impact tile and every adjacent tile. */
static bool thrown_potion_effects(object_type* o_ptr, int center_y, int center_x)
{
    bool ident = false;
    bool affected = false;
    cptr effect_message;
    int y, x;

    if (!potion_has_thrown_effect(o_ptr))
        return false;

    effect_message = thrown_potion_effect_log_message(o_ptr);
    if (effect_message)
        msg_print(effect_message);

    /* Projection effects already handle every monster and item in the radius,
     * including their normal applied/resisted log messages. */
    switch (o_ptr->sval)
    {
    case SV_POTION_SLOWNESS:
        ident = explosion(-1, 1, center_y, center_x, 0, 0, 10, GF_SLOW);
        break;
    case SV_POTION_CONFUSION:
        ident = explosion(-1, 1, center_y, center_x, 0, 0, 10, GF_CONFUSION);
        break;
    case SV_POTION_POISON:
        ident = explosion(-1, 1, center_y, center_x, 5, 4, 0, GF_POIS);
        break;
    case SV_POTION_ORCISH_LIQUOR:
        ident = explosion(-1, 1, center_y, center_x, 3, 4, 0, GF_FIRE);
        break;
    default:
        /* The remaining effects are monster-specific and are applied below. */
        break;
    }

    for (y = center_y - 1; y <= center_y + 1; y++)
    {
        for (x = center_x - 1; x <= center_x + 1; x++)
        {
            int m_idx;
            monster_type* m_ptr;
            monster_race* r_ptr;
            monster_lore* l_ptr;
            char m_name[80];

            if (!in_bounds(y, x) || distance(center_y, center_x, y, x) > 1)
                continue;
            if ((y != center_y || x != center_x)
                && !los(center_y, center_x, y, x))
            {
                continue;
            }

            m_idx = cave_m_idx[y][x];
            if (m_idx <= 0)
                continue;

            m_ptr = &mon_list[m_idx];
            r_ptr = &r_info[m_ptr->r_idx];
            l_ptr = &l_list[m_ptr->r_idx];
            monster_desc(m_name, sizeof(m_name), m_ptr, 0);

            switch (o_ptr->sval)
            {
            case SV_POTION_true_SIGHT:
                if (!m_ptr->ml && (r_ptr->flags2 & RF2_INVISIBLE))
                {
                    m_ptr->ml = true;
                    lite_spot(y, x);
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0);
                    msg_format("%^s appears for an instant!", m_name);
                    l_ptr->flags2 |= RF2_INVISIBLE;
                    ident = true;
                    affected = true;
                }
                break;

            case SV_POTION_BLINDNESS:
                if (m_ptr->alertness >= ALERTNESS_UNWARY)
                {
                    set_alertness(m_ptr,
                        rand_range(ALERTNESS_MIN, ALERTNESS_UNWARY - 1));
                    if (m_ptr->ml)
                    {
                        msg_format("%^s is blinded and loses your trail.",
                            m_name);
                        ident = true;
                    }
                    affected = true;
                }
                break;

            case SV_POTION_DEC_DEX:
                if (!m_ptr->skip_next_turn)
                {
                    m_ptr->skip_next_turn = true;
                    if (m_ptr->ml)
                    {
                        msg_format("%^s staggers, thrown off balance.", m_name);
                        ident = true;
                    }
                    affected = true;
                }
                break;

            case SV_POTION_DEC_GRA:
                if (m_ptr->ml && (r_ptr->flags3 & RF3_NO_STUN))
                    l_ptr->flags3 |= RF3_NO_STUN;

                if (!(r_ptr->flags2 & RF2_MINDLESS)
                    && !monster_race_is_vala(m_ptr->r_idx)
                    && !(r_ptr->flags3 & RF3_NO_STUN))
                {
                    m_ptr->stunned
                        = (byte)MIN(m_ptr->stunned + damroll(4, 4), 255);
                    if (m_ptr->alertness >= ALERTNESS_UNWARY)
                    {
                        set_alertness(m_ptr,
                            rand_range(ALERTNESS_MIN, ALERTNESS_UNWARY - 1));
                    }
                    if (m_ptr->ml)
                    {
                        msg_format("%^s's spirit reels; it stands witless.",
                            m_name);
                        ident = true;
                    }
                    affected = true;
                }
                break;

            default:
                break;
            }
        }
    }

    if (!affected)
    {
        switch (o_ptr->sval)
        {
        case SV_POTION_true_SIGHT:
            msg_print("The clarifying mist reveals nothing.");
            break;
        case SV_POTION_BLINDNESS:
            msg_print("The blinding spray has no apparent effect.");
            break;
        case SV_POTION_DEC_DEX:
            msg_print("The slick haze has no apparent effect.");
            break;
        case SV_POTION_DEC_GRA:
            msg_print("The spirit-severing mist has no apparent effect.");
            break;
        default:
            break;
        }
    }

    /* An observed effect identifies the potion. */
    if (ident && !k_info[o_ptr->k_idx].aware)
    {
        char o_name[80];

        object_aware(o_ptr);
        object_known(o_ptr);
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
        msg_format("You threw %s.", o_name);
        p_ptr->notice |= PN_COMBINE | PN_REORDER;
        p_ptr->window |= PW_INVEN | PW_EQUIP | PW_PLAYER_0;
    }

    p_ptr->redraw |= PR_HEALTHBAR;
    handle_stuff();
    return true;
}

static bool quiver_slot_can_be_thrown(int slot)
{
    object_type* o_ptr;
    u32b f1 = 0, f2 = 0, f3 = 0, f4 = 0;

    if (slot != INVEN_QUIVER1 && slot != INVEN_QUIVER2)
        return false;

    o_ptr = &inventory[slot];
    if (!o_ptr->k_idx)
        return false;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    return player_can_treat_as_throwing_flags(o_ptr, f3);
}

static object_type* throw_item_object(int item)
{
    if (item >= SUPPLIES_INDEX)
        return supplies_entry_at(item - SUPPLIES_INDEX);
    if (item >= 0 && item < INVEN_TOTAL)
        return &inventory[item];
    return NULL;
}

/* Whether a resolved slot is a legal source for the Quick Throw command. */
static bool quick_throw_slot_is_valid(int item)
{
    object_type* o_ptr;

    if (item == INVEN_QUIVER1 || item == INVEN_QUIVER2)
        return player_can_quick_throw_from_quiver(item);

    o_ptr = throw_item_object(item);
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if ((item >= SUPPLIES_INDEX) || (item >= 0 && item < INVEN_PACK))
        return player_can_throw_potions() && potion_has_thrown_effect(o_ptr);

    return false;
}

/*
 * Build the list of inventory slots the player may quick-throw: eligible
 * quivered daggers, plus carried potions with real thrown effects when Alchemy
 * is known.  General throwing weapons remain on f/F.
 * Returns the number of candidates written to "slots".
 */
static int collect_throw_candidates(int* slots, int max)
{
    int n = 0;
    int i;

    if (!slots || max <= 0)
        return 0;

    if ((n < max) && player_can_quick_throw_from_quiver(INVEN_QUIVER1))
        slots[n++] = INVEN_QUIVER1;
    if ((n < max) && player_can_quick_throw_from_quiver(INVEN_QUIVER2))
        slots[n++] = INVEN_QUIVER2;

    if (player_can_throw_potions())
    {
        for (i = 0; (i < supplies_entry_count()) && (n < max); i++)
        {
            object_type* o_ptr = supplies_entry_at(i);

            if (potion_has_thrown_effect(o_ptr))
                slots[n++] = SUPPLIES_INDEX + i;
        }

        /* Keep compatibility with saves that have not ingested pack supplies. */
        for (i = 0; (i < INVEN_PACK) && (n < max); i++)
        {
            object_type* o_ptr = &inventory[i];

            if (potion_has_thrown_effect(o_ptr))
                slots[n++] = i;
        }
    }

    return n;
}

/*
 * Item-tester hook used by the throw chooser overlay: accept only quick-throw
 * candidates (eligible daggers and, with Alchemy, effective thrown potions).
 */
static bool select_throw_slot(int* item)
{
    int slots[THROW_CANDIDATE_MAX];
    int count;

    if (!item)
        return false;

    count = collect_throw_candidates(slots, (int)(sizeof(slots) / sizeof(slots[0])));

    if (count <= 0)
    {
        msg_print("You have nothing ready to throw.");
        return false;
    }
    if (count == 1)
    {
        *item = slots[0];
        return true;
    }

    /*
     * More than one possibility (daggers in both quivers and/or potions):
     * choose from a small overlay anchored at the player, like the trap and
     * door interaction popups.  This never draws on the message row.
     */
    {
        ui_question_option options[THROW_CANDIDATE_MAX];
        char labels[THROW_CANDIDATE_MAX][80];
        int i;
        int choice;

        for (i = 0; i < count; i++)
        {
            object_type* o_ptr = throw_item_object(slots[i]);
            char o_name[80];

            if (!o_ptr || !o_ptr->k_idx)
                return false;
            object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
            if (slots[i] == INVEN_QUIVER1)
                strnfmt(labels[i], sizeof(labels[i]), "%s (1st quiver)", o_name);
            else if (slots[i] == INVEN_QUIVER2)
                strnfmt(labels[i], sizeof(labels[i]), "%s (2nd quiver)", o_name);
            else
                strnfmt(labels[i], sizeof(labels[i]), "%s", o_name);

            options[i].key = (char)('a' + i);
            options[i].label = labels[i];
            options[i].attr = TERM_L_WHITE;
            options[i].disabled = false;
        }

        choice = ui_question_ask("Quick throw", "Choose what to hurl.", options,
            count, p_ptr->py, p_ptr->px, 0);

        if (choice < 0 || choice >= count)
            return false;

        *item = slots[choice];
        return true;
    }
}

/*
 * Throw an object from a quiver.
 *
 * Note: "unseen" monsters are very hard to hit.
 *
 * Should throwing a weapon do full damage?  Should it allow the magic
 * to hit bonus of the weapon to have an effect?  Should it ever cause
 * the item to be destroyed?  Should it do any damage at all?
 */
void do_cmd_throw(bool automatic)
{
    int dir, item;
    int i, j, y, x, ty, tx;
    int ty2,
        tx2; // dummy variables needed to pass to the path projection function
    int tdis;
    u32b f1 = 0, f2 = 0, f3 = 0, f4 = 0;

    int attack_mod = 0, total_attack_mod = 0;
    int total_evasion_mod = 0;
    int hit_result = 0;
    int crit_bonus_dice = 0, slay_bonus_dice = 0;
    int total_bonus_dice = 0;
    int total_ds = 0;
    int dam = 0, prt = 0, prt_percent = 100;
    int net_dam = 0;

    monster_type* m_ptr;
    monster_race* r_ptr;

    object_type* o_ptr;

    object_type* i_ptr;
    object_type object_type_body;

    bool hit_body = false;
    bool hit_wall = false;
    bool treat_as_throwing = false;
    bool has_throwing_ability = false;
    bool has_impale_ability = false;
    bool power_throw_candidate = false;
    bool power_throw_attack = false;
    bool from_supplies = false;
    int supply_index = -1;
    int power_throw_y = 0;
    int power_throw_x = 0;
    int remaining_impale_targets = 0;

    int missed_monsters = 0;

    bool spatial_target = false;

    byte missile_attr;
    char missile_char;

    char o_name[80];
    char punctuation[20];

    int path_n;
    u16b path_g[256];

    int msec = op_ptr->delay_factor * op_ptr->delay_factor;

    u32b noticed_flag
        = 0; // if a slay is noticed it is recorded here and the item identified
    u32b melee_noticed_flag = 0;

    int preset_item = throw_pending_slot;
    bool preset = (preset_item != THROW_PENDING_NONE);

    if (preset)
    {
        item = preset_item;
        automatic = false;
        if (item != INVEN_QUIVER1 && item != INVEN_QUIVER2)
        {
            throw_pending_slot = THROW_PENDING_NONE;
            msg_print("You can only throw items from a quiver.");
            return;
        }
    }

    throw_pending_slot = THROW_PENDING_NONE;

    if (!preset && !select_throw_slot(&item))
        return;

    /* f/F may preset any throwing weapon; t only accepts Quick Throw items. */
    if ((preset && !quiver_slot_can_be_thrown(item))
        || (!preset && !quick_throw_slot_is_valid(item)))
    {
        msg_print("You cannot throw that.");
        return;
    }

    from_supplies = (item >= SUPPLIES_INDEX);
    if (from_supplies)
        supply_index = item - SUPPLIES_INDEX;
    o_ptr = throw_item_object(item);

    if (!o_ptr || !o_ptr->k_idx)
    {
        if (preset)
            msg_print("You have nothing ready to throw.");
        return;
    }

    /* Hack -- Cannot remove cursed items */
    if (!from_supplies && (item >= INVEN_WIELD) && cursed_p(o_ptr))
    {
        if (p_ptr->active_ability[S_WIL][WIL_CURSE_BREAKING])
        {
            /* Message */
            msg_print("With a great strength of will, you break the curse!");

            /* Uncurse the object */
            uncurse_object(o_ptr);
        }
        else
        {
            /* Oops */
            msg_print("You cannot bear to part with it.");

            /* Nope */
            return;
        }
    }

    // Determine throwing range
    tdis = throwing_range(o_ptr);

    /* Examine the item */
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    {
        bool throwing_potion = player_can_throw_potions()
            && potion_has_thrown_effect(o_ptr);

        if (!player_can_treat_as_throwing_flags(o_ptr, f3) && !throwing_potion)
        {
            msg_print("You cannot throw that.");
            return;
        }
    }

    power_throw_candidate = player_can_power_throw_from_quiver(item);

    /* Aim at the nearest clear enemy if asked.  Do not retain an older,
     * farther target: this mode is the one-tap Quick Touch action. */
    if (automatic)
    {
        int target_m_idx;

        get_sorted_target_list(TARGET_KILL, tdis);
        if (!temp_n)
        {
            msg_print("No clear target for automatic throwing.");
            return;
        }

        target_m_idx = cave_m_idx[temp_y[0]][temp_x[0]];
        target_set_monster(target_m_idx);
        health_track(target_m_idx);
        dir = 5;

        if (p_ptr->confused)
        {
            dir = ddd[rand_int(8)];
        }

        if ((dir == 5) && target_okay(tdis))
            player_set_visual_facing_target_immediate(p_ptr->target_row, p_ptr->target_col);
        else
            player_set_visual_facing_dir_immediate(dir);
    }

    // Otherwise get a direction (or cancel) */
    else if (!get_aim_dir(&dir, tdis))
        return;

    int original_slot = (!from_supplies && item >= INVEN_WIELD) ? item : -1;

    /* If we're throwing from equipment (including quivers), set redraw flag */
    bool throwing_from_equipment = (original_slot >= INVEN_WIELD);
    if (throwing_from_equipment && (original_slot == INVEN_QUIVER1 || original_slot == INVEN_QUIVER2))
    {
        p_ptr->redraw |= (PR_QUIVER);
    }

    /* Start at the player */
    y = p_ptr->py;
    x = p_ptr->px;

    /* Predict the "target" location */
    ty = p_ptr->py + 99 * ddy[dir];
    tx = p_ptr->px + 99 * ddx[dir];

    /* Check for "target request" */
    if (dir == 5)
    {
        log_debug("do_cmd_throw: dir=5, checking target_okay(tdis=%d)", tdis);
        log_debug("do_cmd_throw: BEFORE target_okay: target_row=%d target_col=%d target_who=%d",
            p_ptr->target_row, p_ptr->target_col, p_ptr->target_who);
        
        if (!target_okay(tdis))
        {
            msg_print("You have no target.");
            return;
        }

        log_debug("do_cmd_throw: AFTER target_okay: target_row=%d target_col=%d target_who=%d",
            p_ptr->target_row, p_ptr->target_col, p_ptr->target_who);

        ty = p_ptr->target_row;
        tx = p_ptr->target_col;

        /* If we're targeting a location (not a monster), stop the throw there. */
        spatial_target = (p_ptr->target_who == 0);

        /* Oath of Mercy may block throwing at an explicit monster target. */
        if (!spatial_target && (p_ptr->target_who > 0))
        {
            monster_type* target_m_ptr = &mon_list[p_ptr->target_who];
            if (abort_for_mercy(target_m_ptr))
            {
                return;
            }
        }
    }

    if ((dir == DIRECTION_UP) || (dir == DIRECTION_DOWN))
    {
        ty = p_ptr->py;
        tx = p_ptr->px;
    }

    /* Clamp directional throws to the actual range before indexing grids */
    if ((dir != 5) && (dir != DIRECTION_UP) && (dir != DIRECTION_DOWN))
    {
        ty = p_ptr->py + tdis * ddy[dir];
        tx = p_ptr->px + tdis * ddx[dir];

        if (ty < 0)
            ty = 0;
        else if (ty >= p_ptr->cur_map_hgt)
            ty = p_ptr->cur_map_hgt - 1;

        if (tx < 0)
            tx = 0;
        else if (tx >= p_ptr->cur_map_wid)
            tx = p_ptr->cur_map_wid - 1;
    }

    /* Warn before ranged attacks that might hit fleeing enemies (Oath of Valor) */
    if (abort_for_valorous_ranged_path(tdis, ty, tx))
        return;

    /* Handle player fear */
    if (p_ptr->afraid)
    {
        /* Message */
        msg_print("You are too afraid to aim properly!");

        /* Done */
        return;
    }

    if (cave_m_idx[ty][tx] > 0)
    {
        m_ptr = &mon_list[cave_m_idx[ty][tx]];
        r_ptr = &r_info[m_ptr->r_idx];

        if (r_ptr->flags1 & (RF1_PEACEFUL))
        {
            char m_name[80];
            monster_desc(m_name, sizeof(m_name), m_ptr, 0);

            msg_format("You stop before you hit %s.", m_name);

            return;
        }

        if (abort_for_mercy(m_ptr))
        {
            return;
        }
    }

    if (power_throw_candidate)
    {
        if (dir == 5 && !spatial_target && p_ptr->target_who > 0
            && distance(p_ptr->py, p_ptr->px, ty, tx) == 1)
        {
            power_throw_attack = true;
            power_throw_y = ty;
            power_throw_x = tx;
        }
        else if (dir >= 1 && dir <= 9 && dir != 5)
        {
            int adjacent_y = p_ptr->py + ddy[dir];
            int adjacent_x = p_ptr->px + ddx[dir];

            if (in_bounds(adjacent_y, adjacent_x)
                && cave_m_idx[adjacent_y][adjacent_x] > 0)
            {
                power_throw_attack = true;
                power_throw_y = adjacent_y;
                power_throw_x = adjacent_x;
            }
        }
    }

    /*
     * A Power Throw is made from melee stance.  Other quivered throws switch
     * to their ranged mode unless they qualify for the dagger quick-throw.
     */
    if (item == INVEN_QUIVER1 || item == INVEN_QUIVER2)
    {
        int requested_mode = (item == INVEN_QUIVER2)
            ? PLAYER_ACTIVE_WEAPON_RANGED_2
            : PLAYER_ACTIVE_WEAPON_RANGED_1;

        if (player_active_weapon_mode() != requested_mode
            && !power_throw_attack
            && !(player_active_weapon_is_melee()
                && player_can_quick_throw_from_quiver(item)))
        {
            if (!player_set_active_weapon_mode(requested_mode, true, true))
                return;

            if (p_ptr->energy_use > 0)
                return;
        }
    }

    /* Set dummy variables to pass to project_path (so it doesn't clobber the real ones). */
    ty2 = ty;
    tx2 = tx;

    u32b path_flg = PROJECT_THRU;
    if (spatial_target)
        path_flg = 0;

    /* DEBUG: Log throw parameters before path calculation */
    log_debug("do_cmd_throw: dir=%d py=%d px=%d ty=%d tx=%d tdis=%d spatial=%d",
        dir, p_ptr->py, p_ptr->px, ty, tx, tdis, spatial_target ? 1 : 0);

    /* Calculate the path */
    path_n = project_path(
        path_g, tdis, p_ptr->py, p_ptr->px, &ty2, &tx2, path_flg);
    path_n = ABS(path_n);

    /* DEBUG: Log path result */
    log_debug("do_cmd_throw: path_n=%d after project_path", path_n);
    if (path_n > 0)
    {
        log_debug("do_cmd_throw: first grid=(%d,%d) last grid=(%d,%d)",
            GRID_Y(path_g[0]), GRID_X(path_g[0]),
            GRID_Y(path_g[path_n-1]), GRID_X(path_g[path_n-1]));
    }

    if (path_n <= 0)
    {
        msg_print("You cannot throw there.");
        return;
    }

    /* Get local object */
    i_ptr = &object_type_body;

    /* Obtain a local object */
    object_copy(i_ptr, o_ptr);

    /* Single object */
    i_ptr->number = 1;

    /* Set pickup on thrown quiver weapons so they can return to the quiver. */
    i_ptr->pickup = true;
    if ((original_slot == INVEN_QUIVER1) || (original_slot == INVEN_QUIVER2))
        i_ptr->pickup_slot = original_slot;
    else
        i_ptr->pickup_slot = -1;

    /* Reduce and describe inventory */
    if (from_supplies)
    {
        (void)supplies_consume_quantity(supply_index, 1);
    }
    else if (item >= 0)
    {
        inven_item_increase(item, -1);
        inven_item_describe(item);
        inven_item_optimize(item);
        if ((item == INVEN_QUIVER1 || item == INVEN_QUIVER2)
            && inventory[item].k_idx && inventory[item].number <= 0)
        {
            if (p_ptr->equip_cnt > 0)
                p_ptr->equip_cnt--;
            object_wipe(&inventory[item]);
            p_ptr->notice |= (PN_COMBINE | PN_REORDER);
            p_ptr->redraw |= PR_QUIVER;
            p_ptr->window |= (PW_EQUIP | PW_PLAYER_0);
            p_ptr->update |= PU_BONUS;
        }
    }

    /* Reduce and describe floor item */
    else
    {
        floor_item_increase(0 - item, -1);
        floor_item_optimize(0 - item);
    }

    /* Description */
    object_desc(o_name, sizeof(o_name), i_ptr, false, 3);

    /* Find the color and symbol for the object for throwing */
    missile_attr = object_attr(i_ptr);
    missile_char = object_char(i_ptr);
    treat_as_throwing = player_can_treat_as_throwing_flags(i_ptr, f3);
    has_throwing_ability = p_ptr->active_ability[S_MEL][MEL_THROWING]
        || object_grants_ability(i_ptr, S_MEL, MEL_THROWING);
    has_impale_ability = (p_ptr->active_ability[S_MEL][MEL_IMPALE]
        || object_grants_ability(i_ptr, S_MEL, MEL_IMPALE))
        && weapon_is_impale_eligible(i_ptr);
    remaining_impale_targets
        = (has_impale_ability && !power_throw_attack) ? 1 : 0;

    attack_mod = p_ptr->skill_use[S_MEL] + i_ptr->att;

    // subtract out the active melee weapon's bonus (as we had already
    // accounted for it)
    if (player_active_weapon_is_melee())
    {
        attack_mod -= (&inventory[INVEN_WIELD])->att;
        attack_mod -= axe_bonus(&inventory[INVEN_WIELD]);
        attack_mod -= polearm_bonus(&inventory[INVEN_WIELD]);
    }

    /* Weapons that are not good for throwing are much less accurate */
    if (!treat_as_throwing)
    {
        attack_mod -= 5;
    }

    // give people their weapon affinity bonuses if the weapon is thrown
    attack_mod += axe_bonus(i_ptr);
    attack_mod += polearm_bonus(i_ptr);

    if (has_throwing_ability)
        attack_mod += 1;

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = ACTION_MISC;

    /* Hack -- Handle stuff */
    handle_stuff();

    /* Project along the path */
    for (i = 0; i < path_n; ++i)
    {
        int ny = GRID_Y(path_g[i]);
        int nx = GRID_X(path_g[i]);

        log_trace("do_cmd_throw: loop i=%d ny=%d nx=%d cave_floor=%d cave_m_idx=%d",
            i, ny, nx, cave_floor_bold(ny, nx) ? 1 : 0, cave_m_idx[ny][nx]);

        /* Hack -- Stop before hitting walls */
        if (!cave_floor_bold(ny, nx))
        {
            hit_wall = true;
            log_trace("do_cmd_throw: hit wall at i=%d, breaking loop before updating y,x", i);

            // Show collision
            /* Only do visuals if the player can "see" the missile */
            if (panel_contains(ny, nx))
            {
                /* Visual effects */
                print_rel('*', TERM_L_WHITE, ny, nx);
                move_cursor_relative(ny, nx);
                Term_fresh();
                Term_xtra(TERM_XTRA_DELAY, 25 * op_ptr->delay_factor);
                lite_spot(ny, nx);
                Term_fresh();
            }

            /* Delay anyway for consistency */
            else
            {
                /* Pause anyway, for consistancy */
                Term_xtra(TERM_XTRA_DELAY, 25 * op_ptr->delay_factor);
            }
            break;
        }

        /* Advance */
        x = nx;
        y = ny;

        /* Only do visuals if the player can "see" the missile */
        if (panel_contains(y, x) && player_can_see_bold(y, x))
        {
            /* Visual effects */
            print_rel(missile_char, missile_attr, y, x);
            move_cursor_relative(y, x);
            Term_fresh();
            Term_xtra(TERM_XTRA_DELAY, msec);
            lite_spot(y, x);
            Term_fresh();
        }

        /* Delay anyway for consistency */
        else
        {
            /* Pause anyway, for consistancy */
            Term_xtra(TERM_XTRA_DELAY, msec);
        }

        /* Handle monster */
        if (cave_m_idx[y][x] > 0)
        {
            m_ptr = &mon_list[cave_m_idx[y][x]];
            r_ptr = &r_info[m_ptr->r_idx];

            bool potion_effect = potion_has_thrown_effect(i_ptr);
            bool fatal_blow = false;
            int dist = distance(p_ptr->py, p_ptr->px, m_ptr->fy, m_ptr->fx);
            int throw_attack_penalty = throwing_range_attack_penalty(dist, tdis);
            int throw_ds_penalty = throwing_range_ds_penalty(dist, tdis);
            bool power_throw_hit = power_throw_attack
                && y == power_throw_y && x == power_throw_x;
            int power_throw_concentration_bonus = 0;
            int power_throw_focus_bonus = 0;
            object_type* melee_o_ptr = &inventory[INVEN_WIELD];
            u32b melee_f1 = 0, melee_f2 = 0, melee_f3 = 0, melee_f4 = 0;
            int melee_attack_mod = 0;
            int melee_total_attack_mod = 0;
            int melee_hit_result = 0;
            int throw_attack_die = 0;
            int throw_evasion_die = 0;
            int melee_attack_die = 0;
            int melee_evasion_die = 0;
            char thrown_weapon_name[80];
            char melee_weapon_name[80];
            char power_throw_target_name[80];

            // Unaware targets are easier to hit with well-placed throws too.
            int stealth_bonus = stealth_melee_bonus(m_ptr, true);
            if (power_throw_hit)
            {
                power_throw_concentration_bonus = concentration_bonus(y, x);
                power_throw_focus_bonus = focused_attack_bonus();
                total_attack_mod = total_player_attack_ex(m_ptr,
                    attack_mod + stealth_bonus + power_throw_concentration_bonus
                        + power_throw_focus_bonus,
                    false, false);
            }
            else
            {
                total_attack_mod
                    = total_player_attack(m_ptr, attack_mod + stealth_bonus);
            }
            total_attack_mod += (dist / 5) - throw_attack_penalty;

            /* Monsters might notice */
            player_attacked = true;

            // Modifications for shots that go past the target or strike things
            // before the target...
            if ((dir == 5) && target_okay(tdis))
            {
                // if there is a specific target and this is not it, then
                // massively penalise
                if ((ty != y) || (tx != x))
                {
                    total_attack_mod = 0;
                }
            }
            // if it is just a shot in a direction and has already missed
            // something, then massively penalise
            else if (missed_monsters > 0)
            {
                total_attack_mod = 0;
            }
            // if it is a shot in a direction and this is the first monster
            else
            {
                /* Hack -- Track this monster race */
                if (m_ptr->ml)
                    monster_race_track(m_ptr->r_idx);

                /* Hack -- Track this monster */
                if (m_ptr->ml)
                    health_track(cave_m_idx[y][x]);

                /* Hack -- Target this monster */
                if (m_ptr->ml)
                    target_set_monster(cave_m_idx[y][x]);
            }

            // Determine the monster's evasion after all modifiers
            total_evasion_mod = total_monster_evasion(m_ptr, false);

            if (power_throw_hit && !(r_ptr->flags1 & (RF1_PEACEFUL)))
            {
                object_desc(thrown_weapon_name, sizeof(thrown_weapon_name),
                    i_ptr, false, 0);
                object_desc(melee_weapon_name, sizeof(melee_weapon_name),
                    melee_o_ptr, false, 0);
                monster_desc(power_throw_target_name,
                    sizeof(power_throw_target_name), m_ptr, 0);
                msg_format("Power Throw! You hurl your %s, then strike with "
                           "your %s.",
                    thrown_weapon_name, melee_weapon_name);
            }

            // No killing peaceful creatures
            if (r_ptr->flags1 & (RF1_PEACEFUL))
            {
                hit_result = 0;
            }
            else
            {
                /* Test for hit */
                if (power_throw_hit)
                {
                    hit_result = hit_roll_details(total_attack_mod,
                        total_evasion_mod, PLAYER, m_ptr, false,
                        &throw_attack_die, &throw_evasion_die);
                }
                else
                {
                    hit_result = hit_roll(total_attack_mod,
                        total_evasion_mod, PLAYER, m_ptr, true);
                }
            }

            if (hit_result <= 0 && !(r_ptr->flags1 & (RF1_PEACEFUL))
                && (f3 & TR3_ACCURATE))
            {
                if (power_throw_hit)
                {
                    hit_result = hit_roll_details(total_attack_mod,
                        total_evasion_mod, PLAYER, m_ptr, false,
                        &throw_attack_die, &throw_evasion_die);
                }
                else
                {
                    hit_result = hit_roll(total_attack_mod,
                        total_evasion_mod, PLAYER, m_ptr, true);
                }
                if (hit_result > 0)
                {
                    if (power_throw_hit)
                        msg_format("Your thrown %s flies true.",
                            thrown_weapon_name);
                    else
                        msg_format("The %s flies true.", o_name);
                }
            }

            if (power_throw_hit && !(r_ptr->flags1 & (RF1_PEACEFUL)))
            {
                int melee_stealth_bonus = stealth_melee_bonus(m_ptr, false);

                object_flags4(melee_o_ptr, &melee_f1, &melee_f2,
                    &melee_f3, &melee_f4);
                melee_attack_mod = p_ptr->skill_use[S_MEL];
                melee_total_attack_mod = total_player_attack_ex(m_ptr,
                    melee_attack_mod + melee_stealth_bonus
                        + power_throw_concentration_bonus
                        + power_throw_focus_bonus,
                    false, false);
                melee_hit_result = hit_roll_details(melee_total_attack_mod,
                    total_evasion_mod, PLAYER, m_ptr, false,
                    &melee_attack_die, &melee_evasion_die);

                if (melee_hit_result <= 0 && (melee_f3 & TR3_ACCURATE))
                {
                    melee_hit_result = hit_roll_details(
                        melee_total_attack_mod, total_evasion_mod, PLAYER,
                        m_ptr, false, &melee_attack_die, &melee_evasion_die);
                    if (melee_hit_result > 0)
                    {
                        object_desc(melee_weapon_name,
                            sizeof(melee_weapon_name), melee_o_ptr, false, 0);
                        msg_format(
                            "Your wielded %s strikes true.",
                            melee_weapon_name);
                    }
                }

                msg_format("Your thrown %s %s %s.", thrown_weapon_name,
                    (hit_result > 0) ? "hits" : "misses",
                    power_throw_target_name);
                update_combat_rolls1(PLAYER, m_ptr, m_ptr->ml,
                    total_attack_mod, throw_attack_die, total_evasion_mod,
                    throw_evasion_die);
                update_combat_rolls_no_damage();
                msg_format("Your wielded %s %s %s.", melee_weapon_name,
                    (melee_hit_result > 0) ? "hits" : "misses",
                    power_throw_target_name);
                update_combat_rolls1(PLAYER, m_ptr, m_ptr->ml,
                    melee_total_attack_mod, melee_attack_die,
                    total_evasion_mod, melee_evasion_die);
            }

            song_disguise_note_player_attack(cave_m_idx[y][x]);

            /* Resolve every successful component against one protection roll. */
            if (hit_result > 0 || melee_hit_result > 0)
            {
                int melee_crit_bonus_dice = 0;
                int melee_slay_bonus_dice = 0;
                int melee_total_dice = 0;
                int melee_ds = 0;
                int melee_dam = 0;
                int thrown_dam = 0;
                bool thrown_hit = hit_result > 0;
                bool melee_hit = melee_hit_result > 0;

                msg_print("Power Throw combines all successful weapon damage "
                          "against one armor roll.");

                /* Note the collision */
                hit_body = thrown_hit;

                /* Assume a default death */
                cptr note_dies = " dies.";

                /*Mark the monster as attacked by the player*/
                if (thrown_hit)
                    m_ptr->mflag |= (MFLAG_HIT_BY_RANGED);
                if (melee_hit)
                    m_ptr->mflag |= (MFLAG_HIT_BY_MELEE);

                /* Some monsters get "destroyed" */
                if (monster_nonliving(r_ptr))
                {
                    /* Special note at death */
                    note_dies = " is destroyed.";
                }

                /* Apply special damage XXX XXX XXX */
                if (thrown_hit)
                {
                    crit_bonus_dice = crit_bonus(hit_result, i_ptr->weight,
                        r_ptr, S_MEL, true, NULL, i_ptr);

                    if (f3 & TR3_CUMBERSOME)
                        crit_bonus_dice = 0;

                    slay_bonus_dice
                        = slay_bonus(i_ptr, m_ptr, &noticed_flag);

                    /* Calculate the damage from the thrown object */
                    total_bonus_dice = crit_bonus_dice + slay_bonus_dice;
                    total_ds = strength_modified_ds(i_ptr, 0);

                    /* Penalise items that aren't made to be thrown */
                    if (!treat_as_throwing)
                        total_ds /= 2;

                    if (has_throwing_ability && throw_ds_penalty > 0)
                        throw_ds_penalty--;

                    total_ds -= throw_ds_penalty;

                    /* Can't have a negative number of sides */
                    if (total_ds < 0)
                        total_ds = 0;

                    thrown_dam
                        = damroll(i_ptr->dd + total_bonus_dice, total_ds);
                }

                dam = thrown_dam;

                if (melee_hit)
                {
                    melee_crit_bonus_dice = crit_bonus(melee_hit_result,
                        melee_o_ptr->weight, r_ptr, S_MEL, false, NULL,
                        melee_o_ptr);
                    if (melee_f3 & TR3_CUMBERSOME)
                        melee_crit_bonus_dice = 0;

                    melee_slay_bonus_dice
                        = slay_bonus(melee_o_ptr, m_ptr, &melee_noticed_flag);
                    melee_total_dice = p_ptr->mdd + melee_crit_bonus_dice
                        + melee_slay_bonus_dice;
                    melee_ds = p_ptr->mds;
                    melee_dam = damroll(melee_total_dice, melee_ds);
                    dam += melee_dam;
                }
                
                /* Apply armor dice/sides curses/blessings */
                int armor_dice_base = r_ptr->pd - m_ptr->song_armor_dice_penalty;
                if (armor_dice_base < 0)
                    armor_dice_base = 0;
                int armor_dice = armor_dice_base + curse_flag_delta_cur(CUR_MON_ARM_DICE);
                int armor_sides = monster_base_armour_sides(m_ptr) + curse_flag_delta_cur(CUR_MON_ARM_SIDE);
                if (armor_dice < 0) armor_dice = 0;
                if (armor_sides < 1) armor_sides = 1;
                prt = damroll(armor_dice, armor_sides);

                prt_percent = 100;
                if (thrown_hit)
                    prt_percent = prt_after_sharpness(i_ptr, &noticed_flag);
                if (melee_hit)
                {
                    int melee_prt_percent
                        = prt_after_sharpness(melee_o_ptr,
                            &melee_noticed_flag);
                    prt_percent = thrown_hit
                        ? MIN(prt_percent, melee_prt_percent)
                        : melee_prt_percent;
                }
                prt = (prt * prt_percent) / 100;

                net_dam = dam - prt;

                // no negative damage
                if (net_dam < 0)
                    net_dam = 0;

                break_mercy_oath(m_ptr, net_dam);
                break_valorous_oath(m_ptr, net_dam, ATT_MAIN, -1);  // Direct thrown weapon

                /* Handle unseen monster */
                if (!(m_ptr->ml))
                {
                    /* Invisible monster */
                    if (!power_throw_hit)
                        msg_format("The %s finds a mark.", o_name);
                }

                /* Handle visible monster */
                else
                {
                    char m_name[80];

                    /* Get "the monster" or "it" */
                    monster_desc(m_name, sizeof(m_name), m_ptr, 0);

                    int best_crit = MAX(
                        crit_bonus_dice, melee_crit_bonus_dice);

                    if (p_ptr->active_ability[S_STL][STL_CRUEL_BLOW]
                        && (best_crit > 0) && (net_dam > 0)
                        && !(r_ptr->flags1 & (RF1_RES_CRIT)))
                    {
                        int cruel_blow_multiplier
                            = (30 - (60 / (best_crit + 2)));
                        if (skill_check(PLAYER, cruel_blow_multiplier,
                                monster_skill(m_ptr, S_WIL), m_ptr)
                            > 0)
                        {
                            msg_format("%s reels in pain!", m_name);

                            if (!monster_race_is_vala(m_ptr->r_idx)
                                && !(r_ptr->flags3 & (RF3_NO_CONF)))
                                m_ptr->confused += best_crit + 1;

                            scare_onlooking_friends(m_ptr, -20);
                        }
                    }

                    // determine the punctuation for the attack ("...", ".", "!"
                    // etc)
                    attack_punctuation(punctuation, net_dam,
                        MAX(crit_bonus_dice, melee_crit_bonus_dice));

                    /* Message */
                    if (!power_throw_hit)
                    {
                        if (thrown_hit && melee_hit)
                        {
                            msg_format("You strike %s as the %s hits%s",
                                m_name, o_name, punctuation);
                        }
                        else if (melee_hit)
                        {
                            msg_format("You hit %s as the %s flies past%s",
                                m_name, o_name, punctuation);
                        }
                        else
                        {
                            msg_format("The %s hits %s%s", o_name, m_name,
                                punctuation);
                        }
                    }

                    /* Hack -- Track this monster race */
                    if (m_ptr->ml)
                        monster_race_track(m_ptr->r_idx);

                    /* Hack -- Track this monster */
                    if (m_ptr->ml)
                        health_track(cave_m_idx[y][x]);

                    /* Hack -- Target this monster */
                    if (m_ptr->ml)
                        target_set_monster(cave_m_idx[y][x]);
                }

                // if a slay was noticed, then identify the weapon
                if (noticed_flag)
                {
                    if (power_throw_hit)
                    {
                        ident_weapon_by_use_context(
                            i_ptr, m_ptr, noticed_flag, "thrown");
                    }
                    else
                    {
                        ident_weapon_by_use(i_ptr, m_ptr, noticed_flag);
                    }
                }
                if (melee_noticed_flag)
                {
                    ident_weapon_by_use_context(
                        melee_o_ptr, m_ptr, melee_noticed_flag, "wielded");
                    melee_noticed_flag = 0;
                }

                /* No negative net damage */
                if (net_dam < 0)
                    net_dam = 0;

                if (power_throw_hit)
                {
                    update_combat_rolls2_combo(
                        thrown_hit ? i_ptr->dd + total_bonus_dice : 0,
                        thrown_hit ? total_ds : 0, thrown_dam,
                        melee_hit ? melee_total_dice : 0,
                        melee_hit ? melee_ds : 0, melee_dam, armor_dice,
                        armor_sides, prt, prt_percent, GF_HURT, true);
                }
                else
                {
                    update_combat_rolls2(i_ptr->dd + total_bonus_dice, total_ds,
                        dam, armor_dice, armor_sides, prt, prt_percent,
                        GF_HURT, false);
                }

                /* Hit the monster, unless a potion effect has already been done
                 */
                if (!potion_effect)
                {
                    fatal_blow = (mon_take_hit(
                        cave_m_idx[y][x], net_dam, note_dies, -1));
                }

                display_hit(y, x, net_dam, GF_HURT, fatal_blow);
                if (thrown_hit)
                {
                    apply_weapon_combat_effects(
                        i_ptr, m_ptr, S_MEL, net_dam, fatal_blow, "throw");
                }
                if (melee_hit && !fatal_blow)
                {
                    apply_weapon_combat_effects(melee_o_ptr, m_ptr, S_MEL,
                        net_dam, false, "power throw");
                }

                /* Still alive */
                if (!fatal_blow)
                {
                    // alert the monster, even if no damage was done
                    // (if damage was done, then it was alerted by
                    // mon_take_hit() )
                    if (net_dam == 0)
                    {
                        make_alert(m_ptr);
                    }

                    // Morgoth drops his iron crown if he is hit for 10 or more
                    // net damage twice
                    if ((m_ptr->r_idx == R_IDX_MORGOTH)
                        && ((&a_info[ART_MORGOTH_3])->cur_num == 0))
                    {
                        if (net_dam >= 10)
                        {
                            if (p_ptr->morgoth_hits == 0)
                            {
                                msg_print("The force of your blow knocks the "
                                          "Iron Crown off "
                                          "balance.");
                                p_ptr->morgoth_hits++;
                            }
                            else if (p_ptr->morgoth_hits == 1)
                            {
                                drop_iron_crown(m_ptr,
                                    "You knock his crown from off his brow, "
                                    "and it "
                                    "falls to the ground nearby.");
                                p_ptr->morgoth_hits++;
                            }
                        }
                    }

                    /* Message if applicable*/
                    if (!potion_effect)
                        message_pain(cave_m_idx[y][x], net_dam);
                }

                if (remaining_impale_targets > 0)
                {
                    remaining_impale_targets--;
                    continue;
                }
                /* Stop looking if a monster was hit */
                break;
            }
            else
            {
                if (power_throw_hit)
                {
                    update_combat_rolls_no_damage();
                    make_alert(m_ptr);
                    break;
                }

                /* we have missed a target, but could still hit something (with
                 * a penalty) */
                missed_monsters++;
            }
        }
    }

    /* Potions burst at their final impact grid, even on a near miss, and
     * splash every grid within radius one. */
    if (i_ptr->tval == TV_POTION)
    {
        msg_print("The bottle breaks.");
        (void)thrown_potion_effects(i_ptr, y, x);
    }

    /* Have to set this here as well, just in case... */
    /* Monsters might notice */
    player_attacked = true;

    /* Chance of breakage (during attacks) */
    j = breakage_chance(i_ptr, hit_wall);

    /* throwing weapons have a lesser chance */
    if (treat_as_throwing)
        j /= 4;

    /* DEBUG: Log final drop position */
    log_debug("do_cmd_throw: dropping at y=%d x=%d (player at %d,%d) hit_body=%d",
        y, x, p_ptr->py, p_ptr->px, hit_body ? 1 : 0);

    /* Drop (or break) near that location */
    drop_near(i_ptr, j, y, x);

    if (power_throw_attack)
    {
        (void)player_set_active_weapon_mode(
            PLAYER_ACTIVE_WEAPON_MELEE, false, false);
        p_ptr->redraw |= (PR_MEL | PR_ARC | PR_QUIVER);
    }

    // Break the truce if creatures see
    break_truce(false);
}

/*
 * Throw the item currently stored in the supplied slot.
 */
void do_cmd_throw_from_slot(int slot)
{
    throw_pending_slot = slot;
    do_cmd_throw(false);
}
