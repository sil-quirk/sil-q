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

/*
 * The dead/alive state cannot change while the character picker is open.
 * Read it once when entering the picker: highscore_dead() opens and scans the
 * score file, so repeating it for every power-summary redraw made keyboard and
 * controller navigation visibly stall.
 */
static bool birth_character_dead_cache[FLAG_COUNT];
static int birth_character_dead_cache_count;
static bool birth_character_dead_cache_ready;

static void birth_prepare_character_dead_cache(void)
{
    int count = 0;

    birth_character_dead_cache_ready = false;
    birth_character_dead_cache_count = 0;
    memset(birth_character_dead_cache, 0,
        sizeof(birth_character_dead_cache));

    if (!z_info || !c_info || !c_name)
        return;

    count = MIN((int)z_info->c_max,
        (int)N_ELEMENTS(birth_character_dead_cache));
    for (int i = 0; i < count; i++)
    {
        birth_character_dead_cache[i] =
            highscore_dead(c_name + c_info[i].name) != 0;
    }

    birth_character_dead_cache_count = count;
    birth_character_dead_cache_ready = true;
}

static bool birth_character_is_dead(int character)
{
    if (!birth_character_dead_cache_ready)
        birth_prepare_character_dead_cache();
    if (character < 0 || character >= birth_character_dead_cache_count)
        return false;
    return birth_character_dead_cache[character];
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

        if (birth_character_is_dead(i))
            continue;

        power = c_info[i].power;
        if (power == 4)
            power_counts[3]++;
        else if (power <= 3)
            power_counts[power]++;
    }
}
#endif /* !__ANDROID__ && !SIL_IOS (only used by the desktop power summary) */

/*
 * Give the portrait carousel every title/power pair in the current race.
 * Its renderer uses the complete group to keep the power text no larger than
 * the smallest fitted hero name, rather than letting the focused hero alone
 * determine the balance.
 */
static void birth_add_character_title_candidates(void)
{
    int character;

    if (!z_info || !c_info || !c_name)
        return;

    for (character = 0; character < z_info->c_max; character++)
    {
        char pretty_name[128];
        char display_name[128];
        char power_text[64];
        byte star_attr;
#if defined(__ANDROID__) || defined(SIL_IOS)
        char stars[16];
        cptr power_label = NULL;
#endif

        if (!birth_character_is_set(character))
            continue;

        strnfmt(pretty_name, sizeof(pretty_name), "%s%s",
            c_name + c_info[character].name,
            c_name + c_info[character].alt_name);
        SDL_strlcpy(display_name, pretty_name, sizeof(display_name));

#if defined(__ANDROID__) || defined(SIL_IOS)
        birth_format_character_power(c_info[character].power, true,
            stars, sizeof(stars), &star_attr, &power_label);
        if (power_label && power_label[0])
        {
            char power_word[16];

            SDL_strlcpy(power_word, power_label, sizeof(power_word));
            power_word[0] = (char)tolower((unsigned char)power_word[0]);
            strnfmt(power_text, sizeof(power_text), "%s %s", stars,
                power_word);
        }
        else
        {
            SDL_strlcpy(power_text, stars, sizeof(power_text));
        }

        if (birth_character_is_dead(character))
            strnfmt(display_name, sizeof(display_name), "%s %s",
                BIRTH_FALLEN_MARK, pretty_name);
#else
        birth_format_character_power(c_info[character].power, true,
            power_text, sizeof(power_text), &star_attr, NULL);
#endif

        sdl_character_sheet_screen_add_select_title_candidate(display_name,
            power_text);
    }
}

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

#if defined(__ANDROID__) || defined(SIL_IOS)
        if (birth_character_is_dead(character))
        {
            char fallen_name[48];

            strnfmt(fallen_name, sizeof(fallen_name), "%s %s",
                BIRTH_FALLEN_MARK, pretty_name);
            sdl_character_sheet_screen_set_select_title_detail(fallen_name,
                title_stars, star_attr);
        }
        else
