#ifndef INCLUDED_SUPPORT_QUARK_H
#define INCLUDED_SUPPORT_QUARK_H

#include "h-basic.h"

s16b quark_add(cptr str);
cptr quark_str(s16b i);
errr quarks_init(void);
errr quarks_free(void);

#endif /* INCLUDED_SUPPORT_QUARK_H */
