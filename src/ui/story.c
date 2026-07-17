/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "ui/story.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "sdl-config.h"
#include "sdl-sound.h"
#include <stdbool.h>
#include <string.h>

/*
 * The Tale So Far is an SDL semantic-pixel screen.  Story entries are
 * selected by the engine, laid out in pixels, and animated with native
 * texture alpha.  There is deliberately no terminal-grid fallback.
 */

enum {
    STORY_SEMANTIC_FADE_MS = 800,
    STORY_SEMANTIC_HOLD_MS = 2000,
    STORY_SEMANTIC_FRAME_MS = 16
};

static bool story_fast_forward_key(char ch)
{
    return ch == ESCAPE
        || (steamdeck_controls_active() && ch == steamdeck_back_key());
}

static void story_prompt_label(int binding, const char* fallback, char* buf,
    size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback, buflen);
}

static void story_semantic_prompt_text(bool final, char* buf,
    size_t buflen)
{
    if (!buf || buflen == 0)
        return;

    if (sdl_touch_only_device_active())
    {
        SDL_strlcpy(buf, final ? "[Tap to continue]"
            : "[Tap to continue]  *  [Back] fast forward", buflen);
        return;
    }

    if (steamdeck_controls_active())
    {
        char next_label[16];
        char back_label[16];

        story_prompt_label(steamdeck_confirm_key(), "A", next_label,
            sizeof(next_label));
        if (final)
        {
            strnfmt(buf, buflen, "[%s] continue", next_label);
        }
        else
        {
            story_prompt_label(steamdeck_back_key(), "B", back_label,
                sizeof(back_label));
            strnfmt(buf, buflen, "[%s] next  *  [%s] fast forward",
                next_label, back_label);
        }
        return;
    }

    SDL_strlcpy(buf, final ? "[Press any key to continue]"
        : "[Enter] next  *  [Esc] fast forward", buflen);
}

static int story_semantic_transition_key(void)
{
    char ch;

    if (Term_inkey(&ch, false, false) != 0)
        return 0;
    (void)Term_inkey(&ch, false, true);
    return story_fast_forward_key(ch) ? 2 : 1;
}

/* Heading and paragraph share one alpha and animate as one message. */
static int story_semantic_alpha_transition(int entry, int from_alpha,
    int to_alpha)
{
    int frames = MAX(1, STORY_SEMANTIC_FADE_MS
        / STORY_SEMANTIC_FRAME_MS);

    for (int frame = 0; frame <= frames; frame++)
    {
        int key = story_semantic_transition_key();
        float progress = (float)frame / (float)frames;
        int alpha;

        if (key)
        {
            sdl_tale_screen_set_active_entry(entry, (byte)to_alpha);
            Term_fresh();
            return key;
        }

        progress = progress * progress * (3.0f - 2.0f * progress);
        alpha = from_alpha
            + (int)((float)(to_alpha - from_alpha) * progress + 0.5f);
        sdl_tale_screen_set_active_entry(entry,
            (byte)MAX(0, MIN(255, alpha)));
        Term_fresh();
        if (frame < frames)
            Term_xtra(TERM_XTRA_DELAY, STORY_SEMANTIC_FRAME_MS);
    }

    return 0;
}

static int story_semantic_hold_entry(void)
{
    int elapsed = 0;

    while (elapsed < STORY_SEMANTIC_HOLD_MS)
    {
        int key = story_semantic_transition_key();
        int delay;

        if (key)
            return key;
        delay = MIN(STORY_SEMANTIC_FRAME_MS,
            STORY_SEMANTIC_HOLD_MS - elapsed);
        Term_xtra(TERM_XTRA_DELAY, delay);
        elapsed += delay;
    }

    return 0;
}

static int story_semantic_animate_entry(int entry, bool animate,
    bool disappear)
{
    int result = 0;

    if (animate)
    {
        result = story_semantic_alpha_transition(entry, 0, 255);
        if (result == 2)
            return result;
    }
    else
    {
        sdl_tale_screen_set_active_entry(entry, 255);
        Term_fresh();
    }

    result = story_semantic_hold_entry();
    if (result == 2)
        return result;

    if (disappear)
    {
        if (animate)
            result = story_semantic_alpha_transition(entry, 255, 0);
        else
        {
            sdl_tale_screen_set_active_entry(entry, 0);
            Term_fresh();
        }
    }
    return result == 2 ? 2 : 0;
}

