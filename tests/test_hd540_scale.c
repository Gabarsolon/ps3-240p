#include "hd540_fixture.h"
int main(void)
{
 hd540_scale s={0};hd_setup();assert(!hd540_scale_resolve(&s,&c));
 assert(!stores&&hd540_scale_check(&s,0)==1&&hd540_scale_check(&s,1)==0);
 unsigned changed=0;
 for(unsigned i=0;i<HD540_SCALE_WORDS;i++)changed+=s.candidate[i]!=s.original[i];
 assert(changed==4);
 uint32_t enable=(uint32_t)s.candidate[0],window=(uint32_t)(s.candidate[2]>>32),factor=(uint32_t)s.candidate[4];
 assert(enable>>26==24&&((enable>>21)&31)==0&&((enable>>16)&31)==3&&(enable&65535)==0x1010);
 assert(window>>26==24&&((window>>21)&31)==0&&((window>>16)&31)==0&&(window&65535)==539);
 assert(factor>>26==15&&((factor>>21)&31)==0&&((factor>>16)&31)==0&&(factor&65535)==0x1fff);
 uint32_t pitch=(uint32_t)(s.candidate[6]>>32);
 assert(pitch>>26==21&&((pitch>>21)&31)==5&&((pitch>>16)&31)==5&&
        ((pitch>>11)&31)==1&&((pitch>>6)&31)==0&&((pitch>>1)&31)==28&&!(pitch&1));
 assert((uint32_t)s.candidate[6]==(uint32_t)s.original[6]);
 assert((s.candidate[0]>>32)==(s.original[0]>>32)&&(uint32_t)s.candidate[2]==(uint32_t)s.original[2]&&
        (s.candidate[4]>>32)==(s.original[4]>>32));
 assert(!hd540_scale_apply(&s)&&stores==4&&s.dirty&&hd540_scale_check(&s,1)==1);
 assert(!hd540_scale_restore(&s)&&stores==8&&!s.dirty&&hd540_scale_check(&s,0)==1);hd_stock();
 for(int failure=1;failure<=4;failure++){
  hd_setup();memset(&s,0,sizeof(s));assert(!hd540_scale_resolve(&s,&c));drop_store=failure;
  assert(hd540_scale_apply(&s)==-1&&!s.dirty);hd_stock();
 }
 hd_setup();memset(&s,0,sizeof(s));assert(!hd540_scale_resolve(&s,&c));
 assert(!hd540_scale_apply(&s));drop_store=stores+1;
 assert(hd540_scale_restore(&s)==-1&&s.dirty);
 drop_store=0;assert(!hd540_scale_restore(&s)&&!s.dirty);hd_stock();
 hd_setup();memset(&s,0,sizeof(s));assert(!hd540_scale_resolve(&s,&c));
 put64(s.address[1],s.original[1]^1);assert(hd540_scale_apply(&s)==-1&&!stores);
 hd_setup();memset(&s,0,sizeof(s));assert(!hd540_scale_resolve(&s,&c));
 assert(!hd540_scale_apply(&s));put64(s.address[2],0x1234);
 assert(hd540_scale_restore(&s)==-1&&s.dirty&&get(NULL,s.address[2])==0x1234);
 hd_setup();memset(&s,0,sizeof(s));assert(!hd540_scale_resolve(&s,&c));
 put64(0x1220e0,0x1140000);assert(hd540_scale_apply(&s)==-1&&!stores);
 puts("PASS: scaler source-window ISA/scope, all failed stores, checked rollback/retry, changed mappings and unknown neighbor/code refusal");
 return 0;
}
