/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */
#include "angband.h"
#include "score/score_entry.h"
#include "score/score_file_compat.h"
#include "score/score_io.h"
#include "score/score_logic.h"
#include "score/score_paths.h"
#include "score/score_runs.h"
#include "fs/file.h"
#include "fs/savefile-name.h"
#include "fs/path.h"
#include "log/log.h"
#include "metarun.h"
#include "player/killer.h"
#include "platform.h"
#include "externs.h"
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static time_t death_time = (time_t)0;
static int score_idx = -1;

void score_entry_set_death_time(time_t when)
{
    death_time = when;
}

time_t score_entry_death_time(void)
{
    return death_time;
}

bool score_entry_has_committed_current(void)
{
    return score_idx != -1;
}

static const int race_priority[] = {
    3, //Sindar
    2, //Finarfin
    1, //Fingolfin
    5, //Dwarve
    6, //Edain
    0 //Fëanor
};

#define RACE_PRIORITIES (sizeof(race_priority) / sizeof(race_priority[0]))

bool highscore_is_empty(void)
{
    bool opened_here = false;
    
    /* Open the file on-demand (read-only) */
    if (!highscore_fd) {
        char buf[1024];
        build_current_score_path(buf, sizeof(buf));
        safe_setuid_grab();
        highscore_fd = score_file_open(buf, O_RDONLY);
        safe_setuid_drop();
        if (!highscore_fd) {
            log_debug("highscore_is_empty: cannot open scores file, treating as empty");
            return true; /* File doesn't exist = empty = first time */
        }
        opened_here = true;
    }
    
    /* Check entry count from header */
    bool is_empty = (scores_file_entry_count == 0);
    if (opened_here) { SDL_CloseIO(highscore_fd); highscore_fd = NULL; }
    log_debug("highscore_is_empty: entry_count=%u, returning %s", 
              scores_file_entry_count, is_empty ? "true" : "false");
    return is_empty;
}


static int race_has_character(uint16_t race, uint16_t character)
{
    if (character >= z_info->c_max) return 0;
    const uint16_t word  = character / 32U;
    const uint16_t shift = character % 32U;
    return (p_info[race].choice[word] & (1U << shift)) != 0U;
}

static int parse_score_id(const char field[3])
{
    if (!field)
        return -1;
    if (!isdigit((unsigned char)field[0]) || !isdigit((unsigned char)field[1]))
        return -1;
    return (field[0] - '0') * 10 + (field[1] - '0');
}

/* ------------------------------------------------------------------ */
/* helper - build a dummy hi-score entry so we can immediately kill it */
static void build_dummy_entry(high_score *e, uint16_t race, uint16_t character)
{
    memset(e, 0, sizeof(*e));

    /* score / gold / turns are all zero so the entry will sort last   */
    strnfmt(e->what, sizeof e->what, "%s",
            "Hero of the First Age");

    /* 15-char player name - character name fits nicely */
    const char *hname = c_name + c_info[character].name;
    strnfmt(e->who,  sizeof e->who,  "%-.15s", hname);

    /* race & character: two digits each, zero-padded                       */
    strnfmt(e->p_r,  sizeof e->p_r,  "%02u", race);
    strnfmt(e->p_h,  sizeof e->p_h,  "%02u", character);

    /* Save the date in standard encoded form */
    time_t now = time(NULL);
    strftime(e->day, sizeof(e->day), "@%Y%m%d",
        localtime(&now));

    /* immediate cause of death - will be overwritten below anyway      */
    strnfmt(e->how, sizeof e->how, "%s", op_ptr->base_name);
}


/*
 * Prints a nice comma spaced natural number
 */
void comma_number(char* output, int number)
{
    if (number >= 1000000)
    {
        sprintf(output, "%d,%03d,%03d", number / 1000000,
            (number % 1000000) / 1000, number % 1000);
    }
    else if (number >= 1000)
    {
        sprintf(output, "%d,%03d", number / 1000, number % 1000);
    }
    else
    {
        sprintf(output, "%d", number);
    }
}

/*
 * Converts a number into the three letter code of a month
 */
