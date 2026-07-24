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
#include "score/score_postmortem.h"
#include "pane.h"
#include "cmd/ui/cmd-ui-internal.h"
#include "ui/story_font.h"

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
    case MAIN_MENU_SMITHING: return "Smithing";
    case MAIN_MENU_KNOWLEDGE: return "Known lore";
    case MAIN_MENU_HINTS_QUESTS: return "Hints & Quests";
    case MAIN_MENU_HALLS_OF_MANDOS: return "Halls of Mandos";
    case MAIN_MENU_MAP: return "Map";
    case MAIN_MENU_LOG_HISTORY: return "Log & combat history";
    case MAIN_MENU_STORY: return "The tale so far";
    case MAIN_MENU_STORY_STATS: return "Tale statistics";
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
    case MAIN_MENU_SMITHING: return 'f';
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
    case 'f': return MAIN_MENU_SMITHING;
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

static hint_quest_page do_cmd_hint_messages(bool* out_pending_look,
    int* out_look_y,
    int* out_look_x, bool* out_pending_map, int* out_map_y,
    int* out_map_x);
static hint_quest_page do_cmd_thrall_quests(bool* out_pending_look,
    int* out_look_y, int* out_look_x, bool* out_pending_map,
    int* out_map_y, int* out_map_x);

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

static void log_history_wrapped_entry_text(int filter, int idx, char* out,
    size_t out_sz, byte* attr)
{
    if (!out || out_sz == 0 || !attr)
        return;

    out[0] = '\0';
    *attr = TERM_WHITE;

    if (filter == LOG_HISTORY_FILTER_NOTES)
    {
        const log_history_note_line* nl = &log_history_note_lines[idx];
        int len = MIN(nl->len, (int)out_sz - 1);

        memcpy(out, nl->text, len);
        out[len] = '\0';
        return;
    }

    if (log_history_entries[idx].kind == LOG_HISTORY_ENTRY_MESSAGE)
    {
        s16b age = log_history_entries[idx].message_age;

        SDL_strlcpy(out, message_str(age), out_sz);
        *attr = message_color(age);
        return;
    }

    log_history_entry_search_text(&log_history_entries[idx], out, out_sz);
}

static int log_history_wrapped_entry_rows(int filter, int idx, int width)
{
    char text[256];
    byte attr;

    log_history_wrapped_entry_text(filter, idx, text, sizeof(text), &attr);
    return count_wrapped_lines(text, width, 0);
}

static void log_history_draw_wrapped_text(int row, int width, byte attr,
    cptr text, cptr highlight)
{
    int old_wrap = text_out_wrap;
    int old_indent = text_out_indent;
    cptr cursor = text ? text : "";

    text_out_wrap = width;
    text_out_indent = 0;
    Term_gotoxy(0, row);

    while (highlight && highlight[0])
    {
        cptr match = strstr(cursor, highlight);
        char prefix[256];
        int prefix_len;

        if (!match)
            break;
        prefix_len = MIN((int)(match - cursor), (int)sizeof(prefix) - 1);
        memcpy(prefix, cursor, prefix_len);
        prefix[prefix_len] = '\0';
        text_out_to_screen(attr, prefix);
        text_out_to_screen(TERM_YELLOW, highlight);
        cursor = match + strlen(highlight);
    }

    text_out_to_screen(attr, cursor);
    text_out_wrap = old_wrap;
    text_out_indent = old_indent;
}

static int log_history_wrapped_last_page_start(int filter, int count,
    int width, int available_rows)
{
    int top = count;
    int used_rows = 0;

    while (top > 0)
    {
        int rows = log_history_wrapped_entry_rows(filter, top - 1, width);

        if (used_rows > 0 && used_rows + rows > available_rows)
            break;
        top--;
        used_rows += rows;
        if (used_rows >= available_rows)
            break;
    }

    return top;
}

static bool hint_quest_tab_key(char ch)
{
    return (ch == '\t');
}

static hint_quest_page hint_quest_adjacent_page(hint_quest_page page,
    int direction)
{
    if (direction < 0)
    {
        if (page == HINT_QUEST_PAGE_HINTS)
            return HINT_QUEST_PAGE_THRALLS;
        return (hint_quest_page)(page - 1);
    }

    if (page == HINT_QUEST_PAGE_THRALLS)
        return HINT_QUEST_PAGE_HINTS;
    return (hint_quest_page)(page + 1);
}

