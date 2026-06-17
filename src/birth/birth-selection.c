/* File: birth/birth-selection.c */

#include "angband.h"
#include "birth/birth-internal.h"

/*
 * Peoples grouping for the race-selection screen (the "book page").
 *
 * The three Noldorin lineages (separate p_info races sharing the Noldorin
 * stats but with different affinities) are grouped under a Noldor heading;
 * the other peoples follow.  All are directly selectable and each maps to a
 * concrete p_info[] race that ends up in p_ptr->prace.
 *
 * Race indices match lib/edit/race.txt:
 *   0 Feanorians  1 Fingolfinrim  2 Finarfinrim  3 Sindar  4 Naugrim  5 Edain
 * A runtime check (birth_peoples_validate) guards against a future reorder.
 */
typedef struct birth_people {
    cptr name;
    int races[3];
    int race_count;
    cptr lore;
} birth_people;

/*
 * Race-screen text, in three voices.  birth_frame_top and birth_frame_bottom
 * are the second-person "trial" frame (rendered in an accent colour) that ties
 * the choice to the metarun storyline from print_story_intro -- a nameless
 * spirit who borrows the names of the dead to descend into Angband.  Between
 * them, birth_intro_lore is the chronicle of the war (rendered in white).
 * Blank lines (\n\n) separate paragraphs.
 */
static const char birth_frame_top[] =
    "You wake again in the dark, nameless and unremembered \xe2\x80\x94 a "
    "spirit set by the Valar to their long trial: to live the War of the "
    "Jewels once more, in a shape that is not your own.";

static const char birth_intro_lore[] =
    "In the Elder Days, when the Two Trees of Valinor were slain and their "
    "light failed, Morgoth the great Enemy stole the Silmarils \xe2\x80\x94 the "
    "three holy jewels wrought by F\xc3\xab" "anor, in which that light yet lived "
    "\xe2\x80\x94 and fled north to Angband, his fortress of iron beneath the "
    "triple peaks of Thangorodrim. There he set the Jewels in an iron crown "
    "and sat enthroned in darkness, walled about by orcs, balrogs, and "
    "dragons.\n\n"
    "For the Silmarils F\xc3\xab" "anor swore his terrible Oath, and the Noldor "
    "forsook the Blessed Realm and came in exile to Beleriand to make war upon "
    "the Enemy. They raised proud kingdoms and held Angband under siege "
    "through long years \xe2\x80\x94 yet their valour was ever shadowed by the "
    "Kinslaying at Alqualond\xc3\xab and the Doom of Mandos that followed "
    "them.\n\n"
    "They did not fight alone. The Sindar, Grey-elves who dwelt beneath the "
    "stars of Beleriand long before the Noldor came, took up arms in the Elven "
    "realms and at the Havens of the Falas \xe2\x80\x94 though hidden Doriath, "
    "girdled by the power of Melian, held apart behind its enchanted bounds. "
    "The Naugrim, Dwarves of the Blue Mountains, forged matchless mail and "
    "axes and marched to the great battles. And the Edain, the first Men over "
    "the mountains, swore friendship to the Eldar and spent their brief, "
    "valiant lives in the war, winning undying renown.";

static const char birth_frame_bottom[] =
    "Choose now whose name you will wear \xe2\x80\x94 whose courage, whose "
    "grief, whose doom you carry down into Angband. Each Silmaril wrested from "
    "the iron crown brightens the Valar's hope, though your spirit thins; and "
    "in the borrowed glory of another you may, at last, remember your own "
    "forgotten name.";

