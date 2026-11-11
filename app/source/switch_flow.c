#include "switch_flow.h"
#include <stdio.h>
#include <string.h>
static int fail(switch_flow *f,const char *s)
{snprintf(f->error,sizeof(f->error),"%s",s);return -1;}
int switch_tests_available(void)
{
#ifdef TIMING_OFFLINE_TEST
 return 1;
#else
 return 0;
#endif
}
static int select_modes(switch_flow *f,unsigned target,unsigned current,int analog)
{
 if(f->pending)return fail(f,"Finish the current picture test first");
 if(!analog)return fail(f,"Select AV Multi Out in XMB display settings first");
 if(current!=4&&current!=5)return fail(f,"Start from 480i or 576i for this SD experiment");
 f->original=current;f->target=target;f->alternate=target==5?4:5;
 /* Avoid 480p/720p on a 15 kHz SCART television. If the firmware exposes no
  * second SD family, no safe profile-changing transition is known here. */
 if(f->available(f->user,target)<=0||f->available(f->user,f->alternate)<=0)
  return fail(f,"Firmware does not expose both SD modes; timing test stopped");
 return 0;
}
int switch_revert(switch_flow *f)
{
 if(!f->pending)return 0;
 /* Restore RAM BEFORE issuing a fresh profile-changing transition. */
 int ram=patch_undo(f->patch);
 if(ram)return fail(f,"RAM recovery incomplete. Reboot console");
 int a=f->configure(f->user,f->original==4?5:4);
 int b=f->configure(f->user,f->original);
 if(a||b)return fail(f,"RAM restored; display recovery failed. Reboot console");
 f->pending=0;return 0;
}
int switch_begin(switch_flow *f,int family,unsigned current,int analog)
{
 if(!switch_tests_available())return fail(f,"Timing tests disabled after signal loss and console freeze. Reboot restores stock RAM.");
 f->error[0]=0;
 f->patch->error[0]=0;
 if(family<0||family>1)return fail(f,"Invalid timing family");
 if(select_modes(f,family?5:4,current,analog))return -1;
 if(patch_snapshot(f->patch))return fail(f,f->patch->error);
 f->pending=1;
 /* Leave the candidate's profile before patching it; returning to it must
  * pass the AV manager's profile-change gate. Check every API result. */
 if(patch_restore(f->patch)||f->configure(f->user,f->alternate)||
    patch_apply(f->patch,family)||f->configure(f->user,f->target)){
  char cause[200];snprintf(cause,sizeof(cause),"%s",f->patch->error[0]?f->patch->error:"Video configuration failed; see diagnostic log");
  if(switch_revert(f))return -1;
  return fail(f,cause);
 }
 return 0;
}
void switch_keep(switch_flow *f){f->pending=0;}
int switch_stock(switch_flow *f,unsigned current,int analog)
{
 if(!switch_tests_available())return fail(f,"Display transitions disabled. Reboot the console to restore stock timing.");
 f->error[0]=0;
 f->patch->error[0]=0;
 if(select_modes(f,current,current,analog))return -1;
 if(patch_restore(f->patch))return fail(f,f->patch->error);
 if(f->configure(f->user,f->alternate)||f->configure(f->user,current))
  return fail(f,"Stock RAM restored; display request failed. See log");
 return 0;
}
