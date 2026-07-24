#include "angband.h"
#include "sdl/main-sdl-private.h"

enum {
    /* Welcome body text uses the secondary story font (storyfont 2);
     * captions fall back to the primary one via sdl_welcome_slot_for_role(). */
    SDL_WELCOME_STORY_FONT_SLOT = 1,
    SDL_WELCOME_MAX_LINES = 32
};

enum {
    SDL_CHAR_SHEET_INFO_CHOICE_BASE = 9000,
    SDL_CHAR_SHEET_BIRTH_STAT_INFO_BASE = 9200,
    SDL_CHAR_SHEET_BIRTH_SKILL_INFO_BASE = 9300,
    SDL_CHAR_SHEET_BIRTH_TRAIT_INFO_BASE = 9400,
    SDL_CHAR_SHEET_BIRTH_VITAL_INFO_BASE = 9500
};

static bool g_sdl_select_choice_page_only = false;
static bool g_sdl_select_dynamic_description = false;
static int g_sdl_select_menu_rows_per_column = 0;
static bool g_sdl_narrative_portrait_rendering = false;
static int g_sdl_narrative_page_scroll[SDL_BOOK_MAX_PAGES];
static SDL_FRect g_sdl_char_sheet_hover_tooltip_rect;
static int g_sdl_char_sheet_hover_tooltip_choice =
    SDL_CHAR_SHEET_NO_HOVER;

static bool sdl_narrative_portrait_adjust_canvas(SDL_Rect* canvas);

enum {
    SDL_POETRY_MAX_CHOICES = 4,
    SDL_POETRY_MAX_BLOCKS = 8,
    SDL_POETRY_CHOICE_LABEL_LEN = 160,
    SDL_POETRY_CHOICE_BODY_LEN = 1024,
    SDL_POETRY_BLOCK_LEN = 1024
};

typedef struct sdl_poetry_choice_state {
    bool visible;
    int choice;
    byte label_attr;
    byte body_attr;
    byte alpha;
    char label[SDL_POETRY_CHOICE_LABEL_LEN];
    char body[SDL_POETRY_CHOICE_BODY_LEN];
    SDL_FRect hit_rect;
} sdl_poetry_choice_state;

typedef struct sdl_poetry_block_state {
    bool visible;
    byte attr;
    byte alpha;
    char text[SDL_POETRY_BLOCK_LEN];
} sdl_poetry_block_state;

typedef struct sdl_poetry_screen_state {
    bool active;
    bool title_visible;
    bool body_visible;
    bool transition_visible;
    bool prompt_visible;
    byte title_attr;
    byte body_attr;
    byte transition_attr;
    byte title_alpha;
    byte body_alpha;
    byte transition_alpha;
    byte prompt_alpha;
    char title[160];
    char body[2048];
    char transition[512];
    char prompt[160];
    int block_count;
    int choice_count;
    int highlight;
    SDL_FRect prompt_rect;
    sdl_poetry_block_state blocks[SDL_POETRY_MAX_BLOCKS];
    sdl_poetry_choice_state choices[SDL_POETRY_MAX_CHOICES];
} sdl_poetry_screen_state;

static sdl_poetry_screen_state g_sdl_poetry_screen;

typedef struct sdl_poetry_sequence_layout_state {
    bool active;
    float title_y;
} sdl_poetry_sequence_layout_state;

static sdl_poetry_sequence_layout_state g_sdl_poetry_sequence_layout;

enum {
    SDL_PAUSE_TEXT_MAX_LINES = 64,
    SDL_PAUSE_TEXT_LINE_LEN = 256
};

typedef struct sdl_pause_text_line {
    byte attr;
    int indent;
    char text[SDL_PAUSE_TEXT_LINE_LEN];
} sdl_pause_text_line;

typedef struct sdl_pause_text_screen_state {
    bool active;
    int line_count;
    int visible_lines;
    sdl_pause_text_line lines[SDL_PAUSE_TEXT_MAX_LINES];
} sdl_pause_text_screen_state;

static sdl_pause_text_screen_state g_sdl_pause_text_screen;

enum {
    SDL_TALE_MAX_ENTRIES = 128,
    SDL_TALE_HEADING_LEN = 192,
    SDL_TALE_BODY_LEN = 2048,
    SDL_TALE_LAYOUT_MAX_LINES = 1024,
    SDL_TALE_LAYOUT_LINE_LEN = 768,
    SDL_TALE_MAX_PAGES = 128
};

typedef struct sdl_tale_entry {
    char heading[SDL_TALE_HEADING_LEN];
    char body[SDL_TALE_BODY_LEN];
} sdl_tale_entry;

typedef struct sdl_tale_layout_line {
    bool heading;
    int entry;
    char text[SDL_TALE_LAYOUT_LINE_LEN];
} sdl_tale_layout_line;

typedef struct sdl_tale_screen_state {
    bool active;
    bool manuscript;
    bool prompt_visible;
    bool prompt_final;
    bool prompt_hovered;
    bool typewriter_cursor_visible;
    int entry_count;
    int page;
    int active_entry;
    int active_visible_characters;
    byte active_alpha;
    int layout_canvas_w;
    int layout_canvas_h;
    int layout_line_count;
    int page_count;
    char title[192];
    char prompt[192];
    SDL_FRect prompt_next_rect;
    SDL_FRect prompt_skip_rect;
    sdl_tale_entry entries[SDL_TALE_MAX_ENTRIES];
    sdl_tale_layout_line layout_lines[SDL_TALE_LAYOUT_MAX_LINES];
    int page_starts[SDL_TALE_MAX_PAGES + 1];
} sdl_tale_screen_state;

static sdl_tale_screen_state g_sdl_tale_screen;

const sdl_welcome_intro_line g_sdl_welcome_intro_flame[] = {
    { TERM_L_BLUE, SDL_WELCOME_LINE_QUOTE,
        "\"In the beginning Eru, the One," },
    { TERM_L_BLUE, SDL_WELCOME_LINE_QUOTE,
        "made the Ainur of his thought;" },
    { TERM_L_BLUE, SDL_WELCOME_LINE_QUOTE,
        "and they sang, and he was glad.\"" },
    { TERM_SLATE, SDL_WELCOME_LINE_ATTRIBUTION, "— Ainulindalë" },
    { TERM_WHITE, SDL_WELCOME_LINE_TITLE, "S I L - M O R Ë" },
    { TERM_L_BLUE, SDL_WELCOME_LINE_SUBTITLE,
        "~ Shining  Darkness ~" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "In the deeps of Angband, beyond" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "gates of iron and pits of flame," },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "Morgoth hoards the Silmarils —" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "three jewels of living light." },
    { TERM_YELLOW, SDL_WELCOME_LINE_ACTION,
        "Take up blade and burden. Descend." },
    { TERM_YELLOW, SDL_WELCOME_LINE_ACTION,
        "Oaths, quests, blessings of the Valar" },
    { TERM_YELLOW, SDL_WELCOME_LINE_ACTION,
        "await in the First Age reborn." },
    { 0, 0, NULL }
};

const sdl_welcome_intro_line g_sdl_welcome_intro_feanor[] = {
    { TERM_L_BLUE, SDL_WELCOME_LINE_QUOTE, "\"Be he foe or friend," },
    { TERM_L_BLUE, SDL_WELCOME_LINE_QUOTE,
        "  be he foul or clean..." },
    { TERM_L_BLUE, SDL_WELCOME_LINE_QUOTE,
        "  he shall defend, shall be held mine.\"" },
    { TERM_SLATE, SDL_WELCOME_LINE_ATTRIBUTION, "— Oath of Fëanor" },
    { TERM_WHITE, SDL_WELCOME_LINE_TITLE, "S I L - M O R Ë" },
    { TERM_L_BLUE, SDL_WELCOME_LINE_SUBTITLE,
        "~ Shining  Darkness ~" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "In the pits beneath the mountains" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "Morgoth broods upon his throne." },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "Three jewels burn upon his crown —" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "stolen light that is not his own." },
    { TERM_YELLOW, SDL_WELCOME_LINE_ACTION,
        "Take up blade and burden. Descend." },
    { TERM_YELLOW, SDL_WELCOME_LINE_ACTION,
        "Oaths, quests, blessings of the Valar" },
    { TERM_YELLOW, SDL_WELCOME_LINE_ACTION,
        "await in the First Age reborn." },
    { 0, 0, NULL }
};

const sdl_welcome_intro_line g_sdl_welcome_intro_twilight[] = {
    { TERM_WHITE, SDL_WELCOME_LINE_TITLE, "S I L - M O R Ë" },
    { TERM_L_BLUE, SDL_WELCOME_LINE_SUBTITLE,
        "~ Shining  Darkness ~" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "Before the Sun and Moon were wrought" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "the Eldar walked by starlight alone." },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "Now shadow stirs beneath the earth" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "where Morgoth sits upon his throne." },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "Three jewels blaze upon his crown —" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "stolen fire none may reclaim..." },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "unless one dares the iron dark" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "and walks through everlasting flame." },
    { TERM_L_BLUE, SDL_WELCOME_LINE_QUOTE,
        "\"...and the light that blazed in them" },
    { TERM_L_BLUE, SDL_WELCOME_LINE_QUOTE,
        "  no power could dim or mar.\"" },
    { TERM_SLATE, SDL_WELCOME_LINE_ATTRIBUTION, "— Of the Silmarils" },
    { 0, 0, NULL }
};

const sdl_welcome_intro_line g_sdl_welcome_intro_luthien[] = {
    { TERM_L_BLUE, SDL_WELCOME_LINE_QUOTE,
        "\"The leaves were long, the grass was green," },
    { TERM_L_BLUE, SDL_WELCOME_LINE_QUOTE,
        "  the hemlock-umbels tall and fair," },
    { TERM_L_BLUE, SDL_WELCOME_LINE_QUOTE,
        "  and in the glade a light was seen" },
    { TERM_L_BLUE, SDL_WELCOME_LINE_QUOTE,
        "  of stars in shadow shimmering.\"" },
    { TERM_SLATE, SDL_WELCOME_LINE_SONG_ATTRIBUTION,
        "— Of Beren and Lúthien" },
    { TERM_WHITE, SDL_WELCOME_LINE_TITLE, "S I L - M O R Ë" },
    { TERM_L_BLUE, SDL_WELCOME_LINE_SUBTITLE,
        "~ Shining  Darkness ~" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "Even in the deepest dark, a song" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "may still undo the mightiest door." },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "Dare the throne-hall of the Enemy" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "and seize what Morgoth stole of old." },
    { TERM_YELLOW, SDL_WELCOME_LINE_ACTION,
        "Oaths, quests, blessings of the Valar" },
    { TERM_YELLOW, SDL_WELCOME_LINE_ACTION,
        "await in the First Age reborn." },
    { 0, 0, NULL }
};

const sdl_welcome_intro_line g_sdl_welcome_intro_hurin[] = {
    { TERM_L_BLUE, SDL_WELCOME_LINE_QUOTE,
        "\"The day shall come again when you" },
    { TERM_L_BLUE, SDL_WELCOME_LINE_QUOTE,
        "  shall see the Sun once more.\"" },
    { TERM_SLATE, SDL_WELCOME_LINE_ATTRIBUTION, "— Words of Húrin" },
    { TERM_WHITE, SDL_WELCOME_LINE_TITLE, "S I L - M O R Ë" },
    { TERM_L_BLUE, SDL_WELCOME_LINE_SUBTITLE,
        "~ Shining  Darkness ~" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "No chain can hold a will unbroken." },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "Though Morgoth's shadow covers all," },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "the free may still defy the dark" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "and wrest a jewel from his crown." },
    { TERM_YELLOW, SDL_WELCOME_LINE_ACTION,
        "Take up blade and burden. Descend." },
    { TERM_YELLOW, SDL_WELCOME_LINE_ACTION,
        "Oaths, quests, blessings of the Valar" },
    { TERM_YELLOW, SDL_WELCOME_LINE_ACTION,
        "await in the First Age reborn." },
    { TERM_L_BLUE, SDL_WELCOME_LINE_QUOTE, "\"Aure entuluva!\"" },
    { 0, 0, NULL }
};

const sdl_welcome_intro_line g_sdl_welcome_intro_starlight[] = {
    { TERM_WHITE, SDL_WELCOME_LINE_TITLE, "S I L - M O R Ë" },
    { TERM_L_BLUE, SDL_WELCOME_LINE_SUBTITLE,
        "~ Shining  Darkness ~" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "By silver waters Elves first woke" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "beneath the stars ere morning broke." },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "No sun had risen, no moon shone —" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "just heaven's light on lake and stone." },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "Then Morgoth's shadow veiled the land" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "and stole the Light with iron hand." },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "Yet still a whisper stirs the deep:" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "what darkness took, the bold may reap." },
    { TERM_L_BLUE, SDL_WELCOME_LINE_QUOTE,
        "\"...the starlight glittered" },
    { TERM_L_BLUE, SDL_WELCOME_LINE_QUOTE,
        "  on the waters of Cuiviénen.\"" },
    { TERM_SLATE, SDL_WELCOME_LINE_ATTRIBUTION,
        "— Of the Coming of the Elves" },
    { 0, 0, NULL }
};

const sdl_welcome_intro_line g_sdl_welcome_intro_noldor[] = {
    { TERM_WHITE, SDL_WELCOME_LINE_TITLE, "S I L - M O R Ë" },
    { TERM_L_BLUE, SDL_WELCOME_LINE_SUBTITLE,
        "~ Shining  Darkness ~" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "In Valinor the Two Trees shone" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "with gold and silver, leaf and bough." },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "Their mingled light is dead and gone —" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "the world lies under shadow now." },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "Across the ice the exiles came," },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "the Noldor burning with their oath." },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "They traded bliss for grief and flame" },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "and lost the blessing of them both." },
    { TERM_L_BLUE, SDL_WELCOME_LINE_QUOTE,
        "\"...and the Noldor wept" },
    { TERM_L_BLUE, SDL_WELCOME_LINE_QUOTE,
        "  for the beauty of Telperion and Laurelin.\"" },
    { TERM_SLATE, SDL_WELCOME_LINE_ATTRIBUTION,
        "— Of the Darkening of Valinor" },
    { 0, 0, NULL }
};

const sdl_welcome_intro_line* sdl_welcome_intro_lines_for_style(
    int intro_style)
{
    switch (intro_style)
    {
    case INTRO_STYLE_FEANOR: return g_sdl_welcome_intro_feanor;
    case INTRO_STYLE_TWILIGHT: return g_sdl_welcome_intro_twilight;
    case INTRO_STYLE_LUTHIEN: return g_sdl_welcome_intro_luthien;
    case INTRO_STYLE_HURIN: return g_sdl_welcome_intro_hurin;
    case INTRO_STYLE_STARLIGHT: return g_sdl_welcome_intro_starlight;
    case INTRO_STYLE_NOLDOLANTE: return g_sdl_welcome_intro_noldor;
    case INTRO_STYLE_FLAME:
    default: return g_sdl_welcome_intro_flame;
    }
}

SDL_Color sdl_welcome_color(byte attr, byte alpha)
{
    SDL_Color color;

    color.r = angband_color_table[attr][1];
    color.g = angband_color_table[attr][2];
    color.b = angband_color_table[attr][3];
    color.a = alpha;
    return color;
}

bool sdl_welcome_screen_available(void)
{
    return g_state.window && g_state.renderer;
}

void sdl_welcome_screen_clear_hits(void)
{
    g_sdl_welcome_screen.continue_rect = (SDL_FRect){ 0 };
    g_sdl_welcome_screen.quit_rect = (SDL_FRect){ 0 };
}

void sdl_welcome_screen_mark_dirty(void)
{
    if (sdl_welcome_screen_available())
        g_state.need_present = true;
}

int sdl_welcome_screen_normalize_intro_style(int intro_style)
{
    if (intro_style < INTRO_STYLE_FLAME || intro_style >= INTRO_STYLE_MAX)
        return INTRO_STYLE_FLAME;

    return intro_style;
}

bool sdl_welcome_screen_active(void)
{
    return g_sdl_welcome_screen.mode != SDL_WELCOME_SCREEN_HIDDEN;
}

bool sdl_welcome_screen_show_intro(int intro_style, bool show_wizard)
{
    if (!sdl_welcome_screen_available())
        return false;

    g_sdl_welcome_screen.mode = SDL_WELCOME_SCREEN_INTRO;
    g_sdl_welcome_screen.intro_style =
        sdl_welcome_screen_normalize_intro_style(intro_style);
    g_sdl_welcome_screen.show_wizard = show_wizard;
    g_sdl_welcome_screen.new_metarun = false;
    g_sdl_welcome_screen.hover_continue = false;
    g_sdl_welcome_screen.hover_quit = false;
    SDL_strlcpy(g_sdl_welcome_screen.status, "Loading...",
        sizeof(g_sdl_welcome_screen.status));
    sdl_welcome_screen_clear_hits();
    sdl_welcome_screen_mark_dirty();
    return true;
}

bool sdl_welcome_screen_show_menu(bool show_wizard, bool new_metarun)
{
    if (!sdl_welcome_screen_available())
        return false;

    g_sdl_welcome_screen.mode = SDL_WELCOME_SCREEN_MENU;
    g_sdl_welcome_screen.show_wizard = show_wizard;
    g_sdl_welcome_screen.new_metarun = new_metarun;
    g_sdl_welcome_screen.status[0] = '\0';
    sdl_welcome_screen_clear_hits();
    sdl_welcome_screen_mark_dirty();
    return true;
}

bool sdl_welcome_screen_set_status(cptr status)
{
    if (!sdl_welcome_screen_available() || !sdl_welcome_screen_active())
        return false;

    SDL_strlcpy(g_sdl_welcome_screen.status, status ? status : "",
        sizeof(g_sdl_welcome_screen.status));
    sdl_welcome_screen_mark_dirty();
    return true;
}

bool sdl_welcome_screen_show_loading(cptr status)
{
    if (!sdl_welcome_screen_available())
        return false;

    g_sdl_welcome_screen.mode = SDL_WELCOME_SCREEN_LOADING;
    SDL_strlcpy(g_sdl_welcome_screen.status,
        (status && status[0]) ? status : "Loading...",
        sizeof(g_sdl_welcome_screen.status));
    g_sdl_welcome_screen.hover_continue = false;
    g_sdl_welcome_screen.hover_quit = false;
    sdl_welcome_screen_clear_hits();
    sdl_welcome_screen_mark_dirty();
    return true;
}

void sdl_welcome_screen_hide(void)
{
    if (!sdl_welcome_screen_active())
        return;

    g_sdl_welcome_screen.mode = SDL_WELCOME_SCREEN_HIDDEN;
    g_sdl_welcome_screen.status[0] = '\0';
    g_sdl_welcome_screen.hover_continue = false;
    g_sdl_welcome_screen.hover_quit = false;
    g_sdl_narrative_portrait_rendering = false;
    sdl_welcome_screen_clear_hits();
    sdl_welcome_screen_mark_dirty();
}

void sdl_poetry_screen_begin(cptr title, cptr body, cptr transition,
    cptr prompt)
{
    if (!sdl_welcome_screen_available())
        return;

    memset(&g_sdl_poetry_screen, 0, sizeof(g_sdl_poetry_screen));
    g_sdl_poetry_screen.active = true;
    g_sdl_poetry_screen.highlight = -1;
    g_sdl_poetry_screen.title_attr = TERM_RED;
    g_sdl_poetry_screen.body_attr = TERM_WHITE;
    g_sdl_poetry_screen.transition_attr = TERM_L_BLUE;
    g_sdl_poetry_screen.prompt_alpha = 255;
    SDL_strlcpy(g_sdl_poetry_screen.title, title ? title : "",
        sizeof(g_sdl_poetry_screen.title));
    SDL_strlcpy(g_sdl_poetry_screen.body, body ? body : "",
        sizeof(g_sdl_poetry_screen.body));
    SDL_strlcpy(g_sdl_poetry_screen.transition,
        transition ? transition : "",
        sizeof(g_sdl_poetry_screen.transition));
    SDL_strlcpy(g_sdl_poetry_screen.prompt,
        (prompt && prompt[0]) ? prompt : "[Press any key to continue]",
        sizeof(g_sdl_poetry_screen.prompt));
    sdl_welcome_screen_mark_dirty();
}

void sdl_poetry_screen_begin_choices(cptr title)
{
    sdl_poetry_screen_begin(title, "", "", "");
    if (!g_sdl_poetry_screen.active)
        return;
    g_sdl_poetry_screen.title_visible = true;
    g_sdl_poetry_screen.title_attr = TERM_YELLOW;
    g_sdl_poetry_screen.prompt[0] = '\0';
}

void sdl_poetry_screen_begin_blocks(cptr title, cptr prompt)
{
    sdl_poetry_screen_begin(title, "", "", prompt);
}

int sdl_poetry_screen_add_block(cptr text, byte attr)
{
    sdl_poetry_block_state* block;
    int index;

    if (!g_sdl_poetry_screen.active
        || g_sdl_poetry_screen.block_count >= SDL_POETRY_MAX_BLOCKS)
    {
        return -1;
    }

    index = g_sdl_poetry_screen.block_count++;
    block = &g_sdl_poetry_screen.blocks[index];
    memset(block, 0, sizeof(*block));
    block->attr = attr;
    SDL_strlcpy(block->text, text ? text : "", sizeof(block->text));
    sdl_welcome_screen_mark_dirty();
    return index;
}

void sdl_poetry_screen_set_block_visible(int block, bool visible)
{
    if (!g_sdl_poetry_screen.active || block < 0
        || block >= g_sdl_poetry_screen.block_count)
    {
        return;
    }

    g_sdl_poetry_screen.blocks[block].visible = visible;
    sdl_welcome_screen_mark_dirty();
}

void sdl_poetry_screen_set_block_alpha(int block, byte alpha)
{
    if (!g_sdl_poetry_screen.active || block < 0
        || block >= g_sdl_poetry_screen.block_count)
    {
        return;
    }

    g_sdl_poetry_screen.blocks[block].alpha = alpha;
    sdl_welcome_screen_mark_dirty();
}

void sdl_poetry_screen_set_block_attr(int block, byte attr)
{
    if (!g_sdl_poetry_screen.active || block < 0
        || block >= g_sdl_poetry_screen.block_count)
    {
        return;
    }

    g_sdl_poetry_screen.blocks[block].attr = attr;
    sdl_welcome_screen_mark_dirty();
}

void sdl_poetry_screen_add_choice(int choice, cptr label, cptr body)
{
    sdl_poetry_choice_state* item;

    if (!g_sdl_poetry_screen.active
        || g_sdl_poetry_screen.choice_count >= SDL_POETRY_MAX_CHOICES)
    {
        return;
    }

    item = &g_sdl_poetry_screen
        .choices[g_sdl_poetry_screen.choice_count++];
    memset(item, 0, sizeof(*item));
    item->choice = choice;
    item->label_attr = TERM_L_RED;
    item->body_attr = TERM_SLATE;
    item->alpha = 0;
    SDL_strlcpy(item->label, label ? label : "", sizeof(item->label));
    SDL_strlcpy(item->body, body ? body : "", sizeof(item->body));
    sdl_welcome_screen_mark_dirty();
}

void sdl_poetry_screen_set_choice_visible(int choice, bool visible,
    byte label_attr, byte body_attr)
{
    for (int i = 0; i < g_sdl_poetry_screen.choice_count; i++)
    {
        sdl_poetry_choice_state* item = &g_sdl_poetry_screen.choices[i];

        if (item->choice != choice)
            continue;
        item->visible = visible;
        item->label_attr = label_attr;
        item->body_attr = body_attr;
        sdl_welcome_screen_mark_dirty();
        return;
    }
}

void sdl_poetry_screen_set_highlight(int choice)
{
    if (!g_sdl_poetry_screen.active
        || g_sdl_poetry_screen.highlight == choice)
    {
        return;
    }

    g_sdl_poetry_screen.highlight = choice;
    sdl_welcome_screen_mark_dirty();
}

void sdl_poetry_screen_set_alpha(byte title_alpha, byte body_alpha,
    byte transition_alpha, byte prompt_alpha)
{
    if (!g_sdl_poetry_screen.active)
        return;

    g_sdl_poetry_screen.title_alpha = title_alpha;
    g_sdl_poetry_screen.body_alpha = body_alpha;
    g_sdl_poetry_screen.transition_alpha = transition_alpha;
    g_sdl_poetry_screen.prompt_alpha = prompt_alpha;
    sdl_welcome_screen_mark_dirty();
}

void sdl_poetry_screen_set_choice_alpha(int choice, byte alpha)
{
    for (int i = 0; i < g_sdl_poetry_screen.choice_count; i++)
    {
        sdl_poetry_choice_state* item = &g_sdl_poetry_screen.choices[i];

        if (item->choice != choice)
            continue;
        item->alpha = alpha;
        sdl_welcome_screen_mark_dirty();
        return;
    }
}

void sdl_poetry_screen_set_prompt(cptr prompt, bool visible)
{
    if (!g_sdl_poetry_screen.active)
        return;

    SDL_strlcpy(g_sdl_poetry_screen.prompt, prompt ? prompt : "",
        sizeof(g_sdl_poetry_screen.prompt));
    g_sdl_poetry_screen.prompt_visible = visible;
    sdl_welcome_screen_mark_dirty();
}

void sdl_poetry_screen_update(bool title_visible, byte title_attr,
    bool body_visible, byte body_attr, bool transition_visible,
    byte transition_attr, bool prompt_visible)
{
    if (!g_sdl_poetry_screen.active)
        return;

    g_sdl_poetry_screen.title_visible = title_visible;
    g_sdl_poetry_screen.title_attr = title_attr;
    g_sdl_poetry_screen.body_visible = body_visible;
    g_sdl_poetry_screen.body_attr = body_attr;
    g_sdl_poetry_screen.transition_visible = transition_visible;
    g_sdl_poetry_screen.transition_attr = transition_attr;
    g_sdl_poetry_screen.prompt_visible = prompt_visible;
    sdl_welcome_screen_mark_dirty();
}

void sdl_poetry_screen_hide(void)
{
    bool was_active = g_sdl_poetry_screen.active;

    memset(&g_sdl_poetry_screen, 0, sizeof(g_sdl_poetry_screen));
    if (was_active)
        sdl_welcome_screen_mark_dirty();
}

bool sdl_poetry_screen_active(void)
{
    return g_sdl_poetry_screen.active;
}

bool sdl_poetry_screen_captures_pointer(void)
{
    return g_sdl_poetry_screen.active
        && g_sdl_poetry_screen.choice_count > 0
        && g_sdl_poetry_screen.prompt_visible;
}

static bool sdl_poetry_screen_choice_at(float x, float y, int* out_choice)
{
    if (out_choice)
        *out_choice = -1;
    if (!sdl_poetry_screen_captures_pointer())
        return false;

    for (int i = 0; i < g_sdl_poetry_screen.choice_count; i++)
    {
        const sdl_poetry_choice_state* item =
            &g_sdl_poetry_screen.choices[i];

        if (item->visible && sdl_point_in_frect(&item->hit_rect, x, y))
        {
            if (out_choice)
                *out_choice = item->choice;
            return true;
        }
    }

    return false;
}

bool sdl_poetry_screen_handle_pointer(float x, float y, int action)
{
    int choice = -1;
    bool wake = false;

    if (!sdl_poetry_screen_captures_pointer())
        return false;
    (void)sdl_narrative_portrait_transform_pointer(&x, &y);
    if (!sdl_poetry_screen_choice_at(x, y, &choice))
    {
        if (strstr(g_sdl_poetry_screen.prompt, "cancel")
            && sdl_point_in_frect(&g_sdl_poetry_screen.prompt_rect, x, y))
        {
            Term_keypress(ESCAPE);
        }
        return true;
    }

    if (!ui_menu_click_handle_choice_action(choice, action, &wake))
        return true;

    sdl_welcome_screen_mark_dirty();
    Term_keypress((action == UI_MENU_CLICK_SECONDARY)
        ? UI_MENU_CLICK_WAKE_KEY : '\r');
    (void)wake;
    return true;
}

bool sdl_poetry_screen_handle_hover_pointer(float x, float y)
{
    int choice = -1;
    bool wake = false;

    if (!sdl_poetry_screen_captures_pointer())
        return false;
    (void)sdl_narrative_portrait_transform_pointer(&x, &y);
    if (!sdl_poetry_screen_choice_at(x, y, &choice))
    {
        if (ui_menu_click_clear_hover(&wake) && wake)
            Term_keypress(UI_MENU_CLICK_WAKE_KEY);
        return true;
    }
    if (!ui_menu_click_handle_choice_action(choice, UI_MENU_CLICK_HOVER,
            &wake))
    {
        return true;
    }

    sdl_welcome_screen_mark_dirty();
    if (wake)
        Term_keypress(UI_MENU_CLICK_WAKE_KEY);
    return true;
}

bool sdl_pause_text_screen_begin(void)
{
    if (!sdl_welcome_screen_available())
        return false;

    memset(&g_sdl_pause_text_screen, 0, sizeof(g_sdl_pause_text_screen));
    g_sdl_pause_text_screen.active = true;
    sdl_welcome_screen_mark_dirty();
    return true;
}

void sdl_pause_text_screen_add_line(cptr text, byte attr, int indent)
{
    sdl_pause_text_line* line;

    if (!g_sdl_pause_text_screen.active
        || g_sdl_pause_text_screen.line_count >= SDL_PAUSE_TEXT_MAX_LINES)
    {
        return;
    }

    line = &g_sdl_pause_text_screen
        .lines[g_sdl_pause_text_screen.line_count++];
    line->attr = attr;
    line->indent = MAX(0, indent);
    SDL_strlcpy(line->text, text ? text : "", sizeof(line->text));
    sdl_welcome_screen_mark_dirty();
}

void sdl_pause_text_screen_set_visible_lines(int visible_lines)
{
    if (!g_sdl_pause_text_screen.active)
        return;

    g_sdl_pause_text_screen.visible_lines = sdl_char_sheet_clampi(
        visible_lines, 0, g_sdl_pause_text_screen.line_count);
    sdl_welcome_screen_mark_dirty();
}

void sdl_pause_text_screen_hide(void)
{
    bool was_active = g_sdl_pause_text_screen.active;

    memset(&g_sdl_pause_text_screen, 0, sizeof(g_sdl_pause_text_screen));
    if (was_active)
        sdl_welcome_screen_mark_dirty();
}

bool sdl_pause_text_screen_active(void)
{
    return g_sdl_pause_text_screen.active;
}

bool sdl_tale_screen_begin(cptr title)
{
    if (!sdl_welcome_screen_available())
        return false;

    memset(&g_sdl_tale_screen, 0, sizeof(g_sdl_tale_screen));
    g_sdl_tale_screen.active = true;
    g_sdl_tale_screen.active_entry = -1;
    g_sdl_tale_screen.active_visible_characters = -1;
    g_sdl_tale_screen.active_alpha = 0;
    SDL_strlcpy(g_sdl_tale_screen.title,
        (title && title[0]) ? title : "=== The Tale So Far ===",
        sizeof(g_sdl_tale_screen.title));
    sdl_welcome_screen_mark_dirty();
    return true;
}

void sdl_tale_screen_add_entry(cptr heading, cptr body)
{
    sdl_tale_entry* entry;

    if (!g_sdl_tale_screen.active
        || g_sdl_tale_screen.entry_count >= SDL_TALE_MAX_ENTRIES)
    {
        return;
    }

    entry = &g_sdl_tale_screen
        .entries[g_sdl_tale_screen.entry_count++];
    SDL_strlcpy(entry->heading, heading ? heading : "",
        sizeof(entry->heading));
    SDL_strlcpy(entry->body, body ? body : "", sizeof(entry->body));
    g_sdl_tale_screen.layout_canvas_w = 0;
    g_sdl_tale_screen.layout_canvas_h = 0;
    sdl_welcome_screen_mark_dirty();
}

void sdl_tale_screen_set_manuscript(bool enabled)
{
    if (!g_sdl_tale_screen.active)
        return;

    g_sdl_tale_screen.manuscript = enabled;
    g_sdl_tale_screen.layout_canvas_w = 0;
    g_sdl_tale_screen.layout_canvas_h = 0;
    g_sdl_tale_screen.layout_line_count = 0;
    sdl_welcome_screen_mark_dirty();
}

void sdl_tale_screen_hide(void)
{
    bool was_active = g_sdl_tale_screen.active;

    memset(&g_sdl_tale_screen, 0, sizeof(g_sdl_tale_screen));
    if (was_active)
        sdl_welcome_screen_mark_dirty();
}

bool sdl_tale_screen_active(void)
{
    return g_sdl_tale_screen.active;
}

bool sdl_tale_screen_handle_pointer(float x, float y)
{
    if (!g_sdl_tale_screen.active)
        return false;

    (void)sdl_narrative_portrait_transform_pointer(&x, &y);

    if (g_sdl_tale_screen.prompt_visible
        && g_sdl_tale_screen.prompt_skip_rect.w > 0.0f
        && sdl_point_in_frect(&g_sdl_tale_screen.prompt_skip_rect, x, y))
    {
        Term_keypress(ESCAPE);
        return true;
    }

    Term_keypress('\r');
    return true;
}

bool sdl_tale_screen_handle_hover_pointer(float x, float y)
{
    bool hovered;

    if (!g_sdl_tale_screen.active)
        return false;

    (void)sdl_narrative_portrait_transform_pointer(&x, &y);

    hovered = g_sdl_tale_screen.prompt_visible
        && g_sdl_tale_screen.prompt_final
        && g_sdl_tale_screen.prompt_next_rect.w > 0.0f
        && sdl_point_in_frect(&g_sdl_tale_screen.prompt_next_rect, x, y);
    if (g_sdl_tale_screen.prompt_hovered != hovered)
    {
        g_sdl_tale_screen.prompt_hovered = hovered;
        sdl_welcome_screen_mark_dirty();
    }
    return true;
}

bool sdl_welcome_screen_cycle_intro(int direction)
{
    int style;

    if (!sdl_welcome_screen_active())
        return false;

    style = sdl_welcome_screen_normalize_intro_style(
        g_sdl_welcome_screen.intro_style);
    if (direction < 0)
    {
        style = (style > INTRO_STYLE_FLAME)
            ? style - 1
            : INTRO_STYLE_MAX - 1;
    }
    else
    {
        style = (style + 1 < INTRO_STYLE_MAX)
            ? style + 1
            : INTRO_STYLE_FLAME;
    }

    g_sdl_welcome_screen.intro_style = style;
    sdl_welcome_screen_clear_hits();
    sdl_welcome_screen_mark_dirty();
    return true;
}

typedef struct sdl_welcome_layout_line {
    const sdl_welcome_intro_line* source;
    TTF_Font* font;
    SDL_FRect box;
    float line_h;
    float gap_before;
    int text_w;
    int text_h;
    bool centered;
} sdl_welcome_layout_line;

typedef struct sdl_welcome_layout_metrics {
    int base_px;
    float column_x;
    float column_w;
    float footer_x;
    float footer_w;
    float top;
    float intro_bottom;
    float footer_top;
    float footer_line_h;
    float footer_gap;
    TTF_Font* footer_font;
} sdl_welcome_layout_metrics;

typedef struct sdl_welcome_layout_cache {
    bool valid;
    SDL_Rect canvas;
    sdl_welcome_screen_mode mode;
    int intro_style;
    bool show_wizard;
    bool new_metarun;
    int count;
    sdl_welcome_layout_line lines[SDL_WELCOME_MAX_LINES];
    sdl_welcome_layout_metrics metrics;
} sdl_welcome_layout_cache;

static sdl_welcome_layout_cache g_sdl_welcome_layout_cache;

static bool sdl_welcome_layout_cache_matches(const SDL_Rect* canvas)
{
    if (!canvas || !g_sdl_welcome_layout_cache.valid)
        return false;

    return g_sdl_welcome_layout_cache.canvas.x == canvas->x
        && g_sdl_welcome_layout_cache.canvas.y == canvas->y
        && g_sdl_welcome_layout_cache.canvas.w == canvas->w
        && g_sdl_welcome_layout_cache.canvas.h == canvas->h
        && g_sdl_welcome_layout_cache.mode == g_sdl_welcome_screen.mode
        && g_sdl_welcome_layout_cache.intro_style
            == g_sdl_welcome_screen.intro_style
        && g_sdl_welcome_layout_cache.show_wizard
            == g_sdl_welcome_screen.show_wizard
        && g_sdl_welcome_layout_cache.new_metarun
            == g_sdl_welcome_screen.new_metarun;
}

static int sdl_welcome_slot_for_role(sdl_welcome_line_role role)
{
    /* The opening quote, the title, the song credit and the subtitle tagline
     * keep the primary story font (storyfont 1).  Everything else on the
     * welcome screen uses the secondary story font (storyfont 2). */
    switch (role)
    {
    case SDL_WELCOME_LINE_QUOTE:
    case SDL_WELCOME_LINE_TITLE:
    case SDL_WELCOME_LINE_SONG_ATTRIBUTION:
    case SDL_WELCOME_LINE_SUBTITLE:
        return SDL_STORY_FONT_SLOT_DEFAULT;
    case SDL_WELCOME_LINE_ATTRIBUTION:
    case SDL_WELCOME_LINE_BODY:
    case SDL_WELCOME_LINE_ACTION:
    default:
        return SDL_WELCOME_STORY_FONT_SLOT;
    }
}

static float sdl_welcome_role_font_factor(sdl_welcome_line_role role)
{
    switch (role)
    {
    case SDL_WELCOME_LINE_TITLE: return 1.58f;
    case SDL_WELCOME_LINE_SUBTITLE: return 0.94f;
    case SDL_WELCOME_LINE_ATTRIBUTION:
    case SDL_WELCOME_LINE_SONG_ATTRIBUTION: return 0.76f;
    /* The epigraph quote renders in Cinzel (storyfont 1), which is visually
     * larger than the Garamond body at the same px; pull its factor well below
     * 1.0 so the quote reads quieter than the body it introduces. */
    case SDL_WELCOME_LINE_QUOTE: return 0.65f;
    case SDL_WELCOME_LINE_BODY:
    case SDL_WELCOME_LINE_ACTION:
    default: return 1.0f;
    }
}

static bool sdl_welcome_line_is_caption(sdl_welcome_line_role role)
{
    switch (role)
    {
    case SDL_WELCOME_LINE_TITLE:
    case SDL_WELCOME_LINE_SUBTITLE:
    case SDL_WELCOME_LINE_ATTRIBUTION:
    case SDL_WELCOME_LINE_SONG_ATTRIBUTION:
        return true;
    case SDL_WELCOME_LINE_QUOTE:
    case SDL_WELCOME_LINE_BODY:
    case SDL_WELCOME_LINE_ACTION:
    default:
        return false;
    }
}

static bool sdl_welcome_line_is_main_paragraph(sdl_welcome_line_role role)
{
    return role == SDL_WELCOME_LINE_BODY || role == SDL_WELCOME_LINE_ACTION;
}

static bool sdl_welcome_line_is_name(sdl_welcome_line_role role)
{
    return role == SDL_WELCOME_LINE_TITLE
        || role == SDL_WELCOME_LINE_SUBTITLE;
}

static cptr sdl_welcome_display_text(cptr text)
{
    if (!text)
        return "";
    while (*text == ' ' || *text == '\t')
        text++;
    return text;
}

static int sdl_welcome_font_px_for_role(int base_px,
    sdl_welcome_line_role role)
{
    int px = (int)((float)base_px * sdl_welcome_role_font_factor(role)
        + 0.5f);

    return MAX(1, px);
}

static int sdl_welcome_footer_font_px(int base_px)
{
    return MAX(1, (int)((float)base_px * 0.78f + 0.5f));
}

static float sdl_welcome_line_scale_for_role(sdl_welcome_line_role role)
{
    switch (role)
    {
    case SDL_WELCOME_LINE_TITLE: return 1.16f;
    case SDL_WELCOME_LINE_ATTRIBUTION:
    case SDL_WELCOME_LINE_SONG_ATTRIBUTION: return 1.08f;
    case SDL_WELCOME_LINE_SUBTITLE: return 1.10f;
    case SDL_WELCOME_LINE_QUOTE: return 1.12f;
    case SDL_WELCOME_LINE_ACTION: return 1.12f;
    case SDL_WELCOME_LINE_BODY:
    default: return 1.10f;
    }
}

static float sdl_welcome_line_h_for_role(TTF_Font* font, int font_px,
    sdl_welcome_line_role role)
{
    return sdl_char_sheet_line_h(font, font_px,
        sdl_welcome_line_scale_for_role(role));
}

static SDL_FRect sdl_welcome_content_rect(const SDL_Rect* canvas)
{
    SDL_FRect rect = { 0 };
    float margin;

    if (!canvas || !sdl_rect_has_area(canvas))
        return rect;

    margin = sdl_char_sheet_clampf((float)canvas->w * 0.06f, 18.0f,
        140.0f);
    rect.x = (float)canvas->x + margin;
    rect.w = (float)canvas->w - margin * 2.0f;
    if (rect.w < 1.0f)
    {
        rect.x = (float)canvas->x;
        rect.w = (float)canvas->w;
    }
    rect.y = (float)canvas->y;
    rect.h = (float)canvas->h;
    return rect;
}

static bool sdl_welcome_text_surface_size(TTF_Font* font, cptr text,
    int* out_w, int* out_h)
{
    int width = 0;
    int height = 0;

    if (out_w)
        *out_w = 0;
    if (out_h)
        *out_h = 0;
    if (!font || !text || !text[0])
        return false;

    if (!TTF_GetStringSize(font, text, 0, &width, &height))
        return false;
    if (out_w)
        *out_w = width;
    if (out_h)
        *out_h = height;
    return true;
}

static SDL_FRect sdl_welcome_text_rect_for_size(int text_w, int text_h,
    SDL_FRect box, bool centered, float* out_scale)
{
    SDL_FRect rect = { 0 };
    float scale = 1.0f;

    if (out_scale)
        *out_scale = 0.0f;
    if (text_w <= 0 || text_h <= 0 || box.w <= 0.0f || box.h <= 0.0f)
        return rect;

    if ((float)text_w * scale > box.w)
        scale = box.w / (float)text_w;
    if ((float)text_h * scale > box.h)
        scale = box.h / (float)text_h;
    if (scale > 1.0f)
        scale = 1.0f;

    rect.w = (float)text_w * scale;
    rect.h = (float)text_h * scale;
    rect.x = centered ? box.x + (box.w - rect.w) * 0.5f : box.x;
    rect.y = box.y + (box.h - rect.h) * 0.5f;
    if (out_scale)
        *out_scale = scale;
    return rect;
}

static SDL_FRect sdl_welcome_measure_text_box(TTF_Font* font, cptr text,
    SDL_FRect box, bool centered, float* out_scale)
{
    int text_w = 0;
    int text_h = 0;

    if (!sdl_welcome_text_surface_size(font, text, &text_w, &text_h))
        return (SDL_FRect){ 0 };
    return sdl_welcome_text_rect_for_size(text_w, text_h, box, centered,
        out_scale);
}

static SDL_FRect sdl_welcome_draw_text_box(TTF_Font* font, cptr text,
    byte attr, SDL_FRect box, bool centered)
{
    SDL_FRect dst = { 0 };
    SDL_Texture* texture;
    SDL_Color color;
    int text_w = 0;
    int text_h = 0;

    if (!font || !text || !text[0] || box.w <= 0.0f || box.h <= 0.0f)
        return dst;

    color = sdl_welcome_color(attr, 255);
    texture = sdl_ui_text_texture(font, text, color, &text_w, &text_h);
    if (!texture)
        return dst;

    dst = sdl_welcome_text_rect_for_size(text_w, text_h, box,
        centered, NULL);
    if (dst.w > 0.0f && dst.h > 0.0f)
        SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);

    return dst;
}

static int sdl_welcome_text_width_n(TTF_Font* font, cptr text, int len)
{
    char buf[160];
    int width = 0;

    if (!font || !text || len <= 0)
        return 0;
    if (len >= (int)sizeof(buf))
        len = (int)sizeof(buf) - 1;
    memcpy(buf, text, (size_t)len);
    buf[len] = '\0';
    TTF_MeasureString(font, buf, len, 0, &width, NULL);
    return width;
}

static SDL_FRect sdl_welcome_text_span_rect(TTF_Font* font, cptr text,
    int start, int end, SDL_FRect box, bool centered)
{
    SDL_FRect rect = { 0 };
    SDL_FRect full;
    int text_len;
    int prefix_w;
    int span_w;
    float scale = 1.0f;

    if (!font || !text || box.w <= 0.0f || box.h <= 0.0f)
        return rect;

    text_len = (int)strlen(text);
    if (start < 0)
        start = 0;
    if (end > text_len)
        end = text_len;
    if (end <= start)
        return rect;

    full = sdl_welcome_measure_text_box(font, text, box, centered, &scale);
    if (full.w <= 0.0f || full.h <= 0.0f || scale <= 0.0f)
        return rect;

    prefix_w = sdl_welcome_text_width_n(font, text, start);
    span_w = sdl_welcome_text_width_n(font, text + start, end - start);

    rect.x = full.x + (float)prefix_w * scale;
    rect.y = full.y;
    rect.w = (float)span_w * scale;
    rect.h = full.h;
    return rect;
}

static SDL_FRect sdl_welcome_draw_text_span(TTF_Font* font, cptr text,
    int start, int end, byte attr, SDL_FRect box, bool centered)
{
    SDL_FRect hit = { 0 };
    SDL_Texture* texture;
    SDL_Color color;
    char span[128];
    int span_len;
    int text_len;
    int text_w = 0;
    int text_h = 0;
    float scale;

    if (!font || !text || box.w <= 0.0f || box.h <= 0.0f)
        return hit;
    text_len = (int)strlen(text);
    if (start < 0)
        start = 0;
    if (end > text_len)
        end = text_len;
    if (end <= start)
        return hit;

    span_len = end - start;
    if (span_len >= (int)sizeof(span))
        span_len = (int)sizeof(span) - 1;
    memcpy(span, text + start, (size_t)span_len);
    span[span_len] = '\0';

    hit = sdl_welcome_text_span_rect(font, text, start, start + span_len,
        box, centered);
    if (hit.w <= 0.0f || hit.h <= 0.0f)
        return hit;

    color = sdl_welcome_color(attr, 255);
    texture = sdl_ui_text_texture(font, span, color, &text_w, &text_h);
    if (!texture)
        return hit;

    scale = (text_h > 0) ? hit.h / (float)text_h : 1.0f;
    hit.w = (float)text_w * scale;

    SDL_RenderTexture(g_state.renderer, texture, NULL, &hit);
    return hit;
}

bool sdl_welcome_text_token_range(cptr text, cptr token, int* start,
    int* end)
{
    cptr match;

    if (!text || !token || !token[0])
        return false;

    match = strstr(text, token);
    if (!match)
        return false;

    if (start)
        *start = (int)(match - text);
    if (end)
        *end = (int)(match - text) + (int)strlen(token);
    return true;
}

bool sdl_welcome_text_command_range(cptr text, cptr token_a,
    cptr token_b, int* start, int* end)
{
    int a_start = 0;
    int a_end = 0;
    int b_start = 0;
    int b_end = 0;
    bool have_a = sdl_welcome_text_token_range(text, token_a,
        &a_start, &a_end);
    bool have_b = sdl_welcome_text_token_range(text, token_b,
        &b_start, &b_end);

    if (!have_a && !have_b)
        return false;

    if (!have_a)
    {
        a_start = b_start;
        a_end = b_end;
    }
    if (!have_b)
    {
        b_start = a_start;
        b_end = a_end;
    }

    if (start)
        *start = MIN(a_start, b_start);
    if (end)
        *end = MAX(a_end, b_end);
    return true;
}

void sdl_welcome_compose_menu_line(char* menu_line, size_t menu_size,
    char* quit_command, size_t quit_command_size, cptr* primary_token)
{
    cptr quit_token = steamdeck_controls_active() ? "Back/B" : "Q/Esc";

    if (primary_token)
        *primary_token = g_sdl_welcome_screen.new_metarun
            ? "Begin"
            : "Continue";
    if (quit_command && quit_command_size > 0)
        strnfmt(quit_command, quit_command_size, "[%s]", quit_token);
    if (menu_line && menu_size > 0)
    {
        strnfmt(menu_line, menu_size,
            g_sdl_welcome_screen.new_metarun
                ? "[Any key] Begin    [%s] Quit"
                : "[Any key] Continue  [%s] Quit",
            quit_token);
    }
}

static bool sdl_welcome_body_midpoint_break(
    const sdl_welcome_intro_line* lines, int index)
{
    int start;
    int end;
    int total;

    if (!lines || index <= 0 || !lines[index].text
        || lines[index].role != SDL_WELCOME_LINE_BODY
        || lines[index - 1].role != SDL_WELCOME_LINE_BODY)
    {
        return false;
    }

    start = index;
    while (start > 0 && lines[start - 1].text
        && lines[start - 1].role == SDL_WELCOME_LINE_BODY)
    {
        start--;
    }

    end = index;
    while (lines[end].text && lines[end].role == SDL_WELCOME_LINE_BODY)
        end++;

    total = end - start;
    return total >= 6 && index == start + total / 2;
}

static float sdl_welcome_gap_before_line(const sdl_welcome_intro_line* lines,
    int index, float line_h)
{
    sdl_welcome_line_role prev;
    sdl_welcome_line_role role;

    if (!lines || index <= 0 || !lines[index].text)
        return 0.0f;

    role = lines[index].role;
    prev = lines[index - 1].role;

    if (role == prev)
    {
        if (sdl_welcome_body_midpoint_break(lines, index))
            return line_h * 0.42f;
        return line_h * 0.04f;
    }

    if (role == SDL_WELCOME_LINE_ATTRIBUTION
        || role == SDL_WELCOME_LINE_SONG_ATTRIBUTION)
    {
        return line_h * 0.18f;
    }
    if (role == SDL_WELCOME_LINE_TITLE)
        return line_h * 0.80f;
    if (role == SDL_WELCOME_LINE_SUBTITLE)
        return line_h * 0.08f;
    if (role == SDL_WELCOME_LINE_BODY)
        return line_h * 0.58f;
    if (role == SDL_WELCOME_LINE_ACTION)
        return line_h * 0.68f;
    if (role == SDL_WELCOME_LINE_QUOTE)
        return line_h * 1.20f;

    (void)prev;
    return line_h * 0.20f;
}

static bool sdl_welcome_action_footer_gap_for_base(int base_px,
    float* out_gap)
{
    const sdl_welcome_intro_line* lines =
        sdl_welcome_intro_lines_for_style(g_sdl_welcome_screen.intro_style);
    int title_index = -1;
    bool has_action = false;

    for (int i = 0; lines[i].text; i++)
    {
        if (lines[i].role == SDL_WELCOME_LINE_TITLE)
            title_index = i;
        else if (lines[i].role == SDL_WELCOME_LINE_ACTION)
            has_action = true;
    }

    if (!has_action || title_index <= 0)
        return false;

    {
        int font_px = sdl_welcome_font_px_for_role(base_px,
            lines[title_index].role);
        TTF_Font* font = sdl_story_font_for_height_slot(font_px,
            sdl_welcome_slot_for_role(lines[title_index].role));
        float line_h;

        if (!font)
            return false;

        line_h = sdl_welcome_line_h_for_role(font, font_px,
            lines[title_index].role);
        if (out_gap)
        {
            *out_gap = sdl_welcome_gap_before_line(lines, title_index,
                line_h);
        }
    }

    return true;
}

static float sdl_welcome_top_margin(const SDL_Rect* canvas)
{
    return sdl_char_sheet_clampf((float)canvas->h * 0.055f, 18.0f, 66.0f);
}

static float sdl_welcome_bottom_margin(const SDL_Rect* canvas)
{
    return sdl_char_sheet_clampf((float)canvas->h * 0.055f, 18.0f, 60.0f);
}

void sdl_poetry_sequence_layout_begin(void)
{
    SDL_Rect canvas = sdl_get_window_pixel_rect();
    float top_margin;
    float bottom_margin;
    float available_h;

    memset(&g_sdl_poetry_sequence_layout, 0,
        sizeof(g_sdl_poetry_sequence_layout));
    (void)sdl_narrative_portrait_adjust_canvas(&canvas);
    if (!sdl_rect_has_area(&canvas))
        return;

    top_margin = sdl_welcome_top_margin(&canvas);
    bottom_margin = sdl_welcome_bottom_margin(&canvas);
    available_h = MAX(0.0f,
        (float)canvas.h - top_margin - bottom_margin);
    g_sdl_poetry_sequence_layout.active = true;
    g_sdl_poetry_sequence_layout.title_y = (float)canvas.y + top_margin
        + available_h * 0.125f;
}

void sdl_poetry_sequence_layout_end(void)
{
    memset(&g_sdl_poetry_sequence_layout, 0,
        sizeof(g_sdl_poetry_sequence_layout));
}

static float sdl_poetry_sequence_top_margin(const SDL_Rect* canvas,
    float fallback)
{
    if (!g_sdl_poetry_sequence_layout.active || !canvas)
        return fallback;
    return MAX(fallback,
        g_sdl_poetry_sequence_layout.title_y - (float)canvas->y);
}

static float sdl_welcome_footer_line_h_for_base(int base_px,
    TTF_Font** out_font)
{
    int font_px = sdl_welcome_footer_font_px(base_px);
    TTF_Font* font = sdl_story_font_for_height_slot(font_px,
        SDL_WELCOME_STORY_FONT_SLOT);

    if (out_font)
        *out_font = font;
    return sdl_char_sheet_line_h(font, font_px, 1.18f);
}

static float sdl_welcome_footer_height_for_base(int base_px,
    TTF_Font** out_font, float* out_line_h, float* out_gap)
{
    TTF_Font* font = NULL;
    float line_h = sdl_welcome_footer_line_h_for_base(base_px, &font);
    float gap = line_h * 0.36f;
    float height = line_h;

    if (g_sdl_welcome_screen.mode == SDL_WELCOME_SCREEN_MENU
        && g_sdl_welcome_screen.show_wizard)
    {
        height = line_h * 2.0f + gap;
    }

    if (out_font)
        *out_font = font;
    if (out_line_h)
        *out_line_h = line_h;
    if (out_gap)
        *out_gap = gap;
    return height;
}

static float sdl_welcome_footer_width_for_font(TTF_Font* font)
{
    int width = 0;
    int height = 0;
    int max_w = 0;

    if (!font)
        return 0.0f;

    if (g_sdl_welcome_screen.mode == SDL_WELCOME_SCREEN_MENU)
    {
        char menu_line[96];
        char quit_command[32];
        cptr primary_token;
        cptr wizard_line = "Resurrecting a character is a form of cheating.";

        sdl_welcome_compose_menu_line(menu_line, sizeof(menu_line),
            quit_command, sizeof(quit_command), &primary_token);
        if (sdl_welcome_text_surface_size(font, menu_line, &width, &height)
            && width > max_w)
        {
            max_w = width;
        }
        if (g_sdl_welcome_screen.show_wizard
            && sdl_welcome_text_surface_size(font, wizard_line, &width,
                &height)
            && width > max_w)
        {
            max_w = width;
        }
    }

    return (float)max_w;
}

static int sdl_welcome_measure_intro_for_base(int base_px,
    sdl_welcome_layout_line* out, int max_out, float* out_height,
    float* out_width, float* out_main_width)
{
    const sdl_welcome_intro_line* lines;
    float total_h = 0.0f;
    float max_w = 0.0f;
    float main_w = 0.0f;
    int limit = out ? max_out : SDL_WELCOME_MAX_LINES;
    int count = 0;

    if (out_height)
        *out_height = 0.0f;
    if (out_width)
        *out_width = 0.0f;
    if (out_main_width)
        *out_main_width = 0.0f;
    if (limit <= 0)
        return 0;

    lines = sdl_welcome_intro_lines_for_style(
        g_sdl_welcome_screen.intro_style);
    for (int i = 0; lines[i].text && count < limit; i++)
    {
        cptr text = sdl_welcome_display_text(lines[i].text);
        int text_w = 0;
        int text_h = 0;
        int font_px = sdl_welcome_font_px_for_role(base_px, lines[i].role);
        TTF_Font* font = sdl_story_font_for_height_slot(font_px,
            sdl_welcome_slot_for_role(lines[i].role));
        float line_h = sdl_welcome_line_h_for_role(font, font_px,
            lines[i].role);
        float gap = sdl_welcome_gap_before_line(lines, i, line_h);

        if (!font)
            continue;
        if (!sdl_welcome_text_surface_size(font, text, &text_w, &text_h))
            continue;

        if (out)
        {
            out[count].source = &lines[i];
            out[count].font = font;
            out[count].line_h = line_h;
            out[count].gap_before = gap;
            out[count].text_w = text_w;
            out[count].text_h = text_h;
            out[count].centered =
                sdl_welcome_line_is_caption(lines[i].role);
        }
        total_h += gap + line_h;
        if ((float)text_w > max_w)
            max_w = (float)text_w;
        if (sdl_welcome_line_is_main_paragraph(lines[i].role)
            && (float)text_w > main_w)
        {
            main_w = (float)text_w;
        }
        count++;
    }

    if (main_w <= 0.0f)
        main_w = max_w;

    if (out_height)
        *out_height = total_h;
    if (out_width)
        *out_width = max_w;
    if (out_main_width)
        *out_main_width = main_w;
    return count;
}

static bool sdl_welcome_measure_layout_for_base(const SDL_Rect* canvas,
    int base_px, sdl_welcome_layout_line* lines, int max_lines,
    int* out_count, sdl_welcome_layout_metrics* out_metrics, bool apply)
{
    SDL_FRect content;
    TTF_Font* footer_font = NULL;
    float top_margin;
    float bottom_margin;
    float footer_h;
    float footer_line_h;
    float footer_gap;
    float footer_w;
    float intro_h;
    float intro_w;
    float main_w;
    float intro_footer_gap;
    float need_h;
    float available_h;
    float fit_w;
    float column_x;
    float footer_x;
    int count;
    bool fits;

    if (out_count)
        *out_count = 0;
    if (!canvas || !sdl_rect_has_area(canvas))
        return false;

    content = sdl_welcome_content_rect(canvas);
    count = sdl_welcome_measure_intro_for_base(base_px, lines, max_lines,
        &intro_h, &intro_w, &main_w);
    if (count < 1 || intro_h <= 0.0f)
        return false;

    footer_h = sdl_welcome_footer_height_for_base(base_px, &footer_font,
        &footer_line_h, &footer_gap);
    footer_w = sdl_welcome_footer_width_for_font(footer_font);
    fit_w = MAX(intro_w, footer_w);

    top_margin = sdl_welcome_top_margin(canvas);
    bottom_margin = sdl_welcome_bottom_margin(canvas);
    if (!sdl_welcome_action_footer_gap_for_base(base_px, &intro_footer_gap))
    {
        intro_footer_gap = sdl_char_sheet_clampf((float)base_px * 1.35f,
            18.0f, 84.0f);
    }
    available_h = (float)canvas->h - top_margin - bottom_margin;
    need_h = intro_h + intro_footer_gap + footer_h;
    fits = fit_w <= content.w && need_h <= available_h;

    if (apply)
    {
        float footer_top;
        float intro_top;
        float intro_bottom;
        float intro_area_h;
        float vertical_scale = 1.0f;
        float content_center;
        float title_center;
        float footer_w_safe;
        float y;

        if (intro_w > content.w)
            intro_w = content.w;
        if (intro_w < 1.0f)
            intro_w = content.w;
        if (main_w > intro_w)
            main_w = intro_w;
        if (main_w < 1.0f)
            main_w = intro_w;
        footer_w_safe = footer_w;
        if (footer_w_safe > content.w)
            footer_w_safe = content.w;
        if (footer_w_safe < 1.0f)
            footer_w_safe = intro_w;

        content_center = content.x + content.w * 0.5f;
        column_x = content_center - intro_w * 0.5f;
        title_center = column_x + main_w * 0.5f;
        footer_x = content_center - footer_w_safe * 0.5f;

        footer_top = (float)canvas->y + (float)canvas->h
            - bottom_margin - footer_h;
        intro_top = (float)canvas->y + top_margin;
        intro_bottom = footer_top - intro_footer_gap;
        intro_area_h = intro_bottom - intro_top;
        if (intro_area_h < 1.0f)
            intro_area_h = 1.0f;
        if (intro_h > intro_area_h)
            vertical_scale = intro_area_h / intro_h;
        if (vertical_scale > 1.0f)
            vertical_scale = 1.0f;

        y = intro_top + (intro_area_h - intro_h * vertical_scale) * 0.5f;
        for (int i = 0; i < count; i++)
        {
            float box_w = intro_w;
            float box_x = column_x;
            sdl_welcome_line_role role = lines[i].source->role;

            y += lines[i].gap_before * vertical_scale;
            lines[i].line_h *= vertical_scale;
            if (sdl_welcome_line_is_name(role))
            {
                box_w = MAX(main_w, (float)lines[i].text_w);
                if (box_w > content.w)
                    box_w = content.w;
                box_x = title_center - box_w * 0.5f;
                if (box_x < content.x)
                    box_x = content.x;
                if (box_x + box_w > content.x + content.w)
                    box_x = content.x + content.w - box_w;
            }
            lines[i].box.x = box_x;
            lines[i].box.y = y;
            lines[i].box.w = box_w;
            lines[i].box.h = lines[i].line_h;
            y += lines[i].line_h;
        }

        if (out_metrics)
        {
            out_metrics->base_px = base_px;
            out_metrics->column_x = column_x;
            out_metrics->column_w = intro_w;
            out_metrics->footer_x = footer_x;
            out_metrics->footer_w = footer_w_safe;
            out_metrics->top = intro_top;
            out_metrics->intro_bottom = intro_bottom;
            out_metrics->footer_top = footer_top;
            out_metrics->footer_line_h = footer_line_h;
            out_metrics->footer_gap = footer_gap;
            out_metrics->footer_font = footer_font;
        }
        if (out_count)
            *out_count = count;
    }

    return fits;
}

static int sdl_welcome_max_base_px(const SDL_Rect* canvas)
{
    int by_h;
    int by_w;

    if (!canvas || !sdl_rect_has_area(canvas))
        return 18;

    by_h = (int)((float)canvas->h * 0.11f + 0.5f);
    by_w = (int)((float)canvas->w * 0.070f + 0.5f);
    return sdl_char_sheet_clampi(MIN(by_h, by_w), 18, 132);
}

static int sdl_welcome_choose_base_px(const SDL_Rect* canvas)
{
    int min_px = 10;
    int max_px = sdl_welcome_max_base_px(canvas);
    int low_px = min_px;
    int high_px = max_px;
    int best_px = min_px;

    /*
     * Layout feasibility is monotonic.  The old pixel-by-pixel descent loaded
     * nearly the entire story-font cache before character creation; subsequent
     * race sheets then received a nearest cached size instead of the size their
     * own coupled layout requested.
     */
    while (low_px <= high_px)
    {
        int px = low_px + (high_px - low_px) / 2;

        if (sdl_welcome_measure_layout_for_base(canvas, px, NULL, 0,
                NULL, NULL, false))
        {
            best_px = px;
            low_px = px + 1;
        }
        else
        {
            high_px = px - 1;
        }
    }

    return best_px;
}

static int sdl_welcome_prepare_layout(const SDL_Rect* canvas,
    sdl_welcome_layout_line* lines, int max_lines,
    sdl_welcome_layout_metrics* metrics)
{
    int count = 0;
    int base_px;

    if (sdl_welcome_layout_cache_matches(canvas))
    {
        count = g_sdl_welcome_layout_cache.count;
        if (lines && max_lines > 0)
        {
            count = MIN(count, max_lines);
            memcpy(lines, g_sdl_welcome_layout_cache.lines,
                (size_t)count * sizeof(lines[0]));
        }
        if (metrics)
            *metrics = g_sdl_welcome_layout_cache.metrics;
        return count;
    }

    base_px = sdl_welcome_choose_base_px(canvas);
    (void)sdl_welcome_measure_layout_for_base(canvas, base_px, lines,
        max_lines, &count, metrics, true);
    if (lines && metrics && count > 0)
    {
        g_sdl_welcome_layout_cache.valid = true;
        g_sdl_welcome_layout_cache.canvas = *canvas;
        g_sdl_welcome_layout_cache.mode = g_sdl_welcome_screen.mode;
        g_sdl_welcome_layout_cache.intro_style =
            g_sdl_welcome_screen.intro_style;
        g_sdl_welcome_layout_cache.show_wizard =
            g_sdl_welcome_screen.show_wizard;
        g_sdl_welcome_layout_cache.new_metarun =
            g_sdl_welcome_screen.new_metarun;
        g_sdl_welcome_layout_cache.count = count;
        memcpy(g_sdl_welcome_layout_cache.lines, lines,
            (size_t)count * sizeof(lines[0]));
        g_sdl_welcome_layout_cache.metrics = *metrics;
    }
    return count;
}

void sdl_welcome_render_intro_canvas(const SDL_Rect* canvas)
{
    sdl_welcome_layout_line layout[SDL_WELCOME_MAX_LINES];
    sdl_welcome_layout_metrics metrics = { 0 };
    int count;

    if (!canvas || !sdl_rect_has_area(canvas))
        return;

    count = sdl_welcome_prepare_layout(canvas, layout,
        (int)N_ELEMENTS(layout), &metrics);
    for (int i = 0; i < count; i++)
    {
        SDL_FRect box = layout[i].box;
        cptr text = sdl_welcome_display_text(layout[i].source->text);

        (void)sdl_welcome_draw_text_box(layout[i].font, text,
            layout[i].source->attr, box, layout[i].centered);
    }
}

void sdl_welcome_render_status_canvas(const SDL_Rect* canvas)
{
    SDL_FRect box;
    SDL_FRect content;
    sdl_welcome_layout_metrics metrics = { 0 };
    sdl_welcome_layout_line layout[SDL_WELCOME_MAX_LINES];
    cptr status = g_sdl_welcome_screen.status;

    if (!status || !status[0] || !canvas || !sdl_rect_has_area(canvas))
        return;

    (void)sdl_welcome_prepare_layout(canvas, layout,
        (int)N_ELEMENTS(layout), &metrics);
    if (!metrics.footer_font)
        return;

    content = sdl_welcome_content_rect(canvas);
    box.x = content.x;
    box.y = metrics.footer_top;
    box.w = content.w;
    box.h = metrics.footer_line_h;
    (void)sdl_welcome_draw_text_box(metrics.footer_font, status, TERM_SLATE,
        box, true);
}

void sdl_welcome_render_menu_footer_canvas(const SDL_Rect* canvas)
{
    char menu_line[96];
    char quit_command[32];
    cptr primary_token;
    cptr wizard_line = "Resurrecting a character is a form of cheating.";
    sdl_welcome_layout_metrics metrics = { 0 };
    sdl_welcome_layout_line layout[SDL_WELCOME_MAX_LINES];
    SDL_FRect prompt_box;
    int primary_start = 0;
    int primary_end = 0;
    int quit_start = 0;
    int quit_end = 0;
    bool has_primary_range;
    bool has_quit_range;

    if (!canvas || !sdl_rect_has_area(canvas))
        return;

    (void)sdl_welcome_prepare_layout(canvas, layout,
        (int)N_ELEMENTS(layout), &metrics);
    if (!metrics.footer_font)
        return;

    sdl_welcome_screen_clear_hits();

    sdl_welcome_compose_menu_line(menu_line, sizeof(menu_line),
        quit_command, sizeof(quit_command), &primary_token);

    prompt_box.x = metrics.footer_x;
    prompt_box.y = metrics.footer_top;
    if (g_sdl_welcome_screen.show_wizard)
        prompt_box.y += metrics.footer_line_h + metrics.footer_gap;
    prompt_box.w = metrics.footer_w;
    prompt_box.h = metrics.footer_line_h;

    g_sdl_welcome_screen.continue_rect =
        sdl_welcome_draw_text_box(metrics.footer_font, menu_line,
            TERM_SLATE, prompt_box, false);

    has_primary_range = sdl_welcome_text_command_range(menu_line, "[Any key]",
        primary_token, &primary_start, &primary_end);
    has_quit_range = sdl_welcome_text_command_range(menu_line, quit_command,
        "Quit", &quit_start, &quit_end);

    if (has_primary_range)
        g_sdl_welcome_screen.continue_rect =
            sdl_welcome_text_span_rect(metrics.footer_font, menu_line,
                primary_start, primary_end, prompt_box, false);
    if (has_quit_range)
        g_sdl_welcome_screen.quit_rect =
            sdl_welcome_text_span_rect(metrics.footer_font, menu_line,
                quit_start, quit_end, prompt_box, false);

    if (g_sdl_welcome_screen.hover_continue && has_primary_range)
        (void)sdl_welcome_draw_text_span(metrics.footer_font, menu_line,
            primary_start, primary_end, TERM_L_BLUE, prompt_box, false);
    if (g_sdl_welcome_screen.hover_quit && has_quit_range)
        (void)sdl_welcome_draw_text_span(metrics.footer_font, menu_line,
            quit_start, quit_end, TERM_L_BLUE, prompt_box, false);

    if (g_sdl_welcome_screen.show_wizard)
    {
        SDL_FRect wizard_box = prompt_box;

        wizard_box.y = metrics.footer_top;
        (void)sdl_welcome_draw_text_box(metrics.footer_font, wizard_line,
            TERM_BLUE, wizard_box, false);
    }
}

static void sdl_welcome_screen_render_canvas(const SDL_Rect* canvas)
{
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    sdl_welcome_render_intro_canvas(canvas);

    if (g_sdl_welcome_screen.mode == SDL_WELCOME_SCREEN_MENU)
        sdl_welcome_render_menu_footer_canvas(canvas);
    else
        sdl_welcome_render_status_canvas(canvas);
}

/* Narrative screens use their portrait presentation whenever the mobile
 * window itself is portrait.  The old Android-only implementation rendered
 * into a swapped texture and rotated both pixels and pointer coordinates;
 * actual platform orientation makes that extra transform incorrect. */
static bool sdl_narrative_portrait_adjust_canvas(SDL_Rect* canvas)
{
    SDL_Rect safe;

    if (!canvas || !sdl_mobile_portrait_layout_active())
        return false;

    safe = sdl_get_layout_screen_rect();
    if (!sdl_rect_has_area(&safe))
        return false;

    *canvas = safe;
    return true;

}

static bool sdl_narrative_portrait_begin(SDL_Rect* canvas)
{
    if (g_state.renderer)
        (void)SDL_SetRenderTarget(g_state.renderer, NULL);
    g_sdl_narrative_portrait_rendering =
        sdl_narrative_portrait_adjust_canvas(canvas);
    return g_sdl_narrative_portrait_rendering;
}

static void sdl_narrative_portrait_finish(bool portrait)
{
    (void)portrait;
    g_sdl_narrative_portrait_rendering = false;
}

bool sdl_narrative_portrait_transform_pointer(float* x, float* y)
{
    (void)x;
    (void)y;
    return false;
}

void sdl_welcome_screen_render(void)
{
    SDL_Rect window;
    bool portrait;

    if (!sdl_welcome_screen_active() || !sdl_welcome_screen_available())
        return;

    window = sdl_get_window_pixel_rect();
    if (!sdl_rect_has_area(&window))
        return;

    portrait = sdl_narrative_portrait_begin(&window);
    sdl_welcome_screen_render_canvas(&window);
    sdl_narrative_portrait_finish(portrait);
}

enum {
    SDL_CHAR_SHEET_TEXT_LEN = 768,
    SDL_CHAR_SHEET_MAX_LINES = 80,
    SDL_CHAR_SHEET_LIVE_ITEM_MAX = 128,
    SDL_CHAR_SHEET_HIT_MAX = 224
};

typedef struct sdl_char_sheet_line {
    char text[SDL_CHAR_SHEET_TEXT_LEN];
    byte attr;
    int choice;
    char desc[256];
} sdl_char_sheet_line;

void sdl_char_sheet_add_line(sdl_char_sheet_line* lines, int* count,
    int max_count, cptr text, byte attr, int choice, cptr desc);

float sdl_char_sheet_clampf(float value, float min_value,
    float max_value)
{
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

int sdl_char_sheet_clampi(int value, int min_value, int max_value)
{
    if (value < min_value)
        return min_value;
    if (value > max_value)
        return max_value;
    return value;
}

int sdl_char_sheet_text_width(TTF_Font* font, cptr text)
{
    int width = 0;

    if (!font || !text || !text[0])
        return 0;

    TTF_MeasureString(font, text, strlen(text), 0, &width, NULL);
    return width;
}

float sdl_char_sheet_line_h(TTF_Font* font, int fallback_px,
    float scale)
{
    int h = font ? TTF_GetFontHeight(font) : fallback_px;

    if (h < 1)
        h = fallback_px;
    if (h < 1)
        h = 1;
    return (float)h * scale;
}

TTF_Font* sdl_char_sheet_font_for_rows(float available_h, int rows,
    int min_px, int max_px, float line_scale, float* out_line_h,
    int* out_px)
{
    int low_px;
    int high_px;
    int chosen_px = min_px;
    float chosen_line_h = 1.0f;
    TTF_Font* chosen_font = NULL;

    if (rows < 1)
        rows = 1;
    if (available_h < 1.0f)
        available_h = 1.0f;
    if (max_px < min_px)
        max_px = min_px;

    /*
     * Row fit is monotonic.  Searching every intermediate pixel size polluted
     * the shared story-font cache while moving through character races; once
     * full, later races silently received a nearest cached font from an
     * earlier race.  Binary search preserves the same largest-fitting result
     * without creating a global cross-race font coupling.
     */
    low_px = min_px;
    high_px = max_px;
    while (low_px <= high_px)
    {
        int px = low_px + (high_px - low_px) / 2;
        TTF_Font* font = sdl_story_font_for_height(px);
        float line_h;

        if (!font)
        {
            high_px = px - 1;
            continue;
        }

        line_h = sdl_char_sheet_line_h(font, px, line_scale);
        if (line_h * (float)rows <= available_h)
        {
            chosen_font = font;
            chosen_px = px;
            chosen_line_h = line_h;
            low_px = px + 1;
        }
        else
        {
            high_px = px - 1;
        }
    }

    if (!chosen_font)
    {
        chosen_font = sdl_story_font_for_height(min_px);
        chosen_px = min_px;
        chosen_line_h = sdl_char_sheet_line_h(chosen_font, min_px,
            line_scale);
    }

    if (chosen_line_h * (float)rows > available_h)
    {
        chosen_line_h = available_h / (float)rows;
        if (chosen_line_h < 1.0f)
            chosen_line_h = 1.0f;
    }

    if (out_line_h)
        *out_line_h = chosen_line_h;
    if (out_px)
        *out_px = chosen_px;

    return chosen_font;
}

static SDL_FRect sdl_char_sheet_draw_text_aligned(TTF_Font* font, cptr text,
    byte attr, float x, float y, float max_w, float max_h, bool centered,
    bool centered_vertically)
{
    SDL_FRect dst = { 0 };
    SDL_Texture* texture;
    SDL_Color color;
    float scale = 1.0f;
    int text_w = 0;
    int text_h = 0;

    if (!font || !text || !text[0] || max_w <= 0.0f)
        return dst;

    color = sdl_welcome_color(attr, 255);
    texture = sdl_ui_text_texture(font, text, color, &text_w, &text_h);
    if (!texture)
        return dst;

    if (text_w > 0 && (float)text_w > max_w)
        scale = max_w / (float)text_w;
    if (max_h > 0.0f && text_h > 0 && (float)text_h * scale > max_h)
        scale = max_h / (float)text_h;
    if (scale > 1.0f)
        scale = 1.0f;

    dst.w = (float)text_w * scale;
    dst.h = (float)text_h * scale;
    dst.x = centered ? x + (max_w - dst.w) * 0.5f : x;
    dst.y = (centered_vertically && max_h > 0.0f)
        ? y + (max_h - dst.h) * 0.5f
        : y;

    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
    return dst;
}

static SDL_FRect sdl_char_sheet_draw_text_alpha(TTF_Font* font, cptr text,
    byte attr, byte alpha, float x, float y, float max_w, float max_h)
{
    SDL_FRect dst = { 0 };
    SDL_Texture* texture;
    SDL_Color color;
    float scale = 1.0f;
    int text_w = 0;
    int text_h = 0;

    if (!font || !text || !text[0] || alpha == 0 || max_w <= 0.0f)
        return dst;

    color = sdl_welcome_color(attr, alpha);
    texture = sdl_ui_text_texture(font, text, color, &text_w, &text_h);
    if (!texture)
        return dst;

    if (text_w > 0 && (float)text_w > max_w)
        scale = max_w / (float)text_w;
    if (max_h > 0.0f && text_h > 0 && (float)text_h * scale > max_h)
        scale = max_h / (float)text_h;
    if (scale > 1.0f)
        scale = 1.0f;

    dst = (SDL_FRect){
        .x = x,
        .y = y,
        .w = (float)text_w * scale,
        .h = (float)text_h * scale,
    };
    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
    return dst;
}

SDL_FRect sdl_char_sheet_draw_text(TTF_Font* font, cptr text,
    byte attr, float x, float y, float max_w, float max_h, bool centered)
{
    return sdl_char_sheet_draw_text_aligned(font, text, attr, x, y, max_w,
        max_h, centered, false);
}

static SDL_FRect sdl_char_sheet_draw_button_text(TTF_Font* font, cptr text,
    byte attr, const SDL_FRect* rect)
{
    SDL_FRect empty = { 0 };

    if (!rect)
        return empty;

    return sdl_char_sheet_draw_text_aligned(font, text, attr, rect->x,
        rect->y, rect->w, rect->h, true, true);
}

static void sdl_char_sheet_draw_title_text_fonts(TTF_Font* font,
    TTF_Font* suffix_font, cptr title,
    byte title_attr, cptr suffix, byte suffix_attr, float x, float y,
    float max_w, float max_h)
{
    int title_w;
    int suffix_w;
    float used_w;
    float start_x;
    float title_max_w;
    SDL_FRect title_rect;

    if (!suffix || !suffix[0])
    {
        (void)sdl_char_sheet_draw_text(font, title, title_attr, x, y, max_w,
            max_h, true);
        return;
    }

    title_w = sdl_char_sheet_text_width(font, title);
    if (!suffix_font)
        suffix_font = font;
    suffix_w = sdl_char_sheet_text_width(suffix_font, suffix);
    used_w = (float)(title_w + suffix_w);
    if (used_w > max_w)
        used_w = max_w;
    start_x = x + (max_w - used_w) * 0.5f;

    title_max_w = max_w - (float)suffix_w;
    if (title_max_w < max_w * 0.25f)
        title_max_w = max_w * 0.25f;

    title_rect = sdl_char_sheet_draw_text(font, title, title_attr, start_x, y,
        title_max_w, max_h, false);
    if (title_rect.w < max_w)
    {
        (void)sdl_char_sheet_draw_text_aligned(suffix_font, suffix,
            suffix_attr, start_x + title_rect.w, y, max_w - title_rect.w,
            max_h, false, true);
    }
}

void sdl_char_sheet_draw_title_text(TTF_Font* font, cptr title,
    byte title_attr, cptr suffix, byte suffix_attr, float x, float y,
    float max_w, float max_h)
{
    sdl_char_sheet_draw_title_text_fonts(font, font, title, title_attr,
        suffix, suffix_attr, x, y, max_w, max_h);
}

/*
 * Pick the largest power font that is no larger than the fitted name font for
 * any hero in this race.  The name remains on the normal title font; its
 * effective size is the title size multiplied by the horizontal fit scale
 * left after that hero's own power suffix has reserved its measured width.
 */
#if SIL_SDL_MOBILE_BUILD
static TTF_Font* sdl_char_sheet_character_power_font(TTF_Font* title_font,
    int title_px, int slot, float max_w, float max_h)
{
    int candidate_px;
    int width_key = (int)max_w;

    if (!title_font || title_px <= 0 || max_w <= 0.0f
        || g_sdl_character_sheet_screen.select_title_candidate_count <= 0)
    {
        return title_font;
    }

    if (g_sdl_character_sheet_screen.select_title_power_px > 0
        && g_sdl_character_sheet_screen.select_title_power_for_title_px
            == title_px
        && g_sdl_character_sheet_screen.select_title_power_for_width
            == width_key)
    {
        return sdl_story_font_for_height_slot(
            g_sdl_character_sheet_screen.select_title_power_px, slot);
    }

    for (candidate_px = title_px; candidate_px >= 1; candidate_px--)
    {
        TTF_Font* power_font =
            sdl_story_font_for_height_slot(candidate_px, slot);
        bool fits_group = true;

        if (!power_font)
            continue;

        for (int i = 0;
             i < g_sdl_character_sheet_screen.select_title_candidate_count;
             i++)
        {
            const sdl_character_sheet_title_candidate* candidate =
                &g_sdl_character_sheet_screen.select_title_candidates[i];
            int name_w = sdl_char_sheet_text_width(title_font,
                candidate->title);
            int suffix_w = sdl_char_sheet_text_width(power_font,
                candidate->suffix);
            float name_max_w = max_w - (float)suffix_w;
            float name_scale = 1.0f;
            float fitted_name_px;

            if (name_max_w < max_w * 0.25f)
                name_max_w = max_w * 0.25f;
            if (name_w > 0 && (float)name_w > name_max_w)
                name_scale = name_max_w / (float)name_w;
            if (max_h > 0.0f)
            {
                float title_line_h = sdl_char_sheet_line_h(title_font,
                    title_px, 1.0f);

                if (title_line_h * name_scale > max_h)
                    name_scale = max_h / title_line_h;
            }

            fitted_name_px = (float)title_px * MIN(1.0f, name_scale);
            if ((float)candidate_px > fitted_name_px)
            {
                fits_group = false;
                break;
            }
        }

        if (fits_group)
        {
            g_sdl_character_sheet_screen.select_title_power_px = candidate_px;
            g_sdl_character_sheet_screen.select_title_power_for_title_px =
                title_px;
            g_sdl_character_sheet_screen.select_title_power_for_width =
                width_key;
            return power_font;
        }
    }

    g_sdl_character_sheet_screen.select_title_power_px = 1;
    g_sdl_character_sheet_screen.select_title_power_for_title_px = title_px;
    g_sdl_character_sheet_screen.select_title_power_for_width = width_key;
    return sdl_story_font_for_height_slot(1, slot);
}
#endif

int sdl_char_sheet_wrap_text(TTF_Font* font, cptr text, float max_w,
    char (*lines)[SDL_CHAR_SHEET_TEXT_LEN], int max_lines)
{
    const char* p;
    char current[SDL_CHAR_SHEET_TEXT_LEN];
    int line_count = 0;

    if (!font || !text || !text[0])
        return 0;

    if (max_w < 1.0f)
        max_w = 1.0f;

    p = text;
    current[0] = '\0';

    while (*p)
    {
        const char* word_start;
        size_t word_len;
        char word[SDL_CHAR_SHEET_TEXT_LEN];
        char candidate[SDL_CHAR_SHEET_TEXT_LEN];
        bool overflow;

        while (*p && *p != '\n' && isspace((unsigned char)*p))
            p++;

        if (!*p)
            break;

        if (*p == '\n')
        {
            if (line_count < max_lines && lines)
                SDL_strlcpy(lines[line_count], current,
                    SDL_CHAR_SHEET_TEXT_LEN);
            line_count++;
            if (lines && max_lines > 0 && line_count >= max_lines)
                return line_count;
            current[0] = '\0';
            p++;
            continue;
        }

        word_start = p;
        while (*p && *p != '\n' && !isspace((unsigned char)*p))
            p++;

        word_len = (size_t)(p - word_start);
        if (word_len >= sizeof(word))
            word_len = sizeof(word) - 1;
        memcpy(word, word_start, word_len);
        word[word_len] = '\0';

        if (!word[0])
            continue;

        if (!current[0])
        {
            SDL_strlcpy(current, word, sizeof(current));
            continue;
        }

        strnfmt(candidate, sizeof(candidate), "%s %s", current, word);
        overflow = sdl_char_sheet_text_width(font, candidate) > (int)max_w;
        if (overflow)
        {
            if (line_count < max_lines && lines)
                SDL_strlcpy(lines[line_count], current,
                    SDL_CHAR_SHEET_TEXT_LEN);
            line_count++;
            if (lines && max_lines > 0 && line_count >= max_lines)
                return line_count;
            SDL_strlcpy(current, word, sizeof(current));
        }
        else
        {
            SDL_strlcpy(current, candidate, sizeof(current));
        }
    }

    if (current[0])
    {
        if (line_count < max_lines && lines)
            SDL_strlcpy(lines[line_count], current, SDL_CHAR_SHEET_TEXT_LEN);
        line_count++;
    }

    return line_count;
}

/*
 * Return the line budget needed by a fitted character-selection description.
 *
 * select_desc_sizing keeps the layout stable, but no single sample string can
 * represent the worst wrapping at every font and canvas width: a shorter lore
 * entry can need more lines because its word breaks differ.  Measure all lore
 * candidates when they are available so the fitted line count never clips the
 * currently focused character's final line.
 */
enum { FITTED_WRAP_CACHE_SIZE = 128 };

typedef struct fitted_wrap_cache_entry {
    Uint64 candidate_hash;
    TTF_Font* font;
    int width;
    int candidate_count;
    int lines;
} fitted_wrap_cache_entry;

static fitted_wrap_cache_entry
    g_fitted_wrap_cache[FITTED_WRAP_CACHE_SIZE];
static int g_fitted_wrap_next_cache_entry = 0;
static void sdl_char_sheet_book_body_px_cache_clear(void);

/*
 * Font pointers are part of the cache key.  Clear these non-owning references
 * before the story-font cache closes its fonts so an allocator-reused pointer
 * can never inherit an old wrapping result.
 */
void sdl_char_sheet_fitted_wrap_cache_clear(void)
{
    SDL_zero(g_fitted_wrap_cache);
    g_fitted_wrap_next_cache_entry = 0;
    sdl_char_sheet_book_body_px_cache_clear();
}

static int sdl_char_sheet_select_description_line_count(TTF_Font* font,
    cptr fallback, float width)
{
    if (g_sdl_character_sheet_screen.context
        == SDL_CHARACTER_SHEET_BIRTH_SELECT)
    {
        int count =
            g_sdl_character_sheet_screen.select_desc_candidate_count;
        Uint64 candidate_hash =
            g_sdl_character_sheet_screen.select_desc_candidate_hash;
        int wrap_width = (int)width;
        int lines = 0;

        if (count > (int)N_ELEMENTS(
                g_sdl_character_sheet_screen.select_desc_candidates))
        {
            count = (int)N_ELEMENTS(
                g_sdl_character_sheet_screen.select_desc_candidates);
        }
        if (count <= 0)
            return sdl_char_sheet_wrap_text(font, fallback, width, NULL, 0);
        for (int i = 0; i < FITTED_WRAP_CACHE_SIZE; i++)
        {
            const fitted_wrap_cache_entry* entry =
                &g_fitted_wrap_cache[i];

            if (entry->candidate_hash == candidate_hash
                && entry->font == font
                && entry->width == wrap_width
                && entry->candidate_count == count)
            {
                return entry->lines;
            }
        }
        for (int i = 0; i < count; i++)
        {
            int candidate_lines = sdl_char_sheet_wrap_text(font,
                g_sdl_character_sheet_screen.select_desc_candidates[i],
                width, NULL, 0);

            if (candidate_lines > lines)
                lines = candidate_lines;
        }
        if (count > 0)
        {
            fitted_wrap_cache_entry* entry =
                &g_fitted_wrap_cache[
                    g_fitted_wrap_next_cache_entry++
                        % FITTED_WRAP_CACHE_SIZE];

            entry->candidate_hash = candidate_hash;
            entry->font = font;
            entry->width = wrap_width;
            entry->candidate_count = count;
            entry->lines = lines;
        }
        return lines;
    }

    return sdl_char_sheet_wrap_text(font, fallback, width, NULL, 0);
}

static int sdl_char_sheet_fitted_wrap_line_count(TTF_Font* font, cptr text,
    float width, int slot)
{
    if (slot == SDL_STORY_FONT_SLOT_CHAR_DESC)
        return sdl_char_sheet_select_description_line_count(font, text, width);

    return sdl_char_sheet_wrap_text(font, text, width, NULL, 0);
}

TTF_Font* sdl_char_sheet_font_for_wrapped_text(cptr text, float width,
    float available_h, int min_px, int max_px, float line_scale, int slot,
    float* out_line_h, int* out_lines, int* out_px)
{
    int low_px;
    int high_px;
    int chosen_lines = 0;
    float chosen_line_h = 1.0f;
    TTF_Font* chosen_font = NULL;
    int chosen_px = min_px;

    if (out_line_h)
        *out_line_h = 1.0f;
    if (out_lines)
        *out_lines = 0;
    if (out_px)
        *out_px = min_px;

    if (!text || !text[0] || width <= 0.0f || available_h <= 0.0f)
        return sdl_story_font_for_height_slot(min_px, slot);

    if (max_px < min_px)
        max_px = min_px;

    /* Wrapped-text fit is monotonic too; avoid loading every rejected size
     * into the global story-font cache before reaching the race's fit. */
    low_px = min_px;
    high_px = max_px;
    while (low_px <= high_px)
    {
        int px = low_px + (high_px - low_px) / 2;
        TTF_Font* font = sdl_story_font_for_height_slot(px, slot);
        int lines;
        float line_h;

        if (!font)
        {
            high_px = px - 1;
            continue;
        }

        line_h = sdl_char_sheet_line_h(font, px, line_scale);
        lines = sdl_char_sheet_fitted_wrap_line_count(font, text, width,
            slot);
        if (lines <= 0 || line_h * (float)lines <= available_h)
        {
            chosen_font = font;
            chosen_lines = lines;
            chosen_line_h = line_h;
            chosen_px = px;
            low_px = px + 1;
        }
        else
        {
            high_px = px - 1;
        }
    }

    if (!chosen_font)
    {
        chosen_font = sdl_story_font_for_height_slot(min_px, slot);
        chosen_px = min_px;
        chosen_line_h = sdl_char_sheet_line_h(chosen_font, min_px,
            line_scale);
        chosen_lines = sdl_char_sheet_fitted_wrap_line_count(chosen_font,
            text, width, slot);
    }

    if (chosen_lines > 0
        && chosen_line_h * (float)chosen_lines > available_h)
    {
        chosen_line_h = available_h / (float)chosen_lines;
        if (chosen_line_h < 1.0f)
            chosen_line_h = 1.0f;
    }

    if (out_line_h)
        *out_line_h = chosen_line_h;
    if (out_lines)
        *out_lines = chosen_lines;
    if (out_px)
        *out_px = chosen_px;

    return chosen_font;
}

int sdl_char_sheet_font_px_for_line_height(float target_h, int min_px,
    int max_px)
{
    int chosen_px = min_px;
    int low_px;
    int high_px;

    if (target_h < 1.0f)
        target_h = 1.0f;
    if (max_px < min_px)
        max_px = min_px;

    low_px = min_px;
    high_px = max_px;
    while (low_px <= high_px) {
        int px = low_px + (high_px - low_px) / 2;
        TTF_Font* font = sdl_story_font_for_height(px);
        float line_h = sdl_char_sheet_line_h(font, px, 1.0f);

        if (font && line_h <= target_h) {
            chosen_px = px;
            low_px = px + 1;
        } else {
            high_px = px - 1;
        }
    }

    return chosen_px;
}

bool sdl_char_sheet_choice_is_valid(int choice)
{
    return choice >= 0;
}

bool sdl_char_sheet_prompt_choice_is_valid(int choice)
{
    return g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_LIVE
        && choice >= -3 && choice <= -1;
}

void sdl_char_sheet_clear_hits(void)
{
    g_sdl_character_sheet_screen.hit_count = 0;
    g_sdl_char_sheet_hover_tooltip_rect = (SDL_FRect){ 0 };
    g_sdl_char_sheet_hover_tooltip_choice = SDL_CHAR_SHEET_NO_HOVER;
}

void sdl_char_sheet_add_hit(SDL_FRect rect, int choice, cptr desc, byte attr)
{
    sdl_character_sheet_hit* hit;

    if (!sdl_char_sheet_choice_is_valid(choice))
        return;
    if (g_sdl_character_sheet_screen.hit_count >= SDL_CHAR_SHEET_HIT_MAX)
        return;
    if (rect.w <= 0.0f || rect.h <= 0.0f)
        return;

    hit = &g_sdl_character_sheet_screen
               .hits[g_sdl_character_sheet_screen.hit_count++];
    hit->rect = rect;
    hit->choice = choice;
    hit->attr = attr;
    SDL_strlcpy(hit->desc, desc ? desc : "", sizeof(hit->desc));
}

void sdl_char_sheet_add_prompt_hit(SDL_FRect rect, int choice)
{
    sdl_character_sheet_hit* hit;

    if (!sdl_char_sheet_prompt_choice_is_valid(choice))
        return;
    if (g_sdl_character_sheet_screen.hit_count >= SDL_CHAR_SHEET_HIT_MAX)
        return;
    if (rect.w <= 0.0f || rect.h <= 0.0f)
        return;

    hit = &g_sdl_character_sheet_screen
               .hits[g_sdl_character_sheet_screen.hit_count++];
    hit->rect = rect;
    hit->choice = choice;
    hit->attr = TERM_WHITE;
    hit->desc[0] = '\0';
}

/* Book mode: a clickable page-turn button (negative ids outside the prompt
 * range). Bypasses the prompt-range check so the two leaf-turn buttons work. */
void sdl_char_sheet_add_select_button_hit(SDL_FRect rect, int choice)
{
    sdl_character_sheet_hit* hit;

    if (choice >= 0)
        return;
    if (g_sdl_character_sheet_screen.hit_count >= SDL_CHAR_SHEET_HIT_MAX)
        return;
    if (rect.w <= 0.0f || rect.h <= 0.0f)
        return;

    hit = &g_sdl_character_sheet_screen
               .hits[g_sdl_character_sheet_screen.hit_count++];
    hit->rect = rect;
    hit->choice = choice;
    hit->attr = TERM_WHITE;
    hit->desc[0] = '\0';
}

const sdl_character_sheet_hit* sdl_char_sheet_hit_at(float x, float y)
{
    for (int i = g_sdl_character_sheet_screen.hit_count - 1; i >= 0; i--)
    {
        const sdl_character_sheet_hit* hit =
            &g_sdl_character_sheet_screen.hits[i];

        if (sdl_point_in_frect(&hit->rect, x, y))
            return hit;
    }

    return NULL;
}

const sdl_character_sheet_live_item* sdl_char_sheet_live_item_by_choice(
    int choice)
{
    for (int i = 0; i < g_sdl_character_sheet_screen.live_item_count; i++)
    {
        const sdl_character_sheet_live_item* item =
            &g_sdl_character_sheet_screen.live_items[i];

        if (item->choice == choice)
            return item;
    }

    return NULL;
}

const sdl_character_sheet_live_item* sdl_char_sheet_live_skill_item(
    int skill)
{
    for (int i = 0; i < g_sdl_character_sheet_screen.live_item_count; i++)
    {
        const sdl_character_sheet_live_item* item =
            &g_sdl_character_sheet_screen.live_items[i];

        if (item->kind == 0 && item->skill == skill)
            return item;
    }

    return NULL;
}

const sdl_character_sheet_live_item* sdl_char_sheet_live_label_item(
    cptr label)
{
    if (!label || !label[0])
        return NULL;

    for (int i = 0; i < g_sdl_character_sheet_screen.live_item_count; i++)
    {
        const sdl_character_sheet_live_item* item =
            &g_sdl_character_sheet_screen.live_items[i];

        if (item->label[0] && SDL_strcasecmp(item->label, label) == 0)
            return item;
    }

    return NULL;
}

static bool sdl_char_sheet_menu_command_choice(int choice)
{
    return g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_BIRTH_SELECT
        && g_sdl_character_sheet_screen.select_menu_style
        && choice >= SDL_CHAR_SHEET_INFO_CHOICE_BASE;
}

static bool sdl_char_sheet_info_only_choice(int choice)
{
    return choice >= SDL_CHAR_SHEET_INFO_CHOICE_BASE
        && choice < 10000;
}

bool sdl_char_sheet_choice_focused(int choice)
{
    int hover_choice = -1;

    if (!sdl_char_sheet_choice_is_valid(choice))
        return false;
    if (g_sdl_character_sheet_screen.focus_choice == choice)
        return true;
    if (sdl_char_sheet_menu_command_choice(choice))
        return false;
    /* While an interactive value picker (question overlay) is open over the
     * sheet it owns the shared menu-click hover; ignore that hover here, or the
     * row whose index matches the picker's cursor lights up on the sheet too
     * and the selection appears to move in two places at once. */
    if (!sdl_question_menu_captures_pointer()
        && ui_menu_click_get_hover_choice(&hover_choice)
        && hover_choice == choice)
    {
        return true;
    }
    return g_sdl_character_sheet_screen.hover_choice == choice;
}

/*
 * A row is "pressable" only if clicking it actually does something.  On the
 * live sheet the clickable item rows are just skills (a click increases them);
 * traits and plain values are informational, so they get a hover tooltip but
 * no highlight box.  The bottom-row prompt commands (abilities, increase,
 * help, notes, story, file, back) are registered as hits keyed by their
 * command code rather than as live items, and they must stay pressable too -
 * otherwise the pointer handlers silently drop every tap on them.
 * Birth screens draw their own focus rectangles, so their rows count as
 * pressable here.
 */
bool sdl_char_sheet_choice_pressable(int choice)
{
    const sdl_character_sheet_live_item* item;

    if (!sdl_char_sheet_choice_is_valid(choice))
        return false;
    if (g_sdl_character_sheet_screen.context
        == SDL_CHARACTER_SHEET_BIRTH_SELECT)
    {
        if (g_sdl_character_sheet_screen.select_menu_style)
            return true;
        return choice < SDL_CHAR_SHEET_INFO_CHOICE_BASE;
    }
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_LIVE
        && g_sdl_character_sheet_screen.context
            != SDL_CHARACTER_SHEET_BIRTH_PREVIEW)
    {
        if (sdl_char_sheet_info_only_choice(choice))
            return false;
        return true;
    }
    item = sdl_char_sheet_live_item_by_choice(choice);
    if (item)
        return item->kind == 0; /* CHARACTER_SHEET_ITEM_SKILL */

    /* Not an item row -> a bottom-row prompt command; always actionable. */
    return true;
}

bool sdl_char_sheet_prompt_focused(int choice)
{
    int hover_choice = -1;

    if (!sdl_char_sheet_prompt_choice_is_valid(choice))
        return false;
    if (g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_BIRTH_SELECT
        && g_sdl_character_sheet_screen.select_menu_style)
    {
        return false;
    }
    /* See sdl_char_sheet_choice_focused: don't borrow the picker's shared
     * hover while a value picker is modal over the sheet. */
    if (!sdl_question_menu_captures_pointer()
        && ui_menu_click_get_hover_choice(&hover_choice)
        && hover_choice == choice)
    {
        return true;
    }
    return g_sdl_character_sheet_screen.hover_choice == choice;
}

void sdl_char_sheet_draw_focus_rect(SDL_FRect rect, bool strong)
{
    SDL_Color color = g_state.palette[strong ? TERM_WHITE : TERM_L_WHITE];

    if (!g_state.renderer || rect.w <= 0.0f || rect.h <= 0.0f)
        return;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g, color.b,
        strong ? 255 : 210);
    SDL_RenderFillRect(g_state.renderer, &rect);
}

byte sdl_char_sheet_focus_text_attr(byte attr, bool focused)
{
    return focused ? TERM_DARK : attr;
}

void sdl_char_sheet_format_tenths(char* buf, size_t buflen,
    long tenths)
{
    if (!buf || buflen == 0)
        return;
    if (tenths < 0)
        tenths = 0;
    strnfmt(buf, buflen, "%ld.%ld", tenths / 10L, tenths % 10L);
}

byte sdl_char_sheet_format_deep_call(char* buf, size_t buflen)
{
    int base_increment = 0;
    int total_increment = 0;
    int effective_total;

    if (!buf || buflen == 0)
        return TERM_L_GREEN;

    min_depth_timer_status(&base_increment, NULL, &total_increment, NULL,
        NULL);
    effective_total = MAX(0, total_increment);

    if (base_increment > 0)
    {
        long pct = ((long)effective_total * 100L + (base_increment / 2))
            / base_increment;
        if (pct > 999L)
            pct = 999L;
        strnfmt(buf, buflen, "%ld%%", pct);
    }
    else if (effective_total > 0)
        SDL_strlcpy(buf, "INF%", buflen);
    else
        SDL_strlcpy(buf, "0%", buflen);

    if (base_increment <= 0)
        return (effective_total > 0) ? TERM_L_GREEN : TERM_YELLOW;
    if (effective_total > base_increment)
        return TERM_L_GREEN;
    if (effective_total == base_increment)
        return TERM_L_BLUE;
    if (effective_total > 0)
        return TERM_YELLOW;
    return TERM_L_RED;
}

float sdl_char_sheet_min_depth_progress(void)
{
    int progress = 0;
    int threshold = 1;

    min_depth_timer_status(NULL, NULL, NULL, &progress, &threshold);
    if (threshold < 1)
        threshold = 1;
    progress = sdl_char_sheet_clampi(progress, 0, threshold);
    return (float)progress / (float)threshold;
}

void sdl_char_sheet_song_name(byte song, char* out, size_t outsz)
{
    int ability_idx;
    cptr name;

    if (!out || outsz == 0)
        return;
    out[0] = '\0';

    if (song == SNG_NOTHING)
        return;

    ability_idx = ability_index(S_SNG, song);
    if (ability_idx < 0)
        return;

    name = b_name + b_info[ability_idx].name;
    if (prefix(name, "Song of "))
        name += 8;
    SDL_strlcpy(out, name, outsz);
}

static bool sdl_char_sheet_birth_assignment_context(void)
{
    return g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_BIRTH_STATS
        || g_sdl_character_sheet_screen.context
            == SDL_CHARACTER_SHEET_BIRTH_SKILLS;
}

static int sdl_char_sheet_trait_info_choice(int line_index, cptr desc)
{
    if (!desc || !desc[0] || !sdl_char_sheet_birth_assignment_context())
        return -1;
    return SDL_CHAR_SHEET_BIRTH_TRAIT_INFO_BASE + line_index;
}

static void sdl_char_sheet_add_trait_line(sdl_char_sheet_line* lines,
    int* count, int max_count, cptr text, byte attr, int skill,
    int trait_score, bool proficiency, u32b aff_flag, u32b pen_flag,
    cptr desc_label)
{
    const sdl_character_sheet_live_item* item;
    char desc[256];
    cptr item_desc = "";
    int choice;

    if (!lines || !count || !text || !text[0])
        return;

    item = sdl_char_sheet_live_label_item(text);
    if (item && item->desc[0])
        item_desc = item->desc;
    else
    {
        cptr named_desc = "";

        if (desc_label && desc_label[0])
            named_desc = character_sheet_trait_description(desc_label);
        else if (skill < 0)
            named_desc = character_sheet_trait_description(text);

        character_sheet_format_trait_description(text, skill, trait_score,
            proficiency, aff_flag, pen_flag, named_desc, desc, sizeof(desc));
        item_desc = desc;
    }

    choice = item ? item->choice
                  : sdl_char_sheet_trait_info_choice(*count, item_desc);
    sdl_char_sheet_add_line(lines, count, max_count, text, attr, choice,
        item_desc);
}

void sdl_char_sheet_add_line(sdl_char_sheet_line* lines, int* count,
    int max_count, cptr text, byte attr, int choice, cptr desc)
{
    sdl_char_sheet_line* line;

    if (!lines || !count || *count >= max_count || !text || !text[0])
        return;

    line = &lines[*count];
    SDL_zero(*line);
    SDL_strlcpy(line->text, text, sizeof(line->text));
    line->attr = attr;
    line->choice = choice;
    SDL_strlcpy(line->desc, desc ? desc : "", sizeof(line->desc));
    (*count)++;
}

int sdl_char_sheet_collect_vitals(sdl_char_sheet_line* lines,
    int max_count)
{
    int count = 0;
    char cur[64];
    char rhs[64];
    char value[160];
    char label_value[192];
    char vital_desc[256];
    int vital_choice;
    const sdl_character_sheet_live_item* item;

    if (!p_ptr)
        return 0;

#define ADD_VITAL(LABEL, VALUE, ATTR)                                           \
    do {                                                                        \
        item = sdl_char_sheet_live_label_item((LABEL));                         \
        vital_desc[0] = '\0';                                                   \
        if (item && item->desc[0])                                              \
            SDL_strlcpy(vital_desc, item->desc, sizeof(vital_desc));            \
        else if (sdl_char_sheet_birth_assignment_context())                     \
            character_sheet_format_vital_description((LABEL), vital_desc,      \
                sizeof(vital_desc));                                            \
        vital_choice = item ? item->choice                                      \
            : ((vital_desc[0] && sdl_char_sheet_birth_assignment_context())     \
                ? SDL_CHAR_SHEET_BIRTH_VITAL_INFO_BASE + count : -1);           \
        strnfmt(label_value, sizeof(label_value), "%s\t%s", (LABEL), (VALUE));  \
        sdl_char_sheet_add_line(lines, &count, max_count, label_value, (ATTR),  \
            vital_choice, vital_desc);                                          \
    } while (0)

    strnfmt(value, sizeof(value), "%ld / %ld", (long)p_ptr->new_exp,
        (long)p_ptr->exp);
    ADD_VITAL("Exp", value, TERM_L_GREEN);

    sdl_char_sheet_format_tenths(cur, sizeof(cur), (long)p_ptr->total_weight);
    sdl_char_sheet_format_tenths(rhs, sizeof(rhs), (long)weight_limit());
    strnfmt(value, sizeof(value), "%s / %s", cur, rhs);
    ADD_VITAL("Burden", value,
        (p_ptr->total_weight <= weight_limit()) ? TERM_L_GREEN
                                                : TERM_YELLOW);

    if (turn > 0)
    {
        long cur_d = (long)(p_ptr->depth * 50);
        long min_d = (long)(min_depth() * 50);

        cur_d = MIN(cur_d, 1000L);
        min_d = MIN(min_d, 1000L);
        strnfmt(value, sizeof(value), "%ld / %ld", cur_d, min_d);
        ADD_VITAL("Depth c/m", value,
            (cur_d >= min_d) ? TERM_L_GREEN : TERM_YELLOW);

        item = sdl_char_sheet_live_label_item("Minimum depth progress");
        vital_desc[0] = '\0';
        if (item && item->desc[0])
            SDL_strlcpy(vital_desc, item->desc, sizeof(vital_desc));
        else if (sdl_char_sheet_birth_assignment_context())
            character_sheet_format_vital_description("Depth timer",
                vital_desc, sizeof(vital_desc));
        vital_choice = item ? item->choice
            : ((vital_desc[0] && sdl_char_sheet_birth_assignment_context())
                ? SDL_CHAR_SHEET_BIRTH_VITAL_INFO_BASE + count : -1);
        SDL_strlcpy(label_value, "Depth timer\t", sizeof(label_value));
        sdl_char_sheet_add_line(lines, &count, max_count, label_value,
            TERM_L_BLUE, vital_choice, vital_desc);
    }

    {
        byte attr = sdl_char_sheet_format_deep_call(value, sizeof(value));
        ADD_VITAL("Deep Call", value, attr);
    }

    comma_number(value, playerturn);
    ADD_VITAL("Turn", value, TERM_L_GREEN);

    strnfmt(value, sizeof(value), "%d", p_ptr->cur_light);
    ADD_VITAL("Light", value, TERM_L_GREEN);

    strnfmt(value, sizeof(value), "(%+d, %dd%d)", p_ptr->skill_use[S_MEL],
        p_ptr->mdd, p_ptr->mds);
    ADD_VITAL("Melee", value, TERM_L_BLUE);

    if (p_ptr->active_ability[S_MEL][MEL_RAPID_ATTACK])
        ADD_VITAL("Melee x2", value, TERM_L_BLUE);

    if (p_ptr->mds2 > 0)
    {
        strnfmt(value, sizeof(value), "(%+d, %dd%d)",
            p_ptr->skill_use[S_MEL] + p_ptr->offhand_mel_mod, p_ptr->mdd2,
            p_ptr->mds2);
        ADD_VITAL("Offhand", value, TERM_L_BLUE);
    }

    strnfmt(value, sizeof(value), "(%+d, %dd%d)", p_ptr->skill_use[S_ARC],
        p_ptr->add, p_ptr->ads);
    ADD_VITAL("Bows", value, TERM_L_BLUE);

    strnfmt(value, sizeof(value), "[%+d, %d-%d]", p_ptr->skill_use[S_EVN],
        p_min(GF_HURT, true), p_max(GF_HURT, true));
    ADD_VITAL("Armor", value, TERM_L_BLUE);

    strnfmt(value, sizeof(value), "%d / %d", p_ptr->chp, p_ptr->mhp);
    ADD_VITAL("Health", value, TERM_L_BLUE);

    strnfmt(value, sizeof(value), "%d / %d", p_ptr->csp, p_ptr->msp);
    ADD_VITAL("Voice", value, TERM_L_BLUE);

    if (p_ptr->song1 != SNG_NOTHING)
    {
        sdl_char_sheet_song_name(p_ptr->song1, value, sizeof(value));
        ADD_VITAL("Song", value, TERM_L_BLUE);
    }
    if (p_ptr->song2 != SNG_NOTHING)
    {
        sdl_char_sheet_song_name(p_ptr->song2, value, sizeof(value));
        ADD_VITAL("Theme", value, TERM_L_BLUE);
    }

#undef ADD_VITAL

    return count;
}

void sdl_char_sheet_collect_skill_trait(sdl_char_sheet_line* lines,
    int* count, int max_count, cptr label, int skill, int score,
    bool proficiency, u32b aff_flag, u32b pen_flag)
{
    char text[64];
    byte attr;

    if (score == 0)
        return;
    score = sdl_char_sheet_clampi(score, -2, 2);

    if (score == 2)
    {
        strnfmt(text, sizeof(text), "%s++", label);
        attr = TERM_L_GREEN;
    }
    else if (score == 1)
    {
        strnfmt(text, sizeof(text), "%s+", label);
        attr = TERM_GREEN;
    }
    else if (score == -1)
    {
        strnfmt(text, sizeof(text), "%s-", label);
        attr = TERM_RED;
    }
    else
    {
        strnfmt(text, sizeof(text), "%s--", label);
        attr = TERM_L_RED;
    }

    sdl_char_sheet_add_trait_line(lines, count, max_count, text, attr, skill,
        score, proficiency, aff_flag, pen_flag, NULL);
}

int sdl_char_sheet_collect_traits(sdl_char_sheet_line* lines,
    int max_count)
{
    int count = 0;
    int race;
    int character;

    if (!p_ptr || !p_info || !c_info)
        return 0;

    race = p_ptr->prace;
    character = p_ptr->pcharacter;

#define ADD_UNIQUE(LABEL, FLAG, ATTR)                                           \
    do {                                                                        \
        if ((p_info[race].flags & (FLAG)) || (c_info[character].flags & (FLAG)))\
        {                                                                       \
            sdl_char_sheet_add_trait_line(lines, &count, max_count, (LABEL),    \
                (ATTR), -1, 0, false, 0, 0, (LABEL));                           \
        }                                                                       \
    } while (0)

#define ADD_UNIQUE_U(LABEL, FLAG, ATTR)                                         \
    do {                                                                        \
        if (c_info[character].flags_u & (FLAG))                                 \
        {                                                                       \
            sdl_char_sheet_add_trait_line(lines, &count, max_count, (LABEL),    \
                (ATTR), -1, 0, false, 0, 0, (LABEL));                           \
        }                                                                       \
    } while (0)

#define ADD_SKILL_TRAIT(LABEL, SKILL, AFF, PEN, PROFICIENCY)                    \
    do {                                                                        \
        int score = 0;                                                          \
        if (p_info[race].flags & (AFF)) score++;                                \
        if (c_info[character].flags & (AFF)) score++;                           \
        if ((PEN) && (p_info[race].flags & (PEN))) score--;                     \
        if ((PEN) && (c_info[character].flags & (PEN))) score--;                \
        score += curse_flag_count_rhf(AFF);                                     \
        if (PEN) score -= curse_flag_count_rhf(PEN);                            \
        sdl_char_sheet_collect_skill_trait(lines, &count, max_count, (LABEL),   \
            (SKILL), score, (PROFICIENCY), (AFF), (PEN));                       \
    } while (0)

    ADD_UNIQUE_U("Master Artisan", UNQ_SMT_FEANOR, TERM_VIOLET);
    ADD_UNIQUE_U("Creator of Galvorn", UNQ_SMT_EOL, TERM_VIOLET);
    ADD_UNIQUE_U("Chosen of Ulmo", UNQ_WIL_TUOR, TERM_VIOLET);
    ADD_UNIQUE_U("Indomitable Will", UNQ_EARENDIL, TERM_VIOLET);
    ADD_UNIQUE_U("Himself", UNQ_WIL_FIN, TERM_VIOLET);
    ADD_UNIQUE_U("Songs of Power", UNQ_SNG_FIN, TERM_VIOLET);
    ADD_UNIQUE_U("Elven Dance", UNQ_SNG_LUT, TERM_VIOLET);
    ADD_UNIQUE_U("Girdle of Melian", UNQ_SNG_MEL, TERM_VIOLET);
    ADD_UNIQUE_U("Creator of Angrist", UNQ_SMT_TELCHAR, TERM_VIOLET);
    ADD_UNIQUE_U("Old Master", UNQ_SMT_GAMIL, TERM_VIOLET);
    ADD_UNIQUE_U("Ring Master", UNQ_SMT_CELEBRIMBOR, TERM_VIOLET);
    ADD_UNIQUE_U("Aure entuluva", UNQ_SNG_HURIN, TERM_VIOLET);
    ADD_UNIQUE_U("Voice of the Girdle", UNQ_SNG_THINGOL, TERM_VIOLET);
    ADD_UNIQUE_U("Forgotten", UNQ_MIM, TERM_VIOLET);
    ADD_UNIQUE_U("One Handed", UNQ_MEL_MAEDHROS, TERM_VIOLET);
    ADD_UNIQUE_U("Agarwaen", UNQ_WIL_TURIN, TERM_VIOLET);
    ADD_UNIQUE_U("Shadow Walker", UNQ_SNG_TURGON, TERM_VIOLET);
    ADD_UNIQUE_U("Minstrel", UNQ_MINSTREL, TERM_VIOLET);
    ADD_UNIQUE_U("Woven Master", UNQ_WOVEN_MASTER, TERM_VIOLET);
    ADD_UNIQUE("Gift of Eru", RHF_GIFTERU, TERM_VIOLET);
    ADD_UNIQUE("Seafarer", RHF_FREE, TERM_VIOLET);
    ADD_UNIQUE("Kinslayer", RHF_KINSLAYER, TERM_UMBER);
    ADD_UNIQUE("Treacherous", RHF_TREACHERY, TERM_UMBER);
    ADD_UNIQUE("Doom of Mandos", RHF_CURSE, TERM_UMBER);
    ADD_UNIQUE("Morgoth Curse", RHF_MOR_CURSE, TERM_UMBER);

    ADD_SKILL_TRAIT("melee", S_MEL, RHF_MEL_AFFINITY, RHF_MEL_PENALTY,
        false);
    ADD_SKILL_TRAIT("evasion", S_EVN, RHF_EVN_AFFINITY, RHF_EVN_PENALTY,
        false);
    ADD_SKILL_TRAIT("stealth", S_STL, RHF_STL_AFFINITY, RHF_STL_PENALTY,
        false);
    ADD_SKILL_TRAIT("archery", S_ARC, RHF_ARC_AFFINITY, RHF_ARC_PENALTY,
        false);
    ADD_SKILL_TRAIT("will", S_WIL, RHF_WIL_AFFINITY, RHF_WIL_PENALTY,
        false);
    ADD_SKILL_TRAIT("perception", S_PER, RHF_PER_AFFINITY, RHF_PER_PENALTY,
        false);
    ADD_SKILL_TRAIT("smithing", S_SMT, RHF_SMT_AFFINITY, RHF_SMT_PENALTY,
        false);
    ADD_SKILL_TRAIT("song", S_SNG, RHF_SNG_AFFINITY, RHF_SNG_PENALTY,
        false);
    ADD_SKILL_TRAIT("bow", S_ARC, RHF_BOW_PROFICIENCY, 0, true);
    ADD_SKILL_TRAIT("axe", S_MEL, RHF_AXE_PROFICIENCY, 0, true);

#undef ADD_SKILL_TRAIT
#undef ADD_UNIQUE_U
#undef ADD_UNIQUE

    return count;
}

void sdl_char_sheet_title(char* out, size_t outsz)
{
    cptr alt_name = "";

    if (!out || outsz == 0)
        return;

    out[0] = '\0';
    if (!p_ptr || !op_ptr)
        return;

    if (current_character_profile)
        alt_name = c_name + current_character_profile->alt_name;
    else if (c_info)
        alt_name = c_name + c_info[p_ptr->pcharacter].alt_name;

    if (p_ptr->oaths_broken)
        strnfmt(out, outsz, "%s the Oathbreaker", op_ptr->full_name);
    else
        strnfmt(out, outsz, "%s%s", op_ptr->full_name, alt_name);
}

void sdl_char_sheet_draw_heading(TTF_Font* font, cptr heading,
    float x, float y, float w, float line_h)
{
    (void)sdl_char_sheet_draw_text(font, heading, TERM_SLATE, x, y, w,
        line_h * 0.92f, false);
}

/*
 * Draw a skill's "total = base mods..." breakdown with per-segment colour to
 * match the text and birth skill tables: the total in bright green, the base
 * ability in green, and the "=" plus every modifier column in slate.
 */
static void sdl_char_sheet_draw_skill_value(TTF_Font* font, cptr value,
    bool focused, float x, float y, float w, float h)
{
    int full_w = sdl_char_sheet_text_width(font, value);
    int space_w = sdl_char_sheet_text_width(font, " ");
    float scale = (full_w > 0 && (float)full_w > w) ? w / (float)full_w : 1.0f;
    float cursor = x;
    const char* p = value;
    int idx = 0;

    while (*p)
    {
        char tok[32];
        size_t len = 0;
        byte attr;
        int tok_w;

        while (*p == ' ')
            p++;
        if (!*p)
            break;
        while (*p && *p != ' ' && len < sizeof(tok) - 1)
            tok[len++] = *p++;
        tok[len] = '\0';

        if (idx == 0)
            attr = TERM_L_GREEN;          /* total skill */
        else if (streq(tok, "="))
            attr = TERM_SLATE;
        else if (idx == 2)
            attr = TERM_GREEN;            /* base ability */
        else
            attr = TERM_SLATE;            /* stat / equip / misc modifiers */

        tok_w = sdl_char_sheet_text_width(font, tok);
        (void)sdl_char_sheet_draw_text(font, tok,
            sdl_char_sheet_focus_text_attr(attr, focused), cursor, y,
            (float)tok_w * scale + 1.0f, h, false);
        cursor += (float)tok_w * scale + (float)space_w * scale;
        idx++;
    }
}

void sdl_char_sheet_draw_labeled_line(TTF_Font* font, cptr text,
    byte attr, int choice, cptr desc, float x, float y, float w, float line_h,
    float label_fraction)
{
    const char* tab;
    char label[96];
    char value[160];
    SDL_FRect hit_rect = { x, y, 0.0f, line_h };
    bool depth_timer = false;
    /* Only pressable rows get the highlight; the hit (and thus the hover
     * tooltip) is still registered for informational rows below. */
    bool focused = sdl_char_sheet_choice_focused(choice)
        && sdl_char_sheet_choice_pressable(choice);

    if (!text || !text[0])
        return;

    tab = strchr(text, '\t');
    if (tab)
    {
        size_t label_len = (size_t)(tab - text);
        if (label_len >= sizeof(label))
            label_len = sizeof(label) - 1;
        memcpy(label, text, label_len);
        label[label_len] = '\0';
        SDL_strlcpy(value, tab + 1, sizeof(value));
    }
    else
    {
        SDL_strlcpy(label, text, sizeof(label));
        value[0] = '\0';
    }

    depth_timer = streq(label, "Depth timer");

    if (value[0] || depth_timer)
    {
        float label_w = w * label_fraction;
        int label_text_w = sdl_char_sheet_text_width(font, label);
        int value_text_w = sdl_char_sheet_text_width(font, value);

        hit_rect.w = MIN(w, label_w + (float)value_text_w + 8.0f);
        if (hit_rect.w < (float)label_text_w + 8.0f)
            hit_rect.w = MIN(w, (float)label_text_w + 8.0f);
        if (depth_timer)
            hit_rect.w = w;

        if (focused)
            sdl_char_sheet_draw_focus_rect(hit_rect, true);
        if (choice >= 0)
            sdl_char_sheet_add_hit(hit_rect, choice, desc, attr);

        (void)sdl_char_sheet_draw_text(font, label,
            sdl_char_sheet_focus_text_attr(
                depth_timer ? attr : TERM_WHITE, focused), x, y,
            label_w, line_h * 0.92f, false);
        if (depth_timer)
        {
            float bar_h = line_h * 0.42f;
            SDL_FRect bg = { x + label_w, y + (line_h - bar_h) * 0.46f,
                w - label_w, bar_h };
            SDL_FRect fg = bg;
            SDL_Color color = g_state.palette[TERM_L_BLUE];

            fg.w *= sdl_char_sheet_min_depth_progress();
            SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(g_state.renderer, 24, 34, 34, 220);
            SDL_RenderFillRect(g_state.renderer, &bg);
            SDL_SetRenderDrawColor(g_state.renderer, color.r, color.g,
                color.b, 235);
            SDL_RenderFillRect(g_state.renderer, &fg);
        }
        else
        {
            /* Numeric/value column renders in the secondary story font. */
            TTF_Font* value_font = sdl_story_font_slot_sibling(font,
                SDL_STORY_FONT_SLOT_CHAR_NUM);

            /* Skill rows carry a "total = base mods" breakdown; colour each
             * column instead of painting the whole value one shade. */
            if (strstr(value, " = "))
                sdl_char_sheet_draw_skill_value(value_font, value, focused,
                    x + label_w, y, w - label_w, line_h * 0.92f);
            else
                (void)sdl_char_sheet_draw_text(value_font, value,
                    sdl_char_sheet_focus_text_attr(attr, focused), x + label_w,
                    y, w - label_w, line_h * 0.92f, false);
        }
    }
    else
    {
        int label_text_w = sdl_char_sheet_text_width(font, label);

        hit_rect.w = MIN(w, (float)label_text_w + 8.0f);
        if (focused)
            sdl_char_sheet_draw_focus_rect(hit_rect, true);
        if (choice >= 0)
            sdl_char_sheet_add_hit(hit_rect, choice, desc, attr);

        (void)sdl_char_sheet_draw_text(font, label,
            sdl_char_sheet_focus_text_attr(attr, focused), x, y, w,
            line_h * 0.92f, false);
    }
}

void sdl_char_sheet_draw_lines(TTF_Font* font, cptr heading,
    const sdl_char_sheet_line* lines, int count, float x, float y, float w,
    float h, float line_h, float label_fraction)
{
    float row_y = y + line_h;

    sdl_char_sheet_draw_heading(font, heading, x, y, w, line_h);
    for (int i = 0; i < count && row_y + line_h * 0.2f <= y + h; i++)
    {
        sdl_char_sheet_draw_labeled_line(font, lines[i].text, lines[i].attr,
            lines[i].choice, lines[i].desc, x, row_y, w, line_h,
            label_fraction);
        row_y += line_h;
    }
}

void sdl_char_sheet_draw_select_stats(TTF_Font* font, cptr heading,
    const sdl_char_sheet_line* lines, int count,
    const sdl_char_sheet_line* abilities, int ability_count,
    bool show_abilities, float x, float y, float w, float h, float line_h,
    float label_fraction)
{
    float row_y = y + line_h;

    sdl_char_sheet_draw_heading(font, heading, x, y, w, line_h);
    for (int i = 0; i < count && row_y + line_h * 0.2f <= y + h; i++)
    {
        sdl_char_sheet_draw_labeled_line(font, lines[i].text, lines[i].attr,
            lines[i].choice, lines[i].desc, x, row_y, w, line_h,
            label_fraction);
        row_y += line_h;
    }

    if (show_abilities)
    {
        if (row_y + line_h * 0.2f > y + h)
            return;

        sdl_char_sheet_draw_heading(font, "Abilities", x, row_y, w, line_h);
        row_y += line_h;
        for (int i = 0;
             i < ability_count && row_y + line_h * 0.2f <= y + h; i++)
        {
            sdl_char_sheet_draw_labeled_line(font, abilities[i].text,
                abilities[i].attr, abilities[i].choice, abilities[i].desc, x,
                row_y, w, line_h, label_fraction);
            row_y += line_h;
        }
    }

    if (g_sdl_character_sheet_screen.select_rating_count <= 0)
        return;

    {
        int small_px = sdl_char_sheet_clampi((int)(line_h * 0.72f), 12, 52);
        TTF_Font* small_font = sdl_story_font_for_height(small_px);
        float small_h = sdl_char_sheet_line_h(small_font, small_px, 1.05f);
        float gap = small_h * 0.55f;
        cptr title = g_sdl_character_sheet_screen.select_rating_title[0]
            ? g_sdl_character_sheet_screen.select_rating_title : "Heroes Power";

        row_y += gap;
        if (row_y + small_h * 0.2f > y + h)
            return;

        (void)sdl_char_sheet_draw_text(small_font, title, TERM_SLATE,
            x, row_y, w, small_h * 0.94f, false);
        row_y += small_h * 0.95f;

        for (int i = 0;
             i < g_sdl_character_sheet_screen.select_rating_count
                 && row_y + small_h * 0.2f <= y + h;
             i++)
        {
            const sdl_character_sheet_select_rating* r =
                &g_sdl_character_sheet_screen.select_ratings[i];
            char count_text[16];
            float group_w;
            float stars_w;
            float spacing = MAX(4.0f, small_h * 0.22f);
            float cursor_x = x;
            SDL_FRect hit;

            strnfmt(count_text, sizeof(count_text), "%d", r->count);
            group_w = (float)sdl_char_sheet_text_width(small_font, r->group);
            stars_w = (float)sdl_char_sheet_text_width(small_font, r->stars);

            (void)sdl_char_sheet_draw_text(small_font, r->group, TERM_WHITE,
                cursor_x, row_y, w, small_h * 0.94f, false);
            cursor_x += group_w + spacing;
            (void)sdl_char_sheet_draw_text(small_font, r->stars, r->attr,
                cursor_x, row_y, w - (cursor_x - x), small_h * 0.94f, false);
            cursor_x += stars_w + spacing;
            (void)sdl_char_sheet_draw_text(small_font, count_text, TERM_WHITE,
                cursor_x, row_y, w - (cursor_x - x), small_h * 0.94f, false);

            hit.x = x;
            hit.y = row_y;
            hit.w = MIN(w, cursor_x - x
                + (float)sdl_char_sheet_text_width(small_font, count_text)
                + 8.0f);
            hit.h = small_h;
            if (r->desc[0])
                sdl_char_sheet_add_hit(hit, 9100 + i, r->desc, r->attr);

            row_y += small_h * 0.95f;
        }
    }
}

void sdl_char_sheet_draw_traits(TTF_Font* font,
    const sdl_char_sheet_line* lines, int count, float x, float y, float w,
    float h, float line_h, int columns)
{
    int rows_per_col;
    float gap;
    float col_w;

    if (columns < 1)
        columns = 1;
    if (columns > 2)
        columns = 2;

    rows_per_col = (count + columns - 1) / columns;
    if (rows_per_col < 1)
        rows_per_col = 1;
    gap = (columns > 1) ? MIN(w * 0.08f, 28.0f) : 0.0f;
    col_w = (w - gap * (float)(columns - 1)) / (float)columns;

    sdl_char_sheet_draw_heading(font, "Traits", x, y, w, line_h);
    if (count <= 0)
    {
        (void)sdl_char_sheet_draw_text(font, "None", TERM_SLATE, x,
            y + line_h, w, line_h * 0.92f, false);
        return;
    }

    for (int i = 0; i < count; i++)
    {
        int col = i / rows_per_col;
        int row = i % rows_per_col;
        float row_x = x + (float)col * (col_w + gap);
        float row_y = y + line_h + (float)row * line_h;

        if (row_y + line_h * 0.2f > y + h)
            continue;

        sdl_char_sheet_draw_labeled_line(font, lines[i].text, lines[i].attr,
            lines[i].choice, lines[i].desc, row_x, row_y, col_w, line_h,
            0.58f);
    }
}

SDL_FRect sdl_char_sheet_alloc_rect(float x, float y, float w,
    float line_h, int row, int col, int span)
{
    const float cols = 38.0f;
    SDL_FRect rect = { 0 };
    float cell_w;

    if (w <= 0.0f || line_h <= 0.0f || span <= 0)
        return rect;

    if (col < 0)
    {
        span += col;
        col = 0;
    }
    if (col >= (int)cols || span <= 0)
        return rect;
    if (col + span > (int)cols)
        span = (int)cols - col;

    cell_w = w / cols;
    rect.x = x + (float)col * cell_w;
    rect.y = y + (float)row * line_h;
    rect.w = (float)span * cell_w;
    rect.h = line_h;
    return rect;
}

void sdl_char_sheet_alloc_text(TTF_Font* font, float x, float y,
    float w, float line_h, int row, int col, int span, byte attr, cptr text,
    bool focused)
{
    SDL_FRect rect;

    if (!text || !text[0])
        return;

    rect = sdl_char_sheet_alloc_rect(x, y, w, line_h, row, col, span);
    if (rect.w <= 0.0f || rect.h <= 0.0f)
        return;

    (void)sdl_char_sheet_draw_text(font, text,
        sdl_char_sheet_focus_text_attr(attr, focused), rect.x, rect.y,
        rect.w, rect.h * 0.92f, false);
}

bool sdl_char_sheet_alloc_row_visible(float y, float h, float line_h,
    int row)
{
    float row_y = y + (float)row * line_h;

    return row_y + line_h * 0.2f <= y + h;
}

void sdl_char_sheet_copy_trimmed(cptr src, char* dst, size_t dstsz)
{
    size_t len;

    if (!dst || dstsz == 0)
        return;

    SDL_strlcpy(dst, src ? src : "", dstsz);
    len = strlen(dst);
    while (len > 0 && dst[len - 1] == ' ')
        dst[--len] = '\0';
}

void sdl_char_sheet_draw_birth_stat_table_row(TTF_Font* font,
    float x, float y, float w, float h, float line_h, int row, int stat,
    bool allocation)
{
    char label[32];
    char value[32];
    char cost[32];
    char desc[384];
    char hint[192];
    bool focused = false;
    SDL_FRect row_rect;

    if (stat < 0 || stat >= A_MAX
        || !sdl_char_sheet_alloc_row_visible(y, h, line_h, row))
    {
        return;
    }

    focused = allocation && sdl_char_sheet_choice_focused(stat);
    row_rect = sdl_char_sheet_alloc_rect(x, y, w, line_h, row, 0, 38);
    if (focused)
        sdl_char_sheet_draw_focus_rect(row_rect, true);

    sdl_char_sheet_copy_trimmed((p_ptr && p_ptr->stat_drain[stat] < 0)
            ? stat_names_reduced[stat] : stat_names[stat],
        label, sizeof(label));
    cnv_stat(p_ptr ? p_ptr->stat_use[stat]
                   : g_sdl_character_sheet_screen.stat_values[stat],
        value);

    sdl_char_sheet_alloc_text(font, x, y, w, line_h, row, 0, 5,
        TERM_WHITE, label, focused);
    sdl_char_sheet_alloc_text(font, x, y, w, line_h, row, 6, 6,
        (p_ptr && p_ptr->stat_drain[stat] < 0) ? TERM_YELLOW
                                               : TERM_L_GREEN,
        value, focused);

    if (allocation)
    {
        strnfmt(cost, sizeof(cost), "%4d",
            g_sdl_character_sheet_screen.stat_costs[stat]);
        sdl_char_sheet_alloc_text(font, x, y, w, line_h, row, 33, 4,
            TERM_L_WHITE, cost, focused);
    }

    hint[0] = '\0';
    character_sheet_format_stat_hint(stat, 0, false, hint, sizeof(hint));
    if (allocation)
    {
#if SIL_SDL_MOBILE_BUILD
        if (sdl_touch_only_device_active())
        {
            strnfmt(desc, sizeof(desc),
                "%s Cost to raise now: %d. Tap to select; tap the selected row to increase, long tap the selected row to decrease.",
                hint, g_sdl_character_sheet_screen.stat_costs[stat]);
        }
        else
#endif
        strnfmt(desc, sizeof(desc),
            "%s Cost to raise now: %d. Click/tap to select; click/tap the selected row to increase, right-click to decrease.",
            hint, g_sdl_character_sheet_screen.stat_costs[stat]);
        sdl_char_sheet_add_hit(row_rect, stat, desc, TERM_L_BLUE);
    }
    else
    {
        sdl_char_sheet_add_hit(row_rect,
            SDL_CHAR_SHEET_BIRTH_STAT_INFO_BASE + stat, hint, TERM_L_BLUE);
    }
}

void sdl_char_sheet_draw_birth_skill_table_row(TTF_Font* font,
    float x, float y, float w, float h, float line_h, int row, int skill,
    bool allocation)
{
    char buf[32];
    char desc[384];
    cptr hint;
    int increase_cost;
    bool focused;
    SDL_FRect row_rect;

    if (skill < 0 || skill >= S_MAX || skill == S_SPC
        || !sdl_char_sheet_alloc_row_visible(y, h, line_h, row))
    {
        return;
    }

    focused = allocation && sdl_char_sheet_choice_focused(skill);
    row_rect = sdl_char_sheet_alloc_rect(x, y, w, line_h, row, 0, 38);
    if (focused)
        sdl_char_sheet_draw_focus_rect(row_rect, true);

    sdl_char_sheet_alloc_text(font, x, y, w, line_h, row, 0, 10,
        TERM_WHITE, skill_names_full[skill], focused);

    strnfmt(buf, sizeof(buf), "%3d", p_ptr ? p_ptr->skill_use[skill] : 0);
    sdl_char_sheet_alloc_text(font, x, y, w, line_h, row, 11, 3,
        TERM_L_GREEN, buf, focused);
    sdl_char_sheet_alloc_text(font, x, y, w, line_h, row, 15, 1,
        TERM_SLATE, "=", focused);
    strnfmt(buf, sizeof(buf), "%2d", p_ptr ? p_ptr->skill_base[skill] : 0);
    sdl_char_sheet_alloc_text(font, x, y, w, line_h, row, 17, 2,
        TERM_GREEN, buf, focused);

    if (p_ptr && p_ptr->skill_stat_mod[skill] != 0)
    {
        strnfmt(buf, sizeof(buf), "%+3d", p_ptr->skill_stat_mod[skill]);
        sdl_char_sheet_alloc_text(font, x, y, w, line_h, row, 20, 3,
            TERM_SLATE, buf, focused);
    }
    if (p_ptr && p_ptr->skill_equip_mod[skill] != 0)
    {
        strnfmt(buf, sizeof(buf), "%+3d", p_ptr->skill_equip_mod[skill]);
        sdl_char_sheet_alloc_text(font, x, y, w, line_h, row, 24, 3,
            TERM_SLATE, buf, focused);
    }
    if (p_ptr && p_ptr->skill_misc_mod[skill] != 0)
    {
        strnfmt(buf, sizeof(buf), "%+3d", p_ptr->skill_misc_mod[skill]);
        sdl_char_sheet_alloc_text(font, x, y, w, line_h, row, 28, 3,
            TERM_SLATE, buf, focused);
    }

    if (allocation)
    {
        strnfmt(buf, sizeof(buf), "%6d",
            g_sdl_character_sheet_screen.skill_costs[skill]);
        sdl_char_sheet_alloc_text(font, x, y, w, line_h, row, 31, 6,
            TERM_L_WHITE, buf, focused);
    }

    hint = character_sheet_skill_description(skill);
    if (!hint || !hint[0])
        hint = "Skill total and starting base value.";
    if (allocation)
    {
        /* The table shows the cumulative cost of the gains selected so far.
         * The popup should instead preview the cost of the next point. */
        increase_cost = (g_sdl_character_sheet_screen.skill_old_base[skill]
            + g_sdl_character_sheet_screen.skill_gain[skill] + 1) * 100;
#if SIL_SDL_MOBILE_BUILD
        if (sdl_touch_only_device_active())
        {
            strnfmt(desc, sizeof(desc),
                "%s: %s Cost to raise now: %d. Tap to select; tap the selected row to increase, long tap the selected row to decrease.",
                skill_names_full[skill], hint, increase_cost);
        }
        else
#endif
        strnfmt(desc, sizeof(desc),
            "%s: %s Cost to raise now: %d. Click/tap to select; click/tap the selected row to increase, right-click to decrease.",
            skill_names_full[skill], hint, increase_cost);
        sdl_char_sheet_add_hit(row_rect, skill, desc,
            ability_skill_color(skill));
    }
    else
    {
        strnfmt(desc, sizeof(desc), "%s: %s", skill_names_full[skill],
            hint);
        sdl_char_sheet_add_hit(row_rect,
            SDL_CHAR_SHEET_BIRTH_SKILL_INFO_BASE + skill, desc,
            ability_skill_color(skill));
    }
}

void sdl_char_sheet_draw_birth_status_row(TTF_Font* font, float x,
    float y, float w, float h, float line_h, int row, cptr status)
{
#if !SIL_SDL_MOBILE_BUILD
    SDL_FRect hit;
    bool confirm_focused;
    bool back_focused;
    cptr confirm_text = "[Confirm]";
    cptr back_text = "[Esc]";
    int status_span = 20;
    int confirm_col = 23;
    int confirm_span = 9;
    int back_col = 33;
    int back_span = 5;
#endif

    if (!sdl_char_sheet_alloc_row_visible(y, h, line_h, row))
        return;

#if SIL_SDL_MOBILE_BUILD
    /* Mobile already provides persistent Back and Confirm controls below the
     * screen.  Keep this row available for status text, but do not duplicate
     * those controls or register overlapping in-screen hit targets. */
    sdl_char_sheet_alloc_text(font, x, y, w, line_h, row, 0, 38,
        TERM_L_BLUE, status, false);
#else
    sdl_char_sheet_alloc_text(font, x, y, w, line_h, row, 0, status_span,
        TERM_L_BLUE, status, false);

    confirm_focused = sdl_char_sheet_prompt_focused(-2);
    back_focused = sdl_char_sheet_prompt_focused(-1);

    hit = sdl_char_sheet_alloc_rect(x, y, w, line_h, row, confirm_col,
        confirm_span);
    if (confirm_focused)
        sdl_char_sheet_draw_focus_rect(hit, true);
    sdl_char_sheet_alloc_text(font, x, y, w, line_h, row, confirm_col,
        confirm_span, TERM_SLATE, confirm_text, confirm_focused);
    sdl_char_sheet_add_prompt_hit(hit, -2);

    hit = sdl_char_sheet_alloc_rect(x, y, w, line_h, row, back_col,
        back_span);
    if (back_focused)
        sdl_char_sheet_draw_focus_rect(hit, true);
    sdl_char_sheet_alloc_text(font, x, y, w, line_h, row, back_col,
        back_span, TERM_SLATE, back_text, back_focused);
    sdl_char_sheet_add_prompt_hit(hit, -1);
#endif
}

static void sdl_char_sheet_draw_birth_points_row(TTF_Font* font, float x,
    float y, float w, float h, float line_h, int row)
{
    char status[64];

    if (!sdl_char_sheet_alloc_row_visible(y, h, line_h, row))
        return;

    strnfmt(status, sizeof(status), "Points Left: %d",
        g_sdl_character_sheet_screen.points_left);
    sdl_char_sheet_alloc_text(font, x, y, w, line_h, row, 0, 20,
        TERM_L_BLUE, status, false);
}

void sdl_char_sheet_draw_birth_allocation_area(TTF_Font* font,
    float x, float y, float w, float h, float line_h, bool stats_screen)
{
    int skill_row = 7;
    bool allocate_stats = g_sdl_character_sheet_screen.context
        == SDL_CHARACTER_SHEET_BIRTH_STATS;
    bool allocate_skills = g_sdl_character_sheet_screen.context
        == SDL_CHARACTER_SHEET_BIRTH_SKILLS;

    (void)stats_screen;

    if (!font || w <= 0.0f || h <= 0.0f || line_h <= 0.0f)
        return;

    sdl_char_sheet_alloc_text(font, x, y, w, line_h, 0, 0, 14,
        TERM_SLATE, "Attributes", false);

    for (int stat = 0; stat < A_MAX; stat++)
        sdl_char_sheet_draw_birth_stat_table_row(font, x, y, w, h,
            line_h, 1 + stat, stat, allocate_stats);

    if (allocate_stats)
        sdl_char_sheet_draw_birth_points_row(font, x, y, w, h, line_h, 5);

    sdl_char_sheet_alloc_text(font, x, y, w, line_h, 6, 0, 14,
        TERM_SLATE, "Skills", false);

    for (int skill = 0; skill < S_MAX; skill++)
    {
        if (skill == S_SPC)
            continue;
        sdl_char_sheet_draw_birth_skill_table_row(font, x, y, w, h,
            line_h, skill_row++, skill, allocate_skills);
    }
    if (allocate_skills)
        sdl_char_sheet_draw_birth_points_row(font, x, y, w, h, line_h,
            skill_row);
}

int sdl_char_sheet_collect_stats(sdl_char_sheet_line* lines,
    int max_count)
{
    int count = 0;

    if (!p_ptr)
        return 0;

    for (int stat = 0; stat < A_MAX; stat++)
    {
        char label[32];
        char value[32];
        char text[128];
        int choice;
        cptr desc;
        const sdl_character_sheet_live_item* item;

        SDL_strlcpy(label, (p_ptr->stat_drain[stat] < 0)
                ? stat_names_reduced[stat] : stat_names[stat],
            sizeof(label));
        for (size_t len = strlen(label); len > 0 && label[len - 1] == ' ';
             len = strlen(label))
        {
            label[len - 1] = '\0';
        }
        cnv_stat(p_ptr->stat_use[stat], value);

        item = sdl_char_sheet_live_label_item(label);
        choice = item ? item->choice : -1;
        desc = (item && item->desc[0]) ? item->desc : "";
        if (!desc[0])
        {
            switch (stat)
            {
            case A_STR:
                desc = "Strength: melee damage dice and weight capacity.";
                break;
            case A_DEX:
                desc = "Dexterity: melee, evasion, archery, and stealth.";
                break;
            case A_CON:
                desc = "Constitution: maximum health.";
                break;
            case A_GRA:
                desc = "Grace: will, perception, smithing, song, and voice.";
                break;
            default: break;
            }
        }

        strnfmt(text, sizeof(text), "%s\t%s", label, value);
        sdl_char_sheet_add_line(lines, &count, max_count, text,
            (p_ptr->stat_drain[stat] < 0) ? TERM_YELLOW : TERM_L_GREEN,
            choice, desc);
    }

    return count;
}

int sdl_char_sheet_collect_skills(sdl_char_sheet_line* lines,
    int max_count, bool compact)
{
    int count = 0;

    if (!p_ptr)
        return 0;

    for (int skill = 0; skill < S_MAX; skill++)
    {
        char text[160];
        int choice;
        cptr desc;
        const sdl_character_sheet_live_item* item;

        if (skill == S_SPC)
            continue;

        item = sdl_char_sheet_live_skill_item(skill);
        choice = item ? item->choice : -1;
        /* The hover tooltip already spells out the full breakdown, so on
         * narrow (compact) layouts show only the total to keep the column
         * slim, letting the body font grow. */
        desc = item ? item->desc : character_sheet_skill_description(skill);
        if (compact)
            strnfmt(text, sizeof(text), "%s\t%d", skill_names_full[skill],
                p_ptr->skill_use[skill]);
        else
            strnfmt(text, sizeof(text), "%s\t%d = %d %+d %+d %+d",
                skill_names_full[skill], p_ptr->skill_use[skill],
                p_ptr->skill_base[skill], p_ptr->skill_stat_mod[skill],
                p_ptr->skill_equip_mod[skill], p_ptr->skill_misc_mod[skill]);
        sdl_char_sheet_add_line(lines, &count, max_count, text, TERM_L_GREEN,
            choice, desc);
    }

    return count;
}

void sdl_char_sheet_draw_wrapped(TTF_Font* font, cptr text, byte attr,
    float x, float y, float w, float h, float line_h, int line_count)
{
    char lines[SDL_CHAR_SHEET_MAX_LINES][SDL_CHAR_SHEET_TEXT_LEN];
    float row_y = y;
    int draw_count;
    int output_cap;

    if (!font || !text || !text[0] || h <= 1.0f)
        return;

    output_cap = (line_h > 0.0f) ? (int)(h / line_h) + 1 : 1;
    if (line_count > 0 && output_cap > line_count)
        output_cap = line_count;
    if (output_cap > SDL_CHAR_SHEET_MAX_LINES)
        output_cap = SDL_CHAR_SHEET_MAX_LINES;
    if (output_cap < 1)
        output_cap = 1;
    draw_count = sdl_char_sheet_wrap_text(font, text, w, lines, output_cap);
    if (draw_count > SDL_CHAR_SHEET_MAX_LINES)
        draw_count = SDL_CHAR_SHEET_MAX_LINES;

    for (int i = 0; i < draw_count; i++)
    {
        if (row_y + line_h * 0.2f > y + h)
            break;
        (void)sdl_char_sheet_draw_text(font, lines[i], attr, x, row_y,
            w, line_h * 0.96f, false);
        row_y += line_h;
    }
}

static void sdl_char_sheet_draw_wrapped_alpha(TTF_Font* font, cptr text,
    byte attr, byte alpha, float x, float y, float w, float h,
    float line_h, int line_count)
{
    char lines[SDL_CHAR_SHEET_MAX_LINES][SDL_CHAR_SHEET_TEXT_LEN];
    float row_y = y;
    int draw_count;
    int output_cap;

    if (!font || !text || !text[0] || alpha == 0 || h <= 1.0f)
        return;

    output_cap = (line_h > 0.0f) ? (int)(h / line_h) + 1 : 1;
    if (line_count > 0 && output_cap > line_count)
        output_cap = line_count;
    if (output_cap > SDL_CHAR_SHEET_MAX_LINES)
        output_cap = SDL_CHAR_SHEET_MAX_LINES;
    if (output_cap < 1)
        output_cap = 1;
    draw_count = sdl_char_sheet_wrap_text(font, text, w, lines, output_cap);
    if (draw_count > SDL_CHAR_SHEET_MAX_LINES)
        draw_count = SDL_CHAR_SHEET_MAX_LINES;

    for (int i = 0; i < draw_count; i++)
    {
        if (row_y + line_h * 0.2f > y + h)
            break;
        (void)sdl_char_sheet_draw_text_alpha(font, lines[i], attr, alpha,
            x, row_y, w, line_h * 0.96f);
        row_y += line_h;
    }
}

static float sdl_pause_text_indent_px(TTF_Font* font, int indent,
    int font_px)
{
    int space_w;

    if (indent <= 0)
        return 0.0f;

    space_w = sdl_char_sheet_text_width(font, " ");
    if (space_w < 1)
        space_w = MAX(1, font_px / 4);
    return (float)(indent * space_w);
}

static int sdl_pause_text_wrapped_rows(TTF_Font* font,
    const sdl_pause_text_line* line, float column_w, int font_px)
{
    float indent_px;
    float text_w;
    int rows;

    if (!font || !line)
        return 1;
    if (!line->text[0])
        return 1;

    indent_px = sdl_pause_text_indent_px(font, line->indent, font_px);
    text_w = column_w - indent_px;
    if (text_w < column_w * 0.30f)
        text_w = column_w * 0.30f;
    rows = sdl_char_sheet_wrap_text(font, line->text, text_w, NULL, 0);
    return MAX(1, rows);
}

static int sdl_pause_text_total_rows(TTF_Font* font, float column_w,
    int font_px)
{
    int rows = 0;

    for (int i = 0; i < g_sdl_pause_text_screen.line_count; i++)
    {
        rows += sdl_pause_text_wrapped_rows(font,
            &g_sdl_pause_text_screen.lines[i], column_w, font_px);
    }

    return MAX(1, rows);
}

/* Draw pause_with_text() narratives directly on the window canvas.  Each
 * source line remains a semantic line (colour plus relative indentation),
 * while wrapping, font size, and vertical placement are resolved in pixels.
 * This deliberately avoids terminal-cell clipping for enlarged story fonts. */
static void sdl_pause_text_screen_render_canvas(const SDL_Rect* canvas_override)
{
    const sdl_pause_text_screen_state* screen = &g_sdl_pause_text_screen;
    SDL_Rect canvas;
    SDL_FRect content;
    TTF_Font* font = NULL;
    float column_w;
    float column_x;
    float line_h = 0.0f;
    float top_margin;
    float bottom_margin;
    float available_h;
    float total_h = 0.0f;
    float y;
    int total_rows = 1;
    int body_px;
    int min_body_px;
    int max_body_px;

    if (!screen->active || !sdl_welcome_screen_available())
        return;

    canvas = canvas_override ? *canvas_override
                             : sdl_get_window_pixel_rect();
    if (!sdl_rect_has_area(&canvas))
        return;

    content = sdl_welcome_content_rect(&canvas);
    column_w = sdl_char_sheet_clampf((float)canvas.w * 0.72f, 280.0f,
        1280.0f);
    if (column_w > content.w)
        column_w = content.w;
    column_x = content.x + (content.w - column_w) * 0.5f;

    top_margin = sdl_welcome_top_margin(&canvas);
    bottom_margin = sdl_welcome_bottom_margin(&canvas);
    available_h = (float)canvas.h - top_margin - bottom_margin;
    if (available_h < 1.0f)
        available_h = (float)canvas.h;

    min_body_px = MAX(12, (int)((float)canvas.h * 0.018f + 0.5f));
    max_body_px = (int)sdl_char_sheet_clampf((float)canvas.h * 0.040f,
        24.0f, 52.0f);
    if (max_body_px < min_body_px)
        max_body_px = min_body_px;

    for (body_px = max_body_px; body_px >= min_body_px; body_px--)
    {
        font = sdl_story_font_for_height_slot(body_px,
            SDL_WELCOME_STORY_FONT_SLOT);
        if (!font)
            continue;

        line_h = sdl_char_sheet_line_h(font, body_px, 1.18f);
        total_rows = sdl_pause_text_total_rows(font, column_w, body_px);
        total_h = (float)total_rows * line_h;
        if (total_h <= available_h || body_px == min_body_px)
            break;
    }

    if (!font)
        return;

    y = (float)canvas.y + top_margin;
    if (available_h > total_h)
        y += (available_h - total_h) * 0.5f;

    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    for (int i = 0; i < screen->line_count; i++)
    {
        const sdl_pause_text_line* line = &screen->lines[i];
        int wrapped_rows = sdl_pause_text_wrapped_rows(font, line,
            column_w, body_px);
        float indent_px = sdl_pause_text_indent_px(font, line->indent,
            body_px);
        float text_w = column_w - indent_px;

        if (text_w < column_w * 0.30f)
            text_w = column_w * 0.30f;
        if (i < screen->visible_lines && line->text[0])
        {
            sdl_char_sheet_draw_wrapped(font, line->text, line->attr,
                column_x + indent_px, y, text_w,
                (float)wrapped_rows * line_h + line_h, line_h,
                wrapped_rows);
        }
        y += (float)wrapped_rows * line_h;
    }
}

void sdl_pause_text_screen_render(void)
{
    SDL_Rect canvas = sdl_get_window_pixel_rect();
    bool portrait;

    portrait = sdl_narrative_portrait_begin(&canvas);
    sdl_pause_text_screen_render_canvas(&canvas);
    sdl_narrative_portrait_finish(portrait);
}

typedef struct sdl_tale_layout_metrics {
    SDL_Rect canvas;
    SDL_FRect content;
    TTF_Font* title_font;
    TTF_Font* heading_font;
    TTF_Font* body_font;
    TTF_Font* prompt_font;
    float column_x;
    float column_w;
    float title_y;
    float title_h;
    float heading_h;
    float body_line_h;
    float entry_gap;
    float body_y;
    float body_h;
    float prompt_y;
    float prompt_h;
    int body_px;
} sdl_tale_layout_metrics;

static bool sdl_tale_screen_metrics(sdl_tale_layout_metrics* out,
    const SDL_Rect* canvas_override)
{
    SDL_Rect canvas;
    SDL_FRect content;
    float top_margin;
    float bottom_margin;
    int min_body_px;
    int max_body_px;

    if (!out || !sdl_welcome_screen_available())
        return false;

    memset(out, 0, sizeof(*out));
    canvas = canvas_override ? *canvas_override
                             : sdl_get_window_pixel_rect();
    if (!canvas_override)
        (void)sdl_narrative_portrait_adjust_canvas(&canvas);
    if (!sdl_rect_has_area(&canvas))
        return false;
    content = sdl_welcome_content_rect(&canvas);
    top_margin = sdl_welcome_top_margin(&canvas);
    bottom_margin = sdl_welcome_bottom_margin(&canvas);
    min_body_px = MAX(12, (int)((float)canvas.h * 0.017f + 0.5f));
    if (g_sdl_tale_screen.manuscript && canvas.w > canvas.h)
    {
        /*
         * A wide manuscript has ample room for wrapping and pagination, but
         * scaling its type by the conservative shared tale-screen ratio made
         * the intro read like fine print on a landscape phone.  Spend the
         * shorter axis on legibility; page building below absorbs the larger
         * lines without clipping passages.
         */
        max_body_px = (int)sdl_char_sheet_clampf((float)canvas.h * 0.050f,
            30.0f, 52.0f);
    }
    else
    {
        max_body_px = (int)sdl_char_sheet_clampf((float)canvas.h * 0.029f,
            24.0f, 44.0f);
    }
    if (max_body_px < min_body_px)
        max_body_px = min_body_px;

    out->canvas = canvas;
    out->content = content;
    out->column_w = MIN(content.w, (float)canvas.w
        * (g_sdl_tale_screen.manuscript ? 0.72f : 0.92f));
    out->column_x = content.x + (content.w - out->column_w) * 0.5f;

    for (int body_px = max_body_px; body_px >= min_body_px; body_px--)
    {
        int title_px = MAX(body_px + 2,
            (int)((float)body_px * 1.24f + 0.5f));
        int heading_px = MAX(body_px + 1,
            (int)((float)body_px * 1.12f + 0.5f));
        int prompt_px = MAX(12,
            (int)((float)body_px * 0.88f + 0.5f));
        float footer_gap;

        out->title_font = sdl_story_font_for_height_slot(title_px,
            SDL_STORY_FONT_SLOT_DEFAULT);
        out->heading_font = sdl_story_font_for_height_slot(heading_px,
            SDL_STORY_FONT_SLOT_DEFAULT);
        out->body_font = sdl_story_font_for_height_slot(body_px,
            g_sdl_tale_screen.manuscript
                ? SDL_STORY_FONT_SLOT_NARRATIVE
                : SDL_STORY_FONT_SLOT_DEFAULT);
        out->prompt_font = sdl_story_font_for_height_slot(prompt_px,
            g_sdl_tale_screen.manuscript
                ? SDL_STORY_FONT_SLOT_NARRATIVE
                : SDL_STORY_FONT_SLOT_DEFAULT);
        if (!out->title_font || !out->heading_font || !out->body_font
            || !out->prompt_font)
        {
            continue;
        }

        out->title_h = sdl_char_sheet_line_h(out->title_font, title_px,
            1.08f);
        out->heading_h = sdl_char_sheet_line_h(out->heading_font,
            heading_px, 1.10f);
        out->body_line_h = sdl_char_sheet_line_h(out->body_font, body_px,
            1.18f);
        out->entry_gap = out->body_line_h * 0.72f;
        out->prompt_h = sdl_char_sheet_line_h(out->prompt_font, prompt_px,
            1.12f);
        out->title_y = (float)canvas.y + top_margin;
        out->body_y = out->title_y + out->title_h
            + out->body_line_h * 0.72f;
        out->prompt_y = (float)canvas.y + (float)canvas.h - bottom_margin
            - out->prompt_h;
        footer_gap = out->body_line_h * 0.82f;
        out->body_h = out->prompt_y - footer_gap - out->body_y;
        out->body_px = body_px;

        if (out->body_h >= out->heading_h + out->body_line_h * 2.0f
            || body_px == min_body_px)
        {
            return true;
        }
    }

    return false;
}

static void sdl_tale_screen_append_layout_line(bool heading, int entry,
    cptr text)
{
    sdl_tale_layout_line* line;

    if (g_sdl_tale_screen.layout_line_count
        >= SDL_TALE_LAYOUT_MAX_LINES)
    {
        return;
    }

    line = &g_sdl_tale_screen
        .layout_lines[g_sdl_tale_screen.layout_line_count++];
    line->heading = heading;
    line->entry = entry;
    SDL_strlcpy(line->text, text ? text : "", sizeof(line->text));
}

static void sdl_tale_screen_build_layout(
    const sdl_tale_layout_metrics* metrics)
{
    char wrapped[SDL_CHAR_SHEET_MAX_LINES][SDL_CHAR_SHEET_TEXT_LEN];
    int page_start = 0;
    float used_h = 0.0f;

    g_sdl_tale_screen.layout_line_count = 0;
    g_sdl_tale_screen.page_count = 1;
    g_sdl_tale_screen.page_starts[0] = 0;

    for (int i = 0; i < g_sdl_tale_screen.entry_count; i++)
    {
        const sdl_tale_entry* entry = &g_sdl_tale_screen.entries[i];
        int wrapped_count;

        if (entry->heading[0])
            sdl_tale_screen_append_layout_line(true, i, entry->heading);

        wrapped_count = entry->body[0]
            ? sdl_char_sheet_wrap_text(metrics->body_font, entry->body,
                metrics->column_w, wrapped, SDL_CHAR_SHEET_MAX_LINES)
            : 0;
        for (int line = 0; line < wrapped_count; line++)
            sdl_tale_screen_append_layout_line(false, i, wrapped[line]);
    }

    for (int i = 0; i < g_sdl_tale_screen.layout_line_count; i++)
    {
        const sdl_tale_layout_line* line =
            &g_sdl_tale_screen.layout_lines[i];
        float line_h = line->heading
            ? metrics->heading_h : metrics->body_line_h;
        bool new_entry = i > page_start
            && line->entry != g_sdl_tale_screen.layout_lines[i - 1].entry;
        float gap = ((line->heading ||
            (g_sdl_tale_screen.manuscript && new_entry)) && i > page_start)
            ? metrics->entry_gap : 0.0f;
        float keep_h = line_h + gap;

        /* A manuscript leaf turns between passages whenever a whole passage
         * can fit.  This keeps the writing cadence from stranding the last
         * lines of a paragraph on the following leaf. */
        if (g_sdl_tale_screen.manuscript && new_entry)
        {
            float entry_h = 0.0f;

            for (int next = i;
                 next < g_sdl_tale_screen.layout_line_count
                    && g_sdl_tale_screen.layout_lines[next].entry
                        == line->entry;
                 next++)
            {
                entry_h += g_sdl_tale_screen.layout_lines[next].heading
                    ? metrics->heading_h : metrics->body_line_h;
            }
            if (entry_h <= metrics->body_h
                && used_h + metrics->entry_gap + entry_h > metrics->body_h
                && g_sdl_tale_screen.page_count < SDL_TALE_MAX_PAGES)
            {
                page_start = i;
                g_sdl_tale_screen
                    .page_starts[g_sdl_tale_screen.page_count++] = i;
                used_h = 0.0f;
                gap = 0.0f;
                keep_h = line_h;
            }
        }

        /* Do not leave a heading orphaned at the bottom of a page. */
        if (line->heading && i + 1 < g_sdl_tale_screen.layout_line_count
            && !g_sdl_tale_screen.layout_lines[i + 1].heading)
        {
            keep_h += metrics->body_line_h;
        }

        if (i > page_start && used_h + keep_h > metrics->body_h
            && g_sdl_tale_screen.page_count < SDL_TALE_MAX_PAGES)
        {
            page_start = i;
            g_sdl_tale_screen.page_starts[g_sdl_tale_screen.page_count++] = i;
            used_h = 0.0f;
            gap = 0.0f;
        }
        else if (i > page_start && used_h + line_h + gap > metrics->body_h
            && g_sdl_tale_screen.page_count < SDL_TALE_MAX_PAGES)
        {
            page_start = i;
            g_sdl_tale_screen.page_starts[g_sdl_tale_screen.page_count++] = i;
            used_h = 0.0f;
            gap = 0.0f;
        }

        used_h += line_h + gap;
    }

    g_sdl_tale_screen.page_starts[g_sdl_tale_screen.page_count] =
        g_sdl_tale_screen.layout_line_count;
    g_sdl_tale_screen.layout_canvas_w = metrics->canvas.w;
    g_sdl_tale_screen.layout_canvas_h = metrics->canvas.h;
    g_sdl_tale_screen.page = sdl_char_sheet_clampi(g_sdl_tale_screen.page,
        0, MAX(0, g_sdl_tale_screen.page_count - 1));
}

static bool sdl_tale_screen_ensure_layout_for_canvas(
    sdl_tale_layout_metrics* out_metrics, const SDL_Rect* canvas_override)
{
    sdl_tale_layout_metrics metrics;

    if (!g_sdl_tale_screen.active
        || !sdl_tale_screen_metrics(&metrics, canvas_override))
        return false;

    if (g_sdl_tale_screen.layout_canvas_w != metrics.canvas.w
        || g_sdl_tale_screen.layout_canvas_h != metrics.canvas.h
        || g_sdl_tale_screen.layout_line_count <= 0)
    {
        sdl_tale_screen_build_layout(&metrics);
    }
    if (out_metrics)
        *out_metrics = metrics;
    return true;
}

static bool sdl_tale_screen_ensure_layout(
    sdl_tale_layout_metrics* out_metrics)
{
    return sdl_tale_screen_ensure_layout_for_canvas(out_metrics, NULL);
}

static int sdl_tale_utf8_character_count(cptr text)
{
    int bytes = 0;
    int characters = 0;

    if (!text)
        return 0;
    while (text[bytes])
    {
        int sequence_len = utf8_sequence_len(text + bytes);

        if (sequence_len < 1)
            sequence_len = 1;
        bytes += sequence_len;
        characters++;
    }
    return characters;
}

static int sdl_tale_utf8_prefix_bytes(cptr text, int characters)
{
    int bytes = 0;

    if (!text || characters <= 0)
        return 0;
    while (text[bytes] && characters-- > 0)
    {
        int sequence_len = utf8_sequence_len(text + bytes);

        if (sequence_len < 1)
            sequence_len = 1;
        bytes += sequence_len;
    }
    return bytes;
}

int sdl_tale_screen_current_page_entry_count(void)
{
    int start;
    int end;
    int last_entry = -1;
    int count = 0;

    if (!sdl_tale_screen_ensure_layout(NULL))
        return 0;
    start = g_sdl_tale_screen.page_starts[g_sdl_tale_screen.page];
    end = g_sdl_tale_screen.page_starts[g_sdl_tale_screen.page + 1];
    for (int i = start; i < end; i++)
    {
        int entry = g_sdl_tale_screen.layout_lines[i].entry;

        if (entry != last_entry)
        {
            count++;
            last_entry = entry;
        }
    }
    return count;
}

int sdl_tale_screen_current_page_entry_at(int position)
{
    int start;
    int end;
    int last_entry = -1;
    int found = 0;

    if (position < 0 || !sdl_tale_screen_ensure_layout(NULL))
        return -1;
    start = g_sdl_tale_screen.page_starts[g_sdl_tale_screen.page];
    end = g_sdl_tale_screen.page_starts[g_sdl_tale_screen.page + 1];
    for (int i = start; i < end; i++)
    {
        int entry = g_sdl_tale_screen.layout_lines[i].entry;

        if (entry == last_entry)
            continue;
        if (found == position)
            return entry;
        found++;
        last_entry = entry;
    }
    return -1;
}

int sdl_tale_screen_entry_character_count(int entry)
{
    int characters = 0;
    int start;
    int end;

    if (entry < 0 || entry >= g_sdl_tale_screen.entry_count
        || !sdl_tale_screen_ensure_layout(NULL))
    {
        return 0;
    }
    start = g_sdl_tale_screen.page_starts[g_sdl_tale_screen.page];
    end = g_sdl_tale_screen.page_starts[g_sdl_tale_screen.page + 1];
    for (int i = start; i < end; i++)
    {
        const sdl_tale_layout_line* line =
            &g_sdl_tale_screen.layout_lines[i];

        if (line->entry == entry)
            characters += sdl_tale_utf8_character_count(line->text);
    }
    return characters;
}

int sdl_tale_screen_entry_character_at(int entry, int position)
{
    int start;
    int end;

    if (entry < 0 || entry >= g_sdl_tale_screen.entry_count || position < 0
        || !sdl_tale_screen_ensure_layout(NULL))
    {
        return 0;
    }
    start = g_sdl_tale_screen.page_starts[g_sdl_tale_screen.page];
    end = g_sdl_tale_screen.page_starts[g_sdl_tale_screen.page + 1];
    for (int i = start; i < end; i++)
    {
        const sdl_tale_layout_line* line =
            &g_sdl_tale_screen.layout_lines[i];
        int line_characters;

        if (line->entry != entry)
            continue;
        line_characters = sdl_tale_utf8_character_count(line->text);
        if (position < line_characters)
        {
            int bytes = sdl_tale_utf8_prefix_bytes(line->text, position);

            if ((unsigned char)line->text[bytes] == 0xE2
                && (unsigned char)line->text[bytes + 1] == 0x80
                && (unsigned char)line->text[bytes + 2] == 0x94)
            {
                return 0x2014;
            }

            return (unsigned char)line->text[bytes];
        }
        position -= line_characters;
    }
    return 0;
}

void sdl_tale_screen_set_active_entry(int active_entry, byte alpha)
{
    if (!g_sdl_tale_screen.active)
        return;

    g_sdl_tale_screen.active_entry = active_entry;
    g_sdl_tale_screen.active_visible_characters = -1;
    g_sdl_tale_screen.typewriter_cursor_visible = false;
    g_sdl_tale_screen.active_alpha = alpha;
    sdl_welcome_screen_mark_dirty();
}

void sdl_tale_screen_set_typewriter_entry(int active_entry,
    int visible_characters, bool cursor_visible)
{
    if (!g_sdl_tale_screen.active)
        return;

    g_sdl_tale_screen.active_entry = active_entry;
    g_sdl_tale_screen.active_visible_characters = MAX(0,
        visible_characters);
    g_sdl_tale_screen.typewriter_cursor_visible = cursor_visible;
    g_sdl_tale_screen.active_alpha = 255;
    sdl_welcome_screen_mark_dirty();
}

void sdl_tale_screen_set_prompt(cptr prompt, bool visible, bool final)
{
    if (!g_sdl_tale_screen.active)
        return;
    SDL_strlcpy(g_sdl_tale_screen.prompt, prompt ? prompt : "",
        sizeof(g_sdl_tale_screen.prompt));
    g_sdl_tale_screen.prompt_visible = visible;
    g_sdl_tale_screen.prompt_final = visible && final;
    g_sdl_tale_screen.prompt_hovered = false;
    g_sdl_tale_screen.prompt_next_rect = (SDL_FRect){ 0 };
    g_sdl_tale_screen.prompt_skip_rect = (SDL_FRect){ 0 };
    sdl_welcome_screen_mark_dirty();
}

bool sdl_tale_screen_advance_page(void)
{
    if (!sdl_tale_screen_ensure_layout(NULL)
        || g_sdl_tale_screen.page + 1 >= g_sdl_tale_screen.page_count)
    {
        return false;
    }

    g_sdl_tale_screen.page++;
    g_sdl_tale_screen.prompt_visible = false;
    sdl_welcome_screen_mark_dirty();
    return true;
}

bool sdl_tale_screen_is_last_page(void)
{
    if (!sdl_tale_screen_ensure_layout(NULL))
        return true;
    return g_sdl_tale_screen.page + 1 >= g_sdl_tale_screen.page_count;
}

/* Keep the manuscript's inked frame and ornament, but leave its interior
 * transparent so the normal black story canvas remains the only background. */
static void sdl_tale_screen_draw_manuscript_frame(
    const sdl_tale_layout_metrics* metrics)
{
    SDL_Color outer_color;
    SDL_Color inner_color;
    SDL_FRect leaf;
    SDL_FRect inner;
    SDL_FRect rule;
    float pad;
    float bottom;
    float rule_y;
    float rule_half_w;

    if (!metrics)
        return;

    pad = metrics->body_line_h * 1.05f;
    bottom = metrics->prompt_y + metrics->prompt_h
        + metrics->body_line_h * 0.42f;
    leaf = (SDL_FRect){
        .x = MAX(metrics->content.x, metrics->column_x - pad),
        .y = MAX((float)metrics->canvas.y + 6.0f,
            metrics->title_y - metrics->body_line_h * 0.48f),
        .w = MIN(metrics->content.w, metrics->column_w + pad * 2.0f),
        .h = 0.0f,
    };
    if (leaf.x + leaf.w > metrics->content.x + metrics->content.w)
        leaf.w = metrics->content.x + metrics->content.w - leaf.x;
    leaf.h = MIN(bottom - leaf.y,
        (float)metrics->canvas.y + (float)metrics->canvas.h - 6.0f
            - leaf.y);

    outer_color = sdl_welcome_color(TERM_SLATE, 210);
    SDL_SetRenderDrawColor(g_state.renderer, outer_color.r, outer_color.g,
        outer_color.b, outer_color.a);
    (void)SDL_RenderRect(g_state.renderer, &leaf);

    inner = leaf;
    inner.x += 5.0f;
    inner.y += 5.0f;
    inner.w -= 10.0f;
    inner.h -= 10.0f;
    inner_color = sdl_welcome_color(TERM_L_DARK, 220);
    SDL_SetRenderDrawColor(g_state.renderer, inner_color.r, inner_color.g,
        inner_color.b, inner_color.a);
    (void)SDL_RenderRect(g_state.renderer, &inner);

    rule_y = metrics->title_y + metrics->title_h
        + metrics->body_line_h * 0.25f;
    rule_half_w = metrics->column_w * 0.17f;
    SDL_SetRenderDrawColor(g_state.renderer, 111, 81, 38, 190);
    rule = (SDL_FRect){
        .x = metrics->column_x + metrics->column_w * 0.5f
            - rule_half_w - 7.0f,
        .y = rule_y,
        .w = rule_half_w,
        .h = 1.0f,
    };
    (void)SDL_RenderFillRect(g_state.renderer, &rule);
    rule.x = metrics->column_x + metrics->column_w * 0.5f + 7.0f;
    (void)SDL_RenderFillRect(g_state.renderer, &rule);
    rule = (SDL_FRect){
        .x = metrics->column_x + metrics->column_w * 0.5f - 2.0f,
        .y = rule_y - 2.0f,
        .w = 4.0f,
        .h = 4.0f,
    };
    (void)SDL_RenderFillRect(g_state.renderer, &rule);
}

static void sdl_tale_screen_draw_typewriter_cursor(
    const sdl_tale_layout_metrics* metrics, const SDL_FRect* text_rect,
    float y, float line_h)
{
    SDL_FRect cursor;

    if (!metrics || (SDL_GetTicks() / 360u) % 2u != 0u)
        return;

    cursor = (SDL_FRect){
        .x = (text_rect && text_rect->w > 0.0f)
            ? text_rect->x + text_rect->w + 2.0f : metrics->column_x,
        .y = y + line_h * 0.16f,
        .w = MAX(1.0f, (float)metrics->body_px * 0.055f),
        .h = line_h * 0.70f,
    };
    SDL_SetRenderDrawColor(g_state.renderer, 224, 185, 92, 235);
    (void)SDL_RenderFillRect(g_state.renderer, &cursor);
}

static void sdl_tale_screen_render_canvas(const SDL_Rect* canvas)
{
    const sdl_tale_screen_state* screen = &g_sdl_tale_screen;
    sdl_tale_layout_metrics metrics;
    int start;
    int end;
    float y;

    if (!sdl_tale_screen_ensure_layout_for_canvas(&metrics, canvas))
        return;

    start = screen->page_starts[screen->page];
    end = screen->page_starts[screen->page + 1];
    y = metrics.body_y;

    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    if (screen->manuscript)
    {
        char folio[48];
        int folio_w;
        int title_w;
        int remaining = screen->active_visible_characters;
        int previous_entry = -1;
        bool cursor_drawn = false;

        sdl_tale_screen_draw_manuscript_frame(&metrics);
        (void)sdl_char_sheet_draw_text(metrics.title_font, screen->title,
            TERM_YELLOW, metrics.column_x, metrics.title_y,
            metrics.column_w, metrics.title_h, true);

        strnfmt(folio, sizeof(folio), "Page %d of %d", screen->page + 1,
            MAX(1, screen->page_count));
        folio_w = sdl_char_sheet_text_width(metrics.prompt_font, folio);
        title_w = sdl_char_sheet_text_width(metrics.title_font,
            screen->title);
        if ((float)(folio_w + title_w) + metrics.body_line_h * 1.5f
            < metrics.column_w)
        {
            (void)sdl_char_sheet_draw_text(metrics.prompt_font, folio,
                TERM_L_WHITE,
                metrics.column_x + metrics.column_w - (float)folio_w,
                metrics.title_y, (float)folio_w, metrics.title_h, false);
        }

        for (int i = start; i < end; i++)
        {
            const sdl_tale_layout_line* line = &screen->layout_lines[i];
            float line_h = line->heading
                ? metrics.heading_h : metrics.body_line_h;
            byte attr = TERM_WHITE;

            if (previous_entry >= 0 && line->entry != previous_entry)
                y += metrics.entry_gap;

            if (line->entry <= screen->active_entry)
            {
                cptr draw_text = line->text;
                char partial[SDL_TALE_LAYOUT_LINE_LEN];
                byte alpha = 255;
                SDL_FRect text_rect = { 0 };

                if (line->entry == screen->active_entry
                    && remaining >= 0)
                {
                    int line_characters =
                        sdl_tale_utf8_character_count(line->text);
                    int visible = MIN(remaining, line_characters);
                    int bytes = sdl_tale_utf8_prefix_bytes(line->text,
                        visible);

                    if (visible < line_characters)
                    {
                        memcpy(partial, line->text, (size_t)bytes);
                        partial[bytes] = '\0';
                        draw_text = partial;
                    }
                    remaining -= visible;
                }

                text_rect = sdl_char_sheet_draw_text_alpha(
                    line->heading ? metrics.heading_font : metrics.body_font,
                    draw_text, attr, alpha, metrics.column_x, y,
                    metrics.column_w, line_h);

                if (line->entry == screen->active_entry
                    && screen->typewriter_cursor_visible && !cursor_drawn
                    && remaining == 0)
                {
                    sdl_tale_screen_draw_typewriter_cursor(&metrics,
                        &text_rect, y, line_h);
                    cursor_drawn = true;
                }
            }

            y += line_h;
            previous_entry = line->entry;
        }
    }
    else
    {
        (void)sdl_char_sheet_draw_text(metrics.title_font, screen->title,
            TERM_YELLOW, metrics.column_x, metrics.title_y, metrics.column_w,
            metrics.title_h, false);

        for (int i = start; i < end; i++)
        {
            const sdl_tale_layout_line* line = &screen->layout_lines[i];
            float line_h = line->heading
                ? metrics.heading_h : metrics.body_line_h;
            byte attr = line->heading ? TERM_L_BLUE : TERM_WHITE;

            if (line->entry != screen->active_entry)
                continue;
            (void)sdl_char_sheet_draw_text_alpha(
                line->heading ? metrics.heading_font : metrics.body_font,
                line->text, attr, screen->active_alpha,
                metrics.column_x, y, metrics.column_w, line_h);
            y += line_h;
        }
    }

    if (screen->prompt_visible && screen->prompt[0])
    {
        const char* split = strchr(screen->prompt, '*');
        SDL_FRect prompt_rect = sdl_char_sheet_draw_text(
            metrics.prompt_font, screen->prompt,
            screen->manuscript ? TERM_L_WHITE
                : (screen->prompt_final && screen->prompt_hovered
                    ? TERM_YELLOW : TERM_SLATE),
            metrics.column_x, metrics.prompt_y,
            metrics.column_w, metrics.prompt_h, screen->manuscript);

        g_sdl_tale_screen.prompt_next_rect = prompt_rect;
        g_sdl_tale_screen.prompt_skip_rect = (SDL_FRect){ 0 };
        if (split && prompt_rect.w > 0.0f)
        {
            int prefix_len = (int)(split - screen->prompt) + 1;
            int prefix_w = 0;

            TTF_MeasureString(metrics.prompt_font, screen->prompt,
                (size_t)prefix_len, 0, &prefix_w, NULL);
            if ((float)prefix_w < prompt_rect.w)
            {
                g_sdl_tale_screen.prompt_skip_rect = (SDL_FRect){
                    .x = prompt_rect.x + (float)prefix_w,
                    .y = prompt_rect.y,
                    .w = prompt_rect.w - (float)prefix_w,
                    .h = MAX(prompt_rect.h, metrics.prompt_h),
                };
                g_sdl_tale_screen.prompt_next_rect.w = (float)prefix_w;
                g_sdl_tale_screen.prompt_next_rect.h =
                    MAX(prompt_rect.h, metrics.prompt_h);
            }
        }
    }
}

void sdl_tale_screen_render(void)
{
    SDL_Rect canvas = sdl_get_window_pixel_rect();
    bool portrait;

    portrait = sdl_narrative_portrait_begin(&canvas);
    sdl_tale_screen_render_canvas(&canvas);
    sdl_narrative_portrait_finish(portrait);
}

static void sdl_poetry_choice_screen_render(const SDL_Rect* canvas_override)
{
    sdl_poetry_screen_state* screen = &g_sdl_poetry_screen;
    SDL_Rect canvas;
    SDL_FRect content;
    TTF_Font* title_font = NULL;
    TTF_Font* label_font = NULL;
    TTF_Font* body_font = NULL;
    TTF_Font* prompt_font = NULL;
    float column_w;
    float column_x;
    float top_margin;
    float bottom_margin;
    float title_h = 0.0f;
    float label_h = 0.0f;
    float body_line_h = 0.0f;
    float prompt_h = 0.0f;
    float title_gap = 0.0f;
    float choice_gap = 0.0f;
    float label_body_gap = 0.0f;
    float footer_gap = 0.0f;
    float total_h = 0.0f;
    float prompt_y;
    float available_h;
    float y;
    int body_rows[SDL_POETRY_MAX_CHOICES] = { 0 };
    int body_px;
    int min_body_px;
    int max_body_px;

    if (!screen->active || screen->choice_count <= 0
        || !sdl_welcome_screen_available())
    {
        return;
    }

    canvas = canvas_override ? *canvas_override
                             : sdl_get_window_pixel_rect();
    if (!sdl_rect_has_area(&canvas))
        return;

    content = sdl_welcome_content_rect(&canvas);
    column_w = sdl_char_sheet_clampf((float)canvas.w * 0.78f, 300.0f,
        1120.0f);
    if (column_w > content.w)
        column_w = content.w;
    column_x = content.x + (content.w - column_w) * 0.5f;
    top_margin = sdl_welcome_top_margin(&canvas);
    top_margin = sdl_poetry_sequence_top_margin(&canvas, top_margin);
    bottom_margin = sdl_welcome_bottom_margin(&canvas);
    min_body_px = MAX(12, (int)((float)canvas.h * 0.015f + 0.5f));
    max_body_px = (int)sdl_char_sheet_clampf((float)canvas.h * 0.030f,
        20.0f, 38.0f);
    if (max_body_px < min_body_px)
        max_body_px = min_body_px;

    for (body_px = max_body_px; body_px >= min_body_px; body_px--)
    {
        int title_px = MAX(body_px + 3,
            (int)((float)body_px * 1.28f + 0.5f));
        int label_px = MAX(body_px + 1,
            (int)((float)body_px * 1.10f + 0.5f));
        int prompt_px = MAX(12, (int)((float)body_px * 0.82f + 0.5f));

        title_font = sdl_story_font_for_height_slot(title_px,
            SDL_STORY_FONT_SLOT_DEFAULT);
        label_font = sdl_story_font_for_height_slot(label_px,
            SDL_STORY_FONT_SLOT_DEFAULT);
        body_font = sdl_story_font_for_height_slot(body_px,
            SDL_WELCOME_STORY_FONT_SLOT);
        prompt_font = sdl_story_font_for_height_slot(prompt_px,
            SDL_WELCOME_STORY_FONT_SLOT);
        if (!title_font || !label_font || !body_font || !prompt_font)
            continue;

        title_h = sdl_char_sheet_line_h(title_font, title_px, 1.08f);
        label_h = sdl_char_sheet_line_h(label_font, label_px, 1.10f);
        body_line_h = sdl_char_sheet_line_h(body_font, body_px, 1.18f);
        prompt_h = sdl_char_sheet_line_h(prompt_font, prompt_px, 1.14f);
        title_gap = body_line_h * 0.82f;
        choice_gap = body_line_h * 0.72f;
        label_body_gap = body_line_h * 0.18f;
        footer_gap = body_line_h * 0.88f;
        total_h = title_h + title_gap;

        for (int i = 0; i < screen->choice_count; i++)
        {
            body_rows[i] = screen->choices[i].body[0]
                ? sdl_char_sheet_wrap_text(body_font,
                    screen->choices[i].body, column_w - body_line_h,
                    NULL, 0)
                : 0;
            total_h += label_h + label_body_gap
                + (float)body_rows[i] * body_line_h;
            if (i + 1 < screen->choice_count)
                total_h += choice_gap;
        }

        available_h = (float)canvas.h - top_margin - bottom_margin
            - prompt_h - footer_gap;
        if (total_h <= available_h || body_px == min_body_px)
            break;
    }

    if (!title_font || !label_font || !body_font || !prompt_font)
        return;

    prompt_y = (float)canvas.y + (float)canvas.h - bottom_margin
        - prompt_h;
    available_h = prompt_y - footer_gap - ((float)canvas.y + top_margin);
    y = (float)canvas.y + top_margin;
    if (!g_sdl_poetry_sequence_layout.active && available_h > total_h)
        y += (available_h - total_h) * 0.5f;

    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    if (screen->title_visible)
    {
        int text_w = sdl_char_sheet_text_width(title_font, screen->title);
        float title_x = content.x;

        if ((float)text_w < content.w)
            title_x += (content.w - (float)text_w) * 0.5f;
        (void)sdl_char_sheet_draw_text_alpha(title_font, screen->title,
            screen->title_attr, screen->title_alpha, title_x, y,
            content.w, title_h);
    }
    y += title_h + title_gap;

    for (int i = 0; i < screen->choice_count; i++)
    {
        sdl_poetry_choice_state* item = &screen->choices[i];
        bool focused = screen->prompt_visible
            && item->choice == screen->highlight;
        float block_h = label_h + label_body_gap
            + (float)body_rows[i] * body_line_h;
        float pad = MAX(6.0f, body_line_h * 0.34f);
        SDL_FRect hit = {
            .x = column_x - pad,
            .y = y - pad * 0.45f,
            .w = column_w + pad * 2.0f,
            .h = block_h + pad * 0.90f,
        };

        item->hit_rect = hit;
        if (focused)
        {
            SDL_Color fill = sdl_welcome_color(TERM_RED, 34);
            SDL_Color edge = sdl_welcome_color(TERM_RED, 150);

            SDL_SetRenderDrawColor(g_state.renderer, fill.r, fill.g,
                fill.b, fill.a);
            SDL_RenderFillRect(g_state.renderer, &hit);
            SDL_SetRenderDrawColor(g_state.renderer, edge.r, edge.g,
                edge.b, edge.a);
            SDL_RenderRect(g_state.renderer, &hit);
        }

        if (item->visible)
        {
            byte label_attr = focused ? TERM_RED : item->label_attr;

            (void)sdl_char_sheet_draw_text_alpha(label_font, item->label,
                label_attr, item->alpha, column_x, y, column_w, label_h);
            if (body_rows[i] > 0)
            {
                sdl_char_sheet_draw_wrapped_alpha(body_font, item->body,
                    item->body_attr, item->alpha,
                    column_x + body_line_h * 0.55f,
                    y + label_h + label_body_gap,
                    column_w - body_line_h * 0.55f,
                    (float)body_rows[i] * body_line_h + body_line_h,
                    body_line_h, body_rows[i]);
            }
        }

        y += block_h + choice_gap;
    }

    screen->prompt_rect = (SDL_FRect){ 0 };
    if (screen->prompt_visible && screen->prompt[0])
    {
        SDL_FRect prompt_box = {
            .x = content.x,
            .y = prompt_y,
            .w = content.w,
            .h = prompt_h,
        };
        screen->prompt_rect = sdl_welcome_draw_text_box(prompt_font,
            screen->prompt, TERM_SLATE, prompt_box, true);
    }
}

/*
 * Draw poetry on the real window canvas.  It deliberately shares the welcome
 * screen's margins, story-font roles, black field, and responsive pixel
 * sizing, but wraps data-driven paragraphs instead of relying on terminal
 * rows and columns.
 */
static void sdl_poetry_screen_render_canvas(const SDL_Rect* canvas_override)
{
    const sdl_poetry_screen_state* screen = &g_sdl_poetry_screen;
    SDL_Rect canvas;
    SDL_FRect content;
    TTF_Font* title_font = NULL;
    TTF_Font* body_font = NULL;
    TTF_Font* prompt_font = NULL;
    float column_w;
    float column_x;
    float title_h = 0.0f;
    float body_line_h = 0.0f;
    float prompt_line_h = 0.0f;
    float title_gap = 0.0f;
    float transition_gap = 0.0f;
    float block_gap = 0.0f;
    float footer_gap = 0.0f;
    float narrative_h = 0.0f;
    float top_margin;
    float bottom_margin;
    float footer_y;
    float narrative_bottom;
    float y;
    int body_lines = 0;
    int transition_lines = 0;
    int block_lines[SDL_POETRY_MAX_BLOCKS] = { 0 };
    int body_px;
    int max_body_px;
    int min_body_px;
    bool block_mode;

    if (!screen->active || !sdl_welcome_screen_available())
        return;
    if (screen->choice_count > 0)
    {
        sdl_poetry_choice_screen_render(canvas_override);
        return;
    }
    block_mode = (screen->block_count > 0);

    canvas = canvas_override ? *canvas_override
                             : sdl_get_window_pixel_rect();
    if (!sdl_rect_has_area(&canvas))
        return;

    content = sdl_welcome_content_rect(&canvas);
    column_w = sdl_char_sheet_clampf((float)canvas.w * 0.66f, 260.0f,
        920.0f);
    if (column_w > content.w)
        column_w = content.w;
    column_x = content.x + (content.w - column_w) * 0.5f;

    top_margin = sdl_welcome_top_margin(&canvas);
    top_margin = sdl_poetry_sequence_top_margin(&canvas, top_margin);
    bottom_margin = sdl_welcome_bottom_margin(&canvas);
    min_body_px = MAX(12, (int)((float)canvas.h * 0.020f + 0.5f));
    max_body_px = (int)sdl_char_sheet_clampf((float)canvas.h * 0.042f,
        22.0f, 44.0f);
    if (max_body_px < min_body_px)
        max_body_px = min_body_px;

    /* Pick one semantic body size for both paragraphs.  Reserving all text
     * bands from the first fade frame keeps the composition from jumping as
     * the title, poem, transition, and prompt appear. */
    for (body_px = max_body_px; body_px >= min_body_px; body_px--)
    {
        int title_px = MAX(1,
            (int)((float)body_px
                * sdl_welcome_role_font_factor(SDL_WELCOME_LINE_TITLE)
                + 0.5f));
        int prompt_px = sdl_welcome_footer_font_px(body_px);
        float available_h = (float)canvas.h - top_margin - bottom_margin;
        float need_h;

        title_font = sdl_story_font_for_height_slot(title_px,
            SDL_STORY_FONT_SLOT_DEFAULT);
        body_font = sdl_story_font_for_height_slot(body_px,
            SDL_WELCOME_STORY_FONT_SLOT);
        prompt_font = sdl_story_font_for_height_slot(prompt_px,
            SDL_WELCOME_STORY_FONT_SLOT);
        if (!title_font || !body_font || !prompt_font)
            continue;

        title_h = sdl_welcome_line_h_for_role(title_font, title_px,
            SDL_WELCOME_LINE_TITLE);
        body_line_h = sdl_char_sheet_line_h(body_font, body_px, 1.20f);
        prompt_line_h = sdl_char_sheet_line_h(prompt_font, prompt_px, 1.18f);
        title_gap = body_line_h * 0.88f;
        block_gap = body_line_h * 0.78f;
        footer_gap = sdl_char_sheet_clampf((float)body_px * 1.35f,
            18.0f, 84.0f);
        narrative_h = title_h + title_gap;
        if (block_mode)
        {
            memset(block_lines, 0, sizeof(block_lines));
            for (int i = 0; i < screen->block_count; i++)
            {
                block_lines[i] = screen->blocks[i].text[0]
                    ? sdl_char_sheet_wrap_text(body_font,
                        screen->blocks[i].text, column_w, NULL, 0)
                    : 0;
                narrative_h += (float)block_lines[i] * body_line_h;
                if (i + 1 < screen->block_count)
                    narrative_h += block_gap;
            }
        }
        else
        {
            body_lines = screen->body[0]
                ? sdl_char_sheet_wrap_text(body_font, screen->body,
                    column_w, NULL, 0)
                : 0;
            transition_lines = screen->transition[0]
                ? sdl_char_sheet_wrap_text(body_font, screen->transition,
                    column_w, NULL, 0)
                : 0;
            transition_gap = (body_lines > 0 && transition_lines > 0)
                ? body_line_h * 0.78f : 0.0f;
            narrative_h += (float)body_lines * body_line_h + transition_gap
                + (float)transition_lines * body_line_h;
        }
        need_h = narrative_h + footer_gap + prompt_line_h;
        if (need_h <= available_h || body_px == min_body_px)
            break;
    }

    if (!title_font || !body_font || !prompt_font)
        return;

    footer_y = (float)canvas.y + (float)canvas.h - bottom_margin
        - prompt_line_h;
    narrative_bottom = footer_y - footer_gap;
    y = (float)canvas.y + top_margin;
    if (!g_sdl_poetry_sequence_layout.active
        && narrative_bottom - y > narrative_h)
    {
        y += (narrative_bottom - y - narrative_h) * 0.5f;
    }

    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    if (screen->title_visible)
    {
        int text_w = sdl_char_sheet_text_width(title_font, screen->title);
        float title_x = content.x;

        if ((float)text_w < content.w)
            title_x += (content.w - (float)text_w) * 0.5f;
        (void)sdl_char_sheet_draw_text_alpha(title_font, screen->title,
            screen->title_attr, screen->title_alpha, title_x, y,
            content.w, title_h);
    }
    y += title_h + title_gap;

    if (block_mode)
    {
        for (int i = 0; i < screen->block_count; i++)
        {
            const sdl_poetry_block_state* block = &screen->blocks[i];

            if (block->visible && block_lines[i] > 0)
            {
                sdl_char_sheet_draw_wrapped_alpha(body_font, block->text,
                    block->attr, block->alpha, column_x, y, column_w,
                    (float)block_lines[i] * body_line_h + body_line_h,
                    body_line_h, block_lines[i]);
            }
            y += (float)block_lines[i] * body_line_h;
            if (i + 1 < screen->block_count)
                y += block_gap;
        }
    }
    else if (screen->body_visible && body_lines > 0)
    {
        sdl_char_sheet_draw_wrapped_alpha(body_font, screen->body,
            screen->body_attr, screen->body_alpha, column_x, y, column_w,
            (float)body_lines * body_line_h + body_line_h, body_line_h,
            body_lines);
    }
    if (!block_mode)
        y += (float)body_lines * body_line_h + transition_gap;

    if (!block_mode && screen->transition_visible && transition_lines > 0)
    {
        sdl_char_sheet_draw_wrapped_alpha(body_font, screen->transition,
            screen->transition_attr, screen->transition_alpha,
            column_x, y, column_w,
            (float)transition_lines * body_line_h + body_line_h,
            body_line_h, transition_lines);
    }

    if (screen->prompt_visible)
    {
        SDL_FRect prompt_box = {
            .x = content.x,
            .y = footer_y,
            .w = content.w,
            .h = prompt_line_h,
        };
        (void)sdl_welcome_draw_text_box(prompt_font, screen->prompt,
            TERM_SLATE, prompt_box, true);
    }
}

void sdl_poetry_screen_render(void)
{
    SDL_Rect canvas = sdl_get_window_pixel_rect();
    bool portrait;

    portrait = sdl_narrative_portrait_begin(&canvas);
    sdl_poetry_screen_render_canvas(&canvas);
    sdl_narrative_portrait_finish(portrait);
}

void sdl_char_sheet_draw_history(TTF_Font* font, cptr text, float x,
    float y, float w, float h, float line_h, int line_count)
{
    sdl_char_sheet_draw_wrapped(font, text, TERM_WHITE, x, y, w, h, line_h,
        line_count);
}

bool sdl_char_sheet_split_first_paragraph(cptr text, char* first,
    size_t first_len, cptr* rest)
{
    cptr sep;
    cptr body;
    size_t len;

    if (first && first_len > 0)
        first[0] = '\0';
    if (rest)
        *rest = text ? text : "";
    if (!text || !text[0] || !first || first_len <= 0 || !rest)
        return false;

    sep = strstr(text, "\n\n");
    if (!sep)
        return false;

    len = (size_t)(sep - text);
    while (len > 0 && isspace((unsigned char)text[len - 1]))
        len--;
    if (len == 0)
        return false;
    if (len >= first_len)
        len = first_len - 1;

    memcpy(first, text, len);
    first[len] = '\0';

    body = sep;
    while (*body && isspace((unsigned char)*body))
        body++;
    if (!body[0])
        return false;

    *rest = body;
    return true;
}

/* A selection row may remain visible and focused while not being a legal
 * choice.  This is used by the character carousel for fallen heroes: their
 * sheet can still be read, but its confirmation controls must be inert. */
static bool sdl_char_sheet_select_choice_confirmable(int choice)
{
    if (g_sdl_character_sheet_screen.context
        != SDL_CHARACTER_SHEET_BIRTH_SELECT)
    {
        return true;
    }

    for (int i = 0; i < g_sdl_character_sheet_screen.select_row_count; i++)
    {
        const sdl_character_sheet_select_row* row =
            &g_sdl_character_sheet_screen.select_rows[i];

        if (!row->is_heading && row->choice == choice)
            return row->confirmable;
    }

    return true;
}

static bool sdl_char_sheet_selected_choice_confirmable(void)
{
    return sdl_char_sheet_select_choice_confirmable(
        g_sdl_character_sheet_screen.selected_index);
}

void sdl_char_sheet_draw_prompt(TTF_Font* font, cptr prompt, float x,
    float y, float w, float h)
{
    typedef struct sdl_char_sheet_prompt_item {
        cptr label;
        int choice;
    } sdl_char_sheet_prompt_item;

    static const sdl_char_sheet_prompt_item live_items[] = {
        { "X abilities", 'x' },
        { "I increase", 'i' },
        { "? help", '?' },
        { "Esc back", ESCAPE },
#ifdef DEBUG_CURSES
        { "C curses", 'c' },
#endif
    };
    static const sdl_char_sheet_prompt_item birth_items[] = {
        { "Esc back", -1 },
        { "Enter confirm", -2 },
    };
    static const sdl_char_sheet_prompt_item select_items[] = {
        { "Esc back", -1 },
        { "Enter select", -2 },
    };
#if SIL_SDL_MOBILE_BUILD
    /* Touch-first labels for the mobile hero carousel (same tappable ids). */
    static const sdl_char_sheet_prompt_item select_items_touch[] = {
        { "Back", -1 },
        { "Choose", -2 },
    };
    static const sdl_char_sheet_prompt_item birth_items_touch[] = {
        { "Back", -1 },
        { "Confirm", -2 },
    };
    /* Touch-first labels for the live in-game character sheet (same tappable
     * keycodes the keyboard handler uses, but rendered as tap buttons). */
    static const sdl_char_sheet_prompt_item live_items_touch[] = {
        { "Abilities", 'x' },
        { "Increase", 'i' },
        { "Help", '?' },
        { "Back", ESCAPE },
    };
#endif
    static const sdl_char_sheet_prompt_item preview_items[] = {
        { "Continue to Stats", -2 },
    };
    const sdl_char_sheet_prompt_item* items = birth_items;
    int item_count = (int)N_ELEMENTS(birth_items);
    sdl_char_sheet_prompt_item birth_controller_items[2];
    sdl_char_sheet_prompt_item select_controller_items[2];
    char controller_back_label[16];
    char controller_confirm_label[16];
    char birth_controller_back_text[32];
    char birth_controller_confirm_text[32];
    char select_controller_back_text[32];
    char select_controller_confirm_text[32];
    float cursor_x = x;
    int text_widths[16];
    float item_widths[16];
    bool preview_prompt = g_sdl_character_sheet_screen.context
        == SDL_CHARACTER_SHEET_BIRTH_PREVIEW;
    bool controller = steamdeck_controls_active();
#if SIL_SDL_MOBILE_BUILD
    float spacing = MAX(8.0f, h * 0.45f);
    bool touch_only = sdl_touch_only_device_active();
    bool touch_buttons = false;
    float touch_button_pad_x = 0.0f;
#else
    float spacing = MAX(12.0f, h * 0.68f);
#endif
    float total_w = 0.0f;

    (void)prompt;

    if (!font)
        return;

    if (controller)
    {
        sdl_gamepad_action_binding_short_label(steamdeck_back_key(),
            controller_back_label, sizeof(controller_back_label));
        if (streq(controller_back_label, "(unbound)")
            || streq(controller_back_label, "Multiple"))
        {
            SDL_strlcpy(controller_back_label, "B",
                sizeof(controller_back_label));
        }

        sdl_gamepad_action_binding_short_label(steamdeck_confirm_key(),
            controller_confirm_label, sizeof(controller_confirm_label));
        if (streq(controller_confirm_label, "(unbound)")
            || streq(controller_confirm_label, "Multiple"))
        {
            SDL_strlcpy(controller_confirm_label, "A",
                sizeof(controller_confirm_label));
        }

        strnfmt(birth_controller_back_text,
            sizeof(birth_controller_back_text), "%s back",
            controller_back_label);
        strnfmt(birth_controller_confirm_text,
            sizeof(birth_controller_confirm_text), "%s confirm",
            controller_confirm_label);
        strnfmt(select_controller_back_text,
            sizeof(select_controller_back_text), "%s back",
            controller_back_label);
        strnfmt(select_controller_confirm_text,
            sizeof(select_controller_confirm_text), "%s select",
            controller_confirm_label);

        birth_controller_items[0].label = birth_controller_back_text;
        birth_controller_items[0].choice = -1;
        birth_controller_items[1].label = birth_controller_confirm_text;
        birth_controller_items[1].choice = -2;

        select_controller_items[0].label = select_controller_back_text;
        select_controller_items[0].choice = -1;
        select_controller_items[1].label = select_controller_confirm_text;
        select_controller_items[1].choice = -2;
    }

    if (g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_LIVE)
    {
        items = live_items;
        item_count = (int)N_ELEMENTS(live_items);
#if SIL_SDL_MOBILE_BUILD
        if (touch_only)
        {
            items = live_items_touch;
            item_count = (int)N_ELEMENTS(live_items_touch);
            touch_buttons = true;
        }
#endif
    }
    else if (g_sdl_character_sheet_screen.context
        == SDL_CHARACTER_SHEET_BIRTH_PREVIEW)
    {
        items = preview_items;
        item_count = (int)N_ELEMENTS(preview_items);
    }
    else if (g_sdl_character_sheet_screen.context
        == SDL_CHARACTER_SHEET_BIRTH_SELECT)
    {
        items = select_items;
        item_count = (int)N_ELEMENTS(select_items);
        if (controller)
        {
            items = select_controller_items;
            item_count = (int)N_ELEMENTS(select_controller_items);
        }
#if SIL_SDL_MOBILE_BUILD
        if (touch_only
            && (sdl_character_sheet_screen_mobile_carousel_active()
                || g_sdl_character_sheet_screen.select_menu_style
                || g_sdl_select_choice_page_only))
        {
            items = select_items_touch;
            item_count = (int)N_ELEMENTS(select_items_touch);
            touch_buttons = true;
        }
#endif
    }
#if SIL_SDL_MOBILE_BUILD
    else if ((g_sdl_character_sheet_screen.context
                 == SDL_CHARACTER_SHEET_BIRTH_STATS
              || g_sdl_character_sheet_screen.context
                 == SDL_CHARACTER_SHEET_BIRTH_SKILLS)
        && touch_only)
    {
        items = birth_items_touch;
        item_count = (int)N_ELEMENTS(birth_items_touch);
        touch_buttons = true;
    }
#endif
    else if (g_sdl_character_sheet_screen.context
        == SDL_CHARACTER_SHEET_BIRTH_SKILLS)
    {
        items = birth_items;
        item_count = (int)N_ELEMENTS(birth_items);
        if (controller)
        {
            items = birth_controller_items;
            item_count = (int)N_ELEMENTS(birth_controller_items);
        }
    }
    else if (controller)
    {
        items = birth_controller_items;
        item_count = (int)N_ELEMENTS(birth_controller_items);
    }

    if (item_count > (int)N_ELEMENTS(text_widths))
        item_count = (int)N_ELEMENTS(text_widths);

#if SIL_SDL_MOBILE_BUILD
    if (touch_buttons)
        touch_button_pad_x = MAX(16.0f, h * 0.58f);
#endif

    for (int i = 0; i < item_count; i++)
    {
        text_widths[i] = sdl_char_sheet_text_width(font, items[i].label);
        item_widths[i] = (float)text_widths[i];
#if SIL_SDL_MOBILE_BUILD
        if (touch_buttons)
            item_widths[i] += touch_button_pad_x * 2.0f;
#endif
        total_w += item_widths[i];
        if (i + 1 < item_count)
            total_w += spacing;
    }
    if (total_w > w && item_count > 1)
    {
#if SIL_SDL_MOBILE_BUILD
        if (touch_buttons)
        {
            float max_gap = (w - (float)item_count)
                / (float)(item_count - 1);
            float fitted_width;

            /*
             * A touch action must never disappear merely because its natural
             * label width plus padding does not fit the row.  Keep every
             * registered action in one measured strip and let the centered
             * text renderer scale a long label inside its equal-width hit
             * target.  This is especially important for Back, which is last
             * on the live character sheet and was previously clipped on
             * narrow portrait phones.
             */
            spacing = MIN(spacing, MAX(0.0f, max_gap));
            fitted_width = (w - spacing * (float)(item_count - 1))
                / (float)item_count;
            fitted_width = MAX(1.0f, fitted_width);
            for (int i = 0; i < item_count; i++)
                item_widths[i] = fitted_width;
            total_w = fitted_width * (float)item_count
                + spacing * (float)(item_count - 1);
        }
        else
#endif
        {
            spacing =
#if SIL_SDL_MOBILE_BUILD
                MAX(4.0f,
#else
                MAX(6.0f,
#endif
                    (w - (total_w - spacing * (float)(item_count - 1)))
                        / (float)(item_count - 1));
        }
    }
#if SIL_SDL_MOBILE_BUILD
    if (touch_buttons && total_w < w)
        cursor_x = x + (w - total_w) * 0.5f;
#endif

    for (int i = 0; i < item_count; i++)
    {
        cptr label = items[i].label;
        int text_w = text_widths[i];
        float item_w = item_widths[i];
        int choice = items[i].choice;
        bool disabled = (choice == -2
            && !sdl_char_sheet_selected_choice_confirmable());
        bool focused = (choice >= 0)
            ? sdl_char_sheet_choice_focused(choice)
            : sdl_char_sheet_prompt_focused(choice);

        if (disabled)
            focused = false;

        if (!preview_prompt
#if SIL_SDL_MOBILE_BUILD
            && !touch_buttons
#endif
            && cursor_x + item_w > x + w)
        {
            break;
        }

        {
            SDL_FRect hit = { cursor_x, y, item_w + 4.0f, h };
#if SIL_SDL_MOBILE_BUILD
            if (!preview_prompt && !touch_buttons)
            {
                float pad_x = MAX(10.0f, h * 0.32f);

                hit.x = MAX(x, cursor_x - pad_x * 0.5f);
                hit.w = MIN(x + w - hit.x, (float)text_w + 4.0f + pad_x);
            }
            if (touch_buttons)
            {
                SDL_Color fill = disabled
                    ? (SDL_Color){ 76, 76, 76, 220 }
                    : (focused ? (SDL_Color){ 245, 245, 245, 255 }
                               : (SDL_Color){ 156, 156, 156, 238 });
                SDL_Color border = disabled
                    ? (SDL_Color){ 28, 28, 28, 210 }
                    : (focused ? (SDL_Color){ 0, 0, 0, 255 }
                               : (SDL_Color){ 28, 28, 28, 230 });

                hit.w = item_w;
                SDL_SetRenderDrawBlendMode(g_state.renderer,
                    SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(g_state.renderer, fill.r, fill.g,
                    fill.b, fill.a);
                SDL_RenderFillRect(g_state.renderer, &hit);
                SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g,
                    border.b, border.a);
                SDL_RenderRect(g_state.renderer, &hit);
            }
#endif
            if (preview_prompt)
            {
                SDL_Color fill = focused ? (SDL_Color){ 245, 245, 245, 255 }
                                         : (SDL_Color){ 156, 156, 156, 238 };
                SDL_Color border = focused ? (SDL_Color){ 0, 0, 0, 255 }
                                           : (SDL_Color){ 28, 28, 28, 230 };
                float pad_x = MAX(14.0f, h * 0.72f);

                hit.w = MIN(w, (float)text_w + pad_x * 2.0f);
                hit.x = x + (w - hit.w) * 0.5f;
                SDL_SetRenderDrawBlendMode(g_state.renderer,
                    SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(g_state.renderer, fill.r, fill.g,
                    fill.b, fill.a);
                SDL_RenderFillRect(g_state.renderer, &hit);
                SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g,
                    border.b, border.a);
                SDL_RenderRect(g_state.renderer, &hit);
            }
            if (focused && !preview_prompt
#if SIL_SDL_MOBILE_BUILD
                && !touch_buttons
#endif
                )
                sdl_char_sheet_draw_focus_rect(hit, true);
            if (preview_prompt
#if SIL_SDL_MOBILE_BUILD
                || touch_buttons
#endif
                )
            {
                (void)sdl_char_sheet_draw_button_text(font, label,
                    disabled ? TERM_L_DARK : TERM_DARK, &hit);
            }
            else
            {
                (void)sdl_char_sheet_draw_text(font, label, disabled
                    ? TERM_DARK : (focused ? TERM_DARK : TERM_L_WHITE),
                    cursor_x, y, hit.w, h, false);
            }
            if (disabled)
                continue;
            if (choice >= 0)
                sdl_char_sheet_add_hit(hit, choice, "", TERM_WHITE);
            else
                sdl_char_sheet_add_prompt_hit(hit, choice);
        }
        cursor_x += item_w + spacing;
    }
}

/*
 * Floating "Exit" overlay button for touch-only full-screen terminal menus
 * (inventory, equipment, supplies, knowledge/lore, abilities).  Those menus
 * blanket the screen with list/scroll/click cells, so a "tap away to exit"
 * affordance has nowhere to land.  This draws a single tappable button in the
 * bottom-right corner of the main view -- styled like the character sheet's
 * touch buttons -- and a tap injects ESCAPE to close.  A menu opts in each
 * frame via ui_menu_click_set_touch_exit_button(true).
 */
static int sdl_touch_exit_button_font_px(float button_h)
{
    return sdl_char_sheet_clampi((int)(button_h * 0.58f), 16, 52);
}

enum
{
    SDL_TOUCH_MENU_BUTTON_MAX = 10,
    SDL_TOUCH_MENU_EXIT_CHOICE = -0x3fffffff
};

typedef struct sdl_touch_menu_button_layout_entry
{
    SDL_FRect rect;
    int choice;
    byte attr;
    char label[32];
} sdl_touch_menu_button_layout_entry;

int sdl_touch_menu_button_reserved_rows(void)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    int rows;
    float content_h;
    float button_h;
    float margin;
    float covered_h;
    int reserved_rows;

    if (!sdl_touch_only_device_active())
    {
        return 0;
    }
    if (!view->term_ready || view->cell_h <= 0 || view->rect.h <= 0)
        return 0;

    rows = sdl_main_view_visual_rows(view);
    if (rows <= 0)
        return 0;

    content_h = (float)(rows * view->cell_h);
    button_h = sdl_char_sheet_clampf(content_h * 0.060f, 36.0f, 84.0f);
    margin = sdl_char_sheet_clampf(content_h * 0.018f, 8.0f, 26.0f);
    covered_h = button_h + margin;
    reserved_rows = (int)(covered_h / (float)view->cell_h);
    if ((float)(reserved_rows * view->cell_h) < covered_h)
        reserved_rows++;

    return MAX(reserved_rows, 1);
}

static int sdl_touch_menu_button_layout(
    sdl_touch_menu_button_layout_entry buttons[], int max_buttons)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    int cols;
    int rows;
    int count = 0;
    float widths[SDL_TOUCH_MENU_BUTTON_MAX];
    float content_x;
    float content_y;
    float content_w;
    float content_h;
    float bh;
    float gap;
    float margin;
    float pad_x;
    float total_w = 0.0f;
    float x;
    int font_px;
    TTF_Font* font;

    if (!buttons || max_buttons <= 0)
        return 0;
    if (!sdl_touch_only_device_active())
        return 0;
    if (!g_state.renderer)
        return 0;
    if (!view->term_ready || view->cell_w <= 0 || view->cell_h <= 0
        || view->rect.w <= 0 || view->rect.h <= 0)
        return 0;

    cols = sdl_main_view_visual_cols(view);
    rows = sdl_main_view_visual_rows(view);
    if (cols <= 0 || rows <= 0)
        return 0;

    content_x = (float)(view->rect.x + view->margin_x);
    content_y = (float)(view->rect.y + view->margin_y);
    content_w = (float)(cols * view->cell_w);
    content_h = (float)(rows * view->cell_h);

    bh = sdl_char_sheet_clampf(content_h * 0.060f, 36.0f, 84.0f);
    font_px = sdl_touch_exit_button_font_px(bh);
    font = sdl_story_font_for_height_slot(font_px, SDL_STORY_FONT_SLOT_MENU);
    pad_x = MAX(18.0f, bh * 0.62f);
    margin = sdl_char_sheet_clampf(content_h * 0.018f, 8.0f, 26.0f);
    gap = MAX(8.0f, margin * 0.60f);

    for (int i = 0; i < ui_menu_click_touch_button_count()
         && count < max_buttons && count < SDL_TOUCH_MENU_BUTTON_MAX; i++)
    {
        int choice = 0;
        cptr label = NULL;
        byte attr = TERM_DARK;

        if (!ui_menu_click_touch_button_get(i, &choice, &label, &attr)
            || !label || !label[0])
        {
            continue;
        }

        buttons[count].choice = choice;
        buttons[count].attr = attr;
        SDL_strlcpy(buttons[count].label, label, sizeof(buttons[count].label));
        count++;
    }

    if (ui_menu_click_touch_exit_button_active() && count < max_buttons
        && count < SDL_TOUCH_MENU_BUTTON_MAX)
    {
        buttons[count].choice = SDL_TOUCH_MENU_EXIT_CHOICE;
        buttons[count].attr = TERM_DARK;
        SDL_strlcpy(buttons[count].label, "Exit", sizeof(buttons[count].label));
        count++;
    }

    if (count <= 0)
        return 0;

    for (int i = 0; i < count; i++)
    {
        int text_w = font ? sdl_char_sheet_text_width(font, buttons[i].label)
                          : (int)(bh * 1.4f);

        widths[i] = (float)text_w + pad_x * 2.0f;
        if (widths[i] < bh * 1.55f)
            widths[i] = bh * 1.55f;
    }

    {
        float max_w = content_w - margin * 2.0f;

        total_w = 0.0f;
        for (int i = 0; i < count; i++)
        {
            if (i > 0)
                total_w += gap;
            total_w += widths[i];
        }

        if (total_w > max_w)
        {
            float max_gap = (count > 1)
                ? (max_w - (float)count) / (float)(count - 1)
                : 0.0f;
            float fitted_width;

            /*
             * These controls share the bottom row with Exit.  Dropping
             * leading controls until the suffix fit made menu actions vanish
             * on narrow phones.  Fit the complete registered set instead;
             * rendering and pointer handling both consume these rectangles.
             */
            gap = MIN(gap, MAX(0.0f, max_gap));
            fitted_width = (max_w - gap * (float)(count - 1))
                / (float)count;
            fitted_width = MAX(1.0f, fitted_width);
            for (int i = 0; i < count; i++)
                widths[i] = fitted_width;
            total_w = fitted_width * (float)count
                + gap * (float)(count - 1);
        }
    }

    x = content_x + content_w - margin - total_w;
    if (x < content_x + margin)
        x = content_x + margin;

    for (int i = 0; i < count; i++)
    {
        buttons[i].rect = (SDL_FRect){
            x, content_y + content_h - margin - bh, widths[i], bh
        };
        x += widths[i] + gap;
    }

    return count;
}

void sdl_touch_exit_button_render(void)
{
    sdl_touch_menu_button_layout_entry buttons[SDL_TOUCH_MENU_BUTTON_MAX];
    int count;
    int font_px;
    TTF_Font* font;

    count = sdl_touch_menu_button_layout(buttons, N_ELEMENTS(buttons));
    if (count <= 0)
        return;

    font_px = sdl_touch_exit_button_font_px(buttons[0].rect.h);
    font = sdl_story_font_for_height_slot(font_px, SDL_STORY_FONT_SLOT_MENU);

    for (int i = 0; i < count; i++)
    {
        /* Match the character sheet's default touch button chrome. */
        SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_state.renderer, 156, 156, 156, 238);
        SDL_RenderFillRect(g_state.renderer, &buttons[i].rect);
        SDL_SetRenderDrawColor(g_state.renderer, 28, 28, 28, 230);
        SDL_RenderRect(g_state.renderer, &buttons[i].rect);

        if (font)
            (void)sdl_char_sheet_draw_button_text(font, buttons[i].label,
                buttons[i].attr, &buttons[i].rect);
    }
}

bool sdl_touch_exit_button_handle_pointer(float x, float y)
{
    sdl_touch_menu_button_layout_entry buttons[SDL_TOUCH_MENU_BUTTON_MAX];
    int count;

    count = sdl_touch_menu_button_layout(buttons, N_ELEMENTS(buttons));
    if (count <= 0)
        return false;

    for (int i = 0; i < count; i++)
    {
        if (!sdl_point_in_frect(&buttons[i].rect, x, y))
            continue;

        ui_menu_click_clear_pending_hover();
        if (buttons[i].choice == SDL_TOUCH_MENU_EXIT_CHOICE)
        {
            Term_keypress(ESCAPE);
        }
        else
        {
            (void)ui_menu_click_handle_choice_action(buttons[i].choice,
                UI_MENU_CLICK_PRIMARY, NULL);
            Term_keypress(UI_MENU_CLICK_WAKE_KEY);
        }
        return true;
    }

    return false;
}

static void sdl_char_sheet_draw_book_page_controls(TTF_Font* prompt_font,
    float content_x, float content_w, float prompt_y, float prompt_h,
    int page, int page_count)
{
    float bw = MIN(content_w * 0.34f, prompt_h * 9.0f);
    float bh = prompt_h;
    int hov = g_sdl_character_sheet_screen.hover_choice;
#if SIL_SDL_MOBILE_BUILD
    cptr narrative_exit_verb =
        streq(g_sdl_character_sheet_screen.narrative_close_label, "Close")
            ? "close" : "proceed";
#endif

    if (!prompt_font || content_w <= 0.0f || prompt_h <= 0.0f)
        return;

    if (g_sdl_select_choice_page_only)
    {
        sdl_char_sheet_draw_prompt(prompt_font, "", content_x, prompt_y,
            content_w, prompt_h);
        return;
    }

#if SIL_SDL_MOBILE_BUILD
    if (!sdl_touch_only_device_active())
    {
        bool controller = steamdeck_controls_active();
        bool narrative = (g_sdl_character_sheet_screen.context
            == SDL_CHARACTER_SHEET_NARRATIVE);
        bool final_page = (page >= page_count - 1);
        char prompt[160];

        prompt[0] = '\0';
        if (controller)
        {
            char prev_label[16];
            char next_label[16];
            char confirm_label[16];
            char back_label[16];

            sdl_gamepad_action_binding_short_label(steamdeck_prev_page_key(),
                prev_label, sizeof(prev_label));
            if (streq(prev_label, "(unbound)") || streq(prev_label, "Multiple"))
                SDL_strlcpy(prev_label, "L1", sizeof(prev_label));
            sdl_gamepad_action_binding_short_label(steamdeck_next_page_key(),
                next_label, sizeof(next_label));
            if (streq(next_label, "(unbound)") || streq(next_label, "Multiple"))
                SDL_strlcpy(next_label, "R1", sizeof(next_label));
            sdl_gamepad_action_binding_short_label(steamdeck_confirm_key(),
                confirm_label, sizeof(confirm_label));
            if (streq(confirm_label, "(unbound)")
                || streq(confirm_label, "Multiple"))
            {
                SDL_strlcpy(confirm_label, "A", sizeof(confirm_label));
            }
            sdl_gamepad_action_binding_short_label(steamdeck_back_key(),
                back_label, sizeof(back_label));
            if (streq(back_label, "(unbound)") || streq(back_label, "Multiple"))
                SDL_strlcpy(back_label, "B", sizeof(back_label));

            if (final_page)
            {
                if (page > 0 && narrative)
                    strnfmt(prompt, sizeof(prompt),
                        "%s previous  %s continue  %s %s", prev_label,
                        confirm_label, back_label, narrative_exit_verb);
                else if (page > 0)
                    strnfmt(prompt, sizeof(prompt),
                        "%s previous  %s select  %s back", prev_label,
                        confirm_label, back_label);
                else
                    strnfmt(prompt, sizeof(prompt), "%s select  %s back",
                        confirm_label, back_label);
            }
            else if (page > 0)
            {
                strnfmt(prompt, sizeof(prompt),
                    "%s previous  %s/%s next  %s back", prev_label,
                    next_label, confirm_label, back_label);
            }
            else
            {
                strnfmt(prompt, sizeof(prompt), "%s/%s next  %s back",
                    next_label, confirm_label, back_label);
            }
        }
        else
        {
            if (final_page)
            {
                if (page > 0 && narrative)
                    strnfmt(prompt, sizeof(prompt),
                        "Left previous  Enter continue  Esc %s",
                        narrative_exit_verb);
                else if (page > 0)
                    strnfmt(prompt, sizeof(prompt),
                        "Left previous  Enter select  Esc back");
                else
                    strnfmt(prompt, sizeof(prompt), "Enter select  Esc back");
            }
            else if (page > 0)
            {
                strnfmt(prompt, sizeof(prompt),
                    "Left previous  Right/Enter next  Esc back");
            }
            else
            {
                strnfmt(prompt, sizeof(prompt), "Right/Enter next  Esc back");
            }
        }

        (void)sdl_char_sheet_draw_text(prompt_font, prompt, TERM_SLATE,
            content_x, prompt_y, content_w, prompt_h, true);
        if (page > 0)
        {
            SDL_FRect r = { content_x, prompt_y, content_w * 0.45f,
                prompt_h };
            sdl_char_sheet_add_select_button_hit(r,
                SDL_SELECT_CLICK_PAGE_PREV);
        }
        else if (g_sdl_character_sheet_screen.context
            == SDL_CHARACTER_SHEET_BIRTH_SELECT)
        {
            SDL_FRect r = { content_x, prompt_y, content_w * 0.45f,
                prompt_h };
            sdl_char_sheet_add_select_button_hit(r,
                SDL_SELECT_CLICK_PAGE_PREV);
        }
        if (page < page_count - 1 || narrative)
        {
            SDL_FRect r = { content_x + content_w * 0.55f, prompt_y,
                content_w * 0.45f, prompt_h };
            sdl_char_sheet_add_select_button_hit(r,
                SDL_SELECT_CLICK_PAGE_NEXT);
        }
        return;
    }
#endif

    if (page > 0)
    {
        SDL_FRect r = { content_x, prompt_y, bw, bh };
        byte a = (hov == SDL_SELECT_CLICK_PAGE_PREV)
            ? TERM_WHITE : TERM_L_BLUE;

        (void)sdl_char_sheet_draw_text(prompt_font,
            "\xe2\x80\xb9 Previous page", a, content_x, prompt_y, bw,
            bh, true);
        sdl_char_sheet_add_select_button_hit(r,
            SDL_SELECT_CLICK_PAGE_PREV);
    }
    else if (g_sdl_character_sheet_screen.context
        == SDL_CHARACTER_SHEET_BIRTH_SELECT)
    {
        SDL_FRect r = { content_x, prompt_y, bw, bh };
        byte a = (hov == SDL_SELECT_CLICK_PAGE_PREV)
            ? TERM_WHITE : TERM_L_BLUE;

        (void)sdl_char_sheet_draw_text(prompt_font, "Back", a, content_x,
            prompt_y, bw, bh, true);
        sdl_char_sheet_add_select_button_hit(r,
            SDL_SELECT_CLICK_PAGE_PREV);
    }
    if (page < page_count - 1)
    {
        SDL_FRect r = { content_x + content_w - bw, prompt_y, bw, bh };
        byte a = (hov == SDL_SELECT_CLICK_PAGE_NEXT)
            ? TERM_WHITE : TERM_L_BLUE;

        (void)sdl_char_sheet_draw_text(prompt_font,
            "Turn the page \xe2\x80\xba", a,
            content_x + content_w - bw, prompt_y, bw, bh, true);
        sdl_char_sheet_add_select_button_hit(r,
            SDL_SELECT_CLICK_PAGE_NEXT);
    }
    else if (g_sdl_character_sheet_screen.context
        == SDL_CHARACTER_SHEET_NARRATIVE)
    {
        SDL_FRect r = { content_x + content_w - bw, prompt_y, bw, bh };
        byte a = (hov == SDL_SELECT_CLICK_PAGE_NEXT)
            ? TERM_WHITE : TERM_L_BLUE;

        (void)sdl_char_sheet_draw_text(prompt_font,
            "Continue", a, content_x + content_w - bw, prompt_y, bw,
            bh, true);
        sdl_char_sheet_add_select_button_hit(r,
            SDL_SELECT_CLICK_PAGE_NEXT);
    }

    /* Narrative books may opt into an exit button.  The race book instead
     * offers the same central position as a direct shortcut to its final,
     * selectable page. */
    if (g_sdl_character_sheet_screen.narrative_close_enabled
        || (g_sdl_character_sheet_screen.context
            == SDL_CHARACTER_SHEET_BIRTH_SELECT
            && g_sdl_character_sheet_screen.select_book_mode
            && page < page_count - 1))
    {
        cptr close_label = g_sdl_character_sheet_screen.narrative_close_label;
        cptr display_close_label =
            g_sdl_narrative_portrait_rendering
                && close_label
                && streq(close_label, "Proceed to character creation")
            ? "Create character" : close_label;
        bool long_close_label =
            display_close_label && strlen(display_close_label) > 12;
        bool emphasized_close =
            g_sdl_character_sheet_screen.context
                == SDL_CHARACTER_SHEET_NARRATIVE
            && close_label && !streq(close_label, "Close");
        float close_h = emphasized_close ? bh * 1.12f : bh;
        float close_y = prompt_y - (close_h - bh) * 0.5f;
        float cw = long_close_label
            ? MIN(content_w * (emphasized_close ? 0.34f : 0.30f),
                close_h * 9.0f)
            : MIN(content_w * 0.22f, bh * 6.0f);
        float ccx = content_x + (content_w - cw) * 0.5f;
        SDL_FRect r = { ccx, close_y, cw, close_h };
        bool jump_to_last_page = (g_sdl_character_sheet_screen.context
            == SDL_CHARACTER_SHEET_BIRTH_SELECT);
        TTF_Font* close_font = prompt_font;
        byte a = (hov == SDL_SELECT_CLICK_CLOSE) ? TERM_WHITE
            : (jump_to_last_page ? TERM_L_BLUE
                : (emphasized_close ? TERM_L_GREEN : TERM_SLATE));
        cptr label = jump_to_last_page
            ? "Jump to last page" : display_close_label;

        if (emphasized_close)
        {
            TTF_Font* larger_font = sdl_story_font_for_height_slot(
                (int)(close_h + 0.5f), SDL_STORY_FONT_SLOT_DEFAULT);

            if (larger_font)
                close_font = larger_font;
        }
        (void)sdl_char_sheet_draw_text(close_font, label, a, ccx, close_y,
            cw, close_h, true);
        sdl_char_sheet_add_select_button_hit(r, SDL_SELECT_CLICK_CLOSE);
    }
}

void sdl_char_sheet_render_hover_tooltip(void);

TTF_Font* sdl_char_sheet_menu_font_for_rows(float available_h, int rows,
    int min_px, int max_px, float line_scale, float* out_line_h,
    int* out_px)
{
    int chosen_px = min_px;
    int low_px;
    int high_px;
    float chosen_line_h = 1.0f;
    TTF_Font* chosen_font = NULL;

    if (rows < 1)
        rows = 1;
    if (available_h < 1.0f)
        available_h = 1.0f;
    if (max_px < min_px)
        max_px = min_px;

    low_px = min_px;
    high_px = max_px;
    while (low_px <= high_px) {
        int px = low_px + (high_px - low_px) / 2;
        TTF_Font* font =
            sdl_story_font_for_height_slot(px, SDL_STORY_FONT_SLOT_MENU);
        float line_h;

        if (!font) {
            high_px = px - 1;
            continue;
        }

        line_h = sdl_char_sheet_line_h(font, px, line_scale);
        if (line_h * (float)rows <= available_h) {
            chosen_font = font;
            chosen_px = px;
            chosen_line_h = line_h;
            low_px = px + 1;
        } else {
            high_px = px - 1;
        }
    }

    if (!chosen_font) {
        chosen_font = sdl_story_font_for_height_slot(min_px,
            SDL_STORY_FONT_SLOT_MENU);
        chosen_px = min_px;
        chosen_line_h = sdl_char_sheet_line_h(chosen_font, min_px,
            line_scale);
    }

    if (chosen_line_h * (float)rows > available_h)
    {
        chosen_line_h = available_h / (float)rows;
        if (chosen_line_h < 1.0f)
            chosen_line_h = 1.0f;
    }

    if (out_line_h)
        *out_line_h = chosen_line_h;
    if (out_px)
        *out_px = chosen_px;

    return chosen_font;
}

int sdl_char_sheet_menu_max_cols(float content_w, int canvas_h, int row_count)
{
    float ar = (canvas_h > 1) ? content_w / (float)canvas_h : 1.6f;
    int cols = 1;
    int width_cols;

    if (ar >= 1.65f)
        cols = 4;
    else if (ar >= 1.10f)
        cols = 3;
    else
        cols = 2;

    if (row_count > 52 && ar >= 1.20f)
        cols = MAX(cols, 5);
    else if (row_count > 34 && ar >= 1.00f)
        cols = MAX(cols, 4);

    width_cols = (int)(content_w / 210.0f);
    if (width_cols < 1)
        width_cols = 1;
    if (cols > width_cols)
        cols = width_cols;
    if (cols > row_count)
        cols = row_count;
    if (cols < 1)
        cols = 1;

    return cols;
}

static void sdl_char_sheet_split_menu_row(cptr text, char* label,
    size_t label_sz, char* value, size_t value_sz)
{
    const char* tab;

    if (label && label_sz)
        label[0] = '\0';
    if (value && value_sz)
        value[0] = '\0';
    if (!text)
        return;

    tab = strchr(text, '\t');
    if (tab)
    {
        size_t label_len = (size_t)(tab - text);

        if (label && label_sz)
        {
            if (label_len >= label_sz)
                label_len = label_sz - 1;
            memcpy(label, text, label_len);
            label[label_len] = '\0';
        }
        if (value && value_sz)
            SDL_strlcpy(value, tab + 1, value_sz);
    }
    else if (label && label_sz)
    {
        SDL_strlcpy(label, text, label_sz);
    }
}

static float sdl_char_sheet_menu_pair_gap(float line_h)
{
    return sdl_char_sheet_clampf(line_h * 0.48f, 14.0f, 30.0f);
}

static float sdl_char_sheet_menu_row_natural_w(TTF_Font* font,
    TTF_Font* value_font, cptr text, float line_h)
{
    char label[160];
    char value[96];
    float w;

    if (!font || !text || !text[0])
        return 0.0f;

    sdl_char_sheet_split_menu_row(text, label, sizeof(label), value,
        sizeof(value));

    w = (float)sdl_char_sheet_text_width(font, label);
    if (value[0])
    {
        if (!value_font)
            value_font = font;
        w += sdl_char_sheet_menu_pair_gap(line_h)
            + (float)sdl_char_sheet_text_width(value_font, value);
    }

    return w;
}

static float sdl_char_sheet_menu_longest_row_w(TTF_Font* font,
    float line_h)
{
    TTF_Font* value_font;
    float longest = 0.0f;

    if (!font)
        return 0.0f;

    value_font = sdl_story_font_slot_sibling(font, SDL_STORY_FONT_SLOT_MENU);
    if (!value_font)
        value_font = font;

    for (int i = 0; i < g_sdl_character_sheet_screen.select_row_count; i++)
    {
        const sdl_character_sheet_select_row* r =
            &g_sdl_character_sheet_screen.select_rows[i];
        float w = sdl_char_sheet_menu_row_natural_w(font, value_font,
            r->label, line_h);

        if (sdl_char_sheet_menu_command_choice(r->choice))
            continue;
        if (w > longest)
            longest = w;
    }

    return longest;
}

static TTF_Font* sdl_char_sheet_menu_font_for_width(float content_w,
    int canvas_h, float* out_line_h, int* out_px)
{
#if SIL_SDL_MOBILE_BUILD
    int min_px = sdl_char_sheet_clampi((int)((float)canvas_h * 0.034f),
        24, 36);
    int max_px = sdl_char_sheet_clampi((int)((float)canvas_h * 0.058f),
        38, 68);
#else
    int min_px = sdl_char_sheet_clampi((int)((float)canvas_h * 0.028f),
        20, 28);
    int max_px = sdl_char_sheet_clampi((int)((float)canvas_h * 0.046f),
        30, 56);
#endif
    int chosen_px = min_px;
    int low_px;
    int high_px;
    float chosen_line_h = 1.0f;
    TTF_Font* chosen_font = NULL;

    if (max_px < min_px)
        max_px = min_px;
    if (content_w < 1.0f)
        content_w = 1.0f;

    low_px = min_px;
    high_px = max_px;
    while (low_px <= high_px) {
        int px = low_px + (high_px - low_px) / 2;
        TTF_Font* font =
            sdl_story_font_for_height_slot(px, SDL_STORY_FONT_SLOT_MENU);
        float line_h;
        float longest;

        if (!font) {
            high_px = px - 1;
            continue;
        }

        line_h = sdl_char_sheet_line_h(font, px, 1.30f);
        longest = sdl_char_sheet_menu_longest_row_w(font, line_h);
        if (longest <= content_w) {
            chosen_font = font;
            chosen_px = px;
            chosen_line_h = line_h;
            low_px = px + 1;
        } else {
            high_px = px - 1;
        }
    }

    if (!chosen_font) {
        chosen_font = sdl_story_font_for_height_slot(min_px,
            SDL_STORY_FONT_SLOT_MENU);
        chosen_px = min_px;
        chosen_line_h = sdl_char_sheet_line_h(chosen_font, min_px, 1.30f);
    }

    if (out_line_h)
        *out_line_h = chosen_line_h;
    if (out_px)
        *out_px = chosen_px;

    return chosen_font;
}

/*
 * Dynamic choice menus should read like menus, not like full-screen prose.
 * Start from the same font scale as the established pane/question menus, then
 * add only as many columns as are needed to fit the rows at that scale.
 */
static TTF_Font* sdl_char_sheet_menu_font_for_grid(float content_w,
    float available_h, int canvas_h, int row_count, int* out_cols,
    int* out_rows_per_col, float* out_col_gap, float* out_line_h,
    int* out_px)
{
    int max_cols = (row_count >= 12)
        ? sdl_char_sheet_menu_max_cols(content_w, canvas_h, row_count) : 1;
    /* Match the established pane/question-menu scale (see
     * sdl_char_sheet_menu_font_for_width) rather than a prose-sized cap. The
     * grid font is maximised to fit, so a tall panel with only a few rows would
     * otherwise balloon to the cap and dwarf the detail column beside it. */
#if SIL_SDL_MOBILE_BUILD
    int target_px = sdl_char_sheet_clampi(
        (int)((float)canvas_h * 0.058f), 38, 68);
#else
    int target_px = sdl_char_sheet_clampi(
        (int)((float)canvas_h * 0.046f), 30, 56);
#endif
    int min_px = sdl_char_sheet_clampi(sdl_main_menu_pane_font_px(),
        16, target_px);
    TTF_Font* chosen_font = NULL;
    int chosen_cols = 1;
    int chosen_rows = MAX(row_count, 1);
    int chosen_px = min_px;
    int low_px;
    int high_px;
    float chosen_gap = 0.0f;
    float chosen_line_h = 1.0f;

    if (content_w < 1.0f)
        content_w = 1.0f;
    if (available_h < 1.0f)
        available_h = 1.0f;
    if (max_cols < 1)
        max_cols = 1;

    low_px = min_px;
    high_px = target_px;
    while (low_px <= high_px) {
        int px = low_px + (high_px - low_px) / 2;
        TTF_Font* font = sdl_story_font_for_height_slot(px,
            SDL_STORY_FONT_SLOT_MENU);
        float line_h;
        float longest;
        float gap;
        int fit_cols = 0;
        int fit_rows = 0;

        if (!font) {
            high_px = px - 1;
            continue;
        }

        line_h = sdl_char_sheet_line_h(font, px, 1.18f);
        longest = sdl_char_sheet_menu_longest_row_w(font, line_h);
        gap = sdl_char_sheet_clampf(line_h * 0.72f, 14.0f, 34.0f);

        for (int cols = 1; cols <= max_cols; cols++)
        {
            int rows = (row_count + cols - 1) / cols;
            float col_w =
                (content_w - gap * (float)(cols - 1)) / (float)cols;

            if (col_w < 1.0f || longest > col_w)
                continue;
            if (line_h * (float)rows > available_h)
                continue;

            fit_cols = cols;
            fit_rows = rows;
            break;
        }

        if (fit_cols > 0) {
            chosen_font = font;
            chosen_cols = fit_cols;
            chosen_rows = fit_rows;
            chosen_px = px;
            chosen_gap = gap;
            chosen_line_h = line_h;
            low_px = px + 1;
        } else {
            high_px = px - 1;
        }
    }

    if (!chosen_font)
    {
        TTF_Font* font = sdl_story_font_for_height_slot(min_px,
            SDL_STORY_FONT_SLOT_MENU);
        float line_h = sdl_char_sheet_line_h(font, min_px, 1.18f);
        float longest = sdl_char_sheet_menu_longest_row_w(font, line_h);
        float gap = sdl_char_sheet_clampf(line_h * 0.72f, 14.0f, 34.0f);
        int best_cols = 1;
        int best_rows = MAX(row_count, 1);

        for (int cols = 1; cols <= max_cols; cols++)
        {
            int rows = (row_count + cols - 1) / cols;
            float col_w =
                (content_w - gap * (float)(cols - 1)) / (float)cols;

            if (col_w < longest)
                continue;
            best_cols = cols;
            best_rows = rows;
            if (line_h * (float)rows <= available_h)
                break;
        }

        chosen_font = font;
        chosen_cols = best_cols;
        chosen_rows = best_rows;
        chosen_px = min_px;
        chosen_gap = gap;
        chosen_line_h = line_h;
    }

    if (out_cols)
        *out_cols = chosen_cols;
    if (out_rows_per_col)
        *out_rows_per_col = chosen_rows;
    if (out_col_gap)
        *out_col_gap = chosen_gap;
    if (out_line_h)
        *out_line_h = chosen_line_h;
    if (out_px)
        *out_px = chosen_px;

    return chosen_font;
}

void sdl_char_sheet_draw_menu_row(TTF_Font* font, cptr text, byte attr,
    int choice, float x, float y, float w, float line_h, float value_col_x,
    int reset_choice, float reset_w)
{
    char label[160];
    char value[96];
    SDL_FRect hit;
    bool focused;
    float content_w = w;
    float btn_gap = sdl_char_sheet_clampf(line_h * 0.18f, 4.0f, 12.0f);

    if (!font || !text || !text[0] || w <= 0.0f || line_h <= 0.0f)
        return;

    if (reset_choice >= 0 && reset_w > btn_gap)
    {
        content_w = w - reset_w;
        if (content_w < w * 0.35f)
            content_w = w * 0.35f;
    }
    else
    {
        reset_choice = -1;
    }

    sdl_char_sheet_split_menu_row(text, label, sizeof(label), value,
        sizeof(value));

    hit = (SDL_FRect){ x, y, content_w, line_h };
    focused = sdl_char_sheet_choice_focused(choice);
    if (focused)
        sdl_char_sheet_draw_focus_rect(hit, true);

    if (value[0])
    {
        TTF_Font* value_font = sdl_story_font_slot_sibling(font,
            SDL_STORY_FONT_SLOT_MENU);
        int label_need = sdl_char_sheet_text_width(font, label);
        int value_need;
        float gap = sdl_char_sheet_menu_pair_gap(line_h);
        float label_w;
        float value_w;
        float value_x;

        if (!value_font)
            value_font = font;
        value_need = sdl_char_sheet_text_width(value_font, value);
        if (value_col_x > x && value_col_x < x + content_w)
        {
            /* Shared column: every row's value starts at the same x, so the
             * gap sits just past the longest label rather than the far right
             * edge.  The caller picks value_col_x for the whole list. */
            value_x = value_col_x;
            label_w = value_col_x - x - gap;
            if (label_w < 1.0f)
                label_w = 1.0f;
            value_w = (x + content_w) - value_x;
            if (value_w < 1.0f)
                value_w = 1.0f;
        }
        else if ((float)(label_need + value_need) + gap <= content_w)
        {
            value_w = (float)value_need;
            label_w = content_w - value_w - gap;
            value_x = x + label_w + gap;
        }
        else
        {
            value_w = sdl_char_sheet_clampf((float)value_need,
                MIN(content_w * 0.18f, 90.0f), content_w * 0.46f);
            label_w = content_w - value_w - gap;
            if (label_w < content_w * 0.48f)
            {
                label_w = content_w * 0.48f;
                value_w = content_w - label_w - gap;
            }
            if (value_w < 1.0f)
                value_w = 1.0f;
            value_x = x + label_w + gap;
        }

        (void)sdl_char_sheet_draw_text(font, label,
            focused ? TERM_DARK : TERM_WHITE, x, y, label_w,
            line_h * 0.94f, false);
        (void)sdl_char_sheet_draw_text(value_font, value,
            focused ? TERM_DARK : attr, value_x, y, value_w,
            line_h * 0.94f, false);
    }
    else
    {
        (void)sdl_char_sheet_draw_text(font, label,
            focused ? TERM_DARK : attr, x, y, content_w, line_h * 0.94f,
            false);
    }

    if (choice >= 0)
        sdl_char_sheet_add_hit(hit, choice, "", TERM_WHITE);

    if (reset_choice >= 0)
    {
        TTF_Font* btn_font = sdl_story_font_slot_sibling(font,
            SDL_STORY_FONT_SLOT_MENU);
        SDL_FRect btn = { x + content_w + btn_gap, y + line_h * 0.12f,
            reset_w - btn_gap, line_h * 0.76f };
        bool reset_hover =
            (g_sdl_character_sheet_screen.hover_choice == reset_choice);
        SDL_Color fill = reset_hover ? (SDL_Color){ 245, 245, 245, 255 }
                                     : (SDL_Color){ 116, 116, 116, 214 };
        SDL_Color border = reset_hover ? (SDL_Color){ 0, 0, 0, 255 }
                                       : (SDL_Color){ 28, 28, 28, 224 };

        if (!btn_font)
            btn_font = font;
        if (btn.w > 1.0f && btn.h > 1.0f)
        {
            SDL_FRect drawn;
            SDL_FRect label_rect = {
                btn.x + btn.w * 0.08f,
                btn.y + (btn.h - line_h * 0.62f) * 0.5f,
                btn.w * 0.84f,
                line_h * 0.62f
            };

            SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(g_state.renderer, fill.r, fill.g, fill.b,
                fill.a);
            SDL_RenderFillRect(g_state.renderer, &btn);
            SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g,
                border.b, border.a);
            SDL_RenderRect(g_state.renderer, &btn);

            drawn = sdl_char_sheet_draw_button_text(btn_font, "Reset",
                reset_hover ? TERM_DARK : TERM_L_WHITE, &label_rect);
            (void)drawn;

            /* No hover desc: a tooltip here would re-pop on every pointer
             * move over the small button and read as a blinking popup. */
            sdl_char_sheet_add_hit(btn, reset_choice, "", TERM_WHITE);
        }
    }
}

static void sdl_char_sheet_dynamic_menu_headings(char* list, size_t list_sz,
    char* detail, size_t detail_sz)
{
    cptr title = g_sdl_character_sheet_screen.select_title;

    SDL_strlcpy(list, "Choices", list_sz);
    SDL_strlcpy(detail, "Details", detail_sz);
    if (title && strstr(title, "Oath"))
    {
        SDL_strlcpy(list, "Available Oaths", list_sz);
        SDL_strlcpy(detail, "Oath Details", detail_sz);
    }
    else if (title && strstr(title, "Blessing"))
    {
        SDL_strlcpy(list, "Available Blessings", list_sz);
        SDL_strlcpy(detail, "Blessing Details", detail_sz);
    }
    else if (title && strstr(title, "Curse"))
    {
        SDL_strlcpy(list, "Available Curses", list_sz);
        SDL_strlcpy(detail, "Curse Details", detail_sz);
    }
}

static void sdl_char_sheet_dynamic_panel_heading(TTF_Font* font, cptr text,
    SDL_FRect rect, float line_h)
{
    SDL_Color divider = g_state.palette[TERM_SLATE];
    float y = rect.y + line_h;

    (void)sdl_char_sheet_draw_text(font, text, TERM_SLATE, rect.x, rect.y,
        rect.w, line_h * 0.92f, false);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, divider.r, divider.g,
        divider.b, 145);
    SDL_RenderLine(g_state.renderer, rect.x, y, rect.x + rect.w, y);
}

static int sdl_char_sheet_dynamic_detail_line_count(TTF_Font* font,
    float width)
{
    int count = 0;

    for (int i = 0;
         i < g_sdl_character_sheet_screen.select_detail_count; i++)
    {
        const sdl_character_sheet_select_detail* d =
            &g_sdl_character_sheet_screen.select_detail[i];

        count += sdl_char_sheet_wrap_text(font, d->text, width, NULL, 0);
    }
    return count;
}

static bool sdl_char_sheet_render_dynamic_choice_menu(TTF_Font* prompt_font,
    float content_x, float top_y, float content_w, float region_bottom,
    float prompt_y, float prompt_h, int canvas_h)
{
    int row_count = g_sdl_character_sheet_screen.select_row_count;
    int detail_count = g_sdl_character_sheet_screen.select_detail_count;
    float region_h = region_bottom - top_y;
    float panel_gap;
    float heading_h;
    int heading_px;
    TTF_Font* heading_font;
    SDL_FRect list_panel;
    SDL_FRect detail_panel;
    SDL_FRect list_body;
    SDL_FRect detail_body;
    bool wide;
    int column_count = 1;
    int rows_per_column = MAX(row_count, 1);
    float column_gap = 0.0f;
    float column_width;
    float row_line_h = 1.0f;
    int row_px = 16;
    TTF_Font* row_font;
    int selected_row = -1;
    int scroll;
    int max_scroll;
    char list_heading[64];
    char detail_heading[64];
    static int last_selected = -1000000;
    static int last_rows_per_column = -1;

    if (row_count <= 0 || detail_count <= 0 || region_h <= 1.0f)
        return false;

    panel_gap = sdl_char_sheet_clampf(content_w * 0.022f, 16.0f, 42.0f);
    heading_px = sdl_char_sheet_clampi(
        (int)((float)canvas_h * 0.034f), 22, 46);
    heading_font = sdl_story_font_for_height_slot(heading_px,
        SDL_STORY_FONT_SLOT_MENU);
    heading_h = sdl_char_sheet_line_h(heading_font, heading_px, 1.15f);
    wide = content_w >= region_h * 1.12f && content_w >= 700.0f;

    if (wide)
    {
        float list_fraction = (row_count > 10) ? 0.61f : 0.39f;

        list_panel = (SDL_FRect){ content_x, top_y,
            (content_w - panel_gap) * list_fraction, region_h };
        detail_panel = (SDL_FRect){
            list_panel.x + list_panel.w + panel_gap, top_y,
            content_w - list_panel.w - panel_gap, region_h
        };
    }
    else
    {
        float list_fraction = (row_count > 10) ? 0.62f : 0.46f;

        list_panel = (SDL_FRect){ content_x, top_y, content_w,
            (region_h - panel_gap) * list_fraction };
        detail_panel = (SDL_FRect){ content_x,
            list_panel.y + list_panel.h + panel_gap, content_w,
            region_h - list_panel.h - panel_gap };
    }

    list_body = list_panel;
    list_body.y += heading_h * 1.18f;
    list_body.h -= heading_h * 1.18f;
    detail_body = detail_panel;
    detail_body.y += heading_h * 1.18f;
    detail_body.h -= heading_h * 1.18f;
    if (list_body.h < 1.0f)
        list_body.h = 1.0f;
    if (detail_body.h < 1.0f)
        detail_body.h = 1.0f;

    row_font = sdl_char_sheet_menu_font_for_grid(list_body.w,
        list_body.h, canvas_h, row_count, &column_count,
        &rows_per_column, &column_gap, &row_line_h, &row_px);
    if (!row_font)
        return false;
    column_width = (list_body.w
        - column_gap * (float)(column_count - 1)) / (float)column_count;
    if (column_width < 1.0f)
        column_width = 1.0f;
    g_sdl_select_menu_rows_per_column = rows_per_column;

    for (int i = 0; i < row_count; i++)
    {
        const sdl_character_sheet_select_row* r =
            &g_sdl_character_sheet_screen.select_rows[i];

        if (!r->is_heading
            && r->choice == g_sdl_character_sheet_screen.selected_index)
        {
            selected_row = i;
            break;
        }
    }

    max_scroll = (int)(row_line_h * (float)rows_per_column
        - list_body.h + 0.999f);
    if (max_scroll < 0)
        max_scroll = 0;
    scroll = g_sdl_character_sheet_screen.sheet_scroll;
    if (scroll < 0)
        scroll = 0;
    if (scroll > max_scroll)
        scroll = max_scroll;
    if (selected_row >= 0
        && (last_selected != g_sdl_character_sheet_screen.selected_index
            || last_rows_per_column != rows_per_column))
    {
        int visual_row = selected_row % rows_per_column;
        float row_top = row_line_h * (float)visual_row;
        float row_bottom = row_top + row_line_h;

        if (row_top < (float)scroll)
            scroll = (int)row_top;
        else if (row_bottom > (float)scroll + list_body.h)
            scroll = (int)(row_bottom - list_body.h + 0.999f);
        if (scroll < 0)
            scroll = 0;
        if (scroll > max_scroll)
            scroll = max_scroll;
    }
    last_selected = g_sdl_character_sheet_screen.selected_index;
    last_rows_per_column = rows_per_column;
    g_sdl_character_sheet_screen.sheet_scroll = scroll;
    g_sdl_character_sheet_screen.sheet_scroll_max = max_scroll;
    g_sdl_character_sheet_screen.select_scroll_rect = list_body;
    g_sdl_character_sheet_screen.last_body_px = row_px;
    g_sdl_character_sheet_screen.last_body_line_h = row_line_h * 0.94f;

    sdl_char_sheet_dynamic_menu_headings(list_heading,
        sizeof(list_heading), detail_heading, sizeof(detail_heading));
    sdl_char_sheet_dynamic_panel_heading(heading_font, list_heading,
        list_panel, heading_h);
    sdl_char_sheet_dynamic_panel_heading(heading_font, detail_heading,
        detail_panel, heading_h);

    {
        bool had_clip = SDL_RenderClipEnabled(g_state.renderer);
        SDL_Rect old_clip;
        SDL_Rect clip = { (int)list_body.x, (int)list_body.y,
            (int)(list_body.w + 0.5f), (int)(list_body.h + 0.5f) };

        if (had_clip)
            SDL_GetRenderClipRect(g_state.renderer, &old_clip);
        SDL_SetRenderClipRect(g_state.renderer, &clip);
        for (int i = 0; i < row_count; i++)
        {
            const sdl_character_sheet_select_row* r =
                &g_sdl_character_sheet_screen.select_rows[i];
            int col = i / rows_per_column;
            int row = i % rows_per_column;
            float x = list_body.x
                + (float)col * (column_width + column_gap);
            float y = list_body.y + (float)row * row_line_h
                - (float)scroll;

            if (y + row_line_h <= list_body.y
                || y >= list_body.y + list_body.h)
            {
                continue;
            }
            sdl_char_sheet_draw_menu_row(row_font, r->label, r->attr,
                r->choice, x, y, column_width, row_line_h, 0.0f, -1, 0.0f);
        }
        SDL_SetRenderClipRect(g_state.renderer, had_clip ? &old_clip : NULL);
    }

    {
        int target_px = sdl_char_sheet_clampi(
            (int)((float)canvas_h * 0.044f), 24, 58);
        int min_px = sdl_char_sheet_clampi(sdl_main_menu_pane_font_px(),
            15, target_px);
        int detail_px = min_px;
        float detail_lh = 1.0f;
        float item_gap = 0.0f;
        TTF_Font* detail_font = NULL;
        int low_px = min_px;
        int high_px = target_px;

        while (low_px <= high_px) {
            int px = low_px + (high_px - low_px) / 2;
            TTF_Font* font = sdl_story_font_for_height_slot(px,
                SDL_STORY_FONT_SLOT_MENU);
            float lh;
            int lines;
            float needed;

            if (!font) {
                high_px = px - 1;
                continue;
            }
            lh = sdl_char_sheet_line_h(font, px, 1.16f);
            lines = sdl_char_sheet_dynamic_detail_line_count(font,
                detail_body.w);
            needed = (float)lines * lh
                + (float)(detail_count - 1) * lh * 0.34f;
            if (needed <= detail_body.h) {
                detail_font = font;
                detail_px = px;
                detail_lh = lh;
                item_gap = lh * 0.34f;
                low_px = px + 1;
            } else {
                high_px = px - 1;
            }
        }

        if (!detail_font) {
            detail_font = sdl_story_font_for_height_slot(min_px,
                SDL_STORY_FONT_SLOT_MENU);
            detail_px = min_px;
            detail_lh = sdl_char_sheet_line_h(detail_font, min_px, 1.16f);
            item_gap = detail_lh * 0.34f;
        }

        if (detail_font)
        {
            float y = detail_body.y;

            g_sdl_character_sheet_screen.last_desc_px = detail_px;
            g_sdl_character_sheet_screen.last_desc_line_h = detail_lh;
            for (int i = 0; i < detail_count; i++)
            {
                const sdl_character_sheet_select_detail* d =
                    &g_sdl_character_sheet_screen.select_detail[i];
                int lines = sdl_char_sheet_wrap_text(detail_font, d->text,
                    detail_body.w, NULL, 0);
                float h = detail_lh * (float)lines;

                if (y + detail_lh * 0.2f > detail_body.y + detail_body.h)
                    break;
                sdl_char_sheet_draw_wrapped(detail_font, d->text, d->attr,
                    detail_body.x, y, detail_body.w,
                    MIN(h + detail_lh * 0.25f,
                        detail_body.y + detail_body.h - y),
                    detail_lh, lines);
                y += h + item_gap;
            }
        }
    }

    sdl_char_sheet_draw_prompt(prompt_font, "", content_x, prompt_y,
        content_w, prompt_h);
    return true;
}

void sdl_char_sheet_render_menu_select(TTF_Font* prompt_font,
    float content_x, float top_y, float content_w, float region_bottom,
    float prompt_y, float prompt_h, int canvas_h)
{
    int row_count = g_sdl_character_sheet_screen.select_row_count;
    cptr desc = g_sdl_character_sheet_screen.select_description;
    cptr desc_measure =
        g_sdl_character_sheet_screen.select_desc_sizing[0]
            ? g_sdl_character_sheet_screen.select_desc_sizing : desc;
    float region_h = region_bottom - top_y;
    float desc_h = 0.0f;
    float desc_gap = 0.0f;
    int desc_lines = 0;
    int desc_measure_lines = 0;
    int desc_px = 0;
    float desc_line_h = 1.0f;
    TTF_Font* desc_font = NULL;
    int best_px = 0;
    float best_line_h = 1.0f;
    TTF_Font* best_font = NULL;
    float value_col_x = 0.0f;
    float reset_reserve = 0.0f;
    float list_content_w;
    float column_gap = 0.0f;
    float column_width;
    float row_region_h;
    int column_count = 1;
    int rows_per_column;
    int selected_row = -1;
    int scroll;
    int max_scroll;
    bool selected_changed;
    bool had_clip;
    SDL_Rect old_clip;
    SDL_Rect clip_rect;
    static int last_selected_index = -1000000;
    static int last_row_count = -1;
    static int last_rows_per_column = -1;

    if (row_count <= 0 || content_w <= 0.0f || region_h <= 0.0f)
        return;

    if (g_sdl_select_dynamic_description
        && sdl_char_sheet_render_dynamic_choice_menu(prompt_font, content_x,
            top_y, content_w, region_bottom, prompt_y, prompt_h, canvas_h))
    {
        return;
    }

    g_sdl_character_sheet_screen.last_body_px = 0;
    g_sdl_character_sheet_screen.last_body_line_h = 0.0f;
    g_sdl_character_sheet_screen.last_desc_px = 0;
    g_sdl_character_sheet_screen.last_desc_line_h = 0.0f;

    /* Resolve the menu row font up front so the help/description band can be
     * sized relative to it; a fixed canvas fraction looked tiny next to the
     * rows. */
    best_font = sdl_char_sheet_menu_font_for_width(content_w, canvas_h,
        &best_line_h, &best_px);
    if (!best_font)
    {
        best_px = 18;
        best_font = sdl_story_font_for_height_slot(18,
            SDL_STORY_FONT_SLOT_MENU);
        best_line_h = sdl_char_sheet_line_h(best_font, 18, 1.30f);
    }
    g_sdl_character_sheet_screen.last_body_px = best_px;
    g_sdl_character_sheet_screen.last_body_line_h = best_line_h * 0.94f;

    if (desc && desc[0] && g_sdl_select_dynamic_description)
    {
        int min_px = sdl_char_sheet_clampi(
            (int)((float)canvas_h * 0.014f), 10, 16);
        int max_px = sdl_char_sheet_clampi(
            (int)((float)canvas_h * 0.026f), 15, 30);
        float max_desc_h;
        TTF_Font* min_row_font;
        float min_row_h;

        desc_gap = sdl_char_sheet_clampf(region_h * 0.025f, 8.0f, 18.0f);
        min_row_font = sdl_story_font_for_height_slot(min_px,
            SDL_STORY_FONT_SLOT_MENU);
        min_row_h = sdl_char_sheet_line_h(min_row_font, min_px, 1.30f);
        max_desc_h = region_h - desc_gap - min_row_h * 1.45f;
        if (max_desc_h < min_row_h)
            max_desc_h = min_row_h;

        int low_px = min_px;
        int high_px = max_px;

        while (low_px <= high_px) {
            int px = low_px + (high_px - low_px) / 2;
            TTF_Font* font;
            float line_h;
            int measure_lines;
            int actual_lines;
            float measured_h;

            font = sdl_story_font_for_height_slot(px,
                SDL_STORY_FONT_SLOT_MENU);
            if (!font) {
                high_px = px - 1;
                continue;
            }
            line_h = sdl_char_sheet_line_h(font, px, 1.16f);
            measure_lines = sdl_char_sheet_wrap_text(font,
                desc_measure ? desc_measure : desc, content_w, NULL, 0);
            actual_lines = sdl_char_sheet_wrap_text(font, desc, content_w,
                NULL, 0);
            if (measure_lines < actual_lines)
                measure_lines = actual_lines;
            measured_h = line_h * (float)measure_lines;
            if (measured_h <= max_desc_h) {
                desc_font = font;
                desc_px = px;
                desc_line_h = line_h;
                desc_measure_lines = measure_lines;
                desc_lines = actual_lines;
                low_px = px + 1;
            } else {
                high_px = px - 1;
            }
        }

        if (!desc_font)
        {
            desc_px = min_px;
            desc_font = sdl_story_font_for_height_slot(desc_px,
                SDL_STORY_FONT_SLOT_MENU);
            desc_line_h = sdl_char_sheet_line_h(desc_font, desc_px, 1.16f);
            desc_measure_lines = sdl_char_sheet_wrap_text(desc_font,
                desc_measure ? desc_measure : desc, content_w, NULL, 0);
            desc_lines = sdl_char_sheet_wrap_text(desc_font, desc, content_w,
                NULL, 0);
            if (desc_measure_lines < desc_lines)
                desc_measure_lines = desc_lines;
        }

        desc_h = desc_line_h * (float)desc_measure_lines;
        if (desc_h > max_desc_h)
            desc_h = max_desc_h;
        g_sdl_character_sheet_screen.last_desc_px = desc_px;
        g_sdl_character_sheet_screen.last_desc_line_h = desc_line_h;
    }
    else if (desc && desc[0])
    {
        /* Help/description band sits beneath the menu rows.  Size it relative
         * to the row font (a fixed canvas fraction looked tiny next to the
         * larger rows). */
        int desc_measure_lines;

        desc_px = sdl_char_sheet_clampi((int)((float)best_px * 0.95f),
            16, best_px);
        desc_font = sdl_story_font_for_height_slot(desc_px,
            SDL_STORY_FONT_SLOT_MENU);
        desc_line_h = sdl_char_sheet_line_h(desc_font, desc_px, 1.16f);
        desc_lines = sdl_char_sheet_wrap_text(desc_font, desc, content_w,
            NULL, 0);
        desc_measure_lines = sdl_char_sheet_wrap_text(desc_font,
            desc_measure ? desc_measure : desc, content_w, NULL, 0);
        if (desc_measure_lines > desc_lines)
            desc_lines = desc_measure_lines;
        if (desc_lines > 4)
            desc_lines = 4;
        desc_h = desc_line_h * (float)desc_lines;
        desc_gap = sdl_char_sheet_clampf(region_h * 0.025f, 8.0f, 18.0f);
        g_sdl_character_sheet_screen.last_desc_px = desc_px;
        g_sdl_character_sheet_screen.last_desc_line_h = desc_line_h;
    }

    row_region_h = region_h - desc_h - desc_gap;
    if (!g_sdl_select_dynamic_description
        && row_region_h < region_h * 0.55f)
    {
        row_region_h = region_h * 0.55f;
    }
    if (row_region_h < 1.0f)
        row_region_h = 1.0f;

    /* Reserve a right-hand strip for per-row "Reset" buttons when any row asks
     * for one, so the buttons line up and never overlap a row's value. */
    for (int i = 0; i < row_count; i++)
    {
        if (g_sdl_character_sheet_screen.select_rows[i].reset_choice >= 0)
        {
            reset_reserve = sdl_char_sheet_clampf(content_w * 0.20f, 56.0f,
                132.0f);
            break;
        }
    }
    list_content_w = content_w - reset_reserve;
    if (list_content_w < content_w * 0.45f)
    {
        list_content_w = content_w * 0.45f;
        reset_reserve = content_w - list_content_w;
    }

    rows_per_column = row_count;
    if (g_sdl_select_dynamic_description)
    {
        best_font = sdl_char_sheet_menu_font_for_grid(list_content_w,
            row_region_h, canvas_h, row_count, &column_count,
            &rows_per_column, &column_gap, &best_line_h, &best_px);
        if (!best_font)
        {
            best_px = 18;
            best_font = sdl_story_font_for_height_slot(18,
                SDL_STORY_FONT_SLOT_MENU);
            best_line_h = sdl_char_sheet_line_h(best_font, 18, 1.22f);
            column_count = 1;
            rows_per_column = row_count;
            column_gap = 0.0f;
        }
        g_sdl_character_sheet_screen.last_body_px = best_px;
        g_sdl_character_sheet_screen.last_body_line_h =
            best_line_h * 0.94f;
    }
    if (rows_per_column < 1)
        rows_per_column = 1;
    if (column_count < 1)
        column_count = 1;
    column_width = (list_content_w
        - column_gap * (float)(column_count - 1)) / (float)column_count;
    if (column_width < 1.0f)
        column_width = 1.0f;
    g_sdl_select_menu_rows_per_column = rows_per_column;

    for (int i = 0; i < row_count; i++)
    {
        const sdl_character_sheet_select_row* r =
            &g_sdl_character_sheet_screen.select_rows[i];

        if (!r->is_heading
            && r->choice == g_sdl_character_sheet_screen.selected_index)
        {
            selected_row = i;
            break;
        }
    }

    max_scroll = (int)(best_line_h * (float)rows_per_column
        - row_region_h + 0.999f);
    if (max_scroll < 0)
        max_scroll = 0;
    scroll = g_sdl_character_sheet_screen.sheet_scroll;
    if (scroll < 0)
        scroll = 0;
    if (scroll > max_scroll)
        scroll = max_scroll;
    selected_changed =
        (g_sdl_character_sheet_screen.selected_index != last_selected_index)
        || (row_count != last_row_count)
        || (rows_per_column != last_rows_per_column);
    if (selected_row >= 0 && selected_changed)
    {
        int visual_row = selected_row % rows_per_column;
        float row_top = best_line_h * (float)visual_row;
        float row_bottom = row_top + best_line_h;

        if (row_top < (float)scroll)
            scroll = (int)row_top;
        else if (row_bottom > (float)scroll + row_region_h)
            scroll = (int)(row_bottom - row_region_h + 0.999f);
        if (scroll < 0)
            scroll = 0;
        if (scroll > max_scroll)
            scroll = max_scroll;
    }
    g_sdl_character_sheet_screen.sheet_scroll = scroll;
    g_sdl_character_sheet_screen.sheet_scroll_max = max_scroll;
    /* Publish the scrollable list region so a touch drag here pans the list
     * (without selecting) instead of moving the highlight. */
    g_sdl_character_sheet_screen.select_scroll_rect =
        (SDL_FRect){ content_x, top_y, content_w, row_region_h };
    last_selected_index = g_sdl_character_sheet_screen.selected_index;
    last_row_count = row_count;
    last_rows_per_column = rows_per_column;

    /*
     * Align every row's value to a single column placed just past the widest
     * label, keeping a reasonable, screen-scaled gap instead of shoving the
     * values against the far right edge.  value_col_x stays 0 (per-row right
     * align) when no row has a value or when the list is too wide to fit.
     */
    if (column_count == 1)
    {
        TTF_Font* value_font = sdl_story_font_slot_sibling(best_font,
            SDL_STORY_FONT_SLOT_MENU);
        float max_label_w = 0.0f;
        float max_value_w = 0.0f;
        bool any_value = false;

        if (!value_font)
            value_font = best_font;
        for (int i = 0; i < row_count; i++)
        {
            const sdl_character_sheet_select_row* r =
                &g_sdl_character_sheet_screen.select_rows[i];
            char label[160];
            char value[96];
            float lw;
            float vw;

            if (r->is_heading)
                continue;
            sdl_char_sheet_split_menu_row(r->label, label, sizeof(label),
                value, sizeof(value));
            if (!value[0])
                continue;

            any_value = true;
            lw = (float)sdl_char_sheet_text_width(best_font, label);
            vw = (float)sdl_char_sheet_text_width(value_font, value);
            if (lw > max_label_w)
                max_label_w = lw;
            if (vw > max_value_w)
                max_value_w = vw;
        }

        if (any_value)
        {
            float gap = sdl_char_sheet_clampf(content_w * 0.06f, 24.0f, 140.0f);
            float min_gap = sdl_char_sheet_menu_pair_gap(best_line_h);

            value_col_x = content_x + max_label_w + gap;
            if (value_col_x + max_value_w > content_x + list_content_w)
                value_col_x = content_x + list_content_w - max_value_w;
            if (value_col_x < content_x + max_label_w + min_gap)
                value_col_x = 0.0f;
        }
    }

    had_clip = SDL_RenderClipEnabled(g_state.renderer);
    if (had_clip)
        SDL_GetRenderClipRect(g_state.renderer, &old_clip);
    clip_rect = (SDL_Rect){
        (int)content_x, (int)top_y,
        (int)(content_w + 0.5f), (int)(row_region_h + 0.5f)
    };
    SDL_SetRenderClipRect(g_state.renderer, &clip_rect);

    for (int i = 0; i < row_count; i++)
    {
        const sdl_character_sheet_select_row* r =
            &g_sdl_character_sheet_screen.select_rows[i];
        int col = i / rows_per_column;
        int row = i % rows_per_column;
        float x = content_x
            + (float)col * (column_width + column_gap);
        float y = top_y + best_line_h * (float)row - (float)scroll;

        if (y + best_line_h <= top_y)
            continue;
        if (y >= top_y + row_region_h)
            continue;

        if (r->is_heading)
        {
            (void)sdl_char_sheet_draw_text(best_font, r->label,
                TERM_SLATE, x, y, column_width, best_line_h * 0.90f,
                false);
        }
        else
        {
            sdl_char_sheet_draw_menu_row(best_font, r->label, r->attr,
                r->choice, x, y, column_width, best_line_h,
                (column_count == 1) ? value_col_x : 0.0f,
                r->reset_choice, (column_count == 1) ? reset_reserve : 0.0f);
        }
    }

    SDL_SetRenderClipRect(g_state.renderer, had_clip ? &old_clip : NULL);

    if (desc_font && desc && desc[0] && desc_lines > 0)
    {
        float y = top_y + row_region_h + desc_gap;

        if (g_sdl_select_dynamic_description)
        {
            SDL_Color divider = g_state.palette[TERM_SLATE];
            float divider_y = y - desc_gap * 0.5f;

            SDL_SetRenderDrawBlendMode(g_state.renderer,
                SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(g_state.renderer, divider.r, divider.g,
                divider.b, 150);
            SDL_RenderLine(g_state.renderer, content_x, divider_y,
                content_x + content_w, divider_y);
        }
        sdl_char_sheet_draw_wrapped(desc_font, desc, TERM_L_WHITE, content_x,
            y, content_w, desc_h + desc_line_h * 0.25f, desc_line_h,
            desc_lines);
    }

    sdl_char_sheet_draw_prompt(prompt_font, "", content_x, prompt_y,
        content_w, prompt_h);
    sdl_char_sheet_render_hover_tooltip();
}

bool sdl_char_sheet_birth_context(void)
{
    return g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_BIRTH_STATS
        || g_sdl_character_sheet_screen.context
            == SDL_CHARACTER_SHEET_BIRTH_SKILLS;
}

typedef struct sdl_char_sheet_tooltip_run {
    int start;
    int len;
    int line;
    float x;
    byte attr;
} sdl_char_sheet_tooltip_run;

enum { SDL_CHAR_SHEET_TOOLTIP_MAX_RUNS = 128 };

static bool sdl_char_sheet_tooltip_word_char(char ch)
{
    return isalnum((unsigned char)ch) || ch == '_' || ch == '-';
}

static void sdl_char_sheet_tooltip_mark_phrase(cptr text, byte* attrs,
    size_t attrs_len, cptr phrase, byte attr)
{
    size_t text_len;
    size_t phrase_len;

    if (!text || !attrs || !phrase || !phrase[0] || attrs_len == 0)
        return;

    text_len = MIN(strlen(text), attrs_len);
    phrase_len = strlen(phrase);
    if (phrase_len > text_len)
        return;

    for (size_t pos = 0; pos + phrase_len <= text_len; pos++)
    {
        if (pos > 0 && sdl_char_sheet_tooltip_word_char(text[pos - 1]))
            continue;
        if (SDL_strncasecmp(text + pos, phrase, phrase_len) != 0)
            continue;
        if (pos + phrase_len < text_len
            && sdl_char_sheet_tooltip_word_char(text[pos + phrase_len]))
        {
            continue;
        }
        for (size_t i = pos; i < pos + phrase_len; i++)
            attrs[i] = attr;
        pos += phrase_len - 1;
    }
}

static bool sdl_char_sheet_tooltip_value_char(char ch)
{
    return isdigit((unsigned char)ch) || ch == '+' || ch == '-'
        || ch == '.' || ch == ',' || ch == '/' || ch == '%'
        || ch == 'd' || ch == 'D' || ch == '(' || ch == ')'
        || ch == '[' || ch == ']';
}

static void sdl_char_sheet_tooltip_mark_value_before(cptr text, byte* attrs,
    size_t attrs_len, cptr phrase, byte attr)
{
    size_t text_len;
    size_t phrase_len;
    const char* search;

    if (!text || !attrs || !phrase || !phrase[0] || attrs_len == 0)
        return;
    text_len = MIN(strlen(text), attrs_len);
    phrase_len = strlen(phrase);
    search = text;

    while (search && *search)
    {
        const char* match = SDL_strcasestr(search, phrase);
        size_t end;
        size_t start;

        if (!match)
            break;
        end = MIN((size_t)(match - text), text_len);
        while (end > 0 && isspace((unsigned char)text[end - 1]))
            end--;
        start = end;
        while (start > 0
            && sdl_char_sheet_tooltip_value_char(text[start - 1]))
        {
            start--;
        }
        for (size_t i = start; i < end; i++)
            attrs[i] = attr;
        search = match + phrase_len;
    }
}

static void sdl_char_sheet_tooltip_mark_value_after(cptr text, byte* attrs,
    size_t attrs_len, cptr phrase, byte attr)
{
    size_t text_len;
    size_t phrase_len;
    const char* search;

    if (!text || !attrs || !phrase || !phrase[0] || attrs_len == 0)
        return;
    text_len = MIN(strlen(text), attrs_len);
    phrase_len = strlen(phrase);
    search = text;

    while (search && *search)
    {
        const char* match = SDL_strcasestr(search, phrase);
        size_t start;
        size_t end;

        if (!match)
            break;
        start = MIN((size_t)(match - text) + phrase_len, text_len);
        while (start < text_len && isspace((unsigned char)text[start]))
            start++;
        end = start;
        while (end < text_len
            && sdl_char_sheet_tooltip_value_char(text[end]))
        {
            end++;
        }
        for (size_t i = start; i < end; i++)
            attrs[i] = attr;
        search = match + phrase_len;
    }
}

static void sdl_char_sheet_tooltip_attrs(cptr text, byte subject_attr,
    byte* attrs, size_t attrs_len)
{
    static const struct {
        cptr phrase;
        byte attr;
    } rules[] = {
        { "Click/tap", TERM_L_BLUE },
        { "right-click", TERM_L_BLUE },
        { "long tap", TERM_L_BLUE },
        { "Tap", TERM_L_BLUE },
        { "selected row", TERM_L_BLUE },
        { "increase", TERM_L_GREEN },
        { "decrease", TERM_ORANGE },
        { "Unspent XP", TERM_L_GREEN },
        { "unspent", TERM_L_GREEN },
        { "earned", TERM_L_BLUE },
        { "increasing cost", TERM_YELLOW },
        { "skills", TERM_L_BLUE },
        { "abilities", TERM_VIOLET },
        { "Strength", TERM_L_BLUE },
        { "costs 1 speed", TERM_ORANGE },
        { "above 150%", TERM_L_RED },
        { "cannot move or pick up more", TERM_L_RED },
        { "Inventory", TERM_L_BLUE },
        { "supplies", TERM_L_BLUE },
        { "lamp oil", TERM_YELLOW },
        { "maximum", TERM_L_GREEN },
        { "minimum-depth", TERM_YELLOW },
        { "minimum", TERM_YELLOW },
        { "current", TERM_L_BLUE },
        { "progress", TERM_L_BLUE },
        { "stairs", TERM_L_BLUE },
        { "force you deeper", TERM_L_RED },
        { "50-ft rise", TERM_YELLOW },
        { "extra depth", TERM_ORANGE },
        { "carried Deep Call items", TERM_ORANGE },
        { "regeneration", TERM_L_GREEN },
        { "timed effects", TERM_YELLOW },
        { "minimum-depth pressure", TERM_ORANGE },
        { "illuminates nearby tiles", TERM_YELLOW },
        { "helps you see", TERM_L_GREEN },
        { "monsters notice you", TERM_L_RED },
        { "attack score", TERM_L_GREEN },
        { "enemy Evasion", TERM_L_BLUE },
        { "Evasion", TERM_L_BLUE },
        { "Damage", TERM_L_RED },
        { "base weapon damage", TERM_ORANGE },
        { "base bow damage", TERM_ORANGE },
        { "base damage", TERM_ORANGE },
        { "protection", TERM_L_BLUE },
        { "Criticals", TERM_ORANGE },
        { "slays", TERM_VIOLET },
        { "second main-hand attack", TERM_L_GREEN },
        { "offhand", TERM_L_BLUE },
        { "Range", TERM_YELLOW },
        { "physical damage", TERM_L_RED },
        { "hit lands", TERM_L_RED },
        { "hit points", TERM_L_BLUE },
        { "reaching 0 is fatal", TERM_L_RED },
        { "Constitution", TERM_L_BLUE },
        { "maximum Health", TERM_L_GREEN },
        { "resting", TERM_L_GREEN },
        { "restore", TERM_L_GREEN },
        { "song points", TERM_L_GREEN },
        { "Singing", TERM_L_GREEN },
        { "spends current Voice", TERM_YELLOW },
        { "does not regenerate", TERM_ORANGE },
        { "Grace", TERM_L_BLUE },
        { "maximum Voice", TERM_L_GREEN },
        { "primary song", TERM_L_GREEN },
        { "minor theme", TERM_L_BLUE },
        { "reduced Song skill", TERM_ORANGE },
        { "Voice cost", TERM_YELLOW },
        { "synergy pair", TERM_VIOLET },
        { "affinity", TERM_L_GREEN },
        { "resistance", TERM_L_GREEN },
        { "vulnerable", TERM_L_RED },
        { "vulnerability", TERM_L_RED },
        { "penalty", TERM_L_RED },
        { "dangerous", TERM_L_RED },
        { "endanger", TERM_L_RED },
        { "cursed", TERM_UMBER },
        { "curse", TERM_UMBER },
    };
    size_t text_len;
    const char* colon;
    const char* cost;

    if (!text || !attrs || attrs_len == 0)
        return;

    text_len = MIN(strlen(text), attrs_len);
    SDL_memset(attrs, TERM_WHITE, attrs_len);
    if (subject_attr == TERM_WHITE || subject_attr == TERM_L_WHITE
        || subject_attr == TERM_SLATE || subject_attr == TERM_L_DARK)
    {
        subject_attr = TERM_L_BLUE;
    }

    /* Every sheet popup starts with its subject before the first colon.  Keep
     * that label in the row's own semantic colour (skill family, trait state,
     * or vital state) rather than tinting the entire explanation. */
    colon = strchr(text, ':');
    if (colon && (size_t)(colon - text) < text_len)
    {
        size_t end = (size_t)(colon - text) + 1;

        for (size_t i = 0; i < end; i++)
            attrs[i] = subject_attr;
    }

    for (int i = 0; i < (int)N_ELEMENTS(rules); i++)
        sdl_char_sheet_tooltip_mark_phrase(text, attrs, text_len,
            rules[i].phrase, rules[i].attr);

    /* Tie the displayed vital amounts to the concepts that explain them. */
    sdl_char_sheet_tooltip_mark_value_before(text, attrs, text_len,
        "unspent", TERM_L_GREEN);
    sdl_char_sheet_tooltip_mark_value_before(text, attrs, text_len,
        "earned", TERM_L_BLUE);
    sdl_char_sheet_tooltip_mark_value_before(text, attrs, text_len,
        "lb carried", subject_attr);
    sdl_char_sheet_tooltip_mark_value_before(text, attrs, text_len,
        "ft current", TERM_L_BLUE);
    sdl_char_sheet_tooltip_mark_value_before(text, attrs, text_len,
        "ft minimum", TERM_YELLOW);
    sdl_char_sheet_tooltip_mark_value_before(text, attrs, text_len,
        "per turn", TERM_ORANGE);
    sdl_char_sheet_tooltip_mark_value_before(text, attrs, text_len,
        "base", TERM_L_BLUE);
    sdl_char_sheet_tooltip_mark_value_before(text, attrs, text_len,
        "from extra depth", TERM_ORANGE);
    sdl_char_sheet_tooltip_mark_value_before(text, attrs, text_len,
        "toward", TERM_L_BLUE);
    sdl_char_sheet_tooltip_mark_value_before(text, attrs, text_len,
        "player turns", TERM_L_BLUE);
    sdl_char_sheet_tooltip_mark_value_before(text, attrs, text_len,
        "The first value", TERM_YELLOW);
    sdl_char_sheet_tooltip_mark_value_before(text, attrs, text_len,
        "hit points", TERM_L_BLUE);
    sdl_char_sheet_tooltip_mark_value_before(text, attrs, text_len,
        "song points", TERM_L_GREEN);
    sdl_char_sheet_tooltip_mark_value_after(text, attrs, text_len,
        "radius", TERM_YELLOW);
    sdl_char_sheet_tooltip_mark_value_after(text, attrs, text_len,
        "Progress is", TERM_L_BLUE);

    /* Keep the price label distinct from action words, and reserve yellow for
     * the actual amount.  This avoids turning most of an allocation tooltip
     * green while still making the decision-critical number easy to scan. */
    cost = SDL_strcasestr(text, "Cost to raise now:");
    if (cost)
    {
        size_t start = (size_t)(cost - text);
        size_t value = start + strlen("Cost to raise now:");
        size_t end;

        for (size_t i = start; i < value && i < text_len; i++)
            attrs[i] = TERM_L_BLUE;
        while (value < text_len && isspace((unsigned char)text[value]))
            value++;
        end = value;
        while (end < text_len && text[end] != '.'
            && !isspace((unsigned char)text[end]))
        {
            end++;
        }
        for (size_t i = value; i < end; i++)
            attrs[i] = TERM_YELLOW;
    }
}

static float sdl_char_sheet_tooltip_measure_text(TTF_Font* font, cptr text,
    int len)
{
    int advance = 0;

    if (!font || !text || len <= 0)
        return 0.0f;
    len = utf8_safe_prefix_len(text, len);
    if (len <= 0)
        return 0.0f;
    TTF_MeasureString(font, text, (size_t)len, 0, &advance, NULL);
    return (float)advance;
}

static int sdl_char_sheet_tooltip_layout(TTF_Font* font, cptr text,
    const byte* attrs, float max_w, sdl_char_sheet_tooltip_run* runs,
    int max_runs, int* out_lines, float* out_w)
{
    int len = (int)strlen(text);
    int pos = 0;
    int line = 0;
    int run_count = 0;
    float line_w = 0.0f;
    float widest = 0.0f;
    float space_w = sdl_char_sheet_tooltip_measure_text(font, " ", 1);

    while (pos < len && run_count < max_runs)
    {
        int word_start;
        int word_end;
        float word_w;

        while (pos < len && isspace((unsigned char)text[pos]))
        {
            if (text[pos] == '\n' && line_w > 0.0f)
            {
                widest = MAX(widest, line_w);
                line++;
                line_w = 0.0f;
            }
            pos++;
        }
        if (pos >= len)
            break;

        word_start = pos;
        while (pos < len && !isspace((unsigned char)text[pos]))
            pos++;
        word_end = pos;
        word_w = sdl_char_sheet_tooltip_measure_text(font,
            text + word_start, word_end - word_start);

        if (line_w > 0.0f && line_w + space_w + word_w > max_w)
        {
            widest = MAX(widest, line_w);
            line++;
            line_w = 0.0f;
        }
        else if (line_w > 0.0f)
            line_w += space_w;

        for (int sub = word_start; sub < word_end && run_count < max_runs; )
        {
            int sub_end = sub + 1;
            float sub_w;

            while (sub_end < word_end && attrs[sub_end] == attrs[sub])
                sub_end++;
            sub_w = sdl_char_sheet_tooltip_measure_text(font, text + sub,
                sub_end - sub);
            runs[run_count++] = (sdl_char_sheet_tooltip_run){
                .start = sub,
                .len = sub_end - sub,
                .line = line,
                .x = line_w,
                .attr = attrs[sub],
            };
            line_w += sub_w;
            sub = sub_end;
        }
    }

    widest = MAX(widest, line_w);
    if (out_lines)
        *out_lines = (run_count > 0) ? line + 1 : 0;
    if (out_w)
        *out_w = widest;
    return run_count;
}

cptr sdl_char_sheet_hover_desc(SDL_FRect* out_rect, byte* out_attr)
{
    int choice = g_sdl_character_sheet_screen.hover_choice;
    const sdl_character_sheet_live_item* item;

    if (out_rect)
        *out_rect = (SDL_FRect){ 0 };
    if (out_attr)
        *out_attr = TERM_L_BLUE;
    if (choice < 0)
        return "";

    item = sdl_char_sheet_live_item_by_choice(choice);
    for (int i = 0; i < g_sdl_character_sheet_screen.hit_count; i++)
    {
        const sdl_character_sheet_hit* hit =
            &g_sdl_character_sheet_screen.hits[i];

        if (hit->choice != choice)
            continue;

        if (out_rect)
            *out_rect = hit->rect;
        if (out_attr)
            *out_attr = hit->attr;
        if (item && item->desc[0])
            return item->desc;
        return hit->desc;
    }

    return item ? item->desc : "";
}

static bool sdl_char_sheet_hover_tooltip_contains(float x, float y)
{
    return g_sdl_char_sheet_hover_tooltip_choice
            == g_sdl_character_sheet_screen.hover_choice
        && g_sdl_char_sheet_hover_tooltip_choice
            != SDL_CHAR_SHEET_NO_HOVER
        && g_sdl_char_sheet_hover_tooltip_rect.w > 0.0f
        && g_sdl_char_sheet_hover_tooltip_rect.h > 0.0f
        && sdl_point_in_frect(&g_sdl_char_sheet_hover_tooltip_rect, x, y);
}

static bool sdl_char_sheet_dismiss_hover_tooltip_at(float x, float y)
{
    if (!sdl_char_sheet_hover_tooltip_contains(x, y))
        return false;

    g_sdl_character_sheet_screen.hover_choice = SDL_CHAR_SHEET_NO_HOVER;
    g_sdl_char_sheet_hover_tooltip_rect = (SDL_FRect){ 0 };
    g_sdl_char_sheet_hover_tooltip_choice = SDL_CHAR_SHEET_NO_HOVER;
    ui_menu_click_clear_pending_hover();
    g_state.need_present = true;
    return true;
}

void sdl_char_sheet_render_hover_tooltip(void)
{
    SDL_FRect anchor;
    SDL_Rect screen;
    TTF_Font* font;
    SDL_FRect box;
    byte attrs[256];
    byte subject_attr = TERM_L_BLUE;
    sdl_char_sheet_tooltip_run runs[SDL_CHAR_SHEET_TOOLTIP_MAX_RUNS];
    cptr desc = sdl_char_sheet_hover_desc(&anchor, &subject_attr);
    float pad;
    float gap;
    float margin;
    float max_box_w;
    float max_text_w;
    float text_w = 0.0f;
    int font_px;
    int line_h;
    int line_count = 0;
    int run_count;

    if (!desc || !desc[0] || !g_state.renderer || anchor.w <= 0.0f
        || anchor.h <= 0.0f)
    {
        return;
    }

    screen = sdl_get_window_pixel_rect();
    if (!sdl_rect_has_area(&screen))
        return;

    if (g_sdl_character_sheet_screen.last_desc_px > 0)
    {
        int max_px = sdl_char_sheet_clampi((int)((float)screen.h * 0.040f),
            24, 48);

        font_px = (g_sdl_character_sheet_screen.last_desc_px * 6) / 5;
        font_px = sdl_char_sheet_clampi(font_px, 14, max_px);
    }
    else if (g_sdl_character_sheet_screen.last_body_line_h > 0.0f)
    {
        float target_h = g_sdl_character_sheet_screen.last_body_line_h
            * (sdl_char_sheet_birth_context() ? 1.08f : 0.90f);
        int max_px = g_sdl_character_sheet_screen.last_body_px;

        if (max_px < 14)
            max_px = 14;
        font_px = sdl_char_sheet_font_px_for_line_height(target_h, 12,
            max_px);
    }
    else
        font_px = sdl_char_sheet_clampi((int)((float)screen.h * 0.020f), 14,
            30);
    font = sdl_story_font_for_height_slot(font_px, SDL_STORY_FONT_SLOT_MENU);
    if (!font)
        return;

    pad = sdl_char_sheet_clampf((float)font_px * 0.44f, 9.0f, 18.0f);
    gap = sdl_char_sheet_clampf((float)font_px * 0.38f, 7.0f, 16.0f);
    margin = sdl_char_sheet_clampf((float)screen.h * 0.010f, 7.0f, 18.0f);
    max_box_w = MIN((float)screen.w * 0.62f, 980.0f);
    max_box_w = MAX(max_box_w, MIN((float)screen.w - margin * 2.0f, 360.0f));
    max_text_w = max_box_w - pad * 2.0f;
    if (max_text_w <= 1.0f)
        return;

    sdl_char_sheet_tooltip_attrs(desc, subject_attr, attrs, sizeof(attrs));
    run_count = sdl_char_sheet_tooltip_layout(font, desc, attrs, max_text_w,
        runs, SDL_CHAR_SHEET_TOOLTIP_MAX_RUNS, &line_count, &text_w);
    if (run_count <= 0 || line_count <= 0 || text_w <= 0.0f)
        return;
    line_h = TTF_GetFontHeight(font);
    if (line_h < 1)
        line_h = font_px;

    box.w = text_w + pad * 2.0f;
    box.h = (float)(line_count * line_h) + pad * 2.0f;
    box.x = anchor.x + (anchor.w - box.w) * 0.5f;
    box.y = anchor.y - box.h - gap;

    if (box.y < (float)screen.y + margin)
        box.y = anchor.y + anchor.h + gap;
    box.x = sdl_char_sheet_clampf(box.x, (float)screen.x + margin,
        (float)(screen.x + screen.w) - box.w - margin);
    box.y = sdl_char_sheet_clampf(box.y, (float)screen.y + margin,
        (float)(screen.y + screen.h) - box.h - margin);
    g_sdl_char_sheet_hover_tooltip_rect = box;
    g_sdl_char_sheet_hover_tooltip_choice =
        g_sdl_character_sheet_screen.hover_choice;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 220);
    SDL_RenderFillRect(g_state.renderer, &box);
    SDL_SetRenderDrawColor(g_state.renderer, 255, 255, 255, 125);
    SDL_RenderRect(g_state.renderer, &box);

    for (int i = 0; i < run_count; i++)
    {
        const sdl_char_sheet_tooltip_run* run = &runs[i];
        char run_text[256];
        SDL_Texture* texture;
        SDL_FRect dst;
        int copy_len = MIN(run->len, (int)sizeof(run_text) - 1);
        int run_w = 0;
        int run_h = 0;

        SDL_memcpy(run_text, desc + run->start, (size_t)copy_len);
        run_text[copy_len] = '\0';
        texture = sdl_ui_text_texture(font, run_text,
            g_state.palette[run->attr], &run_w, &run_h);
        if (!texture)
            continue;

        dst = (SDL_FRect){
            .x = box.x + pad + run->x,
            .y = box.y + pad + (float)(run->line * line_h),
            .w = (float)run_w,
            .h = (float)run_h,
        };
        SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
    }
}

/*
 * Pixel-semantic panel engine.
 *
 * A "panel" is a titled block of pre-collected labeled lines (vitals, traits,
 * attributes, skills) or the birth allocation table.  The fluid packer decides
 * how many columns fit the available width (column count is emergent, not a
 * hand-tuned breakpoint), distributes the panels across those columns, and
 * sizes one shared body font so the fullest column fits the available height.
 * All drawing reuses the existing labeled-line / traits / allocation drawers.
 */
typedef enum sdl_panel_kind {
    SDL_PANEL_KIND_LINES = 0,   /* heading + labeled lines */
    SDL_PANEL_KIND_SELECT_STATS,/* character selection stats + compact ratings */
    SDL_PANEL_KIND_TRAITS,      /* trait grid */
    SDL_PANEL_KIND_ALLOC,       /* birth allocation table (combined) */
    SDL_PANEL_KIND_ALLOC_STATS, /* attributes in the original birth grid style */
    SDL_PANEL_KIND_ALLOC_SKILLS /* skills in the original birth grid style */
} sdl_panel_kind;

typedef struct sdl_panel {
    sdl_panel_kind kind;
    cptr heading;
    const sdl_char_sheet_line* lines;
    int line_count;
    cptr secondary_heading;
    const sdl_char_sheet_line* secondary_lines;
    int secondary_line_count;
    float label_fraction;
    int weight;          /* relative column-width weight */
    int rows;            /* rows consumed incl. heading (for packing/sizing) */
    float natural_w;     /* measured natural width (filled by the packer) */
    bool splittable;     /* may split into sub-columns when extra width (P3) */
    bool alloc_stats;    /* ALLOC: stats screen vs skills screen */
    int trait_cols;      /* TRAITS: column count chosen at draw time */
} sdl_panel;

#define SDL_CHAR_SHEET_PANEL_MAX 8

/* Width a labeled row needs so neither its label nor value gets scaled down. */
float sdl_char_sheet_row_natural_w(const sdl_char_sheet_line* line,
    TTF_Font* font, float label_fraction)
{
    const char* tab;
    char label[96];
    float lw;
    float vw;

    if (!line || !line->text[0])
        return 0.0f;

    tab = strchr(line->text, '\t');
    if (!tab)
        return (float)sdl_char_sheet_text_width(font, line->text) + 8.0f;

    {
        size_t ll = (size_t)(tab - line->text);
        if (ll >= sizeof(label))
            ll = sizeof(label) - 1;
        memcpy(label, line->text, ll);
        label[ll] = '\0';
    }
    lw = (float)sdl_char_sheet_text_width(font, label);
    vw = (float)sdl_char_sheet_text_width(font, tab + 1);
    if (label_fraction < 0.2f)
        label_fraction = 0.2f;
    if (label_fraction > 0.8f)
        label_fraction = 0.8f;
    return MAX(lw / label_fraction, vw / (1.0f - label_fraction)) + 10.0f;
}

/* Column width a panel needs (widest row/heading) so nothing is scaled down. */
float sdl_char_sheet_panel_natural_w(const sdl_panel* p, TTF_Font* font)
{
    float maxw;

    if (!p)
        return 0.0f;

    maxw = (float)sdl_char_sheet_text_width(font, p->heading ? p->heading : "");
    if (p->secondary_heading)
        maxw = MAX(maxw,
            (float)sdl_char_sheet_text_width(font, p->secondary_heading));
    if (p->kind == SDL_PANEL_KIND_ALLOC)
        return MAX(maxw, (float)sdl_char_sheet_text_width(font,
            "Perception  99 = 99 +99 +99 +99   999999"));
    if (p->kind == SDL_PANEL_KIND_ALLOC_STATS)
        return MAX(maxw, (float)sdl_char_sheet_text_width(font,
            "Constitution   99    cost 9999"));
    if (p->kind == SDL_PANEL_KIND_ALLOC_SKILLS)
        return MAX(maxw, (float)sdl_char_sheet_text_width(font,
            "Perception  99 = 99 +99 +99 +99   999999"));
    if (p->kind == SDL_PANEL_KIND_SELECT_STATS)
        maxw = MAX(maxw, (float)sdl_char_sheet_text_width(font,
            "Constitution\t+99"));
    for (int i = 0; i < p->line_count; i++)
    {
        float w = sdl_char_sheet_row_natural_w(&p->lines[i], font,
            (p->kind == SDL_PANEL_KIND_TRAITS) ? 0.95f : p->label_fraction);
        if (w > maxw)
            maxw = w;
    }
    for (int i = 0; i < p->secondary_line_count; i++)
    {
        float w = sdl_char_sheet_row_natural_w(&p->secondary_lines[i], font,
            p->label_fraction);
        if (w > maxw)
            maxw = w;
    }
    if (p->kind == SDL_PANEL_KIND_SELECT_STATS
        && g_sdl_character_sheet_screen.select_rating_count > 0)
    {
        float rating_w = (float)sdl_char_sheet_text_width(font,
            "Mighty *** 99") * 0.72f;

        if (rating_w > maxw)
            maxw = rating_w;
    }
    return maxw;
}

float sdl_char_sheet_sample_panel_natural_w(TTF_Font* font,
    cptr heading, cptr sample, float label_fraction)
{
    sdl_char_sheet_line line;
    float w = (float)sdl_char_sheet_text_width(font, heading ? heading : "");

    SDL_zero(line);
    SDL_strlcpy(line.text, sample ? sample : "", sizeof(line.text));
    w = MAX(w, sdl_char_sheet_row_natural_w(&line, font, label_fraction));
    return w;
}

int sdl_char_sheet_target_ncols(float content_w, float screen_h);

static int sdl_char_sheet_description_max_px(int col_px, int canvas_h)
{
    int desc_px;
    int canvas_cap;

    if (col_px < 12)
        col_px = 12;

    desc_px = (int)((float)col_px * 1.10f + 0.5f);
    canvas_cap = sdl_char_sheet_clampi((int)((float)canvas_h * 0.082f),
        44, 112);

    if (desc_px > canvas_cap)
        desc_px = canvas_cap;

    return desc_px;
}

/*
 * Usually lore stays close in size to the compact columns.  If every
 * description in the current character set still leaves a complete unused
 * line at that cap, allow a modestly larger font.  This avoids a visibly empty
 * final lore row for shorter sets (notably the Fingolfinrim) without changing
 * sets whose longest entry already consumes the available band.
 */
static int sdl_char_sheet_description_fit_max_px(int col_px, int canvas_h,
    cptr text, float width, float available_h)
{
    int max_px = sdl_char_sheet_description_max_px(col_px, canvas_h);
    TTF_Font* font;
    float line_h;
    int lines;
    int relaxed_px;
    int canvas_cap;

    if (g_sdl_character_sheet_screen.context
            != SDL_CHARACTER_SHEET_BIRTH_SELECT
        || g_sdl_character_sheet_screen.select_desc_candidate_count <= 0
        || !text || !text[0])
    {
        return max_px;
    }

    font = sdl_story_font_for_height_slot(max_px,
        SDL_STORY_FONT_SLOT_CHAR_DESC);
    if (!font)
        return max_px;

    line_h = sdl_char_sheet_line_h(font, max_px, 1.18f);
    lines = sdl_char_sheet_fitted_wrap_line_count(font, text, width,
        SDL_STORY_FONT_SLOT_CHAR_DESC);
    if (lines <= 0
        || line_h * (float)(lines + 1) > available_h)
    {
        return max_px;
    }

    relaxed_px = (int)((float)col_px * 1.12f + 0.5f);
    canvas_cap = sdl_char_sheet_clampi((int)((float)canvas_h * 0.082f),
        44, 112);
    if (relaxed_px > canvas_cap)
        relaxed_px = canvas_cap;
    if (relaxed_px > max_px)
        max_px = relaxed_px;

    return max_px;
}

bool sdl_char_sheet_measure_columns_desc(sdl_panel* panels,
    int panel_count, float content_w, float region_h, int canvas_h,
    cptr desc_sizing, int* out_desc_px, float* out_desc_line_h,
    int* out_desc_lines)
{
    int ncols = sdl_char_sheet_target_ncols(content_w, (float)canvas_h);
    float col_gap = sdl_char_sheet_clampf(content_w * 0.022f, 16.0f, 44.0f);
    float gap = sdl_char_sheet_clampf(region_h * 0.02f, 10.0f, 28.0f);
    int col_of[SDL_CHAR_SHEET_PANEL_MAX];
    float col_rows[SDL_CHAR_SHEET_PANEL_MAX];
    float max_rows = 1.0f;
    float top_line_h = 1.0f;
    float col_width_cap = 1.0e9f;
    int col_px = 0;
    int desc_px = 12;
    float desc_line_h = 1.0f;
    int desc_lines = 0;

    if (out_desc_px)
        *out_desc_px = 12;
    if (out_desc_line_h)
        *out_desc_line_h = 1.0f;
    if (out_desc_lines)
        *out_desc_lines = 0;

    if (!desc_sizing || !desc_sizing[0] || panel_count <= 0
        || content_w <= 0.0f || region_h <= 0.0f)
    {
        return false;
    }
    if (panel_count > SDL_CHAR_SHEET_PANEL_MAX)
        panel_count = SDL_CHAR_SHEET_PANEL_MAX;
    if (ncols > panel_count)
        ncols = panel_count;
    if (ncols < 1)
        ncols = 1;

    {
        int order[SDL_CHAR_SHEET_PANEL_MAX];
        int col_min_idx[SDL_CHAR_SHEET_PANEL_MAX];
        int remap[SDL_CHAR_SHEET_PANEL_MAX];
        int inv[SDL_CHAR_SHEET_PANEL_MAX];

        for (int i = 0; i < panel_count; i++)
            order[i] = i;
        for (int a = 0; a < panel_count; a++)
            for (int b = a + 1; b < panel_count; b++)
                if (panels[order[b]].rows > panels[order[a]].rows)
                {
                    int t = order[a];
                    order[a] = order[b];
                    order[b] = t;
                }

        for (int c = 0; c < ncols; c++)
        {
            col_rows[c] = 0.0f;
            col_min_idx[c] = panel_count;
        }
        for (int k = 0; k < panel_count; k++)
        {
            int i = order[k];
            int best = 0;

            for (int c = 1; c < ncols; c++)
                if (col_rows[c] < col_rows[best])
                    best = c;
            col_of[i] = best;
            col_rows[best] += (float)panels[i].rows + 0.6f;
            if (i < col_min_idx[best])
                col_min_idx[best] = i;
        }

        for (int c = 0; c < ncols; c++)
            remap[c] = c;
        for (int a = 0; a < ncols; a++)
            for (int b = a + 1; b < ncols; b++)
                if (col_min_idx[remap[b]] < col_min_idx[remap[a]])
                {
                    int t = remap[a];
                    remap[a] = remap[b];
                    remap[b] = t;
                }
        for (int c = 0; c < ncols; c++)
            inv[remap[c]] = c;
        for (int i = 0; i < panel_count; i++)
            col_of[i] = inv[col_of[i]];

        for (int c = 0; c < ncols; c++)
            col_rows[c] = 0.0f;
        for (int i = 0; i < panel_count; i++)
            col_rows[col_of[i]] += (float)panels[i].rows + 0.6f;
    }

    for (int c = 0; c < ncols; c++)
        if (col_rows[c] > max_rows)
            max_rows = col_rows[c];

    {
        const int ref_px = 40;
        TTF_Font* ref_font = sdl_story_font_for_height(ref_px);
        float ref_w[SDL_CHAR_SHEET_PANEL_MAX];
        float sum_ref = 0.0f;
        float avail = content_w - col_gap * (float)(ncols - 1);

        if (avail < 1.0f)
            avail = 1.0f;
        for (int c = 0; c < ncols; c++)
            ref_w[c] = 1.0f;
        for (int i = 0; i < panel_count; i++)
        {
            float w = (panels[i].natural_w > 0.0f)
                ? panels[i].natural_w
                : sdl_char_sheet_panel_natural_w(&panels[i], ref_font);
            if (w > ref_w[col_of[i]])
                ref_w[col_of[i]] = w;
        }
        for (int c = 0; c < ncols; c++)
            sum_ref += ref_w[c];
        if (sum_ref < 1.0f)
            sum_ref = 1.0f;
        col_width_cap = (float)ref_px * avail / sum_ref;
    }

    {
        int max_px = sdl_char_sheet_clampi((int)((float)canvas_h * 0.072f),
            40, 100);
        float desc_avail;

        if (col_width_cap < 12.0f)
            col_width_cap = 12.0f;
        if ((float)max_px > col_width_cap)
            max_px = (int)col_width_cap;

        (void)sdl_char_sheet_font_for_rows(region_h * 0.72f,
            (int)(max_rows + 0.5f), 12, max_px, 1.13f, &top_line_h, &col_px);
        desc_avail = region_h - (max_rows * top_line_h) - gap;
        if (desc_avail < top_line_h)
            desc_avail = top_line_h;

        (void)sdl_char_sheet_font_for_wrapped_text(desc_sizing, content_w,
            desc_avail, 12, sdl_char_sheet_description_fit_max_px(col_px,
                canvas_h, desc_sizing, content_w, desc_avail),
            1.18f, SDL_STORY_FONT_SLOT_CHAR_DESC, &desc_line_h,
            &desc_lines, &desc_px);
    }

    if (out_desc_px)
        *out_desc_px = desc_px;
    if (out_desc_line_h)
        *out_desc_line_h = desc_line_h;
    if (out_desc_lines)
        *out_desc_lines = desc_lines;
    return true;
}

void sdl_char_sheet_panel_draw(const sdl_panel* p, TTF_Font* font,
    float x, float y, float w, float h, float line_h)
{
    if (!p)
        return;

    switch (p->kind)
    {
    case SDL_PANEL_KIND_SELECT_STATS:
        sdl_char_sheet_draw_select_stats(font, p->heading, p->lines,
            p->line_count, p->secondary_lines, p->secondary_line_count,
            p->secondary_heading != NULL, x, y, w, h, line_h,
            p->label_fraction);
        break;
    case SDL_PANEL_KIND_TRAITS:
        sdl_char_sheet_draw_traits(font, p->lines, p->line_count, x, y, w, h,
            line_h, p->trait_cols > 0 ? p->trait_cols : 1);
        break;
    case SDL_PANEL_KIND_ALLOC:
        sdl_char_sheet_draw_birth_allocation_area(font, x, y, w, h, line_h,
            p->alloc_stats);
        break;
    case SDL_PANEL_KIND_ALLOC_STATS:
    {
        bool allocate = (g_sdl_character_sheet_screen.context
            == SDL_CHARACTER_SHEET_BIRTH_STATS);

        sdl_char_sheet_draw_heading(font, p->heading, x, y, w, line_h);
        for (int stat = 0; stat < A_MAX; stat++)
            sdl_char_sheet_draw_birth_stat_table_row(font, x, y, w, h, line_h,
                1 + stat, stat, allocate);
        if (allocate)
            sdl_char_sheet_draw_birth_points_row(font, x, y, w, h, line_h,
                A_MAX + 1);
        break;
    }
    case SDL_PANEL_KIND_ALLOC_SKILLS:
    {
        bool allocate = (g_sdl_character_sheet_screen.context
            == SDL_CHARACTER_SHEET_BIRTH_SKILLS);
        int row = 1;

        sdl_char_sheet_draw_heading(font, p->heading, x, y, w, line_h);
        for (int skill = 0; skill < S_MAX; skill++)
        {
            if (skill == S_SPC)
                continue;
            sdl_char_sheet_draw_birth_skill_table_row(font, x, y, w, h,
                line_h, row++, skill, allocate);
        }
        if (allocate)
            sdl_char_sheet_draw_birth_points_row(font, x, y, w, h, line_h,
                row);
        break;
    }
    case SDL_PANEL_KIND_LINES:
    default:
        sdl_char_sheet_draw_lines(font, p->heading, p->lines, p->line_count,
            x, y, w, h, line_h, p->label_fraction);
        break;
    }
}

/*
 * Choose the live-sheet column count from the screen aspect ratio
 * (content width vs the full canvas height -- device-intrinsic, so it is not
 * thrown off by the title/prompt bars or safe-area cropping).  Wider screens
 * get more columns: fewer rows per column means the tallest column shrinks and
 * the body font can grow.  Five columns require the long Vitals group to be
 * split into Vitals (status) + Combat, which the caller does when this returns
 * 5.  Reference points (content_w/canvas_h): typical Android phone landscape
 * (e.g. OnePlus 13, ~2.05-2.2) -> 5; 16:9 desktop (~1.67) -> 4; 4:3 / narrow
 * windows -> 3; portrait -> 2.  Thresholds are simple to tune.
 */
int sdl_char_sheet_target_ncols(float content_w, float screen_h)
{
    float ar = (screen_h > 1.0f) ? content_w / screen_h : 1.8f;

    if (ar >= 1.90f)
        return 5;
    if (ar >= 1.45f)
        return 4;
    if (ar >= 1.05f)
        return 3;
    return 2;
}

/*
 * Live-sheet column layout, matching the birth screens for visual consistency.
 *
 * The column count follows sdl_char_sheet_target_ncols(); panels keep their
 * given left-to-right order, with any panels past the column count stacked into
 * the final column.  Columns use a moderate body font; the description is laid
 * out directly beneath them at a slightly smaller size (never larger than the
 * columns), and the group is centred (large screens) or row-spread (small
 * screens) so there is neither a gap between columns and description nor wasted
 * vertical space.
 */
/* ---- Coach: capture each drawn panel's rect by heading ------------------
 * The first-run coach overlay (sdl-touch-tutorial.c) reads these so it can
 * highlight and step through the real Vitals / Traits / Attributes / Skills
 * blocks instead of guessing coordinates. */
typedef struct sdl_char_sheet_panel_rect_entry {
    char heading[32];
    SDL_FRect rect;
} sdl_char_sheet_panel_rect_entry;

static sdl_char_sheet_panel_rect_entry
    g_char_sheet_panel_rects[SDL_CHAR_SHEET_PANEL_MAX];
static int g_char_sheet_panel_rect_count;

static void sdl_char_sheet_panel_rects_reset(void)
{
    g_char_sheet_panel_rect_count = 0;
}

static void sdl_char_sheet_panel_rect_record(cptr heading, SDL_FRect rect)
{
    sdl_char_sheet_panel_rect_entry* e;

    if (!heading || !heading[0])
        return;
    if (rect.w <= 1.0f || rect.h <= 1.0f)
        return;
    if (g_char_sheet_panel_rect_count >= SDL_CHAR_SHEET_PANEL_MAX)
        return;

    e = &g_char_sheet_panel_rects[g_char_sheet_panel_rect_count++];
    SDL_strlcpy(e->heading, heading, sizeof(e->heading));
    e->rect = rect;
}

bool sdl_char_sheet_panel_rect(cptr heading, SDL_FRect* out)
{
    if (!heading || !out)
        return false;
    for (int i = 0; i < g_char_sheet_panel_rect_count; i++) {
        if (streq(g_char_sheet_panel_rects[i].heading, heading)) {
            *out = g_char_sheet_panel_rects[i].rect;
            return true;
        }
    }
    return false;
}

void sdl_char_sheet_render_columns(sdl_panel* panels, int panel_count,
    float content_x, float top_y, float content_w, float region_h, int canvas_h,
    cptr desc, cptr desc_sizing, int ncols_bias, SDL_FRect* out_alloc_col)
{
    int ncols = sdl_char_sheet_target_ncols(content_w, (float)canvas_h)
        + ncols_bias;
    float col_gap = sdl_char_sheet_clampf(content_w * 0.022f, 16.0f, 44.0f);
    float gap = sdl_char_sheet_clampf(region_h * 0.02f, 10.0f, 28.0f);
    bool has_desc = (desc && desc[0]);
    /* Size the description band from desc_sizing (the longest description in
     * the set) so the layout stays put as the highlighted entry changes; the
     * actual desc is what gets drawn. */
    cptr desc_measure = (desc_sizing && desc_sizing[0]) ? desc_sizing : desc;
    int col_of[SDL_CHAR_SHEET_PANEL_MAX];
    float col_rows[SDL_CHAR_SHEET_PANEL_MAX];
    float col_x[SDL_CHAR_SHEET_PANEL_MAX];
    float col_w[SDL_CHAR_SHEET_PANEL_MAX];
    float max_rows = 1.0f;
    float top_line_h = 1.0f;
    float columns_h;
    float v_off;
    float col_width_cap = 1.0e9f;
    int col_px = 0;
    int desc_px = 0;
    TTF_Font* body;
    TTF_Font* desc_font = NULL;
    float desc_line_h = 1.0f;
    int desc_lines = 0;
    float desc_h = 0.0f;

    if (out_alloc_col)
    {
        out_alloc_col->x = content_x;
        out_alloc_col->y = top_y;
        out_alloc_col->w = content_w;
        out_alloc_col->h = region_h;
    }

    if (panel_count <= 0 || content_w <= 0.0f || region_h <= 0.0f)
        return;
    if (panel_count > SDL_CHAR_SHEET_PANEL_MAX)
        panel_count = SDL_CHAR_SHEET_PANEL_MAX;
    if (ncols > panel_count)
        ncols = panel_count;
    if (ncols < 1)
        ncols = 1;

    /*
     * Balanced assignment: place the tallest panels first, each into the
     * currently-shortest column.  When there are more panels than columns this
     * doubles a SHORT panel (e.g. Attributes) under another column rather than
     * piling everything onto the last one -- so Attributes lands under a
     * Vitals/Combat column and the tall Skills block keeps its own column.
     * Columns are then ordered left-to-right by their smallest original panel
     * index, so the visual order stays sensible (Vitals ... Skills).
     */
    {
        int order[SDL_CHAR_SHEET_PANEL_MAX];
        int col_min_idx[SDL_CHAR_SHEET_PANEL_MAX];
        int remap[SDL_CHAR_SHEET_PANEL_MAX];
        int inv[SDL_CHAR_SHEET_PANEL_MAX];

        for (int i = 0; i < panel_count; i++)
            order[i] = i;
        for (int a = 0; a < panel_count; a++)
            for (int b = a + 1; b < panel_count; b++)
                if (panels[order[b]].rows > panels[order[a]].rows)
                {
                    int t = order[a];
                    order[a] = order[b];
                    order[b] = t;
                }

        for (int c = 0; c < ncols; c++)
        {
            col_rows[c] = 0.0f;
            col_min_idx[c] = panel_count;
        }
        for (int k = 0; k < panel_count; k++)
        {
            int i = order[k];
            int best = 0;

            for (int c = 1; c < ncols; c++)
                if (col_rows[c] < col_rows[best])
                    best = c;
            col_of[i] = best;
            col_rows[best] += (float)panels[i].rows + 0.6f;
            if (i < col_min_idx[best])
                col_min_idx[best] = i;
        }

        for (int c = 0; c < ncols; c++)
            remap[c] = c;
        for (int a = 0; a < ncols; a++)
            for (int b = a + 1; b < ncols; b++)
                if (col_min_idx[remap[b]] < col_min_idx[remap[a]])
                {
                    int t = remap[a];
                    remap[a] = remap[b];
                    remap[b] = t;
                }
        for (int c = 0; c < ncols; c++)
            inv[remap[c]] = c;
        for (int i = 0; i < panel_count; i++)
            col_of[i] = inv[col_of[i]];

        for (int c = 0; c < ncols; c++)
            col_rows[c] = 0.0f;
        for (int i = 0; i < panel_count; i++)
            col_rows[col_of[i]] += (float)panels[i].rows + 0.6f;
    }
    for (int c = 0; c < ncols; c++)
        if (col_rows[c] > max_rows)
            max_rows = col_rows[c];

    /*
     * Size column WIDTHS proportional to each column's natural content width,
     * so every column hits the same width-limited font instead of one
     * narrow-but-wide column throttling the whole sheet.  With proportional
     * widths the width-limited font is the same for all columns:
     *   col_w[c] / ref_w[c] == avail / sum(ref_w)  ->  uniform cap.
     */
    {
        const int ref_px = 40;
        TTF_Font* ref_font = sdl_story_font_for_height(ref_px);
        float ref_w[SDL_CHAR_SHEET_PANEL_MAX];
        float sum_ref = 0.0f;
        float avail = content_w - col_gap * (float)(ncols - 1);
        float cx = content_x;

        if (avail < 1.0f)
            avail = 1.0f;

        for (int c = 0; c < ncols; c++)
            ref_w[c] = 1.0f;
        for (int i = 0; i < panel_count; i++)
        {
            float w = (panels[i].natural_w > 0.0f)
                ? panels[i].natural_w
                : sdl_char_sheet_panel_natural_w(&panels[i], ref_font);
            if (w > ref_w[col_of[i]])
                ref_w[col_of[i]] = w;
        }
        for (int c = 0; c < ncols; c++)
            sum_ref += ref_w[c];
        if (sum_ref < 1.0f)
            sum_ref = 1.0f;

        for (int c = 0; c < ncols; c++)
        {
            col_w[c] = avail * ref_w[c] / sum_ref;
            col_x[c] = cx;
            cx += col_w[c] + col_gap;
        }
        col_width_cap = (float)ref_px * avail / sum_ref;

        /* Report the allocation column so the bottom Confirm/Back controls
         * stay aligned with it.  Points Left is owned and drawn by the active
         * allocation panel itself. */
        if (out_alloc_col)
        {
            int want = (g_sdl_character_sheet_screen.context
                == SDL_CHARACTER_SHEET_BIRTH_STATS)
                ? (int)SDL_PANEL_KIND_ALLOC_STATS
                : (int)SDL_PANEL_KIND_ALLOC_SKILLS;

            for (int i = 0; i < panel_count; i++)
                if (panels[i].kind == SDL_PANEL_KIND_ALLOC
                    || (int)panels[i].kind == want)
                {
                    out_alloc_col->x = col_x[col_of[i]];
                    out_alloc_col->w = col_w[col_of[i]];
                    break;
                }
        }
    }

    {
        int max_px = sdl_char_sheet_clampi((int)((float)canvas_h * 0.072f),
            40, 100);

        /* Don't exceed the shared width limit (keeps text from being scaled
         * down, which would make the full-width description look larger than
         * the columns). */
        if (col_width_cap < 12.0f)
            col_width_cap = 12.0f;
        if ((float)max_px > col_width_cap)
            max_px = (int)col_width_cap;

        /*
         * Columns keep NATURAL row spacing (no airy stretching).  Size them to
         * fill most of the height, reserving room for the description, then
         * grow the description to fill whatever is left.  The description uses
         * its own story face and may be slightly larger than the compact
         * column text.
         */
        body = sdl_char_sheet_font_for_rows(
            has_desc ? region_h * 0.72f : region_h,
            (int)(max_rows + 0.5f), 12, max_px, 1.13f, &top_line_h, &col_px);
        columns_h = max_rows * top_line_h;
        /* Record the column body size so the hover tooltip can render at about
         * half the size of the stats/trait text it explains. */
        g_sdl_character_sheet_screen.last_body_px = col_px;
        g_sdl_character_sheet_screen.last_body_line_h = top_line_h * 0.96f;
        g_sdl_character_sheet_screen.last_desc_px = 0;
        g_sdl_character_sheet_screen.last_desc_line_h = 0.0f;

        if (has_desc)
        {
            float desc_avail = region_h - columns_h - gap;

            if (desc_avail < top_line_h)
                desc_avail = top_line_h;
            desc_font = sdl_char_sheet_font_for_wrapped_text(desc_measure,
                content_w, desc_avail, 12,
                sdl_char_sheet_description_fit_max_px(col_px, canvas_h,
                    desc_measure, content_w, desc_avail),
                1.18f, SDL_STORY_FONT_SLOT_CHAR_DESC, &desc_line_h,
                &desc_lines, &desc_px);
            desc_h = desc_line_h * (float)desc_lines;
            g_sdl_character_sheet_screen.last_desc_px = desc_px;
            g_sdl_character_sheet_screen.last_desc_line_h = desc_line_h;
        }

        v_off = 0.0f;
    }

    sdl_char_sheet_panel_rects_reset();
    for (int c = 0; c < ncols; c++)
    {
        float y = top_y + v_off;

        for (int i = 0; i < panel_count; i++)
        {
            if (col_of[i] != c)
                continue;

            if (panels[i].kind == SDL_PANEL_KIND_TRAITS)
                panels[i].trait_cols = 1;

            sdl_char_sheet_panel_draw(&panels[i], body, col_x[c], y, col_w[c],
                columns_h - (y - (top_y + v_off)), top_line_h);
            sdl_char_sheet_panel_rect_record(panels[i].heading,
                (SDL_FRect){ col_x[c], y, col_w[c],
                    (float)panels[i].rows * top_line_h });
            y += (float)panels[i].rows * top_line_h + top_line_h * 0.6f;
        }
    }

    if (has_desc && desc_lines > 0)
    {
        float desc_y = top_y + v_off + columns_h + gap;

        sdl_char_sheet_draw_history(desc_font, desc, content_x, desc_y,
            content_w, desc_h, desc_line_h, desc_lines);
    }
}

#if SIL_SDL_MOBILE_BUILD
static bool sdl_character_sheet_mobile_character_select_active(void)
{
    return g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_BIRTH_SELECT
        && !g_sdl_character_sheet_screen.select_book_mode
        && !g_sdl_character_sheet_screen.select_menu_style;
}
#endif

/*
 * Public predicate (always defined): true on the mobile hero carousel, where
 * the character list is replaced by a single name flanked by prev/next
 * triangles and Left/Right step between heroes.  Lets the shared birth choice
 * state machine remap Left/Right on that screen without depending on the
 * mobile-only static above.
 */
bool sdl_character_sheet_screen_mobile_carousel_active(void)
{
#if SIL_SDL_MOBILE_BUILD
    return sdl_character_sheet_mobile_character_select_active();
#else
    return false;
#endif
}

/* Shared list/Chronicle scroll state is used by desktop wheels and keyboard
 * input as well as mobile touch drags, so it must not live in the mobile-only
 * carousel block below. */
static bool sdl_character_sheet_scroll_active(void)
{
    bool select_menu = g_sdl_character_sheet_screen.context
            == SDL_CHARACTER_SHEET_BIRTH_SELECT
        && g_sdl_character_sheet_screen.select_menu_style;
    bool landscape_chronicle = !g_sdl_narrative_portrait_rendering
        && g_sdl_character_sheet_screen.context
            == SDL_CHARACTER_SHEET_NARRATIVE
        && g_sdl_character_sheet_screen.narrative_contents_count > 0;

    return (select_menu || landscape_chronicle)
        && g_sdl_character_sheet_screen.sheet_scroll_max > 0;
}

static bool sdl_character_sheet_set_scroll(int scroll)
{
    int old_scroll = g_sdl_character_sheet_screen.sheet_scroll;
    int page = g_sdl_character_sheet_screen.select_page;

    scroll = sdl_char_sheet_clampi(scroll, 0,
        g_sdl_character_sheet_screen.sheet_scroll_max);
    g_sdl_character_sheet_screen.sheet_scroll = scroll;
    if (!g_sdl_narrative_portrait_rendering
        && g_sdl_character_sheet_screen.context
            == SDL_CHARACTER_SHEET_NARRATIVE
        && g_sdl_character_sheet_screen.narrative_contents_count > 0
        && page >= 0 && page < SDL_BOOK_MAX_PAGES)
    {
        g_sdl_narrative_page_scroll[page] = scroll;
    }
    if (scroll == old_scroll)
        return false;

    g_state.need_present = true;
    return true;
}

bool sdl_character_sheet_screen_scroll_book(int direction)
{
    int step;

    if (!sdl_character_sheet_scroll_active() || direction == 0)
        return false;
    step = MAX(48, (int)((float)MAX(1,
        g_sdl_character_sheet_screen.narrative_body_px) * 1.55f));
    return sdl_character_sheet_set_scroll(
        g_sdl_character_sheet_screen.sheet_scroll
            + ((direction > 0) ? step : -step));
}

#if SIL_SDL_MOBILE_BUILD

static void sdl_character_sheet_select_counter(int* out_current,
    int* out_total)
{
    int current = 0;
    int total = 0;

    if (out_current)
        *out_current = 0;
    if (out_total)
        *out_total = 0;

    for (int i = 0; i < g_sdl_character_sheet_screen.select_row_count; i++)
    {
        const sdl_character_sheet_select_row* r =
            &g_sdl_character_sheet_screen.select_rows[i];

        if (r->is_heading)
            continue;
        total++;
        if (r->choice == g_sdl_character_sheet_screen.selected_index)
            current = total;
    }

    if (current <= 0 && g_sdl_character_sheet_screen.selected_index >= 0
        && g_sdl_character_sheet_screen.selected_index < total)
    {
        current = g_sdl_character_sheet_screen.selected_index + 1;
    }
    if (current <= 0 && total > 0)
        current = 1;

    if (out_current)
        *out_current = current;
    if (out_total)
        *out_total = total;
}

/*
 * Drag-to-scroll is also wanted on the generic select-menu-style lists
 * (options, etc.), not only the hero carousel.  Enable it whenever such a menu
 * actually overflows its visible region; tapping a row still selects it.
 */
static void sdl_character_sheet_select_scroll_cancel(void)
{
    SDL_zero(g_sdl_character_sheet_screen.select_scroll_drag);
}

static bool sdl_character_sheet_select_scroll_point_inside(float x, float y)
{
    SDL_FRect r = g_sdl_character_sheet_screen.select_scroll_rect;

    return r.w > 1.0f && r.h > 1.0f
        && x >= r.x && y >= r.y
        && x < r.x + r.w && y < r.y + r.h;
}

static bool sdl_character_sheet_select_scroll_begin(float x, float y,
    SDL_FingerID finger_id)
{
    menu_scroll_drag_state* drag =
        &g_sdl_character_sheet_screen.select_scroll_drag;

    if (!sdl_character_sheet_mobile_character_select_active()
        && !sdl_character_sheet_scroll_active())
        return false;
    if (!sdl_character_sheet_select_scroll_point_inside(x, y))
        return false;

    sdl_character_sheet_select_scroll_cancel();
    drag->active = true;
    drag->dragged = false;
    drag->finger_id = finger_id;
    drag->start_x = x;
    drag->start_y = y;
    drag->last_y = y;
    drag->accum_y = 0.0f;
    return true;
}

static bool sdl_character_sheet_select_scroll_motion(float x, float y,
    SDL_FingerID finger_id)
{
    menu_scroll_drag_state* drag =
        &g_sdl_character_sheet_screen.select_scroll_drag;
    float dx;
    float dy;
    float total_dy;
    int scroll;

    if (!drag->active || drag->finger_id != finger_id)
        return false;

    dx = x - drag->start_x;
    if (dx < 0.0f)
        dx = -dx;
    total_dy = y - drag->start_y;
    if (total_dy < 0.0f)
        total_dy = -total_dy;

    if (!drag->dragged && dx > sdl_touch_swipe_threshold_px()
        && dx > total_dy * 1.2f)
    {
        sdl_character_sheet_select_scroll_cancel();
        return false;
    }

    dy = y - drag->last_y;
    drag->last_y = y;
    if (total_dy > sdl_touch_swipe_threshold_px())
    {
        drag->dragged = true;
        g_sdl_character_sheet_screen.hover_choice = SDL_CHAR_SHEET_NO_HOVER;
        ui_menu_click_clear_pending_hover();
    }

    if (total_dy < 3.0f && !drag->dragged)
        return true;

    scroll = g_sdl_character_sheet_screen.sheet_scroll - (int)dy;
    (void)sdl_character_sheet_set_scroll(scroll);
    return true;
}

static bool sdl_character_sheet_select_scroll_finish(SDL_FingerID finger_id,
    bool* out_tap)
{
    menu_scroll_drag_state* drag =
        &g_sdl_character_sheet_screen.select_scroll_drag;
    bool tap;

    if (!drag->active || drag->finger_id != finger_id)
        return false;

    tap = !drag->dragged;
    sdl_character_sheet_select_scroll_cancel();
    if (out_tap)
        *out_tap = tap;
    return true;
}

/*
 * A filled prev/next triangle for the mobile hero carousel.  Drawn as solid
 * geometry rather than a font glyph so it is always visible regardless of
 * which story font is loaded.  Points left when `left`, otherwise right.
 */
static void sdl_char_sheet_draw_carousel_arrow(SDL_FRect box, bool left,
    byte attr)
{
    SDL_Vertex v[3];
    SDL_Color c;
    SDL_FColor col;
    float cx = box.x + box.w * 0.5f;
    float cy = box.y + box.h * 0.5f;
    float hw = box.w * 0.24f;
    float hh = sdl_char_sheet_clampf(box.h * 0.28f, 1.0f, box.w * 0.42f);

    if (!g_state.renderer || box.w <= 1.0f || box.h <= 1.0f)
        return;

    c = g_state.palette[attr];
    col = (SDL_FColor){ (float)c.r / 255.0f, (float)c.g / 255.0f,
        (float)c.b / 255.0f, 1.0f };

    if (left)
    {
        v[0].position = (SDL_FPoint){ cx - hw, cy };
        v[1].position = (SDL_FPoint){ cx + hw, cy - hh };
        v[2].position = (SDL_FPoint){ cx + hw, cy + hh };
    }
    else
    {
        v[0].position = (SDL_FPoint){ cx + hw, cy };
        v[1].position = (SDL_FPoint){ cx - hw, cy - hh };
        v[2].position = (SDL_FPoint){ cx - hw, cy + hh };
    }
    for (int i = 0; i < 3; i++)
    {
        v[i].color = col;
        v[i].tex_coord.x = 0.0f;
        v[i].tex_coord.y = 0.0f;
    }

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(g_state.renderer, NULL, v, 3, NULL, 0);
}

#define SDL_MOBILE_SELECT_PANEL_GAP_ROWS 0.55f
#define SDL_MOBILE_SELECT_BAD_SCORE (-1.0e30f)

typedef struct sdl_mobile_select_box {
    int panel_first;
    int panel_count;
    SDL_FRect rect;
    int px;
    float line_h;
    float score_line_h;
    float rows;
} sdl_mobile_select_box;

typedef struct sdl_mobile_select_layout {
    sdl_mobile_select_box boxes[SDL_CHAR_SHEET_PANEL_MAX];
    int box_count;
    SDL_FRect lore_rect;
    int desc_px;
    float desc_line_h;
    int desc_lines;
    float score;
} sdl_mobile_select_layout;

static float sdl_char_sheet_mobile_panel_rows(const sdl_panel* panel)
{
    float rows = 0.0f;

    if (!panel)
        return 0.0f;

    if (panel->rows > 0)
        rows = (float)panel->rows;
    else
    {
        if (panel->heading && panel->heading[0])
            rows += 1.0f;
        rows += (float)panel->line_count;
    }

    return (rows > 1.0f) ? rows : 1.0f;
}

static float sdl_char_sheet_mobile_box_rows(const sdl_panel* panels,
    int first, int count)
{
    float rows = 0.0f;

    for (int i = 0; i < count; i++)
    {
        rows += sdl_char_sheet_mobile_panel_rows(&panels[first + i]);
        if (i + 1 < count)
            rows += SDL_MOBILE_SELECT_PANEL_GAP_ROWS;
    }

    return (rows > 1.0f) ? rows : 1.0f;
}

static float sdl_char_sheet_mobile_box_natural_w(const sdl_panel* panels,
    int first, int count, TTF_Font* font)
{
    float w = 1.0f;

    for (int i = 0; i < count; i++)
    {
        float panel_w = sdl_char_sheet_panel_natural_w(
            &panels[first + i], font);

        if (panel_w > w)
            w = panel_w;
    }

    return w;
}

static void sdl_char_sheet_mobile_measure_box(const sdl_panel* panels,
    int first, int count, float w, float h, int min_px, int max_px,
    sdl_mobile_select_box* box)
{
    float rows = sdl_char_sheet_mobile_box_rows(panels, first, count);
    int chosen_px = min_px;
    int low_px;
    int high_px;
    float chosen_lh = 1.0f;
    float chosen_w = 1.0f;
    bool found_fit = false;

    if (!box)
        return;

    if (max_px < min_px)
        max_px = min_px;
    if (w < 1.0f)
        w = 1.0f;
    if (h < 1.0f)
        h = 1.0f;

    low_px = min_px;
    high_px = max_px;
    while (low_px <= high_px) {
        int px = low_px + (high_px - low_px) / 2;
        TTF_Font* font = sdl_story_font_for_height_slot(px,
            SDL_STORY_FONT_SLOT_MENU);
        float lh;
        float natural_w;

        if (!font) {
            high_px = px - 1;
            continue;
        }

        lh = sdl_char_sheet_line_h(font, px, 1.30f);
        natural_w = sdl_char_sheet_mobile_box_natural_w(panels, first, count,
            font);
        if (lh * rows <= h && natural_w <= w) {
            chosen_px = px;
            chosen_lh = lh;
            chosen_w = natural_w;
            found_fit = true;
            low_px = px + 1;
        } else {
            high_px = px - 1;
        }
    }

    if (!found_fit) {
        TTF_Font* font = sdl_story_font_for_height_slot(min_px,
            SDL_STORY_FONT_SLOT_MENU);

        chosen_px = min_px;
        chosen_lh = sdl_char_sheet_line_h(font, min_px, 1.30f);
        chosen_w = sdl_char_sheet_mobile_box_natural_w(panels, first, count,
            font);
    }

    if (chosen_lh * rows > h)
    {
        chosen_lh = h / rows;
        if (chosen_lh < 1.0f)
            chosen_lh = 1.0f;
    }

    box->px = chosen_px;
    box->line_h = chosen_lh;
    box->rows = rows;
    box->score_line_h = chosen_lh;
    if (chosen_w > w && chosen_w > 1.0f)
        box->score_line_h *= w / chosen_w;
}

static void sdl_char_sheet_mobile_measure_desc(cptr text, float w, float h,
    int min_px, int max_px, int* out_px, float* out_line_h, int* out_lines)
{
    int px = max_px;
    float line_h = 1.0f;
    int lines = 0;

    if (max_px < min_px)
        max_px = min_px;
    if (!text || !text[0] || w <= 1.0f || h <= 1.0f)
    {
        TTF_Font* font = sdl_story_font_for_height_slot(px,
            SDL_STORY_FONT_SLOT_CHAR_DESC);

        line_h = sdl_char_sheet_line_h(font, px, 1.22f);
    }
    else
    {
        (void)sdl_char_sheet_font_for_wrapped_text(text, w, h, min_px,
            max_px, 1.22f, SDL_STORY_FONT_SLOT_CHAR_DESC, &line_h, &lines,
            &px);
    }

    if (out_px)
        *out_px = px;
    if (out_line_h)
        *out_line_h = line_h;
    if (out_lines)
        *out_lines = lines;
}

static void sdl_char_sheet_mobile_layout_init(sdl_mobile_select_layout* layout)
{
    SDL_zero(*layout);
    layout->score = SDL_MOBILE_SELECT_BAD_SCORE;
}

static bool sdl_char_sheet_mobile_layout_add_box(
    sdl_mobile_select_layout* layout, int panel_first, int panel_count,
    SDL_FRect rect)
{
    sdl_mobile_select_box* box;

    if (!layout || panel_count <= 0 || rect.w <= 1.0f || rect.h <= 1.0f)
        return false;
    if (layout->box_count >= (int)N_ELEMENTS(layout->boxes))
        return false;

    box = &layout->boxes[layout->box_count++];
    SDL_zero(*box);
    box->panel_first = panel_first;
    box->panel_count = panel_count;
    box->rect = rect;
    return true;
}

static void sdl_char_sheet_mobile_score_layout(
    sdl_mobile_select_layout* layout, const sdl_panel* panels, cptr desc_measure,
    int canvas_h, int min_px, int max_px)
{
    bool has_desc = (desc_measure && desc_measure[0]);
    float min_line = 1.0e9f;
    float weighted_line = 0.0f;
    float weight = 0.0f;
    float fill_score = 0.0f;
    int fill_count = 0;

    if (!layout)
        return;
    layout->score = SDL_MOBILE_SELECT_BAD_SCORE;

    for (int i = 0; i < layout->box_count; i++)
    {
        sdl_mobile_select_box* box = &layout->boxes[i];
        float text_h;

        if (box->rect.w <= 1.0f || box->rect.h <= 1.0f)
            return;

        sdl_char_sheet_mobile_measure_box(panels, box->panel_first,
            box->panel_count, box->rect.w, box->rect.h, min_px, max_px, box);

        if (box->score_line_h < min_line)
            min_line = box->score_line_h;
        weighted_line += box->score_line_h * box->rows;
        weight += box->rows;

        text_h = box->line_h * box->rows;
        if (box->rect.h > 1.0f)
        {
            fill_score += MIN(text_h / box->rect.h, 1.0f);
            fill_count++;
        }
    }

    if (has_desc)
    {
        float desc_h;

        if (layout->lore_rect.w <= 1.0f || layout->lore_rect.h <= 1.0f)
            return;

        sdl_char_sheet_mobile_measure_desc(desc_measure, layout->lore_rect.w,
            layout->lore_rect.h, min_px, max_px, &layout->desc_px,
            &layout->desc_line_h, &layout->desc_lines);
        if (layout->desc_lines > 0)
        {
            if (layout->desc_line_h < min_line)
                min_line = layout->desc_line_h;
            weighted_line += layout->desc_line_h
                * (float)layout->desc_lines * 1.35f;
            weight += (float)layout->desc_lines * 1.35f;

            desc_h = layout->desc_line_h * (float)layout->desc_lines;
            fill_score += MIN(desc_h / layout->lore_rect.h, 1.0f);
            fill_count++;
        }
    }

    if (weight <= 0.0f)
        return;

    if (min_line > 1.0e8f)
        min_line = weighted_line / weight;
    if (fill_count > 0)
        fill_score /= (float)fill_count;

    layout->score = min_line * 1000.0f
        + (weighted_line / weight) * 85.0f
        + fill_score * 35.0f
        + (float)canvas_h * 0.0001f;
}

static void sdl_char_sheet_mobile_consider_layout(
    sdl_mobile_select_layout* best, sdl_mobile_select_layout candidate,
    const sdl_panel* panels, cptr desc_measure, int canvas_h, int min_px,
    int max_px)
{
    sdl_char_sheet_mobile_score_layout(&candidate, panels, desc_measure,
        canvas_h, min_px, max_px);
    if (!best || candidate.score <= best->score)
        return;

    *best = candidate;
}

static void sdl_char_sheet_mobile_split_panel_widths(const sdl_panel* panels,
    int first, int count, float total_w, float* widths)
{
    TTF_Font* ref_font = sdl_story_font_for_height_slot(40,
        SDL_STORY_FONT_SLOT_MENU);
    float weights[SDL_CHAR_SHEET_PANEL_MAX];
    float sum = 0.0f;

    if (!widths || count <= 0)
        return;
    if (total_w < 1.0f)
        total_w = 1.0f;

    for (int i = 0; i < count; i++)
    {
        weights[i] = sdl_char_sheet_panel_natural_w(&panels[first + i],
            ref_font);
        if (weights[i] < 1.0f)
            weights[i] = 1.0f;
        sum += weights[i];
    }
    if (sum < 1.0f)
        sum = 1.0f;

    for (int i = 0; i < count; i++)
        widths[i] = total_w * weights[i] / sum;
}

static void sdl_char_sheet_mobile_add_side_layouts(
    sdl_mobile_select_layout* best, const sdl_panel* panels, int panel_count,
    float content_x, float top_y, float content_w, float region_h,
    float col_gap, cptr desc_measure, int canvas_h, int min_px, int max_px)
{
    static const float stacked_fracs[] = { 0.30f, 0.36f, 0.42f, 0.48f, 0.54f };
    static const float split_fracs[] = { 0.34f, 0.42f, 0.50f, 0.58f, 0.66f };
    int detail_count = panel_count - 1;

    if (detail_count <= 0)
        return;

    for (int i = 0; i < (int)N_ELEMENTS(stacked_fracs); i++)
    {
        float avail = content_w - col_gap;
        float detail_w = avail * stacked_fracs[i];
        float lore_w = avail - detail_w;
        sdl_mobile_select_layout candidate;

        if (avail <= 2.0f || detail_w <= 1.0f || lore_w <= 1.0f)
            continue;

        sdl_char_sheet_mobile_layout_init(&candidate);
        if (!sdl_char_sheet_mobile_layout_add_box(&candidate, 1,
                detail_count, (SDL_FRect){ content_x, top_y, detail_w,
                    region_h }))
        {
            continue;
        }
        candidate.lore_rect = (SDL_FRect){ content_x + detail_w + col_gap,
            top_y, lore_w, region_h };
        sdl_char_sheet_mobile_consider_layout(best, candidate, panels,
            desc_measure, canvas_h, min_px, max_px);
    }

    if (detail_count <= 1)
        return;

    for (int i = 0; i < (int)N_ELEMENTS(split_fracs); i++)
    {
        float total_gap = col_gap * (float)detail_count;
        float avail = content_w - total_gap;
        float detail_w = avail * split_fracs[i];
        float lore_w = avail - detail_w;
        float detail_widths[SDL_CHAR_SHEET_PANEL_MAX];
        float x = content_x;
        sdl_mobile_select_layout candidate;

        if (avail <= 2.0f || detail_w <= 1.0f || lore_w <= 1.0f)
            continue;

        sdl_char_sheet_mobile_split_panel_widths(panels, 1, detail_count,
            detail_w, detail_widths);
        sdl_char_sheet_mobile_layout_init(&candidate);
        for (int p = 0; p < detail_count; p++)
        {
            if (!sdl_char_sheet_mobile_layout_add_box(&candidate, 1 + p, 1,
                    (SDL_FRect){ x, top_y, detail_widths[p], region_h }))
            {
                break;
            }
            x += detail_widths[p] + col_gap;
        }
        if (candidate.box_count != detail_count)
            continue;

        candidate.lore_rect = (SDL_FRect){ x, top_y, lore_w, region_h };
        sdl_char_sheet_mobile_consider_layout(best, candidate, panels,
            desc_measure, canvas_h, min_px, max_px);
    }
}

static void sdl_char_sheet_mobile_add_top_layouts(
    sdl_mobile_select_layout* best, const sdl_panel* panels, int panel_count,
    float content_x, float top_y, float content_w, float region_h,
    float col_gap, float row_gap, cptr desc_measure, int canvas_h, int min_px,
    int max_px)
{
    static const float stacked_fracs[] = { 0.24f, 0.32f, 0.40f, 0.48f, 0.56f };
    static const float split_fracs[] = { 0.22f, 0.30f, 0.38f, 0.46f, 0.54f };
    int detail_count = panel_count - 1;

    if (detail_count <= 0)
        return;

    for (int i = 0; i < (int)N_ELEMENTS(stacked_fracs); i++)
    {
        float avail = region_h - row_gap;
        float detail_h = avail * stacked_fracs[i];
        float lore_h = avail - detail_h;
        sdl_mobile_select_layout candidate;

        if (avail <= 2.0f || detail_h <= 1.0f || lore_h <= 1.0f)
            continue;

        sdl_char_sheet_mobile_layout_init(&candidate);
        if (!sdl_char_sheet_mobile_layout_add_box(&candidate, 1,
                detail_count, (SDL_FRect){ content_x, top_y, content_w,
                    detail_h }))
        {
            continue;
        }
        candidate.lore_rect = (SDL_FRect){ content_x, top_y + detail_h
            + row_gap, content_w, lore_h };
        sdl_char_sheet_mobile_consider_layout(best, candidate, panels,
            desc_measure, canvas_h, min_px, max_px);
    }

    if (detail_count <= 1)
        return;

    for (int i = 0; i < (int)N_ELEMENTS(split_fracs); i++)
    {
        float avail_h = region_h - row_gap;
        float detail_h = avail_h * split_fracs[i];
        float lore_h = avail_h - detail_h;
        float detail_w = content_w - col_gap * (float)(detail_count - 1);
        float widths[SDL_CHAR_SHEET_PANEL_MAX];
        float x = content_x;
        sdl_mobile_select_layout candidate;

        if (avail_h <= 2.0f || detail_h <= 1.0f || lore_h <= 1.0f
            || detail_w <= 1.0f)
        {
            continue;
        }

        sdl_char_sheet_mobile_split_panel_widths(panels, 1, detail_count,
            detail_w, widths);
        sdl_char_sheet_mobile_layout_init(&candidate);
        for (int p = 0; p < detail_count; p++)
        {
            if (!sdl_char_sheet_mobile_layout_add_box(&candidate, 1 + p, 1,
                    (SDL_FRect){ x, top_y, widths[p], detail_h }))
            {
                break;
            }
            x += widths[p] + col_gap;
        }
        if (candidate.box_count != detail_count)
            continue;

        candidate.lore_rect = (SDL_FRect){ content_x, top_y + detail_h
            + row_gap, content_w, lore_h };
        sdl_char_sheet_mobile_consider_layout(best, candidate, panels,
            desc_measure, canvas_h, min_px, max_px);
    }
}

static void sdl_char_sheet_mobile_draw_box(const sdl_mobile_select_box* box,
    const sdl_panel* panels)
{
    TTF_Font* font;
    float y;

    if (!box || box->panel_count <= 0 || box->rect.w <= 1.0f
        || box->rect.h <= 1.0f || box->line_h <= 0.0f)
    {
        return;
    }

    font = sdl_story_font_for_height_slot(box->px, SDL_STORY_FONT_SLOT_MENU);
    if (!font)
        return;

    y = box->rect.y;
    for (int i = 0; i < box->panel_count; i++)
    {
        const sdl_panel* panel = &panels[box->panel_first + i];
        float panel_rows = sdl_char_sheet_mobile_panel_rows(panel);
        float panel_h = panel_rows * box->line_h;

        sdl_char_sheet_panel_draw(panel, font, box->rect.x, y, box->rect.w,
            panel_h, box->line_h);
        y += panel_h + box->line_h * SDL_MOBILE_SELECT_PANEL_GAP_ROWS;
    }
}

/*
 * Mobile hero "carousel" body.  The title row and welcome line are drawn by the
 * screen renderer; this measures the remaining space and chooses the best of
 * several layouts: stats+traits stacked beside lore, stats/traits as separate
 * columns beside lore, or the same choices above lore for portrait screens.
 */
static void sdl_char_sheet_render_mobile_character_select(
    const sdl_char_sheet_line* list_lines, int list_count,
    sdl_panel* panels, int panel_count, float content_x, float top_y,
    float content_w, float region_h, int canvas_h, cptr desc,
    cptr desc_sizing, TTF_Font* prompt_font, float prompt_y, float prompt_h)
{
    float col_gap = sdl_char_sheet_clampf(content_w * 0.03f, 16.0f, 48.0f);
    float row_gap = sdl_char_sheet_clampf(region_h * 0.028f, 10.0f, 36.0f);
    cptr measure = (desc_sizing && desc_sizing[0]) ? desc_sizing : desc;
    int min_px = sdl_char_sheet_clampi((int)((float)canvas_h * 0.014f), 11,
        16);
    int max_px = sdl_char_sheet_clampi((int)((float)canvas_h * 0.058f), 22,
        68);
    sdl_mobile_select_layout best;

    (void)list_lines;
    (void)list_count;

    if (region_h <= 1.0f || content_w <= 1.0f)
        return;

    /* Whole region is a swipe zone (horizontal swipes change hero); there is
     * no scrollable list, so pin the scroll state inert. */
    g_sdl_character_sheet_screen.select_scroll_rect =
        (SDL_FRect){ content_x, top_y, content_w, region_h };
    g_sdl_character_sheet_screen.sheet_scroll = 0;
    g_sdl_character_sheet_screen.sheet_scroll_max = 0;
    g_sdl_character_sheet_screen.last_body_px = 0;
    g_sdl_character_sheet_screen.last_body_line_h = 0.0f;
    g_sdl_character_sheet_screen.last_desc_px = 0;
    g_sdl_character_sheet_screen.last_desc_line_h = 0.0f;

    sdl_char_sheet_mobile_layout_init(&best);
    if (panel_count <= 1)
    {
        sdl_mobile_select_layout candidate;

        sdl_char_sheet_mobile_layout_init(&candidate);
        candidate.lore_rect = (SDL_FRect){ content_x, top_y, content_w,
            region_h };
        sdl_char_sheet_mobile_consider_layout(&best, candidate, panels,
            measure, canvas_h, min_px, max_px);
    }
    else
    {
        sdl_char_sheet_mobile_add_side_layouts(&best, panels, panel_count,
            content_x, top_y, content_w, region_h, col_gap, measure,
            canvas_h, min_px, max_px);
        sdl_char_sheet_mobile_add_top_layouts(&best, panels, panel_count,
            content_x, top_y, content_w, region_h, col_gap, row_gap, measure,
            canvas_h, min_px, max_px);
    }

    if (best.score <= SDL_MOBILE_SELECT_BAD_SCORE * 0.5f)
    {
        sdl_mobile_select_layout candidate;
        float detail_h = (panel_count > 1) ? region_h * 0.40f : 0.0f;

        sdl_char_sheet_mobile_layout_init(&candidate);
        if (panel_count > 1)
            (void)sdl_char_sheet_mobile_layout_add_box(&candidate, 1,
                panel_count - 1, (SDL_FRect){ content_x, top_y, content_w,
                    detail_h });
        candidate.lore_rect = (SDL_FRect){ content_x,
            top_y + detail_h + row_gap, content_w,
            region_h - detail_h - row_gap };
        sdl_char_sheet_mobile_consider_layout(&best, candidate, panels,
            measure, canvas_h, min_px, max_px);
    }

    for (int i = 0; i < best.box_count; i++)
    {
        const sdl_mobile_select_box* box = &best.boxes[i];

        sdl_char_sheet_mobile_draw_box(box, panels);
        if (box->line_h > 0.0f
            && (g_sdl_character_sheet_screen.last_body_line_h <= 0.0f
                || box->line_h
                    < g_sdl_character_sheet_screen.last_body_line_h))
        {
            g_sdl_character_sheet_screen.last_body_px = box->px;
            g_sdl_character_sheet_screen.last_body_line_h =
                box->line_h * 0.96f;
        }
    }

    if (best.lore_rect.h > 1.0f && best.lore_rect.w > 1.0f
        && desc && desc[0])
    {
        TTF_Font* font = sdl_story_font_for_height_slot(best.desc_px,
            SDL_STORY_FONT_SLOT_CHAR_DESC);
        float line_h = best.desc_line_h;
        int lines = sdl_char_sheet_wrap_text(font, desc, best.lore_rect.w,
            NULL, 0);

        if (lines > 0)
        {
            g_sdl_character_sheet_screen.last_desc_px = best.desc_px;
            g_sdl_character_sheet_screen.last_desc_line_h = line_h;
            sdl_char_sheet_draw_history(font, desc, best.lore_rect.x,
                best.lore_rect.y, best.lore_rect.w, best.lore_rect.h, line_h,
                lines);
        }
    }

    /* Bottom prompt: controller labels on handhelds, touch buttons only on
     * touch-only devices. */
    sdl_char_sheet_draw_prompt(prompt_font, "", content_x, prompt_y,
        content_w, prompt_h);
    sdl_char_sheet_render_hover_tooltip();
}
#endif

/* Race "book" page-turn tuning. */
#define SDL_SELECT_PAGE_TURN_MS 850
#define SDL_BOOK_MAX_EMS 60.0f       /* reading measure cap (ems) */
#define SDL_BOOK_WIDTH_FRAC 0.58f    /* book-like column width (fraction of content) */
/* Horizontal / vertical page margin (text inset from the page edge), in body
 * line-heights.  Shared by the sizing helper, the page renderer and the turn. */
#define SDL_BOOK_MARGIN_H 1.10f
#define SDL_BOOK_MARGIN_V 0.85f

void sdl_select_page_turn_free(void);
void sdl_character_sheet_screen_render(void);
static void sdl_char_sheet_draw_story_lamp(float x, float y, float w,
    float h, u32b current, u32b maximum, TTF_Font* label_font,
    float label_h);

/* True while a parchment "book" is on screen -- either the birth/race book or a
 * narrative (quest) book.  The page-curl, swipe and snapshot code is shared. */
bool sdl_char_sheet_book_context(void)
{
    sdl_character_sheet_context c = g_sdl_character_sheet_screen.context;

    return g_sdl_character_sheet_screen.select_book_mode
        && (c == SDL_CHARACTER_SHEET_BIRTH_SELECT
            || c == SDL_CHARACTER_SHEET_NARRATIVE);
}

/* Tale Statistics is a chaptered chronicle.  Readability takes priority over
 * keeping every authored chapter on one physical page in either orientation;
 * dense chapters flow onto continuation pages. */
static bool sdl_char_sheet_chronicle(void)
{
    return g_sdl_character_sheet_screen.context
            == SDL_CHARACTER_SHEET_NARRATIVE
        && g_sdl_character_sheet_screen.narrative_contents_count > 0;
}

/* Portrait chronicles use the black narrative canvas directly. */
static bool sdl_char_sheet_portrait_chronicle(void)
{
    return g_sdl_narrative_portrait_rendering
        && sdl_char_sheet_chronicle();
}

static bool sdl_char_sheet_landscape_chronicle(void)
{
    return !g_sdl_narrative_portrait_rendering
        && sdl_char_sheet_chronicle();
}

/* The reading-column width for a given story px (a measure cap, but never wider
 * than a fraction of the content so the page keeps side margins). */
float sdl_char_sheet_book_width(int body_px, float content_w)
{
    float width_fraction = g_sdl_narrative_portrait_rendering
        ? 0.86f : SDL_BOOK_WIDTH_FRAC;
    float max_ems = g_sdl_narrative_portrait_rendering
        ? 68.0f : SDL_BOOK_MAX_EMS;
    float w = MIN(content_w * width_fraction, (float)body_px * max_ems);

    return (w < 1.0f) ? 1.0f : w;
}

static float sdl_char_sheet_race_book_text_width(int body_px, float content_w,
    bool full_width)
{
    if (full_width)
        return (content_w < 1.0f) ? 1.0f : content_w;
    return sdl_char_sheet_book_width(body_px, content_w);
}

static int sdl_char_sheet_race_book_page_count_for_intro(cptr intro)
{
#if SIL_SDL_MOBILE_BUILD
    if (intro && intro[0])
        return 5;
#else
    if (intro && intro[0])
        return 3;
#endif
    return 2;
}

static bool sdl_char_sheet_race_book_mobile_pages(void)
{
#if SIL_SDL_MOBILE_BUILD
    return g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_BIRTH_SELECT
        && g_sdl_character_sheet_screen.select_book_mode
        && g_sdl_character_sheet_screen.select_page_count >= 5;
#else
    return false;
#endif
}

static int sdl_char_sheet_book_choice_page(void)
{
    int page_count = g_sdl_character_sheet_screen.select_page_count;

    if (page_count < 1)
        page_count = 1;
    return page_count - 1;
}

/*
 * Pack the narrative paragraphs into pages at a given body size, breaking only
 * between whole paragraphs.  Fixed-page books (quest dialogue) use a balanced
 * partition instead of the normal greedy flow, so a short paragraph cannot be
 * stranded on a page by itself while later pages are much fuller.  Returns the
 * page count; if page_start is non-NULL it receives the first-paragraph index
 * of each page (with a [page_count] = para_count sentinel, padded out to
 * SDL_BOOK_MAX_PAGES).
 */
static int sdl_char_sheet_narrative_target_page_count(void);
static bool sdl_char_sheet_narrative_side_lamp_geometry(int body_px,
    float book_w, float region_h, float lh, float* text_w, float* lamp_x,
    float* lamp_w, float* lamp_h);

/* Tale Statistics has persistent chapter links.  In portrait they belong in
 * the leaf as a compact contents band, not in the narrow gutter beside it. */
static float sdl_char_sheet_portrait_contents_height(int body_px,
    float book_w, int* out_px, int* out_columns)
{
    int count = g_sdl_character_sheet_screen.narrative_contents_count;
    int px;
    int columns;
    int rows;
    int header_lines;
    TTF_Font* font;
    float lh;
    float column_gap;
    float column_w;
    float height;

    if (!g_sdl_narrative_portrait_rendering || count <= 0)
        return 0.0f;

    px = sdl_char_sheet_clampi((int)((float)body_px * 0.88f), 40, 64);
    columns = (count >= 4 && book_w >= (float)px * 8.5f) ? 2 : 1;
    rows = (count + columns - 1) / columns;
    font = sdl_story_font_for_height_slot(px,
        SDL_STORY_FONT_SLOT_NARRATIVE);
    lh = sdl_char_sheet_line_h(font, px, 1.24f);
    column_gap = lh * 0.72f;
    column_w = (book_w - column_gap * (float)(columns - 1))
        / (float)columns;
    header_lines = MAX(1,
        sdl_char_sheet_wrap_text(font, "Contents", book_w, NULL, 0));
    height = (float)header_lines * lh + lh * 0.35f;

    /* Measure every row at the largest wrapped entry in that row.  This lets
     * the Contents type stay large even when a translated or future chapter
     * label is wider than its column. */
    for (int row = 0; row < rows; row++)
    {
        int row_lines = 1;

        for (int column = 0; column < columns; column++)
        {
            int i = row * columns + column;
            int lines;

            if (i >= count)
                continue;
            lines = sdl_char_sheet_wrap_text(font,
                g_sdl_character_sheet_screen.narrative_contents_label[i],
                column_w, NULL, 0);
            row_lines = MAX(row_lines, MAX(1, lines));
        }
        height += (float)row_lines * lh;
        if (row + 1 < rows)
            height += lh * 0.24f;
    }
    height += lh * 0.62f;

    if (out_px)
        *out_px = px;
    if (out_columns)
        *out_columns = columns;
    return height;
}

/* Keep pagination's lamp reservation identical to the rendered bottom-lamp
 * geometry.  A fixed nine-line reservation becomes larger than a short
 * landscape page once Chronicle prose uses a genuinely readable font. */
static float sdl_char_sheet_narrative_bottom_lamp_height(float region_h,
    float lh)
{
    return MIN(lh * 8.5f, region_h * 0.48f);
}

int sdl_char_sheet_narrative_pack(int body_px, float content_w,
    float top_y, float region_bottom, int* page_start)
{
    int para_count = g_sdl_character_sheet_screen.narrative_para_count;
    TTF_Font* font = sdl_story_font_for_height_slot(body_px, SDL_STORY_FONT_SLOT_NARRATIVE);
    float book_w = sdl_char_sheet_book_width(body_px, content_w);
    float lh = sdl_char_sheet_line_h(font, body_px, 1.28f);
    float para_gap = lh * 0.6f;
    float region_h = (region_bottom - top_y) - 2.0f * (lh * SDL_BOOK_MARGIN_V);
    float used = (g_sdl_character_sheet_screen.narrative_lamp_enabled
            && g_sdl_character_sheet_screen.narrative_lamp_page == 0
            && !g_sdl_character_sheet_screen.narrative_lamp_side)
        ? sdl_char_sheet_narrative_bottom_lamp_height(region_h, lh) : 0.0f;
    int target_pages = g_sdl_character_sheet_screen.narrative_target_page_count;
    int page = 0;
    int i;

    if (region_h < lh)
        region_h = lh;
    region_h -= sdl_char_sheet_portrait_contents_height(body_px, book_w,
        NULL, NULL);
    if (region_h < lh)
        region_h = lh;
    if (page_start)
        page_start[0] = 0;

    /* Landscape chaptered books retain one physical page per authored section;
     * overflow is handled by the page's vertical scroller.  Portrait
     * chronicles instead gain continuation pages because their tall book
     * composition already makes those leaves natural. */
    if (g_sdl_character_sheet_screen.narrative_contents_count > 0
        && !sdl_char_sheet_portrait_chronicle())
    {
        page = 0;
        for (i = 1; i < para_count; i++)
        {
            if (!g_sdl_character_sheet_screen.narrative_para_break[i]
                || page + 1 >= SDL_BOOK_MAX_PAGES)
                continue;
            page++;
            if (page_start)
                page_start[page] = i;
        }
        if (page_start)
        {
            page_start[page + 1] = para_count;
            for (i = page + 2; i <= SDL_BOOK_MAX_PAGES; i++)
                page_start[i] = para_count;
        }
        return page + 1;
    }

    /* A requested page budget is used by quest books.  Find the partition with
     * the smallest fullest page, subject to every page fitting the actual page
     * region.  This keeps the body font global while allowing the text to use
     * the available pages evenly.  Explicit author breaks remain meaningful for
     * content-driven books, but fixed-page quest books are deliberately
     * rebalanced around them. */
    if (target_pages > 0)
    {
        float para_need[SDL_BOOK_MAX_PARAS];
        float prefix_need[SDL_BOOK_MAX_PARAS + 1];
        float dp[SDL_BOOK_MAX_PAGES + 1][SDL_BOOK_MAX_PARAS + 1];
        int previous[SDL_BOOK_MAX_PAGES + 1][SDL_BOOK_MAX_PARAS + 1];
        const float infinity = 1.0e30f;
        float lamp_h = (g_sdl_character_sheet_screen.narrative_lamp_enabled
                && !g_sdl_character_sheet_screen.narrative_lamp_side)
            ? sdl_char_sheet_narrative_bottom_lamp_height(region_h, lh)
            : 0.0f;
        if (target_pages > SDL_BOOK_MAX_PAGES)
            target_pages = SDL_BOOK_MAX_PAGES;
        if (para_count > 0 && target_pages > para_count)
            target_pages = para_count;

        prefix_need[0] = 0.0f;
        for (i = 0; i < para_count; i++)
        {
            int lines = sdl_char_sheet_wrap_text(font,
                g_sdl_character_sheet_screen.narrative_paras[i], book_w,
                NULL, 0);

            para_need[i] = (float)MAX(lines, 1) * lh;
            prefix_need[i + 1] = prefix_need[i] + para_need[i];
        }

        for (int p = 0; p <= SDL_BOOK_MAX_PAGES; p++)
        {
            for (i = 0; i <= SDL_BOOK_MAX_PARAS; i++)
            {
                dp[p][i] = infinity;
                previous[p][i] = -1;
            }
        }
        dp[0][0] = 0.0f;

        for (int p = 1; p <= target_pages; p++)
        {
            for (i = p; i <= para_count; i++)
            {
                for (int split = p - 1; split < i; split++)
                {
                    int para_count_on_page = i - split;
                    float page_h = prefix_need[i] - prefix_need[split];
                    float candidate;

                    if (para_count_on_page > 1)
                        page_h += para_gap * (float)(para_count_on_page - 1);
                    if (p - 1 == g_sdl_character_sheet_screen.narrative_lamp_page)
                        page_h += lamp_h;
                    if (page_h > region_h || dp[p - 1][split] >= infinity)
                        continue;

                    candidate = MAX(dp[p - 1][split], page_h);
                    if (candidate < dp[p][i])
                    {
                        dp[p][i] = candidate;
                        previous[p][i] = split;
                    }
                }
            }
        }

        if (target_pages > 0 && para_count > 0
            && dp[target_pages][para_count] < infinity)
        {
            int end = para_count;

            if (page_start)
            {
                page_start[target_pages] = para_count;
                for (int p = target_pages; p > 0; p--)
                {
                    int split = previous[p][end];

                    page_start[p - 1] = split;
                    end = split;
                }
                for (i = target_pages + 1; i <= SDL_BOOK_MAX_PAGES; i++)
                    page_start[i] = para_count;
            }
            return target_pages;
        }

        /* If whole paragraphs cannot fit into the requested budget even at this
         * size, fall through to the normal overflow-safe paginator rather than
         * drawing clipped text. */
    }

    for (i = 0; i < para_count; i++)
    {
        int lines = sdl_char_sheet_wrap_text(font,
            g_sdl_character_sheet_screen.narrative_paras[i], book_w, NULL, 0);
        float need = (float)lines * lh;
        float add = (used > 0.0f) ? need + para_gap : need;
        bool hard = (i > 0 && g_sdl_character_sheet_screen.narrative_para_break[i]);

        if (i > 0 && (hard || used + add > region_h)
            && page + 1 < SDL_BOOK_MAX_PAGES)
        {
            page++;
            if (page_start)
                page_start[page] = i;
            used = need
                + ((g_sdl_character_sheet_screen.narrative_lamp_enabled
                        && g_sdl_character_sheet_screen.narrative_lamp_page
                            == page
                        && !g_sdl_character_sheet_screen.narrative_lamp_side)
                      ? sdl_char_sheet_narrative_bottom_lamp_height(region_h,
                            lh)
                      : 0.0f);
        }
        else
        {
            used += add;
        }
    }

    if (page_start)
        for (i = page + 1; i <= SDL_BOOK_MAX_PAGES; i++)
            page_start[i] = para_count;

    return page + 1;
}

/* Cache the final wrap count for each paragraph after pagination settles.
 * Page rendering uses the same font and width repeatedly while animating or
 * waiting for input, so measuring every paragraph again on every present is
 * pure duplicate work.  The lamp page can have a narrower reading column. */
static void sdl_char_sheet_cache_narrative_line_counts(int body_px,
    float content_w, float top_y, float region_bottom)
{
    TTF_Font* font = sdl_story_font_for_height_slot(body_px,
        SDL_STORY_FONT_SLOT_NARRATIVE);
    float book_w = sdl_char_sheet_book_width(body_px, content_w);
    float lh = sdl_char_sheet_line_h(font, body_px, 1.28f);
    float region_h = (region_bottom - top_y)
        - 2.0f * (lh * SDL_BOOK_MARGIN_V);
    float lamp_text_w = book_w;
    bool side_lamp = false;
    int para_count = g_sdl_character_sheet_screen.narrative_para_count;

    if (region_h < lh)
        region_h = lh;
    region_h -= sdl_char_sheet_portrait_contents_height(body_px, book_w,
        NULL, NULL);
    if (region_h < lh)
        region_h = lh;
    if (g_sdl_character_sheet_screen.narrative_lamp_enabled
        && g_sdl_character_sheet_screen.narrative_lamp_side)
        side_lamp = sdl_char_sheet_narrative_side_lamp_geometry(body_px,
            book_w, region_h, lh, &lamp_text_w, NULL, NULL, NULL);

    memset(g_sdl_character_sheet_screen.narrative_para_lines, 0,
        sizeof(g_sdl_character_sheet_screen.narrative_para_lines));
    for (int page = 0;
         page < g_sdl_character_sheet_screen.narrative_page_count; page++)
    {
        int first = g_sdl_character_sheet_screen.narrative_page_start[page];
        int last = g_sdl_character_sheet_screen.narrative_page_start[page + 1];
        float width = (side_lamp
                && page == g_sdl_character_sheet_screen.narrative_lamp_page)
            ? lamp_text_w : book_w;

        if (first < 0)
            first = 0;
        if (last > para_count)
            last = para_count;
        for (int i = first; i < last; i++)
            g_sdl_character_sheet_screen.narrative_para_lines[i] = MAX(1,
                sdl_char_sheet_wrap_text(font,
                    g_sdl_character_sheet_screen.narrative_paras[i], width,
                    NULL, 0));
    }
}

/* Work out whether the lamp can sit beside the text at this size.  Keep a
 * useful reading measure and a comfortably proportioned lamp; otherwise the
 * bottom layout is clearer. */
static bool sdl_char_sheet_narrative_side_lamp_geometry(int body_px,
    float book_w, float region_h, float lh, float* text_w, float* lamp_x,
    float* lamp_w, float* lamp_h)
{
    float h = MIN(region_h * 0.78f, lh * 11.0f);
    float w = MIN(book_w * 0.30f, h * 0.58f);
    float gap = lh * 0.9f;
    float tw = book_w - w - gap;
    bool landscape_chronicle = sdl_char_sheet_landscape_chronicle();

    if (landscape_chronicle)
    {
        /* The Chronicle's short landscape leaf benefits from keeping the lamp
         * beside the prose.  Its proportional font remains readable at a
         * shorter measure than the generic quest-book threshold requires. */
        if (book_w < (float)body_px * 15.0f
            || region_h < lh * 6.0f
            || tw < (float)body_px * 10.0f
            || tw < book_w * 0.55f)
        {
            return false;
        }
    }
    else if (book_w < (float)body_px * 32.0f
        || region_h < lh * 8.0f
        || tw < (float)body_px * 22.0f
        || tw < book_w * 0.58f)
    {
        return false;
    }

    if (text_w) *text_w = tw;
    if (lamp_x) *lamp_x = tw + gap;
    if (lamp_w) *lamp_w = w;
    if (lamp_h) *lamp_h = h;
    return true;
}

/* Check whether every author-defined contents section fits on one physical
 * page at this body size and lamp placement. */
static bool sdl_char_sheet_narrative_sections_fit(int body_px,
    float content_w, float top_y, float region_bottom, bool side_lamp)
{
    TTF_Font* font = sdl_story_font_for_height_slot(body_px,
        SDL_STORY_FONT_SLOT_NARRATIVE);
    float book_w = sdl_char_sheet_book_width(body_px, content_w);
    float lh = sdl_char_sheet_line_h(font, body_px, 1.28f);
    float para_gap = lh * 0.6f;
    float region_h = (region_bottom - top_y)
        - 2.0f * (lh * SDL_BOOK_MARGIN_V);
    float side_text_w = book_w;
    float used;
    bool side_available = false;
    int page = 0;
    int paras_on_page = 0;

    if (region_h < lh)
        region_h = lh;
    if (side_lamp && g_sdl_character_sheet_screen.narrative_lamp_enabled)
        side_available = sdl_char_sheet_narrative_side_lamp_geometry(body_px,
            book_w, region_h, lh, &side_text_w, NULL, NULL, NULL);
    if (side_lamp && !side_available)
        return false;

    used = (g_sdl_character_sheet_screen.narrative_lamp_enabled
            && g_sdl_character_sheet_screen.narrative_lamp_page == 0
            && !side_lamp)
        ? lh * 9.0f : 0.0f;

    for (int i = 0;
         i < g_sdl_character_sheet_screen.narrative_para_count; i++)
    {
        int lines;
        float need;

        if (i > 0 && g_sdl_character_sheet_screen.narrative_para_break[i])
        {
            if (used > region_h)
                return false;
            page++;
            used = (g_sdl_character_sheet_screen.narrative_lamp_enabled
                    && g_sdl_character_sheet_screen.narrative_lamp_page
                        == page && !side_lamp)
                ? lh * 9.0f : 0.0f;
            paras_on_page = 0;
        }

        lines = sdl_char_sheet_wrap_text(font,
            g_sdl_character_sheet_screen.narrative_paras[i],
            (side_lamp
                && page == g_sdl_character_sheet_screen.narrative_lamp_page)
                ? side_text_w : book_w,
            NULL, 0);
        need = (float)MAX(lines, 1) * lh;
        if (paras_on_page > 0 || used > 0.0f)
            need += para_gap;
        used += need;
        paras_on_page++;
    }

    return used <= region_h;
}

static int sdl_char_sheet_narrative_target_page_count(void)
{
    int target = g_sdl_character_sheet_screen.narrative_target_page_count;
    int para_count = g_sdl_character_sheet_screen.narrative_para_count;

    if (target <= 0)
        return 0;
    if (target > SDL_BOOK_MAX_PAGES)
        target = SDL_BOOK_MAX_PAGES;
    if (para_count > 0 && target > para_count)
        target = para_count;

    return target;
}

static void sdl_char_sheet_narrative_expand_pages_to_target(
    int target, int* page_start, int* page_count)
{
    int current;
    int para_count = g_sdl_character_sheet_screen.narrative_para_count;
    int segment_pages[SDL_BOOK_MAX_PAGES];
    int out[SDL_BOOK_MAX_PAGES + 1];
    int extra;
    int out_count = 0;
    int new_page_count;
    int i;

    if (!page_start || !page_count)
        return;
    current = *page_count;
    if (target <= current || para_count <= current)
        return;
    if (target > para_count)
        target = para_count;

    for (i = 0; i < current; i++)
        segment_pages[i] = 1;

    extra = target - current;
    while (extra > 0)
    {
        int best = -1;
        int best_capacity = 0;

        for (i = 0; i < current; i++)
        {
            int start = page_start[i];
            int end = page_start[i + 1];
            int capacity = (end - start) - segment_pages[i];

            if (capacity > best_capacity)
            {
                best = i;
                best_capacity = capacity;
            }
        }

        if (best < 0)
            break;
        segment_pages[best]++;
        extra--;
    }

    out[out_count++] = page_start[0];
    for (i = 0; i < current && out_count < SDL_BOOK_MAX_PAGES; i++)
    {
        int start = page_start[i];
        int end = page_start[i + 1];
        int pages = segment_pages[i];
        int len = end - start;
        int split;

        if (pages < 1)
            pages = 1;
        if (pages > len)
            pages = len;

        for (split = 1; split < pages
             && out_count < SDL_BOOK_MAX_PAGES; split++)
        {
            int boundary = start + (len * split + pages / 2) / pages;

            if (boundary <= out[out_count - 1])
                boundary = out[out_count - 1] + 1;
            if (boundary >= end)
                boundary = end - 1;
            if (boundary > out[out_count - 1])
                out[out_count++] = boundary;
        }

        if (end > out[out_count - 1] && out_count < SDL_BOOK_MAX_PAGES + 1)
            out[out_count++] = end;
    }

    if (out_count < 1)
        return;
    if (out[out_count - 1] != para_count)
    {
        if (out_count < SDL_BOOK_MAX_PAGES + 1)
            out[out_count++] = para_count;
        else
            out[out_count - 1] = para_count;
    }

    new_page_count = out_count - 1;
    if (new_page_count < 1)
        new_page_count = 1;
    if (new_page_count > SDL_BOOK_MAX_PAGES)
        new_page_count = SDL_BOOK_MAX_PAGES;
    for (i = 0; i <= SDL_BOOK_MAX_PAGES; i++)
        page_start[i] = (i <= new_page_count) ? out[i] : para_count;

    *page_count = new_page_count;
}

/*
 * Choose ONE body size for the whole narrative book.  The race book shrinks a
 * single page to fit; narrative text is paginated instead, so we pick the
 * LARGEST readable size that still packs into the fewest pages -- that size
 * fills those pages most fully, and every page of the quest shares it.
 */
int sdl_char_sheet_narrative_choose_px(float canvas_h, float content_w,
    float top_y, float region_bottom)
{
    int min_px = sdl_char_sheet_clampi((int)(canvas_h * 0.030f), 18, 32);
    int target_pages = sdl_char_sheet_narrative_target_page_count();
    /* This is only the upper bound for the fit search.  The actual size is
     * selected below against the fullest balanced page.  Fixed-page quest books
     * get a higher search ceiling so the page geometry—not the former 46 px
     * cap—decides the result; content-driven books retain their existing range. */
    int max_px = target_pages > 0
        ? sdl_char_sheet_clampi((int)(canvas_h * 0.075f), 30, 72)
        : sdl_char_sheet_clampi((int)(canvas_h * 0.048f), 26, 46);
    int min_pages;
    int chosen;

    if (max_px < min_px)
        max_px = min_px;

    /* Portrait has enough height for the large Chronicle typography and
     * continuation leaves. */
    if (sdl_char_sheet_portrait_chronicle())
    {
        g_sdl_character_sheet_screen.narrative_lamp_side = false;
        return sdl_char_sheet_clampi((int)(canvas_h * 0.060f), 56, 76);
    }

    /* Landscape keeps one authored chapter per leaf.  Fit down only to a
     * readable floor; if the fullest chapter still exceeds the short page,
     * keep that floor and let the chapter scroll instead of clipping or using
     * tiny type. */
    if (g_sdl_character_sheet_screen.narrative_contents_count > 0)
    {
        bool landscape_chronicle = sdl_char_sheet_landscape_chronicle();
        int contents_min_px = landscape_chronicle
            ? sdl_char_sheet_clampi((int)(canvas_h * 0.050f), 32, 38)
            : sdl_char_sheet_clampi((int)(canvas_h * 0.018f), 11, 18);
        int contents_max_px = landscape_chronicle
            ? sdl_char_sheet_clampi((int)(canvas_h * 0.070f), 40, 52)
            : max_px;
        int side_px = contents_min_px - 1;
        int bottom_px = contents_min_px - 1;
        int low = contents_min_px;
        int high;

        if (contents_max_px < contents_min_px)
            contents_max_px = contents_min_px;
        high = contents_max_px;

        while (low <= high) {
            int middle = low + (high - low) / 2;

            if (sdl_char_sheet_narrative_sections_fit(middle, content_w,
                    top_y, region_bottom, true)) {
                side_px = middle;
                low = middle + 1;
            } else {
                high = middle - 1;
            }
        }
        low = contents_min_px;
        high = contents_max_px;
        while (low <= high) {
            int middle = low + (high - low) / 2;

            if (sdl_char_sheet_narrative_sections_fit(middle, content_w,
                    top_y, region_bottom, false)) {
                bottom_px = middle;
                low = middle + 1;
            } else {
                high = middle - 1;
            }
        }
        if (side_px >= contents_min_px || bottom_px >= contents_min_px) {
            g_sdl_character_sheet_screen.narrative_lamp_side =
                (side_px >= bottom_px);
            return MAX(side_px, bottom_px);
        }
        if (landscape_chronicle
            && g_sdl_character_sheet_screen.narrative_lamp_enabled)
        {
            TTF_Font* font = sdl_story_font_for_height_slot(contents_min_px,
                SDL_STORY_FONT_SLOT_NARRATIVE);
            float book_w = sdl_char_sheet_book_width(contents_min_px,
                content_w);
            float lh = sdl_char_sheet_line_h(font, contents_min_px, 1.28f);
            float region_h = (region_bottom - top_y)
                - 2.0f * (lh * SDL_BOOK_MARGIN_V);

            g_sdl_character_sheet_screen.narrative_lamp_side =
                sdl_char_sheet_narrative_side_lamp_geometry(contents_min_px,
                    book_w, MAX(lh, region_h), lh, NULL, NULL, NULL, NULL);
        }
        else
        {
            g_sdl_character_sheet_screen.narrative_lamp_side = false;
        }
        return contents_min_px;
    }

    /* Quest dialogue can request a fixed page budget; pick the largest body
     * size that still fits in it, then add soft paragraph breaks if needed. */
    if (target_pages > 0)
    {
        int low = min_px;
        int high = max_px;

        chosen = min_px;
        while (low <= high) {
            int middle = low + (high - low) / 2;

            if (sdl_char_sheet_narrative_pack(middle, content_w, top_y,
                    region_bottom, NULL) <= target_pages) {
                chosen = middle;
                low = middle + 1;
            } else {
                high = middle - 1;
            }
        }
        return chosen;
    }

    min_pages = sdl_char_sheet_narrative_pack(min_px, content_w, top_y,
        region_bottom, NULL);
    chosen = min_px;
    {
        int low = min_px + 1;
        int high = max_px;

        while (low <= high) {
            int middle = low + (high - low) / 2;

            if (sdl_char_sheet_narrative_pack(middle, content_w, top_y,
                    region_bottom, NULL) <= min_pages) {
                chosen = middle;
                low = middle + 1;
            } else {
                high = middle - 1;
            }
        }
    }
    return chosen;
}

static cptr sdl_char_sheet_book_heading_label(
    const sdl_character_sheet_select_row* row, char* buf, size_t buflen,
    bool compact)
{
    char* cut = NULL;
    char* colon;

    if (!row)
        return "";
    if (!compact || !row->is_heading || !buf || buflen == 0)
        return row->label;

    SDL_strlcpy(buf, row->label, buflen);
    cut = strstr(buf, "\xe2\x80\x94");
    colon = strchr(buf, ':');
    if (!cut || (colon && colon < cut))
        cut = colon;

    if (cut)
    {
        while (cut > buf && isspace((unsigned char)cut[-1]))
            cut--;
        *cut = '\0';
    }

    return buf[0] ? buf : row->label;
}

static int sdl_char_sheet_book_second_heading_index(void)
{
    int row_count = g_sdl_character_sheet_screen.select_row_count;
    int heading_count = 0;

    for (int i = 0; i < row_count; i++)
    {
        if (!g_sdl_character_sheet_screen.select_rows[i].is_heading)
            continue;
        heading_count++;
        if (heading_count == 2)
            return i;
    }

    return (row_count + 1) / 2;
}

static float sdl_char_sheet_book_rows_height(TTF_Font* font, float row_w,
    float body_lh, float list_lh, int first, int last, bool compact_headings)
{
    float h = 0.0f;
    int row_count = g_sdl_character_sheet_screen.select_row_count;

    if (!font || row_w <= 0.0f || body_lh <= 0.0f || list_lh <= 0.0f)
        return 0.0f;
    if (first < 0)
        first = 0;
    if (last > row_count)
        last = row_count;
    if (last < first)
        last = first;

    for (int i = first; i < last; i++)
    {
        const sdl_character_sheet_select_row* row =
            &g_sdl_character_sheet_screen.select_rows[i];

        if (row->is_heading)
        {
            char label[160];
            cptr text = sdl_char_sheet_book_heading_label(row, label,
                sizeof(label), compact_headings);
            int lines = sdl_char_sheet_wrap_text(font, text, row_w, NULL, 0);

            if (lines < 1)
                lines = 1;
            h += (float)lines * body_lh;
            if (i + 1 < last)
                h += body_lh * 0.25f;
        }
        else
        {
            h += list_lh;
        }
    }

    return h;
}

static void sdl_char_sheet_draw_book_row_range(TTF_Font* font, float x,
    float y, float w, float region_bottom, float body_lh, float list_lh,
    int first, int last, bool compact_headings, bool register_hits)
{
    int row_count = g_sdl_character_sheet_screen.select_row_count;

    if (!font || w <= 0.0f || body_lh <= 0.0f || list_lh <= 0.0f)
        return;
    if (first < 0)
        first = 0;
    if (last > row_count)
        last = row_count;
    if (last < first)
        last = first;

    for (int i = first; i < last; i++)
    {
        const sdl_character_sheet_select_row* row =
            &g_sdl_character_sheet_screen.select_rows[i];

        if (row->is_heading)
        {
            char label[160];
            cptr text = sdl_char_sheet_book_heading_label(row, label,
                sizeof(label), compact_headings);
            int lines = sdl_char_sheet_wrap_text(font, text, w, NULL, 0);
            float h;

            if (lines < 1)
                lines = 1;
            h = (float)lines * body_lh;
            if (y + body_lh * 0.5f > region_bottom)
                break;

            sdl_char_sheet_draw_wrapped(font, text, TERM_SLATE, x, y, w,
                h + body_lh * 0.25f, body_lh, lines);
            y += h + body_lh * 0.25f;
        }
        else
        {
            bool focused = sdl_char_sheet_choice_focused(row->choice);
            float indent = body_lh * 0.28f;
            float text_x = x + indent;
            float text_w = w - indent;
            float text_y = y + (list_lh - body_lh) * 0.5f;
            int tw;
            SDL_FRect focus;
            SDL_FRect hit;

            if (text_w < w * 0.70f)
            {
                text_x = x;
                text_w = w;
            }
            if (text_y + body_lh * 0.5f > region_bottom)
                break;
            if (text_y < y)
                text_y = y;

            tw = sdl_char_sheet_text_width(font, row->label);
            focus = (SDL_FRect){ text_x, text_y,
                MIN(text_w, (float)tw + body_lh * 0.5f), body_lh };
            hit = (SDL_FRect){ text_x, y, text_w, list_lh };

            if (focused)
                sdl_char_sheet_draw_focus_rect(focus, true);
            (void)sdl_char_sheet_draw_text(font, row->label,
                sdl_char_sheet_focus_text_attr(row->attr, focused), text_x,
                text_y, text_w, body_lh * 0.95f, false);
            if (register_hits && row->choice >= 0)
                sdl_char_sheet_add_hit(hit, row->choice, "", TERM_WHITE);
            y += list_lh;
        }
    }
}

static cptr sdl_char_sheet_skip_story_ws(cptr text)
{
    if (!text)
        return "";
    while (*text && isspace((unsigned char)*text))
        text++;
    return text;
}

static void sdl_char_sheet_copy_story_slice(cptr start, size_t len, char* out,
    size_t out_len)
{
    if (!out || out_len == 0)
        return;
    out[0] = '\0';
    if (!start || len == 0)
        return;

    while (len > 0 && isspace((unsigned char)*start))
    {
        start++;
        len--;
    }
    while (len > 0 && isspace((unsigned char)start[len - 1]))
        len--;
    if (len >= out_len)
        len = out_len - 1;

    memcpy(out, start, len);
    out[len] = '\0';
}

static int sdl_char_sheet_story_lines(TTF_Font* font, cptr text, float width)
{
    if (!font || !text || !text[0] || width <= 0.0f)
        return 0;
    return sdl_char_sheet_wrap_text(font, text, width, NULL, 0);
}

static int sdl_char_sheet_collect_intro_breaks(cptr intro, size_t* breaks,
    int max_count)
{
    size_t len;
    int count = 0;

    if (!intro || !breaks || max_count <= 0)
        return 0;
    len = strlen(intro);
    for (size_t i = 0; i < len; i++)
    {
        unsigned char ch = (unsigned char)intro[i];
        size_t pos = 0;
        size_t next;

        if ((ch == '.' || ch == '?' || ch == '!')
            && (i + 1 < len) && isspace((unsigned char)intro[i + 1]))
        {
            pos = i + 1;
        }
        else if (ch == '\n' && (i + 1 < len) && intro[i + 1] == '\n')
        {
            pos = i;
        }

        if (pos == 0)
            continue;

        next = pos;
        while (next < len && isspace((unsigned char)intro[next]))
            next++;
        if (next < len)
        {
            if (count == 0 || breaks[count - 1] != pos)
                breaks[count++] = pos;
            if (count >= max_count)
                break;
        }
    }

    return count;
}

/*
 * Split the chronicle into two story pages for desktop.  Page 0 carries the
 * trial frame, page 1 carries the charge, so the scoring includes those fixed
 * line counts and chooses the most visually balanced sentence boundary.
 */
static bool sdl_char_sheet_split_intro_two_pages_balanced(cptr intro,
    TTF_Font* font, float width, int head_lines, int tail_lines,
    char* first, size_t first_len, cptr* second)
{
    size_t breaks[32];
    int break_count;
    int best_score = -1;
    size_t best_break = 0;
    size_t intro_len;

    if (first && first_len > 0)
        first[0] = '\0';
    if (second)
        *second = intro ? intro : "";
    if (!intro || !intro[0] || !first || first_len == 0 || !second || !font
        || width <= 0.0f)
    {
        return false;
    }

    intro_len = strlen(intro);
    break_count = sdl_char_sheet_collect_intro_breaks(intro, breaks,
        (int)N_ELEMENTS(breaks));
    if (break_count < 1)
        return false;

    for (int i = 0; i < break_count; i++)
    {
        char one[2048];
        cptr two;
        int l0;
        int l1;
        int spread;
        int total;
        int score;

        if (breaks[i] >= intro_len)
            continue;

        two = sdl_char_sheet_skip_story_ws(intro + breaks[i]);
        sdl_char_sheet_copy_story_slice(intro, breaks[i], one, sizeof(one));
        if (!one[0] || !two[0])
            continue;

        l0 = head_lines + sdl_char_sheet_story_lines(font, one, width);
        l1 = tail_lines + sdl_char_sheet_story_lines(font, two, width);
        spread = ABS(l0 - l1);
        total = l0 + l1;
        score = spread * 100
            + ABS(l0 * 2 - total)
            + ABS(l1 * 2 - total);

        if (best_score < 0 || score < best_score)
        {
            best_score = score;
            best_break = breaks[i];
        }
    }

    if (best_score < 0)
        return false;

    sdl_char_sheet_copy_story_slice(intro, best_break, first, first_len);
    *second = sdl_char_sheet_skip_story_ws(intro + best_break);
    return first[0] && (*second)[0];
}

/*
 * Split the chronicle (intro) into four balanced story pages for mobile.
 * Candidate cuts are sentence boundaries, which lets the longer third paragraph
 * share its sentences instead of forcing lopsided paragraph-only pages.  The
 * score uses rendered line counts and includes the fixed accent framing:
 * page 0 bears the trial frame, page 3 bears the charge.
 */
static bool sdl_char_sheet_split_intro_four_pages_balanced(cptr intro,
    TTF_Font* font, float width, int head_lines, int tail_lines,
    char* first, size_t first_len, char* second, size_t second_len,
    char* third, size_t third_len, cptr* fourth)
{
    size_t breaks[32];
    int break_count;
    int best_score = -1;
    size_t best_first = 0;
    size_t best_second = 0;
    size_t best_third = 0;
    size_t intro_len;

    if (first && first_len > 0)
        first[0] = '\0';
    if (second && second_len > 0)
        second[0] = '\0';
    if (third && third_len > 0)
        third[0] = '\0';
    if (fourth)
        *fourth = intro ? intro : "";
    if (!intro || !intro[0] || !first || first_len == 0 || !second
        || second_len == 0 || !third || third_len == 0 || !fourth || !font
        || width <= 0.0f)
        return false;

    intro_len = strlen(intro);
    break_count = sdl_char_sheet_collect_intro_breaks(intro, breaks,
        (int)N_ELEMENTS(breaks));
    if (break_count < 3)
        return false;

    for (int a = 0; a < break_count - 2; a++)
    {
        for (int b = a + 1; b < break_count - 1; b++)
        {
            for (int c = b + 1; c < break_count; c++)
            {
                char one[2048];
                char two[2048];
                char three_buf[2048];
                cptr four = sdl_char_sheet_skip_story_ws(intro + breaks[c]);
                int l0;
                int l1;
                int l2;
                int l3;
                int max_l;
                int min_l;
                int spread;
                int total;
                int score;

                if (breaks[a] >= intro_len || breaks[b] >= intro_len
                    || breaks[c] >= intro_len)
                    continue;

                sdl_char_sheet_copy_story_slice(intro, breaks[a], one,
                    sizeof(one));
                sdl_char_sheet_copy_story_slice(intro + breaks[a],
                    breaks[b] - breaks[a], two, sizeof(two));
                sdl_char_sheet_copy_story_slice(intro + breaks[b],
                    breaks[c] - breaks[b], three_buf, sizeof(three_buf));
                if (!one[0] || !two[0] || !three_buf[0] || !four[0])
                    continue;

                l0 = head_lines + sdl_char_sheet_story_lines(font, one, width);
                l1 = sdl_char_sheet_story_lines(font, two, width);
                l2 = sdl_char_sheet_story_lines(font, three_buf, width);
                l3 = tail_lines + sdl_char_sheet_story_lines(font, four, width);
                max_l = MAX(MAX(l0, l1), MAX(l2, l3));
                min_l = MIN(MIN(l0, l1), MIN(l2, l3));
                spread = max_l - min_l;
                total = l0 + l1 + l2 + l3;
                score = spread * 100
                    + ABS(l0 * 4 - total)
                    + ABS(l1 * 4 - total)
                    + ABS(l2 * 4 - total)
                    + ABS(l3 * 4 - total);

                if (best_score < 0 || score < best_score)
                {
                    best_score = score;
                    best_first = breaks[a];
                    best_second = breaks[b];
                    best_third = breaks[c];
                }
            }
        }
    }

    if (best_score < 0)
        return false;

    sdl_char_sheet_copy_story_slice(intro, best_first, first, first_len);
    sdl_char_sheet_copy_story_slice(intro + best_first,
        best_second - best_first, second, second_len);
    sdl_char_sheet_copy_story_slice(intro + best_second,
        best_third - best_second, third, third_len);
    *fourth = sdl_char_sheet_skip_story_ws(intro + best_third);
    return first[0] && second[0] && third[0] && (*fourth)[0];
}

static bool sdl_char_sheet_book_story_pages_fit(int px, float content_w,
    float region_h, bool mobile_pages, bool desktop_pages)
{
    cptr frame_top = g_sdl_character_sheet_screen.select_frame_top;
    cptr intro = g_sdl_character_sheet_screen.select_intro;
    cptr frame_bottom = g_sdl_character_sheet_screen.select_frame_bottom;
    TTF_Font* f = sdl_story_font_for_height_slot(px,
        SDL_STORY_FONT_SLOT_CHAR_SELECT);
    float lh;
    float story_w;
    float avail;
    int ft;
    int in;
    int fb;
    char intro_first[2048];
    char intro_second[2048];
    char intro_third[2048];
    cptr intro_fourth = "";
    cptr intro_desktop_second = "";
    bool desktop_split_intro;
    bool split_intro;
    int in_first;
    int in_second;
    int in_third;
    int in_fourth;
    float page0;
    float page1 = 0.0f;
    float page2 = 0.0f;
    float page3 = 0.0f;
    float need;

    if (!f)
        return false;
    lh = sdl_char_sheet_line_h(f, px, 1.28f);
    story_w = sdl_char_sheet_book_width(px, content_w);
    avail = region_h - 2.0f * (lh * SDL_BOOK_MARGIN_V);
    ft = (frame_top && frame_top[0])
        ? sdl_char_sheet_wrap_text(f, frame_top, story_w, NULL, 0) : 0;
    in = (intro && intro[0])
        ? sdl_char_sheet_wrap_text(f, intro, story_w, NULL, 0) : 0;
    fb = (frame_bottom && frame_bottom[0])
        ? sdl_char_sheet_wrap_text(f, frame_bottom, story_w, NULL, 0) : 0;
    desktop_split_intro = desktop_pages && intro && intro[0]
        && sdl_char_sheet_split_intro_two_pages_balanced(intro, f, story_w,
            ft, fb, intro_first, sizeof(intro_first),
            &intro_desktop_second);
    split_intro = mobile_pages && intro && intro[0]
        && sdl_char_sheet_split_intro_four_pages_balanced(intro, f, story_w,
            ft, fb, intro_first, sizeof(intro_first), intro_second,
            sizeof(intro_second), intro_third, sizeof(intro_third),
            &intro_fourth);
    in_first = split_intro
        ? sdl_char_sheet_story_lines(f, intro_first, story_w)
        : (desktop_split_intro
            ? sdl_char_sheet_story_lines(f, intro_first, story_w) : in);
    in_second = split_intro
        ? sdl_char_sheet_story_lines(f, intro_second, story_w)
        : (desktop_split_intro
            ? sdl_char_sheet_story_lines(f, intro_desktop_second, story_w)
            : 0);
    in_third = split_intro
        ? sdl_char_sheet_story_lines(f, intro_third, story_w) : 0;
    in_fourth = split_intro
        ? sdl_char_sheet_story_lines(f, intro_fourth, story_w) : 0;

    /* Match the mandatory story-page layouts at this candidate size. */
    page0 = (float)(ft + in_first) * lh + lh * 0.35f;
    if (mobile_pages) {
        page1 = (float)in_second * lh;
        page2 = (float)in_third * lh;
        page3 = (float)(in_fourth + fb) * lh + lh * 0.8f;
    } else if (desktop_pages) {
        float charge_gap = (in_second > 0 && fb > 0) ? lh * 0.8f : 0.0f;

        page1 = (float)(in_second + fb) * lh + charge_gap;
    }
    need = MAX(MAX(page0, page1), MAX(page2, page3));
    return need <= avail;
}

/*
 * Choose one shared body size (no bigger than the title) for every race-book
 * story page.  The final selection page is deliberately excluded and fitted
 * independently below, so its longer list+lore layout cannot shrink the
 * chronicle.
 */
int sdl_char_sheet_book_body_px(float canvas_h, float content_w,
    float top_y, float region_bottom, int title_px)
{
    float region_h = (region_bottom > top_y) ? (region_bottom - top_y) : 1.0f;
    int min_px = sdl_char_sheet_clampi((int)(canvas_h * 0.018f), 14, 24);
    int lowest_px;
    int low_index;
    int high_index;
    int body_px;
    bool mobile_pages = sdl_char_sheet_race_book_mobile_pages();
    bool desktop_pages = !mobile_pages
        && g_sdl_character_sheet_screen.context
            == SDL_CHARACTER_SHEET_BIRTH_SELECT
        && g_sdl_character_sheet_screen.select_book_mode
        && g_sdl_character_sheet_screen.select_page_count >= 3;

    if (title_px < min_px)
        return min_px;
    lowest_px = title_px - ((title_px - min_px) / 2) * 2;
    body_px = lowest_px;
    low_index = 0;
    high_index = (title_px - lowest_px) / 2;
    while (low_index <= high_index) {
        int index = low_index + (high_index - low_index) / 2;
        int px = lowest_px + index * 2;

        if (sdl_char_sheet_book_story_pages_fit(px, content_w, region_h,
                mobile_pages, desktop_pages))
        {
            body_px = px;
            low_index = index + 1;
        } else {
            high_index = index - 1;
        }
    }
    return body_px;
}

/*
 * Choose the final race-selection page's body size independently.  Measure
 * every candidate description so moving focus never changes the size or clips
 * a race whose particular word breaks wrap onto more lines.
 */
static bool sdl_char_sheet_book_choice_page_fits(int px, float content_w,
    float region_h, bool mobile_pages, bool show_charge)
{
    cptr frame_bottom = g_sdl_character_sheet_screen.select_frame_bottom;
    int row_count = g_sdl_character_sheet_screen.select_row_count;
    TTF_Font* f = sdl_story_font_for_height_slot(px,
        SDL_STORY_FONT_SLOT_CHAR_SELECT);
    float lh;
    float list_lh;
    float choice_w;
    float avail;
    int fb_lines;
    int lore_lines;
    float list_h;
    float need;

    if (!f)
        return false;
    lh = sdl_char_sheet_line_h(f, px, 1.28f);
    list_lh = lh * 1.35f;
    choice_w = sdl_char_sheet_race_book_text_width(px, content_w,
        mobile_pages);
    avail = mobile_pages
        ? region_h : region_h - 2.0f * (lh * SDL_BOOK_MARGIN_V);
    fb_lines = (show_charge && frame_bottom && frame_bottom[0])
        ? sdl_char_sheet_wrap_text(f, frame_bottom, choice_w, NULL, 0) : 0;
    lore_lines = sdl_char_sheet_select_description_line_count(f,
        g_sdl_character_sheet_screen.select_description, choice_w);
    list_h = (float)row_count * list_lh;

    if (mobile_pages) {
        int split = sdl_char_sheet_book_second_heading_index();
        float col_gap = lh * 1.15f;
        float col_w = (choice_w - col_gap) * 0.5f;
        float left_h;
        float right_h;

        if (col_w < 1.0f)
            col_w = choice_w * 0.5f;
        left_h = sdl_char_sheet_book_rows_height(f, col_w, lh, list_lh,
            0, split, true);
        right_h = sdl_char_sheet_book_rows_height(f, col_w, lh, list_lh,
            split, row_count, true);
        list_h = MAX(left_h, right_h);
    }

    need = list_h + (float)lore_lines * lh + lh * 0.8f;
    if (fb_lines > 0)
        need += (float)fb_lines * lh + lh * 0.8f;
    return need <= avail;
}

static int sdl_char_sheet_book_choice_body_px(float canvas_h, float content_w,
    float top_y, float region_bottom, int title_px)
{
    float region_h = (region_bottom > top_y) ? (region_bottom - top_y) : 1.0f;
    int min_px = sdl_char_sheet_clampi((int)(canvas_h * 0.018f), 14, 24);
    int lowest_px;
    int low_index;
    int high_index;
    int body_px;
    bool mobile_pages = sdl_char_sheet_race_book_mobile_pages();
    bool desktop_pages = !mobile_pages
        && g_sdl_character_sheet_screen.context
            == SDL_CHARACTER_SHEET_BIRTH_SELECT
        && g_sdl_character_sheet_screen.select_book_mode
        && g_sdl_character_sheet_screen.select_page_count >= 3;
    bool show_charge = !(mobile_pages || desktop_pages);

    if (title_px < min_px)
        return min_px;
    lowest_px = title_px - ((title_px - min_px) / 2) * 2;
    body_px = lowest_px;
    low_index = 0;
    high_index = (title_px - lowest_px) / 2;
    while (low_index <= high_index) {
        int index = low_index + (high_index - low_index) / 2;
        int px = lowest_px + index * 2;

        if (sdl_char_sheet_book_choice_page_fits(px, content_w, region_h,
                mobile_pages, show_charge))
        {
            body_px = px;
            low_index = index + 1;
        } else {
            high_index = index - 1;
        }
    }

    return body_px;
}

static void sdl_char_sheet_book_body_px_cache_clear(void)
{
    g_sdl_character_sheet_screen.select_book_body_px = 0;
    g_sdl_character_sheet_screen.select_book_choice_body_px = 0;
    g_sdl_character_sheet_screen.select_book_body_for_h = -1;
    g_sdl_character_sheet_screen.select_book_body_for_w = -1;
    g_sdl_character_sheet_screen.select_book_body_for_region_h = -1;
    g_sdl_character_sheet_screen.select_book_body_for_title_px = -1;
    g_sdl_character_sheet_screen.select_book_body_for_layout_hash = 0;
}

static Uint64 sdl_char_sheet_book_layout_hash_bytes(Uint64 hash, cptr text)
{
    const unsigned char* p = (const unsigned char*)(text ? text : "");

    while (*p)
    {
        hash ^= (Uint64)*p++;
        hash *= 1099511628211ULL;
    }
    hash ^= 0xffU;
    hash *= 1099511628211ULL;
    return hash;
}

static Uint64 sdl_char_sheet_book_layout_hash(void)
{
    Uint64 hash = 14695981039346656037ULL;
    int row_count = g_sdl_character_sheet_screen.select_row_count;

    hash ^= (Uint64)g_sdl_character_sheet_screen.select_page_count;
    hash *= 1099511628211ULL;
    hash = sdl_char_sheet_book_layout_hash_bytes(hash,
        g_sdl_character_sheet_screen.select_frame_top);
    hash = sdl_char_sheet_book_layout_hash_bytes(hash,
        g_sdl_character_sheet_screen.select_intro);
    hash = sdl_char_sheet_book_layout_hash_bytes(hash,
        g_sdl_character_sheet_screen.select_frame_bottom);
    hash ^= (Uint64)row_count;
    hash *= 1099511628211ULL;
    hash ^= g_sdl_character_sheet_screen.select_desc_candidate_hash;
    hash *= 1099511628211ULL;
    hash ^= (Uint64)
        g_sdl_character_sheet_screen.select_desc_candidate_count;
    hash *= 1099511628211ULL;

    for (int i = 0; i < row_count; i++)
    {
        const sdl_character_sheet_select_row* row =
            &g_sdl_character_sheet_screen.select_rows[i];

        hash ^= row->is_heading ? 1U : 0U;
        hash *= 1099511628211ULL;
        hash = sdl_char_sheet_book_layout_hash_bytes(hash, row->label);
    }

    return hash;
}

static int sdl_char_sheet_cached_book_body_px(float canvas_h, float content_w,
    float top_y, float region_bottom, int title_px, bool choice_page)
{
    int h = (int)(canvas_h + 0.5f);
    int w = (int)(content_w + 0.5f);
    int region_h = (int)(region_bottom - top_y + 0.5f);
    Uint64 layout_hash = sdl_char_sheet_book_layout_hash();

    if (region_h < 1)
        region_h = 1;

    if (g_sdl_character_sheet_screen.context
        == SDL_CHARACTER_SHEET_NARRATIVE)
    {
        return (g_sdl_character_sheet_screen.narrative_body_px > 0)
            ? g_sdl_character_sheet_screen.narrative_body_px : 20;
    }

    if (g_sdl_character_sheet_screen.select_book_body_px > 0
        && g_sdl_character_sheet_screen.select_book_choice_body_px > 0
        && g_sdl_character_sheet_screen.select_book_body_for_h == h
        && g_sdl_character_sheet_screen.select_book_body_for_w == w
        && g_sdl_character_sheet_screen.select_book_body_for_region_h
            == region_h
        && g_sdl_character_sheet_screen.select_book_body_for_title_px
            == title_px
        && g_sdl_character_sheet_screen.select_book_body_for_layout_hash
            == layout_hash)
    {
        return choice_page
            ? g_sdl_character_sheet_screen.select_book_choice_body_px
            : g_sdl_character_sheet_screen.select_book_body_px;
    }

    g_sdl_character_sheet_screen.select_book_body_px =
        sdl_char_sheet_book_body_px(canvas_h, content_w, top_y,
            region_bottom, title_px);
    g_sdl_character_sheet_screen.select_book_choice_body_px =
        sdl_char_sheet_book_choice_body_px(canvas_h, content_w, top_y,
            region_bottom, title_px);
    g_sdl_character_sheet_screen.select_book_body_for_h = h;
    g_sdl_character_sheet_screen.select_book_body_for_w = w;
    g_sdl_character_sheet_screen.select_book_body_for_region_h = region_h;
    g_sdl_character_sheet_screen.select_book_body_for_title_px = title_px;
    g_sdl_character_sheet_screen.select_book_body_for_layout_hash =
        layout_hash;
    return choice_page
        ? g_sdl_character_sheet_screen.select_book_choice_body_px
        : g_sdl_character_sheet_screen.select_book_body_px;
}

/* A book-page frame: a parchment border with an inner hairline, drawn into the
 * current target.  Always visible, and (being part of the page) it curls with
 * the leaf during a turn. */
void sdl_char_sheet_draw_page_frame(float px, float py, float pw,
    float ph, float bt)
{
    SDL_FRect r;
    float inset;
    float hb;

    if (pw <= 2.0f * bt || ph <= 2.0f * bt)
        return;

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    if (sdl_char_sheet_portrait_chronicle())
        SDL_SetRenderDrawColor(g_state.renderer, 238, 238, 238, 225);
    else
        SDL_SetRenderDrawColor(g_state.renderer, 206, 196, 170, 210);
    r = (SDL_FRect){ px, py, pw, bt };
    SDL_RenderFillRect(g_state.renderer, &r);
    r = (SDL_FRect){ px, py + ph - bt, pw, bt };
    SDL_RenderFillRect(g_state.renderer, &r);
    r = (SDL_FRect){ px, py, bt, ph };
    SDL_RenderFillRect(g_state.renderer, &r);
    r = (SDL_FRect){ px + pw - bt, py, bt, ph };
    SDL_RenderFillRect(g_state.renderer, &r);

    inset = bt * 2.5f;
    hb = (bt * 0.5f < 1.0f) ? 1.0f : bt * 0.5f;
    if (pw <= 2.0f * inset || ph <= 2.0f * inset)
        return;
    if (sdl_char_sheet_portrait_chronicle())
        SDL_SetRenderDrawColor(g_state.renderer, 190, 190, 190, 155);
    else
        SDL_SetRenderDrawColor(g_state.renderer, 150, 140, 120, 150);
    r = (SDL_FRect){ px + inset, py + inset, pw - 2.0f * inset, hb };
    SDL_RenderFillRect(g_state.renderer, &r);
    r = (SDL_FRect){ px + inset, py + ph - inset - hb, pw - 2.0f * inset, hb };
    SDL_RenderFillRect(g_state.renderer, &r);
    r = (SDL_FRect){ px + inset, py + inset, hb, ph - 2.0f * inset };
    SDL_RenderFillRect(g_state.renderer, &r);
    r = (SDL_FRect){ px + pw - inset - hb, py + inset, hb, ph - 2.0f * inset };
    SDL_RenderFillRect(g_state.renderer, &r);
}

/*
 * Split the narrative book's paragraphs into pages that fit the page region,
 * packing whole paragraphs (never mid-paragraph) and breaking when the next
 * would overflow.  Cached against the canvas height so it only recomputes on a
 * resize.  The page count is mirrored into select_page_count for the shared
 * page-turn navigation.
 */
void sdl_char_sheet_paginate_narrative(float canvas_h, float content_w,
    float top_y, float region_bottom, int title_px)
{
    int ih = (int)(canvas_h + 0.5f);
    int iw = (int)(content_w + 0.5f);
    int itop = (int)(top_y + 0.5f);
    int ibottom = (int)(region_bottom + 0.5f);
    int body_px;

    if (g_sdl_character_sheet_screen.narrative_paginated_for_h == ih
        && g_sdl_character_sheet_screen.narrative_paginated_for_w == iw
        && g_sdl_character_sheet_screen.narrative_paginated_for_top == itop
        && g_sdl_character_sheet_screen.narrative_paginated_for_bottom
            == ibottom
        && g_sdl_character_sheet_screen.narrative_paginated_for_title_px
            == title_px
        && g_sdl_character_sheet_screen.narrative_paginated_font_generation
            == g_story_font_generation
        && g_sdl_character_sheet_screen.narrative_paginated_layout_generation
            == g_sdl_character_sheet_screen.narrative_layout_generation)
        return;

    body_px = sdl_char_sheet_narrative_choose_px(canvas_h, content_w, top_y,
        region_bottom);
    g_sdl_character_sheet_screen.narrative_body_px = body_px;
    g_sdl_character_sheet_screen.narrative_contents_body_px = 0;
    g_sdl_character_sheet_screen.narrative_page_count =
        sdl_char_sheet_narrative_pack(body_px, content_w, top_y, region_bottom,
            g_sdl_character_sheet_screen.narrative_page_start);
    sdl_char_sheet_narrative_expand_pages_to_target(
        sdl_char_sheet_narrative_target_page_count(),
        g_sdl_character_sheet_screen.narrative_page_start,
        &g_sdl_character_sheet_screen.narrative_page_count);
    sdl_char_sheet_cache_narrative_line_counts(body_px, content_w, top_y,
        region_bottom);

    g_sdl_character_sheet_screen.select_page_count =
        g_sdl_character_sheet_screen.narrative_page_count;
    if (g_sdl_character_sheet_screen.select_page
        >= g_sdl_character_sheet_screen.select_page_count)
        g_sdl_character_sheet_screen.select_page =
            g_sdl_character_sheet_screen.select_page_count - 1;
    if (g_sdl_character_sheet_screen.select_page < 0)
        g_sdl_character_sheet_screen.select_page = 0;
    g_sdl_character_sheet_screen.narrative_paginated_for_h = ih;
    g_sdl_character_sheet_screen.narrative_paginated_for_w = iw;
    g_sdl_character_sheet_screen.narrative_paginated_for_top = itop;
    g_sdl_character_sheet_screen.narrative_paginated_for_bottom = ibottom;
    g_sdl_character_sheet_screen.narrative_paginated_for_title_px = title_px;
    g_sdl_character_sheet_screen.narrative_paginated_font_generation =
        g_story_font_generation;
    g_sdl_character_sheet_screen.narrative_paginated_layout_generation =
        g_sdl_character_sheet_screen.narrative_layout_generation;
}

/* Resolve a logical contents section to the physical page where its forced
 * paragraph break landed.  A section can occupy more than one physical page
 * when its contents are long, so the logical chapter number is not necessarily
 * the page number. */
static int sdl_char_sheet_narrative_section_page(int section)
{
    int para = 0;
    int breaks = 0;
    int page;

    if (section <= 0)
        return 0;

    for (int i = 1;
         i < g_sdl_character_sheet_screen.narrative_para_count; i++) {
        if (!g_sdl_character_sheet_screen.narrative_para_break[i])
            continue;
        breaks++;
        if (breaks == section) {
            para = i;
            break;
        }
    }

    if (breaks < section)
        return MIN(section,
            MAX(0, g_sdl_character_sheet_screen.narrative_page_count - 1));

    for (page = 0;
         page < g_sdl_character_sheet_screen.narrative_page_count; page++) {
        if (g_sdl_character_sheet_screen.narrative_page_start[page] >= para)
            return page;
    }

    return MAX(0, g_sdl_character_sheet_screen.narrative_page_count - 1);
}

int sdl_character_sheet_screen_book_contents_page(int contents_index)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_NARRATIVE
        || contents_index < 0
        || contents_index >= g_sdl_character_sheet_screen
            .narrative_contents_count)
        return -1;

    return sdl_char_sheet_narrative_section_page(
        g_sdl_character_sheet_screen.narrative_contents_page[contents_index]);
}

/*
 * Draw one page of the narrative book: the paragraphs assigned to this page,
 * as wrapped body text on the parchment.  Text is laid out from the TOP of the
 * page down (like a real book) rather than vertically centred, so a short final
 * page reads from the top instead of floating in the middle.  Paragraphs that
 * were flagged as highlighted -- a quest's actual task and reward -- are drawn
 * in light blue so the player can pick them out without reading the whole page.
 */
void sdl_char_sheet_render_narrative_page(int page, TTF_Font* body_font,
    float book_x, float book_w, float top_y, float region_bottom, float body_lh,
    bool register_hits)
{
    float para_gap = body_lh * 0.6f;
    int para_count = g_sdl_character_sheet_screen.narrative_para_count;
    int page_count = g_sdl_character_sheet_screen.narrative_page_count;
    bool scrollable = sdl_char_sheet_landscape_chronicle();
    float viewport_bottom = region_bottom;
    SDL_FRect viewport;
    float content_h = 0.0f;
    int first;
    int last;
    int scroll = 0;
    int max_scroll = 0;
    float y;
    int i;

    if (page_count <= 0)
        return;
    if (page < 0)
        page = 0;
    if (page >= page_count)
        page = page_count - 1;

    first = g_sdl_character_sheet_screen.narrative_page_start[page];
    last = g_sdl_character_sheet_screen.narrative_page_start[page + 1];
    if (first < 0)
        first = 0;
    if (last > para_count)
        last = para_count;

    /* A bottom lamp is fixed page furniture.  Keep scrolled prose above it;
     * the preferred landscape layout puts the lamp beside the text instead. */
    if (scrollable
        && g_sdl_character_sheet_screen.narrative_lamp_enabled
        && page == g_sdl_character_sheet_screen.narrative_lamp_page
        && !g_sdl_character_sheet_screen.narrative_lamp_side)
    {
        viewport_bottom -= sdl_char_sheet_narrative_bottom_lamp_height(
            region_bottom - top_y, body_lh) + body_lh * 0.30f;
    }
    if (viewport_bottom < top_y + body_lh)
        viewport_bottom = top_y + body_lh;
    viewport = (SDL_FRect){ book_x, top_y, book_w,
        viewport_bottom - top_y };

    for (i = first; i < last; i++)
    {
        int lines = g_sdl_character_sheet_screen.narrative_para_lines[i];

        if (lines <= 0)
            lines = MAX(1, sdl_char_sheet_wrap_text(body_font,
                g_sdl_character_sheet_screen.narrative_paras[i], book_w,
                NULL, 0));
        content_h += (float)lines * body_lh;
        if (i + 1 < last)
            content_h += para_gap;
    }

    if (scrollable)
    {
        max_scroll = (int)SDL_ceilf(MAX(0.0f, content_h - viewport.h));
        scroll = g_sdl_narrative_page_scroll[page];
        scroll = sdl_char_sheet_clampi(scroll, 0, max_scroll);
        g_sdl_narrative_page_scroll[page] = scroll;
    }

    if (register_hits)
    {
        g_sdl_character_sheet_screen.sheet_scroll = scroll;
        g_sdl_character_sheet_screen.sheet_scroll_max = max_scroll;
        g_sdl_character_sheet_screen.select_scroll_rect = scrollable
            ? viewport : (SDL_FRect){ 0 };
    }

    if (scrollable)
    {
        SDL_Rect clip = {
            (int)viewport.x,
            (int)viewport.y,
            MAX(1, (int)(viewport.w + 0.5f)),
            MAX(1, (int)(viewport.h + 0.5f))
        };

        SDL_SetRenderClipRect(g_state.renderer, &clip);
    }

    /* Mimic a real book: start at the top, offset only within the clipped
     * landscape reading viewport. */
    y = top_y - (float)scroll;

    for (i = first; i < last; i++)
    {
        int choice = g_sdl_character_sheet_screen.narrative_para_choice[i];
        bool action = (choice >= 0);
        bool focused = action && sdl_char_sheet_choice_focused(choice);
        int lines = g_sdl_character_sheet_screen.narrative_para_lines[i];
        float h;
        byte attr = g_sdl_character_sheet_screen.narrative_para_attr[i];

        if (lines <= 0)
            lines = MAX(1, sdl_char_sheet_wrap_text(body_font,
                g_sdl_character_sheet_screen.narrative_paras[i], book_w,
                NULL, 0));
        h = (float)lines * body_lh;

        if (g_sdl_character_sheet_screen.narrative_para_highlight[i])
            attr = TERM_L_BLUE;

        if (focused) {
            SDL_FRect focus = { book_x - body_lh * 0.18f,
                y - body_lh * 0.08f, book_w + body_lh * 0.36f,
                h + body_lh * 0.16f };
            sdl_char_sheet_draw_focus_rect(focus, true);
            attr = sdl_char_sheet_focus_text_attr(attr, true);
        }

        sdl_char_sheet_draw_wrapped(body_font,
            g_sdl_character_sheet_screen.narrative_paras[i], attr, book_x,
            y, book_w, h + body_lh, body_lh, 0);
        if (register_hits && action) {
            SDL_FRect hit = { book_x - body_lh * 0.25f,
                y - body_lh * 0.18f, book_w + body_lh * 0.5f,
                h + body_lh * 0.36f };
            float left = MAX(hit.x, viewport.x);
            float top = MAX(hit.y, viewport.y);
            float right = MIN(hit.x + hit.w, viewport.x + viewport.w);
            float bottom = MIN(hit.y + hit.h, viewport.y + viewport.h);

            if (!scrollable || (right > left && bottom > top))
            {
                if (scrollable)
                    hit = (SDL_FRect){ left, top, right - left, bottom - top };
                sdl_char_sheet_add_hit(hit, choice, "", TERM_WHITE);
            }
        }
        y += h;
        if (i + 1 < last)
            y += para_gap;
    }

    if (scrollable)
    {
        SDL_FRect track;
        SDL_FRect thumb;
        float track_w = MAX(2.0f, body_lh * 0.055f);
        float thumb_h = viewport.h;
        float thumb_y = viewport.y;

        SDL_SetRenderClipRect(g_state.renderer, NULL);
        if (max_scroll <= 0)
            return;

        track = (SDL_FRect){ viewport.x + viewport.w + body_lh * 0.20f,
            viewport.y, track_w, viewport.h };
        thumb_h = MAX(body_lh * 0.70f,
            viewport.h * viewport.h / (viewport.h + (float)max_scroll));
        if (thumb_h > viewport.h)
            thumb_h = viewport.h;
        if (max_scroll > 0)
            thumb_y += (viewport.h - thumb_h)
                * ((float)scroll / (float)max_scroll);
        thumb = (SDL_FRect){ track.x, thumb_y, track.w, thumb_h };

        SDL_SetRenderDrawColor(g_state.renderer, 150, 140, 120, 70);
        SDL_RenderFillRect(g_state.renderer, &track);
        SDL_SetRenderDrawColor(g_state.renderer, 224, 185, 92, 205);
        SDL_RenderFillRect(g_state.renderer, &thumb);
    }
}

/*
 * Draw the race choice leaf.  Unlike story leaves, this is redrawn whenever
 * the focus moves, so keep it independent of chronicle pagination and use one
 * body size for headings, race names, and lore.
 */
static void sdl_char_sheet_render_race_choice_page(TTF_Font* body_font,
    float body_lh, float book_x, float book_w, float top_y,
    float region_bottom, bool mobile_pages, bool desktop_pages,
    bool register_hits)
{
    cptr frame_bottom = g_sdl_character_sheet_screen.select_frame_bottom;
    cptr desc = g_sdl_character_sheet_screen.select_description;
    int row_count = g_sdl_character_sheet_screen.select_row_count;
    bool two_columns = mobile_pages
        && !g_sdl_narrative_portrait_rendering;
    bool show_charge = !(mobile_pages || desktop_pages);
    int split = two_columns ? sdl_char_sheet_book_second_heading_index() : 0;
    float list_lh = body_lh * 1.35f;
    float gap2 = body_lh * 0.8f;
    float col_gap = body_lh * 1.15f;
    float col_w = (book_w - col_gap) * 0.5f;
    float list_h = (float)row_count * list_lh;
    int fb_lines = (show_charge && frame_bottom && frame_bottom[0])
        ? sdl_char_sheet_wrap_text(body_font, frame_bottom, book_w, NULL, 0)
        : 0;
    float y = top_y;

    if (two_columns)
    {
        float left_h;
        float right_h;

        if (col_w < 1.0f)
            col_w = book_w * 0.5f;
        left_h = sdl_char_sheet_book_rows_height(body_font, col_w, body_lh,
            list_lh, 0, split, true);
        right_h = sdl_char_sheet_book_rows_height(body_font, col_w, body_lh,
            list_lh, split, row_count, true);
        list_h = (left_h > right_h) ? left_h : right_h;
    }

    if (fb_lines > 0)
    {
        sdl_char_sheet_draw_wrapped(body_font, frame_bottom, TERM_L_BLUE,
            book_x, y, book_w, (float)fb_lines * body_lh + body_lh,
            body_lh, fb_lines);
        y += (float)fb_lines * body_lh + gap2;
    }

    if (two_columns)
    {
        float list_y = y;

        sdl_char_sheet_draw_book_row_range(body_font, book_x, list_y, col_w,
            region_bottom, body_lh, list_lh, 0, split, true, register_hits);
        sdl_char_sheet_draw_book_row_range(body_font,
            book_x + col_w + col_gap, list_y, col_w, region_bottom, body_lh,
            list_lh, split, row_count, true, register_hits);
        y += list_h;
    }
    else
    {
        for (int i = 0; i < row_count; i++)
        {
            const sdl_character_sheet_select_row* row =
                &g_sdl_character_sheet_screen.select_rows[i];
            float text_y = y + (list_lh - body_lh) * 0.5f;

            if (text_y + body_lh > region_bottom)
                break;
            if (row->is_heading)
            {
                (void)sdl_char_sheet_draw_text(body_font, row->label,
                    TERM_SLATE, book_x, text_y, book_w, body_lh * 0.95f,
                    false);
            }
            else
            {
                bool focused = sdl_char_sheet_choice_focused(row->choice);
                float indent = book_w * 0.05f;
                int tw = sdl_char_sheet_text_width(body_font, row->label);
                SDL_FRect focus = { book_x + indent, text_y,
                    MIN(book_w - indent, (float)tw + body_lh * 0.5f),
                    body_lh };
                SDL_FRect hit = { book_x + indent, y, book_w - indent,
                    list_lh };

                if (focused)
                    sdl_char_sheet_draw_focus_rect(focus, true);
                (void)sdl_char_sheet_draw_text(body_font, row->label,
                    sdl_char_sheet_focus_text_attr(row->attr, focused),
                    book_x + indent, text_y, book_w - indent,
                    body_lh * 0.95f, false);
                if (register_hits && row->choice >= 0)
                    sdl_char_sheet_add_hit(hit, row->choice, "", TERM_WHITE);
            }
            y += list_lh;
        }
    }

    y += gap2;
    if (desc && desc[0] && region_bottom - y >= body_lh)
    {
        float lore_h = region_bottom - y;
        int lore_lines = (int)(lore_h / body_lh);

        /*
         * Only draw complete lines.  The generic wrapped-text renderer permits
         * a partly visible final line, which can otherwise intrude into the
         * Android prompt row.
         */
        sdl_char_sheet_draw_history(body_font, desc, book_x, y, book_w,
            lore_h, body_lh, lore_lines);
    }
}

/*
 * Draw one page of the race "book" into the CURRENT render target.
 *
 *   desktop page 0 = trial intro (accent) + first chronicle slice
 *   desktop page 1 = second chronicle slice + charge (accent)
 *   desktop page 2 = selectable peoples list + lore (selection only)
 *
 *   mobile page 0 = trial intro + first chronicle slice
 *   mobile page 1 = second chronicle slice
 *   mobile page 2 = third chronicle slice
 *   mobile page 3 = final chronicle slice + charge
 *   mobile page 4 = full-width peoples list + lore (selection only)
 *
 * The story pages have a parchment frame with margins; the final mobile choice
 * page is full-width and unframed.  Coordinates are absolute in the current
 * target; pass them shifted when the target is an offscreen snapshot.  Story
 * pages share one fitted body size; the final race list+lore page has its own.
 * register_hits adds the people-row click targets on the choice page.
 */
void sdl_char_sheet_render_book_page(int page, float canvas_h,
    float content_x, float content_w, float top_y, float region_bottom,
    int title_px, bool register_hits)
{
    cptr frame_top = g_sdl_character_sheet_screen.select_frame_top;
    cptr intro = g_sdl_character_sheet_screen.select_intro;
    cptr frame_bottom = g_sdl_character_sheet_screen.select_frame_bottom;
    bool narrative =
        (g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_NARRATIVE);
    bool mobile_pages = sdl_char_sheet_race_book_mobile_pages();
    bool desktop_pages = !mobile_pages && !narrative
        && g_sdl_character_sheet_screen.context
            == SDL_CHARACTER_SHEET_BIRTH_SELECT
        && g_sdl_character_sheet_screen.select_book_mode
        && g_sdl_character_sheet_screen.select_page_count >= 3;
    bool split_intro = false;
    bool desktop_split_intro = false;
    char intro_first[2048];
    char intro_second[2048];
    char intro_third[2048];
    cptr intro_fourth = "";
    cptr intro_desktop_second = "";
    float region_h = (region_bottom > top_y) ? (region_bottom - top_y) : 1.0f;
    float book_w;
    float book_x;
    int body_px;
    TTF_Font* body_font;
    float body_lh;
    float gap2;
    float gap_in;
    int ft_lines;
    int in_lines;
    int first_intro_lines;
    int second_intro_lines;
    int third_intro_lines;
    int fourth_intro_lines;
    int fb_lines;
    int choice_page = sdl_char_sheet_book_choice_page();
    bool race_choice_page = !narrative && page >= choice_page;
    bool full_choice_page = mobile_pages && !narrative && page >= choice_page
        && !g_sdl_narrative_portrait_rendering;
    float y;

    body_px = narrative
        ? (g_sdl_character_sheet_screen.narrative_body_px > 0
               ? g_sdl_character_sheet_screen.narrative_body_px : 20)
        : sdl_char_sheet_cached_book_body_px(canvas_h, content_w, top_y,
            region_bottom, title_px, race_choice_page);

    intro_first[0] = '\0';
    intro_second[0] = '\0';
    intro_third[0] = '\0';

    body_font = sdl_story_font_for_height_slot(body_px,
        narrative ? SDL_STORY_FONT_SLOT_NARRATIVE
                  : SDL_STORY_FONT_SLOT_CHAR_SELECT);
    book_w = sdl_char_sheet_race_book_text_width(body_px, content_w,
        full_choice_page);
    book_x = content_x + (content_w - book_w) * 0.5f;
    body_lh = sdl_char_sheet_line_h(body_font, body_px, 1.28f);
    gap2 = body_lh * 0.8f;
    gap_in = body_lh * 0.35f;
    /* Page frame + margins around the text block.  The final mobile race-choice
     * page intentionally drops the book frame and uses the full content area. */
    if (!full_choice_page)
    {
        float mh = body_lh * SDL_BOOK_MARGIN_H;
        float mv = body_lh * SDL_BOOK_MARGIN_V;
        float page_x = book_x - mh;
        float page_w = book_w + 2.0f * mh;
        float bt = (body_lh * 0.07f < 2.0f) ? 2.0f : body_lh * 0.07f;

        if (page_x < content_x)
        {
            page_w -= (content_x - page_x);
            page_x = content_x;
        }
        if (page_x + page_w > content_x + content_w)
            page_w = content_x + content_w - page_x;

        sdl_char_sheet_draw_page_frame(page_x, top_y, page_w, region_h, bt);

        /* Inset the vertical text band by the top/bottom margins. */
        top_y += mv;
        region_bottom -= mv;
        region_h = (region_bottom > top_y) ? (region_bottom - top_y) : 1.0f;
    }

    if (narrative)
    {
        int contents_count = g_sdl_character_sheet_screen.narrative_contents_count;
        float narrative_text_w = book_w;
        float side_lamp_x = 0.0f;
        float side_lamp_w = 0.0f;
        float side_lamp_h = 0.0f;
        bool side_lamp = false;

        if (contents_count > 0 && g_sdl_narrative_portrait_rendering)
        {
            int toc_px = 0;
            int columns = 1;
            TTF_Font* toc_font;
            float toc_h = sdl_char_sheet_portrait_contents_height(body_px,
                book_w, &toc_px, &columns);
            float toc_lh;
            float toc_y;
            float column_gap;
            float column_w;
            SDL_FRect rule;

            toc_font = sdl_story_font_for_height_slot(toc_px,
                SDL_STORY_FONT_SLOT_NARRATIVE);
            toc_lh = sdl_char_sheet_line_h(toc_font, toc_px, 1.24f);
            column_gap = toc_lh * 0.72f;
            column_w = (book_w - column_gap * (float)(columns - 1))
                / (float)columns;
            toc_y = top_y;
            g_sdl_character_sheet_screen.narrative_contents_body_px = toc_px;

            (void)sdl_char_sheet_draw_text(toc_font, "Contents", TERM_YELLOW,
                book_x, toc_y, book_w, toc_lh, true);
            toc_y += toc_lh * 1.35f;
            for (int row = 0;
                 row < (contents_count + columns - 1) / columns; row++)
            {
                int row_lines = 1;

                for (int column = 0; column < columns; column++)
                {
                    int i = row * columns + column;
                    int lines;

                    if (i >= contents_count)
                        continue;
                    lines = sdl_char_sheet_wrap_text(toc_font,
                        g_sdl_character_sheet_screen
                            .narrative_contents_label[i],
                        column_w, NULL, 0);
                    row_lines = MAX(row_lines, MAX(1, lines));
                }

                for (int column = 0; column < columns; column++)
                {
                    int i = row * columns + column;
                    int choice;
                    int contents_page;
                    int next_contents_page;
                    int lines;
                    bool current;
                    bool focused;
                    byte attr;
                    float x;
                    float h;
                    SDL_FRect hit;

                    if (i >= contents_count)
                        continue;
                    choice = g_sdl_character_sheet_screen
                        .narrative_contents_choice[i];
                    contents_page = sdl_char_sheet_narrative_section_page(
                        g_sdl_character_sheet_screen
                            .narrative_contents_page[i]);
                    next_contents_page = (i + 1 < contents_count)
                        ? sdl_char_sheet_narrative_section_page(
                            g_sdl_character_sheet_screen
                                .narrative_contents_page[i + 1])
                        : g_sdl_character_sheet_screen.narrative_page_count;
                    lines = MAX(1, sdl_char_sheet_wrap_text(toc_font,
                        g_sdl_character_sheet_screen
                            .narrative_contents_label[i],
                        column_w, NULL, 0));
                    current = contents_page <= page
                        && page < next_contents_page;
                    focused = sdl_char_sheet_choice_focused(choice);
                    attr = current ? TERM_YELLOW : TERM_L_BLUE;
                    x = book_x
                        + (float)column * (column_w + column_gap);
                    h = (float)lines * toc_lh;
                    hit = (SDL_FRect){ x, toc_y, column_w, h };

                    if (focused)
                        sdl_char_sheet_draw_focus_rect(hit, true);
                    sdl_char_sheet_draw_wrapped(toc_font,
                        g_sdl_character_sheet_screen
                            .narrative_contents_label[i],
                        sdl_char_sheet_focus_text_attr(attr, focused), x,
                        toc_y, column_w, h + toc_lh * 0.1f, toc_lh, lines);
                    if (register_hits)
                        sdl_char_sheet_add_hit(hit, choice, "", TERM_WHITE);
                }
                toc_y += (float)row_lines * toc_lh;
                if (row + 1 < (contents_count + columns - 1) / columns)
                    toc_y += toc_lh * 0.24f;
            }

            rule = (SDL_FRect){
                book_x + book_w * 0.08f,
                top_y + toc_h - MAX(1.0f, toc_lh * 0.08f),
                book_w * 0.84f,
                MAX(1.0f, toc_lh * 0.055f),
            };
            SDL_SetRenderDrawColor(g_state.renderer, 150, 140, 120, 150);
            SDL_RenderFillRect(g_state.renderer, &rule);
            top_y += toc_h;
        }

        if (contents_count > 0 && !g_sdl_narrative_portrait_rendering) {
            float frame_left = MAX(content_x,
                book_x - body_lh * SDL_BOOK_MARGIN_H);
            float toc_x = content_x + content_w * 0.015f;
            float toc_right = frame_left - MAX(4.0f, body_lh * 0.42f);
            float toc_w = MAX(1.0f, toc_right - toc_x);
            int toc_px =
                g_sdl_character_sheet_screen.narrative_contents_body_px;
            TTF_Font* toc_font = NULL;
            float toc_lh = 1.0f;
            float toc_y = top_y;
            float needed_h;

            /* Fit both dimensions against the actual page edge.  This keeps
             * touch/focus rectangles out of the parchment on narrow mobile
             * layouts and avoids hard-coded desktop proportions. */
            if (toc_px < 9) {
                int low_px = 9;
                int high_px = MAX(12, (int)(body_px * 0.82f));
                int chosen_px = 9;

                while (low_px <= high_px) {
                    int candidate_px = low_px + (high_px - low_px) / 2;
                    bool fits = true;

                    toc_font = sdl_story_font_for_height_slot(candidate_px,
                        SDL_STORY_FONT_SLOT_NARRATIVE);
                    if (!toc_font) {
                        high_px = candidate_px - 1;
                        continue;
                    }
                    toc_lh = sdl_char_sheet_line_h(toc_font, candidate_px,
                        1.3f);
                    needed_h = toc_lh * (1.25f
                        + contents_count * 1.18f);
                    if (needed_h > region_bottom - top_y)
                        fits = false;
                    if (sdl_char_sheet_text_width(toc_font, "Contents")
                        > toc_w)
                    {
                        fits = false;
                    }
                    for (int i = 0; fits && i < contents_count; i++) {
                        if (sdl_char_sheet_text_width(toc_font,
                                g_sdl_character_sheet_screen
                                    .narrative_contents_label[i]) > toc_w)
                        {
                            fits = false;
                        }
                    }
                    if (fits) {
                        chosen_px = candidate_px;
                        low_px = candidate_px + 1;
                    } else {
                        high_px = candidate_px - 1;
                    }
                }
                toc_px = chosen_px;
                g_sdl_character_sheet_screen.narrative_contents_body_px =
                    toc_px;
            }
            toc_font = sdl_story_font_for_height_slot(toc_px,
                SDL_STORY_FONT_SLOT_NARRATIVE);
            toc_lh = sdl_char_sheet_line_h(toc_font, toc_px, 1.3f);

            (void)sdl_char_sheet_draw_text(toc_font, "Contents", TERM_YELLOW,
                toc_x, toc_y, toc_w, toc_lh, false);
            toc_y += toc_lh * 1.25f;
            for (int i = 0; i < contents_count; i++) {
                int choice = g_sdl_character_sheet_screen
                    .narrative_contents_choice[i];
                int contents_page =
                    sdl_char_sheet_narrative_section_page(
                        g_sdl_character_sheet_screen
                            .narrative_contents_page[i]);
                int next_contents_page = (i + 1 < contents_count)
                    ? sdl_char_sheet_narrative_section_page(
                        g_sdl_character_sheet_screen
                            .narrative_contents_page[i + 1])
                    : g_sdl_character_sheet_screen.narrative_page_count;
                bool current = contents_page <= page
                    && page < next_contents_page;
                bool focused = sdl_char_sheet_choice_focused(choice);
                byte attr = current ? TERM_YELLOW : TERM_L_BLUE;
                int text_w = sdl_char_sheet_text_width(toc_font,
                    g_sdl_character_sheet_screen.narrative_contents_label[i]);
                SDL_FRect hit = { toc_x, toc_y,
                    MIN(toc_w, (float)text_w + toc_lh * 0.25f), toc_lh };

                if (focused)
                    sdl_char_sheet_draw_focus_rect(hit, true);
                (void)sdl_char_sheet_draw_text(toc_font,
                    g_sdl_character_sheet_screen.narrative_contents_label[i],
                    sdl_char_sheet_focus_text_attr(attr, focused), toc_x,
                    toc_y, toc_w, toc_lh, false);
                if (register_hits)
                    sdl_char_sheet_add_hit(hit, choice, "", TERM_WHITE);
                toc_y += toc_lh * 1.18f;
            }
        }
        side_lamp = g_sdl_character_sheet_screen.narrative_lamp_enabled
            && page == g_sdl_character_sheet_screen.narrative_lamp_page
            && g_sdl_character_sheet_screen.narrative_lamp_side
            && sdl_char_sheet_narrative_side_lamp_geometry(body_px, book_w,
                region_bottom - top_y, body_lh, &narrative_text_w,
                &side_lamp_x, &side_lamp_w, &side_lamp_h);
        sdl_char_sheet_render_narrative_page(page, body_font, book_x,
            narrative_text_w, top_y, region_bottom, body_lh, register_hits);
        if (g_sdl_character_sheet_screen.narrative_lamp_enabled
            && page == g_sdl_character_sheet_screen.narrative_lamp_page)
        {
            if (side_lamp)
            {
                sdl_char_sheet_draw_story_lamp(book_x + side_lamp_x,
                    top_y + ((region_bottom - top_y) - side_lamp_h) * 0.5f,
                    side_lamp_w, side_lamp_h,
                    g_sdl_character_sheet_screen.narrative_lamp_current,
                    g_sdl_character_sheet_screen.narrative_lamp_maximum,
                    body_font, body_lh);
            }
            else
            {
                float lamp_h = sdl_char_sheet_narrative_bottom_lamp_height(
                    region_bottom - top_y, body_lh);
                float lamp_w = MIN(book_w * 0.56f, lamp_h * 0.58f);

                sdl_char_sheet_draw_story_lamp(
                    book_x + (book_w - lamp_w) * 0.5f,
                    region_bottom - lamp_h, lamp_w, lamp_h,
                    g_sdl_character_sheet_screen.narrative_lamp_current,
                    g_sdl_character_sheet_screen.narrative_lamp_maximum,
                    body_font, body_lh);
            }
        }
        return;
    }

    /*
     * Focus changes only affect the final choice page.  Return through its
     * lightweight renderer before measuring or balancing any chronicle text.
     */
    if (page >= choice_page)
    {
        sdl_char_sheet_render_race_choice_page(body_font, body_lh, book_x,
            book_w, top_y, region_bottom, mobile_pages, desktop_pages,
            register_hits);
        return;
    }

    ft_lines = (frame_top && frame_top[0])
        ? sdl_char_sheet_wrap_text(body_font, frame_top, book_w, NULL, 0) : 0;
    in_lines = (intro && intro[0])
        ? sdl_char_sheet_wrap_text(body_font, intro, book_w, NULL, 0) : 0;
    fb_lines = (frame_bottom && frame_bottom[0])
        ? sdl_char_sheet_wrap_text(body_font, frame_bottom, book_w, NULL, 0)
        : 0;

    /* Balance the chronicle across the story pages, weighing the frame above
     * page 0 and the charge on the last story page. */
    if (mobile_pages && intro && intro[0])
        split_intro = sdl_char_sheet_split_intro_four_pages_balanced(intro,
            body_font, book_w, ft_lines, fb_lines, intro_first,
            sizeof(intro_first), intro_second, sizeof(intro_second),
            intro_third, sizeof(intro_third), &intro_fourth);
    else if (desktop_pages && intro && intro[0])
        desktop_split_intro = sdl_char_sheet_split_intro_two_pages_balanced(
            intro, body_font, book_w, ft_lines, fb_lines, intro_first,
            sizeof(intro_first), &intro_desktop_second);
    first_intro_lines = split_intro
        ? sdl_char_sheet_wrap_text(body_font, intro_first, book_w, NULL, 0)
        : (desktop_split_intro
            ? sdl_char_sheet_wrap_text(body_font, intro_first, book_w, NULL, 0)
            : in_lines);
    second_intro_lines = split_intro
        ? sdl_char_sheet_wrap_text(body_font, intro_second, book_w, NULL, 0)
        : (desktop_split_intro
            ? sdl_char_sheet_wrap_text(body_font, intro_desktop_second,
                book_w, NULL, 0)
            : 0);
    third_intro_lines = split_intro
        ? sdl_char_sheet_wrap_text(body_font, intro_third, book_w, NULL, 0)
        : 0;
    fourth_intro_lines = split_intro
        ? sdl_char_sheet_wrap_text(body_font, intro_fourth, book_w, NULL, 0)
        : 0;

    if (page == 0)
    {
        cptr story_text = (split_intro || desktop_split_intro)
            ? intro_first : intro;
        int story_lines = (split_intro || desktop_split_intro)
            ? first_intro_lines : in_lines;

        /* Story page: trial intro (accent) then chronicle text (white). */
        y = top_y;

        if (ft_lines > 0)
        {
            sdl_char_sheet_draw_wrapped(body_font, frame_top, TERM_L_BLUE,
                book_x, y, book_w, (float)ft_lines * body_lh + body_lh,
                body_lh, 0);
            y += (float)ft_lines * body_lh + gap_in;
        }
        if (story_lines > 0)
            sdl_char_sheet_draw_wrapped(body_font, story_text, TERM_WHITE,
                book_x, y, book_w, (float)story_lines * body_lh + body_lh,
                body_lh, 0);
        return;
    }

    if ((mobile_pages || desktop_pages) && page < choice_page)
    {
        cptr story_text = desktop_pages
            ? (desktop_split_intro ? intro_desktop_second : "")
            : ((page == 1) ? intro_second
                : ((page == 2) ? intro_third : intro_fourth));
        int story_lines = desktop_pages
            ? (desktop_split_intro ? second_intro_lines : 0)
            : ((page == 1) ? second_intro_lines
                : ((page == 2) ? third_intro_lines : fourth_intro_lines));
        bool show_charge = (page == choice_page - 1);
        float charge_gap = (story_lines > 0 && show_charge && fb_lines > 0)
            ? gap2 : 0.0f;

        y = top_y;

        if (story_lines > 0)
        {
            sdl_char_sheet_draw_wrapped(body_font, story_text, TERM_WHITE,
                book_x, y, book_w, (float)story_lines * body_lh + body_lh,
                body_lh, 0);
            y += (float)story_lines * body_lh + charge_gap;
        }
        if (show_charge && fb_lines > 0)
            sdl_char_sheet_draw_wrapped(body_font, frame_bottom, TERM_L_BLUE,
                book_x, y, book_w, (float)fb_lines * body_lh + body_lh,
                body_lh, 0);
        return;
    }

}

/*
 * Draw a "leaf" texture turning like a real page: the top-right corner lifts
 * first and the fold sweeps diagonally down to the left spine.  cp (progress)
 * 0 = flat (the texture fully covers region); 1 = the page has swung up and away.
 *
 * A deformable grid mesh gives the corner-led peel: each row rotates about the
 * left spine, but upper rows lead lower rows (so the page twists, top first),
 * and the right/top region lifts toward the viewer.  The leaf is shaded by how
 * much it faces the viewer, fades out as it tips past edge-on, and is outlined
 * with a thin light border so the black page reads against the black screen.
 */
void sdl_char_sheet_draw_curled_leaf(SDL_Texture* leaf, SDL_FRect region,
    float cp)
{
    enum { NX = 20, NY = 10 };
    const float PI = 3.14159265f;
    const float THETA_MAX = PI * 0.62f;   /* swing just past edge-on */
    const float LEAD = 0.45f;             /* how far the top leads the bottom */
    SDL_Vertex verts[(NX + 1) * (NY + 1)];
    int idx[NX * NY * 6];
    float W = region.w;
    float H = region.h;
    float x0 = region.x;
    float y0 = region.y;
    float d;
    float leaf_alpha;
    int vcount = 0;
    int icount = 0;

    if (!leaf || W <= 1.0f || H <= 1.0f)
        return;
    if (cp <= 0.002f)
    {
        SDL_RenderTexture(g_state.renderer, leaf, NULL, &region);
        return;
    }
    if (cp > 1.0f)
        cp = 1.0f;

    d = W * 2.2f;                          /* perspective focal distance */

    /* Fade out over the final stretch as it tips onto its (blank) back. */
    leaf_alpha = (cp > 0.80f) ? (1.0f - (cp - 0.80f) / 0.20f) : 1.0f;
    if (leaf_alpha < 0.0f)
        leaf_alpha = 0.0f;

    for (int j = 0; j <= NY; j++)
    {
        float v = (float)j / (float)NY;
        /* Upper rows (small v) turn ahead of lower rows -> top-right leads. */
        float pr = (cp - v * LEAD) / (1.0f - LEAD);
        float theta;
        float ct;
        float st;
        float facing;

        if (pr < 0.0f) pr = 0.0f;
        if (pr > 1.0f) pr = 1.0f;
        theta = pr * THETA_MAX;
        ct = SDL_cosf(theta);
        st = SDL_sinf(theta);
        facing = (ct + 1.0f) * 0.5f;

        for (int i = 0; i <= NX; i++)
        {
            float u = (float)i / (float)NX;
            float localx = u * W;
            float zr = -localx * st;       /* toward the viewer */
            float persp = d / (d + zr);
            float sx = x0 + localx * ct * persp;
            /* Lift the turning region off the surface; strongest at the
             * top-right corner so the peel clearly starts there. */
            float lift = H * 0.22f * (localx / W) * st * (1.0f - v * 0.55f);
            float sy = y0 + v * H - lift;
            float glint = 0.22f
                * SDL_expf(-(u - (1.0f - cp)) * (u - (1.0f - cp)) / 0.012f);
            float bright = 0.38f + 0.62f * facing + glint;
            SDL_FColor col;

            if (bright > 1.0f)
                bright = 1.0f;
            col.r = bright;
            col.g = bright;
            col.b = bright;
            col.a = leaf_alpha;

            verts[vcount].position.x = sx;
            verts[vcount].position.y = sy;
            verts[vcount].color = col;
            verts[vcount].tex_coord.x = u;
            verts[vcount].tex_coord.y = v;
            vcount++;
        }
    }

    for (int j = 0; j < NY; j++)
    {
        for (int i = 0; i < NX; i++)
        {
            int a = j * (NX + 1) + i;
            int b = a + 1;
            int c = a + (NX + 1);
            int e = c + 1;

            idx[icount++] = a;
            idx[icount++] = c;
            idx[icount++] = b;
            idx[icount++] = b;
            idx[icount++] = c;
            idx[icount++] = e;
        }
    }

    SDL_SetTextureBlendMode(leaf, SDL_BLENDMODE_BLEND);
    if (!SDL_RenderGeometry(g_state.renderer, leaf, verts, vcount, idx,
            icount))
    {
        static bool logged_geometry_failure = false;

        if (!logged_geometry_failure)
        {
            log_warn("page turn geometry draw failed: %s", SDL_GetError());
            logged_geometry_failure = true;
        }
    }
}

/* Animate one page curl between any two leaves.  Contents shortcuts use this
 * to reveal their destination directly instead of playing every intervening
 * turn.  Snapshots are captured lazily on the next render frame. */
void sdl_character_sheet_screen_begin_page_turn_to(int page)
{
    int from;
    int dest;

    if (!g_sdl_character_sheet_screen.select_book_mode)
        return;
    if (g_sdl_character_sheet_screen.page_turn_active)
        return;
    from = g_sdl_character_sheet_screen.select_page;
    dest = page;
    if (dest < 0 || dest >= g_sdl_character_sheet_screen.select_page_count)
        return;
    if (dest == from)
        return;

    sdl_select_page_turn_free();
    g_sdl_character_sheet_screen.page_turn_from_page = from;
    g_sdl_character_sheet_screen.select_page = dest;
    g_sdl_character_sheet_screen.page_turn_dir = (dest > from) ? 1 : -1;
    g_sdl_character_sheet_screen.page_turn_start_ns = SDL_GetTicksNS();
    g_sdl_character_sheet_screen.page_turn_active = true;
    g_state.need_present = true;
}

/* While a page-curl is playing, keep waking the loop to draw frames. */
int sdl_select_page_turn_timeout_ms(Uint64 now_ns)
{
    (void)now_ns;
    if (!g_sdl_character_sheet_screen.page_turn_active)
        return -1;
    return 16;
}

bool sdl_character_sheet_screen_birth_sequence_active(void)
{
    return g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_BIRTH_PREVIEW
        || g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_BIRTH_SELECT
        || g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_BIRTH_STATS
        || g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_BIRTH_SKILLS;
}

static bool sdl_character_sheet_touch_allocation_choice(int choice)
{
    if (g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_BIRTH_STATS)
        return choice >= 0 && choice < A_MAX;
    if (g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_BIRTH_SKILLS)
        return choice >= 0 && choice < S_MAX && choice != S_SPC;
    return false;
}

static void sdl_character_sheet_touch_press_cancel(void)
{
    SDL_zero(g_sdl_character_sheet_screen.touch_press);
}

static void sdl_character_sheet_touch_press_begin(float x, float y,
    SDL_FingerID finger_id)
{
    const sdl_character_sheet_hit* hit = sdl_char_sheet_hit_at(x, y);
    character_sheet_touch_press_state* press =
        &g_sdl_character_sheet_screen.touch_press;

    sdl_character_sheet_touch_press_cancel();
    if (!hit)
        return;
    if (!sdl_character_sheet_touch_allocation_choice(hit->choice)
        && !(g_sdl_character_sheet_screen.context
                == SDL_CHARACTER_SHEET_NARRATIVE
            && hit->choice >= 0
            && sdl_char_sheet_choice_pressable(hit->choice)))
        return;

    press->active = true;
    press->finger_id = finger_id;
    press->choice = hit->choice;
    press->start_x = x;
    press->start_y = y;
    press->start_time = SDL_GetTicksNS();
}

static bool sdl_character_sheet_touch_press_moved(float x, float y)
{
    const character_sheet_touch_press_state* press =
        &g_sdl_character_sheet_screen.touch_press;
    float threshold = sdl_touch_swipe_threshold_px() * 0.35f;
    float dx;
    float dy;

    if (!press->active)
        return false;
    if (threshold < 18.0f)
        threshold = 18.0f;
    dx = x - press->start_x;
    dy = y - press->start_y;
    if (dx < 0.0f)
        dx = -dx;
    if (dy < 0.0f)
        dy = -dy;
    return dx > threshold || dy > threshold;
}

static void sdl_character_sheet_touch_press_motion(float x, float y,
    SDL_FingerID finger_id)
{
    character_sheet_touch_press_state* press =
        &g_sdl_character_sheet_screen.touch_press;

    if (!press->active || press->finger_id != finger_id)
        return;
    if (sdl_character_sheet_touch_press_moved(x, y))
        sdl_character_sheet_touch_press_cancel();
}

static int sdl_character_sheet_touch_press_release_action(float x, float y,
    SDL_FingerID finger_id)
{
    const sdl_character_sheet_hit* hit;
    const character_sheet_touch_press_state* press =
        &g_sdl_character_sheet_screen.touch_press;
    Uint64 held_ns;

    if (!press->active || press->finger_id != finger_id)
        return UI_MENU_CLICK_PRIMARY;
    if (sdl_character_sheet_touch_press_moved(x, y))
        return UI_MENU_CLICK_PRIMARY;

    hit = sdl_char_sheet_hit_at(x, y);
    if (!hit || hit->choice != press->choice)
        return UI_MENU_CLICK_PRIMARY;

    held_ns = SDL_GetTicksNS() - press->start_time;
    if (held_ns >= 500000000ULL)
        return UI_MENU_CLICK_SECONDARY;

    return UI_MENU_CLICK_PRIMARY;
}

void sdl_character_sheet_birth_swipe_cancel(void)
{
    touch_swipe_state* swipe = &g_sdl_character_sheet_screen.birth_swipe;

    swipe->active = false;
    swipe->triggered = false;
    swipe->finger_id = 0;
    swipe->start_x = 0.0f;
    swipe->start_y = 0.0f;
    swipe->last_x = 0.0f;
    swipe->last_y = 0.0f;
}

void sdl_character_sheet_birth_swipe_begin(float x, float y,
    SDL_FingerID finger_id)
{
    touch_swipe_state* swipe = &g_sdl_character_sheet_screen.birth_swipe;

    sdl_character_sheet_birth_swipe_cancel();
    swipe->active = true;
    swipe->triggered = false;
    swipe->finger_id = finger_id;
    swipe->start_x = x;
    swipe->start_y = y;
    swipe->last_x = x;
    swipe->last_y = y;
}

int sdl_character_sheet_birth_swipe_key_for_dir(int dir)
{
#if SIL_SDL_MOBILE_BUILD
    /*
     * Mobile hero carousel: horizontal swipes step between heroes (Left/Right
     * are remapped to prev/next in the shared birth choice loop); vertical
     * swipes are ignored since there is no list to scroll.  Swipe left reveals
     * the next hero, swipe right the previous -- the usual carousel feel.
     */
    if (sdl_character_sheet_mobile_character_select_active())
    {
        if (dir == 4)
            return '6';
        if (dir == 6)
            return '4';
        return 0;
    }
#endif

    if (dir == 8 || dir == 2)
        return '0' + dir;

    /* Narrative book: swipe left turns the page forward, swipe right back; a
     * swipe past the first/last page does nothing (it must not close the book). */
    if (g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_NARRATIVE)
    {
        int page = g_sdl_character_sheet_screen.select_page;
        int count = g_sdl_character_sheet_screen.select_page_count;

        if (dir == 4)
            return (page < count - 1) ? '6' : 0;
        if (dir == 6)
            return (page > 0) ? '4' : 0;
        return 0;
    }

    if (dir == 4)
    {
        if (g_sdl_character_sheet_screen.context
                == SDL_CHARACTER_SHEET_BIRTH_SELECT
            && g_sdl_character_sheet_screen.select_book_mode
            && g_sdl_character_sheet_screen.select_page <= 0
            && g_sdl_character_sheet_screen.select_page_count > 1)
        {
            return '6';
        }

        return '\r';
    }

    if (dir == 6)
    {
        if (g_sdl_character_sheet_screen.context
                == SDL_CHARACTER_SHEET_BIRTH_SELECT
            && g_sdl_character_sheet_screen.select_book_mode
            && g_sdl_character_sheet_screen.select_page > 0)
        {
            return '4';
        }

        return ESCAPE;
    }

    return 0;
}

bool sdl_character_sheet_birth_swipe_motion(float x, float y,
    SDL_FingerID finger_id)
{
    touch_swipe_state* swipe = &g_sdl_character_sheet_screen.birth_swipe;
    float dx;
    float dy;
    int dir;
    int key;

    if (!swipe->active || swipe->finger_id != finger_id)
        return false;

    swipe->last_x = x;
    swipe->last_y = y;
    if (swipe->triggered)
        return true;

    dx = x - swipe->start_x;
    dy = y - swipe->start_y;
    dir = sdl_touch_swipe_direction_for_delta(dx, dy,
        sdl_touch_swipe_threshold_px());
    if (!dir)
        return true;

    swipe->triggered = true;
    if (g_sdl_character_sheet_screen.page_turn_active)
        return true;

    key = sdl_character_sheet_birth_swipe_key_for_dir(dir);
    if (key)
    {
        ui_menu_click_clear_pending_hover();
        Term_keypress(key);
    }

    return true;
}

static void sdl_character_sheet_screen_render_canvas(
    const SDL_Rect* canvas_override)
{
    SDL_Rect canvas;
    float margin_x;
    float margin_top;
    float margin_bottom;
    float content_x;
    float content_w;
    float title_y;
    float title_h;
    float prompt_y;
    float prompt_h;
    float prompt_line_scale;
    float gap;
    float top_y;
    float top_h;
    float region_bottom;
    int title_px;
    int prompt_px;
    TTF_Font* title_font;
    TTF_Font* prompt_font;
    sdl_char_sheet_line vital_lines[SDL_CHAR_SHEET_MAX_LINES];
    sdl_char_sheet_line trait_lines[SDL_CHAR_SHEET_MAX_LINES];
    int vital_count;
    int trait_count;
    char title[128];
    cptr title_suffix;
#if SIL_SDL_MOBILE_BUILD
    char mobile_title_counter[32];
#endif
    cptr history;
    cptr prompt;
    bool menu_select =
        (g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_BIRTH_SELECT
            && g_sdl_character_sheet_screen.select_menu_style);
    int ui_font_slot = menu_select ? SDL_STORY_FONT_SLOT_MENU
                                   : SDL_STORY_FONT_SLOT_DEFAULT;

    if (!sdl_character_sheet_screen_active() || !g_state.renderer)
        return;

    sdl_refresh_safe_area();
    canvas = canvas_override ? *canvas_override
                             : sdl_get_layout_screen_rect();
    if (!sdl_rect_has_area(&canvas))
        return;

    sdl_char_sheet_clear_hits();
    g_sdl_character_sheet_screen.focus_choice =
        (g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_LIVE)
            ? g_sdl_character_sheet_screen.focus_choice
            : g_sdl_character_sheet_screen.selected_index;

    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    margin_x = sdl_char_sheet_clampf((float)canvas.w * 0.026f, 12.0f,
        58.0f);
    margin_top = sdl_char_sheet_clampf((float)canvas.h * 0.014f, 8.0f,
        22.0f);
    margin_bottom = sdl_char_sheet_clampf((float)canvas.h * 0.012f, 7.0f,
        20.0f);
    gap = sdl_char_sheet_clampf((float)canvas.h * 0.018f, 8.0f, 24.0f);
    content_x = (float)canvas.x + margin_x;
    content_w = (float)canvas.w - margin_x * 2.0f;

    title_px = sdl_char_sheet_clampi((int)((float)canvas.h * 0.046f), 24,
        64);
#if SIL_SDL_MOBILE_BUILD
    if (sdl_touch_only_device_active())
    {
        prompt_px = sdl_char_sheet_clampi(
            (int)((float)canvas.h * (menu_select ? 0.062f : 0.052f)),
            menu_select ? 34 : 30, menu_select ? 66 : 58);
        prompt_line_scale = 1.26f;
    }
    else
    {
        prompt_px = sdl_char_sheet_clampi(
            (int)((float)canvas.h * (menu_select ? 0.033f : 0.025f)),
            menu_select ? 18 : 13, menu_select ? 38 : 30);
        prompt_line_scale = 1.02f;
    }
#else
    prompt_px = sdl_char_sheet_clampi(
        (int)((float)canvas.h * (menu_select ? 0.033f : 0.025f)),
        menu_select ? 18 : 13, menu_select ? 38 : 30);
    prompt_line_scale = 1.02f;
#endif
    title_font = sdl_story_font_for_height_slot(title_px, ui_font_slot);
    prompt_font = sdl_story_font_for_height_slot(prompt_px, ui_font_slot);
    title_h = sdl_char_sheet_line_h(title_font, title_px, 1.02f);
    prompt_h = sdl_char_sheet_line_h(prompt_font, prompt_px,
        prompt_line_scale);
    title_y = (float)canvas.y + margin_top;
    prompt_y = (float)(canvas.y + canvas.h) - margin_bottom - prompt_h;

    /*
     * The description is laid out together with the columns by
     * sdl_char_sheet_render_columns (it fills the height left beneath them);
     * the birth/assign screens pass an empty description to hide it entirely.
     */
    history = (p_ptr && p_ptr->history[0]) ? p_ptr->history : "";
    top_y = title_y + title_h + gap;
    region_bottom = prompt_y - gap;
    top_h = (region_bottom > top_y) ? (region_bottom - top_y) : 1.0f;

    if (g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_BIRTH_SELECT)
    {
        cptr select_title = g_sdl_character_sheet_screen.select_title;

        if (g_sdl_character_sheet_screen.select_focus_title[0])
            select_title = g_sdl_character_sheet_screen.select_focus_title;
        SDL_strlcpy(title, select_title, sizeof(title));
    }
    else if (g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_NARRATIVE)
        SDL_strlcpy(title, g_sdl_character_sheet_screen.narrative_title,
            sizeof(title));
    else
    {
        sdl_char_sheet_title(title, sizeof(title));
        if (g_sdl_character_sheet_screen.context
            == SDL_CHARACTER_SHEET_BIRTH_STATS)
            SDL_strlcat(title, " - Assign Attributes", sizeof(title));
        else if (g_sdl_character_sheet_screen.context
            == SDL_CHARACTER_SHEET_BIRTH_SKILLS)
            SDL_strlcat(title, " - Assign Skills", sizeof(title));
    }
    title_suffix = (g_sdl_character_sheet_screen.context
        == SDL_CHARACTER_SHEET_BIRTH_SELECT)
            ? g_sdl_character_sheet_screen.select_title_suffix : "";
#if SIL_SDL_MOBILE_BUILD
    mobile_title_counter[0] = '\0';
    if (sdl_character_sheet_mobile_character_select_active())
    {
        int current = 0;
        int total = 0;

        sdl_character_sheet_select_counter(&current, &total);
        if (total > 0)
            strnfmt(mobile_title_counter, sizeof(mobile_title_counter),
                "%d/%d", current, total);
    }
#endif

    {
        float title_x = content_x;
        float title_w = content_w;
        float title_text_w = title_w;
        TTF_Font* title_suffix_font = title_font;
#if SIL_SDL_MOBILE_BUILD
        float arrow_w = 0.0f;
        bool carousel = sdl_character_sheet_mobile_character_select_active();
        TTF_Font* counter_font = NULL;
        float counter_gap = 0.0f;
        float counter_w = 0.0f;
        float counter_h = 0.0f;

        if (carousel)
        {
            /* Small tappable gutter on each side of the name for the prev/next
             * triangles; keep it narrow so the name stays its normal size. */
            arrow_w = sdl_char_sheet_clampf(title_h * 1.5f, 34.0f, 130.0f);
            if (arrow_w > content_w * 0.12f)
                arrow_w = content_w * 0.12f;
            title_x = content_x + arrow_w;
            title_w = content_w - 2.0f * arrow_w;
            title_text_w = title_w;
            if (mobile_title_counter[0])
            {
                counter_font = sdl_story_font_for_height_slot(title_px,
                    SDL_STORY_FONT_SLOT_MENU);
                if (counter_font)
                {
                    counter_gap = sdl_char_sheet_clampf(title_h * 0.18f,
                        6.0f, 18.0f);
                    counter_w = (float)sdl_char_sheet_text_width(counter_font,
                        mobile_title_counter);
                    counter_h = sdl_char_sheet_line_h(counter_font, title_px,
                        1.02f);
                    if (counter_w + counter_gap > title_w * 0.38f)
                        counter_w = MAX(1.0f, title_w * 0.38f
                            - counter_gap);
                    title_text_w = title_w - counter_w - counter_gap;
                    if (title_text_w < title_w * 0.50f)
                        title_text_w = title_w * 0.50f;
                }
            }

            if (title_suffix && title_suffix[0])
            {
                title_suffix_font = sdl_char_sheet_character_power_font(
                    title_font, title_px, ui_font_slot, title_text_w, title_h);
            }
        }
#endif

        if (g_sdl_narrative_portrait_rendering
            && g_sdl_character_sheet_screen.context
                == SDL_CHARACTER_SHEET_NARRATIVE)
        {
            float measured = (float)sdl_char_sheet_text_width(title_font,
                title);

            if (measured > 0.0f && measured < title_w)
            {
                title_text_w = measured;
                title_x += (title_w - measured) * 0.5f;
            }
        }

        sdl_char_sheet_draw_title_text_fonts(title_font, title_suffix_font,
            title,
            p_ptr && p_ptr->oaths_broken ? TERM_RED : TERM_L_BLUE,
            title_suffix,
            g_sdl_character_sheet_screen.select_title_suffix_attr, title_x,
            title_y, title_text_w, title_h);

#if SIL_SDL_MOBILE_BUILD
        if (counter_font)
        {
            float counter_x = title_x + title_text_w + counter_gap;
            float counter_y = title_y;

            if (counter_h > 0.0f && counter_h < title_h)
                counter_y += (title_h - counter_h) * 0.5f;
            (void)sdl_char_sheet_draw_text(counter_font,
                mobile_title_counter, TERM_SLATE, counter_x, counter_y,
                counter_w, title_h, false);
        }

        if (carousel)
        {
            int current = 0;
            int total = 0;
            int hov = g_sdl_character_sheet_screen.hover_choice;
            float ah = title_h * 1.6f;
            float ay = title_y + (title_h - ah) * 0.5f;
            SDL_FRect lr = { content_x, ay, arrow_w, ah };
            SDL_FRect rr = { content_x + content_w - arrow_w, ay, arrow_w,
                ah };

            sdl_character_sheet_select_counter(&current, &total);

            /* Tapping the name confirms the focused hero, unless that hero
             * is fallen and therefore shown for reference only. */
            if (g_sdl_character_sheet_screen.selected_index >= 0
                && sdl_char_sheet_selected_choice_confirmable())
            {
                SDL_FRect namebox = { title_x, title_y, title_text_w,
                    title_h };

                sdl_char_sheet_add_hit(namebox,
                    g_sdl_character_sheet_screen.selected_index, "",
                    TERM_WHITE);
            }

            if (current > 1)
            {
                sdl_char_sheet_draw_carousel_arrow(lr, true,
                    (hov == SDL_SELECT_CLICK_CAROUSEL_PREV) ? TERM_WHITE
                                                            : TERM_L_BLUE);
                sdl_char_sheet_add_select_button_hit(lr,
                    SDL_SELECT_CLICK_CAROUSEL_PREV);
            }
            if (total > 0 && current < total)
            {
                sdl_char_sheet_draw_carousel_arrow(rr, false,
                    (hov == SDL_SELECT_CLICK_CAROUSEL_NEXT) ? TERM_WHITE
                                                            : TERM_L_BLUE);
                sdl_char_sheet_add_select_button_hit(rr,
                    SDL_SELECT_CLICK_CAROUSEL_NEXT);
            }
        }
#endif
    }

    if (menu_select)
    {
        sdl_char_sheet_render_menu_select(prompt_font, content_x, top_y,
            content_w, region_bottom, prompt_y, prompt_h, canvas.h);
        return;
    }

    /*
     * Race selection (book mode): a story/explanation page -- intro text on
     * top, a grouped *selectable* list of peoples in the middle, and the
     * highlighted entry's lore at the bottom.  No detail panel, no pop-ups;
     * everything is the proportional story font.
     */
    if (sdl_char_sheet_book_context())
    {
        int page;
        int page_count;
        bool turning = g_sdl_character_sheet_screen.page_turn_active;

        /* Narrative book paginates against the live region before we read the
         * page count.  The race book page count is fixed by platform. */
        if (g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_NARRATIVE)
            sdl_char_sheet_paginate_narrative((float)canvas.h, content_w, top_y,
                region_bottom, title_px);

        page = g_sdl_character_sheet_screen.select_page;
        page_count = g_sdl_character_sheet_screen.select_page_count;

        if (turning)
        {
            /*
             * Page-curl: snapshot both leaves into offscreen textures (once),
             * draw the destination flat underneath, then curl the other leaf
             * away on top.  Drives itself via sdl_select_page_turn_timeout_ms.
             */
            Uint64 now = SDL_GetTicksNS();
            float t = (float)((now
                - g_sdl_character_sheet_screen.page_turn_start_ns) / 1000000ULL)
                / (float)SDL_SELECT_PAGE_TURN_MS;
            int dir = g_sdl_character_sheet_screen.page_turn_dir;
            int to_page = page;
            int from_page = g_sdl_character_sheet_screen.page_turn_from_page;
            float te;
            float cp;
            SDL_Texture* bg;
            SDL_Texture* leaf;
            bool snapshots_ready;
            bool captured_snapshots = false;
            /*
             * Capture the union of both leaves.  Story and race-choice pages
             * now have independent font sizes, so either framed column may be
             * wider.  The final mobile choice page remains full-width.
             */
            bool narrative_turn =
                g_sdl_character_sheet_screen.context
                    == SDL_CHARACTER_SHEET_NARRATIVE;
            int choice_page = sdl_char_sheet_book_choice_page();
            bool from_choice = !narrative_turn && from_page >= choice_page;
            bool to_choice = !narrative_turn && to_page >= choice_page;
            int narrative_px = (g_sdl_character_sheet_screen.narrative_body_px
                    > 0)
                ? g_sdl_character_sheet_screen.narrative_body_px : 20;
            int from_body_px = narrative_turn ? narrative_px
                : sdl_char_sheet_cached_book_body_px((float)canvas.h,
                    content_w, top_y, region_bottom, title_px, from_choice);
            int to_body_px = narrative_turn ? narrative_px
                : sdl_char_sheet_cached_book_body_px((float)canvas.h,
                    content_w, top_y, region_bottom, title_px, to_choice);
            int body_slot = (g_sdl_character_sheet_screen.context
                    == SDL_CHARACTER_SHEET_NARRATIVE)
                ? SDL_STORY_FONT_SLOT_NARRATIVE
                : SDL_STORY_FONT_SLOT_CHAR_SELECT;
            float from_lh = sdl_char_sheet_line_h(
                sdl_story_font_for_height_slot(from_body_px, body_slot),
                from_body_px, 1.28f);
            float to_lh = sdl_char_sheet_line_h(
                sdl_story_font_for_height_slot(to_body_px, body_slot),
                to_body_px, 1.28f);
            float from_page_w = sdl_char_sheet_book_width(from_body_px,
                    content_w)
                + 2.0f * from_lh * SDL_BOOK_MARGIN_H;
            float to_page_w = sdl_char_sheet_book_width(to_body_px, content_w)
                + 2.0f * to_lh * SDL_BOOK_MARGIN_H;
            float page_w = MAX(from_page_w, to_page_w);
            float page_x;
            float page_h = region_bottom - top_y;
            bool full_choice_turn =
                sdl_char_sheet_race_book_mobile_pages()
                && (from_choice || to_choice);
            int pw;
            int ph;
            SDL_FRect region;

            if (full_choice_turn)
            {
                page_x = content_x;
                page_w = content_w;
            }
            else
            {
                if (page_w > content_w)
                    page_w = content_w;
                page_x = content_x + (content_w - page_w) * 0.5f;
            }
            pw = (int)(page_w + 0.5f);
            ph = (int)(page_h + 0.5f);
            region = (SDL_FRect){ page_x, top_y, page_w, page_h };

            if (pw < 1) pw = 1;
            if (ph < 1) ph = 1;
            if (t < 0.0f) t = 0.0f;
            if (t >= 1.0f) t = 1.0f;
            /* easeInOutCubic */
            te = (t < 0.5f) ? (4.0f * t * t * t)
                            : (1.0f - SDL_powf(-2.0f * t + 2.0f, 3.0f) * 0.5f);

            snapshots_ready =
                g_sdl_character_sheet_screen.page_turn_from_tex
                && g_sdl_character_sheet_screen.page_turn_to_tex
                && g_sdl_character_sheet_screen.page_turn_tex_w == pw
                && g_sdl_character_sheet_screen.page_turn_tex_h == ph;

            /* (Re)capture snapshots if missing or stale (e.g. after resize). */
            if (!snapshots_ready)
            {
                SDL_Texture* prev_target =
                    SDL_GetRenderTarget(g_state.renderer);
                bool target_restored;

                sdl_select_page_turn_free();
                g_sdl_character_sheet_screen.page_turn_from_tex =
                    SDL_CreateTexture(g_state.renderer,
                        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                        pw, ph);
                g_sdl_character_sheet_screen.page_turn_to_tex =
                    SDL_CreateTexture(g_state.renderer,
                        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                        pw, ph);
                g_sdl_character_sheet_screen.page_turn_tex_w = pw;
                g_sdl_character_sheet_screen.page_turn_tex_h = ph;
                g_sdl_character_sheet_screen.page_turn_active = true;
                snapshots_ready = false;

                if (!g_sdl_character_sheet_screen.page_turn_from_tex
                    || !g_sdl_character_sheet_screen.page_turn_to_tex)
                {
                    static bool logged_texture_failure = false;
                    SDL_Texture* from_tex =
                        g_sdl_character_sheet_screen.page_turn_from_tex;
                    SDL_Texture* to_tex =
                        g_sdl_character_sheet_screen.page_turn_to_tex;

                    if (!logged_texture_failure)
                    {
                        log_warn("page turn snapshot texture create failed: "
                            "size=%dx%d from=%p to=%p error=%s", pw, ph,
                            (void*)from_tex, (void*)to_tex, SDL_GetError());
                        logged_texture_failure = true;
                    }
                }

                if (g_sdl_character_sheet_screen.page_turn_from_tex
                    && g_sdl_character_sheet_screen.page_turn_to_tex)
                {
                    int pages[2] = { from_page, to_page };
                    SDL_Texture* texs[2] = {
                        g_sdl_character_sheet_screen.page_turn_from_tex,
                        g_sdl_character_sheet_screen.page_turn_to_tex };
                    /* Opaque black page filling the whole column texture so the
                     * turning leaf hides the page beneath until it lifts (its
                     * edges are drawn with a light border, not a fill). */
                    SDL_FRect body = { 0.0f, 0.0f, (float)pw, (float)ph };

                    snapshots_ready = true;
                    for (int s = 0; s < 2; s++)
                    {
                        SDL_SetTextureBlendMode(texs[s], SDL_BLENDMODE_BLEND);
                        if (!SDL_SetRenderTarget(g_state.renderer, texs[s]))
                        {
                            static bool logged_target_failure = false;

                            if (!logged_target_failure)
                            {
                                log_warn("page turn snapshot target failed: "
                                    "slot=%d size=%dx%d error=%s", s, pw, ph,
                                    SDL_GetError());
                                logged_target_failure = true;
                            }
                            snapshots_ready = false;
                            break;
                        }
                        SDL_SetRenderClipRect(g_state.renderer, NULL);
                        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 0);
                        SDL_RenderClear(g_state.renderer);
                        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
                        SDL_RenderFillRect(g_state.renderer, &body);
                        /* Render the page into the texture: shift content_x so
                         * the capture region's left edge maps to 0, and use
                         * [0, page_h] for the height so the vertical centring
                         * matches the on-screen layout. */
                        sdl_char_sheet_render_book_page(pages[s],
                            (float)canvas.h,
                            content_x - page_x, content_w, 0.0f, page_h,
                            title_px, false);
                    }
                }

                target_restored = SDL_SetRenderTarget(g_state.renderer,
                    prev_target);
                if (!target_restored)
                {
                    static bool logged_restore_failure = false;

                    if (!logged_restore_failure)
                    {
                        log_warn("page turn render target restore failed: %s",
                            SDL_GetError());
                        logged_restore_failure = true;
                    }
                }
                SDL_SetRenderClipRect(g_state.renderer, NULL);
                if (snapshots_ready && target_restored)
                {
                    captured_snapshots = true;
                }
                else
                {
                    static bool logged_snapshot_unavailable = false;

                    if (!logged_snapshot_unavailable)
                    {
                        log_warn("page turn snapshots unavailable: context=%d "
                            "page=%d->%d size=%dx%d touch_only=%d "
                            "controller=%d",
                            g_sdl_character_sheet_screen.context, from_page,
                            to_page, pw, ph,
                            sdl_touch_only_device_active() ? 1 : 0,
                            steamdeck_controls_active() ? 1 : 0);
                        logged_snapshot_unavailable = true;
                    }
                    /* GPU pressure or a lost target should not make every
                     * animation frame retry two large allocations and two
                     * full page renders.  Land once and let a later, separate
                     * page turn try snapshots again. */
                    if (target_restored) {
                        sdl_select_page_turn_free();
                        sdl_char_sheet_render_book_page(page,
                            (float)canvas.h, content_x, content_w, top_y,
                            region_bottom, title_px, true);
                        sdl_char_sheet_draw_book_page_controls(prompt_font,
                            content_x, content_w, prompt_y, prompt_h, page,
                            page_count);
                    } else {
                        /* The renderer may still have one snapshot bound as
                         * its target.  Do not destroy a potentially bound
                         * texture; normal screen teardown or renderer reset
                         * will release it after the target is recoverable. */
                        g_sdl_character_sheet_screen.page_turn_active = false;
                        g_state.need_present = true;
                    }
                    return;
                }
            }

            if (captured_snapshots)
            {
                g_sdl_character_sheet_screen.page_turn_start_ns =
                    SDL_GetTicksNS();
                t = 0.0f;
                te = 0.0f;
            }

            if (dir > 0) { bg = g_sdl_character_sheet_screen.page_turn_to_tex;
                           leaf = g_sdl_character_sheet_screen.page_turn_from_tex;
                           cp = te; }
            else         { bg = g_sdl_character_sheet_screen.page_turn_from_tex;
                           leaf = g_sdl_character_sheet_screen.page_turn_to_tex;
                           cp = 1.0f - te; }

            if (snapshots_ready && bg && leaf)
            {
                SDL_SetTextureBlendMode(bg, SDL_BLENDMODE_BLEND);
                SDL_RenderTexture(g_state.renderer, bg, NULL, &region);
                sdl_char_sheet_draw_curled_leaf(leaf, region, cp);
            }
            else
            {
                /* Snapshot failed: show the destination page; diagnostics above
                 * explain why the curl path was unavailable. */
                sdl_char_sheet_render_book_page(page, (float)canvas.h,
                    content_x, content_w, top_y, region_bottom, title_px,
                    false);
            }

            if (t >= 1.0f)
            {
                sdl_select_page_turn_free();   /* lands on the new page */
                sdl_char_sheet_draw_book_page_controls(prompt_font,
                    content_x, content_w, prompt_y, prompt_h, page,
                    page_count);
            }
            g_state.need_present = true;
            return;
        }

        /* Static page: draw the current leaf and its bottom turn button(s). */
        sdl_char_sheet_render_book_page(page, (float)canvas.h, content_x,
            content_w, top_y, region_bottom, title_px, true);

        /* Bottom page-turn buttons (mouse): "Previous" on the left of the
         * prompt row, "Turn the page" on the right.  Keyboard uses the
         * arrows / Space (handled in get_player_choice). */
        sdl_char_sheet_draw_book_page_controls(prompt_font, content_x,
            content_w, prompt_y, prompt_h, page, page_count);
        /* No hover tooltip in book mode (no pop-ups). */
        return;
    }

    /*
     * Character selection: a clickable list column, a detail column for the
     * focused choice, and the lore as the description.  Reuses the same fluid
     * column packer + prompt + tooltip as the live sheet.
     */
    if (g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_BIRTH_SELECT)
    {
        /* Reuse the already-declared vitals/traits scratch arrays (this branch
         * returns before they are used for the live sheet) so the select screen
         * adds no extra stack. */
        sdl_char_sheet_line* list_lines = vital_lines;
        sdl_char_sheet_line* detail_lines = trait_lines;
        sdl_panel panels[4];
        int n = 0;
        int list_count = g_sdl_character_sheet_screen.select_row_count;
        int detail_count = g_sdl_character_sheet_screen.select_detail_count;
        int stat_rows_hint =
            g_sdl_character_sheet_screen.select_stat_rows_hint;
        int ability_rows_hint =
            g_sdl_character_sheet_screen.select_ability_rows_hint;
        int ability_rows =
            g_sdl_character_sheet_screen.select_ability_rows;
        int trait_rows_hint =
            g_sdl_character_sheet_screen.select_trait_rows_hint;
        cptr desc = g_sdl_character_sheet_screen.select_description;
        cptr desc_sizing = g_sdl_character_sheet_screen.select_desc_sizing;
        cptr body_desc = desc;
        cptr body_sizing = desc_sizing;
        char subtitle[1024];
        char sizing_subtitle[1024];
        float select_top_y = top_y;
        float select_top_h = top_h;
        int select_welcome_px = 0;
        int select_welcome_lines = 0;

        SDL_zero(vital_lines);
        SDL_zero(trait_lines);
        if (list_count > SDL_CHAR_SHEET_MAX_LINES)
            list_count = SDL_CHAR_SHEET_MAX_LINES;
        if (detail_count > SDL_CHAR_SHEET_MAX_LINES)
            detail_count = SDL_CHAR_SHEET_MAX_LINES;

        for (int i = 0; i < list_count; i++)
        {
            const sdl_character_sheet_select_row* r =
                &g_sdl_character_sheet_screen.select_rows[i];

            SDL_strlcpy(list_lines[i].text, r->label,
                sizeof(list_lines[i].text));
            list_lines[i].attr = r->attr;
            list_lines[i].choice = r->choice;
            SDL_strlcpy(list_lines[i].desc, r->desc,
                sizeof(list_lines[i].desc));
        }
        for (int i = 0; i < detail_count; i++)
        {
            const sdl_character_sheet_select_detail* d =
                &g_sdl_character_sheet_screen.select_detail[i];

            SDL_strlcpy(detail_lines[i].text, d->text,
                sizeof(detail_lines[i].text));
            detail_lines[i].attr = d->attr;
            SDL_strlcpy(detail_lines[i].desc, d->desc,
                sizeof(detail_lines[i].desc));
            /* Stat/trait rows are hoverable for a tooltip (a distinct id range
             * so a click never reads as a selection); rows without a tooltip
             * stay inert. */
            detail_lines[i].choice = (d->desc[0])
                ? (SDL_CHAR_SHEET_INFO_CHOICE_BASE + i) : -1;
        }

        SDL_zero(panels);
        panels[n].kind = SDL_PANEL_KIND_LINES;
        panels[n].heading = "";
        panels[n].lines = list_lines;
        panels[n].line_count = list_count;
        panels[n].label_fraction = 0.96f;
        panels[n].weight = 3;
        panels[n].rows = list_count + 1;
#if !defined(__ANDROID__) && !defined(SIL_IOS)
        {
            TTF_Font* ref_font = sdl_story_font_for_height(40);

            /*
             * Keep long hero names visually separate from the Stats column.
             * The extra natural width becomes trailing space because list
             * labels are left-aligned within their panel.
             */
            panels[n].natural_w =
                sdl_char_sheet_panel_natural_w(&panels[n], ref_font)
                + (float)sdl_char_sheet_text_width(ref_font, "MM");
        }
#endif
        n++;

        if (detail_count > 0)
        {
            if (stat_rows_hint > 0 || ability_rows_hint > 0
                || trait_rows_hint > 0)
            {
                TTF_Font* ref_font = sdl_story_font_for_height(40);
                int stat_count = stat_rows_hint;
                int ability_count;
                int trait_count;

                if (stat_count > detail_count)
                    stat_count = detail_count;
                ability_count = ability_rows;
                if (ability_count > detail_count - stat_count)
                    ability_count = detail_count - stat_count;
                trait_count = detail_count - stat_count - ability_count;

                panels[n].kind = SDL_PANEL_KIND_SELECT_STATS;
                panels[n].heading = "Stats";
                panels[n].lines = detail_lines;
                panels[n].line_count = stat_count;
#if defined(__ANDROID__) || defined(SIL_IOS)
                if (ability_rows_hint > 0 || ability_count > 0)
                {
                    panels[n].secondary_heading = "Abilities";
                    panels[n].secondary_lines = detail_lines + stat_count;
                    panels[n].secondary_line_count = ability_count;
                }
#endif
                panels[n].label_fraction = 0.62f;
                panels[n].weight = 2;
                panels[n].rows = MAX(stat_rows_hint, stat_count) + 1
#if defined(__ANDROID__) || defined(SIL_IOS)
                    + ((ability_rows_hint > 0 || ability_count > 0)
                        ? MAX(ability_rows_hint, ability_count) + 1 : 0)
#endif
                    + ((g_sdl_character_sheet_screen.select_rating_count > 0)
                        ? 4 : 0);
                panels[n].natural_w =
                    sdl_char_sheet_sample_panel_natural_w(ref_font, "Stats",
                        "Constitution\t+99", 0.62f);
#if defined(__ANDROID__) || defined(SIL_IOS)
                panels[n].natural_w = MAX(panels[n].natural_w,
                    sdl_char_sheet_sample_panel_natural_w(ref_font,
                        "Abilities", "Point Blank Archery", 0.62f));
#endif
                n++;

#if !defined(__ANDROID__) && !defined(SIL_IOS)
                if (ability_rows_hint > 0 || ability_count > 0)
                {
                    panels[n].kind = SDL_PANEL_KIND_LINES;
                    panels[n].heading = "Abilities";
                    panels[n].lines = detail_lines + stat_count;
                    panels[n].line_count = ability_count;
                    panels[n].label_fraction = 0.62f;
                    panels[n].weight = 2;
                    panels[n].rows =
                        MAX(ability_rows_hint, ability_count) + 1;
                    panels[n].natural_w =
                        sdl_char_sheet_sample_panel_natural_w(ref_font,
                            "Abilities", "Point Blank Archery", 0.62f);
                    n++;
                }
#endif

                if (trait_rows_hint > 0 || trait_count > 0)
                {
                    panels[n].kind = SDL_PANEL_KIND_LINES;
                    panels[n].heading = "Traits";
                    panels[n].lines =
                        detail_lines + stat_count + ability_count;
                    panels[n].line_count = trait_count;
                    panels[n].label_fraction = 0.62f;
                    panels[n].weight = 2;
                    panels[n].rows = MAX(trait_rows_hint, trait_count) + 1;
                    panels[n].natural_w =
                        sdl_char_sheet_sample_panel_natural_w(ref_font,
                            "Traits", "perception grand penalty", 0.62f);
                    n++;
                }
            }
            else
            {
                /* Fallback for callers without a size hint: split the detail
                 * across two columns so it does not dominate the selection
                 * list's font size. */
                int half = (detail_count + 1) / 2;

                panels[n].kind = SDL_PANEL_KIND_LINES;
                panels[n].heading = "Details";
                panels[n].lines = detail_lines;
                panels[n].line_count = half;
                panels[n].label_fraction = 0.62f;
                panels[n].weight = 2;
                panels[n].rows = half + 1;
                n++;

                if (detail_count > half)
                {
                    panels[n].kind = SDL_PANEL_KIND_LINES;
                    panels[n].heading = "";
                    panels[n].lines = detail_lines + half;
                    panels[n].line_count = detail_count - half;
                    panels[n].label_fraction = 0.62f;
                    panels[n].weight = 2;
                    panels[n].rows = (detail_count - half) + 1;
                    n++;
                }
            }
        }

        if (sdl_char_sheet_split_first_paragraph(desc, subtitle,
                sizeof(subtitle), &body_desc))
        {
            float subtitle_y = title_y + title_h + gap * 0.35f;
            float subtitle_h = 0.0f;
            float subtitle_lh = 1.0f;
            int subtitle_px = 12;
            int subtitle_lines = 0;
            TTF_Font* subtitle_font;

            if (!sdl_char_sheet_split_first_paragraph(desc_sizing,
                    sizing_subtitle, sizeof(sizing_subtitle), &body_sizing))
            {
                SDL_strlcpy(sizing_subtitle, subtitle,
                    sizeof(sizing_subtitle));
                body_sizing = body_desc;
            }

            for (int pass = 0; pass < 4; pass++)
            {
                int want_lines = 1;
                int wcount =
                    g_sdl_character_sheet_screen.select_welcome_count;

                select_top_y = subtitle_y + subtitle_h + gap * 0.75f;
                select_top_h = (region_bottom > select_top_y)
                    ? (region_bottom - select_top_y) : 1.0f;
                if (!sdl_char_sheet_measure_columns_desc(panels, n,
                        content_w, select_top_h, canvas.h, body_sizing,
                        &subtitle_px, &subtitle_lh, NULL))
                {
                    subtitle_px = sdl_char_sheet_clampi(
                        (int)((float)canvas.h * 0.024f), 12, 32);
                    subtitle_lh = sdl_char_sheet_line_h(
                        sdl_story_font_for_height(subtitle_px), subtitle_px,
                        1.18f);
                }

                subtitle_font = sdl_story_font_for_height(subtitle_px);
                for (int k = 0; k < wcount; k++)
                {
                    int wl = sdl_char_sheet_wrap_text(subtitle_font,
                        g_sdl_character_sheet_screen.select_welcome[k],
                        content_w, NULL, 0);
                    if (wl > want_lines)
                        want_lines = wl;
                }
                if (wcount <= 0)
                {
                    want_lines = sdl_char_sheet_wrap_text(subtitle_font,
                        sizing_subtitle, content_w, NULL, 0);
                    if (want_lines < 1)
                        want_lines = 1;
                }
                subtitle_lines = want_lines;
                subtitle_h = subtitle_lh * (float)subtitle_lines;
            }

            select_welcome_px = subtitle_px;
            select_welcome_lines = subtitle_lines;
            subtitle_font = sdl_story_font_for_height(subtitle_px);
            if (subtitle_lines > 0)
            {
                /* Draw the focused welcome inside its measured band. */
                sdl_char_sheet_draw_wrapped(subtitle_font, subtitle,
                    TERM_YELLOW, content_x, subtitle_y, content_w,
                    subtitle_h + subtitle_lh * 0.25f, subtitle_lh,
                    subtitle_lines);
                select_top_y = subtitle_y + subtitle_h + gap * 0.75f;
                select_top_h = (region_bottom > select_top_y)
                    ? (region_bottom - select_top_y) : 1.0f;
            }
        }

#if SIL_SDL_MOBILE_BUILD
        if (sdl_character_sheet_mobile_character_select_active())
        {
            /* Carousel body sits below the name + welcome line and chooses a
             * measured stats/traits/lore layout for the live screen size. */
            sdl_char_sheet_render_mobile_character_select(list_lines,
                list_count, panels, n, content_x, select_top_y, content_w,
                select_top_h, canvas.h, body_desc, body_sizing, prompt_font,
                prompt_y, prompt_h);
            return;
        }
#endif

        sdl_char_sheet_render_columns(panels, n, content_x, select_top_y,
            content_w, select_top_h, canvas.h, body_desc, body_sizing, 0,
            NULL);

        {
            static int logged_focus = -1;
            static int logged_choices = -1;
            static int logged_canvas_w = -1;
            static int logged_canvas_h = -1;
            static size_t logged_hint_len = 0;
            size_t hint_len = body_sizing ? strlen(body_sizing) : 0;

            if (logged_focus != g_sdl_character_sheet_screen.focus_choice
                || logged_choices != list_count
                || logged_canvas_w != canvas.w
                || logged_canvas_h != canvas.h
                || logged_hint_len != hint_len)
            {
                log_debug("character-select fit: focus=%d choices=%d "
                    "trait_rows=%d hint_bytes=%lu welcome=%dpx/%dlines "
                    "body=%dpx canvas=%dx%d",
                    g_sdl_character_sheet_screen.focus_choice, list_count,
                    trait_rows_hint, (unsigned long)hint_len,
                    select_welcome_px, select_welcome_lines,
                    g_sdl_character_sheet_screen.last_desc_px,
                    canvas.w, canvas.h);
                logged_focus = g_sdl_character_sheet_screen.focus_choice;
                logged_choices = list_count;
                logged_canvas_w = canvas.w;
                logged_canvas_h = canvas.h;
                logged_hint_len = hint_len;
            }
        }

        sdl_char_sheet_draw_prompt(prompt_font, "", content_x, prompt_y,
            content_w, prompt_h);
        /* Tooltips for the stat/trait detail rows only (list rows carry none,
         * since the focused choice's text is already shown below). */
        sdl_char_sheet_render_hover_tooltip();
        return;
    }

    SDL_zero(vital_lines);
    SDL_zero(trait_lines);
    vital_count = sdl_char_sheet_collect_vitals(vital_lines,
        SDL_CHAR_SHEET_MAX_LINES);
    trait_count = sdl_char_sheet_collect_traits(trait_lines,
        SDL_CHAR_SHEET_MAX_LINES);

    {
        sdl_char_sheet_line stat_lines[A_MAX + 2];
        sdl_char_sheet_line skill_lines[S_MAX + 2];
        sdl_panel panels[6];
        int n = 0;
        int stat_count = 0;
        int skill_count = 0;
        bool birth = sdl_char_sheet_birth_context();
        bool wide5 = (sdl_char_sheet_target_ncols(content_w,
            (float)canvas.h) >= 5);

        SDL_zero(stat_lines);
        SDL_zero(skill_lines);
        /* The live sheet feeds labeled lines; the birth/assign screens render
         * Attributes & Skills with the original grid drawers (value/breakdown/
         * cost, clickable) instead, so the distribution looks exactly as before
         * -- only the column arrangement is the new adaptive one. */
        if (!birth)
        {
            stat_count = sdl_char_sheet_collect_stats(stat_lines, A_MAX + 2);
            skill_count = sdl_char_sheet_collect_skills(skill_lines,
                S_MAX + 2, wide5);
        }

        SDL_zero(panels);

        /*
         * Wide screens (5-column target) split the long Vitals list into
         * "Vitals" (status) + "Combat" so a fifth column exists and the tallest
         * column shrinks, letting the font grow.
         */
        if (wide5)
        {
            int v_split = vital_count;

            for (int i = 0; i < vital_count; i++)
                if (strncmp(vital_lines[i].text, "Melee\t", 6) == 0)
                {
                    v_split = i;
                    break;
                }
            if (v_split <= 0 || v_split >= vital_count)
                v_split = vital_count / 2;

            panels[n].kind = SDL_PANEL_KIND_LINES;
            panels[n].heading = "Vitals";
            panels[n].lines = vital_lines;
            panels[n].line_count = v_split;
            panels[n].label_fraction = 0.48f;
            panels[n].weight = 3;
            panels[n].rows = v_split + 1;
            n++;

            panels[n].kind = SDL_PANEL_KIND_LINES;
            panels[n].heading = "Combat";
            panels[n].lines = vital_lines + v_split;
            panels[n].line_count = vital_count - v_split;
            panels[n].label_fraction = 0.48f;
            panels[n].weight = 3;
            panels[n].rows = (vital_count - v_split) + 1;
            n++;
        }
        else
        {
            panels[n].kind = SDL_PANEL_KIND_LINES;
            panels[n].heading = "Vitals";
            panels[n].lines = vital_lines;
            panels[n].line_count = vital_count;
            panels[n].label_fraction = 0.48f;
            panels[n].weight = 3;
            panels[n].rows = vital_count + 1;
            n++;
        }

        panels[n].kind = SDL_PANEL_KIND_TRAITS;
        panels[n].heading = "Traits";
        panels[n].lines = trait_lines;
        panels[n].line_count = trait_count;
        panels[n].label_fraction = 0.58f;
        panels[n].weight = 3;
        panels[n].rows = trait_count + 1;
        n++;

        if (birth)
        {
#if SIL_SDL_MOBILE_BUILD
            panels[n].kind = SDL_PANEL_KIND_ALLOC_STATS;
            panels[n].heading = "Attributes";
            panels[n].lines = NULL;
            panels[n].line_count = 0;
            panels[n].label_fraction = 0.44f;
            panels[n].weight = 2;
            panels[n].rows = A_MAX + 2;
            n++;

            panels[n].kind = SDL_PANEL_KIND_ALLOC_SKILLS;
            panels[n].heading = "Skills";
            panels[n].lines = NULL;
            panels[n].line_count = 0;
            panels[n].label_fraction = 0.44f;
            panels[n].weight = 4;
            panels[n].rows = S_MAX + 1;
            n++;
#else
            panels[n].kind = SDL_PANEL_KIND_ALLOC;
            panels[n].heading = "Attributes / Skills";
            panels[n].lines = NULL;
            panels[n].line_count = 0;
            panels[n].label_fraction = 0.44f;
            panels[n].weight = 5;
            panels[n].rows = 8 + (S_MAX - 1);
            panels[n].alloc_stats =
                (g_sdl_character_sheet_screen.context
                    == SDL_CHARACTER_SHEET_BIRTH_STATS);
            n++;
#endif
        }
        else
        {
            panels[n].kind = SDL_PANEL_KIND_LINES;
            panels[n].heading = "Attributes";
            panels[n].lines = stat_lines;
            panels[n].line_count = stat_count;
            panels[n].label_fraction = 0.42f;
            panels[n].weight = 2;
            panels[n].rows = stat_count + 1;
            n++;

            panels[n].kind = SDL_PANEL_KIND_LINES;
            panels[n].heading = "Skills";
            panels[n].lines = skill_lines;
            panels[n].line_count = skill_count;
            panels[n].label_fraction = wide5 ? 0.72f : 0.44f;
            panels[n].weight = 4;
            panels[n].rows = skill_count + 1;
            n++;
        }

        if (birth)
        {
            /*
             * Birth/assign: use one fewer column than the live character
             * sheet. There is no description band here, so the grid has enough
             * vertical room while each column gets more width.
             */
            int fpx = sdl_char_sheet_clampi((int)((float)canvas.h * 0.030f),
                18, 40);
            TTF_Font* ffont = sdl_story_font_for_height(fpx);
            float flh = sdl_char_sheet_line_h(ffont, fpx, 1.1f);
            SDL_FRect alloc_col = { content_x, top_y, content_w, top_h };
            int ncols_bias = -1;

            sdl_char_sheet_render_columns(panels, n, content_x, top_y,
                content_w, top_h - flh - gap, canvas.h, "", NULL, ncols_bias,
                &alloc_col);
            sdl_char_sheet_draw_birth_status_row(ffont, alloc_col.x,
                top_y + top_h - flh, alloc_col.w, flh, flh, 0, "");
        }
        else
        {
            sdl_char_sheet_render_columns(panels, n, content_x, top_y,
                content_w, top_h, canvas.h, history, NULL, 0, NULL);
        }
    }

    if (g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_LIVE)
    {
        prompt = steamdeck_controls_active()
            ? "abilities increase help back notes story file"
            : "abilities increase help back notes story file";
    }
    else
    {
        prompt = steamdeck_controls_active()
            ? "back confirm character"
            : "back confirm character";
    }

    sdl_char_sheet_draw_prompt(prompt_font, prompt, content_x, prompt_y,
        content_w, prompt_h);
    sdl_char_sheet_render_hover_tooltip();
}

void sdl_character_sheet_screen_render(void)
{
    SDL_Rect canvas;
    bool portrait = false;

    if (sdl_char_sheet_book_context())
    {
        sdl_refresh_safe_area();
        canvas = sdl_get_layout_screen_rect();
        portrait = sdl_narrative_portrait_begin(&canvas);
        sdl_character_sheet_screen_render_canvas(&canvas);
    }
    else
    {
        g_sdl_narrative_portrait_rendering = false;
        if (g_state.renderer)
            (void)SDL_SetRenderTarget(g_state.renderer, NULL);
        sdl_character_sheet_screen_render_canvas(NULL);
    }

    sdl_narrative_portrait_finish(portrait);
}

bool sdl_character_sheet_screen_active(void)
{
    return g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_HIDDEN;
}

void sdl_character_sheet_screen_hide(void)
{
    if (!sdl_character_sheet_screen_active())
        return;

    g_sdl_character_sheet_screen.context = SDL_CHARACTER_SHEET_HIDDEN;
    g_sdl_character_sheet_screen.focus_choice = -1;
    g_sdl_character_sheet_screen.selected_index = -1;
    g_sdl_character_sheet_screen.hover_choice = SDL_CHAR_SHEET_NO_HOVER;
    g_sdl_character_sheet_screen.select_menu_style = false;
    g_sdl_select_choice_page_only = false;
    g_sdl_select_dynamic_description = false;
    g_sdl_select_menu_rows_per_column = 0;
    g_sdl_character_sheet_screen.live_item_count = 0;
    g_sdl_character_sheet_screen.sheet_scroll = 0;
    g_sdl_character_sheet_screen.sheet_scroll_max = 0;
    sdl_character_sheet_birth_swipe_cancel();
    sdl_character_sheet_touch_press_cancel();
    sdl_char_sheet_clear_hits();
    ui_menu_click_clear_pending_hover();
    g_state.need_present = true;
}

void sdl_character_sheet_screen_begin_live(int focus_choice)
{
    if (!g_state.window || !g_state.renderer)
        return;

    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_LIVE)
    {
        g_sdl_character_sheet_screen.sheet_scroll = 0;
        g_sdl_character_sheet_screen.sheet_scroll_max = 0;
    }
    g_sdl_character_sheet_screen.context = SDL_CHARACTER_SHEET_LIVE;
    g_sdl_character_sheet_screen.focus_choice = focus_choice;
    g_sdl_character_sheet_screen.selected_index = -1;
    g_sdl_character_sheet_screen.points_left = 0;
    g_sdl_character_sheet_screen.select_menu_style = false;
    g_sdl_character_sheet_screen.live_item_count = 0;
    sdl_character_sheet_touch_press_cancel();
    g_state.need_present = true;
}

void sdl_character_sheet_screen_begin_birth_preview(void)
{
    if (!g_state.window || !g_state.renderer)
        return;

    if (g_sdl_character_sheet_screen.context
        != SDL_CHARACTER_SHEET_BIRTH_PREVIEW)
    {
        g_sdl_character_sheet_screen.sheet_scroll = 0;
        g_sdl_character_sheet_screen.sheet_scroll_max = 0;
    }
    sdl_character_sheet_birth_swipe_cancel();
    sdl_character_sheet_touch_press_cancel();
    g_sdl_character_sheet_screen.context = SDL_CHARACTER_SHEET_BIRTH_PREVIEW;
    g_sdl_character_sheet_screen.focus_choice = -1;
    g_sdl_character_sheet_screen.selected_index = -1;
    g_sdl_character_sheet_screen.points_left = 0;
    g_sdl_character_sheet_screen.select_menu_style = false;
    g_sdl_character_sheet_screen.live_item_count = 0;
    g_state.need_present = true;
}

void sdl_character_sheet_screen_add_live_item(int choice, int kind, int skill,
    int value_kind, cptr label, cptr desc)
{
    sdl_character_sheet_live_item* item;

    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_LIVE
        && g_sdl_character_sheet_screen.context
            != SDL_CHARACTER_SHEET_BIRTH_PREVIEW)
    {
        return;
    }
    if (g_sdl_character_sheet_screen.live_item_count
        >= SDL_CHAR_SHEET_LIVE_ITEM_MAX)
    {
        return;
    }

    item = &g_sdl_character_sheet_screen
                .live_items[g_sdl_character_sheet_screen.live_item_count++];
    SDL_zero(*item);
    item->choice = choice;
    item->kind = kind;
    item->skill = skill;
    item->value_kind = value_kind;
    SDL_strlcpy(item->label, label ? label : "", sizeof(item->label));
    SDL_strlcpy(item->desc, desc ? desc : "", sizeof(item->desc));
}

void sdl_character_sheet_screen_show_birth_stats(const int* stats,
    const int* costs, int selected_stat, int points_left)
{
    if (!g_state.window || !g_state.renderer)
        return;

    if (g_sdl_character_sheet_screen.context
        != SDL_CHARACTER_SHEET_BIRTH_STATS)
    {
        g_sdl_character_sheet_screen.hover_choice =
            SDL_CHAR_SHEET_NO_HOVER;
        ui_menu_click_clear_pending_hover();
    }
    sdl_character_sheet_birth_swipe_cancel();
    sdl_character_sheet_touch_press_cancel();
    g_sdl_character_sheet_screen.context = SDL_CHARACTER_SHEET_BIRTH_STATS;
    g_sdl_character_sheet_screen.sheet_scroll = 0;
    g_sdl_character_sheet_screen.sheet_scroll_max = 0;
    g_sdl_character_sheet_screen.focus_choice = selected_stat;
    g_sdl_character_sheet_screen.selected_index = selected_stat;
    g_sdl_character_sheet_screen.points_left = points_left;
    g_sdl_character_sheet_screen.select_menu_style = false;
    g_sdl_character_sheet_screen.live_item_count = 0;
    for (int i = 0; i < A_MAX; i++)
    {
        g_sdl_character_sheet_screen.stat_values[i] = stats ? stats[i] : 0;
        g_sdl_character_sheet_screen.stat_costs[i] = costs ? costs[i] : 0;
    }
    g_state.need_present = true;
}

void sdl_character_sheet_screen_show_birth_skills(const int* old_base,
    const int* skill_gain, const int* costs, int selected_skill,
    int points_left)
{
    if (!g_state.window || !g_state.renderer)
        return;

    if (g_sdl_character_sheet_screen.context
        != SDL_CHARACTER_SHEET_BIRTH_SKILLS)
    {
        g_sdl_character_sheet_screen.hover_choice =
            SDL_CHAR_SHEET_NO_HOVER;
        ui_menu_click_clear_pending_hover();
    }
    sdl_character_sheet_birth_swipe_cancel();
    sdl_character_sheet_touch_press_cancel();
    g_sdl_character_sheet_screen.context = SDL_CHARACTER_SHEET_BIRTH_SKILLS;
    g_sdl_character_sheet_screen.sheet_scroll = 0;
    g_sdl_character_sheet_screen.sheet_scroll_max = 0;
    g_sdl_character_sheet_screen.focus_choice = selected_skill;
    g_sdl_character_sheet_screen.selected_index = selected_skill;
    g_sdl_character_sheet_screen.points_left = points_left;
    g_sdl_character_sheet_screen.select_menu_style = false;
    g_sdl_character_sheet_screen.live_item_count = 0;
    for (int i = 0; i < S_MAX; i++)
    {
        g_sdl_character_sheet_screen.skill_old_base[i] =
            old_base ? old_base[i] : 0;
        g_sdl_character_sheet_screen.skill_gain[i] =
            skill_gain ? skill_gain[i] : 0;
        g_sdl_character_sheet_screen.skill_costs[i] = costs ? costs[i] : 0;
    }
    g_state.need_present = true;
}

/*
 * Race / lineage / character selection.  Mirrors the live append-builder: the
 * birth keyboard loop (get_player_choice) rebuilds the screen every iteration --
 * begin_select, add a row per visible choice, add the focused choice's detail
 * lines, set the description (lore), then commit.
 */
/* Drop any page-curl snapshot textures and stop the animation. */
void sdl_select_page_turn_free(void)
{
    if (g_sdl_character_sheet_screen.page_turn_from_tex)
        SDL_DestroyTexture(g_sdl_character_sheet_screen.page_turn_from_tex);
    if (g_sdl_character_sheet_screen.page_turn_to_tex)
        SDL_DestroyTexture(g_sdl_character_sheet_screen.page_turn_to_tex);
    g_sdl_character_sheet_screen.page_turn_from_tex = NULL;
    g_sdl_character_sheet_screen.page_turn_to_tex = NULL;
    g_sdl_character_sheet_screen.page_turn_active = false;
}

/* Reopen the book on its first page (called when (re)entering the race screen). */
void sdl_character_sheet_screen_reset_select_page(void)
{
    sdl_select_page_turn_free();
    g_sdl_character_sheet_screen.select_page = 0;
}

void sdl_character_sheet_screen_begin_select(int focus_choice, cptr title)
{
    if (!g_state.window || !g_state.renderer)
        return;

    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
    {
        g_sdl_character_sheet_screen.sheet_scroll = 0;
        g_sdl_character_sheet_screen.sheet_scroll_max = 0;
        sdl_character_sheet_screen_reset_select_page();
    }
    g_sdl_character_sheet_screen.select_page_count = 1;
    sdl_character_sheet_birth_swipe_cancel();
    sdl_character_sheet_touch_press_cancel();
    g_sdl_character_sheet_screen.context = SDL_CHARACTER_SHEET_BIRTH_SELECT;
    g_sdl_character_sheet_screen.focus_choice = focus_choice;
    g_sdl_character_sheet_screen.selected_index = focus_choice;
    g_sdl_character_sheet_screen.points_left = 0;
    g_sdl_character_sheet_screen.live_item_count = 0;
    g_sdl_character_sheet_screen.select_row_count = 0;
    g_sdl_character_sheet_screen.select_detail_count = 0;
    g_sdl_character_sheet_screen.select_welcome_count = 0;
    g_sdl_character_sheet_screen.select_desc_candidate_count = 0;
    g_sdl_character_sheet_screen.select_desc_candidate_hash =
        14695981039346656037ULL;
    g_sdl_character_sheet_screen.select_rating_count = 0;
    g_sdl_character_sheet_screen.select_rating_title[0] = '\0';
    g_sdl_character_sheet_screen.select_stat_rows_hint = 0;
    g_sdl_character_sheet_screen.select_ability_rows_hint = 0;
    g_sdl_character_sheet_screen.select_ability_rows = 0;
    g_sdl_character_sheet_screen.select_trait_rows_hint = 0;
    g_sdl_character_sheet_screen.last_body_px = 0;
    g_sdl_character_sheet_screen.last_body_line_h = 0.0f;
    g_sdl_character_sheet_screen.last_desc_px = 0;
    g_sdl_character_sheet_screen.last_desc_line_h = 0.0f;
    g_sdl_character_sheet_screen.select_description[0] = '\0';
    g_sdl_character_sheet_screen.select_focus_title[0] = '\0';
    g_sdl_character_sheet_screen.select_title_suffix[0] = '\0';
    g_sdl_character_sheet_screen.select_title_suffix_attr = TERM_WHITE;
    g_sdl_character_sheet_screen.select_title_candidate_count = 0;
    g_sdl_character_sheet_screen.select_title_power_px = 0;
    g_sdl_character_sheet_screen.select_title_power_for_title_px = 0;
    g_sdl_character_sheet_screen.select_title_power_for_width = 0;
    g_sdl_character_sheet_screen.select_intro[0] = '\0';
    g_sdl_character_sheet_screen.select_frame_top[0] = '\0';
    g_sdl_character_sheet_screen.select_frame_bottom[0] = '\0';
    g_sdl_character_sheet_screen.select_desc_sizing[0] = '\0';
    g_sdl_character_sheet_screen.select_scroll_rect = (SDL_FRect){ 0 };
    SDL_zero(g_sdl_character_sheet_screen.select_scroll_drag);
    g_sdl_character_sheet_screen.select_book_mode = false;
    g_sdl_character_sheet_screen.select_menu_style = false;
    g_sdl_character_sheet_screen.narrative_close_enabled = false;
    SDL_strlcpy(g_sdl_character_sheet_screen.narrative_close_label, "Close",
        sizeof(g_sdl_character_sheet_screen.narrative_close_label));
    g_sdl_select_choice_page_only = false;
    g_sdl_select_dynamic_description = false;
    g_sdl_select_menu_rows_per_column = 0;
    SDL_strlcpy(g_sdl_character_sheet_screen.select_title, title ? title : "",
        sizeof(g_sdl_character_sheet_screen.select_title));
}

void sdl_character_sheet_screen_set_select_menu_style(bool enabled)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return;

    g_sdl_character_sheet_screen.select_menu_style = enabled;
    g_state.need_present = true;
}

/* ---- Narrative book (N pages with optional actions): quests, stats, etc. - *
 * Open the book, push complete paragraphs, then commit.  Navigation reuses
 * the shared select_page / page-turn accessors and sdl_character_sheet_screen_hide.
 */
static void sdl_character_sheet_narrative_layout_changed(void)
{
    g_sdl_character_sheet_screen.narrative_layout_generation++;
    if (g_sdl_character_sheet_screen.narrative_layout_generation == 0)
        g_sdl_character_sheet_screen.narrative_layout_generation = 1;
    g_sdl_character_sheet_screen.narrative_paginated_for_h = -1;
    g_sdl_character_sheet_screen.narrative_paginated_for_w = -1;
    g_sdl_character_sheet_screen.narrative_contents_body_px = 0;
}

void sdl_character_sheet_screen_begin_book(cptr title)
{
    if (!g_state.window || !g_state.renderer)
        return;

    sdl_select_page_turn_free();
    sdl_character_sheet_birth_swipe_cancel();
    g_sdl_character_sheet_screen.context = SDL_CHARACTER_SHEET_NARRATIVE;
    g_sdl_character_sheet_screen.focus_choice = -1;
    g_sdl_character_sheet_screen.selected_index = -1;
    g_sdl_character_sheet_screen.hover_choice = SDL_CHAR_SHEET_NO_HOVER;
    g_sdl_character_sheet_screen.select_menu_style = false;
    g_sdl_character_sheet_screen.live_item_count = 0;
    g_sdl_character_sheet_screen.select_row_count = 0;
    g_sdl_character_sheet_screen.select_detail_count = 0;
    g_sdl_character_sheet_screen.select_book_mode = true;
    g_sdl_character_sheet_screen.select_page = 0;
    g_sdl_character_sheet_screen.select_page_count = 1;
    g_sdl_character_sheet_screen.sheet_scroll = 0;
    g_sdl_character_sheet_screen.sheet_scroll_max = 0;
    g_sdl_character_sheet_screen.select_scroll_rect = (SDL_FRect){ 0 };
    SDL_zero(g_sdl_character_sheet_screen.select_scroll_drag);
    g_sdl_character_sheet_screen.narrative_para_count = 0;
    g_sdl_character_sheet_screen.narrative_contents_count = 0;
    g_sdl_character_sheet_screen.narrative_pending_break = false;
    g_sdl_character_sheet_screen.narrative_pending_highlight = false;
    g_sdl_character_sheet_screen.narrative_page_count = 0;
    memset(g_sdl_narrative_page_scroll, 0,
        sizeof(g_sdl_narrative_page_scroll));
    g_sdl_character_sheet_screen.narrative_target_page_count = 0;
    g_sdl_character_sheet_screen.narrative_body_px = 0;
    g_sdl_character_sheet_screen.narrative_lamp_enabled = false;
    g_sdl_character_sheet_screen.narrative_lamp_current = 0;
    g_sdl_character_sheet_screen.narrative_lamp_maximum = 0;
    g_sdl_character_sheet_screen.narrative_lamp_page = 0;
    g_sdl_character_sheet_screen.narrative_lamp_side = false;
    g_sdl_character_sheet_screen.narrative_close_enabled = false;
    SDL_strlcpy(g_sdl_character_sheet_screen.narrative_close_label, "Close",
        sizeof(g_sdl_character_sheet_screen.narrative_close_label));
    g_sdl_character_sheet_screen.narrative_paginated_for_h = -1;
    g_sdl_character_sheet_screen.narrative_paginated_for_w = -1;
    SDL_strlcpy(g_sdl_character_sheet_screen.narrative_title,
        title ? title : "",
        sizeof(g_sdl_character_sheet_screen.narrative_title));
    sdl_character_sheet_narrative_layout_changed();
}

void sdl_character_sheet_screen_add_book_paragraph(cptr text)
{
    int n;

    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_NARRATIVE)
        return;
    if (!text || !text[0])
        return;

    n = g_sdl_character_sheet_screen.narrative_para_count;
    if (n >= SDL_BOOK_MAX_PARAS)
        return;

    SDL_strlcpy(g_sdl_character_sheet_screen.narrative_paras[n], text,
        SDL_BOOK_PARA_LEN);
    g_sdl_character_sheet_screen.narrative_para_choice[n] = -1;
    g_sdl_character_sheet_screen.narrative_para_attr[n] = TERM_WHITE;
    g_sdl_character_sheet_screen.narrative_para_break[n] =
        g_sdl_character_sheet_screen.narrative_pending_break;
    g_sdl_character_sheet_screen.narrative_pending_break = false;
    g_sdl_character_sheet_screen.narrative_para_highlight[n] =
        g_sdl_character_sheet_screen.narrative_pending_highlight;
    g_sdl_character_sheet_screen.narrative_pending_highlight = false;
    g_sdl_character_sheet_screen.narrative_para_count = n + 1;
    sdl_character_sheet_narrative_layout_changed();
}

/* A soft radial bloom: a bright centre vertex fading to a transparent rim,
 * drawn as a triangle fan.  The captured light, its motes, the surface sheen
 * and the outer aura are all built from this.  The caller chooses the blend
 * mode -- additive for anything that should read as glow. */
static void sdl_story_lamp_glow(float cx, float cy, float rx, float ry,
    SDL_FColor core)
{
    enum { SEG = 20 };
    SDL_Vertex v[SEG + 2];
    int idx[SEG * 3];
    SDL_FColor rim = core;
    int i;

    if (!g_state.renderer || rx <= 0.0f || ry <= 0.0f)
        return;
    rim.a = 0.0f;

    v[0].position = (SDL_FPoint){ cx, cy };
    v[0].color = core;
    v[0].tex_coord = (SDL_FPoint){ 0.0f, 0.0f };
    for (i = 0; i <= SEG; i++) {
        float a = (float)i / (float)SEG * 6.2831853f;
        v[i + 1].position = (SDL_FPoint){ cx + SDL_cosf(a) * rx,
            cy + SDL_sinf(a) * ry };
        v[i + 1].color = rim;
        v[i + 1].tex_coord = (SDL_FPoint){ 0.0f, 0.0f };
    }
    for (i = 0; i < SEG; i++) {
        idx[i * 3] = 0;
        idx[i * 3 + 1] = i + 1;
        idx[i * 3 + 2] = i + 2;
    }
    SDL_RenderGeometry(g_state.renderer, NULL, v, SEG + 2, idx, SEG * 3);
}

/* Normalised half-width (a fraction of the full width w) along the vessel, with
 * t = 0 at the mouth and t = 1 at the base.  A Catmull-Rom spline through a few
 * turned-pottery radii keeps the silhouette smooth and symmetric, so the glass
 * reads as a graceful Valarin urn rather than the old faceted bottle. */
static float sdl_story_lamp_half_width(float t, float width)
{
    static const float r[] = {
        0.150f, /* flared lip      */
        0.104f, /* neck pinch      */
        0.232f, /* shoulder        */
        0.356f, /* upper belly     */
        0.420f, /* belly (widest)  */
        0.392f, /* lower belly     */
        0.270f, /* haunch          */
        0.150f  /* foot            */
    };
    const int n = (int)N_ELEMENTS(r);
    float u = sdl_char_sheet_clampf(t, 0.0f, 1.0f) * (float)(n - 1);
    int i = (int)u;
    float f, p0, p1, p2, p3;

    if (i > n - 2)
        i = n - 2;
    f = u - (float)i;
    p1 = r[i];
    p2 = r[i + 1];
    p0 = (i > 0) ? r[i - 1] : (2.0f * p1 - p2);
    p3 = (i + 2 < n) ? r[i + 2] : (2.0f * p2 - p1);

    return width * 0.5f * ((2.0f * p1)
        + (-p0 + p2) * f
        + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * f * f
        + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * f * f * f);
}

static void sdl_char_sheet_draw_story_lamp(float x, float y, float w,
    float h, u32b current, u32b maximum, TTF_Font* label_font,
    float label_h)
{
    enum { OUTLINE = 40, FILL_ROWS = 14, FOOT_ARC = 10 };
    SDL_Renderer *rend = g_state.renderer;
    SDL_FPoint left[OUTLINE + 1];
    SDL_FPoint right[OUTLINE + 1];
    float cx = x + w * 0.5f;
    float top = y + h * 0.090f;
    float bottom = y + h * 0.800f;
    float vessel_h = bottom - top;
    float fraction = maximum ? (float)current / (float)maximum : 0.0f;
    float foot_hw, foot_sag;
    char counter[80];
    int i;

    if (!rend || w <= 1.0f || h <= 1.0f)
        return;

    fraction = sdl_char_sheet_clampf(fraction, 0.0f, 1.0f);

    /* ---- Outer aura: the page itself glows where the light is held. ------- */
    if (fraction > 0.0f) {
        SDL_FColor aura = { 1.0f, 0.80f, 0.40f, 0.09f + 0.13f * fraction };
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_ADD);
        sdl_story_lamp_glow(cx, top + vessel_h * 0.62f,
            w * (0.72f + 0.32f * fraction),
            vessel_h * (0.58f + 0.30f * fraction), aura);
    }

    /* ---- The captured light: a warm gradient pooled bright in the belly. -- */
    if (fraction > 0.0f) {
        SDL_Vertex verts[FILL_ROWS * 2];
        int indices[(FILL_ROWS - 1) * 6];
        SDL_FColor surf = { 1.0f, 0.78f, 0.40f, 0.46f };
        SDL_FColor core = { 1.0f, 0.95f, 0.78f, 0.95f };
        float base_t = 0.972f;
        float surf_t = base_t - fraction * (base_t - 0.190f);
        int vc = 0, ic = 0;

        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
        for (i = 0; i < FILL_ROWS; i++) {
            float d = (float)i / (float)(FILL_ROWS - 1);
            float s = d * d * (3.0f - 2.0f * d);
            float t = surf_t + (base_t - surf_t) * d;
            float hw = sdl_story_lamp_half_width(t, w) * 0.86f;
            float py = top + vessel_h * t;
            SDL_FColor c;

            c.r = surf.r + (core.r - surf.r) * s;
            c.g = surf.g + (core.g - surf.g) * s;
            c.b = surf.b + (core.b - surf.b) * s;
            c.a = surf.a + (core.a - surf.a) * s;

            verts[vc].position = (SDL_FPoint){ cx - hw, py };
            verts[vc].color = c;
            verts[vc].tex_coord = (SDL_FPoint){ 0.0f, 0.0f };
            verts[vc + 1].position = (SDL_FPoint){ cx + hw, py };
            verts[vc + 1].color = c;
            verts[vc + 1].tex_coord = (SDL_FPoint){ 0.0f, 0.0f };
            if (i < FILL_ROWS - 1) {
                int b = i * 2;
                indices[ic++] = b;     indices[ic++] = b + 1;
                indices[ic++] = b + 2; indices[ic++] = b + 1;
                indices[ic++] = b + 3; indices[ic++] = b + 2;
            }
            vc += 2;
        }
        SDL_RenderGeometry(rend, NULL, verts, vc, indices, ic);

        /* A bloom low in the belly gives the pool of light some depth. */
        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_ADD);
        {
            float bt = surf_t + (base_t - surf_t) * 0.70f;
            SDL_FColor bloom = { 1.0f, 0.86f, 0.52f, 0.40f };
            sdl_story_lamp_glow(cx, top + vessel_h * bt,
                sdl_story_lamp_half_width(bt, w) * 0.95f,
                vessel_h * 0.22f, bloom);
        }

        /* The luminous surface: a glowing meniscus, not a hard bar edge. */
        {
            float fy = top + vessel_h * surf_t;
            float fhw = sdl_story_lamp_half_width(surf_t, w) * 0.84f;
            float amp = vessel_h * 0.014f;
            SDL_FColor sheen = { 1.0f, 0.95f, 0.74f, 0.55f };
            static const float wave[9] = { 0.0f, 0.5f, -0.35f, 0.45f, -0.3f,
                0.4f, -0.35f, 0.5f, 0.0f };
            SDL_FPoint surface[9];

            sdl_story_lamp_glow(cx, fy, fhw, vessel_h * 0.05f, sheen);
            SDL_SetRenderDrawColor(rend, 255, 246, 214, 235);
            for (i = 0; i < 9; i++) {
                surface[i].x = cx - fhw + 2.0f * fhw * (float)i / 8.0f;
                surface[i].y = fy + wave[i] * amp;
            }
            SDL_RenderLines(rend, surface, 9);
        }

        /* Motes adrift in the glow: living starlight, not square pixels. */
        {
            static const float mx[5] = { -0.55f, 0.48f, -0.20f, 0.60f, 0.16f };
            static const float mt[5] = { 0.32f, 0.55f, 0.74f, 0.22f, 0.88f };
            static const float mr[5] = { 0.050f, 0.034f, 0.060f, 0.028f,
                0.042f };
            SDL_FColor mote = { 1.0f, 0.96f, 0.80f, 0.85f };

            for (i = 0; i < 5; i++) {
                float t = surf_t + (base_t - surf_t) * mt[i];
                float ihw = sdl_story_lamp_half_width(t, w) * 0.78f;
                sdl_story_lamp_glow(cx + mx[i] * ihw, top + vessel_h * t,
                    w * mr[i], w * mr[i], mote);
            }
        }
    }

    /* ---- The glass: a smooth, thin gold outline drawn over the light. ----- */
    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
    for (i = 0; i <= OUTLINE; i++) {
        float t = (float)i / (float)OUTLINE;
        float py = top + vessel_h * t;
        float hw = sdl_story_lamp_half_width(t, w);
        left[i] = (SDL_FPoint){ cx - hw, py };
        right[i] = (SDL_FPoint){ cx + hw, py };
    }
    SDL_SetRenderDrawColor(rend, 236, 208, 138, 235);
    SDL_RenderLines(rend, left, OUTLINE + 1);
    SDL_RenderLines(rend, right, OUTLINE + 1);

    /* Rounded base: a shallow arc dipped just beneath the foot. */
    foot_hw = sdl_story_lamp_half_width(1.0f, w);
    foot_sag = h * 0.022f;
    {
        SDL_FPoint base[FOOT_ARC + 1];
        for (i = 0; i <= FOOT_ARC; i++) {
            float u = (float)i / (float)FOOT_ARC;
            base[i].x = cx - foot_hw + 2.0f * foot_hw * u;
            base[i].y = bottom + foot_sag * SDL_sinf(u * 3.1415927f);
        }
        SDL_RenderLines(rend, base, FOOT_ARC + 1);
    }

    /* A flared lip and one neck band -- quiet decoration on the vessel. */
    {
        float lip_hw = sdl_story_lamp_half_width(0.0f, w);
        float band_hw = sdl_story_lamp_half_width(0.16f, w);
        SDL_RenderLine(rend, cx - lip_hw, top, cx + lip_hw, top);
        SDL_RenderLine(rend, cx - band_hw, top + vessel_h * 0.16f,
            cx + band_hw, top + vessel_h * 0.16f);
    }

    /* A faint highlight down the shoulder sells the curve of the glass. */
    {
        SDL_FPoint shine[5];
        SDL_SetRenderDrawColor(rend, 255, 252, 235, 70);
        for (i = 0; i < 5; i++) {
            float t = 0.26f + 0.24f * (float)i / 4.0f;
            float hw = sdl_story_lamp_half_width(t, w);
            shine[i].x = cx - hw * 0.62f;
            shine[i].y = top + vessel_h * t;
        }
        SDL_RenderLines(rend, shine, 5);
    }

    /* ---- A small radiant star at the mouth: the held light, shining out. -- */
    {
        float sx = cx;
        float sy = top - h * 0.022f;
        float ray = w * 0.12f * (0.6f + 0.4f * fraction);
        SDL_FColor spark = { 1.0f, 0.94f, 0.74f, 0.40f + 0.45f * fraction };
        Uint8 la = (Uint8)(110 + 130 * fraction);

        SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_ADD);
        sdl_story_lamp_glow(sx, sy, ray * 1.3f, ray * 1.3f, spark);
        SDL_SetRenderDrawColor(rend, 255, 248, 220, la);
        SDL_RenderLine(rend, sx - ray, sy, sx + ray, sy);
        SDL_RenderLine(rend, sx, sy - ray * 1.25f, sx, sy + ray * 1.25f);
        SDL_RenderLine(rend, sx - ray * 0.5f, sy - ray * 0.5f,
            sx + ray * 0.5f, sy + ray * 0.5f);
        SDL_RenderLine(rend, sx - ray * 0.5f, sy + ray * 0.5f,
            sx + ray * 0.5f, sy - ray * 0.5f);
    }

    SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);

    strnfmt(counter, sizeof(counter), "Light %lu / %lu",
        (unsigned long)current, (unsigned long)maximum);
    (void)sdl_char_sheet_draw_text(label_font, counter, TERM_YELLOW, x,
        y + h - label_h, w, label_h, true);
}

void sdl_character_sheet_screen_add_book_paragraph_colored(cptr text,
    int attr)
{
    int before = g_sdl_character_sheet_screen.narrative_para_count;

    sdl_character_sheet_screen_add_book_paragraph(text);
    if (g_sdl_character_sheet_screen.narrative_para_count > before) {
        int n = g_sdl_character_sheet_screen.narrative_para_count - 1;
        g_sdl_character_sheet_screen.narrative_para_attr[n] = (byte)attr;
    }
}

/* Adjacent previous/next turn used by arrows, keys, and swipes. */
void sdl_character_sheet_screen_begin_page_turn(int dir)
{
    int from = g_sdl_character_sheet_screen.select_page;

    sdl_character_sheet_screen_begin_page_turn_to(
        from + (dir >= 0 ? 1 : -1));
}

/* Add an accent-coloured, pointer-focusable action to a narrative page. */
void sdl_character_sheet_screen_add_book_action(cptr text, int choice)
{
    sdl_character_sheet_screen_add_book_action_colored(text, choice,
        TERM_L_BLUE);
}

void sdl_character_sheet_screen_add_book_action_colored(cptr text, int choice,
    int attr)
{
    int before = g_sdl_character_sheet_screen.narrative_para_count;

    if (choice < 0)
        return;
    sdl_character_sheet_screen_add_book_paragraph(text);
    if (g_sdl_character_sheet_screen.narrative_para_count > before) {
        int n = g_sdl_character_sheet_screen.narrative_para_count - 1;
        g_sdl_character_sheet_screen.narrative_para_choice[n] = choice;
        g_sdl_character_sheet_screen.narrative_para_attr[n] = (byte)attr;
    }
}

void sdl_character_sheet_screen_add_book_contents(cptr label, int choice,
    int page)
{
    int n;

    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_NARRATIVE
        || !label || !label[0] || choice < 0)
        return;
    n = g_sdl_character_sheet_screen.narrative_contents_count;
    if (n >= SDL_BOOK_MAX_CONTENTS)
        return;
    SDL_strlcpy(g_sdl_character_sheet_screen.narrative_contents_label[n],
        label, sizeof(g_sdl_character_sheet_screen.narrative_contents_label[n]));
    g_sdl_character_sheet_screen.narrative_contents_choice[n] = choice;
    g_sdl_character_sheet_screen.narrative_contents_page[n] = MAX(0, page);
    g_sdl_character_sheet_screen.narrative_contents_count = n + 1;
    sdl_character_sheet_narrative_layout_changed();
}

void sdl_character_sheet_screen_set_book_lamp(u32b current, u32b maximum,
    int page)
{
    bool layout_changed;

    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_NARRATIVE)
        return;
    page = MAX(0, page);
    layout_changed = !g_sdl_character_sheet_screen.narrative_lamp_enabled
        || g_sdl_character_sheet_screen.narrative_lamp_page != page;
    g_sdl_character_sheet_screen.narrative_lamp_enabled = true;
    g_sdl_character_sheet_screen.narrative_lamp_current = current;
    g_sdl_character_sheet_screen.narrative_lamp_maximum = maximum;
    g_sdl_character_sheet_screen.narrative_lamp_page = page;
    if (layout_changed)
        sdl_character_sheet_narrative_layout_changed();
}

/* Opt this narrative book into an on-screen exit button in the bottom control
 * row, so the reader can leave with the mouse (or a touch tap) from any page
 * without the keyboard.  Quest dialogue books leave this off. */
void sdl_character_sheet_screen_set_book_close_button(bool enabled)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_NARRATIVE)
        return;
    g_sdl_character_sheet_screen.narrative_close_enabled = enabled;
}

void sdl_character_sheet_screen_set_book_close_label(cptr label)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_NARRATIVE)
        return;
    SDL_strlcpy(g_sdl_character_sheet_screen.narrative_close_label,
        (label && label[0]) ? label : "Close",
        sizeof(g_sdl_character_sheet_screen.narrative_close_label));
}

void sdl_character_sheet_screen_set_book_target_page_count(int page_count)
{
    int target;

    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_NARRATIVE)
        return;

    target = sdl_char_sheet_clampi(page_count, 0, SDL_BOOK_MAX_PAGES);
    if (target != g_sdl_character_sheet_screen.narrative_target_page_count) {
        g_sdl_character_sheet_screen.narrative_target_page_count = target;
        sdl_character_sheet_narrative_layout_changed();
    }
    g_state.need_present = true;
}

/* Force the next added paragraph to begin a fresh page (author-placed break). */
void sdl_character_sheet_screen_break_book_page(void)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_NARRATIVE)
        return;
    g_sdl_character_sheet_screen.narrative_pending_break = true;
}

/* Flag the next added paragraph to be drawn in the accent colour (light blue),
 * used to set a quest's actual task and reward apart from its narration. */
void sdl_character_sheet_screen_highlight_book_paragraph(void)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_NARRATIVE)
        return;
    g_sdl_character_sheet_screen.narrative_pending_highlight = true;
}

void sdl_character_sheet_screen_commit_book(void)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_NARRATIVE)
        return;

    /* Force a (re)paginate on the next render and open on the first page. */
    sdl_character_sheet_narrative_layout_changed();
    g_sdl_character_sheet_screen.select_page = 0;
    g_state.need_present = true;
}

/* Restore a narrative book to a previously visible page after rebuilding its
 * live data.  Pagination clamps the request once the next frame is laid out. */
void sdl_character_sheet_screen_set_book_page(int page)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_NARRATIVE)
        return;
    g_sdl_character_sheet_screen.select_page = MAX(0, page);
    g_state.need_present = true;
}

void sdl_character_sheet_screen_add_select_row(int choice, cptr label,
    int attr, cptr desc)
{
    sdl_character_sheet_select_row* row;

    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return;
    if (g_sdl_character_sheet_screen.select_row_count
        >= (int)N_ELEMENTS(g_sdl_character_sheet_screen.select_rows))
    {
        return;
    }

    row = &g_sdl_character_sheet_screen
               .select_rows[g_sdl_character_sheet_screen.select_row_count++];
    row->choice = choice;
    row->reset_choice = -1;
    row->attr = (byte)attr;
    row->confirmable = true;
    row->is_heading = false;
    SDL_strlcpy(row->label, label ? label : "", sizeof(row->label));
    SDL_strlcpy(row->desc, desc ? desc : "", sizeof(row->desc));
}

/*
 * Attach a tappable per-row "Reset" button to the most recently added select
 * row.  reset_choice is the menu-click id the button reports when clicked or
 * tapped; pass -1 to remove the button.  Only meaningful for menu-style select
 * lists (settings menus); other select screens leave reset_choice at -1.
 */
void sdl_character_sheet_screen_set_last_select_row_reset(int reset_choice)
{
    int count = g_sdl_character_sheet_screen.select_row_count;

    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return;
    if (count <= 0)
        return;

    g_sdl_character_sheet_screen.select_rows[count - 1].reset_choice =
        reset_choice;
}

void sdl_character_sheet_screen_set_last_select_row_confirmable(
    bool confirmable)
{
    int count = g_sdl_character_sheet_screen.select_row_count;

    if (g_sdl_character_sheet_screen.context
        != SDL_CHARACTER_SHEET_BIRTH_SELECT)
    {
        return;
    }
    if (count <= 0)
        return;

    g_sdl_character_sheet_screen.select_rows[count - 1].confirmable =
        confirmable;
}

/* Book mode: a non-selectable heading/blurb row (e.g. "The Noldor ..."). */
void sdl_character_sheet_screen_add_select_heading(cptr label)
{
    sdl_character_sheet_select_row* row;

    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return;
    if (!label || !label[0])
        return;
    if (g_sdl_character_sheet_screen.select_row_count
        >= (int)N_ELEMENTS(g_sdl_character_sheet_screen.select_rows))
    {
        return;
    }

    row = &g_sdl_character_sheet_screen
               .select_rows[g_sdl_character_sheet_screen.select_row_count++];
    row->choice = -1;
    row->reset_choice = -1;
    row->attr = TERM_L_DARK;
    row->confirmable = false;
    row->is_heading = true;
    SDL_strlcpy(row->label, label, sizeof(row->label));
    row->desc[0] = '\0';
}

/* Book mode: the explanation text shown at the top of the page. Enables the
 * story/explanation layout (no detail panel, no hover pop-ups). */
void sdl_character_sheet_screen_set_select_intro(cptr text)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return;
    SDL_strlcpy(g_sdl_character_sheet_screen.select_intro, text ? text : "",
        sizeof(g_sdl_character_sheet_screen.select_intro));
    g_sdl_character_sheet_screen.select_book_mode = true;
    g_sdl_character_sheet_screen.select_page_count =
        sdl_char_sheet_race_book_page_count_for_intro(text);
    if (g_sdl_character_sheet_screen.select_page < 0
        || g_sdl_character_sheet_screen.select_page
            >= g_sdl_character_sheet_screen.select_page_count)
        g_sdl_character_sheet_screen.select_page = 0;
}

void sdl_character_sheet_screen_show_select_choice_page_only(void)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT
        || !g_sdl_character_sheet_screen.select_book_mode)
    {
        return;
    }

    sdl_select_page_turn_free();
    g_sdl_select_choice_page_only = true;
    g_sdl_character_sheet_screen.select_page =
        sdl_char_sheet_book_choice_page();
    g_state.need_present = true;
}

/* Open the book on its choice (last) page while leaving the earlier story
 * pages reachable -- used when returning to race selection from the character
 * page so the player lands back on the list they chose from, not page 0.
 * Unlike show_select_choice_page_only() this does not hide the story pages. */
void sdl_character_sheet_screen_open_select_choice_page(void)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT
        || !g_sdl_character_sheet_screen.select_book_mode)
    {
        return;
    }

    sdl_select_page_turn_free();
    g_sdl_character_sheet_screen.select_page =
        sdl_char_sheet_book_choice_page();
    g_state.need_present = true;
}

void sdl_character_sheet_screen_set_select_dynamic_description(bool enabled)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return;

    g_sdl_select_dynamic_description = enabled;
    g_state.need_present = true;
}

int sdl_character_sheet_screen_select_menu_rows_per_column(void)
{
    return g_sdl_select_menu_rows_per_column;
}

/* Book navigation accessors for the birth input loop. */
int sdl_character_sheet_screen_select_page(void)
{
    return g_sdl_character_sheet_screen.select_page;
}

int sdl_character_sheet_screen_select_page_count(void)
{
    return g_sdl_character_sheet_screen.select_page_count;
}

bool sdl_character_sheet_screen_page_turning(void)
{
    return g_sdl_character_sheet_screen.page_turn_active;
}

/* Book mode: framing lines (accent colour) above and below the chronicle --
 * the second-person "trial" voice that brackets the historical text. */
void sdl_character_sheet_screen_set_select_frame(cptr top, cptr bottom)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return;
    SDL_strlcpy(g_sdl_character_sheet_screen.select_frame_top, top ? top : "",
        sizeof(g_sdl_character_sheet_screen.select_frame_top));
    SDL_strlcpy(g_sdl_character_sheet_screen.select_frame_bottom,
        bottom ? bottom : "",
        sizeof(g_sdl_character_sheet_screen.select_frame_bottom));
}

void sdl_character_sheet_screen_set_select_title_detail(cptr title,
    cptr suffix, int suffix_attr)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return;

    SDL_strlcpy(g_sdl_character_sheet_screen.select_focus_title,
        title ? title : "",
        sizeof(g_sdl_character_sheet_screen.select_focus_title));
    SDL_strlcpy(g_sdl_character_sheet_screen.select_title_suffix,
        suffix ? suffix : "",
        sizeof(g_sdl_character_sheet_screen.select_title_suffix));
    g_sdl_character_sheet_screen.select_title_suffix_attr = (byte)suffix_attr;
}

void sdl_character_sheet_screen_add_select_title_candidate(cptr title,
    cptr suffix)
{
    sdl_character_sheet_title_candidate* candidate;

    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return;
    if (!title || !title[0])
        return;
    if (g_sdl_character_sheet_screen.select_title_candidate_count
        >= (int)N_ELEMENTS(
            g_sdl_character_sheet_screen.select_title_candidates))
    {
        return;
    }

    candidate = &g_sdl_character_sheet_screen.select_title_candidates[
        g_sdl_character_sheet_screen.select_title_candidate_count++];
    SDL_strlcpy(candidate->title, title, sizeof(candidate->title));
    SDL_strlcpy(candidate->suffix, suffix ? suffix : "",
        sizeof(candidate->suffix));
    g_sdl_character_sheet_screen.select_title_power_px = 0;
}

void sdl_character_sheet_screen_begin_select_rating_summary(cptr title)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return;

    g_sdl_character_sheet_screen.select_rating_count = 0;
    SDL_strlcpy(g_sdl_character_sheet_screen.select_rating_title,
        title ? title : "",
        sizeof(g_sdl_character_sheet_screen.select_rating_title));
}

void sdl_character_sheet_screen_add_select_rating(cptr group, cptr stars,
    int count, int attr, cptr desc)
{
    sdl_character_sheet_select_rating* row;

    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return;
    if (!group || !group[0] || !stars || !stars[0])
        return;
    if (g_sdl_character_sheet_screen.select_rating_count
        >= (int)N_ELEMENTS(g_sdl_character_sheet_screen.select_ratings))
    {
        return;
    }

    row = &g_sdl_character_sheet_screen
               .select_ratings[g_sdl_character_sheet_screen.select_rating_count++];
    row->attr = (byte)attr;
    row->count = count;
    SDL_strlcpy(row->group, group, sizeof(row->group));
    SDL_strlcpy(row->stars, stars, sizeof(row->stars));
    SDL_strlcpy(row->desc, desc ? desc : "", sizeof(row->desc));
}

void sdl_character_sheet_screen_add_select_detail(cptr text, int attr,
    cptr desc)
{
    sdl_character_sheet_select_detail* d;

    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return;
    if (!text || !text[0])
        return;
    if (g_sdl_character_sheet_screen.select_detail_count
        >= (int)N_ELEMENTS(g_sdl_character_sheet_screen.select_detail))
    {
        return;
    }

    d = &g_sdl_character_sheet_screen
             .select_detail[g_sdl_character_sheet_screen.select_detail_count++];
    d->attr = (byte)attr;
    SDL_strlcpy(d->text, text, sizeof(d->text));
    SDL_strlcpy(d->desc, desc ? desc : "", sizeof(d->desc));
}

void sdl_character_sheet_screen_set_select_detail_size_hint(int stat_rows,
    int ability_rows, int trait_rows)
{
    int max_rows = SDL_CHAR_SHEET_MAX_LINES;

    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return;

    if (stat_rows < 0)
        stat_rows = 0;
    if (ability_rows < 0)
        ability_rows = 0;
    if (trait_rows < 0)
        trait_rows = 0;
    if (stat_rows > max_rows)
        stat_rows = max_rows;
    if (ability_rows > max_rows - stat_rows)
        ability_rows = max_rows - stat_rows;
    if (trait_rows > max_rows - stat_rows - ability_rows)
        trait_rows = max_rows - stat_rows - ability_rows;

    g_sdl_character_sheet_screen.select_stat_rows_hint = stat_rows;
    g_sdl_character_sheet_screen.select_ability_rows_hint = ability_rows;
    g_sdl_character_sheet_screen.select_trait_rows_hint = trait_rows;
}

void sdl_character_sheet_screen_set_select_ability_rows(int rows)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return;

    if (rows < 0)
        rows = 0;
    if (rows > SDL_CHAR_SHEET_MAX_LINES)
        rows = SDL_CHAR_SHEET_MAX_LINES;
    g_sdl_character_sheet_screen.select_ability_rows = rows;
}

/* The longest description in the set: used to size the description band so the
 * layout does not reflow as the highlighted entry changes. */
void sdl_character_sheet_screen_set_select_size_hint(cptr longest_desc)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return;
    SDL_strlcpy(g_sdl_character_sheet_screen.select_desc_sizing,
        longest_desc ? longest_desc : "",
        sizeof(g_sdl_character_sheet_screen.select_desc_sizing));
}

void sdl_character_sheet_screen_add_select_description_candidate(cptr text)
{
    int idx;
    const unsigned char* p;
    Uint64 hash;

    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return;
    if (!text || !text[0])
        return;

    idx = g_sdl_character_sheet_screen.select_desc_candidate_count;
    if (idx < 0
        || idx >= (int)N_ELEMENTS(
            g_sdl_character_sheet_screen.select_desc_candidates))
    {
        return;
    }

    SDL_strlcpy(g_sdl_character_sheet_screen.select_desc_candidates[idx],
        text, sizeof(g_sdl_character_sheet_screen.select_desc_candidates[idx]));
    hash = g_sdl_character_sheet_screen.select_desc_candidate_hash;
    p = (const unsigned char*)
        g_sdl_character_sheet_screen.select_desc_candidates[idx];
    while (*p)
    {
        hash ^= (Uint64)*p++;
        hash *= 1099511628211ULL;
    }
    /* Delimit candidates so ["ab", "c"] and ["a", "bc"] differ. */
    hash ^= 0xffU;
    hash *= 1099511628211ULL;
    g_sdl_character_sheet_screen.select_desc_candidate_hash = hash;
    g_sdl_character_sheet_screen.select_desc_candidate_count = idx + 1;
}

/* Register one character's welcome (the first paragraph of its text) for
 * measurement at the real render font.  Reset by begin_select; character
 * selection feeds only the characters belonging to the selected race. */
void sdl_character_sheet_screen_add_select_welcome(cptr text)
{
    int idx = g_sdl_character_sheet_screen.select_welcome_count;
    char first[160];
    cptr body;
    bool split;

    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return;
    if (idx < 0
        || idx >= (int)N_ELEMENTS(g_sdl_character_sheet_screen.select_welcome))
        return;
    if (!text || !text[0])
        return;
    split = sdl_char_sheet_split_first_paragraph(text, first, sizeof(first),
        &body);
    if (!split)
    {
        SDL_strlcpy(first, text, sizeof(first));
        body = text;
    }
    SDL_strlcpy(g_sdl_character_sheet_screen.select_welcome[idx], first,
        sizeof(g_sdl_character_sheet_screen.select_welcome[idx]));
    g_sdl_character_sheet_screen.select_welcome_count = idx + 1;
    sdl_character_sheet_screen_add_select_description_candidate(
        body ? body : "");
}

void sdl_character_sheet_screen_set_select_description(cptr text)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return;
    SDL_strlcpy(g_sdl_character_sheet_screen.select_description,
        text ? text : "",
        sizeof(g_sdl_character_sheet_screen.select_description));
}

bool sdl_character_sheet_screen_commit_select(int selected_index)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return false;
    if (g_sdl_character_sheet_screen.hover_choice >= 0
        && g_sdl_character_sheet_screen.hover_choice != selected_index)
    {
        g_sdl_character_sheet_screen.hover_choice = SDL_CHAR_SHEET_NO_HOVER;
        ui_menu_click_clear_pending_hover();
    }
    g_sdl_character_sheet_screen.selected_index = selected_index;
    g_sdl_character_sheet_screen.focus_choice = selected_index;
    g_state.need_present = true;
    return true;
}

bool sdl_character_sheet_screen_handle_pointer_motion(float x, float y)
{
    const sdl_character_sheet_hit* hit;
    bool wake = false;

    if (!sdl_character_sheet_screen_active())
        return false;

    /* The help popup is the topmost touch target.  Preserve it while the
     * pointer moves onto it so the subsequent tap can dismiss it rather than
     * reaching a covered row. */
    if (sdl_char_sheet_hover_tooltip_contains(x, y))
        return true;

    hit = sdl_char_sheet_hit_at(x, y);
    if (hit)
    {
        bool pressable = (hit->choice < 0)
            || sdl_char_sheet_choice_pressable(hit->choice);

        if (g_sdl_character_sheet_screen.hover_choice != hit->choice)
        {
            g_sdl_character_sheet_screen.hover_choice = hit->choice;
            g_state.need_present = true;
        }
        if (!pressable)
        {
            ui_menu_click_clear_pending_hover();
            return true;
        }
        if (ui_menu_click_handle_choice_action(hit->choice,
                UI_MENU_CLICK_HOVER, &wake)
            && wake)
        {
            Term_keypress(UI_MENU_CLICK_WAKE_KEY);
        }
        return true;
    }

    if (g_sdl_character_sheet_screen.hover_choice != SDL_CHAR_SHEET_NO_HOVER)
    {
        g_sdl_character_sheet_screen.hover_choice = SDL_CHAR_SHEET_NO_HOVER;
        g_state.need_present = true;
    }
    if (ui_menu_click_clear_hover(&wake) && wake)
        Term_keypress(UI_MENU_CLICK_WAKE_KEY);

    return true;
}

bool sdl_character_sheet_screen_handle_pointer_button(float x, float y,
    int action)
{
    const sdl_character_sheet_hit* hit;

    if (!sdl_character_sheet_screen_active())
        return false;

    /* Consume a tap on the visible help text before consulting the underlying
     * character-sheet hit map.  Dismissing help must not change selection. */
    if (sdl_char_sheet_dismiss_hover_tooltip_at(x, y))
        return true;

    hit = sdl_char_sheet_hit_at(x, y);
    if (!hit)
        return true;

    g_sdl_character_sheet_screen.hover_choice = hit->choice;
    if (hit->choice >= 0 && !sdl_char_sheet_choice_pressable(hit->choice))
    {
        ui_menu_click_clear_pending_hover();
        g_state.need_present = true;
        return true;
    }
    if (!ui_menu_click_handle_choice_action(hit->choice, action, NULL))
        return true;
    /* Prompt buttons use -1 for Back and -2 for Choose/Confirm.  The
     * semantic settings menus still inspect the key returned by inkey(), so
     * injecting Enter for both buttons makes Back activate the selected row.
     * Keep the click choice pending for callers that consume it explicitly,
     * while also injecting the matching keyboard equivalent. */
    Term_keypress(hit->choice == -1 ? ESCAPE : '\r');
    g_state.need_present = true;
    return true;
}

bool sdl_character_sheet_screen_handle_pointer_event(
    const SDL_Event* ev)
{
    float x;
    float y;

    if (!ev || !sdl_character_sheet_screen_active())
        return false;

    switch (ev->type)
    {
    case SDL_EVENT_MOUSE_MOTION:
        if (ev->motion.which == SDL_TOUCH_MOUSEID)
            return true;
        x = (float)ev->motion.x;
        y = (float)ev->motion.y;
        if (sdl_char_sheet_book_context())
            (void)sdl_narrative_portrait_transform_pointer(&x, &y);
        return sdl_character_sheet_screen_handle_pointer_motion(x, y);

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (ev->button.which == SDL_TOUCH_MOUSEID)
            return true;
        x = (float)ev->button.x;
        y = (float)ev->button.y;
        if (sdl_char_sheet_book_context())
            (void)sdl_narrative_portrait_transform_pointer(&x, &y);
        if (ev->button.button == SDL_BUTTON_RIGHT)
            return sdl_character_sheet_screen_handle_pointer_button(
                x, y, UI_MENU_CLICK_SECONDARY);
        if (ev->button.button == SDL_BUTTON_LEFT)
            return sdl_character_sheet_screen_handle_pointer_button(
                x, y, UI_MENU_CLICK_PRIMARY);
        return true;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        return true;

    case SDL_EVENT_MOUSE_WHEEL:
        if (g_sdl_character_sheet_screen.sheet_scroll_max > 0)
        {
            int step = (int)(ev->wheel.y * 64.0f);

            (void)sdl_character_sheet_set_scroll(
                g_sdl_character_sheet_screen.sheet_scroll - step);
        }
        return true;

    case SDL_EVENT_FINGER_DOWN:
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return true;
        if (!sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return true;
        if (sdl_char_sheet_book_context())
            (void)sdl_narrative_portrait_transform_pointer(&x, &y);
#if SIL_SDL_MOBILE_BUILD
        if (sdl_character_sheet_select_scroll_begin(x, y,
                ev->tfinger.fingerID))
        {
            /* Only the hero carousel uses a horizontal swipe to change
             * selection; option-style lists just scroll vertically. */
            if (sdl_character_sheet_mobile_character_select_active()
                || g_sdl_character_sheet_screen.context
                    == SDL_CHARACTER_SHEET_NARRATIVE)
            {
                sdl_character_sheet_birth_swipe_begin(x, y,
                    ev->tfinger.fingerID);
            }
            if (g_sdl_character_sheet_screen.context
                == SDL_CHARACTER_SHEET_NARRATIVE)
            {
                sdl_character_sheet_touch_press_begin(x, y,
                    ev->tfinger.fingerID);
            }
            return true;
        }
#endif
        if (sdl_character_sheet_screen_birth_sequence_active()
            || g_sdl_character_sheet_screen.context
                == SDL_CHARACTER_SHEET_NARRATIVE)
        {
            sdl_character_sheet_touch_press_begin(x, y,
                ev->tfinger.fingerID);
            sdl_character_sheet_birth_swipe_begin(x, y, ev->tfinger.fingerID);
            return true;
        }
        return sdl_character_sheet_screen_handle_pointer_button(x, y,
            UI_MENU_CLICK_PRIMARY);

    case SDL_EVENT_FINGER_MOTION:
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return true;
        if (!sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return true;
        if (sdl_char_sheet_book_context())
            (void)sdl_narrative_portrait_transform_pointer(&x, &y);
#if SIL_SDL_MOBILE_BUILD
        if (g_sdl_character_sheet_screen.select_scroll_drag.active
            && g_sdl_character_sheet_screen.select_scroll_drag.finger_id
                == ev->tfinger.fingerID
            && sdl_character_sheet_select_scroll_motion(x, y,
                ev->tfinger.fingerID))
        {
            return true;
        }
#endif
        if (g_sdl_character_sheet_screen.birth_swipe.active
            && g_sdl_character_sheet_screen.birth_swipe.finger_id
                == ev->tfinger.fingerID)
        {
            bool handled;

            sdl_character_sheet_touch_press_motion(x, y,
                ev->tfinger.fingerID);
            handled = sdl_character_sheet_birth_swipe_motion(x, y,
                ev->tfinger.fingerID);
            if (g_sdl_character_sheet_screen.birth_swipe.triggered)
                sdl_character_sheet_touch_press_cancel();
            return handled;
        }
        return sdl_character_sheet_screen_handle_pointer_motion(x, y);

    case SDL_EVENT_FINGER_UP:
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return true;
        if (!sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return true;
        if (sdl_char_sheet_book_context())
            (void)sdl_narrative_portrait_transform_pointer(&x, &y);
#if SIL_SDL_MOBILE_BUILD
        if (g_sdl_character_sheet_screen.select_scroll_drag.active
            && g_sdl_character_sheet_screen.select_scroll_drag.finger_id
                == ev->tfinger.fingerID)
        {
            bool tap = false;
            bool scroll_consumed;

            scroll_consumed = sdl_character_sheet_select_scroll_motion(x, y,
                ev->tfinger.fingerID);
            if (scroll_consumed
                || g_sdl_character_sheet_screen.select_scroll_drag.active)
            {
                (void)sdl_character_sheet_select_scroll_finish(
                    ev->tfinger.fingerID, &tap);
                if (g_sdl_character_sheet_screen.birth_swipe.active
                    && g_sdl_character_sheet_screen.birth_swipe.finger_id
                        == ev->tfinger.fingerID)
                {
                    sdl_character_sheet_birth_swipe_cancel();
                }
                if (tap)
                {
                    int action =
                        sdl_character_sheet_touch_press_release_action(x, y,
                            ev->tfinger.fingerID);

                    sdl_character_sheet_touch_press_cancel();
                    return sdl_character_sheet_screen_handle_pointer_button(x,
                        y, action);
                }
                sdl_character_sheet_touch_press_cancel();
                return true;
            }
        }
#endif
        if (g_sdl_character_sheet_screen.birth_swipe.active
            && g_sdl_character_sheet_screen.birth_swipe.finger_id
                == ev->tfinger.fingerID)
        {
            bool triggered;
            int action;

            sdl_character_sheet_touch_press_motion(x, y,
                ev->tfinger.fingerID);
            (void)sdl_character_sheet_birth_swipe_motion(x, y,
                ev->tfinger.fingerID);
            triggered = g_sdl_character_sheet_screen.birth_swipe.triggered;
            action = sdl_character_sheet_touch_press_release_action(x, y,
                ev->tfinger.fingerID);
            sdl_character_sheet_birth_swipe_cancel();
            sdl_character_sheet_touch_press_cancel();
            if (!triggered)
            {
                return sdl_character_sheet_screen_handle_pointer_button(x,
                    y, action);
            }
        }
        return true;

    case SDL_EVENT_FINGER_CANCELED:
#if SIL_SDL_MOBILE_BUILD
        if (g_sdl_character_sheet_screen.select_scroll_drag.active
            && g_sdl_character_sheet_screen.select_scroll_drag.finger_id
                == ev->tfinger.fingerID)
        {
            sdl_character_sheet_select_scroll_cancel();
        }
#endif
        if (g_sdl_character_sheet_screen.birth_swipe.active
            && g_sdl_character_sheet_screen.birth_swipe.finger_id
                == ev->tfinger.fingerID)
        {
            sdl_character_sheet_birth_swipe_cancel();
        }
        if (g_sdl_character_sheet_screen.touch_press.active
            && g_sdl_character_sheet_screen.touch_press.finger_id
                == ev->tfinger.fingerID)
        {
            sdl_character_sheet_touch_press_cancel();
        }
        return true;

    default:
        return false;
    }
}
