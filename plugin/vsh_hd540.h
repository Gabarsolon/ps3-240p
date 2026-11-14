/* SPDX-License-Identifier: MIT. 4.93 HDMI-only first experiment backend. */
#ifndef VSH_HD540_H
#define VSH_HD540_H
#include "hd540_service.h"
const hd540_ops *vsh_hd540_ops(void);
const char *vsh_hd540_error(void);
void vsh_hd540_set_log(void (*)(const char *,uint32_t));
#endif
