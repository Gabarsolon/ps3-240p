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
    0x00000b3f00000d67ULL,
    0x00000b7f00000c77ULL,
    0x000000ef00000105ULL,
    0x000000f4000000f4ULL,
    0x000000f7054704cbULL,
    0x04cb03d300f700f4ULL,
    0x0001000011000000ULL
};

static const uint64_t g_stock_240p_encoder[ENCODER_WORDS] = {
    0x378038003a003b00ULL
};

static const uint64_t g_patch_240p_encoder[ENCODER_WORDS] = {
    0x370038003a003b00ULL
};

/* -------------------------------------------------------------------------
 * PAL 288p (720x288 @ 50.00 Hz, 15.625 kHz scan)
 * ------------------------------------------------------------------------- */
static const uint64_t g_stock_288p_metadata[METADATA_WORDS] = {
    0x4248000000000001ULL,
    0x0000000400000b40ULL,
    0x0000024000000001ULL
};

static const uint64_t g_patch_288p_metadata[METADATA_WORDS] = {
    0x4248000000000000ULL,
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
    0x0000011f00000137ULL,
    0x0000012300000122ULL,
    0x00000125052d04afULL,
    0x04af03b301250122ULL,
    0x0001000011000000ULL
};

static const uint64_t g_stock_288p_encoder[ENCODER_WORDS] = {
    0x3500360037803800ULL
};

static const uint64_t g_patch_288p_encoder[ENCODER_WORDS] = {
    0x3500360037003800ULL
};

#endif /* TIMING_PAYLOADS_H */
