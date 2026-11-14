/* SPDX-License-Identifier: MIT. No direct MMIO/encoder/clock/persistent writes. */
#include "vsh_hd540.h"
#include "vsh_video.h"
#include "../app/source/hd540_patch.h"
#include "../app/source/hd540_hdmi.h"
#include "../app/source/hd540_scale.h"
#include "../app/source/hd540_cache.h"
#ifndef VSH_HD540_HOST_TEST
#include <sys/file.h>
#include <ppu-lv2.h>
#include <sys/sys_time.h>
#else
extern uint64_t mock_hd_read(uint64_t);
extern void mock_hd_write(uint64_t,uint64_t);
extern int mock_hd_flag(const char *);
extern uint64_t mock_hd_time(void);
#endif
#include <stdio.h>
#include <string.h>

static patch_context transport;
static hd540_patch patch;
static hd540_hdmi hdmi;
static hd540_scale scale;
static hd540_cache cache;
static vsh_video_state baseline;
static uint32_t alternate_flags,alternate_mode;
static char error[160];
static void (*diagnostic)(const char *,uint32_t);
static uint64_t stable_since;
static uint64_t cleanup_since;
static vsh_video_state stable_state;
static vsh_video_state last_observed;
static int have_observed;
void vsh_hd540_set_log(void (*log)(const char *,uint32_t)){diagnostic=log;}
static void note(const char *name,uint32_t value){if(diagnostic)diagnostic(name,value);}
static void observed(const vsh_video_state *s)
{
 if(have_observed&&!memcmp(s,&last_observed,sizeof(*s)))return;
 last_observed=*s;have_observed=1;
 char event[64];
 snprintf(event,sizeof(event),"hd_surface mode=%x pitch=%x fmt=%x",s->mode,s->pitch,s->format);
 note(event,s->flags);
 note("hd_native",((unsigned)s->raw[8]<<24)|((unsigned)s->raw[9]<<16)|(unsigned)s->state);
}
static uint64_t monotonic(void *u)
{
 (void)u;
#ifndef VSH_HD540_HOST_TEST
 return sys_time_get_system_time();
#else
 return mock_hd_time();
#endif
}
static uint64_t read_ram(void *u,uint64_t a)
{
 (void)u;
#ifndef VSH_HD540_HOST_TEST
 lv2syscall3(8,0x7777,0x1008,a);return_to_user_prog(uint64_t);
#else
 return mock_hd_read(a);
#endif
}
static void write_ram(void *u,uint64_t a,uint64_t v)
{
 (void)u;
#ifndef VSH_HD540_HOST_TEST
 lv2syscall4(8,0x7777,0x1009,a,v);
#else
 mock_hd_write(a,v);
#endif
}
static int flag(const char *path)
{
#ifndef VSH_HD540_HOST_TEST
 sysFSStat s;return sysLv2FsStat(path,&s)==0;
#else
 return mock_hd_flag(path);
#endif
}
static int fail(const char *why)
{snprintf(error,sizeof(error),"%s",why);return -1;}
static int prepare(void *u)
{
 (void)u;error[0]=0;
 if(patch.dirty||hdmi.dirty||scale.dirty||cache.dirty)return fail("HD timing/HDMI/scaler/cache recovery is still pending");
 cleanup_since=0;
 if(flag("/dev_hdd0/tmp/240p_enforcer.on"))return fail("Disable SD enforcement and reboot before HD experiments");
 vsh_video_state s;
 if(vsh_video_query(&s)||s.state!=0||s.raw[8]!=1||s.raw[9]!=1||
    (s.flags&0xff)!=5||(s.flags&0xf00)==0xf00||(s.flags&0x20000)||
    s.pitch<1920*4u||s.pitch>=32768||(s.pitch&7))
  return fail("Start with a single HDMI 1080i 59.94/60Hz output");
 memset(&transport,0,sizeof(transport));transport.read=read_ram;transport.write=write_ram;
 if(patch_resolve(&transport))return fail(transport.error);
 if(hd540_patch_resolve(&patch,&transport))return fail(patch.error);
 if(hd540_hdmi_resolve(&hdmi,&transport))return fail(hdmi.error);
 if(hd540_scale_resolve(&scale,&transport))return fail(scale.error);
 if(hd540_cache_resolve(&cache,&transport))return fail(cache.error);
 if(hd540_patch_cache(&patch,0)!=1)return fail("Actual HDMI head is not the audited stock 1080i profile");
 int vic=hd540_hdmi_vic(&hdmi);note("hd_avi_vic",(unsigned)vic);
 if(vic!=5)return fail("Start from the verified stock HDMI AVI description");
 if(vsh_video_hd720(&alternate_flags,&alternate_mode))return fail("No verified HDMI 720p transition is available");
 baseline=s;have_observed=0;observed(&s);note("hd_saved_mode",baseline.mode);note("hd_alt_flags",alternate_flags);note("hd_alt_mode",alternate_mode);return 0;
}
static int alternate(void *u)
{(void)u;return vsh_video_hd_submit(&baseline,alternate_flags,alternate_mode);}
static int at_alternate(void *u)
{
 (void)u;vsh_video_state s;
 if(vsh_video_query(&s))return -1;
 observed(&s);
 if(s.state!=0)return 0;
 if((s.flags&0xff)!=5)return -1;
 /* Native request acceptance alone does not establish that the old 1080i
  * profile is inactive. Wait for the independently mapped AV head cache. */
 patch_cached_video video;
 if(patch_read_cached_video(&transport,&video))return -1;
 const patch_cached_head *h=&video.head[0];
 char event[64];snprintf(event,sizeof(event),"hd_head vid=%x size=%ux%u",h->vid,h->width,h->height);
 note(event,(h->active_lines<<16)|h->total_lines);
 return s.raw[8]==2&&s.raw[9]==2&&s.mode==alternate_mode&&h->state==2&&h->width==1280&&h->height==720&&
  h->active_lines==720&&h->total_lines==750;
}
static int apply(void *u,unsigned kind)
{
 (void)u;
 if(kind!=HD540_TEST&&kind!=HD540_HDMI_CONTROL&&kind!=HD540_HDMI_720_CONTROL)return fail("Unknown HD patch kind");
 if(flag("/dev_hdd0/tmp/240p_enforcer.on"))return fail("SD enforcement was armed during the HD test");
 int rc=hd540_hdmi_apply(&hdmi);note("hd_avi_apply",(unsigned)rc);
 if(rc)return fail(hdmi.error);
 if(kind==HD540_HDMI_CONTROL||kind==HD540_HDMI_720_CONTROL)return 0;
 cleanup_since=0;
 rc=hd540_patch_apply(&patch);note("hd_patch_apply",(unsigned)rc);if(rc)return rc;
 rc=hd540_scale_apply(&scale);note("hd_scale_apply",(unsigned)rc);
 if(rc)return fail(scale.error);
 rc=hd540_cache_apply(&cache);note("hd_cache_apply",(unsigned)rc);
 return rc?fail(cache.error):0;
}
static int original(void *u)
{(void)u;return vsh_video_hd_submit(&baseline,baseline.flags,baseline.mode);}
static int at_original(void *u,int candidate)
{
 (void)u;
 if(candidate==3){
  if(hd540_scale_check(&scale,0)!=1)return -1;
  int ready=at_alternate(u);if(ready!=1)return ready;
  int vic=hd540_hdmi_vic(&hdmi);note("hd_avi_vic",(unsigned)vic);
  return vic==-1?-1:vic==0;
 }
 vsh_video_state s;
 if(vsh_video_query(&s))return -1;
 observed(&s);
 if(s.state!=0)return 0;
 if((s.flags&0xff)!=5)return -1;
 if(s.raw[8]!=1||s.raw[9]!=1||s.mode!=baseline.mode||s.flags!=baseline.flags)return 0;
 int raster=hd540_patch_cache(&patch,candidate==1);if(raster!=1)return raster;
 int scaled=hd540_scale_check(&scale,candidate==1);if(scaled!=1)return scaled;
 /* App exit can leave the old packed pitch visible until XMB rebuilds its
  * framebuffer during the mode round trip. Compare hardware with the live
  * source request, independently agreeing with the current VSH surface;
  * the earlier baseline still owns the saved mode/connector for recovery. */
 uint32_t source_pitch=s.pitch;
 if(candidate==1||(!candidate&&cache.dirty)){
  patch_cached_video video;
  if(patch_read_cached_video(&transport,&video))return -1;
  const patch_cached_head *head=&video.head[0];
  if(source_pitch<1920*4u||source_pitch>=32768||(source_pitch&7))return -1;
  if(head->state!=2||head->width!=1920||head->height!=1080||
     (head->vid!=7&&head->vid!=0x12)||head->pitch!=source_pitch||s.format!=baseline.format)return 0;
  note("hd_live_source_pitch",source_pitch);
 }
 if(candidate==1){
  uint32_t values[10];int traced=hd540_cache_snapshot(&cache,values);
  if(traced!=1)return traced;
  note("hd_cache_calls",values[0]);note("hd_fp_control",values[2]);note("hd_fp_scaler",values[3]);
  note("hd_fp_vstart",values[4]);note("hd_fp_vend",values[5]);
  note("hd_fp_ystep",values[7]);
  note("hd_source_pitch",values[8]);note("hd_source_height",values[9]);
  if(!values[0])return 0;
  if((values[8]&0xfff8u)!=source_pitch*2u)return 0;
 }
 int vic=hd540_hdmi_vic(&hdmi);note("hd_avi_vic",(unsigned)vic);
 if(vic==-1)return -1;
 if(vic!=(candidate?0:5))return 0;
 if(!candidate&&cache.dirty){
  uint32_t values[10];int traced=hd540_cache_snapshot(&cache,values);
  if(traced!=1)return traced;
  note("hd_stock_ystep",values[7]);
  note("hd_stock_pitch",values[8]);
  if((values[8]&0xfff8u)!=source_pitch)return 0;
  uint64_t now=monotonic(NULL);
  if(!cleanup_since){cleanup_since=now;return 0;}
  if(now-cleanup_since<1000000)return 0;
 }
 return candidate?1:hd540_cache_finish(&cache);
}
static int restore(void *u)
{
 (void)u;int a=hd540_patch_restore(&patch),b=hd540_hdmi_restore(&hdmi),d=hd540_scale_restore(&scale),e=hd540_cache_restore(&cache);
 if(e)return fail(cache.error);if(d)return fail(scale.error);if(b)return fail(hdmi.error);return a;
}
static int ready(void *u)
{
 (void)u;
 if(flag("/dev_hdd0/tmp/240p_app.active")){stable_since=0;return 0;}
 vsh_video_state s;if(vsh_video_query(&s)||s.state!=0){stable_since=0;return 0;}
 uint64_t now=monotonic(NULL);
 if(!stable_since||memcmp(&s,&stable_state,sizeof(s))){stable_state=s;stable_since=now;return 0;}
 return now-stable_since>=1500000;
}
const hd540_ops *vsh_hd540_ops(void)
{
 static const hd540_ops ops={prepare,alternate,at_alternate,apply,original,at_original,restore,NULL,ready,monotonic};return &ops;
}
const char *vsh_hd540_error(void)
{return error[0]?error:patch.error;}
