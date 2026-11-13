/* SPDX-License-Identifier: MIT. Native VSH SD corrector (v5).
 * Only corrects the opposite SD family; HD stays untouched. Polling is
 * disarmed without the enable flag and paused while the app is active.
 * Video calls resolve genuine VSH interfaces by export NID (vsh_video.c).
 * The former raw syscalls 107/108 were condition-variable APIs, not video.
 * Start/stop exit their loader thread with syscall 41; the worker is joined.
 */
#include <sys/prx_module.h>
#include <sys/thread.h>
#include <sys/timer.h>
#include <sys/file.h>
#include <sys/sys_time.h>
#include <ppu-lv2.h>
#include <stdint.h>
#include <stdatomic.h>
#include "../app/source/enforcer_mode.h"
#include "vsh_video.h"

#define ENFORCER_FLAG   "/dev_hdd0/tmp/240p_enforcer.on"
#define APP_ACTIVE_FLAG "/dev_hdd0/tmp/240p_app.active"
#define TARGET_PATH     "/dev_hdd0/tmp/240p_target.mode"
#define READY_PATH     "/dev_hdd0/tmp/240p_enforcer.ready"
#define LOG_PATH        "/dev_hdd0/tmp/vsh_240p.log"
#define LOG_LIMIT       (64u * 1024u)

#define RES_480I        4u
#define RES_576I        5u
#define SCAN_INTERLACE  0u
#define ASPECT_16_9     2u
#define ASPECT_4_3      1u
#define DEBOUNCE_TICKS  4u    /* 4 x 500 ms = 2 s of stable 576i */
#define MAX_ATTEMPTS    2u
#define IDLE_MS         30000u

static inline void nap_ms(uint32_t ms) {
    sys_timer_usleep((uint64_t)ms * 1000u);
}

static inline void exit_thread(void) {
    lv2syscall1(41, 0);
    __builtin_unreachable();
}

static int flag_exists(const char *path) {
    sysFSStat st;
    return sysLv2FsStat(path, &st) == 0;
}
static unsigned read_target(void) {
    int fd=-1;char data[3]={0};uint64_t rd=0;
    if(sysLv2FsOpen(TARGET_PATH,SYS_O_RDONLY,&fd,0,NULL,0))return RES_480I;
    int rc=sysLv2FsRead(fd,data,sizeof(data),&rd);sysLv2FsClose(fd);
    return rc?RES_480I:enforcer_target(data,(unsigned)rd);
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
    if (log_fd < 0 || log_bytes > LOG_LIMIT - 96u) return;
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
    log_line("start_native_v5", 0);
    int bound=vsh_video_bind();
    log_line("native_bind",(uint32_t)bound);
    if(bound){atomic_store(&running,0);sysLv2FsClose(log_fd);log_fd=-1;exit_thread();}

    /* Liveness breadcrumb: "start" only proves the thread began. If the next
     * line is missing, the thread never got scheduled again (or the log fd
     * broke after the first write). "alive" proves the worker survived the
     * start-up settle and reached the poll loop. */
    for (unsigned i = 0; i < 6u && atomic_load(&running); i++) nap_ms(500);
    log_line("alive", 0);
    int ready=-1;
    if(sysLv2FsOpen(READY_PATH,SYS_O_WRONLY|SYS_O_CREAT|SYS_O_TRUNC,&ready,0666,NULL,0)==0)sysLv2FsClose(ready);

    unsigned stable = 0, attempts = 0;
    uint64_t idle_until = 0;
    int seen_armed = -1, seen_app = -1;
    unsigned target=RES_480I;
    uint32_t last_state=UINT32_MAX;int last_error=0;
    while (atomic_load(&running)) {
        nap_ms(500);
        int armed = flag_exists(ENFORCER_FLAG);
        if (armed != seen_armed) { seen_armed = armed; target=read_target(); stable=attempts=0; log_line(armed ? "armed" : "disarmed", target); }
        if (!armed) { stable = 0; attempts = 0; continue; }
        int app = flag_exists(APP_ACTIVE_FLAG);
        if (app != seen_app) { seen_app = app; target=read_target(); stable=attempts=0; log_line(app ? "app_active" : "app_gone", target); }
        if (app) { stable = 0; continue; }
        vsh_video_state st;
        int query=vsh_video_query(&st);
        if(query){
            if(query!=last_error)log_line("native_query_error",(uint32_t)query);
            last_error=query;stable=0;continue;
        }
        last_error=0;
        if(st.state!=0){stable=0;continue;}
        uint32_t res=st.raw[8];
        /* Native xsetting scan values: 1=interlace, 2=progressive.
         * The public app API uses 0/1; do not mix these namespaces. */
        uint32_t scan=st.raw[9]==1?SCAN_INTERLACE:1u;
        uint32_t asp=st.raw[11];
        /* code packs the readback: res<<16 | scan<<8 | aspect */
        uint32_t code = (res << 16) | (scan << 8) | asp;
        if(code!=last_state){last_state=code;log_line("native_state",code);}
        if (res == target) {
            if (stable) log_line("target", code);
            stable = 0; attempts = 0; continue;
        }
        if (!enforcer_should_correct(target,res,scan)) { stable = 0; continue; }
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
        int rc=vsh_video_configure(target,asp);
        log_line(rc ? "correct_fail" : "correct_sent", (uint32_t)rc);
    }
    log_line("stop", 0);
    (void)sysLv2FsUnlink(READY_PATH);
    if(log_fd>=0){sysLv2FsClose(log_fd);log_fd=-1;}
    exit_thread();
}

int module_start(size_t args, void *argp) {
    (void)args;
    (void)argp;
    if (!atomic_exchange(&started, 1)) {
        (void)sysLv2FsUnlink(READY_PATH);
        atomic_store(&running, 1);
        if (sysThreadCreate(&thread_id, corrector, NULL, 1100, 65536, THREAD_JOINABLE, "vsh_240p")) {
            atomic_store(&running, 0);
        }
    }
    /* Cobra's loadprx waits for this thread to terminate. Returning normally
     * hangs the caller, so the loader thread must exit here (syscall 41).
     * Mirrors ps3buzz_vsh module_start exactly. */
    exit_thread();
    return 0;
}

int module_stop(size_t args, void *argp) {
    (void)args;
    (void)argp;
    if (atomic_exchange(&running, 0)) {
        uint64_t status = 0;
        (void)sysThreadJoin(thread_id, &status);
    }
    exit_thread();
    return 0;
}
