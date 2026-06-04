/* File: level-generation-screen.c */

#include "angband.h"
#include "level-generation/level-generation-internal.h"


#define LEVEL_GEN_STAGE_DONE LEVEL_GEN_STAGE_COUNT
#define LEVEL_GEN_SCREEN_DEBUG_LINES 32
#define LEVEL_GEN_SCREEN_ISSUES 12


level_gen_screen_state level_gen_screen = {0};
char level_gen_debug_last_greater_vault_name[80] = "";
char level_gen_debug_active_quest_vault_name[80] = "";
char level_gen_debug_last_quest_vault_name[80] = "";
char level_gen_debug_questgiver_name[80] = "";
char level_gen_debug_last_room_name[80] = "";

const char* level_gen_stage_user_labels[LEVEL_GEN_STAGE_COUNT] = {
    "Planning the level",
    "Laying the bedrock",
    "Shaping the halls",
    "Linking passages",
    "Setting doors and stairs",
    "Stocking the treasure",
    "Mustering the foes",
    "Final checks"
};

const char* level_gen_stage_debug_labels[LEVEL_GEN_STAGE_COUNT] = {
    "Planning",
    "Foundations",
    "Rooms and partitions",
    "Connections",
    "Entry placement",
    "Objects and traps",
    "Monster pass",
    "Final validation"
};

const char* level_gen_stage_status[LEVEL_GEN_STAGE_COUNT] = {
    "Surveying the next depth.",
    "Setting the foundation.",
    "The halls are taking shape.",
    "Passages are being linked.",
    "Doors and stairs are being set.",
    "Treasure is being placed.",
    "Creatures are moving in.",
    "Checking the final details."
};

void level_gen_debug_reset_context(void)
{
    g_vault_name[0] = '\0';
    level_gen_debug_last_greater_vault_name[0] = '\0';
    level_gen_debug_active_quest_vault_name[0] = '\0';
    level_gen_debug_last_quest_vault_name[0] = '\0';
    level_gen_debug_questgiver_name[0] = '\0';
    level_gen_debug_last_room_name[0] = '\0';
}

void level_gen_screen_build_partition_summary(char* total_buf,
    size_t total_buflen, char* types_buf, size_t types_buflen);

void level_gen_screen_build_generated_summary(char* quest_buf,
    size_t quest_buflen, char* roulette_buf, size_t roulette_buflen,
    char* giver_buf, size_t giver_buflen, char* gv_buf, size_t gv_buflen);

int level_gen_screen_utf8_prefix_len(cptr text, int max_cols)
{
    int bytes = 0;
    int cols = 0;

    if (!text || max_cols <= 0)
        return 0;

    while (text[bytes])
    {
        int char_len = utf8_sequence_len(text + bytes);
        int char_width;

        if (char_len <= 0)
            break;

        char_width = utf8_display_width_n(text + bytes, char_len);
        if (char_width > 0 && cols + char_width > max_cols)
            break;

        cols += char_width;
        bytes += char_len;
    }

    return bytes;
}

void level_gen_screen_fit_text(char* buf, size_t buflen, cptr text,
    int max_chars)
{
    if (!buflen)
        return;

    if (!text)
        text = "";

    if (max_chars <= 0)
    {
        buf[0] = '\0';
    }
    else if (utf8_display_width_n(text, (int)strlen(text)) <= max_chars)
    {
        SDL_strlcpy(buf, text, buflen);
    }
    else if (max_chars <= 3)
    {
        int copy_len = level_gen_screen_utf8_prefix_len(text, max_chars);
        if (copy_len >= (int)buflen)
            copy_len = (int)buflen - 1;
        SDL_memcpy(buf, text, (size_t)copy_len);
        buf[copy_len] = '\0';
    }
    else
    {
        int copy_len = level_gen_screen_utf8_prefix_len(text, max_chars - 3);
        if (copy_len >= (int)buflen)
            copy_len = (int)buflen - 1;
        SDL_memcpy(buf, text, (size_t)copy_len);
        buf[copy_len] = '\0';
        SDL_strlcat(buf, "...", buflen);
    }
}

void level_gen_screen_put_centered(int row, byte attr, cptr text)
{
    int wid = 80;
    int hgt = 24;
    char buf[256];
    int col;

    Term_get_size(&wid, &hgt);
    if (row < 0 || row >= hgt)
        return;

    level_gen_screen_fit_text(buf, sizeof(buf), text, MAX(1, wid - 2));
    col = (wid - utf8_display_width_n(buf, (int)strlen(buf))) / 2;
    if (col < 0)
        col = 0;

    Term_putstr(col, row, -1, attr, buf);
}

void level_gen_screen_put_fitted(int col, int row, int width, byte attr,
    cptr text)
{
    char buf[256];

    if (width <= 0)
        return;

    level_gen_screen_fit_text(buf, sizeof(buf), text, width);
    Term_putstr(col, row, -1, attr, buf);
}

