#include "angband.h"
#include "sdl-config.h"
#include "sound-config.h"
#include "sdl-sound.h"

extern struct sound_config g_sound_config;
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "log/log.h"
#include <ctype.h>
#include "h-define.h"
#include "blitz.h"
#include "metarun.h"
#include "score/score_artefact.h"
#include "score/score_guid.h"
#include "pane.h"
#include "cmd/ui/cmd-ui-internal.h"

#define MAIN_MENU_LABEL_WIDTH 21
#define MAIN_MENU_SHORTCUT_WIDTH 6

typedef struct main_menu_about_line
{
    byte attr;
    cptr text;
} main_menu_about_line;

typedef struct main_menu_about_span
{
    byte attr;
    cptr text;
} main_menu_about_span;

cptr main_menu_title(int choice)
{
    switch (choice)
    {
    case MAIN_MENU_CHARACTER: return "Character sheet";
    case MAIN_MENU_INVENTORY: return "Inventory";
    case MAIN_MENU_KNOWLEDGE: return "Known lore";
    case MAIN_MENU_HINTS_QUESTS: return "Hints & Quests";
    case MAIN_MENU_HALLS_OF_MANDOS: return "Halls of Mandos";
    case MAIN_MENU_MAP: return "Map";
    case MAIN_MENU_LOG_HISTORY: return "Log & combat history";
    case MAIN_MENU_STORY: return "The story so far";
    case MAIN_MENU_STORY_STATS: return "Story statistics";
    case MAIN_MENU_BLITZ: return "Blitz";
    case MAIN_MENU_OPTIONS: return "Options";
    case MAIN_MENU_HELP: return "Help";
    case MAIN_MENU_ABOUT: return "About";
    case MAIN_MENU_SAVE: return "Save";
    case MAIN_MENU_SAVE_QUIT:
        return death_spectator_active() ? "Quit" : "Quit with save";
    case MAIN_MENU_RETURN_GAME: return "Return to game";
    default: return "";
    }
}

int main_menu_keyboard_key(int choice)
{
    switch (choice)
    {
    case MAIN_MENU_CHARACTER: return 'c';
    case MAIN_MENU_INVENTORY: return 'i';
    case MAIN_MENU_KNOWLEDGE: return 'a';
    case MAIN_MENU_HINTS_QUESTS: return 't';
    case MAIN_MENU_HALLS_OF_MANDOS: return 'd';
    case MAIN_MENU_MAP: return 'm';
    case MAIN_MENU_LOG_HISTORY: return 'l';
    case MAIN_MENU_STORY: return 'y';
    case MAIN_MENU_STORY_STATS: return 'g';
    case MAIN_MENU_BLITZ: return 'z';
    case MAIN_MENU_OPTIONS: return 'o';
    case MAIN_MENU_HELP: return 'h';
    case MAIN_MENU_ABOUT: return 'b';
    case MAIN_MENU_SAVE: return 's';
    case MAIN_MENU_SAVE_QUIT: return 'q';
    case MAIN_MENU_RETURN_GAME: return 'r';
    default: return 0;
    }
}

static size_t main_menu_append_fixed(char* buf, size_t buflen, size_t cur,
    cptr text, int width)
{
    int len = text ? (int)strlen(text) : 0;

    if (!buf || !buflen || cur >= buflen)
        return cur;

    if (text && text[0])
    {
        size_t written = strnfmt(buf + cur, buflen - cur, "%s", text);
        cur += written;
        if (cur >= buflen)
            cur = buflen - 1;
    }

    while ((len < width) && (cur + 1 < buflen))
    {
        buf[cur++] = ' ';
        len++;
    }

    buf[cur] = '\0';
    return cur;
}

static void main_menu_append_right_aligned_shortcut(char* buf, size_t buflen,
    size_t* cur, cptr text)
{
    int len = text ? (int)strlen(text) : 0;
    int left_pad = 0;

    if (!buf || !buflen || !cur || *cur >= buflen)
        return;

    if (len < MAIN_MENU_SHORTCUT_WIDTH)
        left_pad = MAIN_MENU_SHORTCUT_WIDTH - len;

    while ((left_pad > 0) && (*cur + 1 < buflen))
    {
        buf[(*cur)++] = ' ';
        left_pad--;
    }

    if (text && text[0])
        strnfcat(buf, buflen, cur, "%s", text);

    buf[*cur] = '\0';
}

static bool main_menu_controller_binding_for_choice(int choice, int* type,
    int* id, const char** fallback)
{
    int out_type = GAMEPAD_CAPTURE_BUTTON;
    int out_id = -1;
    const char* out_fallback = "";

    switch (choice)
    {
    case MAIN_MENU_HALLS_OF_MANDOS:
        out_id = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER; /* L1 */
        out_fallback = "L1";
        break;
    case MAIN_MENU_LOG_HISTORY:
        out_id = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER; /* R1 */
        out_fallback = "R1";
        break;
    case MAIN_MENU_HINTS_QUESTS:
        out_id = SDL_GAMEPAD_BUTTON_NORTH;         /* Y */
        out_fallback = "Y";
        break;
    case MAIN_MENU_OPTIONS:
        out_id = SDL_GAMEPAD_BUTTON_BACK;          /* Back/View */
        out_fallback = "Back";
        break;
    case MAIN_MENU_SAVE_QUIT:
        out_id = SDL_GAMEPAD_BUTTON_WEST;          /* X */
        out_fallback = "X";
        break;
    default:
        return false;
    }

    if (type)
        *type = out_type;
    if (id)
        *id = out_id;
    if (fallback)
        *fallback = out_fallback;

    return true;
}

static int main_menu_controller_choice_from_key(int key)
{
    if (!steamdeck_controls_active())
        return 0;

    for (int choice = 1; choice <= MAIN_MENU_MAX; choice++)
    {
        int type = 0;
        int id = 0;
        int binding;

        if (!main_menu_controller_binding_for_choice(choice, &type, &id, NULL))
            continue;

        binding = (type == GAMEPAD_CAPTURE_TRIGGER)
            ? get_sdl_gamepad_trigger_binding(id)
            : get_sdl_gamepad_button_binding(id);

        if ((binding != GAMEPAD_BIND_NONE) && (key == binding))
            return choice;
    }

    return 0;
}

static void main_menu_controller_label(int choice, char* buf, size_t buflen)
{
    int type = 0;
    int id = 0;
    int binding;
    const char* fallback = "";

    if (!buf || !buflen)
        return;

    buf[0] = '\0';

    if (!main_menu_controller_binding_for_choice(choice, &type, &id, &fallback))
        return;

    binding = (type == GAMEPAD_CAPTURE_TRIGGER)
        ? get_sdl_gamepad_trigger_binding(id)
        : get_sdl_gamepad_button_binding(id);

    controller_prompt_label(binding, fallback, buf, buflen);
}

void main_menu_shortcut_label(int choice, char* buf, size_t buflen)
{
    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    if (steamdeck_controls_active())
    {
        main_menu_controller_label(choice, buf, buflen);
        return;
    }

    if (sdl_menu_letters_enabled())
    {
        int key = main_menu_keyboard_key(choice);

        if (key)
            strnfmt(buf, buflen, "%c", key);
    }
}

int main_menu_choice_from_key(int key)
{
    int controller_choice;

    controller_choice = main_menu_controller_choice_from_key(key);
    if (controller_choice > 0)
        return controller_choice;

    if (!sdl_menu_letters_enabled())
        return 0;

    key = tolower((unsigned char)key);
    switch (key)
    {
    case 'c': return MAIN_MENU_CHARACTER;
    case 'i': return MAIN_MENU_INVENTORY;
    case 'a': return MAIN_MENU_KNOWLEDGE;
    case 't': return MAIN_MENU_HINTS_QUESTS;
    case 'd': return MAIN_MENU_HALLS_OF_MANDOS;
    case 'm': return MAIN_MENU_MAP;
    case 'l': return MAIN_MENU_LOG_HISTORY;
    case 'y': return MAIN_MENU_STORY;
    case 'g': return MAIN_MENU_STORY_STATS;
    case 'z': return MAIN_MENU_BLITZ;
    case 'o': return MAIN_MENU_OPTIONS;
    case 'h': return MAIN_MENU_HELP;
    case 'b': return MAIN_MENU_ABOUT;
    case 's':
        if (death_spectator_active())
            return 0;
        return MAIN_MENU_SAVE;
    case 'q': return MAIN_MENU_SAVE_QUIT;
    case 'r': return MAIN_MENU_RETURN_GAME;
    default: return 0;
    }
}

static void main_menu_format_line(int choice, char* buf, size_t buflen)
{
    char label[24];
    size_t cur;

    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    cur = main_menu_append_fixed(buf, buflen, 0, main_menu_title(choice),
        MAIN_MENU_LABEL_WIDTH);

    main_menu_shortcut_label(choice, label, sizeof(label));
    if (steamdeck_controls_active())
    {
        if (label[0])
        {
            char shortcut[32];
            strnfmt(shortcut, sizeof(shortcut), "[%s]", label);
            main_menu_append_right_aligned_shortcut(buf, buflen, &cur, shortcut);
        }
        else
            main_menu_append_right_aligned_shortcut(buf, buflen, &cur, "");
    }
    else if (label[0])
    {
        strnfcat(buf, buflen, &cur, "(%s)", label);
    }
    else
        main_menu_append_right_aligned_shortcut(buf, buflen, &cur, "");
}

static void main_menu_format_controller_prompt(char* buf, size_t buflen)
{
    char confirm_label[16];
    char back_label[16];

    if (!buf || !buflen)
        return;

    buf[0] = '\0';
    controller_prompt_label(steamdeck_confirm_key(), "A",
        confirm_label, sizeof(confirm_label));
    controller_prompt_label(steamdeck_back_key(), "B",
        back_label, sizeof(back_label));
    strnfmt(buf, buflen, "D-pad select  %s open  %s back",
        confirm_label, back_label);
}

static int main_menu_calc_width(void)
{
    int max_w = 0;
    for (int i = 1; i <= MAIN_MENU_MAX; i++)
    {
        char line[80];
        int w;

        main_menu_format_line(i, line, sizeof(line));
        w = (int)strlen(line);
        if (w > max_w)
            max_w = w;
    }
    if (steamdeck_controls_active())
    {
        char prompt[96];
        int w;

        main_menu_format_controller_prompt(prompt, sizeof(prompt));
        w = (int)strlen(prompt);
        if (w > max_w)
            max_w = w;
    }
    return max_w;
}

bool main_menu_choice_is_disabled(int choice)
{
    return (choice == MAIN_MENU_SAVE);
}

static int main_menu_about_count_rows(int indent, int wrap_right,
    const main_menu_about_line* lines, const bool* blank_visible)
{
    int total = 0;

    for (int i = 0; lines[i].text; i++)
    {
        if (!lines[i].text[0])
        {
            if (!blank_visible || blank_visible[i])
                total++;
        }
        else
            total += count_wrapped_lines(lines[i].text, wrap_right, indent);
    }

    return total;
}

static bool main_menu_about_drop_bottom_blank(bool* blank_visible,
    const main_menu_about_line* lines, int line_count)
{
    for (int i = line_count - 1; i >= 0; i--)
    {
        if (!lines[i].text[0] && blank_visible[i])
        {
            blank_visible[i] = false;
            return true;
        }
    }

    return false;
}

static void main_menu_about_draw_line(int row, int indent, int wrap_right,
    byte attr, cptr text)
{
    int old_wrap = text_out_wrap;
    int old_indent = text_out_indent;

    text_out_wrap = wrap_right;
    text_out_indent = indent;
    Term_gotoxy(indent, row);
    text_out_to_screen(attr, text ? text : "");
    text_out_wrap = old_wrap;
    text_out_indent = old_indent;
}

static void main_menu_about_draw_spans(int row, int indent, int wrap_right,
    const main_menu_about_span* spans, int span_count)
{
    int old_wrap = text_out_wrap;
    int old_indent = text_out_indent;

    text_out_wrap = wrap_right;
    text_out_indent = indent;
    Term_gotoxy(indent, row);
    for (int i = 0; i < span_count; i++)
        text_out_to_screen(spans[i].attr, spans[i].text);
    text_out_wrap = old_wrap;
    text_out_indent = old_indent;
}