static const birth_people birth_peoples[] = {
    { "Noldor", { 0, 1, 2 }, 3,
        "The Noldor are the High Elves of the West, deep in lore and craft, "
        "who dwelt in the light of Valinor. For love of the stolen Silmarils "
        "-- and, for some, bound by the dreadful Oath of Feanor -- they "
        "returned to Middle-earth in exile to make war upon Morgoth. Proud, "
        "mighty, and gifted, they raised shining kingdoms and forged wondrous "
        "things, yet a doom of sorrow and kinstrife shadows their valour. "
        "Their lineages descend from three lords: Feanor, Fingolfin, and "
        "Finarfin." },
    { "Sindar", { 3 }, 1,
        "The Sindar are the Grey-elves of Beleriand, kindred of the Eldar who "
        "never crossed the Sea but lingered under the stars of Middle-earth. "
        "Masters of song, woodcraft, and secret ways, they ruled the woven "
        "realm of Doriath and the havens of the Falas. Theirs is a quieter "
        "wisdom than the Noldor's, deep-rooted and enduring, though wars not "
        "of their making press hard upon their twilight land." },
    { "Naugrim", { 4 }, 1,
        "The Naugrim, the Dwarves, are the hardy smith-folk who dwell in "
        "halls of stone beneath the mountains. Strong and stubborn, secretive "
        "yet steadfast, they are unmatched in the working of metal and gem. "
        "Their axes are feared and their mail is peerless; greed and grievance "
        "can stir them, but a Dwarf-friend has no truer ally in the long war "
        "against the Shadow." },
    { "Edain", { 5 }, 1,
        "The Edain are the Men of the Three Houses, mortal and short-lived, "
        "who came over the mountains into Beleriand and allied themselves with "
        "the Eldar against Morgoth. Brief their years, but bright their "
        "valour; their deeds outrun their span and live on in song. Bound to "
        "the Elves by love and sorrow, they carry hope into the dark beyond "
        "the reach of the Eldar's fading." },
};
#define BIRTH_PEOPLE_COUNT ((int)N_ELEMENTS(birth_peoples))

/*
 * Verify the hardcoded race indices still match race.txt by name.  Returns
 * true when sane; logs and returns false if the data has been reordered.
 */
static bool birth_peoples_validate(void)
{
    for (int p = 0; p < BIRTH_PEOPLE_COUNT; p++)
    {
        for (int r = 0; r < birth_peoples[p].race_count; r++)
        {
            int race = birth_peoples[p].races[r];

            if (race < 0 || race >= z_info->p_max)
                return false;
        }
    }
    return true;
}

/*
 * Emit the detail-panel lines (stat adjustments, then affinities/traits) for a
 * race (and optionally a character) to the SDL selection screen.  Pass
 * character < 0 for race/lineage screens (uses the Houseless baseline so only
 * racial affinities show); affinities_vary suppresses the affinity list for
 * the multi-lineage Noldor people, where it differs per lineage.
 */
static void birth_format_character_power(byte power, bool leading_space,
    char* stars, size_t stars_len, byte* attr, cptr* label)
{
    cptr star_text = "**";
    byte star_attr = TERM_WHITE;
    cptr power_label = "Fair";

    switch (power)
    {
    case 0:
        star_text = "*";
        star_attr = TERM_RED;
        power_label = "Weak";
        break;
    case 1:
        star_text = "**";
        star_attr = TERM_WHITE;
        power_label = "Fair";
        break;
    case 2:
        star_text = "***";
        star_attr = TERM_GREEN;
        power_label = "Strong";
        break;
    case 3:
    case 4:
        star_text = "***";
        star_attr = TERM_L_GREEN;
        power_label = "Mighty";
        break;
    default:
        break;
    }

    if (stars && stars_len > 0)
        strnfmt(stars, stars_len, "%s%s", leading_space ? " " : "",
            star_text);
    if (attr)
        *attr = star_attr;
    if (label)
        *label = power_label;
}

#if !defined(__ANDROID__) && !defined(SIL_IOS)
static void birth_count_alive_character_powers(int power_counts[4])
{
    int i;

    if (!power_counts)
        return;
    for (i = 0; i < 4; i++)
        power_counts[i] = 0;
    if (!z_info || !c_info || !c_name)
        return;

    for (i = 0; i < z_info->c_max; i++)
    {
        byte power;

        if (highscore_dead(c_name + c_info[i].name))
            continue;

        power = c_info[i].power;
        if (power == 4)
            power_counts[3]++;
        else if (power <= 3)
            power_counts[power]++;
    }
}
#endif /* !__ANDROID__ && !SIL_IOS (only used by the desktop power summary) */

