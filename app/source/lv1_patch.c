#include "lv1_patch.h"
#include "timing_payloads.h"
#include <stdio.h>
#include <string.h>

/* 4.93 layout evidence: docs/LV1_SYSMGR_SS_LOADADDR.md. Physical pages can
 * move every boot. Never assume the text/data segments are contiguous. */
#define PROCESS_TABLE UINT64_C(0x36cdb0)
#define DOMAIN_VTABLE UINT64_C(0x35ecb8)
#define VIDEO_CONTEXT_POINTER 0xc000b440u
#define VIDEO_CONTEXT_HOLDER 0xc000d4c8u
typedef struct {uint64_t physical,size;uint32_t virtual_address;} mapping;
static int bounded(uint64_t a,size_t n)
{return a>=0x1000 && n<=LV1_HEAP_LIMIT && a<=LV1_HEAP_LIMIT-n;}
static int fail(patch_context *c,const char *why)
{snprintf(c->error,sizeof(c->error),"%s",why);if(c->log)c->log("PATCH ERROR: %s",why);return -1;}
static uint64_t read64(patch_context *c,uint64_t a)
{return bounded(a,8)&&!(a&7)?c->read(c->user,a):0;}
static uint32_t read32(patch_context *c,uint64_t a)
{uint64_t v=read64(c,a&~UINT64_C(7));return (uint32_t)(a&4?v:v>>32);}
static int translate(const mapping *m,int count,uint32_t va,uint64_t *pa)
{
 int found=0;
 for(int i=0;i<count;i++)if(va>=m[i].virtual_address &&
     (uint64_t)va-m[i].virtual_address+8<=m[i].size){
  if(found)return -1;
  if(m[i].size>LV1_HEAP_LIMIT||!bounded(m[i].physical,(size_t)m[i].size))return -1;
  *pa=m[i].physical+((uint64_t)va-m[i].virtual_address);found=1;
 }
 return found&&bounded(*pa,8)&&!(*pa&7)?0:-1;
}
/* NTSC profile 2 has a different horizontal placement. PAL profile 3 also
 * has different sync edges/output control. Preserve each profile's fields. */
static const uint64_t ntsc2[7]={
 UINT64_C(0x00000b3f00000d67),UINT64_C(0x00000b8b00000c83),
 UINT64_C(0x000001df0000020c),UINT64_C(0x000001e9000001e8),
 UINT64_C(0x000001ee055304d7),UINT64_C(0x04d703df01f401e2),
 UINT64_C(0x0003000011000000)};
static const uint64_t pal3[7]={
 UINT64_C(0x00000b3f00000d7f),UINT64_C(0x00000b6f00000c6d),
 UINT64_C(0x0000023f00000270),UINT64_C(0x0000024500000244),
 UINT64_C(0x00000249052e04af),UINT64_C(0x04af03b1024e023f),
 UINT64_C(0x0003000000000000)};

