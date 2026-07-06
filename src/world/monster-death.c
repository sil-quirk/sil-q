#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"

/*
 *  Choose the location of a random staircase on the level
 */
bool random_stair_location(int* sy, int* sx)
{
    int stair_y[100];
    int stair_x[100];
    int stair_num = 0;
    int y, x;

    // Note all the stairs
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (cave_stair_bold(y, x))
            {
                stair_y[stair_num] = y;
                stair_x[stair_num] = x;
                if (stair_num < 99)
                    stair_num++;
            }
        }
    }

    // If no valid stairs are found, then bail out (paranoia)
    if (stair_num == 0)
    {
        return (false);
    }

    // Choose a random stair
    stair_num = rand_int(stair_num);
    *sy = stair_y[stair_num];
    *sx = stair_x[stair_num];

    return (true);
}

/*
 * Break the truce in Morgoth's throne room
 */
extern void break_truce(bool obvious)
{
    int i;

    monster_type* m_ptr = NULL; // default to soothe compiler warnings

    char m_name[80];

    if (p_ptr->truce)
    {
        /* Scan all other monsters */
        for (i = mon_max - 1; i >= 1; i--)
        {
            /* Access the monster */
            m_ptr = &mon_list[i];

            /* Ignore dead monsters */
            if (!m_ptr->r_idx)
                continue;

            // Ignore monsters out of line of sight
            if (!los(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px))
                continue;

            // Ignore unalert monsters
            if (m_ptr->alertness < ALERTNESS_ALERT)
                continue;

            /* Get the monster name (using 'something' for hidden creatures) */
            monster_desc(m_name, sizeof(m_name), m_ptr, 0x04);

            p_ptr->truce = false;
        }

        if (obvious)
            p_ptr->truce = false;

        if (!p_ptr->truce)
        {
            if (!obvious)
            {
                msg_format(
                    "%^s lets out a cry! The tension is broken.", m_name);

                /* Make a lot of noise */
                update_flow(m_ptr->fy, m_ptr->fx, FLOW_MONSTER_NOISE);
                monster_perception(false, false, -10);
            }
            else
            {
                msg_print("The tension is broken.");
            }

            /* Scan all other monsters */
            for (i = mon_max - 1; i >= 1; i--)
            {
                /* Access the monster */
                m_ptr = &mon_list[i];

                /* Ignore dead monsters */
                if (!m_ptr->r_idx)
                    continue;

                /* Mark minimum desired range for recalculation */
                m_ptr->min_range = 0;
            }
        }
    }
}

/*
 * Checks whether monsters on two separate coordinates are of the same type
 * (i.e. the same letter or share an RF3_ race flag)
 */
bool similar_monsters(int m1y, int m1x, int m2y, int m2x)
{
    monster_type* m_ptr;
    monster_race* r_ptr;
    monster_type* n_ptr;
    monster_race* nr_ptr;

    /*first check if there are monsters on both coordinates*/
    if (!(cave_m_idx[m1y][m1x] > 0))
        return (false);
    if (!(cave_m_idx[m2y][m2x] > 0))
        return (false);

    /* Access monster 1*/
    m_ptr = &mon_list[cave_m_idx[m1y][m1x]];
    r_ptr = &r_info[m_ptr->r_idx];

    /* Access monster 2*/
    n_ptr = &mon_list[cave_m_idx[m2y][m2x]];
    nr_ptr = &r_info[n_ptr->r_idx];

    /* Monsters have the same symbol */
    if (r_ptr->d_char == nr_ptr->d_char)
        return (true);

    /*
     * Same race (we are not checking all RF3 types
     * because that would be true at
     * the symbol check
     */
    if ((r_ptr->flags3 & (RF3_DRAGON)) && (nr_ptr->flags3 & (RF3_DRAGON)))
        return (true);
    if ((r_ptr->flags3 & (RF3_SERPENT)) && (nr_ptr->flags3 & (RF3_SERPENT)))
        return (true);
    if ((r_ptr->flags3 & (RF3_HORROR)) && (nr_ptr->flags3 & (RF3_HORROR)))
        return (true);

    /*Not the same*/
    return (false);
}

/*
 *  Cause a temporary penalty to morale in monsters of the same type who can see
 * the specified monster. (Used when it dies and for cruel blow).
 */
void scare_onlooking_friends(const monster_type* m_ptr, int amount)
{
    int i;
    int fy, fx, y, x;

    /* Location of main monster */
    fy = m_ptr->fy;
    fx = m_ptr->fx;

    /* Scan monsters */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* n_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[n_ptr->r_idx];

        /* Skip dead monsters */
        if (!n_ptr->r_idx)
            continue;

        /* Location of other monster */
        y = n_ptr->fy;
        x = n_ptr->fx;

        // Only consider alert monsters of the same type in line of sight
        if ((n_ptr->alertness >= ALERTNESS_ALERT)
            && !(r_ptr->flags3 & (RF3_NO_FEAR))
            && similar_monsters(fy, fx, y, x) && los(y, x, fy, fx))
        {
            // cause a temporary morale penalty
            n_ptr->tmp_morale += amount;
        }
    }

    return;
}

/*
 * Create a chosen artefact (mainly for the death of a particular unique)
 */
