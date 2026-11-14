/* SPDX-License-Identifier: MIT. Diagnostic-build integration only. */
#ifndef VSH_PS1_AUDIO_LOG_H
#define VSH_PS1_AUDIO_LOG_H
void vsh_ps1_audio_log_open(void);
void vsh_ps1_audio_log_poll(int armed,int app,unsigned target);
void vsh_ps1_audio_log_close(void);
#endif
