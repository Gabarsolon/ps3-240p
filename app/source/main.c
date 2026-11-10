/*
 * PlayStation 3 Native 240p / 288p Switcher
 *
 * Standalone CFW / Cobra Homebrew Application
 * Overrides LV1 sysmgr_ss video timing tables in volatile RAM
 * and immediately triggers native 15 kHz progressive analog video.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <ppu-lv2.h>
#include <ppu-types.h>
#include <sys/process.h>
#include <sysutil/video.h>
#include <cell/gcm.h>
#include <cell/dbgfont.h>
#include <rsx/rsx.h>
#include <io/pad.h>

#include "timing_payloads.h"
#include "lv1_patch.h"

SYS_PROCESS_PARAM(1001, 0x100000);

#define CB_SIZE         0x100000
#define HOST_SIZE       (32 * 1024 * 1024)
#define MAX_BUFFERS     2

typedef struct {
    uint32_t *ptr;
    uint32_t  offset;
    uint16_t  width;
    uint16_t  height;
    int       id;
} display_buffer;

static uint32_t  depth_pitch;
static uint32_t  depth_offset;
static uint32_t *depth_buffer;

__attribute__((unused)) static void wait_flip(void) {
    while (cellGcmGetFlipStatus() != 0) usleep(200);
    cellGcmResetFlipStatus();
}

static int do_flip(CellGcmContextData *ctx, uint8_t buffer_id) {
    if (cellGcmSetFlip(ctx, buffer_id) == 0) {
        cellGcmFlush(ctx);
        cellGcmSetWaitFlip(ctx);
        return 1;
    }
    return 0;
}

static int make_buffer(display_buffer *b, uint16_t w, uint16_t h, int id) {
    int pitch = (int)(w * sizeof(uint32_t));
    b->ptr = (uint32_t *)rsxMemalign(64, pitch * h);
    if (!b->ptr) return 0;
    if (cellGcmAddressToOffset(b->ptr, &b->offset) != 0) return 0;
    if (cellGcmSetDisplayBuffer((uint8_t)id, b->offset, pitch, w, h) != 0) return 0;
    b->width = w;
    b->height = h;
    b->id = id;
    return 1;
}

static void set_render_target(CellGcmContextData *ctx, display_buffer *b) {
    CellGcmSurface sf = {0};
    sf.colorFormat      = GCM_SURFACE_X8R8G8B8;
    sf.colorTarget      = GCM_SURFACE_TARGET_0;
    sf.colorLocation[0] = GCM_LOCATION_RSX;
    sf.colorOffset[0]   = b->offset;
    sf.colorPitch[0]    = depth_pitch;
    sf.colorLocation[1] = GCM_LOCATION_RSX;
    sf.colorLocation[2] = GCM_LOCATION_RSX;
    sf.colorLocation[3] = GCM_LOCATION_RSX;
    sf.colorPitch[1]    = 64;
    sf.colorPitch[2]    = 64;
    sf.colorPitch[3]    = 64;
    sf.depthFormat      = GCM_SURFACE_ZETA_Z16;
    sf.depthLocation    = GCM_LOCATION_RSX;
    sf.depthOffset      = depth_offset;
    sf.depthPitch       = depth_pitch;
    sf.type             = GCM_SURFACE_TYPE_LINEAR;
    sf.antiAlias        = GCM_SURFACE_CENTER_1;
    sf.width            = b->width;
    sf.height           = b->height;
    sf.x = 0; sf.y = 0;
    cellGcmSetSurface(ctx, &sf);

    float scale_vp[4]  = {  b->width / 2.0f, -(float)b->height / 2.0f, 0.5f, 0.0f };
    float offset_vp[4] = {  b->width / 2.0f,   b->height / 2.0f,      0.5f, 0.0f };
    cellGcmSetViewport(ctx, 0, 0, b->width, b->height, 0.0f, 1.0f, scale_vp, offset_vp);
    cellGcmSetScissor(ctx, 0, 0, b->width, b->height);
}

int main(void) {
    void *host_addr = memalign(1024 * 1024, HOST_SIZE);
    if (!host_addr) return 1;

    videoState state;
    videoConfiguration vcfg;
    videoResolution res;

    if (cellGcmInit(CB_SIZE, HOST_SIZE, host_addr) != 0) return 1;
    if (videoGetState(0, 0, &state) != 0 || state.state != 0) return 1;
    if (videoGetResolution(state.displayMode.resolution, &res) != 0) return 1;

    /* Base display resolution setup */
    memset(&vcfg, 0, sizeof(vcfg));
    vcfg.resolution = state.displayMode.resolution;
    vcfg.format = VIDEO_BUFFER_FORMAT_XRGB;
    vcfg.pitch  = res.width * sizeof(uint32_t);
    vcfg.aspect = state.displayMode.aspect;
    if (videoConfigure(0, &vcfg, NULL, 0) != 0) return 1;
    cellGcmSetFlipMode(GCM_FLIP_VSYNC);

    depth_pitch  = res.width * sizeof(uint32_t);
    depth_buffer = (uint32_t *)rsxMemalign(64, res.height * depth_pitch * 2);
    cellGcmAddressToOffset(depth_buffer, &depth_offset);
    cellGcmResetFlipStatus();

    CellGcmContextData *ctx = CELL_GCM_CURRENT;
    display_buffer bufs[MAX_BUFFERS];
    memset(bufs, 0, sizeof(bufs));
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (!make_buffer(&bufs[i], res.width, res.height, i)) return 1;
    }
    int cur = 0;

    /* Initialize dbgfont */
    size_t dbg_buf_size = CELL_DBGFONT_FRAGMENT_SIZE
                        + CELL_DBGFONT_TEXTURE_SIZE
                        + 12000 * CELL_DBGFONT_VERTEX_SIZE;
    void *dbg_buf = rsxMemalign(128, dbg_buf_size);
    CellDbgFontConfigGcm font_cfg = {0};
    font_cfg.localBufAddr = (sys_addr_t)(uintptr_t)dbg_buf;
    font_cfg.localBufSize = (uint32_t)dbg_buf_size;
    font_cfg.option = CELL_DBGFONT_VERTEX_LOCAL | CELL_DBGFONT_TEXTURE_LOCAL;
    if (cellDbgFontInitGcm(&font_cfg) != 0) return 1;

    /* Initialize Pad Controller input */
    const int pad_ok = (ioPadInit(7) == 0);

    char status_msg[128] = "System ready. Select a mode to switch.";
    uint32_t status_color = 0xff00ff00; /* Bright Green */

    int prev_cross = 0, prev_square = 0, prev_circle = 0, prev_triangle = 0;
    int exit_app = 0;

    while (!exit_app) {
        int btn_cross = 0, btn_square = 0, btn_circle = 0, btn_triangle = 0;

        if (pad_ok) {
            padInfo pi = {0};
            if (ioPadGetInfo(&pi) == 0) {
                for (int i = 0; i < MAX_PADS; i++) {
                    if (!pi.status[i]) continue;
                    padData pd = {0};
                    ioPadGetData(i, &pd);
                    if (pd.BTN_CROSS)    btn_cross = 1;
                    if (pd.BTN_SQUARE)   btn_square = 1;
                    if (pd.BTN_CIRCLE)   btn_circle = 1;
                    if (pd.BTN_TRIANGLE) btn_triangle = 1;
                }
            }
        }

        /* Detect single-press rising edges */
        int press_cross    = (btn_cross && !prev_cross);
        int press_square   = (btn_square && !prev_square);
        int press_circle   = (btn_circle && !prev_circle);
        int press_triangle = (btn_triangle && !prev_triangle);

        prev_cross    = btn_cross;
        prev_square   = btn_square;
        prev_circle   = btn_circle;
        prev_triangle = btn_triangle;

        /* Action: Apply NTSC 240p */
        if (press_cross) {
            bool ok = lv1_apply_override(MODE_240P_NTSC);
            if (ok) {
                memset(&vcfg, 0, sizeof(vcfg));
                vcfg.resolution = VIDEO_RESOLUTION_480;
                vcfg.format = VIDEO_BUFFER_FORMAT_XRGB;
                vcfg.aspect = VIDEO_ASPECT_4_3;
                vcfg.pitch  = res.width * sizeof(uint32_t);
                videoConfigure(0, &vcfg, NULL, 0);
                snprintf(status_msg, sizeof(status_msg), "240p ACTIVE: 720x240 @ 59.94 Hz (15.734 kHz scan)");
                status_color = 0xff00ffff; /* Cyan */
            } else {
                snprintf(status_msg, sizeof(status_msg), "ERROR: Failed to apply 240p override to LV1 RAM!");
                status_color = 0xffff4444; /* Red */
            }
        }

        /* Action: Apply PAL 288p */
        if (press_square) {
            bool ok = lv1_apply_override(MODE_288P_PAL);
            if (ok) {
                memset(&vcfg, 0, sizeof(vcfg));
                vcfg.resolution = VIDEO_RESOLUTION_576;
                vcfg.format = VIDEO_BUFFER_FORMAT_XRGB;
                vcfg.aspect = VIDEO_ASPECT_4_3;
                vcfg.pitch  = res.width * sizeof(uint32_t);
                videoConfigure(0, &vcfg, NULL, 0);
                snprintf(status_msg, sizeof(status_msg), "288p ACTIVE: 720x288 @ 50.00 Hz (15.625 kHz scan)");
                status_color = 0xff00ffff; /* Cyan */
            } else {
                snprintf(status_msg, sizeof(status_msg), "ERROR: Failed to apply 288p override to LV1 RAM!");
                status_color = 0xffff4444; /* Red */
            }
        }

        /* Action: Restore Stock Interlaced Timings */
        if (press_circle) {
            bool ok1 = lv1_restore_stock(MODE_240P_NTSC);
            bool ok2 = lv1_restore_stock(MODE_288P_PAL);
            if (ok1 && ok2) {
                memset(&vcfg, 0, sizeof(vcfg));
                vcfg.resolution = state.displayMode.resolution;
                vcfg.format = VIDEO_BUFFER_FORMAT_XRGB;
                vcfg.aspect = state.displayMode.aspect;
                vcfg.pitch  = res.width * sizeof(uint32_t);
                videoConfigure(0, &vcfg, NULL, 0);
                snprintf(status_msg, sizeof(status_msg), "RESTORED: Official Stock 480i / 576i Interlaced active.");
                status_color = 0xffffcc00; /* Yellow */
            } else {
                snprintf(status_msg, sizeof(status_msg), "ERROR: Could not restore stock LV1 values!");
                status_color = 0xffff4444; /* Red */
            }
        }

        /* Action: Exit to XMB */
        if (press_triangle) {
            exit_app = 1;
            break;
        }

        /* Check live LV1 state */
        int state_240p = lv1_get_mode_state(MODE_240P_NTSC);
        int state_288p = lv1_get_mode_state(MODE_288P_PAL);

        /* Render frame */
        set_render_target(ctx, &bufs[cur]);
        cellGcmSetClearColor(ctx, 0xff0c121e);
        cellGcmSetClearSurface(ctx, GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B | GCM_CLEAR_A);

        /* Draw UI Header */
        cellDbgFontPuts(0.08f, 0.08f, 1.15f, 0xff00e5ff, "==========================================================");
        cellDbgFontPuts(0.08f, 0.12f, 1.35f, 0xffffffff, "     PlayStation 3 Native 240p / 288p Switcher");
        cellDbgFontPuts(0.08f, 0.17f, 0.90f, 0xffaaaaaa, "       MultiAV Analog Progressive CRT Timing Tool (v1.0)");
        cellDbgFontPuts(0.08f, 0.20f, 1.15f, 0xff00e5ff, "==========================================================");

        /* Active Console Video Status */
        cellDbgFontPuts(0.08f, 0.26f, 1.05f, 0xffffff00, "[ Live Video & Memory Status ]");
        cellDbgFontPrintf(0.10f, 0.30f, 0.95f, 0xffffffff, "Active Resolution  : %ux%u", res.width, res.height);
        cellDbgFontPrintf(0.10f, 0.34f, 0.95f,
            (state_240p == STATE_PATCHED) ? 0xff00ff00 : ((state_240p == STATE_STOCK) ? 0xffffffff : 0xffff4444),
            "NTSC Record 1 (480): %s",
            (state_240p == STATE_PATCHED) ? "PATCHED -> 240p PROGRESSIVE (60 Hz)" :
            ((state_240p == STATE_STOCK)   ? "STOCK -> 480i INTERLACED" : "CUSTOM / UNKNOWN"));

        cellDbgFontPrintf(0.10f, 0.38f, 0.95f,
            (state_288p == STATE_PATCHED) ? 0xff00ff00 : ((state_288p == STATE_STOCK) ? 0xffffffff : 0xffff4444),
            "PAL  Record 4 (576): %s",
            (state_288p == STATE_PATCHED) ? "PATCHED -> 288p PROGRESSIVE (50 Hz)" :
            ((state_288p == STATE_STOCK)   ? "STOCK -> 576i INTERLACED" : "CUSTOM / UNKNOWN"));

        /* Controller Options */
        cellDbgFontPuts(0.08f, 0.45f, 1.05f, 0xff00e5ff, "----------------------------------------------------------");
        cellDbgFontPuts(0.08f, 0.48f, 1.05f, 0xffffff00, "[ Controller Actions ]");
        cellDbgFontPuts(0.10f, 0.52f, 1.00f, 0xff00e5ff, "[X] CROSS   : Apply NTSC 240p (720x240 @ 59.94 Hz, 15.734 kHz)");
        cellDbgFontPuts(0.10f, 0.56f, 1.00f, 0xffff80ff, "[#] SQUARE  : Apply PAL  288p (720x288 @ 50.00 Hz, 15.625 kHz)");
        cellDbgFontPuts(0.10f, 0.60f, 1.00f, 0xffff4444, "[O] CIRCLE  : Restore Stock Interlaced (480i / 576i)");
        cellDbgFontPuts(0.10f, 0.64f, 1.00f, 0xff44ff44, "[^] TRIANGLE: Exit to XMB (Dashboard)");
        cellDbgFontPuts(0.08f, 0.68f, 1.05f, 0xff00e5ff, "----------------------------------------------------------");

        /* Status Box */
        cellDbgFontPuts(0.08f, 0.72f, 1.05f, 0xffffffff, "Status:");
        cellDbgFontPrintf(0.10f, 0.76f, 1.10f, status_color, ">> %s", status_msg);

        /* Footer Note */
        cellDbgFontPuts(0.08f, 0.84f, 0.85f, 0xff888888, "Note: All patches are volatile RAM only. Rebooting returns console to stock.");

        cellDbgFontDrawGcm();
        cellGcmFlush(ctx);
        do_flip(ctx, (uint8_t)cur);
        cur = (cur + 1) % MAX_BUFFERS;
        usleep(16000);
    }

    /* Cleanup and exit cleanly */
    ioPadEnd();
    cellDbgFontExitGcm();
    free(dbg_buf);
    cellGcmFinish(CELL_GCM_CURRENT, 0);
    free(host_addr);

    sysProcessExit(0);
    return 0;
}
