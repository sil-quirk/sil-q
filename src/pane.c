#include "pane.h"

#define BOTTOM_PLACEMENTS \
    (PLACE_BOTTOM | PLACE_DOUBLE_BOTTOM)
#define CORNER_PLACEMENTS \
    (PLACE_TOP_LEFT | PLACE_TOP_RIGHT | PLACE_BOTTOM_LEFT | PLACE_BOTTOM_RIGHT)
#define CENTER_BORDER_PLACEMENTS \
    (PLACE_TOP_CENTER | PLACE_BOTTOM_CENTER | PLACE_LEFT_CENTER \
        | PLACE_RIGHT_CENTER)
#define OVERLAY_PLACEMENTS \
    (CORNER_PLACEMENTS | CENTER_BORDER_PLACEMENTS)
#define SIDE_PLACEMENTS \
    (PLACE_LEFT | PLACE_RIGHT | PLACE_DOUBLE_LEFT | PLACE_DOUBLE_RIGHT \
        | CORNER_PLACEMENTS)

static const enum pane_placement pane_placement_order[] = {
    PLACE_LEFT,
    PLACE_RIGHT,
    PLACE_TOP_LEFT,
    PLACE_TOP_CENTER,
    PLACE_TOP_RIGHT,
    PLACE_RIGHT_CENTER,
    PLACE_BOTTOM_RIGHT,
    PLACE_BOTTOM_CENTER,
    PLACE_BOTTOM_LEFT,
    PLACE_LEFT_CENTER,
    PLACE_DOUBLE_LEFT,
    PLACE_DOUBLE_RIGHT,
    PLACE_BOTTOM,
    PLACE_DOUBLE_BOTTOM,
};

static const enum pane_placement pane_default_order[] = {
    PLACE_RIGHT,
    PLACE_LEFT,
    PLACE_TOP_RIGHT,
    PLACE_TOP_CENTER,
    PLACE_TOP_LEFT,
    PLACE_BOTTOM_RIGHT,
    PLACE_RIGHT_CENTER,
    PLACE_BOTTOM_CENTER,
    PLACE_BOTTOM_LEFT,
    PLACE_LEFT_CENTER,
    PLACE_DOUBLE_RIGHT,
    PLACE_DOUBLE_LEFT,
    PLACE_BOTTOM,
    PLACE_DOUBLE_BOTTOM,
};

static struct pane_specs pane_specs[PANE_MAX] = {
    [PANE_INVENTORY] = {.placement = SIDE_PLACEMENTS, .min_rect.rows = 1, .min_rect.cols = 40},
    [PANE_WORN] = {.placement = SIDE_PLACEMENTS, .min_rect.rows = 17, .min_rect.cols = 40},
    [PANE_ROLLS] = {.placement = OVERLAY_PLACEMENTS, .min_rect.rows = 1, .min_rect.cols = 65},
    [PANE_INFO] = {.placement = SIDE_PLACEMENTS | BOTTOM_PLACEMENTS, .min_rect.rows = 1, .min_rect.cols = 40},
    [PANE_CHARACTER] = {.placement = SIDE_PLACEMENTS | BOTTOM_PLACEMENTS, .min_rect.cols = 60},
    [PANE_LOG] = {.placement = SIDE_PLACEMENTS | BOTTOM_PLACEMENTS, .min_rect.rows = 1, .min_rect.cols = 1},
    [PANE_MONSTERS] = {.placement = SIDE_PLACEMENTS, .min_rect.rows = 1, .min_rect.cols = 40},
    [PANE_SUPPLY] = {.placement = SIDE_PLACEMENTS, .min_rect.rows = 1, .min_rect.cols = 40},
    [PANE_MAP] = {.placement = SIDE_PLACEMENTS, .min_rect.rows = 10, .min_rect.cols = 24},
    [PANE_TOUCH] = {.placement = PLACE_DOUBLE_LEFT | PLACE_DOUBLE_RIGHT, .min_rect.rows = 12, .min_rect.cols = 12},
    [PANE_LEFT_PANEL] = {.placement = OVERLAY_PLACEMENTS, .min_rect.rows = 1, .min_rect.cols = 1},
    [PANE_STATUS] = {.placement = OVERLAY_PLACEMENTS, .min_rect.rows = 1, .min_rect.cols = 24},
    [PANE_DEPTH] = {.placement = OVERLAY_PLACEMENTS, .min_rect.rows = 4, .min_rect.cols = 12},
    [PANE_DESCRIPTION] = {.placement = OVERLAY_PLACEMENTS, .min_rect.rows = 1, .min_rect.cols = 1},
    [PANE_OVERLAY_MENU] = {.placement = OVERLAY_PLACEMENTS, .min_rect.rows = 1, .min_rect.cols = 1},
    [PANE_COMBAT] = {.placement = OVERLAY_PLACEMENTS, .min_rect.rows = PANE_COMBAT_OVERLAY_ROWS, .min_rect.cols = PANE_COMBAT_OVERLAY_COLS},
    [PANE_STATUS_DEPTH] = {.placement = OVERLAY_PLACEMENTS, .min_rect.rows = 1, .min_rect.cols = 24},
};

