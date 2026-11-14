/* Self-authored host fixtures; no firmware instruction bytes. */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../plugin/vsh_video.h"
#define PS1_AUDIO_HOST_TEST 1
#define SYS_O_WRONLY 1
#define SYS_O_RDONLY 0
#define SYS_O_CREAT 2
#define SYS_O_TRUNC 4
#define THREAD_JOINABLE 1
typedef int sysFSStat;
typedef unsigned long sys_ppu_thread_t;
static uint64_t clock_us;
static int writes,restore_blocked,mock_audio_identity,flag,request_present;
static int collections;
int ps1_pcm_collect(unsigned sequence){assert(sequence);collections++;return 0;}
static uint32_t request_data[4],reply_data[4];
static vsh_video_state current,baseline;
static int mock_audio_update(unsigned n,uint32_t f,uint32_t m,uint32_t fmt,uint32_t pitch,void *p,int wait)
{
 (void)n;(void)p;(void)wait;writes++;
 if(restore_blocked&&m==12)return -1;
 current.flags=f;current.mode=m;current.format=fmt;current.pitch=pitch;
 current.raw[8]=m==4?4:1;return 0;
}
int vsh_video_bind(void){return 0;}
int vsh_video_query(vsh_video_state *s){*s=current;return 0;}
static int sysLv2FsStat(const char *p,sysFSStat *s){(void)p;(void)s;return flag?0:-1;}
static int sysLv2FsOpen(const char *p,int mode,int *fd,int perm,void *x,int y)
{
 (void)perm;(void)x;(void)y;
 if(strstr(p,".request")){if(!request_present)return -1;*fd=1;}
 else if(strstr(p,".reply"))*fd=2;else *fd=3;
 (void)mode;return 0;
}
static int sysLv2FsRead(int fd,void *p,uint64_t n,uint64_t *got){assert(fd==1&&n==16);memcpy(p,request_data,16);*got=16;return 0;}
static int sysLv2FsWrite(int fd,const void *p,uint64_t n,uint64_t *w){if(fd==2){assert(n==16);memcpy(reply_data,p,16);}*w=n;return 0;}
static int sysLv2FsClose(int fd){(void)fd;return 0;}
static int sysLv2FsUnlink(const char *p){(void)p;return 0;}
static uint64_t sys_time_get_system_time(void){return clock_us;}
static void sys_timer_usleep(uint64_t us){clock_us+=us;}
static int sysThreadCreate(sys_ppu_thread_t *t,void (*f)(void *),void *p,int prio,unsigned stack,int flags,const char *name)
{(void)t;(void)f;(void)p;(void)prio;(void)stack;(void)flags;(void)name;return 0;}
static int sysThreadJoin(sys_ppu_thread_t t,uint64_t *s){(void)t;(void)s;return 0;}
#include "../experiments/ps1_audio/probe.c"
static void reset(void)
{
 memset(&current,0,sizeof(current));current.raw[8]=1;current.raw[9]=2;current.raw[10]=6;current.mode=12;
 current.flags=0x1010205;current.format=5;current.pitch=8192;baseline=current;
 atomic_store(&running,1);active=halted=seen=0;deadline=clock_us=0;
 writes=restore_blocked=mock_audio_identity=flag=request_present=collections=0;memset(reply_data,0,sizeof(reply_data));
}
static void command(unsigned seq,unsigned action,unsigned value)
{request_data[0]=MAGIC;request_data[1]=seq;request_data[2]=action;request_data[3]=value;request_present=1;tick();}
int main(void)
{
 reset();command(1,1,0);assert(!writes&&reply_data[2]==5);
 reset();command(1,2,0);assert(active==1&&writes==1&&current.mode==4);
 request_present=0;clock_us=14999999;tick();assert(writes==1);
 clock_us=15000000;tick();assert(!active&&writes==2&&same(&baseline,&current)&&reply_data[2]==5);
 reset();command(1,2,0);clock_us=5000000;command(2,3,0);assert(writes==2&&active==2);
 uint64_t saved_deadline=deadline;clock_us+=1;tick();assert(!active&&writes==2&&reply_data[1]==2&&reply_data[2]==5);
 /* A failed restore must reach the deadline even if cancel remains in inbox. */
 reset();command(1,2,0);restore_blocked=1;clock_us=1;command(2,3,0);saved_deadline=deadline;
 clock_us=100;tick();assert(deadline==saved_deadline&&writes==2);
 clock_us=saved_deadline;tick();assert(halted&&!active&&reply_data[2]==7);
 command(3,2,0);assert(writes==2&&reply_data[2]==6);
 reset();flag=1;command(1,2,0);assert(!writes&&reply_data[2]==6);
 reset();mock_audio_identity=-1;command(1,2,0);assert(!writes&&reply_data[2]==6);
 reset();current.pitch=7680;command(1,2,0);assert(!writes&&reply_data[2]==6);
 reset();command(1,2,9);assert(!writes&&reply_data[2]==6);
 reset();command(1,99,0);assert(!writes&&reply_data[2]==6);
 reset();flag=1;current.raw[8]=4;command(1,4,0);assert(collections==1&&!writes&&reply_data[2]==5);
 reset();mock_audio_identity=-1;command(1,4,0);assert(!collections&&!writes&&reply_data[2]==6);
 puts("PASS: audio stock control inspection, timeout, disconnected client, cancellation, failed recovery and denied preconditions");
}
