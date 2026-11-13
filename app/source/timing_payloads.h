#ifndef TIMING_PAYLOADS_H
#define TIMING_PAYLOADS_H

#include <stdint.h>

/*
 * PlayStation 3 Analog MultiAV Timing Payloads
 *
 * Target: sysmgr_ss LV1 Volatile DRAM records in Evilnat 4.93 CFW + Cobra
 *
 * Historical physical addresses below are reference observations only.
 * lv1_patch.c resolves every target through the live protection-page list.
 * These are experimental timing candidates, not verified analog waveforms.
 */

#define LV1_HEAP_BASE         0x00000000ULL
#define LV1_HEAP_LIMIT        0x02000000ULL

/* Record 1 (480i stock / 240p patched) */
#define ADDR_240P_METADATA    0x00144160ULL
#define ADDR_240P_RASTER      0x00143B60ULL
#define ADDR_240P_ENCODER     0x00793010ULL

/* Record 4 (576i stock / 288p patched) */
#define ADDR_288P_METADATA    0x001441A8ULL
#define ADDR_288P_RASTER      0x00143C08ULL
#define ADDR_288P_ENCODER     0x00793050ULL

/* Record word counts (all 64-bit uint64_t words) */
#define METADATA_WORDS        3  /* 24 bytes */
#define RASTER_WORDS          7  /* 56 bytes */
#define ENCODER_WORDS         1  /*  8 bytes */

/* -------------------------------------------------------------------------
 * RSX CRTC 2.0 vertical scale factor (4.93 sysmgr_ss code patch)
 *
 * PROVEN from local_only/verification/sysmgr493_80000000_80040000.asm and
 * from the matching 4.93 sysmgr_ss.elf bytes:
 * FUN_80002ca4 is the AV manager's display placement/scaling routine. It
 * writes the vertical scale factor to CRTC 0x68898 in 4.28 fixed point
 * (0x10000000 = 1.0 = one source line per output line):
 *
 *   exact-fit path (source height == raster active height):
 *     0x80002e04  mr   r8,r0
 *     0x80002e08  lis  r25,0x1000      -> r25 = 0x10000000 (1.0)
 *   mismatch path (source height != raster active height, i.e. exactly the
 *   240p case, where a 480/576-line source is scanned into a 240-line
 *   raster):
 *     0x80002e10  ori  r8,r0,0x4       (request bit 2 = "scale needed")
 *     0x80002e14  lis  r0,0x1000       -> r0  = 0x10000000
 *     0x80002e18  divwu r25,r0,r10     -> r25 = 0x10000000 / scale
 *
 * The firmware's per-mode "scale" field is 1 for a 1:1 source/raster pair,
 * so the mismatch path also produces 1.0: the CRTC reads the source 1:1 and
 * only the first raster lines are scanned. That is the cropped, stretched
 * picture: the top half of the image, stretched over the whole screen.
 *
 * The experiment changes the numerator of that division to 0x08000000
 * (0.5 = two source lines per output line) so the whole source is reduced
 * into the raster: 480 source lines fill a 240-line raster.
 *
 * Only the mismatch path is patched, so the stock 480i/576i modes (which
 * fit exactly and take the other path) are unaffected.
 *
 * The 8-byte word is verified at run time before any store, so a live
 * image that differs from the 4.93 build is refused instead of patched.
 *
 * The earlier attempt (commit ca85d2d) patched the request word 0x68880
 * bit 0x10 and a constant at 0x80003100. Both were wrong: 0x68880 bit 0x10
 * is the interlace flag (the interlaced path sets it, the progressive path
 * clears it), and 0x80003104 is a constant 1.0 written to CRTC 0x68289c,
 * not a scale numerator.
 * ------------------------------------------------------------------------- */
#define SCALE_FACTOR_VA        0x80002e10u
#define STOCK_SCALE_FACTOR_WORD UINT64_C(0x600800043c001000) /* ori r8,r0,0x4 ; lis r0,0x1000 */
#define PATCH_SCALE_FACTOR_WORD UINT64_C(0x600800043c000800) /* ori r8,r0,0x4 ; lis r0,0x0800 */
#define SCALE_FACTOR_PATCH_STEP UINT64_C(0x08000000)         /* 0.5 in 4.28 fixed point */

