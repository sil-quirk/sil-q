#include "angband.h"
#include "metarun-internal.h"

/* ------------------------------------------------------------------------
 * Story book
 *
 * This replaces the old single statistics dashboard with five persistent
 * pages.  The book deliberately delegates all mutations to the existing
 * blessing, threshold, and difficulty menus so their validation and save
 * behavior remain the single source of truth.
 * ------------------------------------------------------------------------ */

enum story_book_page {
    STORY_BOOK_STATISTICS = 0,
    STORY_BOOK_BLESSINGS,
    STORY_BOOK_CURSES,
    STORY_BOOK_DIFFICULTY,
    STORY_BOOK_METARUNS,
    STORY_BOOK_PAGE_MAX
};

enum story_book_action {
    STORY_BOOK_PREVIOUS = 100,
    STORY_BOOK_NEXT,
    STORY_BOOK_CLOSE,
    STORY_BOOK_EXCHANGE,
    STORY_BOOK_THRESHOLD,
    STORY_BOOK_CHANGE_DIFFICULTY,
    STORY_BOOK_BLITZ,
    STORY_BOOK_NEW_TALE,
    STORY_BOOK_LOAD_TALE,
    STORY_BOOK_CURSES_EARLIER,
    STORY_BOOK_CURSES_LATER,
    STORY_BOOK_RUNS_NEWER,
    STORY_BOOK_RUNS_OLDER,
    STORY_BOOK_DIFFICULTY_CONFIRM,
    STORY_BOOK_DIFFICULTY_CANCEL,
    STORY_BOOK_PAGE_BASE = 200,
    STORY_BOOK_CURSE_BASE = 10000,
    STORY_BOOK_RUN_BASE = 20000,
    STORY_BOOK_MINOR_BASE = 30000,
    STORY_BOOK_MAJOR_BASE = 31000,
    STORY_BOOK_REMOVE_CURSE_BASE = 32000,
    STORY_BOOK_THRESHOLD_BASE = 33000,
    STORY_BOOK_DIFFICULTY_BASE = 34000
};

enum story_book_tale_request {
    STORY_BOOK_TALE_REQUEST_NONE = 0,
    STORY_BOOK_TALE_REQUEST_NEW,
    STORY_BOOK_TALE_REQUEST_LOAD
};

static void story_book_show_tale_action_error(void)
{
    bool recovery_required = metarun_tale_recovery_required();

    screen_save();
    Term_clear();
    Term_putstr(2, 5, -1, TERM_L_RED,
        "The tale could not be changed safely.");
    if (recovery_required) {
        Term_putstr(2, 7, -1, TERM_WHITE,
            "Tale recovery is pending; restart before starting a Story character.");
    } else {
        Term_putstr(2, 7, -1, TERM_WHITE,
            "The existing tale remains active; see log.txt for details.");
    }
    Term_putstr(2, 9, -1, TERM_L_DARK,
        "Press any key to return to the Chronicle of Tales.");
    metarun_wait_hidden();
    screen_load();
}

static int story_book_curse_get(int id)
{
    if (id < 0 || id >= METAR_CURSE_SLOTS)
        return 0;
    return metar.curse_stacks[id];
}

static bool story_book_curse_seen(int id)
{
    if (id < 0 || id >= METAR_CURSE_SLOTS)
        return false;
    return (metar.curses_seen & (1ULL << id)) != 0;
}



static int story_book_compare_metaruns(const void *a, const void *b)
{
    s16b ia = *(const s16b *)a;
    s16b ib = *(const s16b *)b;
    const metarun *ma = &metaruns[ia];
    const metarun *mb = &metaruns[ib];

    if (ma->last_played != mb->last_played)
        return (ma->last_played < mb->last_played) ? 1 : -1;
    if (ma->id != mb->id)
        return (ma->id < mb->id) ? 1 : -1;
    return 0;
}

static s16b *story_book_metarun_order(void)
{
    s16b *order;

    if (!metaruns || metarun_max <= 0)
        return NULL;
    order = mem_alloc_array(metarun_max, s16b);
    for (s16b i = 0; i < metarun_max; i++)
        order[i] = i;
    qsort(order, metarun_max, sizeof(*order), story_book_compare_metaruns);
    return order;
}

static s16b story_book_metarun_index_at(int position)
{
    s16b *order;
    s16b result = -1;

    if (position < 0 || position >= metarun_max)
        return -1;
    order = story_book_metarun_order();
    if (order) {
        result = order[position];
        order = mem_free(order);
    }
    return result;
}


typedef struct story_book_sdl_state {
    int selected_curse;
    int curse_offset;
    int selected_run;
    int run_offset;
    int pending_difficulty;
} story_book_sdl_state;

static void story_book_sdl_heading(cptr text, bool new_page)
{
    if (new_page)
        sdl_character_sheet_screen_break_book_page();
    sdl_character_sheet_screen_add_book_paragraph_colored(text, TERM_YELLOW);
}

