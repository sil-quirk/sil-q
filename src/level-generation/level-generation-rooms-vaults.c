/* File: level-generation-rooms-vaults.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"


int vault_drop_gate_percent(vault_drop_gate_kind kind)
{
    switch (op_ptr->vault_drop_frequency)
    {
    case VDF_PLENTIFUL:
        return 100;
    case VDF_NORMAL:
        switch (kind)
        {
        case VDG_NORMAL: return 40;
        case VDG_GOOD:   return 66;
        case VDG_GREAT:  return 100;
        case VDG_CHEST:  return 100;
        }
        break;
    case VDF_MODEST:
        switch (kind)
        {
        case VDG_NORMAL: return 20;
        case VDG_GOOD:   return 50;
        case VDG_GREAT:  return 75;
        case VDG_CHEST:  return 100;
        }
        break;
    case VDF_SCARCE:
        switch (kind)
        {
        case VDG_NORMAL: return 10;
        case VDG_GOOD:   return 25;
        case VDG_GREAT:  return 40;
        case VDG_CHEST:  return 66;
        }
        break;
    case VDF_MEAGER:
        switch (kind)
        {
        case VDG_NORMAL: return 0;
        case VDG_GOOD:   return 10;
        case VDG_GREAT:  return 20;
        case VDG_CHEST:  return 33;
        }
        break;
    }

    return 100;
}

bool vault_drop_passes(vault_drop_gate_kind kind)
{
    int chance = vault_drop_gate_percent(kind);

    if (chance <= 0)
        return false;
    if (chance >= 100)
        return true;

    return percent_chance(chance);
}

/*
 * Hack -- fill in "vault" rooms
 */
