/* SPDX-License-Identifier: MIT. Uses only PS3MAPI process GET operations.
 * Open one capped text log; never records audio or arbitrary process names.
 */
#include "vsh_ps1_audio_log.h"
#include "ps1_audio_log.h"
#include <sys/file.h>
#include <sys/sys_time.h>
#include <ppu-lv2.h>
#include <stdio.h>
#include <string.h>
static int fd=-1;
static unsigned bytes;
static ps1_audio_log session;
static int pids(void *u,uint32_t ids[32])
{(void)u;lv2syscall3(8,0x7777,0x21,(uint64_t)(uintptr_t)ids);return_to_user_prog(int);}
static int name(void *u,uint32_t pid,char out[256])
{(void)u;lv2syscall4(8,0x7777,0x22,pid,(uint64_t)(uintptr_t)out);return_to_user_prog(int);}
static int read_process(void *u,uint32_t pid,uint32_t at,void *out,unsigned n)
{
 (void)u;if(!pid||at<0x10000||!n||n>0x1fc||(uint64_t)at+n>0x80000000u)return -1;
 lv2syscall6(8,0x7777,0x31,pid,at,(uint64_t)(uintptr_t)out,n);return_to_user_prog(int);
}
static void line(const char *text)
{
 unsigned n=(unsigned)strlen(text);if(fd<0||n>65536u-bytes)return;
 uint64_t wrote=0;if(sysLv2FsWrite(fd,text,n,&wrote)||wrote!=n){sysLv2FsClose(fd);fd=-1;return;}
 bytes+=n;
}
static void emit(void *u,const ps1_audio_row *r)
{
 (void)u;char text[512];
 int n=snprintf(text,sizeof(text),
  "%llu sample=%u pid=%08x emu=%u rc=%d armed=%d app=%d target=%u video_rc=%d flags=%08x mode=%u pitch=%u native=%u/%u/%u/%u port=%u cached_status=%u channels=%u blocks=%u size=%u consumer=%u writer_offset=%u\n",
  (unsigned long long)r->time_us,r->number,r->pid,r->emulator,r->result,r->armed,r->app,r->target,r->video_result,
  r->video.flags,r->video.mode,r->video.pitch,r->video.raw[8],r->video.raw[9],r->video.raw[10],r->video.raw[11],
  r->port,r->cached_status,r->channels,r->blocks,r->size,r->consumer,r->writer_offset);
 if(n>0&&(unsigned)n<sizeof(text))line(text);
 if(r->number==PS1_LOG_SAMPLES)line("END sample budget exhausted; no further process reads\n");
}
void vsh_ps1_audio_log_open(void)
{
 memset(&session,0,sizeof(session));bytes=0;
 if(sysLv2FsOpen("/dev_hdd0/tmp/ps1_audio_diagnostic.log",SYS_O_WRONLY|SYS_O_CREAT|SYS_O_TRUNC,&fd,0666,0,0))fd=-1;
 line("CRT Toolset 1.4.4-rc2 audio diagnostic v1; max120 samples/5s; emu0=none/1=ps1_emu/2=ps1_netemu; cached_status is not live playback status; no PCM or physical-rate measurement\n");
}
void vsh_ps1_audio_log_poll(int armed,int app,unsigned target)
{
 if(fd<0||session.samples>=PS1_LOG_SAMPLES)return;
 uint64_t now=sys_time_get_system_time();if(now<session.next_us)return;
 vsh_video_state video;memset(&video,0,sizeof(video));int rc=vsh_video_query(&video);
 const ps1_audio_log_ops ops={pids,name,read_process,emit,0};
 ps1_audio_log_tick(&session,&ops,now,&video,rc,armed,app,target);
}
void vsh_ps1_audio_log_close(void)
{if(fd>=0){line("END helper stopped\n");sysLv2FsClose(fd);fd=-1;}}
