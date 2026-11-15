/* SPDX-License-Identifier: MIT. No firmware or OS dependencies. */
#ifndef HDMI_CUSTOM_SERVICE_H
#define HDMI_CUSTOM_SERVICE_H
#include "../../app/source/hdmi_custom_protocol.h"
enum {HC_ENGINE_BUSY,HC_ENGINE_PREVIEW,HC_ENGINE_HELD,HC_ENGINE_DONE,HC_ENGINE_ERROR,HC_ENGINE_FAILED};
typedef struct {
 int (*foreground)(void *);
 int (*start)(void *,unsigned);
 int (*tick)(void *);
 int (*keep)(void *);
 void (*cancel)(void *);
 int (*health)(void *);
 int (*prompt)(void *,unsigned,unsigned); /* start/poll: 0 waiting, 1 keep, 2 cancel, -1 error */
 void (*close_prompt)(void *);
 uint64_t (*clock)(void *);
 void *user;
} hc_ops;
typedef struct {
 uint32_t sequence,seen,state,detail,mode;
 int engine_owned,resume,automatic_keep,trial;
 uint64_t settle,deadline,unhealthy,keep_until;
 unsigned remaining;
} hc_service;
int hc_busy(const hc_service *);
int hc_trial_valid(const hc_test_lease *,unsigned,uint64_t);
int hc_tick(hc_service *,const hc_ops *,uint64_t,const hc_request *,hc_reply *);
#endif
