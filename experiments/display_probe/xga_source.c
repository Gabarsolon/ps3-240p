/* SPDX-License-Identifier: MIT. Source selection only; no hardware stores.
 * Original firmware bytes are retained at runtime, never embedded here. */
#include "xga_source.h"
#include <string.h>
static const uint32_t addresses[16]={0x430858,0x430860,0x430868,0x430870,
 0x430938,0x430940,0x430948,0x430950,0x430980,0x430988,0x430990,0x430998,
 0x430690,0x430698,0x4306a0,0x4306a8};
static uint64_t candidate(const xga_source *s,unsigned index)
{
 uint32_t instruction=0;
 if(!s->native&&index==1)instruction=0x60000000; /* Authored NOP. */
 if(s->native&&(index==5||index==9))instruction=(14u<<26)|(index==5?1024u:768u); /* li r0,dimension */
 /* Keep the logical XMB canvas at its original 1280x720. PAF's native
  * compositor derives its canvas-to-allocation ratio from the separate
  * geometry query. Only the physical allocation changes to 1024x768. */
 return instruction?(s->original[index]&UINT64_C(0xffffffff00000000))|instruction:s->original[index];
}
static int identity(const uint64_t *w)
{
#ifdef XGA_SOURCE_HOST_TEST
 int own=1;for(unsigned i=0;i<16;i++)own&=w[i]==UINT64_C(0x6000000060000000);
 if(own)return 1;
#endif
 uint64_t hash=UINT64_C(14695981039346656037);
 for(unsigned i=0;i<16;i++)for(int shift=56;shift>=0;shift-=8)
  hash=(hash^(unsigned char)(w[i]>>shift))*UINT64_C(1099511628211);
 return hash==UINT64_C(0x614733e5b1c6da9c);
}
static int current(xga_source *s,int allow_candidate)
{
 for(unsigned i=0;i<16;i++){
  uint64_t value;if(s->read(s->user,addresses[i],&value))return -1;
  if(value!=s->original[i]&&!(allow_candidate&&value==candidate(s,i)))return -1;
 }
 return 0;
}
static int resolve(xga_source *s,xga_source_read read,xga_source_write write,void *user,int native)
{
 if(!s||!read||!write||s->dirty)return -1;
 memset(s,0,sizeof(*s));s->read=read;s->write=write;s->user=user;s->native=native;
 for(unsigned i=0;i<16;i++)if(read(user,addresses[i],&s->original[i]))return -1;
 if(!identity(s->original))return -1;
 s->ready=1;return 0;
}
int xga_source_resolve(xga_source *s,xga_source_read read,xga_source_write write,void *user)
{return resolve(s,read,write,user,0);}
int xga_source_resolve_native(xga_source *s,xga_source_read read,xga_source_write write,void *user)
{return resolve(s,read,write,user,1);}
int xga_source_restore(xga_source *s)
{
 if(!s||!s->ready||current(s,1))return -1;
 for(unsigned i=0;i<16;i++)if(candidate(s,i)!=s->original[i]){
  uint64_t value;if(s->read(s->user,addresses[i],&value))return -1;
  if(value==s->original[i])continue;
  (void)s->write(s->user,addresses[i],s->original[i]);
  if(s->read(s->user,addresses[i],&value)||value!=s->original[i])return -1;
 }
 if(current(s,0))return -1;
 s->dirty=0;return 0;
}
int xga_source_apply(xga_source *s)
{
 if(!s||!s->ready||s->dirty||current(s,0))return -1;
 s->dirty=1;
 for(unsigned i=0;i<16;i++)if(candidate(s,i)!=s->original[i]){
  int rc=s->write(s->user,addresses[i],candidate(s,i));uint64_t value;
  if(rc||s->read(s->user,addresses[i],&value)||value!=candidate(s,i)){
   (void)xga_source_restore(s);return -1;
  }
 }
 return 0;
}
