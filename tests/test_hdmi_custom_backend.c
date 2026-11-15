/* Integration proof: actual engine services, fake existing native interfaces. */
#include "../experiments/hdmi_custom/backend.h"
#include "../experiments/display_probe/native.h"
#include "../plugin/vsh_hd540.h"
#include "../plugin/vsh_video.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t now=100;
static vsh_video_state current;
static unsigned pc_dirty,hd_dirty,pc_profiles,hd_applies[7],bridge_i,bridge_p;
uint64_t hc_mock_time(void){return now;}
static uint64_t clock_op(void *u){(void)u;return now;}
static void stock_state(unsigned mode)
{
 memset(&current,0,sizeof(current));current.mode=mode;current.pitch=8192;current.format=5;current.flags=0x1010205;
 current.raw[8]=mode==11?2:1;current.raw[9]=mode==10?1:2;current.raw[10]=6;current.raw[11]=2;
}
int vsh_video_bind(void){return 0;}
int vsh_video_query(vsh_video_state *s){*s=current;return 0;}
int vsh_video_hd_submit(const vsh_video_state *s,uint32_t flags,uint32_t mode)
{assert(s->pitch==8192&&flags==0x1010205&&!pc_dirty&&!hd_dirty);assert(mode==10||mode==12);if(mode==10)bridge_i++;else bridge_p++;stock_state(mode);return 0;}
static int pc_prepare(void *u,unsigned p){(void)u;assert(!hd_dirty&&!pc_dirty);assert(p>=1&&p<=9);pc_profiles|=1u<<p;return 0;}
static int pc_stock(void *u){(void)u;stock_state(11);return 0;}
static int pc_caps(void *u){(void)u;assert(!hd_dirty);return 0;}
static int pc_candidate(void *u){(void)u;assert(!hd_dirty);pc_dirty=1;return 0;}
static int yes(void *u){(void)u;return 1;}
static int no(void *u){(void)u;return 0;}
static int pc_target(void *u,unsigned kind){(void)u;(void)kind;return 1;}
static int pc_restore(void *u){(void)u;pc_dirty=0;stock_state(12);return 0;}
const dp_ops *dp_native_ops(void)
{static const dp_ops o={pc_prepare,pc_stock,pc_caps,pc_candidate,yes,pc_target,pc_restore,yes,clock_op,no,0};return &o;}
void dp_native_log(void (*log)(const char *,uint32_t)){(void)log;}
static int hd_prepare(void *u){(void)u;assert(!pc_dirty&&!hd_dirty&&current.mode==10);return 0;}
static int hd_alt(void *u){(void)u;stock_state(11);return 0;}
static int hd_is_alt(void *u){(void)u;return current.mode==11;}
static int hd_apply(void *u,unsigned kind){(void)u;assert(!pc_dirty&&!hd_dirty);hd_dirty=kind;hd_applies[kind]++;return 0;}
static int hd_original(void *u){(void)u;stock_state(10);return 0;}
static int hd_is_original(void *u,int candidate)
{
 (void)u;if(candidate==3)return current.mode==11&&hd_dirty==HD540_HDMI_720_CONTROL;
 return current.mode==10&&(!candidate?!hd_dirty:candidate==1?hd_dirty==HD540_TEST:hd_dirty==HD540_HDMI_CONTROL);
}
static int hd_restore(void *u){(void)u;hd_dirty=0;return 0;}
const hd540_ops *vsh_hd540_ops(void)
{static const hd540_ops o={hd_prepare,hd_alt,hd_is_alt,hd_apply,hd_original,hd_is_original,hd_restore,0,yes,clock_op};return &o;}
void vsh_hd540_set_log(void (*log)(const char *,uint32_t)){(void)log;}
static int run_until(int wanted)
{
 for(unsigned i=0;i<300;i++){int state=hc_backend_tick(0);if(state==wanted)return state;assert(state!=HC_ENGINE_FAILED&&state!=HC_ENGINE_ERROR);now+=250000;}
 assert(!"Engine did not reach expected state");return -1;
}
int main(void)
{
 for(unsigned mode=1;mode<=5;mode++){
  stock_state(12);assert(!hc_backend_start(0,mode));assert(hc_backend_start(0,mode)==-1);
  run_until(HC_ENGINE_PREVIEW);assert(hc_backend_health(0)==1);assert(!hc_backend_keep(0));
  now+=90000000;assert(hc_backend_tick(0)==HC_ENGINE_HELD);
  hc_backend_cancel(0);run_until(HC_ENGINE_DONE);assert(current.mode==12&&!pc_dirty&&!hd_dirty);
 }
 assert(bridge_i==1&&bridge_p==1&&hd_applies[HD540_TEST]==1&&hd_applies[HD540_HDMI_720_CONTROL]==0);
 assert((pc_profiles&0x20e)==0x20e);
 stock_state(12);assert(!hc_backend_start(0,HC_540));hc_backend_cancel(0);run_until(HC_ENGINE_DONE);assert(current.mode==12&&!hd_dirty);
 stock_state(12);current.raw[9]=1;assert(hc_backend_start(0,HC_540)==-1);
 puts("PASS: serial native/XGA and full 1080p->1080i->540p controls, confirmed keep, stock bridge recovery and cancellation before HD staging");
 return 0;
}
