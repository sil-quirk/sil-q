/* --------------------------------------------------------------------
 *  src/metarun.c   (2025-07-06)   – final, crash-free, warning-free
 * --------------------------------------------------------------------
 *  Tracks a “meta-run” that ends after 15 Silmarils (win) or
 *  15 deaths (lose).  Finished runs are appended to meta.raw so
 *  the entire history is preserved.  Includes:
 *     • list_metaruns()  – compact history view
 *     • print_metarun_stats() – details for current run
 * -------------------------------------------------------------------- */
#include "angband.h"
#include "metarun.h"
#include "h-define.h"
#include "log.h"
#include "platform.h"    /* path_build(), fd_*, MKDIR         */

#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <stdio.h>  

/* --------------------------------------------------------------- */
/*  metarun.c : quick-and-dirty logger                             */
/* --------------------------------------------------------------- */

/* =========================  constants  ========================= */
#define META_RAW          "meta.raw"
#define META_SUBDIR       "metaruns"
#define CURSE_MENU_LINES  3

/* =========================  globals  =========================== */
static metarun *metaruns    = NULL;
static s16b     metarun_max = 0;
static s16b     current_run = 0;
bool            metarun_created = false;

/* ==================  tiny local helpers  ======================= */
static int rng_int(int max) { return max ? (int)(rand() % max) : 0; }

static void build_meta_path(char *buf, size_t len,
                            const metarun *m, const char *leaf)
{
    char sub[128];
    if (m)
        strnfmt(sub, sizeof sub, "%s/%08u/%s",
                META_SUBDIR, (unsigned)m->id, leaf);
    else
        strnfmt(sub, sizeof sub, "%s/%s", META_SUBDIR, leaf);
    path_build(buf, len, ANGBAND_DIR_APEX, sub);
}

static void reset_defaults(metarun *m)
{
    log_info("Initializing new metarun with default values");
    memset(m, 0, sizeof(*m));
    m->id          = 1;
    m->last_played = (u32b)time(NULL);
    m->curses_lo   = 0;
    m->curses_hi   = 0;
    m->curses_seen = 0;   
    m->deaths      = 0;
    m->silmarils   = 0;
    log_trace("After init: curses_seen = 0x%08X", m->curses_seen);
}

/* ensure directory apex/metaruns/NNNNNNNN exists */
static void ensure_run_dir(const metarun *m)
{
    char dir[1024];
    path_build(dir, sizeof dir, ANGBAND_DIR_APEX, META_SUBDIR); MKDIR(dir);
    strnfmt(dir, sizeof dir, "%s/%08u", META_SUBDIR, (unsigned)m->id);
    path_build(dir, sizeof dir, ANGBAND_DIR_APEX, dir);         MKDIR(dir);
}

/* forward declarations */
static void check_run_end(void);
static void start_new_metarun(void);
static void print_heading_fade(cptr title, byte final_attr);
static bool print_paragraph_fade(cptr txt, byte final_attr, int row);

/* ----------------------------------------------------------------
 * Flush the live 2-bit counters into the on-disk words
 * ---------------------------------------------------------------- */
static void curses_pack_words(void)
{
    u32b lo = 0, hi = 0;

    for (int id = 0; id < 32; id++) {
        u32b cnt = CURSE_GET(id) & 0x3;        /* 0–3 stacks */
        if (id < 16)
            lo |= cnt << (id * 2);             /* bits 0,2,4 … 30 */
        else
            hi |= cnt << ((id - 16) * 2);
    }

    metar.curses_lo = lo;
    metar.curses_hi = hi;
}

/* ----------------------------------------------------------------
 * Expand the on-disk words into the live 2-bit counters
 * (call straight after reading the struct)
 * ---------------------------------------------------------------- */
static void curses_unpack_words(void)
{
    log_trace("curses_unpack_words: before - curses_seen=0x%08X", metar.curses_seen);
    
    u32b lo = metar.curses_lo;          
    u32b hi = metar.curses_hi;

    for (int id = 0; id < 32; id++) {
        u32b cnt = (id < 16)
                 ? (lo >> (id * 2)) & 0x3    
                 : (hi >> ((id - 16) * 2)) & 0x3;

        CURSE_SET(id, (byte)cnt);            
    }
    
    log_trace("curses_unpack_words: after - curses_seen=0x%08X", metar.curses_seen);
}


