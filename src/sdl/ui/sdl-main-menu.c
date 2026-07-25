#include "angband.h"
#include "blitz.h"
#include "sdl/main-sdl-private.h"
#include "ui/question.h"

enum {
    SDL_POPUP_NOTIFICATION_TEXT_LEN = 96,
    SDL_POPUP_NOTIFICATION_PHASE_FADE_IN = 0,
    SDL_POPUP_NOTIFICATION_PHASE_HOLD,
    SDL_POPUP_NOTIFICATION_PHASE_FADE_OUT,
    SDL_POPUP_NOTIFICATION_PHASE_DONE,
};

typedef struct sdl_popup_notification_state {
    bool active;
    int phase;
    char text[SDL_POPUP_NOTIFICATION_TEXT_LEN];
    Uint64 fade_in_end_ns;
    Uint64 hold_end_ns;
    Uint64 end_ns;
    Uint64 next_frame_ns;
} sdl_popup_notification_state;

typedef struct sdl_main_menu_button_press_state {
    bool active;
    bool canceled;
    SDL_FingerID finger_id;
    float start_x;
    float start_y;
    Uint64 start_time;
} sdl_main_menu_button_press_state;

static sdl_popup_notification_state g_popup_notification;
static sdl_main_menu_button_press_state g_main_menu_button_press;
static bool g_main_menu_button_disable_prompt_pending;

static bool sdl_main_menu_button_run_pending_disable_prompt(void);

static int sdl_popup_notification_phase(Uint64 now_ns)
{
    if (now_ns < g_popup_notification.fade_in_end_ns)
        return SDL_POPUP_NOTIFICATION_PHASE_FADE_IN;
    if (now_ns < g_popup_notification.hold_end_ns)
        return SDL_POPUP_NOTIFICATION_PHASE_HOLD;
    if (now_ns < g_popup_notification.end_ns)
        return SDL_POPUP_NOTIFICATION_PHASE_FADE_OUT;
    return SDL_POPUP_NOTIFICATION_PHASE_DONE;
}

static int sdl_popup_notification_timeout_until(Uint64 now_ns,
    Uint64 target_ns)
{
    Uint64 remaining_ns;
    Uint64 remaining_ms;

    if (target_ns <= now_ns)
        return 0;

    remaining_ns = target_ns - now_ns;
    remaining_ms = (remaining_ns + 999999ULL) / 1000000ULL;
    if (remaining_ms > (Uint64)INT_MAX)
        return INT_MAX;
    return (int)remaining_ms;
}

void sdl_popup_notification_show(cptr text)
{
    Uint64 now_ns;
    Uint64 fade_ns;
    Uint64 hold_ns;
    int hold_ms;

    if (!text || !text[0])
        return;

    hold_ms = get_sdl_popup_notification_ms();
    if (hold_ms < 0)
        hold_ms = 0;
    if (hold_ms > SDL_POPUP_NOTIFICATION_MAX_MS)
        hold_ms = SDL_POPUP_NOTIFICATION_MAX_MS;
    if (hold_ms == 0)
        return;

    now_ns = SDL_GetTicksNS();
    fade_ns = (Uint64)SDL_NARRATIVE_BANNER_FADE_MS * 1000000ULL;
    hold_ns = (Uint64)hold_ms * 1000000ULL;

    memset(&g_popup_notification, 0, sizeof(g_popup_notification));
    g_popup_notification.active = true;
    g_popup_notification.phase = SDL_POPUP_NOTIFICATION_PHASE_FADE_IN;
    SDL_strlcpy(g_popup_notification.text, text,
        sizeof(g_popup_notification.text));
    g_popup_notification.fade_in_end_ns = now_ns + fade_ns;
    g_popup_notification.hold_end_ns =
        g_popup_notification.fade_in_end_ns + hold_ns;
    g_popup_notification.end_ns = g_popup_notification.hold_end_ns + fade_ns;
    g_popup_notification.next_frame_ns = now_ns
        + (Uint64)SDL_NARRATIVE_BANNER_FADE_FRAME_MS * 1000000ULL;
    g_state.need_present = true;
}

int sdl_popup_notification_pending_timeout_ms(Uint64 now_ns)
{
    int phase;
    Uint64 target_ns;

    if (!g_popup_notification.active)
        return -1;

    phase = sdl_popup_notification_phase(now_ns);
    if (phase == SDL_POPUP_NOTIFICATION_PHASE_DONE)
        return 0;
    if (phase == SDL_POPUP_NOTIFICATION_PHASE_HOLD)
        return sdl_popup_notification_timeout_until(now_ns,
            g_popup_notification.hold_end_ns);

    target_ns = g_popup_notification.next_frame_ns;
    if (phase == SDL_POPUP_NOTIFICATION_PHASE_FADE_IN
        && g_popup_notification.fade_in_end_ns < target_ns)
    {
        target_ns = g_popup_notification.fade_in_end_ns;
    }
    if (phase == SDL_POPUP_NOTIFICATION_PHASE_FADE_OUT
        && g_popup_notification.end_ns < target_ns)
    {
        target_ns = g_popup_notification.end_ns;
    }
    return sdl_popup_notification_timeout_until(now_ns, target_ns);
}

bool sdl_popup_notification_flush_expired(Uint64 now_ns)
{
    int phase;
    bool changed = false;

    if (!g_popup_notification.active)
        return false;

    phase = sdl_popup_notification_phase(now_ns);
    if (phase == SDL_POPUP_NOTIFICATION_PHASE_DONE) {
        memset(&g_popup_notification, 0, sizeof(g_popup_notification));
        changed = true;
    } else if (phase != g_popup_notification.phase) {
        g_popup_notification.phase = phase;
        if (phase == SDL_POPUP_NOTIFICATION_PHASE_HOLD)
            g_popup_notification.next_frame_ns =
                g_popup_notification.hold_end_ns;
        else
            g_popup_notification.next_frame_ns = now_ns
                + (Uint64)SDL_NARRATIVE_BANNER_FADE_FRAME_MS * 1000000ULL;
        changed = true;
    } else if (phase != SDL_POPUP_NOTIFICATION_PHASE_HOLD
        && now_ns >= g_popup_notification.next_frame_ns)
    {
        g_popup_notification.next_frame_ns = now_ns
            + (Uint64)SDL_NARRATIVE_BANNER_FADE_FRAME_MS * 1000000ULL;
        changed = true;
    }

    if (changed)
        g_state.need_present = true;
    return changed;
}

static bool sdl_main_menu_overlay_should_hide_supporting_panes(void)
{
#if SIL_SDL_MOBILE_BUILD
    return true;
#else
    return false;
#endif
}

bool sdl_main_menu_overlay_hides_supporting_panes(void)
{
    return g_main_menu_overlay_active
        && sdl_main_menu_overlay_should_hide_supporting_panes();
}

int sdl_main_menu_pane_font_px(void)
{
    int font_size = (config.aux_view_font_size > 0)
        ? sdl_resolve_aux_view_font_size(config.aux_view_font_size)
#if SIL_SDL_MOBILE_BUILD
        /* Mobile: enlarge popup-menu font one notch (3/4 -> 4/5 of main). */
        : sdl_auto_font_size_from_main(4, 5);
#else
        : sdl_auto_font_size_from_main(3, 4);
#endif
    int font_px = sdl_aux_cell_height_for_font_size(font_size);

    if (font_px < 8)
        font_px = 8;

    return font_px;
}

typedef struct main_menu_pane_layout {
    SDL_FRect panel;
    SDL_FRect rows[MAIN_MENU_MAX + 1];
    float pad_x;
    float row_h;
    float title_w;
    float shortcut_w;
    float shortcut_gap;
    int font_px;
    int first_choice;
    int visible_count;
} main_menu_pane_layout;

