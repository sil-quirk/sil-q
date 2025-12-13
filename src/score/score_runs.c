#include "score/score_runs.h"

#include "angband.h"
#include "externs.h"
#include "fs/path.h"
#include "log/log.h"
#include "metarun.h"
#include "player/killer.h"
#include "score/score_io.h"
#include "score/score_guid.h"
#include "score/score_runs.h"
#include "score/score_logic.h"

#include <SDL3/SDL.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>

/* Helper to build score/meta file path correctly for both portable and normal builds */
static bool build_meta_path(char* buf, size_t len, const char* filename)
{
#ifdef SIL_USE_LOCAL_DATA
    /* Portable build: in apex directory */
    return path_build(buf, len, ANGBAND_DIR_APEX, filename);
#else
    /* Normal build: in meta directory (parent of metaruns) */
    if (ANGBAND_DIR_METARUN && *ANGBAND_DIR_METARUN) {
        char meta_dir[1024];
        SDL_strlcpy(meta_dir, ANGBAND_DIR_METARUN, sizeof(meta_dir));
        char* last_sep = strrchr(meta_dir, PATH_SEP[0]);
        if (last_sep) *last_sep = '\0';
        return path_build(buf, len, meta_dir, filename);
    } else {
        return path_build(buf, len, ANGBAND_DIR_APEX, filename);
    }
#endif
}

#define SCORE_RUNS_DB_VERSION 0x00020000u
#define SCORE_RUN_DETAIL_VERSION 2u
#define SCORE_RUN_ARTEFACT_CAP_MAX 512
#define SCORE_RUN_MONSTER_CAP_MAX 1024
#define SCORE_RUN_MILESTONE_CAP_MAX 512
#define SCORE_RUN_MILESTONE_TEXT_MAX 96

static void score_runs_fill_guid(score_guid64* guid, u64b value);
static score_killer_kind score_runs_killer_kind_from_string(const char* how);
static void score_runs_normalize_name(const char* name, char* out, size_t out_len);
static u32b score_runs_hash_persona(const char* normalized);
static SDL_IOStream* score_runs_open_db(const char* path, score_db_header* header,
                                        bool* created);
static bool score_runs_write_record(SDL_IOStream* file,
                                    const score_record_v1* record,
                                    const score_run_detail_block* details);
static bool score_runs_write_header(SDL_IOStream* file,
                                    const score_db_header* header);

static bool score_runs_legacy_checked = false;

static s32b score_runs_parse_int_field(const char* field, size_t len)
{
    char buf[32];
    parse_score_string(field, len, buf, sizeof(buf));
    if (!buf[0])
        return 0;
    char* end = NULL;
    long value = strtol(buf, &end, 10);
    if (!end || *end != '\0')
        return 0;
    return (s32b)value;
}

static u32b score_runs_parse_u32_field(const char* field, size_t len)
{
    char buf[32];
    parse_score_string(field, len, buf, sizeof(buf));
    if (!buf[0])
        return 0;
    char* end = NULL;
    unsigned long value = strtoul(buf, &end, 10);
    if (!end || *end != '\0')
        return 0;
    if (value > 0xFFFFFFFFUL)
        return 0xFFFFFFFFUL;
    return (u32b)value;
}

static u32b score_runs_parse_day_stamp(const char* field, size_t len)
{
    char buf[16];
    parse_score_string(field, len, buf, sizeof(buf));
    if (!buf[0])
        return 0;
    const char* cursor = buf;
    if (*cursor == '@')
        cursor++;
    if (strlen(cursor) < 8)
        return 0;
    char year_buf[5] = {0};
    char month_buf[3] = {0};
    char day_buf[3] = {0};
    memcpy(year_buf, cursor, 4);
    memcpy(month_buf, cursor + 4, 2);
    memcpy(day_buf, cursor + 6, 2);
    int year = atoi(year_buf);
    int month = atoi(month_buf);
    int day = atoi(day_buf);
    if (year <= 0 || month < 1 || month > 12 || day < 1 || day > 31)
        return 0;

    struct tm when;
    memset(&when, 0, sizeof(when));
    when.tm_year = year - 1900;
    when.tm_mon = month - 1;
    when.tm_mday = day;
    when.tm_hour = 12;
    when.tm_isdst = -1;
    time_t stamp = mktime(&when);
    if (stamp == (time_t)-1)
        return 0;
    return (u32b)stamp;
}

static score_record_status score_runs_status_from_legacy(const char* how_field,
                                                         size_t how_len,
                                                         const char* escaped_field)
{
    char how_buf[64];
    parse_score_string(how_field, how_len, how_buf, sizeof(how_buf));
    if (streq(how_buf, "(alive and well)"))
        return SCORE_RECORD_ALIVE;
    if (escaped_field && tolower((unsigned char)escaped_field[0]) == 't')
        return SCORE_RECORD_ESCAPED;
    return SCORE_RECORD_DEAD;
}

static void score_runs_build_record_from_legacy(score_record_v1* rec,
                                                const high_score* legacy)
{
    memset(rec, 0, sizeof(*rec));
    rec->metarun_id = SCORE_RUNS_METARUN_UNKNOWN;

    char death_text[64];
    parse_score_string(legacy->how, sizeof(legacy->how), death_text, sizeof(death_text));
    if (!death_text[0])
        SDL_strlcpy(death_text, "(unknown)", sizeof(death_text));

    rec->status = score_runs_status_from_legacy(legacy->how,
                                                sizeof(legacy->how),
                                                legacy->escaped);
    rec->run_flags = 0;
    if (tolower((unsigned char)legacy->morgoth_slain[0]) == 't')
        rec->run_flags |= SCORE_RUN_FLAG_MORGOTH_SLAIN;
    if (tolower((unsigned char)legacy->escaped[0]) == 't')
        rec->run_flags |= SCORE_RUN_FLAG_ANGBAND_ESCAPED;

    int race_idx = score_runs_parse_int_field(legacy->p_r, sizeof(legacy->p_r));
    int char_idx = score_runs_parse_int_field(legacy->p_h, sizeof(legacy->p_h));
    if (race_idx < 0) race_idx = 0;
    if (char_idx < 0) char_idx = 0;
    rec->race_id = (byte)(race_idx & 0xFF);
    rec->character_id = (byte)(char_idx & 0xFF);
    if (z_info && race_idx >= 0 && race_idx < z_info->p_max) {
        rec->race_guid = p_info[race_idx].guid;
    } else {
        score_guid_clear(&rec->race_guid);
    }
    if (z_info && char_idx >= 0 && char_idx < z_info->c_max) {
        rec->character_guid = c_info[char_idx].guid;
    } else {
        score_guid_clear(&rec->character_guid);
    }

    rec->max_depth = (u16b)MAX(0, score_runs_parse_int_field(legacy->max_dun,
        sizeof(legacy->max_dun)));
    rec->exit_depth = (u16b)MAX(0, score_runs_parse_int_field(legacy->cur_dun,
        sizeof(legacy->cur_dun)));
    rec->silmarils = (u16b)MAX(0, score_runs_parse_int_field(legacy->silmarils,
        sizeof(legacy->silmarils)));
    rec->uniques_killed = (u16b)MAX(0, score_runs_parse_int_field(legacy->cur_lev,
        sizeof(legacy->cur_lev)));
    rec->quests_completed = 0;
    rec->skills_learned = 0;
    rec->abilities_learned = 0;
    rec->artefacts_found = 0;
    rec->net_curses = (s16b)score_runs_parse_int_field(legacy->pts,
        sizeof(legacy->pts));
    rec->character_power = 0;
    rec->turns_spent = score_runs_parse_u32_field(legacy->turns,
        sizeof(legacy->turns));
    rec->xp_earned = 0;
    rec->kills_total = 0;
    rec->kills_seen = 0;

    score_runs_fill_guid(&rec->killer_guid, 0);
    rec->killer_kind = score_runs_killer_kind_from_string(death_text);
    rec->killer_race_index = 0;
    rec->cause_code = 0;
    SDL_strlcpy(rec->killer_name, death_text, sizeof(rec->killer_name));
    SDL_strlcpy(rec->cause_of_death, death_text, sizeof(rec->cause_of_death));

    rec->created_utc = score_runs_parse_day_stamp(legacy->day, sizeof(legacy->day));
    rec->completed_utc = rec->created_utc;

    char player_name[sizeof(legacy->who) + 1];
    parse_score_string(legacy->who, sizeof(legacy->who),
                       player_name, sizeof(player_name));
    if (!player_name[0])
        SDL_strlcpy(player_name, "(unknown)", sizeof(player_name));
    SDL_strlcpy(rec->player_name, player_name, sizeof(rec->player_name));

    char normalized[64];
    score_runs_normalize_name(player_name, normalized, sizeof(normalized));
    rec->persona_id = score_runs_hash_persona(normalized);

    SDL_strlcpy(rec->savefile_hint, "", sizeof(rec->savefile_hint));
}

