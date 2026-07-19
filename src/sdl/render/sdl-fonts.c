#include "angband.h"
#include "sdl/main-sdl-private.h"

static Uint64 g_story_font_use_clock;

void sdl_apply_font_settings(TTF_Font* font, bool is_story_font)
{
    // Select settings based on font type
    bool bold = is_story_font ? config.story_bold : config.mono_bold;
    bool italic = is_story_font ? config.story_italic : config.mono_italic;
    bool underline = is_story_font ? config.story_underline : config.mono_underline;
    bool strikethrough = is_story_font ? config.story_strikethrough : config.mono_strikethrough;
    int hinting = is_story_font ? config.story_hinting : config.mono_hinting;
    bool kerning = is_story_font ? config.story_kerning : config.mono_kerning;
    int outline = is_story_font ? config.story_outline : config.mono_outline;
    
    // Apply font style settings
    int style = TTF_STYLE_NORMAL;
    if (bold) style |= TTF_STYLE_BOLD;
    if (italic) style |= TTF_STYLE_ITALIC;
    if (underline) style |= TTF_STYLE_UNDERLINE;
    if (strikethrough) style |= TTF_STYLE_STRIKETHROUGH;
    if (style != TTF_STYLE_NORMAL) {
        TTF_SetFontStyle(font, style);
        log_debug("Applied %s font style: %d (bold=%d, italic=%d, underline=%d, strikethrough=%d)",
                 is_story_font ? "story" : "mono", style, bold, italic, underline, strikethrough);
    }
    
    // Apply hinting
    TTF_SetFontHinting(font, hinting);
    log_debug("Applied %s font hinting: %d", is_story_font ? "story" : "mono", hinting);
    
    // Apply kerning
    TTF_SetFontKerning(font, kerning);
    
    // Apply outline
    if (outline > 0) {
        TTF_SetFontOutline(font, outline);
        log_debug("Applied %s font outline: %d", is_story_font ? "story" : "mono", outline);
    }
}

mono_font_style_key sdl_current_mono_font_style_key(void)
{
    mono_font_style_key style;

    style.bold = config.mono_bold;
    style.italic = config.mono_italic;
    style.underline = config.mono_underline;
    style.strikethrough = config.mono_strikethrough;
    style.hinting = config.mono_hinting;
    style.kerning = config.mono_kerning;
    style.outline = config.mono_outline;
    return style;
}

static bool sdl_mono_font_style_keys_equal(const mono_font_style_key* a,
    const mono_font_style_key* b)
{
    return a && b
        && a->bold == b->bold
        && a->italic == b->italic
        && a->underline == b->underline
        && a->strikethrough == b->strikethrough
        && a->hinting == b->hinting
        && a->kerning == b->kerning
        && a->outline == b->outline;
}

void sdl_apply_mono_font_style_key(TTF_Font* font,
    const mono_font_style_key* style)
{
    int ttf_style = TTF_STYLE_NORMAL;

    if (!font || !style)
        return;

    if (style->bold)
        ttf_style |= TTF_STYLE_BOLD;
    if (style->italic)
        ttf_style |= TTF_STYLE_ITALIC;
    if (style->underline)
        ttf_style |= TTF_STYLE_UNDERLINE;
    if (style->strikethrough)
        ttf_style |= TTF_STYLE_STRIKETHROUGH;

    TTF_SetFontStyle(font, ttf_style);
    TTF_SetFontHinting(font, style->hinting);
    TTF_SetFontKerning(font, style->kerning);
    TTF_SetFontOutline(font, (style->outline > 0) ? style->outline : 0);
}

const char* sdl_monospace_font_path(void)
{
    return config.monospace_font[0] != '\0'
        ? config.monospace_font
        : "lib/xtra/font/VictorMono-Medium.ttf";
}

static Uint64 sdl_mono_font_atlas_generation_bump(void)
{
    g_mono_font_atlas_generation++;
    if (g_mono_font_atlas_generation == 0)
        g_mono_font_atlas_generation = 1;
    return g_mono_font_atlas_generation;
}

void sdl_mono_font_prewarm_queue_clear(void)
{
    g_mono_font_prewarm_count = 0;
}

static TTF_Font* sdl_open_fitted_mono_font(const char* font_path,
    int cell_width, int cell_height, const mono_font_style_key* style,
    int* out_font_size, char* error_buf, size_t error_buf_size)
{
    TTF_Font* font;
    int low;
    int high;
    int best = 0;

    if (out_font_size)
        *out_font_size = 0;
    if (!font_path || !font_path[0] || !style || cell_width <= 0
        || cell_height <= 0)
    {
        return NULL;
    }

    low = cell_height / 2;
    if (low < 1)
        low = 1;
    high = cell_height;
    font = TTF_OpenFont(font_path, (float)high);
    if (!font) {
        if (error_buf && error_buf_size)
            strnfmt(error_buf, error_buf_size, "TTF_OpenFont failed: %s",
                SDL_GetError());
        return NULL;
    }
    sdl_apply_mono_font_style_key(font, style);

    while (low <= high) {
        int middle = low + (high - low) / 2;
        int measured_w = 0;

        if (!TTF_SetFontSize(font, (float)middle)
            || !TTF_MeasureString(font, "M", 1, 0, &measured_w, NULL))
        {
            if (error_buf && error_buf_size)
                strnfmt(error_buf, error_buf_size,
                    "font measurement failed: %s", SDL_GetError());
            TTF_CloseFont(font);
            return NULL;
        }
        if (measured_w <= cell_width) {
            best = middle;
            low = middle + 1;
        } else {
            high = middle - 1;
        }
    }

    if (best < 1 || !TTF_SetFontSize(font, (float)best)) {
        if (error_buf && error_buf_size)
            SDL_strlcpy(error_buf, "could not find suitable font size",
                error_buf_size);
        TTF_CloseFont(font);
        return NULL;
    }
    if (out_font_size)
        *out_font_size = best;
    return font;
}

void sdl_mono_font_cache_clear(void)
{
    sdl_mono_font_prewarm_job_shutdown();
    sdl_mono_font_prewarm_queue_clear();
    sdl_ui_text_cache_clear();

    for (int i = 0; i < MAX_MONO_FONT_CACHE; i++) {
        mono_font_atlas_entry* entry = &g_state.mono_font_atlases[i];
        mono_font_entry* font_entry = &g_state.mono_fonts[i];

        if (entry->atlas) {
            SDL_DestroyTexture(entry->atlas);
            entry->atlas = NULL;
        }

        entry->valid = false;
        entry->font_path[0] = '\0';
        entry->cell_width = 0;
        entry->cell_height = 0;
        entry->bold = false;
        entry->italic = false;
        entry->underline = false;
        entry->strikethrough = false;
        entry->hinting = 0;
        entry->kerning = false;
        entry->outline = 0;
        entry->generation = 0;

        if (font_entry->font) {
            TTF_CloseFont(font_entry->font);
            font_entry->font = NULL;
        }

        font_entry->valid = false;
        font_entry->font_path[0] = '\0';
        font_entry->cell_width = 0;
        font_entry->cell_height = 0;
        font_entry->bold = false;
        font_entry->italic = false;
        font_entry->underline = false;
        font_entry->strikethrough = false;
        font_entry->hinting = 0;
        font_entry->kerning = false;
        font_entry->outline = 0;
    }

    sdl_mono_font_atlas_generation_bump();
}

TTF_Font* sdl_load_mono_font_cells(const char* font_path, int cell_width,
    int cell_height)
{
    mono_font_style_key style = sdl_current_mono_font_style_key();
    char error[256] = "";
    TTF_Font* font;

    if (!font_path || !font_path[0] || cell_width <= 0 || cell_height <= 0)
        return NULL;
    font = sdl_open_fitted_mono_font(font_path, cell_width, cell_height,
        &style, NULL, error, sizeof(error));
    if (!font)
        log_error("could not load TTF font '%s': %s", font_path,
            error[0] ? error : SDL_GetError());
    return font;
}