static bool hint_quest_handle_tab_navigation(char ch, bool* tabs_focus,
    bool can_focus_tabs, hint_quest_page current_page,
    hint_quest_page* next_page)
{
    int d = target_dir(ch);

    if (!tabs_focus || !next_page)
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
            *next_page = hint_quest_adjacent_page(current_page, ddx[d]);
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

static void do_cmd_hint_quest_menu(hint_quest_page initial_page,
    bool* out_pending_look, int* out_look_y, int* out_look_x,
    bool* out_pending_map, int* out_map_y, int* out_map_x)
{
    hint_quest_page page = initial_page;
    bool pending_look = false;
    int look_y = -1;
    int look_x = -1;
    bool pending_map = false;
    int map_y = -1;
    int map_x = -1;

    screen_save();
    screen_push_supporting_panes_hidden();

    while (page != HINT_QUEST_PAGE_EXIT)
    {
        if (page == HINT_QUEST_PAGE_QUESTS)
        {
            page = do_cmd_quest_status_page();
        }
        else if (page == HINT_QUEST_PAGE_THRALLS)
        {
            page = do_cmd_thrall_quests(&pending_look, &look_y, &look_x,
                &pending_map, &map_y, &map_x);
        }
        else
        {
            page = do_cmd_hint_messages(&pending_look, &look_y, &look_x,
                &pending_map, &map_y, &map_x);
        }

        if (pending_look || pending_map)
            break;
    }

    sdl_hint_quest_menu_hide();
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
    /* Hide panes BEFORE pushing the menu scale so it is measured from the full
     * screen, matching the inventory/supply menu rather than the smaller
     * window-mode scale. */
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
        "tale.");
    main_menu_blitz_text_line(&row, wrap, TERM_WHITE,
        "Use it to play outside of your metaprogress, or to practise freely: "
        "nothing you do in Blitz touches your normal tale, saves or score.");
    main_menu_blitz_text_line(&row, wrap, TERM_WHITE,
        "Blitz keeps its own separate character and score files, so your tale "
        "character stays safe and can be resumed at any time.");
    main_menu_blitz_text_line(&row, wrap, TERM_WHITE,
        "If you already have a living Blitz character, it will be resumed.");
    main_menu_blitz_text_line(&row, wrap, TERM_SLATE,
        "Switching to Blitz will save your current tale game first.");

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
    case MAIN_MENU_SMITHING: // Smithing (f)
    {
        do_cmd_smithing_screen();
        return true;
    }
    case MAIN_MENU_KNOWLEDGE: // Known lore (a)
    {
        do_cmd_knowledge_browser_page(g_knowledge_last_page);
        return true;
    }
    case MAIN_MENU_HINTS_QUESTS: // Hints & Quests (t)
    {
        do_cmd_hint_quest_menu(HINT_QUEST_PAGE_HINTS, pending_hint_look,
            pending_hint_look_y, pending_hint_look_x, pending_hint_map,
            pending_hint_map_y, pending_hint_map_x);
        return true;
    }
    case MAIN_MENU_HALLS_OF_MANDOS: // Halls of Mandos (d)
    {
        log_info("main menu: opening Halls of Mandos view");
        screen_save();
        screen_push_supporting_panes_hidden();
        if (death_spectator_active())
        {
            high_score final_score;
            const char* postmortem_path = score_postmortem_path();

            if (create_score(&final_score) == 0)
            {
                if (postmortem_path[0])
                {
                    show_scores_interactive_highlight_from_file(
                        postmortem_path, &final_score);
                }
                else
                    show_scores_interactive_highlight(&final_score);
            }
            else
                show_scores_interactive();
        }
        else
            show_scores_interactive();
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
    case MAIN_MENU_STORY: // The tale so far (y)
    {
        /* Save screen before showing story */
        screen_save();
        print_story(15, 1);
        /* Load screen after story */
        screen_load();
        return true;
    }
    case MAIN_MENU_STORY_STATS: // Tale statistics (g)
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
        do_cmd_help_menu();
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
        sdl_minimap_focus(pending_hint_map_y, pending_hint_map_x);
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
    sdl_main_menu_overlay_begin();
}

static bool hint_message_has_source(const hint_message_meta* meta)
{
    return meta && meta->source_y >= 0 && meta->source_x >= 0
        && meta->source_y < p_ptr->cur_map_hgt && meta->source_x < p_ptr->cur_map_wid;
}

typedef enum hint_message_action {
    HINT_MESSAGE_ACTION_NONE = 0,
    HINT_MESSAGE_ACTION_LOOK,
    HINT_MESSAGE_ACTION_MAP,
    HINT_MESSAGE_ACTION_OPEN_HINTS,
    HINT_MESSAGE_ACTION_OPEN_QUESTS,
    HINT_MESSAGE_ACTION_OPEN_THRALLS
} hint_message_action;

enum {
    HINT_MESSAGE_CLICK_TOGGLE_TIPS = -1,
    HINT_MESSAGE_CLICK_BACK = -2,
    HINT_MESSAGE_CLICK_LOOK = -3,
    HINT_MESSAGE_CLICK_MAP = -4,
    HINT_MESSAGE_CLICK_CONTINUE = -5,
    HINT_MESSAGE_CLICK_ENTRY_BASE = 1000
};

static hint_message_action hint_message_tab_action(int choice)
{
    switch (choice)
    {
    case HINT_QUEST_CLICK_HINTS_TAB:
        return HINT_MESSAGE_ACTION_OPEN_HINTS;
    case HINT_QUEST_CLICK_QUESTS_TAB:
        return HINT_MESSAGE_ACTION_OPEN_QUESTS;
    case HINT_QUEST_CLICK_THRALLS_TAB:
        return HINT_MESSAGE_ACTION_OPEN_THRALLS;
    default:
        return HINT_MESSAGE_ACTION_NONE;
    }
}

static hint_quest_page hint_message_action_page(hint_message_action action)
{
    switch (action)
    {
    case HINT_MESSAGE_ACTION_OPEN_HINTS:
        return HINT_QUEST_PAGE_HINTS;
    case HINT_MESSAGE_ACTION_OPEN_QUESTS:
        return HINT_QUEST_PAGE_QUESTS;
    case HINT_MESSAGE_ACTION_OPEN_THRALLS:
        return HINT_QUEST_PAGE_THRALLS;
    default:
        return HINT_QUEST_PAGE_EXIT;
    }
}

static void hint_message_open_map_at(int y, int x)
{
    sdl_minimap_focus(y, x);
    do_cmd_view_map();
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

static void hint_message_body_text(int index, char* out, size_t out_sz)
{
    byte line_count = hint_messages_message_line_count(index);
    bool skipped_title = false;
    char platform_text[1024];

    if (!out || out_sz == 0)
        return;
    out[0] = '\0';

    for (int li = 0; li < line_count; ++li)
    {
        const char* source = hint_messages_message_line(index, li);
        char line[100];
        size_t len;

        if (!source)
            continue;
        while (*source == ' ' || *source == '\t')
            source++;
        strnfmt(line, sizeof(line), "%s", source);
        len = strlen(line);
        while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t'))
            line[--len] = '\0';
        if (!line[0])
            continue;

        if (!skipped_title)
        {
            skipped_title = true;
            continue;
        }
        if (out[0])
            SDL_strlcat(out, " ", out_sz);
        SDL_strlcat(out, line, out_sz);
    }

    hint_text_for_current_platform(out, platform_text, sizeof(platform_text));
    SDL_strlcpy(out, platform_text, out_sz);
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
    char canonical_text[1024];

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
        strnfmt(canonical_text, sizeof(canonical_text), "%s %s", main_text,
            extra_text);
    }
    else
    {
        strnfmt(canonical_text, sizeof(canonical_text), "%s", main_text);
    }

    hint_text_for_current_platform(canonical_text, buf, buf_sz);
    return (buf[0] != '\0');
}

static hint_message_action skeleton_tip_show(int index)
{
    char tip_text[512];
    char ch;
    hint_message_action action = HINT_MESSAGE_ACTION_NONE;

    if (!skeleton_tip_text_by_index(index, tip_text, sizeof(tip_text)))
        return HINT_MESSAGE_ACTION_NONE;

    while (1)
    {
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);
        sdl_hint_quest_menu_begin(HINT_QUEST_PAGE_HINTS, "Hints & Quests",
            "Survival Tip", true, true, 0);
        sdl_hint_quest_menu_add_block(tip_text, TERM_WHITE, 0, 0);
        sdl_hint_quest_menu_add_button(HINT_MESSAGE_CLICK_CONTINUE,
            "Continue", TERM_L_WHITE);
        sdl_hint_quest_menu_add_button(HINT_MESSAGE_CLICK_BACK, "Back",
            TERM_L_WHITE);
        sdl_hint_quest_menu_finish();
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

                action = hint_message_tab_action(clicked_choice);
                break;
            }
            if (ch == UI_MENU_CLICK_WAKE_KEY)
                continue;
        }
        ch = (char)steamdeck_menu_key(ch, 0, 0);
        if (hint_quest_tab_key(ch))
            action = HINT_MESSAGE_ACTION_OPEN_QUESTS;
        break;
    }

    ui_menu_click_clear();
    if (action == HINT_MESSAGE_ACTION_NONE
        || action == HINT_MESSAGE_ACTION_OPEN_HINTS)
    {
        sdl_hint_quest_menu_prepare_leaf_turn(HINT_QUEST_PAGE_HINTS, -1);
    }
    return action;
}

