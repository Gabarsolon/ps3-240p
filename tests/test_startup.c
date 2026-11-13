/* Run the production entry point with the native state observed on hardware. */
#define APP_HOST_TEST 1
#define main ps3_app_main
#include "../app/source/main.c"
#undef main
#include <assert.h>

CellGcmContextData mock_context;
static uint64_t mock_time;
static uint32_t mock_label;
static unsigned flips,configures,state_reads,post_config_reads;
static unsigned flip_mask;
static int scenario,configured,exit_code=-1;
static char log_capture[32768];
static size_t log_length;
static unsigned pad_calls;
static void (*sysutil_callback)(uint64_t,uint64_t,void *);
static void *sysutil_user;
static int overlay_opened,overlay_closed,quit_sent;
static uint64_t overlay_started;
static unsigned overlay_flips;
static uint32_t deferred_label;
static unsigned uart_probes;
static int late_overlay_write,repainted;
static uint32_t clean_pixel;
int mock_uart_permission(uint64_t buffer,uint64_t bytes,uint64_t mode)
{assert(!buffer&&!bytes&&mode==2);uart_probes++;return scenario==16?0:(int)0x80010003u;}
void *memalign(size_t a,size_t n){(void)a;return malloc(n);}
void *rsxMemalign(size_t a,size_t n){return memalign(a,n);}
int usleep(unsigned n){mock_time+=n;return 0;}
uint64_t sys_time_get_system_time(void){mock_time+=1000;return mock_time;}
void sysProcessExit(int code){exit_code=code;}
int sysLv2FsMkdir(const char *p,int mode){(void)p;(void)mode;return 0;}
int sysLv2FsOpen(const char *p,int flags,int *fd,int mode,void *u,int n)
{(void)p;(void)flags;(void)mode;(void)u;(void)n;*fd=1;return 0;}
int sysLv2FsWrite(int fd,const void *p,uint64_t n,uint64_t *w)
{(void)fd;assert(log_length+n<sizeof(log_capture));memcpy(log_capture+log_length,p,(size_t)n);log_length+=(size_t)n;*w=n;return 0;}
int sysLv2FsFsync(int fd){(void)fd;return 0;}
int sysLv2FsClose(int fd){(void)fd;return 0;}
int cellSysutilRegisterCallback(int slot,void (*fn)(uint64_t,uint64_t,void *),void *u)
{assert(slot==0);sysutil_callback=fn;sysutil_user=u;return 0;}
int cellSysutilUnregisterCallback(int slot){(void)slot;return 0;}
int cellSysutilCheckCallback(void)
{
 if(scenario==18&&flips>=2&&!overlay_opened){
  overlay_opened=1;overlay_started=mock_time;clean_pixel=buffers[0][0];
  sysutil_callback(CELL_SYSUTIL_DRAWING_BEGIN,0,sysutil_user);
  sysutil_callback(CELL_SYSUTIL_SYSTEM_MENU_OPEN,0,sysutil_user);
 }
 if(scenario==18&&overlay_opened&&!overlay_closed&&mock_time-overlay_started>=100000){
  overlay_closed=1;sysutil_callback(CELL_SYSUTIL_SYSTEM_MENU_CLOSE,0,sysutil_user);
  sysutil_callback(CELL_SYSUTIL_DRAWING_END,0,sysutil_user);
 }
 /* A late firmware overlay write AFTER the close events and the first
  * refreshed frames. With no controller input, both buffers must recover. */
 if(scenario==18&&overlay_closed&&!late_overlay_write&&mock_time-overlay_started>=300000){
  late_overlay_write=1;buffers[0][0]=buffers[1][0]=0x12345678;
 }
 if(scenario==18&&late_overlay_write&&!quit_sent&&mock_time-overlay_started>=1400000){
  repainted=buffers[0][0]==clean_pixel&&buffers[1][0]==clean_pixel;
  quit_sent=1;sysutil_callback(CELL_SYSUTIL_REQUEST_EXITGAME,0,sysutil_user);
 }
 /* Reproduce an extended, unchanged menu followed by an OS exit request.
  * The callback pump must keep working even when rendering is skipped. */
 if(scenario==17&&mock_time>=UINT64_C(480000000)&&!quit_sent){
  quit_sent=1;sysutil_callback(CELL_SYSUTIL_REQUEST_EXITGAME,0,sysutil_user);
 }
 if(scenario>=9&&scenario<=14&&flips>=2&&!overlay_opened){
  overlay_opened=1;overlay_started=mock_time;
  sysutil_callback(CELL_SYSUTIL_DRAWING_BEGIN,0,sysutil_user);
  if(scenario!=14)sysutil_callback(CELL_SYSUTIL_SYSTEM_MENU_OPEN,0,sysutil_user);
 }
 if(overlay_opened&&!overlay_closed){
  if((scenario==10&&flips>=5)||(scenario==13&&mock_time-overlay_started>=100000)){
   if(!quit_sent){quit_sent=1;sysutil_callback(CELL_SYSUTIL_REQUEST_EXITGAME,0,sysutil_user);}
  }else if(((scenario==9||scenario==14)&&flips>=6)||
           ((scenario==11||scenario==12)&&mock_time-overlay_started>=3000000)){
   overlay_closed=1;
   if(scenario!=14)sysutil_callback(CELL_SYSUTIL_SYSTEM_MENU_CLOSE,0,sysutil_user);
   sysutil_callback(CELL_SYSUTIL_DRAWING_END,0,sysutil_user);
   if(deferred_label)mock_label=deferred_label;
  }
 }
 return 0;
}
int cellGcmInit(unsigned a,unsigned b,void *p){(void)a;(void)b;(void)p;return 0;}
unsigned cellGcmGetFlipStatus(void){return scenario==7||((scenario==11||scenario==13)&&os_active())?1:0;}
void cellGcmResetFlipStatus(void){}
void *cellGcmGetLabelAddress(unsigned id){assert(id==255);return &mock_label;}
void cellGcmSetWriteBackEndLabel(CellGcmContextData *ctx,unsigned id,uint32_t value)
{(void)ctx;assert(id==255);if(scenario==12&&os_active())deferred_label=value;else mock_label=value;}
void cellGcmFlush(CellGcmContextData *ctx){(void)ctx;}
int cellGcmAddressToOffset(void *p,uint32_t *offset){(void)p;*offset=0;return 0;}
int cellGcmSetDisplayBuffer(uint8_t id,uint32_t offset,uint32_t pitch,uint32_t w,uint32_t h)
{(void)id;(void)offset;assert(pitch==w*4&&w==720&&h==576);return 0;}
void cellGcmSetFlipMode(int mode){(void)mode;}
int cellGcmSetFlip(CellGcmContextData *ctx,uint8_t id){(void)ctx;assert(id==flips%2);flip_mask|=1u<<id;flips++;if(os_active())overlay_flips++;return 0;}
void cellGcmSetWaitFlip(CellGcmContextData *ctx){(void)ctx;}
int videoGetState(int out,int dev,videoState *s)
{
 assert(out==0&&dev==0);state_reads++;memset(s,0,sizeof(*s));
 s->displayMode.resolution=5;s->displayMode.scanMode=0;s->displayMode.aspect=1;s->displayMode.refreshRates=2;
 s->state=CELL_VIDEO_OUT_OUTPUT_STATE_ENABLED;
 if(scenario==1)s->state=CELL_VIDEO_OUT_OUTPUT_STATE_DISABLED;
 if(scenario==2&&configured&&post_config_reads++<2)s->state=CELL_VIDEO_OUT_OUTPUT_STATE_PREPARING;
 if(scenario==3&&configured)s->state=CELL_VIDEO_OUT_OUTPUT_STATE_PREPARING;
 if(scenario==6&&configured)s->state=CELL_VIDEO_OUT_OUTPUT_STATE_DISABLED;
 return scenario==4?-1:0;
}
int videoGetResolution(int id,videoResolution *r){assert(id==5);r->width=720;r->height=576;return 0;}
int videoGetDeviceInfo(int out,int index,videoDeviceInfo *d){(void)out;(void)index;d->portType=VIDEO_PORT_SCART;return 0;}
int videoGetResolutionAvailability(int out,unsigned id,int aspect,int option)
{(void)out;(void)id;(void)aspect;(void)option;return 1;}
int videoConfigure(int out,videoConfiguration *cfg,void *option,int blocking)
{assert(out==0&&cfg->resolution==5&&cfg->pitch==2880&&blocking==0);(void)option;configures++;configured=1;return scenario==5?-1:0;}
int ioPadInit(int ports){assert(ports==7);return 0;}
int ioPadEnd(void){return 0;}
int ioPadGetInfo(padInfo *p){memset(p,0,sizeof(*p));p->status[0]=1;return 0;}
int ioPadGetData(int port,padData *p)
{
 assert(port==0&&flip_mask==3&&completed_frames>=2&&!os_active());memset(p,0,sizeof(*p));p->len=1;
 if(scenario==19||scenario==20){
  unsigned key=0;
  if(pad_calls<16){if(!(pad_calls&1))key=DOWN;}
  else {
   const unsigned script[]={CROSS,0,CROSS,0,UP,0,RIGHT,0,UP,0,0,0,CIRCLE,0,CIRCLE};
   unsigned step=pad_calls-16;assert(step<sizeof(script)/sizeof(script[0]));key=script[step];
   if(step==10)key=scenario==19?CROSS:CIRCLE;
  }
  p->BTN_DOWN=!!(key&DOWN);p->BTN_CROSS=!!(key&CROSS);p->BTN_UP=!!(key&UP);
  p->BTN_RIGHT=!!(key&RIGHT);p->BTN_CIRCLE=!!(key&CIRCLE);pad_calls++;
 }else if(scenario==15||scenario==16){
  if(pad_calls==0||pad_calls==2)p->BTN_UP=1;
  else if(pad_calls==4)p->BTN_CROSS=1;
  else if(pad_calls==5||pad_calls==7)p->BTN_CIRCLE=1;
  pad_calls++;assert(pad_calls<=8);
 }else if(scenario==17||scenario==18){pad_calls++;assert(pad_calls<30000);}
 else if(scenario==8){
  /* 7-item menu: DOWN, release, DOWN reach the PAL 288p test (item 2). */
  if(pad_calls==0||pad_calls==2)p->BTN_DOWN=1;
  else if(pad_calls==3)p->BTN_CROSS=1;
  else if(pad_calls==5||pad_calls==7)p->BTN_CIRCLE=1;
  pad_calls++;assert(pad_calls<=8);
 }else if(scenario>=9&&scenario<=14){
  /* Closing Circle remains held for two polls, then release and a new
   * Circle press. Neither of the first two polls may exit the app. */
  if(pad_calls<2||pad_calls==3)p->BTN_CIRCLE=1;
  pad_calls++;assert(pad_calls<=4);
 }else {p->BTN_CIRCLE=1;pad_calls++;}
 return 0;
}
int main(int argc,char **argv)
{
 assert(argc==2);scenario=atoi(argv[1]);
 /* The plugin deploy must compare CONTENT, not just length. A size-only
  * check skipped the deploy whenever a rebuild produced the same byte count,
  * so the console kept running the old plugin (2026-11-13: the v3 plugin
  * never reached the unit because the v2 plugin was also 8112 bytes). */
 assert(!deploy_needed(8112,UINT64_C(0x1111111111111111),8112,UINT64_C(0x1111111111111111)));
 assert(deploy_needed(8112,UINT64_C(0x1111111111111111),8112,UINT64_C(0x2222222222222222)));
 assert(deploy_needed(8112,UINT64_C(0x1111111111111111),8016,UINT64_C(0x1111111111111111)));
 assert(deploy_needed(8112,UINT64_C(0x1111111111111111),0,0));
 assert(deploy_needed(0,0,8112,UINT64_C(0x1111111111111111)));
 int digest_size=123;assert(file_digest("/nonexistent",&digest_size)==0&&digest_size==0);
 assert(!switch_tests_available());
 switch_flow production={0};production.patch=&patch;
 assert(switch_begin(&production,1,5,1)==-1&&configures==0&&state_reads==0);
 assert(switch_stock(&production,5,1)==-1&&configures==0&&state_reads==0);
 assert(strstr(production.error,"disabled"));
 int rc=ps3_app_main();
 if(scenario==19||scenario==20){
  assert(rc==0&&exit_code==0&&configures==1&&pad_calls==31&&!uart_probes);
  assert(gamma_value==(scenario==19?111:100)&&!gamma_preview);
  assert(strstr(log_capture,"Gamma preview value="));
 }else if(scenario==18){
  assert(rc==0&&exit_code==0&&quit_sent&&late_overlay_write&&repainted);
  assert(overlay_closed&&flips>8&&configures==1);
 }else if(scenario==17){
  assert(rc==0&&exit_code==0&&quit_sent&&configures==1&&flips==2);
  assert(mock_time>=UINT64_C(480000000)&&pad_calls>10000);
  assert(strstr(log_capture,"Sysutil EXIT_GAME")&&strstr(log_capture,"Exit; pending_recovery=0"));
 }else if(scenario==15||scenario==16){
  assert(rc==0&&exit_code==0&&configures==1&&uart_probes==1&&uart_probed);
  assert(!patch.ready&&backend==0&&pad_calls==8);
  assert(uart_permission==(scenario==16?0:(int)0x80010003u));
  /* A failed resolve must not emit the old, uninitialized probe result. */
  assert(!strstr(log_capture,"descriptor tables unavailable"));
  assert(!strstr(log_capture,"AV metadata profile="));
  assert(strstr(log_capture,scenario==16?"probe rc=00000000 bytes=0":"probe rc=80010003 bytes=0"));
 }else if(scenario>=9&&scenario<=14){
  assert(rc==0&&exit_code==0&&configures==1&&overlay_opened);
  assert(!strstr(log_capture,"GPU flip timeout")&&!strstr(log_capture,"GPU fence timeout"));
  assert(strstr(log_capture,"Sysutil DRAWING_BEGIN"));
  if(scenario==10||scenario==13){
   assert(quit_sent&&!overlay_closed&&pad_calls==0&&strstr(log_capture,"Sysutil EXIT_GAME"));
  }else {
   assert(overlay_closed&&!quit_sent&&pad_calls==4&&!os_active());
   assert(strstr(log_capture,"Sysutil DRAWING_END"));
   if(scenario==9||scenario==14)assert(overlay_flips>=4&&flips>=7);
   else assert(mock_time-overlay_started>=3000000);
  }
 }else if(scenario==8){
   assert(rc==0&&exit_code==0&&configures==1&&backend==0&&!patch.ready);
  assert(strstr(log_capture,"Blocked timing action family=1; no timing/output changes"));
  assert(flips==9&&pad_calls==8);
 }else if(scenario==0||scenario==2){
  assert(rc==0&&exit_code==0&&flips==2&&configures==1);
  assert(completed_frames==2&&backend==0&&!patch.ready&&!patch.error[0]);
  assert(strstr(log_capture,"Frame 1 rendered")&&strstr(log_capture,"Frame 2 flip completed"));
  assert(strstr(log_capture,"Video readback id=5 scan=0")&&strstr(log_capture,"Exit; pending_recovery=0"));
  if(scenario==2)assert(post_config_reads>=3);
 }else if(scenario==7){
  assert(rc==1&&exit_code==1&&flips==1&&strstr(log_capture,"GPU flip timeout"));
 }else {
  assert(rc==1&&flips==0);
  if(scenario==1||scenario==4)assert(configures==0);
  if(scenario==3||scenario==6)assert(strstr(log_capture,"Video transition timeout")&&mock_time>=5000000&&mock_time<6000000);
  if(scenario==5)assert(strstr(log_capture,"Startup stopped at video configure"));
 }
 if(scenario<15)assert(!uart_probes&&!uart_probed);
 printf("PASS: real app startup scenario %d (queries=%u, flips=%u)\n",scenario,state_reads,flips);return 0;
}
