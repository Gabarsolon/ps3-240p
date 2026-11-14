/* SPDX-License-Identifier: MIT. No PCM capture, process/LV1/MMIO writes or hooks.
 * 4.93 ps1_emu layout identity is the complete initializer fingerprint;
 * unknown emulators/layouts are reported without following their pointers.
 */
#include "ps1_audio_log.h"
#include <stdio.h>
#include <string.h>
static uint32_t b32(const unsigned char *p)
{return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static uint64_t b64(const unsigned char *p){return (uint64_t)b32(p)<<32|b32(p+4);}
static int pointer(uint32_t p,unsigned n)
{return p>=0x10000&&!(p&7)&&(uint64_t)p+n<=0x80000000u;}
static int process_kind(uint32_t pid,const char *name)
{
 const char *base[2]={"main_ps1_emu.self","main_ps1_netemu.self"};char prefixed[64];
 for(unsigned i=0;i<2;i++){
  if(!strcmp(name,base[i]))return (int)i+1;
  snprintf(prefixed,sizeof(prefixed),"%08x_%s",pid,base[i]);
  /* Cobra uses a PID prefix. Never strip an unrelated process's prefix. */
  if(!strcmp(name,prefixed))return (int)i+1;
 }
 return 0;
}
static int snapshot(const ps1_audio_log_ops *o,ps1_audio_row *row)
{
 uint32_t ids[32]={0};char name[256];
 if(o->pids(o->user,ids))return -1;
 for(unsigned i=0;i<32;i++)if(ids[i]){
  memset(name,0,sizeof(name));
  if(o->name(o->user,ids[i],name))return -2;
  name[255]=0;int kind=process_kind(ids[i],name);
  if(kind){if(row->pid)return -3;row->pid=ids[i];row->emulator=(unsigned)kind;}
 }
 if(!row->pid)return 1; /* No PS1 process is an ordinary XMB sample. */
 if(row->emulator!=1)return 2; /* netemu has no verified layout here. */
 unsigned char code[0x1fc],word[8],context[48];
 if(o->read(o->user,row->pid,0x87c90,code,sizeof(code)))return -4;
 uint64_t hash=UINT64_C(14695981039346656037);
 for(unsigned i=0;i<sizeof(code);i++)hash=(hash^code[i])*UINT64_C(1099511628211);
#ifdef PS1_AUDIO_LOG_HOST_TEST
 if(hash!=UINT64_C(0xd0ef0d64fd5b7f0d)&&hash!=UINT64_C(0x0319015562663f55))return -5;
#else
 if(hash!=UINT64_C(0xd0ef0d64fd5b7f0d))return -5;
#endif
 if(o->read(o->user,row->pid,0x17c348,word,4))return -6;
 uint32_t ctx=b32(word);
 if(!pointer(ctx,sizeof(context))||o->read(o->user,row->pid,ctx,context,sizeof(context)))return -7;
 uint32_t port=b32(context),index=b32(context+8),status=b32(context+12),audio=b32(context+36),writer=b32(context+40);
 if(port>=8||(status!=1&&status!=2)||b64(context+16)!=2||b64(context+24)!=8||b32(context+32)!=16384||
    !pointer(index,8)||!pointer(audio,16384)||writer<audio||writer>=audio+16384||(writer-audio)%8)return -8;
 if(o->read(o->user,row->pid,index,word,8))return -9;
 uint64_t consumer=b64(word);if(consumer>=8)return -10;
 /* Recheck the context pointer: game exit/reinitialization can race a read. */
 if(o->read(o->user,row->pid,0x17c348,word,4)||b32(word)!=ctx)return -11;
 row->port=port;row->cached_status=status;row->channels=2;row->blocks=8;row->size=16384;
 row->consumer=(unsigned)consumer;row->writer_offset=writer-audio;return 0;
}
void ps1_audio_log_tick(ps1_audio_log *s,const ps1_audio_log_ops *o,uint64_t now,
 const vsh_video_state *video,int video_result,int armed,int app,unsigned target)
{
 if(!s||!o||!o->pids||!o->name||!o->read||!o->emit||s->samples>=PS1_LOG_SAMPLES||now<s->next_us)return;
 s->next_us=now>UINT64_MAX-PS1_LOG_INTERVAL?UINT64_MAX:now+PS1_LOG_INTERVAL;
 ps1_audio_row row={0};row.time_us=now;row.number=++s->samples;
 row.video_result=video_result;row.armed=armed;row.app=app;row.target=target;
 if(video&&!video_result)row.video=*video;
 row.result=snapshot(o,&row);o->emit(o->user,&row);
}