static int service_pages(patch_context *c,mapping pages[512],int *out_count)
{
 int count=0;
 if(!c->read)return fail(c,"Missing LV1 transport");
 uint64_t proc=read64(c,PROCESS_TABLE+10*16+8);
 if(!bounded(proc,0x28)||(proc&7)||read32(c,proc)!=10)
  return fail(c,"4.93 sysmgr PID 10 not found; check CFW syscalls");
 uint64_t domain=read64(c,proc+0x18);
 if(!bounded(domain,0x40)||(domain&7)||read64(c,domain)!=DOMAIN_VTABLE)
  return fail(c,"4.93 protection-domain identity mismatch");
 uint64_t sentinel=domain+0x28,node=read32(c,domain+0x3c);
 uint64_t visited[512];
 while(node!=sentinel){
  if(count==512||!bounded(node,0x20)||(node&7))return fail(c,"Invalid protection-page list");
  for(int i=0;i<count;i++)if(visited[i]==node)return fail(c,"Protection-page list cycle");
  visited[count]=node;
  uint64_t physical=read64(c,node);
  uint32_t va=read32(c,node+8);
  unsigned shift=(unsigned)(read64(c,node+0x18)>>56);
  /* Only ordinary RAM mappings are relevant; MMIO is never read/written. */
  if(shift<12||shift>32)return fail(c,"Unexpected protection-page size");
  pages[count].physical=physical;pages[count].virtual_address=va;
  pages[count].size=UINT64_C(1)<<shift;
  count++;node=read32(c,node+0x14);
 }
 if(!count)return fail(c,"Empty protection-page list");
 *out_count=count;return 0;
}
int patch_map_targets(patch_context *c,const uint32_t *va,uint64_t *pa,unsigned n)
{
 mapping pages[512];int count=0;
 if(!c||!va||!pa||!n||n>512)return -1;
 if(service_pages(c,pages,&count))return -1;
 for(unsigned i=0;i<n;i++){
  if(translate(pages,count,va[i],&pa[i]))return fail(c,"Service RAM page unavailable/ambiguous");
  for(unsigned j=0;j<i;j++)if(pa[j]==pa[i])return fail(c,"Aliased service RAM targets");
 }
 return 0;
}
int patch_resolve(patch_context *c)
{
 mapping pages[512];int count=0,index=0;
 c->ready=c->have_saved=c->video_ready=0;c->video_context=0;c->error[0]=0;
 if(!c->write)return fail(c,"Missing LV1 transport");
 if(service_pages(c,pages,&count))return -1;
 for(int profile=1;profile<=4;profile++){
  int pal=profile>=3;
  const uint64_t *meta=pal?g_stock_288p_metadata:g_stock_240p_metadata;
  const uint64_t *raster=profile==1?g_stock_240p_raster:profile==2?ntsc2:profile==3?pal3:g_stock_288p_raster;
  const uint64_t *candidate_meta=pal?g_patch_288p_metadata:g_patch_240p_metadata;
  const uint64_t *candidate_raster=pal?g_patch_288p_raster:g_patch_240p_raster;
  for(int j=0;j<10;j++,index++){
   uint32_t va=j<3?0x80039148u+profile*24u+j*8u:0x80038b28u+profile*56u+(j-3)*8u;
   if(translate(pages,count,va,&c->address[index]))return fail(c,"Timing page unavailable/ambiguous");
   c->stock[index]=j<3?meta[j]:raster[j-3];
   uint64_t value=j<3?candidate_meta[j]:raster[j-3];
   if(j>=3&&j<=9){
    int w=j-3;uint64_t repl=candidate_raster[w];
    if(w==1&&(profile==2||profile==3))value=raster[1];
    else if(w==4)value=(value&UINT64_C(0xffffffff))|(repl&UINT64_C(0xffffffff00000000));
    else if(w==5)value=(value&UINT64_C(0xffffffff00000000))|(repl&UINT64_C(0xffffffff));
    else if(w==6)value=(value&UINT64_C(0xffffffff))|(repl&UINT64_C(0xffffffff00000000));
    else value=repl;
   }
   c->candidate[index]=value;c->family[index]=(unsigned)pal;
  }
 }
 for(int family=0;family<2;family++,index++){
  if(translate(pages,count,family?0xc0000050u:0xc0000010u,&c->address[index]))
   return fail(c,"Encoder page unavailable/ambiguous");
  c->stock[index]=family?g_stock_288p_encoder[0]:g_stock_240p_encoder[0];
  c->candidate[index]=family?g_patch_288p_encoder[0]:g_patch_240p_encoder[0];
  c->family[index]=(unsigned)family;
 }
 /* Flag-gated scaler experiment. Verified instruction locations do not
  * prove register semantics or execution; see timing_payloads.h and
  * docs/GEOMETRY_STATUS_2026-11-13.md. Unknown live code is refused. */
 struct {uint32_t va;uint64_t stock;uint64_t patch;const char *name;} code[2]={
  {VSCALE_ENABLE_VA,STOCK_VSCALE_ENABLE_WORD,PATCH_VSCALE_ENABLE_WORD,"vertical scaler enable"},
  {VSCALE_FACTOR_VA,STOCK_VSCALE_FACTOR_WORD,PATCH_VSCALE_FACTOR_WORD,"vertical scale factor"}};
 for(int k=0;k<2;k++){
  if(translate(pages,count,code[k].va,&c->address[index]))
   return fail(c,"Vertical scaler code page unavailable/ambiguous");
  uint64_t live=read64(c,c->address[index]);
  if(live==code[k].stock){
   c->stock[index]=live;
   c->candidate[index]=c->scaler_mode?code[k].patch:live;
  }else if(live==code[k].patch){
   c->stock[index]=code[k].stock;c->candidate[index]=code[k].patch;
  }else{
   if(c->scaler_mode)return fail(c,"Vertical scaler instruction not recognised");
   c->stock[index]=live;c->candidate[index]=live;
  }
  c->family[index]=2;
  index++;
  if(c->log)c->log("%s word %016llx -> %016llx",code[k].name,
   (unsigned long long)live,(unsigned long long)c->candidate[index-1]);
 }
 /* Analog head 1 / PAL profile 4 uses a distinct 574-line special record,
  * not the ordinary profile table. Verify both pointer identities before
  * including its ten data words in the same transaction/rollback. */
 uint64_t ptr_raster,ptr_meta;
 if(translate(pages,count,0xc000af68u,&ptr_raster)||
    translate(pages,count,0xc000af70u,&ptr_meta)||
    read64(c,ptr_raster)!=UINT64_C(0x80038ad8)||
    read64(c,ptr_meta)!=UINT64_C(0x80038b10))
  return fail(c,"PAL special timing pointer identity mismatch");
 for(int j=0;j<10;j++,index++){
  uint32_t va=j<3?0x80038b10u+j*8u:0x80038ad8u+(j-3)*8u;
  if(translate(pages,count,va,&c->address[index]))return fail(c,"PAL special timing page unavailable");
  c->stock[index]=j<3?g_stock_pal_special_metadata[j]:g_stock_pal_special_raster[j-3];
  c->candidate[index]=j<3?g_patch_288p_metadata[j]:g_patch_288p_raster[j-3];
  c->family[index]=MODE_288P_PAL;
 }
 for(int i=0;i<PATCH_WORDS;i++)for(int j=0;j<i;j++)if(c->address[i]==c->address[j])
  return fail(c,"Aliased timing targets");
 c->ready=1;
 if(patch_state(c,0)==STATE_UNKNOWN||patch_state(c,1)==STATE_UNKNOWN){
  c->ready=0;return fail(c,"Timing bytes differ from verified 4.93 data; reboot first");
 }
 /* Ordinary metadata profiles; PAL head 1 can instead use the special
  * record verified above. These globals hold pointers, not descriptors. */
 for(int p=0;p<METADATA_PROBE_PROFILES;p++)for(int w=0;w<3;w++)
  c->metadata_probe[p][w]=read64(c,c->address[p*10+w]);
 /* Read-only AV-service cache, independently mapped at each word. These
  * offsets are verified in the same 4.93 sysmgr image as the timing data.
  * No mapped MMIO is read, and no physical base is assumed contiguous. */
 if(!translate(pages,count,VIDEO_CONTEXT_POINTER,&c->video_address[0])&&
    read64(c,c->video_address[0])==VIDEO_CONTEXT_HOLDER&&
    !translate(pages,count,VIDEO_CONTEXT_HOLDER,&c->video_address[1])){
  uint64_t context=read64(c,c->video_address[1]);
  if(context<=UINT32_MAX-0x230u && context>=0x1000 && !(context&7)){
   c->video_context=(uint32_t)context;c->video_ready=1;
  }
 }
 if(c->video_ready){
  for(int head=0;head<2;head++){
   for(int i=0;i<7;i++)if(translate(pages,count,c->video_context+0x48u+head*56u+i*8u,
       &c->video_address[2+head*7+i]))c->video_ready=0;
   for(int i=0;i<5;i++)if(translate(pages,count,c->video_context+0xb8u+head*40u+i*8u,
       &c->video_address[16+head*5+i]))c->video_ready=0;
  }
  if(translate(pages,count,c->video_context+0x228u,&c->video_address[26]))c->video_ready=0;
 }
 if(c->log)c->log("Resolved %d targets across %d pages; meta1=%llx raster1=%llx meta4=%llx raster4=%llx encoder=%llx/%llx",
  index,count,(unsigned long long)c->address[0],(unsigned long long)c->address[3],
  (unsigned long long)c->address[30],(unsigned long long)c->address[33],
  (unsigned long long)c->address[40],(unsigned long long)c->address[41]);
 return 0;
}
int patch_read_cached_video(patch_context *c,patch_cached_video *out)
{
 uint64_t first[VIDEO_PROBE_WORDS],second[VIDEO_PROBE_WORDS];
 if(!out)return -1;
 memset(out,0,sizeof(*out));
 if(!c->ready||!c->video_ready){snprintf(out->error,sizeof(out->error),"AV cache pages unavailable");return -1;}
 for(int attempt=0;attempt<3;attempt++){
  for(int i=0;i<VIDEO_PROBE_WORDS;i++)first[i]=read64(c,c->video_address[i]);
  for(int i=0;i<VIDEO_PROBE_WORDS;i++)second[i]=read64(c,c->video_address[i]);
  if(memcmp(first,second,sizeof(first)))continue;
  if(first[0]!=VIDEO_CONTEXT_HOLDER||first[1]!=c->video_context){
   snprintf(out->error,sizeof(out->error),"4.93 AV context identity changed; inspect again");return -1;
  }
  for(int h=0;h<2;h++){
   patch_cached_head *head=&out->head[h];
   memcpy(head->raster,first+2+h*7,sizeof(head->raster));
   const uint64_t *request=first+16+h*5;
   head->state=(uint32_t)(h?first[26]:first[26]>>32);
   head->vid=(uint32_t)(request[1]>>32);
   head->width=(uint32_t)request[1]&0xffffu;
   head->height=(uint32_t)(request[2]>>32)&0xffffu;
   head->pitch=(uint32_t)request[2];
   head->active_lines=((uint32_t)(head->raster[2]>>32)&0xffffu)+1;
   head->total_lines=((uint32_t)head->raster[2]&0xffffu)+1;
   head->scan_control=(uint32_t)(head->raster[6]>>32);
   if(head->state>2 || (head->state==2 && (!head->vid||head->vid>0x2a||
       !head->width||!head->height||!head->pitch||head->active_lines>head->total_lines))){
    snprintf(out->error,sizeof(out->error),"AV cache fields inconsistent; snapshot refused");return -1;
   }
   if(c->log)c->log("AV CACHE head=%d state=%u VID=%x surface=%ux%u pitch=%u active=%u total=%u scan_control=%08x (software cache only)",
    h,head->state,head->vid,head->width,head->height,head->pitch,head->active_lines,head->total_lines,head->scan_control);
  }
  return 0;
 }
 snprintf(out->error,sizeof(out->error),"AV cache changed during read; snapshot refused");return -1;
}
int patch_candidate_in_cache(const patch_context *c,const patch_cached_video *video,int family)
{return patch_candidate_on_head(c,video,family,0)||patch_candidate_on_head(c,video,family,1);}
int patch_candidate_on_head(const patch_context *c,const patch_cached_video *video,int family,int head)
{
 if(!c||!video||!c->ready||video->error[0]||family<0||family>1||head<0||head>1)return 0;
 int first_profile=family?2:0;
 if(video->head[head].state==2){
  if(family==MODE_288P_PAL&&!memcmp(video->head[head].raster,c->candidate+PAL_SPECIAL_RASTER_INDEX,7*sizeof(uint64_t)))return 1;
  for(int p=first_profile;p<first_profile+2;p++)
   if(!memcmp(video->head[head].raster,c->candidate+p*10+3,7*sizeof(uint64_t)))return 1;
 }
 return 0;
}
int patch_state(patch_context *c,int family)
{
 int fam_stock=1,fam_patched=1,shared_patched=1;
 if(!c->ready||family<0||family>1)return STATE_UNKNOWN;
 for(int i=0;i<PATCH_WORDS;i++){
  uint64_t v=read64(c,c->address[i]);
  if(v!=c->stock[i]&&v!=c->candidate[i])return STATE_UNKNOWN;
  if(c->family[i]==(unsigned)family){
   fam_stock&=v==c->stock[i];
   fam_patched&=v==c->candidate[i];
  }else if(c->family[i]==2){
   shared_patched&=v==c->candidate[i];
  }
 }
 return (fam_patched&&shared_patched)?STATE_PATCHED:fam_stock?STATE_STOCK:STATE_MIXED;
}
int patch_snapshot(patch_context *c)
{
 c->have_saved=0;
 if(patch_state(c,0)==STATE_UNKNOWN||patch_state(c,1)==STATE_UNKNOWN)
  return fail(c,"Unknown timing bytes; no writes performed");
 for(int i=0;i<PATCH_WORDS;i++){
  uint64_t v=read64(c,c->address[i]);
  if(v!=c->stock[i]&&v!=c->candidate[i])return fail(c,"Timing changed during preflight");
  c->saved[i]=v;
 }
 c->have_saved=1;return 0;
}
static int transaction(patch_context *c,const uint64_t *target)
{
 uint64_t before[PATCH_WORDS];int touched[PATCH_WORDS],n=0;
 if(!c->ready)return fail(c,"LV1 targets have not been verified");
 /* Full preflight precedes the first store. Never accept arbitrary bytes. */
 for(int i=0;i<PATCH_WORDS;i++){
  before[i]=read64(c,c->address[i]);
  if(before[i]!=c->stock[i]&&before[i]!=c->candidate[i])return fail(c,"Unknown timing bytes; no writes performed");
 }
 for(int i=0;i<PATCH_WORDS;i++)if(before[i]!=target[i]){
  if(read64(c,c->address[i])!=before[i])goto rollback;
  touched[n++]=i;c->write(c->user,c->address[i],target[i]);
  if(read64(c,c->address[i])!=target[i])goto rollback;
 }
 /* Recheck the complete state, including unchanged words. */
 for(int i=0;i<PATCH_WORDS;i++)if(read64(c,c->address[i])!=target[i])goto rollback;
 if(c->log)c->log("Timing transaction verified: %d stores",n);
 return 0;
rollback:
 {
 int bad=0;
 while(n){int i=touched[--n];uint64_t live=read64(c,c->address[i]);
  if(live!=before[i]){
   if(live!=target[i]){bad=1;continue;}
   c->write(c->user,c->address[i],before[i]);
   if(read64(c,c->address[i])!=before[i])bad=1;
  }
 }
 return fail(c,bad?"Readback failed; rollback incomplete. Reboot console":"Timing changed/write failed; stores rolled back");
 }
}
int patch_apply(patch_context *c,int family)
{
 uint64_t target[PATCH_WORDS];
 if(family<0||family>2)return fail(c,"Invalid timing family");
 for(int i=0;i<PATCH_WORDS;i++){
  if(family==2)target[i]=c->candidate[i];
  else target[i]=(c->family[i]==(unsigned)family||c->family[i]==2)?c->candidate[i]:c->stock[i];
 }
 /* Single-variable experiment: publish the patched active height in the
  * metadata record. The owner reported no geometry improvement from this
  * A/B. A metadata/raster mismatch alone does not establish the crop cause. */
 if(c->meta_height_mode){
  for(int p=0;p<4;p++){
   int height_index=p*10+2,raster_index=p*10+5;
   if(family!=2&&c->family[height_index]!=(unsigned)family)continue;
   uint32_t active=(uint32_t)((c->candidate[raster_index]>>32)&0xffffu)+1u;
   target[height_index]=(target[height_index]&UINT64_C(0xffffffff))|((uint64_t)active<<32);
  }
 }
 return transaction(c,target);
}
int patch_restore(patch_context *c){return transaction(c,c->stock);}
int patch_undo(patch_context *c)
{return c->have_saved?transaction(c,c->saved):fail(c,"No experiment snapshot");}
