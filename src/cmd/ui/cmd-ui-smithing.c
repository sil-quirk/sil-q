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

bool enchant_then_numbers;

typedef struct smithing_tval_desc
{
    int category;
    int tval;
    cptr desc;
} smithing_tval_desc;

// object being created
object_type smith_o_body;
object_type* smith_o_ptr = &smith_o_body;

// backup object
object_type smith2_o_body;
object_type* smith2_o_ptr = &smith2_o_body;

// super backup object
object_type smith3_o_body;
object_type* smith3_o_ptr = &smith3_o_body;

typedef enum
{
    SMITH_ALLOY_NONE = 0,
    SMITH_ALLOY_MITHRIL,
    SMITH_ALLOY_STAR_IRON,
} smith_alloy_type;

typedef struct
{
    smith_alloy_type type;
    byte bonus_att;
    byte bonus_ds;
    byte bonus_evn;
    byte bonus_ps;
} smith_alloy_state;

static smith_alloy_state smith_alloy;
static smith_alloy_state smith2_alloy;
static smith_alloy_state smith3_alloy;

// artefact being created
#define smith_a_name (z_info->art_self_made_max - 1)
#define smith_a_ptr (&a_info[smith_a_name])

// backup artefact
#define smith2_a_name (z_info->art_self_made_max - 2)
#define smith2_a_ptr (&a_info[smith2_a_name])

/*
 * A structure to hold the costs of smithing something
 */
typedef struct smithing_cost_type
{
    int str;
    int dex;
    int con;
    int gra;
    int exp;
    int smt;
    int mithril;
    int star_iron;
    int alloy_weight;
    int alloy_metal;
    int alloy_mastery;
    int uses;
    int drain;
    int weaponsmith;
    int armoursmith;
    int jeweller;
    int enchantment;
    int artifice;
} smithing_cost_type;

smithing_cost_type smithing_cost;

#define CAT_WEAPON 0
#define CAT_ARMOUR 1
#define CAT_JEWELRY 2

#define MAX_SMITHING_TVALS 17

#define SMT_MENU_CREATE 1
#define SMT_MENU_ENCHANT 2
#define SMT_MENU_ARTEFACT 3
#define SMT_MENU_NUMBERS 4
#define SMT_MENU_MELT 5
#define SMT_MENU_REPAIR 6
#define SMT_MENU_ACCEPT 7

#define SMT_MENU_MAX 7

#define SMT_NUM_MENU_I_ATT 1
#define SMT_NUM_MENU_D_ATT 2
#define SMT_NUM_MENU_I_DS 3
#define SMT_NUM_MENU_D_DS 4
#define SMT_NUM_MENU_I_EVN 5
#define SMT_NUM_MENU_D_EVN 6
#define SMT_NUM_MENU_I_PS 7
#define SMT_NUM_MENU_D_PS 8
#define SMT_NUM_MENU_I_WGT 9
#define SMT_NUM_MENU_D_WGT 10
#define SMT_NUM_MENU_ALLOY_CYCLE 11
#define SMT_NUM_MENU_ALLOY_CLEAR 12
#define SMT_NUM_MENU_EDIT_BONUSES 13

#define SMT_NUM_MENU_MAX 13

#define COL_SMT1 2
#define COL_SMT2_LANDSCAPE 16
static int smith_ui_primary_submenu_col(void);
#define COL_SMT2 (smith_ui_primary_submenu_col())
static int smith_ui_last_desc_row = -1;
static int smith_ui_cost_title_row_override = -1;

typedef enum smith_ui_scroll_id
{
    SMITH_SCROLL_SVAL = 0,
    SMITH_SCROLL_TVAL,
    SMITH_SCROLL_NUMBERS,
    SMITH_SCROLL_BONUSES,
    SMITH_SCROLL_REFORGE,
    SMITH_SCROLL_ENCHANT,
    SMITH_SCROLL_FLAG,
    SMITH_SCROLL_ABILITY,
    SMITH_SCROLL_ARTEFACT,
    SMITH_SCROLL_MELT,
    SMITH_SCROLL_MAX
} smith_ui_scroll_id;

static int smith_ui_scroll_top[SMITH_SCROLL_MAX];
static int smith_ui_touch_drag_sink;

#define SMITH_CLICK_BACK 33000
#define SMITH_ROOT_BACK_LABEL "Smithing Esc"

static int smith_ui_term_wid(void)
{
    int wid = 80;

    if (Term_get_size(&wid, NULL) == 0 && wid > 0)
        return wid;

    return (Term && (Term->wid > 0)) ? Term->wid : 80;
}

static int smith_ui_term_hgt(void)
{
    int hgt = 24;

    if (Term_get_size(NULL, &hgt) == 0 && hgt > 0)
        return hgt;

    return (Term && (Term->hgt > 0)) ? Term->hgt : 24;
}

static bool smith_ui_portrait_layout(void)
{
    return sdl_mobile_portrait_layout_active();
}

static int smith_ui_content_bottom_row(void)
{
    int bottom = smith_ui_term_hgt() - 1
        - sdl_touch_menu_button_reserved_rows();

    return MAX(0, bottom);
}

static int smith_ui_primary_submenu_col(void)
{
    return smith_ui_portrait_layout() ? COL_SMT1 : COL_SMT2_LANDSCAPE;
}

static bool smith_ui_compact_width(void)
{
    return (smith_ui_term_wid() < 72);
}

static bool smith_ui_compact_height(void)
{
    return (smith_ui_term_hgt() <= 18);
}

static int smith_text_count_x;
static int smith_text_count_lines;
static int smith_text_count_indent;
static int smith_text_count_wrap;
static bool smith_text_count_has_output;
static bool smith_text_count_have_space;
static int smith_text_count_chars_since_space;

static void smith_text_count_begin(int indent, int wrap)
{
    smith_text_count_x = indent;
    smith_text_count_lines = 0;
    smith_text_count_indent = indent;
    smith_text_count_wrap = wrap;
    smith_text_count_has_output = false;
    smith_text_count_have_space = false;
    smith_text_count_chars_since_space = 0;
}

static void smith_text_count_out(byte attr, cptr str)
{
    cptr s;

    (void)attr;

    if (!str)
        return;

    for (s = str; *s; s++)
    {
        char ch;

        if (!smith_text_count_has_output)
        {
            smith_text_count_has_output = true;
            smith_text_count_lines = 1;
        }

        if (*s == '\n')
        {
            smith_text_count_x = smith_text_count_indent;
            smith_text_count_lines++;
            smith_text_count_have_space = false;
            smith_text_count_chars_since_space = 0;
            continue;
        }

        ch = isprint((unsigned char)*s) ? *s : ' ';

        if ((smith_text_count_x >= smith_text_count_wrap - 1) && (ch != ' '))
        {
            int moved_chars = 0;

            if (smith_text_count_have_space
                && (smith_text_count_chars_since_space > 0))
            {
                moved_chars = smith_text_count_chars_since_space;
            }

            smith_text_count_x = smith_text_count_indent + moved_chars;
            smith_text_count_lines++;
            smith_text_count_have_space = false;
            smith_text_count_chars_since_space = moved_chars;
        }

        smith_text_count_x++;
        if (smith_text_count_x > smith_text_count_wrap)
            smith_text_count_x = smith_text_count_wrap;

        if (ch == ' ')
        {
            smith_text_count_have_space = true;
            smith_text_count_chars_since_space = 0;
        }
        else
        {
            smith_text_count_chars_since_space++;
        }
    }
}

static int smith_count_object_preview_lines(
    const object_type* o_ptr, cptr lore, bool include_info, int indent, int wrap)
{
    void (*old_hook)(byte, cptr) = text_out_hook;
    void (*old_info_out_flags)(
        const object_type*, u32b*, u32b*, u32b*) = object_info_out_flags;
    int old_wrap = text_out_wrap;
    int old_indent = text_out_indent;
    int lines;

    smith_text_count_begin(indent, wrap);
    text_out_hook = smith_text_count_out;
    text_out_wrap = wrap;
    text_out_indent = indent;
    object_info_out_flags = object_flags;

    if (lore && lore[0])
    {
        text_out_c(TERM_WHITE, lore);
        if (include_info)
            text_out(" ");
    }

    if (include_info)
        (void)object_info_out(o_ptr);

    lines = smith_text_count_has_output ? smith_text_count_lines : 0;

    text_out_hook = old_hook;
    object_info_out_flags = old_info_out_flags;
    text_out_wrap = old_wrap;
    text_out_indent = old_indent;

    return lines;
}

static int smith_ui_secondary_col(void)
{
    if (smith_ui_compact_width())
        return COL_SMT2;

    /* Width budget for the first detail column (e.g. the base-item "Type" list).
     * Size it for its longest label ("Axe or Polearm") plus the letter prefix,
     * icon gap, and a comfortable margin, so names are not truncated. The sval
     * column to the right takes the remaining (ample) width. */
    return COL_SMT2 + 26;
}

static int smith_ui_cost_col(void)
{
    int wid = smith_ui_term_wid();
    int col = wid - (smith_ui_compact_width() ? 15 : 18);
    int min_col = smith_ui_secondary_col() + 14;

    if (col < min_col)
        col = min_col;
    if (col < 32)
        col = 32;
    if (col > wid - 1)
        col = wid - 1;

    return col;
}

#define COL_SMT3 (smith_ui_secondary_col())
#define COL_SMT4 (smith_ui_cost_col())

static int smith_ui_dense_row0(void)
{
    return smith_ui_compact_height() ? 1 : 2;
}

static int smith_ui_cost_title_row(void)
{
    if (smith_ui_portrait_layout()
        && smith_ui_cost_title_row_override >= 0)
    {
        return smith_ui_cost_title_row_override;
    }

    return smith_ui_compact_height() ? 6 : 8;
}

static int smith_ui_cost_item_row(int index0)
{
    return smith_ui_cost_title_row() + 2 + index0;
}

static int smith_ui_desc_col(void)
{
    return COL_SMT1;
}

static void smith_ui_reset_description_state(void)
{
    smith_ui_last_desc_row = -1;
    smith_ui_cost_title_row_override = -1;
}

static void smith_ui_wipe_active_panel(int col)
{
    if (smith_ui_portrait_layout())
    {
        Term_clear();
        smith_ui_reset_description_state();
        return;
    }

    wipe_screen_from(col);
}

static void smith_ui_clear_from_row(int row)
{
    int wid = smith_ui_term_wid();
    int hgt = smith_ui_content_bottom_row() + 1;

    if (row < 0)
        row = 0;
    if (row >= hgt)
        return;

    for (int y = row; y < hgt; y++)
        Term_erase(0, y, wid);
}

static void smith_ui_draw_horizontal_divider(int row)
{
    int wid = smith_ui_term_wid();

    if (row < 0 || row > smith_ui_content_bottom_row())
        return;

    for (int x = 0; x < wid; x++)
        Term_putch(x, row, TERM_L_DARK, '-');
}

static int smith_ui_used_bottom_row(void)
{
    if (!Term || !Term->scr)
        return 0;

    for (int y = smith_ui_content_bottom_row(); y >= 0; y--)
    {
        for (int x = 0; x < smith_ui_term_wid(); x++)
        {
            if ((Term->scr->c[y][x] != ' ')
                || (Term->scr->a[y][x] != Term->attr_blank)
                || (Term->scr->story[y][x] != 0))
            {
                return y;
            }
        }
    }

    return 0;
}

static int smith_ui_min_description_row(void)
{
    return smith_ui_used_bottom_row() + 1;
}

static int smith_ui_description_row_for_lines(int lines)
{
    int hgt = smith_ui_content_bottom_row() + 1;
    int min_row = smith_ui_min_description_row();
    int row;

    if (lines < 1)
        lines = 1;

    if (min_row >= hgt)
        return -1;

    row = hgt - lines;
    if (row < min_row)
        row = min_row;

    return row;
}

static int smith_ui_weight_col(void)
{
    int col = smith_ui_cost_col() - 10;

    if (smith_ui_portrait_layout())
        return -1;

    if (col <= COL_SMT2 + 16)
        return -1;

    return col;
}

static int smith_ui_safe_width(int col, int width)
{
    int term_wid = smith_ui_term_wid();
    int max_width;

    if (col < 0)
    {
        width += col;
        col = 0;
    }

    if (col >= term_wid || width <= 0)
        return 0;

    /* Leave the final column alone so Term_addstr() never marks the cursor
     * unusable after a fitted UI write. */
    max_width = term_wid - col - 1;
    if (max_width < 1 && term_wid > col)
        max_width = 1;

    if (width > max_width)
        width = max_width;

    return (width > 0) ? width : 0;
}

static int smith_ui_line_width(int col)
{
    return smith_ui_safe_width(col, smith_ui_term_wid() - col);
}

static int smith_ui_utf8_prefix_len(cptr text, int max_cols)
{
    int bytes = 0;
    int cols = 0;

    if (!text || max_cols <= 0)
        return 0;

    while (text[bytes])
    {
        int char_len = utf8_sequence_len(text + bytes);
        int char_width;

        if (char_len <= 0)
            break;

        char_width = utf8_display_width_n(text + bytes, char_len);
        if (char_width > 0 && cols + char_width > max_cols)
            break;

        cols += char_width;
        bytes += char_len;
    }

    return bytes;
}

static void smith_ui_fit_text(char* buf, size_t buflen, cptr text, int width)
{
    if (!buf || buflen == 0)
        return;

    if (!text)
        text = "";

    if (width <= 0)
    {
        buf[0] = '\0';
        return;
    }

    if (utf8_display_width_n(text, (int)strlen(text)) <= width)
    {
        SDL_strlcpy(buf, text, buflen);
        return;
    }

    if (width <= 3)
    {
        int copy_len = smith_ui_utf8_prefix_len(text, width);
        if (copy_len >= (int)buflen)
            copy_len = utf8_safe_prefix_len(text, (int)buflen - 1);
        if (copy_len < 0)
            copy_len = 0;
        SDL_memcpy(buf, text, (size_t)copy_len);
        buf[copy_len] = '\0';
        return;
    }

    {
        int copy_len = smith_ui_utf8_prefix_len(text, width - 3);
        if (copy_len >= (int)buflen)
            copy_len = utf8_safe_prefix_len(text, (int)buflen - 1);
        if (copy_len < 0)
            copy_len = 0;
        SDL_memcpy(buf, text, (size_t)copy_len);
        buf[copy_len] = '\0';
        SDL_strlcat(buf, "...", buflen);
    }
}

static void smith_ui_draw_fitted(int col, int row, int width, byte attr,
    cptr text, bool erase)
{
    char fitted[180];

    if (row < 0 || row > smith_ui_content_bottom_row())
        return;

    width = smith_ui_safe_width(col, width);
    if (width <= 0)
        return;

    smith_ui_fit_text(fitted, sizeof(fitted), text, width);
    if (erase)
        Term_erase(col, row, width);
    Term_putstr(col, row, width, attr, fitted);
}

static void smith_ui_put_fitted(int col, int row, int width, byte attr, cptr text)
{
    smith_ui_draw_fitted(col, row, width, attr, text, true);
}

static void smith_ui_fill_row(int col, int row, int width, byte attr)
{
    char fill[180];

    if (row < 0 || row > smith_ui_content_bottom_row())
        return;

    width = smith_ui_safe_width(col, width);
    if (width <= 0)
        return;
    if (width >= (int)sizeof(fill))
        width = (int)sizeof(fill) - 1;

    SDL_memset(fill, ' ', (size_t)width);
    fill[width] = '\0';
    Term_putstr(col, row, width, attr, fill);
}

static byte smith_ui_selected_attr(byte source_attr)
{
    (void)source_attr;
    return (byte)(TERM_UI_SELECTED + TERM_L_BLUE);
}

static int smith_ui_lookup_kind_quiet(int tval, int sval)
{
    int k;

    if (!z_info || !k_info)
        return 0;

    for (k = 1; k < z_info->k_max; k++)
    {
        object_kind* k_ptr = &k_info[k];

        if ((k_ptr->tval == tval) && (k_ptr->sval == sval))
            return k;
    }

    return 0;
}

static bool smith_ui_prepare_icon(object_type* icon, int tval, int sval)
{
    int k_idx;

    if (!icon)
        return false;

    object_wipe(icon);
    k_idx = smith_ui_lookup_kind_quiet(tval, sval);
    if (!k_idx)
        return false;

    object_prep(icon, k_idx);
    icon->ident |= (IDENT_KNOWN | IDENT_SPOIL);
    if (icon->number < 1)
        icon->number = 1;

    return true;
}

static void smith_ui_draw_icon(int col, int row, const object_type* icon,
    bool selected)
{
    if (!icon || !icon->k_idx)
        return;

    (void)selected;
    draw_supply_icon(col, row, icon);
}

static int smith_ui_icon_gap_width(void)
{
    return use_bigtile ? 3 : 2;
}

/*
 * When set, a single-list submenu (no sval/property column at COL_SMT3) lets its
 * COL_SMT2 list run all the way to the cost column instead of stopping short at
 * COL_SMT3. Menus that show a real third column leave this false so the columns
 * do not overlap.
 */
static bool smith_ui_secondary_full_width = false;

static int smith_ui_secondary_next_col(void)
{
    return smith_ui_secondary_full_width ? COL_SMT4 : COL_SMT3;
}

static int smith_ui_next_column_after(int col)
{
    int next_col = smith_ui_term_wid();

    if (smith_ui_portrait_layout())
        return next_col;

    if (col < COL_SMT2)
        next_col = COL_SMT2;
    else if (col < COL_SMT3)
        next_col = smith_ui_secondary_next_col();
    else if (col < COL_SMT4)
        next_col = COL_SMT4;

    if (next_col <= col)
        next_col = smith_ui_term_wid();

    return next_col;
}

static int smith_ui_menu_row_width(int col)
{
    int prefix_col = indexed_menu_prefix_col(col);
    int next_col = smith_ui_next_column_after(col);
    int width = next_col - prefix_col - 1;

    if (width < 1)
        width = smith_ui_line_width(prefix_col);

    return smith_ui_safe_width(prefix_col, width);
}

static void smith_ui_put_icon_menu_row(int choice, int col, int row,
    byte base_attr, cptr label, const object_type* icon, bool selected)
{
    char prefix[8];
    int prefix_col = indexed_menu_prefix_col(col);
    int prefix_w = indexed_menu_letters_enabled() ? 3 : 2;
    int icon_col = prefix_col + prefix_w;
    bool has_icon = icon && icon->k_idx;
    int label_col = has_icon ? (icon_col + smith_ui_icon_gap_width())
                             : (prefix_col + prefix_w);
    int row_w = smith_ui_menu_row_width(col);
    int label_w = row_w - (label_col - prefix_col);
    byte attr = selected ? smith_ui_selected_attr(base_attr) : base_attr;

    if (row < 0 || row > smith_ui_content_bottom_row())
        return;

    if (selected)
        indexed_menu_focus_prefix(prefix, sizeof(prefix), choice - 1);
    else
        indexed_menu_normal_prefix(prefix, sizeof(prefix), choice - 1);

    Term_erase(prefix_col, row, row_w);
    if (selected)
        smith_ui_fill_row(prefix_col, row, row_w, attr);

    /* When selected, the whole row is filled with the highlight above; draw the
     * prefix and label without erasing so the highlight survives in the gaps and
     * trailing spaces. */
    smith_ui_draw_fitted(prefix_col, row, prefix_w, attr, prefix, !selected);
    if (has_icon)
        smith_ui_draw_icon(icon_col, row, icon, selected);
    smith_ui_draw_fitted(label_col, row, label_w, attr, label, !selected);

    ui_menu_click_add(choice, prefix_col, row, row_w);
}

static int smith_ui_put_wrapped(int col, int row, int width, int max_lines,
    byte attr, cptr text)
{
    cptr s = text;
    int used = 0;

    width = smith_ui_safe_width(col, width);
    if (!s || width <= 0 || max_lines <= 0)
        return 0;

    while (*s && used < max_lines)
    {
        char line[180];
        int line_len = 0;

        while (*s == ' ')
            s++;

        while (*s)
        {
            cptr word = s;
            int word_len = 0;

            while (s[word_len] && s[word_len] != ' ')
                word_len++;

            if (word_len <= 0)
                break;

            if (line_len == 0)
            {
                int copy_len = smith_ui_utf8_prefix_len(word, width);

                if (copy_len <= 0 || copy_len > word_len)
                    copy_len = word_len;
                if (copy_len >= (int)sizeof(line))
                    copy_len = utf8_safe_prefix_len(
                        word, (int)sizeof(line) - 1);
                SDL_memcpy(line, word, (size_t)copy_len);
                line[copy_len] = '\0';
                line_len = (int)strlen(line);
                s += copy_len;
            }
            else if (line_len + 1 + word_len <= width
                && line_len + 1 + word_len < (int)sizeof(line))
            {
                line[line_len++] = ' ';
                SDL_memcpy(line + line_len, word, (size_t)word_len);
                line_len += word_len;
                line[line_len] = '\0';
                s += word_len;
            }
            else
            {
                break;
            }

            while (*s == ' ')
                s++;

            if (line_len >= width)
                break;
        }

        if (line_len == 0)
            break;

        smith_ui_put_fitted(col, row + used, width, attr, line);
        used++;
    }

    return used;
}

static void smith_ui_put_cost_line(int index0, byte attr, cptr text)
{
    int col = smith_ui_portrait_layout() ? COL_SMT1
                                         : smith_ui_cost_col() + 2;

    smith_ui_put_fitted(col, smith_ui_cost_item_row(index0),
        smith_ui_line_width(col), attr, text);
}

static int smith_ui_column_width(int col);

static void smith_ui_put_menu_row(int choice, int col, int row,
    byte base_attr, cptr label, bool selected)
{
    char prefix[8];
    int start_col = indexed_menu_prefix_col(col);
    int width = smith_ui_menu_row_width(col);
    int label_width = width - (col - start_col);
    byte attr = selected ? smith_ui_selected_attr(base_attr) : base_attr;

    if (row < 0 || row > smith_ui_content_bottom_row())
        return;

    if (width < 1)
        width = 1;
    if (label_width < 1)
        label_width = 1;

    Term_erase(start_col, row, width);
    if (selected)
        smith_ui_fill_row(start_col, row, width, attr);

    if (!indexed_menu_letters_enabled())
    {
        if (selected)
            indexed_menu_focus_prefix(prefix, sizeof(prefix), choice - 1);
        else
            indexed_menu_normal_prefix(prefix, sizeof(prefix), choice - 1);
        smith_ui_draw_fitted(start_col, row, 2, attr, prefix, false);
    }

    smith_ui_draw_fitted(col, row, label_width, attr, label, false);
    ui_menu_click_add(choice, start_col, row, width);
}

/*
 * Match the abilities menu's pointer contract: pointing/first tapping moves
 * the highlight, while activating an already-highlighted row confirms it.
 * Keyboard and controller confirmations do not pass through this helper.
 */
static bool smith_ui_pointer_choice_confirms(
    int choice, int action, int* highlight)
{
    bool same_choice;

    if (!highlight)
        return false;

    same_choice = (*highlight == choice);
    *highlight = choice;

    return action != UI_MENU_CLICK_HOVER && same_choice;
}

static void smith_ui_add_back_click_target(int col, int row, cptr text)
{
    ui_menu_click_add_text_token(SMITH_CLICK_BACK, col, row, text, "Esc");
    ui_menu_click_add_text_token(SMITH_CLICK_BACK, col, row, text, "ESC");
    ui_menu_click_add_text_token(SMITH_CLICK_BACK, col, row, text, "back");
    ui_menu_click_add_text_token(SMITH_CLICK_BACK, col, row, text, "return");
}

static void smith_ui_begin_touch_scroll_area(bool root_menu)
{
    int bottom_row = smith_ui_content_bottom_row();

    if (bottom_row < 1)
        bottom_row = 1;

    /* Keep the floating navigation control contextual: the root leaves
     * Smithing, while every nested screen returns by its normal Back action. */
    if (root_menu)
        ui_menu_click_set_touch_exit_button(true);
    else
        ui_menu_click_add_touch_button(
            SMITH_CLICK_BACK, "Back", TERM_DARK);

    ui_scroll_area_begin(1, bottom_row, SDL_TOUCH_MENU_CATEGORY_OTHER);
    ui_scroll_area_set_keys('8', '2', '6', '4');

    /* Register a valid zero-range target immediately.  Long lists replace this
     * with their own persistent viewport below.  Short Smithing screens still
     * consume touch drags without translating them into cursor movement. */
    if (sdl_touch_only_device_active())
        ui_scroll_area_set_offset_target(&smith_ui_touch_drag_sink, 0);
}

static int smith_ui_list_bottom_row(int first_row, bool reserve_detail)
{
    int bottom = smith_ui_content_bottom_row();
    int legacy_bottom = MAX_SMITHING_TVALS + 2;

    if (reserve_detail)
        bottom -= smith_ui_portrait_layout() ? 8 : 5;
    if (bottom > legacy_bottom)
        bottom = legacy_bottom;
    if (bottom < first_row)
        bottom = first_row;

    return bottom;
}

static int smith_ui_configure_list_view(smith_ui_scroll_id id, int count,
    int highlight, int first_row, int last_row)
{
    int visible;
    int max_top;
    int* top;
    bool touch_only = sdl_touch_only_device_active();

    if (id < 0 || id >= SMITH_SCROLL_MAX)
        return 0;

    visible = MAX(1, last_row - first_row + 1);
    max_top = MAX(0, count - visible);
    top = &smith_ui_scroll_top[id];

    if (touch_only)
        (void)ui_scroll_area_take_touch_scrolled();

    if (*top < 0)
        *top = 0;
    if (*top > max_top)
        *top = max_top;

    /* Inventory-style touch lists are viewport-driven: an off-screen selection
     * must not pull the list back after the finger drags it.  Non-touch control
     * schemes continue to keep keyboard/controller highlights visible. */
    if (!touch_only && count > 0)
    {
        int selected = MAX(0, MIN(count - 1, highlight - 1));

        if (selected < *top)
            *top = selected;
        else if (selected >= *top + visible)
            *top = selected - visible + 1;
    }

    if (touch_only)
        ui_scroll_area_set_offset_target(top, max_top);

    return *top;
}

static int smith_ui_visible_highlight_row(int highlight, int top,
    int first_row, int last_row)
{
    int selected = highlight - 1;
    int visible = MAX(1, last_row - first_row + 1);

    if (selected < top || selected >= top + visible)
        return -1;

    return first_row + selected - top;
}

static int smith_ui_column_width(int col)
{
    int next_col = smith_ui_term_wid();

    if (smith_ui_portrait_layout())
        return smith_ui_line_width(col);

    if (col < COL_SMT2)
        next_col = COL_SMT2;
    else if (col < COL_SMT3)
        next_col = smith_ui_secondary_next_col();
    else if (col < COL_SMT4)
        next_col = COL_SMT4;

    if (next_col <= col)
        next_col = smith_ui_term_wid();

    return MAX(1, next_col - col - 1);
}

static void smith_ui_put_header(int col, int row, cptr label)
{
    char buf[80];
    int width = smith_ui_column_width(col);

    if (!label)
        label = "";

    if ((int)strlen(label) <= width)
        SDL_strlcpy(buf, label, sizeof(buf));
    else if (width > 3)
        strnfmt(buf, sizeof(buf), "%.*s...", width - 3, label);
    else
        strnfmt(buf, sizeof(buf), "%.*s", width, label);

    smith_ui_put_fitted(col, row, width, TERM_WHITE, buf);
}

static void smith_ui_put_section_header(int col, int row, cptr label)
{
    smith_ui_put_header(col, row, label);
    smith_ui_add_back_click_target(COL_SMT1, 1, SMITH_ROOT_BACK_LABEL);
}

/*
 * A list of tvals and their textual names
 */
static const smithing_tval_desc smithing_tvals[MAX_SMITHING_TVALS] = {
    { CAT_WEAPON, TV_SWORD, "Sword" },
    { CAT_WEAPON, TV_POLEARM, "Axe or Polearm" },
    { CAT_WEAPON, TV_HAFTED, "Blunt Weapon" },
    { CAT_WEAPON, TV_DIGGING, "Digger" },
    { CAT_WEAPON, TV_BOW, "Bow" },
    { CAT_WEAPON, TV_ARROW, "Arrows" },
    { CAT_JEWELRY, TV_RING, "Ring" },
    { CAT_JEWELRY, TV_AMULET, "Amulet" },
    { CAT_JEWELRY, TV_LIGHT, "Light" },
    { CAT_JEWELRY, TV_HORN, "Horn" },
    { CAT_ARMOUR, TV_SOFT_ARMOR, "Soft Armour" },
    { CAT_ARMOUR, TV_MAIL, "Mail" },
    { CAT_ARMOUR, TV_CLOAK, "Cloak" },
    { CAT_ARMOUR, TV_SHIELD, "Shield" },
    { CAT_ARMOUR, TV_HELM, "Helm" },
    { CAT_ARMOUR, TV_GLOVES, "Gloves" },
    { CAT_ARMOUR, TV_BOOTS, "Boots" },
};

static bool smith_tval_icon(int tval, object_type* icon)
{
    switch (tval)
    {
    case TV_SWORD:
        return smith_ui_prepare_icon(icon, TV_SWORD, SV_LONG_SWORD);
    case TV_POLEARM:
        return smith_ui_prepare_icon(icon, TV_POLEARM, SV_HAND_AXE);
    case TV_HAFTED:
        return smith_ui_prepare_icon(icon, TV_HAFTED, SV_WAR_HAMMER);
    case TV_DIGGING:
        return smith_ui_prepare_icon(icon, TV_DIGGING, SV_MATTOCK);
    case TV_BOW:
        return smith_ui_prepare_icon(icon, TV_BOW, SV_LONG_BOW);
    case TV_ARROW:
        return smith_ui_prepare_icon(icon, TV_ARROW, SV_NORMAL_ARROW);
    case TV_RING:
        return smith_ui_prepare_icon(icon, TV_RING, SV_RING_ACCURACY);
    case TV_AMULET:
        return smith_ui_prepare_icon(icon, TV_AMULET, SV_AMULET_CON);
    case TV_LIGHT:
        return smith_ui_prepare_icon(icon, TV_LIGHT, SV_LIGHT_LANTERN);
    case TV_HORN:
        return smith_ui_prepare_icon(icon, TV_HORN, SV_HORN_WARNING);
    case TV_SOFT_ARMOR:
        return smith_ui_prepare_icon(icon, TV_SOFT_ARMOR, SV_LEATHER_ARMOR);
    case TV_MAIL:
        return smith_ui_prepare_icon(icon, TV_MAIL, SV_MAIL_CORSLET);
    case TV_CLOAK:
        return smith_ui_prepare_icon(icon, TV_CLOAK, SV_CLOAK);
    case TV_SHIELD:
        return smith_ui_prepare_icon(icon, TV_SHIELD, SV_ROUND_SHIELD);
    case TV_HELM:
        return smith_ui_prepare_icon(icon, TV_HELM, SV_HELM);
    case TV_GLOVES:
        return smith_ui_prepare_icon(icon, TV_GLOVES, SV_SET_OF_LEATHER_GLOVES);
    case TV_BOOTS:
        return smith_ui_prepare_icon(icon, TV_BOOTS, SV_PAIR_OF_LEATHER_BOOTS);
    default:
        return false;
    }
}

static bool object_has_evil_alignment(const object_type* o_ptr);

static void smith_clear_alloy_state(smith_alloy_state* state)
{
    state->type = SMITH_ALLOY_NONE;
    state->bonus_att = 0;
    state->bonus_ds = 0;
    state->bonus_evn = 0;
    state->bonus_ps = 0;
}

