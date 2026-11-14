#include "../experiments/display_probe/service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t now;
static int prepare_error,enter_error,caps_error,restore_error,at_source,target,original,restores,candidates,stock_calls;
static uint64_t call_delay;
static int prepare(void *u){(void)u;return prepare_error;}
static int stock(void *u){(void)u;stock_calls++;now+=call_delay;original=0;return enter_error;}
static int caps(void *u){(void)u;now+=call_delay;original=0;return caps_error;}
static int candidate(void *u){(void)u;candidates++;now+=call_delay;return enter_error;}
static int source_ok(void *u){(void)u;return at_source;}
static int target_ok(void *u,unsigned kind){(void)u;(void)kind;return target;}
static int restore(void *u){(void)u;restores++;now+=call_delay;return restore_error;}
static int original_ok(void *u){(void)u;return original;}
static uint64_t clock_now(void *u){(void)u;return now;}
static const dp_ops ops={prepare,stock,caps,candidate,source_ok,target_ok,restore,original_ok,clock_now,0};
static dp_service s;
static dp_reply r;
static uint32_t sequence;
static void tick(void){dp_tick(&s,&ops,now,0,&r);}
static void request(unsigned kind){dp_request q={DPROBE_MAGIC,++sequence,kind,1};dp_tick(&s,&ops,now,&q,&r);}
static void reset(void)
{
 memset(&s,0,sizeof(s));now=sequence=0;prepare_error=enter_error=caps_error=restore_error=0;
 at_source=target=0;original=1;restores=candidates=stock_calls=0;call_delay=0;
}
static void finish(void)
{
 target=1;tick();assert(s.state==DP_ACTIVE);
 uint64_t deadline=s.deadline;now=deadline-1;tick();assert(s.state==DP_ACTIVE&&!restores);
 now=deadline;tick();assert(s.state==DP_RESTORE&&restores==1);
 original=1;tick();assert(s.state==DP_DONE&&!s.owned);
}
int main(void)
{
 reset();request(DP_WXGA);assert(s.state==DP_REFUSED&&s.detail==DP_PREREQUISITE&&!stock_calls);
 reset();prepare_error=1;request(DP_STOCK720);assert(s.state==DP_REFUSED&&!restores&&!stock_calls);
 reset();request(DP_INSPECT);assert(s.state==DP_DONE&&!s.owned&&!stock_calls);
 reset();request(DP_STOCK720);finish();assert(s.controls==1);
 restores=0;request(DP_CAPS);finish();assert(s.controls==3);
 restores=0;target=0;request(DP_WXGA);assert(s.state==DP_WAIT_SOURCE&&!candidates);
 at_source=1;tick();assert(s.state==DP_WAIT_TARGET&&candidates==1);finish();assert(!s.detail);
 /* Failure after partially changing capabilities still invokes restoration. */
 reset();caps_error=1;request(DP_CAPS);assert(s.state==DP_RESTORE&&restores==1);
 original=1;tick();assert(s.state==DP_DONE&&s.detail==DP_ENTER_ERROR&&!s.controls);
 /* Caller clock is sampled after a blocking native call. */
 reset();call_delay=3000000;request(DP_STOCK720);assert(s.deadline==8000000);
 now=s.deadline;tick();assert(s.state==DP_RESTORE&&s.deadline==16000000);
 original=1;tick();assert(s.state==DP_DONE&&s.detail==DP_TIMEOUT);
 /* Duplicate inbox cannot restart the hold. */
 reset();request(DP_STOCK720);target=1;tick();uint64_t deadline=s.deadline;
 dp_request duplicate={DPROBE_MAGIC,sequence,DP_STOCK720,1};now+=5000000;
 dp_tick(&s,&ops,now,&duplicate,&r);assert(s.deadline==deadline&&stock_calls==1);
 request(DP_CANCEL);assert(s.state==DP_RESTORE&&s.detail==DP_CANCELLED);original=1;tick();assert(s.state==DP_DONE);
 /* A busy retry cannot veto an independently verified exact restoration. */
 reset();request(DP_STOCK720);target=1;tick();now=s.deadline;tick();
 restore_error=1;now=s.retry;tick();assert(s.restore_error&&s.state==DP_RESTORE);
 original=1;tick();assert(s.state==DP_DONE&&!s.owned&&!s.detail);
 /* Bounded retries latch uncertain recovery and forbid new experiments. */
 reset();request(DP_STOCK720);target=1;tick();now=s.deadline;tick();
 restore_error=1;original=0;now=s.retry;tick();now=s.retry;tick();
 assert(restores==3);now=s.deadline;tick();assert(s.halted&&s.state==DP_RECOVERY_FAILED);
 request(DP_STOCK720);assert(s.halted&&stock_calls==1&&s.state==DP_RECOVERY_FAILED);
 puts("PASS: display-probe controls, full hold, cancellation, partial-write recovery, fresh-clock deadlines, duplicates and recovery latch");
 return 0;
}
