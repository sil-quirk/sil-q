#include "angband.h"
#include "sdl/main-sdl-private.h"

bool sdl_mouse_path_grid_is_open_floor(int y, int x)
{
    return in_bounds(y, x) && cave_floor_bold(y, x)
        && cave_feat[y][x] != FEAT_CHASM;
}

void sdl_mouse_path_feature_visual(int feat, byte* a, char* c)
{
    feature_type* f_ptr;

    if (!a || !c)
        return;

    f_ptr = &f_info[feat];
    if (graphics_are_ascii()) {
        *a = f_ptr->d_attr;
        *c = f_ptr->d_char;
    } else {
        *a = f_ptr->x_attr;
        *c = f_ptr->x_char;
    }
}

bool sdl_mouse_path_minimap_draws_terrain(int y, int x)
{
    byte a = TERM_DARK;
    byte ta = TERM_DARK;
    byte dark_a = TERM_DARK;
    char c = ' ';
    char tc = ' ';
    char dark_c = ' ';

    if (!p_ptr || !in_bounds(y, x))
        return false;

    map_info(y, x, &a, &c, &ta, &tc);
    sdl_mouse_path_feature_visual(FEAT_NONE, &dark_a, &dark_c);

    return (ta != dark_a) || (tc != dark_c);
}

bool sdl_mouse_path_grid_known(int y, int x)
{
    u16b info;
    int m_idx;

    if (!in_bounds(y, x))
        return false;
    if ((y == p_ptr->py) && (x == p_ptr->px))
        return true;

    m_idx = cave_m_idx[y][x];
    if ((m_idx > 0) && (mon_list[m_idx].ml || (mon_list[m_idx].mflag & MFLAG_MARK)))
        return true;

    info = cave_info[y][x];
    return ((info & (CAVE_MARK | CAVE_SEEN)) != 0)
        || ((info & CAVE_VIEW) && sdl_mouse_path_grid_is_open_floor(y, x))
        || (sdl_mouse_path_grid_is_open_floor(y, x)
            && sdl_mouse_path_minimap_draws_terrain(y, x));
}

bool sdl_mouse_feature_known_for_action(int y, int x)
{
    if (!in_bounds(y, x))
        return false;
    if (cave_info[y][x] & (CAVE_MARK))
        return true;

    return grid_info_is_available(y, x)
        && (cave_info[y][x] & (CAVE_SEEN));
}

void sdl_mouse_note_feature_for_action(int y, int x)
{
    if (!sdl_mouse_feature_known_for_action(y, x))
        return;

    cave_info[y][x] |= (CAVE_MARK);
    lite_spot(y, x);
}

bool sdl_mouse_path_grid_is_known_danger(int y, int x)
{
    if (!sdl_mouse_path_grid_known(y, x))
        return false;
    if (cave_feat[y][x] == FEAT_CHASM)
        return true;
    return cave_trap_bold(y, x) && !(cave_info[y][x] & (CAVE_HIDDEN));
}

bool sdl_mouse_path_grid_is_stuck_door(int y, int x)
{
    return (cave_feat[y][x] >= FEAT_DOOR_HEAD + 0x08)
        && (cave_feat[y][x] <= FEAT_DOOR_TAIL);
}

bool sdl_mouse_stuck_door_bash_target(int map_y, int map_x,
    int* out_dir)
{
    int dir;

    if (out_dir)
        *out_dir = 0;
    if (!p_ptr || !in_bounds(map_y, map_x))
        return false;
    if (!sdl_mouse_feature_known_for_action(map_y, map_x))
        return false;
    if (!cave_known_closed_door_bold(map_y, map_x))
        return false;
    if (!sdl_mouse_path_grid_is_stuck_door(map_y, map_x))
        return false;
    if (sdl_mouse_grid_has_visible_monster(map_y, map_x, NULL))
        return false;

    dir = motion_dir(p_ptr->py, p_ptr->px, map_y, map_x);
    if (dir < 1 || dir > 9 || dir == 5)
        return false;
    if ((map_y != p_ptr->py + ddy[dir])
        || (map_x != p_ptr->px + ddx[dir]))
    {
        return false;
    }

    if (out_dir)
        *out_dir = dir;
    return true;
}

int sdl_mouse_path_blocked_target_kind(int y, int x)
{
    if (!sdl_mouse_path_grid_known(y, x))
        return SDL_MOUSE_PATH_BLOCKED_NONE;
    if (sdl_mouse_grid_has_visible_monster(y, x, NULL))
        return SDL_MOUSE_PATH_BLOCKED_NONE;
    if (cave_known_closed_door_bold(y, x)
        && sdl_mouse_path_grid_is_stuck_door(y, x))
    {
        return SDL_MOUSE_PATH_BLOCKED_STUCK_DOOR;
    }
    if (sdl_mouse_path_grid_is_known_danger(y, x))
        return SDL_MOUSE_PATH_BLOCKED_DANGER;

    return SDL_MOUSE_PATH_BLOCKED_NONE;
}

