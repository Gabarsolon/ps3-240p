/* SPDX-License-Identifier: MIT. No filesystem, firmware or app dependencies. */
#include "gamma_service.h"
static void copy_curve(float out[3],const float in[3])
{for(unsigned i=0;i<3;i++)out[i]=in[i];}
int gamma_service_restore(gamma_service *s,const gamma_service_ops *ops)
{
 if(!s->active)return 0;
 int rc=ops->apply(s->original);
 if(!rc){s->active=0;s->deadline=s->retry=0;s->current=0;s->restore_all=0;}return rc;
}
static int cancel(gamma_service *s,const gamma_service_ops *ops)
{
 if(s->restore_all)return -1;
 if(!s->deadline)return 0;
 int rc=ops->apply(s->previous);
 if(!rc){s->current=s->previous_value;s->active=s->current!=0;s->deadline=s->retry=0;}
 return rc;
}
int gamma_service_tick(gamma_service *s,const gamma_service_ops *ops,uint64_t now,
                       const gamma_request *q,gamma_reply *reply)
{
 unsigned status=GAMMA_FAILED;int answer=0;
 if(s->active&&s->deadline&&now>=s->deadline&&now>=s->retry){
  int rc=s->restore_all?gamma_service_restore(s,ops):cancel(s,ops);
  answer=1;status=rc?GAMMA_FAILED:GAMMA_EXPIRED;
  if(rc)s->retry=now+2000000;
 }
 if(q&&q->magic==GAMMA_MAGIC&&q->sequence&&q->sequence!=s->sequence){
  s->sequence=q->sequence;answer=1;status=GAMMA_FAILED;
  if(q->action==GAMMA_TEST&&q->value>=50&&q->value<=400){
   int recovery=s->restore_all?gamma_service_restore(s,ops):cancel(s,ops);
   float before[3];
   if(!recovery&&!ops->snapshot(before)){
    copy_curve(s->previous,before);if(!s->active)copy_curve(s->original,before);
    s->previous_value=s->current;s->restore_all=0;
    s->active=1;s->deadline=now+15000000;s->retry=0;s->current=q->value;
    if(!ops->set(q->value))status=GAMMA_OK;
    else if(cancel(s,ops)){s->deadline=now?now:1;s->retry=now+2000000;}
   }else status=GAMMA_UNSUPPORTED;
  }else if(q->action==GAMMA_KEEP){
   if(s->active&&s->deadline&&!s->retry&&!s->restore_all&&now<s->deadline&&s->current==q->value){s->deadline=0;status=GAMMA_OK;}
  }else if(q->action==GAMMA_RESTORE){
   int rc=gamma_service_restore(s,ops);status=rc?GAMMA_FAILED:GAMMA_OK;
   if(rc){s->restore_all=1;s->deadline=now?now:1;s->retry=now+2000000;}
  }else if(q->action==GAMMA_CANCEL){
   int rc=cancel(s,ops);status=rc?GAMMA_FAILED:GAMMA_OK;
   if(rc){s->deadline=now?now:1;s->retry=now+2000000;}
  }
 }
 if(answer){reply->magic=GAMMA_MAGIC;reply->sequence=s->sequence;reply->status=status;reply->value=s->active?s->current:0;}
 return answer;
}
