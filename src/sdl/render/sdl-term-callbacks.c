#include "angband.h"
#include "sdl/main-sdl-private.h"

errr callback_sdl_xtra(int n, int v)
{
    sdl_view* d = sdl_view_from_term(Term);
    switch (n) {
    case TERM_XTRA_EVENT: {
        SDL_Event ev;

        sdl_present_if_needed(d);
        sdl_input_tutorial_maybe_show_deferred();
        sdl_mono_font_prewarm_process_idle();

        if (v) {
            /* The player has ended the session (quit to title / quit program /
             * death) and the command loop is now only waiting to observe
             * p_ptr->leaving.  The wake keypress posted when the quit was
             * chosen can be discarded before this blocking wait runs -- e.g.
             * do_cmd_save_game() -> disturb() -> flush() arms a deferred
             * Term_flush(), and request_command()/do_cmd_redraw() can flush the
             * key queue outright.  Without the wake key we would block here
             * indefinitely while sdl_quit_transition_consume_event() swallows
             * every real keypress, so the window looks frozen until an OS quit
             * event happens to arrive.  Re-arm the wake instead of blocking so
             * the loop unwinds promptly.  Scope this to !playing so ordinary
             * level transitions (leaving set, still playing) are unaffected. */
            if (sdl_quit_transition_active() && !p_ptr->playing) {
                Term_keypress(ESCAPE);
                return 0;
            }
            sdl_music_update(); /* Update music before waiting */
            Uint64 now_ns = SDL_GetTicksNS();
            int timeout_ms = sdl_gamepad_pending_timeout_ms(now_ns);
            int touch_timeout_ms = sdl_touch_pane_pending_timeout_ms(now_ns);
            int menu_touch_timeout_ms = sdl_menu_touch_pending_timeout_ms(now_ns);
            int character_panel_touch_timeout_ms =
                sdl_character_panel_pending_timeout_ms(now_ns);
            int screen_back_timeout_ms =
                sdl_screen_back_gesture_pending_timeout_ms(now_ns);
            int player_action_timeout_ms =
                sdl_player_action_menu_pending_timeout_ms(now_ns);
            int pointer_attack_touch_timeout_ms =
                sdl_pointer_attack_pending_timeout_ms(now_ns);
            int map_touch_timeout_ms = sdl_map_touch_pending_timeout_ms(now_ns);
            int object_tooltip_timeout_ms =
                sdl_object_tooltip_pending_timeout_ms(now_ns);
            int zone_touch_timeout_ms = sdl_touch_zone_pending_timeout_ms(now_ns);
            int top_panel_touch_timeout_ms =
                sdl_touch_top_panel_pending_timeout_ms(now_ns);
            int log_pane_menu_timeout_ms =
                sdl_log_pane_menu_pending_timeout_ms(now_ns);
            int side_pane_menu_timeout_ms =
                sdl_side_pane_menu_pending_timeout_ms(now_ns);
            int select_page_turn_timeout_ms =
                sdl_select_page_turn_timeout_ms(now_ns);
            int question_menu_timeout_ms =
                sdl_question_menu_pending_timeout_ms(now_ns);
            int round_wheel_timeout_ms =
                sdl_touch_round_pending_timeout_ms(now_ns);
            int thumb_touch_timeout_ms =
                sdl_touch_thumb_pending_timeout_ms(now_ns);
            bool old_blocking_key_wait = g_sdl_blocking_key_wait;
            if (timeout_ms < 0 || (touch_timeout_ms >= 0 && touch_timeout_ms < timeout_ms))
                timeout_ms = touch_timeout_ms;
            if (timeout_ms < 0 || (menu_touch_timeout_ms >= 0 && menu_touch_timeout_ms < timeout_ms))
                timeout_ms = menu_touch_timeout_ms;
            if (timeout_ms < 0 || (character_panel_touch_timeout_ms >= 0
                    && character_panel_touch_timeout_ms < timeout_ms))
            {
                timeout_ms = character_panel_touch_timeout_ms;
            }
            if (timeout_ms < 0 || (screen_back_timeout_ms >= 0
                    && screen_back_timeout_ms < timeout_ms))
            {
                timeout_ms = screen_back_timeout_ms;
            }
            if (timeout_ms < 0 || (player_action_timeout_ms >= 0
                    && player_action_timeout_ms < timeout_ms))
            {
                timeout_ms = player_action_timeout_ms;
            }
            if (timeout_ms < 0 || (pointer_attack_touch_timeout_ms >= 0
                    && pointer_attack_touch_timeout_ms < timeout_ms))
            {
                timeout_ms = pointer_attack_touch_timeout_ms;
            }
            if (timeout_ms < 0 || (map_touch_timeout_ms >= 0 && map_touch_timeout_ms < timeout_ms))
                timeout_ms = map_touch_timeout_ms;
            if (timeout_ms < 0 || (object_tooltip_timeout_ms >= 0
                    && object_tooltip_timeout_ms < timeout_ms))
            {
                timeout_ms = object_tooltip_timeout_ms;
            }
            if (timeout_ms < 0 || (zone_touch_timeout_ms >= 0 && zone_touch_timeout_ms < timeout_ms))
                timeout_ms = zone_touch_timeout_ms;
            if (timeout_ms < 0 || (top_panel_touch_timeout_ms >= 0
                    && top_panel_touch_timeout_ms < timeout_ms))
            {
                timeout_ms = top_panel_touch_timeout_ms;
            }
            if (timeout_ms < 0 || (log_pane_menu_timeout_ms >= 0
                    && log_pane_menu_timeout_ms < timeout_ms))
            {
                timeout_ms = log_pane_menu_timeout_ms;
            }
            if (timeout_ms < 0 || (side_pane_menu_timeout_ms >= 0
                    && side_pane_menu_timeout_ms < timeout_ms))
            {
                timeout_ms = side_pane_menu_timeout_ms;
            }
            if (timeout_ms < 0 || (select_page_turn_timeout_ms >= 0
                    && select_page_turn_timeout_ms < timeout_ms))
            {
                timeout_ms = select_page_turn_timeout_ms;
            }
            if (timeout_ms < 0 || (question_menu_timeout_ms >= 0
                    && question_menu_timeout_ms < timeout_ms))
            {
                timeout_ms = question_menu_timeout_ms;
            }
            if (timeout_ms < 0 || (round_wheel_timeout_ms >= 0
                    && round_wheel_timeout_ms < timeout_ms))
            {
                timeout_ms = round_wheel_timeout_ms;
            }
            if (timeout_ms < 0 || (thumb_touch_timeout_ms >= 0
                    && thumb_touch_timeout_ms < timeout_ms))
            {
                timeout_ms = thumb_touch_timeout_ms;
            }
            g_sdl_blocking_key_wait = true;
            {
                /* Diagnostic (temporary): measure how long we actually BLOCK
                 * here waiting for input.  If a slow request_command turn has a
                 * matching long [IDLEWAIT], the game was idle (think-time, no
                 * bug); if it is slow with no long [IDLEWAIT], it was busy
                 * (a real stall). */
                Uint64 _wb0 = SDL_GetTicksNS();
                bool _wb_got = (timeout_ms >= 0)
                    ? SDL_WaitEventTimeout(&ev, timeout_ms)
                    : SDL_WaitEvent(&ev);
                Uint64 _wb_ms = (SDL_GetTicksNS() - _wb0) / 1000000ULL;
                if (_wb_ms >= 300)
                    log_warn("[IDLEWAIT] blocked %llu ms waiting for input",
                        (unsigned long long)_wb_ms);
                if (_wb_got) {
                    sdl_handle_event(&g_state, &ev);
                    /*
                     * SDL_WaitEvent() removes one event only. Returning to
                     * inkey() after every mouse-motion, timer, or window event
                     * made a queued movement key wait behind repeated frontend
                     * and presentation passes. Preserve event order, but drain
                     * everything already queued in this one input pass.
                     */
                    while (SDL_PollEvent(&ev))
                        sdl_handle_event(&g_state, &ev);
                }
            }
            g_sdl_blocking_key_wait = old_blocking_key_wait;
            Uint64 flush_ns = SDL_GetTicksNS();
            sdl_gamepad_flush_pending_dpad(flush_ns, false);
            sdl_gamepad_flush_pending_left_stick(flush_ns, false);
            sdl_gamepad_flush_pending_shoulder(flush_ns, false);
            sdl_gamepad_flush_pending_confirm(flush_ns);
            sdl_screen_back_gesture_flush_pending_press(flush_ns);
            sdl_touch_pane_flush_pending_press(flush_ns);
            sdl_menu_touch_flush_pending_press(flush_ns);
            sdl_character_panel_flush_pending_press(flush_ns);
            sdl_player_action_menu_flush_pending_press(flush_ns);
            sdl_pointer_attack_flush_pending_press(flush_ns);
            sdl_map_touch_flush_pending_press(flush_ns);
            sdl_object_tooltip_flush_expired(flush_ns);
            sdl_question_menu_flush_expired(flush_ns);
            sdl_touch_zone_flush_pending_press(flush_ns);
            sdl_touch_top_panel_flush_pending_press(flush_ns);
            sdl_touch_thumb_flush_pending_press(flush_ns);
            sdl_log_pane_menu_flush_pending_press(flush_ns);
            sdl_side_pane_menu_flush_pending_press(flush_ns);
            sdl_touch_round_flush_pending_highlight(flush_ns);
            sdl_music_update(); /* Update music after handling event */
        } else {
            /* Non-blocking scan so animation loops (intro fades, etc.) keep running */
            bool handled = false;
            sdl_music_update(); /* Update music streams */
            while (SDL_PollEvent(&ev)) {
                handled = true;
                sdl_handle_event(&g_state, &ev);
            }
            Uint64 flush_ns = SDL_GetTicksNS();
            sdl_gamepad_flush_pending_dpad(flush_ns, false);
            sdl_gamepad_flush_pending_left_stick(flush_ns, false);
            sdl_gamepad_flush_pending_shoulder(flush_ns, false);
            sdl_gamepad_flush_pending_confirm(flush_ns);
            sdl_screen_back_gesture_flush_pending_press(flush_ns);
            sdl_touch_pane_flush_pending_press(flush_ns);
            sdl_menu_touch_flush_pending_press(flush_ns);
            sdl_character_panel_flush_pending_press(flush_ns);
            sdl_player_action_menu_flush_pending_press(flush_ns);
            sdl_pointer_attack_flush_pending_press(flush_ns);
            sdl_map_touch_flush_pending_press(flush_ns);
            sdl_object_tooltip_flush_expired(flush_ns);
            sdl_question_menu_flush_expired(flush_ns);
            sdl_touch_zone_flush_pending_press(flush_ns);
            sdl_touch_top_panel_flush_pending_press(flush_ns);
            sdl_touch_thumb_flush_pending_press(flush_ns);
            sdl_log_pane_menu_flush_pending_press(flush_ns);
            sdl_side_pane_menu_flush_pending_press(flush_ns);
            sdl_touch_round_flush_pending_highlight(flush_ns);

            /* Avoid pegging a CPU core when we're repeatedly asked to poll */
            if (!handled) {
                sdl_mono_font_prewarm_process_idle();
                SDL_Delay(1);
            }
        }
        sdl_present_if_needed(d);
        return 0;
    }
    case TERM_XTRA_FLUSH:
        // Flush pending input events (drain queue)
        {
            SDL_Event ev;
            while (SDL_PollEvent(&ev))
                sdl_handle_event(&g_state, &ev);
        }
        {
            Uint64 flush_ns = SDL_GetTicksNS();
            sdl_gamepad_flush_pending_confirm(flush_ns);
            sdl_screen_back_gesture_flush_pending_press(flush_ns);
            sdl_touch_pane_flush_pending_press(flush_ns);
            sdl_menu_touch_flush_pending_press(flush_ns);
            sdl_character_panel_flush_pending_press(flush_ns);
            sdl_player_action_menu_flush_pending_press(flush_ns);
            sdl_pointer_attack_flush_pending_press(flush_ns);
            sdl_map_touch_flush_pending_press(flush_ns);
            sdl_object_tooltip_flush_expired(flush_ns);
            sdl_question_menu_flush_expired(flush_ns);
            sdl_touch_zone_flush_pending_press(flush_ns);
            sdl_touch_top_panel_flush_pending_press(flush_ns);
            sdl_touch_thumb_flush_pending_press(flush_ns);
            sdl_log_pane_menu_flush_pending_press(flush_ns);
            sdl_side_pane_menu_flush_pending_press(flush_ns);
        }
        sdl_present_if_needed(d);
        return 0;
    case TERM_XTRA_CLEAR:
        if (!d || !d->canvas)
            return 0;
        SDL_SetRenderTarget(g_state.renderer, d->canvas);
        SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0,
            sdl_view_background_alpha(d));
        SDL_RenderClear(g_state.renderer);
        SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
        g_state.need_present = true;
        return 0;
    case TERM_XTRA_FRESH:
        sdl_present_if_needed(d);
        return 0;
    case TERM_XTRA_DELAY: {
        /* Break delay into chunks and process events to keep app responsive */
        Uint32 total_delay = (Uint32)v;
        Uint32 chunk = 20; /* Process events every 20ms */
        
        while (total_delay > 0) {
            Uint32 this_delay = (total_delay < chunk) ? total_delay : chunk;
            SDL_Delay(this_delay);
            total_delay -= this_delay;
            
            /* Update music streams */
            sdl_music_update();
            
            /* Process pending events to prevent "Not Responding" status */
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                sdl_handle_event(&g_state, &ev);
            }
            {
                Uint64 flush_ns = SDL_GetTicksNS();
                sdl_gamepad_flush_pending_confirm(flush_ns);
                sdl_screen_back_gesture_flush_pending_press(flush_ns);
                sdl_touch_pane_flush_pending_press(flush_ns);
                sdl_menu_touch_flush_pending_press(flush_ns);
                sdl_character_panel_flush_pending_press(flush_ns);
                sdl_player_action_menu_flush_pending_press(flush_ns);
                sdl_pointer_attack_flush_pending_press(flush_ns);
                sdl_map_touch_flush_pending_press(flush_ns);
                sdl_object_tooltip_flush_expired(flush_ns);
                sdl_question_menu_flush_expired(flush_ns);
                sdl_touch_zone_flush_pending_press(flush_ns);
                sdl_touch_top_panel_flush_pending_press(flush_ns);
                sdl_touch_thumb_flush_pending_press(flush_ns);
                sdl_log_pane_menu_flush_pending_press(flush_ns);
                sdl_side_pane_menu_flush_pending_press(flush_ns);
            }
        }
        return 0;
    }
    case TERM_XTRA_REACT:
        /* React to global setting changes (graphics mode, colors, etc.) */
        log_debug("TERM_XTRA_REACT received (tiles_mode=%d use_graphics=%d arg_graphics=%d)",
                  g_state.use_tiles, use_graphics, arg_graphics);
        /* Reload colors from the active palette in angband_color_table. */
        sdl_sync_palette();
        reset_visuals(true);
        return 0;
    default:
        return 0;
    }
}