static void score_runs_import_legacy_scores(void)
{
    if (score_runs_legacy_checked)
        return;
    score_runs_legacy_checked = true;

    log_debug("score_runs: starting legacy import");

    char score_path[1024];
    if (!build_meta_path(score_path, sizeof(score_path), "scores.raw"))
        return;

    score_file_ctx snapshot;
    score_file_reset_ctx(&snapshot);
    score_file_ctx* previous_ctx = score_file_set_active_ctx(&snapshot);

    log_debug("score_runs: opening legacy file %s", score_path);
    safe_setuid_grab();
    SDL_IOStream* source = score_file_open(score_path, O_RDONLY);
    safe_setuid_drop();
    if (!source) {
        log_debug("score_runs: scores.raw missing, import skipped");
        score_file_set_active_ctx(previous_ctx);
        return;
    }
    snapshot.fd = source;

    if (highscore_seek(0) != 0) {
        SDL_CloseIO(source);
        snapshot.fd = NULL;
        log_debug("score_runs: highscore_seek() failed");
        score_file_set_active_ctx(previous_ctx);
        return;
    }

    u32b header_entries = snapshot.entry_count;
    log_debug("score_runs: header entry_count=%u", header_entries);
    if (header_entries > MAX_HISCORES)
        header_entries = MAX_HISCORES;
    if (header_entries == 0) {
        SDL_CloseIO(source);
        snapshot.fd = NULL;
        score_file_set_active_ctx(previous_ctx);
        return;
    }

    high_score* legacy = mem_alloc_array(header_entries, high_score);
    if (!legacy) {
        log_debug("score_runs: unable to alloc %u legacy slots", header_entries);
        SDL_CloseIO(source);
        snapshot.fd = NULL;
        score_file_set_active_ctx(previous_ctx);
        return;
    }

    u32b legacy_entries = 0;
    while (legacy_entries < header_entries) {
        if (highscore_read(&legacy[legacy_entries])) {
            log_debug("score_runs: highscore_read() stopped at %u", legacy_entries);
            break;
        }
        legacy_entries++;
    }

    SDL_CloseIO(source);
    snapshot.fd = NULL;

    if (legacy_entries == 0) {
        log_debug("score_runs: no legacy entries loaded");
        mem_free(legacy);
        score_file_set_active_ctx(previous_ctx);
        return;
    }

    log_debug("score_runs: loaded %u entries (header=%u)", legacy_entries, header_entries);

    score_file_set_active_ctx(previous_ctx);

    char runs_path[1024];
    if (!build_meta_path(runs_path, sizeof(runs_path),
            SCORE_RUNS_DB_FILENAME)) {
        log_debug("score_runs: unable to build path for runs.db");
        mem_free(legacy);
        return;
    }

    score_db_header header;
    bool created = false;
    SDL_IOStream* runs_db = score_runs_open_db(runs_path, &header, &created);
    if (!runs_db) {
        log_debug("score_runs: unable to open runs.db for import");
        mem_free(legacy);
        return;
    }

    if (header.record_count >= legacy_entries) {
        log_debug("score_runs: runs.db already has %u entries (legacy=%u)",
            header.record_count, legacy_entries);
        mem_free(legacy);
        SDL_CloseIO(runs_db);
        return;
    }

    score_run_detail_block details;
    memset(&details, 0, sizeof(details));
    details.header.version = SCORE_RUN_DETAIL_VERSION;
    details.header.artefact_capacity = 0;
    details.header.monster_capacity = 0;
    details.header.artefact_count = 0;
    details.header.monster_count = 0;

    SDL_SeekIO(runs_db, 0, SDL_IO_SEEK_END);

    u32b imported = 0;
    for (u32b i = header.record_count; i < legacy_entries; ++i) {
        score_record_v1 record;
        score_runs_build_record_from_legacy(&record, &legacy[i]);

        if (record.status == SCORE_RECORD_ALIVE) {
            log_debug("score_runs: skipping legacy[%u] '%s' (alive snapshot)", i, record.player_name);
            continue;
        }

        record.record_id = header.record_count + imported;
        record.chronological_idx = record.record_id;
        log_debug("score_runs: importing legacy[%u] player='%s' day=%u status=%d depth=%u sil=%u",
            i, record.player_name, record.created_utc, record.status,
            record.max_depth, record.silmarils);
        if (!score_runs_write_record(runs_db, &record, &details)) {
            log_warn("score_runs: failed to import legacy score index %u", i);
            break;
        }
        imported++;
    }

    if (imported > 0) {
        header.record_count += imported;
        if (!score_runs_write_header(runs_db, &header)) {
            log_warn("score_runs: unable to update header after legacy import");
        } else {
            log_info("score_runs: imported %u legacy entries into runs.db", imported);
        }
    } else {
        log_debug("score_runs: no new records imported (already up to date) legacy=%u", legacy_entries);
    }

    if (imported != legacy_entries) {
        log_debug("score_runs: import mismatch legacy=%u imported=%u",
            legacy_entries, imported);
    }

    mem_free(legacy);
    SDL_CloseIO(runs_db);
}