static u32b monster_drop_source_ident(const monster_race* r_ptr)
{
    u32b source_ident = 0;

    if (!r_ptr)
        return 0;

    if (r_ptr->flags3 & RF3_DRAGON)
        source_ident |= IDENT_DRAGON_DROP;
    if (r_ptr->flags1 & RF1_UNIQUE)
        source_ident |= IDENT_UNIQUE_DROP;

    return source_ident;
}

static void create_chosen_artefact_marked(
    byte name1, int y, int x, bool identify, u32b source_ident)
{
    object_type* i_ptr;
    object_type object_type_body;
    artefact_type* a_ptr = &a_info[name1];

    log_trace("create_chosen_artefact: Creating artifact %d at (%d,%d), identify=%s", name1, y, x, identify ? "true" : "false");

    // Don't generate it if one has already been generated
    if (a_ptr->cur_num > 0) {
        log_trace("create_chosen_artefact: Artifact %d already exists (cur_num=%d)", name1, a_ptr->cur_num);
        return;
    }

    // Don't generate it if it's reserved for Tulkas quest (unless this IS the quest reward)
    if (valar_reserved_artifacts && valar_reserved_artifacts[name1] &&
        p_ptr->tulkas_quest != TULKAS_QUEST_COMPLETE) {
        log_trace("create_chosen_artefact: Artifact %d is reserved for Tulkas quest", name1);
        return;
    }

    // Don't generate it in no-artefact games, with one obvious exception
    if (birth_no_artefacts && (name1 != ART_MORGOTH_3)) {
        log_trace("create_chosen_artefact: Skipping artifact %d due to no-artifacts birth option", name1);
        return;
    }

    log_trace("create_chosen_artefact: All checks passed, creating artifact %d", name1);

    /* Get local object */
    i_ptr = &object_type_body;

    log_trace("create_chosen_artefact: Preparing base object for artifact %d (tval=%d, sval=%d)", name1, a_ptr->tval, a_ptr->sval);

    /* Mega-Hack -- Prepare the base object for the artefact */
    object_prep(i_ptr, lookup_kind(a_ptr->tval, a_ptr->sval));

    log_trace("create_chosen_artefact: Base object prepared, applying artifact magic");

    /* Mega-Hack -- Mark this item as the artefact */
    i_ptr->name1 = name1;

    /* Mega-Hack -- Actually create the artefact */
    apply_magic(i_ptr, -1, true, true, true, true);

    if (!character_dungeon)
        source_ident |= IDENT_HOARD_DROP;
    i_ptr->ident |= (source_ident & (IDENT_DRAGON_DROP | IDENT_UNIQUE_DROP));
    i_ptr->ident |= (source_ident & IDENT_HOARD_DROP);

    log_trace("create_chosen_artefact: Magic applied successfully");

    // Identify it if desired
    if (identify)
    {
        log_trace("create_chosen_artefact: Identifying artifact %d", name1);
        object_aware(i_ptr);
        object_known(i_ptr);
        log_trace("create_chosen_artefact: Artifact identified");
    }

    log_trace("create_chosen_artefact: About to drop artifact %d at (%d,%d)", name1, y, x);

    /* Drop it in the dungeon */
    drop_near(i_ptr, -1, y, x);

    log_trace("create_chosen_artefact: Successfully created and dropped artifact %d", name1);
}

extern void create_chosen_artefact(byte name1, int y, int x, bool identify)
{
    create_chosen_artefact_marked(name1, y, x, identify, 0);
}

/*
 * Drops the objects
 */