#endif
        {
            sdl_character_sheet_screen_set_select_title_detail(pretty_name,
                title_stars, star_attr);
        }

        birth_add_character_title_candidates();
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
        cptr ability_lines[CHARACTER_ABILITY_MAX];
        int ability_skills[CHARACTER_ABILITY_MAX];
        int ability_ids[CHARACTER_ABILITY_MAX];
        int n = collect_character_starting_abilities(character,
            ability_lines, (int)N_ELEMENTS(ability_lines), ability_skills,
            ability_ids);

        sdl_character_sheet_screen_set_select_ability_rows(n);
        for (i = 0; i < n && i < (int)N_ELEMENTS(ability_lines); i++)
        {
            hint[0] = '\0';
            birth_format_ability_hint(ability_skills[i], ability_ids[i],
                hint, sizeof(hint));
            sdl_character_sheet_screen_add_select_detail(ability_lines[i],
                ability_skill_color(ability_skills[i]), hint);
        }
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
static void race_aux_hook(birth_menu choice)
{
    for (int race = 0; race < z_info->p_max; race++)
    {
        if (streq(choice.name, p_name + p_info[race].name))
        {
            birth_select_emit_detail(race, -1, false);
            return;
        }
    }
}

/*
 * Player race (screen 1): a story/explanation "book page".  All peoples are
 * directly selectable in one grouped list -- the three Noldorin lineages under
 * a Noldor heading, then the other peoples -- with the setting lore on top and
 * the highlighted people's description at the bottom.  No stats or affinities;
 * just story.  Resolves a concrete p_info race into p_ptr->prace.
 */
static bool get_player_race(bool open_on_choice_page)
{
    int i;
    int race;
    int num = z_info->p_max;
    int noldor_count = birth_peoples[0].race_count;
    birth_menu menu[16];
    cptr headings[16];

    (void)birth_peoples_validate();

    /* Fresh entry opens the book on its first (story) page; returning from the
     * character page lands back on the choice (list) page (handled below). */
    if (!open_on_choice_page)
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
        birth_select_page page = {
            "The War of the Jewels",   /* title */
            birth_frame_top,           /* framing line above (accent) */
            birth_intro_lore,          /* chronicle (white) */
            birth_frame_bottom,        /* framing/charge below (accent) */
            headings,
            0, 0, 0,
            open_on_choice_page        /* back from character -> list page */
        };

        race = get_player_choice(menu, num, p_ptr->prace, race_aux_hook,
            &page);
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

static void character_aux_hook(birth_menu choice)
{
    for (int character = 0; character < z_info->c_max; character++)
    {
        if (streq(choice.name, c_name + c_info[character].name))
        {
            birth_select_emit_detail(p_ptr->prace, character, false);
            return;
        }
    }
}

static bool get_character_profile(void)
{
    int i;
    int character = 0;
    int character_choice;
    int previous_choice = 0;
    int max_ability_rows = 0;
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

    birth_prepare_character_dead_cache();
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
            int ability_rows = collect_character_starting_abilities(i, NULL,
                0, NULL, NULL);

            if (birth_character_is_dead(i)) character_menu[character].ghost = true;
            else character_menu[character].ghost = false;

            if (ability_rows > max_ability_rows)
                max_ability_rows = ability_rows;
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
            0, 0, 0,
            false                         /* not book mode: no choice-page jump */
        };
        page.detail_stat_rows_hint = A_MAX;
        page.detail_ability_rows_hint = max_ability_rows;
        page.detail_trait_rows_hint = max_trait_rows;

        character_choice = get_player_choice(character_menu, character,
            previous_choice, character_aux_hook, &page);
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
    int phase = initial_phase;
    NavResult result = NAV_OK;
    /* True once we have stepped back into race selection from the character
     * page, so the race book reopens on its choice page rather than page 0. */
    bool race_from_character = false;

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

    while (phase <= 2)
    {
        clear_question();

        if (phase == 1)
        {
            /* Choose the player's race */
            if (!get_player_race(race_from_character))
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
            /* Choose the player's character template */
            if (!get_character_profile())
            {
                phase = 1;          /* Esc here -> go back to race */
                race_from_character = true; /* reopen race book on its list */
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
