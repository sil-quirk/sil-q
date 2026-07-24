/* File: dungeon/dungeon-narrative.c */

#include "angband.h"
#include "dungeon-internal.h"

int g_banner_force_redraw_remaining = 0;
char g_active_partition_banner_text[1024] = "";
bool g_active_partition_banner_consumes_input = false;
/* Banners shown while resolving a command should survive until the next
 * command prompt rather than being consumed by the arrival action itself. */
bool g_active_partition_banner_skip_next_decay = false;

static int last_partition_pi = -1;
static level_partition_kind last_partition_kind = LEVEL_PART_NONE;

/* Track which partitions have been narrated and the last narrated style. */
static u32b partition_narrated_mask = 0;
static int last_narrated_style_idx = -1;

/* Forward declarations for partition kind helpers (defined later in file). */
static bool is_big_partition_kind(level_partition_kind kind);
static bool is_small_cave_partition_kind(level_partition_kind kind);
static cptr partition_display_name(level_partition_kind kind);

char greater_vault_xp_name[80] = "";
bool greater_vault_xp_awarded = false;

void reset_level_entry_tracking(void)
{
#ifdef USE_SDL
    sdl_side_map_pane_forget_level();
#endif
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

static byte narrative_banner_turn_setting(void)
{
    if (!op_ptr)
        return DEFAULT_NARRATIVE_BANNER_TURNS;

    if (op_ptr->narrative_banner_turns > NARRATIVE_BANNER_TURNS_MAX)
        return DEFAULT_NARRATIVE_BANNER_TURNS;

    return op_ptr->narrative_banner_turns;
}

bool active_narrative_banner_visible(void)
{
    return g_active_partition_banner_text[0]
        && (g_banner_force_redraw_remaining > 0);
}

cptr active_narrative_banner_text(void)
{
    return active_narrative_banner_visible()
        ? g_active_partition_banner_text
        : "";
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

bool dismiss_active_narrative_banner(void)
{
    bool was_visible = active_narrative_banner_visible();

    clear_active_narrative_banner();
    return was_visible;
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
    bool banner_with_delay =
        (narrative_mode == PARTITION_NARRATIVE_BANNER_DELAY);
    bool command_transition = p_ptr && (p_ptr->command_cmd != 0);

    if (!text || !text[0])
        return;

    if (narrative_mode == PARTITION_NARRATIVE_MESSAGE)
    {
        msg_print(text);
        return;
    }

    if ((narrative_mode != PARTITION_NARRATIVE_BANNER)
        && !banner_with_delay)
        return;

    /* Crossing into a narrated partition is a meaningful interruption, not
     * camera maintenance.  Stop normal running and SDL auto-walk before the
     * banner is presented so the player cannot continue through it. */
    if (command_transition)
        disturb(0, 0);

    /*
     * Partition entry is detected while cleaning up the movement command,
     * before the dungeon loop's next normal refresh.  Present the completed
     * move first so the player changes grids before the banner begins fading
     * in.  Initial level-entry banners have no active command and retain their
     * normal fade speed.
     */
    if (command_transition)
    {
        /* Do not let either the previous banner or the new banner flash at
         * full opacity on the movement frame before the fade begins. */
        clear_active_narrative_banner();
        handle_stuff();
        Term_fresh();
    }

    SDL_strlcpy(g_active_partition_banner_text, text,
        sizeof(g_active_partition_banner_text));
    g_active_partition_banner_consumes_input =
        (narrative_banner_turn_setting() == 0);
    g_active_partition_banner_skip_next_decay =
        command_transition;
    g_banner_force_redraw_remaining = g_active_partition_banner_consumes_input
        ? 1
        : narrative_banner_turn_setting();

    sdl_narrative_banner_show(
        line_delay || banner_with_delay, command_transition);
}

static void display_partition_narrative(int old_sidx, int new_sidx,
    level_partition_kind kind)
{
    char buf[1024];

    build_partition_narrative_text(old_sidx, new_sidx, kind, buf, sizeof(buf));
    display_narrative_text(buf, PARTITION_NARRATIVE_MESSAGE, false);
}

void display_partition_narrative_banner(int old_sidx, int new_sidx,
    level_partition_kind kind, bool line_delay)
{
    char buf[1024];

    build_partition_narrative_text(old_sidx, new_sidx, kind, buf, sizeof(buf));
    display_narrative_text(buf, PARTITION_NARRATIVE_BANNER, line_delay);
}

void update_labyrinth_view_state(bool handle_now)
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

static cptr partition_display_name(level_partition_kind kind)
{
    switch (kind)
    {
    case LEVEL_PART_ROOMY:
        return "Roomy";
    case LEVEL_PART_CAVEY:
        return "Cavey";
    case LEVEL_PART_RUINED:
        return "Ruined";
    case LEVEL_PART_LABYRINTH:
        return "Labyrinth";
    case LEVEL_PART_CHASM:
        return "Chasm";
    case LEVEL_PART_BIG_CAVE:
        return "Big Cave";
    default:
        return NULL;
    }
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
        return PARTITION_NARRATIVE_BANNER_DELAY;
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

void describe_greater_vault_entry(cptr vault_name)
{
    int narrative_mode = op_ptr ? op_ptr->partition_narrative_mode
                                : PARTITION_NARRATIVE_MESSAGE;
    cptr text = vault_entry_message_for_name(vault_name);

    if (!text)
        return;

    if ((narrative_mode == PARTITION_NARRATIVE_BANNER)
        || (narrative_mode == PARTITION_NARRATIVE_BANNER_DELAY))
    {
        /* Banner mode already shows the text on the main term, so push it
         * directly into recall and refresh message windows immediately. */
        queue_message_recall_only(text);
        display_narrative_text(text, narrative_mode, false);
        return;
    }

    /* Great vault entries should always land in the message log too. */
    msg_print(text);
}

void handle_partition_entry(bool force_message, int narrative_mode)
{
    if (!p_ptr || p_ptr->is_dead)
        return;

    int pi = level_partition_index_for_point(p_ptr->py, p_ptr->px);
    level_partition_kind kind = level_partition_kind_for_point(p_ptr->py, p_ptr->px);
    int sidx = styles_decode_color_style(cave_color[p_ptr->py][p_ptr->px]);

    if ((pi >= 0) && (pi != last_partition_pi) && !p_ptr->restoring)
    {
        cptr name = partition_display_name(kind);
        if (name)
        {
            char entry_message[80];
            strnfmt(entry_message, sizeof(entry_message),
                "You have entered a %s partition.", name);
            queue_message_recall_only(entry_message);
        }
    }

    /*
     * Greater vaults have their own first-entry narrative.  Their cells carry
     * vault-specific styles, so the generic partition-style banner would be
     * misleading on both first entry and later re-entry after the vault name
     * has been cleared.
     */
    if (cave_info[p_ptr->py][p_ptr->px] & CAVE_G_VAULT)
    {
        if (pi >= 0 && pi < 32)
            partition_narrated_mask |= (u32b)(1U << pi);

        last_partition_pi = pi;
        last_partition_kind = kind;
        return;
    }

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
            if ((narrative_mode == PARTITION_NARRATIVE_BANNER)
                || (narrative_mode == PARTITION_NARRATIVE_BANNER_DELAY))
                display_partition_narrative_banner(
                    last_narrated_style_idx, sidx, kind,
                    narrative_mode == PARTITION_NARRATIVE_BANNER_DELAY);
            else if (narrative_mode == PARTITION_NARRATIVE_MESSAGE)
                display_partition_narrative(last_narrated_style_idx, sidx, kind);

            if (is_small_cave_partition_kind(kind))
                msg_print("In the natural caves here, torch and lamp drink their fuel twice as fast.");

            partition_narrated_mask |= bit;
            last_narrated_style_idx = sidx;
        }
    }

    last_partition_pi = pi;
    last_partition_kind = kind;
}