bool sdl_mouse_path_grid_walkable(int y, int x)
{
    int m_idx;

    if (!sdl_mouse_path_grid_known(y, x))
        return false;

    m_idx = cave_m_idx[y][x];
    if (m_idx < 0)
        return (y == p_ptr->py) && (x == p_ptr->px);
    if ((m_idx > 0) && mon_list[m_idx].ml)
        return (y == g_mouse_path.target_y) && (x == g_mouse_path.target_x);

    if (sdl_mouse_path_grid_is_known_danger(y, x))
        return false;

    if (cave_known_closed_door_bold(y, x))
        return !sdl_mouse_path_grid_is_stuck_door(y, x);

    return sdl_mouse_path_grid_is_open_floor(y, x);
}

int sdl_mouse_path_heuristic(int y1, int x1, int y2, int x2)
{
    int dy = ABS(y1 - y2);
    int dx = ABS(x1 - x2);

    return MAX(dy, dx) * 100 + MIN(dy, dx);
}

void sdl_mouse_path_order_dirs(int y, int x, int target_y, int target_x, int dirs[8])
{
    static const int base_dirs[8] = { 1, 2, 3, 4, 6, 7, 8, 9 };

    for (int i = 0; i < 8; i++)
        dirs[i] = base_dirs[i];

    for (int i = 1; i < 8; i++) {
        int dir = dirs[i];
        int score = sdl_mouse_path_heuristic(y + ddy[dir], x + ddx[dir],
            target_y, target_x);
        int j = i - 1;

        while (j >= 0) {
            int other = dirs[j];
            int other_score = sdl_mouse_path_heuristic(y + ddy[other],
                x + ddx[other], target_y, target_x);
            if (other_score <= score)
                break;
            dirs[j + 1] = dirs[j];
            j--;
        }

        dirs[j + 1] = dir;
    }
}

bool sdl_mouse_path_is_move_dir(int dir)
{
    return (dir >= 1) && (dir <= 9) && (dir != 5);
}

int sdl_mouse_path_route_dir_index(int dir)
{
    for (int i = 0; i < SDL_MOUSE_PATH_ROUTE_DIRS; i++) {
        if (g_mouse_path_route_dirs[i] == dir)
            return i;
    }

    return -1;
}

byte sdl_mouse_path_route_state(int dir, int chain)
{
    int dir_index = sdl_mouse_path_route_dir_index(dir);

    if (dir_index < 0)
        return 0;
    if (chain < 1)
        chain = 1;
    if (chain > SDL_MOUSE_PATH_SPRINT_CHAIN_MAX)
        chain = SDL_MOUSE_PATH_SPRINT_CHAIN_MAX;

    return (byte)(1 + dir_index * SDL_MOUSE_PATH_SPRINT_CHAIN_MAX
        + (chain - 1));
}

int sdl_mouse_path_route_state_dir(byte state)
{
    if (state == 0)
        return 0;
    if (state >= SDL_MOUSE_PATH_ROUTE_STATE_COUNT)
        return 0;

    return g_mouse_path_route_dirs[
        (state - 1) / SDL_MOUSE_PATH_SPRINT_CHAIN_MAX];
}

int sdl_mouse_path_route_state_chain(byte state)
{
    if (state == 0)
        return 0;
    if (state >= SDL_MOUSE_PATH_ROUTE_STATE_COUNT)
        return 0;

    return ((state - 1) % SDL_MOUSE_PATH_SPRINT_CHAIN_MAX) + 1;
}

bool sdl_mouse_path_dirs_sprint_compatible(int newer_dir, int older_dir)
{
    if (!sdl_mouse_path_is_move_dir(newer_dir)
        || !sdl_mouse_path_is_move_dir(older_dir))
    {
        return false;
    }

    return (newer_dir == older_dir)
        || (newer_dir == cycle[chome[older_dir] - 1])
        || (newer_dir == cycle[chome[older_dir] + 1]);
}

bool sdl_mouse_path_grid_is_safe_leap_landing(int y, int x)
{
    int m_idx;

    if (!sdl_mouse_path_grid_walkable(y, x))
        return false;
    if (cave_known_closed_door_bold(y, x))
        return false;

    m_idx = cave_m_idx[y][x];
    if ((m_idx > 0) && mon_list[m_idx].ml)
        return false;

    return true;
}

bool sdl_mouse_path_state_has_run_up(int y, int x, int dir, byte state)
{
    int previous_dir = sdl_mouse_path_route_state_dir(state);

    if (!previous_dir && y == p_ptr->py && x == p_ptr->px)
        previous_dir = p_ptr->previous_action[1];

    return sdl_mouse_path_dirs_sprint_compatible(dir, previous_dir);
}

