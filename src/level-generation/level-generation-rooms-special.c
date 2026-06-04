/* File: level-generation-rooms-special.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

bool vault_template_has_aule(vault_type *v) {
    if (!v || v->text == 0 || v->hgt == 0) return false;
    char *s = v_text + v->text;
    for (int row = 0; row < v->hgt; ++row) {
        if (strchr(s, 'L')) return true; /* 'L' designates Aulë in template */
        s += strlen(s) + 1; /* advance to next stored line (null-terminated) */
    }
    return false;
}

bool vault_template_has_mandos(vault_type *v) {
    if (!v || v->text == 0 || v->hgt == 0) return false;
    char *s = v_text + v->text;
    for (int row = 0; row < v->hgt; ++row) {
        if (strchr(s, 'N')) return true; /* 'N' designates Mandos in template */
        s += strlen(s) + 1; /* advance to next stored line (null-terminated) */
    }
    return false;
}

bool vault_template_has_duruin(vault_type *v) {
    if (!v) return false;
    /* Check vault name directly - Duruin Bastion is vault ID 461 */
    const char *name = v_name + v->name;
    return (strstr(name, "Duruin") != NULL || strstr(name, "Bastion") != NULL);
}

/* Global variables to store quest vault coordinates for monitoring */
int qv_stored_y1 = -1, qv_stored_x1 = -1, qv_stored_y2 = -1, qv_stored_x2 = -1;
bool qv_placed_this_level = false;  /* Track if quest vault actually placed this level */

/* DEBUGGING: Function to check if quest vault still exists at monitored coordinates */
void check_quest_vault_integrity(const char* checkpoint_name) {
    if (!qv_placed_this_level) {
        log_trace("VAULT INTEGRITY CHECK [%s]: No quest vault placed this level - skipping check", checkpoint_name);
        return;
    }
    if (qv_stored_y1 < 0 || qv_stored_y2 < 0) {
        log_trace("VAULT INTEGRITY CHECK [%s]: No quest vault coordinates stored", checkpoint_name);
        return;
    }

    int check_walls = 0, check_floors = 0, check_features = 0, check_monsters = 0;
    int check_icky = 0, check_room = 0, check_extra = 0;

    for (int cy = qv_stored_y1; cy <= qv_stored_y2; cy++) {
        for (int cx = qv_stored_x1; cx <= qv_stored_x2; cx++) {
            if (cave_feat[cy][cx] == FEAT_WALL_OUTER || cave_feat[cy][cx] == FEAT_WALL_INNER) {
                check_walls++;
            } else if (cave_feat[cy][cx] == FEAT_FLOOR) {
                check_floors++;
            } else if (cave_feat[cy][cx] == FEAT_WALL_EXTRA) {
                check_extra++;
            } else {
                check_features++;
            }

            if (cave_m_idx[cy][cx] > 0) {
                check_monsters++;
            }

            if (cave_info[cy][cx] & CAVE_ICKY) {
                check_icky++;
            }

            if (cave_info[cy][cx] & CAVE_ROOM) {
                check_room++;
            }
        }
    }

    log_trace("VAULT INTEGRITY CHECK [%s]: Area (%d,%d) to (%d,%d)",
              checkpoint_name, qv_stored_y1, qv_stored_x1, qv_stored_y2, qv_stored_x2);
    log_trace("VAULT INTEGRITY CHECK [%s]: %d walls, %d floors, %d features, %d monsters, %d extra_walls",
              checkpoint_name, check_walls, check_floors, check_features, check_monsters, check_extra);
    log_trace("VAULT INTEGRITY CHECK [%s]: %d CAVE_ICKY, %d CAVE_ROOM flags",
              checkpoint_name, check_icky, check_room);

    /* Alert if vault appears to be gone */
    if (check_walls < 50 && check_floors < 30) {
        log_trace("VAULT INTEGRITY WARNING [%s]: Vault appears to have been OVERWRITTEN! Very low content.", checkpoint_name);
    }
}