int level_gen_screen_print_wrapped(int row, int col, int width,
    int max_lines, byte attr, cptr text)
{
    const char* p = text ? text : "";
    int lines = 0;

    if (width <= 0 || max_lines <= 0)
        return 0;

    while (*p && lines < max_lines)
    {
        char buf[256];
        const char* start;
        const char* line_end;
        const char* next_p;
        const char* last_space = NULL;
        int copy_len;
        int cols = 0;

        while (*p == ' ')
            p++;

        if (!*p)
            break;

        start = p;
        line_end = p;

        while (*line_end && *line_end != '\n')
        {
            int char_len = utf8_sequence_len(line_end);
            int char_width;

            if (char_len <= 0)
                break;

            char_width = utf8_display_width_n(line_end, char_len);
            if (char_width > 0 && cols + char_width > width)
            {
                break;
            }

            if (*line_end == ' ')
                last_space = line_end;

            cols += char_width;
            line_end += char_len;
        }

        if (*line_end == '\n')
        {
            next_p = line_end + 1;
        }
        else if (*line_end && last_space && last_space > start)
        {
            next_p = last_space;
            line_end = last_space;
        }
        else
        {
            next_p = line_end;
        }

        while (line_end > start && line_end[-1] == ' ')
            line_end--;

        if ((lines == max_lines - 1) && *next_p)
        {
            if (width > 3)
            {
                copy_len = level_gen_screen_utf8_prefix_len(start, width - 3);
                if (copy_len < 0)
                    copy_len = 0;
                if (copy_len >= (int)sizeof(buf) - 3)
                    copy_len = (int)sizeof(buf) - 4;
                copy_len = utf8_safe_prefix_len(start, copy_len);
                while (copy_len > 0 && start[copy_len - 1] == ' ')
                    copy_len--;
                SDL_memcpy(buf, start, (size_t)copy_len);
                buf[copy_len] = '\0';
                SDL_strlcat(buf, "...", sizeof(buf));
            }
            else
            {
                copy_len = level_gen_screen_utf8_prefix_len(start, width);
                if (copy_len < 0)
                    copy_len = 0;
                if (copy_len >= (int)sizeof(buf))
                    copy_len = (int)sizeof(buf) - 1;
                copy_len = utf8_safe_prefix_len(start, copy_len);
                SDL_memcpy(buf, start, (size_t)copy_len);
                buf[copy_len] = '\0';
            }
        }
        else
        {
            copy_len = (int)(line_end - start);
            if (copy_len >= (int)sizeof(buf))
                copy_len = (int)sizeof(buf) - 1;
            copy_len = utf8_safe_prefix_len(start, copy_len);
            if (copy_len < 0)
                copy_len = 0;
            SDL_memcpy(buf, start, (size_t)copy_len);
            buf[copy_len] = '\0';
        }

        Term_putstr(col, row + lines, -1, attr, buf);
        lines++;

        p = next_p;
    }

    return lines;
}

int level_gen_screen_count_wrapped_lines(int width, int max_lines,
    cptr text)
{
    const char* p = text ? text : "";
    int lines = 0;

    if (width <= 0 || max_lines <= 0)
        return 0;

    while (*p && lines < max_lines)
    {
        const char* start;
        const char* line_end;
        const char* last_space = NULL;
        int cols = 0;

        while (*p == ' ')
            p++;

        if (!*p)
            break;

        start = p;
        line_end = p;

        while (*line_end && *line_end != '\n')
        {
            int char_len = utf8_sequence_len(line_end);
            int char_width;

            if (char_len <= 0)
                break;

            char_width = utf8_display_width_n(line_end, char_len);
            if (char_width > 0 && cols + char_width > width)
                break;

            if (*line_end == ' ')
                last_space = line_end;

            cols += char_width;
            line_end += char_len;
        }

        if (*line_end == '\n')
        {
            p = line_end + 1;
        }
        else if (*line_end && last_space && last_space > start)
        {
            p = last_space;
        }
        else
        {
            p = line_end;
        }

        lines++;
    }

    return lines;
}

void level_gen_screen_format_depth_label(char* buf, size_t buflen)
{
    if (!p_ptr || p_ptr->depth <= 0)
        SDL_strlcpy(buf, "The Gates of Angband", buflen);
    else
        strnfmt(buf, buflen, "%d ft.", p_ptr->depth * 50);
}

bool level_gen_screen_capture_category(cptr category)
{
    if (!category)
        return false;

    return !strcmp(category, "SUMMARY")
        || !strcmp(category, "FAIL")
        || !strcmp(category, "QUEST")
        || !strcmp(category, "PARTITION")
        || !strcmp(category, "CONNECT")
        || !strcmp(category, "STAIRS");
}

