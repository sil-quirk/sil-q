/* File: dungeon.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "blitz.h"
#include "externs.h"
#include "log/log.h"
#include "player/killer.h"
#include "metarun.h"
#include "score/score_runs.h"
#include "score/score_ui.h"
#include "sdl-sound.h"
#include "z-term.h"
#include <time.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

/* Countdown for forcing a redraw after showing the per-style banner */
int g_banner_force_redraw_remaining = 0;
static char g_active_partition_banner_text[1024] = "";
static bool g_active_partition_banner_consumes_input = false;
/* Banners shown while resolving a command should survive until the next
 * command prompt rather than being consumed by the arrival action itself. */
static bool g_active_partition_banner_skip_next_decay = false;

/* Morgoth vault tracking variables - file scope for cross-function access */
static int last_player_y = 0;
static int last_player_x = 0;
static bool was_in_morgoth_vault = false;
static bool morgoth_entry_preconfirmed = false;

static int last_partition_pi = -1;
static level_partition_kind last_partition_kind = LEVEL_PART_NONE;

/* Track which partitions have been narrated and the last narrated style. */
static u32b partition_narrated_mask = 0;
static int last_narrated_style_idx = -1;

/* Forward declarations for partition kind helpers (defined later in file). */
static bool is_big_partition_kind(level_partition_kind kind);
static bool is_small_cave_partition_kind(level_partition_kind kind);

/* Track greater-vault encounter XP so repeated warning prompts can't be farmed. */
static char greater_vault_xp_name[80] = "";
static bool greater_vault_xp_awarded = false;

static void snapshot_run_history(const char* reason)
{
    if (!character_generated || !p_ptr || p_ptr->is_dead)
        return;

    high_score preview;
    if (!build_live_preview_score(&preview))
        return;

    time_t now = time(NULL);
    if (now == (time_t)-1)
        now = 0;

    if (!score_runs_record_current_run(&preview, now, SCORE_RECORD_ALIVE)) {
        log_warn("run snapshot failed (%s)", reason ? reason : "unspecified");
    } else if (reason) {
        log_trace("run snapshot recorded (%s)", reason);
    }
}

static void reset_level_entry_tracking(void)
{
    g_labyrinth_view_active = false;
    g_banner_force_redraw_remaining = 0;
    g_active_partition_banner_text[0] = '\0';
    g_active_partition_banner_consumes_input = false;
    g_active_partition_banner_skip_next_decay = false;
    greater_vault_xp_name[0] = '\0';
    greater_vault_xp_awarded = false;
    last_partition_pi = -1;
    last_partition_kind = LEVEL_PART_NONE;
    partition_narrated_mask = 0;
    last_narrated_style_idx = -1;
}

static bool banner_messages_use_stairs(void)
{
#if defined(__ANDROID__) || defined(SIL_IOS)
    const bool default_value = false;
#else
    const bool default_value = true;
#endif

    if (!op_ptr)
        return default_value;

    return op_ptr->opt[OPT_banner_message_stairs];
}

static byte narrative_banner_turn_setting(void)
{
    if (!op_ptr)
        return DEFAULT_NARRATIVE_BANNER_TURNS;

    if (op_ptr->narrative_banner_turns > NARRATIVE_BANNER_TURNS_MAX)
        return DEFAULT_NARRATIVE_BANNER_TURNS;

    return op_ptr->narrative_banner_turns;
}

static int narrative_banner_rows_for_text(cptr text)
{
    int wid, h;
    const char* p = text;
    int printed_lines = 0;
    enum { MAX_LINES2 = 32, MAX_LEN2 = 255 };
    bool stair_layout = banner_messages_use_stairs();

    if (!text || !text[0])
        return 0;
    if (!Term || !angband_term[0] || (Term != angband_term[0]))
        return 0;

    Term_get_size(&wid, &h);
    if (h <= 1)
        return 0;

    while (*p && printed_lines < MAX_LINES2 && (1 + printed_lines) < h)
    {
        int indent = 14 + (stair_layout ? (2 * printed_lines) : 0);
        int avail;
        int linelen = 0;

        if (use_bigtile && (((indent - COL_MAP) & 1) != 0))
            indent++;
        if (indent >= wid - 1)
            break;

        avail = wid - indent - 1;
        if (avail < 8)
            avail = 8;

        while (*p && (unsigned char)*p <= ' ')
        {
            if (*p == '\n')
            {
                p++;
                break;
            }
            p++;
        }

        while (*p)
        {
            const char* w = p;
            int wlen;
            int need;

            if (*p == '\n')
            {
                p++;
                break;
            }

            while (*p && *p != '\n' && !isspace((unsigned char)*p))
                p++;
            wlen = (int)(p - w);

            if ((wlen > avail) && (linelen == 0))
            {
                int take = (wlen > avail) ? avail : wlen;
                if (take > MAX_LEN2)
                    take = MAX_LEN2;
                p = w + take;
                linelen = take;
                break;
            }

            need = (linelen ? 1 : 0) + wlen;
            if ((linelen + need <= avail) && (linelen + need <= MAX_LEN2))
            {
                linelen += need;
            }
            else
            {
                p = w;
                break;
            }

            while (*p && isspace((unsigned char)*p))
            {
                if (*p == '\n')
                    break;
                p++;
            }
            if (*p == '\n')
            {
                p++;
                break;
            }
        }

        if (linelen == 0)
            break;

        printed_lines++;
    }

    return printed_lines;
}

int active_narrative_banner_rows(void)
{
    if (!g_active_partition_banner_text[0]
        || (g_banner_force_redraw_remaining <= 0))
        return 0;

    return narrative_banner_rows_for_text(g_active_partition_banner_text);
}

static void keep_player_visible_for_narrative_banner(cptr text)
{
    int banner_rows;

    if (!p_ptr || !text || !text[0] || p_ptr->is_dead)
        return;

    banner_rows = narrative_banner_rows_for_text(text);
    if (banner_rows <= 0)
        return;

    if (p_ptr->py >= p_ptr->wy + banner_rows)
        return;

    if (modify_panel(p_ptr->py - banner_rows, p_ptr->wx))
    {
        if (p_ptr->redraw)
            redraw_stuff();
        if (p_ptr->window)
            window_stuff();
        Term_fresh();
    }
}

static void queue_active_partition_banner(void)
{
    int wid, h;
    const char* p = g_active_partition_banner_text;
    int printed_lines = 0;
    enum { MAX_LINES2 = 32, MAX_LEN2 = 255 };
    bool stair_layout = banner_messages_use_stairs();

    if (!p[0] || (g_banner_force_redraw_remaining <= 0))
        return;
    if (!Term || !angband_term[0] || (Term != angband_term[0]))
        return;
    if (character_icky > 0)
        return;

    Term_get_size(&wid, &h);
    if (h <= 1)
        return;

    sdl_story_font_enable();

    while (*p && printed_lines < MAX_LINES2 && (1 + printed_lines) < h)
    {
        int indent = 14 + (stair_layout ? (2 * printed_lines) : 0);
        int avail;
        char buf[MAX_LEN2 + 1];
        int linelen = 0;

        if (use_bigtile && (((indent - COL_MAP) & 1) != 0))
            indent++;
        if (indent >= wid - 1)
            break;

        avail = wid - indent - 1;
        if (avail < 8)
            avail = 8;

        buf[0] = '\0';

        while (*p && (unsigned char)*p <= ' ')
        {
            if (*p == '\n')
            {
                p++;
                break;
            }
            p++;
        }

        while (*p)
        {
            const char* w = p;
            int wlen;
            int need;

            if (*p == '\n')
            {
                p++;
                break;
            }

            while (*p && *p != '\n' && !isspace((unsigned char)*p))
                p++;
            wlen = (int)(p - w);

            if ((wlen > avail) && (linelen == 0))
            {
                int take = (wlen > avail) ? avail : wlen;
                if (take > MAX_LEN2)
                    take = MAX_LEN2;
                memcpy(buf, w, (size_t)take);
                linelen = take;
                buf[linelen] = '\0';
                p = w + take;
                break;
            }

            need = (linelen ? 1 : 0) + wlen;
            if ((linelen + need <= avail) && (linelen + need <= MAX_LEN2))
            {
                if (linelen)
                    buf[linelen++] = ' ';
                memcpy(buf + linelen, w, (size_t)wlen);
                linelen += wlen;
                buf[linelen] = '\0';
            }
            else
            {
                p = w;
                break;
            }

            while (*p && isspace((unsigned char)*p))
            {
                if (*p == '\n')
                    break;
                p++;
            }
            if (*p == '\n')
            {
                p++;
                break;
            }
        }

        if (linelen == 0)
            break;

        c_put_str(TERM_ORANGE, buf, 1 + printed_lines, indent);
        /* Clear only the next tile position to prevent glow overlay from showing through */
        int erase_len = use_bigtile ? 2 : 1;
        if (indent + linelen + erase_len <= wid)
            Term_erase(indent + linelen, 1 + printed_lines, erase_len);
        printed_lines++;
    }

    sdl_story_font_disable();
}

static void narrative_banner_pre_fresh_hook(void)
{
    queue_active_partition_banner();
}

bool active_narrative_banner_consumes_input(void)
{
    return g_active_partition_banner_consumes_input
        && g_active_partition_banner_text[0]
        && (g_banner_force_redraw_remaining > 0);
}

void clear_active_narrative_banner(void)
{
    g_banner_force_redraw_remaining = 0;
    g_active_partition_banner_text[0] = '\0';
    g_active_partition_banner_consumes_input = false;
    g_active_partition_banner_skip_next_decay = false;
}

/*
 * Transition templates for partition narrative.
 * Each template takes (old_S, new_S) as %s arguments.
 */
static const char* transition_templates[] = {
    "The %s gives way to %s.",
    "You leave the %s behind; ahead lies %s.",
    "The %s fades. Now %s surrounds you.",
    "Gone is the %s. In its place, %s.",
    "The %s recedes as %s closes around you.",
};
#define NUM_TRANSITION_TEMPLATES 5

static const char* partition_structural_text(level_partition_kind kind)
{
    switch (kind)
    {
    case LEVEL_PART_LABYRINTH:
        return "The passage splits and twists into a dark labyrinth.";
    case LEVEL_PART_CHASM:
        return "A vast darkness yawns below; only narrow bridges span the gulf.";
    case LEVEL_PART_BIG_CAVE:
        return "A great cavern opens before you, its roof lost in shadow.";
    default:
        return NULL;
    }
}

static const char* big_cave_elemental_text(void)
{
    big_cave_type_t cave_type =
        level_partition_big_cave_type_for_point(p_ptr->py, p_ptr->px);
    switch (cave_type)
    {
    case BIG_CAVE_FIRE:
        return "Searing heat closes around you, and you feel your strength waning.";
    case BIG_CAVE_ICE:
        return "Bitter cold gnaws at your bones, and you shiver with a deathly chill.";
    case BIG_CAVE_POIS:
        return "A noxious miasma fills the air, and poison seeps into your lungs.";
    default:
        return "You feel exposed and vulnerable in this vast empty space.";
    }
}

static void append_narrative_piece(char* buf, size_t size, const char* text)
{
    if (!text || !text[0])
        return;

    if (buf[0])
        SDL_strlcat(buf, " ", size);
    SDL_strlcat(buf, text, size);
}

static void build_partition_narrative_text(int old_sidx, int new_sidx,
    level_partition_kind kind, char* buf, size_t size)
{
    const char* structural = partition_structural_text(kind);
    bool is_transition;

    if (!buf || size == 0)
        return;
    buf[0] = '\0';

    if (structural)
    {
        append_narrative_piece(buf, size, structural);
        if (kind == LEVEL_PART_BIG_CAVE)
        {
            const char* elem = big_cave_elemental_text();
            if (elem)
                append_narrative_piece(buf, size, elem);
        }
    }

    if (is_small_cave_partition_kind(kind))
    {
        append_narrative_piece(buf, size,
            "The air grows close and frowsty in a cramped cave.");
    }

    is_transition = (old_sidx >= 0 && old_sidx != new_sidx);
    if (is_transition)
    {
        const char* old_s = styles_get_style_short_desc(old_sidx);
        const char* new_s = styles_get_style_short_desc(new_sidx);
        if (old_s && new_s)
        {
            char transition_buf[256];
            int tmpl = rand_int(NUM_TRANSITION_TEMPLATES);
            strnfmt(transition_buf, sizeof(transition_buf),
                transition_templates[tmpl], old_s, new_s);
            append_narrative_piece(buf, size, transition_buf);
        }
        else
        {
            const char* m1 = styles_get_style_m1(new_sidx);
            append_narrative_piece(buf, size, m1);
        }
    }
    else
    {
        const char* m1 = styles_get_style_m1(new_sidx);
        append_narrative_piece(buf, size, m1);
    }

    append_narrative_piece(buf, size, styles_get_style_m2(new_sidx));
}

static void display_narrative_text(cptr text, int narrative_mode,
    bool line_delay)
{
    if (!text || !text[0])
        return;

    if (narrative_mode == PARTITION_NARRATIVE_MESSAGE)
    {
        msg_print(text);
        return;
    }

    if (narrative_mode != PARTITION_NARRATIVE_BANNER)
        return;

    keep_player_visible_for_narrative_banner(text);

    g_term_pre_fresh_hook = narrative_banner_pre_fresh_hook;
    g_active_partition_banner_text[0] = '\0';
    print_fade_centered_at_row(text, 1, false, line_delay);
    SDL_strlcpy(g_active_partition_banner_text, text,
        sizeof(g_active_partition_banner_text));
    g_active_partition_banner_consumes_input =
        (narrative_banner_turn_setting() == 0);
    g_active_partition_banner_skip_next_decay =
        (p_ptr && (p_ptr->command_cmd != 0));
    g_banner_force_redraw_remaining = g_active_partition_banner_consumes_input
        ? 1
        : narrative_banner_turn_setting();
}

static void display_partition_narrative(int old_sidx, int new_sidx,
    level_partition_kind kind)
{
    char buf[1024];

    build_partition_narrative_text(old_sidx, new_sidx, kind, buf, sizeof(buf));
    display_narrative_text(buf, PARTITION_NARRATIVE_MESSAGE, false);
}

static void display_partition_narrative_banner(int old_sidx, int new_sidx,
    level_partition_kind kind, bool line_delay)
{
    char buf[1024];

    build_partition_narrative_text(old_sidx, new_sidx, kind, buf, sizeof(buf));
    display_narrative_text(buf, PARTITION_NARRATIVE_BANNER, line_delay);
}

static void morgoth_prompt_controller_label(int binding, const char* fallback,
    char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    sdl_gamepad_action_binding_short_label(binding, buf, buflen);
    if (!buf[0] || streq(buf, "(unbound)") || streq(buf, "Multiple"))
        SDL_strlcpy(buf, fallback ? fallback : "", buflen);
}

static bool morgoth_prompt_confirm_key(int ch)
{
    if (steamdeck_controls_active() && ch == steamdeck_confirm_key())
        return true;

    return portable_controls_active()
        && ((ch == ' ') || (ch == '\r') || (ch == '\n'));
}

static bool morgoth_prompt_cancel_key(int ch)
{
    if (steamdeck_controls_active() && ch == steamdeck_back_key()
        && ch != steamdeck_confirm_key())
        return true;

    return (ch == ESCAPE);
}

static bool confirm_enter_morgoth_hall(void)
{
    char ch;
    int wid, hgt;

    static const char* text[] = {
        "Beyond this passage lies the black hall of Morgoth Bauglir,",
        "the Dark Enemy, and the last of the Iron Hells.",
        "",
        "If you pass within, you may not return until you bear a Silmaril.",
        "Steel yourself: to enter is to choose doom or glory.",
        NULL,
    };

    /* Paranoia */
    message_flush();

    /* Get terminal size */
    Term_get_size(&wid, &hgt);

    /* Save screen */
    screen_save();
    Term_clear();

    /* Title */
    {
        const char* title = "The Iron Gates of Angband";
        int col = (wid - (int)strlen(title)) / 2;
        if (col < 1)
            col = 1;
        Term_putstr(col, 2, -1, TERM_L_RED, title);
    }

    /* Body */
    {
        int row = 6;
        for (int i = 0; text[i] && row < hgt - 5; ++i)
        {
            const char* line = text[i];
            if (!line[0])
            {
                row++;
                continue;
            }

            int len = (int)strlen(line);
            int col = (wid - len) / 2;
            if (col < 1)
                col = 1;

            byte attr = (i == 3) ? TERM_L_RED : TERM_WHITE;
            Term_putstr(col, row, -1, attr, line);
            row++;
        }
    }

    bool steamdeck = steamdeck_controls_active();

    /* Prompt */
    {
        char prompt[80];

        if (steamdeck)
        {
            char confirm_label[16];
            char back_label[16];

            morgoth_prompt_controller_label(steamdeck_confirm_key(), "A",
                confirm_label, sizeof(confirm_label));
            morgoth_prompt_controller_label(steamdeck_back_key(), "B",
                back_label, sizeof(back_label));
            strnfmt(prompt, sizeof(prompt), "Enter Morgoth's hall? [%s/%s]",
                confirm_label, back_label);
        }
        else
        {
            SDL_strlcpy(prompt, portable_controls_active()
                    ? "Enter Morgoth's hall? [y/n/sp]"
                    : "Enter Morgoth's hall? [y/n]",
                sizeof(prompt));
        }

        int col = (wid - (int)strlen(prompt)) / 2;
        if (col < 1)
            col = 1;
        Term_putstr(col, hgt - 3, -1, TERM_YELLOW, prompt);
    }

    /* Get an acceptable answer */
    while (true)
    {
        ch = inkey();
        if (quick_messages)
            break;
        if (morgoth_prompt_cancel_key(ch))
            break;
        if (strchr("YyNn", ch) || morgoth_prompt_confirm_key(ch))
            break;
        bell("Illegal response to a 'yes/no' question!");
    }

    /* Restore screen */
    screen_load();

    /* Normal negation */
    if ((ch != 'Y') && (ch != 'y') && !morgoth_prompt_confirm_key(ch))
        return (false);

    return (true);
}