void process_quest_vault_area(int y0, int x0, vault_type *qv) {
    int y1 = y0 - qv->hgt / 2;
    int x1 = x0 - qv->wid / 2;
    int y2 = y1 + qv->hgt - 1;
    int x2 = x1 + qv->wid - 1;
    bool has_forge = false;
    bool has_aule  = false;
    bool has_mandos = false;

    log_trace("Quest vault processing: Area (%d,%d) to (%d,%d), size %dx%d",
              y1, x1, y2, x2, qv->wid, qv->hgt);

    /* Debug: Check what's actually in the vault area */
    int wall_count = 0, floor_count = 0, monster_count = 0, feature_count = 0;
    for (int dy = y1; dy <= y2; ++dy) {
        for (int dx = x1; dx <= x2; ++dx) {
            if (cave_feat[dy][dx] == FEAT_WALL_OUTER || cave_feat[dy][dx] == FEAT_WALL_INNER) {
                wall_count++;
            } else if (cave_feat[dy][dx] == FEAT_FLOOR) {
                floor_count++;
            } else if (cave_feat[dy][dx] != FEAT_WALL_EXTRA) {
                feature_count++;
            }

            if (cave_m_idx[dy][dx] > 0) {
                monster_count++;
            }

            if ((cave_feat[dy][dx] >= FEAT_FORGE_HEAD) && (cave_feat[dy][dx] <= FEAT_FORGE_TAIL)) {
                if (!has_forge) {
                    p_ptr->aule_forge_y = (byte)dy;
                    p_ptr->aule_forge_x = (byte)dx;
                    has_forge = true;
                    log_trace("Quest vault: Found forge at (%d,%d), feature=%d", dy, dx, cave_feat[dy][dx]);
                }
            }
            if (cave_m_idx[dy][dx] > 0) {
                monster_type *m_ptr = &mon_list[cave_m_idx[dy][dx]];
                if (m_ptr->r_idx == R_IDX_AULE) {
                    has_aule = true;
                    log_trace("Quest vault: Found Aulë at (%d,%d)", dy, dx);
                }
                if (m_ptr->r_idx == R_IDX_MANDOS) {
                    has_mandos = true;
                    p_ptr->mandos_vault_y = (byte)dy;
                    p_ptr->mandos_vault_x = (byte)dx;
                    log_trace("Quest vault: Found Mandos at (%d,%d)", dy, dx);
                }
            }
        }
    }

    log_trace("Quest vault contents: %d walls, %d floors, %d features, %d monsters",
              wall_count, floor_count, feature_count, monster_count);

    /* DEBUGGING: Store quest vault bounds for monitoring */
    qv_stored_y1 = y1; qv_stored_x1 = x1; qv_stored_y2 = y2; qv_stored_x2 = x2;
    qv_placed_this_level = true;  /* Mark that quest vault was actually placed */
    log_trace("QUEST VAULT MONITOR: Storing bounds (%d,%d) to (%d,%d) for tracking",
              qv_stored_y1, qv_stored_x1, qv_stored_y2, qv_stored_x2);

#if DEBUG_QUEST_VAULT
    qv_y1 = y1; qv_x1 = x1; qv_y2 = y2; qv_x2 = x2; /* record bounds */
    qv_capture();
    qv_dump("initial");
    /* Force mark/reveal for debugging */
    for (int ry = y1; ry <= y2; ++ry) for (int rx = x1; rx <= x2; ++rx) cave_info[ry][rx] |= (CAVE_MARK|CAVE_SEEN|CAVE_GLOW);
#endif
    bool quest_pending = false;

    if (has_forge && has_aule && p_ptr->aule_quest == AULE_QUEST_NOT_STARTED &&
        !quest_metarun_blocked(QUEST_ID_AULE, METARUN_QUEST_AULE) &&
        quest_can_initiate_more()) {
        /* Record pending quest state change instead of applying immediately */
        pending_quest_states.has_aule_change = true;
        pending_quest_states.aule_level = p_ptr->depth;
        pending_quest_states.aule_forge_y = p_ptr->aule_forge_y;
        pending_quest_states.aule_forge_x = p_ptr->aule_forge_x;
        quest_pending = true;
        level_gen_debug_note_questgiver(QUEST_ID_AULE);
        log_trace("Aulë quest: FORGE_PRESENT change DEFERRED (quest vault) at %d,%d depth=%d",
                  p_ptr->aule_forge_y, p_ptr->aule_forge_x, p_ptr->depth);
    }
    if (has_mandos && p_ptr->mandos_quest == MANDOS_QUEST_NOT_STARTED &&
        !quest_metarun_blocked(QUEST_ID_MANDOS, METARUN_QUEST_MANDOS) &&
        quest_can_initiate_more() && !quest_pending) {
        /* Record pending quest state change instead of applying immediately */
        pending_quest_states.has_mandos_change = true;
        pending_quest_states.mandos_level = p_ptr->depth;
        pending_quest_states.mandos_vault_y = p_ptr->mandos_vault_y;
        pending_quest_states.mandos_vault_x = p_ptr->mandos_vault_x;
        quest_pending = true;
        level_gen_debug_note_questgiver(QUEST_ID_MANDOS);
        log_trace("Mandos quest: GIVER_PRESENT change DEFERRED (quest vault) at %d,%d depth=%d",
                  p_ptr->mandos_vault_y, p_ptr->mandos_vault_x, p_ptr->depth);
    }
}

