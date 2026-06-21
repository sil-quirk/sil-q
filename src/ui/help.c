/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "ui/help.h"
#include "externs.h"
#include "log/log.h"
#include "sdl-config.h"
#include "sdl-sound.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#define HELP_TOTAL_PAGES 8

/* Drop-in replacement for show_help_screen(int i)
 * Adds a tiny role-based colour shim for consistent, accessible styling.
 *
 * Extras in this version:
 *  - Element-specific roles (darkness, poison, cold, fire, +light, lightning, acid)
 *  - Page 8: Steam Deck controls, with a drawn layout and dynamic key enum/mapping.
 *
 * Usage: paste this whole block where show_help_screen is defined. It only
 * depends on c_put_str() and the TERM_* colour constants already in your codebase.
 */

/* -------- Role-based colour shim ---------------------------------------- */

typedef enum {
    ROLE_HEADER,  /* Page title */
    ROLE_SECTION, /* Section headings */
    ROLE_BODY,    /* Main body text */
    ROLE_SUBTLE,  /* Hints/parentheticals */
    ROLE_GOOD,    /* Positive things, boons, successes */
    ROLE_WARN,    /* Caution/thresholds */
    ROLE_BAD,     /* Harmful/danger state */
    ROLE_TERM,    /* Game terms/keywords */
    ROLE_KEY,     /* Literal keys and glyphs in docs (NOT gameplay glyphs) */
    ROLE_UI,      /* Meta UI labels (menus, screens) */
    /* Element roles (so we can colour by element consistently) */
    ROLE_ELEM_FIRE,
    ROLE_ELEM_COLD,
    ROLE_ELEM_POISON,
    ROLE_ELEM_DARKNESS,
    ROLE_ELEM_LIGHT,
    ROLE__COUNT
} color_role_t;

/* Default theme (dark background). You can swap values at runtime if you add a menu hook. */
static int HELP_THEME[ROLE__COUNT] = {
    [ROLE_HEADER]       = TERM_L_WHITE + TERM_SHADE,
    [ROLE_SECTION]      = TERM_YELLOW,
    [ROLE_BODY]         = TERM_L_WHITE,
    [ROLE_SUBTLE]       = TERM_SLATE,
    [ROLE_GOOD]         = TERM_L_GREEN,
    [ROLE_WARN]         = TERM_ORANGE,
    [ROLE_BAD]          = TERM_L_RED,
    [ROLE_TERM]         = TERM_L_BLUE,
    [ROLE_KEY]          = TERM_WHITE,
    [ROLE_UI]           = TERM_UMBER,
    /* Elements per user request */
    [ROLE_ELEM_FIRE]        = TERM_L_RED,
    [ROLE_ELEM_COLD]        = TERM_BLUE,   /* cold -> blue */
    [ROLE_ELEM_POISON]      = TERM_L_GREEN,  /* poison -> green */
    [ROLE_ELEM_DARKNESS]    = TERM_L_DARK,   /* darkness -> dark */
    [ROLE_ELEM_LIGHT]       = TERM_YELLOW,   /* light/radiance */
};

static inline void put_role(color_role_t role, const char *s, int row, int col) {
    c_put_str(HELP_THEME[role], s, row, col);
}


void binding_action_label(int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    switch (binding) {
    case GAMEPAD_BIND_NONE:
        SDL_strlcpy(buf, "Unbound", buflen);
        return;
    case TOUCH_PANE_BIND_INHERIT:
        SDL_strlcpy(buf, "Main panel button", buflen);
        return;
    case TOUCH_BIND_TOP_PANEL_OPEN:
        SDL_strlcpy(buf, "Open quick access", buflen);
        return;
    case TOUCH_BIND_TOP_PANEL_CLOSE:
        SDL_strlcpy(buf, "Close quick access", buflen);
        return;
    case TOUCH_BIND_MAIN_MENU_KNOWLEDGE:
        SDL_strlcpy(buf, "Known lore", buflen);
        return;
    case TOUCH_BIND_MAIN_MENU_HINTS_QUESTS:
        SDL_strlcpy(buf, "Hints & Quests", buflen);
        return;
    case TOUCH_BIND_TOGGLE_TILES:
        SDL_strlcpy(buf, "Change ASCII / tiles", buflen);
        return;
    case GAMEPAD_BIND_SHIFT:
        SDL_strlcpy(buf, "Shift modifier", buflen);
        return;
    case GAMEPAD_BIND_CTRL:
        SDL_strlcpy(buf, "Ctrl modifier", buflen);
        return;
    case GAMEPAD_BIND_ALT:
        SDL_strlcpy(buf, "Alt modifier", buflen);
        return;
    case INPUT_BIND_CONFIRM:
        SDL_strlcpy(buf, "Confirm", buflen);
        return;
    case ' ':
        SDL_strlcpy(buf, "Confirm (Space)", buflen);
        return;
    case '\r':
        SDL_strlcpy(buf, "Enter", buflen);
        return;
    case ESCAPE:
        SDL_strlcpy(buf, "Back (Esc)", buflen);
        return;
    case '7':
        SDL_strlcpy(buf, "Move NW (7)", buflen);
        return;
    case '8':
        SDL_strlcpy(buf, "Move N (8)", buflen);
        return;
    case '9':
        SDL_strlcpy(buf, "Move NE (9)", buflen);
        return;
    case '4':
        SDL_strlcpy(buf, "Move W (4)", buflen);
        return;
    case '5':
        SDL_strlcpy(buf, "Wait / center (5)", buflen);
        return;
    case '6':
        SDL_strlcpy(buf, "Move E (6)", buflen);
        return;
    case '1':
        SDL_strlcpy(buf, "Move SW (1)", buflen);
        return;
    case '2':
        SDL_strlcpy(buf, "Move S (2)", buflen);
        return;
    case '3':
        SDL_strlcpy(buf, "Move SE (3)", buflen);
        return;
    case '\t':
        SDL_strlcpy(buf, "Active weapon (Tab)", buflen);
        return;
    case 'y':
        SDL_strlcpy(buf, "Abilities (y)", buflen);
        return;
    case 'i':
        SDL_strlcpy(buf, "Inventory (i)", buflen);
        return;
    case 'e':
        SDL_strlcpy(buf, "Equipment (e)", buflen);
        return;
    case 'u':
        SDL_strlcpy(buf, "Use item (u)", buflen);
        return;
    case 'x':
        SDL_strlcpy(buf, "Examine item (x)", buflen);
        return;
    case 's':
        SDL_strlcpy(buf, "Sing (s)", buflen);
        return;
    case 'S':
        SDL_strlcpy(buf, "Stealth (S)", buflen);
        return;
    case KTRL('A'):
        SDL_strlcpy(buf, "Swap staff (^A)", buflen);
        return;
    case KTRL('F'):
        SDL_strlcpy(buf, "Swap quivers (^F)", buflen);
        return;
    case 'f':
        SDL_strlcpy(buf, "Fire (f)", buflen);
        return;
    case 'F':
        SDL_strlcpy(buf, "Second quiver (F)", buflen);
        return;
    case 'h':
        SDL_strlcpy(buf, "Character sheet (h)", buflen);
        return;
    case 'l':
        SDL_strlcpy(buf, "Look (l)", buflen);
        return;
    case 'o':
        SDL_strlcpy(buf, "Open (o)", buflen);
        return;
    case 'q':
        SDL_strlcpy(buf, "Quaff (q)", buflen);
        return;
    case 'r':
        SDL_strlcpy(buf, "Remove (r)", buflen);
        return;
    case 'a':
        SDL_strlcpy(buf, "Activate (a)", buflen);
        return;
    case 'M':
        SDL_strlcpy(buf, "Map (M)", buflen);
        return;
    case 'b':
        SDL_strlcpy(buf, "Bash (b)", buflen);
        return;
    case 'j':
        SDL_strlcpy(buf, "Supplies (j)", buflen);
        return;
    case 'z':
        SDL_strlcpy(buf, "Wait (z)", buflen);
        return;
    case '.':
        SDL_strlcpy(buf, "Run (.)", buflen);
        return;
    case '/':
        SDL_strlcpy(buf, "Alt action (/)", buflen);
        return;
    case 'w':
        SDL_strlcpy(buf, "Wear / wield (w)", buflen);
        return;
    case 'g':
        SDL_strlcpy(buf, "Pick up items (g)", buflen);
        return;
    case 'Z':
        SDL_strlcpy(buf, "Rest (Z)", buflen);
        return;
    case 'c':
        SDL_strlcpy(buf, "Close door (c)", buflen);
        return;
    case 'D':
        SDL_strlcpy(buf, "Disarm trap / chest (D)", buflen);
        return;
    case 'X':
        SDL_strlcpy(buf, "Exchange places (X)", buflen);
        return;
    case '-':
        SDL_strlcpy(buf, "Fletch arrows (-)", buflen);
        return;
    case '{':
        SDL_strlcpy(buf, "Inscribe item ({)", buflen);
        return;
    case 'E':
        SDL_strlcpy(buf, "Eat food (E)", buflen);
        return;
    case 't':
        SDL_strlcpy(buf, "Throw item (t)", buflen);
        return;
    case 'p':
        SDL_strlcpy(buf, "Blow horn (p)", buflen);
        return;
    case 'L':
        SDL_strlcpy(buf, "Pan view (L)", buflen);
        return;
    case KTRL('Q'):
        SDL_strlcpy(buf, "Combat rolls (^Q)", buflen);
        return;
    case KTRL('Y'):
        SDL_strlcpy(buf, "Debug commands (^Y)", buflen);
        return;
    case 'J':
        SDL_strlcpy(buf, "Equip a jewel set (J)", buflen);
        return;
    case '0':
        SDL_strlcpy(buf, "Smithing screen (0)", buflen);
        return;
    case '<':
        SDL_strlcpy(buf, "Go upstairs (<)", buflen);
        return;
    case '>':
        SDL_strlcpy(buf, "Go downstairs (>)", buflen);
        return;
    case 'm':
        SDL_strlcpy(buf, "Main menu (m)", buflen);
        return;
    case '?':
        SDL_strlcpy(buf, "Help (?)", buflen);
        return;
    case '@':
        SDL_strlcpy(buf, "Character sheet (@)", buflen);
        return;
    case 'O':
        SDL_strlcpy(buf, "Options menu (O)", buflen);
        return;
    case ':':
        SDL_strlcpy(buf, "Take notes (:)", buflen);
        return;
    case '~':
        SDL_strlcpy(buf, "Knowledge browser (~)", buflen);
        return;
    case '[':
        SDL_strlcpy(buf, "Monster list ([)", buflen);
        return;
    case ']':
        SDL_strlcpy(buf, "Object list (])", buflen);
        return;
    default:
        if (binding >= 32 && binding <= 126)
            strnfmt(buf, buflen, "Key '%c'", binding);
        else
            strnfmt(buf, buflen, "Key %d", binding);
        return;
    }
}

