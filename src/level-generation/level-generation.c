/* File: level-generation.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

bool cave_gen(void)
{
    int i;

    int l;

    int y, x;

    int room_attempts = 0;

    int is_guaranteed_forge_level = false;
    bool duruin_bastion_forced = false;
    bool is_morgoth_level = (p_ptr->depth == MORGOTH_DEPTH);

    reset_morgoth_layout_state(is_morgoth_level);

    /* Reset labyrinth partition counter for this level */
    current_labyrinth_partitions = 0;

    /* Reset quest vault monitoring variables for this level */
    qv_placed_this_level = false;
    qv_stored_y1 = qv_stored_x1 = qv_stored_y2 = qv_stored_x2 = -1;

    /* Run quest lottery once per level to determine which quest (if any) gets this level */
    if (is_morgoth_level) {
        quest_lottery_winner = 0;
    } else {
        run_quest_lottery();
    }

    /* Debug: Log entry into cave_gen */
    log_trace("cave_gen: Starting level generation (quest_vault_used=%d, lottery_winner=%d)",
              p_ptr->quest_vault_used, quest_lottery_winner);

    log_trace("cave_gen: Quest status at level start - initiated=%d/%d, accepted=%d/%d, varda_quest=%d, lottery_winner=%d",
              quest_initiated_count_this_run(), QUEST_MAX_INITIATED_PER_RUN,
              quest_accepted_count_this_run(), QUEST_MAX_ACCEPTED_PER_RUN,
              p_ptr->varda_quest, quest_lottery_winner);

    /* Varda quest: flag forced bastion placement on first level deeper than 500ft */
    if (!is_morgoth_level && p_ptr->varda_quest == VARDA_QUEST_ACTIVE && !p_ptr->varda_vault_placed && p_ptr->depth > 10) {
        if (!p_ptr->varda_vault_ready) {
            log_trace("Varda quest: Crossing 500ft, setting bastion_ready at depth %d", p_ptr->depth);
        }
        p_ptr->varda_vault_ready = 1;
    }
    s16b mon_gen, obj_room_gen;
    memset(dun, 0, sizeof(*dun));

    /* Sil - determine the dungeon size */
    /* Generate square levels: 4*11 to 15*11 (44x44 to 165x165) */
    /* Probability increases with depth, larger sizes more probable */

    // Base size: 9 blocks (increased from 7 for larger level sizes)
    // Size increases with depth, with bias toward larger sizes
    // Formula: Use multiple dice rolls and take the maximum (biases upward)
    // Two independent uniform rolls: X1 = dieroll(17) (1..17), X2 = dieroll(14) (1..14)
    int base_size = 9;  // Increased from 7 for larger starting levels
    int depth_factor = p_ptr->depth + dieroll(17);  // Higher ceiling (1-17)
    int bonus1 = depth_factor / 3;  // First roll (uses X1)
    int bonus2 = (p_ptr->depth + dieroll(14)) / 3;  // Second roll (uses X2)
    int depth_bonus = MAX(bonus1, bonus2);  // Take maximum (biases larger)

    l = base_size + depth_bonus;
    if (l > MAX_LEVEL_BLOCKS) l = MAX_LEVEL_BLOCKS;  // Hard cap at MAX_LEVEL_BLOCKS
    if (l < 8) l = 8;    // Hard floor at 8 blocks (88x88)

    if (smaller_level_size)
    {
        l -= 3;
        if (l < 6) l = 6; /* Allow 6x6 and 7x7 block maps */
    }

    // Square levels: same dimension for both height and width
    p_ptr->cur_map_hgt = l * (PANEL_HGT);
    p_ptr->cur_map_wid = l * (PANEL_HGT);  // Use PANEL_HGT for both to make square

    /* Fewer room attempts to reduce long regen loops; vault bias handled later */
    room_attempts = l * l * l * 2;
    log_trace("cave_gen: SQUARE map size set to %dx%d (l=%d blocks) room_attempts=%d",
              p_ptr->cur_map_wid, p_ptr->cur_map_hgt, l, room_attempts);

    /* Generation log: level start */
    gen_log_level_start(p_ptr->depth, p_ptr->cur_map_hgt, p_ptr->cur_map_wid);
    genlog_summary("Level %d generation starting: %dx%d map (%d blocks), %d room attempts",
                   p_ptr->depth, p_ptr->cur_map_hgt, p_ptr->cur_map_wid, l, room_attempts);
    genlog_quest("Quest lottery winner=%d, quest_vault_used=%d, varda_quest=%d",
                 quest_lottery_winner, p_ptr->quest_vault_used, p_ptr->varda_quest);
    {
        char detail[160];
        strnfmt(detail, sizeof(detail), "%dx%d map, %d blocks, %d room attempts",
            p_ptr->cur_map_hgt, p_ptr->cur_map_wid, l, room_attempts);
        level_gen_screen_set_stage(LEVEL_GEN_STAGE_PLANNING, detail);
    }

    /* Initialize level style weights and start with basic granite */
    level_gen_screen_set_stage(LEVEL_GEN_STAGE_FOUNDATIONS,
        "Resetting styles, granite, and connection tables.");
    styles_init_for_level();
    /*start with basic granite*/
    basic_granite();
    log_trace("cave_gen: after styles_init/basic_granite");

    log_trace("cave_gen: before connection table init (DUN_ROOMS=%d, conn_size=%zu)", DUN_ROOMS, sizeof(dun->connection));
    /* Initialize the connection table */
    for (y = 0; y < DUN_ROOMS; y++)
    {
        if (y == 0 || y == DUN_ROOMS - 1)
            log_trace("cave_gen: init conn row %d start", y);
        for (x = 0; x < DUN_ROOMS; x++)
        {
            dun->connection[y][x] = false;
        }
        log_trace("cave_gen: connection init row=%d done", y);
    }
    log_trace("cave_gen: after connection table init");

    /* No rooms yet */
    dun->cent_n = 0;
    log_trace("cave_gen: cent_n reset to 0");
    layout_anchor_reset();

    /* Verify dun struct sanity */
    log_trace("cave_gen: sanity check dun ptr=%p cent capacity=%d connection[0][0]=%d piece[0]=%d corner[0]=(y1=%d,x1=%d,y2=%d,x2=%d)",
        (void*)dun, DUN_ROOMS, dun->connection[0][0], dun->piece[0],
        dun->corner[0].y1, dun->corner[0].x1, dun->corner[0].y2, dun->corner[0].x2);

    if (cheat_room)
        msg_format("Forge count is %d.", p_ptr->forge_count);

    // guarantee a forge at first entrance to levels 2, 6, 10 (or below if skipped via shaft)
    if (p_ptr->fixed_forge_count < 3)
    {
        int next_guaranteed_forge_level = 2 + (p_ptr->fixed_forge_count * 4);
        is_guaranteed_forge_level = (next_guaranteed_forge_level <= p_ptr->depth);
        log_trace("Forge forcing check: fixed_forge_count=%d, target_level=%d, current_depth=%d, forcing=%s",
                 p_ptr->fixed_forge_count, next_guaranteed_forge_level, p_ptr->depth,
                 is_guaranteed_forge_level ? "true" : "false");
    }

    if (cheat_room)
        msg_format("Guaranteed forge: %s.",
            is_guaranteed_forge_level ? "true" : "false");

    log_trace("cave_gen: before guaranteed forge handling");
    if (is_guaranteed_forge_level)
    {
        int y = rand_range(5, p_ptr->cur_map_hgt - 5);
        int x = rand_range(5, p_ptr->cur_map_wid - 5);
        log_trace("cave_gen: attempting guaranteed forge at (%d,%d)", y, x);

        if (cheat_room)
            msg_format("Trying to force a forge:");
        p_ptr->force_forge = true;
        p_ptr->fixed_forge_count++;
        log_trace("cave_gen: force_forge=true, fixed_forge_count=%d", p_ptr->fixed_forge_count);

        if (!build_type6(y, x, true))
        {
            if (cheat_room)
                msg_format("failed.");

            p_ptr->fixed_forge_count--;
            return (false);
        }

        if (cheat_room)
            msg_format("succeeded.");
    }
    log_trace("cave_gen: post guaranteed-forge path cent_n=%d", dun->cent_n);
    log_trace("cave_gen: post guaranteed-forge path cent_n=%d", dun->cent_n);

    if (!is_morgoth_level)
    {
        /* Quest vault determination - Allow re-placement during level regeneration */
        log_trace("Quest vault: ENTERING quest vault logic check (quest_vault_used=%d, force_forge=%s, qv_placed_this_level=%s)",
                  p_ptr->quest_vault_used,
                  p_ptr->force_forge ? "true" : "false",
                  qv_placed_this_level ? "true" : "false");
        log_trace("Quest vault: Starting quest vault check (quest_vault_used=%d, force_forge=%s)",
                  p_ptr->quest_vault_used,
                  p_ptr->force_forge ? "true" : "false");

        /* If Varda's quest is active and the bastion is due, force its placement first */
        log_trace("Quest vault check: varda_vault_ready=%d, varda_quest=%d (ACTIVE=%d), varda_vault_placed=%d",
                  p_ptr->varda_vault_ready, p_ptr->varda_quest, VARDA_QUEST_ACTIVE, p_ptr->varda_vault_placed);

        if (p_ptr->varda_vault_ready && p_ptr->varda_quest == VARDA_QUEST_ACTIVE && !p_ptr->varda_vault_placed) {
            log_trace("Quest vault: === DURUIN BASTION FORCE PLACEMENT === Starting at depth %d", p_ptr->depth);
            if (!place_duruin_bastion()) {
                log_trace("Quest vault: === DURUIN BASTION FAILED === Regenerating level");
                return false;
            }
            log_trace("Quest vault: === DURUIN BASTION SUCCESS === Placed successfully");
            duruin_bastion_forced = true;
        } else if (p_ptr->varda_quest == VARDA_QUEST_ACTIVE) {
            log_trace("Quest vault: Varda quest ACTIVE but bastion not ready (vault_ready=%d, vault_placed=%d)",
                      p_ptr->varda_vault_ready, p_ptr->varda_vault_placed);
        }

        /* Quest vaults can add another initiated quest until the per-run cap. */
        if (quest_can_initiate_more() && !duruin_bastion_forced)
        {
            log_trace("Quest vault: Checking placement allowance:");
            log_trace("Quest vault:   initiated=%d/%d, lottery_winner=%d",
                      quest_initiated_count_this_run(), QUEST_MAX_INITIATED_PER_RUN, quest_lottery_winner);
            log_trace("Quest vault:   tulkas=%d, mandos=%d, aule=%d, varda=%d",
                      p_ptr->tulkas_quest, p_ptr->mandos_quest, p_ptr->aule_quest,
                      p_ptr->varda_quest);

            if (quest_lottery_winner > 0) {
                log_trace("Quest vault: === BLOCKED === this level already has roulette quest %d", quest_lottery_winner);
            } else {
                int quest_vault_roll;

                if (cached_quest_vault_roll >= 0)
                {
                    quest_vault_roll = cached_quest_vault_roll;
                    log_trace("Quest vault: Reusing locked roll = %d", quest_vault_roll);
                }
                else
                {
                    quest_vault_roll = dieroll(p_ptr->depth + 5);
                    log_trace("Quest vault: Level determination roll = %d", quest_vault_roll);

                    if (one_in_(5))
                    {
                        int bonus = dieroll(5);
                        quest_vault_roll += bonus;
                        log_trace("Quest vault: Bonus roll (+%d) = %d total", bonus, quest_vault_roll);
                    }

                    cached_quest_vault_roll = quest_vault_roll;
                }

                bool quest_vault_placed = false;
                bool had_eligible_quest_vault = false;

                if (quest_vault_roll >= 18)
                {
                    bool type8_eligible = false;
                    bool type7_eligible = false;
                    bool type6_eligible = false;

                    log_trace("Quest vault: Hit greater vault threshold (%d >= 18), trying quest vaults 8->7->6", quest_vault_roll);
                    quest_vault_placed = try_quest_vault_type(8, &type8_eligible)
                        || try_quest_vault_type(7, &type7_eligible)
                        || try_quest_vault_type(6, &type6_eligible);
                    had_eligible_quest_vault = type8_eligible || type7_eligible || type6_eligible;

                    if (!quest_vault_placed && had_eligible_quest_vault) {
                        log_trace("Quest vault: === FAILED TO PLACE REQUIRED QUEST VAULT === Regenerating level");
                        genlog_fail("QUEST VAULT FAILED: required (roll=%d >= 18), could not place type 8/7/6 '%s' - regenerating",
                            quest_vault_roll,
                            level_gen_debug_last_quest_vault_name[0]
                                ? level_gen_debug_last_quest_vault_name
                                : "(unknown)");
                        gen_log_level_end(false, dun->cent_n, 1);
                        return false; /* Force regeneration to guarantee quest vault spawns */
                    }
                    if (!quest_vault_placed) {
                        log_trace("Quest vault: Roll reserved 8/7/6, but no eligible quest vault exists for this character/run");
                    }
                }
                else if (quest_vault_roll >= 13)
                {
                    bool type7_eligible = false;
                    bool type6_eligible = false;

                    log_trace("Quest vault: Hit lesser vault threshold (%d >= 13), trying quest vaults 7->6", quest_vault_roll);
                    quest_vault_placed = try_quest_vault_type(7, &type7_eligible)
                        || try_quest_vault_type(6, &type6_eligible);
                    had_eligible_quest_vault = type7_eligible || type6_eligible;

                    if (!quest_vault_placed && had_eligible_quest_vault) {
                        log_trace("Quest vault: === FAILED TO PLACE REQUIRED QUEST VAULT === Regenerating level");
                        genlog_fail("QUEST VAULT FAILED: required (roll=%d >= 13), could not place type 7/6 '%s' - regenerating",
                            quest_vault_roll,
                            level_gen_debug_last_quest_vault_name[0]
                                ? level_gen_debug_last_quest_vault_name
                                : "(unknown)");
                        gen_log_level_end(false, dun->cent_n, 1);
                        return false; /* Force regeneration to guarantee quest vault spawns */
                    }
                    if (!quest_vault_placed) {
                        log_trace("Quest vault: Roll reserved 7/6, but no eligible quest vault exists for this character/run");
                    }
                }
                else if (quest_vault_roll >= 8)
                {
                    bool type6_eligible = false;

                    log_trace("Quest vault: Hit interesting room threshold (%d >= 8), trying quest vault 6", quest_vault_roll);
                    quest_vault_placed = try_quest_vault_type(6, &type6_eligible);
                    had_eligible_quest_vault = type6_eligible;

                    if (!quest_vault_placed && had_eligible_quest_vault) {
                        log_trace("Quest vault: === FAILED TO PLACE REQUIRED QUEST VAULT === Regenerating level");
                        genlog_fail("QUEST VAULT FAILED: required (roll=%d >= 8), could not place type 6 '%s' - regenerating",
                            quest_vault_roll,
                            level_gen_debug_last_quest_vault_name[0]
                                ? level_gen_debug_last_quest_vault_name
                                : "(unknown)");
                        gen_log_level_end(false, dun->cent_n, 1);
                        return false; /* Force regeneration to guarantee quest vault spawns */
                    }
                    if (!quest_vault_placed) {
                        log_trace("Quest vault: Roll reserved type 6, but no eligible quest vault exists for this character/run");
                    }
                }
                else
                {
                    log_trace("Quest vault: Roll too low (%d < 8), no quest vault this level", quest_vault_roll);
                }

                if (quest_vault_placed)
                {
                    log_trace("Quest vault: Successfully placed quest vault, no more quest vaults this run");
                }
                else
                {
                    log_trace("Quest vault: No quest vault placed this level");
                }
            }
        }
        else if (duruin_bastion_forced)
        {
            log_trace("Quest vault: Bastion already placed for Varda quest, skipping other quest vault attempts this level");
        }
        else
        {
            log_trace("Quest vault: initiated quest cap reached (%d/%d), skipping quest vault check",
                      quest_initiated_count_this_run(), QUEST_MAX_INITIATED_PER_RUN);
        }
    }

    /* Seed a handful of prefab anchors up front to diversify layout */
    level_gen_screen_set_stage(LEVEL_GEN_STAGE_SHAPING,
        "Generating partitions, rooms, and special areas.");
    seed_prefab_anchors();
    /* Apply quadrant generation modes - this is now the primary room generation */
    apply_quadrant_generation_modes();
    /* DISABLED: ensure_partition_connectivity() was creating dead-end corridors.
     * The corridor system and rescue tunnels handle connectivity instead. */
    /* Repair all outer walls - critical fix for tunnel connectivity after overlapping generation */
    repair_all_outer_walls();

    if (!morgoth_level_active && cached_gv_level_roll_allowed
        && (g_vault_name[0] == '\0'))
    {
        genlog_fail("GV FAILED: reserved a greater vault for this level but '%s' was not placed",
            level_gen_debug_last_greater_vault_name[0]
                ? level_gen_debug_last_greater_vault_name
                : "(unknown)");
        return false;
    }

    /* Verify Morgoth's throne room was placed (should have been done in apply_quadrant_generation_modes) */
    if (morgoth_level_active && !morgoth_partition_reserved)
    {
        log_trace("Morgoth level: throne room was not placed during partition generation");
        return false;
    }

    /* Room saturation loop DISABLED - partition system handles room generation
     * The old approach saturated the map with random rooms which conflicted with
     * the partition-based generation that already creates themed areas. */
