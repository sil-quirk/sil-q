/* File: format.c */

/*
 * Modern string formatting layer backed by z-form's vstrnfmt
 * This module provides the main formatting functions while z-form.c
 * handles the low-level vstrnfmt implementation with custom format sequences.
 */

#include "format.h"
#include "z-form.h"
#include <string.h>

/*
 * Note: vstrnfmt and strnfcat are still implemented in z-form.c
 * because they support custom format sequences like "%^" for capitalization
 * that are used throughout the codebase.
 */

/*
 * Format into a static buffer (for compatibility with old code)
 * 
 * WARNING: This uses a static buffer and is NOT thread-safe.
 * The buffer will be overwritten on the next call to this function.
 */
char* format(cptr fmt, ...)
{
    static char buf[2048];  /* Large enough for most uses */
    va_list vp;
    
    va_start(vp, fmt);
    vstrnfmt(buf, sizeof(buf), fmt, vp);
    va_end(vp);
    
    return buf;
}