void draw_cursor(int x, int y, bool big)
{
    sdl_view* d = sdl_view_from_term(Term);
    if (!d || !d->canvas)
        return;
    if (!Term)
        return;
    /* While a monster/item description overlay is up, the underlying term's
     * cursor would peek out on the first row.  Suppress it. */
    if (g_description_overlay.active)
        return;
    if (x < 0 || y < 0 || x >= Term->wid || y >= Term->hgt)
        return;
    SDL_SetRenderTarget(g_state.renderer, d->canvas);
    SDL_Rect clip = { x * d->cell_w, y * d->cell_h, d->cell_w * (big + 1), d->cell_h };
    SDL_SetRenderClipRect(g_state.renderer, &clip);
    SDL_FRect r = { x * d->cell_w, y * d->cell_h, d->cell_w * (big + 1), d->cell_h };
    SDL_SetRenderDrawColor(g_state.renderer, 0, 255, 255, 255);
    SDL_RenderRect(g_state.renderer, &r);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    g_state.need_present = true;
}

errr callback_sdl_curs(int x, int y)
{
    draw_cursor(x, y, false);
    return 0;
}

errr callback_sdl_bigcurs(int x, int y)
{
    draw_cursor(x, y, true);
    return 0;
}

errr callback_sdl_wipe(int x, int y, int n)
{
    sdl_view* d = sdl_view_from_term(Term);
    if (!d || !d->canvas || !Term || n <= 0)
        return 0;
    if (x < 0 || y < 0 || x >= Term->wid || y >= Term->hgt)
        return 0;
    if (x + n > Term->wid)
        n = Term->wid - x;
    if (n <= 0)
        return 0;
    SDL_SetRenderTarget(g_state.renderer, d->canvas);
    SDL_Rect clip = { x * d->cell_w, y * d->cell_h, n * d->cell_w, d->cell_h };
    SDL_SetRenderClipRect(g_state.renderer, &clip);
    SDL_FRect r = { x * d->cell_w, y * d->cell_h, n * d->cell_w, d->cell_h };
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0,
        sdl_view_background_alpha(d));
    SDL_RenderFillRect(g_state.renderer, &r);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    g_state.need_present = true;
    return 0;
}

errr callback_sdl_text(int x, int y, int n, byte a, cptr s)
{
    sdl_view* d = sdl_view_from_term(Term);
    bool selected_attr = (a >= TERM_UI_SELECTED);
    byte selected_bg_attr = selected_attr ? (byte)(a - TERM_UI_SELECTED)
                                          : TERM_DARK;
    byte fg_attr = selected_attr ? TERM_DARK : a;
    SDL_Color bg_col = { 0, 0, 0, 255 };
    if (!d || !d->canvas || !Term || !s || n <= 0)
        return 0;
    if (x < 0 || y < 0 || x >= Term->wid || y >= Term->hgt)
        return 0;
    if (x + n > Term->wid)
        n = Term->wid - x;
    if (n <= 0)
        return 0;
    SDL_SetRenderTarget(g_state.renderer, d->canvas);
    bg_col.a = selected_attr ? 255 : sdl_view_background_alpha(d);

    TTF_Font* story_font = sdl_story_font_for_view(d);
    if (selected_attr)
    {
        bg_col.r = angband_color_table[selected_bg_attr][1];
        bg_col.g = angband_color_table[selected_bg_attr][2];
        bg_col.b = angband_color_table[selected_bg_attr][3];
    }

    // Check if any character in this chunk should use story font
    // First check the global chunk flag (for whole-line story rendering)
    bool chunk_story_font = (Term && Term->story_chunk_active && story_font);
    
    // Also check per-character story font flags
    if (!chunk_story_font && Term && Term->scr && story_font) {
        // Check if ANY character in this chunk (from x to x+n) has the story font flag
        // story is a byte** (2D array), so we need story[y] which gives us byte* for that row
        if (y >= 0 && y < Term->hgt && Term->scr->story && Term->scr->story[y]) {
            // Check all characters in the chunk, not just the first one
            for (int i = 0; i < n && (x + i) < Term->wid; i++) {
                if (Term->scr->story[y][x + i]) {
                    chunk_story_font = true;
                    log_trace("callback_sdl_text: Using story font based on per-char flag at y=%d x=%d (chunk starts at x=%d)",
                              y, x + i, x);
                    break;
                }
            }
        }
    }

    bool story_mode = (chunk_story_font && story_font);

    if (!story_mode) {
        // Clear destination cell span so shorter/narrower glyphs don't leave leftovers
        SDL_Rect clip = { x * d->cell_w, y * d->cell_h, n * d->cell_w, d->cell_h };
        SDL_SetRenderClipRect(g_state.renderer, &clip);
        SDL_FRect bg = {
            (float)(x * d->cell_w),
            (float)(y * d->cell_h),
            (float)(n * d->cell_w),
            (float)(d->cell_h)
        };
        SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(g_state.renderer, bg_col.r, bg_col.g,
            bg_col.b, bg_col.a);
        SDL_RenderFillRect(g_state.renderer, &bg);
        SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderClipRect(g_state.renderer, NULL);
    } else {
        SDL_SetRenderClipRect(g_state.renderer, NULL);
    }

    /* Use extended color table to support shaded colors (indices 0-255) */
    SDL_Color col;
    col.r = angband_color_table[fg_attr][1];
    col.g = angband_color_table[fg_attr][2];
    col.b = angband_color_table[fg_attr][3];
    col.a = 255;
    
    // Special logging for line 0 (top description line in unified look)
    if (y == 0) {
        log_trace("callback_sdl_text ROW 0: x=%d n=%d chunk_story=%d text='%.*s'",
                  x, n, chunk_story_font, n, s);
    }
    
    // Special logging for the shooting row (y=1 when 0-indexed, or the second row)
    if (y == 1 || y == 2) {
        log_trace("callback_sdl_text ROW %d: chunk_story=%d chunk_active=%d",
                  y, chunk_story_font,
                  (Term && Term->story_chunk_active) ? 1 : 0);
    }
    
    log_trace("callback_sdl_text: chunk_story_font=%s term=%p chunk_flag=%s depth=%d font=%p",
              chunk_story_font ? "true" : "false",
              (void*)Term,
              (Term && Term->story_chunk_active) ? "true" : "false",
              g_state.story_font_depth,
              (void*)story_font);

    byte* story_row = NULL;
    char* row_chars = NULL;
    byte* row_attr = NULL;
    if (Term && Term->scr && y >= 0 && y < Term->hgt) {
        if (Term->scr->story)
            story_row = Term->scr->story[y];
        if (Term->scr->c)
            row_chars = Term->scr->c[y];
        if (Term->scr->a)
            row_attr = Term->scr->a[y];
    }

    if (story_mode) {
        if (story_row) {
            /* Story "free" text must be pixel-packed across color runs.
             * Rendering per-cell or per-run leaves large gaps with proportional fonts. */
            if (row_chars && row_attr)
            {
                sdl_render_story_row_packed(d, story_font, y, story_row, row_chars, row_attr);
                g_state.need_present = true;
                return 0;
            }

            int offset = 0;
            while (offset < n && (x + offset) < Term->wid) {
                int term_col = x + offset;
                byte flags = story_row[term_col];
                bool use_story = (flags & STORY_FLAG_USE) != 0;
                bool grid_align = (flags & STORY_FLAG_CELL_ALIGN) != 0;
                bool slot2 = (flags & STORY_FLAG_SLOT2) != 0;
                TTF_Font* chunk_font = slot2
                    ? sdl_story_font_for_view_slot(d, STORY_FONT_SLOT_SECONDARY)
                    : story_font;

                if (!chunk_font)
                    chunk_font = story_font;

                int chunk_remaining = n - offset;
                int chunk_run = 1;
                while ((chunk_run < chunk_remaining) && (term_col + chunk_run) < Term->wid) {
                    byte next_flags = story_row[term_col + chunk_run];
                    bool next_story = (next_flags & STORY_FLAG_USE) != 0;
                    bool next_grid = (next_flags & STORY_FLAG_CELL_ALIGN) != 0;
                    bool next_slot2 = (next_flags & STORY_FLAG_SLOT2) != 0;
                    if (next_story != use_story)
                        break;
                    if (next_grid != grid_align)
                        break;
                    if (next_slot2 != slot2)
                        break;
                    if (row_attr && row_attr[term_col + chunk_run] != a)
                        break;
                    chunk_run++;
                }

                bool can_extend_story = use_story && row_chars;
                int render_col = term_col;
                int render_end = term_col + chunk_run;

                if (can_extend_story) {
                    while (render_col > 0) {
                        byte prev_flags = story_row[render_col - 1];
                        bool prev_story = (prev_flags & STORY_FLAG_USE) != 0;
                        bool prev_grid = (prev_flags & STORY_FLAG_CELL_ALIGN) != 0;
                        bool prev_slot2 = (prev_flags & STORY_FLAG_SLOT2) != 0;
                        if (!prev_story || prev_grid != grid_align || prev_slot2 != slot2)
                            break;
                        if (row_attr && row_attr[render_col - 1] != a)
                            break;
                        render_col--;
                    }
                    while (render_end < Term->wid) {
                        byte next_flags = story_row[render_end];
                        bool next_story = (next_flags & STORY_FLAG_USE) != 0;
                        bool next_grid = (next_flags & STORY_FLAG_CELL_ALIGN) != 0;
                        bool next_slot2 = (next_flags & STORY_FLAG_SLOT2) != 0;
                        if (!next_story || next_grid != grid_align || next_slot2 != slot2)
                            break;
                        if (row_attr && row_attr[render_end] != a)
                            break;
                        render_end++;
                    }
                }

                int render_run = render_end - render_col;
                const char* render_text = (can_extend_story && row_chars) ? (row_chars + render_col) : (s + offset);

                SDL_FRect clear_rect = {
                    (float)(render_col * d->cell_w),
                    (float)(y * d->cell_h),
                    (float)(render_run * d->cell_w),
                    (float)d->cell_h
                };
                SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_NONE);
                SDL_SetRenderDrawColor(g_state.renderer, bg_col.r,
                    bg_col.g, bg_col.b, bg_col.a);
                SDL_RenderFillRect(g_state.renderer, &clear_rect);
                SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);

                if (use_story) {
                    if (grid_align)
                        sdl_render_story_text_grid(d, chunk_font, render_col, y, render_run, render_text, col);
                    else
                        sdl_render_story_text_free(d, chunk_font, render_col, y, render_run, render_text, col);
                } else {
                    if (utf8_has_non_ascii_n(render_text, render_run)) {
                        const char* font_path = config.monospace_font[0] != '\0'
                            ? config.monospace_font
                            : "lib/xtra/font/VictorMono-Medium.ttf";
                        TTF_Font* mono_font = sdl_acquire_mono_font_cells(font_path,
                            d->cell_w, d->cell_h);

                        if (mono_font)
                            sdl_render_mono_utf8_text_cells(d->font_atlas,
                                (d->font_atlas_cell_w > 0) ? d->font_atlas_cell_w : d->cell_w,
                                (d->font_atlas_cell_h > 0) ? d->font_atlas_cell_h : d->cell_h,
                                mono_font,
                                (float)d->cell_w, (float)d->cell_h, 0.0f,
                                render_col, y, render_run, render_text, col);
                        else
                            sdl_render_mono_text(d, render_col, y, render_run,
                                render_text, col);
                    } else {
                        sdl_render_mono_text(d, render_col, y, render_run, render_text, col);
                    }
                }

                offset += chunk_run;
            }
        } else {
            SDL_FRect clear_rect = {
                (float)(x * d->cell_w),
                (float)(y * d->cell_h),
                (float)(n * d->cell_w),
                (float)d->cell_h
            };
            SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(g_state.renderer, bg_col.r, bg_col.g,
                bg_col.b, bg_col.a);
            SDL_RenderFillRect(g_state.renderer, &clear_rect);
            SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
            sdl_render_story_text_free(d, story_font, x, y, n, s, col);
        }
    } else {
        if (y == 1 || y == 2) {
            log_trace("callback_sdl_text: USING MONO FONT for row %d: '%.30s'", y, s);
        }
        if (utf8_has_non_ascii_n(s, n)) {
            const char* font_path = config.monospace_font[0] != '\0'
                ? config.monospace_font
                : "lib/xtra/font/VictorMono-Medium.ttf";
            TTF_Font* mono_font = sdl_acquire_mono_font_cells(font_path,
                d->cell_w, d->cell_h);

            if (mono_font)
                sdl_render_mono_utf8_text_cells(d->font_atlas,
                    (d->font_atlas_cell_w > 0) ? d->font_atlas_cell_w : d->cell_w,
                    (d->font_atlas_cell_h > 0) ? d->font_atlas_cell_h : d->cell_h,
                    mono_font, (float)d->cell_w,
                    (float)d->cell_h, 0.0f, x, y, n, s, col);
            else
                sdl_render_mono_text(d, x, y, n, s, col);
        } else {
            sdl_render_mono_text(d, x, y, n, s, col);
        }
    }

    g_state.need_present = true;
    return 0;
}

