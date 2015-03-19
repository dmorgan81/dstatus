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
#include <pthread.h>

#include <errno.h>

#include <sys/select.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <alsa/asoundlib.h>
#include <X11/Xlib.h>
#include <linux/wireless.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

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
static CpuStat lstat;
#endif

#ifdef WITH_VOL
typedef struct {
    snd_mixer_t *card;
    snd_mixer_elem_t *mixer;
} VolDev;
static VolDev voldev;

typedef struct {
    int level;
    bool muted;
    pthread_mutex_t mutex;
} VolInfo;
static VolInfo volinfo;
#endif

#ifdef WITH_BKLT
static float backlight;
#endif

#ifdef WITH_WIFI
static char *wifi;
#endif

static int acpid_socket;
static char *acpid_events[] = ACPID_EVENTS;

static void update_status(void);

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

#ifdef WITH_VOL
static void vol_open(void) {
    snd_mixer_t *card;
    snd_mixer_elem_t *mixer;
    snd_mixer_selem_id_t *sid;

    if (snd_mixer_open(&card, 0) < 0)
        die("dstatus: cannot init ALSA");
    if (snd_mixer_attach(card, "default") < 0)
        die("dstatus: cannot init ALSA");
    if (snd_mixer_selem_register(card, NULL, NULL) < 0)
        die("dstatus: cannot init ALSA");
    if (snd_mixer_load(card) < 0)
        die("dstatus: cannot init ALSA");
    voldev.card = card;

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, VOL_MIXER);
    if (!(mixer = snd_mixer_find_selem(card, sid)))
        die("dstatus: cannot init ALSA");
    voldev.mixer = mixer;

    if (pthread_mutex_init(&(volinfo.mutex), NULL) != 0)
        die("dstatus: cannot init volinfo.mutex");
}

static void vol_close(void) {
    snd_mixer_close(voldev.card);
    snd_config_update_free_global();

    pthread_mutex_destroy(&(volinfo.mutex));
}

static void vol_lock(void) {
    if (pthread_mutex_lock(&(volinfo.mutex)) != 0)
        die("dstatus: cannot lock volinfo.mutex");
}

static void vol_unlock(void) {
    if (pthread_mutex_unlock(&(volinfo.mutex)) != 0)
        die("dstatus: cannot unlock volinfo.mutex");
}

static void read_vol(void) {
    int muted;
    long vol, min, max;
    snd_mixer_elem_t *mixer = voldev.mixer;

    vol_lock();

    if (snd_mixer_selem_get_playback_switch(mixer, SND_MIXER_SCHN_MONO, &muted) < 0)
        die("dstatus: cannot set vol.muted");
    volinfo.muted = !muted;

    snd_mixer_selem_get_playback_volume_range(mixer, &min, &max);
    if (snd_mixer_selem_get_playback_volume(mixer, SND_MIXER_SCHN_MONO, &vol) < 0)
        die("dstatus: cannot set vol.level");
    vol = vol < max ? (vol > min ? vol : min) : max;
    volinfo.level = (int) round((vol * 100.0f) / max);

    vol_unlock();
}

static void *vol_thread(void *arg) {
    if (pthread_detach(pthread_self()) != 0)
        die("dstatus: cannot detach vol thread");

    while (true) {
        snd_mixer_wait(voldev.card, -1);
        snd_mixer_handle_events(voldev.card);
        read_vol();
        update_status();
    }

    return NULL;
}
#endif

#ifdef WITH_BKLT
static void read_backlight(void) {
    FILE *fh;
    unsigned int max, current;

    if (!(fh = fopen(BKLT_MAX, "r")))
        goto bklt_error;
    if (fscanf(fh, "%ud", &max) != 1)
        goto bklt_error;
    fclose(fh);

    if (!(fh = fopen(BKLT_CURRENT, "r")))
        goto bklt_error;
    if (fscanf(fh, "%ud", &current) != 1)
        goto bklt_error;
    fclose(fh);

    backlight = 100.0 * ((double) current / max);
    return;

bklt_error:
    fclose(fh);
    die("dstatus: cannot read backlight");
}
#endif

#ifdef WITH_WIFI
static void read_wifi(void) {
    char buffer[IW_ESSID_MAX_SIZE + 1];
    int fd;
    struct iwreq req;

    free(wifi);
    wifi = NULL;

    strncpy(req.ifr_name, WIFI_IFACE, IFNAMSIZ);
    req.u.essid.pointer = &buffer;
    req.u.essid.length = sizeof(buffer);
    req.u.essid.flags = 0;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        die("dstatus: cannot connect to essid socket");
    if (ioctl(fd, SIOCGIWESSID, &req) == -1) {
        close(fd);
        die("dstatus: cannot query for essid");
    }

    close(fd);

    if (strlen(buffer) == 0) {
        if (asprintf(&wifi, "%s", WIFI_NO_CONNECT) == -1)
            die("dstatus: cannot set essid");
    } else {
        if (asprintf(&wifi, "%s", buffer) == -1)
            die("dstatus: cannot set essid");
    }
}

