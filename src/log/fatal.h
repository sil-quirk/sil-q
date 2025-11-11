/* log/fatal.h - process termination helpers */

#ifndef INCLUDED_LOG_FATAL_H
#define INCLUDED_LOG_FATAL_H

#include "../h-basic.h"

typedef void (*quit_hook_fn)(cptr message);

void log_register_quit_hook(quit_hook_fn hook);
void plog(cptr str);
void quit(cptr str);
void core(cptr str);

#endif /* INCLUDED_LOG_FATAL_H */
