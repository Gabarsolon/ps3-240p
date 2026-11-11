/* Expose the numeric plan to an optional private-input comparison. */
#include "lv1_patch.h"
#include <string.h>
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif
static uint64_t read_word(void *u,uint64_t a)
{
 (void)u;
 if(a==0x36cdb0+10*16+8)return 0x120000;
 if(a==0x120000)return UINT64_C(10)<<32;
 if(a==0x120018)return 0x121000;
 if(a==0x121000)return 0x35ecb8;
 if(a==0x121038)return 0x122000;
 if(a==0x122000)return 0x1000000;
 if(a==0x122008)return UINT64_C(0x80000000)<<32;
 if(a==0x122010)return 0x122020;
 if(a==0x122018)return UINT64_C(18)<<56;
 if(a==0x122020)return 0x1100000;
 if(a==0x122028)return UINT64_C(0xc0000000)<<32;
 if(a==0x122030)return 0x121028;
 if(a==0x122038)return UINT64_C(16)<<56;
 return 0;
}
static void no_write(void *u,uint64_t a,uint64_t v){(void)u;(void)a;(void)v;}
EXPORT int audit_word(int index,uint32_t *va,uint64_t *word)
{
 patch_context c;memset(&c,0,sizeof(c));c.read=read_word;c.write=no_write;
 (void)patch_resolve(&c);
 if(index<0||index>=PATCH_WORDS||!c.address[index])return -1;
 uint64_t p=c.address[index];
 *va=(uint32_t)(p>=0x1100000?0xc0000000u+p-0x1100000:0x80000000u+p-0x1000000);
 *word=c.stock[index];return 0;
}