static void score_runs_init_header(score_db_header* header)
{
    memset(header, 0, sizeof(*header));
    memcpy(header->magic, SCORE_DB_MAGIC, sizeof(header->magic));
    header->version = SCORE_RUNS_DB_VERSION;
}

static bool score_runs_read_header(SDL_IOStream* file, score_db_header* header)
{
    if (!file || !header)
        return false;

    if (SDL_SeekIO(file, 0, SDL_IO_SEEK_SET) < 0)
        return false;

    if (SDL_ReadIO(file, header, sizeof(*header)) != sizeof(*header))
        return false;

    if (memcmp(header->magic, SCORE_DB_MAGIC, sizeof(header->magic)) != 0)
        return false;

    return true;
}

static bool score_runs_write_header(SDL_IOStream* file, const score_db_header* header)
{
    if (!file || !header)
        return false;

    if (SDL_SeekIO(file, 0, SDL_IO_SEEK_SET) < 0)
        return false;

    return SDL_WriteIO(file, header, sizeof(*header)) == sizeof(*header);
}

static u16b score_runs_choose_artefact_capacity(void)
{
    if (!z_info)
        return 0;
    u32b cap = z_info->art_max;
    if (cap > SCORE_RUN_ARTEFACT_CAP_MAX)
        cap = SCORE_RUN_ARTEFACT_CAP_MAX;
    if (cap > UINT16_MAX)
        cap = UINT16_MAX;
    return (u16b)cap;
}

static u16b score_runs_choose_monster_capacity(void)
{
    if (!z_info)
        return 0;
    u32b cap = z_info->r_max;
    if (cap > SCORE_RUN_MONSTER_CAP_MAX)
        cap = SCORE_RUN_MONSTER_CAP_MAX;
    if (cap > UINT16_MAX)
        cap = UINT16_MAX;
    return (u16b)cap;
}

static bool score_runs_alloc_detail_block(score_run_detail_block* block,
                                          u16b artefact_cap,
                                          u16b monster_cap)
{
    if (!block)
        return false;

    memset(block, 0, sizeof(*block));
    block->header.version = SCORE_RUN_DETAIL_VERSION;
    block->header.artefact_capacity = artefact_cap;
    block->header.monster_capacity = monster_cap;

    if (artefact_cap > 0) {
        block->artefacts = mem_alloc_array(artefact_cap, score_run_artefact_v1);
        if (!block->artefacts)
            return false;
        memset(block->artefacts, 0,
            artefact_cap * sizeof(score_run_artefact_v1));
    }

    if (monster_cap > 0) {
        block->monsters = mem_alloc_array(monster_cap, score_run_monster_v1);
        if (!block->monsters) {
            mem_free(block->artefacts);
            block->artefacts = NULL;
            return false;
        }
        memset(block->monsters, 0,
            monster_cap * sizeof(score_run_monster_v1));
    }

    return true;
}

static void score_runs_release_detail_block(score_run_detail_block* block)
{
    if (!block)
        return;
    mem_free(block->stats);
    mem_free(block->skills);
    mem_free(block->abilities);
    mem_free(block->milestones);
    mem_free(block->artefacts);
    mem_free(block->monsters);
    memset(block, 0, sizeof(*block));
}

static bool score_runs_read_detail_header(SDL_IOStream* file,
                                          score_run_detail_header_v1* header)
{
    if (!file || !header)
        return false;
    return SDL_ReadIO(file, header, sizeof(*header)) == sizeof(*header);
}

bool score_runs_skip_detail_payload(SDL_IOStream* file,
                                    const score_run_detail_header_v1* header)
{
    if (!file || !header)
        return false;

    if (header->version >= 2) {
        u16b stats_count = 0;
        u16b skills_count = 0;
        u16b ability_count = 0;
        u16b milestone_count = 0;

        if (SDL_ReadIO(file, &stats_count, sizeof(stats_count)) != sizeof(stats_count))
            return false;
        if (SDL_SeekIO(file,
                       (Sint64)stats_count * (Sint64)sizeof(score_run_stat_v1),
                       SDL_IO_SEEK_CUR) < 0)
            return false;

        if (SDL_ReadIO(file, &skills_count, sizeof(skills_count)) != sizeof(skills_count))
            return false;
        if (SDL_SeekIO(file,
                       (Sint64)skills_count * (Sint64)sizeof(score_run_skill_v1),
                       SDL_IO_SEEK_CUR) < 0)
            return false;

        if (SDL_ReadIO(file, &ability_count, sizeof(ability_count)) != sizeof(ability_count))
            return false;
        if (SDL_SeekIO(file,
                       (Sint64)ability_count * (Sint64)sizeof(score_run_ability_v1),
                       SDL_IO_SEEK_CUR) < 0)
            return false;

        if (SDL_ReadIO(file, &milestone_count, sizeof(milestone_count)) != sizeof(milestone_count))
            return false;
        if (SDL_SeekIO(file,
                       (Sint64)milestone_count * (Sint64)sizeof(score_run_milestone_v1),
                       SDL_IO_SEEK_CUR) < 0)
            return false;
    } else {
        /* Older detail payloads have no extra sections to skip */
    }

    Sint64 skip = 0;
    skip += (Sint64)header->artefact_capacity
        * (Sint64)sizeof(score_run_artefact_v1);
    skip += (Sint64)header->monster_capacity
        * (Sint64)sizeof(score_run_monster_v1);
    if (skip < 0)
        return false;
    return SDL_SeekIO(file, skip, SDL_IO_SEEK_CUR) >= 0;
}

static bool score_runs_read_detail_header_at(SDL_IOStream* file, Sint64 record_offset,
                                             score_run_detail_header_v1* header)
{
    if (!file || !header)
        return false;
    if (SDL_SeekIO(file,
                   record_offset + (Sint64)sizeof(score_record_v1),
                   SDL_IO_SEEK_SET) < 0)
        return false;
    return score_runs_read_detail_header(file, header);
}

