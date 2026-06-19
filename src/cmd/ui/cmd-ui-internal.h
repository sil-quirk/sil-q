#ifndef INCLUDED_CMD_UI_INTERNAL_H
#define INCLUDED_CMD_UI_INTERNAL_H

#include "angband.h"
#include "fs/io_sdl.h"

/* String used to show a color sample */
#define COLOR_SAMPLE "###"

/* max length of note output */
#define LINEWRAP 75

/* used for knowledge display */
#define BROWSER_ROWS 16

#define SUPPLY_COMPACT_TERM_WIDTH 80

enum
{
    SUPPLY_CLICK_BACK = -1,
    SUPPLY_CLICK_RECALL = -2,
    SUPPLY_CLICK_USE = -3,
    SUPPLY_CLICK_DROP = -4,
    SUPPLY_CLICK_PAGE_EQUIPPED = -5,
    SUPPLY_CLICK_PAGE_INVENTORY = -6,
    SUPPLY_CLICK_PAGE_SUPPLIES = -7,
    SUPPLY_CLICK_PREVIEW = -8,
    SUPPLY_CLICK_DELETE = -9,
    SUPPLY_CLICK_TAB = -10,
    SUPPLY_CLICK_GROUP_BASE = 1000,
    SUPPLY_CLICK_ENTRY_BASE = 10000
};

#define KNOWLEDGE_CLICK_BACK -1
#define KNOWLEDGE_CLICK_RECALL -2
#define KNOWLEDGE_CLICK_PREV_PAGE -3
#define KNOWLEDGE_CLICK_NEXT_PAGE -4
#define KNOWLEDGE_CLICK_TAB_BASE 100000
#define KNOWLEDGE_CLICK_GROUP_BASE 200000
#define KNOWLEDGE_CLICK_ENTRY_BASE 300000

typedef struct monster_list_entry monster_list_entry;
struct monster_list_entry
{
    s16b r_idx;
    byte amount;
};

typedef struct object_list_entry object_list_entry;
struct object_list_entry
{
    enum
    {
        OBJ_NONE,
        OBJ_NORMAL,
        OBJ_SPECIAL
    } type;
    int idx;
    int e_idx;
    int tval, sval;
};

typedef struct supply_list_entry supply_list_entry;
typedef struct supply_list_columns supply_list_columns;
typedef struct supply_group_icon supply_group_icon;
typedef struct equipment_list_entry equipment_list_entry;

struct supply_list_entry
{
    int item_idx;
    int k_idx;
    int total;
    int supply_idx;
    int equip_idx;
    int preset_idx;
    int floor_idx;
    bool equipped;
    bool single_item_display;
};

struct supply_list_columns
{
    int name_col;
    int name_w;
    int weight_col;
    int turns_col;
    int qty_col;
    int sym_hdr_col;
    int sym_col;
    bool show_weight;
    bool show_turns;
    bool show_qty;
    bool show_sym;
};

struct supply_group_icon
{
    bool has_icon;
    object_type obj;
};

typedef enum equipment_entry_placeholder
{
    EQUIPMENT_ENTRY_PLACEHOLDER_NONE = 0,
    EQUIPMENT_ENTRY_PLACEHOLDER_EMPTY,
    EQUIPMENT_ENTRY_PLACEHOLDER_RESERVED
} equipment_entry_placeholder;

struct equipment_list_entry
{
    int item_idx;
    int supply_idx;
    int equip_idx;
    int floor_idx;
    enum inventory_limit_group limit_group;
    equipment_entry_placeholder placeholder;
    bool equipped;
    bool show_empty_slot; /* render an empty equip slot as a pickable row */
};

typedef struct knowledge_browser_layout knowledge_browser_layout;
typedef struct knowledge_browser_state knowledge_browser_state;

struct knowledge_browser_layout
{
    int term_wid;
    int term_hgt;
    int title_row;
    int tabs_row;
    int header_row;
    int divider_row;
    int list_row;
    int list_rows;
    int status_row;
    int prompt_row;
    int group_col;
    int group_w;
    int divider_col;
    int list_col;
    int list_w;
};

struct knowledge_browser_state
{
    int column[4];
    int group_cur[4];
    int group_top[4];
    int entry_cur[4];
    int entry_top[4];
    bool tabs_focus;
};

extern int g_knowledge_last_page;

bool indexed_menu_letters_enabled(void);
void indexed_menu_entry_label(char* buf, size_t buflen, int index, cptr text);
int menu_text_display_width(cptr text);
void keyed_menu_entry_label(char* buf, size_t buflen, char key, cptr text);
int indexed_menu_prefix_col(int col);
void indexed_menu_focus_prefix(char* buf, size_t buflen, int index);
void indexed_menu_normal_prefix(char* buf, size_t buflen, int index);
char browser_entry_label_for_index(int index);
int browser_entry_index_from_label(int ch, int entry_cnt);
void browser_entry_label_prefix(char* buf, size_t buflen, int index);
int heavy_armour_desc_current_weight(void);
int heavy_armour_desc_current_evasion_bonus(void);
void controller_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen);
void controller_prompt_label_no_sticks(int binding, const char* fallback,
    char* buf, size_t buflen);
void settings_ui_fit_text(char* buf, size_t buflen, cptr text, int max_chars);
int settings_utf8_prefix_len(cptr text, int max_cols);
void draw_supply_icon(int col, int row, const object_type* o_ptr);
void redraw_inven_equip_subwindows(void);
void redraw_monster_subwindows(void);

#endif /* INCLUDED_CMD_UI_INTERNAL_H */
