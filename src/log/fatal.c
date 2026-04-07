/* log/fatal.c - unified plog/quit/core implementations */

#include "../h-basic.h"
#include "../log/log.h"
#include "../log/fatal.h"
#include <stdlib.h>

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
        exit(0);
    }

    if ((str[0] == '-') || (str[0] == '+'))
    {
        int code = atoi(str);
        log_info("quit(): exiting with explicit code %d", code);
        exit(code);
    }

    log_fatal("%s", str);
    exit(EXIT_FAILURE);
}

void core(cptr str)
{
    if (str)
        log_fatal("core(): %s", str);
    else
        log_fatal("core(): no message provided");

    abort();
}