bool sdl_mouse_path_can_step_into_chasm(int y, int x, int dir,
    byte state)
{
    int mid_y;
    int mid_x;
    int land_y;
    int land_x;

    if (!p_ptr || !p_ptr->active_ability[S_EVN][EVN_LEAPING])
        return false;
    if (p_ptr->confused)
        return false;
    if (cave_pit_bold(y, x))
        return false;
    if (cave_feat[y][x] == FEAT_TRAP_WEB)
        return false;
    if (!sdl_mouse_path_state_has_run_up(y, x, dir, state))
        return false;

    mid_y = y + ddy[dir];
    mid_x = x + ddx[dir];
    land_y = mid_y + ddy[dir];
    land_x = mid_x + ddx[dir];

    if (!in_bounds(mid_y, mid_x) || !in_bounds(land_y, land_x))
        return false;
    if (cave_feat[mid_y][mid_x] != FEAT_CHASM)
        return false;
    if (!sdl_mouse_feature_known_for_action(mid_y, mid_x))
        return false;

    return sdl_mouse_path_grid_is_safe_leap_landing(land_y, land_x);
}

byte sdl_mouse_path_initial_route_state(bool sprint_enabled)
{
    int dir;
    int chain = 1;

    if (!p_ptr || !sprint_enabled)
        return 0;

    dir = p_ptr->previous_action[1];
    if (!sdl_mouse_path_is_move_dir(dir))
        return 0;

    for (int i = 1; i < SDL_MOUSE_PATH_SPRINT_CHAIN_MAX; i++) {
        int newer_dir = p_ptr->previous_action[i];
        int older_dir = p_ptr->previous_action[i + 1];

        if (!sdl_mouse_path_dirs_sprint_compatible(newer_dir, older_dir))
            break;

        chain++;
    }

    return sdl_mouse_path_route_state(dir, chain);
}

byte sdl_mouse_path_next_route_state(byte state, int dir,
    bool sprint_enabled)
{
    int previous_dir = sdl_mouse_path_route_state_dir(state);
    int chain = sdl_mouse_path_route_state_chain(state);

    if (!sprint_enabled)
        return sdl_mouse_path_route_state(dir, 1);

    if (previous_dir
        && sdl_mouse_path_dirs_sprint_compatible(dir, previous_dir))
    {
        chain++;
    }
    else
    {
        chain = 1;
    }

    return sdl_mouse_path_route_state(dir, chain);
}

int sdl_mouse_path_route_edge_cost(byte state, int dir,
    bool sprint_enabled)
{
    int previous_dir = sdl_mouse_path_route_state_dir(state);
    int chain = sdl_mouse_path_route_state_chain(state);
    int cost = (sprint_enabled && (chain >= SDL_MOUSE_PATH_SPRINT_CHAIN_MAX))
        ? SDL_MOUSE_PATH_COST_SPRINT
        : SDL_MOUSE_PATH_COST_NORMAL;

    if (previous_dir && (dir != previous_dir))
    {
        cost += sdl_mouse_path_dirs_sprint_compatible(dir, previous_dir)
            ? SDL_MOUSE_PATH_TURN_COST_SOFT
            : SDL_MOUSE_PATH_TURN_COST_HARD;
    }

    return cost;
}

int sdl_mouse_path_route_heuristic(int y, int x, int target_y,
    int target_x, bool sprint_enabled)
{
    int dy = ABS(y - target_y);
    int dx = ABS(x - target_x);
    int min_step_cost = sprint_enabled ? SDL_MOUSE_PATH_COST_SPRINT
                                       : SDL_MOUSE_PATH_COST_NORMAL;

    return MAX(dy, dx) * min_step_cost;
}

size_t sdl_mouse_path_search_index(int y, int x, byte state)
{
    return (((size_t)y * (size_t)g_mouse_path_search.width + (size_t)x)
        * (size_t)SDL_MOUSE_PATH_ROUTE_STATE_COUNT)
        + (size_t)state;
}

size_t sdl_mouse_path_search_grid_state_index(u16b grid, byte state)
{
    return sdl_mouse_path_search_index(GRID_Y(grid), GRID_X(grid), state);
}

void sdl_mouse_path_search_free(void)
{
    SDL_free(g_mouse_path_search.cost);
    SDL_free(g_mouse_path_search.heap_pos);
    SDL_free(g_mouse_path_search.parent_grid);
    SDL_free(g_mouse_path_search.parent_state);
    SDL_free(g_mouse_path_search.heap_priority);
    SDL_free(g_mouse_path_search.heap_grid);
    SDL_free(g_mouse_path_search.heap_state);
    memset(&g_mouse_path_search, 0, sizeof(g_mouse_path_search));
}

bool sdl_mouse_path_search_ensure(size_t state_count)
{
    if (g_mouse_path_search.capacity >= state_count)
        return true;

    sdl_mouse_path_search_free();

    g_mouse_path_search.cost = (int*)SDL_calloc(state_count, sizeof(int));
    g_mouse_path_search.heap_pos = (int*)SDL_calloc(state_count, sizeof(int));
    g_mouse_path_search.parent_grid =
        (u16b*)SDL_calloc(state_count, sizeof(u16b));
    g_mouse_path_search.parent_state =
        (byte*)SDL_calloc(state_count, sizeof(byte));
    g_mouse_path_search.heap_priority =
        (int*)SDL_calloc(state_count, sizeof(int));
    g_mouse_path_search.heap_grid =
        (u16b*)SDL_calloc(state_count, sizeof(u16b));
    g_mouse_path_search.heap_state =
        (byte*)SDL_calloc(state_count, sizeof(byte));

    if (!g_mouse_path_search.cost || !g_mouse_path_search.heap_pos
        || !g_mouse_path_search.parent_grid
        || !g_mouse_path_search.parent_state
        || !g_mouse_path_search.heap_priority
        || !g_mouse_path_search.heap_grid || !g_mouse_path_search.heap_state)
    {
        sdl_mouse_path_search_free();
        return false;
    }

    g_mouse_path_search.capacity = state_count;
    return true;
}

