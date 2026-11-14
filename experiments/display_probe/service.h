/* SPDX-License-Identifier: MIT. Timed, RAM-only native display investigation. */
#ifndef DISPLAY_PROBE_SERVICE_H
#define DISPLAY_PROBE_SERVICE_H
#include <stdint.h>
#define DPROBE_MAGIC UINT32_C(0x44535031)
#define DPROBE_REQUEST "/dev_hdd0/tmp/crt_display_probe.request"
#define DPROBE_REPLY "/dev_hdd0/tmp/crt_display_probe.reply"
#define DPROBE_READY "/dev_hdd0/tmp/crt_display_probe.v1.ready"
#define DPROBE_LOG "/dev_hdd0/tmp/crt_display_probe.log"
enum {DP_INSPECT=1,DP_STOCK720,DP_CAPS,DP_WXGA,DP_CANCEL};
enum {DP_IDLE,DP_WAIT_SOURCE,DP_WAIT_TARGET,DP_ACTIVE,DP_RESTORE,DP_DONE,DP_REFUSED,DP_RECOVERY_FAILED};
enum {DP_OK,DP_CANCELLED,DP_PREPARE_ERROR,DP_ENTER_ERROR,DP_TIMEOUT,DP_RESTORE_ERROR,DP_PREREQUISITE,DP_BAD_REQUEST};
typedef struct {uint32_t magic,sequence,action,value;} dp_request;
typedef struct {uint32_t magic,sequence,state,detail;} dp_reply;
typedef struct {
 int (*prepare)(void *),(*stock)(void *),(*caps)(void *),(*candidate)(void *);
 int (*at_stock)(void *),(*at_target)(void *,unsigned),(*restore)(void *),(*at_original)(void *);
 uint64_t (*clock)(void *);
 void *user;
} dp_ops;
typedef struct {
 uint32_t sequence,seen,state,detail,kind;
 unsigned controls,retries;
 int owned,restore_error,halted;
 uint64_t deadline,retry;
} dp_service;
int dp_busy(const dp_service *);
int dp_tick(dp_service *,const dp_ops *,uint64_t,const dp_request *,dp_reply *);
#endif
