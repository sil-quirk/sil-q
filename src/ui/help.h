#ifndef INCLUDED_UI_HELP_H
#define INCLUDED_UI_HELP_H

#include "h-basic.h"

void binding_action_label(int binding, char* buf, size_t buflen);
void binding_action_short(int binding, char* buf, size_t buflen);
void do_cmd_help(void);
void do_cmd_help_menu(void);

#endif /* INCLUDED_UI_HELP_H */
