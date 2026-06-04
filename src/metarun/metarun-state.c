#include "angband.h"
#include "metarun-internal.h"

/* =========================  globals  =========================== */
metarun *metaruns    = NULL;
s16b     metarun_max = 0;
s16b     current_run = 0;
bool            metarun_created = false;

/* ----------------------- accessors --------------------------- */
const metarun *metarun_current(void)
{
    if (!metaruns) return NULL;
    if (current_run < 0 || current_run >= metarun_max) return NULL;
    return &metaruns[current_run];
}

metarun *metarun_current_mutable(void)
{
    if (!metaruns) return NULL;
    if (current_run < 0 || current_run >= metarun_max) return NULL;
    return &metaruns[current_run];
}

const metarun *metarun_entry_const(s16b idx)
{
    if (!metaruns) return NULL;
    if (idx < 0 || idx >= metarun_max) return NULL;
    return &metaruns[idx];
}

metarun *metarun_entry_mutable(s16b idx)
{
    if (!metaruns) return NULL;
    if (idx < 0 || idx >= metarun_max) return NULL;
    return &metaruns[idx];
}

s16b metarun_current_index(void)
{
    if (!metaruns) return -1;
    if (current_run < 0 || current_run >= metarun_max) return -1;
    return current_run;
}

s16b metarun_entry_count(void)
{
    return metarun_max;
}

int metarun_completed_count(void)
{
    int completed = 0;

    if (!metaruns)
        return 0;

    for (s16b i = 0; i < metarun_max; i++) {
        const metarun *m = &metaruns[i];
        int win_goal = WINCON_SILMARILS;

        if (runtype_info && z_info && m->type < z_info->rt_max) {
            win_goal = runtype_info[m->type].win_con
                ? runtype_info[m->type].win_con
                : WINCON_SILMARILS;
        }

        if (m->silmarils >= win_goal)
            completed++;
    }

    return completed;
}

bool sync_current_metarun_slot(bool stamp_time)
{
    if (!metaruns || current_run < 0 || current_run >= metarun_max) {
        return false;
    }

    if (stamp_time) {
        metar.last_played = (u32b)time(NULL);
    }

    metarun_clamp_and_sync_quests(&metar);
    metaruns[current_run] = metar;
    return true;
}

void metarun_increment_deaths(void)
{
    /* Clamp to byte range; defer saving/UI to caller */
    if (metar.deaths >= 255) return;

    metar.deaths++;

    if (!sync_current_metarun_slot(false)) {
        log_warn("metarun_increment_deaths: unable to sync current slot (idx=%d, max=%d)",
                 current_run, metarun_max);
    }
    refresh_current_metar_score();
}

void metarun_gain_silmarils(byte n)
{
    if (!n) return;
    int total = (int)metar.silmarils + (int)n;
    if (total > 255) total = 255;
    if (total < 0) total = 0;
    metar.silmarils = (byte)total;
    refresh_current_metar_score();

    if (!sync_current_metarun_slot(false)) {
        log_warn("metarun_gain_silmarils: unable to sync current slot (idx=%d, max=%d)",
                 current_run, metarun_max);
    }
}
