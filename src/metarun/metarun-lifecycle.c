#include "angband.h"
#include "metarun-internal.h"
#include "metarun/metarun-files.h"

/* ------------------------------------------------------------------ */
/*  Standard "Press any key..." prompts - use enum, not raw strings     */
/* ------------------------------------------------------------------ */
typedef enum {
    PROMPT_CONTINUE_TALE,
    PROMPT_FACE_TEMPTATION,
    PROMPT_CONTINUE_GENERIC,
    PROMPT_FACE_ECHOES,
    PROMPT_CONCLUDE_TALE,
    PROMPT_WITNESS_CONSEQUENCES,
    PROMPT_RETURN_MIDDLE_EARTH
} prompt_t;

static const char *prompt_text[] = {
    "[Press any key to continue your tale]",
    "[Press any key to face temptation]",
    "[Press any key to continue]",
    "[Press any key to face the echoes]",
    "[Press any key to conclude your tale]",
    "[Press any key to witness the consequences]",
    "[Press any key to return to Middle-earth]"
};

static void wait_prompt(prompt_t id) {         /* tiny wrapper */
    wait_for_keypress_with_prompt(prompt_text[id]);
}

/*
 * Present the pre-Final-Look death poem on the SDL window canvas.  The screen
 * keeps the original staged fade timings and Esc-to-fast-forward behavior,
 * while the renderer owns wrapping and placement in semantic pixels.
 */
static bool show_death_poetry_semantic(cptr title, cptr body,
    cptr transition)
{
    const byte title_fade[] = { TERM_L_DARK, TERM_SLATE, TERM_RED };
    const byte paragraph_fade[] = {
        TERM_L_DARK, TERM_SLATE, TERM_L_WHITE, TERM_WHITE
    };
    const byte transition_fade[] = {
        TERM_L_DARK, TERM_SLATE, TERM_L_WHITE, TERM_L_BLUE
    };
    bool body_fast_forward = false;
    bool transition_fast_forward = false;

    if (!sdl_death_poetry_screen_begin(title, body, transition,
            prompt_text[PROMPT_RETURN_MIDDLE_EARTH]))
    {
        return false;
    }

    sdl_story_font_enable();

    for (int i = 0; i < (int)N_ELEMENTS(title_fade); i++)
    {
        sdl_death_poetry_screen_update(true, title_fade[i], false,
            TERM_WHITE, false, TERM_L_BLUE, false);
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 150);
    }
    Term_xtra(TERM_XTRA_DELAY, 500);

    for (int i = 0; i < (int)N_ELEMENTS(paragraph_fade); i++)
    {
        char ch;

        if (Term_inkey(&ch, false, false) == 0 && ch == ESCAPE)
        {
            body_fast_forward = true;
            break;
        }
        sdl_death_poetry_screen_update(true, TERM_RED, true,
            paragraph_fade[i], false, TERM_L_BLUE, false);
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 125);
    }
    sdl_death_poetry_screen_update(true, TERM_RED, true, TERM_WHITE,
        false, TERM_L_BLUE, false);
    Term_fresh();
    if (!body_fast_forward)
        Term_xtra(TERM_XTRA_DELAY, 1000);

    for (int i = 0; i < (int)N_ELEMENTS(transition_fade); i++)
    {
        char ch;

        if (Term_inkey(&ch, false, false) == 0 && ch == ESCAPE)
        {
            transition_fast_forward = true;
            break;
        }
        sdl_death_poetry_screen_update(true, TERM_RED, true, TERM_WHITE,
            true, transition_fade[i], false);
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 125);
    }
    sdl_death_poetry_screen_update(true, TERM_RED, true, TERM_WHITE,
        true, TERM_L_BLUE, false);
    Term_fresh();
    if (!transition_fast_forward)
        Term_xtra(TERM_XTRA_DELAY, 1000);

    sdl_death_poetry_screen_update(true, TERM_RED, true, TERM_WHITE,
        true, TERM_L_BLUE, true);
    Term_fresh();
    ui_key_wait_dismiss_begin('\r');
    metarun_wait_hidden();
    ui_key_wait_dismiss_clear();

    sdl_death_poetry_screen_hide();
    sdl_story_font_disable();
    return true;
}

/* ------------------------------------------------------------------
 * metarun_update_on_exit() - v5, 30 Jul 2025
 * ------------------------------------------------------------------
 * Implements the finalised story/logic flow discussed in chat:
 *   0.  Escape check (silmarils? gift-of-Eru?)
 *   1.  Escape-curse chooser UI
 *   2.  Victory banner & Silmaril paragraph
 *   3.  Temptation of Treachery (3 rolls - stolen Silmarils don't count)
 *   4.  Story Fragment (pure vs tainted, 1-3 jewels)
 *   5.  Echoes of Kinslaying / "Kill a Kin" (stop at first kill)
 *   6.  Final pause -> apply deferred effects
 *   7.  Persist silmaril/death counters, check run end, save
 *
 *  All narrative helpers (print_heading(), print_paragraph(),
 *  choose_escape_curses_ui(), kinslayer_try_kill(), etc.) are reused.
 * ------------------------------------------------------------------ */
