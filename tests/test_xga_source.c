#include "../experiments/display_probe/xga_source.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint32_t addresses[16]={0x430858,0x430860,0x430868,0x430870,
 0x430938,0x430940,0x430948,0x430950,0x430980,0x430988,0x430990,0x430998,
 0x430690,0x430698,0x4306a0,0x4306a8};
static uint64_t words[16];static unsigned stores,drop_at;static int read_error,uncertain;
static unsigned index_of(uint32_t at)
{for(unsigned i=0;i<16;i++)if(addresses[i]==at)return i;assert(!"unknown address");return 0;}
static int read_word(void *u,uint32_t at,uint64_t *out)
{(void)u;if(read_error)return -1;*out=words[index_of(at)];return 0;}
static int write_word(void *u,uint32_t at,uint64_t value)
{(void)u;unsigned i=index_of(at);assert(i==1||i==5||i==9||i==13||i==15);if(++stores!=drop_at)words[i]=value;return uncertain?-1:0;}
static xga_source s;
static void setup(unsigned native)
{
 memset(&s,0,sizeof(s));stores=drop_at=0;read_error=uncertain=0;
 for(unsigned i=0;i<16;i++)words[i]=UINT64_C(0x6000000060000000);
 int rc=native?xga_source_resolve_native(&s,read_word,write_word,0):xga_source_resolve(&s,read_word,write_word,0);
 assert(!rc&&!stores);
 if(!native){words[1]^=1;s.original[1]=words[1];} /* Authored branch fixture differs from NOP. */
}
int main(void)
{
 for(unsigned native=0;native<2;native++){
  unsigned changes=native?2:1,target=native?5:1;
  setup(native);assert(!xga_source_apply(&s)&&s.dirty&&stores==changes);
  if(native){assert((uint32_t)words[5]==((14u<<26)|1024));assert((uint32_t)words[9]==((14u<<26)|768));}
  if(native){assert(words[13]==s.original[13]&&words[15]==s.original[15]);}
  assert(!xga_source_restore(&s)&&!s.dirty&&stores==2*changes&&!memcmp(words,s.original,sizeof(words)));
  for(unsigned fail=1;fail<=changes;fail++){
   setup(native);drop_at=fail;assert(xga_source_apply(&s)==-1&&!s.dirty&&!memcmp(words,s.original,sizeof(words)));
  }
  setup(native);uncertain=1;assert(xga_source_apply(&s)==-1&&!s.dirty&&!memcmp(words,s.original,sizeof(words)));
  setup(native);assert(!xga_source_apply(&s));drop_at=stores+1;
  assert(xga_source_restore(&s)==-1&&s.dirty);drop_at=0;assert(!xga_source_restore(&s)&&!s.dirty);
  setup(native);words[0]^=1;assert(xga_source_apply(&s)==-1&&!stores);
  setup(native);assert(!xga_source_apply(&s));words[target]=123;
  assert(xga_source_restore(&s)==-1&&s.dirty&&words[target]==123&&stores==changes);
  setup(native);words[2]^=1;memset(&s,0,sizeof(s));assert(xga_source_resolve(&s,read_word,write_word,0)==-1&&!stores);
  setup(native);read_error=1;assert(xga_source_apply(&s)==-1&&!stores);
 }
 puts("PASS: VSH source identity, native dimensions, every failed/uncertain copy, restore retry and foreign-change refusal");
 return 0;
}
