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
#endif
#include "ui_layout.h"
#include "switch_flow.h"

SYS_PROCESS_PARAM(1001,0x100000);
#define HOST_SIZE (32u*1024*1024)
#define MAX_PIXELS (1920u*1080u)
#define LOG_PATH "/dev_hdd0/tmp/ps3_240p.log"
#define APP_ACTIVE_FLAG "/dev_hdd0/tmp/240p_app.active"
#define ENFORCER_FLAG "/dev_hdd0/tmp/240p_enforcer.on"
#define META240_FLAG "/dev_hdd0/tmp/240p_meta240.on"
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
static int inspect(void)
{
 if(flow.pending)return -1;
 uart_permission=probe_uart_permission();uart_probed=1;
 app_log("AV UART permission probe rc=%08x bytes=0; no command sent",(unsigned)uart_permission);
 memset(&patch,0,sizeof(patch));patch.log=app_log;
 memset(&cached_video,0,sizeof(cached_video));
 patch.read=read_mapi;patch.write=write_mapi;backend=1;
 if(patch_resolve(&patch)){
  patch.read=read_cfw;patch.write=write_cfw;backend=2;
  if(patch_resolve(&patch)){backend=0;return -1;}
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
 if(flow.pending)return 240;
 if(patch.ready){
  if(patch_state(&patch,0)==STATE_PATCHED&&state.displayMode.resolution==4)return 240;
  if(patch_state(&patch,1)==STATE_PATCHED&&state.displayMode.resolution==5)return 240;
 }
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
 lv2syscall5(8, 0x1ee7, slot, (uint64_t)(uintptr_t)path, 0, 0);
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
 lv2syscall2(8, 0x1ee8, slot);
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
static int cobra_mapi(unsigned op, uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
#ifndef APP_HOST_TEST
 lv2syscall6(8, 0x7777, op, a, b, c, d);
 return_to_user_prog(int);
#else
 (void)op;(void)a;(void)b;(void)c;(void)d;
 return (int)0x80010006u;
#endif
}
/* Slot scan (pattern from buzz-pad-bridge autoload/app.c): PS3MAPI op 0x47
 * reports the resident plugin name/path per slot, 0x80010006 = empty.
 * Returns the slot with a resident vsh_240p plugin if any, else the first
 * free slot, else 0. */
static unsigned find_slot(unsigned *resident) {
#ifndef APP_HOST_TEST
 static char name[96], path[256];
 unsigned free_slot=0;*resident=0;
 for (unsigned s=1;s<=6;s++) {
  name[0]=0;path[0]=0;
  int rc=cobra_mapi(0x47, s, (uint64_t)(uintptr_t)name, (uint64_t)(uintptr_t)path, 0);
  if (rc && rc != (int)0x80010006u) {app_log("VSH slot %u inspect rc=%08x", s, (unsigned)rc);continue;}
  app_log("VSH slot %u name=%s path=%s", s, name, path);
  if (name[0] && strstr(name, "vsh_240p")) {*resident=s;return s;}
  if (!name[0] && !path[0] && !free_slot) free_slot=s;
 }
 return free_slot;
#else
 (void)resident;return 0;
#endif
}
static void unload_enforcer(void) {
#ifndef APP_HOST_TEST
 static char name[96], path[256];
 unsigned found=0;
 for (unsigned s=1;s<=6;s++) {
  name[0]=0;path[0]=0;
  int rc=cobra_mapi(0x47, s, (uint64_t)(uintptr_t)name, (uint64_t)(uintptr_t)path, 0);
  if (rc==0 && name[0] && strstr(name, "vsh_240p")) {unload_vsh_plugin(s);found++;}
 }
 if (!found)app_log("Enforcer not resident");
#else
 (void)unload_vsh_plugin;(void)cobra_mapi;
#endif
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
 return read_video();
}
static int frame(const ui_view *view)
{
 if(!gpu_ok||!pump()||wait_flip())return -1;
 /* Follow the SDK sysmenu sample: freeze the scene but KEEP flipping while
  * the OS draws. Stop application input/edits, not the presentation loop. */
 if(refresh&&!os_active()){
  if(drain())return -1;
  if(!os_active()){refresh=0;presented_hash=0;hash_frames=0;if(read_video())return -1;}
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
 if(hash!=presented_hash){hash_frames=0;presented_hash=hash;}
 if(hash_frames>=2&&!os_active()){usleep(16000);return 0;}
 if(drain())return -1;
 int is_interlaced=(state.displayMode.scanMode==VIDEO_SCANMODE_INTERLACE);
 if(patch.ready&&(patch_state(&patch,0)==STATE_PATCHED||patch_state(&patch,1)==STATE_PATCHED))is_interlaced=0;
 ui_layout_init(&layout,resolution.width,eff_h,state.displayMode.aspect==VIDEO_ASPECT_16_9,is_interlaced);
 for(unsigned i=0;i<(unsigned)resolution.width*resolution.height;i++)software[i]=UI_BACKGROUND;
 ui_canvas canvas={software,resolution.width,NULL,NULL};
 if(pattern_mode){
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
enum {UP=1,DOWN=2,CROSS=4,CIRCLE=8};
static unsigned input(void)
{
 if(os_active())return 0;
 unsigned pressed=0;int any_held=0,saw_data=0;padInfo info={0};
 if(ioPadGetInfo(&info))return 0;
 for(int i=0;i<7;i++){
  if(!info.status[i]){held[i]=0;continue;}
  padData pd={0};if(ioPadGetData(i,&pd)||pd.len<=0){any_held|=held[i]!=0;continue;}
  saw_data=1;
  unsigned b=(pd.BTN_UP?UP:0)|(pd.BTN_DOWN?DOWN:0)|(pd.BTN_CROSS?CROSS:0)|(pd.BTN_CIRCLE?CIRCLE:0);
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
static void diagnostics(void)
{
 char text[1000];
 char cache[240];
 if(!patch.ready)snprintf(cache,sizeof(cache),"AV cache: select Recheck timing memory first.");
 else if(cached_video.error[0])snprintf(cache,sizeof(cache),"AV cache: %s",cached_video.error);
 else snprintf(cache,sizeof(cache),"AV cached raster (not a measurement):\nHead 0: %u active / %u total, state=%u\nHead 1: %u active / %u total, state=%u",
  cached_video.head[0].active_lines,cached_video.head[0].total_lines,cached_video.head[0].state,
  cached_video.head[1].active_lines,cached_video.head[1].total_lines,cached_video.head[1].state);
 snprintf(text,sizeof(text),"Build 1.2.0 / diagnostics\nReported output: %ux%u %s\nPort: 0x%02x (RGB/SCART supported)\nNTSC tables: %s\nPAL tables: %s\nLV1 access: %s\nAV UART permission: %s (no command sent)\nLog: %s\n\n%s\n\n%s\n\nTiming tests enabled with 15-second automatic recovery safeguard.",
  resolution.width,resolution.height,state.displayMode.scanMode==VIDEO_SCANMODE_INTERLACE?"interlaced":"progressive",
  device.portType,memory_state(patch_state(&patch,0)),memory_state(patch_state(&patch,1)),
  backend==1?"PS3MAPI":backend==2?"CFW 8/9":"not verified",
  !uart_probed?"not checked":!uart_permission?"accepted":(uint32_t)uart_permission==UINT32_C(0x80010003)?"denied":"see log",LOG_PATH,
  cache,patch.error[0]?patch.error:patch.ready?"Timing records match the supported 4.93 layout.":"Choose Recheck timing memory to inspect the tables.");
 status("Output / diagnostics",text);
}
static void test_mode(int family)
{
 if(!switch_tests_available()){
  app_log("Blocked timing action family=%d; no timing/output changes",family);
  status("Timing test unavailable","288p activation is being rebuilt after the failed signal test. Choose Recheck timing memory, then Output / diagnostics to inspect the current AV state.");return;
 }
 if(!log_ok||log_fd<0){status("Test stopped","Cannot save diagnostic log. No timing test started.");return;}
 if(read_video()||inspect()){status("Test stopped",patch.error[0]?patch.error:"Could not query output state");return;}
 /* Present and drain a normal frame before any output transition. */
 ui_view working={0};working.title="Changing SD timing";working.text="Testing candidate timing. Picture confirmation follows; automatic recovery in 15 seconds.";working.page=-1;
 if(frame(&working))return;
 app_log("BEGIN experiment family=%d original_id=%u",family,state.displayMode.resolution);
 if(switch_begin(&flow,family,state.displayMode.resolution,analog())){
  app_log("Experiment failed: %s",flow.error);status("Test stopped",flow.error);return;
 }
 if(!patch_read_cached_video(&patch,&cached_video)){
  int in_cache=patch_candidate_in_cache(&patch,&cached_video,family);
  app_log("Post-switch AV CACHE: match=%d Head0(active=%u,total=%u,scan=%08x,vid=%x) Head1(active=%u,total=%u,scan=%08x,vid=%x)",
   in_cache,
   cached_video.head[0].active_lines,cached_video.head[0].total_lines,cached_video.head[0].scan_control,cached_video.head[0].vid,
   cached_video.head[1].active_lines,cached_video.head[1].total_lines,cached_video.head[1].scan_control,cached_video.head[1].vid);
 }
 uint64_t deadline=now()+15000000;int keep=0;
 while(!exiting&&now()<deadline){
  uint64_t t=now();unsigned remaining=t<deadline?(unsigned)((deadline-t+999999)/1000000):0;
  char text[512];snprintf(text,sizeof(text),"%s candidate loaded (auto restore %us).\nCross: keep / Circle: restore\n\nRuns at quiet Batocera 15.66 kHz line rate (HTotal 3448) @ 60 Hz.\nListen to CRT flyback transformer whine.\nPress Cross to keep or Circle to restore.",
   family==MODE_240P_NTSC?"240p 60Hz (Quiet 15.66 kHz)":"PAL 288p 50Hz",remaining);
  ui_view view={0};view.title="Picture test";view.text=text;view.page=-1;
  if(frame(&view))break;unsigned key=input();
  if(!system_menu&&(key&CROSS)){keep=1;break;}
  if(key&CIRCLE)break;
 }
 if(keep){switch_keep(&flow);app_log("User kept candidate; analog waveform UNVERIFIED");status("Candidate kept","Timing table writes and video requests completed. Verify 240p on the display. Exiting leaves these RAM edits until reboot; XMB/game behavior is unverified.");}
 else {
  int rc=switch_revert(&flow);app_log("Automatic/cancel recovery=%d %s",rc,flow.error);
  if(!exiting)status(rc?"Recovery needs reboot":"Previous settings restored",rc?flow.error:"Previous RAM values and display requests restored. The diagnostic log contains this attempt.");
 }
}
int main(void)
{
 (void)sysLv2FsMkdir("/dev_hdd0/tmp",0777);
 if(sysLv2FsOpen(LOG_PATH,SYS_O_WRONLY|SYS_O_CREAT|SYS_O_TRUNC,&log_fd,0666,NULL,0))log_fd=-1;
 write_flag(APP_ACTIVE_FLAG);
 app_log("PS3 240p build 1.2.0; native enabled=%u preparing=%u; timing tests ENABLED",CELL_VIDEO_OUT_OUTPUT_STATE_ENABLED,CELL_VIDEO_OUT_OUTPUT_STATE_PREPARING);
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
 if(configure(NULL,initial)){app_log("Startup stopped at video configure");return 1;}
 rc=ioPadInit(7);if(rc){app_log("Startup controller init rc=%08x",(unsigned)rc);return 1;}
 flow.patch=&patch;flow.available=available;flow.configure=configure;
 /* Present the menu before any LV1 operation. Inspection is explicit via
  * Recheck timing memory, or occurs when starting a timing test. */
 app_log("UI ready; LV1 inspection deferred until selected");
 const char *items[]={
  "Enable 240p + 288p (Start Background Enforcer)",
  "Test 240p 60Hz (Quiet 15.66 kHz Batocera)",
  "Test PAL 288p 50Hz",
  "Restore Stock Timing (Stop Enforcer)",
  "Output / diagnostics",
  "Geometry pattern (measure crop)",
  "Recheck timing memory",
  "Exit to XMB"
 };
 int selected=0,cached_ntsc=STATE_UNKNOWN,cached_pal=STATE_UNKNOWN,outcome=0;
 uint64_t next_memory_poll=0;
 while(!exiting){
  if(!os_active()&&now()>=next_memory_poll){cached_ntsc=patch_state(&patch,0);cached_pal=patch_state(&patch,1);next_memory_poll=now()+1000000;}
  char title[200];snprintf(title,sizeof(title),"Analog SD timing / v1.2.0\nPAL-60: %s  NTSC: %s",memory_state(cached_pal),memory_state(cached_ntsc));
  ui_view view={0};view.title=title;view.items=items;view.count=8;view.selected=selected;view.page=-1;
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
  if(key&UP)selected=(selected+7)%8;if(key&DOWN)selected=(selected+1)%8;
  if(key&CROSS){
   if(selected==0){
    /* Single-variable geometry experiment: with /dev_hdd0/tmp/240p_meta240.on
     * present, the metadata record also publishes the patched 240-line height
     * instead of the stock 480/576 framebuffer height. */
    patch.meta_height_mode=flag_present(META240_FLAG)?1:0;
    app_log("Metadata height mode=%d (1 = patched active height)", patch.meta_height_mode);
    if(read_video()||inspect())status("Apply Error",patch.error[0]?patch.error:"Output query failed");
    else if(switch_begin(&flow, MODE_ALL_SD, state.displayMode.resolution, analog())){
     status("Apply Error", flow.error[0]?flow.error:"Failed to apply 240p/288p");
    } else {
     switch_keep(&flow);
     read_video();
     write_flag(ENFORCER_FLAG);
     deploy_enforcer();
     unsigned resident=0, slot=find_slot(&resident);
     if (resident) app_log("Enforcer already resident in slot %u", resident);
     else if (!slot) {app_log("No free VSH plugin slot");clear_flag(ENFORCER_FLAG);}
     else if (load_vsh_plugin(slot, "/dev_hdd0/plugins/vsh_240p.sprx")) clear_flag(ENFORCER_FLAG);
     status("240p/288p Enforcer Active",
      "Mode 4 and Mode 5 timings patched in RAM.\n"
      "Enforcer armed and running in VSH.\n\n"
      "- A 576i fallback is corrected to Mode 4 (240p) after ~2 s.\n"
      "- 480i and HD modes are never touched.\n"
      "- Passive while this app is open.\n\n"
      "RAM edits remain active until reboot.\n"
      "Exit to XMB to enjoy native scanlines!");
    }
   }
   else if(selected==1)test_mode(MODE_240P_NTSC);
   else if(selected==2)test_mode(MODE_288P_PAL);
   else if(selected==3){
    clear_flag(ENFORCER_FLAG);
    unload_enforcer();
    if(!switch_tests_available())status("Stock timing","Rebooting clears the timing edits from RAM.");
    else if(read_video()||inspect())status("Restore stopped",patch.error[0]?patch.error:"Output query failed");
    else {int rc=switch_stock(&flow,state.displayMode.resolution,analog());status(rc?"Restore stopped":"Stock timing restored",rc?flow.error:"Stock timing restored and background enforcer stopped.");}
   }else if(selected==4)diagnostics();
   else if(selected==5){pattern_mode=1;app_log("Geometry pattern shown; content=%u lines",(unsigned)active_raster_height());}
   else if(selected==6){int rc=inspect();status(rc?"Memory unavailable":"Memory checked",rc?patch.error:"All target records are recognized. No memory writes performed.");}
   else break;
   next_memory_poll=0;
  }
 }
 if(flow.pending)(void)switch_revert(&flow);
 app_log("Exit; pending_recovery=%d",flow.pending);
 clear_flag(APP_ACTIVE_FLAG);
 (void)ioPadEnd();(void)cellSysutilUnregisterCallback(0);
 if(log_fd>=0)(void)sysLv2FsClose(log_fd);
 /* Keep GPU-visible allocations alive through process teardown. */
 sysProcessExit(outcome);return outcome;
}
