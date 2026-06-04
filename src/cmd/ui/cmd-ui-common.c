#include "angband.h"
#include "sdl-config.h"
#include "sound-config.h"
#include "sdl-sound.h"

extern struct sound_config g_sound_config;
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include <ctype.h>
#include "h-define.h"
#include "metarun.h"
#include "score/score_artefact.h"
#include "score/score_guid.h"
#include "pane.h"
#include "cmd/ui/cmd-ui-internal.h"

bool indexed_menu_letters_enabled(void)
{
    return sdl_menu_letters_enabled();
}

void indexed_menu_entry_label(char* buf, size_t buflen, int index, cptr text)
{
    if (!buf || !buflen)
        return;

    if (indexed_menu_letters_enabled())
        strnfmt(buf, buflen, "%c) %s", (char)'a' + index, text ? text : "");
    else
        strnfmt(buf, buflen, "%s", text ? text : "");
}

int menu_text_display_width(cptr text)
{
    return text ? utf8_display_width_n(text, (int)strlen(text)) : 0;
}

void keyed_menu_entry_label(char* buf, size_t buflen, char key, cptr text)
{
    if (!buf || !buflen)
        return;

    if (indexed_menu_letters_enabled())
        strnfmt(buf, buflen, "%c) %s", key, text ? text : "");
    else
        strnfmt(buf, buflen, "%s", text ? text : "");
}

int indexed_menu_prefix_col(int col)
{
    if (indexed_menu_letters_enabled())
        return col;

    return (col >= 2) ? (col - 2) : col;
}

void indexed_menu_focus_prefix(char* buf, size_t buflen, int index)
{
    if (!buf || !buflen)
        return;

    if (indexed_menu_letters_enabled())
        strnfmt(buf, buflen, "%c)", (char)'a' + index);
    else
        SDL_strlcpy(buf, "> ", buflen);
}

void indexed_menu_normal_prefix(char* buf, size_t buflen, int index)
{
    if (!buf || !buflen)
        return;

    if (indexed_menu_letters_enabled())
        strnfmt(buf, buflen, "%c)", (char)'a' + index);
    else
        SDL_strlcpy(buf, "  ", buflen);
}

char browser_entry_label_for_index(int index)
{
    if (!indexed_menu_letters_enabled())
        return 0;

    /* Keep z available as the browser drop command. */
    if (index < 0 || index >= 25)
        return 0;

    return (char)('a' + index);
}

int browser_entry_index_from_label(int ch, int entry_cnt)
{
    int index;

    if (!indexed_menu_letters_enabled())
        return -1;

    if (ch >= 'A' && ch <= 'Z')
        ch += 'a' - 'A';

    if (ch < 'a' || ch > 'y')
        return -1;

    index = ch - 'a';
    if (index < 0 || index >= entry_cnt)
        return -1;

    return index;
}

void browser_entry_label_prefix(char* buf, size_t buflen, int index)
{
    char label;

    if (!buf || buflen == 0)
        return;

    label = browser_entry_label_for_index(index);
    if (label)
        strnfmt(buf, buflen, "%c) ", label);
    else if (indexed_menu_letters_enabled())
        SDL_strlcpy(buf, "   ", buflen);
    else
        buf[0] = '\0';
}

static bool heavy_armour_desc_evasion_bonus_applies(const object_type* o_ptr)
{
    return (o_ptr->tval == TV_MAIL)
        && ((o_ptr->sval == SV_MAIL_CORSLET)
            || (o_ptr->sval == SV_LONG_CORSLET));
}

int heavy_armour_desc_current_weight(void)
{
    int i;
    int armour_weight = 0;

    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        /* Off-hand weapons are not counted as armour weight. */
        if ((i == INVEN_ARM) && (o_ptr->tval != TV_SHIELD))
            continue;

        if (i >= INVEN_BODY)
            armour_weight += o_ptr->weight;
    }

    return armour_weight;
}

int heavy_armour_desc_current_evasion_bonus(void)
{
    int i;
    int bonus = 0;

    for (i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx)
            continue;

        if (heavy_armour_desc_evasion_bonus_applies(o_ptr))
            bonus++;
    }

    return bonus;
}