static void announce_blessing_gain(int previous_points)
{
    int current_points = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
    if (current_points <= previous_points) return;
    int delta = current_points - previous_points;
    int available = current_points - metar.blessing_points_spent;
    if (available < 0) available = 0;
    msg_format("You gain %d blessing point%s. (%d available)",
               delta, (delta == 1) ? "" : "s", available);
    message_flush();
}

void metarun_update_on_exit(bool died, bool escaped, byte sil_count, s32b final_score)
{
    if (run_mode_is_blitz())
    {
        log_info("Suppressing metarun end-of-run processing for Blitz");
        if (escaped || (p_ptr && p_ptr->morgoth_slain && !died))
        {
            byte summary_sils = sil_count;
            if (p_ptr && p_ptr->morgoth_slain && summary_sils < 3)
                summary_sils = 3;
            blitz_show_end_summary(summary_sils);
        }
        return;
    }

    log_info("Metarun update: died=%s, escaped=%s, sil_count=%d, final_score=%ld",
             died ? "true" : "false", escaped ? "true" : "false", sil_count, (long)final_score);
    int blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;

    if (escaped)
    {
        sdl_music_play_main();
    }

    /* -------- Lineage flags -------------------------------------- */
    u32b character_flags = c_info[p_ptr->pcharacter].flags;
    u32b f_race  = p_info[p_ptr->prace].flags;

    bool has_gift_eru   = (character_flags | f_race) & RHF_GIFTERU;
    bool allow_treachery = (character_flags | f_race) & RHF_TREACHERY;
    bool allow_kinslay   = (character_flags | f_race) & RHF_KINSLAYER;

    bool escaped_with_sils = escaped && (sil_count > 0);
    bool fast_forward = false; // Track if user wants to skip fade effects
    bool morgoth_victory = (p_ptr->morgoth_slain && !escaped && !died);

    /* Treat as a death unless Eru intervenes */
    if (died && !has_gift_eru)
        metarun_increment_deaths();

    /* ------------------------------------------------------------- */
    /* 0. Branch: did we return with Silmarils?                      */
    /*    - any path that reaches here counts as a "run end" event  */
    /* ------------------------------------------------------------- */
    if (morgoth_victory)
    {
        log_info("Metarun: Morgoth victory branch (sil_count=%d)", sil_count);
        screen_save();
        Term_clear();

        print_heading_fade("Beyond Fate", TERM_YELLOW);
        print_paragraph_fade(
            "The illusion of Morgoth lies shattered at your feet.",
            TERM_WHITE, 4);
        print_paragraph_fade(
            "From Valinor, the Valar proclaim your impossible triumph and pour out their blessing.",
            TERM_L_BLUE, 7);
        print_paragraph_fade(
            "Though the true Dark Enemy waits beyond this trial, three Silmarils are counted to your name.",
            TERM_L_BLUE, 10);

        wait_prompt(PROMPT_CONTINUE_TALE);

        screen_load();

        byte awarded = (sil_count < 3) ? 3 : sil_count;
        metarun_gain_silmarils(awarded);
        log_info("Metarun: Morgoth victory awarded %d Silmarils (total now %d)",
                 awarded, (int)metar.silmarils);
        refresh_current_metar_score();
        compute_blessing_pool();
        announce_blessing_gain(blessing_points_before);
        blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
        check_run_end();
        metarun_save_persistent_settings();
        save_metaruns();
        return;
    }
    else if (died)
    {
        log_info("Player died - displaying death narrative");
        /*****  NEW DEATH-NARRATIVE *****/
        screen_save();
        /* A poetic death scene is a self-contained full-screen interlude;
         * do not leave the live combat/log panes over its text. */
        screen_push_supporting_panes_hidden();
        screen_push_touch_pane_hidden();
        Term_clear();

        /* Pick correct sequence number: 0 when Gift-of-Eru fires,
         * otherwise 1-based death counter that was just incremented. */
        byte target_order = has_gift_eru ? 0 : metar.deaths;

        /* Build a pool of candidate story entries.                    */
        int *pool = mem_alloc_array(z_info->st_max, int);
        int pool_sz = 0;
        if (!pool) {
            screen_pop_touch_pane_hidden();
            screen_pop_supporting_panes_hidden();
            screen_load();                 /* restore game view            */
            u32b pool_before = metar.fallen_score_total;
            refresh_current_metar_score();
            compute_blessing_pool();
            if (final_score > 0 && metar.fallen_score_total == pool_before) {
                metar.fallen_score_total += (u32b)final_score;
                update_blessing_ledger(&metar);
                (void)sync_current_metarun_slot(false);
            }
            announce_blessing_gain(blessing_points_before);
            blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
            check_run_end();
            save_metaruns();
            return;
        }
        for (int i = 0; i < z_info->st_max && pool; i++) {
            story_type *st = &st_info[i];
            if (!st->name)            continue;                /* unused slot   */
            if (st->st_type != 1)     continue;                /* not "death"   */
            if (st->order != target_order) continue;           /* wrong order   */
            if (st->runtypes &&
               !(st->runtypes & (1u << metar.type))) continue; /* wrong run-type*/
            pool[pool_sz++] = i;
        }

        /* Fallback - allow any order-0 message if nothing matched.   */
        if (!pool_sz && target_order) {
            for (int i = 0; i < z_info->st_max && pool; i++) {
                story_type *st = &st_info[i];
                if (!st->name || st->st_type != 1) continue;
                if (st->order != 0)   continue;
                if (st->runtypes &&
                   !(st->runtypes & (1u << metar.type))) continue;
                pool[pool_sz++] = i;
            }
        }

        /* Display the chosen fragment with the usual fade-in style.  */
        if (pool_sz) {
            story_type *pick = &st_info[ pool[rand_int(pool_sz)] ];
            cptr title = st_name + pick->name;
            cptr text  = st_text + pick->text;
            char transition_text[256];
            strnfmt(transition_text, sizeof(transition_text),
                    "The hero whose mantle you took has fallen, their tale ends in shadow. "
                    "Yet your spirit returns, for the Valar's trial is not yet complete.");

            if (!show_death_poetry_semantic(title, text, transition_text))
            {
                /* Non-SDL fallback retains the original terminal-grid scene. */
                print_heading_fade(title, TERM_RED);
                print_paragraph_fade(text, TERM_WHITE, 4);

                if (!fast_forward
                    && !print_paragraph_fade(transition_text, TERM_L_BLUE, 8))
                {
                    fast_forward = true;
                }
                else if (fast_forward)
                    print_paragraph(transition_text, TERM_L_BLUE);
                wait_prompt(PROMPT_RETURN_MIDDLE_EARTH);
            }
        }

        screen_pop_touch_pane_hidden();
        screen_pop_supporting_panes_hidden();
        screen_load();                 /* restore game view            */
        pool = mem_free(pool);
        u32b pool_before = metar.fallen_score_total;
        refresh_current_metar_score();
        compute_blessing_pool();
        if (final_score > 0 && metar.fallen_score_total == pool_before) {
            metar.fallen_score_total += (u32b)final_score;
            update_blessing_ledger(&metar);
            (void)sync_current_metarun_slot(false);
        }
        announce_blessing_gain(blessing_points_before);
        blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
        check_run_end();
        save_metaruns();
        return;
    }
    else if (!escaped_with_sils) {
        log_debug("Player escaped without Silmarils - no narrative needed");
        refresh_current_metar_score();
        compute_blessing_pool();
        announce_blessing_gain(blessing_points_before);
        blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
        save_metaruns();
        return;                        /* no further narrative needed  */
    }

    /* ------------------------------------------------------------- */
    /*        Enhanced Narrative Path - escaped with >=1 Silmaril     */
    /* ------------------------------------------------------------- */
    log_info("Player escaped with %d Silmarils - displaying victory narrative", sil_count);
    screen_save();

    /* ============================================================= */
    /* SCENE 1: Escape Curse Selection                              */
    /* ============================================================= */
    int curse_count = sil_count;
    int chosen[4] = { -1, -1, -1, -1 };

    if (sil_count == 3) curse_count = 4;

    int chosen_cnt = choose_escape_curses_ui(curse_count, chosen);

    /* ============================================================= */
    /* SCENE 2: The Binding of Fate                                 */
    /* ============================================================= */
    if (chosen_cnt > 0)
    {
        print_heading_fade("The Binding of Fate", TERM_L_RED);

        for (int i = 0; i < chosen_cnt; ++i)
        {
            char buf[128];
            strnfmt(buf, sizeof buf,
                    "The curse of %s binds your fate.",
                    curse_display_name(chosen[i]));

            if (!fast_forward && print_paragraph_fade(buf, TERM_RED, 4 + i * 2))
            {
                // Continue with fade effects
            }
            else
            {
                fast_forward = true;
                print_paragraph(buf, TERM_RED);
            }
        }

        wait_prompt(PROMPT_CONTINUE_TALE);
        Term_clear();
    }

    /* ============================================================= */
    /* SCENE 3: Victory Declaration                                  */
    /* ============================================================= */
    print_heading_fade("Victory Amid Shadow", TERM_YELLOW);

    const char *victory_text;
    switch (sil_count)
    {
        case 1:
            victory_text = "You emerge victorious from darkness, one holy jewel blazing in your grasp. Morgoth's crown is diminished, yet hope is rekindled, though shadow lingers.";
            break;
        case 2:
            victory_text = "You escape triumphant, two Silmarils blazing fiercely in your hands. Morgoth roars in wrath; his pride is wounded deeply. Your spirit exults, yet your heart begins to feel their burning weight.";
            break;
        case 3:
            victory_text = "All three stolen stars blaze now in your hands; Morgoth's crown lies darkened. Such triumph has not been known since Fëanor himself dreamed it-but even as victory soars, your heart trembles beneath their burning glory.";
            break;
        default:
            victory_text = "You have achieved the impossible, claiming more Silmarils than should exist. Reality itself bends before your triumph.";
            break;
    }

    if (!fast_forward && !print_paragraph_fade(victory_text, TERM_WHITE, 4))
        fast_forward = true;
    else if (fast_forward)
        print_paragraph(victory_text, TERM_WHITE);

    if (allow_treachery)
        wait_prompt(PROMPT_FACE_TEMPTATION);
    else
        wait_prompt(PROMPT_CONTINUE_GENERIC);
    Term_clear();

    /* ============================================================= */
    /* SCENE 4: Temptation of Treachery (Enhanced Messages)        */
    /* ============================================================= */
    byte stolen = 0;
    if (allow_treachery)
    {
        static const int pct[3] = { 20, 50, 95 };

        /* Enhanced escalating treachery messages */
        static const char *success_msgs[3] = {
            "The first jewel shines brightly, its pure light uncorrupted. You master desire, choosing honor.",
            "The second jewel blazes defiant, temptation growing strong-but once more, you cling to honor.",
            "The third Silmaril's holy flame burns fiercely. Yet against all odds, your will resists corruption."
        };

        static const char *failure_msgs[3] = {
            "Greed whispers softly, and you listen. Secretly you withhold the jewel's light, betraying even yourself.",
            "Desire gnaws deeper; you falter, concealing its brilliance in shame, light darkened by your betrayal.",
            "Consumed by lust for its beauty, you claim it secretly, sealing its radiance from all others-a betrayal of all trust."
        };

        print_heading_fade("Temptation of Treachery", TERM_L_UMBER);

        int current_row = 4;

        for (int i = 0; i < sil_count; ++i)
        {
            bool fail = (rand_int(100) < pct[i]);
            if (fail) stolen++;

            const char *tempt_text = fail ? failure_msgs[i] : success_msgs[i];

            if (!fast_forward && !print_paragraph_fade(tempt_text, fail ? TERM_RED : TERM_WHITE, current_row))
                fast_forward = true;
            else if (fast_forward)
                print_paragraph(tempt_text, fail ? TERM_RED : TERM_WHITE);

            current_row += 3; // Space for next paragraph
        }

        if (stolen)
        {
            const char *shadow_text = "In shadows your deeds are recorded-tainted victory shall diminish the jewel's blessing.";
            if (!fast_forward && !print_paragraph_fade(shadow_text, TERM_L_DARK, current_row))
                fast_forward = true;
            else if (fast_forward)
                print_paragraph(shadow_text, TERM_L_DARK);
        }

        wait_prompt(PROMPT_CONTINUE_GENERIC);
        Term_clear();
    }

    byte final_sils = sil_count - stolen;
    bool treachery_occurred = (stolen > 0);

    /* ============================================================= */
    /* SCENE 5: The Weight of Victory                               */
    /* ============================================================= */
    print_heading_fade("The Weight of Victory", TERM_L_BLUE);

    const char *weight_text;
    if (!treachery_occurred)
    {
        const char *pure_frag[3] = {
            "A single star reclaimed, hope rekindled faintly in Middle-earth. Yet Morgoth laughs still, for two remain bound in shadow.",
            "Two jewels shine again beneath sky; Morgoth's power falters greatly. Yet you feel their brilliance burning; temptation ever near.",
            "All three jewels, radiant and pure, blaze again beneath stars. Morgoth's power breaks. Triumph is absolute, your soul soaring."
        };
        weight_text = pure_frag[final_sils-1];
    }
    else
    {
        const char *tainted_frag[3] = {
            "Though victory is yours, its memory darkens. Trust is fragile, and your spirit heavy beneath secret betrayal.",
            "Your heart trembles: Morgoth sees clearly your treachery-he smiles grimly, knowing darkness still dwells in you.",
            "Greatest triumph now mingled with darkest shame. Morgoth's laughter echoes bitterly-he senses your fall."
        };
        weight_text = tainted_frag[sil_count-1];
    }

    if (!fast_forward && !print_paragraph_fade(weight_text, treachery_occurred ? TERM_RED : TERM_L_WHITE, 4))
        fast_forward = true;
    else if (fast_forward)
        print_paragraph(weight_text, treachery_occurred ? TERM_RED : TERM_L_WHITE);

    if (allow_kinslay)
        wait_prompt(PROMPT_FACE_ECHOES);
    else
        wait_prompt(PROMPT_CONCLUDE_TALE);
    Term_clear();

    /* ============================================================= */
    /* SCENE 6: Echoes of Kinslaying (Enhanced Notifications)      */
    /* ============================================================= */
    bool deferred_kill[3] = { false, false, false };
    int kinslaying_victims = 0;
    if (allow_kinslay)
    {
        print_heading_fade("Echoes of Kinslaying", TERM_L_RED);

        static const int kin_pct[3] = { 20, 50, 95 };
        int current_row = 4;

        for (int k = 0; k < sil_count; ++k)
        {
            /* One roll only - use kin_pct[] here and *skip* the roll
             * inside kinslayer_try_kill() later.                        */
            /* one-shot probability (keep a local alias for UI)        */
            bool fail = (rand_int(100) < kin_pct[k]);
            deferred_kill[k] = fail;
            if (fail) kinslaying_victims++;

            const char *echo_text = NULL;
            switch (k)
            {
                case 0: echo_text = fail ?
                    "\"Alqualondë's Grief\"\nBlood stains starlit waves. Your hand remembers the swords at Alqualondë-first grief, first guilt." :
                    "The sorrow of Alqualondë passes over you-your spirit holds fast, blood unstained.";
                    break;
                case 1: echo_text = fail ?
                    "\"Ruin of Doriath\"\nAgain your hand recalls tragedy-fallen halls of Menegroth, Dior's blood shed beneath stolen starlight." :
                    "Memory of Doriath rises briefly, but your blade remains clean, honour upheld.";
                    break;
                case 2: echo_text = fail ?
                    "\"Tragedy at Sirion\"\nEchoes rise from Sirion-Elwing's flight, blood and betrayal. Once more your blade draws innocent blood, sealing doom anew." :
                    "You resist dark whispers recalling Sirion-your sword is stayed, mercy unbroken.";
                    break;
            }

            if (!fast_forward && !print_paragraph_fade(echo_text, fail ? TERM_RED : TERM_L_WHITE, current_row))
                fast_forward = true;
            else if (fast_forward)  print_paragraph(echo_text, fail ? TERM_RED : TERM_L_WHITE);

            current_row += 4; // Space for next echo

            /* Stop at first failure */
            if (fail) break;
        }

        if (kinslaying_victims > 0)
        {
            const char *doom_text = "Blood now stains your triumph, your fate forever woven with grief and shame.";
            if (!fast_forward && !print_paragraph_fade(doom_text, TERM_L_DARK, current_row))
                fast_forward = true;
            else if (fast_forward)
                print_paragraph(doom_text, TERM_L_DARK);
        }

        wait_prompt(PROMPT_CONCLUDE_TALE);
        Term_clear();
    }

    /* ============================================================= */
    /* SCENE 7: Final Summary                                       */
    /* ============================================================= */
    print_heading_fade("The Tale Concludes", TERM_YELLOW);

    char summary[256];
    strnfmt(summary, sizeof summary,
            "Your legend is written: %d Silmaril%s claimed, %s, %s.",
            final_sils,
            (final_sils == 1) ? "" : "s",
            treachery_occurred ? "tainted by treachery" : "pure of heart",
            (kinslaying_victims > 0) ? "stained by kinslaying" : "with honour intact");

    if (!fast_forward && !print_paragraph_fade(summary, TERM_L_GREEN, 4))
        fast_forward = true;
    else if (fast_forward)
        print_paragraph(summary, TERM_L_GREEN);

    bool has_post_summary_scene = allow_kinslay && (kinslaying_victims > 0);

    Term_xtra(TERM_XTRA_DELAY, 3000);
    if (has_post_summary_scene)
        Term_clear();

    /* ============================================================= */
    /* SCENE 8: Kinslaying Execution & Notifications               */
    /* ============================================================= */
    if (has_post_summary_scene)
    {
        /* Show kinslaying notifications BEFORE screen_load() */
        print_heading_fade("The Price of Blood", TERM_RED);

        char kill_msg[128];
        strnfmt(kill_msg, sizeof kill_msg,
                "Your kinslaying echoes through time. %d innocent%s will fall by your hand...",
                kinslaying_victims, (kinslaying_victims == 1) ? "" : "s");

        if (!fast_forward && !print_paragraph_fade(kill_msg, TERM_RED, 4))
            fast_forward = true;
        else if (fast_forward)
            print_paragraph(kill_msg, TERM_RED);

        wait_prompt(PROMPT_WITNESS_CONSEQUENCES);
    }

    /* ------------------------------------------------------------- */
    /*  SCENE 8-bis: actual executions with cinematic feedback       */
    /* ------------------------------------------------------------- */
    if (has_post_summary_scene) {
        Term_clear();
        print_heading_fade("Blood Is Demanded", TERM_RED);

        int row = 4;
        for (int k = 0; k < 3; k++) {
            if (!deferred_kill[k]) continue;

            const char *character =
                kinslayer_try_kill(k + 1, /*do_roll=*/false);
            if (!character) continue;               /* should not happen */

            metarun_increment_deaths();
            log_info("Metarun: kinslaying victim counted as death (%u total)", (unsigned)metar.deaths);

            char buf[96];
            strnfmt(buf, sizeof buf,
                    "A hero %s has fallen beneath your blade.", character);

            if (!fast_forward && !print_paragraph_fade(buf, TERM_RED, row))
                fast_forward = true;
            else if (fast_forward)
                print_paragraph(buf, TERM_RED);

            row += 3;
        }

        wait_prompt(PROMPT_RETURN_MIDDLE_EARTH);
    } else {
        /* no kinslaying scene - still give one clean exit prompt   */
        wait_prompt(PROMPT_RETURN_MIDDLE_EARTH);
    }

    metarun_gain_silmarils(final_sils);
    log_info("Added %d Silmarils to metarun total (now %d)", final_sils, metar.silmarils);
    refresh_current_metar_score();
    print_story(3, true);

    /* Restore the saved play-screen only after every narrative beat */
    screen_load();

    compute_blessing_pool();
    announce_blessing_gain(blessing_points_before);
    blessing_points_before = (metar.blessing_points < 0) ? 0 : metar.blessing_points;
    check_run_end();
    /* Save persistent settings when exiting */
    metarun_save_persistent_settings();

    /* Save metarun data (deaths, silmarils, etc.) */
    save_metaruns();
}


