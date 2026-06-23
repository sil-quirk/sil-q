/* File: object/object-ui-select.c */

#include "angband.h"
#include "externs.h"
#include "object/object-ui-select.h"
#include "object/object-internal.h"
#include "log/log.h"
#include "sdl-config.h"
#include "supplies.h"
#include <ctype.h>


static bool verify_item(cptr prompt, int item)
{
    char o_name[80];

    char out_val[160];

    object_type* o_ptr;

    if (inventory_item_is_supply_summary(item))
        return true;

    o_ptr = inventory_item_to_object_ptr(item);
    if (!o_ptr)
        return false;

    if (!o_ptr->k_idx)
    {
        if (item >= INVEN_WIELD && item < INVEN_TOTAL && item_tester_okay(o_ptr))
        {
            strnfmt(out_val, sizeof(out_val), "%s %s? ", prompt,
                describe_empty_slot(item));
            return get_check(out_val);
        }

        return false;
    }

    /* Describe */
    if (item < 0)
        object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
    else
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

    /* Prompt */
    strnfmt(out_val, sizeof(out_val), "%s %s? ", prompt, o_name);

    /* Query */
    return (get_check(out_val));
}

/*
 * Hack -- allow user to "prevent" certain choices.
 *
 * The item can be negative to mean "item on floor".
 */
bool get_item_allow(int item)
{
    if (inventory_item_is_supply_summary(item))
        return true;

    object_type* o_ptr = inventory_item_to_object_ptr(item);
    if (!o_ptr)
        return false;

    if (!o_ptr->k_idx)
    {
        if (item >= INVEN_WIELD && item < INVEN_TOTAL && item_tester_okay(o_ptr))
            return true;

        return false;
    }

    /* Allow it */
    return (true);
}

/*
 * Verify the "okayness" of a given item.
 *
 * The item can be negative to mean "item on floor".
 */
bool get_item_okay(int item)
{
    if (inventory_item_is_supply_summary(item))
        return supplies_visible_for_current_filter();

    object_type* o_ptr = inventory_item_to_object_ptr(item);
    if (!o_ptr)
        return false;

    if (!o_ptr->k_idx)
    {
        if (item >= INVEN_WIELD && item < INVEN_TOTAL)
            return item_tester_okay(o_ptr);

        return false;
    }

    /* Verify the item */
    return (item_tester_okay(o_ptr));
}

/*
 * Let the user select an item, save its "index"
 *
 * Return true only if an acceptable item was chosen by the user.
 *
 * The selected item must satisfy the "item_tester_hook()" function,
 * if that hook is set, and the "item_tester_tval", if that value is set.
 *
 * All "item_tester" restrictions are cleared before this function returns.
 *
 * The user is allowed to choose acceptable items from the equipment,
 * inventory, or floor, respectively, if the proper flag was given,
 * and there are any acceptable items in that location.
 *
 * The equipment or inventory are displayed (even if no acceptable
 * items are in that location) if the proper flag was given.
 *
 * If there are no acceptable items available anywhere, and "str" is
 * not NULL, then it will be used as the text of a warning message
 * before the function returns.
 *
 * Note that the user must press "-" to specify the item on the floor,
 * and there is no way to "examine" the item on the floor, while the
 * use of "capital" letters will "examine" an inventory/equipment item,
 * and prompt for its use.
 *
 * If a legal item is selected from the inventory, we save it in "cp"
 * directly (0 to 35), and return true.
 *
 * If a legal item is selected from the floor, we save it in "cp" as
 * a negative (-1 to -511), and return true.
 *
 * If no item is available, we do nothing to "cp", and we display a
 * warning message, using "str" if available, and return false.
 *
 * If no item is selected, we do nothing to "cp", and return false.
 *
 * Global "p_ptr->command_new" is used when viewing the inventory or equipment
 * to allow the user to enter a command while viewing those screens, and
 * also to induce "auto-enter" of stores, and other such stuff.
 *
 * Global "p_ptr->command_see" may be set before calling this function to start
 * out in "browse" mode.  It is cleared before this function returns.
 *
 * Global "p_ptr->command_wrk" is used to choose between equip/inven/floor
 * listings.  It is equal to USE_INVEN or USE_EQUIP or USE_FLOOR, except
 * when this function is first called, when it is equal to zero, which will
 * cause it to be set to USE_INVEN.
 *
 * We always erase the prompt when we are done, leaving a blank line,
 * or a warning message, if appropriate, if no items are available.
 *
 * Note that only "acceptable" floor objects get indexes, so between two
 * commands, the indexes of floor objects may change.  XXX XXX XXX
 */
