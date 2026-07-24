#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "pane.h"
#include "supplies.h"
#include "item_set.h"
#include "ui/status/status-internal.h"

/*
 * Prints player's current song (if any)
 */
void prt_song(void)
{
    if (ui_status_pane_owns_left_panel_statuses())
    {
        if (!ui_hide_left_panel())
        {
            Term_erase(COL_SONG, ROW_SONG, LEFT_PANEL_CONTENT_WID);
            if (!ui_compact_height())
                Term_erase(COL_SONG, ROW_SONG + 1, LEFT_PANEL_CONTENT_WID);
        }
        return;
    }

    if (ui_compact_status_line_handles_song())
    {
        prt_status_line_compact();
        return;
    }

    // wipe old songs
    Term_erase(COL_SONG, ROW_SONG, LEFT_PANEL_CONTENT_WID);
    if (!ui_compact_height())
        Term_erase(COL_SONG, ROW_SONG + 1, LEFT_PANEL_CONTENT_WID);

    char* song1_name
        = b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name;
    char* song2_name
        = b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name;

    sdl_story_font_enable();

    if (ui_compact_height())
    {
        /* Compact height: render a single combined song line. */
        char buf[32] = "";
        if (p_ptr->song1 != SNG_NOTHING && p_ptr->song2 != SNG_NOTHING)
            strnfmt(buf, sizeof(buf), "%s+%s", song1_name + 8, song2_name + 8);
        else if (p_ptr->song1 != SNG_NOTHING)
            SDL_strlcpy(buf, song1_name + 8, sizeof(buf));
        else if (p_ptr->song2 != SNG_NOTHING)
            SDL_strlcpy(buf, song2_name + 8, sizeof(buf));

        if (buf[0])
            Term_putstr(COL_SONG, ROW_SONG, LEFT_PANEL_CONTENT_WID,
                panel_touch_zone_attr(SDL_PANEL_CLICK_SONG, ROW_SONG,
                    TERM_L_BLUE),
                buf);
    }
    else
    {
        // show the first song
        if (p_ptr->song1 != SNG_NOTHING)
        {
            Term_putstr(COL_SONG, ROW_SONG, LEFT_PANEL_CONTENT_WID,
                panel_touch_zone_attr(SDL_PANEL_CLICK_SONG, ROW_SONG,
                    TERM_L_BLUE),
                song1_name + 8);
        }

        // show the second song
        if (p_ptr->song2 != SNG_NOTHING)
        {
            Term_putstr(COL_SONG, ROW_SONG + 1, LEFT_PANEL_CONTENT_WID,
                panel_touch_zone_attr(SDL_PANEL_CLICK_SONG, ROW_SONG + 1,
                    TERM_BLUE),
                song2_name + 8);
        }
    }

    sdl_story_font_disable();
}

/*
 * Prints depth in stat area
 */
void prt_depth(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    if (ui_depth_menu_owns_left_panel_depth())
        return;

    char depths[32];
    s16b attr = TERM_WHITE;

    if (!p_ptr->depth)
    {
        SDL_strlcpy(depths, "Surface", sizeof(depths));
    }
    else
    {
        sprintf(depths, "%d ft", p_ptr->depth * 50);
    }

    /* Get color of level based on feeling  -JSV- */
    if ((p_ptr->depth) && (do_feeling))
    {
        if (feeling == 1)
            attr = TERM_VIOLET;
        else if (feeling == 2)
            attr = TERM_RED;
        else if (feeling == 3)
            attr = TERM_L_RED;
        else if (feeling == 4)
            attr = TERM_ORANGE;
        else if (feeling == 5)
            attr = TERM_ORANGE;
        else if (feeling == 6)
            attr = TERM_YELLOW;
        else if (feeling == 7)
            attr = TERM_YELLOW;
        else if (feeling == 8)
            attr = TERM_WHITE;
        else if (feeling == 9)
            attr = TERM_WHITE;
        else if (feeling == 10)
            attr = TERM_L_WHITE;
        else if (feeling >= LEV_THEME_HEAD)
            attr = TERM_BLUE;
    }

    attr = status_touch_zone_attr(SDL_STATUS_CLICK_MAP, COL_DEPTH, 7,
        (byte)attr);

    sdl_story_font_enable();

    /* Right-Adjust the "depth", and clear old values */
    c_prt(attr, format("%7s", depths), ROW_DEPTH, COL_DEPTH);

    sdl_story_font_disable();

    prt_status_line_view_button();
}

/*
 * Prints status of hunger
 */
