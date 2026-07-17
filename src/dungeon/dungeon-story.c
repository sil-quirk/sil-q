/* File: dungeon/dungeon-story.c */

#include "angband.h"
#include "dungeon-internal.h"

enum
{
    STORY_INTRO_TYPE_DELAY_MS = 23,
    STORY_INTRO_FRAME_DELAY_MS = 16,
    STORY_INTRO_PASSAGE_HOLD_MS = 720
};

enum story_intro_input
{
    STORY_INTRO_INPUT_NONE = 0,
    STORY_INTRO_INPUT_REVEAL,
    STORY_INTRO_INPUT_SKIP
};

enum story_intro_prompt
{
    STORY_INTRO_PROMPT_WRITING = 0,
    STORY_INTRO_PROMPT_TURN_PAGE,
    STORY_INTRO_PROMPT_FINISH
};

static bool story_intro_back_key(int ch)
{
    return steamdeck_controls_active() && ch == steamdeck_back_key()
        && ch != steamdeck_confirm_key();
}

static void story_intro_prompt_label(int binding, const char* fallback,
    char* buf, size_t buflen)
{
    morgoth_prompt_controller_label(binding, fallback, buf, buflen);
}

static int story_intro_poll_input(void)
{
    char ch;

    if (Term_inkey(&ch, false, false) != 0)
        return STORY_INTRO_INPUT_NONE;
    (void)Term_inkey(&ch, false, true);
    if (ch == ESCAPE || ch == 'S' || story_intro_back_key(ch))
        return STORY_INTRO_INPUT_SKIP;
    return STORY_INTRO_INPUT_REVEAL;
}

static int story_intro_wait_delay(int delay_ms)
{
    int elapsed = 0;

    while (elapsed < delay_ms)
    {
        int input = story_intro_poll_input();
        int frame_delay;

        if (input != STORY_INTRO_INPUT_NONE)
            return input;
        frame_delay = MIN(STORY_INTRO_FRAME_DELAY_MS, delay_ms - elapsed);
        Term_xtra(TERM_XTRA_DELAY, frame_delay);
        elapsed += frame_delay;
    }
    return STORY_INTRO_INPUT_NONE;
}

static int story_intro_character_delay(int ch, int visible_characters)
{
    switch (ch)
    {
    case '.':
    case '!':
    case '?':
        return 150;
    case ',':
    case ';':
    case ':':
        return 72;
    case '-':
    case 0x2014: /* em dash */
        return 92;
    case ' ':
        return 13;
    default:
        /* A tiny, regular hitch keeps the cadence mechanical without making
         * it feel like a uniform progress bar. */
        return STORY_INTRO_TYPE_DELAY_MS
            + ((visible_characters % 7 == 0) ? 5 : 0);
    }
}

static int story_intro_write_passage(int entry)
{
    int total_characters = sdl_tale_screen_entry_character_count(entry);

    sdl_tale_screen_set_typewriter_entry(entry, 0, true);
    Term_fresh();
    for (int visible = 1; visible <= total_characters; visible++)
    {
        int input = story_intro_poll_input();
        int ch;

        if (input == STORY_INTRO_INPUT_SKIP)
            return input;
        if (input == STORY_INTRO_INPUT_REVEAL)
        {
            sdl_tale_screen_set_typewriter_entry(entry, total_characters,
                false);
            Term_fresh();
            return input;
        }

        sdl_tale_screen_set_typewriter_entry(entry, visible, true);
        Term_fresh();
        ch = sdl_tale_screen_entry_character_at(entry, visible - 1);
        input = story_intro_wait_delay(story_intro_character_delay(ch,
            visible));
        if (input == STORY_INTRO_INPUT_SKIP)
            return input;
        if (input == STORY_INTRO_INPUT_REVEAL)
        {
            sdl_tale_screen_set_typewriter_entry(entry, total_characters,
                false);
            Term_fresh();
            return input;
        }
    }

    sdl_tale_screen_set_typewriter_entry(entry, total_characters, false);
    Term_fresh();
    return STORY_INTRO_INPUT_NONE;
}

static void story_intro_prompt_text(enum story_intro_prompt prompt,
    char* buf, size_t buflen)
{
    cptr action = prompt == STORY_INTRO_PROMPT_WRITING
        ? "finish passage" : "turn the page";

    if (!buf || buflen == 0)
        return;
    if (sdl_touch_only_device_active())
    {
        if (prompt == STORY_INTRO_PROMPT_FINISH)
            SDL_strlcpy(buf, "[Tap to close the chronicle]", buflen);
        else
            strnfmt(buf, buflen, "[Tap to %s]  *  [Skip intro]", action);
        return;
    }
    if (steamdeck_controls_active())
    {
        char confirm_label[16];
        char back_label[16];

        story_intro_prompt_label(steamdeck_confirm_key(), "A",
            confirm_label, sizeof(confirm_label));
        if (prompt == STORY_INTRO_PROMPT_FINISH)
        {
            strnfmt(buf, buflen, "[%s] close the chronicle",
                confirm_label);
        }
        else
        {
            story_intro_prompt_label(steamdeck_back_key(), "B", back_label,
                sizeof(back_label));
            strnfmt(buf, buflen, "[%s] %s  *  [%s] skip intro",
                confirm_label, action, back_label);
        }
        return;
    }

    if (prompt == STORY_INTRO_PROMPT_FINISH)
        SDL_strlcpy(buf, "[Press any key to close the chronicle]", buflen);
    else
        strnfmt(buf, buflen, "[Enter] %s  *  [Esc] skip intro", action);
}