bool get_item(int* cp, cptr pmt, cptr str, int mode)
{
    int py = p_ptr->py;
    int px = p_ptr->px;
    bool combined_inven_equip = ((mode & (USE_INVEN)) && (mode & (USE_EQUIP)));
    bool old_inventory_menu_include_equip =
        inventory_menu_set_include_equip(combined_inven_equip);
    bool old_inventory_choice_debug_logging = inventory_choice_debug_logging;

    char which;

    int i, j;
    int k = INVEN_WIELD; // a default value to soothe compilation warnings

    int i1, i2;
    int e1, e2;
    int f1, f2;

    bool done, item;

    bool oops = false;

    bool use_inven = ((mode & (USE_INVEN)) ? true : false);
    bool use_equip = ((mode & (USE_EQUIP)) ? true : false);
    bool use_floor = ((mode & (USE_FLOOR)) ? true : false);

    bool allow_inven = false;
    bool allow_equip = false;
    bool allow_floor = false;
    bool allow_inven_menu = false;
    bool allow_equip_menu = false;

    bool toggle = false;
    bool saved_hide_cursor = false;
    bool saved_cursor = false;

    char tmp_val[160];
    char out_val[160];

    int floor_list[MAX_FLOOR_STACK];
    int floor_num;

#ifdef ALLOW_REPEAT

    /* Get the item index */
    if (repeat_pull(cp))
    {
        /* Verify the item */
        if (get_item_okay(*cp))
        {
            /* Forget the item_tester_tval restriction */
            item_tester_tval = 0;

            /* Forget the item_tester_hook restriction */
            item_tester_hook = NULL;

            /* Success */
            inventory_choice_debug_logging = old_inventory_choice_debug_logging;
            inventory_menu_set_include_equip(old_inventory_menu_include_equip);
            return (true);
        }
        else
        {
            /* Invalid repeat - reset it */
            repeat_clear();
        }
    }

#endif /* ALLOW_REPEAT */

    (void)Term_get_cursor(&saved_cursor);
    saved_hide_cursor = hide_cursor;
    hide_cursor = true;
    (void)Term_set_cursor(false);

    // save the mode in a global variable version
    p_ptr->get_item_mode = mode;
    inventory_choice_debug_logging = item_prompt_is_replace(pmt);

    if (inventory_choice_debug_logging)
    {
        log_debug("selector[start]: prompt='%s' mode=%d use_inven=%d "
            "use_equip=%d use_floor=%d",
            pmt ? pmt : "", mode, use_inven ? 1 : 0, use_equip ? 1 : 0,
            use_floor ? 1 : 0);
    }

    /* Paranoia XXX XXX XXX */
    message_flush();

    /* Not done */
    done = false;

    /* No item selected */
    item = false;

    /* Full inventory */
    i1 = 0;
    i2 = INVEN_PACK - 1;

    /* Forbid inventory */
    if (!use_inven)
        i2 = -1;

    /* Restrict inventory indexes */
    while ((i1 <= i2) && (!get_item_okay(i1)))
        i1++;
    while ((i1 <= i2) && (!get_item_okay(i2)))
        i2--;

    /* Accept inventory */
    if ((i1 <= i2) || (use_inven && inventory_visible_supply_count() > 0))
        allow_inven = true;

    /* Full equipment */
    e1 = INVEN_WIELD;
    e2 = INVEN_TOTAL - 1;

    /* Forbid equipment */
    if (!use_equip)
        e2 = -1;

    /* Restrict equipment indexes */
    while ((e1 <= e2) && (!get_item_okay(e1)))
        e1++;
    while ((e1 <= e2) && (!get_item_okay(e2)))
        e2--;

    /* Accept equipment */
    if (e1 <= e2)
        allow_equip = true;

    /* Scan all objects in the grid */
    floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, py, px, 0x00);

    /* Full floor */
    f1 = 0;
    f2 = floor_num - 1;

    /* Forbid floor */
    if (!use_floor)
        f2 = -1;

    /* Restrict floor indexes */
    while ((f1 <= f2) && (!get_item_okay(0 - floor_list[f1])))
        f1++;
    while ((f1 <= f2) && (!get_item_okay(0 - floor_list[f2])))
        f2--;

    /* Accept floor */
    if (f1 <= f2)
        allow_floor = true;

    allow_inven_menu = allow_inven || (combined_inven_equip && allow_equip);
    allow_equip_menu = allow_equip && !combined_inven_equip;

    /* Require at least one legal choice */
    if (!allow_inven && !allow_equip && !allow_floor)
    {
        /* Cancel p_ptr->command_see */
        p_ptr->command_see = false;

        /* Oops */
        oops = true;

        /* Done */
        done = true;
    }

    /* Analyze choices */
    else
    {
        /* Preserve the currently viewed pane only if it still has legal choices. */
        if (p_ptr->command_see && (p_ptr->command_wrk == (USE_INVEN))
            && allow_inven_menu)
        {
            p_ptr->command_wrk = (USE_INVEN);
        }
        else if (!combined_inven_equip
            && p_ptr->command_see && (p_ptr->command_wrk == (USE_EQUIP))
            && allow_equip_menu)
        {
            p_ptr->command_wrk = (USE_EQUIP);
        }
        else if (p_ptr->command_see && (p_ptr->command_wrk == (USE_FLOOR))
            && allow_floor)
        {
            p_ptr->command_wrk = (USE_FLOOR);
        }

        /* Otherwise start on the first pane with legal choices. */
        else if (allow_inven_menu)
        {
            p_ptr->command_wrk = (USE_INVEN);
        }
        else if (allow_equip_menu)
        {
            p_ptr->command_wrk = (USE_EQUIP);
        }
        else if (allow_floor)
        {
            p_ptr->command_wrk = (USE_FLOOR);
        }

        /* Hack -- Use (empty) inventory */
        else
        {
            p_ptr->command_wrk = (USE_INVEN);
        }
    }

    /* Item selectors always start with the list visible. */
    p_ptr->command_see = true;

    /* Start out in "display" mode */
    if (p_ptr->command_see)
    {
        /* Save screen */
        screen_save();
    }

    /* Repeat until done */
    /* Row-based display mappings (built when list is visible) */
    int vis_inven[ENHANCED_MAX_LIST];    /* row -> inven index */
    int vis_inven_cnt = 0;
    int vis_equip[INVEN_TOTAL - INVEN_WIELD]; /* row -> equip index */
    int vis_equip_cnt = 0;
    int vis_floor[MAX_FLOOR_STACK]; /* row -> floor_list index (not object index) */
    int vis_floor_cnt = 0;

    int highlight_row = -1; /* row within current visible list */
    bool highlight_active = false;
    inventory_menu_scroll_offset = 0;

    /* Helper lambdas (C89 substitute: static inline style) defined as macros */
#define DRAW_HIGHLIGHT_STORY_VARS()                                                 \
        bool highlight_story_font = false;                                          \
        int highlight_story_w = 0;
#define DRAW_HIGHLIGHT_STORY_UPDATE()                                               \
        if (p_ptr->command_wrk == (USE_INVEN) || p_ptr->command_wrk == (USE_FLOOR)) \
            highlight_story_font = story_inventory_list_active;                     \
        else if (p_ptr->command_wrk == (USE_EQUIP))                                 \
            highlight_story_font = story_equipment_list_active;                     \
        if (highlight_story_font)                                                   \
        {                                                                           \
            int story_term_h = 0;                                                   \
            Term_get_size(&highlight_story_w, &story_term_h);                       \
        }
#define DRAW_HIGHLIGHT_IF_STORY(code)                                               \
    if (highlight_story_font) {                                                     \
        story_font_term_state highlight_story_state;                                \
        story_font_term_push(true, false, &highlight_story_state);                  \
        code;                                                                       \
        story_font_term_pop(&highlight_story_state);                                \
    } else
