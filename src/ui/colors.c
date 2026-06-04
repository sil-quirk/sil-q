/* ui/colors.c - Color name and attribute utilities */

#include "angband.h"
#include "externs.h"
#include "ui/colors.h"

/* Short color names for base colors */
static char* short_color_names[MAX_BASE_COLORS] = {
    "Dark", "White", "Slate", "Orange",
    "Red", "Green", "Blue", "Umber",
    "L.Dark", "L.Slate", "Violet", "Yellow",
    "L.Red", "L.Green", "L.Blue", "L.Umber"
};

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
