/* SPDX-License-Identifier: MIT. Synthetic process memory, never Sony bytes. */
#include "../plugin/ps1_audio_log.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {
 uint32_t ids[32];unsigned reads,emits,code_bad,ctx_bad,layout_bad,consumer_bad,reinit,pcm_reads;
 char process[256];ps1_audio_row last;
} fixture;
static int pids(void *u,uint32_t ids[32]){memcpy(ids,((fixture *)u)->ids,128);return 0;}
static int name(void *u,uint32_t pid,char out[256])
{(void)pid;snprintf(out,256,"%s",((fixture *)u)->process);return 0;}
static void put32(unsigned char *p,uint32_t v)
{for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(24-i*8));}
static int read_process(void *u,uint32_t pid,uint32_t at,void *out,unsigned n)
{
 fixture *f=u;assert(pid==42);f->reads++;unsigned char *p=out;memset(p,0,n);
 if(at==0x87c90){assert(n==0x1fc);if(f->code_bad)p[0]=1;return 0;}
 if(at==0x17c348){assert(n==4);put32(p,f->ctx_bad?0:(f->reinit&&f->reads>4?0x1901000:0x1900000));return 0;}
 if(at==0x1900000){assert(n==48);put32(p+8,0x20010010);put32(p+12,1);put32(p+20,2);
  put32(p+28,8);put32(p+32,f->layout_bad?1:16384);put32(p+36,0x20020000);put32(p+40,0x20020000);return 0;}
 if(at==0x20010010){assert(n==8);put32(p+4,f->consumer_bad?8:3);return 0;}
 if(at==0x20020000)f->pcm_reads++;
 assert(!"Unexpected or PCM-buffer read");return -1;
}
static void emit(void *u,const ps1_audio_row *row){fixture *f=u;f->last=*row;f->emits++;}
static void reset(fixture *f,ps1_audio_log *s)
{memset(f,0,sizeof(*f));memset(s,0,sizeof(*s));f->ids[0]=42;snprintf(f->process,256,"0000002a_main_ps1_emu.self");}
static void tick(fixture *f,ps1_audio_log *s,uint64_t now)
{ps1_audio_log_ops ops={pids,name,read_process,emit,f};vsh_video_state video={0};video.raw[8]=4;video.raw[9]=1;
 ps1_audio_log_tick(s,&ops,now,&video,0,1,0,4);}
int main(void)
{
 fixture f;ps1_audio_log s;reset(&f,&s);tick(&f,&s,0);
 assert(f.emits==1&&f.last.result==0&&f.last.emulator==1&&f.last.pid==42);
 assert(f.last.cached_status==1&&f.last.channels==2&&f.last.blocks==8&&f.last.consumer==3);
 assert(f.last.video.raw[8]==4&&f.last.armed==1&&!f.last.app&&f.last.target==4&&!f.pcm_reads);
 unsigned reads=f.reads;tick(&f,&s,PS1_LOG_INTERVAL-1);assert(f.emits==1&&f.reads==reads);
 tick(&f,&s,PS1_LOG_INTERVAL);assert(f.emits==2);
 for(unsigned i=2;i<PS1_LOG_SAMPLES;i++)tick(&f,&s,(uint64_t)i*PS1_LOG_INTERVAL);
 reads=f.reads;tick(&f,&s,UINT64_C(10000000000));assert(f.emits==120&&f.reads==reads);
 reset(&f,&s);f.code_bad=1;tick(&f,&s,0);assert(f.last.result==-5&&f.reads==1);
 reset(&f,&s);f.ctx_bad=1;tick(&f,&s,0);assert(f.last.result==-7&&f.reads==2);
 reset(&f,&s);f.layout_bad=1;tick(&f,&s,0);assert(f.last.result==-8&&f.reads==3);
 reset(&f,&s);f.consumer_bad=1;tick(&f,&s,0);assert(f.last.result==-10&&f.reads==4);
 reset(&f,&s);f.reinit=1;tick(&f,&s,0);assert(f.last.result==-11&&f.reads==5);
 reset(&f,&s);snprintf(f.process,256,"main_ps1_netemu.self");tick(&f,&s,0);assert(f.last.emulator==2&&f.last.result==2&&!f.reads);
 reset(&f,&s);snprintf(f.process,256,"0000002b_main_ps1_emu.self");tick(&f,&s,0);assert(f.last.result==1&&!f.reads);
 reset(&f,&s);f.ids[1]=43;snprintf(f.process,256,"main_ps1_emu.self");tick(&f,&s,0);assert(f.last.result==-3&&!f.reads);
 reset(&f,&s);memset(f.ids,0,sizeof(f.ids));tick(&f,&s,0);assert(f.last.result==1&&!f.reads);
 puts("PASS: bounded read-only PS1 diagnostic identity, interval, budget, races and no-PCM-read guards");return 0;
}
