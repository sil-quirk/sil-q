#include "angband.h"
#include "support/macro.h"
#include "externs.h"

/*
 * Convert a decimal to a single digit hex number
 */
static char hexify(int i) { return (hexsym[i % 16]); }

/*
 * Convert a hexidecimal-digit into a decimal
 */
static int dehex(char c)
{
    if (isdigit((unsigned char)c))
        return (D2I(c));
    if (isalpha((unsigned char)c))
        return (A2I(tolower((unsigned char)c)) + 10);
    return (0);
}

/*
 * Transform macro trigger name ('\[alt-D]' etc..)
 * into macro trigger key code ('^_O_64\r' or etc..)
 */
static size_t trigger_text_to_ascii(char* buf, size_t max, cptr* strptr)
{
    cptr str = *strptr;
    bool mod_status[MAX_MACRO_MOD];

    int i, len = 0;
    int shiftstatus = 0;
    cptr key_code;

    size_t current_len = strlen(buf);

    /* No definition of trigger names */
    if (macro_template == NULL)
        return 0;

    /* Initialize modifier key status */
    for (i = 0; macro_modifier_chr[i]; i++)
        mod_status[i] = false;

    str++;

    /* Examine modifier keys */
    while (1)
    {
        /* Look for modifier key name */
        for (i = 0; macro_modifier_chr[i]; i++)
        {
            len = strlen(macro_modifier_name[i]);

            if (!SDL_strncasecmp(str, macro_modifier_name[i], len))
                break;
        }

        /* None found? */
        if (!macro_modifier_chr[i])
            break;

        /* Proceed */
        str += len;

        /* This modifier key is pressed */
        mod_status[i] = true;

        /* Shift key might be going to change keycode */
        if ('S' == macro_modifier_chr[i])
            shiftstatus = 1;
    }

    /* Look for trigger name */
    for (i = 0; i < max_macrotrigger; i++)
    {
        len = strlen(macro_trigger_name[i]);

        /* Found it and it is ending with ']' */
        if (!SDL_strncasecmp(str, macro_trigger_name[i], len) && (']' == str[len]))
            break;
    }

    /* Invalid trigger name? */
    if (i == max_macrotrigger)
    {
        /*
         * If this invalid trigger name is ending with ']',
         * skip whole of it to avoid defining strange macro trigger
         */
        str = strchr(str, ']');

        if (str)
        {
            strnfcat(buf, max, &current_len, "\x1F\r");

            *strptr = str; /* where **strptr == ']' */
        }

        return current_len;
    }

    /* Get keycode for this trigger name */
    key_code = macro_trigger_keycode[shiftstatus][i];

    /* Proceed */
    str += len;

    /* Begin with '^_' */
    strnfcat(buf, max, &current_len, "\x1F");

    /* Write key code style trigger using template */
    for (i = 0; macro_template[i]; i++)
    {
        char ch = macro_template[i];
        int j;

        switch (ch)
        {
        case '&':
            /* Modifier key character */
            for (j = 0; macro_modifier_chr[j]; j++)
            {
                if (mod_status[j])
                    strnfcat(
                        buf, max, &current_len, "%c", macro_modifier_chr[j]);
            }
            break;
        case '#':
            /* Key code */
            strnfcat(buf, max, &current_len, "%s", key_code);
            break;
        default:
            /* Fixed string */
            strnfcat(buf, max, &current_len, "%c", ch);
            break;
        }
    }

    /* End with '\r' */
    strnfcat(buf, max, &current_len, "\r");

    /* Succeed */
    *strptr = str; /* where **strptr == ']' */

    return current_len;
}

/*
 * Hack -- convert a printable string into real ascii
 *
 * This function will not work on non-ascii systems.
 *
 * To be safe, "buf" should be at least as large as "str".
 */
