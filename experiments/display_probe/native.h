/* SPDX-License-Identifier: MIT. Audited retail 4.93 native interfaces only. */
#ifndef DISPLAY_PROBE_NATIVE_H
#define DISPLAY_PROBE_NATIVE_H
#include "service.h"
const dp_ops *dp_native_ops(void);
void dp_native_log(void (*)(const char *,uint32_t));
#endif