static void smith_remove_alloy_bonus(object_type* o_ptr, smith_alloy_state* state)
{
    if (!state)
        return;

    if (state->type != SMITH_ALLOY_NONE && o_ptr && o_ptr->k_idx)
    {
        o_ptr->att -= state->bonus_att;
        if (o_ptr->ds >= state->bonus_ds)
            o_ptr->ds -= state->bonus_ds;
        else
            o_ptr->ds = 0;
        o_ptr->evn -= state->bonus_evn;
        if (o_ptr->ps >= state->bonus_ps)
            o_ptr->ps -= state->bonus_ps;
        else
            o_ptr->ps = 0;
    }

    smith_clear_alloy_state(state);
}

static int smith_item_category(const object_type* o_ptr)
{
    if (!o_ptr)
        return -1;

    for (int i = 0; i < MAX_SMITHING_TVALS; i++)
    {
        if (smithing_tvals[i].tval == o_ptr->tval)
            return smithing_tvals[i].category;
    }

    return -1;
}

static bool smith_alloy_applicable(const object_type* o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx)
        return false;
    if (object_has_evil_alignment(o_ptr))
        return false;

    int cat = smith_item_category(o_ptr);
    if ((cat != CAT_WEAPON) && (cat != CAT_ARMOUR))
        return false;

    /* Cannot alloy items that are already made of special metal */
    object_kind* k_ptr = &k_info[o_ptr->k_idx];
    if (k_ptr->flags3 & (TR3_MITHRIL | TR3_STAR_IRON))
        return false;

    /* Armour: only major metal pieces */
    if (cat == CAT_ARMOUR)
    {
        switch (o_ptr->tval)
        {
        case TV_MAIL:
        case TV_SHIELD:
        case TV_HELM:
            return true;
        default:
            return false;
        }
    }

    /* Weapons: exclude quarterstaves (wooden) */
    if ((o_ptr->tval == TV_HAFTED) && (o_ptr->sval == SV_QUARTERSTAFF))
        return false;

    return true;
}

static bool smith_apply_alloy(object_type* o_ptr, smith_alloy_state* state, smith_alloy_type new_type)
{
    if (!o_ptr || !state)
        return false;

    smith_remove_alloy_bonus(o_ptr, state);

    if (new_type == SMITH_ALLOY_NONE)
        return true;

    if (!smith_alloy_applicable(o_ptr))
        return false;

    int cat = smith_item_category(o_ptr);
    if (cat == CAT_WEAPON)
    {
        if (new_type == SMITH_ALLOY_MITHRIL)
            state->bonus_att = 1;
        else if (new_type == SMITH_ALLOY_STAR_IRON)
            state->bonus_ds = 1;
    }
    else if (cat == CAT_ARMOUR)
    {
        if (new_type == SMITH_ALLOY_MITHRIL)
            state->bonus_evn = 1;
        else if (new_type == SMITH_ALLOY_STAR_IRON)
            state->bonus_ps = 1;
    }
    else
    {
        return false;
    }

    o_ptr->att += state->bonus_att;
    o_ptr->ds += state->bonus_ds;
    o_ptr->evn += state->bonus_evn;
    o_ptr->ps += state->bonus_ps;
    state->type = new_type;
    return true;
}

static int smith_alloy_weight_required(const object_type* o_ptr)
{
    int total_weight = o_ptr->weight * ((o_ptr->number > 0) ? o_ptr->number : 1);
    return (total_weight + 3) / 4;
}

/*
 * A structure to hold a flag and its smithing category
 */
typedef struct smithing_flag_cat
{
    int category;
    cptr desc;
} smithing_flag_cat;

#define CAT_STAT 1
#define CAT_SUST 2
#define CAT_SKILL 3
#define CAT_MEL 4
#define CAT_SLAY 5
#define CAT_RES 6
#define CAT_MISC 7

#define MAX_CATS 7

#define MAX_SMITHING_FLAGS (32 * 4)

static const smithing_flag_cat smithing_flag_cats[]
    = { { CAT_STAT, "Stat bonuses" }, { CAT_SUST, "Sustains" },
          { CAT_SKILL, "Skill bonuses" }, { CAT_MEL, "Melee powers" },
          { CAT_SLAY, "Slays" }, { CAT_RES, "Resistances" },
          { CAT_MISC, "Misc" } };

/*
 * A structure to hold a flag and its smithing category
 */
typedef struct smithing_flag_desc
{
    int category;
    u32b flag;
    int flagset;
    cptr desc;
} smithing_flag_desc;

/*
 * A list of tvals and their textual names
 */
static const smithing_flag_desc smithing_flag_types[] = { { CAT_STAT, TR1_STR,
                                                              1, "Str bonus" },
    { CAT_STAT, TR1_DEX, 1, "Dex bonus" },
    { CAT_STAT, TR1_CON, 1, "Con bonus" },
    { CAT_STAT, TR1_GRA, 1, "Gra bonus" },
    { CAT_STAT, TR1_NEG_STR, 1, "Str penalty" },
    { CAT_STAT, TR1_NEG_DEX, 1, "Dex penalty" },
    { CAT_STAT, TR1_NEG_CON, 1, "Con penalty" },
    { CAT_STAT, TR1_NEG_GRA, 1, "Gra penalty" },
    { CAT_SKILL, TR1_ARC, 1, "Archery" }, { CAT_SKILL, TR1_STL, 1, "Stealth" },
    { CAT_SKILL, TR1_PER, 1, "Perception" }, { CAT_SKILL, TR1_WIL, 1, "Will" },
    { CAT_SKILL, TR1_SMT, 1, "Smithing" }, { CAT_SKILL, TR1_SNG, 1, "Song" },
    { CAT_MISC, TR1_DAMAGE_SIDES, 1, "Damage bonus" },
    { CAT_MISC, TR2_LIGHT, 2, "Light" },
    { CAT_MISC, TR2_SLOW_DIGEST, 2, "Sustenance" },
    { CAT_MISC, TR2_REGEN, 2, "Regeneration" },
    { CAT_MISC, TR2_SEE_INVIS, 2, "See Invisible" },
    { CAT_MISC, TR2_FREE_ACT, 2, "Free Action" },
    { CAT_MISC, TR2_SPEED, 2, "Speed" },
    { CAT_MISC, TR2_RADIANCE, 2, "Radiance" },
    { CAT_MISC, TR3_CHEAT_DEATH, 3, "Cheat Death" },
    { CAT_MISC, TR3_STAND_FAST, 3, "Stand Fast" },
    { CAT_MISC, TR3_AVOID_TRAPS, 3, "Avoid Traps" },
    { CAT_MISC, TR3_MEDIC, 3, "Medicine Bonus" },
    { CAT_MISC, TR4_PROT_FIRE, 4, "Protection vs Fire" },
    { CAT_MISC, TR4_PROT_COLD, 4, "Protection vs Cold" },
    { CAT_MISC, TR4_PROT_POIS, 4, "Protection vs Poison" },
    { CAT_MISC, TR4_PROT_DARK, 4, "Protection vs Darkness" },
    { CAT_MEL, TR1_TUNNEL, 1, "Tunneling Bonus" },
    { CAT_MEL, TR1_SHARPNESS, 1, "Sharpness" },
    { CAT_MEL, TR1_SHARPNESS2, 1, "Sharpness2" },
    { CAT_MEL, TR1_VAMPIRIC, 1, "Vampiric" },
    { CAT_MEL, TR3_ACCURATE, 3, "Accurate" },
    { CAT_SLAY, TR1_SLAY_ORC, 1, "Slay Orc" },
    { CAT_SLAY, TR1_SLAY_TROLL, 1, "Slay Troll" },
    { CAT_SLAY, TR1_SLAY_WOLF, 1, "Slay Wolf" },
    { CAT_SLAY, TR1_SLAY_SPIDER, 1, "Slay Spider" },
    { CAT_SLAY, TR1_SLAY_UNDEAD, 1, "Slay Undead" },
    { CAT_SLAY, TR1_SLAY_RAUKO, 1, "Slay Rauko" },
    { CAT_SLAY, TR1_SLAY_DRAGON, 1, "Slay Dragon" },
    { CAT_SLAY, TR4_SLAY_SERPENT, 4, "Slay Serpent" },
    { CAT_SLAY, TR4_SLAY_VAMPIRE, 4, "Slay Vampire" },
    { CAT_SLAY, TR4_SLAY_HORROR, 4, "Slay Horror" },
    { CAT_SLAY, TR4_SLAY_CAT, 4, "Slay Cat" },
    { CAT_SLAY, TR4_SLAY_GIANT, 4, "Slay Giant" },
    { CAT_SLAY, TR1_BRAND_COLD, 1, "Brand with Cold" },
    { CAT_SLAY, TR1_BRAND_FIRE, 1, "Brand with Fire" },
    { CAT_SLAY, TR1_BRAND_POIS, 1, "Brand with Poison" },
    { CAT_SUST, TR2_SUST_STR, 2, "Sustain Str" },
    { CAT_SUST, TR2_SUST_DEX, 2, "Sustain Dex" },
    { CAT_SUST, TR2_SUST_CON, 2, "Sustain Con" },
    { CAT_SUST, TR2_SUST_GRA, 2, "Sustain Gra" },
    { CAT_RES, TR2_RES_COLD, 2, "Resist Cold" },
    { CAT_RES, TR2_RES_FIRE, 2, "Resist Fire" },
    { CAT_RES, TR2_RES_POIS, 2, "Resist Poison" },
    { CAT_RES, TR2_RES_BLEED, 2, "Resist Bleeding" },
    { CAT_RES, TR2_RES_FEAR, 2, "Resist Fear" },
    { CAT_RES, TR2_RES_BLIND, 2, "Resist Blindness" },
    { CAT_RES, TR2_RES_CONFU, 2, "Resist Confusion" },
    { CAT_RES, TR2_RES_STUN, 2, "Resist Stunning" },
    { CAT_RES, TR2_RES_HALLU, 2, "Resist Hallucination" }, { 0, 0, 0, "" } };

/*
 * Artifice (custom artefact) bonus limits.
 *
 * When smithing a custom artefact, the item's max values from the R: line
 * are extended by these per-category bonuses.  All artefact-specific limits
 * live in this single table so they are easy to find and tune.
 *
 * 'bonus' fields are ADDED to the normal max (e.g. weapon att = max_att + 4).
 * 'floor' fields set a MINIMUM artefact max (e.g. rings always reach att 4).
 * The result is: artefact_max = max(normal_max + ego + bonus, floor).
 */

/* Forward declarations for data-driven smithing limit functions */
static void smithing_ego_bonus_sums(const object_type* o_ptr,
    int* max_att_sum, int* max_att_min_inc,
    int* to_ds_sum, int* to_ds_min_inc,
    int* max_evn_sum, int* max_evn_min_inc,
    int* to_ps_sum, int* to_ps_min_inc,
    int* max_pval_sum, int* max_pval_min_inc);
int att_max(void);
int att_min(void);
int ds_max(void);
int ds_min(void);
int evn_max(void);
int evn_min(void);
int ps_max(void);
int ps_min(void);

typedef struct
{
    int att_bonus;
    int att_floor;   /* 0 = unused */
    int ds_bonus;
    int evn_bonus;
    int evn_floor;   /* 0 = unused */
    int ps_bonus;
    int ps_floor;    /* 0 = unused */
    int pval_bonus;
} artifice_limits_t;

/* Indexed by a small enum - looked up via artifice_bonus_for(). */
enum {
    ARTIFICE_ARROW,
    ARTIFICE_MELEE,     /* sword, polearm, hafted */
    ARTIFICE_BOW,
    ARTIFICE_DIGGING,
    ARTIFICE_ARMOR,
    ARTIFICE_GLOVES,
    ARTIFICE_RING,
    ARTIFICE_AMULET,
    ARTIFICE_DEFAULT,
    ARTIFICE_MAX
};

static const artifice_limits_t artifice_table[ARTIFICE_MAX] = {
    /*               att_b att_f ds_b evn_b evn_f ps_b ps_f pval_b */
    /* ARROW   */  {  8,    0,    0,   0,    0,    0,   0,   0  },
    /* MELEE   */  {  4,    0,    2,   1,    0,    0,   0,   4  },
    /* BOW     */  {  4,    0,    2,   0,    0,    0,   0,   4  },
    /* DIGGING */  {  4,    0,    2,   0,    0,    0,   0,   4  },
    /* ARMOR   */  {  1,    0,    0,   1,    0,    2,   0,   4  },
    /* GLOVES  */  {  2,    0,    0,   1,    0,    2,   0,   4  },
    /* RING    */  {  0,    4,    0,   0,    4,    0,   0,   4  },
    /* AMULET  */  {  0,    0,    0,   0,    0,    0,   3,   4  },
    /* DEFAULT */  {  0,    0,    0,   0,    0,    0,   0,   4  },
};

static int artifice_category(const object_type* o_ptr)
{
    switch (o_ptr->tval)
    {
    case TV_ARROW:      return ARTIFICE_ARROW;
    case TV_SWORD:
    case TV_POLEARM:
    case TV_HAFTED:     return ARTIFICE_MELEE;
    case TV_BOW:        return ARTIFICE_BOW;
    case TV_DIGGING:    return ARTIFICE_DIGGING;
    case TV_GLOVES:     return ARTIFICE_GLOVES;
    case TV_BOOTS:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:       return ARTIFICE_ARMOR;
    case TV_RING:       return ARTIFICE_RING;
    case TV_AMULET:     return ARTIFICE_AMULET;
    default:            return ARTIFICE_DEFAULT;
    }
}

static const artifice_limits_t* artifice_bonus_for(const object_type* o_ptr)
{
    return &artifice_table[artifice_category(o_ptr)];
}

/*
 * Determines whether the attack bonus of an item is eligible for modification.
 */
int att_valid(void)
{
    return att_max() > att_min();
}

/*
 * Determines the maximum legal attack bonus for an item.
 * Uses data-driven max_att from object.txt R: lines, ego sums,
 * and the artifice table for custom artefacts.
 */
int att_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_att_sum = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, &max_att_sum, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    int att = k_ptr->max_att;
    att += max_att_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        att += al->att_bonus;
        if (al->att_floor > att)
            att = al->att_floor;
    }

    return (att);
}

/*
 * Determines the minimum legal attack bonus for an item.
 */
int att_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_att_min_inc = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, &max_att_min_inc, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    int att = k_ptr->att;
    att += max_att_min_inc;
    return (att);
}

/*
 * Determines whether the damage sides of an item is eligible for modification.
 */
int ds_valid(void)
{
    return ds_max() > ds_min();
}

/*
 * Determines the maximum legal damage sides for an item.
 */
int ds_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ds_sum = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, &to_ds_sum, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    int ds = k_ptr->max_ds;
    ds += to_ds_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        ds += al->ds_bonus;
    }

    return (ds);
}

/*
 * Determines the minimum legal damage sides for an item.
 */
int ds_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ds_min_inc = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, &to_ds_min_inc, NULL, NULL, NULL, NULL, NULL, NULL);

    int ds = k_ptr->ds;
    ds += to_ds_min_inc;

    /* Never allow weapons to reach 0-sided damage. */
    if (k_ptr->dd > 0 && ds < 1)
        ds = 1;

    return (ds);
}

/*
 * Determines whether the evasion bonus of an item is eligible for modification.
 */
int evn_valid(void)
{
    return evn_max() > evn_min();
}

/*
 * Determines the maximum legal evasion bonus for an item.
 */
int evn_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_evn_sum = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, &max_evn_sum, NULL, NULL, NULL, NULL, NULL);

    int evn = k_ptr->max_evn;
    evn += max_evn_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        evn += al->evn_bonus;
        if (al->evn_floor > evn)
            evn = al->evn_floor;
    }

    return (evn);
}

/*
 * Determines the minimum legal evasion bonus for an item.
 */
int evn_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int max_evn_min_inc = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, &max_evn_min_inc, NULL, NULL, NULL, NULL);

    int evn = k_ptr->evn;
    evn += max_evn_min_inc;
    return (evn);
}

/*
 * Determines whether the protection sides of an item is eligible for
 * modification.
 */
int ps_valid(void)
{
    return ps_max() > ps_min();
}

/*
 * Determines the maximum legal protection sides for an item.
 */
int ps_max()
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ps_sum = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, NULL, &to_ps_sum, NULL, NULL, NULL);

    int ps = k_ptr->max_ps;
    ps += to_ps_sum;

    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        ps += al->ps_bonus;
        if (al->ps_floor > ps)
            ps = al->ps_floor;
    }

    return (ps);
}

/*
 * Determines the minimum legal protection sides for an item.
 */
int ps_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int to_ps_min_inc = 0;
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &to_ps_min_inc, NULL, NULL);

    int ps = k_ptr->ps;
    ps += to_ps_min_inc;
    return (ps);
}

static bool smithing_variable_protection_dice(const object_type* o_ptr)
{
    return o_ptr && o_ptr->tval == TV_AMULET
        && ((o_ptr->sval == SV_AMULET_PROTECTION)
            || (o_ptr->name1 && (o_ptr->pd > 0)));
}

typedef struct
{
    byte pd;
    byte ps;
} smithing_protection_combo;

static const smithing_protection_combo smithing_amulet_protection_combos[] = {
    { 1, 1 },
    { 1, 2 },
    { 1, 3 },
    { 2, 1 },
    { 2, 2 },
    { 2, 3 },
};

static int smithing_protection_combo_index(const object_type* o_ptr)
{
    size_t i;

    if (!smithing_variable_protection_dice(o_ptr))
        return -1;

    for (i = 0; i < N_ELEMENTS(smithing_amulet_protection_combos); i++)
    {
        if ((o_ptr->pd == smithing_amulet_protection_combos[i].pd)
            && (o_ptr->ps == smithing_amulet_protection_combos[i].ps))
        {
            return (int)i;
        }
    }

    return -1;
}

static void smithing_set_protection_combo(object_type* o_ptr, int combo_idx)
{
    if (!o_ptr)
        return;

    if (combo_idx < 0 || combo_idx >= (int)N_ELEMENTS(smithing_amulet_protection_combos))
        return;

    o_ptr->pd = smithing_amulet_protection_combos[combo_idx].pd;
    o_ptr->ps = smithing_amulet_protection_combos[combo_idx].ps;
}

static bool smithing_can_increase_protection(const object_type* o_ptr)
{
    int combo_idx;

    if (!o_ptr)
        return false;

    if (!smithing_variable_protection_dice(o_ptr))
    {
        if (o_ptr->ps < ps_max())
            return true;

        return false;
    }

    combo_idx = smithing_protection_combo_index(o_ptr);
    if (combo_idx >= 0)
        return combo_idx < (int)N_ELEMENTS(smithing_amulet_protection_combos) - 1;

    return (o_ptr->pd <= 1) && (o_ptr->ps < 1);
}

static bool smithing_can_decrease_protection(const object_type* o_ptr)
{
    int combo_idx;

    if (!o_ptr)
        return false;

    if (!smithing_variable_protection_dice(o_ptr))
    {
        if (o_ptr->ps > ps_min())
            return true;

        return false;
    }

    combo_idx = smithing_protection_combo_index(o_ptr);
    if (combo_idx > 0)
        return true;

    return combo_idx == 0 && ps_min() < 1;
}

static void smithing_increase_protection(object_type* o_ptr)
{
    int combo_idx;

    if (!o_ptr)
        return;

    if (!smithing_variable_protection_dice(o_ptr))
    {
        o_ptr->ps++;
        return;
    }

    combo_idx = smithing_protection_combo_index(o_ptr);
    if (combo_idx >= 0)
    {
        smithing_set_protection_combo(o_ptr, combo_idx + 1);
        return;
    }

    smithing_set_protection_combo(o_ptr, 0);
}

static void smithing_decrease_protection(object_type* o_ptr)
{
    int combo_idx;

    if (!o_ptr)
        return;

    if (!smithing_variable_protection_dice(o_ptr))
    {
        o_ptr->ps--;
        return;
    }

    combo_idx = smithing_protection_combo_index(o_ptr);
    if (combo_idx > 0)
    {
        smithing_set_protection_combo(o_ptr, combo_idx - 1);
        return;
    }

    if (combo_idx == 0 && ps_min() < 1)
    {
        o_ptr->pd = 1;
        o_ptr->ps = 0;
    }
}

/*
 * Determines whether the pval of an item is eligible for modification.
 */
int pval_valid(void)
{
    u32b f1, f2, f3;

    object_flags(smith_o_ptr, &f1, &f2, &f3);

    return (f1 & (TR1_PVAL_MASK));
}

/*
 * Determines the maximum legal pval for an item.
 * Uses data-driven max_pval from object.txt R: lines, ego sums,
 * and the artifice table for custom artefacts.
 */
int pval_max(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    u32b f1, f2, f3;
    int max_pval_sum = 0;
    int max_pval_min_inc = 0;

    object_flags(smith_o_ptr, &f1, &f2, &f3);
    smithing_ego_bonus_sums(
        smith_o_ptr, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &max_pval_sum, &max_pval_min_inc);

    /* Start with the data-driven max from R: line */
    int pval = k_ptr->max_pval;

    /* Artefact bonus from the centralized artifice table */
    if (smith_o_ptr->name1)
    {
        const artifice_limits_t* al = artifice_bonus_for(smith_o_ptr);
        pval += al->pval_bonus;
    }

    /* Ego items have pvals limited by their 'special.txt' C: entries. */
    if (cursed_p(smith_o_ptr))
    {
        pval -= max_pval_min_inc;
    }
    else
    {
        pval += max_pval_sum;
    }

    return (pval);
}

/*
 * Determines the minimum legal pval for an item.
 * Accounts for ego min_pval requirements from special.txt C: line.
 */
int pval_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int base_min = k_ptr->pval;

    /* Check both prefix and suffix egos for min_pval requirements */
    byte egos[2] = { object_ego_prefix(smith_o_ptr), object_ego_suffix(smith_o_ptr) };
    for (int i = 0; i < 2; i++)
    {
        byte e_idx = egos[i];
        if (!e_idx)
            continue;

        ego_item_type* e_ptr = &e_info[e_idx];
        if (e_ptr->min_pval > 0)
        {
            /* Ego requires a minimum pval contribution */
            base_min += e_ptr->min_pval;
        }
        else if (e_ptr->max_pval > 0)
        {
            /* Default: at least +1 pval when ego grants pval */
            base_min += 1;
        }
    }

    return base_min;
}

static void smithing_ego_bonus_sums(const object_type* o_ptr,
    int* max_att_sum, int* max_att_min_inc,
    int* to_ds_sum, int* to_ds_min_inc,
    int* max_evn_sum, int* max_evn_min_inc,
    int* to_ps_sum, int* to_ps_min_inc,
    int* max_pval_sum, int* max_pval_min_inc)
{
    if (max_att_sum) *max_att_sum = 0;
    if (max_att_min_inc) *max_att_min_inc = 0;
    if (to_ds_sum) *to_ds_sum = 0;
    if (to_ds_min_inc) *to_ds_min_inc = 0;
    if (max_evn_sum) *max_evn_sum = 0;
    if (max_evn_min_inc) *max_evn_min_inc = 0;
    if (to_ps_sum) *to_ps_sum = 0;
    if (to_ps_min_inc) *to_ps_min_inc = 0;
    if (max_pval_sum) *max_pval_sum = 0;
    if (max_pval_min_inc) *max_pval_min_inc = 0;

    if (!o_ptr || !o_ptr->k_idx)
        return;

    byte egos[2] = { object_ego_prefix(o_ptr), object_ego_suffix(o_ptr) };
    for (int i = 0; i < 2; i++)
    {
        byte e_idx = egos[i];
        if (!e_idx)
            continue;

        ego_item_type* e_ptr = &e_info[e_idx];
        int max_att = (int)(int8_t)e_ptr->max_att;
        int to_ds = (int)(int8_t)e_ptr->to_ds;
        int max_evn = (int)(int8_t)e_ptr->max_evn;
        int to_ps = (int)(int8_t)e_ptr->to_ps;

        if (max_att)
        {
            if (max_att_sum) *max_att_sum += max_att;
            if (max_att_min_inc)
                (*max_att_min_inc) += (max_att > 0) ? 1 : -1;
        }
        if (to_ds)
        {
            if (to_ds_sum) *to_ds_sum += to_ds;
            if (to_ds_min_inc)
                (*to_ds_min_inc) += (to_ds > 0) ? 1 : -1;
        }
        if (max_evn)
        {
            if (max_evn_sum) *max_evn_sum += max_evn;
            if (max_evn_min_inc)
                (*max_evn_min_inc) += (max_evn > 0) ? 1 : -1;
        }
        if (to_ps)
        {
            if (to_ps_sum) *to_ps_sum += to_ps;
            if (to_ps_min_inc)
                (*to_ps_min_inc) += (to_ps > 0) ? 1 : -1;
        }

        if (e_ptr->max_pval > 0)
        {
            if (max_pval_sum) *max_pval_sum += e_ptr->max_pval;
            if (max_pval_min_inc)
                (*max_pval_min_inc) += (e_ptr->min_pval > 0) ? e_ptr->min_pval : 1;
        }
    }
}

/*
 * Determines whether the weight of an item is eligible for modification.
 */
int wgt_valid(void)
{
    switch (smith_o_ptr->tval)
    {
    case TV_ARROW:
    case TV_RING:
    case TV_AMULET:
    case TV_LIGHT:
    case TV_HORN:
    {
        return (false);
    }
    }

    return (true);
}

/*
 * Determines the maximum legal weight for an item.
 */
int wgt_max(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int weight = div_round(k_ptr->weight, 2) * 3;
    return (weight);
}

/*
 * Determines the minimum legal weight for an item.
 */
int wgt_min(void)
{
    object_kind* k_ptr = &k_info[smith_o_ptr->k_idx];
    int weight = div_round(k_ptr->weight, 3) * 2;
    return (weight);
}

/*
 * Moves the light blue highlighted letter.
 */
void move_displayed_highlight(
    int old_highlight, byte old_attr, int new_highlight, int col)
{
    char buf[80];

    // remove highlight from the old label
    indexed_menu_normal_prefix(buf, sizeof(buf), old_highlight - 1);
    Term_putstr(indexed_menu_prefix_col(col), old_highlight + 1, -1, old_attr,
        buf);

    // highlight the new label
    indexed_menu_focus_prefix(buf, sizeof(buf), new_highlight - 1);
    Term_putstr(indexed_menu_prefix_col(col), new_highlight + 1, -1,
        TERM_L_BLUE, buf);
}

bool melt_metal_item(int item_num)
{
    int number = 0;
    int item, i;
    u32b f1, f2, f3;

    for (item = 0; item < INVEN_TOTAL; item++)
    {
        object_type* o_ptr = &inventory[item];

        object_flags(o_ptr, &f1, &f2, &f3);

        /* Skip metal items that can't be melted (Gamil-forged) */
        if ((f3 & (TR3_MITHRIL | TR3_STAR_IRON)) && !(o_ptr->ident & IDENT_CANT_MELT))
        {
            number += 1;
        }

        if (number == item_num)
        {
            int slots_needed = o_ptr->weight / 99;
            int empty_slots = 0;

            // Equipments needs an extra slot
            if (item >= INVEN_WIELD)
                slots_needed++;

            // Count empty slots
            for (i = INVEN_PACK - 1; i > 0; i--)
            {
                if (!(&inventory[i])->k_idx)
                    empty_slots++;
            }

            if (empty_slots < slots_needed)
            {
                msg_print("You do not have enough room in your pack.");
                if (slots_needed - empty_slots == 1)
                {
                    msg_print("You must free up another slot.");
                }
                else
                {
                    msg_format("You must free up %d more slots.",
                        slots_needed - empty_slots);
                }
                return (false);
            }

            {
                char o_name[80];
                char prompt[160];

                object_desc(o_name, sizeof(o_name), o_ptr, false, 4);
                strnfmt(prompt, sizeof(prompt), "Melt %s down? ", o_name);
                if (!get_check(prompt))
                    return (false);
            }

            {
                int slot;
                object_type* i_ptr;
                object_type object_type_body;
                int metal_sval;

                // Determine which metal type to create
                if (f3 & TR3_STAR_IRON)
                    metal_sval = SV_METAL_STAR_IRON;
                else
                    metal_sval = SV_METAL_MITHRIL;

                // Get local object
                i_ptr = &object_type_body;

                // Prepare the base object for the metal
                object_prep(i_ptr, lookup_kind(TV_METAL, metal_sval));

                // set the appropriate quantity
                i_ptr->number = o_ptr->weight;

                // remove the item
                inven_item_increase(item, -1);
                inven_item_describe(item);
                inven_item_optimize(item);
                window_stuff();

                // give the mithril to the player...

                // if there is too much, then break it up
                while (i_ptr->number > 99)
                {
                    object_type* i_ptr2;
                    object_type object_type_body2;

                    // Get local object
                    i_ptr2 = &object_type_body2;

                    // decrease the main stack
                    i_ptr->number -= 99;

                    // Prepare the base object for the metal
                    object_prep(
                        i_ptr2, lookup_kind(TV_METAL, metal_sval));

                    // increase the new stack
                    i_ptr2->number = 99;

                    // give it to the player
                    slot = inven_carry(i_ptr2, true);
                    if ((slot >= 0) && (slot < INVEN_TOTAL))
                    {
                        inven_item_optimize(slot);
                        inven_item_describe(slot);
                    }
                    else
                    {
                        drop_near(i_ptr2, 0, p_ptr->py, p_ptr->px);
                        msg_print("Some metal falls to the floor.");
                    }
                    window_stuff();
                }

                // now give the last stack of mithril to the player
                slot = inven_carry(i_ptr, true);
                if ((slot >= 0) && (slot < INVEN_TOTAL))
                {
                    inven_item_optimize(slot);
                    inven_item_describe(slot);
                }
                else
                {
                    drop_near(i_ptr, 0, p_ptr->py, p_ptr->px);
                    msg_print("Some metal falls to the floor.");
                }
                window_stuff();

                return (true);
            }
        }
    }

    return (false);
}

static int meltable_metal_items_carried(void)
{
    int number = 0;
    int item;
    u32b f1, f2, f3;

    for (item = 0; item < INVEN_TOTAL; item++)
    {
        object_type* o_ptr = &inventory[item];

        object_flags(o_ptr, &f1, &f2, &f3);

        /* Only count metal items that can be melted (exclude Gamil-forged) */
        if ((f3 & (TR3_MITHRIL | TR3_STAR_IRON))
            && !(o_ptr->ident & IDENT_CANT_MELT))
        {
            number += 1;
        }
    }

    return (number);
}

static int metal_carried(byte sval)
{
    int w = 0;
    int item;

    for (item = 0; item < INVEN_WIELD; item++)
    {
        object_type* o_ptr = &inventory[item];

        if ((o_ptr->tval == TV_METAL) && (o_ptr->sval == sval))
        {
            w += o_ptr->number;
        }
    }

    return (w);
}

int mithril_carried(void)
{
    return metal_carried(SV_METAL_MITHRIL);
}

int star_iron_carried(void)
{
    return metal_carried(SV_METAL_STAR_IRON);
}

static void use_metal(byte sval, int cost)
{
    int item;

    for (item = INVEN_WIELD - 1; item >= 0 && cost > 0; item--)
    {
        object_type* o_ptr = &inventory[item];

        if ((o_ptr->tval == TV_METAL) && (o_ptr->sval == sval))
        {
            int use = MIN(o_ptr->number, cost);
            inven_item_increase(item, -use);
            inven_item_describe(item);
            inven_item_optimize(item);
            cost -= use;
        }
    }
}

void use_mithril(int cost)
{
    use_metal(SV_METAL_MITHRIL, cost);
}

void use_star_iron(int cost)
{
    use_metal(SV_METAL_STAR_IRON, cost);
}

/*
 * Determines how many uses are left for a given forge.
 */
int forge_uses(int y, int x)
{
    byte feat = cave_feat[y][x];

    if (!cave_forge_bold(y, x))
        return (0);

    if (feat <= FEAT_FORGE_NORMAL_TAIL)
        return (feat - FEAT_FORGE_NORMAL_HEAD);
    if (feat <= FEAT_FORGE_GOOD_TAIL)
        return (feat - FEAT_FORGE_GOOD_HEAD);
    else
        return (feat - FEAT_FORGE_UNIQUE_HEAD);
}