#if 0
    /* Build some rooms */
    int failed_in_row = 0;
    for (i = 0; i < room_attempts; i++)
    {
        int r = dieroll(p_ptr->depth + 5);
        log_trace("Room generation: depth+5 roll = %d", r);

        if (one_in_(5))
        {
            int bonus = dieroll(5);
            r += bonus;
            log_trace("Room generation: bonus roll (+%d) = %d total", bonus, r);
        }

        // choose a room type based on the level (bias toward vaults)
        if ((r < 4) || one_in_(3))
        {
            // standard room
            log_trace("Room generation: Building standard room (r=%d)", r);
            if (!room_build(1))
                failed_in_row++;
            else
                failed_in_row = 0;
        }
        else if (r < 7)
        {
            // cross room
            log_trace("Room generation: Building cross room (r=%d)", r);
            if (!room_build(2))
                failed_in_row++;
            else
                failed_in_row = 0;
        }
        else if ((r < 14) || one_in_(2))
        {
            // interesting room
            log_trace("Room generation: Building interesting room (r=%d)", r);
            if (!room_build(6))
                failed_in_row++;
            else
                failed_in_row = 0;
        }
        else if (r < 19)
        {
            // lesser vault
            log_trace("Room generation: Building lesser vault (r=%d)", r);
            if (!room_build(7))
                failed_in_row++;
            else
                failed_in_row = 0;
        }
        else
        {
            // greater vault
            log_trace("Room generation: Building greater vault (r=%d)", r);
            if (!room_build(8))
                failed_in_row++;
            else
                failed_in_row = 0;
        }

        // stop if there are too many rooms
        if (dun->cent_n >= room_capacity_limit())
            break;

        // bail out if we are not making progress to avoid infinite loops
        if (failed_in_row > 200)
        {
            log_trace("Room generation: aborting after %d consecutive failures (cent_n=%d)", failed_in_row, dun->cent_n);
            break;
        }
    }
