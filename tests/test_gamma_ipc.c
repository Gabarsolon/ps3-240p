#include "gamma_ipc.h"
#include "../plugin/gamma_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static float curve[3]={1,1,1};
static int snapshot(float out[3]){memcpy(out,curve,sizeof(curve));return 0;}
static int apply(const float in[3]){memcpy(curve,in,sizeof(curve));return 0;}
static int set(unsigned value){for(unsigned i=0;i<3;i++)curve[i]=100.0f/value;return 0;}
int main(void)
{
 const char *path="build/tests/gamma-inbox.bin";
 gamma_service session={0};gamma_service_ops ops={snapshot,apply,set};gamma_reply reply;
 unsigned actions[]={GAMMA_TEST,GAMMA_KEEP,GAMMA_RESTORE,GAMMA_TEST,GAMMA_CANCEL};
 remove(path);remove("build/tests/gamma-inbox.bin.tmp");
 for(unsigned i=0;i<sizeof(actions)/sizeof(actions[0]);i++){
  gamma_request sent={GAMMA_MAGIC,i+1,actions[i],220},received={0};int error=123;
  /* The reader deliberately leaves the previous command in place, matching
   * the resident helper. Windows rename, like LV2, refuses an existing name. */
  assert(!gamma_publish(path,&sent,&error)&&!error);
  FILE *f=fopen(path,"rb");assert(f);assert(fread(&received,1,sizeof(received),f)==sizeof(received));
  assert(fgetc(f)==EOF);fclose(f);assert(!memcmp(&sent,&received,sizeof(sent)));
  assert(gamma_service_tick(&session,&ops,(uint64_t)(i+1)*1000000,&received,&reply));
  assert(reply.status==GAMMA_OK&&reply.sequence==sent.sequence);
 }
 assert(!session.active&&curve[0]==1&&curve[1]==1&&curve[2]==1);
 int error=0;gamma_request q={GAMMA_MAGIC,99,GAMMA_TEST,220};
 assert(gamma_publish("build/tests/nonexistent-directory/inbox",&q,&error)<0&&error);
 remove(path);
 puts("PASS: repeated test/keep/restore commands replace the existing inbox and reach the real gamma service");
 return 0;
}
