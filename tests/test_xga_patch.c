#include "../experiments/display_probe/xga_patch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t memory[XGA_WORDS];
static unsigned writes,fail_at;
static int map_bad,stock_bad,cache_bad;
static unsigned source_w,source_h;
static uint64_t read_word(void *u,uint64_t at){(void)u;assert(at>=0x1000&&at<0x1000+8*XGA_WORDS);return memory[(at-0x1000)/8];}
static void write_word(void *u,uint64_t at,uint64_t value){(void)u;writes++;if(writes!=fail_at)memory[(at-0x1000)/8]=value;}
int patch_map_targets(patch_context *c,const uint32_t *va,uint64_t *pa,unsigned count)
{(void)c;assert(count==XGA_WORDS);for(unsigned i=0;i<count;i++){assert(va[i]>=0x80000000);pa[i]=0x1000+8*i;}return map_bad?-1:0;}
int patch_state(patch_context *c,int family){(void)c;assert(family==0||family==1);return stock_bad?STATE_MIXED:STATE_STOCK;}
int patch_read_cached_video(patch_context *c,patch_cached_video *v){(void)c;memset(v,0,sizeof(*v));v->head[0].state=2;v->head[0].vid=0xd;v->head[0].width=source_w;v->head[0].height=source_h;memcpy(v->head[0].raster,memory+3,sizeof(v->head[0].raster));return cache_bad?-1:0;}
static patch_context context;
static xga_patch patch;
static void reset(unsigned geometry)
{
 for(unsigned i=0;i<XGA_WORDS;i++)memory[i]=UINT64_C(0x6000000060000000);
 memset(&context,0,sizeof(context));context.ready=1;context.read=read_word;context.write=write_word;
 memset(&patch,0,sizeof(patch));writes=fail_at=map_bad=stock_bad=cache_bad=0;
 source_w=geometry==5?1024:geometry>=3?720:1280;source_h=geometry==5?768:geometry>=3?480:720;
 assert(!xga_patch_resolve(&patch,&context,geometry)&&!writes);
}
static unsigned imm(uint64_t w,int hi){return (unsigned)(hi?w>>32:w)&65535;}
int main(void)
{
 for(unsigned mode=0;mode<6;mode++){
  reset(mode);assert((patch.candidate[3]>>32)==1023);
  assert(patch.candidate[0]==patch.original[0]&&patch.candidate[2]==patch.original[2]);
  assert(mode?(uint32_t)patch.candidate[1]==1024:patch.candidate[1]==patch.original[1]);
  if(mode==5){
   for(unsigned i=10;i<XGA_WORDS;i++)assert(patch.candidate[i]==patch.original[i]);
   assert(patch.candidate[9]==patch.original[9]);
  }else if(mode){
   int aspect=mode==2||mode==4;
   unsigned xs=mode>=3?0xb40:0x1400,ys=mode>=3?(aspect?0xd55:0xa00):(aspect?0x1400:0xf00);
   assert(imm(patch.candidate[13],0)==(aspect?96u:0u));
   assert(imm(patch.candidate[14],1)==(aspect?671u:767u));
   assert(imm(patch.candidate[16],1)==1023);
   assert(imm(patch.candidate[10],0)==xs);
   assert(imm(patch.candidate[19],1)==xs);
   assert((imm(patch.candidate[12],0)&0x55)==0x11);
   assert(imm(patch.candidate[17],0)==ys);
   if(mode<3){
    uint32_t compare=(uint32_t)(patch.candidate[18]>>32);
    assert(compare>>26==10&&((compare>>23)&7)==7&&((compare>>16)&31)==11);
    assert((compare&65535)==1280);
   }else assert(patch.candidate[18]==patch.original[18]); /* Native source limits retained. */
  }
  assert(!xga_patch_apply(&patch)&&patch.dirty&&xga_patch_cache(&patch,1)==1);
  assert(xga_patch_apply(&patch)==-1);
  assert(!xga_patch_restore(&patch)&&!patch.dirty&&!memcmp(memory,patch.original,sizeof(memory)));
  /* Every failed candidate store requests exact restoration. */
  unsigned changed=0;for(unsigned i=0;i<XGA_WORDS;i++)changed+=patch.original[i]!=patch.candidate[i];
  for(unsigned failure=1;failure<=changed;failure++){
   reset(mode);fail_at=failure;assert(xga_patch_apply(&patch)==-1);
   assert(!patch.dirty&&!memcmp(memory,patch.original,sizeof(memory)));
  }
 }
 reset(2);memory[5]^=1;assert(xga_patch_apply(&patch)==-1&&!writes);
 reset(1);map_bad=1;assert(xga_patch_apply(&patch)==-1&&!writes);
 reset(2);assert(!xga_patch_apply(&patch));memory[16]^=1;
 assert(xga_patch_restore(&patch)==-1&&patch.dirty);
 memory[16]=patch.candidate[16];assert(!xga_patch_restore(&patch)&&!patch.dirty);
 reset(0);patch.ready=0;memory[10]^=1;assert(xga_patch_resolve(&patch,&context,0)==-1&&!writes);
 reset(0);patch.ready=0;stock_bad=1;assert(xga_patch_resolve(&patch,&context,0)==-1&&!writes);
 puts("PASS: XGA fixed-clock numeric scope, authored viewport coefficients, every failed write restores, mappings/identity/external changes refuse");
 return 0;
}