TTF_Font* sdl_acquire_mono_font_cells(const char* font_path,
    int cell_width, int cell_height)
{
    mono_font_entry* free_entry = NULL;

    if (!font_path || !font_path[0] || cell_height <= 0)
        return NULL;
    if (cell_width < 1)
        cell_width = 1;

    for (int i = 0; i < MAX_MONO_FONT_CACHE; i++) {
        mono_font_entry* entry = &g_state.mono_fonts[i];

        if (!entry->valid) {
            if (!free_entry)
                free_entry = entry;
            continue;
        }

        if (entry->cell_width != cell_width)
            continue;
        if (entry->cell_height != cell_height)
            continue;
        if (entry->bold != config.mono_bold
            || entry->italic != config.mono_italic
            || entry->underline != config.mono_underline
            || entry->strikethrough != config.mono_strikethrough
            || entry->hinting != config.mono_hinting
            || entry->kerning != config.mono_kerning
            || entry->outline != config.mono_outline)
            continue;
        if (!streq(entry->font_path, font_path))
            continue;

        return entry->font;
    }

    if (!free_entry)
    {
        log_warn("Monospace font cache full; cannot cache font for %s at cell %dx%d",
            font_path, cell_width, cell_height);
        return NULL;
    }

    free_entry->font = sdl_load_mono_font_cells(font_path, cell_width, cell_height);
    if (!free_entry->font)
        return NULL;

    free_entry->valid = true;
    SDL_strlcpy(free_entry->font_path, font_path, sizeof(free_entry->font_path));
    free_entry->cell_width = cell_width;
    free_entry->cell_height = cell_height;
    free_entry->bold = config.mono_bold;
    free_entry->italic = config.mono_italic;
    free_entry->underline = config.mono_underline;
    free_entry->strikethrough = config.mono_strikethrough;
    free_entry->hinting = config.mono_hinting;
    free_entry->kerning = config.mono_kerning;
    free_entry->outline = config.mono_outline;

    return free_entry->font;
}

bool sdl_mono_font_atlas_entry_matches_style(
    const mono_font_atlas_entry* entry, const char* font_path, int cell_width,
    int cell_height, const mono_font_style_key* style)
{
    return entry && entry->valid && entry->atlas
        && style
        && entry->cell_width == cell_width
        && entry->cell_height == cell_height
        && entry->bold == style->bold
        && entry->italic == style->italic
        && entry->underline == style->underline
        && entry->strikethrough == style->strikethrough
        && entry->hinting == style->hinting
        && entry->kerning == style->kerning
        && entry->outline == style->outline
        && streq(entry->font_path, font_path);
}

bool sdl_mono_font_atlas_entry_matches(
    const mono_font_atlas_entry* entry, const char* font_path, int cell_width,
    int cell_height)
{
    mono_font_style_key style = sdl_current_mono_font_style_key();

    return sdl_mono_font_atlas_entry_matches_style(entry, font_path,
        cell_width, cell_height, &style);
}

mono_font_atlas_entry* sdl_find_mono_font_atlas_fallback_cells(
    const char* font_path, int cell_width, int cell_height)
{
    mono_font_style_key style = sdl_current_mono_font_style_key();
    mono_font_atlas_entry* best = NULL;
    int best_score = 0x7fffffff;

    if (!font_path || !font_path[0] || cell_width <= 0 || cell_height <= 0)
        return NULL;

    for (int i = 0; i < MAX_MONO_FONT_CACHE; i++) {
        mono_font_atlas_entry* entry = &g_state.mono_font_atlases[i];
        int score;

        if (!entry->valid || !entry->atlas)
            continue;
        if (!sdl_mono_font_atlas_entry_matches_style(entry, font_path,
                entry->cell_width, entry->cell_height, &style))
        {
            continue;
        }

        /* Downscaling a larger atlas can drop bottom scanlines with nearest
         * sampling on small panes. Prefer an equal/smaller fallback and let
         * exact-size prewarm replace it when available. */
        score = abs(entry->cell_height - cell_height)
            + abs(entry->cell_width - cell_width);
        if (entry->cell_height > cell_height || entry->cell_width > cell_width)
            score += 1000000;
        if (!best || score < best_score) {
            best = entry;
            best_score = score;
        }
    }

    return best;
}

bool sdl_mono_font_atlas_cached_cells_style(const char* font_path,
    int cell_width, int cell_height, const mono_font_style_key* style);

bool sdl_mono_font_atlas_cached_cells(const char* font_path,
    int cell_width, int cell_height)
{
    mono_font_style_key style = sdl_current_mono_font_style_key();

    return sdl_mono_font_atlas_cached_cells_style(font_path, cell_width,
        cell_height, &style);
}

Uint64 sdl_mono_font_atlas_generation_for_cells(const char* font_path,
    int cell_width, int cell_height)
{
    mono_font_style_key style = sdl_current_mono_font_style_key();
    mono_font_atlas_entry* fallback;

    for (int i = 0; i < MAX_MONO_FONT_CACHE; i++) {
        mono_font_atlas_entry* entry = &g_state.mono_font_atlases[i];

        if (sdl_mono_font_atlas_entry_matches_style(entry, font_path,
                cell_width, cell_height, &style))
        {
            return entry->generation;
        }
    }
    fallback = sdl_find_mono_font_atlas_fallback_cells(font_path, cell_width,
        cell_height);
    return fallback ? fallback->generation : 0;
}

bool sdl_mono_font_atlas_cached_cells_style(const char* font_path,
    int cell_width, int cell_height, const mono_font_style_key* style)
{
    if (!font_path || !font_path[0] || cell_width <= 0 || cell_height <= 0)
        return false;

    for (int i = 0; i < MAX_MONO_FONT_CACHE; i++) {
        if (sdl_mono_font_atlas_entry_matches_style(
                &g_state.mono_font_atlases[i], font_path, cell_width,
                cell_height, style))
        {
            return true;
        }
    }

    return false;
}

bool sdl_mono_font_atlas_cache_has_free_slot(void)
{
    for (int i = 0; i < MAX_MONO_FONT_CACHE; i++) {
        if (!g_state.mono_font_atlases[i].valid)
            return true;
    }

    return false;
}

bool sdl_store_mono_font_atlas_cells(const char* font_path,
    int cell_width, int cell_height, SDL_Texture* atlas,
    const mono_font_style_key* style)
{
    mono_font_atlas_entry* free_entry = NULL;

    if (!font_path || !font_path[0] || !atlas || !style)
        return false;

    for (int i = 0; i < MAX_MONO_FONT_CACHE; i++) {
        mono_font_atlas_entry* entry = &g_state.mono_font_atlases[i];

        if (!entry->valid) {
            free_entry = entry;
            break;
        }
    }

    if (!free_entry)
        return false;

    free_entry->atlas = atlas;
    free_entry->valid = true;
    SDL_strlcpy(free_entry->font_path, font_path, sizeof(free_entry->font_path));
    free_entry->cell_width = cell_width;
    free_entry->cell_height = cell_height;
    free_entry->bold = style->bold;
    free_entry->italic = style->italic;
    free_entry->underline = style->underline;
    free_entry->strikethrough = style->strikethrough;
    free_entry->hinting = style->hinting;
    free_entry->kerning = style->kerning;
    free_entry->outline = style->outline;
    free_entry->generation = sdl_mono_font_atlas_generation_bump();
    return true;
}

SDL_Texture* sdl_acquire_mono_font_atlas_cells_ex(const char* font_path,
    int cell_width, int cell_height, bool* out_cached,
    int* out_atlas_cell_w, int* out_atlas_cell_h, bool* out_exact,
    bool allow_fallback)
{
    mono_font_style_key style;
    SDL_Texture* atlas;

    if (out_cached)
        *out_cached = false;
    if (out_exact)
        *out_exact = false;
    if (out_atlas_cell_w)
        *out_atlas_cell_w = cell_width;
    if (out_atlas_cell_h)
        *out_atlas_cell_h = cell_height;
    if (cell_width < 1)
        cell_width = 1;
    if (cell_height < 1)
        cell_height = 1;

    for (int i = 0; i < MAX_MONO_FONT_CACHE; i++) {
        mono_font_atlas_entry* entry = &g_state.mono_font_atlases[i];

        if (!entry->valid) {
            continue;
        }

        if (!sdl_mono_font_atlas_entry_matches(entry, font_path, cell_width,
                cell_height))
        {
            continue;
        }

        if (out_cached)
            *out_cached = true;
        if (out_exact)
            *out_exact = true;
        if (out_atlas_cell_w)
            *out_atlas_cell_w = entry->cell_width;
        if (out_atlas_cell_h)
            *out_atlas_cell_h = entry->cell_height;
        return entry->atlas;
    }

    if (allow_fallback) {
        mono_font_atlas_entry* fallback =
            sdl_find_mono_font_atlas_fallback_cells(font_path, cell_width,
                cell_height);
        if (fallback) {
            if (out_cached)
                *out_cached = true;
            if (out_exact)
                *out_exact = false;
            if (out_atlas_cell_w)
                *out_atlas_cell_w = fallback->cell_width;
            if (out_atlas_cell_h)
                *out_atlas_cell_h = fallback->cell_height;
            log_trace("Using fallback mono font atlas %dx%d for requested %dx%d",
                fallback->cell_width, fallback->cell_height, cell_width,
                cell_height);
            return fallback->atlas;
        }
    }

    style = sdl_current_mono_font_style_key();
    if (allow_fallback)
    {
        char error[256] = "";
        int try_h = cell_height;
        int try_w = cell_width;

        while (try_h >= 1)
        {
            atlas = sdl_try_load_ttf_font_cells(font_path, try_w, try_h,
                NULL, error, sizeof(error));
            if (atlas)
            {
                bool exact = (try_w == cell_width && try_h == cell_height);
                bool cached = false;

                if (sdl_store_mono_font_atlas_cells(font_path, try_w, try_h,
                        atlas, &style))
                {
                    cached = true;
                }

                if (out_cached)
                    *out_cached = cached;
                if (out_exact)
                    *out_exact = exact;
                if (out_atlas_cell_w)
                    *out_atlas_cell_w = try_w;
                if (out_atlas_cell_h)
                    *out_atlas_cell_h = try_h;

                if (!exact)
                {
                    log_warn("Using buildable mono font atlas %dx%d for requested %dx%d",
                        try_w, try_h, cell_width, cell_height);
                }

                return atlas;
            }

            try_h--;
            try_w = try_h / 2;
            if (try_w < 1)
                try_w = 1;
        }

        log_warn("Could not build fallback mono font atlas for %s at requested cell %dx%d: %s",
            font_path ? font_path : "(null)", cell_width, cell_height,
            error[0] ? error : "unknown error");
        return NULL;
    }

    atlas = sdl_load_ttf_font_cells(font_path, cell_width, cell_height, NULL);
    if (!atlas)
        return NULL;

    if (!sdl_store_mono_font_atlas_cells(font_path, cell_width, cell_height,
            atlas, &style))
    {
        SDL_DestroyTexture(atlas);
        return NULL;
    }

    if (out_cached)
        *out_cached = true;
    if (out_exact)
        *out_exact = true;

    return atlas;
}