static void story_book_sdl_append(char *buf, size_t size, cptr text)
{
    if (!buf || size == 0 || !text || !text[0])
        return;
    if (buf[0])
        SDL_strlcat(buf, "\n", size);
    SDL_strlcat(buf, text, size);
}

static void story_book_sdl_copy_excerpt(char *out, size_t size, cptr text,
    size_t max_chars)
{
    size_t len;

    if (!out || size == 0)
        return;
    SDL_strlcpy(out, text ? text : "", size);
    len = strlen(out);
    if (len <= max_chars)
        return;
    len = MIN(max_chars, size - 1);
    while (len > 0 && out[len] != ' ')
        len--;
    /* No break space within reach: fall back to a hard cut, but keep it on a
     * UTF-8 sequence boundary so a multibyte symbol is never split. */
    if (len < 8)
        len = (size_t)utf8_safe_prefix_len(out, (int)MIN(max_chars, size - 1));
    out[len] = '\0';
    if (len + 4 < size)
        SDL_strlcat(out, "...", size);
}

static int story_book_sdl_collect_known_curses(int *ids, int capacity)
{
    int count = 0;

    for (int id = 0; id < z_info->cu_max && id < METAR_CURSE_SLOTS; id++) {
        if (!story_book_curse_seen(id))
            continue;
        if (count < capacity)
            ids[count] = id;
        count++;
    }
    return MIN(count, capacity);
}

static int story_book_sdl_difficulty_curse_stacks(int runtype_id,
    int *distinct)
{
    int count = 0;
    int kinds = 0;

    if (runtype_info && runtype_id >= 0 && runtype_id < z_info->rt_max) {
        int limit = MIN(METAR_CURSE_SLOTS, z_info->cu_max);
        for (int id = 0; id < limit; id++) {
            int stacks = runtype_info[runtype_id].curse_stacks[id];
            if (stacks <= 0)
                continue;
            count += stacks;
            kinds++;
        }
    }
    if (distinct)
        *distinct = kinds;
    return count;
}

static void story_book_sdl_difficulty_effects(int runtype_id, char *out,
    size_t size)
{
    int kinds = 0;
    int stacks = story_book_sdl_difficulty_curse_stacks(runtype_id, &kinds);
    int win_goal = WINCON_SILMARILS;
    u32b easier;
    u32b normal;
    u32b harder;

    if (!out || size == 0)
        return;
    out[0] = '\0';
    if (!runtype_info || runtype_id < 0 || runtype_id >= z_info->rt_max)
        return;
    if (runtype_info[runtype_id].win_con)
        win_goal = runtype_info[runtype_id].win_con;
    easier = runtype_threshold_for_mode(runtype_id,
        METARUN_BLESSING_THRESHOLD_EASIER);
    normal = runtype_threshold_for_mode(runtype_id,
        METARUN_BLESSING_THRESHOLD_NORMAL);
    harder = runtype_threshold_for_mode(runtype_id,
        METARUN_BLESSING_THRESHOLD_HARDER);

    if (stacks > 0) {
        strnfmt(out, size,
            "%d starting curse stack%s across %d curse%s. Win at %d "
            "Silmarils. Blessings require %lu/%lu/%lu fallen-score points "
            "on easier/normal/harder thresholds.",
            stacks, stacks == 1 ? "" : "s", kinds, kinds == 1 ? "" : "s",
            win_goal, (unsigned long)easier, (unsigned long)normal,
            (unsigned long)harder);
    } else {
        strnfmt(out, size,
            "No starting curses. Win at %d Silmarils. Blessings require "
            "%lu/%lu/%lu fallen-score points on easier/normal/harder "
            "thresholds.", win_goal, (unsigned long)easier,
            (unsigned long)normal, (unsigned long)harder);
    }
}

