#ifndef INCLUDED_SCORE_IO_H
#define INCLUDED_SCORE_IO_H

#include "h-basic.h"
#include <SDL3/SDL.h>

typedef struct score_file_ctx {
    SDL_IOStream* fd;
    byte version_major;
    byte version_minor;
    byte version_patch;
    byte version_extra;
    u32b entry_count;
} score_file_ctx;

struct high_score;

/* Swap the active score-file context; returns the previous context pointer. */
score_file_ctx* score_file_set_active_ctx(score_file_ctx* ctx);

/* Returns the currently active context (global by default). */
score_file_ctx* score_file_active_ctx(void);

/* Accessor for the module's default/global context. */
score_file_ctx* score_file_global_ctx(void);

/* Utility: check if a score file version supports the curse (pts) field. */
bool scores_version_has_curses(const score_file_ctx* ctx);
bool scores_version_has_run_links(const score_file_ctx* ctx);

bool score_file_load_header(score_file_ctx* ctx, const char *filepath);
SDL_IOStream* score_file_open(const char *filepath, int mode);
bool build_current_score_path(char* buf, size_t len);

/* Convenience helper for zeroing a context before use. */
void score_file_reset_ctx(score_file_ctx* ctx);

int collect_high_scores(struct high_score* out, int capacity, bool sort_by_score);
int collect_story_high_scores(struct high_score* out, int capacity,
                              bool sort_by_score);
int highscore_seek(int index);
errr highscore_read(struct high_score* out);
int highscore_write(const struct high_score* entry);
errr backup_scores_file(const char *filepath);
int score_count_alive_entries(void);
int score_count_story_alive_entries(void);
int score_count_alive_entries_at_path(const char* score_path);
bool score_count_story_alive_entries_checked(int* alive_count);
bool score_count_alive_entries_at_path_checked(const char* score_path,
                                                int* alive_count);
bool score_story_ledger_exists(void);
u32b score_sum_dead_points(void);
u32b score_sum_story_dead_points(void);
bool score_mark_alive_entry_dead_at_path(const char* score_path,
                                         const struct high_score* target,
                                         const char* cause);
bool score_remove_alive_entry_at_path(const char* score_path,
                                      const struct high_score* target);
int highscore_add(struct high_score* score);
void upsert_live_score_on_save(void);
int highscore_dead(char* name);

#endif /* INCLUDED_SCORE_IO_H */
