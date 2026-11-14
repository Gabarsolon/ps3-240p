/* Experimental RAM-only timing editor. No console I/O in this module. */
#ifndef LV1_PATCH_H
#define LV1_PATCH_H
#include <stdint.h>
#include <stddef.h>

enum PatchMode { MODE_240P_NTSC, MODE_288P_PAL, MODE_ALL_SD };
enum PatchState { STATE_UNKNOWN=-1, STATE_STOCK, STATE_PATCHED, STATE_MIXED };
#define PATCH_WORDS 54
#define PAL_SPECIAL_META_INDEX 44
#define PAL_SPECIAL_RASTER_INDEX 47
#define VIDEO_PROBE_WORDS 27
#define METADATA_PROBE_PROFILES 4
typedef struct {
 uint32_t state,vid,width,height,pitch,active_lines,total_lines,scan_control;
 uint64_t raster[7];
} patch_cached_head;
typedef struct {patch_cached_head head[2];char error[128];} patch_cached_video;
typedef uint64_t (*patch_read_fn)(void *,uint64_t);
typedef void (*patch_write_fn)(void *,uint64_t,uint64_t);
typedef void (*patch_log_fn)(const char *,...);
typedef struct {
 patch_read_fn read; patch_write_fn write; void *user; patch_log_fn log;
 uint64_t address[PATCH_WORDS],stock[PATCH_WORDS],candidate[PATCH_WORDS];
 uint64_t saved[PATCH_WORDS];
 uint64_t video_address[VIDEO_PROBE_WORDS];
 uint32_t video_context;
 unsigned family[PATCH_WORDS];
 int ready,have_saved,video_ready;
 /* 0 = metadata keeps the stock framebuffer height (current behaviour),
  * 1 = metadata publishes the patched active height (240). Single-variable
  * experiment for the stretched/cropped XMB; see timing_payloads.h. */
 int meta_height_mode;
 /* 1 = request the experimental scaler code edits. This flag does not
  * establish that the edits executed or that hardware scaling is active. */
 int scaler_mode;
 /* Metadata records for profiles 1..4, captured after successful resolve.
  * These are table RAM, not live scaler registers or per-head descriptors. */
 uint64_t metadata_probe[METADATA_PROBE_PROFILES][3];
 char error[160];
} patch_context;
int patch_resolve(patch_context *);
/* Resolve ordinary service RAM through the current protection-page list.
 * Read-only discovery; callers must validate their own complete record identity. */
int patch_map_targets(patch_context *,const uint32_t *,uint64_t *,unsigned);
int patch_state(patch_context *,int family);
int patch_snapshot(patch_context *);
int patch_apply(patch_context *,int family); /* Also restores the other family. */
int patch_restore(patch_context *);
int patch_undo(patch_context *); /* Restore exact pre-experiment snapshot. */
int patch_read_cached_video(patch_context *,patch_cached_video *);
/* AV-service software cache match only; not a physical waveform measurement. */
int patch_candidate_in_cache(const patch_context *,const patch_cached_video *,int family);
int patch_candidate_on_head(const patch_context *,const patch_cached_video *,int family,int head);
#endif
