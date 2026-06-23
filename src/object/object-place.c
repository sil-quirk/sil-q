/* File: object/object-place.c */

#include "angband.h"
#include "externs.h"
#include "object/object-place.h"
#include "log/log.h"


s16b floor_carry(int y, int x, object_type* j_ptr)
{
    int n = 0;
    bool under_player = (cave_m_idx[y][x] < 0);

    s16b o_idx;

    s16b this_o_idx, next_o_idx = 0;

    /* Scan objects in that grid for combination */
    for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr;

        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Check for combination */
        if (object_similar(o_ptr, j_ptr))
        {
            /* Combine the items */
            object_absorb(o_ptr, j_ptr);

            if (under_player)
            {
                o_ptr->marked = true;
                lite_spot(y, x);
            }

            if (j_ptr->number == 0)
            {
                /* Result */
                return (this_o_idx);
            }
        }

        /* Count objects */
        n++;
    }

    /* The stack is already too large */
    if (n > MAX_FLOOR_STACK)
        return (0);

    // Sil: force no stacking
    if (n)
        return (0);

    /* Make an object */
    o_idx = o_pop();

    /* Success */
    if (o_idx)
    {
        object_type* o_ptr;

        /* Get the object */
        o_ptr = &o_list[o_idx];

        /* Structure Copy */
        object_copy(o_ptr, j_ptr);

        /* Location */
        o_ptr->iy = y;
        o_ptr->ix = x;

        /* Forget monster */
        o_ptr->held_m_idx = 0;

        /* Link the object to the pile */
        o_ptr->next_o_idx = cave_o_idx[y][x];

        /* Link the floor to the object */
        cave_o_idx[y][x] = o_idx;

        if (under_player)
            o_ptr->marked = true;

        /* Notice */
        note_spot(y, x);

        /* Redraw */
        lite_spot(y, x);
    }

    /* Result */
    return (o_idx);
}

/*
 * Let an object fall to the ground at or near a location.
 *
 * The initial location is assumed to be "in_bounds_fully()".
 *
 * This function takes a parameter "chance".  This is the percentage
 * chance that the item will "disappear" instead of drop.  If the object
 * has been thrown, then this is the chance of disappearance on contact.
 *
 * Hack -- this function uses "chance" to determine if it should produce
 * some form of "description" of the drop event (under the player).
 *
 * We check several locations to see if we can find a location at which
 * the object can combine, stack, or be placed. Artefacts and thrown/fired
 * auto-recovery objects will try very hard to be placed, including
 * "teleporting" to a useful grid if needed.
 */
