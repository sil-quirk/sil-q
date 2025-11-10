/* File: format.h */

/*
 * Modern string formatting layer
 * 
 * Provides the custom vstrnfmt()/strnfmt()/strnfcat() helpers used
 * throughout the codebase (supports extensions like "%^" for capitalization).
 */

#ifndef INCLUDED_FORMAT_H
#define INCLUDED_FORMAT_H

#include "h-basic.h"
#include <stdarg.h>

/*
 * Format arguments into given bounded-length buffer
 * Returns: number of characters written (excluding null terminator)
 */
extern size_t vstrnfmt(char* buf, size_t max, cptr fmt, va_list vp);

/*
 * Simple vararg interface to vstrnfmt()
 * Returns: number of characters written (excluding null terminator)
 */
extern size_t strnfmt(char* buf, size_t max, cptr fmt, ...);

/*
 * Append a formatted string to another string
 * 'end' is updated to point to the new end of the string
 */
extern void strnfcat(char* str, size_t max, size_t* end, cptr fmt, ...);

/*
 * Format into a static buffer (NOT thread-safe, but compatible with old code)
 * 
 * WARNING: This returns a pointer to a static buffer that will be overwritten
 * on the next call. For new code, prefer using strnfmt() with a local buffer.
 * 
 * This exists for compatibility with legacy code that uses format() extensively.
 */
extern char* format(cptr fmt, ...);

/*
 * Format a string into a caller-supplied buffer
 * This is a safer alternative to format() which uses a static buffer
 * 
 * Example: char buf[256]; format_string(buf, sizeof(buf), "Value: %d", x);
 */
static inline size_t format_string(char* buf, size_t max, cptr fmt, ...)
{
    va_list vp;
    size_t len;
    
    va_start(vp, fmt);
    len = vstrnfmt(buf, max, fmt, vp);
    va_end(vp);
    
    return len;
}

#endif /* INCLUDED_FORMAT_H */
