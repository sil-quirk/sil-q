/*
 * Private declarations for the SDL frontend modules.
 */

#ifndef INCLUDED_SDL_MAIN_PRIVATE_H
#define INCLUDED_SDL_MAIN_PRIVATE_H

#include "angband.h"
#include "externs.h"
#include "fs/path.h"
#include "fs/io_sdl.h"
#include "log/log.h"
#include "main.h"
#include "z-term.h"
#include "pane.h"
#include "sdl-config.h"
#include "sdl-sound.h"
#include "sound-config.h"
#include <ctype.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#if defined(SDL_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#if defined(SDL_PLATFORM_ANDROID)
#include <jni.h>
#endif
#if defined(SIL_IOS)
#include "main-sdl-ios.h"
#endif

#if defined(__ANDROID__) || defined(SIL_IOS)
#define SIL_SDL_MOBILE_BUILD 1
#else
#define SIL_SDL_MOBILE_BUILD 0
#endif

#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_LINUX) || defined(SDL_PLATFORM_MACOS)
#define SIL_SDL_DESKTOP_HANDHELD_BUILD 1
#else
#define SIL_SDL_DESKTOP_HANDHELD_BUILD 0
#endif

#if SIL_SDL_MOBILE_BUILD || SIL_SDL_DESKTOP_HANDHELD_BUILD
#define SIL_SDL_HANDHELD_DEFAULTS_BUILD 1
#else
#define SIL_SDL_HANDHELD_DEFAULTS_BUILD 0
#endif

enum {
    TILE_SIZE = 16,
    MAX_TERM_DATA = ANGBAND_TERM_MAX,
    MAX_STORY_FONT_CACHE = 128,
    MAX_MONO_FONT_CACHE = 48,
    SDL_MONO_FONT_PREWARM_QUEUE_MAX = 24,
    SDL_MONO_FONT_PREWARM_IDLE_BATCH = 2,
    SDL_MAIN_VIEW_SCALE_PREWARM_RADIUS = 2,
    SDL_OBJECT_TOOLTIP_FONT_SIZE = 16,
    SDL_LEFT_PANEL_COLLAPSED_ROWS = 3,
    TOUCH_PANE_LONG_PRESS_MS = 350,
    TOUCH_SWIPE_MIN_DISTANCE_PX = 24,
    TOUCH_SWIPE_MAX_DISTANCE_PX = 72,
    TOUCH_ROUND_CIRCLE_SEGMENTS = 32,
    MINIMAP_MAX_ZOOM_STEP = 8,
    MINIMAP_MAX_TOUCH_FINGERS = 4,
    SDL_STARTUP_ISSUE_MAX = 1024,
    SDL_TOUCH_YES_NO_MAX_LINES = 8,
    SDL_TOUCH_YES_NO_LINE_LEN = 160,
    SDL_TOUCH_TUTORIAL_MAX_LINES = 14,
    SDL_TOUCH_TUTORIAL_LINE_LEN = 192,
    SDL_STATUS_PANE_MAX_ENTRIES = 48,
    SDL_STATUS_PANE_COLUMNS = 2,
    SDL_NARRATIVE_BANNER_MAX_LINES = 8,
    SDL_NARRATIVE_BANNER_LINE_LEN = 220,
    SDL_LOG_PANE_DEFAULT_ROWS = 5,
    SDL_OVERLAY_LOG_PANE_DEFAULT_ROWS = 5,
    SDL_OVERLAY_LOG_PANE_ALPHA = 220,
    SDL_LOG_PANE_MIN_ROWS = 1,
    SDL_LOG_PANE_MAX_ROWS = 20,
    SDL_LOG_PANE_MENU_MAX_ENTRIES = 6,
    SDL_DEPTH_PANE_HOVER_NONE = 0,
    SDL_DEPTH_PANE_HOVER_LABEL = 1,
    SDL_DEPTH_PANE_HOVER_ZOOM_OUT = 2,
    SDL_DEPTH_PANE_HOVER_ZOOM_IN = 3,
};

/* Left padding (in pixels) between the overlay log band's edge and its text,
 * scaled to the pane's cell width so the messages do not hug the border. */
#define SDL_OVERLAY_LOG_TEXT_LEFT_PAD(d) ((float)(d)->cell_w * 0.6f)

/* Shared by the game-start narrative banner and transient notifications. */
#define SDL_NARRATIVE_BANNER_FADE_MS 1000
#define SDL_NARRATIVE_BANNER_FADE_FRAME_MS 16

#define TOUCH_MOUSE_FALLBACK_FINGER_ID ((SDL_FingerID)~(SDL_FingerID)0)
#if SIL_SDL_MOBILE_BUILD
#define SIDE_PANE_MENU_SCALE 1.6f
#else
#define SIDE_PANE_MENU_SCALE 1.5f
#endif
#define SDL_WHEEL_IDLE_RESET_NS (250ULL * 1000000ULL)
#define SDL_WHEEL_DISCRETE_STEP_UNITS 1.0f
#define SDL_WHEEL_SMOOTH_STEP_UNITS 3.0f
#define SDL_WHEEL_SMOOTH_MIN_STEP_NS (80ULL * 1000000ULL)
#define SDL_WHEEL_DISCRETE_MAX_STEPS_PER_EVENT 8
#define SDL_WHEEL_SMOOTH_MAX_STEPS_PER_EVENT 1
#define SDL_WHEEL_ACCUM_EPSILON 0.001f

typedef struct sdl_layout_recovery_result {
    bool mode_changed;
    bool scale_changed;
    int old_mode;
    int new_mode;
    int old_scale;
    int new_scale;
} sdl_layout_recovery_result;

typedef enum sdl_startup_device_class {
    SDL_STARTUP_DEVICE_DESKTOP = 0,
    SDL_STARTUP_DEVICE_DESKTOP_CONTROLLER,
    SDL_STARTUP_DEVICE_DESKTOP_HANDHELD,
    SDL_STARTUP_DEVICE_ANDROID_HANDHELD,
    SDL_STARTUP_DEVICE_MOBILE_TOUCH,
} sdl_startup_device_class;


enum { SDL_MAIN_VIEW_ZOOM_MIN_MAP_SQUARES = 10 };

enum { STORY_FONT_SLOTS = 2 };

/*
 * Which story-font slot each UI context renders with.  Slot 0 is the default
 * story font; slot 1 is the secondary font (config.story_font2).  This is the
 * single place that decides "where shows which" font -- flip a context here to
 * move it between fonts.
 */
enum {
    SDL_STORY_FONT_SLOT_DEFAULT   = 0,
    SDL_STORY_FONT_SLOT_STATUS    = 0, /* status overlay: Story Font 1 */
    SDL_STORY_FONT_SLOT_NARRATIVE = 1, /* quest / narrative book body text */
    SDL_STORY_FONT_SLOT_MENU      = 1, /* in-game popup selection menus */
    SDL_STORY_FONT_SLOT_LOG       = 1, /* message log and log pane */
    SDL_STORY_FONT_SLOT_CHAR_DESC = 1, /* character sheet description / history */
    SDL_STORY_FONT_SLOT_CHAR_NUM  = 1, /* character sheet numeric values */
    SDL_STORY_FONT_SLOT_CHAR_SELECT = 1, /* character selection (race/house) book body */
    SDL_STORY_FONT_SLOT_TUTORIAL = 1 /* touch/mouse/zones and birth coach tutorials */
};

typedef struct story_font_entry {
    int pixel_height;
    int slot;
    TTF_Font* font;
    Uint64 last_used;
} story_font_entry;

typedef struct story_font_slot_cache {
    char path[256];
    bool bold;
    bool italic;
    bool underline;
    bool strikethrough;
    int hinting;
    bool kerning;
    int outline;
} story_font_slot_cache;

typedef struct mono_font_atlas_entry {
    bool valid;
    char font_path[256];
    int cell_width;
    int cell_height;
    bool bold;
    bool italic;
    bool underline;
    bool strikethrough;
    int hinting;
    bool kerning;
    int outline;
    Uint64 generation;
    SDL_Texture* atlas;
} mono_font_atlas_entry;

typedef struct mono_font_entry {
    bool valid;
    char font_path[256];
    int cell_width;
    int cell_height;
    bool bold;
    bool italic;
    bool underline;
    bool strikethrough;
    int hinting;
    bool kerning;
    int outline;
    TTF_Font* font;
} mono_font_entry;

typedef struct mono_font_prewarm_request {
    char font_path[256];
    int cell_width;
    int cell_height;
} mono_font_prewarm_request;

typedef struct mono_font_style_key {
    bool bold;
    bool italic;
    bool underline;
    bool strikethrough;
    int hinting;
    bool kerning;
    int outline;
} mono_font_style_key;

typedef struct mono_font_prewarm_job {
    SDL_Thread* thread;
    SDL_Mutex* mutex;
    mono_font_prewarm_request req;
    mono_font_style_key style;
    SDL_Surface* surface;
    int actual_font_size;
    bool done;
    bool success;
    Uint64 build_ns;
    char error[256];
} mono_font_prewarm_job;

typedef struct sdl_state {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* tileset;
    SDL_Color palette[16];
    SDL_Rect safe_area;
    float system_scale;
    int tileset_cols;
    bool need_present;
    bool use_tiles;
    
    // Custom fonts
    story_font_entry story_fonts[MAX_STORY_FONT_CACHE];
    int story_font_count;
    int story_font_depth;      // Nesting counter for story font enable/disable
    bool story_font_grid;      // Whether queued story text should snap to cell grid
    int story_font_slot;       // Which story-font slot enable/disable text uses (0 or 1)
    bool story_font_cache_valid;
    story_font_slot_cache story_font_cache[STORY_FONT_SLOTS];
    mono_font_atlas_entry mono_font_atlases[MAX_MONO_FONT_CACHE];
    mono_font_entry mono_fonts[MAX_MONO_FONT_CACHE];
    
} sdl_state;

typedef struct sdl_view {
    SDL_Rect rect;
    SDL_Texture* canvas;
    SDL_Texture* font_atlas;
    bool font_atlas_cached;
    bool font_atlas_exact;
    int font_atlas_cell_w;
    int font_atlas_cell_h;
    int ttf_font_size;
    int cell_w;
    int cell_h;
    int cols;
    int rows;
    int margin_x;
    int margin_y;
    term t;
    bool term_ready;
} sdl_view;

typedef struct sdl_left_panel_metrics {
    bool collapsed;
    bool compact_row;
    int cell_w;
    int cell_h;
    int content_cols;
    int content_w;
    int separator_w;
    int total_w;
    int panel_rows;
    int panel_render_h;
    int top_padding_h;
    int bottom_padding_h;
    int corner_h;
    int visual_rows;
    int source_h;
    int compact_segment_count;
    int compact_source_rows[3];
    int compact_output_cols[3];
    int compact_output_rows[3];
    int compact_widths[3];
} sdl_left_panel_metrics;

typedef struct sdl_left_panel_compact_light_span {
    int icon_cols;
    int text_start;
    int text_width;
    int packed_width;
} sdl_left_panel_compact_light_span;

typedef struct touch_pane_slot_info {
    const char* slot_name;
    const char* default_label;
    int default_binding;
} touch_pane_slot_info;


enum {
    SDL_TOUCH_PANE_ESC_SLOT = 0,
    SDL_TOUCH_PANE_NORTH_SLOT = 10,
    SDL_TOUCH_PANE_WEST_SLOT = 12,
    SDL_TOUCH_PANE_CENTER_SLOT = 13,
    SDL_TOUCH_PANE_EAST_SLOT = 14,
    SDL_TOUCH_PANE_SOUTH_SLOT = 16,
};

enum {
    MAX_GAMEPADS = 4,
    DPAD_DIAGONAL_WINDOW_MS = 200,
    SHOULDER_COMBO_WINDOW_MS = 150,
    /* Rebinding should listen immediately instead of dropping quick inputs. */
    GAMEPAD_CAPTURE_ARM_DELAY_MS = 0,
};

typedef struct gamepad_entry {
    SDL_JoystickID id;
    SDL_Gamepad* pad;
} gamepad_entry;

typedef struct gamepad_input_state {
    gamepad_entry pads[MAX_GAMEPADS];
    int pad_count;
    bool dpad_up;
    bool dpad_down;
    bool dpad_left;
    bool dpad_right;
    int dpad_dir;
    bool dpad_pending;
    int dpad_pending_dir;
    Uint64 dpad_pending_time;
    bool dpad_pending_shift;
    bool dpad_pending_ctrl;
    bool dpad_pending_alt;
    Sint16 left_x;
    Sint16 left_y;
    int left_dir;
    int left_bind_dir;
    bool left_pending;
    int left_pending_dir;
    Uint64 left_pending_time;
    bool left_pending_shift;
    bool left_pending_ctrl;
    bool left_pending_alt;
    Sint16 right_x;
    Sint16 right_y;
    int right_dir;
    bool left_shoulder_down;
    bool right_shoulder_down;
    bool shoulder_pending;
    int shoulder_pending_button;
    Uint64 shoulder_pending_time;
    bool left_trigger_down;
    bool right_trigger_down;
    bool confirm_pending;
    int confirm_pending_button;
    int confirm_pending_binding;
    Uint64 confirm_pending_time;
    bool confirm_long_triggered;
    int shift_held;
    int ctrl_held;
    int alt_held;
} gamepad_input_state;

typedef struct touch_pane_press_state {
    bool active;
    bool mouse;
    SDL_FingerID finger_id;
    int panel;
    int slot;
    bool long_press_enabled;
    float start_x;
    float start_y;
    Uint64 start_time;
} touch_pane_press_state;

typedef struct touch_swipe_state {
    bool active;
    bool triggered;
    SDL_FingerID finger_id;
    float start_x;
    float start_y;
    float last_x;
    float last_y;
} touch_swipe_state;

typedef struct character_sheet_touch_press_state {
    bool active;
    SDL_FingerID finger_id;
    int choice;
    float start_x;
    float start_y;
    Uint64 start_time;
} character_sheet_touch_press_state;

typedef struct welcome_touch_press_state {
    bool active;
    SDL_FingerID finger_id;
    float start_x;
    float start_y;
    Uint64 start_time;
} welcome_touch_press_state;

typedef enum sdl_welcome_screen_mode {
    SDL_WELCOME_SCREEN_HIDDEN = 0,
    SDL_WELCOME_SCREEN_INTRO,
    SDL_WELCOME_SCREEN_MENU,
    SDL_WELCOME_SCREEN_LOADING
} sdl_welcome_screen_mode;

typedef enum sdl_welcome_line_role {
    SDL_WELCOME_LINE_QUOTE = 0,
    SDL_WELCOME_LINE_ATTRIBUTION,
    SDL_WELCOME_LINE_SONG_ATTRIBUTION,
    SDL_WELCOME_LINE_TITLE,
    SDL_WELCOME_LINE_SUBTITLE,
    SDL_WELCOME_LINE_BODY,
    SDL_WELCOME_LINE_ACTION
} sdl_welcome_line_role;

typedef struct sdl_welcome_intro_line {
    byte attr;
    sdl_welcome_line_role role;
    const char* text;
} sdl_welcome_intro_line;

typedef struct sdl_welcome_screen_state {
    sdl_welcome_screen_mode mode;
    int intro_style;
    bool show_wizard;
    bool new_metarun;
    bool hover_continue;
    bool hover_quit;
    char status[160];
    SDL_FRect continue_rect;
    SDL_FRect quit_rect;
} sdl_welcome_screen_state;

typedef enum sdl_character_sheet_context {
    SDL_CHARACTER_SHEET_HIDDEN = 0,
    SDL_CHARACTER_SHEET_LIVE,
    SDL_CHARACTER_SHEET_BIRTH_PREVIEW,
    SDL_CHARACTER_SHEET_BIRTH_STATS,
    SDL_CHARACTER_SHEET_BIRTH_SKILLS,
    SDL_CHARACTER_SHEET_BIRTH_SELECT, /* race / lineage / character selection */
    SDL_CHARACTER_SHEET_NARRATIVE     /* paginated book (quests, story stats) */
} sdl_character_sheet_context;

/* Narrative book (quest offer/completion, etc.): a text-only book paginated
 * across N parchment pages, reusing the select_page/page-curl machinery. */
#define SDL_BOOK_MAX_PARAS 64   /* most paragraphs/actions a single book may hold */
#define SDL_BOOK_PARA_LEN  1024 /* longest single paragraph (re-flowed text) */
#define SDL_BOOK_MAX_PAGES 24   /* most pages a single book may paginate into */
#define SDL_BOOK_MAX_CONTENTS 8 /* persistent contents links beside the leaf */

typedef struct sdl_character_sheet_live_item {
    int choice;
    int kind;
    int skill;
    int value_kind;
    char label[64];
    char desc[256];
} sdl_character_sheet_live_item;

typedef struct sdl_character_sheet_hit {
    SDL_FRect rect;
    int choice;
    byte attr;
    char desc[256];
} sdl_character_sheet_hit;

/* One row on the race/character selection screen: a selectable choice, or a
 * non-selectable heading/blurb (book-page mode, screen 1). */
typedef struct sdl_character_sheet_select_row {
    int choice;        /* choice id consumed by get_player_choice (>= 0) */
    int reset_choice;  /* >=0: draw a tappable per-row "Reset" button, this id */
    byte attr;
    bool confirmable;  /* false: visible/focusable, but cannot be confirmed */
    bool is_heading;   /* book mode: non-selectable heading/blurb text */
    char label[160];
    char desc[256];    /* hover tooltip */
} sdl_character_sheet_select_row;

/* One informational "label<TAB>value" line in the selection detail panel.
 * text must hold a full prefixed line (e.g. "Description: " + the longest
 * oath/curse description, ~310 chars) so long entries are not truncated -- a
 * short buffer here both clips the text and starves the detail font-fit loop,
 * making it pick an oversized font for the now-too-short content. */
typedef struct sdl_character_sheet_select_detail {
    byte attr;
    char text[512];
    char desc[256];   /* hover tooltip (stat/trait explanation) */
} sdl_character_sheet_select_detail;

typedef struct sdl_character_sheet_select_rating {
    byte attr;
    int count;
    char group[32];
    char stars[8];
    char desc[256];
} sdl_character_sheet_select_rating;

typedef struct sdl_character_sheet_title_candidate {
    char title[128];
    char suffix[64];
} sdl_character_sheet_title_candidate;

typedef struct menu_scroll_drag_state {
    bool active;
    bool dragged;
    bool page_fired;
    SDL_FingerID finger_id;
    int area_index;
    float start_x;
    float start_y;
    float last_y;
    float accum_y;
} menu_scroll_drag_state;

/* "Nothing is hovered" sentinel for hover_choice.  It must stay distinct from
 * every real choice value: entries are >= 0, and the prompt buttons reuse small
 * negatives (-1 Back/Esc, -2 Confirm, -3) while page controls use -20/-21.  A
 * plain -1 sentinel collided with the Back button, so it was always drawn
 * focused when the mouse hovered nothing. */
#define SDL_CHAR_SHEET_NO_HOVER (-1000000)

typedef struct sdl_character_sheet_screen_state {
    sdl_character_sheet_context context;
    int focus_choice;
    int selected_index;
    int points_left;
    int stat_values[A_MAX];
    int stat_costs[A_MAX];
    int skill_old_base[S_MAX];
    int skill_gain[S_MAX];
    int skill_costs[S_MAX];
    sdl_character_sheet_live_item live_items[128];
    int live_item_count;
    sdl_character_sheet_select_row select_rows[96];
    int select_row_count;
    sdl_character_sheet_select_detail select_detail[80];
    int select_detail_count;
    sdl_character_sheet_select_rating select_ratings[8];
    int select_rating_count;
    char select_rating_title[64];
    int select_stat_rows_hint;
    int select_ability_rows_hint;
    int select_ability_rows;
    int select_trait_rows_hint;
    char select_description[4096];
    char select_title[96];
    char select_focus_title[96];
    char select_title_suffix[64];
    byte select_title_suffix_attr;
    sdl_character_sheet_title_candidate select_title_candidates[96];
    int select_title_candidate_count;
    int select_title_power_px;
    int select_title_power_for_title_px;
    int select_title_power_for_width;
    char select_intro[2048];   /* book mode: chronicle text (white) */
    char select_frame_top[768];    /* book mode: framing line above (accent) */
    char select_frame_bottom[768]; /* book mode: framing/charge below (accent) */
    char select_desc_sizing[4096]; /* description sample used for font fitting */
    /* Welcomes for the characters in the selected race, measured at the real
     * render font so every sheet in that race uses one shared band height. */
    char select_welcome[96][160]; /* sized to match select_rows[] */
    int select_welcome_count;
    /* Lore bodies for every selectable entry.  The longest byte string is not
     * necessarily the one that wraps to the most lines, so font fitting must
     * measure every candidate at the live width. */
    char select_desc_candidates[96][1280];
    int select_desc_candidate_count;
    Uint64 select_desc_candidate_hash;
    bool select_book_mode;     /* screen 1: story/explanation page, no detail */
    bool select_menu_style;    /* menu mode: pixel rows, storyfont2, no grid */
    int select_page;           /* book mode: current page (last = choice) */
    int select_page_count;     /* book mode: number of pages (1 otherwise) */
    int select_book_body_px;   /* cached body size for race story pages */
    int select_book_choice_body_px; /* cached body size for final choice page */
    int select_book_body_for_h;
    int select_book_body_for_w;
    int select_book_body_for_region_h;
    int select_book_body_for_title_px;
    Uint64 select_book_body_for_layout_hash;
    bool page_turn_active;     /* book mode: a page-curl animation is playing */
    int page_turn_dir;         /* +1 = forward (0->1), -1 = back (1->0) */
    int page_turn_from_page;   /* outgoing page; may be non-adjacent to dest */
    Uint64 page_turn_start_ns; /* animation start timestamp */
    SDL_Texture* page_turn_from_tex; /* outgoing page snapshot (the curling leaf) */
    SDL_Texture* page_turn_to_tex;   /* incoming page snapshot (revealed beneath) */
    int page_turn_tex_w;       /* snapshot pixel size */
    int page_turn_tex_h;
    /* Narrative book (context == NARRATIVE): paragraphs and actions flowed across N pages.
     * Pagination is recomputed lazily in the render path (it needs the live
     * canvas height) and mirrored into select_page_count. */
    char narrative_title[96];
    char narrative_paras[SDL_BOOK_MAX_PARAS][SDL_BOOK_PARA_LEN];
    int narrative_para_choice[SDL_BOOK_MAX_PARAS]; /* >= 0: clickable action */
    byte narrative_para_attr[SDL_BOOK_MAX_PARAS];
    bool narrative_para_break[SDL_BOOK_MAX_PARAS]; /* force a new page before this para */
    bool narrative_pending_break; /* next added paragraph starts a fresh page */
    bool narrative_para_highlight[SDL_BOOK_MAX_PARAS]; /* draw this para in the accent colour (task/reward) */
    bool narrative_pending_highlight; /* next added paragraph is highlighted */
    int narrative_para_count;
    int narrative_page_start[SDL_BOOK_MAX_PAGES + 1]; /* [page]..[page+1) paras */
    int narrative_para_lines[SDL_BOOK_MAX_PARAS]; /* cached final-layout wrap count */
    int narrative_page_count;
    int narrative_target_page_count; /* 0 = content-driven pagination */
    int narrative_body_px;         /* one body size shared by every page of the book */
    int narrative_paginated_for_h; /* canvas.h the page breaks were built for */
    int narrative_paginated_for_w; /* content width the page breaks were built for */
    int narrative_paginated_for_top;
    int narrative_paginated_for_bottom;
    int narrative_paginated_for_title_px;
    Uint64 narrative_paginated_font_generation;
    Uint64 narrative_layout_generation;
    Uint64 narrative_paginated_layout_generation;
    char narrative_contents_label[SDL_BOOK_MAX_CONTENTS][48];
    int narrative_contents_choice[SDL_BOOK_MAX_CONTENTS];
    int narrative_contents_page[SDL_BOOK_MAX_CONTENTS];
    int narrative_contents_count;
    int narrative_contents_body_px; /* cached contents-column font size */
    bool narrative_lamp_enabled;
    u32b narrative_lamp_current;
    u32b narrative_lamp_maximum;
    int narrative_lamp_page;
    bool narrative_lamp_side;     /* place lamp beside text when width permits */
    bool narrative_close_enabled;  /* show an on-screen exit button (mouse/touch) */
    char narrative_close_label[48]; /* context-specific exit button label */
    touch_swipe_state birth_swipe;
    character_sheet_touch_press_state touch_press;
    int last_body_px;          /* last column/list body font px for tooltips */
    float last_body_line_h;    /* rendered row height for last column body */
    int last_desc_px;          /* last description font px */
    float last_desc_line_h;    /* rendered row height for last description */
    sdl_character_sheet_hit hits[224];
    int hit_count;
    int hover_choice;
    int sheet_scroll;
    int sheet_scroll_max;
    SDL_FRect select_scroll_rect;
    menu_scroll_drag_state select_scroll_drag;
} sdl_character_sheet_screen_state;

typedef struct touch_zone_press_state {
    bool active;
    SDL_FingerID finger_id;
    int zone;
    float start_x;
    float start_y;
    Uint64 start_time;
} touch_zone_press_state;

typedef struct touch_top_panel_press_state {
    bool active;
    SDL_FingerID finger_id;
    int slot;
    float start_x;
    float start_y;
    Uint64 start_time;
} touch_top_panel_press_state;

typedef struct touch_round_press_state {
    bool active;
    SDL_FingerID finger_id;
    float center_x;
    float center_y;
    float current_x;
    float current_y;
    float radius;
    float inner_radius;
    int selected_dir;
    bool button_press;
    int button_dir;
    Uint64 start_time;
} touch_round_press_state;

typedef struct menu_touch_press_state {
    bool active;
    bool mouse;
    SDL_FingerID finger_id;
    int col;
    int row;
    float start_x;
    float start_y;
    Uint64 start_time;
} menu_touch_press_state;

typedef struct character_panel_press_state {
    bool active;
    bool mouse;
    SDL_FingerID finger_id;
    bool secondary_enabled;
    float start_x;
    float start_y;
    Uint64 start_time;
} character_panel_press_state;

typedef struct screen_back_touch_press_state {
    bool active;
    SDL_FingerID finger_id;
    float start_x;
    float start_y;
    Uint64 start_time;
} screen_back_touch_press_state;

typedef struct sdl_wheel_step_state {
    float accum_x;
    float accum_y;
    Uint64 last_step_x_timestamp;
    Uint64 last_step_y_timestamp;
    Uint64 last_timestamp;
    SDL_MouseID last_mouse;
    bool smooth_x;
    bool smooth_y;
} sdl_wheel_step_state;

typedef struct pane_layout_drag_state {
    bool active;
    bool dragged;
    bool changed;
    int pane_config_index;
    enum pane_type pane;
    enum pane_placement where;
    float start_x;
    float start_y;
} pane_layout_drag_state;

typedef struct side_pane_menu_entry {
    int pane_config_index;
    enum pane_type pane;
    bool enabled;
    SDL_FRect rect;
} side_pane_menu_entry;

typedef struct side_pane_menu_state {
    bool active;
    int hover_index;
    bool press_active;
    bool press_mouse;
    SDL_FingerID press_finger_id;
    int press_index;
    float anchor_x;
    float anchor_y;
    bool long_press_active;
    bool long_press_opened;
    SDL_FingerID long_press_finger_id;
    enum pane_type long_press_pane;
    float long_press_start_x;
    float long_press_start_y;
    Uint64 long_press_start_time;
} side_pane_menu_state;

typedef struct status_pane_entry {
    char label[32];
    char detail[64];
    byte attr;
} status_pane_entry;

typedef struct status_pane_layout_item {
    bool span;
    int row;
    int x;
    int w;
    char line[96];
    byte attr;
} status_pane_layout_item;

typedef struct status_pane_layout {
    SDL_FRect panel;
    status_pane_layout_item items[SDL_STATUS_PANE_MAX_ENTRIES];
    int layout_count;
    int content_w;
    int font_px;
    int row_h;
    int pad_x;
    int pad_y;
    int item_pad_x;
    bool right_align;
} status_pane_layout;

typedef struct status_depth_pane_layout_item {
    int row_from_bottom;
    int right;
    int w;
    char line[96];
    byte attr;
} status_depth_pane_layout_item;

typedef struct status_depth_pane_layout {
    SDL_FRect panel;
    status_depth_pane_layout_item items[SDL_STATUS_PANE_MAX_ENTRIES + 1];
    int layout_count;
    int row_count;
    int content_w;
    int font_px;
    int row_h;
    int pad_x;
    int pad_y;
    int gap_x;
} status_depth_pane_layout;

typedef enum log_pane_menu_action {
    LOG_PANE_MENU_FILTER,
    LOG_PANE_MENU_ROWS,
    LOG_PANE_MENU_SWITCH
} log_pane_menu_action;

typedef struct log_pane_menu_entry {
    log_pane_menu_action action;
    int filter;
    int row_delta;
    char label[32];
    char hint[32];
    SDL_FRect rect;
} log_pane_menu_entry;

typedef struct log_pane_menu_state {
    bool active;
    int hover_index;
    bool press_active;
    bool press_mouse;
    SDL_FingerID press_finger_id;
    int press_index;
    enum pane_type target_pane;
    float anchor_x;
    float anchor_y;
    bool long_press_active;
    bool long_press_opened;
    SDL_FingerID long_press_finger_id;
    enum pane_type long_press_pane;
    float long_press_start_x;
    float long_press_start_y;
    Uint64 long_press_start_time;
} log_pane_menu_state;

typedef struct map_touch_press_state {
    bool active;
    bool repeat_target;
    SDL_FingerID finger_id;
    int map_y;
    int map_x;
    float start_x;
    float start_y;
    Uint64 start_time;
} map_touch_press_state;

typedef struct object_tooltip_state {
    bool active;
    bool touch;
    bool term_cell;
    bool character_panel_cell;
    bool screen_rect;
    int map_y;
    int map_x;
    int cell_col;
    int cell_row;
    int cell_cols;
    SDL_FRect rect;
    Uint64 expires_at;
    bool persistent;
    char text[640];
} object_tooltip_state;

enum
{
    SDL_DESCRIPTION_OVERLAY_FOOTER_LEN = 160,
    SDL_DESCRIPTION_OVERLAY_ACTION_TOKEN_LEN = 32,
    SDL_DESCRIPTION_OVERLAY_MAX_ACTIONS = 8
};