int required_survivor_target(int win_goal)
{
    int remaining_silmarils = win_goal - metar.silmarils;
    if (remaining_silmarils < 0) remaining_silmarils = 0;

    int required = 0;
    if (remaining_silmarils > 0) {
        required = (remaining_silmarils + 2) / 3;
        required += CURSE_GET(CUR_DEATH);
        if (required < 1) required = 1;
    }

    if (required < 0) required = 0;
    return required;
}


/* ======================  run-state logic  ====================== */
static void report_tale_rollover_failure(void)
{
    log_error("Unable to start the next Tale safely");
    if (metarun_tale_recovery_required())
        msg_print("The next Tale needs recovery. Restart before playing Story mode.");
    else
        msg_print("The next Tale could not be started; the current Tale remains active.");
    message_flush();
}

/* ------------------------------------------------------------------ *
 *  Decide whether the current run just ended, and react accordingly. *
 *  Message text adapts automatically if you set LOSECON_DEATHS = 1.  *
 *  Loss condition takes precedence over win condition.               *
 * ------------------------------------------------------------------ */
void check_run_end(void)
{
    int win_goal = WINCON_SILMARILS;   /* fallback */

    if (runtype_info && metar.type < z_info->rt_max)
    {
        win_goal = runtype_info[metar.type].win_con ? runtype_info[metar.type].win_con : WINCON_SILMARILS;
    }

    /* Keep blessing and survivor data aligned with the score file */
    compute_blessing_pool();
    int alive = metar.alive_characters;

    int remaining_silmarils = win_goal - metar.silmarils;
    if (remaining_silmarils < 0) remaining_silmarils = 0;

    int required_survivors = required_survivor_target(win_goal);

    /* Loss takes precedence over victory */
    if (alive < required_survivors) {
        log_info("Metarun DEFEAT: alive=%d required=%d (remaining silmarils=%d)",
                 alive, required_survivors, remaining_silmarils);

        screen_save();
        Term_clear();

        print_heading_fade("The Trial's End", TERM_RED);

        char defeat_text[256];
        strnfmt(defeat_text, sizeof defeat_text,
                "Only %d hero%s remain, yet %d must endure to reclaim the remaining Silmarils. "
                "This tale falls into shadow; begin anew to kindle hope once more.",
                alive, (alive == 1) ? "" : "es",
                required_survivors);

        print_paragraph_fade(defeat_text, TERM_WHITE, 4);

        wait_for_keypress_with_prompt("[Press any key to begin anew]");
        screen_load();

        if (!start_new_metarun())
            report_tale_rollover_failure();
        return;
    }

    if (metar.silmarils >= win_goal) {
        log_info("Metarun VICTORY: %d Silmarils collected (goal: %d)", metar.silmarils, win_goal);
        screen_save();
        Term_clear();

        print_heading_fade("The Trial's End", TERM_YELLOW);

        char victory_text[256];
        strnfmt(victory_text, sizeof victory_text,
                "%d Silmarils reclaimed from Morgoth's crown! "
                "Hope kindles anew; your long trial approaches its end. "
                "Yet one final ordeal awaits: your ultimate destiny, "
                "as your true self faces the Last Trial.",
                win_goal);

        print_paragraph_fade(victory_text, TERM_L_GREEN, 4);

        const char *implementation_note = "(This final trial is yet to be implemented.)";
        print_paragraph_fade(implementation_note, TERM_L_DARK, 8);

        wait_for_keypress_with_prompt("[Press any key to begin anew]");
        screen_load();

        if (!start_new_metarun())
            report_tale_rollover_failure();
    }
}