bool preconfirm_enter_morgoth_hall(void)
{
    if (!confirm_enter_morgoth_hall())
        return false;
    morgoth_entry_preconfirmed = true;
    return true;
}

static void update_labyrinth_view_state(bool handle_now)
{
    if (!p_ptr || p_ptr->is_dead)
        return;

    level_partition_kind kind = level_partition_kind_for_point(p_ptr->py, p_ptr->px);
    bool want = (kind == LEVEL_PART_LABYRINTH);

    if (want == g_labyrinth_view_active)
        return;

    g_labyrinth_view_active = want;

    p_ptr->redraw |= (PR_MAP);
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_MONSTERS | PU_DISTANCE);

    if (handle_now)
        handle_stuff();
}

static bool is_big_partition_kind(level_partition_kind kind)
{
    return (kind == LEVEL_PART_LABYRINTH || kind == LEVEL_PART_BIG_CAVE
        || kind == LEVEL_PART_CHASM);
}

static bool is_small_cave_partition_kind(level_partition_kind kind)
{
    return (kind == LEVEL_PART_CAVEY);
}

static byte partition_discovery_lore_flag(level_partition_kind kind)
{
    if (!p_ptr)
        return 0;

    switch (kind)
    {
    case LEVEL_PART_LABYRINTH:
        return DISC_LORE_LABYRINTH;
    case LEVEL_PART_CHASM:
        return DISC_LORE_CHASM;
    case LEVEL_PART_BIG_CAVE:
    {
        switch (level_partition_big_cave_type_for_point(p_ptr->py, p_ptr->px))
        {
        case BIG_CAVE_ICE:
            return DISC_LORE_BIG_CAVE_ICE;
        case BIG_CAVE_FIRE:
            return DISC_LORE_BIG_CAVE_FIRE;
        case BIG_CAVE_POIS:
            return DISC_LORE_BIG_CAVE_POIS;
        default:
            return 0;
        }
    }
    default:
        return 0;
    }
}

static int discovery_narrative_mode(bool force_message, int narrative_mode)
{
    if (!force_message)
        return narrative_mode;

    if (!op_ptr)
        return PARTITION_NARRATIVE_OFF;

    switch (op_ptr->level_entry_narrative_mode)
    {
    case LEVEL_ENTRY_NARRATIVE_BANNER_DELAY:
    case LEVEL_ENTRY_NARRATIVE_BANNER:
        return PARTITION_NARRATIVE_BANNER;
    case LEVEL_ENTRY_NARRATIVE_MESSAGE:
        return PARTITION_NARRATIVE_MESSAGE;
    default:
        return PARTITION_NARRATIVE_OFF;
    }
}

static bool discovery_narrative_line_delay(bool force_message)
{
    return force_message && op_ptr
        && (op_ptr->level_entry_narrative_mode
            == LEVEL_ENTRY_NARRATIVE_BANNER_DELAY);
}

static cptr partition_discovery_lore_text(level_partition_kind kind)
{
    big_cave_type_t cave_type = BIG_CAVE_NONE;

    if (kind == LEVEL_PART_BIG_CAVE && p_ptr)
        cave_type = level_partition_big_cave_type_for_point(p_ptr->py, p_ptr->px);

    return partition_config_get_discovery_text(kind, cave_type);
}

static void maybe_award_partition_discovery_xp(level_partition_kind kind,
    int narrative_mode, bool line_delay)
{
    byte bit = partition_discovery_lore_flag(kind);
    cptr text = partition_discovery_lore_text(kind);

    if (!bit || !text)
        return;

    if (p_ptr->discovery_lore_flags & bit)
        return;

    p_ptr->discovery_lore_flags |= bit;
    gain_exp(300);
    display_narrative_text(text, narrative_mode, line_delay);
}

static cptr vault_entry_message_for_name(cptr vault_name)
{
    int i;

    if (!vault_name || !vault_name[0])
        return NULL;

    for (i = 0; i < z_info->v_max; i++)
    {
        vault_type* v_ptr = &v_info[i];
        cptr name;

        if (!v_ptr->name)
            continue;

        name = v_name + v_ptr->name;
        if (strcmp(name, vault_name) != 0)
            continue;

        if (!v_ptr->message)
            return NULL;

        return v_text + v_ptr->message;
    }

    return NULL;
}

static void queue_message_recall_only(cptr text)
{
    if (!text || !text[0])
        return;

    if (character_generated && p_ptr && !p_ptr->is_dead)
        message_add(text, MSG_GENERIC);

    if (!p_ptr)
        return;

    p_ptr->window |= PW_MESSAGE;
    window_stuff();
}

static void describe_greater_vault_entry(cptr vault_name)
{
    int narrative_mode = op_ptr ? op_ptr->partition_narrative_mode
                                : PARTITION_NARRATIVE_MESSAGE;
    cptr text = vault_entry_message_for_name(vault_name);

    if (!text)
        return;

    if (narrative_mode == PARTITION_NARRATIVE_BANNER)
    {
        /* Banner mode already shows the text on the main term, so push it
         * directly into recall and refresh message windows immediately. */
        queue_message_recall_only(text);
        display_narrative_text(text, PARTITION_NARRATIVE_BANNER, false);
        return;
    }

    /* Great vault entries should always land in the message log too. */
    msg_print(text);
}

static void handle_partition_entry(bool force_message, int narrative_mode)
{
    if (!p_ptr || p_ptr->is_dead)
        return;

    int pi = level_partition_index_for_point(p_ptr->py, p_ptr->px);
    level_partition_kind kind = level_partition_kind_for_point(p_ptr->py, p_ptr->px);
    int sidx = styles_decode_color_style(cave_color[p_ptr->py][p_ptr->px]);

    bool is_big = is_big_partition_kind(kind);
    bool was_big = is_big_partition_kind(last_partition_kind);
    bool entered_big = false;
    if (force_message)
    {
        entered_big = is_big;
    }
    else if (is_big)
    {
        if (!was_big)
            entered_big = true;
        else if (pi != last_partition_pi || kind != last_partition_kind)
            entered_big = true;
    }

    if (entered_big)
        maybe_award_partition_discovery_xp(
            kind, discovery_narrative_mode(force_message, narrative_mode),
            discovery_narrative_line_delay(force_message));

    if ((pi >= 0) && (pi < 25) && (sidx >= 0))
    {
        u32b bit = (u32b)(1U << pi);
        if (!(partition_narrated_mask & bit))
        {
            if (narrative_mode == PARTITION_NARRATIVE_BANNER)
                display_partition_narrative_banner(
                    last_narrated_style_idx, sidx, kind, false);
            else if (narrative_mode == PARTITION_NARRATIVE_MESSAGE)
                display_partition_narrative(last_narrated_style_idx, sidx, kind);

            if (is_small_cave_partition_kind(kind))
                msg_print("Here torch and lamp drink their fuel twice as fast.");

            partition_narrated_mask |= bit;
            last_narrated_style_idx = sidx;
        }
    }

    last_partition_pi = pi;
    last_partition_kind = kind;
}

/* Track last depth for music changes (moved from dungeon() for proper reset) */
static int last_music_depth = -999;

/* Track first entry to skip level sound (moved from dungeon() for proper reset) */
static bool first_entry_to_dungeon = true;

/* True while the post-mortem spectator viewport is active. */
static bool death_spectator_mode = false;

/*
 * Reset all dungeon-related static state for a new game.
 * Called from re_init_some_things() to ensure clean state
 * when starting a new game after death without restarting the app.
 */
void reset_dungeon_state(void)
{
    /* Reset file-scope static variables */
    last_player_y = 0;
    last_player_x = 0;
    was_in_morgoth_vault = false;
    morgoth_entry_preconfirmed = false;
    death_spectator_mode = false;
    g_active_partition_banner_text[0] = '\0';

    /* Reset music/sound tracking */
    last_music_depth = -999;
    first_entry_to_dungeon = true;

    /* Reset level entry tracking */
    reset_level_entry_tracking();
}

/* Forward declarations for spectator helpers. */
static bool death_spectator_command_allowed(int command);
static void death_spectator_prepare_display(void);
bool death_spectator_active(void);
/*
 * Return a "feeling" (or NULL) about an item.  Method 1 (Weak).
 * Sil - this method can't distinguish artefacts from ego items
 */
int value_check_aux1(const object_type* o_ptr)
{
    /* Artefacts */
    if (artefact_p(o_ptr))
    {
        /* Normal */
        return (INSCRIP_EXCELLENT);
    }

    /* Ego-Items */
    if (ego_item_p(o_ptr))
    {
        /* Normal */
        return (INSCRIP_EXCELLENT);
    }

    /* Default to "average" */
    return (INSCRIP_AVERAGE);
}

/*
 * Returns true if this object can be pseudo-ided.
 */
bool can_be_pseudo_ided(const object_type* o_ptr)
{
    /* Valid "tval" codes */
    switch (o_ptr->tval)
    {
    case TV_ARROW:
    case TV_BOW:
    case TV_DIGGING:
    case TV_HAFTED:
    case TV_POLEARM:
    case TV_SWORD:
    case TV_BOOTS:
    case TV_GLOVES:
    case TV_HELM:
    case TV_CROWN:
    case TV_SHIELD:
    case TV_CLOAK:
    case TV_SOFT_ARMOR:
    case TV_MAIL:
    {
        return (true);
        break;
    }
    case TV_LIGHT:
    {
        if (o_ptr->sval == SV_LIGHT_LANTERN)
            return (true);
        if (o_ptr->sval == SV_LIGHT_LESSER_JEWEL)
            return (true);
        if (o_ptr->sval == SV_LIGHT_FEANORIAN)
            return (true);
        break;
    }
    }
    return (false);
}

/*
 * Pseudo-id an item
 */
void pseudo_id(object_type* o_ptr)
{
    int feel;

    char o_name[80];

    /* Skip non-sense machines */
    if (!can_be_pseudo_ided(o_ptr))
        return;

    /* It is known, no information needed */
    if (object_known_p(o_ptr))
        return;

    feel = value_check_aux1(o_ptr);

    /* Skip non-feelings */
    if (!feel)
        return;

    /* Get an object description */
    object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

    /* Sense the object */
    o_ptr->discount = feel;

    /* The object has been "sensed" */
    o_ptr->ident |= (IDENT_SENSE);
}

void pseudo_id_everything(void)
{
    int i;
    object_type* o_ptr;

    for (i = 1; i < o_max; i++)
    {
        /* Get the next object from the dungeon */
        o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Ignore known objects */
        if (object_known_p(o_ptr))
            continue;

        /* Pseudo-id it */
        pseudo_id(o_ptr);
    }
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        /* Get the object */
        o_ptr = &inventory[i];

        /* Ignore empty objects */
        if (!o_ptr->k_idx)
            continue;

        /* Ignore known objects */
        if (object_known_p(o_ptr))
            continue;

        /* Pseudo-id it */
        pseudo_id(o_ptr);
    }

    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    handle_stuff();
}

void id_everything(void)
{
    int i;
    object_type* o_ptr;

    for (i = 1; i < o_max; i++)
    {
        /* Get the next object from the dungeon */
        o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Ignore known objects */
        if (object_known_p(o_ptr))
            continue;

        /* Identify it */
        ident(o_ptr);
    }
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        /* Get the object */
        o_ptr = &inventory[i];

        /* Ignore empty objects */
        if (!o_ptr->k_idx)
            continue;

        /* Ignore known objects */
        if (object_known_p(o_ptr))
            continue;

        /* Identify it */
        ident(o_ptr);
    }

    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    handle_stuff();
}

/*
 * automatically identify items of {special} types that the player knows about
 */
void id_known_specials(void)
{
    int i;
    object_type* o_ptr;

    for (i = 1; i < o_max; i++)
    {
        /* Get the next object from the dungeon */
        o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;

        /* Automatically identify any special items you have seen before */
        if (object_has_ego(o_ptr) && !object_known_p(o_ptr))
        {
            bool all_aware = true;
            byte ego_pfx = object_ego_prefix(o_ptr);
            byte ego_sfx = object_ego_suffix(o_ptr);

            if (ego_pfx && !e_info[ego_pfx].aware)
                all_aware = false;
            if (ego_sfx && !e_info[ego_sfx].aware)
                all_aware = false;

            if (!object_uses_smithing_difficulty(o_ptr))
            {
                if (all_aware)
                    ident(o_ptr);
            }
        }
    }
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        /* Get the object */
        o_ptr = &inventory[i];

        /* Ignore empty objects */
        if (!o_ptr->k_idx)
            continue;

        /* Automatically identify any special items you have seen before */
        if (object_has_ego(o_ptr) && !object_known_p(o_ptr))
        {
            bool all_aware = true;
            byte ego_pfx = object_ego_prefix(o_ptr);
            byte ego_sfx = object_ego_suffix(o_ptr);

            if (ego_pfx && !e_info[ego_pfx].aware)
                all_aware = false;
            if (ego_sfx && !e_info[ego_sfx].aware)
                all_aware = false;

            if (!object_uses_smithing_difficulty(o_ptr))
            {
                if (all_aware)
                    ident(o_ptr);
            }
        }
    }

    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);
    handle_stuff();
}

/*
 *  Determines how many points of health/song is regenerated next round
 *  assuming it increases by 'max' points every 'regen_period'.
 *  Note that players use 'playerturn' and monsters use 'turn'.
 *  This lets hasted players regenerate at the right speed.
 */

int regen_amount(int turn_number, int max, int regen_period)
{
    int regen_so_far, regen_next;

    if (turn_number == 0)
    {
        /* do nothing on the first turn of the game */
        return (0);
    }
    if ((turn_number % regen_period) > 0)
    {
        regen_so_far
            = (max * ((turn_number - 1) % regen_period)) / regen_period;
        regen_next = (max * ((turn_number) % regen_period)) / regen_period;
    }
    else
    {
        regen_so_far
            = (max * ((turn_number - 1) % regen_period)) / regen_period;
        regen_next = (max * (regen_period)) / regen_period;
    }

    return (regen_next - regen_so_far);
}

/*
 * Regenerate hit points
 */
static void regenhp(int regen_multiplier)
{
    int old_chp;

    // exit immediately if the multiplier is zero (avoids div by zero error)
    if (regen_multiplier == 0)
        return;

    /* Save the old hitpoints */
    old_chp = p_ptr->chp;

    /* Work out how much increase is due */
    /* where the player should get completely healed every PY_REGEN_HP_PERIOD
     * player turns */

    p_ptr->chp += regen_amount(
        playerturn, p_ptr->mhp, PY_REGEN_HP_PERIOD / regen_multiplier);

    /* Fully healed */
    if (p_ptr->chp >= p_ptr->mhp)
    {
        p_ptr->chp = p_ptr->mhp;
    }

    /* Notice changes */
    if (old_chp != p_ptr->chp)
    {
        /* Redraw */
        p_ptr->redraw |= (PR_HP);

        /* Window stuff */
        p_ptr->window |= (PW_PLAYER_0);
    }
}

/*
 * Regenerate mana points
 */
static void regenmana(int regen_multiplier)
{
    int old_csp;

    // exit immediately if the multiplier is zero (avoids div by zero error)
    if (regen_multiplier == 0)
        return;

    // don't regenerate voice if singing
    if (!singing(SNG_NOTHING))
        return;

    /* Save the old hitpoints */
    old_csp = p_ptr->csp;

    /* Work out how much increase is due */
    /* where the player should get completely recovered every PY_REGEN_SP_PERIOD
     * player turns */

    p_ptr->csp += regen_amount(
        playerturn, p_ptr->msp, PY_REGEN_SP_PERIOD / regen_multiplier);

    /* Fully recovered */
    if (p_ptr->csp >= p_ptr->msp)
    {
        p_ptr->csp = p_ptr->msp;
    }

    /* Redraw mana */
    if (old_csp != p_ptr->csp)
    {
        /* Redraw */
        p_ptr->redraw |= (PR_VOICE);

        /* Window stuff */
        p_ptr->window |= (PW_PLAYER_0);
    }
}

/*
 * Regenerate the monsters (once per 100 game turns)
 */

static void regen_monsters(void)
{
    int i;
    int regen_period;

    /* Regenerate everyone */
    for (i = 1; i < mon_max; i++)
    {
        /* Check the i'th monster */
        monster_type* m_ptr = &mon_list[i];
        monster_race* r_ptr = &r_info[m_ptr->r_idx];

        /* Skip dead monsters */
        if (!m_ptr->r_idx)
            continue;

        /* Allow hp regeneration, if needed. */
        if (m_ptr->hp != m_ptr->maxhp)
        {
            /* Some monsters regenerate quickly */
            if (r_ptr->flags2 & (RF2_REGENERATE))
            {
                regen_period = MON_REGEN_HP_PERIOD / 5;
            }
            else
            {
                regen_period = MON_REGEN_HP_PERIOD;
            }

            m_ptr->hp += regen_amount(turn / 10, m_ptr->maxhp, regen_period);

            /* Do not over-regenerate */
            if (m_ptr->hp > m_ptr->maxhp)
                m_ptr->hp = m_ptr->maxhp;

            /* Fully healed -> flag minimum range for recalculation */
            if (m_ptr->hp == m_ptr->maxhp)
                m_ptr->min_range = 0;
        }

        /* Allow mana regeneration, if needed. */
        if (m_ptr->mana != MON_MANA_MAX)
        {
            // can only regenerate mana if not singing
            if (m_ptr->song == SNG_NOTHING)
            {
                m_ptr->mana += regen_amount(
                    turn / 10, MON_MANA_MAX, MON_REGEN_SP_PERIOD);

                /* Do not over-regenerate */
                if (m_ptr->mana > MON_MANA_MAX)
                    m_ptr->mana = MON_MANA_MAX;

                /* Fully healed -> flag minimum range for recalculation */
                if (m_ptr->mana == MON_MANA_MAX)
                    m_ptr->min_range = 0;
            }
        }
    }
}

