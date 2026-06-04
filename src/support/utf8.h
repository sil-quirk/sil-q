#ifndef INCLUDED_SUPPORT_UTF8_H
#define INCLUDED_SUPPORT_UTF8_H

#include "h-basic.h"

int utf8_sequence_len_n(cptr str, int len);
int utf8_sequence_len(cptr str);
bool utf8_has_non_ascii(cptr str);
bool utf8_has_non_ascii_n(cptr str, int len);
int utf8_display_width_n(cptr str, int len);
int utf8_safe_prefix_len(cptr str, int len);

#endif /* INCLUDED_SUPPORT_UTF8_H */
