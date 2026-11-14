/* SPDX-License-Identifier: MIT */
#ifndef CRT_HEN_DIAGNOSTIC_H
#define CRT_HEN_DIAGNOSTIC_H
#include <stdint.h>
#include <stdio.h>

#define HD_VERSION "0.1.0"
#ifdef HD_PUBLIC_ONLY
#define HD_TITLE_ID "CRTHEN002"
#define HD_TITLE "CRT Public Video Diagnostic"
#define HD_MODE "public-only"
#define HD_REPORT_PATH "/dev_hdd0/tmp/crt_hen_diagnostic_public.json"
#define HD_STAGING_PATH "/dev_hdd0/tmp/crt_hen_diagnostic_public.pending"
#else
#define HD_TITLE_ID "CRTHEN001"
#define HD_TITLE "CRT HEN Diagnostic"
#define HD_MODE "hen-queries"
#define HD_REPORT_PATH "/dev_hdd0/tmp/crt_hen_diagnostic.json"
#define HD_STAGING_PATH "/dev_hdd0/tmp/crt_hen_diagnostic.pending"
#endif
#define HD_UNQUERIED UINT64_MAX
#define HD_VSH_HEADER_ADDRESS UINT64_C(0x10000)
#define HD_VSH_HEADER_BYTES 16u

typedef uint64_t (*hd_raw_fn)(void *,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
typedef struct {
 hd_raw_fn raw;void *user;
 unsigned hen_confirmed,mapi_confirmed;
 uint32_t vsh_pid;
} hd_bus;

typedef struct {unsigned resolution,scan,conversion,aspect,refresh;} hd_mode;
typedef struct {
 int state_rc,configuration_rc,device_count,device_rc;
 unsigned state,color_space;
 hd_mode current;
 unsigned configuration_resolution,configuration_format,configuration_aspect,pitch;
 unsigned port,device_state,device_color_space,rgb_range,reported_modes,mode_count;
 hd_mode modes[32];
} hd_video;
typedef struct {uint64_t rc;char name[96],path[256];} hd_slot;
typedef struct {
 uint64_t hen_identity,hen_revision,core_version,core_minversion,firmware_version;
 uint64_t process_list_rc,header_rc;
 unsigned hen_confirmed,mapi_confirmed,vsh_matches;
 uint32_t vsh_pid;
 unsigned header_valid,elf_class,elf_endian;
 hd_slot slots[6];
 hd_video video;
} hd_report;

/* Closed query boundary. No arbitrary syscall, LV1 opcode or address range. */
uint64_t hd_checked_call(hd_bus *,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
void hd_collect(hd_report *,hd_raw_fn,void *,void (*)(void *,hd_video *));
int hd_write_json(FILE *,const hd_report *);
#endif