bool build_type6(int y0, int x0, bool force_forge)
{
    vault_type* v_ptr;
    int tries = 0;

    /* Pick an interesting room */
    while (true)
    {
        unsigned long long rarity = 0;
        tries++;

        /* Get a random vault record */
        v_ptr = &v_info[rand_int(z_info->v_max)];

        // log_trace("Vault selection: Trying vault #%d '%s' (type=%d, depth=%d, rarity=%d, flags=0x%x)",
        //           (int)(v_ptr - v_info), v_name + v_ptr->name, v_ptr->typ, v_ptr->depth, v_ptr->rarity, v_ptr->flags);

        // if forcing a forge, then skip vaults without forges in them
        if (force_forge && !v_ptr->forge)
        {
            log_trace("Skipping vault - force_forge=true but vault has no forge");
            continue;
        }

        // unless forcing a forge, try additional times to place any vault
        // marked TEST
        if ((tries < 1000) && !(v_ptr->flags & (VLT_TEST))
            && !p_ptr->force_forge)
        {
            // log_trace("Skipping vault - tries=%d, no TEST flag", tries);
            continue;
        }

        rarity = v_ptr->rarity;
        if (p_ptr->depth < 6)
        {
            /* Surface rooms are more common at low depths */
            if (!(v_ptr->flags & (VLT_SURFACE)) && !one_in_(4))
                continue;
        }
        else if (v_ptr->flags & (VLT_SURFACE))
        {
            /* Surface rooms get very much rarer at depth */
            rarity += (1 << (p_ptr->depth));
        }

        /* Accept the first interesting room (but not quest vaults) */
        if ((v_ptr->typ == 6) && (v_ptr->depth <= p_ptr->depth)
            && (v_ptr->max_depth == 0 || p_ptr->depth <= v_ptr->max_depth)
            && (one_in_(rarity)) && !(v_ptr->flags & VLT_QUEST))
            break;

        if (tries > 20000)
        {
            if (!DEPLOYMENT || cheat_room)
                msg_format(
                    "Bug: Could not find a record for an Interesting Room in "
                    "vault.txt");
            return (false);
        }
    }

    if (!force_forge && one_in_(4))
    {
        level_gen_debug_note_room_name(v_name + v_ptr->name);
        if (try_place_docked_vault(v_ptr, NULL, NULL))
        {
            return true;
        }
    }

    level_gen_debug_note_room_name(v_name + v_ptr->name);
    return place_room(y0, x0, v_ptr);
}

/*
 * Type 7 -- lesser vaults (see "vault.txt")
 */
