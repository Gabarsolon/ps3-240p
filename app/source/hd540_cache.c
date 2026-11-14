/* SPDX-License-Identifier: MIT. Own code in checked unused mapped RAM padding.
 * Wrapper performs native cache maintenance and read-only FP telemetry.
 * No Sony instruction bytes, direct MMIO writes or permanent storage writes. */
#include "hd540_cache.h"
#include <stdio.h>
#include <string.h>
#define WRAPPER_VA UINT64_C(0x8003f800)
static int fail(hd540_cache *h,const char *why)
{snprintf(h->error,sizeof(h->error),"%s",why);return -1;}
static void targets(uint32_t va[HD540_CACHE_WORDS])
{
 va[0]=0xc0003350;va[1]=0xc0003358;va[2]=0xc0003360;
 va[3]=0x8000d8e8;va[4]=0x8000d8f0;va[5]=0x8003d4b8;
 for(unsigned i=0;i<HD540_CACHE_QWORDS;i++)va[6+i]=(uint32_t)WRAPPER_VA+i*8;
 for(unsigned i=0;i<6;i++)va[HD540_CACHE_TRACE+i]=0xc000de00+i*8;
}
int hd540_cache_identity(const uint64_t words[2])
{
 if(!words)return 0;
#ifdef TIMING_OFFLINE_TEST
 if(words[0]==UINT64_C(0x6000000060000000)&&words[1]==words[0])return 1;
#endif
 uint64_t hash=UINT64_C(14695981039346656037);
 for(unsigned i=0;i<2;i++)for(int shift=56;shift>=0;shift-=8)
  hash=(hash^(unsigned char)(words[i]>>shift))*UINT64_C(1099511628211);
 return hash==UINT64_C(0x1b2e94354eb99261);
}
static uint64_t rd(hd540_cache *h,unsigned i)
{return h->transport->read(h->transport->user,h->address[i]);}
static void wr(hd540_cache *h,unsigned i,uint64_t value)
{h->transport->write(h->transport->user,h->address[i],value);}
static int current(hd540_cache *h)
{
 uint32_t va[HD540_CACHE_WORDS];uint64_t pa[HD540_CACHE_WORDS];targets(va);
 if(patch_map_targets(h->transport,va,pa,HD540_CACHE_WORDS)||memcmp(pa,h->address,sizeof(pa)))
  return fail(h,"Native wrapper mappings changed; recovery retained");
 for(unsigned i=1;i<6;i++)if(rd(h,i)!=h->original[i])
  return fail(h,"Native callback identity/neighbor changed; refusing overwrite");
 return 0;
}
int hd540_cache_resolve(hd540_cache *h,patch_context *c)
{
 if(!h||!c)return -1;
 if(h->dirty)return fail(h,"Native wrapper cleanup pending");
 memset(h,0,sizeof(*h));h->transport=c;
 uint32_t va[HD540_CACHE_WORDS];targets(va);
 if(!c->ready||!c->read||!c->write||patch_map_targets(c,va,h->address,HD540_CACHE_WORDS))
  return fail(h,"Native wrapper RAM arena unavailable");
 for(unsigned i=0;i<HD540_CACHE_WORDS;i++)h->original[i]=rd(h,i);
 if(h->original[0]!=UINT64_C(0x8000d8ec)||h->original[1]!=UINT64_C(0xc0012f40)||
    h->original[2]||h->original[5]!=UINT64_C(0xc0003350)||!hd540_cache_identity(h->original+3))
  return fail(h,"Native callback differs from audited 4.93");
 for(unsigned i=6;i<HD540_CACHE_WORDS;i++)if(h->original[i])
  return fail(h,"Audited module-padding arena is not empty; no overwrite");
 h->ready=1;return 0;
}
int hd540_cache_snapshot(hd540_cache *h,uint32_t out[10])
{
 if(!h||!h->ready||current(h))return -1;
 if(!h->attached||rd(h,0)!=WRAPPER_VA)return -1;
 for(unsigned i=0;i<HD540_CACHE_QWORDS;i++)if(rd(h,6+i)!=hd540_cache_words[i])return -1;
 uint64_t a[6],b[6];
 for(unsigned i=0;i<6;i++)a[i]=rd(h,HD540_CACHE_TRACE+i);
 for(unsigned i=0;i<6;i++)b[i]=rd(h,HD540_CACHE_TRACE+i);
 if(memcmp(a,b,sizeof(a)))return 0;
 for(unsigned i=0;i<10;i++)out[i]=(uint32_t)(a[i/2]>>(i&1?0:32));
 if(out[1]>1||a[5])return fail(h,"Native telemetry layout changed");
 return out[1]?0:1;
}
static int clear_arena(hd540_cache *h)
{
 int bad=0;
 for(unsigned i=6;i<HD540_CACHE_TRACE;i++){
  uint64_t value=rd(h,i);
  if(value!=h->original[i]&&value!=hd540_cache_words[i-6]){bad=1;continue;}
  if(value!=h->original[i]){wr(h,i,h->original[i]);if(rd(h,i)!=h->original[i])bad=1;}
 }
 /* Only our checked RW arena is used for telemetry. Hook is detached first
  * and the stock RPC has completed, so no wrapper can still be writing it. */
 for(unsigned i=HD540_CACHE_TRACE;i<HD540_CACHE_WORDS;i++)if(rd(h,i)!=h->original[i]){
  wr(h,i,h->original[i]);if(rd(h,i)!=h->original[i])bad=1;
 }
 if(bad)return fail(h,"Native wrapper arena cleanup unconfirmed");
 h->dirty=0;return 0;
}
int hd540_cache_restore(hd540_cache *h)
{
 if(!h||!h->ready||current(h))return -1;
 uint64_t entry=rd(h,0);
 if(entry==WRAPPER_VA){
  for(unsigned i=0;i<HD540_CACHE_QWORDS;i++)if(rd(h,6+i)!=hd540_cache_words[i])
   return fail(h,"Native wrapper body changed; recovery retained");
  h->attached=1;return 0;
 }
 if(entry!=h->original[0])return fail(h,"Unknown native callback change; recovery retained");
 h->attached=0;
 return h->dirty?clear_arena(h):0;
}
int hd540_cache_finish(hd540_cache *h)
{
 if(!h||!h->ready||current(h))return -1;
 if(!h->dirty)return 1;
 if(rd(h,0)==WRAPPER_VA){
  uint32_t values[10];int rc=hd540_cache_snapshot(h,values);if(rc!=1)return rc;
  wr(h,0,h->original[0]);if(rd(h,0)!=h->original[0])return fail(h,"Native callback restoration unconfirmed");
 }else if(rd(h,0)!=h->original[0])return fail(h,"Native callback identity changed before cleanup");
 h->attached=0;
 return clear_arena(h)?-1:1;
}
int hd540_cache_apply(hd540_cache *h)
{
 if(!h||!h->ready||h->dirty||current(h))return -1;
 for(unsigned i=0;i<HD540_CACHE_WORDS;i++)if(rd(h,i)!=h->original[i])
  return fail(h,"Native wrapper arena changed after inspection; no write");
 h->dirty=1;
 for(unsigned i=0;i<HD540_CACHE_QWORDS;i++){
  wr(h,6+i,hd540_cache_words[i]);
  if(rd(h,6+i)!=hd540_cache_words[i]){(void)hd540_cache_restore(h);return fail(h,"Native wrapper staging failed");}
 }
 wr(h,0,WRAPPER_VA);
 if(rd(h,0)!=WRAPPER_VA){(void)hd540_cache_restore(h);return fail(h,"Native callback attachment failed");}
 h->attached=1;return 0;
}