typedef enum main_menu_text_align {
    MAIN_MENU_TEXT_LEFT = 0,
    MAIN_MENU_TEXT_RIGHT,
    MAIN_MENU_TEXT_CENTER
} main_menu_text_align;

bool sdl_main_menu_pane_context_visible(void)
{
    SDL_Rect screen;

    if (!character_generated || !character_dungeon || character_icky)
        return false;
    if (!p_ptr || screen_saved_fullscreen_active())
        return false;
    if (!p_ptr->playing || p_ptr->leaving || p_ptr->is_dead)
    {
        return false;
    }

    screen = sdl_get_layout_screen_rect();
    if (screen.w <= 0 || screen.h <= 0)
        return false;

    return true;
}

const char* sdl_main_menu_mono_font_path(void)
{
    return config.monospace_font[0] != '\0'
        ? config.monospace_font
        : "lib/xtra/font/VictorMono-Medium.ttf";
}

TTF_Font* sdl_main_menu_mono_font_for_height(int font_px)
{
    const char* fallback = "lib/xtra/font/VictorMono-Medium.ttf";
    const char* font_path;
    TTF_Font* font;
    int cell_w;

    if (font_px <= 0)
        return NULL;

    cell_w = (font_px + 1) / 2;
    if (cell_w < 1)
        cell_w = 1;

    font_path = sdl_main_menu_mono_font_path();
    font = sdl_acquire_mono_font_cells(font_path, cell_w, font_px);
    if (!font && !streq(font_path, fallback))
        font = sdl_acquire_mono_font_cells(fallback, cell_w, font_px);

    return font;
}

void sdl_main_menu_shortcut_display_label(int choice, char* buf,
    size_t buflen)
{
    char label[32];

    if (!buf || buflen == 0)
        return;

    buf[0] = '\0';
    main_menu_shortcut_label(choice, label, sizeof(label));
    if (!label[0])
        return;

    if (steamdeck_controls_active())
        strnfmt(buf, buflen, "[%s]", label);
    else
        strnfmt(buf, buflen, "(%s)", label);
}

float sdl_main_menu_draw_text(TTF_Font* font, cptr text, float x,
    float y, float max_w, float row_h, SDL_Color color,
    main_menu_text_align align)
{
    SDL_Texture* texture;
    SDL_FRect dst;
    float scale = 1.0f;
    float max_h;
    int text_w = 0;
    int text_h = 0;

    if (!font || !text || !text[0] || max_w <= 0.0f || row_h <= 0.0f)
        return 0.0f;

    texture = sdl_ui_text_texture(font, text, color, &text_w, &text_h);
    if (!texture)
        return 0.0f;

    if (text_w > 0 && (float)text_w > max_w)
        scale = max_w / (float)text_w;
    max_h = row_h * 0.86f;
    if (text_h > 0 && (float)text_h * scale > max_h)
        scale = max_h / (float)text_h;
    if (scale > 1.0f)
        scale = 1.0f;

    dst.w = (float)text_w * scale;
    dst.h = (float)text_h * scale;
    if (align == MAIN_MENU_TEXT_RIGHT)
        dst.x = x + max_w - dst.w;
    else if (align == MAIN_MENU_TEXT_CENTER)
        dst.x = x + (max_w - dst.w) * 0.5f;
    else
        dst.x = x;
    dst.y = y + (row_h - dst.h) * 0.5f;

    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
    return dst.w;
}

bool sdl_main_menu_pane_button_rect(SDL_FRect* out)
{
    SDL_Rect anchor;
    SDL_Rect screen;
    TTF_Font* font;
    int font_px;
    int text_w;
    float pad_x;
    float panel_w;
    float panel_h;
    cptr label = "Menu";

    if (out)
        *out = (SDL_FRect){ 0 };

    if (!get_sdl_show_main_menu_button()
        || !sdl_main_menu_pane_context_visible())
        return false;

    screen = sdl_get_layout_screen_rect();
    if (!sdl_overlay_pane_anchor_rect(PANE_MAIN, &anchor)) {
        return false;
    }

    font_px = sdl_main_menu_pane_font_px();
    font = sdl_story_font_for_height(font_px);
    text_w = font ? sdl_touch_pane_story_text_width(font, label)
                  : (int)strlen(label) * font_px / 2;
    if (text_w < 1)
        return false;

    pad_x = (float)font_px * 0.78f;
    if (pad_x < 8.0f)
        pad_x = 8.0f;

    panel_w = (float)text_w + pad_x * 2.0f;
    if (panel_w < (float)font_px * 4.4f)
        panel_w = (float)font_px * 4.4f;
    if (panel_w > (float)screen.w)
        panel_w = (float)screen.w;

    panel_h = (float)sdl_main_menu_button_height_for_screen(&screen);
    if (panel_h <= 0.0f)
        return false;

    if (out) {
        *out = sdl_overlay_panel_rect(&anchor, PLACE_TOP_CENTER,
            (int)panel_w, (int)panel_h, &screen);
        out->y = (float)screen.y;
    }

    return true;
}

void sdl_main_menu_overlay_scroll_to_highlight(int visible_count)
{
    int max_first;
    int highlight = g_main_menu_overlay_highlight;

    if (visible_count < 1)
        visible_count = 1;
    if (visible_count > MAIN_MENU_MAX)
        visible_count = MAIN_MENU_MAX;

    max_first = MAIN_MENU_MAX - visible_count + 1;
    if (max_first < 1)
        max_first = 1;

    if (g_main_menu_overlay_first_choice < 1)
        g_main_menu_overlay_first_choice = 1;
    if (g_main_menu_overlay_first_choice > max_first)
        g_main_menu_overlay_first_choice = max_first;

    if (highlight >= 1 && highlight <= MAIN_MENU_MAX)
    {
        if (highlight < g_main_menu_overlay_first_choice)
            g_main_menu_overlay_first_choice = highlight;
        else if (highlight >= g_main_menu_overlay_first_choice + visible_count)
            g_main_menu_overlay_first_choice = highlight - visible_count + 1;
    }

    if (g_main_menu_overlay_first_choice < 1)
        g_main_menu_overlay_first_choice = 1;
    if (g_main_menu_overlay_first_choice > max_first)
        g_main_menu_overlay_first_choice = max_first;
}