/*
 * If player has inscribed the object with "!!", let him know when it's
 * recharged. -LM-
 */
static void recharged_notice(object_type* o_ptr)
{
    char o_name[120];

    cptr s;

    /* No inscription */
    if (!o_ptr->obj_note)
        return;

    /* Find a '!' */
    s = strchr(quark_str(o_ptr->obj_note), '!');

    /* Process notification request. */
    while (s)
    {
        /* Find another '!' */
        if (s[1] == '!')
        {
            /* Describe (briefly) */
            object_desc(o_name, sizeof(o_name), o_ptr, false, 0);

            /*Disturb the player*/
            disturb(0, 0);

            /* Notify the player */
            if (o_ptr->number > 1)
                msg_format("Your %s are all recharged.", o_name);

            /*artefacts*/
            else if (o_ptr->name1)
            {
                msg_format("The %s has recharged.", o_name);
            }

            /*single, non-artefact items*/
            else
                msg_format("Your %s has recharged.", o_name);

            /* Done. */
            return;
        }

        /* Keep looking for '!'s */
        s = strchr(s + 1, '!');
    }
}

/*
 * Scan for artifacts within 22 tiles of player and mark them as seen.
 * This allows players to skip full exploration while still tracking artifacts.
 * Only scans the area that changed (player moved or objects shifted).
 */
static void scan_artifacts_near_player(void)
{
    int py = p_ptr->py;
    int px = p_ptr->px;
    int radius = 22;
    
    /* Scan 44x44 area centered on player */
    for (int dy = -radius; dy <= radius; dy++)
    {
        for (int dx = -radius; dx <= radius; dx++)
        {
            int y = py + dy;
            int x = px + dx;
            
            /* Skip out of bounds */
            if (!in_bounds(y, x))
                continue;

            /* Only consider grids the player can actually see */
            if (!player_can_see_bold(y, x))
                continue;
            
            /* Check for objects at this location */
            s16b this_o_idx = cave_o_idx[y][x];
            
            while (this_o_idx)
            {
                object_type* o_ptr = &o_list[this_o_idx];
                
                /* If this is an artifact that hasn't been marked seen yet */
                if (o_ptr->name1
                    && !(a_info[o_ptr->name1].seen & ART_SEEN_PHYSICAL))
                {
                    a_info[o_ptr->name1].seen |= ART_SEEN_PHYSICAL;
                    
                    /* Optional: log for debugging */
                    if (cheat_peek)
                    {
                        char o_name[80];
                        object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
                        msg_format("Artifact marked as seen: %s", o_name);
                    }
                }
                
                /* Next object in this square */
                this_o_idx = o_ptr->next_o_idx;
            }
        }
    }
}

/*
 * Handle certain things once every 10 game turns
 */
static void process_world(void)
{
    int i, j;

    object_type* o_ptr;

    bool was_ghost = false;

    /* Check for Tulkas quest interaction every turn */
    check_tulkas_quest_interaction();

    /* Check for Aule quest interaction every turn */
    check_aule_quest_interaction();

    /* Check for Niena quest interaction every turn */
    check_niena_quest_interaction();

    /* Check for Orome quest interaction every turn */
    check_orome_quest_interaction();

    /* Check for Varda quest interaction every turn */
    check_varda_quest_interaction();

    /* Stop now unless the turn count is divisible by 10 */
    if (turn % 10)
        return;

    /*** Check the Time and Load ***/
    if (!(turn % 1000))
    {
        /* Check time and load */
        if (0 != check_time())
        {
            /* Warning */
            if (closing_flag <= 2)
            {
                /* Disturb */
                disturb(0, 0);

                /* Count warnings */
                closing_flag++;

                /* Message */
                msg_print("The gates to ANGBAND are closing...");
                msg_print("Please finish up and/or save your game.");
            }

            /* Slam the gate */
            else
            {
                /* Message */
                msg_print("The gates to ANGBAND are now closed.");

                /* Stop playing */
                p_ptr->playing = false;

                /* Leaving */
                p_ptr->leaving = true;
            }
        }
    }

    /*** Handle the "surface" ***/

    /* While on the surface */
    if (p_ptr->depth == 0)
    {
        if (percent_chance(10))
        {
            /* Make a new monster */
            (void)alloc_monster(true, false);
        }
    }

    /*** Process the monsters ***/

    /* Hack - see if there is already a player ghost on the level */
    if (bones_selector)
        was_ghost = true;

    /* Vastly more wandering monsters during the endgame when you have 2 or 3
     * Silmarils */
    if (silmarils_possessed() >= 2)
    {
        int percent = (p_ptr->cur_map_hgt * p_ptr->cur_map_wid)
            / (PANEL_HGT * PANEL_WID_FIXED);

        if (percent_chance(percent))
        {
            /* Make a new monster */
            (void)alloc_monster(true, false);
        }
    }

    /* Check for normal wandering monster generation */
    else if (one_in_(MAX_M_ALLOC_CHANCE))
    {
        /* Make a new monster */
        (void)alloc_monster(true, false);
    }

    // Players with the haunted curse attract wraiths
    if (percent_chance(p_ptr->haunted))
    {
        /* Make a new wraith */
        (void)alloc_monster(true, true);
    }

    /* Hack - if there is a ghost now, and there was not before,
     * give a challenge */
    if ((bones_selector) && (!(was_ghost)))
        ghost_challenge();

    /* Regenerate creatures */
    regen_monsters();

    /*** Process Light ***/

    /* Check for light being wielded */
    o_ptr = &inventory[INVEN_LITE];

    /* Burn some fuel in the current lite */
    if (o_ptr->tval == TV_LIGHT)
    {
        /* Hack -- Use some fuel */
        if (player_light_has_fuel(o_ptr)
            && !((o_ptr->sval == SV_LIGHT_LANTERN)
                && (object_ego_prefix(o_ptr) == EGO_BROKEN_BRASS_LANTERN)))
        {
            /* Decrease life-span */
            int fuel = 1;
            if (fuelable_light_p(o_ptr)
                && (level_partition_kind_for_point(p_ptr->py, p_ptr->px) == LEVEL_PART_CAVEY))
            {
                /*
                 * Small caves: double fuel drain only while standing in the actual
                 * CA-blob cave area (not merely anywhere in the partition).
                 *
                 * CA blobs are generated as (dark) "room" grids; corridors/links are not.
                 */
                if ((cave_info[p_ptr->py][p_ptr->px] & (CAVE_ROOM)) &&
                    !(cave_info[p_ptr->py][p_ptr->px] & (CAVE_GLOW)))
                {
                    fuel = 2;
                }
            }

            player_light_add_fuel(o_ptr, -fuel);
            p_ptr->redraw |= (PR_LIGHT);

            /* Hack -- notice interesting fuel steps */
            if ((player_light_fuel(o_ptr) <= player_light_sputter_threshold(o_ptr))
                || (!(player_light_fuel(o_ptr) % 100)))
            {
                /* Window stuff */
                p_ptr->window |= (PW_EQUIP);
            }

            /* Hack -- Special treatment when blind */
            if (p_ptr->blind)
            {
                /* Hack -- save some light for later */
                if (player_light_fuel(o_ptr) == 0)
                    player_light_set_fuel(o_ptr, 1);
            }

            /* The light is now out */
            else if (player_light_fuel(o_ptr) == 0)
            {
                disturb(0, 0);
                msg_print("Your light has gone out!");
            }

            /* The light is getting dim */
            else if ((player_light_fuel(o_ptr)
                    <= player_light_sputter_threshold(o_ptr))
                && (!(player_light_fuel(o_ptr)
                    % MIN(MAX(player_light_sputter_threshold(o_ptr), 1), 20))))
            {
                // disturb the first time
                if (player_light_fuel(o_ptr) == player_light_sputter_threshold(o_ptr))
                    disturb(0, 0);

                msg_print("Your light is growing faint.");
            }
        }
    }

    /*** Process Inventory ***/

    /* Process equipment */
    for (j = 0, i = INVEN_WIELD; i < INVEN_TOTAL; i++)
    {
        /* Get the object */
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Recharge activatable objects */
        if (o_ptr->timeout > 0 && !fuelable_light_p(o_ptr))
        {
            /* Recharge */
            o_ptr->timeout--;

            /* Notice changes */
            if (!(o_ptr->timeout))
            {
                /* Update window */
                j++;

                /* Message if item is recharged, if inscribed !! */
                if (!(o_ptr->timeout))
                    recharged_notice(o_ptr);
            }
        }
    }

    /* Notice changes */
    if (j)
    {
        /* Window stuff */
        p_ptr->window |= (PW_EQUIP);
    }

    /* Notice changes */
    if (j)
    {
        /* Combine pack */
        p_ptr->notice |= (PN_COMBINE);

        /* Window stuff */
        p_ptr->window |= (PW_INVEN);
    }

    /*** Process Objects ***/

    /* Process objects */
    for (i = 1; i < o_max; i++)
    {
        /* Get the object */
        o_ptr = &o_list[i];

        /* Skip dead objects */
        if (!o_ptr->k_idx)
            continue;
    }
}

/*
 * Verify use of "wizard" mode
 */
static bool enter_wizard_mode(void)
{
    /* Ask first time - unless resurrecting a dead character */
    if (!(p_ptr->noscore & 0x0008) && !p_ptr->is_dead)
    {
        /* Explanation */
        msg_print("You can only enter wizard mode from within debug mode.");
        log_debug("Wizard mode denied - must be in debug mode first");

        return (false);
    }

    /* Mark savefile */
    p_ptr->noscore |= 0x0002;

    log_info("Entering wizard mode - savefile marked (noscore=0x%04X, savefile='%s')",
             (unsigned)p_ptr->noscore, savefile);

    /* Success */
    return (true);
}

#ifdef ALLOW_DEBUG

/*
 * Verify use of "debug" mode
 */
static bool verify_debug_mode(void)
{
    char buf[80] = "It is not mellon";

    /* Ask first time */
    if (!(p_ptr->noscore & 0x0008))
    {
        /* Mention effects */
        msg_print(
            "You are about to use the dangerous, unsupported, debug commands!");
        msg_print(
            "Your machine may crash, and your savefile may become corrupted!");
        message_flush();

        /* Verify request */
        if (!get_check("Are you sure you want to use the debug commands? "))
        {
            return (false);
        }

        // ask for password in deployment versions
        if (DEPLOYMENT)
        {
            if (term_get_string("Password: ", buf, sizeof(buf)))
            {
                if (strcmp(buf, "Gondolin") == 0)
                {
                    /* Mark savefile */
                    p_ptr->noscore |= 0x0008;

                    /* Okay */
                    return (true);
                }
            }

            msg_print("Incorrect password.");
            return (false);
        }
    }

    /* Mark savefile */
    p_ptr->noscore |= 0x0008;

    log_info("Debug mode enabled (noscore=0x%04X, savefile='%s')",
             (unsigned)p_ptr->noscore, savefile);

    /* Okay */
    return (true);
}

#endif /* ALLOW_DEBUG */

/*
 * Parse and execute the current command
 * Give "Warning" on illegal commands.
 */
static void process_command(void)
{
    log_trace("process_command: character_icky=%d, command='%c' (%d)",
              character_icky, p_ptr->command_cmd, (int)p_ptr->command_cmd);

    /* Debug: Log character_icky state but don't aggressively reset it during normal operation */
    if (character_icky > 0) {
        log_debug("process_command: character_icky is %d (may be normal during menu operations)", character_icky);
    }

#ifdef ALLOW_REPEAT

    /* Handle repeating the last command */
    repeat_check();

#endif /* ALLOW_REPEAT */

    /* Disallow actions that would advance time while viewing the final map. */
    if (death_spectator_mode
        && !death_spectator_command_allowed(p_ptr->command_cmd))
    {
        if (p_ptr->command_cmd)
        {
            msg_print("You can no longer take that action.");
        }
        p_ptr->command_cmd = 0;
        return;
    }

    /* Parse the command */
    switch (p_ptr->command_cmd)
    {
    /* Ignore */
    case ' ':
    case '\n':
    case '\r':
    case '\a':
    {
        break;
    }

    /*** Cheating Commands ***/

    /* Toggle Wizard Mode */
    case KTRL('W'):
    {
        if (p_ptr->wizard)
        {
            p_ptr->wizard = false;
            msg_print("Wizard mode off.");
            p_ptr->update |= (PU_BONUS);
        }
        else if (enter_wizard_mode())
        {
            p_ptr->wizard = true;
            msg_print("Wizard mode on.");
            p_ptr->update |= (PU_BONUS);
        }

        /* Update monsters */
        p_ptr->update |= (PU_MONSTERS);

        break;
    }

#ifdef ALLOW_DEBUG

    /* Special "debug" commands */
    case KTRL('Y'):
    {
        if (verify_debug_mode())
        {
            log_info("Ctrl-Y debug menu opened (wizard=%d, noscore=0x%04X, savefile='%s')",
                     p_ptr->wizard ? 1 : 0, (unsigned)p_ptr->noscore, savefile);
            do_cmd_debug();
        }
        break;
    }

#endif

    /*** Inventory Commands ***/

    /* Wear/wield equipment */
    case 'w':
    {
        do_cmd_wield_wrapper();
        break;
    }

    /* Remove equipment */
    case 'r':
    {
        do_cmd_takeoff(NULL, 0);
        break;
    }

    /* Drop an item */
    case 'd':
    {
        do_cmd_drop();
        break;
    }

    /* Destroy an item */
    case 'k':
    {
        do_cmd_destroy();
        break;
    }

    /* Equipment list */
    /* Equipment list */
    case 'e':
    {
        do_cmd_equip_direct();
        break;
    }

    /* Inventory list */
    case 'i':
    {
        do_cmd_inven_direct();
        break;
    }

    /* Sing */
    case 's':
    {
        do_cmd_change_song();
        break;
    }

    /* Ability screen */
    case '\t':
    {
        do_cmd_ability_screen();
        
        /* Force full redraw after screen_load() restored old content */
        p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP | PR_EXP);
        handle_stuff();
        break;
    }

    /* Smithing screen */
    case '0':
    case KTRL('D'): // Hack: required to get Angband-like keyset to work
    {
        do_cmd_smithing_screen();
        break;
    }

    /*** Various commands ***/

    /* Examine an object */
    case 'x':
    {
        do_cmd_observe();
        break;
    }

    /* Hack -- toggle windows */
    case KTRL('E'):
    {
        toggle_inven_equip();
        break;
    }

    /*** Standard "Movement" Commands ***/

    /* Alternate action */
    case '/':
    {
        do_cmd_alter();
        break;
    }

    /* Dig a tunnel */
    case 'T':
    {
        do_cmd_tunnel();
        break;
    }

    /* Walk */
    case ';':
    {
        do_cmd_walk();
        break;
    }

    /*** Running, Resting, Searching, Staying */

    /* Begin Running -- Arg is Max Distance */
    case '.':
    {
        do_cmd_run();
        break;
    }

    /* Hold still */
    case 'z':
    {
        do_cmd_hold();
        break;
    }

    /* Rest */
    case '%':
    case 'Z':
    {
        do_cmd_rest();
        break;
    }

    /* Get */
    case 'g':
    {
        do_cmd_pickup();
        break;
    }

    /* Toggle stealth mode */
    case 'S':
    {
        do_cmd_toggle_stealth();
        break;
    }

    /*** Stairs and Doors and Chests and Traps ***/

    /* Go up staircase */
    case '<':
    {
        // Autosave
        save_game_quietly = true;
        do_cmd_save_game();

        do_cmd_go_up();
        break;
    }

    /* Go down staircase */
    case '>':
    {
        // Autosave
        save_game_quietly = true;
        do_cmd_save_game();

        do_cmd_go_down();
        break;
    }

    /* Open a door or chest */
    case 'o':
    {
        do_cmd_open();
        break;
    }

    /* Close a door */
    case 'c':
    {
        do_cmd_close();
        break;
    }

    /* Bash a door */
    case 'b':
    {
        do_cmd_bash();
        break;
    }

    /* Disarm a trap or chest */
    case 'D':
    {
        do_cmd_disarm();
        break;
    }

    /* Exchange places */
    case 'X':
    {
        do_cmd_exchange();
        break;
    }

    case '-':
    {
        do_cmd_fletchery();
        break;
    }

    /*** Use various objects ***/

    /* Inscribe an object */
    case '{':
    {
        do_cmd_inscribe();
        break;
    }

    /* Activate a staff */
    case 'a':
    {
        do_cmd_activate_staff(NULL, 0);
        break;
    }

    /* Swap the equipped staff with one from the pack */
    case KTRL('A'):
    {
        do_cmd_swap_staff();
        break;
    }

    /* Eat some food */
    case 'E':
    {
        do_cmd_eat_food(NULL, 0);
        break;
    }

    /* Swap the 1st and 2nd quivers */
    case KTRL('F'):
    {
        do_cmd_swap_quivers();
        break;
    }

    /* Fire an arrow from the 1st quiver */
    case 'f':
    {
        do_cmd_fire(1);
        break;
    }

    /* Fire an arrow from the 2nd quiver */
    case 'F':
    {
        do_cmd_fire(2);
        break;
    }

    /* Throw an item */
    case 't':
    {
        do_cmd_throw(false);
        break;
    }

        /* Throw an automatically chosen item at nearest target */
    case KTRL('T'):
    {
        do_cmd_throw(true);
        break;
    }

    /* Play an instrument */
    case 'p':
    {
        do_cmd_play_instrument(NULL, 0);
        break;
    }

    /* Quaff a potion */
    case 'q':
    {
        open_supplies_menu_with_context(SUPPLY_MENU_ACTION_USE, SUPPLY_GROUP_POTIONS, true, true);
        break;
    }

    /* Use an item */
    case 'u':
    {
        do_cmd_use_item();
        break;
    }

    /*** Looking at Things (nearby or on map) ***/

    /* Full dungeon map */
    case 'M':
    {
        do_cmd_view_map();
        break;
    }

    /* Locate player on map */
    case 'L':
    {
        do_cmd_locate();
        break;
    }

    /* Look around */
    case 'l':
    {
        do_cmd_look();
        break;
    }

    /* Target monster or location */
    // case '*':
    //{
    //	do_cmd_target();
    //	break;
    //}

    /*** Help and Such ***/

    /* Help */
    case '?':
    {
        do_cmd_help();
        break;
    }

    /* Character sheet (alternative key) */
    case 'h':
    {
        do_cmd_character_sheet();
        break;
    }
    
    /* Direct access to skill distribution */
    case 'H':
    {
        /* Save screen */
        screen_save();
        
        /* Open skill distribution directly */
        gain_skills();
        
        /* Load screen */
        screen_load();
        
        /* Force full redraw after screen_load() restored old content */
        p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP | PR_EXP);
        handle_stuff();
        break;
    }

    /* Main menu */
    case 'm':
    {
        do_cmd_main_menu();
        break;
    }
    case ESCAPE:
    {
        if (easy_main_menu)
            do_cmd_main_menu();
        break;
    }

    /* Identify symbol */
    // case '/':
    //{
    //	do_cmd_query_symbol();
    //	break;
    //}

    /* Character sheet */
    case '@':
    {
        do_cmd_character_sheet();
        break;
    }

    /*** System Commands ***/

    /* Single line from a pref file */
    // case '"':
    //{
    //	do_cmd_pref();
    //	break;
    //}

    /* Interact with visuals */
    // case '%':
    //{
    //	do_cmd_visuals();
    //	break;
    //}

    /* Interact with options */
    case 'O':
    {
        do_cmd_options();
        do_cmd_redraw();
        break;
    }

    /*** Misc Commands ***/

    /* Take notes */
    case ':':
    {
        do_cmd_note("", p_ptr->depth);
        break;
    }

    /* Show previous message */
    case KTRL('O'):
    {
        do_cmd_message_one();
        break;
    }

    /* Show previous messages */
    case KTRL('P'):
    {
        do_cmd_messages();
        break;
    }

    /* Redraw the screen */
    case KTRL('R'):
    {
        do_cmd_redraw();
        break;
    }