typedef struct description_overlay_action {
    int key;
    char token[SDL_DESCRIPTION_OVERLAY_ACTION_TOKEN_LEN];
} description_overlay_action;

typedef struct description_overlay_state {
    bool active;
    bool interactive;
    bool avoid_active;
    const byte* attrs;
    const char* chars;
    const byte* tattrs;
    const char* tchars;
    const byte* story;
    const byte* health;
    int width;
    int height;
    int target_cols;
    int scroll;
    int avoid_term_col;
    int avoid_term_row;
    int avoid_term_wid;
    int avoid_term_hgt;
    bool footer_always;
    int footer_hover_key;
    bool close_hover;
    int footer_action_count;
    char footer_text[SDL_DESCRIPTION_OVERLAY_FOOTER_LEN];
    description_overlay_action footer_actions[SDL_DESCRIPTION_OVERLAY_MAX_ACTIONS];
} description_overlay_state;

typedef struct description_overlay_layout {
    SDL_FRect panel;
    SDL_FRect close_rect;
    float text_x;
    float text_y;
    float footer_y;
    int cell_w;
    int cell_h;
    int visible_cols;
    int visible_rows;
    int max_scroll;
    int scroll;
    bool footer;
    bool close_button;
} description_overlay_layout;

enum {
    SDL_UNIFIED_LOOK_PROMPT_MAX_BUTTONS = 16,
    SDL_UNIFIED_LOOK_PROMPT_LABEL_VARIANTS = 4,
    SDL_UNIFIED_LOOK_PROMPT_LABEL_LEN = 32,
    SDL_UNIFIED_LOOK_SIDEBAR_MAX_ITEMS = 256,
    SDL_UNIFIED_LOOK_SIDEBAR_SYMBOL_LEN = 8,
    SDL_UNIFIED_LOOK_SIDEBAR_TEXT_LEN = 128
};

typedef struct sdl_unified_look_prompt_button_state {
    int choice;
    char labels[SDL_UNIFIED_LOOK_PROMPT_LABEL_VARIANTS]
        [SDL_UNIFIED_LOOK_PROMPT_LABEL_LEN];
} sdl_unified_look_prompt_button_state;

typedef struct sdl_unified_look_prompt_state {
    bool active;
    int anchor_row;
    int count;
    sdl_unified_look_prompt_button_state
        buttons[SDL_UNIFIED_LOOK_PROMPT_MAX_BUTTONS];
} sdl_unified_look_prompt_state;

typedef enum sdl_unified_look_sidebar_item_kind {
    SDL_UNIFIED_LOOK_SIDEBAR_ITEM_HEADER = 0,
    SDL_UNIFIED_LOOK_SIDEBAR_ITEM_ENTRY = 1
} sdl_unified_look_sidebar_item_kind;

typedef struct sdl_unified_look_sidebar_item_state {
    sdl_unified_look_sidebar_item_kind kind;
    int choice;
    int entity_type;
    int y;
    int x;
    byte symbol_attr;
    byte text_attr;
    s16b health_m_idx;
    byte health_offset;
    byte health_len;
    char symbol[SDL_UNIFIED_LOOK_SIDEBAR_SYMBOL_LEN];
    char text[SDL_UNIFIED_LOOK_SIDEBAR_TEXT_LEN];
} sdl_unified_look_sidebar_item_state;

typedef struct sdl_unified_look_sidebar_state {
    bool active;
    bool compact;
    bool has_selection;
    int selected_choice;
    int count;
    sdl_unified_look_sidebar_item_state
        items[SDL_UNIFIED_LOOK_SIDEBAR_MAX_ITEMS];
} sdl_unified_look_sidebar_state;

enum {
    SDL_SONG_MENU_MAX_ENTRIES = 24,
    SDL_SONG_MENU_LETTER_LEN = 4,
    SDL_SONG_MENU_TEXT_LEN = 64,
    SDL_SONG_MENU_TITLE_LEN = 32
};

typedef struct sdl_song_menu_entry_state {
    int choice;
    byte text_attr;
    char letter[SDL_SONG_MENU_LETTER_LEN];
    char text[SDL_SONG_MENU_TEXT_LEN];
} sdl_song_menu_entry_state;

typedef struct sdl_song_menu_state {
    bool active;
    int count;
    int highlight; /* choice highlighted by keyboard navigation, -1 none */
    char title[SDL_SONG_MENU_TITLE_LEN];
    sdl_song_menu_entry_state entries[SDL_SONG_MENU_MAX_ENTRIES];
} sdl_song_menu_state;

enum {
    SDL_QUESTION_MENU_MAX_ENTRIES = 320,
    SDL_QUESTION_MENU_MAX_BUTTONS = 4,
    SDL_QUESTION_MENU_LETTER_LEN = 4,
    SDL_QUESTION_MENU_TEXT_LEN = 96,
    SDL_QUESTION_MENU_TITLE_LEN = 80,
    SDL_QUESTION_MENU_DESC_LEN = 480
};

typedef struct sdl_question_menu_entry_state {
    int choice;
    byte text_attr;
    char letter[SDL_QUESTION_MENU_LETTER_LEN];
    char text[SDL_QUESTION_MENU_TEXT_LEN];
} sdl_question_menu_entry_state;

typedef struct sdl_question_menu_button_state {
    int choice;
    byte text_attr;
    char text[SDL_QUESTION_MENU_TEXT_LEN];
} sdl_question_menu_button_state;

/* Generic question overlay: a titled panel with an optional wrapped
 * description block and selectable answer rows.  Local questions anchor
 * next to a map grid (the door/trap/wall being asked about); global
 * questions centre on the map view. */
typedef struct sdl_question_menu_state {
    bool active;
    bool blocking_input;
    bool nonblocking;
    bool close_hover;
    bool scroll_follow_highlight;
    bool has_anchor;
    int anchor_y; /* map grid the question is about (local placement) */
    int anchor_x;
    Uint64 expires_at_ns;
    int count;
    int button_count;
    int highlight; /* choice highlighted by keyboard navigation, -1 none */
    int* scroll_offset_ptr;
    char title[SDL_QUESTION_MENU_TITLE_LEN];
    char desc[SDL_QUESTION_MENU_DESC_LEN];
    sdl_question_menu_entry_state entries[SDL_QUESTION_MENU_MAX_ENTRIES];
    sdl_question_menu_button_state buttons[SDL_QUESTION_MENU_MAX_BUTTONS];
} sdl_question_menu_state;

typedef struct unified_look_map_drag_state {
    bool active;
    bool mouse;
    bool dragged;
    bool pinch_active;
    SDL_FingerID finger_id;
    SDL_FingerID pinch_finger_id;
    float start_x;
    float start_y;
    float last_x;
    float last_y;
    float accum_x;
    float accum_y;
    float pinch_x;
    float pinch_y;
    float pinch_start_distance;
    int pinch_start_scale;
} unified_look_map_drag_state;

typedef struct main_map_drag_state {
    bool active;
    bool mouse;
    bool dragged;
    bool pinch_active;
    SDL_FingerID finger_id;
    SDL_FingerID pinch_finger_id;
    float start_x;
    float start_y;
    float last_x;
    float last_y;
    float accum_x;
    float accum_y;
    float pinch_x;
    float pinch_y;
    float pinch_start_distance;
    int pinch_start_scale;
} main_map_drag_state;

enum {
    SDL_PLAYER_ACTION_NONE = 0,
    SDL_PLAYER_ACTION_WAIT,
    SDL_PLAYER_ACTION_USE,
    SDL_PLAYER_ACTION_STEALTH,
    SDL_PLAYER_ACTION_SING,
    SDL_PLAYER_ACTION_EXCHANGE,
    SDL_PLAYER_ACTION_FLETCH,
    SDL_PLAYER_ACTION_EXAMINE,
    SDL_PLAYER_ACTION_ACTIVATE,
    SDL_PLAYER_ACTION_HORN,
    SDL_PLAYER_ACTION_SHOOT,
    SDL_PLAYER_ACTION_QUICK_THROW,
    SDL_PLAYER_ACTION_REST,
    SDL_PLAYER_ACTION_SWAP_QUIVERS,
    SDL_PLAYER_ACTION_CHANGE_STAFF,
    SDL_PLAYER_ACTION_CLOSE_DOOR,
    SDL_PLAYER_ACTION_BASH_DOOR,
    SDL_PLAYER_ACTION_MAX = 17,
    SDL_PLAYER_EXCHANGE_MAX_TARGETS = 8,
};

/* Dedicated UI symbols in the final row of the 16x16 atlas.  Keep these
 * columns centralized: both the player action wheel and Quick Access use
 * them, so their icons must not drift apart again. */
enum {
    SDL_UI_SYMBOL_ROW = 32,
    SDL_UI_SYMBOL_DESCRIPTION = 0,
    SDL_UI_SYMBOL_PICK = 1,
    SDL_UI_SYMBOL_WAIT = 2,
    SDL_UI_SYMBOL_REST = 3,
    SDL_UI_SYMBOL_STEALTH = 4,
    SDL_UI_SYMBOL_DISARM = 5,
    SDL_UI_SYMBOL_OPEN_DOOR = 6,
    SDL_UI_SYMBOL_CLOSE_DOOR = 7,
    SDL_UI_SYMBOL_BASH_DOOR = 8,
    SDL_UI_SYMBOL_FLETCH = 9,
    SDL_UI_SYMBOL_EXCHANGE = 10,
    SDL_UI_SYMBOL_ACTIVATE_STAFF = 11,
    SDL_UI_SYMBOL_BLOW_HORN = 12,
    SDL_UI_SYMBOL_RANGED_ATTACK = 13,
    SDL_UI_SYMBOL_CHANGE_QUIVERS = 14,
    SDL_UI_SYMBOL_INVENTORY = 15,
    SDL_UI_SYMBOL_EQUIPPED = 16,
    SDL_UI_SYMBOL_SUPPLY = 17,
    SDL_UI_SYMBOL_ABILITIES = 18,
    SDL_UI_SYMBOL_CHARACTER = 19,
    SDL_UI_SYMBOL_SMITHING = 20,
    SDL_UI_SYMBOL_VIEW = 21,
    SDL_UI_SYMBOL_LORE = 22,
    SDL_UI_SYMBOL_USE = 23,
    SDL_UI_SYMBOL_SING = 24,
    SDL_UI_SYMBOL_MAP = 25,
};

typedef struct player_action_menu_entry {
    int kind;
    int command;
    cptr label;
    cptr description;
    byte tile_attr;
    char tile_char;
    cptr fallback;
    SDL_FRect rect;
    float center_x;
    float center_y;
    float inner_radius;
    float outer_radius;
    float start_angle;
    float end_angle;
} player_action_menu_entry;

typedef struct player_action_menu_state {
    bool active;
    int hover_kind;
    bool press_active;
    bool press_mouse;
    bool press_gamepad;
    bool press_secondary;
    SDL_FingerID press_finger_id;
    int press_button;
    int press_kind;
    float press_start_x;
    float press_start_y;
    Uint64 press_start_time;
} player_action_menu_state;

typedef struct player_exchange_target_state {
    bool active;
    bool hover_active;
    int hover_y;
    int hover_x;
    int hover_m_idx;
    bool selected;
    int selected_y;
    int selected_x;
    int selected_m_idx;
    bool press_active;
    bool press_mouse;
    SDL_FingerID press_finger_id;
    int press_y;
    int press_x;
    float press_start_x;
    float press_start_y;
} player_exchange_target_state;

typedef struct player_exchange_target_entry {
    int y;
    int x;
    int m_idx;
} player_exchange_target_entry;

typedef struct minimap_touch_finger {
    bool active;
    SDL_FingerID finger_id;
    bool moved;
    float start_x;
    float start_y;
    float x;
    float y;
} minimap_touch_finger;

typedef struct minimap_state {
    bool active;
    int zoom_step;
    bool default_zoom_pending;
    float pan_x;
    float pan_y;
    bool map_layout_valid;
    SDL_FRect map_rect;
    int map_min_y;
    int map_min_x;
    int map_max_y;
    int map_max_x;
    bool focus_active;
    int focus_y;
    int focus_x;
    int left_stick_dir;
    int right_stick_dir;
    SDL_FRect zoom_out_rect;
    SDL_FRect zoom_in_rect;
    SDL_FRect close_rect;
    bool zoom_out_enabled;
    bool zoom_in_enabled;
    bool pending_hint_open;
    int pending_hint_index;
    bool drag_active;
    bool drag_mouse;
    SDL_FingerID drag_finger_id;
    float drag_last_x;
    float drag_last_y;
    minimap_touch_finger fingers[MINIMAP_MAX_TOUCH_FINGERS];
    bool pinch_active;
    int pinch_finger_a;
    int pinch_finger_b;
    float pinch_start_distance;
    int pinch_start_zoom_step;
} minimap_state;

typedef struct side_map_pane_state {
    int zoom_step;
    bool default_zoom_pending;
    float pan_x;
    float pan_y;
    bool press_active;
    bool press_mouse;
    SDL_FingerID press_finger_id;
    bool press_dragged;
    float press_start_x;
    float press_start_y;
    bool drag_active;
    bool drag_mouse;
    SDL_FingerID drag_finger_id;
    float drag_last_x;
    float drag_last_y;
    minimap_touch_finger fingers[MINIMAP_MAX_TOUCH_FINGERS];
    bool pinch_active;
    int pinch_finger_a;
    int pinch_finger_b;
    float pinch_start_distance;
    int pinch_start_zoom_step;
    s16b last_depth;
    byte last_map_hgt;
    byte last_map_wid;
} side_map_pane_state;

typedef struct pointer_attack_state {
    int mode;
    int panel_hover_mode;
    bool hover_visible;
    bool hover_valid;
    bool hover_actionable;
    bool hover_manual;
    int hover_kind;
    int hover_y;
    int hover_x;
    int hover_m_idx;
    bool touch_press_active;
    bool touch_repeat_target;
    SDL_FingerID touch_finger_id;
    int touch_press_y;
    int touch_press_x;
    float touch_start_x;
    float touch_start_y;
    Uint64 touch_start_time;
    bool touch_selected;
    bool touch_selected_manual;
    int touch_selected_mode;
    int touch_selected_kind;
    int touch_selected_y;
    int touch_selected_x;
    int touch_selected_m_idx;
    bool pending;
    int pending_kind;
    int pending_mode;
    int pending_m_idx;
    int pending_y;
    int pending_x;
    bool pending_wake;
} pointer_attack_state;

typedef struct pointer_aim_state {
    bool active;
    int range;
    bool allow_vertical;
    bool hover_visible;
    int hover_y;
    int hover_x;
    bool touch_press_active;
    bool touch_repeat_target;
    SDL_FingerID touch_finger_id;
    int touch_press_y;
    int touch_press_x;
    float touch_start_x;
    float touch_start_y;
    bool touch_selected;
    int touch_selected_y;
    int touch_selected_x;
    bool pending;
    int pending_dir;
    bool pending_wake;
    /* aim-select mode: the interactive fire-targeting UI drives selection
     * from game code; pointer input is reported as events instead of
     * resolving to a direction here. */
    bool select_mode;
    bool select_manual;
    bool select_location;
    bool select_visible;
    int select_y;
    int select_x;
    bool select_mouse_press;
    bool select_mouse_dragged;
    float select_mouse_start_x;
    float select_mouse_start_y;
    float select_mouse_last_x;
    float select_mouse_last_y;
    float select_mouse_accum_x;
    float select_mouse_accum_y;
    bool event_pending;
    int event_kind;
    int event_y;
    int event_x;
    bool event_wake;
    /* aim-select "adjacent choices" flavour: pick one of a small set of
     * highlighted grids (e.g. which door to close/bash). When set, hover/click
     * only act on the listed cells and a small prompt popup is drawn. */
    bool select_adjacent;
    int select_choice_count;
    int select_choice_y[9];
    int select_choice_x[9];
    char select_prompt[64];
} pointer_aim_state;

enum {
    SDL_POINTER_ATTACK_TARGET_NONE = 0,
    SDL_POINTER_ATTACK_TARGET_MONSTER,
    SDL_POINTER_ATTACK_TARGET_LOCATION,
    SDL_POINTER_ATTACK_TARGET_ALTER,
};

enum {
    SDL_MOUSE_PATH_MAX_GRIDS = MAX_DUNGEON_HGT * MAX_DUNGEON_WID,
    SDL_MOUSE_PATH_SPRINT_CHAIN_MAX = 4,
    SDL_MOUSE_PATH_ROUTE_DIRS = 8,
    SDL_MOUSE_PATH_ROUTE_STATE_COUNT =
        1 + SDL_MOUSE_PATH_ROUTE_DIRS * SDL_MOUSE_PATH_SPRINT_CHAIN_MAX,
    SDL_MOUSE_PATH_COST_NORMAL = 1000,
    SDL_MOUSE_PATH_COST_SPRINT = 667,
    SDL_MOUSE_PATH_TURN_COST_SOFT = 25,
    SDL_MOUSE_PATH_TURN_COST_HARD = 90,
};

enum {
    SDL_MOUSE_PATH_BLOCKED_NONE = 0,
    SDL_MOUSE_PATH_BLOCKED_STUCK_DOOR,
    SDL_MOUSE_PATH_BLOCKED_DANGER,
};

enum {
    SDL_OBJECT_TOOLTIP_TOUCH_MS = 2200,
};

typedef struct mouse_path_state {
    bool hover_visible;
    bool follow_active;
    bool path_valid;
    int blocked_target_kind;
    int source_y;
    int source_x;
    int target_y;
    int target_x;
    int target_m_idx;
    int path_len;
    u16b path[SDL_MOUSE_PATH_MAX_GRIDS];
    bool path_wake_pending;
    bool recall_pending;
    int recall_y;
    int recall_x;
    bool stuck_door_bash_pending;
    int stuck_door_bash_y;
    int stuck_door_bash_x;
    int stuck_door_bash_dir;
    bool grid_question_pending;
    int grid_question_y;
    int grid_question_x;
    bool last_step_door_pending;
    int last_step_door_y;
    int last_step_door_x;
    int last_step_player_y;
    int last_step_player_x;
} mouse_path_state;

typedef struct mouse_path_search_state {
    size_t capacity;
    int width;
    int heap_size;
    int* cost;
    int* heap_pos;
    u16b* visit_generation;
    u16b current_generation;
    u16b* parent_grid;
    byte* parent_state;
    int* heap_priority;
    u16b* heap_grid;
    byte* heap_state;
} mouse_path_search_state;

enum {
    TOUCH_ZONE_LEFT_NW = 0,
    TOUCH_ZONE_LEFT_N,
    TOUCH_ZONE_LEFT_W,
    TOUCH_ZONE_LEFT_Z,
    TOUCH_ZONE_LEFT_SW,
    TOUCH_ZONE_LEFT_S,
    TOUCH_ZONE_RIGHT_N,
    TOUCH_ZONE_RIGHT_NE,
    TOUCH_ZONE_RIGHT_SPACE,
    TOUCH_ZONE_RIGHT_E,
    TOUCH_ZONE_RIGHT_S,
    TOUCH_ZONE_RIGHT_SE,
    TOUCH_ZONE_COUNT,
};

extern sdl_state g_state;
extern sdl_view g_views[MAX_TERM_DATA];

typedef enum {
    SDL_TOUCH_YES_NO_PLACEMENT_CENTER = 0,
    SDL_TOUCH_YES_NO_PLACEMENT_LOWER
} sdl_touch_yes_no_prompt_placement;
typedef enum {
    SDL_TOUCH_YES_NO_HOVER_NONE = -1,
    SDL_TOUCH_YES_NO_HOVER_YES = 0,
    SDL_TOUCH_YES_NO_HOVER_NO = 1
} sdl_touch_yes_no_prompt_hover;

