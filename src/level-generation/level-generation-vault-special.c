/* File: level-generation-vault-special.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"

#define DEBUG_QUEST_VAULT 0
#if DEBUG_QUEST_VAULT
int qv_y1 = -1, qv_x1 = -1, qv_y2 = -1, qv_x2 = -1;
int qv_h = 0, qv_w = 0;
unsigned short *qv_feat_snapshot = NULL;

void qv_capture(void) {
    int y,x;
    if (qv_y1 < 0) return;
    qv_h = qv_y2 - qv_y1 + 1;
    qv_w = qv_x2 - qv_x1 + 1;
    mem_free_null(qv_feat_snapshot);
    qv_feat_snapshot = mem_alloc_array(qv_h * qv_w, unsigned short);
    for (y = qv_y1; y <= qv_y2; ++y)
        for (x = qv_x1; x <= qv_x2; ++x)
            qv_feat_snapshot[(y - qv_y1) * qv_w + (x - qv_x1)] = cave_feat[y][x];
    log_trace("Quest vault DEBUG: snapshot captured (%d x %d) bounds (%d,%d)-(%d,%d)", qv_h, qv_w, qv_y1, qv_x1, qv_y2, qv_x2);
}

char qv_glyph(int f) {
    switch (f) {
        case FEAT_FLOOR: return '.'; case FEAT_WALL_OUTER: return '#';
        case FEAT_WALL_INNER: return '+'; case FEAT_WALL_EXTRA: return 'X';
#ifdef FEAT_DOOR_CLOSED
        case FEAT_DOOR_CLOSED: return 'D';
#endif
        case FEAT_FORGE_HEAD: case FEAT_FORGE_TAIL: return 'F';
        default: return '?';
    }
}

void qv_dump(const char *phase) {
    if (qv_y1 < 0) return;
    int y,x; char row[256];
    log_trace("Quest vault DEBUG: layout (%s) bounds (%d,%d)-(%d,%d)", phase, qv_y1, qv_x1, qv_y2, qv_x2);
    for (y = qv_y1; y <= qv_y2; ++y) {
        int idx=0;
        for (x = qv_x1; x <= qv_x2 && idx < (int)sizeof(row)-2; ++x)
            row[idx++] = qv_glyph(cave_feat[y][x]);
        row[idx]='\0';
        log_trace("Quest vault DEBUG ROW %2d: %s", y, row);
    }
}

void qv_compare(void) {
    if (!qv_feat_snapshot) return;
    int diffs=0, y,x;
    for (y = qv_y1; y <= qv_y2; ++y) for (x = qv_x1; x <= qv_x2; ++x) {
        unsigned short before = qv_feat_snapshot[(y - qv_y1) * qv_w + (x - qv_x1)];
        unsigned short now = cave_feat[y][x];
        if (before != now) {
            log_trace("Quest vault DEBUG: tile changed (%d,%d) %d->%d", y, x, before, now);
            if (++diffs >= 50) goto done_diffs;
        }
    }
done_diffs:
    if (!diffs) log_trace("Quest vault DEBUG: no tile changes detected since snapshot");
}
#endif

typedef struct vault_monster_spec {
    char symbol;
    const char* guid_text;
    u64b guid;
    bool guid_cached;
    bool start_sleeping;
    bool ignore_depth;
} vault_monster_spec;

vault_monster_spec vault_monster_table[] = {
    {'C', "9cbdbb88fd4f59dc", 0, false, true, true},
    {'H', "c790972955718680", 0, false, true, false},
    {'@', "4acd2c9fcc5cd6e5", 0, false, true, false},
    {'o', "88ef7547642967b2", 0, false, true, false},
    {'O', "2c739cdb1be99f2c", 0, false, true, false},
    {'Z', "05f49e29acf49a93", 0, false, true, true},
    {'f', "3c10b33361f6f136", 0, false, true, false},
    {'F', "9a6fbc6e7b46f502", 0, false, true, false},
    {'T', "b39a82dfdc1c5ef9", 0, false, true, false},
    {'W', "c92f7e02e189e1bd", 0, false, true, true},
    {'y', "2f6ec4ab45007365", 0, false, true, false},
    {'Y', "0af151dfe09fe455", 0, false, true, false},
    {'A', "ed37fc4fce32643f", 0, false, true, true},
    {'L', "d27e36edf5c2f432", 0, false, true, true},
    {'N', "f134bcd795c27d4f", 0, false, true, true},
    {'D', "3ab7e216cb871fec", 0, false, true, true},
    {'K', "4da7998251196a35", 0, false, true, true}, /* Ancalagon the Black */
    {'I', "7a94fd98505d6076", 0, false, true, true}, /* Flying cold-drake */
    {'J', "49c954b30d9f0406", 0, false, true, true}, /* Flying fire-drake */
    {'R', "0e0f11695f8a443d", 0, false, true, true},
    {'U', "c2485b83ba33934d", 0, false, true, true},
    {'G', "7b038638b2981d20", 0, false, true, true},
    {'V', "58d8cf770bfcbe6f", 0, false, true, true},
    {'B', "9c44dec3f9d6d14c", 0, false, false, true}, /* Duruin, Least of the Balrogs */
    {'q', "ccff426ff2ef0318", 0, false, true, true},  /* Whispering shadow */
    {'j', "d5e4892102e9b48a", 0, false, true, true},  /* Shadow spider */
    {'k', "d2d2f0b7edcf4cf6", 0, false, true, true},  /* Lurking horror */
    {'n', "7783062d13500802", 0, false, true, true},  /* Nightthorn */
};

