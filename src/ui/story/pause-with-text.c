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

static bool pause_with_text_is_tutorial(const char desc[][100])
{
    return desc == tutorial_leave_text || desc == tutorial_win_text
        || desc == tutorial_early_death_text
        || desc == tutorial_late_death_text;
}

/* The legacy tutorial conclusion/death pages do not support inline spans.
 * Their source is already split into short ideas, so colour only the lines
 * carrying success, danger, or concrete next-step advice. */
static byte pause_with_text_tutorial_attr(cptr line)
{
    if (!line)
        return TERM_WHITE;

    if (strstr(line, "Congratulations") || strstr(line, "finished")
        || strstr(line, "survived") || strstr(line, "more than ready"))
    {
        return TERM_L_GREEN;
    }

    if (strstr(line, "slain") || strstr(line, "cannot use savepoints")
        || strstr(line, "if you die") || strstr(line, "You will die")
        || strstr(line, "from that death") || strstr(line, "too deadly"))
    {
        return TERM_L_RED;
    }

    if (strstr(line, "default Race and Character")
        || strstr(line, "starting experience")
        || strstr(line, "Melee and Evasion")
        || strstr(line, "weapons and armour")
        || strstr(line, "restart the tutorial")
        || strstr(line, "escape and heal")
        || strstr(line, "high score")
        || strstr(line, "top priority"))
    {
        return TERM_L_BLUE;
    }

    if (strstr(line, "*think*") || strstr(line, "reflect on what to learn"))
        return TERM_YELLOW;

    return TERM_WHITE;
}

static void pause_with_text_semantic_add(cptr text, byte attr,
    int base_indent, int* line_count)
{
    int leading = 0;

    if (!text)
        text = "";
    while (text[leading] == ' ')
        leading++;

    sdl_pause_text_screen_add_line(text + leading, attr,
        MAX(0, base_indent + leading));
    if (line_count)
        (*line_count)++;
}

/* Preserve source lines, colours, and relative indentation as semantic data;
 * the SDL canvas owns font sizing, wrapping, and rendering. */
static void pause_with_text_sdl(const char desc[][100], int row, int col,
    const char extra[][100], byte extra_attr)
{
    int line_count = 0;
    int origin_col = col;
    int banner_col = MAX(0, col - 5);
    int tail_col = banner_col + 4;
    int n_extra = 0;
    bool tutorial_text = pause_with_text_is_tutorial(desc);

    (void)row;

    if (!sdl_pause_text_screen_begin())
    {
        log_error("Unable to open the SDL pause-text screen");
        return;
    }

    screen_save();
    Term_clear();

    if (extra)
    {
        while (extra[n_extra][0])
            n_extra++;
        origin_col = banner_col;

        if (n_extra > 0)
        {
            pause_with_text_semantic_add(extra[0], extra_attr, 0,
                &line_count);
        }
        if (n_extra > 1)
        {
            pause_with_text_semantic_add("", extra_attr, 0, &line_count);
            for (int i = 1; i < n_extra; i++)
            {
                int line_col = (i == n_extra - 1) ? tail_col : banner_col;

                pause_with_text_semantic_add(extra[i], extra_attr,
                    line_col - origin_col, &line_count);
            }
            pause_with_text_semantic_add("", TERM_WHITE, 0, &line_count);
        }
    }

    for (int i = 0; desc && desc[i][0]; i++)
    {
        byte attr = tutorial_text
            ? pause_with_text_tutorial_attr(desc[i]) : TERM_WHITE;

        pause_with_text_semantic_add(desc[i], attr, col - origin_col,
            &line_count);
    }

    sdl_pause_text_screen_set_visible_lines(0);
    Term_fresh();
    for (int i = 0; i < line_count; i++)
    {
        sdl_pause_text_screen_set_visible_lines(i + 1);
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 50);
    }

    hide_cursor = true;
    (void)inkey();
    hide_cursor = false;

    sdl_pause_text_screen_hide();
    screen_load();
}

void pause_with_text(const char desc[][100], int row, int col,
    const char extra[][100], byte extra_attr)
{
    pause_with_text_sdl(desc, row, col, extra, extra_attr);
}
