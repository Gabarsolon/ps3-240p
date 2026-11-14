/* SPDX-License-Identifier: MIT. Checked VSH source-selection experiment. */
#ifndef XGA_SOURCE_H
#define XGA_SOURCE_H
#include <stdint.h>
typedef int (*xga_source_read)(void *,uint32_t,uint64_t *);
typedef int (*xga_source_write)(void *,uint32_t,uint64_t);
typedef struct {
 xga_source_read read;
 xga_source_write write;
 void *user;
 uint64_t original[16];
 int ready,dirty,native;
} xga_source;
int xga_source_resolve(xga_source *,xga_source_read,xga_source_write,void *);
int xga_source_resolve_native(xga_source *,xga_source_read,xga_source_write,void *);
int xga_source_apply(xga_source *);
int xga_source_restore(xga_source *);
#endif