int current_build_vault_type = 0;
bool current_build_vault_exact_token = false;

bool coord_in_morgoth_region(int y, int x, int margin);

bool monster_special_vault_selection_allowed(void)
{
    if (current_build_vault_exact_token)
        return true;

    return current_build_vault_type == 9;
}

bool monster_special_vault_only_allowed_at(int y, int x)
{
    if (current_build_vault_exact_token)
        return true;

    if (!in_bounds(y, x))
        return false;

    if (current_build_vault_type == 9)
        return true;

    return coord_in_morgoth_region(y, x, 0)
        && ((cave_info[y][x] & CAVE_G_VAULT) != 0);
}

void monster_special_vault_debug_context(
    int* build_vault_type, bool* exact_token)
{
    if (build_vault_type)
        *build_vault_type = current_build_vault_type;
    if (exact_token)
        *exact_token = current_build_vault_exact_token;
}

bool place_vault_monster_token(char symbol, int y, int x)
{
    for (size_t i = 0; i < N_ELEMENTS(vault_monster_table); i++)
    {
        vault_monster_spec* spec = &vault_monster_table[i];
        if (spec->symbol != symbol)
            continue;

        if (!spec->guid_cached)
        {
            spec->guid_cached = true;
            if (!parse_u64b_hex(spec->guid_text, &spec->guid))
            {
                spec->guid = 0;
                log_error("Vault: invalid GUID '%s' for token '%c'",
                    spec->guid_text, symbol);
            }
        }

        if (!spec->guid)
        {
            log_warn("Vault: GUID missing for monster token '%c'", symbol);
            return false;
        }

        bool old_exact_token = current_build_vault_exact_token;
        bool placed;

        log_trace(
            "SPECIAL_VAULT_ONLY exact-token attempt: token='%c' guid=%s depth=%d at=(%d,%d) build_vault_type=%d",
            symbol, spec->guid_text, p_ptr->depth, y, x, current_build_vault_type);

        current_build_vault_exact_token = true;
        placed = place_monster_by_guid(
            y, x, spec->guid, spec->start_sleeping, spec->ignore_depth, NULL);
        current_build_vault_exact_token = old_exact_token;

        if (!placed)
        {
            log_warn("Vault: failed to place monster for token '%c'", symbol);
            return false;
        }

        {
            s16b r_idx = monster_lookup_guid(spec->guid);
            const char* monster_name =
                (r_idx > 0) ? (r_name + r_info[r_idx].name) : "<unknown>";
            log_trace(
                "SPECIAL_VAULT_ONLY exact-token placed: token='%c' monster='%s' r_idx=%d depth=%d at=(%d,%d) build_vault_type=%d",
                symbol, monster_name, r_idx, p_ptr->depth, y, x,
                current_build_vault_type);
        }

        return true;
    }

    return false;
}