/*
 * Determines how high a bonus is provided by a given forge.
 */
int forge_bonus(int y, int x)
{
    byte feat = cave_feat[y][x];

    if (!cave_forge_bold(y, x))
        return (0);

    if (feat <= FEAT_FORGE_NORMAL_TAIL)
        return (0);
    if (feat <= FEAT_FORGE_GOOD_TAIL)
        return (3);
    else
        return (7);
}

/*
 * Determines the difficulty modifier for pvals.
 *
 * The marginal difficulty of increasing a pval increases by 1 each time, if the
 * base is up to 5, by 2 each time if the base is 6--10, and so on.
 */
void dif_mod(int value, int positive_base, int* dif_inc)
{
    int mod = 1 + ((positive_base - 1) / 5);

    // deal with positive values in a triangular number influenced way
    if (value > 0)
    {
        *dif_inc += positive_base * value + mod * (value * (value - 1) / 2);
    }
}

/*
 * Signed difficulty modifier.
 *
 * Positive values use the normal triangular progression.
 * Negative values reduce difficulty, but only by half as much as the matching
 * positive bonus would increase it.
 */
static int dif_mod_signed(int value, int positive_base)
{
    int mod = 1 + ((positive_base - 1) / 5);

    if (value > 0)
    {
        return positive_base * value + mod * (value * (value - 1) / 2);
    }
    else if (value < 0)
    {
        int abs_value = -value;
        int negative_base = (positive_base + 1) / 2;
        int negative_mod = 1 + ((negative_base - 1) / 5);
        return -(negative_base * abs_value
            + negative_mod * (abs_value * (abs_value - 1) / 2));
    }

    return 0;
}

/*
 * Determines the difficulty of a given object.
 */
int object_difficulty(object_type* o_ptr)
{
    object_kind* k_ptr = &k_info[o_ptr->k_idx];
    int x, new, base;
    int i;
    int dif = 0;
    int dif_inc = 0;
    int dif_dec = 0;
    int weight_factor;
    u32b f1, f2, f3, f4;
    int brands = 0;
    int dif_mult = 100;
    int cat = 0; // default to soothe compilation warnings

    bool telchar_bonus = (c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_TELCHAR);
    bool feanor_bonus  = (c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_FEANOR);

    // reset smithing costs
    smithing_cost.str = 0;
    smithing_cost.dex = 0;
    smithing_cost.con = 0;
    smithing_cost.gra = 0;
    smithing_cost.exp = 0;
    smithing_cost.mithril = 0;
    smithing_cost.star_iron = 0;
    smithing_cost.alloy_weight = 0;
    smithing_cost.alloy_metal = SMITH_ALLOY_NONE;
    smithing_cost.alloy_mastery = 0;
    smithing_cost.uses = 1;
    smithing_cost.drain = 0;
    smithing_cost.weaponsmith = 0;
    smithing_cost.armoursmith = 0;
    smithing_cost.jeweller = 0;
    smithing_cost.enchantment = 0;
    smithing_cost.artifice = 0;

    // extract object flags
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    int att_base = o_ptr->att;
    int evn_base = o_ptr->evn;
    int ds_base = o_ptr->ds;
    int ps_base = o_ptr->ps;

    /* When smithing, ignore the optional alloy bonus for difficulty/costs. */
    if (o_ptr == smith_o_ptr)
    {
        att_base -= smith_alloy.bonus_att;
        evn_base -= smith_alloy.bonus_evn;
        ds_base -= smith_alloy.bonus_ds;
        ps_base -= smith_alloy.bonus_ps;
    }

    /* ------------------------------------------------------------------
     *  GAMIL character bonus
     *   Craft mithril items without mithril material
     *   Costs 3 forge uses instead of 1
     *   Mark item with TR3_CANT_MELT so the melt-menu ignores it
     * ------------------------------------------------------------------ */


    /* Telchar: 25 % discount on Sharpness tiers */
    if (telchar_bonus && (f1 & (TR1_SHARPNESS | TR1_SHARPNESS2) || (f3 & TR3_ACCURATE)))
        dif_mult -= 25;

    /*  FEANOR character bonus
     *   40% off on all lamps
     *   25% off on any fire- or light-branded object */
    if (feanor_bonus)
    {
        /* 40% off on all lamps */
        if (o_ptr->tval == TV_LIGHT)
            dif_mult -= 40;
        /* 25% off on any fire- or light-branded object */
        else if ((f1 & TR1_BRAND_FIRE) || (f2 & (TR2_LIGHT | TR2_RADIANCE)))
            dif_mult -= 25;
    }

    // special rules for horns
    if (o_ptr->tval == TV_HORN)
    {
        dif_inc += k_ptr->level - 1;
        switch (o_ptr->sval)
        {
        case SV_HORN_TERROR:
            smithing_cost.gra += 1;
            break;
        case SV_HORN_THUNDER:
            smithing_cost.dex += 1;
            break;
        case SV_HORN_FORCE:
            smithing_cost.str += 1;
            break;
        case SV_HORN_BLASTING:
            smithing_cost.con += 1;
            break;
            // SV_HORN_WARNING
        }
    }

    // different rules for most other items
    else if (!((o_ptr->tval == TV_RING) || (o_ptr->tval == TV_AMULET)))
    {
        // We need to ignore the flags that are basic
        // to the object type and focus on the special/artefact ones. We can do
        // this by subtracting out the basic flags
        f1 &= ~(k_ptr->flags1);
        f2 &= ~(k_ptr->flags2);
        f3 &= ~(k_ptr->flags3);
        f4 &= ~(k_ptr->flags4);

        // need to add tunneling back in...
        if (k_ptr->flags1 & TR1_TUNNEL)
            f1 |= TR1_TUNNEL;

        // need to add stealth back in...
        if (k_ptr->flags1 & TR1_STL)
            f1 |= TR1_STL;

        // need to add accuracy back in...
        if (k_ptr->flags3 & TR3_ACCURATE)
            f3 |= TR3_ACCURATE;

        // need to add sharpness back in...
        if (k_ptr->flags1 & (TR1_SHARPNESS | TR1_SHARPNESS2))
            f1 |= (k_ptr->flags1 & (TR1_SHARPNESS | TR1_SHARPNESS2));

        // need to add mithril-specific flags back in...
        // These are flags that appear on base mithril items but should
        // count toward difficulty as they are "special" properties
        if (k_ptr->flags1 & TR1_DAMAGE_SIDES)
            f1 |= TR1_DAMAGE_SIDES;
        if (k_ptr->flags2 & TR2_REGEN)
            f2 |= TR2_REGEN;
        if (k_ptr->flags2 & TR2_RES_COLD)
            f2 |= TR2_RES_COLD;
        if (k_ptr->flags2 & TR2_RES_FIRE)
            f2 |= TR2_RES_FIRE;
        if (k_ptr->flags3 & TR3_CHEAT_DEATH)
            f3 |= TR3_CHEAT_DEATH;
        if (k_ptr->flags3 & TR3_STAND_FAST)
            f3 |= TR3_STAND_FAST;
        if (k_ptr->flags3 & TR3_ENCHANTABLE)
            f3 |= TR3_ENCHANTABLE;

        // base item
        dif_inc += k_ptr->level / 2;
    }

    // unusual weight
    if (o_ptr->weight == 0)
        weight_factor = 1100;
    else if (o_ptr->weight > k_ptr->weight)
        weight_factor = 100 * o_ptr->weight / k_ptr->weight;
    else
        weight_factor = 100 * k_ptr->weight / o_ptr->weight;

    dif_inc += (weight_factor - 100) / 20;
    if (f4 & (TR4_WEIGHT | TR4_NEG_WEIGHT))
        dif_inc += 5;

    // Jewelry combat bonuses are paid from zero, regardless of base item mins.
    int smith_base_att = ((o_ptr->tval == TV_RING) || (o_ptr->tval == TV_AMULET))
        ? 0
        : k_ptr->att;
    int smith_base_evn = ((o_ptr->tval == TV_RING) || (o_ptr->tval == TV_AMULET))
        ? 0
        : k_ptr->evn;
    int smith_base_ds = ((o_ptr->tval == TV_RING) || (o_ptr->tval == TV_AMULET))
        ? 0
        : k_ptr->ds;
    int smith_base_prot = ((o_ptr->tval == TV_RING) || (o_ptr->tval == TV_AMULET))
        ? 0
        : ((k_ptr->ps > 0) ? ((k_ptr->ps + 1) * k_ptr->pd) : 0);

    // attack bonus
    x = att_base - smith_base_att;

    // special costs for attack bonus for weapons
    if (o_ptr->tval == TV_ARROW || o_ptr->tval == TV_BOW
        || o_ptr->tval == TV_SWORD || o_ptr->tval == TV_POLEARM
        || o_ptr->tval == TV_HAFTED)
    {
        dif_inc += dif_mod_signed(x, 3);
    }
    // normal costs for other items
    else
    {
        dif_inc += dif_mod_signed(x, 6);
        if (x > 0)
            dif_inc -= 1;
    }

    // evasion bonus
    x = evn_base - smith_base_evn;
    if (o_ptr->tval == TV_SOFT_ARMOR || o_ptr->tval == TV_MAIL
        || o_ptr->tval == TV_SHIELD || o_ptr->tval == TV_HELM
        || o_ptr->tval == TV_CROWN || o_ptr->tval == TV_CLOAK
        || o_ptr->tval == TV_GLOVES || o_ptr->tval == TV_BOOTS)
    {
        dif_inc += dif_mod_signed(x, 6);
        if (x > 0)
            dif_inc -= 1;
    }
    else
    {
        dif_inc += dif_mod_signed(x, 9);
        if (x > 0)
            dif_inc -= 2;
    }

    // damage bonus
    x = (ds_base - smith_base_ds);
    // dd used to be a factor here, but a shortsword is far more breakable than
    // a great axe adjusted to make >1 damage sides expensive to smith
    dif_inc += dif_mod_signed(x, 3 * ABS(x) + 2);

    // protection bonus
    base = smith_base_prot;
    int ps_calc = (ps_base > 0) ? ps_base : 0;
    new = (ps_calc > 0) ? ((ps_calc + 1) * o_ptr->pd) : 0;
    x = new - base;

    // special costs for protection sides on hauberks and amulets
    if ((o_ptr->tval == TV_MAIL) && (o_ptr->sval == SV_LONG_CORSLET))
    {
        dif_inc += dif_mod_signed(x, 1);
        if (x > 0)
            dif_inc += 2;
    }
    else if (o_ptr->tval == TV_AMULET)
    {
        dif_inc += dif_mod_signed(x, 1);
        if (x > 0)
            dif_inc += 4;
    }
    else
    {
        dif_inc += dif_mod_signed(x, 3);
    }

    // weapon modifiers
    if (f1 & TR1_SLAY_ORC)
    {
        dif_inc += 3;
    }
    if (f1 & TR1_SLAY_TROLL)
    {
        dif_inc += 3;
    }
    if (f1 & TR1_SLAY_WOLF)
    {
        dif_inc += 3;
    }
    if (f1 & TR1_SLAY_SPIDER)
    {
        dif_inc += 4;
    }
    if (f1 & TR1_SLAY_UNDEAD)
    {
        dif_inc += 3;
    }
    if (f1 & TR1_SLAY_RAUKO)
    {
        dif_inc += 4;
    }
    if (f1 & TR1_SLAY_DRAGON)
    {
        dif_inc += 4;
    }
    if (f1 & TR1_SLAY_MAN_OR_ELF)
    {
        dif_inc += 5;
    }

    if (f4 & TR4_SLAY_SERPENT)
    {
        dif_inc += 4;
    }
    if (f4 & TR4_SLAY_VAMPIRE)
    {
        dif_inc += 4;
    }
    if (f4 & TR4_SLAY_HORROR)
    {
        dif_inc += 4;
    }
    if (f4 & TR4_SLAY_CAT)
    {
        dif_inc += 3;
    }
    if (f4 & TR4_SLAY_GIANT)
    {
        dif_inc += 3;
    }

    if (f1 & TR1_BRAND_COLD)
    {
        dif_inc += 18;
        smithing_cost.str += 2;
        brands++;
    }
    if (f1 & TR1_BRAND_FIRE)
    {
        dif_inc += 14;
        smithing_cost.str += 2;
        brands++;
    }
    if (f1 & TR1_BRAND_POIS)
    {
        if (o_ptr->tval == TV_ARROW)
        {
            dif_inc += 12;
            smithing_cost.str += 1;
        }
        else
        {
            dif_inc += 16;
            smithing_cost.str += 2;
            brands++;
        }
    }
    if (f1 & TR1_BRAND_ELEC)
    {
        dif_inc += 16;  // No monsters have HURT_ELEC, same as poison
        smithing_cost.str += 2;
        brands++;
    }
    if (brands > 1)
    {
        dif_inc += (brands - 1) * 20;
    }

    if (f1 & TR1_SHARPNESS)
    {
        int base = (o_ptr->tval == TV_ARROW) ? 14 : 24;
        dif_inc += base;
        smithing_cost.str += (o_ptr->tval == TV_ARROW) ? 1 : 2;
    }
    if (f1 & TR1_SHARPNESS2)
    {
        int base = 40;
        dif_inc += base;
        smithing_cost.str += 4;
    }
    if (f1 & TR1_VAMPIRIC)
    {
        dif_inc += 6;
        smithing_cost.str += 1;
    }
    if (f3 & TR3_WILL_DRAIN)
    {
        dif_inc += 8;  // Like VAMPIRIC+2
    }
    if (f3 & TR3_ACCURATE)
    {
        dif_inc += 15;
        smithing_cost.dex += 1;
    }
    if (f4 & TR4_ARMOR_SHATTER)
    {
        dif_inc += 15;  // Like ACCURATE
    }
    if (f4 & TR4_DEPTH_SCALE_PS)
    {
        dif_inc += 5;  // Situational
    }
    if (f4 & TR4_PAIRED)
    {
        dif_inc += 3;  // Paired weapon bonus
    }
    if (f4 & TR4_SUBTLETY_THROW)
    {
        dif_inc += 15;
    }
    if (f4 & TR4_LIGHT_ARMOR)
    {
        dif_inc += 2;  // Light armour tag (e.g. the (Light) ego)
    }

    // pval dependent bonuses
    if (f1 & TR1_TUNNEL)
    {
        x = o_ptr->pval - k_ptr->pval;
        dif_mod(x, 8, &dif_inc);
        smithing_cost.str += (x > 0) ? x : 0;
    }

    /* Per-stat/skill bonuses (no longer necessarily tied to a single pval). */
    if (o_ptr->pval > 0 && (f1 & TR1_DAMAGE_SIDES))
    {
        x = o_ptr->pval;
        dif_mod(x, 18, &dif_inc);
        smithing_cost.str += x;
    }

    if (o_ptr->stat_bonus[A_STR] > 0)
    {
        x = o_ptr->stat_bonus[A_STR];
        dif_mod(x, 14, &dif_inc);
        smithing_cost.str += x;
    }
    if (o_ptr->stat_bonus[A_DEX] > 0)
    {
        x = o_ptr->stat_bonus[A_DEX];
        dif_mod(x, 14, &dif_inc);
        smithing_cost.dex += x;
    }
    if (o_ptr->stat_bonus[A_CON] > 0)
    {
        x = o_ptr->stat_bonus[A_CON];
        dif_mod(x, 14, &dif_inc);
        smithing_cost.con += x;
    }
    if (o_ptr->stat_bonus[A_GRA] > 0)
    {
        x = o_ptr->stat_bonus[A_GRA];
        dif_mod(x, 14, &dif_inc);
        smithing_cost.gra += x;
    }

    if (o_ptr->skill_bonus[S_ARC] > 0)
    {
        x = o_ptr->skill_bonus[S_ARC];
        dif_mod(x, 4, &dif_inc);
    }
    if (o_ptr->skill_bonus[S_STL] > 0)
    {
        x = o_ptr->skill_bonus[S_STL];
        dif_mod(x, 4, &dif_inc);
    }
    if (o_ptr->skill_bonus[S_PER] > 0)
    {
        x = o_ptr->skill_bonus[S_PER];
        dif_mod(x, 3, &dif_inc);
    }
    if (o_ptr->skill_bonus[S_WIL] > 0)
    {
        x = o_ptr->skill_bonus[S_WIL];
        dif_mod(x, 3, &dif_inc);
    }
    if (o_ptr->skill_bonus[S_SMT] > 0)
    {
        x = o_ptr->skill_bonus[S_SMT];
        dif_mod(x, 4, &dif_inc);
    }
    if (o_ptr->skill_bonus[S_SNG] > 0)
    {
        x = o_ptr->skill_bonus[S_SNG];
        dif_mod(x, 4, &dif_inc);
    }

    /*
     * Extra difficulty for multiple distinct stat/skill bonuses.
     * First bonus is "free" (already covered by the per-bonus scaling above).
     */
    {
        int stat_count = 0;
        int skill_count = 0;

        if (o_ptr->stat_bonus[A_STR] > 0)
            stat_count++;
        if (o_ptr->stat_bonus[A_DEX] > 0)
            stat_count++;
        if (o_ptr->stat_bonus[A_CON] > 0)
            stat_count++;
        if (o_ptr->stat_bonus[A_GRA] > 0)
            stat_count++;

        if (o_ptr->skill_bonus[S_ARC] > 0)
            skill_count++;
        if (o_ptr->skill_bonus[S_STL] > 0)
            skill_count++;
        if (o_ptr->skill_bonus[S_PER] > 0)
            skill_count++;
        if (o_ptr->skill_bonus[S_WIL] > 0)
            skill_count++;
        if (o_ptr->skill_bonus[S_SMT] > 0)
            skill_count++;
        if (o_ptr->skill_bonus[S_SNG] > 0)
            skill_count++;

        if (stat_count > 1)
            dif_inc += (stat_count - 1) * 7;
        if (skill_count > 1)
            dif_inc += (skill_count - 1) * 3;
    }

    // Sustains
    if (f2 & TR2_SUST_STR)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_SUST_DEX)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_SUST_CON)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_SUST_GRA)
    {
        dif_inc += 2;
    }

    // Abilities
    if (f2 & TR2_SLOW_DIGEST)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_RADIANCE)
    {
        dif_inc += 6;
        smithing_cost.gra += 1;
    }
    if (f2 & TR2_LIGHT)
    {
        dif_inc += 8;
        smithing_cost.gra += 1;
    }
    if (f2 & TR2_REGEN)
    {
        dif_inc += 4;
    }
    if (f2 & TR2_SEE_INVIS)
    {
        dif_inc += 4;
    }
    if (f2 & TR2_FREE_ACT)
    {
        dif_inc += 7;
    }
    if (f2 & TR2_SPEED)
    {
        dif_inc += 40;
        smithing_cost.con += 5;
    }
    if (f3 & TR3_CHEAT_DEATH)
    {
        dif_inc += 13;
        smithing_cost.con += 1;
    }
    if (f3 & TR3_STAND_FAST)
    {
        dif_inc += 2;
    }
    if (f3 & TR3_AVOID_TRAPS)
    {
        dif_inc += 6;
    }
    if (f3 & TR3_MEDIC)
    {
        dif_inc += 4;
    }
    if (f3 & TR3_OATH_BOOST)
    {
        dif_inc += 5;
    }
    if (f3 & TR3_OATH_NEGATE)
    {
        dif_dec += 5;
    }

    // Elemental Resistances
    if (f2 & TR2_RES_COLD)
    {
        dif_inc += 5;
    }
    if (f2 & TR2_RES_FIRE)
    {
        dif_inc += 5;
    }
    if (f2 & TR2_RES_POIS)
    {
        dif_inc += 5;
    }
    if (f2 & TR2_RES_ELEC)
    {
        dif_inc += 5;
    }

    // Other Resistances
    if (f2 & TR2_RES_BLEED)
    {
        dif_inc += 1;
    }
    if (f2 & TR2_RES_BLIND)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_RES_CONFU)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_RES_STUN)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_RES_FEAR)
    {
        dif_inc += 2;
    }
    if (f2 & TR2_RES_HALLU)
    {
        dif_inc += 1;
    }

    // Penalty Flags
    if (!o_ptr->name1)
    {
        if (f2 & TR2_DANGER)
        {
            dif_dec += 5;
        } // only Danger counts
        if (f2 & TR2_DARKNESS)
        {
            dif_dec += 2;  // Changed from 3
        }
        if (f2 & TR2_AGGRAVATE)
        {
            dif_dec += 3;
        }
        if (f2 & TR2_HAUNTED)
        {
            dif_dec += 5;
        }
        if (f2 & TR2_VUL_COLD)
        {
            dif_dec += 4;
        }
        if (f2 & TR2_VUL_FIRE)
        {
            dif_dec += 4;
        }
        if (f2 & TR2_VUL_POIS)
        {
            dif_dec += 4;
        }
        if (f2 & TR2_TRAITOR)
        {
            dif_dec += 2;
        }
        if (f3 & TR3_LIGHT_CURSE)
        {
            dif_dec += 3;
        }
        if (f3 & TR3_CUMBERSOME)
        {
            dif_dec += 3;
        }
        if (f4 & TR4_UNLIGHT)
        {
            dif_dec += 5;  // Worse than DARKNESS - pure negative, no light bonus
        }
        if (f2 & TR2_SLOWNESS)
        {
            dif_dec += 15;
        }
        if (f2 & TR2_HUNGER)
        {
            dif_dec += 3;
        }
        if (f2 & TR2_FEAR)  // Not RES_FEAR!
        {
            dif_dec += 5;
        }
        if (f3 & TR3_HEAVY_CURSE)
        {
            dif_dec += 4;
        }
        if (f3 & TR3_PERMA_CURSE)
        {
            dif_dec += 8;
        }
    }

    // Abilities
    for (i = 0; i < o_ptr->abilities; i++)
    {
        int level = (&b_info[ability_index(
                         o_ptr->skilltype[i], o_ptr->abilitynum[i])])
                        ->level;

        dif_inc += 5 + (level / 3);
        smithing_cost.exp += 50 * level;
    }

    // Penalty for being an artefact
    if (o_ptr->name1)
    {
        if (!(c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_FEANOR)) smithing_cost.uses +=2;
        // else smithing_cost.uses += 2;
    }

    // Set the overall difficulty
    dif = dif_inc - dif_dec;

    // Increased difficulties for minor slots
    switch (wield_slot(o_ptr))
    {
    // case INVEN_WIELD:
    case INVEN_LEFT:
    case INVEN_RIGHT:
    {
        // Celebrimbor: rings are not minor slots (no penalty)
        if (!(c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_CELEBRIMBOR))
        {
            dif_mult += 20;
        }
        break;
    }
    // case INVEN_NECK:
    case INVEN_LITE:
    // case INVEN_BODY:
    case INVEN_OUTER:
    // case INVEN_ARM:
    // case INVEN_HEAD:
    case INVEN_HANDS:
    case INVEN_FEET:
    case INVEN_QUIVER1:
    case INVEN_QUIVER2:
    case INVEN_HORN:
    {
        dif_mult += 20;
        break;
    }
    }

    // Decreased difficulties for easily enchatable items
    if (k_ptr->flags3 & (TR3_ENCHANTABLE))
    {
        dif_mult -= 30;
    }

    // Celebrimbor: treat rings as enchantable
    if ((c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_CELEBRIMBOR)
        && (o_ptr->tval == TV_RING))
    {
        dif_mult -= 30;
    }

    // Mithril
    if (k_ptr->flags3 & TR3_MITHRIL)
    {
        smithing_cost.mithril += o_ptr->weight;
    }
    // Star iron
    if (k_ptr->flags3 & TR3_STAR_IRON)
    {
        smithing_cost.star_iron += o_ptr->weight;
    }

    /* Optional alloy bonus */
    if (smith_alloy.type != SMITH_ALLOY_NONE)
    {
        int alloy_weight = smith_alloy_weight_required(o_ptr);
        smithing_cost.alloy_weight = alloy_weight;
        smithing_cost.alloy_metal = smith_alloy.type;

        if (smith_alloy.type == SMITH_ALLOY_MITHRIL)
            smithing_cost.mithril += alloy_weight;
        else if (smith_alloy.type == SMITH_ALLOY_STAR_IRON)
            smithing_cost.star_iron += alloy_weight;
    }

   /* Gamil character bonus  override normal mithril cost */
  if ((c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_GAMIL)      /* youre Gamil */
      && (k_ptr->flags3 & TR3_MITHRIL)                     /* item is mithril */
      && (mithril_carried() < smithing_cost.mithril))      /* no mithril on hand */
  {
      smithing_cost.uses    = MAX(smithing_cost.uses, 3);  /* cost 3 forge uses */
      smithing_cost.mithril = 0;                           /* waive material */
      o_ptr->ident         |= IDENT_CANT_MELT;             /* cant melt later */
  }

    // Apply the difficulty multiplier
    dif = dif * dif_mult / 100;

    // Artefact arrows are much easier
    if ((o_ptr->tval == TV_ARROW) && (o_ptr->name1))
        dif /= 2;

    // Deal with masterpiece and Aulë's Forge
    int effective_skill = p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px);

    if (p_ptr->have_ability[S_SPC][SPC_AULE]) {
        // Aulë's Forge: supersedes Masterpiece, allows burning base skill for 2x difficulty allowance
        int max_aule_difficulty = effective_skill + (p_ptr->skill_base[S_SMT] * 2);
        if (dif > effective_skill) {
            if (dif <= max_aule_difficulty) {
                // Can craft this with Aulë's Forge - drain base skill efficiently
                int excess = dif - effective_skill;
                smithing_cost.drain += (excess + 1) / 2; // drain 1 skill for every 2 excess points
                log_trace("ABILITY DEBUG: Aulë's Forge drain - base_skill: %d, skill_use: %d, effective: %d, max_aule: %d, difficulty: %d, excess: %d, drain: %d",
                         p_ptr->skill_base[S_SMT], p_ptr->skill_use[S_SMT], effective_skill, max_aule_difficulty, dif, excess, (excess + 1) / 2);
            } else {
                // Too difficult even with Aulë's Forge
                smithing_cost.drain += p_ptr->skill_base[S_SMT] + (dif - max_aule_difficulty);
                log_trace("ABILITY DEBUG: Aulë's Forge insufficient - max possible: %d, difficulty: %d", max_aule_difficulty, dif);
            }
        } else {
            log_trace("ABILITY DEBUG: Aulë's Forge active - no drain needed (difficulty %d <= effective skill %d)", dif, effective_skill);
        }
    } else if (p_ptr->active_ability[S_SMT][SMT_MASTERPIECE]) {
        // Regular Masterpiece ability - allows burning base skill for 1x difficulty allowance
        int max_masterpiece_difficulty = effective_skill + p_ptr->skill_base[S_SMT];
        if (dif > effective_skill) {
            if (dif <= max_masterpiece_difficulty) {
                // Can craft this with Masterpiece - drain base skill normally
                smithing_cost.drain += dif - effective_skill;
            } else {
                // Too difficult even with Masterpiece
                smithing_cost.drain += p_ptr->skill_base[S_SMT] + (dif - max_masterpiece_difficulty);
            }
        }
    }

    bool needs_alloy_mastery = ((k_ptr->flags3 & (TR3_MITHRIL | TR3_STAR_IRON)) != 0)
        || (smith_alloy.type != SMITH_ALLOY_NONE);

    // determine which additional smithing abilities would be required
    cat = smith_item_category(smith_o_ptr);
    if ((cat == CAT_WEAPON) && !p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH])
    {
        smithing_cost.weaponsmith = 1;
    }
    if ((cat == CAT_ARMOUR) && !p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH])
    {
        smithing_cost.armoursmith = 1;
    }
    if ((cat == CAT_JEWELRY) && !p_ptr->active_ability[S_SMT][SMT_JEWELLER])
    {
        smithing_cost.jeweller = 1;
    }
    if (smith_o_ptr->name1 && !p_ptr->active_ability[S_SMT][SMT_ARTEFACT])
    {
        smithing_cost.artifice = 1;
    }
    if (object_has_ego(smith_o_ptr) && !p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT])
    {
        smithing_cost.enchantment = 1;
    }
    if (needs_alloy_mastery && !p_ptr->active_ability[S_SMT][SMT_ALLOY_MASTERY])
    {
        smithing_cost.alloy_mastery = 1;
    }
    if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
    {
        smithing_cost.str = 0;
        smithing_cost.dex = 0;
        smithing_cost.con = 0;
        smithing_cost.gra = 0;
        smithing_cost.exp = 0;
    }

    return (dif);
}

/*
 * Clears the object's name and description at the bottom of the screen.
 */
void wipe_object_description(void)
{
    if (smith_ui_last_desc_row >= 0)
        smith_ui_clear_from_row(smith_ui_last_desc_row);

    smith_ui_reset_description_state();
}

/*
 * Displays the object's name and description at the bottom of the screen.
 */
void prt_object_description(void)
{
    char o_desc[80];
    char buf[80];
    char base_desc_buf[2048];
    cptr base_desc;
    int display_flag;
    int desc_row;
    int desc_col;
    int desc_width;
    int wrap_col;
    int min_desc_row;
    int max_lines;
    int progress_lines;
    int header_lines = 1;
    int info_lines;
    int lore_lines;
    int lore_info_lines;
    int body_lines;
    int total_lines;
    bool show_lore = false;
    bool show_info = false;
    void (*old_hook)(byte, cptr);
    void (*old_info_out_flags)(const object_type*, u32b*, u32b*, u32b*);
    int old_wrap;
    int old_indent;

    wipe_object_description();

    // abort if there is no object to display
    if (smith_o_ptr->tval == 0)
        return;

    desc_col = smith_ui_desc_col();
    desc_width = smith_ui_line_width(desc_col);
    wrap_col = smith_ui_term_wid() - 2;
    if (desc_width <= 0)
        return;
    if (wrap_col <= desc_col + 1)
        wrap_col = desc_col + 2;
    min_desc_row = smith_ui_min_description_row();
    max_lines = smith_ui_content_bottom_row() - min_desc_row + 1;

    if (smith_o_ptr->number > 1)
        display_flag = true;
    else
        display_flag = false;

    object_desc(o_desc, sizeof(o_desc), smith_o_ptr, display_flag, 2);

    SDL_strlcat(o_desc,
        format("   %d.%d lb", smith_o_ptr->weight * smith_o_ptr->number / 10,
            (smith_o_ptr->weight * smith_o_ptr->number) % 10),
        sizeof(o_desc));

    base_desc = object_lore_select_base_text(smith_o_ptr, base_desc_buf,
        sizeof(base_desc_buf));

    progress_lines = p_ptr->smithing_leftover ? 1 : 0;
    info_lines = smith_count_object_preview_lines(
        smith_o_ptr, NULL, true, desc_col, wrap_col);
    lore_lines = smith_count_object_preview_lines(
        smith_o_ptr, base_desc, false, desc_col, wrap_col);
    lore_info_lines = smith_count_object_preview_lines(
        smith_o_ptr, base_desc, true, desc_col, wrap_col);

    show_info = (info_lines > 0);
    if (progress_lines + header_lines + info_lines > max_lines)
        show_info = false;
    if (base_desc && base_desc[0])
    {
        int full_body_lines = show_info ? lore_info_lines : lore_lines;

        if ((full_body_lines > 0)
            && (progress_lines + header_lines + full_body_lines <= max_lines))
        {
            show_lore = true;
        }
    }

    body_lines = show_lore ? (show_info ? lore_info_lines : lore_lines)
                           : (show_info ? info_lines : 0);
    total_lines = progress_lines + header_lines + body_lines;

    desc_row = smith_ui_description_row_for_lines(total_lines);
    if (desc_row < 0)
        return;

    smith_ui_last_desc_row = desc_row;
    smith_ui_clear_from_row(desc_row);

    if (p_ptr->smithing_leftover)
    {
        strnfmt(buf, sizeof(buf), "In progress: %d turns left",
            p_ptr->smithing_leftover);
        smith_ui_put_fitted(desc_col, desc_row, desc_width, TERM_L_BLUE, buf);
        desc_row++;
        if (desc_row > smith_ui_content_bottom_row())
            return;
    }

    smith_ui_put_fitted(desc_col, desc_row, desc_width, TERM_L_WHITE, o_desc);
    desc_row++;
    if (desc_row > smith_ui_content_bottom_row())
        return;

    Term_gotoxy(desc_col, desc_row);

    /* Set hooks for character dump */
    old_info_out_flags = object_info_out_flags;
    object_info_out_flags = object_flags;

    /* Set the indent/wrap */
    old_hook = text_out_hook;
    old_wrap = text_out_wrap;
    old_indent = text_out_indent;
    text_out_indent = desc_col;
    text_out_wrap = wrap_col;

    text_out_hook = text_out_to_screen;

    if (show_lore && base_desc && base_desc[0])
    {
        text_out_c(TERM_WHITE, base_desc);
        if (show_info)
            text_out(" ");
    }

    if (show_info)
        (void)object_info_out(smith_o_ptr);

    /* Reset indent/wrap */
    text_out_hook = old_hook;
    object_info_out_flags = old_info_out_flags;
    text_out_indent = old_indent;
    text_out_wrap = old_wrap;
}