bool build_vault(int y0, int x0, vault_type* v_ptr, bool flip_d)
{
    int ymax = v_ptr->hgt;
    int xmax = v_ptr->wid;
    cptr data = v_text + v_ptr->text;
    int dx, dy, x, y;
    int ax, ay;
    bool flip_v = false;
    bool flip_h = false;
    int multiplier;

    int original_monster_level = monster_level;

    log_trace("build_vault: Building vault '%s' with color=%d at center (%d,%d), size %dx%d",
              v_name + v_ptr->name, v_ptr->color, y0, x0, xmax, ymax);
    log_trace("build_vault: Vault flags = 0x%x, flip_d = %s", v_ptr->flags, flip_d ? "true" : "false");

    /* DEBUGGING: Check if this is a quest vault */
    if (v_ptr->flags & VLT_QUEST) {
        log_trace("build_vault: *** QUEST VAULT DETECTED *** Building '%s'", v_name + v_ptr->name);
    }

    cptr t;

    // Check that the vault doesn't contain invalid things for its depth
    for (t = data, dy = 0; dy < ymax; dy++)
    {
        for (dx = 0; dx < xmax; dx++, t++)
        {
            // Barrow wights can't be deeper than level 13
            if ((*t == 'W') && (p_ptr->depth > 13))
            {
                log_debug("Skipped a barrow wight vault.");
                return (false);
            }

            // chasms can't occur at 1000 ft
            if ((*t == '7') && (p_ptr->depth >= MORGOTH_DEPTH))
            {
                return (false);
            }
        }
    }

    // reflections
    if ((p_ptr->depth > 0) && (p_ptr->depth < MORGOTH_DEPTH))
    {
        // reflect it vertically half the time
        if (one_in_(2))
            flip_v = true;

        // reflect it horizontally half the time
        if (one_in_(2))
            flip_h = true;
    }

    /* Begin the vault style context now that the vault is accepted */
    styles_begin_vault(-1, 0);
    /* If vault has explicit style list, use it (support '*'=-1, '$'=-2) */
    styles_reset_vault_weights();
    if (v_ptr->style_count > 0) {
        for (int si = 0; si < v_ptr->style_count; ++si) {
            int sidx = v_ptr->style_idx[si];
            int w = v_ptr->style_weight[si];
            if (sidx == -1) {
                int lp = styles_get_level_primary_style();
                if (lp >= 0) styles_add_vault_weight(lp, w);
            } else if (sidx == -2) {
                /* '$' token: pick one random style from the current level's
                 * available list and add it with the specified weight. */
                int rs = styles_pick_random_from_level();
                if (rs >= 0) styles_add_vault_weight(rs, w);
            } else {
                styles_add_vault_weight(sidx, w);
            }
        }
    } else {
        /* No S: provided -- choose a random style from the depth-available list */
        int rs = styles_pick_random_from_level();
        if (rs >= 0) styles_add_vault_weight(rs, 1);
    }
    /* Choose one primary style for the entire vault */
    styles_select_vault_primary();
    log_debug("build_vault: level_primary=%d vault_primary=%d",
        styles_get_level_primary_style(), styles_get_vault_primary_style());

    /* Place dungeon features and objects */
    int vault_primary_sidx_for_encoding = styles_get_vault_primary_style();
    int v_min_y = 32767, v_min_x = 32767, v_max_y = -32768, v_max_x = -32768; /* track vault bbox */
    for (t = data, dy = 0; dy < ymax; dy++)
    {
        if (flip_v)
            ay = ymax - 1 - dy;
        else
            ay = dy;

        for (dx = 0; dx < xmax; dx++, t++)
        {
            if (flip_h)
                ax = xmax - 1 - dx;
            else
                ax = dx;

            /* Extract the location */
            x = x0 - (xmax / 2) + ax;
            y = y0 - (ymax / 2) + ay;

            // diagonal flipping
            if (flip_d)
            {
                x = x0 - (ymax / 2) + ay;
                y = y0 - (xmax / 2) + ax;
            }

            /* Hack -- skip "non-grids" but still advance bbox only on placed tiles */
            if (*t == ' ')
                continue;

            /* Track bbox of actual vault content */
            if (y < v_min_y) v_min_y = y;
            if (y > v_max_y) v_max_y = y;
            if (x < v_min_x) v_min_x = x;
            if (x > v_max_x) v_max_x = x;

            /* Lay down a floor, encoding the vault style and forcing first variant */
            if (vault_primary_sidx_for_encoding >= 0) {
                int enc = COLOR_STYLE_BASE + COLOR_STYLE_FLAG_FIRSTVAR + (vault_primary_sidx_for_encoding & (COLOR_STYLE_SLOT_MAX - 1));
                cave_set_feat_with_color(y, x, FEAT_FLOOR, enc);
            } else {
                cave_set_feat(y, x, FEAT_FLOOR);
            }

            /* Part of a vault */
            cave_info[y][x] |= (CAVE_ROOM | CAVE_ICKY);

            /* Analyze the grid */
            switch (*t)
            {
            /* Granite wall (outer) */
            case '$':
            {
                cave_set_feat_with_color(y, x, FEAT_WALL_OUTER, 0);
                break;
            }
            /* Granite wall (inner) */
            case '#':
            {
                cave_set_feat_with_color(y, x, FEAT_WALL_INNER, 0);
                break;
            }

            /* Quartz vein */
            case '%':
            {
                cave_set_feat_with_color(y, x, FEAT_QUARTZ, 0);
                break;
            }

            /* Rubble */
            case ':':
            {
                cave_set_feat_with_color(y, x, FEAT_RUBBLE, 0);
                break;
            }

            /* Glyph of warding */
            case ';':
            {
                cave_set_feat(y, x, FEAT_GLYPH);
                break;
            }

                /* Down staircase */
            case '>':
            {
                cave_set_feat(y, x, FEAT_MORE);
                break;
            }

            /* Up staircase */
            case '<':
            {
                cave_set_feat(y, x, FEAT_LESS);
                break;
            }

            /* Visible door */
            case '+':
            {
                place_closed_door(y, x);
                break;
            }

            /* Secret door */
            case 's':
            {
                place_secret_door(y, x);
                break;
            }

            /* Trap */
            case '^':
            {
                if (one_in_(2))
                    place_trap(y, x);
                break;
            }

            /* Forge */
            case '0':
            {
                place_forge(y, x);
                break;
            }

            /* Chasm */
            case '7':
            {
                cave_set_feat(y, x, FEAT_CHASM);
                break;
            }

            /* Sunlight */
            case ',':
            {
                cave_set_feat(y, x, FEAT_SUNLIGHT);
                break;
            }

            /* Not actually part of the vault after all */
            case ' ':
            {
                // remove room and vault flags
                cave_info[y][x] &= ~(CAVE_ROOM | CAVE_ICKY);
                break;
            }
            }
        }
    }

    /* After placement, apply a 1-tile style halo so adjacent walls/floors match the vault style.
     * Refined: do NOT recolor corridor floor tiles that sit just outside a vault door.
     * We only halo floors when adjacent to a vault wall (not a door), to keep vault
     * entrances blending into the corridor style. Doors themselves remain excluded. */
    if (v_min_y <= v_max_y && v_min_x <= v_max_x) {
        int ay0 = MAX(1, v_min_y - 1);
        int ax0 = MAX(1, v_min_x - 1);
        int ay1 = MIN(p_ptr->cur_map_hgt - 2, v_max_y + 1);
        int ax1 = MIN(p_ptr->cur_map_wid - 2, v_max_x + 1);
        for (int yy = ay0; yy <= ay1; ++yy) {
            for (int xx = ax0; xx <= ax1; ++xx) {
                /* Skip squares that are already part of the vault */
                if (cave_info[yy][xx] & (CAVE_ICKY)) continue;

                /* Only halo cells adjacent to vault content (8-directional),
                 * and classify what kind of vault neighbor it is. */
                bool near_vault_any = false;
                bool near_vault_wall = false;
                bool near_vault_door = false;
                for (int dy2 = -1; dy2 <= 1; ++dy2) {
                    for (int dx2 = -1; dx2 <= 1; ++dx2) {
                        if (dy2 == 0 && dx2 == 0) continue;
                        int ny = yy + dy2, nx = xx + dx2;
                        if (!(cave_info[ny][nx] & (CAVE_ICKY))) continue;
                        near_vault_any = true;
                        int nfeat = cave_feat[ny][nx];
                        /* Door features */
                        if (nfeat == FEAT_OPEN || nfeat == FEAT_BROKEN ||
                            (nfeat >= FEAT_DOOR_HEAD && nfeat <= FEAT_DOOR_TAIL)) {
                            near_vault_door = true;
                        }
                        /* Walls and wall-like */
                        else if ((nfeat >= FEAT_WALL_HEAD && nfeat <= FEAT_WALL_TAIL) ||
                                 nfeat == FEAT_QUARTZ || nfeat == FEAT_RUBBLE) {
                            near_vault_wall = true;
                        }
                    }
                }
                if (!near_vault_any) continue;

                int feat = cave_feat[yy][xx];
                /* Skip doors; let corridor/door visuals remain level-styled */
                if (feat == FEAT_OPEN || feat == FEAT_BROKEN ||
                    (feat >= FEAT_DOOR_HEAD && feat <= FEAT_DOOR_TAIL)) {
                    continue;
                }

                /* Apply to floors only when adjacent to vault walls and NOT adjacent to vault doors */
                if (cave_floorlike_bold(yy, xx)) {
                    if (!(near_vault_wall && !near_vault_door)) continue;
                }
                /* Apply to walls/veins/rubble regardless, to blend the boundary */
                else if ((cave_info[yy][xx] & (CAVE_WALL)) || feat == FEAT_QUARTZ || feat == FEAT_RUBBLE) {
                    /* ok */
                } else {
                    continue;
                }

                {
                    /* Re-encode color to the vault primary style, forcing first variant */
                    int sidx = styles_get_vault_primary_style();
                    if (sidx < 0) sidx = styles_get_level_primary_style();
                    int enc = COLOR_STYLE_BASE + COLOR_STYLE_FLAG_FIRSTVAR + (sidx & (COLOR_STYLE_SLOT_MAX - 1));
                    cave_set_feat_with_color(yy, xx, feat, enc);
                }
            }
        }
    }

    /* Restore level styles after vault placement */
    styles_end_vault();

    /* Place dungeon monsters and objects */
    {
    int previous_build_vault_type = current_build_vault_type;
    current_build_vault_type = v_ptr->typ;
    log_trace(
        "SPECIAL_VAULT_ONLY context enter: vault='%s' type=%d depth=%d previous_type=%d",
        v_name + v_ptr->name, v_ptr->typ, p_ptr->depth,
        previous_build_vault_type);

    for (t = data, dy = 0; dy < ymax; dy++)
    {
        if (flip_v)
            ay = ymax - 1 - dy;
        else
            ay = dy;

        for (dx = 0; dx < xmax; dx++, t++)
        {
            if (flip_h)
                ax = xmax - 1 - dx;
            else
                ax = dx;

            x = x0 - (xmax / 2) + ax;
            y = y0 - (ymax / 2) + ay;

            if (flip_d)
            {
                x = x0 - (ymax / 2) + ay;
                y = y0 - (xmax / 2) + ax;
            }

            if (*t == ' ')
                continue;

            if (is_vault_monster_token(*t))
                place_vault_monster_token(*t, y, x);
        }
    }

    for (t = data, dy = 0; dy < ymax; dy++)
    {
        if (flip_v)
            ay = ymax - 1 - dy;
        else
            ay = dy;

        for (dx = 0; dx < xmax; dx++, t++)
        {
            if (flip_h)
                ax = xmax - 1 - dx;
            else
                ax = dx;

            /* Extract the grid */
            x = x0 - (xmax / 2) + ax;
            y = y0 - (ymax / 2) + ay;

            // diagonal flipping
            if (flip_d)
            {
                x = x0 - (ymax / 2) + ay;
                y = y0 - (xmax / 2) + ax;
            }

            /* Hack -- skip "non-grids" */
            if (*t == ' ')
                continue;

            if (is_vault_monster_token(*t))
                continue;

            /* Analyze the symbol */
            switch (*t)
            {
            /* A monster from 1 level deeper */
            case '1':
            {
                monster_level = player_generation_depth() + 1;
                place_monster(y, x, true, true, true);
                monster_level = original_monster_level;
                break;
            }

            /* A monster from 2 levels deeper */
            case '2':
            {
                monster_level = player_generation_depth() + 2;
                place_monster(y, x, true, true, true);
                monster_level = original_monster_level;
                break;
            }

            /* A monster from 3 levels deeper */
            case '3':
            {
                monster_level = player_generation_depth() + 3;
                place_monster(y, x, true, true, true);
                monster_level = original_monster_level;
                break;
            }

            /* A monster from 4 levels deeper */
            case '4':
            {
                monster_level = player_generation_depth() + 4;
                place_monster(y, x, true, true, true);
                monster_level = original_monster_level;
                break;
            }

            /* An object from 1-5 levels deeper (min-depth penalty only) */
            case '*':
            {
                /* Vault loot tuning: reduce item clutter based on drop frequency setting */
                if (!vault_drop_passes(VDG_NORMAL))
                    break;

                int base_depth = player_generation_depth();
                int penalty_depth = base_depth + dieroll(5);
                partition_drop_profile active_profile =
                    partition_drop_profile_for_mode_source_cfg(
                        drop_mode_for_point(y, x), PARTITION_DROP_SOURCE_FLOOR);
                place_object_with_profile_params(
                    y, x, base_depth, penalty_depth, DROP_QUALITY_NORMAL,
                    DROP_TYPE_NOT_DAMAGED, false, 1, 0, &active_profile);
                break;
            }

            /* A good object from 1-5 levels deeper (min-depth penalty only) */
            case '&':
            {
                if (!vault_drop_passes(VDG_GOOD))
                    break;

                int base_depth = player_generation_depth();
                int penalty_depth = base_depth + dieroll(5);
                partition_drop_profile active_profile =
                    partition_drop_profile_for_mode_source_cfg(
                        drop_mode_for_point(y, x), PARTITION_DROP_SOURCE_FLOOR);
                bool old_allow_noble_from_quality = drop_allow_noble_from_quality;
                drop_allow_noble_from_quality
                    = (op_ptr->noble_item_spawn_mode == NOBLE_ITEM_SPAWN_INCLUDE_VAULTS);
                place_object_with_profile_params(
                    y, x, base_depth, penalty_depth, DROP_QUALITY_GOOD,
                    DROP_TYPE_NOT_DAMAGED, false, 1, 0, &active_profile);
                drop_allow_noble_from_quality = old_allow_noble_from_quality;
                break;
            }

            /* A great object from 1-5 levels deeper (min-depth penalty only) */
            case '!':
            {
                if (!vault_drop_passes(VDG_GREAT))
                    break;

                int base_depth = player_generation_depth();
                int penalty_depth = base_depth + dieroll(5);
                partition_drop_profile active_profile =
                    partition_drop_profile_for_mode_source_cfg(
                        drop_mode_for_point(y, x), PARTITION_DROP_SOURCE_FLOOR);
                bool old_allow_noble_from_quality = drop_allow_noble_from_quality;
                drop_allow_noble_from_quality
                    = (op_ptr->noble_item_spawn_mode == NOBLE_ITEM_SPAWN_INCLUDE_VAULTS);
                place_object_with_profile_params(
                    y, x, base_depth, penalty_depth, DROP_QUALITY_GREAT,
                    DROP_TYPE_NOT_DAMAGED, true,
                    DROP_GREAT_ARTEFACT_WEIGHT_MULTIPLIER,
                    IDENT_HOARD_DROP, &active_profile);
                drop_allow_noble_from_quality = old_allow_noble_from_quality;
                break;
            }

            /* A chest from 5 levels deeper */
            case '~':
            {
                if (!vault_drop_passes(VDG_CHEST))
                    break;

                int chest_depth = player_generation_depth() + 5;

                /* Set vault type context for chest material distribution */
                drop_set_chest_vault_type(v_ptr->typ);

                partition_drop_profile active_profile =
                    partition_drop_profile_for_mode_source_cfg(
                        drop_mode_for_point(y, x), PARTITION_DROP_SOURCE_FLOOR);
                place_object_with_profile_params(
                    y, x, chest_depth, chest_depth, DROP_QUALITY_NORMAL,
                    DROP_TYPE_CHEST, false, 1, 0, &active_profile);
                break;
            }

            /* A skeleton */
            case 'S':
            {
                object_type* i_ptr;
                object_type object_type_body;
                s16b k_idx;

                // make a skeleton 1/2 of the time
                if (one_in_(2))
                {
                    /* Get local object */
                    i_ptr = &object_type_body;

                    /* Wipe the object */
                    object_wipe(i_ptr);

                    if (one_in_(3))
                        k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_HUMAN);
                    else
                        k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_ELF);

                    /* Prepare the item */
                    object_prep(i_ptr, k_idx);

                    i_ptr->pval = 1;

                    /* Drop it in the dungeon */
                    drop_near(i_ptr, -1, y, x);
                }
                break;
            }

            /* A human skeleton */
            case 'h':
            {
                object_type* i_ptr;
                object_type object_type_body;
                s16b k_idx;

                /* Get local object */
                i_ptr = &object_type_body;

                /* Wipe the object */
                object_wipe(i_ptr);

                k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_HUMAN);

                /* Prepare the item */
                object_prep(i_ptr, k_idx);

                i_ptr->pval = 1;

                /* Drop it in the dungeon */
                drop_near(i_ptr, -1, y, x);
                break;
            }

            /* An orc skeleton */
            case 'e':
            {
                object_type* i_ptr;
                object_type object_type_body;
                s16b k_idx;

                /* Get local object */
                i_ptr = &object_type_body;

                /* Wipe the object */
                object_wipe(i_ptr);

                k_idx = lookup_kind(TV_SKELETON, SV_SKELETON_ORC);

                /* Prepare the item */
                object_prep(i_ptr, k_idx);

                i_ptr->pval = 1;

                /* Drop it in the dungeon */
                drop_near(i_ptr, -1, y, x);
                break;
            }

            /* A web */
            case 'w':
            {
                /* Place a web trap */
                cave_set_feat(y, x, FEAT_TRAP_WEB);
                break;
            }

            /* Monster and/or object from 1 level deeper */
            case '?':
            {
                int r = dieroll(3);

                if (r <= 2)
                {
                    monster_level = player_generation_depth() + 1;
                    place_monster(y, x, true, true, true);
                    monster_level = original_monster_level;
                }
                if (r >= 2)
                {
                    /* Vault loot tuning: reduce item clutter based on drop frequency setting */
                    if (!vault_drop_passes(VDG_NORMAL))
                        break;

                    int base_depth = player_generation_depth();
                    int penalty_depth = base_depth + 1;
                    partition_drop_profile active_profile =
                        partition_drop_profile_for_mode_source_cfg(
                            drop_mode_for_point(y, x), PARTITION_DROP_SOURCE_FLOOR);
                    place_object_with_profile_params(
                        y, x, base_depth, penalty_depth, DROP_QUALITY_NORMAL,
                        DROP_TYPE_UNTHEMED, false, 1, 0, &active_profile);
                }
                break;
            }

            /* Carcharoth */
            case 'C':
            {
                place_vault_monster_token('C', y, x);
                break;
            }

            /* silent watcher */
            case 'H':
            {
                place_vault_monster_token('H', y, x);
                break;
            }

            /* easterling spy */
            case '@':
            {
                place_vault_monster_token('@', y, x);
                break;
            }

            /* orc champion */
            case 'o':
            {
                place_vault_monster_token('o', y, x);
                break;
            }

            /* orc captain */
            case 'O':
            {
                place_vault_monster_token('O', y, x);
                break;
            }

            /* Tulkas Unclad */
            case 'P':
            {
                // Vault-based Tulkas spawning disabled - using room-based spawning only
                log_trace("Vault generation: Found 'P' character for Tulkas but vault spawning disabled");
                break;
            }

            case 'z':
            {
                /* Randomly spawn human or elf thrall */
                /* 5% chance for alert thrall (with quest), 95% for dejected thrall */
                int thrall_r_idx;
                if (one_in_(20))
                {
                    /* Alert thrall with quest */
                    thrall_r_idx = one_in_(2) ? R_IDX_ALERT_HUMAN_THRALL : R_IDX_ALERT_ELF_THRALL;
                }
                else
                {
                    /* Dejected thrall (no quest) */
                    thrall_r_idx = one_in_(2) ? R_IDX_HUMAN_THRALL : R_IDX_ELF_THRALL;
                }
                place_monster_one(y, x, thrall_r_idx, true, true, NULL);

                /* Initialize quest for alert thralls */
                if (thrall_r_idx == R_IDX_ALERT_HUMAN_THRALL || thrall_r_idx == R_IDX_ALERT_ELF_THRALL)
                {
                    int m_idx = cave_m_idx[y][x];
                    if (m_idx > 0)
                    {
                        init_thrall_quest(&mon_list[m_idx]);
                    }
                }
                break;
            }

            case 'Z':
            {
                place_vault_monster_token('Z', y, x);
                break;
            }

            /* cat warrior */
            case 'f':
            {
                place_vault_monster_token('f', y, x);
                break;
            }

            /* cat assassin */
            case 'F':
            {
                place_vault_monster_token('F', y, x);
                break;
            }

            /* troll guard */
            case 'T':
            {
                place_vault_monster_token('T', y, x);
                break;
            }

            /* Troll (any monster with RF3_TROLL) */
            case 't':
            {
                place_monster_by_flag(
                    y, x, 3, RF3_TROLL, true,
                    player_generation_depth() + rand_range(1, 4));
                break;
            }

            /* The Mail Corslet of Durin (INSTA_ART; vault-only) */
            case 'u':
            {
                create_chosen_artefact(ART_DURIN, y, x, false);
                break;
            }

            /* barrow wight */
            case 'W':
            {
                place_vault_monster_token('W', y, x);
                break;
            }

            /* dragon */
            case 'd':
            {
                place_monster_by_flag(
                    y, x, 3, RF3_DRAGON, true, player_generation_depth() + 4);
                break;
            }

            /* young cold drake */
            case 'y':
            {
                place_vault_monster_token('y', y, x);
                break;
            }

            /* young fire drake */
            case 'Y':
            {
                place_vault_monster_token('Y', y, x);
                break;
            }

            /* Spider */
            case 'M':
            {
                place_monster_by_flag(
                    y, x, 3, RF3_SPIDER, true,
                    player_generation_depth() + rand_range(1, 4));
                break;
            }

            /* Vampire */
            case 'v':
            {
                place_monster_by_letter(
                    y, x, 'v', true, player_generation_depth() + rand_range(1, 4));
                break;
            }

            /* Wight/Wraith */
            case 'g':
            {
                place_monster_by_letter(
                    y, x, 'W', true, player_generation_depth() + rand_range(1, 4));
                break;
            }

                /* Archer */
            case 'a':
            {
                place_monster_by_flag(
                    y, x, 4, (RF4_ARROW1 | RF4_ARROW2), true,
                    player_generation_depth() + 1);
                break;
            }

                /* Flier */
            case 'b':
            {
                place_monster_by_flag(
                    y, x, 2, (RF2_FLYING), true, player_generation_depth() + 1);
                break;
            }

            /* Wolf */
            case 'c':
            {
                place_monster_by_flag(
                    y, x, 3, RF3_WOLF, true,
                    player_generation_depth() + rand_range(1, 4));
                break;
            }

            /* Rauko */
            case 'r':
            {
                place_monster_by_flag(
                    y, x, 3, RF3_RAUKO, true,
                    player_generation_depth() + rand_range(1, 4));
                break;
            }

                /* Aldor */
            case 'A':
            {
                place_vault_monster_token('A', y, x);
                break;
            }
            /* Aulë (quest giver) */
            case 'L':
            {
                place_vault_monster_token('L', y, x);
                break;
            }
            /* Mandos (quest giver) */
            case 'N':
            {
                place_vault_monster_token('N', y, x);
                break;
            }

            /* Glaurung */
            case 'D':
            {
                place_vault_monster_token('D', y, x);
                break;
            }

            /* Ancalagon the Black */
            case 'K':
            {
                place_vault_monster_token('K', y, x);
                break;
            }

            /* Flying cold-drake */
            case 'I':
            {
                place_vault_monster_token('I', y, x);
                break;
            }

            /* Flying fire-drake */
            case 'J':
            {
                place_vault_monster_token('J', y, x);
                break;
            }

            /* Gothmog */
            case 'R':
            {
                place_vault_monster_token('R', y, x);
                break;
            }

            /* Ungoliant */
            case 'U':
            {
                place_vault_monster_token('U', y, x);
                break;
            }

            /* Gorthaur */
            case 'G':
            {
                place_vault_monster_token('G', y, x);
                break;
            }

            /* Morgoth */
            case 'V':
            {
                place_vault_monster_token('V', y, x);
                break;
            }

            /* Duruin (Least of the Balrogs) */
            case 'B':
            {
                place_vault_monster_token('B', y, x);
                break;
            }

            /* Whispering shadow */
            case 'q':
            {
                place_vault_monster_token('q', y, x);
                break;
            }

            /* Shadow spider */
            case 'j':
            {
                place_vault_monster_token('j', y, x);
                break;
            }

            /* Lurking horror */
            case 'k':
            {
                place_vault_monster_token('k', y, x);
                break;
            }

            /* Nightthorn */
            case 'n':
            {
                place_vault_monster_token('n', y, x);
                break;
            }
            }
        }
    }

    /* Place dungeon features and objects */
    for (t = data, dy = 0; dy < ymax; dy++)
    {
        if (flip_v)
            ay = ymax - 1 - dy;
        else
            ay = dy;

        for (dx = 0; dx < xmax; dx++, t++)
        {
            if (flip_h)
                ax = xmax - 1 - dx;
            else
                ax = dx;

            /* Extract the location */
            x = x0 - (xmax / 2) + ax;
            y = y0 - (ymax / 2) + ay;

            // diagonal flipping
            if (flip_d)
            {
                x = x0 - (ymax / 2) + ay;
                y = y0 - (xmax / 2) + ax;
            }

            /* Hack -- skip "non-grids" */
            if (*t == ' ')
                continue;

            // some vaults are always lit
            if (v_ptr->flags & (VLT_LIGHT))
            {
                cave_info[y][x] |= (CAVE_GLOW);
            }

            // traps are usually 5 times as likely in vaults, but are 10 times
            // as likely if the TRAPS flag is set
            multiplier = (v_ptr->flags & (VLT_TRAPS)) ? 10 : 5;

            // another chance to place traps, with 4 times the normal chance
            // so traps in interesting rooms and vaults are a total of 5 times
            // more likely webbed vaults also have a large chance of receiving
            // webs
            if ((v_ptr->flags & (VLT_WEBS)))
            {
                if (cave_naked_bold(y, x) && one_in_(20))
                {
                    /* Place a web trap */
                    cave_set_feat(y, x, FEAT_TRAP_WEB);

                    // Hide it half the time
                    if (one_in_(2))
                    {
                        cave_info[y][x] |= (CAVE_HIDDEN);
                    }
                }
            }
            else if (dieroll(1000)
                <= trap_placement_chance(y, x) * (multiplier - 1))
            {
                place_trap(y, x);
            }
        }
    }

    current_build_vault_type = previous_build_vault_type;
    log_trace(
        "SPECIAL_VAULT_ONLY context leave: vault='%s' restored_type=%d depth=%d",
        v_name + v_ptr->name, current_build_vault_type, p_ptr->depth);
    }

    log_trace("build_vault: Successfully built vault '%s' at (%d,%d)", v_name + v_ptr->name, y0, x0);
    return (true);
}

