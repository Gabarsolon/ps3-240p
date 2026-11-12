/* SPDX-License-Identifier: MIT. Offline timing matrix and geometry invariants.
 *
 * Reads the shipped payload arrays directly (no console, no re-parsing of the
 * header text) and prints the decoded matrix, so a candidate can be judged
 * before anything is written to a console.
 *
 * Context (2026-11-12): the XMB is stretched and cropped in 240p. PROVEN
 * cause candidate: the patched raster emits 240 active lines while the
 * metadata record still advertises the stock framebuffer height (480 NTSC /
 * 576 PAL). The AV manager reads metadata and raster separately for
 * placement and scaling (docs/LV1_AV_TRACE.md).
 */
#include "raster_sim.h"
#include "timing_payloads.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void show(const char *label, const uint64_t *metadata, const uint64_t *raster)
{
  raster_view m, r;
  raster_decode_metadata(metadata, &m);
  raster_decode_raster(raster, &r);
  printf("  %-22s meta height=%-4u width=%-5u interlace=%u | raster active=%-4u total=%-4u "
         "vsync=%u..%u marker=%u scan=%08x%s\n",
         label, m.height, m.width, m.interlace, r.active_lines, r.total_lines,
         r.v_sync_start, r.v_sync_end, r.v_marker, r.scan_control,
         m.height == r.active_lines ? "" : "  <-- MISMATCH");
}

static void check_candidate(const char *label, const uint64_t *metadata, const uint64_t *raster,
                            const uint64_t *stock_metadata, const uint64_t *stock_raster)
{
  raster_view m, r, sm, sr;
  raster_decode_metadata(metadata, &m);
  raster_decode_raster(raster, &r);
  raster_decode_metadata(stock_metadata, &sm);
  raster_decode_raster(stock_raster, &sr);

  /* Progressive candidate: interlace cleared in metadata and raster. */
  assert(m.interlace == 0);
  assert(r.scan_control == 0x00010000u);
  assert(sm.interlace == 1);
  assert(sr.scan_control == 0x00030000u);

  /* Fewer active lines than the stock raster, same sample width. */
  assert(r.active_lines < sr.active_lines);
  assert(m.width == sm.width);
  assert(m.width == 2880u);

  /* Internally consistent vertical window. */
  assert(r.active_lines <= r.total_lines);
  assert(r.v_sync_start > r.v_active_end);
  assert(r.v_sync_end > r.v_sync_start);
  assert(r.v_sync_end <= r.total_lines);
  assert(r.v_marker >= r.v_active_end && r.v_marker <= r.total_lines);
  (void)label;
}

int main(void)
{
  printf("Decoded timing matrix (host, no console contact):\n");
  show("NTSC stock 480i", g_stock_240p_metadata, g_stock_240p_raster);
  show("NTSC candidate 240p", g_patch_240p_metadata, g_patch_240p_raster);
  show("PAL stock 576i", g_stock_288p_metadata, g_stock_288p_raster);
  show("PAL candidate PAL-60", g_patch_288p_metadata, g_patch_288p_raster);

  printf("Historical candidates (git 0367803, before 47eb61a and ad44c28):\n");
  printf("  %-22s meta height=%-4u raster active=%-4u total=%-4u\n", "NTSC 240p/262 lines", 240u, 240u, 262u);
  printf("  %-22s meta height=%-4u raster active=%-4u total=%-4u\n", "PAL 288p/312 lines", 288u, 288u, 312u);

  check_candidate("NTSC", g_patch_240p_metadata, g_patch_240p_raster,
                  g_stock_240p_metadata, g_stock_240p_raster);
  check_candidate("PAL", g_patch_288p_metadata, g_patch_288p_raster,
                  g_stock_288p_metadata, g_stock_288p_raster);

  /* The metadata-height intent must be declared and must match the shipped
   * arrays, so the known mismatch can never become accidental drift. */
  raster_view ntsc_patch, pal_patch, ntsc_stock, pal_stock;
  raster_decode_metadata(g_patch_240p_metadata, &ntsc_patch);
  raster_decode_metadata(g_patch_288p_metadata, &pal_patch);
  raster_decode_metadata(g_stock_240p_metadata, &ntsc_stock);
  raster_decode_metadata(g_stock_288p_metadata, &pal_stock);
  raster_view ntsc_raster, pal_raster;
  raster_decode_raster(g_patch_240p_raster, &ntsc_raster);
  raster_decode_raster(g_patch_288p_raster, &pal_raster);

  for (int family = 0; family < 2; family++) {
    uint32_t declared = g_meta_height_mode[family];
    uint32_t height = family ? pal_patch.height : ntsc_patch.height;
    uint32_t stock_height = family ? pal_stock.height : ntsc_stock.height;
    uint32_t active = family ? pal_raster.active_lines : ntsc_raster.active_lines;
    assert(declared == META_HEIGHT_STOCK || declared == META_HEIGHT_ACTIVE);
    if (declared == META_HEIGHT_ACTIVE) assert(height == active);
    else assert(height == stock_height);
    printf("  family %s: metadata height %u, raster active %u, declared mode %s\n",
           family ? "PAL" : "NTSC", height, active,
           declared == META_HEIGHT_ACTIVE ? "active" : "stock");
  }

  puts("PASS: decoded timing matrix printed; progressive candidates are internally consistent; "
       "metadata-height intent is declared and matches the shipped payload");
  return 0;
}