void level_gen_screen_append_debug_line(cptr text)
{
    if (!text || !text[0])
        return;

    if (level_gen_screen.debug_count >= LEVEL_GEN_SCREEN_DEBUG_LINES)
    {
        memmove(level_gen_screen.debug_lines, level_gen_screen.debug_lines + 1,
            (LEVEL_GEN_SCREEN_DEBUG_LINES - 1)
            * sizeof(level_gen_screen.debug_lines[0]));
        level_gen_screen.debug_count = LEVEL_GEN_SCREEN_DEBUG_LINES - 1;
    }

    SDL_strlcpy(level_gen_screen.debug_lines[level_gen_screen.debug_count], text,
        sizeof(level_gen_screen.debug_lines[0]));
    level_gen_screen.debug_count++;
}

bool level_gen_screen_extract_context_value(cptr reason, cptr key,
    char* buf, size_t buflen)
{
    char pattern[16];
    const char* start;
    const char* end;

    if (!reason || !key || !buf || buflen == 0)
        return false;

    strnfmt(pattern, sizeof(pattern), "%s=", key);
    start = strstr(reason, pattern);
    if (!start)
        return false;

    start += strlen(pattern);
    end = start;
    while (*end && *end != ',' && *end != ']')
        end++;

    while (end > start && isspace((unsigned char)end[-1]))
        end--;

    if (end <= start)
        return false;

    strnfmt(buf, buflen, "%.*s", (int)(end - start), start);
    return true;
}

void level_gen_screen_extract_issue_key(cptr reason, char* buf,
    size_t buflen)
{
    char named[80];
    const char* end;
    size_t len;

    if (!reason || !reason[0])
    {
        SDL_strlcpy(buf, "Generation failed", buflen);
        return;
    }

    if (strstr(reason, "QUEST VAULT FAILED"))
    {
        if (level_gen_screen_extract_context_value(reason, "qv", named, sizeof(named)))
        {
            strnfmt(buf, buflen, "QUEST VAULT FAILED (%s)", named);
            return;
        }
        if (level_gen_debug_last_quest_vault_name[0])
        {
            strnfmt(buf, buflen, "QUEST VAULT FAILED (%s)",
                level_gen_debug_last_quest_vault_name);
            return;
        }
    }

    if (level_gen_screen_extract_context_value(reason, "gv", named, sizeof(named)))
    {
        end = strchr(reason, ':');
        if (!end)
            end = reason + strlen(reason);
        while (end > reason && isspace((unsigned char)end[-1]))
            end--;
        strnfmt(buf, buflen, "%.*s (%s)", (int)(end - reason), reason, named);
        return;
    }

    if (level_gen_screen_extract_context_value(reason, "qv", named, sizeof(named)))
    {
        end = strchr(reason, ':');
        if (!end)
            end = reason + strlen(reason);
        while (end > reason && isspace((unsigned char)end[-1]))
            end--;
        strnfmt(buf, buflen, "%.*s (%s)", (int)(end - reason), reason, named);
        return;
    }

    end = strchr(reason, ':');
    if (!end)
        end = reason + strlen(reason);

    while (end > reason && isspace((unsigned char)end[-1]))
        end--;

    len = (size_t)(end - reason);
    if (len >= buflen)
        len = buflen - 1;

    memcpy(buf, reason, len);
    buf[len] = '\0';
}

void level_gen_screen_record_issue(cptr reason)
{
    char key[64];

    level_gen_screen_extract_issue_key(reason, key, sizeof(key));

    for (int i = 0; i < level_gen_screen.issue_count; i++)
    {
        if (!strcmp(level_gen_screen.issues[i].key, key))
        {
            level_gen_screen.issues[i].count++;
            return;
        }
    }

    if (level_gen_screen.issue_count >= LEVEL_GEN_SCREEN_ISSUES)
        return;

    SDL_strlcpy(level_gen_screen.issues[level_gen_screen.issue_count].key, key,
        sizeof(level_gen_screen.issues[level_gen_screen.issue_count].key));
    level_gen_screen.issues[level_gen_screen.issue_count].count = 1;
    level_gen_screen.issue_count++;
}

void level_gen_debug_note_room_name(cptr name)
{
    if (name && name[0])
        SDL_strlcpy(level_gen_debug_last_room_name, name,
            sizeof(level_gen_debug_last_room_name));
}

void level_gen_debug_note_greater_vault_name(cptr name)
{
    if (name && name[0])
        SDL_strlcpy(level_gen_debug_last_greater_vault_name, name,
            sizeof(level_gen_debug_last_greater_vault_name));
}

void level_gen_debug_note_quest_vault_name(cptr name)
{
    if (name && name[0])
        SDL_strlcpy(level_gen_debug_last_quest_vault_name, name,
            sizeof(level_gen_debug_last_quest_vault_name));
}