#ifndef VERIFY_SAVEFILE

    /* Hack -- Save and don't quit */
    case KTRL('S'):
    {
        do_cmd_save_game();
        break;
    }

#endif

    /* Save and quit */
    case KTRL('X'):
    case KTRL('C'):
    {
        /* Stop playing */
        p_ptr->playing = false;

        /* Leaving */
        p_ptr->leaving = true;
        break;
    }

    /* Supplies overview */
    case 'j':
    {
        do_cmd_knowledge_supplies(NULL);
        break;
    }

    /* Check knowledge */
    case '~':
    {
        do_cmd_knowledge();
        break;
    }

    case '[':
    {
        do_cmd_view_monsters();
        break;
    }

    case ']':
    {
        do_cmd_view_objects();
        break;
    }

    /* Hack -- Unknown command */
    default:
    {
        prt("Type '?' for help.", 0, 0);
        break;
    }
    }
}

/*
 * Determine if the object can be picked up, and either has "=g" in its
 * inscription or has the pickup flag set to true (e.g. for thrown and fired
 * items)
 */
static bool death_spectator_command_allowed(int command)
{
    if (command == 0)
        return true;

    switch (command)
    {
    case ' ':
    case '\n':
    case '\r':
    case '\a':
    case '?':
    case '@':
    case 'h':
    case 'H':
    case 'i':
    case 'e':
    case 'x':
    case 'M':
    case 'L':
    case 'l':
    case 'm':
    case 'O':
    case ':':
    case 'j':
    case '~':
    case '[':
    case ']':
    case KTRL('E'):
    case KTRL('O'):
    case KTRL('P'):
    case KTRL('R'):
    case ESCAPE:
        return true;
    default:
        return false;
    }
}

static bool death_spectator_continue_input(int command)
{
    if ((command == ' ') || (command == '\n') || (command == '\r'))
    {
        return true;
    }

    if (steamdeck_controls_active() && (command == steamdeck_confirm_key()))
        return true;

    return false;
}

static void death_spectator_prepare_display(void)
{
    int i;

    /* Reveal player knowledge of objects on the final level. */
    for (i = 1; i < o_max; i++)
    {
        object_type* o_ptr = &o_list[i];

        if (!o_ptr->k_idx)
            continue;

        object_aware(o_ptr);
        object_known(o_ptr);
    }

    /* Fully light the level and reveal monsters. */
    Term_clear();
    wiz_light();
    do_cmd_wiz_unhide(255);

    /* Force a comprehensive redraw across all panes. */
    p_ptr->redraw |= 0x0FFFFFFFL;
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0 | PW_MONSTER
        | PW_MONLIST | PW_COMBAT_ROLLS | PW_OVERHEAD);

    handle_stuff();

    if (op_ptr->main_combat_rolls > 0)
    {
        display_main_combat_rolls();
    }

    msg_print(
        "You linger for a final look. Press Esc, Space, or Enter to continue to the tomb.");
}

void death_spectator_view(void)
{
    death_spectator_mode = true;

    /* Clear any queued commands from the main loop. */
    p_ptr->command_cmd = 0;
    p_ptr->command_new = 0;
    p_ptr->command_rep = 0;
    p_ptr->command_arg = 0;
    p_ptr->command_dir = 0;

    /* Prevent lingering keypresses from auto-triggering commands. */
    flush();

    death_spectator_prepare_display();

    while (true)
    {
        request_command();

        if ((p_ptr->command_cmd == ESCAPE)
            || death_spectator_continue_input(p_ptr->command_cmd))
        {
            break;
        }

        if (!death_spectator_command_allowed(p_ptr->command_cmd))
        {
            if (p_ptr->command_cmd)
            {
                msg_print("You can no longer take that action.");
            }
            p_ptr->command_cmd = 0;
            continue;
        }

        process_command();
        handle_stuff();

        /* Reset command state for the next iteration. */
        p_ptr->command_cmd = 0;
        p_ptr->command_new = 0;
        p_ptr->command_rep = 0;
        p_ptr->command_arg = 0;
        p_ptr->command_dir = 0;
    }

    death_spectator_mode = false;

    /* Ensure no residual actions are pending. */
    p_ptr->energy_use = 0;
    p_ptr->command_cmd = 0;
    p_ptr->command_new = 0;
    p_ptr->command_rep = 0;
    p_ptr->command_arg = 0;
    p_ptr->command_dir = 0;
}

bool death_spectator_active(void)
{
    return death_spectator_mode;
}

static bool auto_pickup_okay(const object_type* o_ptr)
{
    int max_qty;
    // cptr s;

    /* It can't be carried */
    if (!inven_carry_okay(o_ptr))
        return (false);

    /*
     * Don't interrupt movement with a quantity prompt when a supply stack
     * only fits partially. The player can still pick it up manually.
     */
    if (supplies_is_supply_object(o_ptr) && o_ptr->number > 1)
    {
        max_qty = supplies_max_absorbable_quantity(o_ptr);
        if ((max_qty > 0) && (max_qty < o_ptr->number))
            return (false);
    }

    /*object is marked to not pickup*/
    if ((k_info[o_ptr->k_idx].squelch == NO_SQUELCH_NEVER_PICKUP)
        && object_aware_p(o_ptr))
        return (false);

    /*object is marked to not pickup*/
    if ((k_info[o_ptr->k_idx].squelch == NO_SQUELCH_ALWAYS_PICKUP)
        && object_aware_p(o_ptr))
        return (true);

    /* object has pickup flag set */
    if (o_ptr->pickup)
        return (true);

    /* No inscription */
    if (!o_ptr->obj_note)
        return (false);

    /* Find a '=' */
    // s = strchr(quark_str(o_ptr->obj_note), '=');

    /* Process inscription */ // Sil-y: turned the =g inscriptions off for now
    // while (s)
    //{
    //	/* Auto-pickup on "=g" */
    //	if (s[1] == 'g') return (true);

    //	/* Find another '=' */
    //	s = strchr(s + 1, '=');
    //}

    /* Don't auto pickup */
    return (false);
}

/*
 * Finish your leap
 */
void land(void)
{
    // the player has landed
    p_ptr->leaping = false;

    // make some noise when landing
    stealth_score -= 5;

    /* Set off traps */
    if (cave_trap_bold(p_ptr->py, p_ptr->px)
        || (cave_feat[p_ptr->py][p_ptr->px] == FEAT_CHASM))
    {
        // If it is hidden
        if (cave_info[p_ptr->py][p_ptr->px] & (CAVE_HIDDEN))
        {
            /* Reveal the trap */
            reveal_trap(p_ptr->py, p_ptr->px);
        }

        /* Hit the trap */
        hit_trap(p_ptr->py, p_ptr->px);
    }
}

/*
 * Continue your leap
 */
void continue_leap(void)
{
    int dir;
    int y_end, x_end; // the desired endpoint of the leap

    dir = p_ptr->previous_action[1];

    /* Get location */
    y_end = p_ptr->py + ddy[dir];
    x_end = p_ptr->px + ddx[dir];

    // display a message until player input is received
    msg_print("You fly through the air.");
    message_flush();

    /* Take a turn */
    p_ptr->energy_use = 100;

    // store the action type
    p_ptr->previous_action[0] = dir;

    // solid objects end the leap
    if (cave_info[y_end][x_end] & (CAVE_WALL))
    {
        if (cave_feat[y_end][x_end] == FEAT_RUBBLE)
        {
            msg_print("You slam into a wall of rubble.");
        }
        if (cave_wall_bold(y_end, x_end))
        {
            msg_print("You slam into a wall.");
        }
        else if (cave_any_closed_door_bold(y_end, x_end))
        {
            msg_print("You slam into a door.");
        }
    }

    // monsters end the leap
    else if (cave_m_idx[y_end][x_end] > 0)
    {
        monster_type* m_ptr = &mon_list[cave_m_idx[y_end][x_end]];
        char m_name[80];

        /* Get the monster name */
        monster_desc(m_name, sizeof(m_name), m_ptr, 0);

        if (m_ptr->ml)
            msg_format("%^s blocks your landing.", m_name);
        else
            msg_format("Some unseen foe blocks your landing.", m_name);
    }

    // successful leap
    else
    {
        // we generously give you your free flanking attack...
        flanking_or_retreat(y_end, x_end);

        // move player to the new position
        monster_swap(p_ptr->py, p_ptr->px, y_end, x_end);
    }

    // land on the ground
    land();
}

/*
 * Hack -- helper function for "process_player()"
 *
 * Check for changes in the "monster memory"
 */
static void process_player_aux(void)
{
    int i;
    bool changed = false;

    static int old_monster_race_idx = 0;

    static u32b old_flags1 = 0L;
    static u32b old_flags2 = 0L;
    static u32b old_flags3 = 0L;
    static u32b old_flags4 = 0L;

    static byte old_blows[MONSTER_BLOW_MAX];

    static byte old_ranged = 0;

    /* Tracking a monster */
    if (p_ptr->monster_race_idx)
    {
        /* Get the monster lore */
        monster_lore* l_ptr = &l_list[p_ptr->monster_race_idx];

        for (i = 0; i < MONSTER_BLOW_MAX; i++)
        {
            if (old_blows[i] != l_ptr->blows[i])
            {
                changed = true;
                break;
            }
        }

        /* Check for change of any kind */
        if (changed || (old_monster_race_idx != p_ptr->monster_race_idx)
            || (old_flags1 != l_ptr->flags1) || (old_flags2 != l_ptr->flags2)
            || (old_flags3 != l_ptr->flags3) || (old_flags4 != l_ptr->flags4)
            || (old_ranged != l_ptr->ranged))

        {
            /* Memorize old race */
            old_monster_race_idx = p_ptr->monster_race_idx;

            /* Memorize flags */
            old_flags1 = l_ptr->flags1;
            old_flags2 = l_ptr->flags2;
            old_flags3 = l_ptr->flags3;
            old_flags4 = l_ptr->flags4;

            /* Memorize blows */
            for (i = 0; i < MONSTER_BLOW_MAX; i++)
                old_blows[i] = l_ptr->blows[i];

            /* Memorize castings */
            old_ranged = l_ptr->ranged;

            /* Window stuff */
            p_ptr->window |= (PW_MONSTER);

            /* Window stuff */
            window_stuff();
        }
    }
}

/*
 * Process the player
 *
 * Notice the annoying code to handle "pack overflow", which
 * must come first just in case somebody manages to corrupt
 * the savefiles by clever use of menu commands or something.
 *
 * Notice the annoying code to handle "monster memory" changes,
 * which allows us to avoid having to update the window flags
 * every time we change any internal monster memory field, and
 * also reduces the number of times that the recall window must
 * be redrawn.
 *
 * Note that the code to check for user abort during repeated commands
 * and running and resting can be disabled entirely with an option, and
 * even if not disabled, it will only check during every 128th game turn
 * while resting, for efficiency.
 */