void prt_hunger(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    sdl_story_font_enable();

    /* Fainting / Starving */
    if (p_ptr->food < PY_FOOD_STARVE)
    {
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_HUNGRY, 8, TERM_RED), "Starving", ROW_HUNGRY, COL_HUNGRY);
    }

    /* Weak */
    else if (p_ptr->food < PY_FOOD_WEAK)
    {
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_HUNGRY, 8, TERM_ORANGE), "Weak    ", ROW_HUNGRY,
            COL_HUNGRY);
    }

    /* Hungry */
    else if (p_ptr->food < PY_FOOD_ALERT)
    {
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_HUNGRY, 8, TERM_YELLOW), "Hungry  ", ROW_HUNGRY,
            COL_HUNGRY);
    }

    /* Normal */
    else if (p_ptr->food < PY_FOOD_FULL)
    {
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_HUNGRY, 8, TERM_L_GREEN), "        ", ROW_HUNGRY,
            COL_HUNGRY);
    }

    /* Full */
    else if (p_ptr->food < PY_FOOD_MAX)
    {
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_HUNGRY, 8, TERM_L_GREEN), "Full    ", ROW_HUNGRY,
            COL_HUNGRY);
    }

    else
    {
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_HUNGRY, 8, TERM_GREEN), "Full    ", ROW_HUNGRY,
            COL_HUNGRY);
    }

    sdl_story_font_disable();
}

/*
 * Prints Blind status
 */
void prt_blind(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    sdl_story_font_enable();

    if (p_ptr->blind)
    {
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_BLIND, 5, TERM_ORANGE), "Blind", ROW_BLIND, COL_BLIND);
    }
    else
    {
        put_str("     ", ROW_BLIND, COL_BLIND);
    }

    sdl_story_font_disable();
}

/*
 * Prints Confusion status
 */
void prt_confused(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_CONFUSED, ROW_CONFUSED, 8);

    if (p_ptr->confused)
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_CONFUSED, 8, TERM_ORANGE), "Confused", ROW_CONFUSED,
            COL_CONFUSED);
        sdl_story_font_disable();
    }
}

/*
 * Prints Fear status
 */
void prt_afraid(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_AFRAID, ROW_AFRAID, 6);

    if (p_ptr->afraid)
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_AFRAID, 6, TERM_ORANGE), "Afraid", ROW_AFRAID,
            COL_AFRAID);
        sdl_story_font_disable();
    }
}

/*
 *  Displays the amount of bleeding.
 *  This is a bit tricky as it is in the same row as poison, *unless* you have
 * both. In which case it is the row above.
 */

void prt_cut(void)
{
    if (ui_compact_status_line_handles_wounds())
    {
        prt_status_line_compact();
        return;
    }

    if (ui_hide_left_panel())
        return;

    if (ui_status_pane_owns_left_panel_statuses())
    {
        Term_erase(COL_CUT, ROW_CUT - 1, 12);
        Term_erase(COL_CUT, ROW_CUT, 12);
        return;
    }

    if (ui_status_system_compact() && ui_wound_rows_overlap_status_line())
        return;

    if (ui_compact_height())
    {
        prt_cut_poisoned_compact();
        return;
    }

    int c = p_ptr->cut;
    char num_buf[8];

    int r = ROW_CUT;

    if (p_ptr->poisoned)
        r--;

    /* Clear both possible rows (story font has variable widths) */
    Term_erase(COL_CUT, ROW_CUT - 1, 12);
    if (!p_ptr->poisoned)
        Term_erase(COL_CUT, ROW_CUT, 12);

    if (c > 100)
    {
        sdl_story_font_enable();
        c_put_str(TERM_RED, "Mortal wound", r, COL_CUT);
        sdl_story_font_disable();
    }
    else if (c > 20)
    {
        sdl_story_font_enable();
        c_put_str(TERM_RED, "Bleeding", r, COL_CUT);
        sdl_story_font_disable();
        sprintf(num_buf, " %-2d", c);
        c_put_str(TERM_RED, num_buf, r, COL_CUT + 8);
    }
    else if (c > 0)
    {
        sdl_story_font_enable();
        c_put_str(TERM_L_RED, "Bleeding", r, COL_CUT);
        sdl_story_font_disable();
        sprintf(num_buf, " %-2d", c);
        c_put_str(TERM_L_RED, num_buf, r, COL_CUT + 8);
    }
}

/*
 * Prints Poisoned status
 */
void prt_poisoned(void)
{
    if (ui_compact_status_line_handles_wounds())
    {
        prt_status_line_compact();
        return;
    }

    if (ui_hide_left_panel())
        return;

    if (ui_status_pane_owns_left_panel_statuses())
    {
        Term_erase(COL_POISONED, ROW_POISONED, 12);
        return;
    }

    if (ui_status_system_compact() && ui_wound_rows_overlap_status_line())
        return;

    if (ui_compact_height())
    {
        prt_cut_poisoned_compact();
        return;
    }

    int p = p_ptr->poisoned;
    char num_buf[8];

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_POISONED, ROW_POISONED, 12);

    if (p > 20)
    {
        sdl_story_font_enable();
        c_put_str(TERM_L_GREEN, "Poisoned", ROW_POISONED, COL_POISONED);
        sdl_story_font_disable();
        sprintf(num_buf, " %-3d", p);
        c_put_str(TERM_L_GREEN, num_buf, ROW_POISONED, COL_POISONED + 8);
    }
    else if (p > 0)
    {
        sdl_story_font_enable();
        c_put_str(TERM_GREEN, "Poisoned", ROW_POISONED, COL_POISONED);
        sdl_story_font_disable();
        sprintf(num_buf, " %-3d", p);
        c_put_str(TERM_GREEN, num_buf, ROW_POISONED, COL_POISONED + 8);
    }
}