extern const char help_sdl[];
extern const char* const sdl_story_fallback_font;
extern const char* const sdl_story_fallback_font2;
extern struct sdl_config config;
extern bool g_hide_left_panel;
extern bool g_sdl_left_panel_pane_source_active;
extern bool g_left_panel_pane_expanded;
extern int g_saved_screen_left_panel_pane_depth;
extern bool g_last_main_cell_hit_left_panel;
extern struct sound_config g_sound_config;
extern char config_file_path[1024];
extern const struct pane_config default_pane_config[];
extern const int default_pane_config_count;
extern struct pane_config pane_config[MAX_PANE_CONFIGS];
extern int pane_config_count;
extern struct sdl_pane_profile g_pane_profiles[SDL_PANE_PROFILE_COUNT];
extern int g_platform_max_main_view_scale[SDL_MIN_TERMINAL_MODE_COUNT];
extern sdl_startup_device_class g_startup_device_class;
extern bool g_touch_tutorial_requested_from_settings;
extern bool g_mouse_tutorial_requested_from_settings;
extern bool g_character_wheel_tutorial_requested_from_settings;
extern bool g_zones_tutorial_requested_from_settings;
extern const int default_pane_config_count;
extern const touch_pane_slot_info g_touch_pane_slots[SDL_TOUCH_PANE_BUTTON_COUNT];
extern const int g_touch_pane_visible_slots[SDL_TOUCH_PANE_VISIBLE_BUTTON_COUNT];
extern mono_font_prewarm_request g_mono_font_prewarm_queue[SDL_MONO_FONT_PREWARM_QUEUE_MAX];
extern int g_mono_font_prewarm_count;
extern mono_font_prewarm_job g_mono_font_prewarm_job;
extern Uint64 g_mono_font_atlas_generation;
extern Uint64 g_story_font_generation;
extern Uint64 g_sdl_present_generation;
extern SDL_Rect g_pane_rects[PANE_MAX];
extern SDL_Texture* g_left_panel_canvas;
extern int g_left_panel_canvas_w;
extern int g_left_panel_canvas_h;
extern bool g_left_panel_debug_dump_rows;
extern bool g_active_side_panes;
extern bool g_active_bottom_panes;
extern int g_description_overlay_main_anchor_depth;
extern int g_description_overlay_full_main_anchor_depth;
extern bool g_supporting_panes_layout_visible;
extern int g_inventory_pane_layout_rows;
extern int g_supply_pane_layout_rows;
extern bool g_touch_pane_hidden_layout_active;
extern bool g_touch_pane_proto_layout_active;
extern bool g_suppress_layout_refresh_present;
extern bool g_skip_main_redraw_on_layout_refresh;
extern bool g_defer_resize_handle_stuff;
extern bool g_touch_tutorial_suppress_runtime_top_panel;
extern gamepad_input_state g_gamepad_state;
extern bool g_gamepad_auto_ui;
extern int g_default_gamepad_button_bindings[SDL_GAMEPAD_BUTTON_COUNT];
extern int g_default_gamepad_trigger_bindings[GAMEPAD_TRIGGER_COUNT];
extern int g_default_gamepad_left_stick_bindings[GAMEPAD_STICK_DIR_COUNT];
extern int g_default_gamepad_right_stick_bindings[GAMEPAD_STICK_DIR_COUNT];
extern int g_default_gamepad_button_combo_bindings[GAMEPAD_MODIFIER_COUNT][SDL_GAMEPAD_BUTTON_COUNT];
extern int g_default_gamepad_trigger_combo_bindings[GAMEPAD_MODIFIER_COUNT][GAMEPAD_TRIGGER_COUNT];
extern int g_default_gamepad_left_stick_combo_bindings[GAMEPAD_MODIFIER_COUNT][GAMEPAD_STICK_DIR_COUNT];
extern int g_default_gamepad_right_stick_combo_bindings[GAMEPAD_MODIFIER_COUNT][GAMEPAD_STICK_DIR_COUNT];
extern int g_default_gamepad_shoulder_combo_binding;
extern bool g_default_gamepad_bindings_ready;
extern bool g_default_mouse_enabled;
extern int g_default_mouse_movement_mode;
extern bool g_default_mouse_tile_pointer;
extern bool g_default_mouse_settings_ready;
extern int g_default_touch_pane_bindings[SDL_TOUCH_PANE_PANEL_COUNT][SDL_TOUCH_PANE_BUTTON_COUNT];
extern char g_default_touch_pane_panel_names[SDL_TOUCH_PANE_PANEL_COUNT][SDL_TOUCH_PANE_LABEL_LEN];
extern bool g_default_touch_menu_command_enabled[SDL_TOUCH_MENU_CATEGORY_COUNT];
extern int g_default_touch_profile;
extern bool g_default_touch_pane_default_open;
extern bool g_default_touch_pane_key_labels_visible;
extern bool g_default_touch_pane_inventory_equipment_cycle;
extern int g_default_touch_movement_mode;
extern int g_default_touch_zone_overlay_mode;
extern bool g_default_touch_round_movement_enabled;
extern int g_default_touch_zone_center_bindings[SDL_TOUCH_ZONE_CENTER_BINDING_COUNT];
extern int g_default_touch_corner_up_down_side;
extern int g_default_touch_corner_action_bindings[SDL_TOUCH_CORNER_ACTION_BINDING_COUNT];
extern bool g_default_touch_top_panel_arrows_visible;
extern bool g_default_touch_top_panel_default_open;
extern float g_default_touch_top_panel_size;
extern int g_default_touch_top_panel_cell_count;
extern int g_default_touch_top_panel_rows;
extern int g_default_touch_top_panel_bindings[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
extern int g_default_touch_top_panel_long_bindings[SDL_TOUCH_TOP_PANEL_BUTTON_COUNT];
extern bool g_default_touch_thumb_enabled;
extern int g_default_touch_thumb_bindings[SDL_TOUCH_THUMB_BUTTON_COUNT];
extern int g_default_touch_thumb_long_bindings[SDL_TOUCH_THUMB_BUTTON_COUNT];
extern bool g_default_touch_swipe_enabled;
extern int g_default_touch_swipe_bindings[TOUCH_SWIPE_DIR_COUNT];
extern bool g_default_touch_pane_bindings_ready;
extern bool g_gamepad_capture_active;
extern bool g_gamepad_capture_ready;
extern Uint64 g_gamepad_capture_arm_time;
extern bool g_gamepad_capture_allow_modifier_combo;
extern int g_gamepad_capture_modifier;
extern int g_gamepad_capture_type;
extern int g_gamepad_capture_id;
extern bool g_keyboard_capture_active;
extern bool g_keyboard_capture_ready;
extern Uint64 g_keyboard_capture_arm_time;
extern SDL_Scancode g_keyboard_capture_scancode;
extern u16b g_keyboard_capture_modifiers;
extern int g_touch_pane_flash_slot;
extern Uint64 g_touch_pane_flash_until;
extern int g_touch_pane_pressed_slot;
extern bool g_touch_pane_second_panel;
extern bool g_touch_pane_ctrl_toggle;
extern bool g_touch_pane_reset_confirm_active;
extern bool g_touch_pane_yes_no_prompt_active;
extern char g_touch_pane_yes_no_prompt_text[SDL_TOUCH_YES_NO_LINE_LEN];
extern sdl_touch_yes_no_prompt_placement g_touch_pane_yes_no_prompt_placement;
extern sdl_touch_yes_no_prompt_hover g_touch_pane_yes_no_prompt_hover;
extern bool g_touch_pane_yes_no_prompt_anchor_active;
extern int g_touch_pane_yes_no_prompt_anchor_y;
extern int g_touch_pane_yes_no_prompt_anchor_x;
extern bool g_touch_pane_mobile_open;
extern touch_pane_press_state g_touch_pane_press;
extern touch_pane_press_state g_touch_thumb_press;
extern int g_touch_thumb_flash_button;
extern Uint64 g_touch_thumb_flash_until;
extern int g_touch_thumb_pressed_button;
extern bool g_touch_mouse_fallback_active;
extern SDL_MouseID g_touch_mouse_fallback_mouse_id;
extern bool g_handheld_untagged_mouse_fallback_logged;
extern touch_swipe_state g_touch_swipe;
extern welcome_touch_press_state g_welcome_touch_press;
extern sdl_welcome_screen_state g_sdl_welcome_screen;
extern sdl_character_sheet_screen_state g_sdl_character_sheet_screen;
extern touch_zone_press_state g_touch_zone_press;
extern touch_top_panel_press_state g_touch_top_panel_press;
extern touch_round_press_state g_touch_round_press;
extern int g_touch_round_last_dir;
extern bool g_player_tile_facing_right;
extern bool g_touch_top_panel_open;
extern int g_touch_top_panel_pressed_slot;
extern int g_touch_top_panel_hover_slot;
extern int g_touch_top_panel_flash_slot;
extern Uint64 g_touch_top_panel_flash_until;
extern menu_touch_press_state g_menu_touch_press;
extern character_panel_press_state g_character_panel_press;
extern int g_screen_back_gesture_depth;
extern bool g_screen_back_right_button_pending;
extern screen_back_touch_press_state g_screen_back_touch_press;
extern bool g_screen_back_suppress_touch_up;
extern SDL_FingerID g_screen_back_suppress_touch_finger_id;
extern menu_scroll_drag_state g_menu_scroll_drag;
extern pane_layout_drag_state g_pane_layout_drag;
extern side_pane_menu_state g_side_pane_menu;
extern log_pane_menu_state g_log_pane_menu;
extern int g_log_pane_display_filters[PANE_MAX];
extern bool g_log_pane_display_pending;
extern enum pane_type g_log_pane_pending_pane;
extern int g_log_pane_pending_filter;
extern map_touch_press_state g_map_touch_press;
extern player_action_menu_state g_player_action_menu;
extern player_exchange_target_state g_player_exchange_target;
extern bool g_map_touch_selected;
extern int g_map_touch_selected_y;
extern int g_map_touch_selected_x;
extern minimap_state g_minimap;
extern side_map_pane_state g_side_map_pane;
extern bool g_minimap_pending_focus_active;
extern int g_minimap_pending_focus_y;
extern int g_minimap_pending_focus_x;
extern pointer_attack_state g_pointer_attack;
extern pointer_aim_state g_pointer_aim;
extern mouse_path_state g_mouse_path;
extern object_tooltip_state g_object_tooltip;
extern description_overlay_state g_description_overlay;
extern main_map_drag_state g_main_map_drag;
extern bool g_unified_look_active;
extern sdl_unified_look_prompt_state g_unified_look_prompt;
extern sdl_unified_look_sidebar_state g_unified_look_sidebar;
extern sdl_song_menu_state g_song_menu;
extern sdl_question_menu_state g_question_menu;
extern bool g_unified_look_map_hover_enabled;
extern bool g_unified_look_map_hover_pending;
extern bool g_unified_look_map_hover_wake_pending;
extern int g_unified_look_map_hover_y;
extern int g_unified_look_map_hover_x;
extern bool g_unified_look_map_describe_pending;
extern int g_unified_look_map_describe_y;
extern int g_unified_look_map_describe_x;
extern bool g_unified_look_map_target_pending;
extern int g_unified_look_map_target_y;
extern int g_unified_look_map_target_x;
extern unified_look_map_drag_state g_unified_look_map_drag;
extern bool g_unified_look_map_pan_pending;
extern bool g_unified_look_map_pan_wake_pending;
extern int g_unified_look_map_pan_dy;
extern int g_unified_look_map_pan_dx;
extern bool g_unified_look_main_zoom_pending;
extern bool g_unified_look_main_zoom_wake_pending;
extern int g_unified_look_main_zoom_scale;
extern int g_main_screen_status_selected_action;
extern int g_main_screen_status_selected_col;
extern int g_main_screen_panel_selected_action;
extern int g_main_screen_panel_selected_row;
extern bool g_main_menu_pane_hover;
extern bool g_main_menu_overlay_active;
extern int g_main_menu_overlay_highlight;
extern int g_main_menu_overlay_hover_choice;
extern int g_main_menu_overlay_first_choice;
extern int g_main_menu_overlay_left_stick_dir;
extern int g_main_menu_overlay_right_stick_dir;
extern int g_depth_pane_hover_action;
extern u16b g_mouse_path_reverse[SDL_MOUSE_PATH_MAX_GRIDS];
extern mouse_path_search_state g_mouse_path_search;
extern const byte g_mouse_path_route_dirs[SDL_MOUSE_PATH_ROUTE_DIRS];
extern bool g_sdl_blocking_key_wait;
extern bool g_direct_touch_present;
extern int g_main_view_zoom_scale;
extern int g_main_view_layout_scale_override;
extern int g_main_view_zoom_suspended_stack[16];
extern int g_main_view_zoom_suspended_depth;
extern int g_terminal_menu_scale_override;
extern int g_terminal_menu_scale_stack[16];
extern int g_terminal_menu_scale_depth;
extern int g_auto_aux_main_cell_h_override;
extern bool g_mobile_lifecycle_watch_registered;
extern bool g_mobile_lifecycle_autosaved;
extern bool g_mobile_first_start_auto_scale_pending;

bool sdl_key_is_escape_or_back(SDL_Keycode key);
void sdl_copy_pane_configs(struct pane_config* dest, int* dest_count, const struct pane_config* src, int src_count);
bool sdl_min_terminal_mode_is_valid(int mode);
int sdl_main_view_scale_floor_for_mode(int mode);
int sdl_platform_max_main_view_scale_for_mode(int mode);
int sdl_main_view_scale_floor(void);
int sdl_clamp_main_view_scale_floor(int scale, int mode);
int sdl_clamp_main_view_scale_platform_bounds(int scale, int mode);
void sdl_store_active_pane_profile(int mode);
void sdl_apply_stored_pane_profile(int mode);
void sdl_seed_all_pane_profiles_from_active(void);
int sdl_pane_config_index_in_array(const struct pane_config* configs, int count, enum pane_type pane);
bool sdl_normalize_unified_log_pane_config(struct pane_config* configs, int* config_count, bool enable_added_log);
void sdl_normalize_unified_log_pane_profiles(bool enable_added_log);
enum pane_placement sdl_default_status_pane_placement(
    const struct pane_config* configs, int config_count);
int sdl_default_main_scale_for_screen_size(int screen_width, int screen_height, int mode);
void sdl_store_platform_max_main_view_scales(int screen_width, int screen_height);
void sdl_refresh_platform_max_main_view_scales_for_current_layout(const char* reason);
void sdl_reset_config_to_resolution_defaults(int screen_width, int screen_height);
int sdl_min_terminal_cols_for_mode(int mode);
int sdl_min_terminal_rows_for_mode(int mode);
int sdl_current_min_terminal_cols(void);
int sdl_current_min_terminal_rows(void);
const char* sdl_min_terminal_mode_name(int mode);
int sdl_touch_pane_visible_slot_at(int visible_index);
sdl_view* sdl_view_from_term(term* t);
enum pane_type sdl_view_pane_type(const sdl_view* view);
bool sdl_view_is_overlay_log_pane(const sdl_view* view);
Uint8 sdl_view_background_alpha(const sdl_view* view);
bool sdl_rect_has_area(const SDL_Rect* rect);
SDL_Rect sdl_get_window_pixel_rect(void);
SDL_Rect sdl_window_rect_to_pixel_rect(const SDL_Rect* rect);
bool sdl_android_has_controller_device(void);
SDL_Rect sdl_get_android_display_cutout_rect(void);
void sdl_refresh_safe_area(void);
SDL_Rect sdl_get_layout_screen_rect(void);
bool sdl_mobile_prefer_safe_edge_alignment(void);
void sdl_resize_for_current_layout(void);
void sdl_sync_palette(void);
void sdl_view_destroy(sdl_view* d);
void sdl_left_panel_canvas_destroy(void);
int sdl_overlay_margin_px(void);
int sdl_overlay_inner_gap_px(void);
int sdl_overlay_edge_gap_px(int area_px, int content_px);
bool sdl_pane_default_enabled_on_migration(enum pane_type pane);
bool sdl_migrate_legacy_main_menu_depth_pane( struct pane_config* configs, int count);
bool sdl_ensure_default_pane_config_entries(struct pane_config* configs, int* config_count, bool enable_new_panes);
void sdl_ensure_default_pane_configs_present(bool enable_new_panes);
void sdl_ensure_default_pane_profiles_present(bool enable_new_panes);
void sdl_ensure_touch_pane_config_present(void);
int sdl_left_panel_pane_config_index(void);
enum pane_placement sdl_left_panel_pane_placement(void);
bool sdl_left_panel_pane_placement_is_right(enum pane_placement where);
bool sdl_left_panel_pane_placement_is_horizontal_center( enum pane_placement where);
bool sdl_left_panel_pane_placement_is_bottom(enum pane_placement where);
bool sdl_left_panel_pane_placement_is_vertical_center( enum pane_placement where);
bool sdl_left_panel_pane_config_enabled(void);
bool sdl_left_panel_cell_has_visible_content(byte a, char c);
void sdl_left_panel_debug_log_cell_size(int content_cols, int initial_cell_w, int initial_cell_h, int final_cell_w, int final_cell_h, int visual_cols, int visual_w, int border_cols, int available_w, int max_cell_w);
void sdl_left_panel_pane_cell_size_for_view(const sdl_view* view, int content_cols, int* out_cell_w, int* out_cell_h);
int sdl_left_panel_pane_rows_for_view(const sdl_view* view);
void sdl_left_panel_debug_log_content_size(int term_w, int term_h, int scan_cols, int scan_rows, int measured_cols, int measured_rows);
bool sdl_left_panel_content_size_for_term(const term* t, int scan_rows, int* out_cols, int* out_rows);
bool sdl_left_panel_render_source_term(const sdl_view* view, int source_rows, term* panel_term, int* out_source_w, int* out_source_h);
void sdl_left_panel_source_invalidate(void);
const term* sdl_left_panel_source_term_for_view(const sdl_view* view, int source_rows);
bool sdl_left_panel_content_size_for_view(const sdl_view* view, int* out_cols, int* out_rows);
bool sdl_left_panel_content_size_for_scratch(const sdl_view* view, int source_rows, int* out_cols, int* out_rows);
int sdl_left_panel_source_row_width_for_term(const term* t, int source_row);
int sdl_left_panel_source_row_width_for_view(const sdl_view* view, int source_row);
bool sdl_left_panel_compact_light_span_for_term(const term* t, const term_win* scr, sdl_left_panel_compact_light_span* out);
int sdl_left_panel_compact_source_row_width_for_view( const sdl_view* view, int source_row, bool row_mode);
void sdl_left_panel_compact_metrics_for_view(const sdl_view* view, sdl_left_panel_metrics* metrics);
bool sdl_left_panel_metrics_for_view(const sdl_view* view, sdl_left_panel_metrics* metrics);
int sdl_left_panel_content_x_for_metrics( const sdl_left_panel_metrics* metrics);
bool sdl_left_panel_pane_rect_for_metrics(const sdl_view* view, const sdl_left_panel_metrics* metrics, SDL_FRect* out_rect);
bool sdl_left_panel_pane_layout_enabled(void);
bool sdl_saved_screen_left_panel_pane_active(void);
bool sdl_left_panel_pane_presentation_active(void);
bool sdl_left_panel_pane_renders_character_panel(void);
bool sdl_combat_overlay_pane_presentation_active(void);
bool sdl_combat_overlay_melee_uses_offhand_row(void);
int sdl_combat_overlay_source_row_count(void);
bool sdl_combat_overlay_source_row_at_index(int index, int* out_row);
int sdl_combat_overlay_visible_row_count(int panel_rows);
bool sdl_combat_overlay_visible_source_row_at_index(int index,
    int panel_rows, int* out_row);
bool sdl_combat_overlay_source_row_visible(int source_row);
bool sdl_left_panel_source_row_hidden(int source_row);
int sdl_left_panel_output_row_for_source_row(int source_row);
int sdl_left_panel_source_row_for_output_row(int output_row);
bool sdl_left_panel_pane_map_coverage(int* start_col, int* cols, int* start_row, int* rows);
bool sdl_combat_overlay_pane_map_coverage(int* start_col, int* cols, int* start_row, int* rows);
bool sdl_overlay_log_pane_map_coverage(int* start_col, int* cols, int* start_row, int* rows);
int sdl_map_overlay_map_coverages(int max_rects, int* start_cols, int* cols, int* start_rows, int* rows);
bool sdl_overlay_log_pane_current_rect(SDL_Rect* out_rect);
void sdl_reset_top_right_overlay_offset(void);
void sdl_apply_top_right_overlay_offset(void);
bool sdl_left_panel_pane_runtime_active(void);
bool sdl_left_panel_pane_collapsed(void);
int sdl_left_panel_compact_mode_normalized(int mode);
bool sdl_left_panel_compact_row_mode(void);
bool sdl_left_panel_pane_has_border_columns(void);
int sdl_main_view_visual_cols_for_width(int width_px, int cell_w);
int sdl_main_view_logical_cols_for_visual_cols(int visual_cols);
const char* sdl_startup_device_class_name(sdl_startup_device_class device);
bool sdl_startup_device_class_uses_controller_ui( sdl_startup_device_class device);
void sdl_apply_startup_input_defaults_to_config( struct sdl_config* target, sdl_startup_device_class device);
void sdl_set_touch_pane_config_enabled(bool enabled);
void sdl_apply_first_start_device_defaults( sdl_startup_device_class device);
sdl_startup_device_class sdl_detect_startup_device_class( int screen_width, int screen_height);
struct pane_config* sdl_find_pane_config_entry(struct pane_config* configs, int count, enum pane_type pane);
void sdl_mobile_reset_default_pane_configs(struct pane_config* configs, int count);
void sdl_mobile_set_touch_pane_enabled(struct pane_config* configs, int count, bool enabled);
void sdl_mobile_disable_bottom_panes(struct pane_config* configs, int count);
void sdl_mobile_configure_bottom_wide(struct pane_config* configs, int count, int rows);
void sdl_mobile_configure_bottom_narrow(struct pane_config* configs, int count, int total_rows);
void sdl_mobile_set_right_panes(struct pane_config* configs, int count, bool inventory_enabled, bool worn_enabled);
int sdl_mobile_pane_cols(const SDL_Rect* panes, const int* cell_widths, enum pane_type type);
int sdl_mobile_pane_rows(const SDL_Rect* panes, const int* cell_heights, enum pane_type type);
bool sdl_mobile_enabled_panes_fit(const struct pane_config* configs, int count, const SDL_Rect* panes, const int* cell_widths, const int* cell_heights);
bool sdl_mobile_layout_fits(const SDL_Rect* screen, int scale, const struct pane_config* configs, int count, SDL_Rect* out_panes, int* out_cell_widths, int* out_cell_heights, int* out_main_cols, int* out_main_rows);
int sdl_mobile_select_default_scale(const SDL_Rect* screen, const struct pane_config* configs, int count);
void sdl_apply_mobile_default_pane_layout(const SDL_Rect* screen, bool has_controller);
bool sdl_mobile_maybe_apply_first_start_auto_scale(const char* reason);
bool sdl_touch_pane_is_config_enabled(void);
bool sdl_touch_pane_is_left_placement(void);
bool sdl_touch_only_mobile_device_active(void);
bool sdl_touch_tutorial_device_available(void);
bool sdl_touch_tutorial_full_mode(void);
bool sdl_touch_pane_uses_mobile_toggle(void);
bool sdl_touch_pane_mobile_layout_open(void);
bool sdl_touch_pane_hidden_mode_active(void);
bool sdl_touch_pane_proto_mode_active(void);
bool sdl_touch_pane_proto_slot_allowed(int slot);
bool sdl_touch_pane_slot_visible_in_current_mode(int slot);
int sdl_touch_pane_proto_binding_for_slot(int slot);
void sdl_touch_pane_refresh_after_layout_toggle(void);
void sdl_touch_pane_set_mobile_open(bool open);
bool sdl_touch_pane_panel_is_valid(int panel);
int sdl_touch_pane_active_panel(void);
int sdl_touch_pane_other_panel(int panel);
int sdl_touch_pane_raw_binding_for_panel(int panel, int index);
int sdl_touch_pane_effective_binding_for_panel(int panel, int index);
bool sdl_query_direct_touch_present(void);
bool sdl_refresh_direct_touch_present(void);
void sdl_note_touch_event_device(SDL_TouchID touch_id);
void sdl_update_cursor_visibility(void);
void sdl_mouse_cursor_button_state(int button, bool pressed);
int sdl_mouse_cursor_animation_timeout_ms(Uint64 now_ns);
void sdl_mouse_cursor_animation_update(Uint64 now_ns);
void sdl_mouse_cursor_shutdown(void);
void sdl_log_mouse_devices(void);
int sdl_build_active_pane_config(struct pane_config* active, bool include_side, bool include_bottom, bool touch_only);
int sdl_configured_main_view_scale(void);
int sdl_main_view_layout_scale(void);
bool sdl_main_view_zoom_keep_for_saved_screen(void);
int sdl_current_main_view_scale(void);
int sdl_auto_font_size_from_main(int numerator, int denominator);
int sdl_auto_aux_view_font_size(void);
int sdl_auto_pane_font_size(enum pane_type type);
int sdl_resolve_aux_view_font_size(int requested_size);
int sdl_resolve_pane_font_size(enum pane_type type, int requested_size);
int sdl_effective_pane_font_size_for_config(const struct pane_config* pc);
int sdl_effective_pane_font_size_for_type(enum pane_type type);
int sdl_aux_cell_height_for_font_size(int font_size);
int sdl_effective_pane_cell_height_for_config(const struct pane_config* pc);
int sdl_effective_pane_cell_height_for_type(enum pane_type type);
int sdl_supporting_pane_cell_width(enum pane_type type,
    enum pane_placement where, int cell_h);
void sdl_build_supporting_pane_metrics(const struct pane_config* configs, int count, int* cell_widths, int* cell_heights);
bool sdl_prune_unusable_panes(struct pane_config* active, int active_count, SDL_Rect* panes, const int* cell_widths, const int* cell_heights);
int sdl_inventory_pane_desired_rows(void);
bool sdl_inventory_pane_dynamic_configured(void);
void sdl_apply_dynamic_inventory_pane_size(struct pane_config* active, int active_count);
int sdl_supply_pane_desired_rows(void);
bool sdl_supply_pane_dynamic_configured(void);
void sdl_apply_dynamic_supply_pane_size(struct pane_config* active, int active_count);
int sdl_touch_pane_target_width_px(int pane_height_px);
void sdl_apply_dynamic_auto_pane_sizes(struct pane_config* active, int active_count, const SDL_Rect* screen, const int* cell_widths, const int* cell_heights, int margin_px);
void sdl_place_active_panes(const SDL_Rect* screen, SDL_Rect* panes, bool include_side, bool include_bottom, bool touch_only);
bool sdl_active_group_has_visible(const struct pane_config* active, int active_count, const SDL_Rect* panes, bool side);
void sdl_place_active_panes_fitting_main(const SDL_Rect* screen, SDL_Rect* panes, bool include_side, bool include_bottom, bool touch_only, bool* out_side, bool* out_bottom);
int sdl_bottom_pane_group_rows_for_minimum(const SDL_Rect* panes, enum pane_placement where);
int sdl_bottom_pane_rows_for_minimum(const SDL_Rect* panes);
void sdl_compute_pruned_split_panes_for_mode_ex(const SDL_Rect* screen, int mode, int scale, bool aux_follows_candidate_scale, SDL_Rect* panes, bool* out_side, bool* out_bottom, int* out_cols, int* out_rows, int* out_rows_with_bottom);
void sdl_mark_active_supporting_panes_dirty(const SDL_Rect* panes);
bool sdl_hide_supporting_panes_mode_effective(void);
void sdl_draw_pane_edges(const SDL_Rect* rect, bool draw_left, bool draw_top, bool draw_right, bool draw_bottom);
bool sdl_should_show_supporting_panes(void);
bool sdl_layout_matches_supporting_pane_visibility(void);
void sdl_compute_display_panes(SDL_Rect* panes);
int sdl_max_scale_for_rect_mode(const SDL_Rect* rect, int mode);
int sdl_max_scale_for_rect(const SDL_Rect* rect);
bool sdl_apply_default_main_scale_for_layout(const char* reason);
int sdl_max_scale_for_layout(const SDL_Rect* screen, int mode);
int sdl_main_view_terminal_cols_for_map_squares(int map_cols);
int sdl_main_view_terminal_rows_for_map_squares(int map_rows);
int sdl_main_view_map_cols_for_terminal_cols(int cols);
int sdl_main_view_map_rows_for_terminal_rows(int rows);
int sdl_max_main_view_zoom_scale_for_layout(const SDL_Rect* screen, int mode);
int sdl_min_main_view_zoom_scale_for_layout(const SDL_Rect* screen, int mode);
int sdl_max_scale_for_window_mode(int mode);
bool sdl_mode_scale_fits_window(const SDL_Rect* screen, int mode, int scale, int* cols, int* rows);
void sdl_ensure_window_size_for_min_terminal(const SDL_Rect* screen, int* window_width, int* window_height);
void sdl_format_layout_recovery_message(const char* reason, const sdl_layout_recovery_result* recovery, char* buf, size_t buflen);
void sdl_append_issue_line(char* buf, size_t buflen, const char* line);
bool sdl_recover_layout_for_current_window(const char* reason, bool notify_user, sdl_layout_recovery_result* recovery);
bool sdl_prompt_reset_sdl_defaults(const char* issue_summary, int screen_width, int screen_height);
bool sdl_resolution_matches_pair(int width, int height, int native_w, int native_h);
bool sdl_is_desktop_handheld_resolution(int width, int height);
bool sdl_mobile_portrait_layout_active(void);
bool sdl_mobile_orientation_matches_layout(void);
void sdl_mobile_portrait_scale_reference_rect(const SDL_Rect* source,
    SDL_Rect* out);
bool sdl_mobile_portrait_control_regions(SDL_Rect* out_left,
    SDL_Rect* out_right);
sdl_startup_device_class sdl_prompt_desktop_startup_input_device( int screen_width, int screen_height);
bool sdl_touch_pane_binding_is_direction(int binding);
bool sdl_touch_pane_slot_uses_long_press(int slot, int binding);
bool sdl_touch_pane_confirm_binding(int binding);
bool sdl_touch_pane_main_panel_has_confirm_excluding(int skip_index);
void sdl_touch_pane_ensure_main_panel_confirm(void);
void sdl_touch_pane_begin_reset_confirm(void);
void sdl_touch_pane_finish_reset_confirm(bool confirmed);
void sdl_touch_pane_handle_reset_prompt_pointer(float x, float y);
bool sdl_touch_pane_compute_layout(const SDL_Rect* pane_rect, SDL_FRect* slot_rects);
bool sdl_touch_pane_current_rect(SDL_Rect* out_rect);
bool sdl_side_map_pane_current_rect(SDL_Rect* out_rect);
const struct pane_config* sdl_status_pane_config(void);
const struct pane_config* sdl_depth_pane_config(void);
const struct pane_config* sdl_status_depth_pane_config(void);
float sdl_overlay_panel_x(const SDL_Rect* anchor, enum pane_placement where, int panel_w);
float sdl_overlay_panel_y(const SDL_Rect* anchor, enum pane_placement where, int panel_h);
SDL_FRect sdl_overlay_panel_rect(const SDL_Rect* anchor, enum pane_placement where, int panel_w, int panel_h, const SDL_Rect* screen);
bool sdl_status_pane_current_rect(SDL_Rect* out_rect, enum pane_placement* out_where);
const struct pane_config* sdl_combat_overlay_pane_config(void);
bool sdl_combat_overlay_connected_to_left_panel(bool* out_combat_below);
bool sdl_combat_overlay_pane_current_rect(SDL_Rect* out_rect);
bool sdl_combat_overlay_pane_content_rect(SDL_Rect* out_rect);
float sdl_touch_pane_clampf(float value, float min_value, float max_value);
int sdl_touch_pane_story_text_width(TTF_Font* font, cptr text);
SDL_Color sdl_status_pane_color(byte attr);
void sdl_status_pane_format_turns(char* buf, size_t buflen, int turns);
void sdl_status_pane_add(status_pane_entry* entries, int max_entries, int* count, cptr label, cptr detail, byte attr);
void sdl_status_pane_add_timed(status_pane_entry* entries, int max_entries, int* count, cptr label, int turns, byte attr);
bool sdl_status_pane_song_name(byte song, char* out, size_t out_sz);
bool sdl_status_pane_current_song_detail(char* out, size_t out_sz);
int sdl_status_pane_collect(status_pane_entry* entries, int max_entries);
void sdl_status_pane_entry_line(const status_pane_entry* entry, char* out, size_t out_sz);
int sdl_status_pane_text_width(TTF_Font* font, cptr text);
bool sdl_status_pane_entry_prefers_span(const status_pane_entry* entry);
int sdl_status_pane_layout_entries(TTF_Font* font, const status_pane_entry* entries, int visible_count, int more_count, status_pane_layout_item* items, int max_items, int max_content_w, int max_rows, int item_pad_x, int column_gap_x, int min_item_w, int* out_content_w, int* out_row_count);
void sdl_status_pane_draw_text(TTF_Font* font, cptr text, SDL_Color color, float x, float y, float max_w, float row_h, bool right_align);
bool sdl_status_pane_layout(status_pane_layout* out);
void sdl_status_pane_render(void);
bool sdl_status_depth_pane_layout(status_depth_pane_layout* out);
bool sdl_status_depth_pane_current_rect(SDL_FRect* out);
void sdl_status_depth_pane_render(void);
bool sdl_overlay_pane_anchor_rect(enum pane_type pane_type, SDL_Rect* out);
void sdl_push_description_overlay_main_anchor(void);
void sdl_pop_description_overlay_main_anchor(void);
void sdl_push_description_overlay_full_main_anchor(void);
void sdl_pop_description_overlay_full_main_anchor(void);
int sdl_main_menu_pane_font_px(void);
int sdl_main_menu_button_height_for_screen(const SDL_Rect* screen);
bool sdl_main_menu_pane_context_visible(void);
const char* sdl_main_menu_mono_font_path(void);
TTF_Font* sdl_main_menu_mono_font_for_height(int font_px);
void sdl_main_menu_shortcut_display_label(int choice, char* buf, size_t buflen);
const sdl_welcome_intro_line* sdl_welcome_intro_lines_for_style( int intro_style);
SDL_Color sdl_welcome_color(byte attr, byte alpha);
bool sdl_welcome_screen_available(void);
void sdl_welcome_screen_clear_hits(void);
void sdl_welcome_screen_mark_dirty(void);
int sdl_welcome_screen_normalize_intro_style(int intro_style);
bool sdl_welcome_screen_active(void);
bool sdl_welcome_screen_show_intro(int intro_style, bool show_wizard);
bool sdl_welcome_screen_show_menu(bool show_wizard, bool new_metarun);
bool sdl_welcome_screen_set_status(cptr status);
bool sdl_welcome_screen_show_loading(cptr status);
void sdl_welcome_screen_hide(void);
bool sdl_welcome_screen_cycle_intro(int direction);
bool sdl_welcome_text_token_range(cptr text, cptr token, int* start, int* end);
bool sdl_welcome_text_command_range(cptr text, cptr token_a, cptr token_b, int* start, int* end);
void sdl_welcome_compose_menu_line(char* menu_line, size_t menu_size, char* quit_command, size_t quit_command_size, cptr* primary_token);
void sdl_welcome_render_intro_canvas(const SDL_Rect* canvas);
void sdl_welcome_render_status_canvas(const SDL_Rect* canvas);
void sdl_welcome_render_menu_footer_canvas(const SDL_Rect* canvas);
bool sdl_narrative_portrait_transform_pointer(float* x, float* y);
void sdl_welcome_screen_render(void);
void sdl_halls_screen_render(void);
bool sdl_halls_screen_handle_pointer_event(const SDL_Event* ev);
void sdl_poetry_screen_begin(cptr title, cptr body, cptr transition,
    cptr prompt);
void sdl_poetry_sequence_layout_begin(void);
void sdl_poetry_sequence_layout_end(void);
void sdl_poetry_screen_begin_choices(cptr title);
void sdl_poetry_screen_begin_blocks(cptr title, cptr prompt);
int sdl_poetry_screen_add_block(cptr text, byte attr);
void sdl_poetry_screen_set_block_visible(int block, bool visible);
void sdl_poetry_screen_set_block_alpha(int block, byte alpha);
void sdl_poetry_screen_set_block_attr(int block, byte attr);
void sdl_poetry_screen_add_choice(int choice, cptr label, cptr body);
void sdl_poetry_screen_set_choice_visible(int choice, bool visible,
    byte label_attr, byte body_attr);
void sdl_poetry_screen_set_choice_alpha(int choice, byte alpha);
void sdl_poetry_screen_set_highlight(int choice);
void sdl_poetry_screen_set_prompt(cptr prompt, bool visible);
void sdl_poetry_screen_set_alpha(byte title_alpha, byte body_alpha,
    byte transition_alpha, byte prompt_alpha);
void sdl_poetry_screen_update(bool title_visible, byte title_attr,
    bool body_visible, byte body_attr, bool transition_visible,
    byte transition_attr, bool prompt_visible);
void sdl_poetry_screen_hide(void);
bool sdl_poetry_screen_active(void);
bool sdl_poetry_screen_captures_pointer(void);
bool sdl_poetry_screen_handle_pointer(float x, float y, int action);
bool sdl_poetry_screen_handle_hover_pointer(float x, float y);
void sdl_poetry_screen_render(void);
bool sdl_pause_text_screen_begin(void);
void sdl_pause_text_screen_add_line(cptr text, byte attr, int indent);
void sdl_pause_text_screen_set_visible_lines(int visible_lines);
void sdl_pause_text_screen_hide(void);
bool sdl_pause_text_screen_active(void);
void sdl_pause_text_screen_render(void);
bool sdl_tale_screen_begin(cptr title);
void sdl_tale_screen_add_entry(cptr heading, cptr body);
void sdl_tale_screen_set_manuscript(bool enabled);
int sdl_tale_screen_current_page_entry_count(void);
int sdl_tale_screen_current_page_entry_at(int position);
int sdl_tale_screen_entry_character_count(int entry);
int sdl_tale_screen_entry_character_at(int entry, int position);
void sdl_tale_screen_set_active_entry(int active_entry, byte alpha);
void sdl_tale_screen_set_typewriter_entry(int active_entry,
    int visible_characters, bool cursor_visible);
void sdl_tale_screen_set_prompt(cptr prompt, bool visible, bool final);
bool sdl_tale_screen_advance_page(void);
bool sdl_tale_screen_is_last_page(void);
void sdl_tale_screen_hide(void);
bool sdl_tale_screen_active(void);
bool sdl_tale_screen_handle_pointer(float x, float y);
bool sdl_tale_screen_handle_hover_pointer(float x, float y);
void sdl_tale_screen_render(void);
float sdl_char_sheet_clampf(float value, float min_value, float max_value);
int sdl_char_sheet_clampi(int value, int min_value, int max_value);
int sdl_char_sheet_text_width(TTF_Font* font, cptr text);
float sdl_char_sheet_line_h(TTF_Font* font, int fallback_px, float scale);
TTF_Font* sdl_char_sheet_font_for_rows(float available_h, int rows, int min_px, int max_px, float line_scale, float* out_line_h, int* out_px);
SDL_FRect sdl_char_sheet_draw_text(TTF_Font* font, cptr text, byte attr, float x, float y, float max_w, float max_h, bool centered);
void sdl_char_sheet_draw_title_text(TTF_Font* font, cptr title, byte title_attr, cptr suffix, byte suffix_attr, float x, float y, float max_w, float max_h);
TTF_Font* sdl_char_sheet_font_for_wrapped_text(cptr text, float width, float available_h, int min_px, int max_px, float line_scale, int slot, float* out_line_h, int* out_lines, int* out_px);
int sdl_char_sheet_font_px_for_line_height(float target_h, int min_px, int max_px);
bool sdl_char_sheet_choice_is_valid(int choice);
bool sdl_char_sheet_prompt_choice_is_valid(int choice);
void sdl_char_sheet_clear_hits(void);
bool sdl_char_sheet_panel_rect(cptr heading, SDL_FRect* out);
void sdl_char_sheet_add_hit(SDL_FRect rect, int choice, cptr desc, byte attr);
void sdl_char_sheet_add_prompt_hit(SDL_FRect rect, int choice);
void sdl_char_sheet_add_select_button_hit(SDL_FRect rect, int choice);
const sdl_character_sheet_hit* sdl_char_sheet_hit_at(float x, float y);
const sdl_character_sheet_live_item* sdl_char_sheet_live_item_by_choice( int choice);
const sdl_character_sheet_live_item* sdl_char_sheet_live_skill_item( int skill);
const sdl_character_sheet_live_item* sdl_char_sheet_live_label_item( cptr label);
bool sdl_char_sheet_choice_focused(int choice);
bool sdl_char_sheet_choice_pressable(int choice);
bool sdl_char_sheet_prompt_focused(int choice);
void sdl_char_sheet_draw_focus_rect(SDL_FRect rect, bool strong);
byte sdl_char_sheet_focus_text_attr(byte attr, bool focused);
void sdl_char_sheet_format_tenths(char* buf, size_t buflen, long tenths);
byte sdl_char_sheet_format_deep_call(char* buf, size_t buflen);
float sdl_char_sheet_min_depth_progress(void);
void sdl_char_sheet_song_name(byte song, char* out, size_t outsz);
void sdl_char_sheet_title(char* out, size_t outsz);
void sdl_char_sheet_draw_heading(TTF_Font* font, cptr heading, float x, float y, float w, float line_h);
void sdl_char_sheet_draw_labeled_line(TTF_Font* font, cptr text, byte attr, int choice, cptr desc, float x, float y, float w, float line_h, float label_fraction);
SDL_FRect sdl_char_sheet_alloc_rect(float x, float y, float w, float line_h, int row, int col, int span);
void sdl_char_sheet_alloc_text(TTF_Font* font, float x, float y, float w, float line_h, int row, int col, int span, byte attr, cptr text, bool focused);
bool sdl_char_sheet_alloc_row_visible(float y, float h, float line_h, int row);
void sdl_char_sheet_copy_trimmed(cptr src, char* dst, size_t dstsz);
void sdl_char_sheet_draw_birth_stat_table_row(TTF_Font* font, float x, float y, float w, float h, float line_h, int row, int stat, bool allocation);
void sdl_char_sheet_draw_birth_skill_table_row(TTF_Font* font, float x, float y, float w, float h, float line_h, int row, int skill, bool allocation);
void sdl_char_sheet_draw_birth_status_row(TTF_Font* font, float x, float y, float w, float h, float line_h, int row, cptr status);
void sdl_char_sheet_draw_birth_allocation_area(TTF_Font* font, float x, float y, float w, float h, float line_h, bool stats_screen);
void sdl_char_sheet_draw_wrapped(TTF_Font* font, cptr text, byte attr, float x, float y, float w, float h, float line_h, int line_count);
void sdl_char_sheet_draw_history(TTF_Font* font, cptr text, float x, float y, float w, float h, float line_h, int line_count);
bool sdl_char_sheet_split_first_paragraph(cptr text, char* first, size_t first_len, cptr* rest);
void sdl_char_sheet_draw_prompt(TTF_Font* font, cptr prompt, float x, float y, float w, float h);
bool sdl_char_sheet_birth_context(void);
cptr sdl_char_sheet_hover_desc(SDL_FRect* out_rect, byte* out_attr);
void sdl_char_sheet_render_hover_tooltip(void);
float sdl_char_sheet_sample_panel_natural_w(TTF_Font* font, cptr heading, cptr sample, float label_fraction);
int sdl_char_sheet_target_ncols(float content_w, float screen_h);
bool sdl_char_sheet_book_context(void);
float sdl_char_sheet_book_width(int body_px, float content_w);
int sdl_char_sheet_narrative_pack(int body_px, float content_w, float top_y, float region_bottom, int* page_start);
int sdl_char_sheet_narrative_choose_px(float canvas_h, float content_w, float top_y, float region_bottom);
int sdl_char_sheet_book_body_px(float canvas_h, float content_w, float top_y, float region_bottom, int title_px);
void sdl_char_sheet_draw_page_frame(float px, float py, float pw, float ph, float bt);
void sdl_char_sheet_paginate_narrative(float canvas_h, float content_w, float top_y, float region_bottom, int title_px);
void sdl_char_sheet_render_narrative_page(int page, TTF_Font* body_font,
    float book_x, float book_w, float top_y, float region_bottom,
    float body_lh, bool register_hits);
void sdl_char_sheet_render_book_page(int page, float canvas_h, float content_x, float content_w, float top_y, float region_bottom, int title_px, bool register_hits);
void sdl_char_sheet_draw_curled_leaf(SDL_Texture* leaf, SDL_FRect region, float cp);
void sdl_character_sheet_screen_begin_page_turn(int dir);
int sdl_select_page_turn_timeout_ms(Uint64 now_ns);
bool sdl_character_sheet_screen_birth_sequence_active(void);
void sdl_character_sheet_birth_swipe_cancel(void);
void sdl_character_sheet_birth_swipe_begin(float x, float y, SDL_FingerID finger_id);
int sdl_character_sheet_birth_swipe_key_for_dir(int dir);
bool sdl_character_sheet_birth_swipe_motion(float x, float y, SDL_FingerID finger_id);
void sdl_character_sheet_screen_render(void);
bool sdl_character_sheet_screen_active(void);
void sdl_character_sheet_screen_hide(void);
void sdl_character_sheet_screen_begin_live(int focus_choice);
void sdl_character_sheet_screen_begin_birth_preview(void);
void sdl_character_sheet_screen_add_live_item(int choice, int kind, int skill, int value_kind, cptr label, cptr desc);
void sdl_character_sheet_screen_show_birth_stats(const int* stats, const int* costs, int selected_stat, int points_left);
void sdl_character_sheet_screen_show_birth_skills(const int* old_base, const int* skill_gain, const int* costs, int selected_skill, int points_left);
void sdl_select_page_turn_free(void);
void sdl_character_sheet_screen_reset_select_page(void);
void sdl_character_sheet_screen_begin_select(int focus_choice, cptr title);
void sdl_character_sheet_screen_set_select_menu_style(bool enabled);
void sdl_character_sheet_screen_begin_book(cptr title);
void sdl_character_sheet_screen_add_book_paragraph(cptr text);
void sdl_character_sheet_screen_break_book_page(void);
void sdl_character_sheet_screen_highlight_book_paragraph(void);
void sdl_character_sheet_screen_commit_book(void);
void sdl_character_sheet_screen_set_book_target_page_count(int page_count);
int sdl_character_sheet_screen_book_contents_page(int contents_index);
void sdl_character_sheet_screen_add_select_row(int choice, cptr label, int attr, cptr desc);
void sdl_character_sheet_screen_set_last_select_row_reset(int reset_choice);
void sdl_character_sheet_screen_set_last_select_row_confirmable(bool confirmable);
void sdl_character_sheet_screen_add_select_heading(cptr label);
void sdl_character_sheet_screen_set_select_intro(cptr text);
int sdl_character_sheet_screen_select_page(void);
int sdl_character_sheet_screen_select_page_count(void);
bool sdl_character_sheet_screen_page_turning(void);
void sdl_character_sheet_screen_set_select_frame(cptr top, cptr bottom);
void sdl_character_sheet_screen_set_select_title_detail(cptr title, cptr suffix, int suffix_attr);
void sdl_character_sheet_screen_add_select_title_candidate(cptr title,
    cptr suffix);
void sdl_character_sheet_screen_begin_select_rating_summary(cptr title);
void sdl_character_sheet_screen_add_select_rating(cptr group, cptr stars, int count, int attr, cptr desc);
void sdl_character_sheet_screen_add_select_detail(cptr text, int attr, cptr desc);
void sdl_character_sheet_screen_set_select_detail_size_hint(int stat_rows, int ability_rows, int trait_rows);
void sdl_character_sheet_screen_set_select_ability_rows(int rows);
void sdl_character_sheet_screen_set_select_size_hint(cptr longest_desc);
void sdl_character_sheet_screen_add_select_description_candidate(cptr text);
void sdl_character_sheet_screen_set_select_description(cptr text);
bool sdl_character_sheet_screen_commit_select(int selected_index);
bool sdl_character_sheet_screen_handle_pointer_motion(float x, float y);
bool sdl_character_sheet_screen_handle_pointer_button(float x, float y, int action);
bool sdl_character_sheet_screen_handle_pointer_event( const SDL_Event* ev);
bool sdl_main_menu_pane_button_rect(SDL_FRect* out);
void sdl_main_menu_overlay_scroll_to_highlight(int visible_count);
bool sdl_main_menu_pane_current_rect(SDL_FRect* out);
bool sdl_main_menu_choice_disabled_now(int choice);
void sdl_main_menu_overlay_reset_nav_input(void);
bool sdl_main_menu_overlay_hides_supporting_panes(void);
void sdl_main_menu_overlay_close(void);
void sdl_main_menu_overlay_begin(void);
void sdl_main_menu_overlay_move(int delta);
void sdl_main_menu_overlay_choose(int choice);
bool sdl_main_menu_overlay_handle_direction(int dir);
bool sdl_main_menu_overlay_handle_touch_binding(int binding);
void sdl_main_menu_overlay_flash_touch_slot(int slot);
bool sdl_main_menu_overlay_handle_touch_pane_point(float x, float y, bool activate);
bool sdl_main_menu_overlay_handle_gamepad_axis( const SDL_GamepadAxisEvent* ev);
void sdl_main_menu_pane_render(void);
void sdl_popup_notification_render(void);
int sdl_popup_notification_pending_timeout_ms(Uint64 now_ns);
bool sdl_popup_notification_flush_expired(Uint64 now_ns);
bool sdl_main_menu_pane_hit(float x, float y, SDL_FRect* out_rect);
bool sdl_main_menu_pane_handle_hover_pointer(float x, float y);
bool sdl_main_menu_pane_handle_pointer(float x, float y);
bool sdl_main_menu_button_handle_secondary_pointer(float x, float y);
bool sdl_main_menu_button_handle_long_press_down(float x, float y,
    SDL_FingerID finger_id);
bool sdl_main_menu_button_handle_long_press_motion(float x, float y,
    SDL_FingerID finger_id);
bool sdl_main_menu_button_handle_long_press_up(float x, float y,
    SDL_FingerID finger_id);
void sdl_main_menu_button_cancel_long_press(SDL_FingerID finger_id);
int sdl_main_menu_button_pending_timeout_ms(Uint64 now_ns);
bool sdl_main_menu_button_flush_pending_press(Uint64 now_ns);
int sdl_main_menu_overlay_choice_at(float x, float y, bool* in_panel);
bool sdl_main_menu_overlay_handle_pointer_motion(float x, float y);
bool sdl_main_menu_overlay_handle_pointer_down(float x, float y);
bool sdl_main_menu_overlay_handle_key(const SDL_KeyboardEvent* ev);
bool sdl_main_menu_overlay_handle_gamepad_button( const SDL_GamepadButtonEvent* ev);
bool sdl_main_menu_overlay_handle_event(const SDL_Event* ev);
cptr sdl_depth_menu_partition_label(void);
void sdl_depth_menu_pane_label(char* out, size_t out_sz);
int sdl_depth_menu_pane_font_px(void);
bool sdl_depth_menu_pane_current_rect(SDL_FRect* out);
void sdl_depth_menu_pane_render(void);
int sdl_depth_menu_pane_hit_action(float x, float y, SDL_FRect* out_rect);
bool sdl_depth_menu_pane_handle_hover_pointer(float x, float y);
bool sdl_depth_menu_pane_handle_pointer(float x, float y);
int sdl_touch_pane_yes_no_prompt_font_px(float cell_h, int screen_h);
void sdl_touch_pane_append_ellipsis(char* line, size_t line_size);
bool sdl_narrative_banner_overlay_enabled(void);
bool sdl_narrative_banner_top_center_pane_rect( const struct pane_config* pc, SDL_FRect* out);
int sdl_narrative_banner_top_center_panes_bottom(void);
void sdl_narrative_banner_apply_top_center_avoidance(SDL_Rect* rect, int min_h);
int sdl_narrative_banner_overlay_log_left(void);
void sdl_narrative_banner_apply_overlay_log_avoidance(SDL_Rect* rect);
bool sdl_narrative_banner_base_rect(SDL_Rect* out);
int sdl_narrative_banner_font_px(const SDL_Rect* rect);
float sdl_narrative_banner_line_h(TTF_Font* font, int font_px);
float sdl_narrative_banner_max_text_w(const SDL_Rect* rect, int font_px);
int sdl_narrative_banner_wrap_lines(cptr text, TTF_Font* font, float max_w, char lines[][SDL_NARRATIVE_BANNER_LINE_LEN], int max_lines);
int sdl_narrative_banner_line_count(void);
void sdl_narrative_banner_draw_line(TTF_Font* font, cptr text, SDL_Color color, float left_x, float y, float max_w, float line_h);
void sdl_narrative_banner_render(void);
bool sdl_narrative_banner_handle_pointer(float x, float y);
void sdl_narrative_banner_show(bool line_delay, bool fast_fade);
int sdl_touch_pane_wrap_prompt_lines(cptr text, TTF_Font* font, float max_w, char lines[][SDL_TOUCH_YES_NO_LINE_LEN], int max_lines);
bool sdl_touch_pane_yes_no_prompt_layout(SDL_FRect* panel_rect, SDL_FRect* prompt_rect, SDL_FRect* yes_rect, SDL_FRect* no_rect);
bool sdl_touch_pane_point_to_slot(float x, float y, int* out_slot);
bool sdl_touch_pane_yes_no_prompt_hit(float x, float y, sdl_touch_yes_no_prompt_hover* button);
void sdl_touch_pane_clear_yes_no_prompt(void);
bool sdl_touch_pane_handle_yes_no_prompt_hover(float x, float y);
bool sdl_touch_pane_handle_yes_no_prompt_pointer(float x, float y);
void sdl_touch_pane_draw_arrow(const SDL_FRect* rect, int binding, SDL_Color color);
void sdl_touch_pane_draw_button_text_scaled(const SDL_FRect* rect, const char* name, const char* symbol, SDL_Color color, float single_text_font_ratio, float single_text_height_ratio);
void sdl_touch_pane_draw_button_text_px(const SDL_FRect* rect, const char* name, const char* symbol, SDL_Color color, int name_px, int symbol_px);
void sdl_touch_pane_draw_button_text(const SDL_FRect* rect, const char* name, const char* symbol, SDL_Color color);
void sdl_touch_pane_draw_wrapped_prompt(const SDL_FRect* rect, cptr text, SDL_Color color, int font_px);
void sdl_touch_pane_binding_symbol(int binding, char* buf, size_t buflen);
bool sdl_touch_pane_label_is_symbol_only(const char* label);
bool sdl_touch_pane_should_hide_symbol(const char* name, const char* symbol);
void sdl_touch_pane_render_reset_prompt(void);
void sdl_touch_pane_render_yes_no_prompt(void);
void sdl_touch_pane_default_label_for_panel_slot(int panel, int index, char* buf, size_t buflen);
void sdl_touch_pane_proto_label_for_slot(int index, char* buf, size_t buflen);
void sdl_touch_pane_base_label_for_slot(int panel, int index, char* buf, size_t buflen);
void sdl_touch_pane_display_label_for_slot(int panel, int index, char* buf, size_t buflen);
void sdl_unified_look_prompt_clear(void);
void sdl_unified_look_prompt_begin(int anchor_row);
void sdl_unified_look_prompt_add(int choice, cptr full, cptr medium,
    cptr compact, cptr tiny);
void sdl_unified_look_prompt_finish(void);
void sdl_unified_look_sidebar_clear(void);
void sdl_unified_look_sidebar_begin(bool compact, bool has_selection,
    int selected_choice);
void sdl_unified_look_sidebar_add_header(cptr text);
void sdl_unified_look_sidebar_add_entry(int choice, int entity_type, int y,
    int x, byte symbol_attr, byte text_attr, cptr symbol, cptr text);
void sdl_unified_look_sidebar_finish(void);
void sdl_unified_look_prompt_render(void);
void sdl_unified_look_sidebar_render(void);
bool sdl_unified_look_prompt_contains_point(float x, float y);
bool sdl_unified_look_prompt_handle_pointer(float x, float y, int action);
bool sdl_unified_look_prompt_handle_hover_pointer(float x, float y);
bool sdl_unified_look_sidebar_handle_pointer(float x, float y, int action);
bool sdl_unified_look_sidebar_handle_hover_pointer(float x, float y);
int sdl_main_view_visual_cols(const sdl_view* view);
int sdl_main_view_visual_rows(const sdl_view* view);
bool sdl_term_get_size_hook(term* t, int* w, int* h);
bool sdl_left_panel_pane_rect_for_view(const sdl_view* view, SDL_FRect* out_rect);
void sdl_update_left_panel_pane_rect(void);
void sdl_merge_frect_bounds(const SDL_FRect* rect, bool* have, float* x1, float* y1, float* x2, float* y2);
bool sdl_main_cell_rect(int col, int row, int cols, int rows, SDL_FRect* out);
bool sdl_left_panel_source_cell_rect(int col, int row, int cols, int rows, SDL_FRect* out);
bool sdl_combat_overlay_cell_rect(int col, int source_row, int cols, int rows, SDL_FRect* out);
bool sdl_combat_overlay_point_to_cell(float x, float y, int* out_col, int* out_source_row);
bool sdl_main_view_point_to_cell(float x, float y, int* out_col, int* out_row);
bool sdl_mouse_gameplay_context_active(void);
bool sdl_main_view_point_to_map(float x, float y, int* out_y, int* out_x);
bool sdl_main_view_point_hits_outer_cell(float x, float y);
bool sdl_main_view_point_to_look_map(float x, float y, int* out_y, int* out_x);
void sdl_unified_look_set_map_hover_enabled(bool enabled);
void sdl_unified_look_set_active(bool active);
bool sdl_unified_look_pointer_input_active(void);
void sdl_unified_look_cancel_map_drag(void);
void sdl_unified_look_clear_map_hover(void);
bool sdl_unified_look_take_map_hover(int* y, int* x);
bool sdl_unified_look_take_map_describe(int* y, int* x);
bool sdl_unified_look_take_map_target(int* y, int* x);
bool sdl_unified_look_take_map_pan(int* dy, int* dx);
bool sdl_unified_look_take_main_zoom(int* scale);
bool sdl_unified_look_point_to_drag_map(float x, float y);
void sdl_unified_look_queue_map_pan(int dy, int dx);
int sdl_unified_look_effective_zoom_scale(void);
int sdl_unified_look_clamp_zoom_scale(int scale);
void sdl_unified_look_queue_main_zoom_target(int scale);
void sdl_unified_look_queue_main_zoom_delta(int delta);
float sdl_unified_look_pinch_distance(void);
int sdl_unified_look_zoom_delta_for_pinch_ratio(float ratio);
void sdl_unified_look_update_map_pinch(void);
bool sdl_unified_look_begin_map_pinch(float x, float y, SDL_FingerID finger_id);
void sdl_wheel_step_state_reset(sdl_wheel_step_state* state);
void sdl_wheel_step_state_prepare(sdl_wheel_step_state* state, const SDL_MouseWheelEvent* wheel);
bool sdl_wheel_axis_value_looks_smooth(float value);
void sdl_wheel_clamp_accum(float* accum, float step_units);
int sdl_wheel_consume_axis_value(sdl_wheel_step_state* state, const SDL_MouseWheelEvent* wheel, float* accum, float value, bool* smooth_axis, Uint64* last_step_timestamp);
int sdl_wheel_step_state_consume_axis(sdl_wheel_step_state* state, const SDL_MouseWheelEvent* wheel, bool vertical);
int sdl_wheel_step_state_consume_primary_axis( sdl_wheel_step_state* state, const SDL_MouseWheelEvent* wheel);
bool sdl_unified_look_handle_map_zoom_wheel( const SDL_MouseWheelEvent* wheel);
int sdl_main_screen_clamp_main_view_scale(int scale);
bool sdl_main_screen_set_main_view_scale_target(int scale);
bool sdl_main_screen_adjust_main_view_scale(int delta);
bool sdl_main_map_point_to_drag_map(float x, float y);
bool sdl_main_map_apply_pan(int pan_dy, int pan_dx);
void sdl_main_map_cancel_drag(void);
void sdl_main_map_mark_dragged(void);
float sdl_main_map_pinch_distance(void);
int sdl_main_map_zoom_delta_for_pinch_ratio(float ratio);
void sdl_main_map_update_pinch(void);
bool sdl_main_map_begin_pinch(float x, float y, SDL_FingerID finger_id);
bool sdl_main_map_handle_drag_down(float x, float y, bool mouse, SDL_FingerID finger_id);
bool sdl_main_map_handle_drag_motion(float x, float y, bool mouse, SDL_FingerID finger_id);
bool sdl_main_map_handle_mouse_release_click(float x, float y);
bool sdl_main_map_handle_drag_up(float x, float y, bool mouse, SDL_FingerID finger_id);
bool sdl_main_map_handle_zoom_wheel(const SDL_MouseWheelEvent* wheel);
void sdl_main_map_flush_pending_pan(void);
bool sdl_unified_look_handle_map_drag_down(float x, float y, bool mouse, SDL_FingerID finger_id);
bool sdl_unified_look_handle_map_drag_motion(float x, float y, bool mouse, SDL_FingerID finger_id);
bool sdl_unified_look_handle_map_drag_up(float x, float y, bool mouse, SDL_FingerID finger_id);
bool sdl_unified_look_handle_map_hover_pointer(float x, float y);
bool sdl_mouse_path_grid_is_open_floor(int y, int x);
bool sdl_mouse_path_grid_known(int y, int x);
bool sdl_mouse_feature_known_for_action(int y, int x);
void sdl_mouse_note_feature_for_action(int y, int x);
bool sdl_mouse_path_grid_is_known_danger(int y, int x);
bool sdl_mouse_path_grid_is_stuck_door(int y, int x);
bool sdl_mouse_stuck_door_bash_target(int map_y, int map_x, int* out_dir);
int sdl_mouse_path_blocked_target_kind(int y, int x);
bool sdl_mouse_path_grid_walkable(int y, int x);
int sdl_mouse_path_heuristic(int y1, int x1, int y2, int x2);
void sdl_mouse_path_order_dirs(int y, int x, int target_y, int target_x, int dirs[8]);
bool sdl_mouse_path_is_move_dir(int dir);
int sdl_mouse_path_route_dir_index(int dir);
byte sdl_mouse_path_route_state(int dir, int chain);
int sdl_mouse_path_route_state_dir(byte state);
int sdl_mouse_path_route_state_chain(byte state);
bool sdl_mouse_path_dirs_sprint_compatible(int newer_dir, int older_dir);
bool sdl_mouse_path_grid_is_safe_leap_landing(int y, int x);
bool sdl_mouse_path_state_has_run_up(int y, int x, int dir, byte state);
bool sdl_mouse_path_can_step_into_chasm(int y, int x, int dir, byte state);
byte sdl_mouse_path_initial_route_state(bool sprint_enabled);
byte sdl_mouse_path_next_route_state(byte state, int dir, bool sprint_enabled);
int sdl_mouse_path_route_edge_cost(byte state, int dir, bool sprint_enabled);
int sdl_mouse_path_route_heuristic(int y, int x, int target_y, int target_x, bool sprint_enabled);
size_t sdl_mouse_path_search_index(int y, int x, byte state);
size_t sdl_mouse_path_search_grid_state_index(u16b grid, byte state);
void sdl_mouse_path_search_free(void);
bool sdl_mouse_path_search_ensure(size_t state_count);
bool sdl_mouse_path_heap_less(int a, int b);
void sdl_mouse_path_heap_swap(int a, int b);
void sdl_mouse_path_heap_sift_up(int pos);
void sdl_mouse_path_heap_sift_down(int pos);
bool sdl_mouse_path_heap_insert_or_decrease(u16b grid, byte state, int priority);
bool sdl_mouse_path_heap_pop(u16b* grid, byte* state);
bool sdl_mouse_path_build_from_search(int target_y, int target_x, byte target_state);
bool sdl_mouse_path_compute_route(int target_y, int target_x, bool sprint_enabled);
bool sdl_mouse_path_compute_blocked_target_route(int target_y, int target_x, bool sprint_enabled);
bool sdl_mouse_path_compute(int target_y, int target_x);
bool sdl_mouse_path_select_grid(int map_y, int map_x, bool report_failure);
bool sdl_mouse_path_start_follow_grid(int map_y, int map_x);
void sdl_mouse_path_cancel(void);
bool sdl_mouse_stuck_door_bash_queue_prompt(int map_y, int map_x);
bool sdl_mouse_stuck_door_handle_left_click(int map_y, int map_x);
int sdl_mouse_movement_normalized_mode(int mode);
void sdl_mouse_path_handle_motion(float x, float y);
bool sdl_mouse_path_handle_movement_click(float x, float y);
bool sdl_mouse_path_handle_left_click(float x, float y);
bool sdl_mouse_feature_action_for_grid(int map_y, int map_x, int* out_command, int* out_dir);
bool sdl_mouse_feature_action_queue_grid(int map_y, int map_x);
bool sdl_mouse_path_handle_right_movement_click(float x, float y);
bool sdl_mouse_path_handle_right_click(float x, float y);
bool sdl_mouse_consume_wake_key(void);
bool sdl_pointer_attack_mode_is_ranged(int mode);
bool sdl_pointer_attack_manual_modifier_active(void);
bool sdl_pointer_attack_input_context_active(void);
bool sdl_pointer_attack_mode_active(void);
void sdl_pointer_attack_clear_hover(void);
void sdl_pointer_attack_clear_touch_selection(void);
void sdl_pointer_attack_cancel_touch_press(void);
void sdl_pointer_attack_clear_pending(void);
int sdl_pointer_attack_binding_mode(int binding);
bool sdl_pointer_attack_binding_toggled(int binding);
int sdl_pointer_attack_current_mode(void);
int sdl_pointer_attack_panel_display_mode(void);
bool sdl_pointer_attack_panel_mode_highlighted(int mode);
bool sdl_pointer_attack_panel_quiver_highlighted(int mode);
const char* sdl_pointer_attack_mode_name(int mode);
void sdl_pointer_attack_refresh_mode_display(bool redraw_map);
void sdl_pointer_attack_reset_to_melee(void);
void sdl_pointer_attack_set_mode(int mode);
void sdl_pointer_attack_activate_panel_choice(int mode, bool quiver_only);
void sdl_pointer_attack_set_panel_hover_mode(int mode);
bool sdl_pointer_attack_toggle_binding(int binding);
int sdl_pointer_attack_ranged_range(int mode);
bool sdl_pointer_attack_adjacent_dir_for_grid(int y, int x, int* out_dir);
bool sdl_pointer_attack_adjacent_tunnel_target(int y, int x, int* out_dir);
bool sdl_pointer_attack_manual_location_valid(int mode, int y, int x);
int sdl_pointer_attack_target_kind(int mode, bool manual, int y, int x, int* out_m_idx);
bool sdl_pointer_attack_target_hoverable(int mode, bool manual, int y, int x, int* out_m_idx, int* out_kind);
bool sdl_pointer_attack_target_actionable(int mode, bool manual, int y, int x, int* out_m_idx);
cptr sdl_pointer_attack_blocked_message(int mode, int kind);
bool sdl_pointer_attack_update_hover_grid(int map_y, int map_x, bool manual);
bool sdl_pointer_attack_queue_target(int mode, bool manual, int map_y, int map_x);
bool sdl_pointer_attack_handle_motion(float x, float y);
bool sdl_pointer_attack_handle_left_click(float x, float y);
bool sdl_pointer_attack_touch_same_selected_target(int mode, bool manual, int map_y, int map_x);
bool sdl_pointer_attack_handle_touch_down(float x, float y, SDL_FingerID finger_id);
bool sdl_pointer_attack_handle_touch_motion(float x, float y, SDL_FingerID finger_id);
bool sdl_pointer_attack_handle_touch_up(float x, float y, SDL_FingerID finger_id);
int sdl_pointer_attack_pending_timeout_ms(Uint64 now_ns);
bool sdl_pointer_attack_flush_pending_press(Uint64 now_ns);
bool sdl_pointer_attack_take_render_target(int* mode, bool* manual, int* kind, int* y, int* x, int* m_idx, bool* actionable);
void sdl_pointer_attack_render_cell(int y, int x, SDL_Color color, bool target);
void sdl_pointer_attack_render_manual_ranged_overlay(int mode, SDL_Color color);
void sdl_pointer_attack_render(void);
bool sdl_pointer_attack_take_command(int* command, int* dir);
void sdl_pointer_aim_cancel_touch_press(void);
void sdl_pointer_aim_clear_hover(void);
void sdl_pointer_aim_clear_touch_selection(void);
bool sdl_pointer_aim_point_to_map(float x, float y, int* out_y, int* out_x);
bool sdl_pointer_aim_in_range(int y, int x);
bool sdl_pointer_aim_location_targetable(int y, int x);
bool sdl_pointer_aim_resolve_grid(int map_y, int map_x, int* out_dir, bool* out_exact_target);
bool sdl_pointer_aim_queue_grid(int map_y, int map_x);
void sdl_pointer_aim_begin(int range, bool allow_vertical);
void sdl_pointer_aim_end(void);
bool sdl_pointer_aim_take_direction(int* dir);
void sdl_pointer_aim_select_begin(int range, bool allow_vertical);
void sdl_pointer_aim_select_end(void);
void sdl_pointer_aim_select_set_manual(bool manual);
void sdl_pointer_aim_select_set_location(bool location);
void sdl_pointer_aim_select_update(int y, int x);
bool sdl_pointer_aim_select_take_event(int* kind, int* y, int* x);
void sdl_pointer_aim_select_set_choices(const int* ys, const int* xs,
    int count, cptr prompt);
bool sdl_pointer_aim_update_hover_grid(int map_y, int map_x);
bool sdl_pointer_aim_handle_motion(float x, float y);
bool sdl_pointer_aim_handle_left_click(float x, float y);
bool sdl_pointer_aim_handle_left_release(float x, float y);
bool sdl_pointer_aim_handle_touch_down(float x, float y, SDL_FingerID finger_id);
bool sdl_pointer_aim_handle_touch_motion(float x, float y, SDL_FingerID finger_id);
bool sdl_pointer_aim_handle_touch_up(float x, float y, SDL_FingerID finger_id);
bool sdl_pointer_aim_take_render_target(int* y, int* x);
void sdl_pointer_aim_render_center_path(int dir, int target_y, int target_x, SDL_Color path_color, SDL_Color target_color);
void sdl_pointer_aim_render_cone(int dir, int target_y, int target_x, SDL_Color path_color, SDL_Color target_color);
void sdl_pointer_aim_render(void);
bool sdl_mouse_stuck_door_bash_take_command(int* command, int* dir);
bool sdl_mouse_path_has_pending_key(void);
bool sdl_mouse_path_take_step_command(int* command, int* dir);
bool sdl_mouse_grid_has_visible_monster(int y, int x, int* out_m_idx);
bool sdl_mouse_monster_is_friendly(int m_idx);
bool sdl_mouse_grid_has_marked_object(int y, int x, object_type** out_obj);
void sdl_object_tooltip_clear(void);
void sdl_object_tooltip_begin_persistent(void);
void sdl_object_tooltip_end_persistent(void);
bool sdl_object_tooltip_dismiss_persistent_on_press(void);
void sdl_object_tooltip_append_part(char* buf, size_t buflen, byte* attrs, cptr text, byte attr);
bool sdl_object_tooltip_feature_name(int y, int x, cptr* out_name);
void sdl_object_tooltip_monster_morale_text(monster_type* m_ptr, char* out, size_t out_len, byte* out_attr);
bool sdl_object_tooltip_format_grid(int y, int x, char* out, size_t out_len, byte* attrs);
bool sdl_object_tooltip_show_grid(int map_y, int map_x, bool touch);
bool sdl_object_tooltip_show_text_at_cell_ex(int col, int row, int cols, cptr text, bool touch, bool character_panel_cell);
bool sdl_object_tooltip_show_text_at_cell(int col, int row, int cols, cptr text, bool touch);
bool sdl_object_tooltip_show_character_panel_text_at_cell(int col, int row, int cols, cptr text, bool touch);
bool sdl_object_tooltip_show_text_at_rect(const SDL_FRect* rect, cptr text, bool touch);
bool sdl_hover_tooltip_show_text(int col, int row, int cols, cptr text, bool touch);
void sdl_hover_tooltip_clear(void);
int sdl_object_tooltip_font_px(void);
bool sdl_object_tooltip_pointer_hits_term_cell(float x, float y);
void sdl_object_tooltip_handle_mouse_motion(float x, float y);
int sdl_object_tooltip_pending_timeout_ms(Uint64 now_ns);
bool sdl_object_tooltip_flush_expired(Uint64 now_ns);
void sdl_object_tooltip_render(void);
int sdl_description_overlay_font_px(void);
int sdl_description_overlay_max_cols(void);
int sdl_description_overlay_capture_cols(int terminal_cols,
    bool interactive);
int sdl_description_overlay_visible_cols(void);
SDL_Color sdl_description_overlay_attr_color(byte attr);
cptr sdl_description_overlay_footer_text( const description_overlay_state* overlay);
void sdl_description_overlay_set_footer(cptr text, bool always);
void sdl_description_overlay_clear_footer_actions(void);
void sdl_description_overlay_add_footer_action(int key, cptr token);
bool sdl_description_overlay_has_footer_action(int key);
bool sdl_description_overlay_footer_action_label(int key, char* buf,
    size_t buflen);
void sdl_description_overlay_clear_avoid(void);
void sdl_description_overlay_set_avoid_term_rect(int col, int row, int wid, int hgt);
bool sdl_description_overlay_token_matches_hover( const description_overlay_state* overlay, cptr text, int col);
bool sdl_description_overlay_avoid_rect(SDL_FRect* out);
bool sdl_description_overlay_rects_intersect( const SDL_FRect* a, const SDL_FRect* b);
int sdl_description_overlay_rows_for_panel_space(float available_h, int pad_y, int cell_h, bool footer, int header_rows);
bool sdl_description_overlay_fit_around_avoid( const SDL_Rect* anchor, int margin, int pad_y, int cell_h, bool footer, int header_rows, float panel_x, float panel_w, int* visible_rows, float* panel_h, float* panel_y);
bool sdl_description_overlay_layout(description_overlay_layout* out);
bool sdl_description_overlay_scroll_to_layout( const description_overlay_layout* layout, int scroll);
bool sdl_description_overlay_scroll_by(int rows);
bool sdl_description_overlay_scroll_page(int direction);
bool sdl_description_overlay_handle_mouse_wheel( const SDL_MouseWheelEvent* wheel);
bool sdl_description_overlay_contains_point(float x, float y);
void sdl_description_overlay_touch_scroll_cancel(void);
bool sdl_description_overlay_touch_scroll_handle_pointer_down(float x,
    float y, SDL_FingerID finger_id);
bool sdl_description_overlay_touch_scroll_handle_pointer_motion(float x,
    float y, SDL_FingerID finger_id);
bool sdl_description_overlay_touch_scroll_handle_pointer_up(
    SDL_FingerID finger_id);
void sdl_description_overlay_render_char(SDL_Texture* atlas, int atlas_cell_w, int atlas_cell_h, float cell_w, float cell_h, float x, float y, byte attr, char ch);
void sdl_description_overlay_render_text(SDL_Texture* atlas, int atlas_cell_w, int atlas_cell_h, const char* text, float x, float y, float cell_w, float cell_h, byte attr);
void sdl_description_overlay_render(void);
void sdl_touch_exit_button_render(void);
bool sdl_touch_exit_button_handle_pointer(float x, float y);
int sdl_touch_menu_button_reserved_rows(void);
bool sdl_description_overlay_handle_close_hover(float x, float y);
bool sdl_description_overlay_handle_close_pointer(float x, float y);
int sdl_description_overlay_footer_action_at(float x, float y);
bool sdl_description_overlay_handle_footer_hover(float x, float y);
bool sdl_description_overlay_handle_footer_pointer(float x, float y);
bool sdl_unified_look_handle_map_describe_pointer(float x, float y);
bool sdl_unified_look_handle_map_target_pointer(float x, float y);
bool sdl_mouse_grid_has_marked_searched_skeleton(int y, int x, object_type** out_obj);
bool sdl_mouse_grid_has_recallable_content(int y, int x);
bool sdl_mouse_grid_has_describable_content(int y, int x);
void sdl_mouse_recall_object(object_type* o_ptr);
bool sdl_mouse_recall_handle_right_click(float x, float y);
bool sdl_mouse_recall_handle_right_click_if_available(float x, float y);
void sdl_map_touch_cancel_press(void);
bool sdl_map_touch_is_same_selected_target(int map_y, int map_x);
bool sdl_map_touch_handle_pointer_down(float x, float y, SDL_FingerID finger_id);
bool sdl_map_touch_handle_pointer_motion(float x, float y, SDL_FingerID finger_id);
bool sdl_map_touch_handle_pointer_up(float x, float y, SDL_FingerID finger_id);
int sdl_map_touch_pending_timeout_ms(Uint64 now_ns);
bool sdl_map_touch_flush_pending_press(Uint64 now_ns);
bool sdl_mouse_recall_process_pending(void);
void sdl_mouse_path_render(void);
bool sdl_player_map_rect(int y, int x, SDL_FRect* out_rect);
bool sdl_point_in_frect(const SDL_FRect* rect, float x, float y);
void sdl_player_confirm_at_player(void);
bool sdl_main_view_point_is_player_grid(float x, float y);
bool sdl_player_has_equipped_staff(void);
bool sdl_player_has_equipped_horn(void);
bool sdl_player_has_singable_song(void);
void sdl_player_action_menu_add_entry(player_action_menu_entry* entries, int* count, int kind, int command, cptr label);
cptr sdl_player_action_menu_fallback_for_kind(int kind);
void sdl_player_action_menu_tile_for_kind(int kind, byte* out_attr, char* out_char);
int sdl_player_action_menu_collect(player_action_menu_entry* entries);
int sdl_player_action_menu_collect_secondary(int primary_kind, player_action_menu_entry* entries);
int sdl_player_action_menu_secondary_owner(int kind);
bool sdl_player_has_floor_item_underfoot(void);
bool sdl_player_action_menu_kind_supports_secondary(int kind);
int sdl_player_action_menu_default_kind(void);
int sdl_player_action_menu_hover_index( player_action_menu_entry* entries, int count);
void sdl_player_action_menu_select_default(void);
void sdl_player_action_menu_move_hover(int delta);
void sdl_player_action_menu_move_hover_vertical(int delta);
void sdl_player_action_menu_activate_hover(void);
void sdl_player_action_menu_start_gamepad_press(int button, int kind);
bool sdl_player_action_menu_handle_gamepad_confirm(int button, bool down);
void sdl_player_action_menu_slot_offset(int slot, int count, float* out_x, float* out_y);
bool sdl_player_action_menu_layout(player_action_menu_entry* entries, int* out_count);
int sdl_player_action_menu_kind_at(float x, float y);
void sdl_player_action_menu_cancel_press(void);
void sdl_player_action_menu_cancel(void);
bool sdl_player_exchange_target_valid(int y, int x, int* out_m_idx);
bool sdl_player_exchange_has_any_target(void);
int sdl_player_exchange_collect_targets( player_exchange_target_entry* entries, int max_entries);
int sdl_player_exchange_target_index( const player_exchange_target_entry* entries, int count, int y, int x, int m_idx);
void sdl_player_exchange_set_hover( const player_exchange_target_entry* entry);
void sdl_player_exchange_select_default(void);
void sdl_player_exchange_cancel_press(void);
void sdl_player_exchange_cancel(void);
bool sdl_player_exchange_context_active(void);
bool sdl_player_exchange_begin(bool report_no_target);
void sdl_player_exchange_begin_direction_prompt(void);
void sdl_player_exchange_cancel_direction_prompt(void);
void sdl_player_exchange_update_hover(float x, float y);
bool sdl_player_exchange_same_selection(int y, int x, int m_idx);
void sdl_player_exchange_execute(int y, int x);
void sdl_player_exchange_select_or_execute(int y, int x, int m_idx);
void sdl_player_exchange_move_hover(int delta);
void sdl_player_exchange_activate_hover(void);
void sdl_player_action_menu_activate_kind(int kind, bool secondary);
bool sdl_player_action_menu_open(void);
bool sdl_player_action_menu_handle_gamepad_button( SDL_GamepadButton button, bool down);
bool sdl_player_exchange_handle_gamepad_button( SDL_GamepadButton button, bool down);
bool sdl_player_action_menu_handle_pointer_down(float x, float y, SDL_FingerID finger_id, bool mouse, bool secondary);
bool sdl_player_action_menu_handle_pointer_motion(float x, float y, SDL_FingerID finger_id, bool mouse);
bool sdl_player_action_menu_handle_pointer_up(float x, float y, SDL_FingerID finger_id, bool mouse, bool secondary);
int sdl_player_action_menu_pending_timeout_ms(Uint64 now_ns);
bool sdl_player_action_menu_flush_pending_press(Uint64 now_ns);
bool sdl_player_exchange_handle_pointer_down(float x, float y, SDL_FingerID finger_id, bool mouse);
bool sdl_player_exchange_handle_pointer_motion(float x, float y, SDL_FingerID finger_id, bool mouse);
bool sdl_player_exchange_handle_pointer_up(float x, float y, SDL_FingerID finger_id, bool mouse);
void sdl_player_exchange_render_cell(int y, int x, SDL_Color color, int alpha, bool thick);
void sdl_player_exchange_render(void);
void sdl_player_action_menu_render(void);
bool sdl_point_in_rect(const SDL_Rect* rect, float x, float y);
bool sdl_left_panel_pane_hit(float x, float y);
void sdl_left_panel_pane_set_expanded(bool expanded);
bool sdl_point_in_view_rect(enum pane_type pane, float x, float y);
bool sdl_menu_pointer_hits_non_main_pane(float x, float y);
bool sdl_main_screen_point_over_overlay_pane(float x, float y);
bool sdl_pane_command_shortcuts_active(void);
bool sdl_main_screen_click_shortcuts_active(void);
bool sdl_main_screen_handle_menu_text_pointer(float x, float y, int action);
bool sdl_main_screen_menu_pointer_hits_cell(float x, float y);
bool sdl_main_screen_handle_menu_outside_pointer(float x, float y, bool primary);
bool sdl_main_screen_menu_outside_armor_cycle_pointer(int col, int row);
bool sdl_main_screen_handle_menu_hover_pointer(float x, float y);
bool sdl_status_line_partition_label_at_col(int row, int col);
bool sdl_status_line_song_label_at_col(int row, int col);
bool sdl_status_line_depth_label_at_col(int row, int col);
bool sdl_status_line_view_label_at_col(int row, int col);
bool sdl_status_line_touch_zone_selected(int action, int col, int width);
bool sdl_character_panel_touch_zone_selected(int action, int row);
void sdl_main_screen_touch_zone_selection_set(int status_action, int status_col, int panel_action, int panel_row, bool redraw);
int sdl_status_line_click_action_at_cell(int col, int row);
bool sdl_main_screen_handle_status_line_hover_pointer(float x, float y);
bool sdl_status_line_action_is_corner_exempt(int action);
bool sdl_handle_status_line_click_action(int action);
bool sdl_main_screen_handle_corner_exempt_status_pointer(float x, float y);
bool sdl_main_screen_handle_status_line_pointer(float x, float y);
bool sdl_screen_row_contains_ci(const term* t, int row, cptr needle);
unsigned char sdl_screen_char_at(const term* t, int row, int col);
bool sdl_screen_segment_col_hits_ci(const term* t, int row, int start_col, int width, int hit_col, cptr needle);
bool sdl_screen_shows_any_key_prompt(void);
bool sdl_screen_shows_welcome_screen(void);
bool sdl_welcome_screen_handle_gamepad_button(SDL_GamepadButton button, bool down);
bool sdl_pointer_activate_welcome_screen(void);
bool sdl_welcome_screen_handle_pointer_motion(float x, float y);
bool sdl_pointer_activate_welcome_screen_at(float x, float y);
void sdl_welcome_touch_cancel_press(void);
bool sdl_welcome_touch_handle_pointer_down(float x, float y, SDL_FingerID finger_id);
bool sdl_welcome_touch_handle_pointer_motion(float x, float y, SDL_FingerID finger_id);
bool sdl_welcome_touch_handle_pointer_up(float x, float y, SDL_FingerID finger_id);
bool sdl_pointer_dismiss_any_key_prompt(void);
void sdl_menu_touch_cancel(void);
bool sdl_menu_touch_handle_pointer_down(float x, float y, SDL_FingerID finger_id, bool mouse);
bool sdl_menu_touch_handle_pointer_motion(float x, float y, SDL_FingerID finger_id, bool mouse);
bool sdl_menu_touch_handle_pointer_up(float x, float y, SDL_FingerID finger_id, bool mouse);
int sdl_menu_touch_pending_timeout_ms(Uint64 now_ns);
bool sdl_menu_touch_flush_pending_press(Uint64 now_ns);
bool sdl_screen_back_gesture_active(void);
void sdl_screen_back_touch_cancel(void);
void sdl_screen_back_gesture_begin(void);
void sdl_screen_back_gesture_end(void);
void sdl_screen_back_gesture_cancel_touch_inputs(void);
bool sdl_screen_back_gesture_handle_event(const SDL_Event* ev);
int sdl_screen_back_gesture_pending_timeout_ms(Uint64 now_ns);
bool sdl_screen_back_gesture_flush_pending_press(Uint64 now_ns);
void sdl_menu_scroll_send_key_repeated(int key, int count);
void sdl_menu_scroll_send_lines(int lines);
bool sdl_menu_scroll_handle_mouse_wheel(const SDL_MouseWheelEvent* wheel);
bool sdl_menu_scroll_handle_mouse_button(float x, float y);
void sdl_menu_scroll_cancel(void);
int sdl_minimap_clamp_zoom_step(int step);
float sdl_minimap_zoom_factor_for_step(int step);
float sdl_minimap_zoom_factor(void);
int sdl_minimap_zoom_step_for_factor(float factor);
int sdl_minimap_default_zoom_step(float fit_scale, const sdl_view* d);
float sdl_minimap_clampf(float value, float min_value, float max_value);
bool sdl_minimap_point_in_rect(float x, float y, const SDL_FRect* rect);
bool sdl_minimap_point_in_rect_expanded(float x, float y, const SDL_FRect* rect, float pad);
bool sdl_minimap_window_to_canvas_point(float x, float y, float* out_x, float* out_y);
bool sdl_minimap_focus_point_valid(int y, int x);
bool sdl_minimap_hint_source_in_bounds(const hint_message_meta* meta);
bool sdl_minimap_has_hint_source_at(int y, int x);
bool sdl_minimap_grid_opened(int y, int x);
void sdl_minimap_focus(int y, int x);
void sdl_minimap_clear_gamepad_modal_state(void);
void sdl_minimap_clear_touches(void);
void sdl_minimap_clear_map_layout(void);
void sdl_minimap_store_map_layout(const SDL_FRect* map_rect, int min_y, int min_x, int max_y, int max_x);
void sdl_minimap_begin(void);
void sdl_minimap_end(void);
void sdl_minimap_map_texture_cache_clear(void);
void sdl_minimap_flush_pending_redraw(void);
bool sdl_minimap_redraw(void);
bool sdl_minimap_set_zoom_step(int step);
bool sdl_minimap_adjust_zoom(int delta);
bool sdl_minimap_offset_by(float dx, float dy);
bool sdl_minimap_pan(int dx, int dy);
void sdl_minimap_cancel_drag(void);
void sdl_minimap_begin_drag(bool mouse, SDL_FingerID finger_id, float x, float y);
float sdl_minimap_tap_threshold_px(void);
bool sdl_minimap_finger_moved_from_start( const minimap_touch_finger* finger, float x, float y);
bool sdl_minimap_drag_to(bool mouse, SDL_FingerID finger_id, float x, float y);
void sdl_minimap_close(void);
bool sdl_minimap_handle_control_point(float x, float y);
bool sdl_minimap_hint_source_marker_rect(const hint_message_meta* meta, const SDL_FRect* map_dst, int min_y, int min_x, int max_y, int max_x, float min_marker, SDL_FRect* out_marker);
bool sdl_minimap_grid_at_canvas_point(float x, float y, int* out_y, int* out_x);
bool sdl_minimap_hint_source_at_canvas_point(float x, float y, int* out_index, int* out_y, int* out_x);
bool sdl_minimap_focus_hint_source_at_canvas_point(float x, float y);
bool sdl_minimap_queue_hint_source_at_canvas_point(float x, float y);
bool sdl_minimap_take_hint_click(int* out_index);
int sdl_minimap_find_finger(SDL_FingerID finger_id);
int sdl_minimap_active_finger_count(void);
bool sdl_minimap_first_two_fingers(int* out_a, int* out_b);
float sdl_minimap_finger_distance(int a, int b);
void sdl_minimap_start_pinch_if_possible(void);
void sdl_minimap_start_drag_from_first_finger(void);
int sdl_minimap_zoom_delta_for_pinch_ratio(float ratio);
bool sdl_minimap_update_pinch(void);
void sdl_minimap_add_or_update_finger(SDL_FingerID finger_id, float x, float y);
void sdl_minimap_remove_finger(SDL_FingerID finger_id);
bool sdl_minimap_handle_touch_down(float x, float y, SDL_FingerID finger_id);
bool sdl_minimap_handle_touch_motion(float x, float y, SDL_FingerID finger_id);
bool sdl_minimap_handle_touch_up(float x, float y, SDL_FingerID finger_id);
bool sdl_minimap_handle_mouse_wheel(const SDL_MouseWheelEvent* wheel);
bool sdl_minimap_handle_mouse_button(const SDL_MouseButtonEvent* button);
bool sdl_minimap_handle_key(const SDL_KeyboardEvent* key_event);
bool sdl_minimap_handle_event(const SDL_Event* ev);
bool sdl_minimap_handle_gamepad_button(SDL_GamepadButton button, bool down);
bool sdl_minimap_handle_gamepad_axis(const SDL_GamepadAxisEvent* ev);
void sdl_minimap_layout_controls(const sdl_view* d, int canvas_w, int canvas_h);
void sdl_minimap_draw_button_symbol(const SDL_FRect* rect, int symbol, SDL_Color color);
void sdl_minimap_draw_button(const SDL_FRect* rect, bool enabled, int symbol);
void sdl_minimap_draw_controls(sdl_view* d, int canvas_w, int canvas_h);
void sdl_minimap_draw_prompt(sdl_view* d, int canvas_w, int canvas_h);
bool sdl_menu_scroll_handle_pointer_down(float x, float y, SDL_FingerID finger_id);
bool sdl_menu_scroll_handle_pointer_motion(float x, float y, SDL_FingerID finger_id);
bool sdl_menu_scroll_handle_pointer_up(SDL_FingerID finger_id);
bool sdl_main_screen_cell_hits_character_panel(int col, int row);
int sdl_hidden_left_panel_attack_mode_at_cell(int col, int row);
bool sdl_hidden_left_panel_attack_is_quiver_at_cell(int col, int row);
int sdl_left_panel_quiver_attack_mode_at_col(int col);
int sdl_visible_character_panel_attack_mode_at_cell(int col, int row);
void sdl_enqueue_bypassed_command(int command);
int sdl_hidden_left_panel_click_action_at_cell(int col, int row);
int sdl_visible_character_panel_click_action_at_cell(int col, int row);
int sdl_visible_character_panel_block_top_row(int click_action);
cptr sdl_character_panel_click_tooltip_text(int click_action);
cptr sdl_character_panel_attack_tooltip_text(int attack_mode,
    bool quiver_only);
void sdl_character_panel_tooltip_span(int col, int row, int attack_mode, int click_action, int* out_col, int* out_cols);
void sdl_character_panel_show_hover_tooltip(int col, int row, int attack_mode,
    bool quiver_only, int click_action, bool touch);
bool sdl_handle_character_panel_click_action(int click_action);
bool sdl_main_screen_handle_character_panel_hover_pointer(float x, float y);
bool sdl_binding_opens_pane_menu(int binding);
bool sdl_main_screen_handle_character_panel_pointer(float x, float y);
bool sdl_main_screen_show_character_panel_popup(float x, float y, bool touch);
bool sdl_main_screen_handle_character_panel_secondary_pointer(float x, float y);
bool sdl_main_screen_character_panel_pointer_hit(float x, float y);
void sdl_character_panel_cancel_press(void);
bool sdl_character_panel_press_matches(bool mouse, SDL_FingerID finger_id);
bool sdl_character_panel_handle_pointer_down(float x, float y, bool mouse, SDL_FingerID finger_id);
bool sdl_character_panel_handle_pointer_motion(float x, float y, bool mouse, SDL_FingerID finger_id);
bool sdl_character_panel_handle_pointer_up(float x, float y, bool mouse, SDL_FingerID finger_id);
int sdl_character_panel_pending_timeout_ms(Uint64 now_ns);
bool sdl_character_panel_flush_pending_press(Uint64 now_ns);
bool sdl_main_screen_handle_supporting_pane_pointer(float x, float y);
bool sdl_pane_layout_group_enabled(enum pane_placement where);
bool sdl_pane_layout_config_draggable(int index);
int sdl_pane_layout_config_at(float x, float y);
float sdl_pane_layout_drag_threshold_px(void);
bool sdl_pane_layout_move_config(int from, int insert_before);
bool sdl_pane_layout_drag_reorder_at(float x, float y);
void sdl_pane_layout_drag_send_click(enum pane_type pane);
void sdl_pane_layout_drag_cancel(void);
bool sdl_pane_layout_drag_handle_pointer_down(float x, float y);
bool sdl_pane_layout_drag_handle_pointer_motion(float x, float y);
bool sdl_pane_layout_drag_handle_pointer_up(float x, float y);
bool sdl_log_history_filter_is_valid(int filter);
void sdl_log_pane_sync_display_filter_from_config(void);
bool sdl_log_pane_is_filterable(enum pane_type pane);
int sdl_log_pane_display_filter(int pane);
void sdl_log_pane_apply_display_filter(enum pane_type pane, int filter);
void sdl_log_pane_queue_display_filter(enum pane_type pane, int filter);
bool sdl_log_pane_display_process_pending(void);
bool sdl_log_pane_menu_point_to_log_pane(float x, float y, enum pane_type* out_pane);
int sdl_log_pane_config_index(enum pane_type pane);
int sdl_log_pane_current_rows(enum pane_type pane);
void sdl_log_pane_set_rows(enum pane_type pane, int rows);
void sdl_log_pane_menu_add_entry(log_pane_menu_entry* entries, int* count, log_pane_menu_action action, int filter, int row_delta, cptr label, cptr hint);
int sdl_log_pane_menu_collect(enum pane_type pane, log_pane_menu_entry* entries);
int sdl_log_pane_menu_font_px(enum pane_type pane);
bool sdl_log_pane_menu_layout(log_pane_menu_entry* entries, int* out_count, SDL_FRect* out_panel);
int sdl_log_pane_menu_index_at(float x, float y);
void sdl_log_pane_menu_clear_long_press(void);
void sdl_log_pane_menu_cancel(void);
bool sdl_log_pane_menu_open_at(float x, float y, enum pane_type pane);
bool sdl_log_pane_menu_open_from_pointer(float x, float y);
void sdl_log_pane_menu_activate(int menu_index);
bool sdl_log_pane_menu_handle_pointer_down(float x, float y, SDL_FingerID finger_id, bool mouse);
bool sdl_log_pane_menu_handle_pointer_motion(float x, float y, SDL_FingerID finger_id, bool mouse);
bool sdl_log_pane_menu_handle_pointer_up(float x, float y, SDL_FingerID finger_id, bool mouse);
bool sdl_log_pane_menu_handle_long_press_down(float x, float y, SDL_FingerID finger_id);
bool sdl_log_pane_menu_handle_long_press_motion(float x, float y, SDL_FingerID finger_id);
bool sdl_log_pane_menu_handle_long_press_up(float x, float y, SDL_FingerID finger_id);
void sdl_log_pane_menu_cancel_long_press(SDL_FingerID finger_id);
int sdl_log_pane_menu_pending_timeout_ms(Uint64 now_ns);
bool sdl_log_pane_menu_flush_pending_press(Uint64 now_ns);
void sdl_log_pane_menu_render(void);
const char* sdl_side_pane_menu_label(enum pane_type pane);
bool sdl_side_pane_menu_config_is_entry(int index);
int sdl_side_pane_menu_collect(side_pane_menu_entry* entries);
bool sdl_side_pane_menu_point_to_side_pane(float x, float y, bool include_map, int* out_config_index, enum pane_type* out_pane);
bool sdl_side_pane_menu_layout(side_pane_menu_entry* entries, int* out_count, SDL_FRect* out_panel);
int sdl_side_pane_menu_index_at(float x, float y);
void sdl_side_pane_menu_clear_long_press(void);
void sdl_side_pane_menu_cancel(void);
bool sdl_side_pane_menu_open_at(float x, float y);
bool sdl_side_pane_menu_open_from_pointer(float x, float y);
void sdl_side_pane_menu_toggle(int menu_index);
bool sdl_side_pane_menu_handle_pointer_down(float x, float y, SDL_FingerID finger_id, bool mouse);
bool sdl_side_pane_menu_handle_pointer_motion(float x, float y, SDL_FingerID finger_id, bool mouse);
bool sdl_side_pane_menu_handle_pointer_up(float x, float y, SDL_FingerID finger_id, bool mouse);
bool sdl_side_pane_menu_handle_long_press_down(float x, float y, SDL_FingerID finger_id);
bool sdl_side_pane_menu_handle_long_press_motion(float x, float y, SDL_FingerID finger_id);
bool sdl_side_pane_menu_handle_long_press_up(float x, float y, SDL_FingerID finger_id);
void sdl_side_pane_menu_cancel_long_press(SDL_FingerID finger_id);
int sdl_side_pane_menu_pending_timeout_ms(Uint64 now_ns);
bool sdl_side_pane_menu_flush_pending_press(Uint64 now_ns);
void sdl_side_pane_menu_render(void);
void sdl_touch_pane_send_confirm_action(void);
void sdl_touch_pane_send_binding(int binding, bool second_panel, bool long_press);
int sdl_inventory_equipment_cycle_binding(int binding);
void sdl_touch_pane_send_slot(int panel, int index, bool long_press);
int sdl_touch_swipe_index_for_keypad_dir(int dir);
float sdl_touch_swipe_threshold_px(void);
int sdl_touch_swipe_direction_for_delta(float dx, float dy, float threshold);
float sdl_touch_swipe_edge_px(const SDL_Rect* screen);
bool sdl_touch_swipe_point_near_top_panel_edge(float x, float y);
bool sdl_touch_swipe_round_layer_start_allowed(float x, float y);
bool sdl_touch_swipe_start_can_toggle_top_panel(void);
bool sdl_touch_swipe_binding_is_top_panel_action(int binding);
void sdl_touch_swipe_cancel(void);
bool sdl_touch_swipe_handle_pointer_down(float x, float y, SDL_FingerID finger_id);
bool sdl_touch_swipe_handle_pointer_motion(float x, float y, SDL_FingerID finger_id);
void sdl_touch_swipe_handle_pointer_up(float x, float y, SDL_FingerID finger_id);
void sdl_touch_pane_cancel_press(void);
int sdl_touch_pane_pending_timeout_ms(Uint64 now_ns);
bool sdl_touch_pane_flush_pending_press(Uint64 now_ns);
bool sdl_touch_pane_handle_pointer_down(float x, float y, bool mouse, SDL_FingerID finger_id);
bool sdl_touch_pane_handle_pointer_motion(float x, float y, bool mouse, SDL_FingerID finger_id);
void sdl_touch_pane_handle_pointer_up(float x, float y, bool mouse, SDL_FingerID finger_id);
int sdl_touch_thumb_button_binding(int index, bool long_press);
bool sdl_touch_thumb_config_enabled(void);
bool sdl_touch_thumb_layout_active(void);
bool sdl_touch_thumb_compute_rects(SDL_FRect* out_rects);
bool sdl_touch_thumb_current_bounds(SDL_FRect* out_bounds);
bool sdl_touch_thumb_point_to_button(float px, float py, int* out_index);
void sdl_touch_thumb_render(void);
void sdl_touch_thumb_cancel_press(void);
int sdl_touch_thumb_pending_timeout_ms(Uint64 now_ns);
bool sdl_touch_thumb_flush_pending_press(Uint64 now_ns);
bool sdl_touch_thumb_handle_pointer_down(float x, float y, bool mouse, SDL_FingerID finger_id);
bool sdl_touch_thumb_handle_pointer_motion(float x, float y, bool mouse, SDL_FingerID finger_id);
bool sdl_touch_thumb_handle_pointer_up(float x, float y, bool mouse, SDL_FingerID finger_id);
bool sdl_touch_round_layer_config_enabled(void);
bool sdl_touch_round_layer_controls_active(void);
bool sdl_touch_round_point_excluded(float x, float y);
bool sdl_touch_movement_point_blocked_by_overlay(float x, float y);
float sdl_touch_round_radius_px(void);
bool sdl_touch_round_compute_clip_rect(SDL_Rect* out_clip);
bool sdl_touch_round_compute_layout(float* out_cx, float* out_cy,
    float* out_radius, float* out_inner_radius, SDL_Rect* out_clip);
void sdl_touch_target_layout_begin(void);
void sdl_touch_target_layout_end(void);
int sdl_touch_round_dir_for_delta(float dx, float dy);
void sdl_touch_round_send_dir(int dir, bool ctrl, bool run);
void sdl_touch_round_cancel_press(void);
bool sdl_touch_round_handle_pointer_down(float x, float y, SDL_FingerID finger_id);
bool sdl_touch_round_handle_pointer_motion(float x, float y, SDL_FingerID finger_id);
bool sdl_touch_round_handle_pointer_up(float x, float y, SDL_FingerID finger_id);
int sdl_touch_round_pending_timeout_ms(Uint64 now_ns);
void sdl_touch_round_flush_pending_highlight(Uint64 now_ns);
void sdl_touch_round_draw_circle(float cx, float cy, float radius, SDL_Color color);
void sdl_touch_round_draw_sector_lines(float cx, float cy, float inner_radius, float outer_radius, SDL_Color color);
bool sdl_touch_round_dir_to_map_rect(int dir, SDL_FRect* out_rect);
void sdl_touch_round_render_target_square(int dir, bool ctrl, bool run);
const char* sdl_touch_round_dir_label(int dir);
const char* sdl_touch_round_ctrl_action_for_dir(int dir);
void sdl_touch_round_ctrl_action_label(int dir, char* buf, size_t buflen);
void sdl_touch_round_render_ctrl_action_label(int dir, float radius, const SDL_Rect* clip);
void sdl_touch_round_render(void);
int sdl_touch_zone_overlay_mode_normalized(int mode);
bool sdl_touch_zone_overlay_visible(void);
bool sdl_touch_zone_layout_visible(void);
bool sdl_touch_zone_controls_active(void);
bool sdl_touch_zone_compute_layout_for_screen(const SDL_Rect* screen, SDL_FRect* zone_rects);
bool sdl_touch_zone_compute_layout(SDL_FRect* zone_rects);
bool sdl_touch_zone_point_to_zone(float x, float y, int* out_zone);
int sdl_touch_corner_up_down_side_normalized(int side);
bool sdl_touch_corner_up_down_on_left(void);
int sdl_touch_zone_center_binding_index(int zone, bool long_press);
int sdl_touch_zone_binding_for_center(int zone, bool long_press);
int sdl_touch_zone_corner_action_binding_index(int zone, bool long_press);
int sdl_touch_zone_binding_for_corner_action(int zone, bool long_press);
void sdl_touch_corner_action_binding_label(int binding, char* buf, size_t buflen);
bool sdl_touch_corner_action_apply_zoom_binding(int binding);
bool sdl_touch_zone_corner_action_label(int zone, char* name, size_t name_len, char* symbol, size_t symbol_len);
void sdl_touch_zone_button_label(int zone, char* name, size_t name_len, char* symbol, size_t symbol_len);
void sdl_touch_zone_render_markers(void);
bool sdl_touch_zone_is_arrow(int zone);
int sdl_touch_zone_arrow_dir(int zone);
SDL_FColor sdl_touch_hidden_indicator_fcolor(SDL_Color color);
float sdl_touch_hidden_indicator_size_for_screen(const SDL_Rect* screen);
void sdl_touch_hidden_indicator_draw_triangle( const SDL_FPoint points[3], SDL_Color fill, SDL_Color outline);
void sdl_touch_hidden_indicator_render_pane(const SDL_Rect* screen, bool right_side, float size, SDL_Color fill, SDL_Color outline);
void sdl_touch_hidden_indicator_render_bottom(const SDL_Rect* screen, float size, SDL_Color fill, SDL_Color outline);
void sdl_touch_hidden_indicator_render(void);
bool sdl_touch_hidden_indicator_handle_pointer_down(float x, float y, bool touch);
void sdl_touch_zone_send(int zone, bool long_press);
void sdl_touch_zone_cancel_press(void);
bool sdl_touch_zone_handle_pointer_down(float x, float y, SDL_FingerID finger_id);
bool sdl_touch_zone_handle_pointer_motion(float x, float y, SDL_FingerID finger_id);
bool sdl_touch_zone_handle_pointer_up(float x, float y, SDL_FingerID finger_id);
int sdl_touch_zone_pending_timeout_ms(Uint64 now_ns);
bool sdl_touch_zone_flush_pending_press(Uint64 now_ns);
bool sdl_touch_top_panel_layout_visible(void);
void sdl_touch_top_panel_set_open(bool open);
float sdl_touch_top_panel_size_normalized(float size);
int sdl_touch_top_panel_cell_count_normalized(int count);
int sdl_touch_top_panel_columns_normalized(int columns);
int sdl_touch_top_panel_rows_normalized(int rows);
int sdl_touch_top_panel_visible_button_count(void);
bool sdl_touch_top_panel_current_anchor(SDL_Rect* out_screen,
    SDL_Rect* out_anchor, enum pane_placement* out_where);
bool sdl_touch_top_panel_compute_layout_for_anchor(const SDL_Rect* screen,
    const SDL_Rect* anchor, enum pane_placement where,
    SDL_FRect* button_rects, SDL_FRect* out_panel);
bool sdl_touch_top_panel_compute_layout_for_screen( const SDL_Rect* screen, SDL_FRect* button_rects, SDL_FRect* out_panel);
bool sdl_touch_top_panel_point_to_slot(float x, float y, int* out_slot);
bool sdl_touch_top_panel_pointer_claims_point(float x, float y);
bool sdl_touch_top_panel_handle_secondary_pointer(float x, float y);
int sdl_touch_top_panel_binding_for_slot(int slot, bool long_press);
void sdl_touch_top_panel_label_for_slot(int slot, bool long_press, char* buf, size_t buflen);
void sdl_touch_top_panel_render_buttons( const SDL_FRect* button_rects);
void sdl_touch_top_panel_render(void);
void sdl_touch_top_panel_send_slot(int slot, bool long_press);
void sdl_touch_top_panel_cancel_press(void);
bool sdl_touch_top_panel_handle_pointer_down(float x, float y, SDL_FingerID finger_id);
bool sdl_touch_top_panel_handle_pointer_motion(float x, float y, bool mouse,
    SDL_FingerID finger_id);
bool sdl_touch_top_panel_handle_pointer_up(float x, float y, SDL_FingerID finger_id);
int sdl_touch_top_panel_pending_timeout_ms(Uint64 now_ns);
bool sdl_touch_top_panel_flush_pending_press(Uint64 now_ns);
bool sdl_gamepad_shift_active(void);
bool sdl_gamepad_ctrl_active(void);
bool sdl_gamepad_alt_active(void);
int sdl_gamepad_modifier_index(int binding);
int sdl_gamepad_single_active_modifier(void);
int sdl_gamepad_combo_binding_for_input(int modifier, int type, int id);
void sdl_gamepad_mark_auto_ui(void);
void sdl_gamepad_apply_modifier(int binding, bool down);
void sdl_send_macro_key(int key, bool shift, bool ctrl, bool alt);
int sdl_keymap_mode(void);
int sdl_shifted_ascii_for_key(int key);
char sdl_direction_char_for_key(int key);
int sdl_direction_for_key_char(char ch);
bool sdl_send_modified_direction_action(int dir, char dir_ch, bool shift, bool ctrl, bool alt, bool gui);
bool sdl_try_send_modified_direction_key(int key, bool shift, bool ctrl, bool alt, bool gui);
bool sdl_try_send_movement_event(const SDL_KeyboardEvent* key_event);
bool sdl_try_send_shadowed_command_event(const SDL_KeyboardEvent* key_event);
bool sdl_try_send_preset_command_alias(const SDL_KeyboardEvent* key_event);
bool sdl_try_send_modified_direction_event(const SDL_KeyboardEvent* key_event);
bool sdl_handle_jewelry_preset_shortcut( const SDL_KeyboardEvent* key_event);
bool sdl_handle_global_layout_shortcut(const SDL_KeyboardEvent* key_event);
void sdl_gamepad_send_key(int key, bool use_macro_mods);
void sdl_gamepad_send_key_raw(int key);
void sdl_gamepad_send_shoulder_combo(void);
void sdl_gamepad_send_direction_mods(int dir, bool shift, bool ctrl, bool alt);
int sdl_gamepad_axis_to_dir(Sint16 x, Sint16 y, int deadzone);
int sdl_gamepad_axis_to_cardinal_dir(Sint16 x, Sint16 y, int deadzone);
void sdl_gamepad_send_direction(int dir);
void sdl_gamepad_clear_pending_dpad(void);
void sdl_gamepad_set_pending_dpad(int dir);
bool sdl_gamepad_flush_pending_dpad(Uint64 now_ns, bool force);
void sdl_gamepad_clear_pending_left_stick(void);
void sdl_gamepad_set_pending_left_stick(int dir);
bool sdl_gamepad_flush_pending_left_stick(Uint64 now_ns, bool force);
void sdl_gamepad_clear_pending_confirm(void);
bool sdl_gamepad_confirm_long_press_available(int binding);
bool sdl_touch_top_panel_compute_layout(SDL_FRect* button_rects, SDL_FRect* out_panel);
bool sdl_gamepad_handle_confirm_long_press_button( int button, int binding, bool down);
int sdl_gamepad_pending_confirm_timeout_ms(Uint64 now_ns);
bool sdl_gamepad_flush_pending_confirm(Uint64 now_ns);
void sdl_gamepad_clear_pending_shoulder(void);
void sdl_gamepad_set_pending_shoulder(int button);
bool sdl_gamepad_flush_pending_shoulder(Uint64 now_ns, bool force);
bool sdl_gamepad_resolve_pending_shoulder_with_modifier(int binding);
int sdl_gamepad_pending_timeout_ms(Uint64 now_ns);
const char* sdl_gamepad_button_label(int button);
const char* sdl_gamepad_button_short_label(int button);
const char* sdl_gamepad_trigger_label(int index);
const char* sdl_gamepad_trigger_short_label(int index);
const char* sdl_gamepad_stick_dir_label(int type, int dir, bool short_label);
void sdl_gamepad_binding_label_ex(int type, int id, char* buf, size_t buflen, bool short_label);
bool sdl_gamepad_action_is_confirm(int binding);
bool sdl_gamepad_action_binding_equals(int lhs, int rhs);
int sdl_gamepad_direct_binding_count(int binding, int* out_type, int* out_id);
int sdl_gamepad_physical_binding_count(int binding, int* out_type, int* out_id);
int sdl_gamepad_combo_action_binding_count(int binding, int* out_modifier_type, int* out_modifier_id, int* out_type, int* out_id);
void sdl_gamepad_action_binding_label_ex(int binding, char* buf, size_t buflen, bool short_label);
void sdl_gamepad_action_binding_label(int binding, char* buf, size_t buflen);
void sdl_gamepad_action_binding_short_label(int binding, char* buf, size_t buflen);
int sdl_gamepad_capture_binding_for_input(int type, int id);
bool sdl_gamepad_capture_queue_input(int type, int id);
int steamdeck_back_key(void);
int steamdeck_confirm_key(void);
int steamdeck_prev_page_key(void);
int steamdeck_next_page_key(void);
int steamdeck_menu_key(int key, int prev_page_key, int next_page_key);
int steamdeck_info_key(void);
int steamdeck_alt_action_key(void);
int steamdeck_secondary_key(void);
void sdl_gamepad_handle_button(const SDL_GamepadButtonEvent* ev);
void sdl_gamepad_handle_axis(const SDL_GamepadAxisEvent* ev);
void sdl_gamepad_open(SDL_JoystickID id);
void sdl_gamepad_close(SDL_JoystickID id);
void sdl_gamepad_handle_device(const SDL_GamepadDeviceEvent* ev);
void sdl_gamepad_init(void);
void sdl_gamepad_shutdown(void);
void resize(const SDL_Rect* screen);
void sdl_handle_renderer_reset(void);
bool sdl_mobile_lifecycle_handle_event(const SDL_Event* ev);
bool SDLCALL sdl_mobile_lifecycle_event_watch(void* userdata, SDL_Event* ev);
void sdl_mobile_lifecycle_register(void);
void sdl_mobile_lifecycle_unregister(void);
bool sdl_event_is_disabled_mouse_input(const SDL_Event* ev);
bool sdl_event_is_touch_mouse_input(const SDL_Event* ev);
bool sdl_ascii_contains_ci(cptr text, cptr needle);
bool sdl_mouse_id_looks_like_touchscreen(SDL_MouseID id);
bool sdl_event_is_handheld_touch_fallback_input(const SDL_Event* ev);
void sdl_log_handheld_touch_mouse_fallback(SDL_MouseID id);
bool sdl_dispatch_touch_mouse_fallback(sdl_state* st, SDL_EventType type, SDL_WindowID window_id, Uint64 timestamp, float x, float y, float dx, float dy);
bool sdl_try_handle_touch_mouse_fallback_event(sdl_state* st, const SDL_Event* ev);
void sdl_normalize_event_to_render_coords(SDL_Event* ev);
bool sdl_quit_transition_active(void);
bool sdl_quit_transition_input_event(const SDL_Event* ev);
bool sdl_quit_transition_consume_event(const SDL_Event* ev);
bool sdl_yes_no_prompt_handle_modal_event(const SDL_Event* ev);
void sdl_handle_event(sdl_state* st, SDL_Event* ev);
void sdl_touch_pane_render(void);
void sdl_restore_render_target(sdl_view* d);
bool sdl_left_panel_ensure_canvas(int width, int height);
bool sdl_left_panel_cell_is_tile(byte a, char c);
void sdl_left_panel_debug_make_row_text(const term_win* scr, int row, int source_col, int end_col, char* out, size_t out_size, int* out_visible_width);
void sdl_left_panel_debug_log_source_row(const term* t, const term_win* scr, int source_row, int source_col, int width, int end_col, int dest_col, int dest_row, float content_x, int cell_w, int cell_h);
SDL_Color sdl_left_panel_background_color(void);
void sdl_render_mono_text_scaled(SDL_Texture* atlas, int atlas_cell_w, int atlas_cell_h, float cell_w, float cell_h, float origin_x, float origin_y, int x, int y, int n, const char* s, SDL_Color col);
byte sdl_left_panel_pane_render_attr_for_cell(int source_col, int source_row, byte attr);
void sdl_render_left_panel_source_row_cells(const sdl_view* view, const term* source_term, term_win* scr, int source_row, int source_col, int width, int dest_col, int dest_row, float content_x, float content_y, int cell_w, int cell_h, SDL_Texture* font_atlas, int atlas_cell_w, int atlas_cell_h, TTF_Font* mono_font);
void sdl_left_panel_debug_log_frame(const sdl_view* view, const sdl_left_panel_metrics* metrics, const SDL_FRect* dst_left, int visual_cols, int visual_rows, int source_h, int visual_w, int canvas_w);
bool sdl_render_left_panel_pane_from_cells(const sdl_view* view, const SDL_FRect* dst_left);
void sdl_combat_overlay_pane_render(void);
bool sdl_render_main_view_with_left_panel(const sdl_view* view);
bool sdl_saved_screen_cell_changed(const term_win* scr, const term_win* mem, int x, int y);
void sdl_redraw_saved_screen_overlay_cells(const sdl_view* view, const SDL_FRect* clip_rect);
bool sdl_render_saved_screen_left_panel_backdrop(const sdl_view* view);
bool sdl_render_current_window_frame(void);
void sdl_set_present_suppressed(bool suppressed);
void sdl_present_batch_begin(void);
void sdl_present_batch_end(void);
void sdl_present_if_needed(sdl_view* d);
float sdl_touch_tutorial_draw_text_line(cptr text, float x, float y, float max_w, int font_px, SDL_Color color, bool centered);
int sdl_touch_tutorial_wrap_lines(cptr text, TTF_Font* font, float max_w, char lines[][SDL_TOUCH_TUTORIAL_LINE_LEN], int max_lines);
float sdl_touch_tutorial_draw_wrapped(cptr text, float x, float y, float max_w, int font_px, SDL_Color color);
float sdl_touch_tutorial_draw_wrapped_centered(cptr text, float x, float y, float max_w, int font_px, SDL_Color color);
int sdl_touch_tutorial_line_count(cptr text, int font_px, float max_w);
void sdl_touch_tutorial_draw_screen_dim(const SDL_Rect* screen, Uint8 alpha);
float sdl_touch_tutorial_top_reserved_height(const SDL_Rect* screen);
float sdl_touch_tutorial_default_header_y(const SDL_Rect* screen);
float sdl_touch_tutorial_draw_header_at(const SDL_Rect* screen, cptr title, cptr body, int page, int page_count, float y);
float sdl_touch_tutorial_draw_header(const SDL_Rect* screen, cptr title, cptr body, int page, int page_count);
void sdl_touch_tutorial_draw_footer(const SDL_Rect* screen, bool mouse, bool single_page);
bool sdl_touch_tutorial_cell_rect(int col, int row, int cols, int rows, SDL_FRect* out);
bool sdl_touch_tutorial_view_rect(enum pane_type pane, SDL_FRect* out);
void sdl_touch_tutorial_clamp_box_to_screen(SDL_FRect* box, const SDL_Rect* screen, float margin);
bool sdl_touch_tutorial_compact_layout(const SDL_Rect* screen);
void sdl_touch_tutorial_draw_compact_zone_label( const SDL_Rect* screen, const SDL_FRect* zone, cptr label);
void sdl_touch_tutorial_draw_compact_zone_legend( const SDL_Rect* screen, float min_y, const char* const* lines, int line_count, bool mouse, bool mobile_section);
void sdl_touch_tutorial_draw_zone_highlight(const SDL_FRect* zone);
void sdl_touch_tutorial_draw_zone_prompt(const SDL_Rect* screen, const SDL_FRect* zone, cptr title, cptr detail, float min_y);
void sdl_touch_tutorial_draw_info_panel(const SDL_Rect* screen, float x, float y, float w, cptr title, cptr body);
void sdl_touch_tutorial_draw_main_screen_zones_compact( const SDL_Rect* screen, float header_bottom, bool mouse, int section);
void sdl_touch_tutorial_draw_main_screen_zones( const SDL_Rect* screen, bool mouse, float min_callout_y);
void sdl_touch_tutorial_draw_zones_page(const SDL_Rect* screen, int page, int page_count, bool mouse);
void sdl_touch_tutorial_draw_overlay_menu(const SDL_Rect* screen);
void sdl_touch_tutorial_draw_pane_page(const SDL_Rect* screen, int page, int page_count);
void sdl_touch_tutorial_draw_movement_page(const SDL_Rect* screen, int page, int page_count);
void sdl_touch_tutorial_draw_buttonwheel_page(const SDL_Rect* screen,
    int page, int page_count);
int sdl_touch_tutorial_wait_action(Uint64 accept_after_ns);
void sdl_touch_tutorial_draw_page(int page, bool full, int page_count, bool mouse);
void sdl_touch_tutorial_prepare_snapshot(void);
void sdl_touch_tutorial_run(bool full, bool mouse);
int sdl_touch_tutorial_current_choice_index(void);
int sdl_touch_tutorial_choose_profile(void);
void sdl_touch_tutorial_save_profile_choice(int profile);
void sdl_touch_tutorial_run_fixed(void);
void sdl_touch_mark_tutorial_seen_and_save(void);
void sdl_mouse_mark_tutorial_seen_and_save(void);
void sdl_touch_request_tutorial_from_settings(void);
void sdl_mouse_request_tutorial_from_settings(void);
bool sdl_touch_settings_tutorial_requested(void);
bool sdl_mouse_settings_tutorial_requested(void);
void sdl_touch_show_requested_tutorial(void);
void sdl_mouse_show_requested_tutorial(void);
void sdl_touch_tutorial_maybe_show_deferred(void);
void sdl_mouse_tutorial_maybe_show_deferred(void);
void sdl_zones_tutorial_maybe_show_deferred(void);
void sdl_input_tutorial_maybe_show_deferred(void);
void sdl_touch_show_tutorial(void);
void sdl_mouse_show_tutorial(void);
void sdl_zones_show_tutorial(void);
void sdl_zones_show_requested_tutorial(void);
void sdl_zones_request_tutorial_from_settings(void);
bool sdl_zones_settings_tutorial_requested(void);
void sdl_touch_maybe_show_first_game_tutorial(void);
void sdl_mouse_maybe_show_first_game_tutorial(void);
void sdl_character_wheel_maybe_show_first_game_tutorial(void);
void sdl_character_wheel_mark_tutorial_seen_and_save(void);
void sdl_character_wheel_request_tutorial_from_settings(void);
bool sdl_character_wheel_settings_tutorial_requested(void);
errr callback_sdl_xtra(int n, int v);
void draw_cursor(int x, int y, bool big);
errr callback_sdl_curs(int x, int y);
errr callback_sdl_bigcurs(int x, int y);
errr callback_sdl_wipe(int x, int y, int n);
errr callback_sdl_text(int x, int y, int n, byte a, cptr s);
void sdl_draw_tileset_sprite_ex(byte a, char c, const SDL_FRect* dst, bool icon, SDL_FlipMode flip);
void sdl_draw_tileset_sprite(byte a, char c, const SDL_FRect* dst, bool icon);
bool sdl_map_grid_is_player(int y, int x);
bool sdl_player_tile_directional_enabled(void);
bool sdl_player_tile_handcrafted_enabled(void);
bool sdl_player_tile_apply_horizontal_facing(int dir);
bool sdl_player_tile_facing_right(void);
byte sdl_player_tile_handcrafted_right_attr(byte a);
void sdl_draw_ascii_minimap_cell(byte a, char c, byte ta, char tc, const SDL_FRect* dst);
bool sdl_rage_grid_filter_active(int y, int x);
bool sdl_rage_wall_tint_active(int y, int x);
bool sdl_rage_floor_tint_active(int y, int x);
bool sdl_rage_visible_floor_object(int y, int x);
bool sdl_rage_base_floor_tint_active(int y, int x);
bool sdl_rage_base_object_tint_active(int y, int x);
u32b sdl_rage_wall_filter_hash(int y, int x, u32b phase);
void sdl_restore_tileset_mod(void);
SDL_Rect sdl_frect_to_clip_rect(const SDL_FRect* rect);
void sdl_draw_rage_tile_filter(byte a, char c, int y, int x, const SDL_FRect* dst);
void sdl_draw_map_tile_layers_at(int dy, int dx, byte a, char c, byte ta, char tc, const SDL_FRect* dst);
bool sdl_minimap_hint_source_valid(const hint_message_meta* meta);
void sdl_minimap_expand_bounds_for_hint_sources(int* min_y, int* min_x, int* max_y, int* max_x, bool* any);
const object_type* sdl_minimap_skeleton_at(int y, int x);
void sdl_minimap_draw_hint_source_symbol(const object_type* o_ptr, const SDL_FRect* dst);
void sdl_minimap_draw_hint_sources(const SDL_FRect* map_dst, int min_y, int min_x, int max_y, int max_x);
void sdl_minimap_draw_focus_tip(sdl_view* d, int canvas_w, int canvas_h, const SDL_FRect* map_dst, int min_y, int min_x, int max_y, int max_x);
bool sdl_minimap_known_bounds(int* min_y, int* min_x, int* max_y, int* max_x);
void sdl_side_map_pane_note_level(void);
void sdl_side_map_pane_forget_level(void);
void sdl_side_map_pane_invalidate_cell(int y, int x);
void sdl_side_map_pane_texture_cache_clear(void);
void sdl_side_map_pane_redraw(void);
bool sdl_side_map_pane_content_rect(SDL_FRect* out_rect, SDL_Rect* out_clip);
void sdl_side_map_pane_draw_player_marker(const SDL_FRect* map_dst, int min_y, int min_x, int max_y, int max_x);
void sdl_side_map_pane_render_empty(const SDL_FRect* content);
void sdl_side_map_pane_render(void);
bool sdl_side_map_pane_adjust_zoom(int delta);
bool sdl_side_map_pane_offset_by(float dx, float dy);
void sdl_side_map_pane_begin_press(bool mouse, SDL_FingerID finger_id, float x, float y);
bool sdl_side_map_pane_press_matches(bool mouse, SDL_FingerID finger_id);
void sdl_side_map_pane_update_press_drag(float x, float y);
void sdl_side_map_pane_clear_press(void);
void sdl_side_map_pane_send_click(void);
void sdl_side_map_pane_begin_drag(bool mouse, SDL_FingerID finger_id, float x, float y);
bool sdl_side_map_pane_drag_to(bool mouse, SDL_FingerID finger_id, float x, float y);
void sdl_side_map_pane_cancel_drag(void);
int sdl_side_map_pane_find_finger(SDL_FingerID finger_id);
int sdl_side_map_pane_active_finger_count(void);
bool sdl_side_map_pane_first_two_fingers(int* out_a, int* out_b);
float sdl_side_map_pane_finger_distance(int a, int b);
void sdl_side_map_pane_start_pinch_if_possible(void);
void sdl_side_map_pane_start_drag_from_first_finger(void);
bool sdl_side_map_pane_update_pinch(void);
void sdl_side_map_pane_add_or_update_finger(SDL_FingerID finger_id, float x, float y);
bool sdl_side_map_pane_remove_finger(SDL_FingerID finger_id);
bool sdl_side_map_pane_handle_mouse_wheel( const SDL_MouseWheelEvent* wheel);
bool sdl_side_map_pane_handle_pointer_down(float x, float y, bool mouse, SDL_FingerID finger_id);
bool sdl_side_map_pane_handle_pointer_motion(float x, float y, bool mouse, SDL_FingerID finger_id);
bool sdl_side_map_pane_handle_pointer_up(float x, float y, bool mouse, SDL_FingerID finger_id);
void sdl_side_map_pane_cancel_pointer(SDL_FingerID finger_id, bool mouse);
bool sdl_display_pixel_map(int* cy, int* cx);
errr callback_sdl_pict(int x, int y, int n, const byte* ap, const char* cp, const byte* tap, const char* tcp);
bool sdl_load_tileset_texture(void);
void sdl_apply_tiles_to_terms(bool tiles);
void sdl_mark_tiles_mode_game_redraw(void);
bool sdl_set_tiles_runtime(bool value);
void sdl_finish_tiles_mode_change(void);
void callback_sdl_nuke();
void callback_sdl_init(term* t);
errr sdl_view_link_term(sdl_view* d, int term_index);
void sdl_apply_font_settings(TTF_Font* font, bool is_story_font);
mono_font_style_key sdl_current_mono_font_style_key(void);
void sdl_apply_mono_font_style_key(TTF_Font* font, const mono_font_style_key* style);
const char* sdl_monospace_font_path(void);
void sdl_mono_font_prewarm_queue_clear(void);
void sdl_mono_font_cache_clear(void);
TTF_Font* sdl_load_mono_font_cells(const char* font_path, int cell_width, int cell_height);
TTF_Font* sdl_acquire_mono_font_cells(const char* font_path, int cell_width, int cell_height);
bool sdl_mono_font_atlas_entry_matches_style( const mono_font_atlas_entry* entry, const char* font_path, int cell_width, int cell_height, const mono_font_style_key* style);
bool sdl_mono_font_atlas_entry_matches( const mono_font_atlas_entry* entry, const char* font_path, int cell_width, int cell_height);
mono_font_atlas_entry* sdl_find_mono_font_atlas_fallback_cells( const char* font_path, int cell_width, int cell_height);
bool sdl_mono_font_atlas_cached_cells(const char* font_path, int cell_width, int cell_height);
Uint64 sdl_mono_font_atlas_generation_for_cells(const char* font_path,
    int cell_width, int cell_height);
bool sdl_mono_font_atlas_cached_cells_style(const char* font_path, int cell_width, int cell_height, const mono_font_style_key* style);
bool sdl_mono_font_atlas_cache_has_free_slot(void);
bool sdl_store_mono_font_atlas_cells(const char* font_path, int cell_width, int cell_height, SDL_Texture* atlas, const mono_font_style_key* style);
SDL_Texture* sdl_acquire_mono_font_atlas_cells_ex(const char* font_path, int cell_width, int cell_height, bool* out_cached, int* out_atlas_cell_w, int* out_atlas_cell_h, bool* out_exact, bool allow_fallback);
void sdl_story_font_cache_clear(void);
void sdl_char_sheet_fitted_wrap_cache_clear(void);
bool sdl_story_font_cache_matches_config(void);
void sdl_story_font_cache_mark_config(void);
const char* sdl_story_font_path_for_slot(int slot);
TTF_Font* sdl_story_font_for_height(int pixel_height);
TTF_Font* sdl_story_font_for_height_slot(int pixel_height, int slot);
TTF_Font* sdl_story_font_for_view(const sdl_view* d);
TTF_Font* sdl_story_font_for_view_slot(const sdl_view* d, int slot);
TTF_Font* sdl_story_font_slot_sibling(TTF_Font* font, int slot);
bool sdl_mono_atlas_codepoint_visible(Uint32 ch);
void sdl_mono_atlas_set_error(char* error_buf, size_t error_buf_size, const char* message);
SDL_Surface* sdl_build_ttf_font_atlas_surface(const char* font_path, int cell_width, int cell_height, const mono_font_style_key* style, int* actual_font_size, char* error_buf, size_t error_buf_size);
SDL_Texture* sdl_texture_from_mono_atlas_surface(SDL_Surface* surface, char* error_buf, size_t error_buf_size);
SDL_Texture* sdl_try_load_ttf_font_cells(const char* font_path, int cell_width, int cell_height, int* actual_font_size, char* error_buf, size_t error_buf_size);
SDL_Texture* sdl_load_ttf_font_cells(const char* font_path, int cell_width, int cell_height, int* actual_font_size);
void sdl_window_set_position(int x, int y);
void sdl_window_create(int window_width, int window_height, bool fullscreen, bool use_tiles);
bool sdl_window_reassert_fullscreen(cptr reason);
bool sdl_view_create(sdl_view* d, SDL_Rect rect, const char* font_path, int font_size, int scale, int margin);
TTF_Font* sdl_load_font_with_fallback(const char* font_path, int font_size, const char* fallback_path);
void sdl_load_story_fonts(void);
bool sdl_mono_font_prewarm_request_exists(const char* font_path, int cell_width, int cell_height);
void sdl_queue_mono_font_atlas_prewarm_cells(const char* font_path, int cell_width, int cell_height);
void sdl_queue_main_view_scale_prewarm_main_only(int scale);
void sdl_queue_main_view_scale_prewarm_panes(int scale);
void sdl_queue_main_view_scale_neighbors_prewarm(const char* reason);
bool sdl_mono_font_prewarm_pop_request(mono_font_prewarm_request* out);
int SDLCALL sdl_mono_font_prewarm_thread(void* data);
void sdl_mono_font_prewarm_job_reset(void);
void sdl_mono_font_prewarm_job_shutdown(void);
bool sdl_mono_font_prewarm_finish_ready(void);
bool sdl_mono_font_prewarm_start_next(void);
void sdl_mono_font_prewarm_process_idle(void);
void sdl_story_font_enable(void);
void sdl_story_font_disable(void);
bool sdl_is_story_font_enabled(void);
void sdl_story_font_set_grid(bool grid);
bool sdl_is_story_font_grid(void);
void sdl_story_font_set_slot(int slot);
void sdl_story_font_reset(void);
int sdl_story_font_text_width(cptr text, int len);
int sdl_get_cell_width(void);
int sdl_main_view_visible_col0(void);
int sdl_main_view_visible_cols(void);
void sdl_quit_hook(cptr str);
errr init_sdl(int argc, char **argv);
void get_sdl_config_info(char* buf, size_t size);
bool save_pane_config_to_json(void);
cptr get_sdl_config_path(void);
int get_sdl_main_view_scale(void);
int get_sdl_effective_main_view_scale(void);
int get_sdl_min_main_view_scale(void);
int get_sdl_terminal_menu_scale_offset(void);
void set_sdl_terminal_menu_scale_offset(int value);
int get_sdl_mobile_starting_zoom_offset(void);
void set_sdl_mobile_starting_zoom_offset(int value);
bool get_sdl_mobile_portrait_mode(void);
void set_sdl_mobile_portrait_mode(bool value);
void sdl_set_mobile_orientation_hint(bool portrait);
int get_sdl_min_terminal_mode(void);
void set_sdl_min_terminal_mode(int value);
void set_sdl_main_view_scale(int value);
bool set_sdl_main_view_zoom_scale(int value);
void sdl_suspend_main_view_zoom_for_saved_screen(void);
bool sdl_resume_main_view_zoom_for_saved_screen(void);
int get_sdl_aux_view_font_size(void);
int get_sdl_effective_aux_view_font_size(void);
void set_sdl_aux_view_font_size(int value);
int get_sdl_margin(void);
void set_sdl_margin(int value);
int get_sdl_camera_center_clearance(void);
void set_sdl_camera_center_clearance(int value);
bool get_sdl_fullscreen(void);
void set_sdl_fullscreen(bool value);
bool get_sdl_tiles(void);
bool get_sdl_use_unsafe_area(void);
void set_sdl_use_unsafe_area(bool value);
void set_sdl_tiles(bool value);
void sdl_current_default_dimensions(int* out_w, int* out_h);
void sdl_show_interface_settings_reset_notice(void);
void sdl_reset_interface_settings_to_defaults_for_migration(void);
int get_pane_config_count(void);
int get_sdl_pane_type(int index);
int get_sdl_pane_where(int index);
void set_sdl_pane_where(int index, int where);
int get_sdl_pane_stack_order(int index);
int get_sdl_pane_stack_count(int where);
void set_sdl_pane_where_order(int index, int where, int order);
bool get_sdl_pane_enabled(int index);
int get_sdl_pane_rows(int index);
int get_sdl_pane_cols(int index);
int get_sdl_pane_font_size(int index);
int get_sdl_pane_effective_font_size(int index);
int sdl_pane_current_size(int index, bool want_rows);
int get_sdl_pane_current_rows(int index);
int get_sdl_pane_current_cols(int index);
void set_sdl_pane_rows(int index, int rows);
void set_sdl_pane_cols(int index, int cols);
void set_sdl_pane_font_size(int index, int font_size);
void set_sdl_pane_enabled(int index, bool enabled);
bool sdl_pane_group_matches(enum pane_placement where, bool side);
bool sdl_default_pane_enabled_for_group(enum pane_type type, bool side, bool* found);
void sdl_enable_default_panes_for_empty_group(bool side);
bool get_sdl_enable_right_panes(void);
void set_sdl_enable_right_panes(bool value);
bool get_sdl_enable_bottom_panes(void);
void set_sdl_enable_bottom_panes(bool value);
bool get_sdl_show_pane_borders(void);
void set_sdl_show_pane_borders(bool value);
bool get_sdl_left_overlays_touch_screen_edge(void);
void set_sdl_left_overlays_touch_screen_edge(bool value);
bool get_sdl_show_overlay_log_border(void);
void set_sdl_show_overlay_log_border(bool value);
bool get_sdl_hide_left_panel(void);
void sdl_push_saved_screen_left_panel_pane(void);
void sdl_pop_saved_screen_left_panel_pane(void);
bool get_sdl_left_panel_expanded_on_launch(void);
void set_sdl_left_panel_expanded_on_launch(bool value);
int get_sdl_left_panel_compact_mode(void);
void set_sdl_left_panel_compact_mode(int mode);
int get_sdl_intro_style(void);
void set_sdl_intro_style(int style);
void sdl_gamepad_load_default_bindings(void);
void sdl_mouse_load_default_settings(void);
void sdl_touch_pane_load_default_bindings(void);
bool steamdeck_controls_active(void);
bool sdl_menu_letters_enabled(void);
bool sdl_touch_only_device_active(void);
bool portable_controls_active(void);
bool get_sdl_gamepad_enabled(void);
void set_sdl_gamepad_enabled(bool value);
bool get_sdl_gamepad_auto_mode(void);
void set_sdl_gamepad_auto_mode(bool value);
bool get_sdl_steamdeck_mode(void);
void set_sdl_steamdeck_mode(bool value);
bool get_sdl_steamdeck_inv_equip_same_button_cycle(void);
void set_sdl_steamdeck_inv_equip_same_button_cycle(bool value);
bool get_sdl_gamepad_use_dpad(void);
void set_sdl_gamepad_use_dpad(bool value);
bool get_sdl_gamepad_use_left_stick(void);
void set_sdl_gamepad_use_left_stick(bool value);
bool get_sdl_gamepad_default_enabled(void);
bool get_sdl_gamepad_default_auto_mode(void);
bool get_sdl_steamdeck_default_mode(void);
bool get_sdl_steamdeck_default_inv_equip_same_button_cycle(void);
bool get_sdl_gamepad_default_use_dpad(void);
bool get_sdl_gamepad_default_use_left_stick(void);
int get_sdl_gamepad_button_binding(int button);
void set_sdl_gamepad_button_binding(int button, int binding);
int get_sdl_gamepad_trigger_binding(int index);
void set_sdl_gamepad_trigger_binding(int index, int binding);
int get_sdl_gamepad_left_stick_binding(int dir);
void set_sdl_gamepad_left_stick_binding(int dir, int binding);
int get_sdl_gamepad_right_stick_binding(int dir);
void set_sdl_gamepad_right_stick_binding(int dir, int binding);
int get_sdl_gamepad_combo_binding(int modifier, int type, int id);
void set_sdl_gamepad_combo_binding(int modifier, int type, int id, int binding);
int get_sdl_gamepad_shoulder_combo_binding(void);
void set_sdl_gamepad_shoulder_combo_binding(int binding);
int get_sdl_gamepad_default_button_binding(int button);
int get_sdl_gamepad_default_trigger_binding(int index);
int get_sdl_gamepad_default_left_stick_binding(int dir);
int get_sdl_gamepad_default_right_stick_binding(int dir);
int get_sdl_gamepad_default_combo_binding(int modifier, int type, int id);
int get_sdl_gamepad_default_shoulder_combo_binding(void);
void sdl_gamepad_reset_bindings_to_default(void);
int sdl_touch_profile_normalized(int profile);
int get_sdl_mouse_movement_mode(void);
void set_sdl_mouse_movement_mode(int mode);
int get_sdl_mouse_movement_default_mode(void);
bool get_sdl_mouse_enabled(void);
void set_sdl_mouse_enabled(bool enabled);
bool get_sdl_mouse_default_enabled(void);
bool get_sdl_mouse_tile_pointer(void);
void set_sdl_mouse_tile_pointer(bool enabled);
bool get_sdl_mouse_default_tile_pointer(void);
int get_sdl_touch_pane_binding(int index);
int get_sdl_touch_pane_binding_for_panel(int panel, int index);
void set_sdl_touch_pane_binding(int index, int binding);
void set_sdl_touch_pane_binding_for_panel(int panel, int index, int binding);
int get_sdl_touch_pane_default_binding(int index);
int get_sdl_touch_pane_default_binding_for_panel(int panel, int index);
bool get_sdl_touch_pane_enabled(void);
void set_sdl_touch_pane_enabled(bool value);
bool get_sdl_touch_pane_default_open(void);
void set_sdl_touch_pane_default_open(bool value);
bool get_sdl_touch_pane_default_open_default(void);
bool get_sdl_touch_pane_key_labels_visible(void);
void set_sdl_touch_pane_key_labels_visible(bool value);
bool get_sdl_touch_pane_key_labels_default_visible(void);
bool get_sdl_touch_pane_inventory_equipment_cycle(void);
void set_sdl_touch_pane_inventory_equipment_cycle(bool value);
bool get_sdl_touch_pane_inventory_equipment_default_cycle(void);
int get_sdl_touch_pane_placement(void);
void set_sdl_touch_pane_placement(int placement);
int sdl_touch_movement_normalized_mode(int mode);
int sdl_touch_menu_category_normalized(int category);
bool get_sdl_touch_menu_commands_enabled(int category);
void set_sdl_touch_menu_commands_enabled(int category, bool value);
bool get_sdl_touch_menu_commands_default_enabled(int category);
int get_sdl_touch_profile(void);
void set_sdl_touch_profile(int profile);
int get_sdl_touch_profile_default(void);
void sdl_touch_cancel_all_inputs(void);
void sdl_touch_apply_profile(int profile);
int get_sdl_touch_movement_mode(void);
void set_sdl_touch_movement_mode(int mode);
int get_sdl_touch_movement_default_mode(void);
bool get_sdl_touch_round_movement_enabled(void);
void set_sdl_touch_round_movement_enabled(bool value);
bool get_sdl_touch_round_movement_default_enabled(void);
int get_sdl_touch_zone_overlay_mode(void);
void set_sdl_touch_zone_overlay_mode(int mode);
int get_sdl_touch_zone_overlay_default_mode(void);
int get_sdl_touch_zone_center_binding(int index);
void set_sdl_touch_zone_center_binding(int index, int binding);
int get_sdl_touch_zone_center_default_binding(int index);
int get_sdl_touch_corner_up_down_side(void);
void set_sdl_touch_corner_up_down_side(int side);
int get_sdl_touch_corner_up_down_default_side(void);
int get_sdl_touch_corner_action_binding(int index);
void set_sdl_touch_corner_action_binding(int index, int binding);
int get_sdl_touch_corner_action_default_binding(int index);
bool get_sdl_touch_top_panel_arrows_visible(void);
void set_sdl_touch_top_panel_arrows_visible(bool value);
bool get_sdl_touch_top_panel_arrows_default_visible(void);
bool get_sdl_touch_top_panel_default_open(void);
void set_sdl_touch_top_panel_default_open(bool value);
bool get_sdl_touch_top_panel_default_open_default(void);
float get_sdl_touch_top_panel_size(void);
void set_sdl_touch_top_panel_size(float size);
float get_sdl_touch_top_panel_default_size(void);
int get_sdl_touch_top_panel_columns(void);
void set_sdl_touch_top_panel_columns(int columns);
int get_sdl_touch_top_panel_default_columns(void);
int get_sdl_touch_top_panel_cell_count(void);
void set_sdl_touch_top_panel_cell_count(int count);
int get_sdl_touch_top_panel_default_cell_count(void);
int get_sdl_touch_top_panel_rows(void);
void set_sdl_touch_top_panel_rows(int rows);
int get_sdl_touch_top_panel_default_rows(void);
int get_sdl_touch_top_panel_binding(int index, bool long_press);
void set_sdl_touch_top_panel_binding(int index, bool long_press, int binding);
int get_sdl_touch_top_panel_default_binding(int index, bool long_press);
bool sdl_ensure_main_menu_access(void);
bool get_sdl_touch_swipe_enabled(void);
void set_sdl_touch_swipe_enabled(bool value);
int get_sdl_touch_swipe_binding(int dir);
void set_sdl_touch_swipe_binding(int dir, int binding);
bool get_sdl_touch_swipe_default_enabled(void);
int get_sdl_touch_swipe_default_binding(int dir);
void sdl_touch_pane_reset_bindings_to_default(void);
void sdl_touch_pane_begin_yes_no_prompt_impl(cptr prompt, sdl_touch_yes_no_prompt_placement placement);
void sdl_touch_pane_begin_yes_no_prompt(cptr prompt);
void sdl_touch_pane_begin_yes_no_prompt_lower(cptr prompt);
void sdl_touch_pane_begin_yes_no_prompt_near(cptr prompt, int map_y, int map_x);
void sdl_touch_pane_end_yes_no_prompt(void);
cptr get_sdl_touch_pane_slot_name(int index);
void get_sdl_touch_pane_button_label(int index, char* buf, size_t buflen);
void set_sdl_touch_pane_button_label(int index, cptr label);
void clear_sdl_touch_pane_button_label(int index);
void get_sdl_touch_pane_button_label_for_panel(int panel, int index, char* buf, size_t buflen);
void set_sdl_touch_pane_button_label_for_panel(int panel, int index, cptr label);
void clear_sdl_touch_pane_button_label_for_panel(int panel, int index);
void get_sdl_touch_pane_panel_name(int panel, char* buf, size_t buflen);
void set_sdl_touch_pane_panel_name(int panel, cptr name);
bool sdl_gamepad_capture_begin(bool allow_modifier_combo);
void sdl_gamepad_capture_cancel(void);
bool sdl_gamepad_capture_poll(int* out_type, int* out_id, int* out_modifier);
bool sdl_keyboard_capture_begin(void);
void sdl_keyboard_capture_cancel(void);
bool sdl_keyboard_capture_poll(SDL_Scancode* out_scancode,
    u16b* out_modifiers);
int get_sdl_max_scale(void);
int get_sdl_max_main_view_zoom_scale(void);
int get_sdl_min_main_view_zoom_scale(void);
void sdl_clamp_main_view_zoom_to_current_layout(void);
void sdl_refresh_supporting_panes_layout(void);
void sdl_refresh_supporting_panes_layout_deferred(void);
void sdl_apply_runtime_zoom(void);
void sdl_apply_config_impl(bool request_redraw);
void sdl_apply_config(void);
void sdl_apply_config_no_redraw(void);
int get_sdl_platform_max_main_view_scale(void);
int get_sdl_terminal_menu_scale(void);
void sdl_push_terminal_menu_scale(void);
void sdl_pop_terminal_menu_scale(void);
bool sdl_description_overlay_present(const byte* attrs, const char* chars, const byte* tattrs, const char* tchars, const byte* story, const byte* health, int width, int height, int target_cols, int scroll, bool interactive, int* out_visible_rows, int* out_max_scroll);
void sdl_description_overlay_clear(void);
void sdl_request_redraw(void);
void sdl_apply_story_font_state(bool active);
void sdl_apply_story_grid_state(bool grid);
void sdl_apply_story_slot_state(int slot);
void sdl_story_font_reset_state(void);
void sdl_render_mono_text(sdl_view* d, int x, int y, int n, const char* s, SDL_Color col);
SDL_Texture* sdl_ui_text_texture(TTF_Font* font, cptr text, SDL_Color color,
    int* out_width, int* out_height);
SDL_Texture* sdl_ui_wrapped_text_texture(TTF_Font* font, cptr text,
    int wrap_width, SDL_Color color, int* out_width, int* out_height);
void sdl_ui_text_cache_clear(void);
void sdl_ui_text_cache_clear_font(TTF_Font* font);
void sdl_render_mono_utf8_glyph(TTF_Font* font, float cell_w, float cell_h, float origin_x, float origin_y, int x, int y, int cell_offset, int cell_span, const char* s, int len, SDL_Color col);
void sdl_render_mono_utf8_text_cells(SDL_Texture* atlas, int atlas_cell_w, int atlas_cell_h, TTF_Font* font, float cell_w, float cell_h, float origin_x, int x, int y, int n, const char* s, SDL_Color col);
void sdl_render_mono_utf8_text_cells_at(SDL_Texture* atlas, int atlas_cell_w, int atlas_cell_h, TTF_Font* font, float cell_w, float cell_h, float origin_x, float origin_y, int x, int y, int n, const char* s, SDL_Color col);
void sdl_render_story_text_free(sdl_view* d, TTF_Font* font, int x, int y, int n, const char* s, SDL_Color col);
int sdl_render_story_text_free_px(sdl_view* d, TTF_Font* font, float x_px, int y, const char* s, int n, SDL_Color col, float max_w_px);
bool sdl_story_cell_is_text(byte a, char c);
byte sdl_ui_text_fg_attr(byte attr);
byte sdl_ui_text_bg_attr(byte attr);
SDL_Color sdl_color_from_attr(byte attr);
void sdl_render_health_bar_rect(const SDL_FRect* rect, byte level,
    byte fill_attr);
void sdl_fill_cell_span_with_attr(sdl_view* d, int x, int y, int n, byte attr);
void sdl_render_story_row_packed(sdl_view* d, TTF_Font* font, int y, const byte* story_row, const char* row_chars, const byte* row_attr);
void sdl_render_story_text_grid(sdl_view* d, TTF_Font* font, int x, int y, int n, const char* s, SDL_Color col);

sdl_view* sdl_view_from_term(term* t);
void sdl_view_destroy(sdl_view* d);
void resize(const SDL_Rect* screen);
bool steamdeck_controls_active(void);
bool sdl_rect_has_area(const SDL_Rect* rect);
SDL_Rect sdl_get_window_pixel_rect(void);
SDL_Rect sdl_window_rect_to_pixel_rect(const SDL_Rect* rect);
#if defined(SDL_PLATFORM_ANDROID)
int sdl_android_sdk_int(JNIEnv* env);
SDL_Rect sdl_get_android_display_cutout_rect(void);
#endif
void sdl_refresh_safe_area(void);
SDL_Rect sdl_get_layout_screen_rect(void);
void sdl_log_mouse_devices(void);
bool sdl_mobile_prefer_safe_edge_alignment(void);
void sdl_resize_for_current_layout(void);
bool sdl_finger_event_to_render_coords(const SDL_TouchFingerEvent* finger,
    float* out_x, float* out_y);
void sdl_handle_event(sdl_state* st, SDL_Event* ev);
bool sdl_try_handle_touch_mouse_fallback_event(sdl_state* st,
    const SDL_Event* ev);
#if SIL_SDL_MOBILE_BUILD
bool sdl_mobile_lifecycle_handle_event(const SDL_Event* ev);
bool SDLCALL sdl_mobile_lifecycle_event_watch(void* userdata, SDL_Event* ev);
void sdl_mobile_lifecycle_register(void);
void sdl_mobile_lifecycle_unregister(void);
#endif
void sdl_quit_hook(cptr str);
errr callback_sdl_xtra(int n, int v);
void sdl_gamepad_init(void);
void sdl_gamepad_shutdown(void);
void sdl_gamepad_handle_button(const SDL_GamepadButtonEvent* ev);
void sdl_gamepad_handle_axis(const SDL_GamepadAxisEvent* ev);
void sdl_gamepad_handle_device(const SDL_GamepadDeviceEvent* ev);
void sdl_gamepad_open(SDL_JoystickID id);
void sdl_gamepad_close(SDL_JoystickID id);
void sdl_gamepad_mark_auto_ui(void);
int sdl_gamepad_axis_to_dir(Sint16 x, Sint16 y, int deadzone);
int sdl_gamepad_axis_to_cardinal_dir(Sint16 x, Sint16 y, int deadzone);
void sdl_gamepad_send_direction(int dir);
void sdl_gamepad_send_direction_mods(int dir, bool shift, bool ctrl, bool alt);
void sdl_gamepad_send_key(int key, bool use_macro_mods);
void sdl_gamepad_send_key_raw(int key);
bool sdl_gamepad_action_is_confirm(int binding);
void sdl_send_macro_key(int key, bool shift, bool ctrl, bool alt);
int sdl_keymap_mode(void);
int sdl_shifted_ascii_for_key(int key);
char sdl_direction_char_for_key(int key);
int sdl_direction_for_key_char(char ch);
bool sdl_send_modified_direction_action(int dir, char dir_ch, bool shift, bool ctrl, bool alt,
    bool gui);
bool sdl_try_send_modified_direction_key(int key, bool shift, bool ctrl, bool alt, bool gui);
bool sdl_try_send_movement_event(const SDL_KeyboardEvent* key_event);
bool sdl_try_send_shadowed_command_event(const SDL_KeyboardEvent* key_event);
bool sdl_try_send_preset_command_alias(const SDL_KeyboardEvent* key_event);
bool sdl_try_send_modified_direction_event(const SDL_KeyboardEvent* key_event);
bool sdl_handle_global_layout_shortcut(const SDL_KeyboardEvent* key_event);
void sdl_gamepad_apply_modifier(int binding, bool down);
bool sdl_gamepad_shift_active(void);
bool sdl_gamepad_ctrl_active(void);
bool sdl_gamepad_alt_active(void);
int sdl_gamepad_modifier_index(int binding);
int sdl_gamepad_single_active_modifier(void);
int sdl_gamepad_combo_binding_for_input(int modifier, int type, int id);
void sdl_gamepad_clear_pending_dpad(void);
void sdl_gamepad_set_pending_dpad(int dir);
bool sdl_gamepad_flush_pending_dpad(Uint64 now_ns, bool force);
void sdl_gamepad_clear_pending_left_stick(void);
void sdl_gamepad_set_pending_left_stick(int dir);
bool sdl_gamepad_flush_pending_left_stick(Uint64 now_ns, bool force);
void sdl_gamepad_clear_pending_shoulder(void);
void sdl_gamepad_set_pending_shoulder(int button);
bool sdl_gamepad_flush_pending_shoulder(Uint64 now_ns, bool force);
bool sdl_gamepad_resolve_pending_shoulder_with_modifier(int binding);
void sdl_gamepad_clear_pending_confirm(void);
bool sdl_gamepad_handle_confirm_long_press_button(
    int button, int binding, bool down);
int sdl_gamepad_pending_confirm_timeout_ms(Uint64 now_ns);
bool sdl_gamepad_flush_pending_confirm(Uint64 now_ns);
int sdl_gamepad_capture_binding_for_input(int type, int id);
bool sdl_gamepad_capture_queue_input(int type, int id);
int sdl_gamepad_pending_timeout_ms(Uint64 now_ns);
void sdl_apply_story_font_state(bool active);
void sdl_apply_story_grid_state(bool grid);
void sdl_apply_story_slot_state(int slot);
void sdl_story_font_reset_state(void);
void sdl_story_font_cache_clear(void);
bool sdl_story_font_cache_matches_config(void);
void sdl_story_font_cache_mark_config(void);
void sdl_mono_font_cache_clear(void);
void sdl_mono_font_prewarm_job_shutdown(void);
void sdl_mono_font_prewarm_process_idle(void);
void sdl_queue_mono_font_atlas_prewarm_cells(const char* font_path,
    int cell_width, int cell_height);
void sdl_queue_main_view_scale_neighbors_prewarm(const char* reason);
const char* sdl_monospace_font_path(void);
SDL_Texture* sdl_acquire_mono_font_atlas_cells_ex(
    const char* font_path, int cell_width, int cell_height, bool* out_cached,
    int* out_atlas_cell_w, int* out_atlas_cell_h, bool* out_exact,
    bool allow_fallback);
TTF_Font* sdl_acquire_mono_font_cells(const char* font_path,
    int cell_width, int cell_height);
TTF_Font* sdl_story_font_for_height(int pixel_height);
TTF_Font* sdl_story_font_for_view(const sdl_view* d);
#if SIL_SDL_MOBILE_BUILD
void sdl_ensure_default_pane_configs_present(bool enable_new_panes);
#endif
void sdl_ensure_default_pane_profiles_present(bool enable_new_panes);
void sdl_ensure_touch_pane_config_present(void);
bool sdl_touch_only_mobile_device_active(void);
bool sdl_touch_pane_uses_mobile_toggle(void);
bool sdl_touch_pane_mobile_layout_open(void);
bool sdl_touch_pane_proto_mode_active(void);
bool sdl_touch_pane_proto_slot_allowed(int slot);
bool sdl_touch_pane_slot_visible_in_current_mode(int slot);
int sdl_touch_pane_proto_binding_for_slot(int slot);
void sdl_touch_pane_refresh_after_layout_toggle(void);
void sdl_touch_pane_set_mobile_open(bool open);
bool sdl_touch_pane_panel_is_valid(int panel);
int sdl_touch_pane_active_panel(void);
int sdl_touch_pane_other_panel(int panel);
int sdl_touch_pane_raw_binding_for_panel(int panel, int index);
int sdl_touch_pane_effective_binding_for_panel(int panel, int index);
bool sdl_touch_pane_point_to_slot(float x, float y, int* out_slot);
bool sdl_touch_pane_current_rect(SDL_Rect* out_rect);
bool sdl_touch_pane_compute_layout(const SDL_Rect* pane_rect, SDL_FRect* slot_rects);
float sdl_touch_pane_clampf(float value, float min_value, float max_value);
int sdl_touch_pane_story_text_width(TTF_Font* font, cptr text);
int sdl_touch_pane_yes_no_prompt_font_px(float cell_h, int screen_h);
int sdl_touch_pane_wrap_prompt_lines(cptr text, TTF_Font* font,
    float max_w, char lines[][SDL_TOUCH_YES_NO_LINE_LEN], int max_lines);
bool sdl_touch_pane_yes_no_prompt_layout(SDL_FRect* panel_rect,
    SDL_FRect* prompt_rect, SDL_FRect* yes_rect, SDL_FRect* no_rect);
bool sdl_touch_pane_handle_yes_no_prompt_hover(float x, float y);
bool sdl_touch_pane_handle_yes_no_prompt_pointer(float x, float y);
bool sdl_touch_pane_binding_is_direction(int binding);
bool sdl_touch_pane_slot_uses_long_press(int slot, int binding);
bool sdl_touch_pane_confirm_binding(int binding);
bool sdl_touch_pane_main_panel_has_confirm_excluding(int skip_index);
void sdl_touch_pane_ensure_main_panel_confirm(void);
void sdl_touch_pane_begin_reset_confirm(void);
void sdl_touch_pane_finish_reset_confirm(bool confirmed);
void sdl_touch_pane_handle_reset_prompt_pointer(float x, float y);
void sdl_touch_pane_draw_arrow(const SDL_FRect* rect, int binding, SDL_Color color);
void sdl_touch_pane_draw_button_text_scaled(const SDL_FRect* rect, const char* name,
    const char* symbol, SDL_Color color, float single_text_font_ratio,
    float single_text_height_ratio);
void sdl_touch_pane_draw_button_text_px(const SDL_FRect* rect, const char* name,
    const char* symbol, SDL_Color color, int name_px, int symbol_px);
void sdl_touch_pane_draw_button_text(const SDL_FRect* rect, const char* name, const char* symbol,
    SDL_Color color);
void sdl_touch_pane_binding_symbol(int binding, char* buf, size_t buflen);
bool sdl_touch_pane_label_is_symbol_only(const char* label);
bool sdl_touch_pane_should_hide_symbol(const char* name, const char* symbol);
void sdl_touch_pane_render_reset_prompt(void);
void sdl_touch_pane_draw_wrapped_prompt(const SDL_FRect* rect,
    cptr text, SDL_Color color, int font_px);
void sdl_touch_pane_render_yes_no_prompt(void);
void sdl_touch_pane_default_label_for_panel_slot(int panel, int index, char* buf, size_t buflen);
void sdl_touch_pane_proto_label_for_slot(int index, char* buf, size_t buflen);
void sdl_touch_pane_base_label_for_slot(int panel, int index, char* buf, size_t buflen);
void sdl_touch_pane_display_label_for_slot(int panel, int index, char* buf, size_t buflen);
void sdl_touch_pane_render(void);
void sdl_unified_look_prompt_clear(void);
void sdl_unified_look_prompt_begin(int anchor_row);
void sdl_unified_look_prompt_add(int choice, cptr full, cptr medium,
    cptr compact, cptr tiny);
void sdl_unified_look_prompt_finish(void);
void sdl_unified_look_sidebar_clear(void);
void sdl_unified_look_sidebar_begin(bool compact, bool has_selection,
    int selected_choice);
void sdl_unified_look_sidebar_add_header(cptr text);
void sdl_unified_look_sidebar_add_entry(int choice, int entity_type, int y,
    int x, byte symbol_attr, byte text_attr, cptr symbol, cptr text);
void sdl_unified_look_sidebar_finish(void);
void sdl_unified_look_prompt_render(void);
void sdl_unified_look_sidebar_render(void);
bool sdl_unified_look_prompt_handle_pointer(float x, float y, int action);
bool sdl_unified_look_prompt_handle_hover_pointer(float x, float y);
bool sdl_unified_look_sidebar_handle_pointer(float x, float y, int action);
bool sdl_unified_look_sidebar_handle_hover_pointer(float x, float y);
void sdl_song_menu_render(void);
bool sdl_song_menu_handle_pointer(float x, float y, int action);
bool sdl_song_menu_handle_hover_pointer(float x, float y);
void sdl_question_menu_render(void);
bool sdl_question_menu_handle_pointer(float x, float y, int action);
bool sdl_question_menu_handle_hover_pointer(float x, float y);
void sdl_hint_quest_menu_render(void);
bool sdl_hint_quest_menu_handle_event(const SDL_Event* ev);
int sdl_hint_quest_menu_pending_timeout_ms(Uint64 now_ns);
bool sdl_question_menu_handle_touch_down(float x, float y,
    SDL_FingerID finger_id);
bool sdl_question_menu_handle_touch_motion(float x, float y,
    SDL_FingerID finger_id);
bool sdl_question_menu_handle_touch_up(float x, float y,
    SDL_FingerID finger_id);
void sdl_question_menu_cancel_touch(void);
void sdl_question_menu_set_scroll_offset_target(int* offset,
    bool follow_highlight);
bool sdl_question_menu_take_touch_scrolled(void);
void sdl_question_menu_set_blocking_input(bool blocking);
bool sdl_question_menu_blocks_input(void);
bool sdl_question_menu_captures_pointer(void);
void sdl_question_menu_clear_nonblocking(void);
void sdl_question_menu_set_nonblocking(bool nonblocking);
void sdl_question_menu_set_timeout_ms(int ms);
int sdl_question_menu_pending_timeout_ms(Uint64 now_ns);
bool sdl_question_menu_flush_expired(Uint64 now_ns);
void sdl_question_menu_add_button(int choice, cptr text, byte attr);
bool sdl_map_grid_cell_rect(int y, int x, SDL_FRect* out);
bool sdl_grid_question_queue(int map_y, int map_x);
bool sdl_grid_question_take_command(int* command, int* dir);
bool sdl_touch_pane_handle_pointer_down(float x, float y, bool mouse, SDL_FingerID finger_id);
bool sdl_touch_pane_handle_pointer_motion(float x, float y, bool mouse, SDL_FingerID finger_id);
void sdl_touch_pane_handle_pointer_up(float x, float y, bool mouse, SDL_FingerID finger_id);
void sdl_touch_pane_cancel_press(void);
int sdl_touch_pane_pending_timeout_ms(Uint64 now_ns);
bool sdl_touch_pane_flush_pending_press(Uint64 now_ns);
void sdl_touch_pane_send_slot(int panel, int index, bool long_press);
void sdl_touch_pane_send_binding(int binding, bool second_panel, bool long_press);
void sdl_touch_pane_load_default_bindings(void);
bool sdl_touch_pane_is_config_enabled(void);
bool sdl_touch_pane_is_left_placement(void);
bool sdl_main_screen_handle_menu_text_pointer(float x, float y, int action);
bool sdl_main_screen_handle_menu_outside_pointer(float x, float y,
    bool primary);
bool sdl_main_screen_handle_menu_hover_pointer(float x, float y);
bool sdl_main_screen_menu_pointer_hits_cell(float x, float y);
bool sdl_main_screen_menu_outside_armor_cycle_pointer(int col, int row);
void sdl_main_screen_touch_zone_selection_set(int status_action,
    int status_col, int panel_action, int panel_row, bool redraw);
void sdl_enqueue_bypassed_command(int command);
int sdl_inventory_equipment_cycle_binding(int binding);
void sdl_unified_look_clear_map_hover(void);
bool sdl_unified_look_handle_map_drag_down(float x, float y,
    bool mouse, SDL_FingerID finger_id);
bool sdl_unified_look_handle_map_drag_motion(float x, float y,
    bool mouse, SDL_FingerID finger_id);
bool sdl_unified_look_handle_map_drag_up(float x, float y,
    bool mouse, SDL_FingerID finger_id);
bool sdl_unified_look_handle_map_zoom_wheel(
    const SDL_MouseWheelEvent* wheel);
void sdl_unified_look_cancel_map_drag(void);
bool sdl_unified_look_handle_map_hover_pointer(float x, float y);
bool sdl_unified_look_handle_map_describe_pointer(float x, float y);
bool sdl_unified_look_handle_map_target_pointer(float x, float y);
void sdl_main_map_cancel_drag(void);
bool sdl_main_map_handle_drag_down(float x, float y,
    bool mouse, SDL_FingerID finger_id);
bool sdl_main_map_handle_drag_motion(float x, float y,
    bool mouse, SDL_FingerID finger_id);
bool sdl_main_map_handle_drag_up(float x, float y,
    bool mouse, SDL_FingerID finger_id);
bool sdl_main_map_handle_zoom_wheel(
    const SDL_MouseWheelEvent* wheel);
bool sdl_main_screen_adjust_main_view_scale(int delta);
void sdl_apply_runtime_zoom(void);
bool sdl_main_menu_pane_current_rect(SDL_FRect* out);
bool sdl_main_menu_pane_handle_hover_pointer(float x, float y);
bool sdl_main_menu_pane_handle_pointer(float x, float y);
void sdl_main_menu_pane_render(void);
bool sdl_quit_transition_active(void);
bool sdl_depth_menu_pane_current_rect(SDL_FRect* out);
bool sdl_depth_menu_pane_handle_hover_pointer(float x, float y);
bool sdl_depth_menu_pane_handle_pointer(float x, float y);
void sdl_depth_menu_pane_render(void);
bool sdl_main_screen_handle_status_line_hover_pointer(float x, float y);
bool sdl_main_screen_handle_character_panel_hover_pointer(float x, float y);
bool sdl_main_screen_handle_status_line_pointer(float x, float y);
bool sdl_screen_segment_col_hits_ci(const term* t, int row,
    int start_col, int width, int hit_col, cptr needle);
unsigned char sdl_screen_char_at(const term* t, int row, int col);
bool sdl_welcome_touch_handle_pointer_down(float x, float y, SDL_FingerID finger_id);
bool sdl_welcome_touch_handle_pointer_motion(float x, float y, SDL_FingerID finger_id);
bool sdl_welcome_touch_handle_pointer_up(float x, float y, SDL_FingerID finger_id);
void sdl_welcome_touch_cancel_press(void);
bool sdl_welcome_screen_cycle_intro(int direction);
bool sdl_welcome_screen_handle_pointer_motion(float x, float y);
bool sdl_pointer_activate_welcome_screen_at(float x, float y);
bool sdl_pointer_activate_welcome_screen(void);
bool sdl_point_in_frect(const SDL_FRect* rect, float x, float y);
bool sdl_character_sheet_screen_handle_pointer_motion(float x, float y);
bool sdl_character_sheet_screen_handle_pointer_button(float x, float y,
    int action);
bool sdl_character_sheet_screen_handle_pointer_event(const SDL_Event* ev);
bool sdl_pointer_dismiss_any_key_prompt(void);
bool sdl_menu_touch_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id, bool mouse);
bool sdl_menu_touch_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id, bool mouse);
bool sdl_menu_touch_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id, bool mouse);
void sdl_menu_touch_cancel(void);
int sdl_menu_touch_pending_timeout_ms(Uint64 now_ns);
bool sdl_menu_touch_flush_pending_press(Uint64 now_ns);
bool sdl_character_panel_handle_pointer_down(float x, float y,
    bool mouse, SDL_FingerID finger_id);
