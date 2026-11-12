/* SPDX-License-Identifier: MIT. VSH resident SD corrector (safe v2).
 *
 * v1 (2026-11-11) froze the console. Root causes, checked against the SDK
 * headers (cell/cell_video_out.h, sysutil/video.h):
 *   - the scanMode test was inverted: VIDEO_SCANMODE_INTERLACE is 0 and
 *     PROGRESSIVE is 1, so the corrector re-fired on every 500 ms poll;
 *   - every fired configure carried pitch = 0;
 *   - module_start() ended with syscall 41, killing the Cobra loader thread.
 *
 * v2 contract:
 *   - armed only while /dev_hdd0/tmp/240p_enforcer.on exists;
 *   - passive while /dev_hdd0/tmp/240p_app.active exists, because the
 *     switcher app manages its own transitions (flip drain + readback);
 *   - corrects only PAL 576i (resolution 5, scanMode 0) to resolution 4,
 *     whose patched LV1 timing table outputs 240p; 480i and HD are untouched;
 *   - requires ~2 s of stable readback before acting, at most two
 *     corrections per episode, 30 s idle after failures;
 *   - every event is logged to /dev_hdd0/tmp/vsh_240p.log.
 */
#include <sys/prx_module.h>
#include <sys/thread.h>
#include <sys/timer.h>
#include <sys/file.h>
#include <sys/sys_time.h>
#include <ppu-lv2.h>
#include <stdint.h>
#include <stdatomic.h>

#define ENFORCER_FLAG   "/dev_hdd0/tmp/240p_enforcer.on"
#define APP_ACTIVE_FLAG "/dev_hdd0/tmp/240p_app.active"
#define LOG_PATH        "/dev_hdd0/tmp/vsh_240p.log"
#define LOG_LIMIT       (64u * 1024u)

#define RES_480I        4u
#define RES_576I        5u
#define SCAN_INTERLACE  0u
#define ASPECT_16_9     2u
#define ASPECT_4_3      1u
#define PITCH_720_XRGB  2880u /* 720 * 4 (XRGB); v1 sent 0 here */
#define DEBOUNCE_TICKS  4u    /* 4 x 500 ms = 2 s of stable 576i */
#define MAX_ATTEMPTS    2u
#define IDLE_MS         30000u

typedef struct {
    uint8_t  resolution;
    uint8_t  scanMode;
    uint8_t  conversion;
    uint8_t  aspect;
    uint8_t  padding[2];
    uint16_t refreshRates;
} videoDisplayMode;

typedef struct {
    uint8_t          state;
    uint8_t          colorSpace;
    uint8_t          padding[6];
    videoDisplayMode displayMode;
} videoState;

typedef struct {
    uint8_t  resolution;
    uint8_t  format;
    uint8_t  aspect;
    uint8_t  padding[9];
    uint32_t pitch;
} videoConfiguration;

static inline void nap_ms(uint32_t ms) {
    sys_timer_usleep((uint64_t)ms * 1000u);
}

static inline int sys_video_configure(uint32_t videoOut, videoConfiguration *cfg) {
    lv2syscall4(107, (uint64_t)videoOut, (uint64_t)(uintptr_t)cfg, 0, 0);
    return_to_user_prog(int);
}

static inline int sys_video_get_state(uint32_t videoOut, videoState *state) {
    lv2syscall3(108, (uint64_t)videoOut, 0, (uint64_t)(uintptr_t)state);
    return_to_user_prog(int);
}

static inline void exit_thread(void) {
    lv2syscall1(41, 0);
    __builtin_unreachable();
}

static int flag_exists(const char *path) {
    sysFSStat st;
    return sysLv2FsStat(path, &st) == 0;
}

static int log_fd=-1;
static unsigned log_bytes;

/* Logging follows the proven ps3buzz_vsh pattern (buzz-pad-bridge
 * ps3/plugin/autoload/vsh.c): open once with O_TRUNC, keep the descriptor.
 * The earlier per-line open with O_APPEND created the file but wrote zero
 * bytes and wedged the dev_hdd0 write path for every other writer. */
static void log_open(void) {
    (void)sysLv2FsOpen(LOG_PATH, SYS_O_WRONLY | SYS_O_CREAT | SYS_O_TRUNC, &log_fd, 0666, NULL, 0);
}