void atomonth(int number, char* output)
{
    switch (number)
    {
    case 1:
        sprintf(output, "Jan");
        break;
    case 2:
        sprintf(output, "Feb");
        break;
    case 3:
        sprintf(output, "Mar");
        break;
    case 4:
        sprintf(output, "Apr");
        break;
    case 5:
        sprintf(output, "May");
        break;
    case 6:
        sprintf(output, "Jun");
        break;
    case 7:
        sprintf(output, "Jul");
        break;
    case 8:
        sprintf(output, "Aug");
        break;
    case 9:
        sprintf(output, "Sep");
        break;
    case 10:
        sprintf(output, "Oct");
        break;
    case 11:
        sprintf(output, "Nov");
        break;
    case 12:
        sprintf(output, "Dec");
        break;
    }
}

/*
 * Display a single score.
 * Assumes the high score list is already open.
 */




/*
 * Display the scores in a given range.
 * Assumes the high score list is already open.
 * Only five entries per line, too much info.
 *
 * Mega-Hack -- allow "fake" entry at the given position.
 */


/* Show 20 compact entries per page ---------------------------------- */


/*
 * Hack -- Display the scores in a given range and quit.
 *
 * This function is only called from "main.c" when the user asks
 * to see the "high scores".
 */


/* Public entry - compact list */




int silmarils_possessed(void)
{
    int silmarils = 0;
    int i;

    for (i = 0; i < INVEN_TOTAL; i++)
    {
        if (((&inventory[i])->tval == TV_LIGHT)
            && ((&inventory[i])->sval == SV_LIGHT_SILMARIL))
            silmarils += (&inventory[i])->number;
        if ((&inventory[i])->name1 == ART_MORGOTH_1)
            silmarils += 1;
        if ((&inventory[i])->name1 == ART_MORGOTH_2)
            silmarils += 2;
        if ((&inventory[i])->name1 == ART_MORGOTH_3)
            silmarils += 3;
    }

    return silmarils;
}

/*
 * Checks if the player has Morgoth's crown (any version) in inventory
 * Returns the crown artifact number (ART_MORGOTH_0-3) or 0 if not found
 */
int has_iron_crown(void)
{
    int i;

    for (i = 0; i < INVEN_TOTAL; i++)
    {
        int name1 = (&inventory[i])->name1;
        if ((name1 >= ART_MORGOTH_0) && (name1 <= ART_MORGOTH_3))
        {
            return name1;  // Return which crown variant they have
        }
    }

    return 0;  // No crown
}

/*
 * Creates a score record for the player
 */
errr create_score(high_score* the_score)
{
    /* Clear the record */
    memset(the_score, 0, sizeof(high_score));

    /* Save the version */
    strnfmt(the_score->what, sizeof(the_score->what), "%s", VERSION_STRING);

    /* Store the net curse count (curses - blessings)
     * curse_stacks[i] > 0 means curses, < 0 means blessings, so sum gives net value */
    int curse_total = 0;
    for (int id = 0; id < METAR_CURSE_SLOTS; ++id)
    {
        curse_total += CURSE_GET(id);
    }
    strnfmt(the_score->pts, sizeof(the_score->pts), "%4d", curse_total);

    /* Save the current player turn */
    strnfmt(
        the_score->turns, sizeof(the_score->turns), "%9lu", (long)playerturn);
    the_score->turns[9] = '\0';

    /* Save the date in standard encoded form */
    strftime(the_score->day, sizeof(the_score->day), "@%Y%m%d",
        localtime(&death_time));

    /* Save the player name (15 chars) - fall back to base_name to avoid empty live entries */
    const char* score_name = op_ptr->full_name;
    if (!score_name || !score_name[0]) {
        score_name = op_ptr->base_name[0] ? op_ptr->base_name : "nameless";
        log_warn("create_score: full_name empty, using fallback '%s' for score entry", score_name);
    }
    strnfmt(the_score->who, sizeof(the_score->who), "%-.15s", score_name);

    /* Save the player info XXX XXX XXX */
    strnfmt(the_score->uid, sizeof(the_score->uid), "%7u", player_uid);
    strnfmt(the_score->p_r, sizeof(the_score->p_r), "%2d", p_ptr->prace);
    strnfmt(the_score->p_h, sizeof(the_score->p_h), "%2d", p_ptr->pcharacter);

    /* Save the level and such */
    strnfmt(
        the_score->cur_dun, sizeof(the_score->cur_dun), "%3d", p_ptr->depth);
    the_score->cur_dun[3] = '\0';
    strnfmt(the_score->max_dun, sizeof(the_score->max_dun), "%3d",
        p_ptr->max_depth);
    the_score->max_dun[3] = '\0';

    /* Save unique monsters killed count */
    int uniques_killed = unique_bane_type_killed();
    strnfmt(the_score->cur_lev, sizeof(the_score->cur_lev), "%3d", uniques_killed);
    the_score->cur_lev[3] = '\0';

    /* Save the cause of death (49 chars) */
    strnfmt(the_score->how, sizeof(the_score->how), "%-.49s", p_ptr->died_from);

    /* Save the number of silmarils, whether morgoth is slain, whether the
     * player has escaped */
    int recorded_silmarils = silmarils_possessed();
    if (p_ptr->morgoth_slain && recorded_silmarils < 3)
        recorded_silmarils = 3;
    strnfmt(the_score->silmarils, sizeof(the_score->silmarils), "%1d",
        recorded_silmarils);
    the_score->silmarils[1] = '\0';

    if (p_ptr->morgoth_slain)
    {
        strnfmt(
            the_score->morgoth_slain, sizeof(the_score->morgoth_slain), "t");
    }
    else
    {
        strnfmt(
            the_score->morgoth_slain, sizeof(the_score->morgoth_slain), "f");
    }
    if (p_ptr->escaped)
    {
        strnfmt(the_score->escaped, sizeof(the_score->escaped), "t");
    }
    else
    {
        strnfmt(the_score->escaped, sizeof(the_score->escaped), "f");
    }

    return (0);
}