static void birth_select_emit_detail(int race, int character, bool affinities_vary)
{
    char line[128];
    char hint[256];
    int i;

    if (race < 0 || race >= z_info->p_max)
        return;

    for (i = 0; i < A_MAX; i++)
    {
        char name[32];
        int len;
        int adj;
        byte attr;

        SDL_strlcpy(name, stat_names[i] ? stat_names[i] : "", sizeof(name));
        len = (int)strlen(name);
        while (len > 0 && name[len - 1] == ' ')
            name[--len] = '\0';

        if (character >= 0)
            adj = c_info[character].h_adj[i] + p_info[race].r_adj[i]
                + curses_stat_adj(i);
        else
            adj = p_info[race].r_adj[i];

        if (adj < 0)            attr = TERM_RED;
        else if (adj == 0)      attr = TERM_L_DARK;
        else if (adj == 1)      attr = TERM_GREEN;
        else if (adj == 2)      attr = TERM_L_GREEN;
        else                    attr = TERM_L_BLUE;

        strnfmt(line, sizeof(line), "%s\t%+d", name, adj);
        hint[0] = '\0';
        character_sheet_format_stat_hint(i, adj, true, hint, sizeof(hint));
        sdl_character_sheet_screen_add_select_detail(line, attr, hint);
    }

    if (character >= 0)
    {
        char pretty_name[40];
        char title_stars[32];
        char stars[16];
        byte star_attr;
#if defined(__ANDROID__) || defined(SIL_IOS)
        cptr power_label = NULL;
#endif
#if !defined(__ANDROID__) && !defined(SIL_IOS)
        int power_counts[4];

        birth_count_alive_character_powers(power_counts);

        sdl_character_sheet_screen_begin_select_rating_summary("Heroes Power");
        birth_format_character_power(3, false, stars, sizeof(stars),
            &star_attr, NULL);
        sdl_character_sheet_screen_add_select_rating("Mighty", stars,
            power_counts[3], star_attr, "Mighty heroes still alive.");
        birth_format_character_power(2, false, stars, sizeof(stars),
            &star_attr, NULL);
        sdl_character_sheet_screen_add_select_rating("Strong", stars,
            power_counts[2], star_attr, "Strong heroes still alive.");
        birth_format_character_power(1, false, stars, sizeof(stars),
            &star_attr, NULL);
        sdl_character_sheet_screen_add_select_rating("Fair", stars,
            power_counts[1], star_attr, "Fair heroes still alive.");
        birth_format_character_power(0, false, stars, sizeof(stars),
            &star_attr, NULL);
        sdl_character_sheet_screen_add_select_rating("Weak", stars,
            power_counts[0], star_attr, "Weak heroes still alive.");
#endif

        strnfmt(pretty_name, sizeof(pretty_name), "%s%s",
            c_name + c_info[character].name,
            c_name + c_info[character].alt_name);

#if defined(__ANDROID__) || defined(SIL_IOS)
        birth_format_character_power(c_info[character].power, true,
            stars, sizeof(stars), &star_attr, &power_label);
        if (power_label && power_label[0])
        {
            char power_word[16];

            SDL_strlcpy(power_word, power_label, sizeof(power_word));
            power_word[0] = (char)tolower((unsigned char)power_word[0]);
            strnfmt(title_stars, sizeof(title_stars), "%s %s", stars,
                power_word);
        }
        else
        {
            SDL_strlcpy(title_stars, stars, sizeof(title_stars));
        }
#else
        birth_format_character_power(c_info[character].power, true,
            title_stars, sizeof(title_stars), &star_attr, NULL);
#endif

        sdl_character_sheet_screen_set_select_title_detail(pretty_name,
            title_stars, star_attr);
    }

    if (affinities_vary)
    {
        sdl_character_sheet_screen_add_select_detail(
            "Affinities vary by lineage", TERM_SLATE,
            "Each Noldorin lineage has its own skill affinities; choose a "
            "lineage to see them.");
        return;
    }

    {
        birth_compact_flag_line traits[48];
        int n = collect_character_trait_lines(race,
            (character >= 0) ? character : 0, false, traits,
            (int)N_ELEMENTS(traits), NULL);

        for (i = 0; i < n; i++)
            if (traits[i].txt && traits[i].txt[0])
            {
                hint[0] = '\0';
                birth_format_trait_hint(&traits[i], hint, sizeof(hint));
                sdl_character_sheet_screen_add_select_detail(traits[i].txt,
                    traits[i].attr, hint);
            }
    }
}

/*
 * Display additional information about each race during the selection.
 */