bool sdl_mouse_path_heap_less(int a, int b)
{
    int a_priority = g_mouse_path_search.heap_priority[a];
    int b_priority = g_mouse_path_search.heap_priority[b];

    if (a_priority != b_priority)
        return a_priority < b_priority;

    return g_mouse_path_search.cost[sdl_mouse_path_search_grid_state_index(
               g_mouse_path_search.heap_grid[a],
               g_mouse_path_search.heap_state[a])]
        < g_mouse_path_search.cost[sdl_mouse_path_search_grid_state_index(
               g_mouse_path_search.heap_grid[b],
               g_mouse_path_search.heap_state[b])];
}

void sdl_mouse_path_heap_swap(int a, int b)
{
    int priority = g_mouse_path_search.heap_priority[a];
    u16b grid = g_mouse_path_search.heap_grid[a];
    byte state = g_mouse_path_search.heap_state[a];

    g_mouse_path_search.heap_priority[a] =
        g_mouse_path_search.heap_priority[b];
    g_mouse_path_search.heap_grid[a] = g_mouse_path_search.heap_grid[b];
    g_mouse_path_search.heap_state[a] = g_mouse_path_search.heap_state[b];
    g_mouse_path_search.heap_priority[b] = priority;
    g_mouse_path_search.heap_grid[b] = grid;
    g_mouse_path_search.heap_state[b] = state;

    g_mouse_path_search.heap_pos[sdl_mouse_path_search_grid_state_index(
        g_mouse_path_search.heap_grid[a],
        g_mouse_path_search.heap_state[a])] = a;
    g_mouse_path_search.heap_pos[sdl_mouse_path_search_grid_state_index(
        g_mouse_path_search.heap_grid[b],
        g_mouse_path_search.heap_state[b])] = b;
}

void sdl_mouse_path_heap_sift_up(int pos)
{
    while (pos > 0) {
        int parent = (pos - 1) / 2;

        if (!sdl_mouse_path_heap_less(pos, parent))
            break;

        sdl_mouse_path_heap_swap(pos, parent);
        pos = parent;
    }
}

void sdl_mouse_path_heap_sift_down(int pos)
{
    while (true) {
        int left = pos * 2 + 1;
        int right = left + 1;
        int best = pos;

        if (left < g_mouse_path_search.heap_size
            && sdl_mouse_path_heap_less(left, best))
        {
            best = left;
        }

        if (right < g_mouse_path_search.heap_size
            && sdl_mouse_path_heap_less(right, best))
        {
            best = right;
        }

        if (best == pos)
            break;

        sdl_mouse_path_heap_swap(pos, best);
        pos = best;
    }
}

bool sdl_mouse_path_heap_insert_or_decrease(u16b grid, byte state,
    int priority)
{
    size_t idx = sdl_mouse_path_search_grid_state_index(grid, state);
    int pos = g_mouse_path_search.heap_pos[idx];

    if (pos >= 0)
    {
        g_mouse_path_search.heap_priority[pos] = priority;
        sdl_mouse_path_heap_sift_up(pos);
        return true;
    }

    if ((size_t)g_mouse_path_search.heap_size
        >= g_mouse_path_search.capacity)
    {
        return false;
    }

    pos = g_mouse_path_search.heap_size++;
    g_mouse_path_search.heap_priority[pos] = priority;
    g_mouse_path_search.heap_grid[pos] = grid;
    g_mouse_path_search.heap_state[pos] = state;
    g_mouse_path_search.heap_pos[idx] = pos;
    sdl_mouse_path_heap_sift_up(pos);
    return true;
}

bool sdl_mouse_path_heap_pop(u16b* grid, byte* state)
{
    size_t root_idx;

    if (g_mouse_path_search.heap_size <= 0)
        return false;

    *grid = g_mouse_path_search.heap_grid[0];
    *state = g_mouse_path_search.heap_state[0];
    root_idx = sdl_mouse_path_search_grid_state_index(*grid, *state);
    g_mouse_path_search.heap_pos[root_idx] = -1;

    g_mouse_path_search.heap_size--;
    if (g_mouse_path_search.heap_size > 0)
    {
        g_mouse_path_search.heap_priority[0] =
            g_mouse_path_search.heap_priority[g_mouse_path_search.heap_size];
        g_mouse_path_search.heap_grid[0] =
            g_mouse_path_search.heap_grid[g_mouse_path_search.heap_size];
        g_mouse_path_search.heap_state[0] =
            g_mouse_path_search.heap_state[g_mouse_path_search.heap_size];
        g_mouse_path_search.heap_pos[sdl_mouse_path_search_grid_state_index(
            g_mouse_path_search.heap_grid[0],
            g_mouse_path_search.heap_state[0])] = 0;
        sdl_mouse_path_heap_sift_down(0);
    }

    return true;
}

