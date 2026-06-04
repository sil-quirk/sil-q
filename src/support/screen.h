#ifndef INCLUDED_SUPPORT_SCREEN_H
#define INCLUDED_SUPPORT_SCREEN_H

#include "h-basic.h"

bool screen_saved_fullscreen_active(void);
void screen_push_supporting_panes_hidden(void);
void screen_pop_supporting_panes_hidden(void);
bool screen_supporting_panes_hidden_active(void);
void screen_set_startup_supporting_panes_hidden(bool hidden);
bool screen_startup_supporting_panes_hidden_active(void);
void screen_push_touch_pane_hidden(void);
void screen_pop_touch_pane_hidden(void);
bool screen_touch_pane_hidden_active(void);
void screen_set_startup_touch_pane_hidden(bool hidden);
bool screen_startup_touch_pane_hidden_active(void);
void screen_push_touch_pane_proto(void);
void screen_pop_touch_pane_proto(void);
bool screen_touch_pane_proto_active(void);
void ui_reset_transient_state_for_new_session(void);
void screen_clear_all_terms_no_fresh(void);
void screen_save(void);
void screen_load(void);
void screen_load_quiet(void);
void move_cursor(int row, int col);
void c_put_str(byte attr, cptr str, int row, int col);
void put_str(cptr str, int row, int col);
void c_prt(byte attr, cptr str, int row, int col);
void prt(cptr str, int row, int col);
void clear_from(int row);

#endif /* INCLUDED_SUPPORT_SCREEN_H */
