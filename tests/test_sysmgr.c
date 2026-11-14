/* Synthetic regression: no firmware bytes and no console transport. */
#define main sd_reference_main
#include "test_patch.c"
#undef main
#include "sysmgr.h"
static void relocate(unsigned pid)
{
 put64(0x36cdb0+10*16+8,0);put32(0x120000,pid);
 put64(0x36cdb0+pid*16+8,0x120000);
}
static void other(unsigned pid,int identity)
{
 put64(0x36cdb0+pid*16+8,0x130000);put32(0x130000,pid);put64(0x130018,0x131000);
 put64(0x131000,0x35ecb8);put32(0x13103c,0x132000);
 put64(0x132000,0x1940000);put32(0x132008,0x80005000);
 put32(0x132014,0x132020);ram[0x132018]=12;
 put64(0x132020,0x1950000);put32(0x132028,0xc000a000);
 put32(0x132034,0x131028);ram[0x132038]=12;
 for(unsigned i=0;i<32;i++)ram[0x1940e78+i]=(unsigned char)(i+(identity?0:1));
 put64(0x1950f68,0x80038ad8);put64(0x1950f70,0x80038b10);
}
static unsigned mutation,hits;
static uint64_t changing(void *user,uint64_t at)
{
 if(mutation==1&&at==physical(0x80005e78)&&!hits++)put64(0x36cdb0+31*16+8,0x138000);
 if(mutation==2&&at==0x120000&&++hits==3)put64(0x122000,0x17f0000);
 if(mutation==3&&at==physical(0x80005e78)&&++hits==2)ram[at]^=1;
 return get(user,at);
}
static uint64_t denied(void *user,uint64_t at){(void)user;(void)at;return 0;}
static uint64_t error_value(void *user,uint64_t at){(void)user;(void)at;return UINT64_MAX;}
int main(void)
{
 for(unsigned pid=0;pid<32;pid++){
  setup();relocate(pid);assert(!patch_resolve(&c)&&c.sysmgr_pid==pid&&!stores);
  assert(!patch_snapshot(&c)&&!patch_apply(&c,MODE_ALL_SD));
  assert(!patch_undo(&c));is_stock();
 }
 setup();relocate(3);other(10,0);assert(!patch_resolve(&c)&&c.sysmgr_pid==3&&!stores);
 setup();other(3,1);assert(patch_resolve(&c)&&!c.ready&&!stores);
 assert(strstr(c.error,"Ambiguous"));
 setup();relocate(3);put32(0x120000,10);assert(patch_resolve(&c)&&!stores);
 setup();ram[physical(0x80005e78)+31]^=1;assert(patch_resolve(&c)&&!stores);
 setup();put64(0x122400,UINT64_C(0x28000680000));assert(patch_resolve(&c)&&!stores);
 setup();put64(0x122400,UINT64_MAX-4095);assert(patch_resolve(&c)&&!stores);
 setup();put32(0x122414,0x122400);assert(patch_resolve(&c)&&!stores);
 setup();put32(0x122408,0x80038000);assert(patch_resolve(&c)&&!stores);
 for(mutation=1;mutation<=3;mutation++){
  setup();hits=0;c.read=changing;assert(patch_resolve(&c)&&!c.ready&&!stores);
  assert(strstr(c.error,"changed"));
 }
 sysmgr_page pages[SYSMGR_PAGE_LIMIT];unsigned n=99;sysmgr_report report;
 assert(sysmgr_discover(denied,NULL,pages,&n,&report)&&!n&&strstr(report.error,"all zero"));
 n=99;assert(sysmgr_discover(error_value,NULL,pages,&n,&report)&&!n&&!report.domains);
 n=99;assert(sysmgr_discover(NULL,NULL,pages,&n,&report)&&!n);
 puts("PASS: all 32 relocated PIDs preserve SD apply/undo; decoy, duplicate, denied reads, changed identity/table/maps and MMIO/cycle bounds refuse without writes");
 return 0;
}
