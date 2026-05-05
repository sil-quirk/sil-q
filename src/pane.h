#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "SDL3/SDL_rect.h"

#define MAX_PANE_CONFIGS 10

// Available pane types in the game.
enum pane_type {
    PANE_MAIN = 0,
    PANE_INVENTORY = 1,
    PANE_WORN = 2, // worn items
    PANE_ROLLS = 3,
    PANE_INFO = 4, // monster info window
    PANE_CHARACTER = 5, // - character sheet
    PANE_LOG = 6,
    PANE_MONSTERS = 7, // - visible monsters window
    PANE_MAP = 8, // interactive side map
    PANE_TOUCH = 9, // touchscreen / mouse action pad
    PANE_MAX = 10,
};

// Where the pane is placed.
enum pane_placement {
    PLACE_BOTTOM = 1u << 0,
    PLACE_RIGHT = 1u << 1,
    PLACE_LEFT = 1u << 2,
    PLACE_DOUBLE_LEFT = 1u << 3,
    PLACE_DOUBLE_RIGHT = 1u << 4,
    PLACE_DOUBLE_BOTTOM = 1u << 5,
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

// Specifications of a pane - where it can be placed and what is its minimum
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
    bool enabled;
    struct rect rect;
    // Monospace font size for this supporting pane. Zero uses the default aux
    // font size, which may itself be auto-derived from the main view scale.
    int font_size;
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

bool pane_placement_is_bottom(enum pane_placement where);
bool pane_placement_is_side(enum pane_placement where);
bool pane_type_allows_placement(enum pane_type type, enum pane_placement where);
int pane_primary_min_cells(enum pane_type type, enum pane_placement where);
int pane_secondary_min_cells(enum pane_type type, enum pane_placement where);
enum pane_placement pane_first_allowed_placement(enum pane_type type);
enum pane_placement pane_next_allowed_placement(enum pane_type type,
    enum pane_placement current, int delta);
const char* pane_placement_name(enum pane_placement where);

void place_panes(const struct pane_config* config, int count, SDL_Rect* panes,
    const SDL_Rect* window, const int* cell_widths, const int* cell_heights,
    int margin);
