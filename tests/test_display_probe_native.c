#include "../experiments/display_probe/native.h"
#include "../plugin/vsh_video.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static vsh_video_state current;
static uint32_t masks[2],debug_type,manager,objects[2];
static unsigned writes,updates;
static int blocked,identity_error,partial_failure,map_error;
static uint32_t word(uint32_t at)
{
 if(at==0x738bf4)return manager;
 if(at==manager+0x24)return objects[0];if(at==manager+0x28)return 0;if(at==manager+0x34)return objects[1];
 if(at==objects[0])return 0x6ca380;if(at==objects[1])return 0x6ca268;
 if(at==0x6ca380+0x40)return 0x6e3ed8;if(at==0x6ca268+0x40)return 0x6e3d08;
 if(at==0x6e3ed8)return 0x47cb8c;if(at==0x6e3d08)return 0x4740c4;
 if(at==objects[0]+0x6c)return masks[0];if(at==objects[1]+0x9c)return masks[1];
 if(at==0x70f13c)return debug_type;
 if(at==0x100000+0x10)return 0x6e33c0;if(at==0x6e33c0)return 0x455558;
 assert(!"unexpected VSH read");return 0;
}
int vsh_video_copy(uint32_t at,void *out,unsigned n){assert(n==4);uint32_t v=word(at);memcpy(out,&v,4);return 0;}
uint32_t vsh_video_export(const char *name,uint32_t nid){(void)name;assert(nid==0x4712f276||nid==0xd8af2727||nid==0x3aecdd23);return 0x123456;}
int vsh_video_bind(void){return 0;}
int vsh_video_query(vsh_video_state *s){*s=current;return 0;}
int vsh_video_hd720(uint32_t *f,uint32_t *m){*f=current.flags;*m=11;return 0;}
int dp_mock_flag(const char *path){(void)path;return blocked;}
uint64_t dp_mock_clock(void){return 1234;}
int dp_mock_identity(uint32_t at,unsigned n,uint64_t h){assert(at&&n&&h);return identity_error;}
uint32_t dp_mock_table(void){return 0x100000;}
int dp_mock_info(unsigned a,unsigned b,void *out){assert(!a&&!b);memset(out,0,0x1cc);return 0;}
int dp_mock_map(uint32_t *flags,uint32_t *mode,int res,int asp,unsigned refresh)
{assert(res==(debug_type==13?1:2)&&asp==2&&!refresh);*flags=0x40210;*mode=debug_type==13?12:11;return map_error?-1:2;}
int dp_mock_update(unsigned a,uint32_t flags,uint32_t mode,uint32_t fmt,uint32_t pitch,void *option,int wait)
{
 assert(!a&&!option&&wait==1);assert(flags&0x10000000u);assert(mode==11||mode==12);
 assert(fmt==5&&(pitch==8192||pitch==5120));updates++;current.mode=mode;current.pitch=mode==11?5120:8192;
 current.flags=flags&~0x10000000u;current.raw[8]=mode==11?2:1;current.raw[9]=2;
 current.raw[10]=(flags&0xff)==0x10?(masks[0]==0x40000000u?1:masks[0]==0x20000000u?2:3):6;
 if((flags&0xff)==0x10)debug_type=0; /* Native configuration consumes its request flag. */
 return 0;
}
int dp_mock_hdmi(uint32_t p,uint32_t m,unsigned reserved){assert(p==objects[0]&&!reserved);masks[0]=m;writes++;return 0;}
int dp_mock_analog(uint32_t p,uint32_t m){assert(p==objects[1]);masks[1]=m;writes++;return partial_failure&&m==0x40000000u?-1:0;}
int dp_mock_global(uint32_t d){debug_type=d;writes++;return 0;}
static void reset(void)
{
 memset(&current,0,sizeof(current));current.mode=12;current.flags=0x1010205;current.format=5;current.pitch=8192;
 current.raw[8]=1;current.raw[9]=2;current.raw[10]=6;current.raw[11]=2;
 masks[0]=0x6068;masks[1]=0x101;debug_type=0;manager=0x20000000;objects[0]=0x20001000;objects[1]=0x20002000;
 writes=updates=0;blocked=identity_error=partial_failure=map_error=0;
}
int main(void)
{
 const dp_ops *o=dp_native_ops();reset();assert(!o->prepare(0,1));assert(!writes&&!updates);
 assert(!o->stock(0)&&updates==1&&o->at_stock(0)==1);
 assert(!o->candidate(0)&&updates==2&&writes==3&&masks[0]==0x40000000&&debug_type==0);
 assert((current.flags&0x003fc0ff)==0x40010);
 assert(o->at_target(0,DP_WXGA)==1);
 assert(!o->restore(0)&&o->at_original(0)==1&&masks[0]==0x6068&&masks[1]==0x101&&debug_type==0);
 for(unsigned preset=2;preset<=3;preset++){
  reset();assert(!o->prepare(0,preset));assert(!o->stock(0)&&o->at_stock(0)==1);
  assert(!o->candidate(0)&&o->at_target(0,DP_WXGA)==1);
  assert(current.raw[10]==preset&&masks[0]==(0x80000000u>>preset));
  assert(!o->restore(0)&&o->at_original(0)==1&&masks[0]==0x6068&&masks[1]==0x101);
 }
 reset();assert(o->prepare(0,4)==-1&&!writes&&!updates);
 /* Failed second setter mutates first: undo exact nonzero defaults, not zero. */
 reset();assert(!o->prepare(0,1));partial_failure=1;assert(o->caps(0)==-1&&writes==2);
 assert(!o->restore(0)&&o->at_original(0)==1&&masks[0]==0x6068&&masks[1]==0x101);
 reset();assert(!o->prepare(0,1));assert(!o->stock(0)&&o->at_stock(0)==1);map_error=1;
 assert(o->candidate(0)==-1&&updates==1);assert(!o->restore(0)&&o->at_original(0)==1);
 reset();current.pitch=7680;assert(o->prepare(0,1)==1&&!writes&&!updates);
 reset();current.format=7;assert(o->prepare(0,1)==1&&!writes&&!updates);
 reset();current.state=2;assert(o->prepare(0,1)==1&&!writes&&!updates);
 reset();blocked=1;assert(o->prepare(0,1)==-1&&!writes&&!updates);
 reset();identity_error=1;assert(o->prepare(0,1)==-1&&!writes&&!updates);
 reset();current.flags=0x1010202;assert(o->prepare(0,1)==-1&&!writes&&!updates);
 reset();current.raw[9]=1;assert(o->prepare(0,1)==-1&&!writes&&!updates);
 reset();debug_type=11;assert(o->prepare(0,1)==-1&&!writes&&!updates);
 /* External ownership changes must prevent native writes. */
 reset();assert(!o->prepare(0,1));manager+=0x100;assert(o->caps(0)==-1&&!writes&&!updates);
 reset();assert(!o->prepare(0,1));masks[0]^=1;assert(o->caps(0)==-1&&!writes&&!updates);
 puts("PASS: native display source bounds, exact nonzero capability rollback, partial native failure, connector/scan/debug/identity refusal and ownership checks");
 return 0;
}
