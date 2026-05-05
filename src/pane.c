#include "pane.h"

#define BOTTOM_PLACEMENTS \
    (PLACE_BOTTOM | PLACE_DOUBLE_BOTTOM)
#define SIDE_PLACEMENTS \
    (PLACE_LEFT | PLACE_RIGHT | PLACE_DOUBLE_LEFT | PLACE_DOUBLE_RIGHT)

static const enum pane_placement pane_placement_order[] = {
    PLACE_LEFT,
    PLACE_RIGHT,
    PLACE_DOUBLE_LEFT,
    PLACE_DOUBLE_RIGHT,
    PLACE_BOTTOM,
    PLACE_DOUBLE_BOTTOM,
};

static const enum pane_placement pane_default_order[] = {
    PLACE_RIGHT,
    PLACE_LEFT,
    PLACE_DOUBLE_RIGHT,
    PLACE_DOUBLE_LEFT,
    PLACE_BOTTOM,
    PLACE_DOUBLE_BOTTOM,
};

static struct pane_specs pane_specs[PANE_MAX] = {
    [PANE_INVENTORY] = {.placement = SIDE_PLACEMENTS, .min_rect.rows = 22, .min_rect.cols = 40},
    [PANE_WORN] = {.placement = SIDE_PLACEMENTS, .min_rect.rows = 17, .min_rect.cols = 40},
    [PANE_ROLLS] = {.placement = BOTTOM_PLACEMENTS, .min_rect.rows = 1, .min_rect.cols = 65},
    [PANE_INFO] = {.placement = SIDE_PLACEMENTS | BOTTOM_PLACEMENTS, .min_rect.rows = 1, .min_rect.cols = 40},
    [PANE_CHARACTER] = {.placement = SIDE_PLACEMENTS | BOTTOM_PLACEMENTS, .min_rect.cols = 60},
    [PANE_LOG] = {.placement = SIDE_PLACEMENTS | BOTTOM_PLACEMENTS, .min_rect.rows = 1, .min_rect.cols = 40},
    [PANE_MONSTERS] = {.placement = SIDE_PLACEMENTS, .min_rect.rows = 1, .min_rect.cols = 40},
    [PANE_MAP] = {.placement = SIDE_PLACEMENTS, .min_rect.rows = 10, .min_rect.cols = 24},
    [PANE_TOUCH] = {.placement = PLACE_DOUBLE_LEFT | PLACE_DOUBLE_RIGHT, .min_rect.rows = 12, .min_rect.cols = 12},
};

static bool pane_placement_is_left(enum pane_placement where)
{
    return (where == PLACE_LEFT || where == PLACE_DOUBLE_LEFT);
}

static bool pane_placement_is_right(enum pane_placement where)
{
    return (where == PLACE_RIGHT || where == PLACE_DOUBLE_RIGHT);
}

bool pane_placement_is_bottom(enum pane_placement where)
{
    return (where == PLACE_BOTTOM || where == PLACE_DOUBLE_BOTTOM);
}

static int pane_primary_cell_px(enum pane_type type, enum pane_placement where,
    const int* cell_widths, const int* cell_heights)
{
    return pane_placement_is_bottom(where) ? cell_heights[type] : cell_widths[type];
}

static int pane_secondary_cell_px(enum pane_type type, enum pane_placement where,
    const int* cell_widths, const int* cell_heights)
{
    return pane_placement_is_bottom(where) ? cell_widths[type] : cell_heights[type];
}

static int pane_primary_size_cells(const struct pane_config* config,
    enum pane_placement where)
{
    return pane_placement_is_bottom(where) ? config->rect.rows : config->rect.cols;
}

int pane_primary_min_cells(enum pane_type type, enum pane_placement where)
{
    return pane_placement_is_bottom(where)
        ? pane_specs[type].min_rect.rows
        : pane_specs[type].min_rect.cols;
}

int pane_secondary_min_cells(enum pane_type type, enum pane_placement where)
{
    return pane_placement_is_bottom(where)
        ? pane_specs[type].min_rect.cols
        : pane_specs[type].min_rect.rows;
}

static int pane_group_primary_pixels(const struct pane_config* config, int count,
    enum pane_placement where, const int* cell_widths, const int* cell_heights,
    int margin_px)
{
    int pixels = 0;

    for (int i = 0; i < count; i++) {
        int requested;
        int minimum;
        int candidate_cells;
        int candidate_px;

        if (!config[i].enabled || config[i].where != where)
            continue;

        requested = pane_primary_size_cells(&config[i], where);
        minimum = pane_primary_min_cells(config[i].pane, where);
        candidate_cells = (requested > minimum) ? requested : minimum;
        candidate_px = candidate_cells
            * pane_primary_cell_px(config[i].pane, where, cell_widths, cell_heights)
            + margin_px;
        if (candidate_px > pixels)
            pixels = candidate_px;
    }

    return pixels;
}

static int pane_group_count(const struct pane_config* config, int count,
    enum pane_placement where)
{
    int active = 0;

    for (int i = 0; i < count; i++) {
        if (config[i].enabled && config[i].where == where)
            active++;
    }

