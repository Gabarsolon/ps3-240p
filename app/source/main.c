/* SPDX-License-Identifier: MIT. Buzz Bridge software UI / GCM presentation.
 * Native low-line output is an experiment; table readback is not a waveform.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#ifdef APP_HOST_TEST
#include "app_mock.h"
#else
#include <malloc.h>
#include <unistd.h>
#include <stdatomic.h>
#include <ppu-lv2.h>
#include <sys/process.h>
#include <sys/file.h>
#include <sys/sys_time.h>
#include <sysutil/video.h>
#include <cell/cell_video_out.h>
#include <cell/gcm.h>
#include <cell/sysutil.h>
#include <rsx/rsx.h>
#include <io/pad.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <net/net.h>
#endif
#include "ui_layout.h"
#include "switch_flow.h"
#include "sd_default.h"
#include "plugin_lifecycle.h"
#include "gamma.h"
#include "gamma_protocol.h"
#include "gamma_ipc.h"
#ifdef CRT_ENABLE_HD540
#include "hd540_protocol.h"
#define CRT_BUILD_VERSION "1.5.0-dev19"
#else
#define CRT_BUILD_VERSION "1.4.3"
#endif

SYS_PROCESS_PARAM(1001,0x100000);
#define HOST_SIZE (32u*1024*1024)
#define MAX_PIXELS (1920u*1080u)
#define LOG_PATH "/dev_hdd0/tmp/ps3_240p.log"
#define APP_ACTIVE_FLAG "/dev_hdd0/tmp/240p_app.active"
#define ENFORCER_FLAG "/dev_hdd0/tmp/240p_enforcer.on"
#define TARGET_PATH "/dev_hdd0/tmp/240p_target.mode"
#define ENFORCER_READY "/dev_hdd0/tmp/240p_enforcer.ready"
#define META240_FLAG "/dev_hdd0/tmp/240p_meta240.on"
#define SCALER_FLAG "/dev_hdd0/tmp/240p_scaler.on"
#define GAMMA_PATH "/dev_hdd0/game/PS240P001/USRDIR/gamma.txt"
#define GAMMA_DIR "/dev_hdd0/game/PS240P001/USRDIR/shaders"
static int gamma_value=CRT_GAMMA_NEUTRAL,gamma_preview,gamma_digit;
static unsigned gamma_system_value;
static uint8_t gamma_ramp[256];
static uint32_t *buffers[2],*software,offsets[2],pitch,label_value;
static videoState state;
static videoDeviceInfo device;
static videoResolution resolution;
static ui_layout layout;
static ui_result drawn;
static int pattern_mode;
static patch_context patch;
static patch_cached_video cached_video;
static switch_flow flow;
static int exiting,system_menu,system_drawing,refresh,pending,front,log_fd=-1,log_ok=1,gpu_ok=1;
static int backend;
static int uart_probed,uart_permission;
static uint64_t log_bytes;
static unsigned held[7];
static int input_armed=1;
static uint64_t presented_hash;
static uint64_t redraw_until;
static unsigned hash_frames;
static unsigned submitted_frames,completed_frames;
static uint64_t next_heartbeat;
static struct {uint64_t status,param;} events[16];
static unsigned event_count,event_dropped;
static int os_active(void){return system_menu||system_drawing;}
static uint64_t now(void){return (uint64_t)sys_time_get_system_time();}
static void app_log(const char *fmt,...)
{
 if(log_fd<0||!log_ok||log_bytes>1024u*1024u)return;
 char line[768];int at=snprintf(line,sizeof(line),"%llu ",(unsigned long long)(now()/1000));
 va_list ap;va_start(ap,fmt);int n=vsnprintf(line+at,sizeof(line)-(size_t)at-2,fmt,ap);va_end(ap);
 if(n<0){log_ok=0;return;}
 size_t length=(size_t)n+(size_t)at;if(length>sizeof(line)-2)length=sizeof(line)-2;
 line[length++]='\n';uint64_t wrote=0;
 if(sysLv2FsWrite(log_fd,line,length,&wrote)||wrote!=length||sysLv2FsFsync(log_fd))log_ok=0;
 else log_bytes+=length;
}
static void event(uint64_t status,uint64_t param,void *arg)
{
 (void)arg;
 /* Do not perform file I/O from inside a firmware callback. */
 if(event_count<16){events[event_count].status=status;events[event_count++].param=param;}else event_dropped++;
 if(status==CELL_SYSUTIL_REQUEST_EXITGAME)exiting=1;
 if(status==CELL_SYSUTIL_DRAWING_BEGIN){system_drawing=1;input_armed=0;}
 if(status==CELL_SYSUTIL_DRAWING_END){system_drawing=0;refresh=1;}
 if(status==CELL_SYSUTIL_SYSTEM_MENU_OPEN){system_menu=1;input_armed=0;}
 if(status==CELL_SYSUTIL_SYSTEM_MENU_CLOSE){system_menu=0;refresh=1;}
}
static int pump(void)
{
 int rc=cellSysutilCheckCallback();
 for(unsigned i=0;i<event_count;i++){
  uint64_t s=events[i].status;
  const char *name=s==CELL_SYSUTIL_REQUEST_EXITGAME?"EXIT_GAME":s==CELL_SYSUTIL_DRAWING_BEGIN?"DRAWING_BEGIN":
   s==CELL_SYSUTIL_DRAWING_END?"DRAWING_END":s==CELL_SYSUTIL_SYSTEM_MENU_OPEN?"MENU_OPEN":
   s==CELL_SYSUTIL_SYSTEM_MENU_CLOSE?"MENU_CLOSE":"OTHER";
  app_log("Sysutil %s status=%04llx param=%llx menu=%d drawing=%d exit=%d",name,
   (unsigned long long)s,(unsigned long long)events[i].param,system_menu,system_drawing,exiting);
 }
 event_count=0;if(event_dropped){app_log("Sysutil dropped events=%u",event_dropped);event_dropped=0;}
 if(rc){app_log("Callback error=%08x",(unsigned)rc);exiting=1;}
 return !exiting;
}
#ifndef APP_HOST_TEST
static uint64_t read_mapi(void *u,uint64_t address)
{(void)u;lv2syscall3(8,0x7777,0x1008,address);return_to_user_prog(uint64_t);}
static void write_mapi(void *u,uint64_t address,uint64_t value)
{(void)u;lv2syscall4(8,0x7777,0x1009,address,value);}
static uint64_t read_cfw(void *u,uint64_t address)
{(void)u;lv2syscall1(8,address);return_to_user_prog(uint64_t);}
static void write_cfw(void *u,uint64_t address,uint64_t value)
{(void)u;lv2syscall2(9,address,value);}
/* 4.93 syscall 369 checks access, then returns on zero length before buffer
 * allocation/copy or UART lookup/send (docs/288P_ACTIVATION.md). */