const char* level_gen_debug_quest_name(int quest_id)
{
    quest_type* q_ptr;

    if (quest_id > 0 && quest_id < z_info->quest_max)
    {
        q_ptr = &quest_info[quest_id];
        if (q_ptr->name && quest_name_text)
            return quest_name_text + q_ptr->name;
    }

    switch (quest_id)
    {
    case QUEST_ID_TULKAS:
        return "Tulkas the Strong";
    case QUEST_ID_AULE:
        return "Aulë the Smith";
    case QUEST_ID_MANDOS:
        return "Mandos the Doomsman";
    case QUEST_ID_NIENA:
        return "Nienna, Lady of Pity";
    case QUEST_ID_OROME:
        return "Oromë the Hunter";
    case QUEST_ID_VARDA:
        return "Varda, Lady of the Stars";
    default:
        return NULL;
    }
}

void level_gen_debug_note_questgiver(int quest_id)
{
    const char* name = level_gen_debug_quest_name(quest_id);

    if (name && name[0])
        SDL_strlcpy(level_gen_debug_questgiver_name, name,
            sizeof(level_gen_debug_questgiver_name));
}

void level_gen_debug_activate_quest_vault_name(cptr name)
{
    if (name && name[0])
        SDL_strlcpy(level_gen_debug_active_quest_vault_name, name,
            sizeof(level_gen_debug_active_quest_vault_name));
}

void level_gen_debug_append_context(char* buf, size_t buflen, cptr key,
    cptr value)
{
    char tmp[256];

    if (!value || !value[0])
        return;

    if (buf[0])
        strnfmt(tmp, sizeof(tmp), "%s, %s=%s", buf, key, value);
    else
        strnfmt(tmp, sizeof(tmp), "%s=%s", key, value);

    SDL_strlcpy(buf, tmp, buflen);
}

void level_gen_debug_build_failure_reason(char* buf, size_t buflen,
    cptr reason)
{
    char context[256] = "";

    level_gen_debug_append_context(
        context, sizeof(context), "gv",
        g_vault_name[0] ? g_vault_name : level_gen_debug_last_greater_vault_name);
    level_gen_debug_append_context(
        context, sizeof(context), "qv",
        level_gen_debug_active_quest_vault_name[0]
            ? level_gen_debug_active_quest_vault_name
            : level_gen_debug_last_quest_vault_name);
    level_gen_debug_append_context(
        context, sizeof(context), "room", level_gen_debug_last_room_name);

    if (context[0])
        strnfmt(buf, buflen, "%s [%s]", reason ? reason : "Generation failed.",
            context);
    else
        SDL_strlcpy(buf, reason ? reason : "Generation failed.", buflen);
}

void level_gen_screen_build_generated_summary(char* quest_buf,
    size_t quest_buflen, char* roulette_buf, size_t roulette_buflen,
    char* giver_buf, size_t giver_buflen, char* gv_buf, size_t gv_buflen)
{
    int roulette_winner = debug_get_quest_lottery_winner();
    const char* roulette_name = level_gen_debug_quest_name(roulette_winner);

    if (level_gen_debug_active_quest_vault_name[0])
        strnfmt(quest_buf, quest_buflen, "Quest vaults: 1 (%s)",
            level_gen_debug_active_quest_vault_name);
    else
        SDL_strlcpy(quest_buf, "Quest vaults: 0", quest_buflen);

    if (roulette_name && roulette_name[0])
        strnfmt(roulette_buf, roulette_buflen, "Roulette winner: %s",
            roulette_name);
    else
        SDL_strlcpy(roulette_buf, "Roulette winner: none", roulette_buflen);

    if (level_gen_debug_questgiver_name[0])
        strnfmt(giver_buf, giver_buflen, "Quest giver spawned: 1 (%s)",
            level_gen_debug_questgiver_name);
    else
        SDL_strlcpy(giver_buf, "Quest giver spawned: 0", giver_buflen);

    if (g_vault_name[0])
        strnfmt(gv_buf, gv_buflen, "Greater vaults: 1 (%s)", g_vault_name);
    else
        SDL_strlcpy(gv_buf, "Greater vaults: 0", gv_buflen);
}

void level_gen_screen_draw_recent_events(int row, int col, int width,
    int max_rows)
{
    int start = level_gen_screen.debug_count;
    int used_rows = 0;

    if (width <= 0 || max_rows <= 0)
        return;

    while (start > 0)
    {
        int remaining = max_rows - used_rows;
        int needed;

        if (remaining <= 0)
            break;

        needed = level_gen_screen_count_wrapped_lines(width, remaining,
            level_gen_screen.debug_lines[start - 1]);
        if (needed <= 0)
        {
            start--;
            continue;
        }

        used_rows += needed;
        start--;
    }

    for (int i = start; i < level_gen_screen.debug_count && max_rows > 0; ++i)
    {
        int drawn = level_gen_screen_print_wrapped(row, col, width, max_rows,
            TERM_SLATE, level_gen_screen.debug_lines[i]);

        row += drawn;
        max_rows -= drawn;
    }
}