int drop_loot(monster_type* m_ptr)
{
    int j, y, x;

    int dump_item = 0;

    int number = 0;

    s16b this_o_idx, next_o_idx = 0;

    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    bool visible = (m_ptr->ml || (r_ptr->flags1 & (RF1_UNIQUE)));

    bool chest = (r_ptr->flags1 & (RF1_DROP_CHEST)) ? true : false;
    bool good = false;
    bool great = false;
    bool superb = false;
    bool artefact = false;

    object_type* i_ptr;
    object_type object_type_body;
    u32b source_ident = monster_drop_source_ident(r_ptr);

    int original_object_level = object_level;

    if (r_ptr->flags1 & (RF1_DROP_GOOD))
    {
        good = true;
    }
    if (r_ptr->flags1 & (RF1_DROP_GREAT))
    {
        great = true;
    }
    if (r_ptr->flags2 & (RF2_DROP_SUPERB))
    {
        superb = true;
    }
    if (r_ptr->flags3 & (RF3_DROP_ARTEFACT))
    {
        artefact = true;
    }

    /* Get the location */
    y = m_ptr->fy;
    x = m_ptr->fx;

    /* Stone creatures turn into rubble */
    if ((r_ptr->flags3 & (RF3_STONE)) && !cave_stair_bold(y, x))
    {
        cave_set_feat(y, x, FEAT_RUBBLE);
    }

    /* Drop objects being carried */
    for (this_o_idx = m_ptr->hold_o_idx; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr;

        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /*Remove the mark to hide when monsters carry this object*/
        o_ptr->ident &= ~(IDENT_HIDE_CARRY);

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Paranoia */
        o_ptr->held_m_idx = 0;

        /* Get local object */
        i_ptr = &object_type_body;

        /* Copy the object */
        object_copy(i_ptr, o_ptr);
        i_ptr->ident |= source_ident;

        /* Delete the object */
        delete_object_idx(this_o_idx);

        /* Drop it */
        drop_near(i_ptr, -1, y, x);
    }

    /* Forget objects */
    m_ptr->hold_o_idx = 0;

    /* Mega-Hack -- drop special treasures */
    if (r_ptr->flags1 & (RF1_DROP_CHOSEN))
    {
        /* Drop Morgoth's treasures */
        if (m_ptr->r_idx == R_IDX_MORGOTH)
        {
            // create the Massive Hammer 'Grond'
            create_chosen_artefact_marked(ART_GROND, y, x, true, source_ident);

            // create the Iron Crown of Morgoth
            create_chosen_artefact_marked(
                ART_MORGOTH_3, y, x, true, source_ident);
        }
        // Drop Calris from Gothmog
        else if (m_ptr->r_idx == R_IDX_GOTHMOG)
        {
            // create the Greatsword 'Calris'
            create_chosen_artefact_marked(
                ART_CALRIS, y, x, false, source_ident);
        }
        // Drop Galvorn Armour of Maeglin
        else if (r_ptr->d_char == '@')
        {
            // create the Armour of Maeglin
            create_chosen_artefact_marked(
                ART_MAEGLIN, y, x, false, source_ident);
        }
        // Drop Iron Spear of Boldog
        else if (r_ptr->d_char == 'o')
        {
            // create the Armour of Maeglin
            create_chosen_artefact_marked(
                ART_BOLDOG, y, x, false, source_ident);
        }
        // Drop Glend
        else if (r_ptr->d_char == 'G')
        {
            // create the Greatsword 'Glend'
            create_chosen_artefact_marked(
                ART_GLEND, y, x, false, source_ident);
        }
        // Drop Wolf-Hame of Drauglin
        else if (r_ptr->d_char == 'C')
        {
            // create the Wolf-Hame of Draugluin
            create_chosen_artefact_marked(
                ART_DRAUGLUIN, y, x, false, source_ident);
        }
        // Drop Bat-Fell of Thuringwethil
        else if (r_ptr->d_char == 'v')
        {
            // create the Bet-Fell of Thuringwethil
            create_chosen_artefact_marked(
                ART_THURINGWETHIL, y, x, false, source_ident);
        }
    }

    /* Determine how much we can drop */
    if ((r_ptr->flags1 & (RF1_DROP_33)) && percent_chance(33))
        number++;
    if (r_ptr->flags1 & (RF1_DROP_100))
        number += 1;
    if (r_ptr->flags1 & (RF1_DROP_1D2))
        number += damroll(1, 2);
    if (r_ptr->flags1 & (RF1_DROP_2D2))
        number += damroll(2, 2);
    if (r_ptr->flags1 & (RF1_DROP_3D2))
        number += damroll(3, 2);
    if (r_ptr->flags1 & (RF1_DROP_4D2))
        number += damroll(4, 2);
    if (r_ptr->flags3 & (RF3_DROP_1D3))
        number += damroll(1, 3);

    /* DROP_ARTEFACT must always yield at least one drop slot. */
    if (artefact && number < 1)
        number = 1;

    // Favoured drops 1: arrows from archers
    if ((number > 0)
        && ((r_ptr->flags4 & (RF4_ARROW1)) || (r_ptr->flags4 & (RF4_ARROW2)))
        && percent_chance(r_ptr->freq_ranged / 2))
    {
        int depth_cap = player_generation_depth();
        int gen_depth = MIN(r_ptr->level, depth_cap);
        drop_profile arrow_profile;

        /* Get local object */
        i_ptr = &object_type_body;

        /* Wipe the object */
        object_wipe(i_ptr);

        drop_profile_default(&arrow_profile);
        arrow_profile.weight_weapon = 0;
        arrow_profile.weight_armor = 0;
        arrow_profile.weight_jewelry = 0;
        arrow_profile.weight_supply = 100;
        arrow_profile.supply_potion = 0;
        arrow_profile.supply_herb = 0;
        arrow_profile.supply_gem = 0;
        arrow_profile.supply_staff = 0;
        arrow_profile.supply_light = 0;
        arrow_profile.supply_arrows = 100;
        arrow_profile.supply_tunneling = 0;

        if (!drop_generate_object_profiled(gen_depth, DROP_QUALITY_NORMAL,
                DROP_TYPE_NOT_DAMAGED, 0, false, &arrow_profile, i_ptr))
        {
            s16b k_idx = lookup_kind(TV_ARROW, SV_NORMAL_ARROW);
            object_prep(i_ptr, k_idx);
        }
        else if (i_ptr->tval != TV_ARROW)
        {
            s16b k_idx = lookup_kind(TV_ARROW, SV_NORMAL_ARROW);
            object_prep(i_ptr, k_idx);
        }

        i_ptr->number = damroll(2, 8);

        object_known(i_ptr);
        i_ptr->ident |= source_ident;

        /* Assume seen XXX XXX XXX */
        dump_item++;

        /* Drop it in the dungeon */
        drop_near(i_ptr, -1, y, x);

        // Drop one fewer object to compensate
        number--;
    }

    // Favoured drops 2: torches from easterlings
    if ((number > 0) && (r_ptr->d_char == '@') && (r_ptr->light == 1)
        && (easter_time() || one_in_(3)))
    {
        /* Get local object */
        i_ptr = &object_type_body;

        /* Wipe the object */
        object_wipe(i_ptr);

        // Special treat for Easter time
        if (easter_time())
        {
            /* Hack	-- Give the player an object */
            /* Get the object_kind */
            s16b k_idx = lookup_kind(TV_FOOD, rand_int(9));

            /* Prepare the item */
            object_prep(i_ptr, k_idx);
        }

        // Normally just go for a torch
        else
        {
            /* Use unified light-source drop logic (A: schedule gating + min-depth penalty). */
            int depth_cap = player_generation_depth();
            int gen_depth = MIN(r_ptr->level, depth_cap);
            if (!drop_generate_object(gen_depth, DROP_QUALITY_NORMAL, DROP_TYPE_TORCHES,
                    false, i_ptr))
            {
                /* Fallback: always try to give a basic torch */
                s16b k_idx = lookup_kind(TV_LIGHT, SV_LIGHT_TORCH);
                object_prep(i_ptr, k_idx);
                apply_magic(i_ptr, gen_depth, false, false, false, false);
            }
        }

        i_ptr->ident |= source_ident;

        /* Assume seen XXX XXX XXX */
        dump_item++;

        /* Drop it in the dungeon */
        drop_near(i_ptr, -1, y, x);

        // Drop one fewer object to compensate
        number--;
    }

    /* Use the monster's level, but cap to dungeon depth so A: schedule gates
     * are enforced by the current level (prevents early lantern/jewel drops). */
    int depth_cap = player_generation_depth();
    object_level = MIN(r_ptr->level, depth_cap);
    drop_quality quality = artefact ? DROP_QUALITY_ARTEFACT
                                    : drop_quality_from_flags(good, great, superb);
    level_partition_kind part_kind = level_partition_kind_for_point(y, x);
    drop_profile monster_profile;
    int normal_drop_type;
    drop_profile_for_partition_kind_source(
        part_kind, PARTITION_DROP_SOURCE_MONSTER, &monster_profile);
    normal_drop_type = monster_profile.allow_damaged
        ? DROP_TYPE_UNTHEMED
        : DROP_TYPE_NOT_DAMAGED;

    byte old_gen_mode = object_generation_mode;
    object_generation_mode = OB_GEN_MODE_MONSTER_DROP;

    /* Drop some objects */
    for (j = 0; j < number; j++)
    {
        /* Get local object */
        i_ptr = &object_type_body;

        /* Wipe the object */
        object_wipe(i_ptr);

        if (artefact && (j == 0))
        {
            if (!make_guaranteed_artefact_with_profile(
                    i_ptr, quality, normal_drop_type, &monster_profile))
            {
                log_warn(
                    "drop_loot: DROP_ARTEFACT failed to find an eligible artefact for '%s' (r_idx=%d, depth=%d); falling back to normal loot",
                    r_name + r_ptr->name, m_ptr->r_idx, p_ptr ? p_ptr->depth : 0);

                /* Only fall back once the legal artefact pool is exhausted. */
                if (chest)
                {
                    if (!make_object_with_profile(
                            i_ptr, quality, DROP_TYPE_CHEST, &monster_profile))
                        continue;
                    if (i_ptr->tval == TV_CHEST)
                    {
                        i_ptr->xtra1 =
                            (byte)(0x80 | (byte)level_partition_kind_for_point(y, x));
                    }
                }
                else if (!make_object_with_profile(
                             i_ptr, quality, normal_drop_type,
                             &monster_profile))
                {
                    continue;
                }
            }
        }
        else if (chest)
        {
            if (!make_object_with_profile(
                    i_ptr, quality, DROP_TYPE_CHEST, &monster_profile))
                continue;
            if (i_ptr->tval == TV_CHEST)
                i_ptr->xtra1 = (byte)(0x80 | (byte)level_partition_kind_for_point(y, x));
        }

        /* Make an object */
        else if (!make_object_with_profile(
                     i_ptr, quality, normal_drop_type, &monster_profile))
            continue;

        i_ptr->ident |= source_ident;

        /* Assume seen XXX XXX XXX */
        dump_item++;

        /* Drop it in the dungeon */
        drop_near(i_ptr, -1, y, x);
    }

    object_generation_mode = old_gen_mode;

    /* Reset the object level */
    object_level = original_object_level;

    /* Take note of any dropped treasure */
    if (visible && (dump_item))
    {
        /* Take notes on treasure */
        lore_treasure(cave_m_idx[m_ptr->fy][m_ptr->fx], dump_item);
    }

    return dump_item;
}