static void main_menu_about(void)
{
    int wid, hgt;
    int menu_w;
    int box_w;
    int box_left;
    int text_indent;
    int wrap_right;
    int body_rows;
    int row_top;
    int row;
    char ch;
    bool saved_hide_cursor;
    static const main_menu_about_line about_lines[] = {
        { TERM_WHITE, "Sil-More is an evolution of SilQ, a famous roguelike" },
        { TERM_WHITE, "taking place in the First Age of Beleriand." },
        { TERM_WHITE, "" },
        { TERM_WHITE, "Developers: k0rtess and sinefabula." },
        { TERM_WHITE, "Gamedesigner: k0rtess." },
        { TERM_WHITE, "Tileset: MicroChasm." },
        { TERM_WHITE, "Main music theme: sinefabula." },
        { TERM_WHITE, "Ambient music theme: West Wind." },
        { TERM_WHITE, "Logo: sinefabula." },
        { TERM_WHITE, "" },
        { TERM_WHITE, "Our love to Maedhros aka Carcharos for playing so much," },
        { TERM_WHITE, "finding those pescy bugs and giving cool ideas." },
        { TERM_L_BLUE, "Special thanks to original Sil and SilQ" },
        { TERM_L_BLUE, "developers: half, Scatha and Quirk." },
        { TERM_WHITE, "" },
        { TERM_WHITE, "Honorable mentions:" },
        { TERM_WHITE, "Sound: Kenney, qubodup, TomMusic, LeoHPaz." },
        { TERM_WHITE, "Walls: Wolffius, Pine Druid, Backterria, Ninjikin." },
        { TERM_WHITE, "" },
        { TERM_L_RED, "And our deep love to Tolkien and his timeless creations." },
        { TERM_WHITE, "" },
        { 0, NULL }
    };

    if (p_ptr && p_ptr->playing)
        sdl_music_play_death();

    screen_save();
    screen_push_supporting_panes_hidden();
    sdl_push_terminal_menu_scale();

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    menu_w = main_menu_calc_width();
    box_w = MIN(MAX(menu_w + 24, 68), 76);
    if (box_w > (wid > 2 ? wid - 2 : wid))
        box_w = (wid > 2) ? (wid - 2) : wid;
    if (box_w < 1)
        box_w = 1;

    box_left = (wid - box_w) / 2;
    if (box_left < 0)
        box_left = 0;

    text_indent = box_left + 2;
    wrap_right = box_left + box_w - 1;

    Term_clear();

    {
        int line_count = 0;
        bool blank_visible[sizeof(about_lines) / sizeof(about_lines[0])] = { false };
        int max_body_rows;

        while (about_lines[line_count].text)
            line_count++;

        for (int i = 0; i < line_count; i++)
            blank_visible[i] = true;

        body_rows = main_menu_about_count_rows(text_indent, wrap_right,
            about_lines, blank_visible);

        max_body_rows = (hgt > 2) ? (hgt - 2) : 0;
        while ((body_rows > max_body_rows)
            && main_menu_about_drop_bottom_blank(blank_visible, about_lines,
                line_count))
        {
            body_rows -= 1;
        }

        row_top = (hgt > body_rows + 2) ? 1 : 0;

        {
            int panel_h = body_rows + 2;
            if (panel_h > hgt - row_top)
                panel_h = hgt - row_top;
            for (int i = 0; i < panel_h; i++)
            {
                int y = row_top + i;
                if (y >= 0 && y < hgt)
                    Term_erase(box_left, y, box_w);
            }
        }

        {
            cptr title = "About Sil-More";
            int title_x = box_left + MAX((box_w - (int)strlen(title)) / 2 - 2, 0);
            Term_putstr(title_x, row_top, -1, TERM_YELLOW, title);
        }

        row = row_top + 1;
        for (int i = 0; i < line_count; i++)
        {
            cptr text = about_lines[i].text;

            if (!text[0])
            {
                if (blank_visible[i])
                    row++;
                continue;
            }

            main_menu_about_draw_line(row, text_indent, wrap_right,
                about_lines[i].attr, text);

            if (i == 0)
            {
                static const main_menu_about_span intro_label_spans[] = {
                    { TERM_VIOLET, "Sil-More" },
                    { TERM_WHITE, " is an evolution of " },
                    { TERM_L_BLUE, "SilQ" },
                    { TERM_WHITE, ", a famous roguelike" },
                };
                main_menu_about_draw_spans(row, text_indent, wrap_right,
                    intro_label_spans,
                    (int)(sizeof(intro_label_spans)
                        / sizeof(intro_label_spans[0])));
            }
            else if ((i >= 3) && (i <= 8))
            {
                static const main_menu_about_span label_spans[][2] = {
                    {
                        { TERM_YELLOW, "Developers:" },
                        { TERM_WHITE, " k0rtess and sinefabula." },
                    },
                    {
                        { TERM_YELLOW, "Gamedesigner:" },
                        { TERM_WHITE, " k0rtess." },
                    },
                    {
                        { TERM_YELLOW, "Tileset:" },
                        { TERM_WHITE, " MicroChasm." },
                    },
                    {
                        { TERM_YELLOW, "Main music theme:" },
                        { TERM_WHITE, " sinefabula." },
                    },
                    {
                        { TERM_YELLOW, "Ambient music theme:" },
                        { TERM_WHITE, " West Wind." },
                    },
                    {
                        { TERM_YELLOW, "Logo:" },
                        { TERM_WHITE, " sinefabula." },
                    },
                };
                int label_index = i - 3;
                main_menu_about_draw_spans(row, text_indent, wrap_right,
                    label_spans[label_index], 2);
            }
            else if (i == 15)
            {
                static const main_menu_about_span mentions_spans[] = {
                    { TERM_YELLOW, "Honorable mentions:" },
                };
                main_menu_about_draw_spans(row, text_indent, wrap_right,
                    mentions_spans, 1);
            }

            row += count_wrapped_lines(text, wrap_right, text_indent);
        }
    }

    if (row >= hgt)
        row = hgt - 1;

    if (steamdeck_controls_active())
    {
        char back_label[16];
        char prompt_buf[48];

        controller_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));
        strnfmt(prompt_buf, sizeof(prompt_buf), "[%s] return", back_label);
        Term_putstr(text_indent, row, -1, TERM_L_WHITE, prompt_buf);
    }
    else
    {
        Term_putstr(text_indent, row, -1, TERM_L_WHITE,
            "[Press any key to return]");
    }
    Term_fresh();

    flush();
    saved_hide_cursor = hide_cursor;
    hide_cursor = true;
    ch = inkey();
    hide_cursor = saved_hide_cursor;
    (void)ch;

    sdl_pop_terminal_menu_scale();
    screen_pop_supporting_panes_hidden();
    screen_load();

    if (p_ptr && p_ptr->playing)
        sdl_music_stop_main();
}

static bool do_cmd_hint_messages(bool* out_pending_look, int* out_look_y,
    int* out_look_x, bool* out_pending_map, int* out_map_y,
    int* out_map_x, bool manage_screen);

static void do_cmd_log_history_menu(void)
{
    do_cmd_messages_with_filter(LOG_HISTORY_FILTER_ALL);
}

enum {
    LOG_HISTORY_CLICK_FILTER_ALL = -20001,
    LOG_HISTORY_CLICK_FILTER_MESSAGES = -20002,
    LOG_HISTORY_CLICK_FILTER_COMBAT = -20003,
    LOG_HISTORY_CLICK_FILTER_NOTES = -20004
};

typedef enum log_history_entry_kind
{
    LOG_HISTORY_ENTRY_MESSAGE,
    LOG_HISTORY_ENTRY_COMBAT
} log_history_entry_kind;

typedef struct log_history_entry
{
    log_history_entry_kind kind;
    u32b sequence;
    int tie_breaker;
    s16b message_age;
    int history_idx;
    int roll_idx;
    combat_roll* roll;
} log_history_entry;

#define LOG_HISTORY_MAX_ENTRIES \
    (MESSAGE_MAX + ((MAX_COMBAT_HISTORY + 1) * MAX_COMBAT_ROLLS))

static log_history_entry log_history_entries[LOG_HISTORY_MAX_ENTRIES];

static int log_history_clamp_filter(int filter)
{
    if ((filter < LOG_HISTORY_FILTER_ALL)
        || (filter > LOG_HISTORY_FILTER_NOTES))
    {
        return LOG_HISTORY_FILTER_ALL;
    }

    return filter;
}

static cptr log_history_filter_label(int filter)
{
    switch (filter)
    {
    case LOG_HISTORY_FILTER_MESSAGES:
        return "Log";
    case LOG_HISTORY_FILTER_COMBAT:
        return "Combat";
    case LOG_HISTORY_FILTER_NOTES:
        return "Notes";
    default:
        return "All";
    }
}

static bool log_history_filter_includes_messages(int filter)
{
    return (filter == LOG_HISTORY_FILTER_ALL)
        || (filter == LOG_HISTORY_FILTER_MESSAGES);
}

static bool log_history_filter_includes_combat(int filter)
{
    return (filter == LOG_HISTORY_FILTER_ALL)
        || (filter == LOG_HISTORY_FILTER_COMBAT);
}

static int log_history_click_to_filter(int click_choice)
{
    switch (click_choice)
    {
    case LOG_HISTORY_CLICK_FILTER_MESSAGES:
        return LOG_HISTORY_FILTER_MESSAGES;
    case LOG_HISTORY_CLICK_FILTER_COMBAT:
        return LOG_HISTORY_FILTER_COMBAT;
    case LOG_HISTORY_CLICK_FILTER_NOTES:
        return LOG_HISTORY_FILTER_NOTES;
    default:
        return LOG_HISTORY_FILTER_ALL;
    }
}

static int log_history_filter_to_click(int filter)
{
    switch (filter)
    {
    case LOG_HISTORY_FILTER_MESSAGES:
        return LOG_HISTORY_CLICK_FILTER_MESSAGES;
    case LOG_HISTORY_FILTER_COMBAT:
        return LOG_HISTORY_CLICK_FILTER_COMBAT;
    case LOG_HISTORY_FILTER_NOTES:
        return LOG_HISTORY_CLICK_FILTER_NOTES;
    default:
        return LOG_HISTORY_CLICK_FILTER_ALL;
    }
}

static int log_history_next_filter(int filter)
{
    switch (filter)
    {
    case LOG_HISTORY_FILTER_ALL:
        return LOG_HISTORY_FILTER_MESSAGES;
    case LOG_HISTORY_FILTER_MESSAGES:
        return LOG_HISTORY_FILTER_COMBAT;
    case LOG_HISTORY_FILTER_COMBAT:
        return LOG_HISTORY_FILTER_NOTES;
    default:
        return LOG_HISTORY_FILTER_ALL;
    }
}

static int log_history_entry_compare_newest_first(const void* a, const void* b)
{
    const log_history_entry* entry_a = (const log_history_entry*)a;
    const log_history_entry* entry_b = (const log_history_entry*)b;

    if (entry_a->sequence < entry_b->sequence)
        return 1;
    if (entry_a->sequence > entry_b->sequence)
        return -1;

    if (entry_a->tie_breaker < entry_b->tie_breaker)
        return 1;
    if (entry_a->tie_breaker > entry_b->tie_breaker)
        return -1;

    return (int)entry_a->kind - (int)entry_b->kind;
}

static int log_history_collect_entries(log_history_entry* entries,
    int max_entries, int filter)
{
    int count = 0;

    if (log_history_filter_includes_messages(filter))
    {
        s16b num = message_num();

        for (s16b age = 0; (age < num) && (count < max_entries); age++)
        {
            entries[count].kind = LOG_HISTORY_ENTRY_MESSAGE;
            entries[count].sequence = message_sequence(age);
            if (entries[count].sequence == 0)
                entries[count].sequence = (u32b)(num - age);
            entries[count].tie_breaker = num - age;
            entries[count].message_age = age;
            entries[count].history_idx = -1;
            entries[count].roll_idx = -1;
            entries[count].roll = NULL;
            count++;
        }
    }

    if (log_history_filter_includes_combat(filter))
    {
        for (int r = 0; (r < combat_number) && (count < max_entries)
             && (r < MAX_COMBAT_ROLLS); r++)
        {
            combat_roll* roll = &combat_rolls[0][r];

            if (roll->att_type == COMBAT_ROLL_NONE)
                continue;

            entries[count].kind = LOG_HISTORY_ENTRY_COMBAT;
            entries[count].sequence = roll->sequence;
            if (entries[count].sequence == 0)
                entries[count].sequence = (u32b)turn;
            entries[count].tie_breaker = r;
            entries[count].message_age = -1;
            entries[count].history_idx = -1;
            entries[count].roll_idx = r;
            entries[count].roll = roll;
            count++;
        }

        for (int h = 0; (h < combat_history_count) && (count < max_entries);
             h++)
        {
            int hist_idx =
                (combat_history_head - h + MAX_COMBAT_HISTORY)
                % MAX_COMBAT_HISTORY;
            combat_history_round* round = &combat_history[hist_idx];
            int rolls = round->num_rolls;

            if (rolls > MAX_COMBAT_ROLLS)
                rolls = MAX_COMBAT_ROLLS;

            for (int r = 0; (r < rolls) && (count < max_entries); r++)
            {
                combat_roll* roll = &round->rolls[r];

                if (roll->att_type == COMBAT_ROLL_NONE)
                    continue;

                entries[count].kind = LOG_HISTORY_ENTRY_COMBAT;
                entries[count].sequence = roll->sequence;
                if (entries[count].sequence == 0)
                    entries[count].sequence = (u32b)round->turn_count;
                entries[count].tie_breaker = r;
                entries[count].message_age = -1;
                entries[count].history_idx = hist_idx;
                entries[count].roll_idx = r;
                entries[count].roll = roll;
                count++;
            }
        }
    }

    qsort(entries, count, sizeof(log_history_entry),
        log_history_entry_compare_newest_first);

    return count;
}

typedef struct log_history_note_line
{
    const char* text;
    int len;
} log_history_note_line;

#define LOG_HISTORY_MAX_NOTE_LINES 4096

static log_history_note_line log_history_note_lines[LOG_HISTORY_MAX_NOTE_LINES];

/*
 * Split the player's notes file into displayable lines in chronological order
 * (the character header first, then notes oldest-to-newest).  Unlike the
 * message/combat log this is read top-down like a document, so the lines are
 * kept in buffer order.  They point straight into notes_buffer, which is only
 * mutated when a note is added (never while this viewer is open).
 */
static int log_history_collect_notes(void)
{
    int count = 0;
    const char* line = notes_buffer;
    const char* p = notes_buffer;

    for (;; p++)
    {
        if (*p == '\n' || *p == '\0')
        {
            int len = (int)(p - line);

            if (len > 0 && count < LOG_HISTORY_MAX_NOTE_LINES)
            {
                log_history_note_lines[count].text = line;
                log_history_note_lines[count].len = len;
                count++;
            }
            if (*p == '\0')
                break;
            line = p + 1;
        }
    }

    return count;
}

static int log_history_draw_filter(int row, int col, cptr label, int filter,
    int active_filter, int hover_filter)
{
    char token[32];
    int click_choice = log_history_filter_to_click(filter);
    byte attr = TERM_YELLOW;

    if (filter == active_filter)
        strnfmt(token, sizeof(token), "[%s]", label);
    else
        strnfmt(token, sizeof(token), " %s ", label);

    if (hover_filter == filter)
        attr = TERM_YELLOW + TERM_SHADE;

    Term_putstr(col, row, -1, attr, token);
    ui_menu_click_add(click_choice, col, row, (int)strlen(token));

    return col + (int)strlen(token) + 1;
}

static void log_history_draw_filters(int active_filter, int hover_filter,
    int term_wid)
{
    int col = 0;

    if (term_wid < 1)
        term_wid = 80;

    Term_putstr(0, 0, term_wid, TERM_L_WHITE + TERM_SHADE, "Logs");
    Term_erase(0, 1, 255);

    Term_putstr(0, 1, -1, TERM_WHITE, "Filter: ");
    col = 8;
    col = log_history_draw_filter(1, col, "All", LOG_HISTORY_FILTER_ALL,
        active_filter, hover_filter);
    col = log_history_draw_filter(1, col, "Log",
        LOG_HISTORY_FILTER_MESSAGES, active_filter, hover_filter);
    col = log_history_draw_filter(1, col, "Combat",
        LOG_HISTORY_FILTER_COMBAT, active_filter, hover_filter);
    (void)log_history_draw_filter(1, col, "Notes",
        LOG_HISTORY_FILTER_NOTES, active_filter, hover_filter);
}

static bool log_history_filter_key(char ch)
{
    return (ch == 'e') || (ch == 'E') || (ch == 'i') || (ch == 'I')
        || (ch == '\t') || (ch == 'f') || (ch == 'F');
}

static void log_history_putstr_scrolled(int* col, int row, int offset,
    byte attr, cptr text)
{
    int len = (int)strlen(text);
    int draw_col = *col - offset;
    int text_offset = 0;

    if (draw_col < 0)
    {
        text_offset = -draw_col;
        draw_col = 0;
    }

    if (text_offset < len)
        Term_putstr(draw_col, row, -1, attr, text + text_offset);

    *col += len;
}

static void log_history_put_tile_scrolled(int* col, int row, int offset,
    byte attr, char chr)
{
    int draw_col = *col - offset;

    if (draw_col >= 0)
    {
        Term_queue_char(draw_col, row, attr, chr, 0, 0);
        if (use_bigtile && !graphics_are_ascii())
        {
            if ((attr & 0x80) && ((byte)chr & 0x80))
                Term_queue_char(draw_col + 1, row, 255, -1, 0, 0);
            else
                Term_queue_char(draw_col + 1, row, TERM_WHITE, ' ', 0, 0);
        }
    }

    *col += 1;
    if (use_bigtile && !graphics_are_ascii())
        *col += 1;
}

