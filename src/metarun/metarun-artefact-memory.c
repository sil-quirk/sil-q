#include "angband.h"
#include "metarun-internal.h"

#define METARUN_ARTEFACT_MEMORY_FILENAME "artefact_memory.db"
#define METARUN_ARTEFACT_MEMORY_MAGIC "AMEM"
#define METARUN_ARTEFACT_MEMORY_VERSION 0x00010000u
#define METARUN_ARTEFACT_MEMORY_EASY_ID 0x01
#define METARUN_ARTEFACT_MEMORY_REVEALED 0x02

typedef struct metarun_artefact_memory_header {
    char magic[4];
    u32b version;
    u32b record_count;
    u32b reserved;
} metarun_artefact_memory_header;

typedef struct metarun_artefact_memory_record {
    u32b metarun_id;
    guid64 guid;
    u16b a_idx;
    byte flags;
    byte reserved;
} metarun_artefact_memory_record;

static bool metarun_guid_is_zero(const guid64 *guid)
{
    return !guid || (guid->hi == 0 && guid->lo == 0);
}

static bool metarun_guid_equal(const guid64 *a, const guid64 *b)
{
    if (!a || !b)
        return false;
    return (a->hi == b->hi) && (a->lo == b->lo);
}

static bool metarun_artefact_valid_index(int a_idx)
{
    return z_info && a_info && a_idx > 0 && a_idx < z_info->art_max;
}

static int metarun_artefact_lookup_guid(guid64 guid)
{
    if (!z_info || !a_info || metarun_guid_is_zero(&guid))
        return 0;

    for (int i = 1; i < z_info->art_max; i++) {
        artefact_type *a_ptr = &a_info[i];
        if ((a_ptr->tval + a_ptr->sval) == 0)
            continue;
        if (metarun_guid_equal(&a_ptr->guid, &guid))
            return i;
    }

    return 0;
}

static int metarun_artefact_memory_resolve(
    const metarun_artefact_memory_record *record)
{
    if (!record)
        return 0;

    if (!metarun_guid_is_zero(&record->guid)) {
        int by_guid = metarun_artefact_lookup_guid(record->guid);
        return by_guid;
    }

    if (metarun_artefact_valid_index(record->a_idx))
        return record->a_idx;

    return 0;
}

static void metarun_artefact_memory_init_header(
    metarun_artefact_memory_header *header)
{
    memset(header, 0, sizeof(*header));
    memcpy(header->magic, METARUN_ARTEFACT_MEMORY_MAGIC, sizeof(header->magic));
    header->version = METARUN_ARTEFACT_MEMORY_VERSION;
}

static bool metarun_artefact_memory_write_header(SDL_IOStream *file,
    const metarun_artefact_memory_header *header)
{
    if (!file || !header)
        return false;

    if (SDL_SeekIO(file, 0, SDL_IO_SEEK_SET) < 0)
        return false;
    if (SDL_WriteIO(file, header, sizeof(*header)) != sizeof(*header))
        return false;
    SDL_SeekIO(file, 0, SDL_IO_SEEK_END);
    return true;
}