s16b drop_near(object_type* j_ptr, int chance, int y, int x)
{
    int i, k, d, s;

    int bs, bn;
    int by, bx;
    int dy, dx;
    int ty, tx;

    object_type* o_ptr;

    char o_name[80];

    bool flag = false;

    bool plural = false;
    const bool is_silmaril = (j_ptr->tval == TV_LIGHT) && (j_ptr->sval == SV_LIGHT_SILMARIL);
    const bool impact_is_floor =
        (cave_feat[y][x] == FEAT_FLOOR) || (cave_feat[y][x] == FEAT_SUNLIGHT);
    const bool force_place = artefact_p(j_ptr) || is_silmaril || j_ptr->pickup;
    const bool try_hard_place = force_place || impact_is_floor;
    const bool can_clobber = force_place;
    const int scan_radius = try_hard_place ? 10 : 4;
    const int scan_dist2_max = (scan_radius * scan_radius) + 1;

    /* Extract plural */
    if (j_ptr->number != 1)
        plural = true;

    /* Describe object */
    object_desc(o_name, sizeof(o_name), j_ptr, false, 0);

    /* Handle normal "breakage" */
    if (!artefact_p(j_ptr) && percent_chance(chance))
    {
        // The potion breaking message has already been displayed
        if (j_ptr->tval != TV_POTION)
        {
            /* Message */
            msg_format("The %s break%s.", o_name, (plural ? "" : "s"));
        }

        /* Debug */
        // if (p_ptr->wizard) msg_print("Breakage (breakage).");

        /* Failure */
        return (0);
    }

    /* Score */
    bs = -1;

    /* Picker */
    bn = 0;

    /* Default */
    by = y;
    bx = x;

    /* Scan local grids */
    for (dy = -scan_radius; dy <= scan_radius; dy++)
    {
        /* Scan local grids */
        for (dx = -scan_radius; dx <= scan_radius; dx++)
        {
            bool comb = false;
            ////int path_n;
            ////u16b path_g[256];
            ////int ty2, tx2; // store a copy of the target grid that can get
            /// changed by project_path()

            /* Calculate actual distance */
            d = (dy * dy) + (dx * dx);

            /* Ignore distant grids */
            if (d > scan_dist2_max)
                continue;

            /* Location */
            ty = y + dy;
            tx = x + dx;

            // copy of the variables
            ////ty2 = ty;
            ////tx2 = tx;

            /* Skip illegal grids */
            if (!in_bounds_fully(ty, tx))
                continue;

            /* Require line of sight */
            if (!los(y, x, ty, tx))
                continue;

            /* Calculate the path */
            ////path_n = project_path(path_g, 10, p_ptr->py, p_ptr->px, &ty2,
            ///&tx2, PROJECT_NO_CHASM);

            // if there was a chasm in the way, skip this spot
            ////if ((ty != ty2) || (tx != tx2)) continue;

            /* Require floor space */
            if (cave_feat[ty][tx] != FEAT_FLOOR
                && cave_feat[ty][tx] != FEAT_SUNLIGHT)
                continue;

            /* Don't put things under peaceful monsters */
            if (cave_m_idx[ty][tx] > 0 && !attacker_at(ty, tx))
                continue;

            /* No objects */
            k = 0;

            /* Scan objects in that grid */
            for (o_ptr = get_first_object(ty, tx); o_ptr;
                 o_ptr = get_next_object(o_ptr))
            {
                /* Check for possible combination */
                if (object_similar(o_ptr, j_ptr))
                    comb = true;

                /* Count objects */
                k++;
            }

            /* Add new object */
            if (!comb)
                k++;

            // Sil: force no stacking
            if (k > 1)
                continue;

            /* Paranoia */
            if (k > MAX_FLOOR_STACK)
                continue;

            /* Calculate score */
            s = 1000 - (d + k * 5);

            /* Skip bad values */
            if (s < bs)
                continue;

            /* New best value */
            if (s > bs)
                bn = 0;

            /* Apply the randomizer to equivalent values */
            if ((++bn >= 2) && (rand_int(bn) != 0))
                continue;

            /* Keep score */
            bs = s;

            /* Track it */
            by = ty;
            bx = tx;

            /* Okay */
            flag = true;
        }
    }

    /* Handle lack of space */
    if (!flag && !try_hard_place)
    {
        /* Debug */
        if (p_ptr->wizard)
            msg_print("Breakage (no floor space).");

        /* Failure */
        return (0);
    }

    /* Don't silently lose items just because there is no nearby empty floor. */
    for (i = 0; try_hard_place && !flag && (i < 20000); i++)
    {
        /* First try */
        if (i == 0)
        {
            ty = y;
            tx = x;
        }

        /* Bounce around */
        else if (i < 100)
        {
            ty = rand_range(by - 1, by + 1);
            tx = rand_range(bx - 1, bx + 1);
        }

        /* Get deperate and teleport it somewhere*/
        else
        {
            ty = rand_int(p_ptr->cur_map_hgt);
            tx = rand_int(p_ptr->cur_map_wid);
        }

        /* Skip illegal grids */
        if (!in_bounds_fully(ty, tx))
            continue;

        /* Require floor space */
        if (cave_feat[ty][tx] != FEAT_FLOOR && cave_feat[ty][tx] != FEAT_SUNLIGHT)
            continue;

        /* Don't put things under peaceful monsters */
        if (cave_m_idx[ty][tx] > 0 && !attacker_at(ty, tx))
            continue;

        /* Bounce to that location */
        by = ty;
        bx = tx;

        // Clear ordinary junk if this object must be force-placed.
        if (can_clobber && cave_o_idx[ty][tx] != 0)
        {
            object_type* o_ptr = &o_list[cave_o_idx[ty][tx]];
            const bool o_is_silmaril =
                (o_ptr->tval == TV_LIGHT) && (o_ptr->sval == SV_LIGHT_SILMARIL);
            if (!artefact_p(o_ptr) && !o_is_silmaril)
            {
                /* Delete the object */
                delete_object_idx(cave_o_idx[ty][tx]);
            }
        }

        /* Require an empty grid */
        if (cave_o_idx[by][bx] != 0)
            continue;

        /* Require floor space */
        if (cave_feat[by][bx] != FEAT_FLOOR && cave_feat[by][bx] != FEAT_SUNLIGHT)
            continue;

        /* Okay */
        flag = true;
    }

    /* Give it to the floor */
    s16b o_idx = floor_carry(by, bx, j_ptr);
    if (!o_idx)
    {
        /* Message */
        if (player_has_los_bold(y, x))
        {
            msg_format("The %s disappear%s.", o_name, (plural ? "" : "s"));
        }

        /* Debug */
        if (p_ptr->wizard)
            msg_print("Breakage (too many objects).");

        /* Failure */
        return (0);
    }

    update_stuff();

    /* Sound - material-based drop sound (strictly matches design table) */
    {
        int drop_sound = MSG_DROP_GENERIC;
        const bool is_boots = (j_ptr->tval == TV_BOOTS);
        const bool is_gloves = (j_ptr->tval == TV_GLOVES);
        const bool is_greaves = is_boots &&
            (j_ptr->sval == SV_PAIR_OF_STEEL_GREAVES || j_ptr->sval == SV_PAIR_OF_MITHRIL_GREAVES);
        const bool is_gauntlets = is_gloves && (j_ptr->sval == SV_SET_OF_GAUNTLETS);

        if (j_ptr->tval == TV_POTION || j_ptr->tval == TV_FLASK || j_ptr->tval == TV_GEM ||
            (j_ptr->tval == TV_LIGHT && j_ptr->sval == SV_LIGHT_SILMARIL)) {
            drop_sound = MSG_DROP_GLASS;
        }
        else if (j_ptr->tval == TV_RING || j_ptr->tval == TV_AMULET ||
                 (j_ptr->tval == TV_LIGHT && (j_ptr->sval == SV_LIGHT_FEANORIAN ||
                                               j_ptr->sval == SV_LIGHT_LESSER_JEWEL))) {
            drop_sound = MSG_DROP_SMALL_METAL;
        }
        else if ((j_ptr->tval == TV_SOFT_ARMOR && j_ptr->sval == SV_ROBE) ||
                 j_ptr->tval == TV_FOOD || j_ptr->tval == TV_EASTER || j_ptr->tval == TV_NOTE) {
            drop_sound = MSG_DROP_CLOTH;
        }
        else if ((is_boots && !is_greaves) || (is_gloves && !is_gauntlets) ||
                 (j_ptr->tval == TV_SOFT_ARMOR &&
                  (j_ptr->sval == SV_LEATHER_ARMOR || j_ptr->sval == SV_STUDDED_LEATHER))) {
            drop_sound = MSG_DROP_LEATHER;
        }
        else if (j_ptr->tval == TV_MAIL || j_ptr->tval == TV_SHIELD ||
                 j_ptr->tval == TV_CHEST || j_ptr->tval == TV_METAL || j_ptr->tval == TV_DIGGING ||
                 (j_ptr->tval == TV_HELM && (j_ptr->sval == SV_GREAT_HELM || j_ptr->sval == SV_DWARF_MASK))) {
            drop_sound = MSG_DROP_BIG_METAL;
        }
        else if (j_ptr->tval == TV_SWORD || j_ptr->tval == TV_POLEARM || j_ptr->tval == TV_CROWN ||
                 (j_ptr->tval == TV_HELM && j_ptr->sval != SV_GREAT_HELM && j_ptr->sval != SV_DWARF_MASK) ||
                 (j_ptr->tval == TV_LIGHT && j_ptr->sval == SV_LIGHT_LANTERN) ||
                 is_greaves || is_gauntlets) {
            drop_sound = MSG_DROP_METAL_MEDIUM;
        }
        else if (j_ptr->tval == TV_HAFTED || j_ptr->tval == TV_STAFF || j_ptr->tval == TV_HORN ||
                 j_ptr->tval == TV_ARROW ||
                 (j_ptr->tval == TV_LIGHT && (j_ptr->sval == SV_LIGHT_TORCH ||
                                               j_ptr->sval == SV_LIGHT_MALLORN))) {
            drop_sound = MSG_DROP_WOOD;
        }
        else {
            drop_sound = MSG_DROP_GENERIC;
        }

        /* Only play drop sound while the player is actively in a live dungeon. */
        if (character_dungeon) {
            sound(drop_sound);
        }
    }

    /* Mega-Hack -- no message if "dropped" by player */
    /* Message when an object falls under the player */
    if (chance && (cave_m_idx[by][bx] < 0))
    {
        msg_print("You feel something roll beneath your feet.");
    }

    return (o_idx);
}