static bool pane_placement_is_left(enum pane_placement where)
{
    return (where == PLACE_LEFT || where == PLACE_DOUBLE_LEFT
        || where == PLACE_TOP_LEFT || where == PLACE_BOTTOM_LEFT
        || where == PLACE_LEFT_CENTER);
}

static bool pane_placement_is_right(enum pane_placement where)
{
    return (where == PLACE_RIGHT || where == PLACE_DOUBLE_RIGHT
        || where == PLACE_TOP_RIGHT || where == PLACE_BOTTOM_RIGHT
        || where == PLACE_RIGHT_CENTER);
}

static bool pane_placement_is_bottom_edge(enum pane_placement where)
{
    return (where == PLACE_BOTTOM_LEFT || where == PLACE_BOTTOM_CENTER
        || where == PLACE_BOTTOM_RIGHT);
}

static bool pane_placement_is_horizontal_center(enum pane_placement where)
{
    return (where == PLACE_TOP_CENTER || where == PLACE_BOTTOM_CENTER);
}

static bool pane_placement_is_vertical_center(enum pane_placement where)
{
    return (where == PLACE_LEFT_CENTER || where == PLACE_RIGHT_CENTER);
}

bool pane_placement_is_bottom(enum pane_placement where)
{
    return (where == PLACE_BOTTOM || where == PLACE_DOUBLE_BOTTOM);
}

bool pane_placement_is_corner(enum pane_placement where)
{
    return (where == PLACE_TOP_LEFT || where == PLACE_TOP_RIGHT
        || where == PLACE_BOTTOM_LEFT || where == PLACE_BOTTOM_RIGHT);
}

bool pane_placement_is_center_border(enum pane_placement where)
{
    return (where == PLACE_TOP_CENTER || where == PLACE_BOTTOM_CENTER
        || where == PLACE_LEFT_CENTER || where == PLACE_RIGHT_CENTER);
}

