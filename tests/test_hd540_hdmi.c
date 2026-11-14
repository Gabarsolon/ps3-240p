#include "hd540_fixture.h"
int main(void)
{
 hd540_hdmi h={0};hd_setup();assert(!hd540_hdmi_resolve(&h,&c)&&!stores);
 assert(hd540_hdmi_cache(&h,5)==1&&hd540_hdmi_cache(&h,0)==0);
 uint64_t first=get(NULL,h.address[0]),second=get(NULL,h.address[1]);
 assert(!hd540_hdmi_apply(&h)&&stores==1&&h.dirty);
 assert(get(NULL,h.address[0])==((first&UINT64_C(0xffffffff00000000))|(14u<<26)|(9u<<21)));
 assert(get(NULL,physical(0x800399c0))==UINT64_C(0x123456789a05bcde));
 assert(get(NULL,h.address[1])==second);hd_stock(); /* No RSX or SD timing changes. */
 assert(hd540_hdmi_cache(&h,0)==0); /* Code RAM write alone is not proof of execution. */
 hdmi_cache(0);assert(hd540_hdmi_cache(&h,0)==1);
 assert(hd540_hdmi_resolve(&h,&c)==-1&&h.dirty);
 assert(!hd540_hdmi_restore(&h)&&stores==2&&!h.dirty&&get(NULL,h.address[0])==first);
 hdmi_cache(5);assert(hd540_hdmi_cache(&h,5)==1);
 hd_setup();memset(&h,0,sizeof(h));assert(!hd540_hdmi_resolve(&h,&c));drop_store=1;
 assert(hd540_hdmi_apply(&h)==-1&&!h.dirty&&get(NULL,h.address[0])==h.original[0]);
 hd_setup();memset(&h,0,sizeof(h));assert(!hd540_hdmi_resolve(&h,&c));assert(!hd540_hdmi_apply(&h));
 drop_store=stores+1;assert(hd540_hdmi_restore(&h)==-1&&h.dirty);
 drop_store=0;assert(!hd540_hdmi_restore(&h)&&!h.dirty);
 hd_setup();memset(&h,0,sizeof(h));assert(!hd540_hdmi_resolve(&h,&c));assert(!hd540_hdmi_apply(&h));
 put64(h.address[0],0x1234);assert(hd540_hdmi_restore(&h)==-1&&h.dirty&&get(NULL,h.address[0])==0x1234);
 h.dirty=0;hd_setup();memset(&h,0,sizeof(h));put64(physical(0x800399c8),0x1234);
 assert(hd540_hdmi_resolve(&h,&c)==-1&&!stores);
 hd_setup();memset(&h,0,sizeof(h));assert(!hd540_hdmi_resolve(&h,&c));
 put64(physical(0xc000b560),0xc000def0);assert(hd540_hdmi_apply(&h)==-1&&!stores);
 hd_setup();memset(&h,0,sizeof(h));assert(!hd540_hdmi_resolve(&h,&c));
 put64(0x122140,0x1d00000);assert(hd540_hdmi_apply(&h)==-1&&!stores);
 hd_setup();memset(&h,0,sizeof(h));assert(!hd540_hdmi_resolve(&h,&c));
 put64(0x1c00ef0,get(NULL,0x1c00ef0)^UINT64_C(0x0100000000));
 assert(hd540_hdmi_cache(&h,5)==0); /* Invalid checksum is never readiness. */
 hdmi_cache(0);put64(0x1c00ef0,0);assert(hd540_hdmi_cache(&h,0)==0);
 puts("PASS: fingerprinted AVI builder, one generated instruction, unchanged transmitter template, packet checksum/VIC execution readback, rollback/retry, page/pointer guards and external-change refusal");
 return 0;
}
