/* SPDX-License-Identifier: MIT. Serial GPU-quiesce / native-call / rebind handshake. */
#ifndef HDMI_CUSTOM_VIDEO_H
#define HDMI_CUSTOM_VIDEO_H
#include <stdint.h>
typedef int (*hc_video_fn)(unsigned,uint32_t,uint32_t,uint32_t,uint32_t,void *,int);
int hc_video_begin(unsigned);
int hc_video_app_alive(void);
int hc_video_departed(void);
int hc_video_submit(hc_video_fn,uint32_t,uint32_t,uint32_t,uint32_t);
int hc_video_ready(void);
int hc_video_scanout_matches(unsigned,unsigned,unsigned);
void hc_video_poll(void);
void hc_video_log(void (*)(const char *,uint32_t));
#endif