static void story_book_sdl_build(bool startup_scene,
    story_book_sdl_state *state, int restore_page)
{
    char buf[1024];
    char line[256];
    const char *difficulty = "Unknown";
    int win_goal = WINCON_SILMARILS;
    int required_survivors;
    int available;
    u32b threshold;

    if (!state)
        return;
    sdl_character_sheet_screen_begin_book(
        "The Chronicle of the Long Defiance");

    /* Let the reader leave from any page with the mouse (or a touch tap). */
    sdl_character_sheet_screen_set_book_close_button(true);
    if (startup_scene) {
        sdl_character_sheet_screen_set_book_close_label(
            "Proceed to character creation");
    }

    sdl_character_sheet_screen_add_book_contents("I. Statistics",
        STORY_BOOK_PAGE_BASE + STORY_BOOK_STATISTICS, STORY_BOOK_STATISTICS);
    sdl_character_sheet_screen_add_book_contents("II. Blessings",
        STORY_BOOK_PAGE_BASE + STORY_BOOK_BLESSINGS, STORY_BOOK_BLESSINGS);
    sdl_character_sheet_screen_add_book_contents("III. Curses",
        STORY_BOOK_PAGE_BASE + STORY_BOOK_CURSES, STORY_BOOK_CURSES);
    sdl_character_sheet_screen_add_book_contents("IV. Difficulty",
        STORY_BOOK_PAGE_BASE + STORY_BOOK_DIFFICULTY, STORY_BOOK_DIFFICULTY);
    sdl_character_sheet_screen_add_book_contents("V. Tales",
        STORY_BOOK_PAGE_BASE + STORY_BOOK_METARUNS, STORY_BOOK_METARUNS);

    refresh_current_metar_score();
    compute_blessing_pool();
    metarun_sanitize_major_blessing_bits(&metar);

    if (runtype_info && metar.type < z_info->rt_max
        && runtype_info[metar.type].name[0])
    {
        difficulty = runtype_info[metar.type].name;
        if (runtype_info[metar.type].win_con)
            win_goal = runtype_info[metar.type].win_con;
    }
    required_survivors = required_survivor_target(win_goal);
    available = blessing_points_available();
    threshold = metarun_threshold_value(&metar);

    /* Page I: statistics.  The tale is told in the warm tones of the Jar of
     * Light that stands beside it -- tan for the framing, gold for glory and
     * the Silmarils, cream for the count of the living and the dead, and amber
     * (the colour of the held light) for the road to the next blessing. */
    story_book_sdl_heading("I - The Measure of the Tale", false);

    strnfmt(line, sizeof(line), "Tale %u on %s difficulty.",
        (unsigned)metar.id, difficulty);
    sdl_character_sheet_screen_add_book_paragraph_colored(line, TERM_L_UMBER);

    strnfmt(line, sizeof(line),
        "Tale score: %lu  (best hero run: %lu).   Silmarils: %d of %d.",
        (unsigned long)metar.score,
        (unsigned long)get_best_run_score_from_highscores(),
        metar.silmarils, win_goal);
    sdl_character_sheet_screen_add_book_paragraph_colored(line, TERM_YELLOW);

    strnfmt(line, sizeof(line), "Living heroes: %d (need %d).   Deaths: %d.",
        metar.alive_characters, required_survivors, metar.deaths);
    sdl_character_sheet_screen_add_book_paragraph_colored(line, TERM_L_WHITE);

    strnfmt(line, sizeof(line),
        "Blessing points: %d available, %d spent, %d earned.",
        available, metar.blessing_points_spent, metar.blessing_points);
    sdl_character_sheet_screen_add_book_paragraph_colored(line, TERM_L_UMBER);

    strnfmt(line, sizeof(line),
        "Fallen score: %lu.   Next blessing: %lu of %lu (%s).",
        (unsigned long)metar.fallen_score_total,
        (unsigned long)metar.fallen_score_pool, (unsigned long)threshold,
        threshold_mode_name(metarun_get_threshold_mode(&metar)));
    sdl_character_sheet_screen_add_book_paragraph_colored(line, TERM_ORANGE);

    sdl_character_sheet_screen_set_book_lamp(metar.fallen_score_pool,
        threshold, STORY_BOOK_STATISTICS);
    /* Page II: blessings and the exchange. */
    story_book_sdl_heading("II - Blessings of the West", true);
    buf[0] = '\0';
    strnfmt(line, sizeof(line), "%d blessing point%s remain to be bestowed.",
        available, available == 1 ? "" : "s");
    story_book_sdl_append(buf, sizeof(buf), line);
    char minor_line[512] = "Minor blessings: ";
    int minor_count = 0;
    for (int id = 0; id < z_info->cu_max; id++) {
        int stacks = story_book_curse_get(id);
        if (stacks >= 0)
            continue;
        if (minor_count > 0)
            SDL_strlcat(minor_line, ", ", sizeof(minor_line));
        SDL_strlcat(minor_line, blessing_display_name(id), sizeof(minor_line));
        minor_count++;
        if (minor_count >= 3)
            break;
    }
    if (!minor_count)
        SDL_strlcat(minor_line, "none", sizeof(minor_line));
    story_book_sdl_append(buf, sizeof(buf), minor_line);
    char major_line[512] = "Major blessings: ";
    int major_count = 0;
    for (int i = 0; i < metarun_major_blessing_count(); i++) {
        if (!metarun_has_major_blessing_index(i))
            continue;
        if (major_count > 0)
            SDL_strlcat(major_line, ", ", sizeof(major_line));
        SDL_strlcat(major_line, major_blessing_name_str(i), sizeof(major_line));
        major_count++;
        if (major_count >= 3)
            break;
    }
    if (!major_count)
        SDL_strlcat(major_line, "none", sizeof(major_line));
    story_book_sdl_append(buf, sizeof(buf), major_line);
    sdl_character_sheet_screen_add_book_paragraph_colored(buf, TERM_L_GREEN);

    if (!run_mode_is_blitz() && available > 0) {
        int minor_choices[3];
        int minor_choices_count = metarun_inline_minor_blessing_choices(
            minor_choices);

        for (int i = 0; i < minor_choices_count; i++) {
            int id = minor_choices[i];
            strnfmt(line, sizeof(line), "Receive %s (cost 1)",
                blessing_display_name(id));
            sdl_character_sheet_screen_add_book_action_colored(line,
                STORY_BOOK_MINOR_BASE + id, TERM_L_GREEN);
        }
        int shown_curses = 0;
        for (int id = 0; id < z_info->cu_max && shown_curses < 3; id++) {
            int stacks = MAX(story_book_curse_get(id), 0);
            if (stacks <= 0)
                continue;
            strnfmt(line, sizeof(line), "Lift one stack of %s (cost 1)",
                curse_display_name(id));
            sdl_character_sheet_screen_add_book_action_colored(line,
                STORY_BOOK_REMOVE_CURSE_BASE + id, TERM_L_RED);
            shown_curses++;
        }
        int shown_major = 0;
        for (int i = 0; i < major_blessing_capacity() && shown_major < 3; i++) {
            int cost;
            if (!major_blessing_def(i) || metarun_has_major_blessing_index(i))
                continue;
            cost = major_blessing_cost(i);
            strnfmt(line, sizeof(line), "Seal %s (cost %d)",
                major_blessing_name_str(i), cost);
            if (cost <= available) {
                sdl_character_sheet_screen_add_book_action_colored(line,
                    STORY_BOOK_MAJOR_BASE + i, TERM_YELLOW);
            } else {
                sdl_character_sheet_screen_add_book_paragraph_colored(line,
                    TERM_L_DARK);
            }
            shown_major++;
        }
    } else if (run_mode_is_blitz()) {
        sdl_character_sheet_screen_add_book_paragraph_colored(
            "Tale blessings cannot be changed during a Blitz run.",
            TERM_L_DARK);
    } else {
        sdl_character_sheet_screen_add_book_paragraph_colored(
            "Earn another blessing point to alter the gifts of this tale.",
            TERM_L_DARK);
    }

    /* Page III: known curses, with a bounded live list and full selected lore. */
    story_book_sdl_heading("III - The Curses Made Known", true);
    int curse_ids[METAR_CURSE_SLOTS];
    int curse_count = story_book_sdl_collect_known_curses(curse_ids,
        N_ELEMENTS(curse_ids));
    if (curse_count <= 0) {
        state->selected_curse = -1;
        state->curse_offset = 0;
        sdl_character_sheet_screen_add_book_paragraph(
            "No curse has yet revealed its full nature in this tale.");
    } else {
        int selected_pos = 0;

        if (state->curse_offset < 0) state->curse_offset = 0;
        if (state->curse_offset >= curse_count)
            state->curse_offset = MAX(0, curse_count - 5);
        for (int i = 0; i < curse_count; i++) {
            if (curse_ids[i] == state->selected_curse) {
                selected_pos = i;
                break;
            }
        }
        if (state->selected_curse < 0
            || !story_book_curse_seen(state->selected_curse)) {
            state->selected_curse = curse_ids[state->curse_offset];
            selected_pos = state->curse_offset;
        }
        if (selected_pos < state->curse_offset
            || selected_pos >= state->curse_offset + 5)
            state->curse_offset = (selected_pos / 5) * 5;

        int end = MIN(curse_count, state->curse_offset + 5);
        for (int i = state->curse_offset; i < end; i++) {
            int id = curse_ids[i];
            int stacks = story_book_curse_get(id);
            char suffix[32] = "";
            if (stacks > 0)
                strnfmt(suffix, sizeof(suffix), " (active x%d)", stacks);
            strnfmt(line, sizeof(line), "%c %s%s",
                id == state->selected_curse ? '>' : '-',
                curse_display_name(id), suffix);
            sdl_character_sheet_screen_add_book_action_colored(line,
                STORY_BOOK_CURSE_BASE + id,
                stacks > 0 ? TERM_L_RED : TERM_SLATE);
        }

        const curse_type *curse = &cu_info[state->selected_curse];
        char desc[384];
        char power[384];
        story_book_sdl_copy_excerpt(desc, sizeof(desc),
            curse->text ? cu_text + curse->text : "No description recorded.",
            300);
        story_book_sdl_copy_excerpt(power, sizeof(power),
            curse->power ? cu_text + curse->power : "Unknown", 260);
        strnfmt(buf, sizeof(buf), "%s\n%s",
            curse_display_name(state->selected_curse), desc);
        sdl_character_sheet_screen_add_book_paragraph_colored(buf, TERM_SLATE);
        strnfmt(buf, sizeof(buf), "Known effect: %s", power);
        sdl_character_sheet_screen_add_book_paragraph_colored(buf, TERM_L_RED);
        if (state->curse_offset > 0)
            sdl_character_sheet_screen_add_book_action_colored(
                "Earlier known curses", STORY_BOOK_CURSES_EARLIER,
                TERM_L_BLUE);
        if (end < curse_count)
            sdl_character_sheet_screen_add_book_action_colored(
                "Later known curses", STORY_BOOK_CURSES_LATER,
                TERM_L_BLUE);
    }

    /* Page IV: difficulty changes are previewed and confirmed on this page. */
    story_book_sdl_heading("IV - The Weight of Doom", true);
    if (!run_mode_is_blitz() && state->pending_difficulty >= 0 && runtype_info
        && state->pending_difficulty < z_info->rt_max
        && runtype_info[state->pending_difficulty].name[0])
    {
        int pending = state->pending_difficulty;

        strnfmt(buf, sizeof(buf), "Change %s to %s?", difficulty,
            runtype_info[pending].name);
        sdl_character_sheet_screen_add_book_paragraph_colored(buf,
            runtype_info[pending].colour);
        story_book_sdl_difficulty_effects(pending, buf, sizeof(buf));
        sdl_character_sheet_screen_add_book_paragraph_colored(buf, TERM_WHITE);
        sdl_character_sheet_screen_add_book_paragraph_colored(
            "WARNING: This difficulty increase is permanent for this tale. "
            "You cannot return to the current difficulty after "
            "confirming.", TERM_L_RED);
        sdl_character_sheet_screen_add_book_action_colored(
            "Confirm permanent difficulty change",
            STORY_BOOK_DIFFICULTY_CONFIRM, TERM_L_RED);
        sdl_character_sheet_screen_add_book_action_colored(
            "Cancel - keep current difficulty", STORY_BOOK_DIFFICULTY_CANCEL,
            TERM_L_GREEN);
    } else {
        const metarun_blessing_threshold_mode modes[] = {
            METARUN_BLESSING_THRESHOLD_EASIER,
            METARUN_BLESSING_THRESHOLD_NORMAL,
            METARUN_BLESSING_THRESHOLD_HARDER
        };

        strnfmt(buf, sizeof(buf),
            "Current: %s. Blessing threshold: %s - %lu points.", difficulty,
            threshold_mode_name(metarun_get_threshold_mode(&metar)),
            (unsigned long)threshold);
        sdl_character_sheet_screen_add_book_paragraph_colored(buf,
            TERM_L_BLUE);
        for (int i = 0; i < (int)N_ELEMENTS(modes); i++) {
            byte attr = modes[i] == METARUN_BLESSING_THRESHOLD_EASIER
                ? TERM_L_GREEN
                : (modes[i] == METARUN_BLESSING_THRESHOLD_HARDER
                    ? TERM_ORANGE : TERM_WHITE);
            strnfmt(line, sizeof(line), "%s threshold - %lu points",
                threshold_mode_name(modes[i]),
                (unsigned long)runtype_threshold_for_mode(metar.type,
                    modes[i]));
            if (!run_mode_is_blitz()) {
                sdl_character_sheet_screen_add_book_action_colored(line,
                    STORY_BOOK_THRESHOLD_BASE + modes[i], attr);
            } else {
                sdl_character_sheet_screen_add_book_paragraph_colored(line,
                    attr);
            }
        }
        sdl_character_sheet_screen_add_book_paragraph_colored(
            "Difficulty levels (select one to review its effects):",
            TERM_SLATE);
        for (int i = 0; runtype_info && i < z_info->rt_max; i++) {
            int stacks;
            byte attr;

            if (!runtype_info[i].name[0])
                continue;
            stacks = story_book_sdl_difficulty_curse_stacks(i, NULL);
            strnfmt(line, sizeof(line), "%c %s - %s", i == metar.type ? '>' : '-',
                runtype_info[i].name,
                stacks > 0 ? format("%d starting curse stack%s", stacks,
                    stacks == 1 ? "" : "s") : "no starting curses");
            attr = i == metar.type ? TERM_YELLOW
                : (i < metar.max_difficulty_reached ? TERM_L_DARK
                                                     : runtype_info[i].colour);
            if (!run_mode_is_blitz() && i != metar.type
                && i >= metar.max_difficulty_reached) {
                sdl_character_sheet_screen_add_book_action_colored(line,
                    STORY_BOOK_DIFFICULTY_BASE + i, attr);
            } else {
                if (i < metar.max_difficulty_reached)
                    SDL_strlcat(line, " (locked)", sizeof(line));
                sdl_character_sheet_screen_add_book_paragraph_colored(line,
                    attr);
            }
        }
        if (run_mode_is_blitz())
            sdl_character_sheet_screen_add_book_paragraph_colored(
                "Tale difficulty cannot be changed during a Blitz run.",
                TERM_L_DARK);
    }

    /* Page V: click a tale to replace the detail paragraph in place. */
    story_book_sdl_heading("V - The Chronicle of Tales", true);
    if (startup_scene) {
        sdl_character_sheet_screen_add_book_paragraph_colored(
            "Blitz is a separate, self-contained run for practice or quick "
            "play. It does not affect this tale, its heroes, blessings, "
            "saves, or score.", TERM_SLATE);
        sdl_character_sheet_screen_add_book_action_colored(
            "Start a separate Blitz run", STORY_BOOK_BLITZ, TERM_L_BLUE);
    }
    s16b *order = story_book_metarun_order();
    bool can_manage = metarun_tale_management_available();
    if (can_manage) {
        sdl_character_sheet_screen_add_book_action_colored(
            steamdeck_controls_active() ? "Y - Start a new tale"
                                        : "N - Start a new tale",
            STORY_BOOK_NEW_TALE, TERM_L_BLUE);
    }
    if (!order) {
        state->selected_run = 0;
        state->run_offset = 0;
        sdl_character_sheet_screen_add_book_paragraph(
            "No tales have been recorded.");
    } else {
        if (state->selected_run < 0) state->selected_run = 0;
        if (state->selected_run >= metarun_max)
            state->selected_run = metarun_max - 1;
        if (state->run_offset < 0) state->run_offset = 0;
        if (state->run_offset >= metarun_max)
            state->run_offset = MAX(0, metarun_max - 5);
        if (state->selected_run < state->run_offset
            || state->selected_run >= state->run_offset + 5)
            state->run_offset = (state->selected_run / 5) * 5;

        if (can_manage && order[state->selected_run] != current_run) {
            sdl_character_sheet_screen_add_book_action_colored(
                steamdeck_controls_active() ? "A - Load selected tale"
                                            : "Enter - Load selected tale",
                STORY_BOOK_LOAD_TALE,
                TERM_L_BLUE);
        }

        int end = MIN(metarun_max, state->run_offset + 5);
        for (int pos = state->run_offset; pos < end; pos++) {
            s16b idx = order[pos];
            const metarun *m = &metaruns[idx];
            time_t played = (time_t)m->last_played;
            char date[24] = "unknown date";
            struct tm *when = localtime(&played);
            if (when)
                strftime(date, sizeof(date), "%Y-%m-%d", when);
            strnfmt(line, sizeof(line), "%c Tale %u - score %lu - %s%s",
                pos == state->selected_run ? '>' : '-', (unsigned)m->id,
                (unsigned long)m->score, date,
                idx == current_run ? " (current)" : "");
            sdl_character_sheet_screen_add_book_action_colored(line,
                STORY_BOOK_RUN_BASE + pos,
                idx == current_run ? TERM_YELLOW : TERM_WHITE);
        }

        s16b idx = order[state->selected_run];
        const metarun *m = &metaruns[idx];
        const char *run_difficulty = "Unknown";
        int run_win_goal = WINCON_SILMARILS;
        int curses = 0;
        int blessings = 0;
        int majors = 0;
        const char *result = "In progress";
        if (runtype_info && m->type < z_info->rt_max
            && runtype_info[m->type].name[0])
        {
            run_difficulty = runtype_info[m->type].name;
            if (runtype_info[m->type].win_con)
                run_win_goal = runtype_info[m->type].win_con;
        }
        if (m->silmarils >= run_win_goal) result = "Victory";
        else if (m->deaths >= LOSECON_DEATHS) result = "Defeat";
        for (int id = 0; id < METAR_CURSE_SLOTS; id++) {
            if (m->curse_stacks[id] > 0) curses += m->curse_stacks[id];
            else blessings -= m->curse_stacks[id];
        }
        for (int i = 0; i < major_blessing_capacity() && i < 16; i++) {
            if (m->major_blessings & (1U << i)) majors++;
        }
        strnfmt(buf, sizeof(buf),
            "Tale %u - %s.\n%s difficulty; %d Silmarils; %d deaths; best hero run %lu.\n"
            "%d curse stacks, %d minor blessing stacks, %d major blessings.",
            (unsigned)m->id, result, run_difficulty, m->silmarils, m->deaths,
            (unsigned long)m->best_run_score, curses, blessings, majors);
        sdl_character_sheet_screen_add_book_paragraph_colored(buf, TERM_SLATE);
        if (state->run_offset > 0)
            sdl_character_sheet_screen_add_book_action_colored(
                "Newer tales", STORY_BOOK_RUNS_NEWER, TERM_L_BLUE);
        if (end < metarun_max)
            sdl_character_sheet_screen_add_book_action_colored(
                "Older tales", STORY_BOOK_RUNS_OLDER, TERM_L_BLUE);
        order = mem_free(order);
    }

    sdl_character_sheet_screen_commit_book();
    sdl_character_sheet_screen_set_book_page(restore_page);
}

