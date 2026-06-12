#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "sdl-config.h"

const char entry_poetry[][100] = { { "Into the vast and echoing gloom," },
    { "more dread than many-tunnelled tomb" },
    //	{ "in labyrinthine pyramid" },
    //	{ "where everlasting death is hid," },
    { "  down awful corridors that wind" },
    { "    down to a menace dark enshrined;" },
    { "      down to the mountain's roots profound," },
    { "devoured, tormented, bored and ground" },
    { "by seething vermin spawned of stone;" },
    { "  down to the depths they went alone..." },

    { "" } };

const char tutorial_leave_text[][100] = {
    { "You have finished the first half of the tutorial and are ready" },
    { "to create a new character." }, { " " },
    { "Don't let the choices overwhelm you the first time." },
    { "Just start with the default Race and Character, then invest most" },
    { "of your starting experience in Melee and Evasion." },
    { "Once the game begins, finding some weapons and armour should" },
    { "be your top priority." }, { " " },
    { "Remember that a key feature of Sil (and all Roguelike games)" },
    { "is that you cannot use savepoints: if you die, that's it." },
    { "It is thus a challenging game where you need to really *think*." },
    { "You will die many times. When you do: reflect on what to learn" },
    { "from that death, see if you set a high score, then think about" },
    { "all the things you want to do differently with the next character..." },

    { "" }
};

const char tutorial_win_text[][100] = {
    { "Congratulations. You have survived a fire-drake (usually found" },
    { "at 900 ft!), and have finished the tutorial in fine form." },
    { "You are more than ready to create a new character." }, { " " },
    { "Don't let the choices overwhelm you the first time." },
    { "Just start with the default Race and Character, then invest most" },
    { "of your starting experience in Melee and Evasion." },
    { "Once the game begins, finding some weapons and armour should" },
    { "be your top priority." }, { " " },
    { "Remember that a key feature of Sil (and all Roguelike games)" },
    { "is that you cannot use savepoints: if you die, that's it." },
    { "It is thus a challenging game where you need to really *think*." },
    { "You will die many times. When you do: reflect on what to learn" },
    { "from that death, see if you set a high score, then think about" },
    { "all the things you want to do differently with the next character..." },

    { "" }
};

const char tutorial_early_death_text[][100] = { { "You have been slain." },
    { " " },
    { "A key feature of Sil (and all Roguelike games) is that you cannot" },
    { "use savepoints: if you die, that's it!" },
    { "It is thus a challenging game where you need to really *think*." },
    { " " },
    { "However, it is a bit frustrating to die before the end of the" },
    { "tutorial, so we evidently made it a bit too deadly." }, { " " },
    { "Just restart the tutorial and you should be back to where you" },
    { "were in a couple of minutes. Remember that if combat is not going" },
    { "your way, you can try to escape and heal, then either come back" },
    { "and again to defeat your adversary, or simply ignore it." },

    { "" } };

const char tutorial_late_death_text[][100] = {
    { "Congratulations: you have finished the tutorial." }, { " " },
    { "You have also just been through a rite of passage: dying." },
    { "Remember that a key feature of Sil (and all Roguelike games)" },
    { "is that you cannot use savepoints: if you die, that's it." },
    { "It is thus a challenging game where you need to really *think*." },
    { "You will die many times. When you do: reflect on what to learn" },
    { "from that death, see if you set a high score, then think about" },
    { "all the things you want to do differently with the next character..." },
    { " " },
    { "You are now more than ready to create a character and start playing." },
    { " " }, { "Don't let the choices overwhelm you the first time." },
    { "Just start with the default Race and Character, then invest most" },
    { "of your starting experience in Melee and Evasion." },
    { "Once the game begins, finding some weapons and armour should" },
    { "be your top priority." },

    { "" }
};

const char throne_poetry[][100] = { { "Loud rose a din of laughter hoarse," },
    { "  self-loathing yet without remorse;" },
    { "    loud came a singing harsh and fierce" },
    { "      like swords of terror souls to pierce." },
    { "Red was the glare through open doors" },
    { "  of firelight mirrored on brazen floors," },
    { "    and up the arches towering clomb" },
    { "      to glooms unguessed, to vaulted dome" },
    { "        swathed in wavering smokes and steams" },
    { "          stabbed with flickering lightning-gleams." },

    { "" } };