/*
 * Enters a player's name on a hi-score table, if "legal".
 *
 * Assumes "signals_ignore_tstp()" has been called.
 */
errr score_entry_enter(high_score* the_score)
{
#ifndef SCORE_CHEATERS
    int j;
#endif /* SCORE_CHEATERS */

    /* No score file */
    if (!highscore_fd)
    {
        Term_putstr(15, 8, -1, TERM_L_DARK, "(no high score file found)");
        return (0);
    }

#ifndef SCORE_WIZARDS
    /* Wizard-mode pre-empts scoring */
    if (p_ptr->noscore & 0x000F)
    {
        Term_putstr(15, 8, -1, TERM_L_DARK, "(no high score for wizards)");
        score_idx = -1;
        return (0);
    }
#endif

    /* Hack -- Interupted */
    if (!p_ptr->escaped && streq(p_ptr->died_from, "Interrupting"))
    {
        Term_putstr(15, 8, -1, TERM_L_DARK, "(no high score when interrupted)");
        score_idx = -1;
        return (0);
    }

    /* Allow recording of voluntary death ("their own hand").
       This ensures aborted characters are written to the score file and
       won't be treated as alive on the next startup. */

#ifndef SCORE_CHEATERS
    /* Cheaters are not scored */
    for (j = OPT_SCORE; j < OPT_MAX; ++j)
    {
        if (!op_ptr->opt[j])
            continue;

        Term_putstr(15, 8, -1, TERM_L_DARK, "(no high score when cheating)");
        score_idx = -1;
        return (0);
    }

    // People who cheated death are not scored
    if (p_ptr->noscore & 0x0001)
    {
        Term_putstr(15, 8, -1, TERM_L_DARK, "(no high score when cheating)");
        score_idx = -1;
        return (0);
    }
#endif /* SCORE_CHEATERS */

    /* Grab permissions */
    safe_setuid_grab();

    /* Lock (for writing) the highscore file, or fail */
    /* TODO: File locking not supported with FILE* - temporarily disabled */
    /* if (fd_lock(highscore_fd, F_WRLCK)) */
    if (0)
        return (1);

    /* Drop permissions */
    safe_setuid_drop();

    /* Add a new entry to the score list, see where it went */
    score_idx = highscore_add(the_score);

    /* Close the file after writing.
     * Functions that need to read scores will open the file fresh. */
    if (highscore_fd)
    {
        /* Grab permissions */
        safe_setuid_grab();
        
        SDL_CloseIO(highscore_fd);
        highscore_fd = NULL;
        
        /* Drop permissions */
        safe_setuid_drop();
    }

    /* Grab permissions */
    safe_setuid_grab();

    /* Unlock the highscore file, or fail */
    /* TODO: File locking not supported with FILE* - temporarily disabled */
    /* if (fd_lock(highscore_fd, F_UNLCK)) */
    if (0)
        return (1);

    /* Drop permissions */
    safe_setuid_drop();

    /* Success */
    return (0);
}

/*
 * Enters a player's name on a hi-score table, if "legal", and in any
 * case, displays some relevant portion of the high score list.
 *
 * Assumes "signals_ignore_tstp()" has been called.
 */