/* Story fonts come in numbered slots (STORY_FONT_SLOTS); slot 0 is the default
 * font used everywhere, slot 1 is the secondary font assigned to select UI
 * contexts (quest book, popup menus, log).  Each slot has its own configured
 * path, style options and fallback. */
static int sdl_story_slot_clamp(int slot)
{
    if (slot < 0)
        return 0;
    if (slot >= STORY_FONT_SLOTS)
        return STORY_FONT_SLOTS - 1;
    return slot;
}

const char* sdl_story_font_path_for_slot(int slot)
{
    if (sdl_story_slot_clamp(slot) == 1)
        return (config.story_font2[0] != '\0') ? config.story_font2 : NULL;
    return (config.story_font[0] != '\0') ? config.story_font : NULL;
}

static const char* sdl_story_fallback_for_slot(int slot)
{
    return (sdl_story_slot_clamp(slot) == 1)
        ? sdl_story_fallback_font2
        : sdl_story_fallback_font;
}

static void sdl_story_slot_style(int slot, mono_font_style_key* out)
{
    if (sdl_story_slot_clamp(slot) == 1) {
        out->bold = config.story2_bold;
        out->italic = config.story2_italic;
        out->underline = config.story2_underline;
        out->strikethrough = config.story2_strikethrough;
        out->hinting = config.story2_hinting;
        out->kerning = config.story2_kerning;
        out->outline = config.story2_outline;
    } else {
        out->bold = config.story_bold;
        out->italic = config.story_italic;
        out->underline = config.story_underline;
        out->strikethrough = config.story_strikethrough;
        out->hinting = config.story_hinting;
        out->kerning = config.story_kerning;
        out->outline = config.story_outline;
    }
}

/*
 * Load a story font for a slot, applying that slot's style and falling back to
 * the slot's fallback font when the configured path fails to load.
 */
static TTF_Font* sdl_load_story_font_slot(const char* font_path, int font_size, int slot)
{
    mono_font_style_key style;
    TTF_Font* font = NULL;

    sdl_story_slot_style(slot, &style);

    if (font_path && font_path[0] != '\0') {
        font = TTF_OpenFont(font_path, font_size);
        if (font) {
            log_trace("Loaded story font (slot %d): %s at size %d", slot,
                font_path, font_size);
            sdl_apply_mono_font_style_key(font, &style);
            return font;
        }
        log_warn("Failed to load story font (slot %d) '%s': %s", slot, font_path, SDL_GetError());
    }

    const char* fallback = sdl_story_fallback_for_slot(slot);
    font = TTF_OpenFont(fallback, font_size);
    if (font) {
        log_trace("Using fallback story font (slot %d): %s at size %d",
            slot, fallback, font_size);
        sdl_apply_mono_font_style_key(font, &style);
    } else {
        log_error("Failed to load fallback story font (slot %d) '%s': %s", slot, fallback, SDL_GetError());
    }
    return font;
}

void sdl_story_font_cache_clear(void)
{
    /* Wrapped-description entries hold non-owning TTF_Font pointers. */
    sdl_char_sheet_fitted_wrap_cache_clear();
    sdl_ui_text_cache_clear();

    for (int i = 0; i < g_state.story_font_count; i++) {
        if (g_state.story_fonts[i].font) {
            TTF_CloseFont(g_state.story_fonts[i].font);
            g_state.story_fonts[i].font = NULL;
        }
        g_state.story_fonts[i].pixel_height = 0;
        g_state.story_fonts[i].slot = 0;
        g_state.story_fonts[i].last_used = 0;
    }
    g_state.story_font_count = 0;
    g_story_font_use_clock = 0;
    g_state.story_font_cache_valid = false;
    memset(g_state.story_font_cache, 0, sizeof(g_state.story_font_cache));
    g_story_font_generation++;
    if (g_story_font_generation == 0)
        g_story_font_generation = 1;
}

bool sdl_story_font_cache_matches_config(void)
{
    if (!g_state.story_font_cache_valid)
        return false;

    for (int slot = 0; slot < STORY_FONT_SLOTS; slot++) {
        const char* font_path = sdl_story_font_path_for_slot(slot);
        const story_font_slot_cache* c = &g_state.story_font_cache[slot];
        mono_font_style_key style;

        if (!font_path)
            font_path = "";
        sdl_story_slot_style(slot, &style);

        if (!streq(c->path, font_path)
            || c->bold != style.bold
            || c->italic != style.italic
            || c->underline != style.underline
            || c->strikethrough != style.strikethrough
            || c->hinting != style.hinting
            || c->kerning != style.kerning
            || c->outline != style.outline)
        {
            return false;
        }
    }

    return true;
}

void sdl_story_font_cache_mark_config(void)
{
    g_state.story_font_cache_valid = true;

    for (int slot = 0; slot < STORY_FONT_SLOTS; slot++) {
        const char* font_path = sdl_story_font_path_for_slot(slot);
        story_font_slot_cache* c = &g_state.story_font_cache[slot];
        mono_font_style_key style;

        if (!font_path)
            font_path = "";
        sdl_story_slot_style(slot, &style);

        SDL_strlcpy(c->path, font_path, sizeof(c->path));
        c->bold = style.bold;
        c->italic = style.italic;
        c->underline = style.underline;
        c->strikethrough = style.strikethrough;
        c->hinting = style.hinting;
        c->kerning = style.kerning;
        c->outline = style.outline;
    }
}

TTF_Font* sdl_story_font_for_height_slot(int pixel_height, int slot)
{
    const char* font_path;
    TTF_Font* font;
    int target_index;

    if (pixel_height <= 0)
        return NULL;

    slot = sdl_story_slot_clamp(slot);

    for (int i = 0; i < g_state.story_font_count; i++) {
        if (g_state.story_fonts[i].pixel_height == pixel_height
            && g_state.story_fonts[i].slot == slot) {
            g_state.story_fonts[i].last_used = ++g_story_font_use_clock;
            return g_state.story_fonts[i].font;
        }
    }

    font_path = sdl_story_font_path_for_slot(slot);
    font = sdl_load_story_font_slot(font_path, pixel_height, slot);
    if (!font)
        return NULL;

    if (g_state.story_font_count >= MAX_STORY_FONT_CACHE) {
        target_index = 0;

        for (int i = 1; i < g_state.story_font_count; i++) {
            if (g_state.story_fonts[i].last_used
                < g_state.story_fonts[target_index].last_used)
            {
                target_index = i;
            }
        }
        /* These caches hold borrowed font pointers/textures.  Drop references
         * to the victim before closing it without flushing unrelated labels. */
        sdl_char_sheet_fitted_wrap_cache_clear();
        sdl_ui_text_cache_clear_font(
            g_state.story_fonts[target_index].font);
        TTF_CloseFont(g_state.story_fonts[target_index].font);
    } else {
        target_index = g_state.story_font_count++;
    }

    g_state.story_fonts[target_index].pixel_height = pixel_height;
    g_state.story_fonts[target_index].slot = slot;
    g_state.story_fonts[target_index].font = font;
    g_state.story_fonts[target_index].last_used = ++g_story_font_use_clock;
    return font;
}