bool sdl_mouse_path_build_from_search(int target_y, int target_x,
    byte target_state)
{
    u16b start = GRID(p_ptr->py, p_ptr->px);
    u16b here = GRID(target_y, target_x);
    byte state = target_state;
    int reverse_len = 0;

    while (here != start) {
        int y = GRID_Y(here);
        int x = GRID_X(here);
        size_t idx;
        u16b parent;
        byte parent_state;

        if (!in_bounds(y, x))
            return false;
        if (state >= SDL_MOUSE_PATH_ROUTE_STATE_COUNT)
            return false;
        if (reverse_len >= SDL_MOUSE_PATH_MAX_GRIDS)
            return false;

        idx = sdl_mouse_path_search_grid_state_index(here, state);
        parent = g_mouse_path_search.parent_grid[idx];
        parent_state = g_mouse_path_search.parent_state[idx];

        if ((parent == here) && (parent_state == state))
            return false;
        if (!in_bounds(GRID_Y(parent), GRID_X(parent)))
            return false;

        g_mouse_path_reverse[reverse_len++] = here;
        here = parent;
        state = parent_state;
    }

    g_mouse_path.path_len = reverse_len;
    for (int i = 0; i < reverse_len; i++)
        g_mouse_path.path[i] = g_mouse_path_reverse[reverse_len - i - 1];

    return reverse_len > 0;
}

bool sdl_mouse_path_compute_route(int target_y, int target_x,
    bool sprint_enabled)
{
    int map_hgt = p_ptr->cur_map_hgt;
    int map_wid = p_ptr->cur_map_wid;
    size_t state_count = (size_t)map_hgt * (size_t)map_wid
        * (size_t)SDL_MOUSE_PATH_ROUTE_STATE_COUNT;
    u16b start_grid = GRID(p_ptr->py, p_ptr->px);
    byte start_state = sdl_mouse_path_initial_route_state(sprint_enabled);
    size_t start_idx;

    if (!sdl_mouse_path_search_ensure(state_count))
        return false;

    g_mouse_path_search.width = map_wid;
    g_mouse_path_search.heap_size = 0;
    memset(g_mouse_path_search.cost, 0x3f,
        state_count * sizeof(*g_mouse_path_search.cost));
    memset(g_mouse_path_search.heap_pos, 0xFF,
        state_count * sizeof(*g_mouse_path_search.heap_pos));

    start_idx = sdl_mouse_path_search_grid_state_index(start_grid, start_state);
    g_mouse_path_search.cost[start_idx] = 0;
    g_mouse_path_search.parent_grid[start_idx] = start_grid;
    g_mouse_path_search.parent_state[start_idx] = start_state;
    if (!sdl_mouse_path_heap_insert_or_decrease(start_grid, start_state,
            sdl_mouse_path_route_heuristic(
                p_ptr->py, p_ptr->px, target_y, target_x, sprint_enabled)))
    {
        return false;
    }

    while (g_mouse_path_search.heap_size > 0) {
        u16b grid;
        byte state;
        int y;
        int x;
        size_t idx;
        int base_cost;
        int dirs[8];

        if (!sdl_mouse_path_heap_pop(&grid, &state))
            break;

        y = GRID_Y(grid);
        x = GRID_X(grid);
        idx = sdl_mouse_path_search_grid_state_index(grid, state);
        base_cost = g_mouse_path_search.cost[idx];

        if ((y == target_y) && (x == target_x))
            return sdl_mouse_path_build_from_search(target_y, target_x, state);

        sdl_mouse_path_order_dirs(y, x, target_y, target_x, dirs);
        for (int i = 0; i < 8; i++) {
            int dir = dirs[i];
            int ny = y + ddy[dir];
            int nx = x + ddx[dir];
            bool chasm_step = false;
            byte next_state;
            u16b next_grid;
            size_t next_idx;
            int next_cost;
            int priority;

            if (!in_bounds(ny, nx))
                continue;
            if (cave_feat[y][x] == FEAT_CHASM) {
                int previous_dir = sdl_mouse_path_route_state_dir(state);

                if (dir != previous_dir)
                    continue;
                if (!sdl_mouse_path_grid_is_safe_leap_landing(ny, nx))
                    continue;
            } else if (!sdl_mouse_path_grid_walkable(ny, nx)) {
                if (!sdl_mouse_path_can_step_into_chasm(y, x, dir, state))
                    continue;
                chasm_step = true;
            }

            next_state = sdl_mouse_path_next_route_state(
                state, dir, sprint_enabled);
            next_grid = GRID(ny, nx);
            next_idx = sdl_mouse_path_search_grid_state_index(
                next_grid, next_state);
            next_cost =
                base_cost + sdl_mouse_path_route_edge_cost(
                    state, dir, sprint_enabled);
            if (chasm_step)
                next_cost += SDL_MOUSE_PATH_COST_NORMAL;

            if (next_cost >= g_mouse_path_search.cost[next_idx])
                continue;

            priority = next_cost
                + sdl_mouse_path_route_heuristic(
                    ny, nx, target_y, target_x, sprint_enabled);
            g_mouse_path_search.cost[next_idx] = next_cost;
            g_mouse_path_search.parent_grid[next_idx] = grid;
            g_mouse_path_search.parent_state[next_idx] = state;

            if (!sdl_mouse_path_heap_insert_or_decrease(
                    next_grid, next_state, priority))
            {
                return false;
            }
        }
    }

    return false;
}