void text_to_ascii(char* buf, size_t len, cptr str)
{
    char* s = buf;

    /* Analyze the "ascii" string */
    while (*str)
    {
        /* Check if the buffer is long enough */
        if (s >= buf + len - 1)
            break;

        /* Backslash codes */
        if (*str == '\\')
        {
            /* Skip the backslash */
            str++;

            /* Paranoia */
            if (!(*str))
                break;

            /* Macro Trigger */
            if (*str == '[')
            {
                /* Terminate before appending the trigger */
                *s = '\0';

                s += trigger_text_to_ascii(buf, len, &str);
            }

            /* Hack -- simple way to specify Escape */
            else if (*str == 'e')
            {
                *s++ = ESCAPE;
            }

            /* Hack -- simple way to specify "space" */
            else if (*str == 's')
            {
                *s++ = ' ';
            }

            /* Backspace */
            else if (*str == 'b')
            {
                *s++ = '\b';
            }

            /* Newline */
            else if (*str == 'n')
            {
                *s++ = '\n';
            }

            /* Return */
            else if (*str == 'r')
            {
                *s++ = '\r';
            }

            /* Tab */
            else if (*str == 't')
            {
                *s++ = '\t';
            }

            /* Bell */
            else if (*str == 'a')
            {
                *s++ = '\a';
            }

            /* Actual "backslash" */
            else if (*str == '\\')
            {
                *s++ = '\\';
            }

            /* Hack -- Actual "caret" */
            else if (*str == '^')
            {
                *s++ = '^';
            }

            /* Hack -- Hex-mode */
            else if (*str == 'x')
            {
                if (isxdigit((unsigned char)(*(str + 1)))
                    && isxdigit((unsigned char)(*(str + 2))))
                {
                    *s = 16 * dehex(*++str);
                    *s++ += dehex(*++str);
                }
                else
                {
                    /* HACK - Invalid hex number */
                    *s++ = '?';
                }
            }

            /* Oops */
            else
            {
                *s = *str;
            }

            /* Skip the final char */
            str++;
        }

        /* Normal Control codes */
        else if (*str == '^')
        {
            str++;

            if (*str)
            {
                *s++ = KTRL(*str);
                str++;
            }
        }

        /* Normal chars */
        else
        {
            *s++ = *str++;
        }
    }

    /* Terminate */
    *s = '\0';
}

/*
 * Transform macro trigger key code ('^_O_64\r' or etc..)
 * into macro trigger name ('\[alt-D]' etc..)
 */
static size_t trigger_ascii_to_text(char* buf, size_t max, cptr* strptr)
{
    cptr str = *strptr;
    char key_code[100];
    int i;
    cptr tmp;
    size_t current_len = strlen(buf);

    /* No definition of trigger names */
    if (macro_template == NULL)
        return 0;

    /* Trigger name will be written as '\[name]' */
    strnfcat(buf, max, &current_len, "\\[");

    /* Use template to read key-code style trigger */
    for (i = 0; macro_template[i]; i++)
    {
        int j;
        char ch = macro_template[i];

        switch (ch)
        {
        case '&':

            /* Read modifier */
            while (strchr(macro_modifier_chr, *str))
            {
                tmp = strchr(macro_modifier_chr, *str);
                j = (int)(tmp - macro_modifier_chr);
                strnfcat(buf, max, &current_len, "%s", macro_modifier_name[j]);
                str++;
            }
            break;
        case '#':
        {
            u16b x;
            /* Read key code */
            for (x = 0; *str && (*str != '\r') && (x < sizeof(key_code) - 1);
                 x++)
                key_code[x] = *str++;
            key_code[x] = '\0';
            break;
        }
        default:
            /* Skip fixed strings */
            if (ch != *str)
                return 0;
            str++;
        }
    }

    /* Key code style triggers always end with '\r' */
    if (*str++ != '\r')
        return 0;

    /* Look for trigger name with given keycode (normal or shifted keycode) */
    for (i = 0; i < max_macrotrigger; i++)
    {
        if (!SDL_strcasecmp(key_code, macro_trigger_keycode[0][i])
            || !SDL_strcasecmp(key_code, macro_trigger_keycode[1][i]))
            break;
    }

    /* Not found? */
    if (i == max_macrotrigger)
        return 0;

    /* Write trigger name + "]" */
    strnfcat(buf, max, &current_len, "%s]", macro_trigger_name[i]);

    /* Succeed */
    *strptr = str;
    return current_len;
}

