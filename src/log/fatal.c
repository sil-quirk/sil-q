/* log/fatal.c - unified plog/quit/core implementations */

#include "../h-basic.h"
#include "../log/log.h"
#include "../log/fatal.h"
#include <stdio.h>
#include <stdlib.h>
#if defined(__ANDROID__)
#include <unistd.h>   /* _exit */
#endif

/*
 * Terminate the process.
 *
 * On Android plain exit() runs the C++ static/global destructors, which tear
 * down libhwui's global TaskManager mutex while its hwuiTask worker threads are
 * still alive.  The next render task then locks a destroyed mutex and the
 * process aborts with SIGABRT ("FORTIFY: pthread_mutex_lock called on a
 * destroyed mutex") instead of exiting cleanly -- the user sees a "crash" on
 * every quit.  Flush our log (written per-line anyway) and terminate
 * immediately with _exit() so those destructors never run.  The quit hooks
 * (which nuke the terms) have already executed before we get here.
 */
static void sil_process_exit(int code)
{
    fflush(NULL);
#if defined(__ANDROID__)
    _exit(code);
#else
    exit(code);
#endif
}

#define MAX_QUIT_HOOKS 8

static quit_hook_fn quit_hooks[MAX_QUIT_HOOKS];
static size_t quit_hook_count = 0;

void log_register_quit_hook(quit_hook_fn hook)
{
    if (!hook)
        return;

    if (quit_hook_count >= MAX_QUIT_HOOKS)
    {
        log_warn("Too many quit hooks registered; ignoring new hook");
        return;
    }

    quit_hooks[quit_hook_count++] = hook;
}

void plog(cptr str)
{
    if (!str)
        return;

    log_error("%s", str);
}

static void invoke_quit_hooks(cptr str)
{
    while (quit_hook_count > 0)
    {
        quit_hook_fn hook = quit_hooks[--quit_hook_count];
        hook(str);
    }
}

void quit(cptr str)
{
    invoke_quit_hooks(str);

    if (!str)
    {
        log_info("quit(): clean shutdown requested");
        sil_process_exit(0);
    }

    if ((str[0] == '-') || (str[0] == '+'))
    {
        int code = atoi(str);
        log_info("quit(): exiting with explicit code %d", code);
        sil_process_exit(code);
    }

    log_fatal("%s", str);
    sil_process_exit(EXIT_FAILURE);
}

void core(cptr str)
{
    if (str)
        log_fatal("core(): %s", str);
    else
        log_fatal("core(): no message provided");

    abort();
}
