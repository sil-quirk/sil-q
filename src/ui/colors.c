/* ui/colors.c - Color name and attribute utilities */

#include "angband.h"
#include "cJSON.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include "ui/colors.h"

#include <string.h>

typedef struct ui_color_preset {
    char id[UI_COLOR_PRESET_ID_LEN];
    char label[UI_COLOR_PRESET_LABEL_LEN];
    byte base[16][3];
} ui_color_preset;

static ui_color_preset g_palette_presets[UI_COLOR_PRESET_MAX];
static int g_palette_preset_count = 0;
static char g_current_palette_preset[UI_COLOR_PRESET_ID_LEN] = "";

static const struct {
    const char* id;
    const char* label;
    byte base[16][3];
} builtin_palette_presets[] = {
    {
        "classic",
        "Classic",
        {
            { 0x00, 0x00, 0x00 }, { 0xFF, 0xFF, 0xFF },
            { 0x80, 0x80, 0x80 }, { 0xFF, 0x80, 0x00 },
            { 0xC0, 0x00, 0x00 }, { 0x00, 0x80, 0x40 },
            { 0x00, 0x40, 0xFF }, { 0x80, 0x40, 0x00 },
            { 0x50, 0x50, 0x50 }, { 0xC0, 0xC0, 0xC0 },
            { 0xA0, 0x00, 0xFF }, { 0xFF, 0xFF, 0x00 },
            { 0xFF, 0x60, 0x60 }, { 0x00, 0xFF, 0x00 },
            { 0x00, 0xFF, 0xFF }, { 0xC0, 0x80, 0x40 }
        }
    },
    {
        "embers",
        "Embers",
        {
            { 0x0A, 0x06, 0x04 }, { 0xF7, 0xEE, 0xD8 },
            { 0x8B, 0x78, 0x68 }, { 0xF2, 0x9A, 0x3A },
            { 0xCC, 0x4A, 0x2B }, { 0x4E, 0x8B, 0x57 },
            { 0x4A, 0x79, 0xC9 }, { 0x8A, 0x58, 0x36 },
            { 0x43, 0x39, 0x33 }, { 0xD7, 0xC8, 0xB4 },
            { 0x9A, 0x5F, 0xD6 }, { 0xF1, 0xD4, 0x54 },
            { 0xF2, 0x83, 0x5B }, { 0x5E, 0xC0, 0x76 },
            { 0x73, 0xC8, 0xE3 }, { 0xD1, 0x9A, 0x5A }
        }
    },
    {
        "twilight",
        "Twilight",
        {
            { 0x05, 0x08, 0x10 }, { 0xE8, 0xF0, 0xFF },
            { 0x7A, 0x83, 0x99 }, { 0xE6, 0x8A, 0x3A },
            { 0xB6, 0x44, 0x5C }, { 0x4B, 0x9A, 0x79 },
            { 0x4C, 0x76, 0xD8 }, { 0x8C, 0x63, 0x3E },
            { 0x39, 0x45, 0x56 }, { 0xC8, 0xD2, 0xE8 },
            { 0x8F, 0x62, 0xD8 }, { 0xE8, 0xD6, 0x67 },
            { 0xEB, 0x7A, 0x89 }, { 0x63, 0xD2, 0x8A },
            { 0x73, 0xC4, 0xFF }, { 0xC6, 0x97, 0x62 }
        }
    }
};

/* Short color names for base colors */
static char* short_color_names[MAX_BASE_COLORS] = {
    "Dark", "White", "Slate", "Orange",
    "Red", "Green", "Blue", "Umber",
    "L.Dark", "L.Slate", "Violet", "Yellow",
    "L.Red", "L.Green", "L.Blue", "L.Umber"
};

static void ui_colors_reset_preset_registry(void)
{
    memset(g_palette_presets, 0, sizeof(g_palette_presets));
    g_palette_preset_count = 0;
}

static bool ui_colors_append_preset(const char* id, const char* label,
    const byte base[16][3])
{
    ui_color_preset* preset;

    if (!id || !id[0] || !base
        || g_palette_preset_count >= UI_COLOR_PRESET_MAX)
    {
        return false;
    }

    preset = &g_palette_presets[g_palette_preset_count++];
    SDL_strlcpy(preset->id, id, sizeof(preset->id));
    SDL_strlcpy(preset->label,
        (label && label[0]) ? label : id, sizeof(preset->label));
    memcpy(preset->base, base, sizeof(preset->base));
    return true;
}

