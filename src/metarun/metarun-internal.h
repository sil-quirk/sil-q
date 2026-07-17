#ifndef INCLUDED_METARUN_INTERNAL_H
#define INCLUDED_METARUN_INTERNAL_H

#ifndef WINDOWS
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#endif

#include "../metarun.h"

#include "blitz.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "metarun_legacy.h"
#include "sdl-sound.h"
#include "h-define.h"
#include "platform.h"
#include "supplies.h"

#include <SDL3/SDL.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef WINDOWS
#include <windows.h>
#else
#include <dirent.h>
#include <sys/types.h>
#endif

/* Enable this to delete old save/score files on fresh metarun start. */
/* #define METARUN_CLEANUP_OLD_FILES */

#define CURSE_MENU_LINES 3

extern metarun *metaruns;
extern s16b metarun_max;
extern s16b current_run;

/* Shared state helpers */
bool sync_current_metarun_slot(bool stamp_time);

/* Score and blessing economy helpers */
void update_blessing_ledger(metarun *m);
int major_blessing_capacity(void);
const major_blessing_type *major_blessing_def(int idx);
cptr major_blessing_name_str(int idx);
cptr major_blessing_short_desc(int idx);
cptr major_blessing_detail_desc(int idx);
cptr major_blessing_unlock_msg(int idx);
int major_blessing_cost(int idx);
metarun_major_effect major_blessing_effect(int idx);
u32b runtype_threshold_for_mode(int runtype_id, metarun_blessing_threshold_mode mode);
u32b metarun_threshold_value(const metarun *m);
const char *threshold_mode_name(metarun_blessing_threshold_mode mode);
u32b get_best_run_score_from_highscores(void);
u32b compute_metarun_score(const metarun *m);

/* Persistence helpers */
bool build_meta_path(char *buf, size_t len, const metarun *m, const char *leaf);
void reset_defaults(metarun *m);
void apply_difficulty_curses(metarun *m);
void ensure_run_dir(const metarun *m);

/* Lifecycle helpers */
int required_survivor_target(int win_goal);
bool start_new_metarun(void);

/* Shared metarun UI helpers */
void metarun_prompt_label(int binding, const char *fallback, char *buf, size_t buflen);
int metarun_term_width(void);
char metarun_inkey_hidden(void);
void metarun_wait_hidden(void);
void print_heading_fade(cptr title, byte final_attr);
bool print_paragraph_fade(cptr txt, byte final_attr, int row);
void wait_for_keypress_with_prompt(cptr prompt);
cptr curse_display_name(int idx);
cptr blessing_display_name(int idx);
cptr metarun_display_pad(char *buf, size_t size, cptr name, int cols);
void open_blessing_exchange(void);
int metarun_inline_minor_blessing_choices(int out[3]);
bool metarun_inline_choose_minor_blessing(int id);
bool metarun_inline_choose_major_blessing(int idx);
bool metarun_inline_remove_curse(int id);
void show_all_active_curses(void);
void choose_difficulty_menu(void);
bool metarun_set_difficulty_inline(int choice);

#endif /* INCLUDED_METARUN_INTERNAL_H */