void binding_action_short(int binding, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    switch (binding) {
    case GAMEPAD_BIND_NONE:
        SDL_strlcpy(buf, "Unbound", buflen);
        return;
    case TOUCH_PANE_BIND_INHERIT:
        SDL_strlcpy(buf, "Main", buflen);
        return;
    case TOUCH_BIND_TOP_PANEL_OPEN:
        SDL_strlcpy(buf, "Top Open", buflen);
        return;
    case TOUCH_BIND_TOP_PANEL_CLOSE:
        SDL_strlcpy(buf, "Top Close", buflen);
        return;
    case TOUCH_BIND_MAIN_MENU_KNOWLEDGE:
        SDL_strlcpy(buf, "Lore", buflen);
        return;
    case TOUCH_BIND_MAIN_MENU_HINTS_QUESTS:
        SDL_strlcpy(buf, "Hints", buflen);
        return;
    case TOUCH_BIND_TOGGLE_TILES:
        SDL_strlcpy(buf, "Change ASCII/Tiles", buflen);
        return;
    case GAMEPAD_BIND_SHIFT:
        SDL_strlcpy(buf, "Shift", buflen);
        return;
    case GAMEPAD_BIND_CTRL:
        SDL_strlcpy(buf, "Ctrl", buflen);
        return;
    case GAMEPAD_BIND_ALT:
        SDL_strlcpy(buf, "Alt", buflen);
        return;
    case INPUT_BIND_CONFIRM:
        SDL_strlcpy(buf, "Confirm", buflen);
        return;
    case ' ':
        SDL_strlcpy(buf, "Confirm", buflen);
        return;
    case '\r':
        SDL_strlcpy(buf, "Enter", buflen);
        return;
    case ESCAPE:
        SDL_strlcpy(buf, "Back", buflen);
        return;
    case '7':
        SDL_strlcpy(buf, "NW", buflen);
        return;
    case '8':
        SDL_strlcpy(buf, "N", buflen);
        return;
    case '9':
        SDL_strlcpy(buf, "NE", buflen);
        return;
    case '4':
        SDL_strlcpy(buf, "W", buflen);
        return;
    case '5':
        SDL_strlcpy(buf, "Wait", buflen);
        return;
    case '6':
        SDL_strlcpy(buf, "E", buflen);
        return;
    case '1':
        SDL_strlcpy(buf, "SW", buflen);
        return;
    case '2':
        SDL_strlcpy(buf, "S", buflen);
        return;
    case '3':
        SDL_strlcpy(buf, "SE", buflen);
        return;
    case '\t':
        SDL_strlcpy(buf, "Change active weapon", buflen);
        return;
    case 'y':
        SDL_strlcpy(buf, "Abilities", buflen);
        return;
    case 'i':
        SDL_strlcpy(buf, "Inventory", buflen);
        return;
    case 'e':
        SDL_strlcpy(buf, "Equipment", buflen);
        return;
    case 'u':
        SDL_strlcpy(buf, "Use", buflen);
        return;
    case 'x':
        SDL_strlcpy(buf, "Examine", buflen);
        return;
    case 's':
        SDL_strlcpy(buf, "Sing", buflen);
        return;
    case 'S':
        SDL_strlcpy(buf, "Stealth", buflen);
        return;
    case KTRL('A'):
        SDL_strlcpy(buf, "Swap staff", buflen);
        return;
    case KTRL('F'):
        SDL_strlcpy(buf, "Swap quiver", buflen);
        return;
    case 'f':
        SDL_strlcpy(buf, "Fire", buflen);
        return;
    case 'F':
        SDL_strlcpy(buf, "Fire 2nd", buflen);
        return;
    case 'h':
        SDL_strlcpy(buf, "Character", buflen);
        return;
    case 'l':
        SDL_strlcpy(buf, "Look", buflen);
        return;
    case 'o':
        SDL_strlcpy(buf, "Open", buflen);
        return;
    case 'q':
        SDL_strlcpy(buf, "Quaff", buflen);
        return;
    case 'r':
        SDL_strlcpy(buf, "Remove", buflen);
        return;
    case 'a':
        SDL_strlcpy(buf, "Staff", buflen);
        return;
    case 'M':
        SDL_strlcpy(buf, "Map", buflen);
        return;
    case 'b':
        SDL_strlcpy(buf, "Bash", buflen);
        return;
    case 'j':
        SDL_strlcpy(buf, "Supplies", buflen);
        return;
    case 'z':
        SDL_strlcpy(buf, "Wait", buflen);
        return;
    case '.':
        SDL_strlcpy(buf, "Run", buflen);
        return;
    case '/':
        SDL_strlcpy(buf, "Alt", buflen);
        return;
    case 'w':
        SDL_strlcpy(buf, "Wear", buflen);
        return;
    case 'g':
        SDL_strlcpy(buf, "Pickup", buflen);
        return;
    case 'Z':
        SDL_strlcpy(buf, "Rest", buflen);
        return;
    case 'c':
        SDL_strlcpy(buf, "Close", buflen);
        return;
    case 'D':
        SDL_strlcpy(buf, "Disarm", buflen);
        return;
    case 'X':
        SDL_strlcpy(buf, "Exchange", buflen);
        return;
    case '-':
        SDL_strlcpy(buf, "Fletch", buflen);
        return;
    case '{':
        SDL_strlcpy(buf, "Inscribe", buflen);
        return;
    case 'E':
        SDL_strlcpy(buf, "Eat", buflen);
        return;
    case 't':
        SDL_strlcpy(buf, "Throw", buflen);
        return;
    case 'p':
        SDL_strlcpy(buf, "Horn", buflen);
        return;
    case 'L':
        SDL_strlcpy(buf, "Pan", buflen);
        return;
    case KTRL('Q'):
        SDL_strlcpy(buf, "Combat", buflen);
        return;
    case KTRL('Y'):
        SDL_strlcpy(buf, "Debug command", buflen);
        return;
    case 'J':
        SDL_strlcpy(buf, "Equip a jewel set", buflen);
        return;
    case '0':
        SDL_strlcpy(buf, "Smithing", buflen);
        return;
    case '<':
        SDL_strlcpy(buf, "Up", buflen);
        return;
    case '>':
        SDL_strlcpy(buf, "Down", buflen);
        return;
    case 'm':
        SDL_strlcpy(buf, "Menu", buflen);
        return;
    case '?':
        SDL_strlcpy(buf, "Help", buflen);
        return;
    case '@':
        SDL_strlcpy(buf, "Character", buflen);
        return;
    case 'O':
        SDL_strlcpy(buf, "Options", buflen);
        return;
    case ':':
        SDL_strlcpy(buf, "Notes", buflen);
        return;
    case '~':
        SDL_strlcpy(buf, "Knowledge", buflen);
        return;
    case '[':
        SDL_strlcpy(buf, "Monsters", buflen);
        return;
    case ']':
        SDL_strlcpy(buf, "Objects", buflen);
        return;
    default:
        if (binding >= 32 && binding <= 126)
            strnfmt(buf, buflen, "%c", binding);
        else
            strnfmt(buf, buflen, "%d", binding);
        return;
    }
}

static void help_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

/* ------------------------------------------------------------------------
 * Dynamic help pagination
 *
 * Requirement: keep the *existing* help text and colouring exactly the same.
 * Only formatting (which strings appear on which page) should change.
 *
 * Approach: record the handcrafted legacy help rendering into a list of draw
 * operations, stack all 8 pages into one document, then paginate that document
 * by current terminal height.
 */

typedef struct {
    bool use_role;
    bool is_heading;
    color_role_t role;
    byte attr;
    const char* text;
    int y;
    int x;
} help_draw_op_t;

#define HELP_DOC_MAX_OPS 8192
#define HELP_DOC_MAX_ROWS 1024
#define HELP_DOC_MAX_PAGES 256
#define HELP_DOC_MAX_COLS 256
#define HELP_DOC_STRING_POOL_SIZE 65536
#define HELP_DOC_DISPLAY_MAX_ROWS 4096
#define HELP_DOC_DISPLAY_MAX_SPANS 16384
#define HELP_DOC_DISPLAY_STRING_POOL_SIZE 131072

static help_draw_op_t g_help_doc_ops[HELP_DOC_MAX_OPS];
static int g_help_doc_ops_n = 0;

static char g_help_doc_string_pool[HELP_DOC_STRING_POOL_SIZE];
static size_t g_help_doc_string_pool_used = 0;

typedef struct {
    bool use_role;
    color_role_t role;
    byte attr;
    int x;
    const char* text;
} help_display_span_t;

typedef struct {
    bool has_content;
    bool is_heading;
    int span_start;
    int span_count;
} help_display_row_t;

typedef struct {
    bool used;
    bool use_role;
    color_role_t role;
    byte attr;
    char ch;
} help_row_cell_t;

static help_display_row_t g_help_display_rows[HELP_DOC_DISPLAY_MAX_ROWS];
static int g_help_display_rows_n = 0;

static help_display_span_t g_help_display_spans[HELP_DOC_DISPLAY_MAX_SPANS];
static int g_help_display_spans_n = 0;

static char g_help_display_string_pool[HELP_DOC_DISPLAY_STRING_POOL_SIZE];
static size_t g_help_display_string_pool_used = 0;

static bool g_help_record_ops = false;
static int g_help_record_base_y = 0;
static int g_help_record_page_min_y = 0;
static int g_help_record_page_max_y = 0;

/* Forward declaration: used for recording. */
static void show_help_screen_legacy(int i, bool include_header);

static const char* help_doc_intern_string(const char* s)
{
    size_t len;
    char* dst;

    if (!s)
        s = "";

    len = strlen(s) + 1;
    if (len > HELP_DOC_STRING_POOL_SIZE)
        return s;

    if (g_help_doc_string_pool_used + len > HELP_DOC_STRING_POOL_SIZE)
        return s;

    dst = g_help_doc_string_pool + g_help_doc_string_pool_used;
    memcpy(dst, s, len);
    g_help_doc_string_pool_used += len;
    return dst;
}

static const char* help_display_intern_string(const char* s)
{
    size_t len;
    char* dst;

    if (!s)
        s = "";

    len = strlen(s) + 1;
    if (len > HELP_DOC_DISPLAY_STRING_POOL_SIZE)
        return s;

    if (g_help_display_string_pool_used + len > HELP_DOC_DISPLAY_STRING_POOL_SIZE)
        return s;

    dst = g_help_display_string_pool + g_help_display_string_pool_used;
    memcpy(dst, s, len);
    g_help_display_string_pool_used += len;
    return dst;
}

static void help_doc_record_role(color_role_t role, const char* s, bool is_heading, int row, int col)
{
    if (g_help_doc_ops_n >= HELP_DOC_MAX_OPS)
        return;

    g_help_doc_ops[g_help_doc_ops_n].use_role = true;
    g_help_doc_ops[g_help_doc_ops_n].is_heading = is_heading;
    g_help_doc_ops[g_help_doc_ops_n].role = role;
    g_help_doc_ops[g_help_doc_ops_n].attr = 0;
    g_help_doc_ops[g_help_doc_ops_n].text = help_doc_intern_string(s);
    g_help_doc_ops[g_help_doc_ops_n].y = g_help_record_base_y + row;
    g_help_doc_ops[g_help_doc_ops_n].x = col;
    g_help_doc_ops_n++;

    if (g_help_record_page_min_y > g_help_record_base_y + row)
        g_help_record_page_min_y = g_help_record_base_y + row;
    if (g_help_record_page_max_y < g_help_record_base_y + row)
        g_help_record_page_max_y = g_help_record_base_y + row;
}

