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
    SDL_CHAR_SHEET_BIRTH_TRAIT_INFO_BASE = 9400
};

static bool g_sdl_select_choice_page_only = false;
static bool g_sdl_select_dynamic_description = false;
static int g_sdl_select_menu_rows_per_column = 0;

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
        "In the deeps of Angband, beyond gates of iron and pits of flame," },
    { TERM_WHITE, SDL_WELCOME_LINE_BODY,
        "Morgoth hoards the Silmarils — three jewels of living light." },
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
    sdl_welcome_screen_clear_hits();
    sdl_welcome_screen_mark_dirty();
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
    SDL_Surface* surface;
    SDL_Color color = { 255, 255, 255, 255 };

    if (out_w)
        *out_w = 0;
    if (out_h)
        *out_h = 0;
    if (!font || !text || !text[0])
        return false;

    surface = TTF_RenderText_Blended(font, text, 0, color);
    if (!surface)
        return false;
    if (out_w)
        *out_w = surface->w;
    if (out_h)
        *out_h = surface->h;
    SDL_DestroySurface(surface);
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
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_Color color;

    if (!font || !text || !text[0] || box.w <= 0.0f || box.h <= 0.0f)
        return dst;

    color = sdl_welcome_color(attr, 255);
    surface = TTF_RenderText_Blended(font, text, 0, color);
    if (!surface)
        return dst;

    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (!texture)
    {
        SDL_DestroySurface(surface);
        return dst;
    }

    dst = sdl_welcome_text_rect_for_size(surface->w, surface->h, box,
        centered, NULL);
    if (dst.w > 0.0f && dst.h > 0.0f)
    {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
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
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_Color color;
    char span[128];
    int span_len;
    int text_len;
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
    surface = TTF_RenderText_Blended(font, span, 0, color);
    if (!surface)
        return hit;

    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (!texture) {
        SDL_DestroySurface(surface);
        return hit;
    }

    scale = (surface->h > 0) ? hit.h / (float)surface->h : 1.0f;
    hit.w = (float)surface->w * scale;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(g_state.renderer, texture, NULL, &hit);

    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
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
        return line_h * 0.64f;

    (void)prev;
    return line_h * 0.20f;
}

static float sdl_welcome_top_margin(const SDL_Rect* canvas)
{
    return sdl_char_sheet_clampf((float)canvas->h * 0.055f, 18.0f, 66.0f);
}

static float sdl_welcome_bottom_margin(const SDL_Rect* canvas)
{
    return sdl_char_sheet_clampf((float)canvas->h * 0.055f, 18.0f, 60.0f);
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
    intro_footer_gap = sdl_char_sheet_clampf((float)base_px * 0.70f,
        10.0f, 56.0f);
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

void sdl_welcome_screen_render(void)
{
    SDL_Rect canvas;

    if (!sdl_welcome_screen_active() || !sdl_welcome_screen_available())
        return;

    canvas = sdl_get_window_pixel_rect();
    if (!sdl_rect_has_area(&canvas))
        return;

    SDL_SetRenderTarget(g_state.renderer, NULL);
    SDL_SetRenderClipRect(g_state.renderer, NULL);
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_state.renderer);

    sdl_welcome_render_intro_canvas(&canvas);

    if (g_sdl_welcome_screen.mode == SDL_WELCOME_SCREEN_MENU)
        sdl_welcome_render_menu_footer_canvas(&canvas);
    else
        sdl_welcome_render_status_canvas(&canvas);
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

SDL_FRect sdl_char_sheet_draw_text(TTF_Font* font, cptr text,
    byte attr, float x, float y, float max_w, float max_h, bool centered)
{
    SDL_FRect dst = { 0 };
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_Color color;
    float scale = 1.0f;

    if (!font || !text || !text[0] || max_w <= 0.0f)
        return dst;

    color = sdl_welcome_color(attr, 255);
    surface = TTF_RenderText_Blended(font, text, 0, color);
    if (!surface)
        return dst;

    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (!texture)
    {
        SDL_DestroySurface(surface);
        return dst;
    }

    if (surface->w > 0 && (float)surface->w > max_w)
        scale = max_w / (float)surface->w;
    if (max_h > 0.0f && surface->h > 0 && (float)surface->h * scale > max_h)
        scale = max_h / (float)surface->h;
    if (scale > 1.0f)
        scale = 1.0f;

    dst.w = (float)surface->w * scale;
    dst.h = (float)surface->h * scale;
    dst.x = centered ? x + (max_w - dst.w) * 0.5f : x;
    dst.y = y;

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(g_state.renderer, texture, NULL, &dst);

    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
    return dst;
}

void sdl_char_sheet_draw_title_text(TTF_Font* font, cptr title,
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
    suffix_w = sdl_char_sheet_text_width(font, suffix);
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
        (void)sdl_char_sheet_draw_text(font, suffix, suffix_attr,
            start_x + title_rect.w, y, max_w - title_rect.w, max_h, false);
    }
}

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

/*
 * Font pointers are part of the cache key.  Clear these non-owning references
 * before the story-font cache closes its fonts so an allocator-reused pointer
 * can never inherit an old wrapping result.
 */
void sdl_char_sheet_fitted_wrap_cache_clear(void)
{
    SDL_zero(g_fitted_wrap_cache);
    g_fitted_wrap_next_cache_entry = 0;
}

static int sdl_char_sheet_fitted_wrap_line_count(TTF_Font* font, cptr text,
    float width, int slot)
{
    if (slot == SDL_STORY_FONT_SLOT_CHAR_DESC
        && g_sdl_character_sheet_screen.context
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
            return sdl_char_sheet_wrap_text(font, text, width, NULL, 0);
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

    if (target_h < 1.0f)
        target_h = 1.0f;
    if (max_px < min_px)
        max_px = min_px;

    for (int px = max_px; px >= min_px; px--)
    {
        TTF_Font* font = sdl_story_font_for_height(px);
        float line_h = sdl_char_sheet_line_h(font, px, 1.0f);

        chosen_px = px;
        if (line_h <= target_h)
            break;
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
}

void sdl_char_sheet_add_hit(SDL_FRect rect, int choice, cptr desc)
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
    const sdl_character_sheet_live_item* item;

    if (!p_ptr)
        return 0;

#define ADD_VITAL(LABEL, VALUE, ATTR)                                           \
    do {                                                                        \
        item = sdl_char_sheet_live_label_item((LABEL));                         \
        strnfmt(label_value, sizeof(label_value), "%s\t%s", (LABEL), (VALUE));  \
        sdl_char_sheet_add_line(lines, &count, max_count, label_value, (ATTR),  \
            item ? item->choice : -1, item ? item->desc : "");                 \
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
        SDL_strlcpy(label_value, "Depth timer\t", sizeof(label_value));
        sdl_char_sheet_add_line(lines, &count, max_count, label_value,
            TERM_L_BLUE, item ? item->choice : -1,
            item ? item->desc : "Progress toward the next minimum-depth increase.");
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
            sdl_char_sheet_add_hit(hit_rect, choice, desc);

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
            sdl_char_sheet_add_hit(hit_rect, choice, desc);

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
                sdl_char_sheet_add_hit(hit, 9100 + i, r->desc);

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
        sdl_char_sheet_add_hit(row_rect, stat, desc);
    }
    else
    {
        sdl_char_sheet_add_hit(row_rect,
            SDL_CHAR_SHEET_BIRTH_STAT_INFO_BASE + stat, hint);
    }
}

void sdl_char_sheet_draw_birth_skill_table_row(TTF_Font* font,
    float x, float y, float w, float h, float line_h, int row, int skill,
    bool allocation)
{
    char buf[32];
    char desc[384];
    cptr hint;
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
#if SIL_SDL_MOBILE_BUILD
        if (sdl_touch_only_device_active())
        {
            strnfmt(desc, sizeof(desc),
                "%s: %s Cost to raise now: %d. Tap to select; tap the selected row to increase, long tap the selected row to decrease.",
                skill_names_full[skill], hint,
                g_sdl_character_sheet_screen.skill_costs[skill]);
        }
        else
#endif
        strnfmt(desc, sizeof(desc),
            "%s: %s Cost to raise now: %d. Click/tap to select; click/tap the selected row to increase, right-click to decrease.",
            skill_names_full[skill], hint,
            g_sdl_character_sheet_screen.skill_costs[skill]);
        sdl_char_sheet_add_hit(row_rect, skill, desc);
    }
    else
    {
        strnfmt(desc, sizeof(desc), "%s: %s", skill_names_full[skill],
            hint);
        sdl_char_sheet_add_hit(row_rect,
            SDL_CHAR_SHEET_BIRTH_SKILL_INFO_BASE + skill, desc);
    }
}