void sdl_draw_tileset_sprite_ex(byte a, char c, const SDL_FRect* dst,
    bool icon, SDL_FlipMode flip)
{
    int mask;
    SDL_FRect src;

    if (!g_state.tileset || !dst)
        return;

    mask = icon ? 0x7F : TILE_INDEX_MASK;
    src.x = (float)(((byte)c & mask) * TILE_SIZE);
    src.y = (float)((a & mask) * TILE_SIZE);
    src.w = TILE_SIZE;
    src.h = TILE_SIZE;
    if (flip == SDL_FLIP_NONE)
        SDL_RenderTexture(g_state.renderer, g_state.tileset, &src, dst);
    else
        SDL_RenderTextureRotated(g_state.renderer, g_state.tileset, &src,
            dst, 0.0, NULL, flip);
}

void sdl_draw_tileset_sprite(byte a, char c, const SDL_FRect* dst,
    bool icon)
{
    sdl_draw_tileset_sprite_ex(a, c, dst, icon, SDL_FLIP_NONE);
}

bool sdl_map_grid_is_player(int y, int x)
{
    return p_ptr && (y >= 0) && (x >= 0) && (y < p_ptr->cur_map_hgt)
        && (x < p_ptr->cur_map_wid) && (cave_m_idx[y][x] < 0);
}

enum {
    SDL_PLAYER_TILE_LEFT_ROW_ELF = 13,
    SDL_PLAYER_TILE_LEFT_ROW_DWARF_MAN = 14,
    SDL_PLAYER_TILE_RIGHT_ROW_ELF = 24,
    SDL_PLAYER_TILE_RIGHT_ROW_DWARF_MAN = 25
};

bool sdl_player_tile_directional_enabled(void)
{
    return op_ptr && mirror_player_tile_facing;
}

bool sdl_player_tile_handcrafted_enabled(void)
{
    return op_ptr && mirror_player_tile_facing && handcrafted_player_tile_facing;
}

bool sdl_player_tile_apply_horizontal_facing(int dir)
{
    switch (dir) {
    case 1:
    case 4:
    case 7:
        g_player_tile_facing_right = false;
        return true;
    case 3:
    case 6:
    case 9:
        g_player_tile_facing_right = true;
        return true;
    default:
        return false;
    }
}

bool sdl_player_tile_facing_right(void)
{
    int dir;

    if (!p_ptr || !sdl_player_tile_directional_enabled()) {
        g_player_tile_facing_right = false;
        return false;
    }

    if (playerturn <= 1 && p_ptr->previous_action[0] == ACTION_NOTHING
        && p_ptr->previous_action[1] == ACTION_NOTHING)
    {
        g_player_tile_facing_right = false;
    }

    dir = p_ptr->visual_facing_dir;
    if (sdl_player_tile_apply_horizontal_facing(dir))
        return g_player_tile_facing_right;

    dir = p_ptr->previous_action[0];
    (void)sdl_player_tile_apply_horizontal_facing(dir);

    return g_player_tile_facing_right;
}

byte sdl_player_tile_handcrafted_right_attr(byte a)
{
    switch (TILE_GET_INDEX(a))
    {
    case SDL_PLAYER_TILE_LEFT_ROW_ELF:
        return TILE_SET_INDEX(a, SDL_PLAYER_TILE_RIGHT_ROW_ELF);
    case SDL_PLAYER_TILE_LEFT_ROW_DWARF_MAN:
        return TILE_SET_INDEX(a, SDL_PLAYER_TILE_RIGHT_ROW_DWARF_MAN);
    default: return a;
    }
}

static byte sdl_horizontal_facing_from_dir(int dir)
{
    switch (dir) {
    case 1:
    case 4:
    case 7:
        return MONSTER_TILE_FACING_LEFT;
    case 3:
    case 6:
    case 9:
        return MONSTER_TILE_FACING_RIGHT;
    default:
        return MONSTER_TILE_FACING_NONE;
    }
}

static bool sdl_monster_tile_should_flip(int y, int x, byte a, char c)
{
    int m_idx;
    int r_idx;
    monster_type* m_ptr;
    monster_race* r_ptr;
    byte current_facing;

    if (!op_ptr || !mirror_monster_tile_facing || !p_ptr || !mon_list
        || !r_info || !z_info)
        return false;

    if ((y < 0) || (x < 0) || (y >= p_ptr->cur_map_hgt)
        || (x >= p_ptr->cur_map_wid))
        return false;

    m_idx = cave_m_idx[y][x];
    if (m_idx <= 0)
        return false;

    m_ptr = &mon_list[m_idx];
    if (!m_ptr->ml)
        return false;

    r_idx = (p_ptr->image) ? m_ptr->image_r_idx : m_ptr->r_idx;
    if ((r_idx <= 0) || (r_idx >= z_info->r_max))
        return false;

    r_ptr = &r_info[r_idx];
    if (r_ptr->tile_facing == MONSTER_TILE_FACING_NONE)
        return false;
    if (!(r_ptr->x_attr & TILE_FLAG) || !(((byte)r_ptr->x_char) & TILE_FLAG)
        || (TILE_GET_INDEX(a) != TILE_GET_INDEX(r_ptr->x_attr))
        || (TILE_GET_INDEX((byte)c) != TILE_GET_INDEX((byte)r_ptr->x_char)))
    {
        return false;
    }

    current_facing = sdl_horizontal_facing_from_dir(m_ptr->visual_facing_dir);
    if (current_facing == MONSTER_TILE_FACING_NONE)
        current_facing = sdl_horizontal_facing_from_dir(m_ptr->previous_action[0]);
    if (current_facing == MONSTER_TILE_FACING_NONE)
        return false;

    return current_facing != r_ptr->tile_facing;
}

void sdl_draw_ascii_minimap_cell(byte a, char c, byte ta, char tc,
    const SDL_FRect* dst)
{
    byte draw_a = a;
    char draw_c = c;

    if (!dst)
        return;

    if ((byte)draw_c == ' ' && (byte)tc != ' ') {
        draw_a = ta;
        draw_c = tc;
    }

    if ((byte)draw_c == ' ')
        return;

    SDL_SetRenderDrawColor(g_state.renderer, angband_color_table[draw_a][1],
        angband_color_table[draw_a][2], angband_color_table[draw_a][3], 255);
    SDL_RenderFillRect(g_state.renderer, dst);
}

bool sdl_rage_grid_filter_active(int y, int x)
{
    u16b info;

    if (!p_ptr || p_ptr->is_dead || !p_ptr->rage)
        return false;
    if ((y < 0) || (x < 0) || (y >= p_ptr->cur_map_hgt)
        || (x >= p_ptr->cur_map_wid))
    {
        return false;
    }
    info = cave_info[y][x];
    if (!(info & (CAVE_MARK | CAVE_SEEN)))
        return false;
    if ((p_ptr->rage || g_labyrinth_view_active) && !(info & CAVE_SEEN))
        return false;

    return true;
}

bool sdl_rage_wall_tint_active(int y, int x)
{
    byte feat;

    if (!sdl_rage_grid_filter_active(y, x))
        return false;

    feat = f_info[cave_feat[y][x]].mimic;
    return (feat >= FEAT_WALL_HEAD) && (feat <= FEAT_WALL_TAIL)
        && (feat != FEAT_RUBBLE);
}

bool sdl_rage_floor_tint_active(int y, int x)
{
    byte feat;

    if (!sdl_rage_grid_filter_active(y, x))
        return false;

    if (cave_floorlike_bold(y, x))
        return true;

    feat = f_info[cave_feat[y][x]].mimic;
    return ((feat >= FEAT_TRAP_HEAD) && (feat <= FEAT_TRAP_TAIL))
        || ((feat >= FEAT_STAIR_HEAD) && (feat <= FEAT_STAIR_TAIL))
        || ((feat >= FEAT_FORGE_HEAD) && (feat <= FEAT_FORGE_TAIL))
        || (feat == FEAT_SUNLIGHT)
        || (feat == FEAT_RUBBLE);
}

bool sdl_rage_visible_floor_object(int y, int x)
{
    object_type* o_ptr;
    byte feat;

    if (!sdl_rage_grid_filter_active(y, x))
        return false;

    feat = f_info[cave_feat[y][x]].mimic;
    if ((feat != FEAT_FLOOR) && (feat != FEAT_SUNLIGHT))
        return false;

    for (o_ptr = get_first_object(y, x); o_ptr;
         o_ptr = get_next_object(o_ptr))
    {
        if (o_ptr->marked)
            return true;
    }

    return false;
}

bool sdl_rage_base_floor_tint_active(int y, int x)
{
    if (!sdl_rage_floor_tint_active(y, x))
        return false;

    return cave_m_idx[y][x] == 0 && !sdl_rage_visible_floor_object(y, x)
        && cave_floorlike_bold(y, x);
}

bool sdl_rage_base_object_tint_active(int y, int x)
{
    if (!sdl_rage_visible_floor_object(y, x))
        return false;

    return cave_m_idx[y][x] == 0;
}

u32b sdl_rage_wall_filter_hash(int y, int x, u32b phase)
{
    u32b h = (u32b)x * 374761393u;

    h ^= (u32b)y * 668265263u;
    h ^= phase * 2246822519u;
    h ^= h >> 13;
    h *= 3266489917u;
    h ^= h >> 16;
    return h;
}