static void ui_colors_load_builtin_presets(void)
{
    for (int i = 0; i < (int)N_ELEMENTS(builtin_palette_presets); i++)
    {
        (void)ui_colors_append_preset(builtin_palette_presets[i].id,
            builtin_palette_presets[i].label, builtin_palette_presets[i].base);
    }
}

static bool ui_colors_parse_preset_colors(cJSON* colors, byte out[16][3])
{
    if (!cJSON_IsArray(colors) || cJSON_GetArraySize(colors) != 16)
        return false;

    for (int i = 0; i < 16; i++)
    {
        cJSON* color = cJSON_GetArrayItem(colors, i);

        if (!cJSON_IsArray(color) || cJSON_GetArraySize(color) != 3)
            return false;

        for (int channel = 0; channel < 3; channel++)
        {
            cJSON* value = cJSON_GetArrayItem(color, channel);

            if (!cJSON_IsNumber(value) || value->valueint < 0
                || value->valueint > 255)
            {
                return false;
            }
            out[i][channel] = (byte)value->valueint;
        }
    }

    return true;
}

static bool ui_colors_load_presets_from_file(cptr path)
{
    SDL_IOStream* file;
    cJSON* root;
    cJSON* presets;
    char* buffer;
    Sint64 file_size;
    size_t read;
    bool loaded_any = false;

    file = sdl_fopen(path, "rb");
    if (!file)
        return false;

    file_size = SDL_GetIOSize(file);
    if (file_size <= 0 || file_size > 1024 * 1024)
    {
        sdl_fclose(file);
        return false;
    }

    buffer = mem_alloc_array((size_t)file_size + 1, char);
    read = SDL_ReadIO(file, buffer, (size_t)file_size);
    buffer[read] = '\0';
    sdl_fclose(file);

    root = cJSON_Parse(buffer);
    mem_free(buffer);
    if (!root)
        return false;

    presets = cJSON_GetObjectItemCaseSensitive(root, "presets");
    if (cJSON_IsArray(presets))
    {
        cJSON* preset = NULL;

        cJSON_ArrayForEach(preset, presets)
        {
            cJSON* id = cJSON_GetObjectItemCaseSensitive(preset, "id");
            cJSON* label = cJSON_GetObjectItemCaseSensitive(preset, "label");
            cJSON* colors = cJSON_GetObjectItemCaseSensitive(preset, "colors");
            byte base[16][3];

            if (!cJSON_IsString(id) || !id->valuestring
                || !ui_colors_parse_preset_colors(colors, base))
            {
                continue;
            }

            loaded_any |= ui_colors_append_preset(id->valuestring,
                cJSON_IsString(label) ? label->valuestring : id->valuestring,
                base);
        }
    }

    cJSON_Delete(root);
    return loaded_any;
}

static byte ui_colors_scale_channel(byte value, int numerator)
{
    int scaled = (value * numerator + 4) / 8;
    return (byte)MIN(255, MAX(0, scaled));
}

static void ui_colors_write_derived_palette(const byte base[16][3])
{
    static const int numerators[4] = { 8, 6, 4, 3 };

    for (int shade = 0; shade < 4; shade++)
    {
        for (int color = 0; color < 16; color++)
        {
            int idx = color + shade * 16;

            angband_color_table[idx][0] = 0;
            for (int channel = 0; channel < 3; channel++)
            {
                angband_color_table[idx][channel + 1] =
                    ui_colors_scale_channel(base[color][channel],
                        numerators[shade]);
            }
        }
    }
}

bool ui_colors_load_palette_presets(void)
{
    char path[1024] = "";

    ui_colors_reset_preset_registry();
    if (!ANGBAND_DIR_PREF || !ANGBAND_DIR_PREF[0]
        || !path_build(path, sizeof(path), ANGBAND_DIR_PREF,
            "palette_presets.json")
        || !ui_colors_load_presets_from_file(path))
    {
        ui_colors_load_builtin_presets();
        log_warn("Palette presets unavailable or invalid%s%s; using built-ins",
            path[0] ? ": " : "", path);
    }

    if (g_palette_preset_count <= 0)
        ui_colors_load_builtin_presets();

    return g_palette_preset_count > 0;
}

int ui_colors_palette_preset_count(void)
{
    return g_palette_preset_count;
}

cptr ui_colors_palette_preset_id(int index)
{
    if (index < 0 || index >= g_palette_preset_count)
        return NULL;
    return g_palette_presets[index].id;
}

cptr ui_colors_palette_preset_label(int index)
{
    if (index < 0 || index >= g_palette_preset_count)
        return NULL;
    return g_palette_presets[index].label;
}