static void print_story_sdl(const int* sel_idx, int start, int total,
    bool fade_in)
{
    bool fast_forward = false;
    bool saved_cursor_state = false;
    bool saved_hide_cursor = false;
    char prompt[96];
    int completed_entry = -1;
    int final_entry = total - start - 1;

    if (!sel_idx || start < 0 || start >= total)
        return;
    if (!sdl_tale_screen_begin("=== The Tale So Far ==="))
    {
        log_error("Unable to open the SDL Tale screen");
        return;
    }

    for (int idx = start; idx < total; idx++)
    {
        const story_type* st = &st_info[sel_idx[idx]];
        cptr heading = st->name ? st_name + st->name : "";
        cptr body = st->text ? st_text + st->text : "";

        sdl_tale_screen_add_entry(heading, body);
    }

    screen_push_supporting_panes_hidden();
    screen_push_touch_pane_hidden();
    Term_clear();
    (void)Term_get_cursor(&saved_cursor_state);
    saved_hide_cursor = hide_cursor;
    hide_cursor = true;
    (void)Term_set_cursor(false);
    sdl_tale_screen_set_active_entry(-1, 0);

    for (;;)
    {
        int page_entries = sdl_tale_screen_current_page_entry_count();

        if (!fast_forward)
        {
            story_semantic_prompt_text(false, prompt, sizeof(prompt));
            sdl_tale_screen_set_prompt(prompt, true, false);
        }
        else
        {
            sdl_tale_screen_set_prompt("", false, false);
        }

        for (int position = 0; position < page_entries; position++)
        {
            int entry = sdl_tale_screen_current_page_entry_at(position);

            if (entry < 0 || entry <= completed_entry)
                continue;

            completed_entry = entry;
            if (!fast_forward)
            {
                int fade_result = story_semantic_animate_entry(entry,
                    fade_in, entry != final_entry);

                if (fade_result == 2)
                {
                    fast_forward = true;
                    sdl_tale_screen_set_prompt("", false, false);
                }
            }
        }

        if (sdl_tale_screen_is_last_page())
            break;
        if (!sdl_tale_screen_advance_page())
            break;
    }

    sdl_tale_screen_set_active_entry(completed_entry, 255);
    story_semantic_prompt_text(true, prompt, sizeof(prompt));
    sdl_tale_screen_set_prompt(prompt, true, true);
    Term_fresh();
    (void)inkey();
    Term_flush();

    if (completed_entry >= 0 && fade_in)
        (void)story_semantic_alpha_transition(completed_entry, 255, 0);
    else
        sdl_tale_screen_set_active_entry(-1, 0);

    sdl_tale_screen_hide();
    screen_pop_touch_pane_hidden();
    screen_pop_supporting_panes_hidden();
    (void)Term_set_cursor(saved_cursor_state);
    hide_cursor = saved_hide_cursor;
    log_debug("Story display completed (SDL semantic pixel layout)");
}

void print_story(int last_parts, bool fade_in)
{
    int sils = metar.silmarils;
    byte rt = metar.type;
    int total = 0;
    int max_st = z_info->st_max;
    static int sel_idx[1024];
    int start;

    if (max_st > (int)N_ELEMENTS(sel_idx))
        max_st = (int)N_ELEMENTS(sel_idx);

    log_debug("Building SDL story list: sils=%d, rt=%d, max_st=%d",
        sils, rt, max_st);

    for (int i = 0; i < max_st; i++)
    {
        story_type* st = &st_info[i];

        if (!st->name && !st->text)
            continue;
        if (st->st_type != 0)
            continue;
        if (!(st->runtypes == 0
            || (rt < 32 && (st->runtypes & (1UL << rt)))))
        {
            continue;
        }
        if (st->order <= (byte)sils)
        {
            sel_idx[total++] = i;
            log_trace("Added story %d (order=%d) to selection", i,
                st->order);
        }
    }

    if (total == 0)
    {
        log_debug("No stories match criteria: sils=%d, rt=%d", sils, rt);
        return;
    }

    for (int i = 1; i < total; i++)
    {
        int key = sel_idx[i];
        byte key_ord = st_info[key].order;
        int j = i - 1;

        while (j >= 0 && st_info[sel_idx[j]].order > key_ord)
        {
            sel_idx[j + 1] = sel_idx[j];
            j--;
        }
        sel_idx[j + 1] = key;
    }

    start = (last_parts > 0 && last_parts < total)
        ? total - last_parts : 0;
    log_debug("SDL story range: start=%d, total=%d, fade=%s", start,
        total, fade_in ? "true" : "false");
    print_story_sdl(sel_idx, start, total, fade_in);
}
