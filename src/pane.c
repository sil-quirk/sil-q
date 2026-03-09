#include "pane.h"

static struct pane_specs pane_specs[PANE_MAX] = {
    // For each type of pane, list where it can be placed.
    [PANE_INVENTORY] = {.placement = PLACE_RIGHT, .min_rect.cols = 40},
    [PANE_WORN] = {.placement = PLACE_RIGHT, .min_rect.cols = 30},
    [PANE_ROLLS] = {.placement = PLACE_BOTTOM, .min_rect.rows = 1, .min_rect.cols = 65}, 
    [PANE_INFO] = {.placement = PLACE_RIGHT | PLACE_BOTTOM, .min_rect.rows = 1}, // fill
    [PANE_CHARACTER] = {.placement = PLACE_BOTTOM, .min_rect.cols = 60},
    [PANE_LOG] = {.placement = PLACE_RIGHT | PLACE_BOTTOM}, 
    [PANE_MONSTERS] = {.placement = PLACE_RIGHT}, // fill
    [PANE_TOUCH] = {.placement = PLACE_RIGHT | PLACE_BOTTOM, .min_rect.rows = 12, .min_rect.cols = 12},
};

static struct rect split_by_axis(enum pane_placement place,
    const struct pane_config* config, int count, SDL_Rect* panes,
    const struct rect window_px, struct rect cell, int margin_px)
{
    const int axis = place - 1;
    const int other_axis = 1 - axis;
    int active_count = 0;

    // Minimum required size of the split.
    int split_size = 0;
    int distr_px = window_px.size[other_axis];
    // Number of panes which don't have any sizes requested so the rest of the
    // window will be split equally among them.
    int distr_count = 0;
    int actual_sizes[PANE_MAX] = {0};
    for (int i = 0; i < count; i++) {
        if (!config[i].enabled)
            continue;
        if (config[i].where != place)
            continue;
        active_count++;
        if (config[i].rect.size[axis] > split_size)
            split_size = config[i].rect.size[axis];
        if (pane_specs[config[i].pane].min_rect.size[axis] > split_size)
            split_size = pane_specs[config[i].pane].min_rect.size[axis];

        if (config[i].rect.size[other_axis]) {
            // Fixed size of the pane.
            actual_sizes[i] = cell.size[other_axis] * config[i].rect.size[other_axis] + margin_px;
            distr_px -= actual_sizes[i];
        } else if (config[i].ratio) {
            // Size set by given ratio.
            actual_sizes[i] = window_px.size[other_axis] * config[i].ratio;
            int cells = (actual_sizes[i] - margin_px) / cell.size[other_axis];
            int min_size = pane_specs[config[i].pane].min_rect.size[other_axis];
            if (cells < min_size)
                actual_sizes[i] = cell.size[other_axis] * min_size + margin_px;
            distr_px -= actual_sizes[i];
        } else {
            // Size not set, it will be calculated later from what is left.
            distr_count++;
        }
    }

    if (active_count <= 0)
        return window_px;

    int split_px = split_size * cell.size[axis] + margin_px;
    int rest_px = window_px.size[axis] - split_px;
    int coord = 0;
    for (int i = 0; i < count; i++) {
        if (!config[i].enabled)
            continue;
        if (config[i].where != place)
            continue;
        int pane_px;
        if (actual_sizes[i]) {
            pane_px = actual_sizes[i];
        } else {
            pane_px = distr_px / distr_count;
            int cells = (pane_px - margin_px) / cell.size[other_axis];
            int min_size = pane_specs[config[i].pane].min_rect.size[other_axis];
            if (cells < min_size)
                pane_px = cell.size[other_axis] * min_size + margin_px;
            distr_px -= pane_px;
            distr_count--;
        }
        SDL_Rect* pane = &panes[config[i].pane];
        if (place == PLACE_BOTTOM) {
            *pane = (SDL_Rect){
                .x = coord,
                .y = rest_px,
                .w = pane_px,
                .h = split_px,
            };
        } else {
            *pane = (SDL_Rect){
                .x = rest_px,
                .y = coord,
                .w = split_px,
                .h = pane_px,
            };
        }
        coord += pane_px;
    }

    struct rect rest;
    rest.size[axis] = rest_px;
    rest.size[other_axis] = window_px.size[other_axis];
    return rest;
}

void place_panes(const struct pane_config* config, int count, SDL_Rect* panes,
    const SDL_Rect* window, int cell_width, int cell_height, int margin)
{
    struct rect cell = {.rows = cell_height, .cols = cell_width};
    struct rect top = split_by_axis(PLACE_BOTTOM, config, count, panes,
        (struct rect){.rows = window->h, .cols = window->w}, cell, margin);
    struct rect main = split_by_axis(PLACE_RIGHT, config, count, panes,
        top, cell, margin);
    panes[PANE_MAIN] = (SDL_Rect){.w = main.cols, .h = main.rows};
}