bool sdl_main_menu_overlay_layout(main_menu_pane_layout* out)
{
    SDL_Rect anchor;
    SDL_Rect screen;
    TTF_Font* story_font;
    TTF_Font* mono_font;
    int font_px;
    int title_w = 0;
    int shortcut_w = 0;
    int text_h;
    float pad_x;
    float pad_y;
    float row_h;
    float panel_w;
    float panel_h;
    float max_panel_w;
    float max_panel_h;
    float shortcut_gap;
    int visible_count = MAIN_MENU_MAX;

    if (out)
        *out = (main_menu_pane_layout){ 0 };
    if (!sdl_main_menu_pane_context_visible())
        return false;

    screen = sdl_get_layout_screen_rect();
    if (sdl_main_menu_overlay_should_hide_supporting_panes())
        anchor = screen;
    else if (!sdl_overlay_pane_anchor_rect(PANE_MAIN, &anchor))
        return false;

    font_px = sdl_main_menu_pane_font_px();
    story_font = sdl_story_font_for_height(font_px);
    mono_font = sdl_main_menu_mono_font_for_height(font_px);
    if (!story_font)
        return false;
    text_h = TTF_GetFontHeight(story_font);
    if (text_h < 1)
        text_h = font_px;

    for (int i = 1; i <= MAIN_MENU_MAX; i++) {
        char shortcut[32];
        int w = sdl_touch_pane_story_text_width(story_font,
            main_menu_title(i));

        if (w > title_w)
            title_w = w;
        sdl_main_menu_shortcut_display_label(i, shortcut, sizeof(shortcut));
        if (shortcut[0]) {
            w = mono_font ? sdl_touch_pane_story_text_width(mono_font,
                shortcut) : (int)strlen(shortcut) * font_px / 2;
            if (w > shortcut_w)
                shortcut_w = w;
        }
    }

    pad_x = (float)font_px * 0.64f;
    pad_y = (float)font_px * 0.22f;
#if !SIL_SDL_MOBILE_BUILD
    row_h = (float)font_px * 1.12f;
#endif
    if (pad_x < 11.0f)
        pad_x = 11.0f;
    if (pad_y < 5.0f)
        pad_y = 5.0f;
#if !SIL_SDL_MOBILE_BUILD
    if (row_h < (float)font_px + 3.0f)
        row_h = (float)font_px + 3.0f;
#endif

    shortcut_gap = shortcut_w > 0 ? (float)font_px * 0.38f : 0.0f;
    if (shortcut_w > 0 && shortcut_gap < 5.0f)
        shortcut_gap = 5.0f;
    panel_w = pad_x * 2.0f + (float)title_w
        + (float)shortcut_w + shortcut_gap;
    if (panel_w < (float)font_px * 11.0f)
        panel_w = (float)font_px * 11.0f;
    max_panel_w = (float)screen.w - 12.0f;
    if (max_panel_w < 1.0f)
        max_panel_w = (float)screen.w;
    panel_w = sdl_touch_pane_clampf(panel_w, 1.0f, max_panel_w);

#if SIL_SDL_MOBILE_BUILD
    {
        float overlay_margin = (float)sdl_overlay_margin_px();

        max_panel_h = (float)screen.h - overlay_margin * 2.0f;
        if (max_panel_h < 1.0f)
            max_panel_h = (float)screen.h;

        panel_h = max_panel_h;
        row_h = (panel_h - pad_y * 2.0f) / (float)MAIN_MENU_MAX;
        /* Keep the empty vertical gap between adjacent labels no larger than
         * the rendered label height. */
        if (row_h > (float)text_h * 2.0f)
            row_h = (float)text_h * 2.0f;
        if (row_h < 1.0f)
            row_h = 1.0f;
        panel_h = pad_y * 2.0f + row_h * (float)MAIN_MENU_MAX;
        visible_count = MAIN_MENU_MAX;
    }
#else
    panel_h = pad_y * 2.0f + row_h * (float)MAIN_MENU_MAX;
    max_panel_h = (float)screen.h - 12.0f;
    if (max_panel_h < 1.0f)
        max_panel_h = (float)screen.h;
    if (panel_h > max_panel_h && max_panel_h > pad_y * 2.0f) {
        visible_count = (int)((max_panel_h - pad_y * 2.0f) / row_h);
        if (visible_count < 1)
            visible_count = 1;
        if (visible_count > MAIN_MENU_MAX)
            visible_count = MAIN_MENU_MAX;
        panel_h = pad_y * 2.0f + row_h * (float)visible_count;
    }
#endif

    sdl_main_menu_overlay_scroll_to_highlight(visible_count);

    if (out)
    {
        out->panel = sdl_overlay_panel_rect(&anchor, PLACE_TOP_CENTER,
            (int)(panel_w + 0.5f), (int)(panel_h + 0.5f), &screen);
        out->panel.y = (float)screen.y;
        out->pad_x = pad_x;
        out->row_h = row_h;
        out->title_w = (float)title_w;
        out->shortcut_w = (float)shortcut_w;
        out->shortcut_gap = shortcut_gap;
        out->font_px = font_px;
        out->first_choice = g_main_menu_overlay_first_choice;
        out->visible_count = visible_count;

        for (int i = 1; i <= MAIN_MENU_MAX; i++) {
            if (i < out->first_choice
                || i >= out->first_choice + out->visible_count)
            {
                out->rows[i] = (SDL_FRect){ 0 };
                continue;
            }

            out->rows[i] = (SDL_FRect){
                .x = out->panel.x + pad_x * 0.44f,
                .y = out->panel.y + pad_y
                    + (float)(i - out->first_choice) * row_h,
                .w = out->panel.w - pad_x * 0.88f,
                .h = row_h,
            };
        }
    }

    return true;
}

bool sdl_main_menu_pane_current_rect(SDL_FRect* out)
{
    main_menu_pane_layout layout;

    if (out)
        *out = (SDL_FRect){ 0 };

    if (g_main_menu_overlay_active) {
        if (!sdl_main_menu_overlay_layout(&layout))
            return false;
        if (out)
            *out = layout.panel;
        return true;
    }

    return sdl_main_menu_pane_button_rect(out);
}

bool sdl_main_menu_choice_disabled_now(int choice)
{
    return death_spectator_active() && main_menu_choice_is_disabled(choice);
}

void sdl_main_menu_overlay_reset_nav_input(void)
{
    g_gamepad_state.dpad_up = false;
    g_gamepad_state.dpad_down = false;
    g_gamepad_state.dpad_left = false;
    g_gamepad_state.dpad_right = false;
    g_gamepad_state.dpad_dir = 0;
    sdl_gamepad_clear_pending_dpad();

    g_gamepad_state.left_x = 0;
    g_gamepad_state.left_y = 0;
    g_gamepad_state.left_dir = 0;
    g_gamepad_state.left_bind_dir = -1;
    sdl_gamepad_clear_pending_left_stick();

    g_gamepad_state.right_x = 0;
    g_gamepad_state.right_y = 0;
    g_gamepad_state.right_dir = -1;

    g_main_menu_overlay_left_stick_dir = 0;
    g_main_menu_overlay_right_stick_dir = 0;
}

void sdl_main_menu_overlay_close(void)
{
    if (!g_main_menu_overlay_active)
        return;

    g_main_menu_overlay_active = false;
    g_main_menu_overlay_hover_choice = 0;
    g_main_menu_pane_hover = false;
    sdl_main_menu_overlay_reset_nav_input();
    g_state.need_present = true;
}

void sdl_main_menu_overlay_begin(void)
{
    main_menu_pane_layout layout;

    if (sdl_main_menu_button_run_pending_disable_prompt())
        return;

    if (!sdl_main_menu_overlay_layout(&layout))
    {
        log_warn("Unable to lay out the SDL main menu overlay");
        return;
    }

    (void)dismiss_active_narrative_banner();
    sdl_player_action_menu_cancel();
    sdl_touch_round_cancel_press();
    sdl_touch_top_panel_cancel_press();
    /* Drop any active hover tooltip (e.g. a terrain "granite wall" popup).
     * While the overlay is open it consumes mouse-motion events, so the
     * tooltip's own motion handler never runs to clear it, and the overlay
     * doesn't set character_icky, so the render-time safety net keeps drawing
     * it on top of the menu. */
    sdl_object_tooltip_clear();
    if (p_ptr && character_generated && character_icky == 0) {
        sdl_mark_tiles_mode_game_redraw();
        handle_stuff();
        if (Term)
            Term_fresh();
    }

    g_main_menu_overlay_active = true;
    g_main_menu_pane_hover = false;
    g_main_menu_overlay_hover_choice = 0;
    sdl_main_menu_overlay_reset_nav_input();
    if (g_main_menu_overlay_highlight < 1
        || g_main_menu_overlay_highlight > MAIN_MENU_MAX
        || sdl_main_menu_choice_disabled_now(g_main_menu_overlay_highlight))
    {
        g_main_menu_overlay_highlight = MAIN_MENU_CHARACTER;
    }
    if (sdl_main_menu_choice_disabled_now(g_main_menu_overlay_highlight))
        g_main_menu_overlay_highlight = MAIN_MENU_RETURN_GAME;

    sdl_main_menu_overlay_scroll_to_highlight(layout.visible_count);
    g_state.need_present = true;
}

