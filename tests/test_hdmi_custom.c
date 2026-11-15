/* Behavioral proof for session confirmation, expiry and exclusive recovery. */
#include "../experiments/hdmi_custom/service.h"
#include "../experiments/hdmi_custom/confirm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static hc_service s;
static uint64_t now,delay;
static int foreground,engine,health,choice,start_error,keep_error,starts,keeps,cancels,closes;
static unsigned seq;
static int fore(void *u){(void)u;return foreground;}
static int start(void *u,unsigned mode){(void)u;assert(mode>=1&&mode<=5);starts++;engine=HC_ENGINE_BUSY;return start_error;}
static int tick_engine(void *u){(void)u;return engine;}
static int keep(void *u){(void)u;keeps++;if(!keep_error)engine=HC_ENGINE_HELD;return keep_error;}
static void cancel(void *u){(void)u;cancels++;engine=HC_ENGINE_BUSY;}
static int check(void *u){(void)u;return health;}
static int prompt(void *u,unsigned mode,unsigned seconds){(void)u;assert(mode>=1&&mode<=5&&seconds&&seconds<=15);now+=delay;return choice;}
static void close_prompt(void *u){(void)u;closes++;}
static uint64_t clock_now(void *u){(void)u;return now;}
static const hc_ops ops={fore,start,tick_engine,keep,cancel,check,prompt,close_prompt,clock_now,0};
static void tick(void){hc_reply r;hc_tick(&s,&ops,now,NULL,&r);}
static void request(unsigned action,unsigned mode){hc_request q={HC_MAGIC,++seq,action,mode};hc_tick(&s,&ops,now,&q,NULL);}
static void reset(void)
{memset(&s,0,sizeof(s));now=1;delay=0;foreground=choice=start_error=keep_error=starts=keeps=cancels=closes=0;health=1;engine=HC_ENGINE_BUSY;}
static void preview(unsigned mode)
{request(HC_SELECT,mode);assert(s.state==HC_WAIT_APP&&!starts);now+=3000000;tick();assert(s.state==HC_PREPARING&&starts==1);engine=HC_ENGINE_PREVIEW;tick();assert(s.state==HC_PREVIEW&&s.remaining==15);}
int main(void)
{
 hc_test_lease lease={HC_MAGIC,HC_XGA,120000001};
 assert(hc_trial_valid(&lease,HC_XGA,1));assert(!hc_trial_valid(&lease,HC_540,1));
 assert(!hc_trial_valid(&lease,HC_XGA,0));assert(!hc_trial_valid(&lease,HC_XGA,lease.expires));
 lease.magic=0;assert(!hc_trial_valid(&lease,HC_XGA,1));
 for(unsigned mode=1;mode<=5;mode++){
  reset();preview(mode);uint64_t end=s.deadline;now=end-1;tick();assert(s.state==HC_PREVIEW&&!cancels);
  now=end;tick();assert(s.state==HC_RESTORING&&cancels==1&&!keeps);
  request(HC_SELECT,mode);assert(s.state==HC_RESTORING&&starts==1); /* Cannot steal owned RAM. */
  engine=HC_ENGINE_DONE;tick();assert(s.state==HC_DONE&&!s.engine_owned&&!s.mode);
  reset();preview(mode);choice=1;tick();assert(s.state==HC_KEPT&&keeps==1&&!s.remaining);
  now+=120000000;tick();assert(s.state==HC_KEPT&&!cancels); /* Survives old preview deadline. */
  foreground=1;tick();assert(s.state==HC_RESTORING&&cancels==1&&s.resume);
  engine=HC_ENGINE_DONE;tick();assert(s.state==HC_SUSPENDED&&s.mode==mode);
  now+=30000000;tick();assert(starts==1);foreground=0;tick();now+=3000000;tick();assert(s.state==HC_PREPARING&&starts==2);
  engine=HC_ENGINE_PREVIEW;tick();assert(s.state==HC_KEPT&&keeps==2); /* Same confirmed mode resumes. */
  request(HC_STOP,0);assert(s.state==HC_RESTORING&&!s.resume);engine=HC_ENGINE_DONE;tick();assert(s.state==HC_DONE&&!s.mode);
 }
 reset();preview(1);choice=2;tick();assert(s.state==HC_RESTORING&&s.detail==HC_CANCELLED&&!keeps);
 reset();preview(1);choice=-1;tick();assert(s.state==HC_RESTORING&&s.detail==HC_INPUT);
 reset();preview(1);choice=1;now=s.deadline-10;delay=20;tick();assert(s.state==HC_RESTORING&&s.detail==HC_EXPIRED&&!keeps);
 reset();preview(1);choice=1;keep_error=1;tick();assert(s.state==HC_RESTORING&&cancels==1);
 reset();preview(1);engine=HC_ENGINE_FAILED;tick();assert(s.state==HC_FAILED&&s.engine_owned);
 request(HC_SELECT,2);assert(s.state==HC_FAILED&&starts==1);
 reset();preview(1);choice=1;tick();choice=0;health=0;tick();now+=2999999;tick();assert(s.state==HC_KEPT);
 now++;tick();assert(s.state==HC_RESTORING&&s.detail==HC_CHANGED);
 reset();foreground=1;request(HC_SELECT,3);now+=25000000;tick();assert(s.state==HC_REFUSED&&!starts);
 reset();request(HC_SELECT,0);assert(s.state==HC_REFUSED&&!starts);
 reset();request(HC_SELECT,6);assert(s.state==HC_REFUSED&&!starts);
 reset();foreground=-1;request(HC_SELECT,1);assert(s.state==HC_REFUSED&&!starts);
 reset();preview(1);foreground=1;tick();assert(s.state==HC_RESTORING&&!s.resume); /* Unconfirmed never auto-resumes. */
 reset();foreground=1;request(HC_SELECT,1);request(HC_STOP,0);assert(s.state==HC_DONE&&!starts);
 reset();request(HC_TRIAL,1);now+=3000000;tick();engine=HC_ENGINE_PREVIEW;tick();choice=1;tick();assert(s.state==HC_KEPT&&s.keep_until);
 now=s.keep_until;tick();assert(s.state==HC_RESTORING&&s.detail==HC_EXPIRED&&!s.resume);
 reset();request(HC_APP_SELECT,1);now+=3000000;tick();engine=HC_ENGINE_PREVIEW;tick();
 hc_request keep_request={HC_MAGIC,++seq,HC_KEEP,s.sequence};hc_tick(&s,&ops,now,&keep_request,NULL);
 assert(s.state==HC_KEPT&&s.sequence==keep_request.sequence&&keeps==1);
 reset();preview(1);keep_request=(hc_request){HC_MAGIC,++seq,HC_KEEP,s.sequence+99};hc_tick(&s,&ops,now,&keep_request,NULL);assert(s.state==HC_PREVIEW&&!keeps);
 hc_confirm pad={0};assert(!hc_confirm_sample(&pad,0x4000));assert(!hc_confirm_sample(&pad,0));
 assert(!hc_confirm_sample(&pad,0x4000));assert(hc_confirm_sample(&pad,0)==1); /* New full press/release only. */
 assert(!hc_confirm_sample(&pad,0x6000));assert(hc_confirm_sample(&pad,0)==2); /* Cancel wins simultaneous input. */
 puts("PASS: all five modes, full preview expiry, released confirmation, keep beyond deadline, stop, suspend/resume, refusal guards and failed-recovery latch");
 return 0;
}