bool sdl_character_panel_handle_pointer_motion(float x, float y,
    bool mouse, SDL_FingerID finger_id);
bool sdl_character_panel_handle_pointer_up(float x, float y,
    bool mouse, SDL_FingerID finger_id);
void sdl_character_panel_cancel_press(void);
int sdl_character_panel_pending_timeout_ms(Uint64 now_ns);
bool sdl_character_panel_flush_pending_press(Uint64 now_ns);
bool sdl_screen_back_gesture_handle_event(const SDL_Event* ev);
int sdl_screen_back_gesture_pending_timeout_ms(Uint64 now_ns);
bool sdl_screen_back_gesture_flush_pending_press(Uint64 now_ns);
bool sdl_menu_scroll_handle_mouse_wheel(const SDL_MouseWheelEvent* wheel);
bool sdl_menu_scroll_handle_mouse_button(float x, float y);
bool sdl_menu_scroll_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id);
bool sdl_menu_scroll_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id);
bool sdl_menu_scroll_handle_pointer_up(SDL_FingerID finger_id);
void sdl_menu_scroll_cancel(void);
bool sdl_description_overlay_handle_mouse_wheel(
    const SDL_MouseWheelEvent* wheel);
bool sdl_description_overlay_contains_point(float x, float y);
bool sdl_side_map_pane_current_rect(SDL_Rect* out_rect);
void sdl_side_map_pane_render(void);
bool sdl_side_map_pane_handle_mouse_wheel(
    const SDL_MouseWheelEvent* wheel);
