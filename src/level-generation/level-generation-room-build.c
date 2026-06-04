/* File: level-generation-room-build.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

#if 0
bool room_build(int typ)
{
    int y, x;

    if (dun->cent_n >= room_capacity_limit())
    {
        return (false);
    }

    y = rand_range(5, p_ptr->cur_map_hgt - 5);
    x = rand_range(5, p_ptr->cur_map_wid - 5);

    /* Build a room */
    switch (typ)
    {
    /* Build an appropriate room */
    // Greater Vault
    case 8:
    {
        if (!build_type8(y, x))
        {
            return (false);
        }
        break;
    }
    // Lesser Vault
    case 7:
    {
        if (!build_type7(y, x))
        {
            return (false);
        }
        break;
    }
    // Least Vault
    case 6:
    {
        if (!build_type6(y, x, false))
        {
            return (false);
        }
        break;
    }
    // Cross Room
    case 2:
    {
        if (!build_type2(y, x))
        {
            return (false);
        }
        break;
    }
    // Normal Room
    case 1:
    {
        if (!build_type1(y, x))
        {
            return (false);
        }
        break;
    }
    /* Paranoia */
    default:
        return (false);
    }

    /* Success */
    return (true);
}
#endif

/*
 * Try to place a quest vault of specified type using forced placement strategy
 * Returns true if successfully placed, false otherwise
 */
bool place_duruin_bastion(void)
{
    vault_type* qv_ptr;
    int y, x;

    log_trace("Varda quest: Attempting to force-place Duruin Bastion at depth %d", p_ptr->depth);

    for (int i = 0; i < z_info->v_max; i++)
    {
        qv_ptr = &v_info[i];
        if (!(qv_ptr->flags & VLT_QUEST)) continue;
        if (!vault_template_has_duruin(qv_ptr)) continue;
        if (qv_ptr->depth > p_ptr->depth) continue;
        if (qv_ptr->max_depth != 0 && p_ptr->depth > qv_ptr->max_depth) continue;
        if (!quest_vault_surface_roll_allows(qv_ptr, p_ptr->depth)) continue;

        /* Found Duruin Bastion - attempt placement and return result */
        log_trace("Varda quest: Found Duruin Bastion vault at index %d: '%s', attempting placement", i, v_name + qv_ptr->name);
        log_trace("Varda quest: Vault details - typ=%d, hgt=%d, wid=%d, depth=%d, flags=0x%x",
                  qv_ptr->typ, qv_ptr->hgt, qv_ptr->wid, qv_ptr->depth, qv_ptr->flags);
        level_gen_debug_note_quest_vault_name(v_name + qv_ptr->name);

        int center_y = p_ptr->cur_map_hgt / 2;
        int center_x = p_ptr->cur_map_wid / 2;

        /* Attempt primary placement near center */
        y = center_y + rand_range(-p_ptr->cur_map_hgt/6, p_ptr->cur_map_hgt/6);
        x = center_x + rand_range(-p_ptr->cur_map_wid/6, p_ptr->cur_map_wid/6);
        y = MAX(qv_ptr->hgt/2 + 3, MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt/2 - 3));
        x = MAX(qv_ptr->wid/2 + 3, MIN(x, p_ptr->cur_map_wid - qv_ptr->wid/2 - 3));

        if (place_room_forced(y, x, qv_ptr)) {
            qv_placed_this_level = true;
            level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
            process_quest_vault_area(y, x, qv_ptr);
            pending_quest_states.has_varda_change = true;
            pending_quest_states.varda_level = p_ptr->depth;
            pending_quest_states.varda_vault_y = y;
            pending_quest_states.varda_vault_x = x;
            log_trace("Varda quest: Duruin Bastion placed at (%d,%d)", y, x);
            genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d)",
                v_name + qv_ptr->name, y, x);
            return true;
        }

        /* Fallback attempts with wider variance */
        for (int attempts = 0; attempts < 10; attempts++) {
            y = center_y + rand_range(-p_ptr->cur_map_hgt/4, p_ptr->cur_map_hgt/4);
            x = center_x + rand_range(-p_ptr->cur_map_wid/4, p_ptr->cur_map_wid/4);
            y = MAX(qv_ptr->hgt/2 + 3, MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt/2 - 3));
            x = MAX(qv_ptr->wid/2 + 3, MIN(x, p_ptr->cur_map_wid - qv_ptr->wid/2 - 3));

            if (place_room_forced(y, x, qv_ptr)) {
                qv_placed_this_level = true;
                level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
                process_quest_vault_area(y, x, qv_ptr);
                pending_quest_states.has_varda_change = true;
                pending_quest_states.varda_level = p_ptr->depth;
                pending_quest_states.varda_vault_y = y;
                pending_quest_states.varda_vault_x = x;
                log_trace("Varda quest: Duruin Bastion placed on fallback attempt %d at (%d,%d)", attempts + 1, y, x);
                genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d) on fallback %d",
                    v_name + qv_ptr->name, y, x, attempts + 1);
                return true;
            }
        }

        log_trace("Varda quest: Random placement failed for '%s', scanning the full map for a guaranteed fit",
            v_name + qv_ptr->name);
        if (place_room_forced_exhaustive(qv_ptr, &y, &x))
        {
            qv_placed_this_level = true;
            level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
            process_quest_vault_area(y, x, qv_ptr);
            pending_quest_states.has_varda_change = true;
            pending_quest_states.varda_level = p_ptr->depth;
            pending_quest_states.varda_vault_y = y;
            pending_quest_states.varda_vault_x = x;
            log_trace("Varda quest: Duruin Bastion placed by exhaustive scan at (%d,%d)", y, x);
            genlog_quest("QUEST VAULT PLACED: '%s' at (%d,%d) after full-map scan",
                v_name + qv_ptr->name, y, x);
            return true;
        }

        /* If we reach here, Duruin placement failed - return immediately without trying other vaults */
        log_trace("Varda quest: Duruin Bastion placement failed after all attempts, returning false");
        genlog_quest("QUEST VAULT FAILED: '%s' could not be placed",
            v_name + qv_ptr->name);
        return false;
    }

    log_trace("Varda quest: Failed to find Duruin Bastion vault template at depth %d", p_ptr->depth);
    return false;
}

