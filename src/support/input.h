#ifndef INCLUDED_SUPPORT_INPUT_H
#define INCLUDED_SUPPORT_INPUT_H

#include "h-basic.h"

void flush(void);
void flush_fail(void);
char inkey(void);
bool inkey_next_active(void);
void inkey_next_set(cptr keys);
#ifdef ALLOW_REPEAT
void repeat_push(int what);
bool repeat_pull(int* what);
void repeat_clear(void);
void repeat_check(void);
#endif

#endif /* INCLUDED_SUPPORT_INPUT_H */