static void process_player(void)
{
    int i;
    int amount;
    int regen_multiplier;
    int depth_counter_increment;

    // reset the number of times you have riposted since last turn
    p_ptr->ripostes = 0;

    // reset whether you have just woken up from entrancement
    p_ptr->was_entranced = false;

    // update the player's torch radius
    calc_torch();

    song_disguise_new_player_turn();
    song_duels_new_player_turn();

    /*** Check certain things between player turns (don't need to do this when
     * restoring a game) ***/

    if (!p_ptr->restoring)
    {
        /*** Check for interrupts ***/

        /* Complete resting */
        if (p_ptr->resting < 0)
        {
            /* Basic resting */
            if (p_ptr->resting == -1)
            {
                /* Stop resting */
                if ((p_ptr->chp == p_ptr->mhp) && (p_ptr->csp == p_ptr->msp))
                {
                    disturb(0, 0);
                }
            }

            /* Complete resting */
            else if (p_ptr->resting == -2)
            {
                /* Stop resting */
                if ((p_ptr->chp == p_ptr->mhp)
                    && ((p_ptr->csp == p_ptr->msp) || !singing(SNG_NOTHING))
                    && !p_ptr->blind && !p_ptr->confused && !p_ptr->poisoned
                    && !p_ptr->afraid && !p_ptr->stun && !p_ptr->cut
                    && !p_ptr->slow && !p_ptr->entranced)
                {
                    disturb(0, 0);
                }
            }
        }

        /* Check for "player abort" */
        if (p_ptr->running || p_ptr->fletching || p_ptr->smithing
            || p_ptr->command_rep || (p_ptr->resting && !(turn & 0x7F)))
        {
            /* Do not wait */
            inkey_scan = true;

            /* Check for a key */
            if (inkey())
            {
                /* Flush input */
                flush();

                /* Disturb */
                disturb(0, 0);

                /* Hack -- Show a Message */
                msg_print("Cancelled.");
            }
        }

        /*** Other checks ***/

        do_betrayal_ring_amulet();

        // Make the stealth-modified noise (has to occur after monsters have had
        // a chance to move)
        monster_perception(true, true, stealth_score);

        // Stop stealth mode if something happened
        if (stop_stealth_mode)
        {
            /* Cancel */
            p_ptr->stealth_mode = false;

            /* Recalculate bonuses */
            p_ptr->update |= (PU_BONUS);

            /* Redraw the state */
            p_ptr->redraw |= (PR_STATE);

            // Reset the flag
            stop_stealth_mode = false;
        }

        // Morgoth will announce a challenge if adjacent
        if (p_ptr->truce && (p_ptr->depth == MORGOTH_DEPTH))
        {
            int d, yy, xx;

            /* Check around the character */
            for (d = 0; d < 8; d++)
            {
                monster_type* m_ptr;

                /* Extract adjacent (legal) location */
                yy = p_ptr->py + ddy_ddd[d];
                xx = p_ptr->px + ddx_ddd[d];

                // paranoia
                if (cave_m_idx[yy][xx] < 0)
                    continue;

                m_ptr = &mon_list[cave_m_idx[yy][xx]];

                if ((m_ptr->r_idx == R_IDX_MORGOTH)
                    && (m_ptr->alertness >= ALERTNESS_ALERT))
                {
                    msg_print("With a voice as of rolling thunder, Morgoth, "
                              "Lord of Darkness, "
                              "speaks:");
                    msg_print("'You dare challenge me in mine own hall? Now is "
                              "your death upon "
                              "you!'");

                    // Break the truce (always)
                    break_truce(true);
                }
            }
        }

        /* List all challenge options at the start of the game */
        if (playerturn == 1)
        {
            for (i = 0; i < OPT_PAGE_PER; i++)
            {
                int option_number = option_page[CHALLENGE_PAGE][i];

                /* Collect options on this "page" */
                if ((option_number != OPT_NONE) && (op_ptr->opt[option_number]))
                {
                    do_cmd_note(
                        format("Challenge: %s", option_desc[option_number]),
                        p_ptr->depth);
                }
            }
        }

        if (p_ptr->previous_action[0] != ACTION_ARCHERY)
        {
            p_ptr->killed_enemy_with_arrow = false;
            p_ptr->redraw |= PR_ARC;
        }

        // shuffle along the array of previous actions
        for (i = ACTION_MAX - 1; i > 0; i--)
        {
            p_ptr->previous_action[i] = p_ptr->previous_action[i - 1];
        }
        // put in a default for this turn
        // Sil-y: it is possible that this isn't always changed to something
        // else, but I think it is
        p_ptr->previous_action[0] = ACTION_NOTHING;

        /* Redraw stuff (if needed) */
        if (p_ptr->window)
            window_stuff();

        // Sil-y: have to update the player bonuses at every turn with
        // sprinting, dodging etc.
        //        this might cause annoying slowdowns, I'm not sure
        p_ptr->update |= (PU_BONUS);
    }

    /*** Handle actual user input ***/

    /* Repeat until energy is reduced */
    do
    {
        /* Notice stuff (if needed) */
        if (p_ptr->notice)
            notice_stuff();

        /* Update stuff (if needed) */
        if (p_ptr->update)
            update_stuff();

        /* Redraw stuff (if needed) */
        if (p_ptr->redraw)
            redraw_stuff();

        /* Redraw stuff (if needed) */
        if (p_ptr->window)
            window_stuff();

        /* Place the cursor on the player or target */
        if (hilite_player)
            move_cursor_relative(p_ptr->py, p_ptr->px);
        if (hilite_target && target_sighted())
            move_cursor_relative(p_ptr->target_row, p_ptr->target_col);

        if (cheat_noise)
            display_noise_map();
        else if (cheat_scent)
            display_scent_map();
        else if (cheat_light)
            display_light_map();

        /* Refresh */
        Term_fresh();

        /* Hack -- Pack Overflow if needed */
        check_pack_overflow();

        if (cave_o_idx[p_ptr->py][p_ptr->px] != 0)
        {
            (&o_list[cave_o_idx[p_ptr->py][p_ptr->px]])->marked = true;
        }

        /* Hack -- cancel "lurking browse mode" */
        if (!p_ptr->command_new)
            p_ptr->command_see = false;

        /* Assume free turn */
        p_ptr->energy_use = 0;

    // Reset number of attacks this turn happens at start of player energy loop

        // get base stealth score for the round
        // this will get modified by the type of action
        stealth_score = p_ptr->skill_use[S_STL];

        // display a note at the start of the game
        if ((cave_o_idx[p_ptr->py][p_ptr->px] != 0))
        {
            object_type* o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];
            if ((o_ptr->tval == TV_NOTE) && (playerturn == 1))
            {
                note_info_screen(o_ptr);
            }
        }

        /* Leaping */
        if (p_ptr->leaping)
        {
            continue_leap();
        }

        /* Entranced or Knocked Out */
        else if ((p_ptr->entranced) || (p_ptr->stun > 100))
        {
            // stop singing
            change_song(SNG_NOTHING);

            /* Take a turn */
            p_ptr->energy_use = 100;

            // store the action type
            p_ptr->previous_action[0] = ACTION_MISC;
        }

        /* Smithing */
        else if (p_ptr->smithing)
        {
            if (p_ptr->smithing == 1)
            {
                // Display a message
                msg_print("You complete your work.");

                create_smithing_item();

                /* Aule quest: check for success condition during forging */
                {
                    int diff = object_difficulty(smith_o_ptr);
                    p_ptr->aule_last_object_diff = diff;
                    if (diff > 20 && p_ptr->aule_quest == AULE_QUEST_ACTIVE) {
                        p_ptr->aule_quest = AULE_QUEST_SUCCESS;
                        log_trace("Aule quest: state -> SUCCESS (diff=%d)", diff);
                        msg_print("Your forging radiates unparalleled craft!");
                        msg_print("You sense that Aule would be pleased with this work...");
                        msg_print("Seek out Aule to receive his blessing.");
                    }
                }
            }

            /* Reduce smithing count */
            p_ptr->smithing--;

            /* Reduce smithing leftover counter */
            p_ptr->smithing_leftover--;

            /* Take a turn */
            p_ptr->energy_use = 100;

            // store the action type
            p_ptr->previous_action[0] = ACTION_MISC;

            /* Redraw the state */
            p_ptr->redraw |= (PR_STATE);
        }
    /* Aule quest: no longer requires standing at special forge; acceptance handled during forging */

        /* Fletching */
        else if (p_ptr->fletching)
        {
            if (p_ptr->fletching == 1)
            {
                // Display a message
                msg_print("You complete your work.");

                finish_fletching(0);
            }

            /* Reduce fletching count */
            p_ptr->fletching--;

            /* Take a turn */
            p_ptr->energy_use = 100;

            // store the action type
            p_ptr->previous_action[0] = ACTION_MISC;

            /* Redraw the state */
            p_ptr->redraw |= (PR_STATE);
        }

        /* Resting */
        else if (p_ptr->resting)
        {
            /* Timed rest */
            if (p_ptr->resting > 0)
            {
                /* Reduce rest count */
                p_ptr->resting--;

                /* Redraw the state */
                p_ptr->redraw |= (PR_STATE);
            }

            /* Take a turn */
            p_ptr->energy_use = 100;

            // store the action type
            p_ptr->previous_action[0] = 5;

            // store the 'focus' attribute
            p_ptr->focused = true;

            /* Searching */
            search();
        }

        /* Recovering footing */
        else if (p_ptr->skip_next_turn)
        {
            // let the player know
            if (p_ptr->knocked_back)
            {
                msg_print("You recover your footing.");

                // force a -more-
                message_flush();
                p_ptr->knocked_back = false;
            }

            // reset flag
            p_ptr->skip_next_turn = false;

            /* Take a turn */
            p_ptr->energy_use = 100;

            // store the action type
            p_ptr->previous_action[0] = ACTION_MISC;

            // Pause to show enemies moving.
            Term_xtra(TERM_XTRA_DELAY, 500);
        }

        /* Running */
        else if (p_ptr->running)
        {
            /* Take a step */
            run_step(0);

            // Pause for 17 miliseconds (minimum needed for mac OS X to pause)
            if (!instant_run)
            {
                Term_xtra(TERM_XTRA_DELAY, 17);
            }
        }

        /* Repeated command */
        else if (p_ptr->command_rep)
        {
            /* Hack -- Assume messages were seen */
            msg_flag = false;

            /* Clear the top line */
            prt("", 0, 0);

            /* Process the command */
            process_command();

            /* Count this execution */
            if (p_ptr->command_rep)
            {
                /* Count this execution */
                p_ptr->command_rep--;

                /* Redraw the state */
                p_ptr->redraw |= (PR_STATE);
            }
        }

        /* Normal command */
        else
        {
            char out_val[160];
            char o_name[80];
            object_type* o_ptr;

            // build an object description
            if (cave_o_idx[p_ptr->py][p_ptr->px])
            {
                o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];

                /* Describe the object */
                object_desc(o_name, sizeof(o_name), o_ptr, true, 3);
                strnfmt(out_val, sizeof(out_val), "Pick up %s? ", o_name);
            }

            // always offer to pickup if the mode is on, there is an object
            // present, and you have just moved
            if (always_pickup && cave_o_idx[p_ptr->py][p_ptr->px]
                && (o_ptr->tval != TV_NOTE) && (p_ptr->previous_action[1] >= 1)
                && (p_ptr->previous_action[1] <= 9)
                && (p_ptr->previous_action[1] != 5))
            {
                // allow the player to decline to pick up the object
                if (get_check(out_val))
                {
                    /* Handle "objects" */
                    py_pickup();
                }
            }

            // if the player hasn't used their turn picking something up...
            if (p_ptr->energy_use < 100)
            {
                /* Check monster recall */
                process_player_aux();

                /* Place the cursor on the player or target */
                if (hilite_player)
                    move_cursor_relative(p_ptr->py, p_ptr->px);
                if (hilite_target && target_sighted())
                    move_cursor_relative(p_ptr->target_row, p_ptr->target_col);

                /* We are certainly no longer in the process of restoring a game
                 */
                p_ptr->restoring = false;

                /* Get a command (normal) */
                request_command();

                /* Process the command */
                process_command();
            }

            // check the item under the player
            o_ptr = &o_list[cave_o_idx[p_ptr->py][p_ptr->px]];

            /* Test for auto-pickup for thrown/fired items */
            if (auto_pickup_okay(o_ptr))
            {
                /* Pick up the object */
                py_pickup_aux(cave_o_idx[p_ptr->py][p_ptr->px]);
            }
        }

        /*** Clean up ***/

        /* Update labyrinth map restriction and partition-entry messages/XP. */
        update_labyrinth_view_state(true);
        handle_partition_entry(false, op_ptr->partition_narrative_mode);

        bool in_morgoth_vault = (p_ptr->depth == MORGOTH_DEPTH)
            && (cave_info[p_ptr->py][p_ptr->px] & (CAVE_G_VAULT));

        /* Check for greater vault squares */
        if ((cave_info[p_ptr->py][p_ptr->px] & (CAVE_G_VAULT))
            && (g_vault_name[0] != '\0') && !was_in_morgoth_vault)
        {
            bool clear_vault_name = true;

            if (strcmp(greater_vault_xp_name, g_vault_name) != 0)
            {
                SDL_strlcpy(greater_vault_xp_name, g_vault_name, sizeof(greater_vault_xp_name));
                greater_vault_xp_awarded = false;
            }

            if (in_morgoth_vault)
            {
                bool allow_entry = morgoth_entry_preconfirmed;
                if (!allow_entry)
                    allow_entry = confirm_enter_morgoth_hall();

                if (!allow_entry)
                {
                    clear_vault_name = false;

                    if (in_bounds_fully(last_player_y, last_player_x))
                    {
                        p_ptr->py = last_player_y;
                        p_ptr->px = last_player_x;
                        p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_PANEL);
                        p_ptr->redraw |= (PR_MAP);
                    }
                }
                else
                {
                    const int vault_xp = 500;
                    char note[120];
                    strnfmt(note, sizeof(note), "Entered %s", g_vault_name);
                    do_cmd_note(note, p_ptr->depth);

                    p_ptr->morgoth_hall_entered = true;

                    describe_greater_vault_entry(g_vault_name);
                    msg_print("From within you hear the harsh din of feasting in Morgoth's own hall.");
                    if (!greater_vault_xp_awarded)
                    {
                        gain_exp(vault_xp);
                        greater_vault_xp_awarded = true;
                    }

                    pause_with_text(throne_poetry, 5, 13, NULL, 0);
                    p_ptr->truce = true;
                    msg_print("There is a strange tension in the air.");
                    if (p_ptr->skill_use[S_PER] >= 15)
                        msg_print("You feel that Morgoth's servants are reluctant to attack before he delivers judgment.");
                }
            }
            else
            {
                const int vault_xp = 500;
                char note[120];
                strnfmt(note, sizeof(note), "Entered %s", g_vault_name);

                do_cmd_note(note, p_ptr->depth);
                describe_greater_vault_entry(g_vault_name);
                if (!greater_vault_xp_awarded)
                {
                    gain_exp(vault_xp);
                    greater_vault_xp_awarded = true;
                }
            }

            if (clear_vault_name)
            {
                g_vault_name[0] = '\0';
                greater_vault_xp_name[0] = '\0';
                greater_vault_xp_awarded = false;
            }
        }

        in_morgoth_vault = (p_ptr->depth == MORGOTH_DEPTH)
            && (cave_info[p_ptr->py][p_ptr->px] & (CAVE_G_VAULT));

        if (p_ptr->morgoth_hall_entered && was_in_morgoth_vault && !in_morgoth_vault
            && (silmarils_possessed() == 0))
        {
            msg_print("The Shadow bars your way: you cannot flee without a Silmaril.");

            if (in_bounds_fully(last_player_y, last_player_x))
            {
                p_ptr->py = last_player_y;
                p_ptr->px = last_player_x;
                p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_PANEL);
                p_ptr->redraw |= (PR_MAP);
                in_morgoth_vault = true;
            }
        }

        if (was_in_morgoth_vault && !in_morgoth_vault && p_ptr->truce)
        {
            break_truce(true);
        }

        was_in_morgoth_vault = in_morgoth_vault;
        last_player_y = p_ptr->py;
        last_player_x = p_ptr->px;
        morgoth_entry_preconfirmed = false;

        /* Significant */
        if (p_ptr->energy_use)
        {
            /* Use some energy */
            p_ptr->energy -= p_ptr->energy_use;

            /* Hack -- constant hallucination */
            if (p_ptr->image)
                p_ptr->redraw |= (PR_MAP);

            /* Shimmer monsters if needed */
            if (shimmer_monsters)
            {
                /* Clear the flag */
                shimmer_monsters = false;

                /* Shimmer multi-hued monsters */
                for (i = 1; i < mon_max; i++)
                {
                    monster_type* m_ptr;
                    monster_race* r_ptr;

                    /* Get the monster */
                    m_ptr = &mon_list[i];

                    /* Skip dead monsters */
                    if (!m_ptr->r_idx)
                        continue;

                    /* Get the monster race */
                    r_ptr = &r_info[m_ptr->r_idx];

                    /* Skip non-multi-hued monsters */
                    if (!(r_ptr->flags1 & (RF1_ATTR_MULTI)))
                        continue;

                    /* Reset the flag */
                    shimmer_monsters = true;

                    /* Redraw regardless */
                    lite_spot(m_ptr->fy, m_ptr->fx);
                }
            }

            /* Repair "mark" flags */
            if (repair_mflag_mark)
            {
                /* Reset the flag */
                repair_mflag_mark = false;

                /* Process the monsters */
                for (i = 1; i < mon_max; i++)
                {
                    monster_type* m_ptr;

                    /* Get the monster */
                    m_ptr = &mon_list[i];

                    /* Skip dead monsters */
                    /* if (!m_ptr->r_idx) continue; */

                    /* Repair "mark" flag */
                    if (m_ptr->mflag & (MFLAG_MARK))
                    {
                        /* Skip "show" monsters */
                        if (m_ptr->mflag & (MFLAG_SHOW))
                        {
                            /* Repair "mark" flag */
                            repair_mflag_mark = true;

                            /* Skip */
                            continue;
                        }

                        /* Forget flag */
                        m_ptr->mflag &= ~(MFLAG_MARK);

                        /* Update the monster */
                        update_mon(i, false);
                    }
                }
            }
        }

        /* Repair "show" flags */
        if (repair_mflag_show)
        {
            /* Reset the flag */
            repair_mflag_show = false;

            /* Process the monsters */
            for (i = 1; i < mon_max; i++)
            {
                monster_type* m_ptr;

                /* Get the monster */
                m_ptr = &mon_list[i];

                /* Skip dead monsters */
                /* if (!m_ptr->r_idx) continue; */

                /* Clear "show" flag */
                m_ptr->mflag &= ~(MFLAG_SHOW);
            }
        }
    } while (!p_ptr->energy_use && !p_ptr->leaving);

    // if the player is exiting the the game in some manner then stop processing
    // now
    if (p_ptr->leaving)
        return;

    /* Do song effects */
    sing();

    // make less noise if you did nothing at all
    // (+7 in total whether or not stealth mode is used)
    if (p_ptr->resting)
    {
        if (p_ptr->stealth_mode)
            stealth_score += 2;
        else
            stealth_score += 7;
    }

    // make much more noise when smithing
    if (p_ptr->smithing)
    {
        /* Make a lot of noise */
        monster_perception(true, false, -10);
    }

    // update player noise
    update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);

    /* Update scent trail */
    update_smell();

    /* possibly identify passive abilities every so often*/
    if (one_in_(100))
    {
        ident_passive();
    }

    /*** Damage over Time ***/

    /* Take damage from poison */
    if (p_ptr->poisoned)
    {
        /* Take damage */

        // amount is one fifth of the poison, rounding up
        amount = (p_ptr->poisoned + 4) / 5;

        killer_mark_other(SCORE_KILLER_OTHER);
        take_hit(amount, "poison");
    }

    /* Take damage from cuts */
    if (p_ptr->cut)
    {
        amount = (p_ptr->cut + 4) / 5;

        /* Take damage */
        killer_mark_other(SCORE_KILLER_OTHER);
        take_hit(amount, "a fatal wound");
    }

    /*** Check the Food, and Regenerate ***/

    /* Basic digestion rate */
    i = 1;

    // Note: speed and regeneration are taken into account already in the hunger
    // rate

    // Hack: slow hunger rates are done statistically
    if (p_ptr->hunger < 0)
    {
        if (!one_in_(int_exp(3, -(p_ptr->hunger))))
        {
            i = 0;
        }
    }
    else if (p_ptr->hunger > 0)
    {
        i *= int_exp(3, p_ptr->hunger);
    }

    /* Digest very quickly when gorged */
    if (p_ptr->food >= PY_FOOD_MAX)
        i *= 50;

    /* CUR_HUNGER increases p_ptr->hunger modifier (applied in calc_bonuses) */
    /* This is now handled via p_ptr->hunger in calc_bonuses() */
    /* Each stack adds +1 to hunger rate, giving 3x, 9x, 27x scaling */

    /* Digest some food */
    (void)set_food(p_ptr->food - i);

    /* Starve to death (slowly) */
    if (p_ptr->food < PY_FOOD_STARVE)
    {
        /* Calculate damage */
        i = 1; // old: (PY_FOOD_STARVE - p_ptr->food) / 10;

        /* Take damage */
        killer_mark_other(SCORE_KILLER_OTHER);
        take_hit(i, "starvation");
    }

    /* Lower the staircasiness */
    if (p_ptr->staircasiness > 0)
    {
        // decreases much faster on the escape
        if (p_ptr->on_the_run)
        {
            // amount is one hundredth of the current value, rounding up
            amount = (p_ptr->staircasiness + 99) / 100;
        }

        else
        {
            // amount is one thousandth of the current value, rounding up
            amount = (p_ptr->staircasiness + 999) / 1000;
        }

        p_ptr->staircasiness -= amount;
    }

    /* Regeneration ability */
    regen_multiplier = p_ptr->regenerate + 1;

    /* Regenerate the mana */
    if (p_ptr->csp < p_ptr->msp)
    {
        regenmana(regen_multiplier);
    }

    /* Various things interfere with healing */
    if (p_ptr->food < PY_FOOD_STARVE)
        regen_multiplier = 0;
    if (p_ptr->poisoned)
        regen_multiplier = 0;
    if (p_ptr->cut)
        regen_multiplier = 0;

    /* Regenerate Hit Points if needed */
    if (p_ptr->chp < p_ptr->mhp)
    {
        regenhp(regen_multiplier);
    }

    /*** Timeout Various Things ***/

    amount = 1;

    /* Hack -- Hallucinating */
    if (p_ptr->image)
    {
        (void)set_image(p_ptr->image - amount);
    }

    /* Blindness */
    if (p_ptr->blind)
    {
        (void)set_blind(p_ptr->blind - amount);
    }

    /* Timed see-invisible */
    if (p_ptr->tim_invis)
    {
        (void)set_tim_invis(p_ptr->tim_invis - 1);
    }

    /* Entranced */
    if (p_ptr->entranced)
    {
        (void)set_entranced(p_ptr->entranced - amount);
    }

    /* Confusion */
    if (p_ptr->confused)
    {
        (void)set_confused(p_ptr->confused - amount);
    }

    /* Afraid */
    if (p_ptr->afraid)
    {
        (void)set_afraid(p_ptr->afraid - amount);
    }

    /* Darkened */
    if (p_ptr->darkened)
    {
        (void)set_darkened(p_ptr->darkened - amount);
    }

    /* Fast */
    if (p_ptr->fast)
    {
        (void)set_fast(p_ptr->fast - 1);
    }

    /* Slow */
    if (p_ptr->slow)
    {
        if (singing(SNG_FREEDOM))
            (void)set_slow(p_ptr->slow - ability_bonus(S_SNG, SNG_FREEDOM));
        else
            (void)set_slow(p_ptr->slow - 1);
    }

    /* Rage */
    if (p_ptr->rage)
    {
        (void)set_rage(p_ptr->rage - 1);
    }

    /* Temporary Strength */
    if (p_ptr->tmp_str)
    {
        (void)set_tmp_str(p_ptr->tmp_str - 1);
    }

    /* Temporary Dexterity */
    if (p_ptr->tmp_dex)
    {
        (void)set_tmp_dex(p_ptr->tmp_dex - 1);
    }

    /* Temporary Constitution */
    if (p_ptr->tmp_con)
    {
        (void)set_tmp_con(p_ptr->tmp_con - 1);
    }

    /* Temporary Grace */
    if (p_ptr->tmp_gra)
    {
        (void)set_tmp_gra(p_ptr->tmp_gra - 1);
    }

    /* Temporary Perception */
    if (p_ptr->tmp_per)
    {
        (void)set_tmp_per(p_ptr->tmp_per - 1);
    }

    /* Song of Challenge lingering effect */
    if (p_ptr->song_challenge_effect)
    {
        p_ptr->song_challenge_effect -= 1;
    }

    /* Song of Elbereth lingering effect */
    if (p_ptr->song_elbereth_effect)
    {
        p_ptr->song_elbereth_effect -= 1;
    }

    /* Oppose Fire */
    if (p_ptr->oppose_fire)
    {
        (void)set_oppose_fire(p_ptr->oppose_fire - 1);
    }

    /* Oppose Cold */
    if (p_ptr->oppose_cold)
    {
        (void)set_oppose_cold(p_ptr->oppose_cold - 1);
    }

    /* Oppose Poison */
    if (p_ptr->oppose_pois)
    {
        (void)set_oppose_pois(p_ptr->oppose_pois - 1);
    }

    /*** Poison and Stun and Cut ***/

    /* Poison */
    if (p_ptr->poisoned)
    {
        // adjust is one fifth of the poison, rounding up
        int adjust = (p_ptr->poisoned + 4) / 5;

        /* Apply some healing */
        (void)set_poisoned(p_ptr->poisoned - adjust * amount);
    }

    /* Stun */
    if (p_ptr->stun)
    {
        int adjust = 1;

        /* Apply some healing */
        (void)set_stun(p_ptr->stun - adjust * amount);
    }

    /* Cut */
    if (p_ptr->cut)
    {
        // adjust is one fifth of the wound, rounding up
        int adjust = (p_ptr->cut + 4) / 5;

        /* Apply some healing */
        (void)set_cut(p_ptr->cut - adjust * amount);
    }

    // reset the focus flag if the player didn't 'pass' this turn
    if (p_ptr->previous_action[0] != 5)
    {
        p_ptr->focused = false;
    }

    // if the player didn't attack or 'pass' then the consecutive attacks needs
    // to be reset
    if (!player_attacked && (p_ptr->previous_action[0] != 5))
    {
        p_ptr->consecutive_attacks = 0;
        p_ptr->last_attack_m_idx = 0;
    }

    // boots of radiance
    if (inventory[INVEN_FEET].k_idx)
    {
        u32b f1, f2, f3;
        object_type* o_ptr = &inventory[INVEN_FEET];

        /* Extract the flags */
        object_flags(o_ptr, &f1, &f2, &f3);

        if (f2 & (TR2_RADIANCE))
        {
            if (!(cave_info[p_ptr->py][p_ptr->px] & (CAVE_GLOW)))
            {
                if (!object_known_p(o_ptr) && one_in_(10))
                {
                    char o_short_name[80];
                    char o_full_name[80];

                    object_desc(
                        o_short_name, sizeof(o_short_name), o_ptr, false, 0);
                    object_aware(o_ptr);
                    object_known(o_ptr);
                    object_desc(
                        o_full_name, sizeof(o_full_name), o_ptr, true, 3);

                    msg_print("Your footsteps leave a trail of light!");
                    msg_format("You recognize your %s to be %s", o_short_name,
                        o_full_name);
                }

                cave_info[p_ptr->py][p_ptr->px] |= CAVE_GLOW;
            }
        }
    }

    playerturn++;

    /* Count down active narrative banners by full player turns.
       0-turn banners are dismissed in request_command() before any action. */
    if (g_banner_force_redraw_remaining > 0)
    {
        if (g_active_partition_banner_skip_next_decay)
        {
            g_active_partition_banner_skip_next_decay = false;
        }
        else
        {
            g_banner_force_redraw_remaining--;
        }
        if (g_banner_force_redraw_remaining == 0)
        {
            g_active_partition_banner_consumes_input = false;
            g_active_partition_banner_text[0] = '\0';
            do_cmd_redraw();
        }
    }

    min_depth_timer_status(NULL, NULL, &depth_counter_increment, NULL, NULL);

    min_depth_counter += depth_counter_increment > 0 ?
        depth_counter_increment : 0;

    /* Window stuff */

    // Sil-y: note that these are now being set every single turn, somewhat
    // defeating their purpose
    p_ptr->window |= (PW_INVEN | PW_EQUIP);
    
    /*
     * Do NOT set PW_COMBAT_ROLLS unconditionally here - it should only be
     * set when combat data actually changes (via update_combat_rolls functions).
     * Setting it every turn causes the combat roll subwindow to refresh with
     * stale data before new combat happens, creating a one-turn delay.
     * 
     * Also, do NOT refresh the main-terminal combat rolls here.
     * We refresh them after monster processing in the main loop so that
     * both sides of the current round (player and monsters) are included.
     */
}

