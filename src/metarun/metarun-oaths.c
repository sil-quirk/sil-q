#include "angband.h"
#include "metarun-internal.h"

bool oath_unlocked(int oath_id)
{
    if (run_mode_is_blitz()) return false;
    if (current_run < 0 || current_run >= metarun_max) return false;
    if (oath_id < 1 || !z_info || oath_id >= z_info->oath_max) return false;

    byte oath_bit = (1 << (oath_id - 1)); /* Convert 1-5 to bits 1,2,4,8,16 */
    return (metaruns[current_run].unlocked_oaths & oath_bit) != 0;
}

/*
 * Check if an oath is banned in the current metarun
 */
bool oath_banned(int oath_id)
{
    if (run_mode_is_blitz()) return false;
    if (current_run < 0 || current_run >= metarun_max) return false;
    if (oath_id < 1 || !z_info || oath_id >= z_info->oath_max) return false;

    byte oath_bit = (1 << (oath_id - 1)); /* Convert 1-5 to bits 1,2,4,8,16 */
    return (metaruns[current_run].banned_oaths & oath_bit) != 0;
}

/*
 * Unlock an oath in the current metarun
 */
void metarun_unlock_oath(int oath_id)
{
    if (run_mode_is_blitz()) return;
    if (current_run < 0 || current_run >= metarun_max) {
        log_trace("Oath unlock: Invalid current_run=%d, metarun_max=%d", current_run, metarun_max);
        return;
    }
    if (oath_id < 1 || !z_info || oath_id >= z_info->oath_max) {
        log_trace("Oath unlock: Invalid oath_id=%d", oath_id);
        return;
    }

    byte oath_bit = (1 << (oath_id - 1)); /* Convert 1-based oath_id to bitmask */

    /* Update both the global metar and the metaruns array */
    metar.unlocked_oaths |= oath_bit;
    metaruns[current_run].unlocked_oaths |= oath_bit;

    log_trace("Oath unlock: Unlocked oath %d (bit %d) in metarun[%d], unlocked_oaths=0x%02X",
              oath_id, oath_bit, current_run, metaruns[current_run].unlocked_oaths);

    /* Save immediately to persist the change */
    save_metaruns();
}

/*
 * Ban an oath in the current metarun (when broken)
 */
void metarun_ban_oath(int oath_id)
{
    if (run_mode_is_blitz()) return;
    if (current_run < 0 || current_run >= metarun_max) {
        log_trace("Oath ban: Invalid current_run=%d, metarun_max=%d", current_run, metarun_max);
        return;
    }
    if (oath_id < 1 || !z_info || oath_id >= z_info->oath_max) {
        log_trace("Oath ban: Invalid oath_id=%d", oath_id);
        return;
    }

    byte oath_bit = (1 << (oath_id - 1)); /* Convert 1-based oath_id to bitmask */

    /* Update both the global metar and the metaruns array */
    metar.banned_oaths |= oath_bit;
    metaruns[current_run].banned_oaths |= oath_bit;

    log_trace("Oath ban: Banned oath %d (bit %d) in metarun[%d], banned_oaths=0x%02X",
              oath_id, oath_bit, current_run, metaruns[current_run].banned_oaths);

    /* Save immediately to persist the change */
    refresh_current_metar_score();
    save_metaruns();
}

/*
 * Get bitmask of oaths available for selection (unlocked but not banned)
 */
int get_available_oaths_mask(void)
{
    if (run_mode_is_blitz()) {
        int available = 0;
        int max_oath_id;

        if (!blitz_oaths_enabled())
            return 0;
        if (!z_info)
            return 0;
        if (z_info->oath_max <= 1)
            return 0;

        max_oath_id = MIN(OATH_LIGHT, z_info->oath_max - 1);

        for (int i = 1; i <= max_oath_id; i++)
            available |= (1 << (i - 1));

        return available;
    }

    if (current_run < 0 || current_run >= metarun_max) return 0;

    byte unlocked = metaruns[current_run].unlocked_oaths;
    byte banned = metaruns[current_run].banned_oaths;
    byte available = unlocked & ~banned;

    log_trace("Oath availability: unlocked=0x%02X, banned=0x%02X, available=0x%02X",
              unlocked, banned, available);

    return available;
}
