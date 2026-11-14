#include "hd540_fixture.h"
static void empty(const hd540_cache *h)
{
 for(unsigned i=0;i<HD540_CACHE_WORDS;i++)assert(get(NULL,h->address[i])==h->original[i]);
}
int main(void)
{
 hd540_cache h={0};hd_setup();assert(!hd540_cache_resolve(&h,&c)&&!stores);
 assert(!hd540_cache_apply(&h)&&stores==HD540_CACHE_QWORDS+1&&h.dirty&&h.attached);
 assert(get(NULL,h.address[0])==UINT64_C(0x8003f800));
 uint32_t values[10];assert(hd540_cache_snapshot(&h,values)==1&&values[0]==0);
 put64(h.address[HD540_CACHE_TRACE],(UINT64_C(1)<<32)|1);
 assert(hd540_cache_snapshot(&h,values)==0);
 put64(h.address[HD540_CACHE_TRACE],UINT64_C(1)<<32);
 put64(h.address[HD540_CACHE_TRACE+1],(UINT64_C(0x11003011)<<32)|0x1010);
 put64(h.address[HD540_CACHE_TRACE+2],539);
 put64(h.address[HD540_CACHE_TRACE+3],0x20000000);
 assert(hd540_cache_snapshot(&h,values)==1&&values[0]==1&&values[2]==0x11003011&&values[3]==0x1010&&values[5]==539);
 assert(values[7]==0x20000000);
 assert(!hd540_cache_restore(&h)&&h.dirty&&h.attached); /* Keep flush for stock RPC. */
 assert(hd540_cache_finish(&h)==1&&!h.dirty&&!h.attached);empty(&h);hd_stock();
 for(unsigned failure=1;failure<=HD540_CACHE_QWORDS+1;failure++){
  hd_setup();memset(&h,0,sizeof(h));assert(!hd540_cache_resolve(&h,&c));drop_store=(int)failure;
  assert(hd540_cache_apply(&h)==-1&&!h.dirty&&!h.attached);empty(&h);
 }
 hd_setup();memset(&h,0,sizeof(h));assert(!hd540_cache_resolve(&h,&c));assert(!hd540_cache_apply(&h));
 drop_store=stores+1;assert(hd540_cache_finish(&h)==-1&&h.dirty&&h.attached);
 drop_store=0;assert(hd540_cache_finish(&h)==1&&!h.dirty);empty(&h);
 hd_setup();memset(&h,0,sizeof(h));assert(!hd540_cache_resolve(&h,&c));assert(!hd540_cache_apply(&h));
 put64(h.address[6],0x1234);assert(hd540_cache_restore(&h)==-1&&h.dirty&&get(NULL,h.address[6])==0x1234);
 hd_setup();memset(&h,0,sizeof(h));put64(physical(0x8003f800),1);
 assert(hd540_cache_resolve(&h,&c)==-1&&!stores&&!h.ready);
 hd_setup();memset(&h,0,sizeof(h));assert(!hd540_cache_resolve(&h,&c));put64(h.address[1],0);
 assert(hd540_cache_apply(&h)==-1&&!stores);
 hd_setup();memset(&h,0,sizeof(h));assert(!hd540_cache_resolve(&h,&c));put64(0x122180,0x1140000);
 assert(hd540_cache_apply(&h)==-1&&!stores);
 puts("PASS: native wrapper arena/descriptor identity, all staged-store failures, deferred stock flush/cleanup, in-flight gate, retry and changed mapping/body refusal");
 return 0;
}
