#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "SDL3/SDL_rect.h"

#define MAX_PANE_CONFIGS 16
#define PANE_LOG_OVERLAY_COMBAT_COLS 48
#define PANE_COMBAT_OVERLAY_COLS 12
#define PANE_COMBAT_OVERLAY_ROWS 4
#define PANE_COMBAT_OVERLAY_MIN_ROWS 3
/* Visible (translucent) width of the overlay log; cells left of this stay
 * transparent so the panel reads as a narrow right-hand band. */
#define PANE_LOG_OVERLAY_VISIBLE_COLS 31

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
    PANE_SUPPLY = 8, // - supply cache window
    PANE_MAP = 9, // interactive side map
    PANE_TOUCH = 10, // touchscreen / mouse action pad
    PANE_LEFT_PANEL = 11, // classic/compact character panel overlay
    PANE_STATUS = 12, // current character statuses, rendered as SDL overlay
    PANE_DEPTH = 13, // dungeon depth/partition and zoom overlay
    PANE_MAIN_MENU = 14, // obsolete; recognized only for config migration
    PANE_DESCRIPTION = 15, // modal item/monster description overlay
    PANE_OVERLAY_MENU = 16, // touch/mouse overlay command menu
    PANE_COMBAT = 17, // detached melee/archery/quiver overlay
    PANE_MAX = 18,
};

// Where the pane is placed.
enum pane_placement {
    PLACE_BOTTOM = 1u << 0,
    PLACE_RIGHT = 1u << 1,
    PLACE_LEFT = 1u << 2,
    PLACE_DOUBLE_LEFT = 1u << 3,
    PLACE_DOUBLE_RIGHT = 1u << 4,
    PLACE_DOUBLE_BOTTOM = 1u << 5,
    PLACE_TOP_LEFT = 1u << 6,
    PLACE_TOP_RIGHT = 1u << 7,
    PLACE_BOTTOM_LEFT = 1u << 8,
    PLACE_BOTTOM_RIGHT = 1u << 9,
    PLACE_TOP_CENTER = 1u << 10,
    PLACE_BOTTOM_CENTER = 1u << 11,
    PLACE_LEFT_CENTER = 1u << 12,
    PLACE_RIGHT_CENTER = 1u << 13,
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
    // font size, which may itself be auto-derived from the configured main
    // view scale.
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
bool pane_placement_is_corner(enum pane_placement where);
bool pane_placement_is_center_border(enum pane_placement where);
bool pane_placement_is_overlay(enum pane_placement where);
bool pane_type_allows_placement(enum pane_type type, enum pane_placement where);
int pane_primary_min_cells(enum pane_type type, enum pane_placement where);
int pane_log_overlay_left_margin(int term_cols);
int pane_secondary_min_cells(enum pane_type type, enum pane_placement where);
enum pane_placement pane_first_allowed_placement(enum pane_type type);
enum pane_placement pane_next_allowed_placement(enum pane_type type,
    enum pane_placement current, int delta);
const char* pane_placement_name(enum pane_placement where);
const char* pane_placement_display_name(enum pane_placement where);

void place_panes(const struct pane_config* config, int count, SDL_Rect* panes,
    const SDL_Rect* window, const int* cell_widths, const int* cell_heights,
    int margin);
