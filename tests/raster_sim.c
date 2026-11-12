/* SPDX-License-Identifier: MIT. Timing record decoder (host side). */
#include "raster_sim.h"
#include "timing_payloads.h"
#include <string.h>

static uint32_t hi(uint64_t word) { return (uint32_t)(word >> 32); }
static uint32_t lo(uint64_t word) { return (uint32_t)(word & UINT64_C(0xffffffff)); }

void raster_decode_metadata(const uint64_t *metadata, raster_view *view)
{
  memset(view, 0, sizeof(*view));
  view->refresh_bits = hi(metadata[0]);
  view->interlace = lo(metadata[0]);
  view->multiplier = hi(metadata[1]);
  view->width = lo(metadata[1]);
  view->height = hi(metadata[2]);
  view->family = lo(metadata[2]);
}

void raster_decode_raster(const uint64_t *raster, raster_view *view)
{
  view->h_active_end = hi(raster[0]);
  view->h_total = lo(raster[0]) + 1u;
  view->h_sync_start = hi(raster[1]);
  view->h_sync_end = lo(raster[1]);
  view->v_active_end = hi(raster[2]);
  view->v_total = lo(raster[2]) + 1u;
  view->v_marker = hi(raster[3]);
  view->v_sync_start = lo(raster[3]);
  view->v_sync_end = hi(raster[4]);
  view->window_end = hi(raster[5]);
  view->window_start = lo(raster[5]);
  view->scan_control = hi(raster[6]);
  view->output_control = lo(raster[6]);
  view->active_lines = view->v_active_end + 1u;
  view->total_lines = view->v_total;
}