static hint_message_action hint_message_show_internal(int index, int* source_y, int* source_x,
    bool standalone)
{
    char ch;
    char body_text[768];
    hint_message_meta meta;
    byte stored_line_count;
    hint_message_action action = HINT_MESSAGE_ACTION_NONE;
    bool steamdeck = steamdeck_controls_active();

    hint_messages_ensure_level_state();
    stored_line_count = hint_messages_message_line_count(index);
    if (!stored_line_count)
        return HINT_MESSAGE_ACTION_NONE;

    hint_messages_message_meta(index, &meta);
    hint_message_body_text(index, body_text, sizeof(body_text));
    if (standalone)
        screen_save();

    while (1)
    {
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);
        sdl_hint_quest_menu_begin(HINT_QUEST_PAGE_HINTS, "Hints & Quests",
            "Hint Message", true, true, 0);
        sdl_hint_quest_menu_add_block(
            hint_message_title(index), TERM_L_WHITE, 0, 0);
        sdl_hint_quest_menu_add_block(body_text, TERM_WHITE, 0, 0);
        sdl_hint_quest_menu_add_button(HINT_MESSAGE_CLICK_CONTINUE,
            "Continue", TERM_L_WHITE);
        if (hint_message_has_source(&meta))
        {
            sdl_hint_quest_menu_add_button(HINT_MESSAGE_CLICK_LOOK, "Look",
                TERM_L_BLUE);
            sdl_hint_quest_menu_add_button(HINT_MESSAGE_CLICK_MAP, "Map",
                TERM_L_BLUE);
        }
        sdl_hint_quest_menu_add_button(HINT_MESSAGE_CLICK_BACK, "Back",
            TERM_L_WHITE);
        sdl_hint_quest_menu_finish();
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

                action = hint_message_tab_action(clicked_choice);
                if (action != HINT_MESSAGE_ACTION_NONE)
                    break;

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

        if (hint_quest_tab_key(ch))
        {
            action = HINT_MESSAGE_ACTION_OPEN_QUESTS;
            break;
        }

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
    if (standalone || action == HINT_MESSAGE_ACTION_LOOK
        || action == HINT_MESSAGE_ACTION_MAP)
    {
        sdl_hint_quest_menu_hide();
    }
    else if (action == HINT_MESSAGE_ACTION_NONE
        || action == HINT_MESSAGE_ACTION_OPEN_HINTS)
    {
        sdl_hint_quest_menu_prepare_leaf_turn(HINT_QUEST_PAGE_HINTS, -1);
    }
    if (standalone)
        screen_load();

    return action;
}

