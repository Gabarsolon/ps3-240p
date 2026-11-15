/* SPDX-License-Identifier: MIT. Checked native cache/telemetry wrapper lifecycle. */
#ifndef HD540_CACHE_H
#define HD540_CACHE_H
#include "lv1_patch.h"
/* CRT_DIAG_HD_WRAPPER: diagnostic custom build using the dev19 540p wrapper. */
#if defined(CRT_CUSTOM_HDMI) && !defined(CRT_DIAG_HD_WRAPPER)
#include "../../experiments/hdmi_custom/cache_payload.h"
#define HD540_CACHE_SAMPLES 24u
#define HD540_CACHE_TRACE_WORDS 13u
#define HD540_CACHE_EXTRA_WORDS 1u
#elif defined(CRT_XGA_PROBE) && !defined(CRT_DIAG_HD_WRAPPER)
#include "../../experiments/display_probe/xga_cache_payload.h"
#define HD540_CACHE_SAMPLES 22u
#define HD540_CACHE_TRACE_WORDS 12u
#define HD540_CACHE_EXTRA_WORDS 1u
#else
#include "hd540_cache_payload.h"
#define HD540_CACHE_SAMPLES 10u
#define HD540_CACHE_TRACE_WORDS 6u
#define HD540_CACHE_EXTRA_WORDS 0u
#endif
#define HD540_CACHE_WORDS (6u+HD540_CACHE_QWORDS+HD540_CACHE_TRACE_WORDS+HD540_CACHE_EXTRA_WORDS)
#define HD540_CACHE_TRACE (6u+HD540_CACHE_QWORDS)
typedef struct {
 patch_context *transport;
 uint64_t address[HD540_CACHE_WORDS],original[HD540_CACHE_WORDS];
 int ready,dirty,attached,normalize_source;
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
int hd540_cache_snapshot(hd540_cache *,uint32_t [HD540_CACHE_SAMPLES]);
#endif