cptr ui_colors_current_palette_preset(void)
{
    return g_current_palette_preset;
}

bool ui_colors_apply_palette_preset(cptr id)
{
    int index = 0;

    if (g_palette_preset_count <= 0)
        ui_colors_load_builtin_presets();
    if (g_palette_preset_count <= 0)
        return false;

    if (id && id[0])
    {
        for (index = 0; index < g_palette_preset_count; index++)
        {
            if (streq(g_palette_presets[index].id, id))
                break;
        }
        if (index >= g_palette_preset_count)
            index = 0;
    }

    ui_colors_write_derived_palette(g_palette_presets[index].base);
    SDL_strlcpy(g_current_palette_preset, g_palette_presets[index].id,
        sizeof(g_current_palette_preset));
    return true;
}

/*
 * Extract a textual representation of an attribute.
 * Returns the base color name, optionally with a shade suffix.
 */
cptr attr_to_text(byte a)
{
    char* base;

    base = short_color_names[GET_BASE_COLOR(a)];

#if DO_YOU_WANT_THIS_IN_MONSTER_SPOILERS_Q

    if (GET_SHADE(a) > 0)
    {
        static char buf[25];

        strnfmt(buf, sizeof(buf), "%s%d", base, GET_SHADE(a));

        return (buf);
    }

#endif

    return (base);
}

/*
 * Convert a "color letter" into an "actual" color
 * The colors are: dwsorgbuDWvyRGBU, as shown below
 */
int color_char_to_attr(char c)
{
    switch (c)
    {
    case 'd':
        return (TERM_DARK);
    case 'w':
        return (TERM_WHITE);
    case 's':
        return (TERM_SLATE);
    case 'o':
        return (TERM_ORANGE);
    case 'r':
        return (TERM_RED);
    case 'g':
        return (TERM_GREEN);
    case 'b':
        return (TERM_BLUE);
    case 'u':
        return (TERM_UMBER);

    case 'D':
        return (TERM_L_DARK);
    case 'W':
        return (TERM_L_WHITE);
    case 'v':
        return (TERM_VIOLET);
    case 'y':
        return (TERM_YELLOW);
    case 'R':
        return (TERM_L_RED);
    case 'G':
        return (TERM_L_GREEN);
    case 'B':
        return (TERM_L_BLUE);
    case 'U':
        return (TERM_L_UMBER);
    }

    return (-1);
}

#ifdef SUPPORT_GAMMA

/* Table of gamma values */
byte gamma_table[256];

/* Table of ln(x / 256) * 256 for x going from 0 -> 255 */
static const s16b gamma_helper[256] = { 0, -1420, -1242, -1138, -1065, -1007,
    -961, -921, -887, -857, -830, -806, -783, -762, -744, -726, -710, -694,
    -679, -666, -652, -640, -628, -617, -606, -596, -586, -576, -567, -577,
    -549, -541, -532, -525, -517, -509, -502, -495, -488, -482, -475, -469,
    -463, -457, -451, -455, -439, -434, -429, -423, -418, -413, -408, -403,
    -398, -394, -389, -385, -380, -376, -371, -367, -363, -359, -355, -351,
    -347, -343, -339, -336, -332, -328, -325, -321, -318, -314, -311, -308,
    -304, -301, -298, -295, -291, -288, -285, -282, -279, -276, -273, -271,
    -268, -265, -262, -259, -257, -254, -251, -248, -246, -243, -241, -238,
    -236, -233, -231, -228, -226, -223, -221, -219, -216, -214, -212, -209,
    -207, -205, -203, -200, -198, -196, -194, -192, -190, -188, -186, -184,
    -182, -180, -178, -176, -174, -172, -170, -168, -166, -164, -162, -160,
    -158, -156, -155, -153, -151, -149, -147, -146, -144, -142, -140, -139,
    -137, -135, -134, -132, -130, -128, -127, -125, -124, -122, -120, -119,
    -117, -116, -114, -112, -111, -109, -108, -106, -105, -103, -102, -100, -99,
    -97, -96, -95, -93, -92, -90, -89, -87, -86, -85, -83, -82, -80, -79, -78,
    -76, -75, -74, -72, -71, -70, -68, -67, -66, -65, -63, -62, -61, -59, -58,
    -57, -56, -54, -53, -52, -51, -50, -48, -47, -46, -45, -44, -42, -41, -40,
    -39, -38, -37, -35, -34, -33, -32, -31, -30, -29, -27, -26, -25, -24, -23,
    -22, -21, -20, -19, -18, -17, -16, -14, -13, -12, -11, -10, -9, -8, -7, -6,
    -5, -4, -3, -2, -1 };

