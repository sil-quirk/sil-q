#include "angband.h"
#include "externs.h"
#include "fs/io_sdl.h"
#include "fs/path.h"
#include "h-define.h"
#include "init.h"
#include "log/log.h"
#include "metarun.h"
#include "score/score_guid.h"
#include "init-parse-internal.h"
#include "init-object-bonuses.h"
#include <ctype.h>

#ifdef ALLOW_TEMPLATES
static byte skeleton_note_parse_sval_token(const char* tok, bool* ok)
{
    if (ok)
        *ok = false;
    if (!tok || !*tok)
        return 0;
    if (streq(tok, "ELF"))
    {
        if (ok) *ok = true;
        return SV_SKELETON_ELF;
    }
    if (streq(tok, "HUMAN"))
    {
        if (ok) *ok = true;
        return SV_SKELETON_HUMAN;
    }
    if (streq(tok, "ORC"))
    {
        if (ok) *ok = true;
        return SV_SKELETON_ORC;
    }
    if (streq(tok, "ANY"))
    {
        if (ok) *ok = true;
        return SV_SKELETON_NOTE_ANY;
    }
    return 0;
}

static byte skeleton_note_parse_hint_token(const char* tok)
{
    if (!tok)
        return SKEL_HINT_NONE;
    if (streq(tok, "GREAT_VAULT"))
        return SKEL_HINT_GREAT_VAULT;
    if (streq(tok, "VAULT_ARTIFACT"))
        return SKEL_HINT_VAULT_ARTIFACT;
    if (streq(tok, "STAIRS"))
        return SKEL_HINT_STAIRS;
    if (streq(tok, "PARTITION"))
        return SKEL_HINT_PARTITION_PRESENCE;
    if (streq(tok, "FORGE"))
        return SKEL_HINT_FORGE;
    if (streq(tok, "UNIQUE"))
        return SKEL_HINT_UNIQUE_MONSTER;
    if (streq(tok, "TIP"))
        return SKEL_HINT_TIP;
    if (streq(tok, "SIZE"))
        return SKEL_HINT_LEVEL_SIZE;
    if (streq(tok, "QUEST"))
        return SKEL_HINT_QUEST;
    if (streq(tok, "LABYRINTH"))
        return SKEL_HINT_PART_LABYRINTH;
    if (streq(tok, "CHASM"))
        return SKEL_HINT_PART_CHASM;
    if (streq(tok, "CAVE"))
        return SKEL_HINT_PART_CAVE;
    if (streq(tok, "CAVE_ICE"))
        return SKEL_HINT_PART_CAVE_ICE;
    if (streq(tok, "CAVE_FIRE"))
        return SKEL_HINT_PART_CAVE_FIRE;
    if (streq(tok, "CAVE_POIS"))
        return SKEL_HINT_PART_CAVE_POIS;
    if (streq(tok, "ROOMY"))
        return SKEL_HINT_PART_ROOMY;
    if (streq(tok, "RUINED"))
        return SKEL_HINT_PART_RUINED;
    if (streq(tok, "CAVEY"))
        return SKEL_HINT_PART_CAVEY;
    return SKEL_HINT_NONE;
}

/*
 * Parse skeleton_note.txt
 *
 * Formats:
 *   O:<SVAL>:<weight>:<text>
 *   C:<SVAL>:<weight>:<text>
 *   M:<SVAL>:<HINT>:<weight>:<text>[||<extra text>]
 *
 * SVAL may be ELF/HUMAN/ORC/ANY
 * HINT may be GREAT_VAULT/VAULT_ARTIFACT/STAIRS/PARTITION/FORGE/UNIQUE/TIP/SIZE/QUEST/LABYRINTH/CHASM/CAVE/CAVE_ICE/CAVE_FIRE/CAVE_POIS/ROOMY/RUINED/CAVEY
 * Weight is optional (defaults to 100) and clamped to a byte.
 * Extra text, when present, is stored as an optional companion line.
 */
