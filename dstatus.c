#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>

#include <time.h>
#include <strings.h>
#include <signal.h>
#include <alloca.h>

#include <errno.h>

#include <X11/Xlib.h>

#include "config.h"

#define EMPTY_STRING calloc(1, sizeof(char))

extern int errno;

#ifdef WITH_X
static Display *dpy;
#endif

static void die(const char *fmt, ...) {
    va_list ap;
    char *s;

    va_start(ap, fmt);
    vasprintf(&s, fmt, ap);
    perror(s);
    free(s);
    va_end(ap);
    exit(EXIT_FAILURE);
}

static void trap(const int signo) {
    if (signo != SIGINT)
        return;

#ifdef WITH_X
    XCloseDisplay(dpy);
#endif

    exit(EXIT_SUCCESS);
}

static char *get_time() {
#ifndef WITH_TIME
    return EMPTY_STRING;
#else
    char *s;
    time_t t;

    if (!(s = calloc(DATE_TIME_MAX_LEN + 1, sizeof(char))))
        die("dstatus: cannot allocate memory for time");

    t = time(NULL);
    if (strftime(s, DATE_TIME_MAX_LEN, DATE_TIME_FMT, localtime(&t)) == -1)
        die("dstatus: cannot format time");

    return s;
#endif
}

static void set_status(const char *sep, ...) {
    va_list ap;
    char *status;

    va_start(ap, sep);
    if (vasprintf(&status, "%s", ap) == -1)
        die("dstatus: cannot set status");
    va_end(ap);

#ifdef WITH_X
    XStoreName(dpy, DefaultRootWindow(dpy), status);
    XSync(dpy, false);
#else
    printf("%s\n", status);
#endif

    free(status);
}

static void update_status() {
    char * time = get_time();

    set_status(SEPARATOR, MODULES);

    free(time);
}

int main(void) {
#ifdef WITH_X
    if (!(dpy = XOpenDisplay(NULL)))
        die("dstatus: cannot open display");
#else
    setbuf(stdout, NULL);
#endif

    signal(SIGINT, trap);

    for (;;sleep(1))
        update_status();

    return EXIT_SUCCESS;
}
