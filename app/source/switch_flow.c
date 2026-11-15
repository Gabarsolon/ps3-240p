#include "switch_flow.h"
#include <stdio.h>
#include <string.h>
static int fail(switch_flow *f,const char *s)
{snprintf(f->error,sizeof(f->error),"%s",s);return -1;}
int switch_tests_available(void)
{
#if defined(TIMING_OFFLINE_TEST) || defined(PS3_ENABLE_TIMING_SWITCH)
 return 1;
#else
 return 0;
#endif
}
static int select_modes(switch_flow *f,unsigned target,unsigned current,int analog)
{
 if(f->pending)return fail(f,"Finish the current picture test first");
 if(target!=4&&target!=5)return fail(f,"Invalid SD target");
 if(!analog)return fail(f,"Select AV Multi Out in XMB display settings first");
 if(current!=4&&current!=5)return fail(f,"Start from 480i or 576i for this SD experiment");
 f->original=current;f->target=target;f->alternate=target==5?4:5;
 /* Avoid 480p/720p on a 15 kHz SCART television; the transition uses only the
  * two interlaced SD families (480i and 576i), both 15 kHz-safe.
  * cellVideoOutGetResolutionAvailability reports only the console's region SD
  * standard, so a PAL unit advertises 576i alone and an NTSC unit 480i alone,
  * yet the encoder drives either interlaced family and the app configures
  * either in practice. Require only that one SD family be exposed; the
  * transition still drives both, and configure() plus the automatic revert
  * recover a console that genuinely refuses the second family. */
 if(f->available(f->user,target)<=0&&f->available(f->user,f->alternate)<=0)
  return fail(f,"Firmware exposes no SD mode for AV Multi; timing test stopped");
 return 0;
}
int switch_revert(switch_flow *f)
{
 if(!f->pending)return 0;
 /* Restore RAM BEFORE issuing a fresh profile-changing transition. */
 int ram=patch_undo(f->patch);
 if(ram)return fail(f,"RAM recovery incomplete. Reboot console");
 /* Bounce through the other SD family to force the AV manager to reprogram
  * stock timing, then return to the original profile. The bounce is
  * best-effort: a console that cannot configure the opposite SD family (an
  * NTSC unit refusing 576i, say) must still recover, so only a failure to
  * reach the original profile is fatal. When the forward flip failed before
  * leaving the original, the display never changed and this confirms it. */
 int a=f->configure(f->user,f->original==4?5:4);(void)a;
 /* Also invalidate the original head's cached index so returning to the
  * original mode reprograms stock timing even when the bounce could not leave
  * it (the console refused the other SD family). Best-effort. */
 (void)patch_invalidate_head(f->patch,f->original==5?1:0);
 int b=f->configure(f->user,f->original);
 if(b)return fail(f,"RAM restored; display recovery failed. Reboot console");
 f->pending=0;return 0;
}
int switch_begin(switch_flow *f,int family,unsigned current,int analog)
{return switch_begin_target(f,family,family==MODE_288P_PAL?5:4,current,analog);}
int switch_begin_target(switch_flow *f,int family,unsigned target,unsigned current,int analog)
{
 if(!switch_tests_available())return fail(f,"Timing tests disabled after signal loss and console freeze. Reboot restores stock RAM.");
 f->error[0]=0;
 f->patch->error[0]=0;
 if(family<0||family>2)return fail(f,"Invalid timing family");
 if((family==MODE_240P_NTSC&&target!=4)||(family==MODE_288P_PAL&&target!=5))return fail(f,"Timing family does not match SD target");
 if(select_modes(f,target,current,analog))return -1;
 if(patch_snapshot(f->patch))return fail(f,f->patch->error);
 f->pending=1;
 if(patch_restore(f->patch)){
  char cause[200];snprintf(cause,sizeof(cause),"%s",f->patch->error[0]?f->patch->error:"Stock restore failed");
  if(switch_revert(f))return -1;
  return fail(f,cause);
 }
 /* Preferred path: leave the candidate's SD profile for the other SD family
  * and return, so the AV manager's profile-change gate reprograms the patched
  * table. A region-locked console exposes only its own SD standard and cannot
  * configure the other family, so when that leave fails, fall back to
  * invalidating the head's cached profile index: the AV manager then reprograms
  * from the patched table on a repeat of the current mode, with no family
  * change. head 0 drives 480i, head 1 drives 576i. */
 int head=f->target==5?1:0;
 int rc=f->configure(f->user,f->alternate)
   ? (patch_apply(f->patch,family)||patch_invalidate_head(f->patch,head)||f->configure(f->user,f->target))
   : (patch_apply(f->patch,family)||f->configure(f->user,f->target));
 if(rc){
  char cause[200];snprintf(cause,sizeof(cause),"%s",f->patch->error[0]?f->patch->error:"Video configuration failed; see diagnostic log");
  if(switch_revert(f))return -1;
  return fail(f,cause);
 }
 return 0;
}
void switch_keep(switch_flow *f){f->pending=0;}
int switch_stock(switch_flow *f,unsigned current,int analog)
{int rc=switch_stock_begin(f,current,current,analog);if(!rc)switch_keep(f);return rc;}
int switch_stock_begin(switch_flow *f,unsigned target,unsigned current,int analog)
{
 if(!switch_tests_available())return fail(f,"Display transitions disabled. Reboot the console to restore stock timing.");
 f->error[0]=0;
 f->patch->error[0]=0;
 if(select_modes(f,target,current,analog))return -1;
 if(patch_snapshot(f->patch))return fail(f,f->patch->error);
 f->pending=1;
 if(patch_restore(f->patch)||f->configure(f->user,f->alternate)||f->configure(f->user,target)){
  char cause[200];snprintf(cause,sizeof(cause),"%s",f->patch->error[0]?f->patch->error:"Stock display request failed");
  if(switch_revert(f))return -1;
  return fail(f,cause);
 }
 return 0;
}