void sdl_main_menu_overlay_move(int delta)
{
    main_menu_pane_layout layout;
    int choice = g_main_menu_overlay_highlight;

    if (delta == 0)
        return;
    if (choice < 1 || choice > MAIN_MENU_MAX)
        choice = MAIN_MENU_CHARACTER;

    for (int step = 0; step < MAIN_MENU_MAX; step++) {
        choice += delta;
        if (choice < 1)
            choice = MAIN_MENU_MAX;
        else if (choice > MAIN_MENU_MAX)
            choice = 1;
        if (!sdl_main_menu_choice_disabled_now(choice)) {
            g_main_menu_overlay_highlight = choice;
            if (sdl_main_menu_overlay_layout(&layout))
                sdl_main_menu_overlay_scroll_to_highlight(layout.visible_count);
            g_state.need_present = true;
            return;
        }
    }
}

void sdl_main_menu_overlay_choose(int choice)
{
    bool executed;
    bool restore_command_wait;

    if (choice < 1 || choice > MAIN_MENU_MAX)
        return;

    g_main_menu_overlay_highlight = choice;
    if (sdl_main_menu_choice_disabled_now(choice)) {
        msg_print("You can no longer take that action.");
        g_state.need_present = true;
        return;
    }

    sdl_main_menu_overlay_close();
    log_debug("sdl main menu overlay: executing choice %d", choice);
    restore_command_wait = inkey_flag;
    executed = do_cmd_main_menu_execute_choice(choice);
    /* The SDL overlay can execute modal screens while request_command() is
     * still blocked in an outer inkey(). Those modal screens call inkey()
     * themselves, whose cleanup clears inkey_flag and leaves pointer command
     * shortcuts inactive until the next physical keypress. Restore the outer
     * command-wait state after returning from the menu action. */
    if (restore_command_wait && character_icky == 0)
        inkey_flag = true;

    if (executed && choice == MAIN_MENU_SAVE_QUIT
        && (sdl_quit_transition_active() || death_spectator_active()))
    {
        log_info("sdl main menu overlay: quit choice completed; waking command loop with escape");
        Term_keypress(ESCAPE);
    }

    /* Starting Blitz ends the current session (quit-to-menu); wake the
     * blocked command loop so the relaunch into Blitz proceeds promptly. */
    if (executed && choice == MAIN_MENU_BLITZ && blitz_launch_requested())
    {
        log_info("sdl main menu overlay: blitz launch armed; waking command loop with escape");
        Term_keypress(ESCAPE);
    }
}

bool sdl_main_menu_overlay_handle_direction(int dir)
{
    if (dir < 1 || dir > 9)
        return false;

    if (ddy[dir] < 0)
        sdl_main_menu_overlay_move(-1);
    else if (ddy[dir] > 0)
        sdl_main_menu_overlay_move(1);
    else if (ddx[dir] < 0)
        sdl_main_menu_overlay_close();
    else if (ddx[dir] > 0)
        sdl_main_menu_overlay_choose(g_main_menu_overlay_highlight);
    else
        sdl_main_menu_overlay_choose(g_main_menu_overlay_highlight);

    return true;
}

bool sdl_main_menu_overlay_handle_touch_binding(int binding)
{
    if (binding == GAMEPAD_BIND_NONE)
        return false;

    if (binding == GAMEPAD_BIND_SHIFT) {
        g_touch_pane_second_panel = !g_touch_pane_second_panel;
        g_state.need_present = true;
        return true;
    }

    if (binding == ESCAPE) {
        sdl_main_menu_overlay_close();
        return true;
    }

    if (sdl_touch_pane_confirm_binding(binding)) {
        sdl_main_menu_overlay_choose(g_main_menu_overlay_highlight);
        return true;
    }

    if (sdl_touch_pane_binding_is_direction(binding))
        return sdl_main_menu_overlay_handle_direction(D2I(binding));

    return false;
}

void sdl_main_menu_overlay_flash_touch_slot(int slot)
{
    if (slot < 0)
        return;

    g_touch_pane_flash_slot = slot;
    g_touch_pane_flash_until = SDL_GetTicksNS() + 150000000ULL;
    g_state.need_present = true;
}

bool sdl_main_menu_overlay_handle_touch_pane_point(float x, float y,
    bool activate)
{
    int slot = -1;
    int panel;
    int binding;

    if (!sdl_touch_pane_point_to_slot(x, y, &slot))
        return false;
    if (slot < 0)
        return true;

    if (!activate)
        return true;

    panel = sdl_touch_pane_active_panel();
    binding = sdl_touch_pane_effective_binding_for_panel(panel, slot);

    (void)sdl_main_menu_overlay_handle_touch_binding(binding);
    sdl_main_menu_overlay_flash_touch_slot(slot);
    return true;
}

bool sdl_main_menu_overlay_handle_gamepad_axis(
    const SDL_GamepadAxisEvent* ev)
{
    int deadzone;
    int dir;

    if (!g_main_menu_overlay_active || !ev)
        return false;
    if (!config.gamepad_enabled)
        return true;

    sdl_gamepad_mark_auto_ui();

    deadzone = config.gamepad_deadzone;
    if (deadzone < 0)
        deadzone = 0;

    if (ev->axis == SDL_GAMEPAD_AXIS_LEFTX
        || ev->axis == SDL_GAMEPAD_AXIS_LEFTY)
    {
        if (ev->axis == SDL_GAMEPAD_AXIS_LEFTX)
            g_gamepad_state.left_x = ev->value;
        else
            g_gamepad_state.left_y = ev->value;

        dir = sdl_gamepad_axis_to_dir(g_gamepad_state.left_x,
            g_gamepad_state.left_y, deadzone);
        if (dir != g_main_menu_overlay_left_stick_dir) {
            g_main_menu_overlay_left_stick_dir = dir;
            if (dir)
                (void)sdl_main_menu_overlay_handle_direction(dir);
        }
        return true;
    }

    if (ev->axis == SDL_GAMEPAD_AXIS_RIGHTX
        || ev->axis == SDL_GAMEPAD_AXIS_RIGHTY)
    {
        if (ev->axis == SDL_GAMEPAD_AXIS_RIGHTX)
            g_gamepad_state.right_x = ev->value;
        else
            g_gamepad_state.right_y = ev->value;

        dir = sdl_gamepad_axis_to_dir(g_gamepad_state.right_x,
            g_gamepad_state.right_y, deadzone);
        if (dir != g_main_menu_overlay_right_stick_dir) {
            g_main_menu_overlay_right_stick_dir = dir;
            if (dir)
                (void)sdl_main_menu_overlay_handle_direction(dir);
        }
        return true;
    }

    return false;
}

static Uint8 sdl_popup_notification_alpha(Uint64 now_ns)
{
    Uint64 fade_ns =
        (Uint64)SDL_NARRATIVE_BANNER_FADE_MS * 1000000ULL;
    Uint64 amount_ns;

    if (!g_popup_notification.active || fade_ns == 0)
        return 0;

    if (now_ns < g_popup_notification.fade_in_end_ns) {
        Uint64 start_ns = g_popup_notification.fade_in_end_ns - fade_ns;

        amount_ns = (now_ns > start_ns) ? now_ns - start_ns : 0;
    } else if (now_ns < g_popup_notification.hold_end_ns) {
        return SDL_ALPHA_OPAQUE;
    } else if (now_ns < g_popup_notification.end_ns) {
        amount_ns = g_popup_notification.end_ns - now_ns;
    } else {
        return 0;
    }

    return (Uint8)((amount_ns * SDL_ALPHA_OPAQUE + fade_ns / 2) / fade_ns);
}

