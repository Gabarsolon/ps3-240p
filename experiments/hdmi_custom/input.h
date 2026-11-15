/* SPDX-License-Identifier: MIT. Bounded, checked VSH pad ownership. */
#ifndef HDMI_CUSTOM_INPUT_H
#define HDMI_CUSTOM_INPUT_H
#include <stdint.h>
int hc_input_bind(void);
int hc_foreground(void *);
int hc_prompt(void *,unsigned,unsigned);
void hc_close_prompt(void *);
int hc_input_cleanup(void);
void hc_input_log(void (*)(const char *,uint32_t));
#endif
