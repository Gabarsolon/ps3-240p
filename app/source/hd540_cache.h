/* SPDX-License-Identifier: MIT. Checked native cache/telemetry wrapper lifecycle. */
#ifndef HD540_CACHE_H
#define HD540_CACHE_H
#include "lv1_patch.h"
#include "hd540_cache_payload.h"
#define HD540_CACHE_WORDS (6u+HD540_CACHE_QWORDS+6u)
#define HD540_CACHE_TRACE (6u+HD540_CACHE_QWORDS)
typedef struct {
 patch_context *transport;
 uint64_t address[HD540_CACHE_WORDS],original[HD540_CACHE_WORDS];
 int ready,dirty,attached;
 char error[160];
} hd540_cache;
int hd540_cache_identity(const uint64_t [2]);
int hd540_cache_resolve(hd540_cache *,patch_context *);
int hd540_cache_apply(hd540_cache *);
/* Restore partial staging, but keep an attached cache hook until stock RPCs
 * have refreshed the ORIGINAL instructions. No functional edits are retained. */
int hd540_cache_restore(hd540_cache *);
/* Call only after enabled native stock state / cached stock raster / AVI 5. */
int hd540_cache_finish(hd540_cache *);
int hd540_cache_snapshot(hd540_cache *,uint32_t [10]);
#endif
