/* SPDX-License-Identifier: MIT. The app owns its GPU buffers; VSH owns video RPCs. */
#include "video.h"
#include "../../app/source/hdmi_custom_protocol.h"
#include "../../plugin/vsh_video.h"
#include "backend.h"
#ifndef HC_VIDEO_HOST_TEST
#include <sys/file.h>
#include <sys/sys_time.h>
#else
extern uint64_t hc_video_mock_time(void);
extern int hc_video_mock_read(const char *,void *,unsigned);
extern int hc_video_mock_publish(const hc_video *);
extern int hc_video_mock_app_active(void);
#endif
#include <string.h>
static unsigned owner,serial;
static int active,departed,pending;
static uint64_t deadline;
static unsigned heartbeat_owner;
static uint64_t last_heartbeat;
static hc_video command;
static hc_video_fn native_call;
static void (*logger)(const char *,uint32_t);
static uint64_t clock_now(void){
#ifdef HC_VIDEO_HOST_TEST
 return hc_video_mock_time();
#else
 return (uint64_t)sys_time_get_system_time();
#endif
}
static void note(const char *s,uint32_t v){if(logger)logger(s,v);}
void hc_video_log(void (*log)(const char *,uint32_t)){logger=log;}
static int read_exact(const char *path,void *out,unsigned size)
{
#ifdef HC_VIDEO_HOST_TEST
 return hc_video_mock_read(path,out,size);
#else
 int fd;uint64_t got=0;unsigned char bytes[128];if(size>=sizeof(bytes))return -1;
 if(sysLv2FsOpen(path,SYS_O_RDONLY,&fd,0,0,0))return -1;
 int rc=sysLv2FsRead(fd,bytes,size+1,&got);sysLv2FsClose(fd);
 if(rc||got!=size)return -1;memcpy(out,bytes,size);return 0;
#endif
}
static int publish(void)
{
#ifdef HC_VIDEO_HOST_TEST
 return hc_video_mock_publish(&command);
#else
 int fd;uint64_t wrote=0;if(sysLv2FsOpen(HC_VIDEO ".stage",SYS_O_CREAT|SYS_O_WRONLY|SYS_O_TRUNC,&fd,0666,0,0))return -1;
 int rc=sysLv2FsWrite(fd,&command,sizeof(command),&wrote);sysLv2FsClose(fd);
 if(rc||wrote!=sizeof(command))return -1;
 (void)sysLv2FsUnlink(HC_VIDEO);return sysLv2FsRename(HC_VIDEO ".stage",HC_VIDEO);
#endif
}
static int live(unsigned token)
{
#ifdef HC_VIDEO_HOST_TEST
 int present=hc_video_mock_app_active();
#else
 sysFSStat st;int present=!sysLv2FsStat("/dev_hdd0/tmp/240p_app.active",&st);
#endif
 uint64_t now=clock_now();hc_app context;if(!present)return 0;
 /* LV2 rename cannot replace a file: publishing briefly removes its name.
  * A missing/partial sample never refreshes the cached lease. */
 if(read_exact(HC_APP,&context,sizeof(context)))return heartbeat_owner==token&&last_heartbeat<=now&&now-last_heartbeat<3000000;
 if(context.magic!=HC_MAGIC||context.owner!=token||!context.pid||context.heartbeat>now||now-context.heartbeat>=3000000)return 0;
 heartbeat_owner=token;last_heartbeat=context.heartbeat;return 1;
}
int hc_video_begin(unsigned token)
{
 if(pending||!live(token))return -1;
 owner=token;active=1;departed=0;memset(&command,0,sizeof(command));note("app_video_owner",owner);return 0;
}
int hc_video_app_alive(void)
{
 if(active&&!live(owner)){active=0;departed=1;pending=0;note("app_video_departed",owner);}
 return active;
}
int hc_video_departed(void){(void)hc_video_app_alive();int result=departed;departed=0;return result;}
int hc_video_submit(hc_video_fn fn,uint32_t flags,uint32_t mode,uint32_t format,uint32_t pitch)
{
 if(!fn)return -1;
 if(!hc_video_app_alive())return fn(0,flags,mode,format,pitch,0,1);
 if(pending){
  return command.flags==flags&&command.mode==mode&&command.format==format&&command.pitch==pitch?0:-1;
 }
 if(!++serial)++serial;
 command=(hc_video){HC_MAGIC,owner,serial,HC_VIDEO_QUIESCE,flags,mode,format,pitch,0,0,1};
 native_call=fn;pending=1;deadline=clock_now()+5000000;
 note("video_quiesce",serial);
 if(publish()){pending=0;command.phase=HC_VIDEO_ERROR;return -1;}
 return 0;
}
int hc_video_ready(void)
{if(!hc_video_app_alive())return 1;return command.phase==HC_VIDEO_ERROR?-1:!pending;}
int hc_video_scanout_matches(unsigned width,unsigned height,unsigned pitch)
{return hc_video_app_alive()&&!pending&&command.phase==HC_VIDEO_DONE&&command.factor&&
 command.width==width&&command.height==height&&command.pitch*command.factor==pitch;}
void hc_video_poll(void)
{
 if(!hc_video_app_alive()||!pending)return;
 hc_video_ack ack;int matched=!read_exact(HC_VIDEO_ACK,&ack,sizeof(ack))&&
  ack.magic==HC_MAGIC&&ack.owner==owner&&ack.operation==command.operation&&ack.phase==command.phase;
 if(matched&&ack.error){note("video_app_error",ack.error);command.phase=HC_VIDEO_ERROR;pending=0;return;}
 if(command.phase==HC_VIDEO_QUIESCE&&matched){
  note("video_quiesced",command.operation);
  int rc=native_call(0,command.flags,command.mode,command.format,command.pitch,0,1);note("video_native_rc",(unsigned)rc);
  if(rc){command.phase=HC_VIDEO_ERROR;pending=0;return;}
  command.phase=0;deadline=clock_now()+5000000;
 }else if(!command.phase){
  vsh_video_state s;
  if(!vsh_video_query(&s)&&!s.state&&s.mode==command.mode){
   if((s.mode!=10&&s.mode!=11&&s.mode!=12)||s.pitch>8192||(s.pitch&7)||s.pitch<4096){command.phase=HC_VIDEO_ERROR;pending=0;return;}
   command.pitch=s.pitch;command.format=s.format;
   command.width=s.mode==11?(s.pitch==4096?1024u:1280u):1920u;
   command.height=s.mode==11?(s.pitch==4096?768u:720u):1080u;
   command.factor=hc_backend_scanout_factor();command.phase=HC_VIDEO_BIND;deadline=clock_now()+5000000;
   note("video_bind_pitch",command.pitch);note("video_bind_factor",command.factor);
   if(publish()){command.phase=HC_VIDEO_ERROR;pending=0;return;}
  }
 }else if(command.phase==HC_VIDEO_BIND&&matched){
  command.phase=HC_VIDEO_DONE;pending=0;note("video_bound",command.operation);return;
 }
 if(pending&&clock_now()>=deadline){note("video_timeout",command.phase);command.phase=HC_VIDEO_ERROR;pending=0;}
}