/*
 * Generate helper -- test a rectangle to see if it is all rock with reduced padding
 * (i.e. not floor and not icky) - used for quest vaults to reduce placement failures
 */
bool solid_rock_reduced_padding(int y1, int x1, int y2, int x2)
{
    int y, x;

    if (x2 >= MAX_DUNGEON_WID || y2 >= MAX_DUNGEON_HGT)
        return (false);

    for (y = y1; y <= y2; y++)
    {
        for (x = x1; x <= x2; x++)
        {
            if (cave_feat[y][x] == FEAT_FLOOR)
                return (false);
            if (cave_info[y][x] & CAVE_ICKY)
                return (false);
        }
    }
    return (true);
}

/*
 * Compute vault bounds for a candidate placement, accounting for diagonal rotation.
 */
void compute_vault_bounds(
    int y0, int x0, const vault_type* v_ptr, bool flip_d,
    int* y1, int* x1, int* y2, int* x2)
{
    if (flip_d)
    {
        /* determine the coordinates with height/width flipped */
        *y1 = y0 - (v_ptr->wid / 2);
        *x1 = x0 - (v_ptr->hgt / 2);
        *y2 = *y1 + v_ptr->wid - 1;
        *x2 = *x1 + v_ptr->hgt - 1;
    }
    else
    {
        /* determine the coordinates */
        *y1 = y0 - (v_ptr->hgt / 2);
        *x1 = x0 - (v_ptr->wid / 2);
        *y2 = *y1 + v_ptr->hgt - 1;
        *x2 = *x1 + v_ptr->wid - 1;
    }
}

