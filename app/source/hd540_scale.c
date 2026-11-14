/* SPDX-License-Identifier: MIT. No direct MMIO, clocks, flash or firmware writes. */
#include "hd540_scale.h"
#include <stdio.h>
#include <string.h>
#ifdef TIMING_OFFLINE_TEST
#include "timing_payloads.h"
#endif
static const uint32_t targets[HD540_SCALE_WORDS]={
 0x80002f48u,0x80002f50u,0x80003058u,0x80003060u,0x80003100u,0x80003108u,
 0x800036c8u,0x800036d0u};
static int fail(hd540_scale *s,const char *why)
{snprintf(s->error,sizeof(s->error),"%s",why);return -1;}
int hd540_scale_identity(const uint64_t words[HD540_SCALE_WORDS])
{
 if(!words)return 0;
#ifdef TIMING_OFFLINE_TEST
 /* Existing legacy scaler fixture plus self-authored OR/NOP instructions. */
 uint64_t nop=UINT64_C(0x6000000060000000);
 uint32_t or_word=(31u<<26)|(4u<<11)|(444u<<1);
 if(words[0]==STOCK_VSCALE_ENABLE_WORD&&words[1]==nop&&
    words[2]==(((uint64_t)or_word<<32)|0x60000000u)&&words[3]==nop&&
    words[4]==STOCK_VSCALE_FACTOR_WORD&&words[5]==nop&&words[6]==nop&&words[7]==nop)return 1;
#endif
 uint64_t hash=UINT64_C(14695981039346656037);
 for(unsigned i=0;i<HD540_SCALE_WORDS;i++)for(int shift=56;shift>=0;shift-=8)
  hash=(hash^(unsigned char)(words[i]>>shift))*UINT64_C(1099511628211);
 return hash==UINT64_C(0xd059dd47ffaf6c27);
}
static int current(hd540_scale *s)
{
 uint64_t address[HD540_SCALE_WORDS];
 if(patch_map_targets(s->transport,targets,address,HD540_SCALE_WORDS)||memcmp(address,s->address,sizeof(address)))
  return fail(s,"Scaler code mappings changed; refusing stale addresses");
 return 0;
}
static uint64_t read_word(hd540_scale *s,unsigned i)
{return s->transport->read(s->transport->user,s->address[i]);}
int hd540_scale_resolve(hd540_scale *s,patch_context *c)
{
 if(!s||!c)return -1;
 if(s->dirty)return fail(s,"Scaler code recovery pending");
 memset(s,0,sizeof(*s));s->transport=c;
 if(!c->ready||!c->read||!c->write||patch_map_targets(c,targets,s->address,HD540_SCALE_WORDS))
  return fail(s,"Audited scaler RAM transport unavailable");
 for(unsigned i=0;i<HD540_SCALE_WORDS;i++)s->original[i]=read_word(s,i);
 if(!hd540_scale_identity(s->original))return fail(s,"Scaler code differs from audited 4.93");
 memcpy(s->candidate,s->original,sizeof(s->candidate));
 /* Self-authored PPC encodings, original Sony code saved only at runtime:
  * ori r3,r0,0x1010: Y scaling plus retained vertical interpolation;
  * ori r0,r0,539: bounded output valid-window end, retaining upper bits;
  * addis r0,0,0x1fff: below the 2.0 boundary that read as zero in dev16.
  * All adjacent instructions and fingerprinted neighbors are preserved. */
 uint32_t enable=(24u<<26)|(3u<<16)|0x1010u;
 uint32_t window=(24u<<26)|539u;
 uint32_t factor=(15u<<26)|0x1fffu;
 s->candidate[0]=(s->original[0]&UINT64_C(0xffffffff00000000))|enable;
 s->candidate[2]=((uint64_t)window<<32)|(uint32_t)s->original[2];
 s->candidate[4]=(s->original[4]&UINT64_C(0xffffffff00000000))|factor;
 /* Native pitch writer at 0x8000369c: double only the supplied scanout
  * row pitch. Renderer and cached request retain their original pitch.
  * Caller bounds the source pitch below 32768 so the doubled value fits
  * the native low-16-bit pitch field. rlwinm r5,r5,1,0,28. */
 uint32_t pitch=(21u<<26)|(5u<<21)|(5u<<16)|(1u<<11)|(28u<<1);
 s->candidate[6]=((uint64_t)pitch<<32)|(uint32_t)s->original[6];
 s->ready=1;return 0;
}
int hd540_scale_check(hd540_scale *s,int patched)
{
 if(!s||!s->ready||current(s))return -1;
 const uint64_t *expected=patched?s->candidate:s->original;
 for(unsigned i=0;i<HD540_SCALE_WORDS;i++)if(read_word(s,i)!=expected[i])return 0;
 return 1;
}
int hd540_scale_restore(hd540_scale *s)
{
 if(!s||!s->ready||current(s))return -1;
 int bad=0;
 for(unsigned i=0;i<HD540_SCALE_WORDS;i++){
  uint64_t value=read_word(s,i);
  if(value==s->original[i])continue;
  if(value!=s->candidate[i]||s->candidate[i]==s->original[i]){bad=1;continue;}
  s->transport->write(s->transport->user,s->address[i],s->original[i]);
  if(read_word(s,i)!=s->original[i])bad=1;
 }
 if(bad)return fail(s,"Scaler code restoration unconfirmed; recovery retained");
 s->dirty=0;return 0;
}
int hd540_scale_apply(hd540_scale *s)
{
 if(!s||!s->ready||s->dirty)return -1;
 if(hd540_scale_check(s,0)!=1)return fail(s,"Scaler code changed after inspection; no write");
 s->dirty=1;
 for(unsigned i=0;i<HD540_SCALE_WORDS;i++)if(s->candidate[i]!=s->original[i]){
  s->transport->write(s->transport->user,s->address[i],s->candidate[i]);
  if(read_word(s,i)!=s->candidate[i]){(void)hd540_scale_restore(s);return fail(s,"Scaler code write/readback failed");}
 }
 return 0;
}