static void help_doc_record_attr(byte attr, const char* s, int row, int col)
{
    if (g_help_doc_ops_n >= HELP_DOC_MAX_OPS)
        return;

    g_help_doc_ops[g_help_doc_ops_n].use_role = false;
    g_help_doc_ops[g_help_doc_ops_n].is_heading = false;
    g_help_doc_ops[g_help_doc_ops_n].role = ROLE_BODY;
    g_help_doc_ops[g_help_doc_ops_n].attr = attr;
    g_help_doc_ops[g_help_doc_ops_n].text = help_doc_intern_string(s);
    g_help_doc_ops[g_help_doc_ops_n].y = g_help_record_base_y + row;
    g_help_doc_ops[g_help_doc_ops_n].x = col;
    g_help_doc_ops_n++;

    if (g_help_record_page_min_y > g_help_record_base_y + row)
        g_help_record_page_min_y = g_help_record_base_y + row;
    if (g_help_record_page_max_y < g_help_record_base_y + row)
        g_help_record_page_max_y = g_help_record_base_y + row;
}

static void help_emit_role(color_role_t role, const char* s, int row, int col)
{
    if (g_help_record_ops)
        help_doc_record_role(role, s, false, row, col);
    else
        put_role(role, s, row, col);
}

static void help_emit_heading(const char* s, int row, int col)
{
    if (g_help_record_ops)
        help_doc_record_role(ROLE_SECTION, s, true, row, col);
    else
        put_role(ROLE_SECTION, s, row, col);
}

static void help_emit_attr(byte attr, const char* s, int row, int col)
{
    if (g_help_record_ops)
        help_doc_record_attr(attr, s, row, col);
    else
        c_put_str(attr, s, row, col);
}

static bool help_use_legacy_layout(int wid, int hgt)
{
    return (wid == 80) && (hgt == 24);
}

static void help_display_reset(void)
{
    g_help_display_rows_n = 0;
    g_help_display_spans_n = 0;
    g_help_display_string_pool_used = 0;
}

static void help_display_add_blank_row(void)
{
    if (g_help_display_rows_n >= HELP_DOC_DISPLAY_MAX_ROWS)
        return;

    g_help_display_rows[g_help_display_rows_n].has_content = false;
    g_help_display_rows[g_help_display_rows_n].is_heading = false;
    g_help_display_rows[g_help_display_rows_n].span_start = g_help_display_spans_n;
    g_help_display_rows[g_help_display_rows_n].span_count = 0;
    g_help_display_rows_n++;
}

static void help_display_add_wrapped_row(const help_row_cell_t* cells, int len,
    int indent, bool is_heading)
{
    int row_index;
    int start = 0;

    if (!cells || len <= 0)
    {
        help_display_add_blank_row();
        return;
    }

    if (g_help_display_rows_n >= HELP_DOC_DISPLAY_MAX_ROWS)
        return;

    row_index = g_help_display_rows_n++;
    g_help_display_rows[row_index].has_content = true;
    g_help_display_rows[row_index].is_heading = is_heading;
    g_help_display_rows[row_index].span_start = g_help_display_spans_n;
    g_help_display_rows[row_index].span_count = 0;

    while (start < len)
    {
        int end = start + 1;
        bool use_role = cells[start].use_role;
        color_role_t role = cells[start].role;
        byte attr = cells[start].attr;
        char span_buf[HELP_DOC_MAX_COLS + 1];
        int span_len;
        bool all_spaces = true;

        while (end < len
            && cells[end].use_role == use_role
            && cells[end].role == role
            && cells[end].attr == attr)
        {
            end++;
        }

        span_len = end - start;
        if (span_len > HELP_DOC_MAX_COLS)
            span_len = HELP_DOC_MAX_COLS;

        for (int i = 0; i < span_len; i++)
        {
            span_buf[i] = cells[start + i].ch;
            if (span_buf[i] != ' ')
                all_spaces = false;
        }
        span_buf[span_len] = '\0';

        if (!all_spaces && g_help_display_spans_n < HELP_DOC_DISPLAY_MAX_SPANS)
        {
            g_help_display_spans[g_help_display_spans_n].use_role = use_role;
            g_help_display_spans[g_help_display_spans_n].role = role;
            g_help_display_spans[g_help_display_spans_n].attr = attr;
            g_help_display_spans[g_help_display_spans_n].x = 1 + indent + start;
            g_help_display_spans[g_help_display_spans_n].text =
                help_display_intern_string(span_buf);
            g_help_display_spans_n++;
            g_help_display_rows[row_index].span_count++;
        }

        start = end;
    }
}

static void help_build_compact_source_row(int source_y,
    help_row_cell_t cells[HELP_DOC_MAX_COLS], int* first_used, int* last_used,
    bool* has_content, bool* is_heading)
{
    int min_x = HELP_DOC_MAX_COLS;
    int max_x = -1;
    bool found = false;
    bool heading = false;

    memset(cells, 0, sizeof(help_row_cell_t) * HELP_DOC_MAX_COLS);

    for (int op = 0; op < g_help_doc_ops_n; op++)
    {
        const char* text;

        if (g_help_doc_ops[op].y != source_y)
            continue;

        text = g_help_doc_ops[op].text ? g_help_doc_ops[op].text : "";
        if (g_help_doc_ops[op].is_heading)
            heading = true;

        for (int i = 0; text[i]; i++)
        {
            int x = g_help_doc_ops[op].x + i;

            if (x < 0 || x >= HELP_DOC_MAX_COLS)
                continue;

            cells[x].used = true;
            cells[x].use_role = g_help_doc_ops[op].use_role;
            cells[x].role = g_help_doc_ops[op].role;
            cells[x].attr = g_help_doc_ops[op].attr;
            cells[x].ch = text[i];

            if (x < min_x)
                min_x = x;
            if (x > max_x)
                max_x = x;
            found = true;
        }
    }

    if (first_used)
        *first_used = found ? min_x : 0;
    if (last_used)
        *last_used = found ? max_x : -1;
    if (has_content)
        *has_content = found;
    if (is_heading)
        *is_heading = heading;
}

static int help_compact_row_indent(int first_used)
{
    int indent = first_used - 1;

    if (indent < 0)
        indent = 0;
    if (indent > 4)
        indent = 4;

    return indent;
}

static int help_build_compact_display_rows(int term_wid, int doc_hgt)
{
    help_row_cell_t source_cells[HELP_DOC_MAX_COLS];

    help_display_reset();

    for (int y = 0; y < doc_hgt; y++)
    {
        help_row_cell_t compact_cells[HELP_DOC_MAX_COLS];
        help_row_cell_t fill_style;
        int first_used = 0;
        int last_used = -1;
        int compact_len = 0;
        int indent;
        int wrap_width;
        int pos = 0;
        bool has_content = false;
        bool is_heading = false;
        bool have_fill = false;
        bool first_line = true;

        help_build_compact_source_row(y, source_cells, &first_used, &last_used,
            &has_content, &is_heading);

        if (!has_content)
        {
            help_display_add_blank_row();
            continue;
        }

        for (int x = first_used; x <= last_used && compact_len < HELP_DOC_MAX_COLS; x++)
        {
            if (source_cells[x].used)
            {
                compact_cells[compact_len++] = source_cells[x];
                fill_style = source_cells[x];
                have_fill = true;
            }
            else
            {
                compact_cells[compact_len].used = true;
                compact_cells[compact_len].use_role = have_fill ? fill_style.use_role : true;
                compact_cells[compact_len].role = have_fill ? fill_style.role : ROLE_BODY;
                compact_cells[compact_len].attr = have_fill ? fill_style.attr : TERM_WHITE;
                compact_cells[compact_len].ch = ' ';
                compact_len++;
            }
        }

        while (compact_len > 0 && compact_cells[compact_len - 1].ch == ' ')
            compact_len--;

        if (compact_len <= 0)
        {
            help_display_add_blank_row();
            continue;
        }

        indent = help_compact_row_indent(first_used);
        wrap_width = term_wid - indent - 2;
        if (wrap_width < 1)
            wrap_width = 1;
        if (wrap_width > HELP_DOC_MAX_COLS)
            wrap_width = HELP_DOC_MAX_COLS;

        while (pos < compact_len)
        {
            int line_start = pos;
            int line_end;
            int next_pos;

            if (!first_line)
            {
                while (line_start < compact_len && compact_cells[line_start].ch == ' ')
                    line_start++;
            }

            if (line_start >= compact_len)
                break;

            if ((compact_len - line_start) <= wrap_width)
            {
                line_end = compact_len;
                next_pos = compact_len;
            }
            else
            {
                int limit = line_start + wrap_width;
                int break_at = -1;

                for (int i = line_start; i < limit; i++)
                {
                    if (compact_cells[i].ch == ' ')
                        break_at = i;
                }

                if (break_at > line_start)
                {
                    line_end = break_at;
                    next_pos = break_at + 1;
                }
                else
                {
                    line_end = limit;
                    next_pos = limit;
                }
            }

            while (line_end > line_start && compact_cells[line_end - 1].ch == ' ')
                line_end--;

            if (line_end > line_start)
            {
                help_display_add_wrapped_row(compact_cells + line_start,
                    line_end - line_start, indent, is_heading && first_line);
            }

            pos = next_pos;
            first_line = false;
        }
    }

    return g_help_display_rows_n;
}

static int help_dynamic_build_display_pages(int term_hgt, int display_hgt,
    int page_starts[HELP_DOC_MAX_PAGES], int page_ends[HELP_DOC_MAX_PAGES])
{
    int capacity = term_hgt - 3;
    int start_row = 0;
    int page_count = 0;

    if (capacity < 4)
        capacity = 4;

    while ((start_row < display_hgt) && (page_count < HELP_DOC_MAX_PAGES))
    {
        int end_row;
        int last_content;

        while (start_row < display_hgt
            && !g_help_display_rows[start_row].has_content)
        {
            start_row++;
        }

        if (start_row >= display_hgt)
            break;

        end_row = start_row + capacity - 1;
        if (end_row >= display_hgt)
            end_row = display_hgt - 1;

        last_content = end_row;
        while (last_content >= start_row
            && !g_help_display_rows[last_content].has_content)
        {
            last_content--;
        }

        while (last_content >= start_row
            && g_help_display_rows[last_content].is_heading)
        {
            end_row = last_content - 1;
            if (end_row < start_row)
            {
                end_row = start_row;
                break;
            }

            last_content = end_row;
            while (last_content >= start_row
                && !g_help_display_rows[last_content].has_content)
            {
                last_content--;
            }
        }

        page_starts[page_count] = start_row;
        page_ends[page_count] = end_row;
        page_count++;

        start_row = end_row + 1;
    }

    if (page_count < 1)
    {
        page_starts[0] = 0;
        page_ends[0] = 0;
        page_count = 1;
    }

    return page_count;
}

