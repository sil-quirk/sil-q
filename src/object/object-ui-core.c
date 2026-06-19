/* File: object/object-ui-core.c */

#include "angband.h"
#include "externs.h"
#include "object/object-ui-display.h"
#include "object/object-ui-select.h"
#include "object/object-internal.h"
#include "log/log.h"
#include "sdl-config.h"
#include "supplies.h"
#include "item_set.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>


#include <ctype.h>
#include <stdlib.h>
#define ENHANCED_MAX_LIST 80
#include <stddef.h>
bool inventory_menu_include_equip = false;
bool inventory_menu_expand_supplies = false;
bool inventory_choice_debug_logging = false;
int inventory_menu_scroll_offset = 0;

bool story_inventory_list_active = false;
bool story_equipment_list_active = false;

bool supplies_visible_for_current_filter(void);
bool inventory_menu_uses_visible_labels(void);
bool inventory_menu_uses_expanded_supplies(void);
int inventory_visible_supply_count(void);
int inventory_visible_supply_item_at(int ordinal);
int inventory_visible_supply_ordinal(int item);
int inventory_visible_inven_item_at(int ordinal);
int inventory_visible_inven_ordinal(int item);
char inventory_visible_label_for_item(int item);
bool inventory_item_is_supply_summary(int item);
bool inventory_item_is_supply_entry(int item);
bool inventory_item_is_equipment(int item);
object_type* inventory_item_to_object_ptr(int item);
bool inventory_item_uses_inven_channel(int item);
void describe_inventory_menu_entry(int item, char* buf, size_t len);
bool get_item_okay(int item);
bool item_prompt_is_replace(cptr pmt);
void log_inventory_selector_state(cptr stage, cptr pmt,
    const int vis_inven[], int vis_inven_cnt);
void format_supply_summary(char* buf, size_t len);
void equipment_weight_layout_rows(int first_row, int item_count,
    int term_hgt, int* divider_row, int* text_row);

bool inventory_menu_set_expand_supplies(bool enabled)
{
    bool old = inventory_menu_expand_supplies;
    inventory_menu_expand_supplies = enabled;
    return old;
}

void inventory_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

bool inventory_menu_same_button_cycle_enabled(void)
{
    if (!portable_controls_active())
        return false;

    if (!steamdeck_controls_active())
        return false;

    return get_sdl_steamdeck_inv_equip_same_button_cycle();
}

void story_print_equipment_prefix(int row, int col, byte attr, cptr prefix)
{
    const int prefix_core_width = 12;
    char label_buf[32];

    if (!prefix) prefix = "";

    const char* colon = strchr(prefix, ':');
    size_t len = colon ? (size_t)(colon - prefix) : strlen(prefix);
    if (len >= sizeof(label_buf))
        len = sizeof(label_buf) - 1;

    memcpy(label_buf, prefix, len);
    label_buf[len] = '\0';

    while (len > 0 && isspace((unsigned char)label_buf[len - 1]))
        label_buf[--len] = '\0';

    story_print_text(row, col, prefix_core_width, attr, label_buf);
    story_print_text_grid(row, col + prefix_core_width, 2, attr, ": ");
}

static void story_prepare_equipment_desc(char* dest, size_t dest_size, cptr src,
    int slot, bool has_object, int max_cols)
{
    if (!dest || dest_size == 0)
        return;

    if (!src)
        src = "";

    SDL_strlcpy(dest, src, dest_size);

    if (slot == INVEN_QUIVER2 && !has_object)
    {
        char base[160];
        SDL_strlcpy(base, dest, sizeof(base));
        if (base[0])
            strnfmt(dest, dest_size, "%s (keeps passive bonuses)", base);
        else
            SDL_strlcpy(dest, "(keeps passive bonuses)", dest_size);
    }

    if (max_cols > 0 && sdl_is_story_font_enabled())
    {
        int cell_width = sdl_get_cell_width();
        int max_pixels = max_cols * cell_width;
        size_t len = strlen(dest);

        while (len > 0 && sdl_story_font_text_width(dest, (int)len) > max_pixels)
        {
            dest[--len] = '\0';
            while (len > 0 && isspace((unsigned char)dest[len - 1]))
            dest[--len] = '\0';
        }
    }
}

static bool menu_prompt_fits_one_row(cptr prompt, int term_wid,
    bool use_story_font)
{
    size_t len;

    if (!prompt)
        return true;

    if (term_wid <= 1)
        return false;

    len = strlen(prompt);

    if (len >= (size_t)term_wid)
        return false;

    if (use_story_font && sdl_is_story_font_enabled())
    {
        int cell_width = sdl_get_cell_width();
        int max_pixels;

        if (cell_width <= 0)
            return true;

        max_pixels = (term_wid - 1) * cell_width;
        return sdl_story_font_text_width(prompt, (int)len) <= max_pixels;
    }

    return true;
}

bool menu_prompt_drop_suffix_if_wrapped(char* prompt, cptr suffix,
    int term_wid, bool use_story_font)
{
    size_t prompt_len;
    size_t suffix_len;

    if (!prompt || !suffix || !suffix[0])
        return false;

    if (menu_prompt_fits_one_row(prompt, term_wid, use_story_font))
        return true;

    prompt_len = strlen(prompt);
    suffix_len = strlen(suffix);

    if (prompt_len >= suffix_len
        && streq(prompt + prompt_len - suffix_len, suffix))
    {
        prompt[prompt_len - suffix_len] = '\0';
        prompt_len -= suffix_len;
        while (prompt_len > 0 && isspace((unsigned char)prompt[prompt_len - 1]))
            prompt[--prompt_len] = '\0';
    }

    while (prompt[0] && !menu_prompt_fits_one_row(prompt, term_wid,
               use_story_font))
    {
        size_t len = strlen(prompt);

        if (len == 0)
            break;

        prompt[len - 1] = '\0';
    }

    return false;
}

bool death_spectator_allow_menu_action(void)
{
    if (!death_spectator_active())
        return true;

    msg_print("You can no longer take that action.");
    return false;
}

/*
 * Apply tilemode overrides for special artefacts.
 * Currently used to distinguish Morgoth's crown variants
 * once Silmarils have been removed.
 */
byte object_attr_graphics_override(const object_type* o_ptr, byte base_attr)
{
    if (!o_ptr)
        return base_attr;

    /* Only adjust if this is already a tile (high bit set). */
    if (!(base_attr & 0x80))
        return base_attr;

    byte preserved = base_attr & (GRAPHICS_GLOW_MASK | TILE_FLAG);

    if ((o_ptr->name1 >= ART_MORGOTH_0) && (o_ptr->name1 <= ART_MORGOTH_2))
    {
        byte base = preserved | TILE_FLAG;
        return TILE_SET_INDEX(base, 12);
    }

    return base_attr;
}

char object_char_graphics_override(const object_type* o_ptr, char base_char)
{
    if (!o_ptr)
        return base_char;

    /* Only adjust if this is already a tile (high bit set). */
    if (!(base_char & 0x80))
        return base_char;

    byte preserved = base_char & (GRAPHICS_ALERT_MASK | TILE_FLAG);
    byte column = 0;

    switch (o_ptr->name1)
    {
    case ART_MORGOTH_2:
        column = 23;
        break;
    case ART_MORGOTH_1:
        column = 24;
        break;
    case ART_MORGOTH_0:
        column = 25;
        break;
    default:
        return base_char;
    }

    byte base = preserved | TILE_FLAG;
    return (char)TILE_SET_INDEX(base, column);
}

bool inventory_menu_set_include_equip(bool include)
{
    bool old = inventory_menu_include_equip;
    inventory_menu_include_equip = include;
    return old;
}

#include <time.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

char index_to_label(int i)
{
    if ((inventory_menu_uses_visible_labels()
        || inventory_menu_uses_expanded_supplies())
        && (i < INVEN_WIELD || i >= SUPPLIES_INDEX))
    {
        int ordinal = inventory_visible_inven_ordinal(i);
        if (ordinal >= 0)
            return I2A(ordinal);
        return 'a';
    }

    /* Indexes for "inven" get an offset when supplies are present */
    if (i < INVEN_WIELD)
    {
        int offset;

        if ((p_ptr->get_item_mode != 0) || inventory_menu_uses_expanded_supplies())
            offset = inventory_visible_supply_count();
        else
            offset = (supplies_entry_count() > 0) ? 1 : 0;

        return (I2A(i + offset));
    }

    /* Indexes for "equip" are offset */
    return (I2A(i - INVEN_WIELD));
}

/*
 * Convert a label into the index of an item in the "inven".
 *
 * Return "-1" if the label does not indicate a real item.
 */
s16b label_to_inven(int c)
{
    int i;
    int result;

    /* Convert */
    i = (islower((unsigned char)c) ? A2I(c) : -1);

    if (inventory_menu_uses_visible_labels()
        || inventory_menu_uses_expanded_supplies())
    {
        result = inventory_visible_inven_item_at(i);
        if (inventory_choice_debug_logging)
        {
            log_debug("selector[label_to_inven]: input='%c' ordinal=%d result=%d",
                c, i, result);
        }
        return result;
    }
    else if (supplies_entry_count() > 0)
    {
        if (c == supplies_label_char())
        {
            if (inventory_choice_debug_logging)
            {
                log_debug("selector[label_to_inven]: input='%c' result=%d "
                    "(supply summary)", c, SUPPLIES_INDEX);
            }
            return SUPPLIES_INDEX;
        }

        i -= 1;
    }

    /* Verify the index */
    if ((i < 0) || (i >= INVEN_PACK))
        return (-1);

    /* Empty slots can never be chosen */
    if (!inventory[i].k_idx)
        return (-1);

    /* Return the index */
    if (inventory_choice_debug_logging)
    {
        log_debug("selector[label_to_inven]: input='%c' slot=%d result=%d",
            c, i, i);
    }
    return (i);
}

