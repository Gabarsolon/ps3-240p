#include "../plugin/vsh_video.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
static unsigned current,format,pitch,aspect;
static int state,query_error,map_error,maps,updates;
static int hd;
static uint32_t hd_flags,hd_mode;
int mock_native_get_state(unsigned out,unsigned device,void *data)
{assert(!out&&!device);if(query_error)return query_error;unsigned char *b=data;b[8]=(unsigned char)current;b[9]=1;b[11]=(unsigned char)aspect;return 0;}
int mock_native_get_display(uint32_t *flags,uint32_t *mode,uint32_t *fmt,uint32_t *stride)
{*flags=0x01011102;*mode=current==5?6:3;*fmt=format;*stride=pitch;return state;}
int mock_native_map(uint32_t *flags,uint32_t *mode,int res,int asp,unsigned option)
{assert((res==4||res==5||(hd&&res==2))&&asp>=1&&asp<=2&&!option);maps++;if(map_error)return map_error;if(res==2){*flags=hd_flags;*mode=hd_mode;}else{*flags=res==5?0x21102:0x11102;*mode=res==5?6:3;}return 1;}
int mock_native_update(unsigned out,uint32_t flags,uint32_t mode,uint32_t fmt,uint32_t stride,void *option,int wait)
{
 assert(!out&&!option&&wait==1);assert(fmt==format&&stride==pitch);
 assert((flags==0x21102&&mode==6)||(flags==0x11102&&mode==3)||(hd&&flags==0x10205&&mode==11));
 updates++;current=mode==6?5:4;return 0;
}
static void reset(void){current=4;format=5;pitch=3072;aspect=1;hd=0;hd_flags=0x10205;hd_mode=8;state=query_error=map_error=maps=updates=0;assert(!vsh_video_bind());}
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
 reset();hd=1;pitch=7680;uint32_t flags,mode;
 /* Real dev3 failure: the native mapper accepts a 720-line source but
  * returns conversion mode 8, leaving the output at 1080i. Request the
  * independently audited progressive mode, retaining connector/refresh. */
 assert(!vsh_video_hd720(&flags,&mode)&&flags==0x10205&&mode==11);
 hd_mode=11;assert(!vsh_video_hd720(&flags,&mode)&&mode==11);
 hd_mode=9;assert(vsh_video_hd720(&flags,&mode)==-1);
 hd_mode=12;assert(vsh_video_hd720(&flags,&mode)==-1);
 hd_mode=0;assert(vsh_video_hd720(&flags,&mode)==-1);
 hd_mode=8;hd_flags=0x10202;assert(vsh_video_hd720(&flags,&mode)==-1);
 hd_flags=0x10f05;assert(vsh_video_hd720(&flags,&mode)==-1);
 hd_flags=0x20205;assert(vsh_video_hd720(&flags,&mode)==-1);
 hd_flags=0x10205;assert(!vsh_video_hd720(&flags,&mode)&&mode==11&&!updates);
 vsh_video_state surface={0};surface.format=format;surface.pitch=pitch;
 assert(!vsh_video_hd_submit(&surface,flags,mode)&&updates==1);
 assert(vsh_video_hd_submit(&surface,0x11102,mode)==-1&&updates==1);
 assert(vsh_video_hd_submit(&surface,0x11f05,mode)==-1&&updates==1);
 surface.pitch=5120;assert(vsh_video_hd_submit(&surface,flags,mode)==-1&&updates==1);
 surface.pitch=7681;assert(vsh_video_hd_submit(&surface,flags,mode)==-1&&updates==1);
 surface.pitch=7680;assert(vsh_video_hd_submit(&surface,flags,0)==-1&&updates==1);
 assert(vsh_video_hd_submit(NULL,flags,mode)==-1&&updates==1);
 map_error=-5;assert(vsh_video_hd720(&flags,&mode)==-1);
 puts("PASS: native SD switch/return, surface preservation, explicit progressive HD mode instead of 1080i conversion, HD/busy/error/invalid-pitch refusal");return 0;
}