void show_hint_message_screen(int index)
{
    int look_y = -1;
    int look_x = -1;
    hint_message_action action;
    hint_quest_page page;

    action = hint_message_show_internal(index, &look_y, &look_x, true);
    page = hint_message_action_page(action);
    if (page != HINT_QUEST_PAGE_EXIT)
    {
        bool pending_look = false;
        bool pending_map = false;
        int pending_y = -1;
        int pending_x = -1;
        int pending_map_y = -1;
        int pending_map_x = -1;

        do_cmd_hint_quest_menu(page, &pending_look, &pending_y, &pending_x,
            &pending_map, &pending_map_y, &pending_map_x);
        if (pending_map)
        {
            action = HINT_MESSAGE_ACTION_MAP;
            look_y = pending_map_y;
            look_x = pending_map_x;
        }
        else if (pending_look)
        {
            action = HINT_MESSAGE_ACTION_LOOK;
            look_y = pending_y;
            look_x = pending_x;
        }
        else
        {
            action = HINT_MESSAGE_ACTION_NONE;
        }
    }

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

static void hint_message_pixel_list_text(int index, char* out,
    size_t out_sz)
{
    hint_message_meta meta;

    if (!out || !out_sz)
        return;
    strnfmt(out, out_sz, "%d. %s", index + 1, hint_message_title(index));
    hint_messages_message_meta(index, &meta);
    if (meta.cue_count <= 0)
        return;

    SDL_strlcat(out, "  [", out_sz);
    for (int cue = 0; cue < meta.cue_count; ++cue)
    {
        if (cue > 0)
            SDL_strlcat(out, "; ", out_sz);
        if (meta.cue_dists[cue][0])
            SDL_strlcat(out, meta.cue_dists[cue], out_sz);
        if (meta.cue_dists[cue][0] && meta.cue_dirs[cue][0])
            SDL_strlcat(out, " ", out_sz);
        if (meta.cue_dirs[cue][0])
            SDL_strlcat(out, meta.cue_dirs[cue], out_sz);
    }
    SDL_strlcat(out, "]", out_sz);
}

static hint_quest_page do_cmd_hint_messages(bool* out_pending_look,
    int* out_look_y,
    int* out_look_x, bool* out_pending_map, int* out_map_y,
    int* out_map_x)
{
    char ch;
    bool pending_look = false;
    int look_y = -1;
    int look_x = -1;
    bool pending_map = false;
    int map_y = -1;
    int map_x = -1;
    hint_quest_page next_page = HINT_QUEST_PAGE_EXIT;
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
    show_all_tips = false;

    while (1)
    {
        int n = show_all_tips ? tip_n : level_n;
        char section[64];

        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);

        if (n > 0)
        {
            if (sel < 0)
                sel = 0;
            if (sel >= n)
                sel = n - 1;
        }
        else
        {
            sel = 0;
        }

        if (show_all_tips)
            strnfmt(section, sizeof(section), "All Tutorial Hints (%d)",
                tip_n);
        else
            strnfmt(section, sizeof(section), "Hint Messages (%d)", level_n);

        sdl_hint_quest_menu_begin(HINT_QUEST_PAGE_HINTS, "Hints & Quests",
            section, true, false,
            n > 0 ? HINT_MESSAGE_CLICK_ENTRY_BASE + sel : 0);

        if (n <= 0)
        {
            sdl_hint_quest_menu_add_block(
                show_all_tips ? "No tutorial hints are available."
                    : "You recall no hint messages on this level.",
                TERM_SLATE, 0, 0);
        }
        for (int idx = 0; idx < n; ++idx)
        {
            char entry[1024];

            if (show_all_tips)
            {
                char tip[768];

                if (!skeleton_tip_text_by_index(idx, tip, sizeof(tip)))
                    tip[0] = '\0';
                strnfmt(entry, sizeof(entry), "%d. %s", idx + 1, tip);
            }
            else
                hint_message_pixel_list_text(idx, entry, sizeof(entry));
            sdl_hint_quest_menu_add_block(entry, TERM_WHITE, 0,
                HINT_MESSAGE_CLICK_ENTRY_BASE + idx);
        }

        if (tip_n > 0)
            sdl_hint_quest_menu_add_button(HINT_MESSAGE_CLICK_TOGGLE_TIPS,
                show_all_tips ? "Level Hints" : "All Tips", TERM_L_WHITE);
        if (!show_all_tips && n > 0)
        {
            hint_message_meta meta;

            hint_messages_message_meta(sel, &meta);
            if (hint_message_has_source(&meta))
            {
                sdl_hint_quest_menu_add_button(HINT_MESSAGE_CLICK_LOOK,
                    "Look", TERM_L_BLUE);
                sdl_hint_quest_menu_add_button(HINT_MESSAGE_CLICK_MAP,
                    "Map", TERM_L_BLUE);
            }
        }
        sdl_hint_quest_menu_add_button(HINT_MESSAGE_CLICK_BACK, "Back",
            TERM_L_WHITE);
        sdl_hint_quest_menu_finish();

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
                        tabs_focus = false;
                        continue;
                    }
                    next_page = HINT_QUEST_PAGE_QUESTS;
                    break;
                }
                else if (clicked_choice == HINT_QUEST_CLICK_THRALLS_TAB)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                    {
                        tabs_focus = false;
                        continue;
                    }
                    next_page = HINT_QUEST_PAGE_THRALLS;
                    break;
                }
                else if (clicked_choice == HINT_QUEST_CLICK_HINTS_TAB)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                    {
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

        if (next_page != HINT_QUEST_PAGE_EXIT)
            break;

        if (hint_quest_tab_key(ch))
        {
            next_page = HINT_QUEST_PAGE_QUESTS;
            break;
        }

        if (hint_quest_handle_tab_navigation(ch, &tabs_focus,
                (n <= 0) || (sel == 0), HINT_QUEST_PAGE_HINTS,
                &next_page))
        {
            if (next_page != HINT_QUEST_PAGE_EXIT)
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
            hint_quest_page selected_page;

            sdl_hint_quest_menu_prepare_leaf_turn(
                HINT_QUEST_PAGE_HINTS, 1);

            if (show_all_tips)
            {
                action = skeleton_tip_show(sel);
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

            selected_page = hint_message_action_page(action);
            if (selected_page != HINT_QUEST_PAGE_EXIT
                && selected_page != HINT_QUEST_PAGE_HINTS)
            {
                next_page = selected_page;
                break;
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
    if (next_page != HINT_QUEST_PAGE_EXIT && !pending_look && !pending_map)
        sdl_hint_quest_menu_prepare_page_turn(next_page);
    else
        sdl_hint_quest_menu_hide();

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

    return next_page;
}

static int thrall_quest_collect(s16b entries[], int limit)
{
    int count = 0;

    if (!entries || limit <= 0)
        return 0;

    for (int m_idx = 1; m_idx < mon_max && count < limit; ++m_idx)
    {
        monster_type* m_ptr = &mon_list[m_idx];

        if (!m_ptr->r_idx || !is_alert_thrall(m_ptr))
            continue;
        if (!m_ptr->thrall_quest_requested
            || m_ptr->thrall_quest_item == THRALL_QUEST_NONE)
        {
            continue;
        }
        if (m_ptr->thrall_quest_completed == THRALL_QUEST_STATE_REWARDED)
            continue;

        entries[count++] = (s16b)m_idx;
    }

    return count;
}

static cptr thrall_quest_giver_name(const monster_type* m_ptr)
{
    return (m_ptr && m_ptr->r_idx == R_IDX_ALERT_ELF_THRALL)
        ? "elven thrall" : "human thrall";
}

static void thrall_quest_format_goal(const monster_type* m_ptr, char* buf,
    size_t buflen, bool include_inventory_status)
{
    cptr giver;
    cptr item;

    if (!buf || !buflen)
        return;
    if (!m_ptr)
    {
        SDL_strlcpy(buf, "The thrall quest is no longer available.", buflen);
        return;
    }

    giver = thrall_quest_giver_name(m_ptr);
    item = get_thrall_quest_item_name(m_ptr->thrall_quest_item);

    if (m_ptr->thrall_quest_completed == THRALL_QUEST_STATE_REWARD_PENDING)
    {
        strnfmt(buf, buflen,
            "Return to the %s and choose the aid he will grant you.", giver);
    }
    else
    {
        strnfmt(buf, buflen, "Bring %s to the %s.", item, giver);
        if (include_inventory_status
            && player_has_thrall_quest_item(m_ptr->thrall_quest_item) >= 0)
        {
            SDL_strlcat(buf, " You carry the requested item.", buflen);
        }
    }
}

static hint_message_action thrall_quest_show_internal(int m_idx,
    int* source_y, int* source_x)
{
    char ch;
    char title[96];
    char goal[320];
    hint_message_action action = HINT_MESSAGE_ACTION_NONE;
    bool steamdeck = steamdeck_controls_active();
    monster_type* m_ptr;

    if (m_idx <= 0 || m_idx >= mon_max)
        return HINT_MESSAGE_ACTION_NONE;
    m_ptr = &mon_list[m_idx];
    if (!m_ptr->r_idx || !is_alert_thrall(m_ptr))
        return HINT_MESSAGE_ACTION_NONE;

    strnfmt(title, sizeof(title), "Thrall Quest: %s",
        m_ptr->r_idx == R_IDX_ALERT_ELF_THRALL
            ? "Elven Thrall" : "Human Thrall");
    thrall_quest_format_goal(m_ptr, goal, sizeof(goal), true);

    while (true)
    {
        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);
        sdl_hint_quest_menu_begin(HINT_QUEST_PAGE_THRALLS, "Hints & Quests",
            title, true, true, 0);
        sdl_hint_quest_menu_add_block(goal, TERM_WHITE, 0, 0);
        sdl_hint_quest_menu_add_block(
            "Location: on this dungeon level. Use Look or Map to find the thrall.",
            TERM_SLATE, 0, 0);
        sdl_hint_quest_menu_add_button(HINT_MESSAGE_CLICK_CONTINUE,
            "Continue", TERM_L_WHITE);
        sdl_hint_quest_menu_add_button(HINT_MESSAGE_CLICK_LOOK, "Look",
            TERM_L_BLUE);
        sdl_hint_quest_menu_add_button(HINT_MESSAGE_CLICK_MAP, "Map",
            TERM_L_BLUE);
        sdl_hint_quest_menu_add_button(HINT_MESSAGE_CLICK_BACK, "Back",
            TERM_L_WHITE);
        sdl_hint_quest_menu_finish();
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

                action = hint_message_tab_action(clicked_choice);
                if (action != HINT_MESSAGE_ACTION_NONE)
                    break;

                if (clicked_choice == HINT_MESSAGE_CLICK_LOOK)
                    action = HINT_MESSAGE_ACTION_LOOK;
                else if (clicked_choice == HINT_MESSAGE_CLICK_MAP)
                    action = HINT_MESSAGE_ACTION_MAP;
                break;
            }
            if (ch == UI_MENU_CLICK_WAKE_KEY)
                continue;
        }

        ch = (char)steamdeck_menu_key(ch, 0, 0);
        if (hint_quest_tab_key(ch))
        {
            action = HINT_MESSAGE_ACTION_OPEN_HINTS;
            break;
        }
        if (steamdeck && ch == steamdeck_back_key())
            break;
        if (ch == 'l' || ch == 'L'
            || (steamdeck && ch == steamdeck_alt_action_key()))
        {
            action = HINT_MESSAGE_ACTION_LOOK;
            break;
        }
        if (ch == 'm' || ch == 'M')
        {
            action = HINT_MESSAGE_ACTION_MAP;
            break;
        }
        break;
    }

    if (action == HINT_MESSAGE_ACTION_LOOK
        || action == HINT_MESSAGE_ACTION_MAP)
    {
        if (source_y)
            *source_y = m_ptr->fy;
        if (source_x)
            *source_x = m_ptr->fx;
    }

    ui_menu_click_clear();
    if (action == HINT_MESSAGE_ACTION_LOOK
        || action == HINT_MESSAGE_ACTION_MAP)
    {
        sdl_hint_quest_menu_hide();
    }
    else if (action == HINT_MESSAGE_ACTION_NONE
        || action == HINT_MESSAGE_ACTION_OPEN_THRALLS)
    {
        sdl_hint_quest_menu_prepare_leaf_turn(HINT_QUEST_PAGE_THRALLS, -1);
    }
    return action;
}

