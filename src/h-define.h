/* File: h-define.h */

#ifndef INCLUDED_H_DEFINE_H
#define INCLUDED_H_DEFINE_H

/*
 * Define some simple constants
 */

/*
 * Hack -- Define NULL
 */
#ifndef NULL
#define NULL ((void*)0)
#endif /* NULL */

/*
 * Hack -- force definitions -- see fd_seek()
 */
#ifndef SEEK_SET
#define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#define SEEK_CUR 1
#endif
#ifndef SEEK_END
#define SEEK_END 2
#endif

/*
 * Hack -- force definitions -- see fd_lock()  XXX XXX XXX
 */
#ifndef F_UNLCK
#define F_UNLCK 0
#endif
#ifndef F_RDLCK
#define F_RDLCK 1
#endif
#ifndef F_WRLCK
#define F_WRLCK 2
#endif

/*
 * The constants "TRUE" and "FALSE"
 */

#undef TRUE
#define TRUE 1

#undef FALSE
#define FALSE 0

/**** Simple "Macros" ****/

/*
 * Non-typed minimum value macro
 */
#undef MIN
#define MIN(a, b) (((a) > (b)) ? (b) : (a))

/*
 * Non-typed maximum value macro
 */
#undef MAX
#define MAX(a, b) (((a) < (b)) ? (b) : (a))

/*
 * Non-typed absolute value macro
 */
#undef ABS
#define ABS(a) (((a) < 0) ? (-(a)) : (a))

/*
 * Non-typed sign extractor macro
 */
#undef SGN
#define SGN(a) (((a) < 0) ? (-1) : ((a) != 0))

/*
 * Non-typed between value macro
 */
#undef ORDERED
#define ORDERED(a, b, c)                                                       \
    ((((a) <= (b)) && ((b) <= (c))) || (((c) <= (b)) && ((b) <= (a))))

/*
 * Hack -- allow use of "ASCII" and "EBCDIC" for "indexes", "digits",
 * and "Control-Characters".
 *
 * Note that all "index" values must be "lowercase letters", while
 * all "digits" must be "digits".  Control characters can be made
 * from any legal characters.  XXX XXX XXX
 */
#define A2I(X) ((X) - 'a')
#define I2A(X) ((X) + 'a')
#define D2I(X) ((X) - '0')
#define I2D(X) ((X) + '0')
#define KTRL(X) ((X) & 0x1F)
#define UN_KTRL(X) ((X) + 64)
#define ESCAPE '\033'

/*
 * Annotate intentional switch/case fallthroughs.
 *
 * - GCC and Clang warn about implicit fallthrough (if enabled), and both
 *   define __GNUC__.
 * - MSVC doesn't warn, so it doesn't need a fallthrough marker. But it would
 *   error on parsing `__attribute__((fallthrough))`, hence the empty else
 *   branch.
 */
#ifdef __GNUC__
#define FALLTHROUGH __attribute__((fallthrough))
#else
#define FALLTHROUGH
#endif

#endif /* INCLUDED_H_DEFINE_H */
