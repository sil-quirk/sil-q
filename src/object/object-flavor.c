/* File: object/object-flavor.c */

#include "angband.h"
#include "externs.h"
#include "object/object-flavor.h"
#include "object/object-internal.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "init.h"
#include "log/log.h"
#include "cJSON.h"
#include <ctype.h>
#include <stdlib.h>


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

bool object_is_unidentified_for_display(const object_type* o_ptr);

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

    refresh_effect_visuals_for_graphics_mode();

    /* Shared object list text colors come from JSON. */
    load_object_text_colors_json();
}
