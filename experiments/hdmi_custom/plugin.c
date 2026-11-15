/* SPDX-License-Identifier: MIT. One resident session, no boot registration. */
#include "service.h"
#include "backend.h"
#include "input.h"
#include "video.h"
#include <sys/thread.h>
#include <sys/timer.h>
#include <sys/file.h>
#include <sys/sys_time.h>
#include <ppu-lv2.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static hc_service service;
static atomic_int running;
static sys_ppu_thread_t thread;
static int log_fd=-1;
static unsigned log_bytes;
static void __attribute__((noinline,noreturn)) exit_thread(void)
{lv2syscall1(41,0);__builtin_unreachable();}
static void log_line(const char *event,uint32_t value)
{
 if(log_fd<0||log_bytes>256*1024-256)return;
 char line[200];uint64_t wrote=0;
 int n=snprintf(line,sizeof(line),"%llu %s code=%08x\n",(unsigned long long)sys_time_get_system_time(),event,value);
 if(n<0||(unsigned)n>=sizeof(line))return;
 if(!sysLv2FsWrite(log_fd,line,(unsigned)n,&wrote))log_bytes+=(unsigned)wrote;
}
static uint64_t clock_now(void *u){(void)u;return sys_time_get_system_time();}
static void publish(const hc_reply *r)
{
 int fd;uint64_t written=0;
 if(!sysLv2FsOpen(HC_REPLY ".stage",SYS_O_CREAT|SYS_O_WRONLY|SYS_O_TRUNC,&fd,0666,0,0)){
  int rc=sysLv2FsWrite(fd,r,sizeof(*r),&written);sysLv2FsClose(fd);
  if(!rc&&written==sizeof(*r)){
   (void)sysLv2FsUnlink(HC_REPLY);(void)sysLv2FsRename(HC_REPLY ".stage",HC_REPLY);
  }
 }
}
static int request(hc_request *q)
{
 int fd;uint64_t got=0;unsigned char bytes[sizeof(*q)+1];
 if(sysLv2FsOpen(HC_REQUEST,SYS_O_RDONLY,&fd,0,0,0))return 0;
 int rc=sysLv2FsRead(fd,bytes,sizeof(bytes),&got);sysLv2FsClose(fd);
 if(rc||got!=sizeof(*q))return 0;memcpy(q,bytes,sizeof(*q));return 1;
}
static void worker(void *unused)
{
 (void)unused;
 (void)sysLv2FsOpen(HC_LOG,SYS_O_CREAT|SYS_O_WRONLY|SYS_O_TRUNC,&log_fd,0666,0,0);
 log_line("hdmi_custom_v2_start",0);hc_backend_log(log_line);hc_input_log(log_line);hc_video_log(log_line);
 (void)sysLv2FsUnlink(HC_REQUEST);(void)sysLv2FsUnlink(HC_REPLY);
 if(hc_input_bind()){log_line("input_identity_refused",1);atomic_store(&running,0);}
 else{int fd;if(!sysLv2FsOpen(HC_READY,SYS_O_CREAT|SYS_O_WRONLY|SYS_O_TRUNC,&fd,0666,0,0))sysLv2FsClose(fd);}
 const hc_ops ops={hc_foreground,hc_backend_start,hc_backend_tick,hc_backend_keep,hc_backend_cancel,
  hc_backend_health,hc_prompt,hc_close_prompt,clock_now,0};
 int stopping=0;uint64_t poll_at=0;
 while(atomic_load(&running)||service.engine_owned||hc_input_cleanup()!=1){
  hc_request q={0};int have=0;uint64_t now=clock_now(0);
  if(!atomic_load(&running)&&!stopping){stopping=1;q=(hc_request){HC_MAGIC,service.seen+1u,HC_STOP,0};if(!q.sequence)q.sequence=1;have=1;}
  else if(now>=poll_at){poll_at=now+250000;have=request(&q);}
  if(have&&q.magic==HC_MAGIC&&q.sequence!=service.seen&&q.action==HC_APP_SELECT&&!service.engine_owned&&service.state!=HC_FAILED){
   if(hc_video_begin(q.sequence)){log_line("app_owner_refused",q.sequence);q.action=0;}
   else{
    int fd;uint64_t got=0;unsigned char bytes[sizeof(hc_test_lease)+1];hc_test_lease lease;
    if(!sysLv2FsOpen(HC_TEST_LEASE,SYS_O_RDONLY,&fd,0,0,0)){
     int rc=sysLv2FsRead(fd,bytes,sizeof(bytes),&got);sysLv2FsClose(fd);
     if(!rc&&got==sizeof(lease)){
      memcpy(&lease,bytes,sizeof(lease));
      if(hc_trial_valid(&lease,q.mode,now)){
       if(sysLv2FsUnlink(HC_TEST_LEASE)){log_line("test_lease_consume_failed",1);q.action=0;}
       else{q.action=HC_TRIAL;log_line("app_test_lease_120_seconds",q.sequence);}
      }
     }
    }
   }
  }
  hc_video_poll();
  hc_reply reply;unsigned before=service.state;
  if(hc_tick(&service,&ops,now,have?&q:NULL,&reply)){
   if(before!=service.state){log_line("sequence",reply.sequence);log_line("mode",reply.mode);log_line("state",reply.state);log_line("detail",reply.detail);}
   publish(&reply);
  }
  if(hc_input_cleanup()<0)log_line("pad_cleanup_unconfirmed",1);
  sys_timer_usleep(25000);
 }
 (void)sysLv2FsUnlink(HC_READY);log_line("hdmi_custom_stop",service.state);
 if(log_fd>=0)sysLv2FsClose(log_fd);exit_thread();
}
static void __attribute__((noinline)) start_worker(void)
{
 (void)sysLv2FsUnlink(HC_READY);atomic_store(&running,1);
 if(sysThreadCreate(&thread,worker,0,1100,65536,THREAD_JOINABLE,HC_MODULE))atomic_store(&running,0);
}
int module_start(size_t n,void *p){(void)n;(void)p;start_worker();exit_thread();}
int module_stop(size_t n,void *p)
{(void)n;(void)p;if(atomic_exchange(&running,0)){uint64_t status;(void)sysThreadJoin(thread,&status);}exit_thread();}
