/* SPDX-License-Identifier: MIT. Bounded, read-only PS1 process sampler. */
#ifndef PS1_AUDIO_PCM_H
#define PS1_AUDIO_PCM_H
#include <stdint.h>
/* Writes at most 64 independent ring snapshots to a sequence-specific HDD
 * diagnostic file. No input recording, process hooks or memory writes. */
int ps1_pcm_collect(unsigned sequence);
#endif