/*
 * Convert a label into the index of a item in the "equip".
 *
 * Return "-1" if the label does not indicate a real item.
 */
s16b label_to_equip(int c)
{
    int i;

    /* Convert */
    i = (islower((unsigned char)c) ? A2I(c) : -1) + INVEN_WIELD;

    /* Verify the index */
    if ((i < INVEN_WIELD) || (i >= INVEN_TOTAL))
        return (-1);

    /* Empty slots can normally never be chosen, except explicit slot prompts. */
    if (!inventory[i].k_idx
        && !(throw_slot_menu_active && throw_slot_enabled[i]))
        return (-1);

    /* Return the index */
    return (i);
}

bool supplies_visible_for_current_filter(void)
{
    if (supplies_entry_count() <= 0)
        return false;

    if (item_tester_full)
        return true;

    if (!item_tester_tval && !item_tester_hook)
        return true;

    if (supplies_has_pending_action())
        return true;

    return supplies_any_match_item_tester();
}

bool inventory_menu_uses_visible_labels(void)
{
    return (p_ptr->get_item_mode != 0);
}

bool inventory_menu_uses_expanded_supplies(void)
{
    return inventory_menu_expand_supplies;
}

static bool supply_entry_matches_current_filter(int idx)
{
    object_type* o_ptr = supplies_entry_at(idx);

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (item_tester_full)
        return true;

    if (!item_tester_tval && !item_tester_hook)
        return true;

    if (supplies_has_pending_action())
        return true;

    return item_tester_okay(o_ptr);
}

int inventory_visible_supply_count(void)
{
    int count = 0;

    if (supplies_entry_count() <= 0)
        return 0;

    if (!inventory_menu_uses_expanded_supplies())
        return supplies_visible_for_current_filter() ? 1 : 0;

    for (int i = 0; i < supplies_entry_count(); i++)
    {
        if (supply_entry_matches_current_filter(i))
            count++;
    }

    return count;
}

int inventory_visible_supply_item_at(int ordinal)
{
    int count = 0;

    if (ordinal < 0)
        return -1;

    if (!inventory_menu_uses_expanded_supplies())
        return ((ordinal == 0) && supplies_visible_for_current_filter())
            ? SUPPLIES_INDEX
            : -1;

    for (int i = 0; i < supplies_entry_count(); i++)
    {
        if (!supply_entry_matches_current_filter(i))
            continue;

        if (count == ordinal)
            return SUPPLIES_INDEX + i;

        count++;
    }

    return -1;
}

int inventory_visible_supply_ordinal(int item)
{
    int ordinal = 0;
    int supply_idx = item - SUPPLIES_INDEX;

    if (item < SUPPLIES_INDEX)
        return -1;

    if (!inventory_menu_uses_expanded_supplies())
        return ((item == SUPPLIES_INDEX) && supplies_visible_for_current_filter())
            ? 0
            : -1;

    if (supply_idx < 0 || supply_idx >= supplies_entry_count())
        return -1;

    if (!supply_entry_matches_current_filter(supply_idx))
        return -1;

    for (int i = 0; i < supply_idx; i++)
    {
        if (supply_entry_matches_current_filter(i))
            ordinal++;
    }

    return ordinal;
}

int inventory_visible_inven_item_at(int ordinal)
{
    if (ordinal < 0)
        return -1;

    int supply_count = inventory_visible_supply_count();
    if (ordinal < supply_count)
        return inventory_visible_supply_item_at(ordinal);

    int visible = supply_count;

    for (int i = 0; i < INVEN_PACK; i++)
    {
        if (!inventory[i].k_idx || !get_item_okay(i))
            continue;

        if (visible == ordinal)
            return i;

        visible++;
    }

    if (inventory_menu_include_equip)
    {
        for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            if (!get_item_okay(i))
                continue;

            if (visible == ordinal)
                return i;

            visible++;
        }
    }

    return -1;
}

int inventory_visible_inven_ordinal(int item)
{
    int supply_ordinal = inventory_visible_supply_ordinal(item);
    if (supply_ordinal >= 0)
        return supply_ordinal;

    int visible = inventory_visible_supply_count();

    for (int i = 0; i < INVEN_PACK; i++)
    {
        if (!inventory[i].k_idx || !get_item_okay(i))
            continue;

        if (i == item)
            return visible;

        visible++;
    }

    if (inventory_menu_include_equip)
    {
        for (int i = INVEN_WIELD; i < INVEN_TOTAL; i++)
        {
            if (!get_item_okay(i))
                continue;

            if (i == item)
                return visible;

            visible++;
        }
    }

    return -1;
}

char inventory_visible_label_for_item(int item)
{
    int ordinal = inventory_visible_inven_ordinal(item);

    if (ordinal >= 0)
        return I2A(ordinal);

    return index_to_label(item);
}

bool inventory_item_is_supply_summary(int item)
{
    return (item == SUPPLIES_INDEX) && !inventory_menu_uses_expanded_supplies();
}

bool inventory_item_is_supply_entry(int item)
{
    return (item >= SUPPLIES_INDEX) && inventory_menu_uses_expanded_supplies();
}

bool inventory_item_is_equipment(int item)
{
    return (item >= INVEN_WIELD) && (item < INVEN_TOTAL);
}

object_type* inventory_item_to_object_ptr(int item)
{
    if (item >= SUPPLIES_INDEX)
        return supplies_entry_at(item - SUPPLIES_INDEX);

    if (item >= 0)
        return &inventory[item];

    return &o_list[0 - item];
}

bool inventory_item_uses_inven_channel(int item)
{
    return ((item >= 0) && (item < INVEN_WIELD)) || (item >= SUPPLIES_INDEX);
}

void describe_inventory_menu_entry(int item, char* buf, size_t len)
{
    object_type* o_ptr;
    char o_name[80];

    if (!buf || len == 0)
        return;

    buf[0] = '\0';

    if (inventory_item_is_supply_summary(item))
    {
        format_supply_summary(buf, len);
        return;
    }

    o_ptr = inventory_item_to_object_ptr(item);
    if (!o_ptr)
    {
        SDL_strlcpy(buf, "(invalid)", len);
        return;
    }

    if (item < 0)
    {
        object_desc_floor(buf, len, o_ptr, true, 3);
        return;
    }

    if (!o_ptr->k_idx)
    {
        if (inventory_item_is_equipment(item))
            SDL_strlcpy(buf, describe_empty_slot(item), len);
        else
            SDL_strlcpy(buf, "(invalid)", len);
        return;
    }

    if (inventory_menu_include_equip && inventory_item_is_equipment(item))
    {
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
        strnfmt(buf, len, "%s: %s", mention_use(item), o_name);
        return;
    }

    if (inventory_item_is_supply_entry(item))
        object_desc(buf, len, o_ptr, true, 3);
    else
        object_desc_floor(buf, len, o_ptr, true, 3);
}

bool item_prompt_is_replace(cptr pmt)
{
    return pmt && strstr(pmt, "Replace which item to pick up ") != NULL;
}

void log_inventory_selector_state(cptr stage, cptr pmt,
    const int vis_inven[], int vis_inven_cnt)
{
    char desc[80];

    if (!inventory_choice_debug_logging)
        return;

    log_debug("selector[%s]: prompt='%s' see=%d wrk=%d vis_inven_cnt=%d "
        "visible_supplies=%d expanded_supplies=%d include_equip=%d",
        stage ? stage : "?", pmt ? pmt : "", p_ptr->command_see ? 1 : 0,
        p_ptr->command_wrk, vis_inven_cnt, inventory_visible_supply_count(),
        inventory_menu_uses_expanded_supplies() ? 1 : 0,
        inventory_menu_include_equip ? 1 : 0);

    for (int row = 0; row < vis_inven_cnt; row++)
    {
        int item = vis_inven[row];
        object_type* o_ptr = inventory_item_to_object_ptr(item);
        char label = index_to_label(item);

        if (o_ptr && o_ptr->k_idx)
            object_desc(desc, sizeof(desc), o_ptr, true, 3);
        else if (inventory_item_is_supply_summary(item))
            format_supply_summary(desc, sizeof(desc));
        else
            SDL_strlcpy(desc, "(invalid)", sizeof(desc));

        log_debug("selector[%s]: row=%d item=%d label=%c supply_summary=%d "
            "supply_entry=%d desc='%s'",
            stage ? stage : "?", row, item, label ? label : '?',
            inventory_item_is_supply_summary(item) ? 1 : 0,
            inventory_item_is_supply_entry(item) ? 1 : 0, desc);
    }
}

void format_supply_summary(char* buf, size_t len)
{
    int herbs = 0;
    int food = 0;
    int potions = 0;
    int gems = 0;
    int lights = 0;
    bool first = true;
    char segment[32];

    if (!buf || len == 0)
        return;

    supplies_count_totals(&herbs, &food, &potions, &gems, &lights);

    SDL_strlcpy(buf, "Supplies", len);

    if (herbs <= 0 && food <= 0 && potions <= 0 && gems <= 0 && lights <= 0)
        return;

    SDL_strlcat(buf, " (", len);

    if (herbs > 0)
    {
        strnfmt(segment, sizeof(segment), "%d herb%s", herbs,
            (herbs == 1) ? "" : "s");
        SDL_strlcat(buf, segment, len);
        first = false;
    }

    if (food > 0)
    {
        if (!first)
            SDL_strlcat(buf, ", ", len);
        strnfmt(segment, sizeof(segment), "%d food", food);
        SDL_strlcat(buf, segment, len);
        first = false;
    }

    if (potions > 0)
    {
        if (!first)
            SDL_strlcat(buf, ", ", len);
        strnfmt(segment, sizeof(segment), "%d potion%s", potions,
            (potions == 1) ? "" : "s");
        SDL_strlcat(buf, segment, len);
        first = false;
    }

    if (gems > 0)
    {
        if (!first)
            SDL_strlcat(buf, ", ", len);
        strnfmt(segment, sizeof(segment), "%d gem%s", gems,
            (gems == 1) ? "" : "s");
        SDL_strlcat(buf, segment, len);
        first = false;
    }

    if (lights > 0)
    {
        if (!first)
            SDL_strlcat(buf, ", ", len);
        strnfmt(segment, sizeof(segment), "%d oil slot%s", lights,
            (lights == 1) ? "" : "s");
        SDL_strlcat(buf, segment, len);
    }

    SDL_strlcat(buf, ")", len);
}