bool sdl_side_map_pane_handle_pointer_down(float x, float y,
    bool mouse, SDL_FingerID finger_id);
bool sdl_side_map_pane_handle_pointer_motion(float x, float y,
    bool mouse, SDL_FingerID finger_id);
bool sdl_side_map_pane_handle_pointer_up(float x, float y,
    bool mouse, SDL_FingerID finger_id);
void sdl_side_map_pane_cancel_pointer(SDL_FingerID finger_id,
    bool mouse);
bool sdl_minimap_handle_event(const SDL_Event* ev);
bool sdl_minimap_handle_gamepad_button(SDL_GamepadButton button,
    bool down);
bool sdl_minimap_handle_gamepad_axis(const SDL_GamepadAxisEvent* ev);
bool sdl_minimap_redraw(void);
bool sdl_minimap_has_hint_source_at(int y, int x);
bool sdl_minimap_hint_source_valid(const hint_message_meta* meta);
bool sdl_main_screen_click_shortcuts_active(void);
bool sdl_mouse_gameplay_context_active(void);
bool sdl_main_view_point_to_map(float x, float y, int* out_y, int* out_x);
bool sdl_player_map_rect(int y, int x, SDL_FRect* out_rect);
bool sdl_mouse_grid_has_visible_monster(int y, int x, int* out_m_idx);
bool sdl_mouse_feature_action_for_grid(int map_y, int map_x,
    int* out_command, int* out_dir);
