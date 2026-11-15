/* SPDX-License-Identifier: MIT. One serial owner for both existing engines. */
#ifndef HDMI_CUSTOM_BACKEND_H
#define HDMI_CUSTOM_BACKEND_H
#include "service.h"
int hc_backend_start(void *,unsigned);
int hc_backend_tick(void *);
int hc_backend_keep(void *);
void hc_backend_cancel(void *);
int hc_backend_health(void *);
void hc_backend_log(void (*)(const char *,uint32_t));
unsigned hc_backend_scanout_factor(void);
#endif