/*
 * Prints Searching, Resting, Entrancement, Smithing, or 'count' status
 * Display is always exactly 10 characters wide (see below)
 *
 * This function was a major bottleneck when resting, so a lot of
 * the text formatting code was optimized in place below.
 */
void prt_state(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    byte attr = TERM_WHITE;

    char text[16];

    /* Entrancement */
    if (p_ptr->entranced)
    {
        attr = TERM_RED;

        SDL_strlcpy(text, "Entranced!", sizeof(text));
    }

    /* Smithing */
    if (p_ptr->smithing)
    {
        SDL_strlcpy(text, "Smithing  ", sizeof(text));
    }

    if (p_ptr->fletching)
    {
        SDL_strlcpy(text, "Fletching ", sizeof(text));
    }
    else if (p_ptr->rage)
    {
        attr = TERM_RED;
        SDL_strlcpy(text, "Rage      ", sizeof(text));
    }

    /* Resting */
    else if (p_ptr->resting)
    {
        int i;
        int n = p_ptr->resting;

        /* Start with "Rest" */
        SDL_strlcpy(text, "Rest      ", sizeof(text));

        /* Extensive (timed) rest */
        if (n >= 1000)
        {
            i = n / 100;
            text[9] = '0';
            text[8] = '0';
            text[7] = I2D(i % 10);
            if (i >= 10)
            {
                i = i / 10;
                text[6] = I2D(i % 10);
                if (i >= 10)
                {
                    text[5] = I2D(i / 10);
                }
            }
        }

        /* Long (timed) rest */
        else if (n >= 100)
        {
            i = n;
            text[9] = I2D(i % 10);
            i = i / 10;
            text[8] = I2D(i % 10);
            text[7] = I2D(i / 10);
        }

        /* Medium (timed) rest */
        else if (n >= 10)
        {
            i = n;
            text[9] = I2D(i % 10);
            text[8] = I2D(i / 10);
        }

        /* Short (timed) rest */
        else if (n > 0)
        {
            i = n;
            text[9] = I2D(i);
        }

        /* Rest until healed */
        else if (n == -1)
        {
            text[5] = text[6] = text[7] = text[8] = text[9] = '*';
        }

        /* Rest until done */
        else if (n == -2)
        {
            text[5] = text[6] = text[7] = text[8] = text[9] = '&';
        }
    }

    /* Repeating */
    else if (p_ptr->command_rep)
    {
        if (p_ptr->command_rep > 999)
        {
            sprintf(text, "Rep. %3d00", p_ptr->command_rep / 100);
        }
        else
        {
            sprintf(text, "Repeat %3d", p_ptr->command_rep);
        }
    }

    /* Stealth mode */
    else if (p_ptr->stealth_mode)
    {
        SDL_strlcpy(text, "Stealth   ", sizeof(text));
    }

    /* Nothing interesting */
    else
    {
        text[0] = '\0';
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_STATE, ROW_STATE, 10);

    /* Display the info if any */
    if (text[0])
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_STATE, 10, attr), text, ROW_STATE, COL_STATE);
        sdl_story_font_disable();
    }
    else
    {
        prt_status_line_main_menu_hint(false);
    }
}

/*
 * Prints the speed of a character.			-CJS-
 */
void prt_speed(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    int i = p_ptr->pspeed;

    byte attr = TERM_WHITE;
    char buf[32] = "";

    /* Fast */
    if (i > 2)
    {
        attr = TERM_L_GREEN;
        sprintf(buf, "Fast");
    }

    /* Slow */
    else if (i < 2)
    {
        attr = TERM_ORANGE;
        sprintf(buf, "Slow");
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_SPEED, ROW_SPEED, 4);

    /* Display the speed if not normal */
    if (buf[0])
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_SPEED, 4, attr), buf, ROW_SPEED, COL_SPEED);
        sdl_story_font_disable();
    }
}