/*
 * Scatter some weighted-quality objects near the player
 */
void acquirement(int y1, int x1, int num, drop_quality quality)
{
    object_type* i_ptr;
    object_type object_type_body;
    drop_quality spawn_quality =
        (quality < DROP_QUALITY_GOOD) ? DROP_QUALITY_GOOD : quality;

    /* Acquirement */
    while (num--)
    {
        /* Get local object */
        i_ptr = &object_type_body;

        /* Wipe the object */
        object_wipe(i_ptr);

        /* Make a good-or-better object (if possible) */
        if (!make_object(i_ptr, spawn_quality, DROP_TYPE_NOT_DAMAGED))
            continue;

        /* Drop the object */
        drop_near(i_ptr, -1, y1, x1);
    }
}

/*
 * Attempt to place an object (normal or weighted quality) at the given location.
 */
void place_object(int y, int x, drop_quality quality, int droptype,
    bool allow_artefacts)
{
    object_type* i_ptr;
    object_type object_type_body;

    /* Paranoia */
    if (!in_bounds(y, x))
        return;

    /* Hack -- clean floor space */
    if (!cave_clean_bold(y, x))
        return;

    /* Get local object */
    i_ptr = &object_type_body;

    /* Wipe the object */
    object_wipe(i_ptr);

    /* Make an object (if possible) */
    int depth = object_level;
    while (!drop_generate_object(depth, quality, droptype, allow_artefacts, i_ptr))
        continue;

    /* Give it to the floor */
    if (!floor_carry(y, x, i_ptr))
    {
        /* Hack -- Preserve artefacts */
        a_info[i_ptr->name1].cur_num = 0;
    }
}