static bool score_runs_write_record(SDL_IOStream* file,
                                    const score_record_v1* record,
                                    const score_run_detail_block* details)
{
    if (!file || !record || !details)
        return false;

    if (SDL_WriteIO(file, record, sizeof(*record)) != sizeof(*record))
        return false;

    if (SDL_WriteIO(file, &details->header, sizeof(details->header))
        != sizeof(details->header))
        return false;

    if (details->header.version >= 2) {
        u16b stats_count = details->stats_count;
        u16b skills_count = details->skills_count;
        u16b ability_count = details->ability_count;
        u16b milestone_count = details->milestone_count;

        if (SDL_WriteIO(file, &stats_count, sizeof(stats_count)) != sizeof(stats_count))
            return false;
        if (stats_count > 0) {
            size_t bytes = (size_t)stats_count * sizeof(score_run_stat_v1);
            if (!details->stats ||
                SDL_WriteIO(file, details->stats, bytes) != bytes)
                return false;
        }

        if (SDL_WriteIO(file, &skills_count, sizeof(skills_count)) != sizeof(skills_count))
            return false;
        if (skills_count > 0) {
            size_t bytes = (size_t)skills_count * sizeof(score_run_skill_v1);
            if (!details->skills ||
                SDL_WriteIO(file, details->skills, bytes) != bytes)
                return false;
        }

        if (SDL_WriteIO(file, &ability_count, sizeof(ability_count)) != sizeof(ability_count))
            return false;
        if (ability_count > 0) {
            size_t bytes = (size_t)ability_count * sizeof(score_run_ability_v1);
            if (!details->abilities ||
                SDL_WriteIO(file, details->abilities, bytes) != bytes)
                return false;
        }

        if (SDL_WriteIO(file, &milestone_count, sizeof(milestone_count)) != sizeof(milestone_count))
            return false;
        if (milestone_count > 0) {
            size_t bytes = (size_t)milestone_count * sizeof(score_run_milestone_v1);
            if (!details->milestones ||
                SDL_WriteIO(file, details->milestones, bytes) != bytes)
                return false;
        }
    }

    size_t artefact_bytes = (size_t)details->header.artefact_capacity
        * sizeof(score_run_artefact_v1);
    if (artefact_bytes > 0 && details->artefacts) {
        if (SDL_WriteIO(file, details->artefacts, artefact_bytes)
            != artefact_bytes)
            return false;
    } else if (artefact_bytes > 0) {
        return false;
    }

    size_t monster_bytes = (size_t)details->header.monster_capacity
        * sizeof(score_run_monster_v1);
    if (monster_bytes > 0 && details->monsters) {
        if (SDL_WriteIO(file, details->monsters, monster_bytes)
            != monster_bytes)
            return false;
    } else if (monster_bytes > 0) {
        return false;
    }

    return true;
}

static SDL_IOStream* score_runs_open_db(const char* path, score_db_header* header,
                                        bool* created)
{
    if (created)
        *created = false;
    SDL_IOStream* file = SDL_IOFromFile(path, "r+b");
    if (!file) {
        file = SDL_IOFromFile(path, "w+b");
        if (!file)
            return NULL;
        if (created)
            *created = true;
    }

    if (created && *created) {
        score_runs_init_header(header);
        (void)score_runs_write_header(file, header);
        SDL_SeekIO(file, 0, SDL_IO_SEEK_END);
        return file;
    }

    bool need_reset = false;
    if (!score_runs_read_header(file, header) ||
        header->version != SCORE_RUNS_DB_VERSION) {
        log_warn("score_runs: invalid or legacy header in %s, recreating", path);
        need_reset = true;
    }

    if (need_reset) {
        SDL_CloseIO(file);
        file = SDL_IOFromFile(path, "w+b");
        if (!file)
            return NULL;
        if (created)
            *created = true;
        score_runs_init_header(header);
        if (SDL_WriteIO(file, header, sizeof(*header)) != sizeof(*header)) {
            SDL_CloseIO(file);
            return NULL;
        }
    }

    SDL_SeekIO(file, 0, SDL_IO_SEEK_END);
    return file;
}

static bool score_runs_find_existing(SDL_IOStream* file, u32b metarun_id,
                                     u32b persona_id,
                                     score_record_v1* existing,
                                     Sint64* offset)
{
    if (!file)
        return false;

    if (SDL_SeekIO(file, sizeof(score_db_header), SDL_IO_SEEK_SET) < 0)
        return false;

    score_record_v1 temp;
    score_run_detail_header_v1 detail;
    Sint64 pos = SDL_TellIO(file);
    while (SDL_ReadIO(file, &temp, sizeof(temp)) == sizeof(temp)) {
        if (temp.metarun_id == metarun_id &&
            temp.persona_id == persona_id &&
            temp.status == SCORE_RECORD_ALIVE) {
            if (existing)
                *existing = temp;
            if (offset)
                *offset = pos;
            SDL_SeekIO(file, 0, SDL_IO_SEEK_END);
            return true;
        }
        if (!score_runs_read_detail_header(file, &detail) ||
            !score_runs_skip_detail_payload(file, &detail)) {
            break;
        }
        pos = SDL_TellIO(file);
    }

    SDL_SeekIO(file, 0, SDL_IO_SEEK_END);
    return false;
}

static int score_runs_count_for_metarun(SDL_IOStream* file, u32b metarun_id)
{
    if (!file)
        return 0;

    if (SDL_SeekIO(file, sizeof(score_db_header), SDL_IO_SEEK_SET) < 0)
        return 0;

    score_record_v1 temp;
    score_run_detail_header_v1 detail;
    int count = 0;
    while (SDL_ReadIO(file, &temp, sizeof(temp)) == sizeof(temp)) {
        if (temp.metarun_id == metarun_id)
            count++;
        if (!score_runs_read_detail_header(file, &detail) ||
            !score_runs_skip_detail_payload(file, &detail)) {
            break;
        }
    }

    SDL_SeekIO(file, 0, SDL_IO_SEEK_END);
    return count;
}

static void score_runs_normalize_name(const char* name, char* out, size_t out_len)
{
    if (!out || out_len == 0)
        return;

    if (!name) {
        out[0] = '\0';
        return;
    }

    size_t out_pos = 0;
    bool pending_space = false;

    while (*name && isspace((unsigned char)*name))
        name++;

    while (*name && out_pos + 1 < out_len) {
        unsigned char ch = (unsigned char)*name++;
        if (isspace(ch)) {
            pending_space = true;
            continue;
        }
        if (pending_space && out_pos > 0 && out_pos + 1 < out_len) {
            out[out_pos++] = ' ';
        }
        pending_space = false;
        out[out_pos++] = (char)tolower(ch);
    }

    if (out_pos > 0 && out[out_pos - 1] == ' ')
        out_pos--;

    out[out_pos] = '\0';
}

static u32b score_runs_hash_persona(const char* normalized)
{
    const u32b fnv_offset = 2166136261u;
    const u32b fnv_prime = 16777619u;
    u32b hash = fnv_offset;
    if (!normalized)
        return hash;

    while (*normalized) {
        hash ^= (unsigned char)*normalized++;
        hash *= fnv_prime;
    }
    return hash;
}

static void score_runs_fill_guid(score_guid64* guid, u64b value)
{
    if (!guid)
        return;
    *guid = score_guid_from_u64(value);
}

static u16b score_runs_completed_quests(void)
{
    u16b count = 0;
    if (p_ptr->tulkas_quest >= TULKAS_QUEST_COMPLETE)
        count++;
    if (p_ptr->aule_quest >= AULE_QUEST_SUCCESS)
        count++;
    if (p_ptr->mandos_quest >= MANDOS_QUEST_SUCCESS)
        count++;
    if (p_ptr->niena_quest >= NIENA_QUEST_SUCCESS)
        count++;
    if (p_ptr->orome_quest >= OROME_QUEST_SUCCESS)
        count++;
    if (p_ptr->varda_quest >= VARDA_QUEST_SUCCESS)
        count++;
    return count;
}