static const char* partition_abbrev_for_point(int y, int x)
{
    switch (level_partition_kind_for_point(y, x))
    {
    case LEVEL_PART_ROOMY:
        return "Room";
    case LEVEL_PART_RUINED:
        return "Ruin";
    case LEVEL_PART_CAVEY:
        return "Caves";
    case LEVEL_PART_BIG_CAVE:
        return "BigCa";
    case LEVEL_PART_LABYRINTH:
        return "Labir";
    case LEVEL_PART_CHASM:
        return "Chasm";
    default:
        return "";
    }
}

void prt_partition(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    if (!p_ptr)
        return;

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_PARTITION, ROW_PARTITION, 5);

    const char* label = partition_abbrev_for_point(p_ptr->py, p_ptr->px);
    if (!label[0])
        return;

    sdl_story_font_enable();
    c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAP, COL_PARTITION,
        (int)strlen(label), TERM_WHITE), label, ROW_PARTITION, COL_PARTITION);
    sdl_story_font_disable();
}

/*
 * Prints message regarding difficult terrain
 */
void prt_terrain(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_TERRAIN, ROW_TERRAIN, 5);

    if (cave_pit_bold(p_ptr->py, p_ptr->px))
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_TERRAIN, 3, TERM_ORANGE), "Pit", ROW_TERRAIN, COL_TERRAIN);
        sdl_story_font_disable();
    }
    else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB)
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_TERRAIN, 3, TERM_ORANGE), "Web", ROW_TERRAIN, COL_TERRAIN);
        sdl_story_font_disable();
    }
    else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_SUNLIGHT)
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_TERRAIN, 3, TERM_YELLOW), "Sun", ROW_TERRAIN, COL_TERRAIN);
        sdl_story_font_disable();
    }

    prt_partition();
}

void prt_cut_poisoned_compact(void)
{
    if (!Term || !p_ptr)
        return;

    const int row = ROW_CUT;
    const int col = COL_CUT;
    const int width = 12;

    Term_erase(col, row, width);

    int x = col;

    int c = p_ptr->cut;
    int p = p_ptr->poisoned;

    if (c > 0)
    {
        byte cut_attr = (c > 20) ? TERM_RED : TERM_L_RED;
        char cut_buf[16];

        if (c > 100)
        {
            cut_attr = TERM_RED;
            SDL_strlcpy(cut_buf, "MW", sizeof(cut_buf));
        }
        else
        {
            strnfmt(cut_buf, sizeof(cut_buf), "Bld:%d", c);
        }

        int len = (int)strlen(cut_buf);
        if (x + len > col + width)
            len = (col + width) - x;
        if (len > 0)
            Term_putstr(x, row, len, cut_attr, cut_buf);
        x += len;
    }

    if (p > 0 && x < col + width)
    {
        if (c > 0 && x < col + width)
        {
            Term_putstr(x, row, 1, TERM_WHITE, " ");
            x++;
        }

        byte pois_attr = (p > 20) ? TERM_L_GREEN : TERM_GREEN;
        char pois_buf[16];
        strnfmt(pois_buf, sizeof(pois_buf), "Poi:%d", p);
        int len = (int)strlen(pois_buf);
        if (x + len > col + width)
            len = (col + width) - x;
        if (len > 0)
            Term_putstr(x, row, len, pois_attr, pois_buf);
    }
}

void prt_stun(void)
{
    if (ui_status_system_compact())
    {
        prt_status_line_compact();
        return;
    }

    int s = p_ptr->stun;

    /* Clear the area first (story font has variable widths) */
    Term_erase(COL_STUN, ROW_STUN, 12);

    if (s > 100)
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_STUN, 11, TERM_RED), "Knocked out", ROW_STUN, COL_STUN);
        sdl_story_font_disable();
    }
    else if (s > 50)
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_STUN, 10, TERM_ORANGE), "Heavy stun", ROW_STUN, COL_STUN);
        sdl_story_font_disable();
    }
    else if (s)
    {
        sdl_story_font_enable();
        c_put_str(status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
            COL_STUN, 4, TERM_ORANGE), "Stun", ROW_STUN, COL_STUN);
        sdl_story_font_disable();
    }
}

typedef struct {
    const char* long_text;
    const char* short_text;
    byte attr;
    bool required;
    int click_action;
} status_seg;

static int status_line_len(const status_seg* segs, int count, bool use_long,
                           const bool* include)
{
    int len = 0;
    int shown = 0;
    for (int i = 0; i < count; i++)
    {
        if (include && !include[i])
            continue;
        const char* t = use_long ? segs[i].long_text : segs[i].short_text;
        if (!t || !t[0])
            continue;
        if (shown > 0)
            len += 1;
        len += (int)strlen(t);
        shown++;
    }
    return len;
}