static void race_aux_hook(birth_menu r_str)
{
    int race, i, adj;
    char s[50];
    byte attr;

    /* Extract the proper race index from the string. */
    for (race = 0; race < z_info->p_max; race++)
    {
        if (!strcmp(r_str.name, p_name + p_info[race].name))
            break;
    }

    if (race == z_info->p_max)
        return;

    /* Pixel-semantic selection screen: feed the detail panel and stop (no
     * terminal grid underdraw). */
    if (sdl_character_sheet_screen_active())
    {
        birth_select_emit_detail(race, -1, false);
        return;
    }

    /* Display the stats */
    for (i = 0; i < A_MAX; i++)
    {
        /*dump the stats*/
        strnfmt(s, sizeof(s), "%s", stat_names[i]);
        Term_putstr(RACE_AUX_COL, TABLE_ROW + i, -1, TERM_WHITE, s);

        adj = p_info[race].r_adj[i];
        strnfmt(s, sizeof(s), "%+d", adj);

        if (adj < 0)
            attr = TERM_RED;
        else if (adj == 0)
            attr = TERM_L_DARK;
        else if (adj == 1)
            attr = TERM_GREEN;
        else if (adj == 2)
            attr = TERM_L_GREEN;
        else
            attr = TERM_L_BLUE;

        Term_putstr(RACE_AUX_COL + 4, TABLE_ROW + i, -1, attr, s);
        {
            char desc[256];

            character_sheet_format_stat_hint(i, adj, true, desc,
                sizeof(desc));
            birth_detail_hover_add(RACE_AUX_COL, TABLE_ROW + i, 8, desc);
        }
    }

    /* Display the race flags */

    Term_putstr(RACE_AUX_COL, TABLE_ROW + A_MAX + 1, -1, TERM_WHITE,
        "                         ");
    Term_putstr(RACE_AUX_COL, TABLE_ROW + A_MAX + 2, -1, TERM_WHITE,
        "                         ");
    Term_putstr(RACE_AUX_COL, TABLE_ROW + A_MAX + 3, -1, TERM_WHITE,
        "                         ");
    Term_putstr(RACE_AUX_COL, TABLE_ROW + A_MAX + 4, -1, TERM_WHITE,
        "                        ");

    /* Clear the TOTAL_AUX_COL area (where character info was displayed) */
    Term_putstr(TOTAL_AUX_COL, HEADER_ROW, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 1, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 2, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 3, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 4, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 5, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 6, -1, TERM_WHITE,
        "                                         ");
    Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX + 7, -1, TERM_WHITE,
        "                                         ");

    // print_rh_flags(race, 0, RACE_AUX_COL, TABLE_ROW + A_MAX + 1);
}

/*
 * Player race (screen 1): a story/explanation "book page".  All peoples are
 * directly selectable in one grouped list -- the three Noldorin lineages under
 * a Noldor heading, then the other peoples -- with the setting lore on top and
 * the highlighted people's description at the bottom.  No stats or affinities;
 * just story.  Resolves a concrete p_info race into p_ptr->prace.
 */
static bool get_player_race(void)
{
    int i;
    int race;
    int num = z_info->p_max;
    int noldor_count = birth_peoples[0].race_count;
    birth_menu menu[16];
    cptr headings[16];

    (void)birth_peoples_validate();

    /* Always open the book on its first (story) page. */
    sdl_character_sheet_screen_reset_select_page();

    if (num > (int)N_ELEMENTS(menu))
        num = (int)N_ELEMENTS(menu);

    for (i = 0; i < num; i++)
    {
        menu[i].name = p_name + p_info[i].name;
        menu[i].ghost = false;
        menu[i].text = p_text + p_info[i].text;   /* shown at the bottom */
        headings[i] = NULL;
    }

    /* Group headings: Noldor (its lineages), then the other peoples. */
    if (num > 0)
        headings[0] =
            "The Noldor \xe2\x80\x94 exiled High Elves of three royal houses, "
            "whose names you may take:";
    if (noldor_count > 0 && noldor_count < num)
        headings[noldor_count] = "The other free peoples of Beleriand:";

    {
        /*
         * race_aux_hook only feeds the terminal-fallback detail; the SDL book
         * page ignores it (no stats/affinities -- just story).
         */
        birth_select_page page = {
            "The War of the Jewels",   /* title */
            birth_frame_top,           /* framing line above (accent) */
            birth_intro_lore,          /* chronicle (white) */
            birth_frame_bottom,        /* framing/charge below (accent) */
            headings,
            0, 0
        };

        race = get_player_choice(menu, num, p_ptr->prace, RACE_COL, 15,
            race_aux_hook, false, &page);
    }

    /* No selection -> back to main menu. */
    if (race == INVALID_CHOICE)
        return (false);

    /* If different race to last time, wipe history, age, height, weight. */
    if (race != p_ptr->prace)
    {
        p_ptr->history[0] = '\0';
        p_ptr->age = 0;
        p_ptr->ht = 0;
        p_ptr->wt = 0;
        for (i = 0; i < A_MAX; i++)
        {
            p_ptr->stat_base[i] = 0;
        }
    }
    p_ptr->prace = race;

    /* Save the race pointer */
    rp_ptr = &p_info[p_ptr->prace];

    /* Success */
    return (true);
}

