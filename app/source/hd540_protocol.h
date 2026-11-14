/* SPDX-License-Identifier: MIT. Experimental timed test, no keep/enforce action. */
#ifndef HD540_PROTOCOL_H
#define HD540_PROTOCOL_H
#include <stdint.h>
#define HD540_MAGIC UINT32_C(0x48443542) /* HD5I, rebuilt XMB source-pitch check */
#define HD540_REQUEST "/dev_hdd0/tmp/crt_hd540.request"
#define HD540_REPLY "/dev_hdd0/tmp/crt_hd540.reply"
#define HD540_READY "/dev_hdd0/tmp/crt_hd540.v18.ready"
#define HD540_EXPERIMENTAL_VALUE 1u
#define HD540_TEST_SECONDS 15u
enum {HD540_INSPECT=1,HD540_TEST=2,HD540_CANCEL=3,HD540_ROUNDTRIP=4,HD540_HDMI_CONTROL=5,
      HD540_HDMI_720_CONTROL=6};
enum {HD540_IDLE,HD540_LEAVING,HD540_ENTERING,HD540_ACTIVE,
      HD540_RESTORE_RAM,HD540_RESTORE_LEAVING,HD540_RESTORE_ENTERING,
      HD540_DONE,HD540_REFUSED,HD540_RECOVERY_FAILED,HD540_WAIT_APP_EXIT};
typedef struct {uint32_t magic,sequence,action,value;} hd540_request;
typedef struct {uint32_t magic,sequence,status,detail;} hd540_reply;
_Static_assert(sizeof(hd540_request)==16,"HD request ABI");
_Static_assert(sizeof(hd540_reply)==16,"HD reply ABI");
#endif
