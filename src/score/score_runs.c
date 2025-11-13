#include "score/score_runs.h"

#include "angband.h"
#include "externs.h"
#include "fs/path.h"
#include "log/log.h"
#include "metarun.h"
#include "player/killer.h"
#include "score/score_guid.h"

#include <SDL3/SDL.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>

#define SCORE_RUNS_DB_VERSION 0x00010000u
#define SCORE_RUNS_DB_FILENAME "runs.db"

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

    if (!score_runs_read_header(file, header)) {
        log_warn("score_runs: invalid header in %s, recreating", path);
        score_runs_init_header(header);
        if (!score_runs_write_header(file, header)) {
            SDL_CloseIO(file);
            return NULL;
        }
    }

    SDL_SeekIO(file, 0, SDL_IO_SEEK_END);
    return file;
}

static bool score_runs_find_existing(SDL_IOStream* file, u32b metarun_id,
                                     u32b character_id,
                                     score_record_v1* existing,
                                     Sint64* offset)
{
    if (!file)
        return false;

    if (SDL_SeekIO(file, sizeof(score_db_header), SDL_IO_SEEK_SET) < 0)
        return false;

    score_record_v1 temp;
    Sint64 pos = SDL_TellIO(file);
    while (SDL_ReadIO(file, &temp, sizeof(temp)) == sizeof(temp)) {
        if (temp.metarun_id == metarun_id &&
            temp.character_id == character_id &&
            temp.status == SCORE_RECORD_ALIVE) {
            if (existing)
                *existing = temp;
            if (offset)
                *offset = pos;
            SDL_SeekIO(file, 0, SDL_IO_SEEK_END);
            return true;
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
    int count = 0;
    while (SDL_ReadIO(file, &temp, sizeof(temp)) == sizeof(temp)) {
        if (temp.metarun_id == metarun_id)
            count++;
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

static u32b score_runs_hash_character(const char* normalized)
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

static s16b score_runs_house_power(void)
{
    int power = 3;
    bool gift = false;

    if (z_info && p_ptr->phouse < z_info->c_max) {
        power = c_info[p_ptr->phouse].power;
        gift = (c_info[p_ptr->phouse].flags & RHF_GIFTERU) != 0;
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
    rec->character_id = 0;
    rec->created_utc = (u32b)snapshot_time;
    rec->completed_utc = (u32b)snapshot_time;
    rec->status = status;
    rec->run_flags = score_runs_run_flags();
    rec->race_id = (byte)(p_ptr->prace & 0xFF);
    rec->house_id = (byte)(p_ptr->phouse & 0xFF);
    rec->max_depth = (u16b)MAX(p_ptr->max_depth, 0);
    rec->exit_depth = (u16b)MAX(p_ptr->depth, 0);
    rec->silmarils = score_runs_silmarils();
    rec->uniques_killed = (u16b)unique_bane_type_killed();
    rec->quests_completed = score_runs_completed_quests();
    rec->skills_learned = score_runs_skills_learned();
    rec->abilities_learned = score_runs_abilities_learned();
    rec->artefacts_found = score_runs_artefacts_found();
    rec->net_curses = score_runs_net_curses();
    rec->house_power = score_runs_house_power();
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

    char normalized[64];
    score_runs_normalize_name(op_ptr->full_name, normalized, sizeof(normalized));
    rec->character_id = score_runs_hash_character(normalized);

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

    char path[1024];
    if (!path_build(path, sizeof(path), ANGBAND_DIR_APEX, SCORE_RUNS_DB_FILENAME)) {
        log_warn("score_runs: unable to build path for runs database");
        return false;
    }

    score_record_v1 record;
    score_runs_build_record(&record, legacy_score, snapshot_time, status);

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
                                          record.character_id, &existing, &offset);

    bool success = false;
    if (found) {
        record.record_id = existing.record_id;
        record.chronological_idx = existing.chronological_idx;
        if (SDL_SeekIO(db, offset, SDL_IO_SEEK_SET) >= 0 &&
            SDL_WriteIO(db, &record, sizeof(record)) == sizeof(record)) {
            success = true;
        } else {
            log_warn("score_runs: failed to update record_id=%u", record.record_id);
        }
    } else {
        record.record_id = header.record_count;
        record.chronological_idx = (u32b)score_runs_count_for_metarun(db, record.metarun_id);
        if (SDL_WriteIO(db, &record, sizeof(record)) == sizeof(record)) {
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

    SDL_CloseIO(db);
    safe_setuid_drop();

    if (success) {
        log_info("score_runs: recorded run #%u for metarun %u (chron=%u status=%d)",
                 record.record_id, record.metarun_id, record.chronological_idx,
                 record.status);
    }

    return success;
}
