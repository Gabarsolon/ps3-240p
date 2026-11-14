/* SPDX-License-Identifier: MIT. No filesystem, GPU waits or firmware APIs. */
#include "hd540_service.h"
#define TRANSITION_US UINT64_C(5000000)
#define TEST_US ((uint64_t)HD540_TEST_SECONDS*UINT64_C(1000000))
static uint64_t clock_after(const hd540_ops *o,uint64_t now)
{uint64_t t=o->clock?o->clock(o->user):now;return t>now?t:now;}
int hd540_service_busy(const hd540_service *s)
{return s->recovering||s->state==HD540_WAIT_APP_EXIT||s->state==HD540_LEAVING||s->state==HD540_ENTERING||s->state==HD540_ACTIVE;}
static void done(hd540_service *s)
{
 s->state=HD540_DONE;s->recovering=0;s->deadline=s->retry=0;
 if(s->had_error)s->roundtrip_ok=s->hdmi_progressive_ok=0;
 if(s->kind==HD540_ROUNDTRIP&&!s->had_error&&s->moved)s->roundtrip_ok=1;
 if(s->kind==HD540_HDMI_720_CONTROL&&!s->had_error&&s->moved)s->hdmi_progressive_ok=1;
}
static void recover(hd540_service *s,uint64_t now)
{
 if(s->recovering)return;
 if(s->had_error)s->roundtrip_ok=s->hdmi_progressive_ok=0;
 if(!s->moved){done(s);return;}
 s->recovering=1;s->state=HD540_RESTORE_RAM;s->retry=now;
}
static void halt(hd540_service *s)
{s->state=HD540_RECOVERY_FAILED;s->had_error=s->halted=1;s->deadline=s->retry=0;}
int hd540_service_tick(hd540_service *s,const hd540_ops *o,uint64_t now,
                       const hd540_request *q,hd540_reply *reply)
{
 unsigned before=s->state;int answer=0;
 if(q&&q->magic==HD540_MAGIC&&q->sequence&&q->sequence!=s->seen){
  s->seen=q->sequence;
  if(hd540_service_busy(s)){
   if(q->action==HD540_CANCEL&&!s->halted)recover(s,now);
  }else{
   s->sequence=q->sequence;answer=1;s->had_error=0;
   int test=q->action==HD540_TEST||q->action==HD540_ROUNDTRIP||q->action==HD540_HDMI_CONTROL||q->action==HD540_HDMI_720_CONTROL;
   if((q->action==HD540_INSPECT||test)&&(!test||q->value==HD540_EXPERIMENTAL_VALUE)&&
      (q->action!=HD540_TEST||(s->roundtrip_ok&&s->hdmi_progressive_ok))&&
      ((q->action!=HD540_HDMI_CONTROL&&q->action!=HD540_HDMI_720_CONTROL)||s->roundtrip_ok)&&!o->prepare(o->user)){
    if(!test)s->state=HD540_IDLE;
    else{
     s->kind=q->action;s->moved=s->patched=s->halted=0;s->video_tries=0;
     s->state=HD540_WAIT_APP_EXIT;s->deadline=clock_after(o,now)+15000000;
    }
   }else s->state=HD540_REFUSED;
  }
 }
 if(s->state==HD540_WAIT_APP_EXIT){
  int ready=o->ready?o->ready(o->user):0;
  if(ready==1){
   /* Snapshot AFTER app exit: the XMB has a different pitch. */
   if(o->prepare(o->user))s->state=HD540_REFUSED;
   else{
    s->moved=1;s->state=HD540_LEAVING;
    /* For the progressive control, patch before leaving stock 1080i.
     * The actual transition into 720p must generate the changed AVI packet;
     * a same-mode 720p request may skip native packet generation. */
    int rc=0;
    if(s->kind==HD540_HDMI_720_CONTROL){s->patched=1;rc=o->apply(o->user,s->kind);}
    if(!rc)rc=o->alternate(o->user);now=clock_after(o,now);
    s->deadline=now+TRANSITION_US;
    if(rc){s->had_error=1;recover(s,now);}
   }
  }else if(ready<0||now>=s->deadline)s->state=HD540_REFUSED;
 }else if(s->state==HD540_LEAVING){
  int ready=o->is_alternate(o->user);now=clock_after(o,now);
  if(ready==1){
   int rc=0;
   if(s->kind!=HD540_HDMI_720_CONTROL){
    if(s->kind!=HD540_ROUNDTRIP){s->patched=1;rc=o->apply(o->user,s->kind);}
    if(!rc)rc=o->original(o->user);
   }
   now=clock_after(o,now);
   if(rc){s->had_error=1;recover(s,now);}
   else{s->state=HD540_ENTERING;s->deadline=now+TRANSITION_US;}
  }else if(ready<0||now>=s->deadline){s->had_error=1;recover(s,now);}
 }else if(s->state==HD540_ENTERING){
  int expected=s->patched?(s->kind==HD540_HDMI_720_CONTROL?3:s->kind==HD540_HDMI_CONTROL?2:1):0;
  int ready=o->is_original(o->user,expected);now=clock_after(o,now);
  if(ready==1){
   if(!s->patched)done(s);
   else{s->state=HD540_ACTIVE;s->deadline=now+TEST_US;}
  }else if(ready<0||now>=s->deadline){s->had_error=1;recover(s,now);}
 }else if(s->state==HD540_ACTIVE&&now>=s->deadline)recover(s,now);
 if(s->recovering&&!s->halted&&now>=s->retry){
  if(s->state==HD540_RESTORE_RAM){
   /* RAM retries issue no video commands until restoration succeeds. */
   int rc=o->restore(o->user);now=clock_after(o,now);
   if(rc){s->had_error=1;s->retry=now+2000000;}
   else{
    int was_patched=s->patched;s->patched=0;
    if(was_patched){
     /* A same-mode return restored the AV cache in the first candidate
      * test but left OBS black. Restore RAM first, then leave 1080i and
      * select stock 720p before returning. One cycle, never a retry loop. */
     s->video_tries=2;
     int rc=o->alternate(o->user);if(rc)s->had_error=1;
     s->state=HD540_RESTORE_LEAVING;
     s->deadline=clock_after(o,now)+(rc?0:TRANSITION_US);
    }else{
     s->video_tries=1;(void)o->original(o->user);
     s->state=HD540_RESTORE_ENTERING;s->deadline=clock_after(o,now)+TRANSITION_US;
    }
   }
  }else if(s->state==HD540_RESTORE_ENTERING){
   int ready=o->is_original(o->user,0);now=clock_after(o,now);
   if(ready==1)done(s);
   else if(ready<0||now>=s->deadline){
    if(s->video_tries>=2)halt(s);
    else{
     s->video_tries=2;(void)o->alternate(o->user);
     s->state=HD540_RESTORE_LEAVING;s->deadline=clock_after(o,now)+TRANSITION_US;
    }
   }
  }else if(s->state==HD540_RESTORE_LEAVING){
   int ready=o->is_alternate(o->user);now=clock_after(o,now);
   if(ready==1||ready<0||now>=s->deadline){
    /* A failed/timed-out alternate still gets the final original request. */
    if(ready!=1)s->had_error=1;
    (void)o->original(o->user);s->state=HD540_RESTORE_ENTERING;s->deadline=clock_after(o,now)+TRANSITION_US;
   }
  }
 }
 if(before!=s->state)answer=1;
 if(answer&&reply){reply->magic=HD540_MAGIC;reply->sequence=s->sequence;reply->status=s->state;reply->detail=(unsigned)s->had_error;}
 return answer;
}