void level_gen_screen_draw_user(int wid, int hgt)
{
    static const char spinner_frames[] = "|/-\\";
    int top;
    int row;
    int stage_row;
    int bar_width;
    int filled = 0;
    int progress_units = 0;
    char buf[256];
    char bar[80];

    level_gen_screen.spinner =
        (level_gen_screen.spinner + 1) % ((int)sizeof(spinner_frames) - 1);

    top = MAX(0, (hgt - (LEVEL_GEN_STAGE_COUNT + 8)) / 2);
    row = top;

    level_gen_screen_put_centered(row++, TERM_L_BLUE, "Preparing the Level");
    level_gen_screen_put_centered(row++, TERM_L_WHITE, level_gen_screen.depth_label);
    row++;

    if (level_gen_screen.stage == LEVEL_GEN_STAGE_DONE)
    {
        level_gen_screen_put_centered(
            row++, TERM_L_GREEN,
            level_gen_screen.final_text[0] ? level_gen_screen.final_text
                                           : "The level is ready.");
    }
    else
    {
        strnfmt(buf, sizeof(buf), "%c %s",
            spinner_frames[level_gen_screen.spinner],
            level_gen_screen.status_text[0] ? level_gen_screen.status_text
                                            : "Preparing the level.");
        level_gen_screen_put_centered(row++, TERM_YELLOW, buf);
    }

    bar_width = MIN(34, MAX(18, wid - 10));
    if (level_gen_screen.stage == LEVEL_GEN_STAGE_DONE)
    {
        filled = bar_width;
    }
    else
    {
        progress_units = (2 * MAX(level_gen_screen.stage, 0)) + 1;
        filled = (progress_units * bar_width) / (2 * LEVEL_GEN_STAGE_COUNT);
        if (filled < 1)
            filled = 1;
        if (filled > bar_width)
            filled = bar_width;
    }

    if ((size_t)(bar_width + 3) > sizeof(bar))
        bar_width = (int)sizeof(bar) - 3;

    bar[0] = '[';
    for (int i = 0; i < bar_width; i++)
        bar[i + 1] = (i < filled) ? '#' : '.';
    bar[bar_width + 1] = ']';
    bar[bar_width + 2] = '\0';
    level_gen_screen_put_centered(row++, TERM_L_GREEN, bar);

    row++;
    stage_row = row;

    for (int i = 0; i < LEVEL_GEN_STAGE_COUNT && stage_row < hgt - 1; i++)
    {
        byte attr = TERM_SLATE;
        char marker = ' ';

        if (level_gen_screen.stage == LEVEL_GEN_STAGE_DONE
            || i < level_gen_screen.stage)
        {
            attr = TERM_L_GREEN;
            marker = 'x';
        }
        else if (i == level_gen_screen.stage)
        {
            attr = TERM_YELLOW;
            marker = '>';
        }

        strnfmt(buf, sizeof(buf), "[%c] %s", marker,
            level_gen_stage_user_labels[i]);
        level_gen_screen_put_centered(stage_row++, attr, buf);
    }

    level_gen_screen_put_centered(
        hgt - 1, TERM_SLATE, "Large levels can take a moment.");
}

