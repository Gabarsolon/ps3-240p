/* SPDX-License-Identifier: MIT. All MMIO accesses stay inside Sony's AV API.
 * The only LV1 operation below is a bounded ordinary-RAM read. Evidence and
 * the fingerprint's function boundaries: docs/GAMMA.md. No Sony code copied.
 */
#include "vsh_gamma.h"
#include "vsh_video.h"
#include <stdint.h>
#ifndef VSH_GAMMA_HOST_TEST
#include <ppu-lv2.h>
#else
extern uint64_t mock_gamma_ram64(uint64_t);
extern int mock_gamma_set(const void *,unsigned);
#endif

static uint32_t setter_opd[2] __attribute__((aligned(8)));
typedef struct {uint32_t head,mask;float exponent[3];uint32_t reserved[29];} lut_request;
_Static_assert(sizeof(lut_request)==0x88,"Native AV LUT record size");
static uint64_t ram64(uint64_t a)
{
 if(a<0x1000||a>0x1fffff8||(a&7))return 0;
#ifndef VSH_GAMMA_HOST_TEST
 lv2syscall3(8,0x7777,0x1008,a);return_to_user_prog(uint64_t);
#else
 return mock_gamma_ram64(a);
#endif
}
static uint32_t ram32(uint64_t a)
{uint64_t v=ram64(a&~UINT64_C(7));return a&4?(uint32_t)v:(uint32_t)(v>>32);}
typedef struct {uint64_t physical,size;uint32_t va;} page;
static int address(const page *pages,unsigned count,uint32_t va,uint64_t *physical)
{
 unsigned found=0;
 for(unsigned i=0;i<count;i++)if(va>=pages[i].va&&(uint64_t)va-pages[i].va+8<=pages[i].size){
  *physical=pages[i].physical+va-pages[i].va;found++;
 }
 return found==1&&*physical>=0x1000&&*physical<=0x1fffff8&&!(*physical&7)?0:-1;
}
static int read_exponents(float out[3],int require_sd)
{
 page pages[512];unsigned count=0;
 uint64_t proc=ram64(0x36cdb0+10*16+8);
 if(ram32(proc)!=10)return -1;
 uint64_t domain=ram64(proc+0x18);if(ram64(domain)!=0x35ecb8)return -1;
 uint64_t sentinel=domain+0x28,node=ram32(domain+0x3c),seen[512];
 while(node!=sentinel){
  if(count==512||node<0x1000||node>0x1ffffd0||(node&7))return -1;
  for(unsigned i=0;i<count;i++)if(seen[i]==node)return -1;
  seen[count]=node;unsigned shift=(unsigned)(ram64(node+0x18)>>56);
  if(shift<12||shift>25)return -1;
  page *p=&pages[count++];p->physical=ram64(node);p->va=ram32(node+8);p->size=UINT64_C(1)<<shift;
  node=ram32(node+0x14);
 }
 uint64_t pa;
 if(address(pages,count,0xc000b440,&pa)||ram64(pa)!=0xc000d4c8)return -1;
 if(address(pages,count,0xc000d4c8,&pa))return -1;
 uint64_t ctx=ram64(pa);if(ctx<0x1000||ctx>0xfffffc00||(ctx&7))return -1;
 if(require_sd){
  /* Head 1's actual request, not the public API's remembered XMB mode. */
  if(address(pages,count,(uint32_t)ctx+0x228,&pa)||(uint32_t)ram64(pa)!=2)return -1;
  if(address(pages,count,(uint32_t)ctx+0xe8,&pa))return -1;
  unsigned width=(unsigned)ram64(pa)&65535u;
  if(address(pages,count,(uint32_t)ctx+0xf0,&pa))return -1;
  unsigned height=(unsigned)(ram64(pa)>>32)&65535u;
  if(width!=720||(height!=480&&height!=576))return -1;
  if(address(pages,count,(uint32_t)ctx+0xf8,&pa))return -1;
  if((uint32_t)ram64(pa)!=7)return -1; /* verified analog RGB8 format */
 }
 union {uint32_t u;float f;} v;
 for(unsigned i=0;i<3;i++){
  uint32_t va=(uint32_t)ctx+0x1a8+i*4;
  if(address(pages,count,va&~7u,&pa))return -1;
  uint64_t word=ram64(pa);v.u=va&4?(uint32_t)word:(uint32_t)(word>>32);out[i]=v.f;
  if(!(out[i]>=0.05f&&out[i]<=10.0f))return -1;
 }
 return 0;
}
int vsh_gamma_bind(void)
{
#ifndef VSH_GAMMA_HOST_TEST
 /* Adjacent, exported SDK anchor gives the relocated text and TOC. Verify
  * the COMPLETE internal wrapper before constructing a callable descriptor. */
 uint32_t anchor=vsh_video_export("sdk",0xd7a665f4),opd[2];unsigned char code[0x98];
 if(!anchor||vsh_video_copy(anchor,opd,sizeof(opd))||opd[0]<0x10098)return -1;
 uint32_t at=opd[0]-0x98;
 if(vsh_video_copy(at,code,sizeof(code)))return -1;
 uint64_t hash=UINT64_C(14695981039346656037);
 for(unsigned i=0;i<sizeof(code);i++)hash=(hash^code[i])*UINT64_C(1099511628211);
 if(hash!=UINT64_C(0x117e63857c651649))return -1; /* Filled from the privately audited input. */
 setter_opd[0]=at;setter_opd[1]=opd[1];return 0;
#else
 setter_opd[0]=1;return 0;
#endif
}
int vsh_gamma_snapshot(float original[3])
{return setter_opd[0]&&original?read_exponents(original,1):-1;}
int vsh_gamma_apply(const float exponent[3])
{
 float before[3];if(!exponent||!setter_opd[0]||read_exponents(before,1))return -1;
 lut_request request={0};request.head=1;request.mask=7;
 for(unsigned i=0;i<3;i++){
  if(!(exponent[i]>=0.05f&&exponent[i]<=10.0f))return -1;
  request.exponent[i]=exponent[i];
 }
#ifndef VSH_GAMMA_HOST_TEST
 int (*set)(const lut_request *,unsigned)=(void *)(uintptr_t)setter_opd;
 int rc=set(&request,1);
#else
 int rc=mock_gamma_set(&request,1);
#endif
 if(rc)return rc;
 float actual[3];if(read_exponents(actual,1))return -1;
 for(unsigned i=0;i<3;i++)if(actual[i]!=exponent[i])return -1;
 return 0;
}
int vsh_gamma_set(unsigned value)
{
 if(value<50||value>400)return -1;
 float exponent[3]={100.0f/value,100.0f/value,100.0f/value};return vsh_gamma_apply(exponent);
}