// Check character flags
bool birth_character_is_set(int bit) {
    if (bit < 0 || bit >= FLAG_COUNT) return false;  // Out of bounds
    int word = bit / 32;
    int shift = bit % 32;
    return (rp_ptr->choice[word] & (1U << shift)) != 0;
}

/*
 * Display additional information about each character during the selection.
 */

static void character_aux_hook(birth_menu c_str)
{
    int character_idx, i, adj;
    int term_wid = 80;
    int term_hgt = 24;
    int description_row = birth_description_base_row();
    bool compact_layout = character_flags_need_compact_layout();
    bool tight_height = character_selection_tight_height();
    int name_col;
    int fallback_name_col;
    bool aligned_name_fits;
    char s[128];
    byte attr;

    /* Extract the proper character index from the string. */
    for (character_idx = 0; character_idx < z_info->c_max; character_idx++)
    {
        if (!strcmp(c_str.name, c_name + c_info[character_idx].name))
            break;
    }

    if (character_idx == z_info->c_max)
        return;

    /* Pixel-semantic selection screen: feed the detail panel and stop (no
     * terminal grid underdraw). */
    if (sdl_character_sheet_screen_active())
    {
        birth_select_emit_detail(p_ptr->prace, character_idx, false);
        return;
    }

    Term_get_size(&term_wid, &term_hgt);
    if (term_wid < 1)
        term_wid = 80;
    if (term_hgt < 1)
        term_hgt = 24;

    /* Clear the entire TOTAL_AUX_COL area FIRST before displaying new info */
    /* Clear from HEADER_ROW down but stop before DESCRIPTION_ROW to preserve history */
    for (i = HEADER_ROW; i < description_row; i++)
    {
        Term_putstr(TOTAL_AUX_COL, i, -1, TERM_WHITE,
            "                                         ");
        /* Also clear the right side area where penalties/flags appear */
        Term_putstr(TOTAL_AUX_COL + 21, i, -1, TERM_WHITE,
            "                                         ");
    }

    /* Also clear the abilities area (col + 7) but only in the same range */
    for (i = 0; i < description_row; i++)
    {
        Term_erase(TOTAL_AUX_COL + 7, i, 60);  /* Wider clearing */
    }

    /* Now display the new stats */
    for (i = 0; i < A_MAX; i++)
    {
        /*dump potential total stats*/
        strnfmt(s, sizeof(s), "%s", stat_names[i]);
        Term_putstr(TOTAL_AUX_COL, TABLE_ROW + i, -1, TERM_WHITE, s);

        adj = c_info[character_idx].h_adj[i] + rp_ptr->r_adj[i] + curses_stat_adj(i);
        strnfmt(s, sizeof(s), "%+d", adj);

        if (adj < 0)
            attr = TERM_RED;
        else if (adj == 0)
            attr = TERM_L_DARK;
        else if (adj == 1)
            attr = TERM_GREEN;
        else if (adj == 2)
            attr = TERM_L_GREEN;
        else
            attr = TERM_L_BLUE;

        Term_putstr(TOTAL_AUX_COL + 4, TABLE_ROW + i, -1, attr, s);
        {
            char desc[256];

            character_sheet_format_stat_hint(i, adj, true, desc,
                sizeof(desc));
            birth_detail_hover_add(TOTAL_AUX_COL, TABLE_ROW + i, 8, desc);
        }
    }
    // Check dead
    // if (c_str.ghost) Term_putstr(TOTAL_AUX_COL, QUESTION_ROW + A_MAX + 7, -1, TERM_RED,
    //     "Dead");
    // else Term_putstr(TOTAL_AUX_COL, TABLE_ROW + A_MAX +7, -1, TERM_L_BLUE,
    //     "Alive");
    char pretty_name[40];
    bool pretty_name_has_utf8;
    strnfmt(pretty_name, sizeof(pretty_name), "%s%s", c_name + c_info[character_idx].name, c_name + c_info[character_idx].alt_name); 
    pretty_name_has_utf8 = utf8_has_non_ascii(pretty_name);
    
    /* Add power stars to the character name */
    char power_stars[16];
    byte star_attr;
#if defined(__ANDROID__) || defined(SIL_IOS)
    cptr power_label = NULL;
    char power_suffix[32];

    birth_format_character_power(c_info[character_idx].power, true,
        power_stars, sizeof(power_stars), &star_attr, &power_label);
    if (power_label && power_label[0])
    {
        char power_word[16];

        SDL_strlcpy(power_word, power_label, sizeof(power_word));
        power_word[0] = (char)tolower((unsigned char)power_word[0]);
        strnfmt(power_suffix, sizeof(power_suffix), "%s %s", power_stars,
            power_word);
        SDL_strlcpy(power_stars, power_suffix, sizeof(power_stars));
    }
#else
    birth_format_character_power(c_info[character_idx].power, true,
        power_stars, sizeof(power_stars), &star_attr, NULL);
#endif
    
    fallback_name_col = QUESTION_COL
        + utf8_display_width_n(character_selection_header_text(true),
            (int)strlen(character_selection_header_text(true)))
        + 1;
    if (fallback_name_col < 0)
        fallback_name_col = 0;
    Term_erase(fallback_name_col, HEADER_ROW, 255);

    aligned_name_fits =
        (TOTAL_AUX_COL
            + utf8_display_width_n(pretty_name, (int)strlen(pretty_name))
            + utf8_display_width_n(power_stars, (int)strlen(power_stars))
            < term_wid);
    name_col = aligned_name_fits ? TOTAL_AUX_COL : fallback_name_col;

    Term_putstr(name_col, HEADER_ROW, -1, TERM_L_BLUE, pretty_name);
    if (pretty_name_has_utf8)
    {
        cptr star_text = (power_stars[0] == ' ') ? power_stars + 1 : power_stars;

        Term_putstr(name_col + (int)strlen(pretty_name), HEADER_ROW, -1,
            star_attr, star_text);
    }
    else
    {
        Term_putstr(name_col
            + utf8_display_width_n(pretty_name, (int)strlen(pretty_name)),
            HEADER_ROW, -1, star_attr, power_stars);
    }
    birth_invalidate_cells(fallback_name_col, HEADER_ROW,
        term_wid - fallback_name_col);
    
    {
        int legend_row = (compact_layout && tight_height) ? 9 : 10;
        int left_block_width = CLASS_COL;
        int character_list_count = 0;
        int character_list_rows;
        int wide_clear_row;

        if (legend_row < TABLE_ROW + A_MAX + 3)
            legend_row = TABLE_ROW + A_MAX + 3;

        if (left_block_width < 1)
            left_block_width = 1;

        for (i = 0; i < z_info->c_max; i++)
            if (birth_character_is_set(i))
                character_list_count++;

        character_list_rows = choice_visible_capacity(character_list_count,
            c_str.text, true);
        wide_clear_row = TABLE_ROW + character_list_rows;
        if (wide_clear_row < legend_row)
            wide_clear_row = legend_row;

        for (i = legend_row; i < birth_prompt_row(); ++i)
            Term_erase(0, i, left_block_width);
        for (i = wide_clear_row; i < birth_prompt_row(); ++i)
            Term_erase(0, i, TOTAL_AUX_COL - 1);
    }

    print_rh_flags(
        p_ptr->prace, character_idx, TOTAL_AUX_COL, TABLE_ROW + A_MAX + 1);

#if !defined(__ANDROID__) && !defined(SIL_IOS)
    {
        int legend_col = 2;  /* Left side */
        int legend_row = (compact_layout && tight_height) ? 9 : 10;
        int legend_limit_row = birth_prompt_row();
        bool legend_has_room;

        legend_has_room = (legend_row + 3 < legend_limit_row);
        if (compact_layout)
        {
            int compact_first_row = description_row;

            /*
             * Compact flags can start one row above description_row when the
             * screen is short. Keep the optional power legend whenever its
             * rows end before the compact block starts; no blank separator is
             * required.
             */
            if (Term && Term->hgt > 0 && Term->hgt < 24)
                compact_first_row--;

            legend_has_room = !tight_height
                && (compact_first_row > legend_row + 3)
                && (legend_row + 3 < legend_limit_row);
        }

        if (legend_has_room)
        {
            /* Count alive heroes by power level across ALL races */
            int power_counts[4] = {0, 0, 0, 0};  /* weak, fair, strong, mighty (P:3/P:4) */
            birth_count_alive_character_powers(power_counts);

            /* Display legend without "Power Rating:" header */
            Term_putstr(legend_col, legend_row, -1, TERM_L_GREEN, "***");
            strnfmt(s, sizeof(s), "Mighty %d", power_counts[3]);
            Term_putstr(legend_col + 4, legend_row, -1, TERM_WHITE, s);

            Term_putstr(legend_col, legend_row + 1, -1, TERM_GREEN, "***");
            strnfmt(s, sizeof(s), "Strong %d", power_counts[2]);
            Term_putstr(legend_col + 4, legend_row + 1, -1, TERM_WHITE, s);

            Term_putstr(legend_col, legend_row + 2, -1, TERM_WHITE, "**");
            strnfmt(s, sizeof(s), "Fair %d", power_counts[1]);
            Term_putstr(legend_col + 4, legend_row + 2, -1, TERM_WHITE, s);

            Term_putstr(legend_col, legend_row + 3, -1, TERM_RED, "*");
            strnfmt(s, sizeof(s), "Weak %d", power_counts[0]);
            Term_putstr(legend_col + 4, legend_row + 3, -1, TERM_WHITE, s);
        }
    }
#endif
}
/*
 * Player character template selection
 */
