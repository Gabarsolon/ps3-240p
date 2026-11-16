#ifndef SWITCH_FLOW_H
#define SWITCH_FLOW_H
#include "lv1_patch.h"
/* Resolution IDs are the public video-out namespace, not LV1 profile IDs. */
typedef struct {
 patch_context *patch;
 int (*available)(void *,unsigned);
 int (*configure)(void *,unsigned);
 /* Same SD mode with a non-timing change (aspect toggle) so the request is not
  * dropped as a same-mode no-op; may be NULL. */
 int (*reassert)(void *,unsigned);
 /* Debug: treat the other SD family as refused (exercise the single-SD path
  * on a console that can flip). */
 int force_single_sd;
 void *user;
 unsigned original,target,alternate;
 int pending;
 char error[200];
} switch_flow;
/* Production builds explicitly opt into checked SD transitions. */
int switch_tests_available(void);
int switch_begin(switch_flow *,int family,unsigned current,int analog);
int switch_begin_target(switch_flow *,int family,unsigned target,unsigned current,int analog);
int switch_stock_begin(switch_flow *,unsigned target,unsigned current,int analog);
int switch_revert(switch_flow *);
void switch_keep(switch_flow *);
int switch_stock(switch_flow *,unsigned current,int analog);
#endif
