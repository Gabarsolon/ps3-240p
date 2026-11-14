#include "hd540_fixture.h"
int main(void)
{
 hd_setup();assert(!stores&&hd540_patch_cache(&hd,0)==1&&hd540_patch_cache(&hd,1)==0);
 assert(!hd540_patch_apply(&hd)&&stores==12&&hd.dirty);is_stock();
 assert(hd540_patch_cache(&hd,1)==0); /* RAM writes alone do not update the cache. */
 cache_hd(hd.candidate+10);assert(hd540_patch_cache(&hd,1)==1);
 for(unsigned k=0;k<2;k++){
  assert(hd.candidate[k*10+5]==((UINT64_C(539)<<32)|561));
  assert(hd.candidate[k*10+6]==((UINT64_C(542)<<32)|541));
  assert((hd.candidate[k*10+7]>>32)==546);
  assert((uint32_t)hd.candidate[k*10+8]==((546u<<16)|541));
  assert(hd.candidate[k*10+9]==(hd.original[k*10+9]&~UINT64_C(0x300)));
  assert(!(uint32_t)hd.candidate[k*10]); /* Progressive fetching retained. */
  assert((hd.candidate[k*10]>>32)==(hd.original[k*10]>>32)); /* Rate unchanged. */
  assert(hd.candidate[k*10+1]==hd.original[k*10+1]&&hd.candidate[k*10+2]==hd.original[k*10+2]); /* Full source dimensions retained. */
 }
 uint64_t saved=hd.original[5];assert(hd540_patch_resolve(&hd,&c)==-1&&hd.original[5]==saved);
 assert(!hd540_patch_restore(&hd)&&!hd.dirty&&stores==24);hd_stock();
 assert(hd540_patch_cache(&hd,0)==0);cache_hd(hd.original+10);assert(hd540_patch_cache(&hd,0)==1);
 for(int failure=1;failure<=12;failure++){
  hd_setup();drop_store=failure;assert(hd540_patch_apply(&hd)==-1&&!hd.dirty);hd_stock();
 }
 hd_setup();assert(!hd540_patch_apply(&hd));drop_store=stores+1;
 assert(hd540_patch_restore(&hd)==-1&&hd.dirty);
 drop_store=0;assert(!hd540_patch_restore(&hd));hd_stock();
 hd_setup();put64(hd.address[2],hd.original[2]^1);assert(hd540_patch_apply(&hd)==-1&&!stores&&!hd.dirty);
 hd_setup();put64(0x122020,0x1140000);assert(hd540_patch_apply(&hd)==-1&&!stores);
 hd_setup();assert(!hd540_patch_apply(&hd));put64(hd.address[5],0x1234);
 assert(hd540_patch_restore(&hd)==-1&&hd.dirty&&get(NULL,hd.address[5])==0x1234);
 hd_setup();put64(hd.address[0],hd.original[0]^1);assert(hd540_patch_resolve(&hd,&c)==-1&&!hd.ready&&!stores);
 hd_setup();assert(!patch_apply(&c,MODE_240P_NTSC));stores=0;
 assert(hd540_patch_resolve(&hd,&c)==-1&&!stores);
 puts("PASS: HD full-record identity, progressive raster with FP SCALE mode, twelve-word scope, source/SD preservation, cache distinction, every partial-write rollback, restore retry and external-change refusal");
 return 0;
}