static bool get_character_profile(void)
{
    int i;
    int character = 0;
    int character_choice;
    int previous_choice = 0;
    int max_trait_rows = 0;
    birth_menu* character_menu;
    /* Per-row "welcome + chronicle" text, kept alive for get_player_choice. */
    static char character_desc_buf[48][1280];

    int no_character_flags = 1;
    for (int idx = 0; idx < FLAG_WORDS; ++idx) {
        if (rp_ptr->choice[idx] != 0) {
            no_character_flags = 0;
            break;  // At least one flag is set
        }
    }
    // default to the baseline character automatically if no choices are available
    if (no_character_flags)
    {
        p_ptr->pcharacter = 0;
        current_character_profile = &c_info[p_ptr->pcharacter];
        return (true);
    }

    character_menu = mem_alloc_array(z_info->c_max, birth_menu);

    /* Tabulate characters.  The shown description leads with the hero's own
     * second-person welcome (the B: line, e.g. "You rise aflame-spirit of
     * fire...") -- the name you put on -- followed by their chronicle. */
    for (i = 0; i < z_info->c_max; i++)
    {

        /* Analyze */
        if (birth_character_is_set(i))
        {
            cptr welcome = c_name + c_info[i].start_string;
            cptr lore = c_text + c_info[i].text;
            int trait_rows = collect_character_trait_lines(p_ptr->prace, i,
                false, NULL, 0, NULL);

            if (highscore_dead(c_name + c_info[i].name)) character_menu[character].ghost = true;
            else character_menu[character].ghost = false;

            if (trait_rows > max_trait_rows)
                max_trait_rows = trait_rows;

            character_menu[character].name = c_name + c_info[i].name;
            if (character < (int)N_ELEMENTS(character_desc_buf))
            {
                if (welcome && welcome[0])
                    strnfmt(character_desc_buf[character],
                        sizeof(character_desc_buf[character]), "%s\n\n%s",
                        welcome, lore);
                else
                    strnfmt(character_desc_buf[character],
                        sizeof(character_desc_buf[character]), "%s", lore);
                character_menu[character].text = character_desc_buf[character];
            }
            else
            {
                character_menu[character].text = lore;
            }
            if (p_ptr->pcharacter == i)
                previous_choice = character;
            character++;
        }
    }

    screen_push_touch_pane_hidden();
    {
        birth_select_page page = {
            "Whose fate will you carry?", /* title (the borrowing voice) */
            NULL, NULL, NULL,             /* not book mode: keep the detail panel */
            NULL,
            0, 0
        };
        page.detail_stat_rows_hint = A_MAX;
        page.detail_trait_rows_hint = max_trait_rows;

        character_choice = get_player_choice(
            character_menu, character, previous_choice, CLASS_COL, 22,
            character_aux_hook, true, &page);
    }
    screen_pop_touch_pane_hidden();

    /* No selection? */
    if (character_choice == INVALID_CHOICE)
    {
        character_menu = mem_free(character_menu);
        return (false);
    }

    /* Get character from choice number */
    character = 0;
    for (i = 0; i < z_info->c_max; i++)
    {
        if (birth_character_is_set(i))
        {
            if (character_choice == character)
            {
                // if different character to last time, then wipe the history, age,
                // height, weight
                if (i != p_ptr->pcharacter)
                {
                    int j;

                    p_ptr->history[0] = '\0';
                    p_ptr->age = 0;
                    p_ptr->ht = 0;
                    p_ptr->wt = 0;
                    for (j = 0; j < A_MAX; j++)
                    {
                        p_ptr->stat_base[j] = 0;
                    }
                }
                p_ptr->pcharacter = i;
            }
            character++;
        }
    }

    /* Cache the selected character template */
    current_character_profile = &c_info[p_ptr->pcharacter];

    character_menu = mem_free(character_menu);

    return (true);
}