/*
 * Place a room using forced placement strategy with reduced padding for quest vaults.
 * The caller can suppress failure logging when doing an exhaustive fit scan.
 */
bool place_room_forced_internal(
    int y0, int x0, vault_type* v_ptr, bool flip_d, bool log_failures)
{
    int y1, x1, y2, x2;

    compute_vault_bounds(y0, x0, v_ptr, flip_d, &y1, &x1, &y2, &x2);

    if (log_failures)
    {
        log_trace(
            "place_room_forced: Attempting quest vault '%s' at center (%d,%d), size %dx%d, flip_d=%s",
            v_name + v_ptr->name, y0, x0, v_ptr->hgt, v_ptr->wid,
            flip_d ? "true" : "false");
    }

    /* make sure that the location is within the map bounds */
    if ((y1 <= 2) || (x1 <= 2) || (y2 >= p_ptr->cur_map_hgt - 2)
        || (x2 >= p_ptr->cur_map_wid - 2))
    {
        if (log_failures)
        {
            log_trace(
                "place_room_forced: Vault bounds check failed - y1=%d x1=%d y2=%d x2=%d (map size %dx%d)",
                y1, x1, y2, x2, p_ptr->cur_map_hgt, p_ptr->cur_map_wid);
        }
        return (false);
    }

    /* make sure that the location is empty using reduced padding (1 cell instead of 2) */
    if (!solid_rock_reduced_padding(y1 - 1, x1 - 1, y2 + 1, x2 + 1))
    {
        if (log_failures)
        {
            log_trace(
                "place_room_forced: solid_rock_reduced_padding check failed - area not empty around (%d,%d)-(%d,%d)",
                y1 - 1, x1 - 1, y2 + 1, x2 + 1);
        }
        return (false);
    }

    /* Try building the vault */
    if (!build_vault(y0, x0, v_ptr, flip_d))
    {
        if (log_failures)
        {
            log_trace(
                "place_room_forced: build_vault failed for quest vault '%s' at (%d,%d)",
                v_name + v_ptr->name, y0, x0);
        }
        return (false);
    }

    if (log_failures)
    {
        log_trace(
            "place_room_forced: Successfully built quest vault '%s' at (%d,%d) with reduced padding",
            v_name + v_ptr->name, y0, x0);
    }

    /* save the corner locations */
    dun->corner[dun->cent_n].y1 = y1 + 1;
    dun->corner[dun->cent_n].x1 = x1 + 1;
    dun->corner[dun->cent_n].y2 = y2 - 1;
    dun->corner[dun->cent_n].x2 = x2 - 1;

    /* Save the room location */
    dun->cent[dun->cent_n].y = y0;
    dun->cent[dun->cent_n].x = x0;
    dun->kind[dun->cent_n] = (byte)v_ptr->typ;
    dun->is_quest[dun->cent_n] = (v_ptr->flags & VLT_QUEST) ? true : false;
    dun->cent_n++;

    /* Cause a special feeling */
    good_item_flag = true;

    log_trace("build_vault: *** SUCCESSFULLY COMPLETED *** vault '%s' at (%d,%d)",
              v_name + v_ptr->name, y0, x0);

    /* DEBUGGING: For quest vaults, do immediate verification */
    if (v_ptr->flags & VLT_QUEST) {
        int verify_y1 = y0 - v_ptr->hgt / 2;
        int verify_x1 = x0 - v_ptr->wid / 2;
        int verify_y2 = verify_y1 + v_ptr->hgt - 1;
        int verify_x2 = verify_x1 + v_ptr->wid - 1;

        int post_walls = 0, post_floors = 0, post_features = 0, post_monsters = 0;
        int post_icky = 0, post_room = 0;

        for (int vy = verify_y1; vy <= verify_y2; vy++) {
            for (int vx = verify_x1; vx <= verify_x2; vx++) {
                if (cave_feat[vy][vx] == FEAT_WALL_OUTER || cave_feat[vy][vx] == FEAT_WALL_INNER) {
                    post_walls++;
                } else if (cave_feat[vy][vx] == FEAT_FLOOR) {
                    post_floors++;
                } else if (cave_feat[vy][vx] != FEAT_WALL_EXTRA) {
                    post_features++;
                }

                if (cave_m_idx[vy][vx] > 0) {
                    post_monsters++;
                }

                if (cave_info[vy][vx] & CAVE_ICKY) {
                    post_icky++;
                }

                if (cave_info[vy][vx] & CAVE_ROOM) {
                    post_room++;
                }
            }
        }

        log_trace("build_vault: QUEST VAULT POST-BUILD VERIFICATION: Area (%d,%d) to (%d,%d)",
                  verify_y1, verify_x1, verify_y2, verify_x2);
        log_trace("build_vault: QUEST VAULT POST-BUILD: %d walls, %d floors, %d features, %d monsters",
                  post_walls, post_floors, post_features, post_monsters);
        log_trace("build_vault: QUEST VAULT POST-BUILD: %d CAVE_ICKY, %d CAVE_ROOM flags",
                  post_icky, post_room);
    }

    return (true);
}

