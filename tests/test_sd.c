#include "sd_default.h"
#include "enforcer_mode.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int value,calls,fail_call,fail_from,ambiguous_write,numeric_only,no_store;
static int request(void *u,const char *path,char *reply,size_t capacity)
{
 (void)u;calls++;
 if(!strcmp(path,"/xmb.ps3$xregistry(/setting/display/0/modeid)")){
  if(calls==fail_call)return -1;
  unsigned mode=numeric_only?(unsigned)value:257;
  snprintf(reply,capacity,"xregistry(/setting/display/0/modeid) => %u (0x%04x)",mode,mode);return 0;
 }
 const char *base="/xmb.ps3$xregistry(/setting/display/tvSystem)";
 size_t n=strlen(base);assert(!strncmp(path,base,n));
 if(path[n]){assert(path[n]=='='&&(path[n+1]=='0'||path[n+1]=='1')&&!path[n+2]);if(!no_store)value=path[n+1]-'0';}
 if(calls==fail_call||(fail_from&&calls>=fail_from))return -1;
 if(ambiguous_write&&calls==3){snprintf(reply,capacity,"OK");return 0;}
 if(path[n]){snprintf(reply,capacity,"xregistry(/setting/display/tvSystem) => %d",value);return 0;}
 snprintf(reply,capacity,"<h2>xregistry(/setting/display/tvSystem) => %d (0x%04x)</h2>",value,value);return 0;
}
static void reset(int initial){value=initial;calls=fail_call=fail_from=ambiguous_write=numeric_only=no_store=0;}
int main(void)
{
 assert(sd_default_parse("OK")==-1);
 assert(sd_default_parse("xregistry(/other/key) => 0 (0x0000)")==-1);
 assert(sd_default_parse("xregistry(/setting/display/tvSystem) => 2 (0x0002)")==-1);
 assert(sd_default_parse("xregistry(/setting/display/tvSystem) => 0 (0x0001)")==-1);
 for(unsigned target=4;target<=5;target++){
  int desired=target==5;
  reset(desired);assert(!sd_default_save(request,NULL,target)&&calls==2&&value==desired);
  reset(!desired);assert(!sd_default_save(request,NULL,target)&&calls==4&&value==desired);
  reset(!desired);fail_call=1;assert(sd_default_save(request,NULL,target)==-1&&calls==1&&value==!desired);
  for(int failure=2;failure<=4;failure+=2){
   reset(!desired);fail_call=failure;assert(sd_default_save(request,NULL,target)==-1&&value==!desired);
  }
  reset(!desired);fail_call=3;assert(!sd_default_save(request,NULL,target)&&value==desired);
  reset(!desired);ambiguous_write=1;assert(!sd_default_save(request,NULL,target)&&value==desired);
  reset(!desired);no_store=1;assert(sd_default_save(request,NULL,target)==-1&&value==!desired);
  reset(!desired);fail_from=4;assert(sd_default_save(request,NULL,target)==-2);
  reset(!desired);numeric_only=1;assert(sd_default_save(request,NULL,target)==-1&&calls==2&&value==!desired);
 }
 reset(0);assert(sd_default_save(request,NULL,2)==-1&&!calls);
 assert(enforcer_target("5\n",2)==5&&enforcer_target("4\n",2)==4);
 assert(enforcer_target("5",1)==4&&enforcer_target("5\n!",3)==4);
 assert(enforcer_target("x\n",2)==4);
 for(unsigned target=4;target<=5;target++)for(unsigned res=1;res<=10;res++)for(unsigned scan=0;scan<=1;scan++)
  assert(enforcer_should_correct(target,res,scan)==((res==4||res==5)&&res!=target&&scan==0));
 assert(!enforcer_should_correct(0,5,0));
 puts("PASS: verified default read/write, ambiguous/failing write recovery, SD preference and HD exclusion");return 0;
}
