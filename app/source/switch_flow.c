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
/* Leave for the other SD family; nonzero when refused (or forced refused). */
static int leave_sd(switch_flow *f,unsigned id)
{
 if(f->force_single_sd){
  if(f->patch->log)f->patch->log("Forced single-SD: configure id=%u treated as refused",id);
  return -1;
 }
 return f->configure(f->user,id);
}
static unsigned hd_bounce(switch_flow *f)
{return f->available(f->user,2)>0?2u:f->available(f->user,1)>0?1u:0u;}
/* Programmed analog head; target-derived default only when ambiguous. */
static int active_head(switch_flow *f,unsigned sd)
{int h=patch_active_head(f->patch);return h<0?(sd==5?1:0):h;}
/* Any candidate raster live on either head (software AV cache). */
static int candidate_live(switch_flow *f)
{
 patch_cached_video v;
 if(patch_read_cached_video(f->patch,&v))return 0;
 return patch_candidate_in_cache(f->patch,&v,0)||patch_candidate_in_cache(f->patch,&v,1);
}
/* dev10 primary reprogram path (all consoles): clear the programmed head's
 * profile-index flag and re-request the SD mode with a non-timing field changed
 * and restored (buffer format, then pitch). The request is not dropped as a
 * same-mode no-op, so the AV mode-setter runs a full reprogram from the current
 * table with no resolution change (owner PAL console, dev9b: format variant,
 * 574/625 -> 576/624 in ~0.5 s). Returns 0 only when the mode-setter ran. */
static int reprogram(switch_flow *f,unsigned id)
{
 if(!f->reassert)return -1;
 int head=active_head(f,id);
 (void)patch_invalidate_head(f->patch,head);
 int rc=f->reassert(f->user,id);
 if(f->patch->log)f->patch->log("Reprogram by format reassert id=%u head=%d rc=%d",id,head,rc);
 return rc;
}
int switch_revert(switch_flow *f)
{
 if(!f->pending)return 0;
 /* Restore RAM BEFORE issuing a fresh profile-changing transition. */
 int ram=patch_undo(f->patch);
 if(ram)return fail(f,"RAM recovery incomplete. Reboot console");
 /* Reprogram the restored (pre-switch) tables in place (format reassert). A
  * successful reprogram means the mode-setter re-ran from the restored tables,
  * which is correct even when the pre-switch state was itself a kept candidate
  * (a live candidate raster is then the right result, so it is not a reason to
  * flip; owner dev10 log). Only if the reprogram does not reach the mode-setter,
  * fall back to a real mode change: the other SD family (15 kHz-safe), else an
  * available HD mode. Best-effort: only failing to reach the original is fatal. */
 if(reprogram(f,f->original)){
  if(leave_sd(f,f->original==4?5:4)){
   unsigned bounce=hd_bounce(f);
   if(bounce)(void)f->configure(f->user,bounce);
  }
 }
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
 /* dev10: patch the table, then reprogram in place (format reassert) on every
  * console. A same-mode re-request alone never reaches the AV mode-setter
  * (issue #1, dev7 log), and single-SD consoles cannot flip to the other SD
  * family or reach any HD mode (dev8 log), so the format reassert is the only
  * path that works everywhere; it also avoids the 50/60 Hz flash of the old
  * flip. Fallbacks if the candidate is not live afterwards: the old flip via
  * the other SD family, else a bounce through an available HD mode. */
 int rc=patch_apply(f->patch,family);
 if(!rc){
  int ra=reprogram(f,f->target);
  int live=!ra&&candidate_live(f);
  if(f->patch->log)f->patch->log("Format reassert id=%u rc=%d live=%d",f->target,ra,live);
  if(!live){
   if(!leave_sd(f,f->alternate)){
    if(f->patch->log)f->patch->log("Fallback: flip via id=%u",f->alternate);
    rc=f->configure(f->user,f->target);
   }else{
    unsigned bounce=hd_bounce(f);
    if(f->patch->log)f->patch->log("Fallback: HD bounce id=%u (alternate=%u refused)",bounce,f->alternate);
    if(bounce)rc=f->configure(f->user,bounce)||f->configure(f->user,f->target);
   }
  }
 }
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
 if(patch_restore(f->patch)||(reprogram(f,target)&&(leave_sd(f,f->alternate)||f->configure(f->user,target)))){
  char cause[200];snprintf(cause,sizeof(cause),"%s",f->patch->error[0]?f->patch->error:"Stock display request failed");
  if(switch_revert(f))return -1;
  return fail(f,cause);
 }
 return 0;
}
