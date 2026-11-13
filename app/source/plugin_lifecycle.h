/* Checked lifecycle for this app's VSH helper; no console I/O here. */
#ifndef PLUGIN_LIFECYCLE_H
#define PLUGIN_LIFECYCLE_H
#define COBRA_LOAD_VSH_PLUGIN 0x1ee7u
#define COBRA_UNLOAD_VSH_PLUGIN 0x364fu
typedef struct {
 int (*info)(void *,unsigned,char *,char *);
 int (*load)(void *,unsigned);
 int (*unload)(void *,unsigned);
 void *user;
 char error[160];
} plugin_lifecycle;
int helper_start(plugin_lifecycle *,int replace,unsigned *slot,int *reused);
int helper_stop(plugin_lifecycle *);
#endif