/*
const char throne_poetry2[][100] =
{
        { "To Morgoth's hall, where dreadful feast" },
        { "he held, and drank the blood of beast" },
        { "and lives of Men, she stumbling came:" },
        { "her eyes were dazed with smoke and flame." },
        { "The pillars, reared like monstrous shores" },
        { "to bear earth's overwhelming floors," },
        { "were devil-carven, shaped with skill" },
        { "such as unholy dreams doth fill:" },
        { "they towered like trees into the air," },
        { "whose trunks are rooted in despair," },
        { "whose shade is death, whose fruit is bane," },
        { "whose boughs like serpents writhe in pain." },
        { "Beneath them ranged with spear and sword" },
        { "stood Morgoth's sable-armoured horde:" },
        { "the fire on blade and boss of shield" },
        { "was red as blood on stricken field." },
        { "Beneath a monstrous column loomed" },
        { "the throne of Morgoth, and the doomed" },
        { "and dying gasped upon the floor:" },
        { "his hideous footstool, rape of war." },

        { "" }
};
*/

const char ultimate_bug_text[][100]
    = { { "Against all hope, the Black Foe of the World is cast down," },
          { "  his form broken, his fire quenched in the wreck of his pride." },
          { "    Though malice so great may not wholly perish," },
          { "      for this age of Arda his shadow is lifted." },
          { "But Angband groans above its fallen master," },
          { "  and you are buried still beneath the roots of the North." },
          { "    The songs must be sung under open sky --" },
          { "      run now, and bear the light out of the dark!" },

          { "" } };

static int pause_with_text_max_line_width(const char lines[][100])
{
    int max_width = 0;

    if (!lines)
        return 0;

    for (int i = 0; lines[i][0]; ++i)
    {
        int width = (int)strlen(lines[i]);
        if (width > max_width)
            max_width = width;
    }

    return max_width;
}

static int pause_with_text_fit_column(int col, int term_wid, int text_width)
{
    int max_col;

    if (col < 0)
        col = 0;
    if (term_wid <= 2 || text_width <= 0)
        return col;

    max_col = term_wid - text_width - 3;
    if (max_col < 0)
        max_col = 0;

    if (col > max_col)
        col = max_col;

    return col;
}

static cptr pause_with_text_fit_segment_text(cptr text, int max_cols)
{
    if (!text)
        return "";

    while ((*text == ' ') && (max_cols > 0)
        && ((int)strlen(text) > max_cols))
    {
        text++;
    }

    return text;
}

static int pause_with_text_print_wrapped_segment(int row, int col, byte attr,
                                                 cptr text, int delay_msec)
{
    int term_wid = 80;
    int term_hgt = 24;
    int max_cols;
    int wrap_col;
    int rows_used = 1;

    if (!text)
        text = "";

    Term_get_size(&term_wid, &term_hgt);
    if (term_wid < 1)
        term_wid = 80;
    if (term_hgt < 1)
        term_hgt = 24;

    if (row < 0 || row >= term_hgt)
        return 0;

    if (col < 0)
        col = 0;
    if (col >= term_wid)
        col = term_wid - 1;

    max_cols = term_wid - col - 2;
    if (max_cols < 1)
        max_cols = 1;

    text = pause_with_text_fit_segment_text(text, MAX(1, max_cols - 1));
    wrap_col = col + max_cols;

    if (*text)
    {
        if (sdl_is_story_font_enabled())
            rows_used = count_wrapped_lines_story(text, wrap_col, col);
        else
            rows_used = count_wrapped_lines(text, wrap_col, col);

        if (rows_used < 1)
            rows_used = 1;
    }

    story_print_text(row, col, max_cols, attr, text);
    Term_fresh();

    if (delay_msec > 0)
        Term_xtra(TERM_XTRA_DELAY, delay_msec);

    return rows_used;
}

static int pause_with_text_count_wrapped_segment(int col, cptr text)
{
    int term_wid = 80;
    int term_hgt = 24;
    int max_cols;
    int wrap_col;
    int rows_used = 1;

    if (!text)
        text = "";

    Term_get_size(&term_wid, &term_hgt);
    if (term_wid < 1)
        term_wid = 80;
    (void)term_hgt;

    if (col < 0)
        col = 0;
    if (col >= term_wid)
        col = term_wid - 1;

    max_cols = term_wid - col - 2;
    if (max_cols < 1)
        max_cols = 1;

    text = pause_with_text_fit_segment_text(text, MAX(1, max_cols - 1));
    wrap_col = col + max_cols;

    if (*text)
    {
        if (sdl_is_story_font_enabled())
            rows_used = count_wrapped_lines_story(text, wrap_col, col);
        else
            rows_used = count_wrapped_lines(text, wrap_col, col);

        if (rows_used < 1)
            rows_used = 1;
    }

    return rows_used;
}

