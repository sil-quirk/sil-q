#ifndef INCLUDED_SUPPORT_FEEDBACK_H
#define INCLUDED_SUPPORT_FEEDBACK_H

#include "h-basic.h"

void bell(cptr reason);
void sound(int val);
void sound_delayed(int val, unsigned int delay_ms);

#endif /* INCLUDED_SUPPORT_FEEDBACK_H */