static void log_history_format_damage_dice(const combat_roll* roll,
    char* buf, size_t buflen)
{
    bool first = roll->dd > 0 && roll->ds > 0;
    bool second = roll->dd2 > 0 && roll->ds2 > 0;

    if (first && second)
        strnfmt(buf, buflen, "(%dd%d+%dd%d)", roll->dd, roll->ds,
            roll->dd2, roll->ds2);
    else if (second)
        strnfmt(buf, buflen, "(%dd%d)", roll->dd2, roll->ds2);
    else
        strnfmt(buf, buflen, "(%dd%d)", roll->dd, roll->ds);
}

static void log_history_draw_combat_entry(const log_history_entry* entry,
    int row, int offset)
{
    combat_roll* roll = entry->roll;
    int a_att, a_evn, a_hit, a_dam_roll, a_prot_roll, a_net_dam;
    int col = 0;
    char buf[120];
    bool is_player_attack = roll->is_attacker_player;

    if (is_player_attack)
    {
        a_att = TERM_L_BLUE;
        a_evn = TERM_WHITE;
        a_hit = TERM_L_RED;
        a_dam_roll = TERM_L_BLUE;
        a_net_dam = TERM_L_RED;
        if (roll->prt_percent >= 100)
            a_prot_roll = TERM_WHITE;
        else if (roll->prt_percent >= 1)
            a_prot_roll = TERM_SLATE;
        else
            a_prot_roll = TERM_DARK;
    }
    else
    {
        a_att = TERM_WHITE;
        a_evn = TERM_L_BLUE;
        a_hit = TERM_L_RED;
        a_dam_roll = TERM_WHITE;
        a_net_dam = TERM_L_RED;
        if (roll->prt_percent >= 100)
            a_prot_roll = TERM_L_BLUE;
        else if (roll->prt_percent >= 1)
            a_prot_roll = TERM_BLUE;
        else
            a_prot_roll = TERM_DARK;
    }

    log_history_putstr_scrolled(&col, row, offset, TERM_WHITE, " ");
    log_history_put_tile_scrolled(&col, row, offset, roll->attacker_attr,
        roll->attacker_char);

    if (roll->att_type == COMBAT_ROLL_ROLL)
    {
        int net_att;

        if (roll->att < 10)
            strnfmt(buf, sizeof(buf), "  (%+d)", roll->att);
        else
            strnfmt(buf, sizeof(buf), " (%+d)", roll->att);
        log_history_putstr_scrolled(&col, row, offset, a_att, buf);

        strnfmt(buf, sizeof(buf), "%4d", roll->att + roll->att_roll);
        log_history_putstr_scrolled(&col, row, offset, a_att, buf);

        net_att = roll->att_roll + roll->att - roll->evn_roll - roll->evn;
        if (net_att > 0)
            strnfmt(buf, sizeof(buf), "%4d", net_att);
        else
            SDL_strlcpy(buf, "   -", sizeof(buf));
        log_history_putstr_scrolled(&col, row, offset,
            (net_att > 0) ? a_hit : TERM_SLATE, buf);

        strnfmt(buf, sizeof(buf), "%4d", roll->evn + roll->evn_roll);
        log_history_putstr_scrolled(&col, row, offset, a_evn, buf);

        if (roll->evn < 10)
            strnfmt(buf, sizeof(buf), "   [%+d]", roll->evn);
        else
            strnfmt(buf, sizeof(buf), "  [%+d]", roll->evn);
        log_history_putstr_scrolled(&col, row, offset, a_evn, buf);

        log_history_putstr_scrolled(&col, row, offset, TERM_WHITE, " ");
        log_history_put_tile_scrolled(&col, row, offset, roll->defender_attr,
            roll->defender_char);

        if (!roll->no_damage && (net_att > 0 || roll->force_damage))
        {
            int net_dam = roll->dam - roll->prot;
            if (net_dam < 0)
                net_dam = 0;

            log_history_putstr_scrolled(&col, row, offset, TERM_L_DARK,
                "  ->");

            log_history_format_damage_dice(roll, buf, sizeof(buf));
            log_history_putstr_scrolled(&col, row, offset, a_dam_roll, buf);

            strnfmt(buf, sizeof(buf), "%4d", roll->dam);
            log_history_putstr_scrolled(&col, row, offset, a_dam_roll, buf);

            if (net_dam > 0)
                strnfmt(buf, sizeof(buf), "%4d", net_dam);
            else
                SDL_strlcpy(buf, "   -", sizeof(buf));
            log_history_putstr_scrolled(&col, row, offset,
                (net_dam > 0) ? a_net_dam : TERM_SLATE, buf);

            strnfmt(buf, sizeof(buf), "%4d", roll->prot);
            log_history_putstr_scrolled(&col, row, offset, a_prot_roll, buf);
        }
    }
    else if (roll->att_type == COMBAT_ROLL_AUTO)
    {
        int net_dam = roll->dam - roll->prot;
        if (net_dam < 0)
            net_dam = 0;

        log_history_putstr_scrolled(&col, row, offset, TERM_L_DARK,
            "                         ");
        log_history_putstr_scrolled(&col, row, offset, TERM_WHITE, " ");
        log_history_put_tile_scrolled(&col, row, offset, roll->defender_attr,
            roll->defender_char);
        log_history_putstr_scrolled(&col, row, offset, TERM_L_DARK, "  ->");

        if (roll->ds < 10)
            strnfmt(buf, sizeof(buf), "   (%dd%d)", roll->dd, roll->ds);
        else
            strnfmt(buf, sizeof(buf), "  (%dd%d)", roll->dd, roll->ds);
        log_history_putstr_scrolled(&col, row, offset, a_dam_roll, buf);

        strnfmt(buf, sizeof(buf), "%4d", roll->dam);
        log_history_putstr_scrolled(&col, row, offset, a_dam_roll, buf);

        if (net_dam > 0)
            strnfmt(buf, sizeof(buf), "%4d", net_dam);
        else
            SDL_strlcpy(buf, "   -", sizeof(buf));
        log_history_putstr_scrolled(&col, row, offset,
            (net_dam > 0) ? a_net_dam : TERM_SLATE, buf);

        strnfmt(buf, sizeof(buf), "%4d", roll->prot);
        log_history_putstr_scrolled(&col, row, offset, a_prot_roll, buf);
    }
}

static void log_history_entry_search_text(const log_history_entry* entry,
    char* out, size_t out_sz)
{
    if (entry->kind == LOG_HISTORY_ENTRY_MESSAGE)
    {
        SDL_strlcpy(out, message_str(entry->message_age), out_sz);
    }
    else
    {
        combat_roll* roll = entry->roll;
        int net_att = roll->att_roll + roll->att - roll->evn_roll - roll->evn;
        int net_dam = roll->dam - roll->prot;
        char dice[48];

        if (net_dam < 0)
            net_dam = 0;

        log_history_format_damage_dice(roll, dice, sizeof(dice));
        strnfmt(out, out_sz,
            "Combat %c to %c att %+d hit %d evn %+d damage %s net %d "
            "protection %d",
            roll->attacker_char, roll->defender_char, roll->att, net_att,
            roll->evn, dice, net_dam, roll->prot);
    }
}

enum {
    HINT_QUEST_CLICK_HINTS_TAB = -20101,
    HINT_QUEST_CLICK_QUESTS_TAB = -20102
};

static int hint_quest_draw_tab(int row, int col, cptr label, bool active,
    bool hovered, int click_choice)
{
    char tab[32];
    byte attr = hovered ? TERM_YELLOW + TERM_SHADE : TERM_YELLOW;

    strnfmt(tab, sizeof(tab), active ? "[%s]" : " %s ", label);
    Term_putstr(col, row, -1, attr, tab);
    ui_menu_click_add(click_choice, col, row, (int)strlen(tab));
    return col + (int)strlen(tab) + 1;
}

static void hint_quest_draw_tabs(bool quest_active, int hover_tab, int term_wid)
{
    int col = 0;

    if (term_wid < 1)
        term_wid = 80;

    Term_putstr(0, 0, term_wid, TERM_L_WHITE + TERM_SHADE,
        "Hints & Quests");
    Term_erase(0, 1, 255);

    col = hint_quest_draw_tab(1, col, "Hints", !quest_active,
        hover_tab == HINT_QUEST_CLICK_HINTS_TAB,
        HINT_QUEST_CLICK_HINTS_TAB);
    (void)hint_quest_draw_tab(1, col, "Quests", quest_active,
        hover_tab == HINT_QUEST_CLICK_QUESTS_TAB,
        HINT_QUEST_CLICK_QUESTS_TAB);
}

static bool hint_quest_tab_key(char ch)
{
    return (ch == '\t');
}

static bool hint_quest_handle_tab_navigation(char ch, bool* tabs_focus,
    bool can_focus_tabs, bool* switch_tabs)
{
    int d = target_dir(ch);

    if (!tabs_focus || !switch_tabs)
        return false;

    if (!*tabs_focus)
    {
        if (can_focus_tabs && d && !ddx[d] && (ddy[d] < 0))
        {
            *tabs_focus = true;
            return true;
        }

        return false;
    }

    if (d)
    {
        if (ddx[d])
        {
            *switch_tabs = true;
            return true;
        }
        if (ddy[d] > 0)
        {
            *tabs_focus = false;
            return true;
        }
        if (ddy[d] < 0)
            return true;
    }

    return false;
}

static void do_cmd_hint_quest_menu(bool* out_pending_look, int* out_look_y,
    int* out_look_x, bool* out_pending_map, int* out_map_y,
    int* out_map_x)
{
    bool quest_tab = false;
    bool pending_look = false;
    int look_y = -1;
    int look_x = -1;
    bool pending_map = false;
    int map_y = -1;
    int map_x = -1;

    screen_save();
    screen_push_supporting_panes_hidden();
    sdl_push_terminal_menu_scale();

    while (true)
    {
        if (quest_tab)
        {
            if (!do_cmd_quest_status_tabs_in_place())
                break;
            quest_tab = false;
            continue;
        }

        if (!do_cmd_hint_messages(&pending_look, &look_y, &look_x,
                &pending_map, &map_y, &map_x, false))
            break;

        if (pending_look || pending_map)
            break;

        quest_tab = true;
    }

    sdl_pop_terminal_menu_scale();
    screen_pop_supporting_panes_hidden();
    screen_load();

    if (out_pending_look)
        *out_pending_look = pending_look;
    if (out_look_y)
        *out_look_y = look_y;
    if (out_look_x)
        *out_look_x = look_x;
    if (out_pending_map)
        *out_pending_map = pending_map;
    if (out_map_y)
        *out_map_y = map_y;
    if (out_map_x)
        *out_map_x = map_x;
}

/*
 * Explain Blitz mode, then (on confirmation) save the current story game and
 * arm a relaunch into Blitz.  An existing living Blitz character is resumed;
 * otherwise character creation starts a new run.  The current play_game()
 * session is ended via quit_to_menu so the engine comes back up in Blitz mode
 * (see blitz_launch_requested() handling in initial_menu() and close_game()).
 */
static void main_menu_blitz_text_line(int* row, int wrap, byte attr, cptr text)
{
    Term_gotoxy(2, *row);
    text_out_c(attr, text);
    *row += count_wrapped_lines(text, wrap, 2) + 1;
}

static void do_cmd_start_blitz(void)
{
    int wid = 80;
    int hgt = 24;
    int row = 2;
    int wrap;

    if (run_mode_is_blitz())
    {
        msg_print("You are already playing a Blitz run.");
        return;
    }

    if (!p_ptr || !p_ptr->playing || p_ptr->is_dead
        || death_spectator_active())
    {
        msg_print("You cannot start a Blitz run right now.");
        return;
    }

    screen_save();
    /* Hide panes BEFORE pushing the menu scale so get_sdl_terminal_menu_scale()
     * measures the full screen and lands on max-1, matching the inventory/supply
     * menu rather than the smaller window-mode scale. */
    screen_push_supporting_panes_hidden();
    screen_push_touch_pane_hidden();
    sdl_push_terminal_menu_scale();

    Term_clear();
    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    wrap = MAX(20, wid - 4);
    c_put_str(TERM_YELLOW, "Blitz Mode", row, MAX((wid - 10) / 2, 0));
    row += 2;

    text_out_hook = text_out_to_screen;
    text_out_wrap = wrap;
    text_out_indent = 2;

    main_menu_blitz_text_line(&row, wrap, TERM_L_WHITE,
        "Blitz is a self-contained run, played entirely apart from your "
        "story.");
    main_menu_blitz_text_line(&row, wrap, TERM_WHITE,
        "Use it to play outside of your metaprogress, or to practise freely: "
        "nothing you do in Blitz touches your normal metarun, saves or score.");
    main_menu_blitz_text_line(&row, wrap, TERM_WHITE,
        "Blitz keeps its own separate character and score files, so your story "
        "character stays safe and can be resumed at any time.");
    main_menu_blitz_text_line(&row, wrap, TERM_WHITE,
        "If you already have a living Blitz character, it will be resumed.");
    main_menu_blitz_text_line(&row, wrap, TERM_SLATE,
        "Switching to Blitz will save your current story game first.");

    Term_fresh();

    if (!get_check_lower("Save your story game and switch to Blitz now? "))
    {
        sdl_pop_terminal_menu_scale();
        screen_pop_touch_pane_hidden();
        screen_pop_supporting_panes_hidden();
        screen_load();
        return;
    }

    sdl_pop_terminal_menu_scale();
    screen_pop_touch_pane_hidden();
    screen_pop_supporting_panes_hidden();
    screen_load();

    /* Persist the story game before leaving its session. */
    save_game_quietly = true;
    do_cmd_save_game();

    /* Arm the Blitz relaunch and end the current (story) session so the game
     * comes back up directly in Blitz mode. */
    blitz_request_launch();
    p_ptr->quit_to_menu = true;
    p_ptr->playing = false;
    p_ptr->leaving = true;
    (void)Term_flush();
}