/* pause_with_text: prints name+alt, explicit blank line, then wrapped start splits */
void pause_with_text(const char desc[][100], int row, int col,
                     const char extra[][100], byte extra_attr)
{
    int i_main = 0, msec = 50;
    int banner_lines = 0;
    int main_rows = 0;
    int term_wid = 80;
    int term_hgt = 24;
    int banner_col;
    int tail_col;
    bool show_banner_gap = true;
    bool show_stanza_gap = true;

    /* 0. save & clear screen */
    screen_save();
    Term_clear();
    Term_get_size(&term_wid, &term_hgt);
    if (term_wid < 1)
        term_wid = 80;
    if (term_hgt < 1)
        term_hgt = 24;

    col = pause_with_text_fit_column(col, term_wid,
        pause_with_text_max_line_width(desc));

    sdl_story_font_enable();
    log_debug("Banner: story font enabled");

    banner_col = col - 5;
    if (banner_col < 0)
        banner_col = 0;
    tail_col = banner_col + 4;
    if (tail_col < 0)
        tail_col = 0;

    if (extra)
    {
        int n_extra = 0;

        banner_lines += pause_with_text_count_wrapped_segment(banner_col, extra[0]);
        while (extra[n_extra][0])
            n_extra++;

        if (n_extra > 1)
        {
            for (int i = 1; i < n_extra; ++i)
            {
                int segment_col = (i == n_extra - 1) ? tail_col : banner_col;
                banner_lines += pause_with_text_count_wrapped_segment(segment_col, extra[i]);
            }
        }
        else
        {
            show_banner_gap = false;
            show_stanza_gap = false;
        }
    }
    else
    {
        show_banner_gap = false;
        show_stanza_gap = false;
    }

    if (show_banner_gap)
        banner_lines++;
    if (show_stanza_gap)
        banner_lines++;

    while (desc && desc[i_main][0])
    {
        main_rows += pause_with_text_count_wrapped_segment(col, desc[i_main]);
        ++i_main;
    }

    if ((banner_lines + main_rows) > term_hgt && show_stanza_gap)
    {
        banner_lines--;
        show_stanza_gap = false;
    }

    if ((banner_lines + main_rows) > term_hgt && show_banner_gap)
    {
        banner_lines--;
        show_banner_gap = false;
    }

    {
        int total_rows = banner_lines + main_rows;

        if (extra)
        {
            row = (term_hgt - total_rows) / 2;
        }
        else
        {
            int slack = term_hgt - total_rows;

            if (row < 0)
                row = 0;
            if (slack < 0)
                slack = 0;
            if (row > slack)
                row = slack;
        }

        if (row < 0)
            row = 0;
    }

    banner_lines = 0;
    main_rows = 0;

    /* 1. optional banner */
    if (extra) {
        int n_extra = 0;

        while (extra[n_extra][0]) n_extra++;

        /* Line 1: name+alt */
        banner_lines += pause_with_text_print_wrapped_segment(
            row + banner_lines, banner_col, extra_attr, extra[0], msec);

        /* Line 2: blank line */
        if (show_banner_gap)
        {
            banner_lines += pause_with_text_print_wrapped_segment(
                row + banner_lines, banner_col, extra_attr, "", msec);
        }

        /* Lines 3+: start splits, last one shifted further right */
        for (int i = 1; i < n_extra; ++i) {
            int shift = (i == n_extra - 1) ? tail_col : banner_col;
            banner_lines += pause_with_text_print_wrapped_segment(
                row + banner_lines, shift, extra_attr, extra[i], msec);
        }

        /* separator before stanza */
        if (show_stanza_gap)
            banner_lines++;
    }

    /* 2. main stanza */
    i_main = 0;
    while (desc && desc[i_main][0]) {
        main_rows += pause_with_text_print_wrapped_segment(
            row + banner_lines + main_rows, col, TERM_WHITE, desc[i_main], msec);
        ++i_main;
    }

    log_debug("Banner: story font disabled");
    sdl_story_font_disable();

    ui_menu_click_begin();
    for (int click_row = 0; click_row < term_hgt; click_row++)
        ui_menu_click_add_full_row(1, click_row);

    /* 3. wait for key */
    hide_cursor = true;
    (void)inkey();
    hide_cursor = false;
    ui_menu_click_clear();

    /* 4. wipe the area used */
    int total = banner_lines + main_rows;
    int max_row = MIN(row + total, term_hgt);
    for (int j = row; j < max_row; ++j) {
        Term_erase(0, j, 255);
    }

    screen_load();
}