static int help_build_document_ops(int* out_doc_hgt, bool row_has_content[HELP_DOC_MAX_ROWS], bool row_has_heading[HELP_DOC_MAX_ROWS])
{
    int page;
    int base_y = 0;
    int start_op;
    int end_op;
    int shift;
    int doc_max_y = -1;

    g_help_doc_ops_n = 0;
    g_help_doc_string_pool_used = 0;

    for (page = 1; page <= HELP_TOTAL_PAGES; page++)
    {
        int op_idx;
        int page_height;

        g_help_record_ops = true;
        g_help_record_base_y = base_y;
        g_help_record_page_min_y = INT_MAX;
        g_help_record_page_max_y = INT_MIN;

        start_op = g_help_doc_ops_n;
        show_help_screen_legacy(page, false);
        end_op = g_help_doc_ops_n;

        if (end_op <= start_op)
            continue;

        /* Normalize each recorded legacy page so it starts at y=base_y */
        shift = g_help_record_page_min_y - base_y;
        if (shift < 0)
            shift = 0;
        for (op_idx = start_op; op_idx < end_op; op_idx++)
            g_help_doc_ops[op_idx].y -= shift;

        page_height = (g_help_record_page_max_y - g_help_record_page_min_y + 1);
        if (page_height < 1)
            page_height = 1;

        base_y += page_height + 1;
    }

    g_help_record_ops = false;

    /* Build per-row markers */
    for (int r = 0; r < HELP_DOC_MAX_ROWS; r++)
    {
        row_has_content[r] = false;
        row_has_heading[r] = false;
    }

    for (int op = 0; op < g_help_doc_ops_n; op++)
    {
        int y = g_help_doc_ops[op].y;
        if (y < 0 || y >= HELP_DOC_MAX_ROWS)
            continue;
        row_has_content[y] = true;
        if (g_help_doc_ops[op].is_heading)
            row_has_heading[y] = true;
        if (y > doc_max_y)
            doc_max_y = y;
    }

    if (doc_max_y < 0)
        doc_max_y = 0;
    if (out_doc_hgt)
        *out_doc_hgt = doc_max_y + 1;

    return g_help_doc_ops_n;
}

static int help_dynamic_build_document_pages(
    int term_hgt,
    int doc_hgt,
    const bool row_has_content[HELP_DOC_MAX_ROWS],
    const bool row_has_heading[HELP_DOC_MAX_ROWS],
    int page_starts[HELP_DOC_MAX_PAGES],
    int page_ends[HELP_DOC_MAX_PAGES])
{
    int capacity = term_hgt - 3;
    int start_y = 0;
    int page_count = 0;

    if (capacity < 4)
        capacity = 4;

    while ((start_y < doc_hgt) && (page_count < HELP_DOC_MAX_PAGES))
    {
        int end_y = start_y + capacity - 1;
        int last_content;

        if (end_y >= doc_hgt)
            end_y = doc_hgt - 1;

        /* Ensure we don't end on a heading row (titles must have text under them) */
        last_content = end_y;
        while (last_content >= start_y && !row_has_content[last_content])
            last_content--;

        while (last_content >= start_y && row_has_heading[last_content])
        {
            end_y = last_content - 1;
            if (end_y < start_y)
            {
                end_y = start_y;
                break;
            }

            last_content = end_y;
            while (last_content >= start_y && !row_has_content[last_content])
                last_content--;
        }

        page_starts[page_count] = start_y;
        page_ends[page_count] = end_y;
        page_count++;

        start_y = end_y + 1;
    }

    if (page_count < 1)
    {
        page_starts[0] = 0;
        page_ends[0] = 0;
        page_count = 1;
    }

    return page_count;
}

static void show_help_screen_dynamic_document(
    int page,
    int total_pages,
    int term_hgt,
    int doc_start_y,
    int doc_end_y)
{
    char header[96];
    const int col = 1;
    const int top = 2;

    strnfmt(header, sizeof(header),
        "SIL-MORE: SHINING DARKNESS - HELP [%d/%d]",
        page, total_pages);
    put_role(ROLE_HEADER, header, 0, col);

    for (int op = 0; op < g_help_doc_ops_n; op++)
    {
        int y = g_help_doc_ops[op].y;
        int x = g_help_doc_ops[op].x;
        int screen_y;

        if (y < doc_start_y || y > doc_end_y)
            continue;

        screen_y = top + (y - doc_start_y);
        if (screen_y < top || screen_y >= term_hgt - 1)
            continue;

        if (g_help_doc_ops[op].use_role)
            put_role(g_help_doc_ops[op].role, g_help_doc_ops[op].text, screen_y, x);
        else
            c_put_str(g_help_doc_ops[op].attr, g_help_doc_ops[op].text, screen_y, x);
    }
}

static void show_help_screen_compact_document(
    int page,
    int total_pages,
    int term_hgt,
    int row_start,
    int row_end)
{
    char header[96];
    const int col = 1;
    const int top = 2;

    strnfmt(header, sizeof(header),
        "SIL-MORE: SHINING DARKNESS - HELP [%d/%d]",
        page, total_pages);
    put_role(ROLE_HEADER, header, 0, col);

    for (int row = row_start; row <= row_end; row++)
    {
        int screen_y = top + (row - row_start);

        if (screen_y < top || screen_y >= term_hgt - 1)
            continue;

        if (!g_help_display_rows[row].has_content)
            continue;

        for (int i = 0; i < g_help_display_rows[row].span_count; i++)
        {
            help_display_span_t* span =
                &g_help_display_spans[g_help_display_rows[row].span_start + i];

            if (span->use_role)
                put_role(span->role, span->text, screen_y, span->x);
            else
                c_put_str(span->attr, span->text, screen_y, span->x);
        }
    }
}

/* -------- Help pages ----------------------------------------------------- */

/*
 * NOTE: This function is used in two modes:
 *  - Normal draw mode: g_help_record_ops=false (calls put_role/c_put_str)
 *  - Record mode:      g_help_record_ops=true  (records ops, no drawing)
 *
 * We redirect put_role/c_put_str to record-aware emitters via macros.
 */