#endif

    /*set the permanent walls*/
    set_perm_boundry();

    /* Post-partition seeders DISABLED - partition system already handles these
     * CA blob and BSP slice anchors were duplicating work the partitions do */
#if 0
    /* Carve CA blob anchors into remaining granite */
    seed_ca_blob_anchors();
    /* Add BSP-slice anchors for rectangular-but-offset caverns */
    seed_bsp_slice_anchors();
#endif

    /* If generation stalled, force a couple of simple rooms to avoid regen loops */
    ensure_minimum_rooms();

    layout_anchor_capture_existing_rooms();

    /* Log final room count for debugging */
    log_trace("Room generation completed: %d rooms generated (quest_vault_placed=%s)",
              dun->cent_n, qv_placed_this_level ? "true" : "false");

    /*start over on all levels with less than two rooms due to inevitable
     * crash*/
    /* QUEST VAULT FIX: Use original room requirement, quest vault regeneration will be handled differently */
    if (dun->cent_n < ROOM_MIN)
    {
        if (cheat_room)
            msg_format("Not enough rooms (%d < %d).", dun->cent_n, ROOM_MIN);
        if (p_ptr->force_forge)
            p_ptr->fixed_forge_count--;
        log_trace("Level generation failed: Only %d rooms generated, minimum %d required", dun->cent_n, ROOM_MIN);
        genlog_fail("NOT ENOUGH ROOMS: %d generated, minimum %d required", dun->cent_n, ROOM_MIN);
        gen_log_level_end(false, dun->cent_n, 1);
        return (false);
    }

    /* DEBUGGING: Check if quest vault still exists after room generation */
    check_quest_vault_integrity("AFTER_ROOM_GENERATION");

    /* make the tunnels */
    /* Sil - This has been changed considerably */
    level_gen_screen_set_stage(LEVEL_GEN_STAGE_LINKING,
        "Connecting rooms and validating access.");
    if (!connect_rooms_stairs())
    {
        if (cheat_room)
            msg_format("Couldn't connect the rooms.");
        if (p_ptr->force_forge)
            p_ptr->fixed_forge_count--;
        log_trace("Level generation failed: connect_rooms_stairs() returned false");
        genlog_fail("CONNECTIVITY FAILED: connect_rooms_stairs() could not link rooms (rooms=%d)", dun->cent_n);
        gen_log_level_end(false, dun->cent_n, 1);
        return (false);
    }

    /* DEBUGGING: Check if quest vault still exists after tunnel making */
    check_quest_vault_integrity("AFTER_TUNNEL_GENERATION");

    if (morgoth_level_active && !connect_morgoth_entry_tunnels())
    {
        if (cheat_room)
            msg_format("Morgoth entry tunnels failed to connect.");
        if (p_ptr->force_forge)
            p_ptr->fixed_forge_count--;
        log_trace("Level generation failed: connect_morgoth_entry_tunnels() returned false");
        gen_log_level_end(false, dun->cent_n, 1);
        return (false);
    }

    /* randomise the doors (except those in vaults) */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if ((cave_feat[y][x] == FEAT_DOOR_HEAD)
                && !(cave_info[y][x] & (CAVE_ICKY)))
            {
                if (one_in_(4))
                    cave_set_feat(y, x, FEAT_FLOOR);
                else
                    place_random_door(y, x);
            }
        }
    squash_double_doors();
    prune_invalid_nonvault_doors();

    if (morgoth_level_active)
    {
        for (y = 0; y < p_ptr->cur_map_hgt; y++)
        {
            for (x = 0; x < p_ptr->cur_map_wid; x++)
            {
                if ((cave_feat[y][x] == FEAT_MORE)
                    || (cave_feat[y][x] == FEAT_MORE_SHAFT))
                {
                    cave_set_feat(y, x, FEAT_LESS);
                }
            }
        }
    }

    /* DEBUGGING: Check if quest vault still exists after door randomization */
    check_quest_vault_integrity("AFTER_DOOR_RANDOMIZATION");

    /* place the stairs, traps, rubble, secret doors, and player */
    level_gen_screen_set_stage(LEVEL_GEN_STAGE_ENTRY,
        "Placing stairs, rubble, doors, and player start.");
    if (!place_rubble_player())
    {
        if (cheat_room)
            msg_format("Couldn't place, rubble, or player.");
        if (p_ptr->force_forge)
            p_ptr->fixed_forge_count--;
        log_trace("Level generation failed: place_rubble_player() returned false");
        genlog_fail("PLACEMENT FAILED: place_rubble_player() could not place stairs/player");
        gen_log_level_end(false, dun->cent_n, 1);
        return (false);
    }

    if (p_ptr->depth == 1 && p_ptr->stairs_taken == 0)
        make_patches_of_sunlight();

    // check dungeon connectivity
    if (!check_connectivity())
    {
        if (cheat_room)
            msg_format("Failed connectivity.");
        if (p_ptr->force_forge)
            p_ptr->fixed_forge_count--;
        log_trace("Level generation failed: check_connectivity() returned false");
        gen_log_level_end(false, dun->cent_n, 1);
        return (false);
    }

    prune_invalid_nonvault_doors();

    {
        partition_population_plan plans[PARTITION_META_MAX];
        int plan_count = build_partition_population_plans(plans, PARTITION_META_MAX);
        int obj_corr_gen = 0;
        int special_scatter_placed;
        int room_objects_placed;
        int corr_objects_placed;
        int monsters_placed;

        obj_room_gen = 0;
        mon_gen = 0;

        level_gen_screen_set_stage(LEVEL_GEN_STAGE_TREASURE,
            "Placing objects, treasure, and traps.");

        for (int pi = 0; pi < plan_count; ++pi)
        {
            obj_room_gen += plans[pi].room_objects;
            obj_corr_gen += plans[pi].corr_objects;
            if (!partition_monster_pass_skips_plan(&plans[pi]))
                mon_gen += plans[pi].monsters_total;

            log_trace(
                "Partition plan: pi=%d mode=%d rooms=%d floors=%d room_floors=%d corridor_floors=%d base_mon=%d floor_mon=%d depth_mon=%d precurse_mon=%d curse_mon=%d total_mon=%d room_obj=%d corr_obj=%d",
                plans[pi].pi, plans[pi].mode, plans[pi].room_centers,
                plans[pi].floor_count, plans[pi].room_floor_count,
                plans[pi].corridor_floor_count, plans[pi].monsters_base,
                plans[pi].monsters_floor, plans[pi].monsters_depth,
                plans[pi].monsters_precurse, plans[pi].monsters_curse_bonus,
                plans[pi].monsters_total, plans[pi].room_objects,
                plans[pi].corr_objects);
        }

        special_scatter_placed = run_partition_special_scatter_pass(plans, plan_count);
        room_objects_placed = run_partition_object_pass(plans, plan_count, true);
        corr_objects_placed = run_partition_object_pass(plans, plan_count, false);

        log_trace("Room objects: target=%d placed=%d", obj_room_gen, room_objects_placed);
        log_trace("Corridor objects: target=%d placed=%d", obj_corr_gen, corr_objects_placed);
        log_trace("Special scatter placements: %d", special_scatter_placed);

        /* Keep trap placement ahead of monsters, matching the old occupancy order. */
        place_traps();

        level_gen_screen_set_stage(LEVEL_GEN_STAGE_MONSTERS,
            "Placing the monster population.");
        monsters_placed = run_partition_monster_pass(plans, plan_count);
        log_trace("Partition monster pass: target=%d placed=%d", mon_gen, monsters_placed);
    }

    level_gen_screen_set_stage(LEVEL_GEN_STAGE_FINALIZING,
        "Final quest, boss, and success checks.");

    /* Check for Varda quest spawning - lottery-based */
    log_trace("Varda spawn check: lottery_winner=%d (QUEST_ID_VARDA=%d), depth=%d, varda_quest=%d",
              quest_lottery_winner, QUEST_ID_VARDA, p_ptr->depth, p_ptr->varda_quest);

    if (quest_lottery_winner == QUEST_ID_VARDA) { /* Varda is quest ID 6 */
        log_trace("Varda spawn: === VARDA WON LOTTERY === Attempting spawn at depth %d", p_ptr->depth);
        log_trace("Varda spawn: Current state - varda_quest=%d, initiated=%d/%d",
                  p_ptr->varda_quest, quest_initiated_count_this_run(), QUEST_MAX_INITIATED_PER_RUN);

        /* Safety: enforce early-depth requirement even if data is misconfigured */
        if (p_ptr->depth > 3) {
            log_trace("Varda spawn: FAILED - depth %d exceeds allowed range 1-3", p_ptr->depth);
            genlog_quest("VARDA SPAWN FAILED: depth %d > 3, forcing regeneration", p_ptr->depth);
            gen_log_level_end(false, dun->cent_n, 1);
            return false; /* Force regeneration until early depth is honored */
        }

        /* Ensure there is at least some sunlight on the level */
        log_trace("Varda spawn: Ensuring sunlight exists on level");
        ensure_sunlight_for_varda();
        log_trace("Varda spawn: Sunlight check complete");

        /* Check if Varda already exists on this level */
        log_trace("Varda spawn: Checking if Varda already exists on this level (mon_max=%d)", mon_max);
        bool varda_exists = false;
        for (int j = 1; j < mon_max; j++)
        {
            monster_type *m_ptr = &mon_list[j];
            if (m_ptr->r_idx == R_IDX_VARDA)
            {
                varda_exists = true;
                log_trace("Varda spawn: Found existing Varda at monster index %d", j);
                break;
            }
        }

        if (!varda_exists)
        {
            log_trace("Varda spawn: No existing Varda found, attempting placement");
             bool varda_spawned = false;

             int try_y = -1;
             int try_x = -1;
             int total_sunlight = 0;
            int empty_sunlight = 0;
            int spawnable_sunlight = pick_varda_sunlight_spawn_tile(&try_y, &try_x, &total_sunlight, &empty_sunlight);

            log_trace("Varda spawn: Sunlight tiles total=%d, empty=%d, spawnable=%d",
                total_sunlight, empty_sunlight, spawnable_sunlight);

            if (spawnable_sunlight == 0) {
                log_trace("Varda spawn: No spawnable sunlight tiles available, forcing a sunlit tile");
                if (force_varda_sunlight_tile(&try_y, &try_x)) {
                    spawnable_sunlight = 1;
                }
            }

            if (spawnable_sunlight > 0) {
                if (place_monster_one(try_y, try_x, R_IDX_VARDA, true, true, NULL)) {
                    varda_spawned = true;
                } else {
                    log_trace("Varda spawn: Primary sunlight tile rejected, scanning for fallback");

                    int access[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
                    for (int y = 0; y < p_ptr->cur_map_hgt; y++)
                    {
                        for (int x = 0; x < p_ptr->cur_map_wid; x++)
                        {
                            access[y][x] = false;
                        }
                    }
                    flood_access(p_ptr->py, p_ptr->px, access, false);

                    for (int y = 1; y < p_ptr->cur_map_hgt - 1 && !varda_spawned; y++) {
                        for (int x = 1; x < p_ptr->cur_map_wid - 1 && !varda_spawned; x++) {
                            if (!varda_sunlight_tile_ok(y, x, true)) continue;
                            if (!varda_no_rubble_path_tile_ok(y, x, access)) continue;
                            if (place_monster_one(y, x, R_IDX_VARDA, true, true, NULL)) {
                                try_y = y;
                                try_x = x;
                                varda_spawned = true;
                            }
                        }
                    }
                }
            }

            if (varda_spawned) {
                p_ptr->varda_quest = VARDA_QUEST_GIVER_PRESENT;
                quest_note_initiated(QUEST_ID_VARDA);
                p_ptr->varda_level = p_ptr->depth;
                level_gen_debug_note_questgiver(QUEST_ID_VARDA);
                log_trace("Varda spawn: === SUCCESS === Placed at (%d,%d) on sunlight tile", try_y, try_x);
                log_trace("Varda spawn: Quest state set to GIVER_PRESENT (%d)", p_ptr->varda_quest);
            }

            if (!varda_spawned)
            {
                log_trace("Varda spawn: === FAILED === Could not find valid sunlight tile after forcing - REGENERATING LEVEL");
                genlog_fail("VARDA SPAWN FAILED: could not find valid sunlight tile after forcing - regenerating");
                gen_log_level_end(false, dun->cent_n, 1);
                return false; /* Force regeneration to honor 100% spawn guarantee */
            }
        }
        else
        {
            log_trace("Varda spawn: Varda already present on level, skipping placement");
        }
    } else {
        log_trace("Varda spawn: SKIPPED - did not win lottery (winner=%d)", quest_lottery_winner);
    }

    /* Check for Tulkas quest spawning - only if it won the lottery */
    int tulkas_completions = metarun_quest_completion_count(METARUN_QUEST_TULKAS);
    log_trace("Tulkas spawn check: quest=%d, depth=%d, metarun_completions=%d, lottery_winner=%d",
             p_ptr->tulkas_quest, p_ptr->depth, tulkas_completions, quest_lottery_winner);

    /* Only attempt Tulkas spawning if it won the lottery */
    if (quest_lottery_winner == 1) { /* Tulkas is quest ID 1 */
        log_trace("Tulkas spawn: Tulkas WON the lottery - attempting spawn");

        /* Try to find a room to spawn Tulkas in */
        int attempts;
        bool tulkas_spawned = false;

        log_trace("Tulkas spawn: Lottery winner attempting placement at depth %d", p_ptr->depth);

        /* Check if Tulkas already exists on this level */
        bool tulkas_exists = false;
        int j;
        for (j = 1; j < mon_max; j++)
        {
            monster_type *m_ptr = &mon_list[j];
            if (m_ptr->r_idx == R_IDX_TULKAS)
                {
                    tulkas_exists = true;
                    break;
                }
            }

            if (!tulkas_exists)
            {
                /* Try to spawn Tulkas near the player's starting room */
                int player_y = p_ptr->py;
                int player_x = p_ptr->px;

                /* Try to find a spot in the same room as the player first */
                for (attempts = 0; attempts < 50 && !tulkas_spawned; attempts++)
                {
                    /* Search in a radius around the player */
                    int dy = rand_range(-2, 2);
                    int dx = rand_range(-2, 2);
                    int try_y = player_y + dy;
                    int try_x = player_x + dx;

                    /* Must be valid coordinates, floor in the same room, and not too close to player */
                    if (try_y > 0 && try_y < p_ptr->cur_map_hgt - 1 &&
                        try_x > 0 && try_x < p_ptr->cur_map_wid - 1 &&
                        cave_floor_bold(try_y, try_x) &&
                        (cave_info[try_y][try_x] & CAVE_ROOM) &&
                        !(cave_info[try_y][try_x] & CAVE_ICKY) &&
                        cave_m_idx[try_y][try_x] == 0 &&
                        distance(player_y, player_x, try_y, try_x) >= 2)
                    {
                        if (place_monster_one(try_y, try_x, R_IDX_TULKAS, true, true, NULL))
                        {
                            p_ptr->tulkas_quest = TULKAS_QUEST_GIVER_PRESENT;
                            quest_note_initiated(QUEST_ID_TULKAS);
                            level_gen_debug_note_questgiver(QUEST_ID_TULKAS);
                            tulkas_spawned = true;
                            log_trace("Tulkas spawned near player at (%d, %d), player at (%d, %d), quest state: %d",
                                     try_y, try_x, player_y, player_x, p_ptr->tulkas_quest);
                        }
                    }
                }

                /* If that failed, try any room on the level */
                if (!tulkas_spawned)
                {
                    for (attempts = 0; attempts < 100 && !tulkas_spawned; attempts++)
                    {
                        int room_y = rand_int(p_ptr->cur_map_hgt);
                        int room_x = rand_int(p_ptr->cur_map_wid);

                        /* Must be a floor in a room, not in a vault/interesting room */
                        if (cave_floor_bold(room_y, room_x) &&
                            (cave_info[room_y][room_x] & CAVE_ROOM) &&
                            !(cave_info[room_y][room_x] & CAVE_ICKY) &&
                            cave_m_idx[room_y][room_x] == 0)
                        {
                            if (place_monster_one(room_y, room_x, R_IDX_TULKAS, true, true, NULL))
                            {
                                p_ptr->tulkas_quest = TULKAS_QUEST_GIVER_PRESENT;
                                quest_note_initiated(QUEST_ID_TULKAS);
                                level_gen_debug_note_questgiver(QUEST_ID_TULKAS);
                                tulkas_spawned = true;
                                log_trace("Tulkas spawned in fallback room at (%d, %d), quest state: %d",
                                         room_y, room_x, p_ptr->tulkas_quest);
                            }
                        }
                    }
                }

                if (!tulkas_spawned)
                {
                    log_trace("Failed to spawn Tulkas in room after all attempts");
                }
            }
            else
            {
                log_trace("Tulkas already exists on level, skipping room spawn");
            }
    }

    /* Check for Nienna room-based spawning - LOTTERY SYSTEM */
    int niena_completions = metarun_quest_completion_count(METARUN_QUEST_NIENA);
    log_trace("Nienna spawn check: quest=%d, depth=%d, level_size_l=%d, metarun_completions=%d, lottery_winner=%d",
             p_ptr->niena_quest, p_ptr->depth, l, niena_completions, quest_lottery_winner);

    /* Only attempt Nienna spawning if it won the lottery */
    if (quest_lottery_winner == 4) { /* Nienna is quest ID 4 */
        log_trace("Nienna spawn: Nienna WON the lottery - attempting spawn");

        /* Check level size requirement: must be maximum size (l >= 5) */
        if (l < 5) {
            log_trace("Nienna spawn: FAILED - level too small (l=%d, need l>=5)", l);
            genlog_quest("NIENA SPAWN FAILED: level size %d < 5, forcing regeneration", l);
            gen_log_level_end(false, dun->cent_n, 1);
            return false; /* Force regeneration until we get a big enough level */
        }

        /* Nienna's challenge is tied to the player's actual start, not the
         * closest arbitrary up/down pair somewhere else on the level. */
        int min_stair_dist = calculate_nearest_down_stair_distance_from(p_ptr->py, p_ptr->px);
        log_trace("Nienna spawn: Calculated player-to-nearest-down distance = %d", min_stair_dist);

        if (min_stair_dist < 87) {
            log_trace("Nienna spawn: FAILED - nearest down stair too close to player start (distance=%d, need >=87)", min_stair_dist);
            genlog_quest("NIENA SPAWN FAILED: player-to-down distance %d < 87, forcing regeneration", min_stair_dist);
            gen_log_level_end(false, dun->cent_n, 1);
            return false; /* Force regeneration until stairs are far enough apart */
        }

        log_trace("Nienna spawn: Distance check PASSED (player-to-down=%d >= 87)", min_stair_dist);

        /* Try to find a room to spawn Nienna in near the up stairs */
        int attempts;
        bool niena_spawned = false;

        log_trace("Nienna spawn: Lottery winner attempting placement at depth %d, level_size=%d, stair_distance=%d",
                  p_ptr->depth, l, min_stair_dist);

        /* Check if Nienna already exists on this level */
        bool niena_exists = false;
        int j;
        for (j = 1; j < mon_max; j++)
        {
            monster_type *m_ptr = &mon_list[j];
            if (m_ptr->r_idx == R_IDX_NIENA)
            {
                niena_exists = true;
                break;
            }
        }

        if (!niena_exists)
        {
            /* Try to spawn Nienna near the player's starting position (up stairs) */
            int player_y = p_ptr->py;
            int player_x = p_ptr->px;

            log_trace("Nienna spawn: Attempting to place near player at (%d,%d)", player_y, player_x);

            /* Verify player has valid coordinates */
            if (player_y > 0 && player_y < p_ptr->cur_map_hgt - 1 &&
                player_x > 0 && player_x < p_ptr->cur_map_wid - 1)
            {
                /* Try to find a spot in the same room as the player first */
                for (attempts = 0; attempts < 50 && !niena_spawned; attempts++)
                {
                    /* Search in a radius around the player */
                    int dy = rand_range(-2, 2);
                    int dx = rand_range(-2, 2);
                    int try_y = player_y + dy;
                    int try_x = player_x + dx;

                    /* Must be valid coordinates, floor in the same room, and not too close to player */
                    if (try_y > 0 && try_y < p_ptr->cur_map_hgt - 1 &&
                        try_x > 0 && try_x < p_ptr->cur_map_wid - 1 &&
                        cave_floor_bold(try_y, try_x) &&
                        (cave_info[try_y][try_x] & CAVE_ROOM) &&
                        !(cave_info[try_y][try_x] & CAVE_ICKY) &&
                        cave_m_idx[try_y][try_x] == 0 &&
                        distance(player_y, player_x, try_y, try_x) >= 2 &&
                        los(player_y, player_x, try_y, try_x))
                    {
                        if (place_monster_one(try_y, try_x, R_IDX_NIENA, true, true, NULL))
                        {
                            p_ptr->niena_quest = NIENA_QUEST_GIVER_PRESENT;
                            quest_note_initiated(QUEST_ID_NIENA);
                            level_gen_debug_note_questgiver(QUEST_ID_NIENA);
                            niena_spawned = true;
                            log_trace("Nienna spawned near player at (%d, %d), player at (%d, %d), quest state: %d",
                                     try_y, try_x, player_y, player_x, p_ptr->niena_quest);
                        }
                    }
                }
            }
            else
            {
                log_trace("Nienna spawn: Invalid player coordinates (%d,%d), falling back to any room", player_y, player_x);
            }

            /* If that failed, try any room on the level */
            if (!niena_spawned)
            {
                log_trace("Nienna spawn: Near-player placement failed, trying any suitable room");
                for (attempts = 0; attempts < 100 && !niena_spawned; attempts++)
                {
                    int room_y = rand_int(p_ptr->cur_map_hgt);
                    int room_x = rand_int(p_ptr->cur_map_wid);

                    /* Must be valid coordinates and a floor in a room */
                    if (room_y > 0 && room_y < p_ptr->cur_map_hgt - 1 &&
                        room_x > 0 && room_x < p_ptr->cur_map_wid - 1 &&
                        cave_floor_bold(room_y, room_x) &&
                        (cave_info[room_y][room_x] & CAVE_ROOM) &&
                        !(cave_info[room_y][room_x] & CAVE_ICKY) &&
                        cave_m_idx[room_y][room_x] == 0)
                    {
                        if (place_monster_one(room_y, room_x, R_IDX_NIENA, true, true, NULL))
                        {
                            p_ptr->niena_quest = NIENA_QUEST_GIVER_PRESENT;
                            quest_note_initiated(QUEST_ID_NIENA);
                            level_gen_debug_note_questgiver(QUEST_ID_NIENA);
                            niena_spawned = true;
                            log_trace("Nienna spawned in fallback room at (%d, %d), quest state: %d",
                                     room_y, room_x, p_ptr->niena_quest);
                        }
                    }
                }
            }

            if (!niena_spawned)
            {
                log_trace("Nienna spawn: Random placement failed, scanning the full level for a valid room tile");
                for (int scan_y = 1; scan_y < p_ptr->cur_map_hgt - 1 && !niena_spawned; scan_y++)
                {
                    for (int scan_x = 1; scan_x < p_ptr->cur_map_wid - 1 && !niena_spawned; scan_x++)
                    {
                        if (!cave_floor_bold(scan_y, scan_x))
                            continue;
                        if (!(cave_info[scan_y][scan_x] & CAVE_ROOM))
                            continue;
                        if (cave_info[scan_y][scan_x] & CAVE_ICKY)
                            continue;
                        if (cave_m_idx[scan_y][scan_x] != 0)
                            continue;
                        if (distance(player_y, player_x, scan_y, scan_x) < 2)
                            continue;

                        if (place_monster_one(scan_y, scan_x, R_IDX_NIENA, true, true, NULL))
                        {
                            p_ptr->niena_quest = NIENA_QUEST_GIVER_PRESENT;
                            quest_note_initiated(QUEST_ID_NIENA);
                            level_gen_debug_note_questgiver(QUEST_ID_NIENA);
                            niena_spawned = true;
                            log_trace("Nienna spawned by exhaustive scan at (%d, %d), quest state: %d",
                                scan_y, scan_x, p_ptr->niena_quest);
                        }
                    }
                }
            }

            if (!niena_spawned)
            {
                log_trace("Nienna spawn: FAILED to spawn after all attempts - forcing regeneration");
                genlog_fail("NIENA SPAWN FAILED: could not place monster after all attempts - regenerating");
                gen_log_level_end(false, dun->cent_n, 1);
                return false; /* Force regeneration */
            }
        }
        else
        {
            log_trace("Nienna already exists on level, skipping room spawn");
        }
    } else {
        log_trace("Nienna spawn: SKIPPED - did not win lottery (winner=%d)", quest_lottery_winner);
    }

    /* Check for Oromë quest spawning - only if it won the lottery */
    int orome_completions = metarun_quest_completion_count(METARUN_QUEST_OROME);
    bool orome_blocked = quest_metarun_blocked(QUEST_ID_OROME, METARUN_QUEST_OROME);
    log_trace("Oromë spawn check: quest=%d, depth=%d, metarun_completions=%d, lottery_winner=%d, blocked=%s",
             p_ptr->orome_quest, p_ptr->depth,
             orome_completions,
             quest_lottery_winner,
             orome_blocked ? "yes" : "no");

    /* Only attempt Oromë spawning if it won the lottery and isn't blocked by metarun history */
    if (orome_blocked) {
        log_trace("Oromë spawn: blocked by metarun state (requires active oath or under cap)");
        quest_lottery_winner = 0; /* Treat level as quest-free if history blocks this quest */
    } else if (quest_lottery_winner == 5) { /* Oromë is quest ID 5 */
        log_trace("Oromë spawn: Oromë WON the lottery - attempting spawn");

        /* Try to find a room to spawn Oromë in */
        int attempts;
        bool orome_spawned = false;

        log_trace("Oromë spawn: Lottery winner attempting placement at depth %d", p_ptr->depth);

        /* Check if Oromë already exists on this level */
        bool orome_exists = false;
        int j;
        for (j = 1; j < mon_max; j++)
        {
            monster_type *m_ptr = &mon_list[j];
            if (m_ptr->r_idx == R_IDX_OROME)
            {
                orome_exists = true;
                break;
            }
        }

        if (!orome_exists)
        {
            /* Try to spawn Oromë near the player's starting room */
            int player_y = p_ptr->py;
            int player_x = p_ptr->px;

            /* Try to find a spot in the same room as the player first */
            for (attempts = 0; attempts < 50 && !orome_spawned; attempts++)
            {
                /* Search in a radius around the player */
                int dy = rand_range(-2, 2);
                int dx = rand_range(-2, 2);
                int try_y = player_y + dy;
                int try_x = player_x + dx;

                /* Must be valid coordinates, floor in the same room, and not too close to player */
                if (try_y > 0 && try_y < p_ptr->cur_map_hgt - 1 &&
                    try_x > 0 && try_x < p_ptr->cur_map_wid - 1 &&
                    cave_floor_bold(try_y, try_x) &&
                    (cave_info[try_y][try_x] & CAVE_ROOM) &&
                    !(cave_info[try_y][try_x] & CAVE_ICKY) &&
                    cave_m_idx[try_y][try_x] == 0 &&
                    distance(player_y, player_x, try_y, try_x) >= 2 &&
                    los(player_y, player_x, try_y, try_x))
                {
                    if (place_monster_one(try_y, try_x, R_IDX_OROME, true, true, NULL))
                    {
                        p_ptr->orome_quest = OROME_QUEST_GIVER_PRESENT;
                        quest_note_initiated(QUEST_ID_OROME);
                        level_gen_debug_note_questgiver(QUEST_ID_OROME);
                        orome_spawned = true;
                        log_trace("Oromë spawned near player at (%d, %d), player at (%d, %d), quest state: %d",
                                 try_y, try_x, player_y, player_x, p_ptr->orome_quest);
                    }
                }
            }

            /* If that failed, try any room on the level */
            if (!orome_spawned)
            {
                for (attempts = 0; attempts < 100 && !orome_spawned; attempts++)
                {
                    int room_y = rand_int(p_ptr->cur_map_hgt);
                    int room_x = rand_int(p_ptr->cur_map_wid);

                    /* Must be a floor in a room, not in a vault/interesting room */
                    if (cave_floor_bold(room_y, room_x) &&
                        (cave_info[room_y][room_x] & CAVE_ROOM) &&
                        !(cave_info[room_y][room_x] & CAVE_ICKY) &&
                        cave_m_idx[room_y][room_x] == 0)
                    {
                        if (place_monster_one(room_y, room_x, R_IDX_OROME, true, true, NULL))
                        {
                            p_ptr->orome_quest = OROME_QUEST_GIVER_PRESENT;
                            quest_note_initiated(QUEST_ID_OROME);
                            level_gen_debug_note_questgiver(QUEST_ID_OROME);
                            orome_spawned = true;
                            log_trace("Oromë spawned in fallback room at (%d, %d), quest state: %d",
                                     room_y, room_x, p_ptr->orome_quest);
                        }
                    }
                }
            }

            if (!orome_spawned)
            {
                log_trace("Oromë spawn: FAILED - could not place monster after 150 attempts");
                genlog_fail("OROME SPAWN FAILED: could not place monster after 150 attempts - regenerating");
                gen_log_level_end(false, dun->cent_n, 1);
                return false; /* Force regeneration */
            }
        }
        else
        {
            log_trace("Oromë already exists on level, skipping room spawn");
        }
    } else {
        log_trace("Oromë spawn: SKIPPED - did not win lottery (winner=%d)", quest_lottery_winner);
    }

    // place Morgoth if on the run
    if (p_ptr->on_the_run && !p_ptr->morgoth_slain)
    {
        static int morgoth_path_dist[MAX_DUNGEON_HGT][MAX_DUNGEON_WID];
        bool placed = false;
        int placed_y = 0;
        int placed_x = 0;
        int sils = silmarils_possessed();
        int max_dist = 50 - (sils * 8);
        int min_dist = 9 - sils;
        int max_path = 3 * (p_ptr->cur_map_hgt + p_ptr->cur_map_wid);

        if (max_dist < min_dist + 2)
            max_dist = min_dist + 2;

        build_morgoth_escape_path_distances(morgoth_path_dist, max_path);

        /* Prefer spawning within a chase radius scaled by Silmarils. */
        for (i = 0; i <= 180; i++)
        {
            int dy = rand_range(-max_dist, max_dist);
            int dx = rand_range(-max_dist, max_dist);
            int dist = ABS(dy) + ABS(dx);

            if (dist < min_dist || dist > max_dist)
                continue;

            y = p_ptr->py + dy;
            x = p_ptr->px + dx;

            if (!in_bounds_fully(y, x))
                continue;
            if (!cave_empty_bold(y, x))
                continue;
            if (cave_info[y][x] & (CAVE_ICKY))
                continue;
            if (!morgoth_escape_spawn_path_ok(y, x, dist, morgoth_path_dist))
                continue;

            if (place_monster_one(y, x, R_IDX_MORGOTH, false, true, NULL))
            {
                placed = true;
                placed_y = y;
                placed_x = x;
                break;
            }
        }

        if (!placed)
        {
            for (int pass = 0; pass < 2 && !placed; pass++)
            {
                for (y = 1; y < p_ptr->cur_map_hgt - 1 && !placed; ++y)
                {
                    for (x = 1; x < p_ptr->cur_map_wid - 1 && !placed; ++x)
                    {
                        int dist = ABS(y - p_ptr->py) + ABS(x - p_ptr->px);

                        if (dist < min_dist)
                            continue;
                        if ((pass == 0) && (dist > max_dist))
                            continue;
                        if (!cave_empty_bold(y, x))
                            continue;
                        if (cave_info[y][x] & (CAVE_ICKY))
                            continue;
                        if (!morgoth_escape_spawn_path_ok(
                                y, x, dist, morgoth_path_dist))
                            continue;

                        if (place_monster_one(
                                y, x, R_IDX_MORGOTH, false, true, NULL))
                        {
                            placed = true;
                            placed_y = y;
                            placed_x = x;
                        }
                    }
                }
            }
        }

        if (placed && cave_m_idx[placed_y][placed_x] > 0)
        {
            monster_type* m_ptr = &mon_list[cave_m_idx[placed_y][placed_x]];
            if (m_ptr->r_idx == R_IDX_MORGOTH)
            {
                if (m_ptr->alertness < ALERTNESS_ALERT)
                    m_ptr->alertness = ALERTNESS_ALERT;
                m_ptr->min_range = 0;
            }
        }
        else if (!placed)
        {
            log_trace("Morgoth spawn: FAILED to place Morgoth while on the run (depth=%d, min_dist=%d, max_dist=%d)",
                p_ptr->depth, min_dist, max_dist);
        }
    }
    p_ptr->force_forge = false;

    /* Level generation successful - log completion */
    genlog_summary("Level %d generation COMPLETE: %d rooms, quest_lottery=%d",
                   p_ptr->depth, dun->cent_n, quest_lottery_winner);
    gen_log_level_end(true, dun->cent_n, 1);
    gen_log_flush();

    return (true);
}

