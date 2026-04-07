/* ui/story_font.h - Story font helpers and rendering utilities */

#ifndef INCLUDED_UI_STORY_FONT_H
#define INCLUDED_UI_STORY_FONT_H

#include "../h-basic.h"

typedef struct term term;

typedef struct story_font_term_state
{
    term* t;
    bool active;
    bool grid;
} story_font_term_state;

int count_wrapped_lines_story(cptr str, int wrap_cols, int indent);

bool story_inventory_enabled(void);
bool story_equipment_enabled(void);
bool story_look_enabled(void);
bool story_character_enabled(void);
bool story_monster_desc_enabled(void);

void story_font_term_push(bool active, bool grid, story_font_term_state* prev);
void story_font_term_pop(story_font_term_state* prev);

void text_out_to_screen_story(byte a, cptr str);

void story_print_text(int row, int col, int max_cols, byte attr, cptr text);
void story_print_text_grid(int row, int col, int max_cols, byte attr, cptr text);
void story_print_mono(int row, int col, byte attr, cptr text);
void story_fill_rect(int row, int col, int width_cols, byte attr);

#endif /* INCLUDED_UI_STORY_FONT_H */