static int drop_orcish_liquor(monster_type* m_ptr)
{
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;
    s16b k_idx;

    if (!(r_ptr->flags3 & (RF3_ORC)) || !percent_chance(10))
        return 0;

    k_idx = lookup_kind(TV_POTION, SV_POTION_ORCISH_LIQUOR);
    if (!k_idx)
    {
        log_warn("drop_orcish_liquor: missing Orcish Liquor kind");
        return 0;
    }

    object_wipe(i_ptr);
    object_prep(i_ptr, k_idx);
    drop_near(i_ptr, -1, m_ptr->fy, m_ptr->fx);
    return 1;
}

static const char morgoth_second_wind_text[][100]
    = { { "I am the Elder King: Melkor, first and mightiest of all the Valar," },
        { "who was before the world, and made it." },
        { "The shadow of my purpose lies upon Arda, and all that is in it" },
        { "bends slowly and surely to my will." },
        { "Think not that I shall go down so easily, nor that your stroke is the end." },

        { "" } };

static void morgoth_second_wind_message(void)
{
    flush();
    pause_with_text(morgoth_second_wind_text, 4, 8, NULL, 0);
}

/*
 * Makes Morgoth progressively more dangerous.
 */
void anger_morgoth(int level)
{
    monster_race* r_ptr = &r_info[R_IDX_MORGOTH];

    log_debug("anger_morgoth: called with level=%d, current morgoth_state=%d",
              level, p_ptr->morgoth_state);

    if (p_ptr->morgoth_state >= level)
    {
        log_debug("anger_morgoth: no change, already at or above level %d", level);
        return;
    }

    log_debug("anger_morgoth: transitioning from state %d to state %d",
              p_ptr->morgoth_state, level);
    log_debug("anger_morgoth: BEFORE - att=%d dd=%dd%d evn=%d pd=%d wil=%d per=%d light=%d",
              r_ptr->blow[0].att, r_ptr->blow[0].dd, r_ptr->blow[0].ds,
              r_ptr->evn, r_ptr->pd, r_ptr->wil, r_ptr->per, r_ptr->light);

    /* Apply all changes cumulatively up to the target level */
    /* This ensures stats are correct even when skipping intermediate states */

    /* State 0: Base values (for reference) */
    if (level >= 0)
    {
        r_ptr->evn = 20;
        r_ptr->blow[0].att = 20;
        r_ptr->blow[0].dd = 6; /* 6d10 */
        r_ptr->pd = 5;        /* 5d4 */
        r_ptr->wil = 25;
        r_ptr->per = 10;
        r_ptr->light = 7;
    }

    /* State 1: Crown lost */
    if (level >= 1)
    {
        r_ptr->evn = 25;
        r_ptr->pd = 6;        /* 6d4 */
        r_ptr->light = 0;
        r_ptr->per = 15;
        log_debug("anger_morgoth: applying state 1 changes - crown lost");
    }

    /* State 2: Hurt or 1st Silmaril stolen */
    if (level >= 2)
    {
        r_ptr->blow[0].att = 30;
        r_ptr->blow[0].dd = 7; /* 7d10 */
        r_ptr->wil = 30;
        r_ptr->per = 20;
        r_ptr->evn = 30;
        r_ptr->pd = 7;        /* 7d4 */
        log_debug("anger_morgoth: applying state 2 changes - hurt or silmaril");
    }

    /* State 3: Furious or 2nd Silmaril stolen */
    if (level >= 3)
    {
        r_ptr->pd = 8;        /* 8d4 */
        r_ptr->evn = 35;
        r_ptr->wil = 35;
        r_ptr->per = 25;
        log_debug("anger_morgoth: applying state 3 changes - badly hurt/furious");
    }

    /* State 4: Apoplectic or 3rd Silmaril stolen */
    if (level >= 4)
    {
        r_ptr->evn = 40;
        r_ptr->pd = 9;        /* 9d4 */
        r_ptr->blow[0].att = 40;
        r_ptr->blow[0].dd = 8; /* 8d10 */
        r_ptr->wil = 40;
        r_ptr->per = 30;
        log_debug("anger_morgoth: applying state 4 changes - desperate/apoplectic");
    }

    /* State 5: Desperate (final) */
    if (level >= 5)
    {
        r_ptr->evn = 60;
        r_ptr->pd = 12;
        r_ptr->ps = 5;      /* 12d5 */
        r_ptr->blow[0].att = 60;
        r_ptr->blow[0].dd = 10;
        r_ptr->blow[0].ds = 10; /* 10d10 */
        r_ptr->wil = 50;
        r_ptr->per = 40;
        log_debug("anger_morgoth: applying state 5 changes - final desperate");
    }

    /* State 6: God (final desperate, but will and perception maxed) */
    if (level >= 6)
    {
        r_ptr->wil = 100;
        r_ptr->per = 100;
        log_debug("anger_morgoth: applying state 6 changes - god");
    }

    p_ptr->morgoth_state = level;

    log_debug("anger_morgoth: AFTER - att=%d dd=%dd%d evn=%d pd=%d wil=%d per=%d light=%d",
              r_ptr->blow[0].att, r_ptr->blow[0].dd, r_ptr->blow[0].ds,
              r_ptr->evn, r_ptr->pd, r_ptr->wil, r_ptr->per, r_ptr->light);
    log_debug("anger_morgoth: state successfully changed to %d", p_ptr->morgoth_state);
}