/*
 * Predict the player's location, and display it.
 */
bool build_live_preview_score(high_score* out)
{
    /* Final Look borrows the live dungeon UI flags, but the run has already
     * been finalized and recorded.  Never synthesize another alive snapshot
     * from that presentation state. */
    if (!out || !character_generated || death_spectator_active())
        return false;

    char saved_how[sizeof(p_ptr->died_from)];
    SDL_strlcpy(saved_how, p_ptr->died_from, sizeof(saved_how));

    time_t previous_time = death_time;
    time_t now = time(NULL);
    if (now != (time_t)-1)
        death_time = now;

    SDL_strlcpy(p_ptr->died_from, "(alive and well)", sizeof(p_ptr->died_from));

    bool ok = (create_score(out) == 0);

    SDL_strlcpy(p_ptr->died_from, saved_how, sizeof(saved_how));
    death_time = previous_time;

    return ok;
}

#if defined(__ANDROID__) || defined(SIL_IOS)
bool mobile_autosave_game(cptr reason)
{
    static bool in_progress = false;

    if (in_progress)
        return false;

    if (!character_generated || !p_ptr || p_ptr->is_dead || !p_ptr->playing
        || death_spectator_active())
        return false;

    if (DEPLOYMENT && p_ptr->game_type != 0)
    {
        log_info("mobile autosave skipped during tutorial mode (%s)",
            reason ? reason : "unspecified");
        return false;
    }

    if (!savefile[0] && op_ptr && op_ptr->full_name[0])
        process_player_name(true);

    if (!savefile[0])
    {
        log_warn("mobile autosave skipped: savefile path is empty (%s)",
            reason ? reason : "unspecified");
        return false;
    }

    in_progress = true;

    char saved_how[sizeof(p_ptr->died_from)];
    SDL_strlcpy(saved_how, p_ptr->died_from, sizeof(saved_how));
    SDL_strlcpy(p_ptr->died_from, "(alive and well)", sizeof(p_ptr->died_from));

    log_info("mobile autosave starting (%s) -> '%s'",
        reason ? reason : "unspecified", savefile);
    bool ok = save_player();

    SDL_strlcpy(p_ptr->died_from, saved_how, sizeof(p_ptr->died_from));

    if (ok)
    {
        upsert_live_score_on_save();

        high_score live_score;
        if (build_live_preview_score(&live_score))
        {
            time_t now = time(NULL);
            if (!score_runs_record_current_run(&live_score, now, SCORE_RECORD_ALIVE))
            {
                log_warn("mobile autosave: failed to persist live run snapshot for '%s'",
                    op_ptr->full_name);
            }
        }

        log_info("mobile autosave completed (%s)", reason ? reason : "unspecified");
    }
    else
    {
        log_error("mobile autosave failed (%s)", reason ? reason : "unspecified");
    }

    in_progress = false;
    return ok;
}
#else
bool mobile_autosave_game(cptr reason)
{
    (void)reason;
    return false;
}
#endif




/* Display the high score table (optionally long form) without committing a new score.
 * If character_generated is true and player is alive, show predicted placement.
 */























/*  Returns NULL when nothing was slain, or a static string with the
 *  character name of the slain hero.  If @do_roll is false, the caller has
 *  already performed the RNG check and we kill un-conditionally.       */