static Uint8 sdl_popup_notification_scaled_alpha(Uint8 alpha,
    Uint8 scale)
{
    if (!alpha || !scale)
        return 0;
    return (Uint8)(((int)alpha * (int)scale + SDL_ALPHA_OPAQUE / 2)
        / SDL_ALPHA_OPAQUE);
}

static bool sdl_popup_notification_layout(SDL_FRect* out_panel,
    TTF_Font** out_font)
{
    SDL_FRect menu_rect;
    SDL_Rect screen;
    TTF_Font* font;
    int font_px;
    int text_w;
    float pad_x;
    float pad_y;
    float gap;
    float panel_w;
    float panel_h;
    float max_panel_w;
    SDL_FRect panel;
    bool menu_button_visible;

    if (!g_popup_notification.active || g_main_menu_overlay_active
        || !sdl_main_menu_pane_context_visible())
    {
        return false;
    }
    menu_button_visible = sdl_main_menu_pane_button_rect(&menu_rect);

    screen = sdl_get_layout_screen_rect();
    font_px = sdl_main_menu_pane_font_px();
    font = sdl_story_font_for_height_slot(font_px, SDL_STORY_FONT_SLOT_MENU);
    if (!font)
        return false;

    text_w = sdl_touch_pane_story_text_width(font,
        g_popup_notification.text);
    if (text_w < 1)
        return false;

    pad_x = sdl_touch_pane_clampf((float)font_px * 0.72f, 9.0f, 24.0f);
    pad_y = sdl_touch_pane_clampf((float)font_px * 0.28f, 4.0f, 12.0f);
    gap = sdl_touch_pane_clampf((float)font_px * 0.22f, 3.0f, 9.0f);
    panel_w = (float)text_w + pad_x * 2.0f;
    if (menu_button_visible && panel_w < menu_rect.w)
        panel_w = menu_rect.w;

    max_panel_w = (float)screen.w - (float)sdl_overlay_margin_px() * 2.0f;
    if (max_panel_w < 1.0f)
        max_panel_w = (float)screen.w;
    if (panel_w > max_panel_w)
        panel_w = max_panel_w;

    panel_h = (float)font_px * 1.10f + pad_y * 2.0f;
    panel = (SDL_FRect){ .w = panel_w, .h = panel_h };
    if (menu_button_visible) {
        panel.x = menu_rect.x + (menu_rect.w - panel_w) * 0.5f;
        panel.y = menu_rect.y + menu_rect.h + gap;
    } else {
        panel.x = (float)screen.x + ((float)screen.w - panel_w) * 0.5f;
        panel.y = (float)screen.y + gap;
    }

    if (panel.x < (float)screen.x + (float)sdl_overlay_margin_px())
        panel.x = (float)screen.x + (float)sdl_overlay_margin_px();
    if (panel.x + panel.w
        > (float)(screen.x + screen.w - sdl_overlay_margin_px()))
    {
        panel.x = (float)(screen.x + screen.w - sdl_overlay_margin_px())
            - panel.w;
    }
    if (panel.y + panel.h
        > (float)(screen.y + screen.h - sdl_overlay_margin_px()))
    {
        panel.y = (float)(screen.y + screen.h - sdl_overlay_margin_px())
            - panel.h;
    }

    if (out_panel)
        *out_panel = panel;
    if (out_font)
        *out_font = font;
    return true;
}

void sdl_popup_notification_render(void)
{
    SDL_FRect panel;
    SDL_FRect shadow;
    TTF_Font* font;
    SDL_Color text;
    Uint8 alpha;

    alpha = sdl_popup_notification_alpha(SDL_GetTicksNS());
    if (!alpha || !sdl_popup_notification_layout(&panel, &font))
        return;

    shadow = panel;
    shadow.x += 2.0f;
    shadow.y += 2.0f;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0,
        sdl_popup_notification_scaled_alpha(alpha, 126));
    SDL_RenderFillRect(g_state.renderer, &shadow);
    SDL_SetRenderDrawColor(g_state.renderer, 8, 10, 12,
        sdl_popup_notification_scaled_alpha(alpha, 226));
    SDL_RenderFillRect(g_state.renderer, &panel);
    SDL_SetRenderDrawColor(g_state.renderer, 255, 184, 94,
        sdl_popup_notification_scaled_alpha(alpha, 154));
    SDL_RenderRect(g_state.renderer, &panel);

    text = g_state.palette[TERM_L_WHITE];
    text.a = alpha;
    (void)sdl_main_menu_draw_text(font, g_popup_notification.text,
        panel.x + panel.w * 0.07f, panel.y, panel.w * 0.86f, panel.h,
        text, MAIN_MENU_TEXT_CENTER);
}

void sdl_main_menu_pane_render(void)
{
    main_menu_pane_layout layout;
    SDL_FRect rect;
    SDL_FRect shadow;
    SDL_Color text;
    TTF_Font* story_font;
    TTF_Font* mono_font;

    if (g_main_menu_overlay_active) {
        if (!sdl_main_menu_overlay_layout(&layout))
            return;

        story_font = sdl_story_font_for_height(layout.font_px);
        mono_font = sdl_main_menu_mono_font_for_height(layout.font_px);
        if (!story_font)
            return;

        shadow = layout.panel;
        shadow.x += 3.0f;
        shadow.y += 3.0f;

        SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 154);
        SDL_RenderFillRect(g_state.renderer, &shadow);
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 242);
        SDL_RenderFillRect(g_state.renderer, &layout.panel);

        for (int i = layout.first_choice;
             i < layout.first_choice + layout.visible_count
                 && i <= MAIN_MENU_MAX;
             i++)
        {
            SDL_FRect row = layout.rows[i];
            bool selected = (i == g_main_menu_overlay_highlight);
            bool disabled = sdl_main_menu_choice_disabled_now(i);
            SDL_Color row_text = disabled
                ? g_state.palette[TERM_L_DARK]
                : g_state.palette[selected ? TERM_L_BLUE : TERM_WHITE];
            SDL_Color shortcut = row_text;
            char shortcut_label[32];
            float inner = layout.pad_x * 0.42f;
            float title_x = row.x + inner;
            float title_w = layout.title_w;
            float available_title_w = row.w - inner * 2.0f;

            sdl_main_menu_shortcut_display_label(i, shortcut_label,
                sizeof(shortcut_label));
            if (shortcut_label[0] && layout.shortcut_w > 0.0f)
                available_title_w -= layout.shortcut_w + layout.shortcut_gap;
            if (title_w > available_title_w)
                title_w = available_title_w;
            if (title_w < 1.0f)
                title_w = 1.0f;

            (void)sdl_main_menu_draw_text(story_font, main_menu_title(i),
                title_x, row.y, title_w, row.h, row_text,
                MAIN_MENU_TEXT_LEFT);

            if (shortcut_label[0] && layout.shortcut_w > 0.0f && mono_font) {
                float shortcut_x = title_x + title_w + layout.shortcut_gap;
                float right_limit = row.x + row.w - inner;

                if (shortcut_x + layout.shortcut_w > right_limit)
                    shortcut_x = right_limit - layout.shortcut_w;
                if (shortcut_x < title_x + title_w)
                    shortcut_x = title_x + title_w;
                sdl_main_menu_draw_text(mono_font, shortcut_label,
                    shortcut_x, row.y, layout.shortcut_w, row.h, shortcut,
                    MAIN_MENU_TEXT_LEFT);
            }
        }
        return;
    }

    if (!sdl_main_menu_pane_current_rect(&rect))
        return;

    text = g_state.palette[g_main_menu_pane_hover
        ? TERM_YELLOW : TERM_L_WHITE];

    shadow = rect;
    shadow.x += 2.0f;
    shadow.y += 2.0f;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 126);
    SDL_RenderFillRect(g_state.renderer, &shadow);
    if (g_main_menu_pane_hover)
        SDL_SetRenderDrawColor(g_state.renderer, 22, 24, 18, 238);
    else
        SDL_SetRenderDrawColor(g_state.renderer, 8, 10, 12, 226);
    SDL_RenderFillRect(g_state.renderer, &rect);

    story_font = sdl_story_font_for_height(sdl_main_menu_pane_font_px());
    sdl_main_menu_draw_text(story_font, "Menu", rect.x + rect.w * 0.10f,
        rect.y, rect.w * 0.80f, rect.h, text,
        MAIN_MENU_TEXT_CENTER);
}