static bool do_cmd_main_menu_execute_choice_impl(int actiontype,
    bool* pending_hint_look, int* pending_hint_look_y,
    int* pending_hint_look_x, bool* pending_hint_map,
    int* pending_hint_map_y, int* pending_hint_map_x)
{
    switch (actiontype)
    {
    case MAIN_MENU_CHARACTER: // Character sheet (c)
    {
        do_cmd_character_sheet();
        return true;
    }
    case MAIN_MENU_INVENTORY: // Inventory (i)
    {
        do_cmd_inven_direct();
        return true;
    }
    case MAIN_MENU_KNOWLEDGE: // Known lore (a)
    {
        do_cmd_knowledge_browser_page(g_knowledge_last_page);
        return true;
    }
    case MAIN_MENU_HINTS_QUESTS: // Hints & Quests (t)
    {
        do_cmd_hint_quest_menu(pending_hint_look, pending_hint_look_y,
            pending_hint_look_x, pending_hint_map, pending_hint_map_y,
            pending_hint_map_x);
        return true;
    }
    case MAIN_MENU_HALLS_OF_MANDOS: // Halls of Mandos (d)
    {
        log_info("main menu: opening Halls of Mandos view");
        screen_save();
        screen_push_supporting_panes_hidden();
        sdl_push_terminal_menu_scale();
        show_scores_interactive(true);
        sdl_pop_terminal_menu_scale();
        screen_pop_supporting_panes_hidden();
        screen_load();
        return true;
    }
    case MAIN_MENU_MAP: // Map (m)
    {
        do_cmd_view_map();
        return true;
    }
    case MAIN_MENU_LOG_HISTORY: // Log & combat history (l)
    {
        do_cmd_log_history_menu();
        return true;
    }
    case MAIN_MENU_STORY: // The story so far (y)
    {
        /* Save screen before showing story */
        screen_save();
        print_story(15, 1);
        /* Load screen after story */
        screen_load();
        return true;
    }
    case MAIN_MENU_STORY_STATS: // Story statistics (g)
    {
        print_metarun_stats();
        return true;
    }
    case MAIN_MENU_BLITZ: // Blitz (z)
    {
        do_cmd_start_blitz();
        return true;
    }
    case MAIN_MENU_OPTIONS: // Options (o)
    {
        do_cmd_options();
        return true;
    }
    case MAIN_MENU_HELP: // Help (h)
    {
        do_cmd_help();
        return true;
    }
    case MAIN_MENU_ABOUT: // About (b)
    {
        main_menu_about();
        return true;
    }
    case MAIN_MENU_SAVE: // Save (s)
    {
        do_cmd_save_game();
        return true;
    }
    case MAIN_MENU_SAVE_QUIT: // Quit with save (q)
    {
        if (death_spectator_active())
        {
            log_info("main menu: death spectator quit requested");
            death_spectator_request_exit();
            return true;
        }

        log_info("main menu: quit with save requested (playing=%d leaving=%d icky=%d)",
            p_ptr->playing ? 1 : 0, p_ptr->leaving ? 1 : 0,
            character_icky);

        /* Exit the application after the save/score screen. */
        p_ptr->quit_to_menu = false;

        /* Stop accepting normal gameplay/menu input while the quit completes. */
        p_ptr->leaving = true;
        p_ptr->playing = false;
        log_debug("main menu: quit state armed before save (playing=%d leaving=%d quit_to_menu=%d)",
            p_ptr->playing ? 1 : 0, p_ptr->leaving ? 1 : 0,
            p_ptr->quit_to_menu ? 1 : 0);
        (void)Term_flush();

        do_cmd_save_game();
        (void)Term_flush();
        log_debug("main menu: quit save completed (playing=%d leaving=%d saved=%d)",
            p_ptr->playing ? 1 : 0, p_ptr->leaving ? 1 : 0,
            character_saved ? 1 : 0);

        return true;
    }
    case MAIN_MENU_RETURN_GAME: // Return to game (r)
    case -1:
        return true;
    default:
        return false;
    }
}

bool do_cmd_main_menu_execute_choice(int actiontype)
{
    bool pending_hint_look = false;
    int pending_hint_look_y = -1;
    int pending_hint_look_x = -1;
    bool pending_hint_map = false;
    int pending_hint_map_y = -1;
    int pending_hint_map_x = -1;
    bool executed;

    if (death_spectator_active() && main_menu_choice_is_disabled(actiontype))
    {
        msg_print("You can no longer take that action.");
        return false;
    }

    executed = do_cmd_main_menu_execute_choice_impl(actiontype,
        &pending_hint_look, &pending_hint_look_y, &pending_hint_look_x,
        &pending_hint_map, &pending_hint_map_y, &pending_hint_map_x);

    ui_menu_click_clear();
    ui_scroll_area_clear();

    if (pending_hint_map)
    {
        do_cmd_redraw();
#ifdef USE_SDL
        sdl_minimap_focus(pending_hint_map_y, pending_hint_map_x);
#endif
        do_cmd_view_map();
    }
    else if (pending_hint_look)
    {
        do_cmd_redraw();
        do_cmd_look_at(pending_hint_look_y, pending_hint_look_x);
    }

    return executed;
}

/*
 * Brings up a menu for choosing some of the game's more abstruse options.
 */
void do_cmd_main_menu(void)
{
    (void)sdl_main_menu_overlay_begin();
}

static bool hint_message_has_source(const hint_message_meta* meta)
{
    return meta && meta->source_y >= 0 && meta->source_x >= 0
        && meta->source_y < p_ptr->cur_map_hgt && meta->source_x < p_ptr->cur_map_wid;
}

typedef enum hint_message_action {
    HINT_MESSAGE_ACTION_NONE = 0,
    HINT_MESSAGE_ACTION_LOOK,
    HINT_MESSAGE_ACTION_MAP
} hint_message_action;

enum {
    HINT_MESSAGE_CLICK_TOGGLE_TIPS = -1,
    HINT_MESSAGE_CLICK_BACK = -2,
    HINT_MESSAGE_CLICK_LOOK = -3,
    HINT_MESSAGE_CLICK_MAP = -4,
    HINT_MESSAGE_CLICK_CONTINUE = -5,
    HINT_MESSAGE_CLICK_ENTRY_BASE = 1000
};

static void hint_message_open_map_at(int y, int x)
{
    sdl_minimap_focus(y, x);
    do_cmd_view_map();
}

static bool hint_message_is_word_boundary(char ch)
{
    return (ch == '\0') || !isalnum((unsigned char)ch);
}

static bool hint_message_phrase_matches_ci(const char* line, int offset,
    const char* phrase)
{
    size_t len;

    if (!line || !phrase || !phrase[0])
        return false;

    len = strlen(phrase);
    if (SDL_strncasecmp(line + offset, phrase, len) != 0)
        return false;

    if (offset > 0 && !hint_message_is_word_boundary(line[offset - 1]))
        return false;

    return hint_message_is_word_boundary(line[offset + len]);
}

static bool hint_message_phrase_matches(const char* line, int offset, const char* phrase)
{
    size_t len;

    if (!line || !phrase || !phrase[0])
        return false;

    len = strlen(phrase);
    if (strncmp(line + offset, phrase, len) != 0)
        return false;

    if (offset > 0 && !hint_message_is_word_boundary(line[offset - 1]))
        return false;

    return hint_message_is_word_boundary(line[offset + len]);
}

typedef struct tutorial_highlight_rule {
    const char* phrase;
    byte attr;
} tutorial_highlight_rule;

static const tutorial_highlight_rule tutorial_highlight_rules[] = {
    { "Alt+'+'", TERM_WHITE },
    { "Alt+'-'", TERM_WHITE },
    { "Alt+'i'", TERM_WHITE },
    { "Alt+'l'", TERM_WHITE },
    { "'S'", TERM_WHITE },
    { "critical hit", TERM_L_BLUE },
    { "damage dice", TERM_L_BLUE },
    { "damage die", TERM_L_BLUE },
    { "damage sides", TERM_L_BLUE },
    { "damage side", TERM_L_BLUE },
    { "song points", TERM_L_BLUE },
    { "line of sight", TERM_L_BLUE },
    { "light radius", TERM_YELLOW },
    { "right panel", TERM_UMBER },
    { "bottom panel", TERM_UMBER },
    { "status panel", TERM_UMBER },
    { "bright star rating", TERM_L_GREEN },
    { "mixed elemental", TERM_L_BLUE },
    { "pure elemental", TERM_L_BLUE },
    { "vulnerabilities", TERM_L_RED },
    { "vulnerability", TERM_L_RED },
    { "vulnerable", TERM_L_RED },
    { "resistances", TERM_L_GREEN },
    { "resistance", TERM_L_GREEN },
    { "cursed", TERM_ORANGE },
    { "curse", TERM_ORANGE },
    { "jinx", TERM_ORANGE },
    { "elemental", TERM_L_BLUE },
    { "Protection", TERM_L_BLUE },
    { "protection", TERM_L_BLUE },
    { "Evasion", TERM_L_BLUE },
    { "evasion", TERM_L_BLUE },
    { "Attack", TERM_L_BLUE },
    { "attack", TERM_L_BLUE },
    { "Damage", TERM_L_BLUE },
    { "damage", TERM_L_BLUE },
    { "Stealth", TERM_L_BLUE },
    { "stealth", TERM_L_BLUE },
    { "Will", TERM_L_BLUE },
    { "will", TERM_L_BLUE },
    { "Perception", TERM_L_BLUE },
    { "perception", TERM_L_BLUE },
    { "Constitution", TERM_L_BLUE },
    { "constitution", TERM_L_BLUE },
    { "Dexterity", TERM_L_BLUE },
    { "dexterity", TERM_L_BLUE },
    { "Grace", TERM_L_BLUE },
    { "grace", TERM_L_BLUE },
    { "Strength", TERM_L_BLUE },
    { "strength", TERM_L_BLUE },
    { "Smithing", TERM_L_BLUE },
    { "smithing", TERM_L_BLUE },
    { "Song", TERM_L_BLUE },
    { "song", TERM_L_BLUE },
    { "Archery", TERM_L_BLUE },
    { "archery", TERM_L_BLUE },
    { "HP", TERM_L_BLUE },
    { "XP", TERM_L_BLUE },
    { "quiver", TERM_L_BLUE },
    { "inventory", TERM_L_BLUE },
    { "options", TERM_UMBER },
    { "light", TERM_YELLOW },
    { "fire", TERM_L_RED },
    { "ice", TERM_BLUE },
    { "cold", TERM_BLUE },
    { "poison", TERM_L_GREEN },
};

static int tutorial_hint_match_length(const char* line, int offset, byte* out_attr)
{
    int best_len = 0;
    byte best_attr = TERM_WHITE;

    for (int i = 0; i < (int)N_ELEMENTS(tutorial_highlight_rules); ++i)
    {
        const tutorial_highlight_rule* rule = &tutorial_highlight_rules[i];
        int len;

        if (!hint_message_phrase_matches_ci(line, offset, rule->phrase))
            continue;

        len = (int)strlen(rule->phrase);
        if (len > best_len)
        {
            best_len = len;
            best_attr = rule->attr;
        }
    }

    if (out_attr)
        *out_attr = best_attr;

    return best_len;
}

static int hint_message_match_length(const char* line, int offset,
    const hint_message_meta* meta, byte* out_attr)
{
    int best_len = 0;
    byte best_attr = TERM_WHITE;

    if (!meta)
        return 0;

    for (int cue = 0; cue < meta->cue_count; ++cue)
    {
        const char* dist = meta->cue_dists[cue];
        const char* dir = meta->cue_dirs[cue];

        if (hint_message_phrase_matches(line, offset, dist))
        {
            int len = (int)strlen(dist);
            if (len > best_len)
            {
                best_len = len;
                best_attr = TERM_YELLOW;
            }
        }

        if (hint_message_phrase_matches(line, offset, dir))
        {
            int len = (int)strlen(dir);
            if (len > best_len)
            {
                best_len = len;
                best_attr = TERM_L_BLUE;
            }
        }
    }

    if (out_attr)
        *out_attr = best_attr;

    return best_len;
}

static void hint_message_put_segment(int row, int col, byte attr, const char* text)
{
    if (!text || !text[0])
        return;

    if (sdl_is_story_font_enabled())
        story_print_text(row, col, 0, attr, text);
    else
        Term_putstr(col, row, -1, attr, text);
}

static byte hint_message_selected_attr(byte source_attr)
{
    (void)source_attr;
    return (byte)(TERM_UI_SELECTED + TERM_L_BLUE);
}

static void hint_message_fill_row(int row, int width, byte attr)
{
    char fill[180];
    int term_wid = Term ? Term->wid : 80;
    int term_hgt = Term ? Term->hgt : 24;

    if (row < 0 || row >= term_hgt || width <= 0)
        return;
    if (width > term_wid)
        width = term_wid;
    if (width >= (int)sizeof(fill))
        width = (int)sizeof(fill) - 1;

    SDL_memset(fill, ' ', (size_t)width);
    fill[width] = '\0';
    Term_putstr(0, row, width, attr, fill);
}

static int hint_message_selection_width(int text_col, const char* text, int wid)
{
    int text_len = text ? (int)strlen(text) : 0;
    int width = text_col + text_len;

    if (width < 1)
        width = 1;
    if (wid > 0 && width > wid)
        width = wid;

    return width;
}

static void hint_message_draw_colored_line(int row, int col, byte base_attr,
    const char* line, const hint_message_meta* meta, bool highlight_tutorial)
{
    int start = 0;
    int cursor = col;
    int len;

    if (!line)
        line = "";

    if (base_attr >= TERM_UI_SELECTED)
    {
        hint_message_put_segment(row, col, base_attr, line);
        return;
    }

    len = (int)strlen(line);
    for (int i = 0; i < len; )
    {
        byte match_attr = base_attr;
        int match_len = hint_message_match_length(line, i, meta, &match_attr);
        if (highlight_tutorial)
        {
            byte tutorial_attr = base_attr;
            int tutorial_len = tutorial_hint_match_length(line, i, &tutorial_attr);
            if (tutorial_len > match_len)
            {
                match_len = tutorial_len;
                match_attr = tutorial_attr;
            }
        }
        if (match_len > 0)
        {
            if (i > start)
            {
                char plain[256];
                int plain_len = i - start;
                memcpy(plain, line + start, plain_len);
                plain[plain_len] = '\0';
                hint_message_put_segment(row, cursor, base_attr, plain);
                cursor += plain_len;
            }

            {
                char special[256];
                memcpy(special, line + i, match_len);
                special[match_len] = '\0';
                hint_message_put_segment(row, cursor, match_attr, special);
            }

            cursor += match_len;
            i += match_len;
            start = i;
        }
        else
        {
            ++i;
        }
    }

    if (start < len)
    {
        char tail[256];
        int tail_len = len - start;
        memcpy(tail, line + start, tail_len);
        tail[tail_len] = '\0';
        hint_message_put_segment(row, cursor, base_attr, tail);
    }
}

static const char* hint_message_title(int index)
{
    byte line_count = hint_messages_message_line_count(index);
    for (int li = 0; li < line_count; ++li)
    {
        const char* line = hint_messages_message_line(index, li);
        if (line && line[0])
            return line;
    }

    return "";
}

static const char* hint_message_pick_prompt(int wid,
    const char* const prompts[], int prompt_count)
{
    int avail = wid;

    if (avail < 1)
        avail = 1;

    for (int i = 0; i < prompt_count; ++i)
    {
        if (!prompts[i])
            continue;
        if ((int)strlen(prompts[i]) <= avail)
            return prompts[i];
    }

    return (prompt_count > 0 && prompts[prompt_count - 1])
        ? prompts[prompt_count - 1]
        : "";
}