/*
 * Place a room using forced placement strategy with reduced padding for quest vaults.
 * Prefer the legacy random orientation first, but also try the alternate orientation
 * before giving up so "must place" quest content does not miss obvious fits.
 */
bool place_room_forced(int y0, int x0, vault_type* v_ptr)
{
    bool allow_flip = !(v_ptr->flags & (VLT_NO_ROTATION));
    bool preferred_flip = allow_flip ? one_in_(3) : false;

    if (place_room_forced_internal(y0, x0, v_ptr, preferred_flip, true))
        return true;

    if (allow_flip && place_room_forced_internal(y0, x0, v_ptr, !preferred_flip, false))
    {
        log_trace("place_room_forced: Quest vault '%s' fit after trying alternate orientation at (%d,%d)",
            v_name + v_ptr->name, y0, x0);
        return true;
    }

    return false;
}

/*
 * Final fallback for "must place" quest vaults: scan the whole map for any fit.
 * This avoids regeneration loops caused by a handful of unlucky center-biased samples.
 */
bool place_room_forced_exhaustive(
    vault_type* v_ptr, int* placed_y, int* placed_x)
{
    bool allow_flip = !(v_ptr->flags & (VLT_NO_ROTATION));

    for (int y = 3; y < p_ptr->cur_map_hgt - 3; y++)
    {
        for (int x = 3; x < p_ptr->cur_map_wid - 3; x++)
        {
            if (place_room_forced_internal(y, x, v_ptr, false, false))
            {
                if (placed_y) *placed_y = y;
                if (placed_x) *placed_x = x;
                return true;
            }

            if (allow_flip && place_room_forced_internal(y, x, v_ptr, true, false))
            {
                if (placed_y) *placed_y = y;
                if (placed_x) *placed_x = x;
                return true;
            }
        }
    }

    return false;
}

