/* SPDX-License-Identifier: MIT. Publish one complete command to the helper. */
#ifndef CRT_GAMMA_IPC_H
#define CRT_GAMMA_IPC_H
#include "gamma_protocol.h"
/* The transient inbox may briefly be absent. The helper ignores missing
 * files and duplicate sequences; it must never see an incomplete new file. */
int gamma_publish(const char *path,const gamma_request *,int *error);
#endif