/*
 * Build the gamma table so that floating point isn't needed.
 *
 * Note gamma goes from 0->256.  The old value of 100 is now 128.
 */
void build_gamma_table(int gamma)
{
    int i, n;

    /*
     * value is the current sum.
     * diff is the new term to add to the series.
     */
    long value, diff;

    /* Hack - convergence is bad in these cases. */
    gamma_table[0] = 0;
    gamma_table[255] = 255;

    for (i = 1; i < 255; i++)
    {
        /*
         * Initialise the Taylor series
         *
         * value and diff have been scaled by 256
         */
        n = 1;
        value = 256L * 256L;
        diff = ((long)gamma_helper[i]) * (gamma - 256);

        while (diff)
        {
            value += diff;
            n++;

            /*
             * Use the following identiy to calculate the gamma table.
             * exp(x) = 1 + x + x^2/2 + x^3/(2*3) + x^4/(2*3*4) +...
             *
             * n is the current term number.
             *
             * The gamma_helper array contains a table of
             * ln(x/256) * 256
             * This is used because a^b = exp(b*ln(a))
             *
             * In this case:
             * a is i / 256
             * b is gamma.
             *
             * Note that everything is scaled by 256 for accuracy,
             * plus another factor of 256 for the final result to
             * be from 0-255.  Thus gamma_helper[] * gamma must be
             * divided by 256*256 each itteration, to get back to
             * the original power series.
             */
            diff = (((diff / 256) * gamma_helper[i]) * (gamma - 256))
                / (256 * n);
        }

        /*
         * Store the value in the table so that the
         * floating point pow function isn't needed.
         */
        gamma_table[i] = ((long)(value / 256) * i) / 256;
    }
}

#endif /* SUPPORT_GAMMA */

/*
 * Returns a string which contains the name of a extended color.
 * Examples: "Dark", "Red1", "Yellow5", etc.
 * IMPORTANT: the returned string is statically allocated so it must *not* be
 * freed and its value changes between calls to this function.
 */
cptr get_ext_color_name(byte ext_color)
{
    static char buf[25];

    if (GET_SHADE(ext_color) > 0)
    {
        strnfmt(buf, sizeof(buf), "%s%d",
            color_names[GET_BASE_COLOR(ext_color)], GET_SHADE(ext_color));
    }
    else
    {
        strnfmt(buf, sizeof(buf), "%s", color_names[GET_BASE_COLOR(ext_color)]);
    }

    return buf;
}

/*
 * Converts a string to a terminal color byte.
 */
int color_text_to_attr(cptr name)
{
    int i, len, base, shade;

    /* Optimize name searching. See below */
    static byte len_names[MAX_BASE_COLORS];

    /* Separate the color name and the shade number */
    /* Only letters can be part of the name */
    for (i = 0; isalpha(name[i]); i++)
        ;

    /* Store the start of the shade number */
    len = i;

    /* Check for invalid characters in the shade part */
    while (name[i])
    {
        /* No digit, exit */
        if (!isdigit(name[i]))
            return (-1);
        ++i;
    }

    /* Initialize the shade */
    shade = 0;

    /* Only analyze the shade if there is one */
    if (name[len])
    {
        /* Convert to number */
        shade = atoi(name + len);

        /* Check bounds */
        if ((shade < 0) || (shade > MAX_SHADES - 1))
            return (-1);
    }

    /* Extra, allow the use of strings like "r1", "U5", etc. */
    if (len == 1)
    {
        /* Convert one character, check sanity */
        if ((base = color_char_to_attr(name[0])) == -1)
            return (-1);

        /* Build the extended color */
        return (MAKE_EXTENDED_COLOR(base, shade));
    }

    /* Hack - Initialize the length array once */
    if (!len_names[0])
    {
        for (base = 0; base < MAX_BASE_COLORS; base++)
        {
            /* Store the length of each color name */
            len_names[base] = (byte)strlen(color_names[base & 0x0F]);
        }
    }

    /* Find the name */
    for (base = 0; base < MAX_BASE_COLORS; base++)
    {
        /* Somewhat optimize the search */
        if (len != len_names[base])
            continue;

        /* Compare only the found name */
        if (SDL_strncasecmp(name, color_names[base & 0x0F], len) == 0)
        {
            /* Build the extended color */
            return (MAKE_EXTENDED_COLOR(base, shade));
        }
    }

    /* We can not find it */
    return (-1);
}
