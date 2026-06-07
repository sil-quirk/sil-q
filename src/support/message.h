#ifndef INCLUDED_SUPPORT_MESSAGE_H
#define INCLUDED_SUPPORT_MESSAGE_H

#include "h-basic.h"

s16b message_num(void);
cptr message_str(s16b age);
u16b message_type(s16b age);
byte message_color(s16b age);
void log_history_note_sequence(u32b sequence);
u32b message_sequence(s16b age);
u32b log_history_next_sequence(void);
void message_set_latest_sequence(u32b sequence);
errr message_color_define(u16b type, byte color);
void message_add(cptr str, u16b type);
errr messages_init(void);
void messages_free(void);

bool ui_message_line_enabled(void);
void message_line_reset_column(void);
void msg_print(cptr msg);
void msg_format(cptr fmt, ...);
void msg_debug(cptr fmt, ...);
void message(u16b message_type, s16b extra, cptr message);
void message_format(u16b message_type, s16b extra, cptr fmt, ...);
void message_flush(void);
void message_discard_pending(void);

#endif /* INCLUDED_SUPPORT_MESSAGE_H */