/*
 * Create the gates to Angband level
 */
void gates_gen(void)
{
    int y, x;
    int i;
    int py = -1, px = -1;

    memset(dun, 0, sizeof(*dun));
    layout_anchor_reset();
    reset_morgoth_layout_state(false);
    current_partition_rows = 0;
    current_partition_cols = 0;
    current_partition_count = 0;
    current_labyrinth_partitions = 0;
    reset_partition_population_metadata();
    for (i = 0; i < PARTITION_META_MAX; ++i)
    {
        current_partition_modes[i] = QUAD_MODE_ROOMY;
        current_partition_densities[i] = DENSITY_NORMAL;
        current_partition_big_cave_types[i] = BIG_CAVE_NONE;
    }

    /* Restrict to single-screen size */
    p_ptr->cur_map_hgt = (3 * PANEL_HGT);
    p_ptr->cur_map_wid = (2 * PANEL_WID_FIXED);

    /* Initialize level style weights for depth 0 */
    styles_init_for_level();
    /* If no primary style was selected (e.g., no rules loaded yet), force style 13 */
    if (styles_get_level_primary_style() < 0) {
        styles_set_loaded_level_primary(13);
        log_info("gates_gen: forced level primary style to 13 for depth 0");
    }

    /*start with basic granite*/
    basic_granite();

    /*set the permanent walls*/
    set_perm_boundry();

    if (!build_type10(17, 33))
    {
        log_error("gates_gen: failed to build Gates of Angband vault");
    }

    /* Find an up staircase */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            if (cave_feat[y][x] == FEAT_MORE)
            {
                py = y;
                px = x;
            }
        }
    }

    if ((py < 0) || (px < 0))
    {
        msg_format("Failed to find a down staircase in the gates level");
        py = p_ptr->cur_map_hgt / 2;
        px = p_ptr->cur_map_wid / 2;
        for (y = py - 1; y <= py + 1; ++y)
        {
            for (x = px - 1; x <= px + 1; ++x)
            {
                if (!in_bounds(y, x))
                    continue;
                cave_set_feat(y, x, FEAT_FLOOR);
            }
        }
        cave_set_feat(py, px, FEAT_MORE);
    }

    /* Delete any monster on the starting square */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Only get the monster on the same square */
        if ((m_ptr->fy != py) || (m_ptr->fx != px))
            continue;

        /* Delete the monster */
        delete_monster_idx(i);
    }

    /* Place the player */
    player_place(py, px);
}

