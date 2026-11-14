/* SPDX-License-Identifier: MIT. Temporary RAM code; no MMIO/clocks/flash writes. */
#include "hd540_hdmi.h"
#include <stdio.h>
#include <string.h>

/* The shared template also enters the transmitter command (dev7 failed
 * its stock control). Override only the AVI builder's byte load instead.
 * Original Sony instructions are saved at runtime, never embedded here.
 * Packet checksum/readback establish execution; RAM readback alone cannot. */
static const uint32_t targets[HD540_HDMI_TARGETS]={
 0x80010000u,0x80010008u,0x800399c0u,0x800399c8u,0xc000b568u,0xc000b560u,
 0xc000cef0u,0xc000cef8u,0xc000cf00u};
static int fail(hd540_hdmi *h,const char *why)
{snprintf(h->error,sizeof(h->error),"%s",why);return -1;}
static uint64_t fingerprint(const uint64_t words[2])
{
 uint64_t hash=UINT64_C(14695981039346656037);
 for(unsigned i=0;i<2;i++)for(int shift=56;shift>=0;shift-=8)
  hash=(hash^(unsigned char)(words[i]>>shift))*UINT64_C(1099511628211);
 return hash;
}
int hd540_hdmi_identity(const uint64_t words[2])
{
 if(!words)return 0;
#ifdef TIMING_OFFLINE_TEST
 uint32_t load=(34u<<26)|(9u<<21)|(29u<<16)|2u;
 if(words[0]==((UINT64_C(0x60000000)<<32)|load)&&words[1]==UINT64_C(0x6000000060000000))return 1;
#endif
 return fingerprint(words)==UINT64_C(0xd091a35e6fbfce75);
}
int hd540_hdmi_template_identity(const uint64_t words[2])
{
 if(!words)return 0;
#ifdef TIMING_OFFLINE_TEST
 if(words[0]==UINT64_C(0x123456789a05bcde)&&words[1]==UINT64_C(0x0123456789abcdef))return 1;
#endif
 return fingerprint(words)==UINT64_C(0x04c9600504867132);
}
static uint64_t candidate_word(const hd540_hdmi *h)
{
 /* Self-authored PPC: add immediate zero from architectural zero into
  * the original byte-load destination; adjacent instruction is preserved. */
 uint32_t destination=((uint32_t)h->original[0]>>21)&31u;
 uint32_t zero=(14u<<26)|(destination<<21);
 return (h->original[0]&UINT64_C(0xffffffff00000000))|zero;
}
static uint64_t read_word(hd540_hdmi *h,unsigned i)
{patch_context *c=h->transport;return c->read(c->user,h->address[i]);}
static int current(hd540_hdmi *h)
{
 uint64_t pa[HD540_HDMI_TARGETS];
 if(patch_map_targets(h->transport,targets,pa,HD540_HDMI_TARGETS)||memcmp(pa,h->address,sizeof(pa)))
  return fail(h,"HDMI RAM mappings changed; refusing stale addresses");
 if(read_word(h,4)!=UINT64_C(0x8003997b)||read_word(h,5)!=UINT64_C(0xc000cef0))
  return fail(h,"4.93 HDMI template/cache pointer identity changed");
 return 0;
}
int hd540_hdmi_resolve(hd540_hdmi *h,patch_context *c)
{
 if(!h||!c)return -1;
 if(h->dirty)return fail(h,"HDMI template recovery is still pending");
 memset(h,0,sizeof(*h));h->transport=c;
 if(!c->ready||!c->read||!c->write)return fail(h,"Audited service RAM transport unavailable");
 if(patch_map_targets(c,targets,h->address,HD540_HDMI_TARGETS))return fail(h,c->error);
 if(current(h))return -1;
 for(unsigned i=0;i<4;i++)h->original[i]=read_word(h,i);
 uint32_t load=(uint32_t)h->original[0];
 if(!hd540_hdmi_identity(h->original)||load>>26!=34||((load>>21)&31)!=9||((load>>16)&31)!=29||(load&65535)!=2)
  return fail(h,"AVI builder differs from audited 4.93 code");
 if(!hd540_hdmi_template_identity(h->original+2))
  return fail(h,"1080i60 HDMI template differs from audited stock");
 h->ready=1;return 0;
}
int hd540_hdmi_apply(hd540_hdmi *h)
{
 if(!h||!h->ready||h->dirty)return -1;
 if(current(h))return -1;
 for(unsigned i=0;i<4;i++)if(read_word(h,i)!=h->original[i])
  return fail(h,"AVI code/template changed after inspection; no write");
 h->dirty=1;
 uint64_t candidate=candidate_word(h);
 h->transport->write(h->transport->user,h->address[0],candidate);
 if(read_word(h,0)!=candidate){(void)hd540_hdmi_restore(h);return fail(h,"AVI code write/readback failed");}
 return 0;
}
int hd540_hdmi_restore(hd540_hdmi *h)
{
 if(!h||!h->ready)return -1;
 if(current(h))return -1;
 for(unsigned i=1;i<4;i++)if(read_word(h,i)!=h->original[i])return fail(h,"AVI neighboring code/template changed; recovery pending");
 uint64_t value=read_word(h,0);
 if(value!=h->original[0]){
  if(value!=candidate_word(h))return fail(h,"Unknown AVI code change; refusing overwrite");
  h->transport->write(h->transport->user,h->address[0],h->original[0]);
  if(read_word(h,0)!=h->original[0])return fail(h,"AVI code restoration not confirmed");
 }
 h->dirty=0;return 0;
}
int hd540_hdmi_vic(hd540_hdmi *h)
{
 if(!h||!h->ready||current(h))return -1;
 if(read_word(h,0)!=(h->dirty?candidate_word(h):h->original[0]))return -1;
 for(unsigned i=1;i<4;i++)if(read_word(h,i)!=h->original[i])return -1;
 uint64_t a[3],b[3];
 for(unsigned i=0;i<3;i++)a[i]=read_word(h,6+i);
 for(unsigned i=0;i<3;i++)b[i]=read_word(h,6+i);
 if(memcmp(a,b,sizeof(a)))return -2;
 unsigned char packet[24];
 for(unsigned i=0;i<3;i++)for(unsigned j=0;j<8;j++)packet[i*8+j]=(unsigned char)(a[i]>>(56-j*8));
 if(packet[0]!=0x82||packet[1]!=2||packet[2]!=13)return -2;
 unsigned sum=0;for(unsigned i=0;i<17;i++)sum+=packet[i];
 if(sum&0xff)return -2;
 return packet[7]&0x7f;
}
int hd540_hdmi_cache(hd540_hdmi *h,unsigned expected)
{
 int vic=hd540_hdmi_vic(h);
 return vic==-1?-1:vic>=0&&(unsigned)vic==expected;
}