bool place_room(int y0, int x0, vault_type* v_ptr)
{
    int y1, x1, y2, x2;
    bool flip_d;

    log_trace("place_room: Attempting to place vault '%s' at center (%d,%d), size %dx%d",
             v_name + v_ptr->name, y0, x0, v_ptr->hgt, v_ptr->wid);

    // choose whether to rotate (flip diagonally)
    flip_d = one_in_(3);

    // some vaults ask not be be rotated
    if (v_ptr->flags & (VLT_NO_ROTATION))
        flip_d = false;

    if (flip_d)
    {
        /* determine the coordinates with height/width flipped */
        y1 = y0 - (v_ptr->wid / 2);
        x1 = x0 - (v_ptr->hgt / 2);
        y2 = y1 + v_ptr->wid - 1;
        x2 = x1 + v_ptr->hgt - 1;
    }

    else
    {
        /* determine the coordinates */
        y1 = y0 - (v_ptr->hgt / 2);
        x1 = x0 - (v_ptr->wid / 2);
        y2 = y1 + v_ptr->hgt - 1;
        x2 = x1 + v_ptr->wid - 1;
    }

    /* make sure that the location is within the map bounds */
    if ((y1 <= 3) || (x1 <= 3) || (y2 >= p_ptr->cur_map_hgt - 3)
        || (x2 >= p_ptr->cur_map_wid - 3))
    {
        log_trace("place_room: Vault bounds check failed - y1=%d x1=%d y2=%d x2=%d (map size %dx%d)",
                 y1, x1, y2, x2, p_ptr->cur_map_hgt, p_ptr->cur_map_wid);
        return (false);
    }
    /* make sure that the location is empty */
    if (!solid_rock(y1 - 2, x1 - 2, y2 + 2, x2 + 2))
    {
        log_trace("place_room: solid_rock check failed - area not empty around (%d,%d)-(%d,%d)",
                 y1 - 2, x1 - 2, y2 + 2, x2 + 2);
        return (false);
    }

    /* Try building the vault */
    if (!build_vault(y0, x0, v_ptr, flip_d))
    {
        log_trace("place_room: build_vault failed for vault '%s' at (%d,%d)",
                 v_name + v_ptr->name, y0, x0);
        return (false);
    }

    log_trace("place_room: Successfully built vault '%s' at (%d,%d)",
             v_name + v_ptr->name, y0, x0);

    /* save the corner locations */
    dun->corner[dun->cent_n].y1 = y1 + 1;
    dun->corner[dun->cent_n].x1 = x1 + 1;
    dun->corner[dun->cent_n].y2 = y2 - 1;
    dun->corner[dun->cent_n].x2 = x2 - 1;

    /* Save the room location */
    dun->cent[dun->cent_n].y = y0;
    dun->cent[dun->cent_n].x = x0;
    dun->kind[dun->cent_n] = (byte)v_ptr->typ;
    dun->is_quest[dun->cent_n] = (v_ptr->flags & VLT_QUEST) ? true : false;
    dun->cent_n++;

    /* Cause a special feeling */
    good_item_flag = true;

    return (true);
}


