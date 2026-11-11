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
static uint32_t *buffers[2],*software,offsets[2],pitch,label_value;
static videoState state;
static videoDeviceInfo device;
static videoResolution resolution;
static ui_layout layout;
static ui_result drawn;
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
 cellGcmResetFlipStatus();pending=0;completed_frames++;
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
 ui_layout_init(&layout,res.width,res.height,next.displayMode.aspect==VIDEO_ASPECT_16_9,
  next.displayMode.scanMode==VIDEO_SCANMODE_INTERLACE);
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
  if(!os_active()){refresh=0;presented_hash=0;if(read_video())return -1;}
 }
 /* Same idle presentation policy as Buzz Bridge: unchanged screens stay on
  * the display while callbacks and controller input continue to be pumped. */
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
 /* Prime BOTH registered buffers before idle reuse. Complete and log the
  * initial flip pair so first-frame behavior can be checked on hardware. */
 if(hash==presented_hash&&submitted_frames>=2&&!os_active()){usleep(16000);return 0;}
 if(drain())return -1;
 ui_canvas canvas={software,resolution.width,NULL,NULL};
 if((!os_active()||!submitted_frames)&&ui_render(&layout,view,&canvas,&drawn))return -1;
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
 submitted_frames++;
 if(submitted_frames<=2)app_log("Frame %u flip submitted: buffer=%d",submitted_frames,front);
 pending=1;front^=1;if(!os_active())presented_hash=hash;usleep(10000);return 0;
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
 snprintf(text,sizeof(text),"Build 1.1.6 / diagnostics\nReported output: %ux%u %s\nPort: 0x%02x (RGB/SCART supported)\nNTSC tables: %s\nPAL tables: %s\nLV1 access: %s\nAV UART permission: %s (no command sent)\nLog: %s\n\n%s\n\n%s\n\nTiming tests enabled with 15-second automatic recovery safeguard.",
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
 uint64_t deadline=now()+15000000;int keep=0;
 while(!exiting&&now()<deadline){
  uint64_t t=now();unsigned remaining=t<deadline?(unsigned)((deadline-t+999999)/1000000):0;
  char text[512];snprintf(text,sizeof(text),"%s candidate loaded.\nCross: keep / Circle: restore\nAuto restore in %u seconds.\n\nIf the display still says 576i, choose restore. Native progressive output is unverified.",
   family?"PAL 288p":"NTSC 240p",remaining);
  ui_view view={0};view.title="Picture test";view.text=text;view.page=-1;
  if(frame(&view))break;unsigned key=input();
  if(!system_menu&&(key&CROSS)){keep=1;break;}
  if(key&CIRCLE)break;
 }
 if(keep){switch_keep(&flow);app_log("User kept candidate; analog waveform UNVERIFIED");status("Candidate kept","Timing table writes and video requests completed. Verify 288p/240p on the display. Exiting leaves these RAM edits until reboot; XMB/game behavior is unverified.");}
 else {
  int rc=switch_revert(&flow);app_log("Automatic/cancel recovery=%d %s",rc,flow.error);
  if(!exiting)status(rc?"Recovery needs reboot":"Previous settings restored",rc?flow.error:"Previous RAM values and display requests restored. The diagnostic log contains this attempt.");
 }
}
int main(void)
{
 (void)sysLv2FsMkdir("/dev_hdd0/tmp",0777);
 if(sysLv2FsOpen(LOG_PATH,SYS_O_WRONLY|SYS_O_CREAT|SYS_O_TRUNC,&log_fd,0666,NULL,0))log_fd=-1;
 app_log("PS3 240p build 1.1.6; native enabled=%u preparing=%u; timing tests ENABLED",CELL_VIDEO_OUT_OUTPUT_STATE_ENABLED,CELL_VIDEO_OUT_OUTPUT_STATE_PREPARING);
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
 const char *items[]={"Test PAL 288p (50 Hz)","Test NTSC 240p (60 Hz)","Restore stock timing","Output / diagnostics","Recheck timing memory","Exit to XMB"};
 int selected=0,cached_ntsc=STATE_UNKNOWN,cached_pal=STATE_UNKNOWN,outcome=0;
 uint64_t next_memory_poll=0;
 while(!exiting){
  if(!os_active()&&now()>=next_memory_poll){cached_ntsc=patch_state(&patch,0);cached_pal=patch_state(&patch,1);next_memory_poll=now()+1000000;}
  char title[200];snprintf(title,sizeof(title),"Analog SD timing / v1.1.6\nPAL: %s  NTSC: %s",memory_state(cached_pal),memory_state(cached_ntsc));
  ui_view view={0};view.title=title;view.items=items;view.count=6;view.selected=selected;view.page=-1;
  if(frame(&view)){app_log("Frame loop stopped: exit_request=%d gpu_ok=%d",exiting,gpu_ok);if(!exiting)outcome=1;break;}
  /* Complete the first pair before accepting a choice, including quit. */
  if(submitted_frames<2||completed_frames<2)continue;
  if(now()>=next_heartbeat){app_log("Menu alive: frames=%u completed=%u system_menu=%d",submitted_frames,completed_frames,system_menu);next_heartbeat=now()+15000000;}
  unsigned key=input();
  if(key&CIRCLE)break;
  if(key&UP)selected=(selected+5)%6;if(key&DOWN)selected=(selected+1)%6;
  if(key&CROSS){
   if(selected<2)test_mode(selected==0?MODE_288P_PAL:MODE_240P_NTSC);
   else if(selected==2){
    if(!switch_tests_available())status("Stock timing","Rebooting clears the timing edits from RAM. In-app mode transitions are disabled because the previous recovery lost the picture. This choice does not change output settings or timing memory.");
    else if(read_video()||inspect())status("Restore stopped",patch.error[0]?patch.error:"Output query failed");
    else {int rc=switch_stock(&flow,state.displayMode.resolution,analog());status(rc?"Restore stopped":"Stock timing restored",rc?flow.error:"Stock timing records verified and SD mode transition completed.");}
   }else if(selected==3)diagnostics();
   else if(selected==4){int rc=inspect();status(rc?"Memory unavailable":"Memory checked",rc?patch.error:"All target records are recognized. No memory writes performed.");}
   else break;
   next_memory_poll=0;
  }
 }
 if(flow.pending)(void)switch_revert(&flow);
 app_log("Exit; pending_recovery=%d",flow.pending);
 (void)ioPadEnd();(void)cellSysutilUnregisterCallback(0);
 if(log_fd>=0)(void)sysLv2FsClose(log_fd);
 /* Keep GPU-visible allocations alive through process teardown. */
 sysProcessExit(outcome);return outcome;
}
