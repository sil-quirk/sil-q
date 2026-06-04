#ifndef INCLUDED_PLATFORM_SIGNALS_H
#define INCLUDED_PLATFORM_SIGNALS_H

#include "h-basic.h"

#ifdef HANDLE_SIGNALS
extern void (*(*signal_aux)(int, void (*)(int)))(int);
#endif
void signals_ignore_tstp(void);
void signals_handle_tstp(void);
void signals_init(void);

#endif /* INCLUDED_PLATFORM_SIGNALS_H */