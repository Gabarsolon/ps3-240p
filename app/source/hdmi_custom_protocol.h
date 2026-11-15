/* SPDX-License-Identifier: MIT. Session-only HDMI confirmation and ownership. */
#ifndef HDMI_CUSTOM_PROTOCOL_H
#define HDMI_CUSTOM_PROTOCOL_H
#include <stdint.h>
#define HC_MAGIC UINT32_C(0x48434d32)
#define HC_MODULE "crt_hdmi_custom"
#define HC_ROOT "/dev_hdd0/tmp/crt_hdmi_custom"
#define HC_REQUEST HC_ROOT ".request"
#define HC_REPLY HC_ROOT ".reply"
#define HC_READY HC_ROOT ".v2.ready"
#define HC_LOG HC_ROOT ".log"
enum {HC_SELECT=1,HC_STOP,HC_TRIAL,HC_APP_SELECT,HC_KEEP};
#define HC_APP HC_ROOT ".app"
#define HC_VIDEO HC_ROOT ".video"
#define HC_VIDEO_ACK HC_ROOT ".video_ack"
#define HC_TEST_LEASE HC_ROOT ".test_lease"
/* Optional automation guard, consumed once. It never changes boot settings. */
typedef struct {uint32_t magic,mode;uint64_t expires;} hc_test_lease;
enum {HC_VIDEO_QUIESCE=1,HC_VIDEO_BIND,HC_VIDEO_DONE,HC_VIDEO_ERROR};
typedef struct {uint32_t magic,owner,pid,reserved;uint64_t heartbeat;} hc_app;
typedef struct {uint32_t magic,owner,operation,phase,flags,mode,format,pitch,width,height,factor;} hc_video;
typedef struct {uint32_t magic,owner,operation,phase,error;} hc_video_ack;
enum {HC_XGA=1,HC_WXGA,HC_SXGA,HC_WUXGA,HC_540,HC_MODE_COUNT=5};
enum {HC_IDLE,HC_WAIT_APP,HC_PREPARING,HC_PREVIEW,HC_KEPT,HC_RESTORING,
      HC_SUSPENDED,HC_DONE,HC_REFUSED,HC_FAILED};
enum {HC_OK,HC_CANCELLED,HC_EXPIRED,HC_BACKEND,HC_INPUT,HC_CHANGED,HC_BAD_REQUEST};
typedef struct {uint32_t magic,sequence,action,mode;} hc_request;
typedef struct {uint32_t magic,sequence,state,detail,mode,remaining;} hc_reply;
_Static_assert(sizeof(hc_request)==16,"HDMI request ABI");
_Static_assert(sizeof(hc_reply)==24,"HDMI reply ABI");
#endif
