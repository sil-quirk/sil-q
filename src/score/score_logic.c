#include "angband.h"
#include "log/log.h"
#include "externs.h"

#include <ctype.h>
#include <limits.h>
#include <string.h>

#include "score/score_io.h"
#include "score/score_logic.h"

/* --------------------------------------------------------------------- */
/* Internal helpers                                                      */
/* --------------------------------------------------------------------- */

typedef struct score_breakdown {
    int base_score;
    int mult_bp;
    int silmarils;
    int max_depth;
    int cur_depth;
    int depth_up;
    int curses;
    int house_power;
    int uniques_killed;
    bool escaped;
    bool morgoth_slain;
} score_breakdown;

static int clampi(int value, int minimum, int maximum)
{
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

static score_breakdown calculate_score_breakdown(const high_score* score)
{
    score_breakdown result = {0};

    int raw_max_depth = parse_score_int(score->max_dun, sizeof(score->max_dun), 0);
    int raw_cur_depth = parse_score_int(score->cur_dun, sizeof(score->cur_dun), 0);
    int silmarils = parse_score_int(score->silmarils, sizeof(score->silmarils), 0);
    int curses = 0;

    if (scores_version_has_curses(score_file_global_ctx())) {
        curses = parse_score_int(score->pts, sizeof(score->pts), 0);
        log_trace("calculate_score_breakdown: '%s' pts field='%.*s' parsed as curses=%d (version %d.%d.%d.%d)",
                  score->who, (int)sizeof(score->pts), score->pts, curses,
                  score_file_global_ctx()->version_major,
                  score_file_global_ctx()->version_minor,
                  score_file_global_ctx()->version_patch,
                  score_file_global_ctx()->version_extra);
    } else {
        log_trace("calculate_score_breakdown: '%s' pts field ignored (old version %d.%d.%d.%d)",
                  score->who,
                  score_file_global_ctx()->version_major,
                  score_file_global_ctx()->version_minor,
                  score_file_global_ctx()->version_patch,
                  score_file_global_ctx()->version_extra);
    }

    int uniques_killed = parse_score_int(score->cur_lev, sizeof(score->cur_lev), 0);
    bool morgoth = (score->morgoth_slain[0] == 't');
    bool escaped = (score->escaped[0] == 't');

    if (silmarils < 0)
        silmarils = 0;
    curses = clampi(curses, -1000, 1000);
    uniques_killed = clampi(uniques_killed, 0, 999);

    if (morgoth && silmarils < 3)
        silmarils = 3;

    int depth_down = clampi(raw_max_depth, 0, MORGOTH_DEPTH);
    int depth_up = clampi(20 - raw_cur_depth, 0, MORGOTH_DEPTH);
    if (morgoth)
        depth_up = MORGOTH_DEPTH;

    int base = 10 * depth_down;
    base += 3 * uniques_killed;

    if (silmarils > 0) {
        base += 5 * depth_up;
        base += 100;
        if (silmarils > 1)
            base += 50;
        if (silmarils > 2)
            base += 50;
    }

    if (morgoth)
        base += 300;

    if (escaped)
        base += 100;

    int house_index = parse_score_int(score->p_h, sizeof(score->p_h), -1);
    int house_power = 3;
    bool gift_of_eru = false;
    int race_index = parse_score_int(score->p_r, sizeof(score->p_r), -1);

    if (race_index >= 0 && z_info && p_info && race_index < z_info->p_max) {
        if (p_info[race_index].flags & RHF_GIFTERU)
            gift_of_eru = true;
    }

    if (house_index >= 0 && z_info && c_info && house_index < z_info->c_max) {
        house_power = c_info[house_index].power;
        if (c_info[house_index].flags & RHF_GIFTERU)
            gift_of_eru = true;
        log_trace("calculate_score_breakdown: house_index=%d, house_power=%d (from c_info)",
                  house_index, house_power);
    } else {
        log_trace("calculate_score_breakdown: Using default house_power=3 (house_index=%d, z_info=%p, c_info=%p, z_info->c_max=%d)",
                  house_index, (void*)z_info, (void*)c_info, z_info ? z_info->c_max : -1);
    }

    house_power = clampi(house_power, -100, 100);
    if (gift_of_eru && house_power > 0) {
        house_power--;
    }

    int mult_bp = 1000;
    int house_diff = 3 - house_power;
    if (house_diff >= 0)
        mult_bp += house_diff * 220;
    else
        mult_bp += house_diff * 100;

    if (curses >= 0)
        mult_bp += curses * 55;
    else
        mult_bp += curses * 20;

    if (mult_bp < 0)
        mult_bp = 0;

    result.base_score = base;
    result.mult_bp = mult_bp;
    result.silmarils = silmarils;
    result.max_depth = depth_down;
    result.cur_depth = clampi(raw_cur_depth, 0, MORGOTH_DEPTH);
    result.depth_up = depth_up;
    result.curses = curses;
    result.house_power = house_power;
    result.uniques_killed = uniques_killed;
    result.escaped = escaped;
    result.morgoth_slain = morgoth;

    return result;
}

static int score_points_from_breakdown(const score_breakdown* breakdown)
{
    if (!breakdown)
        return 0;

    long total = (long)breakdown->base_score * (long)breakdown->mult_bp;
    total = (total + 500) / 1000;

    if (total > INT_MAX)
        total = INT_MAX;
    else if (total < INT_MIN)
        total = INT_MIN;

    return (int)total;
}

/* --------------------------------------------------------------------- */
/* Public API                                                            */
/* --------------------------------------------------------------------- */

int parse_score_int(const char* field, size_t field_len, int fallback)
{
    if (!field)
        return fallback;

    char buffer[16];
    size_t copy_len = field_len;
    if (copy_len >= sizeof(buffer))
        copy_len = sizeof(buffer) - 1;

    memcpy(buffer, field, copy_len);
    buffer[copy_len] = '\0';

    char* start = buffer;
    while (*start && isspace((unsigned char)*start))
        start++;

    if (*start == '\0')
        return fallback;

    char* end = NULL;
    long value = strtol(start, &end, 10);
    if (start == end)
        return fallback;

    if (value > INT_MAX)
        return INT_MAX;
    if (value < INT_MIN)
        return INT_MIN;

    return (int)value;
}

void parse_score_string(const char *field, size_t field_len,
                        char *out, size_t out_len)
{
    if (!out || out_len == 0) {
        return;
    }

    if (!field) {
        out[0] = '\0';
        return;
    }

    size_t copy_len = field_len;
    if (copy_len >= out_len)
        copy_len = out_len - 1;

    memcpy(out, field, copy_len);
    out[copy_len] = '\0';

    while (copy_len > 0 &&
           (out[copy_len - 1] == '\0' || out[copy_len - 1] == ' ')) {
        out[--copy_len] = '\0';
    }

    size_t start = 0;
    while (out[start] == ' ')
        start++;

    if (start > 0) {
        memmove(out, out + start, copy_len - start + 1);
    }
}

int score_points(const high_score* score)
{
    if (!score)
        return 0;

    score_breakdown breakdown = calculate_score_breakdown(score);
    int total = score_points_from_breakdown(&breakdown);

    const char* who = (score->who[0] != '\0') ? score->who : "<unknown>";
    log_debug(
        "score_points: '%s' base=%d mult=%d (power=%d curses=%d sil=%d depth_down=%d depth_up=%d uniques=%d escaped=%s morgoth=%s) => %d",
        who, breakdown.base_score, breakdown.mult_bp, breakdown.house_power,
        breakdown.curses, breakdown.silmarils, breakdown.max_depth,
        breakdown.depth_up, breakdown.uniques_killed, breakdown.escaped ? "yes" : "no",
        breakdown.morgoth_slain ? "yes" : "no", total);

    return total;
}

int score_compare(const high_score* a, const high_score* b)
{
    if (!a || !b)
        return 0;

    score_breakdown breakdown_a = calculate_score_breakdown(a);
    score_breakdown breakdown_b = calculate_score_breakdown(b);

    int score_a = score_points_from_breakdown(&breakdown_a);
    int score_b = score_points_from_breakdown(&breakdown_b);

    if (score_a > score_b)
        return -1;
    if (score_a < score_b)
        return 1;

    if (breakdown_a.escaped != breakdown_b.escaped)
        return breakdown_a.escaped ? -1 : 1;

    bool a_has_sil = (breakdown_a.silmarils > 0);
    bool b_has_sil = (breakdown_b.silmarils > 0);
    if (a_has_sil != b_has_sil)
        return a_has_sil ? -1 : 1;

    if (breakdown_a.morgoth_slain != breakdown_b.morgoth_slain)
        return breakdown_a.morgoth_slain ? -1 : 1;

    if (breakdown_a.silmarils != breakdown_b.silmarils)
        return (breakdown_a.silmarils > breakdown_b.silmarils) ? -1 : 1;

    if (breakdown_a.max_depth != breakdown_b.max_depth)
        return (breakdown_a.max_depth > breakdown_b.max_depth) ? -1 : 1;

    if (breakdown_a.mult_bp != breakdown_b.mult_bp)
        return (breakdown_a.mult_bp > breakdown_b.mult_bp) ? -1 : 1;

    return 0;
}
