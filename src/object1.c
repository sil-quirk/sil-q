/* File: object1.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "sdl-config.h"
#include "supplies.h"
#include "item_set.h"
#include "cJSON.h"
#include <ctype.h>
#include <stdlib.h>
#define ENHANCED_MAX_LIST 80
#include <stddef.h>
static bool inventory_menu_include_equip = false;
static bool inventory_menu_expand_supplies = false;
static bool inventory_choice_debug_logging = false;
static int inventory_menu_scroll_offset = 0;

static bool story_inventory_list_active = false;
static bool story_equipment_list_active = false;

static bool supplies_visible_for_current_filter(void);
static bool inventory_menu_uses_visible_labels(void);
static bool inventory_menu_uses_expanded_supplies(void);
static int inventory_visible_supply_count(void);
static int inventory_visible_supply_item_at(int ordinal);
static int inventory_visible_supply_ordinal(int item);
static int inventory_visible_inven_item_at(int ordinal);
static int inventory_visible_inven_ordinal(int item);
static char inventory_visible_label_for_item(int item);
static bool inventory_item_is_supply_summary(int item);
static bool inventory_item_is_supply_entry(int item);
static bool inventory_item_is_equipment(int item);
static object_type* inventory_item_to_object_ptr(int item);
static bool inventory_item_uses_inven_channel(int item);
static void describe_inventory_menu_entry(int item, char* buf, size_t len);
static bool get_item_okay(int item);
static bool item_prompt_is_replace(cptr pmt);
static void log_inventory_selector_state(cptr stage, cptr pmt,
    const int vis_inven[], int vis_inven_cnt);
static void format_supply_summary(char* buf, size_t len);
static void equipment_weight_layout_rows(int first_row, int item_count,
    int term_hgt, int* divider_row, int* text_row);

bool inventory_menu_set_expand_supplies(bool enabled)
{
    bool old = inventory_menu_expand_supplies;
    inventory_menu_expand_supplies = enabled;
    return old;
}

static void inventory_prompt_label(int binding, const char* fallback, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

static bool inventory_menu_same_button_cycle_enabled(void)
{
    if (!portable_controls_active())
        return false;

    if (!steamdeck_controls_active())
        return false;

    return get_sdl_steamdeck_inv_equip_same_button_cycle();
}

static void story_print_equipment_prefix(int row, int col, byte attr, cptr prefix)
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

static bool menu_prompt_drop_suffix_if_wrapped(char* prompt, cptr suffix,
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

static bool death_spectator_allow_menu_action(void)
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

static void flavor_assign_fixed(void)
{
    int i, j;

    for (i = 0; i < z_info->flavor_max; i++)
    {
        flavor_type* flavor_ptr = &flavor_info[i];

        /* Skip random flavors */
        if (flavor_ptr->sval == SV_UNKNOWN)
            continue;

        for (j = 0; j < z_info->k_max; j++)
        {
            /* Skip other objects */
            if ((k_info[j].tval == flavor_ptr->tval)
                && (k_info[j].sval == flavor_ptr->sval))
            {
                /* Store the flavor index */
                k_info[j].flavor = i;
            }
        }
    }
}

static void flavor_assign_random(byte tval)
{
    int i, j;
    int flavor_count = 0;
    int choice;

    /* Count the random flavors for the given tval */
    for (i = 0; i < z_info->flavor_max; i++)
    {
        if ((flavor_info[i].tval == tval)
            && (flavor_info[i].sval == SV_UNKNOWN))
        {
            flavor_count++;
        }
    }

    for (i = 0; i < z_info->k_max; i++)
    {
        /* Skip other object types */
        if (k_info[i].tval != tval)
            continue;

        /* Skip objects that already are flavored */
        if (k_info[i].flavor != 0)
            continue;

        /* HACK - Ordinary food is "boring" */
        if ((tval == TV_FOOD) && (k_info[i].sval >= SV_FOOD_MIN_FOOD))
            continue;

        if (!flavor_count)
            quit(format("Not enough flavors for tval %d.", tval));

        /* Select a flavor */
        choice = rand_int(flavor_count);

        /* Find and store the flavor */
        for (j = 0; j < z_info->flavor_max; j++)
        {
            /* Skip other tvals */
            if (flavor_info[j].tval != tval)
                continue;

            /* Skip assigned svals */
            if (flavor_info[j].sval != SV_UNKNOWN)
                continue;

            if (choice == 0)
            {
                /* Store the flavor index */
                k_info[i].flavor = j;

                /* Mark the flavor as used */
                flavor_info[j].sval = k_info[i].sval;

                /* One less flavor to choose from */
                flavor_count--;

                break;
            }

            choice--;
        }
    }
}

// Is the current day between Easter Sunday?
// if so, herbs become easter eggs
bool easter_time(void)
{
    /* Stubbed out (original implementation used time functions). */
    return false;
}

/*
 * Prepare the "variable" part of the "k_info" array.
 *
 * The "color"/"metal"/"type" of an item is its "flavor".
 * For the most part, flavors are assigned randomly each game.
 *
 * Initialize descriptions for the "colored" objects, including:
 * Rings, Amulets, Staffs, Horns, Food, Potions, Scrolls.
 *
 * The first 4 entries for potions are fixed (Miruvor, unused, Orcish Liquor,
 * unused).
 *
 * Hack -- make sure everything stays the same for each saved game
 * This is accomplished by the use of a saved "random seed".
 * Since no other functions are called while the special
 * seed is in effect, so this function is pretty "safe".
 */
void flavor_init(void)
{
    int i;

    u64b saved_state = Rand_state_export();
    Rand_state_import(seed_flavor);

    flavor_assign_fixed();

    flavor_assign_random(TV_RING);
    flavor_assign_random(TV_AMULET);
    flavor_assign_random(TV_STAFF);
    flavor_assign_random(TV_GEM);
    flavor_assign_random(TV_HORN);
    flavor_assign_random(TV_FOOD);
    flavor_assign_random(TV_POTION);

    Rand_state_import(saved_state);

    /* Analyze every object */
    for (i = 1; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        /*Skip "empty" objects*/
        if (!k_ptr->name)
            continue;

        /*No flavor yields aware*/
        if (!k_ptr->flavor || (k_ptr->tval == TV_ARROW))
            k_ptr->aware = true;

        // Easter Eggs
        if (easter_time() && (k_ptr->tval == TV_FOOD) && k_ptr->flavor)
        {
            k_ptr->flavor += 20;
        }
    }
}

static bool object_is_unidentified_for_display(const object_type* o_ptr);

/*
 * Get the display color for an object text, applying artifact shade if identified
 * This is used for TEXT color in inventory/equipment displays
 * Uses MAKE_EXTENDED_COLOR to create proper shaded colors
 */
byte object_display_color(const object_type* o_ptr, byte base_color)
{
    if (!o_ptr || !o_ptr->k_idx)
        return base_color;

    if (unidentified_items_slate && object_is_unidentified_for_display(o_ptr))
        return TERM_SLATE;

    byte color_to_use = base_color;
    
    /* Bows are light umber by default, but allow artifact coloring to override */
    if (o_ptr->tval == TV_BOW)
    {
        color_to_use = TERM_L_UMBER;
    }

    /* Check for artifact-specific color (works in both modes) */
    if (o_ptr->name1 && a_info[o_ptr->name1].d_attr)
    {
        color_to_use = a_info[o_ptr->name1].d_attr;
    }
    
    /* Apply special handling when artifact_unique_color option is enabled */
    if (artifact_unique_color)
    {
        /* Identified artifacts are yellow (including artifact rings) */
        if (artefact_p(o_ptr) && object_known_p(o_ptr))
        {
            return TERM_YELLOW;
        }
        
        /* Non-artifact rings are orange when option is enabled */
        if (o_ptr->tval == TV_RING)
        {
            return TERM_ORANGE;
        }
    }
    else
    {
        /* Default mode: use shade 3 for identified artifacts */
        if (artefact_p(o_ptr) && object_known_p(o_ptr))
        {
            return MAKE_EXTENDED_COLOR(color_to_use, 3);
        }
    }

    if (unidentified_items_slate && color_to_use == TERM_SLATE)
        return TERM_WHITE;
    
    return color_to_use;
}

static void load_object_text_colors_json(void)
{
    char path[1024];
    SDL_IOStream* f = NULL;
    char* buffer = NULL;
    cJSON* root = NULL;
    int loaded_entries = 0;

    if (!ANGBAND_DIR_PREF || !ANGBAND_DIR_PREF[0])
    {
        log_warn("object text colors: ANGBAND_DIR_PREF is not set");
        return;
    }

    if (!path_build(path, sizeof(path), ANGBAND_DIR_PREF, "object_text_colors.json"))
    {
        log_warn("object text colors: unable to build config path");
        return;
    }

    f = sdl_fopen(path, "rb");
    if (!f)
    {
        log_warn("object text colors: config not found at '%s'", path);
        return;
    }

    Sint64 file_size = SDL_GetIOSize(f);
    if (file_size < 0 || file_size > 1024 * 1024)
    {
        log_warn("object text colors: invalid file size for '%s'", path);
        sdl_fclose(f);
        return;
    }

    buffer = mem_alloc_array((size_t)file_size + 1, char);
    if (!buffer)
    {
        log_error("object text colors: out of memory");
        sdl_fclose(f);
        return;
    }

    size_t length = (size_t)file_size;
    size_t read = SDL_ReadIO(f, buffer, length);
    buffer[read] = '\0';
    sdl_fclose(f);
    f = NULL;

    root = cJSON_Parse(buffer);
    mem_free(buffer);
    buffer = NULL;

    if (!root)
    {
        log_warn("object text colors: failed to parse '%s'", path);
        return;
    }

    cJSON* default_attr = cJSON_GetObjectItemCaseSensitive(root, "defaultAttr");
    if (cJSON_IsNumber(default_attr))
    {
        int attr = default_attr->valueint;
        if (attr >= 0 && attr <= 255)
        {
            for (int i = 0; i < (int)N_ELEMENTS(tval_to_attr); i++)
                tval_to_attr[i] = (byte)attr;
        }
    }

    cJSON* entries = cJSON_GetObjectItemCaseSensitive(root, "entries");
    if (!cJSON_IsArray(entries))
    {
        log_warn("object text colors: missing 'entries' array in '%s'", path);
        cJSON_Delete(root);
        return;
    }

    cJSON* entry = NULL;
    cJSON_ArrayForEach(entry, entries)
    {
        cJSON* tval = cJSON_GetObjectItemCaseSensitive(entry, "tval");
        cJSON* attr = cJSON_GetObjectItemCaseSensitive(entry, "attr");

        if (!cJSON_IsNumber(tval) || !cJSON_IsNumber(attr))
            continue;

        int tval_value = tval->valueint;
        int attr_value = attr->valueint;

        if (tval_value < 0 || tval_value >= (int)N_ELEMENTS(tval_to_attr))
            continue;
        if (attr_value < 0 || attr_value > 255)
            continue;

        tval_to_attr[tval_value] = (byte)attr_value;
        loaded_entries++;
    }

    cJSON_Delete(root);
    log_debug("object text colors: loaded %d entries from '%s'", loaded_entries, path);
}

/*
 * Reset the "visual" lists
 *
 * This involves resetting various things to their "default" state.
 *
 * If the "prefs" flag is true, then we will also load the appropriate
 * "user pref file" based on the current setting of the "use_graphics"
 * flag.  This is useful for switching "graphics" on/off.
 *
 * The features, objects, and monsters, should all be encoded in the
 * relevant "font.pref" and/or "graf.prf" files.  XXX XXX XXX
 *
 * The "prefs" parameter is no longer meaningful.  XXX XXX XXX
 */
void reset_visuals(bool unused)
{
    int i;

    /* Unused parameter */
    (void)unused;

    /* Extract default attr/char code for features */
    for (i = 0; i < z_info->f_max; i++)
    {
        feature_type* f_ptr = &f_info[i];

        /* Only reset if no tile was specified in data file (T: line) */
        if (!(f_ptr->x_attr & 0x80))
        {
            f_ptr->x_attr = f_ptr->d_attr;
            f_ptr->x_char = f_ptr->d_char;
        }
    }

    /* Extract default attr/char code for objects */
    for (i = 0; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        /* Only reset if no tile was specified in data file (T: line) */
        if (!(k_ptr->x_attr & 0x80))
        {
            k_ptr->x_attr = k_ptr->d_attr;
            k_ptr->x_char = k_ptr->d_char;
        }
    }

    /* Extract default attr/char code for monsters */
    for (i = 0; i < z_info->r_max; i++)
    {
        monster_race* r_ptr = &r_info[i];

        /* Only reset if no tile was specified in data file (T: line) */
        if (!(r_ptr->x_attr & 0x80))
        {
            r_ptr->x_attr = r_ptr->d_attr;
            r_ptr->x_char = r_ptr->d_char;
        }
    }

    /* Extract default attr/char code for flavors */
    for (i = 0; i < z_info->flavor_max; i++)
    {
        flavor_type* flavor_ptr = &flavor_info[i];

        /* Only reset if no tile was specified in data file (T: line) */
        if (!(flavor_ptr->x_attr & 0x80))
        {
            flavor_ptr->x_attr = flavor_ptr->d_attr;
            flavor_ptr->x_char = flavor_ptr->d_char;
        }
    }

    /* Extract attr/chars for inventory objects (by tval) */
    for (i = 0; i < 128; i++)
    {
        /* Default to 'light dark' */
        tval_to_attr[i] = TERM_L_DARK;
    }

    /* Graphic symbols */
    if (use_graphics)
    {
        /* Process "graf.prf" */
        process_pref_file("graf.prf");
    }

    /* Normal symbols */
    else
    {
        /* Process "font.prf" */
        process_pref_file("font.prf");
    }

    /* Shared object list text colors now come from JSON, not E: pref entries. */
    load_object_text_colors_json();
}

/*
 * Modes of object_flags_aux()
 */
#define OBJECT_FLAGS_FULL 1 /* Full info */
#define OBJECT_FLAGS_KNOWN 2 /* Only flags known to the player */

/*
 * Obtain the "flags" for an item (extended version with f4)
 */
static void object_flags_aux(
    int mode, const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3, u32b* f4)
{
    object_kind* k_ptr;

    if (mode == OBJECT_FLAGS_KNOWN)
    {
        /* Clear */
        (*f1) = (*f2) = (*f3) = (*f4) = 0L;

        /* Must be identified (or being listed in object knowledge) */
        if (!object_known_p(o_ptr) && !(o_ptr->ident & IDENT_SPOIL))
            return;
    }

    k_ptr = &k_info[o_ptr->k_idx];

    /* Base object */
    (*f1) = k_ptr->flags1;
    (*f2) = k_ptr->flags2;
    (*f3) = k_ptr->flags3;
    (*f4) = k_ptr->flags4;

    /* Artefact flags add to the base object (never replace it) */
    if ((mode == OBJECT_FLAGS_FULL) || (mode == OBJECT_FLAGS_KNOWN))
    {
        /* Artefact */
        if (o_ptr->name1)
        {
            artefact_type* a_ptr = &a_info[o_ptr->name1];

            (*f1) |= a_ptr->flags1;
            (*f2) |= a_ptr->flags2;
            (*f3) |= a_ptr->flags3;
            (*f4) |= a_ptr->flags4;
        }
    }

    /* Ego-items (prefix + suffix) */
    {
        byte ego_prefix = object_ego_prefix(o_ptr);
        if (ego_prefix)
        {
            ego_item_type* e_ptr = &e_info[ego_prefix];
            (*f1) |= e_ptr->flags1;
            (*f2) |= e_ptr->flags2;
            (*f3) |= e_ptr->flags3;
            (*f4) |= e_ptr->flags4;
        }

        byte ego_suffix = object_ego_suffix(o_ptr);
        if (ego_suffix)
        {
            ego_item_type* e_ptr = &e_info[ego_suffix];
            (*f1) |= e_ptr->flags1;
            (*f2) |= e_ptr->flags2;
            (*f3) |= e_ptr->flags3;
            (*f4) |= e_ptr->flags4;
        }
    }

    if (o_ptr->ident & IDENT_UNCURSED)
    {
        (*f3) &= ~(TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE);
    }
}

/*
 * Obtain the "flags" for an item (legacy 3-flag version)
 */
void object_flags(const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3)
{
    u32b dummy_f4;
    object_flags_aux(OBJECT_FLAGS_FULL, o_ptr, f1, f2, f3, &dummy_f4);
}

/*
 * Obtain the "flags" for an item (extended version with f4)
 */
void object_flags4(const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3, u32b* f4)
{
    object_flags_aux(OBJECT_FLAGS_FULL, o_ptr, f1, f2, f3, f4);
}

/*
 * Obtain the "flags" for an item which are known to the player
 */
void object_flags_known(const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3)
{
    u32b dummy_f4;
    object_flags_aux(OBJECT_FLAGS_KNOWN, o_ptr, f1, f2, f3, &dummy_f4);
}

/*
 * Obtain the "flags" for an item which are known to the player (extended)
 */
void object_flags_known4(const object_type* o_ptr, u32b* f1, u32b* f2, u32b* f3, u32b* f4)
{
    object_flags_aux(OBJECT_FLAGS_KNOWN, o_ptr, f1, f2, f3, f4);
}

bool object_grants_ability(const object_type* o_ptr, int skilltype, int abilitynum)
{
    int i;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    for (i = 0; i < o_ptr->abilities; i++)
    {
        if ((o_ptr->skilltype[i] == skilltype)
            && (o_ptr->abilitynum[i] == abilitynum))
        {
            return true;
        }
    }

    return false;
}

/*
 * Efficient version of '(T) += sprintf((T), "%c", (C))'
 */
#define object_desc_chr_macro(T, C)                                            \
    do                                                                         \
    {                                                                          \
        /* Copy the char */                                                    \
        *(T)++ = (C);                                                          \
                                                                               \
    } while (0)

/*
 * Efficient version of '(T) += sprintf((T), "%s", (S))'
 */
#define object_desc_str_macro(T, S)                                            \
    do                                                                         \
    {                                                                          \
        cptr s = (S);                                                          \
                                                                               \
        /* Copy the string */                                                  \
        while (*s)                                                             \
            *(T)++ = *s++;                                                     \
                                                                               \
    } while (0)

/*
 * Efficient version of '(T) += sprintf((T), "%u", (N))'
 */
#define object_desc_num_macro(T, N)                                            \
    do                                                                         \
    {                                                                          \
        int n = (N);                                                           \
                                                                               \
        int p;                                                                 \
                                                                               \
        /* Find "size" of "n" */                                               \
        for (p = 1; n >= p * 10; p = p * 10) /* loop */                        \
            ;                                                                  \
                                                                               \
        /* Dump each digit */                                                  \
        while (p >= 1)                                                         \
        {                                                                      \
            /* Dump the digit */                                               \
            *(T)++ = I2D(n / p);                                               \
                                                                               \
            /* Remove the digit */                                             \
            n = n % p;                                                         \
                                                                               \
            /* Process next digit */                                           \
            p = p / 10;                                                        \
        }                                                                      \
                                                                               \
    } while (0)

/*
 * Efficient version of '(T) += sprintf((T), "%+d", (I))'
 */
#define object_desc_int_macro(T, I)                                            \
    do                                                                         \
    {                                                                          \
        int i = (I);                                                           \
                                                                               \
        /* Negative */                                                         \
        if (i < 0)                                                             \
        {                                                                      \
            /* Take the absolute value */                                      \
            i = 0 - i;                                                         \
                                                                               \
            /* Use a "minus" sign */                                           \
            *(T)++ = '-';                                                      \
        }                                                                      \
                                                                               \
        /* Positive (or zero) */                                               \
        else                                                                   \
        {                                                                      \
            /* Use a "plus" sign */                                            \
            *(T)++ = '+';                                                      \
        }                                                                      \
                                                                               \
        /* Dump the number itself */                                           \
        object_desc_num_macro(T, i);                                           \
                                                                               \
    } while (0)

/*
 * Strip an "object name" into a buffer.
 */
void strip_name(char* buf, int k_idx)
{
    char* t;

    object_kind* k_ptr = &k_info[k_idx];

    cptr str = (k_name + k_ptr->name);

    /* Skip past leading characters */
    while ((*str == ' ') || (*str == '&'))
        str++;

    /* Copy useful chars */
    for (t = buf; *str; str++)
    {
        if (*str != '~')
            *t++ = *str;
    }

    /* Terminate the new name */
    *t = '\0';
}

