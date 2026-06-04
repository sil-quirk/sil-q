#ifndef INCLUDED_SUPPORT_MACRO_H
#define INCLUDED_SUPPORT_MACRO_H

#include "h-basic.h"

void text_to_ascii(char* buf, size_t len, cptr str);
void ascii_to_text(char* buf, size_t len, cptr str);
int macro_find_exact(cptr pat);
int macro_find_check(cptr pat);
int macro_find_maybe(cptr pat);
int macro_find_ready(cptr pat);
errr macro_add(cptr pat, cptr act);
errr macro_init(void);
errr macro_free(void);
errr macro_trigger_free(void);

#endif /* INCLUDED_SUPPORT_MACRO_H */