static SDL_IOStream *metarun_artefact_memory_open(
    metarun_artefact_memory_header *header, bool create)
{
    char path[1024];
    SDL_IOStream *file;
    bool created = false;

    if (!header)
        return NULL;
    if (!build_meta_path(path, sizeof(path), NULL,
            METARUN_ARTEFACT_MEMORY_FILENAME))
    {
        log_warn("metarun artefact memory: unable to build database path");
        return NULL;
    }

    safe_setuid_grab();
    file = SDL_IOFromFile(path, create ? "r+b" : "rb");
    if (!file && create) {
        file = SDL_IOFromFile(path, "w+b");
        created = file ? true : false;
    }
    safe_setuid_drop();

    if (!file)
        return NULL;

    if (created) {
        metarun_artefact_memory_init_header(header);
        if (!metarun_artefact_memory_write_header(file, header)) {
            SDL_CloseIO(file);
            return NULL;
        }
        return file;
    }

    if (SDL_ReadIO(file, header, sizeof(*header)) == sizeof(*header) &&
        memcmp(header->magic, METARUN_ARTEFACT_MEMORY_MAGIC,
            sizeof(header->magic)) == 0 &&
        header->version == METARUN_ARTEFACT_MEMORY_VERSION)
    {
        SDL_SeekIO(file, 0, SDL_IO_SEEK_END);
        return file;
    }

    if (!create) {
        SDL_CloseIO(file);
        return NULL;
    }

    SDL_CloseIO(file);
    log_warn("metarun artefact memory: resetting invalid database %s", path);

    safe_setuid_grab();
    file = SDL_IOFromFile(path, "w+b");
    safe_setuid_drop();
    if (!file)
        return NULL;

    metarun_artefact_memory_init_header(header);
    if (!metarun_artefact_memory_write_header(file, header)) {
        SDL_CloseIO(file);
        return NULL;
    }

    return file;
}

static bool metarun_artefact_memory_record_matches(
    const metarun_artefact_memory_record *record, u32b metarun_id,
    int a_idx, guid64 guid)
{
    if (!record || record->metarun_id != metarun_id)
        return false;

    if (!metarun_guid_is_zero(&guid) && !metarun_guid_is_zero(&record->guid))
        return metarun_guid_equal(&record->guid, &guid);

    return record->a_idx == a_idx;
}

static bool metarun_artefact_memory_exists(void)
{
    metarun_artefact_memory_header header;
    SDL_IOStream *file = metarun_artefact_memory_open(&header, false);
    if (!file)
        return false;

    SDL_CloseIO(file);
    return true;
}

static void metarun_artefact_memory_apply_flags(int a_idx, byte flags)
{
    if (!metarun_artefact_valid_index(a_idx))
        return;

    artefact_type *a_ptr = &a_info[a_idx];
    if ((flags & METARUN_ARTEFACT_MEMORY_REVEALED) != 0)
        a_ptr->seen |= ART_SEEN_REVEALED;

    if ((flags & METARUN_ARTEFACT_MEMORY_EASY_ID) != 0 &&
        (a_ptr->flags3 & TR3_EASY_ID))
    {
        a_ptr->seen |= ART_SEEN_REVEALED | ART_SEEN_METARUN_EASY_ID;
    }
}

static bool metarun_record_artefact_memory(int a_idx, byte flags)
{
    if (run_mode_is_blitz())
        return false;
    if (!metarun_artefact_valid_index(a_idx) || flags == 0)
        return false;

    artefact_type *a_ptr = &a_info[a_idx];
    if ((a_ptr->tval + a_ptr->sval) == 0)
        return false;

    if ((flags & METARUN_ARTEFACT_MEMORY_EASY_ID) != 0 &&
        !(a_ptr->flags3 & TR3_EASY_ID))
    {
        flags &= ~METARUN_ARTEFACT_MEMORY_EASY_ID;
    }
    if (flags == 0)
        return false;

    u32b metarun_id = metar.id;
    guid64 guid = a_ptr->guid;
    metarun_artefact_memory_apply_flags(a_idx, flags);

    metarun_artefact_memory_header header;
    SDL_IOStream *file = metarun_artefact_memory_open(&header, true);
    if (!file) {
        log_warn("metarun artefact memory: unable to open database for write");
        return false;
    }

    bool found = false;
    bool ok = false;
    Sint64 found_offset = -1;
    metarun_artefact_memory_record record;

    if (SDL_SeekIO(file, sizeof(header), SDL_IO_SEEK_SET) >= 0) {
        for (u32b i = 0; i < header.record_count; i++) {
            Sint64 offset = SDL_TellIO(file);
            if (SDL_ReadIO(file, &record, sizeof(record)) != sizeof(record))
                break;

            if (metarun_artefact_memory_record_matches(&record, metarun_id,
                    a_idx, guid))
            {
                found = true;
                found_offset = offset;
                break;
            }
        }
    }

    if (found) {
        record.a_idx = (u16b)a_idx;
        record.guid = guid;
        record.flags |= flags;
        if (SDL_SeekIO(file, found_offset, SDL_IO_SEEK_SET) >= 0 &&
            SDL_WriteIO(file, &record, sizeof(record)) == sizeof(record))
        {
            ok = true;
        }
    } else {
        memset(&record, 0, sizeof(record));
        record.metarun_id = metarun_id;
        record.guid = guid;
        record.a_idx = (u16b)a_idx;
        record.flags = flags;

        if (SDL_SeekIO(file, 0, SDL_IO_SEEK_END) >= 0 &&
            SDL_WriteIO(file, &record, sizeof(record)) == sizeof(record))
        {
            header.record_count++;
            ok = metarun_artefact_memory_write_header(file, &header);
        }
    }

    SDL_CloseIO(file);
    return ok;
}

