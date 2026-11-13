/* SPDX-License-Identifier: MIT. Display correction, independent of timing. */
#ifndef CRT_GAMMA_H
#define CRT_GAMMA_H
#include <stddef.h>
#include <stdint.h>
#define CRT_GAMMA_MIN 50
#define CRT_GAMMA_MAX 400
#define CRT_GAMMA_NEUTRAL 100
/* Values are hundredths: 220 means 2.20. Greater than 1 brightens shadows. */
int crt_gamma_parse(const char *text,int *value);
void crt_gamma_format(int value,char text[5]);
int crt_gamma_ramp(int value,uint8_t ramp[256]);
int crt_gamma_load(const char *path,int *value);
int crt_gamma_save(const char *path,int value);
/* Self-contained Cg source, with the value baked in for older PS3 builds. */
int crt_gamma_shader(int value,char *out,size_t capacity);
#endif
