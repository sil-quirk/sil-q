#ifndef INCLUDED_INIT_LIFECYCLE_H
#define INCLUDED_INIT_LIFECYCLE_H

#include "angband.h"

void display_introduction(void);
void init_angband(void);
NavResult initial_menu(bool* start_new);
void cleanup_angband(void);

#endif /* INCLUDED_INIT_LIFECYCLE_H */