static bool metarun_next_id(u32b* next_id)
{
    u32b maximum = 0;

    if (!next_id || metarun_max >= SHRT_MAX)
        return false;
    for (s16b i = 0; metaruns && i < metarun_max; i++)
        maximum = MAX(maximum, metaruns[i].id);
    if (maximum == UINT32_MAX)
        return false;
    *next_id = maximum + 1;
    return true;
}

static metarun* metarun_allocate_grown_array(void)
{
    metarun* grown;

    if (metarun_max < 0 || metarun_max >= SHRT_MAX)
        return NULL;
    grown = mem_alloc_array(metarun_max + 1, metarun);
    if (grown && metaruns && metarun_max > 0)
        memcpy(grown, metaruns, sizeof(metarun) * metarun_max);
    return grown;
}

static bool metarun_needs_score_ledger(const metarun* tale)
{
    if (!tale)
        return false;
    return tale->deaths > 0 || tale->silmarils > 0 || tale->score > 0
        || tale->best_run_score > 0 || tale->fallen_score_total > 0
        || tale->completed_quests != 0 || tale->blessing_points > 0;
}

bool metarun_tale_management_available(void)
{
    int alive = 0;

    if (run_mode_is_blitz())
        return false;
    if (metarun_tale_recovery_required())
        return false;
    if (character_generated && p_ptr && p_ptr->playing
        && !death_spectator_active())
        return false;
    if (score_count_story_alive_entries_checked(&alive))
        return alive == 0;

    /* A never-played first Tale may not have created scores.raw yet.  A file
     * that exists but cannot be read is corruption and must fail closed. */
    return !score_story_ledger_exists()
        && !metarun_needs_score_ledger(&metar);
}

