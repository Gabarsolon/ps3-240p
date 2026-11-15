/* SPDX-License-Identifier: MIT. Reuse verified timing transactions serially. */
#include "backend.h"
#include "../display_probe/native.h"
#include "../../plugin/vsh_hd540.h"
#include "../../plugin/vsh_video.h"
#if defined(CRT_CUSTOM_HDMI) && !defined(HC_BACKEND_HOST_TEST)
#include "video.h"
#endif
#ifndef HC_BACKEND_HOST_TEST
#include <sys/sys_time.h>
#else
extern uint64_t hc_mock_time(void);
#endif
#include <string.h>
enum {NATIVE=1,BRIDGE_I,HD,BRIDGE_P,DONE,ERROR,FAILED};
static dp_service pc;
static hd540_service hd;
static vsh_video_state original;
static unsigned phase,selected,stage,serial;
static int stopping,had_error,pending;
static uint64_t deadline,retry,health_at;
static unsigned attempts;
static int last_health;
unsigned hc_backend_scanout_factor(void)
{return 1u; /* App draws/registers at its pitch; the patched native writer doubles
  * the fetch (see vsh_hd540.c alternate). 2x here showed a stretched top half. */}
static void (*log_event)(const char *,uint32_t);
static uint64_t clock_now(void){
#ifdef HC_BACKEND_HOST_TEST
 return hc_mock_time();
#else
 return sys_time_get_system_time();
#endif
}
static void note(const char *name,uint32_t v){if(log_event)log_event(name,v);}
void hc_backend_log(void (*log)(const char *,uint32_t))
{log_event=log;dp_native_log(log);vsh_hd540_set_log(log);}
static unsigned pc_profile(unsigned mode)
{static const unsigned profiles[]={0,9,1,2,3};return profiles[mode];}
static unsigned next_serial(void){if(!++serial)++serial;return serial;}
static void return_progressive(void)
{
 phase=BRIDGE_P;attempts=1;retry=clock_now()+1000000;deadline=clock_now()+8000000;
 int rc=vsh_video_hd_submit(&original,original.flags,original.mode);note("bridge_restore_submit",(unsigned)rc);
}
int hc_backend_start(void *u,unsigned mode)
{
 (void)u;
 if(mode<1||mode>HC_MODE_COUNT||(phase&&phase!=DONE&&phase!=ERROR))return -1;
 vsh_video_state s;
 if(vsh_video_bind()||vsh_video_query(&s)||s.state)return -1;
 /* A native 1080i start (XMB at 1080i, typical for HD CRTs) skips the 1080p bridge
  * for 540p only; recovery returns to the same saved 1080i mode. */
 int native_i=mode==HC_540&&s.mode==10&&s.raw[9]==1;
 if((s.mode!=12&&!native_i)||s.raw[8]!=1||(!native_i&&s.raw[9]!=2)||
    (!native_i&&s.raw[10]!=6)||s.format!=5||s.pitch!=8192||(s.flags&0xff)!=5||(s.flags&0xf00)==0xf00||
    (s.flags&0x20000))return -1;
 original=s;selected=mode;stopping=had_error=0;health_at=0;last_health=1;
 if(mode!=HC_540){memset(&pc,0,sizeof(pc));phase=NATIVE;pending=1;}
 else if(native_i){
  memset(&hd,0,sizeof(hd));phase=BRIDGE_I;stage=0;pending=0;
  deadline=clock_now()+8000000;note("native_1080i_start",1);
 }else{
  memset(&hd,0,sizeof(hd));phase=BRIDGE_I;stage=0;pending=0;
  deadline=clock_now()+8000000;
  int rc=vsh_video_hd_submit(&original,original.flags,10);note("bridge_1080i_submit",(unsigned)rc);
  if(rc){had_error=1;return_progressive();}
 }
 return 0;
}
void hc_backend_cancel(void *u)
{
 (void)u;if(stopping)return;stopping=1;
 if(phase==NATIVE)pending=2;
 else if(phase==BRIDGE_I)return_progressive();
 else if(phase==HD){if(hd540_service_busy(&hd))pending=2;else return_progressive();}
}
int hc_backend_keep(void *u)
{
 (void)u;
 if(phase==NATIVE)return dp_keep(&pc);
 if(phase==HD)return hd540_service_keep(&hd);
 return -1;
}
int hc_backend_health(void *u)
{
 (void)u;uint64_t now=clock_now();if(now<health_at)return last_health;health_at=now+1000000;
 if(phase==NATIVE)last_health=dp_native_ops()->at_target(NULL,DP_WXGA);
 else if(phase==HD)last_health=vsh_hd540_ops()->is_original(NULL,1);
 else last_health=0;
 return last_health;
}
int hc_backend_tick(void *u)
{
 (void)u;uint64_t now=clock_now();
 if(phase==NATIVE){
  dp_request q={DPROBE_MAGIC,next_serial(),pending==2?DP_CANCEL:DP_AUTO,pc_profile(selected)};
  dp_reply reply;dp_tick(&pc,dp_native_ops(),now,pending?&q:NULL,&reply);pending=0;
  if(pc.state==DP_RECOVERY_FAILED){phase=FAILED;return HC_ENGINE_FAILED;}
  if(pc.state==DP_REFUSED){phase=ERROR;return HC_ENGINE_ERROR;}
  if(pc.state==DP_DONE){phase=DONE;return pc.detail&&pc.detail!=DP_CANCELLED?HC_ENGINE_ERROR:HC_ENGINE_DONE;}
  if(pc.state==DP_HELD)return HC_ENGINE_HELD;
  return pc.state==DP_ACTIVE&&pc.kind==DP_WXGA?HC_ENGINE_PREVIEW:HC_ENGINE_BUSY;
 }
 if(phase==BRIDGE_I){
  vsh_video_state s;
  int ready=1;
#if defined(CRT_CUSTOM_HDMI) && !defined(HC_BACKEND_HOST_TEST)
  ready=hc_video_ready();
#endif
  if(ready==1&&!vsh_video_query(&s)&&!s.state&&s.mode==10&&s.raw[8]==1&&s.raw[9]==1&&s.pitch==8192){phase=HD;pending=1;}
  else if(now>=deadline){had_error=1;return_progressive();}
 }
 if(phase==HD){
  /* Stock AVI is kept for 540p; the black AVI-zero 720p control is skipped. */
  static const unsigned actions[]={HD540_ROUNDTRIP,HD540_TEST};
  enum {LAST_STAGE=sizeof(actions)/sizeof(actions[0])-1};
  hd540_request q={HD540_MAGIC,next_serial(),pending==2?HD540_CANCEL:actions[stage],HD540_EXPERIMENTAL_VALUE};
  hd540_reply reply;hd540_service_tick(&hd,vsh_hd540_ops(),now,pending?&q:NULL,&reply);pending=0;
  if(hd.state==HD540_RECOVERY_FAILED){phase=FAILED;return HC_ENGINE_FAILED;}
  if(hd.state==HD540_REFUSED){had_error=1;return_progressive();}
  else if(hd.state==HD540_DONE){
   if(hd.had_error)had_error=1;
   if(stopping||had_error||stage==LAST_STAGE)return_progressive();
   else{++stage;pending=1;note("hd_auto_stage",stage);}
  }else if(hd.state==HD540_HELD)return HC_ENGINE_HELD;
  else if(hd.state==HD540_ACTIVE&&hd.kind==HD540_TEST)return HC_ENGINE_PREVIEW;
 }
 if(phase==BRIDGE_P){
  vsh_video_state s;
  int ready=1;
#if defined(CRT_CUSTOM_HDMI) && !defined(HC_BACKEND_HOST_TEST)
  ready=hc_video_ready();
#endif
  if(ready==1&&!vsh_video_query(&s)&&!s.state&&s.mode==original.mode&&s.format==original.format&&
     s.pitch==original.pitch&&!((s.flags^original.flags)&~0x10000000u)&&!memcmp(s.raw+8,original.raw+8,8)){
   phase=had_error?ERROR:DONE;note("bridge_1080p_restored",1);
  }else if(now>=deadline){phase=FAILED;note("bridge_1080p_unconfirmed",1);}
  else if(now>=retry&&attempts<3){++attempts;retry=now+1000000;
   note("bridge_restore_retry",(unsigned)vsh_video_hd_submit(&original,original.flags,original.mode));}
 }
 return phase==FAILED?HC_ENGINE_FAILED:phase==ERROR?HC_ENGINE_ERROR:phase==DONE?HC_ENGINE_DONE:HC_ENGINE_BUSY;
}