static int morgoth_hp_based_state(const monster_type* m_ptr)
{
    long hp;
    long maxhp;

    if (m_ptr == NULL)
        return 0;

    hp = (long)m_ptr->hp;
    maxhp = (long)m_ptr->maxhp;

    if (hp <= 0 || maxhp <= 0)
        return 0;

    /* HP thresholds (inclusive): 80/60/40/20% -> states 2/3/4/5 */
    if ((hp * 100L) <= (maxhp * 20L))
        return 5;
    if ((hp * 100L) <= (maxhp * 40L))
        return 4;
    if ((hp * 100L) <= (maxhp * 60L))
        return 3;
    if ((hp * 100L) <= (maxhp * 80L))
        return 2;

    return 0;
}

void maybe_update_morgoth_state_from_hp(monster_type* m_ptr)
{
    int target_state;

    if (m_ptr == NULL)
        return;
    if (m_ptr->r_idx != R_IDX_MORGOTH)
        return;

    target_state = morgoth_hp_based_state(m_ptr);
    if (target_state <= 0)
        return;

    if (target_state <= p_ptr->morgoth_state)
        return;

    switch (target_state)
    {
    case 2:
        msg_print("Morgoth grows angry.");
        break;
    case 3:
        msg_print("Morgoth grows furious.");
        break;
    case 4:
        msg_print("Morgoth grows apoplectic.");
        break;
    case 5:
        msg_print("Morgoth grows desperate.");
        break;
    default:
        break;
    }

    message_flush();
    anger_morgoth(target_state);
}

