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

typedef struct run_scene_block {
    cptr text;
    byte attr;
    bool outcome_colour_reveal;
} run_scene_block;

static void show_run_scene(cptr title, byte title_attr,
    const run_scene_block blocks[], int block_count, cptr prompt,
    int hold_ms, bool wait_for_key, bool immediate, bool* fast_forward)
{
    cptr texts[8];
    byte attrs[8];
    bool outcome_reveals[8];

    if (!blocks || block_count < 1 || block_count > (int)N_ELEMENTS(texts))
        return;

    for (int i = 0; i < block_count; i++)
    {
        texts[i] = blocks[i].text;
        attrs[i] = blocks[i].attr;
        outcome_reveals[i] = blocks[i].outcome_colour_reveal;
    }

    Term_clear();
    metarun_show_poetry_blocks(title, title_attr, texts, attrs,
        outcome_reveals, block_count, prompt, hold_ms, wait_for_key,
        immediate, fast_forward);
}

static void show_run_summary_screen(int final_sils,
    bool treachery_occurred, bool kinslaying_occurred, cptr prompt,
    int hold_ms, bool wait_for_key, bool* fast_forward)
{
    char summary[256];
    run_scene_block block;

    strnfmt(summary, sizeof(summary),
        "Your legend is written: %d Silmaril%s claimed, %s, %s.",
        final_sils, (final_sils == 1) ? "" : "s",
        treachery_occurred ? "tainted by treachery" : "pure of heart",
        kinslaying_occurred ? "stained by kinslaying"
                            : "with honour intact");

    block.text = summary;
    block.attr = TERM_L_GREEN;
    block.outcome_colour_reveal = false;
    show_run_scene("The Tale Concludes", TERM_YELLOW, &block, 1, prompt,
        hold_ms, wait_for_key, false, fast_forward);
}

static void show_run_binding_screen(int chosen_count, const int chosen[4],
    bool* fast_forward)
{
    char text[4][128];
    run_scene_block blocks[4];

    chosen_count = MIN(chosen_count, (int)N_ELEMENTS(blocks));
    for (int i = 0; i < chosen_count; i++)
    {
        strnfmt(text[i], sizeof(text[i]), "The curse of %s binds your fate.",
            curse_display_name(chosen[i]));
        blocks[i].text = text[i];
        blocks[i].attr = TERM_RED;
        blocks[i].outcome_colour_reveal = false;
    }
    if (chosen_count > 0)
    {
        show_run_scene("The Binding of Fate", TERM_L_RED, blocks,
            chosen_count, prompt_text[PROMPT_CONTINUE_TALE], 0, true,
            false, fast_forward);
    }
}

static cptr run_victory_text(int sil_count)
{
    switch (sil_count)
    {
        case 1:
            return "You emerge victorious from darkness, one holy jewel blazing in your grasp. Morgoth's crown is diminished, yet hope is rekindled, though shadow lingers.";
        case 2:
            return "You escape triumphant, two Silmarils blazing fiercely in your hands. Morgoth roars in wrath; his pride is wounded deeply. Your spirit exults, yet your heart begins to feel their burning weight.";
        case 3:
            return "All three stolen stars blaze now in your hands; Morgoth's crown lies darkened. Such triumph has not been known since Fëanor himself dreamed it-but even as victory soars, your heart trembles beneath their burning glory.";
        default:
            return "You have achieved the impossible, claiming more Silmarils than should exist. Reality itself bends before your triumph.";
    }
}

static void show_run_victory_screen(int sil_count, bool allow_treachery,
    bool* fast_forward)
{
    run_scene_block block = {
        run_victory_text(sil_count), TERM_WHITE, false
    };

    show_run_scene("Victory Amid Shadow", TERM_YELLOW, &block, 1,
        allow_treachery ? prompt_text[PROMPT_FACE_TEMPTATION]
                         : prompt_text[PROMPT_CONTINUE_GENERIC],
        0, true, false, fast_forward);
}