/*
 * Creates a description of the item "o_ptr", and stores it in "buf".
 *
 * One can choose the "verbosity" of the description, including whether
 * or not the "number" of items should be described, and how much detail
 * should be used when describing the item.
 *
 * The given "buf" should be at least 80 chars long to hold the longest
 * possible description, which can get pretty long, including inscriptions,
 * such as:
 * "no more Maces of Disruption (Defender) (+10,+10) [+5] (+3 to stealth)".

 * Note that the object description will be clipped to fit into the given
 * buffer size.
 *
 * Note the use of "object_desc_int_macro()" and "object_desc_num_macro()"
 * and "object_desc_str_macro()" and "object_desc_chr_macro()" as extremely
 * efficient, portable, versions of some common "sprintf()" commands (without
 * the bounds checking or termination writing), which allow a pointer to
 * efficiently move through a buffer while modifying it in various ways.
 *
 * Various improper uses and/or placements of "&" or "~" characters can
 * easily induce out-of-bounds memory accesses.  Some of these could be
 * easily checked for, if efficiency was not a concern.
 *
 * Note that all special items (when known) append an "Ego-Item Name", unless
 * the item is also an artefact, which should never happen.
 *
 * Note that all artefacts (when known) append an "Artefact Name", so we
 * have special processing for "Specials" (artefact Lites, Rings, Amulets).
 * The "Specials" never use "modifiers" if they are "known", since they
 * have special "descriptions", such as "The Necklace of the Dwarves".
 *
 * Special Lite's use the "k_info" base-name (Phial, Star, or Arkenstone),
 * plus the artefact name, just like any other artefact, if known.
 *
 * Special Ring's and Amulet's, if not "aware", use the same code as normal
 * rings and amulets, and if "aware", use the "k_info" base-name (Ring or
 * Amulet or Necklace).  They will NEVER "append" the "k_info" name.  But,
 * they will append the artefact name, just like any artefact, if known.
 *
 * None of the Special Rings/Amulets are "EASY_KNOW", though they could be,
 * at least, those which have no "pluses", such as the three artefact lites.
 *
 * The "pluralization" rules are extremely hackish, in fact, for efficiency,
 * we only handle things like "torch"/"torches" and "cutlass"/"cutlasses",
 * and we would not handle "box"/"boxes", or "knife"/"knives", correctly.
 * Of course, it would be easy to add rules for these forms.
 *
 * If "pref" is true then a "numeric" prefix will be pre-pended, else is is
 * assumed that a string such as "The" or "Your" will be pre-pended later.
 *
 * Modes ("pref" is true):
 *  -1 -- Chain Mail
 *   0 -- Chain Mail of Death
 *   1 -- A Cloak of Death [1,+3]
 *   2 -- An Amulet of Death [1,+3] <+2>
 *   3 -- 5 Rings of Death [1,+3] <+2> {nifty}
 *
 * Modes ("pref" is false):
 *  -1 -- Chain Mail
 *   0 -- Chain Mail of Death
 *   1 -- Cloak of Death [1,+3]
 *   2 -- Amulet of Death [1,+3] <+2>
 *   3 -- Rings of Death [1,+3] <+2> {nifty}
 */
static void object_desc_trim_spaces(char* s)
{
    if (!s) return;

    char* start = s;
    while (*start && isspace((unsigned char)*start))
        ++start;

    if (start != s)
        memmove(s, start, strlen(start) + 1);

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))
        s[--len] = '\0';
}

static void object_desc_mode4_shorten(char* buf, size_t max, const object_type* o_ptr, bool apply_rules)
{
    if (!buf) return;

    log_debug("mode4_shorten: INPUT buf='%s' max=%zu apply_rules=%d", buf, max, apply_rules);

    char source[256];
    size_t src_idx = 0;
    bool insert_space = false;

    for (size_t i = 0; buf[i] && src_idx < sizeof(source) - 1; ++i)
    {
        unsigned char c = (unsigned char)buf[i];

        if (c < 32)
        {
            if (src_idx > 0 && source[src_idx - 1] != ' ')
                insert_space = true;
            continue;
        }

        if (insert_space && c != ' ' && src_idx < sizeof(source) - 1)
            source[src_idx++] = ' ';

        insert_space = false;

        source[src_idx++] = (char)c;
    }
    source[src_idx] = '\0';

    log_debug("mode4_shorten: after cleanup source='%s'", source);

    SDL_strlcpy(buf, source, max);

    size_t len = strlen(source);
    if (!len)
    {
        log_debug("mode4_shorten: empty source, returning");
        return;
    }

    size_t stats_idx = len;
    for (size_t i = 0; i < len; ++i)
    {
        char c = source[i];
        if (c == '(' || c == '[' || c == '<' || c == '{')
        {
            stats_idx = i;
            break;
        }
    }

    log_debug("mode4_shorten: stats_idx=%zu len=%zu", stats_idx, len);

    char base[256];
    char stats[256];
    base[0] = '\0';
    stats[0] = '\0';

    if (stats_idx < len)
    {
        /* Copy only the characters before stats_idx - strnfmt was not respecting %.*s */
        if (stats_idx > 0 && stats_idx < sizeof(base))
        {
            memcpy(base, source, stats_idx);
            base[stats_idx] = '\0';
        }
        SDL_strlcpy(stats, source + stats_idx, sizeof(stats));
        log_debug("mode4_shorten: split - base='%s' stats='%s'", base, stats);
    }
    else
    {
        SDL_strlcpy(base, source, sizeof(base));
        log_debug("mode4_shorten: no stats found - base='%s'", base);
    }

    object_desc_trim_spaces(base);
    object_desc_trim_spaces(stats);
    
    log_debug("mode4_shorten: after trim - base='%s' stats='%s'", base, stats);

    /* If base name is empty after processing, use the original source instead of stats-only */
    if (!base[0])
    {
        log_debug("mode4_shorten: base is empty! source='%s'", source);
        
        /* Try to extract at least something from the original - fallback to first word */
        const char* first_word_end = source;
        while (*first_word_end && !isspace((unsigned char)*first_word_end) 
               && *first_word_end != '(' && *first_word_end != '[' 
               && *first_word_end != '<' && *first_word_end != '{')
            first_word_end++;
        
        size_t first_word_len = first_word_end - source;
        log_debug("mode4_shorten: first_word_len=%zu", first_word_len);
        
        if (first_word_len > 0 && first_word_len < sizeof(base))
        {
            memcpy(base, source, first_word_len);
            base[first_word_len] = '\0';
            object_desc_trim_spaces(base);
            log_debug("mode4_shorten: extracted first word base='%s'", base);
        }
        
        /* If still nothing, just use the whole source unchanged */
        if (!base[0])
        {
            log_debug("mode4_shorten: still empty, returning source unchanged");
            SDL_strlcpy(buf, source, max);
            return;
        }
    }

    char trailing_suffix[64];
    trailing_suffix[0] = '\0';

    if (apply_rules)
    {
        log_debug("mode4_shorten: applying rules to base='%s'", base);
        
        size_t base_len_tmp = strlen(base);
        size_t idx = base_len_tmp;

        while (idx > 0 && isspace((unsigned char)base[idx - 1]))
            --idx;

        size_t digit_start = idx;
        while (digit_start > 0 && isdigit((unsigned char)base[digit_start - 1]))
            --digit_start;

        if (digit_start < idx)
        {
            size_t copy_len = idx - digit_start;
            if (copy_len < sizeof(trailing_suffix))
            {
                memcpy(trailing_suffix, base + digit_start, copy_len);
                trailing_suffix[copy_len] = '\0';
                base[digit_start] = '\0';
                object_desc_trim_spaces(base);
                log_debug("mode4_shorten: extracted trailing_suffix='%s' base_after='%s'", trailing_suffix, base);
            }
        }
    }
    else
    {
        log_debug("mode4_shorten: skipping rules (artifact)");
    }

    if (!apply_rules)
    {
        char rebuilt_basic[256];
        rebuilt_basic[0] = '\0';
        SDL_strlcpy(rebuilt_basic, base, sizeof(rebuilt_basic));
        if (stats[0])
        {
            if (rebuilt_basic[0])
                SDL_strlcat(rebuilt_basic, " ", sizeof(rebuilt_basic));
            SDL_strlcat(rebuilt_basic, stats, sizeof(rebuilt_basic));
        }
        object_desc_trim_spaces(rebuilt_basic);
        log_debug("mode4_shorten: no rules, OUTPUT='%s'", rebuilt_basic);
        SDL_strlcpy(buf, rebuilt_basic, max);
        return;
    }

    /* Preserve ego prefix in shortened output (prefixes moved from "(Ego)" suffix to leading words). */
    char ego_prefix_label[128];
    ego_prefix_label[0] = '\0';
    if (o_ptr && object_ego_prefix(o_ptr))
    {
        byte e_idx = object_ego_prefix(o_ptr);
        if (e_idx > 0 && e_idx < z_info->e_max && e_info[e_idx].name)
        {
            const char* raw = e_name + e_info[e_idx].name;
            if (ego_name_is_prefix(raw))
            {
                size_t raw_len = strlen(raw);
                size_t copy_len = (raw_len >= 2) ? (raw_len - 2) : 0;
                if (copy_len >= sizeof(ego_prefix_label))
                    copy_len = sizeof(ego_prefix_label) - 1;
                if (copy_len > 0)
                {
                    memcpy(ego_prefix_label, raw + 1, copy_len);
                    ego_prefix_label[copy_len] = '\0';
                    object_desc_trim_spaces(ego_prefix_label);
                }
            }
        }
    }
    if (ego_prefix_label[0])
    {
        /* Only preserve the prefix when it is actually visible in the base name (avoid leaking unknown egos). */
        size_t pre_len = strlen(ego_prefix_label);
        if (strncmp(base, ego_prefix_label, pre_len) != 0
            || (base[pre_len] && !isspace((unsigned char)base[pre_len])))
        {
            ego_prefix_label[0] = '\0';
        }
    }

    log_debug("mode4_shorten: processing 'of' pattern in base='%s'", base);

    /* Determine if this item has an ego suffix enchantment (e.g., "... of Speed") */
    bool has_ego = (o_ptr && object_ego_suffix(o_ptr));
    
    char lower[256];
    size_t base_len = strlen(base);
    for (size_t i = 0; i < base_len && i < sizeof(lower) - 1; ++i)
        lower[i] = (char)tolower((unsigned char)base[i]);
    lower[base_len] = '\0';

    log_debug("mode4_shorten: lower='%s' has_ego=%d", lower, has_ego);

    /*
     * Split on " of " to separate base type from qualifier:
     * - Ego items: split on the LAST " of " (handles phrases like "Pair of Boots of Speed").
     * - Non-ego items: split only when the word before "of" is a known base type
     *   ("Ring of Frost", "Potion of Healing", etc), avoiding cases where "of"
     *   is part of the base phrase.
     */
    char* split_point = NULL;
    if (has_ego)
    {
        /* Find the LAST " of " - this should be the ego enchantment */
        for (char* search = lower; (search = strstr(search, " of ")) != NULL; ++search)
            split_point = search;
    }
    else
    {
        char* first_of = strstr(lower, " of ");
        if (first_of)
        {
            size_t word_end = (size_t)(first_of - lower);
            while (word_end > 0 && isspace((unsigned char)lower[word_end - 1]))
                --word_end;

            size_t word_start = word_end;
            while (word_start > 0 && !isspace((unsigned char)lower[word_start - 1]))
                --word_start;

            char head_word[32];
            size_t word_len = word_end - word_start;
            if (word_len > 0 && word_len < sizeof(head_word))
            {
                memcpy(head_word, lower + word_start, word_len);
                head_word[word_len] = '\0';

                static const char* split_words[] = {
                    "amulet", "amulets",
                    "gem",    "gems",
                    "herb",   "herbs",
                    "horn",   "horns",
                    "potion", "potions",
                    "ring",   "rings",
                    "staff",  "staves",
                };

                for (size_t i = 0; i < N_ELEMENTS(split_words); ++i)
                {
                    if (!strcmp(head_word, split_words[i]))
                    {
                        split_point = first_of;
                        break;
                    }
                }
            }
        }
    }

    char first_part[256];
    char second_part[256];
    first_part[0] = '\0';
    second_part[0] = '\0';

    if (split_point)
    {
        size_t index = (size_t)(split_point - lower);
        if (index < sizeof(first_part))
        {
            memcpy(first_part, base, index);
            first_part[index] = '\0';
        }
        SDL_strlcpy(second_part, base + index + 4, sizeof(second_part));
        log_debug("mode4_shorten: found ego 'of' at %zu - first='%s' second='%s'", index, first_part, second_part);
    }
    else
    {
        SDL_strlcpy(first_part, base, sizeof(first_part));
        log_debug("mode4_shorten: no ego split - first='%s'", first_part);
    }

    object_desc_trim_spaces(first_part);
    object_desc_trim_spaces(second_part);
    
    log_debug("mode4_shorten: after trim - first='%s' second='%s'", first_part, second_part);

    /* If we have a prefix ego, shorten the base type without dropping the prefix. */
    char* first_part_for_short = first_part;
    if (ego_prefix_label[0])
    {
        size_t pre_len = strlen(ego_prefix_label);
        if (!strncmp(first_part_for_short, ego_prefix_label, pre_len))
        {
            char* p = first_part_for_short + pre_len;
            if (*p == '\0')
            {
                first_part_for_short = p;
            }
            else if (isspace((unsigned char)*p))
            {
                while (*p && isspace((unsigned char)*p))
                    ++p;
                first_part_for_short = p;
            }
        }
    }

    char short_first[128];
    short_first[0] = '\0';

    if (first_part_for_short[0])
    {
        log_debug("mode4_shorten: extracting last words from first_part='%s'", first_part_for_short);

        char* cursor = first_part_for_short;
        char* last_start = NULL;
        size_t last_len = 0;
        char* prev_start = NULL;
        size_t prev_len = 0;

        while (*cursor)
        {
            while (*cursor && isspace((unsigned char)*cursor))
                ++cursor;
            if (!*cursor)
                break;

            char* word_start = cursor;
            bool has_alpha = false;
            while (*cursor && !isspace((unsigned char)*cursor))
            {
                if (isalpha((unsigned char)*cursor))
                    has_alpha = true;
                ++cursor;
            }

            size_t word_len = (size_t)(cursor - word_start);
            if (has_alpha && word_len > 0)
            {
                prev_start = last_start;
                prev_len = last_len;
                last_start = word_start;
                last_len = word_len;
                log_debug("mode4_shorten: found alpha word at offset %td len=%zu", word_start - first_part, word_len);
            }
        }

        bool use_prev = false;
        if (apply_rules && last_start && last_len > 0 && prev_start && prev_len > 0)
        {
            char prev_word[32];
            size_t copy_len = prev_len;
            if (copy_len >= sizeof(prev_word))
                copy_len = sizeof(prev_word) - 1;
            memcpy(prev_word, prev_start, copy_len);
            prev_word[copy_len] = '\0';

            for (size_t k = 0; prev_word[k]; ++k)
                prev_word[k] = (char)tolower((unsigned char)prev_word[k]);

            if (strcmp(prev_word, "of") && strcmp(prev_word, "a")
                && strcmp(prev_word, "an") && strcmp(prev_word, "the")
                && strcmp(prev_word, "and"))
            {
                use_prev = true;
            }
        }

        if (last_start && last_len > 0)
        {
            size_t out = 0;

            if (use_prev)
            {
                size_t take_prev = prev_len;
                if (take_prev >= sizeof(short_first))
                    take_prev = sizeof(short_first) - 1;
                memcpy(short_first + out, prev_start, take_prev);
                out += take_prev;

                if (out + 1 < sizeof(short_first))
                    short_first[out++] = ' ';
            }

            size_t remaining = sizeof(short_first) - 1 - out;
            size_t take_last = last_len;
            if (take_last > remaining)
                take_last = remaining;
            memcpy(short_first + out, last_start, take_last);
            out += take_last;

            short_first[out] = '\0';
            object_desc_trim_spaces(short_first);
            log_debug("mode4_shorten: short_first='%s' (use_prev=%d)", short_first, use_prev ? 1 : 0);
        }
    }

    if (ego_prefix_label[0])
    {
        if (short_first[0] && !strncmp(short_first, ego_prefix_label, strlen(ego_prefix_label)))
        {
            /* Already includes prefix; do nothing. */
        }
        else
        {
            char with_prefix[128];
            with_prefix[0] = '\0';
            SDL_strlcpy(with_prefix, ego_prefix_label, sizeof(with_prefix));
            if (short_first[0])
            {
                SDL_strlcat(with_prefix, " ", sizeof(with_prefix));
                SDL_strlcat(with_prefix, short_first, sizeof(with_prefix));
            }
            SDL_strlcpy(short_first, with_prefix, sizeof(short_first));
            object_desc_trim_spaces(short_first);
        }
    }

    char cleaned_second[256];
    cleaned_second[0] = '\0';
    const char* s = second_part;
    while (s && *s)
    {
        while (isspace((unsigned char)*s))
            ++s;
        if (!*s)
            break;

        const char* token_start = s;
        while (*s && !isspace((unsigned char)*s))
            ++s;
        size_t token_len = (size_t)(s - token_start);
        if (!token_len)
            continue;

        char token[64];
        if (token_len < sizeof(token))
        {
            memcpy(token, token_start, token_len);
            token[token_len] = '\0';
        }
        else
        {
            continue;
        }

        char token_lower[64];
        size_t tok_len = strlen(token);
        for (size_t k = 0; k < tok_len && k < sizeof(token_lower) - 1; ++k)
            token_lower[k] = (char)tolower((unsigned char)token[k]);
        token_lower[tok_len] = '\0';

        if (!strcmp(token_lower, "of") || !strcmp(token_lower, "a") || !strcmp(token_lower, "the"))
            continue;

        if (cleaned_second[0])
            SDL_strlcat(cleaned_second, " ", sizeof(cleaned_second));
        SDL_strlcat(cleaned_second, token, sizeof(cleaned_second));
    }

    char name_part[256];
    name_part[0] = '\0';
    
    /* Build name from shortened first part (base item name) */
    if (short_first[0])
    {
        SDL_strlcpy(name_part, short_first, sizeof(name_part));
        log_debug("mode4_shorten: name_part from short_first='%s'", name_part);
    }
    
    /* Add enchantment qualifier if present (e.g., "of Speed") */
    if (cleaned_second[0])
    {
        if (name_part[0])
            SDL_strlcat(name_part, " ", sizeof(name_part));
        SDL_strlcat(name_part, cleaned_second, sizeof(name_part));
        log_debug("mode4_shorten: name_part after adding cleaned_second='%s'", name_part);
    }
    
    /* Fallback if nothing was extracted */
    if (!name_part[0])
    {
        SDL_strlcpy(name_part, base, sizeof(name_part));
        log_debug("mode4_shorten: name_part fallback to base='%s'", name_part);
    }

    if (trailing_suffix[0])
    {
        if (name_part[0])
            SDL_strlcat(name_part, " ", sizeof(name_part));
        SDL_strlcat(name_part, trailing_suffix, sizeof(name_part));
        log_debug("mode4_shorten: name_part after suffix='%s'", name_part);
    }

    object_desc_trim_spaces(name_part);
    log_debug("mode4_shorten: final name_part='%s'", name_part);

    char rebuilt[256];
    rebuilt[0] = '\0';
    SDL_strlcpy(rebuilt, name_part, sizeof(rebuilt));
    if (stats[0])
    {
        if (rebuilt[0])
            SDL_strlcat(rebuilt, " ", sizeof(rebuilt));
        SDL_strlcat(rebuilt, stats, sizeof(rebuilt));
    }

    object_desc_trim_spaces(rebuilt);
    log_debug("mode4_shorten: FINAL OUTPUT='%s'", rebuilt);
    SDL_strlcpy(buf, rebuilt, max);
}

static const char* object_desc_unidentified_inscription(int pref, int mode)
{
    if (mode >= 4)
        return "?";
    if (pref)
        return "unknown";
    return "not identified";
}

