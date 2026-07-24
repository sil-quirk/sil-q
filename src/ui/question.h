#ifndef INCLUDED_UI_QUESTION_H
#define INCLUDED_UI_QUESTION_H

#include "h-basic.h"

/*
 * A selectable answer in a question overlay.  `key` is the letter shortcut
 * (0 for none); `label` is the row text; `attr` its TERM_* colour.
 */
typedef struct ui_question_option {
    char key;
    cptr label;
    byte attr;
    bool disabled;
} ui_question_option;

typedef struct ui_question_button {
    int choice;
    char key;
    cptr label;
    byte attr;
    bool disabled;
} ui_question_button;

/*
 * Ask a question through the SDL question overlay (never the top message
 * row).  Local questions pass the map grid they are about in
 * anchor_y/anchor_x so the panel spawns next to the object; pass
 * UI_QUESTION_GLOBAL for both to centre it on the map instead.
 *
 * Blocks in an inkey loop: arrows scroll the highlighted answer,
 * Enter/Space/click/tap confirm, letter shortcuts pick directly, Escape
 * cancels.  Disabled choices are drawn in grey, skipped by arrow navigation,
 * and cannot be confirmed by any input method.  Returns the chosen option
 * index, or -1 on cancel.
 */
#define UI_QUESTION_GLOBAL (-1)

int ui_question_ask(cptr title, cptr desc, const ui_question_option* options,
    int count, int anchor_y, int anchor_x, int default_index);

/*
 * Same modal overlay, but preserves whatever screen/menu is already painted
 * behind it.  Use this for in-menu value pickers; ui_question_ask() still
 * repaints the map first for dungeon-local questions.
 */
int ui_question_ask_overlay(cptr title, cptr desc,
    const ui_question_option* options, int count, int anchor_y, int anchor_x,
    int default_index);

int ui_question_ask_overlay_buttons(cptr title, cptr desc,
    const ui_question_option* options, int count,
    const ui_question_button* buttons, int button_count, int anchor_y,
    int anchor_x, int default_index);

#endif /* INCLUDED_UI_QUESTION_H */
