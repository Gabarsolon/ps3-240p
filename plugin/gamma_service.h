/* SPDX-License-Identifier: MIT. Recovery timer lives in VSH, outside the app. */
#ifndef CRT_GAMMA_SERVICE_H
#define CRT_GAMMA_SERVICE_H
#include "../app/source/gamma_protocol.h"
typedef struct {
 unsigned sequence,current,previous_value;int active,restore_all;
 uint64_t deadline,retry;float original[3],previous[3];
} gamma_service;
typedef struct {
 int (*snapshot)(float [3]);int (*apply)(const float [3]);int (*set)(unsigned);
} gamma_service_ops;
int gamma_service_restore(gamma_service *,const gamma_service_ops *);
/* Returns 1 if a reply should be written. NULL request advances only time. */
int gamma_service_tick(gamma_service *,const gamma_service_ops *,uint64_t now,
                       const gamma_request *,gamma_reply *);
#endif