static bool object_is_unidentified_for_display(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (o_ptr->ident & IDENT_SPOIL)
        return false;

    if (!object_aware_p(o_ptr))
        return true;

    if (o_ptr->tval == TV_STAFF || o_ptr->tval == TV_HORN)
        return !object_known_p(o_ptr);

    if (object_uses_smithing_difficulty(o_ptr))
        return !object_known_p(o_ptr);

    return false;
}

static void object_desc_append_inscription(char* buf, size_t max, const char* tag)
{
    if (!tag || !tag[0] || !buf || max < 2)
        return;

    if (buf[0])
        SDL_strlcat(buf, ", ", max);
    SDL_strlcat(buf, tag, max);
}

static const char* object_desc_curse_inscription(const object_type* o_ptr,
    bool known, u32b f3)
{
    if (!known || !cursed_p(o_ptr))
        return NULL;

    if (f3 & TR3_PERMA_CURSE)
        return "bound by the Oath of Feanor";

    if (f3 & TR3_HEAVY_CURSE)
        return "heavily cursed";

    return "cursed";
}

static const char* object_desc_feeling_inscription(int discount)
{
    if (discount < INSCRIP_NULL)
        return NULL;

    switch (discount)
    {
    case INSCRIP_AVERAGE:
    case INSCRIP_GOOD_STRONG:
    case INSCRIP_EXCELLENT:
    case INSCRIP_SPECIAL:
        return NULL;

    case INSCRIP_TERRIBLE:
    case INSCRIP_WORTHLESS:
        return "cursed";

    default:
        return inscrip_text[discount - INSCRIP_NULL];
    }
}

static void object_desc_prepend_prefix(char* dst, size_t dst_size,
    const char* base, const char* prefix)
{
    if (!dst || dst_size == 0)
        return;

    dst[0] = '\0';

    if (!base)
        base = "";
    if (!prefix || !prefix[0])
    {
        SDL_strlcpy(dst, base, dst_size);
        return;
    }

    if (base[0] == '&' && base[1] == ' ')
    {
        SDL_strlcpy(dst, "& ", dst_size);
        SDL_strlcat(dst, prefix, dst_size);
        SDL_strlcat(dst, " ", dst_size);
        SDL_strlcat(dst, base + 2, dst_size);
    }
    else
    {
        SDL_strlcpy(dst, prefix, dst_size);
        SDL_strlcat(dst, " ", dst_size);
        SDL_strlcat(dst, base, dst_size);
    }
}

void object_desc(
    char* buf, size_t max, const object_type* o_ptr, int pref, int mode)
{
    cptr basenm;
    cptr modstr;

    bool aware;
    bool known;

    bool flavor;

    bool append_name;

    char* b;

    char* t;

    cptr s;

    cptr u;
    cptr v;

    char a1 = '<', a2 = '>';
    char p1 = '(', p2 = ')';
    char b1 = '[', b2 = ']';
    char c1 = '{', c2 = '}';

    char discount_buf[80];
    char special_buf[80];

    char tmp_buf[128];

    u32b f1, f2, f3;

    object_kind* k_ptr = &k_info[o_ptr->k_idx];

    /* Extract some flags */
    object_flags(o_ptr, &f1, &f2, &f3);

    /* See if the object is "aware" */
    aware = (object_aware_p(o_ptr) ? true : false);

    /* See if the object is "known" */
    known = (object_known_p(o_ptr) ? true : false);

    /* See if the object is "flavored" */
    flavor = (k_ptr->flavor ? true : false);

    /* We have seen the object */
    if (aware)
        k_ptr->everseen = true;

    /* Object is being listed in the object knowledge or smithing screen */
    if (o_ptr->ident & IDENT_SPOIL)
    {
        /* Don't show flavors */
        flavor = false;

        /* Pretend known and aware */
        aware = true;
        known = true;
    }

    /* Player has now seen the item
     *
     * This code must be exactly here to properly handle objects in
     * stores (fake assignment to "aware", see above) and unaware objects
     * in the dungeon.
     */
    if (aware)
        k_ptr->everseen = true;

    /* Assume no name appending */
    append_name = false;

    /* Extract default "base" string */
    basenm = (k_name + k_ptr->name);

    /* Assume no "modifier" string */
    modstr = "";

    /* Analyze the object */
    switch (o_ptr->tval)
    {
    /* Some objects are easy to describe */
    case TV_SKELETON:
    case TV_METAL:
    case TV_NOTE:
    case TV_FLASK:
    case TV_CHEST:
    {
        break;
    }

    /* Missiles/Bows/Weapons */
    case TV_ARROW:
    case TV_BOW:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_DIGGING:
    {
        break;
    }

    /* Armour */
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_CLOAK:
    case TV_CROWN:
    case TV_HELM:
    case TV_SHIELD:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        break;
    }

    /* Lights (including a few "Specials") */
    case TV_LIGHT:
    {
        break;
    }

    /* Amulets (including a few "Specials") */
    case TV_AMULET:
    {
        /* Hack -- Known artefacts */
        if (artefact_p(o_ptr) && aware)
            break;

        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = true;
        basenm = (flavor ? "& # Amulet~" : "& Amulet~");

        break;
    }

    /* Rings (including a few "Specials") */
    case TV_RING:
    {
        /* Hack -- Known artefacts */
        if (artefact_p(o_ptr) && aware)
            break;

        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = true;
        basenm = (flavor ? "& # Ring~" : "& Ring~");

        break;
    }

    /* Staffs */
    case TV_STAFF:
    {
        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = true;
        basenm = (flavor ? "& # Staff~" : "& Staff~");

        break;
    }

    /* Gems */
    case TV_GEM:
    {
        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = true;
        basenm = (flavor ? "& # Gem~" : "& Gem~");

        break;
    }

    /* Horns */
    case TV_HORN:
    {
        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = true;
        basenm = (flavor ? "& # Horn~" : "& Horn~");

        break;
    }

    /* Potions */
    case TV_POTION:
    {
        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = true;
        basenm = (flavor ? "& # Potion~" : "& Potion~");

        break;
    }

    /* Food */
    case TV_FOOD:
    {
        /* Ordinary food is "boring" */
        if (o_ptr->sval >= SV_FOOD_MIN_FOOD)
            break;

        /* Color the object */
        modstr = flavor_text + flavor_info[k_ptr->flavor].text;
        if (aware)
            append_name = true;
        basenm = (flavor ? "& # Herb~" : "& Herb~");

        // Easter Eggs
        if (easter_time())
            basenm = (flavor ? "& # Easter Egg~" : "& Easter Egg~");

        break;
    }

    /* Hack -- Default -- Used in the "inventory" routine */
    default:
    {
        SDL_strlcpy(buf, "(nothing)", max);
        return;
    }
    }

    /* Start dumping the result */
    t = b = tmp_buf;

    /* Insert visible runtime prefixes before normal ego prefixes. */
    char basenm_with_runtime[128];
    basenm_with_runtime[0] = '\0';
    if (object_is_fire_broken(o_ptr))
    {
        object_desc_prepend_prefix(
            basenm_with_runtime, sizeof(basenm_with_runtime), basenm, "(broken)");
        basenm = basenm_with_runtime;
    }

    /* Insert ego prefix into base name (after '& ' if present). */
    char basenm_with_prefix[128];
    basenm_with_prefix[0] = '\0';
    if (known && object_ego_prefix(o_ptr))
    {
        ego_item_type* e_ptr = &e_info[object_ego_prefix(o_ptr)];
        const char* raw = e_name + e_ptr->name;

        char prefix_buf[80];
        prefix_buf[0] = '\0';
        if (raw && raw[0])
        {
            if (ego_name_is_prefix(raw))
            {
                size_t len = strlen(raw);
                size_t copy_len = (len >= 2) ? (len - 2) : 0;
                if (copy_len >= sizeof(prefix_buf))
                    copy_len = sizeof(prefix_buf) - 1;
                if (copy_len > 0)
                {
                    memcpy(prefix_buf, raw + 1, copy_len);
                    prefix_buf[copy_len] = '\0';
                }
            }
            else
            {
                SDL_strlcpy(prefix_buf, raw, sizeof(prefix_buf));
            }
        }

        if (prefix_buf[0])
        {
            object_desc_prepend_prefix(
                basenm_with_prefix, sizeof(basenm_with_prefix), basenm, prefix_buf);
            basenm = basenm_with_prefix;
        }
    }

    /* Begin */
    s = basenm;

    /* Handle objects which sometimes use "a" or "an" */
    if (*s == '&')
    {
        /* Paranoia XXX XXX XXX */
        /* ASSERT(s[1] == ' '); */

        /* Skip the ampersand and the following space */
        s += 2;

        /* No prefix */
        if (!pref)
        {
            /* Nothing */
        }

        /* Hack -- None left */
        else if (o_ptr->number <= 0)
        {
            object_desc_str_macro(t, "no more ");
        }

        /* Extract the number */
        else if (o_ptr->number > 1)
        {
            object_desc_num_macro(t, o_ptr->number);
            object_desc_chr_macro(t, ' ');
        }

        /* Hack -- The only one of its kind */
        else if (known && artefact_p(o_ptr))
        {
            object_desc_str_macro(t, "The ");
        }

        /* Hack -- A single one, and next character will be a vowel */
        else if ((*s == '#') ? is_a_vowel(modstr[0]) : is_a_vowel(*s))
        {
            object_desc_str_macro(t, "an ");
        }

        /* A single one, and next character will be a non-vowel */
        else
        {
            object_desc_str_macro(t, "a ");
        }
    }

    /* Handle objects which never use "a" or "an" */
    else
    {
        /* No pref */
        if (!pref)
        {
            /* Nothing */
        }

        /* Hack -- all gone */
        else if (o_ptr->number <= 0)
        {
            object_desc_str_macro(t, "no more ");
        }

        /* Prefix a number if required */
        else if (o_ptr->number > 1)
        {
            object_desc_num_macro(t, o_ptr->number);
            object_desc_chr_macro(t, ' ');
        }

        /* Hack -- The only one of its kind */
        else if (known && artefact_p(o_ptr))
        {
            object_desc_str_macro(t, "The ");
        }

        /* Hack -- A single item, so no prefix needed */
        else
        {
            /* Nothing */
        }
    }

    /* Paranoia XXX XXX XXX */
    /* ASSERT(*s != '~'); */

    /* Copy the string */
    for (; *s; s++)
    {
        /* Pluralizer */
        if (*s == '~')
        {
            /* Add a plural if needed */
            if (o_ptr->number != 1)
            {
                char k = t[-1];

                /* Hack -- "Cutlass-es" and "Torch-es" */
                if ((k == 's') || (k == 'h'))
                    *t++ = 'e';

                /* Add an 's' */
                *t++ = 's';
            }
        }

        /* Modifier */
        else if (*s == '#')
        {
            /* Append the modifier */
            object_desc_str_macro(t, modstr);
        }

        /* Normal */
        else
        {
            /* Copy */
            *t++ = *s;
        }
    }

    /* No more details wanted */
    if (mode < 0)
        goto object_desc_done;

    /* Append the "kind name" to the "base name" */
    if (append_name)
    {
        object_desc_str_macro(t, " of ");
        object_desc_str_macro(t, (k_name + k_ptr->name));
    }

    /* Hack -- Append "Artefact" or "Special" names */
    if (known)
    {
        /* Grab any artefact name */
        if (o_ptr->name1)
        {
            artefact_type* a_ptr = &a_info[o_ptr->name1];

            object_desc_chr_macro(t, ' ');
            object_desc_str_macro(t, a_ptr->name);
        }

        /* Grab any special item suffix name */
        else if (object_ego_suffix(o_ptr))
        {
            ego_item_type* e_ptr = &e_info[object_ego_suffix(o_ptr)];
            object_desc_chr_macro(t, ' ');
            object_desc_str_macro(t, (e_name + e_ptr->name));
        }
    }

    /* No more details wanted */
    if (mode < 1)
        goto object_desc_done;

    /* Hack -- Chests and skeletons must be described in detail */
    if (o_ptr->tval == TV_SKELETON)
    {
        cptr tail = "";

        /* May be "searched" */
        if (!o_ptr->pval)
        {
            tail = " (searched)";
        }

        object_desc_str_macro(t, tail);
    }
    else if (o_ptr->tval == TV_CHEST)
    {
        cptr tail = "";

        /* Not searched yet */
        if (!known || (o_ptr->sval == SV_CHEST_PRESENT))
        {
            /* Nothing */
        }

        /* May be "empty" */
        else if (!o_ptr->pval)
        {
            tail = " (empty)";
        }

        /* May be "disarmed" */
        else if (o_ptr->pval < 0)
        {
            if (object_chest_trap_flags(o_ptr))
            {
                tail = " (disarmed)";
            }
            else
            {
                tail = " (unlocked)";
            }
        }

        /* Describe the traps, if any */
        else
        {
            /* Describe the traps */
            switch (object_chest_trap_flags(o_ptr))
            {
            case 0:
            {
                tail = " (Locked)";
                break;
            }
            case CHEST_GAS_CONF:
            {
                tail = " (Gas Trap)";
                break;
            }
            case CHEST_GAS_STUN:
            {
                tail = " (Gas Trap)";
                break;
            }
            case CHEST_GAS_POISON:
            {
                tail = " (Gas Trap)";
                break;
            }
            case CHEST_NEEDLE_HALLU:
            {
                tail = " (Poison Needle)";
                break;
            }
            case CHEST_NEEDLE_ENTRANCE:
            {
                tail = " (Poison Needle)";
                break;
            }
            case CHEST_NEEDLE_LOSE_STR:
            {
                tail = " (Poison Needle)";
                break;
            }
            case CHEST_FLAME:
            {
                tail = " (Flame Trap)";
                break;
            }
            default:
            {
                tail = " (Multiple Traps)";
                break;
            }
            }
        }

        /* Append the tail */
        object_desc_str_macro(t, tail);
    }

    /* Dump base weapon info */
    switch (o_ptr->tval)
    {
    /* Missiles */
    case TV_ARROW:
    {
        /* Append a "hit" string if nonzero */
        if (o_ptr->att != 0)
        {
            object_desc_chr_macro(t, ' ');
            object_desc_chr_macro(t, p1);
            object_desc_int_macro(t, o_ptr->att);
            object_desc_chr_macro(t, p2);
        }

        /* All done */
        break;
    }

    /* Weapons */
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_DIGGING:
    case TV_BOW:
    {
        /* Append a "hit,damage" string */
        object_desc_chr_macro(t, ' ');
        object_desc_chr_macro(t, p1);
        object_desc_int_macro(t, o_ptr->att);
        object_desc_chr_macro(t, ',');
        object_desc_num_macro(t, o_ptr->dd);
        object_desc_chr_macro(t, 'd');
        /* Bonus for 'hand and a half' weapons like the bastard sword when used
         * with two hands */
        object_desc_num_macro(t, o_ptr->ds + hand_and_a_half_bonus(o_ptr));
        object_desc_chr_macro(t, p2);

        /* All done */
        break;
    }

    default: /* not a weapon */
    {
        /* don't display for unidentified rings */
        if ((o_ptr->tval == TV_RING) && !object_known_p(o_ptr))
        {
            break;
        }
        if (o_ptr->att)
        {
            object_desc_chr_macro(t, ' ');
            object_desc_chr_macro(t, p1);
            object_desc_int_macro(t, o_ptr->att);
            object_desc_chr_macro(t, p2);
        }
    }
    }

    /* show evasion/protection info */

    /* but don't display for unidentified rings */
    if ((o_ptr->tval == TV_RING) && !object_known_p(o_ptr))
    {
        // do nothing
    }
    else if (o_ptr->pd && o_ptr->ps)
    {
        object_desc_chr_macro(t, ' ');
        object_desc_chr_macro(t, b1);
        object_desc_int_macro(t, o_ptr->evn);
        object_desc_chr_macro(t, ',');
        object_desc_num_macro(t, o_ptr->pd);
        object_desc_chr_macro(t, 'd');
        object_desc_num_macro(t, o_ptr->ps);
        object_desc_chr_macro(t, b2);
    }
    else if (o_ptr->evn)
    {
        object_desc_chr_macro(t, ' ');
        object_desc_chr_macro(t, b1);
        object_desc_int_macro(t, o_ptr->evn);
        object_desc_chr_macro(t, b2);
    }

    /* No more details wanted */
    if (mode < 2)
        goto object_desc_done;

    /* Hack -- Staffs have charges */
    if (o_ptr->tval == TV_STAFF)
    {
        if (known)
        {
            /* Dump " (N charges)" */
            object_desc_chr_macro(t, ' ');
            object_desc_chr_macro(t, p1);

            /* Always show actual usable charges (internal pval is 2x for mechanics) */
            int visible_charges = (o_ptr->pval + CHANNELING_CHARGE_MULTIPLIER - 1)
                / CHANNELING_CHARGE_MULTIPLIER;
            object_desc_num_macro(t, visible_charges);

            /*write out the word charge(s) as appropriate*/
            object_desc_str_macro(t, " charge");
            if (visible_charges != 1)
            {
                object_desc_chr_macro(t, 's');
            }
            object_desc_chr_macro(t, p2);
        }

        else if ((o_ptr->xtra1 > 0) && !(o_ptr->ident & (IDENT_EMPTY)))
        {
            /* Dump " (used N times)" */
            object_desc_chr_macro(t, ' ');
            object_desc_chr_macro(t, p1);

            /*write out the word charge(s) as appropriate*/
            object_desc_str_macro(t, "used ");
            object_desc_num_macro(t, o_ptr->xtra1);
            object_desc_str_macro(t, " time");
            if (o_ptr->xtra1 != 1)
            {
                object_desc_chr_macro(t, 's');
            }
            object_desc_chr_macro(t, p2);
        }
    }

    /* Hack -- Process Lanterns/Torches */
    if (fuelable_light_p(o_ptr)
        && !(o_ptr->tval == TV_LIGHT && o_ptr->sval == SV_LIGHT_LANTERN
            && player_light_uses_oil_pool(o_ptr)))
    {
        /* Hack -- Turns of light for normal lites */
        object_desc_str_macro(t, " (");
        object_desc_num_macro(t, player_light_fuel(o_ptr));
        object_desc_str_macro(t, " turns)");
    }

    /* Dump "pval" flags for wearable items */
    u32b pval_f1 = object_pval_flags1(o_ptr);
    if (known && pval_f1)
    {
        int best = 0;
        int best_abs = 0;

        if (pval_f1 & (TR1_STR | TR1_NEG_STR))
        {
            int v = o_ptr->stat_bonus[A_STR];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }
        if (pval_f1 & (TR1_DEX | TR1_NEG_DEX))
        {
            int v = o_ptr->stat_bonus[A_DEX];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }
        if (pval_f1 & (TR1_CON | TR1_NEG_CON))
        {
            int v = o_ptr->stat_bonus[A_CON];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }
        if (pval_f1 & (TR1_GRA | TR1_NEG_GRA))
        {
            int v = o_ptr->stat_bonus[A_GRA];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }

        if (pval_f1 & TR1_MEL)
        {
            int v = o_ptr->skill_bonus[S_MEL];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }
        if (pval_f1 & TR1_ARC)
        {
            int v = o_ptr->skill_bonus[S_ARC];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }
        if (pval_f1 & TR1_STL)
        {
            int v = o_ptr->skill_bonus[S_STL];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }
        if (pval_f1 & TR1_PER)
        {
            int v = o_ptr->skill_bonus[S_PER];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }
        if (pval_f1 & TR1_WIL)
        {
            int v = o_ptr->skill_bonus[S_WIL];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }
        if (pval_f1 & TR1_SMT)
        {
            int v = o_ptr->skill_bonus[S_SMT];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }
        if (pval_f1 & TR1_SNG)
        {
            int v = o_ptr->skill_bonus[S_SNG];
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }

        if (pval_f1 & (TR1_TUNNEL | TR1_DAMAGE_SIDES))
        {
            int v = o_ptr->pval;
            int av = ABS(v);
            if (av > best_abs) { best_abs = av; best = v; }
        }

        cptr tail = "";
        cptr tail2 = "";

        /* Start the display */
        object_desc_chr_macro(t, ' ');
        object_desc_chr_macro(t, a1);

        /* Dump the best representative pval-style bonus. */
        object_desc_int_macro(t, best);

        /* Add the descriptor */
        object_desc_str_macro(t, tail);
        object_desc_str_macro(t, tail2);

        /* Finish the display */
        object_desc_chr_macro(t, a2);
    }

    /* Indicate "charging" objects, but not horns or lights */
    if (known && o_ptr->timeout && !fuelable_light_p(o_ptr))
    {
        /* Hack -- Dump " (charging)" if relevant */
        object_desc_str_macro(t, " (recharging)");
    }

    /* No more details wanted */
    if (mode < 3)
        goto object_desc_done;

    /* Use standard inscription */
    if (o_ptr->obj_note)
    {
        u = quark_str(o_ptr->obj_note);
    }

    /* Use nothing */
    else
    {
        u = NULL;
    }

    special_buf[0] = '\0';
    if (object_is_unidentified_for_display(o_ptr))
    {
        object_desc_append_inscription(
            special_buf, sizeof(special_buf),
            object_desc_unidentified_inscription(pref, mode));
    }

    v = NULL;
    discount_buf[0] = '\0';

    if (o_ptr->discount >= INSCRIP_NULL)
    {
        v = object_desc_feeling_inscription(o_ptr->discount);
    }
    else if ((v = object_desc_curse_inscription(o_ptr, known, f3)) != NULL)
    {
    }
    else if (!known && (o_ptr->ident & (IDENT_EMPTY)))
    {
        v = "empty";
    }
    else if (!aware && object_tried_p(o_ptr))
    {
        v = "tried";
    }
    else if (o_ptr->discount > 0)
    {
        char* q = discount_buf;
        object_desc_num_macro(q, o_ptr->discount);
        object_desc_str_macro(q, "% off");
        *q = '\0';
        v = discount_buf;
    }

    object_desc_append_inscription(special_buf, sizeof(special_buf), v);
    v = special_buf[0] ? special_buf : NULL;

    /* Inscription */
    if (u || v)
    {
        /* Begin the inscription */
        *t++ = ' ';
        *t++ = c1;

        /* Standard inscription */
        if (u)
        {
            /* Append the inscription */
            while ((t < b + 75) && *u)
                *t++ = *u++;
        }

        /* Special inscription too */
        if (u && v && (t < b + 75))
        {
            /* Separator */
            *t++ = ',';
            *t++ = ' ';
        }

        /* Special inscription */
        if (v)
        {
            /* Append the inscription */
            while ((t < b + 75) && *v)
                *t++ = *v++;
        }

        /* Terminate the inscription */
        *t++ = c2;
    }

