/* Exercise the real helper handshake without a GPU, console or filesystem. */
#include "../experiments/hdmi_custom/video.h"
#include "../app/source/hdmi_custom_protocol.h"
#include "../plugin/vsh_video.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t now=100;
static int alive=1,calls,fail_publish,context_gap;
static unsigned factor=1,source_pitch=5120;
static hc_video sent;
static hc_video_ack ack;
static vsh_video_state native;
uint64_t hc_video_mock_time(void){return now;}
int hc_video_mock_app_active(void){return alive;}
int hc_video_mock_read(const char *path,void *out,unsigned size)
{if(!strcmp(path,HC_APP)){if(context_gap)return -1;assert(size==sizeof(hc_app));hc_app context={HC_MAGIC,77,123,0,now};memcpy(out,&context,size);return 0;}
 assert(!strcmp(path,HC_VIDEO_ACK)&&size==sizeof(ack));memcpy(out,&ack,size);return 0;}
int hc_video_mock_publish(const hc_video *q){sent=*q;return fail_publish;}
int vsh_video_query(vsh_video_state *s){*s=native;return 0;}
unsigned hc_backend_scanout_factor(void){return factor;}
static int configure(unsigned head,uint32_t flags,uint32_t mode,uint32_t format,uint32_t pitch,void *p,int wait)
{assert(!head&&flags==5&&format==5&&pitch==8192&&!p&&wait==1);calls++;native.mode=mode;native.pitch=source_pitch;native.state=1;return 0;}
static void acknowledge(unsigned phase)
{ack=(hc_video_ack){HC_MAGIC,77,sent.operation,phase,0};}
int main(void)
{
 for(unsigned sample=0;sample<3;sample++){
  memset(&ack,0,sizeof(ack));source_pitch=sample==0?4096:sample==1?5120:8192;factor=sample==2?2:1;
  unsigned mode=sample==2?10:11;int before=calls;assert(!hc_video_begin(77));
  assert(!hc_video_submit(configure,5,mode,5,8192));assert(!hc_video_ready()&&calls==before);
  assert(hc_video_submit(configure,5,mode+1,5,8192)==-1); /* No overwrite of pending operation. */
  hc_video_poll();assert(calls==before);
  acknowledge(HC_VIDEO_QUIESCE);ack.owner=99;hc_video_poll();assert(calls==before);
  ack.owner=77;hc_video_poll();assert(calls==before+1&&!hc_video_ready());
  hc_video_poll();assert(!hc_video_ready());native.state=0;hc_video_poll();
  assert(sent.phase==HC_VIDEO_BIND&&sent.pitch==source_pitch&&sent.factor==factor);
  assert(sent.width==(sample==0?1024u:sample==1?1280u:1920u));
  assert(sent.height==(sample==0?768u:sample==1?720u:1080u));
  hc_video_poll();assert(!hc_video_ready());assert(!hc_video_scanout_matches(sent.width,sent.height,source_pitch*factor));
  acknowledge(HC_VIDEO_BIND);hc_video_poll();assert(hc_video_ready()==1);
  assert(hc_video_scanout_matches(sent.width,sent.height,source_pitch*factor));
  assert(!hc_video_scanout_matches(sent.width,sent.height,source_pitch*factor+8));
  assert(!hc_video_scanout_matches(sent.width,sent.height+1,source_pitch*factor));
 }
 assert(!hc_video_begin(77));assert(!hc_video_submit(configure,5,11,5,8192));now+=5000000;hc_video_poll();assert(hc_video_ready()==-1);
 assert(!hc_video_submit(configure,5,11,5,8192));alive=0;assert(hc_video_ready()==1);assert(hc_video_departed()==1&&hc_video_departed()==0);
 int before=calls;assert(!hc_video_submit(configure,5,12,5,8192)&&calls==before+1); /* Recovery falls back to native XMB. */
 assert(hc_video_begin(77)==-1);alive=1;assert(!hc_video_begin(77));fail_publish=1;assert(hc_video_submit(configure,5,11,5,8192)==-1);
 context_gap=1;now+=2999999;assert(hc_video_app_alive());now++;assert(!hc_video_app_alive()); /* Missing publications cannot extend the lease. */
 puts("PASS: no video call before GPU drain, exact ACK ownership, enabled-source rebind, correct XGA/HD pitch geometry, bound-before-ready, timeout and lost-app fallback");
 return 0;
}
