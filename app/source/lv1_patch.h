#ifndef LV1_PATCH_H
#define LV1_PATCH_H

#include <stdint.h>
#include <stdbool.h>
#include <ppu-lv2.h>
#include "timing_payloads.h"

enum PatchMode {
    MODE_240P_NTSC = 0,
    MODE_288P_PAL  = 1
};

enum PatchState {
    STATE_UNKNOWN = -1,
    STATE_STOCK   = 0,
    STATE_PATCHED = 1
};

/*
 * Direct LV1 Peek / Poke through LV2 Syscalls
 * Syscall 8 = sys_peek_lv1(addr)
 * Syscall 9 = sys_poke_lv1(addr, val)
 */
static inline uint64_t lv1_peek_raw(uint64_t addr) {
    lv2syscall1(8, addr);
    return_to_user_prog(uint64_t);
}

static inline void lv1_poke_raw(uint64_t addr, uint64_t val) {
    lv2syscall2(9, addr, val);
}

/* PS3MAPI fallback opcodes */
static inline uint64_t mapi_peek_raw(uint64_t addr) {
    lv2syscall4(8, 0x7777, 0x1008, addr, 0);
    return_to_user_prog(uint64_t);
}

static inline void mapi_poke_raw(uint64_t addr, uint64_t val) {
    lv2syscall5(8, 0x7777, 0x1009, addr, val, 0);
}

/* Bounds and alignment check */
static inline bool is_safe_lv1_addr(uint64_t addr, size_t bytes) {
    if ((addr % 8) != 0) return false;
    if (addr + bytes > LV1_HEAP_LIMIT) return false;
    return true;
}

static inline uint64_t lv1_peek(uint64_t addr) {
    if (!is_safe_lv1_addr(addr, 8)) return 0;
    uint64_t val = lv1_peek_raw(addr);
    return val;
}

static inline bool lv1_poke(uint64_t addr, uint64_t val) {
    if (!is_safe_lv1_addr(addr, 8)) return false;
    lv1_poke_raw(addr, val);
    uint64_t readback = lv1_peek_raw(addr);
    if (readback == val) return true;

    /* Fallback to PS3MAPI if standard poke did not latch */
    mapi_poke_raw(addr, val);
    readback = lv1_peek_raw(addr);
    if (readback == val) return true;
    readback = mapi_peek_raw(addr);
    return (readback == val);
}

/* Inspect whether a timing profile is currently stock, patched, or unknown */
static inline int lv1_get_mode_state(enum PatchMode mode) {
    const uint64_t *stock_meta, *patch_meta;
    uint64_t meta_addr;

    if (mode == MODE_240P_NTSC) {
        meta_addr = ADDR_240P_METADATA;
        stock_meta = g_stock_240p_metadata;
        patch_meta = g_patch_240p_metadata;
    } else {
        meta_addr = ADDR_288P_METADATA;
        stock_meta = g_stock_288p_metadata;
        patch_meta = g_patch_288p_metadata;
    }

    uint64_t val0 = lv1_peek(meta_addr);
    if (val0 == stock_meta[0]) return STATE_STOCK;
    if (val0 == patch_meta[0]) return STATE_PATCHED;
    return STATE_UNKNOWN;
}

/* Apply 240p or 288p override to volatile RAM */
static inline bool lv1_apply_override(enum PatchMode mode) {
    uint64_t meta_addr, rast_addr, enc_addr;
    const uint64_t *patch_meta, *patch_rast, *patch_enc;

    if (mode == MODE_240P_NTSC) {
        meta_addr  = ADDR_240P_METADATA;
        rast_addr  = ADDR_240P_RASTER;
        enc_addr   = ADDR_240P_ENCODER;
        patch_meta = g_patch_240p_metadata;
        patch_rast = g_patch_240p_raster;
        patch_enc  = g_patch_240p_encoder;
    } else {
        meta_addr  = ADDR_288P_METADATA;
        rast_addr  = ADDR_288P_RASTER;
        enc_addr   = ADDR_288P_ENCODER;
        patch_meta = g_patch_288p_metadata;
        patch_rast = g_patch_288p_raster;
        patch_enc  = g_patch_288p_encoder;
    }

    /* 1. Poke Metadata */
    for (int i = 0; i < METADATA_WORDS; i++) {
        if (!lv1_poke(meta_addr + (i * 8), patch_meta[i])) return false;
    }

    /* 2. Poke Raster */
    for (int i = 0; i < RASTER_WORDS; i++) {
        if (!lv1_poke(rast_addr + (i * 8), patch_rast[i])) return false;
    }

    /* 3. Poke Encoder Register 0x37 */
    for (int i = 0; i < ENCODER_WORDS; i++) {
        if (!lv1_poke(enc_addr + (i * 8), patch_enc[i])) return false;
    }

    return true;
}

/* Restore official stock 480i or 576i timings to volatile RAM */
static inline bool lv1_restore_stock(enum PatchMode mode) {
    uint64_t meta_addr, rast_addr, enc_addr;
    const uint64_t *stock_meta, *stock_rast, *stock_enc;

    if (mode == MODE_240P_NTSC) {
        meta_addr  = ADDR_240P_METADATA;
        rast_addr  = ADDR_240P_RASTER;
        enc_addr   = ADDR_240P_ENCODER;
        stock_meta = g_stock_240p_metadata;
        stock_rast = g_stock_240p_raster;
        stock_enc  = g_stock_240p_encoder;
    } else {
        meta_addr  = ADDR_288P_METADATA;
        rast_addr  = ADDR_288P_RASTER;
        enc_addr   = ADDR_288P_ENCODER;
        stock_meta = g_stock_288p_metadata;
        stock_rast = g_stock_288p_raster;
        stock_enc  = g_stock_288p_encoder;
    }

    /* 1. Poke Metadata */
    for (int i = 0; i < METADATA_WORDS; i++) {
        if (!lv1_poke(meta_addr + (i * 8), stock_meta[i])) return false;
    }

    /* 2. Poke Raster */
    for (int i = 0; i < RASTER_WORDS; i++) {
        if (!lv1_poke(rast_addr + (i * 8), stock_rast[i])) return false;
    }

    /* 3. Poke Encoder */
    for (int i = 0; i < ENCODER_WORDS; i++) {
        if (!lv1_poke(enc_addr + (i * 8), stock_enc[i])) return false;
    }

    return true;
}

#endif /* LV1_PATCH_H */