/*
 * Create the level containing Morgoth's throne room
 */
#if 0
void throne_gen(void)
{
    int y, x;
    int i;
    int py = 0, px = 0;

    // display the throne poetry
    pause_with_text(throne_poetry, 5, 13, NULL, 0);

    // set the 'truce' in action
    p_ptr->truce = true;

    /* Restrict to single-screen size */
    p_ptr->cur_map_hgt = (3 * PANEL_HGT);
    p_ptr->cur_map_wid = (3 * PANEL_WID_FIXED);

    /*start with basic granite*/
    basic_granite();

    /*set the permanent walls*/
    set_perm_boundry();

    build_type9(16, 38, NULL);

    /* Find an up staircase */
    for (y = 0; y < p_ptr->cur_map_hgt; y++)
    {
        for (x = 0; x < p_ptr->cur_map_wid; x++)
        {
            // Sil-y: assumes the important staircase is at the centre of the
            // level
            if ((cave_feat[y][x] == FEAT_LESS) && (x >= 30) && (x <= 45))
            {
                py = y;
                px = x;
            }
        }
    }

    if ((py == 0) || (px == 0))
    {
        msg_format("Failed to find an up staircase in the throne-room");
    }

    /* Delete any monster on the starting square */
    for (i = 1; i < mon_max; i++)
    {
        monster_type* m_ptr = &mon_list[i];

        /* Paranoia -- Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Only get the monster on the same square */
        if ((m_ptr->fy != py) || (m_ptr->fx != px))
            continue;

        /* Delete the monster */
        delete_monster_idx(i);
    }

    /* Place the player */
    player_place(py, px);
}
#endif