/*
 * Determine which equipment slot (if any) an item likes
 */
s16b wield_slot(const object_type* o_ptr)
{
    /* Slot for equipment */
    switch (o_ptr->tval)
    {
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    {
        return (INVEN_WIELD);
    }

    case TV_BOW:
    {
        return (INVEN_BOW);
    }

    case TV_STAFF:
    {
        return (INVEN_STAFF);
    }

    case TV_HORN:
    {
        return (INVEN_HORN);
    }

    case TV_RING:
    {
        /* Use the right hand first */
        if (!inventory[INVEN_RIGHT].k_idx)
            return (INVEN_RIGHT);

        /* Use the left hand for swapping (by default) */
        return (INVEN_LEFT);
    }

    case TV_AMULET:
    {
        return (INVEN_NECK);
    }

    case TV_LIGHT:
    {
        return (INVEN_LITE);
    }

    case TV_MAIL:
    case TV_SOFT_ARMOR:
    {
        return (INVEN_BODY);
    }

    case TV_CLOAK:
    {
        return (INVEN_OUTER);
    }

    case TV_SHIELD:
    {
        return (INVEN_ARM);
    }

    case TV_CROWN:
    case TV_HELM:
    {
        return (INVEN_HEAD);
    }

    case TV_GLOVES:
    {
        return (INVEN_HANDS);
    }

    case TV_BOOTS:
    {
        return (INVEN_FEET);
    }

    case TV_ARROW:
    {
        // Use the first similar quiver if there is one
        if (object_similar(&inventory[INVEN_QUIVER1], o_ptr))
            return (INVEN_QUIVER1);
        if (object_similar(&inventory[INVEN_QUIVER2], o_ptr))
            return (INVEN_QUIVER2);

        // Use the 2nd quiver if it is the only empty one
        if (!inventory[INVEN_QUIVER2].k_idx && inventory[INVEN_QUIVER1].k_idx)
            return (INVEN_QUIVER2);

        // Use the 1st quiver otherwise
        return (INVEN_QUIVER1);
    }
    }

    /* No slot available */
    return (-1);
}

/*
 * Return a string mentioning how a given item is carried
 */
cptr describe_empty_slot(int i)
{
    cptr p;

    /* Examine the location */
    switch (i)
    {
    case INVEN_WIELD:
        p = "(no weapon)";
        break;
    case INVEN_BOW:
        p = "(no bow)";
        break;
    case INVEN_STAFF:
        p = "(no walking staff)";
        break;
    case INVEN_LEFT:
        p = "(no left ring)";
        break;
    case INVEN_RIGHT:
        p = "(no right ring)";
        break;
    case INVEN_NECK:
        p = "(no amulet)";
        break;
    case INVEN_LITE:
        p = "(no light source)";
        break;
    case INVEN_BODY:
        p = "(no body armour)";
        break;
    case INVEN_OUTER:
        p = "(no cloak)";
        break;
    case INVEN_ARM:
        p = "(no shield)";
        break;
    case INVEN_HEAD:
        p = "(no helmet)";
        break;
    case INVEN_HANDS:
        p = "(no gloves)";
        break;
    case INVEN_FEET:
        p = "(no boots)";
        break;
    case INVEN_QUIVER1:
        p = "(empty 1st quiver)";
        break;
    case INVEN_QUIVER2:
        p = "(empty 2nd quiver)";
        break;
    case INVEN_HORN:
        p = "(no horn)";
        break;
    default:
        p = "(empty slot)";
        break;
    }

    /* Return the result */
    return (p);
}

/*
 * Return a string mentioning how a given item is carried
 */
cptr mention_use(int i)
{
    cptr p;

    /* Examine the location */
    switch (i)
    {
    case INVEN_WIELD:
        p = "Wielding";
        break;
    case INVEN_BOW:
        p = "Shooting";
        break;
    case INVEN_STAFF:
        p = "Walking staff";
        break;
    case INVEN_LEFT:
        p = "Left ring";
        break;
    case INVEN_RIGHT:
        p = "Right ring";
        break;
    case INVEN_NECK:
        p = "Around neck";
        break;
    case INVEN_LITE:
        p = "Light";
        break;
    case INVEN_BODY:
        p = "On body";
        break;
    case INVEN_OUTER:
        p = "About body";
        break;
    case INVEN_ARM:
        p = "Off-hand";
        break;
    case INVEN_HEAD:
        p = "On head";
        break;
    case INVEN_HANDS:
        p = "On hands";
        break;
    case INVEN_FEET:
        p = "On feet";
        break;
    case INVEN_QUIVER1:
        p = "1st quiver";
        break;
    case INVEN_QUIVER2:
        p = "2nd quiver";
        break;
    case INVEN_HORN:
        p = "Horn";
        break;
    default:
        p = "In pack";
        break;
    }

    /* Return the result */
    return (p);
}

/*
 * Return a string describing how a given item is being worn.
 * Currently, only used for items in the equipment, not inventory.
 */
cptr describe_use(int i)
{
    cptr p;

    switch (i)
    {
    case INVEN_WIELD:
        p = "wielding";
        break;
    case INVEN_BOW:
        p = "wielding";
        break;
    case INVEN_STAFF:
        p = "using as a walking staff";
        break;
    case INVEN_LEFT:
        p = "wearing on your left hand";
        break;
    case INVEN_RIGHT:
        p = "wearing on your right hand";
        break;
    case INVEN_NECK:
        p = "wearing around your neck";
        break;
    case INVEN_LITE:
        p = "using to light the way";
        break;
    case INVEN_BODY:
        p = "wearing on your body";
        break;
    case INVEN_OUTER:
        p = "wearing on your back";
        break;
    case INVEN_ARM:
        p = "wearing on your arm";
        break;
    case INVEN_HEAD:
        p = "wearing on your head";
        break;
    case INVEN_HANDS:
        p = "wearing on your hands";
        break;
    case INVEN_FEET:
        p = "wearing on your feet";
        break;
    case INVEN_QUIVER1:
        p = "carrying in your quiver";
        break;
    case INVEN_QUIVER2:
        p = "carrying in your quiver";
        break;
    case INVEN_HORN:
        p = "carrying at your side";
        break;
    default:
        p = "carrying in your pack";
        break;
    }

    /* Return the result */
    return p;
}

/*
 * Return true if this skeleton has already been searched.
 */
bool object_is_searched_skeleton(const object_type* o_ptr)
{
    return o_ptr && o_ptr->k_idx && (o_ptr->tval == TV_SKELETON)
        && (o_ptr->pval <= 0);
}

/*
 * Check an item against the item tester info
 */
bool item_tester_okay(const object_type* o_ptr)
{
    bool in_inventory = (o_ptr >= inventory) && (o_ptr < inventory + INVEN_TOTAL);

    if (throw_slot_menu_active && in_inventory)
    {
        int idx = (int)(o_ptr - inventory);

        if (!throw_slot_enabled[idx])
            return (false);

        if (!o_ptr->k_idx)
            return (true);
    }

    /* Require an item */
    if (!o_ptr->k_idx)
    {
        /* Hack -- allow listing empty slots only for equipment */
        if (item_tester_full && in_inventory && (o_ptr >= inventory + INVEN_WIELD))
            return (true);
        return (false);
    }

    if (!in_inventory && object_is_searched_skeleton(o_ptr))
        return (false);

    /* Check the tval */
    if (item_tester_tval)
    {
        if (!(item_tester_tval == o_ptr->tval))
            return (false);
    }

    /* Check the hook */
    if (item_tester_hook)
    {
        if (!(*item_tester_hook)(o_ptr))
            return (false);
    }

    /* Assume okay */
    return (true);
}

/*
 * Get the indexes of objects at a given floor location.
 *
 * Return the number of object indexes acquired.
 *
 * Never acquire more than "size" object indexes, and never return a
 * number bigger than "size", even if more floor objects exist.
 *
 * Valid flags are any combination of the bits:
 *
 *   0x01 -- Verify item tester
 *   0x02 -- Marked items only
 */
int scan_floor(int* items, int size, int y, int x, int mode)
{
    int this_o_idx, next_o_idx;

    int num = 0;

    /* Sanity */
    if (!in_bounds(y, x))
        return (0);

    /* Scan all objects in the grid */
    for (this_o_idx = cave_o_idx[y][x]; this_o_idx; this_o_idx = next_o_idx)
    {
        object_type* o_ptr;

        /* Get the object */
        o_ptr = &o_list[this_o_idx];

        /* Get the next object */
        next_o_idx = o_ptr->next_o_idx;

        /* Verify item tester */
        if ((mode & 0x01) && !item_tester_okay(o_ptr))
            continue;

        /* Marked items only */
        if ((mode & 0x02) && !o_ptr->marked)
            continue;

        /* Accept this item */
        items[num++] = this_o_idx;

        /* Enforce size limit */
        if (num >= size)
            break;
    }

    /* Result */
    return (num);
}

/*
 * Choice window "shadow" of the "show_inven()" function
 */
