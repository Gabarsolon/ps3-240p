/* SPDX-License-Identifier: MIT. LV2 rename cannot replace an existing file. */
#include "gamma_ipc.h"
#include <errno.h>
#include <stdio.h>
int gamma_publish(const char *path,const gamma_request *request,int *error)
{
 if(error)*error=0;
 if(!path||!request){if(error)*error=EINVAL;return -1;}
 char temporary[512];int n=snprintf(temporary,sizeof(temporary),"%s.tmp",path);
 if(n<0||(unsigned)n>=sizeof(temporary)){if(error)*error=ENAMETOOLONG;return -1;}
 FILE *f=fopen(temporary,"wb");if(!f){if(error)*error=errno;return -1;}
 int saved=0;
 if(fwrite(request,1,sizeof(*request),f)!=sizeof(*request))saved=errno?errno:EIO;
 if(fflush(f)&&!saved)saved=errno?errno:EIO;
 if(fclose(f)&&!saved)saved=errno?errno:EIO;
 if(saved){if(error)*error=saved;return -1;}
 /* This is an expendable inbox, not a saved preference. Removing the old
  * name before publishing avoids LV2's EEXIST failure. The reader sees an
  * old complete request, no request, or a new complete request. */
 if(remove(path)&&errno!=ENOENT){if(error)*error=errno;return -2;}
 if(rename(temporary,path)){if(error)*error=errno;return -3;}
 return 0;
}
