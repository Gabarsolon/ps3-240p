/* SPDX-License-Identifier: MIT. Query-only HEN protocol; no LV1 operations. */
#include "diagnostic.h"
#include <string.h>

#ifndef HD_PUBLIC_ONLY
static uint64_t pointer(const void *p){return (uint64_t)(uintptr_t)p;}
#endif
uint64_t hd_checked_call(hd_bus *bus,uint64_t op,uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e)
{
#ifdef HD_PUBLIC_ONLY
 (void)bus;(void)op;(void)a;(void)b;(void)c;(void)d;(void)e;return HD_UNQUERIED;
#else
 if(!bus||!bus->raw)return HD_UNQUERIED;
 int allowed=0;
 if(op==0x1337)allowed=!(a|b|c|d|e);
 else if(bus->hen_confirmed){
  if(op==0x1339)allowed=!(a|b|c|d|e);
  else if(op==0x7777){
   if(a==0x11)allowed=!(b|c|d|e); /* Core identity, before other PS3MAPI queries. */
   else if(bus->mapi_confirmed){
    if(a==0x12||a==0x14)allowed=!(b|c|d|e);
    else if(a==0x47)allowed=b>=1&&b<=6&&c&&d&&!e;
    else if(a==0x21)allowed=b&&!(c|d|e);
    else if(a==0x22)allowed=b&&b<=UINT32_MAX&&c&&!(d|e);
    else if(a==0x31)allowed=bus->vsh_pid&&b==bus->vsh_pid&&
      c==HD_VSH_HEADER_ADDRESS&&d&&e==HD_VSH_HEADER_BYTES;
   }
  }
 }
 return allowed?bus->raw(bus->user,op,a,b,c,d,e):HD_UNQUERIED;
#endif
}
#ifndef HD_PUBLIC_ONLY
static uint64_t scalar(hd_bus *b,uint64_t subop)
{return hd_checked_call(b,0x7777,subop,0,0,0,0);}
#endif

void hd_collect(hd_report *r,hd_raw_fn raw,void *user,void (*video)(void *,hd_video *))
{
 memset(r,0,sizeof(*r));
 r->hen_identity=r->hen_revision=r->core_version=r->core_minversion=
  r->firmware_version=r->process_list_rc=r->header_rc=HD_UNQUERIED;
 for(unsigned i=0;i<6;i++)r->slots[i].rc=HD_UNQUERIED;
 r->video.state_rc=r->video.configuration_rc=r->video.device_count=r->video.device_rc=-1;
 if(video)video(user,&r->video);
#ifdef HD_PUBLIC_ONLY
 (void)raw;return;
#else
 hd_bus bus={raw,user,0,0,0};
 r->hen_identity=hd_checked_call(&bus,0x1337,0,0,0,0,0);
 if(r->hen_identity!=0x1337)return;
 r->hen_confirmed=bus.hen_confirmed=1;
 r->hen_revision=hd_checked_call(&bus,0x1339,0,0,0,0,0);
 r->core_version=scalar(&bus,0x11);
 /* Errors and zero are not an API identity. This is not an LV1 capability test. */
 if(r->core_version<0x100||r->core_version>0xffff)return;
 r->mapi_confirmed=bus.mapi_confirmed=1;
 r->core_minversion=scalar(&bus,0x12);r->firmware_version=scalar(&bus,0x14);
 for(unsigned i=0;i<6;i++){
  hd_slot *s=&r->slots[i];
  s->rc=hd_checked_call(&bus,0x7777,0x47,i+1,pointer(s->name),pointer(s->path),0);
  s->name[sizeof(s->name)-1]=0;s->path[sizeof(s->path)-1]=0;
  if(s->rc){s->name[0]=s->path[0]=0;}
 }
 uint32_t pids[16]={0};
 r->process_list_rc=hd_checked_call(&bus,0x7777,0x21,pointer(pids),0,0,0);
 if(r->process_list_rc)return;
 for(unsigned i=0;i<16;i++)if(pids[i]){
  /* Upstream copies a process name (25-byte source) without its terminator. */
  char name[256]={0};
  uint64_t rc=hd_checked_call(&bus,0x7777,0x22,pids[i],pointer(name),0,0);
  name[sizeof(name)-1]=0;
  /* HEN passes through get_process_name verbatim; accept only exact known
   * spellings, never upstream's broader strstr(name,"vsh") selection. */
  if(!rc&&(!strcmp(name,"vsh")||!strcmp(name,"main_vsh.self"))){
   if(!r->vsh_matches||r->vsh_pid!=pids[i])r->vsh_matches++;
   r->vsh_pid=pids[i];
  }
 }
 if(r->vsh_matches!=1){r->vsh_pid=0;return;}
 bus.vsh_pid=r->vsh_pid;
 /* One fixed 16-byte ELF identification read, never a pointer walk or dump. */
 unsigned char header[HD_VSH_HEADER_BYTES]={0};
 r->header_rc=hd_checked_call(&bus,0x7777,0x31,r->vsh_pid,HD_VSH_HEADER_ADDRESS,pointer(header),sizeof(header));
 if(!r->header_rc){
  r->elf_class=header[4];r->elf_endian=header[5];
  r->header_valid=!memcmp(header,"\177ELF",4)&&header[4]==2&&header[5]==2&&header[6]==1;
 }
#endif
}