static int probe_uart_permission(void)
{lv2syscall3(369,0,0,2);return_to_user_prog(int);}
#else
static uint64_t read_mapi(void *u,uint64_t address){(void)u;(void)address;return 0;}
static uint64_t read_cfw(void *u,uint64_t address){return read_mapi(u,address);}
static void write_mapi(void *u,uint64_t address,uint64_t value){(void)u;(void)address;(void)value;}
static void write_cfw(void *u,uint64_t address,uint64_t value){write_mapi(u,address,value);}
static int probe_uart_permission(void){return mock_uart_permission(0,0,2);}
#endif
static int flag_present(const char *path);
static int inspect(void)
{
 if(flow.pending)return -1;
 uart_permission=probe_uart_permission();uart_probed=1;
 app_log("AV UART permission probe rc=%08x bytes=0; no command sent",(unsigned)uart_permission);
 memset(&patch,0,sizeof(patch));patch.log=app_log;
 /* The experiment flags are read here, after the context is cleared and
  * before the targets are resolved: the scaler mode changes what the
  * resolver accepts as the candidate bytes. */
 patch.meta_height_mode=flag_present(META240_FLAG)?1:0;
 patch.scaler_mode=0; /* Retired after live register tests; use field fetching. */
 app_log("Metadata height mode=%d (1 = patched active height); scaler mode=%d",
  patch.meta_height_mode,patch.scaler_mode);
 if(patch.scaler_mode)app_log("Scaler candidate=Y-enable mask=0x10, source-step 2.0; execution and hardware effect UNVERIFIED");
 memset(&cached_video,0,sizeof(cached_video));
 patch.read=read_mapi;patch.write=write_mapi;backend=1;
 if(patch_resolve(&patch)){
  patch.read=read_cfw;patch.write=write_cfw;backend=2;
  if(patch_resolve(&patch)){backend=0;return -1;}
 }
 /* Log only after resolution populated the metadata snapshot. */
 for(int p=0;p<METADATA_PROBE_PROFILES;p++){
  const uint64_t *m=patch.metadata_probe[p];
  app_log("AV metadata profile=%d interlace=%u sample_multiplier=%u width_samples=%u height=%u family=%u (table RAM only)",
   p+1,(unsigned)m[0],(unsigned)(m[1]>>32),(unsigned)m[1],
   (unsigned)(m[2]>>32),(unsigned)m[2]);
 }
 app_log("LV1 backend=%s NTSC=%d PAL=%d",backend==1?"PS3MAPI":"CFW 8/9",patch_state(&patch,0),patch_state(&patch,1));
 if(patch_read_cached_video(&patch,&cached_video))app_log("AV cache snapshot unavailable: %s",cached_video.error);
 return 0;
}
static int wait_flip(void)
{
 if(!pending)return 0;
 uint64_t deadline=now()+2000000;
 while(cellGcmGetFlipStatus()!=0){
  if(!pump())return -1;
  /* Firmware can suspend application GPU work while its UI is active. */
  if(os_active())deadline=now()+2000000;
  else if(now()>=deadline){app_log("GPU flip timeout");gpu_ok=0;return -1;}
  usleep(1000);
 }
 pending=0;completed_frames++;
 if(completed_frames<=2)app_log("Frame %u flip completed",completed_frames);
 return 0;
}
static int drain(void)
{
 CellGcmContextData *ctx=CELL_GCM_CURRENT;uint32_t expected=++label_value;
 volatile uint32_t *label=(volatile uint32_t *)cellGcmGetLabelAddress(255);
 if(!label){gpu_ok=0;return -1;}
 cellGcmSetWriteBackEndLabel(ctx,255,expected);cellGcmFlush(ctx);
 uint64_t deadline=now()+2000000;
 while(*label!=expected){
  if(!pump())return -1;
  if(os_active())deadline=now()+2000000;
  else if(now()>=deadline){app_log("GPU fence timeout");gpu_ok=0;return -1;}
  usleep(1000);
 }
 return 0;
}
static int active_raster_height(void)
{
 /* Both field-fetch recipes consume the full 480/576-line framebuffer. */
 return resolution.height;
}
static int read_video(void)
{
 videoState next={0};videoResolution res={0};videoDeviceInfo info={0};
 int rc=videoGetState(0,0,&next);
 /* videoGetState is a thin adapter to native cellVideoOutGetState. Its
  * enabled state is 0; legacy VIDEO_STATE_ENABLED incorrectly denotes 1. */
 if(rc||next.state!=CELL_VIDEO_OUT_OUTPUT_STATE_ENABLED){app_log("Video state rc=%08x state=%u id=%u scan=%u",(unsigned)rc,next.state,next.displayMode.resolution,next.displayMode.scanMode);return -1;}
 rc=videoGetResolution(next.displayMode.resolution,&res);
 if(rc||res.width<640||res.height<480||res.width>1920||res.height>1080){app_log("Invalid resolution rc=%08x id=%u size=%ux%u",(unsigned)rc,next.displayMode.resolution,res.width,res.height);return -1;}
 int info_rc=videoGetDeviceInfo(0,0,&info);
 app_log("Video readback id=%u scan=%u aspect=%u refresh=%u surface=%ux%u port=%02x device_rc=%08x",
  next.displayMode.resolution,next.displayMode.scanMode,next.displayMode.aspect,next.displayMode.refreshRates,
  res.width,res.height,info.portType,(unsigned)info_rc);
 state=next;resolution=res;device=info;pitch=res.width*4u;
 for(int i=0;i<2;i++)if(cellGcmSetDisplayBuffer((uint8_t)i,offsets[i],pitch,res.width,res.height))return -1;
 int eff_h=active_raster_height();
 int is_interlaced=(next.displayMode.scanMode==VIDEO_SCANMODE_INTERLACE);
 if(patch.ready&&(patch_state(&patch,0)==STATE_PATCHED||patch_state(&patch,1)==STATE_PATCHED))is_interlaced=0;
 ui_layout_init(&layout,res.width,eff_h,next.displayMode.aspect==VIDEO_ASPECT_16_9,is_interlaced);
 return 0;
}
static int available(void *u,unsigned id)
{
 (void)u;int rc=videoGetResolutionAvailability(0,id,VIDEO_ASPECT_4_3,0);
 app_log("Resolution availability id=%u result=%08x",id,(unsigned)rc);return rc;
}
static int analog(void)
{
 return device.portType==VIDEO_PORT_COMPOSITE||device.portType==VIDEO_PORT_COMPONENT||
  device.portType==VIDEO_PORT_RGB||device.portType==VIDEO_PORT_SCART||device.portType==VIDEO_PORT_D;
}
static int load_vsh_plugin(unsigned slot, const char *path) {
#ifndef APP_HOST_TEST
 int rc;
 app_log("Cobra load begin slot=%u", slot);
 lv2syscall5(8, COBRA_LOAD_VSH_PLUGIN, slot, (uint64_t)(uintptr_t)path, 0, 0);
 rc=(int)p1; /* r3 holds the syscall result; register_passing_1 reads r4 */
 app_log("Cobra load slot=%u rc=%08x", slot, (unsigned)rc);
 return rc;
#else
 (void)slot; (void)path; return 0;
#endif
}
static int unload_vsh_plugin(unsigned slot) {
#ifndef APP_HOST_TEST
 int rc;
 lv2syscall2(8, COBRA_UNLOAD_VSH_PLUGIN, slot);
 rc=(int)p1;
 app_log("Cobra unload slot=%u rc=%08x", slot, (unsigned)rc);
 return rc;
#else
 (void)slot; return 0;
#endif
}
static int flag_present(const char *path) {
#ifndef APP_HOST_TEST
 sysFSStat st;return sysLv2FsStat(path,&st)==0;
#else
 (void)path;return 0;
#endif
}
static void write_flag(const char *path) {
#ifndef APP_HOST_TEST
 int fd=-1;if(sysLv2FsOpen(path,SYS_O_WRONLY|SYS_O_CREAT|SYS_O_TRUNC,&fd,0666,NULL,0)==0)sysLv2FsClose(fd);
#else
 (void)path;
#endif
}
static void clear_flag(const char *path) {
#ifndef APP_HOST_TEST
 (void)sysLv2FsUnlink(path);
#else
 (void)path;
#endif
}
static int write_target(unsigned target) {
#ifndef APP_HOST_TEST
 int fd=-1;uint64_t wr=0;char data[2]={(char)('0'+target),'\n'};
 if(sysLv2FsOpen(TARGET_PATH,SYS_O_WRONLY|SYS_O_CREAT|SYS_O_TRUNC,&fd,0666,NULL,0))return -1;
 int rc=sysLv2FsWrite(fd,data,sizeof(data),&wr);
 if(!rc)rc=sysLv2FsFsync(fd);
 sysLv2FsClose(fd);return rc||wr!=sizeof(data)?-1:0;
#else
 (void)target;return 0;
#endif
}
/* Named setting through the owner's existing webMAN service, loopback only.
 * Bound socket waits; accept a real HTTP 200 plus the setting's readback. */
static int registry_request(void *u,const char *path,char *reply,size_t capacity) {
 (void)u;
#ifndef APP_HOST_TEST
 int owned=netInitialize()==0,rc=-1,fd=socket(AF_INET,SOCK_STREAM,0);
 if(fd<0){if(owned)netDeinitialize();return -1;}
 struct timeval timeout={3,0};
 if(setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout))||
    setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout)))goto done;
 struct sockaddr_in addr;memset(&addr,0,sizeof(addr));addr.sin_family=AF_INET;
 addr.sin_port=htons(80);addr.sin_addr.s_addr=htonl(0x7f000001u);
 if(connect(fd,(struct sockaddr *)&addr,sizeof(addr)))goto done;
 char request[256];int n=snprintf(request,sizeof(request),"GET %s HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n",path);
 if(n<0||(size_t)n>=sizeof(request))goto done;
 for(int sent=0;sent<n;){int count=(int)send(fd,request+sent,(size_t)(n-sent),0);if(count<=0)goto done;sent+=count;}
 size_t used=0;reply[0]=0;
 while(used+1<capacity){
  int count=(int)recv(fd,reply+used,capacity-used-1,0);
  if(count<0)goto done;
  if(!count)break;
  used+=(size_t)count;reply[used]=0;
 }
 if(used+1==capacity)goto done;
 if(!strncmp(reply,"HTTP/1.0 200",12)||!strncmp(reply,"HTTP/1.1 200",12))rc=0;
