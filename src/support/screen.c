#include "angband.h"
#include "support/screen.h"
#include "externs.h"
#include "log/log.h"
#include "ui/menu-click.h"

/*
 * Hack -- prevent "accidents" in "screen_save()" or "screen_load()"
 */
static int screen_depth = 0;
static int supporting_panes_hidden_depth = 0;
static int touch_pane_hidden_depth = 0;
static int touch_pane_proto_depth = 0;
static bool startup_supporting_panes_hidden = false;
static bool startup_touch_pane_hidden = false;

bool screen_saved_fullscreen_active(void)
{
    return (startup_supporting_panes_hidden || supporting_panes_hidden_depth > 0);
}

void screen_push_supporting_panes_hidden(void)
{
    supporting_panes_hidden_depth++;
    sdl_refresh_supporting_panes_layout();
}

void screen_pop_supporting_panes_hidden(void)
{
    if (supporting_panes_hidden_depth > 0)
        supporting_panes_hidden_depth--;
    sdl_refresh_supporting_panes_layout();
}

bool screen_supporting_panes_hidden_active(void)
{
    return (supporting_panes_hidden_depth > 0);
}

void screen_set_startup_supporting_panes_hidden(bool hidden)
{
    startup_supporting_panes_hidden = hidden;
    sdl_refresh_supporting_panes_layout();
}

bool screen_startup_supporting_panes_hidden_active(void)
{
    return startup_supporting_panes_hidden;
}

void screen_push_touch_pane_hidden(void)
{
    touch_pane_hidden_depth++;
    sdl_refresh_supporting_panes_layout();
}

void screen_pop_touch_pane_hidden(void)
{
    if (touch_pane_hidden_depth > 0)
        touch_pane_hidden_depth--;
    sdl_refresh_supporting_panes_layout();
}

bool screen_touch_pane_hidden_active(void)
{
    return (touch_pane_hidden_depth > 0);
}

void screen_set_startup_touch_pane_hidden(bool hidden)
{
    startup_touch_pane_hidden = hidden;
    sdl_refresh_supporting_panes_layout();
}

bool screen_startup_touch_pane_hidden_active(void)
{
    return startup_touch_pane_hidden;
}

void screen_push_touch_pane_proto(void)
{
    touch_pane_proto_depth++;
    sdl_refresh_supporting_panes_layout();
}

void screen_pop_touch_pane_proto(void)
{
    if (touch_pane_proto_depth > 0)
        touch_pane_proto_depth--;
    sdl_refresh_supporting_panes_layout();
}

bool screen_touch_pane_proto_active(void)
{
    return (touch_pane_proto_depth > 0);
}

void ui_reset_transient_state_for_new_session(void)
{
    bool pane_depth_changed = supporting_panes_hidden_depth
        || touch_pane_hidden_depth || touch_pane_proto_depth;

    ui_menu_click_clear();
    ui_scroll_area_clear();
    ui_key_wait_dismiss_clear();
    sdl_mouse_path_cancel();

    supporting_panes_hidden_depth = 0;
    touch_pane_hidden_depth = 0;
    touch_pane_proto_depth = 0;
    if (pane_depth_changed)
        sdl_refresh_supporting_panes_layout();

    item_tester_full = false;
    item_tester_tval = 0;
    item_tester_hook = NULL;

    hide_cursor = false;
    sdl_story_font_reset();

    /* A new session must start at the default (un-zoomed) grid.  Otherwise a
     * gameplay main-view zoom left over from the previous session keeps the
     * terminal short and trips play_game()'s minimum-size check -- this is what
     * aborted the relaunch into Blitz (62x15 grid, needs >= 50x18). */
    sdl_reset_main_view_zoom();
}

void screen_clear_all_terms_no_fresh(void)
{
    term* old = Term;

    for (int i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        if (!angband_term[i])
            continue;

        Term_activate(angband_term[i]);
        Term_clear();
    }

    Term_activate(old);
}

/*
 * Save the screen, and increase the "icky" depth.
 *
 * This function must match exactly one call to "screen_load()".
 */
void screen_save(void)
{
    g_term_clear_hook = NULL;
    sdl_mouse_path_cancel();

    /* Hack -- Flush messages */
    message_flush();

    sdl_suspend_main_view_zoom_for_saved_screen();

    /* Log line 0 state before save */
    if (Term && Term->scr)
    {
        char buffer_content[256];
        byte* scr_story = Term->scr->story[0];
        int i;
        int len = Term->wid;
        for (i = 0; i < len && i < 80; i++)
        {
            char c = Term->scr->c[0][i];
            buffer_content[i] = (c >= 32 && c <= 126) ? c : '.';
        }
        buffer_content[i] = '\0';
        log_debug("screen_save: BEFORE save row=0 buffer='%s' story_flags[0-10]=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                  buffer_content,
                  scr_story[0], scr_story[1], scr_story[2], scr_story[3], scr_story[4],
                  scr_story[5], scr_story[6], scr_story[7], scr_story[8], scr_story[9], scr_story[10]);
    }
    
    /* Save the screen (if legal) */
    if (screen_depth++ == 0)
        Term_save();

    /* Increase "icky" depth */
    character_icky++;
    log_debug("screen_save: character_icky incremented to %d, screen_depth=%d", character_icky, screen_depth);
}