static const char* hint_message_detail_prompt(bool has_source, int wid)
{
    static const char* const simple_prompts[] = {
        "[Press any key to continue]",
        "[Any key]"
    };
    static const char* const source_prompts[] = {
        "[Press any key, 'l' to look, 'm' to show skeleton on map]",
        "[Any key; 'l' look at skeleton; 'm' map]",
        "[Any key; l look; m map]"
    };

    if (steamdeck_controls_active())
    {
        static char prompt_long[128];
        static char prompt_mid[96];
        static char prompt_short[80];
        char confirm_label[16];
        char back_label[16];
        char look_label[16];
        char map_label[16];

        controller_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        controller_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));

        if (has_source)
        {
            const char* prompts[] = {
                prompt_long,
                prompt_mid,
                prompt_short
            };

            controller_prompt_label(steamdeck_alt_action_key(), "X",
                look_label, sizeof(look_label));
            controller_prompt_label('M', "Map", map_label, sizeof(map_label));
            strnfmt(prompt_long, sizeof(prompt_long),
                "[%s] continue  [%s] look  [%s] map  [%s] back",
                confirm_label, look_label, map_label, back_label);
            strnfmt(prompt_mid, sizeof(prompt_mid),
                "[%s] continue  [%s] look  [%s] map",
                confirm_label, look_label, map_label);
            strnfmt(prompt_short, sizeof(prompt_short), "[%s] ok  [%s] map",
                confirm_label, map_label);

            return hint_message_pick_prompt(wid, prompts, N_ELEMENTS(prompts));
        }

        {
            const char* prompts[] = {
                prompt_long,
                prompt_short
            };

            strnfmt(prompt_long, sizeof(prompt_long),
                "[%s] continue  [%s] back", confirm_label, back_label);
            strnfmt(prompt_short, sizeof(prompt_short), "[%s] continue",
                confirm_label);

            return hint_message_pick_prompt(wid, prompts, N_ELEMENTS(prompts));
        }
    }

    if (has_source)
        return hint_message_pick_prompt(wid, source_prompts,
            N_ELEMENTS(source_prompts));

    return hint_message_pick_prompt(wid, simple_prompts,
        N_ELEMENTS(simple_prompts));
}

static void hint_message_detail_register_prompt(const char* prompt,
    int row, bool has_source)
{
    if (!prompt)
        return;

    ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_CONTINUE, 0, row,
        prompt, "continue");
    ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_CONTINUE, 0, row,
        prompt, "Any key");
    ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_CONTINUE, 0, row,
        prompt, "any key");
    ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_CONTINUE, 0, row,
        prompt, "ok");
    ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_BACK, 0, row,
        prompt, "back");

    if (!has_source)
        return;

    ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_LOOK, 0, row,
        prompt, "look");
    ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_LOOK, 0, row,
        prompt, "'l'");
    ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_MAP, 0, row,
        prompt, "map");
    ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_MAP, 0, row,
        prompt, "'m'");
}

/*
 * Touch-only command buttons for the hint detail screen.  Replaces the
 * keyboard-key prompt with tap targets: Look / Map (when the hint points at a
 * map location) plus Back.  Any non-Look/Map click in the detail loop closes
 * the screen, so the Back button needs no special handling.
 */
static void hint_message_detail_touch_buttons(int row, bool has_source)
{
    int col = 0;

    if (has_source)
    {
        col = ui_menu_click_put_button(HINT_MESSAGE_CLICK_LOOK, row, col,
            TERM_L_WHITE, "Look");
        col = ui_menu_click_put_button(HINT_MESSAGE_CLICK_MAP, row, col,
            TERM_L_WHITE, "Map");
    }
    (void)ui_menu_click_put_button(HINT_MESSAGE_CLICK_BACK, row, col,
        TERM_L_WHITE, "Back");
}

static const char* hint_message_list_prompt(bool show_all_tips,
    int level_n, int tip_n, int wid)
{
    static const char* const tip_list_prompts[] = {
        "[Tab tabs, Dir move, Enter read, h level hints, Esc]",
        "[Tab tabs, Enter read, h hints, Esc]",
        "[Tab, Enter, h, Esc]"
    };
    static const char* const level_list_prompts[] = {
        "[Tab tabs, Dir move, Enter read, h tips, l look, m map, Esc]",
        "[Tab tabs, Enter, h tips, l look, m map, Esc]",
        "[Tab, Enter, h, l, m, Esc]"
    };
    static const char* const no_level_with_tips_prompts[] = {
        "[No level hint messages. Tab tabs, h all tips, Esc]",
        "[No level hints. Tab tabs, h tips, Esc]",
        "[No hints. Tab, h, Esc]"
    };
    static const char* const no_level_prompts[] = {
        "[No level hint messages. Tab tabs, Esc]",
        "[No level hints. Tab, Esc]"
    };

    if (steamdeck_controls_active())
    {
        static char prompt_long[160];
        static char prompt_mid[128];
        static char prompt_short[96];
        char confirm_label[16];
        char toggle_label[16];
        char look_label[16];
        char map_label[16];
        char back_label[16];

        controller_prompt_label(steamdeck_confirm_key(), "A", confirm_label,
            sizeof(confirm_label));
        controller_prompt_label(steamdeck_secondary_key(), "Y", toggle_label,
            sizeof(toggle_label));
        controller_prompt_label(steamdeck_alt_action_key(), "X", look_label,
            sizeof(look_label));
        controller_prompt_label('M', "Map", map_label, sizeof(map_label));
        controller_prompt_label(steamdeck_back_key(), "B", back_label,
            sizeof(back_label));

        if (show_all_tips)
        {
            const char* prompts[] = {
                prompt_long,
                prompt_mid,
                prompt_short
            };

            strnfmt(prompt_long, sizeof(prompt_long),
                "D-pad move  [%s] read  [%s] level hints  [%s] back",
                confirm_label, toggle_label, back_label);
            strnfmt(prompt_mid, sizeof(prompt_mid),
                "D-pad  [%s] read  [%s] hints  [%s] back",
                confirm_label, toggle_label, back_label);
            strnfmt(prompt_short, sizeof(prompt_short),
                "[%s] read  [%s] hints  [%s] back",
                confirm_label, toggle_label, back_label);

            return hint_message_pick_prompt(wid, prompts, N_ELEMENTS(prompts));
        }

        if (level_n > 0)
        {
            const char* prompts[] = {
                prompt_long,
                prompt_mid,
                prompt_short
            };

            strnfmt(prompt_long, sizeof(prompt_long),
                "D-pad move  [%s] read  [%s] tips  [%s] look  [%s] map  [%s] back",
                confirm_label, toggle_label, look_label, map_label, back_label);
            strnfmt(prompt_mid, sizeof(prompt_mid),
                "D-pad  [%s] read  [%s] tips  [%s] look  [%s] map",
                confirm_label, toggle_label, look_label, map_label);
            strnfmt(prompt_short, sizeof(prompt_short),
                "[%s] read  [%s] look  [%s] map",
                confirm_label, look_label, map_label);

            return hint_message_pick_prompt(wid, prompts, N_ELEMENTS(prompts));
        }

        if (tip_n > 0)
        {
            const char* prompts[] = {
                prompt_long,
                prompt_short
            };

            strnfmt(prompt_long, sizeof(prompt_long),
                "No level hints.  [%s] all tips  [%s] back",
                toggle_label, back_label);
            strnfmt(prompt_short, sizeof(prompt_short),
                "No hints.  [%s] tips  [%s] back", toggle_label, back_label);

            return hint_message_pick_prompt(wid, prompts, N_ELEMENTS(prompts));
        }

        {
            const char* prompts[] = {
                prompt_long,
                prompt_short
            };

            strnfmt(prompt_long, sizeof(prompt_long),
                "No level hint messages.  [%s] back", back_label);
            strnfmt(prompt_short, sizeof(prompt_short), "No hints.  [%s] back",
                back_label);

            return hint_message_pick_prompt(wid, prompts, N_ELEMENTS(prompts));
        }
    }

    if (show_all_tips)
        return hint_message_pick_prompt(wid, tip_list_prompts,
            N_ELEMENTS(tip_list_prompts));

    if (level_n > 0)
        return hint_message_pick_prompt(wid, level_list_prompts,
            N_ELEMENTS(level_list_prompts));

    if (tip_n > 0)
        return hint_message_pick_prompt(wid, no_level_with_tips_prompts,
            N_ELEMENTS(no_level_with_tips_prompts));

    return hint_message_pick_prompt(wid, no_level_prompts,
        N_ELEMENTS(no_level_prompts));
}

static void hint_message_list_register_prompt(const char* prompt, int row,
    bool show_all_tips, int level_n, int tip_n)
{
    if (!prompt)
        return;

    /* The "Tab" command word switches to the Quests tab, mirroring the tab
     * buttons at the top of the screen. */
    ui_menu_click_add_text_token(HINT_QUEST_CLICK_QUESTS_TAB, 0, row,
        prompt, "Tab");

    if (tip_n > 0)
    {
        /* Include the leading "h " so the whole command (not just the word)
         * lights up on hover. */
        ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_TOGGLE_TIPS, 0,
            row, prompt, "h level hints");
        ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_TOGGLE_TIPS, 0,
            row, prompt, "h all tips");
        ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_TOGGLE_TIPS, 0,
            row, prompt, "h hints");
        ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_TOGGLE_TIPS, 0,
            row, prompt, "h tips");
        ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_TOGGLE_TIPS, 0,
            row, prompt, "h,");
    }

    ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_CONTINUE, 0, row,
        prompt, "Enter");
    ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_CONTINUE, 0, row,
        prompt, "read");
    ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_BACK, 0, row,
        prompt, "ESCAPE");
    ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_BACK, 0, row,
        prompt, "ESC");
    ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_BACK, 0, row,
        prompt, "Esc");
    ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_BACK, 0, row,
        prompt, "back");

    if (!show_all_tips && level_n > 0)
    {
        ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_LOOK, 0, row,
            prompt, "look");
        ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_LOOK, 0, row,
            prompt, "'l'");
        ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_LOOK, 0, row,
            prompt, "l,");
        ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_MAP, 0, row,
            prompt, "map");
        ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_MAP, 0, row,
            prompt, "'m'");
        ui_menu_click_add_text_token(HINT_MESSAGE_CLICK_MAP, 0, row,
            prompt, "m,");
    }
}

/*
 * Touch-only command buttons for the hint list screen.  The Quests tab is
 * already a tappable button at the top and reading a hint is done by tapping
 * its row, so the footer only needs the tips toggle (when tutorial tips exist)
 * and Back.  Per-hint Look/Map live on the detail screen.
 */
static void hint_message_list_touch_buttons(int row, bool show_all_tips,
    int tip_n)
{
    int col = 0;

    if (tip_n > 0)
        col = ui_menu_click_put_button(HINT_MESSAGE_CLICK_TOGGLE_TIPS, row, col,
            TERM_L_WHITE, show_all_tips ? "Level Hints" : "All Tips");
    (void)ui_menu_click_put_button(HINT_MESSAGE_CLICK_BACK, row, col,
        TERM_L_WHITE, "Back");
}

typedef struct hint_message_display_line {
    char text[256];
    byte source_line;
} hint_message_display_line;

enum {
    HINT_MESSAGE_DISPLAY_TEXT_MAX = 256,
    HINT_MESSAGE_DISPLAY_LINES_MAX = 48,
    HINT_MESSAGE_LIST_LINES_MAX = 64
};

static int hint_message_wrap_list_text(const char* text, int wrap_cols,
    hint_message_display_line* lines, int limit);

static int hint_message_list_emit_token(int base_row, int wid, int text_col,
    int max_rows, int* used_rows, int* cursor_col, bool draw, byte attr,
    const char* text)
{
    int full_width;
    int len;
    int row;

    if (!text || !text[0] || !used_rows || !cursor_col)
        return true;

    full_width = wid - text_col - 1;
    if (full_width < 1)
        full_width = 1;

    len = (int)strlen(text);

    if (*used_rows <= 0)
    {
        *used_rows = 1;
        *cursor_col = text_col;
    }

    if (*cursor_col + len > wid - 1 && *cursor_col > text_col)
    {
        if (*used_rows >= max_rows)
            return false;

        row = base_row + *used_rows;
        if (draw)
        {
            Term_erase(0, row, 255);
            if (attr >= TERM_UI_SELECTED)
                hint_message_fill_row(row, text_col, attr);
        }
        (*used_rows)++;
        *cursor_col = text_col;
    }

    row = base_row + (*used_rows - 1);

    if (len <= full_width)
    {
        if (draw)
            hint_message_put_segment(row, *cursor_col, attr, text);
        *cursor_col += len;
        return true;
    }

    {
        hint_message_display_line lines[HINT_MESSAGE_LIST_LINES_MAX];
        int line_count = hint_message_wrap_list_text(text, full_width, lines,
            HINT_MESSAGE_LIST_LINES_MAX);

        for (int li = 0; li < line_count; ++li)
        {
            if (li > 0 || *cursor_col > text_col)
            {
                if (*used_rows >= max_rows)
                    return false;

                row = base_row + *used_rows;
                if (draw)
                {
                    Term_erase(0, row, 255);
                    if (attr >= TERM_UI_SELECTED)
                        hint_message_fill_row(row, text_col, attr);
                }
                (*used_rows)++;
                *cursor_col = text_col;
            }

            row = base_row + (*used_rows - 1);
            if (draw)
                hint_message_put_segment(row, *cursor_col, attr, lines[li].text);
            *cursor_col += (int)strlen(lines[li].text);
        }
    }

    return true;
}

static int skeleton_tip_template_count(void)
{
    int count = 0;

    if (!skeleton_note_info || !skeleton_note_text || !z_info)
        return 0;

    for (int i = 0; i < z_info->skeleton_note_max; ++i)
    {
        const skeleton_note_template* tip = &skeleton_note_info[i];

        if (tip->role != SKELETON_NOTE_ROLE_HINT || tip->hint != SKEL_HINT_TIP)
            continue;
        if (tip->weight == 0 || tip->text == 0)
            continue;

        count++;
    }

    return count;
}

static const skeleton_note_template* skeleton_tip_template_by_index(int index)
{
    int seen = 0;

    if (index < 0 || !skeleton_note_info || !skeleton_note_text || !z_info)
        return NULL;

    for (int i = 0; i < z_info->skeleton_note_max; ++i)
    {
        const skeleton_note_template* tip = &skeleton_note_info[i];

        if (tip->role != SKELETON_NOTE_ROLE_HINT || tip->hint != SKEL_HINT_TIP)
            continue;
        if (tip->weight == 0 || tip->text == 0)
            continue;

        if (seen == index)
            return tip;

        seen++;
    }

    return NULL;
}

static bool skeleton_tip_text_by_index(int index, char* buf, size_t buf_sz)
{
    const skeleton_note_template* tip = skeleton_tip_template_by_index(index);
    const char* main_text;
    const char* extra_text = NULL;

    if (!buf || buf_sz == 0)
        return false;

    buf[0] = '\0';

    if (!tip || !skeleton_note_text)
        return false;

    main_text = skeleton_note_text + tip->text;
    if (tip->extra_text)
        extra_text = skeleton_note_text + tip->extra_text;

    if (extra_text && extra_text[0])
    {
        strnfmt(buf, buf_sz, "%s %s", main_text, extra_text);
    }
    else
    {
        strnfmt(buf, buf_sz, "%s", main_text);
    }

    return (buf[0] != '\0');
}