bool pane_placement_is_overlay(enum pane_placement where)
{
    return pane_placement_is_corner(where)
        || pane_placement_is_center_border(where);
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

static int pane_secondary_size_cells(const struct pane_config* config,
    enum pane_placement where)
{
    return pane_placement_is_bottom(where) ? config->rect.cols : config->rect.rows;
}

int pane_primary_min_cells(enum pane_type type, enum pane_placement where)
{
    if (type == PANE_ROLLS && pane_placement_is_overlay(where))
        return PANE_LOG_OVERLAY_COMBAT_COLS;

    return pane_placement_is_bottom(where)
        ? pane_specs[type].min_rect.rows
        : pane_specs[type].min_rect.cols;
}

/*
 * Left margin (in cells) the overlay log keeps transparent so its visible
 * background forms a narrow right-hand band.  The cell grid stays full width
 * (the combat-roll line needs it), but only the rightmost VISIBLE_COLS carry
 * the translucent panel; messages are indented to and wrapped within them.
 */
int pane_log_overlay_left_margin(int term_cols)
{
    int margin = term_cols - PANE_LOG_OVERLAY_VISIBLE_COLS;

    return (margin > 0) ? margin : 0;
}

int pane_log_overlay_vertical_margin_px(int cell_height)
{
    if (cell_height <= 0)
        return 0;

    /* Round 0.4 cell to the nearest pixel. */
    return (cell_height * 2 + 2) / 5;
}

int pane_log_overlay_vertical_padding_px(int cell_height)
{
    /* The log is flush at the top and keeps breathing room only below. */
    return pane_log_overlay_vertical_margin_px(cell_height);
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

static int pane_corner_secondary_pixels(const struct pane_config* config,
    enum pane_placement where, const int* cell_widths, const int* cell_heights,
    int margin_px, const SDL_Rect* area)
{
    int requested;
    int minimum;
    int cells;
    int pixels;
    int cell_px;

    requested = pane_secondary_size_cells(config, where);
    minimum = pane_secondary_min_cells(config->pane, where);
    if (requested <= 0 && config->ratio > 0.0f && area && area->h > 0) {
        pixels = (int)(area->h * config->ratio);
        cell_px = pane_secondary_cell_px(config->pane, where, cell_widths,
            cell_heights);
        cells = (pixels - margin_px) / cell_px;
        if (cells < minimum)
            pixels = cell_px * minimum + margin_px;
        return pixels;
    }

    cells = (requested > minimum) ? requested : minimum;
    cell_px = pane_secondary_cell_px(config->pane, where, cell_widths,
        cell_heights);
    pixels = cells * cell_px + margin_px;
    if (config->pane == PANE_ROLLS && pane_placement_is_overlay(where))
        pixels += pane_log_overlay_vertical_padding_px(cell_px);
    return pixels;
}

static int pane_corner_primary_pixels(const struct pane_config* config,
    enum pane_placement where, const int* cell_widths, const int* cell_heights,
    int margin_px)
{
    int requested;
    int minimum;
    int cells;

    requested = pane_primary_size_cells(config, where);
    minimum = pane_primary_min_cells(config->pane, where);
    cells = (requested > minimum) ? requested : minimum;
    return cells * pane_primary_cell_px(config->pane, where, cell_widths,
        cell_heights) + margin_px;
}

static int pane_overlay_edge_gap(int area_px, int content_px, int margin_px)
{
    int requested_gap;
    int max_gap;

    if (margin_px <= 0 || area_px <= content_px)
        return 0;

    requested_gap = margin_px * 5;
    max_gap = (area_px - content_px) / 2;
    if (max_gap <= 0)
        return 0;

    return (requested_gap < max_gap) ? requested_gap : max_gap;
}

static void layout_overlay_group(enum pane_placement where,
    const struct pane_config* config, int count, SDL_Rect* panes,
    const SDL_Rect* area, const int* cell_widths, const int* cell_heights,
    int margin_px)
{
    int active_count = pane_group_count(config, count, where);
    int pane_widths[MAX_PANE_CONFIGS] = { 0 };
    int pane_heights[MAX_PANE_CONFIGS] = { 0 };
    int split_px = 0;
    int start_y;
    int total_h = 0;
    int edge_gap_x;
    int edge_gap_y;
    bool bottom_edge;

    if (active_count <= 0 || !area || area->w <= 0 || area->h <= 0)
        return;

    for (int i = 0; i < count; i++) {
        int pane_w;
        int pane_px;
        int remaining_px;

        if (!config[i].enabled || config[i].where != where)
            continue;

        pane_w = pane_corner_primary_pixels(&config[i], where, cell_widths,
            cell_heights, margin_px);
        if (pane_w > area->w)
            pane_w = area->w;
        pane_widths[i] = pane_w;
        if (pane_w > split_px)
            split_px = pane_w;

        pane_px = pane_corner_secondary_pixels(&config[i], where, cell_widths,
            cell_heights, margin_px, area);
        remaining_px = area->h - total_h;
        if (remaining_px < 0)
            remaining_px = 0;
        if (pane_px > remaining_px)
            pane_px = remaining_px;
        pane_heights[i] = pane_px;
        total_h += pane_px;
        if (remaining_px <= pane_px)
            break;
    }

    if (total_h <= 0)
        return;

    edge_gap_x = pane_overlay_edge_gap(area->w, split_px, margin_px);
    edge_gap_y = pane_overlay_edge_gap(area->h, total_h, margin_px);

    /* Top Right is a live stack: start it at the safe edge, then let the
     * actual visible Menu/left-panel rectangles move it down as needed.  A
     * generic inset here leaves a phantom row when Menu is disabled and
     * misaligns the stack with the flush Top Left panel. */
    if (where == PLACE_TOP_RIGHT)
        edge_gap_y = 0;

    bottom_edge = pane_placement_is_bottom_edge(where);
    if (bottom_edge)
        start_y = area->y + area->h - edge_gap_y;
    else if (pane_placement_is_vertical_center(where))
        start_y = area->y + (area->h - total_h) / 2;
    else
        start_y = area->y + edge_gap_y;

    for (int i = 0; i < count; i++) {
        SDL_Rect* pane;
        int pane_w = pane_widths[i];
        int pane_px = pane_heights[i];
        int pane_edge_gap_x;
        int pane_x;
        int pane_y;

        if (!config[i].enabled || config[i].where != where)
            continue;
        if (pane_w <= 0)
            continue;
        if (pane_px <= 0)
            continue;

        pane_edge_gap_x = (config[i].pane == PANE_ROLLS) ? 0 : edge_gap_x;

        if (pane_placement_is_left(where))
            pane_x = area->x + pane_edge_gap_x;
        else if (pane_placement_is_right(where))
            pane_x = area->x + area->w - pane_w - pane_edge_gap_x;
        else if (pane_placement_is_horizontal_center(where))
            pane_x = area->x + (area->w - pane_w) / 2;
        else
            pane_x = area->x + pane_edge_gap_x;
        pane_y = bottom_edge ? start_y - pane_px : start_y;

        pane = &panes[config[i].pane];
        *pane = (SDL_Rect){
            .x = pane_x,
            .y = pane_y,
            .w = pane_w,
            .h = pane_px,
        };
        if (bottom_edge)
            start_y -= pane_px;
        else
            start_y += pane_px;
    }
}

bool pane_placement_is_side(enum pane_placement where)
{
    return (pane_placement_is_left(where) || pane_placement_is_right(where)
        || pane_placement_is_horizontal_center(where));
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
    if (type == PANE_LEFT_PANEL
        && pane_type_allows_placement(type, PLACE_TOP_LEFT))
    {
        return PLACE_TOP_LEFT;
    }
    if (type == PANE_STATUS
        && pane_type_allows_placement(type, PLACE_TOP_CENTER))
    {
        return PLACE_TOP_CENTER;
    }
    if (type == PANE_DEPTH
        && pane_type_allows_placement(type, PLACE_TOP_RIGHT))
    {
        return PLACE_TOP_RIGHT;
    }
    if (type == PANE_STATUS_DEPTH
        && pane_type_allows_placement(type, PLACE_BOTTOM_RIGHT))
    {
        return PLACE_BOTTOM_RIGHT;
    }
    if (type == PANE_COMBAT
        && pane_type_allows_placement(type, PLACE_BOTTOM_LEFT))
    {
        return PLACE_BOTTOM_LEFT;
    }
    if (type == PANE_ROLLS
        && pane_type_allows_placement(type, PLACE_TOP_RIGHT))
    {
        return PLACE_TOP_RIGHT;
    }
    if ((type == PANE_DESCRIPTION || type == PANE_OVERLAY_MENU)
        && pane_type_allows_placement(type, PLACE_BOTTOM_CENTER))
    {
        return PLACE_BOTTOM_CENTER;
    }

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
    case PLACE_TOP_LEFT:
        return "TOP_LEFT";
    case PLACE_TOP_RIGHT:
        return "TOP_RIGHT";
    case PLACE_TOP_CENTER:
        return "TOP_CENTER";
    case PLACE_BOTTOM_LEFT:
        return "BOTTOM_LEFT";
    case PLACE_BOTTOM_RIGHT:
        return "BOTTOM_RIGHT";
    case PLACE_BOTTOM_CENTER:
        return "BOTTOM_CENTER";
    case PLACE_LEFT_CENTER:
        return "LEFT_CENTER";
    case PLACE_RIGHT_CENTER:
        return "RIGHT_CENTER";
    case PLACE_DOUBLE_LEFT:
        return "DOUBLE_LEFT";
    case PLACE_DOUBLE_RIGHT:
        return "DOUBLE_RIGHT";
    default:
        return "?";
    }
}

const char* pane_placement_display_name(enum pane_placement where)
{
    switch (where) {
    case PLACE_BOTTOM:
        return "bottom";
    case PLACE_DOUBLE_BOTTOM:
        return "double bottom";
    case PLACE_RIGHT:
        return "right";
    case PLACE_LEFT:
        return "left";
    case PLACE_TOP_LEFT:
        return "top left";
    case PLACE_TOP_RIGHT:
        return "top right";
    case PLACE_TOP_CENTER:
        return "top center";
    case PLACE_BOTTOM_LEFT:
        return "bottom left";
    case PLACE_BOTTOM_RIGHT:
        return "bottom right";
    case PLACE_BOTTOM_CENTER:
        return "bottom center";
    case PLACE_LEFT_CENTER:
        return "left center";
    case PLACE_RIGHT_CENTER:
        return "right center";
    case PLACE_DOUBLE_LEFT:
        return "double left";
    case PLACE_DOUBLE_RIGHT:
        return "double right";
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

    layout_overlay_group(PLACE_TOP_LEFT, config, count, panes, &main,
        cell_widths, cell_heights, margin);
    layout_overlay_group(PLACE_TOP_CENTER, config, count, panes, &main,
        cell_widths, cell_heights, margin);
    layout_overlay_group(PLACE_TOP_RIGHT, config, count, panes, &main,
        cell_widths, cell_heights, margin);
    layout_overlay_group(PLACE_LEFT_CENTER, config, count, panes, &main,
        cell_widths, cell_heights, margin);
    layout_overlay_group(PLACE_RIGHT_CENTER, config, count, panes, &main,
        cell_widths, cell_heights, margin);
    layout_overlay_group(PLACE_BOTTOM_LEFT, config, count, panes, &main,
        cell_widths, cell_heights, margin);
    layout_overlay_group(PLACE_BOTTOM_CENTER, config, count, panes, &main,
        cell_widths, cell_heights, margin);
    layout_overlay_group(PLACE_BOTTOM_RIGHT, config, count, panes, &main,
        cell_widths, cell_heights, margin);

    panes[PANE_MAIN] = main;
}