static hint_quest_page do_cmd_thrall_quests(bool* out_pending_look,
    int* out_look_y, int* out_look_x, bool* out_pending_map,
    int* out_map_y, int* out_map_x)
{
    s16b entries[MAX_MONSTERS];
    int count = thrall_quest_collect(entries, N_ELEMENTS(entries));
    int sel = 0;
    bool tabs_focus = false;
    bool pending_look = false;
    bool pending_map = false;
    int look_y = -1;
    int look_x = -1;
    int map_y = -1;
    int map_x = -1;
    hint_quest_page next_page = HINT_QUEST_PAGE_EXIT;
    bool steamdeck = steamdeck_controls_active();

    while (true)
    {
        char section[64];
        char ch;

        ui_menu_click_begin();
        ui_menu_click_set_hover_enabled(true);
        ui_menu_click_set_touch_category(SDL_TOUCH_MENU_CATEGORY_OTHER);

        if (count > 0)
        {
            sel = MAX(0, MIN(sel, count - 1));
        }
        else
        {
            sel = 0;
        }

        strnfmt(section, sizeof(section), "Thrall Quests (%d)", count);
        sdl_hint_quest_menu_begin(HINT_QUEST_PAGE_THRALLS,
            "Hints & Quests", section, true, false,
            count > 0 ? HINT_MESSAGE_CLICK_ENTRY_BASE + sel : 0);

        if (count <= 0)
        {
            sdl_hint_quest_menu_add_block(
                "No active thrall requests on this level.", TERM_SLATE,
                0, 0);
        }
        for (int idx = 0; idx < count; ++idx)
        {
            char goal[256];
            char entry[320];

            thrall_quest_format_goal(&mon_list[entries[idx]], goal,
                sizeof(goal), false);
            strnfmt(entry, sizeof(entry), "%d. %s", idx + 1, goal);
            sdl_hint_quest_menu_add_block(entry, TERM_WHITE, 0,
                HINT_MESSAGE_CLICK_ENTRY_BASE + idx);
        }
        if (count > 0)
        {
            sdl_hint_quest_menu_add_button(HINT_MESSAGE_CLICK_LOOK, "Look",
                TERM_L_BLUE);
            sdl_hint_quest_menu_add_button(HINT_MESSAGE_CLICK_MAP, "Map",
                TERM_L_BLUE);
        }
        sdl_hint_quest_menu_add_button(HINT_MESSAGE_CLICK_BACK, "Back",
            TERM_L_WHITE);
        sdl_hint_quest_menu_finish();

        Term_fresh();
        hide_cursor = true;
        ch = inkey();
        hide_cursor = false;

        {
            int clicked_choice = 0;
            int click_action = UI_MENU_CLICK_PRIMARY;
            bool generated_command = false;

            if (ui_menu_click_take_action(&clicked_choice, &click_action))
            {
                if (clicked_choice == HINT_QUEST_CLICK_HINTS_TAB
                    || clicked_choice == HINT_QUEST_CLICK_QUESTS_TAB)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                    {
                        tabs_focus = false;
                        continue;
                    }
                    next_page = (clicked_choice == HINT_QUEST_CLICK_HINTS_TAB)
                        ? HINT_QUEST_PAGE_HINTS : HINT_QUEST_PAGE_QUESTS;
                    break;
                }
                if (clicked_choice == HINT_QUEST_CLICK_THRALLS_TAB)
                {
                    if (click_action == UI_MENU_CLICK_HOVER)
                    {
                        tabs_focus = false;
                    }
                    continue;
                }
                if (clicked_choice >= HINT_MESSAGE_CLICK_ENTRY_BASE)
                {
                    int idx = clicked_choice - HINT_MESSAGE_CLICK_ENTRY_BASE;

                    if (idx >= 0 && idx < count)
                    {
                        sel = idx;
                        if (click_action == UI_MENU_CLICK_HOVER)
                            continue;
                        ch = '\r';
                        generated_command = true;
                    }
                }
                else if (click_action == UI_MENU_CLICK_HOVER)
                {
                    tabs_focus = false;
                    continue;
                }
                else if (clicked_choice == HINT_MESSAGE_CLICK_LOOK)
                {
                    ch = 'l';
                    generated_command = true;
                }
                else if (clicked_choice == HINT_MESSAGE_CLICK_MAP)
                {
                    ch = 'm';
                    generated_command = true;
                }
                else if (clicked_choice == HINT_MESSAGE_CLICK_BACK)
                {
                    ch = ESCAPE;
                    generated_command = true;
                }
            }
            else if (ch == UI_MENU_CLICK_WAKE_KEY)
            {
                continue;
            }

            if (!generated_command)
                ch = (char)steamdeck_menu_key(ch, '\t', '\t');
        }

        if (hint_quest_tab_key(ch))
        {
            next_page = HINT_QUEST_PAGE_HINTS;
            break;
        }
        if (hint_quest_handle_tab_navigation(ch, &tabs_focus,
                count <= 0 || sel == 0, HINT_QUEST_PAGE_THRALLS,
                &next_page))
        {
            if (next_page != HINT_QUEST_PAGE_EXIT)
                break;
            continue;
        }
        if (ch == ESCAPE || (steamdeck && ch == steamdeck_back_key()))
            break;
        if (count <= 0)
        {
            bell(NULL);
            continue;
        }
        if (ch == '8')
        {
            sel = (sel > 0) ? sel - 1 : count - 1;
            continue;
        }
        if (ch == '2')
        {
            sel = (sel + 1 < count) ? sel + 1 : 0;
            continue;
        }
        if (ch == '\r' || ch == '\n' || ch == ' ' || ch == '6'
            || (steamdeck && ch == steamdeck_confirm_key()))
        {
            int y = -1;
            int x = -1;
            hint_message_action action;
            hint_quest_page selected_page;

            sdl_hint_quest_menu_prepare_leaf_turn(
                HINT_QUEST_PAGE_THRALLS, 1);
            action = thrall_quest_show_internal(entries[sel], &y, &x);

            if (action == HINT_MESSAGE_ACTION_LOOK)
            {
                pending_look = true;
                look_y = y;
                look_x = x;
                break;
            }
            if (action == HINT_MESSAGE_ACTION_MAP)
            {
                pending_map = true;
                map_y = y;
                map_x = x;
                break;
            }

            selected_page = hint_message_action_page(action);
            if (selected_page != HINT_QUEST_PAGE_EXIT
                && selected_page != HINT_QUEST_PAGE_THRALLS)
            {
                next_page = selected_page;
                break;
            }
            continue;
        }
        if (ch == 'l' || ch == 'L'
            || (steamdeck && ch == steamdeck_alt_action_key()))
        {
            monster_type* m_ptr = &mon_list[entries[sel]];

            pending_look = true;
            look_y = m_ptr->fy;
            look_x = m_ptr->fx;
            break;
        }
        if (ch == 'm' || ch == 'M')
        {
            monster_type* m_ptr = &mon_list[entries[sel]];

            pending_map = true;
            map_y = m_ptr->fy;
            map_x = m_ptr->fx;
            break;
        }
        bell(NULL);
    }

    ui_menu_click_clear();
    if (next_page != HINT_QUEST_PAGE_EXIT && !pending_look && !pending_map)
        sdl_hint_quest_menu_prepare_page_turn(next_page);
    else
        sdl_hint_quest_menu_hide();
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
    return next_page;
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
 * Long messages wrap to the available width and pagination accounts for the
 * resulting variable-height entries.
 *
 * Attempt to only hilite the matching portions of the string.
 */
