/* log/bootstrap.h - Logger initialization */

#ifndef INCLUDED_LOG_BOOTSTRAP_H
#define INCLUDED_LOG_BOOTSTRAP_H

#include "../h-basic.h"

/*
 * Initialises logger. Opens `log.txt` file and sets log level for stdout and
 * file from `SIL_LOG_LEVEL` environment variable. The `quiet` argument disables
 * stdout when set to true (essential for terminal modes like ncurses where
 * screen output would be garbled otherwise).
 */
extern void init_logger(bool quiet, const char* exe_path);

#endif /* INCLUDED_LOG_BOOTSTRAP_H */