/* Ensure we have clear granite around a prospective docked vault, allowing
 * the contact edge to abut an existing vault wall. */
bool area_clear_for_vault_dock(
    int y1, int x1, int y2, int x2, vault_dock_dir_t dir)
{
    int y_lo = y1 - 1;
    int y_hi = y2 + 1;
    int x_lo = x1 - 1;
    int x_hi = x2 + 1;

    if ((y_lo < 1) || (x_lo < 1) || (y_hi >= p_ptr->cur_map_hgt - 1)
        || (x_hi >= p_ptr->cur_map_wid - 1))
    {
        return false;
    }

    for (int y = y_lo; y <= y_hi; ++y)
    {
        for (int x = x_lo; x <= x_hi; ++x)
        {
            bool on_contact = false;
            switch (dir)
            {
            case VAULT_DOCK_EAST:
                on_contact = (x == x1 - 1);
                break;
            case VAULT_DOCK_WEST:
                on_contact = (x == x2 + 1);
                break;
            case VAULT_DOCK_NORTH:
                on_contact = (y == y2 + 1);
                break;
            case VAULT_DOCK_SOUTH:
                on_contact = (y == y1 - 1);
                break;
            }

            if (on_contact)
            {
                /* Allow touching an existing vault wall, but not overlapping
                 * known open space such as corridors. */
                if ((cave_feat[y][x] == FEAT_FLOOR)
                    && !(cave_info[y][x] & (CAVE_ICKY)))
                {
                    return false;
                }
                continue;
            }

            if (cave_info[y][x] & (CAVE_ROOM | CAVE_ICKY))
            {
                return false;
            }

            if (cave_feat[y][x] != FEAT_WALL_EXTRA)
            {
                return false;
            }
        }
    }

    return true;
}

/* Pick a contact point along one edge of an existing vault, preferring doors
 * but falling back to plain walls. */
bool choose_vault_contact(
    int base_idx, vault_dock_dir_t dir, int* y_out, int* x_out)
{
    int y1 = dun->corner[base_idx].y1 - 1;
    int y2 = dun->corner[base_idx].y2 + 1;
    int x1 = dun->corner[base_idx].x1 - 1;
    int x2 = dun->corner[base_idx].x2 + 1;

    int door_seen = 0, wall_seen = 0;
    int door_y = 0, door_x = 0, wall_y = 0, wall_x = 0;

    if (dir == VAULT_DOCK_EAST || dir == VAULT_DOCK_WEST)
    {
        int x = (dir == VAULT_DOCK_EAST) ? x2 : x1;
        for (int y = y1 + 1; y <= y2 - 1; ++y)
        {
            if (!(cave_info[y][x] & (CAVE_ICKY)))
                continue;
            int feat = cave_feat[y][x];
            if (feature_is_any_door(feat))
            {
                door_seen++;
                if (one_in_(door_seen))
                {
                    door_y = y;
                    door_x = x;
                }
            }
            else if ((feat >= FEAT_WALL_HEAD) && (feat <= FEAT_WALL_TAIL))
            {
                wall_seen++;
                if (one_in_(wall_seen))
                {
                    wall_y = y;
                    wall_x = x;
                }
            }
        }
    }
    else
    {
        int y = (dir == VAULT_DOCK_NORTH) ? y1 : y2;
        for (int x = x1 + 1; x <= x2 - 1; ++x)
        {
            if (!(cave_info[y][x] & (CAVE_ICKY)))
                continue;
            int feat = cave_feat[y][x];
            if (feature_is_any_door(feat))
            {
                door_seen++;
                if (one_in_(door_seen))
                {
                    door_y = y;
                    door_x = x;
                }
            }
            else if ((feat >= FEAT_WALL_HEAD) && (feat <= FEAT_WALL_TAIL))
            {
                wall_seen++;
                if (one_in_(wall_seen))
                {
                    wall_y = y;
                    wall_x = x;
                }
            }
        }
    }

    if (door_seen > 0)
    {
        *y_out = door_y;
        *x_out = door_x;
        return true;
    }
    if (wall_seen > 0)
    {
        *y_out = wall_y;
        *x_out = wall_x;
        return true;
    }
    return false;
}

/* Attempt to place a vault flush against an existing vault so that a single
 * door separates them. Returns the placed centre if successful. */