static cptr run_treachery_text(int sil_index, bool failed)
{
    static const cptr success_msgs[3] = {
        "The first jewel shines brightly, its pure light uncorrupted. You master desire, choosing honor.",
        "The second jewel blazes defiant, temptation growing strong-but once more, you cling to honor.",
        "The third Silmaril's holy flame burns fiercely. Yet against all odds, your will resists corruption."
    };
    static const cptr failure_msgs[3] = {
        "Greed whispers softly, and you listen. Secretly you withhold the jewel's light, betraying even yourself.",
        "Desire gnaws deeper; you falter, concealing its brilliance in shame, light darkened by your betrayal.",
        "Consumed by lust for its beauty, you claim it secretly, sealing its radiance from all others-a betrayal of all trust."
    };

    sil_index = MAX(0, MIN(sil_index, 2));
    return failed ? failure_msgs[sil_index] : success_msgs[sil_index];
}

static void show_run_treachery_screen(int sil_count, const bool failed[3],
    int stolen, bool* fast_forward)
{
    static const cptr shadow_text =
        "In shadows your deeds are recorded-tainted victory shall diminish the jewel's blessing.";
    run_scene_block blocks[4];
    int count = MIN(sil_count, 3);

    for (int i = 0; i < count; i++)
    {
        blocks[i].text = run_treachery_text(i, failed[i]);
        blocks[i].attr = failed[i] ? TERM_RED : TERM_L_GREEN;
        blocks[i].outcome_colour_reveal = true;
    }
    if (stolen > 0 && count < (int)N_ELEMENTS(blocks))
    {
        blocks[count].text = shadow_text;
        blocks[count].attr = TERM_L_DARK;
        blocks[count].outcome_colour_reveal = false;
        count++;
    }

    show_run_scene("Temptation of Treachery", TERM_L_UMBER, blocks, count,
        prompt_text[PROMPT_CONTINUE_GENERIC], 0, true, false,
        fast_forward);
}

static cptr run_weight_text(int sil_count, int final_sils,
    bool treachery_occurred)
{
    static const cptr pure_frag[3] = {
        "A single star reclaimed, hope rekindled faintly in Middle-earth. Yet Morgoth laughs still, for two remain bound in shadow.",
        "Two jewels shine again beneath sky; Morgoth's power falters greatly. Yet you feel their brilliance burning; temptation ever near.",
        "All three jewels, radiant and pure, blaze again beneath stars. Morgoth's power breaks. Triumph is absolute, your soul soaring."
    };
    static const cptr tainted_frag[3] = {
        "Though victory is yours, its memory darkens. Trust is fragile, and your spirit heavy beneath secret betrayal.",
        "Your heart trembles: Morgoth sees clearly your treachery-he smiles grimly, knowing darkness still dwells in you.",
        "Greatest triumph now mingled with darkest shame. Morgoth's laughter echoes bitterly-he senses your fall."
    };

    if (treachery_occurred)
        return tainted_frag[MAX(0, MIN(sil_count - 1, 2))];
    return pure_frag[MAX(0, MIN(final_sils - 1, 2))];
}

static void show_run_weight_screen(int sil_count, int final_sils,
    bool treachery_occurred, bool allow_kinslay, bool* fast_forward)
{
    run_scene_block block = {
        run_weight_text(sil_count, final_sils, treachery_occurred),
        treachery_occurred ? TERM_RED : TERM_L_WHITE,
        false
    };

    show_run_scene("The Weight of Victory", TERM_L_BLUE, &block, 1,
        allow_kinslay ? prompt_text[PROMPT_FACE_ECHOES]
                      : prompt_text[PROMPT_CONCLUDE_TALE],
        0, true, false, fast_forward);
}