static int hint_message_effective_wrap_cols(int wrap_cols,
    size_t line_capacity)
{
    int max_cols = (int)line_capacity - 1;

    if (max_cols < 1)
        max_cols = 1;
    if (wrap_cols < 1)
        wrap_cols = 1;
    if (wrap_cols > max_cols)
        wrap_cols = max_cols;

    return wrap_cols;
}

static int hint_message_max_chars_fit_pixels(const char* text, int max_chars,
    int max_px, int cell_width)
{
    if (!text || max_chars <= 0)
        return 0;

    if (max_px <= 0 || cell_width <= 0)
        return max_chars;

    int lo = 1;
    int hi = max_chars;
    int best = 1;

    while (lo <= hi)
    {
        int mid = (lo + hi) / 2;
        int width = sdl_story_font_text_width(text, mid);
        if (width <= 0)
            width = mid * cell_width;

        if (width <= max_px)
        {
            best = mid;
            lo = mid + 1;
        }
        else
        {
            hi = mid - 1;
        }
    }

    return best;
}

static int hint_message_append_wrapped_segment_mono(const char* seg,
    hint_message_display_line* lines, int idx, int limit, int wrap_cols,
    byte source_line)
{
    int len;
    int pos;

    if (!seg || !seg[0] || !lines || limit <= idx)
        return idx;

    wrap_cols = hint_message_effective_wrap_cols(
        wrap_cols, sizeof(lines[0].text));

    len = (int)strlen(seg);
    pos = 0;

    while (pos < len && idx < limit)
    {
        int remaining;
        int take;

        while (pos < len && seg[pos] == ' ')
            pos++;
        if (pos >= len)
            break;

        remaining = len - pos;
        take = (remaining <= wrap_cols) ? remaining : wrap_cols;

        if (remaining > wrap_cols)
        {
            int end = pos + take;
            int split = -1;
            for (int j = end - 1; j > pos; --j)
            {
                if (seg[j] == ' ')
                {
                    split = j;
                    break;
                }
            }
            if (split > pos)
                take = split - pos;
        }

        while (take > 0 && seg[pos + take - 1] == ' ')
            take--;

        if (take <= 0)
            break;

        strnfmt(lines[idx].text, sizeof(lines[idx].text), "%.*s", take, seg + pos);
        lines[idx].source_line = source_line;
        idx++;
        pos += take;
    }

    return idx;
}

static int hint_message_append_wrapped_segment_story(const char* seg,
    hint_message_display_line* lines, int idx, int limit, int wrap_cols,
    byte source_line)
{
    int cell_width;
    int wrap_px;
    int space_px;
    int max_line_chars;
    const char* s;

    if (!seg || !seg[0] || !lines || limit <= idx)
        return idx;

    wrap_cols = hint_message_effective_wrap_cols(
        wrap_cols, sizeof(lines[0].text));

    cell_width = sdl_get_cell_width();
    if (cell_width <= 0)
        return hint_message_append_wrapped_segment_mono(
            seg, lines, idx, limit, wrap_cols, source_line);

    wrap_px = wrap_cols * cell_width;
    space_px = sdl_story_font_text_width(" ", 1);
    if (space_px <= 0)
        space_px = cell_width;

    max_line_chars = wrap_cols;
    if (max_line_chars > HINT_MESSAGE_DISPLAY_TEXT_MAX - 1)
        max_line_chars = HINT_MESSAGE_DISPLAY_TEXT_MAX - 1;

    s = seg;
    while (*s && idx < limit)
    {
        char out[HINT_MESSAGE_DISPLAY_TEXT_MAX];
        int out_len = 0;
        int line_px = 0;
        bool first_word = true;

        while (*s == ' ')
            s++;
        if (!*s)
            break;

        while (*s)
        {
            const char* word;
            int word_len = 0;
            int word_px;
            int add_px;
            int add_chars;

            while (*s == ' ')
                s++;
            if (!*s)
                break;

            word = s;
            while (word[word_len] && word[word_len] != ' ')
                word_len++;

            word_px = sdl_story_font_text_width(word, word_len);
            if (word_px <= 0)
                word_px = word_len * cell_width;

            add_px = word_px + (first_word ? 0 : space_px);
            add_chars = word_len + (first_word ? 0 : 1);

            if (!first_word
                && ((line_px + add_px) > wrap_px
                    || (out_len + add_chars) > max_line_chars))
            {
                break;
            }

            if (first_word && (word_px > wrap_px || word_len > max_line_chars))
            {
                int max_chars = word_len;
                int fit;

                if (max_chars > max_line_chars - out_len)
                    max_chars = max_line_chars - out_len;
                fit = hint_message_max_chars_fit_pixels(
                    word, max_chars, wrap_px, cell_width);
                if (fit <= 0)
                    fit = 1;

                memcpy(out + out_len, word, fit);
                out_len += fit;
                out[out_len] = '\0';
                s += fit;
                break;
            }

            if (!first_word)
            {
                out[out_len++] = ' ';
                line_px += space_px;
            }

            if (word_len > HINT_MESSAGE_DISPLAY_TEXT_MAX - 1 - out_len)
                word_len = HINT_MESSAGE_DISPLAY_TEXT_MAX - 1 - out_len;
            if (word_len > max_line_chars - out_len)
                word_len = max_line_chars - out_len;
            memcpy(out + out_len, word, word_len);
            out_len += word_len;
            out[out_len] = '\0';
            line_px += word_px;

            s += word_len;
            first_word = false;
        }

        if (out_len > 0)
        {
            strnfmt(lines[idx].text, sizeof(lines[idx].text), "%s", out);
            lines[idx].source_line = source_line;
            idx++;
        }

        while (*s == ' ')
            s++;
    }

    return idx;
}

static int hint_message_append_wrapped_text(const char* text,
    hint_message_display_line* lines, int idx, int limit, int wrap_cols,
    byte source_line)
{
    char expanded[512];
    char* seg;

    if (!text || !lines || limit <= idx)
        return idx;

    if (!text[0])
    {
        lines[idx].text[0] = '\0';
        lines[idx].source_line = source_line;
        return idx + 1;
    }

    strnfmt(expanded, sizeof(expanded), "%s", text);
    seg = expanded;

    while (seg && *seg && idx < limit)
    {
        char* next = strchr(seg, '|');
        if (next)
        {
            *next = '\0';
            next++;
        }

        while (*seg == ' ')
            seg++;

        if (*seg)
        {
            if (sdl_is_story_font_enabled() && sdl_story_font_text_width(" ", 1) > 0
                && sdl_get_cell_width() > 0)
            {
                idx = hint_message_append_wrapped_segment_story(
                    seg, lines, idx, limit, wrap_cols, source_line);
            }
            else
            {
                idx = hint_message_append_wrapped_segment_mono(
                    seg, lines, idx, limit, wrap_cols, source_line);
            }
        }
        else
        {
            lines[idx].text[0] = '\0';
            lines[idx].source_line = source_line;
            idx++;
        }

        seg = next;
    }

    return idx;
}

static int hint_message_wrap_list_text(const char* text, int wrap_cols,
    hint_message_display_line* lines, int limit)
{
    int line_count = hint_message_append_wrapped_text(
        text, lines, 0, limit, wrap_cols, 0);

    if (line_count <= 0 && limit > 0)
    {
        lines[0].text[0] = '\0';
        lines[0].source_line = 0;
        line_count = 1;
    }

    return line_count;
}

static int hint_message_draw_wrapped_list_entry(int row, int idx,
    bool selected, int wid, int max_rows, const char* text,
    const hint_message_meta* meta, bool highlight_tutorial)
{
    char prefix[8];
    hint_message_display_line lines[HINT_MESSAGE_LIST_LINES_MAX];
    byte selected_attr = hint_message_selected_attr(TERM_WHITE);
    byte prefix_attr = selected ? selected_attr : TERM_WHITE;
    byte title_attr = selected ? selected_attr : TERM_WHITE;
    int text_col;
    int line_count;
    int draw_count;

    if (max_rows <= 0)
        return 0;

    strnfmt(prefix, sizeof(prefix), "%2d) ", idx + 1);
    text_col = (int)strlen(prefix);
    line_count = hint_message_wrap_list_text(text ? text : "",
        wid - text_col - 1, lines, HINT_MESSAGE_LIST_LINES_MAX);
    draw_count = MIN(line_count, max_rows);

    for (int li = 0; li < draw_count; ++li)
    {
        Term_erase(0, row + li, 255);
        if (selected)
        {
            int selection_w = hint_message_selection_width(text_col,
                lines[li].text, wid);
            hint_message_fill_row(row + li, selection_w, selected_attr);
        }

        if (li == 0)
            Term_putstr(0, row + li, -1, prefix_attr, prefix);

        hint_message_draw_colored_line(row + li, text_col, title_attr,
            lines[li].text, meta, highlight_tutorial);
    }

    return draw_count;
}

static int hint_message_layout_list_entry(int row, int idx, bool selected,
    int wid, int max_rows, bool draw)
{
    hint_message_meta meta;
    char prefix[8];
    hint_message_display_line title_lines[HINT_MESSAGE_LIST_LINES_MAX];
    const char* title = hint_message_title(idx);
    byte selected_attr = hint_message_selected_attr(TERM_WHITE);
    byte prefix_attr = selected ? selected_attr : TERM_WHITE;
    byte title_attr = selected ? selected_attr : TERM_WHITE;
    byte chrome_attr = selected ? selected_attr : TERM_SLATE;
    byte cue_dist_attr = selected ? selected_attr : TERM_YELLOW;
    byte cue_dir_attr = selected ? selected_attr : TERM_L_BLUE;
    int text_col;
    int title_count;
    int used_rows = 0;
    int cursor_col = 0;

    if (max_rows <= 0)
        return 0;

    hint_messages_message_meta(idx, &meta);
    strnfmt(prefix, sizeof(prefix), "%2d) ", idx + 1);
    text_col = (int)strlen(prefix);
    title_count = hint_message_wrap_list_text(title ? title : "",
        wid - text_col - 1, title_lines, HINT_MESSAGE_LIST_LINES_MAX);

    for (int li = 0; li < title_count && used_rows < max_rows; ++li)
    {
        if (draw)
        {
            Term_erase(0, row + used_rows, 255);
            if (selected)
            {
                int selection_w = hint_message_selection_width(text_col,
                    title_lines[li].text, wid);
                hint_message_fill_row(row + used_rows, selection_w,
                    selected_attr);
            }
            if (li == 0)
                Term_putstr(0, row + used_rows, -1, prefix_attr, prefix);
            hint_message_put_segment(row + used_rows, text_col, title_attr,
                title_lines[li].text);
        }

        used_rows++;
    }

    if (used_rows <= 0 || meta.cue_count <= 0)
        return used_rows;

    cursor_col = text_col + (int)strlen(title_lines[title_count - 1].text);

    if (!hint_message_list_emit_token(row, wid, text_col, max_rows,
            &used_rows, &cursor_col, draw, chrome_attr, " ["))
    {
        return used_rows;
    }

    for (int cue = 0; cue < meta.cue_count; ++cue)
    {
        if (cue > 0)
        {
            if (!hint_message_list_emit_token(row, wid, text_col, max_rows,
                    &used_rows, &cursor_col, draw, chrome_attr, "; "))
            {
                return used_rows;
            }
        }

        if (meta.cue_dists[cue][0])
        {
            if (!hint_message_list_emit_token(row, wid, text_col, max_rows,
                    &used_rows, &cursor_col, draw, cue_dist_attr,
                    meta.cue_dists[cue]))
            {
                return used_rows;
            }
        }

        if (meta.cue_dists[cue][0] && meta.cue_dirs[cue][0])
        {
            if (!hint_message_list_emit_token(row, wid, text_col, max_rows,
                    &used_rows, &cursor_col, draw, chrome_attr, " "))
            {
                return used_rows;
            }
        }

        if (meta.cue_dirs[cue][0])
        {
            if (!hint_message_list_emit_token(row, wid, text_col, max_rows,
                    &used_rows, &cursor_col, draw, cue_dir_attr,
                    meta.cue_dirs[cue]))
            {
                return used_rows;
            }
        }
    }

    (void)hint_message_list_emit_token(row, wid, text_col, max_rows,
        &used_rows, &cursor_col, draw, chrome_attr, "]");

    return used_rows;
}

static int hint_message_list_entry_height(int idx, int wid)
{
    return hint_message_layout_list_entry(0, idx, false, wid,
        HINT_MESSAGE_LIST_LINES_MAX, false);
}

static int hint_message_draw_list_row(int row, int idx, bool selected, int wid,
    int max_rows)
{
    return hint_message_layout_list_entry(row, idx, selected, wid,
        max_rows, true);
}

static int skeleton_tip_list_entry_height(int idx, int wid)
{
    char text[512];
    hint_message_display_line lines[HINT_MESSAGE_LIST_LINES_MAX];
    char prefix[8];
    int text_col;

    if (!skeleton_tip_text_by_index(idx, text, sizeof(text)))
        text[0] = '\0';

    strnfmt(prefix, sizeof(prefix), "%2d) ", idx + 1);
    text_col = (int)strlen(prefix);

    return hint_message_wrap_list_text(text, wid - text_col - 1,
        lines, HINT_MESSAGE_LIST_LINES_MAX);
}

static int skeleton_tip_draw_list_row(int row, int idx, bool selected, int wid,
    int max_rows)
{
    char tip_text[512];

    if (!skeleton_tip_text_by_index(idx, tip_text, sizeof(tip_text)))
        tip_text[0] = '\0';

    return hint_message_draw_wrapped_list_entry(row, idx, selected, wid,
        max_rows, tip_text, NULL, true);
}

static bool skeleton_tip_show_internal(int index, bool manage_screen)
{
    int wid = 80;
    int hgt = 24;
    int row = 4;
    int col = 8;
    hint_message_display_line lines[HINT_MESSAGE_DISPLAY_LINES_MAX];
    char tip_text[512];
    char ch;
    int line_count = 0;

    if (!skeleton_tip_text_by_index(index, tip_text, sizeof(tip_text)))
        return false;

    if (manage_screen)
        screen_save();

    sdl_story_font_enable();

    while (1)
    {
        Term_clear();
        Term_get_size(&wid, &hgt);
        line_count = 0;
        ui_scroll_area_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);

        line_count = hint_message_append_wrapped_text(
            "Hint: Survival Tip", lines, line_count,
            HINT_MESSAGE_DISPLAY_LINES_MAX, wid - col - 1, 0);
        line_count = hint_message_append_wrapped_text(
            tip_text, lines, line_count, HINT_MESSAGE_DISPLAY_LINES_MAX,
            wid - col - 1, 1);

        for (int li = 0; li < line_count && row + li < hgt - 1; ++li)
        {
            bool title_line = (lines[li].source_line == 0);
            byte base_attr = title_line ? TERM_L_WHITE : TERM_WHITE;
            hint_message_draw_colored_line(row + li, col, base_attr, lines[li].text,
                NULL, !title_line);
            ui_menu_click_add_full_row(HINT_MESSAGE_CLICK_CONTINUE, row + li);
        }

        if (sdl_touch_only_device_active())
        {
            hint_message_detail_touch_buttons(hgt - 1, false);
        }
        else
        {
            const char* prompt = hint_message_detail_prompt(false, wid);
            prt(prompt, hgt - 1, 0);
            hint_message_detail_register_prompt(prompt, hgt - 1, false);
        }
        Term_fresh();

        hide_cursor = true;
        ch = inkey();
        hide_cursor = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;

                (void)clicked_choice;
                break;
            }
            if (ch == UI_MENU_CLICK_WAKE_KEY)
                continue;
        }
        ch = (char)steamdeck_menu_key(ch, 0, 0);
        break;
    }

    ui_menu_click_clear();
    ui_scroll_area_clear();
    sdl_story_font_disable();
    if (manage_screen)
        screen_load();

    return false;
}