bool metarun_tale_recovery_required(void)
{
    return story_scorefile_switch_recovery_required();
}

static bool metarun_activation_stamp(u32b* stamp)
{
    u32b latest = 0;
    time_t now;

    if (!stamp)
        return false;
    for (s16b i = 0; metaruns && i < metarun_max; i++)
        latest = MAX(latest, metaruns[i].last_played);
    if (latest == UINT32_MAX) {
        log_error("Cannot activate another tale: last-played counter exhausted");
        return false;
    }
    now = time(NULL);
    *stamp = (now > 0 && (u64b)now <= UINT32_MAX
        && (u64b)now > (u64b)latest)
        ? (u32b)now : latest + 1;
    return true;
}

static void metarun_initialize_new_slot(metarun* entries, s16b idx,
    u32b new_id, u32b activation_time)
{
    reset_defaults(&entries[idx]);
    entries[idx].id = new_id;
    entries[idx].type = 0;
    entries[idx].last_played = activation_time;
}

static bool metarun_commit_new_slot(metarun* grown, u32b new_id,
    u32b activation_time)
{
    metarun* old = metaruns;
    metarun old_metar = metar;
    s16b old_max = metarun_max;
    s16b old_current = current_run;
    bool old_created = metarun_created;

    if (!grown)
        return false;

    metarun_initialize_new_slot(grown, old_max, new_id, activation_time);
    metaruns = grown;
    metarun_max = old_max + 1;
    current_run = old_max;
    metar = metaruns[current_run];
    metarun_created = true;
    apply_difficulty_curses(&metar);
    metarun_apply_runtime_effects();
    ensure_run_dir(&metar);

    if (save_metaruns() != 0) {
        bool ledger_ok;
        bool metadata_ok;
        bool journal_ok = false;

        log_error("New Tale %u became active but could not be persisted",
            (unsigned)new_id);
        metaruns = old;
        metarun_max = old_max;
        current_run = old_current;
        metar = old_metar;
        metarun_created = old_created;
        ledger_ok = restore_story_scorefile_for_tale(old_metar.id);
        metarun_apply_runtime_effects();
        metadata_ok = save_metaruns() == 0;
        if (ledger_ok && metadata_ok)
            journal_ok = finish_story_scorefile_switch();
        if (!ledger_ok || !metadata_ok || !journal_ok)
            log_error("Automatic Tale rollover remains pending for startup "
                "recovery");
        grown = mem_free(grown);
        return false;
    }
    if (!finish_story_scorefile_switch()) {
        log_error("Automatic Tale rollover is committed but its recovery "
            "journal could not be removed");
        if (old)
            old = mem_free(old);
        return false;
    }
    if (old)
        old = mem_free(old);
    log_info("New Tale %u created and initialized", (unsigned)new_id);
    return true;
}