static u16b score_runs_skills_learned(void)
{
    u32b total = 0;
    for (int i = 0; i < S_MAX; i++) {
        if (i == S_SPC)
            continue;
        if (p_ptr->skill_base[i] > 0)
            total += (u32b)p_ptr->skill_base[i];
    }
    if (total > UINT16_MAX)
        total = UINT16_MAX;
    return (u16b)total;
}

static u16b score_runs_abilities_learned(void)
{
    u32b total = 0;
    for (int skill = 0; skill < S_MAX; skill++) {
        for (int abil = 0; abil < ABILITIES_MAX; abil++) {
            if (p_ptr->innate_ability[skill][abil])
                total++;
        }
    }
    if (total > UINT16_MAX)
        total = UINT16_MAX;
    return (u16b)total;
}

static s16b score_runs_net_curses(void)
{
    s32b net = 0;
    for (int id = 0; id < METAR_CURSE_SLOTS; ++id) {
        net += CURSE_GET(id);
    }
    if (net > INT16_MAX)
        return INT16_MAX;
    if (net < INT16_MIN)
        return INT16_MIN;
    return (s16b)net;
}

static u16b score_runs_artefacts_found(void)
{
    if (!a_info || !z_info)
        return 0;

    u32b total = 0;
    for (int i = 0; i < z_info->art_max; i++) {
        artefact_type* a_ptr = &a_info[i];
        if (a_ptr->cur_num > 0 && !(a_ptr->flags3 & TR3_INSTA_ART))
            total++;
    }
    if (total > UINT16_MAX)
        total = UINT16_MAX;
    return (u16b)total;
}

static void score_runs_collect_kill_totals(u32b* kills_total, u32b* kills_seen)
{
    u32b total = 0;
    u32b seen = 0;
    if (!l_list || !z_info)
        goto store;

    for (int i = 1; i < z_info->r_max; i++) {
        monster_lore* lore = &l_list[i];
        if (lore->pkills > 0)
            total += (u32b)lore->pkills;
        if (lore->psights > 0)
            seen += (u32b)lore->psights;
    }

store:
    if (kills_total)
        *kills_total = total;
    if (kills_seen)
        *kills_seen = seen;
}

static u16b score_runs_collect_artefact_entries(score_run_artefact_v1* entries,
                                                u16b capacity)
{
    if (!entries || capacity == 0 || !a_info || !z_info)
        return 0;

    u16b count = 0;
    for (int i = 0; i < z_info->art_max && count < capacity; i++) {
        artefact_type* art = &a_info[i];
        if (!art)
            continue;
        if (art->cur_num <= 0)
            continue;
        if (art->flags3 & TR3_INSTA_ART)
            continue;

        score_run_artefact_v1* slot = &entries[count++];
        slot->guid = art->guid;
        slot->a_idx = (u16b)i;
        slot->tval = art->tval;
        slot->sval = art->sval;
        slot->forged = (i >= z_info->art_rand_max) ? 1 : 0;
    }

    return count;
}

static u16b score_runs_collect_monster_entries(score_run_monster_v1* entries,
                                               u16b capacity)
{
    if (!entries || capacity == 0 || !l_list || !r_info || !z_info)
        return 0;

    u16b count = 0;
    for (int i = 1; i < z_info->r_max && count < capacity; i++) {
        monster_lore* lore = &l_list[i];
        if (!lore)
            continue;
        int seen = MAX(lore->psights, 0);
        int killed = MAX(lore->pkills, 0);
        int deaths = MAX(lore->deaths, 0);
        if (seen == 0 && killed == 0 && deaths == 0)
            continue;

        monster_race* race = &r_info[i];
        score_run_monster_v1* slot = &entries[count++];
        if (race)
            slot->guid = score_guid_from_u64(race->guid);
        else
            score_guid_clear(&slot->guid);
        slot->r_idx = (u16b)i;
        slot->seen = (seen > UINT16_MAX) ? UINT16_MAX : (u16b)seen;
        slot->killed = (killed > UINT16_MAX) ? UINT16_MAX : (u16b)killed;
        slot->deaths = (deaths > UINT16_MAX) ? UINT16_MAX : (u16b)deaths;
    }

    return count;
}

static u16b score_runs_collect_stat_entries(score_run_stat_v1* entries,
                                            u16b capacity)
{
    if (!entries || capacity == 0 || !p_ptr)
        return 0;

    u16b count = 0;
    for (int stat = 0; stat < A_MAX && count < capacity; stat++) {
        score_run_stat_v1* slot = &entries[count++];
        slot->stat_index = (byte)stat;
        slot->reserved = 0;
        slot->base = p_ptr->stat_base[stat];
        slot->drain = p_ptr->stat_drain[stat];
        slot->current = p_ptr->stat_use[stat];
    }
    return count;
}

static u16b score_runs_collect_skill_entries(score_run_skill_v1* entries,
                                             u16b capacity)
{
    if (!entries || capacity == 0 || !p_ptr)
        return 0;

    u16b count = 0;
    for (int skill = 0; skill < S_MAX && count < capacity; skill++) {
        score_run_skill_v1* slot = &entries[count++];
        slot->skill_index = (byte)skill;
        slot->reserved = 0;
        slot->base = p_ptr->skill_base[skill];
        slot->current = p_ptr->skill_use[skill];
        slot->stat_bonus = p_ptr->skill_stat_mod[skill];
        int bonus = p_ptr->skill_equip_mod[skill] + p_ptr->skill_misc_mod[skill];
        if (bonus > INT16_MAX)
            slot->item_bonus = INT16_MAX;
        else if (bonus < INT16_MIN)
            slot->item_bonus = INT16_MIN;
        else
            slot->item_bonus = (s16b)bonus;
    }
    return count;
}

static u16b score_runs_collect_ability_entries(score_run_ability_v1* entries,
                                               u16b capacity)
{
    if (!entries || capacity == 0 || !p_ptr)
        return 0;

    u16b total = p_ptr->ability_timeline_count;
    if (total > capacity)
        total = capacity;

    for (u16b i = 0; i < total; i++) {
        score_run_ability_v1* slot = &entries[i];
        slot->skill_index = p_ptr->ability_timeline_skill[i];
        slot->ability_index = p_ptr->ability_timeline_ability[i];
        slot->order = (u16b)(i + 1);
        slot->player_turn = p_ptr->ability_timeline_turn[i];
        slot->depth = p_ptr->ability_timeline_depth[i];
        slot->reserved = 0;
    }
    return total;
}

static size_t score_runs_trim_copy(const char* src, size_t len,
                                   char* dst, size_t dst_len)
{
    while (len > 0 && isspace((unsigned char)*src)) {
        src++;
        len--;
    }
    while (len > 0 && isspace((unsigned char)src[len - 1]))
        len--;
    if (!dst || dst_len == 0)
        return len;
    size_t copy = (len < dst_len - 1) ? len : dst_len - 1;
    if (copy > 0)
        memcpy(dst, src, copy);
    dst[copy] = '\0';
    return copy;
}