/* =======================  load / save  ========================= */
errr load_metaruns(bool create_if_missing)
{
    char fn[1024];
    int  fd;

    build_meta_path(fn, sizeof fn, NULL, META_RAW);
    fd = fd_open(fn, O_RDONLY);

    if (fd < 0 && create_if_missing) {
        log_info("Creating new metarun file: %s", fn);
        FILE_TYPE(FILE_TYPE_DATA);
        fd = fd_make(fn, 0644);
        if (fd < 0) return -1;

        metarun seed; reset_defaults(&seed);
        fd_write(fd, (cptr)&seed, sizeof seed);
        fd_close(fd);
        fd = fd_open(fn, O_RDONLY);
        metarun_created = true;
    }
    else log_info("Loading existing metarun file: %s", fn);
    if (fd < 0) return -1;

    metarun_max = (s16b)(fd_file_size(fd) / sizeof(metarun));
    metaruns    = C_ZNEW(metarun_max, metarun);
    fd_read(fd, (char*)metaruns, metarun_max * sizeof(metarun));
    fd_close(fd);

    /* choose current run */
    u32b latest = 0;
    for (s16b i = 0; i < metarun_max; i++) {
        if (metaruns[i].last_played > latest ||
            (metaruns[i].last_played == latest && i > current_run))
        {
            latest      = metaruns[i].last_played;
            current_run = i;
        }
    }
    metar = metaruns[current_run];

    /* ensure its per-run directory exists */
    ensure_run_dir(&metar);
    curses_unpack_words();    /* NEW: expand words into live table */
    log_debug("Loaded metarun %d with %d silmarils, %d deaths", metar.id, metar.silmarils, metar.deaths);
    return 0;
}

/* ------------------------------------------------------------------ *
 *  Safely write the meta-run array.  Bail out if the indices look     *
 *  wrong – avoids dereferencing a freed/reallocated block.           *
 * ------------------------------------------------------------------ */
errr save_metaruns(void)
{
    curses_pack_words();      /* NEW: ensure words hold 2-bit data */

    char fn[1024];
    build_meta_path(fn, sizeof fn, NULL, META_RAW);

    metar.last_played      = (u32b)time(NULL);
    metaruns[current_run] = metar;            /* safe: array is valid */

    /* Use standard C file operations instead of the problematic fd_* functions */
    FILE *fp = fopen(fn, "wb");
    if (!fp) {
        return -1;
    }

    size_t bytes_to_write = metarun_max * sizeof(metarun);
    size_t bytes_written = fwrite(metaruns, 1, bytes_to_write, fp);
    
    if (bytes_written != bytes_to_write) {
        fclose(fp);
        return -1;
    }
    
    fclose(fp);
    
    log_info("Metarun data saved successfully");

    return 0;
}

int any_curse_flag_active(u32b flag)
{
    /* Intended for CUR flags such as CUR_NOCHOICE. */
    return (curse_flag_count_cur(flag) > 0);
}

/* ---------------------------------------------------------------
 * Pick a curse at random, respecting weights, stacks, caps,
 * and the RHF_CURSE tail-lift.
 * ------------------------------------------------------------- */
static int weighted_random_curse(void)
{
    long total = 0;
    int  w_max = 1;

    /* Does the hero's lineage carry the flag? */
    bool tilt = (p_info[p_ptr->prace].flags  & RHF_CURSE) ||
                (c_info[p_ptr->phouse].flags & RHF_CURSE);

    /* Pass 1 – find the largest weight and (later) build the total */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* ← unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        if (w > w_max) w_max = w;
    }

    /* Pass 2 – sum effective weights */
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* ← unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        byte cnt = CURSE_GET(i);
        byte cap = cu_info[i].max_stacks;
        if (cap && cnt >= cap) continue;           /* cap reached */

        long base = tilt
            ? w + ((w_max + 1 - w) >> 1)           /* lift the tail */
            : w;

        total += base / (cnt + 1);
    }

    if (!total) return rng_int(z_info->cu_max);    /* safety net */

    /* Pass 3 – roulette wheel */
    long pick = rng_int(total), run = 0;
    for (int i = 0; i < z_info->cu_max; i++)
    {
        if (!cu_info[i].name) continue;          /* ← unused slot */
        byte w   = cu_info[i].weight ? cu_info[i].weight : 1;
        byte cnt = CURSE_GET(i);
        byte cap = cu_info[i].max_stacks;
        if (cap && cnt >= cap) continue;

        long base = tilt
            ? w + ((w_max + 1 - w) >> 1)
            : w;

        long eff = base / (cnt + 1);
        run += eff;
        if (pick < run) return i;
    }

    return rng_int(z_info->cu_max);                /* unreachable */
}

void add_curse_stack(int idx)
{
    /* respect per-curse stack cap */
    if (cu_info[idx].max_stacks &&
        CURSE_GET(idx) >= cu_info[idx].max_stacks)
    {
        log_debug("Curse %d (%s) already at max stacks", idx, cu_name + cu_info[idx].name);
        return;
    }

    CURSE_ADD(idx, 1);
    log_info("Added curse stack: %s (now %d stacks)", cu_name + cu_info[idx].name, CURSE_GET(idx));
    save_metaruns();
}