TTF_Font* sdl_story_font_for_height(int pixel_height)
{
    return sdl_story_font_for_height_slot(pixel_height, 0);
}

TTF_Font* sdl_story_font_for_view_slot(const sdl_view* d, int slot)
{
    if (!d)
        return NULL;
    return sdl_story_font_for_height_slot(d->cell_h, slot);
}

/*
 * Return the font of the requested slot at the SAME pixel height as an
 * already-resolved story font.  Used to render part of a line (e.g. numeric
 * values) in a different slot without re-deriving the layout's pixel size.
 */
TTF_Font* sdl_story_font_slot_sibling(TTF_Font* font, int slot)
{
    if (!font)
        return NULL;
    for (int i = 0; i < g_state.story_font_count; i++) {
        if (g_state.story_fonts[i].font == font) {
            g_state.story_fonts[i].last_used = ++g_story_font_use_clock;
            TTF_Font* sibling = sdl_story_font_for_height_slot(
                g_state.story_fonts[i].pixel_height, slot);
            return sibling ? sibling : font;
        }
    }
    return font;
}

TTF_Font* sdl_story_font_for_view(const sdl_view* d)
{
    return sdl_story_font_for_view_slot(d, 0);
}

bool sdl_mono_atlas_codepoint_visible(Uint32 ch)
{
    return (ch >= 32 && ch <= 126) || (ch >= 160 && ch <= 255);
}

void sdl_mono_atlas_set_error(char* error_buf, size_t error_buf_size,
    const char* message)
{
    if (error_buf && error_buf_size)
        SDL_strlcpy(error_buf, message ? message : "", error_buf_size);
}

SDL_Surface* sdl_build_ttf_font_atlas_surface(const char* font_path,
    int cell_width, int cell_height, const mono_font_style_key* style,
    int* actual_font_size, char* error_buf, size_t error_buf_size)
{
    int font_size = 0;
    int bottom_guard;
    int draw_height;
    TTF_Font* font = NULL;

    if (cell_width < 1)
        cell_width = 1;
    if (cell_height < 1)
        cell_height = 1;
    sdl_mono_atlas_set_error(error_buf, error_buf_size, "");
    font = sdl_open_fitted_mono_font(font_path, cell_width, cell_height,
        style, &font_size, error_buf, error_buf_size);
    if (!font) {
        return NULL;
    }

    bottom_guard = (cell_height >= 32) ? 2 : (cell_height >= 10) ? 1 : 0;
    draw_height = cell_height - bottom_guard;
    if (draw_height < 1)
        draw_height = cell_height;

    SDL_Surface* atlas_surface = SDL_CreateSurface(16 * cell_width,
        16 * cell_height, SDL_PIXELFORMAT_RGBA8888);
    if (!atlas_surface) {
        strnfmt(error_buf, error_buf_size, "SDL_CreateSurface failed: %s",
            SDL_GetError());
        TTF_CloseFont(font);
        return NULL;
    }
    SDL_FillSurfaceRect(atlas_surface, NULL,
        SDL_MapSurfaceRGBA(atlas_surface, 0, 0, 0, 0));
    SDL_Color white = (SDL_Color){255, 255, 255, 255};
    for (Uint32 ch = 0; ch < 256; ch++) {
        SDL_Surface* glyph_surface;
        SDL_Rect dst;

        if (!sdl_mono_atlas_codepoint_visible(ch))
            continue;

        glyph_surface = TTF_RenderGlyph_Blended(font, ch, white);
        if (!glyph_surface) {
            /* Some codepoints (e.g. U+00AD soft hyphen) have no visible glyph
             * and render to zero width; newer SDL_ttf reports this as an error
             * rather than returning a blank surface. Treat that the same as the
             * zero-width-surface case below and skip the glyph instead of
             * failing the whole atlas. */
            const char* render_err = SDL_GetError();
            if (render_err && SDL_strstr(render_err, "zero width")) {
                continue;
            }
            strnfmt(error_buf, error_buf_size,
                "could not render TTF atlas glyph %u: %s",
                (unsigned)ch, render_err ? render_err : "");
            SDL_DestroySurface(atlas_surface);
            TTF_CloseFont(font);
            return NULL;
        }

        if (glyph_surface->w <= 0 || glyph_surface->h <= 0) {
            SDL_DestroySurface(glyph_surface);
            continue;
        }

        /* Keep the old mono-atlas width behaviour, but leave a tiny descent
         * guard so one-row sampling differences do not shave the bottom of
         * glyphs on small high-density panels. */
        dst.w = cell_width;
        dst.h = draw_height;
        dst.x = (int)(ch & 15) * cell_width;
        dst.y = (int)(ch >> 4) * cell_height;

        SDL_SetSurfaceBlendMode(glyph_surface, SDL_BLENDMODE_BLEND);
        if (!SDL_BlitSurfaceScaled(glyph_surface, NULL, atlas_surface, &dst,
                SDL_SCALEMODE_LINEAR))
        {
            strnfmt(error_buf, error_buf_size,
                "could not blit TTF atlas glyph %u: %s",
                (unsigned)ch, SDL_GetError());
            SDL_DestroySurface(glyph_surface);
            SDL_DestroySurface(atlas_surface);
            TTF_CloseFont(font);
            return NULL;
        }
        SDL_DestroySurface(glyph_surface);
    }

    TTF_CloseFont(font);
    if (actual_font_size)
        *actual_font_size = font_size;
    return atlas_surface;
}