bool morgoth_enter_final_stage(int m_idx)
{
    monster_type* m_ptr;
    int restored;

    if ((m_idx <= 0) || (m_idx >= mon_max))
        return (false);

    m_ptr = &mon_list[m_idx];

    if (m_ptr->r_idx != R_IDX_MORGOTH)
        return (false);

    if (p_ptr->morgoth_second_wind)
        return (false);

    restored = (int)((long)m_ptr->maxhp * 20L / 100L);
    if (restored < 1)
        restored = 1;

    m_ptr->hp = restored;
    p_ptr->morgoth_second_wind = 1;

    if (p_ptr->health_who == m_idx)
        p_ptr->redraw |= (PR_HEALTHBAR);
    if (m_ptr->ml
        && (styled_monster_health_bars || styled_monster_tile_health_bars))
    {
        if (styled_monster_health_bars)
        {
            p_ptr->window |= PW_MONLIST;
            if (p_ptr->health_who == m_idx)
                p_ptr->window |= PW_MONSTER;
        }
        if (styled_monster_tile_health_bars)
            lite_spot(m_ptr->fy, m_ptr->fx);
    }

    log_info("Morgoth entered final stage at %d/%d HP.",
             m_ptr->hp, m_ptr->maxhp);
    anger_morgoth(6);
    morgoth_second_wind_message();
    set_alertness(m_ptr, ALERTNESS_VERY_ALERT);
    m_ptr->mflag |= (MFLAG_ACTV);
    m_ptr->min_range = 0;

    return (true);
}

static void fail_niena_quest(void)
{
    p_ptr->niena_quest = NIENA_QUEST_FAILED;
    p_ptr->niena_level = 0;

    log_trace("Nienna quest failed after a kill (seen=%d, killed=%d)",
              p_ptr->niena_monsters_seen, p_ptr->niena_monsters_killed);

    msg_print("A long, sorrowful silence settles over you.");
    msg_print("You have taken a life and failed Nienna's mercy quest.");
}

/*
 * Handle the "death" of a monster.
 *
 * Disperse treasures centered at the monster location based on the
 * various flags contained in the monster flags fields.
 *
 * Note that only the player can induce "monster_death()" on Uniques.
 *
 * Note that monsters can now carry objects, and when a monster dies,
 * it drops all of its objects, which may disappear in crowded rooms.
 */
