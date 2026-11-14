/* SPDX-License-Identifier: MIT. Hypothesis only: docs/HD_CRT_540P.md. */
#ifndef HD540_PATCH_H
#define HD540_PATCH_H
#include "lv1_patch.h"
#define HD540_WORDS 20
typedef struct {
 patch_context *transport;
 uint64_t address[HD540_WORDS],original[HD540_WORDS],candidate[HD540_WORDS];
 int ready,dirty;
 char error[160];
} hd540_patch;
int hd540_patch_resolve(hd540_patch *,patch_context *);
int hd540_patch_apply(hd540_patch *);
int hd540_patch_restore(hd540_patch *);
int hd540_patch_cache(hd540_patch *,int candidate);
/* Hash numeric metadata/raster fields in service byte order, not host order. */
uint64_t hd540_record_hash(const uint64_t [10]);
int hd540_record_valid(unsigned,const uint64_t [10]);
#endif