static byte status_depth_attr(void)
{
    s16b attr = TERM_WHITE;

    if ((p_ptr->depth) && (do_feeling))
    {
        if (feeling == 1)
            attr = TERM_VIOLET;
        else if (feeling == 2)
            attr = TERM_RED;
        else if (feeling == 3)
            attr = TERM_L_RED;
        else if (feeling == 4)
            attr = TERM_ORANGE;
        else if (feeling == 5)
            attr = TERM_ORANGE;
        else if (feeling == 6)
            attr = TERM_YELLOW;
        else if (feeling == 7)
            attr = TERM_YELLOW;
        else if (feeling == 8)
            attr = TERM_WHITE;
        else if (feeling == 9)
            attr = TERM_WHITE;
        else if (feeling == 10)
            attr = TERM_L_WHITE;
        else if (feeling >= LEV_THEME_HEAD)
            attr = TERM_BLUE;
    }

    return (byte)attr;
}

bool status_state_text(char* out_long, size_t out_long_sz,
                              char* out_short, size_t out_short_sz,
                              byte* out_attr)
{
    if (!p_ptr)
        return false;

    out_long[0] = '\0';
    out_short[0] = '\0';
    if (out_attr)
        *out_attr = TERM_WHITE;

    if (p_ptr->entranced)
    {
        if (out_attr)
            *out_attr = TERM_RED;
        SDL_strlcpy(out_long, "Entranced", out_long_sz);
        SDL_strlcpy(out_short, "En", out_short_sz);
        return true;
    }

    if (p_ptr->smithing)
    {
        SDL_strlcpy(out_long, "Smithing", out_long_sz);
        SDL_strlcpy(out_short, "Sm", out_short_sz);
        return true;
    }

    if (p_ptr->fletching)
    {
        SDL_strlcpy(out_long, "Fletching", out_long_sz);
        SDL_strlcpy(out_short, "Fl", out_short_sz);
        return true;
    }

    if (p_ptr->rage)
    {
        if (out_attr)
            *out_attr = TERM_RED;
        SDL_strlcpy(out_long, "Rage", out_long_sz);
        SDL_strlcpy(out_short, "Rg", out_short_sz);
        return true;
    }

    if (p_ptr->resting)
    {
        int n = p_ptr->resting;
        if (n == -1)
        {
            SDL_strlcpy(out_long, "Rest*", out_long_sz);
            SDL_strlcpy(out_short, "R*", out_short_sz);
        }
        else if (n == -2)
        {
            SDL_strlcpy(out_long, "Rest&", out_long_sz);
            SDL_strlcpy(out_short, "R&", out_short_sz);
        }
        else if (n >= 1000)
        {
            strnfmt(out_long, out_long_sz, "Rest %d", n);
            strnfmt(out_short, out_short_sz, "R%dk", n / 1000);
        }
        else
        {
            strnfmt(out_long, out_long_sz, "Rest %d", n);
            strnfmt(out_short, out_short_sz, "R%d", n);
        }
        return true;
    }

    if (p_ptr->command_rep)
    {
        strnfmt(out_long, out_long_sz, "Repeat %d", p_ptr->command_rep);
        strnfmt(out_short, out_short_sz, "Rp%d", p_ptr->command_rep);
        return true;
    }

    if (p_ptr->stealth_mode)
    {
        SDL_strlcpy(out_long, "Stealth", out_long_sz);
        SDL_strlcpy(out_short, "St", out_short_sz);
        return true;
    }

    return false;
}

static bool status_state_is_critical(cptr state)
{
    if (!state || !state[0])
        return false;

    return streq(state, "Entranced") || streq(state, "Stealth");
}

static const char* status_partition_short(const char* long_label)
{
    if (!long_label || !long_label[0])
        return "";
    if (!strcmp(long_label, "Room"))
        return "Rm";
    if (!strcmp(long_label, "Ruin"))
        return "Ru";
    if (!strcmp(long_label, "Caves"))
        return "Cv";
    if (!strcmp(long_label, "BigCa"))
        return "BC";
    if (!strcmp(long_label, "Labir"))
        return "Lb";
    if (!strcmp(long_label, "Chasm"))
        return "Ch";
    return long_label;
}

static bool status_line_span_blank(int row, int col, int width)
{
    if (!Term || !Term->scr || !Term->scr->c)
        return false;
    if (row < 0 || row >= Term->hgt)
        return false;
    if (col < 0 || width <= 0 || col + width > Term->wid)
        return false;

    for (int i = 0; i < width; i++)
    {
        unsigned char ch = (unsigned char)Term->scr->c[row][col + i];

        if (ch && ch != (unsigned char)Term->char_blank && ch != ' ')
            return false;
    }

    return true;
}

