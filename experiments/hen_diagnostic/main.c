/* SPDX-License-Identifier: MIT. Runs once, publishes its report, exits to XMB. */
#include "diagnostic.h"
#include <stdlib.h>
#include <string.h>
#ifndef HD_PUBLIC_ONLY
extern uint64_t hd_native_raw(void *,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
#endif
extern void hd_native_video(void *,hd_video *);

int main(void)
{
 /* Opening our own report before querying also avoids probing without a log. */
 FILE *f=fopen(HD_STAGING_PATH,"wb");if(!f)return 1;
 hd_report report;
#ifdef HD_PUBLIC_ONLY
 hd_collect(&report,NULL,NULL,hd_native_video);
#else
 hd_collect(&report,hd_native_raw,NULL,hd_native_video);
#endif
 int bad=hd_write_json(f,&report);
 if(fflush(f))bad=1;
 if(fclose(f))bad=1;
 if(bad){remove(HD_STAGING_PATH);return 2;}
 /* Read back the complete small report before publishing. */
 f=fopen(HD_STAGING_PATH,"rb");if(!f)return 3;
 unsigned bytes=0;int ch,last=0;
 while((ch=fgetc(f))!=EOF&&bytes<16384){last=ch;bytes++;}
 bad=ferror(f)||bytes<128||bytes>=16384||last!='\n';
 if(fclose(f))bad=1;
 if(bad){remove(HD_STAGING_PATH);return 4;}
 if(rename(HD_STAGING_PATH,HD_REPORT_PATH))return 5;
 return 0;
}