static bool score_runs_is_milestone_header(const char* line, size_t len)
{
    if (len < 5)
        return false;

    size_t i = 0;
    while (i < len && line[i] == ' ')
        i++;

    bool has_digit = false;
    while (i < len && (isdigit((unsigned char)line[i]) || line[i] == ',')) {
        has_digit = true;
        i++;
    }
    if (!has_digit)
        return false;

    size_t spaces = 0;
    while (i < len && line[i] == ' ') {
        spaces++;
        i++;
    }
    if (spaces < 2)
        return false;

    for (size_t j = i; j + 2 < len; j++) {
        if (line[j] == ' ' && line[j + 1] == ' ' && line[j + 2] == ' ')
            return true;
    }

    return false;
}

static u32b score_runs_parse_turn_field(const char* line, size_t len)
{
    char digits[32];
    size_t pos = 0;
    size_t i = 0;

    while (i < len && line[i] == ' ')
        i++;

    while (i < len && (isdigit((unsigned char)line[i]) || line[i] == ',')) {
        if (isdigit((unsigned char)line[i]) && pos + 1 < sizeof(digits))
            digits[pos++] = line[i];
        i++;
    }

    digits[pos] = '\0';
    if (pos == 0)
        return 0;
    return (u32b)strtoul(digits, NULL, 10);
}

static void score_runs_extract_depth(const char* line, size_t start,
                                     size_t end, score_run_milestone_v1* entry)
{
    if (!entry || end <= start)
        return;
    score_runs_trim_copy(line + start, end - start,
        entry->depth_label, sizeof(entry->depth_label));

    char digits[16];
    size_t pos = 0;
    for (size_t i = 0; entry->depth_label[i] && pos + 1 < sizeof(digits); i++) {
        if (isdigit((unsigned char)entry->depth_label[i]))
            digits[pos++] = entry->depth_label[i];
    }
    digits[pos] = '\0';
    if (pos == 0) {
        entry->depth = 0;
        return;
    }
    long feet = strtol(digits, NULL, 10);
    if (feet < 0)
        feet = 0;
    entry->depth = (s16b)(feet / 50);
}

static void score_runs_append_milestone_text(score_run_milestone_v1* entry,
                                             const char* line, size_t len)
{
    if (!entry || len == 0)
        return;
    char buffer[SCORE_RUN_MILESTONE_TEXT_MAX];
    size_t copy = score_runs_trim_copy(line, len, buffer, sizeof(buffer));
    if (copy == 0)
        return;
    size_t cur = strlen(entry->note);
    if (cur > 0 && cur + 1 < sizeof(entry->note)) {
        entry->note[cur++] = ' ';
        entry->note[cur] = '\0';
    }
    SDL_strlcat(entry->note, buffer, sizeof(entry->note));
}

static void score_runs_parse_milestone_header(const char* line, size_t len,
                                              score_run_milestone_v1* entry)
{
    if (!entry)
        return;
    memset(entry, 0, sizeof(*entry));
    entry->player_turn = score_runs_parse_turn_field(line, len);

    size_t depth_start = 0;
    size_t depth_end = len;
    size_t i = 0;

    while (i < len && line[i] == ' ')
        i++;
    while (i < len && (isdigit((unsigned char)line[i]) || line[i] == ','))
        i++;
    size_t spacer = 0;
    while (i < len && line[i] == ' ' && spacer < 2) {
        i++;
        spacer++;
    }
    depth_start = i;

    for (; i + 2 < len; i++) {
        if (line[i] == ' ' && line[i + 1] == ' ' && line[i + 2] == ' ') {
            depth_end = i;
            size_t text_start = i + 3;
            if (text_start < len)
                score_runs_append_milestone_text(entry, line + text_start, len - text_start);
            break;
        }
    }
    if (depth_end < depth_start)
        depth_end = depth_start;
    score_runs_extract_depth(line, depth_start, depth_end, entry);
}

static u16b score_runs_collect_milestones(score_run_milestone_v1* entries,
                                          u16b capacity)
{
    if (capacity == 0 || !notes_buffer[0])
        return 0;

    u16b stored = 0;
    score_run_milestone_v1 current;
    bool have_current = false;
    const char* cursor = notes_buffer;

    while (*cursor) {
        size_t len = 0;
        while (cursor[len] && cursor[len] != '\n')
            len++;
        bool header = score_runs_is_milestone_header(cursor, len);
        if (header) {
            if (have_current && stored < capacity) {
                if (entries)
                    entries[stored] = current;
                stored++;
            }
            have_current = true;
            score_runs_parse_milestone_header(cursor, len, &current);
        } else if (have_current) {
            score_runs_append_milestone_text(&current, cursor, len);
        }

        if (!cursor[len])
            break;
        cursor += len + 1;

        if (!entries && stored >= capacity)
            break;
    }

    if (have_current && stored < capacity) {
        if (entries)
            entries[stored] = current;
        stored++;
    }

    if (stored > capacity)
        stored = capacity;

    return stored;
}
static bool score_runs_build_details(score_run_detail_block* block,
                                     u16b artefact_cap,
                                     u16b monster_cap)
{
    if (!score_runs_alloc_detail_block(block, artefact_cap, monster_cap))
        return false;

    if (p_ptr) {
        block->stats = mem_alloc_array(A_MAX, score_run_stat_v1);
        if (!block->stats)
            goto fail;
        block->stats_count = score_runs_collect_stat_entries(
            block->stats, (u16b)A_MAX);

        block->skills = mem_alloc_array(S_MAX, score_run_skill_v1);
        if (!block->skills)
            goto fail;
        block->skills_count = score_runs_collect_skill_entries(
            block->skills, (u16b)S_MAX);

        ability_log_sync_missing();
        u16b ability_cap = p_ptr->ability_timeline_count;
        if (ability_cap > ABILITY_TIMELINE_MAX)
            ability_cap = ABILITY_TIMELINE_MAX;
        if (ability_cap > 0) {
            block->abilities = mem_alloc_array(ability_cap, score_run_ability_v1);
            if (!block->abilities)
                goto fail;
        }
        block->ability_count = score_runs_collect_ability_entries(
            block->abilities, ability_cap);
    }

    u16b milestone_cap = score_runs_collect_milestones(
        NULL, SCORE_RUN_MILESTONE_CAP_MAX);
    if (milestone_cap > 0) {
        block->milestones = mem_alloc_array(milestone_cap,
            score_run_milestone_v1);
        if (!block->milestones)
            goto fail;
        block->milestone_count = score_runs_collect_milestones(
            block->milestones, milestone_cap);
    }

    block->header.artefact_count = score_runs_collect_artefact_entries(
        block->artefacts, block->header.artefact_capacity);
    block->header.monster_count = score_runs_collect_monster_entries(
        block->monsters, block->header.monster_capacity);
    return true;

fail:
    score_runs_release_detail_block(block);
    return false;
}

static s16b score_runs_character_power(void)
{
    int power = 3;
    bool gift = false;

    if (z_info && p_ptr->pcharacter < z_info->c_max) {
        power = c_info[p_ptr->pcharacter].power;
        gift = (c_info[p_ptr->pcharacter].flags & RHF_GIFTERU) != 0;
    }
    if (z_info && p_ptr->prace < z_info->p_max) {
        if (p_info[p_ptr->prace].flags & RHF_GIFTERU)
            gift = true;
    }
    if (gift && power > 0)
        power--;
    if (power > INT16_MAX)
        power = INT16_MAX;
    if (power < INT16_MIN)
        power = INT16_MIN;
    return (s16b)power;
}