void prt_status_line_main_menu_hint(bool compact_centered)
{
    const char* label = STATUS_MAIN_MENU_HINT;
    int len = (int)strlen(label);
    int row = ROW_STATE;
    int col = COL_STATE;
    byte attr = status_touch_zone_attr(SDL_STATUS_CLICK_MAIN_MENU,
        -1, 0, TERM_SLATE);

    if (!Term || !p_ptr || len <= 0)
        return;
    if (row < 0 || row >= Term->hgt)
        return;
    if (Term->wid < len)
        return;

    if (compact_centered)
    {
        if (Term->wid < len + 2)
            return;

        col = (Term->wid - len) / 2;
        if (!status_line_span_blank(row, col - 1, len + 2))
            return;
    }
    else if (col < 0 || col + len > Term->wid)
    {
        return;
    }

    if (!compact_centered)
        sdl_story_font_enable();
    Term_putstr(col, row, len, attr, label);
    if (!compact_centered)
        sdl_story_font_disable();
}

static bool status_line_choose_view_button(cptr* label, int* col, int* len)
{
    cptr labels[] = { STATUS_VIEW_LABEL, STATUS_VIEW_LABEL_SHORT };

    if (!Term || !label || !col || !len)
        return false;
    if (ROW_STATUS < 0 || ROW_STATUS >= Term->hgt)
        return false;

    for (size_t i = 0; i < N_ELEMENTS(labels); i++)
    {
        int label_len = (int)strlen(labels[i]);
        int label_col = Term->wid - label_len;
        int clear_col = label_col > 0 ? label_col - 1 : label_col;
        int clear_width = label_len + (label_col > 0 ? 1 : 0);

        if (label_len <= 0 || label_col < 0)
            continue;
        if (!status_line_span_blank(ROW_STATUS, clear_col, clear_width))
            continue;

        *label = labels[i];
        *col = label_col;
        *len = label_len;
        return true;
    }

    return false;
}

static bool status_line_right_label_matches(cptr label)
{
    int len;
    int col;

    if (!Term || !Term->scr || !Term->scr->c || !label)
        return false;
    if (ROW_STATUS < 0 || ROW_STATUS >= Term->hgt)
        return false;

    len = (int)strlen(label);
    col = Term->wid - len;
    if (len <= 0 || col < 0)
        return false;

    for (int i = 0; i < len; i++)
    {
        unsigned char actual = (unsigned char)Term->scr->c[ROW_STATUS][col + i];
        unsigned char expected = (unsigned char)label[i];

        if (!actual || actual == (unsigned char)Term->char_blank)
            actual = ' ';
        if (actual != expected)
            return false;
    }

    return true;
}

static void prt_status_line_clear_view_button(void)
{
    cptr labels[] = { STATUS_VIEW_LABEL, STATUS_VIEW_LABEL_SHORT };

    if (!Term || ui_status_system_compact())
        return;

    for (size_t i = 0; i < N_ELEMENTS(labels); i++)
    {
        int len = (int)strlen(labels[i]);
        int col = Term->wid - len;

        if (col >= 0 && status_line_right_label_matches(labels[i]))
            Term_erase(col, ROW_STATUS, len);
    }
}

void prt_status_line_view_button(void)
{
    cptr label = NULL;
    int col = 0;
    int len = 0;
    byte attr;

    prt_status_line_clear_view_button();

    if (!status_line_choose_view_button(&label, &col, &len))
        return;

    attr = status_touch_zone_attr(SDL_STATUS_CLICK_VIEW, col, len,
        TERM_L_BLUE);

    if (ui_status_system_compact())
    {
        Term_putstr(col, ROW_STATUS, len, attr, label);
    }
    else
    {
        sdl_story_font_enable();
        c_put_str(attr, label, ROW_STATUS, col);
        sdl_story_font_disable();
    }
}