/*
 * Determines whether an item is too difficult to make.
 */
int too_difficult(object_type* o_ptr)
{
    int ability = p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px);
    int dif = object_difficulty(o_ptr);

    if (p_ptr->have_ability[S_SPC][SPC_AULE]) {
        // Aulë's Forge: can craft up to skill_use + (skill_base * 2)
        int max_aule_difficulty = ability + (p_ptr->skill_base[S_SMT] * 2);
        log_trace("ABILITY DEBUG: Aulë's Forge too_difficult check - max possible: %d, difficulty: %d", max_aule_difficulty, dif);
        if (max_aule_difficulty >= dif)
            return (false);
        else
            return (true);
    } else if (p_ptr->active_ability[S_SMT][SMT_MASTERPIECE]) {
        // Masterpiece: can craft up to skill_use + skill_base
        ability += p_ptr->skill_base[S_SMT];
    }

    if (ability < dif)
        return (true);
    else
        return (false);
}

/*
 * Displays the object's difficulty and costs in the right hand side of the
 * screen.
 */
void prt_object_difficulty(void)
{
    int dif;
    char buf[80];
    int turn_multiplier = 10;
    int costs = 0;
    byte attr;
    bool affordable = true;
    bool compact = smith_ui_compact_width();
    bool portrait = smith_ui_portrait_layout();
    int cost_title_row;
    int measure_row = -1;

    // abort if there is no object to display
    if (smith_o_ptr->tval == 0)
        return;

    if (portrait)
    {
        int divider_row = smith_ui_used_bottom_row() + 1;

        smith_ui_draw_horizontal_divider(divider_row);
        measure_row = divider_row + 1;
    }

    // display difficulty information
    if (too_difficult(smith_o_ptr))
        attr = TERM_L_DARK;
    else
        attr = TERM_SLATE;

    if (!portrait)
    {
        smith_ui_put_fitted(COL_SMT4, 2, smith_ui_line_width(COL_SMT4), attr,
            "Difficulty:");
    }

    // change colour if smithing drain is required
    if ((smithing_cost.drain > 0)
        && (smithing_cost.drain <= p_ptr->skill_base[S_SMT]))
    {
        attr = TERM_BLUE;
    }

    // calculate difficulty (and costs)
    dif = object_difficulty(smith_o_ptr);

    if (portrait)
    {
        int used;

        strnfmt(buf, sizeof(buf), "Measure   Difficulty: %d / %d", dif,
            p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px));
        used = smith_ui_put_wrapped(COL_SMT1, measure_row,
            smith_ui_line_width(COL_SMT1),
            MAX(1, smith_ui_content_bottom_row() - measure_row + 1), attr,
            buf);
        smith_ui_cost_title_row_override = measure_row + MAX(1, used);
    }
    else
    {
        sprintf(buf, "%d", dif);
        smith_ui_put_fitted(COL_SMT4 + 2, 4, 4, attr, buf);

        if (compact)
            strnfmt(buf, sizeof(buf), "/%d",
                p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px));
        else
            strnfmt(buf, sizeof(buf), "(max %d)",
                p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px));
        smith_ui_put_fitted(COL_SMT4 + (compact ? 4 : 5), 4,
            smith_ui_line_width(COL_SMT4 + (compact ? 4 : 5)), TERM_L_DARK,
            buf);
    }

    cost_title_row = smith_ui_cost_title_row();

    // display cost information
    if (smithing_cost.weaponsmith)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Weaponsmith");
        costs++;
    }
    if (smithing_cost.armoursmith)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Armoursmith");
        costs++;
    }
    if (smithing_cost.jeweller)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Jeweller");
        costs++;
    }
    if (smithing_cost.enchantment)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Enchantment");
        costs++;
    }
    if (smithing_cost.artifice)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Artifice");
        costs++;
    }
    if (smithing_cost.alloy_mastery)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Alloy Mastery");
        costs++;
    }
    if (smithing_cost.uses > 0)
    {
        if (forge_uses(p_ptr->py, p_ptr->px) >= smithing_cost.uses)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        if (smithing_cost.uses == 1)
        {
            sprintf(buf, "%d Use", smithing_cost.uses);
        }
        else
        {
            sprintf(buf, "%d Uses", smithing_cost.uses);
        }
        if (compact || portrait)
        {
            strnfmt(buf, sizeof(buf), "%d/%d uses", smithing_cost.uses,
                forge_uses(p_ptr->py, p_ptr->px));
            smith_ui_put_cost_line(costs, attr, buf);
        }
        else
        {
            smith_ui_put_cost_line(costs, attr, buf);
            strnfmt(buf, sizeof(buf), "(of %d)", forge_uses(p_ptr->py, p_ptr->px));
            smith_ui_put_fitted(COL_SMT4 + 9, smith_ui_cost_item_row(costs),
                smith_ui_line_width(COL_SMT4 + 9), TERM_L_DARK, buf);
        }
        costs++;
    }
    if (smithing_cost.drain > 0)
    {
        if (smithing_cost.drain <= p_ptr->skill_base[S_SMT])
        {
            attr = TERM_BLUE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, "%d Smithing", smithing_cost.drain);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.mithril > 0)
    {
        if (smithing_cost.mithril <= mithril_carried())
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, compact ? "%d.%d lb Mith" : "%d.%d lb Mithril",
            smithing_cost.mithril / 10,
            smithing_cost.mithril % 10);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.star_iron > 0)
    {
        if (smithing_cost.star_iron <= star_iron_carried())
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, compact ? "%d.%d lb StIron" : "%d.%d lb Star Iron",
            smithing_cost.star_iron / 10,
            smithing_cost.star_iron % 10);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.str > 0)
    {
        if (p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR]
                - smithing_cost.str
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, "%d Str", smithing_cost.str);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.dex > 0)
    {
        if (p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX]
                - smithing_cost.dex
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, "%d Dex", smithing_cost.dex);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.con > 0)
    {
        if (p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON]
                - smithing_cost.con
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, "%d Con", smithing_cost.con);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.gra > 0)
    {
        if (p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA]
                - smithing_cost.gra
            >= -5)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, "%d Gra", smithing_cost.gra);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (smithing_cost.exp > 0)
    {
        if (p_ptr->new_exp >= smithing_cost.exp)
        {
            attr = TERM_SLATE;
        }
        else
        {
            attr = TERM_L_DARK;
            affordable = false;
        }
        sprintf(buf, "%d Exp", smithing_cost.exp);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
    {
        turn_multiplier /= 2;
    }

    attr = TERM_SLATE;
    sprintf(buf, "%d Turns", MAX(10, dif * turn_multiplier));
    smith_ui_put_cost_line(costs, attr, buf);
    costs++;

    // if (costs == 0)
    //{
    //	Term_putstr(COL_SMT4 + 2, 10 + costs, -1, TERM_SLATE, "-");
    //}

    // display cost title
    if (affordable)
        attr = TERM_SLATE;
    else
        attr = TERM_L_DARK;
    smith_ui_put_fitted(portrait ? COL_SMT1 : COL_SMT4, cost_title_row,
        smith_ui_line_width(portrait ? COL_SMT1 : COL_SMT4), attr, "Cost:");
}

/*
 * Checks whether you can pay the costs in terms of ability points and
 * experience needed to make the object.
 */
bool affordable(object_type* o_ptr)
{
    bool can_afford = true;

    // can't afford non-existant items
    if (o_ptr->tval == 0)
        return (false);
    if (object_has_evil_alignment(o_ptr))
        return (false);

    if (too_difficult(o_ptr))
        can_afford = false;
    if ((smithing_cost.str > 0)
        && (p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR]
                - smithing_cost.str
            < -5))
        can_afford = false;
    if ((smithing_cost.dex > 0)
        && (p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX]
                - smithing_cost.dex
            < -5))
        can_afford = false;
    if ((smithing_cost.con > 0)
        && (p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON]
                - smithing_cost.con
            < -5))
        can_afford = false;
    if ((smithing_cost.gra > 0)
        && (p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA]
                - smithing_cost.gra
            < -5))
        can_afford = false;
    if (smithing_cost.exp > p_ptr->new_exp)
        can_afford = false;
    if ((smithing_cost.mithril > 0)
        && (smithing_cost.mithril > mithril_carried()))
        can_afford = false;
    if ((smithing_cost.star_iron > 0)
        && (smithing_cost.star_iron > star_iron_carried()))
        can_afford = false;
    if (forge_uses(p_ptr->py, p_ptr->px) < smithing_cost.uses)
        can_afford = false;

    if (smithing_cost.weaponsmith || smithing_cost.armoursmith
        || smithing_cost.jeweller || smithing_cost.enchantment
        || smithing_cost.artifice || smithing_cost.alloy_mastery)
        can_afford = false;

    return (can_afford);
}

/*
 * Pay the costs in terms of ability points and experience needed to make the
 * object.
 */
void pay_costs()
{
    if (smithing_cost.str > 0)
        p_ptr->stat_drain[A_STR] -= smithing_cost.str;
    if (smithing_cost.dex > 0)
        p_ptr->stat_drain[A_DEX] -= smithing_cost.dex;
    if (smithing_cost.con > 0)
        p_ptr->stat_drain[A_CON] -= smithing_cost.con;
    if (smithing_cost.gra > 0)
        p_ptr->stat_drain[A_GRA] -= smithing_cost.gra;

    if (smithing_cost.exp > 0)
        p_ptr->new_exp -= smithing_cost.exp;
    if (smithing_cost.mithril > 0)
        use_mithril(smithing_cost.mithril);
    if (smithing_cost.star_iron > 0)
        use_star_iron(smithing_cost.star_iron);
    if (smithing_cost.uses > 0)
        cave_feat[p_ptr->py][p_ptr->px] -= smithing_cost.uses;
    if (smithing_cost.drain > 0)
        p_ptr->skill_base[S_SMT] -= smithing_cost.drain;

    /* Calculate the bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Set the redraw flag for everything */
    p_ptr->redraw |= (PR_EXP | PR_BASIC);
}

typedef struct reforge_preview_type
{
    int scaled_difficulty;
    int raw_delta_difficulty;
    int turns;
    smithing_cost_type cost;
    bool affordable;
    bool needs_forge;
    bool needs_forge_resources;
    bool needs_reforging;
} reforge_preview_type;

static void smithing_cost_reset_local(smithing_cost_type* cost)
{
    if (!cost)
        return;

    memset(cost, 0, sizeof(*cost));
}

static bool smith_has_category_ability(const object_type* o_ptr)
{
    int cat;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    cat = smith_item_category(o_ptr);
    if ((cat == CAT_WEAPON) && !p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH])
        return false;
    if ((cat == CAT_ARMOUR) && !p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH])
        return false;
    if ((cat == CAT_JEWELRY) && !p_ptr->active_ability[S_SMT][SMT_JEWELLER])
        return false;

    return true;
}

static bool object_has_evil_alignment(const object_type* o_ptr)
{
    u32b f1, f2, f3, f4;

    if (!o_ptr || !o_ptr->k_idx)
        return false;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    (void)f1;
    (void)f2;
    (void)f3;
    return (f4 & TR4_EVIL_ITEM) != 0;
}

static bool smith_has_alignment_conflict(const object_type* o_ptr,
    int prefix_idx, int suffix_idx)
{
    u32b f1, f2, f3, f4;
    bool has_noble;
    bool has_evil;

    if (!o_ptr)
        return false;

    object_flags4(o_ptr, &f1, &f2, &f3, &f4);
    (void)f1;
    (void)f2;
    (void)f3;

    has_noble = ((f4 & TR4_NOBLE_ITEM) != 0);
    has_evil = ((f4 & TR4_EVIL_ITEM) != 0);

    if (prefix_idx > 0)
    {
        if (e_info[prefix_idx].flags4 & TR4_NOBLE_ITEM)
            has_noble = true;
        if (e_info[prefix_idx].flags4 & TR4_EVIL_ITEM)
            has_evil = true;
    }

    if (suffix_idx > 0)
    {
        if (e_info[suffix_idx].flags4 & TR4_NOBLE_ITEM)
            has_noble = true;
        if (e_info[suffix_idx].flags4 & TR4_EVIL_ITEM)
            has_evil = true;
    }

    return has_noble && has_evil;
}

static bool ego_forbids_prefix_combo(int e_idx)
{
    if (e_idx <= 0 || e_idx >= z_info->e_max)
        return false;

    return (e_info[e_idx].flags4 & TR4_NO_PREFIX) != 0;
}

static bool smith_ego_is_forbidden_affix(const ego_item_type* e_ptr)
{
    if (!e_ptr)
        return true;
    if (e_ptr->flags3 & (TR3_DAMAGED | TR3_NO_SMITHING))
        return true;
    if (e_ptr->flags4 & (TR4_JINX | TR4_EVIL_ITEM))
        return true;
    return false;
}

static bool smith_ego_matches_item_type(const object_type* o_ptr,
    const ego_item_type* e_ptr)
{
    int j;

    if (!o_ptr || !o_ptr->k_idx || !e_ptr)
        return false;

    for (j = 0; j < EGO_TVALS_MAX; j++)
    {
        if (o_ptr->tval != e_ptr->tval[j])
            continue;
        if (o_ptr->sval < e_ptr->min_sval[j])
            continue;
        if (o_ptr->sval > e_ptr->max_sval[j])
            continue;

        return true;
    }

    return false;
}

static bool smith_ego_can_apply_to_object(const object_type* o_ptr, int e_idx,
    int fixed_prefix, int fixed_suffix, bool selecting_prefix)
{
    ego_item_type* e_ptr;
    const char* raw_name;
    bool is_prefix;

    if (!o_ptr || !o_ptr->k_idx || e_idx <= 0 || e_idx >= z_info->e_max)
        return false;

    e_ptr = &e_info[e_idx];
    raw_name = e_name + e_ptr->name;
    is_prefix = ego_name_is_prefix(raw_name);

    if (selecting_prefix != is_prefix)
        return false;
    if (smith_ego_is_forbidden_affix(e_ptr))
        return false;
    if (!smith_ego_matches_item_type(o_ptr, e_ptr))
        return false;

    if (selecting_prefix)
    {
        if (ego_forbids_prefix_combo(fixed_suffix))
            return false;
        if (smith_has_alignment_conflict(o_ptr, e_idx, fixed_suffix))
            return false;
    }
    else
    {
        if ((fixed_prefix != 0) && ego_forbids_prefix_combo(e_idx))
            return false;
        if (smith_has_alignment_conflict(o_ptr, fixed_prefix, e_idx))
            return false;
    }

    return true;
}

static bool ego_prefix_can_apply_to_object(const object_type* o_ptr, int e_idx)
{
    return smith_ego_can_apply_to_object(o_ptr, e_idx, 0, 0, true);
}

static bool object_can_reforge_prefix_aux(
    const object_type* o_ptr, bool require_category_ability)
{
    int i;

    if (!o_ptr || !o_ptr->k_idx)
        return false;
    if (o_ptr->name1)
        return false;
    if (object_is_damaged_item(o_ptr))
        return false;
    if (object_has_evil_alignment(o_ptr))
        return false;
    if (is_smithed_by_player(o_ptr))
        return false;
    if (object_ego_prefix(o_ptr))
        return false;
    if (ego_forbids_prefix_combo((int)object_ego_suffix(o_ptr)))
        return false;
    if (require_category_ability && !smith_has_category_ability(o_ptr))
        return false;

    for (i = 1; i < z_info->e_max; i++)
    {
        if (ego_prefix_can_apply_to_object(o_ptr, i))
            return true;
    }

    return false;
}

static bool object_can_preview_reforge_prefix(const object_type* o_ptr)
{
    return object_can_reforge_prefix_aux(o_ptr, false);
}

static int find_reforge_target_item(void)
{
    int i;

    for (i = 0; i < INVEN_TOTAL; i++)
    {
        object_type* o_ptr = &inventory[i];

        if (!o_ptr->k_idx)
            continue;
        if (object_can_repair_damage(o_ptr)
            || object_can_preview_reforge_prefix(o_ptr))
            return i;
    }

    return -1;
}

static void smith_eval_object(const object_type* src, int* difficulty,
    smithing_cost_type* cost_out)
{
    object_type smith_backup;
    smith_alloy_state alloy_backup = smith_alloy;
    smithing_cost_type smithing_cost_backup = smithing_cost;

    if (!src || !src->k_idx)
        return;

    object_copy(&smith_backup, smith_o_ptr);
    object_copy(smith_o_ptr, src);
    smith_clear_alloy_state(&smith_alloy);

    if (difficulty)
        *difficulty = object_difficulty(smith_o_ptr);
    else
        (void)object_difficulty(smith_o_ptr);

    if (cost_out)
        *cost_out = smithing_cost;

    object_copy(smith_o_ptr, &smith_backup);
    smith_alloy = alloy_backup;
    smithing_cost = smithing_cost_backup;
}

static bool smith_reforge_difficulty_affordable(int difficulty, int* drain_out)
{
    int effective_skill = p_ptr->skill_use[S_SMT] + forge_bonus(p_ptr->py, p_ptr->px);

    if (drain_out)
        *drain_out = 0;

    if (p_ptr->have_ability[S_SPC][SPC_AULE])
    {
        int max_aule_difficulty = effective_skill + (p_ptr->skill_base[S_SMT] * 2);

        if (difficulty <= effective_skill)
            return true;
        if (difficulty <= max_aule_difficulty)
        {
            if (drain_out)
                *drain_out = (difficulty - effective_skill + 1) / 2;
            return true;
        }
        if (drain_out)
            *drain_out = p_ptr->skill_base[S_SMT] + (difficulty - max_aule_difficulty);
        return false;
    }

    if (p_ptr->active_ability[S_SMT][SMT_MASTERPIECE])
    {
        int max_masterpiece_difficulty = effective_skill + p_ptr->skill_base[S_SMT];

        if (difficulty <= effective_skill)
            return true;
        if (difficulty <= max_masterpiece_difficulty)
        {
            if (drain_out)
                *drain_out = difficulty - effective_skill;
            return true;
        }
        if (drain_out)
            *drain_out = p_ptr->skill_base[S_SMT] + (difficulty - max_masterpiece_difficulty);
        return false;
    }

    return (difficulty <= effective_skill);
}

static void smithing_cost_delta_positive(const smithing_cost_type* before,
    const smithing_cost_type* after, smithing_cost_type* delta)
{
    smithing_cost_reset_local(delta);

    if (!before || !after || !delta)
        return;

    delta->str = MAX(0, after->str - before->str);
    delta->dex = MAX(0, after->dex - before->dex);
    delta->con = MAX(0, after->con - before->con);
    delta->gra = MAX(0, after->gra - before->gra);
    delta->exp = MAX(0, after->exp - before->exp);
    delta->mithril = MAX(0, after->mithril - before->mithril);
    delta->star_iron = MAX(0, after->star_iron - before->star_iron);
}

static bool reforge_preview_build(const object_type* source, int prefix_idx,
    reforge_preview_type* preview)
{
    int before_diff = 0;
    int after_diff = 0;
    int turn_multiplier = 10;
    smithing_cost_type before_cost;
    smithing_cost_type after_cost;

    if (!source || !source->k_idx || !preview || prefix_idx <= 0)
        return false;

    memset(preview, 0, sizeof(*preview));
    smithing_cost_reset_local(&before_cost);
    smithing_cost_reset_local(&after_cost);

    smith_eval_object(source, &before_diff, &before_cost);

    object_copy(smith_o_ptr, source);
    object_set_ego_prefix(smith_o_ptr, prefix_idx);
    if (!object_apply_ego_affix(smith_o_ptr, prefix_idx, true))
        return false;

    smith_eval_object(smith_o_ptr, &after_diff, &after_cost);

    preview->raw_delta_difficulty = MAX(0, after_diff - before_diff);
    preview->scaled_difficulty = (preview->raw_delta_difficulty * 3 + 1) / 2;
    smithing_cost_delta_positive(&before_cost, &after_cost, &preview->cost);
    preview->cost.uses = 1;

    preview->affordable
        = smith_reforge_difficulty_affordable(
            preview->scaled_difficulty, &preview->cost.drain);

    if (!p_ptr->active_ability[S_SMT][SMT_REPAIR])
    {
        preview->needs_reforging = true;
        preview->affordable = false;
    }

    switch (smith_item_category(source))
    {
    case CAT_WEAPON:
        if (!p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH])
        {
            preview->cost.weaponsmith = 1;
            preview->affordable = false;
        }
        break;
    case CAT_ARMOUR:
        if (!p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH])
        {
            preview->cost.armoursmith = 1;
            preview->affordable = false;
        }
        break;
    case CAT_JEWELRY:
        if (!p_ptr->active_ability[S_SMT][SMT_JEWELLER])
        {
            preview->cost.jeweller = 1;
            preview->affordable = false;
        }
        break;
    }

    if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
    {
        preview->cost.str = 0;
        preview->cost.dex = 0;
        preview->cost.con = 0;
        preview->cost.gra = 0;
        preview->cost.exp = 0;
        turn_multiplier /= 2;
    }

    if ((preview->cost.str > 0)
        && (p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR]
                - preview->cost.str
            < -5))
        preview->affordable = false;
    if ((preview->cost.dex > 0)
        && (p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX]
                - preview->cost.dex
            < -5))
        preview->affordable = false;
    if ((preview->cost.con > 0)
        && (p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON]
                - preview->cost.con
            < -5))
        preview->affordable = false;
    if ((preview->cost.gra > 0)
        && (p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA]
                - preview->cost.gra
            < -5))
        preview->affordable = false;
    if (preview->cost.exp > p_ptr->new_exp)
        preview->affordable = false;
    if ((preview->cost.mithril > 0)
        && (preview->cost.mithril > mithril_carried()))
        preview->affordable = false;
    if ((preview->cost.star_iron > 0)
        && (preview->cost.star_iron > star_iron_carried()))
        preview->affordable = false;
    if (!cave_forge_bold(p_ptr->py, p_ptr->px))
    {
        preview->needs_forge = true;
        preview->affordable = false;
    }
    else if (forge_uses(p_ptr->py, p_ptr->px) < preview->cost.uses)
    {
        preview->needs_forge_resources = true;
        preview->affordable = false;
    }
    if ((preview->cost.drain > 0)
        && (preview->cost.drain > p_ptr->skill_base[S_SMT]))
        preview->affordable = false;

    preview->turns = MAX(10, preview->scaled_difficulty * turn_multiplier);
    return true;
}

static void pay_smithing_cost_struct(const smithing_cost_type* cost)
{
    if (!cost)
        return;

    if (cost->str > 0)
        p_ptr->stat_drain[A_STR] -= cost->str;
    if (cost->dex > 0)
        p_ptr->stat_drain[A_DEX] -= cost->dex;
    if (cost->con > 0)
        p_ptr->stat_drain[A_CON] -= cost->con;
    if (cost->gra > 0)
        p_ptr->stat_drain[A_GRA] -= cost->gra;
    if (cost->exp > 0)
        p_ptr->new_exp -= cost->exp;
    if (cost->mithril > 0)
        use_mithril(cost->mithril);
    if (cost->star_iron > 0)
        use_star_iron(cost->star_iron);
    if (cost->uses > 0)
    {
        cave_feat[p_ptr->py][p_ptr->px] -= cost->uses;
        lite_spot(p_ptr->py, p_ptr->px);
    }
    if (cost->drain > 0)
        p_ptr->skill_base[S_SMT] -= cost->drain;

    p_ptr->update |= (PU_BONUS);
    p_ptr->redraw |= (PR_EXP | PR_BASIC);
}

// Determine default stack sizes for smithing-created items.
// Normal: arrows 24/18/12, daggers & spears 3/2/1 (normal/enchanted/artefact).
// This keeps arrows and throwable weapons in sensible stack counts.
static byte smith_default_stack_size(const object_type* o_ptr)
{
    bool is_arrow = (o_ptr->tval == TV_ARROW);
    bool is_spear = (o_ptr->tval == TV_POLEARM) && (o_ptr->sval == SV_SPEAR);
    bool is_dagger = (o_ptr->tval == TV_SWORD) && (o_ptr->sval == SV_DAGGER);

    if (!(is_arrow || is_spear || is_dagger))
    {
        return (o_ptr->number ? o_ptr->number : 1);
    }

    bool is_artifact = (o_ptr->name1 != 0);
    bool is_enchanted = (!is_artifact) && object_has_ego(o_ptr);

    if (is_arrow)
    {
        if (is_artifact) return 12;
        if (is_enchanted) return 18;
        return 24;
    }

    if (is_artifact) return 1;
    if (is_enchanted) return 2;
    return 3;
}

/*
 * Creates the base object (not in the dungeon, but just as a work in progress).
 */
void create_base_object(int tval, int sval)
{
    /* Wipe the object */
    object_wipe(smith_o_ptr);
    smith_clear_alloy_state(&smith_alloy);

    /* Prepare the item */
    object_prep(smith_o_ptr, lookup_kind(tval, sval));

    // set the pval to 1 if needed (and evasion/accuracy for rings)
    apply_magic_fake(smith_o_ptr);

    // use a default weight
    smith_o_ptr->weight = (&k_info[smith_o_ptr->k_idx])->weight;

    // display all attributes
    smith_o_ptr->ident |= (IDENT_KNOWN | IDENT_SPOIL);

    // Apply default stack sizes for smithing output
    smith_o_ptr->number = smith_default_stack_size(smith_o_ptr);
}

/*
 * Performs the interface and selection work for the sval part of the base item
 * menu.
 */
int create_sval_menu_aux(int tval, int* highlight)
{
    char ch;
    int i, num;
    bool valid[20];
    int sval[20];
    char names[20][80];
    object_type icons[20];
    int list_col = COL_SMT3;
    int title_row = MAX(0, smith_ui_dense_row0() - 1);
    int first_row = smith_ui_dense_row0();
    int last_row = smith_ui_list_bottom_row(first_row, true);
    int top;

    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_outside_cancel_enabled(true);
    smith_ui_begin_touch_scroll_area(false);

    // clear the right of the screen
    smith_ui_wipe_active_panel(indexed_menu_prefix_col(list_col));
    smith_ui_put_section_header(list_col, title_row, "Subtype");

    /* We have to search the whole itemlist. */
    for (num = 0, i = 1; i < z_info->k_max; i++)
    {
        object_kind* k_ptr = &k_info[i];

        /* Analyze matching items */
        if (k_ptr->tval == tval)
        {
            if (num >= (int)N_ELEMENTS(sval))
                break;

            /* Skip instant artefact item types */
            if (k_ptr->flags3 & (TR3_INSTA_ART))
                continue;
            if (k_ptr->flags4 & TR4_EVIL_ITEM)
                continue;

            /* Skip certain item types that cannot be made */
            if (k_ptr->flags3 & (TR3_NO_SMITHING))
            {
                bool allow_override = false;

                /* Check for specific character unique flag and sval overrides */
                if ((c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_EOL) &&
                    (k_ptr->tval == TV_SOFT_ARMOR) && (k_ptr->sval == SV_ARMOUR_OF_GALVORN))
                {
                    allow_override = true;
                }

                if (!allow_override)
                    continue;
            }

            /* Get the "name" of object "i" */
            strip_name(names[num], i);

            // make a simple version of the object
            create_base_object(tval, k_ptr->sval);
            object_copy(&icons[num], smith_o_ptr);

            // Check whether it is a valid choice for creating
            if (affordable(smith_o_ptr))
                valid[num] = true;
            else
                valid[num] = false;

            /* Remember the object sval */
            sval[num] = k_ptr->sval;

            // count the applicable items
            num++;
        }
    }

    if (num <= 0)
        return -1;
    if (*highlight < 1)
        *highlight = 1;
    if (*highlight > num)
        *highlight = num;

    top = smith_ui_configure_list_view(SMITH_SCROLL_SVAL, num, *highlight,
        first_row, last_row);
    for (i = top; i < num && i < top + (last_row - first_row + 1); i++)
    {
        int row = first_row + i - top;

        smith_ui_put_icon_menu_row(i + 1, list_col, row,
            valid[i] ? TERM_WHITE : TERM_SLATE, names[i], &icons[i],
            *highlight == i + 1);
    }

    // make a simple version of the object
    create_base_object(tval, sval[*highlight - 1]);

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(indexed_menu_prefix_col(list_col),
        first_row + MAX(0, MIN(last_row - first_row, *highlight - 1 - top)));

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    {
        int clicked_choice = 0;
        int click_action = UI_MENU_CLICK_PRIMARY;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if ((clicked_choice == SMITH_CLICK_BACK)
                || (click_action == UI_MENU_CLICK_SECONDARY))
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    return 0;
                ch = ESCAPE;
            }
            else if (clicked_choice >= 1 && clicked_choice <= num)
            {
                if (!smith_ui_pointer_choice_confirms(
                        clicked_choice, click_action, highlight))
                {
                    return 0;
                }
                ch = '\r';
            }
        }
    }

    if (sdl_menu_letters_enabled()
        && (ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        *highlight = (int)ch - 'a' + 1;
        ch = '\r';
    }

    /* Materialize the selected row on every confirmation path.  A direct
     * pointer activation can change the highlight after the preview above was
     * built, while keyboard/controller navigation redraws before confirming. */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        create_base_object(tval, sval[*highlight - 1]);
        return (*highlight);
    }

    if ((ch == '4') || (ch == ESCAPE))
    {
        *highlight = -1;
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = num;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else if (*highlight == num)
            *highlight = 1;
    }

    return (0);
}

/*
 * Displays a menu for choosing a base item's sval.
 */
bool create_sval_menu(int tval)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;
    bool completed = false;

    /* Save screen */
    screen_save();

    /* Process Events until "Return to Game" is selected */
    while (!leave_menu)
    {
        choice = create_sval_menu_aux(tval, &highlight);

        if (choice >= 1)
        {
            leave_menu = true;
            completed = true;
        }
        else if (choice == -1)
        {
            /* Wipe the object */
            object_wipe(smith_o_ptr);
            smith_clear_alloy_state(&smith_alloy);

            leave_menu = true;
        }
    }

    /* Load screen */
    ui_menu_click_clear();
    screen_load();

    return (completed);
}

/*
 * Performs the interface and selection work for the tval part of the base item
 * menu.
 */