const char *kinslayer_try_kill(uint8_t n_sils, bool do_roll)
{
    log_info("Kinslayer attempt: n_sils=%u", n_sils);

    /* 1) Probability check */
    static const int pct_tab[4] = { 0, 20, 50, 95 };
    if (do_roll) {
        if (n_sils == 0) return NULL;
        if (n_sils > 3)  n_sils = 3;
        int roll = rand_int(100);
        if (roll >= pct_tab[n_sils]) {
            log_debug("Kinslayer roll failed: %d >= %d (n_sils=%d)", roll, pct_tab[n_sils], n_sils);
            return NULL;
        }
    }

    /* 2) Build path to scores.raw */
    char score_path[1024];
    score_build_meta_path(score_path, sizeof(score_path), "scores.raw");

    /* 3) Open global highscore_fd (version-aware) if not already open */
    if (!highscore_fd) {
        log_trace("highscore_fd < 0, opening %s (version-aware)", score_path);
        safe_setuid_grab();
        highscore_fd = score_file_open(score_path, O_RDWR);
        safe_setuid_drop();
        if (!highscore_fd) {
            quit(format("Cannot open %s (%d)", score_path, errno));
            return NULL; /* NOTREACHED */
        }
        log_trace("opened highscore_fd (score file loaded)");
    }

    /* 4) Determine number of records (exclude header) */
    SDL_SeekIO(highscore_fd, 0, SDL_IO_SEEK_END);
    off_t file_end = SDL_TellIO(highscore_fd);
    off_t payload  = file_end - (off_t)sizeof(score_file_header);
    int n_recs = (int)(payload / (off_t)sizeof(high_score));
    if (scores_file_entry_count < (u32b)n_recs)
        n_recs = (int)scores_file_entry_count;
    log_trace("hi-score file size=%lld, payload=%lld, records=%d",
              (long long)file_end, (long long)payload, n_recs);

    /* 5) Build list of races with eligible characters and apply weighted selection */
    
    bool *hero_ineligible = calloc(z_info->c_max, sizeof(*hero_ineligible));
    if (!hero_ineligible) {
        safe_setuid_grab();
        if (SDL_CloseIO(highscore_fd) != 0)
            log_warn("fclose(highscore_fd) failed, errno=%d", errno);
        safe_setuid_drop();
        highscore_fd = NULL;
        quit("Out of memory in kinslayer_try_kill()");
    }

    if (n_recs > 0 && highscore_seek(0) == 0) {
        high_score entry;
        for (int r = 0; r < n_recs; ++r) {
            if (highscore_read(&entry)) break;
            int character = parse_score_id(entry.p_h);
            if (character < 0 || character >= (int)z_info->c_max)
                continue;
            bool escaped = (tolower((unsigned char)entry.escaped[0]) == 't');
            bool dead = (strcmp(entry.how, "(alive and well)") != 0);
            if (escaped || dead)
                hero_ineligible[character] = true;
        }
    }

    /* 5.a) First pass: identify which races have eligible characters */
    uint16_t eligible_races[RACE_PRIORITIES];
    size_t eligible_count = 0;
    
    for (size_t i = 0; i < RACE_PRIORITIES && eligible_count < RACE_PRIORITIES; ++i) {
        uint16_t race = race_priority[i];
        
        /* Check if this race has any eligible characters */
        bool has_eligible = false;
        for (uint16_t h = 0; h < z_info->c_max; ++h) {
            if (!race_has_character(race, h)) continue;
            if (hero_ineligible[h]) continue;
            const char *hname = c_name + c_info[h].name;
            if (strcmp(hname, op_ptr->base_name) == 0) continue;
            has_eligible = true;
            break;
        }
        
        if (has_eligible) {
            eligible_races[eligible_count++] = race;
            log_trace("race priority[%zu]=%u added to eligible list (position %zu)", 
                      i, race, eligible_count - 1);
        } else {
            log_trace("race priority[%zu]=%u has no eligible characters, skipping", i, race);
        }
    }
    
    if (eligible_count == 0) {
        log_debug("No eligible races found - no kill performed");
        free(hero_ineligible);
        safe_setuid_grab();
        if (SDL_CloseIO(highscore_fd) != 0)
            log_warn("fclose(highscore_fd) failed, errno=%d", errno);
        safe_setuid_drop();
        highscore_fd = NULL;
        return NULL;
    }
    
    /* 5.b) Apply weighted random selection to first 3 eligible races */
    /* Weights: 50%, 30%, 20% (normalized to 100) */
    static const int weights[3] = { 50, 30, 20 };
    int total_weight = 0;
    int applicable_races = (eligible_count < 3) ? (int)eligible_count : 3;
    
    for (int i = 0; i < applicable_races; ++i) {
        total_weight += weights[i];
    }
    
    /* Select race using weighted random */
    int roll = rand_int(total_weight);
    int cumulative = 0;
    uint16_t selected_race = eligible_races[0]; /* fallback */
    
    for (int i = 0; i < applicable_races; ++i) {
        cumulative += weights[i];
        if (roll < cumulative) {
            selected_race = eligible_races[i];
            log_info("Weighted race selection: chose race %u (position %d, weight %d%%)", 
                     selected_race, i, weights[i]);
            break;
        }
    }
    
    /* 5.c) Now process the selected race */
    uint16_t race = selected_race;
    log_trace("Processing selected race=%u", race);
    
    /* Build pool of eligible characters for selected race */
    uint16_t *pool = malloc(z_info->c_max * sizeof *pool);
    if (!pool) {
        free(hero_ineligible);
        SDL_CloseIO(highscore_fd);
        quit("Out of memory in kinslayer_try_kill()");
    }
    size_t pool_n = 0;
    for (uint16_t h = 0; h < z_info->c_max; ++h) {
        if (!race_has_character(race, h)) continue;
        if (hero_ineligible[h]) continue;
        const char *hname = c_name + c_info[h].name;
        if (strcmp(hname, op_ptr->base_name) == 0) continue;
        pool[pool_n++] = h;
    }
    log_trace("race %u: %zu eligible characters", race, pool_n);
    if (pool_n == 0) {
        free(pool);
        free(hero_ineligible);
        safe_setuid_grab();
        if (SDL_CloseIO(highscore_fd) != 0)
            log_warn("fclose(highscore_fd) failed, errno=%d", errno);
        safe_setuid_drop();
        highscore_fd = NULL;
        return NULL;
    }

    /* 5.d) Pick one character */
    uint16_t character_sel = pool[rand_int((int)pool_n)];
    const char *hname = c_name + c_info[character_sel].name;
    free(pool);
    pool = NULL;
    free(hero_ineligible);
    hero_ineligible = NULL;
    log_info("Kinslayer selected character %u (%s) for elimination", character_sel, hname);

    /* 5.e) Scan for existing entry */
    int hit = -1;
    high_score entry;
    for (int r = 0; r < n_recs; ++r) {
        if (highscore_seek(r)) break;
        if (highscore_read(&entry)) break;
        if (entry.p_r[0] == '0' + (race/10) &&
            entry.p_r[1] == '0' + (race%10) &&
            entry.p_h[0] == '0' + (character_sel/10) &&
            entry.p_h[1] == '0' + (character_sel%10)) {
            hit = r;
            break;
        }
    }
    log_trace("scan: entry_offset=%d", hit);

    if (hit >= 0) {
        /* 5.f) Found - check alive AND not escaped */
            if (highscore_dead(entry.who)) {
                log_debug("hero already dead - no kill performed");
                if (pool) free(pool);
                if (hero_ineligible) free(hero_ineligible);
                safe_setuid_grab();
                if (SDL_CloseIO(highscore_fd) != 0) {
                    log_warn("fclose(highscore_fd) failed, errno=%d", errno);
                }
                safe_setuid_drop();
                highscore_fd = NULL;
                return NULL;
            }
            /* Also check if hero has escaped */
            if (entry.escaped[0] == 't') {
                log_debug("hero has escaped - no kill performed");
                if (pool) free(pool);
                if (hero_ineligible) free(hero_ineligible);
                safe_setuid_grab();
                if (SDL_CloseIO(highscore_fd) != 0) {
                    log_warn("fclose(highscore_fd) failed, errno=%d", errno);
                }
                safe_setuid_drop();
                highscore_fd = NULL;
                return NULL;
            }
            /* kill existing */
            if (highscore_seek(hit) == 0 && highscore_read(&entry) == 0) {
                strnfmt(entry.how, sizeof entry.how, "%s", op_ptr->base_name);
                highscore_seek(hit);
                highscore_write(&entry);
                log_info("Kinslayer killed existing hero: \"%s\"", entry.who);
            } else {
                log_warn("Failed to re-read existing entry at slot %d", hit);
            }
        }
        else {
            /* 5.e) No record - insert dummy */
            high_score dummy;
            build_dummy_entry(&dummy, race, character_sel);
            log_trace("no existing record - inserting dummy \"%s\"", dummy.who);

            /* position for add */
            highscore_seek(0);
            int slot = highscore_add(&dummy);
            if (slot < 0)
                log_error("highscore_add() failed");
            else
                log_info("Kinslayer inserted dummy entry \"%s\" at slot %d",
                        dummy.who, slot);
        }

        /* 6) UI is now handled by metarun_update_on_exit() */
        static char killed_character[32];
        SDL_strlcpy(killed_character, hname, sizeof killed_character);

        /* 7) Close the descriptor and reset before returning */
        if (hero_ineligible) free(hero_ineligible);
        safe_setuid_grab();
        if (SDL_CloseIO(highscore_fd) != 0) {
            log_warn("fclose(highscore_fd) failed, errno=%d", errno);
        }
        safe_setuid_drop();
        highscore_fd = NULL;
        return killed_character;
}

/*
 * Hack -- Dump a character description file
 *
 * XXX XXX XXX Allow the "full" flag to dump additional info,
 * and trigger its usage from various places in the code.
 */
