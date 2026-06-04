#ifndef SIL_UI_STATUS_INTERNAL_H
#define SIL_UI_STATUS_INTERNAL_H

#include "angband.h"

typedef struct hidden_overlay_line {
    char text[32];
    char short_text[16];
    byte attr;
    bool has_icon;
    byte icon_attr;
    char icon_char;
    byte pointer_attack_mode;
    byte click_action;
} hidden_overlay_line;

#define STATUS_MAIN_MENU_HINT "main menu"
#define STATUS_VIEW_LABEL "View"
#define STATUS_VIEW_LABEL_SHORT "Vw"

bool ui_hide_left_panel(void);
bool ui_status_system_compact(void);
bool ui_compact_height(void);
bool ui_compact_status_line_handles_song(void);
bool ui_compact_status_line_handles_wounds(void);
bool ui_wound_rows_overlap_status_line(void);
bool ui_status_pane_owns_left_panel_statuses(void);
bool ui_depth_menu_owns_left_panel_depth(void);

bool status_state_text(char* out_long, size_t out_long_sz,
    char* out_short, size_t out_short_sz, byte* out_attr);
byte panel_touch_zone_attr(int action, int row, byte base_attr);
byte status_touch_zone_attr(int action, int col, int width, byte base_attr);
void prt_status_line_compact(void);
void prt_cut_poisoned_compact(void);
void prt_status_line_main_menu_hint(bool compact_centered);
void prt_status_line_view_button(void);

void prt_player_name(void);
void prt_stat(int stat);
void prt_exp(void);
void prt_mel(void);
void prt_arc(void);
void prt_quiver(void);
void prt_evn(void);
void prt_hp(void);
void prt_char_health_graphic(void);
void prt_light(void);
void prt_sp(void);
void prt_hidden_top_vitals(void);
void redraw_hidden_left_panel_overlay(void);

void prt_song(void);
void prt_depth(void);
void prt_hunger(void);
void prt_blind(void);
void prt_confused(void);
void prt_afraid(void);
void prt_cut(void);
void prt_poisoned(void);
void prt_state(void);
void prt_speed(void);
void prt_partition(void);
void prt_terrain(void);
void prt_stun(void);

void health_redraw(void);

bool hidden_left_panel_visible(void);
int hidden_left_panel_build_lines(hidden_overlay_line* lines, int max_lines);
bool hidden_left_panel_sync_mask(
    const hidden_overlay_line* lines, int line_count);

#endif