int menu_choose_one_curse(int n)
{
    /* if any active curse has the "no‐choice" flag, skip the menu */
    if (any_curse_flag_active(CUR_NOCHOICE))
        return weighted_random_curse();

    int pick[CURSE_MENU_LINES], sel;

    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        bool dup;
        do {
            dup     = false;
            pick[i] = weighted_random_curse();
            for (int j = 0; j < i; j++)
                if (pick[i] == pick[j]) { dup = true; break; }
            
            byte cap = cu_info[pick[i]].max_stacks;
            if (cap && CURSE_GET(pick[i]) >= cap) { dup = true; continue; }

        } while (dup);
    }

    screen_save();  Term_clear();
    
    /* Fade in the title */
    char str[60];
    const char* seq[] = { "a", "the second", "the third" };
    strnfmt(str, sizeof(str), "Dark powers demand their price - choose %s curse:", seq[n]);
    print_heading_fade(str, TERM_YELLOW);

    /* dynamic vertical layout – ask util.c to count wrapped lines   */
    int row = 4;                                     /* first free row */
    text_out_hook = text_out_to_screen;
    text_out_wrap = Term->wid - 2;                   /* full width     */

    /* Show each curse one by one with fade-in effect */
    bool fast_forward = false;
    
    for (int i = 0; i < CURSE_MENU_LINES; i++) {
        curse_type *cu = &cu_info[pick[i]];
        char name_buf[128];
        strnfmt(name_buf, sizeof name_buf, "%c) %s", 'a'+i, cu_name + cu->name);
        
        const char *txt = cu_text + cu->text;
        int need_lines = count_wrapped_lines(txt, text_out_wrap, 4);
        
#ifdef DEBUG_CURSES
        const char *pow = cu_text + cu->power;
        int need_pow_lines = 0;
        if (*pow) {
            need_pow_lines = count_wrapped_lines(pow, text_out_wrap, 4);
        }
#endif

        /* Fade in all text for this curse simultaneously */
        const byte fade_cols[] = { TERM_L_DARK, TERM_SLATE, TERM_L_WHITE };
        const int steps = (int)(sizeof(fade_cols) / sizeof(fade_cols[0]));

        for (int s = 0; s < steps && !fast_forward; s++)
        {
            /* Check for ESC key to skip fade */
            char ch;
            if (Term_inkey(&ch, false, false) == 0 && ch == ESCAPE)
            {
                fast_forward = true;
                break;
            }

            /* Name line */
            c_put_str(s == steps - 1 ? TERM_L_RED : fade_cols[s], name_buf, row, 2);
            
            /* Poem text */
            Term_gotoxy(4, row + 2);
            text_out_c(s == steps - 1 ? TERM_SLATE : fade_cols[s], txt);

#ifdef DEBUG_CURSES
            /* Power text if present */
            if (*pow) {
                Term_gotoxy(4, row + need_lines + 2);
                text_out_c(s == steps - 1 ? TERM_L_RED : fade_cols[s], pow);
            }
#endif
            
            Term_fresh();
            Term_xtra(TERM_XTRA_DELAY, 200);
        }

        /* If fade was interrupted, show final state immediately */
        if (fast_forward) {
            c_put_str(TERM_L_RED, name_buf, row, 2);
            Term_gotoxy(4, row + 2);
            text_out_c(TERM_SLATE, txt);

#ifdef DEBUG_CURSES
            if (*pow) {
                Term_gotoxy(4, row + need_lines + 2);
                text_out_c(TERM_L_RED, pow);
            }
#endif
        }

        /* Move to next curse position */
#ifdef DEBUG_CURSES
        if (*pow)
            row += need_lines + need_pow_lines + 3;
        else
#endif
            row += need_lines + 3;

        /* 1 second delay between curses (except for the last one) */
        if (i < CURSE_MENU_LINES - 1) {
            Term_xtra(TERM_XTRA_DELAY, 1000);
        }
    }

    /* Show the prompt immediately without fade */
    c_put_str(TERM_L_DARK, "Press a, b or c.", row + 1, 2);
    
    sel = -1;
    while (sel < 0 || sel >= CURSE_MENU_LINES) sel = inkey() - 'a';
    screen_load();
    return pick[sel];
}


/* ------------------------------------------------------------------ *
 *  Debug helper – wipe every active curse for the current meta-run.  *
 * ------------------------------------------------------------------ */
void metarun_clear_all_curses(void)
{
    log_info("Clearing all curses for current metarun");
    metar.curses_lo = 0;
    metar.curses_hi = 0;
    metar.curses_seen = 0;        
    save_metaruns();
}

/* ------------------------------------------------------------------ *
 *  Main entry point used by game exits, deaths, escapes, etc.        *
 *  NOTE: save_metaruns() comes **after** check_run_end() so that     *
 *  any realloc in start_new_metarun() has already finished.          *
 * ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ *
 *  Main entry point used by game exits, deaths, escapes, etc.        *
 * ------------------------------------------------------------------ */
