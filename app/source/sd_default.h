/* webMAN's named xRegistry interface; never replace the registry file. */
#ifndef SD_DEFAULT_H
#define SD_DEFAULT_H
#include <stddef.h>
typedef int (*sd_request_fn)(void *,const char *,char *,size_t);
int sd_default_parse(const char *reply);
/* 0 = saved/read back; -1 = failed, original intact/restored;
 * -2 = failed and original registry value could not be verified restored. */
int sd_default_save(sd_request_fn,void *,unsigned resolution);
#endif