bool sdl_mouse_path_compute_blocked_target_route(int target_y,
    int target_x, bool sprint_enabled)
{
    int dir;
    int dirs[8];

    dir = motion_dir(p_ptr->py, p_ptr->px, target_y, target_x);
    if (dir >= 1 && dir <= 9 && dir != 5
        && target_y == p_ptr->py + ddy[dir]
        && target_x == p_ptr->px + ddx[dir])
    {
        g_mouse_path.path_len = 0;
        return true;
    }

    sdl_mouse_path_order_dirs(target_y, target_x, p_ptr->py, p_ptr->px, dirs);
    for (int i = 0; i < 8; i++) {
        int approach_y = target_y + ddy[dirs[i]];
        int approach_x = target_x + ddx[dirs[i]];

        if (!in_bounds(approach_y, approach_x))
            continue;
        if (!sdl_mouse_path_grid_walkable(approach_y, approach_x))
            continue;
        if (approach_y == p_ptr->py && approach_x == p_ptr->px) {
            g_mouse_path.path_len = 0;
            return true;
        }
        if (sdl_mouse_path_compute_route(approach_y, approach_x,
                sprint_enabled))
        {
            return true;
        }
    }

    return false;
}

bool sdl_mouse_path_compute(int target_y, int target_x)
{
    int target_m_idx = 0;
    int blocked_target_kind = SDL_MOUSE_PATH_BLOCKED_NONE;
    bool sprint_enabled;

    g_mouse_path.path_valid = false;
    g_mouse_path.path_len = 0;
    g_mouse_path.blocked_target_kind = SDL_MOUSE_PATH_BLOCKED_NONE;

    if (!sdl_mouse_gameplay_context_active())
        return false;
    if (!in_bounds(target_y, target_x))
        return false;

    g_mouse_path.source_y = p_ptr->py;
    g_mouse_path.source_x = p_ptr->px;
    g_mouse_path.target_y = target_y;
    g_mouse_path.target_x = target_x;
    (void)sdl_mouse_grid_has_visible_monster(target_y, target_x, &target_m_idx);
    g_mouse_path.target_m_idx = target_m_idx;

    if ((target_y == p_ptr->py) && (target_x == p_ptr->px))
        return false;
    if (!sdl_mouse_path_grid_walkable(target_y, target_x))
    {
        blocked_target_kind =
            sdl_mouse_path_blocked_target_kind(target_y, target_x);
        if (blocked_target_kind == SDL_MOUSE_PATH_BLOCKED_NONE)
            return false;

        sprint_enabled =
            p_ptr->active_ability[S_EVN][EVN_SPRINTING] ? true : false;
        if (!sdl_mouse_path_compute_blocked_target_route(target_y, target_x,
                sprint_enabled))
        {
            if (blocked_target_kind == SDL_MOUSE_PATH_BLOCKED_DANGER)
            {
                g_mouse_path.blocked_target_kind = blocked_target_kind;
                g_mouse_path.path_len = 0;
                g_mouse_path.path_valid = true;
                return true;
            }
            return false;
        }

        g_mouse_path.blocked_target_kind = blocked_target_kind;
        g_mouse_path.path_valid = true;
        return true;
    }

    sprint_enabled = p_ptr->active_ability[S_EVN][EVN_SPRINTING] ? true : false;
    if (!sdl_mouse_path_compute_route(target_y, target_x, sprint_enabled))
        return false;

    g_mouse_path.path_valid = true;
    return true;
}

bool sdl_mouse_path_select_grid(int map_y, int map_x, bool report_failure)
{
    bool ok;

    g_mouse_path.hover_visible = true;
    ok = sdl_mouse_path_compute(map_y, map_x);
    if (!ok && report_failure)
        bell("No mouse path.");
    g_state.need_present = true;
    return ok;
}

bool sdl_mouse_path_start_follow_grid(int map_y, int map_x)
{
    if (!sdl_mouse_path_select_grid(map_y, map_x, true))
        return false;

    g_mouse_path.follow_active = true;
    g_mouse_path.path_wake_pending = true;
    g_map_touch_selected = false;
    g_state.need_present = true;
    Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}