static score_killer_kind score_runs_killer_kind_from_string(const char* how)
{
    if (!how || !*how)
        return SCORE_KILLER_OTHER;

    char lowered[64];
    size_t n = strlen(how);
    if (n >= sizeof(lowered))
        n = sizeof(lowered) - 1;
    for (size_t i = 0; i < n; i++) {
        lowered[i] = (char)tolower((unsigned char)how[i]);
    }
    lowered[n] = '\0';

    if (strstr(lowered, "own hand") || strstr(lowered, "suicide"))
        return SCORE_KILLER_SELF;
    if (strstr(lowered, "trap") || strstr(lowered, "dart") || strstr(lowered, "snare"))
        return SCORE_KILLER_TRAP;
    if (strstr(lowered, "fell") || strstr(lowered, "fall") || strstr(lowered, "chasm"))
        return SCORE_KILLER_FALL;
    if (strstr(lowered, "starvation") || strstr(lowered, "poison") ||
        strstr(lowered, "wound") || strstr(lowered, "strain") || strstr(lowered, "stress"))
        return SCORE_KILLER_OTHER;

    return SCORE_KILLER_MONSTER;
}

static void score_runs_savefile_hint(char* out, size_t len)
{
    if (!out || len == 0)
        return;

    if (!savefile[0]) {
        out[0] = '\0';
        return;
    }

    const char* stem = savefile;
    for (const char* cursor = savefile; *cursor; ++cursor) {
        if (*cursor == '/' || *cursor == '\\')
            stem = cursor + 1;
    }

    SDL_strlcpy(out, stem, len);
}

static u16b score_runs_silmarils(void)
{
    int sil = silmarils_possessed();
    if (sil < 0)
        sil = 0;
    if (p_ptr->morgoth_slain && sil < 3)
        sil = 3;
    if (sil > 0xFFFF)
        sil = 0xFFFF;
    return (u16b)sil;
}

static byte score_runs_run_flags(void)
{
    byte flags = 0;
    if (p_ptr->morgoth_slain)
        flags |= SCORE_RUN_FLAG_MORGOTH_SLAIN;
    if (p_ptr->escaped)
        flags |= SCORE_RUN_FLAG_ANGBAND_ESCAPED;
    if (p_ptr->noscore & 0x000F)
        flags |= SCORE_RUN_FLAG_NOSCORE;
    if (p_ptr->wizard || (p_ptr->noscore & 0x0008))
        flags |= SCORE_RUN_FLAG_CHEAT;
    return flags;
}

static void score_runs_build_record(score_record_v1* rec,
                                    const struct high_score* legacy,
                                    time_t snapshot_time,
                                    score_record_status status)
{
    memset(rec, 0, sizeof(*rec));
    rec->metarun_id = metar.id;
    rec->persona_id = 0;
    rec->created_utc = (u32b)snapshot_time;
    rec->completed_utc = (u32b)snapshot_time;
    rec->status = status;
    rec->run_flags = score_runs_run_flags();
    rec->race_id = (byte)(p_ptr->prace & 0xFF);
    rec->character_id = (byte)(p_ptr->pcharacter & 0xFF);
    if (z_info && p_ptr->prace < z_info->p_max) {
        rec->race_guid = p_info[p_ptr->prace].guid;
    } else {
        score_guid_clear(&rec->race_guid);
    }
    if (z_info && p_ptr->pcharacter < z_info->c_max) {
        rec->character_guid = c_info[p_ptr->pcharacter].guid;
    } else {
        score_guid_clear(&rec->character_guid);
    }
    rec->max_depth = (u16b)MAX(p_ptr->max_depth, 0);
    rec->exit_depth = (u16b)MAX(p_ptr->depth, 0);
    rec->silmarils = score_runs_silmarils();
    rec->uniques_killed = (u16b)unique_bane_type_killed();
    rec->quests_completed = score_runs_completed_quests();
    rec->skills_learned = score_runs_skills_learned();
    rec->abilities_learned = score_runs_abilities_learned();
    rec->artefacts_found = score_runs_artefacts_found();
    rec->net_curses = score_runs_net_curses();
    rec->character_power = score_runs_character_power();
    rec->turns_spent = (playerturn >= 0) ? (u32b)playerturn : 0;
    rec->xp_earned = (p_ptr->exp >= 0) ? (u32b)p_ptr->exp : 0;
    score_runs_collect_kill_totals(&rec->kills_total, &rec->kills_seen);

    score_runs_fill_guid(&rec->killer_guid, 0);
    rec->killer_kind = score_runs_killer_kind_from_string(p_ptr->died_from);
    rec->killer_race_index = 0;
    rec->cause_code = 0;

    const killer_info* killer = killer_last();
    if (status != SCORE_RECORD_ALIVE && killer && killer->valid) {
        rec->killer_kind = killer->kind;
        rec->killer_race_index = killer->race_index;
        rec->killer_guid = killer->guid;
    }

    if (*p_ptr->died_from) {
        SDL_strlcpy(rec->killer_name, p_ptr->died_from, sizeof(rec->killer_name));
        SDL_strlcpy(rec->cause_of_death, p_ptr->died_from, sizeof(rec->cause_of_death));
    } else {
        SDL_strlcpy(rec->killer_name, "(unknown)", sizeof(rec->killer_name));
        SDL_strlcpy(rec->cause_of_death, "(unknown)", sizeof(rec->cause_of_death));
    }

    score_runs_savefile_hint(rec->savefile_hint, sizeof(rec->savefile_hint));
    SDL_strlcpy(rec->player_name, op_ptr->full_name, sizeof(rec->player_name));

    char normalized[64];
    score_runs_normalize_name(op_ptr->full_name, normalized, sizeof(normalized));
    rec->persona_id = score_runs_hash_persona(normalized);

    (void)legacy;
}