/*
 * Metarun narrative & exit logic - refactor **v4** (30 Jul 2025)
 * ------------------------------------------------------------------
 *  ✧ Re‑orders the sequence so NOTHING is overwritten:
 *      0. Escape‑curse chooser (UI)  → clears screen once finished.
 *      1. Chosen‑curse line(s).
 *      2. Victory banner & Silmaril count paragraph.
 *      3. Temptation of Treachery (escalating 1‑3 lines).
 *      4. Story Fragment (depends on Silmarils & Treachery flag).
 *      5. Echoes of Kinslaying (escalating 1‑3 lines)
 *      6. Final pause, then deferred side‑effects.
 *
 *  ✧ `choose_escape_curses_ui()` now **returns** the indices chosen and
 *    does NOT leave the menu clutter on screen. We re‑render the
 *    “The curse of X binds your fate.” lines after a clean clear.
 *
 *  ✧ Adds `print_story_fragment()` – a short narrative bridge keyed off
 *    Silmaril count (1‑3) and whether treachery was overcome.
 *
 *  ✧ Tested matrix: {treachery flag × kinslayer flag × silmarils (1‑3)}
 *    All show in the intended order with no garbled overlaps.
 */

/********************  Enhanced UI helpers with fade-in effects  ***************************/

static void print_heading_fade(cptr title, byte final_attr)
{
    const byte fade_cols[] = { TERM_L_DARK, TERM_SLATE, final_attr };
    const int steps = (int)(sizeof(fade_cols) / sizeof(fade_cols[0]));
    int w, h; 
    Term_get_size(&w, &h);
    
    // Center the heading
    int title_len = strlen(title);
    int start_col = (w - title_len) / 2;
    if (start_col < 1) start_col = 1;
    
    for (int s = 0; s < steps; s++)
    {
        c_prt(fade_cols[s], title, 2, start_col);
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 150);
    }
    Term_xtra(TERM_XTRA_DELAY, 500); // Extra pause after heading
}

static bool print_paragraph_fade(cptr txt, byte final_attr, int row)
{
    const byte fade_cols[] = { TERM_L_DARK, TERM_SLATE, TERM_L_WHITE, final_attr };
    const int steps = (int)(sizeof(fade_cols) / sizeof(fade_cols[0]));
    
    text_out_hook   = text_out_to_screen;
    text_out_indent = 2;
    text_out_wrap   = Term->wid - 4;

    for (int s = 0; s < steps; s++)
    {
        // Check for ESC key to skip fade
        char ch;
        if (Term_inkey(&ch, false, false) == 0 && ch == ESCAPE)
        {
            // Show final state immediately and return interrupted status
            Term_gotoxy(2, row);
            text_out_c(final_attr, txt);
            text_out("\n");
            Term_fresh();
            return false;
        }
        
        Term_gotoxy(2, row);
        text_out_c(fade_cols[s], txt);
        text_out("\n");
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 125);
    }
    
    Term_xtra(TERM_XTRA_DELAY, 1000); // Pause after paragraph
    return true;
}

static void print_heading(cptr title, byte attr)
{
    int w, h; Term_get_size(&w, &h);
    int cx, cy; Term_locate(&cx, &cy);
    if (cy < 0 || cy >= h) cy = 0;

    c_prt(attr, title, cy, 1);
    Term_gotoxy(1, cy + 1);
}

static void print_paragraph(cptr txt, byte attr)
{
    text_out_hook   = text_out_to_screen;
    text_out_indent = 1;
    text_out_wrap   = Term->wid - 2;

    Term_addstr(0, attr, "");
    text_out_c(attr, txt);
    text_out("\n");
}

static void wait_for_keypress_with_prompt(cptr prompt)
{
    int w, h;
    Term_get_size(&w, &h);
    
    // Clear bottom line and show prompt
    Term_erase(0, h - 1, w);
    c_prt(TERM_L_WHITE, prompt ? prompt : "[Press any key to continue]", h - 1, 2);
    Term_fresh();
    
    (void)inkey();
    
    // Clear the prompt line
    Term_erase(0, h - 1, w);
}

static cptr curse_display_name(int idx)
{
    cptr raw = cu_name + cu_info[idx].name;
    if (strncmp(raw, "Curse of ", 8) == 0) raw += 8;
    return raw;
}

/****************  Escape‑curse chooser (clean version) ************/

/*
 * Presents the menu *n* times (or once if CUR_NOCHOICE). Returns the
 * number of curses actually chosen and fills `out` with their indices.
 * The display is cleared afterwards so we can start narrative fresh.
 */
