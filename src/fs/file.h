#ifndef INCLUDED_FS_FILE_H
#define INCLUDED_FS_FILE_H

#include "h-basic.h"

void safe_setuid_drop(void);
void safe_setuid_grab(void);
s16b tokenize(char* buf, s16b num, char** tokens);
errr check_time(void);
errr check_time_init(void);

#endif /* INCLUDED_FS_FILE_H */