/*
 * Dungeon generation can set some flags indicating that certain one-off
 * things have happened (artefacts, unique greater vaults, unique forge).
 * But if generation fails, we need to reset these flags.
 *
 * "You can't unring a bell." -- Tom Waits
 */
void unring_a_bell(void)
{
    object_type* o_ptr;
    int y, x, i;

    // look through the dungeon objects for artefacts
    for (i = 1; i < o_max; i++)
    {
        /* Get the object */
        o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        if (o_ptr->name1)
        {
            artefact_type* a_ptr = &a_info[o_ptr->name1];
            a_ptr->cur_num = 0;
        }
    }

    // Look through the map for the unique forge
    for (y = 0; y < p_ptr->cur_map_hgt; ++y)
    {
        for (x = 0; x < p_ptr->cur_map_wid; ++x)
        {
            // Reset the unique forge
            if ((cave_feat[y][x] >= FEAT_FORGE_UNIQUE_HEAD)
                && (cave_feat[y][x] <= FEAT_FORGE_UNIQUE_TAIL))
            {
                p_ptr->unique_forge_made = false;
            }
        }
    }

    /* DEBUGGING: Final check if quest vault still exists at end of generation */
    check_quest_vault_integrity("END_OF_GENERATION");

    // If there is a greater vault...
    if (g_vault_name[0] != '\0')
    {
        // wipe vault name
        g_vault_name[0] = '\0';

        // look for the final greater vault entry
        for (i = 0; i < MAX_GREATER_VAULTS; i++)
        {
            // wipe the final entry
            if (i == MAX_GREATER_VAULTS - 1)
            {
                p_ptr->greater_vaults[i] = 0;
                break;
            }
            else if (p_ptr->greater_vaults[i + 1] == 0)
            {
                p_ptr->greater_vaults[i] = 0;
                break;
            }
        }
    }
}

