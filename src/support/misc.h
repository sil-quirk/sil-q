#ifndef INCLUDED_SUPPORT_MISC_H
#define INCLUDED_SUPPORT_MISC_H

#include "h-basic.h"

bool no_light(void);
bool parse_u64b_hex(const char* text, u64b* out);
#ifdef SET_UID
void user_name(char* buf, size_t len, int id);
#endif
#ifdef CHECK_MODIFICATION_TIME
errr check_modification_date_sdl(cptr raw_path, cptr txt_path);
#endif
int int_exp(int base, int power);
int damroll(int num, int sides);
bool is_a_vowel(int ch);

#endif /* INCLUDED_SUPPORT_MISC_H */
