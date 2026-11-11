#include "lv1_patch.h"
#include "switch_flow.h"
#include "timing_payloads.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char ram[0x02000000];
static int stores,drop_store,video_calls,fail_video_call,unavailable;
static unsigned calls[16];
static patch_context c;
static int cache_unstable,cache_reads;
static void put64(uint64_t a,uint64_t v)
{assert(a+8<=sizeof(ram));for(int i=7;i>=0;i--){ram[a+i]=(unsigned char)v;v>>=8;}}
static void put32(uint64_t a,uint32_t v)
{for(int i=3;i>=0;i--){ram[a+i]=(unsigned char)v;v>>=8;}}
static uint64_t get(void *u,uint64_t a)
{(void)u;assert(!(a&7)&&a>=0x1000&&a+8<=sizeof(ram));uint64_t v=0;for(int i=0;i<8;i++)v=(v<<8)|ram[a+i];if(cache_unstable&&a==c.video_address[26])v^=(uint64_t)(++cache_reads&1);return v;}
static void set(void *u,uint64_t a,uint64_t v)
{(void)u;assert(!(a&7)&&a+8<=sizeof(ram));stores++;if(stores!=drop_store)put64(a,v);}
/* Deliberately unrelated physical pages. Both text target pages are
 * far from the historically observed addresses and from each other. */
