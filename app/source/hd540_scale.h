/* SPDX-License-Identifier: MIT. Temporary audited RAM source-window experiment. */
#ifndef HD540_SCALE_H
#define HD540_SCALE_H
#include "lv1_patch.h"
#define HD540_SCALE_WORDS 8
typedef struct {
 patch_context *transport;
 uint64_t address[HD540_SCALE_WORDS],original[HD540_SCALE_WORDS],candidate[HD540_SCALE_WORDS];
 int ready,dirty;
 char error[160];
} hd540_scale;
int hd540_scale_identity(const uint64_t [HD540_SCALE_WORDS]);
int hd540_scale_resolve(hd540_scale *,patch_context *);
int hd540_scale_apply(hd540_scale *);
int hd540_scale_restore(hd540_scale *);
int hd540_scale_check(hd540_scale *,int);
#endif
