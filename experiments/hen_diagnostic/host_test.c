/* SPDX-License-Identifier: MIT. Self-authored host fixtures, no firmware bytes. */
#include "diagnostic.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct {int scenario;unsigned calls,reads,slots,names;} fixture;
static uint64_t raw(void *user,uint64_t op,uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e)
{
 fixture *t=user;t->calls++;
 if(op==0x1337){assert(!(a|b|c|d|e));return t->scenario==1?0:0x1337;}
 if(op==0x1339){assert(!(a|b|c|d|e));return 0x0360;}
 assert(op==0x7777);
 if(a==0x11)return t->scenario==2?UINT64_C(0x80010003):0x124;
 if(a==0x12)return 0x111;
 if(a==0x14)return 0x493;
 if(a==0x47){
  assert(b>=1&&b<=6&&c&&d&&!e);t->slots++;
  if(t->scenario==3)return 0x80010006;
  if(b==1){
   strcpy((char *)(uintptr_t)c,"own\"plugin\n");
   strcpy((char *)(uintptr_t)d,"/dev_hdd0/home/00000001/opaque.sprx");return 0;
  }
  return 0x80010006;
 }
 if(a==0x21){
  assert(b&&!(c|d|e));
  if(t->scenario==4)return 0x80010003;
  uint32_t *p=(void *)(uintptr_t)b;p[0]=100;p[1]=200;
  if(t->scenario==5)p[2]=300;
  if(t->scenario==9)p[2]=100;
  return 0;
 }
 if(a==0x22){
  assert(b&&c&&!(d|e));t->names++;
  const char *name=b==100||(t->scenario==5&&b==300)?"vsh":"other_process_not_logged";
  if(t->scenario==11&&b==100)name="main_vsh.self";
  if(t->scenario==12&&b==100)name="unverified_vsh_copy";
  if(t->scenario==6)name="not_vsh";
  memcpy((void *)(uintptr_t)c,name,strlen(name));return 0;
 }
 assert(a==0x31&&b==100&&c==HD_VSH_HEADER_ADDRESS&&d&&e==16);t->reads++;
 if(t->scenario==7)return 0x80010003;
 unsigned char *p=(void *)(uintptr_t)d;memcpy(p,"\177ELF",4);p[4]=2;p[5]=2;p[6]=1;
 if(t->scenario==8)p[0]=0;return 0;
}
static void video(void *user,hd_video *v)
{
 fixture *t=user;
 v->state_rc=v->configuration_rc=v->device_rc=0;v->device_count=1;
 v->state=1;v->port=0x85;v->pitch=2880;
 v->current=(hd_mode){5,0,0,1,2};v->modes[0]=v->current;
 v->mode_count=t->scenario==10?100:1;v->reported_modes=v->mode_count;
}
static void gate_tests(void)
{
 fixture t={0};hd_bus b={raw,&t,1,1,100};
 const uint64_t denied[]={0x1008,0x1009,0x1006,0x1007,0x32,0x48,0x46,0x95,0x92,0x2000};
 for(unsigned i=0;i<sizeof(denied)/sizeof(*denied);i++)
  assert(hd_checked_call(&b,0x7777,denied[i],1,1,1,1)==HD_UNQUERIED);
 assert(hd_checked_call(&b,0x1234,0,0,0,0,0)==HD_UNQUERIED);
 assert(hd_checked_call(&b,0x1337,1,0,0,0,0)==HD_UNQUERIED);
 assert(hd_checked_call(&b,0x7777,0x47,0,1,1,0)==HD_UNQUERIED);
 assert(hd_checked_call(&b,0x7777,0x47,7,1,1,0)==HD_UNQUERIED);
 assert(hd_checked_call(&b,0x7777,0x31,101,0x10000,1,16)==HD_UNQUERIED);
 assert(hd_checked_call(&b,0x7777,0x31,100,0x10008,1,16)==HD_UNQUERIED);
 assert(hd_checked_call(&b,0x7777,0x31,100,0x10000,1,17)==HD_UNQUERIED);
 assert(hd_checked_call(&b,0x7777,0x31,100,0x10000,0,16)==HD_UNQUERIED);
 assert(t.calls==0);
 b.hen_confirmed=0;
 assert(hd_checked_call(&b,0x1339,0,0,0,0,0)==HD_UNQUERIED);
 assert(hd_checked_call(&b,0x7777,0x11,0,0,0,0)==HD_UNQUERIED);
 b.hen_confirmed=1;b.mapi_confirmed=0;
 assert(hd_checked_call(&b,0x7777,0x21,1,0,0,0)==HD_UNQUERIED);
 assert(t.calls==0);
}
int main(int argc,char **argv)
{
 gate_tests();fixture t={argc>1?atoi(argv[1]):0,0,0,0,0};hd_report report;
 hd_collect(&report,raw,&t,video);
#ifdef HD_PUBLIC_ONLY
 assert(t.calls==0&&t.reads==0&&t.slots==0&&t.names==0);
 assert(report.hen_identity==HD_UNQUERIED&&report.hen_revision==HD_UNQUERIED&&report.core_version==HD_UNQUERIED);
#else
 if(t.scenario==1)assert(t.calls==1&&t.reads==0&&t.slots==0);
 else if(t.scenario==2)assert(t.calls==3&&t.reads==0&&t.slots==0);
 else{
  assert(t.slots==6&&t.reads<=1&&t.names<=16);
  if(t.scenario==4||t.scenario==5||t.scenario==6||t.scenario==12)assert(t.reads==0);
  else assert(t.reads==1);
 }
#endif
 assert(!hd_write_json(stdout,&report));return 0;
}