/*
 * Generate a random dungeon level
 *
 * Hack -- regenerate any "overflow" levels
 *
 * Note that this function resets "cave_feat" and "cave_info" directly.
 */
void generate_cave(void)
{
    int y, x, i;
    bool is_morgoth_level = (p_ptr->depth == MORGOTH_DEPTH);

    log_info("generate_cave: Function entry - about to start");
    log_debug("generate_cave: Starting cave generation");

    /* Reset per-level color cache so depth group re-rolls when entering a new level */
    reset_depth_color_cache();

    /* The dungeon is not ready */
    character_dungeon = false;

    /* Don't know feeling yet */
    do_feeling = 0;

    /*allow uniques to be generated everywhere but in nests/pits*/
    allow_uniques = true;

    /* Never carry the throne-room truce between levels */
    p_ptr->truce = false;

    /* Restrict quest monsters from spawning outside their quest contexts */
    get_mon_num_hook = quest_monster_spawn_okay;

    // display the entry poetry
if (playerturn == 0) {
    char extra[4][100];
    int idx = 0;

    /* Prepare pointers */
    const char *name = c_name + current_character_profile->name;
    const char *alt = c_name + current_character_profile->alt_name;
    const char *start = c_name + current_character_profile->start_string;

    /* Line 1: CharacterName AltName! */
    strnfmt(extra[idx], 100, "%s%s!", name, alt);
    idx++;

    /* Split start string (motto) at first '-' */
    const char *dash_start = strchr(start, '-');
    if (dash_start) {
        size_t prefix_len = (size_t)(dash_start - start + 1);

        /* Line 2: up to and including dash */
        if (prefix_len >= sizeof(extra[idx]))
            prefix_len = sizeof(extra[idx]) - 1;
        memcpy(extra[idx], start, prefix_len);
        extra[idx][prefix_len] = '\0';
        idx++;
        /* Line 3: remainder after dash */
        strnfmt(extra[idx], 100, "%s", dash_start + 1);
        idx++;
    } else {
        /* No dash: all in one line */
        strnfmt(extra[idx], 100, "%s", start);
        idx++;
    }

    /* sentinel */
    extra[idx][0] = '\0';

    /* Display the one-time character intro with the secondary story font. */
    sdl_story_font_set_slot(STORY_FONT_SLOT_SECONDARY);
    pause_with_text(entry_poetry, 4, 13, extra, TERM_YELLOW);
}


    /* Safety check: make sure cave_color is allocated */
    if (!cave_color) {
        log_error("generate_cave: cave_color array is not allocated!");
        return;
    }

    level_gen_screen_begin();
    reset_generation_retry_locks();

    // reset smithing leftover (as there is no access to the old forge)
    p_ptr->smithing_leftover = 0;

    // reset the forced skipping of next turn (a bit rough to miss first turn if
    // you fell down)
    p_ptr->skip_next_turn = false;

    byte preserved_initiated_count = run_quest_initiated_count();

    while (true)
    {
        bool okay = true;
        bool quest_vault_placed_this_attempt = false; /* Track if quest vault placed in this attempt */

        cptr why = NULL;

        level_gen_screen_start_attempt();

        /* QUEST VAULT REGENERATION DEBUG: Log each regeneration attempt */
        log_trace("QUEST VAULT FIX: Starting level generation attempt (quest_vault_used=%d)",
                  p_ptr->quest_vault_used);

        /* Reset pending quest state changes at the start of each generation attempt */
        reset_pending_quest_states();

        /* Reset quest states that may have been set during previous failed attempts */
        reset_quest_vault_states(preserved_initiated_count);

        /* Paranoia: Check that cave_color is allocated */
        if (!cave_color)
        {
            log_error("cave_color array is not allocated!");
            quit("cave_color array not allocated");
        }

        /* Reset */
        o_max = 1;
        mon_max = 1;
        feeling = 0;

        /* Start with a blank cave */
        for (y = 0; y < MAX_DUNGEON_HGT; y++)
        {
            for (x = 0; x < MAX_DUNGEON_WID; x++)
            {
                /* No flags */
                cave_info[y][x] = 0;

                /* No features */
                cave_feat[y][x] = 0;

                /* No colors (use default) */
                cave_color[y][x] = 0;

                /* No natural cave footprint */
                cave_natural[y][x] = 0;

                /* No rewired traps */
                cave_rewired[y][x] = 0;

                /* No objects */
                cave_o_idx[y][x] = 0;

                /* No monsters */
                cave_m_idx[y][x] = 0;

                for (i = 0; i < MAX_FLOWS; i++)
                {
                    cave_cost[i][y][x] = FLOW_MAX_DIST;
                }

                cave_when[y][x] = 0;
            }
        }

    log_debug("generate_cave: Cave initialization completed successfully");

        // reset the wandering monster pauses
        for (i = 0; i < MAX_FLOWS; i++)
        {
            wandering_pause[i] = 0;
        }

        /* Mega-Hack -- no player yet */
        p_ptr->px = p_ptr->py = 0;

        /* Hack -- illegal panel */
        p_ptr->wy = MAX_DUNGEON_HGT;
        p_ptr->wx = MAX_DUNGEON_WID;

        /* Reset generation depth; the Gates use depth 20 tables while displayed as 0. */
        monster_level = player_generation_depth();
        object_level = player_generation_depth();

        /* Nothing special here yet */
        good_item_flag = false;

        /* Nothing good here yet */
        rating = 0;

        /* Build the gates to Angband */
        if (!p_ptr->depth)
        {
            level_gen_screen_set_stage(LEVEL_GEN_STAGE_FOUNDATIONS,
                "Preparing the Gates.");
            level_gen_screen_set_stage(LEVEL_GEN_STAGE_SHAPING,
                "Building the Gates of Angband.");
            gates_gen();
            level_gen_screen_set_stage(LEVEL_GEN_STAGE_FINALIZING,
                "Final touches on the Gates.");

            /* Hack -- Clear stairs request */
            p_ptr->create_stair = 0;
        }

        /* Build a real level */
        else
        {
            /* Make a dungeon, or report the failure to make one*/
            if (cave_gen())
            {
                okay = true;
                if (is_morgoth_level)
                {
                    /* Depth 20 uses the partition system; keep entry stairs so the player can retreat. */
                }
                /* Check if quest vault was placed during this level generation */
                if (qv_placed_this_level) {
                    quest_vault_placed_this_attempt = true;
                }
                /* Also check if we have pending quest state changes that indicate a quest vault was placed */
                if (pending_quest_states.has_aule_change || pending_quest_states.has_mandos_change || pending_quest_states.has_varda_change) {
                    quest_vault_placed_this_attempt = true;
                }
            }
            else
            {
                okay = false;
            }
        }

        /*message*/
        if (!okay)
        {
            if (cheat_room || cheat_hear || cheat_peek || cheat_xtra)
                why = "defective level";

            // Must reset all the artefacts that were generated on the defective
            // level
            for (i = 1; i < o_max; i++)
            {
                /* Get the object */
                object_type* o_ptr = &o_list[i];

                /* Skip dead objects */
                if (!o_ptr->k_idx)
                    continue;

                /* If artefact. */
                if (o_ptr->name1)
                {
                    /* Reset its count */
                    a_info[o_ptr->name1].cur_num = 0;
                    a_info[o_ptr->name1].found_num = 0;
                }
            }
        }

        else
        {
            /* Extract the feeling */
            if (!feeling)
            {
                if (rating > 100)
                    feeling = 2;
                else if (rating > 80)
                    feeling = 3;
                else if (rating > 60)
                    feeling = 4;
                else if (rating > 40)
                    feeling = 5;
                else if (rating > 30)
                    feeling = 6;
                else if (rating > 20)
                    feeling = 7;
                else if (rating > 10)
                    feeling = 8;
                else if (rating > 0)
                    feeling = 9;
                else
                    feeling = 10;

                /* Hack -- Have a special feeling sometimes */
                if (good_item_flag && !(PRESERVE_MODE))
                    feeling = 1;

                /* Hack -- no feeling at the gates */
                if (!p_ptr->depth)
                    feeling = 0;
            }

            /* Prevent object over-flow */
            if (o_max >= z_info->o_max)
            {
                /* Message */
                why = "too many objects";

                /* Message */
                okay = false;
            }

            /* Prevent monster over-flow */
            if (mon_max >= MAX_MONSTERS)
            {
                /* Message */
                why = "too many monsters";

                /* Message */
                okay = false;
            }
        }

        /* Accept */
        if (okay)
        {
            /* QUEST VAULT REGENERATION FIX: Apply pending quest state changes when level generation is COMPLETELY successful */
            apply_pending_quest_states();

            /* Only count quest vaults when level generation is completely successful. */
            if (quest_vault_placed_this_attempt) {
                if (p_ptr->quest_vault_used < 255) p_ptr->quest_vault_used++;
                log_trace("QUEST VAULT FIX: Level completely successful - quest_vault_used now %d",
                          p_ptr->quest_vault_used);
            } else {
                log_trace("QUEST VAULT FIX: Level successful but no quest vault placed this attempt");
            }
            log_trace("QUEST VAULT FIX: Breaking from regeneration loop with successful level");
            break;
        }

        level_gen_screen_note_failure(why ? why : level_gen_screen.last_failure);

        if (why)
        {
            log_trace("QUEST VAULT FIX: Level generation failed (%s), regenerating (quest_vault_used=%d)",
                      why, p_ptr->quest_vault_used);
        }
        else
        {
            log_trace("QUEST VAULT FIX: Level generation failed (unknown reason), regenerating (quest_vault_used=%d)",
                      p_ptr->quest_vault_used);
        }

        // Undo unique things!
        unring_a_bell();

        /* Wipe the objects */
        wipe_o_list();

        /* Wipe the monsters */
        wipe_mon_list();
    }

    /* The dungeon is ready */
    character_dungeon = true;

    /* Reset the number of traps on the level. */
    num_trap_on_level = 0;

    /* Reset per-level skeleton note limits once the layout is finalized */
    skeleton_note_level_reset();

    /* Normalize the chasm-footprint tags after all generation edits. */
    apply_chasm_partition_tags();

    /* Enforce partition/room lighting rules (e.g. labyrinth/CA_BLOB always dark). */
    apply_partition_and_room_glow_rules();

    /* Note any forges generated -- have to do this here in case generation
     * fails earlier */
    for (y = 0; y < MAX_DUNGEON_HGT; y++)
    {
        for (x = 0; x < MAX_DUNGEON_WID; x++)
        {
            if (cave_forge_bold(y, x))
            {
                p_ptr->forge_count++;
            }
        }
    }

    level_gen_screen_finish(true);

    // Valar quest doesn't provide map rewards like the old thrall quest
}
