#ifndef INCLUDED_OBJECT_SETTINGS_H
#define INCLUDED_OBJECT_SETTINGS_H

#include "h-basic.h"

/*
 * Load the line-oriented squelch and autoinscription formats used by the
 * object settings UI.  These files are intentionally narrower than the
 * removed general-purpose preference-file format.
 */
errr object_settings_load(cptr name);

#endif /* INCLUDED_OBJECT_SETTINGS_H */