bool sdl_mouse_feature_action_queue_grid(int map_y, int map_x);
bool sdl_object_tooltip_show_grid(int map_y, int map_x, bool touch);
bool sdl_object_tooltip_show_text_at_cell(int col, int row, int cols,
    cptr text, bool touch);
bool sdl_mouse_grid_has_describable_content(int y, int x);
bool sdl_mouse_path_compute(int target_y, int target_x);
bool sdl_mouse_stuck_door_bash_queue_prompt(int map_y, int map_x);
bool sdl_mouse_stuck_door_bash_take_command(int* command, int* dir);
int sdl_mouse_movement_normalized_mode(int mode);
bool sdl_main_screen_cell_hits_character_panel(int col, int row);
int sdl_visible_character_panel_click_action_at_cell(int col, int row);
void sdl_player_confirm_at_player(void);
bool sdl_player_action_menu_open(void);
void sdl_player_action_menu_activate_kind(int kind, bool secondary);
bool sdl_player_action_menu_handle_gamepad_button(
    SDL_GamepadButton button, bool down);
bool sdl_player_action_menu_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id, bool mouse, bool secondary);
bool sdl_player_action_menu_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id, bool mouse);
bool sdl_player_action_menu_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id, bool mouse, bool secondary);
int sdl_player_action_menu_pending_timeout_ms(Uint64 now_ns);
bool sdl_player_action_menu_flush_pending_press(Uint64 now_ns);
void sdl_player_action_menu_cancel_press(void);
void sdl_player_action_menu_cancel(void);
bool sdl_player_exchange_handle_gamepad_button(
    SDL_GamepadButton button, bool down);
