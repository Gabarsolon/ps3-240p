#include "hd540_fixture.h"
#include "../plugin/vsh_hd540.h"
#include "../plugin/vsh_video.h"
static vsh_video_state native;
static int armed,query_fail,map_fail,submitted;
static uint32_t submitted_flags,submitted_mode;
static int app_present;
static uint64_t mock_time;
uint64_t mock_hd_read(uint64_t a){return get(NULL,a);}
void mock_hd_write(uint64_t a,uint64_t v){set(NULL,a,v);}
int mock_hd_flag(const char *path){if(!strcmp(path,"/dev_hdd0/tmp/240p_app.active"))return app_present;assert(!strcmp(path,"/dev_hdd0/tmp/240p_enforcer.on"));return armed;}
uint64_t mock_hd_time(void){return mock_time;}
int vsh_video_query(vsh_video_state *s){if(query_fail)return -1;*s=native;return 0;}
int vsh_video_hd720(uint32_t *flags,uint32_t *mode)
{*flags=0x11105;*mode=5;return map_fail;}
int vsh_video_hd_submit(const vsh_video_state *s,uint32_t flags,uint32_t mode)
{assert(s->pitch==7680&&s->format==7);submitted++;submitted_flags=flags;submitted_mode=mode;return 0;}
static void reset(void)
{
 hd_setup();hd_stock();memset(&native,0,sizeof(native));
 native.raw[8]=1;native.raw[9]=1;native.flags=0x11105;native.mode=0x123;native.format=7;native.pitch=7680;
 armed=query_fail=map_fail=submitted=app_present=0;mock_time=1;
}
static void recovered(const hd540_ops *ops)
{
 assert(!ops->restore(NULL));
 native.raw[8]=1;native.raw[9]=1;native.mode=0x123;native.flags=0x11105;native.state=0;
 cache_hd(hd.original+10);hdmi_cache(5);
 if(get(NULL,physical(0xc0003350))==UINT64_C(0x8003f800))
  put64(physical(0xc000de20),UINT64_C(7680)<<32);
 int ready=ops->is_original(NULL,0);
 if(!ready){mock_time+=1000000;ready=ops->is_original(NULL,0);}
 assert(ready==1);hd_stock();
}
int main(void)
{
 const hd540_ops *ops=vsh_hd540_ops();reset();assert(!ops->prepare(NULL)&&!stores&&!submitted);
 app_present=1;assert(ops->ready(NULL)==0);app_present=0;assert(ops->ready(NULL)==0);
 mock_time=1500001;assert(ops->ready(NULL)==1);native.pitch=8192;assert(ops->ready(NULL)==0);
 native.pitch=7680;assert(ops->ready(NULL)==0);mock_time+=1500000;assert(ops->ready(NULL)==1);
 assert(!ops->alternate(NULL)&&submitted_flags==0x11105&&submitted_mode==5);
 assert(!ops->apply(NULL,HD540_TEST)&&stores==17+HD540_CACHE_QWORDS+1);
 native.raw[8]=2;native.raw[9]=2;native.mode=5;
 assert(ops->is_alternate(NULL)==0); /* Native state can lead actual scanout. */
 put64(physical(0xa0024000),(UINT64_C(9)<<32)|1280);
 put64(physical(0xa0024008),(UINT64_C(720)<<32)|7680);
 put64(physical(0xa0023f98),(UINT64_C(719)<<32)|749);
 assert(ops->is_alternate(NULL)==1);
 hdmi_cache(0);
 assert(!ops->original(NULL)&&submitted_mode==0x123); /* Exact saved mode, not re-mapped 1080p. */
 assert(ops->is_original(NULL,1)==0);
 native.raw[8]=1;native.raw[9]=1;native.mode=0x123;cache_hd(hd.candidate+10);
 assert(ops->is_original(NULL,1)==0); /* New wrapper must actually execute. */
 put64(physical(0xc000de00),UINT64_C(1)<<32);
 put64(physical(0xc000de08),(UINT64_C(0x11003011)<<32)|0x1010);
 put64(physical(0xc000de10),539);
 put64(physical(0xc000de20),UINT64_C(15363)<<32);
 assert(ops->is_original(NULL,1)==1);
 put64(physical(0xc000de20),UINT64_C(7680)<<32);
 assert(ops->is_original(NULL,1)==0); /* Native fetch must use doubled pitch. */
 recovered(ops);
 assert(get(NULL,physical(0xc0003350))==UINT64_C(0x8000d8ec));
 for(unsigned i=0;i<HD540_CACHE_QWORDS;i++)assert(!get(NULL,physical(0x8003f800+i*8)));
 /* Real app launch regression: saved app pitch is 7680; XMB's rebuilt
  * 1080i source uses 8192. Both candidate and cleanup must follow the
  * agreeing live VSH/source pitch, without accepting stale/mismatched data. */
 reset();assert(!ops->prepare(NULL));assert(!ops->apply(NULL,HD540_TEST));
 cache_hd(hd.candidate+10);hdmi_cache(0);
 native.pitch=8192;put64(physical(0xc000de00),UINT64_C(1)<<32);
 put64(physical(0xc000de20),UINT64_C(16384)<<32);
 assert(ops->is_original(NULL,1)==0); /* Cache still has old app pitch. */
 put64(physical(0xa0024008),(UINT64_C(1080)<<32)|8192);
 assert(ops->is_original(NULL,1)==1);
 put64(physical(0xc000de20),UINT64_C(15360)<<32);
 assert(ops->is_original(NULL,1)==0); /* Old-baseline double is not success. */
 put64(physical(0xc000de20),UINT64_C(16384)<<32);
 native.pitch=32768;assert(ops->is_original(NULL,1)==-1);native.pitch=8192;
 native.format=6;assert(ops->is_original(NULL,1)==0);native.format=7;
 assert(!ops->restore(NULL));cache_hd(hd.original+10);hdmi_cache(5);
 put64(physical(0xa0024008),(UINT64_C(1080)<<32)|8192);
 put64(physical(0xc000de20),UINT64_C(7680)<<32);
 assert(ops->is_original(NULL,0)==0);
 put64(physical(0xc000de20),UINT64_C(8192)<<32);
 assert(ops->is_original(NULL,0)==0);mock_time+=1000000;
 assert(ops->is_original(NULL,0)==1);
 assert(get(NULL,physical(0xc0003350))==UINT64_C(0x8000d8ec));
 for(unsigned i=0;i<HD540_CACHE_QWORDS;i++)assert(!get(NULL,physical(0x8003f800+i*8)));
 reset();assert(!ops->prepare(NULL));assert(!ops->apply(NULL,HD540_HDMI_CONTROL)&&stores==1);
 hd_stock();assert(ops->is_original(NULL,2)==0);hdmi_cache(0);
 assert(ops->is_original(NULL,2)==1&&ops->is_original(NULL,1)==0);
 recovered(ops);
 reset();assert(!ops->prepare(NULL));assert(!ops->apply(NULL,HD540_HDMI_720_CONTROL)&&stores==1);
 assert(ops->is_original(NULL,3)==0); /* Still stock 1080i, not the control target. */
 native.raw[8]=2;native.raw[9]=2;native.mode=5;
 put64(physical(0xa0024000),(UINT64_C(9)<<32)|1280);
 put64(physical(0xa0024008),(UINT64_C(720)<<32)|7680);
 put64(physical(0xa0023f98),(UINT64_C(719)<<32)|749);
 assert(ops->is_original(NULL,3)==0); /* Stock AVI code is not proof of patch execution. */
 hdmi_cache(0);assert(ops->is_original(NULL,3)==1);
 put64(physical(0xa0023f98),(UINT64_C(719)<<32)|1124);
 assert(ops->is_original(NULL,3)==0); /* Native surface alone cannot pass. */
 recovered(ops);
 reset();assert(!ops->prepare(NULL));assert(ops->apply(NULL,999)==-1&&!stores);
 for(int failure=1;failure<=17+(int)HD540_CACHE_QWORDS+1;failure++){
  reset();assert(!ops->prepare(NULL));drop_store=failure;
  assert(ops->apply(NULL,HD540_TEST)==-1);
  drop_store=0;recovered(ops);
  assert(get(NULL,physical(0x800399c0))==UINT64_C(0x123456789a05bcde));
 }
 reset();native.flags=0x11102;assert(ops->prepare(NULL)==-1&&!stores&&!submitted);
 reset();native.flags=0x11f05;assert(ops->prepare(NULL)==-1&&!stores);
 reset();native.flags=0x21105;assert(ops->prepare(NULL)==-1&&!stores);
 reset();native.raw[9]=2;assert(ops->prepare(NULL)==-1&&!stores);
 reset();native.raw[8]=2;assert(ops->prepare(NULL)==-1&&!stores);
 reset();native.pitch=5120;assert(ops->prepare(NULL)==-1&&!stores);
 reset();native.pitch=32768;assert(ops->prepare(NULL)==-1&&!stores);
 reset();native.pitch=8196;assert(ops->prepare(NULL)==-1&&!stores);
 reset();native.state=2;assert(ops->prepare(NULL)==-1&&!stores);
 reset();query_fail=1;assert(ops->prepare(NULL)==-1&&!stores);
 reset();map_fail=1;assert(ops->prepare(NULL)==-1&&!stores);
 reset();armed=1;assert(ops->prepare(NULL)==-1&&!stores);
 reset();put64(physical(0xa0024000),(UINT64_C(8)<<32)|1920);
 assert(ops->prepare(NULL)==-1&&!stores);
 reset();assert(!ops->prepare(NULL));armed=1;assert(ops->apply(NULL,HD540_TEST)==-1&&!stores);
 assert(vsh_hd540_error()[0]);
 puts("PASS: native HD backend rejects analog/clone/50Hz/progressive/busy/bad-pitch/armed-SD/wrong-head inputs, preserves the exact original mode and performs no preflight writes");
 return 0;
}