int create_tval_menu_aux(int* highlight)
{
    char ch;
    int i;
    bool valid[MAX_SMITHING_TVALS];
    bool has_icon[MAX_SMITHING_TVALS];
    byte row_attr[MAX_SMITHING_TVALS];
    object_type icons[MAX_SMITHING_TVALS];
    int title_row = MAX(0, smith_ui_dense_row0() - 1);
    int first_row = smith_ui_dense_row0();
    int last_row = smith_ui_list_bottom_row(first_row, false);
    int top;

    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_outside_cancel_enabled(true);
    smith_ui_begin_touch_scroll_area(false);

    // clear the right of the screen
    smith_ui_wipe_active_panel(indexed_menu_prefix_col(COL_SMT2));
    smith_ui_put_section_header(COL_SMT2, title_row, "Type");

    // clear bottom of the screen
    wipe_object_description();

    /* Wipe the smithing object */
    object_wipe(smith_o_ptr);
    smith_clear_alloy_state(&smith_alloy);

    if (*highlight < 1)
        *highlight = 1;
    if (*highlight > MAX_SMITHING_TVALS)
        *highlight = MAX_SMITHING_TVALS;
    top = smith_ui_configure_list_view(SMITH_SCROLL_TVAL,
        MAX_SMITHING_TVALS, *highlight, first_row, last_row);

    for (i = 0; i < MAX_SMITHING_TVALS; i++)
    {
        byte valid_attr = TERM_WHITE;

        has_icon[i] = smith_tval_icon(smithing_tvals[i].tval, &icons[i]);
        valid[i] = false;

        if (smithing_tvals[i].category == CAT_WEAPON)
        {
            valid[i] = true;
            valid_attr = p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH]
                ? TERM_WHITE
                : TERM_RED;
        }
        if (smithing_tvals[i].category == CAT_ARMOUR)
        {
            valid[i] = true;
            valid_attr = p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH]
                ? TERM_WHITE
                : TERM_RED;
        }
        if (smithing_tvals[i].category == CAT_JEWELRY)
        {
            valid[i] = true;
            valid_attr = p_ptr->active_ability[S_SMT][SMT_JEWELLER] ? TERM_WHITE
                                                                    : TERM_RED;
        }
        row_attr[i] = valid[i] ? valid_attr : TERM_L_DARK;

        if (i >= top && i < top + (last_row - first_row + 1))
        {
            int row = first_row + i - top;

            smith_ui_put_icon_menu_row(i + 1, COL_SMT2, row,
                row_attr[i], smithing_tvals[i].desc,
                has_icon[i] ? &icons[i] : NULL, *highlight == i + 1);
        }
    }

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(indexed_menu_prefix_col(COL_SMT2),
        first_row + MAX(0, MIN(last_row - first_row, *highlight - 1 - top)));

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    {
        int clicked_choice = 0;
        int click_action = UI_MENU_CLICK_PRIMARY;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if ((clicked_choice == SMITH_CLICK_BACK)
                || (click_action == UI_MENU_CLICK_SECONDARY))
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    return 0;
                ch = ESCAPE;
            }
            else if (clicked_choice >= 1 && clicked_choice <= MAX_SMITHING_TVALS)
            {
                if (!smith_ui_pointer_choice_confirms(
                        clicked_choice, click_action, highlight))
                {
                    return 0;
                }
                ch = '\r';
            }
        }
    }

    // choose an option by letter
    if (sdl_menu_letters_enabled()
        && (ch >= 'a') && (ch <= (char)'a' + MAX_SMITHING_TVALS - 1))
    {
        *highlight = (int)ch - 'a' + 1;

        if (valid[*highlight - 1])
            return (*highlight);
        else
            bell("Invalid choice.");
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (valid[*highlight - 1])
        {
            /* The Type panel remains visible behind Subtype.  Repaint it with
             * the activated row so a direct click/tap does not leave the old
             * first-row highlight frozen in the parent screen. */
            for (i = top;
                 i < MAX_SMITHING_TVALS
                     && i < top + (last_row - first_row + 1);
                 i++)
            {
                int row = first_row + i - top;

                smith_ui_put_icon_menu_row(i + 1, COL_SMT2, row,
                    row_attr[i], smithing_tvals[i].desc,
                    has_icon[i] ? &icons[i] : NULL, *highlight == i + 1);
            }
            Term_fresh();
            return (*highlight);
        }
        else
            bell("Invalid choice.");
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = MAX_SMITHING_TVALS;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < MAX_SMITHING_TVALS)
            (*highlight)++;
        else if (*highlight == MAX_SMITHING_TVALS)
            *highlight = 1;
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        return (-1);
    }

    return (0);
}

/*
 * Displays a menu for choosing a base item's tval.
 */
void create_tval_menu(void)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;

    /* Save screen */
    screen_save();

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = create_tval_menu_aux(&highlight);

        if (choice >= 1)
        {
            if (create_sval_menu(smithing_tvals[choice - 1].tval))
            {
                leave_menu = true;
            }
        }
        else if (choice == -1)
        {
            leave_menu = true;
        }
    }

    enchant_then_numbers = false;

    /* Load screen */
    ui_menu_click_clear();
    screen_load();
}

/*
 * Actually modifies the numbers on an item.
 */
static void smith_apply_stat_skill_flag_delta(object_type* o_ptr, u32b f1_before, u32b f1_after)
{
    if (!o_ptr)
        return;

    int pval = o_ptr->pval;
    int pval_abs = ABS(pval);

    bool before_str = (f1_before & (TR1_STR | TR1_NEG_STR)) != 0;
    bool after_str = (f1_after & (TR1_STR | TR1_NEG_STR)) != 0;
    if (!after_str)
    {
        o_ptr->stat_bonus[A_STR] = 0;
    }
    else if (!before_str)
    {
        o_ptr->stat_bonus[A_STR] = (f1_after & TR1_NEG_STR) ? -pval_abs : pval_abs;
    }
    if ((f1_after & TR1_STR) && !(f1_after & TR1_NEG_STR) && o_ptr->stat_bonus[A_STR] < 0)
        o_ptr->stat_bonus[A_STR] = -o_ptr->stat_bonus[A_STR];
    if ((f1_after & TR1_NEG_STR) && !(f1_after & TR1_STR) && o_ptr->stat_bonus[A_STR] > 0)
        o_ptr->stat_bonus[A_STR] = -o_ptr->stat_bonus[A_STR];

    bool before_dex = (f1_before & (TR1_DEX | TR1_NEG_DEX)) != 0;
    bool after_dex = (f1_after & (TR1_DEX | TR1_NEG_DEX)) != 0;
    if (!after_dex)
    {
        o_ptr->stat_bonus[A_DEX] = 0;
    }
    else if (!before_dex)
    {
        o_ptr->stat_bonus[A_DEX] = (f1_after & TR1_NEG_DEX) ? -pval_abs : pval_abs;
    }
    if ((f1_after & TR1_DEX) && !(f1_after & TR1_NEG_DEX) && o_ptr->stat_bonus[A_DEX] < 0)
        o_ptr->stat_bonus[A_DEX] = -o_ptr->stat_bonus[A_DEX];
    if ((f1_after & TR1_NEG_DEX) && !(f1_after & TR1_DEX) && o_ptr->stat_bonus[A_DEX] > 0)
        o_ptr->stat_bonus[A_DEX] = -o_ptr->stat_bonus[A_DEX];

    bool before_con = (f1_before & (TR1_CON | TR1_NEG_CON)) != 0;
    bool after_con = (f1_after & (TR1_CON | TR1_NEG_CON)) != 0;
    if (!after_con)
    {
        o_ptr->stat_bonus[A_CON] = 0;
    }
    else if (!before_con)
    {
        o_ptr->stat_bonus[A_CON] = (f1_after & TR1_NEG_CON) ? -pval_abs : pval_abs;
    }
    if ((f1_after & TR1_CON) && !(f1_after & TR1_NEG_CON) && o_ptr->stat_bonus[A_CON] < 0)
        o_ptr->stat_bonus[A_CON] = -o_ptr->stat_bonus[A_CON];
    if ((f1_after & TR1_NEG_CON) && !(f1_after & TR1_CON) && o_ptr->stat_bonus[A_CON] > 0)
        o_ptr->stat_bonus[A_CON] = -o_ptr->stat_bonus[A_CON];

    bool before_gra = (f1_before & (TR1_GRA | TR1_NEG_GRA)) != 0;
    bool after_gra = (f1_after & (TR1_GRA | TR1_NEG_GRA)) != 0;
    if (!after_gra)
    {
        o_ptr->stat_bonus[A_GRA] = 0;
    }
    else if (!before_gra)
    {
        o_ptr->stat_bonus[A_GRA] = (f1_after & TR1_NEG_GRA) ? -pval_abs : pval_abs;
    }
    if ((f1_after & TR1_GRA) && !(f1_after & TR1_NEG_GRA) && o_ptr->stat_bonus[A_GRA] < 0)
        o_ptr->stat_bonus[A_GRA] = -o_ptr->stat_bonus[A_GRA];
    if ((f1_after & TR1_NEG_GRA) && !(f1_after & TR1_GRA) && o_ptr->stat_bonus[A_GRA] > 0)
        o_ptr->stat_bonus[A_GRA] = -o_ptr->stat_bonus[A_GRA];

    bool before_mel = (f1_before & TR1_MEL) != 0;
    bool after_mel = (f1_after & TR1_MEL) != 0;
    if (!after_mel)
        o_ptr->skill_bonus[S_MEL] = 0;
    else if (!before_mel)
        o_ptr->skill_bonus[S_MEL] = pval;

    bool before_arc = (f1_before & TR1_ARC) != 0;
    bool after_arc = (f1_after & TR1_ARC) != 0;
    if (!after_arc)
        o_ptr->skill_bonus[S_ARC] = 0;
    else if (!before_arc)
        o_ptr->skill_bonus[S_ARC] = pval;

    bool before_stl = (f1_before & TR1_STL) != 0;
    bool after_stl = (f1_after & TR1_STL) != 0;
    if (!after_stl)
        o_ptr->skill_bonus[S_STL] = 0;
    else if (!before_stl)
        o_ptr->skill_bonus[S_STL] = pval;

    bool before_per = (f1_before & TR1_PER) != 0;
    bool after_per = (f1_after & TR1_PER) != 0;
    if (!after_per)
        o_ptr->skill_bonus[S_PER] = 0;
    else if (!before_per)
        o_ptr->skill_bonus[S_PER] = pval;

    bool before_wil = (f1_before & TR1_WIL) != 0;
    bool after_wil = (f1_after & TR1_WIL) != 0;
    if (!after_wil)
        o_ptr->skill_bonus[S_WIL] = 0;
    else if (!before_wil)
        o_ptr->skill_bonus[S_WIL] = pval;

    bool before_smt = (f1_before & TR1_SMT) != 0;
    bool after_smt = (f1_after & TR1_SMT) != 0;
    if (!after_smt)
        o_ptr->skill_bonus[S_SMT] = 0;
    else if (!before_smt)
        o_ptr->skill_bonus[S_SMT] = pval;

    bool before_sng = (f1_before & TR1_SNG) != 0;
    bool after_sng = (f1_after & TR1_SNG) != 0;
    if (!after_sng)
        o_ptr->skill_bonus[S_SNG] = 0;
    else if (!before_sng)
        o_ptr->skill_bonus[S_SNG] = pval;
}

void modify_numbers(int choice)
{
    switch (choice)
    {
    case SMT_NUM_MENU_I_ATT:
    {
        if ((smith_o_ptr->tval == TV_ARROW) && !smith_o_ptr->name1)
            smith_o_ptr->att += 3;
        else
            smith_o_ptr->att++;
        break;
    }
    case SMT_NUM_MENU_D_ATT:
    {
        if ((smith_o_ptr->tval == TV_ARROW) && !smith_o_ptr->name1)
            smith_o_ptr->att -= 3;
        else
            smith_o_ptr->att--;
        break;
    }
    case SMT_NUM_MENU_I_DS:
        smith_o_ptr->ds++;
        break;
    case SMT_NUM_MENU_D_DS:
        smith_o_ptr->ds--;
        break;
    case SMT_NUM_MENU_I_EVN:
        smith_o_ptr->evn++;
        break;
    case SMT_NUM_MENU_D_EVN:
        smith_o_ptr->evn--;
        break;
    case SMT_NUM_MENU_I_PS:
        smithing_increase_protection(smith_o_ptr);
        break;
    case SMT_NUM_MENU_D_PS:
        smithing_decrease_protection(smith_o_ptr);
        break;
    case SMT_NUM_MENU_I_WGT:
        smith_o_ptr->weight += 5;
        break;
    case SMT_NUM_MENU_D_WGT:
        smith_o_ptr->weight -= 5;
        break;
    case SMT_NUM_MENU_ALLOY_CYCLE:
    {
        if (!p_ptr->active_ability[S_SMT][SMT_ALLOY_MASTERY])
        {
            bell("You need Alloy mastery to do that.");
            break;
        }
        if (!smith_alloy_applicable(smith_o_ptr))
        {
            bell("Alloying doesn't apply to this item.");
            smith_remove_alloy_bonus(smith_o_ptr, &smith_alloy);
            break;
        }

        smith_alloy_type next = SMITH_ALLOY_NONE;
        if (smith_alloy.type == SMITH_ALLOY_NONE)
            next = SMITH_ALLOY_MITHRIL;
        else if (smith_alloy.type == SMITH_ALLOY_MITHRIL)
            next = SMITH_ALLOY_STAR_IRON;
        else
            next = SMITH_ALLOY_NONE;

        smith_apply_alloy(smith_o_ptr, &smith_alloy, next);
        break;
    }
    case SMT_NUM_MENU_ALLOY_CLEAR:
        smith_remove_alloy_bonus(smith_o_ptr, &smith_alloy);
        break;
    }

    return;
}

/*
 * Performs the interface and selection work for the numbers menu.
 */
int numbers_menu_aux(int* highlight)
{
    int i;
    char ch;
    char buf[80];
    byte attr[SMT_NUM_MENU_MAX];
    bool valid[SMT_NUM_MENU_MAX];
    bool can_afford[SMT_NUM_MENU_MAX] = { false };
    const int first_row = 2;
    const int last_row = smith_ui_list_bottom_row(first_row, true);
    int top;

    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_outside_cancel_enabled(true);
    smith_ui_begin_touch_scroll_area(false);

    // clear the right of the screen
    smith_ui_wipe_active_panel(indexed_menu_prefix_col(COL_SMT2));
    smith_ui_put_section_header(COL_SMT2, 1, "Adjust");

    memset(valid, 0, sizeof(valid));

    valid[SMT_NUM_MENU_I_ATT - 1]
        = att_valid() && (smith_o_ptr->att < att_max());
    valid[SMT_NUM_MENU_D_ATT - 1]
        = att_valid() && (smith_o_ptr->att > att_min());
    valid[SMT_NUM_MENU_I_DS - 1] = ds_valid() && (smith_o_ptr->ds < ds_max());
    valid[SMT_NUM_MENU_D_DS - 1] = ds_valid() && (smith_o_ptr->ds > ds_min());
    valid[SMT_NUM_MENU_I_EVN - 1]
        = evn_valid() && (smith_o_ptr->evn < evn_max());
    valid[SMT_NUM_MENU_D_EVN - 1]
        = evn_valid() && (smith_o_ptr->evn > evn_min());
    valid[SMT_NUM_MENU_I_PS - 1] = ps_valid() && smithing_can_increase_protection(smith_o_ptr);
    valid[SMT_NUM_MENU_D_PS - 1] = ps_valid() && smithing_can_decrease_protection(smith_o_ptr);
    valid[SMT_NUM_MENU_I_WGT - 1]
        = wgt_valid() && ((smith_o_ptr->weight + 5) <= wgt_max());
    valid[SMT_NUM_MENU_D_WGT - 1]
        = wgt_valid() && ((smith_o_ptr->weight - 5) >= wgt_min());
    {
        u32b f1, f2, f3;
        object_flags(smith_o_ptr, &f1, &f2, &f3);
        valid[SMT_NUM_MENU_EDIT_BONUSES - 1] = (f1 & (TR1_STR | TR1_NEG_STR | TR1_DEX
                                                     | TR1_NEG_DEX | TR1_CON
                                                     | TR1_NEG_CON | TR1_GRA
                                                     | TR1_NEG_GRA | TR1_MEL
                                                     | TR1_ARC | TR1_STL
                                                     | TR1_PER | TR1_WIL
                                                     | TR1_SMT | TR1_SNG
                                                     | TR1_DAMAGE_SIDES
                                                     | TR1_TUNNEL))
            != 0;
    }
    bool alloy_applicable = smith_alloy_applicable(smith_o_ptr);
    bool has_alloy_mastery = p_ptr->active_ability[S_SMT][SMT_ALLOY_MASTERY];
    int alloy_weight = alloy_applicable ? smith_alloy_weight_required(smith_o_ptr) : 0;
    int mithril_have = mithril_carried();
    int star_iron_have = star_iron_carried();
    valid[SMT_NUM_MENU_ALLOY_CYCLE - 1] = alloy_applicable && has_alloy_mastery;
    valid[SMT_NUM_MENU_ALLOY_CLEAR - 1] = (smith_alloy.type != SMITH_ALLOY_NONE);

    // retrieve a super backup of the object
    object_copy(smith3_o_ptr, smith_o_ptr);
    smith3_alloy = smith_alloy;
    for (i = 0; i < SMT_NUM_MENU_MAX; i++)
    {
        if ((i == SMT_NUM_MENU_ALLOY_CYCLE - 1)
            || (i == SMT_NUM_MENU_ALLOY_CLEAR - 1)
            || (i == SMT_NUM_MENU_EDIT_BONUSES - 1))
        {
            can_afford[i] = valid[i];
            if (i == SMT_NUM_MENU_ALLOY_CYCLE - 1 && valid[i])
            {
                bool has_any_metal = (mithril_have >= alloy_weight)
                    || (star_iron_have >= alloy_weight);
                attr[i] = has_any_metal ? TERM_WHITE : TERM_SLATE;
            }
            else
            {
                attr[i] = valid[i] ? TERM_WHITE : TERM_L_DARK;
            }
            continue;
        }
        if (valid[i])
        {
            modify_numbers(i + 1);
            can_afford[i] = affordable(smith_o_ptr);

            // retrieve a super backup of the object
            object_copy(smith_o_ptr, smith3_o_ptr);
            smith_alloy = smith3_alloy;
        }

        attr[i] = valid[i] ? (can_afford[i] ? TERM_WHITE : TERM_SLATE)
                           : TERM_L_DARK;
    }

    if (*highlight < 1)
        *highlight = 1;
    if (*highlight > SMT_NUM_MENU_MAX)
        *highlight = SMT_NUM_MENU_MAX;
    top = smith_ui_configure_list_view(SMITH_SCROLL_NUMBERS,
        SMT_NUM_MENU_MAX, *highlight, first_row, last_row);

    {
        static cptr number_menu_labels[SMT_NUM_MENU_MAX] = {
            "increase attack bonus",
            "decrease attack bonus",
            "increase damage sides",
            "decrease damage sides",
            "increase evasion bonus",
            "decrease evasion bonus",
            "increase protection",
            "decrease protection",
            "increase weight",
            "decrease weight",
            "cycle alloy (none/mithril/star iron)",
            "remove alloy bonus",
            "adjust special bonuses",
        };

        for (i = 0; i < SMT_NUM_MENU_MAX; i++)
        {
            int row;

            if (i < top || i >= top + (last_row - first_row + 1))
                continue;
            row = first_row + i - top;
            indexed_menu_entry_label(buf, sizeof(buf), i, number_menu_labels[i]);
            smith_ui_put_menu_row(i + 1, COL_SMT2, row, attr[i], buf,
                *highlight == i + 1);
        }
    }
    if (alloy_applicable)
    {
        byte info_attr = has_alloy_mastery ? TERM_SLATE : TERM_L_DARK;
        if (!has_alloy_mastery)
        {
            strnfmt(buf, 80, "Alloy needs %d.%d lb metal (requires Alloy mastery)",
                alloy_weight / 10, alloy_weight % 10);
        }
        else
        {
            if (smith_alloy.type == SMITH_ALLOY_MITHRIL)
                info_attr = (mithril_have >= alloy_weight) ? TERM_SLATE : TERM_RED;
            else if (smith_alloy.type == SMITH_ALLOY_STAR_IRON)
                info_attr = (star_iron_have >= alloy_weight) ? TERM_SLATE : TERM_RED;
            strnfmt(buf, 80,
                "Alloy needs %d.%d lb (mithril %d.%d, star iron %d.%d)",
                alloy_weight / 10, alloy_weight % 10, mithril_have / 10,
                mithril_have % 10, star_iron_have / 10, star_iron_have % 10);
        }
        smith_ui_put_wrapped(COL_SMT2, last_row + 1,
            smith_ui_column_width(COL_SMT2), 2, info_attr, buf);
    }
    else if (!has_alloy_mastery)
    {
        smith_ui_put_wrapped(COL_SMT2, last_row + 1,
            smith_ui_column_width(COL_SMT2), 2, TERM_L_DARK,
            "Alloy requires Alloy mastery.");
    }

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(2,
        first_row + MAX(0, MIN(last_row - first_row, *highlight - 1 - top)));

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    {
        int clicked_choice = 0;
        int click_action = UI_MENU_CLICK_PRIMARY;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if ((clicked_choice == SMITH_CLICK_BACK)
                || (click_action == UI_MENU_CLICK_SECONDARY))
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    return 0;
                ch = ESCAPE;
            }
            else if (clicked_choice >= 1 && clicked_choice <= SMT_NUM_MENU_MAX)
            {
                if (!smith_ui_pointer_choice_confirms(
                        clicked_choice, click_action, highlight))
                {
                    return 0;
                }
                ch = '\r';
            }
        }
    }

    // choose an option by letter
    if (sdl_menu_letters_enabled()
        && (ch >= 'a') && (ch <= (char)'a' + SMT_NUM_MENU_MAX - 1))
    {
        int old_highlight = *highlight;

        *highlight = (int)ch - 'a' + 1;

        // move the light blue highlight
        move_displayed_highlight(
            old_highlight, attr[old_highlight - 1], *highlight, COL_SMT2);

        if (valid[*highlight - 1])
            return (*highlight);
        else
            bell("Invalid choice.");
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (valid[*highlight - 1])
            return (*highlight);
        else
            bell("Invalid choice.");
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = SMT_NUM_MENU_MAX;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < SMT_NUM_MENU_MAX)
            (*highlight)++;
        else if (*highlight == SMT_NUM_MENU_MAX)
            *highlight = 1;
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        *highlight = -1;
        return (*highlight);
    }

    return (0);
}

typedef enum
{
    SMT_BONUS_ENTRY_STAT = 0,
    SMT_BONUS_ENTRY_SKILL = 1,
    SMT_BONUS_ENTRY_SPECIAL = 2,
} smith_bonus_entry_kind;

typedef enum
{
    SMT_BONUS_SPECIAL_DAMAGE_SIDES = 0,
    SMT_BONUS_SPECIAL_TUNNEL = 1,
} smith_bonus_special_kind;

typedef struct
{
    smith_bonus_entry_kind kind;
    int index;
    u32b flag_pos;
    u32b flag_neg;
    u32b flag;
} smith_bonus_entry;

typedef struct
{
    smith_bonus_entry entry;
    int delta;
} smith_bonus_action;

static const char* smith_bonus_stat_name(int stat)
{
    switch (stat)
    {
    case A_STR:
        return "Strength";
    case A_DEX:
        return "Dexterity";
    case A_CON:
        return "Constitution";
    case A_GRA:
        return "Grace";
    default:
        return "Unknown";
    }
}

static const char* smith_bonus_special_name(int special)
{
    switch (special)
    {
    case SMT_BONUS_SPECIAL_DAMAGE_SIDES:
        return "Damage bonus";
    case SMT_BONUS_SPECIAL_TUNNEL:
        return "Tunneling";
    default:
        return "Unknown";
    }
}

static int smith_collect_bonus_entries(smith_bonus_entry* entries, int max_entries)
{
    u32b f1, f2, f3;
    int n = 0;

    if (!entries || max_entries <= 0)
        return 0;

    object_flags(smith_o_ptr, &f1, &f2, &f3);

    struct stat_flag_map
    {
        int stat;
        u32b flag_pos;
        u32b flag_neg;
    };

    static const struct stat_flag_map stat_flags[A_MAX] = {
        { A_STR, TR1_STR, TR1_NEG_STR },
        { A_DEX, TR1_DEX, TR1_NEG_DEX },
        { A_CON, TR1_CON, TR1_NEG_CON },
        { A_GRA, TR1_GRA, TR1_NEG_GRA },
    };

    for (int i = 0; i < A_MAX && n < max_entries; i++)
    {
        if ((f1 & (stat_flags[i].flag_pos | stat_flags[i].flag_neg)) == 0)
            continue;

        entries[n].kind = SMT_BONUS_ENTRY_STAT;
        entries[n].index = stat_flags[i].stat;
        entries[n].flag_pos = stat_flags[i].flag_pos;
        entries[n].flag_neg = stat_flags[i].flag_neg;
        entries[n].flag = 0;
        n++;
    }

    struct skill_flag_map
    {
        int skill;
        u32b flag;
    };

    static const struct skill_flag_map skill_flags[] = {
        { S_MEL, TR1_MEL },
        { S_ARC, TR1_ARC },
        { S_STL, TR1_STL },
        { S_PER, TR1_PER },
        { S_WIL, TR1_WIL },
        { S_SMT, TR1_SMT },
        { S_SNG, TR1_SNG },
    };

    for (int i = 0; i < (int)N_ELEMENTS(skill_flags) && n < max_entries; i++)
    {
        if ((f1 & skill_flags[i].flag) == 0)
            continue;

        entries[n].kind = SMT_BONUS_ENTRY_SKILL;
        entries[n].index = skill_flags[i].skill;
        entries[n].flag_pos = 0;
        entries[n].flag_neg = 0;
        entries[n].flag = skill_flags[i].flag;
        n++;
    }

    struct special_flag_map
    {
        int special;
        u32b flag;
    };

    static const struct special_flag_map special_flags[] = {
        { SMT_BONUS_SPECIAL_DAMAGE_SIDES, TR1_DAMAGE_SIDES },
        { SMT_BONUS_SPECIAL_TUNNEL, TR1_TUNNEL },
    };

    for (int i = 0; i < (int)N_ELEMENTS(special_flags) && n < max_entries; i++)
    {
        if ((f1 & special_flags[i].flag) == 0)
            continue;

        entries[n].kind = SMT_BONUS_ENTRY_SPECIAL;
        entries[n].index = special_flags[i].special;
        entries[n].flag_pos = 0;
        entries[n].flag_neg = 0;
        entries[n].flag = special_flags[i].flag;
        n++;
    }

    return n;
}

static int smith_collect_bonus_actions(smith_bonus_action* actions, int max_actions)
{
    smith_bonus_entry entries[16];
    int entry_count = smith_collect_bonus_entries(entries, (int)N_ELEMENTS(entries));
    int action_count = 0;

    if (!actions || max_actions <= 0)
        return 0;

    for (int i = 0; i < entry_count && action_count < max_actions; i++)
    {
        actions[action_count].entry = entries[i];
        actions[action_count].delta = 1;
        action_count++;

        if (action_count >= max_actions)
            break;
        actions[action_count].entry = entries[i];
        actions[action_count].delta = -1;
        action_count++;
    }

    return action_count;
}

static bool smith_adjust_bonus_entry(const smith_bonus_entry* entry, int delta)
{
    int max_bonus = pval_max();
    int floor_bonus = pval_min(); /* respect ego min_pval */
    int min_bonus = 0;
    int value = 0;

    if (!entry || !smith_o_ptr || delta == 0)
        return false;

    if (entry->kind == SMT_BONUS_ENTRY_STAT)
    {
        u32b f1, f2, f3;
        object_flags(smith_o_ptr, &f1, &f2, &f3);

        bool has_pos = (f1 & entry->flag_pos) != 0;
        bool has_neg = (f1 & entry->flag_neg) != 0;

        if (has_pos && has_neg)
        {
            min_bonus = -max_bonus;
        }
        else if (has_neg)
        {
            min_bonus = -max_bonus;
            max_bonus = 0;
        }
        else
        {
            /* Positive stat: honour ego min_pval as the lower bound */
            min_bonus = floor_bonus;
        }

        value = smith_o_ptr->stat_bonus[entry->index];
        int new_value = value + delta;
        if (new_value < min_bonus || new_value > max_bonus)
            return false;

        smith_o_ptr->stat_bonus[entry->index] = new_value;
        return true;
    }

    if (entry->kind == SMT_BONUS_ENTRY_SKILL)
    {
        /* Skill bonus: honour ego min_pval as the lower bound */
        value = smith_o_ptr->skill_bonus[entry->index];
        int new_value = value + delta;
        if (new_value < floor_bonus || new_value > max_bonus)
            return false;
        smith_o_ptr->skill_bonus[entry->index] = new_value;
        return true;
    }

    value = smith_o_ptr->pval;
    int new_value = value + delta;
    if (new_value < floor_bonus || new_value > max_bonus)
        return false;
    smith_o_ptr->pval = (s16b)new_value;
    return true;
}

static int smith_bonus_menu_aux(int* highlight)
{
    char ch;
    char buf[80];
    smith_bonus_action actions[26];
    bool valid[26] = { false };
    bool can_afford[26] = { false };
    byte attr[26];
    int num = smith_collect_bonus_actions(actions, (int)N_ELEMENTS(actions));
    const int first_row = 2;
    const int max_row = smith_ui_list_bottom_row(first_row, true);
    const int max_visible = max_row - first_row + 1;
    int top;

    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_outside_cancel_enabled(true);
    smith_ui_begin_touch_scroll_area(false);

    smith_ui_wipe_active_panel(indexed_menu_prefix_col(COL_SMT2));

    smith_ui_put_section_header(COL_SMT2, 1, "Bonuses");

    if (num <= 0)
    {
        smith_ui_put_fitted(COL_SMT2, 3,
            smith_ui_column_width(COL_SMT2), TERM_L_DARK,
            "(No editable special bonuses on this item.)");
        ui_menu_click_add_full_row(SMITH_CLICK_BACK, 3);
        Term_fresh();
        hide_cursor = true;
        (void)inkey();
        hide_cursor = false;
        return -1;
    }

    if (*highlight < 1)
        *highlight = 1;
    if (*highlight > num)
        *highlight = num;

    top = smith_ui_configure_list_view(SMITH_SCROLL_BONUSES, num, *highlight,
        first_row, max_row) + 1;
    if (num > max_visible)
    {
        int end = top + max_visible - 1;
        if (end > num)
            end = num;
        strnfmt(buf, sizeof(buf),
            "Adjust special bonuses [%d-%d/%d]", top, end,
            num);
        smith_ui_put_section_header(COL_SMT2, 1, buf);
    }

    object_type snapshot;
    smith_alloy_state alloy_snapshot = smith_alloy;

    for (int i = 0; i < num; i++)
    {
        object_copy(&snapshot, smith_o_ptr);

        if (smith_adjust_bonus_entry(&actions[i].entry, actions[i].delta))
        {
            valid[i] = true;
            can_afford[i] = affordable(smith_o_ptr);
        }

        object_copy(smith_o_ptr, &snapshot);
        smith_alloy = alloy_snapshot;

        attr[i] = valid[i] ? (can_afford[i] ? TERM_WHITE : TERM_SLATE)
                           : TERM_L_DARK;

        const char* name = NULL;
        int value = 0;
        if (actions[i].entry.kind == SMT_BONUS_ENTRY_STAT)
        {
            name = smith_bonus_stat_name(actions[i].entry.index);
            value = smith_o_ptr->stat_bonus[actions[i].entry.index];
        }
        else if (actions[i].entry.kind == SMT_BONUS_ENTRY_SKILL)
        {
            name = skill_names_full[actions[i].entry.index];
            value = smith_o_ptr->skill_bonus[actions[i].entry.index];
        }
        else
        {
            name = smith_bonus_special_name(actions[i].entry.index);
            value = smith_o_ptr->pval;
        }
        const char* verb = (actions[i].delta > 0) ? "increase" : "decrease";

        int entry_idx = i + 1;
        int row = first_row + (entry_idx - top);
        if (row >= first_row && row <= max_row)
        {
            char action_label[80];
            strnfmt(action_label, sizeof(action_label), "%s %-12s (%+d)",
                verb, name, value);
            indexed_menu_entry_label(buf, sizeof(buf), i, action_label);
            smith_ui_put_menu_row(entry_idx, COL_SMT2, row, attr[i], buf,
                *highlight == entry_idx);
        }
    }

    int hl_row = smith_ui_visible_highlight_row(
        *highlight, top - 1, first_row, max_row);

    prt_object_difficulty();
    prt_object_description();

    Term_fresh();
    Term_gotoxy(2, (hl_row >= first_row) ? hl_row : first_row);

    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    {
        int clicked_choice = 0;
        int click_action = UI_MENU_CLICK_PRIMARY;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if ((clicked_choice == SMITH_CLICK_BACK)
                || (click_action == UI_MENU_CLICK_SECONDARY))
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    return 0;
                ch = ESCAPE;
            }
            else if (clicked_choice >= 1 && clicked_choice <= num)
            {
                if (!smith_ui_pointer_choice_confirms(
                        clicked_choice, click_action, highlight))
                {
                    return 0;
                }
                ch = '\r';
            }
        }
    }

    if ((ch == '4') || (ch == ESCAPE))
        return -1;

    if (sdl_menu_letters_enabled()
        && (ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        int old_highlight = *highlight;

        *highlight = (int)ch - 'a' + 1;

        if (valid[*highlight - 1])
            return (*highlight);

        *highlight = old_highlight;
        bell("Invalid choice.");
    }

    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else
            *highlight = num;
        return 0;
    }

    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else
            *highlight = 1;
        return 0;
    }

    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (valid[*highlight - 1])
            return *highlight;
        bell("Invalid choice.");
        return 0;
    }

    return 0;
}