static bool metarun_rollback_activation(s16b old_current,
    const metarun* old_metar, bool old_created, s16b changed_idx,
    const metarun* changed_before)
{
    bool ledger_ok;
    bool metadata_ok;

    if (changed_before && metaruns && changed_idx >= 0
        && changed_idx < metarun_max)
    {
        metaruns[changed_idx] = *changed_before;
    }
    current_run = old_current;
    metar = *old_metar;
    metarun_created = old_created;
    ledger_ok = restore_story_scorefile_for_tale(old_metar->id);
    metarun_apply_runtime_effects();
    metadata_ok = save_metaruns() == 0;
    if (ledger_ok && metadata_ok)
        return finish_story_scorefile_switch();
    return false;
}

bool metarun_activate_tale(s16b idx)
{
    metarun target_before;
    metarun old_metar;
    u32b activation_time;
    u32b outgoing_id;
    s16b old_current;
    bool old_created;
    bool allow_empty_outgoing;

    if (!metarun_tale_management_available()) {
        log_warn("Tale activation rejected while a character is running");
        return false;
    }
    if (!metaruns || idx < 0 || idx >= metarun_max)
        return false;
    if (idx == current_run)
        return true;
    if (!metarun_activation_stamp(&activation_time))
        return false;

    compute_blessing_pool();
    refresh_current_metar_score();
    if (!sync_current_metarun_slot(false))
        return false;

    old_current = current_run;
    old_metar = metar;
    old_created = metarun_created;
    target_before = metaruns[idx];
    outgoing_id = metar.id;
    allow_empty_outgoing = !metarun_needs_score_ledger(&metar);
    if (!switch_story_scorefile_between_tales(outgoing_id,
            target_before.id, false, allow_empty_outgoing))
    {
        return false;
    }

    current_run = idx;
    metar = metaruns[current_run];
    metar.last_played = activation_time;
    metarun_clamp_and_sync_quests(&metar);
    metarun_sanitize_blessing_economy(&metar);
    metarun_sanitize_major_blessing_bits(&metar);
    metarun_created = false;
    compute_blessing_pool();
    refresh_current_metar_score();
    metarun_apply_runtime_effects();
    ensure_run_dir(&metar);

    if (save_metaruns() != 0 || !finish_story_scorefile_switch()) {
        log_error("Tale %u activation failed; restoring Tale %u",
            (unsigned)metar.id, (unsigned)old_metar.id);
        if (!metarun_rollback_activation(old_current, &old_metar,
                old_created, idx, &target_before))
        {
            log_error("Tale activation rollback remains pending for startup "
                "recovery");
        }
        return false;
    }
    log_info("Activated Tale %u (array index %d)", (unsigned)metar.id, idx);
    return true;
}