void level_gen_screen_draw_debug(int wid, int hgt)
{
    char buf[256];
    char qv_status_buf[256];
    char partition_buf[256];
    char type_buf[256];
    char quest_buf[256];
    char roulette_buf[256];
    char giver_buf[256];
    char gv_buf[256];
    int width = MAX(1, wid - 2);
    int footer_row = MAX(0, hgt - 1);
    bool split = (wid >= 90);

    level_gen_screen_put_centered(0, TERM_L_BLUE, "Level Generation Debug");

    strnfmt(buf, sizeof(buf), "%s | attempts %d | retries %d | %s",
        level_gen_screen.depth_label,
        MAX(level_gen_screen.attempt, 1),
        level_gen_screen.total_failures,
        (level_gen_screen.stage == LEVEL_GEN_STAGE_DONE) ? "ready" : "running");
    level_gen_screen_put_fitted(1, 1, width, TERM_L_WHITE, buf);

    if (level_gen_debug_active_quest_vault_name[0])
    {
        strnfmt(qv_status_buf, sizeof(qv_status_buf), "Quest vault status: %s",
            level_gen_debug_active_quest_vault_name);
    }
    else if (level_gen_screen.stage != LEVEL_GEN_STAGE_DONE
        && level_gen_screen.last_quest_vault_failure[0])
    {
        strnfmt(qv_status_buf, sizeof(qv_status_buf), "Quest vault status: %s",
            level_gen_screen.last_quest_vault_failure);
    }
    else
    {
        qv_status_buf[0] = '\0';
    }

    level_gen_screen_build_partition_summary(partition_buf,
        sizeof(partition_buf), type_buf, sizeof(type_buf));
    level_gen_screen_build_generated_summary(quest_buf, sizeof(quest_buf),
        roulette_buf, sizeof(roulette_buf), giver_buf, sizeof(giver_buf),
        gv_buf, sizeof(gv_buf));

    if (split)
    {
        int left_col = 1;
        int gap = 2;
        int left_w = MAX(32, MIN(42, wid / 3));
        int right_col;
        int right_w;
        int left_row = 3;
        int right_row = 3;
        int issue_lines;
        int recent_lines;

        if (left_w > wid - 26)
            left_w = wid - 26;
        right_col = left_col + left_w + gap;
        right_w = MAX(10, wid - right_col - 1);

        level_gen_screen_put_fitted(left_col, 2, left_w, TERM_L_BLUE, "Summary:");
        level_gen_screen_put_fitted(right_col, 2, right_w, TERM_L_BLUE,
            "Recent generation events:");

        strnfmt(buf, sizeof(buf), "Stage: %s",
            (level_gen_screen.stage == LEVEL_GEN_STAGE_DONE)
                ? "Complete"
                : level_gen_stage_debug_labels[level_gen_screen.stage]);
        level_gen_screen_put_fitted(left_col, left_row++, left_w, TERM_YELLOW, buf);

        strnfmt(buf, sizeof(buf), "Current: %s",
            level_gen_screen.detail_text[0] ? level_gen_screen.detail_text
                                            : "(waiting)");
        left_row += level_gen_screen_print_wrapped(
            left_row, left_col, left_w, 2, TERM_SLATE, buf);

        strnfmt(buf, sizeof(buf), "Last retry: %s",
            level_gen_screen.last_failure[0] ? level_gen_screen.last_failure
                                             : "(none)");
        left_row += level_gen_screen_print_wrapped(
            left_row, left_col, left_w, 3, TERM_ORANGE, buf);

        if (left_row < footer_row)
        {
            left_row += level_gen_screen_print_wrapped(
                left_row, left_col, left_w, footer_row - left_row,
                TERM_SLATE, partition_buf);
        }
        if (type_buf[0] && left_row < footer_row)
        {
            left_row += level_gen_screen_print_wrapped(
                left_row, left_col, left_w, footer_row - left_row,
                TERM_SLATE, type_buf);
        }
        if (left_row < footer_row)
        {
            left_row += level_gen_screen_print_wrapped(
                left_row, left_col, left_w, footer_row - left_row,
                TERM_YELLOW, quest_buf);
        }
        if (left_row < footer_row)
        {
            left_row += level_gen_screen_print_wrapped(
                left_row, left_col, left_w, footer_row - left_row,
                TERM_YELLOW, roulette_buf);
        }
        if (left_row < footer_row)
        {
            left_row += level_gen_screen_print_wrapped(
                left_row, left_col, left_w, footer_row - left_row,
                TERM_YELLOW, giver_buf);
        }
        if (left_row < footer_row)
        {
            left_row += level_gen_screen_print_wrapped(
                left_row, left_col, left_w, footer_row - left_row,
                TERM_L_GREEN, gv_buf);
        }
        if (qv_status_buf[0] && left_row < footer_row)
        {
            left_row += level_gen_screen_print_wrapped(
                left_row, left_col, left_w, footer_row - left_row,
                TERM_ORANGE, qv_status_buf);
        }

        if (left_row < footer_row)
            level_gen_screen_put_fitted(left_col, left_row++, left_w, TERM_L_BLUE,
                "Main issues:");

        issue_lines = MAX(1, MIN(6,
            level_gen_screen.issue_count ? level_gen_screen.issue_count : 1));
        for (int shown = 0; shown < issue_lines && left_row < footer_row; shown++)
        {
            int best = -1;

            for (int i = 0; i < level_gen_screen.issue_count; i++)
            {
                if (level_gen_screen.issues[i].count <= 0)
                    continue;
                if (best < 0
                    || level_gen_screen.issues[i].count
                        > level_gen_screen.issues[best].count)
                {
                    best = i;
                }
            }

            if (best < 0)
            {
                level_gen_screen_put_fitted(left_col, left_row++, left_w, TERM_SLATE,
                    "(no retries yet)");
                break;
            }

            strnfmt(buf, sizeof(buf), "%dx %s",
                level_gen_screen.issues[best].count,
                level_gen_screen.issues[best].key);
            left_row += level_gen_screen_print_wrapped(
                left_row, left_col, left_w, 2, TERM_SLATE, buf);
            level_gen_screen.issues[best].count *= -1;
        }

        recent_lines = MAX(1, footer_row - right_row);
        level_gen_screen_draw_recent_events(right_row, right_col, right_w,
            recent_lines);
    }
    else
    {
        int row = 3;
        int issue_lines;
        int recent_lines;
        int remaining;

        strnfmt(buf, sizeof(buf), "Stage: %s",
            (level_gen_screen.stage == LEVEL_GEN_STAGE_DONE)
                ? "Complete"
                : level_gen_stage_debug_labels[level_gen_screen.stage]);
        level_gen_screen_put_fitted(1, row++, width, TERM_YELLOW, buf);

        strnfmt(buf, sizeof(buf), "Current: %s",
            level_gen_screen.detail_text[0] ? level_gen_screen.detail_text
                                            : "(waiting)");
        row += level_gen_screen_print_wrapped(row, 1, width, 2, TERM_SLATE, buf);

        strnfmt(buf, sizeof(buf), "Last retry: %s",
            level_gen_screen.last_failure[0] ? level_gen_screen.last_failure
                                             : "(none)");
        row += level_gen_screen_print_wrapped(row, 1, width, 2, TERM_ORANGE, buf);

        if (row < footer_row)
        {
            row += level_gen_screen_print_wrapped(
                row, 1, width, footer_row - row, TERM_SLATE, partition_buf);
        }
        if (type_buf[0] && row < footer_row)
        {
            row += level_gen_screen_print_wrapped(
                row, 1, width, footer_row - row, TERM_SLATE, type_buf);
        }
        if (row < footer_row)
        {
            row += level_gen_screen_print_wrapped(
                row, 1, width, footer_row - row, TERM_YELLOW, quest_buf);
        }
        if (row < footer_row)
        {
            row += level_gen_screen_print_wrapped(
                row, 1, width, footer_row - row, TERM_YELLOW, roulette_buf);
        }
        if (row < footer_row)
        {
            row += level_gen_screen_print_wrapped(
                row, 1, width, footer_row - row, TERM_YELLOW, giver_buf);
        }
        if (row < footer_row)
        {
            row += level_gen_screen_print_wrapped(
                row, 1, width, footer_row - row, TERM_L_GREEN, gv_buf);
        }
        if (qv_status_buf[0] && row < footer_row)
        {
            row += level_gen_screen_print_wrapped(
                row, 1, width, footer_row - row, TERM_ORANGE, qv_status_buf);
        }

        remaining = footer_row - row;
        issue_lines = MIN(4, MAX(2, remaining / 3));
        if (issue_lines > remaining - 2)
            issue_lines = MAX(1, remaining - 2);
        recent_lines = MAX(1, remaining - issue_lines - 1);

        if (row < footer_row)
            level_gen_screen_put_fitted(1, row++, width, TERM_L_BLUE, "Main issues:");

        for (int shown = 0; shown < issue_lines && row < footer_row; shown++)
        {
            int best = -1;

            for (int i = 0; i < level_gen_screen.issue_count; i++)
            {
                if (level_gen_screen.issues[i].count <= 0)
                    continue;
                if (best < 0
                    || level_gen_screen.issues[i].count
                        > level_gen_screen.issues[best].count)
                {
                    best = i;
                }
            }

            if (best < 0)
            {
                level_gen_screen_put_fitted(1, row++, width, TERM_SLATE,
                    "(no retries yet)");
                break;
            }

            strnfmt(buf, sizeof(buf), "%dx %s",
                level_gen_screen.issues[best].count,
                level_gen_screen.issues[best].key);
            level_gen_screen_put_fitted(1, row++, width, TERM_SLATE, buf);
            level_gen_screen.issues[best].count *= -1;
        }

        if (row < footer_row)
            level_gen_screen_put_fitted(1, row++, width, TERM_L_BLUE,
                "Recent generation events:");

        level_gen_screen_draw_recent_events(row, 1, width, recent_lines);
    }

    for (int i = 0; i < level_gen_screen.issue_count; i++)
        level_gen_screen.issues[i].count = ABS(level_gen_screen.issues[i].count);

    if (level_gen_screen.stage == LEVEL_GEN_STAGE_DONE)
    {
        level_gen_screen_put_centered(
            footer_row, TERM_L_WHITE, "Press any key to continue.");
    }
    else
    {
        level_gen_screen_put_fitted(1, footer_row, width, TERM_SLATE,
            "Watching generation live...");
    }
}