static void story_book_show_sdl(bool startup_scene)
{
    int restore_page = 0;

    for (;;) {
        story_book_sdl_state state = { -1, 0, 0, 0, -1 };
        bool done = false;
        bool launch_blitz = false;
        bool tale_action_ok = true;
        enum story_book_tale_request tale_request =
            STORY_BOOK_TALE_REQUEST_NONE;
        s16b requested_tale = -1;

        if (!startup_scene)
            screen_save();
        screen_push_supporting_panes_hidden();

        story_book_sdl_build(startup_scene, &state, restore_page);

        while (!done) {
        int key;
        int clicked = 0;
        int click_action = UI_MENU_CLICK_PRIMARY;
        int page;
        int page_count;
        int tales_page;
        bool tales_section;

        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        key = metarun_inkey_hidden();

        if (ui_menu_click_take_action(&clicked, &click_action)) {
            ui_menu_click_clear();
            if (click_action == UI_MENU_CLICK_HOVER)
                continue;

            /* The on-screen exit button (mouse/touch) leaves the book. */
            if (clicked == SDL_SELECT_CLICK_CLOSE) {
                done = true;
                continue;
            }

            page = sdl_character_sheet_screen_select_page();
            page_count = sdl_character_sheet_screen_select_page_count();
            if (clicked == SDL_SELECT_CLICK_PAGE_PREV) {
                if (page > 0 && !sdl_character_sheet_screen_page_turning())
                    sdl_character_sheet_screen_begin_page_turn(-1);
                continue;
            }
            if (clicked == SDL_SELECT_CLICK_PAGE_NEXT) {
                if (page < page_count - 1
                    && !sdl_character_sheet_screen_page_turning())
                    sdl_character_sheet_screen_begin_page_turn(+1);
                else if (page >= page_count - 1)
                    done = true;
                continue;
            }

            if (clicked >= STORY_BOOK_PAGE_BASE
                && clicked < STORY_BOOK_PAGE_BASE + STORY_BOOK_PAGE_MAX)
            {
                int target = sdl_character_sheet_screen_book_contents_page(
                    clicked - STORY_BOOK_PAGE_BASE);
                if (target >= 0 && target < page_count && target != page
                    && !sdl_character_sheet_screen_page_turning())
                    sdl_character_sheet_screen_begin_page_turn_to(target);
                continue;
            }

            if (clicked >= STORY_BOOK_CURSE_BASE
                && clicked < STORY_BOOK_CURSE_BASE + METAR_CURSE_SLOTS)
            {
                state.selected_curse = clicked - STORY_BOOK_CURSE_BASE;
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked >= STORY_BOOK_RUN_BASE
                && clicked < STORY_BOOK_RUN_BASE + metarun_max)
            {
                state.selected_run = clicked - STORY_BOOK_RUN_BASE;
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked == STORY_BOOK_NEW_TALE
                && metarun_tale_management_available())
            {
                tale_request = STORY_BOOK_TALE_REQUEST_NEW;
                done = true;
                continue;
            }
            if (clicked == STORY_BOOK_LOAD_TALE
                && metarun_tale_management_available())
            {
                requested_tale = story_book_metarun_index_at(
                    state.selected_run);
                if (requested_tale >= 0 && requested_tale != current_run) {
                    tale_request = STORY_BOOK_TALE_REQUEST_LOAD;
                    done = true;
                }
                continue;
            }

            if (!run_mode_is_blitz() && clicked >= STORY_BOOK_MINOR_BASE
                && clicked < STORY_BOOK_MINOR_BASE + METAR_CURSE_SLOTS)
            {
                (void)metarun_inline_choose_minor_blessing(
                    clicked - STORY_BOOK_MINOR_BASE);
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (!run_mode_is_blitz() && clicked >= STORY_BOOK_MAJOR_BASE
                && clicked < STORY_BOOK_MAJOR_BASE + major_blessing_capacity())
            {
                (void)metarun_inline_choose_major_blessing(
                    clicked - STORY_BOOK_MAJOR_BASE);
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (!run_mode_is_blitz() && clicked >= STORY_BOOK_REMOVE_CURSE_BASE
                && clicked < STORY_BOOK_REMOVE_CURSE_BASE + METAR_CURSE_SLOTS)
            {
                (void)metarun_inline_remove_curse(
                    clicked - STORY_BOOK_REMOVE_CURSE_BASE);
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (!run_mode_is_blitz() && clicked >= STORY_BOOK_THRESHOLD_BASE
                && clicked < STORY_BOOK_THRESHOLD_BASE
                    + METARUN_BLESSING_THRESHOLD_MODE_MAX)
            {
                metarun_blessing_threshold_mode mode =
                    (metarun_blessing_threshold_mode)(clicked
                        - STORY_BOOK_THRESHOLD_BASE);
                metarun_set_threshold_mode(&metar, mode);
                update_blessing_ledger(&metar);
                if (!sync_current_metarun_slot(false))
                    log_warn("Inline threshold change failed to sync metarun");
                save_metaruns();
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (!run_mode_is_blitz() && clicked >= STORY_BOOK_DIFFICULTY_BASE
                && clicked < STORY_BOOK_DIFFICULTY_BASE + z_info->rt_max)
            {
                state.pending_difficulty = clicked
                    - STORY_BOOK_DIFFICULTY_BASE;
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (!run_mode_is_blitz()
                && clicked == STORY_BOOK_DIFFICULTY_CONFIRM) {
                if (state.pending_difficulty >= 0)
                    (void)metarun_set_difficulty_inline(
                        state.pending_difficulty);
                state.pending_difficulty = -1;
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked == STORY_BOOK_DIFFICULTY_CANCEL) {
                state.pending_difficulty = -1;
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked == STORY_BOOK_CURSES_EARLIER) {
                state.curse_offset = MAX(0, state.curse_offset - 5);
                state.selected_curse = -1;
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked == STORY_BOOK_CURSES_LATER) {
                state.curse_offset += 5;
                state.selected_curse = -1;
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked == STORY_BOOK_RUNS_NEWER) {
                state.run_offset = MAX(0, state.run_offset - 5);
                state.selected_run = state.run_offset;
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked == STORY_BOOK_RUNS_OLDER) {
                state.run_offset += 5;
                state.selected_run = state.run_offset;
                (void)story_book_sdl_build(startup_scene, &state, page);
                continue;
            }
            if (clicked == STORY_BOOK_BLITZ) {
                launch_blitz = true;
                done = true;
                continue;
            }
        } else if (key == UI_MENU_CLICK_WAKE_KEY) {
            ui_menu_click_clear();
            continue;
        }
        ui_menu_click_clear();

        if (steamdeck_controls_active())
            key = steamdeck_menu_key(key, '4', '6');
        if (sdl_character_sheet_screen_page_turning())
            continue;
        page = sdl_character_sheet_screen_select_page();
        page_count = sdl_character_sheet_screen_select_page_count();
        tales_page = sdl_character_sheet_screen_book_contents_page(
            STORY_BOOK_METARUNS);
        tales_section = tales_page >= 0 && page >= tales_page;
        if (tales_section
            && ((key == 'n' || key == 'N')
                || (steamdeck_controls_active()
                    && key == steamdeck_secondary_key()))
            && metarun_tale_management_available())
        {
            tale_request = STORY_BOOK_TALE_REQUEST_NEW;
            done = true;
        }
        else if (tales_section
            && (key == '\r' || key == '\n')
            && metarun_tale_management_available())
        {
            requested_tale = story_book_metarun_index_at(state.selected_run);
            if (requested_tale >= 0 && requested_tale != current_run) {
                tale_request = STORY_BOOK_TALE_REQUEST_LOAD;
                done = true;
            } else if (requested_tale == current_run)
                done = true;
        }
        else if (tales_section && metarun_max > 0
            && (key == '8' || key == 'k' || key == 'K' || key == '-'))
        {
            state.selected_run = MAX(0, state.selected_run - 1);
            (void)story_book_sdl_build(startup_scene, &state, page);
        }
        else if (tales_section && metarun_max > 0
            && (key == '2' || key == 'j' || key == 'J' || key == '+'))
        {
            state.selected_run = MIN(metarun_max - 1,
                state.selected_run + 1);
            (void)story_book_sdl_build(startup_scene, &state, page);
        }
        else if (!tales_section
            && (key == '8' || key == 'k' || key == 'K' || key == '-'))
        {
            (void)sdl_character_sheet_screen_scroll_book(-1);
        }
        else if (!tales_section
            && (key == '2' || key == 'j' || key == 'J' || key == '+'))
        {
            (void)sdl_character_sheet_screen_scroll_book(+1);
        }
        else if (key == ESCAPE || key == 'q' || key == 'Q')
            done = true;
        else if (key == '4' && page > 0)
            sdl_character_sheet_screen_begin_page_turn(-1);
        else if (key == '6' || key == ' ' || key == '\r' || key == '\n') {
            if (page < page_count - 1)
                sdl_character_sheet_screen_begin_page_turn(+1);
            else
                done = true;
        }
        }

        ui_menu_click_clear();
        restore_page = sdl_character_sheet_screen_select_page();
        sdl_character_sheet_screen_hide();
        screen_pop_supporting_panes_hidden();
        if (!startup_scene)
            screen_load();
        if (tale_request == STORY_BOOK_TALE_REQUEST_NEW) {
            tale_action_ok = metarun_create_tale();
            if (tale_action_ok && startup_scene && !run_mode_is_blitz()) {
                print_story_intro();
                metarun_created = false;
            }
        } else if (tale_request == STORY_BOOK_TALE_REQUEST_LOAD) {
            tale_action_ok = metarun_activate_tale(requested_tale);
        } else if (launch_blitz) {
            run_mode_set_pending(RUN_MODE_BLITZ);
            run_mode_set_current(RUN_MODE_BLITZ);
        }
        if (!tale_action_ok) {
            story_book_show_tale_action_error();
            continue;
        }
        return;
    }
}

void print_metarun_stats(void)
{
    bool startup_scene;

    refresh_current_metar_score();
    if (current_run < 0 || current_run >= metarun_max)
    {
        log_warn("Cannot open Tale Statistics without current tale data");
        return;
    }

    startup_scene = (!character_generated || !p_ptr || !p_ptr->playing);
    story_book_show_sdl(startup_scene);
}