/*
 * Hack -- convert a string into a printable form
 *
 * This function will not work on non-ascii systems.
 */
void ascii_to_text(char* buf, size_t len, cptr str)
{
    char* s = buf;

    /* Analyze the "ascii" string */
    while (*str)
    {
        byte i = (byte)(*str++);

        /* Check if the buffer is long enough */
        /* HACK - always assume worst case (hex-value + '\0') */
        if (s >= buf + len - 5)
            break;

        if (i == ESCAPE)
        {
            *s++ = '\\';
            *s++ = 'e';
        }
        else if (i == ' ')
        {
            *s++ = '\\';
            *s++ = 's';
        }
        else if (i == '\b')
        {
            *s++ = '\\';
            *s++ = 'b';
        }
        else if (i == '\t')
        {
            *s++ = '\\';
            *s++ = 't';
        }
        else if (i == '\a')
        {
            *s++ = '\\';
            *s++ = 'a';
        }
        else if (i == '\n')
        {
            *s++ = '\\';
            *s++ = 'n';
        }
        else if (i == '\r')
        {
            *s++ = '\\';
            *s++ = 'r';
        }
        else if (i == '\\')
        {
            *s++ = '\\';
            *s++ = '\\';
        }
        else if (i == '^')
        {
            *s++ = '\\';
            *s++ = '^';
        }
        /* Macro Trigger */
        else if (i == 31)
        {
            size_t offset;

            /* Terminate before appending the trigger */
            *s = '\0';

            offset = trigger_ascii_to_text(buf, len, &str);

            if (offset == 0)
            {
                /* No trigger found */
                *s++ = '^';
                *s++ = '_';
            }
            else
                s += offset;
        }
        else if (i < 32)
        {
            *s++ = '^';
            *s++ = UN_KTRL(i);
        }
        else if (i < 127)
        {
            *s++ = i;
        }
        else
        {
            *s++ = '\\';
            *s++ = 'x';
            *s++ = hexify((int)i / 16);
            *s++ = hexify((int)i % 16);
        }
    }

    /* Terminate */
    *s = '\0';
}

/*
 * The "macro" package
 *
 * Functions are provided to manipulate a collection of macros, each
 * of which has a trigger pattern string and a resulting action string
 * and a small set of flags.
 */

/*
 * Determine if any macros have ever started with a given character.
 */
static bool macro__use[256];

/*
 * Find the macro (if any) which exactly matches the given pattern
 */
int macro_find_exact(cptr pat)
{
    int i;

    /* Nothing possible */
    if (!macro__use[(byte)(pat[0])])
    {
        return (-1);
    }

    /* Scan the macros */
    for (i = 0; i < macro__num; ++i)
    {
        /* Skip macros which do not match the pattern */
        if (!streq(macro__pat[i], pat))
            continue;

        /* Found one */
        return (i);
    }

    /* No matches */
    return (-1);
}

/*
 * Find the first macro (if any) which contains the given pattern
 */
int macro_find_check(cptr pat)
{
    int i;

    /* Nothing possible */
    if (!macro__use[(byte)(pat[0])])
    {
        return (-1);
    }

    /* Scan the macros */
    for (i = 0; i < macro__num; ++i)
    {
        /* Skip macros which do not contain the pattern */
        if (!prefix(macro__pat[i], pat))
            continue;

        /* Found one */
        return (i);
    }

    /* Nothing */
    return (-1);
}

/*
 * Find the first macro (if any) which contains the given pattern and more
 */
int macro_find_maybe(cptr pat)
{
    int i;

    /* Nothing possible */
    if (!macro__use[(byte)(pat[0])])
    {
        return (-1);
    }

    /* Scan the macros */
    for (i = 0; i < macro__num; ++i)
    {
        /* Skip macros which do not contain the pattern */
        if (!prefix(macro__pat[i], pat))
            continue;

        /* Skip macros which exactly match the pattern XXX XXX */
        if (streq(macro__pat[i], pat))
            continue;

        /* Found one */
        return (i);
    }

    /* Nothing */
    return (-1);
}