/*
 * Choose a trap type, place it in the dungeon at the given grid and 'hide' it
 *
 */
void place_trap(int y, int x)
{
    int feat;

    /* Paranoia */
    if (!in_bounds(y, x))
        return;

    /* Require empty, clean, floor grid */
    if (!cave_naked_bold(y, x))
        return;

    bool prefer_web = (p_ptr->depth >= 8)
        && (level_partition_kind_for_point(y, x) == LEVEL_PART_CAVEY);

    /* Pick a trap */
    while (1)
    {
        /* Hack -- pick a trap */
        if (prefer_web && (rand_int(100) < 45))
            feat = FEAT_TRAP_WEB;
        else
            feat = rand_range(FEAT_TRAP_HEAD, FEAT_TRAP_TAIL);

        switch (feat)
        {
        case FEAT_TRAP_false_FLOOR:
        {
            // 5-18
            if (p_ptr->depth < 5)
                continue;
            if (p_ptr->depth > 18)
                continue;

            // skip half the time as they are otherwise too common
            if (one_in_(2))
                continue;
            break;
        }
        case FEAT_TRAP_PIT:
        {
            // 5-10
            if (p_ptr->depth < 5)
                continue;
            if (p_ptr->depth > 10)
                continue;
            break;
        }
        case FEAT_TRAP_SPIKED_PIT:
        {
            // 0, 11-17
            if (p_ptr->depth == 0)
                break;
            if (p_ptr->depth < 11)
                continue;
            if (p_ptr->depth > 17)
                continue;
            break;
        }
        case FEAT_TRAP_DART:
        {
            // 8-15
            if (p_ptr->depth < 8)
                continue;
            if (p_ptr->depth > 15)
                continue;
            break;
        }
        case FEAT_TRAP_GAS_CONF:
        {
            // 1-13
            if (p_ptr->depth < 1)
                continue;
            if (p_ptr->depth > 13)
                continue;
            break;
        }
        case FEAT_TRAP_GAS_MEMORY:
        {
            // removed these for now due to player frustration
            continue;

            // 14-

            // if (p_ptr->depth < 14) continue;
            // break;
        }
        case FEAT_TRAP_ALARM:
        {
            // 0-
            break;
        }
        case FEAT_TRAP_FLASH:
        {
            // 1-
            if (p_ptr->depth < 1)
                continue;
            break;
        }
        case FEAT_TRAP_CALTROPS:
        {
            // 0-
            break;
        }
        case FEAT_TRAP_ROOST:
        {
            // 0, 3-6
            if (p_ptr->depth == 0)
                break;
            if (p_ptr->depth < 3)
                continue;
            if (p_ptr->depth > 6)
                continue;
            break;
        }
        case FEAT_TRAP_WEB:
        {
            int d, dir, floor_count = 0;

            // 8-
            if (p_ptr->depth < 8)
                continue;

            // make sure there are at least two adjacent floor squares
            for (d = 0; d < 8; d++)
            {
                dir = cycle[d];

                if (cave_floor_bold(y + ddy[dir], x + ddx[dir]))
                    floor_count++;
            }
            if (floor_count < 2)
                continue;

            break;
        }
        case FEAT_TRAP_DEADFALL:
        {
            // 0, 14-
            if (p_ptr->depth == 0)
                break;
            if (p_ptr->depth < 14)
                continue;
            break;
        }
        case FEAT_TRAP_ACID:
        {
            // 1-
            if (p_ptr->depth < 1)
                continue;
            break;
        }
        case FEAT_TRAP_IMPRISONMENT:
        {
            // 6-
            if (p_ptr->depth < 6)
                continue;

            // skip half the time as they are otherwise too common
            if (one_in_(2))
                continue;

            break;
        }
        }

        /* Done */
        break;
    }

    /* Activate the trap */
    cave_set_feat(y, x, feat);

    // Hide the trap
    cave_info[y][x] |= (CAVE_HIDDEN);
}

