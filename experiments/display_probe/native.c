/* SPDX-License-Identifier: MIT. No direct stores to VSH/LV1/MMIO/flash. */
#include "native.h"
#include "../../plugin/vsh_video.h"
#ifndef DP_NATIVE_HOST_TEST
#include <sys/file.h>
#include <sys/sys_time.h>
#else
extern int dp_mock_flag(const char *);
extern uint64_t dp_mock_clock(void);
extern int dp_mock_identity(uint32_t,unsigned,uint64_t);
extern uint32_t dp_mock_table(void);
extern int dp_mock_info(unsigned,unsigned,void *);
extern int dp_mock_update(unsigned,uint32_t,uint32_t,uint32_t,uint32_t,void *,int);
extern int dp_mock_hdmi(uint32_t,uint32_t,unsigned);
extern int dp_mock_analog(uint32_t,uint32_t);
extern int dp_mock_global(uint32_t);
extern int dp_mock_map(uint32_t *,uint32_t *,int,int,unsigned);
#endif
#include <stdio.h>
#include <string.h>
static void (*logger)(const char *,uint32_t);
void dp_native_log(void (*log)(const char *,uint32_t)){logger=log;}
static void note(const char *event,uint32_t value){if(logger)logger(event,value);}
static int fail(const char *why){note(why,UINT32_MAX);return -1;}
static int read32(uint32_t at,uint32_t *value){return vsh_video_copy(at,value,4);}
static uint64_t clock_now(void *u){(void)u;
#ifndef DP_NATIVE_HOST_TEST
 return sys_time_get_system_time();
#else
 return dp_mock_clock();
#endif
}
static vsh_video_state saved,source,last;
static uint32_t manager,objects[2],masks[2],debug_saved;
static unsigned dirty[2],debug_dirty;static int observed;
static int (*device_info)(unsigned,unsigned,void *);
static int (*update_display)(unsigned,uint32_t,uint32_t,uint32_t,uint32_t,void *,int);
static int (*map_display)(uint32_t *,uint32_t *,int,int,unsigned);
#ifndef DP_NATIVE_HOST_TEST
static uint32_t hdmi_fd[2] __attribute__((aligned(8)))={0x47c534,0x6f5558};
static uint32_t analog_fd[2] __attribute__((aligned(8)))={0x474128,0x6f5558};
static uint32_t global_fd[2] __attribute__((aligned(8)))={0xc56d8,0x6f5558};
static int (*hdmi_caps)(uint32_t,uint32_t,unsigned)=(void *)hdmi_fd;
static int (*analog_caps)(uint32_t,uint32_t)=(void *)analog_fd;
static int (*global_monitor)(uint32_t)=(void *)global_fd;
static int flag(const char *path){sysFSStat s;return sysLv2FsStat(path,&s)==0;}
#else
static int (*hdmi_caps)(uint32_t,uint32_t,unsigned)=dp_mock_hdmi;
static int (*analog_caps)(uint32_t,uint32_t)=dp_mock_analog;
static int (*global_monitor)(uint32_t)=dp_mock_global;
static int flag(const char *path){return dp_mock_flag(path);}
#endif
static int fingerprint(uint32_t at,unsigned n,uint64_t expected)
{
#ifdef DP_NATIVE_HOST_TEST
 return dp_mock_identity(at,n,expected);
#else
 unsigned char bytes[64];if(n>sizeof(bytes)||vsh_video_copy(at,bytes,n))return -1;
 uint64_t h=UINT64_C(14695981039346656037);
 for(unsigned i=0;i<n;i++)h=(h^bytes[i])*UINT64_C(1099511628211);
 return h==expected?0:-1;
#endif
}
static int identities(void)
{
 return fingerprint(0x47c534,64,UINT64_C(0xc281861996bb86db))||
  fingerprint(0x474128,64,UINT64_C(0x459007062f7056f2))||
  fingerprint(0xc56d8,16,UINT64_C(0xc531d7732e8ed8d3))||
  fingerprint(0x455558,32,UINT64_C(0xdd7a4e8ea214e07a));
}
static int pointers(void)
{
 uint32_t p,a,b,extra;
 return read32(0x738bf4,&p)||p!=manager||read32(p+0x24,&a)||a!=objects[0]||
  read32(p+0x28,&extra)||extra||read32(p+0x34,&b)||b!=objects[1];
}
static int mask_read(unsigned i,uint32_t *m){return read32(objects[i]+(i?0x9c:0x6c),m);}
static int masks_at(int candidate)
{
 if(pointers()){note("device_pointer_mismatch",candidate);return -1;}
 for(unsigned i=0;i<2;i++){
  uint32_t m;if(mask_read(i,&m))return -1;
  if(m!=(candidate?0x40000000:masks[i])){note(i?"analog_mask_mismatch":"hdmi_mask_mismatch",m);return 0;}
 }
 uint32_t d;if(read32(0x70f13c,&d))return -1;
 /* VSH configuration bridge 0xc825c consumes this one-shot flag. Only the
  * verified converted target may accept the acknowledged original value. */
 if(candidate==2&&d==debug_saved)return 1;
 if(d!=(candidate?11:debug_saved)){note("debug_monitor_mismatch",d);return 0;}return 1;
}
static void device_dump(void)
{
 unsigned char info[0x1cc]={0};int rc=device_info(0,0,info);note("device_info_rc",(unsigned)rc);
 if(rc)return;
 note("device_port",info[0]);unsigned count=info[5];
 if(count>53){note("device_modes_out_of_bounds",count);return;}
 for(unsigned i=0;i<count;i++){
  const unsigned char *m=info+0x20+8*i;char text[80];
  snprintf(text,sizeof(text),"device_mode %u res=%x scan=%x cvt=%x asp=%x",i,m[0],m[1],m[2],m[3]);
  note(text,((unsigned)m[6]<<8)|m[7]);
 }
}
static int query(vsh_video_state *s)
{
 if(vsh_video_query(s))return -1;
 if(!observed||memcmp(s,&last,sizeof(*s))){
  char text[80];snprintf(text,sizeof(text),"display state=%d mode=%x fmt=%x pitch=%x",s->state,s->mode,s->format,s->pitch);
  note(text,s->flags);note("display_raw",((unsigned)s->raw[8]<<24)|((unsigned)s->raw[9]<<16)|((unsigned)s->raw[10]<<8)|s->raw[11]);
  last=*s;observed=1;
 }
 return 0;
}
static int prepare(void *u)
{
 (void)u;
 if(dirty[0]||dirty[1]||debug_dirty)return fail("previous_caps_recovery_pending");
 if(flag("/dev_hdd0/tmp/240p_app.active")||flag("/dev_hdd0/tmp/240p_enforcer.on"))return fail("app_or_sd_enforcement_active");
 if(vsh_video_bind()||identities())return fail("unsupported_native_display_identity");
 uint32_t export=vsh_video_export("xsetting",0x4712f276),table,descriptor,code;
 if(!export)return fail("display_interface_missing");
#ifndef DP_NATIVE_HOST_TEST
 uint32_t (*get_table)(void)=(void *)(uintptr_t)export;table=get_table();
#else
 table=dp_mock_table();
#endif
 if(read32(table+0x10,&descriptor)||read32(descriptor,&code)||code!=0x455558)return fail("device_query_identity_changed");
#ifndef DP_NATIVE_HOST_TEST
 device_info=(void *)(uintptr_t)descriptor;
#else
 device_info=dp_mock_info;
#endif
 uint32_t update=vsh_video_export("paf",0xd8af2727);
 uint32_t mapper=vsh_video_export("vshmain",0x3aecdd23);
 if(!update||!mapper)return fail("native_submit_missing");
#ifndef DP_NATIVE_HOST_TEST
 update_display=(void *)(uintptr_t)update;
 map_display=(void *)(uintptr_t)mapper;
#else
 update_display=dp_mock_update;
 map_display=dp_mock_map;
#endif
 /* The native conversion byte is an internal enum (observed stock value 6),
  * not the public API's conversion NONE=0. Preserve it exactly on return. */
 if(query(&saved)||saved.state||saved.mode!=12||saved.raw[8]!=1||saved.raw[9]!=2||saved.raw[10]!=6||
   (saved.flags&0xff)!=5||(saved.flags&0xf00)==0xf00||(saved.flags&0x20000)||
   saved.pitch<1920*4||saved.pitch>=32768||(saved.pitch&7))return fail("start_single_stock_hdmi_1080p_60");
 if(read32(0x738bf4,&manager)||manager<0x10000||manager>0x7fffff00||
   read32(manager+0x24,&objects[0])||read32(manager+0x34,&objects[1])||!objects[0]||!objects[1]||pointers())
  return fail("display_device_layout_changed");
 for(unsigned i=0;i<2;i++){
  uint32_t vt,fd,entry;
  if(read32(objects[i],&vt)||vt!=(i?0x6ca268:0x6ca380)||read32(vt+0x40,&fd)||
     read32(fd,&entry)||entry!=(i?0x4740c4:0x47cb8c)||mask_read(i,&masks[i]))return fail("device_backend_identity_changed");
  note(i?"saved_analog_mask":"saved_hdmi_mask",masks[i]);
 }
 if(read32(0x70f13c,&debug_saved)||debug_saved)return fail("existing_debug_monitor_override");
 note("saved_debug_monitor",debug_saved);device_dump();return 0;
}
static int submit_flags(const vsh_video_state *s,uint32_t flags,unsigned mode)
{
 if(pointers()||identities())return fail("display_ownership_changed");
 int pc=(flags&0xff)==0x10;
 /* VSH mapper 0xc7080 deliberately selects category 0x10 for its PC
  * conversion path, even when its device-info port is HDMI. Accept only
  * the exact native WXGA/60 mapping from our owned 720-line source. */
 if(pc&&(s!=&source||mode!=11||(flags&0x003fc000u)!=0x40000||masks_at(1)!=1))return fail("unsupported_pc_mapping");
 if((mode!=11&&mode!=saved.mode)||(!pc&&(flags&0xff)!=5)||(flags&0xf00)==0xf00||
    s->format!=saved.format||s->pitch<1280*4||s->pitch>=32768||(s->pitch&7))return fail("unsupported_source_contract");
 int rc=update_display(0,flags|0x10000000u,mode,s->format,s->pitch,0,1);
 note("submit_rc",(unsigned)rc);return rc;
}
static int submit(const vsh_video_state *s,unsigned mode){return submit_flags(s,s->flags,mode);}
static int stock(void *u){(void)u;return submit(&saved,11);}
static int at_stock(void *u)
{
 (void)u;vsh_video_state s;if(query(&s))return -1;
 int ok=!s.state&&s.raw[8]==2&&s.raw[9]==2&&s.mode==11&&(s.flags&0xff)==5&&s.pitch>=1280*4&&s.pitch<32768&&!(s.pitch&7);
 if(ok)source=s;return ok;
}
static int caps(void *u)
{
 (void)u;if(pointers()||identities()||masks_at(0)!=1)return fail("caps_changed_before_apply");
 for(unsigned i=0;i<2;i++){
  dirty[i]=1;int rc=i?analog_caps(objects[i],0x40000000):hdmi_caps(objects[i],0x40000000,0);
  note(i?"analog_caps_rc":"hdmi_caps_rc",(unsigned)rc);if(rc)return -1;
 }
 debug_dirty=1;int rc=global_monitor(11);note("global_monitor_rc",(unsigned)rc);
 device_dump();return rc||masks_at(1)!=1?-1:0;
}
static int candidate(void *u)
{
 if(caps(u))return -1;
 uint32_t flags=0,mode=0;int rc=map_display(&flags,&mode,2,2,0);
 note("pc_mapper_rc",(unsigned)rc);note("pc_mapper_flags",flags);note("pc_mapper_mode",mode);
 if(rc<=0||(flags&0xff)!=0x10||mode!=11||(flags&0x003fc000u)!=0x40000)return fail("unexpected_native_pc_mapping");
 /* Preserve the current RGB/color-depth selection, as normal VSH does. */
 flags=(flags&~0x0fc00000u)|(source.flags&0x0fc00000u);
 return submit_flags(&source,flags,mode);
}
static int at_target(void *u,unsigned kind)
{
 if(kind==DP_STOCK720)return at_stock(u);
 vsh_video_state s;if(query(&s))return -1;
 if(s.state!=0)return 0;
 if(kind==DP_CAPS){int match=masks_at(1);return match<0?-1:match&&s.mode==saved.mode;}
 if(s.mode!=11||s.raw[8]!=2||s.raw[9]!=2||s.raw[10]!=1||(s.flags&0xff)!=0x10||
    s.pitch<1280*4||s.pitch>=32768||(s.pitch&7))return 0;
 return masks_at(2);
}
static int restore(void *u)
{
 (void)u;int error=0;
 if(pointers()||identities())return fail("restore_identity_changed");
 for(unsigned i=0;i<2;i++)if(dirty[i]){
  uint32_t m;if(mask_read(i,&m)|| (m!=masks[i]&&m!=0x40000000u)){error=1;continue;}
  if(m!=masks[i]){
   int rc=i?analog_caps(objects[i],masks[i]):hdmi_caps(objects[i],masks[i],0);
   note(i?"restore_analog_rc":"restore_hdmi_rc",(unsigned)rc);
  }
  if(mask_read(i,&m)||m!=masks[i])error=1;else dirty[i]=0;
 }
 if(debug_dirty){
  uint32_t d;if(read32(0x70f13c,&d)||(d!=11&&d!=debug_saved))error=1;
  else{if(d!=debug_saved)(void)global_monitor(debug_saved);if(read32(0x70f13c,&d)||d!=debug_saved)error=1;else debug_dirty=0;}
 }
 int rc=submit(&saved,saved.mode);return rc||error?-1:0;
}
static int at_original(void *u)
{
 (void)u;vsh_video_state s;if(query(&s)||masks_at(0)!=1)return 0;
 unsigned mismatch=(!!s.state)|((s.mode!=saved.mode)<<1)|((s.format!=saved.format)<<2)|
  ((s.pitch!=saved.pitch)<<3)|((((s.flags^saved.flags)&~0x10000000u)!=0)<<4)|
  ((memcmp(s.raw+8,saved.raw+8,8)!=0)<<5);
 note("original_check",mismatch);return !mismatch;
}
const dp_ops *dp_native_ops(void)
{
 static const dp_ops ops={prepare,stock,caps,candidate,at_stock,at_target,restore,at_original,clock_now,0};return &ops;
}