void sdl_restore_tileset_mod(void)
{
    if (!g_state.tileset)
        return;

    SDL_SetTextureColorMod(g_state.tileset, 255, 255, 255);
    SDL_SetTextureAlphaMod(g_state.tileset, 255);
}

SDL_Rect sdl_frect_to_clip_rect(const SDL_FRect* rect)
{
    SDL_Rect clip = { 0, 0, 0, 0 };

    if (!rect)
        return clip;

    clip.x = (int)rect->x;
    clip.y = (int)rect->y;
    clip.w = (int)(rect->w + 0.5f);
    clip.h = (int)(rect->h + 0.5f);
    return clip;
}

void sdl_draw_rage_tile_filter(byte a, char c, int y, int x,
    const SDL_FRect* dst)
{
    SDL_Color tint = g_state.palette[TERM_RED];
    SDL_Rect old_clip;
    SDL_Rect base_clip;
    bool had_clip;
    u32b phase;
    u32b hash;

    if (!dst)
        return;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, tint.r, tint.g, tint.b, 24);
    SDL_RenderFillRect(g_state.renderer, dst);

    if (!g_state.tileset || !(a & TILE_FLAG) || !(((byte)c) & TILE_FLAG))
        return;

    had_clip = SDL_RenderClipEnabled(g_state.renderer);
    if (had_clip)
        SDL_GetRenderClipRect(g_state.renderer, &old_clip);

    base_clip = sdl_frect_to_clip_rect(dst);
    if (base_clip.w <= 0 || base_clip.h <= 0)
        return;
    if (had_clip) {
        SDL_Rect clipped;

        if (!SDL_GetRectIntersection(&base_clip, &old_clip, &clipped))
            return;
        base_clip = clipped;
    }

    phase = (u32b)(SDL_GetTicks() / 120u);
    hash = sdl_rage_wall_filter_hash(y, x, phase);

    SDL_SetTextureColorMod(g_state.tileset, 255, 96, 80);
    SDL_SetTextureAlphaMod(g_state.tileset, 34);

    for (int band = 0; band < 4; band++) {
        SDL_Rect band_clip = base_clip;
        SDL_FRect shifted = *dst;
        SDL_Rect final_clip;
        int y0 = base_clip.y + (base_clip.h * band) / 4;
        int y1 = base_clip.y + (base_clip.h * (band + 1)) / 4;
        int offset_pick = (int)((hash >> (band * 5)) & 0x03);
        int offset = 0;

        if (offset_pick == 0)
            offset = -1;
        else if (offset_pick == 1)
            offset = 1;
        else if ((band == 1) || (band == 3))
            offset = ((hash >> (band + 17)) & 0x01) ? 1 : -1;

        band_clip.y = y0;
        band_clip.h = y1 - y0;
        if (band_clip.h <= 0)
            continue;

        if (had_clip) {
            if (!SDL_GetRectIntersection(&band_clip, &old_clip, &final_clip))
                continue;
        } else {
            final_clip = band_clip;
        }

        SDL_SetRenderClipRect(g_state.renderer, &final_clip);
        shifted.x += (float)offset;
        sdl_draw_tileset_sprite(a, c, &shifted, false);
    }

    SDL_SetRenderClipRect(g_state.renderer, had_clip ? &old_clip : NULL);
    sdl_restore_tileset_mod();
}

/* A trap the player has rewired (re-keyed to catch monsters). */
static bool sdl_rewired_trap_tint_active(int y, int x)
{
    if ((y < 0) || (x < 0) || !cave_rewired)
        return false;

    return cave_rewired[y][x] && cave_trap_bold(y, x);
}

/* A steady violet wash marks a rewired trap in graphical tile mode, the
 * counterpart of the ASCII glyph recolour done in map_info(). */
static void sdl_draw_rewired_trap_tint(const SDL_FRect* dst)
{
    SDL_Color tint = g_state.palette[TERM_VIOLET];

    if (!dst)
        return;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, tint.r, tint.g, tint.b, 90);
    SDL_RenderFillRect(g_state.renderer, dst);
}

void sdl_draw_map_tile_layers_at(int dy, int dx, byte a, char c, byte ta,
    char tc, const SDL_FRect* dst)
{
    bool terrain_tile = (ta & TILE_FLAG) && (((byte)tc) & TILE_FLAG);
    bool base_tile = (a & TILE_FLAG) && (((byte)c) & TILE_FLAG);
    bool ui_background = !terrain_tile && ta >= TERM_UI_SELECTED && tc == ' ';
    bool glow = (a & GRAPHICS_GLOW_MASK) != 0;
    bool alert = (((byte)c) & GRAPHICS_ALERT_MASK) != 0;
    bool seen = (((byte)tc) & GRAPHICS_SEEN_MASK) != 0;
    bool sleep = !ui_background && (ta & GRAPHICS_SLEEP_MASK) != 0;
    bool tile_mode = g_state.use_tiles && g_state.tileset;

    if (!dst)
        return;

    if (ui_background)
    {
        byte bg_attr = (byte)(ta - TERM_UI_SELECTED);

        SDL_SetRenderDrawColor(g_state.renderer,
            angband_color_table[bg_attr][1],
            angband_color_table[bg_attr][2],
            angband_color_table[bg_attr][3], 255);
    }
    else
    {
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    }
    SDL_RenderFillRect(g_state.renderer, dst);

    if (!tile_mode) {
        sdl_draw_ascii_minimap_cell(a, c, ta, tc, dst);
        return;
    }

    if (!terrain_tile && !base_tile)
        return;

    /* Terrain underlay */
    if (terrain_tile)
        sdl_draw_tileset_sprite(ta, tc, dst, false);
    if (sdl_rage_wall_tint_active(dy, dx) && (cave_m_idx[dy][dx] != 0))
        sdl_draw_rage_tile_filter(ta, tc, dy, dx, dst);
    else if (sdl_rage_floor_tint_active(dy, dx))
        sdl_draw_rage_tile_filter(ta, tc, dy, dx, dst);

    if ((dy >= 0) && (dx >= 0) && p_ptr && (dy < p_ptr->cur_map_hgt)
        && (dx < p_ptr->cur_map_wid))
    {
        u16b info = cave_info[dy][dx];
        bool hide_square = (!p_ptr->is_dead)
            && (p_ptr->rage || g_labyrinth_view_active)
            && !(info & (CAVE_SEEN));

        if (!hide_square) {
            s16b m_idx = cave_m_idx[dy][dx];
            bool creature_visible = (m_idx < 0)
                || ((m_idx > 0) && mon_list[m_idx].ml);

            if (creature_visible && (info & (CAVE_MARK))) {
                byte feat = cave_feat[dy][dx];
                feat = f_info[feat].mimic;

                if (((feat >= FEAT_TRAP_HEAD) && (feat <= FEAT_TRAP_TAIL))
                    || ((feat >= FEAT_STAIR_HEAD) && (feat <= FEAT_STAIR_TAIL))
                    || ((feat >= FEAT_FORGE_HEAD) && (feat <= FEAT_FORGE_TAIL))
                    || (feat == FEAT_SUNLIGHT))
                {
                    feature_type* f_ptr = &f_info[feat];
                    byte feat_a = f_ptr->x_attr;
                    char feat_c = f_ptr->x_char;

                    if ((use_graphics == GRAPHICS_MICROCHASM)
                        && feat_supports_lighting(feat)) {
                        bool is_dark = p_ptr->blind
                            || ((cave_light[dy][dx] <= 0)
                                && !(info & (CAVE_GLOW)));
                        if (is_dark || !(info & (CAVE_SEEN)))
                            feat_c += 1;
                    }

                    sdl_draw_tileset_sprite(feat_a, feat_c, dst, false);
                }
            }

            /* Keep a floor item visible beneath the player tile. */
            if (m_idx < 0) {
                byte feat = cave_feat[dy][dx];

                if ((feat == FEAT_FLOOR) || (feat == FEAT_SUNLIGHT)) {
                    object_type* o_ptr;

                    for (o_ptr = get_first_object(dy, dx); o_ptr;
                         o_ptr = get_next_object(o_ptr)) {
                        if (o_ptr->marked) {
                            byte obj_a = object_attr(o_ptr);
                            byte obj_c = (byte)object_char(o_ptr);

                            if ((obj_a & TILE_FLAG) && (obj_c & TILE_FLAG))
                            {
                                sdl_draw_tileset_sprite(obj_a, (char)obj_c,
                                    dst, false);
                                if (sdl_rage_visible_floor_object(dy, dx))
                                    sdl_draw_rage_tile_filter(obj_a,
                                        (char)obj_c, dy, dx, dst);
                            }

                            break;
                        }
                    }
                }
            }
        }
    }

    if (glow) {
        byte icon_a = misc_to_attr[ICON_GLOW];
        byte icon_c = (byte)misc_to_char[ICON_GLOW];
        if ((icon_a & TILE_FLAG) && (icon_c & TILE_FLAG))
            sdl_draw_tileset_sprite(icon_a, (char)icon_c, dst, true);
    }

    /* Base tile */
    if (base_tile) {
        byte draw_a = a;
        SDL_FlipMode flip = SDL_FLIP_NONE;

        if (sdl_map_grid_is_player(dy, dx) && sdl_player_tile_facing_right())
        {
            if (sdl_player_tile_handcrafted_enabled())
                draw_a = sdl_player_tile_handcrafted_right_attr(a);
            else
                flip = SDL_FLIP_HORIZONTAL;
        }
        else if (sdl_monster_tile_should_flip(dy, dx, a, c))
        {
            flip = SDL_FLIP_HORIZONTAL;
        }

        sdl_draw_tileset_sprite_ex(draw_a, c, dst, false, flip);
    }

    if (sdl_rage_wall_tint_active(dy, dx) && (cave_m_idx[dy][dx] == 0))
        sdl_draw_rage_tile_filter(a, c, dy, dx, dst);
    else if (sdl_rage_base_object_tint_active(dy, dx))
        sdl_draw_rage_tile_filter(a, c, dy, dx, dst);
    else if (sdl_rage_base_floor_tint_active(dy, dx))
        sdl_draw_rage_tile_filter(a, c, dy, dx, dst);

    /* Mark a rewired trap with a steady violet wash (tile mode) */
    if (sdl_rewired_trap_tint_active(dy, dx))
        sdl_draw_rewired_trap_tint(dst);

    if (sleep) {
        byte icon_a = misc_to_attr[ICON_SLEEPING];
        byte icon_c = (byte)misc_to_char[ICON_SLEEPING];
        if ((icon_a & TILE_FLAG) && (icon_c & TILE_FLAG))
            sdl_draw_tileset_sprite(icon_a, (char)icon_c, dst, true);
    }

    if (seen) {
        byte icon_a = misc_to_attr[ICON_MONSTER_SEES_PLAYER];
        byte icon_c = (byte)misc_to_char[ICON_MONSTER_SEES_PLAYER];
        if ((icon_a & TILE_FLAG) && (icon_c & TILE_FLAG))
            sdl_draw_tileset_sprite(icon_a, (char)icon_c, dst, true);
    }

    if (alert) {
        byte icon_a = misc_to_attr[ICON_ALERT];
        byte icon_c = (byte)misc_to_char[ICON_ALERT];
        if ((icon_a & TILE_FLAG) && (icon_c & TILE_FLAG))
            sdl_draw_tileset_sprite(icon_a, (char)icon_c, dst, true);
    }
}

bool sdl_minimap_hint_source_valid(const hint_message_meta* meta)
{
    return sdl_minimap_hint_source_in_bounds(meta);
}

void sdl_minimap_expand_bounds_for_hint_sources(int* min_y, int* min_x,
    int* max_y, int* max_x, bool* any)
{
    byte count;

    if (!min_y || !min_x || !max_y || !max_x || !any)
        return;

    count = hint_messages_count_for_save();
    for (int i = 0; i < count; i++) {
        hint_message_meta meta;

        hint_messages_message_meta(i, &meta);
        if (!sdl_minimap_hint_source_valid(&meta))
            continue;

        if (meta.source_y < *min_y) *min_y = meta.source_y;
        if (meta.source_y > *max_y) *max_y = meta.source_y;
        if (meta.source_x < *min_x) *min_x = meta.source_x;
        if (meta.source_x > *max_x) *max_x = meta.source_x;
        *any = true;
    }
}