done:
 socketclose(fd);if(owned)netDeinitialize();return rc;
#else
 (void)path;(void)reply;(void)capacity;return -1;
#endif
}
static int cobra_mapi(unsigned op, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
#ifndef APP_HOST_TEST
 lv2syscall6(8, 0x7777, op, a, b, c, d);
 return_to_user_prog(int);
#else
 (void)op;(void)a;(void)b;(void)c;(void)d;
 return (int)0x80010006u;
#endif
}
static int helper_info(void *u,unsigned slot,char *name,char *path)
{
 (void)u;
 int rc=cobra_mapi(0x47,slot,(uint64_t)(uintptr_t)name,(uint64_t)(uintptr_t)path,0);
 name[95]=path[255]=0;
 app_log("VSH slot %u rc=%08x name=%s path=%s",slot,(unsigned)rc,name,path);return rc;
}
static int helper_load(void *u,unsigned slot)
{(void)u;return load_vsh_plugin(slot,"/dev_hdd0/plugins/vsh_240p.sprx");}
static int helper_unload(void *u,unsigned slot)
{(void)u;return unload_vsh_plugin(slot);}
static plugin_lifecycle helper_ops(void)
{
 plugin_lifecycle ops={0};ops.info=helper_info;ops.load=helper_load;ops.unload=helper_unload;return ops;
}
/* Deploy is needed unless the installed file is byte-identical to the staged
 * build. A size-only comparison silently skipped the deploy whenever a
 * rebuild produced the same byte count: the console kept running the OLD
 * plugin while the app reported "already current", so a new plugin build
 * never reached the unit (observed 2026-11-13: the v3 plugin stayed on the
 * console because the v2 plugin had the same 8112-byte length). */
static int deploy_needed(int src_size,uint64_t src_hash,int dst_size,uint64_t dst_hash)
{
 return src_size<=0||dst_size!=src_size||dst_hash!=src_hash;
}
/* FNV-1a 64 over the file contents; also returns the byte count. */
static uint64_t file_digest(const char *path,int *size_out)
{
#ifndef APP_HOST_TEST
 int fd=-1;uint64_t hash=UINT64_C(14695981039346656037),total=0;
 if(sysLv2FsOpen(path,SYS_O_RDONLY,&fd,0,NULL,0)!=0)return 0;
 unsigned char buf[2048];uint64_t rd=0;
 while(sysLv2FsRead(fd,buf,sizeof(buf),&rd)==0&&rd>0){
  total+=rd;
  for(uint64_t i=0;i<rd;i++)hash=(hash^buf[i])*UINT64_C(1099511628211);
 }
 sysLv2FsClose(fd);
 if(size_out)*size_out=(int)total;
 return total?hash:0;
#else
 (void)path;if(size_out)*size_out=0;return 0;
#endif
}
static void deploy_enforcer(void) {
#ifndef APP_HOST_TEST
 const char *src_path="/dev_hdd0/game/PS240P001/USRDIR/vsh_240p.sprx";
 const char *dst_path="/dev_hdd0/plugins/vsh_240p.sprx";
 int src_size=0,dst_size=0;
 uint64_t src_hash=file_digest(src_path,&src_size);
 uint64_t dst_hash=file_digest(dst_path,&dst_size);
 app_log("Enforcer staged=%u bytes hash=%08x%08x installed=%u bytes hash=%08x%08x",
  (unsigned)src_size,(unsigned)(src_hash>>32),(unsigned)(src_hash&0xffffffffu),
  (unsigned)dst_size,(unsigned)(dst_hash>>32),(unsigned)(dst_hash&0xffffffffu));
 if(!deploy_needed(src_size,src_hash,dst_size,dst_hash)){
  app_log("Enforcer plugin already current (%u bytes)",(unsigned)dst_size);
  return;
 }
 if(src_size<=0){app_log("Enforcer deploy: USRDIR source missing or empty");return;}
 int in_fd=-1,out_fd=-1;
 if(sysLv2FsOpen(src_path,SYS_O_RDONLY,&in_fd,0,NULL,0)==0){
  if(sysLv2FsOpen(dst_path,SYS_O_WRONLY|SYS_O_CREAT|SYS_O_TRUNC,&out_fd,0777,NULL,0)==0){
   char buf[2048];uint64_t rd=0,wr=0;int bad=0;
   while(sysLv2FsRead(in_fd,buf,sizeof(buf),&rd)==0&&rd>0){
    if(sysLv2FsWrite(out_fd,buf,rd,&wr)!=0||wr!=rd){bad=1;break;}
   }
   sysLv2FsClose(out_fd);
   if(bad)app_log("Enforcer deploy: write error");
  }
  sysLv2FsClose(in_fd);
 }
 int done_size=0;uint64_t done_hash=file_digest(dst_path,&done_size);
 if(!deploy_needed(src_size,src_hash,done_size,done_hash))
  app_log("Enforcer deployed to %s (%u bytes hash=%08x%08x)",dst_path,(unsigned)done_size,
   (unsigned)(done_hash>>32),(unsigned)(done_hash&0xffffffffu));
 else
  app_log("Enforcer deploy FAILED (now %u bytes hash=%08x%08x, expected %u bytes hash=%08x%08x)",
   (unsigned)done_size,(unsigned)(done_hash>>32),(unsigned)(done_hash&0xffffffffu),
   (unsigned)src_size,(unsigned)(src_hash>>32),(unsigned)(src_hash&0xffffffffu));
#endif
}
static int configure(void *u,unsigned id)
{
 (void)u;
 if(wait_flip()||drain())return -1;
 videoResolution res={0};videoConfiguration cfg={0};
 int rc=videoGetResolution((int)id,&res);if(rc)return -1;
 cfg.resolution=(uint8_t)id;cfg.format=VIDEO_BUFFER_FORMAT_XRGB;
 cfg.aspect=state.displayMode.aspect==VIDEO_ASPECT_16_9?VIDEO_ASPECT_16_9:VIDEO_ASPECT_4_3;
 cfg.pitch=res.width*4u;
 /* Nonblocking firmware request; wait for an enabled, matching readback. */
 rc=videoConfigure(0,&cfg,NULL,0);app_log("videoConfigure id=%u result=%08x",id,(unsigned)rc);
 if(rc)return -1;
 uint64_t deadline=now()+5000000;
 for(;;){
  videoState next={0};rc=videoGetState(0,0,&next);
  if(rc){app_log("Video transition query=%08x",(unsigned)rc);return -1;}
  if(next.state==CELL_VIDEO_OUT_OUTPUT_STATE_ENABLED&&next.displayMode.resolution==id)break;
  (void)pump();if(now()>=deadline){app_log("Video transition timeout id=%u state=%u reported_id=%u",id,next.state,next.displayMode.resolution);return -1;}
  usleep(1000);
 }
 /* Registering display buffers does not initialize the scanout format.
  * Check the real app configuration before drawing packed 32-bit pixels. */
 videoConfiguration actual={0};rc=videoGetConfiguration(0,&actual,NULL);
 app_log("Framebuffer configuration readback rc=%08x id=%u format=%u pitch=%u expected_pitch=%u",
  (unsigned)rc,actual.resolution,actual.format,actual.pitch,cfg.pitch);
 if(rc||actual.resolution!=id||actual.format!=VIDEO_BUFFER_FORMAT_XRGB||actual.pitch!=cfg.pitch)return -1;
 return read_video();
}
static int frame(const ui_view *view)
{
 if(!gpu_ok||!pump()||wait_flip())return -1;
 /* Follow the SDK sysmenu sample: freeze the scene but KEEP flipping while
  * the OS draws. Stop application input/edits, not the presentation loop. */
 if(refresh&&!os_active()){
  if(drain())return -1;
  if(!os_active()){
   refresh=0;presented_hash=0;hash_frames=0;
   /* VSH can finish writing dimming/spinner pixels after its close events.
    * Repaint through the handoff instead of caching the first two frames. */
   redraw_until=now()+1000000;
   if(read_video())return -1;
  }
 }
 /* Prime BOTH registered buffers on content change before idle reuse. */
 uint64_t hash=UINT64_C(14695981039346656037);
 const char *strings[258];int string_count=0;
 strings[string_count++]=view->title;strings[string_count++]=view->text;
 for(int i=0;view->items&&i<view->count&&i<256;i++)strings[string_count++]=view->items[i];
 for(int i=0;i<string_count;i++){
  const unsigned char *s=(const unsigned char *)(strings[i]?strings[i]:"");
  do{hash=(hash^*s)*UINT64_C(1099511628211);}while(*s++);
 }
 hash=(hash^(unsigned)view->selected)*UINT64_C(1099511628211);
 hash=(hash^(unsigned)view->page)*UINT64_C(1099511628211);
 hash=(hash^resolution.width)*UINT64_C(1099511628211);
 hash=(hash^resolution.height)*UINT64_C(1099511628211);
 int eff_h=active_raster_height();
 hash=(hash^(unsigned)eff_h)*UINT64_C(1099511628211);
 hash=(hash^(unsigned)pattern_mode)*UINT64_C(1099511628211);
 hash=(hash^(unsigned)gamma_preview)*UINT64_C(1099511628211);
 hash=(hash^(unsigned)gamma_value)*UINT64_C(1099511628211);
 hash=(hash^(unsigned)gamma_digit)*UINT64_C(1099511628211);
 hash=(hash^gamma_system_value)*UINT64_C(1099511628211);
 if(hash!=presented_hash){hash_frames=0;presented_hash=hash;}
 if(hash_frames>=2&&!os_active()&&now()>=redraw_until){usleep(16000);return 0;}
 if(drain())return -1;
 int is_interlaced=(state.displayMode.scanMode==VIDEO_SCANMODE_INTERLACE);
 if(patch.ready&&(patch_state(&patch,0)==STATE_PATCHED||patch_state(&patch,1)==STATE_PATCHED))is_interlaced=0;
 ui_layout_init(&layout,resolution.width,eff_h,state.displayMode.aspect==VIDEO_ASPECT_16_9,is_interlaced);
 for(unsigned i=0;i<(unsigned)resolution.width*resolution.height;i++)software[i]=UI_BACKGROUND;
 ui_canvas canvas={software,resolution.width,NULL,NULL};
 if(gamma_preview){
  if(ui_render_gamma(&layout,&canvas,&drawn,gamma_value,gamma_digit,gamma_ramp,(int)gamma_system_value))return -1;
 }else if(pattern_mode){
  /* Pattern fills the public buffer: the low-line content sits on top, the
   * magenta band marks the area a cropped 240-line raster would hide. */
  if(ui_render_pattern(&canvas,resolution.width,resolution.height,eff_h<64?64:eff_h))return -1;
 } else if((!os_active()||!submitted_frames)&&ui_render(&layout,view,&canvas,&drawn))return -1;
 memcpy(buffers[front],software,(size_t)pitch*resolution.height);
 if(submitted_frames<2){
  unsigned ink=0,first=0;
  for(unsigned i=0;i<(unsigned)resolution.width*resolution.height;i++)if(software[i]!=UI_BACKGROUND){if(!ink)first=i;ink++;}
  app_log("Frame %u rendered: buffer=%d offset=%08x size=%ux%u ink=%u sample=%08x/%08x",
   submitted_frames+1,front,offsets[front],resolution.width,resolution.height,ink,software[first],buffers[front][first]);
 }
 atomic_thread_fence(memory_order_seq_cst);cellGcmResetFlipStatus();
 int rc=cellGcmSetFlip(CELL_GCM_CURRENT,(uint8_t)front);
 if(rc){app_log("Flip submission=%08x",(unsigned)rc);gpu_ok=0;return -1;}
 cellGcmSetWaitFlip(CELL_GCM_CURRENT);cellGcmFlush(CELL_GCM_CURRENT);
 submitted_frames++;hash_frames++;
 if(submitted_frames<=2)app_log("Frame %u flip submitted: buffer=%d",submitted_frames,front);
 pending=1;front^=1;usleep(10000);return 0;
}
enum {UP=1,DOWN=2,CROSS=4,CIRCLE=8,LEFT=16,RIGHT=32};
static unsigned input(void)
{
 if(os_active())return 0;
 unsigned pressed=0;int any_held=0,saw_data=0;padInfo info={0};
 if(ioPadGetInfo(&info))return 0;
 for(int i=0;i<7;i++){
  if(!info.status[i]){held[i]=0;continue;}
  padData pd={0};if(ioPadGetData(i,&pd)||pd.len<=0){any_held|=held[i]!=0;continue;}
  saw_data=1;
  unsigned b=(pd.BTN_UP?UP:0)|(pd.BTN_DOWN?DOWN:0)|(pd.BTN_CROSS?CROSS:0)|(pd.BTN_CIRCLE?CIRCLE:0)|
   (pd.BTN_LEFT?LEFT:0)|(pd.BTN_RIGHT?RIGHT:0);
  pressed|=b&~held[i];held[i]=b;any_held|=b!=0;
 }
 /* Circle used to dismiss the OS menu must not become an app quit. */
 if(!input_armed){if(saw_data&&!any_held)input_armed=1;return 0;}
 return pressed;
}
static void status(const char *title,const char *text)
{
 int page=0;ui_view view={0};view.title=title;view.text=text;view.page=page;
 while(!exiting){
  view.page=page;if(frame(&view))break;unsigned key=input();
  page=drawn.page;
  if(key&CIRCLE)break;
  if((key&UP)&&page>0)page--;
  if((key&DOWN)&&page+1<drawn.pages)page++;
  if(key&CROSS){if(page+1<drawn.pages)page++;else break;}
 }
}
static const char *memory_state(int s)
{return s==STATE_STOCK?"stock":s==STATE_PATCHED?"candidate":s==STATE_MIXED?"partial":"unavailable";}