void prt_status_line_compact(void)
{
    if (!Term || !p_ptr)
        return;

    const int row = ROW_STATE;
    if (row < 0)
        return;

    Term_erase(0, row, Term->wid);

    status_seg segs[16];
    int seg_count = 0;
    bool fold_song = ui_compact_status_line_handles_song();
    bool fold_wounds = ui_compact_status_line_handles_wounds();

    char hunger_long[16] = "";
    char hunger_short[8] = "";
    byte hunger_attr = TERM_WHITE;
    bool hunger_required = false;

    if (p_ptr->food < PY_FOOD_STARVE) {
        SDL_strlcpy(hunger_long, "Starving", sizeof(hunger_long));
        SDL_strlcpy(hunger_short, "St", sizeof(hunger_short));
        hunger_attr = TERM_RED;
        hunger_required = true;
    } else if (p_ptr->food < PY_FOOD_WEAK) {
        SDL_strlcpy(hunger_long, "Weak", sizeof(hunger_long));
        SDL_strlcpy(hunger_short, "Wk", sizeof(hunger_short));
        hunger_attr = TERM_ORANGE;
        hunger_required = true;
    } else if (p_ptr->food < PY_FOOD_ALERT) {
        SDL_strlcpy(hunger_long, "Hungry", sizeof(hunger_long));
        SDL_strlcpy(hunger_short, "Hu", sizeof(hunger_short));
        hunger_attr = TERM_YELLOW;
        hunger_required = true;
    } else if (p_ptr->food >= PY_FOOD_FULL) {
        SDL_strlcpy(hunger_long, "Full", sizeof(hunger_long));
        SDL_strlcpy(hunger_short, "Fu", sizeof(hunger_short));
        hunger_attr = TERM_L_GREEN;
    }

    char stun_long[16] = "";
    char stun_short[8] = "";
    byte stun_attr = TERM_WHITE;
    if (p_ptr->stun > 100) {
        SDL_strlcpy(stun_long, "Knocked out", sizeof(stun_long));
        SDL_strlcpy(stun_short, "KO", sizeof(stun_short));
        stun_attr = TERM_RED;
    } else if (p_ptr->stun > 50) {
        SDL_strlcpy(stun_long, "Heavy stun", sizeof(stun_long));
        SDL_strlcpy(stun_short, "HS", sizeof(stun_short));
        stun_attr = TERM_ORANGE;
    } else if (p_ptr->stun) {
        SDL_strlcpy(stun_long, "Stun", sizeof(stun_long));
        SDL_strlcpy(stun_short, "St", sizeof(stun_short));
        stun_attr = TERM_ORANGE;
    }

    char state_long[24] = "";
    char state_short[12] = "";
    byte state_attr = TERM_WHITE;
    (void)status_state_text(state_long, sizeof(state_long), state_short,
        sizeof(state_short), &state_attr);
    bool state_required = status_state_is_critical(state_long);

    char cut_long[16] = "";
    char cut_short[8] = "";
    byte cut_attr = TERM_WHITE;
    if (fold_wounds)
    {
        if (p_ptr->cut > 100) {
            SDL_strlcpy(cut_long, "Mortal", sizeof(cut_long));
            SDL_strlcpy(cut_short, "MW", sizeof(cut_short));
            cut_attr = TERM_RED;
        } else if (p_ptr->cut > 20) {
            strnfmt(cut_long, sizeof(cut_long), "Bleed %d", p_ptr->cut);
            strnfmt(cut_short, sizeof(cut_short), "B%d", p_ptr->cut);
            cut_attr = TERM_RED;
        } else if (p_ptr->cut > 0) {
            strnfmt(cut_long, sizeof(cut_long), "Bleed %d", p_ptr->cut);
            strnfmt(cut_short, sizeof(cut_short), "B%d", p_ptr->cut);
            cut_attr = TERM_L_RED;
        }
    }

    char pois_long[16] = "";
    char pois_short[8] = "";
    byte pois_attr = TERM_WHITE;
    if (fold_wounds)
    {
        if (p_ptr->poisoned > 20) {
            strnfmt(pois_long, sizeof(pois_long), "Poison %d", p_ptr->poisoned);
            strnfmt(pois_short, sizeof(pois_short), "P%d", p_ptr->poisoned);
            pois_attr = TERM_L_GREEN;
        } else if (p_ptr->poisoned > 0) {
            strnfmt(pois_long, sizeof(pois_long), "Poison %d", p_ptr->poisoned);
            strnfmt(pois_short, sizeof(pois_short), "P%d", p_ptr->poisoned);
            pois_attr = TERM_GREEN;
        }
    }

    char speed_long[8] = "";
    char speed_short[4] = "";
    byte speed_attr = TERM_WHITE;
    if (p_ptr->pspeed > 2) {
        SDL_strlcpy(speed_long, "Fast", sizeof(speed_long));
        SDL_strlcpy(speed_short, "Fa", sizeof(speed_short));
        speed_attr = TERM_L_GREEN;
    } else if (p_ptr->pspeed < 2) {
        SDL_strlcpy(speed_long, "Slow", sizeof(speed_long));
        SDL_strlcpy(speed_short, "Sl", sizeof(speed_short));
        speed_attr = TERM_ORANGE;
    }

    char terrain_long[8] = "";
    char terrain_short[4] = "";
    byte terrain_attr = TERM_ORANGE;
    if (cave_pit_bold(p_ptr->py, p_ptr->px)) {
        SDL_strlcpy(terrain_long, "Pit", sizeof(terrain_long));
        SDL_strlcpy(terrain_short, "Pt", sizeof(terrain_short));
    } else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_TRAP_WEB) {
        SDL_strlcpy(terrain_long, "Web", sizeof(terrain_long));
        SDL_strlcpy(terrain_short, "Wb", sizeof(terrain_short));
    } else if (cave_feat[p_ptr->py][p_ptr->px] == FEAT_SUNLIGHT) {
        SDL_strlcpy(terrain_long, "Sun", sizeof(terrain_long));
        SDL_strlcpy(terrain_short, "Sn", sizeof(terrain_short));
        terrain_attr = TERM_YELLOW;
    }

    const char* part_long = partition_abbrev_for_point(p_ptr->py, p_ptr->px);
    const char* part_short = status_partition_short(part_long);

    char depth_long[16] = "";
    char depth_short[16] = "";
    int feet = p_ptr->depth * 50;
    if (!p_ptr->depth) {
        SDL_strlcpy(depth_long, "Surface", sizeof(depth_long));
        SDL_strlcpy(depth_short, "0'", sizeof(depth_short));
    } else {
        strnfmt(depth_long, sizeof(depth_long), "%d ft", feet);
        strnfmt(depth_short, sizeof(depth_short), "%d'", feet);
    }
    byte depth_attr = status_depth_attr();

    char song_long[32] = "";
    char song_short[12] = "";
    if (fold_song && (p_ptr->song1 != SNG_NOTHING || p_ptr->song2 != SNG_NOTHING))
    {
        char* song1_name
            = b_name + (&b_info[ability_index(S_SNG, p_ptr->song1)])->name;
        char* song2_name
            = b_name + (&b_info[ability_index(S_SNG, p_ptr->song2)])->name;

        if (p_ptr->song1 != SNG_NOTHING && p_ptr->song2 != SNG_NOTHING)
            strnfmt(song_long, sizeof(song_long), "%s+%s", song1_name + 8,
                song2_name + 8);
        else if (p_ptr->song1 != SNG_NOTHING)
            SDL_strlcpy(song_long, song1_name + 8, sizeof(song_long));
        else if (p_ptr->song2 != SNG_NOTHING)
            SDL_strlcpy(song_long, song2_name + 8, sizeof(song_long));

        if (song_long[0])
            strnfmt(song_short, sizeof(song_short), "S:%.*s", 6, song_long);
    }

    #define ADD_SEG(LTXT, STXT, ATTR, REQ, ACTION) \
        do { \
            if ((LTXT)[0]) { \
                segs[seg_count].long_text = (LTXT); \
                segs[seg_count].short_text = (STXT)[0] ? (STXT) : (LTXT); \
                segs[seg_count].attr = (ATTR); \
                segs[seg_count].required = (REQ); \
                segs[seg_count].click_action = (ACTION); \
                seg_count++; \
            } \
        } while (0)

    ADD_SEG(hunger_long, hunger_short, hunger_attr, hunger_required,
        SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(p_ptr->blind ? "Blind" : "", "Bl", TERM_ORANGE, true,
        SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(p_ptr->confused ? "Confused" : "", "Cn", TERM_ORANGE, true,
        SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(cut_long, cut_short, cut_attr, true, SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(pois_long, pois_short, pois_attr, true, SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(stun_long, stun_short, stun_attr, true, SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(p_ptr->afraid ? "Afraid" : "", "Af", TERM_ORANGE, true,
        SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(song_long, song_short, TERM_L_BLUE, false, SDL_STATUS_CLICK_SONG);
    ADD_SEG(state_long, state_short, state_attr, state_required,
        SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(speed_long, speed_short, speed_attr, true,
        SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(terrain_long, terrain_short, terrain_attr, true,
        SDL_STATUS_CLICK_MAIN_MENU);
    ADD_SEG(part_long, part_short, TERM_WHITE, false, SDL_STATUS_CLICK_MAP);
    ADD_SEG(depth_long, depth_short, depth_attr, false, SDL_STATUS_CLICK_MAP);

    #undef ADD_SEG

    int max_w = Term->wid;
    if (max_w <= 0)
        return;

    bool include[16];
    for (int i = 0; i < seg_count; i++)
        include[i] = true;

    bool use_long = (status_line_len(segs, seg_count, true, include) <= max_w);
    if (!use_long)
    {
        while (status_line_len(segs, seg_count, false, include) > max_w)
        {
            bool dropped = false;
            for (int i = seg_count - 1; i >= 0; i--)
            {
                if (!include[i])
                    continue;
                if (segs[i].required)
                    continue;
                include[i] = false;
                dropped = true;
                break;
            }
            if (!dropped)
                break;
        }
    }

    int x = 0;
    bool first = true;
    for (int i = 0; i < seg_count; i++)
    {
        if (!include[i])
            continue;
        const char* t = use_long ? segs[i].long_text : segs[i].short_text;
        if (!t || !t[0])
            continue;

        if (!first)
        {
            if (x < max_w)
                Term_putstr(x, row, 1, TERM_WHITE, " ");
            x++;
        }

        int remaining = max_w - x;
        if (remaining <= 0)
            break;
        int n = (int)strlen(t);
        if (n > remaining)
            n = remaining;
        if (n > 0)
            Term_putstr(x, row, n,
                status_touch_zone_attr(segs[i].click_action, x, n,
                    segs[i].attr),
                t);
        x += n;
        first = false;
    }

    prt_status_line_view_button();
    prt_status_line_main_menu_hint(true);
}
