/* SPDX-License-Identifier: MIT. CFW-only, reversible WXGA active-width experiment. */
#ifndef XGA_PATCH_H
#define XGA_PATCH_H
#include "../../app/source/lv1_patch.h"
#define XGA_WORDS 20u
typedef struct {
 patch_context *transport;
 uint64_t address[XGA_WORDS],original[XGA_WORDS],candidate[XGA_WORDS];
 unsigned geometry;
 int ready,dirty;
 char error[160];
} xga_patch;
int xga_patch_resolve(xga_patch *,patch_context *,unsigned);
int xga_patch_apply(xga_patch *);
int xga_patch_restore(xga_patch *);
int xga_patch_cache(xga_patch *,int);
#endif
