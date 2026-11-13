/* SPDX-License-Identifier: MIT. Fixed-width IPC, no pointers or native floats. */
#ifndef CRT_GAMMA_PROTOCOL_H
#define CRT_GAMMA_PROTOCOL_H
#include <stdint.h>
#define GAMMA_REQUEST "/dev_hdd0/tmp/crt_gamma.request"
#define GAMMA_REPLY "/dev_hdd0/tmp/crt_gamma.reply"
#define GAMMA_READY "/dev_hdd0/tmp/crt_gamma.ready"
#define GAMMA_MAGIC UINT32_C(0x43473031)
enum {GAMMA_TEST=1,GAMMA_KEEP=2,GAMMA_RESTORE=3,GAMMA_CANCEL=4};
enum {GAMMA_OK=0,GAMMA_UNSUPPORTED=1,GAMMA_FAILED=2,GAMMA_EXPIRED=3};
typedef struct {uint32_t magic,sequence,action,value;} gamma_request;
typedef struct {uint32_t magic,sequence,status,value;} gamma_reply;
#endif