bool build_type7(int y0, int x0)
{
    vault_type* v_ptr;
    int tries = 0;

    /* Pick a lesser vault */
    while (true)
    {
        tries++;

        /* Get a random vault record */
        v_ptr = &v_info[rand_int(z_info->v_max)];

        // try additional times to place any vault marked TEST
        if ((tries < 1000) && !(v_ptr->flags & (VLT_TEST)))
            continue;

        /* Accept the first lesser vault (but not quest vaults) */
        if ((v_ptr->typ == 7) && (v_ptr->depth <= p_ptr->depth)
            && (v_ptr->max_depth == 0 || p_ptr->depth <= v_ptr->max_depth)
            && (one_in_(v_ptr->rarity)) && !(v_ptr->flags & VLT_QUEST))
            break;

        if (tries > 2000)
        {
            msg_format(
                "Bug: Could not find a record for a Lesser Vault in vault.txt");
            return (false);
        }
    }

    bool placed = false;
    int placed_y = y0, placed_x = x0;

    level_gen_debug_note_room_name(v_name + v_ptr->name);
    if (one_in_(4) && try_place_docked_vault(v_ptr, &placed_y, &placed_x))
    {
        placed = true;
    }
    else if (place_room(y0, x0, v_ptr))
    {
        placed = true;
    }

    if (!placed)
        return false;
    /* Message */
    if (cheat_room)
        msg_format("LV (%s).", v_name + v_ptr->name);

    return true;
}

/*
 * Mark greater vault grids with the CAVE_G_VAULT flag.
 * Returns true if it succeds.
 */
bool mark_g_vault(int y0, int x0, int ymax, int xmax)
{
    int y1, x1, y2, x2, y, x;

    /* Get the coordinates */
    y1 = y0 - ymax / 2;
    x1 = x0 - xmax / 2;
    y2 = y1 + ymax - 1;
    x2 = x1 + xmax - 1;

    /* Step 1 - Mark all grids inside that perimeter with the new flag */
    for (y = y1 + 1; y < y2; y++)
    {
        for (x = x1 + 1; x < x2; x++)
        {
            cave_info[y][x] |= (CAVE_G_VAULT);
        }
    }

    return (true);
}

bool vault_type8_is_repeated(s16b v_idx)
{
    for (int i = 0; i < MAX_GREATER_VAULTS; i++)
    {
        if (p_ptr->greater_vaults[i] == v_idx)
            return true;
    }

    return false;
}

bool vault_type8_is_eligible(s16b v_idx, bool test_only)
{
    vault_type* v_ptr = &v_info[v_idx];

    if (v_ptr->typ != 8)
        return false;
    if (v_ptr->flags & VLT_QUEST)
        return false;
    if (v_ptr->depth > p_ptr->depth)
        return false;
    if (v_ptr->max_depth != 0 && p_ptr->depth > v_ptr->max_depth)
        return false;
    if (vault_type8_is_repeated(v_idx))
        return false;
    if (test_only && !(v_ptr->flags & VLT_TEST))
        return false;

    return true;
}

bool any_eligible_type8_test_vault(void)
{
    for (int i = 0; i < z_info->v_max; i++)
    {
        if (vault_type8_is_eligible(i, false) && (v_info[i].flags & VLT_TEST))
            return true;
    }

    return false;
}

bool choose_reserved_type8(vault_type** out_v_ptr, s16b* out_v_idx)
{
    int tries = 0;
    bool test_only = any_eligible_type8_test_vault();

    while (tries++ < 2000)
    {
        s16b v_idx = rand_int(z_info->v_max);
        vault_type* v_ptr = &v_info[v_idx];

        if (!vault_type8_is_eligible(v_idx, test_only))
            continue;

        if (!one_in_(vault_type8_generation_rarity(v_ptr, p_ptr->depth)))
            continue;

        *out_v_ptr = v_ptr;
        *out_v_idx = v_idx;
        return true;
    }

    return false;
}