/* -------------------------------------------------------------------------
 * NTSC 240p (720x240 @ 59.94 Hz, 15.734 kHz scan)
 * ------------------------------------------------------------------------- */
static const uint64_t g_stock_240p_metadata[METADATA_WORDS] = {
    0x426fc28f00000001ULL,
    0x0000000400000b40ULL,
    0x000001e000000000ULL
};

static const uint64_t g_patch_240p_metadata[METADATA_WORDS] = {
    0x426fc29f00000000ULL,
    0x0000000400000b40ULL,
    0x000001e000000000ULL
};

static const uint64_t g_stock_240p_raster[RASTER_WORDS] = {
    0x00000b3f00000d67ULL,
    0x00000b7f00000c77ULL,
    0x000001df0000020cULL,
    0x000001e9000001e8ULL,
    0x000001ee054704cbULL,
    0x04cb03d301f401e2ULL,
    0x0003000011000000ULL
};

static const uint64_t g_patch_240p_raster[RASTER_WORDS] = {
    0x00000b3f00000d77ULL,
    0x00000b9100000c8fULL,
    0x000000ef00000104ULL,
    0x000000f3000000f2ULL,
    0x000000f5055404d5ULL,
    0x04d503d700f500f2ULL,
    0x0001000011000000ULL
};

static const uint64_t g_stock_240p_encoder[ENCODER_WORDS] = {
    0x378038003a003b00ULL
};

static const uint64_t g_patch_240p_encoder[ENCODER_WORDS] = {
    0x370038003a003b00ULL
};

/* -------------------------------------------------------------------------
 * PAL-60 240p (720x240 @ 60.10 Hz, 15.625 kHz scan - Quiet European CRT)
 * ------------------------------------------------------------------------- */
static const uint64_t g_stock_288p_metadata[METADATA_WORDS] = {
    0x4248000000000001ULL,
    0x0000000400000b40ULL,
    0x0000024000000001ULL
};

static const uint64_t g_patch_288p_metadata[METADATA_WORDS] = {
    0x4270624d00000000ULL,
    0x0000000400000b40ULL,
    0x0000024000000001ULL
};

static const uint64_t g_stock_288p_raster[RASTER_WORDS] = {
    0x00000b3f00000d7fULL,
    0x00000b6f00000c6bULL,
    0x0000023f00000270ULL,
    0x0000024500000244ULL,
    0x0000024a052d04afULL,
    0x04af03b3024a0244ULL,
    0x0003000011000000ULL
};

static const uint64_t g_patch_288p_raster[RASTER_WORDS] = {
    0x00000b3f00000d7fULL,
    0x00000b6f00000c6bULL,
    0x000000ef00000103ULL,
    0x000000f4000000f3ULL,
    0x000000f6052d04afULL,
    0x04af03b300f600f3ULL,
    0x0001000011000000ULL
};

static const uint64_t g_stock_288p_encoder[ENCODER_WORDS] = {
    0x3500360037803800ULL
};

static const uint64_t g_patch_288p_encoder[ENCODER_WORDS] = {
    0x3500360037003800ULL
};

/* -------------------------------------------------------------------------
 * Declared metadata-height intent, per candidate family (NTSC, PAL).
 *
 * PROVEN mismatch (app/source/timing_payloads.h below): the patched raster
 * emits 240 active lines while the metadata record still advertises the
 * stock framebuffer height (480 NTSC / 576 PAL). The AV manager reads
 * metadata and raster separately for placement and scaling
 * (docs/LV1_AV_TRACE.md), so the XMB can still be laid out as a 480-line
 * frame while only 240 lines are active - the reported stretched and cropped
 * picture. Commit 47eb61a changed the metadata heights from 240/288 to
 * 480/576 without recording the reason.
 *
 * META_HEIGHT_STOCK is today's measured behaviour (mismatch kept).
 * META_HEIGHT_ACTIVE publishes the patched active height in the metadata
 * record and is the next single-variable console candidate.
 * Declaring it here keeps the mismatch from ever being accidental drift.
 * ------------------------------------------------------------------------- */
#define META_HEIGHT_STOCK  0
#define META_HEIGHT_ACTIVE 1
static const int g_meta_height_mode[2] = { META_HEIGHT_STOCK, META_HEIGHT_STOCK };

#endif /* TIMING_PAYLOADS_H */