bool try_quest_vault_type(int v_type, bool *had_eligible_candidate)
{
    int i;
    vault_type* qv_ptr;
    int y, x;
    bool attempted_placement = false;

    if (had_eligible_candidate)
        *had_eligible_candidate = false;

    if (!quest_can_initiate_more()) {
        log_trace("Quest vault: skipped type %d - initiated quest cap reached (%d/%d)",
                  v_type, quest_initiated_count_this_run(), QUEST_MAX_INITIATED_PER_RUN);
        return false;
    }

    log_trace("Quest vault: Attempting type %d quest vault with forced placement strategy", v_type);

    for (i = 0; i < z_info->v_max; i++)
    {
        qv_ptr = &v_info[i];
        if (qv_ptr->typ != v_type) continue;
        if (!(qv_ptr->flags & VLT_QUEST)) continue;
        if (qv_ptr->depth > p_ptr->depth) continue;
        if (qv_ptr->max_depth != 0 && p_ptr->depth > qv_ptr->max_depth) continue;
        if (!quest_vault_surface_roll_allows(qv_ptr, p_ptr->depth)) continue;
        if (vault_template_has_duruin(qv_ptr)) {
            log_trace("Quest vault: Skipping Duruin Bastion in generic placement path (quest-only)");
            continue;
        }

        log_trace("Quest vault: Checking vault %d '%s' (rarity=%d)", i, v_name + qv_ptr->name, qv_ptr->rarity);

        /* Once the quest-vault roll has committed this level to quest content,
         * still honor SURFACE weighting, but do not re-gate by template rarity. */

        /* Check Aulë requirements */
        if (vault_template_has_aule(qv_ptr)) {
            log_trace("Quest vault: === AULE VAULT DETECTED === Checking eligibility (depth=%d)", p_ptr->depth);
            log_trace("Quest vault: initiated quest count=%d/%d",
                      quest_initiated_count_this_run(), QUEST_MAX_INITIATED_PER_RUN);
            log_trace("  Player SMT skill_base = %d", p_ptr->skill_base[S_SMT]);
            log_trace("  Player SMT skill_use = %d", p_ptr->skill_use[S_SMT]);

            /* Use data-driven eligibility check from quest.txt E: field */
            if (!check_quest_eligibility(2, p_ptr->depth)) { /* Aulë is quest index 2 */
                log_trace("Quest vault: Aulë vault skipped (eligibility check failed)");
                continue;
            }
            log_trace("Quest vault: Aulë eligibility check PASSED");

            if (quest_metarun_blocked(QUEST_ID_AULE, METARUN_QUEST_AULE)) {
                log_trace("Quest vault: Aulë vault skipped (quest blocked by metarun)");
                continue;
            }
            if (!quest_can_initiate_more()) {
                log_trace("Quest vault: === AULE BLOCKED === initiated quest cap reached");
                continue;
            }
            log_trace("Quest vault: === AULE APPROVED === All checks passed, proceeding with generation");
        }

        /* Check Mandos requirements */
        if (vault_template_has_mandos(qv_ptr)) {
            log_trace("Quest vault: Checking Mandos vault '%s' - mandos_quest=%d, initiated=%d/%d",
                     v_name + qv_ptr->name, p_ptr->mandos_quest,
                     quest_initiated_count_this_run(), QUEST_MAX_INITIATED_PER_RUN);
            if (p_ptr->mandos_quest != MANDOS_QUEST_NOT_STARTED) {
                log_trace("Quest vault: Mandos vault skipped (quest state %d)",
                         p_ptr->mandos_quest);
                continue;
            }
            if (quest_metarun_blocked(QUEST_ID_MANDOS, METARUN_QUEST_MANDOS)) {
                log_trace("Quest vault: Mandos vault skipped (quest blocked by metarun)");
                continue;
            }
            if (!quest_can_initiate_more()) {
                log_trace("Quest vault: Mandos vault skipped (initiated quest cap reached)");
                continue;
            }
        }

        attempted_placement = true;
        if (had_eligible_candidate)
            *had_eligible_candidate = true;
        level_gen_debug_note_quest_vault_name(v_name + qv_ptr->name);

        /* Use forced placement strategy like forge placement:
         * Pick optimal location near center and use reduced padding */

        /* Calculate optimal placement position (center of map with some variation) */
        int center_y = p_ptr->cur_map_hgt / 2;
        int center_x = p_ptr->cur_map_wid / 2;

        /* Add some randomness but keep near center for best chance of success */
        y = center_y + rand_range(-p_ptr->cur_map_hgt/6, p_ptr->cur_map_hgt/6);
        x = center_x + rand_range(-p_ptr->cur_map_wid/6, p_ptr->cur_map_wid/6);

        /* Ensure within reasonable bounds */
        y = MAX(qv_ptr->hgt/2 + 3, MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt/2 - 3));
        x = MAX(qv_ptr->wid/2 + 3, MIN(x, p_ptr->cur_map_wid - qv_ptr->wid/2 - 3));

        log_trace("Quest vault: Attempting forced placement of '%s' at optimal location (%d,%d) (center: %d,%d)",
                 v_name + qv_ptr->name, y, x, center_y, center_x);

        if (place_room_forced(y, x, qv_ptr)) {
            /* Mark that quest vault was placed in this attempt */
            qv_placed_this_level = true;  /* Track for integrity checks */
            level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);

            /* DEBUGGING: Verify vault actually exists at coordinates immediately after placement */
            int y1 = y - qv_ptr->hgt / 2;
            int x1 = x - qv_ptr->wid / 2;
            int y2 = y1 + qv_ptr->hgt - 1;
            int x2 = x1 + qv_ptr->wid - 1;

            int verify_walls = 0, verify_floors = 0, verify_features = 0, verify_monsters = 0;
            int verify_icky = 0, verify_room = 0;

            for (int vy = y1; vy <= y2; vy++) {
                for (int vx = x1; vx <= x2; vx++) {
                    if (cave_feat[vy][vx] == FEAT_WALL_OUTER || cave_feat[vy][vx] == FEAT_WALL_INNER) {
                        verify_walls++;
                    } else if (cave_feat[vy][vx] == FEAT_FLOOR) {
                        verify_floors++;
                    } else if (cave_feat[vy][vx] != FEAT_WALL_EXTRA) {
                        verify_features++;
                    }

                    if (cave_m_idx[vy][vx] > 0) {
                        verify_monsters++;
                    }

                    if (cave_info[vy][vx] & CAVE_ICKY) {
                        verify_icky++;
                    }

                    if (cave_info[vy][vx] & CAVE_ROOM) {
                        verify_room++;
                    }
                }
            }

            log_trace("VAULT VERIFICATION IMMEDIATELY AFTER PLACEMENT: Area (%d,%d) to (%d,%d)",
                      y1, x1, y2, x2);
            log_trace("VAULT VERIFICATION: %d walls, %d floors, %d features, %d monsters",
                      verify_walls, verify_floors, verify_features, verify_monsters);
            log_trace("VAULT VERIFICATION: %d CAVE_ICKY, %d CAVE_ROOM flags",
                      verify_icky, verify_room);

            process_quest_vault_area(y, x, qv_ptr);
            log_trace("Quest vault: Type %d quest vault '%s' placed at (%d,%d) using forced strategy",
                     v_type, v_name + qv_ptr->name, y, x);
            genlog_quest("QUEST VAULT PLACED: '%s' type=%d at (%d,%d)",
                v_name + qv_ptr->name, v_type, y, x);
            return true;
        } else {
            log_trace("Quest vault: Failed to place vault '%s' at (%d,%d) even with forced strategy",
                     v_name + qv_ptr->name, y, x);
            /* Try a few more strategic locations before giving up */
            for (int attempts = 0; attempts < 10; attempts++) {
                y = center_y + rand_range(-p_ptr->cur_map_hgt/4, p_ptr->cur_map_hgt/4);
                x = center_x + rand_range(-p_ptr->cur_map_wid/4, p_ptr->cur_map_wid/4);
                y = MAX(qv_ptr->hgt/2 + 3, MIN(y, p_ptr->cur_map_hgt - qv_ptr->hgt/2 - 3));
                x = MAX(qv_ptr->wid/2 + 3, MIN(x, p_ptr->cur_map_wid - qv_ptr->wid/2 - 3));

                if (place_room_forced(y, x, qv_ptr)) {
                    /* Mark that quest vault was placed in this attempt */
                    qv_placed_this_level = true;  /* Track for integrity checks */
                    level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);

                    /* DEBUGGING: Verify vault actually exists at coordinates immediately after placement */
                    int y1 = y - qv_ptr->hgt / 2;
                    int x1 = x - qv_ptr->wid / 2;
                    int y2 = y1 + qv_ptr->hgt - 1;
                    int x2 = x1 + qv_ptr->wid - 1;

                    int verify_walls = 0, verify_floors = 0, verify_features = 0, verify_monsters = 0;
                    int verify_icky = 0, verify_room = 0;

                    for (int vy = y1; vy <= y2; vy++) {
                        for (int vx = x1; vx <= x2; vx++) {
                            if (cave_feat[vy][vx] == FEAT_WALL_OUTER || cave_feat[vy][vx] == FEAT_WALL_INNER) {
                                verify_walls++;
                            } else if (cave_feat[vy][vx] == FEAT_FLOOR) {
                                verify_floors++;
                            } else if (cave_feat[vy][vx] != FEAT_WALL_EXTRA) {
                                verify_features++;
                            }

                            if (cave_m_idx[vy][vx] > 0) {
                                verify_monsters++;
                            }

                            if (cave_info[vy][vx] & CAVE_ICKY) {
                                verify_icky++;
                            }

                            if (cave_info[vy][vx] & CAVE_ROOM) {
                                verify_room++;
                            }
                        }
                    }

                    log_trace("VAULT VERIFICATION (FALLBACK) IMMEDIATELY AFTER PLACEMENT: Area (%d,%d) to (%d,%d)",
                              y1, x1, y2, x2);
                    log_trace("VAULT VERIFICATION (FALLBACK): %d walls, %d floors, %d features, %d monsters",
                              verify_walls, verify_floors, verify_features, verify_monsters);
                    log_trace("VAULT VERIFICATION (FALLBACK): %d CAVE_ICKY, %d CAVE_ROOM flags",
                              verify_icky, verify_room);

                    process_quest_vault_area(y, x, qv_ptr);
                    log_trace("Quest vault: Type %d quest vault '%s' placed at (%d,%d) using fallback attempt %d",
                             v_type, v_name + qv_ptr->name, y, x, attempts + 1);
                    genlog_quest("QUEST VAULT PLACED: '%s' type=%d at (%d,%d) on fallback %d",
                        v_name + qv_ptr->name, v_type, y, x, attempts + 1);
                    return true;
                }
            }

            log_trace("Quest vault: Random placement failed for '%s', scanning the full map for any valid fit",
                v_name + qv_ptr->name);
            if (place_room_forced_exhaustive(qv_ptr, &y, &x))
            {
                qv_placed_this_level = true;
                level_gen_debug_activate_quest_vault_name(v_name + qv_ptr->name);
                process_quest_vault_area(y, x, qv_ptr);
                log_trace("Quest vault: Type %d quest vault '%s' placed at (%d,%d) by exhaustive scan",
                    v_type, v_name + qv_ptr->name, y, x);
                genlog_quest("QUEST VAULT PLACED: '%s' type=%d at (%d,%d) after full-map scan",
                    v_name + qv_ptr->name, v_type, y, x);
                return true;
            }

            genlog_quest("QUEST VAULT FAILED: '%s' type=%d could not be placed",
                v_name + qv_ptr->name, v_type);
        }
    }

    if (attempted_placement)
    {
        log_trace("Quest vault: Type %d had eligible templates, but none fit this attempt", v_type);
    }
    else
    {
        log_trace("Quest vault: Type %d has no eligible templates for this character/depth", v_type);
    }

    return false;
}