bool sdl_player_exchange_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id, bool mouse);
bool sdl_player_exchange_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id, bool mouse);
bool sdl_player_exchange_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id, bool mouse);
void sdl_player_exchange_cancel(void);
void sdl_player_action_menu_render(void);
void sdl_player_exchange_render(void);
bool sdl_pointer_attack_binding_toggled(int binding);
bool sdl_pointer_attack_toggle_binding(int binding);
bool sdl_pointer_attack_handle_motion(float x, float y);
bool sdl_pointer_attack_handle_left_click(float x, float y);
bool sdl_pointer_attack_handle_touch_down(float x, float y,
    SDL_FingerID finger_id);
bool sdl_pointer_attack_handle_touch_motion(float x, float y,
    SDL_FingerID finger_id);
bool sdl_pointer_attack_handle_touch_up(float x, float y,
    SDL_FingerID finger_id);
int sdl_pointer_attack_pending_timeout_ms(Uint64 now_ns);
bool sdl_pointer_attack_flush_pending_press(Uint64 now_ns);
void sdl_pointer_attack_cancel_touch_press(void);
void sdl_pointer_attack_set_panel_hover_mode(int mode);
void sdl_pointer_attack_render(void);
bool sdl_pointer_aim_handle_motion(float x, float y);
bool sdl_pointer_aim_handle_left_click(float x, float y);
bool sdl_pointer_aim_handle_left_release(float x, float y);
bool sdl_pointer_aim_handle_touch_down(float x, float y,
    SDL_FingerID finger_id);
