/* SPDX-License-Identifier: MIT. Read-only, bounded process/config telemetry. */
#ifndef PS1_AUDIO_LOG_H
#define PS1_AUDIO_LOG_H
#include <stdint.h>
#include "vsh_video.h"
#define PS1_LOG_SAMPLES 120u
#define PS1_LOG_INTERVAL UINT64_C(5000000)
typedef struct {
 uint64_t time_us;uint32_t number,pid,emulator;
 int result,video_result,armed,app;unsigned target;
 vsh_video_state video;
 uint32_t port,cached_status,channels,blocks,size,consumer,writer_offset;
} ps1_audio_row;
typedef struct {
 int (*pids)(void *,uint32_t [32]);
 int (*name)(void *,uint32_t,char [256]);
 int (*read)(void *,uint32_t,uint32_t,void *,unsigned);
 void (*emit)(void *,const ps1_audio_row *);
 void *user;
} ps1_audio_log_ops;
typedef struct {uint64_t next_us;unsigned samples;} ps1_audio_log;
void ps1_audio_log_tick(ps1_audio_log *,const ps1_audio_log_ops *,uint64_t,
 const vsh_video_state *,int,int,int,unsigned);
#endif