static void gamma_edit(void)
{
 int original=gamma_value;gamma_preview=1;gamma_digit=1;
 (void)crt_gamma_ramp(gamma_value,gamma_ramp);
 ui_view view={0};view.title="Gamma preview";
 while(!exiting){
  if(frame(&view))break;unsigned key=input();
  if(key&CIRCLE){gamma_value=original;break;}
  if(key&CROSS)break;
  if(key&LEFT)gamma_digit=(gamma_digit+2)%3;
  if(key&RIGHT)gamma_digit=(gamma_digit+1)%3;
  const int steps[]={100,10,1};
  if(key&UP)gamma_value+=steps[gamma_digit];
  if(key&DOWN)gamma_value-=steps[gamma_digit];
  if(gamma_value<CRT_GAMMA_MIN)gamma_value=CRT_GAMMA_MIN;
  if(gamma_value>CRT_GAMMA_MAX)gamma_value=CRT_GAMMA_MAX;
  if(key&(UP|DOWN))(void)crt_gamma_ramp(gamma_value,gamma_ramp);
 }
 gamma_preview=0;
 app_log("Gamma preview value=%d hundredths; app framebuffer only",gamma_value);
}
static int gamma_write(const char *path,const char *text)
{
 FILE *f=fopen(path,"wb");if(!f)return -1;
 size_t n=strlen(text);int failed=fwrite(text,1,n,f)!=n;
 if(fflush(f))failed=1;if(fclose(f))failed=1;
 if(failed)return -1;
 f=fopen(path,"rb");if(!f)return -1;
 for(size_t i=0;i<n;i++)if(fgetc(f)!=(unsigned char)text[i]){fclose(f);return -1;}
 failed=fgetc(f)!=EOF||ferror(f);if(fclose(f))failed=1;return failed?-1:0;
}
static void gamma_export(void)
{
 char shader[2048],path[256],preset[256],number[5],message[640];
 crt_gamma_format(gamma_value,number);
 if(crt_gamma_shader(gamma_value,shader,sizeof(shader))<0){status("Gamma export failed","Could not generate shader.");return;}
 (void)sysLv2FsMkdir(GAMMA_DIR,0777);
 snprintf(path,sizeof(path),GAMMA_DIR "/crt-gamma-%s.cg",number);
 int rc=gamma_write(path,shader);
 snprintf(preset,sizeof(preset),"shaders = \"1\"\nshader0 = \"crt-gamma-%s.cg\"\nfilter_linear0 = \"false\"\n",number);
 snprintf(path,sizeof(path),GAMMA_DIR "/crt-gamma-%s.cgp",number);
 if(!rc)rc=gamma_write(path,preset);
 app_log("Gamma shader export value=%d rc=%d path=%s",gamma_value,rc,path);
 if(rc){status("Gamma export failed","Could not write and verify both shader files. Existing RetroArch settings were not changed.");return;}
 snprintf(message,sizeof(message),"Gamma %s shader written and read back.\n\nIn RetroArch: Quick Menu > Shaders. Enable shaders and load:\n%s\n\nSave a game/core preset if desired. This affects RetroArch content only. XMB, PS1_emu and other games are unchanged. RetroArch hardware validation pending.",number,path);
 status("RetroArch gamma shader exported",message);
}
static int gamma_load_helper(void *u,unsigned slot)
{
 /* Called only for a fresh load. An old flag can survive reboot even though
  * its timing patches cannot; loading gamma must not switch the SD family. */
 clear_flag(ENFORCER_FLAG);return helper_load(u,slot);
}
static int gamma_helper(void)
{
 int source_size=0,runtime_size=0;
 uint64_t source=file_digest("/dev_hdd0/game/PS240P001/USRDIR/vsh_240p.sprx",&source_size);
 deploy_enforcer();
 uint64_t runtime=file_digest("/dev_hdd0/plugins/vsh_240p.sprx",&runtime_size);
 if(deploy_needed(source_size,source,runtime_size,runtime))return -1;
 plugin_lifecycle ops=helper_ops();ops.load=gamma_load_helper;unsigned slot=0;int reused=0;
 /* Capability is checked below. An old resident helper needs a reboot;
  * gamma must not stop a working timing enforcer to replace it. */
 if(helper_start(&ops,0,&slot,&reused)){app_log("Gamma helper: %s",ops.error);return -1;}
 uint64_t deadline=now()+5000000;
 ui_view view={0};view.title="Preparing native gamma";view.text="Waiting for the resident display helper...";
 while(!flag_present(GAMMA_READY)&&now()<deadline&&!exiting){if(frame(&view))break;usleep(50000);}
 return flag_present(GAMMA_READY)?0:-1;
}
static int gamma_exchange(unsigned action,unsigned value)
{
#ifndef APP_HOST_TEST
 static uint32_t sequence;
 if(!sequence)sequence=(uint32_t)now();if(!++sequence)sequence++;
 gamma_request request={GAMMA_MAGIC,sequence,action,value};
 int error=0,published=gamma_publish(GAMMA_REQUEST,&request,&error);
 app_log("Gamma publish action=%u sequence=%u stage=%d errno=%d",action,sequence,published,error);
 if(published)return -2;
 uint64_t deadline=now()+3000000;
 ui_view view={0};view.title="Native gamma";view.text="Waiting for the AV service...";
 while(!exiting&&now()<deadline){
  FILE *f=fopen(GAMMA_REPLY,"rb");
  if(f){gamma_reply reply={0};size_t n=fread(&reply,1,sizeof(reply),f);fclose(f);
   if(n==sizeof(reply)&&reply.magic==GAMMA_MAGIC&&reply.sequence==sequence){
    gamma_system_value=reply.value>=50&&reply.value<=400?reply.value:0;
    app_log("Gamma reply action=%u status=%u value=%u",action,reply.status,reply.value);
    return reply.status==GAMMA_OK?0:reply.status==GAMMA_UNSUPPORTED?-3:
     reply.status==GAMMA_EXPIRED?-5:-4;
   }
  }
  if(frame(&view))break;usleep(50000);
 }
 app_log("Gamma acknowledgement timeout action=%u sequence=%u",action,sequence);return -6;
#else
 (void)action;(void)value;return -1;
#endif
}
static void gamma_native_test(void)
{
 if(gamma_helper()){
  status("Gamma helper unavailable","Reboot once after updating from an older build, then reopen CRT Toolset and try again. Native gamma currently requires the audited 4.93 CEX VSH layout. Existing timing enforcement has not been stopped.");return;
 }
 int result=gamma_exchange(GAMMA_TEST,(unsigned)gamma_value);
 if(result){
  (void)gamma_exchange(GAMMA_CANCEL,0);
  const char *message=result==-2?"Could not publish the command to the helper. No new gamma test was started. See the app log for the filesystem error.":
   result==-3?"The helper could not inspect a supported active output. Native gamma currently requires 720x480/576 analog RGB8. See the helper log.":
   result==-5?"The recovery timer expired before the app received confirmation. The helper restored the previous curve. Start a new test and press Cross to keep it before the countdown ends. The timer continues while the PS menu is open.":
   result==-6?"The helper did not acknowledge the command in time. Any test it received still has automatic recovery. See the app and helper logs.":
   "The helper reported that the native AV call or readback failed. Automatic restoration remains armed if needed. See the helper log.";
  status("Native gamma test stopped",message);return;
 }
 uint64_t deadline=now()+12000000;
 while(!exiting&&now()<deadline){
  char text[512],number[5];crt_gamma_format(gamma_value,number);
  uint64_t t=now();unsigned remaining=t<deadline?(unsigned)((deadline-t+999999)/1000000):0;
  snprintf(text,sizeof(text),"Native analog gamma %s\n\nCross: keep for this session\nCircle: restore previous curve\nAutomatic restore in %u seconds.\n\nCheck shadows and color. Global picture quality and game transitions need CRT validation. Do not combine with a gamma shader.",number,remaining);
  ui_view v={0};v.title="Keep native gamma?";v.text=text;v.page=-1;
  if(frame(&v))break;unsigned key=input();
  if(key&CIRCLE)break;
  if(key&CROSS){
   if(!gamma_exchange(GAMMA_KEEP,(unsigned)gamma_value)){
    status("Native gamma kept","The AV service accepted the curve. It remains active after exiting this app until restored, changed by another app, or rebooted. This experimental build does not force it back after game video changes. Choose Restore system gamma to undo it.");return;
   }
   break;
  }
 }
 int rc=gamma_exchange(GAMMA_CANCEL,0);
 if(!exiting)status(rc?"Gamma restore not confirmed":"Previous gamma restored",rc?"The helper's recovery timer will retry. See the helper log.":"The saved three-channel curve was restored through the native AV service.");
}
static void gamma_menu(void)
{
 int selected=0;
#ifndef APP_HOST_TEST
 gamma_system_value=0;
 if(flag_present(GAMMA_READY)){
  FILE *f=fopen(GAMMA_REPLY,"rb");if(f){gamma_reply reply={0};size_t n=fread(&reply,1,sizeof(reply),f);fclose(f);
   if(n==sizeof(reply)&&reply.magic==GAMMA_MAGIC&&reply.value>=50&&reply.value<=400)gamma_system_value=reply.value;
  }
 }
#endif
 while(!exiting){
  char label[80],number[5];crt_gamma_format(gamma_value,number);
  snprintf(label,sizeof(label),"Enter gamma value: %s",number);
  const char *options[]={label,"Test system gamma (15-second recovery)","Restore system gamma","Reset value to 1.00 (neutral)","Save gamma preference","Export RetroArch Cg shader","About gamma / calibration","Back"};
  const int count=(int)(sizeof(options)/sizeof(options[0]));
  ui_view v={0};v.title="Gamma / analog RGB experiment";v.items=options;v.count=count;v.selected=selected;v.page=-1;
  if(frame(&v))break;unsigned key=input();if(key&CIRCLE)break;
  if(key&UP)selected=(selected+count-1)%count;if(key&DOWN)selected=(selected+1)%count;
  if(key&CROSS){
   if(selected==0)gamma_edit();
   else if(selected==1)gamma_native_test();
   else if(selected==2){int rc=gamma_helper()||gamma_exchange(GAMMA_RESTORE,0);status(rc?"Gamma restore not confirmed":"System gamma restored",rc?"Reboot to load the new helper if an old one remains resident. See the helper log.":"The curve saved before the native test has been restored.");}
   else if(selected==3){gamma_value=100;}
   else if(selected==4){int rc=crt_gamma_save(GAMMA_PATH,gamma_value);app_log("Save gamma preference value=%d rc=%d",gamma_value,rc);status(rc?"Gamma save failed":"Gamma preference saved",rc?"The setting could not be saved and verified.":"Saved for CRT Toolset's next start. Saving does not apply a curve or enable gamma at boot. Choose Test system gamma to apply it.");}
   else if(selected==5)gamma_export();
   else if(selected==6)status("Gamma / CRT calibration","1.00 is neutral. Higher values brighten shadows and midtones.\n\nCalibrate the TV first:\n1. Set normal Contrast/Picture.\n2. Adjust Brightness so black is dark, keeping near-black detail visible. A black screen alone is not enough.\n3. Adjust gamma using the ramps and normal pictures, then test and keep it.\n\nAfter changing TV contrast or brightness, recheck black level, then gamma. The controls can interact.\n\nCRT Brightness sets black level. Contrast sets picture gain/white output; a CRT has no backlight.\n\nThe editor preview does not apply native gamma. Test system gamma applies it with timed recovery. Saving remembers only the value.\n\nUse supported CFW and analog RGB8 SD output. Games may reset the curve. RetroArch export is a separate alternative; avoid applying both.\n\nGamma cannot recover source values clipped to zero or repair TV hardware.");
   else break;
  }
 }
}
#ifdef CRT_ENABLE_HD540
static unsigned hd_sequence;
static unsigned hd_pending_sequence;
static int hd_publish(unsigned action,unsigned *sequence)
{
 if(!hd_sequence)hd_sequence=(unsigned)now();if(!++hd_sequence)++hd_sequence;
 /* Reuse the tested atomic 16-byte inbox publisher, with the HD magic. */
 gamma_request envelope={HD540_MAGIC,hd_sequence,action,(action==HD540_TEST||action==HD540_ROUNDTRIP||action==HD540_HDMI_CONTROL||action==HD540_HDMI_720_CONTROL)?HD540_EXPERIMENTAL_VALUE:0};
 int error=0,rc=gamma_publish(HD540_REQUEST,&envelope,&error);
 app_log("HD publish action=%u seq=%u rc=%d errno=%d",action,hd_sequence,rc,error);
 *sequence=hd_sequence;return rc;
}
static int hd_reply(unsigned sequence,hd540_reply *reply)
{
 FILE *f=fopen(HD540_REPLY,"rb");if(!f)return -1;
 unsigned char data[sizeof(*reply)+1];size_t n=fread(data,1,sizeof(data),f);int bad=ferror(f);fclose(f);
 if(bad||n!=sizeof(*reply))return -1;
 memcpy(reply,data,sizeof(*reply));
 return reply->magic==HD540_MAGIC&&reply->sequence==sequence&&reply->status<=HD540_WAIT_APP_EXIT?0:-1;
}
static void hd_run(unsigned action)
{
 if(read_video()||device.portType!=1||state.displayMode.resolution!=1||
    state.displayMode.scanMode!=0||!(state.displayMode.refreshRates&5u)||
    (state.displayMode.refreshRates&2u)||flag_present(ENFORCER_FLAG)){
  app_log("HD app preflight refused port=%u res=%u scan=%u refresh=%u",device.portType,state.displayMode.resolution,state.displayMode.scanMode,state.displayMode.refreshRates);
  status("HD experiment unavailable","Start with HDMI 1080i at 59.94/60Hz. Use Pause SD enforcement in this submenu if needed; reboot to clear old SD timing edits. This first build does not test component or RGB SCART.");return;
 }
 if(gamma_helper()||!flag_present(HD540_READY)){
  status("HD helper unavailable","Install the matching experimental build and reboot once, then reopen CRT Toolset. An older resident helper cannot perform this test. No HD patch was requested.");return;
 }
 unsigned sequence=0;if(hd_publish(action,&sequence)){
  status("HD command not sent","Could not publish the helper command. See the app log.");return;
 }
 if(action!=HD540_INSPECT)hd_pending_sequence=sequence;
 const char *names[]={"Inspection passed","Leaving 1080i","Entering candidate timing","Candidate active: 15-second test",
  "Restoring timing RAM","Recovery: selecting 720p","Recovery: returning to 1080i","Stock 1080i restored",
  "Experiment refused","Recovery stopped: reboot required","Waiting for app exit"};
 uint64_t deadline=now()+40000000;unsigned last=UINT32_MAX;int cancelled=0;
 while(!exiting&&now()<deadline){
  hd540_reply reply={0};int valid=!hd_reply(sequence,&reply);
  if(valid&&reply.status!=last){last=reply.status;app_log("HD state=%u error=%u",reply.status,reply.detail);}
  if(valid&&reply.status==HD540_WAIT_APP_EXIT&&action!=HD540_INSPECT){
   app_log("HD test queued; normal app exit before any display change");
   hd_pending_sequence=0;exiting=1;return;
  }
  if(valid&&(reply.status==HD540_DONE||reply.status==HD540_REFUSED||
      (action==HD540_INSPECT&&reply.status==HD540_IDLE))){
   if(action!=HD540_INSPECT)hd_pending_sequence=0;
   status(reply.status==HD540_REFUSED?"HD inspection refused":reply.status==HD540_DONE?"Stock 1080i restored":"HD preflight passed",
    reply.status==HD540_REFUSED?"The output/firmware guards or prerequisites did not pass. First run the stock round trip, then the stock 720p AVI-only control before the 540p candidate. Check its picture before proceeding. See /dev_hdd0/tmp/vsh_240p.log.":
    reply.status==HD540_DONE?(reply.detail?"A test/transition error occurred. The helper restored timing RAM and confirmed stock 1080i in the AV cache. This is not proof of a 540p waveform.":
    "The helper restored timing RAM and confirmed stock 1080i in the AV cache. Save the capture and logs for analysis; picture stability alone does not prove 540p."):
    "HDMI 1080i, native transition mapping and stock 4.93 timing records passed inspection. No display or timing changes were made.");return;
  }
  char text[512];const char *phase=valid?names[reply.status]:"Waiting for helper acknowledgement";
  const char *experiment=action==HD540_HDMI_720_CONTROL?
   "Control: stock progressive 720p timing and transmitter template. Only the generated AVI video code is cleared for 15 seconds.":action==HD540_HDMI_CONTROL?
   "Control: stock 1080i timing and transmitter template. Only the generated AVI video code is cleared for 15 seconds.":
   "Candidate: unverified progressive 540-active / 562-total raster with full source dimensions and AVI code 0, held for 15 seconds.";
  snprintf(text,sizeof(text),"%s\n\n%s\n\nAutomatic recovery restores RAM, selects stock 720p, then returns to 1080i.\n\nCircle: request early recovery. Leave the controller untouched to observe the test.",phase,experiment);
  ui_view view={0};view.title="Experimental HDMI 540p";view.text=text;view.page=-1;
  if(frame(&view))break;
  if((input()&CIRCLE)&&!cancelled){unsigned ignored=0;(void)hd_publish(HD540_CANCEL,&ignored);cancelled=1;}
  usleep(50000);
 }
 unsigned ignored=0;(void)hd_publish(HD540_CANCEL,&ignored);
 if(!exiting)status("HD recovery not confirmed","The app stopped waiting and requested cancellation. The resident helper retains recovery and retries. If the picture does not return, reboot. Do not start another timing experiment.");
}
static void hd_menu(void)
{
 const char *items[]={"Inspect HDMI 1080i (read only)","Test stock 720p / 1080i round trip (exit to XMB)","Test AVI packet only (stock 720p; 15 seconds)","Test AVI packet only (stock 1080i; 15 seconds)","Test 540p with custom AVI packet (15 seconds)","Pause SD enforcement (no video switch)","Back"};int selected=0;
 while(!exiting){
  if(hd_pending_sequence){
   status("HD recovery pending","The previous HD test has not confirmed restoration. Wait for recovery or reboot before another timing/gamma change.");return;
  }
  ui_view view={0};view.title="HD CRT experiments / dev19";view.items=items;view.count=7;view.selected=selected;view.page=-1;
  if(frame(&view))break;unsigned key=input();if(key&CIRCLE)break;
  if(key&UP)selected=(selected+6)%7;if(key&DOWN)selected=(selected+1)%7;
  if(key&CROSS){
   if(selected==6)break;
   if(selected==5){
    clear_flag(ENFORCER_FLAG);int armed=flag_present(ENFORCER_FLAG);
    app_log("HD pause SD enforcement: armed=%d",armed);
    status(armed?"Could not pause SD enforcement":"SD enforcement paused",
     armed?"Could not remove the helper enable flag; see the app log.":"The background SD corrector is disarmed. The video mode was not changed. Reboot if old timing edits are still active; HD inspection will check the stock records.");
   }else hd_run(selected==0?HD540_INSPECT:selected==1?HD540_ROUNDTRIP:selected==2?HD540_HDMI_720_CONTROL:selected==3?HD540_HDMI_CONTROL:HD540_TEST);
  }
 }
}
#endif
static void diagnostics(void)
{
 char text[1000];
 char cache[240];
 if(!patch.ready)snprintf(cache,sizeof(cache),"AV cache: select Recheck timing memory first.");
 else if(cached_video.error[0])snprintf(cache,sizeof(cache),"AV cache: %s",cached_video.error);
 else snprintf(cache,sizeof(cache),"AV cached raster (not a measurement):\nHead 0: %u active / %u total, state=%u\nHead 1: %u active / %u total, state=%u",
  cached_video.head[0].active_lines,cached_video.head[0].total_lines,cached_video.head[0].state,
  cached_video.head[1].active_lines,cached_video.head[1].total_lines,cached_video.head[1].state);
 snprintf(text,sizeof(text),"CRT Toolset 1.4.3 / diagnostics\nReported output: %ux%u %s\nPort: 0x%02x (RGB/SCART supported)\nNTSC tables: %s\nPAL tables: %s\nLV1 access: %s\nAV UART permission: %s (no command sent)\nLog: %s\n\n%s\n\n%s\n\nTiming tests enabled with 15-second automatic recovery safeguard.",
  resolution.width,resolution.height,state.displayMode.scanMode==VIDEO_SCANMODE_INTERLACE?"interlaced":"progressive",
  device.portType,memory_state(patch_state(&patch,0)),memory_state(patch_state(&patch,1)),
  backend==1?"PS3MAPI":backend==2?"CFW 8/9":"not verified",
  !uart_probed?"not checked":!uart_permission?"accepted":(uint32_t)uart_permission==UINT32_C(0x80010003)?"denied":"see log",LOG_PATH,
  cache,patch.error[0]?patch.error:patch.ready?"Timing records match the supported 4.93 layout.":"Choose Recheck timing memory to inspect the tables.");
 status("Output / diagnostics",text);
}
/* Called only after a checked transition; no persistent setting changes until
 * the owner accepts the picture. Losing sync therefore leaves a timer running. */