void level_gen_screen_draw_now(void)
{
    int wid = 80;
    int hgt = 24;

    if (!level_gen_screen.active || !Term)
        return;

    Term_get_size(&wid, &hgt);
    if (wid < 1)
        wid = 80;
    if (hgt < 1)
        hgt = 24;

    Term_clear();

    if (level_gen_screen.debug)
        level_gen_screen_draw_debug(wid, hgt);
    else
        level_gen_screen_draw_user(wid, hgt);

    Term_fresh();
    Term_xtra(TERM_XTRA_BORED, 0);
}

void level_gen_screen_maybe_draw(bool force)
{
    Uint64 now;
    Uint64 min_interval;

    if (!level_gen_screen.active || !Term)
        return;

    now = SDL_GetTicks();
    min_interval = level_gen_screen.debug ? 75 : 125;

    if (!force && (now - level_gen_screen.last_draw_ticks < min_interval))
        return;

    level_gen_screen.last_draw_ticks = now;
    level_gen_screen_draw_now();
}

void level_gen_screen_observer(cptr category, cptr message)
{
    char line[192];

    if (!level_gen_screen.active)
        return;

    if (message
        && (!strcmp(category ? category : "", "FAIL")
            || strstr(message, "FAILED")
            || strstr(message, "forcing regeneration")))
    {
        SDL_strlcpy(level_gen_screen.last_failure, message,
            sizeof(level_gen_screen.last_failure));
    }

    if (message && strstr(message, "QUEST VAULT FAILED"))
    {
        SDL_strlcpy(level_gen_screen.last_quest_vault_failure, message,
            sizeof(level_gen_screen.last_quest_vault_failure));
    }

    if (level_gen_screen.debug && level_gen_screen_capture_category(category))
    {
        strnfmt(line, sizeof(line), "%s: %s", category ? category : "GEN",
            message ? message : "");
        level_gen_screen_append_debug_line(line);
    }

    level_gen_screen_maybe_draw(false);
}