/*
 * Helper function for 'player_birth()'.
 *
 * This function allows the player to select a race and character template, and
 * modify options (including the birth options).
 */
static NavResult character_creation_from_phase(int initial_phase)
{
    int i;

    int phase = initial_phase;
    NavResult result = NAV_OK;

    if (phase < 1 || phase > 2)
        phase = 1;
    if (phase == 2)
    {
        if (!p_ptr || !z_info || p_ptr->prace >= z_info->p_max)
            phase = 1;
        else
            rp_ptr = &p_info[p_ptr->prace];
    }

    screen_push_touch_pane_hidden();

    /*** Instructions ***/

    /* Clear screen */
    Term_clear();

    /* Display some helpful information */
    draw_character_selection_header(false);

    if (steamdeck_controls_active()) {
        int prompt_row = birth_prompt_row();
        char random_label[16];
        char back_label[16];
        char options_label[16];
        char scores_label[16];
        char full_desc_label[16];
        char help_label[16];
        char quit_label[16];
        char prompt_buf[160];

        birth_prompt_label('r', "r", random_label, sizeof(random_label));
        birth_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        birth_prompt_label('o', "o", options_label, sizeof(options_label));
        birth_prompt_label('s', "s", scores_label, sizeof(scores_label));
        birth_prompt_label(steamdeck_alt_action_key(), "X", full_desc_label,
            sizeof(full_desc_label));
        birth_prompt_label('?', "?", help_label, sizeof(help_label));
        if (streq(help_label, "?"))
            birth_prompt_label('h', "h", help_label, sizeof(help_label));
        birth_prompt_label('q', "q", quit_label, sizeof(quit_label));

        strnfmt(prompt_buf, sizeof(prompt_buf),
            "%s-random  %s-back  %s-options  %s-scores  %s-description  %s-help  %s-quit",
            random_label, back_label, options_label, scores_label, full_desc_label, help_label, quit_label);
        Term_putstr(QUESTION_COL, prompt_row, -1, TERM_SLATE, prompt_buf);
    } else if (sdl_touch_only_device_active()) {
        Term_putstr(QUESTION_COL, birth_prompt_row(), -1, TERM_SLATE,
            "Tap a row to choose, tap away to go back");
    } else {
        Term_putstr(QUESTION_COL, birth_prompt_row(), -1, TERM_SLATE,
            "r -random   ESC -back   o -options   s -scores   f -description   h -help   q -quit");
    }

    while (phase <= 2)
    {
        clear_question();

        if (phase == 1)
        {
            /* Choose the player's race */
            if (!get_player_race())
            {
                result = NAV_TO_MAIN; /* Esc at first screen -> back to main menu */
                goto cleanup;
            }

            /* Clean up */
            clear_question();

            phase++;
        }

        if (phase == 2)
        {
            draw_character_selection_header(true);

            /* Choose the player's character template */
            if (!get_character_profile())
            {
                phase = 1;          /* Esc here -> go back to race */
                draw_character_selection_header(false);
                /* Clear the character display area when going back to race selection */
                for (i = HEADER_ROW; i <= TABLE_ROW + A_MAX + 10; i++)
                {
                    Term_erase(TOTAL_AUX_COL, i, 255);
                }
                continue;
            }

            /* Clean up */
            clear_question();

            phase++;
        }
    }

    finalize_character_creation_selection();

    /* Done */
    result = NAV_OK;

cleanup:
    /* Hide the pixel-semantic selection overlay so the following terminal
     * screens (oath selection, etc.) and the main menu are visible again. */
    sdl_character_sheet_screen_hide();
    ui_menu_click_clear();
    screen_pop_touch_pane_hidden();
    return result;

}

NavResult character_creation(void)
{
    return character_creation_from_phase(1);
}

NavResult character_creation_resume_character(void)
{
    return character_creation_from_phase(2);
}