bool sdl_main_menu_pane_hit(float x, float y, SDL_FRect* out_rect)
{
    SDL_FRect rect;

    if (!sdl_main_screen_click_shortcuts_active())
        return false;
    if (!sdl_main_menu_pane_current_rect(&rect))
        return false;
    if (out_rect)
        *out_rect = rect;

    return x >= rect.x && x < rect.x + rect.w
        && y >= rect.y && y < rect.y + rect.h;
}

bool sdl_main_menu_pane_handle_hover_pointer(float x, float y)
{
    bool hit = sdl_main_menu_pane_hit(x, y, NULL);

    if (g_main_menu_pane_hover != hit)
    {
        g_main_menu_pane_hover = hit;
        g_state.need_present = true;
    }

    if (hit)
        sdl_pointer_attack_set_panel_hover_mode(SDL_POINTER_ATTACK_NONE);

    return hit;
}

bool sdl_main_menu_pane_handle_pointer(float x, float y)
{
    if (!sdl_main_menu_pane_hit(x, y, NULL))
        return false;

    g_main_menu_pane_hover = false;
    g_state.need_present = true;
    sdl_main_menu_overlay_begin();
    return true;
}

static void sdl_main_menu_button_clear_press(void)
{
    bool redraw = g_main_menu_button_press.active || g_main_menu_pane_hover;

    memset(&g_main_menu_button_press, 0,
        sizeof(g_main_menu_button_press));
    g_main_menu_pane_hover = false;
    if (redraw)
        g_state.need_present = true;
}

static void sdl_main_menu_button_queue_disable_prompt(void)
{
    sdl_main_menu_button_clear_press();
    if (g_main_menu_button_disable_prompt_pending)
        return;

    g_main_menu_button_disable_prompt_pending = true;
    Term_keypress('m');
}

bool sdl_main_menu_button_handle_secondary_pointer(float x, float y)
{
    if (!sdl_main_menu_pane_hit(x, y, NULL))
        return false;

    sdl_main_menu_button_queue_disable_prompt();
    return true;
}

bool sdl_main_menu_button_handle_long_press_down(float x, float y,
    SDL_FingerID finger_id)
{
    if (!sdl_main_menu_pane_hit(x, y, NULL))
        return false;

    sdl_main_menu_button_clear_press();
    g_main_menu_button_press.active = true;
    g_main_menu_button_press.finger_id = finger_id;
    g_main_menu_button_press.start_x = x;
    g_main_menu_button_press.start_y = y;
    g_main_menu_button_press.start_time = SDL_GetTicksNS();
    g_main_menu_pane_hover = true;
    g_state.need_present = true;
    return true;
}

bool sdl_main_menu_button_handle_long_press_motion(float x, float y,
    SDL_FingerID finger_id)
{
    float dx;
    float dy;
    float threshold;

    if (!g_main_menu_button_press.active
        || g_main_menu_button_press.finger_id != finger_id)
    {
        return false;
    }
    if (g_main_menu_button_press.canceled)
        return true;

    dx = x - g_main_menu_button_press.start_x;
    dy = y - g_main_menu_button_press.start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;
    threshold = sdl_touch_swipe_threshold_px();
    if (dx > threshold || dy > threshold) {
        g_main_menu_button_press.canceled = true;
        g_main_menu_pane_hover = false;
        g_state.need_present = true;
    }

    return true;
}

bool sdl_main_menu_button_handle_long_press_up(float x, float y,
    SDL_FingerID finger_id)
{
    bool activate;

    if (!g_main_menu_button_press.active
        || g_main_menu_button_press.finger_id != finger_id)
    {
        return false;
    }

    activate = !g_main_menu_button_press.canceled
        && sdl_main_menu_pane_hit(x, y, NULL);
    sdl_main_menu_button_clear_press();
    if (activate)
        sdl_main_menu_overlay_begin();
    return true;
}

void sdl_main_menu_button_cancel_long_press(SDL_FingerID finger_id)
{
    if (!g_main_menu_button_press.active
        || g_main_menu_button_press.finger_id != finger_id)
    {
        return;
    }

    sdl_main_menu_button_clear_press();
}

