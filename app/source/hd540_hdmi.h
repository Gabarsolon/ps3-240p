/* SPDX-License-Identifier: MIT. 4.93 temporary RAM AVI-builder experiment. */
#ifndef HD540_HDMI_H
#define HD540_HDMI_H
#include "lv1_patch.h"
#define HD540_HDMI_TARGETS 9
typedef struct {
 patch_context *transport;
 uint64_t address[HD540_HDMI_TARGETS],original[4];
 int ready,dirty;
 char error[160];
} hd540_hdmi;
int hd540_hdmi_identity(const uint64_t words[2]);
int hd540_hdmi_template_identity(const uint64_t words[2]);
int hd540_hdmi_resolve(hd540_hdmi *,patch_context *);
int hd540_hdmi_apply(hd540_hdmi *);
int hd540_hdmi_restore(hd540_hdmi *);
/* Validated software AVI cache, not an on-wire measurement. */
int hd540_hdmi_vic(hd540_hdmi *);
int hd540_hdmi_cache(hd540_hdmi *,unsigned expected);
#endif