void level_gen_screen_begin(void)
{
    memset(&level_gen_screen, 0, sizeof(level_gen_screen));

    if (!Term)
        return;

    level_gen_screen.active = true;
    level_gen_screen.debug = (op_ptr && show_level_generation_debug);
    level_gen_screen.stage = LEVEL_GEN_STAGE_PLANNING;
    level_gen_screen_format_depth_label(level_gen_screen.depth_label,
        sizeof(level_gen_screen.depth_label));
    SDL_strlcpy(level_gen_screen.status_text, "Preparing the level.",
        sizeof(level_gen_screen.status_text));

    screen_save();
    level_gen_screen.screen_saved = true;
    gen_log_set_observer(level_gen_screen_observer);
    level_gen_screen_maybe_draw(true);
}

void level_gen_screen_start_attempt(void)
{
    if (!level_gen_screen.active)
        return;

    level_gen_screen.attempt++;
    level_gen_debug_reset_context();
    level_gen_screen.stage = LEVEL_GEN_STAGE_PLANNING;
    level_gen_screen.detail_text[0] = '\0';
    level_gen_screen.final_text[0] = '\0';
    SDL_strlcpy(level_gen_screen.status_text, level_gen_stage_status[LEVEL_GEN_STAGE_PLANNING],
        sizeof(level_gen_screen.status_text));
    level_gen_screen_maybe_draw(true);
}

void level_gen_screen_set_stage(level_gen_screen_stage_t stage,
    cptr detail)
{
    if (!level_gen_screen.active)
        return;

    level_gen_screen.stage = stage;
    SDL_strlcpy(level_gen_screen.status_text, level_gen_stage_status[stage],
        sizeof(level_gen_screen.status_text));
    if (detail)
        SDL_strlcpy(level_gen_screen.detail_text, detail,
            sizeof(level_gen_screen.detail_text));
    else
        level_gen_screen.detail_text[0] = '\0';

    level_gen_screen_maybe_draw(true);
}

void level_gen_screen_note_failure(cptr reason)
{
    cptr active_reason = reason;
    char decorated_reason[256];

    if (!level_gen_screen.active)
        return;

    if (!active_reason || !active_reason[0])
        active_reason = level_gen_screen.last_failure;
    if (!active_reason || !active_reason[0])
        active_reason = "Generation failed.";

    level_gen_debug_build_failure_reason(
        decorated_reason, sizeof(decorated_reason), active_reason);
    SDL_strlcpy(level_gen_screen.last_failure, decorated_reason,
        sizeof(level_gen_screen.last_failure));
    level_gen_screen.total_failures++;
    level_gen_screen_record_issue(decorated_reason);

    SDL_strlcpy(level_gen_screen.status_text, "Trying another arrangement.",
        sizeof(level_gen_screen.status_text));
    if (level_gen_screen.debug)
    {
        SDL_strlcpy(level_gen_screen.detail_text, decorated_reason,
            sizeof(level_gen_screen.detail_text));
    }

    level_gen_screen_maybe_draw(true);
}

void level_gen_screen_finish(bool success)
{
    if (!level_gen_screen.active)
        return;

    if (success)
    {
        level_gen_screen.stage = LEVEL_GEN_STAGE_DONE;
        SDL_strlcpy(level_gen_screen.final_text, "The level is ready.",
            sizeof(level_gen_screen.final_text));
        level_gen_screen_maybe_draw(true);
    }

    gen_log_set_observer(NULL);

    if (success && level_gen_screen.debug)
    {
        flush();
        hide_cursor = true;
        (void)inkey();
        hide_cursor = false;
    }

    if (level_gen_screen.screen_saved)
        screen_load();

    memset(&level_gen_screen, 0, sizeof(level_gen_screen));
}
