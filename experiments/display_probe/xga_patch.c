/* SPDX-License-Identifier: MIT. No clock, encoder, MMIO, flash or boot writes.
 * This is a non-DMT 1024x768 experiment using the tested WXGA clock/totals.
 */
#include "xga_patch.h"
#include <stdio.h>
#include <string.h>
static const uint32_t targets[XGA_WORDS]={
 0x800392c8,0x800392d0,0x800392d8,
 0x80038ea8,0x80038eb0,0x80038eb8,0x80038ec0,0x80038ec8,0x80038ed0,0x80038ed8,
 0x80002d70,0x80002e08,0x80002f98,0x80003030,0x80003058,0x80003078,0x800030a0,0x80003100,0x80025fc8,0x80002e18};
static int fail(xga_patch *p,const char *why){snprintf(p->error,sizeof(p->error),"%s",why);return -1;}
static int identity(const uint64_t *w)
{
 uint64_t h=UINT64_C(14695981039346656037);
 for(unsigned i=0;i<XGA_WORDS;i++)for(int shift=56;shift>=0;shift-=8)h=(h^(unsigned char)(w[i]>>shift))*UINT64_C(1099511628211);
#ifdef XGA_PATCH_HOST_TEST
 if(h==UINT64_C(0x3f8aadf096883fa5))return 1; /* All-NOP author-created host fixture only. */
#endif
 return h==UINT64_C(0x2d4f977b059d09a6);
}
static int current(xga_patch *p)
{
 uint64_t mapped[XGA_WORDS];
 if(patch_map_targets(p->transport,targets,mapped,XGA_WORDS)||memcmp(mapped,p->address,sizeof(mapped)))return fail(p,"XGA service mappings changed");
 return 0;
}
static uint64_t low(uint64_t w,uint32_t value){return (w&UINT64_C(0xffffffff00000000))|value;}
static uint64_t high(uint64_t w,uint32_t value){return ((uint64_t)value<<32)|(uint32_t)w;}
static uint32_t ori(unsigned destination,unsigned source,unsigned immediate){return (24u<<26)|(source<<21)|(destination<<16)|immediate;}
static uint32_t lis(unsigned destination,unsigned immediate){return (15u<<26)|(destination<<21)|immediate;}
int xga_patch_resolve(xga_patch *p,patch_context *c,unsigned geometry)
{
 if(!p||!c)return -1;
 if(p->dirty)return fail(p,"XGA recovery still pending");
 memset(p,0,sizeof(*p));p->transport=c;p->geometry=geometry;
 if(geometry>5||!c->ready||!c->read||!c->write||patch_state(c,0)!=STATE_STOCK||patch_state(c,1)!=STATE_STOCK)return fail(p,"Start with stock CFW timing");
 if(patch_map_targets(c,targets,p->address,XGA_WORDS))return fail(p,"XGA service RAM mappings unavailable");
 for(unsigned i=0;i<XGA_WORDS;i++)p->original[i]=c->read(c->user,p->address[i]);
 if(!identity(p->original))return fail(p,"WXGA timing/scaler identity differs from audited 4.93");
 memcpy(p->candidate,p->original,sizeof(p->candidate));
 /* Keep the clock, all sync edges/totals, full 1280x720 source bounds and
  * metadata unchanged. Only active horizontal end becomes 1023. */
 p->candidate[3]=high(p->original[3],1023);
 if(geometry){
  /* Match the output metadata to the smaller active raster. This describes
   * output geometry; the independently checked source stays1280x720. */
  p->candidate[1]=low(p->original[1],1024);
  /* A1024x768 VSH source needs no scaler/policy instruction edits. */
  if(geometry==5){p->ready=1;return 0;}
  /* Bound the source width to the1280 pixels already required by this
   * backend. Retain the native rejection branch, height/pitch checks and
   * every adjacent instruction; do not disable validation wholesale. */
  if(geometry<3)p->candidate[18]=high(p->original[18],(10u<<26)|(7u<<23)|(11u<<16)|1280u);
  /* Geometry1 stretches to1024x768. Geometry2 preserves the16:9 source in
   * a1024x576 box, centered vertically at96. Supplied step values are
   * provisional; actual geometry must be validated independently.
   * These are original authored PPC encodings; surrounding words are saved
   * only at runtime and protected by the complete source fingerprint. */
  int aspect=geometry==2||geometry==4;
  unsigned top=aspect?96:0,bottom=aspect?671:767;
  unsigned xstep=geometry>=3?0xb40:0x1400;
  unsigned ystep=geometry>=3?(aspect?0xd55:0xa00):(aspect?0x1400:0xf00);
  p->candidate[9]=p->original[9]&~UINT64_C(0x300); /* Native FP SCALE policy. */
  p->candidate[10]=low(p->original[10],lis(25,xstep));
  p->candidate[11]=high(p->original[11],lis(25,xstep));
  p->candidate[19]=high(p->original[19],lis(25,xstep));
  /* Both documented axis enables, preserving the native horizontal policy
   * derived in r8. Do not force the unverified extra vertical bit6. */
  p->candidate[12]=low(p->original[12],ori(3,8,0x1111));
  p->candidate[13]=low(p->original[13],ori(0,0,top));
  p->candidate[14]=high(p->original[14],ori(0,0,bottom));
  p->candidate[15]=low(p->original[15],ori(0,0,0));
  p->candidate[16]=high(p->original[16],ori(0,0,1023));
  p->candidate[17]=low(p->original[17],lis(0,ystep));
 }
 p->ready=1;return 0;
}
int xga_patch_restore(xga_patch *p)
{
 if(!p||!p->ready||current(p))return -1;
 int failed=0;patch_context *c=p->transport;
 for(unsigned i=0;i<XGA_WORDS;i++)if(p->candidate[i]!=p->original[i]){
  uint64_t live=c->read(c->user,p->address[i]);
  if(live==p->original[i])continue;
  if(live!=p->candidate[i]){failed=1;continue;}
  c->write(c->user,p->address[i],p->original[i]);
  if(c->read(c->user,p->address[i])!=p->original[i])failed=1;
 }
 if(failed)return fail(p,"XGA restoration unconfirmed; retry required");
 p->dirty=0;return 0;
}
int xga_patch_apply(xga_patch *p)
{
 if(!p||!p->ready||p->dirty||current(p))return -1;
 patch_context *c=p->transport;
 for(unsigned i=0;i<XGA_WORDS;i++)if(c->read(c->user,p->address[i])!=p->original[i])return fail(p,"XGA source changed after inspection");
 p->dirty=1;
 for(unsigned i=0;i<XGA_WORDS;i++)if(p->candidate[i]!=p->original[i]){
  c->write(c->user,p->address[i],p->candidate[i]);
  if(c->read(c->user,p->address[i])!=p->candidate[i]){(void)xga_patch_restore(p);return fail(p,"XGA write/readback failed; recovery requested");}
 }
 return 0;
}
int xga_patch_cache(xga_patch *p,int candidate)
{
 if(!p||!p->ready)return -1;
 patch_cached_video v;if(patch_read_cached_video(p->transport,&v))return -1;
 const patch_cached_head *h=&v.head[0];
 unsigned width=p->geometry==5?1024:p->geometry>=3?720:1280;
 unsigned height=p->geometry==5?768:p->geometry>=3?480:720;
 if(h->state!=2||h->width!=width||h->height!=height||h->vid!=0xd)return 0;
 return !memcmp(h->raster,(candidate?p->candidate:p->original)+3,sizeof(h->raster));
}
