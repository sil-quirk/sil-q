#ifndef INCLUDED_INIT_PARSE_INTERNAL_H
#define INCLUDED_INIT_PARSE_INTERNAL_H

#include "angband.h"
#include "init.h"

#define TR1 0
#define TR2 1
#define TR3 2
#define TR4 3
#define RF1 4
#define RF2 5
#define RF3 6
#define RF4 7
#define RHF 8
#define VLT 9
#define CUR 10
#define UNQ 11
#define MAX_FLAG_SETS 12

errr parse_tile_line(const char* buf, byte* x_attr, char* x_char);
bool add_text(u32b* offset, header* head, cptr buf);
u32b add_name(header* head, cptr buf);
errr grab_one_flag(u32b** flag, cptr errstr, cptr what);

#endif /* INCLUDED_INIT_PARSE_INTERNAL_H */