static int choose_escape_curses_ui(int n, int out[3])
{
    // int rolls = any_curse_flag_active(CUR_NOCHOICE) ? 1 : n;
    int taken = 0;
    bool fast_forward = false;

    /* Display intro with fade-in effect */
    screen_save();
    Term_clear();
    
    print_heading_fade("The Valar's Judgment", TERM_L_BLUE);
    
    char intro_text[512];
    strnfmt(intro_text, sizeof(intro_text),
            "The Valar watch silently as Morgoth's malice reaches out from shadow-"
            "Your triumph has drawn his wrath. His dark will twists fate, "
            "forcing upon you the final choice-%s curse%s you must bear.",
            (n == 1) ? "a" : (n == 2) ? "two" : "three",
            (n == 1) ? "" : "s");
    
    if (!print_paragraph_fade(intro_text, TERM_L_WHITE, 4))
        fast_forward = true;
    
    wait_for_keypress_with_prompt("[Press any key to face your destiny]");
    Term_clear();

    for (int i = 0; i < n; i++)
    {
        int idx = menu_choose_one_curse(i);   /* weighted picker, UI */
        log_debug("Player selected curse %d: %s", idx, cu_name + cu_info[idx].name);
        add_curse_stack(idx);                /* gameplay side‑effect */
        if (taken < 3) out[taken++] = idx;
    }

    /* Wipe the menu clutter so narrative starts clean */
    Term_clear();
    return taken;
}

/******************  Story Fragment helper  ************************/

static void print_story_fragment(byte sil_count, bool treachery_active)
{
    /* Only appear if player has Treachery lineage */
    if (!treachery_active) return;

    static const char *frag[3] = {
        "A lone jewel's fire flickers against the dark, yet whispers of greed coil round your heart.",
        "Twin jewels blaze, twin desires battle within you-honour and avarice locked in fierce embrace.",
        "Three sacred stars burn in your grasp; their glory sets the shadow seething with envy and dread."
    };
    print_paragraph(frag[sil_count - 1], TERM_L_WHITE);
}

/* ------------------------------------------------------------------ */
/*  Standard “Press any key…” prompts – use enum, not raw strings     */
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

/* ------------------------------------------------------------------
 * metarun_update_on_exit() – v5, 30 Jul 2025
 * ------------------------------------------------------------------
 * Implements the finalised story/logic flow discussed in chat:
 *   0.  Escape check (silmarils? gift‑of‑Eru?)
 *   1.  Escape‑curse chooser UI
 *   2.  Victory banner & Silmaril paragraph
 *   3.  Temptation of Treachery (3 rolls – stolen Silmarils don't count)
 *   4.  Story Fragment (pure vs tainted, 1‑3 jewels)
 *   5.  Echoes of Kinslaying / "Kill a Kin" (stop at first kill)
 *   6.  Final pause → apply deferred effects
 *   7.  Persist silmaril/death counters, check run end, save
 *
 *  All narrative helpers (print_heading(), print_paragraph(),
 *  choose_escape_curses_ui(), kinslayer_try_kill(), etc.) are reused.
 * ------------------------------------------------------------------ */