static hint_message_action hint_message_show_internal(int index, int* source_y, int* source_x,
    bool manage_screen)
{
    int wid = 80;
    int hgt = 24;
    int row = 4;
    int col = 8;
    hint_message_display_line display_lines[HINT_MESSAGE_DISPLAY_LINES_MAX];
    char ch;
    hint_message_meta meta;
    byte stored_line_count;
    int display_line_count = 0;
    hint_message_action action = HINT_MESSAGE_ACTION_NONE;
    bool highlight_tutorial = false;
    bool steamdeck = steamdeck_controls_active();

    hint_messages_ensure_level_state();
    stored_line_count = hint_messages_message_line_count(index);
    if (!stored_line_count)
        return HINT_MESSAGE_ACTION_NONE;

    hint_messages_message_meta(index, &meta);
    highlight_tutorial = (strstr(hint_messages_message_line(index, 0), "Survival Tip") != NULL);

    if (manage_screen)
        screen_save();

    sdl_story_font_enable();

    while (1)
    {
        Term_clear();
        Term_get_size(&wid, &hgt);
        display_line_count = 0;
        ui_scroll_area_clear();
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);

        for (int li = 0; li < stored_line_count; ++li)
        {
            display_line_count = hint_message_append_wrapped_text(
                hint_messages_message_line(index, li),
                display_lines, display_line_count,
                HINT_MESSAGE_DISPLAY_LINES_MAX, wid - col - 1, (byte)li);
        }

        for (int li = 0; li < display_line_count && row + li < hgt - 1; ++li)
        {
            bool title_line = (display_lines[li].source_line == 0);
            byte base_attr = title_line ? TERM_L_WHITE : TERM_WHITE;
            hint_message_draw_colored_line(row + li, col, base_attr,
                display_lines[li].text, title_line ? NULL : &meta,
                (highlight_tutorial && !title_line));
            ui_menu_click_add_full_row(HINT_MESSAGE_CLICK_CONTINUE, row + li);
        }

        {
            bool has_source = hint_message_has_source(&meta);

            if (sdl_touch_only_device_active())
            {
                hint_message_detail_touch_buttons(hgt - 1, has_source);
            }
            else
            {
                const char* prompt = hint_message_detail_prompt(has_source,
                    wid);
                prt(prompt, hgt - 1, 0);
                hint_message_detail_register_prompt(prompt, hgt - 1,
                    has_source);
            }
        }

        Term_fresh();

        hide_cursor = true;
        ch = inkey();
        hide_cursor = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (click_action == UI_MENU_CLICK_HOVER)
                    continue;

                if (clicked_choice == HINT_MESSAGE_CLICK_LOOK
                    && hint_message_has_source(&meta))
                {
                    if (source_y)
                        *source_y = meta.source_y;
                    if (source_x)
                        *source_x = meta.source_x;
                    action = HINT_MESSAGE_ACTION_LOOK;
                    break;
                }

                if (clicked_choice == HINT_MESSAGE_CLICK_MAP
                    && hint_message_has_source(&meta))
                {
                    if (source_y)
                        *source_y = meta.source_y;
                    if (source_x)
                        *source_x = meta.source_x;
                    action = HINT_MESSAGE_ACTION_MAP;
                    break;
                }

                break;
            }
            if (ch == UI_MENU_CLICK_WAKE_KEY)
                continue;
        }
        ch = (char)steamdeck_menu_key(ch, 0, 0);

        if (steamdeck && ch == steamdeck_back_key())
            break;

        if ((ch == 'l' || ch == 'L'
                || (steamdeck && ch == steamdeck_alt_action_key()))
            && hint_message_has_source(&meta))
        {
            if (source_y)
                *source_y = meta.source_y;
            if (source_x)
                *source_x = meta.source_x;
            action = HINT_MESSAGE_ACTION_LOOK;
            break;
        }

        if ((ch == 'm' || ch == 'M')
            && hint_message_has_source(&meta))
        {
            if (source_y)
                *source_y = meta.source_y;
            if (source_x)
                *source_x = meta.source_x;
            action = HINT_MESSAGE_ACTION_MAP;
            break;
        }

        break;
    }

    ui_menu_click_clear();
    ui_scroll_area_clear();
    sdl_story_font_disable();
    if (manage_screen)
        screen_load();

    return action;
}

void show_hint_message_screen(int index)
{
    int look_y = -1;
    int look_x = -1;
    hint_message_action action;

    action = hint_message_show_internal(index, &look_y, &look_x, true);
    if (action == HINT_MESSAGE_ACTION_LOOK)
    {
        do_cmd_redraw();
        do_cmd_look_at(look_y, look_x);
    }
    else if (action == HINT_MESSAGE_ACTION_MAP)
    {
        do_cmd_redraw();
        hint_message_open_map_at(look_y, look_x);
    }
}

static bool do_cmd_hint_messages(bool* out_pending_look, int* out_look_y,
    int* out_look_x, bool* out_pending_map, int* out_map_y,
    int* out_map_x, bool manage_screen)
{
    char ch;

    int wid, hgt;
    bool pending_look = false;
    int look_y = -1;
    int look_x = -1;
    bool pending_map = false;
    int map_y = -1;
    int map_x = -1;
    bool switch_to_quests = false;
    int hover_tab = 0;
    bool show_all_tips = false;
    bool tabs_focus = false;
    bool steamdeck = steamdeck_controls_active();

    /* Clear any active banner before opening hint messages */
    if (dismiss_active_narrative_banner()) {
        do_cmd_redraw();
    }

    hint_messages_ensure_level_state();

    int level_n = (int)hint_messages_count_for_save();
    int tip_n = skeleton_tip_template_count();
    int sel = 0;
    int top = 0;
    show_all_tips = false;

    if (manage_screen)
    {
        screen_save();
        screen_push_supporting_panes_hidden();
    }

    while (1)
    {
        int n = show_all_tips ? tip_n : level_n;
        int draw_row = 0;
        int body_top;
        int body_bottom;

        Term_get_size(&wid, &hgt);
        Term_clear();

        int rows = hgt - 5;
        if (rows < 1)
            rows = 1;
        body_top = 3;
        body_bottom = body_top + rows - 1;
        if (body_bottom >= hgt - 1)
            body_bottom = hgt - 2;
        if (body_bottom < body_top)
            body_bottom = body_top;
        ui_scroll_area_begin(body_top, body_bottom,
            SDL_TOUCH_MENU_CATEGORY_OTHER);
        ui_scroll_area_set_keys('8', '2', '6', '4');
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);

        if (n > 0)
        {
            if (sel < 0)
                sel = 0;
            if (sel >= n)
                sel = n - 1;

            if (sel < top)
                top = sel;
            if (top < 0)
                top = 0;

            while (top < sel)
            {
                int used = 0;

                for (int idx = top; idx <= sel; ++idx)
                {
                    int height = show_all_tips
                        ? skeleton_tip_list_entry_height(idx, wid)
                        : hint_message_list_entry_height(idx, wid);

                    used += MIN(height, rows);
                }

                if (used <= rows)
                    break;

                top++;
            }

            if (top >= n)
                top = n - 1;
        }
        else
        {
            sel = 0;
            top = 0;
        }

        hint_quest_draw_tabs(false,
            tabs_focus ? HINT_QUEST_CLICK_HINTS_TAB : hover_tab, wid);

        if (show_all_tips)
            prt(format("All Tutorial Hints (%d)", tip_n), 2, 0);
        else
            prt(format("Hint Messages (%d)", level_n), 2, 0);

        if (sdl_touch_only_device_active())
        {
            hint_message_list_touch_buttons(hgt - 1, show_all_tips, tip_n);
        }
        else
        {
            const char* prompt = hint_message_list_prompt(show_all_tips,
                level_n, tip_n, wid);
            prt(prompt, hgt - 1, 0);
            hint_message_list_register_prompt(prompt, hgt - 1,
                show_all_tips, level_n, tip_n);
        }

        if (n <= 0)
        {
            Term_putstr(0, body_top, -1, TERM_SLATE,
                show_all_tips ? "No tutorial hints are available."
                             : "You recall no hint messages on this level.");
        }

        for (int idx = top; idx < n && draw_row < rows; ++idx)
        {
            int used;
            int entry_row = body_top + draw_row;

            if (show_all_tips)
                used = skeleton_tip_draw_list_row(entry_row, idx,
                    idx == sel, wid, rows - draw_row);
            else
                used = hint_message_draw_list_row(entry_row, idx,
                    idx == sel, wid, rows - draw_row);

            if (used <= 0)
                break;

            for (int click_row = 0; click_row < used; ++click_row)
            {
                int row_y = entry_row + click_row;
                if (row_y > body_bottom)
                    break;
                ui_menu_click_add(HINT_MESSAGE_CLICK_ENTRY_BASE + idx,
                    0, row_y, wid);
            }

            draw_row += used;
        }

        Term_fresh();
        {
            bool saved_hide_cursor = hide_cursor;

            hide_cursor = true;
            ch = inkey();
            hide_cursor = saved_hide_cursor;
        }
        bool click_generated_command = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (clicked_choice == HINT_QUEST_CLICK_QUESTS_TAB)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                    {
                        hover_tab = clicked_choice;
                        tabs_focus = false;
                        continue;
                    }
                    switch_to_quests = true;
                    break;
                }
                else if (clicked_choice == HINT_QUEST_CLICK_HINTS_TAB)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                    {
                        hover_tab = clicked_choice;
                        tabs_focus = false;
                        continue;
                    }
                    continue;
                }
                else if (clicked_choice >= HINT_MESSAGE_CLICK_ENTRY_BASE)
                {
                    int clicked_idx =
                        clicked_choice - HINT_MESSAGE_CLICK_ENTRY_BASE;
                    if (clicked_idx >= 0 && clicked_idx < n)
                    {
                        if (click_action == UI_MENU_CLICK_HOVER)
                        {
                            hover_tab = 0;
                            tabs_focus = false;
                        }
                        sel = clicked_idx;
                        if (click_action == UI_MENU_CLICK_HOVER)
                            continue;
                        ch = '\r';
                        click_generated_command = true;
                    }
                    else if (click_action == UI_MENU_CLICK_HOVER)
                    {
                        tabs_focus = false;
                        continue;
                    }
                }
                else
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                    {
                        hover_tab = 0;
                        tabs_focus = false;
                        continue;
                    }

                    switch (clicked_choice)
                    {
                    case HINT_MESSAGE_CLICK_TOGGLE_TIPS:
                        ch = 'h';
                        click_generated_command = true;
                        break;
                    case HINT_MESSAGE_CLICK_LOOK:
                        ch = 'l';
                        click_generated_command = true;
                        break;
                    case HINT_MESSAGE_CLICK_MAP:
                        ch = 'm';
                        click_generated_command = true;
                        break;
                    case HINT_MESSAGE_CLICK_BACK:
                        ch = ESCAPE;
                        click_generated_command = true;
                        break;
                    case HINT_MESSAGE_CLICK_CONTINUE:
                        ch = '\r';
                        click_generated_command = true;
                        break;
                    default:
                        break;
                    }
                }
            }
            else if (ch == UI_MENU_CLICK_WAKE_KEY)
            {
                continue;
            }
        }

        if (!click_generated_command)
            ch = (char)steamdeck_menu_key(ch, '\t', '\t');

        if (switch_to_quests)
            break;

        if (hint_quest_tab_key(ch))
        {
            switch_to_quests = true;
            break;
        }

        if (hint_quest_handle_tab_navigation(ch, &tabs_focus,
                (n <= 0) || (sel == 0), &switch_to_quests))
        {
            if (switch_to_quests)
                break;
            continue;
        }

        if (ch == ESCAPE || (steamdeck && ch == steamdeck_back_key()))
            break;

        if (ch == 'h' || ch == 'H'
            || (steamdeck && ch == steamdeck_secondary_key()))
        {
            if (tip_n <= 0)
            {
                bell(NULL);
                continue;
            }

            show_all_tips = !show_all_tips;
            sel = 0;
            top = 0;
            continue;
        }

        if (n <= 0)
        {
            bell(NULL);
            continue;
        }

        if (ch == '8')
        {
            sel = (sel > 0) ? (sel - 1) : (n - 1);
            continue;
        }

        if (ch == '2')
        {
            sel = (sel + 1 < n) ? (sel + 1) : 0;
            continue;
        }

        if ((ch == '\r') || (ch == '\n') || (ch == ' ') || (ch == '6')
            || (steamdeck && ch == steamdeck_confirm_key()))
        {
            int selected_look_y = -1;
            int selected_look_x = -1;
            hint_message_action action = HINT_MESSAGE_ACTION_NONE;

            if (show_all_tips)
            {
                (void)skeleton_tip_show_internal(sel, false);
            }
            else
            {
                action = hint_message_show_internal(sel, &selected_look_y,
                    &selected_look_x, false);
                if (action == HINT_MESSAGE_ACTION_LOOK)
                {
                    pending_look = true;
                    look_y = selected_look_y;
                    look_x = selected_look_x;
                    break;
                }
                if (action == HINT_MESSAGE_ACTION_MAP)
                {
                    pending_map = true;
                    map_y = selected_look_y;
                    map_x = selected_look_x;
                    break;
                }
            }
            continue;
        }

        if (ch == 'l' || ch == 'L'
            || (steamdeck && ch == steamdeck_alt_action_key()))
        {
            hint_message_meta meta;

            if (show_all_tips)
            {
                bell(NULL);
                continue;
            }

            hint_messages_message_meta(sel, &meta);
            if (hint_message_has_source(&meta))
            {
                pending_look = true;
                look_y = meta.source_y;
                look_x = meta.source_x;
                break;
            }

            bell(NULL);
            continue;
        }

        if (ch == 'm' || ch == 'M')
        {
            hint_message_meta meta;

            if (show_all_tips)
            {
                bell(NULL);
                continue;
            }

            hint_messages_message_meta(sel, &meta);
            if (hint_message_has_source(&meta))
            {
                pending_map = true;
                map_y = meta.source_y;
                map_x = meta.source_x;
                break;
            }

            bell(NULL);
            continue;
        }

        bell(NULL);
    }

    ui_menu_click_clear();
    ui_scroll_area_clear();

    if (manage_screen)
    {
        screen_pop_supporting_panes_hidden();
        screen_load();
    }

    if (out_pending_look)
        *out_pending_look = pending_look;
    if (out_look_y)
        *out_look_y = look_y;
    if (out_look_x)
        *out_look_x = look_x;
    if (out_pending_map)
        *out_pending_map = pending_map;
    if (out_map_y)
        *out_map_y = map_y;
    if (out_map_x)
        *out_map_x = map_x;

    return switch_to_quests;
}