static int confirm_picture(const char *label)
{
 uint64_t deadline=now()+15000000;
 while(!exiting&&now()<deadline){
  uint64_t t=now();unsigned remaining=t<deadline?(unsigned)((deadline-t+999999)/1000000):0;
  char text[384];snprintf(text,sizeof(text),"%s\n\nCross: keep / Circle: restore\nAutomatic restore in %u seconds.\n\nCheck the full picture and display sync.",label,remaining);
  ui_view view={0};view.title="Keep this picture?";view.text=text;view.page=-1;
  if(frame(&view))break;unsigned key=input();
  if(!os_active()&&(key&CROSS))return 1;
  if(key&CIRCLE)break;
 }
 return 0;
}
static void recover_picture(void)
{
 int rc=switch_revert(&flow);app_log("Automatic/cancel recovery=%d %s",rc,flow.error);
 if(!exiting)status(rc?"Recovery needs reboot":"Previous settings restored",rc?flow.error:"Previous RAM values and display mode restored. The saved boot default was not changed.");
}
static int begin_picture(int family,unsigned target)
{
 if(!switch_tests_available()){
  app_log("Blocked timing action family=%d; no timing/output changes",family);
  status("Timing test unavailable","This build disables display transitions.");return -1;
 }
 if(!log_ok||log_fd<0){status("Test stopped","Cannot save diagnostic log. No timing test started.");return -1;}
 if(read_video()||inspect()){status("Test stopped",patch.error[0]?patch.error:"Could not query output state");return -1;}
 ui_view view={0};view.title="Changing SD timing";view.text="Picture confirmation follows; automatic recovery in 15 seconds.";view.page=-1;
 if(frame(&view))return -1;
 app_log("BEGIN experiment family=%d target=%u original_id=%u",family,target,state.displayMode.resolution);
 if(switch_begin_target(&flow,family,target,state.displayMode.resolution,analog())){
  app_log("Experiment failed: %s",flow.error);status("Test stopped",flow.error);return -1;
 }
 int cache_rc=patch_read_cached_video(&patch,&cached_video);
 int active=!cache_rc&&patch_candidate_on_head(&patch,&cached_video,target==5?1:0,1);
 app_log("Post-switch analog AV cache match=%d Head0=%u/%u Head1=%u/%u (software cache only)",
  active,cached_video.head[0].active_lines,cached_video.head[0].total_lines,
  cached_video.head[1].active_lines,cached_video.head[1].total_lines);
 if(!active){
  int rc=switch_revert(&flow);
  status("Timing was not activated",rc?flow.error:"The active analog timing did not match the candidate. Previous timing and mode were restored. See the diagnostic log.");return -1;
 }
 return 0;
}
static void test_mode(int family)
{
 unsigned target=family==MODE_288P_PAL?5:4;
 if(begin_picture(family,target))return;
 if(!confirm_picture(target==5?"PAL 288p / nominal 50 Hz":"NTSC 240p / nominal 60 Hz")){recover_picture();return;}
 switch_keep(&flow);app_log("Owner kept candidate; electrical field parity UNMEASURED");
 status("Candidate kept","RAM timing changes remain until reboot. The saved boot default and background enforcer preference were not changed.");
}
static void enable_mode(unsigned target)
{
 if(begin_picture(MODE_ALL_SD,target))return;
 if(!confirm_picture(target==5?"Enable PAL 288p / nominal 50 Hz":"Enable NTSC 240p / nominal 60 Hz")){recover_picture();return;}
 switch_keep(&flow);
 clear_flag(ENFORCER_FLAG);
 if(write_target(target)){status("Picture enabled; helper stopped","Could not save the background mode preference. RAM timings are active until reboot.");return;}
 int src_size=0,dst_size=0;
 uint64_t src_hash=file_digest("/dev_hdd0/game/PS240P001/USRDIR/vsh_240p.sprx",&src_size);
 uint64_t dst_hash=file_digest("/dev_hdd0/plugins/vsh_240p.sprx",&dst_size);
 int replace=deploy_needed(src_size,src_hash,dst_size,dst_hash);
 deploy_enforcer();
 dst_hash=file_digest("/dev_hdd0/plugins/vsh_240p.sprx",&dst_size);
 if(deploy_needed(src_size,src_hash,dst_size,dst_hash)){
  status("Picture enabled; helper stopped","Background plugin deployment could not be verified. RAM timings are active until reboot.");return;
 }
 plugin_lifecycle ops=helper_ops();unsigned slot=0;int reused=0;
 if(helper_start(&ops,replace,&slot,&reused)){
  app_log("Enforcer start failed: %s",ops.error);
  status("Picture enabled; helper stopped",ops.error);return;
 }
 app_log("Enforcer %s slot=%u target=%u",reused?"reused":"started",slot,target);
 uint64_t ready_deadline=now()+5000000;
 while(!flag_present(ENFORCER_READY)&&now()<ready_deadline&&!exiting){(void)pump();usleep(50000);}
 if(!flag_present(ENFORCER_READY)){
  app_log("Enforcer native worker not ready");
  status("Picture enabled; helper stopped","The native display worker is not ready. Reboot if an older helper remains loaded, then enable again. See the helper log.");return;
 }
 write_flag(ENFORCER_FLAG);
 if(!flag_present(ENFORCER_FLAG)){
  status("Picture enabled; helper stopped","Could not create the background enforcement flag.");return;
 }
 status(target==5?"288p background mode enabled":"240p background mode enabled",
  target==5?"PAL 288p selected. SD fallbacks are corrected to Mode 5 after about two seconds. HD output is untouched. Exit to XMB. Reboot clears the RAM timings.":
            "NTSC 240p selected. SD fallbacks are corrected to Mode 4 after about two seconds. HD output is untouched. Exit to XMB. Reboot clears the RAM timings.");
}
static void stock_mode(unsigned target)
{
 if(!switch_tests_available()){status("Stock timing","This build disables display transitions.");return;}
 if(read_video()||inspect()){status("Switch stopped",patch.error[0]?patch.error:"Output query failed");return;}
 if(switch_stock_begin(&flow,target,state.displayMode.resolution,analog())){status("Switch stopped",flow.error);return;}
 if(!confirm_picture(target==5?"Stock 576i / 50 Hz. Cross also saves this boot default.":"Stock 480i / 60 Hz. Cross also saves this boot default.")){recover_picture();return;}
 int rc=sd_default_save(registry_request,NULL,target);app_log("Saved SD default target=%u rc=%d",target,rc);
 if(rc){
  int recovered=switch_revert(&flow);
  status("Default could not be saved",rc==-2?"webMAN did not verify recovery of the old registry setting. Check /setting/display/tvSystem before rebooting.":
   recovered?flow.error:"Previous picture restored. webMAN must be running with xRegistry access to save the boot default.");return;
 }
 switch_keep(&flow);clear_flag(ENFORCER_FLAG);
 app_log("Stock selected; background correction disarmed (resident helper retained for reuse)");
 status(target==5?"576i selected and saved":"480i selected and saved",
  "Stock timing is active now. The selected XMB/boot default was saved and read back through webMAN. Background correction is stopped. No reboot was requested.");
}
int main(void)
{
 (void)sysLv2FsMkdir("/dev_hdd0/tmp",0777);
 if(sysLv2FsOpen(LOG_PATH,SYS_O_WRONLY|SYS_O_CREAT|SYS_O_TRUNC,&log_fd,0666,NULL,0))log_fd=-1;
 write_flag(APP_ACTIVE_FLAG);
 app_log("CRT Toolset build " CRT_BUILD_VERSION "; native enabled=%u preparing=%u; timing tests ENABLED",CELL_VIDEO_OUT_OUTPUT_STATE_ENABLED,CELL_VIDEO_OUT_OUTPUT_STATE_PREPARING);
 void *host=memalign(1024*1024,HOST_SIZE);
 if(!host){app_log("Startup failed: host allocation");return 1;}
 int rc=cellSysutilRegisterCallback(0,event,NULL);if(rc){app_log("Startup callback rc=%08x",(unsigned)rc);return 1;}
 rc=cellGcmInit(0x100000,HOST_SIZE,host);if(rc){app_log("Startup GCM rc=%08x",(unsigned)rc);return 1;}
 for(int i=0;i<2;i++){
  buffers[i]=rsxMemalign(64,MAX_PIXELS*4u);
  if(!buffers[i]||cellGcmAddressToOffset(buffers[i],&offsets[i])){app_log("Startup display allocation failed index=%d",i);return 1;}
  app_log("Display allocation %d address=%p offset=%08x",i,(void *)buffers[i],offsets[i]);
 }
 software=malloc(MAX_PIXELS*4u);if(!software){app_log("Startup software allocation failed");return 1;}
 cellGcmSetFlipMode(GCM_FLIP_VSYNC);cellGcmResetFlipStatus();
 if(read_video()){app_log("Startup stopped at video query");return 1;}
 unsigned initial=state.displayMode.resolution;
 unsigned initial_scan=state.displayMode.scanMode;
 videoConfiguration inherited={0};int inherited_rc=videoGetConfiguration(0,&inherited,NULL);
 app_log("Inherited framebuffer rc=%08x id=%u format=%u pitch=%u scan=%u",
  (unsigned)inherited_rc,inherited.resolution,inherited.format,inherited.pitch,initial_scan);
 /* Even when the resolution is unchanged, explicitly initialize XRGB32
  * and the app's pitch. Dev1's HD shortcut inherited the XMB surface setup. */
 if(configure(NULL,initial)){app_log("Startup stopped at video configure");return 1;}
 app_log("Startup scan readback before=%u after=%u",initial_scan,state.displayMode.scanMode);
 rc=ioPadInit(7);if(rc){app_log("Startup controller init rc=%08x",(unsigned)rc);return 1;}
 flow.patch=&patch;flow.available=available;flow.configure=configure;
 if(crt_gamma_load(GAMMA_PATH,&gamma_value))gamma_value=CRT_GAMMA_NEUTRAL;
 app_log("Gamma preference=%d hundredths; not automatically applied",gamma_value);
 /* Present the menu before any LV1 operation. Inspection is explicit via
  * Recheck timing memory, or occurs when starting a timing test. */
 app_log("UI ready; LV1 inspection deferred until selected");
 const char *items[]={
  "Test 240p 60Hz (15-second recovery)",
  "Enable 240p 60Hz + background enforcer",
  "Test PAL 288p 50Hz (15-second recovery)",
  "Enable 288p 50Hz + background enforcer",
  "Stock 480i 60Hz + save boot default",
  "Stock 576i 50Hz + save boot default",
  "Output / diagnostics",
  "Geometry pattern (measure crop)",
  "Gamma / shadow correction",
  "Recheck timing memory",
#ifdef CRT_ENABLE_HD540
  "HD CRT / experimental 540p",
#endif
  "Exit to XMB"
 };
 const int item_count=(int)(sizeof(items)/sizeof(items[0]));
 int selected=0,cached_ntsc=STATE_UNKNOWN,cached_pal=STATE_UNKNOWN,outcome=0;
 uint64_t next_memory_poll=0;
 while(!exiting){
#ifdef CRT_ENABLE_HD540
  if(hd_pending_sequence){
   hd540_reply reply;
   if(!hd_reply(hd_pending_sequence,&reply)&&(reply.status==HD540_DONE||reply.status==HD540_REFUSED))hd_pending_sequence=0;
  }
#endif
  if(!os_active()&&now()>=next_memory_poll){cached_ntsc=patch_state(&patch,0);cached_pal=patch_state(&patch,1);next_memory_poll=now()+1000000;}
  char title[200];snprintf(title,sizeof(title),"CRT Toolset / v" CRT_BUILD_VERSION "\nPAL: %s  NTSC: %s",memory_state(cached_pal),memory_state(cached_ntsc));
  ui_view view={0};view.title=title;view.items=items;view.count=item_count;view.selected=selected;view.page=-1;
  if(frame(&view)){app_log("Frame loop stopped: exit_request=%d gpu_ok=%d",exiting,gpu_ok);if(!exiting)outcome=1;break;}
  /* Complete the first pair before accepting a choice, including quit. */
  if(submitted_frames<2||completed_frames<2)continue;
  if(now()>=next_heartbeat){app_log("Menu alive: frames=%u completed=%u system_menu=%d",submitted_frames,completed_frames,system_menu);next_heartbeat=now()+15000000;}
  unsigned key=input();
  if(pattern_mode){
   if(key&(CIRCLE|CROSS)){pattern_mode=0;app_log("Geometry pattern closed");}
   continue;
  }
  if(key&CIRCLE)break;
  if(key&UP)selected=(selected+item_count-1)%item_count;if(key&DOWN)selected=(selected+1)%item_count;
  if(key&CROSS){
#ifdef CRT_ENABLE_HD540
   if(hd_pending_sequence&&(selected<6||selected==8)){
    status("HD recovery pending","Wait for confirmed stock recovery or reboot before another timing/gamma change.");continue;
   }
#endif
   if(selected==0)test_mode(MODE_240P_NTSC);
   else if(selected==1)enable_mode(4);
   else if(selected==2)test_mode(MODE_288P_PAL);
   else if(selected==3)enable_mode(5);
   else if(selected==4)stock_mode(4);
   else if(selected==5)stock_mode(5);
   else if(selected==6)diagnostics();
   else if(selected==7){pattern_mode=1;app_log("Geometry pattern shown; content=%u lines",(unsigned)active_raster_height());}
   else if(selected==8)gamma_menu();
   else if(selected==9){int rc=inspect();status(rc?"Memory unavailable":"Memory checked",rc?patch.error:"All target records are recognized. No memory writes performed.");}
#ifdef CRT_ENABLE_HD540
   else if(selected==10)hd_menu();
#endif
   else break;
   next_memory_poll=0;
  }
 }
 if(flow.pending)(void)switch_revert(&flow);
#ifdef CRT_ENABLE_HD540
 if(hd_pending_sequence){unsigned ignored=0;(void)hd_publish(HD540_CANCEL,&ignored);}
#endif
 app_log("Exit; pending_recovery=%d",flow.pending);
 clear_flag(APP_ACTIVE_FLAG);
 app_log("Exit: active flag cleared");
 (void)ioPadEnd();app_log("Exit: pad closed");
 (void)cellSysutilUnregisterCallback(0);app_log("Exit: callbacks removed; process termination next");
 if(log_fd>=0)(void)sysLv2FsClose(log_fd);
 /* Keep GPU-visible allocations alive through process teardown. */
 sysProcessExit(outcome);return outcome;
}