static void screen_load_impl(bool refresh_restored_screen)
{
    bool restored_screen = false;
    bool resumed_main_view_zoom;

    /* Hack -- Flush messages */
    message_flush();

    /* Load the screen (if legal) */
    if (--screen_depth == 0)
    {
        Term_load();
        restored_screen = true;
    }

    /* Decrease "icky" depth */
    character_icky--;
    log_debug("screen_load: character_icky decremented to %d, screen_depth=%d", character_icky, screen_depth);
    
    /* Log line 0 state after load */
    if (Term && Term->scr)
    {
        char buffer_content[256];
        byte* scr_story = Term->scr->story[0];
        int i;
        int len = Term->wid;
        for (i = 0; i < len && i < 80; i++)
        {
            char c = Term->scr->c[0][i];
            buffer_content[i] = (c >= 32 && c <= 126) ? c : '.';
        }
        buffer_content[i] = '\0';
        log_debug("screen_load: AFTER load row=0 buffer='%s' story_flags[0-10]=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                  buffer_content,
                  scr_story[0], scr_story[1], scr_story[2], scr_story[3], scr_story[4],
                  scr_story[5], scr_story[6], scr_story[7], scr_story[8], scr_story[9], scr_story[10]);
    }

    resumed_main_view_zoom =
        sdl_resume_main_view_zoom_for_saved_screen();
    sdl_refresh_supporting_panes_layout();

    /*
     * A saved-screen menu temporarily suspends gameplay zoom.  If restoring it
     * resized the terminal, redraw at that final size before presenting;
     * otherwise the configured-scale dungeon is visible for one frame.
     */
    if (restored_screen && refresh_restored_screen)
    {
        if (resumed_main_view_zoom && character_generated && !character_icky
            && p_ptr && p_ptr->playing && character_dungeon)
        {
            do_cmd_redraw();
        }
        else
        {
            Term_fresh();
        }
    }

    if (character_generated && !character_icky && p_ptr && p_ptr->playing
        && p_ptr->window)
    {
        handle_stuff();
    }
}

/*
 * Load the screen, and decrease the "icky" depth.
 *
 * This function must match exactly one call to "screen_save()".
 */
void screen_load(void)
{
    screen_load_impl(true);
}

void screen_load_quiet(void)
{
    screen_load_impl(false);
}

/*
 * Move the cursor
 */
void move_cursor(int row, int col) { Term_gotoxy(col, row); }

/*
 * Display a string on the screen using an attribute.
 *
 * At the given location, using the given attribute, if allowed,
 * add the given string.  Do not clear the line.
 */
void c_put_str(byte attr, cptr str, int row, int col)
{
    /* Position cursor, Dump the attr/text */
    Term_putstr(col, row, -1, attr, str);
}

/*
 * As above, but in "white"
 */
void put_str(cptr str, int row, int col)
{
    /* Spawn */
    Term_putstr(col, row, -1, TERM_WHITE, str);
}

/*
 * Display a string on the screen using an attribute, and clear
 * to the end of the line.
 */
void c_prt(byte attr, cptr str, int row, int col)
{
    /* Log what we're about to print, especially for line 0 */
    if (row == 0)
    {
        log_debug("c_prt: row=0 col=%d attr=%d str='%s' story_font_active=%d", 
                  col, attr, str, Term && Term->story_font_active ? 1 : 0);
        
        /* Log current buffer state before erase */
        if (Term && Term->scr)
        {
            char buffer_content[256];
            byte* scr_story = Term->scr->story[0];
            int i;
            int len = Term->wid;
            for (i = 0; i < len && i < 80; i++)
            {
                char c = Term->scr->c[0][i];
                buffer_content[i] = (c >= 32 && c <= 126) ? c : '.';
            }
            buffer_content[i] = '\0';
            log_debug("c_prt: BEFORE erase row=0 buffer='%s' story_flags[0-10]=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                      buffer_content,
                      scr_story[0], scr_story[1], scr_story[2], scr_story[3], scr_story[4],
                      scr_story[5], scr_story[6], scr_story[7], scr_story[8], scr_story[9], scr_story[10]);
        }
    }
    
    /* Clear line, position cursor */
    Term_erase(col, row, 255);
    
    /* Log buffer state after erase, before adding text */
    if (row == 0 && Term && Term->scr)
    {
        byte* scr_story = Term->scr->story[0];
        log_debug("c_prt: AFTER erase row=0 story_flags[0-10]=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                  scr_story[0], scr_story[1], scr_story[2], scr_story[3], scr_story[4],
                  scr_story[5], scr_story[6], scr_story[7], scr_story[8], scr_story[9], scr_story[10]);
    }

    /* Dump the attr/text */
    Term_addstr(-1, attr, str);
    /* Log buffer state after adding text */
    if (row == 0 && Term && Term->scr)
    {
        char buffer_content[256];
        byte* scr_story = Term->scr->story[0];
        int i;
        int len = Term->wid;
        for (i = 0; i < len && i < 80; i++)
        {
            char c = Term->scr->c[0][i];
            buffer_content[i] = (c >= 32 && c <= 126) ? c : '.';
        }
        buffer_content[i] = '\0';
        log_debug("c_prt: AFTER addstr row=0 buffer='%s' story_flags[0-10]=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                  buffer_content,
                  scr_story[0], scr_story[1], scr_story[2], scr_story[3], scr_story[4],
                  scr_story[5], scr_story[6], scr_story[7], scr_story[8], scr_story[9], scr_story[10]);
    }
}

/*
 * As above, but in "white"
 */
void prt(cptr str, int row, int col)
{
    /* Spawn */
    c_prt(TERM_WHITE, str, row, col);
}

/*
 * Clear part of the screen
 */
void clear_from(int row)
{
    int y;

    /* Erase requested rows */
    for (y = row; y < Term->hgt; y++)
    {
        /* Erase part of the screen */
        Term_erase(0, y, 255);
    }
}
