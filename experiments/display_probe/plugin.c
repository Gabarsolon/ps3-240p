/* SPDX-License-Identifier: MIT. Standalone probe; no installed app replacement. */
#include "native.h"
#include <sys/thread.h>
#include <sys/timer.h>
#include <sys/file.h>
#include <sys/sys_time.h>
#include <ppu-lv2.h>
#include <stdatomic.h>
#include <stdio.h>
static atomic_int running;
static sys_ppu_thread_t thread;
static dp_service service;
static int log_fd=-1;
static unsigned log_bytes;
static void __attribute__((noinline,noreturn)) exit_thread(void)
{lv2syscall1(41,0);__builtin_unreachable();}
static void log_line(const char *event,uint32_t value)
{
 if(log_fd<0||log_bytes>128*1024-200)return;
 char line[192];uint64_t wrote=0;
 int n=snprintf(line,sizeof(line),"%llu %s code=%08x\n",(unsigned long long)sys_time_get_system_time(),event,(unsigned)value);
 if(n<0)return;if((unsigned)n>=sizeof(line))n=sizeof(line)-1;
 if(!sysLv2FsWrite(log_fd,line,(uint64_t)n,&wrote))log_bytes+=(unsigned)wrote;
}
static void publish(const dp_reply *reply)
{
 int fd=-1;uint64_t wrote=0;
 if(!sysLv2FsOpen(DPROBE_REPLY,SYS_O_WRONLY|SYS_O_CREAT|SYS_O_TRUNC,&fd,0666,0,0)){
  int rc=sysLv2FsWrite(fd,reply,sizeof(*reply),&wrote);sysLv2FsClose(fd);
  if(rc||wrote!=sizeof(*reply))log_line("reply_write_failed",(unsigned)rc);
 }
}
static void poll(void)
{
 dp_request q={0};int fd=-1;uint64_t got=0;
 if(!sysLv2FsOpen(DPROBE_REQUEST,SYS_O_RDONLY,&fd,0,0,0)){
  int rc=sysLv2FsRead(fd,&q,sizeof(q),&got);sysLv2FsClose(fd);
  if(rc||got!=sizeof(q))q.magic=0;
 }
 dp_reply reply;
 if(dp_tick(&service,dp_native_ops(),sys_time_get_system_time(),&q,&reply)){
  log_line("sequence",reply.sequence);log_line("kind",service.kind);log_line("preset",service.profile);
  log_line("state",reply.state);log_line("detail",reply.detail);publish(&reply);
 }
}
static void worker(void *u)
{
 (void)u;
 (void)sysLv2FsOpen(DPROBE_LOG,SYS_O_WRONLY|SYS_O_CREAT|SYS_O_TRUNC,&log_fd,0666,0,0);
 log_line("display_probe_v2_start",0);dp_native_log(log_line);
 (void)sysLv2FsUnlink(DPROBE_REQUEST);(void)sysLv2FsUnlink(DPROBE_REPLY);
 int ready=-1;
 if(!sysLv2FsOpen(DPROBE_READY,SYS_O_WRONLY|SYS_O_CREAT|SYS_O_TRUNC,&ready,0666,0,0))sysLv2FsClose(ready);
 while(atomic_load(&running)){sys_timer_usleep(250000);poll();}
 if(dp_busy(&service)&&!service.halted){
  dp_request cancel={DPROBE_MAGIC,service.seen+1u,DP_CANCEL,1};if(!cancel.sequence)cancel.sequence=1;
  dp_reply reply;dp_tick(&service,dp_native_ops(),sys_time_get_system_time(),&cancel,&reply);publish(&reply);
  while(dp_busy(&service)&&!service.halted){sys_timer_usleep(250000);poll();}
 }
 (void)sysLv2FsUnlink(DPROBE_READY);log_line("display_probe_stop",service.state);
 if(log_fd>=0)sysLv2FsClose(log_fd);exit_thread();
}
static void __attribute__((noinline)) start_worker(void)
{
 (void)sysLv2FsUnlink(DPROBE_READY);atomic_store(&running,1);
 if(sysThreadCreate(&thread,worker,0,1100,65536,THREAD_JOINABLE,"crt_display_probe"))atomic_store(&running,0);
}
int module_start(size_t args,void *data){(void)args;(void)data;start_worker();exit_thread();}
int module_stop(size_t args,void *data)
{
 (void)args;(void)data;if(atomic_exchange(&running,0)){uint64_t status=0;(void)sysThreadJoin(thread,&status);}exit_thread();
}