/*
 * Show previous messages to the user
 *
 * The screen format uses line 0 and 23 for headers and prompts,
 * skips line 1 and 22, and uses line 2 thru 21 for old messages.
 *
 * This command shows you which commands you are viewing, and allows
 * you to "search" for strings in the recall.
 *
 * Note that messages may be longer than 80 characters, but they are
 * displayed using "infinite" length, with a special sub-command to
 * "slide" the virtual display to the left or right.
 *
 * Attempt to only hilite the matching portions of the string.
 */
void do_cmd_messages_with_filter(int initial_filter)
{
    char ch;

    int i, j, n, q;
    int wid, hgt;
    char prompt[160];

    char shower[80];
    char finder[80];
    int filter = log_history_clamp_filter(initial_filter);
    int hover_filter = -1;

    /* Clear any active banner before opening message history */
    if (dismiss_active_narrative_banner()) {
        do_cmd_redraw();
    }

    /* Wipe finder */
    SDL_strlcpy(finder, "", sizeof(finder));

    /* Wipe shower */
    SDL_strlcpy(shower, "", sizeof(shower));

    /* Total messages */
    n = 0;

    /* Start on first message */
    i = 0;

    /* Start at leftmost edge */
    q = 0;

    /* Save screen */
    screen_save();
    screen_push_supporting_panes_hidden();
    sdl_push_terminal_menu_scale();

    /* Get size after any hidden-pane layout change */
    Term_get_size(&wid, &hgt);

    /* Process requests until done */
    while (1)
    {
        int body_top;
        int body_bottom;
        int visible_rows;
        int max_i;
        int page_rows;
        int range_first;
        int range_last;
        int old_i;
        int old_q;

        /* Clear screen */
        Term_clear();

        body_top = 3;
        body_bottom = hgt - 3;
        if (body_bottom < body_top)
        {
            body_top = 2;
            body_bottom = hgt - 2;
        }
        if (body_bottom < body_top)
        {
            body_top = 0;
            body_bottom = hgt - 1;
        }

        visible_rows = body_bottom - body_top + 1;
        if (visible_rows < 1)
            visible_rows = 1;

        bool notes_mode = (filter == LOG_HISTORY_FILTER_NOTES);

        if (notes_mode)
            n = log_history_collect_notes();
        else
            n = log_history_collect_entries(log_history_entries,
                LOG_HISTORY_MAX_ENTRIES, filter);

        max_i = (n > visible_rows) ? (n - visible_rows) : 0;
        if (i > max_i)
            i = max_i;
        if (i < 0)
            i = 0;

        page_rows = (visible_rows > 1) ? (visible_rows - 1) : 1;
        ui_scroll_area_begin(body_top, body_bottom,
            SDL_TOUCH_MENU_CATEGORY_OTHER);
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_touch_exit_button(true);

        /* Dump log entries */
        for (j = 0; (j < visible_rows) && (i + j < n); j++)
        {
            log_history_entry* entry;
            int line_y = notes_mode ? (body_top + j) : (body_bottom - j);

            if (notes_mode)
            {
                const log_history_note_line* nl =
                    &log_history_note_lines[i + j];
                char line_buf[256];
                int len = nl->len;
                cptr msg;

                if (len > (int)sizeof(line_buf) - 1)
                    len = (int)sizeof(line_buf) - 1;
                memcpy(line_buf, nl->text, len);
                line_buf[len] = '\0';

                /* Apply horizontal scroll */
                msg = ((int)strlen(line_buf) >= q) ? (line_buf + q) : "";

                Term_putstr(0, line_y, -1, TERM_WHITE, msg);

                /* Hilite "shower" */
                if (shower[0])
                {
                    cptr str = msg;

                    while ((str = strstr(str, shower)) != NULL)
                    {
                        int slen = strlen(shower);

                        Term_putstr(str - msg, line_y, slen, TERM_YELLOW,
                            shower);
                        str += slen;
                    }
                }

                continue;
            }

            entry = &log_history_entries[i + j];

            if (entry->kind == LOG_HISTORY_ENTRY_COMBAT)
            {
                log_history_draw_combat_entry(entry, line_y, q);
                continue;
            }
            else
            {
                cptr msg = message_str(entry->message_age);
                byte attr = message_color(entry->message_age);

                /* Apply horizontal scroll */
                msg = ((int)strlen(msg) >= q) ? (msg + q) : "";

                /* Dump the messages, bottom to top */
                Term_putstr(0, line_y, -1, attr, msg);

                /* Hilite "shower" */
                if (shower[0])
                {
                    cptr str = msg;

                    /* Display matches */
                    while ((str = strstr(str, shower)) != NULL)
                    {
                        int len = strlen(shower);

                        /* Display the match */
                        Term_putstr(
                            str - msg, line_y, len, TERM_YELLOW, shower);

                        /* Advance */
                        str += len;
                    }
                }
            }
        }

        range_first = (n > 0) ? (i + 1) : 0;
        range_last = (n > 0) ? (i + j) : 0;

        log_history_draw_filters(filter, hover_filter, wid);

        /* Display header XXX XXX XXX */
        prt(format(
                "Log (%s, %d-%d of %d), Offset %d",
                log_history_filter_label(filter), range_first, range_last, n,
                q),
            (body_top > 0) ? body_top - 1 : 0, 0);

        /* Display prompt */
        {
            if (steamdeck_controls_active())
            {
                char prev_label[16];
                char next_label[16];
                char back_label[16];
                char prompt_full[160];
                char prompt_mid[128];
                char prompt_short[80];
                const char* variants[3];

                controller_prompt_label(steamdeck_prev_page_key(), "L1",
                    prev_label, sizeof(prev_label));
                controller_prompt_label(steamdeck_next_page_key(), "R1",
                    next_label, sizeof(next_label));
                controller_prompt_label(steamdeck_back_key(), "B",
                    back_label, sizeof(back_label));
                strnfmt(prompt_full, sizeof(prompt_full),
                    "%s/%s page  Up/Down line  / find  = highlight  Left/Right pan  %s back",
                    prev_label, next_label, back_label);
                strnfmt(prompt_mid, sizeof(prompt_mid),
                    "%s/%s page  Up/Down line  / find  %s back",
                    prev_label, next_label, back_label);
                strnfmt(prompt_short, sizeof(prompt_short),
                    "%s/%s page  %s back", prev_label, next_label, back_label);
                variants[0] = prompt_full;
                variants[1] = prompt_mid;
                variants[2] = prompt_short;
                terminal_prompt_pick_variant(prompt, sizeof(prompt), wid, false,
                    variants, N_ELEMENTS(variants));
            }
            else if (sdl_touch_only_device_active())
            {
                const char* variants[] = {
                    "Tap a tab to filter, drag to scroll",
                    "Tap a tab, drag to scroll",
                    "Tap a tab to filter"
                };
                terminal_prompt_pick_variant(prompt, sizeof(prompt), wid, false,
                    variants, N_ELEMENTS(variants));
            }
            else
            {
                const char* variants[] = {
                    "e/i filter  Up/Down line  PgUp/PgDn page  / find  = highlight  Left/Right pan  Esc",
                    "e/i filter  Up/Down line  Pg page  / find  Left/Right pan  Esc",
                    "e/i filter  / find  Left/Right pan  Esc",
                    "Esc"
                };
                terminal_prompt_pick_variant(prompt, sizeof(prompt), wid, false,
                    variants, N_ELEMENTS(variants));
            }
        }
        prt(prompt, hgt - 1, 0);
        ui_menu_click_add_text_token('8', 0, hgt - 1, prompt, "Up");
        ui_menu_click_add_text_token('2', 0, hgt - 1, prompt, "Down");
        ui_menu_click_add_text_token('9', 0, hgt - 1, prompt, "PgUp");
        ui_menu_click_add_text_token('3', 0, hgt - 1, prompt, "PgDn");
        ui_menu_click_add_text_token('i', 0, hgt - 1, prompt, "filter");
        ui_menu_click_add_text_token('i', 0, hgt - 1, prompt, "e/i");
        ui_menu_click_add_text_token('/', 0, hgt - 1, prompt, "/");
        ui_menu_click_add_text_token('/', 0, hgt - 1, prompt, "find");
        ui_menu_click_add_text_token('=', 0, hgt - 1, prompt, "=");
        ui_menu_click_add_text_token('=', 0, hgt - 1, prompt, "highlight");
        ui_menu_click_add_text_token('4', 0, hgt - 1, prompt, "Left");
        ui_menu_click_add_text_token('6', 0, hgt - 1, prompt, "Right");
        ui_menu_click_add_text_token(ESCAPE, 0, hgt - 1, prompt, "Esc");

        /* Get a command without showing the terminal cursor */
        (void)Term_set_cursor(false);
        Term_fresh();
        {
            bool saved_hide_cursor = hide_cursor;
            hide_cursor = true;
            ch = inkey();
            hide_cursor = saved_hide_cursor;
        }

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if ((clicked_choice == LOG_HISTORY_CLICK_FILTER_ALL)
                    || (clicked_choice == LOG_HISTORY_CLICK_FILTER_MESSAGES)
                    || (clicked_choice == LOG_HISTORY_CLICK_FILTER_COMBAT)
                    || (clicked_choice == LOG_HISTORY_CLICK_FILTER_NOTES))
                {
                    int clicked_filter =
                        log_history_click_to_filter(clicked_choice);

                    if (click_action == UI_MENU_CLICK_HOVER)
                    {
                        hover_filter = clicked_filter;
                        continue;
                    }
                    filter = clicked_filter;
                    i = 0;
                    q = 0;
                    continue;
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                {
                    hover_filter = -1;
                    continue;
                }
                else
                    ch = (char)clicked_choice;
            }
            else if (ch == UI_MENU_CLICK_WAKE_KEY)
            {
                continue;
            }
        }
        ch = (char)steamdeck_menu_key(ch, 'p', 'n');

        if (log_history_filter_key(ch))
        {
            filter = log_history_next_filter(filter);
            i = 0;
            q = 0;
            continue;
        }

        /* Exit on Escape */
        if (ch == ESCAPE)
            break;

        /* Hack -- Save the old index */
        old_i = i;
        old_q = q;

        /*
         * The notes tab reads top-down like a document, so flip the vertical
         * navigation keys: there "down" means a larger index (later notes),
         * the opposite of the bottom-anchored message/combat log.
         */
        if (notes_mode)
        {
            switch (ch)
            {
            case '8': ch = '2'; break;
            case '2': ch = '8'; break;
            case '9': ch = '3'; break;
            case '3': ch = '9'; break;
            case '7': ch = '1'; break;
            case '1': ch = '7'; break;
            default: break;
            }
        }

        /* Horizontal scroll */
        if (ch == '4')
        {
            /* Scroll left */
            q = (q >= wid / 2) ? (q - wid / 2) : 0;

            /* Success */
            continue;
        }

        /* Horizontal scroll */
        if (ch == '6')
        {
            /* Scroll right */
            q = q + wid / 2;

            /* Success */
            continue;
        }

        /* Hack -- handle show */
        if (ch == '=')
        {
            ui_menu_click_clear();
            ui_scroll_area_clear();

            /* Prompt */
            prt("Highlight: ", hgt - 1, 0);

            /* Get a "shower" string, or continue */
            if (!askfor_aux(shower, sizeof(shower)))
                continue;

            /* Okay */
            continue;
        }

        /* Hack -- handle find */
        if (ch == '/')
        {
            s16b z;

            ui_menu_click_clear();
            ui_scroll_area_clear();

            /* Prompt */
            prt("Find: ", hgt - 1, 0);

            /* Get a "finder" string, or continue */
            if (!askfor_aux(finder, sizeof(finder)))
                continue;

            /* Show it */
            SDL_strlcpy(shower, finder, sizeof(shower));

            /* Scan log entries */
            for (z = i + 1; z < n; z++)
            {
                char search_text[160];

                if (notes_mode)
                {
                    const log_history_note_line* nl =
                        &log_history_note_lines[z];
                    int len = nl->len;

                    if (len > (int)sizeof(search_text) - 1)
                        len = (int)sizeof(search_text) - 1;
                    memcpy(search_text, nl->text, len);
                    search_text[len] = '\0';
                }
                else
                {
                    log_history_entry_search_text(&log_history_entries[z],
                        search_text, sizeof(search_text));
                }

                /* Search for it */
                if (strstr(search_text, finder))
                {
                    /* New location */
                    i = z;

                    /* Done */
                    break;
                }
            }
        }

        /* Scroll one older message */
        if (ch == '8')
        {
            if (i < max_i)
                i += 1;
        }

        /* Scroll one newer message */
        if (ch == '2')
        {
            if (i > 0)
                i -= 1;
        }

        /* Page older */
        if (ch == '9')
        {
            i += page_rows;
            if (i > max_i)
                i = max_i;
        }

        /* Page newer */
        if (ch == '3')
        {
            i -= page_rows;
            if (i < 0)
                i = 0;
        }

        /* Jump to oldest visible messages */
        if (ch == '7')
        {
            i = max_i;
        }

        /* Jump to newest messages */
        if (ch == '1')
        {
            i = 0;
        }

        /* Recall one page of older messages */
        if ((ch == 'p') || (ch == KTRL('P')) || (ch == ' '))
        {
            /* Go older if legal */
            i += page_rows;
            if (i > max_i)
                i = max_i;
        }

        /* Recall 10 older messages */
        if (ch == '+')
        {
            /* Go older if legal */
            if (i + 10 < max_i)
                i += 10;
            else
                i = max_i;
        }

        /* Recall one page of newer messages */
        if ((ch == 'n') || (ch == KTRL('N')))
        {
            /* Go newer (if able) */
            i = (i >= page_rows) ? (i - page_rows) : 0;
        }

        /* Recall 10 newer messages */
        if (ch == '-')
        {
            /* Go newer (if able) */
            i = (i >= 10) ? (i - 10) : 0;
        }

        /* Hack -- Error of some kind */
        if (i == old_i && q == old_q)
            bell(NULL);
    }

    ui_scroll_area_clear();
    ui_menu_click_clear();

    /* Load screen */
    sdl_pop_terminal_menu_scale();
    screen_pop_supporting_panes_hidden();
    screen_load();
}

void do_cmd_messages(void)
{
    do_cmd_messages_with_filter(LOG_HISTORY_FILTER_ALL);
}

/*
 * Ask for a "user pref line" and process it
 */