static void string(FILE *f,const char *s)
{
 fputc('"',f);
 for(const unsigned char *p=(const unsigned char *)s;*p;p++){
  if(*p=='"'||*p=='\\'){fputc('\\',f);fputc(*p,f);}
  else if(*p<32||*p>=127)fprintf(f,"\\u%04x",*p);
  else fputc(*p,f);
 }
 fputc('"',f);
}
static void value(FILE *f,const char *name,uint64_t v)
{fprintf(f,"\"%s\":",name);if(v==HD_UNQUERIED)fputs("null",f);else fprintf(f,"\"0x%016llx\"",(unsigned long long)v);}
static void mode(FILE *f,const hd_mode *m)
{fprintf(f,"{\"resolution\":%u,\"scan\":%u,\"conversion\":%u,\"aspect\":%u,\"refresh_mask\":%u}",m->resolution,m->scan,m->conversion,m->aspect,m->refresh);}
int hd_write_json(FILE *f,const hd_report *r)
{
 if(!f||!r)return -1;
 fputs("{\n\"schema\":1,\"diagnostic\":\"" HD_VERSION "\",\"title_id\":\"" HD_TITLE_ID "\",\"mode\":\"" HD_MODE "\",\n",f);
 fputs("\"lv1\":{\"capability\":\"UNTESTED\",\"operations_requested\":0},\n",f);
 fputs("\"scope\":\"Query replies only; no low-line, gamma or game compatibility established\",\n",f);
 const char *hen_status=r->hen_identity==HD_UNQUERIED?"UNTESTED":r->hen_confirmed?"MATCH":"NOT_MATCHED";
 fprintf(f,"\"hen\":{\"status\":\"%s\",\"matched\":%s,",hen_status,r->hen_confirmed?"true":"false");
 value(f,"identity",r->hen_identity);fputc(',',f);value(f,"revision",r->hen_revision);fputs("},\n",f);
 fprintf(f,"\"ps3mapi\":{\"identity_accepted\":%s,",r->mapi_confirmed?"true":"false");
 value(f,"core",r->core_version);fputc(',',f);value(f,"minimum",r->core_minversion);
 fputc(',',f);value(f,"firmware",r->firmware_version);fputs("},\n\"slots\":[",f);
 for(unsigned i=0;i<6;i++){
  const hd_slot *s=&r->slots[i];if(i)fputc(',',f);
  fprintf(f,"{\"slot\":%u,",i+1);value(f,"rc",s->rc);fputs(",\"name\":",f);string(f,s->name);
  /* Do not publish arbitrary paths that can contain home/account IDs. */
  const char *leaf=strrchr(s->path,'/');leaf=leaf?leaf+1:s->path;
  fputs(",\"filename\":",f);string(f,leaf);fputc('}',f);
 }
 fputs("],\n\"vsh_read\":{",f);value(f,"process_list_rc",r->process_list_rc);
 fprintf(f,",\"matching_processes\":%u,\"pid\":%u,\"address\":\"0x00010000\",\"bytes\":16,",r->vsh_matches,r->vsh_pid);
 value(f,"rc",r->header_rc);
 const char *read_status=r->header_rc==HD_UNQUERIED?"UNTESTED":r->header_rc?"READ_FAILED":r->header_valid?"HEADER_MATCH":"HEADER_MISMATCH";
 fprintf(f,",\"status\":\"%s\",\"elf64_big_endian\":%s,\"class\":%u,\"endian\":%u},\n",read_status,r->header_valid?"true":"false",r->elf_class,r->elf_endian);
 const hd_video *v=&r->video;
 fprintf(f,"\"public_video\":{\"state_rc\":%d,\"configuration_rc\":%d,\"device_count\":%d,\"device_rc\":%d,\n",v->state_rc,v->configuration_rc,v->device_count,v->device_rc);
 fprintf(f,"\"state\":%u,\"color_space\":%u,\"current\":",v->state,v->color_space);mode(f,&v->current);
 fprintf(f,",\"configuration\":{\"resolution\":%u,\"format\":%u,\"aspect\":%u,\"pitch\":%u},\n",v->configuration_resolution,v->configuration_format,v->configuration_aspect,v->pitch);
 fprintf(f,"\"device\":{\"port\":%u,\"state\":%u,\"color_space\":%u,\"rgb_range\":%u,\"reported_modes\":%u,\"modes\":[",v->port,v->device_state,v->device_color_space,v->rgb_range,v->reported_modes);
 unsigned n=v->mode_count<32?v->mode_count:32;
 for(unsigned i=0;i<n;i++){if(i)fputc(',',f);mode(f,&v->modes[i]);}
 fputs("]}}\n}\n",f);return ferror(f)?-1:0;
}