int sdl_main_menu_button_pending_timeout_ms(Uint64 now_ns)
{
    Uint64 elapsed;

    if (!g_main_menu_button_press.active
        || g_main_menu_button_press.canceled)
    {
        return -1;
    }
    if (!sdl_main_menu_pane_hit(g_main_menu_button_press.start_x,
            g_main_menu_button_press.start_y, NULL))
    {
        return 0;
    }

    elapsed = now_ns - g_main_menu_button_press.start_time;
    if (elapsed >= (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
        return 0;
    return TOUCH_PANE_LONG_PRESS_MS - (int)(elapsed / 1000000ULL);
}

bool sdl_main_menu_button_flush_pending_press(Uint64 now_ns)
{
    if (!g_main_menu_button_press.active)
        return false;
    if (!sdl_main_menu_pane_hit(g_main_menu_button_press.start_x,
            g_main_menu_button_press.start_y, NULL))
    {
        sdl_main_menu_button_clear_press();
        return false;
    }
    if (g_main_menu_button_press.canceled
        || now_ns - g_main_menu_button_press.start_time
            < (Uint64)TOUCH_PANE_LONG_PRESS_MS * 1000000ULL)
    {
        return false;
    }

    sdl_main_menu_button_queue_disable_prompt();
    return true;
}

static bool sdl_quick_access_has_binding(int binding)
{
    int count = get_sdl_touch_top_panel_cell_count();

    for (int i = 0; i < count; i++) {
        if (get_sdl_touch_top_panel_binding(i, false) == binding
            || get_sdl_touch_top_panel_binding(i, true) == binding)
        {
            return true;
        }
    }

    return false;
}

static bool sdl_quick_access_profile_insert_binding(
    struct sdl_pane_profile* profile, int binding)
{
    int count;
    int insert_at;

    if (!profile)
        return false;
    count = profile->touch_top_panel_cell_count;
    if (count < 0)
        count = 0;
    if (count > SDL_TOUCH_TOP_PANEL_BUTTON_COUNT)
        count = SDL_TOUCH_TOP_PANEL_BUTTON_COUNT;

    for (int i = 0; i < count; i++) {
        if (profile->touch_top_panel_bindings[i] == binding
            || profile->touch_top_panel_long_bindings[i] == binding)
        {
            return true;
        }
    }
    for (int i = 0; i < count; i++) {
        if (profile->touch_top_panel_bindings[i] == GAMEPAD_BIND_NONE
            && profile->touch_top_panel_long_bindings[i] == GAMEPAD_BIND_NONE)
        {
            profile->touch_top_panel_bindings[i] = binding;
            return true;
        }
    }
    if (count >= SDL_TOUCH_TOP_PANEL_BUTTON_COUNT)
        return false;

    /* Keep the portrait Main Menu entry last when a newly unlocked skill is
     * accepted later in the run. */
    insert_at = count;
    for (int i = 0; i < count; i++) {
        if (profile->touch_top_panel_bindings[i] == 'm'
            || profile->touch_top_panel_long_bindings[i] == 'm')
        {
            insert_at = i;
            break;
        }
    }
    for (int i = count; i > insert_at; i--) {
        profile->touch_top_panel_bindings[i] =
            profile->touch_top_panel_bindings[i - 1];
        profile->touch_top_panel_long_bindings[i] =
            profile->touch_top_panel_long_bindings[i - 1];
    }
    profile->touch_top_panel_bindings[insert_at] = binding;
    profile->touch_top_panel_long_bindings[insert_at] = GAMEPAD_BIND_NONE;
    profile->touch_top_panel_cell_count = count + 1;
    return true;
}

static void sdl_quick_access_suggest_unlocked_shortcut(cptr unlock_name,
    cptr action_name, int binding, bool character_wheel_available,
    cptr custom_desc)
{
    ui_question_option options[2];
    int count;
    bool has_room = false;
    char desc[320];
    int choice;

    if (!g_state.window || death_spectator_active())
        return;
    if (!unlock_name || !unlock_name[0] || !action_name || !action_name[0])
        return;
    if (sdl_quick_access_has_binding(binding))
        return;

    count = get_sdl_touch_top_panel_cell_count();
    if (count < SDL_TOUCH_TOP_PANEL_BUTTON_COUNT)
        has_room = true;
    for (int i = 0; !has_room && i < count; i++) {
        has_room = get_sdl_touch_top_panel_binding(i, false)
                == GAMEPAD_BIND_NONE
            && get_sdl_touch_top_panel_binding(i, true) == GAMEPAD_BIND_NONE;
    }
    if (!has_room)
        return;

    if (custom_desc && custom_desc[0]) {
        SDL_strlcpy(desc, custom_desc, sizeof(desc));
    } else if (character_wheel_available) {
        strnfmt(desc, sizeof(desc),
            "You acquired %s. Add %s to Quick Access? You can also use it "
            "through the character wheel.",
            unlock_name, action_name);
    } else {
        strnfmt(desc, sizeof(desc),
            "You acquired %s. Add %s to Quick Access?",
            unlock_name, action_name);
    }
    options[0] = (ui_question_option){ 'y', "Add to Quick Access",
        TERM_L_GREEN, false };
    options[1]
        = (ui_question_option){ 'n', "Not now", TERM_SLATE, false };
    choice = ui_question_ask("New Quick Access action", desc, options, 2,
        UI_QUESTION_GLOBAL, UI_QUESTION_GLOBAL, 1);
    if (choice != 0)
        return;

    /* Apply the accepted shortcut to each saved layout.  Otherwise the
     * one-time skill transition could add it only to the orientation that
     * happened to be active when the player trained. */
    sdl_store_active_pane_profile(config.min_terminal_mode);
    for (int i = 0; i < SDL_PANE_PROFILE_COUNT; i++)
        (void)sdl_quick_access_profile_insert_binding(&g_pane_profiles[i],
            binding);
    sdl_apply_stored_pane_profile(config.min_terminal_mode);
    sdl_apply_config();
    (void)save_pane_config_to_json();
    log_info("Added %s to Quick Access after %s %s", action_name,
        custom_desc ? "equipping" : "acquiring", unlock_name);
}

void sdl_quick_access_suggest_skill_shortcut(int skill)
{
    if (skill == S_SNG) {
        sdl_quick_access_suggest_unlocked_shortcut("Song", "Sing", 's',
            false, NULL);
    } else if (skill == S_SMT) {
        sdl_quick_access_suggest_unlocked_shortcut("Smithing", "Smithing",
            '0', false, NULL);
    }
}

void sdl_quick_access_suggest_ability_shortcut(int skill, int ability)
{
    if (skill == S_ARC && ability == ARC_FLETCHERY) {
        sdl_quick_access_suggest_unlocked_shortcut("Fletchery", "Fletch",
            '-', true, NULL);
    } else if (skill == S_STL && ability == STL_EXCHANGE_PLACES) {
        sdl_quick_access_suggest_unlocked_shortcut("Exchange Places",
            "Exchange Places", 'X', true, NULL);
    }
}

void sdl_quick_access_suggest_equipped_item(int tval)
{
    byte prompt_flag;
    cptr item_name;
    cptr action_name;
    cptr wheel_verb;
    int binding;
    char desc[320];

    if (!p_ptr)
        return;

    if (tval == TV_STAFF) {
        prompt_flag = QUICK_ACCESS_PROMPT_STAFF;
        item_name = "a staff";
        action_name = "Activate staff";
        wheel_verb = "activate";
        binding = 'a';
    } else if (tval == TV_HORN) {
        prompt_flag = QUICK_ACCESS_PROMPT_HORN;
        item_name = "a horn";
        action_name = "Blow horn";
        wheel_verb = "blow";
        binding = 'p';
    } else {
        return;
    }

    if (p_ptr->quick_access_prompt_flags & prompt_flag)
        return;

    /* The first successful equip consumes the offer even when it is declined. */
    p_ptr->quick_access_prompt_flags |= prompt_flag;
    strnfmt(desc, sizeof(desc),
        "You equipped %s for the first time this run. Add %s to Quick "
        "Access? You can also %s it from the character wheel.",
        item_name, action_name, wheel_verb);
    sdl_quick_access_suggest_unlocked_shortcut(item_name, action_name,
        binding, true, desc);
}

void sdl_quick_access_suggest_starting_shortcuts(void)
{
    if (!p_ptr)
        return;

    if (p_ptr->skill_base[S_SNG] > 0)
        sdl_quick_access_suggest_skill_shortcut(S_SNG);
    if (p_ptr->skill_base[S_SMT] > 0)
        sdl_quick_access_suggest_skill_shortcut(S_SMT);
    if (p_ptr->innate_ability[S_ARC][ARC_FLETCHERY])
        sdl_quick_access_suggest_ability_shortcut(S_ARC, ARC_FLETCHERY);
    if (p_ptr->innate_ability[S_STL][STL_EXCHANGE_PLACES])
        sdl_quick_access_suggest_ability_shortcut(S_STL,
            STL_EXCHANGE_PLACES);
}

static bool sdl_main_menu_button_run_pending_disable_prompt(void)
{
    ui_question_option options[2];
    const char* desc =
        "Turn off the fixed Main Menu button? Main Menu will be added to "
        "Quick Access, and Quick Access will be enabled if needed.";
    int choice;

    if (!g_main_menu_button_disable_prompt_pending)
        return false;
    g_main_menu_button_disable_prompt_pending = false;

    options[0]
        = (ui_question_option){ 'y', "Turn off button", TERM_ORANGE, false };
    options[1]
        = (ui_question_option){ 'c', "Cancel", TERM_SLATE, false };

    choice = ui_question_ask("Turn off Main Menu button", desc, options,
        2, UI_QUESTION_GLOBAL, UI_QUESTION_GLOBAL, 1);
    if (choice != 0)
        return true;

    set_sdl_show_main_menu_button(false);
    sdl_apply_config();
    (void)save_pane_config_to_json();
    log_info("Fixed Main Menu button disabled with Quick Access recovery configured");
    return true;
}

int sdl_main_menu_overlay_choice_at(float x, float y, bool* in_panel)
{
    main_menu_pane_layout layout;

    if (in_panel)
        *in_panel = false;
    if (!g_main_menu_overlay_active)
        return 0;
    if (!sdl_main_menu_overlay_layout(&layout))
        return 0;

    if (x < layout.panel.x || x >= layout.panel.x + layout.panel.w
        || y < layout.panel.y || y >= layout.panel.y + layout.panel.h)
    {
        return 0;
    }

    if (in_panel)
        *in_panel = true;
    for (int i = layout.first_choice;
         i < layout.first_choice + layout.visible_count && i <= MAIN_MENU_MAX;
         i++)
    {
        SDL_FRect row = layout.rows[i];

        if (x >= row.x && x < row.x + row.w
            && y >= row.y && y < row.y + row.h)
        {
            return i;
        }
    }

    return 0;
}

bool sdl_main_menu_overlay_handle_pointer_motion(float x, float y)
{
    bool in_panel = false;
    int choice = sdl_main_menu_overlay_choice_at(x, y, &in_panel);

    if (!g_main_menu_overlay_active)
        return false;

    if (choice > 0 && !sdl_main_menu_choice_disabled_now(choice))
        g_main_menu_overlay_highlight = choice;
    if (g_main_menu_overlay_hover_choice != choice) {
        g_main_menu_overlay_hover_choice = choice;
        g_state.need_present = true;
    }
    sdl_pointer_attack_set_panel_hover_mode(SDL_POINTER_ATTACK_NONE);
    (void)in_panel;
    return true;
}

bool sdl_main_menu_overlay_handle_pointer_down(float x, float y)
{
    bool in_panel = false;
    int choice = sdl_main_menu_overlay_choice_at(x, y, &in_panel);

    if (!g_main_menu_overlay_active)
        return false;

    if (choice > 0) {
        sdl_main_menu_overlay_choose(choice);
        return true;
    }
    if (!in_panel)
        sdl_main_menu_overlay_close();
    return true;
}

bool sdl_main_menu_overlay_handle_key(const SDL_KeyboardEvent* ev)
{
    int key;
    int choice;

    if (!g_main_menu_overlay_active || !ev)
        return false;

    key = ev->key;
    if (sdl_key_is_escape_or_back(key) || key == SDLK_LEFT
        || key == SDLK_KP_4)
    {
        sdl_main_menu_overlay_close();
        return true;
    }
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE
        || key == SDLK_RIGHT || key == SDLK_KP_6)
    {
        sdl_main_menu_overlay_choose(g_main_menu_overlay_highlight);
        return true;
    }
    if (key == SDLK_UP || key == SDLK_KP_8) {
        sdl_main_menu_overlay_move(-1);
        return true;
    }
    if (key == SDLK_DOWN || key == SDLK_KP_2) {
        sdl_main_menu_overlay_move(1);
        return true;
    }

    if (key > 0 && key < 256) {
        int ch = key;

        if (SDL_isalpha(ch))
            ch = tolower((unsigned char)ch);
        choice = main_menu_choice_from_key(ch);
        if (choice > 0) {
            sdl_main_menu_overlay_choose(choice);
            return true;
        }
        if (ch == '8') {
            sdl_main_menu_overlay_move(-1);
            return true;
        }
        if (ch == '2') {
            sdl_main_menu_overlay_move(1);
            return true;
        }
        if (ch == '4') {
            sdl_main_menu_overlay_close();
            return true;
        }
        if (ch == '6') {
            sdl_main_menu_overlay_choose(g_main_menu_overlay_highlight);
            return true;
        }
    }

    return true;
}

bool sdl_main_menu_overlay_handle_gamepad_button(
    const SDL_GamepadButtonEvent* ev)
{
    SDL_GamepadButton button;
    int binding;
    int choice;

    if (!g_main_menu_overlay_active || !ev)
        return false;
    if (!ev->down)
        return true;

    sdl_gamepad_mark_auto_ui();
    button = (SDL_GamepadButton)ev->button;
    binding = sdl_gamepad_capture_binding_for_input(GAMEPAD_CAPTURE_BUTTON,
        (int)button);

    if (button == SDL_GAMEPAD_BUTTON_DPAD_UP || binding == '8') {
        sdl_main_menu_overlay_move(-1);
        return true;
    }
    if (button == SDL_GAMEPAD_BUTTON_DPAD_DOWN || binding == '2') {
        sdl_main_menu_overlay_move(1);
        return true;
    }
    if (button == SDL_GAMEPAD_BUTTON_DPAD_LEFT
        || binding == steamdeck_back_key())
    {
        sdl_main_menu_overlay_close();
        return true;
    }
    if (button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT
        || binding == steamdeck_confirm_key())
    {
        sdl_main_menu_overlay_choose(g_main_menu_overlay_highlight);
        return true;
    }

    choice = main_menu_choice_from_key(binding);
    if (choice > 0) {
        sdl_main_menu_overlay_choose(choice);
        return true;
    }

    if (button == SDL_GAMEPAD_BUTTON_EAST) {
        sdl_main_menu_overlay_close();
        return true;
    }
    if (button == SDL_GAMEPAD_BUTTON_SOUTH) {
        sdl_main_menu_overlay_choose(g_main_menu_overlay_highlight);
        return true;
    }

    return true;
}

bool sdl_main_menu_overlay_handle_event(const SDL_Event* ev)
{
    float x;
    float y;

    if (!g_main_menu_overlay_active || !ev)
        return false;

    switch (ev->type)
    {
    case SDL_EVENT_KEY_DOWN:
        return sdl_main_menu_overlay_handle_key(&ev->key);
    case SDL_EVENT_MOUSE_MOTION:
        {
            bool in_panel = false;

            (void)sdl_main_menu_overlay_choice_at((float)ev->motion.x,
                (float)ev->motion.y, &in_panel);
            if (!in_panel && sdl_main_menu_overlay_handle_touch_pane_point(
                    (float)ev->motion.x, (float)ev->motion.y, false))
            {
                return true;
            }
        }
        return sdl_main_menu_overlay_handle_pointer_motion(
            (float)ev->motion.x, (float)ev->motion.y);
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (ev->button.button == SDL_BUTTON_RIGHT) {
            sdl_main_menu_overlay_close();
            return true;
        }
        if (ev->button.button == SDL_BUTTON_LEFT) {
            {
                bool in_panel = false;

                (void)sdl_main_menu_overlay_choice_at((float)ev->button.x,
                    (float)ev->button.y, &in_panel);
                if (!in_panel && sdl_main_menu_overlay_handle_touch_pane_point(
                        (float)ev->button.x, (float)ev->button.y, true))
                {
                    return true;
                }
            }
            return sdl_main_menu_overlay_handle_pointer_down(
                (float)ev->button.x, (float)ev->button.y);
        }
        return true;
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
        return true;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return true;
        if (!sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return true;
        {
            bool in_panel = false;

            (void)sdl_main_menu_overlay_choice_at(x, y, &in_panel);
            if (!in_panel && sdl_main_menu_overlay_handle_touch_pane_point(x, y,
                    ev->type == SDL_EVENT_FINGER_DOWN))
            {
                return true;
            }
        }
        if (ev->type == SDL_EVENT_FINGER_DOWN)
            return sdl_main_menu_overlay_handle_pointer_down(x, y);
        return sdl_main_menu_overlay_handle_pointer_motion(x, y);
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        return true;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        return sdl_main_menu_overlay_handle_gamepad_button(&ev->gbutton);
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        return sdl_main_menu_overlay_handle_gamepad_axis(&ev->gaxis);
    default:
        return false;
    }
}