static void smith_bonus_menu(void)
{
    int highlight = 1;
    bool leave_menu = false;

    screen_save();

    while (!leave_menu)
    {
        int choice = smith_bonus_menu_aux(&highlight);
        if (choice == -1)
            leave_menu = true;
        else if (choice >= 1)
        {
            smith_bonus_action actions[26];
            int num = smith_collect_bonus_actions(actions, (int)N_ELEMENTS(actions));
            if (choice <= num)
                (void)smith_adjust_bonus_entry(&actions[choice - 1].entry, actions[choice - 1].delta);
        }
    }

    ui_menu_click_clear();
    screen_load();
}

/*
 * Displays a menu for modifying numerical bonuses and weight of an item.
 */
void numbers_menu(void)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;

    if (object_has_ego(smith_o_ptr))
        enchant_then_numbers = true;

    /* This submenu has no third column, so let its list use the full width. */
    bool saved_full_width = smith_ui_secondary_full_width;
    smith_ui_secondary_full_width = true;

    /* Save screen */
    screen_save();

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = numbers_menu_aux(&highlight);

        switch (choice)
        {
        case -1:
        {
            leave_menu = true;
            break;
        }

        default:
        {
            if (choice == SMT_NUM_MENU_EDIT_BONUSES)
                smith_bonus_menu();
            else
                modify_numbers(choice);
            break;
        }
        }
    }

    /* Load screen */
    ui_menu_click_clear();
    screen_load();

    smith_ui_secondary_full_width = saved_full_width;

    return;
}

static void ego_name_for_enchant_menu(int e_idx, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;
    buf[0] = '\0';
    if (e_idx <= 0 || e_idx >= z_info->e_max)
        return;

    ego_item_type* e_ptr = &e_info[e_idx];
    const char* raw = e_name + e_ptr->name;
    if (!raw || !raw[0])
        return;

    if (ego_name_is_prefix(raw))
    {
        size_t len = strlen(raw);
        size_t copy_len = (len >= 2) ? (len - 2) : 0;
        if (copy_len >= buflen)
            copy_len = buflen - 1;
        if (copy_len > 0)
        {
            memcpy(buf, raw + 1, copy_len);
            buf[copy_len] = '\0';
        }
        return;
    }

    SDL_strlcpy(buf, raw, buflen);
}

static void prt_reforge_preview(const reforge_preview_type* preview)
{
    char buf[80];
    int costs = 0;
    byte attr = TERM_SLATE;
    bool compact = smith_ui_compact_width();
    bool portrait = smith_ui_portrait_layout();
    int measure_row = -1;

    if (!portrait)
        wipe_screen_from(COL_SMT4);

    if (!preview)
        return;

    if (!preview->affordable)
        attr = TERM_L_DARK;

    if (portrait)
    {
        int divider_row = smith_ui_used_bottom_row() + 1;

        smith_ui_draw_horizontal_divider(divider_row);
        measure_row = divider_row + 1;
        strnfmt(buf, sizeof(buf), "Measure   Reforge: %d (+%d raw)",
            preview->scaled_difficulty, preview->raw_delta_difficulty);
        smith_ui_cost_title_row_override = measure_row
            + MAX(1, smith_ui_put_wrapped(COL_SMT1, measure_row,
                         smith_ui_line_width(COL_SMT1),
                         MAX(1, smith_ui_content_bottom_row() - measure_row + 1),
                         attr, buf));
    }
    else
    {
        smith_ui_put_fitted(COL_SMT4, 2, smith_ui_line_width(COL_SMT4), attr,
            "Reforge Diff:");
        strnfmt(buf, sizeof(buf), "%d", preview->scaled_difficulty);
        smith_ui_put_fitted(COL_SMT4 + 2, 4, 4, attr, buf);

        if (compact)
            strnfmt(buf, sizeof(buf), "+%d raw", preview->raw_delta_difficulty);
        else
            strnfmt(buf, sizeof(buf), "(+%d raw)",
                preview->raw_delta_difficulty);
        smith_ui_put_fitted(COL_SMT4 + (compact ? 4 : 5), 4,
            smith_ui_line_width(COL_SMT4 + (compact ? 4 : 5)), TERM_L_DARK,
            buf);
    }

    smith_ui_put_fitted(portrait ? COL_SMT1 : COL_SMT4,
        smith_ui_cost_title_row(),
        smith_ui_line_width(portrait ? COL_SMT1 : COL_SMT4),
        preview->affordable ? TERM_SLATE : TERM_L_DARK, "Cost:");

    if (preview->needs_reforging)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Reforging");
        costs++;
    }
    if (preview->cost.weaponsmith)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Weaponsmith");
        costs++;
    }
    if (preview->cost.armoursmith)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Armoursmith");
        costs++;
    }
    if (preview->cost.jeweller)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Jeweller");
        costs++;
    }
    if (preview->needs_forge)
    {
        smith_ui_put_cost_line(costs, TERM_RED, "Forge");
        costs++;
    }
    if (preview->cost.uses > 0)
    {
        attr = (preview->needs_forge || preview->needs_forge_resources)
            ? TERM_RED
            : TERM_SLATE;
        if (compact)
            strnfmt(buf, sizeof(buf), "%d/%d uses", preview->cost.uses,
                forge_uses(p_ptr->py, p_ptr->px));
        else
            strnfmt(buf, sizeof(buf), "%d Use%s", preview->cost.uses,
                (preview->cost.uses == 1) ? "" : "s");
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.drain > 0)
    {
        attr = (preview->cost.drain <= p_ptr->skill_base[S_SMT])
            ? TERM_BLUE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Smithing", preview->cost.drain);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.mithril > 0)
    {
        attr = (preview->cost.mithril <= mithril_carried()) ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), compact ? "%d.%d lb Mith" : "%d.%d lb Mithril",
            preview->cost.mithril / 10, preview->cost.mithril % 10);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.star_iron > 0)
    {
        attr = (preview->cost.star_iron <= star_iron_carried()) ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), compact ? "%d.%d lb StIron" : "%d.%d lb Star Iron",
            preview->cost.star_iron / 10, preview->cost.star_iron % 10);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.str > 0)
    {
        attr = (p_ptr->stat_base[A_STR] + p_ptr->stat_drain[A_STR] - preview->cost.str >= -5)
            ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Str", preview->cost.str);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.dex > 0)
    {
        attr = (p_ptr->stat_base[A_DEX] + p_ptr->stat_drain[A_DEX] - preview->cost.dex >= -5)
            ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Dex", preview->cost.dex);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.con > 0)
    {
        attr = (p_ptr->stat_base[A_CON] + p_ptr->stat_drain[A_CON] - preview->cost.con >= -5)
            ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Con", preview->cost.con);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.gra > 0)
    {
        attr = (p_ptr->stat_base[A_GRA] + p_ptr->stat_drain[A_GRA] - preview->cost.gra >= -5)
            ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Gra", preview->cost.gra);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }
    if (preview->cost.exp > 0)
    {
        attr = (p_ptr->new_exp >= preview->cost.exp) ? TERM_SLATE : TERM_L_DARK;
        strnfmt(buf, sizeof(buf), "%d Exp", preview->cost.exp);
        smith_ui_put_cost_line(costs, attr, buf);
        costs++;
    }

    strnfmt(buf, sizeof(buf), "%d Turns", preview->turns);
    smith_ui_put_cost_line(costs, TERM_SLATE, buf);
}

static bool reforge_preview_missing_ability(const reforge_preview_type* preview)
{
    if (!preview)
        return false;

    return preview->needs_reforging || preview->cost.weaponsmith
        || preview->cost.armoursmith || preview->cost.jeweller;
}

static bool reforge_preview_missing_forge(const reforge_preview_type* preview)
{
    if (!preview)
        return false;

    return preview->needs_forge || preview->needs_forge_resources;
}

static bool reforge_preview_primary_blocker(const reforge_preview_type* preview)
{
    return reforge_preview_missing_ability(preview)
        || reforge_preview_missing_forge(preview);
}

static int reforge_prefix_menu(const object_type* source)
{
    char ch;
    char buf[80];
    int i;
    int highlight = 1;
    int entry_count = 0;
    int choice[26];
    bool valid[26];
    reforge_preview_type previews[26];
    const int first_row = 2;
    const int last_row = smith_ui_list_bottom_row(first_row, true);

    if (!source || !source->k_idx)
        return 0;

    screen_save();

    while (true)
    {
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_outside_cancel_enabled(true);
        smith_ui_begin_touch_scroll_area(false);

        smith_ui_wipe_active_panel(indexed_menu_prefix_col(COL_SMT2));
        smith_ui_put_section_header(COL_SMT2, 1, "Prefix");

        entry_count = 0;
        memset(choice, 0, sizeof(choice));
        memset(valid, 0, sizeof(valid));
        memset(previews, 0, sizeof(previews));

        for (i = 1; i < z_info->e_max && entry_count < (int)N_ELEMENTS(choice); i++)
        {
            if (!ego_prefix_can_apply_to_object(source, i))
                continue;
            if (!reforge_preview_build(source, i, &previews[entry_count]))
                continue;

            valid[entry_count] = previews[entry_count].affordable;
            choice[entry_count] = i;

            entry_count++;
        }

        if (entry_count == 0)
        {
            smith_ui_put_fitted(COL_SMT2, 3,
                smith_ui_column_width(COL_SMT2), TERM_L_DARK,
                "(No legal prefixes available.)");
            ui_menu_click_add_full_row(SMITH_CLICK_BACK, 3);
            Term_fresh();
            hide_cursor = true;
            (void)inkey();
            hide_cursor = false;
            ui_menu_click_clear();
            screen_load();
            return 0;
        }

        if (highlight < 1) highlight = 1;
        if (highlight > entry_count) highlight = entry_count;

        {
            int top = smith_ui_configure_list_view(SMITH_SCROLL_REFORGE,
                entry_count, highlight, first_row, last_row);

            for (i = top;
                 i < entry_count && i < top + (last_row - first_row + 1); i++)
            {
                char ego_label[64];
                int row = first_row + i - top;

                ego_name_for_enchant_menu(choice[i], ego_label,
                    sizeof(ego_label));
                indexed_menu_entry_label(buf, sizeof(buf), i, ego_label);
                smith_ui_put_menu_row(i + 1, COL_SMT2, row,
                    valid[i] ? TERM_WHITE
                             : (reforge_preview_primary_blocker(&previews[i])
                                    ? TERM_RED
                                    : TERM_L_DARK),
                    buf, highlight == i + 1);
            }

            Term_gotoxy(14,
                first_row + MAX(0,
                    MIN(last_row - first_row, highlight - 1 - top)));
        }

        (void)reforge_preview_build(source, choice[highlight - 1],
            &previews[highlight - 1]);
        prt_reforge_preview(&previews[highlight - 1]);
        prt_object_description();

        Term_fresh();
        hide_cursor = true;
        ch = inkey();
        hide_cursor = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if ((clicked_choice == SMITH_CLICK_BACK)
                    || (click_action == UI_MENU_CLICK_SECONDARY))
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                        continue;
                    ch = ESCAPE;
                }
                else if (clicked_choice >= 1 && clicked_choice <= entry_count)
                {
                    if (!smith_ui_pointer_choice_confirms(
                            clicked_choice, click_action, &highlight))
                    {
                        continue;
                    }
                    ch = '\r';
                }
            }
        }

        if (sdl_menu_letters_enabled()
            && (ch >= 'a') && (ch <= (char)'a' + entry_count - 1))
        {
            highlight = (int)ch - 'a' + 1;
            if (reforge_preview_missing_ability(&previews[highlight - 1]))
                bell("You lack the ability for that reforge.");
            else if (reforge_preview_missing_forge(&previews[highlight - 1]))
                bell("You need a forge with resources for that reforge.");
            else if (!valid[highlight - 1])
                bell("You cannot afford that reforge.");
            else
            {
                ui_menu_click_clear();
                screen_load();
                return choice[highlight - 1];
            }
        }
        else if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
            || (ch == ARROW_RIGHT)
#endif
            )
        {
            if (reforge_preview_missing_ability(&previews[highlight - 1]))
                bell("You lack the ability for that reforge.");
            else if (reforge_preview_missing_forge(&previews[highlight - 1]))
                bell("You need a forge with resources for that reforge.");
            else if (!valid[highlight - 1])
                bell("You cannot afford that reforge.");
            else
            {
                ui_menu_click_clear();
                screen_load();
                return choice[highlight - 1];
            }
        }
        else if ((ch == ESCAPE) || (ch == '4')
#ifdef ARROW_LEFT
            || (ch == ARROW_LEFT)
#endif
            )
        {
            ui_menu_click_clear();
            screen_load();
            return 0;
        }
        else if (ch == '8'
#ifdef ARROW_UP
            || ch == ARROW_UP
#endif
            )
        {
            if (highlight > 1)
                highlight--;
            else
                highlight = entry_count;
        }
        else if (ch == '2'
#ifdef ARROW_DOWN
            || ch == ARROW_DOWN
#endif
            )
        {
            if (highlight < entry_count)
                highlight++;
            else
                highlight = 1;
        }
    }
}

static void create_special(int ego_prefix, int ego_suffix)
{
    /* Retrieve a backup of the object */
    object_copy(smith_o_ptr, smith2_o_ptr);
    smith_alloy = smith2_alloy;

    /* Suffix egos marked NO_PREFIX cannot be combined with any prefix. */
    if (ego_forbids_prefix_combo(ego_suffix))
        ego_prefix = 0;

    /* Apply requested ego affixes */
    object_set_ego_prefix(smith_o_ptr, ego_prefix);
    object_set_ego_suffix(smith_o_ptr, ego_suffix);

    /* Apply ego bonuses */
    if (object_has_ego(smith_o_ptr))
        object_into_special(smith_o_ptr, p_ptr->skill_use[S_SMT], true);

    /* Re-evaluate stack size now that an enchantment is applied */
    smith_o_ptr->number = smith_default_stack_size(smith_o_ptr);
}

static bool enchant_menu_has_applicable_affix(const object_type* base_o_ptr,
    int fixed_prefix, int fixed_suffix, bool selecting_prefix)
{
    int i;

    if (!base_o_ptr || !smith_o_ptr || base_o_ptr->tval == 0)
        return false;
    if (object_has_evil_alignment(smith_o_ptr))
        return false;

    for (i = 1; i < z_info->e_max; i++)
    {
        if (smith_ego_can_apply_to_object(
                base_o_ptr, i, fixed_prefix, fixed_suffix, selecting_prefix))
            return true;
    }

    return false;
}

/*
 * Performs the interface and selection work for the enchantment menu.
 */
static int enchant_menu_aux(int* highlight, int fixed_prefix, int fixed_suffix,
    bool selecting_prefix, const object_type* base_o_ptr)
{
    char ch;
    int i;
    int entry_count = 0;
    char buf[80];
    bool valid[26];
    int choice[26];
    char title[80];
    const int first_row = 2;
    const int last_row = smith_ui_list_bottom_row(first_row, true);
    int top;
    bool no_prefix_allowed = selecting_prefix
        && ego_forbids_prefix_combo(fixed_suffix);

    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_outside_cancel_enabled(true);
    smith_ui_begin_touch_scroll_area(false);

    // clear the right of the screen
    smith_ui_wipe_active_panel(indexed_menu_prefix_col(COL_SMT2));

    /* Header */
    strnfmt(title, sizeof(title), "%s", selecting_prefix ? "prefix" : "suffix");
    title[0] = (char)toupper((unsigned char)title[0]);
    smith_ui_put_section_header(COL_SMT2, 1, title);

    /* Always allow selecting no affix */
    valid[entry_count] = true;
    choice[entry_count] = 0;
    entry_count++;

    /* We have to search the whole special item list. */
    for (i = 1; i < z_info->e_max && entry_count < (int)N_ELEMENTS(choice); i++)
    {
        if (smith_ego_can_apply_to_object(
                base_o_ptr, i, fixed_prefix, fixed_suffix, selecting_prefix))
        {
            /* Make a preview 'special' version of the object */
            if (selecting_prefix)
                create_special(i, fixed_suffix);
            else
                create_special(fixed_prefix, i);

            // Check whether it is a valid choice for creating
            if (affordable(smith_o_ptr))
            {
                valid[entry_count] = true;
            }
            else
            {
                valid[entry_count] = false;
            }

            /* Remember the object index */
            choice[entry_count] = i;

            // count the applicable items
            entry_count++;
        }
    }

    if (*highlight < 1) *highlight = 1;
    if (*highlight > entry_count) *highlight = entry_count;

    top = smith_ui_configure_list_view(SMITH_SCROLL_ENCHANT, entry_count,
        *highlight, first_row, last_row);
    for (i = top;
         i < entry_count && i < top + (last_row - first_row + 1); i++)
    {
        char ego_label[64];
        int row = first_row + i - top;

        if (choice[i] == 0)
            SDL_strlcpy(ego_label, "(none)", sizeof(ego_label));
        else
            ego_name_for_enchant_menu(choice[i], ego_label, sizeof(ego_label));
        indexed_menu_entry_label(buf, sizeof(buf), i, ego_label);
        smith_ui_put_menu_row(i + 1, COL_SMT2, row,
            valid[i] ? TERM_WHITE : TERM_SLATE, buf,
            *highlight == i + 1);
    }
    if (no_prefix_allowed)
    {
        smith_ui_put_wrapped(COL_SMT2, last_row + 1,
            smith_ui_column_width(COL_SMT2), 2, TERM_SLATE,
            "No prefix is allowed with this suffix.");
    }

    /* Make a preview 'special' version of the object */
    if (selecting_prefix)
        create_special(choice[*highlight - 1], fixed_suffix);
    else
        create_special(fixed_prefix, choice[*highlight - 1]);

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14,
        first_row + MAX(0, MIN(last_row - first_row, *highlight - 1 - top)));

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    {
        int clicked_choice = 0;
        int click_action = UI_MENU_CLICK_PRIMARY;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if ((clicked_choice == SMITH_CLICK_BACK)
                || (click_action == UI_MENU_CLICK_SECONDARY))
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    return 0;
                ch = ESCAPE;
            }
            else if (clicked_choice >= 1 && clicked_choice <= entry_count)
            {
                if (!smith_ui_pointer_choice_confirms(
                        clicked_choice, click_action, highlight))
                {
                    return 0;
                }
                ch = '\r';
            }
        }
    }

    /* Choose by letter */
    if (sdl_menu_letters_enabled()
        && (ch >= 'a') && (ch <= (char)'a' + entry_count - 1))
    {
        *highlight = (int)ch - 'a' + 1;
        ch = '\r';
    }

    /* Rebuild from the selected logical row for every input source.  Pointer
     * activation can update the highlight after the old preview was made. */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
#ifdef ARROW_RIGHT
        || (ch == ARROW_RIGHT)
#endif
        )
    {
        if (selecting_prefix)
            create_special(choice[*highlight - 1], fixed_suffix);
        else
            create_special(fixed_prefix, choice[*highlight - 1]);

        return (*highlight);
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE)
#ifdef ARROW_LEFT
        || (ch == ARROW_LEFT)
#endif
        )
    {
        *highlight = -1;
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8'
#ifdef ARROW_UP
        || (ch == ARROW_UP)
#endif
        )
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = entry_count;
    }

    /* Next item */
    if (ch == '2'
#ifdef ARROW_DOWN
        || (ch == ARROW_DOWN)
#endif
        )
    {
        if (*highlight < entry_count)
            (*highlight)++;
        else if (*highlight == entry_count)
            *highlight = 1;
    }

    return (0);
}

/*
 * Brings up a menu for making an item into a {special} item.
 */
bool enchant_menu(void)
{
    int prefix_highlight = 1;
    int suffix_highlight = 1;

    bool completed = false;
    bool leave_menu = false;

    /* Save screen */
    screen_save();

    // stop the item being an artefact, if it was
    smith_o_ptr->name1 = 0;
    smith2_o_ptr->name1 = 0;

    int selected_prefix = (int)object_ego_prefix(smith_o_ptr);
    int selected_suffix = (int)object_ego_suffix(smith_o_ptr);

    bool show_prefix_step =
        enchant_menu_has_applicable_affix(
            smith2_o_ptr, 0, selected_suffix, true) || (selected_prefix != 0);
    bool show_suffix_step =
        enchant_menu_has_applicable_affix(
            smith2_o_ptr, selected_prefix, 0, false) || (selected_suffix != 0);

    if (!show_prefix_step && !show_suffix_step)
    {
        /* Nothing to select; bail out without changing the item. */
        ui_menu_click_clear();
        screen_load();
        return false;
    }

    bool selecting_prefix = show_prefix_step;

    /* This submenu has no third column, so let its list use the full width. */
    bool saved_full_width = smith_ui_secondary_full_width;
    smith_ui_secondary_full_width = true;

    /* Process events until menu is abandoned */
    while (!leave_menu)
    {
        if (selecting_prefix)
        {
            int choice_idx = enchant_menu_aux(
                &prefix_highlight, 0, selected_suffix, true, smith2_o_ptr);

            if (choice_idx == -1)
            {
                completed = false;
                leave_menu = true;
                continue;
            }

            if (choice_idx >= 1)
            {
                selected_prefix = (int)object_ego_prefix(smith_o_ptr);
                create_special(selected_prefix, selected_suffix);

                if (show_suffix_step)
                {
                    selecting_prefix = false;
                    continue;
                }

                completed = true;
                leave_menu = true;
                continue;
            }
        }
        else
        {
            int choice_idx = enchant_menu_aux(
                &suffix_highlight, selected_prefix, 0, false, smith2_o_ptr);

            if (choice_idx == -1)
            {
                if (show_prefix_step)
                {
                    /* Back to prefix selection */
                    create_special(selected_prefix, selected_suffix);
                    selecting_prefix = true;
                    continue;
                }

                completed = false;
                leave_menu = true;
                continue;
            }

            if (choice_idx >= 1)
            {
                selected_suffix = (int)object_ego_suffix(smith_o_ptr);
                create_special(selected_prefix, selected_suffix);
                completed = true;
                leave_menu = true;
                continue;
            }
        }
    }

    /* Load screen */
    ui_menu_click_clear();
    screen_load();

    smith_ui_secondary_full_width = saved_full_width;

    return (completed);
}

/*
 * Copies an artefact structure over the top of another one.
 */
void artefact_copy(artefact_type* a1_ptr, artefact_type* a2_ptr)
{
    /* Copy the structure */
    memcpy(a1_ptr, a2_ptr, sizeof(artefact_type));
}

/*
 * Fills in the details on the artefact type being created.
 */
void add_artefact_details(void)
{
    smith_a_ptr->tval = smith_o_ptr->tval;
    smith_a_ptr->sval = smith_o_ptr->sval;
    smith_a_ptr->pval = smith_o_ptr->pval;
    smith_a_ptr->att = smith_o_ptr->att;
    smith_a_ptr->evn = smith_o_ptr->evn;
    smith_a_ptr->dd = smith_o_ptr->dd;
    smith_a_ptr->ds = smith_o_ptr->ds;
    smith_a_ptr->pd = smith_o_ptr->pd;
    smith_a_ptr->ps = smith_o_ptr->ps;
    smith_a_ptr->weight = smith_o_ptr->weight;
    smith_a_ptr->flags1 |= (&k_info[smith_o_ptr->k_idx])->flags1;
    smith_a_ptr->flags2 |= (&k_info[smith_o_ptr->k_idx])->flags2;
    smith_a_ptr->flags3 |= (&k_info[smith_o_ptr->k_idx])->flags3;

    memcpy(smith_a_ptr->stat_bonus, smith_o_ptr->stat_bonus, sizeof(smith_a_ptr->stat_bonus));
    memcpy(smith_a_ptr->skill_bonus, smith_o_ptr->skill_bonus, sizeof(smith_a_ptr->skill_bonus));
    memset(smith_a_ptr->stat_bonus_set, 0, sizeof(smith_a_ptr->stat_bonus_set));
    memset(smith_a_ptr->skill_bonus_set, 0, sizeof(smith_a_ptr->skill_bonus_set));

    smith_a_ptr->cur_num = 1;
    smith_a_ptr->found_num = 1;
    smith_a_ptr->spawn_num = 1;
    smith_a_ptr->level = object_difficulty(smith_o_ptr);
    smith_a_ptr->rarity = 10;
}

/*
 * Prepares an artefact for modification.
 */
void prepare_artefact(void)
{
    int i;

    log_debug("Preparing artifact for modification");

    // retrieve a backup of the artefact
    artefact_copy(smith_a_ptr, smith2_a_ptr);

    // retrieve a backup of the object
    object_copy(smith_o_ptr, smith2_o_ptr);
    smith_alloy = smith2_alloy;

    // set its 'artefact' name to reflect the chosen type
    smith_o_ptr->name1 = smith_a_name;

    // Restore default stack sizes for arrows and other throwable gear
    smith_o_ptr->number = smith_default_stack_size(smith_o_ptr);

    // as abilities are represented on the o_ptr not the a_ptr in Sil
    // we need to synchronise them on the smith_o_ptr
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
        smith_o_ptr->bane_type[i] = smith_a_ptr->bane_type[i];
    }
    smith_o_ptr->abilities = smith_a_ptr->abilities;

    log_trace("Artifact preparation complete - %d abilities synchronized", smith_a_ptr->abilities);
}

/*
 * Does the given object type support the given flag type?
 */
bool applicable_flag(u32b f, int flagset, object_type* o_ptr)
{
    bool ok = false;
    int i;
    u32b f1, f2, f3, f4;

    /* Telchar may always put SHARPNESS II on a melee weapon               */
    if ((flagset == 1) && (f == TR1_SHARPNESS2) &&
        (c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_TELCHAR))
    {
        switch (smith_o_ptr->tval)                   /* any melee weapon   */
        {
            case TV_SWORD: case TV_HAFTED:
            case TV_POLEARM: case TV_DIGGING:
                return true;
        }
    }

    /* Extract the object flags */
    object_flags4(o_ptr, &f1, &f2, &f3, &f4);

    /* Warhammers-only: Smithing bonus requires Brand Fire on the same item. */
    if ((flagset == 1) && (f == TR1_SMT))
    {
        if (o_ptr->tval != TV_HAFTED || o_ptr->sval != SV_WAR_HAMMER)
            return false;
        if (!(f1 & TR1_BRAND_FIRE))
            return false;
        return true;
    }

    /* Go through the list of artefacts and see if the flag is applicable for
     * this type  */
    for (i = ART_ULTIMATE; i < z_info->art_norm_max; i++)
    {
        /* Access the artefact */
        artefact_type* a_ptr = &a_info[i];

        /* Skip other types of artefacts */
        if (a_ptr->tval != o_ptr->tval)
            continue;

        switch (flagset)
        {
        case 1:
        {
            if (a_ptr->flags1 & f)
                ok = true;
            break;
        }
        case 2:
        {
            if (a_ptr->flags2 & f)
                ok = true;
            break;
        }
        case 3:
        {
            if (a_ptr->flags3 & f)
                ok = true;
            break;
        }
        case 4:
        {
            if (a_ptr->flags4 & f)
                ok = true;
            break;
        }
        }
    }

    return (ok);
}

/*
 * Adds a given flag to the dummy artefact.
 */
void add_artefact_flag(u32b f, int flagset)
{
    u32b f1_before, f2, f3;
    u32b f1_after;

    log_trace("Adding artifact flag %u in flagset %d", f, flagset);

    // prepare the artefact and object for modification
    prepare_artefact();

    object_flags(smith_o_ptr, &f1_before, &f2, &f3);

    // set new flag on the artefact
    if (flagset == 1)
        smith_a_ptr->flags1 |= f;
    if (flagset == 2)
        smith_a_ptr->flags2 |= f;
    if (flagset == 3)
        smith_a_ptr->flags3 |= f;
    if (flagset == 4)
        smith_a_ptr->flags4 |= f;

    object_flags(smith_o_ptr, &f1_after, &f2, &f3);
    smith_apply_stat_skill_flag_delta(smith_o_ptr, f1_before, f1_after);
}

/*
 * Removes a given flag from the dummy artefact.
 */
void remove_artefact_flag(u32b f, int flagset)
{
    u32b f1_before, f2, f3;
    u32b f1_after;

    log_trace("Removing artifact flag %u from flagset %d", f, flagset);

    // prepare the artefact and object for modification
    prepare_artefact();

    object_flags(smith_o_ptr, &f1_before, &f2, &f3);

    // unset new flag on the artefact
    if (flagset == 1)
        smith_a_ptr->flags1 &= ~(f);
    if (flagset == 2)
        smith_a_ptr->flags2 &= ~(f);
    if (flagset == 3)
        smith_a_ptr->flags3 &= ~(f);
    if (flagset == 4)
        smith_a_ptr->flags4 &= ~(f);

    /* Keep Smithing dependent on Brand Fire. */
    if ((flagset == 1) && (f == TR1_BRAND_FIRE))
        smith_a_ptr->flags1 &= ~(TR1_SMT);

    object_flags(smith_o_ptr, &f1_after, &f2, &f3);
    smith_apply_stat_skill_flag_delta(smith_o_ptr, f1_before, f1_after);
}

/*
 * Performs the interface and selection work for the artefact flag selection.
 */