void sdl_char_sheet_draw_birth_status_row(TTF_Font* font, float x,
    float y, float w, float h, float line_h, int row, cptr status)
{
    SDL_FRect hit;
    bool confirm_focused;
    bool back_focused;
#if SIL_SDL_MOBILE_BUILD
    bool touch = sdl_touch_only_device_active();
    cptr confirm_text = touch ? "Confirm" : "[Confirm]";
    cptr back_text = touch ? "Back" : "[Esc]";
    int status_span = touch ? 18 : 20;
    int confirm_col = touch ? 19 : 23;
    int confirm_span = touch ? 10 : 9;
    int back_col = touch ? 30 : 33;
    int back_span = touch ? 8 : 5;
#else
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

    sdl_char_sheet_alloc_text(font, x, y, w, line_h, 6, 0, 14,
        TERM_SLATE, "Skills", false);

    for (int skill = 0; skill < S_MAX; skill++)
    {
        if (skill == S_SPC)
            continue;
        sdl_char_sheet_draw_birth_skill_table_row(font, x, y, w, h,
            line_h, skill_row++, skill, allocate_skills);
    }
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

    if (!font || !text || !text[0] || h <= 1.0f)
        return;

    SDL_zero(lines);
    draw_count = sdl_char_sheet_wrap_text(font, text, w, lines,
        SDL_CHAR_SHEET_MAX_LINES);
    if (line_count > 0 && draw_count > line_count)
        draw_count = line_count;
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
        spacing =
#if SIL_SDL_MOBILE_BUILD
            MAX(4.0f,
#else
            MAX(6.0f,
#endif
                (w - (total_w - spacing * (float)(item_count - 1)))
                    / (float)(item_count - 1));
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
        bool focused = (choice >= 0)
            ? sdl_char_sheet_choice_focused(choice)
            : sdl_char_sheet_prompt_focused(choice);

        if (!preview_prompt && cursor_x + item_w > x + w)
            break;

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
                SDL_Color fill = focused ? (SDL_Color){ 245, 245, 245, 255 }
                                         : (SDL_Color){ 156, 156, 156, 238 };
                SDL_Color border = focused ? (SDL_Color){ 0, 0, 0, 255 }
                                           : (SDL_Color){ 28, 28, 28, 230 };

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
            (void)sdl_char_sheet_draw_text(font, label,
                (preview_prompt
#if SIL_SDL_MOBILE_BUILD
                    || touch_buttons
#endif
                    ) ? TERM_DARK : (focused ? TERM_DARK : TERM_L_WHITE),
                (preview_prompt
#if SIL_SDL_MOBILE_BUILD
                    || touch_buttons
#endif
                    ) ? hit.x : cursor_x, y, hit.w, h,
                preview_prompt
#if SIL_SDL_MOBILE_BUILD
                    || touch_buttons
#endif
                );
            if (choice >= 0)
                sdl_char_sheet_add_hit(hit, choice, "");
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

static bool sdl_touch_exit_button_rect(SDL_FRect* out)
{
    const sdl_view* view = &g_views[PANE_MAIN];
    int cols;
    int rows;
    float content_x;
    float content_y;
    float content_w;
    float content_h;
    float bh;
    float bw;
    float margin;
    int font_px;
    TTF_Font* font;
    int text_w;
    float pad_x;

    if (!out)
        return false;
    *out = (SDL_FRect){ 0 };

    if (!ui_menu_click_touch_exit_button_active())
        return false;
    if (!sdl_touch_only_device_active())
        return false;
    if (!g_state.renderer)
        return false;
    if (!view->term_ready || view->cell_w <= 0 || view->cell_h <= 0
        || view->rect.w <= 0 || view->rect.h <= 0)
        return false;

    cols = sdl_main_view_visual_cols(view);
    rows = sdl_main_view_visual_rows(view);
    if (cols <= 0 || rows <= 0)
        return false;

    content_x = (float)(view->rect.x + view->margin_x);
    content_y = (float)(view->rect.y + view->margin_y);
    content_w = (float)(cols * view->cell_w);
    content_h = (float)(rows * view->cell_h);

    bh = sdl_char_sheet_clampf(content_h * 0.060f, 36.0f, 84.0f);
    font_px = sdl_touch_exit_button_font_px(bh);
    font = sdl_story_font_for_height_slot(font_px, SDL_STORY_FONT_SLOT_MENU);
    text_w = font ? sdl_char_sheet_text_width(font, "Exit") : (int)(bh * 1.4f);
    pad_x = MAX(18.0f, bh * 0.62f);
    bw = (float)text_w + pad_x * 2.0f;
    margin = sdl_char_sheet_clampf(content_h * 0.018f, 8.0f, 26.0f);

    out->w = bw;
    out->h = bh;
    out->x = content_x + content_w - margin - bw;
    out->y = content_y + content_h - margin - bh;
    return true;
}

void sdl_touch_exit_button_render(void)
{
    SDL_FRect rect;
    int font_px;
    TTF_Font* font;

    if (!sdl_touch_exit_button_rect(&rect))
        return;

    /* Match the character sheet's default (unfocused) touch button chrome. */
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 156, 156, 156, 238);
    SDL_RenderFillRect(g_state.renderer, &rect);
    SDL_SetRenderDrawColor(g_state.renderer, 28, 28, 28, 230);
    SDL_RenderRect(g_state.renderer, &rect);

    font_px = sdl_touch_exit_button_font_px(rect.h);
    font = sdl_story_font_for_height_slot(font_px, SDL_STORY_FONT_SLOT_MENU);
    if (font)
    {
        float line_h = sdl_char_sheet_line_h(font, font_px, 1.0f);
        float text_y = rect.y + (rect.h - line_h) * 0.5f;

        (void)sdl_char_sheet_draw_text(font, "Exit", TERM_DARK, rect.x, text_y,
            rect.w, rect.h, true);
    }
}

bool sdl_touch_exit_button_handle_pointer(float x, float y)
{
    SDL_FRect rect;

    if (!sdl_touch_exit_button_rect(&rect))
        return false;
    if (!sdl_point_in_frect(&rect, x, y))
        return false;

    ui_menu_click_clear_pending_hover();
    Term_keypress(ESCAPE);
    return true;
}

static void sdl_char_sheet_draw_book_page_controls(TTF_Font* prompt_font,
    float content_x, float content_w, float prompt_y, float prompt_h,
    int page, int page_count)
{
    float bw = MIN(content_w * 0.34f, prompt_h * 9.0f);
    float bh = prompt_h;
    int hov = g_sdl_character_sheet_screen.hover_choice;

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
                        "%s previous  %s continue  %s close", prev_label,
                        confirm_label, back_label);
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
                        "Left previous  Enter continue  Esc close");
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

    /* A centred "Close" button (opt-in) sits between the page arrows so the
     * book can be left with the mouse or a touch tap from any page. */
    if (g_sdl_character_sheet_screen.narrative_close_enabled)
    {
        float cw = MIN(content_w * 0.22f, bh * 6.0f);
        float ccx = content_x + (content_w - cw) * 0.5f;
        SDL_FRect r = { ccx, prompt_y, cw, bh };
        byte a = (hov == SDL_SELECT_CLICK_CLOSE) ? TERM_WHITE : TERM_SLATE;

        (void)sdl_char_sheet_draw_text(prompt_font, "Close", a, ccx, prompt_y,
            cw, bh, true);
        sdl_char_sheet_add_select_button_hit(r, SDL_SELECT_CLICK_CLOSE);
    }
}