bool metarun_create_tale(void)
{
    metarun* grown;
    metarun* old;
    metarun old_metar;
    u32b new_id;
    u32b activation_time;
    s16b old_max;
    s16b old_current;
    bool old_created;
    bool allow_empty_outgoing;

    if (!metarun_tale_management_available()) {
        log_warn("New tale creation rejected while a character is running");
        return false;
    }
    compute_blessing_pool();
    refresh_current_metar_score();
    if (!sync_current_metarun_slot(false) || !metarun_next_id(&new_id)
        || !metarun_activation_stamp(&activation_time))
    {
        return false;
    }
    grown = metarun_allocate_grown_array();
    if (!grown)
        return false;
    allow_empty_outgoing = !metarun_needs_score_ledger(&metar);
    if (!switch_story_scorefile_between_tales(metar.id, new_id, true,
            allow_empty_outgoing))
    {
        grown = mem_free(grown);
        return false;
    }

    old = metaruns;
    old_metar = metar;
    old_max = metarun_max;
    old_current = current_run;
    old_created = metarun_created;
    metarun_initialize_new_slot(grown, old_max, new_id, activation_time);
    metaruns = grown;
    metarun_max = old_max + 1;
    current_run = old_max;
    metar = metaruns[current_run];
    metarun_created = true;
    apply_difficulty_curses(&metar);
    metarun_apply_runtime_effects();
    ensure_run_dir(&metar);

    if (save_metaruns() != 0 || !finish_story_scorefile_switch()) {
        bool ledger_ok;
        bool metadata_ok;
        bool journal_ok = false;

        log_error("New Tale %u could not be committed; restoring Tale %u",
            (unsigned)new_id, (unsigned)old_metar.id);
        metaruns = old;
        metarun_max = old_max;
        current_run = old_current;
        metar = old_metar;
        metarun_created = old_created;
        ledger_ok = restore_story_scorefile_for_tale(old_metar.id);
        metadata_ok = save_metaruns() == 0;
        if (ledger_ok && metadata_ok)
            journal_ok = finish_story_scorefile_switch();
        if (!ledger_ok || !metadata_ok || !journal_ok)
            log_error("New Tale rollback remains pending for startup recovery");
        metarun_apply_runtime_effects();
        grown = mem_free(grown);
        return false;
    }

    if (old)
        old = mem_free(old);
    log_info("New Tale %u created and initialized", (unsigned)new_id);
    return true;
}