const object_type* sdl_minimap_skeleton_at(int y, int x)
{
    object_type* o_ptr;

    if (!p_ptr || y < 0 || x < 0 || y >= p_ptr->cur_map_hgt
        || x >= p_ptr->cur_map_wid)
    {
        return NULL;
    }

    for (o_ptr = get_first_object(y, x); o_ptr; o_ptr = get_next_object(o_ptr)) {
        if (o_ptr->k_idx && o_ptr->tval == TV_SKELETON)
            return o_ptr;
    }

    return NULL;
}

void sdl_minimap_draw_hint_source_symbol(const object_type* o_ptr,
    const SDL_FRect* dst)
{
    byte obj_a;
    byte obj_c;

    if (!o_ptr || !dst)
        return;

    obj_a = object_attr(o_ptr);
    obj_c = (byte)object_char(o_ptr);

    if (g_state.use_tiles && g_state.tileset
        && (obj_a & TILE_FLAG) && (obj_c & TILE_FLAG))
    {
        sdl_draw_tileset_sprite(obj_a, (char)obj_c, dst, false);
        return;
    }

    sdl_draw_ascii_minimap_cell(obj_a, (char)obj_c, obj_a, (char)obj_c, dst);
}

void sdl_minimap_draw_hint_sources(const SDL_FRect* map_dst, int min_y,
    int min_x, int max_y, int max_x)
{
    byte count;
    int map_rows = max_y - min_y + 1;
    int map_cols = max_x - min_x + 1;
    float grid_w;
    float grid_h;

    if (!map_dst || map_rows <= 0 || map_cols <= 0)
        return;

    count = hint_messages_count_for_save();
    if (!count)
        return;

    grid_w = map_dst->w / (float)map_cols;
    grid_h = map_dst->h / (float)map_rows;

    for (int i = 0; i < count; i++) {
        hint_message_meta meta;
        SDL_FRect cell;
        SDL_FRect marker;
        float center_x;
        float center_y;
        float min_marker = 6.0f;
        bool focused;
        const object_type* skel;

        hint_messages_message_meta(i, &meta);
        if (!sdl_minimap_hint_source_valid(&meta))
            continue;
        if (meta.source_y < min_y || meta.source_y > max_y
            || meta.source_x < min_x || meta.source_x > max_x)
        {
            continue;
        }

        focused = g_minimap.focus_active
            && (meta.source_y == g_minimap.focus_y)
            && (meta.source_x == g_minimap.focus_x);
        if (focused)
            min_marker = 12.0f;

        cell.x = map_dst->x + (float)(meta.source_x - min_x) * grid_w;
        cell.y = map_dst->y + (float)(meta.source_y - min_y) * grid_h;
        cell.w = grid_w;
        cell.h = grid_h;

        center_x = cell.x + cell.w * 0.5f;
        center_y = cell.y + cell.h * 0.5f;
        marker = cell;
        if (marker.w < min_marker) {
            marker.w = min_marker;
            marker.x = center_x - marker.w * 0.5f;
        }
        if (marker.h < min_marker) {
            marker.h = min_marker;
            marker.y = center_y - marker.h * 0.5f;
        }

        if (focused)
            SDL_SetRenderDrawColor(g_state.renderer, 255, 230, 80, 104);
        else
            SDL_SetRenderDrawColor(g_state.renderer, 70, 220, 230, 64);
        SDL_RenderFillRect(g_state.renderer, &marker);

        skel = sdl_minimap_skeleton_at(meta.source_y, meta.source_x);
        sdl_minimap_draw_hint_source_symbol(skel, &marker);

        if (focused)
            SDL_SetRenderDrawColor(g_state.renderer, 255, 255, 255, 255);
        else
            SDL_SetRenderDrawColor(g_state.renderer, 255, 238, 120, 240);
        SDL_RenderRect(g_state.renderer, &marker);
        if (marker.w >= 8.0f && marker.h >= 8.0f) {
            SDL_FRect inner = {
                marker.x + 1.0f,
                marker.y + 1.0f,
                marker.w - 2.0f,
                marker.h - 2.0f
            };
            SDL_RenderRect(g_state.renderer, &inner);
        }
    }
}

void sdl_minimap_draw_focus_tip(sdl_view* d, int canvas_w, int canvas_h,
    const SDL_FRect* map_dst, int min_y, int min_x, int max_y, int max_x)
{
    char tip[160];
    char display[160];
    int map_rows;
    int map_cols;
    int max_text_cols;
    int len;
    int box_cols;
    int col;
    int row;
    int max_row;
    float grid_w;
    float grid_h;
    float anchor_x;
    float anchor_y;
    SDL_FRect box;
    SDL_Color text = {235, 242, 236, 255};

    if (!g_minimap.active || !g_minimap.focus_active || !d || !map_dst)
        return;
    if (d->cell_w <= 0 || d->cell_h <= 0 || d->cols <= 0 || d->rows <= 0)
        return;
    if (canvas_w <= 0 || canvas_h <= 0)
        return;
    if (g_minimap.focus_y < min_y || g_minimap.focus_y > max_y
        || g_minimap.focus_x < min_x || g_minimap.focus_x > max_x)
    {
        return;
    }
    if (!hint_messages_short_tip_for_source(g_minimap.focus_y,
            g_minimap.focus_x, tip, sizeof(tip)))
    {
        return;
    }

    max_text_cols = d->cols - 4;
    if (max_text_cols < 8)
        return;

    strnfmt(display, sizeof(display), "%s", tip);
    len = (int)strlen(display);
    if (len > max_text_cols)
    {
        int keep = max_text_cols - 3;
        if (keep < 1)
            keep = 1;
        strnfmt(display, sizeof(display), "%.*s...", keep, tip);
        len = (int)strlen(display);
    }
    if (len <= 0)
        return;

    map_rows = max_y - min_y + 1;
    map_cols = max_x - min_x + 1;
    if (map_rows <= 0 || map_cols <= 0)
        return;

    grid_w = map_dst->w / (float)map_cols;
    grid_h = map_dst->h / (float)map_rows;
    anchor_x = map_dst->x + ((float)(g_minimap.focus_x - min_x) + 0.5f)
        * grid_w;
    anchor_y = map_dst->y + ((float)(g_minimap.focus_y - min_y) + 0.5f)
        * grid_h;

    box_cols = len + 2;
    if (box_cols > d->cols)
        box_cols = d->cols;

    col = (int)((anchor_x + 10.0f) / (float)d->cell_w);
    row = (int)((anchor_y - (float)d->cell_h * 2.0f) / (float)d->cell_h);
    if (col + box_cols > d->cols)
        col = d->cols - box_cols;
    if (col < 0)
        col = 0;

    max_row = (canvas_h / d->cell_h) - 1;
    if (max_row < 0)
        return;
    if (row > max_row)
        row = max_row;
    if (row < 0)
        row = 0;

    box.x = (float)(col * d->cell_w);
    box.y = (float)(row * d->cell_h);
    box.w = (float)(box_cols * d->cell_w);
    box.h = (float)d->cell_h;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 12, 18, 16, 232);
    SDL_RenderFillRect(g_state.renderer, &box);
    SDL_SetRenderDrawColor(g_state.renderer, 80, 245, 130, 235);
    SDL_RenderRect(g_state.renderer, &box);
    sdl_render_mono_text(d, col + 1, row, len, display, text);
}

bool sdl_minimap_known_bounds(int* min_y, int* min_x, int* max_y,
    int* max_x)
{
    bool any = false;

    if (!p_ptr || !min_y || !min_x || !max_y || !max_x)
        return false;
    if (p_ptr->cur_map_hgt <= 0 || p_ptr->cur_map_wid <= 0)
        return false;

    *min_y = p_ptr->cur_map_hgt;
    *min_x = p_ptr->cur_map_wid;
    *max_y = 0;
    *max_x = 0;

    for (int y = 0; y < p_ptr->cur_map_hgt; y++) {
        for (int x = 0; x < p_ptr->cur_map_wid; x++) {
            if (!(cave_info[y][x] & (CAVE_MARK | CAVE_SEEN)))
                continue;

            if (y < *min_y) *min_y = y;
            if (y > *max_y) *max_y = y;
            if (x < *min_x) *min_x = x;
            if (x > *max_x) *max_x = x;
            any = true;
        }
    }

    for (int i = 1; i < mon_max; i++) {
        monster_type* m_ptr = &mon_list[i];
        int y;
        int x;

        if (!m_ptr->r_idx)
            continue;
        if (!m_ptr->ml && !(m_ptr->mflag & MFLAG_MARK))
            continue;

        y = m_ptr->fy;
        x = m_ptr->fx;
        if (!sdl_minimap_focus_point_valid(y, x))
            continue;

        if (y < *min_y) *min_y = y;
        if (y > *max_y) *max_y = y;
        if (x < *min_x) *min_x = x;
        if (x > *max_x) *max_x = x;
        any = true;
    }

    sdl_minimap_expand_bounds_for_hint_sources(min_y, min_x, max_y, max_x,
        &any);

    if (g_minimap.focus_active
        && sdl_minimap_grid_opened(g_minimap.focus_y, g_minimap.focus_x))
    {
        if (g_minimap.focus_y < *min_y) *min_y = g_minimap.focus_y;
        if (g_minimap.focus_y > *max_y) *max_y = g_minimap.focus_y;
        if (g_minimap.focus_x < *min_x) *min_x = g_minimap.focus_x;
        if (g_minimap.focus_x > *max_x) *max_x = g_minimap.focus_x;
        any = true;
    }

    if (!any) {
        if (sdl_minimap_focus_point_valid(p_ptr->py, p_ptr->px)) {
            *min_y = p_ptr->py;
            *max_y = p_ptr->py;
            *min_x = p_ptr->px;
            *max_x = p_ptr->px;
            any = true;
        } else {
            return false;
        }
    }

    if (p_ptr->py >= 0 && p_ptr->py < p_ptr->cur_map_hgt
        && p_ptr->px >= 0 && p_ptr->px < p_ptr->cur_map_wid)
    {
        if (p_ptr->py < *min_y) *min_y = p_ptr->py;
        if (p_ptr->py > *max_y) *max_y = p_ptr->py;
        if (p_ptr->px < *min_x) *min_x = p_ptr->px;
        if (p_ptr->px > *max_x) *max_x = p_ptr->px;
    }

    return (*max_y >= *min_y) && (*max_x >= *min_x);
}

void sdl_side_map_pane_note_level(void)
{
    if (!p_ptr)
        return;

    if (g_side_map_pane.last_depth == p_ptr->depth
        && g_side_map_pane.last_map_hgt == p_ptr->cur_map_hgt
        && g_side_map_pane.last_map_wid == p_ptr->cur_map_wid)
    {
        return;
    }

    g_side_map_pane.last_depth = p_ptr->depth;
    g_side_map_pane.last_map_hgt = p_ptr->cur_map_hgt;
    g_side_map_pane.last_map_wid = p_ptr->cur_map_wid;
    g_side_map_pane.default_zoom_pending = true;
    g_side_map_pane.pan_x = 0.0f;
    g_side_map_pane.pan_y = 0.0f;
    g_side_map_pane.press_active = false;
    g_side_map_pane.drag_active = false;
    g_side_map_pane.pinch_active = false;
    memset(g_side_map_pane.fingers, 0, sizeof(g_side_map_pane.fingers));
}

void sdl_side_map_pane_redraw(void)
{
    g_state.need_present = true;
}

bool sdl_side_map_pane_content_rect(SDL_FRect* out_rect,
    SDL_Rect* out_clip)
{
    SDL_Rect pane_rect;
    float pad;

    if (!out_rect)
        return false;
    if (!sdl_side_map_pane_current_rect(&pane_rect))
        return false;

    if (out_clip)
        *out_clip = pane_rect;

    pad = g_state.system_scale * 2.0f;
    if (pad < 2.0f)
        pad = 2.0f;
    if ((float)pane_rect.w <= pad * 2.0f
        || (float)pane_rect.h <= pad * 2.0f)
    {
        return false;
    }

    *out_rect = (SDL_FRect){
        .x = (float)pane_rect.x + pad,
        .y = (float)pane_rect.y + pad,
        .w = (float)pane_rect.w - pad * 2.0f,
        .h = (float)pane_rect.h - pad * 2.0f,
    };
    return out_rect->w > 0.0f && out_rect->h > 0.0f;
}

