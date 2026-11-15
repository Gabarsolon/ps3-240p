/* SPDX-License-Identifier: MIT. Timed, RAM-only native display investigation. */
#ifndef DISPLAY_PROBE_SERVICE_H
#define DISPLAY_PROBE_SERVICE_H
#include <stdint.h>
#ifdef CRT_XGA_PROBE
#define DPROBE_MAGIC UINT32_C(0x58474131)
#define DPROBE_ROOT "/dev_hdd0/tmp/crt_xga_probe"
#define DPROBE_READY DPROBE_ROOT ".v1.ready"
#define DPROBE_PROFILES 9u
#define DPROBE_MODULE "crt_xga_probe"
#define DPROBE_START "xga_probe_v1_start"
#else
#define DPROBE_MAGIC UINT32_C(0x44535032)
#define DPROBE_ROOT "/dev_hdd0/tmp/crt_display_probe"
#define DPROBE_READY DPROBE_ROOT ".v2.ready"
#define DPROBE_PROFILES 3u
#define DPROBE_MODULE "crt_display_probe"
#define DPROBE_START "display_probe_v2_start"
#endif
#define DPROBE_REQUEST DPROBE_ROOT ".request"
#define DPROBE_REPLY DPROBE_ROOT ".reply"
#define DPROBE_LOG DPROBE_ROOT ".log"

enum {DP_INSPECT=1,DP_STOCK720,DP_CAPS,DP_WXGA,DP_CANCEL,DP_AUTO};
enum {DP_IDLE,DP_WAIT_SOURCE,DP_WAIT_TARGET,DP_ACTIVE,DP_RESTORE,DP_DONE,DP_REFUSED,DP_RECOVERY_FAILED,DP_WAIT_APP,DP_HELD};
enum {DP_OK,DP_CANCELLED,DP_PREPARE_ERROR,DP_ENTER_ERROR,DP_TIMEOUT,DP_RESTORE_ERROR,DP_PREREQUISITE,DP_BAD_REQUEST};
typedef struct {uint32_t magic,sequence,action,value;} dp_request;
typedef struct {uint32_t magic,sequence,state,detail;} dp_reply;
typedef struct {
 int (*prepare)(void *,unsigned);
 int (*stock)(void *),(*caps)(void *),(*candidate)(void *);
 int (*at_stock)(void *),(*at_target)(void *,unsigned),(*restore)(void *),(*at_original)(void *);
 uint64_t (*clock)(void *);
 int (*app_active)(void *);
 void *user;
} dp_ops;
typedef struct {
 uint32_t sequence,seen,state,detail,kind,profile;
 unsigned controls,retries;
 int owned,restore_error,halted,automatic;
 uint64_t deadline,retry,app_settle;
} dp_service;
int dp_busy(const dp_service *);
/* Explicit confirmation of a candidate; controls cannot be held. */
int dp_keep(dp_service *);
int dp_tick(dp_service *,const dp_ops *,uint64_t,const dp_request *,dp_reply *);
#endif