/* ------------------------------------------------------------------
 *  Start a brand-new meta-run after an automatic victory/defeat rollover.
 * ------------------------------------------------------------------ */
bool start_new_metarun(void)
{
    metarun* grown;
    u32b new_id;
    u32b activation_time;

    if (run_mode_is_blitz()) {
        log_warn("Ignoring automatic Tale rollover during Blitz");
        return false;
    }

    log_info("Starting new metarun (previous run ID: %d)", metar.id);
    log_debug("metarun: pre-finalize state (wizard=%d, noscore=0x%04X, savefile='%s')",
              p_ptr ? (p_ptr->wizard ? 1 : 0) : -1,
              p_ptr ? (unsigned)p_ptr->noscore : 0,
              savefile);

    if (!sync_current_metarun_slot(true)) {
        log_warn("metarun: unable to snapshot current run before rollover (idx=%d, max=%d)",
                 current_run, metarun_max);
    }
    if (!metarun_next_id(&new_id)
        || !metarun_activation_stamp(&activation_time))
        return false;
    grown = metarun_allocate_grown_array();
    if (!grown)
        return false;

     /* Before wiping scores for the next run, backup and clear save files */
     backup_and_clear_saves();

     /* Before wiping scores for the next run, finalize current ones:
         - mark all alive entries as dead by their own hand
         - save any corresponding savefiles as dead
         Then archive/clear the score file so the next run starts clean. */
     metarun_finalize_scores_and_saves();
     if (!begin_story_scorefile_rollover(metar.id, new_id,
             !metarun_needs_score_ledger(&metar))) {
         grown = mem_free(grown);
         return false;
     }

    /* Hard purge the current savefile if this was a noscore wizard/debug run */
    if (!run_mode_is_blitz() && p_ptr
        && (p_ptr->wizard || (p_ptr->noscore & 0x0008))
        && (p_ptr->noscore & 0x000F)) {
        if (savefile[0]) {
            bool deleted;
            safe_setuid_grab();
            deleted = fd_kill(savefile);
            safe_setuid_drop();
            if (deleted) {
                log_info("metarun: deleted noscore savefile '%s'", savefile);
            } else {
                log_warn("metarun: failed to delete noscore savefile '%s'", savefile);
            }
        }
    } else {
        log_info("metarun: purge skipped (wizard=%d, noscore=0x%04X, savefile='%s')",
                 p_ptr ? (p_ptr->wizard ? 1 : 0) : -1,
                 p_ptr ? (unsigned)p_ptr->noscore : 0,
                 savefile);
    }
    return metarun_commit_new_slot(grown, new_id, activation_time);
}
