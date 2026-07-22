#ifndef INCLUDED_SCORE_LOGIC_H
#define INCLUDED_SCORE_LOGIC_H

#include "h-basic.h"

struct high_score;
typedef struct high_score high_score;

typedef struct score_breakdown {
    int base_score;
    int mult_bp;
    int total_score;
    int descent_points;
    int unique_points;
    int ascent_points;
    int silmaril_points;
    int morgoth_points;
    int escape_points;
    int character_mult_bp;
    int curse_mult_bp;
    int silmarils;
    int max_depth;
    int cur_depth;
    int depth_up;
    int curses;
    int character_power;
    int uniques_killed;
    bool escaped;
    bool morgoth_slain;
} score_breakdown;

int parse_score_int(const char* field, size_t field_len, int fallback);
void parse_score_string(const char* field, size_t field_len,
                        char* out, size_t out_len);

score_breakdown score_calculate_breakdown(const high_score* score);
int score_points(const high_score* score);
int score_compare(const high_score* a, const high_score* b);

#endif /* INCLUDED_SCORE_LOGIC_H */