void sdl_side_map_pane_draw_player_marker(const SDL_FRect* map_dst,
    int min_y, int min_x, int max_y, int max_x)
{
    int map_rows;
    int map_cols;
    float grid_w;
    float grid_h;
    SDL_FRect player_dst;

    if (!p_ptr || !map_dst)
        return;
    if (p_ptr->py < min_y || p_ptr->py > max_y
        || p_ptr->px < min_x || p_ptr->px > max_x)
    {
        return;
    }

    map_rows = max_y - min_y + 1;
    map_cols = max_x - min_x + 1;
    if (map_rows <= 0 || map_cols <= 0)
        return;

    grid_w = map_dst->w / (float)map_cols;
    grid_h = map_dst->h / (float)map_rows;
    player_dst = (SDL_FRect){
        .x = map_dst->x + (float)(p_ptr->px - min_x) * grid_w,
        .y = map_dst->y + (float)(p_ptr->py - min_y) * grid_h,
        .w = grid_w,
        .h = grid_h,
    };

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 70, 240, 120, 72);
    SDL_RenderFillRect(g_state.renderer, &player_dst);
    SDL_SetRenderDrawColor(g_state.renderer, 80, 255, 130, 255);
    SDL_RenderRect(g_state.renderer, &player_dst);
    if (player_dst.w >= 4.0f && player_dst.h >= 4.0f) {
        player_dst.x += 1.0f;
        player_dst.y += 1.0f;
        player_dst.w -= 2.0f;
        player_dst.h -= 2.0f;
        SDL_RenderRect(g_state.renderer, &player_dst);
    }
}

void sdl_side_map_pane_render_empty(const SDL_FRect* content)
{
    if (!content)
        return;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 5, 7, 8, 255);
    SDL_RenderFillRect(g_state.renderer, content);
}

void sdl_side_map_pane_render(void)
{
    SDL_FRect content;
    SDL_Rect clip;
    SDL_Texture* map_texture;
    SDL_Texture* restore_target;
    int min_y, min_x, max_y, max_x;
    int map_rows;
    int map_cols;
    int source_w;
    int source_h;
    float fit_scale;
    float scale;
    float base_x = 0.0f;
    float base_y = 0.0f;
    SDL_FRect map_dst;

    if (!g_state.renderer || !p_ptr)
        return;
    if (!sdl_side_map_pane_content_rect(&content, &clip))
        return;

    sdl_side_map_pane_note_level();
    sdl_side_map_pane_render_empty(&content);

    if (!sdl_minimap_known_bounds(&min_y, &min_x, &max_y, &max_x))
        return;

    map_rows = max_y - min_y + 1;
    map_cols = max_x - min_x + 1;
    if (map_rows <= 0 || map_cols <= 0)
        return;

    source_w = map_cols * TILE_SIZE;
    source_h = map_rows * TILE_SIZE;
    if (source_w <= 0 || source_h <= 0)
        return;

    fit_scale = content.w / (float)source_w;
    if (content.h / (float)source_h < fit_scale)
        fit_scale = content.h / (float)source_h;
    if (fit_scale <= 0.0f)
        return;

    if (g_side_map_pane.default_zoom_pending) {
        g_side_map_pane.zoom_step = 0;
        g_side_map_pane.pan_x = 0.0f;
        g_side_map_pane.pan_y = 0.0f;
        g_side_map_pane.default_zoom_pending = false;
    }

    scale = fit_scale
        * sdl_minimap_zoom_factor_for_step(g_side_map_pane.zoom_step);
    if (scale <= 0.0f)
        return;

    map_dst.w = (float)source_w * scale;
    map_dst.h = (float)source_h * scale;

    if (p_ptr->py < min_y || p_ptr->py > max_y
        || p_ptr->px < min_x || p_ptr->px > max_x)
    {
        map_dst.x = content.x + (content.w - map_dst.w) * 0.5f;
        map_dst.y = content.y + (content.h - map_dst.h) * 0.5f;
        g_side_map_pane.pan_x = 0.0f;
        g_side_map_pane.pan_y = 0.0f;
    } else {
        float player_src_x = ((float)(p_ptr->px - min_x) + 0.5f)
            * (float)TILE_SIZE * scale;
        float player_src_y = ((float)(p_ptr->py - min_y) + 0.5f)
            * (float)TILE_SIZE * scale;

        base_x = content.x + content.w * 0.5f - player_src_x;
        base_y = content.y + content.h * 0.5f - player_src_y;
        map_dst.x = base_x + g_side_map_pane.pan_x;
        map_dst.y = base_y + g_side_map_pane.pan_y;

        if (map_dst.w <= content.w)
            map_dst.x = content.x + (content.w - map_dst.w) * 0.5f;
        else
            map_dst.x = sdl_minimap_clampf(map_dst.x,
                content.x + content.w - map_dst.w, content.x);

        if (map_dst.h <= content.h)
            map_dst.y = content.y + (content.h - map_dst.h) * 0.5f;
        else
            map_dst.y = sdl_minimap_clampf(map_dst.y,
                content.y + content.h - map_dst.h, content.y);

        g_side_map_pane.pan_x = map_dst.x - base_x;
        g_side_map_pane.pan_y = map_dst.y - base_y;
    }

    map_texture = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, source_w, source_h);
    if (!map_texture) {
        log_warn("sdl_side_map_pane_render: texture %dx%d failed: %s",
            source_w, source_h, SDL_GetError());
        return;
    }

    SDL_SetTextureBlendMode(map_texture, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(map_texture, SDL_SCALEMODE_NEAREST);

    restore_target = SDL_GetRenderTarget(g_state.renderer);
    SDL_SetRenderTarget(g_state.renderer, map_texture);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            byte a = TERM_DARK;
            byte ta = TERM_DARK;
            char c = ' ';
            char tc = ' ';
            SDL_FRect cell_dst = {
                .x = (float)((x - min_x) * TILE_SIZE),
                .y = (float)((y - min_y) * TILE_SIZE),
                .w = TILE_SIZE,
                .h = TILE_SIZE,
            };

            map_info(y, x, &a, &c, &ta, &tc);
            sdl_draw_map_tile_layers_at(y, x, a, c, ta, tc, &cell_dst);
        }
    }

    SDL_SetRenderTarget(g_state.renderer, restore_target);
    SDL_SetRenderClipRect(g_state.renderer, &clip);
    SDL_RenderTexture(g_state.renderer, map_texture, NULL, &map_dst);
    SDL_DestroyTexture(map_texture);

    sdl_minimap_draw_hint_sources(&map_dst, min_y, min_x, max_y, max_x);
    sdl_side_map_pane_draw_player_marker(&map_dst, min_y, min_x, max_y,
        max_x);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
}

bool sdl_side_map_pane_adjust_zoom(int delta)
{
    int new_step;

    if (delta == 0)
        return false;

    new_step = sdl_minimap_clamp_zoom_step(g_side_map_pane.zoom_step + delta);
    if (new_step == g_side_map_pane.zoom_step)
        return false;

    g_side_map_pane.zoom_step = new_step;
    g_side_map_pane.default_zoom_pending = false;
    sdl_side_map_pane_redraw();
    return true;
}

bool sdl_side_map_pane_offset_by(float dx, float dy)
{
    if (dx == 0.0f && dy == 0.0f)
        return false;

    g_side_map_pane.pan_x += dx;
    g_side_map_pane.pan_y += dy;
    g_side_map_pane.default_zoom_pending = false;
    sdl_side_map_pane_redraw();
    return true;
}

void sdl_side_map_pane_begin_press(bool mouse, SDL_FingerID finger_id,
    float x, float y)
{
    g_side_map_pane.press_active = true;
    g_side_map_pane.press_mouse = mouse;
    g_side_map_pane.press_finger_id = finger_id;
    g_side_map_pane.press_dragged = false;
    g_side_map_pane.press_start_x = x;
    g_side_map_pane.press_start_y = y;
}

bool sdl_side_map_pane_press_matches(bool mouse,
    SDL_FingerID finger_id)
{
    if (!g_side_map_pane.press_active)
        return false;
    if (g_side_map_pane.press_mouse != mouse)
        return false;
    if (!mouse && g_side_map_pane.press_finger_id != finger_id)
        return false;

    return true;
}

void sdl_side_map_pane_update_press_drag(float x, float y)
{
    float dx;
    float dy;
    float threshold;

    if (!g_side_map_pane.press_active || g_side_map_pane.press_dragged)
        return;

    dx = x - g_side_map_pane.press_start_x;
    dy = y - g_side_map_pane.press_start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;

    threshold = g_side_map_pane.press_mouse
        ? sdl_pane_layout_drag_threshold_px()
        : sdl_touch_swipe_threshold_px();

    if (dx > threshold || dy > threshold)
    {
        g_side_map_pane.press_dragged = true;
    }
}

void sdl_side_map_pane_clear_press(void)
{
    g_side_map_pane.press_active = false;
    g_side_map_pane.press_mouse = false;
    g_side_map_pane.press_finger_id = 0;
    g_side_map_pane.press_dragged = false;
    g_side_map_pane.press_start_x = 0.0f;
    g_side_map_pane.press_start_y = 0.0f;
}

void sdl_side_map_pane_send_click(void)
{
    if (sdl_pane_command_shortcuts_active())
        sdl_enqueue_bypassed_command('M');
}

void sdl_side_map_pane_begin_drag(bool mouse, SDL_FingerID finger_id,
    float x, float y)
{
    g_side_map_pane.drag_active = true;
    g_side_map_pane.drag_mouse = mouse;
    g_side_map_pane.drag_finger_id = finger_id;
    g_side_map_pane.drag_last_x = x;
    g_side_map_pane.drag_last_y = y;
}

bool sdl_side_map_pane_drag_to(bool mouse, SDL_FingerID finger_id,
    float x, float y)
{
    float dx;
    float dy;

    if (!g_side_map_pane.drag_active || g_side_map_pane.drag_mouse != mouse)
        return false;
    if (!mouse && g_side_map_pane.drag_finger_id != finger_id)
        return false;

    dx = x - g_side_map_pane.drag_last_x;
    dy = y - g_side_map_pane.drag_last_y;
    g_side_map_pane.drag_last_x = x;
    g_side_map_pane.drag_last_y = y;

    (void)sdl_side_map_pane_offset_by(dx, dy);
    return true;
}

void sdl_side_map_pane_cancel_drag(void)
{
    g_side_map_pane.drag_active = false;
    g_side_map_pane.drag_mouse = false;
    g_side_map_pane.drag_finger_id = 0;
    g_side_map_pane.drag_last_x = 0.0f;
    g_side_map_pane.drag_last_y = 0.0f;
}

int sdl_side_map_pane_find_finger(SDL_FingerID finger_id)
{
    for (int i = 0; i < MINIMAP_MAX_TOUCH_FINGERS; i++) {
        if (g_side_map_pane.fingers[i].active
            && g_side_map_pane.fingers[i].finger_id == finger_id)
        {
            return i;
        }
    }

    return -1;
}

int sdl_side_map_pane_active_finger_count(void)
{
    int count = 0;

    for (int i = 0; i < MINIMAP_MAX_TOUCH_FINGERS; i++) {
        if (g_side_map_pane.fingers[i].active)
            count++;
    }

    return count;
}

bool sdl_side_map_pane_first_two_fingers(int* out_a, int* out_b)
{
    int first = -1;

    for (int i = 0; i < MINIMAP_MAX_TOUCH_FINGERS; i++) {
        if (!g_side_map_pane.fingers[i].active)
            continue;
        if (first < 0) {
            first = i;
            continue;
        }
        if (out_a) *out_a = first;
        if (out_b) *out_b = i;
        return true;
    }

    return false;
}

float sdl_side_map_pane_finger_distance(int a, int b)
{
    float dx;
    float dy;

    if (a < 0 || a >= MINIMAP_MAX_TOUCH_FINGERS
        || b < 0 || b >= MINIMAP_MAX_TOUCH_FINGERS
        || !g_side_map_pane.fingers[a].active
        || !g_side_map_pane.fingers[b].active)
    {
        return 0.0f;
    }

    dx = g_side_map_pane.fingers[a].x - g_side_map_pane.fingers[b].x;
    dy = g_side_map_pane.fingers[a].y - g_side_map_pane.fingers[b].y;
    return SDL_sqrtf(dx * dx + dy * dy);
}