    return active;
}

static bool pane_group_is_last_enabled(const struct pane_config* config, int count,
    enum pane_placement where, int index)
{
    for (int i = index + 1; i < count; i++) {
        if (config[i].enabled && config[i].where == where)
            return false;
    }

    return true;
}

static void layout_bottom_group(enum pane_placement where,
    const struct pane_config* config, int count, SDL_Rect* panes,
    SDL_Rect* area, const int* cell_widths, const int* cell_heights,
    int margin_px)
{
    int active_count = pane_group_count(config, count, where);
    int actual_sizes[PANE_MAX] = { 0 };
    int split_px;
    int distr_px;
    int distr_count = 0;
    int coord;

    if (active_count <= 0)
        return;

    split_px = pane_group_primary_pixels(config, count, where, cell_widths,
        cell_heights, margin_px);
    distr_px = area->w;

    for (int i = 0; i < count; i++) {
        int pane_px;
        int cells;
        int min_size;
        int cell_px;

        if (!config[i].enabled || config[i].where != where)
            continue;

        if (config[i].rect.cols > 0) {
            cell_px = pane_secondary_cell_px(config[i].pane, where,
                cell_widths, cell_heights);
            actual_sizes[i] = cell_px * config[i].rect.cols + margin_px;
            distr_px -= actual_sizes[i];
            continue;
        }

        if (config[i].ratio > 0.0f) {
            pane_px = (int)(area->w * config[i].ratio);
            cell_px = pane_secondary_cell_px(config[i].pane, where,
                cell_widths, cell_heights);
            cells = (pane_px - margin_px) / cell_px;
            min_size = pane_secondary_min_cells(config[i].pane, where);
            if (cells < min_size)
                pane_px = cell_px * min_size + margin_px;
            actual_sizes[i] = pane_px;
            distr_px -= pane_px;
            continue;
        }

        distr_count++;
    }

    coord = area->x;
    for (int i = 0; i < count; i++) {
        SDL_Rect* pane;
        int pane_px;
        int cells;
        int min_size;
        int cell_px;
        int remaining_px;

        if (!config[i].enabled || config[i].where != where)
            continue;

        if (actual_sizes[i] > 0) {
            pane_px = actual_sizes[i];
        } else {
            pane_px = distr_px / distr_count;
            cell_px = pane_secondary_cell_px(config[i].pane, where,
                cell_widths, cell_heights);
            cells = (pane_px - margin_px) / cell_px;
            min_size = pane_secondary_min_cells(config[i].pane, where);
            if (cells < min_size)
                pane_px = cell_px * min_size + margin_px;
            distr_px -= pane_px;
            distr_count--;
        }

        remaining_px = area->x + area->w - coord;
        if (remaining_px < 0)
            remaining_px = 0;
        if (pane_group_is_last_enabled(config, count, where, i))
            pane_px = remaining_px;
        if (pane_px > remaining_px)
            pane_px = remaining_px;

        pane = &panes[config[i].pane];
        *pane = (SDL_Rect){
            .x = coord,
            .y = area->y + area->h - split_px,
            .w = pane_px,
            .h = split_px,
        };
        coord += pane_px;
    }

    area->h -= split_px;
}

static void layout_side_group(enum pane_placement where,
    const struct pane_config* config, int count, SDL_Rect* panes, SDL_Rect* area,
    const int* cell_widths, const int* cell_heights, int margin_px)
{
    int active_count = pane_group_count(config, count, where);
    int actual_sizes[PANE_MAX] = { 0 };
    int split_px;
    int distr_px;
    int distr_count = 0;
    int pane_x;
    int coord;

    if (active_count <= 0)
        return;

    split_px = pane_group_primary_pixels(config, count, where, cell_widths,
        cell_heights, margin_px);
    distr_px = area->h;

    for (int i = 0; i < count; i++) {
        int pane_px;
        int cells;
        int min_size;
        int cell_px;

        if (!config[i].enabled || config[i].where != where)
            continue;

        if (config[i].rect.rows > 0) {
            cell_px = pane_secondary_cell_px(config[i].pane, where, cell_widths,
                cell_heights);
            actual_sizes[i] = cell_px * config[i].rect.rows + margin_px;
            distr_px -= actual_sizes[i];
            continue;
        }

        if (config[i].ratio > 0.0f) {
            pane_px = (int)(area->h * config[i].ratio);
            cell_px = pane_secondary_cell_px(config[i].pane, where, cell_widths,
                cell_heights);
            cells = (pane_px - margin_px) / cell_px;
            min_size = pane_secondary_min_cells(config[i].pane, where);
            if (cells < min_size)
                pane_px = cell_px * min_size + margin_px;
            actual_sizes[i] = pane_px;
            distr_px -= pane_px;
            continue;
        }

        distr_count++;
    }

    pane_x = pane_placement_is_left(where)
        ? area->x
        : area->x + area->w - split_px;
    coord = area->y;
    for (int i = 0; i < count; i++) {
        SDL_Rect* pane;
        int pane_px;
        int cells;
        int min_size;
        int cell_px;
        int remaining_px;

        if (!config[i].enabled || config[i].where != where)
            continue;

        if (actual_sizes[i] > 0) {
            pane_px = actual_sizes[i];
        } else {
            pane_px = distr_px / distr_count;
            cell_px = pane_secondary_cell_px(config[i].pane, where, cell_widths,
                cell_heights);
            cells = (pane_px - margin_px) / cell_px;
            min_size = pane_secondary_min_cells(config[i].pane, where);
            if (cells < min_size)
                pane_px = cell_px * min_size + margin_px;
            distr_px -= pane_px;
            distr_count--;
        }

        remaining_px = area->y + area->h - coord;
        if (remaining_px < 0)
            remaining_px = 0;
        if (pane_group_is_last_enabled(config, count, where, i))
            pane_px = remaining_px;
        if (pane_px > remaining_px)
            pane_px = remaining_px;

        pane = &panes[config[i].pane];
        *pane = (SDL_Rect){
            .x = pane_x,
            .y = coord,
            .w = split_px,
            .h = pane_px,
        };
        coord += pane_px;
    }

    if (pane_placement_is_left(where))
        area->x += split_px;
    area->w -= split_px;
}