void sdl_mouse_path_cancel(void)
{
    if (!g_mouse_path.hover_visible && !g_mouse_path.follow_active
        && !g_mouse_path.path_valid && !g_map_touch_press.active
        && !g_map_touch_selected && !g_mouse_path.stuck_door_bash_pending)
    {
        return;
    }

    g_map_touch_press.active = false;
    g_map_touch_press.repeat_target = false;
    g_map_touch_selected = false;
    g_mouse_path.hover_visible = false;
    g_mouse_path.follow_active = false;
    g_mouse_path.path_valid = false;
    g_mouse_path.blocked_target_kind = SDL_MOUSE_PATH_BLOCKED_NONE;
    g_mouse_path.path_wake_pending = false;
    g_mouse_path.target_m_idx = 0;
    g_mouse_path.path_len = 0;
    g_mouse_path.stuck_door_bash_pending = false;
    g_mouse_path.stuck_door_bash_y = 0;
    g_mouse_path.stuck_door_bash_x = 0;
    g_mouse_path.stuck_door_bash_dir = 0;
    g_mouse_path.grid_question_pending = false;
    g_mouse_path.grid_question_y = 0;
    g_mouse_path.grid_question_x = 0;
    g_mouse_path.last_step_door_pending = false;
    g_mouse_path.last_step_door_y = 0;
    g_mouse_path.last_step_door_x = 0;
    g_mouse_path.last_step_player_y = 0;
    g_mouse_path.last_step_player_x = 0;
    g_state.need_present = true;
}

bool sdl_mouse_stuck_door_bash_queue_prompt(int map_y, int map_x)
{
    int dir = 0;

    if (!sdl_mouse_stuck_door_bash_target(map_y, map_x, &dir))
        return false;

    sdl_mouse_note_feature_for_action(map_y, map_x);
    sdl_mouse_path_cancel();
    g_mouse_path.stuck_door_bash_pending = true;
    g_mouse_path.stuck_door_bash_y = map_y;
    g_mouse_path.stuck_door_bash_x = map_x;
    g_mouse_path.stuck_door_bash_dir = dir;
    g_state.need_present = true;
    Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}

bool sdl_mouse_stuck_door_handle_left_click(int map_y, int map_x)
{
    if (sdl_mouse_stuck_door_bash_queue_prompt(map_y, map_x))
        return true;
    if (sdl_mouse_movement_normalized_mode(config.mouse_movement_mode)
        == SDL_MOUSE_MOVEMENT_OFF)
    {
        return false;
    }
    if (sdl_mouse_path_blocked_target_kind(map_y, map_x)
        != SDL_MOUSE_PATH_BLOCKED_STUCK_DOOR)
    {
        return false;
    }

    return sdl_mouse_path_start_follow_grid(map_y, map_x);
}

int sdl_mouse_movement_normalized_mode(int mode)
{
    if (mode == SDL_MOUSE_MOVEMENT_OFF
        || mode == SDL_MOUSE_MOVEMENT_RIGHT_ONLY)
    {
        return mode;
    }

    return SDL_MOUSE_MOVEMENT_ON;
}

void sdl_mouse_path_handle_motion(float x, float y)
{
    int map_y = 0;
    int map_x = 0;
    bool had_visible = g_mouse_path.hover_visible;
    bool had_valid = g_mouse_path.path_valid;
    int old_target_y = g_mouse_path.target_y;
    int old_target_x = g_mouse_path.target_x;

    if (sdl_mouse_movement_normalized_mode(config.mouse_movement_mode)
        == SDL_MOUSE_MOVEMENT_OFF)
    {
        if (g_mouse_path.hover_visible || g_mouse_path.path_valid
            || g_mouse_path.follow_active)
        {
            sdl_mouse_path_cancel();
        }
        return;
    }

    if (g_mouse_path.follow_active)
        return;

    /* Only refresh the hover path while idle, waiting for the player's command.
     * During turn processing (movement, combat, animations) the event pump
     * still delivers motion events, but recomputing the path and re-presenting
     * then just churns the overlay and spins the loop -- leave the existing
     * preview as-is until it is the player's turn again. */
    if (!g_sdl_blocking_key_wait)
        return;

    if (!sdl_main_view_point_to_map(x, y, &map_y, &map_x)) {
        if (g_mouse_path.hover_visible || g_mouse_path.path_valid) {
            g_mouse_path.hover_visible = false;
            g_mouse_path.path_valid = false;
            g_mouse_path.path_len = 0;
            g_state.need_present = true;
        }
        return;
    }

    if (g_map_touch_selected
        && (map_y != g_map_touch_selected_y || map_x != g_map_touch_selected_x))
    {
        g_map_touch_selected = false;
    }

    /* The hover path only depends on the target grid and the (stationary)
     * player position.  A single map cell spans many pixels, so dragging the
     * cursor fires a burst of motion events that all map to the same grid;
     * recomputing the full search and re-presenting for each one spins the
     * event loop and churns the path overlay.  Skip when we already have a
     * hover path for this exact grid from the current player position. */
    if (g_mouse_path.hover_visible
        && map_y == g_mouse_path.target_y
        && map_x == g_mouse_path.target_x
        && g_mouse_path.source_y == p_ptr->py
        && g_mouse_path.source_x == p_ptr->px)
    {
        return;
    }

    /* Rate-limit the recompute.  On a large level (this one is 187x187, ~1.1M
     * search states) a single hover search is heavy, and sweeping the cursor
     * toward a target crosses many cells; without this, each crossed cell
     * re-runs the search and re-presents the frame, which is the residual
     * "delay"/overlay churn while aiming.  Recompute at most ~10x/second.  The
     * dedupe above leaves the target grid unchanged when we skip, so the next
     * motion still refreshes once the cursor settles, and clicking always
     * computes a fresh path via sdl_mouse_path_start_follow_grid(). */
    {
        static Uint64 last_hover_compute_ns = 0;
        Uint64 now_ns = SDL_GetTicksNS();

        if (last_hover_compute_ns != 0
            && now_ns - last_hover_compute_ns < 100000000ULL)
        {
            return;
        }
        last_hover_compute_ns = now_ns;
    }

    g_mouse_path.hover_visible = true;
    (void)sdl_mouse_path_compute(map_y, map_x);

    if (had_visible != g_mouse_path.hover_visible
        || had_valid != g_mouse_path.path_valid
        || old_target_y != g_mouse_path.target_y
        || old_target_x != g_mouse_path.target_x)
    {
        g_state.need_present = true;
    }
}