/*
 * Interact with the current dungeon level.
 *
 * This function will not exit until the level is completed,
 * the user dies, or the game is terminated.
 */
static void dungeon(void)
{
    monster_type* m_ptr;
    int i;

    log_debug("Entering dungeon level %d", p_ptr->depth);

    /* Play level transition sound (but not on first entry) */
    if (!first_entry_to_dungeon) {
        sound(MSG_LEVEL);
    }
    first_entry_to_dungeon = false;
    
    /* Depth 0 (the Gates) is still active gameplay and should use ambient music. */
    bool was_in_dungeon = (last_music_depth >= 0);
    bool now_in_dungeon = (p_ptr->depth >= 0);
    
    if (now_in_dungeon && !was_in_dungeon) {
        /* Entering dungeon from surface - switch to ambient */
        log_debug("Switching to ambient music (entering dungeon)");
        sdl_music_stop_main();
        sdl_music_play_ambient();
    } else if (!now_in_dungeon && was_in_dungeon) {
        /* Leaving dungeon gameplay - keep ambient running, clear any overlay. */
        log_debug("Leaving dungeon gameplay - preserving ambient music");
        sdl_music_stop_main();
        sdl_music_play_ambient();
    }
    
    last_music_depth = p_ptr->depth;

    /* Hack -- enforce illegal panel */
    p_ptr->wy = p_ptr->cur_map_hgt;
    p_ptr->wx = p_ptr->cur_map_wid;

    /* Not leaving */
    p_ptr->leaving = false;

    /* Reset the "command" vars */
    p_ptr->command_cmd = 0;
    p_ptr->command_new = 0;
    p_ptr->command_rep = 0;
    p_ptr->command_arg = 0;
    p_ptr->command_dir = 0;

    /* Cancel the target */
    target_set_monster(0);

    /* Cancel the health bar */
    health_track(0);

    /* Reset shimmer flags */
    shimmer_monsters = true;
    shimmer_objects = true;

    /* Reset repair flags */
    repair_mflag_show = true;
    repair_mflag_mark = true;

    /* Disturb */
    disturb(0, 0);

    /* Track maximum dungeon level */
    if (p_ptr->max_depth < p_ptr->depth)
    {
        log_info("Player reached new maximum depth: %d", p_ptr->depth);
        for (i = p_ptr->max_depth + 1; i <= p_ptr->depth; i++)
        {
            if (i > 1)
            {
                int new_exp = i * 50;
                gain_exp(new_exp);
                p_ptr->descent_exp += new_exp;

                log_debug("Depth %d reached, gained %d descent experience", i, new_exp);

                // Sil-x
                // do_cmd_note(format("exp:%d = s:5000 + e:%d + k:%d + d:%d +
                // i:%d",
                //		    p_ptr->exp, p_ptr->encounter_exp,
                // p_ptr->kill_exp, p_ptr->descent_exp, p_ptr->ident_exp), i);
            }
        }
        p_ptr->max_depth = p_ptr->depth;
    }

    /* No stairs from the surface */
    if (!p_ptr->depth)
    {
        p_ptr->create_stair = false;
    }

    /* Make a staircase */
    if (p_ptr->create_stair)
    {
        log_debug("Creating staircase at player position");
        /* Place a staircase */
        if (cave_valid_bold(p_ptr->py, p_ptr->px))
        {
            /* XXX XXX XXX */
            delete_object(p_ptr->py, p_ptr->px);

            cave_set_feat(p_ptr->py, p_ptr->px, p_ptr->create_stair);

            /* Mark the stairs as known */
            cave_info[p_ptr->py][p_ptr->px] |= (CAVE_MARK);

            log_trace("Staircase created and marked at (%d, %d)", p_ptr->py, p_ptr->px);
        }

        /* Cancel the stair request */
        p_ptr->create_stair = false;
    }

    /* Make rubble */
    if (p_ptr->create_rubble)
    {
        log_debug("Creating rubble via earthquake");
        earthquake(p_ptr->py, p_ptr->px, -1, -1, 5, 0);

        /* Cancel the rubble request */
        p_ptr->create_rubble = false;
    }

    /* Choose panel */
    log_debug("Verifying panel position");
    verify_panel();

    /* Flush messages */
    log_debug("Flushing messages");
    message_flush();

    /* Set labyrinth LOS-only map restriction before first draw. */
    update_labyrinth_view_state(false);

    /* Hack -- Increase "xtra" depth */
    log_debug("Increasing character_xtra depth for display setup");
    character_xtra++;

    /* Clear */
    log_debug("Clearing terminal");
    Term_clear();

    /* Update stuff */
    log_info("Starting initial dungeon display setup");
    p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA);

    /* Update stuff */
    log_debug("Running initial update_stuff");
    update_stuff();

    /* Fully update the visuals (and monster distances) */
    log_debug("Setting up view and distance updates");
    p_ptr->update |= (PU_FORGET_VIEW | PU_UPDATE_VIEW | PU_DISTANCE);

    /* Redraw dungeon */
    log_debug("Setting up full redraw");
    p_ptr->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP | PR_EQUIPPY | PR_RESIST);

    /* Window stuff */
    log_debug("Setting up window updates");
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

    /* Window stuff */
    p_ptr->window |= (PW_MONSTER | PW_MONLIST | PW_COMBAT_ROLLS);

    /* Update main terminal combat rolls if enabled */
    if (op_ptr->main_combat_rolls > 0)
    {
        display_main_combat_rolls();
    }

    /* Window stuff */
    p_ptr->window |= (PW_OVERHEAD);

    /* Update stuff */
    log_debug("Running second update_stuff");
    update_stuff();

    /* Redraw stuff */
    log_debug("Running redraw_stuff");
    redraw_stuff();

    /* Redraw stuff */
    log_debug("Running window_stuff");
    window_stuff();

    /* Hack -- Decrease "xtra" depth */
    log_debug("Decreasing character_xtra depth after display setup");
    character_xtra--;

    /* Update stuff */
    log_debug("Final update_stuff in setup");
    p_ptr->update |= (PU_BONUS | PU_HP | PU_MANA);

    /* Combine / Reorder the pack */
    log_debug("Setting up inventory notices");
    p_ptr->notice |= (PN_COMBINE | PN_REORDER);

    /* Notice stuff */
    log_debug("Running notice_stuff");
    notice_stuff();

    /* Update stuff */
    log_debug("Running final update_stuff");
    update_stuff();

    /* Redraw stuff */
    log_debug("Running final redraw_stuff");
    redraw_stuff();

    /* Window stuff */
    log_debug("Running final window_stuff");
    window_stuff();

    /* Refresh */
    log_debug("Final terminal refresh");
    Term_fresh();

    /* Show partition entry messages/XP after the initial draw so they can't be cleared by the setup flush. */
    {
        int entry_mode = PARTITION_NARRATIVE_OFF;
        if (op_ptr->level_entry_narrative_mode == LEVEL_ENTRY_NARRATIVE_MESSAGE)
            entry_mode = PARTITION_NARRATIVE_MESSAGE;
        handle_partition_entry(true, entry_mode);
    }

    log_info("Dungeon display setup completed successfully");

    /* Log final state after setup */
    log_debug("Final setup state: character_generated=%s, character_icky=%d, update=0x%08X, redraw=0x%08X, window=0x%08X",
              character_generated ? "true" : "false", character_icky,
              p_ptr->update, p_ptr->redraw, p_ptr->window);

    /* Handle delayed death */
    if (p_ptr->is_dead) {
        log_info("Player is dead, exiting dungeon");
        return;
    }

    /* Announce (or repeat) the feeling */
    // if ((p_ptr->depth) && (do_feeling)) do_cmd_feeling();

    /* Announce a player ghost challenge. -LM- */
    if (bones_selector)
        ghost_challenge();

    // explain the truce for the final level
    if ((p_ptr->depth == MORGOTH_DEPTH) && p_ptr->truce)
    {
        msg_print("There is a strange tension in the air.");
        if (p_ptr->skill_use[S_PER] >= 15)
            msg_print("You feel that Morgoth's servants are reluctant to "
                      "attack before he "
                      "delivers judgment.");
    }

    /*** Process this dungeon level ***/

    /* Reset generation depth; the Gates use depth 20 tables while displayed as 0. */
    monster_level = player_generation_depth();
    object_level = player_generation_depth();

    /* Show initial partition narrative according to the configured display mode. */
    if ((op_ptr->level_entry_narrative_mode == LEVEL_ENTRY_NARRATIVE_BANNER_DELAY)
        || (op_ptr->level_entry_narrative_mode == LEVEL_ENTRY_NARRATIVE_BANNER))
    {
        int spawn_sidx = styles_decode_color_style(cave_color[p_ptr->py][p_ptr->px]);
        level_partition_kind spawn_kind =
            level_partition_kind_for_point(p_ptr->py, p_ptr->px);
        if (spawn_sidx >= 0) {
            display_partition_narrative_banner(
                -1, spawn_sidx, spawn_kind,
                op_ptr->level_entry_narrative_mode
                    == LEVEL_ENTRY_NARRATIVE_BANNER_DELAY);
        }
    }

    varda_quest_notice_bastion_level_entry();

    was_in_morgoth_vault = (p_ptr->depth == MORGOTH_DEPTH) && (cave_info[p_ptr->py][p_ptr->px] & CAVE_G_VAULT);
    if ((p_ptr->depth == MORGOTH_DEPTH) && !p_ptr->morgoth_hall_entered
        && (was_in_morgoth_vault || (silmarils_possessed() > 0)))
    {
        p_ptr->morgoth_hall_entered = true;
    }
    log_live_special_vault_only_monsters("dungeon loop start");
    last_player_y = p_ptr->py;
    last_player_x = p_ptr->px;

    log_info("Starting main dungeon loop for depth %d", p_ptr->depth);

    /* Main loop */
    while (true)
    {
        /* Hack -- Compact the monster list occasionally */
        if (mon_cnt + 10 > MAX_MONSTERS) {
            log_debug("Compacting monster list (count: %d)", mon_cnt);
            compact_monsters(20);
        }

        /* Hack -- Compress the monster list occasionally */
        if (mon_cnt + 32 < MAX_MONSTERS)
            compact_monsters(0);

        /* Hack -- Compact the object list occasionally */
        if (o_cnt + 32 > z_info->o_max) {
            log_debug("Compacting object list (count: %d)", o_cnt);
            compact_objects(64);
        }

        /* Hack -- Compress the object list occasionally */
        if (o_cnt + 32 < o_max)
            compact_objects(0);

        /*** Apply energy ***/

          /* Can the player move? */
        while ((p_ptr->energy >= 100) && (!p_ptr->leaving))
          {
            /* Start a new combat round BEFORE any actors move this turn.
                    This ensures monsters that act before the player (due to higher
                    energy) are recorded in the same current round as the player's
                    actions, avoiding a one-turn lag in the bottom log. */
            log_trace("[LOOP] Begin player-energy turn: energy=%d", p_ptr->energy);
                new_combat_round();
            log_trace("[LOOP] After new_combat_round: turns_since_combat=%d combat_number=%d old=%d", turns_since_combat, combat_number, combat_number_old);

                /* Process monster with even more energy first */
            log_trace("[LOOP] process_monsters pre-player: threshold=%d", p_ptr->energy + 1);
            process_monsters(p_ptr->energy + 1);
            log_trace("[LOOP] after process_monsters pre-player: combat_number=%d old=%d", combat_number, combat_number_old);

                /* Show newly added monster attacks immediately so they are not perceived as a turn late */
                if (op_ptr->main_combat_rolls > 0)
                {
                    log_trace("[LOOP] interim display_main_combat_rolls pre-player");
                    display_main_combat_rolls();
                }

            /* If still alive */
            if (!p_ptr->leaving)
            {
                /* Update stuff */
                if (p_ptr->update) {
                    update_stuff();
                }

                /* Redraw stuff */
                if (p_ptr->redraw) {
                    redraw_stuff();
                }

                /* Process the player */
                log_trace("[LOOP] process_player start");
                process_player();
                log_trace("[LOOP] process_player end: combat_number=%d old=%d", combat_number, combat_number_old);
                
                /* Scan for artifacts near player and mark as seen */
                scan_artifacts_near_player();
                
                /* Set combat rolls window flag after player actions complete */
                if (combat_number > 0) {
                    p_ptr->window |= (PW_COMBAT_ROLLS);
                }
            }
        }

        /* Notice stuff */
        if (p_ptr->notice) {
            notice_stuff();
        }

        /* Update stuff */
        if (p_ptr->update) {
            update_stuff();
        }

        /* Redraw stuff */
        if (p_ptr->redraw) {
            redraw_stuff();
        }

        /* Redraw stuff */
        if (p_ptr->window) {
            window_stuff();
        }

        /* Place the cursor on the player or target */
        if (hilite_player)
            move_cursor_relative(p_ptr->py, p_ptr->px);
        if (hilite_target && target_sighted())
            move_cursor_relative(p_ptr->target_row, p_ptr->target_col);

        /* Optional fresh */
        if (fresh_after)
            Term_fresh();

        /* Handle "leaving" */
    if (p_ptr->leaving) {
            log_info("Player leaving dungeon level %d", p_ptr->depth);
            break;
        }

        /* Process monsters (any that haven't had a chance to move yet) */
    log_trace("[LOOP] process_monsters post-player: threshold=100");
    process_monsters(100);
    log_trace("[LOOP] after process_monsters post-player: combat_number=%d old=%d", combat_number, combat_number_old);
    
        /* Set combat rolls window flag after all monster actions complete */
        if (combat_number > 0) {
            p_ptr->window |= (PW_COMBAT_ROLLS);
        }

        /* Update main terminal combat rolls after monster processing */
        if (op_ptr->main_combat_rolls > 0)
        {
            log_trace("[LOOP] display_main_combat_rolls() now");
            display_main_combat_rolls();
        }

        /* Notice stuff */
        if (p_ptr->notice)
            notice_stuff();

        /* Update stuff */
        if (p_ptr->update)
            update_stuff();

        /* Redraw stuff */
        if (p_ptr->redraw)
            redraw_stuff();

        /* Redraw stuff */
        if (p_ptr->window)
            window_stuff();

        /* Place the cursor on the player or target */
        if (hilite_player)
            move_cursor_relative(p_ptr->py, p_ptr->px);
        if (hilite_target && target_sighted())
            move_cursor_relative(p_ptr->target_row, p_ptr->target_col);

        /* Optional fresh */
        if (fresh_after)
            Term_fresh();

        /* Handle "leaving" */
    if (p_ptr->leaving)
            break;

        /* Process the world */
        process_world();

        /* Notice stuff */
        if (p_ptr->notice)
            notice_stuff();

        /* Update stuff */
        if (p_ptr->update)
            update_stuff();

        /* Redraw stuff */
        if (p_ptr->redraw)
            redraw_stuff();

        /* Window stuff */
        if (p_ptr->window)
            window_stuff();

        /* Place the cursor on the player or target */
        if (hilite_player)
            move_cursor_relative(p_ptr->py, p_ptr->px);
        if (hilite_target && target_sighted())
            move_cursor_relative(p_ptr->target_row, p_ptr->target_col);

        /* Optional fresh */
        if (fresh_after)
            Term_fresh();

        /* Handle "leaving" */
    if (p_ptr->leaving)
            break;

        /* Give the player some energy */
        p_ptr->energy += extract_energy[p_ptr->pspeed];

        /* Give energy to all monsters */
        bool freeze_morgoth_vault = (p_ptr->depth == MORGOTH_DEPTH)
            && !p_ptr->morgoth_hall_entered && (silmarils_possessed() == 0);
        for (i = mon_max - 1; i >= 1; i--)
        {
            /* Access the monster */
            m_ptr = &mon_list[i];

            /* Ignore "dead" monsters */
            if (!m_ptr->r_idx)
                continue;

            /* Keep Morgoth's hall frozen until the player enters it */
            if (freeze_morgoth_vault
                && (cave_info[m_ptr->fy][m_ptr->fx] & CAVE_G_VAULT))
            {
                m_ptr->energy = 0;
                continue;
            }

            /* Give this monster some energy */
            m_ptr->energy += extract_energy[m_ptr->mspeed];
        }

        /* Count game turns */
        turn++;
    }
}

