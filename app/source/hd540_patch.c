/* SPDX-License-Identifier: MIT. Only twelve ordinary-RAM words may be changed.
 * No instruction, clock, encoder, persistent setting or MMIO writes.
 * Full-source/progressive-fetch 540-line timing is an UNVERIFIED hypothesis.
 */
#include "hd540_patch.h"
#include <stdio.h>
#include <string.h>

static int fail(hd540_patch *p,const char *why)
{snprintf(p->error,sizeof(p->error),"%s",why);if(p->transport&&p->transport->log)p->transport->log("HD540: %s",why);return -1;}
uint64_t hd540_record_hash(const uint64_t words[10])
{
 uint64_t hash=UINT64_C(14695981039346656037);
 for(unsigned i=0;i<10;i++)for(int shift=56;shift>=0;shift-=8)
  hash=(hash^(unsigned char)(words[i]>>shift))*UINT64_C(1099511628211);
 return hash;
}
int hd540_record_valid(unsigned index,const uint64_t words[10])
{
 const uint64_t identity[2]={UINT64_C(0xd0284e11a4e5c0bb),UINT64_C(0xa165428986605bf9)};
 return index<2&&words&&hd540_record_hash(words)==identity[index];
}
static int mappings_current(hd540_patch *p)
{
 uint32_t va[HD540_WORDS];uint64_t pa[HD540_WORDS];
 for(unsigned k=0;k<2;k++)for(unsigned w=0;w<10;w++)
  va[k*10+w]=w<3?0x80039148u+(7+k)*24u+w*8u:0x80038b28u+(7+k)*56u+(w-3)*8u;
 if(patch_map_targets(p->transport,va,pa,HD540_WORDS)||memcmp(pa,p->address,sizeof(pa)))
  return fail(p,"Service RAM mappings changed; refusing writes to old physical pages");
 return 0;
}
int hd540_patch_resolve(hd540_patch *p,patch_context *c)
{
 if(!p||!c)return -1;
 if(p->dirty)return fail(p,"Recovery pending; cannot replace the saved timing snapshot");
 memset(p,0,sizeof(*p));p->transport=c;
 if(!c->ready||!c->read||!c->write||patch_state(c,0)!=STATE_STOCK||patch_state(c,1)!=STATE_STOCK)
  return fail(p,"Start from stock timing; disable SD enforcement and reboot first");
 for(unsigned i=42;i<44;i++)if(c->read(c->user,c->address[i])!=c->stock[i])
  return fail(p,"Retired scaler experiment is still patched; reboot first");
 uint32_t va[HD540_WORDS];
 for(unsigned k=0;k<2;k++)for(unsigned w=0;w<10;w++)
  va[k*10+w]=w<3?0x80039148u+(7+k)*24u+w*8u:0x80038b28u+(7+k)*56u+(w-3)*8u;
 if(patch_map_targets(c,va,p->address,HD540_WORDS))return fail(p,c->error);
 for(unsigned i=0;i<HD540_WORDS;i++)p->original[i]=c->read(c->user,p->address[i]);
 /* Fingerprints of complete numeric records; audited against private 4.93.
  * These fingerprints do not embed Sony code or a firmware record dump. */
 for(unsigned k=0;k<2;k++)if(!hd540_record_valid(k,p->original+k*10))
  return fail(p,"1080i timing record identity differs from audited 4.93 stock data");
 memcpy(p->candidate,p->original,sizeof(p->candidate));
 for(unsigned k=0;k<2;k++){
  /* Independent progressive modeline: 1920x540, totals 2200x562,
   * physical VSYNC 542..547, positive H/V. Raster counters are zero-based.
   * Keep full source dimensions for validator/surface safety. Source-window
   * fitting is still unverified; do not claim a complete picture from cache.
   * Progressive fetching produced detected 540p in dev11. Field fetching
   * made the same raster black in dev12. Source-window code is a separate
   * guarded experiment; this module does not write any instructions.
   * Select FP SCALE rather than CENTER so the source-window scaler can run.
   * Preserve stock clock/horizontal timing and all other HD control bits. */
  p->candidate[k*10]=p->original[k*10]&UINT64_C(0xffffffff00000000);
  p->candidate[k*10+5]=(UINT64_C(539)<<32)|561u;
  p->candidate[k*10+6]=(UINT64_C(542)<<32)|541u;
  p->candidate[k*10+7]=(p->original[k*10+7]&UINT64_C(0xffffffff))|(UINT64_C(546)<<32);
  p->candidate[k*10+8]=(p->original[k*10+8]&UINT64_C(0xffffffff00000000))|((uint64_t)546<<16)|541u;
  p->candidate[k*10+9]=p->original[k*10+9]&~UINT64_C(0x300);
 }
 p->ready=1;return 0;
}
int hd540_patch_restore(hd540_patch *p)
{
 if(!p||!p->ready)return -1;
 if(mappings_current(p))return -1;
 int failed=0;patch_context *c=p->transport;
 for(unsigned i=0;i<HD540_WORDS;i++)if(p->candidate[i]!=p->original[i]){
  uint64_t live=c->read(c->user,p->address[i]);
  if(live==p->original[i])continue;
  if(live!=p->candidate[i]){failed=1;continue;} /* Do not overwrite third-party data. */
  c->write(c->user,p->address[i],p->original[i]);
  if(c->read(c->user,p->address[i])!=p->original[i])failed=1;
 }
 if(failed)return fail(p,"Timing RAM recovery incomplete; retry armed, reboot if necessary");
 p->dirty=0;return 0;
}
int hd540_patch_apply(hd540_patch *p)
{
 if(!p||!p->ready||p->dirty)return -1;
 if(mappings_current(p))return -1;
 patch_context *c=p->transport;
 for(unsigned i=0;i<HD540_WORDS;i++)if(c->read(c->user,p->address[i])!=p->original[i])
  return fail(p,"Timing changed after inspection; no patch applied");
 p->dirty=1;
 for(unsigned i=0;i<HD540_WORDS;i++)if(p->candidate[i]!=p->original[i]){
  c->write(c->user,p->address[i],p->candidate[i]);
  if(c->read(c->user,p->address[i])!=p->candidate[i]){
   (void)hd540_patch_restore(p);return fail(p,"Timing write/readback failed; recovery requested");
  }
 }
 return 0;
}
int hd540_patch_cache(hd540_patch *p,int candidate)
{
 if(!p||!p->ready)return -1;
 patch_cached_video v;
 if(patch_read_cached_video(p->transport,&v))return -1;
 const patch_cached_head *h=&v.head[0];
 if(h->state!=2||h->width!=1920||h->height!=1080||(h->vid!=7&&h->vid!=0x12))return 0;
 unsigned k=h->vid==0x12?0:1;
 return !memcmp(h->raster,(candidate?p->candidate:p->original)+k*10+3,sizeof(h->raster));
}