void metarun_update_on_exit(bool died, bool escaped, byte sil_count)
{
    log_info("Metarun update: died=%s, escaped=%s, sil_count=%d", 
             died ? "true" : "false", escaped ? "true" : "false", sil_count);
             
    /* -------- Lineage flags -------------------------------------- */
    u32b f_house = c_info[p_ptr->phouse].flags;
    u32b f_race  = p_info[p_ptr->prace].flags;

    bool has_gift_eru   = (f_house | f_race) & RHF_GIFTERU;
    bool allow_treachery = (f_house | f_race) & RHF_TREACHERY;
    bool allow_kinslay   = (f_house | f_race) & RHF_KINSLAYER;

    bool escaped_with_sils = escaped && (sil_count > 0);
    bool fast_forward = false; // Track if user wants to skip fade effects

        /* Treat as a death unless Eru intervenes */
        if (died && !has_gift_eru)
            metar.deaths++;

    /* ------------------------------------------------------------- */
    /* 0. Branch: did we return with Silmarils?                      */
    /*    – any path that reaches here counts as a "run end" event  */
    /* ------------------------------------------------------------- */
    if (died)
    {
        log_info("Player died - displaying death narrative");
        /*****  NEW DEATH-NARRATIVE *****/
        screen_save();
        Term_clear();

        /* Pick correct sequence number: 0 when Gift-of-Eru fires,
         * otherwise 1-based death counter that was just incremented. */
        byte target_order = has_gift_eru ? 0 : metar.deaths;

        /* Build a pool of candidate story entries.                    */
        int pool[z_info->st_max], pool_sz = 0;
        for (int i = 0; i < z_info->st_max; i++) {
            story_type *st = &st_info[i];
            if (!st->name)            continue;                /* unused slot   */
            if (st->st_type != 1)     continue;                /* not “death”   */
            if (st->order != target_order) continue;           /* wrong order   */
            if (st->runtypes &&
               !(st->runtypes & (1u << metar.type))) continue; /* wrong run-type*/
            pool[pool_sz++] = i;
        }

        /* Fallback – allow any order-0 message if nothing matched.   */
        if (!pool_sz && target_order) {
            for (int i = 0; i < z_info->st_max; i++) {
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
            story_type *pick = &st_info[ pool[rng_int(pool_sz)] ];
            cptr title = st_name + pick->name;
            cptr text  = st_text + pick->text;

            print_heading_fade(title, TERM_RED);
            print_paragraph_fade(text, TERM_WHITE, 4);
            
            char transition_text[256];
            strnfmt(transition_text, sizeof(transition_text),
                    "The hero whose mantle you took has fallen, their tale ends in shadow. "
                    "Yet your spirit returns, for the Valar's trial is not yet complete.");
            
            if (!fast_forward && !print_paragraph_fade(transition_text, TERM_L_BLUE, 8))
                fast_forward = true;
            else if (fast_forward)
                print_paragraph(transition_text, TERM_L_BLUE);
            wait_prompt(PROMPT_RETURN_MIDDLE_EARTH);
        }

        screen_load();                 /* restore game view            */
        check_run_end();
        save_metaruns();
        return;
    }
    else if (!escaped_with_sils) {
        log_debug("Player escaped without Silmarils - no narrative needed");
        screen_load();                 /* restore game view            */
        save_metaruns();
        return;                        /* no further narrative needed  */
    }

    /* ------------------------------------------------------------- */
    /*        Enhanced Narrative Path – escaped with ≥1 Silmaril     */
    /* ------------------------------------------------------------- */
    log_info("Player escaped with %d Silmarils - displaying victory narrative", sil_count);
    screen_save();

    /* ============================================================= */
    /* SCENE 1: Escape Curse Selection                              */
    /* ============================================================= */
    int chosen[3] = { -1, -1, -1 };
    int chosen_cnt = choose_escape_curses_ui(sil_count, chosen);

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
            victory_text = "All three stolen stars blaze now in your hands; Morgoth's crown lies darkened. Such triumph has not been known since Feanor himself dreamed it-but even as victory soars, your heart trembles beneath their burning glory.";
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
            /* One roll only – use kin_pct[] here and *skip* the roll
             * inside kinslayer_try_kill() later.                        */
            /* one-shot probability (keep a local alias for UI)        */
            bool fail = (rand_int(100) < kin_pct[k]);
            deferred_kill[k] = fail;
            if (fail) kinslaying_victims++;

            const char *echo_text = NULL;
            switch (k)
            {
                case 0: echo_text = fail ?
                    "\"Alqualonde's Grief\"\nBlood stains starlit waves. Your hand remembers the swords at Alqualonde-first grief, first guilt." :
                    "The sorrow of Alqualonde passes over you-your spirit holds fast, blood unstained.";
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

    Term_xtra(TERM_XTRA_DELAY, 3000);

    /* ============================================================= */
    /* SCENE 8: Kinslaying Execution & Notifications               */
    /* ============================================================= */
    if (allow_kinslay && kinslaying_victims > 0)
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
    if (allow_kinslay && kinslaying_victims > 0) {
        Term_clear();
        print_heading_fade("Blood Is Demanded", TERM_RED);

        int row = 4;
        for (int k = 0; k < 3; k++) {
            if (!deferred_kill[k]) continue;

            const char *house =
                kinslayer_try_kill(k + 1, /*do_roll=*/false);
            if (!house) continue;               /* should not happen */

            char buf[96];
            strnfmt(buf, sizeof buf,
                    "A hero %s has fallen beneath your blade.", house);

            if (!fast_forward && !print_paragraph_fade(buf, TERM_RED, row))
                fast_forward = true;
            else if (fast_forward)
                print_paragraph(buf, TERM_RED);

            row += 3;
        }

        wait_prompt(PROMPT_RETURN_MIDDLE_EARTH);
    } else {
        /* no kinslaying scene – still give one clean exit prompt   */
        wait_prompt(PROMPT_RETURN_MIDDLE_EARTH);
    }

    metar.silmarils += final_sils;    
    log_info("Added %d Silmarils to metarun total (now %d)", final_sils, metar.silmarils);
    print_story(3, true);

    /* Restore the saved play-screen only after every narrative beat */
    screen_load();

    check_run_end();
    save_metaruns();
}


/* ======================  run-state logic  ====================== */
/* ------------------------------------------------------------------ *
 *  Decide whether the current run just ended, and react accordingly. *
 *  Message text adapts automatically if you set LOSECON_DEATHS = 1.  *
 * ------------------------------------------------------------------ */
static void check_run_end(void)
{
    int max_deaths = MAX(1, LOSECON_DEATHS - 3 * curse_flag_count(CUR_DEATH));

    if (metar.silmarils >= WINCON_SILMARILS) {
        log_info("Metarun VICTORY: %d Silmarils collected (goal: %d)", metar.silmarils, WINCON_SILMARILS);
        screen_save();
        Term_clear();
        
        print_heading_fade("The Trial's End", TERM_YELLOW);
        
        const char *victory_text = "Fifteen Silmarils reclaimed from Morgoth's crown! "
                                 "Hope kindles anew; your long trial approaches its end. "
                                 "Yet one final ordeal awaits—your ultimate destiny, "
                                 "as your true self faces the Last Trial.";
        
        print_paragraph_fade(victory_text, TERM_L_GREEN, 4);
        
        const char *implementation_note = "(This final trial is yet to be implemented.)";
        print_paragraph_fade(implementation_note, TERM_L_DARK, 8);
        
        wait_for_keypress_with_prompt("[Press any key to begin anew]");
        screen_load();
        
        start_new_metarun();

    } else if (metar.deaths >= max_deaths) {
        log_info("Metarun DEFEAT: %d deaths reached (limit: %d)", metar.deaths, max_deaths);
        screen_save();
        Term_clear();
        
        print_heading_fade("The Trial's End", TERM_RED);
        
        char defeat_text[256];
        strnfmt(defeat_text, sizeof(defeat_text),
                "%d hero%s fallen; the halls of Mandos echo with grief. "
                "This trial ends in shadow—the run is lost. "
                "Begin anew to reclaim lost hope.",
                max_deaths, (max_deaths == 1) ? " has" : "es have");
        
        print_paragraph_fade(defeat_text, TERM_WHITE, 4);
        
        wait_for_keypress_with_prompt("[Press any key to begin anew]");
        screen_load();
        
        start_new_metarun();
    }
}



/* ------------------------------------------------------------------
 *  Start a brand-new meta-run.
 *  We snapshot the finished run **after** the array has been grown,
 *  so we only write once and always with the final pointer.
 * ------------------------------------------------------------------ */
static void start_new_metarun(void)
{
    log_info("Starting new metarun (previous run ID: %d)", metar.id);
    clear_scorefile();
    /* Save old state */
    s16b old_max   = metarun_max;
    metarun *old   = metaruns;

    /* Try to allocate a new array for one more run */
    metarun *tmp = C_RNEW(old_max + 1, metarun);
    if (!tmp) {
        /* Allocation failed - keep everything as is */
        return;
    }

    /* Copy over the previous runs (if any) */
    if (old) {
        C_COPY(tmp, old, old_max, metarun);
    }

    /* Free the old array just once */
    FREE(old);

    /* Commit the new array and size */
    metaruns    = tmp;
    metarun_max = old_max + 1;

    /* Initialize the brand-new slot */
    reset_defaults(&metaruns[metarun_max - 1]);
    metaruns[metarun_max - 1].id = metar.id + 1;

    /* Update globals */
    current_run      = metarun_max - 1;
    metar             = metaruns[current_run];
    metarun_created  = true;

    /* Persist and prepare */
    save_metaruns();      /* safe now that metaruns≠NULL */ 
    ensure_run_dir(&metar);
    log_info("New metarun %d created and initialized", metar.id);
}


/*
 * Enhanced print_metarun_stats():
 * - Draws bracketed progress bars for Silmarils & Deaths with colored stars
 * - Displays numeric counts next to each bar
 * - Aligns labels & values for a cleaner layout
 * - Lists active curses with D: and (optionally) P: details
 */
/* Updated print_metarun_stats(): prettier layout, star & death bars, curses list */
void print_metarun_stats(void)
{
    int row = 1;
    int col = 2;
    char buf[128];
    int x;

    /* Save & clear screen */
    screen_save();
    Term_clear();

    /* Title */
    Term_putstr(col, row++, -1, TERM_YELLOW, "=== Current Story Statistics ===");
    row++;

    /* Run ID */
    snprintf(buf, sizeof buf, "Run-ID     : %u", metar.id);
    Term_putstr(col, row++, -1, TERM_WHITE, buf);
    row++;

    /* Silmarils bar */
    snprintf(buf, sizeof buf, "Silmarils  : ");
    Term_putstr(col, row, -1, TERM_WHITE, buf);
    x = col + strlen(buf);
    for (int i = 0; i < WINCON_SILMARILS; i++) {
        byte attr = (i < metar.silmarils) ? TERM_L_GREEN : TERM_L_WHITE;
        Term_putch(x++, row, attr, '*');
    }
    snprintf(buf, sizeof buf, "  (%d/%d)", metar.silmarils, WINCON_SILMARILS);
    Term_putstr(x + 1, row++, -1, TERM_WHITE, buf);
    // row++;

    /* Deaths bar */
    snprintf(buf, sizeof buf, "Deaths     : ");
    Term_putstr(col, row, -1, TERM_WHITE, buf);
    x = col + strlen(buf);
    for (int i = 0; i < LOSECON_DEATHS; i++) {
        byte attr = (i < metar.deaths) ? TERM_RED : TERM_L_WHITE;
        Term_putch(x++, row, attr, 'x');
    }
    snprintf(buf, sizeof buf, "  (%d/%d)", metar.deaths, LOSECON_DEATHS);
    Term_putstr(x + 1, row++, -1, TERM_WHITE, buf);
    row += 2;

    /* Active curses list */
    Term_putstr(col, row++, -1, TERM_YELLOW, "Active Curses:");
#ifdef DEBUG_CURSES
    Term_putstr(col, row++, -1, TERM_L_DARK, "(showing D:stacks and P:effect)");
#endif
    for (int id = 0; id < z_info->cu_max; id++) {
        byte cnt = CURSE_GET(id);
        if (!cnt) continue;
        /* Build line: id, name, D:count, optional P:text */
        cptr name = cu_name + cu_info[id].name;
#ifdef DEBUG_CURSES
        cptr pow = cu_text + cu_info[id].power;
        snprintf(buf, sizeof buf, " %2d: %-20s D:%d P:%s", id, name, cnt, pow);
#else
        snprintf(buf, sizeof buf, " %2d: %-20s D:%d", id, name, cnt);
#endif
        Term_putstr(col + 2, row++, -1, TERM_WHITE, buf);
    }
    row++;

    /* Prompt and restore */
    Term_putstr(col, row++, -1, TERM_L_DARK, "[Press any key to continue]");
    (void)inkey();
    screen_load();
}

/* compact table of all meta-runs */
void list_metaruns(void)
{
    screen_save();
    Term_clear();
    c_prt(TERM_L_GREEN, "Meta-run history", 1, 2);
    c_put_str(TERM_L_DARK,
              " ID       Sil  Dth  Res  Last played", 3, 2);

    int row = 4;
    for (s16b i = 0; i < metarun_max; i++) {
        const metarun *m = &metaruns[i];
        char res = (m->silmarils >= WINCON_SILMARILS) ? 'W' :
                   (m->deaths    >= LOSECON_DEATHS)   ? 'L' : ' ';
        char date[16];
        strftime(date, sizeof date, "%Y-%m-%d",
                 localtime((time_t*)&m->last_played));

        c_put_str((i == current_run) ? TERM_YELLOW : TERM_WHITE,
                  format("%08u   %2d   %2d   %c   %s",
                         m->id, m->silmarils, m->deaths, res, date),
                  row++, 2);

        if (row >= 23 && i+1 < metarun_max) {   /* page break */
            c_put_str(TERM_L_DARK, "[more – any key]", 23, 2);
            inkey();  Term_clear();
            row = 4;
            c_prt(TERM_L_GREEN, "Meta-run history (cont.)", 1, 2);
            c_put_str(TERM_L_DARK,
                      " ID       Sil  Dth  Res  Last played", 3, 2);
        }
    }
    c_put_str(TERM_L_DARK, "Press any key to return.", row+1, 2);
    inkey();
    screen_load();
}

void show_known_curses_menu(void)
{
    int shown = 0;
    int row = 2;
    int id;

    log_trace("show_known_curses_menu: metar.curses_seen=0x%08lX",
            (unsigned long) metar.curses_seen);


    /* Collect and count first */
    for (id = 0; id < (int)z_info->cu_max; id++)
        if (CURSE_SEEN(id)) {
                shown++;
                int seen = CURSE_SEEN(id) ? 1 : 0;
                log_trace("show_known_curses_menu: id=%d, name='%s', Seen=%d",
                id, cu_name + cu_info[id].name, seen);
            }
    if (!shown) {
        log_debug("No curses have been seen yet");
        msg_print("You have not identified any curses yet.");
        return;
    }

    log_info("Displaying %d known curses", shown);

    screen_save();
    Term_clear();
    Term_putstr(1, 0, -1, TERM_L_WHITE + TERM_SHADE, "Known Curses:");

    row = 2;

    /* Enable wrapped text helper */
    text_out_hook = text_out_to_screen;
    text_out_wrap = Term->wid - 4;   /* generous rhs margin */

    for (id = 0; id < (int)z_info->cu_max; id++)
    {
        if (!CURSE_SEEN(id)) continue;

        curse_type *c = &cu_info[id];
        cptr cname  = cu_name + c->name;
        cptr cdesc  = cu_text + c->text;
        cptr cpower = cu_text + c->power;

        /* Name */
        c_put_str(TERM_L_RED, cname, row, 1);
        row++;

        /* Description (wrapped) */
        Term_gotoxy(3, row);
        text_out_c(TERM_WHITE, cdesc);
        row += count_wrapped_lines(cdesc, text_out_wrap, 3);

        /* Optional power line */
        if (*cpower)
        {
            Term_gotoxy(3, row);
            text_out_c(TERM_L_DARK, cpower);
            row += count_wrapped_lines(cpower, text_out_wrap, 3);
        }

        /* Page wrap (match self_knowledge style) */
        if (row >= 21)
        {
            Term_putstr(1, row, -1, TERM_L_WHITE, "(press any key)");
            (void)inkey();
            Term_clear();
            Term_putstr(1, 0, -1, TERM_L_WHITE + TERM_SHADE, "Known Curses:");
            row = 2;
        }
    }

    Term_putstr(1, row+1, -1, TERM_L_WHITE, "(press any key)");
    (void)inkey();
    screen_load();
}