bool pane_placement_is_side(enum pane_placement where)
{
    return (pane_placement_is_left(where) || pane_placement_is_right(where));
}

static bool pane_layout_has_side_touch_pane(const struct pane_config* config,
    int count)
{
    for (int i = 0; i < count; i++) {
        if (!config[i].enabled)
            continue;
        if (config[i].pane != PANE_TOUCH)
            continue;
        if (pane_placement_is_side(config[i].where))
            return true;
    }

    return false;
}

bool pane_type_allows_placement(enum pane_type type, enum pane_placement where)
{
    if (type <= PANE_MAIN || type >= PANE_MAX)
        return false;

    return ((pane_specs[type].placement & where) != 0);
}

enum pane_placement pane_first_allowed_placement(enum pane_type type)
{
    for (int i = 0; i < (int)(sizeof(pane_default_order) / sizeof(pane_default_order[0])); i++) {
        if (pane_type_allows_placement(type, pane_default_order[i]))
            return pane_default_order[i];
    }

    return PLACE_RIGHT;
}

enum pane_placement pane_next_allowed_placement(enum pane_type type,
    enum pane_placement current, int delta)
{
    const int placement_count =
        (int)(sizeof(pane_placement_order) / sizeof(pane_placement_order[0]));
    int start = -1;
    int step = (delta < 0) ? -1 : 1;

    for (int i = 0; i < placement_count; i++) {
        if (pane_placement_order[i] == current) {
            start = i;
            break;
        }
    }

    if (start < 0)
        start = (step > 0) ? placement_count - 1 : 0;

    for (int i = 0; i < placement_count; i++) {
        int idx = (start + step * (i + 1) + placement_count) % placement_count;
        enum pane_placement candidate = pane_placement_order[idx];
        if (pane_type_allows_placement(type, candidate))
            return candidate;
    }

    return current;
}

const char* pane_placement_name(enum pane_placement where)
{
    switch (where) {
    case PLACE_BOTTOM:
        return "BOTTOM";
    case PLACE_DOUBLE_BOTTOM:
        return "DOUBLE_BOTTOM";
    case PLACE_RIGHT:
        return "RIGHT";
    case PLACE_LEFT:
        return "LEFT";
    case PLACE_DOUBLE_LEFT:
        return "DOUBLE_LEFT";
    case PLACE_DOUBLE_RIGHT:
        return "DOUBLE_RIGHT";
    default:
        return "?";
    }
}

void place_panes(const struct pane_config* config, int count, SDL_Rect* panes,
    const SDL_Rect* window, const int* cell_widths, const int* cell_heights,
    int margin)
{
    SDL_Rect main = *window;
    bool side_first = pane_layout_has_side_touch_pane(config, count);

    if (!side_first) {
        layout_bottom_group(PLACE_DOUBLE_BOTTOM, config, count, panes, &main,
            cell_widths, cell_heights, margin);
        layout_bottom_group(PLACE_BOTTOM, config, count, panes, &main,
            cell_widths, cell_heights, margin);
    }

    layout_side_group(PLACE_DOUBLE_LEFT, config, count, panes, &main,
        cell_widths, cell_heights, margin);
    layout_side_group(PLACE_LEFT, config, count, panes, &main, cell_widths,
        cell_heights, margin);
    layout_side_group(PLACE_DOUBLE_RIGHT, config, count, panes, &main,
        cell_widths, cell_heights, margin);
    layout_side_group(PLACE_RIGHT, config, count, panes, &main, cell_widths,
        cell_heights, margin);

    if (side_first) {
        layout_bottom_group(PLACE_DOUBLE_BOTTOM, config, count, panes, &main,
            cell_widths, cell_heights, margin);
        layout_bottom_group(PLACE_BOTTOM, config, count, panes, &main,
            cell_widths, cell_heights, margin);
    }

    panes[PANE_MAIN] = main;
}
