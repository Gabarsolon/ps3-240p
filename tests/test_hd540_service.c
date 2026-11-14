#include "../plugin/hd540_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static hd540_service s;
static int display,patched,prepares,alt_calls,orig_calls,restores,applies,app_gone,
 fail_prepare,fail_apply,fail_restore,fail_alt,fail_orig,stall_alt,stall_orig,cache_error;
static uint64_t mock_time,query_delay,submit_delay;
static int capture_black;
static int prepare(void *u){(void)u;prepares++;return fail_prepare;}
static int alt(void *u){(void)u;if(s.kind==HD540_HDMI_720_CONTROL&&!s.recovering)assert(patched==3);alt_calls++;if(fail_alt)return -1;if(!stall_alt){display=2;if(!patched)capture_black=0;}return 0;}
static int at_alt(void *u){(void)u;mock_time+=query_delay;return display==2;}
static int apply(void *u,unsigned kind){(void)u;applies++;patched=kind==HD540_HDMI_720_CONTROL?3:kind==HD540_HDMI_CONTROL?2:1;capture_black=kind==HD540_TEST;return fail_apply;}
static int original(void *u){(void)u;orig_calls++;mock_time+=submit_delay;if(fail_orig)return -1;if(!stall_orig)display=1;return 0;}
static int at_original(void *u,int candidate){(void)u;if(cache_error)return -1;return display==(candidate==3?2:1)&&patched==candidate;}
static int restore(void *u){(void)u;restores++;if(fail_restore)return -1;patched=0;return 0;}
static int ready(void *u){(void)u;return app_gone;}
static uint64_t clock_now(void *u){(void)u;return mock_time;}
static const hd540_ops ops={prepare,alt,at_alt,apply,original,at_original,restore,NULL,ready,clock_now};
static void reset(void)
{
 memset(&s,0,sizeof(s));display=1;
 patched=prepares=alt_calls=orig_calls=restores=applies=app_gone=fail_prepare=fail_apply=fail_restore=fail_alt=fail_orig=stall_alt=stall_orig=cache_error=0;
 mock_time=query_delay=submit_delay=0;
 capture_black=0;
}
static void tick(uint64_t t,const hd540_request *q)
{hd540_reply reply;mock_time=t;(void)hd540_service_tick(&s,&ops,t,q,&reply);}
static hd540_request request(unsigned seq,unsigned action)
{hd540_request q={HD540_MAGIC,seq,action,HD540_EXPERIMENTAL_VALUE};return q;}
static void start(unsigned kind)
{
 hd540_request q=request(s.seen+1,kind);tick(100,&q);
 assert(s.state==HD540_WAIT_APP_EXIT&&!alt_calls&&!applies);
 tick(200,&q);assert(s.state==HD540_WAIT_APP_EXIT&&!alt_calls);
 app_gone=1;tick(300,&q);assert(s.state==HD540_LEAVING&&alt_calls==1&&prepares==2);
}
static void active(void)
{
 s.roundtrip_ok=s.hdmi_progressive_ok=1;start(HD540_TEST);tick(400,NULL);assert(s.state==HD540_ENTERING&&applies==1);
 tick(500,NULL);assert(s.state==HD540_ACTIVE&&s.deadline==15000500);
}
int main(void)
{
 reset();hd540_request q=request(1,HD540_INSPECT);tick(1,&q);assert(s.state==HD540_IDLE&&prepares==1&&!alt_calls);
 tick(2,&q);assert(prepares==1);
 reset();q=request(1,HD540_TEST);tick(1,&q);assert(s.state==HD540_REFUSED&&!prepares&&!alt_calls);
 reset();s.roundtrip_ok=1;q=request(1,HD540_TEST);tick(1,&q);
 assert(s.state==HD540_REFUSED&&!prepares&&!applies); /* Progressive HDMI control prerequisite. */
 reset();q=request(1,HD540_HDMI_CONTROL);tick(1,&q);assert(s.state==HD540_REFUSED&&!prepares);
 reset();q=request(1,HD540_HDMI_720_CONTROL);tick(1,&q);assert(s.state==HD540_REFUSED&&!prepares);
 reset();s.roundtrip_ok=1;start(HD540_HDMI_720_CONTROL);
 assert(applies==1&&patched==3&&display==2&&!orig_calls);
 tick(400,NULL);assert(s.state==HD540_ENTERING&&applies==1&&!orig_calls);
 tick(500,NULL);assert(s.state==HD540_ACTIVE&&display==2&&s.deadline==15000500);
 tick(s.deadline-1,NULL);assert(s.state==HD540_ACTIVE&&patched==3);
 tick(s.deadline,NULL);assert(s.state==HD540_RESTORE_LEAVING&&!patched&&!orig_calls);
 tick(15000501,NULL);tick(15000502,NULL);
 assert(s.state==HD540_DONE&&display==1&&orig_calls==1&&s.roundtrip_ok&&s.hdmi_progressive_ok&&!s.had_error);
 /* Successful progressive control and recovery unlock the candidate. */
 q=request(s.seen+1,HD540_TEST);tick(16000000,&q);
 assert(s.state==HD540_LEAVING&&s.kind==HD540_TEST&&patched==0);
 tick(16000100,NULL);tick(16000200,NULL);
 assert(s.state==HD540_ACTIVE&&patched==1);
 /* A failed early AVI write must restore before any video request. */
 reset();s.roundtrip_ok=1;fail_apply=fail_restore=1;q=request(1,HD540_HDMI_720_CONTROL);
 tick(100,&q);app_gone=1;tick(300,NULL);
 assert(s.state==HD540_RESTORE_RAM&&patched==3&&!alt_calls&&!orig_calls);
 fail_restore=0;tick(s.retry,NULL);tick(s.deadline,NULL);tick(s.deadline,NULL);
 assert(s.state==HD540_DONE&&!patched&&display==1&&s.had_error&&!s.roundtrip_ok);
 /* Missing changed-packet readiness times out and returns to stock. */
 reset();s.roundtrip_ok=1;start(HD540_HDMI_720_CONTROL);tick(400,NULL);cache_error=1;
 tick(s.deadline,NULL);assert(s.state==HD540_RESTORE_LEAVING&&!patched);
 cache_error=0;tick(s.deadline,NULL);tick(s.deadline,NULL);
 assert(s.state==HD540_DONE&&display==1&&s.had_error);
 reset();s.roundtrip_ok=1;start(HD540_HDMI_CONTROL);tick(400,NULL);tick(500,NULL);
 assert(s.state==HD540_ACTIVE&&patched==2&&!capture_black&&s.deadline==15000500);
 tick(s.deadline,NULL);assert(s.state==HD540_RESTORE_LEAVING&&!patched);
 tick(15000501,NULL);tick(15000502,NULL);
 assert(s.state==HD540_DONE&&!s.hdmi_progressive_ok&&s.roundtrip_ok&&!s.had_error);
 q=request(s.seen+1,HD540_TEST);tick(16000000,&q);
 assert(s.state==HD540_REFUSED&&!s.hdmi_progressive_ok); /* Interlaced control is insufficient. */
 reset();start(HD540_ROUNDTRIP);tick(400,NULL);tick(500,NULL);
 assert(s.state==HD540_DONE&&s.roundtrip_ok&&!applies&&!restores&&!patched&&display==1);
 /* Dev4 live regression: slow cache/diagnostic work before the return
  * must not consume the next transition's five-second budget. */
 reset();start(HD540_ROUNDTRIP);query_delay=2000000;submit_delay=500000;stall_orig=1;
 tick(400,NULL);assert(s.state==HD540_ENTERING&&s.deadline==7500400&&!s.had_error);
 tick(6000400,NULL);assert(s.state==HD540_ENTERING&&!s.had_error);
 display=1;tick(6500400,NULL);assert(s.state==HD540_DONE&&s.roundtrip_ok&&!applies&&!restores);
 reset();start(HD540_ROUNDTRIP);query_delay=2000000;submit_delay=500000;stall_orig=1;
 tick(400,NULL);tick(s.deadline,NULL);
 assert(s.state==HD540_RESTORE_ENTERING&&s.had_error&&!s.roundtrip_ok); /* A real timeout still fails. */
 reset();start(HD540_ROUNDTRIP);fail_prepare=0; /* Duplicate request cannot re-arm. */
 q=request(2,HD540_TEST);tick(350,&q);assert(prepares==2&&s.kind==HD540_ROUNDTRIP);
 tick(400,NULL);tick(500,NULL);assert(s.state==HD540_DONE&&!applies);
 reset();s.roundtrip_ok=s.hdmi_progressive_ok=1;q=request(1,HD540_TEST);tick(100,&q);tick(15000100,NULL);
 assert(s.state==HD540_REFUSED&&!alt_calls&&!applies); /* App never exited. */
 reset();s.roundtrip_ok=s.hdmi_progressive_ok=1;q=request(1,HD540_TEST);tick(100,&q);fail_prepare=1;app_gone=1;tick(300,NULL);
 assert(s.state==HD540_REFUSED&&!alt_calls&&!applies); /* XMB differs from preflight. */
 reset();active();uint64_t end=s.deadline;q=request(2,HD540_TEST);tick(600,&q);assert(s.deadline==end);
 assert(capture_black); /* Cached candidate readiness cannot establish a picture. */
 tick(end-1,NULL);assert(s.state==HD540_ACTIVE&&patched&&!restores);
 tick(end,NULL);assert(s.state==HD540_RESTORE_LEAVING&&!patched&&orig_calls==1&&alt_calls==2&&!capture_black);
 tick(end+1,NULL);assert(s.state==HD540_RESTORE_ENTERING&&orig_calls==2);
 tick(end+2,NULL);assert(s.state==HD540_DONE&&!s.recovering&&!capture_black);
 reset();active();q=request(2,HD540_CANCEL);tick(700,&q);assert(s.state==HD540_RESTORE_LEAVING&&!patched);
 tick(701,&q);assert(s.state==HD540_RESTORE_ENTERING);tick(702,NULL);assert(s.state==HD540_DONE);
 reset();s.roundtrip_ok=s.hdmi_progressive_ok=1;stall_alt=1;start(HD540_TEST);tick(5000300,NULL);
 assert(s.state==HD540_RESTORE_ENTERING&&!applies&&orig_calls==1);tick(5000301,NULL);
 assert(s.state==HD540_DONE&&!s.roundtrip_ok); /* Failed alternate still returns directly. */
 reset();active();fail_restore=1;tick(s.deadline,NULL);
 assert(s.state==HD540_RESTORE_RAM&&patched&&orig_calls==1);
 unsigned calls=(unsigned)orig_calls;uint64_t retry=s.retry;tick(retry-1,NULL);assert((unsigned)orig_calls==calls);
 fail_restore=0;tick(retry,NULL);tick(retry+1,NULL);tick(retry+2,NULL);assert(s.state==HD540_DONE&&!patched);
 /* A stalled stock 720p recovery must still send the final original
  * request when its bounded allowance expires. */
 reset();active();stall_alt=1;tick(s.deadline,NULL);
 assert(s.state==HD540_RESTORE_LEAVING&&!patched&&orig_calls==1);
 tick(s.deadline,NULL);assert(s.state==HD540_RESTORE_ENTERING&&orig_calls==2&&s.had_error);
 tick(s.deadline-1,NULL);assert(s.state==HD540_DONE&&!s.roundtrip_ok);
 reset();active();stall_orig=1;display=2;cache_error=1;tick(s.deadline,NULL);
 assert(s.state==HD540_RESTORE_LEAVING&&alt_calls==2);
 q=request(2,HD540_CANCEL);tick(15000501,&q);assert(s.video_tries==2); /* Cancel cannot reset budget. */
 assert(s.state==HD540_RESTORE_ENTERING&&orig_calls==2);
 tick(s.deadline,NULL);assert(s.halted&&s.state==HD540_RECOVERY_FAILED&&!patched);
 unsigned old_alt=(unsigned)alt_calls,old_orig=(unsigned)orig_calls;
 for(unsigned i=0;i<1000;i++){q=request(3+i,HD540_CANCEL);tick(UINT64_C(100000000)+i*1000000,&q);}
 assert((unsigned)alt_calls==old_alt&&(unsigned)orig_calls==old_orig&&s.halted);
 /* A failure before candidate writes retains direct original recovery,
  * then at most one verified alternate/final-original attempt. */
 reset();s.roundtrip_ok=s.hdmi_progressive_ok=1;stall_alt=1;start(HD540_TEST);stall_orig=1;cache_error=1;
 tick(s.deadline,NULL);assert(s.state==HD540_RESTORE_ENTERING&&orig_calls==1&&!applies);
 tick(s.deadline,NULL);assert(s.state==HD540_RESTORE_LEAVING&&alt_calls==2);
 tick(s.deadline,NULL);assert(s.state==HD540_RESTORE_ENTERING&&orig_calls==2);
 tick(s.deadline,NULL);assert(s.halted&&s.state==HD540_RECOVERY_FAILED);
 reset();q=request(1,HD540_ROUNDTRIP);q.magic=0;tick(1,&q);assert(!prepares);
 q.magic=HD540_MAGIC;q.value=999;tick(2,&q);assert(s.state==HD540_REFUSED&&!prepares);
 puts("PASS: XMB exit/fresh-snapshot gates, fresh callback deadlines, stock-only round trip, exact 15-second candidate hold, RAM-first stock recovery cycle, timed-out alternate still returns, RAM-only retry and bounded/latched video recovery");
 return 0;
}