bool place_type8_vault(int y0, int x0, vault_type* v_ptr, s16b v_idx)
{
    bool placed = false;
    int placed_y = y0;
    int placed_x = x0;

    level_gen_debug_note_greater_vault_name(v_name + v_ptr->name);
    if (one_in_(2) && try_place_docked_vault(v_ptr, &placed_y, &placed_x))
    {
        placed = true;
    }
    else if (place_room(y0, x0, v_ptr))
    {
        placed = true;
    }

    if (!placed)
        return false;

    for (int i = 0; i < MAX_GREATER_VAULTS; i++)
    {
        if (p_ptr->greater_vaults[i] == 0)
        {
            p_ptr->greater_vaults[i] = v_idx;
            break;
        }
    }

    if (cheat_room)
        msg_format("GV (%s).", v_name + v_ptr->name);

    if (mark_g_vault(placed_y, placed_x, v_ptr->hgt, v_ptr->wid))
    {
        SDL_strlcpy(g_vault_name, v_name + v_ptr->name, sizeof(g_vault_name));
    }

    return true;
}

bool build_reserved_type8(int y0, int x0)
{
    vault_type* v_ptr = NULL;
    s16b v_idx = 0;

    if (g_vault_name[0] != '\0')
        return false;

    if (!choose_reserved_type8(&v_ptr, &v_idx))
        return false;

    return place_type8_vault(y0, x0, v_ptr, v_idx);
}

/*
 * Type 8 -- greater vaults (see "vault.txt")
 */
bool build_type8(int y0, int x0)
{
    vault_type* v_ptr = NULL;
    int tries = 0;
    bool found = false;
    bool repeated = false;
    int i;
    s16b v_idx;
    bool prefer_test = any_eligible_type8_test_vault();

    // Can only have one greater vault per level
    if (g_vault_name[0] != '\0')
    {
        return (false);
    }

    /* Pick a greater vault */
    while (!found)
    {
        tries++;

        /* Get a random vault record */
        v_idx = rand_int(z_info->v_max);
        v_ptr = &v_info[v_idx];

        // try additional times to place any vault marked TEST
        if (prefer_test && (tries < 1000) && !(v_ptr->flags & (VLT_TEST)))
            continue;

        /* Surface vaults get exponentially rarer at depth */
        {
            /* Accept the first greater vault (but not quest vaults) */
            if ((v_ptr->typ == 8) && (v_ptr->depth <= p_ptr->depth)
                && (v_ptr->max_depth == 0 || p_ptr->depth <= v_ptr->max_depth)
                && (one_in_(vault_type8_generation_rarity(v_ptr, p_ptr->depth))) && !(v_ptr->flags & VLT_QUEST))
        {
            repeated = false;
            for (i = 0; i < MAX_GREATER_VAULTS; i++)
            {
                if (v_idx == p_ptr->greater_vaults[i])
                {
                    repeated = true;
                }
            }

            if (!repeated)
                found = true;
            }
        }

        if (tries > 2000)
        {
            // if (!repeated) msg_debug("Bug: Could not find a record for a
            // Greater Vault in vault.txt");
            return (false);
        }
    }

    return place_type8_vault(y0, x0, v_ptr, v_idx);
}

/*
 * Type 9 -- Morgoth's vault (see "vault.txt")
 */
bool build_type9(int y0, int x0, vault_type** used_vault)
{
    vault_type* v_ptr;
    int tries = 0;

    /* Pick a version of Morgoth's vault */
    while (true)
    {
        /* Get a random vault record */
        v_ptr = &v_info[rand_int(z_info->v_max)];

        /* Accept the first morgoth vault */
        if (v_ptr->typ == 9)
            break;

        tries++;
        if (tries > 10000)
        {
            msg_format(
                "Could not find a record for Morgoth's Vault in vault.txt");
            return (false);
        }
    }

    if (used_vault)
        *used_vault = v_ptr;

    /* Try building the vault */
    if (!build_vault(y0, x0, v_ptr, false))
    {
        return (false);
    }

    /* Cause a special feeling */
    good_item_flag = true;

    /* Hack -- Mark vault grids with the CAVE_G_VAULT flag */
    if (mark_g_vault(y0, x0, v_ptr->hgt, v_ptr->wid))
    {
        SDL_strlcpy(g_vault_name, v_name + v_ptr->name, sizeof(g_vault_name));
    }

    return (true);
}
