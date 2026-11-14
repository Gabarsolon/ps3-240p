#include "sysmgr.h"
#include <string.h>

/* PROVEN: private 4.93 LV1 Process table/constructor and sysmgr ELF entry.
 * Addresses, hashes and provenance: docs/SYSMGR_DISCOVERY.md. */
#define PROCESS_TABLE UINT64_C(0x36cdb0)
#define DOMAIN_VTABLE UINT64_C(0x35ecb8)
#define ENTRY_VA 0x80005e78u
#define FNV_OFFSET UINT64_C(14695981039346656037)
#define FNV_PRIME UINT64_C(1099511628211)

static int bounded(uint64_t a,uint64_t n)
{return a>=0x1000&&n&&n<=SYSMGR_RAM_LIMIT&&a<=SYSMGR_RAM_LIMIT-n;}
static uint64_t word(sysmgr_read_fn read,void *user,uint64_t a)
{return bounded(a,8)&&!(a&7)?read(user,a):0;}
static uint32_t half(sysmgr_read_fn read,void *user,uint64_t a)
{uint64_t v=word(read,user,a&~UINT64_C(7));return (uint32_t)(a&4?v:v>>32);}
static uint64_t hash_word(uint64_t hash,uint64_t value)
{for(int i=7;i>=0;i--)hash=(hash^(unsigned char)(value>>(i*8)))*FNV_PRIME;return hash;}
int sysmgr_entry_identity(const uint64_t words[4])
{
 uint64_t hash=FNV_OFFSET;
 for(unsigned i=0;i<4;i++)hash=hash_word(hash,words[i]);
#ifdef SYSMGR_SYNTHETIC_TEST
 /* Tests use self-authored bytes 0..31, never a firmware fixture. */
 return hash==UINT64_C(0xe6cb594c1a148ac5);
#else
 return hash==UINT64_C(0xd79673d8c39c85a6);
#endif
}
int sysmgr_translate(const sysmgr_page *pages,unsigned count,uint32_t va,uint64_t *pa)
{
 unsigned found=0;
 for(unsigned i=0;i<count;i++)if(va>=pages[i].virtual_address&&
     (uint64_t)va-pages[i].virtual_address+8<=pages[i].size){
  if(found++||!bounded(pages[i].physical,pages[i].size))return -1;
  *pa=pages[i].physical+((uint64_t)va-pages[i].virtual_address);
 }
 return found==1&&bounded(*pa,8)&&!(*pa&7)?0:-1;
}
static int candidate(sysmgr_read_fn read,void *user,unsigned pid,uint64_t proc,
                     sysmgr_page *pages,unsigned *count,uint64_t *domain_out,
                     uint64_t *signature,unsigned *domains)
{
 if(!bounded(proc,0x28)||(proc&7)||half(read,user,proc)!=pid)return -1;
 uint64_t domain=word(read,user,proc+0x18);
 if(!bounded(domain,0x40)||(domain&7)||word(read,user,domain)!=DOMAIN_VTABLE)return -1;
 (*domains)++;
 uint64_t sentinel=domain+0x28,node=half(read,user,domain+0x3c),seen[SYSMGR_PAGE_LIMIT];
 uint64_t hash=hash_word(hash_word(FNV_OFFSET,proc),domain);
 unsigned n=0;
 while(node!=sentinel){
  if(n==SYSMGR_PAGE_LIMIT||!bounded(node,0x20)||(node&7))return -1;
  for(unsigned i=0;i<n;i++)if(seen[i]==node)return -1;
  seen[n]=node;hash=hash_word(hash,node);
  uint64_t raw[4];for(unsigned i=0;i<4;i++){raw[i]=word(read,user,node+i*8);hash=hash_word(hash,raw[i]);}
  unsigned shift=(unsigned)(raw[3]>>56);
  if(shift<12||shift>32)return -1;
  pages[n].physical=raw[0];pages[n].virtual_address=(uint32_t)(raw[1]>>32);
  pages[n].size=UINT64_C(1)<<shift;
  n++;node=(uint32_t)raw[2];
 }
 if(!n)return -1;
 uint64_t entry[4],pa;
 for(unsigned i=0;i<4;i++){
  if(sysmgr_translate(pages,n,ENTRY_VA+i*8,&pa))return -1;
  entry[i]=word(read,user,pa);
 }
 if(!sysmgr_entry_identity(entry))return -1;
 if(sysmgr_translate(pages,n,0xc000af68u,&pa)||word(read,user,pa)!=0x80038ad8u||
    sysmgr_translate(pages,n,0xc000af70u,&pa)||word(read,user,pa)!=0x80038b10u)return -1;
 *count=n;*domain_out=domain;*signature=hash;return 0;
}
static int refuse(sysmgr_report *report,const char *reason)
{
 /* Keep this shared module independent of stdio in the resident VSH helper. */
 size_t i=0;for(;i+1<sizeof(report->error)&&reason[i];i++)report->error[i]=reason[i];
 report->error[i]=0;return -1;
}
int sysmgr_discover(sysmgr_read_fn read,void *user,sysmgr_page *pages,
                    unsigned *out_count,sysmgr_report *report)
{
 if(!out_count||!report)return -1;
 *out_count=0;memset(report,0,sizeof(*report));report->slot10_pid=UINT32_MAX;
 if(!read||!pages)return refuse(report,"Missing LV1 read transport");
 uint64_t table[32],selected_hash=0;
 for(unsigned pid=0;pid<32;pid++)table[pid]=word(read,user,PROCESS_TABLE+pid*16+8);
 report->slot10_pointer=table[10];
 if(bounded(table[10],0x28)&&!(table[10]&7))report->slot10_pid=half(read,user,table[10]);
 for(unsigned pid=0;pid<32;pid++)if(table[pid]){
  report->occupied++;
  unsigned n=0;uint64_t domain,signature;
  if(candidate(read,user,pid,table[pid],pages,&n,&domain,&signature,&report->domains))continue;
  report->matches++;report->pid=pid;report->process=table[pid];report->domain=domain;selected_hash=signature;
 }
 if(report->matches>1)return refuse(report,"Ambiguous 4.93 sysmgr identities; no writes allowed");
 if(!report->matches)return refuse(report,report->occupied?
   "No verified 4.93 sysmgr in 32 slots; check LV1 reads/layout (see log)":
   "LV1 process pointers all zero; reads unavailable or layout unsupported");
 unsigned n=0,ignored=0;uint64_t domain,signature;
 if(candidate(read,user,report->pid,report->process,pages,&n,&domain,&signature,&ignored)||
    domain!=report->domain||signature!=selected_hash)
  return refuse(report,"sysmgr identity/mappings changed during discovery; retry");
 for(unsigned pid=0;pid<32;pid++)if(table[pid]!=word(read,user,PROCESS_TABLE+pid*16+8))
  return refuse(report,"LV1 process table changed during discovery; retry");
 *out_count=n;return 0;
}
