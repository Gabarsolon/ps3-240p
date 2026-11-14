/* SPDX-License-Identifier: MIT. Timed STOCK-output control and read-only PCM.
 * CFW owner's checked 4.93 VSH only. No flash, LV1, MMIO or audio writes.
 * Native flag interpretation: private VSH mapper 0xc6b18 and mode bridge
 * 0xc7ecc. Sony-derived evidence stays in local_only/ps1_audio/.
 */
#include "../../plugin/vsh_video.h"
#include "pcm.h"
#ifndef PS1_AUDIO_HOST_TEST
#include <sys/thread.h>
#include <sys/timer.h>
#include <sys/file.h>
#include <sys/sys_time.h>
#include <ppu-lv2.h>
#endif
#include <stdatomic.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define MAGIC 0x41554431u
#define BASE "/dev_hdd0/tmp/crt_ps1_audio"
typedef struct {uint32_t magic,sequence,action,value;} packet;
static atomic_int running;
static sys_ppu_thread_t worker_id;
static int log_fd=-1,active,halted;
static unsigned seen;
static uint64_t deadline;
static vsh_video_state saved;
static int (*update)(unsigned,uint32_t,uint32_t,uint32_t,uint32_t,void *,int);
static void __attribute__((noinline,noreturn)) exit_thread(void)
{
#ifndef PS1_AUDIO_HOST_TEST
 lv2syscall1(41,0);__builtin_unreachable();
#else
 abort();
#endif
}
static void note(const char *event,unsigned value)
{
 if(log_fd<0)return;char line[160];uint64_t wrote=0;
 int n=snprintf(line,sizeof(line),"%llu %s %08x\n",(unsigned long long)sys_time_get_system_time(),event,value);
 if(n>0&&(unsigned)n<sizeof(line))(void)sysLv2FsWrite(log_fd,line,(unsigned)n,&wrote);
}
static void answer(unsigned seq,unsigned state,unsigned detail)
{
 packet p={MAGIC,seq,state,detail};int fd=-1;uint64_t wrote=0;
 if(!sysLv2FsOpen(BASE ".reply",SYS_O_WRONLY|SYS_O_CREAT|SYS_O_TRUNC,&fd,0666,0,0)){
  (void)sysLv2FsWrite(fd,&p,sizeof(p),&wrote);sysLv2FsClose(fd);
 }
 note("state",state);note("detail",detail);
}
static int exists(const char *path){sysFSStat st;return !sysLv2FsStat(path,&st);}
static int identity(void)
{
#ifdef PS1_AUDIO_HOST_TEST
 update=mock_audio_update;return mock_audio_identity;
#else
 uint32_t opd[2];unsigned char code[32];
 unsigned at=vsh_video_export("paf",0xd8af2727);
 if(!at||vsh_video_copy(at,opd,sizeof(opd))||opd[0]!=0x38ac04||opd[1]!=0x6f5558||vsh_video_copy(0x455558,code,sizeof(code)))return -1;
 uint64_t hash=UINT64_C(14695981039346656037);
 for(unsigned i=0;i<sizeof(code);i++)hash=(hash^code[i])*UINT64_C(1099511628211);
 if(hash!=UINT64_C(0xdd7a4e8ea214e07a))return -1;
 update=(void *)(uintptr_t)at;return 0;
#endif
}
static int same(const vsh_video_state *a,const vsh_video_state *b)
{
 return !b->state&&a->flags==b->flags&&a->mode==b->mode&&a->format==b->format&&a->pitch==b->pitch&&
  !memcmp(a->raw+8,b->raw+8,8);
}
static int prepare(void)
{
 if(exists("/dev_hdd0/tmp/240p_app.active")||exists("/dev_hdd0/tmp/240p_enforcer.on"))return -1;
 if(identity()){note("identity_refused",1);return -1;}
 if(vsh_video_query(&saved)){note("query_refused",1);return -1;}
 note("query_flags",saved.flags);note("query_mode",saved.mode);note("query_format",saved.format);note("query_pitch",saved.pitch);
 if(saved.state||saved.raw[8]!=1||saved.raw[9]!=2||saved.raw[10]!=6||saved.mode!=12||
    (saved.flags&0xff)!=5||(saved.flags&0xf00)==0xf00||(saved.flags&0x20000)||saved.format!=5||saved.pitch!=8192)return -1;
 note("saved_flags",saved.flags);note("saved_mode",saved.mode);note("saved_pitch",saved.pitch);return 0;
}
static void restore(void)
{
 if(!active)return;
 /* Keep recovery active before submission. A lost HTTP client cannot stop it. */
 int rc=update(0,saved.flags,saved.mode,saved.format,saved.pitch,0,1);
 note("restore_submit",(unsigned)rc);deadline=sys_time_get_system_time()+UINT64_C(8000000);active=2;
}
static void tick(void)
{
 uint64_t now=sys_time_get_system_time();vsh_video_state live;
 if(active==1&&now>=deadline)restore();
 if(active==2){
  if(!vsh_video_query(&live)&&same(&saved,&live)){active=0;answer(seen,5,0);}
  else if(now>=deadline){halted=1;active=0;answer(seen,7,1);}
 }
 if(!atomic_load(&running))return;
 packet q={0};int fd=-1;uint64_t got=0;
 if(!sysLv2FsOpen(BASE ".request",SYS_O_RDONLY,&fd,0,0,0)){
  int rc=sysLv2FsRead(fd,&q,sizeof(q),&got);sysLv2FsClose(fd);if(rc||got!=sizeof(q))q.magic=0;
 }
 if(q.magic!=MAGIC||!q.sequence||q.sequence==seen)return;
 if(active){
  if(q.action==3&&!q.value){seen=q.sequence;if(active==1)restore();answer(seen,4,0);}
  return;
 }
 seen=q.sequence;
 if(q.action==4&&!q.value&&!halted){
  int rc=identity();if(!rc)rc=ps1_pcm_collect(seen);
  answer(seen,rc?6:5,(unsigned)rc);return;
 }
 if(halted||q.value||q.action<1||q.action>2||prepare()){answer(seen,6,1);return;}
 if(q.action==1){answer(seen,5,0);return;}
 /* Native connector 1 = composite/S-video, output-head flag 0x100;
  * native mode 4 = stock 480i, refresh-family flag 0x10000.
  * Preserve the existing color/depth fields and allocated surface storage.
  */
 uint32_t flags=(saved.flags&0x0fc00000u)|0x10101u;
 active=1;deadline=now+UINT64_C(15000000);
 int rc=update(0,flags,4,saved.format,saved.pitch,0,1);
 note("stock_analog_submit",(unsigned)rc);
 if(rc){restore();return;}
 answer(seen,3,0);
}
static void worker(void *unused)
{
 (void)unused;
 (void)sysLv2FsOpen(BASE ".log",SYS_O_WRONLY|SYS_O_CREAT|SYS_O_TRUNC,&log_fd,0666,0,0);
 note("ps1_audio_control_v2",0);
 if(vsh_video_bind()){note("bind_failed",1);halted=1;}
 (void)sysLv2FsUnlink(BASE ".request");(void)sysLv2FsUnlink(BASE ".reply");
 int fd=-1;if(!sysLv2FsOpen(BASE ".v2.ready",SYS_O_WRONLY|SYS_O_CREAT|SYS_O_TRUNC,&fd,0666,0,0))sysLv2FsClose(fd);
 while(atomic_load(&running)||active){sys_timer_usleep(100000);if(!atomic_load(&running)&&active==1)restore();tick();}
 (void)sysLv2FsUnlink(BASE ".v2.ready");if(log_fd>=0)sysLv2FsClose(log_fd);exit_thread();
}
static void __attribute__((noinline)) start_worker(void)
{
 atomic_store(&running,1);
 if(sysThreadCreate(&worker_id,worker,0,1100,65536,THREAD_JOINABLE,"crt_ps1_audio"))atomic_store(&running,0);
}
int module_start(size_t argc,void *argv){(void)argc;(void)argv;start_worker();exit_thread();}
int module_stop(size_t argc,void *argv)
{
 (void)argc;(void)argv;
 if(atomic_exchange(&running,0)){uint64_t status=0;(void)sysThreadJoin(worker_id,&status);}exit_thread();
}