#define put_role help_emit_role
#define c_put_str help_emit_attr
static void show_help_screen_legacy(int i, bool include_header)
{
    int row, col, col2;
    char page_header[96];

    switch (i)
    {
    case 1:
    {
        /* SIL-MORE: HELP [1/8]: GOAL & HEROES */
        row = 0; col = 1;
        if (include_header)
        {
            sprintf(page_header, "SIL-MORE: SHINING DARKNESS - HELP [%d/%d]: GOAL & HEROES", i, HELP_TOTAL_PAGES);
            put_role(ROLE_HEADER, page_header, row, col);
        }
        row += 2;

        help_emit_heading("GOAL", row, col); row++;
        put_role(ROLE_BODY, "- Steal ", row, col);
        put_role(ROLE_TERM, "Silmarils", row, col + 8);
        put_role(ROLE_BODY, " across runs; the saga ends when you've taken ", row, col + 17);
        put_role(ROLE_WARN, "fifteen", row, col + 64);
        put_role(ROLE_BODY, ".", row, col + 71);
        row++;
        put_role(ROLE_BODY, "- Plan for the ", row, col);
        put_role(ROLE_TERM, "long war", row, col + 15);
        put_role(ROLE_BODY, ": every ", row, col + 23);
        put_role(ROLE_TERM, "Silmaril", row, col + 31);
        put_role(ROLE_BODY, " twists ", row, col + 39);
        put_role(ROLE_TERM, "fate", row, col + 47);
        put_role(ROLE_BODY, " and reshapes play.", row, col + 51);
        row++;
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_BAD, "Permadeath", row, col + 2);
        put_role(ROLE_BODY, ": a fallen hero is gone for the saga.", row, col + 12);
        row++;
        put_role(ROLE_BODY, "- The saga ends when no heroes remain. Worthy deaths earn ", row, col);
        put_role(ROLE_GOOD, "Valar blessings", row, col + 58);
        put_role(ROLE_BODY, ".", row, col + 73);
        row += 2;

        help_emit_heading("HEROES OF LEGEND", row, col); row++;
        put_role(ROLE_BODY, "- Choose a fixed hero: ", row, col);
        put_role(ROLE_TERM, "Fëanor, Fingolfin, Beren, Lúthien", row, col + 23);
        put_role(ROLE_BODY, ", and others.", row, col + 56);
        row++;
        put_role(ROLE_BODY, "- Each bears a signature trait: ", row, col);
        put_role(ROLE_TERM, "Master Artisan, Elven Dance, Creator of Angrist", row, col + 32);
        put_role(ROLE_BODY, ".", row, col + 80);
        row++;
        put_role(ROLE_BODY, "- Some traits are shared across lines: ", row, col);
        put_role(ROLE_TERM, "Kinslayer, Gift of Eru", row, col + 39);
        put_role(ROLE_BODY, ", and more.", row, col + 61);
        row++;
        put_role(ROLE_BODY, "- A power rating is shown during selection. New? Start with the most powerful.", row, col);
        row++;
        put_role(ROLE_BODY, "- Expert? Forge your own path-synergy beats raw rating.", row, col);
        row++;
        put_role(ROLE_BODY, "- Remember: the game ends after 15 ", row, col);
        put_role(ROLE_TERM, "Silmarils", row, col + 36);
        put_role(ROLE_BODY, ", not one.", row, col + 45);
        row++;
        put_role(ROLE_BODY, "- Tags are intentionally sparse-learn by doing.", row, col);
        row++;
        put_role(ROLE_BODY, "- Hint - The Stave of Self-Knowledge can show hidden traits.", row, col);
        row += 2;

        help_emit_heading("HELP FROM VALAR", row, col); row++;
        put_role(ROLE_BODY, "- The ", row, col);
        put_role(ROLE_TERM, "Valar", row, col + 6);
        put_role(ROLE_BODY, " guide worthy heroes through ", row, col + 11);
        put_role(ROLE_TERM, "sacred quests", row, col + 40);
        put_role(ROLE_BODY, ".", row, col + 53);
        row++;
        put_role(ROLE_BODY, "- Seek the ", row, col);
        put_role(ROLE_TERM, "halls of knowledge", row, col + 11);
        put_role(ROLE_BODY, " where ancient wisdom dwells.", row, col + 29);
        row++;
        put_role(ROLE_BODY, "- Each quest reveals ", row, col);
        put_role(ROLE_TERM, "hidden truths", row, col + 21);
        put_role(ROLE_BODY, " and grants ", row, col + 34);
        put_role(ROLE_GOOD, "divine blessings", row, col + 46);
        put_role(ROLE_BODY, ".", row, col + 62);
        row++;
        put_role(ROLE_BODY, "- The path of ", row, col);
        put_role(ROLE_WARN, "redemption", row, col + 14);
        put_role(ROLE_BODY, " is always open to those who seek it.", row, col + 24);
        break;
    }

    case 2:
    {
        /* SIL-MORE: HELP [2/8]: START & DEPTH */
        row = 0; col = 1;
        if (include_header)
        {
            sprintf(page_header, "SIL-MORE: SHINING DARKNESS - HELP [%d/%d]: START & DEPTH", i, HELP_TOTAL_PAGES);
            put_role(ROLE_HEADER, page_header, row, col);
        }
        row += 2;

        help_emit_heading("START", row, col); row++;
        put_role(ROLE_BODY, "- You begin with a ", row, col);
        put_role(ROLE_SUBTLE, "basic weapon", row, col + 19);
        put_role(ROLE_BODY, " and ", row, col + 31);
        put_role(ROLE_WARN, "no armour", row, col + 36);
        put_role(ROLE_BODY, "-", row, col + 45);
        put_role(ROLE_GOOD, "gear up fast", row, col + 46);
        put_role(ROLE_BODY, ".", row, col + 58);
        row++;
        put_role(ROLE_BODY, "- Search early rooms for ", row, col);
        put_role(ROLE_GOOD, "armour, torches, bow and arrows", row, col + 25);
        put_role(ROLE_BODY, ".", row, col + 56);
        row += 2;

        help_emit_heading("DEPTH & ESCAPE", row, col); row++;
        put_role(ROLE_BODY, "- Angband drags you down: your ", row, col);
        put_role(ROLE_TERM, "Minimum Depth", row, col + 31);
        put_role(ROLE_BODY, " rises as time passes.", row, col + 44);
        row++;
        put_role(ROLE_BODY, "- You cannot climb above it unless bearing a ", row, col);
        put_role(ROLE_TERM, "Silmaril", row, col + 45);
        put_role(ROLE_BODY, ".", row, col + 53);
        row++;
        put_role(ROLE_BODY, "- Every lvl is generated anew-don't be afraid to climb back upstairs if stuck.", row, col);
        row += 2;

        help_emit_heading("ELEMENTS", row, col); row++;
        /* Fire */
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_ELEM_FIRE, "Fire", row, col + 2);
        put_role(ROLE_BODY, ": common; ", row, col + 6);
        put_role(ROLE_ELEM_FIRE, "burns and ruins", row, col + 16);
        put_role(ROLE_BODY, " many wares; treat ", row, col + 31);
        put_role(ROLE_ELEM_FIRE, "flame of Udun", row, col + 50);
        put_role(ROLE_BODY, " with respect.", row, col + 63);
        row++;
        /* Cold */
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_ELEM_COLD, "Cold", row, col + 2);
        put_role(ROLE_BODY, ": rarer; can ", row, col + 6);
        put_role(ROLE_ELEM_COLD, "destroy potions/oil", row, col + 19);
        put_role(ROLE_BODY, "; ", row, col + 38);
        put_role(ROLE_GOOD, "warmth is life", row, col + 40);
        put_role(ROLE_BODY, ".", row, col + 54);
        row++;
        /* Poison */
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_ELEM_POISON, "Poison", row, col + 2);
        put_role(ROLE_BODY, ": builds a ", row, col + 8);
        put_role(ROLE_WARN, "counter", row, col + 19);
        put_role(ROLE_BODY, " and ", row, col + 26);
        put_role(ROLE_BAD, "bleeds you over time", row, col + 31);
        put_role(ROLE_BODY, "-", row, col + 51);
        put_role(ROLE_GOOD, "cleanse", row, col + 52);
        put_role(ROLE_BODY, " or wait it out.", row, col + 59);
        row++;
        /* Darkness */
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_ELEM_DARKNESS, "Darkness", row, col + 2);
        put_role(ROLE_BODY, ": only bright ", row, col + 10);
        put_role(ROLE_ELEM_LIGHT, "light", row, col + 24);
        put_role(ROLE_BODY, " truly resists it; carry your own dawn.", row, col + 30);
        row++;

        put_role(ROLE_BODY, "- Mixed elemental attacks will roll extra dice when you lack resistance.", row, col);
        row += 2;

        help_emit_heading("STATUS & MORALE", row, col); row++;
        put_role(ROLE_BODY, "- Foes are Asleep, Unwary, Alert; your noise sets the stage.", row, col);
        row++;
        put_role(ROLE_BODY, "- Stealth turns (S) and waiting are potent for slipping past sentries.", row, col);
        row++;
        put_role(ROLE_BODY, "- Foes can be Aggressive, Confident, Fleeing and run if their morale breaks.", row, col);
        break;
    }

    case 3:
    {
        /* SIL-MORE: HELP [3/8]: COMBAT & DEFENCE */
        row = 0; col = 1;
        if (include_header)
        {
            sprintf(page_header, "SIL-MORE: SHINING DARKNESS - HELP [%d/%d]: COMBAT & DEFENCE", i, HELP_TOTAL_PAGES);
            put_role(ROLE_HEADER, page_header, row, col);
        }
        row += 2;

        help_emit_heading("COMBAT BASICS", row, col); row++;
        put_role(ROLE_BODY, "- Two opposed rolls decide hits: your Melee vs their ", row, col);
        put_role(ROLE_TERM, "Evasion", row, col + 53);
        put_role(ROLE_BODY, " (and vice versa).", row, col + 60);
        row++;
        put_role(ROLE_BODY, "- On a hit, your Damage meets their ", row, col);
        put_role(ROLE_TERM, "Protection", row, col + 37);
        put_role(ROLE_BODY, "; only the excess gets through.", row, col + 47);
        row++;
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_TERM, "Evasion", row, col + 2);
        put_role(ROLE_BODY, " avoids getting hit; ", row, col + 9);
        put_role(ROLE_TERM, "Armour", row, col + 30);
        put_role(ROLE_BODY, " [", row, col + 36);
        put_role(ROLE_TERM, "Protection", row, col + 38);
        put_role(ROLE_BODY, "] soaks what lands.", row, col + 48);
        row++;
        put_role(ROLE_BODY, "- Being ", row, col);
        put_role(ROLE_BAD, "surrounded", row, col + 8);
        put_role(ROLE_BODY, " crushes your evasion-fight in ", row, col + 18);
        put_role(ROLE_GOOD, "doorways and angles", row, col + 50);
        put_role(ROLE_BODY, ".", row, col + 69);
        row++;
        put_role(ROLE_BODY, "- Firing a bow in melee invites ", row, col);
        put_role(ROLE_BAD, "free strikes", row, col + 33);
        put_role(ROLE_BODY, "; make space before shooting.", row, col + 45);
        row++;
        put_role(ROLE_BODY, "- Great successes can trigger ", row, col);
        put_role(ROLE_GOOD, "criticals", row, col + 30);
        put_role(ROLE_BODY, " for extra hurt.", row, col + 39);
        row += 2;

        help_emit_heading("NUMBERS AT A GLANCE", row, col); row++;
        put_role(ROLE_BODY, "- Weapons show (attack, damage). ", row, col);
        put_role(ROLE_TERM, "Armour", row, col + 33);
        put_role(ROLE_BODY, " shows [evasion, protection].", row, col + 39);
        row++;
        put_role(ROLE_BODY, "- Ex: You (Str 3, Melee 16, Longsword 2d5, 2.0 lb) vs Orc [+4, 2d4]:", row, col);
        row += 2;
        put_role(ROLE_BODY, "  @ ", row, col);
        put_role(ROLE_GOOD, "(+16) 34", row, col + 4);
        put_role(ROLE_BAD, " 20", row, col + 12);
        put_role(ROLE_BODY, "  14 [+4] o", row, col + 15);
        put_role(ROLE_BODY, "  ->  ", row, col + 26);
        put_role(ROLE_GOOD, "(4d7) 19", row, col + 32);
        put_role(ROLE_BODY, " - ", row, col + 40);
        put_role(ROLE_WARN, "3", row, col + 43);
        put_role(ROLE_BODY, " = ", row, col + 44);
        put_role(ROLE_BAD, "16", row, col + 47);
        row ++;
        put_role(ROLE_BODY, "  Attack:  ", row, col);
        put_role(ROLE_GOOD, "1d20+16=34", row, col + 11);
        put_role(ROLE_BODY, " vs ", row, col + 21);
        put_role(ROLE_BODY, "1d20+4=14", row, col + 25);
        put_role(ROLE_BODY, "  ->  margin ", row, col + 34);
        put_role(ROLE_BAD, "20", row, col + 47);
        put_role(ROLE_BODY, " = ", row, col + 49);
        put_role(ROLE_BAD, "double crit", row, col + 52);
        put_role(ROLE_BODY, "!", row, col + 63);
        row++;
        put_role(ROLE_BODY, "  Damage:  2d5 +2 (Str 3 capped by 2 lb = 2 more sides) = 2d7,", row, col);
        row++;
        put_role(ROLE_BODY, "           +2d7 (1d per 7+weight in margin) = ", row, col);
        put_role(ROLE_GOOD, "4d7=19", row, col + 46);
        put_role(ROLE_BODY, " - ", row, col + 52);
        put_role(ROLE_WARN, "2d4=3", row, col + 55);
        put_role(ROLE_BODY, " = ", row, col + 60);
        put_role(ROLE_BAD, "16 dmg", row, col + 63);
        put_role(ROLE_BODY, "!", row, col + 69);
        row += 2;

        help_emit_heading("EVASION VS ARMOUR", row, col); row++;
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_TERM, "Evasion", row, col + 2);
        put_role(ROLE_BODY, " helps you not be hit at all; it's reduced if surrounded.", row, col + 9);
        row++;
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_TERM, "Armour", row, col + 2);
        put_role(ROLE_BODY, " reduces damage after a hit; more protection, less pain.", row, col + 8);
        row++;
        put_role(ROLE_BODY, "- Build around one or balance both; edges matter in tight fights.", row, col);
        break;
    }

    case 4:
    {
        /* SIL-MORE: HELP [4/8]: EARLY TIPS */
        row = 0; col = 1;
        if (include_header)
        {
            sprintf(page_header, "SIL-MORE: SHINING DARKNESS - HELP [%d/%d]: EARLY TIPS", i, HELP_TOTAL_PAGES);
            put_role(ROLE_HEADER, page_header, row, col);
        }
        row += 2;

        help_emit_heading("CRAFT & GEAR", row, col); row++;
        put_role(ROLE_BODY, "- Guaranteed ", row, col);
        put_role(ROLE_GOOD, "forges", row, col + 13);
        put_role(ROLE_BODY, " at 100', 300', and 500'-plan your craft route.", row, col + 19);
        row++;
        put_role(ROLE_BODY, "- Find armour and a bow first; control fights before you win them.", row, col);
        row += 2;

        help_emit_heading("TACTICS", row, col); row++;
        put_role(ROLE_BODY, "- Do not rush: this is tactical. Lure, isolate, and retreat often.", row, col);
        row++;
        put_role(ROLE_BODY, "- Doors and corners are force multipliers-avoid being surrounded.", row, col);
        row++;
        put_role(ROLE_BODY, "- Stairs are traffic-reset, ambush, or move on; do not loiter.", row, col);
        row += 2;

        help_emit_heading("ABILITIES", row, col); row++;
        put_role(ROLE_BODY, "- Abilities matter: a single pick can flip a matchup.", row, col);
        row++;
        put_role(ROLE_BODY, "- Choose abilities that reinforce your plan: stealth, control, or brute force.", row, col);
        row += 2;

        help_emit_heading("TONE & APPROACH", row, col); row++;
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_TERM, "Cunning", row, col + 2);
        put_role(ROLE_BODY, " over ", row, col + 9);
        put_role(ROLE_BAD, "cruelty", row, col + 15);
        put_role(ROLE_BODY, "; ", row, col + 22);
        put_role(ROLE_SECTION, "light", row, col + 24);
        put_role(ROLE_BODY, " over ", row, col + 29);
        put_role(ROLE_SUBTLE, "darkness", row, col + 35);
        put_role(ROLE_BODY, "; ", row, col + 43);
        put_role(ROLE_GOOD, "retreat is wisdom", row, col + 45);
        put_role(ROLE_BODY, ".", row, col + 62);
        row++;
        put_role(ROLE_BODY, "- ", row, col);
        put_role(ROLE_GOOD, "Doors, distance, and silence", row, col + 2);
        put_role(ROLE_BODY, " are ", row, col + 30);
        put_role(ROLE_TERM, "weapons", row, col + 35);
        put_role(ROLE_BODY, ".", row, col + 42);
        row++;
        put_role(ROLE_BODY, "- Your ", row, col);
        put_role(ROLE_TERM, "legend", row, col + 7);
        put_role(ROLE_BODY, " is a ", row, col + 13);
        put_role(ROLE_TERM, "mosaic of choices", row, col + 19);
        put_role(ROLE_BODY, "-small ", row, col + 36);
        put_role(ROLE_GOOD, "edges", row, col + 43);
        put_role(ROLE_BODY, " add up.", row, col + 48);
        row++;
        put_role(ROLE_BODY, "- You are the light you carry. Choose your fights; write your legend.", row, col);
        break;
    }

    case 5:
    {
        /* SIL-MORE: HELP [5/8]: MOVEMENT & MISCELLANEOUS */
        if (include_header)
        {
            sprintf(page_header, "HELP [%d/%d]: MOVEMENT & MISCELLANEOUS", i, HELP_TOTAL_PAGES);
            put_role(ROLE_HEADER, page_header, 0, 1);
        }

        row = 3; col = 3; col2 = col + 8;
        help_emit_heading("Movement etc", row - 2, col - 1);

        put_role(ROLE_KEY,   "7 8 9", row, col);
        put_role(ROLE_SUBTLE," \\|/ ", row + 1, col);
        put_role(ROLE_KEY,   "4 5 6", row + 2, col);
        put_role(ROLE_SUBTLE,"-", row + 2, col + 1);
        put_role(ROLE_SUBTLE,"-", row + 2, col + 3);
        put_role(ROLE_SUBTLE," /|\\ ", row + 3, col);
        put_role(ROLE_KEY,   "1 2 3", row + 4, col);

        put_role(ROLE_SUBTLE, "Use direction keys", row + 0, col2);
        put_role(ROLE_KEY,    "direction keys", row + 0, col2 + 4);
        put_role(ROLE_SUBTLE, "to move, attack, or open doors", row + 1, col2);
        put_role(ROLE_SUBTLE, "(You may need numlock)", row + 2, col2);
        put_role(ROLE_KEY,    "numlock", row + 2, col2 + 14);
        put_role(ROLE_SUBTLE, "Use 5 or z to wait a turn (& search)", row + 4, col2);
        put_role(ROLE_KEY,    "5", row + 4, col2 + 4);
        put_role(ROLE_KEY,    "z", row + 4, col2 + 9);

        row += 6;
        put_role(ROLE_SUBTLE, "Use shift or . to move continuously", row, col);
        put_role(ROLE_KEY,    "shift", row, col + 4);
        put_role(ROLE_KEY,    ".", row, col + 13);
        row++;
        put_role(ROLE_SUBTLE, "- direction 5 or z rests until healed", row, col + 2);
        row += 2;

        put_role(ROLE_SUBTLE, "Use control or / to interact with a square:", row, col);
        put_role(ROLE_KEY,    "control", row, col + 4);
        if (angband_keyset) put_role(ROLE_KEY, "+", row, col + 15); else put_role(ROLE_KEY, "/", row, col + 15);
        row++;
        put_role(ROLE_SUBTLE, "- tunnels through rubble/walls", row, col + 2); row++;
        put_role(ROLE_SUBTLE, "- closes open doors", row, col + 2);           row++;
        put_role(ROLE_SUBTLE, "- bashes closed doors", row, col + 2);          row++;
        put_role(ROLE_SUBTLE, "- disarms floor traps", row, col + 2);          row++;
        put_role(ROLE_SUBTLE, "- disarms/opens chests and searches skeletons", row, col + 2); row++;
        put_role(ROLE_SUBTLE, "- attacks monsters without moving", row, col + 2); row += 2;
        put_role(ROLE_SUBTLE, "Interacting with your own square also:", row, col); row++;
        put_role(ROLE_SUBTLE, "- picks up an item", row, col + 2); row++;
        put_role(ROLE_SUBTLE, "- uses a staircase/forge", row, col + 2); row++;
        put_role(ROLE_SUBTLE, "- can be done by pressing , or Space", row, col + 2);
        put_role(ROLE_KEY,    ",", row, col + 28);
        put_role(ROLE_KEY,    "Space", row, col + 33);

        row = 3; col = 52;
        help_emit_heading("Miscellaneous", row - 2, col);

        put_role(ROLE_KEY,  "f F", row, col - 1); put_role(ROLE_SUBTLE, "/", row, col); put_role(ROLE_SUBTLE, "fire from quiver 1/2", row, col + 3); row++;
        put_role(ROLE_KEY, "^f", row, col - 1); put_role(ROLE_SUBTLE, "swap quiver 1/2", row, col + 3); row++;
        if (angband_keyset) put_role(ROLE_KEY, " a", row, col); else put_role(ROLE_KEY, " s", row, col); put_role(ROLE_SUBTLE, "sing", row, col + 3); row++;
        put_role(ROLE_KEY, " S", row, col); put_role(ROLE_SUBTLE, "stealth mode", row, col + 3); row++;
        put_role(ROLE_KEY, " n", row, col); put_role(ROLE_SUBTLE, "repeat last command", row, col + 3); row++;
        if (angband_keyset) put_role(ROLE_KEY, " 0", row, col); else put_role(ROLE_KEY, " R", row, col); put_role(ROLE_SUBTLE, "repeat next command", row, col + 3); row += 2;
        put_role(ROLE_KEY, " l", row, col); put_role(ROLE_SUBTLE, "look (at things)", row, col + 3); row++;
        put_role(ROLE_KEY, " L", row, col); put_role(ROLE_SUBTLE, "look (around dungeon)", row, col + 3); row++;
        put_role(ROLE_KEY, " M", row, col); put_role(ROLE_SUBTLE, "display map of level", row, col + 3); row += 2;
        put_role(ROLE_KEY, " m", row, col); put_role(ROLE_UI,  "main menu", row, col + 3); row++;
        put_role(ROLE_KEY, "Tab", row, col - 1); put_role(ROLE_UI,  "change active weapon", row, col + 3); row++;
        put_role(ROLE_KEY, " y", row, col); put_role(ROLE_UI,  "display ability screen", row, col + 3); row++;
        if (angband_keyset) put_role(ROLE_KEY, " C", row, col); else put_role(ROLE_KEY, " @/h", row, col-2); put_role(ROLE_UI, "display character sheet", row, col + 3); row++;
        if (angband_keyset) put_role(ROLE_KEY, " =", row, col); else put_role(ROLE_KEY, " O", row, col); put_role(ROLE_UI, "set options", row, col + 3); row += 2;
        put_role(ROLE_KEY, "^s", row, col); put_role(ROLE_UI,  "save", row, col + 3); row++;
        put_role(ROLE_KEY, "^x", row, col); put_role(ROLE_UI,  "save and quit", row, col + 3); row++;
        // put_role(ROLE_KEY, " Q", row, col); put_role(ROLE_UI,  "abort current game", row, col + 3);
        break;
    }

    case 6:
    {
        /* SIL-MORE: HELP [6/8]: TERRAIN & ITEMS */
        if (include_header)
        {
            sprintf(page_header, "HELP [%d/%d]: TERRAIN & ITEMS", i, HELP_TOTAL_PAGES);
            put_role(ROLE_HEADER, page_header, 0, 1);
        }

        row = 3; col = 3;
        help_emit_heading("Terrain ", row - 2, col - 1);

        /* Keep gameplay glyph colours as-is; only change the labels to ROLE_BODY */
        if (hybrid_walls) { c_put_str(TERM_L_WHITE + (MAX_COLORS * BG_DARK), "#", row, col); }
        else if (solid_walls) { c_put_str(TERM_L_WHITE + (MAX_COLORS * BG_SAME), "#", row, col); }
        else { c_put_str(TERM_L_WHITE, "#", row, col); }
        put_role(ROLE_BODY, "wall", row, col + 2); row++;
        c_put_str(TERM_WHITE + (MAX_COLORS * BG_SAME), "%", row, col); put_role(ROLE_BODY, "quartz vein", row, col + 2); row++;
        c_put_str(TERM_SLATE, ":", row, col); put_role(ROLE_BODY, "rubble", row, col + 2); row++;
        c_put_str(TERM_L_UMBER, "+", row, col); put_role(ROLE_BODY, "closed door", row, col + 2); row++;
        c_put_str(TERM_L_UMBER, "'", row, col); put_role(ROLE_BODY, "open door", row, col + 2); row++;
        c_put_str(TERM_L_GREEN, "+", row, col); c_put_str(TERM_L_BLUE, "+", row, col + 1); c_put_str(TERM_VIOLET, "+", row, col + 2); put_role(ROLE_BODY, "warded doors", row, col + 4); row++;
        c_put_str(TERM_L_WHITE, ">", row, col); put_role(ROLE_BODY, "staircase down", row, col + 2); row++;
        c_put_str(TERM_L_WHITE, "<", row, col); put_role(ROLE_BODY, "staircase up", row, col + 2); row++;
        c_put_str(TERM_SLATE, "0", row, col); put_role(ROLE_BODY, "forge", row, col + 2); row++;
        c_put_str(TERM_YELLOW, "^", row, col); put_role(ROLE_BODY, "trap", row, col + 2); row++;
        c_put_str(TERM_L_GREEN, ";", row, col); put_role(ROLE_BODY, "warding glyph", row, col + 2); row++;
        c_put_str(TERM_L_WHITE, ".", row, col); put_role(ROLE_BODY, "empty floor", row, col + 2); row++;

        row = 3; col = 27;
        help_emit_heading("Items", row - 2, col - 1);
        c_put_str(TERM_L_WHITE, "| ", row, col); put_role(ROLE_BODY, "blades", row, col + 2); row++;
        c_put_str(TERM_SLATE, "/ ", row, col); put_role(ROLE_BODY, "axes & polearms", row, col + 2); row++;
        c_put_str(TERM_UMBER, "\\ ", row, col); put_role(ROLE_BODY, "blunt weapons", row, col + 2); row++;
        c_put_str(TERM_L_UMBER, "( ", row, col); put_role(ROLE_BODY, "soft armour", row, col + 2); row++;
        c_put_str(TERM_L_WHITE, "[ ", row, col); put_role(ROLE_BODY, "mail", row, col + 2); row++;
        c_put_str(TERM_L_WHITE, ") ", row, col); put_role(ROLE_BODY, "shields", row, col + 2); row++;
        c_put_str(TERM_L_WHITE, "] ", row, col); put_role(ROLE_BODY, "misc armour", row, col + 2); row++;
        c_put_str(TERM_RED, "= ", row, col); put_role(ROLE_BODY, "rings", row, col + 2); row++;
        c_put_str(TERM_ORANGE, "\" ", row, col); put_role(ROLE_BODY, "amulets", row, col + 2); row++;
        c_put_str(TERM_L_UMBER, "~ ", row, col); put_role(ROLE_BODY, "light sources", row, col + 2); row++;
        c_put_str(TERM_UMBER, "} ", row, col); put_role(ROLE_BODY, "bows", row, col + 2); row++;
        c_put_str(TERM_L_UMBER, "- ", row, col); put_role(ROLE_BODY, "arrows", row, col + 2); row++;
        c_put_str(TERM_L_UMBER, ", ", row, col); put_role(ROLE_BODY, "food", row, col + 2); row++;
        c_put_str(TERM_L_BLUE, "! ", row, col); put_role(ROLE_BODY, "potions", row, col + 2); row++;
        c_put_str(TERM_UMBER, "_ ", row, col); put_role(ROLE_BODY, "staves", row, col + 2); row++;
        c_put_str(TERM_L_UMBER, "? ", row, col); put_role(ROLE_BODY, "instruments", row, col + 2); row++;
        c_put_str(TERM_YELLOW, "! ", row, col); put_role(ROLE_BODY, "flasks of oil", row, col + 2); row++;

        row = 3; col = 52;
        help_emit_heading("Item Commands", row - 2, col - 1);
        if (angband_keyset) put_role(ROLE_KEY, "U", row, col); else put_role(ROLE_KEY, "u", row, col); put_role(ROLE_UI, "use", row, col + 2); row++;
        put_role(ROLE_KEY, "d", row, col); put_role(ROLE_UI, "drop", row, col + 2); row++;
        if (angband_keyset) put_role(ROLE_KEY, "I", row, col); else put_role(ROLE_KEY, "x", row, col); put_role(ROLE_UI, "examine", row, col + 2); row++;
        if (angband_keyset) put_role(ROLE_KEY, "v", row, col); else put_role(ROLE_KEY, "t", row, col); put_role(ROLE_UI, "throw", row, col + 2); row++;
        if (angband_keyset) put_role(ROLE_KEY, "^v", row, col - 1); else put_role(ROLE_KEY, "^t", row, col - 1); put_role(ROLE_UI, "throw (auto-target)", row, col + 2); row++;
        put_role(ROLE_KEY, "k", row, col); put_role(ROLE_UI, "destroy", row, col + 2); row++;
        put_role(ROLE_KEY, "{", row, col); put_role(ROLE_UI, "inscribe", row, col + 2); row++;
        break;
    }

    case 7:
    {
        /* SIL-MORE: HELP [7/8]: ADVANCED COMMANDS */
        if (include_header)
        {
            sprintf(page_header, "HELP [%d/%d]: ADVANCED COMMANDS", i, HELP_TOTAL_PAGES);
            put_role(ROLE_HEADER, page_header, 0, 1);
        }

        row = 3; col = 3;
        help_emit_heading("Superfluous", row - 2, col - 1);

        put_role(ROLE_KEY, "i", row, col); put_role(ROLE_UI,  "display inventory", row, col + 2); row++;
        put_role(ROLE_KEY, "e", row, col); put_role(ROLE_UI,  "display equipped items", row, col + 2); row += 2;
        put_role(ROLE_KEY, "g", row, col); put_role(ROLE_UI,  "get", row, col + 2); row++;
        put_role(ROLE_KEY, "w", row, col); put_role(ROLE_UI,  "wear/wield", row, col + 2); row++;
        if (angband_keyset) put_role(ROLE_KEY, "t", row, col); else put_role(ROLE_KEY, "r", row, col); put_role(ROLE_UI,  "remove", row, col + 2); row++;
        put_role(ROLE_KEY, "E", row, col); put_role(ROLE_UI,  "eat food", row, col + 2); row++;
        put_role(ROLE_KEY, "q", row, col); put_role(ROLE_UI,  "quaff potion", row, col + 2); row++;
        if (angband_keyset) put_role(ROLE_KEY, "u", row, col); else put_role(ROLE_KEY, "a", row, col); put_role(ROLE_UI,  "activate staff", row, col + 2); row++;
        put_role(ROLE_KEY, "^a", row, col); put_role(ROLE_UI,  "swap staff", row, col + 2); row++;
        put_role(ROLE_KEY, "p", row, col); put_role(ROLE_UI,  "play instrument", row, col + 2); row += 2;

        put_role(ROLE_KEY, "o", row, col); put_role(ROLE_UI,  "open door/chest", row, col + 2); row++;
        put_role(ROLE_KEY, "c", row, col); put_role(ROLE_UI,  "close door", row, col + 2); row++;
        put_role(ROLE_KEY, "b", row, col); put_role(ROLE_UI,  "bash door", row, col + 2); row++;
        put_role(ROLE_KEY, "D", row, col); put_role(ROLE_UI,  "disarm trap", row, col + 2); row++;
        put_role(ROLE_KEY, "T", row, col); put_role(ROLE_UI,  "tunnel", row, col + 2); row++;
        put_role(ROLE_KEY, ">", row, col); put_role(ROLE_UI,  "descend stairs", row, col + 2); row++;
        put_role(ROLE_KEY, "<", row, col); put_role(ROLE_UI,  "ascend stairs", row, col + 2); row++;
        put_role(ROLE_KEY, "0", row, col); put_role(ROLE_UI,  "forge an item", row, col + 2); row++;

        row = 3; col = 34;
        help_emit_heading("Advanced", row - 2, col);

        put_role(ROLE_KEY,  " :", row, col); put_role(ROLE_UI,   "write a note", row, col + 3); row++;
        put_role(ROLE_KEY,  " )", row, col); put_role(ROLE_UI,   "save screen shot", row, col + 3); row += 2;
        if (angband_keyset) put_role(ROLE_KEY, " @", row, col); else put_role(ROLE_KEY, " $", row, col); put_role(ROLE_UI,   "set macros", row, col + 3); row++;
        put_role(ROLE_KEY,  " &", row, col); put_role(ROLE_UI,   "set colours", row, col + 3); row += 2;
        put_role(ROLE_KEY,  "^p", row, col); put_role(ROLE_UI,   "display prior messages", row, col + 3); row++;
        put_role(ROLE_KEY,  "^r", row, col); put_role(ROLE_UI,   "redraw screen", row, col + 3); row++;
        put_role(ROLE_KEY,  "^e", row, col); put_role(ROLE_UI,   "switch inven/equip display in windows", row, col + 3); row++;
        put_role(ROLE_KEY,  " V", row, col); put_role(ROLE_UI,   "version information", row, col + 3); row++;

        row = 16; col = 35; col2 = 43;
        help_emit_heading("hjkl movement", row - 2, col - 1);

        put_role(ROLE_KEY,   "y k u", row, col);
        put_role(ROLE_SUBTLE," \\|/ ", row + 1, col);
        put_role(ROLE_KEY,   "h z l", row + 2, col);
        put_role(ROLE_SUBTLE,"-", row + 2, col + 1);
        put_role(ROLE_SUBTLE,"-", row + 2, col + 3);
        put_role(ROLE_SUBTLE," /|\\ ", row + 3, col);
        put_role(ROLE_KEY,   "b j n", row + 4, col);

        put_role(ROLE_SUBTLE, "If the hjkl movement option is on", row, col2);
        put_role(ROLE_SUBTLE, "then these keys move you around", row + 1, col2);
        put_role(ROLE_SUBTLE, "Use shift to 'run'", row + 3, col2); put_role(ROLE_KEY, "shift", row + 3, col2 + 4);
        put_role(ROLE_SUBTLE, "Use control for the underlying", row + 4, col2); put_role(ROLE_KEY, "control", row + 4, col2 + 4);
        put_role(ROLE_SUBTLE, "key-commands", row + 5, col2);
        break;
    }

    case 8:
    {
        /* SIL-MORE: HELP [8/8]: STEAM DECK CONTROLS */
        row = 0; col = 1;
        if (include_header)
        {
            sprintf(page_header, "SIL-MORE: SHINING DARKNESS - HELP [%d/%d]: STEAM DECK CONTROLS", i, HELP_TOTAL_PAGES);
            put_role(ROLE_HEADER, page_header, row, col);
        }
        row += 2;

        /* Movement and Action Controls */
        col = 1;
        help_emit_heading("MOVEMENT & ACTION", row, col); row += 2;
        put_role(ROLE_KEY, "D-pad / Left Stick", row, col);
        put_role(ROLE_BODY, " - Movement", row, col + 22); row++;

        char action_buf[96];
        int binding = 0;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_SOUTH);
        binding_action_label(binding, action_buf, sizeof(action_buf));
        put_role(ROLE_KEY, "A", row, col); put_role(ROLE_BODY, " - ", row, col + 2);
        put_role(ROLE_BODY, action_buf, row, col + 5); row++;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_WEST);
        binding_action_label(binding, action_buf, sizeof(action_buf));
        put_role(ROLE_KEY, "X", row, col); put_role(ROLE_BODY, " - ", row, col + 2);
        put_role(ROLE_BODY, action_buf, row, col + 5); row++;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_NORTH);
        binding_action_label(binding, action_buf, sizeof(action_buf));
        put_role(ROLE_KEY, "Y", row, col); put_role(ROLE_BODY, " - ", row, col + 2);
        put_role(ROLE_BODY, action_buf, row, col + 5); row++;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_EAST);
        binding_action_label(binding, action_buf, sizeof(action_buf));
        put_role(ROLE_KEY, "B", row, col); put_role(ROLE_BODY, " - ", row, col + 2);
        put_role(ROLE_BODY, action_buf, row, col + 5); row++;

        {
            char rs_up[24];
            char rs_down[24];
            char rs_left[24];
            char rs_right[24];
            char rs_line[120];
            binding_action_short(get_sdl_gamepad_right_stick_binding(GAMEPAD_STICK_DIR_UP), rs_up, sizeof(rs_up));
            binding_action_short(get_sdl_gamepad_right_stick_binding(GAMEPAD_STICK_DIR_DOWN), rs_down, sizeof(rs_down));
            binding_action_short(get_sdl_gamepad_right_stick_binding(GAMEPAD_STICK_DIR_LEFT), rs_left, sizeof(rs_left));
            binding_action_short(get_sdl_gamepad_right_stick_binding(GAMEPAD_STICK_DIR_RIGHT), rs_right, sizeof(rs_right));
            strnfmt(rs_line, sizeof(rs_line), "Up:%s  Down:%s  Left:%s  Right:%s",
                    rs_up, rs_down, rs_left, rs_right);
            put_role(ROLE_KEY, "Right Stick", row, col);
            put_role(ROLE_BODY, " - ", row, col + 11);
            put_role(ROLE_BODY, rs_line, row, col + 14);
            row++;
        }

        row += 1;

        /* Left and right side controls */
        int left_header_row = row;
        int left_start_row = row + 2;
        help_emit_heading("LEFT SIDE CONTROLS", left_header_row, col);

        row = left_start_row;
        const char* input = NULL;
        int text_col = 0;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "L1 (Bumper)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        binding = get_sdl_gamepad_trigger_binding(0);
        binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "L2 (Trigger)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_LEFT_PADDLE1);
        binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "L4 (Back)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_LEFT_PADDLE2);
        binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "L5 (Back)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        binding = get_sdl_gamepad_shoulder_combo_binding();
        binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "L1+R1 Combo";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        int left_end_row = row;

        col = 42;
        row = left_header_row;
        help_emit_heading("RIGHT SIDE CONTROLS", row, col);
        row = left_start_row;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "R1 (Bumper)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        binding = get_sdl_gamepad_trigger_binding(1);
        binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "R2 (Trigger)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1);
        binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "R4 (Back)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2);
        binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "R5 (Back)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_START);
        binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "Start (Menu)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        binding = get_sdl_gamepad_button_binding(SDL_GAMEPAD_BUTTON_BACK);
        binding_action_label(binding, action_buf, sizeof(action_buf));
        input = "Back (View)";
        put_role(ROLE_KEY, input, row, col);
        text_col = col + (int)strlen(input);
        put_role(ROLE_BODY, " - ", row, text_col);
        put_role(ROLE_BODY, action_buf, row, text_col + 3); row++;

        int right_end_row = row;

        row = (left_end_row > right_end_row) ? left_end_row : right_end_row;
        row += 1;
        {
            char shift_label[16];
            char note_buf[120];
            help_prompt_label(GAMEPAD_BIND_SHIFT, "L2", shift_label, sizeof(shift_label));
            strnfmt(note_buf, sizeof(note_buf),
                    "Shift: %s+A Rest, %s+B 2nd quiver, %s+X Examine, %s+Y Stealth",
                    shift_label, shift_label, shift_label, shift_label);
            put_role(ROLE_SUBTLE, note_buf, row, 1);
        }

        row += 1;
        {
            char ctrl_label[16];
            char note_buf[120];
            help_prompt_label(GAMEPAD_BIND_CTRL, "R2", ctrl_label, sizeof(ctrl_label));
            strnfmt(note_buf, sizeof(note_buf),
                    "Ctrl:  %s+A Wait, %s+B Fletch, %s+X Exchange, %s+Y Smith",
                    ctrl_label, ctrl_label, ctrl_label, ctrl_label);
            put_role(ROLE_SUBTLE, note_buf, row, 1);
        }

        row += 1;
        {
            char shift_label[16];
            char ctrl_label[16];
            char note_buf[120];
            help_prompt_label(GAMEPAD_BIND_SHIFT, "L2", shift_label, sizeof(shift_label));
            help_prompt_label(GAMEPAD_BIND_CTRL, "R2", ctrl_label, sizeof(ctrl_label));
            strnfmt(note_buf, sizeof(note_buf),
                    "Shoulders: %s+L1 Map, %s+R1 Horn, %s+L1 Activate, %s+R1 Supplies",
                    shift_label, shift_label, ctrl_label, ctrl_label);
            put_role(ROLE_SUBTLE, note_buf, row, 1);
        }

        row += 1;
        {
            char ctrl_label[16];
            char back_label[16];
            char note_buf[120];
            help_prompt_label(GAMEPAD_BIND_CTRL, "R2", ctrl_label, sizeof(ctrl_label));
            help_prompt_label('h', "Back", back_label, sizeof(back_label));
            strnfmt(note_buf, sizeof(note_buf),
                    "View:  %s Character, %s+%s Abilities",
                    back_label, ctrl_label, back_label);
            put_role(ROLE_SUBTLE, note_buf, row, 1);
        }

        row += 1;
        put_role(ROLE_SUBTLE, "Customize bindings via Options -> Controller Settings.", row, 1);
        
        break;
    }
    }
}

