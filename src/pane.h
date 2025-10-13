#pragma once
#include <stdint.h>
#include "SDL3/SDL_rect.h"

// Available pane types in the game.
enum pane_type {
    PANE_MAIN = 0,
    PANE_INVENTORY = 1,
    PANE_WORN = 2, // worn items
    PANE_ROLLS = 3,
    PANE_INFO = 4, // monster info window
    PANE_CHARACTER = 5, // — character sheet
    PANE_LOG = 6,
    PANE_MONSTERS = 7, // — visible monsters window
    PANE_MAX = 8,
};

// Where the pane is placed — on the right or in the bottom of the screen.
enum pane_placement {
    PLACE_BOTTOM = 1,
    PLACE_RIGHT = 2,
};

struct rect {
    union {
        struct {
            int rows;
            int cols;
        };
        int size[2];
    };
};

// Specifications of a pane — where it can be placed and what is its minimum
// size.
struct pane_specs {
    // Combination of `pane_placement` denoting allowed placement for this pane.
    uint32_t placement;
    // These can be zero if not specified.
    struct rect min_rect;
};

// Configuration for a pane.
struct pane_config {
    // What pane is configured.
    enum pane_type pane;
    // Where the pane is placed.
    enum pane_placement where;
    struct rect rect;
    // Ratio along the secondary axis, so if the pane is on the right, it's part
    // of the height of the whole window it takes, and if the pane is in the
    // bottom, it's the part of the width of the whole window. The axis will be
    // split in equal parts between panes which have ratio == 0 and don't have
    // their secondary axis size.
    float ratio;
};

// Instance of a pane itself.
struct pane {
    SDL_Rect rect;
    int index;
};

void place_panes(const struct pane_config* config, int count, SDL_Rect* panes,
    const SDL_Rect* window, int cell_width, int cell_height, int margin);