void metarun_apply_artefact_memory(void)
{
    if (run_mode_is_blitz())
        return;
    if (!z_info || !a_info)
        return;

    metarun_artefact_memory_header header;
    SDL_IOStream *file = metarun_artefact_memory_open(&header, false);
    if (!file)
        return;

    if (SDL_SeekIO(file, sizeof(header), SDL_IO_SEEK_SET) < 0) {
        SDL_CloseIO(file);
        return;
    }

    int applied = 0;
    for (u32b i = 0; i < header.record_count; i++) {
        metarun_artefact_memory_record record;
        if (SDL_ReadIO(file, &record, sizeof(record)) != sizeof(record))
            break;
        if (record.metarun_id != metar.id)
            continue;

        int a_idx = metarun_artefact_memory_resolve(&record);
        if (!a_idx)
            continue;

        metarun_artefact_memory_apply_flags(a_idx, record.flags);
        applied++;
    }

    SDL_CloseIO(file);
    if (applied > 0)
        log_debug("metarun artefact memory: applied %d remembered records", applied);
}

void metarun_seed_artefact_memory_from_current_state_if_missing(void)
{
    if (run_mode_is_blitz())
        return;
    if (!z_info || !a_info)
        return;
    if (metarun_artefact_memory_exists())
        return;

    int seeded = 0;
    for (int i = 1; i < z_info->art_max; i++) {
        artefact_type *a_ptr = &a_info[i];
        byte flags = 0;

        if ((a_ptr->tval + a_ptr->sval) == 0)
            continue;

        if ((a_ptr->flags3 & TR3_EASY_ID) &&
            (a_ptr->found_num > 0 ||
                (a_ptr->seen & ART_SEEN_METARUN_EASY_ID)))
        {
            flags |= METARUN_ARTEFACT_MEMORY_EASY_ID;
        }

        if (a_ptr->seen & ART_SEEN_REVEALED)
            flags |= METARUN_ARTEFACT_MEMORY_REVEALED;

        if (flags && metarun_record_artefact_memory(i, flags))
            seeded++;
    }

    if (seeded > 0) {
        log_info("metarun artefact memory: seeded %d records from current save state",
            seeded);
    }
}

bool metarun_record_artefact_identification(int a_idx)
{
    return metarun_record_artefact_memory(a_idx,
        METARUN_ARTEFACT_MEMORY_EASY_ID);
}

bool metarun_record_artefact_revealed(int a_idx)
{
    return metarun_record_artefact_memory(a_idx,
        METARUN_ARTEFACT_MEMORY_REVEALED);
}

bool metarun_try_identify_remembered_artefact(object_type *o_ptr)
{
    if (!o_ptr || !o_ptr->k_idx || !o_ptr->name1)
        return false;
    if (object_known_p(o_ptr))
        return false;
    if (!metarun_artefact_valid_index(o_ptr->name1))
        return false;

    artefact_type *a_ptr = &a_info[o_ptr->name1];
    if (!(a_ptr->flags3 & TR3_EASY_ID))
        return false;
    if (!(a_ptr->seen & ART_SEEN_METARUN_EASY_ID))
        return false;

    ident(o_ptr);
    return true;
}