void sdl_side_map_pane_start_pinch_if_possible(void)
{
    int a = -1;
    int b = -1;
    float distance;

    if (!sdl_side_map_pane_first_two_fingers(&a, &b)) {
        g_side_map_pane.pinch_active = false;
        return;
    }

    distance = sdl_side_map_pane_finger_distance(a, b);
    if (distance < 8.0f) {
        g_side_map_pane.pinch_active = false;
        return;
    }

    g_side_map_pane.pinch_active = true;
    g_side_map_pane.press_dragged = true;
    g_side_map_pane.pinch_finger_a = a;
    g_side_map_pane.pinch_finger_b = b;
    g_side_map_pane.pinch_start_distance = distance;
    g_side_map_pane.pinch_start_zoom_step = g_side_map_pane.zoom_step;
}

void sdl_side_map_pane_start_drag_from_first_finger(void)
{
    for (int i = 0; i < MINIMAP_MAX_TOUCH_FINGERS; i++) {
        if (!g_side_map_pane.fingers[i].active)
            continue;

        sdl_side_map_pane_begin_drag(false,
            g_side_map_pane.fingers[i].finger_id,
            g_side_map_pane.fingers[i].x, g_side_map_pane.fingers[i].y);
        return;
    }

    sdl_side_map_pane_cancel_drag();
}

bool sdl_side_map_pane_update_pinch(void)
{
    float distance;
    float ratio;
    int delta;
    int target_step;
    int new_step;

    if (!g_side_map_pane.pinch_active)
        return false;
    if (g_side_map_pane.pinch_finger_a < 0
        || g_side_map_pane.pinch_finger_b < 0
        || g_side_map_pane.pinch_start_distance < 8.0f)
    {
        return false;
    }

    distance = sdl_side_map_pane_finger_distance(
        g_side_map_pane.pinch_finger_a, g_side_map_pane.pinch_finger_b);
    if (distance < 8.0f)
        return false;

    ratio = distance / g_side_map_pane.pinch_start_distance;
    delta = sdl_minimap_zoom_delta_for_pinch_ratio(ratio);
    target_step = g_side_map_pane.pinch_start_zoom_step + delta;
    new_step = sdl_minimap_clamp_zoom_step(target_step);
    if (new_step == g_side_map_pane.zoom_step)
        return false;

    g_side_map_pane.press_dragged = true;
    g_side_map_pane.zoom_step = new_step;
    g_side_map_pane.default_zoom_pending = false;
    sdl_side_map_pane_redraw();
    return true;
}

void sdl_side_map_pane_add_or_update_finger(SDL_FingerID finger_id,
    float x, float y)
{
    int index = sdl_side_map_pane_find_finger(finger_id);

    if (index < 0) {
        for (int i = 0; i < MINIMAP_MAX_TOUCH_FINGERS; i++) {
            if (!g_side_map_pane.fingers[i].active) {
                index = i;
                g_side_map_pane.fingers[i].active = true;
                g_side_map_pane.fingers[i].finger_id = finger_id;
                break;
            }
        }
    }

    if (index < 0)
        return;

    g_side_map_pane.fingers[index].x = x;
    g_side_map_pane.fingers[index].y = y;
}

bool sdl_side_map_pane_remove_finger(SDL_FingerID finger_id)
{
    int index = sdl_side_map_pane_find_finger(finger_id);

    if (index < 0)
        return false;

    memset(&g_side_map_pane.fingers[index], 0,
        sizeof(g_side_map_pane.fingers[index]));

    if (g_side_map_pane.drag_active && !g_side_map_pane.drag_mouse
        && g_side_map_pane.drag_finger_id == finger_id)
    {
        sdl_side_map_pane_cancel_drag();
    }

    if (sdl_side_map_pane_active_finger_count() >= 2)
        sdl_side_map_pane_start_pinch_if_possible();
    else {
        g_side_map_pane.pinch_active = false;
        if (sdl_side_map_pane_active_finger_count() == 1)
            sdl_side_map_pane_start_drag_from_first_finger();
    }

    return true;
}

bool sdl_side_map_pane_handle_mouse_wheel(
    const SDL_MouseWheelEvent* wheel)
{
    static sdl_wheel_step_state wheel_state;
    SDL_Rect pane_rect;
    int steps;

    if (!wheel)
        return false;
    if (wheel->which == SDL_TOUCH_MOUSEID)
        return false;
    if (!sdl_side_map_pane_current_rect(&pane_rect))
        return false;
    if (!sdl_point_in_rect(&pane_rect, wheel->mouse_x, wheel->mouse_y))
        return false;

    steps = sdl_wheel_step_state_consume_primary_axis(&wheel_state, wheel);
    if (steps != 0)
        (void)sdl_side_map_pane_adjust_zoom(steps);

    return true;
}

bool sdl_side_map_pane_handle_pointer_down(float x, float y,
    bool mouse, SDL_FingerID finger_id)
{
    SDL_Rect pane_rect;

    if (!sdl_side_map_pane_current_rect(&pane_rect))
        return false;
    if (!sdl_point_in_rect(&pane_rect, x, y))
        return false;

    if (mouse) {
        sdl_side_map_pane_begin_press(true, 0, x, y);
        sdl_side_map_pane_begin_drag(true, 0, x, y);
        return true;
    }

    if (sdl_side_map_pane_active_finger_count() == 0)
        sdl_side_map_pane_begin_press(false, finger_id, x, y);
    else
        g_side_map_pane.press_dragged = true;

    sdl_side_map_pane_add_or_update_finger(finger_id, x, y);
    if (sdl_side_map_pane_active_finger_count() >= 2) {
        g_side_map_pane.press_dragged = true;
        sdl_side_map_pane_cancel_drag();
        sdl_side_map_pane_start_pinch_if_possible();
    } else {
        sdl_side_map_pane_begin_drag(false, finger_id, x, y);
    }

    return true;
}

bool sdl_side_map_pane_handle_pointer_motion(float x, float y,
    bool mouse, SDL_FingerID finger_id)
{
    if (mouse) {
        if (!g_side_map_pane.drag_active || !g_side_map_pane.drag_mouse)
            return false;
        if (!sdl_side_map_pane_current_rect(NULL)) {
            sdl_side_map_pane_cancel_drag();
            sdl_side_map_pane_clear_press();
            return true;
        }
        if (sdl_side_map_pane_press_matches(true, 0))
            sdl_side_map_pane_update_press_drag(x, y);
        return sdl_side_map_pane_drag_to(true, 0, x, y);
    }

    if (sdl_side_map_pane_find_finger(finger_id) < 0)
        return false;
    if (!sdl_side_map_pane_current_rect(NULL)) {
        (void)sdl_side_map_pane_remove_finger(finger_id);
        if (sdl_side_map_pane_press_matches(false, finger_id))
            sdl_side_map_pane_clear_press();
        return true;
    }

    if (sdl_side_map_pane_press_matches(false, finger_id))
        sdl_side_map_pane_update_press_drag(x, y);
    sdl_side_map_pane_add_or_update_finger(finger_id, x, y);
    if (g_side_map_pane.pinch_active)
        (void)sdl_side_map_pane_update_pinch();
    else
        (void)sdl_side_map_pane_drag_to(false, finger_id, x, y);

    return true;
}

bool sdl_side_map_pane_handle_pointer_up(float x, float y,
    bool mouse, SDL_FingerID finger_id)
{
    SDL_Rect pane_rect;
    bool clicked = false;

    if (mouse) {
        if (!g_side_map_pane.drag_active || !g_side_map_pane.drag_mouse)
            return false;
        clicked = sdl_side_map_pane_press_matches(true, 0)
            && !g_side_map_pane.press_dragged
            && sdl_side_map_pane_current_rect(&pane_rect)
            && sdl_point_in_rect(&pane_rect, x, y);
        sdl_side_map_pane_cancel_drag();
        sdl_side_map_pane_clear_press();
        if (clicked)
            sdl_side_map_pane_send_click();
        return true;
    }

    clicked = sdl_side_map_pane_press_matches(false, finger_id)
        && !g_side_map_pane.press_dragged
        && sdl_side_map_pane_current_rect(&pane_rect)
        && sdl_point_in_rect(&pane_rect, x, y);

    if (!sdl_side_map_pane_remove_finger(finger_id))
        return false;

    if (sdl_side_map_pane_press_matches(false, finger_id))
        sdl_side_map_pane_clear_press();

    if (clicked)
        sdl_side_map_pane_send_click();

    return true;
}

void sdl_side_map_pane_cancel_pointer(SDL_FingerID finger_id,
    bool mouse)
{
    if (mouse) {
        if (g_side_map_pane.drag_active && g_side_map_pane.drag_mouse)
            sdl_side_map_pane_cancel_drag();
        if (sdl_side_map_pane_press_matches(true, 0))
            sdl_side_map_pane_clear_press();
        return;
    }

    (void)sdl_side_map_pane_remove_finger(finger_id);
    if (sdl_side_map_pane_press_matches(false, finger_id))
        sdl_side_map_pane_clear_press();
}

