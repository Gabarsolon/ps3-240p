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
 * Experimental scaler code edits; not a verified geometry fix.
 *
 * PROVEN (private 4.93 sysmgr_ss.elf): the verified words at the two VAs
 * below feed display control +0x880 and the constant written to +0x89c in
 * the placement routine at 0x80002ca4. RAM readback is not execution proof.
 *
 * CLAIMED for Nouveau's hardware path (Linux dispnv04/nvreg.h): +0x880
 * bit 0x10 is YSCALE_ENABLE; bit 0x1000 is YINTERP_BILINEAR. Source:
 * https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/nouveau/dispnv04/nvreg.h
 * Thus the previous experiment cleared a filtering bit, not a proven
 * "vertical exact fit" gate. Its 0.5 factor had no visible effect (owner
 * report, OpenCode session ses_e30be46b5ffeJVtR41gZRa0Q2h, last message).
 *
 * INFERRED candidate for RSX: preserve filtering, set bit 0x10, and use
 * a 2.0 source-line step for 480 -> 240. Nouveau's dfp.c uses source /
 * output ratios for scaling; the RSX +0x89c format/direction and analog
 * path remain UNKNOWN until measured. This is not a PAL 576 -> 240 ratio.
 *
 * The former ca85d2d candidate used these same values. Its historical
 * result does not prove these values wrong; execution/cache coherency,
 * scanout windows and actual register state were not established.
 *
 * The flag gates the experiment. A different live instruction is refused.
 * Reboot before testing after a different scaler experiment.
 * ------------------------------------------------------------------------- */
#define VSCALE_ENABLE_VA   0x80002f48u
#define STOCK_VSCALE_ENABLE_WORD UINT64_C(0x3ca0011b60031000) /* lis r5,0x11b ; ori r3,r0,0x1000 */
#define PATCH_VSCALE_ENABLE_WORD UINT64_C(0x3ca0011b60031010)
#define VSCALE_FACTOR_VA   0x80003100u
#define STOCK_VSCALE_FACTOR_WORD UINT64_C(0xeb21ffc83c001000) /* ld r25,-0x38(r1) ; lis r0,0x1000 */
#define PATCH_VSCALE_FACTOR_WORD UINT64_C(0xeb21ffc83c002000)
#define VSCALE_FACTOR_STEP UINT64_C(0x20000000) /* Hypothesis: 2 source lines per output line. */

/* The earlier horizontal target is kept for provenance:
 * 0x80002e10 is the *horizontal* scale numerator on a path that the SD
 * modes never take. */
#define HSCALE_FACTOR_VA        0x80002e10u
#define STOCK_HSCALE_FACTOR_WORD UINT64_C(0x600800043c001000) /* ori r8,r0,0x4 ; lis r0,0x1000 */

/* PROVEN geometry recipe: docs/GEOMETRY_STATUS_2026-11-13.md, matched
 * field-timing experiment. Keep full 480-line source fetching, use an even
 * 522-line frame (261 lines/field), and retain the successful field-sized
 * packed window. Owner reported a full, correct picture. Electrical field
 * parity remains unmeasured. PAL below applies the same field-fetch method
 * to 576 source lines / 624 frame lines; its picture is not yet verified.
 * Source-interlace metadata here controls fetching; it is not a waveform
 * classification. Do not halve active height while retaining field fetch.
 */
/* -------------------------------------------------------------------------
 * NTSC 240p (720x240 @ 59.94 Hz, 15.734 kHz scan)
 * ------------------------------------------------------------------------- */
static const uint64_t g_stock_240p_metadata[METADATA_WORDS] = {
    0x426fc28f00000001ULL,
    0x0000000400000b40ULL,
    0x000001e000000000ULL
};

static const uint64_t g_patch_240p_metadata[METADATA_WORDS] = {
    0x426fc29f00000001ULL,
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
    0x000001df00000209ULL,
    0x000001e9000001e8ULL,
    0x000001ee055404d5ULL,
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
 * PAL 288p candidate (576 source / 624 frame lines; 312 lines per field)
 * ------------------------------------------------------------------------- */
static const uint64_t g_stock_288p_metadata[METADATA_WORDS] = {
    0x4248000000000001ULL,
    0x0000000400000b40ULL,
    0x0000024000000001ULL
};

static const uint64_t g_patch_288p_metadata[METADATA_WORDS] = {
    0x4248000000000001ULL,
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
    0x0000023f0000026fULL,
    0x0000024500000244ULL,
    0x0000024a052d04afULL,
    0x04af03b301250122ULL,
    0x0001000011000000ULL
};

static const uint64_t g_stock_288p_encoder[ENCODER_WORDS] = {
    0x3500360037803800ULL
};

static const uint64_t g_patch_288p_encoder[ENCODER_WORDS] = {
    0x3500360037003800ULL
};

/* PROVEN: head 1 / internal profile 4 bypasses the ordinary PAL arrays.
 * Private 4.93 sysmgr setup 0x800005e4 selects these pointer-backed records
 * at 0xc000af68/0xc000af70 (VAs 0x80038ad8/0x80038b10). Live capture:
 * local_only/activation/pal288-development/special-path/pointers.json.
 * Their stock active height is 574. Candidate uses the full 576 source
 * recipe above; no code edits or MMIO writes are needed. */
static const uint64_t g_stock_pal_special_metadata[METADATA_WORDS] = {
    0x4248000000000001ULL, 0x0000000400000b40ULL, 0x0000023e00000001ULL
};
static const uint64_t g_stock_pal_special_raster[RASTER_WORDS] = {
    0x00000b3f00000d7fULL, 0x00000b6f00000c6bULL,
    0x0000023d00000270ULL, 0x0000024500000244ULL,
    0x0000024a052d04afULL, 0x04af03b3024a0244ULL,
    0x0003000011000000ULL
};

/* -------------------------------------------------------------------------
 * Declared metadata-height intent, per candidate family (NTSC, PAL).
 *
 * PROVEN from the arrays above: NTSC now retains 480 source lines with
 * a 522-line field-fetch frame; PAL retains 576 source lines and uses a
 * 624-line frame. The earlier metadata-height A/B was
 * reported visually unchanged by the owner in the recovered OpenCode
 * session; this mismatch alone is not proof of the crop's cause.
 *
 * META_HEIGHT_STOCK retains the source framebuffer height.
 * META_HEIGHT_ACTIVE is the earlier, optional A/B candidate.
 * Declaring it here keeps the mismatch from ever being accidental drift.
 * ------------------------------------------------------------------------- */
#define META_HEIGHT_STOCK  0
#define META_HEIGHT_ACTIVE 1
static const int g_meta_height_mode[2] = { META_HEIGHT_STOCK, META_HEIGHT_STOCK };

#endif /* TIMING_PAYLOADS_H */