void monster_death(int m_idx)
{
    monster_type* m_ptr = &mon_list[m_idx];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];
    monster_lore* l_ptr = &l_list[m_ptr->r_idx];

    s32b new_exp;

    int multiplier = 1;

    /* Track monster death for Nienna mercy quest */
    if (p_ptr->niena_quest == NIENA_QUEST_ACTIVE && m_ptr->r_idx != R_IDX_NIENA) {
        p_ptr->niena_monsters_killed++;
        log_trace("Nienna quest: Monster killed (total killed=%d, seen=%d)",
                 p_ptr->niena_monsters_killed, p_ptr->niena_monsters_seen);
        fail_niena_quest();
    }

    /* Track monster death for Oromë hunting quest - global kill counting */
    if (p_ptr->orome_quest >= OROME_QUEST_ACTIVE) {
        bool target_killed = false;

        /* Check if killed monster matches any hunt target type */
        if (r_ptr->flags3 & RF3_WOLF) {
            p_ptr->orome_wolves_killed++;
            target_killed = true;
            log_trace("Oromë quest: Wolf killed (wolves=%d, spiders=%d, serpents=%d, vampires=%d)",
                     p_ptr->orome_wolves_killed, p_ptr->orome_spiders_killed,
                     p_ptr->orome_serpents_killed, p_ptr->orome_vampires_killed);
        }
        /* Spider check: exclude trivial 'hatchling' variants from the Oromë count */
        if (r_ptr->flags3 & RF3_SPIDER) {
            bool is_hatchling = false;
            if (r_ptr->name)
            {
                const char* rname = r_name + r_ptr->name;
                /* Case-insensitive substring search for "hatchling" */
                for (const char* p = rname; *p; p++)
                {
                    if (SDL_strncasecmp(p, "hatchling", 9) == 0)
                    {
                        is_hatchling = true;
                        break;
                    }
                }
            }

            if (!is_hatchling)
            {
                p_ptr->orome_spiders_killed++;
                target_killed = true;
                log_trace("Oromë quest: Spider killed (wolves=%d, spiders=%d, serpents=%d, vampires=%d)",
                         p_ptr->orome_wolves_killed, p_ptr->orome_spiders_killed,
                         p_ptr->orome_serpents_killed, p_ptr->orome_vampires_killed);
            }
            else
            {
                log_trace("Oromë quest: Spider hatchling excluded from spider count (name='%s')", (r_ptr->name ? (r_name + r_ptr->name) : "<unnamed>"));
            }
        }
        if (r_ptr->flags3 & RF3_SERPENT) {
            p_ptr->orome_serpents_killed++;
            target_killed = true;
            log_trace("Oromë quest: Serpent killed (wolves=%d, spiders=%d, serpents=%d, vampires=%d)",
                     p_ptr->orome_wolves_killed, p_ptr->orome_spiders_killed,
                     p_ptr->orome_serpents_killed, p_ptr->orome_vampires_killed);
        }
        if (r_ptr->flags3 & RF3_VAMPIRE) {
            p_ptr->orome_vampires_killed++;
            target_killed = true;
            log_trace("Oromë quest: Vampire killed (wolves=%d, spiders=%d, serpents=%d, vampires=%d)",
                     p_ptr->orome_wolves_killed, p_ptr->orome_spiders_killed,
                     p_ptr->orome_serpents_killed, p_ptr->orome_vampires_killed);
        }

        if (target_killed) {
            log_trace("Oromë quest: Hunt target monster killed, checking for completion thresholds...");
        }
    }

    /*
     *   1. General monster death things
     */

    // Special message and flag setting for killing Morgoth
    if (m_ptr->r_idx == R_IDX_MORGOTH)
    {
        p_ptr->morgoth_slain = true;
        log_info("Morgoth slain by player; initiating Morgoth victory sequence.");
        msg_print("Morgoth's form shudders, radiant fissures racing across his iron crown!");
        msg_print("From beyond the West, the Valar proclaim your impossible triumph.");
        message_flush();
        do_cmd_morgoth_victory();
    }

    /* If the player kills a Unique, write a note. */
    if (r_ptr->flags1 & RF1_UNIQUE)
    {
        char note2[120];
        char real_name[120];

        /* Get the monster's real name for the notes file */
        monster_desc_race(real_name, sizeof(real_name), m_ptr->r_idx);

        /* Write note */
        if (monster_nonliving(r_ptr))
            SDL_strlcpy(note2, format("Destroyed %s", real_name), sizeof(note2));
        else
            SDL_strlcpy(note2, format("Slew %s", real_name), sizeof(note2));

        do_cmd_note(note2, p_ptr->depth);
    }

    /* Update monster list window */
    p_ptr->window |= PW_MONLIST;

    /* Check for Tulkas quest completion */
    check_tulkas_quest_completion(m_ptr->r_idx);

    /* Check for Mandos quest completion */
    check_mandos_quest_completion(m_ptr->r_idx);

    /* Check for Varda quest completion */
    check_varda_quest_completion(m_ptr->r_idx);

    /* Check for Oromë quest completion */
    check_orome_quest_completion();

    /* Give some experience for the kill */
    new_exp = adjusted_mon_exp(r_ptr, true);
    gain_exp(new_exp);
    p_ptr->kill_exp += new_exp;

    /* Uniques stay dead */
    if (r_ptr->flags1 & (RF1_UNIQUE))
    {
        r_ptr->max_num = 0;
    }

    /* Count kills this life */
    if (l_ptr->pkills < MAX_SHORT)
        l_ptr->pkills++;

    /* Count kills in all lives */
    if (l_ptr->tkills < MAX_SHORT)
        l_ptr->tkills++;

    // since it was killed, it was definitely encountered
    if (!m_ptr->encountered)
    {
        int new_exp;

        new_exp = adjusted_mon_exp(r_ptr, false);

        // gain experience for encounter
        gain_exp(new_exp);
        p_ptr->encounter_exp += new_exp;

        // update stats
        m_ptr->encountered = true;
        l_ptr->psights++;
        if (l_ptr->tsights < MAX_SHORT)
            l_ptr->tsights++;
    }

    /*
     *   2. Lower the morale of similar monsters that can see the deed.
     */

    // double effect for creatures with escorts
    if ((r_ptr->flags1 & (RF1_ESCORT)) || (r_ptr->flags1 & (RF1_ESCORTS)))
        multiplier = 4;

    scare_onlooking_friends(m_ptr, -40 * multiplier);

    /*
     *   3. Dropping items
     */

    // monsters who fell into chasms also don't generate loot...
    if (!((cave_feat[m_ptr->fy][m_ptr->fx] == FEAT_CHASM)
            && !(r_ptr->flags2 & (RF2_FLYING))))
    {
        int normal_loot_count = 0;
        int bonus_loot_count = 0;

        // drop the normal loot for non-territorial monsters
        if (!(r_ptr->flags2 & (RF2_TERRITORIAL)))
        {
            normal_loot_count = drop_loot(m_ptr);
        }

        // Every orc has an additional independent 10% chance to drop Orcish Liquor.
        bonus_loot_count = drop_orcish_liquor(m_ptr);

        if (bonus_loot_count && (m_ptr->ml || (r_ptr->flags1 & (RF1_UNIQUE))))
        {
            lore_treasure(cave_m_idx[m_ptr->fy][m_ptr->fx],
                normal_loot_count + bonus_loot_count);
        }
    }

    return;
}

/*
 * This adjusts a monster's raw experience point value according to the number
 * killed so far The formula is:
 *
 * (depth*10) / (kills+1)
 *
 * This is doubled for uniques.
 *
 * ((depth*25) * 4) / (kills + 4)  <- previous version
 *
 * 100 90 83 76 71 66 62  (10,10)   <- earliest version?
 * 100 80 66 57 50 44 40  (4,4)     <- this is the previous version (without
 * the 1.5 multiplier) 100 66 50 40 33 28 25  (2,2) 100 50 33 25 20 16 14  (1,1)
 * <- this is the current version
 *
 * 100 90 81 72 65 59 53  (10%)     <- exponential alternatives
 * 100 80 64 51 40 32 25  (20%)
 *
 * This function is called when gaining experience and when displaying it in
 * monster recall.
 */