static int story_intro_wait_for_key(void)
{
    char ch = inkey();

    if (ch == ESCAPE || ch == 'S' || story_intro_back_key(ch))
        return STORY_INTRO_INPUT_SKIP;
    return STORY_INTRO_INPUT_REVEAL;
}

/**
 * Introductory narrative display.  The engine supplies semantic passages;
 * SDL owns pixel wrapping, manuscript layout, pagination, and glyph reveal.
 */
void print_story_intro(void)
{
    static cptr intro_texts[] = {
        "You awaken in darkness. No name. No memory. Only a quiet ache of "
        "courage deep inside you, like embers buried beneath ash.",

        "Far below, Morgoth waits upon his throne\xE2\x80\x94" "iron-dark and "
        "crowned in flame. Upon his brow shine three Silmarils, stolen stars. "
        "He senses your stirring. He knows you will come.",

        "Far above, beyond the shadows of Angband, the Valar watch silently. "
        "They offer no guidance, yet their presence fills you with "
        "strength\xE2\x80\x94" "and dread.",

        "You will return many times, each death and rebirth etched into the "
        "endless stone halls of Mandos. Each fall will draw your spirit "
        "deeper into shadow, closer to a doom from which you cannot escape.",

        "Yet each victory\xE2\x80\x94" "each Silmaril wrested from Morgoth's "
        "crown\xE2\x80\x94" "will brighten the Valar's hope, even as your soul "
        "grows thinner, your strength fading with every triumph.",

        "You envy the Edain, whose Gift from Il\xC3\xBA" "vatar frees them from "
        "the bonds of Mandos and the world. Yet you do not know if such "
        "release can ever be yours. You do not know who\xE2\x80\x94" "or even "
        "what\xE2\x80\x94" "you truly are.",

        "For each time you awaken, you will carry the names of heroes beloved "
        "and feared\xE2\x80\x94" "bright spirits, fiery hearts, proud kings and "
        "exiles, wanderers beneath sun and stars, whose courage you borrow, "
        "but whose fates are not your own.",

        "This is the trial set by the Valar: to walk the narrow way between "
        "shadow and light, to bear the borrowed glory of the great, and to "
        "win back at last the name that was taken from you.",

        "Now the path before you opens, and your trial begins."
    };
    bool saved_cursor_state = false;
    bool saved_hide_cursor;
    char prompt[128];
    int total = (int)N_ELEMENTS(intro_texts);
    bool panes_hidden = false;

    sdl_music_play_main_full();
    if (!sdl_tale_screen_begin("THE NAMELESS CHRONICLE"))
    {
        log_error("Unable to open the SDL story-intro manuscript");
        return;
    }
    sdl_tale_screen_set_manuscript(true);
    for (int i = 0; i < total; i++)
        sdl_tale_screen_add_entry("", intro_texts[i]);

    screen_push_supporting_panes_hidden();
    screen_push_touch_pane_hidden();
    panes_hidden = true;
    Term_clear();
    (void)Term_get_cursor(&saved_cursor_state);
    saved_hide_cursor = hide_cursor;
    hide_cursor = true;
    (void)Term_set_cursor(false);
    sdl_tale_screen_set_active_entry(-1, 0);

    for (;;)
    {
        int page_entries = sdl_tale_screen_current_page_entry_count();

        for (int position = 0; position < page_entries; position++)
        {
            int entry = sdl_tale_screen_current_page_entry_at(position);
            int input;

            if (entry < 0)
                continue;
            story_intro_prompt_text(STORY_INTRO_PROMPT_WRITING, prompt,
                sizeof(prompt));
            sdl_tale_screen_set_prompt(prompt, true, false);
            input = story_intro_write_passage(entry);
            if (input == STORY_INTRO_INPUT_SKIP)
                goto cleanup_intro;

            input = story_intro_wait_delay(STORY_INTRO_PASSAGE_HOLD_MS);
            if (input == STORY_INTRO_INPUT_SKIP)
                goto cleanup_intro;
        }

        if (sdl_tale_screen_is_last_page())
            break;

        story_intro_prompt_text(STORY_INTRO_PROMPT_TURN_PAGE, prompt,
            sizeof(prompt));
        sdl_tale_screen_set_prompt(prompt, true, false);
        Term_fresh();
        if (story_intro_wait_for_key() == STORY_INTRO_INPUT_SKIP)
            goto cleanup_intro;
        if (!sdl_tale_screen_advance_page())
            break;
        sdl_tale_screen_set_active_entry(-1, 0);
    }

    story_intro_prompt_text(STORY_INTRO_PROMPT_FINISH, prompt,
        sizeof(prompt));
    sdl_tale_screen_set_prompt(prompt, true, true);
    Term_fresh();
    (void)story_intro_wait_for_key();

cleanup_intro:
    Term_flush();
    sdl_tale_screen_hide();
    Term_clear();
    if (panes_hidden)
    {
        screen_pop_touch_pane_hidden();
        screen_pop_supporting_panes_hidden();
    }
    (void)Term_set_cursor(saved_cursor_state);
    hide_cursor = saved_hide_cursor;
    log_debug("Story intro completed (SDL semantic manuscript layout)");
}
