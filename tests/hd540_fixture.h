#ifndef HD540_FIXTURE_H
#define HD540_FIXTURE_H
/* Reuse the relocated-page/transport fixture exercised by the SD suite. */
#define main sd_reference_main
#include "test_patch.c"
#undef main
#include "hd540_patch.h"
#include "hd540_hdmi.h"
#include "hd540_scale.h"
#include "hd540_cache.h"
static hd540_patch hd;
static inline void hdmi_cache(unsigned vic)
{
 unsigned char packet[24]={0x82,2,13};packet[7]=(unsigned char)vic;
 unsigned sum=0;for(unsigned i=0;i<17;i++)sum+=packet[i];packet[3]=(unsigned char)(0u-sum);
 for(unsigned i=0;i<3;i++){
  uint64_t word=0;for(unsigned j=0;j<8;j++)word=(word<<8)|packet[i*8+j];
  put64(0x1c00ef0+i*8,word);
 }
}
static inline void hdmi_fixture(void)
{
 /* Append an unrelated page for the native AVI software cache. */
 put32(0x122414,0x122140);
 put64(0x122140,0x1c00000);put32(0x122148,0xc000c000);
 put32(0x122154,0x122160);ram[0x122158]=12;
 put64(0x122160,0x1d00000);put32(0x122168,0x80010000);
 put32(0x122174,0x122180);ram[0x122178]=12;
 uint32_t extra[]={0x8003f000,0xc0003000,0x8000d000,0x8003d000};
 for(unsigned i=0;i<4;i++){
  uint32_t at=0x122180+i*32;
  put64(at,physical(extra[i]));put32(at+8,extra[i]);
  put32(at+0x14,i==3?0x121028:at+32);ram[at+0x18]=12;
 }
 put64(physical(0xc0003350),0x8000d8ec);put64(physical(0xc0003358),0xc0012f40);
 put64(physical(0x8000d8e8),UINT64_C(0x6000000060000000));
 put64(physical(0x8000d8f0),UINT64_C(0x6000000060000000));
 put64(physical(0x8003d4b8),0xc0003350);
 uint32_t load=(34u<<26)|(9u<<21)|(29u<<16)|2u;
 put64(0x1d00000,(UINT64_C(0x60000000)<<32)|load);
 put64(0x1d00008,UINT64_C(0x6000000060000000));
 put64(physical(0xc000b568),0x8003997b);
 put64(physical(0xc000b560),0xc000cef0);
 /* Deliberately synthetic bytes; only the documented VIC position is 5. */
 put64(physical(0x800399c0),UINT64_C(0x123456789a05bcde));
 put64(physical(0x800399c8),UINT64_C(0x0123456789abcdef));
 hdmi_cache(5);
}
static void cache_hd(const uint64_t *record)
{
 for(unsigned i=0;i<7;i++)put64(physical(0xa0023f88+i*8),record[i+3]);
 put64(physical(0xa0024000),(UINT64_C(7)<<32)|1920);
 put64(physical(0xa0024008),(UINT64_C(1080)<<32)|7680);
 put64(physical(0xa0024168),UINT64_C(2)<<32);
}
static void hd_setup(void)
{
 setup();memset(&hd,0,sizeof(hd));
 /* Numeric mode fixture, independently expressed as timing parameters. */
 uint32_t fields[]={1919,2199,2007,2051,1079,1124,1085,1084,1094,
  (929u<<16)|907,(907u<<16)|863,(1094u<<16)|1084,0,0x11003111};
 for(unsigned k=0;k<2;k++){
  uint32_t p=7+k;
  put64(physical(0x80039148+p*24),((uint64_t)(k?0x426fc28fu:0x42700000u)<<32)|1);
  put64(physical(0x80039150+p*24),(UINT64_C(1)<<32)|1920);
  put64(physical(0x80039158+p*24),UINT64_C(1080)<<32);
  for(unsigned w=0;w<7;w++)put64(physical(0x80038b28+p*56+w*8),((uint64_t)fields[w*2]<<32)|fields[w*2+1]);
 }
 assert(!hd540_patch_resolve(&hd,&c));cache_hd(hd.original+10);hdmi_fixture();
 /* Self-authored neighboring code fixture, retaining the legacy scaler
  * baseline words already installed by setup(). */
 uint64_t nop=UINT64_C(0x6000000060000000);
 put64(physical(0x80002f50),nop);
 uint32_t or_word=(31u<<26)|(4u<<11)|(444u<<1);
 put64(physical(0x80003058),((uint64_t)or_word<<32)|0x60000000u);
 put64(physical(0x80003060),nop);put64(physical(0x80003108),nop);
 put64(physical(0x800036c8),nop);put64(physical(0x800036d0),nop);
}
static void hd_stock(void)
{for(unsigned i=0;i<HD540_WORDS;i++)assert(get(NULL,hd.address[i])==hd.original[i]);is_stock();}
#endif
