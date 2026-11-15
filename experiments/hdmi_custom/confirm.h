/* SPDX-License-Identifier: MIT. Confirmation requires a new press and release. */
#ifndef HDMI_CUSTOM_CONFIRM_H
#define HDMI_CUSTOM_CONFIRM_H
typedef struct {int released,choice;} hc_confirm;
int hc_confirm_sample(hc_confirm *,unsigned);
#endif