errr parse_skeleton_note_info(char* buf, header* head)
{
    static int next_idx = 0;
    skeleton_note_role role = SKELETON_NOTE_ROLE_NONE;
    char buf_copy[1024];

    strnfmt(buf_copy, sizeof(buf_copy), "%s", buf);

    /* Reset per-file */
    if (error_idx < 0)
        next_idx = 0;

    if (!buf[0] || buf[0] == '#')
        return 0;

    if (buf[0] == 'O')
        role = SKELETON_NOTE_ROLE_OPENING;
    else if (buf[0] == 'C')
        role = SKELETON_NOTE_ROLE_SIGNOFF;
    else if (buf[0] == 'M')
        role = SKELETON_NOTE_ROLE_HINT;
    else
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;

    if (next_idx >= head->info_num)
        return PARSE_ERROR_TOO_MANY_ENTRIES;

    skeleton_note_template* note = (skeleton_note_template*)head->info_ptr + next_idx;

    char* cursor = buf + 2;
    char* sval_tok = cursor;
    char* sep = strchr(cursor, ':');
    if (!sep)
    {
        log_error("skeleton_note.txt: missing sval separator on line %d (buf='%s')",
            error_line, buf_copy);
        return PARSE_ERROR_GENERIC;
    }
    *sep = '\0';
    cursor = sep + 1;

    bool valid_sval = false;
    byte sval = skeleton_note_parse_sval_token(sval_tok, &valid_sval);
    if (!valid_sval)
    {
        log_error("skeleton_note.txt: invalid sval '%s' on line %d (buf='%s')",
            sval_tok, error_line, buf_copy);
        return PARSE_ERROR_GENERIC;
    }

    byte hint = SKEL_HINT_NONE;
    if (role == SKELETON_NOTE_ROLE_HINT)
    {
        char* hint_tok = cursor;
        sep = strchr(cursor, ':');
        if (!sep)
        {
            log_error("skeleton_note.txt: missing hint separator on line %d (buf='%s')",
                error_line, buf_copy);
            return PARSE_ERROR_GENERIC;
        }
        *sep = '\0';
        cursor = sep + 1;
        hint = skeleton_note_parse_hint_token(hint_tok);
        if (hint == SKEL_HINT_NONE)
        {
            log_error("skeleton_note.txt: invalid hint '%s' on line %d (buf='%s')",
                hint_tok, error_line, buf_copy);
            return PARSE_ERROR_INVALID_FLAG;
        }
    }

    long weight = 100;
    sep = strchr(cursor, ':');
    if (sep)
    {
        *sep = '\0';
        weight = atol(cursor);
        cursor = sep + 1;
    }
    else
    {
        /* no separator on this line: cursor already points at the value */
    }

    if (weight < 0 || weight > 255)
    {
        log_error("skeleton_note.txt: weight out of bounds (%ld) on line %d (buf='%s')",
            weight, error_line, buf_copy);
        return PARSE_ERROR_OUT_OF_BOUNDS;
    }

    if (!cursor || cursor[0] == '\0')
    {
        log_error("skeleton_note.txt: missing text payload on line %d (buf='%s')",
            error_line, buf_copy);
        return PARSE_ERROR_GENERIC;
    }

    char* extra_cursor = strstr(cursor, "||");
    if (extra_cursor)
    {
        *extra_cursor = '\0';
        extra_cursor += 2;
        if (extra_cursor[0] == '\0')
        {
            log_error("skeleton_note.txt: missing extra text payload on line %d (buf='%s')",
                error_line, buf_copy);
            return PARSE_ERROR_GENERIC;
        }
    }

    if (cursor[0] == '\0')
    {
        log_error("skeleton_note.txt: missing text payload on line %d (buf='%s')",
            error_line, buf_copy);
        return PARSE_ERROR_GENERIC;
    }

    note->sval = sval;
    note->hint = hint;
    note->role = role;
    note->weight = (byte)weight;
    note->extra_text = 0;

    if (!add_text(&note->text, head, cursor))
        return PARSE_ERROR_OUT_OF_MEMORY;

    if (extra_cursor && !add_text(&note->extra_text, head, extra_cursor))
        return PARSE_ERROR_OUT_OF_MEMORY;

    next_idx++;
    error_idx = next_idx;
    return 0;
}

/*
 * Grab one flag in a special item_type from a textual string
 */

#endif /* ALLOW_TEMPLATES */