void do_cmd_messages_with_filter(int initial_filter)
{
    char ch;

    int i, j, n;
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
        int used_rows = 0;

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

        max_i = log_history_wrapped_last_page_start(filter, n, wid,
            visible_rows);
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
        for (j = 0; (i + j < n); j++)
        {
            char wrapped_text[256];
            byte wrapped_attr;
            int wrapped_rows = log_history_wrapped_entry_rows(filter,
                i + j, wid);
            int line_y;

            if (used_rows + wrapped_rows > visible_rows)
                break;

            line_y = notes_mode ? (body_top + used_rows)
                                : (body_bottom - used_rows - wrapped_rows + 1);
            log_history_wrapped_entry_text(filter, i + j, wrapped_text,
                sizeof(wrapped_text), &wrapped_attr);
            if (!notes_mode
                && log_history_entries[i + j].kind == LOG_HISTORY_ENTRY_COMBAT
                && wrapped_rows == 1)
            {
                log_history_draw_combat_entry(&log_history_entries[i + j],
                    line_y, 0);
            }
            else
            {
                log_history_draw_wrapped_text(line_y, wid, wrapped_attr,
                    wrapped_text, shower);
            }
            used_rows += wrapped_rows;
        }

        range_first = (n > 0) ? (i + 1) : 0;
        range_last = (n > 0) ? (i + j) : 0;
        page_rows = (j > 1) ? (j - 1) : 1;

        log_history_draw_filters(filter, hover_filter, wid);

        /* Display header XXX XXX XXX */
        prt(format("Log (%s, %d-%d of %d)",
                log_history_filter_label(filter), range_first, range_last, n),
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
                    "%s/%s page  Up/Down line  / find  = highlight  %s back",
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
                    "e/i filter  Up/Down line  PgUp/PgDn page  / find  = highlight  Esc",
                    "e/i filter  Up/Down line  Pg page  / find  Esc",
                    "e/i filter  / find  Esc",
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
            continue;
        }

        /* Exit on Escape */
        if (ch == ESCAPE)
            break;

        /* Hack -- Save the old index */
        old_i = i;

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
        if (i == old_i)
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