/*
 *  Reveal a trap and mark its location on the map.
 */
void reveal_trap(int y, int x)
{
    // remove the 'hidden' flag from the grid
    cave_info[y][x] &= ~(CAVE_HIDDEN);

    /* Notice/Redraw */
    if (character_dungeon)
    {
        /* Notice */
        note_spot(y, x);

        /* Hack -- Memorize */
        cave_info[y][x] |= (CAVE_MARK);

        /* Redraw */
        lite_spot(y, x);
    }
}

/*
 * Place a secret door at the given location
 */
void place_secret_door(int y, int x)
{
    /* Create secret door */
    cave_set_feat(y, x, FEAT_SECRET);
}

/*
 * Place a random type of closed door at the given location.
 */
void place_closed_door(int y, int x)
{
    int tmp, power;

    /* Choose an object */
    tmp = rand_int(100);

    // vault generation
    if (cave_info[y][x] & (CAVE_ICKY))
    {
        /* Closed doors (88%) */
        if (tmp < 88)
        {
            /* Create closed door */
            cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x00);
        }

        /* Locked doors (8%) */
        else if (tmp < 96)
        {
            /* Create locked door */
            power = (10 + p_ptr->depth + dieroll(15)) / 5;
            power = MIN(7, power);
            cave_set_feat(y, x, FEAT_DOOR_HEAD + power);
        }

        /* Jammed doors (4%) */
        else
        {
            /* Create jammed door */
            power = (10 + p_ptr->depth + dieroll(15)) / 5;
            power = MIN(7, power);
            cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x08 + power);
        }
    }

    // normal generation
    else
    {
        /* Closed doors (75%) */
        if (tmp < 75)
        {
            /* Create closed door */
            cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x00);
        }

        /* Locked doors (24%) */
        else if (tmp < 99)
        {
            /* Create locked door */
            power = (p_ptr->depth + dieroll(15)) / 5;
            power = MIN(7, power);
            cave_set_feat(y, x, FEAT_DOOR_HEAD + power);
        }

        /* Stuck doors (1%) */
        else
        {
            /* Create jammed door */
            power = (p_ptr->depth + dieroll(15)) / 5;
            power = MIN(7, power);
            cave_set_feat(y, x, FEAT_DOOR_HEAD + 0x08 + power);
        }
    }
}

