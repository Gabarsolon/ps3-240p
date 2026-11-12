/* SPDX-License-Identifier: MIT. Decoder for the LV1 timing records.
 *
 * Each 64-bit payload word holds two big-endian 32-bit fields, so the
 * metadata record is 6 x 32-bit words and the raster record is 14 x 32-bit
 * words (docs/FEASIBILITY_240P_RSX_ENCODER.md, docs/LV1_AV_TRACE.md).
 * This decoder exists so the host tests and the offline matrix tool read the
 * shipped payload arrays directly instead of re-parsing the header text.
 */
#ifndef RASTER_SIM_H
#define RASTER_SIM_H
#include <stdint.h>

typedef struct {
  uint32_t refresh_bits, interlace, multiplier, width, height, family;
  uint32_t h_active_end, h_total, h_sync_start, h_sync_end;
  uint32_t v_active_end, v_total, v_marker, v_sync_start, v_sync_end;
  uint32_t window_end, window_start, scan_control, output_control;
  uint32_t active_lines, total_lines;
} raster_view;

void raster_decode_metadata(const uint64_t *metadata, raster_view *view);
void raster_decode_raster(const uint64_t *raster, raster_view *view);

#endif
