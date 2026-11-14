/* SPDX-License-Identifier: MIT. Original implementation; no firmware bytes.
 * Private 4.93 ps1_emu initializer 0x87c90..0x87e8c establishes the context
 * at pointer slot 0x17c348. A full fingerprint precedes every PCM read.
 */
#include "pcm.h"
#include <ppu-lv2.h>
#include <sys/file.h>
#include <sys/timer.h>
#include <sys/sys_time.h>
#include <stdio.h>
#include <string.h>
static int copy(unsigned pid,uint32_t at,void *out,unsigned n)
{
 if(at<0x10000||!n||n>65536||(uint64_t)at+n>0x80000000u)return -1;
 lv2syscall6(8,0x7777,0x31,pid,at,(uint64_t)(uintptr_t)out,n);return_to_user_prog(int);
}
static int pids(uint32_t out[32])
{lv2syscall3(8,0x7777,0x21,(uint64_t)(uintptr_t)out);return_to_user_prog(int);}
static int name(unsigned pid,char out[256])
{lv2syscall4(8,0x7777,0x22,pid,(uint64_t)(uintptr_t)out);return_to_user_prog(int);}
static uint32_t b32(const unsigned char *p)
{return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static uint64_t b64(const unsigned char *p){return (uint64_t)b32(p)<<32|b32(p+4);}
static int pointer(uint32_t p,unsigned n)
{return p>=0x10000&&!(p&7)&&(uint64_t)p+n<=0x80000000u;}
typedef struct {
 uint32_t magic,sequence,pid,index_before,index_after,record;
 uint64_t begin_us,end_us;
 unsigned char pcm[16384];
} pcm_record;
_Static_assert(sizeof(pcm_record)==16424,"Private snapshot container layout");
static pcm_record record;
int ps1_pcm_collect(unsigned sequence)
{
 uint32_t ids[32]={0};unsigned pid=0;char label[256]={0},expected[64];
 if(!sequence||pids(ids))return -1;
 for(unsigned i=0;i<32;i++)if(ids[i]){
  memset(label,0,sizeof(label));if(name(ids[i],label))return -2;label[255]=0;
  snprintf(expected,sizeof(expected),"%08x_main_ps1_emu.self",ids[i]);
  if(!strcmp(label,"main_ps1_emu.self")||!strcmp(label,expected)){if(pid)return -3;pid=ids[i];}
 }
 if(!pid)return -4;
 unsigned char code[0x1fc],word[8],context[48];
 if(copy(pid,0x87c90,code,sizeof(code)))return -5;
 uint64_t hash=UINT64_C(14695981039346656037);
 for(unsigned i=0;i<sizeof(code);i++)hash=(hash^code[i])*UINT64_C(1099511628211);
 if(hash!=UINT64_C(0xd0ef0d64fd5b7f0d)||copy(pid,0x17c348,word,4))return -6;
 uint32_t ctx=b32(word);
 if(!pointer(ctx,sizeof(context))||copy(pid,ctx,context,sizeof(context)))return -7;
 uint32_t index=b32(context+8),state=b32(context+12),audio=b32(context+36);
 if(b32(context)>=8||(state!=1&&state!=2)||b64(context+16)!=2||b64(context+24)!=8||
    b32(context+32)!=16384||!pointer(index,8)||!pointer(audio,16384))return -8;
 char path[96];snprintf(path,sizeof(path),"/dev_hdd0/tmp/crt_ps1_audio.%u.pcm",sequence);
 int fd=-1;if(sysLv2FsOpen(path,SYS_O_WRONLY|SYS_O_CREAT|SYS_O_TRUNC,&fd,0666,0,0))return -9;
 record.magic=0x41554452;record.sequence=sequence;record.pid=pid;
 int result=0;
 for(unsigned i=0;i<64;i++){
  record.record=i;record.begin_us=sys_time_get_system_time();
  if(copy(pid,index,word,8)){result=-10;break;}record.index_before=(uint32_t)b64(word);
  if(copy(pid,audio,record.pcm,sizeof(record.pcm))||copy(pid,index,word,8)){result=-11;break;}
  record.index_after=(uint32_t)b64(word);record.end_us=sys_time_get_system_time();
  if(record.index_before>=8||record.index_after>=8){result=-12;break;}
  uint64_t wrote=0;
  if(sysLv2FsWrite(fd,&record,sizeof(record),&wrote)||wrote!=sizeof(record)){result=-13;break;}
  sys_timer_usleep(20000);
 }
 sysLv2FsClose(fd);return result;
}