/* Tiny proxy for frontends to query current depth without including player headers */
int p_ptr_depth_proxy(void) { return p_ptr ? p_ptr->depth : 0; }

/*
 * Process some user pref files
 */
static void process_some_user_pref_files(void)
{
    char buf[1024];

    /* Process the "user.prf" file */
    (void)process_pref_file("user.prf");

    /* Process the "user.scb" autoinscriptions file */
    (void)process_pref_file("user.scb");

    /* Process the "races.prf" file */
    (void)process_pref_file("races.prf");

    /* Get the "PLAYER.prf" filename */
    (void)strnfmt(buf, sizeof(buf), "%s.prf", op_ptr->base_name);

    /* Process the "PLAYER.prf" file */
    (void)process_pref_file(buf);
}

/*
 * Hack - Know inventory upon death
 */
static void death_knowledge(void)
{
    int i;

    object_type* o_ptr;

    /* Hack -- Know everything in the inven/equip */
    for (i = 0; i < INVEN_TOTAL; i++)
    {
        o_ptr = &inventory[i];

        /* Skip non-objects */
        if (!o_ptr->k_idx)
            continue;

        /* Aware and Known */
        object_aware(o_ptr);
        object_known(o_ptr);
    }

    p_ptr->window |= (PW_INVEN | PW_EQUIP);

    /* Hack -- Recalculate bonuses */
    p_ptr->update |= (PU_BONUS);

    /* Handle stuff */
    handle_stuff();
}

static bool story_intro_skip_requested(void)
{
    char check_key;

    if (Term_inkey(&check_key, false, false) == 0)
    {
        Term_inkey(&check_key, false, true);
        if (check_key == ESCAPE || check_key == '\n' || check_key == '\r')
            return true;
    }

    return false;
}

static int story_intro_count_paragraph_rows(cptr text, int wrap_width)
{
    int rows = 0;
    int col = 0;
    bool line_has_content = false;
    bool pending_space = false;
    cptr s = text ? text : "";

    if (wrap_width < 1)
        wrap_width = 1;

    while (*s)
    {
        int word_len = 0;

        if (*s == '\n')
        {
            col = 0;
            line_has_content = false;
            pending_space = false;
            s++;
            continue;
        }

        if (*s == ' ' || *s == '\t')
        {
            pending_space = line_has_content;
            s++;
            continue;
        }

        while (s[word_len] && s[word_len] != ' ' && s[word_len] != '\t' && s[word_len] != '\n')
            word_len++;

        if (pending_space && line_has_content)
        {
            if (col + 1 + word_len > wrap_width)
            {
                col = 0;
                line_has_content = false;
            }
            else
            {
                col++;
            }
            pending_space = false;
        }

        for (int i = 0; i < word_len; ++i)
        {
            if (col >= wrap_width)
            {
                col = 0;
                line_has_content = false;
            }

            if (!line_has_content)
            {
                rows++;
                line_has_content = true;
            }

            col++;
        }

        s += word_len;
    }

    return (rows > 0) ? rows : 1;
}

static void story_intro_putch(int x, int y, char ch, bool *skipped)
{
    if (!*skipped && story_intro_skip_requested())
        *skipped = true;

    Term_putch(x, y, TERM_WHITE, ch);

    if (!*skipped)
    {
        Term_fresh();
        Term_xtra(TERM_XTRA_DELAY, 30);
    }
}

static bool story_intro_render_paragraph(cptr text, int indent, int wrap_width, int *row)
{
    int col = 0;
    bool line_has_content = false;
    bool pending_space = false;
    bool skipped = false;
    cptr s = text ? text : "";

    if (!row)
        return false;

    if (wrap_width < 1)
        wrap_width = 1;

    while (*s)
    {
        int word_len = 0;

        if (*s == '\n')
        {
            (*row)++;
            col = 0;
            line_has_content = false;
            pending_space = false;
            s++;
            continue;
        }

        if (*s == ' ' || *s == '\t')
        {
            pending_space = line_has_content;
            s++;
            continue;
        }

        while (s[word_len] && s[word_len] != ' ' && s[word_len] != '\t' && s[word_len] != '\n')
            word_len++;

        if (pending_space && line_has_content)
        {
            if (col + 1 + word_len > wrap_width)
            {
                (*row)++;
                col = 0;
                line_has_content = false;
            }
            else
            {
                story_intro_putch(indent + col, *row, ' ', &skipped);
                col++;
            }
            pending_space = false;
        }

        for (int i = 0; i < word_len; ++i)
        {
            if (col >= wrap_width)
            {
                (*row)++;
                col = 0;
                line_has_content = false;
            }

            story_intro_putch(indent + col, *row, s[i], &skipped);
            col++;
            line_has_content = true;
        }

        s += word_len;
    }

    if (skipped)
        Term_fresh();

    return skipped;
}

/**
 * Introductory narrative display, one paragraph per prompt.
 * Implemented as a static function to restrict linkage.
 */
static void print_story_intro(void)
{
    bool story_intro_story_font = true;
    screen_push_supporting_panes_hidden();
    sdl_story_font_enable();
    sdl_music_play_main_full();
    int wid, h;
    const int indent = 2;

    /* Narrative paragraphs as valid C string literals with embedded \n */
    cptr intro_texts[] = {
        "You awaken in darkness.\n"
        "No name. No memory.\n"
        "Only a quiet ache of courage deep inside you,\n"
        "like embers buried beneath ash.\n",

        "Far below, Morgoth waits upon his throne-\n"
        "iron-dark and crowned in flame.\n"
        "Upon his brow shine three Silmarils, stolen stars.\n"
        "He senses your stirring. He knows you will come.\n",

        "Far above, beyond the shadows of Angband,\n"
        "the Valar watch silently.\n"
        "They offer no guidance, yet their presence\n"
        "fills you with strength-and dread.\n",

        "You will return many times, each death and rebirth\n"
        "etched into the endless stone halls of Mandos.\n"
        "Each fall will draw your spirit deeper into shadow,\n"
        "closer to a doom from which you cannot escape.\n",

        "Yet each victory-each Silmaril wrested from Morgoth's crown-\n"
        "will brighten the Valar's hope,\n"
        "even as your soul grows thinner,\n"
        "your strength fading with every triumph.\n",

        "You envy the Edain, whose Gift from Iluvatar\n"
        "frees them from the bonds of Mandos and the world.\n"
        "Yet you do not know if such release can ever be yours.\n"
        "You do not know who-or even what-you truly are.\n",

        "For each time you awaken,\n"
        "you will carry the names of heroes beloved and feared-\n"
        "bright spirits, fiery hearts, proud kings and exiles,\n"
        "wanderers beneath sun and stars,\n"
        "whose courage you borrow, but whose fates are not your own.\n",

        "This is the trial set by the Valar:\n"
        "to reclaim your forgotten name,\n"
        "to balance shadow and light,\n"
        "and to find within the borrowed glory of others\n"
        "your true self.\n",

        "Now the path before you opens,\n"
        "and your trial begins.\n"
    };

    int total = sizeof(intro_texts) / sizeof(intro_texts[0]);
    Term_get_size(&wid, &h);
    int wrap_width = wid - indent;

    /* Start on a blank screen */
    Term_clear();
    int row = 1;

    for (int idx = 0; idx < total; idx++) {
        const char *s = intro_texts[idx];
        int lines_needed = story_intro_count_paragraph_rows(s, wrap_width) + 1;
        bool skipped;

        /* Check if we have enough space for the whole paragraph */
        if (row + lines_needed >= h - 1) {
            Term_putstr(15, h - 1, -1, TERM_L_WHITE, "(press any key)");
            hide_cursor = true;
            {
                char k = inkey();
                if (k == 'S') { /* Capital S skips the intro entirely */
                    Term_clear();
                    goto cleanup_intro;
                }
            }
            Term_clear();
            row = 1;
        }

        skipped = story_intro_render_paragraph(s, indent, wrap_width, &row);

        /* Leave one blank line after each paragraph */
        row++;

        /* 1 second pause after paragraph (skip if we already skipped typewriter) */
        if (!skipped) {
            Term_xtra(TERM_XTRA_DELAY, 1000);
        }
    }

    /* Final "finish" prompt with difficulty option */
    Term_putstr(8, h - 2, -1, TERM_L_WHITE, "[c] Change difficulty (experienced players)");
    Term_putstr(15, h - 1, -1, TERM_L_WHITE, "(press any key to finish)");

    /* Handle input */
    hide_cursor = true;
    char key = inkey();
    if (key == 'S') {
        Term_clear();
        goto cleanup_intro;
    }
    if (key == 'c' || key == 'C')
    {
        Term_clear();
        choose_difficulty_level();
        goto cleanup_intro;
    }

    Term_clear();

    /* Flush any queued keypresses that accumulated during the intro */
    Term_flush();

cleanup_intro:
    screen_pop_supporting_panes_hidden();
    if (story_intro_story_font)
        sdl_story_font_reset();
    
    return;
}

static void maybe_show_blitz_unlock_screen(void)
{
    int wid = 80;
    int hgt = 24;
    int row = 2;

    if (run_mode_is_blitz())
        return;
    if (!op_ptr || op_ptr->opt[OPT_unlock_blitz_mode])
        return;
    if (metarun_completed_count() < 1)
        return;

    screen_save();
    Term_clear();
    Term_get_size(&wid, &hgt);
    row = 2;

    c_put_str(TERM_YELLOW, "Congratulations, you have unlocked Blitz Mode!",
        row++, MAX((wid - 47) / 2, 0));
    row += 2;

    text_out_hook = text_out_to_screen;
    text_out_wrap = MAX(20, wid - 4);
    text_out_indent = 2;

    Term_gotoxy(2, row);
    text_out_c(TERM_L_WHITE, "Blitz is a self-contained challenge run.");
    row += count_wrapped_lines("Blitz is a self-contained challenge run.", text_out_wrap, 2) + 1;

    Term_gotoxy(2, row);
    text_out_c(TERM_WHITE,
        "Story progress, metaruns, saves, and score stay separate.");
    row += count_wrapped_lines(
        "Story progress, metaruns, saves, and score stay separate.",
        text_out_wrap, 2) + 1;

    Term_gotoxy(2, row);
    text_out_c(TERM_WHITE,
        "Each Blitz run lets you choose character flow, oaths, blessings, and curses.");
    row += count_wrapped_lines(
        "Each Blitz run lets you choose character flow, oaths, blessings, and curses.",
        text_out_wrap, 2) + 1;

    Term_gotoxy(2, row);
    text_out_c(TERM_SLATE,
        "Run history entries are still recorded and marked as Blitz.");
    row += count_wrapped_lines(
        "Run history entries are still recorded and marked as Blitz.",
        text_out_wrap, 2) + 1;

    c_put_str(TERM_L_BLUE, "Press any key to continue.", MIN(row + 1, hgt - 1), 2);
    Term_fresh();
    (void)inkey();
    screen_load();

    op_ptr->opt[OPT_unlock_blitz_mode] = true;
    save_pane_config_to_json();
}