/*
 * Place a random type of door at the given location.
 */
void place_random_door(int y, int x)
{
    int tmp;

    /* Choose an object */
    tmp = rand_int(60 + p_ptr->depth);

    /* Open doors */
    if (tmp < 20)
    {
        /* Create open door */
        cave_set_feat(y, x, FEAT_OPEN);
    }

    /* Closed, locked, or stuck doors */
    else if (tmp < 60)
    {
        /* Create closed door */
        place_closed_door(y, x);
    }

    /* Secret doors */
    else
    {
        /* Create secret door */
        cave_set_feat(y, x, FEAT_SECRET);
    }
}

/*
 * Place a random type of forge at the given location.
 */
void place_forge(int y, int x)
{
    int uses, power, p, effective_depth, i;

    effective_depth = p_ptr->depth;

    if (cave_info[y][x] & (CAVE_G_VAULT))
    {
        effective_depth *= 2;
    }

    power = 1;

    // roll once per level of depth and keep the best roll
    for (i = 0; i < effective_depth; i++)
    {
        p = dieroll(1000);

        power = MAX(power, p);
    }

    uses = 2 + damroll(1, 2);

    // to prevent start-scumming on the initial forge
    if (p_ptr->depth <= 2)
    {
        uses = 3;
        power = 0;
    }

    // unique forge
    if ((power >= 1000) && !p_ptr->unique_forge_made)
    {
        uses = 3;
        cave_set_feat(y, x, FEAT_FORGE_UNIQUE_HEAD + uses);

        p_ptr->unique_forge_made = true;

        if (cheat_room)
            msg_print("Orodruth.");
    }

    // enchanted forge
    else if (power >= 990)
    {
        cave_set_feat(y, x, FEAT_FORGE_GOOD_HEAD + uses);
        if (cheat_room)
            msg_print("Enchanted forge.");
    }

    // normal forge
    else
    {
        cave_set_feat(y, x, FEAT_FORGE_NORMAL_HEAD + uses);
        if (cheat_room)
            msg_print("Forge.");
    }
}

/*
 * Describe the charges on an item in the inventory.
 */
