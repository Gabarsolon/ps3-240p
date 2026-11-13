#include "../plugin/vsh_video.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
static unsigned current,format,pitch,aspect;
static int state,query_error,map_error,maps,updates;
int mock_native_get_state(unsigned out,unsigned device,void *data)
{assert(!out&&!device);if(query_error)return query_error;unsigned char *b=data;b[8]=(unsigned char)current;b[9]=1;b[11]=(unsigned char)aspect;return 0;}
int mock_native_get_display(uint32_t *flags,uint32_t *mode,uint32_t *fmt,uint32_t *stride)
{*flags=0x01011102;*mode=current==5?6:3;*fmt=format;*stride=pitch;return state;}
int mock_native_map(uint32_t *flags,uint32_t *mode,int res,int asp,unsigned option)
{assert((res==4||res==5)&&asp>=1&&asp<=2&&!option);maps++;if(map_error)return map_error;*flags=res==5?0x21102:0x11102;*mode=res==5?6:3;return 1;}
int mock_native_update(unsigned out,uint32_t flags,uint32_t mode,uint32_t fmt,uint32_t stride,void *option,int wait)
{
 assert(!out&&!option&&wait==1);assert(fmt==format&&stride==pitch);
 assert((flags==0x21102&&mode==6)||(flags==0x11102&&mode==3));
 updates++;current=mode==6?5:4;return 0;
}
static void reset(void){current=4;format=5;pitch=3072;aspect=1;state=query_error=map_error=maps=updates=0;assert(!vsh_video_bind());}
int main(void)
{
 reset();assert(!vsh_video_configure(5,1)&&current==5&&updates==1);
 assert(!vsh_video_configure(4,1)&&current==4&&updates==2);
 /* Preserve the actual active surface, including a wide/aligned stride. */
 reset();format=7;pitch=5120;aspect=2;assert(!vsh_video_configure(5,2)&&updates==1);
 reset();current=2;assert(vsh_video_configure(5,1)==-1&&!updates&&!maps);
 reset();state=2;assert(vsh_video_configure(5,1)==-1&&!updates&&!maps);
 reset();query_error=-4;assert(vsh_video_configure(5,1)==-1&&!updates&&!maps);
 reset();map_error=-5;assert(vsh_video_configure(5,1)==-1&&!updates&&maps==1);
 reset();pitch=0;assert(vsh_video_configure(5,1)==-1&&!updates&&!maps);
 reset();pitch=UINT32_MAX;assert(vsh_video_configure(5,1)==-1&&!updates&&!maps);
 reset();assert(vsh_video_configure(1,1)==-1&&!updates&&!maps);
 puts("PASS: native SD switch/return, active format/pitch preservation, positive mapper success, HD/busy/error/invalid-pitch refusal");return 0;
}