/*
 * Find the longest macro (if any) which starts with the given pattern
 */
int macro_find_ready(cptr pat)
{
    int i, t, n = -1, s = -1;

    /* Nothing possible */
    if (!macro__use[(byte)(pat[0])])
    {
        return (-1);
    }

    /* Scan the macros */
    for (i = 0; i < macro__num; ++i)
    {
        /* Skip macros which are not contained by the pattern */
        if (!prefix(pat, macro__pat[i]))
            continue;

        /* Obtain the length of this macro */
        t = strlen(macro__pat[i]);

        /* Only track the "longest" pattern */
        if ((n >= 0) && (s > t))
            continue;

        /* Track the entry */
        n = i;
        s = t;
    }

    /* Result */
    return (n);
}

/*
 * Add a macro definition (or redefinition).
 *
 * We should use "act == NULL" to "remove" a macro, but this might make it
 * impossible to save the "removal" of a macro definition.  XXX XXX XXX
 *
 * We should consider refusing to allow macros which contain existing macros,
 * or which are contained in existing macros, because this would simplify the
 * macro analysis code.  XXX XXX XXX
 *
 * We should consider removing the "command macro" crap, and replacing it
 * with some kind of "powerful keymap" ability, but this might make it hard
 * to change the "roguelike" option from inside the game.  XXX XXX XXX
 */
errr macro_add(cptr pat, cptr act)
{
    int n;

    /* Paranoia -- require data */
    if (!pat || !act)
        return (-1);

    /* Look for any existing macro */
    n = macro_find_exact(pat);

    /* Replace existing macro */
    if (n >= 0)
    {
        /* Free the old macro action */
        str_free(macro__act[n]);
    }

    /* Create a new macro */
    else
    {
        /* Get a new index */
        n = macro__num++;

        /* Boundary check */
        if (macro__num >= MACRO_MAX)
            quit("Too many macros!");

        /* Save the pattern */
        macro__pat[n] = str_dup(pat);
    }

    /* Save the action */
    macro__act[n] = str_dup(act);

    /* Efficiency */
    macro__use[(byte)(pat[0])] = true;

    /* Success */
    return (0);
}

/*
 * Initialize the "macro" package
 */
errr macro_init(void)
{
    /* Macro patterns */
    macro__pat = mem_alloc_array(MACRO_MAX, cptr);

    /* Macro actions */
    macro__act = mem_alloc_array(MACRO_MAX, cptr);

    /* Success */
    return (0);
}

/*
 * Free the macro package
 */
errr macro_free(void)
{
    int i, j;

    /* Free the macros */
    for (i = 0; i < macro__num; ++i)
    {
        str_free(macro__pat[i]);
        str_free(macro__act[i]);
    }

    mem_free_null(macro__pat);
    mem_free_null(macro__act);

    /* Free the keymaps */
    for (i = 0; i < KEYMAP_MODES; ++i)
    {
        for (j = 0; j < (int)N_ELEMENTS(keymap_act[i]); ++j)
        {
            str_free(keymap_act[i][j]);
            keymap_act[i][j] = NULL;
        }
    }

    /* Success */
    return (0);
}

/*
 * Free the macro trigger package
 */
errr macro_trigger_free(void)
{
    int i;
    int num;

    if (macro_template != NULL)
    {
        /* Free the template */
        str_free(macro_template);
        macro_template = NULL;

        /* Free the trigger names and keycodes */
        for (i = 0; i < max_macrotrigger; i++)
        {
            str_free(macro_trigger_name[i]);

            str_free(macro_trigger_keycode[0][i]);
            str_free(macro_trigger_keycode[1][i]);
        }

        /* No more macro triggers */
        max_macrotrigger = 0;

        /* Count modifier-characters */
        num = strlen(macro_modifier_chr);

        /* Free modifier names */
        for (i = 0; i < num; i++)
        {
            str_free(macro_modifier_name[i]);
        }

        /* Free modifier chars */
        str_free(macro_modifier_chr);
        macro_modifier_chr = NULL;
    }

    /* Success */
    return (0);
}