SDL_Texture* sdl_texture_from_mono_atlas_surface(SDL_Surface* surface,
    char* error_buf, size_t error_buf_size)
{
    SDL_Texture* font_atlas;

    sdl_mono_atlas_set_error(error_buf, error_buf_size, "");
    if (!surface) {
        sdl_mono_atlas_set_error(error_buf, error_buf_size,
            "missing TTF glyph atlas surface");
        return NULL;
    }

    font_atlas = SDL_CreateTextureFromSurface(g_state.renderer, surface);
    if (!font_atlas) {
        strnfmt(error_buf, error_buf_size,
            "SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
        return NULL;
    }

    SDL_SetTextureBlendMode(font_atlas, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(font_atlas, SDL_SCALEMODE_NEAREST);
    return font_atlas;
}

// Loads TTF font with given size. Attempts to fit the font into a cell assming
// 1:2 aspect ratio. The font size is expected to take into account any scaling,
// either HiDPI or user. So on a HiDPI screen to use font size 12, this function
// would expect 24 given scaling factor of 2.0.
SDL_Texture* sdl_try_load_ttf_font_cells(const char* font_path,
    int cell_width, int cell_height, int* actual_font_size, char* error_buf,
    size_t error_buf_size)
{
    Uint64 start_ns = SDL_GetTicksNS();
    mono_font_style_key style = sdl_current_mono_font_style_key();
    SDL_Surface* atlas_surface;
    SDL_Texture* font_atlas;
    int font_size = 0;
    char local_error[256];

    sdl_mono_atlas_set_error(error_buf, error_buf_size, "");

    atlas_surface = sdl_build_ttf_font_atlas_surface(font_path, cell_width,
        cell_height, &style, &font_size, local_error, sizeof(local_error));
    if (!atlas_surface) {
        sdl_mono_atlas_set_error(error_buf, error_buf_size, local_error);
        return NULL;
    }

    font_atlas = sdl_texture_from_mono_atlas_surface(atlas_surface,
        local_error, sizeof(local_error));
    SDL_DestroySurface(atlas_surface);
    if (!font_atlas) {
        sdl_mono_atlas_set_error(error_buf, error_buf_size, local_error);
        return NULL;
    }

    if (actual_font_size)
        *actual_font_size = font_size;
    log_debug("Built mono font atlas %dx%d from '%s' in %llu ms",
        cell_width, cell_height, font_path ? font_path : "(null)",
        (unsigned long long)((SDL_GetTicksNS() - start_ns) / 1000000ULL));
    return font_atlas;
}

SDL_Texture* sdl_load_ttf_font_cells(const char* font_path,
    int cell_width, int cell_height, int* actual_font_size)
{
    SDL_Texture* font_atlas;
    char error[256];

    font_atlas = sdl_try_load_ttf_font_cells(font_path, cell_width,
        cell_height, actual_font_size, error, sizeof(error));
    if (!font_atlas)
    {
        log_error("could not build TTF glyph atlas: %s", error);
        quit("could not build TTF glyph atlas");
    }

    return font_atlas;
}

void sdl_window_set_position(int x, int y)
{
    if (g_state.window && x >= 0 && y >= 0) {
        SDL_SetWindowPosition(g_state.window, x, y);
        log_debug("Window position set to (%d, %d)", x, y);
    }
}

bool sdl_window_reassert_fullscreen(cptr reason)
{
    SDL_WindowFlags flags;

    if (!g_state.window || !config.fullscreen)
        return true;

    flags = SDL_GetWindowFlags(g_state.window);
    if (!(flags & SDL_WINDOW_FULLSCREEN))
    {
        log_warn("Fullscreen flag was lost during %s; restoring it",
            reason ? reason : "window update");
        if (!SDL_SetWindowFullscreen(g_state.window, true))
        {
            log_error("Failed to restore fullscreen during %s: %s",
                reason ? reason : "window update", SDL_GetError());
            return false;
        }
    }

    /* SDL fullscreen transitions can be asynchronous.  Synchronize before
     * raising so Windows recognizes this as the foreground fullscreen window
     * and places the taskbar behind it.  SDL_RaiseWindow does not leave the
     * game permanently topmost. */
    if (!SDL_SyncWindow(g_state.window))
    {
        log_warn("Fullscreen synchronization timed out during %s: %s",
            reason ? reason : "window update", SDL_GetError());
    }
    if (!SDL_RaiseWindow(g_state.window))
    {
        log_warn("Failed to raise fullscreen window during %s: %s",
            reason ? reason : "window update", SDL_GetError());
    }

    flags = SDL_GetWindowFlags(g_state.window);
    if (!(flags & SDL_WINDOW_FULLSCREEN))
    {
        log_error("Window is still not fullscreen after %s",
            reason ? reason : "window update");
        return false;
    }

    log_debug("Fullscreen synchronized and raised during %s",
        reason ? reason : "window update");
    return true;
}

void sdl_window_create(int window_width, int window_height, bool fullscreen, bool use_tiles)
{
    SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE;
    if (fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    if (!SDL_CreateWindowAndRenderer("Sil-more SDL3", window_width, window_height,
            flags, &g_state.window, &g_state.renderer))
    {
        log_error("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        quit("could not create SDL window");
    }

    if (fullscreen)
        (void)sdl_window_reassert_fullscreen("window creation");

    // Enable V-SYNC for consistent frame timing across GPU vendors.
    // Without V-SYNC, NVIDIA drivers may buffer commands and return immediately
    // from SDL_RenderPresent(), causing timing-dependent rendering issues.
    if (!SDL_SetRenderVSync(g_state.renderer, 1)) {
        log_warn("Failed to enable V-SYNC: %s", SDL_GetError());
    }

    g_state.system_scale = SDL_GetWindowDisplayScale(g_state.window);
    log_debug("window scale is %g", g_state.system_scale);
#if defined(SIL_IOS)
    sdl_ios_install_orientation_observer(g_state.window);
#endif
    sdl_refresh_safe_area();

    // Ensure predictable alpha blending (cursor/text)
    SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
    g_state.use_tiles = use_tiles;
    if (g_state.use_tiles) {
        log_debug("preparing tileset");
        if (sdl_load_tileset_texture()) {
            log_debug("tileset loaded");
        } else {
            quit("could not load tileset");
        }
    }
}

bool sdl_view_create(sdl_view* d, SDL_Rect rect, const char* font_path, int font_size, int scale, int margin)
{
    Uint64 start_ns = SDL_GetTicksNS();
    Uint64 atlas_ns;
    Uint64 canvas_ns;
    Uint8 bg_alpha;
    bool atlas_was_cached;
    log_debug("view rect=(%d %d %d %d)", rect.x, rect.y, rect.w, rect.h);

    if (scale) {
        // Integer scaling mode.
#if defined(__ANDROID__) || defined(SIL_IOS)
        int requested_scale = scale;
        SDL_Rect scale_reference;
        bool runtime_zoom = (g_main_view_zoom_scale > 0
            && scale == g_main_view_zoom_scale);
        int min_cols = runtime_zoom
            ? sdl_main_view_terminal_cols_for_map_squares(
                SDL_MAIN_VIEW_ZOOM_MIN_MAP_SQUARES)
            : sdl_current_min_terminal_cols();
        int min_rows = runtime_zoom
            ? sdl_main_view_terminal_rows_for_map_squares(
                SDL_MAIN_VIEW_ZOOM_MIN_MAP_SQUARES)
            : sdl_current_min_terminal_rows();
        int bottom_rows_for_minimum = 0;
        int max_scale_for_min_cols;
        int max_scale_for_min_rows;
        int effective_scale = requested_scale;

        scale_reference = rect;
        if (sdl_mobile_portrait_layout_active()) {
            SDL_Rect screen = sdl_get_layout_screen_rect();

            /* Orientation does not change the physical display's scale.
             * Size portrait cells from the full screen with its axes
             * reversed, not from the smaller map pane left after reserving
             * the portrait HUD. */
            if (sdl_rect_has_area(&screen))
                sdl_mobile_portrait_scale_reference_rect(&screen,
                    &scale_reference);
        }
        max_scale_for_min_cols = (scale_reference.w / min_cols) * 2
            / TILE_SIZE;
        max_scale_for_min_rows = scale_reference.h / min_rows / TILE_SIZE;

        if (!runtime_zoom) {
            bottom_rows_for_minimum =
                sdl_bottom_pane_rows_for_minimum(g_pane_rects);
            if (bottom_rows_for_minimum > 0) {
                min_rows -= bottom_rows_for_minimum;
                if (min_rows < 1)
                    min_rows = 1;
                max_scale_for_min_rows = scale_reference.h / min_rows
                    / TILE_SIZE;
            }
        }

        if (max_scale_for_min_cols < SDL_MAIN_VIEW_MIN_SCALE)
            max_scale_for_min_cols = SDL_MAIN_VIEW_MIN_SCALE;
        if (max_scale_for_min_rows < SDL_MAIN_VIEW_MIN_SCALE)
            max_scale_for_min_rows = SDL_MAIN_VIEW_MIN_SCALE;

        int max_scale_for_min_size = max_scale_for_min_cols;
        if (runtime_zoom) {
            if (max_scale_for_min_rows > max_scale_for_min_size)
                max_scale_for_min_size = max_scale_for_min_rows;
        } else if (max_scale_for_min_rows < max_scale_for_min_size) {
            max_scale_for_min_size = max_scale_for_min_rows;
        }

        if (effective_scale > max_scale_for_min_size)
            effective_scale = max_scale_for_min_size;

        d->cell_w = effective_scale * TILE_SIZE / 2;
        d->cell_h = effective_scale * TILE_SIZE;

        if (effective_scale != requested_scale) {
            if (runtime_zoom) {
                log_info("Mobile main view zoom clamped from %d to %d to keep one visible map axis >=%d squares",
                         requested_scale, effective_scale,
                         SDL_MAIN_VIEW_ZOOM_MIN_MAP_SQUARES);
            } else {
                log_info("Mobile main view scale clamped from %d to %d to keep >=%dx%d (%s)",
                         requested_scale, effective_scale, min_cols, min_rows,
                         sdl_min_terminal_mode_name(config.min_terminal_mode));
            }
        }
#else
        d->cell_w = scale * TILE_SIZE / 2;
        d->cell_h = scale * TILE_SIZE;
#endif
    } else if (font_size) {
        // Non-integer scaling mode.
        d->cell_h = g_state.system_scale * font_size;
        d->cell_w = d->cell_h / 2;
        if (d->cell_w < 1)
            d->cell_w = 1;
    } else {
        quit("sdl_view_create: font_size and scale cannot both be zero");
    }

    d->rect = rect;
    d->cols = rect.w / d->cell_w;
    if (scale)
        d->cols = sdl_main_view_logical_cols_for_visual_cols(d->cols);
    d->rows = rect.h / d->cell_h;
    /*
     * The overlay log keeps a bottom margin (painted as panel background by
     * the renderer), so size it to the whole number of rows that fit once
     * that padding is reserved.  The content stays flush with the top.
     */
    if (sdl_view_is_overlay_log_pane(d)) {
        int vmargin = pane_log_overlay_vertical_padding_px(d->cell_h);
        int avail = rect.h - vmargin;
        int config_index = sdl_pane_config_index_in_array(pane_config,
            pane_config_count, PANE_ROLLS);
        int requested_rows = (config_index >= 0)
            ? pane_config[config_index].rect.rows : 0;

        if (avail >= d->cell_h)
            d->rows = avail / d->cell_h;
        if (requested_rows > 0 && d->rows > requested_rows)
            d->rows = requested_rows;
    }
#if defined(__ANDROID__) || defined(SIL_IOS)
    if (scale) {
        log_info("Mobile main view: scale=%d cell=(%d,%d) cols=%d rows=%d (min=%dx%d %s)",
                 d->cell_h / TILE_SIZE, d->cell_w, d->cell_h, d->cols, d->rows,
                 sdl_current_min_terminal_cols(), sdl_current_min_terminal_rows(),
                 sdl_min_terminal_mode_name(config.min_terminal_mode));
    }
#endif
    /* Center the cell grid inside the view rect.
     * Do not force a minimum margin larger than the available slack; that can
     * shift the term off-center and even partially off-screen. */
    (void)margin;
    d->margin_x = (rect.w
        - sdl_main_view_visual_cols_for_width(rect.w, d->cell_w) * d->cell_w)
        / 2;
    if (d->margin_x < 0)
        d->margin_x = 0;
    d->margin_y = sdl_mobile_prefer_safe_edge_alignment()
        ? 0
        : (rect.h - d->rows * d->cell_h) / 2;
    if (d->margin_y < 0)
        d->margin_y = 0;
    log_debug("view cols=%d rows=%d cell=(%d, %d) margin=(%d, %d)",
        d->cols, d->rows, d->cell_w, d->cell_h,
        d->margin_x, d->margin_y);

    if (d->cols <= 0 || d->rows <= 0) {
        log_warn("Skipping view creation for rect=(%d %d %d %d): fits %dx%d cells at (%d,%d)",
            rect.x, rect.y, rect.w, rect.h, d->cols, d->rows, d->cell_w, d->cell_h);
        return false;
    }

    atlas_ns = SDL_GetTicksNS();
    d->font_atlas = sdl_acquire_mono_font_atlas_cells_ex(font_path, d->cell_w,
        d->cell_h, &d->font_atlas_cached, &d->font_atlas_cell_w,
        &d->font_atlas_cell_h, &d->font_atlas_exact, true);
    atlas_ns = SDL_GetTicksNS() - atlas_ns;
    atlas_was_cached = d->font_atlas_cached;
    if (!d->font_atlas) {
        log_error("Failed to acquire font atlas for rect=(%d %d %d %d)", rect.x,
            rect.y, rect.w, rect.h);
        quit("could not create font atlas");
    }
    if (!d->font_atlas_exact)
        sdl_queue_mono_font_atlas_prewarm_cells(font_path, d->cell_w,
            d->cell_h);
    SDL_SetTextureBlendMode(d->font_atlas, SDL_BLENDMODE_BLEND);
    SDL_SetTextureColorMod(d->font_atlas, 255, 255, 255);
    SDL_SetTextureAlphaMod(d->font_atlas, 255);

    // Create a persistent offscreen canvas to render into.
    canvas_ns = SDL_GetTicksNS();
    d->canvas = SDL_CreateTexture(g_state.renderer, SDL_PIXELFORMAT_RGBA8888,
                                  SDL_TEXTUREACCESS_TARGET,
                                  d->cols * d->cell_w, d->rows * d->cell_h);
    if (d->canvas) {
        log_debug("view canvas %dx%d", d->canvas->w, d->canvas->h);
        bg_alpha = sdl_view_background_alpha(d);
        SDL_SetTextureBlendMode(d->canvas,
            (bg_alpha < SDL_ALPHA_OPAQUE) ? SDL_BLENDMODE_BLEND
                                          : SDL_BLENDMODE_NONE);
        SDL_SetTextureScaleMode(d->canvas, SDL_SCALEMODE_NEAREST);
        SDL_SetRenderTarget(g_state.renderer, d->canvas);
        SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(g_state.renderer, 0, 0, 0, bg_alpha);
        SDL_RenderClear(g_state.renderer);
        SDL_SetRenderDrawBlendMode(g_state.renderer, SDL_BLENDMODE_BLEND);
        canvas_ns = SDL_GetTicksNS() - canvas_ns;
        g_state.need_present = true;
    } else {
        log_error("Create canvas failed: %s", SDL_GetError());
        quit("could not create canvas");
    }

    log_debug("view create completed in %llu ms (rect=%dx%d cell=%dx%d cols=%d rows=%d atlas=%llu ms %s %s atlas_cell=%dx%d canvas=%llu ms)",
        (unsigned long long)((SDL_GetTicksNS() - start_ns) / 1000000ULL),
        rect.w, rect.h, d->cell_w, d->cell_h, d->cols, d->rows,
        (unsigned long long)(atlas_ns / 1000000ULL),
        atlas_was_cached ? "cached" : "uncached",
        d->font_atlas_exact ? "exact" : "fallback",
        d->font_atlas_cell_w, d->font_atlas_cell_h,
        (unsigned long long)(canvas_ns / 1000000ULL));

    return true;
}

/*
 * Load a TTF font with fallback to default if not found
 */
TTF_Font* sdl_load_font_with_fallback(const char* font_path, int font_size, const char* fallback_path)
{
    TTF_Font* font = NULL;
    
    // Try to load the specified font if provided
    if (font_path && font_path[0] != '\0') {
        font = TTF_OpenFont(font_path, font_size);
        if (font) {
            log_debug("Loaded custom font: %s at size %d", font_path, font_size);
            // Apply story font settings
            sdl_apply_font_settings(font, true);
            return font;
        } else {
            log_warn("Failed to load custom font '%s': %s", font_path, SDL_GetError());
        }
    }
    
    // Fall back to default font
    font = TTF_OpenFont(fallback_path, font_size);
    if (font) {
        log_debug("Using fallback font: %s at size %d", fallback_path, font_size);
        // Apply story font settings to fallback too
        sdl_apply_font_settings(font, true);
    } else {
        log_error("Failed to load fallback font '%s': %s", fallback_path, SDL_GetError());
    }
    
    return font;
}

/*
 * Load story fonts from configuration for main/aux view sizes.
 * If no story font is set in the SDL settings (or the SDL JSON is missing),
 * fall back to MarcellusSC-Regular.ttf located in lib/xtra/font.
 */
void sdl_load_story_fonts(void)
{
    int main_cell_h = sdl_current_main_view_scale() * TILE_SIZE;
    int pane_cell_widths[PANE_MAX] = { 0 };
    int pane_cell_heights[PANE_MAX] = { 0 };
    
    log_info("Loading story fonts...");
    log_debug("Story font config: '%s'", config.story_font[0] != '\0' ? config.story_font : "(not set)");
    log_debug("Story font sizes: main=%d default-aux=%d", main_cell_h,
        (int)(g_state.system_scale * sdl_resolve_aux_view_font_size(config.aux_view_font_size)));
    
    if (!sdl_story_font_cache_matches_config()) {
        sdl_story_font_cache_clear();
        sdl_story_font_cache_mark_config();
    }

    for (int slot = 0; slot < STORY_FONT_SLOTS; slot++)
        (void)sdl_story_font_for_height_slot(main_cell_h, slot);
    sdl_build_supporting_pane_metrics(pane_config, pane_config_count,
        pane_cell_widths, pane_cell_heights);
    for (int i = 1; i < PANE_MAX; i++) {
        if (pane_cell_heights[i] > 0) {
            for (int slot = 0; slot < STORY_FONT_SLOTS; slot++)
                (void)sdl_story_font_for_height_slot(pane_cell_heights[i], slot);
        }
    }
    
    // Initialize flag to false
    g_state.story_font_depth = 0;
    if (Term)
    {
        Term->story_font_active = false;
    }
    
    log_info("Story fonts loaded successfully");
}

bool sdl_mono_font_prewarm_request_exists(const char* font_path,
    int cell_width, int cell_height)
{
    const mono_font_prewarm_job* job = &g_mono_font_prewarm_job;

    if (job->thread
        && job->req.cell_width == cell_width
        && job->req.cell_height == cell_height
        && streq(job->req.font_path, font_path))
    {
        return true;
    }

    for (int i = 0; i < g_mono_font_prewarm_count; i++) {
        const mono_font_prewarm_request* req = &g_mono_font_prewarm_queue[i];

        if (req->cell_width == cell_width && req->cell_height == cell_height
            && streq(req->font_path, font_path))
        {
            return true;
        }
    }

    return false;
}

void sdl_queue_mono_font_atlas_prewarm_cells(const char* font_path,
    int cell_width, int cell_height)
{
    mono_font_prewarm_request* req;

    if (!font_path || !font_path[0] || cell_width <= 0 || cell_height <= 0)
        return;
    if (sdl_mono_font_atlas_cached_cells(font_path, cell_width, cell_height))
        return;
    if (sdl_mono_font_prewarm_request_exists(font_path, cell_width,
            cell_height))
    {
        return;
    }
    if (g_mono_font_prewarm_count >= SDL_MONO_FONT_PREWARM_QUEUE_MAX) {
        log_debug("Mono font prewarm queue full; skipping cell %dx%d",
            cell_width, cell_height);
        return;
    }

    req = &g_mono_font_prewarm_queue[g_mono_font_prewarm_count++];
    SDL_strlcpy(req->font_path, font_path, sizeof(req->font_path));
    req->cell_width = cell_width;
    req->cell_height = cell_height;
}

void sdl_queue_main_view_scale_prewarm_main_only(int scale)
{
    const char* font_path = sdl_monospace_font_path();

    if (scale <= 0)
        return;

    sdl_queue_mono_font_atlas_prewarm_cells(font_path,
        scale * TILE_SIZE / 2, scale * TILE_SIZE);
}

#if !SIL_SDL_MOBILE_BUILD
void sdl_queue_main_view_scale_prewarm_panes(int scale)
{
    const char* font_path = sdl_monospace_font_path();
    int saved_aux_override = g_auto_aux_main_cell_h_override;
    int cell_widths[PANE_MAX] = { 0 };
    int cell_heights[PANE_MAX] = { 0 };

    if (scale <= 0)
        return;

    g_auto_aux_main_cell_h_override = scale * TILE_SIZE;
    sdl_build_supporting_pane_metrics(pane_config, pane_config_count,
        cell_widths, cell_heights);
    g_auto_aux_main_cell_h_override = saved_aux_override;

    for (int i = 1; i < PANE_MAX; i++) {
        if (cell_widths[i] > 0 && cell_heights[i] > 0)
            sdl_queue_mono_font_atlas_prewarm_cells(font_path, cell_widths[i],
                cell_heights[i]);
    }
}
#endif

void sdl_queue_main_view_scale_neighbors_prewarm(const char* reason)
{
    SDL_Rect screen;
    int current_scale;
    int min_scale;
    int max_scale;
    int candidates[SDL_MAIN_VIEW_SCALE_PREWARM_RADIUS * 2];
    int candidate_count = 0;
    int old_count;

    if (!g_state.window || !g_state.renderer)
        return;

    sdl_mono_font_prewarm_queue_clear();
    sdl_refresh_safe_area();
    screen = sdl_get_layout_screen_rect();
    if (!sdl_rect_has_area(&screen))
        return;

    current_scale = sdl_current_main_view_scale();
    min_scale = sdl_min_main_view_zoom_scale_for_layout(&screen,
        config.min_terminal_mode);
    max_scale = sdl_max_main_view_zoom_scale_for_layout(&screen,
        config.min_terminal_mode);

    if (min_scale < SDL_MAIN_VIEW_MIN_SCALE)
        min_scale = SDL_MAIN_VIEW_MIN_SCALE;
    if (max_scale < min_scale)
        max_scale = min_scale;

    for (int step = 1; step <= SDL_MAIN_VIEW_SCALE_PREWARM_RADIUS; step++) {
        int larger = current_scale + step;
        int smaller = current_scale - step;

        if (larger <= max_scale)
            candidates[candidate_count++] = larger;
        if (smaller >= min_scale)
            candidates[candidate_count++] = smaller;
    }

    old_count = g_mono_font_prewarm_count;
    sdl_queue_main_view_scale_prewarm_main_only(current_scale);
    for (int i = 0; i < candidate_count; i++)
        sdl_queue_main_view_scale_prewarm_main_only(candidates[i]);
#if !SIL_SDL_MOBILE_BUILD
    for (int i = 0; i < candidate_count; i++)
        sdl_queue_main_view_scale_prewarm_panes(candidates[i]);
#endif

    if (g_mono_font_prewarm_count > old_count) {
        log_debug("%s: queued %d mono font atlas prewarm request(s) around scale %d",
            reason ? reason : "scale prewarm",
            g_mono_font_prewarm_count - old_count, current_scale);
    }
}

bool sdl_mono_font_prewarm_pop_request(mono_font_prewarm_request* out)
{
    if (g_mono_font_prewarm_count <= 0)
        return false;

    if (out)
        *out = g_mono_font_prewarm_queue[0];
    g_mono_font_prewarm_count--;
    if (g_mono_font_prewarm_count > 0) {
        memmove(&g_mono_font_prewarm_queue[0], &g_mono_font_prewarm_queue[1],
            sizeof(g_mono_font_prewarm_queue[0])
                * (size_t)g_mono_font_prewarm_count);
    }

    return true;
}

int SDLCALL sdl_mono_font_prewarm_thread(void* data)
{
    mono_font_prewarm_job* job = (mono_font_prewarm_job*)data;
    SDL_Surface* surface;
    int actual_font_size = 0;
    Uint64 start_ns;
    Uint64 build_ns;
    char error[256];

    if (!job || !job->mutex)
        return 1;

    start_ns = SDL_GetTicksNS();
    surface = sdl_build_ttf_font_atlas_surface(job->req.font_path,
        job->req.cell_width, job->req.cell_height, &job->style,
        &actual_font_size, error, sizeof(error));
    build_ns = SDL_GetTicksNS() - start_ns;

    SDL_LockMutex(job->mutex);
    job->surface = surface;
    job->actual_font_size = actual_font_size;
    job->build_ns = build_ns;
    job->success = (surface != NULL);
    SDL_strlcpy(job->error, job->success ? "" : error, sizeof(job->error));
    job->done = true;
    SDL_UnlockMutex(job->mutex);
    return job->success ? 0 : 1;
}

void sdl_mono_font_prewarm_job_reset(void)
{
    memset(&g_mono_font_prewarm_job, 0, sizeof(g_mono_font_prewarm_job));
}

void sdl_mono_font_prewarm_job_shutdown(void)
{
    mono_font_prewarm_job* job = &g_mono_font_prewarm_job;

    if (job->thread) {
        SDL_WaitThread(job->thread, NULL);
        job->thread = NULL;
    }
    if (job->surface) {
        SDL_DestroySurface(job->surface);
        job->surface = NULL;
    }
    if (job->mutex) {
        SDL_DestroyMutex(job->mutex);
        job->mutex = NULL;
    }
    sdl_mono_font_prewarm_job_reset();
}

static void sdl_mono_font_prewarm_adopt_exact(
    const mono_font_prewarm_request* req, const mono_font_style_key* style,
    SDL_Texture* atlas)
{
    mono_font_style_key current_style = sdl_current_mono_font_style_key();
    term* old_term;
    bool redraw_started = false;
    bool active_consumer = false;

    if (!req || !style || !atlas
        || !streq(req->font_path, sdl_monospace_font_path())
        || !sdl_mono_font_style_keys_equal(style, &current_style))
    {
        return;
    }

    old_term = Term;
    for (int i = 0; i < MAX_TERM_DATA; i++) {
        sdl_view* view = &g_views[i];

        if (!view->term_ready || view->cell_w != req->cell_width
            || view->cell_h != req->cell_height)
        {
            continue;
        }
        active_consumer = true;
        if (view->font_atlas == atlas && view->font_atlas_exact)
            continue;
        if (view->font_atlas && !view->font_atlas_cached)
            SDL_DestroyTexture(view->font_atlas);
        view->font_atlas = atlas;
        view->font_atlas_cached = true;
        view->font_atlas_exact = true;
        view->font_atlas_cell_w = req->cell_width;
        view->font_atlas_cell_h = req->cell_height;
        SDL_SetTextureBlendMode(atlas, SDL_BLENDMODE_BLEND);
        SDL_SetTextureColorMod(atlas, 255, 255, 255);
        SDL_SetTextureAlphaMod(atlas, 255);

        if (!redraw_started) {
            sdl_present_batch_begin();
            redraw_started = true;
        }
        Term_activate(&view->t);
        Term_redraw();
    }

    /* The styled left panel and combat overlay acquire atlases directly
     * rather than owning a view pointer.  Wake one frame when this exact size
     * is currently visible; the per-size atlas generation invalidates only
     * the matching retained panel. */
    if (g_views[PANE_MAIN].term_ready) {
        sdl_left_panel_metrics metrics;
        SDL_Rect combat_rect;

        if (sdl_left_panel_metrics_for_view(&g_views[PANE_MAIN], &metrics)
            && metrics.cell_w == req->cell_width
            && metrics.cell_h == req->cell_height)
        {
            active_consumer = true;
        }
        if (sdl_combat_overlay_pane_content_rect(&combat_rect)) {
            int cell_h = sdl_effective_pane_cell_height_for_type(PANE_COMBAT);
            int cell_w;

            if (cell_h < 1)
                cell_h = 1;
            cell_w = MAX(1, cell_h / 2);
            if (cell_w == req->cell_width && cell_h == req->cell_height)
                active_consumer = true;
        }
    }

    if (old_term)
        Term_activate(old_term);
    if (active_consumer)
        g_state.need_present = true;
    if (redraw_started)
        sdl_present_batch_end();
}

bool sdl_mono_font_prewarm_finish_ready(void)
{
    mono_font_prewarm_job* job = &g_mono_font_prewarm_job;
    mono_font_prewarm_request req;
    mono_font_style_key style;
    SDL_Surface* surface = NULL;
    SDL_Texture* atlas = NULL;
    Uint64 build_ns = 0;
    bool success = false;
    bool done = false;
    char error[256];

    if (!job->thread || !job->mutex)
        return false;

    SDL_LockMutex(job->mutex);
    done = job->done;
    if (done) {
        req = job->req;
        style = job->style;
        surface = job->surface;
        job->surface = NULL;
        build_ns = job->build_ns;
        success = job->success;
        SDL_strlcpy(error, job->error, sizeof(error));
    }
    SDL_UnlockMutex(job->mutex);

    if (!done)
        return false;

    SDL_WaitThread(job->thread, NULL);
    SDL_DestroyMutex(job->mutex);
    job->thread = NULL;
    job->mutex = NULL;

    if (success && surface
        && !sdl_mono_font_atlas_cached_cells_style(req.font_path,
            req.cell_width, req.cell_height, &style)
        && sdl_mono_font_atlas_cache_has_free_slot())
    {
        char texture_error[256];

        atlas = sdl_texture_from_mono_atlas_surface(surface, texture_error,
            sizeof(texture_error));
        if (atlas) {
            if (!sdl_store_mono_font_atlas_cells(req.font_path,
                    req.cell_width, req.cell_height, atlas, &style))
            {
                SDL_DestroyTexture(atlas);
                atlas = NULL;
            }
        } else {
            log_warn("Prewarm upload failed for mono font atlas %dx%d: %s",
                req.cell_width, req.cell_height, texture_error);
        }
    }

    if (surface)
        SDL_DestroySurface(surface);

    if (success && atlas) {
        sdl_mono_font_prewarm_adopt_exact(&req, &style, atlas);
        log_debug("Prewarmed mono font atlas %dx%d asynchronously in %llu ms",
            req.cell_width, req.cell_height,
            (unsigned long long)(build_ns / 1000000ULL));
    } else if (!success) {
        log_warn("Prewarm failed for mono font atlas %dx%d: %s",
            req.cell_width, req.cell_height, error);
    }

    sdl_mono_font_prewarm_job_reset();
    return true;
}

bool sdl_mono_font_prewarm_start_next(void)
{
    mono_font_prewarm_request req;
    mono_font_prewarm_job* job = &g_mono_font_prewarm_job;

    if (job->thread)
        return false;
    if (!g_state.renderer)
        return false;

    while (sdl_mono_font_prewarm_pop_request(&req)) {
        if (sdl_mono_font_atlas_cached_cells(req.font_path, req.cell_width,
                req.cell_height))
        {
            continue;
        }
        if (!sdl_mono_font_atlas_cache_has_free_slot()) {
            log_debug("Mono font atlas cache full; skipping prewarm for %dx%d",
                req.cell_width, req.cell_height);
            continue;
        }

        memset(job, 0, sizeof(*job));
        job->req = req;
        job->style = sdl_current_mono_font_style_key();
        job->mutex = SDL_CreateMutex();
        if (!job->mutex) {
            log_warn("Could not create mono font prewarm mutex: %s",
                SDL_GetError());
            sdl_mono_font_prewarm_job_reset();
            return false;
        }
        job->thread = SDL_CreateThread(sdl_mono_font_prewarm_thread,
            "mono-atlas-prewarm", job);
        if (!job->thread) {
            log_warn("Could not start mono font prewarm thread: %s",
                SDL_GetError());
            SDL_DestroyMutex(job->mutex);
            sdl_mono_font_prewarm_job_reset();
            return false;
        }
        log_debug("Started async mono font atlas prewarm %dx%d",
            req.cell_width, req.cell_height);
        return true;
    }

    return false;
}

void sdl_mono_font_prewarm_process_idle(void)
{
    int processed = 0;

    /* Never upload or start an atlas while player input is already queued. */
    if (SDL_PollEvent(NULL))
        return;

    if (sdl_mono_font_prewarm_finish_ready())
        processed++;

    while (!g_mono_font_prewarm_job.thread
        && g_mono_font_prewarm_count > 0
        && processed < SDL_MONO_FONT_PREWARM_IDLE_BATCH)
    {
        if (SDL_PollEvent(NULL))
            break;
        if (!sdl_mono_font_prewarm_start_next())
            break;
        processed++;
    }
}

/*
 * Enable story font mode - subsequent text will use custom font
 */
void sdl_story_font_enable(void)
{
    g_state.story_font_depth++;
    if (g_state.story_font_depth == 1)
        sdl_apply_story_font_state(true);
    log_trace("Story font ENABLED (depth=%d)", g_state.story_font_depth);
}

/*
 * Disable story font mode - subsequent text will use monospace font
 */
void sdl_story_font_disable(void)
{
    if (g_state.story_font_depth > 0)
        g_state.story_font_depth--;
    bool active = (g_state.story_font_depth > 0);
    sdl_apply_story_font_state(active);
    if (!active) {
        sdl_story_font_set_grid(false);
        sdl_story_font_set_slot(0);
    }
    log_trace("Story font DISABLED (depth=%d)", g_state.story_font_depth);
}

/*
 * Check if story font is currently enabled
 */
bool sdl_is_story_font_enabled(void)
{
    return (Term && Term->story_font_active);
}

void sdl_story_font_set_grid(bool grid)
{
    if (g_state.story_font_grid == grid)
        return;
    g_state.story_font_grid = grid;
    log_trace("Story font grid %s", grid ? "ENABLED" : "DISABLED");
    sdl_apply_story_grid_state(grid);
}

bool sdl_is_story_font_grid(void)
{
    return (Term && Term->story_font_grid);
}

void sdl_story_font_set_slot(int slot)
{
    if (g_state.story_font_slot == slot)
        return;
    g_state.story_font_slot = slot;
    log_trace("Story font slot %d", slot);
    sdl_apply_story_slot_state(slot);
}

void sdl_story_font_reset(void)
{
    sdl_story_font_reset_state();
}

/*
 * Get the pixel width of text when rendered with the story font.
 * Returns 0 if story font is not available.
 * IMPORTANT: This returns the width AFTER scaling to match cell height.
 */
int sdl_story_font_text_width(cptr text, int len)
{
    if (!text)
        return 0;
    if (len <= 0)
        return 0;

    len = utf8_safe_prefix_len(text, len);
    if (len <= 0)
        return 0;

    sdl_view* d = NULL;
    if (Term)
        d = sdl_view_from_term(Term);
    if (!d || !d->term_ready) {
        if (g_views[0].term_ready)
            d = &g_views[0];
    }
    if (!d)
        return 0;

    int slot = Term ? Term->story_font_slot : SDL_STORY_FONT_SLOT_DEFAULT;
    TTF_Font* font = sdl_story_font_for_view_slot(d, slot);
    if (!font)
        return 0;

    /* Measure the text width using SDL_ttf (unscaled) */
    int w = 0;
    TTF_MeasureString(font, text, len, 0, &w, NULL);

    /* Apply the same scaling that's used when rendering */
    int font_h = TTF_GetFontHeight(font);
    if (font_h > 0) {
        float cell_h_f = (float)d->cell_h;
        float surf_h_f = (float)font_h;
        float scale = cell_h_f / surf_h_f;
        w = (int)((float)w * scale);
    }

    return w;
}

/*
 * Get the cell width in pixels for the main terminal view.
 * This is used to convert terminal columns to pixel width.
 */
int sdl_get_cell_width(void)
{
    if (g_views[0].term_ready) {
        return g_views[0].cell_w;
    }
    return 8; /* fallback */
}

int sdl_main_view_visible_col0(void)
{
    if (sdl_left_panel_pane_presentation_active())
        return COL_MAP;

    return 0;
}

int sdl_main_view_visible_cols(void)
{
    int cols;

    if (!g_views[PANE_MAIN].term_ready)
        return 0;

    cols = sdl_main_view_visual_cols(&g_views[PANE_MAIN]);
    if (cols < 0)
        cols = 0;

    return cols;
}

// Quit hook to save window configuration on exit
