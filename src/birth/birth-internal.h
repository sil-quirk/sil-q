#ifndef INCLUDED_BIRTH_INTERNAL_H
#define INCLUDED_BIRTH_INTERNAL_H

#include "angband.h"
#include "externs.h"
#include "birth/birth.h"
#include "blitz.h"
#include "fs/path.h"
#include "log/log.h"
#include "player/killer.h"
#include "sdl-config.h"
#include "z-term.h"
#include "metarun.h"

/* Three-column layout constants (same as cmd4.c) */
#define COL_SKILL 2
#define COL_ABILITY 15
#define COL_DESCRIPTION 25

/* Locations of the tables on the screen */
#define HEADER_ROW 0
#define QUESTION_ROW 1
#define TABLE_ROW 2
#define DESCRIPTION_ROW 15
#define INSTRUCT_ROW 22

#define QUESTION_COL 2
#define RACE_COL 2
#define RACE_AUX_COL 19
#define CLASS_COL 17
#define CLASS_AUX_COL 27
#define TOTAL_AUX_COL 35
#define INVALID_CHOICE 255
#define BIRTH_FALLEN_MARK "\xe2\x80\xa0"

#define BLITZ_MAX_EFFECT_COUNT 9
#define MAX_COST 13

typedef struct birther birther;
typedef struct birth_menu birth_menu;

struct birther
{
    s16b age;
    s16b wt;
    s16b ht;
    s16b sc;
    s16b stat[A_MAX];
    char history[550];
};

struct birth_menu
{
    bool ghost;
    cptr name;
    cptr text;
};

typedef struct birth_select_page
{
    cptr title;
    cptr frame_top;
    cptr intro;
    cptr frame_bottom;
    cptr* group_headings;
    int detail_stat_rows_hint;
    int detail_trait_rows_hint;
    bool open_on_choice_page; /* book mode: open on the list page, not page 0 */
} birth_select_page;

typedef struct birth_compact_flag_line
{
    cptr txt;
    byte attr;
    int side;
    int skill;
    int trait_score;
    bool proficiency;
    u32b aff_flag;
    u32b pen_flag;
    cptr desc_label;
} birth_compact_flag_line;

extern const int birth_stat_costs[11];

int get_start_xp(void);
int curses_stat_adj(int s);
void get_extra(void);
void player_outfit(void);
void finalize_character_creation_selection(void);

int birth_stat_increase_cost(int stat);
int birth_skill_cost(int base, int points);

void clear_question(void);
void birth_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen);
bool birth_confirm_input(int ch, bool steamdeck);
bool birth_confirm_unspent_stat_points(int points_left, bool steamdeck);
int character_choice_index_by_name(cptr choice_name);
bool character_description_has_room(void);
bool character_selection_tight_height(void);
bool character_flags_need_compact_layout(void);
cptr character_selection_header_text(bool character_phase);
void draw_character_selection_header(bool character_phase);
int birth_prompt_row(void);
int birth_description_base_row(void);
int choice_description_row(int visible_rows,
    bool allow_full_description_screen);
int choice_visible_capacity(int num, cptr text,
    bool allow_full_description_screen);
int choice_description_fit_row(int row, int visible_rows, cptr text);
int birth_wrap_col(int indent);
cptr birth_wrap_line(cptr text, int width, char* buf, size_t buflen);
int birth_utf8_prefix_len(cptr text, int max_cols);
int birth_wrapped_line_count(cptr text, int indent);
void birth_put_wrapped_text(byte attr, cptr text, int row, int col);
void birth_put_str_fit(byte attr, cptr text, int row, int col);
void birth_invalidate_cells(int col, int row, int width);
int birth_wrapped_entry_lines(cptr entries[], int entry_n, int width,
    int max_entries);
int birth_put_wrapped_entries(byte attr, cptr entries[], int entry_n,
    int row, int col, int width, int max_rows, int max_entries);

void birth_detail_hover_add(int col, int row, int width, cptr desc);
int get_player_choice(birth_menu* choices, int num, int def, int col,
    int wid, void (*hook)(birth_menu), bool allow_full_description_screen,
    const birth_select_page* page);

int collect_character_trait_lines(int race, int character, bool short_labels,
    birth_compact_flag_line out[], int out_max, int* max_line_len);
void birth_format_trait_hint(const birth_compact_flag_line* line,
    char* buf, size_t buflen);
void print_rh_flags(int race, int character, int col, int row);

bool birth_character_is_set(int bit);

NavResult select_oath(void);
NavResult blitz_configure_effects(void);
NavResult blitz_auto_build_character(void);
void birth_register_allocation_prompt_clicks(int row, cptr prompt,
    int col, cptr back_label, cptr confirm_label, cptr quit_label);
void birth_draw_allocation_confirm_status(int row, int col, int end_col,
    cptr status);
void birth_configure_allocation_sheet_layout(bool stats_screen,
    int* skill_first_row_out, int* status_row_out);
void birth_display_skill_allocation_compact(int selected_skill,
    const int old_base[S_MAX], const int skill_gain[S_MAX], int points_left,
    bool steamdeck);
NavResult player_birth_aux_2(int stats[A_MAX]);

#endif /* INCLUDED_BIRTH_INTERNAL_H */
