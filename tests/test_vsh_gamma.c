#include "../plugin/vsh_gamma.h"
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char ram[0x2000000];
static int calls,fail,drop;
static void put(uint64_t at,uint64_t v)
{assert(!(at&7)&&at+8<=sizeof(ram));for(int i=7;i>=0;i--){ram[at+i]=(unsigned char)v;v>>=8;}}
uint64_t mock_gamma_ram64(uint64_t at)
{assert(!(at&7)&&at>=0x1000&&at+8<=sizeof(ram));uint64_t v=0;for(unsigned i=0;i<8;i++)v=(v<<8)|ram[at+i];return v;}
static void exponent(unsigned channel,float value)
{
 union {float f;uint32_t u;} v={.f=value};uint64_t at=0x1301a8+channel*4;
 uint64_t old=mock_gamma_ram64(at&~UINT64_C(7));
 put(at&~UINT64_C(7),at&4?(old&UINT64_C(0xffffffff00000000))|v.u:((uint64_t)v.u<<32)|(uint32_t)old);
}
int mock_gamma_set(const void *data,unsigned count)
{
 const uint32_t *p=data;assert(count==1&&p[0]==1&&p[1]==7);calls++;
 if(fail)return -7;
 float values[3];memcpy(values,p+2,sizeof(values));
 if(!drop)for(unsigned i=0;i<3;i++)exponent(i,values[i]);
 return 0;
}
int main(void)
{
 /* Disjoint maps and a real-style linked protection-page list. */
 put(0x36cdb0+10*16+8,0x120000);put(0x120000,UINT64_C(10)<<32);put(0x120018,0x121000);
 put(0x121000,0x35ecb8);put(0x121038,0x122000);
 unsigned va[]={0xc000b000,0xc000d000,0xa0001000,0x80005000,0xc000a000};unsigned pa[]={0x160000,0x180000,0x130000,0x190000,0x1a0000};
 for(unsigned i=0;i<5;i++){
  unsigned at=0x122000+i*0x20;put(at,pa[i]);put(at+8,(uint64_t)va[i]<<32);
  put(at+0x10,i==4?0x121028:at+0x20);put(at+0x18,UINT64_C(12)<<56);
 }
 for(unsigned i=0;i<32;i++)ram[0x190e78+i]=(unsigned char)i;
 put(0x1a0f68,0x80038ad8);put(0x1a0f70,0x80038b10);
 put(0x160440,0xc000d4c8);put(0x1804c8,0xa0001000);
 put(0x1300e8,(UINT64_C(3)<<32)|720);put(0x1300f0,(UINT64_C(576)<<32)|3072);
 put(0x1300f8,7);for(unsigned i=0;i<3;i++)exponent(i,1.0f);
 put(0x130228,2);
 float original[3];assert(vsh_gamma_snapshot(original));assert(!vsh_gamma_bind());
 assert(!vsh_gamma_snapshot(original));assert(original[0]==1&&original[1]==1&&original[2]==1);
 assert(!vsh_gamma_set(220));float actual[3];assert(!vsh_gamma_snapshot(actual));
 for(unsigned i=0;i<3;i++)assert(actual[i]==100.0f/220);
 assert(!vsh_gamma_apply(original));assert(!vsh_gamma_snapshot(actual));assert(actual[2]==1);
 int before=calls;assert(vsh_gamma_set(0));put(0x1300f0,UINT64_C(720)<<32);
 assert(vsh_gamma_set(220)&&calls==before);put(0x1300f0,(UINT64_C(480)<<32)|2880);
 put(0x1300f8,8);assert(vsh_gamma_set(220)&&calls==before);put(0x1300f8,7);
 fail=1;assert(vsh_gamma_set(220)==-7);fail=0;drop=1;assert(vsh_gamma_set(220));drop=0;
 /* Nonlinear baseline must survive restoration, including distinct channels. */
 exponent(0,0.9f);exponent(1,1.1f);exponent(2,1.2f);assert(!vsh_gamma_snapshot(original));
 assert(!vsh_gamma_set(220));assert(!vsh_gamma_apply(original));assert(!vsh_gamma_snapshot(actual));
 assert(!memcmp(original,actual,sizeof(actual)));
 /* Gamma readback and native calls must follow a relocated service too. */
 put(0x36cdb0+10*16+8,0);put(0x36cdb0+3*16+8,0x120000);put(0x120000,UINT64_C(3)<<32);
 assert(!vsh_gamma_set(220));assert(!vsh_gamma_apply(original));
 /* Invalid mappings and NaN never reach the native call. */
 before=calls;put(0x130228,0);assert(vsh_gamma_set(220)&&calls==before);put(0x130228,2);
 put(0x1301a8,UINT64_C(0x7fc000003f800000));assert(vsh_gamma_set(220)&&calls==before);
 exponent(0,1.0f);assert(vsh_gamma_apply(NULL)&&vsh_gamma_snapshot(NULL));
 put(0x160440,0x12345678);assert(vsh_gamma_set(220)&&calls==before);
 puts("PASS: analog SD gamma guard, native errors/readback, noncontiguous mapping and exact three-channel restoration");
 return 0;
}