bool score_runs_record_current_run(const struct high_score* legacy_score,
                                   time_t snapshot_time,
                                   score_record_status status)
{
    if (!legacy_score) {
        log_warn("score_runs: skipping record (no legacy score snapshot)");
        return false;
    }

    score_runs_import_legacy_scores();

    char path[1024];
    if (!build_meta_path(path, sizeof(path), SCORE_RUNS_DB_FILENAME)) {
        log_warn("score_runs: unable to build path for runs database");
        return false;
    }

    score_record_v1 record;
    score_runs_build_record(&record, legacy_score, snapshot_time, status);
    u16b default_art_cap = score_runs_choose_artefact_capacity();
    u16b default_mon_cap = score_runs_choose_monster_capacity();
    score_run_detail_block details;
    memset(&details, 0, sizeof(details));

    safe_setuid_grab();

    score_db_header header;
    bool created = false;
    SDL_IOStream* db = score_runs_open_db(path, &header, &created);
    if (!db) {
        safe_setuid_drop();
        log_warn("score_runs: unable to open %s", path);
        return false;
    }

    score_record_v1 existing;
    Sint64 offset = -1;
    bool found = score_runs_find_existing(db, record.metarun_id,
                                          record.persona_id, &existing, &offset);

    score_run_detail_header_v1 existing_detail;
    bool details_ready = false;
    if (found) {
        if (!score_runs_read_detail_header_at(db, offset, &existing_detail)) {
            log_warn("score_runs: detail header missing for record_id=%u, appending new entry",
                existing.record_id);
            found = false;
        } else if (existing_detail.version != SCORE_RUN_DETAIL_VERSION) {
            log_warn("score_runs: detail payload version mismatch for record_id=%u "
                "(stored=%u, expected=%u) - rebuilding with defaults",
                existing.record_id, (unsigned)existing_detail.version,
                (unsigned)SCORE_RUN_DETAIL_VERSION);
            details_ready = false;
        } else if (!score_runs_build_details(&details,
                                             existing_detail.artefact_capacity,
                                             existing_detail.monster_capacity)) {
            log_warn("score_runs: unable to rebuild detail payload for record_id=%u",
                existing.record_id);
            SDL_CloseIO(db);
            safe_setuid_drop();
            return false;
        } else {
            details_ready = true;
        }
    }

    if (!details_ready) {
        if (!score_runs_build_details(&details, default_art_cap, default_mon_cap)) {
            log_warn("score_runs: unable to gather run detail payload");
            SDL_CloseIO(db);
            safe_setuid_drop();
            return false;
        }
        details_ready = true;
    }

    bool success = false;
    if (found) {
        record.record_id = existing.record_id;
        record.chronological_idx = existing.chronological_idx;
        if (SDL_SeekIO(db, offset, SDL_IO_SEEK_SET) >= 0 &&
            score_runs_write_record(db, &record, &details)) {
            success = true;
        } else {
            log_warn("score_runs: failed to update record_id=%u", record.record_id);
        }
        SDL_SeekIO(db, 0, SDL_IO_SEEK_END);
    } else {
        record.record_id = header.record_count;
        record.chronological_idx = (u32b)score_runs_count_for_metarun(db, record.metarun_id);
        if (score_runs_write_record(db, &record, &details)) {
            header.record_count++;
            if (score_runs_write_header(db, &header)) {
                success = true;
            } else {
                log_warn("score_runs: failed to refresh runs.db header");
            }
        } else {
            log_warn("score_runs: failed to append record_id=%u to %s",
                     record.record_id, path);
        }
    }

    score_runs_release_detail_block(&details);
    SDL_CloseIO(db);
    safe_setuid_drop();

    if (success) {
        log_info("score_runs: recorded run #%u for metarun %u (chron=%u status=%d)",
                 record.record_id, record.metarun_id, record.chronological_idx,
                 record.status);
    }

    return success;
}

bool score_runs_load_details(s64b detail_offset, score_run_detail_block* out)
{
    if (!out || detail_offset < 0)
        return false;

    memset(out, 0, sizeof(*out));

    char path[1024];
    if (!build_meta_path(path, sizeof(path), SCORE_RUNS_DB_FILENAME))
        return false;

    safe_setuid_grab();
    SDL_IOStream* file = SDL_IOFromFile(path, "rb");
    safe_setuid_drop();
    if (!file)
        return false;

    bool ok = false;
    if (SDL_SeekIO(file, (Sint64)detail_offset, SDL_IO_SEEK_SET) < 0)
        goto done;

    score_run_detail_header_v1 header;
    if (!score_runs_read_detail_header(file, &header))
        goto done;

    if (!score_runs_alloc_detail_block(out,
            header.artefact_capacity, header.monster_capacity))
        goto done;
    out->header = header;

    if (header.version >= 2) {
        u16b stats_count = 0;
        u16b skills_count = 0;
        u16b ability_count = 0;
        u16b milestone_count = 0;

        if (SDL_ReadIO(file, &stats_count, sizeof(stats_count)) != sizeof(stats_count))
            goto done;
        out->stats_count = stats_count;
        if (stats_count > 0) {
            out->stats = mem_alloc_array(stats_count, score_run_stat_v1);
            if (!out->stats)
                goto done;
            size_t bytes = (size_t)stats_count * sizeof(score_run_stat_v1);
            if (SDL_ReadIO(file, out->stats, bytes) != bytes)
                goto done;
        }

        if (SDL_ReadIO(file, &skills_count, sizeof(skills_count)) != sizeof(skills_count))
            goto done;
        out->skills_count = skills_count;
        if (skills_count > 0) {
            out->skills = mem_alloc_array(skills_count, score_run_skill_v1);
            if (!out->skills)
                goto done;
            size_t bytes = (size_t)skills_count * sizeof(score_run_skill_v1);
            if (SDL_ReadIO(file, out->skills, bytes) != bytes)
                goto done;
        }

        if (SDL_ReadIO(file, &ability_count, sizeof(ability_count)) != sizeof(ability_count))
            goto done;
        out->ability_count = ability_count;
        if (ability_count > 0) {
            out->abilities = mem_alloc_array(ability_count, score_run_ability_v1);
            if (!out->abilities)
                goto done;
            size_t bytes = (size_t)ability_count * sizeof(score_run_ability_v1);
            if (SDL_ReadIO(file, out->abilities, bytes) != bytes)
                goto done;
        }

        if (SDL_ReadIO(file, &milestone_count, sizeof(milestone_count)) != sizeof(milestone_count))
            goto done;
        out->milestone_count = milestone_count;
        if (milestone_count > 0) {
            out->milestones = mem_alloc_array(milestone_count, score_run_milestone_v1);
            if (!out->milestones)
                goto done;
            size_t bytes = (size_t)milestone_count * sizeof(score_run_milestone_v1);
            if (SDL_ReadIO(file, out->milestones, bytes) != bytes)
                goto done;
        }
    }

    size_t artefact_bytes = (size_t)header.artefact_capacity
        * sizeof(score_run_artefact_v1);
    if (artefact_bytes > 0) {
        if (SDL_ReadIO(file, out->artefacts, artefact_bytes)
            != artefact_bytes)
            goto done;
    }

    size_t monster_bytes = (size_t)header.monster_capacity
        * sizeof(score_run_monster_v1);
    if (monster_bytes > 0) {
        if (SDL_ReadIO(file, out->monsters, monster_bytes)
            != monster_bytes)
            goto done;
    }

    ok = true;

done:
    if (!ok)
        score_runs_release_detail_block(out);
    SDL_CloseIO(file);
    return ok;
}

void score_runs_free_details(score_run_detail_block* details)
{
    score_runs_release_detail_block(details);
}

bool score_runs_snapshot_details(score_run_detail_block* out)
{
    if (!out)
        return false;
    u16b art_cap = score_runs_choose_artefact_capacity();
    u16b mon_cap = score_runs_choose_monster_capacity();
    return score_runs_build_details(out, art_cap, mon_cap);
}