/*
 * Actually play a game.
 *
 * This function is called from a variety of entry points, since both
 * the standard "main.c" file, as well as several platform-specific
 * "main-xxx.c" files, call this function to start a new game with a
 * new savefile, start a new game with an existing savefile, or resume
 * a saved game with an existing savefile.
 *
 * If the "new_game" parameter is true, and the savefile contains a
 * living character, then that character will be killed, so that the
 * player may start a new game with that savefile.  This is only used
 * by the "-n" option in "main.c".
 *
 * If the savefile does not exist, cannot be loaded, or contains a dead
 * (non-wizard-mode) character, then a new game will be started.
 *
 * Some platforms (Windows) start brand new games with "savefile" and 
 * "op_ptr->base_name" both empty, and initialize them later based on 
 * the player name. To prevent weirdness, we must initialize 
 * "op_ptr->base_name" to "nameless" if it is empty.
 *
 * Note that we load the RNG state from savefiles and
 * only initialize it when starting a brand new character.
 */
PlayResult play_game(void)
{
    bool new_game = false;

    log_info("play_game: FUNCTION ENTERED");

    /* Safety: Fix character_icky imbalance from previous game sessions */
    if (character_icky != 0)
    {
        log_info("play_game: Fixing character_icky imbalance - was %d, resetting to 0", character_icky);
        character_icky = 0;
    }

    /* Hack -- Increase "icky" depth */
    character_icky++;
    log_debug("play_game: character_icky incremented to %d", character_icky);

    /* Verify main term */
    if (!term_screen)
    {
        quit("main window does not exist");
    }

    /* Make sure main term is active */
    Term_activate(term_screen);

    /* Verify minimum size */
    {
        /* get_sdl_min_terminal_mode() returns 0=normal, 1=compact. */
        const bool compact_mode = (get_sdl_min_terminal_mode() != 0);
        const int min_hgt = compact_mode ? 18 : 24;
        const int min_wid = compact_mode ? 50 : 80;
        if ((Term->hgt < min_hgt) || (Term->wid < min_wid))
        {
#if defined(__ANDROID__) || defined(SIL_IOS)
            log_error("main window too small on mobile: %dx%d (need at least %dx%d)",
                Term->wid, Term->hgt, min_wid, min_hgt);
#else
            log_error("main window too small: %dx%d (need at least %dx%d)",
                Term->wid, Term->hgt, min_wid, min_hgt);
#endif
            quit("main window is too small");
        }
    }

    /* Hack -- Turn off the cursor */
    (void)Term_set_cursor(false);

    /* Hack -- Default base_name */
    if (!op_ptr->base_name[0])
    {
        SDL_strlcpy(op_ptr->base_name, "nameless", sizeof(op_ptr->base_name));
    }

    run_mode_activate_pending();
    maybe_show_blitz_unlock_screen();

    bool startup_stats_screen = false;

    if (!run_mode_is_blitz()) {
        if (metarun_created) /* show only the first time ever */
            print_story_intro();
        else {
            print_metarun_stats();
            startup_stats_screen = true;
        }

        /* Story-intro handoff still wants the next startup screen to own the
         * full redraw. Story statistics may keep its frame alive a little
         * longer so a delayed "Loading..." overlay can reuse it during
         * autoload instead of flashing a separate screen. */
        if (!startup_stats_screen) {
            screen_clear_all_terms_no_fresh();
            message_discard_pending();
        }
    }

    /* New startup behavior: try to auto-load any alive character
     * lingering in the scorefile. If successful, skip character
     * selection and proceed directly. */
    character_loaded = false;
    character_loaded_dead = false;
    if (startup_stats_screen)
        startup_loading_overlay_arm();
    bool autoloaded = autoload_alive_from_scores();
    if (startup_stats_screen)
        startup_loading_overlay_disarm();
    if (autoloaded && character_loaded)
    {
        log_info("Auto-loaded alive character from scores; skipping selection");
        new_game = false;
    }
    else if (startup_stats_screen)
    {
        screen_clear_all_terms_no_fresh();
        message_discard_pending();
    }

    log_info("Starting new game session");

     /* Only reset flags if no character has been loaded yet.
         If autoload succeeded, keep the loaded state and the
         dungeon-loaded flag set by load_player(). */
     if (!character_loaded) {
          character_dungeon = false;
          character_loaded = false;
          character_loaded_dead = false;
     }

    for (;;)
    {
        /* If we already loaded a living character, break to init */
        if (character_loaded) break;

        /* Wipe the player each time we (re)enter creation */
        player_wipe();

    log_info("Choosing character");
        NavResult cr = run_mode_is_blitz() ? blitz_character_creation()
                                           : character_creation();
        if (cr == NAV_TO_MAIN) {
            log_info("Returning to main menu from character creation");
            sdl_music_stop_main();
            sdl_music_stop_ambient();
            return PLAY_DONE;
        }
        if (cr == NAV_QUIT) {
            log_info("Quitting from character creation");
            return PLAY_QUIT;
        }

        /* Set player name from character BEFORE load_player() so savefile path is correct */
        SDL_strlcpy(op_ptr->full_name, c_name + c_info[p_ptr->pcharacter].name, sizeof(op_ptr->full_name));
        process_player_name(true);  /* Update savefile path */
        log_debug("Player name set to: %s (character %d), savefile: %s", op_ptr->full_name, p_ptr->pcharacter, savefile);

        /* Attempt to load (manual path) */
        (void)load_player();

        /*
         * If we loaded a dead character savefile, load_player() returns false but
         * leaves p_ptr populated with the dead character's state. We need to wipe
         * and properly re-initialize for a fresh start with the same character type.
         */
        if (character_loaded_dead)
        {
            log_info("Loaded dead character from '%s' - wiping for fresh restart",
                savefile);

            /* Wipe player data - this will restore prace/pcharacter/stats from dead char */
            player_wipe();

            /* Re-initialize global race/character pointers to match restored values */
            rp_ptr = &p_info[p_ptr->prace];
            current_character_profile = &c_info[p_ptr->pcharacter];

            /* Clear the flag after wipe so subsequent code knows we're starting fresh */
            character_loaded_dead = false;
        }

        log_info(character_loaded ? "Character loaded" :
            (character_loaded_dead ? "Character loaded dead" : "Character creation started"));

        new_game = !character_loaded;

        if (new_game)
        {
            log_info("Starting new game - initializing character");
        /* Init RNG */
        {
            u64b seed = (u64b)time(NULL);

#ifdef SET_UID
            seed ^= ((seed >> 3) * (getpid() << 1));
#endif

            Rand_state_init(seed);
            log_debug("RNG initialized with seed: %llu", (unsigned long long)seed);
        }

        log_info("Rolling up a new character");
        log_trace("Character creation phase: setting up dungeon state");
        /* The dungeon is not ready */
        character_dungeon = false;

        /* Hack -- seed for flavors */
        seed_flavor = rand_int(0x10000000);

        /* Hack -- seed for random artefacts */
        seed_randart = rand_int(0x10000000);

        log_debug("Game seeds initialized - flavor: %u, randart: %u", seed_flavor, seed_randart);

        /* Roll up a new character */
        NavResult br = player_birth();
        if (br == NAV_BACK) {
            log_debug("Returning to character selection from birth");
            /* back to Character Selection */
            continue;
        }
        if (br == NAV_TO_MAIN) {
            log_info("Returning to main menu from character birth");
            sdl_music_stop_main();
            sdl_music_stop_ambient();
            return PLAY_DONE;
        }
        if (br == NAV_QUIT) {
            log_info("Quitting from character birth");
            return PLAY_QUIT;
        }
        /* NAV_OK falls through */

        // Reset the autoinscriptions
        autoinscribe_clean();
        autoinscribe_init();

        log_debug("New character rolled up - autoinscriptions reset");

        /* Hack -- enter the world */
        if (!character_loaded) {
        turn = 1;
        playerturn = 0;
        min_depth_counter = 0;

        /* Start player on level 1 */
        p_ptr->depth = 1;

        log_debug("New game state initialized - starting at depth 1, turn 1");
        }
        }

        /* succeeded (either loaded or created) - exit the creation loop */
        break;
    }

    /* Normal machine (process player name) */
    if (savefile[0])
    {
        process_player_name(false);
    }

    /* Weird machine (process player name, pick savefile name) */
    else
    {
        process_player_name(true);
    }

    /* Only show story when no alive character exists (fresh start or all characters dead) */
    if (!run_mode_is_blitz() && score_count_alive_entries() == 0)
    {
        sdl_music_play_main_full();
        print_story(15,1);
        screen_clear_all_terms_no_fresh();
        message_discard_pending();
    }

    log_debug("Game initialization complete, starting main game loop");
    log_trace("QUEST DEBUG: Quest states loaded - Aule: %d, Mandos: %d, Tulkas: %d",
             p_ptr->aule_quest, p_ptr->mandos_quest, p_ptr->tulkas_quest);
    log_trace("QUEST DEBUG: Special abilities - have_ability[S_SPC][SPC_MANDOS]=%d, have_ability[S_SPC][SPC_AULE]=%d",
             p_ptr->have_ability[S_SPC][SPC_MANDOS], p_ptr->have_ability[S_SPC][SPC_AULE]);
    log_trace("QUEST DEBUG: Special abilities - active_ability[S_SPC][SPC_MANDOS]=%d, active_ability[S_SPC][SPC_AULE]=%d",
             p_ptr->active_ability[S_SPC][SPC_MANDOS], p_ptr->active_ability[S_SPC][SPC_AULE]);

    /* Validate quest states after load (auto-complete if targets are dead) */
    validate_tulkas_quest_on_load();

    /* Hack -- Enter wizard mode */
    if (arg_wizard && enter_wizard_mode())
    {
        p_ptr->wizard = true;
        log_debug("Wizard mode activated");
    }

    /* Flavor the objects */
    flavor_init();

    /* Build or load the drop catalog (needs flavored kinds) */
    drop_system_init();

    /* Reset visuals */
    reset_visuals(true);

    /* Window stuff */
    p_ptr->window |= (PW_INVEN | PW_EQUIP | PW_PLAYER_0);

    /* Window stuff */
    p_ptr->window |= (PW_MONSTER | PW_MESSAGE);

    /* Window stuff */
    window_stuff();

    /* Process some user pref files */
    process_some_user_pref_files();

    /* Set or clear "hjkl_movement" if requested */
    if (arg_force_original)
        hjkl_movement = false;
    if (arg_force_roguelike)
        hjkl_movement = true;

    /* React to changes */
    Term_xtra(TERM_XTRA_REACT, 0);

    /* Generate a dungeon level if needed */
    if (!character_dungeon)
    {
        log_info("Generating initial dungeon level");
        reset_level_entry_tracking();
        /* About to call generate_cave() function */
        generate_cave();
        log_debug("Initial dungeon level generated successfully");
    }

    /* Character is now "complete" */
    character_generated = true;
    log_debug("play_game: character_generated set to true - character creation complete");
    ability_log_sync_missing();
    snapshot_run_history("character start");

    /* If Tulkas quest was auto-completed on load, spawn Tulkas and show messages */
    if (p_ptr->tulkas_quest == TULKAS_QUEST_COMPLETE && p_ptr->tulkas_quest_complete == 1)
    {
        int y, x;
        bool spawned = false;
        
        log_trace("Spawning Tulkas for auto-completed quest on load");
        
        /* Try to find a suitable spot near the player */
        for (y = p_ptr->py - 3; y <= p_ptr->py + 3 && !spawned; y++)
        {
            for (x = p_ptr->px - 3; x <= p_ptr->px + 3 && !spawned; x++)
            {
                if (in_bounds(y, x) && cave_floor_bold(y, x) && 
                    cave_m_idx[y][x] == 0 && distance(p_ptr->py, p_ptr->px, y, x) >= 2)
                {
                    if (place_monster_one(y, x, R_IDX_TULKAS, true, true, NULL))
                    {
                        msg_print("Upon loading, you recall that your quest target has already fallen!");
                        msg_print("Tulkas Unclad materializes nearby with a booming laugh, ready to reward your valor!");
                        spawned = true;
                        log_trace("Tulkas spawned at (%d, %d) for auto-completed quest", y, x);
                    }
                }
            }
        }
        
        if (!spawned)
        {
            log_trace("Failed to spawn Tulkas near player, will retry on next level");
        }
    }

    /* Start with normal object generation mode */
    object_generation_mode = OB_GEN_MODE_NORMAL;

    /* Start playing */
    p_ptr->playing = true;
    metarun_created = false;

    log_info("Game session started - entering play mode");
    
    /* Any active run, including the Gates at depth 0, uses ambient gameplay music. */
    log_debug("Starting game session at depth=%d - switching to ambient music", p_ptr->depth);
    sdl_music_stop_main();
    sdl_music_play_ambient();
    last_music_depth = p_ptr->depth;

    /* Hack -- Enforce "delayed death" */
    if (p_ptr->chp <= 0)
        p_ptr->is_dead = true;

    /* Redraw everything */
    // Sil-y: added to get 'shades' right in extra inventory terms
    screen_set_startup_supporting_panes_hidden(false);
    do_cmd_redraw();

    // update player noise
    update_flow(p_ptr->py, p_ptr->px, FLOW_PLAYER_NOISE);

    // reset combat roll info
    turns_since_combat = 0;

    // assume the player is on the ground and not being knocked back
    p_ptr->leaping = false;
    p_ptr->knocked_back = false;

    /* Hack -- Decrease "icky" depth before entering main game loop */
    character_icky--;
    log_debug("play_game: character_icky decremented to %d (entering main game loop)", character_icky);

    /* Process */
    while (true)
    {
        log_trace("Starting dungeon level processing loop");
        /* Process the level */
        dungeon();

        /* Notice stuff */
        if (p_ptr->notice)
            notice_stuff();

        /* Update stuff */
        if (p_ptr->update)
            update_stuff();

        /* Redraw stuff */
        if (p_ptr->redraw)
            redraw_stuff();

        /* Window stuff */
        if (p_ptr->window)
            window_stuff();

        /* Cancel the target */
        target_set_monster(0);

        /* Cancel the health bar */
        health_track(0);

        /* Forget the view */
        forget_view();

        /* Handle "quit and save" */
        if (!p_ptr->playing && !p_ptr->is_dead)
        {
            log_info("Player quit and saved - exiting game loop");
            
            /* Stop gameplay audio; the title screen chooses its own track. */
            sdl_music_stop_main();
            sdl_music_stop_ambient();
            
            break;
        }

        /* Erase the old cave */
        /* If the character is dead, then we don't erase yet */
        if (!p_ptr->is_dead)
        {
            log_trace("Cleaning up level data for transition");
            wipe_o_list();
            wipe_mon_list();
        }

        /* XXX XXX XXX */
        message_flush();

        /* Accidental Death */
        if (p_ptr->playing && p_ptr->is_dead)
        {
            log_info("Player '%s' died at level %d, turn %d.",
                op_ptr->base_name, p_ptr->depth, turn);
            /* Mega-Hack -- Allow player to cheat death */
            if ((p_ptr->wizard || (p_ptr->noscore & 0x0008) || cheat_live)
                && !get_check("Die? "))
            {
                log_debug("Player cheated death - restoring to full health");
                /* Mark savefile */
                p_ptr->noscore |= 0x0001;

                /* Message */
                msg_print("You invoke wizard mode and cheat death.");
                message_flush();

                /* Cheat death */
                p_ptr->is_dead = false;

                /* Restore hit points */
                p_ptr->chp = p_ptr->mhp;
                p_ptr->chp_frac = 0;

                /* Restore voice */
                p_ptr->csp = p_ptr->msp;
                p_ptr->csp_frac = 0;

                /* Hack -- Healing */
                (void)set_blind(0);
                (void)set_confused(0);
                (void)set_poisoned(0);
                (void)set_afraid(0);
                (void)set_entranced(0);
                (void)set_image(0);
                (void)set_stun(0);
                (void)set_cut(0);
                (void)res_stat(A_STR, 20);
                (void)res_stat(A_CON, 20);
                (void)res_stat(A_DEX, 20);
                (void)res_stat(A_GRA, 20);

                /* Hack -- Prevent starvation */
                (void)set_food(PY_FOOD_FULL - 1);

                /* Note cause of death XXX XXX XXX */
                SDL_strlcpy(p_ptr->died_from, "Cheating death",
                    sizeof(p_ptr->died_from));

                /* Need to generate a new level */
                p_ptr->leaving = true;
            }
        }

        /* Take a mini screenshot for dead characters */
        if (p_ptr->is_dead)
        {
            log_debug("Character dead - taking screenshot and revealing map");
            
            /* Stop gameplay audio; the title screen chooses its own track. */
            sdl_music_stop_main();
            sdl_music_stop_ambient();
            if (p_ptr->escaped || p_ptr->morgoth_slain)
            {
                sdl_music_play_main();
            }
            else
            {
                sdl_music_play_death();
            }
            
            death_knowledge();

            do_cmd_wiz_unhide(255);
            update_view();
            mini_screenshot();
            detect_all_doors_traps();
        }

        /* Handle "death" */
        if (p_ptr->is_dead)
        {
            log_info("Character '%s' died - ending game session", op_ptr->base_name);
            break;
        }

        /* Make a new level */
        log_info("Generating new dungeon level at depth %d", p_ptr->depth);
        reset_level_entry_tracking();
        generate_cave();
        log_debug("New dungeon level generated successfully");
    }

    /* Close stuff */
    log_info("Player '%s' has left the game.", op_ptr->base_name);

    /* Hack -- Decrease "icky" depth */
    character_icky--;
    log_debug("play_game: character_icky decremented to %d (function exit)", character_icky);

    close_game();
    if (!p_ptr->is_dead && !p_ptr->playing)
    {
        if (p_ptr->quit_to_menu)
        {
            p_ptr->quit_to_menu = false; /* Reset the flag */
            return PLAY_DONE;
        }
        else
        {
            return PLAY_QUIT;
        }
    } else {
        return PLAY_DONE;
    }
}