static void *wifi_thread(void *arg) {
    struct sockaddr_nl addr;
    int sock, len;
    char buffer[4096];
    struct nlmsghdr *nlh;

    if (pthread_detach(pthread_self()) != 0)
        die("dstatus: cannot detach wifi thread");

    if ((sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) == -1)
        die("dstatus: cannot open NETLINK_ROUTE socket");

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTMGRP_IPV4_IFADDR;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        die("dstatus: cannot bind to NETLINK_ROUTE socket");

    nlh = (struct nlmsghdr *) buffer;
    while ((len = recv(sock, nlh, sizeof(buffer), 0)) > 0) {
        while ((NLMSG_OK(nlh, len)) && (nlh->nlmsg_type != NLMSG_DONE)) {
            if (nlh->nlmsg_type == RTM_NEWADDR || nlh->nlmsg_type == RTM_DELADDR) {
                read_wifi();
                update_status();
            }
            nlh = NLMSG_NEXT(nlh, len);
        }
    }

    die("dstatus: end of wifi thread");

    return NULL;
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

    if (asprintf(&s, "%0.0f%%", CONSTRAIN(f)) == -1)
        die("dstatus: cannot format percentage");
    return s;
}

static char *get_wifi(void) {
#ifndef WITH_WIFI
    return EMPTY_STRING;
#else
    char *s;

    if (asprintf(&s, WIFI_FMT, wifi) == -1)
        die("dstatus: cannot format wifi");
    return s;
#endif // WITH_WIFI
}


static char *get_backlight(void) {
#ifndef WITH_BKLT
    return EMPTY_STRING;
#else
    char *s, *bar;

#ifdef BKLT_USE_BAR
    bar = draw_bar(backlight);
#else
    bar = draw_percent(backlight);
#endif // BKLT_USE_VAR

    if (asprintf(&s, BKLT_FMT, bar) == -1)
        die("dstatus: cannot format backlight");
    free(bar);

    return s;
#endif // WITH_BKLT
}

static char *get_vol(void) {
#ifndef WITH_VOL
    return EMPTY_STRING;
#else
    char *s, *bar;

    vol_lock();

    if (volinfo.muted) {
        if (asprintf(&bar, "%s", VOL_MUTED) == -1)
            die("dstatus: cannot format vol muted");
    } else {
#ifdef VOL_USE_BAR
        bar = draw_bar((float) volinfo.level);
#else
        bar = draw_percent((float) volinfo.level);
#endif // VOL_USE_BAR
    }

    vol_unlock();

    if (asprintf(&s, VOL_FMT, bar) == -1)
        die("dstatus: cannot format vol");
    free(bar);

    return s;
#endif // WITH_VOL
}

static char *get_batt(void) {
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

static char *get_cpu(void) {
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

static char *get_time(void) {
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

static void update_status(void) {
    char *time = get_time();
    char *cpu = get_cpu();
    char *batt = get_batt();
    char *vol = get_vol();
    char *backlight = get_backlight();
    char *wifi = get_wifi();

    set_status(STATUS_FMT, MODULES);

    free(time);
    free(cpu);
    free(batt);
    free(vol);
    free(backlight);
    free(wifi);
}

static char *read_acpid_socket(void) {
    char *s = NULL, *rs;
    char buffer[4096];
    fd_set rfds;
    ssize_t n;
    size_t size = 0;

    FD_ZERO(&rfds);
    FD_SET(acpid_socket, &rfds);

    if (select(acpid_socket+1, &rfds, NULL, NULL, NULL) == -1)
        die("dstatus: cannot read from acpid socket");

    if ((n = read(acpid_socket, buffer, sizeof(buffer))) == -1)
        die("dstatus: cannot read from acpid socket");

    if (!(rs = realloc(s, size + n + 1)))
        die("dstatus: cannot read from acpid socket");
    s = rs;

    memcpy(s + size, buffer, n);
    size += n;
    *(s+size) = 0;

    return s;
}

static void process_acpid_event(char *event) {
    char *s, *found;
    int len = sizeof(acpid_events) / sizeof(acpid_events[0]);

    for (int i = 0; i < len; i++) {
        s = acpid_events[i];
        if ((found = strstr(event, s))) {
#ifdef WITH_BKLT
            read_backlight();
#endif
            update_status();
            break;
        }
    }
}

static void *acpid_thread(void *arg) {
    char *s;
    struct sockaddr_un remote;

    if (pthread_detach(pthread_self()) != 0)
        die("dstatus: cannot detach acpid thread");

    if ((acpid_socket = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
        die("dstatus: cannot connect to acpid socket");

    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, ACPID_SOCKET);
    if (connect(acpid_socket, (struct sockaddr *)&remote, sizeof(remote)) == -1)
        die("dstatus: cannot connect to acpid socket");

    if (fcntl(acpid_socket, F_SETFD, FD_CLOEXEC) == -1)
        die("dstatus: cannot connect to acpid socket");

    while (true) {
        s = read_acpid_socket();
        process_acpid_event(s);
        free(s);
    }

    return NULL;
}

static void trap(const int signo) {
    if (signo != SIGINT)
        return;

#ifdef WITH_X
    XCloseDisplay(dpy);
#endif

#ifdef WITH_VOL
    vol_close();
#endif

#ifdef WITH_WIFI
    free(wifi);
#endif

    close(acpid_socket);

    exit(EXIT_SUCCESS);
}

int main(void) {
    pthread_t thrd_acpid, thrd_vol, thrd_wifi;

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

#ifdef WITH_BKLT
    read_backlight();
#endif

#ifdef WITH_WIFI
    read_wifi();
    if (pthread_create(&thrd_wifi, NULL, wifi_thread, NULL) != 0)
        die("dstatus: cannot create wifi thread");
#endif

#ifdef WITH_VOL
    vol_open();
    read_vol();
    if (pthread_create(&thrd_vol, NULL, vol_thread, NULL) != 0)
        die("dstatus: cannot create vol thread");
#endif

    if (pthread_create(&thrd_acpid, NULL, acpid_thread, NULL) != 0)
        die("dstatus: cannot create acpid thread");

    for (;;sleep(1))
        update_status();

    return EXIT_SUCCESS;
}