static void log_line(const char *event, uint32_t code) {
    if (log_fd < 0 || log_bytes > 65500u) return;
    char buf[96];
    char *p = buf;
    uint64_t us = sys_time_get_system_time();
    char digits[20];
    unsigned n = 0;
    if (!us) digits[n++] = '0';
    while (us) { digits[n++] = (char)('0' + (int)(us % 10u)); us /= 10u; }
    while (n) *p++ = digits[--n];
    *p++ = ' ';
    const char *s = event;
    while (*s) *p++ = *s++;
    *p++ = ' ';
    *p++ = 'c'; *p++ = 'o'; *p++ = 'd'; *p++ = 'e'; *p++ = '=';
    static const char hexdig[] = "0123456789abcdef";
    for (int i = 28; i >= 0; i -= 4) *p++ = hexdig[(code >> i) & 0xFu];
    *p++ = '\n';
    uint64_t wr = 0;
    if (sysLv2FsWrite(log_fd, buf, (uint64_t)(p - buf), &wr) == 0) log_bytes += (unsigned)wr;
}

static atomic_int running, started;
static sys_ppu_thread_t thread_id;

static void corrector(void *arg) {
    (void)arg;
    log_open();
    log_line("start", 0);
    for (unsigned i = 0; i < 6u && atomic_load(&running); i++) nap_ms(500);
    unsigned stable = 0, attempts = 0;
    uint64_t idle_until = 0;
    int seen_armed = -1, seen_app = -1;
    while (atomic_load(&running)) {
        nap_ms(500);
        int armed = flag_exists(ENFORCER_FLAG);
        if (armed != seen_armed) { seen_armed = armed; log_line(armed ? "armed" : "disarmed", 0); }
        if (!armed) { stable = 0; attempts = 0; continue; }
        int app = flag_exists(APP_ACTIVE_FLAG);
        if (app != seen_app) { seen_app = app; log_line(app ? "app_active" : "app_gone", 0); }
        if (app) { stable = 0; continue; }
        videoState st;
        uint8_t *z = (uint8_t *)&st;
        for (unsigned i = 0; i < sizeof(st); i++) z[i] = 0;
        if (sys_video_get_state(0, &st) != 0 || st.state != 0) { stable = 0; continue; }
        uint32_t res = st.displayMode.resolution;
        uint32_t scan = st.displayMode.scanMode;
        uint32_t asp = st.displayMode.aspect;
        /* code packs the readback: res<<16 | scan<<8 | aspect */
        uint32_t code = (res << 16) | (scan << 8) | asp;
        if (res == RES_480I) {
            if (stable) log_line("target", code);
            stable = 0; attempts = 0; continue;
        }
        if (res != RES_576I || scan != SCAN_INTERLACE) { stable = 0; continue; }
        stable++;
        if (stable < DEBOUNCE_TICKS) continue;
        uint64_t now = sys_time_get_system_time();
        if (now < idle_until) continue;
        if (attempts >= MAX_ATTEMPTS) {
            log_line("giving_up", code);
            idle_until = now + (uint64_t)IDLE_MS * 1000u;
            attempts = 0; stable = 0;
            continue;
        }
        attempts++;
        stable = DEBOUNCE_TICKS - 2u; /* require two fresh ticks before a retry */
        log_line("correct", code);
        videoConfiguration cfg;
        uint8_t *c = (uint8_t *)&cfg;
        for (unsigned i = 0; i < sizeof(cfg); i++) c[i] = 0;
        cfg.resolution = (uint8_t)RES_480I;
        cfg.format = 0; /* VIDEO_BUFFER_FORMAT_XRGB */
        cfg.aspect = (uint8_t)(asp == ASPECT_16_9 ? ASPECT_16_9 : ASPECT_4_3);
        cfg.pitch = PITCH_720_XRGB;
        int rc = sys_video_configure(0, &cfg);
        log_line(rc ? "correct_fail" : "correct_sent", (uint32_t)rc);
    }
    log_line("stop", 0);
    exit_thread();
}

int module_start(size_t args, void *argp) {
    (void)args;
    (void)argp;
    if (!atomic_exchange(&started, 1)) {
        atomic_store(&running, 1);
        if (sysThreadCreate(&thread_id, corrector, NULL, 1100, 65536, THREAD_JOINABLE, "vsh_240p")) {
            atomic_store(&running, 0);
        }
    }
    /* Return normally; v1 terminated the Cobra loader thread here. */
    return 0;
}

int module_stop(size_t args, void *argp) {
    (void)args;
    (void)argp;
    if (atomic_exchange(&running, 0)) {
        uint64_t status = 0;
        (void)sysThreadJoin(thread_id, &status);
    }
    return 0;
}
