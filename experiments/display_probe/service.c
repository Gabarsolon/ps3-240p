/* SPDX-License-Identifier: MIT. Recovery runs on the console, independent of PC. */
#include "service.h"
#define SECOND UINT64_C(1000000)
int dp_busy(const dp_service *s){return s->owned||s->state==DP_RESTORE||s->state==DP_WAIT_APP;}
static uint64_t fresh(const dp_ops *o,uint64_t now){return o->clock?o->clock(o->user):now;}
static void recover(dp_service *s,const dp_ops *o,uint64_t now,unsigned detail)
{
 if(detail)s->detail=detail;
 s->state=DP_RESTORE;s->retries=0;
 s->restore_error=o->restore(o->user)!=0;
 s->deadline=fresh(o,now)+5*SECOND;s->retry=fresh(o,now)+SECOND;
}
static unsigned next_kind(const dp_service *s)
{
 if(!(s->controls&1))return DP_STOCK720;
 if(!(s->controls&(1u<<s->profile)))return DP_CAPS;
 return DP_WXGA;
}
static void begin(dp_service *s,const dp_ops *o,uint64_t now,unsigned kind)
{
 s->kind=kind;
 int ready=o->prepare(o->user,kind==DP_STOCK720?1:s->profile);
 if(ready>0&&s->state==DP_WAIT_APP)return;
 if(ready){s->state=DP_REFUSED;s->detail=DP_PREPARE_ERROR;s->automatic=0;return;}
 s->owned=1;
 int rc=kind==DP_CAPS?o->caps(o->user):o->stock(o->user);
 if(rc)recover(s,o,now,DP_ENTER_ERROR);
 else{s->state=kind==DP_WXGA?DP_WAIT_SOURCE:DP_WAIT_TARGET;s->deadline=fresh(o,now)+5*SECOND;}
}
int dp_tick(dp_service *s,const dp_ops *o,uint64_t now,const dp_request *q,dp_reply *r)
{
 unsigned previous=s->state,old_detail=s->detail;uint32_t old_sequence=s->sequence;
 if(q&&q->magic==DPROBE_MAGIC&&q->sequence&&q->sequence!=s->seen){
  s->seen=q->sequence;
  if(q->action==DP_CANCEL&&dp_busy(s)&&!s->halted){
   s->sequence=q->sequence;s->automatic=0;
   if(s->state==DP_WAIT_APP){s->state=DP_DONE;s->detail=DP_CANCELLED;}
   else recover(s,o,now,DP_CANCELLED);
  }else if(!dp_busy(s)&&!s->halted){
   s->sequence=q->sequence;s->detail=0;s->kind=q->action;s->profile=q->value;s->automatic=0;
   if(q->value<1||q->value>3||((q->action==DP_INSPECT||q->action==DP_STOCK720)&&q->value!=1)||
     q->action<DP_INSPECT||q->action>DP_AUTO||q->action==DP_CANCEL){s->state=DP_REFUSED;s->detail=DP_BAD_REQUEST;}
   else if(q->action==DP_AUTO){s->automatic=1;s->state=DP_WAIT_APP;s->app_settle=0;s->deadline=fresh(o,now)+20*SECOND;}
   else if(q->action==DP_WXGA&&(s->controls&(1u|(1u<<q->value)))!=(1u|(1u<<q->value))){s->state=DP_REFUSED;s->detail=DP_PREREQUISITE;}
   else if(q->action==DP_INSPECT){s->state=o->prepare(o->user,q->value)?DP_REFUSED:DP_DONE;if(s->state==DP_REFUSED)s->detail=DP_PREPARE_ERROR;}
   else begin(s,o,now,q->action);
  }
 }
 now=fresh(o,now);
 if(s->state==DP_WAIT_APP){
  if(now>=s->deadline){s->state=DP_REFUSED;s->detail=DP_TIMEOUT;s->automatic=0;}
  else if(o->app_active&&o->app_active(o->user))s->app_settle=0;
  else if(!s->app_settle)s->app_settle=now+3*SECOND;
  else if(now>=s->app_settle)begin(s,o,now,next_kind(s));
 }else if(s->state==DP_WAIT_SOURCE){
  int rc=o->at_stock(o->user);
  if(rc<0)recover(s,o,now,DP_ENTER_ERROR);
  else if(rc){
   if(o->candidate(o->user))recover(s,o,now,DP_ENTER_ERROR);
   else{s->state=DP_WAIT_TARGET;s->deadline=fresh(o,now)+5*SECOND;}
  }else if(now>=s->deadline)recover(s,o,now,DP_TIMEOUT);
 }else if(s->state==DP_WAIT_TARGET){
  int rc=o->at_target(o->user,s->kind);
  if(rc<0)recover(s,o,now,DP_ENTER_ERROR);
  else if(rc){s->state=DP_ACTIVE;s->deadline=fresh(o,now)+15*SECOND;}
  else if(now>=s->deadline)recover(s,o,now,DP_TIMEOUT);
 }else if(s->state==DP_ACTIVE&&now>=s->deadline)recover(s,o,now,0);
 else if(s->state==DP_RESTORE){
  int rc=o->at_original(o->user);
  /* Independent full-state readback is stronger than a transient busy return
   * from a redundant restore submission during an already accepted switch. */
  if(rc==1){
   s->state=DP_DONE;s->owned=0;
   if(!s->detail){if(s->kind==DP_STOCK720)s->controls|=1;if(s->kind==DP_CAPS)s->controls|=1u<<s->profile;}
   else s->controls=0;
   if(s->automatic&&!s->detail&&s->kind!=DP_WXGA)begin(s,o,now,next_kind(s));
   else s->automatic=0;
  }else if(now>=s->deadline){s->state=DP_RECOVERY_FAILED;s->detail=DP_RESTORE_ERROR;s->halted=1;s->controls=0;}
  else if(now>=s->retry&&s->retries<2){
   ++s->retries;s->restore_error=o->restore(o->user)!=0;s->retry=fresh(o,now)+SECOND;
  }
 }
 if(r)*r=(dp_reply){DPROBE_MAGIC,s->sequence,s->state,s->detail};
 return previous!=s->state||old_detail!=s->detail||old_sequence!=s->sequence;
}