void display_inven(void)
{
    register int i, n, z = 0;

    object_type* o_ptr;

    byte attr;
    bool use_story_font = story_inventory_enabled();
    story_font_term_state story_state;

    char tmp_val[80];

    char o_name[80];

    bool floor_item = false;

    int w = Term->wid;
    int col = w - 11;
    if (col < 0) col = 0;
    int offset = use_bigtile ? 6 : 5;

    story_font_term_push(use_story_font, false, &story_state);

    /* Find the "final" slot */
    for (i = 0; i < INVEN_PACK; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Track */
        z = i + 1;
    }

    /* Display the pack (and the floor) */
    for (i = 0; i <= z; i++)
    {
        if (i == z)
        {
            // get item from floor
            o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
            i = INVEN_WIELD;
            if (!o_ptr->k_idx)
                continue;
            floor_item = true;
        }
        else
        {
            // get item from inventory
            o_ptr = &inventory[i];
        }

        /* Start with an empty "index" */
        tmp_val[0] = tmp_val[1] = tmp_val[2] = ' ';
        tmp_val[3] = '\0';

        /* Is this item "acceptable"? */
        if (item_tester_okay(o_ptr))
        {
            // first, do this for inventory items...
            if (!floor_item)
            {
                // does the current command even allow inventory items to be
                // used?
                if ((p_ptr->get_item_mode == 0)
                    || (p_ptr->get_item_mode & (USE_INVEN)))
                {
                    /* Prepare a "label" */
                    tmp_val[0] = index_to_label(i);

                    /* Bracket the "label" --(-- */
                    tmp_val[1] = ')';
                }
            }
            // now for the floor item (if any)
            else
            {
                // does the current command even allow floor items to be used?
                if ((p_ptr->get_item_mode == 0)
                    || (p_ptr->get_item_mode & (USE_FLOOR)))
                {
                    /* Prepare a "label" */
                    tmp_val[0] = '-';

                    /* Bracket the "label" --(-- */
                    tmp_val[1] = ')';
                }
            }
        }

        // use white if the inventory is being queried, slate if the equipment
        // is
        if ((p_ptr->command_wrk == 0) || (p_ptr->command_wrk & (USE_INVEN)))
            attr = TERM_WHITE;
        else
            attr = TERM_SLATE;

        /* Clear the line first (story font needs a clean slate) */
        Term_erase(0, i, 255);

        /* Display the index (or blank space) */
        if (use_story_font)
            story_print_text(i, 0, 3, attr, tmp_val);
        else
            Term_putstr(0, i, 3, attr, tmp_val);

        /* Display the symbol */
        Term_putch(3, i, object_attr(o_ptr), object_char(o_ptr));
        if (use_bigtile)
        {
            Term_putch(4, i, 255, -1);
        }

        /* Obtain an item description */
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

        /* Truncate description so weights align cleanly */
        int max_desc = w - offset - 1;
        if (show_weights && col > offset)
            max_desc = col - offset - 1;
        if (max_desc < 1) max_desc = 1;
        if (max_desc >= (int)sizeof(o_name)) max_desc = (int)sizeof(o_name) - 1;
        o_name[max_desc] = '\0';

        /* Obtain the length of the description */
        n = (int)strlen(o_name);

        /* Get inventory color (match show_inven/show_equip scheme) */
        if (weapon_glows(o_ptr))
            attr = object_display_color(o_ptr, TERM_L_BLUE);
        else
            attr = object_display_color(o_ptr, tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

        /* Display the entry itself */
        Term_putch(offset - 1, i, attr, ' ');
        if (use_story_font)
            story_print_text(i, offset, max_desc, attr, o_name);
        else
            Term_putstr(offset, i, n, attr, o_name);

        /* Display the weight if needed */
        if (o_ptr->weight)
        {
            int wgt = o_ptr->weight * o_ptr->number;
            sprintf(tmp_val, "%3d.%1d lb", wgt / 10, wgt % 10);
            if (use_story_font)
                story_print_text_grid(i, col, 8, attr, tmp_val);
            else
                Term_putstr(col, i, -1, attr, tmp_val);
        }
    }

    /* Erase the rest of the window */
    for (i = z; i < Term->hgt; i++)
    {
        if ((i != INVEN_WIELD) || !floor_item)
        {
            /* Erase the line */
            Term_erase(0, i, 255);
        }
    }

    story_font_term_pop(&story_state);
}

/*
 * Choice window "shadow" of the "show_equip()" function
 */
void display_equip(void)
{
    register int i, n;
    object_type* o_ptr;
    byte attr;
    int armour_weight = 0;

    char tmp_val[80];

    char o_name[80];

    int w = Term->wid;
    int col = w - 11;
    if (col < 0) col = 0;
    int offset = use_bigtile ? 6 : 5;

    bool use_story_font = story_equipment_enabled();
    story_font_term_state story_state;
    story_font_term_push(use_story_font, false, &story_state);

    /* Display the equipment */
    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        /* Examine the item */
        o_ptr = &inventory[i];
        
        /* Start with an empty "index" */
        tmp_val[0] = tmp_val[1] = tmp_val[2] = ' ';
        tmp_val[3] = '\0';

        /* Is this item "acceptable"? */
        if (item_tester_okay(o_ptr))
        {
            // does the current command even allow equipment to be used?
            if ((p_ptr->get_item_mode == 0)
                || (p_ptr->get_item_mode & (USE_EQUIP)))
            {
                /* Prepare an "index" */
                tmp_val[0] = index_to_label(i);

                /* Bracket the "index" --)-- */
                tmp_val[1] = ')';
            }
        }

        // use white if the equipment is being queried, slate if the inventory
        // is
        if ((p_ptr->command_wrk == 0) || (p_ptr->command_wrk & (USE_EQUIP)))
            attr = TERM_WHITE;
        else
            attr = TERM_SLATE;

        /* Clear the line first (story font needs a clean slate) */
        Term_erase(0, i - INVEN_WIELD, 255);

        /* Display the index (or blank space) */
        if (use_story_font)
            story_print_text(i - INVEN_WIELD, 0, 3, attr, tmp_val);
        else
            Term_putstr(0, i - INVEN_WIELD, 3, attr, tmp_val);

        /* Display the symbol */
        if (!o_ptr->tval)
        {
            /* object_char() for an empty slot gives '\0'.  Use ' ' instead. */
            Term_putch(3, i - INVEN_WIELD, attr, ' ');
            if (use_bigtile)
            {
                Term_putch(4, i - INVEN_WIELD, attr, ' ');
            }
        }
        else
        {
            Term_putch(
                3, i - INVEN_WIELD, object_attr(o_ptr), object_char(o_ptr));
            if (use_bigtile)
            {
                Term_putch(4, i - INVEN_WIELD, 255, -1);
            }
        }

        /* Obtain an item description */
        if (o_ptr->tval)
        {
            object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
        }
        else
        {
            sprintf(o_name, "%s", describe_empty_slot(i));
        }

        /* Obtain the length of the description */
        int max_desc = w - offset - 1;
        if (show_weights && col > offset)
            max_desc = col - offset - 1;
        if (max_desc < 1) max_desc = 1;
        if (max_desc >= (int)sizeof(o_name)) max_desc = (int)sizeof(o_name) - 1;
        o_name[max_desc] = '\0';
        n = (int)strlen(o_name);

        /* Get inventory color (match show_inven/show_equip scheme) */
        if (!o_ptr->tval)
            attr = TERM_L_DARK;
        else if (weapon_glows(o_ptr))
            attr = object_display_color(o_ptr, TERM_L_BLUE);
        else
            attr = object_display_color(o_ptr, tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

        /* Display the entry itself */
        Term_putch(offset - 1, i - INVEN_WIELD, attr, ' ');
        if (use_story_font)
            story_print_text(i - INVEN_WIELD, offset, max_desc, attr, o_name);
        else
            Term_putstr(offset, i - INVEN_WIELD, n, attr, o_name);

        /* Display the weight (if needed) */
        if (o_ptr->weight)
        {
            int wgt = o_ptr->weight * o_ptr->number;
            sprintf(tmp_val, "%3d.%1d lb ", wgt / 10, wgt % 10);
            if ((i >= INVEN_BODY) && (i <= INVEN_FEET))
            {
                if (use_story_font)
                    story_print_text_grid(i - INVEN_WIELD, col, 8, TERM_SLATE, tmp_val);
                else
                    Term_putstr(col, i - INVEN_WIELD, -1, TERM_SLATE, tmp_val);
                armour_weight += wgt;
            }
            else
            {
                if (use_story_font)
                    story_print_text_grid(i - INVEN_WIELD, col, 8, attr, tmp_val);
                else
                    Term_putstr(col, i - INVEN_WIELD, -1, attr, tmp_val);
            }
        }

        // Term_erase(w - 12 + strlen(mention_use(i)), i - INVEN_WIELD, 255);
    }

    /* Put in the total weight (if any armour equipped) */
    if (armour_weight)
    {
        int divider_row;
        int text_row;
        int equip_rows = INVEN_TOTAL - INVEN_WIELD;

        equipment_weight_layout_rows(0, equip_rows, Term->hgt, &divider_row,
            &text_row);
        if (equip_rows < Term->hgt)
            Term_erase(0, equip_rows, 255);
        if ((equip_rows + 1) < Term->hgt)
            Term_erase(0, equip_rows + 1, 255);
        
        if (use_story_font)
        {
            strnfmt(tmp_val, sizeof(tmp_val), "armour: %3d.%1d lb",
                armour_weight / 10, armour_weight % 10);
            if (divider_row >= 0)
                story_print_text_grid(divider_row, col, 8, TERM_L_DARK,
                    "--------");
            if (text_row >= 0)
            {
                int armour_col = col - 8;
                if (armour_col < 0)
                    armour_col = 0;
                story_print_text_grid(text_row, armour_col, 16, TERM_SLATE,
                    tmp_val);
            }
        }
        else
        {
            strnfmt(tmp_val, sizeof(tmp_val), "armour: %3d.%1d lb",
                armour_weight / 10, armour_weight % 10);
            if (divider_row >= 0)
                Term_putstr(col, divider_row, -1, TERM_L_DARK, "--------");
            if (text_row >= 0)
            {
                int armour_col = col - 8;
                if (armour_col < 0)
                    armour_col = 0;
                Term_putstr(armour_col, text_row, -1, TERM_SLATE, tmp_val);
            }
        }
    }

    /* Erase the rest of the window (after the armour weight display) */
    int erase_start = INVEN_TOTAL - INVEN_WIELD;
    if (armour_weight)
    {
        int divider_row;
        int text_row;

        equipment_weight_layout_rows(0, erase_start, Term->hgt, &divider_row,
            &text_row);
        if (text_row >= 0)
            erase_start = text_row + 1;
        else if (divider_row >= 0)
            erase_start = divider_row + 1;
    }
    for (i = erase_start; i < Term->hgt; i++)
    {
        /* Clear that line */
        Term_erase(0, i, 255);
    }

    story_font_term_pop(&story_state);
}

/*
 * Helper function to draw an item tile/pictogram when in graphics mode.
 * This should be called before displaying the item description text.
 * Returns the column offset where text should start after the tile.
 */
int draw_item_tile_with_background(int x, int y, object_type* o_ptr,
    byte bg_attr)
{
    /* Only draw tiles in graphics mode (not ASCII or pseudo-graphics) */
    if (use_graphics != GRAPHICS_NONE && use_graphics != GRAPHICS_PSEUDO && o_ptr && o_ptr->k_idx)
    {
        /* Get the attribute from object_attr - in graphics mode this is a tile index, not a color code
         * We do NOT shade tile indices as they refer to specific graphics in the tileset */
        byte attr = object_attr(o_ptr);
        char chr = object_char(o_ptr);
        
        /* Draw the tile using the standard object display functions */
        if (bg_attr)
            Term_queue_char(x, y, attr, chr, bg_attr, ' ');
        else
            Term_putch(x, y, attr, chr);
        
        /* Handle bigtile mode (tiles that occupy 2 cells) */
        if (use_bigtile)
        {
            if (bg_attr)
            {
                if (attr & TILE_FLAG)
                    Term_queue_char(x + 1, y, 255, -1, bg_attr, ' ');
                else
                    Term_queue_char(x + 1, y, bg_attr, ' ', bg_attr, ' ');
            }
            else
            {
                Term_putch(x + 1, y, 255, -1);
            }
            return x + 2; /* Text starts 2 cells after tile */
        }
        
        return x + 1; /* Text starts 1 cell after tile */
    }
    
    /* No tile drawn, text starts at the same position */
    return x;
}

int draw_item_tile(int x, int y, object_type* o_ptr)
{
    return draw_item_tile_with_background(x, y, o_ptr, 0);
}

object_type* prepare_supply_icon_object(object_type* o_ptr)
{
    static s16b supply_icon_k_idx = -1;

    if (!o_ptr)
        return NULL;

    if (supply_icon_k_idx < 0)
        supply_icon_k_idx = lookup_kind(TV_CHEST, SV_CHEST_SMALL_WOODEN);

    if (supply_icon_k_idx <= 0)
    {
        object_wipe(o_ptr);
        return NULL;
    }

    object_wipe(o_ptr);
    o_ptr->k_idx = supply_icon_k_idx;
    o_ptr->image_k_idx = supply_icon_k_idx;
    o_ptr->tval = TV_CHEST;
    o_ptr->sval = SV_CHEST_SMALL_WOODEN;
    o_ptr->number = 1;
    o_ptr->pval = 0;
    return o_ptr;
}

int menu_term_width(void)
{
    if (Term && Term->wid > 0)
        return Term->wid;

    return 80;
}

int menu_term_height(void)
{
    if (Term && Term->hgt > 0)
        return Term->hgt;

    return 24;
}

byte inventory_menu_selected_attr(byte source_attr)
{
    (void)source_attr;
    return (byte)(TERM_UI_SELECTED + TERM_L_BLUE);
}

static void inventory_menu_fill_selected_row(int col, int row, int width,
    byte attr)
{
    int term_wid = menu_term_width();
    int term_hgt = menu_term_height();

    if (!Term || row < 0 || row >= term_hgt || width <= 0)
        return;

    if (col < 0)
    {
        width += col;
        col = 0;
    }
    if (col >= term_wid || width <= 0)
        return;
    if (col + width > term_wid)
        width = term_wid - col;

    for (int x = col; x < col + width; x++)
        Term_putch(x, row, attr, ' ');
}

void inventory_menu_fill_selected_span(int start_col, int end_col,
    int row, byte attr)
{
    if (end_col <= start_col)
        return;

    inventory_menu_fill_selected_row(start_col, row, end_col - start_col,
        attr);
}

int inventory_menu_visible_rows_for_height(int term_hgt)
{
    int rows = term_hgt - 1;

    if (rows < 0)
        rows = 0;

    if (rows > ENHANCED_MAX_LIST)
        rows = ENHANCED_MAX_LIST;

    return rows;
}

static int inventory_menu_clamp_scroll(int scroll, int total_rows,
    int visible_rows)
{
    int max_scroll;

    if (total_rows <= 0 || visible_rows <= 0 || total_rows <= visible_rows)
        return 0;

    max_scroll = total_rows - visible_rows;

    if (scroll < 0)
        return 0;
    if (scroll > max_scroll)
        return max_scroll;

    return scroll;
}

int inventory_menu_scroll_to_selection(int scroll, int selected_row,
    int total_rows, int visible_rows, int extra_rows_after_selection)
{
    int bottom_slack;

    scroll = inventory_menu_clamp_scroll(scroll, total_rows, visible_rows);

    if (selected_row < 0 || total_rows <= 0 || visible_rows <= 0)
        return scroll;

    if (extra_rows_after_selection < 0)
        extra_rows_after_selection = 0;
    if (extra_rows_after_selection >= visible_rows)
        extra_rows_after_selection = visible_rows - 1;

    if (selected_row < scroll)
    {
        scroll = selected_row;
    }
    else
    {
        bottom_slack = visible_rows - 1 - extra_rows_after_selection;
        if ((selected_row - scroll) > bottom_slack)
            scroll = selected_row - bottom_slack;
    }

    return inventory_menu_clamp_scroll(scroll, total_rows, visible_rows);
}

int menu_weight_col_for_width(int term_wid)
{
    /* Reserve 8 columns for weight and 4 columns for the trailing label. */
    int col = term_wid - 12;

    if (col < 0)
        col = 0;

    return col;
}

int menu_label_col_for_width(int term_wid, bool display_weights)
{
    /* Labels are rendered as " (a)" or "(a)" and need 4 terminal cells. */
    int col = term_wid - 4;

    (void)display_weights;

    if (col < 0)
        col = 0;

    return col;
}

int menu_center_col_for_len(int term_wid, int len)
{
    if (len >= term_wid)
        return 0;

    return (term_wid - len) / 2;
}

int menu_overlay_clear_col(int col)
{
    /* Keep a one-cell gutter so centered overlays stay visually separate. */
    if (col > 0)
        return col - 1;

    return 0;
}

int menu_desc_limit(int text_col, int label_col, int weight_col,
    bool display_weights)
{
    int right_edge = display_weights ? weight_col : label_col;
    int limit = right_edge - text_col;

    if (limit < 1)
        limit = 1;

    return limit;
}

void story_render_inventory_entry(int row, int base_col, int label_col,
    cptr desc, byte desc_attr, bool display_weights, cptr weight_text,
    byte weight_attr, cptr label_text, byte label_attr, const object_type* o_ptr,
    bool highlight, int story_term_w)
{
    int weight_col = display_weights ? MAX(0, label_col - 8) : label_col;
    const int label_width = 4;
    byte fill_attr = inventory_menu_selected_attr(desc_attr);

    (void)story_term_w;

    if (highlight)
    {
        desc_attr = fill_attr;
        weight_attr = fill_attr;
        label_attr = fill_attr;
    }

    int text_col = base_col;
    int selection_end = label_col + label_width;

    Term_erase(base_col, row, 255);
    if (highlight)
        story_fill_rect(row, base_col, selection_end - base_col, fill_attr);
    if (o_ptr && o_ptr->k_idx)
        text_col = draw_item_tile_with_background(base_col, row,
            (object_type*)o_ptr, highlight ? fill_attr : 0);
    int desc_limit = menu_desc_limit(text_col, label_col, weight_col,
        display_weights);
    story_print_text(row, text_col, desc_limit, desc_attr, desc);

    if (display_weights && weight_text && weight_text[0])
    {
        int weight_width = label_col - weight_col;
        if (weight_width < 1)
            weight_width = 1;
        story_print_text_grid(row, weight_col, weight_width, weight_attr,
            weight_text);
    }

    if (label_text && label_text[0])
        story_print_text_grid(row, label_col, label_width, label_attr,
            label_text);
}

void story_render_equipment_entry(int row, int col, int slot, cptr prefix,
    byte prefix_attr, cptr desc, byte desc_attr, bool display_weights,
    cptr weight_text, byte weight_attr, cptr label_text, byte label_attr,
    const object_type* o_ptr, bool highlight, int story_term_w)
{
    int term_wid = (story_term_w > 0) ? story_term_w : menu_term_width();
    int label_col = menu_label_col_for_width(term_wid, display_weights);
    int weight_col = menu_weight_col_for_width(term_wid);
    int clear_col = menu_overlay_clear_col(col);
    const int label_width = 4;
    bool has_object = (o_ptr && o_ptr->k_idx);
    byte fill_attr = inventory_menu_selected_attr(desc_attr);

    if (highlight)
    {
        prefix_attr = fill_attr;
        desc_attr = fill_attr;
        weight_attr = fill_attr;
        label_attr = fill_attr;
    }

    int text_col = col + 12 + 2;
    int selection_end = label_col + label_width;

    Term_erase(clear_col, row, 255);

    if (highlight)
        story_fill_rect(row, col, selection_end - col, fill_attr);
    if (has_object)
        text_col = draw_item_tile_with_background(col + 12 + 2, row,
            (object_type*)o_ptr, highlight ? fill_attr : 0);
    story_print_equipment_prefix(row, col, prefix_attr, prefix);

    int desc_limit = menu_desc_limit(text_col, label_col, weight_col,
        display_weights);

    char combined_desc[160];
    story_prepare_equipment_desc(combined_desc, sizeof(combined_desc), desc,
        slot, has_object, desc_limit);
    story_print_text(row, text_col, desc_limit, desc_attr, combined_desc);

    if (display_weights && weight_text && weight_text[0])
    {
        int weight_width = label_col - weight_col;
        if (weight_width < 1)
            weight_width = 1;
        story_print_text_grid(row, weight_col, weight_width, weight_attr,
            weight_text);
    }

    if (label_text && label_text[0])
        story_print_text_grid(row, label_col, label_width, label_attr,
            label_text);
}

void equipment_weight_layout_rows(int first_row, int item_count,
    int term_hgt, int* divider_row, int* text_row)
{
    int next_row = first_row + item_count;
    int bottom_row = term_hgt - 1;

    *divider_row = -1;
    *text_row = -1;

    if (term_hgt <= 0)
        return;

    if (next_row + 1 <= bottom_row)
    {
        *divider_row = next_row;
        *text_row = next_row + 1;
    }
    else if (next_row <= bottom_row)
    {
        *text_row = next_row;
    }
}

int equipment_first_occupied_row(int entry_count, const int* out_index)
{
    for (int idx = 0; idx < entry_count; idx++)
    {
        int slot = out_index[idx];

        if ((slot >= INVEN_WIELD) && (slot < INVEN_TOTAL)
            && inventory[slot].k_idx)
            return idx;
    }

    return -1;
}

int equipment_next_occupied_row(int entry_count, const int* out_index,
    int current, int step)
{
    if (entry_count <= 0)
        return -1;

    if (current < 0 || current >= entry_count)
        current = equipment_first_occupied_row(entry_count, out_index);

    if (current < 0)
        return -1;

    for (int count = 0; count < entry_count; count++)
    {
        current = (current + entry_count + step) % entry_count;
        if (inventory[out_index[current]].k_idx)
            return current;
    }

    return -1;
}

void draw_equipment_story_rows(int col, int entry_count, int* out_index,
    byte* out_color, char out_desc[][80], bool highlight_active,
    int highlight_index, bool display_weights, int story_term_w)
{
    int term_wid = (story_term_w > 0) ? story_term_w : menu_term_width();
    int label_col_base = menu_label_col_for_width(term_wid, display_weights);
    int weight_col = menu_weight_col_for_width(term_wid);
    int clear_col = menu_overlay_clear_col(col);
    const int label_width = 4;

    log_trace("draw_equipment_story_rows: entry_count=%d, highlight_active=%d, highlight_index=%d",
        entry_count, highlight_active, highlight_index);

    for (int idx = 0; idx < entry_count; idx++)
    {
        int row = idx + 1;
        bool is_highlight = highlight_active && idx == highlight_index;
        byte selected_attr = inventory_menu_selected_attr(out_color[idx]);
        byte line_attr = is_highlight ? selected_attr : out_color[idx];
        int slot = out_index[idx];
        object_type* o_ptr = &inventory[slot];
        bool has_object = o_ptr->k_idx != 0;

        if (is_highlight)
        {
            log_trace("draw_equipment_story_rows: Drawing HIGHLIGHTED row %d, slot=%d, has_object=%d, desc='%s'",
                row, slot, has_object, out_desc[idx]);
        }

        Term_erase(clear_col, row, 255);

        char prefix[32];
        strnfmt(prefix, sizeof(prefix), "%-12s: ", mention_use(slot));
        byte prefix_attr = is_highlight ? selected_attr : TERM_WHITE;

        int text_col = col + 12 + 2;
        int label_col = label_col_base;
        log_trace("draw_equipment_story_rows: Row %d - text_col calculated as %d (col=%d + 12 + 2)", row, text_col, col);
        if (is_highlight)
        {
            int selection_end = label_col + label_width;

            log_trace("draw_equipment_story_rows: Filling highlight row %d", row);
            story_fill_rect(row, col, selection_end - col, selected_attr);
        }
        if (has_object)
        {
            int tile_end_col = draw_item_tile_with_background(text_col, row,
                o_ptr, is_highlight ? selected_attr : 0);
            log_trace("draw_equipment_story_rows: Row %d - drew tile, text_col updated from %d to %d", row, text_col, tile_end_col);
            text_col = tile_end_col;
        }
        log_trace("draw_equipment_story_rows: Row %d - printing prefix '%s' at col=%d", row, prefix, col);
        story_print_equipment_prefix(row, col, prefix_attr, prefix);

        int desc_limit = menu_desc_limit(text_col, label_col, weight_col,
            display_weights);

        char combined_desc[160];
        story_prepare_equipment_desc(combined_desc, sizeof(combined_desc),
            out_desc[idx], slot, has_object, desc_limit);

        log_trace("draw_equipment_story_rows: Row %d - printing desc '%s' at col=%d limit=%d",
            row, combined_desc, text_col, desc_limit);
        story_print_text(row, text_col, desc_limit, line_attr, combined_desc);

        if (display_weights && has_object && o_ptr->weight)
        {
            int wgt = o_ptr->weight * o_ptr->number;
            char weight_buf[16];
            strnfmt(weight_buf, sizeof(weight_buf), "%2d.%1d lb", wgt / 10, wgt % 10);
            int weight_width = label_col - weight_col;
            if (weight_width < 1)
                weight_width = 1;
            log_trace("draw_equipment_story_rows: Row %d - printing weight '%s' at col=%d width=%d", row, weight_buf, weight_col, weight_width);
            story_print_text_grid(row, weight_col, weight_width, line_attr,
                weight_buf);
        }

        char label_buf[8];
        strnfmt(label_buf, sizeof(label_buf), "(%c)", index_to_label(slot));
        byte label_attr = is_highlight ? selected_attr : TERM_WHITE;
        log_trace("draw_equipment_story_rows: Row %d - printing label '%s' at col=%d width=%d (label_col_base=%d)", row, label_buf, label_col, label_width, label_col_base);
        story_print_text_grid(row, label_col, label_width, label_attr,
            label_buf);
    }

    log_trace("draw_equipment_story_rows: Finished drawing all rows");
}

/*
 * Display the inventory.
 *
 * Hack -- do not display "trailing" empty slots
 */
void show_inven(void)
{
    int i, j, k, l;
    int col, len, lim;
    int term_wid = menu_term_width();
    int term_hgt = menu_term_height();
    int display_rows = inventory_menu_visible_rows_for_height(term_hgt);
    int weight_col = menu_weight_col_for_width(term_wid);
    int label_col = menu_label_col_for_width(term_wid, show_weights);

    object_type* o_ptr;

    char o_name[80];

    char tmp_val[80];

    int out_index[ENHANCED_MAX_LIST];
    byte out_color[ENHANCED_MAX_LIST];
    char out_desc[ENHANCED_MAX_LIST][80];

    bool use_story_font = story_inventory_enabled();
    story_font_term_state story_state;
    int story_term_w = 0;
    story_inventory_list_active = use_story_font;
    story_font_term_push(use_story_font, false, &story_state);
    if (use_story_font) {
        int story_term_h = 0;
        Term_get_size(&story_term_w, &story_term_h);
    }

    /* Default length */
    len = 29;

    /* Maximum space allowed for descriptions */
    lim = term_wid - 3;

    if (lim < 0)
        lim = 0;

    /* Require space for weight (if needed) */
    if (show_weights && lim > (weight_col - 1))
        lim = weight_col - 1;

    if (lim < 0)
        lim = 0;

    bool include_supplies = supplies_visible_for_current_filter();

    k = 0;

    if (include_supplies && !inventory_menu_uses_expanded_supplies()
        && k < ENHANCED_MAX_LIST)
    {
        char supply_desc[80];
        format_supply_summary(supply_desc, sizeof(supply_desc));
        out_index[k] = SUPPLIES_INDEX;
        out_color[k] = TERM_L_WHITE;
        SDL_strlcpy(out_desc[k], supply_desc, sizeof(out_desc[0]));

        l = (int)strlen(out_desc[k]) + 5;
        if (show_weights)
            l += 9;
        if (l > len)
            len = l;

        k++;
    }

    if (include_supplies && inventory_menu_uses_expanded_supplies())
    {
        for (i = 0; i < supplies_entry_count() && k < ENHANCED_MAX_LIST; i++)
        {
            int item = SUPPLIES_INDEX + i;

            o_ptr = supplies_entry_at(i);
            if (!o_ptr || !o_ptr->k_idx || !supply_entry_matches_current_filter(i))
                continue;

            describe_inventory_menu_entry(item, o_name, sizeof(o_name));
            o_name[lim] = '\0';

            out_index[k] = item;
            out_color[k] = weapon_glows(o_ptr)
                ? object_display_color(o_ptr, TERM_L_BLUE)
                : object_display_color(o_ptr,
                    tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);
            SDL_strlcpy(out_desc[k], o_name, sizeof(out_desc[0]));

            l = strlen(out_desc[k]) + 5;
            if (show_weights)
                l += 9;
            if (l > len)
                len = l;

            k++;
        }
    }

    for (i = 0; i < INVEN_PACK && k < ENHANCED_MAX_LIST; i++)
    {
        o_ptr = &inventory[i];

        /* Is this item acceptable? */
        if (!item_tester_okay(o_ptr))
            continue;

        /* Describe the object */
        describe_inventory_menu_entry(i, o_name, sizeof(o_name));

        /* Hack -- enforce max length */
        o_name[lim] = '\0';

        /* Save the index */
        out_index[k] = i;

        /* Get inventory color */
        if (weapon_glows(o_ptr))
            out_color[k] = object_display_color(o_ptr, TERM_L_BLUE);
        else
            out_color[k] = object_display_color(o_ptr, tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

        /* Save the object description */
        SDL_strlcpy(out_desc[k], o_name, sizeof(out_desc[0]));

        /* Find the predicted "line length" */
        l = strlen(out_desc[k]) + 5;

        /* Be sure to account for the weight */
        if (show_weights)
            l += 9;

        /* Maintain the maximum length */
        if (l > len)
            len = l;

        /* Advance to next "line" */
        k++;
    }

    if (inventory_menu_include_equip)
    {
        for (i = INVEN_WIELD; i < INVEN_TOTAL && k < ENHANCED_MAX_LIST; i++)
        {
            o_ptr = &inventory[i];

            if (!get_item_okay(i))
                continue;

            describe_inventory_menu_entry(i, o_name, sizeof(o_name));
            o_name[lim] = '\0';

            out_index[k] = i;
            out_color[k] = o_ptr->k_idx
                ? (weapon_glows(o_ptr)
                    ? object_display_color(o_ptr, TERM_L_BLUE)
                    : object_display_color(o_ptr,
                        tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]))
                : TERM_L_DARK;
            SDL_strlcpy(out_desc[k], o_name, sizeof(out_desc[0]));

            l = strlen(out_desc[k]) + 5;
            if (show_weights)
                l += 9;
            if (l > len)
                len = l;

            k++;
        }
    }

    /* Find the column to start in */
    col = menu_center_col_for_len(term_wid, len);

    int scroll_offset = (p_ptr && p_ptr->get_item_mode != 0)
        ? inventory_menu_scroll_offset
        : 0;
    scroll_offset = inventory_menu_clamp_scroll(scroll_offset, k, display_rows);
    if (p_ptr && p_ptr->get_item_mode != 0)
        inventory_menu_scroll_offset = scroll_offset;

    if (display_rows <= 0)
    {
        story_font_term_pop(&story_state);
        return;
    }

    int visible_count = k - scroll_offset;
    if (visible_count > display_rows)
        visible_count = display_rows;
    if (visible_count < 0)
        visible_count = 0;

    /* Output each entry */
    for (j = 0; j < visible_count; j++)
    {
        int entry = scroll_offset + j;
        int idx = out_index[entry];
        bool is_supply_summary = inventory_item_is_supply_summary(idx);
        bool is_supply_entry = inventory_item_is_supply_entry(idx);
        object_type* cur_obj = is_supply_entry ? supplies_entry_at(idx - SUPPLIES_INDEX)
            : (is_supply_summary ? NULL : &inventory[idx]);
        object_type supply_icon;
        object_type* display_obj = is_supply_summary
            ? prepare_supply_icon_object(&supply_icon)
            : cur_obj;
        if (use_story_font)
        {
            char weight_buf[16];
            cptr weight_ptr = NULL;
            if (show_weights)
            {
                int wgt = is_supply_summary ? supplies_limit_weight()
                    : (cur_obj->weight * cur_obj->number);
                strnfmt(weight_buf, sizeof(weight_buf), "%2d.%1d lb", wgt / 10, wgt % 10);
                weight_ptr = weight_buf;
            }

            char label_buf[8];
            if (is_supply_summary)
            {
                char label = supplies_label_char();
                int slot = supplies_virtual_slot();
                if (!label && slot >= 0)
                    label = inventory_visible_label_for_item(slot);
                if (!label)
                    label = 'a';
                strnfmt(label_buf, sizeof(label_buf), "(%c)", label);
            }
            else
            {
                strnfmt(label_buf, sizeof(label_buf), "(%c)",
                    inventory_visible_label_for_item(idx));
            }

            story_render_inventory_entry(j + 1, col, label_col,
                out_desc[entry], out_color[entry],
                show_weights, weight_ptr, out_color[entry], label_buf, TERM_WHITE,
                display_obj, false, story_term_w);
            if (inventory_choice_debug_logging)
            {
                log_debug("show_inven: row=%d idx=%d label='%s' supply_summary=%d "
                    "supply_entry=%d desc='%s'",
                    j + 1, idx, label_buf, is_supply_summary ? 1 : 0,
                    is_supply_entry ? 1 : 0, out_desc[entry]);
            }
            continue;
        }

        prt("", j + 1, col);

        /* Draw tile if in graphics mode. */
        int text_col = col;
        if (display_obj && display_obj->k_idx)
        {
            text_col = draw_item_tile(col, j + 1, display_obj);
        }

        c_put_str(out_color[entry], out_desc[entry], j + 1, text_col);

        if (show_weights)
        {
            int wgt;
            if (is_supply_summary)
                wgt = supplies_limit_weight();
            else
                wgt = cur_obj->weight * cur_obj->number;
            sprintf(tmp_val, "%3d.%1d lb", wgt / 10, wgt % 10);
            c_put_str(out_color[entry], tmp_val, j + 1, weight_col);
        }

        /* Print the item letter at the end */
        if (is_supply_summary)
        {
            char label = supplies_label_char();
            int slot = supplies_virtual_slot();
            if (!label && slot >= 0)
                label = inventory_visible_label_for_item(slot);
            if (!label)
                label = 'a';
            sprintf(tmp_val, " (%c)", label);
        }
        else
        {
            sprintf(tmp_val, " (%c)", inventory_visible_label_for_item(idx));
        }

        if (inventory_choice_debug_logging)
        {
            log_debug("show_inven: row=%d idx=%d label='%s' supply_summary=%d "
                "supply_entry=%d desc='%s'",
                j + 1, idx, tmp_val, is_supply_summary ? 1 : 0,
                is_supply_entry ? 1 : 0, out_desc[entry]);
        }

        put_str(tmp_val, j + 1, label_col);
    }

    /* Make a "shadow" below the list (only if needed) */
    if (j && (j < term_hgt - 1))
    {
        if (use_story_font)
            Term_erase(col, j + 1, 255);
        else
            prt("", j + 1, col);
    }

    story_font_term_pop(&story_state);
}

/*
 * Display the supply cache in a sub-window.
 */
void display_supplies(void)
{
    int term_wid = menu_term_width();
    int term_hgt = menu_term_height();
    int count = supplies_entry_count();
    int rows = term_hgt - 1;
    int max_rows;
    int weight_col = menu_weight_col_for_width(term_wid);
    bool display_weights = show_weights && term_wid >= 42;
    char header[80];

    if (rows < 0)
        rows = 0;
    max_rows = MIN(rows, 26);
    if (count > max_rows && max_rows > 0 && max_rows == rows)
        max_rows--;

    for (int row = 0; row < term_hgt; row++)
        Term_erase(0, row, 255);

    strnfmt(header, sizeof(header), "Supply %d.%1d/%d.%1d lb",
        supplies_limit_weight() / 10, supplies_limit_weight() % 10,
        supplies_current_weight_cap() / 10,
        supplies_current_weight_cap() % 10);
    Term_putstr(0, 0, term_wid, TERM_L_BLUE, header);

    if (count <= 0)
    {
        if (term_hgt > 1)
            Term_putstr(0, 1, term_wid, TERM_SLATE, "(none)");
        return;
    }

    for (int i = 0; i < count && i < max_rows; i++)
    {
        object_type* o_ptr = supplies_entry_at(i);
        char o_name[80];
        char label[4];
        byte attr;
        int row = i + 1;
        int text_col = 4;
        int desc_limit;

        if (!o_ptr || !o_ptr->k_idx)
            continue;

        strnfmt(label, sizeof(label), "%c)", supplies_label_for_entry(i));
        Term_putstr(0, row, 3, TERM_WHITE, label);

        text_col = draw_item_tile(text_col, row, o_ptr);
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
        attr = weapon_glows(o_ptr)
            ? object_display_color(o_ptr, TERM_L_BLUE)
            : object_display_color(o_ptr,
                tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

        desc_limit = display_weights ? weight_col - text_col - 1
                                      : term_wid - text_col;
        if (desc_limit < 1)
            desc_limit = 1;
        if (desc_limit < (int)sizeof(o_name))
            o_name[desc_limit] = '\0';
        Term_putstr(text_col, row, desc_limit, attr, o_name);

        if (display_weights && o_ptr->weight)
        {
            int wgt = o_ptr->weight * MAX(o_ptr->number, 1);
            char weight_buf[16];

            strnfmt(weight_buf, sizeof(weight_buf), "%2d.%1d lb",
                wgt / 10, wgt % 10);
            Term_putstr(weight_col, row, term_wid - weight_col,
                attr, weight_buf);
        }
    }

    if (count > max_rows && term_hgt > 1)
    {
        char more[40];
        int row = term_hgt - 1;

        strnfmt(more, sizeof(more), "... %d more", count - max_rows);
        Term_erase(0, row, 255);
        Term_putstr(0, row, term_wid, TERM_SLATE, more);
    }
}

/*
 * Display the equipment.
 */
void show_equip(void)
{
    int i, j, k, l;
    int col, len, lim;
    int clear_col;
    int term_wid = menu_term_width();
    int term_hgt = (Term && Term->hgt > 0) ? Term->hgt : 24;
    int weight_col = menu_weight_col_for_width(term_wid);
    int label_col = menu_label_col_for_width(term_wid, show_weights);

    object_type* o_ptr;

    char tmp_val[80];

    char o_name[80];

    int out_index[24];
    byte out_color[24];
    char out_desc[24][80];

    int armour_weight = 0;

    bool use_story_font = story_equipment_enabled();
    story_font_term_state story_state;
    int story_term_w = 0;
    story_equipment_list_active = use_story_font;
    story_font_term_push(use_story_font, false, &story_state);
    if (use_story_font) {
        int story_term_h = 0;
        Term_get_size(&story_term_w, &story_term_h);
    }
    else
    {
        log_debug("show_equip: Story font DISABLED, using mono font");
    }

    /* Default length */
    len = 29;

    /* Maximum space allowed for descriptions */
    lim = term_wid - 3;

    if (lim < 0)
        lim = 0;

    /* Require space for labels */
    lim -= (14 + 2);

    /* Require space for weight (if needed) */
    if (show_weights)
        lim -= 9;

    if (lim < 0)
        lim = 0;

    /* Scan the equipment list */
    for (k = 0, i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        bool is_empty = !o_ptr->k_idx;

        /* Is this item acceptable? */
        if (!item_tester_okay(o_ptr))
            continue;

        if (is_empty)
        {
            SDL_strlcpy(o_name, describe_empty_slot(i), sizeof(o_name));
            out_color[k] = TERM_L_DARK;
        }
        else
        {
            /* Description */
            object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
            out_color[k] = object_display_color(o_ptr, tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);
        }

        /* Truncate the description */
        o_name[lim] = 0;

        /* Save the index */
        out_index[k] = i;

        /* Save the description */
        SDL_strlcpy(out_desc[k], o_name, sizeof(out_desc[0]));

        /* Extract the maximal length (see below) */
        l = strlen(out_desc[k]) + (2 + 3);

        /* Increase length for labels */
        l += (12 + 2);

        /* Increase length for weight (if needed) */
        if (show_weights)
            l += 9;

        /* Maintain the max-length */
        if (l > len)
            len = l;

        /* Advance the entry */
        k++;
    }

    /* Hack -- Find a column to start in */
    col = menu_center_col_for_len(term_wid, len);
    clear_col = menu_overlay_clear_col(col);

    /* Output each entry */
    for (j = 0; j < k; j++)
    {
        /* Get the index */
        i = out_index[j];

        /* Get the item */
        o_ptr = &inventory[i];

        log_trace("show_equip: Rendering row %d, slot %d, desc='%s'", j + 1, i, out_desc[j]);

        char prefix_buf[32];
        strnfmt(prefix_buf, sizeof(prefix_buf), "%-12s: ", mention_use(i));

        const char* desc_ptr = out_desc[j];

        char label_buf[8];
        strnfmt(label_buf, sizeof(label_buf), " (%c)", index_to_label(i));
        char weight_buf[16];
        cptr weight_ptr = NULL;
        byte weight_attr = out_color[j];
        if (show_weights && o_ptr->weight)
        {
            int wgt = o_ptr->weight * o_ptr->number;
            sprintf(weight_buf, "%2d.%1d lb", wgt / 10, wgt % 10);
            weight_ptr = weight_buf;

            if ((i >= INVEN_BODY) && (i <= INVEN_FEET))
            {
                weight_attr = TERM_SLATE;
                armour_weight += wgt;
            }
        }

        if (use_story_font)
        {
            story_render_equipment_entry(j + 1, col, i, prefix_buf, TERM_WHITE,
                desc_ptr, out_color[j], show_weights, weight_ptr, weight_attr,
                label_buf, TERM_WHITE, o_ptr->k_idx ? o_ptr : NULL, false, story_term_w);
            continue;
        }

        /* Clear the line */
        Term_erase(clear_col, j + 1, 255);

        /* Mention the use */
        log_trace("show_equip: Row %d - put_str prefix '%s'", j + 1, prefix_buf);
        put_str(prefix_buf, j + 1, col);

        /* Draw tile if in graphics mode and item exists */
        int text_col = col + 12 + 2;
        if (o_ptr->k_idx)
        {
            text_col = draw_item_tile(col + 12 + 2, j + 1, o_ptr);
        }

        /* Display the entry itself */
        log_trace("show_equip: Row %d - c_put_str desc '%s' at col %d", j + 1, out_desc[j], text_col);
        c_put_str(out_color[j], out_desc[j], j + 1, text_col);

        /* Display the weight if needed */
        if (show_weights && o_ptr->weight)
        {
            if (weight_attr == TERM_SLATE)
                c_put_str(TERM_SLATE, weight_buf, j + 1, weight_col);
            else
                c_put_str(out_color[j], weight_buf, j + 1, weight_col);
        }

        if (i == INVEN_QUIVER2)
        {
            int note_col = col + 12 + 2 + (int)strlen(out_desc[j]);
            c_put_str(TERM_L_DARK, " (keeps passive bonuses)", j + 1, note_col);
        }

        /* Print the item letter at the end */
        log_trace("show_equip: Row %d - put_str label '%s' at col %d", j + 1, label_buf, label_col);
        put_str(label_buf, j + 1, label_col);
    }
    
    log_trace("show_equip: Finished rendering all %d entries", k);

    /* Put in the total weight */
    if (armour_weight)
    {
        int divider_row;
        int text_row;

        equipment_weight_layout_rows(1, k, term_hgt, &divider_row, &text_row);
        if ((j + 1) < term_hgt)
            Term_erase(clear_col, j + 1, 255);
        if ((j + 2) < term_hgt)
            Term_erase(clear_col, j + 2, 255);

        if (use_story_font)
        {
            strnfmt(tmp_val, sizeof(tmp_val), "armour: %3d.%1d lb",
                armour_weight / 10, armour_weight % 10);
            if (divider_row >= 0)
                story_print_text_grid(divider_row, weight_col, 8, TERM_L_DARK,
                    "--------");
            if (text_row >= 0)
                story_print_text_grid(text_row, MAX(0, weight_col - 8), 16,
                    TERM_SLATE, tmp_val);
        }
        else
        {
            strnfmt(tmp_val, sizeof(tmp_val), "armour: %3d.%1d lb",
                armour_weight / 10,
                armour_weight % 10);
            if (divider_row >= 0)
                c_put_str(TERM_L_DARK, "--------", divider_row, weight_col);
            if (text_row >= 0)
                c_put_str(TERM_SLATE, tmp_val, text_row,
                    MAX(0, weight_col - 8));
        }
    }
    else if (j && (j < term_hgt - 1))
    {
        Term_erase(clear_col, j + 1, 255);
    }

    story_font_term_pop(&story_state);
}

/*
 * Display a list of the items on the floor at the given location.
 */
void show_floor(const int* floor_list, int floor_num)
{
    int i, j, k, l;
    int col, len, lim;
    int term_wid = menu_term_width();
    int term_hgt = (Term && Term->hgt > 0) ? Term->hgt : 24;
    int weight_col = menu_weight_col_for_width(term_wid);
    int label_col = menu_label_col_for_width(term_wid, show_weights);

    object_type* o_ptr;

    char o_name[80];

    char tmp_val[80];

    int out_index[MAX_FLOOR_STACK];
    byte out_color[MAX_FLOOR_STACK];
    char out_desc[MAX_FLOOR_STACK][80];

    /* Default length */
    len = 29;

    /* Maximum space allowed for descriptions */
    lim = term_wid - 3;
    if (lim < 0)
        lim = 0;

    /* Require space for weight (if needed) */
    if (show_weights && lim > (weight_col - 1))
        lim = weight_col - 1;
    if (lim < 0)
        lim = 0;

    /* Display the inventory */
    for (k = 0, i = 0; i < floor_num; i++)
    {
        o_ptr = &o_list[floor_list[i]];

        /* Is this item acceptable? */
        if (!item_tester_okay(o_ptr))
            continue;

        /* Describe the object */
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

        /* Hack -- enforce max length */
        o_name[lim] = '\0';

        /* Save the index */
        out_index[k] = i;

        /* Get inventory color */
        out_color[k] = object_display_color(o_ptr, tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

        /* Save the object description */
        SDL_strlcpy(out_desc[k], o_name, sizeof(out_desc[0]));

        /* Find the predicted "line length" */
        l = strlen(out_desc[k]) + 5;

        /* Be sure to account for the weight */
        if (show_weights)
            l += 9;

        /* Maintain the maximum length */
        if (l > len)
            len = l;

        /* Advance to next "line" */
        k++;
    }

    /* Find the column to start in */
    col = menu_center_col_for_len(term_wid, len);

    /* Output each entry */
    for (j = 0; j < k; j++)
    {
        /* Get the index */
        i = floor_list[out_index[j]];

        /* Get the item */
        o_ptr = &o_list[i];

        /* Clear the line */
        prt("", j + 1, col);

        /* Draw tile if in graphics mode */
        int text_col = col;
        if (o_ptr->k_idx)
        {
            text_col = draw_item_tile(col, j + 1, o_ptr);
        }

        /* Display the entry itself */
        c_put_str(out_color[j], out_desc[j], j + 1, text_col);

        /* Display the weight if needed */
        if (show_weights)
        {
            int wgt = o_ptr->weight * o_ptr->number;
            sprintf(tmp_val, "%3d.%1d lb", wgt / 10, wgt % 10);
            c_put_str(out_color[j], tmp_val, j + 1, weight_col);
        }

        /* Print the item letter at the end */
        if (steamdeck_controls_active())
            SDL_strlcpy(tmp_val, "    ", sizeof(tmp_val));
        else
            sprintf(tmp_val, " (%c)", index_to_label(out_index[j]));
        put_str(tmp_val, j + 1, label_col);
    }

    /* Make a "shadow" below the list (only if needed) */
    if (j && (j < term_hgt - 1))
        prt("", j + 1, col);
}

/*
 * Flip "inven" and "equip" in any sub-windows
 */
void toggle_inven_equip(void)
{
    int j;

    /* Scan windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        /* Unused */
        if (!angband_term[j])
            continue;

        /* Flip inven to equip */
        if (op_ptr->window_flag[j] & (PW_INVEN))
        {
            /* Flip flags */
            op_ptr->window_flag[j] &= ~(PW_INVEN);
            op_ptr->window_flag[j] |= (PW_EQUIP);

            /* Window stuff */
            p_ptr->window |= (PW_EQUIP);
        }

        /* Flip inven to equip */
        else if (op_ptr->window_flag[j] & (PW_EQUIP))
        {
            /* Flip flags */
            op_ptr->window_flag[j] &= ~(PW_EQUIP);
            op_ptr->window_flag[j] |= (PW_INVEN);

            /* Window stuff */
            p_ptr->window |= (PW_INVEN);
        }
    }
}

/*
 * Verify the choice of an item.
 *
 * The item can be negative to mean "item on floor".
 */
