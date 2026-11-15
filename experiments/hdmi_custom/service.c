/* SPDX-License-Identifier: MIT. Confirmed settings never survive a reboot. */
#include "service.h"
#define SECOND UINT64_C(1000000)
int hc_trial_valid(const hc_test_lease *lease,unsigned mode,uint64_t now)
{return lease&&lease->magic==HC_MAGIC&&mode>=1&&mode<=HC_MODE_COUNT&&lease->mode==mode&&
 lease->expires>now&&lease->expires-now<=120*SECOND;}
static uint64_t fresh(const hc_ops *o,uint64_t now){return o->clock?o->clock(o->user):now;}
int hc_busy(const hc_service *s){return s->engine_owned||s->state==HC_WAIT_APP||s->state==HC_SUSPENDED;}
static void restore(hc_service *s,const hc_ops *o,unsigned why,int resume)
{
 o->close_prompt(o->user);s->remaining=0;s->detail=why;s->resume=resume;
 if(s->engine_owned){o->cancel(o->user);s->state=HC_RESTORING;}
 else{s->state=resume?HC_SUSPENDED:HC_DONE;if(!resume)s->mode=0;}
}
int hc_tick(hc_service *s,const hc_ops *o,uint64_t now,const hc_request *q,hc_reply *r)
{
 hc_reply before={HC_MAGIC,s->sequence,s->state,s->detail,s->mode,s->remaining};
 if(q&&q->magic==HC_MAGIC&&q->sequence&&q->sequence!=s->seen){
  s->seen=q->sequence;
  if(q->action==HC_STOP&&q->mode==0&&s->state!=HC_FAILED){s->sequence=q->sequence;s->keep_until=0;restore(s,o,HC_CANCELLED,0);}
  else if(q->action==HC_KEEP&&s->state==HC_PREVIEW&&q->mode==s->sequence&&fresh(o,now)<s->deadline){
   if(o->keep(o->user))restore(s,o,HC_BACKEND,0);
   else{s->sequence=q->sequence;o->close_prompt(o->user);s->state=HC_KEPT;s->remaining=0;s->unhealthy=0;if(s->trial)s->keep_until=fresh(o,now)+120*SECOND;}
  }
  else if((q->action==HC_SELECT||q->action==HC_TRIAL||q->action==HC_APP_SELECT)&&q->mode>=1&&q->mode<=HC_MODE_COUNT&&
    !s->engine_owned&&s->state!=HC_FAILED){
   s->sequence=q->sequence;s->mode=q->mode;s->detail=0;s->resume=0;s->automatic_keep=0;
   s->trial=q->action==HC_TRIAL;s->keep_until=0;
   s->state=HC_WAIT_APP;s->settle=0;s->deadline=fresh(o,now)+25*SECOND;
  }else if(!hc_busy(s)&&s->state!=HC_FAILED){s->sequence=q->sequence;s->state=HC_REFUSED;s->detail=HC_BAD_REQUEST;}
 }
 now=fresh(o,now);
 if(s->keep_until&&now>=s->keep_until&&s->state!=HC_FAILED){s->keep_until=0;restore(s,o,HC_EXPIRED,0);}
 if(s->state==HC_WAIT_APP||s->state==HC_SUSPENDED){
  int foreground=o->foreground(o->user);
  if(foreground<0){s->state=HC_REFUSED;s->detail=HC_BACKEND;s->resume=0;}
  else if(foreground)s->settle=0;
  else if(!s->settle)s->settle=now+3*SECOND;
  else if(now>=s->settle){
   if(o->start(o->user,s->mode)){s->state=HC_REFUSED;s->detail=HC_BACKEND;s->resume=0;}
   else{s->engine_owned=1;s->automatic_keep=s->resume;s->resume=0;s->state=HC_PREPARING;}
  }
  if(s->state==HC_WAIT_APP&&now>=s->deadline){s->state=HC_REFUSED;s->detail=HC_EXPIRED;}
 }
 if(s->engine_owned){
  int engine=o->tick(o->user);now=fresh(o,now);
  if(engine==HC_ENGINE_FAILED){o->close_prompt(o->user);s->state=HC_FAILED;s->detail=HC_BACKEND;s->resume=0;}
  else if(engine==HC_ENGINE_DONE||engine==HC_ENGINE_ERROR){
   o->close_prompt(o->user);s->engine_owned=0;s->remaining=0;s->settle=0;
   if(engine==HC_ENGINE_ERROR){s->state=HC_REFUSED;s->detail=HC_BACKEND;s->resume=0;}
   else{if(s->state==HC_PREVIEW&&!s->detail)s->detail=HC_EXPIRED;
    s->state=s->resume?HC_SUSPENDED:HC_DONE;if(!s->resume)s->mode=0;}
  }else if(s->state!=HC_RESTORING&&s->state!=HC_FAILED){
   int foreground=o->foreground(o->user);
   if(foreground)restore(s,o,foreground<0?HC_BACKEND:HC_OK,s->state==HC_KEPT&&foreground>0);
   else if(s->state==HC_PREPARING&&engine==HC_ENGINE_PREVIEW){
    s->deadline=now+15*SECOND;s->state=HC_PREVIEW;s->remaining=15;
    if(s->automatic_keep){if(o->keep(o->user))restore(s,o,HC_BACKEND,0);else{s->state=HC_KEPT;s->remaining=0;s->unhealthy=0;}}
   }else if(s->state==HC_PREVIEW){
    if(now>=s->deadline)restore(s,o,HC_EXPIRED,0);
    else{
     s->remaining=(unsigned)((s->deadline-now+SECOND-1)/SECOND);
     int choice=o->prompt(o->user,s->mode,s->remaining);now=fresh(o,now);
     if(choice<0)restore(s,o,HC_INPUT,0);
     else if(choice==2)restore(s,o,HC_CANCELLED,0);
     else if(choice==1){
      if(now>=s->deadline)restore(s,o,HC_EXPIRED,0);
      else if(o->keep(o->user))restore(s,o,HC_BACKEND,0);
      else{o->close_prompt(o->user);s->state=HC_KEPT;s->remaining=0;s->unhealthy=0;if(s->trial)s->keep_until=now+120*SECOND;}
     }
    }
   }else if(s->state==HC_KEPT){
    int health=o->health(o->user);
    if(health==1)s->unhealthy=0;
    else if(health<0)restore(s,o,HC_CHANGED,0);
    else if(!s->unhealthy)s->unhealthy=now+3*SECOND;
    else if(now>=s->unhealthy)restore(s,o,HC_CHANGED,0);
   }
  }
 }
 hc_reply reply={HC_MAGIC,s->sequence,s->state,s->detail,s->mode,s->remaining};
 if(r)*r=reply;
 return before.sequence!=reply.sequence||before.state!=reply.state||before.detail!=reply.detail||before.mode!=reply.mode||before.remaining!=reply.remaining;
}
