#include "../plugin/gamma_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static float curve[3]={0.9f,1.0f,1.1f};
static int setters,restores,fail_restore,fail_set,fail_snapshot;
static int snapshot(float out[3]){if(fail_snapshot)return -1;memcpy(out,curve,sizeof(curve));return 0;}
static int apply(const float in[3]){restores++;if(fail_restore)return -1;memcpy(curve,in,sizeof(curve));return 0;}
static int set(unsigned n){setters++;for(unsigned i=0;i<3;i++)curve[i]=100.0f/n;return fail_set?-1:0;}
int main(void)
{
 gamma_service s={0};gamma_service_ops ops={snapshot,apply,set};gamma_reply r;
 gamma_request q={GAMMA_MAGIC,1,GAMMA_TEST,220};
 float baseline[3];memcpy(baseline,curve,sizeof(curve));
 assert(gamma_service_tick(&s,&ops,100,&q,&r)&&r.status==GAMMA_OK&&s.active);
 /* Repeated requests do not extend a test or repeat hardware calls. */
 assert(!gamma_service_tick(&s,&ops,1000000,&q,&r)&&setters==1);
 assert(!gamma_service_tick(&s,&ops,15000099,NULL,&r));
 assert(gamma_service_tick(&s,&ops,15000100,NULL,&r)&&r.status==GAMMA_EXPIRED&&!s.active);
 assert(!memcmp(baseline,curve,sizeof(curve)));
 /* A delayed keep after expiration is refused. */
 q.sequence++;q.action=GAMMA_KEEP;assert(gamma_service_tick(&s,&ops,15000101,&q,&r)&&r.status==GAMMA_FAILED);
 q.sequence++;q.action=GAMMA_TEST;assert(gamma_service_tick(&s,&ops,16000000,&q,&r));
 q.sequence++;q.action=GAMMA_KEEP;assert(gamma_service_tick(&s,&ops,17000000,&q,&r)&&r.status==GAMMA_OK&&!s.deadline);
 assert(!gamma_service_tick(&s,&ops,90000000,NULL,&r)&&s.active);
 q.sequence++;q.action=GAMMA_RESTORE;assert(gamma_service_tick(&s,&ops,90000001,&q,&r)&&r.status==GAMMA_OK&&!s.active);
 assert(!memcmp(curve,baseline,sizeof(curve)));
 /* API errors may have partial effects: rollback still runs. */
 q.sequence++;q.action=GAMMA_TEST;fail_set=1;
 assert(gamma_service_tick(&s,&ops,100000000,&q,&r)&&r.status==GAMMA_FAILED&&!s.active);
 assert(!memcmp(curve,baseline,sizeof(curve)));fail_set=0;
 /* Recovery keeps the original snapshot until restoration succeeds. */
 q.sequence++;assert(gamma_service_tick(&s,&ops,110000000,&q,&r));fail_restore=1;
 assert(gamma_service_tick(&s,&ops,125000000,NULL,&r)&&r.status==GAMMA_FAILED&&s.active);
 int before=restores;assert(!gamma_service_tick(&s,&ops,125500000,NULL,&r)&&restores==before);
 q.sequence++;q.action=GAMMA_KEEP;assert(gamma_service_tick(&s,&ops,126000000,&q,&r)&&r.status==GAMMA_FAILED);
 fail_restore=0;assert(gamma_service_tick(&s,&ops,127000000,NULL,&r)&&!s.active);
 assert(!memcmp(curve,baseline,sizeof(curve)));
 before=setters;q.sequence++;q.action=GAMMA_TEST;q.value=401;
 assert(gamma_service_tick(&s,&ops,130000000,&q,&r)&&r.status==GAMMA_FAILED&&setters==before);
 q.sequence++;q.value=220;fail_snapshot=1;
 assert(gamma_service_tick(&s,&ops,130000001,&q,&r)&&r.status==GAMMA_UNSUPPORTED&&!s.active);
 fail_snapshot=0;
 /* A cancelled/expired adjustment restores the prior KEPT curve. Explicit
  * restore must still recover the original three-channel session baseline. */
 q.sequence++;q.action=GAMMA_TEST;q.value=220;assert(gamma_service_tick(&s,&ops,140000000,&q,&r));
 q.sequence++;q.action=GAMMA_KEEP;assert(gamma_service_tick(&s,&ops,141000000,&q,&r)&&r.status==GAMMA_OK);
 float kept[3];memcpy(kept,curve,sizeof(curve));
 q.sequence++;q.action=GAMMA_TEST;q.value=180;assert(gamma_service_tick(&s,&ops,142000000,&q,&r)&&r.status==GAMMA_OK);
 q.sequence++;q.action=GAMMA_CANCEL;assert(gamma_service_tick(&s,&ops,143000000,&q,&r)&&r.status==GAMMA_OK);
 assert(s.active&&s.current==220&&!s.deadline&&!memcmp(kept,curve,sizeof(curve)));
 q.sequence++;q.action=GAMMA_TEST;q.value=240;assert(gamma_service_tick(&s,&ops,144000000,&q,&r)&&r.status==GAMMA_OK);
 assert(gamma_service_tick(&s,&ops,159000000,NULL,&r)&&r.status==GAMMA_EXPIRED&&r.value==220);
 assert(!memcmp(kept,curve,sizeof(curve)));
 q.sequence++;q.action=GAMMA_RESTORE;fail_restore=1;
 assert(gamma_service_tick(&s,&ops,160000000,&q,&r)&&r.status==GAMMA_FAILED&&s.restore_all);
 q.sequence++;q.action=GAMMA_KEEP;q.value=220;
 assert(gamma_service_tick(&s,&ops,160000001,&q,&r)&&r.status==GAMMA_FAILED);
 fail_restore=0;assert(gamma_service_tick(&s,&ops,162000000,NULL,&r)&&!s.active);
 assert(!memcmp(baseline,curve,sizeof(curve)));
 puts("PASS: gamma restores without app input, exact baseline, stale/late keep refusal, partial-failure rollback and recovery retries");
 return 0;
}