#undef put_role
#undef c_put_str



/* Click targets for pointer / touch navigation in the help viewer. */
enum {
    HELP_CLICK_PREV = 1,
    HELP_CLICK_NEXT,
    HELP_CLICK_QUIT
};

/*
 * Peruse the On-Line-Help
 */
void do_cmd_help(void)
{
    int i = 1;
    int ch; /* int (not char) so EOF and negative key bindings compare correctly */
    bool row_has_content[HELP_DOC_MAX_ROWS];
    bool row_has_heading[HELP_DOC_MAX_ROWS];
    int page_starts[HELP_DOC_MAX_PAGES];
    int page_ends[HELP_DOC_MAX_PAGES];
    int doc_hgt = 0;

    /* Clear any active banner before opening help */
    extern int g_banner_force_redraw_remaining;
    if (g_banner_force_redraw_remaining > 0) {
        g_banner_force_redraw_remaining = 0;
        do_cmd_redraw();
    }

    /* Save screen */
    screen_save();
    screen_push_supporting_panes_hidden();
    sdl_push_terminal_menu_scale();
    if (p_ptr && p_ptr->playing)
        sdl_music_play_menu_theme();

    /* Interact until done */
    while (1)
    {
        int wid, hgt;
        int total_pages;
        int compact_doc_hgt = 0;
        bool compact_dynamic = false;
        bool legacy;

        /* Get current terminal size before deciding layout */
        Term_get_size(&wid, &hgt);
        legacy = help_use_legacy_layout(wid, hgt);
        compact_dynamic = (!legacy && wid < 80);

        if (legacy)
        {
            total_pages = HELP_TOTAL_PAGES;
        }
        else
        {
            /* Rebuild each time so controller bindings / options are current */
            help_build_document_ops(&doc_hgt, row_has_content, row_has_heading);
            if (compact_dynamic)
            {
                compact_doc_hgt = help_build_compact_display_rows(wid, doc_hgt);
                total_pages = help_dynamic_build_display_pages(
                    hgt,
                    compact_doc_hgt,
                    page_starts,
                    page_ends);
            }
            else
            {
                total_pages = help_dynamic_build_document_pages(
                    hgt,
                    doc_hgt,
                    row_has_content,
                    row_has_heading,
                    page_starts,
                    page_ends);
            }
        }

        if (total_pages < 1)
            total_pages = 1;

        if (i < 1)
            i = 1;
        if (i > total_pages)
            i = total_pages;

        /* Clear screen */
        Term_clear();

        if (legacy)
        {
            show_help_screen_legacy(i, true);
        }
        else if (compact_dynamic)
        {
            int start_row = page_starts[i - 1];
            int end_row = page_ends[i - 1];
            show_help_screen_compact_document(i, total_pages, hgt, start_row, end_row);
        }
        else
        {
            int start_y = page_starts[i - 1];
            int end_y = page_ends[i - 1];
            show_help_screen_dynamic_document(i, total_pages, hgt, start_y, end_y);
        }

        /* Navigation prompt + pointer / touch hit regions */
        {
            char nav[192];
            int nav_row = hgt - 1;
            int mid_col = wid / 2;

            if (sdl_touch_only_device_active()) {
                SDL_strlcpy(nav,
                    "Swipe or tap left/right to turn pages   "
                    "tap Exit to close",
                    sizeof(nav));
            } else if (steamdeck_controls_active()) {
                char prev_label[16];
                char next_page_label[16];
                char next_label[16];
                char back_label[16];
                help_prompt_label(steamdeck_prev_page_key(), "L1",
                    prev_label, sizeof(prev_label));
                help_prompt_label(steamdeck_next_page_key(), "R1",
                    next_page_label, sizeof(next_page_label));
                help_prompt_label(steamdeck_confirm_key(), "A", next_label,
                    sizeof(next_label));
                help_prompt_label(steamdeck_back_key(), "B", back_label,
                    sizeof(back_label));
                strnfmt(nav, sizeof(nav),
                    "Navigation: [%s/%s] Prev/Next  [%s] Next  [%s] Back",
                    prev_label, next_page_label, next_label, back_label);
            } else {
                strnfmt(nav, sizeof(nav),
                    "Prev   Next   Quit    "
                    "[Left] Prev  [Right] Next  [X+1-%d] Page  [Q/Esc] Quit",
                    total_pages);
            }
            c_put_str(TERM_WHITE, nav, nav_row, 1);

            /*
             * Pointer / touch navigation.  The footer words are clickable
             * (mouse), a floating Exit button covers touch quit, and the page
             * body is split into a left ("previous") and right ("next") tap
             * zone that also accept page-turning swipes and the mouse wheel.
             * The scroll areas inject the same '4'/'6' keys the keyboard path
             * uses, so they flow through the navigation switch below.
             */
            ui_menu_click_begin();
            ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);
            ui_menu_click_set_touch_exit_button(true);
            ui_menu_click_add_text_token(HELP_CLICK_PREV, 1, nav_row, nav,
                "Prev");
            ui_menu_click_add_text_token(HELP_CLICK_NEXT, 1, nav_row, nav,
                "Next");
            ui_menu_click_add_text_token(HELP_CLICK_QUIT, 1, nav_row, nav,
                "Quit");

            if (mid_col < 1)
                mid_col = 1;
            ui_scroll_area_begin_cols(0, mid_col - 1, 2, nav_row - 1,
                SDL_TOUCH_MENU_CATEGORY_OTHER);
            ui_scroll_area_set_keys('4', '6', '4', '6');
            ui_scroll_area_set_tap_key('4');
            ui_scroll_area_set_page_mode(true);
            ui_scroll_area_add_cols(mid_col, wid - 1, 2, nav_row - 1,
                SDL_TOUCH_MENU_CATEGORY_OTHER);
            ui_scroll_area_set_keys('4', '6', '4', '6');
            ui_scroll_area_set_tap_key('6');
            ui_scroll_area_set_page_mode(true);
        }

        ch = inkey();

        /* Resolve a mouse / touch click into a navigation command. */
        {
            int choice = 0;
            int action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&choice, &action)) {
                ui_menu_click_clear();
                if (action != UI_MENU_CLICK_HOVER) {
                    switch (choice) {
                    case HELP_CLICK_PREV: ch = '4'; break;
                    case HELP_CLICK_NEXT: ch = '6'; break;
                    case HELP_CLICK_QUIT: ch = ESCAPE; break;
                    default: break;
                    }
                }
            } else {
                ui_menu_click_clear();
            }
        }
        ui_scroll_area_clear();

        /* Bare wake (hover or non-actionable click): redraw without paging. */
        if (ch == UI_MENU_CLICK_WAKE_KEY) {
            message_flush();
            continue;
        }

        ch = steamdeck_menu_key(ch, '4', '6');

        /* Enhanced navigation */
        if (ch != EOF)
        {
            /* Quit commands */
            if ((ch == 'q') || (ch == 'Q') || (ch == ESCAPE))
            {
                break;
            }
            /* Previous page */
            else if ((ch == '8') || (ch == '-') || (ch == '4'))
            {
                i--;
                if (i < 1)
                    i = 1;
            }
            /* Next page */
            else if ((ch == '2') || (ch == '6') || (ch == ' ') || (ch == '\r') || (ch == '\n'))
            {
                i++;
            }
            /* Direct page navigation with 'x' prefix */
            else if (ch == 'x' || ch == 'X')
            {
                char prompt[32];
                char tmp[8];
                strnfmt(prompt, sizeof(prompt), "Page (1-%d): ", total_pages);
                prt(prompt, hgt - 1, 0);
                SDL_strlcpy(tmp, "1", sizeof(tmp));
                if (askfor_aux(tmp, sizeof(tmp)))
                {
                    int target = atoi(tmp);
                    if ((target >= 1) && (target <= total_pages))
                        i = target;
                }
            }
            /* Default: next page */
            else
            {
                i++;
            }
        }

        /* Done */
        if (i > total_pages)
            break;

        /* Flush messages */
        message_flush();
    }

    /* Load screen */
    sdl_pop_terminal_menu_scale();
    screen_pop_supporting_panes_hidden();
    screen_load();
    if (p_ptr && p_ptr->playing)
        sdl_music_stop_main();
}

/*
 * Process the player name and extract a clean "base name".
 *
 * If "sf" is true, then we initialize "savefile" based on player name.
 *
 * Some platforms (Windows) leave the "savefile" empty when a new 
 * character is created, and then when the character is done being 
 * created, they call this function to choose a new savefile name.
 */
