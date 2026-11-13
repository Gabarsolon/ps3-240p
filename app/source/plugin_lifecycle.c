#include "plugin_lifecycle.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static int scan(plugin_lifecycle *p,unsigned *resident,unsigned *empty)
{
 *resident=*empty=0;
 for(unsigned s=1;s<=6;s++){
  /* PS3MAPI need not terminate copied strings. Clear the entire buffers,
   * not just byte zero, so data from the previous slot cannot leak in. */
  char name[96]={0},path[256]={0};
  int rc=p->info(p->user,s,name,path);name[95]=path[255]=0;
  if(rc&&(uint32_t)rc!=UINT32_C(0x80010006)){
   snprintf(p->error,sizeof(p->error),"Could not inspect VSH slot %u (%08x)",s,(unsigned)rc);return -1;
  }
  if((uint32_t)rc==UINT32_C(0x80010006)){if(!*empty)*empty=s;continue;}
  if(!strcmp(name,"vsh_240p")){
   if(*resident){snprintf(p->error,sizeof(p->error),"Multiple enforcers are resident; reboot before enabling");return -1;}
   *resident=s;
  }
  if(!name[0]&&!path[0]&&!*empty)*empty=s;
 }
 return 0;
}
int helper_stop(plugin_lifecycle *p)
{
 unsigned resident,empty;p->error[0]=0;
 if(scan(p,&resident,&empty))return -1;
 if(!resident)return 0;
 int rc=p->unload(p->user,resident);
 if(rc){snprintf(p->error,sizeof(p->error),"Enforcer unload failed (%08x)",(unsigned)rc);return -1;}
 if(scan(p,&resident,&empty))return -1;
 if(resident){snprintf(p->error,sizeof(p->error),"Enforcer slot remained occupied after unload");return -1;}
 return 0;
}
int helper_start(plugin_lifecycle *p,int replace,unsigned *slot,int *reused)
{
 unsigned resident,empty;p->error[0]=0;*slot=0;*reused=0;
 if(scan(p,&resident,&empty))return -1;
 if(resident&&!replace){*slot=resident;*reused=1;return 0;}
 if(resident){
  if(helper_stop(p)||scan(p,&resident,&empty))return -1;
 }
 if(!empty){snprintf(p->error,sizeof(p->error),"No free VSH plugin slot");return -1;}
 int rc=p->load(p->user,empty);
 if(rc){snprintf(p->error,sizeof(p->error),"Enforcer load failed (%08x)",(unsigned)rc);return -1;}
 unsigned loaded=empty;
 if(scan(p,&resident,&empty))return -1;
 if(resident!=loaded){snprintf(p->error,sizeof(p->error),"Enforcer load did not populate the selected slot");return -1;}
 *slot=loaded;return 0;
}