bool sdl_mouse_path_handle_movement_click(float x, float y)
{
    int map_y = 0;
    int map_x = 0;

    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (!sdl_main_view_point_to_map(x, y, &map_y, &map_x))
        return false;

    if (sdl_mouse_stuck_door_bash_queue_prompt(map_y, map_x))
        return true;

    (void)sdl_mouse_path_start_follow_grid(map_y, map_x);
    return true;
}

bool sdl_mouse_path_handle_left_click(float x, float y)
{
    int map_y = 0;
    int map_x = 0;

    if (sdl_main_screen_click_shortcuts_active()
        && sdl_main_view_point_to_map(x, y, &map_y, &map_x)
        && map_y == p_ptr->py && map_x == p_ptr->px)
    {
        sdl_player_confirm_at_player();
        return true;
    }

    if (sdl_main_screen_click_shortcuts_active()
        && sdl_main_view_point_to_map(x, y, &map_y, &map_x)
        && sdl_mouse_stuck_door_handle_left_click(map_y, map_x))
    {
        return true;
    }

    if (sdl_mouse_movement_normalized_mode(config.mouse_movement_mode)
        != SDL_MOUSE_MOVEMENT_ON)
    {
        return false;
    }

    return sdl_mouse_path_handle_movement_click(x, y);
}

/*
 * True when right-clicking / long-pressing the given grid should open the
 * grid interaction popup (adjacent doors, traps, walls, veins, chests,
 * skeletons and empty/dark squares).
 */
bool sdl_mouse_feature_action_for_grid(int map_y, int map_x,
    int* out_command, int* out_dir)
{
    if (out_command)
        *out_command = 0;
    if (out_dir)
        *out_dir = 0;
    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (!p_ptr || !in_bounds(map_y, map_x))
        return false;

    return grid_interact_available(map_y, map_x);
}

/*
 * Queue the grid interaction question for the game loop: the popup itself
 * (and any blocking input) must run in game context, not in the SDL event
 * handler, so this just records the grid and wakes the command loop;
 * sdl_grid_question_take_command picks it up from request_command.
 */
bool sdl_grid_question_queue(int map_y, int map_x)
{
    if (!sdl_mouse_feature_action_for_grid(map_y, map_x, NULL, NULL))
        return false;

    sdl_mouse_path_cancel();
    g_mouse_path.grid_question_pending = true;
    g_mouse_path.grid_question_y = map_y;
    g_mouse_path.grid_question_x = map_x;
    g_state.need_present = true;
    Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}

bool sdl_mouse_feature_action_queue_grid(int map_y, int map_x)
{
    return sdl_grid_question_queue(map_y, map_x);
}

bool sdl_mouse_path_handle_right_movement_click(float x, float y)
{
    if (sdl_mouse_movement_normalized_mode(config.mouse_movement_mode)
        != SDL_MOUSE_MOVEMENT_RIGHT_ONLY)
    {
        return false;
    }

    return sdl_mouse_path_handle_movement_click(x, y);
}

bool sdl_mouse_path_handle_right_click(float x, float y)
{
    int map_y = 0;
    int map_x = 0;

    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (!sdl_main_view_point_to_map(x, y, &map_y, &map_x))
        return false;

    if (map_y != p_ptr->py || map_x != p_ptr->px)
    {
        if (sdl_mouse_feature_action_queue_grid(map_y, map_x))
            return true;
        if (sdl_mouse_grid_has_describable_content(map_y, map_x))
            return false;
        return false;
    }

    return sdl_player_action_menu_open();
}

bool sdl_mouse_consume_wake_key(void)
{
    char ch = '\0';

    if (!Term)
        return false;
    if (Term_inkey(&ch, false, false) != 0)
        return false;
    if (ch != UI_MENU_CLICK_WAKE_KEY)
        return false;

    (void)Term_inkey(&ch, false, true);
    return true;
}