s32b adjusted_mon_exp(const monster_race* r_ptr, bool kill)
{
    s32b exp;
    int mexp = r_ptr->level * 10;
    int mkills = l_list[r_ptr - r_info].pkills;
    int msights = l_list[r_ptr - r_info].psights;
    int repeats = kill ? mkills : msights;

    if (kill)
    {
        if (r_ptr->flags1 & RF1_UNIQUE)
        {
            exp = mexp * 2;
        }
        else
        {
            exp = (repeats >= 30) ? 0 : (mexp >> repeats);
        }

        if (r_ptr->flags1 & RF1_PEACEFUL)
        {
            exp = 0;
        }
    }
    else
    {
        if (r_ptr->flags1 & RF1_UNIQUE)
        {
            exp = mexp * 2;
        }
        else
        {
            exp = (repeats >= 30) ? 0 : (mexp >> repeats);
        }
    }

    return (exp);
}

/*
 * Decrease a monster's hit points, handle monster death.
 *
 * We return true if the monster has been killed (and deleted).
 *
 * We announce monster death (using an optional "death message"
 * if given, and a otherwise a generic killed/destroyed message).
 *
 * Only "physical attacks" can induce the "You have slain" message.
 * Missile and Spell attacks will induce the "dies" message, or
 * various "specialized" messages.  Note that "You have destroyed"
 * and "is destroyed" are synonyms for "You have slain" and "dies".
 *
 * Invisible monsters induce a special "You have killed it." message.
 */
bool mon_take_hit(int m_idx, int dam, cptr note, int who)
{
    monster_type* m_ptr = &mon_list[m_idx];
    monster_race* r_ptr = &r_info[m_ptr->r_idx];

    /* Redraw (later) if needed */
    if (p_ptr->health_who == m_idx)
        p_ptr->redraw |= (PR_HEALTHBAR);

    /* Hurt it */
    m_ptr->hp -= dam;

    if (m_ptr->ml
        && (styled_monster_health_bars || styled_monster_tile_health_bars))
    {
        if (styled_monster_health_bars)
        {
            p_ptr->window |= PW_MONLIST;
            if (p_ptr->health_who == m_idx)
                p_ptr->window |= PW_MONSTER;
        }
        if (styled_monster_tile_health_bars)
            lite_spot(m_ptr->fy, m_ptr->fx);
    }

    if (dam > 0)
        maybe_update_morgoth_state_from_hp(m_ptr);

    /* It is dead now */
    if (m_ptr->hp <= 0)
    {
        if (m_ptr->r_idx == R_IDX_MORGOTH && !p_ptr->morgoth_second_wind)
        {
            log_info("Morgoth reached 0 HP; entering god state.");
            (void)morgoth_enter_final_stage(m_idx);

            return (false);
        }

        char m_name[80];

        /* Extract monster name */
        monster_desc(m_name, sizeof(m_name), m_ptr, 0);

        /* Death by Missile/Spell attack */
        if (note)
        {
            /* Hack -- allow message suppression */
            if (strlen(note) <= 1)
            {
                /* Be silent */
            }

            else
            {
                message_format(MSG_KILL, m_ptr->r_idx, "%^s%s", m_name, note);
            }
        }

        /* Death by physical attack -- invisible monster */
        else if (!m_ptr->ml)
        {
            // You only get messages for unseen monsters if you kill them
            if ((who < 0)
                && (distance(m_ptr->fy, m_ptr->fx, p_ptr->py, p_ptr->px) == 1))
                message_format(
                    MSG_KILL, m_ptr->r_idx, "You have killed %s.", m_name);
            // else			message_format(MSG_KILL, m_ptr->r_idx,
            // "%^s has been killed.", m_name);
        }

        /* Death by Physical attack -- non-living monster */
        else if (monster_nonliving(r_ptr))
        {
            if (who < 0)
                message_format(
                    MSG_KILL, m_ptr->r_idx, "You have destroyed %s.", m_name);
            else
                message_format(
                    MSG_KILL, m_ptr->r_idx, "%^s has been destroyed.", m_name);
        }

        /* Death by Physical attack -- living monster */
        else
        {
            if (who < 0)
                message_format(
                    MSG_KILL, m_ptr->r_idx, "You have slain %s.", m_name);
            else
                message_format(
                    MSG_KILL, m_ptr->r_idx, "%^s has been slain.", m_name);
        }

        /* Generate treasure */
        monster_death(m_idx);

        /* Auto-recall only if visible or unique */
        if (m_ptr->ml || (r_ptr->flags1 & (RF1_UNIQUE)))
        {
            monster_race_track(m_ptr->r_idx);
        }

        /* Delete the monster */
        delete_monster_idx(m_idx);

        /* Monster is dead */
        return (true);
    }

    // Wake it up if there was real damage dealt
    if (dam > 0)
    {
        int random_level = rand_range(ALERTNESS_ALERT, ALERTNESS_QUITE_ALERT);
        set_alertness(m_ptr, MAX(m_ptr->alertness + dam, random_level + dam));
    }

    /* Monster will always go active */
    m_ptr->mflag |= (MFLAG_ACTV);

    /* Recalculate desired minimum range */
    if (dam > 0)
        m_ptr->min_range = 0;

    /* Not dead yet */
    return (false);
}