int artefact_flag_menu_aux(int category, int* highlight)
{
    char ch;
    int i, num = 0;
    char buf[80];
    bool flag_present[MAX_SMITHING_FLAGS] = { false };
    bool flag_valid[MAX_SMITHING_FLAGS] = { false };
    bool flag_affordable[MAX_SMITHING_FLAGS] = { false };
    u32b flag[MAX_SMITHING_FLAGS];
    int flagset[MAX_SMITHING_FLAGS];
    byte flag_attr[MAX_SMITHING_FLAGS];
    cptr flag_desc[MAX_SMITHING_FLAGS];
    const int first_row = 2;
    const int last_row = smith_ui_list_bottom_row(first_row, true);
    int top;

    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_outside_cancel_enabled(true);
    smith_ui_begin_touch_scroll_area(false);

    // clear the right of the screen
    smith_ui_wipe_active_panel(indexed_menu_prefix_col(COL_SMT3));
    smith_ui_put_section_header(COL_SMT3, 1, "Property");

    // display the categories
    for (i = 0; smithing_flag_types[i].flag != 0; i++)
    {
        if (category == smithing_flag_types[i].category)
        {
            /* Telchar-only: skip Sharpness2 if not in character Telchar */
            if ((smithing_flag_types[i].flagset == 1) &&
                (smithing_flag_types[i].flag == TR1_SHARPNESS2) &&
                !(c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_TELCHAR))
            {
                /* don't even consider it */
                continue;
            }
            flag[num] = smithing_flag_types[i].flag;
            flagset[num] = smithing_flag_types[i].flagset;

            if (((flagset[num] == 1) && (smith2_a_ptr->flags1 & flag[num]))
                || ((flagset[num] == 2) && (smith2_a_ptr->flags2 & flag[num]))
                || ((flagset[num] == 3) && (smith2_a_ptr->flags3 & flag[num]))
                || ((flagset[num] == 4) && (smith2_a_ptr->flags4 & flag[num])))
            {
                flag_present[num] = true;
                flag_valid[num] = true;
            }

            else
            {
                // require that the flag can be present on the object
                if (applicable_flag(flag[num], flagset[num], smith_o_ptr))
                {
                    flag_valid[num] = true;

                    // add this flag to the dummy artefact under construction
                    add_artefact_flag(flag[num], flagset[num]);

                    // Check whether it is a valid choice for creating (needs to
                    // be affordable and successful)
                    if (affordable(smith_o_ptr))
                    {
                        flag_affordable[num] = true;
                    }
                }
            }

        // /* Lock Sharpness II behind Telchar forge */
        // if (flag[num] == TR1_SHARPNESS2 &&
        //     !(c_info[p_ptr->pcharacter].flags_u & UNQ_SMT_TELCHAR))
        //     flag_valid[num] = false;

            flag_attr[num] = flag_present[num]
                ? TERM_BLUE
                : (flag_valid[num]
                        ? (flag_affordable[num] ? TERM_WHITE : TERM_SLATE)
                        : TERM_L_DARK);
            flag_desc[num] = smithing_flag_types[i].desc;

            num++;
        }
    }

    /* Abort if there are no choices */
    if (num == 0)
    {
        smith_ui_put_fitted(COL_SMT3, 3,
            smith_ui_column_width(COL_SMT3), TERM_L_DARK,
            "(No properties can be added to this item.)");
        ui_menu_click_add_full_row(SMITH_CLICK_BACK, 3);
        Term_fresh();
        hide_cursor = true;
        (void)inkey();
        hide_cursor = false;
        return (-1);
    }

    if (*highlight < 1)
        *highlight = 1;
    if (*highlight > num)
        *highlight = num;
    top = smith_ui_configure_list_view(SMITH_SCROLL_FLAG, num, *highlight,
        first_row, last_row);
    for (i = top; i < num && i < top + (last_row - first_row + 1); i++)
    {
        int row = first_row + i - top;

        indexed_menu_entry_label(buf, sizeof(buf), i, flag_desc[i]);
        smith_ui_put_menu_row(i + 1, COL_SMT3, row, flag_attr[i], buf,
            *highlight == i + 1);
    }

    // add this flag to the dummy artefact under construction
    add_artefact_flag(flag[*highlight - 1], flagset[*highlight - 1]);

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14,
        first_row + MAX(0, MIN(last_row - first_row, *highlight - 1 - top)));

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    {
        int clicked_choice = 0;
        int click_action = UI_MENU_CLICK_PRIMARY;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if ((clicked_choice == SMITH_CLICK_BACK)
                || (click_action == UI_MENU_CLICK_SECONDARY))
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    return 0;
                ch = ESCAPE;
            }
            else if (clicked_choice >= 1 && clicked_choice <= num)
            {
                if (!smith_ui_pointer_choice_confirms(
                        clicked_choice, click_action, highlight))
                {
                    return 0;
                }
                ch = '\r';
            }
        }
    }

    /* Choose by letter */
    if (sdl_menu_letters_enabled()
        && (ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        *highlight = (int)ch - 'a' + 1;
        ch = '\r';
    }

    /* Toggle the selected logical row from the saved baseline.  add/remove
     * prepare that baseline themselves, so this is correct even when a click
     * changed the highlight after another row was previewed. */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (flag_valid[*highlight - 1])
        {
            if (flag_present[*highlight - 1])
                remove_artefact_flag(
                    flag[*highlight - 1], flagset[*highlight - 1]);
            else
                add_artefact_flag(
                    flag[*highlight - 1], flagset[*highlight - 1]);

            // backup the new artefact
            artefact_copy(smith2_a_ptr, smith_a_ptr);
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;

            return (*highlight);
        }

        else
        {
            bell("Invalid choice.");
        }
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        *highlight = -1;

        // restore the backup artefact and object
        prepare_artefact();

        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = num;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else if (*highlight == num)
            *highlight = 1;
    }

    return (0);
}

/*
 * Brings up a menu to select individual flags of a given type to
 * add to (or subtract from) an artefact.
 */
void artefact_flag_menu(int category)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;

    /* Save screen */
    screen_save();

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = artefact_flag_menu_aux(category, &highlight);

        if (choice >= 1)
        {
            // don't leave the menu
        }
        else if (choice == -1)
        {
            leave_menu = true;
        }
    }

    /* Load screen */
    ui_menu_click_clear();
    screen_load();
}

/*
 * Can this ability be applied to any item at all?
 * Returns false for stat-only abilities like Grace/Strength/etc that have no valid item types.
 */
static bool ability_can_be_smithed(ability_type* b_ptr)
{
    int j;

    /* Check if this ability has any valid item types defined */
    for (j = 0; j < ABILITY_TVALS_MAX; j++)
    {
        if (b_ptr->tval[j] != 0)
            return true;
    }

    return false;
}

/*
 * Does the given object type support the given ability type?
 */
bool applicable_ability(ability_type* b_ptr, object_type* o_ptr)
{
    bool ok = false;
    int j;

    u32b f1, f2, f3;

    /* Test if this is a legal item type for this ability */
    for (j = 0; j < ABILITY_TVALS_MAX; j++)
    {
        /* Require identical base type */
        if (o_ptr->tval == b_ptr->tval[j])
        {
            /* Require sval in bounds, lower */
            if (o_ptr->sval >= b_ptr->min_sval[j])
            {
                /* Require sval in bounds, upper */
                if (o_ptr->sval <= b_ptr->max_sval[j])
                {
                    /* Accept */
                    ok = true;
                }
            }
        }
    }

    // Polearm Mastery is OK for Polearms
    object_flags(o_ptr, &f1, &f2, &f3);
    if (f3 & TR3_POLEARM)
    {
        if ((b_ptr->skilltype == S_MEL) && (b_ptr->abilitynum == MEL_POLEARMS))
            ok = true;
    }

    return (ok);
}

/*
 * Adds a given ability to the dummy artefact.
 */
void add_artefact_ability(int skilltype, int abilitynum)
{
    int i;

    log_trace("Adding artifact ability - skill:%d ability:%d", skilltype, abilitynum);

    // prepare the artefact and object for modification
    prepare_artefact();

    // set new ability on the artefact
    if (smith_a_ptr->abilities < 4)
    {
        bool already_present = false;

        for (i = 0; i < smith_a_ptr->abilities; i++)
        {
            if ((smith_a_ptr->skilltype[i] == skilltype)
                && (smith_a_ptr->abilitynum[i] == abilitynum))
            {
                already_present = true;
            }
        }

        if (!already_present)
        {
            smith_a_ptr->skilltype[smith_a_ptr->abilities] = skilltype;
            smith_a_ptr->abilitynum[smith_a_ptr->abilities] = abilitynum;
            smith_a_ptr->bane_type[smith_a_ptr->abilities] = 0; // Player-smithed banes use player choice
            smith_a_ptr->abilities++;
        }
    }

    // as abilities are represented on the o_ptr not the a_ptr in Sil
    // we need to synchronise them on the smith_o_ptr
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
        smith_o_ptr->bane_type[i] = smith_a_ptr->bane_type[i];
    }
    smith_o_ptr->abilities = smith_a_ptr->abilities;
}

/*
 * Removes a given ability from the dummy artefact.
 */
void remove_artefact_ability(int skilltype, int abilitynum)
{
    int i;
    int location = -1;

    log_trace("Removing artifact ability - skill:%d ability:%d", skilltype, abilitynum);

    // prepare the artefact and object for modification
    prepare_artefact();

    // remove new ability on the artefact
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        if ((smith_a_ptr->skilltype[i] == skilltype)
            && (smith_a_ptr->abilitynum[i] == abilitynum))
        {
            location = i;
        }
    }

    if (location >= 0)
    {
        for (i = location; i < smith_a_ptr->abilities - 1; i++)
        {
            smith_a_ptr->skilltype[i] = smith_a_ptr->skilltype[i + 1];
            smith_a_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i + 1];
            smith_a_ptr->bane_type[i] = smith_a_ptr->bane_type[i + 1];
        }

        smith_a_ptr->skilltype[smith_a_ptr->abilities - 1] = 0;
        smith_a_ptr->abilitynum[smith_a_ptr->abilities - 1] = 0;
        smith_a_ptr->bane_type[smith_a_ptr->abilities - 1] = 0;

        smith_a_ptr->abilities--;
    }

    // as abilities are represented on the o_ptr not the a_ptr in Sil
    // we need to synchronise them on the smith_o_ptr
    for (i = 0; i < smith_a_ptr->abilities; i++)
    {
        smith_o_ptr->skilltype[i] = smith_a_ptr->skilltype[i];
        smith_o_ptr->abilitynum[i] = smith_a_ptr->abilitynum[i];
        smith_o_ptr->bane_type[i] = smith_a_ptr->bane_type[i];
    }
    smith_o_ptr->abilities = smith_a_ptr->abilities;
}

/*
 * Determines if an artefact type has a given ability.
 */
bool has_ability(artefact_type* a_ptr, int skilltype, int abilitynum)
{
    int i;

    for (i = 0; i < a_ptr->abilities; i++)
    {
        if ((a_ptr->skilltype[i] == skilltype)
            && (a_ptr->abilitynum[i] == abilitynum))
            return (true);
    }

    return (false);
}

/*
 * Performs the interface and selection work for the artefact flag selection.
 */
int artefact_ability_menu_aux(int skill, int* highlight)
{
    char ch;
    int i, num = 0;
    char buf[80];
    ability_type* b_ptr;
    const int first_row = 2;
    const int last_row = smith_ui_list_bottom_row(first_row, true);
    int top;

    /* Allocate arrays dynamically based on actual max abilities */
    bool* ability_present = mem_alloc_array(z_info->b_max, bool);
    bool* ability_valid = mem_alloc_array(z_info->b_max, bool);
    bool* ability_affordable = mem_alloc_array(z_info->b_max, bool);
    int* ability_nums = mem_alloc_array(z_info->b_max, int);

    /* Initialize arrays to zero/false */
    memset(ability_present, 0, z_info->b_max * sizeof(bool));
    memset(ability_valid, 0, z_info->b_max * sizeof(bool));
    memset(ability_affordable, 0, z_info->b_max * sizeof(bool));

    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_outside_cancel_enabled(true);
    smith_ui_begin_touch_scroll_area(false);

    // clear the right of the screen
    smith_ui_wipe_active_panel(indexed_menu_prefix_col(COL_SMT3));
    smith_ui_put_section_header(COL_SMT3, 1, "Ability");

    // list the abilities
    for (i = 0; i < z_info->b_max; i++)
    {
        b_ptr = &b_info[i];

        /* Skip non-entries */
        if (!b_ptr->name)
            continue;

        /* Skip entries for the wrong skill type */
        if (b_ptr->skilltype != skill)
            continue;

        /* Skip abilities that can't be smithed onto any item (like Grace, stat improvements) */
        if (!ability_can_be_smithed(b_ptr))
            continue;

        // Store the mapping from display index to actual ability number
        ability_nums[num] = b_ptr->abilitynum;

        // Determine the appropriate colour
        if (has_ability(smith2_a_ptr, skill, b_ptr->abilitynum))
        {
            ability_present[num] = true;
            ability_valid[num] = true;
        }
        else
        {
            // require that the ability can be present on the object
            if (applicable_ability(b_ptr, smith_o_ptr))
            {
                ability_valid[num] = true;

                // add this flag to the dummy artefact under construction
                add_artefact_ability(skill, b_ptr->abilitynum);

                // require that the ability was successfully added
                if (has_ability(smith_a_ptr, skill, b_ptr->abilitynum))
                {
                    // Check whether it is a valid choice for creating (needs to
                    // be affordable and successful)
                    if (affordable(smith_o_ptr))
                    {
                        ability_affordable[num] = true;
                    }
                }

                // if the ability wasn't added properly (the item had too many),
                // then it is not valid after all
                else
                {
                    ability_valid[num] = false;
                }
            }
        }

        num++;
    }

    if (num == 0)
    {
        smith_ui_put_fitted(COL_SMT3, 3,
            smith_ui_column_width(COL_SMT3), TERM_L_DARK,
            "(No abilities can be added to this item.)");
        ui_menu_click_add_full_row(SMITH_CLICK_BACK, 3);
        Term_fresh();
        hide_cursor = true;
        (void)inkey();
        hide_cursor = false;
        mem_free(ability_present);
        mem_free(ability_valid);
        mem_free(ability_affordable);
        mem_free(ability_nums);
        return (-1);
    }

    if (*highlight < 1)
        *highlight = 1;
    if (*highlight > num)
        *highlight = num;
    top = smith_ui_configure_list_view(SMITH_SCROLL_ABILITY, num, *highlight,
        first_row, last_row);
    for (i = top; i < num && i < top + (last_row - first_row + 1); i++)
    {
        byte attr = ability_present[i]
            ? TERM_BLUE
            : (ability_valid[i]
                    ? (ability_affordable[i] ? TERM_WHITE : TERM_SLATE)
                    : TERM_L_DARK);
        cptr name = "Ability";
        int row = first_row + i - top;

        for (int j = 0; j < z_info->b_max; j++)
        {
            b_ptr = &b_info[j];
            if (b_ptr->name && b_ptr->skilltype == skill
                && b_ptr->abilitynum == ability_nums[i])
            {
                name = b_name + b_ptr->name;
                break;
            }
        }
        indexed_menu_entry_label(buf, sizeof(buf), i, name);
        smith_ui_put_menu_row(i + 1, COL_SMT3, row, attr, buf,
            *highlight == i + 1);
    }

    // add this ability to the dummy artefact under construction (use actual ability number)
    add_artefact_ability(skill, ability_nums[*highlight - 1]);

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14,
        first_row + MAX(0, MIN(last_row - first_row, *highlight - 1 - top)));

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    {
        int clicked_choice = 0;
        int click_action = UI_MENU_CLICK_PRIMARY;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if ((clicked_choice == SMITH_CLICK_BACK)
                || (click_action == UI_MENU_CLICK_SECONDARY))
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                {
                    mem_free(ability_present);
                    mem_free(ability_valid);
                    mem_free(ability_affordable);
                    mem_free(ability_nums);
                    return 0;
                }
                ch = ESCAPE;
            }
            else if (clicked_choice >= 1 && clicked_choice <= num)
            {
                if (!smith_ui_pointer_choice_confirms(
                        clicked_choice, click_action, highlight))
                {
                    mem_free(ability_present);
                    mem_free(ability_valid);
                    mem_free(ability_affordable);
                    mem_free(ability_nums);
                    return 0;
                }
                ch = '\r';
            }
        }
    }

    /* Choose by letter */
    if (sdl_menu_letters_enabled()
        && (ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        *highlight = (int)ch - 'a' + 1;
        ch = '\r';
    }

    /* Toggle the selected logical row from the saved baseline.  This avoids
     * committing the old preview after a one-step pointer activation. */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (ability_valid[*highlight - 1])
        {
            if (ability_present[*highlight - 1])
                remove_artefact_ability(skill, ability_nums[*highlight - 1]);
            else
                add_artefact_ability(skill, ability_nums[*highlight - 1]);

            // backup the new artefact
            artefact_copy(smith2_a_ptr, smith_a_ptr);

            mem_free(ability_present);
            mem_free(ability_valid);
            mem_free(ability_affordable);
            mem_free(ability_nums);
            return (*highlight);
        }
        else
        {
            bell("Invalid choice.");
        }
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        // remove any tentatively-added ability from the object
        if (!ability_present[*highlight - 1])
            remove_artefact_ability(skill, ability_nums[*highlight - 1]);

        // restore the backup artefact
        artefact_copy(smith_a_ptr, smith2_a_ptr);

        *highlight = -1;

        mem_free(ability_present);
        mem_free(ability_valid);
        mem_free(ability_affordable);
        mem_free(ability_nums);
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = num;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else if (*highlight == num)
            *highlight = 1;
    }

    mem_free(ability_present);
    mem_free(ability_valid);
    mem_free(ability_affordable);
    mem_free(ability_nums);
    return (0);
}

/*
 * Brings up a menu to select individual abilities of a given skill to
 * add to (or subtract from) an artefact.
 */
void artefact_ability_menu(int skill)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;

    /* Save screen */
    screen_save();

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = artefact_ability_menu_aux(skill, &highlight);

        if (choice >= 1)
        {
            // don't leave the menu
        }
        else if (choice == -1)
        {
            leave_menu = true;
        }
    }

    /* Load screen */
    ui_menu_click_clear();
    screen_load();
}

/*
 * Allows the player to choose a new name for an artefact.
 */
void rename_artefact(void)
{
    char tmp[20];
    char old_name[20];
    char o_desc[30];
    bool name_selected = false;
    int row = (smith_ui_last_desc_row >= 0) ? smith_ui_last_desc_row
                                            : smith_ui_content_bottom_row();
    int col = smith_ui_desc_col();

    // Clear the names
    tmp[0] = '\0';
    old_name[0] = '\0';

    // Clear object name
    Term_erase(0, row, smith_ui_term_wid());

    // Determine object name
    object_desc(o_desc, sizeof(o_desc), smith_o_ptr, false, -1);

    // Display shortened object name
    smith_ui_put_fitted(col, row, smith_ui_line_width(col), TERM_L_WHITE,
        o_desc);

    // use old name as a default
    SDL_strlcpy(tmp, smith2_a_ptr->name, sizeof(tmp));

    // save a copy too
    SDL_strlcpy(old_name, op_ptr->full_name, sizeof(old_name));

    /* Prompt for a new name */
    Term_gotoxy(col + strlen(o_desc) + 1, row);

    while (!name_selected)
    {
        if (askfor_name(tmp, sizeof(tmp)))
        {
            SDL_strlcpy(smith2_a_ptr->name, tmp, MAX_LEN_ART_NAME);
            p_ptr->redraw |= (PR_MISC);
        }
        else
        {
            SDL_strlcpy(smith2_a_ptr->name, old_name, MAX_LEN_ART_NAME);
            return;
        }

        if (tmp[0] != '\0')
            name_selected = true;
        else
            SDL_strlcpy(smith2_a_ptr->name, old_name, MAX_LEN_ART_NAME);
    }

    // retrieve a backup of the artefact (all the modifications were done to
    // this backup copy)
    artefact_copy(smith_a_ptr, smith2_a_ptr);
}

/*
 * Performs the interface and selection work for the 1st level artefact menu.
 */
int artefact_menu_aux(int* highlight)
{
    char ch;
    int i, num;
    char buf[80];
    int display_idx = 0;
    const int first_row = smith_ui_dense_row0();
    const int last_row = smith_ui_list_bottom_row(first_row, true);
    int top;

    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_outside_cancel_enabled(true);
    smith_ui_begin_touch_scroll_area(false);

    // clear the right of the screen
    smith_ui_wipe_active_panel(indexed_menu_prefix_col(COL_SMT2));
    smith_ui_put_section_header(COL_SMT2, MAX(0, smith_ui_dense_row0() - 1),
        "Artifice");

    num = MAX_CATS + S_MAX;
    if (*highlight < 1)
        *highlight = 1;
    if (*highlight > num)
        *highlight = num;
    top = smith_ui_configure_list_view(SMITH_SCROLL_ARTEFACT, num, *highlight,
        first_row, last_row);

    // display the categories for flags
    for (i = 0; i < MAX_CATS; i++)
    {
        int row;

        if (i < top || i >= top + (last_row - first_row + 1))
            continue;
        row = first_row + i - top;
        indexed_menu_entry_label(buf, sizeof(buf), i, smithing_flag_cats[i].desc);
        smith_ui_put_menu_row(i + 1, COL_SMT2, row, TERM_WHITE, buf,
            *highlight == i + 1);
    }

    // display the categories for abilities (skip Special abilities - S_SPC)
    for (i = 0; i < S_MAX; i++)
    {
        int logical_idx;
        int row;

        /* Skip Special abilities - they cannot be smithed onto items */
        if (i == S_SPC) continue;

        logical_idx = MAX_CATS + display_idx;
        if (logical_idx >= top
            && logical_idx < top + (last_row - first_row + 1))
        {
            row = first_row + logical_idx - top;
            indexed_menu_entry_label(buf, sizeof(buf), logical_idx,
                skill_names_full[i]);
            smith_ui_put_menu_row(logical_idx + 1, COL_SMT2, row, TERM_WHITE,
                buf, *highlight == logical_idx + 1);
        }
        display_idx++;
    }

    // Menu item for naming artefacts
    if (num - 1 >= top
        && num - 1 < top + (last_row - first_row + 1))
    {
        int row = first_row + num - 1 - top;

        indexed_menu_entry_label(buf, sizeof(buf), num - 1, "Name Artefact");
        smith_ui_put_menu_row(num, COL_SMT2, row, TERM_WHITE, buf,
            *highlight == num);
    }

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14,
        first_row + MAX(0, MIN(last_row - first_row, *highlight - 1 - top)));

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    {
        int clicked_choice = 0;
        int click_action = UI_MENU_CLICK_PRIMARY;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if ((clicked_choice == SMITH_CLICK_BACK)
                || (click_action == UI_MENU_CLICK_SECONDARY))
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    return 0;
                ch = ESCAPE;
            }
            else if (clicked_choice >= 1 && clicked_choice <= num)
            {
                if (!smith_ui_pointer_choice_confirms(
                        clicked_choice, click_action, highlight))
                {
                    return 0;
                }
                ch = '\r';
            }
        }
    }

    /* Choose by letter */
    if (sdl_menu_letters_enabled()
        && (ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        *highlight = (int)ch - 'a' + 1;

        return (*highlight);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        return (*highlight);
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        *highlight = -1;
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = num;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else if (*highlight == num)
            *highlight = 1;
    }

    return (0);
}

/*
 * Brings up a menu for making a base item into an artefact,
 * by adding flags of various types.
 */
void artefact_menu(void)
{
    int choice = -1;
    int highlight = 1;

    char buf[36];
    bool leave_menu = false;

    log_info("Player opened artifact creation menu");

    /* Save screen */
    screen_save();

    if (!smith_o_ptr->name1)
    {
        log_debug("Initializing new artifact creation");
        // wipe the existing artefact (and its backup)
        artefact_wipe(smith_a_name);
        artefact_wipe(smith2_a_name);

        // add 'ignore all'
        smith2_a_ptr->flags3 |= (TR3_IGNORE_MASK);

        // change the SV for rings and amulets when they start to get made into
        // artefacts
        if (smith_o_ptr->tval == TV_RING)
        {
            create_base_object(TV_RING, SV_RING_SELF_MADE);
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;
        }
        if (smith_o_ptr->tval == TV_AMULET)
        {
            create_base_object(TV_AMULET, SV_AMULET_SELF_MADE);
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;
            smith2_o_ptr->pd = 1;
        }
    }

    // set the backup artefact name to the player character's name
    if (strlen(smith2_a_ptr->name) == 0)
    {
        sprintf(buf, "of %s", op_ptr->full_name);
        SDL_strlcpy(smith2_a_ptr->name, buf, MAX_LEN_ART_NAME);
    }

    // prepare the artefact and object for modification
    prepare_artefact();

    /* Number of skill categories displayed (S_MAX minus Special abilities) */
    int num_skills = S_MAX - 1;

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = artefact_menu_aux(&highlight);

        if (choice == MAX_CATS + num_skills + 1)
        {
            rename_artefact();
        }
        else if (choice >= MAX_CATS + 1)
        {
            artefact_ability_menu(choice - MAX_CATS - 1);
        }
        else if (choice >= 1)
        {
            artefact_flag_menu(choice);
        }
        else if (choice == -1)
        {
            leave_menu = true;
        }
    }

    /* Load screen */
    ui_menu_click_clear();
    screen_load();

    return;
}

/*
 * Performs the interface and selection work for the melting menu.
 */
int melt_menu_aux(int* highlight)
{
    char ch;
    int i;
    int num = 0;
    object_type* o_ptr;
    u32b f1, f2, f3;
    char desc[80];
    char buf[80];
    const int first_row = smith_ui_dense_row0();
    const int last_row = smith_ui_list_bottom_row(first_row, false);
    int top;

    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_outside_cancel_enabled(true);
    smith_ui_begin_touch_scroll_area(false);

    // clear the right of the screen
    smith_ui_wipe_active_panel(indexed_menu_prefix_col(COL_SMT2));
    smith_ui_put_section_header(COL_SMT2, MAX(0, smith_ui_dense_row0() - 1),
        "Melt");

    // clear bottom of the screen
    wipe_object_description();

    for (i = 0; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        object_flags(o_ptr, &f1, &f2, &f3);

        /* ignore metal items that carry the "can't melt" tag */
        if ((f3 & (TR3_MITHRIL | TR3_STAR_IRON)) && !(o_ptr->ident & IDENT_CANT_MELT))
            num++;
    }

    if (num == 0)
    {
        smith_ui_put_fitted(COL_SMT2, 3,
            smith_ui_column_width(COL_SMT2), TERM_L_DARK,
            "(No carried metal items can be melted.)");
        ui_menu_click_add_full_row(SMITH_CLICK_BACK, 3);
        Term_fresh();
        hide_cursor = true;
        (void)inkey();
        hide_cursor = false;
        return (-1);
    }

    if (*highlight < 1)
        *highlight = 1;
    if (*highlight > num)
        *highlight = num;
    top = smith_ui_configure_list_view(SMITH_SCROLL_MELT, num, *highlight,
        first_row, last_row);

    num = 0;
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        int row;

        o_ptr = &inventory[i];
        object_flags(o_ptr, &f1, &f2, &f3);
        if (!((f3 & (TR3_MITHRIL | TR3_STAR_IRON))
                && !(o_ptr->ident & IDENT_CANT_MELT)))
        {
            continue;
        }

        if (num >= top && num < top + (last_row - first_row + 1))
        {
            row = first_row + num - top;
            object_desc(desc, 80, o_ptr, false, 2);
            indexed_menu_entry_label(buf, sizeof(buf), num, desc);
            smith_ui_put_menu_row(num + 1, COL_SMT2, row, TERM_WHITE, buf,
                *highlight == num + 1);

            if (smith_ui_weight_col() > 0)
            {
                strnfmt(buf, 80, "%2d.%d lb", o_ptr->weight / 10,
                    o_ptr->weight % 10);
                smith_ui_put_fitted(smith_ui_weight_col(), row,
                    smith_ui_line_width(smith_ui_weight_col()),
                    (*highlight == num + 1)
                        ? smith_ui_selected_attr(TERM_WHITE)
                        : TERM_WHITE,
                    buf);
            }
        }
        num++;
    }

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(14,
        first_row + MAX(0, MIN(last_row - first_row, *highlight - 1 - top)));

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    {
        int clicked_choice = 0;
        int click_action = UI_MENU_CLICK_PRIMARY;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if ((clicked_choice == SMITH_CLICK_BACK)
                || (click_action == UI_MENU_CLICK_SECONDARY))
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    return 0;
                ch = ESCAPE;
            }
            else if (clicked_choice >= 1 && clicked_choice <= num)
            {
                if (!smith_ui_pointer_choice_confirms(
                        clicked_choice, click_action, highlight))
                {
                    return 0;
                }
                ch = '\r';
            }
        }
    }

    // choose an option by letter
    if (sdl_menu_letters_enabled()
        && (ch >= 'a') && (ch <= (char)'a' + num - 1))
    {
        int old_highlight = *highlight;

        *highlight = (int)ch - 'a' + 1;

        // move the light blue highlight
        move_displayed_highlight(
            old_highlight, TERM_WHITE, *highlight, COL_SMT2);

        return (*highlight);
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        return (*highlight);
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = num;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < num)
            (*highlight)++;
        else if (*highlight == num)
            *highlight = 1;
    }

    /* Exit */
    if ((ch == '4') || (ch == ESCAPE))
    {
        return (-1);
    }

    return (0);
}

/*
 * Produces the menu for melting down mithril and star-iron items into their metal pieces.
 */
void melt_menu(void)
{
    int choice = -1;
    int highlight = 1;

    bool leave_menu = false;

    /* This submenu has no third column, so let its list use the full width. */
    bool saved_full_width = smith_ui_secondary_full_width;
    smith_ui_secondary_full_width = true;

    /* Save screen */
    screen_save();

    /* Process Events until menu is abandoned */
    while (!leave_menu)
    {
        choice = melt_menu_aux(&highlight);

        if (choice >= 1)
        {
            if (melt_metal_item(choice))
            {
                leave_menu = true;
            }
        }
        else if (choice == -1)
        {
            leave_menu = true;
        }
    }

    /* Load screen */
    ui_menu_click_clear();
    screen_load();

    smith_ui_secondary_full_width = saved_full_width;
}

static bool smith_item_tester_hook_reforge_target(const object_type* o_ptr)
{
    return object_can_repair_damage(o_ptr)
        || object_can_preview_reforge_prefix(o_ptr);
}