object_desc_done:

    /* Terminate */
    *t = '\0';

    if ((mode == 4) && !pref)
    {
        bool apply_rules = !artefact_p(o_ptr);
        object_desc_mode4_shorten(tmp_buf, sizeof(tmp_buf), o_ptr, apply_rules);
    }

    /* Copy the string over */
    SDL_strlcpy(buf, tmp_buf, max);
}

/*
 * Describe an item that is known to be lying on the floor.
 *
 * For smithing-difficulty items that have not been identified yet (and have
 * not been handled by the player), suppress the combat stats in the short
 * name display (e.g. show just "Short Sword").
 */
void object_desc_floor(
    char* buf, size_t max, const object_type* o_ptr, int pref, int mode)
{
    const char* u;
    const char* v;
    char discount_buf[80];
    char special_buf[80];

    bool aware;
    bool known;
    u32b f1, f2, f3;

    if (!o_ptr || !o_ptr->k_idx)
    {
        SDL_strlcpy(buf, "(nothing)", max);
        return;
    }

    if (!object_uses_smithing_difficulty(o_ptr) || object_known_p(o_ptr)
        || (o_ptr->ident & IDENT_HANDLED))
    {
        object_desc(buf, max, o_ptr, pref, mode);
        return;
    }

    /* Base name only (no combat stats). */
    object_desc(buf, max, o_ptr, pref, 0);

    /* Match object_desc() inscription behavior for mode 3+. */
    if (mode < 3)
        return;

    aware = (object_aware_p(o_ptr) ? true : false);
    known = (object_known_p(o_ptr) ? true : false);
    object_flags(o_ptr, &f1, &f2, &f3);
    (void)f1;
    (void)f2;

    if (o_ptr->obj_note)
        u = quark_str(o_ptr->obj_note);
    else
        u = NULL;

    special_buf[0] = '\0';
    if (object_is_unidentified_for_display(o_ptr))
    {
        object_desc_append_inscription(
            special_buf, sizeof(special_buf),
            object_desc_unidentified_inscription(pref, mode));
    }

    v = NULL;
    discount_buf[0] = '\0';

    if (o_ptr->discount >= INSCRIP_NULL)
    {
        v = object_desc_feeling_inscription(o_ptr->discount);
    }
    else if ((v = object_desc_curse_inscription(o_ptr, known, f3)) != NULL)
    {
    }
    else if (!known && (o_ptr->ident & (IDENT_EMPTY)))
    {
        v = "empty";
    }
    else if (!aware && object_tried_p(o_ptr))
    {
        v = "tried";
    }
    else if (o_ptr->discount > 0)
    {
        strnfmt(discount_buf, sizeof(discount_buf), "%d%% off", o_ptr->discount);
        v = discount_buf;
    }

    object_desc_append_inscription(special_buf, sizeof(special_buf), v);
    v = special_buf[0] ? special_buf : NULL;

    if (u || v)
    {
        SDL_strlcat(buf, " {", max);
        if (u)
            SDL_strlcat(buf, u, max);
        if (u && v)
            SDL_strlcat(buf, ", ", max);
        if (v)
            SDL_strlcat(buf, v, max);
        SDL_strlcat(buf, "}", max);
    }
}

/*
 * Describe an item and pretend the item is fully known and has no flavor.
 */
void object_desc_spoil(
    char* buf, size_t max, const object_type* o_ptr, int pref, int mode)
{
    object_type object_type_body;
    object_type* i_ptr = &object_type_body;

    /* Make a backup */
    object_copy(i_ptr, o_ptr);

    /* Set it to display as identified but without flavour */
    i_ptr->ident |= IDENT_SPOIL;

    /* Describe */
    object_desc(buf, max, i_ptr, pref, mode);
}

/*
 * Describe an item's random attributes for "character dumps"
 */
void identify_random_gen(const object_type* o_ptr)
{
    /* Set hooks for character dump */
    object_info_out_flags = object_flags_known;

    /* Set the indent/wrap */
    text_out_indent = 3;
    text_out_wrap = 65;

    /* Dump the info */
    if (object_info_out(o_ptr))
        text_out("\n");

    /* Reset indent/wrap */
    text_out_indent = 0;
    text_out_wrap = 0;
}

/*
 * Convert an inventory index into a one character label.
 *
 * Note that the label does NOT distinguish inven/equip.
 */
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

static bool supplies_visible_for_current_filter(void)
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

static bool inventory_menu_uses_visible_labels(void)
{
    return (p_ptr->get_item_mode != 0);
}

static bool inventory_menu_uses_expanded_supplies(void)
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

static int inventory_visible_supply_count(void)
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

static int inventory_visible_supply_item_at(int ordinal)
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

static int inventory_visible_supply_ordinal(int item)
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

static int inventory_visible_inven_item_at(int ordinal)
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

static int inventory_visible_inven_ordinal(int item)
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

static char inventory_visible_label_for_item(int item)
{
    int ordinal = inventory_visible_inven_ordinal(item);

    if (ordinal >= 0)
        return I2A(ordinal);

    return index_to_label(item);
}

static bool inventory_item_is_supply_summary(int item)
{
    return (item == SUPPLIES_INDEX) && !inventory_menu_uses_expanded_supplies();
}

static bool inventory_item_is_supply_entry(int item)
{
    return (item >= SUPPLIES_INDEX) && inventory_menu_uses_expanded_supplies();
}

static bool inventory_item_is_equipment(int item)
{
    return (item >= INVEN_WIELD) && (item < INVEN_TOTAL);
}

static object_type* inventory_item_to_object_ptr(int item)
{
    if (item >= SUPPLIES_INDEX)
        return supplies_entry_at(item - SUPPLIES_INDEX);

    if (item >= 0)
        return &inventory[item];

    return &o_list[0 - item];
}

static bool inventory_item_uses_inven_channel(int item)
{
    return ((item >= 0) && (item < INVEN_WIELD)) || (item >= SUPPLIES_INDEX);
}

static void describe_inventory_menu_entry(int item, char* buf, size_t len)
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

static bool item_prompt_is_replace(cptr pmt)
{
    return pmt && strstr(pmt, "Replace which item to pick up ") != NULL;
}