bool try_place_docked_vault(
    vault_type* v_ptr, int* out_y0, int* out_x0)
{
    if (!room_kind_is_vault((byte)v_ptr->typ))
    {
        return false;
    }

    /* Never dock Morgoth's throne room */
    if (v_ptr->typ == 9)
    {
        return false;
    }

    if (v_ptr->flags & (VLT_QUEST))
    {
        return false;
    }

    if (dun->cent_n >= room_capacity_limit())
    {
        return false;
    }

    /* Collect existing vault indices to target */
    int vault_indices[CENT_MAX];
    int vault_count = 0;
    for (int i = 0; i < dun->cent_n; ++i)
    {
        if (room_kind_is_vault(dun->kind[i]) && !dun->is_quest[i] && dun->kind[i] != 9)
        {
            vault_indices[vault_count++] = i;
        }
    }
    if (vault_count == 0)
    {
        return false;
    }

    /* Try a handful of random attachment attempts */
    for (int attempt = 0; attempt < 40; ++attempt)
    {
        styles_set_vault_avoid_style(-1);
        int base_idx = vault_indices[rand_int(vault_count)];
        int base_y1 = dun->corner[base_idx].y1 - 1;
        int base_y2 = dun->corner[base_idx].y2 + 1;
        int base_x1 = dun->corner[base_idx].x1 - 1;
        int base_x2 = dun->corner[base_idx].x2 + 1;

        vault_dock_dir_t dir_order[4] = {VAULT_DOCK_NORTH, VAULT_DOCK_EAST,
            VAULT_DOCK_SOUTH, VAULT_DOCK_WEST};
        for (int s = 0; s < 4; ++s)
        {
            int swap_idx = rand_int(4);
            vault_dock_dir_t tmp = dir_order[s];
            dir_order[s] = dir_order[swap_idx];
            dir_order[swap_idx] = tmp;
        }

        for (int di = 0; di < 4; ++di)
        {
            vault_dock_dir_t dir = dir_order[di];
            int contact_y = 0, contact_x = 0;
            if (!choose_vault_contact(base_idx, dir, &contact_y, &contact_x))
            {
                continue;
            }

            /* Prefer a different primary style than the contacted vault */
            int avoid_style = style_at_color(contact_y, contact_x);
            styles_set_vault_avoid_style(avoid_style);

            bool flip_d = (!(v_ptr->flags & (VLT_NO_ROTATION)) && one_in_(3));
            int h = flip_d ? v_ptr->wid : v_ptr->hgt;
            int w = flip_d ? v_ptr->hgt : v_ptr->wid;

            int y0 = 0, x0 = 0, y1 = 0, x1 = 0, y2 = 0, x2 = 0;

            switch (dir)
            {
            case VAULT_DOCK_EAST:
                x1 = base_x2 + 1;
                x2 = x1 + w - 1;
                x0 = x1 + w / 2;
                y0 = contact_y;
                y1 = y0 - h / 2;
                y2 = y1 + h - 1;
                break;
            case VAULT_DOCK_WEST:
                x2 = base_x1 - 1;
                x1 = x2 - w + 1;
                x0 = x1 + w / 2;
                y0 = contact_y;
                y1 = y0 - h / 2;
                y2 = y1 + h - 1;
                break;
            case VAULT_DOCK_NORTH:
                y2 = base_y1 - 1;
                y1 = y2 - h + 1;
                y0 = y1 + h / 2;
                x0 = contact_x;
                x1 = x0 - w / 2;
                x2 = x1 + w - 1;
                break;
            case VAULT_DOCK_SOUTH:
                y1 = base_y2 + 1;
                y2 = y1 + h - 1;
                y0 = y1 + h / 2;
                x0 = contact_x;
                x1 = x0 - w / 2;
                x2 = x1 + w - 1;
                break;
            }

            if ((y1 <= 3) || (x1 <= 3) || (y2 >= p_ptr->cur_map_hgt - 3)
                || (x2 >= p_ptr->cur_map_wid - 3))
            {
                continue;
            }

            if (!area_clear_for_vault_dock(y1, x1, y2, x2, dir))
            {
                continue;
            }

            if (!build_vault(y0, x0, v_ptr, flip_d))
            {
                styles_set_vault_avoid_style(-1);
                continue;
            }

            dun->corner[dun->cent_n].y1 = y1 + 1;
            dun->corner[dun->cent_n].x1 = x1 + 1;
            dun->corner[dun->cent_n].y2 = y2 - 1;
            dun->corner[dun->cent_n].x2 = x2 - 1;
            dun->cent[dun->cent_n].y = y0;
            dun->cent[dun->cent_n].x = x0;
            dun->kind[dun->cent_n] = (byte)v_ptr->typ;
            dun->is_quest[dun->cent_n] = false;
            int new_idx = dun->cent_n;
            dun->cent_n++;

            dun->connection[base_idx][new_idx] = true;
            dun->connection[new_idx][base_idx] = true;

            int new_y = contact_y;
            int new_x = contact_x;
            if (dir == VAULT_DOCK_EAST)
                new_x = contact_x + 1;
            else if (dir == VAULT_DOCK_WEST)
                new_x = contact_x - 1;
            else if (dir == VAULT_DOCK_SOUTH)
                new_y = contact_y + 1;
            else
                new_y = contact_y - 1;

            if (!feature_is_any_door(cave_feat[contact_y][contact_x]))
            {
                place_closed_door(contact_y, contact_x);
            }

            /* Carve through walls in BOTH vaults to ensure passability.
             * We need to carve into the docked vault AND into the base vault,
             * since either side may have thick walls at the contact point. */
            int dy = 0, dx = 0;
            if (dir == VAULT_DOCK_EAST) dx = 1;
            else if (dir == VAULT_DOCK_WEST) dx = -1;
            else if (dir == VAULT_DOCK_SOUTH) dy = 1;
            else dy = -1;

            /* Carve in both directions from the door */
            for (int side = 0; side < 2; ++side)
            {
                int carve_dy, carve_dx, start_y, start_x;

                if (side == 0)
                {
                    /* Carve into the docked vault */
                    carve_dy = dy;
                    carve_dx = dx;
                    start_y = new_y;
                    start_x = new_x;
                }
                else
                {
                    /* Carve into the base vault (opposite direction) */
                    carve_dy = -dy;
                    carve_dx = -dx;
                    start_y = contact_y - dy;
                    start_x = contact_x - dx;
                }

                int carve_y = start_y;
                int carve_x = start_x;
                int max_carve = 6;
                bool found_floor = false;

                for (int c = 0; c < max_carve; ++c)
                {
                    int feat = cave_feat[carve_y][carve_x];
                    if (feat == FEAT_FLOOR || feature_is_any_door(feat))
                    {
                        found_floor = true;
                        break;
                    }
                    if (!(cave_info[carve_y][carve_x] & CAVE_ICKY))
                        break;
                    cave_set_feat(carve_y, carve_x, FEAT_FLOOR);
                    carve_y += carve_dy;
                    carve_x += carve_dx;
                }

                /* If straight carve didn't find floor, search perpendicular */
                if (!found_floor)
                {
                    int perp_dy = (carve_dy == 0) ? 1 : 0;
                    int perp_dx = (carve_dx == 0) ? 1 : 0;
                    int search_radius = 8;

                    for (int sign = -1; sign <= 1; sign += 2)
                    {
                        for (int dist = 1; dist <= search_radius; ++dist)
                        {
                            int check_y = carve_y + sign * perp_dy * dist;
                            int check_x = carve_x + sign * perp_dx * dist;

                            if (!(cave_info[check_y][check_x] & CAVE_ICKY))
                                break;

                            int feat = cave_feat[check_y][check_x];
                            if (feat == FEAT_FLOOR || feature_is_any_door(feat))
                            {
                                for (int d = 1; d < dist; ++d)
                                {
                                    int path_y = carve_y + sign * perp_dy * d;
                                    int path_x = carve_x + sign * perp_dx * d;
                                    cave_set_feat(path_y, path_x, FEAT_FLOOR);
                                }
                                found_floor = true;
                                break;
                            }
                        }
                        if (found_floor) break;
                    }
                }
            }

            good_item_flag = true;

            if (out_y0)
                *out_y0 = y0;
            if (out_x0)
                *out_x0 = x0;

            styles_set_vault_avoid_style(-1);
            return true;
        }
    }

    styles_set_vault_avoid_style(-1);
    return false;
}
