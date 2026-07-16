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
    int detail_ability_rows_hint;
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

int get_player_choice(birth_menu* choices, int num, int def,
    void (*hook)(birth_menu), const birth_select_page* page);

int collect_character_trait_lines(int race, int character, bool short_labels,
    birth_compact_flag_line out[], int out_max, int* max_line_len);
int collect_character_starting_abilities(int character, cptr out[],
    int out_max, int out_skill[], int out_ability[]);
void birth_format_ability_hint(int skill, int ability, char* buf,
    size_t buflen);
void birth_format_trait_hint(const birth_compact_flag_line* line,
    char* buf, size_t buflen);

bool birth_character_is_set(int bit);

NavResult select_oath(void);
NavResult blitz_configure_effects(void);
NavResult blitz_auto_build_character(void);
NavResult player_birth_aux_2(int stats[A_MAX]);

#endif /* INCLUDED_BIRTH_INTERNAL_H */