bool is_vault_monster_token(char symbol)
{
    for (size_t i = 0; i < N_ELEMENTS(vault_monster_table); i++)
    {
        if (vault_monster_table[i].symbol == symbol)
            return true;
    }

    return false;
}

bool chasm_mask_has_square_space(
    const bool* mask, int h, int w, int cy, int cx, int radius)
{
    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            int ny = cy + dy;
            int nx = cx + dx;

            if (ny < 0 || nx < 0 || ny >= h || nx >= w)
                return false;
            if (!mask[ny * w + nx])
                return false;
        }
    }

    return true;
}

bool choose_chasm_sanctum_seed(
    const bool* is_cave, int h, int w, int* out_y, int* out_x)
{
    int center_y = h / 2;
    int center_x = w / 2;
    int best_score = 0;
    bool found = false;

    for (int y = 2; y < h - 2; ++y)
    {
        for (int x = 2; x < w - 2; ++x)
        {
            int score;

            if (!is_cave[y * w + x])
                continue;
            if (!chasm_mask_has_square_space(is_cave, h, w, y, x, 2))
                continue;

            score = distance(y, x, center_y, center_x);
            if (!found || score < best_score)
            {
                best_score = score;
                *out_y = y;
                *out_x = x;
                found = true;
            }
        }
    }

    return found;
}

const int chasm_sanctum_ambush_offsets[8][2] = {
    {-2, -2}, {-2, 0}, {-2, 2},
    {0, -2},            {0, 2},
    {2, -2},  {2, 0},  {2, 2}
};

bool chasm_sanctum_drop_marker_present(int y, int x)
{
    for (object_type* o_ptr = get_first_object(y, x); o_ptr;
        o_ptr = get_next_object(o_ptr))
    {
        if (o_ptr->ident & IDENT_CHASM_SANCTUM_ITEM)
            return true;
    }

    return false;
}

bool chasm_sanctum_ambush_tile(int y, int x)
{
    for (int i = 0; i < 8; ++i)
    {
        int cy = y - chasm_sanctum_ambush_offsets[i][0];
        int cx = x - chasm_sanctum_ambush_offsets[i][1];

        if (!in_bounds_fully(cy, cx))
            continue;
        if (chasm_sanctum_drop_marker_present(cy, cx))
            return true;
    }

    return false;
}

bool place_exact_skeleton_at(int y, int x, byte sval)
{
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;
    s16b k_idx;

    if (!in_bounds_fully(y, x))
        return false;
    if (cave_feat[y][x] != FEAT_FLOOR)
        return false;
    if (cave_o_idx[y][x] != 0)
        return false;

    k_idx = lookup_kind(TV_SKELETON, sval);
    if (!k_idx)
        return false;

    object_wipe(i_ptr);
    object_prep(i_ptr, k_idx);
    i_ptr->pval = 1;

    return (floor_carry(y, x, i_ptr) != 0);
}

bool place_chasm_sanctum_drop_at(int y, int x)
{
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;

    if (!in_bounds_fully(y, x))
        return false;
    if (cave_feat[y][x] != FEAT_FLOOR)
        return false;
    if (cave_o_idx[y][x] != 0)
        return false;

    object_wipe(i_ptr);
    if (!drop_generate_chasm_sanctum_object(p_ptr->depth, i_ptr))
        return false;

    i_ptr->ident |= IDENT_CHASM_SANCTUM_ITEM | IDENT_CHASM_SANCTUM_DROP;

    return (floor_carry(y, x, i_ptr) != 0);
}

void place_chasm_island_sanctum(int cy, int cx)
{
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            int ny = cy + dy;
            int nx = cx + dx;

            if (dy == 0 && dx == 0)
                continue;
            if (!in_bounds_fully(ny, nx))
                continue;
            if (cave_feat[ny][nx] != FEAT_FLOOR)
                continue;

            cave_set_feat(ny, nx, FEAT_GLYPH);
            cave_info[ny][nx] |= (CAVE_ROOM | CAVE_CHASM_AREA);
        }
    }

    if (!place_chasm_sanctum_drop_at(cy, cx))
    {
        log_warn("Chasm sanctum: failed to place EVIL drop at (%d,%d), falling back to elf skeleton",
            cy, cx);
        (void)place_exact_skeleton_at(cy, cx, SV_SKELETON_ELF);
    }
}
