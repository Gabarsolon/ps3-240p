#include "vsh_video.h"
#ifndef VSH_VIDEO_HOST_TEST
#include <sys/process.h>
#include <ppu-lv2.h>
#endif
#include <stddef.h>

static int (*get_state)(unsigned,unsigned,void *);
static int (*get_display)(uint32_t *,uint32_t *,uint32_t *,uint32_t *);
static int (*map_mode)(uint32_t *,uint32_t *,int,int,unsigned);
static int (*update_display)(unsigned,uint32_t,uint32_t,uint32_t,uint32_t,void *,int);
#ifndef VSH_VIDEO_HOST_TEST
static uint32_t pid;
static int copy(uint32_t at,void *out,unsigned n)
{
 if(at<0x10000||n>16384||(uint64_t)at+n>0x80000000u)return -1;
 lv2syscall6(8,0x7777,0x31,pid,at,(uint64_t)(uintptr_t)out,n);
 return_to_user_prog(int);
}
static int opd_valid(uint32_t at)
{
 uint32_t words[2];
 return !(at&3)&&!copy(at,words,sizeof(words))&&words[0]>=0x10000&&
  !(words[0]&3)&&words[0]<0x80000000u&&words[1]>=0x10000&&words[1]<0x80000000u;
}
static int equal(const char *a,const char *b)
{for(unsigned i=0;i<63;i++){if(a[i]!=b[i])return 0;if(!a[i])return 1;}return 0;}
static uint32_t resolve(const char *library,uint32_t nid)
{
 uint32_t base;
 if(copy(0x1008c,&base,4)||base>0x7ffff000u)return 0;
 for(unsigned i=0;i<512;i++){
  uint32_t at,h[7];char name[64]={0};
  if(copy(base+0x984+i*4,&at,4)||!at)return 0;
  if(copy(at,h,sizeof(h)))return 0;
  unsigned count=h[1]&0xffffu;
  if(count>4096||copy(h[4],name,sizeof(name)-1)||!equal(name,library))continue;
  for(unsigned j=0;j<count;j++){
   uint32_t id,fn;
   if(copy(h[5]+j*4,&id,4))return 0;
   if(id==nid){if(copy(h[6]+j*4,&fn,4)||!opd_valid(fn))return 0;return fn;}
  }
  return 0;
 }
 return 0;
}
uint32_t vsh_video_export(const char *library,uint32_t nid){return resolve(library,nid);}
int vsh_video_copy(uint32_t address,void *out,unsigned length){return copy(address,out,length);}
int vsh_video_bind(void)
{
 pid=sysProcessGetPid();
 uint32_t getter=resolve("xsetting",0x4712f276),method,table;
 uint32_t query=resolve("paf",0x40410f89),map=resolve("vshmain",0x3aecdd23),update=resolve("paf",0xd8af2727);
 if(!getter||!query||!map||!update)return -1;
 uint32_t (*get_table)(void)=(void *)(uintptr_t)getter;
 table=get_table();
 if(copy(table+0xc,&method,4)||!opd_valid(method))return -1;
 get_state=(void *)(uintptr_t)method;get_display=(void *)(uintptr_t)query;
 map_mode=(void *)(uintptr_t)map;update_display=(void *)(uintptr_t)update;
 return 0;
}
#else
extern int mock_native_get_state(unsigned,unsigned,void *);
extern int mock_native_get_display(uint32_t *,uint32_t *,uint32_t *,uint32_t *);
extern int mock_native_map(uint32_t *,uint32_t *,int,int,unsigned);
extern int mock_native_update(unsigned,uint32_t,uint32_t,uint32_t,uint32_t,void *,int);
int vsh_video_bind(void)
{
 get_state=mock_native_get_state;get_display=mock_native_get_display;
 map_mode=mock_native_map;update_display=mock_native_update;return 0;
}
#endif
int vsh_video_query(vsh_video_state *s)
{
 if(!get_state||!get_display)return -1;
 for(unsigned i=0;i<sizeof(*s);i++)((unsigned char *)s)[i]=0;
 int rc=get_state(0,0,s->raw);if(rc)return rc;
 s->state=get_display(&s->flags,&s->mode,&s->format,&s->pitch);
 return s->state<0?s->state:0;
}
int vsh_video_map(unsigned resolution,unsigned aspect,uint32_t *flags,uint32_t *mode)
{
 if(!map_mode||(resolution!=4&&resolution!=5))return -1;
 *flags=*mode=0;return map_mode(flags,mode,(int)resolution,aspect==2?2:1,0);
}
int vsh_video_configure(unsigned resolution,unsigned aspect)
{
 vsh_video_state s;uint32_t flags,mode;
 if(vsh_video_query(&s)||s.state!=0||(s.raw[8]!=4&&s.raw[8]!=5)||!s.pitch||s.pitch>65536)return -1;
 if(vsh_video_map(resolution,aspect,&flags,&mode)<0)return -1;
 /* Keep the active surface's format and pitch. The VSH RPC configure path
  * uses these same exported mapper/platform functions. Mode IDs are SD-only.
  * Last argument 1 submits without an unbounded wait for display completion. */
 return update_display(0,flags,mode,s.format,s.pitch,NULL,1);
}
int vsh_video_hd720(uint32_t *flags,uint32_t *mode)
{
 if(!map_mode||!flags||!mode)return -1;
 *flags=*mode=0;
 int rc=map_mode(flags,mode,2,2,0);
 /* Private 4.93 VSH mapper 0xc7080: low byte 5 selects HDMI.
  * Refuse clone/analog/50Hz output instead of guessing connection flags. */
 if(rc<=0||(*flags&0xff)!=5||(*flags&0xf00)==0xf00||
    (*flags&0x20000)||(*mode!=8&&*mode!=11))return -1;
 /* 4.93 VSH 0xc56f4 may map a 720 source to native mode 8 when the
  * selected output requires conversion to 1080i. That is not a physical
  * 720p transition. Audited native mode 11 selects progressive 720;
  * 0x435524/0x4303f0 and the 0xc7ecc configuration bridge establish the
  * resolution/scan distinction. The HD caller has already fingerprinted
  * 4.93 and must still verify the independent AV head before patching.
  * Preserve the mapper's connector/refresh flags and all surface fields. */
 *mode=11;
 return 0;
}
int vsh_video_hd_submit(const vsh_video_state *surface,uint32_t flags,uint32_t mode)
{
 if(!update_display||!surface||(flags&0xff)!=5||(flags&0xf00)==0xf00||!mode||
    !surface->pitch||surface->pitch<1920*4u||surface->pitch>65536||(surface->pitch&3))return -1;
 return update_display(0,flags,mode,surface->format,surface->pitch,NULL,1);
}
