#include "angband.h"
#include "externs.h"
#include "log/log.h"
#include "metarun.h"
#include "pane.h"
#include "supplies.h"
#include "item_set.h"

static u32b ability_log_turn_value(void)
{
    if (playerturn < 0)
        return 0;
    return (u32b)playerturn;
}

static s16b ability_log_depth_value(void)
{
    if (!p_ptr)
        return 0;
    int depth = p_ptr->depth;
    if (depth < 0)
        depth = 0;
    if (depth > INT16_MAX)
        depth = INT16_MAX;
    return (s16b)depth;
}

static bool ability_log_has_entry(int skilltype, int abilitynum)
{
    if (!p_ptr)
        return false;

    u16b count = p_ptr->ability_timeline_count;
    if (count > ABILITY_TIMELINE_MAX)
        count = ABILITY_TIMELINE_MAX;

    for (u16b i = 0; i < count; i++) {
        if (p_ptr->ability_timeline_skill[i] == skilltype
            && p_ptr->ability_timeline_ability[i] == abilitynum)
            return true;
    }
    return false;
}

static void ability_log_append(int skilltype, int abilitynum,
                               u32b turn_value, s16b depth_value)
{
    if (!p_ptr)
        return;
    if (skilltype < 0 || skilltype >= S_MAX
        || abilitynum < 0 || abilitynum >= ABILITIES_MAX)
        return;
    if (ability_log_has_entry(skilltype, abilitynum))
        return;

    u16b count = p_ptr->ability_timeline_count;
    if (count >= ABILITY_TIMELINE_MAX)
        return;

    p_ptr->ability_timeline_skill[count] = (byte)skilltype;
    p_ptr->ability_timeline_ability[count] = (byte)abilitynum;
    p_ptr->ability_timeline_turn[count] = turn_value;
    p_ptr->ability_timeline_depth[count] = depth_value;
    p_ptr->ability_timeline_count = count + 1;
}

void ability_log_reset(void)
{
    if (!p_ptr)
        return;
    p_ptr->ability_timeline_count = 0;
    memset(p_ptr->ability_timeline_skill, 0,
        sizeof(p_ptr->ability_timeline_skill));
    memset(p_ptr->ability_timeline_ability, 0,
        sizeof(p_ptr->ability_timeline_ability));
    memset(p_ptr->ability_timeline_turn, 0,
        sizeof(p_ptr->ability_timeline_turn));
    memset(p_ptr->ability_timeline_depth, 0,
        sizeof(p_ptr->ability_timeline_depth));
}

void ability_log_record_gain(int skilltype, int abilitynum)
{
    ability_log_append(skilltype, abilitynum,
        ability_log_turn_value(), ability_log_depth_value());
}

void ability_log_sync_missing(void)
{
    if (!p_ptr)
        return;

    s16b depth = ability_log_depth_value();
    for (int skill = 0; skill < S_MAX; skill++) {
        for (int abil = 0; abil < ABILITIES_MAX; abil++) {
            if (!p_ptr->innate_ability[skill][abil])
                continue;
            ability_log_append(skill, abil, 0, depth);
        }
    }
}