/* Build mapping arrays for currently selected list. */                             \
#define BUILD_VISIBLE_LIST()                                                         \
    do {                                                                            \
        vis_inven_cnt = 0;                                                          \
        vis_equip_cnt = 0;                                                          \
        vis_floor_cnt = 0;                                                          \
        if (!p_ptr->command_see)                                                    \
            highlight_active = false;                                               \
        if (p_ptr->command_wrk == (USE_INVEN)) {                                    \
            int supply_count = inventory_visible_supply_count();                    \
            for (int ord = 0; ord < supply_count && vis_inven_cnt < ENHANCED_MAX_LIST; ++ord) { \
                int item_index = inventory_visible_supply_item_at(ord);             \
                if (item_index >= SUPPLIES_INDEX)                                   \
                    vis_inven[vis_inven_cnt++] = item_index;                        \
            }                                                                       \
            for (int ii = 0; ii < INVEN_PACK && vis_inven_cnt < ENHANCED_MAX_LIST; ++ii) { \
                if (inventory[ii].k_idx && get_item_okay(ii))                       \
                    vis_inven[vis_inven_cnt++] = ii;                                \
            }                                                                       \
            if (inventory_menu_include_equip) {                                     \
                for (int ii = INVEN_WIELD; ii < INVEN_TOTAL && vis_inven_cnt < ENHANCED_MAX_LIST; ++ii) { \
                    if (get_item_okay(ii))                                          \
                        vis_inven[vis_inven_cnt++] = ii;                            \
                }                                                                   \
            }                                                                       \
            if (vis_inven_cnt <= 0) {                                               \
                highlight_row = -1;                                                 \
                if (p_ptr->command_see) highlight_active = false;                   \
            } else if (!highlight_active || highlight_row < 0 || highlight_row >= vis_inven_cnt) { \
                highlight_row = 0;                                                  \
                if (p_ptr->command_see) highlight_active = true;                    \
            }                                                                       \
        } else if (p_ptr->command_wrk == (USE_EQUIP)) {                             \
            for (int ii = INVEN_WIELD; ii < INVEN_TOTAL; ++ii) {                    \
                bool include_slot = false;                                          \
                if (inventory[ii].k_idx) {                                          \
                    include_slot = get_item_okay(ii);                               \
                } else if (throw_slot_menu_active && throw_slot_enabled[ii]) {      \
                    include_slot = true;                                            \
                }                                                                   \
                if (include_slot)                                                   \
                    vis_equip[vis_equip_cnt++] = ii;                                \
            }                                                                       \
            if (vis_equip_cnt <= 0) {                                               \
                highlight_row = -1;                                                 \
                if (p_ptr->command_see) highlight_active = false;                   \
            } else if (!highlight_active || highlight_row < 0 || highlight_row >= vis_equip_cnt) { \
                highlight_row = 0;                                                  \
                if (p_ptr->command_see) highlight_active = true;                    \
            }                                                                       \
        } else if (p_ptr->command_wrk == (USE_FLOOR)) {                             \
            for (int ii = 0; ii < floor_num; ++ii) {                                \
                int obj_idx = floor_list[ii];                                       \
                if (get_item_okay(0 - obj_idx))                                     \
                    vis_floor[vis_floor_cnt++] = ii;                                \
            }                                                                       \
            if (vis_floor_cnt <= 0) {                                               \
                highlight_row = -1;                                                 \
                if (p_ptr->command_see) highlight_active = false;                   \
            } else if (!highlight_active || highlight_row < 0 || highlight_row >= vis_floor_cnt) { \
                highlight_row = 0;                                                  \
                if (p_ptr->command_see) highlight_active = true;                    \
            }                                                                       \
        }                                                                           \
    } while (0)
#define MOVE_HIGHLIGHT(dir)                                                          \
    do {                                                                            \
        if (!highlight_active) break;                                               \
        if (p_ptr->command_wrk == (USE_INVEN) && vis_inven_cnt > 0) {                \
            highlight_row = (highlight_row + (vis_inven_cnt) + (dir)) % vis_inven_cnt;\
        } else if (p_ptr->command_wrk == (USE_EQUIP) && vis_equip_cnt > 0) {         \
            highlight_row = (highlight_row + (vis_equip_cnt) + (dir)) % vis_equip_cnt;\
        } else if (p_ptr->command_wrk == (USE_FLOOR) && vis_floor_cnt > 0) {         \
            highlight_row = (highlight_row + (vis_floor_cnt) + (dir)) % vis_floor_cnt;\
        }                                                                           \
    } while (0)
#define CURRENT_ROW_CHOICE(row_idx, out_item, out_valid)                            \
    do {                                                                            \
        int _row_idx = (row_idx);                                                    \
        (out_valid) = false;                                                        \
        if (p_ptr->command_wrk == (USE_INVEN)                                      \
            && _row_idx >= 0 && _row_idx < vis_inven_cnt) {                         \
            (out_item) = vis_inven[_row_idx];                                       \
            (out_valid) = true;                                                     \
        } else if (p_ptr->command_wrk == (USE_EQUIP)                               \
            && _row_idx >= 0 && _row_idx < vis_equip_cnt) {                         \
            (out_item) = vis_equip[_row_idx];                                       \
            (out_valid) = true;                                                     \
        } else if (p_ptr->command_wrk == (USE_FLOOR)                               \
            && _row_idx >= 0 && _row_idx < vis_floor_cnt) {                         \
            int _obj_idx = floor_list[vis_floor[_row_idx]];                         \
            (out_item) = 0 - _obj_idx;                                              \
            (out_valid) = true;                                                     \
        }                                                                           \
    } while (0)

    /* Draw highlight: re-render the line with reversed attr marker */