bool sdl_display_pixel_map(int* cy, int* cx)
{
    sdl_view* d;
    SDL_Texture* map_texture;
    SDL_Texture* restore_target;
    int min_y, min_x, max_y, max_x;
    int map_rows, map_cols;
    int source_w, source_h;
    int canvas_w, canvas_h;
    float scale_x, scale_y, scale;
    float base_x = 0.0f;
    float base_y = 0.0f;
    SDL_FRect map_dst;

    if (!Term || !p_ptr || !g_state.renderer)
        return false;

    d = sdl_view_from_term(Term);
    if (!d || !d->canvas || d->cell_w <= 0 || d->cell_h <= 0)
        return false;

    if (!sdl_minimap_known_bounds(&min_y, &min_x, &max_y, &max_x))
        return false;

    map_rows = max_y - min_y + 1;
    map_cols = max_x - min_x + 1;
    if (map_rows <= 0 || map_cols <= 0)
        return false;

    source_w = map_cols * TILE_SIZE;
    source_h = map_rows * TILE_SIZE;
    canvas_w = d->cols * d->cell_w;
    canvas_h = (d->rows > 1 ? d->rows - 1 : d->rows) * d->cell_h;
    if (source_w <= 0 || source_h <= 0 || canvas_w <= 0 || canvas_h <= 0)
        return false;

    map_texture = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET, source_w, source_h);
    if (!map_texture) {
        log_warn("sdl_display_pixel_map: texture %dx%d failed: %s",
            source_w, source_h, SDL_GetError());
        return false;
    }

    SDL_SetTextureBlendMode(map_texture, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(map_texture, SDL_SCALEMODE_NEAREST);

    restore_target = SDL_GetRenderTarget(g_state.renderer);
    SDL_SetRenderTarget(g_state.renderer, map_texture);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            byte a = TERM_DARK;
            byte ta = TERM_DARK;
            char c = ' ';
            char tc = ' ';
            SDL_FRect cell_dst = {
                (float)((x - min_x) * TILE_SIZE),
                (float)((y - min_y) * TILE_SIZE),
                TILE_SIZE,
                TILE_SIZE
            };

            map_info(y, x, &a, &c, &ta, &tc);
            sdl_draw_map_tile_layers_at(y, x, a, c, ta, tc, &cell_dst);
        }
    }

    SDL_SetRenderTarget(g_state.renderer, d->canvas);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    scale_x = (float)canvas_w / (float)source_w;
    scale_y = (float)canvas_h / (float)source_h;
    scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (g_minimap.active && g_minimap.default_zoom_pending) {
        g_minimap.zoom_step = sdl_minimap_default_zoom_step(scale, d);
        g_minimap.default_zoom_pending = false;
    }
    if (g_minimap.active)
        scale *= sdl_minimap_zoom_factor();
    if (scale <= 0.0f) {
        SDL_DestroyTexture(map_texture);
        SDL_SetRenderTarget(g_state.renderer, restore_target);
        return false;
    }

    map_dst.w = (float)source_w * scale;
    map_dst.h = (float)source_h * scale;
    if (g_minimap.active)
    {
        int center_y = p_ptr->py;
        int center_x = p_ptr->px;

        if (g_minimap.focus_active
            && g_minimap.focus_y >= min_y && g_minimap.focus_y <= max_y
            && g_minimap.focus_x >= min_x && g_minimap.focus_x <= max_x)
        {
            center_y = g_minimap.focus_y;
            center_x = g_minimap.focus_x;
        }

        if (center_y < min_y || center_y > max_y
            || center_x < min_x || center_x > max_x)
        {
            map_dst.x = ((float)canvas_w - map_dst.w) * 0.5f;
            map_dst.y = ((float)canvas_h - map_dst.h) * 0.5f;
            g_minimap.pan_x = 0.0f;
            g_minimap.pan_y = 0.0f;
        }
        else
        {
            float player_src_x = ((float)(center_x - min_x) + 0.5f)
                * (float)TILE_SIZE * scale;
            float player_src_y = ((float)(center_y - min_y) + 0.5f)
                * (float)TILE_SIZE * scale;

            base_x = (float)canvas_w * 0.5f - player_src_x;
            base_y = (float)canvas_h * 0.5f - player_src_y;
            map_dst.x = base_x + g_minimap.pan_x;
            map_dst.y = base_y + g_minimap.pan_y;
            if (map_dst.w <= (float)canvas_w)
                map_dst.x = ((float)canvas_w - map_dst.w) * 0.5f;
            else
                map_dst.x = sdl_minimap_clampf(map_dst.x,
                    (float)canvas_w - map_dst.w, 0.0f);

            if (map_dst.h <= (float)canvas_h)
                map_dst.y = ((float)canvas_h - map_dst.h) * 0.5f;
            else
                map_dst.y = sdl_minimap_clampf(map_dst.y,
                    (float)canvas_h - map_dst.h, 0.0f);

            g_minimap.pan_x = map_dst.x - base_x;
            g_minimap.pan_y = map_dst.y - base_y;
        }
    } else {
        map_dst.x = ((float)canvas_w - map_dst.w) * 0.5f;
        map_dst.y = ((float)canvas_h - map_dst.h) * 0.5f;
    }
    sdl_minimap_store_map_layout(&map_dst, min_y, min_x, max_y, max_x);
    SDL_RenderTexture(g_state.renderer, map_texture, NULL, &map_dst);

    SDL_SetRenderDrawColor(g_state.renderer, 255, 255, 255, 80);
    SDL_RenderRect(g_state.renderer, &map_dst);

    sdl_minimap_draw_hint_sources(&map_dst, min_y, min_x, max_y, max_x);

    if (p_ptr->py >= min_y && p_ptr->py <= max_y
        && p_ptr->px >= min_x && p_ptr->px <= max_x)
    {
        float grid_w = map_dst.w / (float)map_cols;
        float grid_h = map_dst.h / (float)map_rows;
        SDL_FRect player_dst = {
            map_dst.x + (float)(p_ptr->px - min_x) * grid_w,
            map_dst.y + (float)(p_ptr->py - min_y) * grid_h,
            grid_w,
            grid_h
        };
        float center_x = player_dst.x + player_dst.w * 0.5f;
        float center_y = player_dst.y + player_dst.h * 0.5f;

        SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_state.renderer, 70, 240, 120, 72);
        SDL_RenderFillRect(g_state.renderer, &player_dst);
        SDL_SetRenderDrawColor(g_state.renderer, 80, 255, 130, 255);
        SDL_RenderRect(g_state.renderer, &player_dst);
        if (player_dst.w >= 4.0f && player_dst.h >= 4.0f) {
            player_dst.x += 1.0f;
            player_dst.y += 1.0f;
            player_dst.w -= 2.0f;
            player_dst.h -= 2.0f;
            SDL_RenderRect(g_state.renderer, &player_dst);
        }

        if (cx) {
            int term_x = (int)(center_x / (float)d->cell_w);
            if (term_x < 0) term_x = 0;
            if (term_x >= Term->wid) term_x = Term->wid - 1;
            *cx = term_x;
        }
        if (cy) {
            int term_y = (int)(center_y / (float)d->cell_h);
            if (term_y < 0) term_y = 0;
            if (term_y >= Term->hgt) term_y = Term->hgt - 1;
            *cy = term_y;
        }
    }

    sdl_minimap_draw_focus_tip(d, canvas_w, canvas_h, &map_dst, min_y, min_x,
        max_y, max_x);

    if (g_minimap.active) {
        sdl_minimap_draw_controls(d, canvas_w, canvas_h);
        sdl_minimap_draw_prompt(d, canvas_w, canvas_h);
    }

    SDL_DestroyTexture(map_texture);
    SDL_SetRenderTarget(g_state.renderer, d->canvas);
    g_state.need_present = true;
    return true;
}

errr callback_sdl_pict(int x, int y, int n, const byte* ap, const char* cp,
                       const byte* tap, const char* tcp)
{
    sdl_view* d = sdl_view_from_term(Term);
    if (!d || !d->canvas || !Term || !ap || !cp || !tap || !tcp || n <= 0)
        return 0;
    if (x < 0 || y < 0 || x >= Term->wid || y >= Term->hgt)
        return 0;
    if (x + n > Term->wid)
        n = Term->wid - x;
    if (n <= 0)
        return 0;
    //log_trace("sdl3_pict stripe start: y=%d x=%d n=%d", y, x, n);

    SDL_SetRenderTarget(g_state.renderer, d->canvas);
    SDL_SetRenderClipRect(g_state.renderer, &(SDL_Rect){
        x * d->cell_w,
        y * d->cell_h,
        n * d->cell_w * (use_bigtile + 1),
        d->cell_h,
    });

    SDL_FRect dst = {
        x * d->cell_w,
        y * d->cell_h,
        d->cell_w * (use_bigtile + 1),
        d->cell_h,
    };

    const byte* pict_story_row = (Term->scr && Term->scr->story
        && y >= 0 && y < Term->hgt) ? Term->scr->story[y] : NULL;

    for (int i = 0; i < n; ++i, dst.x += dst.w) {
        byte a = ap[i];
        char c = cp[i];
        int dy = -1;
        int dx = -1;

        /*
         * Combat-roll tiles are drawn inline by the pixel-packed row renderer
         * (see sdl_render_story_row_packed); skip them here so they are not
         * also stamped at their cell position.
         */
        if (pict_story_row && (x + i) < Term->wid
            && (pict_story_row[x + i] & STORY_FLAG_PIXEL_PACK))
            continue;

        if (Term == term_screen) {
            int term_x = x + (i * (use_bigtile + 1));
            if (y >= ROW_MAP && term_x >= COL_MAP) {
                int map_y = y - ROW_MAP;
                int map_x = term_x - COL_MAP;
                if (use_bigtile)
                    map_x /= 2;

                dy = p_ptr->wy + map_y;
                dx = p_ptr->wx + map_x;
            }
        }

        sdl_draw_map_tile_layers_at(dy, dx, a, c, tap[i], tcp[i], &dst);
    }

    SDL_SetRenderClipRect(g_state.renderer, NULL);
    g_state.need_present = true;
    return 0;
}

bool sdl_load_tileset_texture(void)
{
    SDL_Surface* ts;
    SDL_Texture* texture;
    int tileset_width;

    if (g_state.tileset)
        return true;

    ts = IMG_Load("lib/xtra/graf/16x16.png");
    if (!ts) {
        log_error("Failed to load tileset PNG: %s", SDL_GetError());
        return false;
    }

    tileset_width = ts->w;
    texture = SDL_CreateTextureFromSurface(g_state.renderer, ts);
    SDL_DestroySurface(ts);
    if (!texture) {
        log_error("Failed to create tileset texture: %s", SDL_GetError());
        return false;
    }

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    g_state.tileset = texture;
    g_state.tileset_cols = tileset_width / TILE_SIZE;
    return true;
}

void sdl_apply_tiles_to_terms(bool tiles)
{
    for (int i = 0; i < MAX_TERM_DATA; i++) {
        term* t = &g_views[i].t;

        if (!g_views[i].term_ready)
            continue;

        t->higher_pict = tiles;
        t->pict_hook = tiles ? callback_sdl_pict : NULL;
    }
}

void sdl_mark_tiles_mode_game_redraw(void)
{
    if (!p_ptr || !character_generated)
        return;

    p_ptr->notice |= (PN_COMBINE | PN_REORDER);
    p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA
        | PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS);
    p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP | PR_EQUIPPY | PR_RESIST);
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0
        | PW_MESSAGE | PW_OVERHEAD | PW_MONSTER | PW_OBJECT | PW_MONLIST
        | PW_SUPPLY);
}

bool sdl_set_tiles_runtime(bool value)
{
    if (value && !sdl_load_tileset_texture()) {
        log_warn("Tiles mode request ignored because the tileset could not be loaded");
        return false;
    }

    config.tiles = value;
    g_state.use_tiles = value;

    if (value) {
        ANGBAND_GRAF = "new";
        arg_graphics = GRAPHICS_MICROCHASM;
        use_graphics = GRAPHICS_MICROCHASM;
        use_bigtile = true;
    } else {
        ANGBAND_GRAF = "old";
        arg_graphics = GRAPHICS_PSEUDO;
        use_graphics = GRAPHICS_PSEUDO;
        use_bigtile = false;
    }

    sdl_apply_tiles_to_terms(value);
    log_info("Tiles mode %s", value ? "enabled" : "disabled");
    return true;
}

void sdl_finish_tiles_mode_change(void)
{
    sdl_mark_tiles_mode_game_redraw();

    if (character_dungeon && p_ptr && p_ptr->playing && character_icky == 0) {
        Term_keypress(KTRL('R'));
        return;
    }

    if (Term)
        Term_xtra(TERM_XTRA_REACT, 0);

    if (Term)
        sdl_request_redraw();
}

void callback_sdl_nuke() {
    log_debug("sdl3_term_nuke");
    sdl_view* d = sdl_view_from_term(Term);
    if (!d)
        return;

    // if (d->font) {
    //     TTF_CloseFont(d->font);
    //     d->font = NULL;
    // }
    // for (int i = 0; i < 256; ++i) {
    //     if (d->glyph_cache[i]) {
    //         SDL_DestroyTexture(d->glyph_cache[i]);
    //         d->glyph_cache[i] = NULL;
    //     }
    // }
    if (d->font_atlas) {
        if (!d->font_atlas_cached)
            SDL_DestroyTexture(d->font_atlas);
    }
    d->font_atlas = NULL;
    // if (d->tileset)
    //     SDL_DestroyTexture(d->tileset);
    if (d->canvas)
        SDL_DestroyTexture(d->canvas);
    d->canvas = NULL;
    d->font_atlas_cached = false;
    d->font_atlas_exact = false;
    d->font_atlas_cell_w = 0;
    d->font_atlas_cell_h = 0;
    // if (d->renderer)
    //     SDL_DestroyRenderer(d->renderer);
    // if (d->window)
    //     SDL_DestroyWindow(d->window);
}

void callback_sdl_init(term* t)
{
    (void)t;
}

errr sdl_view_link_term(sdl_view* d, int term_index)
{
    Uint64 start_ns = SDL_GetTicksNS();
    term* t = &d->t;
    if (d->term_ready) {
        term* old = Term;
        Uint64 resize_ns;
        Uint64 redraw_ns;
        Term_activate(t);
        resize_ns = SDL_GetTicksNS();
        Term_resize(d->cols, d->rows);
        resize_ns = SDL_GetTicksNS() - resize_ns;
        redraw_ns = SDL_GetTicksNS();
        if (!(g_skip_main_redraw_on_layout_refresh && term_index == PANE_MAIN))
            Term_redraw();
        redraw_ns = SDL_GetTicksNS() - redraw_ns;
        Term_activate(old);
        log_debug("view %d term relink completed in %llu ms (Term_resize=%llu ms Term_redraw=%llu ms cols=%d rows=%d)",
            term_index,
            (unsigned long long)((SDL_GetTicksNS() - start_ns) / 1000000ULL),
            (unsigned long long)(resize_ns / 1000000ULL),
            (unsigned long long)(redraw_ns / 1000000ULL), d->cols, d->rows);
        return 0;
    }
    term_init(t, d->cols, d->rows, 256);
    t->soft_cursor = true;
    t->higher_pict = g_state.use_tiles;
    t->never_frosh = true;
    t->init_hook = callback_sdl_init;
    t->nuke_hook = callback_sdl_nuke;
    t->xtra_hook = callback_sdl_xtra;
    t->curs_hook = callback_sdl_curs;
    t->bigcurs_hook = callback_sdl_bigcurs;
    t->wipe_hook = callback_sdl_wipe;
    t->text_hook = callback_sdl_text;
    if (g_state.use_tiles)
        t->pict_hook = callback_sdl_pict;
    t->data = (void*)(uintptr_t)term_index;
    angband_term[term_index] = t;
    d->term_ready = true;
    log_debug("view %d term init completed in %llu ms (cols=%d rows=%d)",
        term_index,
        (unsigned long long)((SDL_GetTicksNS() - start_ns) / 1000000ULL),
        d->cols, d->rows);
    return 0;
}

// Helper to apply font rendering settings to a TTF_Font