bool sdl_pointer_aim_handle_touch_motion(float x, float y,
    SDL_FingerID finger_id);
bool sdl_pointer_aim_handle_touch_up(float x, float y,
    SDL_FingerID finger_id);
void sdl_pointer_aim_render(void);
void sdl_mouse_path_handle_motion(float x, float y);
bool sdl_mouse_path_handle_left_click(float x, float y);
bool sdl_mouse_path_handle_right_movement_click(float x, float y);
bool sdl_mouse_recall_handle_right_click(float x, float y);
void sdl_mouse_path_render(void);
bool sdl_pointer_attack_take_command(int* command, int* dir);
bool sdl_pointer_aim_take_direction(int* dir);
bool sdl_mouse_path_take_step_command(int* command, int* dir);
bool sdl_mouse_recall_process_pending(void);
void sdl_mouse_path_cancel(void);
void sdl_object_tooltip_clear(void);
bool sdl_map_touch_handle_pointer_down(float x, float y, SDL_FingerID finger_id);
bool sdl_map_touch_handle_pointer_motion(float x, float y, SDL_FingerID finger_id);
bool sdl_map_touch_handle_pointer_up(float x, float y, SDL_FingerID finger_id);
void sdl_map_touch_cancel_press(void);
int sdl_map_touch_pending_timeout_ms(Uint64 now_ns);
bool sdl_map_touch_flush_pending_press(Uint64 now_ns);
bool sdl_main_screen_handle_character_panel_pointer(float x, float y);
bool sdl_main_screen_handle_character_panel_secondary_pointer(float x, float y);
bool sdl_main_screen_handle_supporting_pane_pointer(float x, float y);
bool sdl_pane_layout_drag_handle_pointer_down(float x, float y);
bool sdl_pane_layout_drag_handle_pointer_motion(float x, float y);
bool sdl_pane_layout_drag_handle_pointer_up(float x, float y);
void sdl_pane_layout_drag_cancel(void);
bool sdl_log_pane_menu_open_from_pointer(float x, float y);
bool sdl_log_history_filter_is_valid(int filter);
void sdl_log_pane_sync_display_filter_from_config(void);
bool sdl_log_pane_menu_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id, bool mouse);
bool sdl_log_pane_menu_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id, bool mouse);
bool sdl_log_pane_menu_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id, bool mouse);
bool sdl_log_pane_menu_handle_long_press_down(float x, float y,
    SDL_FingerID finger_id);
bool sdl_log_pane_menu_handle_long_press_motion(float x, float y,
    SDL_FingerID finger_id);
bool sdl_log_pane_menu_handle_long_press_up(float x, float y,
    SDL_FingerID finger_id);
void sdl_log_pane_menu_cancel_long_press(SDL_FingerID finger_id);
int sdl_log_pane_menu_pending_timeout_ms(Uint64 now_ns);
bool sdl_log_pane_menu_flush_pending_press(Uint64 now_ns);
void sdl_log_pane_menu_cancel(void);
void sdl_log_pane_menu_render(void);
bool sdl_side_pane_menu_open_from_pointer(float x, float y);
bool sdl_side_pane_menu_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id, bool mouse);
bool sdl_side_pane_menu_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id, bool mouse);
bool sdl_side_pane_menu_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id, bool mouse);
bool sdl_side_pane_menu_handle_long_press_down(float x, float y,
    SDL_FingerID finger_id);
bool sdl_side_pane_menu_handle_long_press_motion(float x, float y,
    SDL_FingerID finger_id);
bool sdl_side_pane_menu_handle_long_press_up(float x, float y,
    SDL_FingerID finger_id);
void sdl_side_pane_menu_cancel_long_press(SDL_FingerID finger_id);
int sdl_side_pane_menu_pending_timeout_ms(Uint64 now_ns);
bool sdl_side_pane_menu_flush_pending_press(Uint64 now_ns);
void sdl_side_pane_menu_cancel(void);
void sdl_side_pane_menu_render(void);
int sdl_touch_swipe_index_for_keypad_dir(int dir);
float sdl_touch_swipe_threshold_px(void);
int sdl_touch_swipe_direction_for_delta(float dx, float dy, float threshold);
void sdl_touch_swipe_cancel(void);
bool sdl_touch_swipe_handle_pointer_down(float x, float y, SDL_FingerID finger_id);
bool sdl_touch_swipe_handle_pointer_motion(float x, float y, SDL_FingerID finger_id);
void sdl_touch_swipe_handle_pointer_up(float x, float y, SDL_FingerID finger_id);
int sdl_touch_zone_overlay_mode_normalized(int mode);
bool sdl_touch_tutorial_device_available(void);
bool sdl_touch_tutorial_full_mode(void);
bool sdl_touch_zone_overlay_visible(void);
bool sdl_touch_zone_layout_visible(void);
bool sdl_touch_zone_controls_active(void);
bool sdl_touch_zone_compute_layout_for_screen(const SDL_Rect* screen,
    SDL_FRect* zone_rects);
int sdl_touch_zone_center_binding_index(int zone, bool long_press);
int sdl_touch_zone_binding_for_center(int zone, bool long_press);
void sdl_touch_zone_button_label(int zone, char* name, size_t name_len,
    char* symbol, size_t symbol_len);
bool sdl_touch_zone_handle_pointer_down(float x, float y, SDL_FingerID finger_id);
bool sdl_touch_zone_handle_pointer_motion(float x, float y, SDL_FingerID finger_id);
bool sdl_touch_zone_handle_pointer_up(float x, float y, SDL_FingerID finger_id);
void sdl_touch_zone_cancel_press(void);
int sdl_touch_zone_pending_timeout_ms(Uint64 now_ns);
bool sdl_touch_zone_flush_pending_press(Uint64 now_ns);
void sdl_touch_zone_render_markers(void);
bool sdl_touch_round_point_excluded(float x, float y);
bool sdl_touch_round_layer_controls_active(void);
bool sdl_touch_round_layer_config_enabled(void);
bool sdl_touch_movement_point_blocked_by_overlay(float x, float y);
bool sdl_touch_round_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id);
bool sdl_touch_round_handle_pointer_motion(float x, float y,
    SDL_FingerID finger_id);
bool sdl_touch_round_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id);
void sdl_touch_round_cancel_press(void);
void sdl_touch_round_render(void);
bool sdl_touch_round_compute_layout(float* out_cx, float* out_cy,
    float* out_radius, float* out_inner_radius, SDL_Rect* out_clip);
void sdl_touch_round_render_target_square(int dir, bool ctrl, bool run);
int sdl_touch_profile_normalized(int profile);
void sdl_touch_hidden_indicator_render(void);
bool sdl_touch_hidden_indicator_handle_pointer_down(float x, float y, bool touch);
bool sdl_touch_top_panel_layout_visible(void);
void sdl_touch_top_panel_set_open(bool open);
float sdl_touch_top_panel_size_normalized(float size);
int sdl_touch_top_panel_cell_count_normalized(int count);
int sdl_touch_top_panel_columns_normalized(int columns);
int sdl_touch_top_panel_rows_normalized(int rows);
bool sdl_touch_top_panel_current_anchor(SDL_Rect* out_screen,
    SDL_Rect* out_anchor, enum pane_placement* out_where);
bool sdl_touch_top_panel_compute_layout_for_anchor(const SDL_Rect* screen,
    const SDL_Rect* anchor, enum pane_placement where,
    SDL_FRect* button_rects, SDL_FRect* out_panel);
bool sdl_touch_top_panel_compute_layout_for_screen(
    const SDL_Rect* screen, SDL_FRect* button_rects, SDL_FRect* out_panel);
bool sdl_touch_top_panel_compute_layout(SDL_FRect* button_rects,
    SDL_FRect* out_panel);
bool sdl_touch_top_panel_compute_layout_for_display(SDL_FRect* button_rects,
    SDL_FRect* out_panel);
void sdl_touch_top_panel_render_buttons(
    const SDL_FRect* button_rects);
bool sdl_touch_top_panel_point_to_slot(float x, float y, int* out_slot);
bool sdl_touch_top_panel_pointer_claims_point(float x, float y);
bool sdl_touch_top_panel_handle_secondary_pointer(float x, float y);
void sdl_touch_top_panel_render(void);
bool sdl_touch_top_panel_handle_pointer_down(float x, float y,
    SDL_FingerID finger_id);
bool sdl_touch_top_panel_handle_pointer_motion(float x, float y,
    bool mouse, SDL_FingerID finger_id);
bool sdl_touch_top_panel_handle_pointer_up(float x, float y,
    SDL_FingerID finger_id);
void sdl_touch_top_panel_cancel_press(void);
int sdl_touch_top_panel_pending_timeout_ms(Uint64 now_ns);
bool sdl_touch_top_panel_flush_pending_press(Uint64 now_ns);
void sdl_touch_cancel_all_inputs(void);
bool sdl_render_current_window_frame(void);
void sdl_restore_render_target(sdl_view* d);
void sdl_touch_tutorial_maybe_show_deferred(void);
void sdl_input_tutorial_maybe_show_deferred(void);
void sdl_touch_tutorial_run(bool full, bool mouse);
void sdl_touch_tutorial_run_fixed(void);
void sdl_touch_show_tutorial(void);
void sdl_mouse_show_tutorial(void);
int get_sdl_touch_pane_binding_for_panel(int panel, int index);
void set_sdl_touch_pane_binding_for_panel(int panel, int index, int binding);
int get_sdl_touch_pane_default_binding_for_panel(int panel, int index);
bool get_sdl_touch_pane_key_labels_visible(void);
void set_sdl_touch_pane_key_labels_visible(bool value);
bool get_sdl_touch_pane_key_labels_default_visible(void);
bool get_sdl_touch_pane_inventory_equipment_cycle(void);
void set_sdl_touch_pane_inventory_equipment_cycle(bool value);
bool get_sdl_touch_pane_inventory_equipment_default_cycle(void);
void get_sdl_touch_pane_button_label_for_panel(int panel, int index, char* buf, size_t buflen);
void set_sdl_touch_pane_button_label_for_panel(int panel, int index, cptr label);
void clear_sdl_touch_pane_button_label_for_panel(int panel, int index);
void get_sdl_touch_pane_panel_name(int panel, char* buf, size_t buflen);
void set_sdl_touch_pane_panel_name(int panel, cptr name);
void sdl_update_cursor_visibility(void);
void sdl_draw_pane_edges(const SDL_Rect* rect, bool draw_left,
    bool draw_top, bool draw_right, bool draw_bottom);
bool sdl_should_show_supporting_panes(void);
bool sdl_hide_supporting_panes_mode_effective(void);
bool sdl_layout_matches_supporting_pane_visibility(void);
bool sdl_status_pane_current_rect(SDL_Rect* out_rect,
    enum pane_placement* out_where);
void sdl_status_pane_render(void);
bool sdl_status_depth_pane_current_rect(SDL_FRect* out);
void sdl_status_depth_pane_render(void);
void sdl_narrative_banner_render(void);
void sdl_present_if_needed(sdl_view* d);
int sdl_build_active_pane_config(struct pane_config* active, bool include_side,
    bool include_bottom, bool touch_only);
int sdl_configured_main_view_scale(void);
int sdl_main_view_layout_scale(void);
int sdl_current_main_view_scale(void);
int sdl_auto_font_size_from_main(int numerator, int denominator);
int sdl_auto_aux_view_font_size(void);
int sdl_auto_pane_font_size(enum pane_type type);
int sdl_resolve_aux_view_font_size(int requested_size);
int sdl_resolve_pane_font_size(enum pane_type type, int requested_size);
int sdl_effective_pane_font_size_for_config(const struct pane_config* pc);
int sdl_effective_pane_font_size_for_type(enum pane_type type);
int sdl_aux_cell_height_for_font_size(int font_size);
int sdl_effective_pane_cell_height_for_config(const struct pane_config* pc);
int sdl_effective_pane_cell_height_for_type(enum pane_type type);
bool sdl_left_panel_pane_layout_enabled(void);
bool sdl_left_panel_pane_presentation_active(void);
bool sdl_left_panel_pane_runtime_active(void);
bool sdl_saved_screen_left_panel_pane_active(void);
bool sdl_left_panel_pane_collapsed(void);
enum pane_placement sdl_left_panel_pane_placement(void);
bool sdl_left_panel_compact_row_mode(void);
bool sdl_left_panel_pane_has_border_columns(void);
bool sdl_left_panel_pane_rect_for_metrics(const sdl_view* view,
    const sdl_left_panel_metrics* metrics, SDL_FRect* out_rect);
void sdl_left_panel_pane_cell_size_for_view(const sdl_view* view,
    int content_cols, int* out_cell_w, int* out_cell_h);
bool sdl_left_panel_metrics_for_view(const sdl_view* view,
    sdl_left_panel_metrics* metrics);
void sdl_update_left_panel_pane_rect(void);
int sdl_main_view_visual_cols(const sdl_view* view);
int sdl_main_view_visual_rows(const sdl_view* view);
int sdl_main_view_visual_cols_for_width(int width_px, int cell_w);
int sdl_main_view_logical_cols_for_visual_cols(int visual_cols);
bool sdl_term_get_size_hook(term* t, int* w, int* h);
bool sdl_main_cell_rect(int col, int row, int cols, int rows,
    SDL_FRect* out);
void sdl_build_supporting_pane_metrics(const struct pane_config* configs,
    int count, int* cell_widths, int* cell_heights);
bool sdl_prune_unusable_panes(struct pane_config* active,
    int active_count, SDL_Rect* panes, const int* cell_widths,
    const int* cell_heights);
void sdl_place_active_panes(const SDL_Rect* screen, SDL_Rect* panes,
    bool include_side, bool include_bottom, bool touch_only);
void sdl_compute_display_panes(SDL_Rect* panes);
int sdl_max_scale_for_rect(const SDL_Rect* rect);
int sdl_max_scale_for_rect_mode(const SDL_Rect* rect, int mode);
bool sdl_apply_default_main_scale_for_layout(const char* reason);
int sdl_max_scale_for_layout(const SDL_Rect* screen, int mode);
int sdl_max_main_view_zoom_scale_for_layout(const SDL_Rect* screen, int mode);
int sdl_max_scale_for_window_mode(int mode);
bool sdl_mode_scale_fits_window(const SDL_Rect* screen, int mode,
    int scale, int* cols, int* rows);
int get_sdl_min_main_view_zoom_scale(void);
void sdl_clamp_main_view_zoom_to_current_layout(void);
void sdl_ensure_window_size_for_min_terminal(const SDL_Rect* screen,
    int* window_width, int* window_height);
bool sdl_recover_layout_for_current_window(const char* reason,
    bool notify_user, sdl_layout_recovery_result* recovery);
void sdl_format_layout_recovery_message(const char* reason,
    const sdl_layout_recovery_result* recovery, char* buf, size_t buflen);
void sdl_append_issue_line(char* buf, size_t buflen, const char* line);
void sdl_reset_config_to_resolution_defaults(int screen_width,
    int screen_height);
bool sdl_prompt_reset_sdl_defaults(const char* issue_summary,
    int screen_width, int screen_height);
#if SIL_SDL_MOBILE_BUILD
bool sdl_prompt_mobile_startup_portrait_mode(void);
#endif
#if SIL_SDL_DESKTOP_HANDHELD_BUILD
bool sdl_is_desktop_handheld_resolution(int width, int height);
sdl_startup_device_class sdl_prompt_desktop_startup_input_device(
    int screen_width, int screen_height);
#endif
int sdl_touch_pane_target_width_px(int pane_height_px);
void sdl_apply_dynamic_auto_pane_sizes(struct pane_config* active,
    int active_count, const SDL_Rect* screen, const int* cell_widths,
    const int* cell_heights, int margin_px);
void sdl_touch_pane_send_confirm_action(void);
void sdl_render_mono_text(sdl_view* d, int x, int y, int n, const char* s, SDL_Color col);
void sdl_render_mono_utf8_text_cells(SDL_Texture* atlas,
    int atlas_cell_w, int atlas_cell_h, TTF_Font* font, float cell_w,
    float cell_h, float origin_x, int x, int y, int n, const char* s,
    SDL_Color col);
void sdl_render_mono_utf8_text_cells_at(SDL_Texture* atlas,
    int atlas_cell_w, int atlas_cell_h, TTF_Font* font, float cell_w,
    float cell_h, float origin_x, float origin_y, int x, int y, int n,
    const char* s, SDL_Color col);
void sdl_render_story_text_free(sdl_view* d, TTF_Font* font, int x, int y, int n, const char* s,
    SDL_Color col);
void sdl_render_story_text_grid(sdl_view* d, TTF_Font* font, int x, int y, int n, const char* s,
    SDL_Color col);
int sdl_render_story_text_free_px(sdl_view* d, TTF_Font* font, float x_px, int y, const char* s, int n,
    SDL_Color col,
    float max_w_px);
void sdl_render_story_row_packed(sdl_view* d, TTF_Font* font, int y, const byte* story_row,
    const char* row_chars, const byte* row_attr);
void draw_cursor(int x, int y, bool big);
errr callback_sdl_curs(int x, int y);
errr callback_sdl_bigcurs(int x, int y);
errr callback_sdl_wipe(int x, int y, int n);
errr callback_sdl_text(int x, int y, int n, byte a, cptr s);
errr callback_sdl_pict(int x, int y, int n, const byte* ap, const char* cp,
                       const byte* tap, const char* tcp);
void sdl_draw_map_tile_layers_at(int dy, int dx, byte a, char c,
    byte ta, char tc, const SDL_FRect* dst);
void callback_sdl_nuke();
void callback_sdl_init(term* t);
errr sdl_view_link_term(sdl_view* d, int term_index);
SDL_Texture* sdl_load_ttf_font_cells(const char* font_path,
    int cell_width, int cell_height, int* actual_font_size);
SDL_Texture* sdl_try_load_ttf_font_cells(const char* font_path,
    int cell_width, int cell_height, int* actual_font_size, char* error_buf,
    size_t error_buf_size);
void sdl_window_create(int window_width, int window_height, bool fullscreen, bool use_tiles);
void sdl_window_set_position(int x, int y);
bool sdl_view_create(sdl_view* d, SDL_Rect rect, const char* font_path, int font_size, int scale, int margin);
void sdl_load_story_fonts(void);
TTF_Font* sdl_load_font_with_fallback(const char* font_path, int font_size, const char* fallback_path);
void sdl_handle_renderer_reset(void);
void sdl_request_redraw(void);


#endif /* INCLUDED_SDL_MAIN_PRIVATE_H */