static uint64_t physical(uint32_t va)
{
 if((va&0xfffff000u)==0x80039000u)return 0x1800000+(va&0xfff);
 if((va&0xfffff000u)==0x80038000u)return 0x1250000+(va&0xfff);
 if((va&0xfffff000u)==0xc0000000u)return 0x1a30000+(va&0xfff);
 if((va&0xfffff000u)==0xc000b000u)return 0x1b00000+(va&0xfff);
 if((va&0xfffff000u)==0xc000d000u)return 0x1e00000+(va&0xfff);
 if((va&0xfffff000u)==0xa0023000u)return 0x1300000+(va&0xfff);
 if((va&0xfffff000u)==0xa0024000u)return 0x1600000+(va&0xfff);
 abort();
}
static void setup(void)
{
 memset(ram,0,sizeof(ram));memset(&c,0,sizeof(c));c.read=get;c.write=set;
 stores=drop_store=video_calls=fail_video_call=unavailable=0;
 cache_unstable=cache_reads=0;
 put64(0x36cdb0+10*16+8,0x120000);put32(0x120000,10);put64(0x120018,0x121000);
 put64(0x121000,0x35ecb8);put32(0x12103c,0x122000);
 unsigned vas[]={0x80039000,0x80038000,0xc0000000,0xc000b000,0xc000d000,0xa0023000,0xa0024000};
 for(int i=0;i<7;i++){
  uint64_t node=0x122000+i*0x20;
  put64(node,physical(vas[i]));put32(node+8,vas[i]);
  put32(node+0x14,i==6?0x121028:(uint32_t)node+0x20);ram[node+0x18]=12;
 }
 /* First resolve populates the target plan, then rejects empty bytes. */
 assert(patch_resolve(&c)==-1);
 for(int i=0;i<PATCH_WORDS;i++)put64(c.address[i],c.stock[i]);
 put64(physical(0xc000b440),0xc000d4c8);
 put64(physical(0xc000d4c8),0xa0023f40);
 for(int i=0;i<7;i++)put64(physical(0xa0023f88+i*8),g_stock_288p_raster[i]);
 put64(physical(0xa0024000),UINT64_C(3)<<32|720);
 put64(physical(0xa0024008),UINT64_C(576)<<32|2880);
 put64(physical(0xa0024168),UINT64_C(2)<<32);
 assert(!patch_resolve(&c));assert(!stores);
 /* Independent address/alias/field checks catch profile-selection bugs. */
 assert(c.address[0]==physical(0x80039160));
 assert(c.address[10]==physical(0x80039178));
 assert(c.address[20]==physical(0x80039190));
 assert(c.address[30]==physical(0x800391a8));
 assert(c.address[33]==physical(0x80038c08));
 assert(c.stock[34]==UINT64_C(0x00000b6f00000c6b));
 assert(c.stock[37]==UINT64_C(0x0000024a052d04af));
 assert(c.stock[38]==UINT64_C(0x04af03b3024a0244));
 assert((c.candidate[29]&0xffffffffu)==0); /* Preserve PAL profile 3 routing. */
 assert(c.candidate[14]==c.stock[14]); /* Preserve NTSC alias placement. */
}
static void is_stock(void)
{for(int i=0;i<PATCH_WORDS;i++)assert(get(NULL,c.address[i])==c.stock[i]);}
static int avail(void *u,unsigned id){(void)u;assert(id==4||id==5);return unavailable?unavailable:1;}
static int video(void *u,unsigned id)
{(void)u;assert(video_calls<16);calls[video_calls++]=id;return video_calls==fail_video_call?-1:0;}
static switch_flow flow(void)
{switch_flow f={0};f.patch=&c;f.available=avail;f.configure=video;return f;}
int main(void)
{
 setup();assert(patch_state(&c,0)==STATE_STOCK&&patch_state(&c,1)==STATE_STOCK);
 patch_cached_video snapshot;
 assert(c.video_ready&&!patch_read_cached_video(&c,&snapshot)&&!stores);
 assert(snapshot.head[0].state==2&&snapshot.head[1].state==0);
 assert(snapshot.head[0].vid==3&&snapshot.head[0].width==720&&snapshot.head[0].height==576&&snapshot.head[0].pitch==2880);
 assert(snapshot.head[0].active_lines==576&&snapshot.head[0].total_lines==625&&snapshot.head[0].scan_control==0x30000);
 assert(!patch_candidate_in_cache(&c,&snapshot,1));
 assert(!patch_snapshot(&c));assert(!patch_apply(&c,1));
 assert(patch_state(&c,1)==STATE_PATCHED&&patch_state(&c,0)==STATE_STOCK);
 /* Table writes alone must NOT be classified as AV activation. */
 assert(!patch_read_cached_video(&c,&snapshot)&&!patch_candidate_in_cache(&c,&snapshot,1));
 for(int i=0;i<7;i++)put64(c.video_address[2+i],c.candidate[33+i]);
 assert(!patch_read_cached_video(&c,&snapshot)&&patch_candidate_in_cache(&c,&snapshot,1));
 assert(!patch_candidate_in_cache(&c,&snapshot,0));
 assert(!patch_apply(&c,0));assert(patch_state(&c,1)==STATE_STOCK&&patch_state(&c,0)==STATE_PATCHED);
 assert(!patch_undo(&c));is_stock();
 setup();put64(c.video_address[0],0xc000cee8);assert(patch_read_cached_video(&c,&snapshot)==-1&&!stores);
 setup();put64(c.video_address[1],0xa0023f48);assert(patch_read_cached_video(&c,&snapshot)==-1&&!stores);
 setup();put64(c.video_address[1],UINT64_C(0x100000000));assert(!patch_resolve(&c)&&!c.video_ready&&!stores);
 setup();put64(c.video_address[1],0xc000cee8);assert(!patch_resolve(&c)&&!c.video_ready&&!stores);
 setup();cache_unstable=1;assert(patch_read_cached_video(&c,&snapshot)==-1&&!stores);
 setup();put64(c.video_address[26],UINT64_C(3)<<32);assert(patch_read_cached_video(&c,&snapshot)==-1&&!stores);
 setup();put64(c.video_address[4],UINT64_C(0x000002700000023f));assert(patch_read_cached_video(&c,&snapshot)==-1&&!stores);
 setup();c.video_ready=0;assert(patch_read_cached_video(&c,&snapshot)==-1&&!stores);
 setup();assert(!patch_read_cached_video(&c,&snapshot));
 for(int i=0;i<7;i++)snapshot.head[1].raster[i]=c.candidate[33+i];
 assert(!patch_candidate_in_cache(&c,&snapshot,1)); /* Inactive head cannot prove activation. */
 snapshot.head[1].state=1;assert(!patch_candidate_in_cache(&c,&snapshot,1)); /* Only state 2 is configured. */
 setup();assert(patch_apply(&c,7)==-1&&!stores);
 /* A mismatch in the LAST encoder target must prevent every earlier store. */
 setup();put64(c.address[41],1);assert(patch_apply(&c,1)==-1&&!stores);
 setup();put64(c.address[39],1);assert(patch_restore(&c)==-1&&!stores);
 setup();put64(c.address[0],c.candidate[0]);assert(patch_state(&c,0)==STATE_MIXED);
 assert(!patch_restore(&c));is_stock();
 setup();assert(!patch_apply(&c,1));int changed=stores;assert(changed>8);
 for(int i=1;i<=changed;i++){
  setup();drop_store=i;assert(patch_apply(&c,1)==-1);is_stock();
 }
 setup();put32(0x120000,3);assert(patch_resolve(&c)==-1&&!stores);
 setup();put64(0x121000,0x3602b8);assert(patch_resolve(&c)==-1&&!stores);
 setup();put32(0x122054,0x122000);assert(patch_resolve(&c)==-1&&!stores);
 setup();put64(0x122000,UINT64_MAX-4095);assert(patch_resolve(&c)==-1&&!stores);
 setup();put32(0x122008,0x80037000);assert(patch_resolve(&c)==-1&&!stores);
 setup();ram[0x122018]=63;assert(patch_resolve(&c)==-1&&!stores);
 setup();put32(0x122028,0x80039000);assert(patch_resolve(&c)==-1&&!stores);
 setup();put64(0x36cdb0+10*16+8,UINT64_MAX-7);assert(patch_resolve(&c)==-1&&!stores);
 setup();switch_flow f=flow();assert(switch_begin(&f,1,5,0)==-1&&!stores&&!video_calls);
 setup();f=flow();unavailable=0;assert(switch_begin(&f,1,2,1)==-1&&!stores&&!video_calls);
 setup();f=flow();unavailable=-1;assert(switch_begin(&f,1,5,1)==-1&&!stores&&!video_calls);
 setup();f=flow();assert(!switch_begin(&f,1,5,1));
 assert(f.pending&&video_calls==2&&calls[0]==4&&calls[1]==5&&patch_state(&c,1)==STATE_PATCHED);
 assert(switch_begin(&f,0,5,1)==-1);assert(!switch_revert(&f));
 assert(!f.pending&&video_calls==4&&calls[2]==4&&calls[3]==5);is_stock();
 setup();f=flow();assert(!switch_begin(&f,0,5,1));switch_keep(&f);
 assert(!f.pending&&patch_state(&c,0)==STATE_PATCHED);assert(!switch_stock(&f,4,1));is_stock();
 for(int i=1;i<=2;i++){
  setup();f=flow();fail_video_call=i;assert(switch_begin(&f,1,5,1)==-1&&!f.pending);is_stock();
 }
 setup();f=flow();assert(!switch_begin(&f,1,5,1));fail_video_call=4;
 assert(switch_revert(&f)==-1&&f.pending);is_stock();
 puts("PASS: live page resolution, both SD aliases, full preflight, every failed PAL store rollback, checked video transitions and recovery");
 return 0;
}