static cptr run_kinslaying_text(int sil_index, bool failed)
{
    static const cptr success_msgs[3] = {
        "The sorrow of Alqualondë passes over you-your spirit holds fast, blood unstained.",
        "Memory of Doriath rises briefly, but your blade remains clean, honour upheld.",
        "You resist dark whispers recalling Sirion-your sword is stayed, mercy unbroken."
    };
    static const cptr failure_msgs[3] = {
        "\"Alqualondë's Grief\"\nBlood stains starlit waves. Your hand remembers the swords at Alqualondë-first grief, first guilt.",
        "\"Ruin of Doriath\"\nAgain your hand recalls tragedy-fallen halls of Menegroth, Dior's blood shed beneath stolen starlight.",
        "\"Tragedy at Sirion\"\nEchoes rise from Sirion-Elwing's flight, blood and betrayal. Once more your blade draws innocent blood, sealing doom anew."
    };

    sil_index = MAX(0, MIN(sil_index, 2));
    return failed ? failure_msgs[sil_index] : success_msgs[sil_index];
}

static void show_run_kinslaying_screen(int sil_count, const bool failed[3],
    bool kinslaying_occurred, bool* fast_forward)
{
    static const cptr doom_text =
        "Blood now stains your triumph, your fate forever woven with grief and shame.";
    run_scene_block blocks[4];
    int count = 0;

    for (int i = 0; i < MIN(sil_count, 3); i++)
    {
        blocks[count].text = run_kinslaying_text(i, failed[i]);
        blocks[count].attr = failed[i] ? TERM_RED : TERM_L_GREEN;
        blocks[count].outcome_colour_reveal = true;
        count++;
        if (failed[i])
            break;
    }
    if (kinslaying_occurred && count < (int)N_ELEMENTS(blocks))
    {
        blocks[count].text = doom_text;
        blocks[count].attr = TERM_L_DARK;
        blocks[count].outcome_colour_reveal = false;
        count++;
    }

    show_run_scene("Echoes of Kinslaying", TERM_L_RED, blocks, count,
        prompt_text[PROMPT_CONCLUDE_TALE], 0, true, false, fast_forward);
}

static void show_run_price_screen(int victims, bool* fast_forward)
{
    char text[128];
    run_scene_block block;

    strnfmt(text, sizeof(text),
        "Your kinslaying echoes through time. %d innocent%s will fall by your hand...",
        victims, (victims == 1) ? "" : "s");
    block.text = text;
    block.attr = TERM_RED;
    block.outcome_colour_reveal = false;
    show_run_scene("The Price of Blood", TERM_RED, &block, 1,
        prompt_text[PROMPT_WITNESS_CONSEQUENCES], 0, true, true,
        fast_forward);
}

static void show_run_blood_demanded_screen(cptr fallen_names[], int count,
    bool* fast_forward)
{
    char text[3][96];
    run_scene_block blocks[3];

    count = MIN(count, (int)N_ELEMENTS(blocks));
    for (int i = 0; i < count; i++)
    {
        strnfmt(text[i], sizeof(text[i]),
            "A hero %s has fallen beneath your blade.", fallen_names[i]);
        blocks[i].text = text[i];
        blocks[i].attr = TERM_RED;
        blocks[i].outcome_colour_reveal = false;
    }
    if (count > 0)
    {
        show_run_scene("Blood Is Demanded", TERM_RED, blocks, count,
            prompt_text[PROMPT_RETURN_MIDDLE_EARTH], 0, true,
            true, fast_forward);
    }
}