static void log_inventory_selector_state(cptr stage, cptr pmt,
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

static void format_supply_summary(char* buf, size_t len)
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
static int draw_item_tile(int x, int y, object_type* o_ptr)
{
    /* Only draw tiles in graphics mode (not ASCII or pseudo-graphics) */
    if (use_graphics != GRAPHICS_NONE && use_graphics != GRAPHICS_PSEUDO && o_ptr && o_ptr->k_idx)
    {
        /* Get the attribute from object_attr - in graphics mode this is a tile index, not a color code
         * We do NOT shade tile indices as they refer to specific graphics in the tileset */
        byte attr = object_attr(o_ptr);
        
        /* Draw the tile using the standard object display functions */
        Term_putch(x, y, attr, object_char(o_ptr));
        
        /* Handle bigtile mode (tiles that occupy 2 cells) */
        if (use_bigtile)
        {
            Term_putch(x + 1, y, 255, -1);
            return x + 2; /* Text starts 2 cells after tile */
        }
        
        return x + 1; /* Text starts 1 cell after tile */
    }
    
    /* No tile drawn, text starts at the same position */
    return x;
}

static object_type* prepare_supply_icon_object(object_type* o_ptr)
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

static bool inventory_selection_uses_square(void)
{
    return op_ptr && op_ptr->opt[OPT_inventory_selection_square];
}

static bool inventory_selection_frame_is_big(const object_type* o_ptr)
{
    return use_bigtile && use_graphics != GRAPHICS_NONE
        && use_graphics != GRAPHICS_PSEUDO && o_ptr && o_ptr->k_idx;
}

static void draw_inventory_selection_frame(int x, int y,
    const object_type* o_ptr)
{
    (void)Term_set_extra_cursor(true, x, y,
        inventory_selection_frame_is_big(o_ptr));
}

static int menu_term_width(void)
{
    if (Term && Term->wid > 0)
        return Term->wid;

    return 80;
}

static int menu_term_height(void)
{
    if (Term && Term->hgt > 0)
        return Term->hgt;

    return 24;
}

static int inventory_menu_visible_rows_for_height(int term_hgt)
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

static int inventory_menu_scroll_to_selection(int scroll, int selected_row,
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

static int menu_weight_col_for_width(int term_wid)
{
    /* Reserve 8 columns for weight and 4 columns for the trailing label. */
    int col = term_wid - 12;

    if (col < 0)
        col = 0;

    return col;
}

static int menu_label_col_for_width(int term_wid, bool display_weights)
{
    /* Labels are rendered as " (a)" or "(a)" and need 4 terminal cells. */
    int col = term_wid - 4;

    (void)display_weights;

    if (col < 0)
        col = 0;

    return col;
}

static int menu_center_col_for_len(int term_wid, int len)
{
    if (len >= term_wid)
        return 0;

    return (term_wid - len) / 2;
}

static int menu_overlay_clear_col(int col)
{
    /* Keep a one-cell gutter so centered overlays stay visually separate. */
    if (col > 0)
        return col - 1;

    return 0;
}

static int menu_desc_limit(int text_col, int label_col, int weight_col,
    bool display_weights)
{
    int right_edge = display_weights ? weight_col : label_col;
    int limit = right_edge - text_col;

    if (limit < 1)
        limit = 1;

    return limit;
}

static void story_render_inventory_entry(int row, int base_col, int label_col,
    cptr desc, byte desc_attr, bool display_weights, cptr weight_text,
    byte weight_attr, cptr label_text, byte label_attr, const object_type* o_ptr,
    bool highlight, int story_term_w)
{
    int term_wid = (story_term_w > 0) ? story_term_w : menu_term_width();
    int highlight_cols = term_wid;
    int weight_col = display_weights ? MAX(0, label_col - 8) : label_col;
    const int label_width = 4;
    bool square_selection = highlight && inventory_selection_uses_square();

    Term_erase(base_col, row, 255);
    if (highlight)
        story_fill_rect(row, base_col, highlight_cols - base_col, TERM_L_BLUE);

    int text_col = base_col;
    if (o_ptr && o_ptr->k_idx)
        text_col = draw_item_tile(base_col, row, (object_type*)o_ptr);
    if (square_selection)
        draw_inventory_selection_frame(base_col, row, o_ptr);

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

static void story_render_equipment_entry(int row, int col, int slot, cptr prefix,
    byte prefix_attr, cptr desc, byte desc_attr, bool display_weights,
    cptr weight_text, byte weight_attr, cptr label_text, byte label_attr,
    const object_type* o_ptr, bool highlight, int story_term_w)
{
    int term_wid = (story_term_w > 0) ? story_term_w : menu_term_width();
    int highlight_cols = term_wid;
    int label_col = menu_label_col_for_width(term_wid, display_weights);
    int weight_col = menu_weight_col_for_width(term_wid);
    int clear_col = menu_overlay_clear_col(col);
    const int label_width = 4;
    bool has_object = (o_ptr && o_ptr->k_idx);
    bool square_selection = highlight && inventory_selection_uses_square();

    Term_erase(clear_col, row, 255);
    if (highlight)
        story_fill_rect(row, col, highlight_cols - col, TERM_L_BLUE);

    story_print_equipment_prefix(row, col, prefix_attr, prefix);

    int text_col = col + 12 + 2;
    if (has_object)
        text_col = draw_item_tile(col + 12 + 2, row, (object_type*)o_ptr);
    if (square_selection)
        draw_inventory_selection_frame(col + 12 + 2, row,
            has_object ? o_ptr : NULL);

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

static void equipment_weight_layout_rows(int first_row, int item_count,
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

static int equipment_first_occupied_row(int entry_count, const int* out_index)
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

static int equipment_next_occupied_row(int entry_count, const int* out_index,
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

static void draw_equipment_story_rows(int col, int entry_count, int* out_index,
    byte* out_color, char out_desc[][80], bool highlight_active,
    int highlight_index, bool display_weights, int story_term_w)
{
    int term_wid = (story_term_w > 0) ? story_term_w : menu_term_width();
    int label_col_base = menu_label_col_for_width(term_wid, display_weights);
    int weight_col = menu_weight_col_for_width(term_wid);
    int highlight_cols = term_wid;
    int clear_col = menu_overlay_clear_col(col);
    const int label_width = 4;
    bool square_selection = inventory_selection_uses_square();

    log_trace("draw_equipment_story_rows: entry_count=%d, highlight_active=%d, highlight_index=%d",
        entry_count, highlight_active, highlight_index);

    for (int idx = 0; idx < entry_count; idx++)
    {
        int row = idx + 1;
        bool is_highlight = highlight_active && idx == highlight_index;
        byte line_attr = is_highlight ? TERM_L_BLUE : out_color[idx];
        int slot = out_index[idx];
        object_type* o_ptr = &inventory[slot];
        bool has_object = o_ptr->k_idx != 0;

        if (is_highlight)
        {
            log_trace("draw_equipment_story_rows: Drawing HIGHLIGHTED row %d, slot=%d, has_object=%d, desc='%s'",
                row, slot, has_object, out_desc[idx]);
        }

        Term_erase(clear_col, row, 255);
        if (is_highlight)
        {
            log_trace("draw_equipment_story_rows: Filling highlight rect at row %d", row);
            story_fill_rect(row, col, highlight_cols - col, TERM_L_BLUE);
        }

        char prefix[32];
        strnfmt(prefix, sizeof(prefix), "%-12s: ", mention_use(slot));
        byte prefix_attr = is_highlight ? TERM_L_BLUE : TERM_WHITE;
        log_trace("draw_equipment_story_rows: Row %d - printing prefix '%s' at col=%d", row, prefix, col);
        story_print_equipment_prefix(row, col, prefix_attr, prefix);

        int text_col = col + 12 + 2;
        int tile_col = text_col;
        log_trace("draw_equipment_story_rows: Row %d - text_col calculated as %d (col=%d + 12 + 2)", row, text_col, col);
        if (has_object)
        {
            int tile_end_col = draw_item_tile(text_col, row, o_ptr);
            log_trace("draw_equipment_story_rows: Row %d - drew tile, text_col updated from %d to %d", row, text_col, tile_end_col);
            text_col = tile_end_col;
        }
        if (is_highlight && square_selection)
            draw_inventory_selection_frame(tile_col, row, has_object ? o_ptr : NULL);

        int label_col = label_col_base;
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
        byte label_attr = is_highlight ? TERM_L_BLUE : TERM_WHITE;
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
static bool get_item_allow(int item)
{
    cptr s;

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

    /* No inscription */
    if (!o_ptr->obj_note)
        return (true);

    /* Find a '!' */
    s = strchr(quark_str(o_ptr->obj_note), '!');

    /* Process preventions */
    while (s)
    {
        /* Check the "restriction" */
        if ((s[1] == p_ptr->command_cmd) || (s[1] == '*'))
        {
            /* Verify the choice */
            if (!verify_item("Really try", item))
                return (false);
        }

        /* Find another '!' */
        s = strchr(s + 1, '!');
    }

    /* Allow it */
    return (true);
}

/*
 * Verify the "okayness" of a given item.
 *
 * The item can be negative to mean "item on floor".
 */
static bool get_item_okay(int item)
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
 * Find the "first" inventory object with the given "tag".
 *
 * A "tag" is a char "n" appearing as "@n" anywhere in the
 * inscription of an object.
 *
 * Also, the tag "@xn" will work as well, where "n" is a tag-char,
 * and "x" is the "current" p_ptr->command_cmd code.
 *
 * Also works with '[' for first valid choice and ']' for last valid choice.
 */
static int get_tag(int* cp, char tag)
{
    int i;
    cptr s;

    /* Check every object */
    for (i = 0; i < INVEN_TOTAL; ++i)
    {
        object_type* o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Skip empty inscriptions */
        if (!o_ptr->obj_note)
            continue;

        /* Find a '@' */
        s = strchr(quark_str(o_ptr->obj_note), '@');

        /* Process all tags */
        while (s)
        {
            /* Check the normal tags */
            if (s[1] == tag)
            {
                /* Save the actual inventory ID */
                *cp = i;

                /* Success */
                return (true);
            }

            /* Check the special tags */
            if ((s[1] == p_ptr->command_cmd) && (s[2] == tag))
            {
                /* Save the actual inventory ID */
                *cp = i;

                /* Success */
                return (true);
            }

            /* Find another '@' */
            s = strchr(s + 1, '@');
        }
    }

    /* No such tag */
    return (false);
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
        byte attr = TERM_L_BLUE;                                                    \
        bool square_selection = inventory_selection_uses_square();                  \
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
                    if (display_obj && display_obj->k_idx)                          \
                        text_col = draw_item_tile(col, row+1, display_obj);         \
                    if (square_selection)                                           \
                        draw_inventory_selection_frame(col, row+1, display_obj);    \
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
                    if (o_ptr && o_ptr->k_idx)                                       \
                        text_col = draw_item_tile(col, row+1, o_ptr);               \
                    if (square_selection)                                           \
                        draw_inventory_selection_frame(col, row+1,                  \
                            (o_ptr && o_ptr->k_idx) ? o_ptr : NULL);                \
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
                  c_put_str(attr,usebuf,row+1,col);                                 \
                  int text_col = col + 12 + 2;                                      \
                  if (o_ptr->k_idx)                                                 \
                      text_col = draw_item_tile(col+12+2, row+1, o_ptr);            \
                  if (square_selection)                                             \
                      draw_inventory_selection_frame(col+12+2, row+1,               \
                          o_ptr->k_idx ? o_ptr : NULL);                             \
                  c_put_str(attr,tmp,row+1,text_col);                               \
                  if (show_weights && o_ptr->weight){ int wgt=o_ptr->weight*o_ptr->number; char w[16]; sprintf(w,"%2d.%1d lb",wgt/10,wgt%10); c_put_str(attr,w,row+1,weight_col);} \
                  { char lab[8]; sprintf(lab, " (%c)", index_to_label(item_index)); int label_col = label_col_base; c_put_str(attr,lab,row+1,label_col); }\
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
                int text_col = draw_item_tile(col, row+1, o_ptr);                   \
                if (square_selection)                                               \
                    draw_inventory_selection_frame(col, row+1, o_ptr);              \
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
        inventory_menu_scroll_offset = inventory_menu_scroll_to_selection(
            inventory_menu_scroll_offset, highlight_row, vis_inven_cnt,
            display_rows, 0);
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

        /* Build the prompt */
        strnfmt(tmp_val, sizeof(tmp_val), "(%s) %s", out_val, pmt);

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
            bool tag_found = get_tag(&k, which);
            if (!tag_found && p_ptr->command_see && highlight_active
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
            else if (!tag_found)
            {
                bell("Illegal object choice (tag)!");
                break;
            }

            /* Hack -- Validate the item */
            if (inventory_item_uses_inven_channel(k) ? !allow_inven
                : !allow_equip)
            {
                bell("Illegal object choice (tag)!");
                break;
            }

            /* Validate the item */
            if (!get_item_okay(k))
            {
                bell("Illegal object choice (tag)!");
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
                bell("Illegal object choice (tag)!");
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

static int enhanced_menu_primary_select_action(void)
{
    return (current_menu_command == 'x') ? ENHANCED_ACTION_EXAMINE
                                         : ENHANCED_ACTION_USE;
}

/*
 * Enhanced inventory display with scrolling and navigation
 * EXACTLY replicates show_inven() algorithm then adds highlighting
 */
#define MAX_COMPARE_LINES 2
#define ENHANCED_MENU_CLICK_SWITCH (-1000000)
#define ENHANCED_MENU_CLICK_DROP (-1000001)

static void enhanced_menu_highlight_prompt_token(cptr text, cptr token,
    byte attr)
{
    cptr match;

    if (!text || !token || !token[0])
        return;

    match = strstr(text, token);
    if (!match)
        return;

    Term_putstr((int)(match - text), 0, (int)strlen(token), attr, token);
}

static void append_compare_slot(int* slots, int* count, int slot)
{
    if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
        return;

    for (int i = 0; i < *count; i++)
    {
        if (slots[i] == slot)
            return;
    }

    if (*count < MAX_COMPARE_LINES)
        slots[(*count)++] = slot;
}


void describe_item_with_comparisons(int item_index, bool include_comparisons)
{
    const object_type* objects[MAX_COMPARE_LINES + 1];
    const char* headings[MAX_COMPARE_LINES + 1];
    char heading_texts[MAX_COMPARE_LINES + 1][64];
    int count = 0;
    object_type* base_obj;
    bool is_floor = (item_index < 0);

    if (item_index == ENHANCED_MENU_NO_SELECTION)
        return;

    if (is_floor)
    {
        int floor_idx = 0 - item_index;
        if (floor_idx <= 0 || floor_idx >= o_max)
            return;
        base_obj = &o_list[floor_idx];
    }
    else
    {
        if (item_index < 0 || item_index >= INVEN_TOTAL)
            return;
        base_obj = &inventory[item_index];
    }

    if (!base_obj->k_idx)
        return;

    /* Opening an item description attempts smithing-difficulty identification. */
    {
        bool is_equipped = (!is_floor && item_index >= INVEN_WIELD);
        (void)player_try_identify_smithing_object_on_examine(base_obj,
            is_equipped);
    }

    strnfmt(heading_texts[count], sizeof(heading_texts[count]), "%s:",
        is_floor ? "Selected item (floor)" : "Selected item");
    headings[count] = heading_texts[count];
    objects[count++] = base_obj;

    if (include_comparisons)
    {
        int slots[MAX_COMPARE_LINES];
        int slot_count = 0;

        append_compare_slot(slots, &slot_count, wield_slot(base_obj));

        if (base_obj->tval == TV_RING)
        {
            append_compare_slot(slots, &slot_count, INVEN_LEFT);
            append_compare_slot(slots, &slot_count, INVEN_RIGHT);
        }
        else if (base_obj->tval == TV_ARROW)
        {
            append_compare_slot(slots, &slot_count, INVEN_QUIVER1);
            append_compare_slot(slots, &slot_count, INVEN_QUIVER2);
        }

        for (int i = 0; i < slot_count && count < MAX_COMPARE_LINES + 1; i++)
        {
            int slot = slots[i];
            if (slot < INVEN_WIELD || slot >= INVEN_TOTAL)
                continue;

            object_type* equip_obj = &inventory[slot];
            strnfmt(heading_texts[count], sizeof(heading_texts[count]), "%s:", mention_use(slot));
            headings[count] = heading_texts[count];
            if (equip_obj->k_idx)
                objects[count] = equip_obj;
            else
                objects[count] = NULL;
            count++;
        }
    }

    object_info_screen_multi(objects, headings, count);
}

void show_inven_enhanced(void)
{
    int which;
    bool done = false;
    bool saved_hide_cursor = hide_cursor;
    bool saved_cursor = false;
    char out_val[160];

    bool use_story_font = story_inventory_enabled();
    story_font_term_state story_state;
    int story_term_w = 0;
    story_inventory_list_active = use_story_font;
    story_font_term_push(use_story_font, false, &story_state);
    if (use_story_font) {
        int story_term_h = 0;
        Term_get_size(&story_term_w, &story_term_h);
    }

    (void)Term_get_cursor(&saved_cursor);
    hide_cursor = true;
    (void)Term_set_cursor(false);

    /* Variables exactly matching show_inven() */
    int i, k, z;
    int col, len, lim;
    int term_wid = menu_term_width();
    int term_hgt = (Term && Term->hgt > 0) ? Term->hgt : 24;
    int weight_col = menu_weight_col_for_width(term_wid);
    int label_col_base = menu_label_col_for_width(term_wid, show_weights);
    int highlight_row = -1;
    bool highlight_active = false;
    int previous_total_rows = 0;
    int previous_compare_count = 0;
    int previous_highlight_row = -1; /* track last frame highlight for surgical clears */
    int scroll_top = 0;
    int previous_scroll_top = 0;
    int visible_rows = inventory_menu_visible_rows_for_height(term_hgt);
    bool first_render = true;
    bool drop_click_mode = false;
    
    /* Floor items variables */
    int floor_list[MAX_FLOOR_STACK];
    int floor_num;
    bool has_floor_items = false;
    bool include_supplies = !inventory_menu_include_equip && supplies_visible_for_current_filter();
    
    object_type* o_ptr;
    char o_name[80];
    char tmp_val[80];
    
    /* Arrays exactly matching show_inven() - expanded to include floor items */
    int out_index[ENHANCED_MAX_LIST];
    byte out_color[ENHANCED_MAX_LIST];
    char out_desc[ENHANCED_MAX_LIST][80];
    bool out_is_floor[ENHANCED_MAX_LIST];  /* Track which entries are floor items */
    bool out_is_supply[ENHANCED_MAX_LIST]; /* Track which entries are supply items */
    
    /* Default length (exactly like show_inven) */
    len = 29;
    
    /* Maximum space allowed for descriptions (exactly like show_inven) */
    lim = term_wid - 3;
    if (lim < 0)
        lim = 0;
    
    /* Require space for weight if needed (exactly like show_inven) */
    if (show_weights && lim > (weight_col - 1))
        lim = weight_col - 1;
    if (lim < 0)
        lim = 0;
    
    /* Scan floor items first to see if we have any */
    floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px, 0x00);
    has_floor_items = false;
    for (i = 0; i < floor_num; i++) {
        o_ptr = &o_list[floor_list[i]];
        if (item_tester_okay(o_ptr)) {
            has_floor_items = true;
            break;
        }
    }
    
    /* Find the "final" slot (exactly like show_inven) */
    z = 0;  /* Initialize z */
    for (i = 0; i < INVEN_PACK; i++)
    {
        o_ptr = &inventory[i];
        if (!o_ptr->k_idx) continue;
        z = i + 1;
    }
    
    /* Limit displayed items to leave room for supplies if they will be shown */
    if (include_supplies)
    {
        int max_items = INVEN_PACK - 1;  /* Reserve one slot for supplies */
        if (z > max_items)
            z = max_items;
    }
    
    /* Build combined list with floor items first, then inventory */
    k = 0;

    if (has_floor_items)
    {
        for (i = 0; i < floor_num; i++)
        {
            o_ptr = &o_list[floor_list[i]];

            if (!item_tester_okay(o_ptr))
                continue;

            object_desc_floor(o_name, sizeof(o_name), o_ptr, true, 3);
            o_name[lim] = '\0';

            out_index[k] = 0 - floor_list[i];
            out_is_floor[k] = true;
            out_is_supply[k] = false;
            out_color[k] = weapon_glows(o_ptr) 
                ? object_display_color(o_ptr, TERM_L_BLUE)
                : object_display_color(o_ptr, tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);
            SDL_strlcpy(out_desc[k], o_name, sizeof(out_desc[0]));

            int l = (int)strlen(out_desc[k]) + 5;
            if (show_weights)
                l += 9;
            if (l > len)
                len = l;

            k++;
        }
    }

    if (include_supplies && k < (int)N_ELEMENTS(out_index))
    {
        char supply_desc[80];
        format_supply_summary(supply_desc, sizeof(supply_desc));
        out_index[k] = SUPPLIES_INDEX;
        out_is_floor[k] = false;
        out_is_supply[k] = true;
        out_color[k] = TERM_L_WHITE;
        SDL_strlcpy(out_desc[k], supply_desc, sizeof(out_desc[0]));

        int l = (int)strlen(out_desc[k]) + 5;
        if (show_weights)
            l += 9;
        if (l > len)
            len = l;

        k++;
    }

    for (i = 0; i < z && k < (int)N_ELEMENTS(out_index); i++)
    {
        o_ptr = &inventory[i];

        if (!item_tester_okay(o_ptr))
            continue;

        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
        o_name[lim] = '\0';

        out_index[k] = i;
        out_is_floor[k] = false;
        out_is_supply[k] = false;
        out_color[k] = weapon_glows(o_ptr) 
            ? object_display_color(o_ptr, TERM_L_BLUE)
            : object_display_color(o_ptr, tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);
        SDL_strlcpy(out_desc[k], o_name, sizeof(out_desc[0]));

        int l = (int)strlen(out_desc[k]) + 5;
        if (show_weights)
            l += 9;
        if (l > len)
            len = l;

        k++;
    }

    /* Find the column to start in (exactly like show_inven) */
    col = menu_center_col_for_len(term_wid, len);
    
    log_debug("show_inven_enhanced: k=%d items, len=%d, col=%d, story_term_w=%d", k, len, col, story_term_w);
    
    /* Enable highlight if we have items */
    if (k > 0) {
        highlight_row = 0;
        highlight_active = true;
    }
    
    /* Main interaction loop */
    while (!done)
    {
        (void)Term_set_extra_cursor(false, 0, 0, false);
        term_hgt = menu_term_height();
        visible_rows = inventory_menu_visible_rows_for_height(term_hgt);
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
        bool square_selection = inventory_selection_uses_square();

        /* Show the prompt - different text based on how menu was opened */
        extern char current_menu_command;
        const bool controller_controls = steamdeck_controls_active();
        if (controller_controls) {
            char confirm_label[16];
            char desc_label[16];
            char cycle_label[16];
            bool same_button_cycle = inventory_menu_same_button_cycle_enabled();

            inventory_prompt_label(' ', "A", confirm_label, sizeof(confirm_label));
            inventory_prompt_label('x', "RS Right", desc_label, sizeof(desc_label));

            if (current_menu_command == 'u') {
                inventory_prompt_label('u', "X", cycle_label, sizeof(cycle_label));
                strnfmt(out_val, sizeof(out_val),
                    "%s-use  %s-desc  <- drop  %s-cycle (Inventory)",
                    confirm_label, desc_label, cycle_label);
            } else if (current_menu_command == 'x') {
                inventory_prompt_label('x', "RS Right", cycle_label, sizeof(cycle_label));
                strnfmt(out_val, sizeof(out_val),
                    "%s-examine  %s-desc  <- drop  %s-cycle (Inventory)",
                    confirm_label, desc_label, cycle_label);
            } else if (same_button_cycle) {
                inventory_prompt_label('i', "R1", cycle_label, sizeof(cycle_label));
                strnfmt(out_val, sizeof(out_val),
                    "%s-use  %s-desc  <- drop  %s-cycle (Inventory)",
                    confirm_label, desc_label, cycle_label);
            } else {
                strnfmt(out_val, sizeof(out_val),
                    "%s-use  %s-desc  <- drop (Inventory)",
                    confirm_label, desc_label);
            }
        } else if (current_menu_command == 'u') {
            sprintf(out_val, "Space-Use, -> description, <- drop, %c again-cycle  (Inventory)", current_menu_command);
        }
        else if (current_menu_command == 'x') {
            sprintf(out_val, "Space-Examine, -> description, <- drop, %c again-cycle  (Inventory)", current_menu_command);
        }
        else {
            sprintf(out_val, "Space-Use, -> description, <- drop  (Inventory)");
        }
        prt(out_val, 0, 0);
        ui_menu_click_add_text_token(ENHANCED_MENU_CLICK_DROP, 0, 0,
            out_val, "drop");
        ui_menu_click_add_text_token(ENHANCED_MENU_CLICK_SWITCH, 0, 0,
            out_val, "Inventory");
        if (drop_click_mode)
            enhanced_menu_highlight_prompt_token(out_val, "drop", TERM_YELLOW);

        bool allow_compare = (current_menu_command == 'u' || current_menu_command == 'x');
        
        log_debug("show_inven_enhanced: current_menu_command='%c' (%d), allow_compare=%d", 
            current_menu_command, (int)current_menu_command, allow_compare);

        int compare_count = 0;
        char compare_label[MAX_COMPARE_LINES][4];
        char compare_prefix[MAX_COMPARE_LINES][20];
        char compare_desc[MAX_COMPARE_LINES][80];
        byte compare_attr[MAX_COMPARE_LINES];
        bool compare_has_weight[MAX_COMPARE_LINES];
        int compare_weight[MAX_COMPARE_LINES];
        object_type* compare_obj[MAX_COMPARE_LINES]; /* Track object pointers for tile display */

        for (int c = 0; c < MAX_COMPARE_LINES; c++)
        {
            compare_label[c][0] = '\0';
            compare_prefix[c][0] = '\0';
            compare_desc[c][0] = '\0';
            compare_attr[c] = TERM_SLATE;
            compare_has_weight[c] = false;
            compare_weight[c] = 0;
            compare_obj[c] = NULL;
        }

        /* Surgical pre-clear: only rows that changed (old highlight + its compare lines, new highlight region, trailing cleared rows).
           This avoids full-frame flicker and keeps ordering erase -> print -> redraw. */
        int redraw_y1 = -1, redraw_y2 = -1; /* aggregate redraw bounds */
        if (use_story_font && allow_compare && !first_render)
        {
            int label_col_tmp = label_col_base;
            const int label_width_tmp = 4;
            int max_print_col = label_col_tmp + label_width_tmp;
            int erase_w = (max_print_col > col) ? (max_print_col - col + 1) : 0;
            if (story_term_w > 80) erase_w = (erase_w * story_term_w) / 80;

            /* Clear previous highlight block */
            if (previous_highlight_row >= 0 && previous_highlight_row < k)
            {
                int base = 1 + previous_highlight_row - previous_scroll_top;
                for (int r = 0; base >= 1 && r <= previous_compare_count; ++r)
                {
                    int rr = base + r;
                    if (rr > visible_rows)
                        break;
                    if (erase_w > 0) Term_erase(col, rr, erase_w);
                    if (show_weights) Term_erase(weight_col, rr, 9);
                    if (redraw_y1 < 0 || rr < redraw_y1) redraw_y1 = rr;
                    if (redraw_y2 < 0 || rr > redraw_y2) redraw_y2 = rr;
                }
            }
            /* Clear current highlight prospective block */
            if (highlight_active && highlight_row >= 0 && highlight_row < k)
            {
                int base = 1 + highlight_row - scroll_top;
                for (int r = 0; base >= 1 && r <= MAX_COMPARE_LINES; ++r)
                {
                    int rr = base + r;
                    if (rr > visible_rows)
                        break;
                    if (erase_w > 0) Term_erase(col, rr, erase_w);
                    if (show_weights) Term_erase(weight_col, rr, 9);
                    if (redraw_y1 < 0 || rr < redraw_y1) redraw_y1 = rr;
                    if (redraw_y2 < 0 || rr > redraw_y2) redraw_y2 = rr;
                }
            }
        }

        if (allow_compare && highlight_active && highlight_row >= 0 && highlight_row < k)
        {
            bool highlighted_is_supply = out_is_supply[highlight_row];
            object_type* highlighted_obj = NULL;

            log_debug("COMPARE SETUP CHECK: highlight_row=%d, is_supply=%d, is_floor=%d", 
                highlight_row, highlighted_is_supply, out_is_floor[highlight_row]);

            if (!highlighted_is_supply)
            {
                highlighted_obj = out_is_floor[highlight_row]
                    ? &o_list[0 - out_index[highlight_row]]
                    : &inventory[out_index[highlight_row]];
                    
                log_debug("COMPARE SETUP: highlighted_obj=%p, k_idx=%d", 
                    highlighted_obj, highlighted_obj ? highlighted_obj->k_idx : 0);
            }

            if (highlighted_obj)
            {
                int slot_candidates[MAX_COMPARE_LINES];
                int slot_count = 0;

                int primary_slot = wield_slot(highlighted_obj);
                append_compare_slot(slot_candidates, &slot_count, primary_slot);

                if (highlighted_obj->tval == TV_RING)
                {
                    append_compare_slot(slot_candidates, &slot_count, INVEN_LEFT);
                    append_compare_slot(slot_candidates, &slot_count, INVEN_RIGHT);
                }
                else if (highlighted_obj->tval == TV_ARROW)
                {
                    append_compare_slot(slot_candidates, &slot_count, INVEN_QUIVER1);
                    append_compare_slot(slot_candidates, &slot_count, INVEN_QUIVER2);
                }

                for (int idx = 0; idx < slot_count; idx++)
                {
                    int slot = slot_candidates[idx];

                    strnfmt(compare_label[idx], sizeof(compare_label[idx]), "%c", index_to_label(slot));
                    strnfmt(compare_prefix[idx], sizeof(compare_prefix[idx]), "%-12s: ", mention_use(slot));

                    /* Calculate limit based on actual column positions
                     * Text starts at col + 12 + 2 (after prefix "%-12s: ")
                     * Plus 2-3 more if tile is drawn (assume worst case: +3)
                     * Text ends at column 70 (weight) or 71 (no weight)
                     * Use conservative limit to prevent overflow
                     * For story font, reduce by 20% to account for proportional spacing */
                    int text_start_col = col + 12 + 2 + 3;  /* +3 for tile worst case */
                    int text_end_col = show_weights ? weight_col : label_col_base;
                    int compare_lim = text_end_col - text_start_col;
                    /* We'll enforce the exact visual width at render time; 
                       keep character truncation only as a hard ceiling */
                    
                    if (compare_lim < 0)
                        compare_lim = 0;
                    if (compare_lim >= (int)sizeof(compare_desc[idx]))
                        compare_lim = (int)sizeof(compare_desc[idx]) - 1;

                    object_type* equipped_obj = &inventory[slot];
                    if (equipped_obj->k_idx)
                    {
                        compare_obj[idx] = equipped_obj; /* Store for tile display */
                        object_desc(compare_desc[idx], sizeof(compare_desc[idx]), equipped_obj, true, 3);
                        compare_desc[idx][compare_lim] = '\0';
                        log_debug("COMPARE SETUP slot=%d: orig_len=%d, compare_lim=%d, truncated='%s'",
                            slot, (int)strlen(compare_desc[idx]), compare_lim, compare_desc[idx]);
                        compare_attr[idx] = weapon_glows(equipped_obj) 
                            ? object_display_color(equipped_obj, TERM_L_BLUE)
                            : object_display_color(equipped_obj, tval_to_attr[equipped_obj->tval % N_ELEMENTS(tval_to_attr)]);
                        if (show_weights && equipped_obj->weight)
                        {
                            compare_has_weight[idx] = true;
                            compare_weight[idx] = equipped_obj->weight * equipped_obj->number;
                        }
                    }
                    else
                    {
                        compare_obj[idx] = NULL; /* No object for empty slots */
                        cptr empty_text = describe_empty_slot(slot);
                        SDL_strlcpy(compare_desc[idx], empty_text, sizeof(compare_desc[idx]));
                        if (compare_lim < (int)sizeof(compare_desc[idx]))
                            compare_desc[idx][compare_lim] = '\0';
                        compare_attr[idx] = TERM_SLATE;
                    }
                }

                compare_count = slot_count;
                log_debug("COMPARE SETUP COMPLETE: compare_count=%d", compare_count);
            }
            else
            {
                log_debug("COMPARE SETUP SKIPPED: highlighted_obj is NULL");
            }
        }

        scroll_top = inventory_menu_scroll_to_selection(scroll_top,
            highlight_row, k, visible_rows, allow_compare ? compare_count : 0);

        int clear_rows = 0;
        for (int row_idx = scroll_top; row_idx < k && clear_rows < visible_rows;
             row_idx++)
        {
            clear_rows++;
            if (allow_compare && compare_count > 0 && row_idx == highlight_row)
            {
                int compare_rows = compare_count;
                if (clear_rows + compare_rows > visible_rows)
                    compare_rows = visible_rows - clear_rows;
                if (compare_rows > 0)
                    clear_rows += compare_rows;
            }
        }
        if (previous_total_rows > clear_rows)
            clear_rows = previous_total_rows;
        if (clear_rows > visible_rows)
            clear_rows = visible_rows;

        if (clear_rows > 0)
        {
            for (int clear_row = 1; clear_row <= clear_rows; clear_row++)
            {
                if (use_story_font)
                {
                    int erase_w = 255;
                    int weight_erase_w = 9;
                    if (story_term_w > 80)
                    {
                        erase_w = (erase_w * story_term_w) / 80;
                        weight_erase_w = (weight_erase_w * story_term_w) / 80;
                    }
                    Term_erase(col, clear_row, erase_w);
                    if (show_weights)
                        Term_erase(weight_col, clear_row, weight_erase_w);
                }
                else
                {
                    prt("", clear_row, col);
                    if (show_weights)
                        prt("", clear_row, weight_col);
                }
            }
        }

        /* Render combined list with floor entries first */
        int next_row = 1;
        for (int j = scroll_top; j < k && next_row <= visible_rows; j++)
        {
            bool is_floor_item = out_is_floor[j];
            bool is_supply_item = out_is_supply[j];
            object_type* line_obj = NULL;
            object_type supply_icon;
            object_type* display_obj = NULL;
            bool is_highlight = highlight_active && (highlight_row == j);
            byte line_attr = is_highlight ? TERM_L_BLUE : out_color[j];
            int row = next_row;

            if (is_floor_item)
                line_obj = &o_list[0 - out_index[j]];
            else if (!is_supply_item)
                line_obj = &inventory[out_index[j]];
            display_obj = is_supply_item
                ? prepare_supply_icon_object(&supply_icon)
                : line_obj;

            int label_col = label_col_base;

            ui_menu_click_add(j, col, row, term_wid - col);

            if (use_story_font)
            {
                /* Pre-clear handles row erasing, just do highlighting */
                if (!allow_compare)
                {
                    /* Only erase if comparison is disabled (no pre-clear) */
                    int erase_w = 255;
                    if (story_term_w > 80) erase_w = (erase_w * story_term_w) / 80;
                    Term_erase(col, row, erase_w);
                }
                if (is_highlight)
                {
                    int highlight_cols = term_wid;
                    story_fill_rect(row, col, highlight_cols - col, TERM_L_BLUE);
                }
            }
            else
            {
                prt("", row, col);
            }

            /* Draw tile if in graphics mode. */
            int text_col = col;
            int tile_col = text_col;
            if (display_obj && display_obj->k_idx)
            {
                text_col = draw_item_tile(col, row, display_obj);
            }
            if (is_highlight && square_selection)
                draw_inventory_selection_frame(tile_col, row, display_obj);
            
            log_trace("ITEM ROW %d: col=%d, text_col=%d, is_highlight=%d, desc='%.30s'",
                row, col, text_col, is_highlight, out_desc[j]);

            if (use_story_font)
            {
                int desc_limit = menu_desc_limit(text_col, label_col,
                    weight_col, show_weights);
                
                /* Convert limit from mono columns to story font cells */
                if (story_term_w > 80) {
                    desc_limit = (desc_limit * story_term_w) / 80;
                }
                
                log_trace("ITEM ROW %d STORY: desc_limit=%d (scaled), text_len=%d",
                    row, desc_limit, (int)strlen(out_desc[j]));
                story_print_text(row, text_col, desc_limit, line_attr, out_desc[j]);
            }
            else
            {
                c_put_str(line_attr, out_desc[j], row, text_col);
            }

            if (show_weights)
            {
                int wgt = 0;
                if (is_supply_item)
                    wgt = supplies_limit_weight();
                else if (line_obj)
                    wgt = line_obj->weight * line_obj->number;
                strnfmt(tmp_val, sizeof(tmp_val), "%2d.%1d lb", wgt / 10, wgt % 10);
                if (use_story_font)
                {
                    int weight_width = label_col - weight_col;
                    if (weight_width < 1)
                        weight_width = 1;
                    story_print_text_grid(row, weight_col, weight_width, line_attr, tmp_val);
                }
                else
                    c_put_str(line_attr, tmp_val, row, weight_col);
            }

            /* Print the item letter at the end */
            if (is_floor_item)
                strnfmt(tmp_val, sizeof(tmp_val), " (-)");
            else if (is_supply_item)
            {
                char label = supplies_label_char();
                int slot = supplies_virtual_slot();
                if (!label && slot >= 0)
                    label = index_to_label(slot);
                if (!label)
                    label = 'a';
                strnfmt(tmp_val, sizeof(tmp_val), "(%c)", label);
            }
            else
                strnfmt(tmp_val, sizeof(tmp_val), "(%c)", index_to_label(out_index[j]));

            const int label_width = 4;
            byte label_attr = is_highlight ? TERM_L_BLUE : TERM_WHITE;
            log_trace("ITEM RENDER row=%d: label_col=%d, show_weights=%d, label='%s'", 
                row, label_col, show_weights, tmp_val);
            if (use_story_font)
            {
                story_print_text_grid(row, label_col, label_width,
                    label_attr, tmp_val);
            }
            else
            {
                if (is_highlight)
                    c_put_str(label_attr, tmp_val, row, label_col);
                else
                    put_str(tmp_val, row, label_col);
            }

            next_row++;
            
            if (j == 3) {
                log_debug("AT ITEM j=3: compare_count=%d, highlight_row=%d, j==highlight_row=%d", 
                    compare_count, highlight_row, (j == highlight_row));
            }
            
            log_trace("CHECKING COMPARE RENDER: j=%d, compare_count=%d, highlight_row=%d, condition=%d",
                j, compare_count, highlight_row, (compare_count > 0 && j == highlight_row));

            if (compare_count > 0 && j == highlight_row)
            {
                log_trace(">>> INSIDE IF BLOCK: About to render %d comparison lines at next_row=%d", 
                    compare_count, next_row);
                    
                for (int idx = 0; idx < compare_count && next_row <= visible_rows; idx++)
                {
                    int compare_row = next_row;
                    
                    log_trace("COMPARE LINE idx=%d will render at row=%d", idx, compare_row);

                    /* Pre-clear handles row erasing for comparison mode */
                    if (use_story_font && !allow_compare) {
                        int erase_w = 255;
                        if (story_term_w > 80) erase_w = (erase_w * story_term_w) / 80;
                        Term_erase(col, compare_row, erase_w);
                    }
                    else if (!use_story_font)
                        prt("", compare_row, col);

                    if (use_story_font)
                        story_print_equipment_prefix(compare_row, col, TERM_WHITE, compare_prefix[idx]);
                    else
                        c_put_str(TERM_WHITE, compare_prefix[idx], compare_row, col);

                    /* Draw tile if in graphics mode for equipped items */
                    int compare_text_col = col + 12 + 2;
                    if (compare_obj[idx] && compare_obj[idx]->k_idx)
                    {
                        compare_text_col = draw_item_tile(col + 12 + 2, compare_row, compare_obj[idx]);
                    }
                    
                    log_debug("COMPARE RENDER idx=%d row=%d: col=%d, text_col=%d, desc='%s'",
                        idx, compare_row, col, compare_text_col, compare_desc[idx]);

                    if (use_story_font)
                    {
                        /* Bound the story-font rendering to the available grid width,
                           scaled to story cells, so we can safely show more text. */
                        int text_end_col = show_weights ? weight_col : label_col;
                        int desc_limit = text_end_col - compare_text_col;
                        if (desc_limit < 1) desc_limit = 1;
                        if (story_term_w > 80) desc_limit = (desc_limit * story_term_w) / 80;
                        log_debug("COMPARE RENDER STORY: text_col=%d, desc_limit=%d, text_len=%d",
                            compare_text_col, desc_limit, (int)strlen(compare_desc[idx]));
                        story_print_text(compare_row, compare_text_col, desc_limit, compare_attr[idx], compare_desc[idx]);
                    }
                    else
                    {
                        c_put_str(compare_attr[idx], compare_desc[idx], compare_row, compare_text_col);
                    }

                    if (show_weights)
                    {
                        if (compare_has_weight[idx])
                        {
                            strnfmt(tmp_val, sizeof(tmp_val), "%2d.%1d lb", compare_weight[idx] / 10, compare_weight[idx] % 10);
                            if (use_story_font)
                            {
                                int compare_weight_width = label_col - weight_col;
                                if (compare_weight_width < 1)
                                    compare_weight_width = 1;
                                story_print_text_grid(compare_row, weight_col, compare_weight_width, compare_attr[idx], tmp_val);
                            }
                            else
                                c_put_str(compare_attr[idx], tmp_val, compare_row, weight_col);
                        }
                        else
                        {
                            if (use_story_font)
                                Term_erase(weight_col, compare_row, 9);
                            else
                                prt("", compare_row, weight_col);
                        }
                    }

                    /* Print the item letter at the end of compare line */
                    if (compare_label[idx][0])
                    {
                        char label_str[8];
                        strnfmt(label_str, sizeof(label_str), "(%s)", compare_label[idx]);
                        if (use_story_font)
                            story_print_text_grid(compare_row, label_col,
                                label_width, compare_attr[idx], label_str);
                        else
                            c_put_str(compare_attr[idx], label_str, compare_row, label_col);
                    }

                    next_row++;
                }
            }
        }

        int total_rows = next_row - 1;

        /* End-of-frame redraw of just changed rows */
        if (use_story_font && allow_compare)
        {
            if (compare_count > 0 && highlight_row >= 0 && highlight_row < k)
            {
                int base = 1 + highlight_row - scroll_top;
                /* Redraw all items from highlighted row to end, since comparison lines shift everything down */
                int last = total_rows;
                if (base < 1)
                    base = 1;
                if (last > visible_rows)
                    last = visible_rows;
                if (redraw_y1 < 0 || base < redraw_y1) redraw_y1 = base;
                if (redraw_y2 < 0 || last > redraw_y2) redraw_y2 = last;
            }
            /* Include trailing cleared rows if list shrank */
            if (total_rows < previous_total_rows)
            {
                int shrink_start = total_rows + 1;
                int shrink_end = previous_total_rows;
                if (redraw_y1 < 0 || shrink_start < redraw_y1) redraw_y1 = shrink_start;
                if (redraw_y2 < 0 || shrink_end > redraw_y2) redraw_y2 = shrink_end;
            }
            if (redraw_y1 > 0 && redraw_y2 >= redraw_y1)
            {
                int max_col = label_col_base + 4;
                if (max_col > Term->wid - 1) max_col = Term->wid - 1;
                Term_redraw_section(col, redraw_y1, max_col, redraw_y2);
            }
        }

        if (total_rows < previous_total_rows)
        {
            int clear_col = col;
            for (int clear_row = total_rows + 1;
                 clear_row <= previous_total_rows && clear_row <= visible_rows;
                 clear_row++)
            {
                if (use_story_font)
                {
                    int erase_w = 255;
                    int weight_erase_w = 9;
                    if (story_term_w > 80) {
                        erase_w = (erase_w * story_term_w) / 80;
                        weight_erase_w = (weight_erase_w * story_term_w) / 80;
                    }
                    Term_erase(col, clear_row, erase_w);
                    if (show_weights)
                        Term_erase(weight_col, clear_row, weight_erase_w);
                }
                else
                {
                    prt("", clear_row, clear_col);
                    if (show_weights)
                        prt("", clear_row, weight_col);
                }
            }
        }

    previous_total_rows = total_rows;
    previous_compare_count = compare_count;
    previous_highlight_row = highlight_row;
    previous_scroll_top = scroll_top;
    first_render = false;  /* subsequent frames use surgical region */

        if (highlight_active && highlight_row >= 0 && highlight_row < k)
        {
            const char* row_type = out_is_floor[highlight_row] ? "floor"
                : (out_is_supply[highlight_row] ? "supply" : "inventory");
            log_debug("show_inven_enhanced: Highlighted row %d (%s)", highlight_row, row_type);
        }
        
        /* Get a key */
        which = inkey();

        {
            int clicked_row = -1;
            int click_action = UI_MENU_CLICK_PRIMARY;

            bool click_taken =
                ui_menu_click_take_action(&clicked_row, &click_action);

            if (click_taken && clicked_row == ENHANCED_MENU_CLICK_SWITCH)
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                done = true;
                enhanced_menu_action = ENHANCED_ACTION_SWITCH;
                continue;
            }

            if (click_taken && clicked_row == ENHANCED_MENU_CLICK_DROP)
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                drop_click_mode = !drop_click_mode;
                continue;
            }

            if (click_taken && clicked_row >= 0 && clicked_row < k)
            {
                highlight_row = clicked_row;
                highlight_active = true;
                enhanced_inventory_selected_item = out_index[highlight_row];

                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;

                if (click_action == UI_MENU_CLICK_SECONDARY)
                {
                    done = true;
                    enhanced_menu_action = out_is_supply[highlight_row]
                        ? ENHANCED_ACTION_SUPPLIES
                        : ENHANCED_ACTION_EXAMINE;
                }
                else if (drop_click_mode)
                {
                    if (out_is_floor[highlight_row])
                    {
                        bell("Cannot drop floor items!");
                        continue;
                    }
                    if (!death_spectator_allow_menu_action())
                        continue;

                    done = true;
                    enhanced_menu_action = ENHANCED_ACTION_DROP;
                }
                else
                {
                    int primary_action = enhanced_menu_primary_select_action();

                    if (primary_action == ENHANCED_ACTION_USE
                        && !death_spectator_allow_menu_action())
                        continue;

                    done = true;
                    enhanced_menu_action = primary_action;
                }

                continue;
            }
            if (click_taken && click_action == UI_MENU_CLICK_HOVER)
                continue;
            if (!click_taken && which == UI_MENU_CLICK_WAKE_KEY)
                continue;
        }
        
        log_trace("show_inven_enhanced: Key pressed: %d ('%c')", which, (which >= 32 && which <= 126) ? which : '?');
        
        /* Parse it */
        switch (which)
        {
        case ESCAPE:
            log_trace("show_inven_enhanced: ESC pressed, setting action to 0 and exiting");
            enhanced_menu_action = ENHANCED_ACTION_NONE;  /* Explicitly set to exit */
            done = true;
            break;
            
        case 'i':
            if ((current_menu_command == 0) && inventory_menu_same_button_cycle_enabled()) {
                enhanced_menu_action = ENHANCED_ACTION_SWITCH;
                log_trace("show_inven_enhanced: Direct access I key - same-button cycle to equipment (action=1)");
                done = true;
                break;
            }

            /* Already in inventory */
            break;
            
        case 'e':
            /* Handle E key based on access mode and portable UI support */
            {
                extern char current_menu_command;
                if (current_menu_command != 0) {
                    /* Command access (u/x pressed) */
                    if (controller_controls) {
                        /* Controller UI: E/I switch menus */
                        enhanced_menu_action = ENHANCED_ACTION_SWITCH;
                        done = true;
                    } else {
                        /* Keyboard-only: E/I are just letters, not menu switching */
                        /* Fall through to default letter handling */
                        goto default_case;
                    }
                } else {
                    /* Direct access (i/e pressed) */
                    if (controller_controls) {
                        /* Controller UI: E/I switch menus */
                        enhanced_menu_action = ENHANCED_ACTION_SWITCH;
                        log_trace("show_inven_enhanced: Direct access E key - switching to equipment (action=1)");
                        done = true;
                    } else {
                        /* Keyboard-only: E/I are just letters */
                        goto default_case;
                    }
                }
            }
            break;
            
        /* Handle cycling when the original command is pressed */
        case 'u':
            if (current_menu_command == which) {
                /* Same command - cycle to equipment */
                enhanced_menu_action = ENHANCED_ACTION_SWITCH;
                log_trace("show_inven_enhanced: Command cycling (%c) - switching to equipment (action=1)", which);
                done = true;
            }
            /* Different command does nothing */
            break;

        case 'x':
            if (current_menu_command == which) {
                /* Same command - cycle to equipment */
                enhanced_menu_action = ENHANCED_ACTION_SWITCH;
                log_trace("show_inven_enhanced: Command cycling (%c) - switching to equipment (action=1)", which);
                done = true;
            } else if (controller_controls) {
                if (highlight_active && highlight_row >= 0 && highlight_row < k) {
                    enhanced_menu_action = ENHANCED_ACTION_EXAMINE;
                    enhanced_inventory_selected_item = out_index[highlight_row];
                    done = true;
                }
            }
            break;
            
        /* Toggle keys - switch to equipment */
        case '/':           /* / - toggle to equipment */
        case KTRL('I'):     /* Ctrl+I - toggle to equipment */
        case KTRL('E'):     /* Ctrl+E - toggle to equipment */
            enhanced_menu_action = ENHANCED_ACTION_SWITCH;
            done = true;
            break;
            
        case '8':
            if (highlight_active && k > 0) {
                highlight_row = (highlight_row + k - 1) % k;
            }
            break;
            
        case '2':
            if (highlight_active && k > 0) {
                highlight_row = (highlight_row + 1) % k;
            }
            break;
            
        case ' ':        /* Space - use item or examine based on context */
        case '\r':       /* Enter - use item or examine based on context */
        case '\n':       /* Enter (alternative) - use item or examine based on context */
            if (highlight_active && highlight_row >= 0 && highlight_row < k) {
                extern char current_menu_command;
                if (current_menu_command == 'x') {
                    /* Examine mode - mark for examination */
                    done = true;
                    enhanced_menu_action = ENHANCED_ACTION_EXAMINE;
                    enhanced_inventory_selected_item = out_index[highlight_row];
                } else {
                    if (!death_spectator_allow_menu_action())
                        break;

                    /* Use mode - defer use until after menu restore */
                    done = true;
                    enhanced_menu_action = ENHANCED_ACTION_USE;
                    enhanced_inventory_selected_item = out_index[highlight_row];
                }
            }
            break;

        case '6':        /* Arrow right - description */
            if (highlight_active && highlight_row >= 0 && highlight_row < k) {
                /* Mark that we want to examine an item */
                enhanced_menu_action = ENHANCED_ACTION_EXAMINE;
                enhanced_inventory_selected_item = out_index[highlight_row];
                done = true;
            }
            break;
            
        case '4':        /* Arrow left - drop item */
            if (highlight_active && highlight_row >= 0 && highlight_row < k) {
                if (!out_is_floor[highlight_row]) {
                    if (!death_spectator_allow_menu_action())
                        break;

                    /* Can only drop inventory items, not floor items */
                    done = true;
                    enhanced_menu_action = ENHANCED_ACTION_DROP;
                    enhanced_inventory_selected_item = out_index[highlight_row];
                } else {
                    bell("Cannot drop floor items!");
                }
            }
            break;
            
            default:
            default_case:
            /* Handle item selection by letter or dash */
            if ((which >= 'a' && which <= 'z') || (which >= 'A' && which <= 'Z') || which == '-') {
                extern char current_menu_command;
                bool allow_letters = false;
                
                /* Letter selection is hidden/disabled only in controller UI mode. */
                allow_letters = !controller_controls;
                if (!allow_letters) {
                    bell("Use arrow keys and Space to select items in this mode");
                    break;
                }
                if (death_spectator_active()) {
                    death_spectator_allow_menu_action();
                    break;
                }
                bool item_found = false;
                
                /* Check for dash (-) which selects floor item */
                if (which == '-' && has_floor_items) {
                    /* Find the floor item in our display list */
                    for (i = 0; i < k; i++) {
                        if (out_is_floor[i]) {
                            done = true;
                            item_found = true;
                            
                            /* Handle based on menu context */
                            extern char current_menu_command;
                            if (current_menu_command != 0) {
                                /* Command mode - defer action */
                                if (current_menu_command == 'u') {
                                    if (!death_spectator_allow_menu_action()) {
                                        item_found = true;
                                        done = false;
                                        break;
                                    }
                                    enhanced_menu_action = ENHANCED_ACTION_USE;
                                    enhanced_inventory_selected_item = out_index[i];
                                } else if (current_menu_command == 'x') {
                                    /* For observe mode, mark for examination */
                                    enhanced_menu_action = ENHANCED_ACTION_EXAMINE;
                                    enhanced_inventory_selected_item = out_index[i];
                                }
                            } else {
                                /* Direct access mode - use the floor item directly */
                                if (!death_spectator_allow_menu_action()) {
                                    item_found = true;
                                    done = false;
                                } else {
                                    enhanced_menu_action = ENHANCED_ACTION_USE;
                                    enhanced_inventory_selected_item = out_index[i];
                                }
                            }
                            break;
                        }
                    }
                }
                
                /* If no floor item found, check inventory items by letter */
                if (!item_found && which != '-') {
                    int item = label_to_inven(which);
                    if (item >= 0 && (inventory[item].k_idx || (throw_slot_menu_active && throw_slot_enabled[item]))) {
                        /* Check if this inventory item is in our display list */
                        for (i = 0; i < k; i++) {
                            if (!out_is_floor[i] && out_index[i] == item) {
                                done = true;
                                item_found = true;
                                
                                /* Handle based on menu context */
                                extern char current_menu_command;
                                if (current_menu_command != 0) {
                                    /* Command mode - defer action */
                                    if (current_menu_command == 'u') {
                                        if (!death_spectator_allow_menu_action()) {
                                            item_found = true;
                                            done = false;
                                            break;
                                        }
                                        enhanced_menu_action = ENHANCED_ACTION_USE;
                                        enhanced_inventory_selected_item = item;
                                    } else if (current_menu_command == 'x') {
                                        /* For observe mode, mark for examination */
                                        enhanced_menu_action = ENHANCED_ACTION_EXAMINE;
                                        enhanced_inventory_selected_item = item;
                                    }
                                } else {
                                    /* Direct access mode - use the traditional method */
                                    if (!death_spectator_allow_menu_action()) {
                                        item_found = true;
                                        done = false;
                                    } else {
                                        p_ptr->command_new = which;
                                        p_ptr->command_see = true;
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
                
                if (!item_found) {
                    bell("Illegal object choice!");
                }
            }
            else {
                bell("Invalid command!");
            }
            break;
        }
    }
    
    (void)Term_set_extra_cursor(false, 0, 0, false);
    ui_menu_click_clear();
    hide_cursor = saved_hide_cursor;
    (void)Term_set_cursor(saved_cursor);
    story_font_term_pop(&story_state);
    log_trace("show_inven_enhanced: Exiting, action=%d", enhanced_menu_action);
}

/* Global variables for equipment menu switching */
int enhanced_equip_action = ENHANCED_ACTION_NONE;
int enhanced_equipment_selected_item = ENHANCED_MENU_NO_SELECTION;

/*
 * Enhanced equipment display with scrolling and navigation
 * EXACTLY replicates show_equip() algorithm then adds highlighting
 * Only navigates through actually equipped items
 */
void show_equip_enhanced(void)
{
    int which;
    bool done = false;
    bool saved_hide_cursor = hide_cursor;
    bool saved_cursor = false;
    char out_val[160];
    bool drop_click_mode = false;
    
    log_debug("show_equip_enhanced: Starting equipment enhanced menu");
    log_debug("show_equip_enhanced: INVEN_BODY=%d, INVEN_FEET=%d, show_weights=%d", 
        INVEN_BODY, INVEN_FEET, show_weights);
    
    bool use_story_font = story_equipment_enabled();
    story_font_term_state story_state;
    int story_term_w = 0;
    story_equipment_list_active = use_story_font;
    story_font_term_push(use_story_font, false, &story_state);
    if (use_story_font) {
        int story_term_h = 0;
        Term_get_size(&story_term_w, &story_term_h);
    }

    (void)Term_get_cursor(&saved_cursor);
    hide_cursor = true;
    (void)Term_set_cursor(false);

    /* Variables exactly matching show_equip() */
    int i, k, l;
    int col, len, lim;
    int clear_col;
    int term_wid = menu_term_width();
    int term_hgt = (Term && Term->hgt > 0) ? Term->hgt : 24;
    int weight_col = menu_weight_col_for_width(term_wid);
    int label_col_base = menu_label_col_for_width(term_wid, show_weights);
    int highlight_index = -1;  /* Index in the equipped items array */
    bool highlight_active = false;
    
    object_type* o_ptr;
    char tmp_val[80];
    char o_name[80];
    
    /* Arrays exactly matching show_equip() */
    int out_index[24];        /* Slot numbers of equipped items */
    byte out_color[24];
    char out_desc[24][80];
    int armour_weight = 0;    /* Total armour weight */
    
    /* Default length (exactly like show_equip) */
    len = 29;
    
    /* Maximum space allowed for descriptions (exactly like show_equip) */
    lim = term_wid - 3;
    if (lim < 0)
        lim = 0;
    
    /* Require space for labels (exactly like show_equip) */
    lim -= (14 + 2);
    
    /* Require space for weight if needed (exactly like show_equip) */
    if (show_weights) lim -= 9;

    if (lim < 0)
        lim = 0;
    
    /* Scan the equipment list for display. Empty slots are shown when allowed. */
    log_debug("show_equip_enhanced: Starting equipment scan, show_weights=%d", show_weights);
    for (k = 0, i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];
        bool is_empty = !o_ptr->k_idx;
        
        if (!item_tester_okay(o_ptr)) continue;
        
        if (is_empty)
        {
            SDL_strlcpy(o_name, describe_empty_slot(i), sizeof(o_name));
            out_color[k] = TERM_L_DARK;
        }
        else
        {
            /* Description (exactly like show_equip) */
            object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
            out_color[k] = object_display_color(o_ptr, tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);
        }
        
        /* Truncate the description (exactly like show_equip) */
        o_name[lim] = 0;
        
        /* Save the index (exactly like show_equip) */
        out_index[k] = i;
        
        /* Save the description (exactly like show_equip) */
        SDL_strlcpy(out_desc[k], o_name, sizeof(out_desc[0]));
        
        /* Calculate armour weight (for body armour, cloak, shield, helmet, gloves, boots) */
        if (show_weights && o_ptr->weight && (i >= INVEN_BODY) && (i <= INVEN_FEET))
        {
            int item_weight = o_ptr->weight * o_ptr->number;
            armour_weight += item_weight;
            log_debug("show_equip_enhanced: Slot %d (%s) weight=%d, total armour_weight now=%d", 
                i, describe_empty_slot(i), item_weight, armour_weight);
        }
        else if (i >= INVEN_BODY && i <= INVEN_FEET)
        {
            log_trace("show_equip_enhanced: Slot %d (%s) skipped: show_weights=%d, o_ptr->weight=%d, is_empty=%d",
                i, describe_empty_slot(i), show_weights, o_ptr->weight, is_empty);
        }
        
        /* Extract the maximal length (exactly like show_equip) */
        l = strlen(out_desc[k]) + (2 + 3);
        
        /* Increase length for labels (exactly like show_equip) */
        l += (12 + 2);
        
        /* Increase length for weight if needed (exactly like show_equip) */
        if (show_weights) l += 9;
        
        /* Maintain the max-length (exactly like show_equip) */
        if (l > len) len = l;
        
        /* Advance the entry (exactly like show_equip) */
        k++;
    }
    
    log_debug("show_equip_enhanced: Equipment scan complete, k=%d items, total armour_weight=%d", k, armour_weight);
    
    /* Find the column to start in (exactly like show_equip) */
    col = menu_center_col_for_len(term_wid, len);
    clear_col = menu_overlay_clear_col(col);
    
    log_debug("show_equip_enhanced: k=%d equipped items, len=%d, col=%d", k, len, col);
    
    /* Enable highlight if there are occupied rows to navigate. */
    highlight_index = equipment_first_occupied_row(k, out_index);
    if (highlight_index >= 0) {
        highlight_active = true;
    }
    
    /* Main interaction loop */
    while (!done)
    {
        (void)Term_set_extra_cursor(false, 0, 0, false);
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
        bool square_selection = inventory_selection_uses_square();

        /* Display equipment list */
        if (use_story_font)
        {
            log_trace("show_equip_enhanced: Calling draw_equipment_story_rows with highlight_index=%d", highlight_index);
            draw_equipment_story_rows(col, k, out_index, out_color, out_desc,
                highlight_active, highlight_index, show_weights, story_term_w);
            log_trace("show_equip_enhanced: draw_equipment_story_rows FINISHED");
            
            /* Display armour weight total if any armour equipped */
            log_debug("show_equip_enhanced: Checking armour weight display: armour_weight=%d", armour_weight);
            if (armour_weight)
            {
                int divider_row;
                int text_row;

                equipment_weight_layout_rows(1, k, term_hgt, &divider_row,
                    &text_row);
                if ((k + 1) < term_hgt)
                    Term_erase(clear_col, k + 1, 255);
                if ((k + 2) < term_hgt)
                    Term_erase(clear_col, k + 2, 255);

                log_debug("show_equip_enhanced: Displaying armour weight at rows %d/%d (k=%d)",
                    divider_row, text_row, k);

                if (divider_row >= 0)
                {
                    log_trace("show_equip_enhanced: Rendering armour divider at row %d",
                        divider_row);
                    story_print_text_grid(divider_row, weight_col, 8,
                        TERM_L_DARK, "--------");
                }
                strnfmt(tmp_val, sizeof(tmp_val), "armour: %3d.%1d lb",
                    armour_weight / 10, armour_weight % 10);
                log_debug("show_equip_enhanced: Armour weight text: '%s'", tmp_val);
                if (text_row >= 0)
                    story_print_text_grid(text_row, MAX(0, weight_col - 8), 16,
                        TERM_SLATE, tmp_val);
            }
            else
            {
                log_debug("show_equip_enhanced: NOT displaying armour weight (armour_weight=%d)", armour_weight);
                if (k && (k < term_hgt - 1))
                    Term_erase(clear_col, k + 1, 255);
            }
        }
        else
        {
            log_trace("show_equip_enhanced: Calling show_equip() [mono path]");
            show_equip();
            log_trace("show_equip_enhanced: show_equip() FINISHED");
        }
        
        /* Show the prompt - different text based on how menu was opened */
        extern char current_menu_command;
        const bool controller_controls = steamdeck_controls_active();
        const char* prompt_suffix = " (Equipped)";
        bool prompt_context_visible;
        if (controller_controls) {
            char confirm_label[16];
            char desc_label[16];
            char cycle_label[16];
            bool same_button_cycle = inventory_menu_same_button_cycle_enabled();

            inventory_prompt_label(' ', "A", confirm_label, sizeof(confirm_label));
            inventory_prompt_label('x', "RS Right", desc_label, sizeof(desc_label));

            if (current_menu_command == 'u') {
                inventory_prompt_label('u', "X", cycle_label, sizeof(cycle_label));
                strnfmt(out_val, sizeof(out_val),
                    "%s-remove  %s-desc  <- drop  %s-cycle%s",
                    confirm_label, desc_label, cycle_label, prompt_suffix);
            } else if (current_menu_command == 'x') {
                inventory_prompt_label('x', "RS Right", cycle_label, sizeof(cycle_label));
                strnfmt(out_val, sizeof(out_val),
                    "%s-remove  %s-desc  <- drop  %s-cycle%s",
                    confirm_label, desc_label, cycle_label, prompt_suffix);
            } else if (same_button_cycle) {
                inventory_prompt_label('e', "L1", cycle_label, sizeof(cycle_label));
                strnfmt(out_val, sizeof(out_val),
                    "%s-remove  %s-desc  <- drop  %s-cycle%s",
                    confirm_label, desc_label, cycle_label, prompt_suffix);
            } else {
                strnfmt(out_val, sizeof(out_val),
                    "%s-remove  %s-desc  <- drop%s",
                    confirm_label, desc_label, prompt_suffix);
            }
        } else if (current_menu_command == 'u') {
            sprintf(out_val,
                "Space-Remove, -> description, <- drop, %c again-cycle  (Equipped)",
                current_menu_command);
        }
        else if (current_menu_command == 'x') {
            sprintf(out_val,
                "Space-Remove, -> description, <- drop, %c again-cycle  (Equipped)",
                current_menu_command);
        }
        else {
            sprintf(out_val,
                "Space-Remove, -> description, <- drop  (Equipped)");
        }
        prompt_context_visible = menu_prompt_drop_suffix_if_wrapped(out_val,
            prompt_suffix, term_wid, use_story_font);
        prt(out_val, 0, 0);
        ui_menu_click_add_text_token(ENHANCED_MENU_CLICK_DROP, 0, 0,
            out_val, "drop");
        if (prompt_context_visible)
            ui_menu_click_add_text_token(ENHANCED_MENU_CLICK_SWITCH, 0, 0,
                out_val, "Equipped");
        if (drop_click_mode)
            enhanced_menu_highlight_prompt_token(out_val, "drop", TERM_YELLOW);
        
        /* Highlight current selection - find the display row for this equipped item */
        if (!use_story_font && highlight_active && highlight_index >= 0 && highlight_index < k)
        {
            /* Get the actual slot index of the highlighted equipped item */
            int highlighted_slot = out_index[highlight_index];
            
            log_trace("show_equip_enhanced: MONO highlight overlay - item %d (slot %d)", highlight_index, highlighted_slot);
            log_debug("show_equip_enhanced: Highlighting equipped item %d (slot %d)", highlight_index, highlighted_slot);
            
            int display_row = highlight_index + 1;
            
            if (display_row > 0)
            {
                /* Get the item */
                object_type* o_ptr = &inventory[highlighted_slot];
                
                log_debug("show_equip_enhanced: Found display row %d for slot %d", display_row, highlighted_slot);
                
                /* Clear the line (exactly like show_equip) */
                Term_erase(clear_col, display_row, 255);

                byte prefix_attr = TERM_L_BLUE;
                byte line_attr = TERM_L_BLUE;
                byte label_attr = TERM_L_BLUE;
                
                /* Mention the use (exactly like show_equip) */
                strnfmt(tmp_val, sizeof(tmp_val), "%-12s: ", mention_use(highlighted_slot));
                c_put_str(prefix_attr, tmp_val, display_row, col);
                
                /* Draw tile if in graphics mode */
                int text_col = col + 12 + 2;
                int tile_col = text_col;
                if (o_ptr->k_idx)
                {
                    text_col = draw_item_tile(col + 12 + 2, display_row, o_ptr);
                }
                if (square_selection)
                    draw_inventory_selection_frame(tile_col, display_row,
                        o_ptr->k_idx ? o_ptr : NULL);
                
                /* Display the entry itself (exactly like show_equip) */
                c_put_str(line_attr, out_desc[highlight_index], display_row, text_col);
                
                /* Display the weight if needed (exactly like show_equip) */
                if (show_weights && o_ptr->weight)
                {
                    int wgt = o_ptr->weight * o_ptr->number;
                    sprintf(tmp_val, "%3d.%1d lb", wgt / 10, wgt % 10);
                    c_put_str(line_attr, tmp_val, display_row, weight_col);
                }
                
                if (highlighted_slot == INVEN_QUIVER2)
                {
                    /* Account for potential tile offset when calculating note position */
                    int note_col = text_col + (int)strlen(out_desc[highlight_index]);
                    c_put_str(TERM_L_DARK, " (keeps passive bonuses)", display_row, note_col);
                }

                /* Print the item letter at the end with highlight */
                sprintf(tmp_val, " (%c)", index_to_label(highlighted_slot));
                int label_col = label_col_base;
                c_put_str(label_attr, tmp_val, display_row, label_col);
                
                log_debug("show_equip_enhanced: Drew highlight at display row %d, col %d", display_row, col);
            }
        }

        for (int click_i = 0; click_i < k; click_i++)
        {
            int click_slot = out_index[click_i];
            if (click_slot >= INVEN_WIELD && click_slot < INVEN_TOTAL
                && inventory[click_slot].k_idx)
            {
                ui_menu_click_add(click_i, col, click_i + 1, term_wid - col);
            }
        }
        
        /* Get a key */
        which = inkey();

        {
            int clicked_row = -1;
            int click_action = UI_MENU_CLICK_PRIMARY;

            bool click_taken =
                ui_menu_click_take_action(&clicked_row, &click_action);

            if (click_taken && clicked_row == ENHANCED_MENU_CLICK_SWITCH)
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                done = true;
                enhanced_equip_action = ENHANCED_ACTION_SWITCH;
                continue;
            }

            if (click_taken && clicked_row == ENHANCED_MENU_CLICK_DROP)
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                drop_click_mode = !drop_click_mode;
                continue;
            }

            if (click_taken && clicked_row >= 0 && clicked_row < k)
            {
                int clicked_slot = out_index[clicked_row];
                if (clicked_slot >= INVEN_WIELD && clicked_slot < INVEN_TOTAL
                    && inventory[clicked_slot].k_idx)
                {
                    highlight_index = clicked_row;
                    highlight_active = true;
                    enhanced_equipment_selected_item = clicked_slot;
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;

                    done = true;
                    enhanced_equip_action = (click_action == UI_MENU_CLICK_SECONDARY)
                        ? ENHANCED_ACTION_EXAMINE
                        : (drop_click_mode ? ENHANCED_ACTION_DROP
                                           : enhanced_menu_primary_select_action());
                    if ((enhanced_equip_action == ENHANCED_ACTION_USE
                            || enhanced_equip_action == ENHANCED_ACTION_DROP)
                        && !death_spectator_allow_menu_action())
                    {
                        done = false;
                        enhanced_equip_action = ENHANCED_ACTION_NONE;
                    }
                    continue;
                }
            }
            if (click_taken && click_action == UI_MENU_CLICK_HOVER)
                continue;
            if (!click_taken && which == UI_MENU_CLICK_WAKE_KEY)
                continue;
        }
        
        log_trace("show_equip_enhanced: Key pressed: %d ('%c')", which, (which >= 32 && which <= 126) ? which : '?');
        
        /* Parse it */
        switch (which)
        {
        case ESCAPE:
            log_trace("show_equip_enhanced: ESC pressed, setting action to 0 and exiting");
            enhanced_equip_action = ENHANCED_ACTION_NONE;  /* Explicitly set to exit */
            done = true;
            break;
        
        case 'e':
            if ((current_menu_command == 0) && inventory_menu_same_button_cycle_enabled()) {
                enhanced_equip_action = ENHANCED_ACTION_SWITCH;
                log_trace("show_equip_enhanced: Direct access E key - same-button cycle to inventory (action=1)");
                done = true;
                break;
            }

            /* Already in equipment */
            break;
        
        case 'i':
            /* Handle I key based on access mode and portable UI support */
            {
                extern char current_menu_command;
                if (current_menu_command != 0) {
                    /* Command access (u/x pressed) */
                    if (controller_controls) {
                        /* Controller UI: E/I switch menus */
                        enhanced_equip_action = ENHANCED_ACTION_SWITCH;
                        done = true;
                    } else {
                        /* Keyboard-only: E/I are just letters, not menu switching */
                        /* Fall through to default letter handling */
                        goto equip_default_case;
                    }
                } else {
                    /* Direct access (i/e pressed) */
                    if (controller_controls) {
                        /* Controller UI: E/I switch menus */
                        enhanced_equip_action = ENHANCED_ACTION_SWITCH;
                        log_trace("show_equip_enhanced: Direct access I key - switching to inventory (action=1)");
                        done = true;
                    } else {
                        /* Keyboard-only: E/I are just letters */
                        goto equip_default_case;
                    }
                }
            }
            break;
        
        /* Handle cycling when the original command is pressed */
        case 'u':
            if (current_menu_command == which) {
                /* Same command - cycle to inventory */
                enhanced_equip_action = ENHANCED_ACTION_SWITCH;
                log_trace("show_equip_enhanced: Command cycling (%c) - switching to inventory (action=1)", which);
                done = true;
            }
            /* Different command does nothing */
            break;

        case 'x':
            if (current_menu_command == which) {
                /* Same command - cycle to inventory */
                enhanced_equip_action = ENHANCED_ACTION_SWITCH;
                log_trace("show_equip_enhanced: Command cycling (%c) - switching to inventory (action=1)", which);
                done = true;
            } else if (controller_controls) {
                if (highlight_active && highlight_index >= 0 && highlight_index < k) {
                    enhanced_equip_action = ENHANCED_ACTION_EXAMINE;
                    enhanced_equipment_selected_item = out_index[highlight_index];
                    done = true;
                }
            }
            break;
        
        /* Toggle keys - switch to inventory */
        case '/':           /* / - toggle to inventory */
        case KTRL('I'):     /* Ctrl+I - toggle to inventory */
        case KTRL('E'):     /* Ctrl+E - toggle to inventory */
            enhanced_equip_action = ENHANCED_ACTION_SWITCH;
            done = true;
            break;
        
        case '8':
            if (highlight_active && k > 0) {
                int next = equipment_next_occupied_row(k, out_index,
                    highlight_index, -1);
                if (next >= 0)
                    highlight_index = next;
            }
            break;
        
        case '2':
            if (highlight_active && k > 0) {
                int next = equipment_next_occupied_row(k, out_index,
                    highlight_index, 1);
                if (next >= 0)
                    highlight_index = next;
            }
            break;
        
        case ' ':        /* Space - use item or examine based on context */
        case '\r':       /* Enter - use item or examine based on context */
        case '\n':       /* Enter (alternative) - use item or examine based on context */
            if (highlight_active && highlight_index >= 0 && highlight_index < k) {
                extern char current_menu_command;
                if (current_menu_command == 'x') {
                    /* Examine mode - mark for examination */
                    done = true;
                    enhanced_equip_action = ENHANCED_ACTION_EXAMINE;
                    enhanced_equipment_selected_item = out_index[highlight_index];
                } else {
                    if (!death_spectator_allow_menu_action())
                        break;

                    /* Use mode - defer action */
                    done = true;
                    enhanced_equip_action = ENHANCED_ACTION_USE;
                    enhanced_equipment_selected_item = out_index[highlight_index];
                }
            }
            break;

        case '6':        /* Arrow right - description */
            if (highlight_active && highlight_index >= 0 && highlight_index < k) {
                /* Mark that we want to examine an item */
                enhanced_equip_action = ENHANCED_ACTION_EXAMINE;
                enhanced_equipment_selected_item = out_index[highlight_index];
                done = true;
            }
            break;

        case '4':        /* Arrow left - drop item */
            if (highlight_active && highlight_index >= 0 && highlight_index < k) {
                if (!death_spectator_allow_menu_action())
                    break;

                done = true;
                enhanced_equip_action = ENHANCED_ACTION_DROP;
                enhanced_equipment_selected_item = out_index[highlight_index];
            }
            break;
        
        default:
        equip_default_case:
            /* Handle item selection by letter */
            if ((which >= 'a' && which <= 'z') || (which >= 'A' && which <= 'Z')) {
                extern char current_menu_command;
                bool allow_letters = false;
                
                /* Letter selection is hidden/disabled only in controller UI mode. */
                allow_letters = !controller_controls;
                
                if (!allow_letters) {
                    bell("Use arrow keys and Space to select items in this mode");
                    break;
                }
                if (death_spectator_active()) {
                    death_spectator_allow_menu_action();
                    break;
                }
                int item = label_to_equip(which);
                if (item >= INVEN_WIELD && item < INVEN_TOTAL && (inventory[item].k_idx || (throw_slot_menu_active && throw_slot_enabled[item]))) {
                    done = true;
                    
                    /* Handle based on menu context */
                    extern char current_menu_command;
                    if (current_menu_command != 0) {
                        /* Command mode - defer action */
                        if (current_menu_command == 'u') {
                            if (!death_spectator_allow_menu_action()) {
                                done = false;
                                break;
                            }
                            enhanced_equip_action = ENHANCED_ACTION_USE;
                            enhanced_equipment_selected_item = item;
                        } else if (current_menu_command == 'x') {
                            /* For observe mode, mark for examination */
                            enhanced_equip_action = ENHANCED_ACTION_EXAMINE;
                            enhanced_equipment_selected_item = item;
                        }
                    } else {
                        /* Direct access mode - use the traditional method */
                        if (!death_spectator_allow_menu_action()) {
                            done = false;
                        } else {
                            p_ptr->command_new = which;
                            p_ptr->command_see = true;
                        }
                    }
                }
                else {
                    bell("Illegal object choice!");
                }
            }
            else {
                bell("Invalid command!");
            }
            break;
        }
    }
    
    (void)Term_set_extra_cursor(false, 0, 0, false);
    ui_menu_click_clear();
    hide_cursor = saved_hide_cursor;
    (void)Term_set_cursor(saved_cursor);
    story_font_term_pop(&story_state);
    log_trace("show_equip_enhanced: Exiting equipment enhanced menu, action=%d", enhanced_equip_action);
}

typedef enum
{
    IDENT_ENTRY_INVEN,
    IDENT_ENTRY_EQUIP,
    IDENT_ENTRY_FLOOR,
    IDENT_ENTRY_SUPPLY
} ident_entry_type;

typedef struct
{
    ident_entry_type type;
    int index;
    int supply_index;
    int floor_o_idx;
    object_type* o_ptr;
    char label[6];
    char prefix[24];
    char desc[80];
    byte color;
} ident_entry;

#define MAX_IDENT_SUPPLY 256
#define MAX_IDENT_ENTRIES (INVEN_PACK + (INVEN_TOTAL - INVEN_WIELD) + MAX_FLOOR_STACK + MAX_IDENT_SUPPLY)

static void build_ident_entry_label(int order, char out[6])
{
    char label = index_to_label(order);
    out[0] = label;
    out[1] = ')';
    out[2] = '\0';
}

static void draw_ident_line(const ident_entry* entry, int row, int col,
    int weight_col, bool highlight)
{
    byte attr = highlight ? TERM_L_BLUE : entry->color;
    byte label_attr = highlight ? TERM_L_BLUE : TERM_WHITE;
    int offset = col + 3;
    char weight_buf[16];

    prt("", row, col);

    if (highlight)
        c_put_str(label_attr, entry->label, row, col);
    else
        put_str(entry->label, row, col);

    if (entry->prefix[0] != '\0')
    {
        if (highlight)
            c_put_str(attr, entry->prefix, row, offset);
        else
            put_str(entry->prefix, row, offset);
        offset += (int)strlen(entry->prefix);
    }

    c_put_str(attr, entry->desc, row, offset);

    if (show_weights)
    {
        int wgt = entry->o_ptr->weight * entry->o_ptr->number;
        if (entry->o_ptr->weight || entry->type != IDENT_ENTRY_EQUIP)
        {
            strnfmt(weight_buf, sizeof(weight_buf), "%2d.%1d lb", wgt / 10, wgt % 10);
            c_put_str(attr, weight_buf, row, weight_col);
        }
    }
}

bool display_unified_identify_menu(bool include_floor, int* out_item, object_type** out_object)
{
    ident_entry entries[MAX_IDENT_ENTRIES];
    int entry_count = 0;
    int term_wid = menu_term_width();
    int term_hgt = (Term && Term->hgt > 0) ? Term->hgt : 24;
    int weight_col = menu_weight_col_for_width(term_wid);
    int len = 29;
    const int base_lim = term_wid - 3;
    const int lim_no_weight = base_lim - (show_weights ? 9 : 0);
    int floor_list[MAX_FLOOR_STACK];
    int floor_num = 0;
    int supply_count = supplies_entry_count();

    if (include_floor)
        floor_num = scan_floor(floor_list, MAX_FLOOR_STACK, p_ptr->py, p_ptr->px, 0x00);

    if (include_floor)
    {
        for (int i = 0; i < floor_num && entry_count < MAX_IDENT_ENTRIES; i++)
        {
            int o_idx = floor_list[i];
            object_type* o_ptr = &o_list[o_idx];
            if (!o_ptr->k_idx || !object_is_unidentified_for_display(o_ptr))
                continue;

            ident_entry* entry = &entries[entry_count];
            entry->type = IDENT_ENTRY_FLOOR;
            entry->index = 0;
            entry->supply_index = -1;
            entry->floor_o_idx = o_idx;
            entry->o_ptr = o_ptr;
            strnfmt(entry->label, sizeof(entry->label), "-)");
            entry->prefix[0] = '\0';

            object_desc(entry->desc, sizeof(entry->desc), o_ptr, true, 3);
            if (lim_no_weight >= 0)
                entry->desc[lim_no_weight] = '\0';

            entry->color = weapon_glows(o_ptr) 
                ? object_display_color(o_ptr, TERM_L_BLUE)
                : object_display_color(o_ptr, tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

            int row_len = (int)strlen(entry->desc) + 5;
            if (show_weights)
                row_len += 9;
            if (row_len > len)
                len = row_len;

            entry_count++;
        }
    }

    for (int i = 0; i < supply_count && entry_count < MAX_IDENT_ENTRIES; i++)
    {
        object_type* o_ptr = supplies_entry_at(i);
        if (!o_ptr || !o_ptr->k_idx || !object_is_unidentified_for_display(o_ptr))
            continue;

        ident_entry* entry = &entries[entry_count];
        entry->type = IDENT_ENTRY_SUPPLY;
        entry->index = i;
        entry->supply_index = i;
        entry->floor_o_idx = 0;
        entry->o_ptr = o_ptr;
        build_ident_entry_label(entry_count, entry->label);

        const char* supply_prefix = "Supplies: ";
        if (o_ptr->tval == TV_POTION)
            supply_prefix = "Supplies (potions): ";
        else if (o_ptr->tval == TV_GEM)
            supply_prefix = "Supplies (gems): ";
        else if (o_ptr->tval == TV_FOOD)
            supply_prefix = "Supplies (food): ";
        else if (supplies_group_matches_object(SUPPLY_GROUP_LIGHTS, o_ptr))
            supply_prefix = "Supplies (lights/oil): ";

        strnfmt(entry->prefix, sizeof(entry->prefix), "%s", supply_prefix);

        int prefix_len = (int)strlen(entry->prefix);
        int desc_lim = lim_no_weight - prefix_len;
        if (desc_lim < 0)
            desc_lim = 0;

        object_desc(entry->desc, sizeof(entry->desc), o_ptr, true, 3);
        entry->desc[desc_lim] = '\0';

        entry->color = weapon_glows(o_ptr) 
            ? object_display_color(o_ptr, TERM_L_BLUE)
            : object_display_color(o_ptr, tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)]);

        int row_len = prefix_len + (int)strlen(entry->desc) + 5;
        if (show_weights && o_ptr->weight)
            row_len += 9;
        if (row_len > len)
            len = row_len;

        entry_count++;
    }

    for (int i = 0; i < INVEN_PACK && entry_count < MAX_IDENT_ENTRIES; i++)
    {
        object_type* o_ptr = &inventory[i];
        if (!o_ptr->k_idx || !object_is_unidentified_for_display(o_ptr))
            continue;

        ident_entry* entry = &entries[entry_count];
        entry->type = IDENT_ENTRY_INVEN;
        entry->index = i;
        entry->supply_index = -1;
        entry->floor_o_idx = 0;
        entry->o_ptr = o_ptr;
        build_ident_entry_label(entry_count, entry->label);
        entry->prefix[0] = '\0';

        object_desc(entry->desc, sizeof(entry->desc), o_ptr, true, 3);
        if (lim_no_weight >= 0)
            entry->desc[lim_no_weight] = '\0';

        entry->color = weapon_glows(o_ptr) ? TERM_L_BLUE
            : tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)];

        int row_len = (int)strlen(entry->desc) + 5;
        if (show_weights)
            row_len += 9;
        if (row_len > len)
            len = row_len;

        entry_count++;
    }

    for (int i = INVEN_WIELD; i < INVEN_TOTAL && entry_count < MAX_IDENT_ENTRIES; i++)
    {
        object_type* o_ptr = &inventory[i];
        if (!o_ptr->k_idx || !object_is_unidentified_for_display(o_ptr))
            continue;

        ident_entry* entry = &entries[entry_count];
        entry->type = IDENT_ENTRY_EQUIP;
        entry->index = i;
        entry->supply_index = -1;
        entry->floor_o_idx = 0;
        entry->o_ptr = o_ptr;
        build_ident_entry_label(entry_count, entry->label);
        strnfmt(entry->prefix, sizeof(entry->prefix), "%-12s: ", mention_use(i));

        int prefix_len = (int)strlen(entry->prefix);
        int desc_lim = lim_no_weight - prefix_len;
        if (desc_lim < 0)
            desc_lim = 0;

        object_desc(entry->desc, sizeof(entry->desc), o_ptr, true, 3);
        entry->desc[desc_lim] = '\0';

        entry->color = tval_to_attr[o_ptr->tval % N_ELEMENTS(tval_to_attr)];

        int row_len = prefix_len + (int)strlen(entry->desc) + 5;
        if (show_weights && o_ptr->weight)
            row_len += 9;
        if (row_len > len)
            len = row_len;

        entry_count++;
    }

    if (entry_count == 0)
    {
        msg_print("There is nothing unidentified here.");
        return false;
    }

    int col = menu_center_col_for_len(term_wid, len);
    int highlight = 0;
    bool done = false;
    bool success = false;
    bool controller_controls = steamdeck_controls_active();

    screen_save();

    int clear_start = (col > 1) ? (col - 2) : col;
    int clear_width = term_wid - clear_start;
    if (clear_width < 0)
        clear_width = 0;
    int base_rows = MIN(term_hgt - 1, entry_count + 1);
    int rows_to_clear = base_rows;

    log_trace("display_unified_identify_menu: init clear entry_count=%d, start_col=%d, width=%d, rows=%d",
        entry_count, clear_start, clear_width, rows_to_clear);

    while (!done)
    {
        char prompt[96];

        Term_erase(0, 0, 255);

        for (int row = 1; row <= rows_to_clear && row < term_hgt; row++)
        {
            Term_erase(clear_start, row, clear_width);
        }
        log_trace("display_unified_identify_menu: redraw cleared rows 1-%d from col %d width %d",
            MIN(rows_to_clear, term_hgt - 1), clear_start, clear_width);

        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_touch_category(
            SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
        ui_scroll_area_begin(1, term_hgt - 1,
            SDL_TOUCH_MENU_CATEGORY_INVENTORY_EQUIPMENT);
        ui_scroll_area_set_keys('8', '2', '6', '4');

        if (controller_controls)
        {
            char confirm_label[16];
            char inspect_label[16];
            char back_label[16];

            inventory_prompt_label(steamdeck_confirm_key(), "A",
                confirm_label, sizeof(confirm_label));
            inventory_prompt_label(steamdeck_info_key(), "RS Right",
                inspect_label, sizeof(inspect_label));
            inventory_prompt_label(steamdeck_back_key(), "B", back_label,
                sizeof(back_label));
            strnfmt(prompt, sizeof(prompt),
                "Identify: %s, %s Inspect, %s cancel", confirm_label,
                inspect_label, back_label);
            prt(prompt, 0, 0);
            ui_menu_click_add_text_token(-2, 0, 0, prompt, confirm_label);
            ui_menu_click_add_text_token(-3, 0, 0, prompt, inspect_label);
            ui_menu_click_add_text_token(-3, 0, 0, prompt, "Inspect");
            ui_menu_click_add_text_token(-1, 0, 0, prompt, back_label);
            ui_menu_click_add_text_token(-1, 0, 0, prompt, "cancel");
        }
        else
        {
            strnfmt(prompt, sizeof(prompt),
                "Identify: Space, <- Inspect, ESC to cancel");
            prt(prompt, 0, 0);
            ui_menu_click_add_text_token(-2, 0, 0, prompt, "Space");
            ui_menu_click_add_text_token(-3, 0, 0, prompt, "Inspect");
            ui_menu_click_add_text_token(-1, 0, 0, prompt, "cancel");
        }

        for (int i = 0; i < entry_count; i++)
        {
            draw_ident_line(&entries[i], i + 1, col, weight_col,
                (i == highlight));
            ui_menu_click_add_full_row(i, i + 1);
        }

        if (entry_count && entry_count < term_hgt - 1)
            prt("", entry_count + 1, col);

        rows_to_clear = base_rows;

        int key = inkey();
        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                ui_menu_click_clear();
                if (clicked_choice >= 0 && clicked_choice < entry_count)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                    {
                        highlight = clicked_choice;
                        continue;
                    }

                    if (click_action == UI_MENU_CLICK_SECONDARY)
                    {
                        highlight = clicked_choice;
                        key = '4';
                    }
                    else if (clicked_choice != highlight)
                    {
                        highlight = clicked_choice;
                        continue;
                    }
                    else
                        key = ' ';
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                    continue;
                else if (clicked_choice == -1)
                    key = ESCAPE;
                else if (clicked_choice == -2)
                    key = ' ';
                else if (clicked_choice == -3)
                    key = '4';
            }
        }

        if (controller_controls && key == steamdeck_back_key())
            key = ESCAPE;
        else if (controller_controls && key == steamdeck_confirm_key())
            key = ' ';
        else if (controller_controls && key == steamdeck_info_key())
            key = '4';

        switch (key)
        {
        case UI_MENU_CLICK_WAKE_KEY:
            break;

        case ESCAPE:
            done = true;
            success = false;
            break;

        case '8':
        case 'k':
        case 'K':
            highlight = (highlight + entry_count - 1) % entry_count;
            break;

        case '2':
        case 'j':
        case 'J':
            highlight = (highlight + 1) % entry_count;
            break;

        case '4':
        case 'h':
        case 'H':
            ui_menu_click_clear();
            ui_scroll_area_clear();
            (void)player_try_identify_smithing_object_on_examine(
                entries[highlight].o_ptr,
                (entries[highlight].type == IDENT_ENTRY_EQUIP));
            object_info_screen(entries[highlight].o_ptr);
            break;

        case ' ':
        case 13:
        case 10:
            success = true;
            done = true;
            break;

        default:
            bell("Invalid command!");
            break;
        }
    }

    ui_menu_click_clear();
    ui_scroll_area_clear();
    screen_load();

    if (!success)
        return false;

    ident_entry* chosen = &entries[highlight];

    if (chosen->type == IDENT_ENTRY_FLOOR)
    {
        *out_item = 0 - chosen->floor_o_idx;
        *out_object = &o_list[chosen->floor_o_idx];
    }
    else if (chosen->type == IDENT_ENTRY_SUPPLY)
    {
        *out_item = SUPPLIES_INDEX + chosen->supply_index;
        *out_object = chosen->o_ptr;
    }
    else
    {
        *out_item = chosen->index;
        *out_object = &inventory[chosen->index];
    }

    return true;
}

/*
 * Returns the paired artefact index for a given artefact, or 0 if none.
 * Paired weapons can be wielded together in main hand and off-hand
 * without requiring Two Weapon Fighting and without off-hand penalties.
 */
int get_paired_artefact(int art_idx)
{
    return item_sets_get_paired_artefact(art_idx);
}

bool player_can_treat_as_throwing_flags(const object_type* o_ptr, u32b f3)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;

    if (f3 & TR3_THROWING)
        return true;

    return false;
}

bool weapon_is_impale_eligible(const object_type* o_ptr)
{
    u32b f1 = 0, f2 = 0, f3 = 0, f4 = 0;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    if (f3 & TR3_POLEARM)
        return true;

    if ((o_ptr->tval == TV_SWORD)
        && (k_info[o_ptr->k_idx].flags3 & TR3_TWO_HANDED))
    {
        return true;
    }

    return false;
}

bool player_can_treat_as_throwing(const object_type* o_ptr)
{
    u32b f1 = 0, f2 = 0, f3 = 0, f4 = 0;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    return player_can_treat_as_throwing_flags(o_ptr, f3);
}

#undef MAX_COMPARE_LINES
#undef MAX_IDENT_ENTRIES