#define DRAW_HIGHLIGHT()                                                             \
    do {                                                                            \
        if (!highlight_active || !p_ptr->command_see) break;                        \
        byte attr = inventory_menu_selected_attr(TERM_L_BLUE);                      \
        int col = 0;                                                                \
        int term_wid = menu_term_width();                                           \
        int weight_col = menu_weight_col_for_width(term_wid);                       \
        int label_col_base = menu_label_col_for_width(term_wid, show_weights);      \
        int visible_rows = inventory_menu_visible_rows_for_height(menu_term_height());\
        int len = 29;                                                                \
        int lim = term_wid - 3;                                                     \
        char tmp[80];                                                               \
        DRAW_HIGHLIGHT_STORY_VARS()                                                 \
        DRAW_HIGHLIGHT_STORY_UPDATE()                                               \
        if (show_weights && lim > (weight_col - 1)) lim = weight_col - 1;          \
        if (p_ptr->command_wrk == (USE_EQUIP)) { lim -= (14 + 2); }                 \
        if (lim < 0) lim = 0;                                                       \
        /* Recompute layout length by scanning visible list */                     \
        if (p_ptr->command_wrk == (USE_INVEN)) {                                    \
            for (int r=0;r<vis_inven_cnt;r++){                                      \
                int entry = vis_inven[r];                                           \
                describe_inventory_menu_entry(entry, tmp, sizeof(tmp));             \
                tmp[lim]='\0';                                                     \
                int l=strlen(tmp)+5 + (show_weights?9:0);                           \
                if (l>len) len=l;                                                   \
            }                                                                       \
        } else if (p_ptr->command_wrk == (USE_EQUIP)) {                             \
            for (int r=0;r<vis_equip_cnt;r++){                                      \
                object_type* o_ptr=&inventory[vis_equip[r]];                        \
                if (o_ptr->k_idx)                                                   \
                    object_desc(tmp, sizeof(tmp), o_ptr, true, 3);                  \
                else                                                                \
                    SDL_strlcpy(tmp, describe_empty_slot(vis_equip[r]), sizeof(tmp)); \
                tmp[lim]='\0';                                                     \
                int l=strlen(tmp)+(2+3)+(12+2)+(show_weights?9:0);                  \
                if (l>len) len=l;                                                   \
            }                                                                       \
        } else if (p_ptr->command_wrk == (USE_FLOOR)) {                             \
            for (int r=0;r<vis_floor_cnt;r++){                                      \
                object_type* o_ptr=&o_list[floor_list[vis_floor[r]]];               \
                object_desc_floor(tmp, sizeof(tmp), o_ptr, true, 3);                \
                tmp[lim]='\0';                                                     \
                int l=strlen(tmp)+5 + (show_weights?9:0);                           \
                if (l>len) len=l;                                                   \
            }                                                                       \
        }                                                                           \
        col = menu_center_col_for_len(term_wid, len);                               \
        /* Determine row and item */                                                \
        int row=-1; int item_index=0; int floor_slot=-1;                            \
        if (p_ptr->command_wrk == (USE_INVEN) && highlight_row < vis_inven_cnt) {   \
            row = highlight_row - inventory_menu_scroll_offset;                     \
            if (row < 0 || row >= visible_rows) break;                              \
            item_index = vis_inven[highlight_row];                                  \
            prt("", row+1, col);                                                    \
            int label_col = label_col_base;                                         \
            if (inventory_item_is_supply_summary(item_index)) {                     \
                char label = supplies_label_char();                                 \
                int slot = supplies_virtual_slot();                                 \
                object_type supply_icon;                                            \
                object_type* display_obj = prepare_supply_icon_object(&supply_icon); \
                if (!label && slot >= 0) label = inventory_visible_label_for_item(slot); \
                if (!label) label = 'a';                                            \
                format_supply_summary(tmp, sizeof(tmp));                            \
                tmp[lim]='\0';                                                     \
                DRAW_HIGHLIGHT_IF_STORY({                                           \
                    char lab[8]; sprintf(lab, "(%c)", label);                       \
                    char wbuf[16]; cptr wptr = NULL;                                \
                    if (show_weights) {                                             \
                        int wgt = supplies_limit_weight();                          \
                        strnfmt(wbuf, sizeof(wbuf), "%2d.%1d lb", wgt / 10, wgt % 10); \
                        wptr = wbuf;                                                \
                    }                                                               \
                    story_render_inventory_entry(row + 1, col, label_col, tmp, attr, \
                        show_weights, wptr, attr, lab, attr, display_obj, true, highlight_story_w); \
                })                                                                  \
                {                                                                   \
                    int text_col = col;                                             \
                    inventory_menu_fill_selected_span(col, label_col + 4, row+1, attr); \
                    if (display_obj && display_obj->k_idx)                          \
                        text_col = draw_item_tile_with_background(col, row+1,       \
                            display_obj, attr);                                     \
                    c_put_str(attr,tmp,row+1,text_col);                             \
                    if (show_weights){ int wgt = supplies_limit_weight(); char w[16]; strnfmt(w, sizeof(w), "%2d.%1d lb", wgt / 10, wgt % 10); c_put_str(attr,w,row+1,weight_col);} \
                    { char lab[8]; sprintf(lab, " (%c)", label); c_put_str(attr,lab,row+1,label_col); }\
                }                                                                   \
            } else {                                                                \
                object_type* o_ptr = inventory_item_to_object_ptr(item_index);      \
                if (!o_ptr || (!o_ptr->k_idx && !inventory_item_is_equipment(item_index))) break; \
                describe_inventory_menu_entry(item_index, tmp, sizeof(tmp));        \
                tmp[lim]='\0';                                                      \
                DRAW_HIGHLIGHT_IF_STORY({                                           \
                    char lab[8]; sprintf(lab, "(%c)", inventory_visible_label_for_item(item_index));  \
                    char wbuf[16]; cptr wptr = NULL;                                \
                    if (show_weights){ int wgt= o_ptr->weight*o_ptr->number; strnfmt(wbuf, sizeof(wbuf), "%2d.%1d lb", wgt / 10, wgt % 10); wptr = wbuf; } \
                    story_render_inventory_entry(row + 1, col, label_col, tmp, attr, \
                        show_weights, wptr, attr, lab, attr, (o_ptr && o_ptr->k_idx) ? o_ptr : NULL, true, highlight_story_w); \
                })                                                                  \
                {                                                                   \
                    int text_col = col;                                              \
                    inventory_menu_fill_selected_span(col, label_col + 4, row+1, attr); \
                    if (o_ptr && o_ptr->k_idx)                                       \
                        text_col = draw_item_tile_with_background(col, row+1,       \
                            o_ptr, attr);                                           \
                    c_put_str(attr,tmp,row+1,text_col);                             \
                    if (show_weights){ int wgt= o_ptr->weight*o_ptr->number; char w[16]; strnfmt(w, sizeof(w), "%2d.%1d lb", wgt / 10, wgt % 10); c_put_str(attr,w,row+1,weight_col);} \
                    { char lab[8]; sprintf(lab, " (%c)", inventory_visible_label_for_item(item_index)); c_put_str(attr,lab,row+1,label_col); }\
                }                                                                   \
            }                                                                       \
        } else if (p_ptr->command_wrk == (USE_EQUIP) && highlight_row < vis_equip_cnt){\
            row = highlight_row; item_index = vis_equip[highlight_row];             \
            object_type* o_ptr=&inventory[item_index];                              \
            if (o_ptr->k_idx)                                                       \
                object_desc(tmp,sizeof(tmp),o_ptr,true,3);                          \
            else                                                                    \
                SDL_strlcpy(tmp, describe_empty_slot(item_index), sizeof(tmp));     \
            tmp[lim]='\0';                                                          \
            Term_erase(menu_overlay_clear_col(col), row+1, 255);                    \
            { char usebuf[32]; strnfmt(usebuf,sizeof(usebuf),"%-12s: ", mention_use(item_index)); \
              DRAW_HIGHLIGHT_IF_STORY({                                             \
                  char lab[8]; sprintf(lab, "(%c)", index_to_label(item_index));    \
                  char wbuf[16]; cptr wptr = NULL;                                  \
                  if (show_weights && o_ptr->weight) {                              \
                      int wgt=o_ptr->weight*o_ptr->number;                          \
                      sprintf(wbuf,"%2d.%1d lb",wgt/10,wgt%10);                     \
                      wptr = wbuf;                                                  \
                  }                                                                 \
                  story_render_equipment_entry(row + 1, col, item_index, usebuf, attr, tmp, attr, \
                      show_weights, wptr, attr, lab, attr, o_ptr->k_idx ? o_ptr : NULL, true, highlight_story_w); \
              })                                                                    \
              {                                                                     \
                  int text_col = col + 12 + 2;                                      \
                  int label_col = label_col_base;                                   \
                  inventory_menu_fill_selected_span(col, label_col + 4, row+1, attr); \
                  if (o_ptr->k_idx)                                                 \
                      text_col = draw_item_tile_with_background(col+12+2, row+1,    \
                          o_ptr, attr);                                             \
                  c_put_str(attr,usebuf,row+1,col);                                 \
                  c_put_str(attr,tmp,row+1,text_col);                               \
                  if (show_weights && o_ptr->weight){ int wgt=o_ptr->weight*o_ptr->number; char w[16]; sprintf(w,"%2d.%1d lb",wgt/10,wgt%10); c_put_str(attr,w,row+1,weight_col);} \
                  { char lab[8]; sprintf(lab, " (%c)", index_to_label(item_index)); c_put_str(attr,lab,row+1,label_col); }\
              }                                                                     \
            }                                                                       \
        } else if (p_ptr->command_wrk == (USE_FLOOR) && highlight_row < vis_floor_cnt){\
            row = highlight_row; floor_slot = vis_floor[highlight_row];             \
            int obj_idx = floor_list[floor_slot];                                   \
            object_type* o_ptr=&o_list[obj_idx];                                    \
            object_desc_floor(tmp,sizeof(tmp),o_ptr,true,3); tmp[lim]='\0';         \
            prt("", row+1, col);                                                    \
            int label_col = label_col_base;                                         \
            DRAW_HIGHLIGHT_IF_STORY({                                               \
                char lab[8]; sprintf(lab, "(%c)", index_to_label(floor_slot));      \
                char wbuf[16]; cptr wptr = NULL;                                    \
                if (show_weights){ int wgt=o_ptr->weight*o_ptr->number; strnfmt(wbuf, sizeof(wbuf), "%2d.%1d lb", wgt / 10, wgt % 10); wptr = wbuf; } \
                story_render_inventory_entry(row + 1, col, label_col, tmp, attr,    \
                    show_weights, wptr, attr, lab, attr, o_ptr, true, highlight_story_w); \
            })                                                                      \
            {                                                                       \
                int text_col = col;                                                  \
                inventory_menu_fill_selected_span(col, label_col + 4, row+1, attr); \
                text_col = draw_item_tile_with_background(col, row+1, o_ptr, attr); \
                c_put_str(attr,tmp,row+1,text_col);                                 \
                if (show_weights){ int wgt=o_ptr->weight*o_ptr->number; char w[16]; strnfmt(w, sizeof(w), "%2d.%1d lb", wgt / 10, wgt % 10); c_put_str(attr,w,row+1,weight_col);} \
                { char lab[8]; sprintf(lab, " (%c)", index_to_label(floor_slot)); c_put_str(attr,lab,row+1,label_col); }\
            }                                                                       \
        }                                                                           \
    } while (0)

    while (!done)
    {
        (void)Term_set_extra_cursor(false, 0, 0, false);

        int ni = 0;
        int ne = 0;

        /* Scan windows */
        for (j = 0; j < ANGBAND_TERM_MAX; j++)
        {
            /* Unused */
            if (!angband_term[j])
                continue;

            /* Count windows displaying inven */
            if (op_ptr->window_flag[j] & (PW_INVEN))
                ni++;

            /* Count windows displaying equip */
            if (op_ptr->window_flag[j] & (PW_EQUIP))
                ne++;
        }

        /* Toggle if needed */
        if (((p_ptr->command_wrk == (USE_EQUIP)) && ni && !ne)
            || ((p_ptr->command_wrk == (USE_INVEN)) && !ni && ne))
        {
            /* Toggle */
            toggle_inven_equip();

            /* Track toggles */
            toggle = !toggle;
        }

        /* Update */
        p_ptr->window |= (PW_INVEN | PW_EQUIP);

        /* Redraw windows */
        window_stuff();

    /* Build visible list and ensure initial highlight */
    BUILD_VISIBLE_LIST();
    if (p_ptr->command_wrk == (USE_INVEN))
    {
        int display_rows = inventory_menu_visible_rows_for_height(menu_term_height());
        int max_scroll_offset = MAX(0, vis_inven_cnt - display_rows);

        if (sdl_touch_only_device_active())
        {
            /* Touch-only inventory is viewport-driven: dragging pans the
             * list, and tapping selects.  Keep the finger-chosen offset. */
            (void)ui_scroll_area_take_touch_scrolled();
            if (inventory_menu_scroll_offset > max_scroll_offset)
                inventory_menu_scroll_offset = max_scroll_offset;
            if (inventory_menu_scroll_offset < 0)
                inventory_menu_scroll_offset = 0;
        }
        else
        {
            inventory_menu_scroll_offset = inventory_menu_scroll_to_selection(
                inventory_menu_scroll_offset, highlight_row, vis_inven_cnt,
                display_rows, 0);
        }
    }
    else
    {
        inventory_menu_scroll_offset = 0;
    }

    if (p_ptr->command_wrk == (USE_INVEN))
    {
        log_inventory_selector_state("visible-list", pmt, vis_inven, vis_inven_cnt);
    }

    /* Viewing inventory */
    if (p_ptr->command_wrk == (USE_INVEN))
    {
            /* Redraw if needed */
            if (p_ptr->command_see)
                show_inven();

            /* Begin the prompt */
            sprintf(out_val, combined_inven_equip ? "Items:" : "Inven:");

            /* List choices */
            if (vis_inven_cnt > 0)
            {
                /* Build the prompt */
                sprintf(tmp_val, " %c-%c,",
                    inventory_visible_label_for_item(vis_inven[0]),
                    inventory_visible_label_for_item(vis_inven[vis_inven_cnt - 1]));

                /* Append */
                SDL_strlcat(out_val, tmp_val, sizeof(out_val));
            }

            /* Indicate ability to "view" */
            if (!p_ptr->command_see)
                SDL_strlcat(out_val, " * to see,", sizeof(out_val));

            /* Indicate legality of "toggle" */
            if (!combined_inven_equip && allow_equip_menu)
                SDL_strlcat(out_val, " / for Equip,", sizeof(out_val));

            /* Indicate legality of the "floor" */
            if (allow_floor)
                SDL_strlcat(out_val, " - for floor,", sizeof(out_val));
    }

    /* Viewing equipment */
    else if (p_ptr->command_wrk == (USE_EQUIP))
    {
            /* Redraw if needed */
            if (p_ptr->command_see)
            {
                log_trace("get_item: command_see is true, calling show_equip()");
                show_equip();
            }

            /* Begin the prompt */
            sprintf(out_val, "Equip:");

            /* List choices */
            if (e1 <= e2)
            {
                /* Build the prompt */
                sprintf(
                    tmp_val, " %c-%c,", index_to_label(e1), index_to_label(e2));

                /* Append */
                SDL_strlcat(out_val, tmp_val, sizeof(out_val));
            }

            /* Indicate ability to "view" */
            if (!p_ptr->command_see)
                SDL_strlcat(out_val, " * to see,", sizeof(out_val));

            /* Indicate legality of "toggle" */
            if (allow_inven_menu)
                SDL_strlcat(out_val, " / for Inven,", sizeof(out_val));

            /* Indicate legality of the "floor" */
            if (allow_floor)
                SDL_strlcat(out_val, " - for floor,", sizeof(out_val));
    }

    /* Viewing floor */
    else
    {
            /* Redraw if needed */
            if (p_ptr->command_see)
                show_floor(floor_list, floor_num);

            /* Begin the prompt */
            sprintf(out_val, "Floor:");

            /* List choices */
            if (f1 <= f2)
            {
                /* Build the prompt */
                sprintf(tmp_val, " %c-%c,", I2A(f1), I2A(f2));

                /* Append */
                SDL_strlcat(out_val, tmp_val, sizeof(out_val));
            }

            /* Indicate ability to "view" */
            if (!p_ptr->command_see)
                SDL_strlcat(out_val, " * to see,", sizeof(out_val));

            /* Append */
            if (allow_inven_menu)
                SDL_strlcat(out_val,
                    combined_inven_equip ? " / for Items," : " / for Inven,",
                    sizeof(out_val));

            /* Append */
            else if (allow_equip_menu)
                SDL_strlcat(out_val, " / for Equip,", sizeof(out_val));
        }

        /* Finish the prompt */
        SDL_strlcat(out_val, " ESC", sizeof(out_val));

        /* Touch-only users tap rows (and switch via the on-screen panes)
         * rather than pressing letter/symbol keys, so show a tap-oriented
         * prompt instead of keyboard shortcuts. */
        if (sdl_touch_only_device_active())
        {
            cptr touch_label =
                (p_ptr->command_wrk == (USE_EQUIP)) ? "a piece of equipment"
              : (p_ptr->command_wrk == (USE_FLOOR)) ? "a floor item"
              : "an item";

            strnfmt(tmp_val, sizeof(tmp_val),
                "(Tap %s, tap away to cancel) %s", touch_label, pmt);
        }
        else
        {
            /* Build the prompt */
            strnfmt(tmp_val, sizeof(tmp_val), "(%s) %s", out_val, pmt);
        }

        if (inventory_choice_debug_logging && p_ptr->command_wrk == (USE_INVEN))
        {
            log_debug("selector[prompt]: out_val='%s' full_prompt='%s' "
                "highlight_row=%d highlight_active=%d",
                out_val, tmp_val, highlight_row, highlight_active ? 1 : 0);
    }

        /* Show the prompt */
        /* Use story font for prompt if the current list has story font enabled */
        if ((p_ptr->command_wrk == (USE_INVEN) || p_ptr->command_wrk == (USE_FLOOR)) && story_inventory_list_active)
        {
            story_font_term_state prompt_story_state;
            story_font_term_push(true, false, &prompt_story_state);
            prt(tmp_val, 0, 0);
            story_font_term_pop(&prompt_story_state);
        }
        else if (p_ptr->command_wrk == (USE_EQUIP) && story_equipment_list_active)
        {
            story_font_term_state prompt_story_state;
            story_font_term_push(true, false, &prompt_story_state);
            prt(tmp_val, 0, 0);
            story_font_term_pop(&prompt_story_state);
        }
        else
            prt(tmp_val, 0, 0);

    /* Draw current highlight overlay if any */
    DRAW_HIGHLIGHT();

        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_touch_category(
            SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
        /* Touch-only users have no Esc key, so let a tap outside the list
         * cancel the selection. */
        if (sdl_touch_only_device_active())
            ui_menu_click_set_outside_cancel_enabled(true);
        ui_scroll_area_clear();

        if (p_ptr->command_see)
        {
            int click_rows = 0;
            int display_rows = inventory_menu_visible_rows_for_height(
                menu_term_height());

            if (p_ptr->command_wrk == (USE_INVEN))
            {
                int first_row = inventory_menu_scroll_offset;
                click_rows = vis_inven_cnt - first_row;
                if (click_rows > display_rows)
                    click_rows = display_rows;
                if (click_rows < 0)
                    click_rows = 0;
                for (int click_row = 0; click_row < click_rows; click_row++)
                    ui_menu_click_add_full_row(first_row + click_row,
                        click_row + 1);
            }
            else if (p_ptr->command_wrk == (USE_EQUIP))
            {
                click_rows = vis_equip_cnt;
                if (click_rows > display_rows)
                    click_rows = display_rows;
                for (int click_row = 0; click_row < click_rows; click_row++)
                    ui_menu_click_add_full_row(click_row, click_row + 1);
            }
            else if (p_ptr->command_wrk == (USE_FLOOR))
            {
                click_rows = vis_floor_cnt;
                if (click_rows > display_rows)
                    click_rows = display_rows;
                for (int click_row = 0; click_row < click_rows; click_row++)
                    ui_menu_click_add_full_row(click_row, click_row + 1);
            }

            if (click_rows > 0)
            {
                ui_scroll_area_begin(1, click_rows,
                    SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
                ui_scroll_area_set_keys('8', '2', '6', '4');
                if (p_ptr->command_wrk == (USE_INVEN)
                    && sdl_touch_only_device_active())
                {
                    /* Touch-only: drag pans the inventory list without moving
                     * the selection (tap a row to pick it). */
                    ui_scroll_area_set_offset_target(
                        &inventory_menu_scroll_offset,
                        MAX(0, vis_inven_cnt - display_rows));
                }
            }
        }

        /* Get a key */
        which = inkey();

        {
            int clicked_row = -1;
            int click_action = UI_MENU_CLICK_PRIMARY;
            bool click_taken =
                ui_menu_click_take_action(&clicked_row, &click_action);

            if (click_taken)
            {
                int clicked_item = 0;
                bool have_selection = false;

                CURRENT_ROW_CHOICE(clicked_row, clicked_item, have_selection);

                if (have_selection)
                {
                    object_type* clicked_obj = inventory_item_to_object_ptr(
                        clicked_item);
                    bool legal_channel =
                        (inventory_item_uses_inven_channel(clicked_item)
                            && allow_inven)
                        || (clicked_item >= INVEN_WIELD
                            && clicked_item < INVEN_TOTAL && allow_equip)
                        || (clicked_item < 0 && allow_floor);

                    highlight_row = clicked_row;
                    highlight_active = true;

                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;

                    if (click_action == UI_MENU_CLICK_SECONDARY
                        && !inventory_item_is_supply_summary(clicked_item)
                        && clicked_obj && clicked_obj->k_idx)
                    {
                        describe_item_with_comparisons(clicked_item, true);
                        continue;
                    }

                    if (!legal_channel || !get_item_okay(clicked_item))
                    {
                        bell("Illegal object choice (click)!");
                        continue;
                    }

                    if (!get_item_allow(clicked_item))
                    {
                        done = true;
                        continue;
                    }

                    (*cp) = clicked_item;
                    item = true;
                    done = true;
                    continue;
                }

                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
            }

            if (!click_taken && which == UI_MENU_CLICK_WAKE_KEY)
                continue;
        }

        /* Parse it */
        switch (which)
        {
        case ESCAPE:
        {
            done = true;
            break;
        }

        case '*':
        case '?':
        case ' ':
        {
            bool handled_space = false;

            if (which == ' ' && p_ptr->command_see && highlight_active)
            {
                int k = 0;
                bool have_selection = false;

                if (p_ptr->command_wrk == (USE_INVEN) && highlight_row >= 0 && highlight_row < vis_inven_cnt)
                {
                    k = vis_inven[highlight_row];
                    have_selection = true;
                }
                else if (p_ptr->command_wrk == (USE_EQUIP) && highlight_row >= 0 && highlight_row < vis_equip_cnt)
                {
                    k = vis_equip[highlight_row];
                    have_selection = true;
                }
                else if (p_ptr->command_wrk == (USE_FLOOR) && highlight_row >= 0 && highlight_row < vis_floor_cnt)
                {
                    int obj_idx = floor_list[vis_floor[highlight_row]];
                    k = 0 - obj_idx;
                    have_selection = true;
                }

                if (have_selection)
                {
                    if ((inventory_item_uses_inven_channel(k) && !allow_inven) ||
                        (k >= INVEN_WIELD && k < INVEN_TOTAL && !allow_equip) ||
                        (k < 0 && !allow_floor) ||
                        !get_item_okay(k) ||
                        !get_item_allow(k))
                    {
                        have_selection = false;
                    }
                }

                if (have_selection)
                {
                    (*cp) = k;
                    item = true;
                    done = true;
                    handled_space = true;
                }
            }

            if (handled_space)
                break;

            /* Hide the list */
            if (p_ptr->command_see)
            {
                /* Flip flag */
                p_ptr->command_see = false;

                (void)Term_set_extra_cursor(false, 0, 0, false);

                /* Load screen */
                screen_load();
            }

            /* Show the list */
            else
            {
                /* Save screen */
                screen_save();

                /* Flip flag */
                p_ptr->command_see = true;
            }

            break;
        }

        case '/':
        {
            if (combined_inven_equip)
            {
                if (allow_floor)
                {
                    p_ptr->command_wrk = (p_ptr->command_wrk == (USE_FLOOR))
                        ? (USE_INVEN)
                        : (USE_FLOOR);
                }
                else
                {
                    bell("Cannot switch item selector!");
                    break;
                }
            }
            /* Toggle to inventory */
            else if (allow_inven_menu && (p_ptr->command_wrk != (USE_INVEN)))
            {
                p_ptr->command_wrk = (USE_INVEN);
            }

            /* Toggle to equipment */
            else if (allow_equip_menu && (p_ptr->command_wrk != (USE_EQUIP)))
            {
                p_ptr->command_wrk = (USE_EQUIP);
            }
            else if (allow_floor && (p_ptr->command_wrk != (USE_FLOOR)))
            {
                p_ptr->command_wrk = (USE_FLOOR);
            }

            /* No toggle allowed */
            else
            {
                bell("Cannot switch item selector!");
                break;
            }

            /* Hack -- Fix screen */
            if (p_ptr->command_see)
            {
                (void)Term_set_extra_cursor(false, 0, 0, false);

                /* Load screen */
                screen_load();

                /* Save screen */
                screen_save();
            }

            /* Need to redraw */
            break;
        }

        case '-':
        {
            /* Paranoia */
            if (!allow_floor)
            {
                bell("Cannot select floor!");
                break;
            }

            /* Check each legal object */
            for (i = 0; i < floor_num; ++i)
            {
                /* Special index */
                k = 0 - floor_list[i];

                /* Skip non-okay objects */
                if (!get_item_okay(k))
                    continue;

                /* Allow player to "refuse" certain actions */
                if (!get_item_allow(k))
                    continue;

                /* Accept that choice */
                (*cp) = k;
                item = true;
                done = true;
                break;
            }

            break;
        }

        case 'x':
        case 'X':
#ifdef ARROW_RIGHT
        case ARROW_RIGHT:
#endif
        {
            if (p_ptr->command_see && highlight_active)
            {
                int examine_index = 0;
                bool have_selection = false;

                if (p_ptr->command_wrk == (USE_INVEN) && highlight_row >= 0 && highlight_row < vis_inven_cnt)
                {
                    examine_index = vis_inven[highlight_row];
                    have_selection = true;
                }
                else if (p_ptr->command_wrk == (USE_EQUIP) && highlight_row >= 0 && highlight_row < vis_equip_cnt)
                {
                    examine_index = vis_equip[highlight_row];
                    have_selection = true;
                }
                else if (p_ptr->command_wrk == (USE_FLOOR) && highlight_row >= 0 && highlight_row < vis_floor_cnt)
                {
                    int obj_idx = floor_list[vis_floor[highlight_row]];
                    examine_index = 0 - obj_idx;
                    have_selection = true;
                }

                if (have_selection)
                {
                    describe_item_with_comparisons(examine_index, true);
                }
                else
                {
                    bell("Nothing is selected to examine.");
                }
            }
            else
            {
                bell("No highlighted item to examine.");
            }

            break;
        }

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        {
            if (p_ptr->command_see && highlight_active
                && (which == '2' || which == '8' || which == '6'))
            {
                /* Numpad navigation mode like main menu */
                if (which == '8')
                {
                    MOVE_HIGHLIGHT(-1);
                    break; /* continue loop */
                }
                if (which == '2')
                {
                    MOVE_HIGHLIGHT(+1);
                    break;
                }
                if (which == '6')
                {
                    /* map row to actual item */
                    if (p_ptr->command_wrk == (USE_INVEN) && highlight_row < vis_inven_cnt) {
                        k = vis_inven[highlight_row];
                    } else if (p_ptr->command_wrk == (USE_EQUIP) && highlight_row < vis_equip_cnt) {
                        k = vis_equip[highlight_row];
                    } else if (p_ptr->command_wrk == (USE_FLOOR) && highlight_row < vis_floor_cnt) {
                        int obj_idx = floor_list[vis_floor[highlight_row]]; k = 0 - obj_idx; }
                    else { break; }
                }
            }
            else
            {
                bell("Illegal object choice!");
                break;
            }

            /* Hack -- Validate the item */
            if (inventory_item_uses_inven_channel(k) ? !allow_inven
                : !allow_equip)
            {
                bell("Illegal object choice!");
                break;
            }

            /* Validate the item */
            if (!get_item_okay(k))
            {
                bell("Illegal object choice!");
                break;
            }

            /* Allow player to "refuse" certain actions */
            if (!get_item_allow(k))
            {
                done = true;
                break;
            }

            /* Accept that choice */
            (*cp) = k;
            item = true;
            done = true;
            break;
        }

        case '[':
        case ']':
        {
            bool item_found = false;

            /* Convert letter to inventory index */
            if (p_ptr->command_wrk == (USE_INVEN))
            {
                if (vis_inven_cnt > 0)
                {
                    k = (which == '[') ? vis_inven[0]
                        : vis_inven[vis_inven_cnt - 1];
                    item_found = true;
                }
            }

            /* Convert letter to equipment index */
            else if (p_ptr->command_wrk == (USE_EQUIP))
            {
                if (vis_equip_cnt > 0)
                {
                    k = (which == '[') ? vis_equip[0]
                        : vis_equip[vis_equip_cnt - 1];
                    item_found = true;
                }
            }

            /* Hack -- Validate the item */
            if (inventory_item_uses_inven_channel(k) ? !allow_inven
                : !allow_equip)
            {
                bell("Illegal object choice!");
                break;
            }

            /* Validate the item */
            if (!item_found)
            {
                bell("No valid items found.");
                break;
            }

            /* Allow player to "refuse" certain actions */
            if (!get_item_allow(k))
            {
                done = true;
                break;
            }

            /* Accept that choice */
            (*cp) = k;
            item = true;
            done = true;
            break;
        }

        case '\n':
        case '\r':
        {
            /* If we have an active highlight, use it like main menu selection */
            if (highlight_active) {
                if (p_ptr->command_wrk == (USE_INVEN) && highlight_row < vis_inven_cnt) {
                    k = vis_inven[highlight_row];
                } else if (p_ptr->command_wrk == (USE_EQUIP) && highlight_row < vis_equip_cnt) {
                    k = vis_equip[highlight_row];
                } else if (p_ptr->command_wrk == (USE_FLOOR) && highlight_row < vis_floor_cnt) {
                    int obj_idx = floor_list[vis_floor[highlight_row]]; k = 0 - obj_idx; }
                else { break; }
                if (!get_item_okay(k)) { bell("Illegal object choice (highlight)!"); break; }
                if (!get_item_allow(k)) { done = true; break; }
                (*cp)=k; item=true; done=true; break; }

            /* Choose "default" inventory item */
            if (p_ptr->command_wrk == (USE_INVEN))
            {
                if (vis_inven_cnt != 1)
                {
                    bell("Illegal object choice (default)!");
                    break;
                }

                k = vis_inven[0];
            }

            /* Choose "default" equipment item */
            else if (p_ptr->command_wrk == (USE_EQUIP))
            {
                if (e1 != e2)
                {
                    bell("Illegal object choice (default)!");
                    break;
                }

                k = e1;
            }

            /* Choose "default" floor item */
            else
            {
                if (f1 != f2)
                {
                    bell("Illegal object choice (default)!");
                    break;
                }

                k = 0 - floor_list[f1];
            }

            /* Validate the item */
            if (!get_item_okay(k))
            {
                bell("Illegal object choice (default)!");
                break;
            }

            /* Allow player to "refuse" certain actions */
            if (!get_item_allow(k))
            {
                done = true;
                break;
            }

            /* Accept that choice */
            (*cp) = k;
            item = true;
            done = true;
            break;
        }

        default:
        {
            bool verify;

            /* Allow numpad navigation keys here too if list visible */
            if (p_ptr->command_see && highlight_active) {
                if (which == '8') { MOVE_HIGHLIGHT(-1); break; }
                if (which == '2') { MOVE_HIGHLIGHT(+1); break; }
                if (which == '6') { /* select */
                    if (p_ptr->command_wrk == (USE_INVEN) && highlight_row < vis_inven_cnt) 
                        k = vis_inven[highlight_row];
                    else if (p_ptr->command_wrk == (USE_EQUIP) && highlight_row < vis_equip_cnt) 
                        k = vis_equip[highlight_row];
                    else if (p_ptr->command_wrk == (USE_FLOOR) && highlight_row < vis_floor_cnt) { 
                        int obj_idx = floor_list[vis_floor[highlight_row]]; 
                        k = 0 - obj_idx; 
                    }
                    else break; 
                    
                    if (!get_item_okay(k)) { 
                        bell("Illegal object choice (highlight)!"); 
                        break; 
                    } 
                    if (!get_item_allow(k)) { 
                        done=true; 
                        break; 
                    } 
                    (*cp)=k; 
                    item=true; 
                    done=true; 
                    break; 
                }
            }

            if (steamdeck_controls_active() && isalpha((unsigned char)which))
            {
                bell("Use D-pad and confirm to select items in this mode.");
                break;
            }

            /* Note verify */
            verify = (isupper((unsigned char)which) ? true : false);

            /* Lowercase */
            which = tolower((unsigned char)which);

            /* Convert letter to inventory index */
            if (p_ptr->command_wrk == (USE_INVEN))
            {
                k = label_to_inven(which);

                if (k < 0)
                {
                    bell("Illegal object choice (inven)!");
                    break;
                }
            }

            /* Convert letter to equipment index */
            else if (p_ptr->command_wrk == (USE_EQUIP))
            {
                k = label_to_equip(which);

                if (k < 0)
                {
                    bell("Illegal object choice (equip)!");
                    break;
                }
            }

            /* Convert letter to floor index */
            else
            {
                k = (islower((unsigned char)which) ? A2I(which) : -1);

                if (k < 0 || k >= floor_num)
                {
                    bell("Illegal object choice (floor)!");
                    break;
                }

                /* Special index */
                k = 0 - floor_list[k];
            }

            /* Validate the item */
            if (!get_item_okay(k))
            {
                bell("Illegal object choice (normal)!");
                break;
            }

            /* Verify the item */
            if (verify && !verify_item("Try", k))
            {
                done = true;
                break;
            }

            /* Allow player to "refuse" certain actions */
            if (!get_item_allow(k))
            {
                done = true;
                break;
            }

            /* Accept that choice */
            (*cp) = k;
            item = true;
            done = true;
            break;
        }
        }
    }

#undef BUILD_VISIBLE_LIST
#undef MOVE_HIGHLIGHT
#undef CURRENT_ROW_CHOICE
#undef DRAW_HIGHLIGHT
#undef DRAW_HIGHLIGHT_STORY_VARS
#undef DRAW_HIGHLIGHT_STORY_UPDATE
#undef DRAW_HIGHLIGHT_IF_STORY

    (void)Term_set_extra_cursor(false, 0, 0, false);
    ui_menu_click_clear();
    ui_scroll_area_clear();

    /* Fix the screen if necessary */
    if (p_ptr->command_see)
    {
        /* Load screen */
        screen_load();

        /* Hack -- Cancel "display" */
        p_ptr->command_see = false;
    }

    story_inventory_list_active = false;
    story_equipment_list_active = false;
    inventory_menu_scroll_offset = 0;

    // Forget whether inventory or equipment was being examined
    p_ptr->command_wrk = 0;

    // Forget whether inventory or equipment or floor or combinations were
    // examinable
    p_ptr->get_item_mode = 0;

    /* Forget the item_tester_tval restriction */
    item_tester_tval = 0;

    /* Forget the item_tester_hook restriction */
    item_tester_hook = NULL;

    /* Clean up */
    /* Toggle again if needed */
    if (toggle)
        toggle_inven_equip();

    /* Update */
    p_ptr->window |= (PW_INVEN | PW_EQUIP);

    /* Window stuff */
    window_stuff();

    /* Clear the prompt line */
    prt("", 0, 0);

    /* Warning if needed */
    if (oops && str)
        msg_print(str);

#ifdef ALLOW_REPEAT

    /* Save item if available */
    if (item)
        repeat_push(*cp);

#endif /* ALLOW_REPEAT */

    inventory_menu_set_include_equip(old_inventory_menu_include_equip);
    inventory_choice_debug_logging = old_inventory_choice_debug_logging;
    hide_cursor = saved_hide_cursor;
    (void)Term_set_cursor(saved_cursor);

    /* Result */
    return (item);
}

/* Global variables for menu switching */
int enhanced_menu_action = ENHANCED_ACTION_NONE;
int enhanced_inventory_selected_item = ENHANCED_MENU_NO_SELECTION;

/* Global variables for command-specific menu cycling */
char current_menu_command = 0;     /* 'u', 'x', etc. - which command opened the menu */
int current_menu_state = 0;        /* 0=inventory, 1=equipment */