void metarun_debug_show_run_summary(int silmarils, int stolen_silmarils,
    bool show_treachery, int kinslaying_attempt)
{
    int chosen[4] = { -1, -1, -1, -1 };
    int curse_count;
    int chosen_count;
    int final_sils;
    bool treachery_failed[3] = { false, false, false };
    bool kinslaying_failed[3] = { false, false, false };
    bool show_kinslaying = (kinslaying_attempt >= 0);
    bool kinslaying_occurred;
    bool fast_forward = false;

    silmarils = MAX(1, MIN(silmarils, 3));
    stolen_silmarils = show_treachery
        ? MAX(0, MIN(stolen_silmarils, silmarils)) : 0;
    final_sils = silmarils - stolen_silmarils;
    curse_count = (silmarils == 3) ? 4 : silmarils;

    for (int i = silmarils - stolen_silmarils; i < silmarils; i++)
        treachery_failed[i] = true;
    if (kinslaying_attempt > 0)
    {
        kinslaying_attempt = MIN(kinslaying_attempt, silmarils);
        kinslaying_failed[kinslaying_attempt - 1] = true;
    }
    kinslaying_occurred = (kinslaying_attempt > 0);

    screen_save();
    screen_push_supporting_panes_hidden();
    screen_push_touch_pane_hidden();
    sdl_poetry_sequence_layout_begin();

    chosen_count = metarun_debug_choose_escape_curses(curse_count, chosen);
    show_run_binding_screen(chosen_count, chosen, &fast_forward);
    show_run_victory_screen(silmarils, show_treachery, &fast_forward);
    if (show_treachery)
    {
        show_run_treachery_screen(silmarils, treachery_failed,
            stolen_silmarils, &fast_forward);
    }
    show_run_weight_screen(silmarils, final_sils, stolen_silmarils > 0,
        show_kinslaying, &fast_forward);
    if (show_kinslaying)
    {
        show_run_kinslaying_screen(silmarils, kinslaying_failed,
            kinslaying_occurred, &fast_forward);
    }

    show_run_summary_screen(final_sils, stolen_silmarils > 0,
        kinslaying_occurred, prompt_text[PROMPT_RETURN_MIDDLE_EARTH],
        3000, !kinslaying_occurred, &fast_forward);
    if (kinslaying_occurred)
    {
        cptr preview_names[1] = { "of a former tale" };

        show_run_price_screen(1, &fast_forward);
        show_run_blood_demanded_screen(preview_names, 1, &fast_forward);
    }

    sdl_poetry_sequence_layout_end();
    screen_pop_touch_pane_hidden();
    screen_pop_supporting_panes_hidden();
    screen_load();
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
 *  The SDL narrative helpers, curse chooser, and kinslaying flow are reused.
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

            metarun_show_poetry_scene(title, TERM_RED, text, TERM_WHITE,
                transition_text, TERM_L_BLUE,
                prompt_text[PROMPT_RETURN_MIDDLE_EARTH]);
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
    sdl_poetry_sequence_layout_begin();

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
        show_run_binding_screen(chosen_cnt, chosen, &fast_forward);

    /* ============================================================= */
    /* SCENE 3: Victory Declaration                                  */
    /* ============================================================= */
    show_run_victory_screen(sil_count, allow_treachery, &fast_forward);

    /* ============================================================= */
    /* SCENE 4: Temptation of Treachery (Enhanced Messages)        */
    /* ============================================================= */
    byte stolen = 0;
    bool treachery_failed[3] = { false, false, false };
    if (allow_treachery)
    {
        static const int pct[3] = { 20, 50, 95 };

        for (int i = 0; i < sil_count; ++i)
        {
            bool fail = (rand_int(100) < pct[i]);
            treachery_failed[i] = fail;
            if (fail) stolen++;
        }

        show_run_treachery_screen(sil_count, treachery_failed, stolen,
            &fast_forward);
    }

    byte final_sils = sil_count - stolen;
    bool treachery_occurred = (stolen > 0);

    /* ============================================================= */
    /* SCENE 5: The Weight of Victory                               */
    /* ============================================================= */
    show_run_weight_screen(sil_count, final_sils, treachery_occurred,
        allow_kinslay, &fast_forward);

    /* ============================================================= */
    /* SCENE 6: Echoes of Kinslaying (Enhanced Notifications)      */
    /* ============================================================= */
    bool deferred_kill[3] = { false, false, false };
    int kinslaying_victims = 0;
    if (allow_kinslay)
    {
        static const int kin_pct[3] = { 20, 50, 95 };

        for (int k = 0; k < sil_count; ++k)
        {
            /* One roll only - use kin_pct[] here and *skip* the roll
             * inside kinslayer_try_kill() later.                        */
            /* one-shot probability (keep a local alias for UI)        */
            bool fail = (rand_int(100) < kin_pct[k]);
            deferred_kill[k] = fail;
            if (fail) kinslaying_victims++;

            /* Stop at first failure */
            if (fail) break;
        }

        show_run_kinslaying_screen(sil_count, deferred_kill,
            kinslaying_victims > 0, &fast_forward);
    }

    /* ============================================================= */
    /* SCENE 7: Final Summary                                       */
    /* ============================================================= */
    bool has_post_summary_scene = allow_kinslay && (kinslaying_victims > 0);

    show_run_summary_screen(final_sils, treachery_occurred,
        kinslaying_victims > 0,
        prompt_text[PROMPT_RETURN_MIDDLE_EARTH], 3000,
        !has_post_summary_scene, &fast_forward);

    /* ============================================================= */
    /* SCENE 8: Kinslaying Execution & Notifications               */
    /* ============================================================= */
    if (has_post_summary_scene)
        show_run_price_screen(kinslaying_victims, &fast_forward);

    /* ------------------------------------------------------------- */
    /*  SCENE 8-bis: actual executions with cinematic feedback       */
    /* ------------------------------------------------------------- */
    if (has_post_summary_scene) {
        char fallen_names[3][32];
        cptr fallen_name_ptrs[3];
        int fallen_count = 0;

        for (int k = 0; k < 3; k++) {
            if (!deferred_kill[k]) continue;

            const char *character =
                kinslayer_try_kill(k + 1, /*do_roll=*/false);
            if (!character) continue;               /* should not happen */

            metarun_increment_deaths();
            log_info("Metarun: kinslaying victim counted as death (%u total)", (unsigned)metar.deaths);
            SDL_strlcpy(fallen_names[fallen_count], character,
                sizeof(fallen_names[fallen_count]));
            fallen_name_ptrs[fallen_count] = fallen_names[fallen_count];
            fallen_count++;
        }

        show_run_blood_demanded_screen(fallen_name_ptrs, fallen_count,
            &fast_forward);
    }

    metarun_gain_silmarils(final_sils);
    log_info("Added %d Silmarils to metarun total (now %d)", final_sils, metar.silmarils);
    refresh_current_metar_score();
    print_story(3, true);

    /* Restore the saved play-screen only after every narrative beat */
    sdl_poetry_sequence_layout_end();
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

static void show_run_result_screen(bool victory, int alive,
    int required_survivors, int win_goal, cptr prompt)
{
    char body[256];
    cptr note = "";
    byte title_attr = victory ? TERM_YELLOW : TERM_RED;
    byte body_attr = victory ? TERM_L_GREEN : TERM_WHITE;

    if (victory)
    {
        strnfmt(body, sizeof(body),
            "%d Silmarils reclaimed from Morgoth's crown! "
            "Hope kindles anew; your long trial approaches its end. "
            "Yet one final ordeal awaits: your ultimate destiny, "
            "as your true self faces the Last Trial.", win_goal);
        note = "(This final trial is yet to be implemented.)";
    }
    else
    {
        strnfmt(body, sizeof(body),
            "Only %d hero%s remain, yet %d must endure to reclaim the remaining Silmarils. "
            "This tale falls into shadow; begin anew to kindle hope once more.",
            alive, (alive == 1) ? "" : "es", required_survivors);
    }

    screen_save();
    metarun_show_poetry_scene("The Trial's End", title_attr, body,
        body_attr, note, TERM_L_DARK, prompt);
    screen_load();
}

void metarun_debug_show_run_result(bool victory, int silmarils, int alive,
    int required_survivors)
{
    if (required_survivors < 1)
        required_survivors = 1;
    if (alive < 0)
        alive = 0;

    show_run_result_screen(victory, alive, required_survivors,
        MAX(0, silmarils),
        "[Press any key to return to the game]");
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

        show_run_result_screen(false, alive, required_survivors, win_goal,
            "[Press any key to begin anew]");

        if (!start_new_metarun())
            report_tale_rollover_failure();
        return;
    }

    if (metar.silmarils >= win_goal) {
        log_info("Metarun VICTORY: %d Silmarils collected (goal: %d)", metar.silmarils, win_goal);
        show_run_result_screen(true, alive, required_survivors, win_goal,
            "[Press any key to begin anew]");

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
