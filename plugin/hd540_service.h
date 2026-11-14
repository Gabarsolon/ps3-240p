/* SPDX-License-Identifier: MIT. Tick-driven recovery independent of app UI. */
#ifndef HD540_SERVICE_H
#define HD540_SERVICE_H
#include "../app/source/hd540_protocol.h"
typedef struct {
 int (*prepare)(void *),(*alternate)(void *),(*is_alternate)(void *);
 int (*apply)(void *,unsigned),(*original)(void *),(*is_original)(void *,int);
 int (*restore)(void *);
 void *user;
 int (*ready)(void *); /* App gone and XMB surface stable; no display writes. */
 uint64_t (*clock)(void *); /* Monotonic time after callbacks; NULL uses tick time. */
} hd540_ops;
typedef struct {
 unsigned sequence,seen,state,kind,video_tries;
 int recovering,had_error,moved,patched,halted,roundtrip_ok,hdmi_progressive_ok;
 uint64_t deadline,retry;
} hd540_service;
int hd540_service_busy(const hd540_service *);
/* Unknown/duplicate requests cannot extend a test or acknowledge success. */
int hd540_service_tick(hd540_service *,const hd540_ops *,uint64_t,
                       const hd540_request *,hd540_reply *);
#endif
