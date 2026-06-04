/* File: object/object-info.h */

#ifndef INCLUDED_OBJECT_INFO_H
#define INCLUDED_OBJECT_INFO_H

#include "angband.h"

#ifndef OBJECT_INFO_SCREEN_ACTION_DEFINED
#define OBJECT_INFO_SCREEN_ACTION_DEFINED
typedef struct object_info_screen_action
{
    int key;
    cptr token;
} object_info_screen_action;
#endif

bool object_info_out(const object_type* o_ptr);
cptr object_lore_select_base_text(const object_type* o_ptr, char* out,
    size_t out_sz);
void note_info_screen(const object_type* o_ptr);
void object_info_screen(const object_type* o_ptr);
void object_info_screen_multi(const object_type** objects, const char** headings,
    int count);
char object_info_screen_multi_with_actions(const object_type** objects,
    const char** headings, int count, cptr footer,
    const object_info_screen_action* actions, int action_count);
bool object_info_overlay_show_multi(const object_type** objects,
    const char** headings, int count);
void object_info_overlay_clear(void);

#endif /* INCLUDED_OBJECT_INFO_H */
