#ifndef SWITCH_FLOW_H
#define SWITCH_FLOW_H
#include "lv1_patch.h"
/* Resolution IDs are the public video-out namespace, not LV1 profile IDs. */
typedef struct {
 patch_context *patch;
 int (*available)(void *,unsigned);
 int (*configure)(void *,unsigned);
 void *user;
 unsigned original,target,alternate;
 int pending;
 char error[200];
} switch_flow;
/* Production timing transitions are disabled after owner-reported signal
 * loss and a PS-menu console freeze. Host fixtures can exercise old logic. */
int switch_tests_available(void);
int switch_begin(switch_flow *,int family,unsigned current,int analog);
int switch_revert(switch_flow *);
void switch_keep(switch_flow *);
int switch_stock(switch_flow *,unsigned current,int analog);
#endif