static void smith_root_build_entries(bool valid[SMT_MENU_MAX],
    byte menu_attr[SMT_MENU_MAX], char labels[SMT_MENU_MAX][32])
{
    bool at_forge = cave_forge_bold(p_ptr->py, p_ptr->px);
    int uses = at_forge ? forge_uses(p_ptr->py, p_ptr->px) : 0;
    bool has_item = (smith_o_ptr->tval != 0);
    int reforge_target = find_reforge_target_item();
    byte valid_attr;

    SDL_strlcpy(labels[SMT_MENU_CREATE - 1], "Base Item", 32);
    SDL_strlcpy(labels[SMT_MENU_ENCHANT - 1], "Enchant", 32);
    SDL_strlcpy(labels[SMT_MENU_ARTEFACT - 1], "Artifice", 32);
    SDL_strlcpy(labels[SMT_MENU_NUMBERS - 1], "Numbers", 32);
    SDL_strlcpy(labels[SMT_MENU_MELT - 1], "Melt", 32);
    SDL_strlcpy(labels[SMT_MENU_REPAIR - 1], "Reforge", 32);
    SDL_strlcpy(labels[SMT_MENU_ACCEPT - 1],
        (p_ptr->smithing_leftover == 0) ? "Accept" : "Resume", 32);

    valid[SMT_MENU_CREATE - 1] = true;
    valid[SMT_MENU_ENCHANT - 1] = (!smith_o_ptr->name1)
        && (!enchant_then_numbers) && has_item
        && (smith_o_ptr->tval != TV_HORN)
        && !((smith_o_ptr->tval == TV_DIGGING)
            && (smith_o_ptr->sval == SV_SHOVEL));
    valid[SMT_MENU_ARTEFACT - 1] = (!object_has_ego(smith_o_ptr)) && has_item
        && (smith_o_ptr->tval != TV_HORN)
        && (p_ptr->self_made_arts
            < z_info->art_self_made_max - z_info->art_rand_max - 2);
    valid[SMT_MENU_NUMBERS - 1] = has_item;
    valid[SMT_MENU_MELT - 1] = meltable_metal_items_carried() && at_forge;
    valid[SMT_MENU_REPAIR - 1] = (reforge_target >= 0);
    valid[SMT_MENU_ACCEPT - 1] = affordable(smith_o_ptr) && at_forge
        && (uses > 0);

    valid_attr = (p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH]
                     || p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH]
                     || p_ptr->active_ability[S_SMT][SMT_JEWELLER])
        ? TERM_WHITE
        : TERM_RED;
    menu_attr[SMT_MENU_CREATE - 1]
        = valid[SMT_MENU_CREATE - 1] ? valid_attr : TERM_L_DARK;

    valid_attr = p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT] ? TERM_WHITE
                                                               : TERM_RED;
    menu_attr[SMT_MENU_ENCHANT - 1]
        = valid[SMT_MENU_ENCHANT - 1] ? valid_attr : TERM_L_DARK;

    valid_attr = p_ptr->active_ability[S_SMT][SMT_ARTEFACT] ? TERM_WHITE
                                                            : TERM_RED;
    menu_attr[SMT_MENU_ARTEFACT - 1]
        = valid[SMT_MENU_ARTEFACT - 1] ? valid_attr : TERM_L_DARK;

    menu_attr[SMT_MENU_NUMBERS - 1]
        = valid[SMT_MENU_NUMBERS - 1] ? TERM_WHITE : TERM_L_DARK;
    menu_attr[SMT_MENU_MELT - 1]
        = valid[SMT_MENU_MELT - 1] ? TERM_WHITE : TERM_L_DARK;

    valid_attr = (p_ptr->active_ability[S_SMT][SMT_REPAIR] && at_forge
                     && (uses > 0))
        ? TERM_WHITE
        : TERM_RED;
    menu_attr[SMT_MENU_REPAIR - 1]
        = valid[SMT_MENU_REPAIR - 1] ? valid_attr : TERM_L_DARK;

    menu_attr[SMT_MENU_ACCEPT - 1]
        = valid[SMT_MENU_ACCEPT - 1] ? TERM_WHITE : TERM_L_DARK;
}

static int smith_root_detail_col(void)
{
    int term_wid = smith_ui_term_wid();
    int cost_col = smith_ui_cost_col();
    int detail_col;

    if (smith_ui_portrait_layout())
        return -1;

    if (term_wid >= 76)
        detail_col = 34;
    else if (term_wid >= 64)
        detail_col = 30;
    else
        return -1;

    if (detail_col + 16 >= cost_col)
        return -1;

    return detail_col;
}

static int smith_root_list_width(int detail_col)
{
    int prefix_col = indexed_menu_prefix_col(COL_SMT1);
    int width = COL_SMT2 - prefix_col - 1;

    (void)detail_col;

    if (smith_ui_portrait_layout())
        width = smith_ui_line_width(prefix_col);

    if (width < 1)
        width = smith_ui_line_width(prefix_col);

    return smith_ui_safe_width(prefix_col, width);
}

static cptr smith_root_detail_text(int choice)
{
    switch (choice)
    {
    case SMT_MENU_CREATE:
        return "Choose the first shape of the work: blade, mail, light, jewel, horn, bow, or tool.";
    case SMT_MENU_ENCHANT:
        return "Lay a named craft upon the base item. Enchantments and Artifice claim the same place in the design.";
    case SMT_MENU_ARTEFACT:
        return "Name a work of your own making and bind its powers directly into the finished piece.";
    case SMT_MENU_NUMBERS:
        return "Tune weight, accuracy, evasion, protection, damage, and alloyed metal before the final hammer-fall.";
    case SMT_MENU_MELT:
        return "Break mithril or star-iron gear back into raw pieces for the next work of the forge.";
    case SMT_MENU_REPAIR:
        return "Repair damaged gear or hammer a prefix onto a found item. Prefix reforging uses one and a half times the difficulty gained.";
    case SMT_MENU_ACCEPT:
        return "Commit the design to the forge and spend the listed resources to finish the work.";
    default:
        return "";
    }
}

static void smith_root_note_for_choice(int choice, bool valid, char* buf,
    size_t buflen)
{
    bool at_forge = cave_forge_bold(p_ptr->py, p_ptr->px);
    int uses = at_forge ? forge_uses(p_ptr->py, p_ptr->px) : 0;

    if (!buf || buflen == 0)
        return;

    buf[0] = '\0';

    switch (choice)
    {
    case SMT_MENU_CREATE:
        if (!(p_ptr->active_ability[S_SMT][SMT_WEAPONSMITH]
                || p_ptr->active_ability[S_SMT][SMT_ARMOURSMITH]
                || p_ptr->active_ability[S_SMT][SMT_JEWELLER]))
        {
            SDL_strlcpy(buf, "No smithing craft is learned yet.", buflen);
        }
        break;
    case SMT_MENU_ENCHANT:
        if (!smith_o_ptr->tval)
            SDL_strlcpy(buf, "Select a base item first.", buflen);
        else if (smith_o_ptr->name1)
            SDL_strlcpy(buf, "Artifice already claims this work.", buflen);
        else if (enchant_then_numbers)
            SDL_strlcpy(buf, "The Numbers menu locked this enchantment pass.", buflen);
        else if (smith_o_ptr->tval == TV_HORN)
            SDL_strlcpy(buf, "Horns cannot take this enchantment path.", buflen);
        else if ((smith_o_ptr->tval == TV_DIGGING)
            && (smith_o_ptr->sval == SV_SHOVEL))
        {
            SDL_strlcpy(buf, "Shovels cannot be enchanted here.", buflen);
        }
        else if (!p_ptr->active_ability[S_SMT][SMT_ENCHANTMENT])
            SDL_strlcpy(buf, "Requires Enchantment.", buflen);
        break;
    case SMT_MENU_ARTEFACT:
        if (!smith_o_ptr->tval)
            SDL_strlcpy(buf, "Select a base item first.", buflen);
        else if (object_has_ego(smith_o_ptr))
            SDL_strlcpy(buf, "Remove the enchantment choice first.", buflen);
        else if (smith_o_ptr->tval == TV_HORN)
            SDL_strlcpy(buf, "Horns cannot be made into custom artefacts.", buflen);
        else if (!p_ptr->active_ability[S_SMT][SMT_ARTEFACT])
            SDL_strlcpy(buf, "Requires Artifice.", buflen);
        else if (!valid)
            SDL_strlcpy(buf, "Self-made artefact limit reached.", buflen);
        break;
    case SMT_MENU_NUMBERS:
        if (!smith_o_ptr->tval)
            SDL_strlcpy(buf, "Select a base item first.", buflen);
        break;
    case SMT_MENU_MELT:
        if (!at_forge)
            SDL_strlcpy(buf, "Requires a forge.", buflen);
        else if (!meltable_metal_items_carried())
            SDL_strlcpy(buf, "Carry mithril or star-iron gear to melt.", buflen);
        break;
    case SMT_MENU_REPAIR:
        if (find_reforge_target_item() < 0)
            SDL_strlcpy(buf, "Carry damaged or prefixable gear.", buflen);
        else if (!at_forge)
            SDL_strlcpy(buf, "Preview only away from a forge.", buflen);
        else if (uses <= 0)
            SDL_strlcpy(buf, "This forge is exhausted.", buflen);
        else if (!p_ptr->active_ability[S_SMT][SMT_REPAIR])
            SDL_strlcpy(buf, "Requires Reforging.", buflen);
        break;
    case SMT_MENU_ACCEPT:
        if (!smith_o_ptr->tval)
            SDL_strlcpy(buf, "Select a base item first.", buflen);
        else if (!at_forge)
            SDL_strlcpy(buf, "Requires a forge.", buflen);
        else if (uses <= 0)
            SDL_strlcpy(buf, "This forge is exhausted.", buflen);
        else if (object_has_evil_alignment(smith_o_ptr))
            SDL_strlcpy(buf, "This work is too fell to make.", buflen);
        else if (!valid)
            SDL_strlcpy(buf, "The design is beyond your current means.", buflen);
        break;
    }
}

static int smith_root_draw_header(void)
{
    char title[80];
    char status[160];
    int skill = p_ptr->skill_use[S_SMT];
    int bonus = forge_bonus(p_ptr->py, p_ptr->px);
    bool at_forge = cave_forge_bold(p_ptr->py, p_ptr->px);
    int uses = at_forge ? forge_uses(p_ptr->py, p_ptr->px) : 0;
    int col = indexed_menu_prefix_col(COL_SMT1);
    int width = smith_ui_line_width(col);
    int row = 0;
    int used;

    SDL_strlcpy(title, "Smithing Esc - Work of the Forge", sizeof(title));
    if (smith_ui_portrait_layout())
        used = smith_ui_put_wrapped(col, row, width,
            MAX(1, smith_ui_content_bottom_row() - row + 1),
            TERM_L_WHITE + TERM_SHADE, title);
    else
    {
        smith_ui_put_fitted(col, row, width, TERM_L_WHITE + TERM_SHADE, title);
        used = 1;
    }
    if (used < 1)
        used = 1;
    for (int y = row; y < row + used; y++)
        ui_menu_click_add_full_row(SMITH_CLICK_BACK, y);
    smith_ui_add_back_click_target(col, row, title);
    row += used;

    if (at_forge)
    {
        strnfmt(status, sizeof(status),
            "Forge: %d use%s   Smithing: %d%s%d   Metal: %d.%d mithril, %d.%d star-iron",
            uses, (uses == 1) ? "" : "s", skill, (bonus >= 0) ? "+" : "",
            bonus, mithril_carried() / 10, mithril_carried() % 10,
            star_iron_carried() / 10, star_iron_carried() % 10);
    }
    else
    {
        strnfmt(status, sizeof(status),
            "Exploration preview   Smithing: %d   Metal: %d.%d mithril, %d.%d star-iron",
            skill, mithril_carried() / 10, mithril_carried() % 10,
            star_iron_carried() / 10, star_iron_carried() % 10);
    }

    if (smith_ui_portrait_layout())
    {
        used = smith_ui_put_wrapped(col, row, width,
            MAX(1, smith_ui_content_bottom_row() - row + 1), TERM_SLATE,
            status);
        row += MAX(1, used);
    }
    else
    {
        smith_ui_put_fitted(col, row, width, TERM_SLATE, status);
        row++;
    }

    return row;
}

static int smith_root_draw_chrome(int detail_col, int list_w, int header_row)
{
    int term_wid = smith_ui_term_wid();
    int prefix_col = indexed_menu_prefix_col(COL_SMT1);
    int divider_row = header_row + 1;

    smith_ui_put_fitted(prefix_col, header_row, list_w, TERM_SLATE, "Work");
    if (!smith_ui_portrait_layout() && detail_col > 0)
    {
        int width = smith_ui_cost_col() - detail_col - 2;
        smith_ui_put_fitted(detail_col, header_row, width, TERM_SLATE, "Lore");
    }
    if (!smith_ui_portrait_layout())
    {
        smith_ui_put_fitted(smith_ui_cost_col(), header_row,
            smith_ui_line_width(smith_ui_cost_col()), TERM_SLATE, "Measure");
    }

    if (divider_row <= smith_ui_content_bottom_row())
    {
        for (int x = 0; x < term_wid; x++)
            Term_putch(x, divider_row, TERM_L_DARK, '=');
    }

    return divider_row + 1;
}

static void smith_root_draw_action(int choice, int row, int list_w,
    bool selected, byte base_attr, cptr label)
{
    char prefix[8];
    int prefix_col = indexed_menu_prefix_col(COL_SMT1);
    int prefix_w = indexed_menu_letters_enabled() ? 3 : 2;
    int label_col = prefix_col + prefix_w;
    int label_w = list_w - (label_col - prefix_col);
    byte attr = selected ? smith_ui_selected_attr(base_attr) : base_attr;

    if (row < 0 || row > smith_ui_content_bottom_row())
        return;

    if (selected)
        indexed_menu_focus_prefix(prefix, sizeof(prefix), choice - 1);
    else
        indexed_menu_normal_prefix(prefix, sizeof(prefix), choice - 1);

    Term_erase(prefix_col, row, list_w);
    if (selected)
        smith_ui_fill_row(prefix_col, row, list_w, attr);

    /* Preserve the highlight fill across the gaps and trailing spaces by drawing
     * the prefix and label without erasing when the row is selected. */
    smith_ui_draw_fitted(prefix_col, row, prefix_w, attr, prefix, !selected);
    smith_ui_draw_fitted(label_col, row, label_w, attr, label, !selected);

    ui_menu_click_add(choice, prefix_col, row, list_w);
}

static void smith_root_draw_detail(int choice, int detail_col, int list_w,
    int action_row, bool valid, cptr label)
{
    char note[120];
    int col = detail_col;
    int width;
    int row = action_row;
    int max_lines;

    if (detail_col <= 0)
    {
        col = indexed_menu_prefix_col(COL_SMT1);
        if (smith_ui_portrait_layout())
        {
            int divider_row = action_row + SMT_MENU_MAX;

            smith_ui_draw_horizontal_divider(divider_row);
            smith_ui_put_fitted(col, divider_row + 1,
                smith_ui_line_width(col), TERM_SLATE, "Lore");
            row = divider_row + 2;
            width = smith_ui_line_width(col);
        }
        else
        {
            row = 12;
            width = smith_ui_cost_col() - col - 2;
            if (width < 20)
                width = list_w;
        }
    }
    else
    {
        width = smith_ui_cost_col() - detail_col - 2;
    }

    width = smith_ui_safe_width(col, width);
    if (width <= 0)
        return;

    max_lines = (detail_col > 0) ? 5 : 4;

    smith_ui_put_fitted(col, row, width, TERM_L_WHITE, label);
    row += 2;
    row += smith_ui_put_wrapped(col, row, width, max_lines, TERM_SLATE,
        smith_root_detail_text(choice));

    smith_root_note_for_choice(choice, valid, note, sizeof(note));
    if (note[0] && row < smith_ui_content_bottom_row())
    {
        byte note_attr = valid ? TERM_L_DARK : TERM_RED;

        row++;
        smith_ui_put_wrapped(col, row, width, 2, note_attr, note);
    }
}

static int smith_root_draw(int highlight, const bool valid[SMT_MENU_MAX],
    const byte menu_attr[SMT_MENU_MAX], char labels[SMT_MENU_MAX][32])
{
    int detail_col = smith_root_detail_col();
    int list_w = smith_root_list_width(detail_col);
    int header_row;
    int action_row;

    Term_clear();
    smith_ui_reset_description_state();

    header_row = smith_root_draw_header();
    action_row = smith_root_draw_chrome(detail_col, list_w, header_row);

    for (int i = 0; i < SMT_MENU_MAX; i++)
    {
        smith_root_draw_action(i + 1, action_row + i, list_w,
            highlight == i + 1, menu_attr[i], labels[i]);
    }

    if (highlight >= 1 && highlight <= SMT_MENU_MAX)
        smith_root_draw_detail(highlight, detail_col, list_w, action_row,
            valid[highlight - 1], labels[highlight - 1]);

    return action_row;
}

static void smithing_redraw_root_after_item_picker(void)
{
    bool valid[SMT_MENU_MAX];
    byte menu_attr[SMT_MENU_MAX];
    char labels[SMT_MENU_MAX][32];
    int highlight = SMT_MENU_REPAIR;

    ui_menu_click_clear();
    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_outside_cancel_enabled(true);
    smith_ui_begin_touch_scroll_area(true);

    smith_root_build_entries(valid, menu_attr, labels);
    smith_root_draw(highlight, valid, menu_attr, labels);
    prt_object_difficulty();
    prt_object_description();
    Term_fresh();
}

static bool smith_reforge_item(void)
{
    int slot = -1;
    int prefix_idx = 0;
    char old_name[80];
    char new_name[80];
    object_type smith_backup;
    object_type smith2_backup;
    smith_alloy_state alloy_backup = smith_alloy;
    smith_alloy_state alloy2_backup = smith2_alloy;

    smith_ui_wipe_active_panel(indexed_menu_prefix_col(COL_SMT2));
    wipe_object_description();
    Term_fresh();

    item_tester_hook = smith_item_tester_hook_reforge_target;
    if (!open_inventory_item_select_menu(USE_EQUIP | USE_INVEN,
            "Reforge which item? ",
            "You have nothing to repair or reforge.", &slot))
    {
        item_tester_hook = NULL;
        smithing_redraw_root_after_item_picker();
        return false;
    }
    item_tester_hook = NULL;
    smithing_redraw_root_after_item_picker();

    if (slot < 0)
        return false;

    object_copy(&smith_backup, smith_o_ptr);
    object_copy(&smith2_backup, smith2_o_ptr);

    if (object_can_repair_damage(&inventory[slot]))
    {
        if (!cave_forge_bold(p_ptr->py, p_ptr->px))
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell("You can only reforge items at a forge.");
            return false;
        }

        if (forge_uses(p_ptr->py, p_ptr->px) <= 0)
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell("This forge has no resources left.");
            return false;
        }

        if (!p_ptr->active_ability[S_SMT][SMT_REPAIR])
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell("You do not know how to reforge gear.");
            return false;
        }

        if (!repair_damaged_item(slot))
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell("You cannot repair that item.");
            return false;
        }

        cave_feat[p_ptr->py][p_ptr->px] -= 1;
        lite_spot(p_ptr->py, p_ptr->px);

        object_desc(new_name, sizeof(new_name), &inventory[slot], true, 0);
        msg_format("You repair %s.", new_name);
    }
    else
    {
        reforge_preview_type preview;

        if (!object_can_preview_reforge_prefix(&inventory[slot]))
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell("You cannot reforge that item.");
            return false;
        }

        prefix_idx = reforge_prefix_menu(&inventory[slot]);
        if (!prefix_idx)
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            return false;
        }

        if (!reforge_preview_build(&inventory[slot], prefix_idx, &preview)
            || !preview.affordable)
        {
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell("You cannot afford that reforge.");
            return false;
        }

        object_desc(old_name, sizeof(old_name), &inventory[slot], true, 0);
        object_set_ego_prefix(&inventory[slot], prefix_idx);
        if (!object_apply_ego_affix(&inventory[slot], prefix_idx, true))
        {
            object_set_ego_prefix(&inventory[slot], 0);
            object_copy(smith_o_ptr, &smith_backup);
            object_copy(smith2_o_ptr, &smith2_backup);
            smith_alloy = alloy_backup;
            smith2_alloy = alloy2_backup;
            bell("You cannot reforge that item.");
            return false;
        }

        pay_smithing_cost_struct(&preview.cost);
        inventory[slot].unused1 = 2;
        object_aware(&inventory[slot]);
        object_known(&inventory[slot]);
        object_desc(new_name, sizeof(new_name), &inventory[slot], true, 0);
        msg_format("You reforge %s into %s.", old_name, new_name);
        p_ptr->window |= (PW_INVEN | PW_EQUIP);
    }

    object_copy(smith_o_ptr, &smith_backup);
    object_copy(smith2_o_ptr, &smith2_backup);
    smith_alloy = alloy_backup;
    smith2_alloy = alloy2_backup;

    p_ptr->redraw |= PR_BASIC;
    return true;
}

/*
 * Performs the interface and selection work for the smithing screen.
 */
int smithing_menu_aux(int* highlight)
{
    char ch;
    bool valid[SMT_MENU_MAX];
    byte menu_attr[SMT_MENU_MAX];
    char labels[SMT_MENU_MAX][32];
    int action_row;

    ui_menu_click_begin();
    ui_menu_click_set_hover_enabled(true);
    ui_menu_click_set_outside_cancel_enabled(true);
    smith_ui_begin_touch_scroll_area(true);

    smith_root_build_entries(valid, menu_attr, labels);
    action_row = smith_root_draw(*highlight, valid, menu_attr, labels);

    // display the object difficulty
    prt_object_difficulty();

    // display the object description
    prt_object_description();

    /* Flush the prompt */
    Term_fresh();

    /* Place cursor at current choice */
    Term_gotoxy(indexed_menu_prefix_col(COL_SMT1),
        action_row + *highlight - 1);

    /* Get key (while allowing menu commands) */
    hide_cursor = true;
    ch = inkey();
    hide_cursor = false;

    {
        int clicked_choice = 0;
        int click_action = UI_MENU_CLICK_PRIMARY;

        if (ui_menu_click_take_action(&clicked_choice, &click_action))
        {
            if ((clicked_choice == SMITH_CLICK_BACK)
                || (click_action == UI_MENU_CLICK_SECONDARY))
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    return 0;
                ch = ESCAPE;
            }
            else if (clicked_choice >= 1 && clicked_choice <= SMT_MENU_MAX)
            {
                if (!smith_ui_pointer_choice_confirms(
                        clicked_choice, click_action, highlight))
                {
                    return 0;
                }
                ch = '\r';
            }
        }
    }

    // choose an option by letter
    if (sdl_menu_letters_enabled()
        && (ch >= 'a') && (ch <= (char)'a' + SMT_MENU_MAX - 1))
    {
        *highlight = (int)ch - 'a' + 1;

        if (valid[*highlight - 1])
            return (*highlight);
        else
            bell("Invalid choice.");
    }

    /* Choose current  */
    if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6'))
    {
        if (valid[*highlight - 1])
            return (*highlight);
        else
            bell("Invalid choice.");
    }

    /* Prev item */
    if (ch == '8')
    {
        if (*highlight > 1)
            (*highlight)--;
        else if (*highlight == 1)
            *highlight = SMT_MENU_MAX;
    }

    /* Next item */
    if (ch == '2')
    {
        if (*highlight < SMT_MENU_MAX)
            (*highlight)++;
        else if (*highlight == SMT_MENU_MAX)
            *highlight = 1;
    }

    /* Leave menu */
    if ((ch == ESCAPE) || (ch == '4'))
    {
        return (-1);
    }

    return (0);
}

/*
 * Brings up a screen for making new items (only works at a forge).
 * Leads to many submenus which help to determine the item's attributes.
 */
void do_cmd_smithing_screen(void)
{
    int actiontype = -1;
    int highlight = 1;
    bool leave_menu = false;
    bool create = false;
    bool death_view = death_spectator_active();
    int old_smithing = p_ptr->smithing;
    int old_smithing_leftover = p_ptr->smithing_leftover;

    // if (!cave_forge_bold(p_ptr->py, p_ptr->px))
    //{
    //	msg_print("You can only create items at a forge.");
    //	return;
    //}

    if (cave_forge_bold(p_ptr->py, p_ptr->px)
        && forge_uses(p_ptr->py, p_ptr->px) == 0)
    {
        msg_print("The resources of this forge are exhausted.");
        msg_print(
            "You will be able to browse the options but not make new things.");
    }

    /* Save screen */
    screen_save();
    screen_push_supporting_panes_hidden();
    sdl_push_terminal_menu_scale();

    /* Clear screen */
    Term_clear();
    smith_ui_reset_description_state();

    // Hack: flag that we are in the middle of smithing
    p_ptr->smithing = 1;

    // deal with previous interruptions
    if (p_ptr->smithing_leftover > 0)
    {
        // default to 'resume' if an item is already in progress
        highlight = SMT_MENU_ACCEPT;

        // and backup the smithing item
        object_copy(smith2_o_ptr, smith_o_ptr);
        smith2_alloy = smith_alloy;
    }

    // otherwise wipe the smithing item
    else
    {
        object_wipe(smith_o_ptr);
        smith_clear_alloy_state(&smith_alloy);
    }

    /* Process Events until "Return to Game" is selected */
    while (!leave_menu)
    {
        actiontype = smithing_menu_aux(&highlight);

        // if an action has been selected...
        switch (actiontype)
        {
        case SMT_MENU_CREATE:
        {
            // this is not a resumption of smithing an item
            if (!death_view)
                p_ptr->smithing_leftover = 0;

            create_tval_menu();

            // backup the smithing object
            object_copy(smith2_o_ptr, smith_o_ptr);
            smith2_alloy = smith_alloy;

            break;
        }
        case SMT_MENU_ENCHANT:
        {
            if (smith_o_ptr->tval)
            {
                // this is not a resumption of smithing an item
                if (!death_view)
                    p_ptr->smithing_leftover = 0;

                if (!enchant_menu())
                {
                    // restore the smithing object
                    object_copy(smith_o_ptr, smith2_o_ptr);
                    smith_alloy = smith2_alloy;
                }
            }
            else
            {
                bell("You must first select a base item.");
            }

            break;
        }
        case SMT_MENU_ARTEFACT:
        {
            if (smith_o_ptr->tval)
            {
                // this is not a resumption of smithing an item
                if (!death_view)
                    p_ptr->smithing_leftover = 0;

                artefact_menu();
            }
            else
            {
                bell("You must first select a base item.");
            }

            break;
        }
        case SMT_MENU_NUMBERS:
        {
            if (smith_o_ptr->tval)
            {
                // this is not a resumption of smithing an item
                if (!death_view)
                    p_ptr->smithing_leftover = 0;

                numbers_menu();

                // backup the smithing object
                object_copy(smith2_o_ptr, smith_o_ptr);
                smith2_alloy = smith_alloy;
            }
            else
            {
                bell("You must first select a base item.");
            }

            break;
        }
        case SMT_MENU_MELT:
        {
            if (death_view)
            {
                msg_print("You cannot do that during this final look.");
                break;
            }

            if (meltable_metal_items_carried())
            {
                // this is not a resumption of smithing an item
                p_ptr->smithing_leftover = 0;

                melt_menu();
            }
            else
            {
                bell("You don't have any mithril or star-iron items.");
            }

            break;
        }
        case SMT_MENU_REPAIR:
        {
            if (death_view)
            {
                msg_print("You cannot do that during this final look.");
                break;
            }

            smith_reforge_item();
            break;
        }
        case SMT_MENU_ACCEPT:
        {
            if (death_view)
            {
                msg_print("You cannot do that during this final look.");
                break;
            }

            if (smithing_cost.drain > 0)
            {
                char buf[80];

                sprintf(buf,
                    "This will drain your smithing skill by %d points. "
                    "Proceed? ",
                    smithing_cost.drain);
                if (!get_check(buf))
                    break;
            }

            create = true;
            leave_menu = true;
            break;
        }
        case -1:
        {
            leave_menu = true;
            break;
        }
        }
    }

    if (create)
    {
        int turn_multiplier = 10;

        if (p_ptr->active_ability[S_SMT][SMT_EXPERTISE])
        {
            turn_multiplier /= 2;
        }

        // Display a message
        msg_print("You begin your work.");

        // add the details to the artefact type if applicable
        if (smith_o_ptr->name1)
            add_artefact_details();

        /* Cancel stealth mode */
        if (p_ptr->stealth_mode && pixel_monster_status_icons)
            p_ptr->redraw |= (PR_MAP);
        p_ptr->stealth_mode = false;

        // Allow the resumption of interrupted smithing
        if (p_ptr->smithing_leftover > 0)
        {
            p_ptr->smithing = p_ptr->smithing_leftover;
        }
        else
        {
            // Set smithing counter
            p_ptr->smithing
                = MAX(10, object_difficulty(smith_o_ptr) * turn_multiplier);

            // Also set the smithing leftover counter (to allow you to resume if
            // interrupted)
            p_ptr->smithing_leftover = p_ptr->smithing;
        }

        /* Recalculate bonuses */
        p_ptr->update |= (PU_BONUS);

        /* Redraw the state */
        p_ptr->redraw |= (PR_STATE);

        /* Handle stuff */
        handle_stuff();

        /* Refresh */
        Term_fresh();
    }

    else if (death_view)
    {
        p_ptr->smithing = old_smithing;
        p_ptr->smithing_leftover = old_smithing_leftover;
    }
    else
    {
        if (p_ptr->smithing_leftover == 0)
        {
            /* Wipe the smithing object */
            object_wipe(smith_o_ptr);
            smith_clear_alloy_state(&smith_alloy);
        }

        // Hack: flag that we are done with smithing
        p_ptr->smithing = 0;
    }

    /* Load screen */
    smith_ui_reset_description_state();
    ui_menu_click_clear();
    ui_scroll_area_clear();
    sdl_pop_terminal_menu_scale();
    screen_pop_supporting_panes_hidden();
    screen_load();
}

/*
 * Actually creates the item.
 */
void create_smithing_item(void)
{
    int slot;
    object_type* o_ptr;
    char o_name[80];

    log_debug("Creating smithing item");

    // pay the ability/experience costs of smithing
    pay_costs();

    // if making an artefact, copy its attributes into the proper place in the
    // a_info array
    if (smith_o_ptr->name1)
    {
        log_info("Creating new artifact");
        smith_o_ptr->name1 = z_info->art_rand_max + p_ptr->self_made_arts;

        artefact_copy(&a_info[smith_o_ptr->name1], smith_a_ptr);
        artefact_type* created = &a_info[smith_o_ptr->name1];
        if (score_guid_is_zero(&created->guid)) {
            created->guid = score_guid_random();
        }
        (void)score_artefact_register(created);
        p_ptr->self_made_arts++;

        // make sure to display it as cursed if it is so
        if (smith_a_ptr->flags3
            & (TR3_LIGHT_CURSE | TR3_HEAVY_CURSE | TR3_PERMA_CURSE))
        {
            smith_o_ptr->ident |= (IDENT_CURSED);
            log_debug("Artifact marked as cursed");
        }

        // Store the depth at which it was created
        smith_o_ptr->xtra1 = p_ptr->depth;

        log_debug("Artifact #%d created at depth %d", p_ptr->self_made_arts, p_ptr->depth);
    }

        /* ------------------------------------------------------ */
        /* New escape-curse: smithing can back-fire               */
        /* ------------------------------------------------------ */
        {
            int stacks = curse_flag_count_cur(CUR_SMITHCURSE);          /* 0-3 */
            if (stacks &&            /* must have the curse          */
                !(smith_o_ptr->ident & IDENT_CURSED) &&             /* not already */
                (smith_o_ptr->tval != TV_LIGHT))                    /* skip torches */
            {
                if (rand_int(100) < 10 * stacks)                    /* 10 % / stack */
                {
                    log_debug("Smithing curse triggered - adding random curse");
                    add_random_curse(smith_o_ptr);
                }
            }
        }


    // remove the spoiler ident flag
    smith_o_ptr->ident &= ~(IDENT_SPOIL);

    // identify the object
    ident(smith_o_ptr);

    // create description
    object_desc(o_name, sizeof(o_name), smith_o_ptr, true, 3);

    // Record the depth where the object was created
    do_cmd_note(format("Made %s  %d.%d lb", o_name,
                    (smith_o_ptr->weight * smith_o_ptr->number) / 10,
                    (smith_o_ptr->weight * smith_o_ptr->number) % 10),
        p_ptr->depth);

    // Get the slot of the forged item
    slot = inven_carry(smith_o_ptr, true);

    // Check if the item couldn't fit in inventory (e.g., group limit)
    if (slot < 0)
    {
        // Drop it on the floor instead
        log_debug("Smithed item couldn't fit in inventory, dropping to floor");
        drop_near(smith_o_ptr, 0, p_ptr->py, p_ptr->px);

        // Describe the object
        object_desc(o_name, sizeof(o_name), smith_o_ptr, true, 3);

        // Message
        msg_format("You have forged %s, but it falls to the floor.", o_name);
        log_info("Created smithing item (dropped): %s", o_name);
    }
    else
    {
        // Get the item itself
        o_ptr = &inventory[slot];

        // Mark the item as smithed by the player (using unused1 field)
        o_ptr->unused1 = 1;  /* 1 = smithed by player, 0 = found item */

        // Describe the object
        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);

        // Message
        msg_format("You have %s (%c).", o_name, index_to_label(slot));
        log_info("Created smithing item: %s", o_name);
    }

    // Wipe the smithing object
    object_wipe(smith_o_ptr);
    smith_clear_alloy_state(&smith_alloy);
}