void sdl_char_sheet_render_hover_tooltip(void);

TTF_Font* sdl_char_sheet_menu_font_for_rows(float available_h, int rows,
    int min_px, int max_px, float line_scale, float* out_line_h,
    int* out_px)
{
    int chosen_px = min_px;
    float chosen_line_h = 1.0f;
    TTF_Font* chosen_font = NULL;

    if (rows < 1)
        rows = 1;
    if (available_h < 1.0f)
        available_h = 1.0f;
    if (max_px < min_px)
        max_px = min_px;

    for (int px = max_px; px >= min_px; px--)
    {
        TTF_Font* font =
            sdl_story_font_for_height_slot(px, SDL_STORY_FONT_SLOT_MENU);
        float line_h;

        if (!font)
            continue;

        line_h = sdl_char_sheet_line_h(font, px, line_scale);
        chosen_font = font;
        chosen_px = px;
        chosen_line_h = line_h;
        if (line_h * (float)rows <= available_h)
            break;
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
    float chosen_line_h = 1.0f;
    TTF_Font* chosen_font = NULL;

    if (max_px < min_px)
        max_px = min_px;
    if (content_w < 1.0f)
        content_w = 1.0f;

    for (int px = max_px; px >= min_px; px--)
    {
        TTF_Font* font =
            sdl_story_font_for_height_slot(px, SDL_STORY_FONT_SLOT_MENU);
        float line_h;
        float longest;

        if (!font)
            continue;

        line_h = sdl_char_sheet_line_h(font, px, 1.30f);
        longest = sdl_char_sheet_menu_longest_row_w(font, line_h);

        chosen_font = font;
        chosen_px = px;
        chosen_line_h = line_h;

        if (longest <= content_w)
            break;
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
    float chosen_gap = 0.0f;
    float chosen_line_h = 1.0f;

    if (content_w < 1.0f)
        content_w = 1.0f;
    if (available_h < 1.0f)
        available_h = 1.0f;
    if (max_cols < 1)
        max_cols = 1;

    for (int px = target_px; px >= min_px && !chosen_font; px--)
    {
        TTF_Font* font = sdl_story_font_for_height_slot(px,
            SDL_STORY_FONT_SLOT_MENU);
        float line_h;
        float longest;
        float gap;

        if (!font)
            continue;

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

            chosen_font = font;
            chosen_cols = cols;
            chosen_rows = rows;
            chosen_px = px;
            chosen_gap = gap;
            chosen_line_h = line_h;
            break;
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
        sdl_char_sheet_add_hit(hit, choice, "");

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

            SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(g_state.renderer, fill.r, fill.g, fill.b,
                fill.a);
            SDL_RenderFillRect(g_state.renderer, &btn);
            SDL_SetRenderDrawColor(g_state.renderer, border.r, border.g,
                border.b, border.a);
            SDL_RenderRect(g_state.renderer, &btn);

            drawn = sdl_char_sheet_draw_text(btn_font, "Reset",
                reset_hover ? TERM_DARK : TERM_L_WHITE,
                btn.x + btn.w * 0.08f,
                btn.y + (btn.h - line_h * 0.62f) * 0.5f,
                btn.w * 0.84f, line_h * 0.62f, true);
            (void)drawn;

            /* No hover desc: a tooltip here would re-pop on every pointer
             * move over the small button and read as a blinking popup. */
            sdl_char_sheet_add_hit(btn, reset_choice, "");
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

        for (int px = target_px; px >= min_px; px--)
        {
            TTF_Font* font = sdl_story_font_for_height_slot(px,
                SDL_STORY_FONT_SLOT_MENU);
            float lh;
            int lines;
            float needed;

            if (!font)
                continue;
            lh = sdl_char_sheet_line_h(font, px, 1.16f);
            lines = sdl_char_sheet_dynamic_detail_line_count(font,
                detail_body.w);
            needed = (float)lines * lh
                + (float)(detail_count - 1) * lh * 0.34f;
            detail_font = font;
            detail_px = px;
            detail_lh = lh;
            item_gap = lh * 0.34f;
            if (needed <= detail_body.h)
                break;
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

        for (desc_px = max_px; desc_px >= min_px; desc_px--)
        {
            float measured_h;

            desc_font = sdl_story_font_for_height_slot(desc_px,
                SDL_STORY_FONT_SLOT_MENU);
            if (!desc_font)
                continue;
            desc_line_h = sdl_char_sheet_line_h(desc_font, desc_px, 1.16f);
            desc_measure_lines = sdl_char_sheet_wrap_text(desc_font,
                desc_measure ? desc_measure : desc, content_w, NULL, 0);
            desc_lines = sdl_char_sheet_wrap_text(desc_font, desc, content_w,
                NULL, 0);
            if (desc_measure_lines < desc_lines)
                desc_measure_lines = desc_lines;
            measured_h = desc_line_h * (float)desc_measure_lines;
            if (measured_h <= max_desc_h || desc_px == min_px)
                break;
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
cptr sdl_char_sheet_hover_desc(SDL_FRect* out_rect)
{
    int choice = g_sdl_character_sheet_screen.hover_choice;
    const sdl_character_sheet_live_item* item;

    if (out_rect)
        *out_rect = (SDL_FRect){ 0 };
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
        if (item && item->desc[0])
            return item->desc;
        return hit->desc;
    }

    return item ? item->desc : "";
}

void sdl_char_sheet_render_hover_tooltip(void)
{
    SDL_FRect anchor;
    SDL_Rect screen;
    TTF_Font* font;
    SDL_Surface* surface;
    SDL_Texture* texture;
    SDL_FRect box;
    SDL_FRect text_dst;
    SDL_Color text_color = g_state.palette[TERM_WHITE];
    cptr desc = sdl_char_sheet_hover_desc(&anchor);
    float pad;
    float gap;
    float margin;
    float max_box_w;
    float max_text_w;
    int font_px;
    int wrap_w;

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

    wrap_w = (int)(max_text_w + 0.5f);
    surface = TTF_RenderText_Blended_Wrapped(font, desc, 0, text_color,
        wrap_w);
    if (!surface)
        return;

    texture = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (!texture)
    {
        SDL_DestroySurface(surface);
        return;
    }

    box.w = (float)surface->w + pad * 2.0f;
    box.h = (float)surface->h + pad * 2.0f;
    box.x = anchor.x + (anchor.w - box.w) * 0.5f;
    box.y = anchor.y - box.h - gap;

    if (box.y < (float)screen.y + margin)
        box.y = anchor.y + anchor.h + gap;
    box.x = sdl_char_sheet_clampf(box.x, (float)screen.x + margin,
        (float)(screen.x + screen.w) - box.w - margin);
    box.y = sdl_char_sheet_clampf(box.y, (float)screen.y + margin,
        (float)(screen.y + screen.h) - box.h - margin);

    text_dst = (SDL_FRect){
        .x = box.x + pad,
        .y = box.y + pad,
        .w = (float)surface->w,
        .h = (float)surface->h,
    };

    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, 220);
    SDL_RenderFillRect(g_state.renderer, &box);
    SDL_SetRenderDrawColor(g_state.renderer, 255, 255, 255, 125);
    SDL_RenderRect(g_state.renderer, &box);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderTexture(g_state.renderer, texture, NULL, &text_dst);

    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
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
            p->line_count, x, y, w, h, line_h, p->label_fraction);
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

        /* Report the column that holds the category being distributed, so the
         * caller can place the Points/Confirm footer directly under it. */
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
static bool sdl_character_sheet_select_menu_scroll_active(void)
{
    return g_sdl_character_sheet_screen.context
            == SDL_CHARACTER_SHEET_BIRTH_SELECT
        && g_sdl_character_sheet_screen.select_menu_style
        && g_sdl_character_sheet_screen.sheet_scroll_max > 0;
}

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
        && !sdl_character_sheet_select_menu_scroll_active())
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
    if (scroll < 0)
        scroll = 0;
    if (scroll > g_sdl_character_sheet_screen.sheet_scroll_max)
        scroll = g_sdl_character_sheet_screen.sheet_scroll_max;
    if (scroll != g_sdl_character_sheet_screen.sheet_scroll)
    {
        g_sdl_character_sheet_screen.sheet_scroll = scroll;
        g_state.need_present = true;
    }
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
    float chosen_lh = 1.0f;
    float chosen_w = 1.0f;

    if (!box)
        return;

    if (max_px < min_px)
        max_px = min_px;
    if (w < 1.0f)
        w = 1.0f;
    if (h < 1.0f)
        h = 1.0f;

    for (int px = max_px; px >= min_px; px--)
    {
        TTF_Font* font = sdl_story_font_for_height_slot(px,
            SDL_STORY_FONT_SLOT_MENU);
        float lh;
        float natural_w;

        if (!font)
            continue;

        lh = sdl_char_sheet_line_h(font, px, 1.30f);
        natural_w = sdl_char_sheet_mobile_box_natural_w(panels, first, count,
            font);
        chosen_px = px;
        chosen_lh = lh;
        chosen_w = natural_w;

        if (lh * rows <= h && natural_w <= w)
            break;
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

/* The reading-column width for a given story px (a measure cap, but never wider
 * than a fraction of the content so the page keeps side margins). */
float sdl_char_sheet_book_width(int body_px, float content_w)
{
    /* Same book-like measure on every platform, including the mobile race
     * book -- it reads as a normal book rather than a wide mobile column. */
    float w = MIN(content_w * SDL_BOOK_WIDTH_FRAC,
        (float)body_px * SDL_BOOK_MAX_EMS);

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
 * between whole paragraphs.  Returns the page count; if page_start is non-NULL
 * it receives the first-paragraph index of each page (with a [page_count] =
 * para_count sentinel, padded out to SDL_BOOK_MAX_PAGES).
 */
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
            && g_sdl_character_sheet_screen.narrative_lamp_page == 0)
        ? lh * 9.0f : 0.0f;
    int page = 0;
    int i;

    if (region_h < lh)
        region_h = lh;
    if (page_start)
        page_start[0] = 0;

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
                            == page)
                      ? lh * 9.0f : 0.0f);
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
    int max_px = sdl_char_sheet_clampi((int)(canvas_h * 0.048f), 26, 46);
    int min_pages;
    int chosen;
    int px;

    if (max_px < min_px)
        max_px = min_px;

    min_pages = sdl_char_sheet_narrative_pack(min_px, content_w, top_y,
        region_bottom, NULL);
    chosen = min_px;
    for (px = min_px + 1; px <= max_px; px++)
    {
        if (sdl_char_sheet_narrative_pack(px, content_w, top_y, region_bottom,
                NULL) <= min_pages)
            chosen = px;
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
                sdl_char_sheet_add_hit(hit, row->choice, "");
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

/*
 * Largest size at which the two-column peoples list (plus a reserve for the
 * highlighted lore beneath it) fills the final mobile selection page.  The
 * selection is the focus of that page, so it gets its own size -- bigger than
 * the shared story size -- to read as the largest text in the book.  region_h is
 * the inner text region (the page margins are already applied by the caller).
 */
static int sdl_char_sheet_book_select_px(float content_w, float region_h,
    int min_px, int max_px, bool full_width)
{
    int row_count = g_sdl_character_sheet_screen.select_row_count;
    int split = sdl_char_sheet_book_second_heading_index();
    int chosen = min_px;

    if (max_px < min_px)
        max_px = min_px;

    for (int px = max_px; px >= min_px; px--)
    {
        TTF_Font* f = sdl_story_font_for_height_slot(px,
            SDL_STORY_FONT_SLOT_CHAR_SELECT);
        float lh = sdl_char_sheet_line_h(f, px, 1.28f);
        float list_lh = lh * 1.35f;
        float book_w = sdl_char_sheet_race_book_text_width(px, content_w,
            full_width);
        float col_gap = lh * 1.15f;
        float col_w = (book_w - col_gap) * 0.5f;
        float avail = region_h;
        float lore_reserve = lh * 3.0f;   /* a few lines for the lore below */
        float left_h;
        float right_h;
        float list_h;
        bool names_fit = true;

        if (col_w < 1.0f)
            col_w = book_w * 0.5f;

        /* Keep the people names on one line within their column: wrapped names
         * read poorly, so the column width caps how large the list may grow. */
        for (int i = 0; i < row_count; i++)
        {
            const sdl_character_sheet_select_row* r =
                &g_sdl_character_sheet_screen.select_rows[i];

            if (r->is_heading)
                continue;
            if ((float)sdl_char_sheet_text_width(f, r->label)
                > col_w - lh * 0.28f)
            {
                names_fit = false;
                break;
            }
        }

        left_h = sdl_char_sheet_book_rows_height(f, col_w, lh, list_lh,
            0, split, true);
        right_h = sdl_char_sheet_book_rows_height(f, col_w, lh, list_lh,
            split, row_count, true);
        list_h = (left_h > right_h) ? left_h : right_h;

        if (names_fit && list_h + lh * 1.6f + lore_reserve <= avail)
        {
            chosen = px;
            break;
        }
    }

    return chosen;
}

/*
 * Shared story px (no bigger than the title) for the race book.  The lore on
 * the choice page is intentionally treated as flowing remainder text; sizing to
 * the lore collapses the chronicle page on wide mobile layouts.
 */
int sdl_char_sheet_book_body_px(float canvas_h, float content_w,
    float top_y, float region_bottom, int title_px)
{
    cptr frame_top = g_sdl_character_sheet_screen.select_frame_top;
    cptr intro = g_sdl_character_sheet_screen.select_intro;
    cptr frame_bottom = g_sdl_character_sheet_screen.select_frame_bottom;
    int row_count = g_sdl_character_sheet_screen.select_row_count;
    float region_h = (region_bottom > top_y) ? (region_bottom - top_y) : 1.0f;
    int min_px = sdl_char_sheet_clampi((int)(canvas_h * 0.018f), 14, 24);
    int body_px = min_px;
    bool mobile_pages = sdl_char_sheet_race_book_mobile_pages();
    bool desktop_pages = !mobile_pages
        && g_sdl_character_sheet_screen.context
            == SDL_CHARACTER_SHEET_BIRTH_SELECT
        && g_sdl_character_sheet_screen.select_book_mode
        && g_sdl_character_sheet_screen.select_page_count >= 3;

    for (int px = title_px; px >= min_px; px -= 2)
    {
        TTF_Font* f = sdl_story_font_for_height_slot(px,
            SDL_STORY_FONT_SLOT_CHAR_SELECT);
        float lh = sdl_char_sheet_line_h(f, px, 1.28f);
        float list_lh = lh * 1.35f;
        float story_w = sdl_char_sheet_book_width(px, content_w);
        float choice_w = sdl_char_sheet_race_book_text_width(px, content_w,
            mobile_pages);
        float avail = region_h - 2.0f * (lh * SDL_BOOK_MARGIN_V);
        float choice_avail = mobile_pages ? region_h : avail;
        int ft = (frame_top && frame_top[0])
            ? sdl_char_sheet_wrap_text(f, frame_top, story_w, NULL, 0) : 0;
        int in = (intro && intro[0])
            ? sdl_char_sheet_wrap_text(f, intro, story_w, NULL, 0) : 0;
        int fb = (frame_bottom && frame_bottom[0])
            ? sdl_char_sheet_wrap_text(f, frame_bottom, story_w, NULL, 0)
            : 0;
        char intro_first[2048];
        char intro_second[2048];
        char intro_third[2048];
        cptr intro_fourth = "";
        cptr intro_desktop_second = "";
        bool desktop_split_intro = desktop_pages && intro && intro[0]
            && sdl_char_sheet_split_intro_two_pages_balanced(intro, f,
                story_w, ft, fb, intro_first, sizeof(intro_first),
                &intro_desktop_second);
        bool split_intro = mobile_pages && intro && intro[0]
            && sdl_char_sheet_split_intro_four_pages_balanced(intro, f,
                story_w, ft, fb, intro_first, sizeof(intro_first),
                intro_second, sizeof(intro_second), intro_third,
                sizeof(intro_third), &intro_fourth);
        int in_first = split_intro
            ? sdl_char_sheet_story_lines(f, intro_first, story_w)
            : (desktop_split_intro
                ? sdl_char_sheet_story_lines(f, intro_first, story_w)
                : in);
        int in_second = split_intro
            ? sdl_char_sheet_story_lines(f, intro_second, story_w)
            : (desktop_split_intro
                ? sdl_char_sheet_story_lines(f, intro_desktop_second, story_w)
                : 0);
        int in_third = split_intro
            ? sdl_char_sheet_story_lines(f, intro_third, story_w)
            : 0;
        int in_fourth = split_intro
            ? sdl_char_sheet_story_lines(f, intro_fourth, story_w)
            : 0;
        float list_h = (float)row_count * list_lh;
        float page0;
        float page1 = 0.0f;
        float page2 = 0.0f;
        float page3 = 0.0f;
        float choice_page;
        float need;

        if (mobile_pages)
        {
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
            list_h = (left_h > right_h) ? left_h : right_h;
        }

        /* Match the mandatory page layouts.  The highlighted lore is drawn into
         * whatever space remains after the list.  On mobile, the story spans
         * four balanced pages and the final choice page is full-width.  On
         * desktop, the story spans two pages and the final page is selection
         * only, which lets the shared book font grow. */
        page0 = (float)(ft + in_first) * lh + lh * 0.35f;
        if (mobile_pages)
        {
            page1 = (float)in_second * lh;
            page2 = (float)in_third * lh;
            page3 = (float)(in_fourth + fb) * lh + lh * 0.8f;
            choice_page = list_h + lh * 1.6f;
        }
        else if (desktop_pages)
        {
            float charge_gap = (in_second > 0 && fb > 0) ? lh * 0.8f : 0.0f;

            page1 = (float)(in_second + fb) * lh + charge_gap;
            choice_page = list_h + lh * 1.6f;
        }
        else
        {
            choice_page = (float)fb * lh + list_h + lh * 1.6f;
        }
        need = (page0 > page1) ? page0 : page1;
        if (page2 > need)
            need = page2;
        if (page3 > need)
            need = page3;
        if (choice_page > need)
            need = choice_page;

        body_px = px;
        if (need <= avail && (!mobile_pages || choice_page <= choice_avail))
            break;
    }
    return body_px;
}

static void sdl_char_sheet_book_body_px_cache_clear(void)
{
    g_sdl_character_sheet_screen.select_book_body_px = 0;
    g_sdl_character_sheet_screen.select_book_body_for_h = -1;
    g_sdl_character_sheet_screen.select_book_body_for_w = -1;
    g_sdl_character_sheet_screen.select_book_body_for_region_h = -1;
    g_sdl_character_sheet_screen.select_book_body_for_title_px = -1;
}

static int sdl_char_sheet_cached_book_body_px(float canvas_h, float content_w,
    float top_y, float region_bottom, int title_px)
{
    int h = (int)(canvas_h + 0.5f);
    int w = (int)(content_w + 0.5f);
    int region_h = (int)(region_bottom - top_y + 0.5f);

    if (region_h < 1)
        region_h = 1;

    if (g_sdl_character_sheet_screen.context
        == SDL_CHARACTER_SHEET_NARRATIVE)
    {
        return (g_sdl_character_sheet_screen.narrative_body_px > 0)
            ? g_sdl_character_sheet_screen.narrative_body_px : 20;
    }

    if (g_sdl_character_sheet_screen.select_book_body_px > 0
        && g_sdl_character_sheet_screen.select_book_body_for_h == h
        && g_sdl_character_sheet_screen.select_book_body_for_w == w
        && g_sdl_character_sheet_screen.select_book_body_for_region_h
            == region_h
        && g_sdl_character_sheet_screen.select_book_body_for_title_px
            == title_px)
    {
        return g_sdl_character_sheet_screen.select_book_body_px;
    }

    g_sdl_character_sheet_screen.select_book_body_px =
        sdl_char_sheet_book_body_px(canvas_h, content_w, top_y,
            region_bottom, title_px);
    g_sdl_character_sheet_screen.select_book_body_for_h = h;
    g_sdl_character_sheet_screen.select_book_body_for_w = w;
    g_sdl_character_sheet_screen.select_book_body_for_region_h = region_h;
    g_sdl_character_sheet_screen.select_book_body_for_title_px = title_px;
    return g_sdl_character_sheet_screen.select_book_body_px;
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
    int body_px;

    (void)title_px;
    if (g_sdl_character_sheet_screen.narrative_paginated_for_h == ih
        && g_sdl_character_sheet_screen.narrative_paginated_for_w == iw)
        return;

    body_px = sdl_char_sheet_narrative_choose_px(canvas_h, content_w, top_y,
        region_bottom);
    g_sdl_character_sheet_screen.narrative_body_px = body_px;
    g_sdl_character_sheet_screen.narrative_page_count =
        sdl_char_sheet_narrative_pack(body_px, content_w, top_y, region_bottom,
            g_sdl_character_sheet_screen.narrative_page_start);

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
    int first;
    int last;
    float y;
    int i;

    (void)region_bottom;

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

    /* Mimic a real book: start the text at the top of the page. */
    y = top_y;

    for (i = first; i < last; i++)
    {
        int choice = g_sdl_character_sheet_screen.narrative_para_choice[i];
        bool action = (choice >= 0);
        bool focused = action && sdl_char_sheet_choice_focused(choice);
        int lines = sdl_char_sheet_wrap_text(body_font,
            g_sdl_character_sheet_screen.narrative_paras[i], book_w, NULL, 0);
        float h = (float)lines * body_lh;
        byte attr = g_sdl_character_sheet_screen.narrative_para_attr[i];

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
            sdl_char_sheet_add_hit(hit, choice, "");
        }
        y += h;
        if (i + 1 < last)
            y += para_gap;
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
 * target; pass them shifted when the target is an offscreen snapshot.  The
 * story pages share one body size; the final mobile selection list gets its own
 * larger size so it reads as the biggest text.  register_hits adds the
 * people-row click targets on the choice page.
 */
void sdl_char_sheet_render_book_page(int page, float canvas_h,
    float content_x, float content_w, float top_y, float region_bottom,
    int title_px, bool register_hits)
{
    cptr frame_top = g_sdl_character_sheet_screen.select_frame_top;
    cptr intro = g_sdl_character_sheet_screen.select_intro;
    cptr frame_bottom = g_sdl_character_sheet_screen.select_frame_bottom;
    cptr desc = g_sdl_character_sheet_screen.select_description;
    int row_count = g_sdl_character_sheet_screen.select_row_count;
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
    int body_px = narrative
        ? (g_sdl_character_sheet_screen.narrative_body_px > 0
               ? g_sdl_character_sheet_screen.narrative_body_px : 20)
        : sdl_char_sheet_cached_book_body_px(canvas_h, content_w, top_y,
            region_bottom, title_px);
    TTF_Font* body_font;
    float body_lh;
    float list_lh;
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
    bool full_choice_page = mobile_pages && !narrative && page >= choice_page;
    float y;

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
    list_lh = body_lh * 1.35f;
    gap2 = body_lh * 0.8f;
    gap_in = body_lh * 0.35f;
    ft_lines = (frame_top && frame_top[0])
        ? sdl_char_sheet_wrap_text(body_font, frame_top, book_w, NULL, 0) : 0;
    in_lines = (intro && intro[0])
        ? sdl_char_sheet_wrap_text(body_font, intro, book_w, NULL, 0) : 0;
    fb_lines = (frame_bottom && frame_bottom[0])
        ? sdl_char_sheet_wrap_text(body_font, frame_bottom, book_w, NULL, 0) : 0;
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

        if (contents_count > 0) {
            float frame_left = MAX(content_x,
                book_x - body_lh * SDL_BOOK_MARGIN_H);
            float toc_x = content_x + content_w * 0.015f;
            float toc_right = frame_left - MAX(4.0f, body_lh * 0.42f);
            float toc_w = MAX(1.0f, toc_right - toc_x);
            int toc_px = MAX(12, (int)(body_px * 0.82f));
            TTF_Font* toc_font = NULL;
            float toc_lh = 1.0f;
            float toc_y = top_y;
            float needed_h;

            /* Fit both dimensions against the actual page edge.  This keeps
             * touch/focus rectangles out of the parchment on narrow mobile
             * layouts and avoids hard-coded desktop proportions. */
            while (toc_px > 9) {
                bool fits = true;

                toc_font = sdl_story_font_for_height_slot(toc_px,
                    SDL_STORY_FONT_SLOT_NARRATIVE);
                toc_lh = sdl_char_sheet_line_h(toc_font, toc_px, 1.3f);
                needed_h = toc_lh * (1.25f + contents_count * 1.18f);
                if (needed_h > region_bottom - top_y)
                    fits = false;
                if (sdl_char_sheet_text_width(toc_font, "Contents") > toc_w)
                    fits = false;
                for (int i = 0; fits && i < contents_count; i++) {
                    if (sdl_char_sheet_text_width(toc_font,
                            g_sdl_character_sheet_screen
                                .narrative_contents_label[i]) > toc_w)
                        fits = false;
                }
                if (fits)
                    break;
                toc_px--;
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
                bool current = g_sdl_character_sheet_screen
                    .narrative_contents_page[i] == page;
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
                    sdl_char_sheet_add_hit(hit, choice, "");
                toc_y += toc_lh * 1.18f;
            }
        }
        sdl_char_sheet_render_narrative_page(page, body_font, book_x, book_w,
            top_y, region_bottom, body_lh, register_hits);
        if (g_sdl_character_sheet_screen.narrative_lamp_enabled
            && page == g_sdl_character_sheet_screen.narrative_lamp_page)
        {
            float lamp_h = MIN(body_lh * 8.5f,
                (region_bottom - top_y) * 0.48f);
            float lamp_w = MIN(book_w * 0.56f, lamp_h * 0.58f);

            sdl_char_sheet_draw_story_lamp(
                book_x + (book_w - lamp_w) * 0.5f,
                region_bottom - lamp_h, lamp_w, lamp_h,
                g_sdl_character_sheet_screen.narrative_lamp_current,
                g_sdl_character_sheet_screen.narrative_lamp_maximum,
                body_font, body_lh);
        }
        return;
    }

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

    /* Choice page: the grouped list, then the lore.  Legacy two-page desktop
     * books also show the charge here; mobile and the three-page desktop book
     * show it on the previous story page so the final page is selection-only. */
    {
        bool two_columns = mobile_pages;
        bool show_charge = !(mobile_pages || desktop_pages);
        int split = two_columns ? sdl_char_sheet_book_second_heading_index() : 0;
        /* The two-column selection is the focus of the final mobile page, so it
         * gets its own larger size; the lore beneath keeps the story size. */
        int list_px = two_columns
            ? sdl_char_sheet_book_select_px(content_w, region_h, body_px,
                sdl_char_sheet_clampi((int)(canvas_h * 0.06f), body_px, 56),
                full_choice_page)
            : body_px;
        TTF_Font* list_font = (list_px == body_px) ? body_font
            : sdl_story_font_for_height_slot(list_px,
                SDL_STORY_FONT_SLOT_CHAR_SELECT);
        float list_body_lh = (list_px == body_px) ? body_lh
            : sdl_char_sheet_line_h(list_font, list_px, 1.28f);
        float list_row_lh = list_body_lh * 1.35f;
        float col_gap = list_body_lh * 1.15f;
        float col_w = (book_w - col_gap) * 0.5f;
        float list_h = (float)row_count * list_row_lh;

        if (two_columns)
        {
            float left_h;
            float right_h;

            if (col_w < 1.0f)
                col_w = book_w * 0.5f;
            left_h = sdl_char_sheet_book_rows_height(list_font, col_w,
                list_body_lh, list_row_lh, 0, split, true);
            right_h = sdl_char_sheet_book_rows_height(list_font, col_w,
                list_body_lh, list_row_lh, split, row_count, true);
            list_h = (left_h > right_h) ? left_h : right_h;
        }

        y = top_y;

        if (show_charge && fb_lines > 0)
        {
            sdl_char_sheet_draw_wrapped(body_font, frame_bottom, TERM_L_BLUE,
                book_x, y, book_w, (float)fb_lines * body_lh + body_lh,
                body_lh, 0);
            y += (float)fb_lines * body_lh + gap2;
        }

        if (two_columns)
        {
            float list_y = y;

            sdl_char_sheet_draw_book_row_range(list_font, book_x, list_y,
                col_w, region_bottom, list_body_lh, list_row_lh, 0, split, true,
                register_hits);
            sdl_char_sheet_draw_book_row_range(list_font,
                book_x + col_w + col_gap, list_y, col_w, region_bottom,
                list_body_lh, list_row_lh, split, row_count, true,
                register_hits);
            y += list_h;
        }
        else
        {
            for (int i = 0; i < row_count; i++)
            {
                const sdl_character_sheet_select_row* r =
                    &g_sdl_character_sheet_screen.select_rows[i];
                float text_y = y + (list_lh - body_lh) * 0.5f;

                if (text_y + body_lh * 0.5f > region_bottom)
                    break;
                if (text_y < y)
                    text_y = y;

                if (r->is_heading)
                {
                    (void)sdl_char_sheet_draw_text(body_font, r->label,
                        TERM_SLATE, book_x, text_y, book_w,
                        body_lh * 0.95f, false);
                }
                else
                {
                    bool focused = sdl_char_sheet_choice_focused(r->choice);
                    float indent = book_w * 0.05f;
                    int tw = sdl_char_sheet_text_width(body_font, r->label);
                    SDL_FRect focus = { book_x + indent, text_y,
                        MIN(book_w - indent, (float)tw + body_lh * 0.5f),
                        body_lh };
                    SDL_FRect hit = { book_x + indent, y, book_w - indent,
                        list_lh };

                    if (focused)
                        sdl_char_sheet_draw_focus_rect(focus, true);
                    (void)sdl_char_sheet_draw_text(body_font, r->label,
                        sdl_char_sheet_focus_text_attr(r->attr, focused),
                        book_x + indent, text_y, book_w - indent,
                        body_lh * 0.95f, false);
                    if (register_hits && r->choice >= 0)
                        sdl_char_sheet_add_hit(hit, r->choice, "");
                }
                y += list_lh;
            }
        }

        y += gap2;
    }
    if (region_bottom - y > body_lh * 0.5f)
        sdl_char_sheet_draw_history(body_font, desc, book_x, y, book_w,
            region_bottom - y, body_lh, 0);
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

    /*
     * Drive the curl synchronously: the render path clears page_turn_active
     * once the animation completes.  Self-contained so it does not depend on
     * the blocking event loop re-presenting on a timeout.  A wall-clock guard
     * makes sure the loop always terminates.
     */
    {
        while (g_sdl_character_sheet_screen.page_turn_active)
        {
            Uint64 now = SDL_GetTicksNS();
            Uint64 deadline = g_sdl_character_sheet_screen.page_turn_start_ns
                + (Uint64)(SDL_SELECT_PAGE_TURN_MS + 250) * 1000000ULL;
            bool timed_out = now > deadline;

            if (!g_state.renderer || timed_out)
            {
                log_warn("page turn aborted: renderer=%p timed_out=%d "
                    "page=%d dest=%d",
                    (void*)g_state.renderer, timed_out ? 1 : 0,
                    g_sdl_character_sheet_screen.select_page, dest);
                sdl_select_page_turn_free();
                break;
            }
            sdl_character_sheet_screen_render();
            SDL_RenderPresent(g_state.renderer);
            SDL_Delay(12);
        }
    }
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

void sdl_character_sheet_screen_render(void)
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
    canvas = sdl_get_layout_screen_rect();
    if (!sdl_rect_has_area(&canvas))
        return;

    sdl_char_sheet_clear_hits();
    g_sdl_character_sheet_screen.focus_choice =
        (g_sdl_character_sheet_screen.context == SDL_CHARACTER_SHEET_LIVE)
            ? g_sdl_character_sheet_screen.focus_choice
            : g_sdl_character_sheet_screen.selected_index;

    SDL_SetRenderTarget(g_state.renderer, NULL);
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
        }
#endif

        sdl_char_sheet_draw_title_text(title_font, title,
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

            /* Tapping the name confirms the focused hero. */
            if (g_sdl_character_sheet_screen.selected_index >= 0)
            {
                SDL_FRect namebox = { title_x, title_y, title_text_w,
                    title_h };

                sdl_char_sheet_add_hit(namebox,
                    g_sdl_character_sheet_screen.selected_index, "");
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
             * Story pages curl as the framed book column.  The final mobile
             * race-choice page is full-width, so transitions touching it must
             * capture the full content area or it visibly clips to the previous
             * book frame during the turn.
             */
            int body_px =
                (g_sdl_character_sheet_screen.context
                    == SDL_CHARACTER_SHEET_NARRATIVE)
                    ? (g_sdl_character_sheet_screen.narrative_body_px > 0
                           ? g_sdl_character_sheet_screen.narrative_body_px : 20)
                    : sdl_char_sheet_cached_book_body_px((float)canvas.h,
                        content_w, top_y, region_bottom, title_px);
            float book_w = sdl_char_sheet_book_width(body_px, content_w);
            float book_x = content_x + (content_w - book_w) * 0.5f;
            int body_slot = (g_sdl_character_sheet_screen.context
                    == SDL_CHARACTER_SHEET_NARRATIVE)
                ? SDL_STORY_FONT_SLOT_NARRATIVE
                : SDL_STORY_FONT_SLOT_CHAR_SELECT;
            float body_lh = sdl_char_sheet_line_h(
                sdl_story_font_for_height_slot(body_px, body_slot), body_px, 1.28f);
            float mh = body_lh * SDL_BOOK_MARGIN_H;
            float page_x = book_x - mh;
            float page_w = book_w + 2.0f * mh;
            float page_h = region_bottom - top_y;
            bool full_choice_turn =
                sdl_char_sheet_race_book_mobile_pages()
                && (from_page >= sdl_char_sheet_book_choice_page()
                    || to_page >= sdl_char_sheet_book_choice_page());
            int pw;
            int ph;
            SDL_FRect region;

            if (full_choice_turn)
            {
                page_x = content_x;
                page_w = content_w;
            }
            else if (page_x < content_x)
            {
                page_w -= (content_x - page_x);
                page_x = content_x;
            }
            if (page_x + page_w > content_x + content_w)
                page_w = content_x + content_w - page_x;
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

                if (!SDL_SetRenderTarget(g_state.renderer, prev_target))
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
                if (snapshots_ready)
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
                    if (g_sdl_character_sheet_screen.page_turn_from_tex)
                    {
                        SDL_DestroyTexture(
                            g_sdl_character_sheet_screen.page_turn_from_tex);
                    }
                    if (g_sdl_character_sheet_screen.page_turn_to_tex)
                    {
                        SDL_DestroyTexture(
                            g_sdl_character_sheet_screen.page_turn_to_tex);
                    }
                    g_sdl_character_sheet_screen.page_turn_from_tex = NULL;
                    g_sdl_character_sheet_screen.page_turn_to_tex = NULL;
                    g_sdl_character_sheet_screen.page_turn_tex_w = 0;
                    g_sdl_character_sheet_screen.page_turn_tex_h = 0;
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
        sdl_panel panels[3];
        int n = 0;
        int list_count = g_sdl_character_sheet_screen.select_row_count;
        int detail_count = g_sdl_character_sheet_screen.select_detail_count;
        int stat_rows_hint =
            g_sdl_character_sheet_screen.select_stat_rows_hint;
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
        n++;

        if (detail_count > 0)
        {
            if (stat_rows_hint > 0 || trait_rows_hint > 0)
            {
                TTF_Font* ref_font = sdl_story_font_for_height(40);
                int stat_count = stat_rows_hint;
                int trait_count;

                if (stat_count > detail_count)
                    stat_count = detail_count;
                trait_count = detail_count - stat_count;

                panels[n].kind = SDL_PANEL_KIND_SELECT_STATS;
                panels[n].heading = "Stats";
                panels[n].lines = detail_lines;
                panels[n].line_count = stat_count;
                panels[n].label_fraction = 0.62f;
                panels[n].weight = 2;
                panels[n].rows = MAX(stat_rows_hint, stat_count) + 1
                    + ((g_sdl_character_sheet_screen.select_rating_count > 0)
                        ? 4 : 0);
                panels[n].natural_w = sdl_char_sheet_sample_panel_natural_w(
                    ref_font, "Stats", "Constitution\t+99", 0.62f);
                n++;

                if (trait_rows_hint > 0 || trait_count > 0)
                {
                    panels[n].kind = SDL_PANEL_KIND_LINES;
                    panels[n].heading = "Traits";
                    panels[n].lines = detail_lines + stat_count;
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
            panels[n].rows = A_MAX + 1;
            n++;

            panels[n].kind = SDL_PANEL_KIND_ALLOC_SKILLS;
            panels[n].heading = "Skills";
            panels[n].lines = NULL;
            panels[n].line_count = 0;
            panels[n].label_fraction = 0.44f;
            panels[n].weight = 4;
            panels[n].rows = S_MAX;
            n++;
#else
            panels[n].kind = SDL_PANEL_KIND_ALLOC;
            panels[n].heading = "Attributes / Skills";
            panels[n].lines = NULL;
            panels[n].line_count = 0;
            panels[n].label_fraction = 0.44f;
            panels[n].weight = 5;
            panels[n].rows = 7 + (S_MAX - 1);
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
            char status[64];
            SDL_FRect alloc_col = { content_x, top_y, content_w, top_h };
            int ncols_bias = -1;

            sdl_char_sheet_render_columns(panels, n, content_x, top_y,
                content_w, top_h - flh - gap, canvas.h, "", NULL, ncols_bias,
                &alloc_col);
            strnfmt(status, sizeof(status), "Points Left: %d",
                g_sdl_character_sheet_screen.points_left);
            sdl_char_sheet_draw_birth_status_row(ffont, alloc_col.x,
                top_y + top_h - flh, alloc_col.w, flh, flh, 0, status);
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

bool sdl_character_sheet_screen_begin_live(int focus_choice)
{
    if (!g_state.window || !g_state.renderer)
        return false;

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
    return true;
}

bool sdl_character_sheet_screen_begin_birth_preview(void)
{
    if (!g_state.window || !g_state.renderer)
        return false;

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
    return true;
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

bool sdl_character_sheet_screen_show_birth_stats(const int* stats,
    const int* costs, int selected_stat, int points_left)
{
    if (!g_state.window || !g_state.renderer)
        return false;

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
    return true;
}

bool sdl_character_sheet_screen_show_birth_skills(const int* old_base,
    const int* skill_gain, const int* costs, int selected_skill,
    int points_left)
{
    if (!g_state.window || !g_state.renderer)
        return false;

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
    return true;
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

bool sdl_character_sheet_screen_begin_select(int focus_choice, cptr title)
{
    if (!g_state.window || !g_state.renderer)
        return false;

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
    g_sdl_character_sheet_screen.select_trait_rows_hint = 0;
    g_sdl_character_sheet_screen.last_body_px = 0;
    g_sdl_character_sheet_screen.last_body_line_h = 0.0f;
    g_sdl_character_sheet_screen.last_desc_px = 0;
    g_sdl_character_sheet_screen.last_desc_line_h = 0.0f;
    g_sdl_character_sheet_screen.select_description[0] = '\0';
    g_sdl_character_sheet_screen.select_focus_title[0] = '\0';
    g_sdl_character_sheet_screen.select_title_suffix[0] = '\0';
    g_sdl_character_sheet_screen.select_title_suffix_attr = TERM_WHITE;
    g_sdl_character_sheet_screen.select_intro[0] = '\0';
    g_sdl_character_sheet_screen.select_frame_top[0] = '\0';
    g_sdl_character_sheet_screen.select_frame_bottom[0] = '\0';
    g_sdl_character_sheet_screen.select_desc_sizing[0] = '\0';
    sdl_char_sheet_book_body_px_cache_clear();
    g_sdl_character_sheet_screen.select_scroll_rect = (SDL_FRect){ 0 };
    SDL_zero(g_sdl_character_sheet_screen.select_scroll_drag);
    g_sdl_character_sheet_screen.select_book_mode = false;
    g_sdl_character_sheet_screen.select_menu_style = false;
    g_sdl_select_choice_page_only = false;
    g_sdl_select_dynamic_description = false;
    g_sdl_select_menu_rows_per_column = 0;
    SDL_strlcpy(g_sdl_character_sheet_screen.select_title, title ? title : "",
        sizeof(g_sdl_character_sheet_screen.select_title));
    return true;
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
bool sdl_character_sheet_screen_begin_book(cptr title)
{
    if (!g_state.window || !g_state.renderer)
        return false;

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
    g_sdl_character_sheet_screen.narrative_para_count = 0;
    g_sdl_character_sheet_screen.narrative_contents_count = 0;
    g_sdl_character_sheet_screen.narrative_pending_break = false;
    g_sdl_character_sheet_screen.narrative_pending_highlight = false;
    g_sdl_character_sheet_screen.narrative_page_count = 0;
    g_sdl_character_sheet_screen.narrative_body_px = 0;
    g_sdl_character_sheet_screen.narrative_lamp_enabled = false;
    g_sdl_character_sheet_screen.narrative_lamp_current = 0;
    g_sdl_character_sheet_screen.narrative_lamp_maximum = 0;
    g_sdl_character_sheet_screen.narrative_lamp_page = 0;
    g_sdl_character_sheet_screen.narrative_close_enabled = false;
    g_sdl_character_sheet_screen.narrative_paginated_for_h = -1;
    g_sdl_character_sheet_screen.narrative_paginated_for_w = -1;
    SDL_strlcpy(g_sdl_character_sheet_screen.narrative_title,
        title ? title : "",
        sizeof(g_sdl_character_sheet_screen.narrative_title));
    return true;
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
}

void sdl_character_sheet_screen_set_book_lamp(u32b current, u32b maximum,
    int page)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_NARRATIVE)
        return;
    g_sdl_character_sheet_screen.narrative_lamp_enabled = true;
    g_sdl_character_sheet_screen.narrative_lamp_current = current;
    g_sdl_character_sheet_screen.narrative_lamp_maximum = maximum;
    g_sdl_character_sheet_screen.narrative_lamp_page = MAX(0, page);
}

/* Opt this narrative book into an on-screen "Close" button in the bottom
 * control row, so the reader can leave with the mouse (or a touch tap) from any
 * page without the keyboard.  Quest dialogue books leave this off. */
void sdl_character_sheet_screen_set_book_close_button(bool enabled)
{
    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_NARRATIVE)
        return;
    g_sdl_character_sheet_screen.narrative_close_enabled = enabled;
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
    g_sdl_character_sheet_screen.narrative_paginated_for_h = -1;
    g_sdl_character_sheet_screen.narrative_paginated_for_w = -1;
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
    row->is_heading = false;
    SDL_strlcpy(row->label, label ? label : "", sizeof(row->label));
    SDL_strlcpy(row->desc, desc ? desc : "", sizeof(row->desc));
    sdl_char_sheet_book_body_px_cache_clear();
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
    row->is_heading = true;
    SDL_strlcpy(row->label, label, sizeof(row->label));
    row->desc[0] = '\0';
    sdl_char_sheet_book_body_px_cache_clear();
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
    sdl_char_sheet_book_body_px_cache_clear();
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
    sdl_char_sheet_book_body_px_cache_clear();
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
    int trait_rows)
{
    int max_rows = SDL_CHAR_SHEET_MAX_LINES;

    if (g_sdl_character_sheet_screen.context != SDL_CHARACTER_SHEET_BIRTH_SELECT)
        return;

    if (stat_rows < 0)
        stat_rows = 0;
    if (trait_rows < 0)
        trait_rows = 0;
    if (stat_rows > max_rows)
        stat_rows = max_rows;
    if (trait_rows > max_rows - stat_rows)
        trait_rows = max_rows - stat_rows;

    g_sdl_character_sheet_screen.select_stat_rows_hint = stat_rows;
    g_sdl_character_sheet_screen.select_trait_rows_hint = trait_rows;
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

    idx = g_sdl_character_sheet_screen.select_desc_candidate_count;
    if (idx >= 0
        && idx < (int)N_ELEMENTS(
            g_sdl_character_sheet_screen.select_desc_candidates))
    {
        const unsigned char* p;
        Uint64 hash =
            g_sdl_character_sheet_screen.select_desc_candidate_hash;

        SDL_strlcpy(
            g_sdl_character_sheet_screen.select_desc_candidates[idx],
            body ? body : "",
            sizeof(g_sdl_character_sheet_screen.select_desc_candidates[idx]));
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
    Term_keypress('\r');
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
        return sdl_character_sheet_screen_handle_pointer_motion(
            (float)ev->motion.x, (float)ev->motion.y);

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (ev->button.which == SDL_TOUCH_MOUSEID)
            return true;
        if (ev->button.button == SDL_BUTTON_RIGHT)
            return sdl_character_sheet_screen_handle_pointer_button(
                (float)ev->button.x, (float)ev->button.y,
                UI_MENU_CLICK_SECONDARY);
        if (ev->button.button == SDL_BUTTON_LEFT)
            return sdl_character_sheet_screen_handle_pointer_button(
                (float)ev->button.x, (float)ev->button.y,
                UI_MENU_CLICK_PRIMARY);
        return true;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        return true;

    case SDL_EVENT_MOUSE_WHEEL:
        if (g_sdl_character_sheet_screen.sheet_scroll_max > 0)
        {
            int step = (int)(ev->wheel.y * 64.0f);
            int s = g_sdl_character_sheet_screen.sheet_scroll - step;

            if (s < 0)
                s = 0;
            if (s > g_sdl_character_sheet_screen.sheet_scroll_max)
                s = g_sdl_character_sheet_screen.sheet_scroll_max;
            if (s != g_sdl_character_sheet_screen.sheet_scroll)
            {
                g_sdl_character_sheet_screen.sheet_scroll = s;
                g_state.need_present = true;
            }
        }
        return true;

    case SDL_EVENT_FINGER_DOWN:
        if (ev->tfinger.windowID != SDL_GetWindowID(g_state.window))
            return true;
        if (!sdl_finger_event_to_render_coords(&ev->tfinger, &x, &y))
            return true;
#if SIL_SDL_MOBILE_BUILD
        if (sdl_character_sheet_select_scroll_begin(x, y,
                ev->tfinger.fingerID))
        {
            /* Only the hero carousel uses a horizontal swipe to change
             * selection; option-style lists just scroll vertically. */
            if (sdl_character_sheet_mobile_character_select_active())
                sdl_character_sheet_birth_swipe_begin(x, y,
                    ev->tfinger.fingerID);
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
