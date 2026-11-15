/* SPDX-License-Identifier: MIT. No GPU hooks or display-register stores.
 * The pad initialization byte is borrowed only during the bounded preview.
 * Original value and root ownership are checked before every restoration. */
#include "input.h"
#include "confirm.h"
#include "video.h"
#include "../../plugin/vsh_video.h"
#include <sys/file.h>
#include <sys/sys_time.h>
#include <ppu-lv2.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static int (*game_state)(void);
static int (*notify)(int,const char *);
static unsigned pad_object,pad_root,last[8],last_buttons;
static int guarded,closing,have_sample;
static hc_confirm confirm;
static uint64_t close_deadline;
static unsigned last_second;
static void (*logger)(const char *,uint32_t);
void hc_input_log(void (*log)(const char *,uint32_t)){logger=log;}
static void note(const char *name,uint32_t v){if(logger)logger(name,v);}
static int read32(uint32_t at,uint32_t *v){return vsh_video_copy(at,v,4);}
static int identity(uint32_t descriptor,uint32_t code,uint64_t expected)
{
 uint32_t words[2];unsigned char bytes[32];
 if(!descriptor||vsh_video_copy(descriptor,words,8)||words[0]!=code||words[1]!=0x6f5558||
    vsh_video_copy(code,bytes,sizeof(bytes)))return -1;
 uint64_t h=UINT64_C(14695981039346656037);
 for(unsigned i=0;i<sizeof(bytes);i++)h=(h^bytes[i])*UINT64_C(1099511628211);
 return h==expected?0:-1;
}
int hc_input_bind(void)
{
 if(vsh_video_bind())return -1;
 uint32_t state=vsh_video_export("vshmain",0xeb757101),popup=vsh_video_export("vshtask",0xa02d46e7);
 uint32_t pad=vsh_video_export("sys_io",0x3733ea3c);
 if(identity(state,0xbb908,UINT64_C(0x90bb19955adf1814))||
    identity(popup,0xdab88,UINT64_C(0xc695a54a51d634ac))||
    identity(pad,0x61aa0c,UINT64_C(0xecf3d493abaa176d)))return -1;
 uint32_t object;
 if(read32(0x6f0898,&object)||object!=0x74819c)return -1;
 game_state=(void *)(uintptr_t)state;notify=(void *)(uintptr_t)popup;pad_object=object;return 0;
}
int hc_foreground(void *u)
{
 (void)u;if(!game_state)return -1;
 if(hc_video_departed())return 1;
 if(hc_video_app_alive())return 0;
 sysFSStat s;if(!sysLv2FsStat("/dev_hdd0/tmp/240p_app.active",&s))return 1;
 return game_state()!=0;
}
static int owned(void)
{
 uint32_t object,root;
 return !read32(0x6f0898,&object)&&object==pad_object&&!read32(object+4,&root)&&root==pad_root;
}
static int hid(uint32_t port,unsigned char data[128])
{lv2syscall4(502,port,0xff,(uint64_t)(uintptr_t)data,128);return_to_user_prog(int);}
static int sample(unsigned *buttons)
{
 if(!owned())return -1;
 for(unsigned i=0;i<8;i++){
  uint32_t handle;if(read32(pad_root+0x104+i*0xe8,&handle))return -1;
  if(!handle||handle==UINT32_MAX){last[i]=0;continue;}
  unsigned char data[128]={0};int got=hid(handle,data);
  if(got<0)continue;
  if(got>128)return -1;
  if(got>=8){uint16_t digital[2];memcpy(digital,data+4,4);last[i]=digital[0]|((unsigned)digital[1]<<8);have_sample=1;}
 }
 *buttons=0;for(unsigned i=0;i<8;i++)*buttons|=last[i];last_buttons=*buttons;return 0;
}
static int open_prompt(void)
{
 if(guarded)return closing?-1:0;
 unsigned char flag;uint32_t root;
 if(!pad_object||vsh_video_copy(pad_object,&flag,1)||flag!=1||read32(pad_object+4,&root)||
    root<0x10000||root>0x7ffff000)return -1;
 pad_root=root;memset(last,0,sizeof(last));memset(&confirm,0,sizeof(confirm));have_sample=0;last_buttons=0;
 if(!owned())return -1;
 guarded=1;closing=0;last_second=0;
 *(volatile unsigned char *)(uintptr_t)pad_object=0;atomic_thread_fence(memory_order_seq_cst);
 if(vsh_video_copy(pad_object,&flag,1)||flag!=0)return -1;
 note("pad_guard_acquired",pad_object);return 0;
}
int hc_prompt(void *u,unsigned mode,unsigned seconds)
{
 if(hc_video_app_alive())return 0; /* App owns confirmation and its controller. */
 (void)u;if(open_prompt())return -1;
 unsigned buttons;if(sample(&buttons))return -1;
 if(seconds!=last_second){
  static const char *const modes[]={"","1024x768","1280x768","1280x1024","1920x1200","540p"};
  if(seconds==15||seconds%4==0){char text[160];snprintf(text,sizeof(text),"Keep HDMI %s?\nX: keep  O: restore\nRestores in %u seconds",modes[mode],seconds);notify(0,text);}
  last_second=seconds;
 }
 return have_sample?hc_confirm_sample(&confirm,buttons):0;
}
void hc_close_prompt(void *u)
{
 (void)u;if(guarded&&!closing){closing=1;close_deadline=sys_time_get_system_time()+2000000;}
}
int hc_input_cleanup(void)
{
 if(!guarded)return 1;
 if(!closing)return 0;
 unsigned buttons=last_buttons;int rc=sample(&buttons);
 if(rc||!buttons||(uint64_t)sys_time_get_system_time()>=close_deadline){
  unsigned char flag;
  if(!owned()||vsh_video_copy(pad_object,&flag,1)||flag>1)return -1;
  if(!flag){*(volatile unsigned char *)(uintptr_t)pad_object=1;atomic_thread_fence(memory_order_seq_cst);}
  if(vsh_video_copy(pad_object,&flag,1)||flag!=1)return -1;
  guarded=closing=0;note("pad_guard_restored",pad_object);return 1;
 }
 return 0;
}
