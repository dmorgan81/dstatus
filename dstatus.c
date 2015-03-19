#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>

#include <time.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include <alloca.h>

#include <errno.h>

#include <X11/Xlib.h>

#include "config.h"

#define EMPTY_STRING calloc(1, sizeof(char))
#define CONSTRAIN(A) (fmaxf(0.0, fminf(100.0, (A))))

extern int errno;

#ifdef WITH_X
static Display *dpy;
#endif

#ifdef WITH_CPU
typedef struct {
    unsigned long long idle;
    unsigned long long total;
} CpuStat;
CpuStat lstat;
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

#ifdef WITH_CPU
static void read_stat(CpuStat *stat) {
    FILE *fh;
    unsigned long long user, nice, system, idle;

    if (!(fh = fopen("/proc/stat", "r")))
        die("dstatus: cannot read /proc/stat");

    if (fscanf(fh, "%*s\t%llu\t%llu\t%llu\t%llu", &user, &nice, &system, &idle) != 4) {
        fclose(fh);
        die("dstatus: cannot parse /proc/stat");
    }
    fclose(fh);

    stat->idle = idle;
    stat->total = user + nice + system + idle;
}
#endif

static char *draw_bar(const float f) {
    char *s;

    if (!(s = calloc(BAR_LEN+1, sizeof(char))))
        die("dstatus: cannot allocate memory for bar");

    for (int i = 0, b = (int) round(CONSTRAIN(f) / BAR_LEN); i < BAR_LEN; i++)
        s[i] = i < b ? BAR_CHAR_ON : BAR_CHAR_OFF;

    return s;
}

static char *draw_percent(const float f) {
    char *s;


    if (asprintf(&s, "%0.2f", CONSTRAIN(f)) == -1)
        die("dstatus: cannot format percentage");
    return s;
}


static char *get_batt() {
#ifndef WITH_BATT
    return EMPTY_STRING;
#else
    FILE *fh;
    char *s, *bar;
    unsigned long now, full;
    float batt;
    char status;

    if (!(fh = fopen(BATT_CHARGE_NOW, "r")))
        goto batt_err;
    if (fscanf(fh, "%lu", &now) != 1)
        goto batt_err;
    fclose(fh);

    if (!(fh = fopen(BATT_CHARGE_FULL, "r")))
        goto batt_err;
    if (fscanf(fh, "%lu", &full) != 1)
        goto batt_err;
    fclose(fh);

    if (full <= 0)
        goto batt_err;

    batt = 100.0 * ((float) now / full);

    if (!(fh = fopen(BATT_STATUS, "r")) || ((status = fgetc(fh)) == EOF))
        status = 'U';
    fclose(fh);

#ifdef BATT_USE_BAR
    bar = draw_bar(batt);
#else
    bar = draw_percent(batt);
#endif // BATT_USE_BAR
    if (asprintf(&s, BATT_FMT, status, bar) == -1)
        die("dstatus: cannot format batt");
    free(bar);

    return s;
batt_err:
    fclose(fh);
    if (asprintf(&s, BATT_FMT, 'U', "<error>") == -1)
        die("dstatus: cannot format batt error");
    return s;
#endif // WITH_BATT
}

static char *get_cpu() {
#ifndef WITH_CPU
    return EMPTY_STRING;
#else
    char *s, *bar;
    float cpu;
    unsigned long long total;
    CpuStat stat;

    read_stat(&stat);

    total = stat.total - lstat.total;
    cpu = total > 0 ? 100.0 * ((double) (total - (stat.idle - lstat.idle)) / total) : 0.0;
    lstat.idle = stat.idle;
    lstat.total = stat.total;

#ifdef CPU_USE_BAR
    bar = draw_bar(cpu);
#else
    bar = draw_percent(cpu);
#endif // CPU_USE_BAR
    if (asprintf(&s, CPU_FMT, bar) == -1)
        die("dstatus: cannot format cpu");
    free(bar);

    return s;
#endif // WITH_CPU
}

static char *get_time() {
#ifndef WITH_TIME
    return EMPTY_STRING;
#else
    static char *cache;
    char *s;
    time_t t;
    size_t len;

    t = time(NULL);

    if (!cache || t % 60 == 0) {
        /* Note that if you want to display seconds, this cache won't work. */
        free(cache);

        if (!(cache = calloc(DATE_TIME_MAX_LEN + 1, sizeof(char))))
            die("dstatus: cannot allocate memory for time");
        if (strftime(cache, DATE_TIME_MAX_LEN, DATE_TIME_FMT, localtime(&t)) == -1)
            die("dstatus: cannot format time");
    }

    len = strlen(cache) + 1;
    if (!(s = calloc(len, sizeof(char))))
        die("dstatus: cannot allocate memory for time");
    strncpy(s, cache, len);

    return s;
#endif
}

static void set_status(const char *fmt, ...) {
    va_list ap;
    char *status;

    va_start(ap, fmt);
    if (vasprintf(&status, fmt, ap) == -1)
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
    char *time = get_time();
    char *cpu = get_cpu();
    char *batt = get_batt();

    set_status(STATUS_FMT, MODULES);

    free(time);
    free(cpu);
    free(batt);
}

int main(void) {
#ifdef WITH_X
    if (!(dpy = XOpenDisplay(NULL)))
        die("dstatus: cannot open display");
#else
    setbuf(stdout, NULL);
#endif

    signal(SIGINT, trap);

#ifdef WITH_CPU
    read_stat(&lstat);
#endif

    for (;;sleep(1))
        update_status();

    return EXIT_SUCCESS;
}
