#ifndef INCLUDED_SCORE_LOGIC_H
#define INCLUDED_SCORE_LOGIC_H

#include "h-basic.h"

struct high_score;
typedef struct high_score high_score;

int parse_score_int(const char* field, size_t field_len, int fallback);
void parse_score_string(const char* field, size_t field_len,
                        char* out, size_t out_len);

int score_points(const high_score* score);
int score_compare(const high_score* a, const high_score* b);

#endif /* INCLUDED_SCORE_LOGIC_H */
