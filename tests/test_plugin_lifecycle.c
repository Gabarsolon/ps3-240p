#include "plugin_lifecycle.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char *names[7],*paths[7];
static unsigned loaded,unloaded,fail_info;
static int fail_load,fail_unload,sticky;
static int info(void *u,unsigned s,char *name,char *path)
{
 (void)u;assert(s>=1&&s<=6);
 /* Reproduce PS3MAPI's unterminated string copies. */
 for(unsigned i=0;i<96;i++)assert(!name[i]);
 for(unsigned i=0;i<256;i++)assert(!path[i]);
 if(s==fail_info)return -5;
 if(!names[s])return (int)0x80010006u;
 memcpy(name,names[s],strlen(names[s]));memcpy(path,paths[s],strlen(paths[s]));return 0;
}
static int load(void *u,unsigned s)
{(void)u;loaded++;assert(!names[s]);if(fail_load)return -9;names[s]="vsh_240p";paths[s]="/dev_hdd0/plugins/vsh_240p.sprx";return 0;}
static int unload(void *u,unsigned s)
{(void)u;unloaded++;assert(!strcmp(names[s],"vsh_240p"));if(fail_unload)return -7;if(!sticky)names[s]=paths[s]=NULL;return 0;}
static plugin_lifecycle reset(int resident)
{
 memset(names,0,sizeof(names));memset(paths,0,sizeof(paths));loaded=unloaded=fail_info=0;fail_load=fail_unload=sticky=0;
 names[1]="WWWD";paths[1]="/dev_hdd0/plugins/webftp_server.sprx";
 if(resident){names[2]="vsh_240p";paths[2]="/dev_hdd0/plugins/vsh_240p.sprx";}
 plugin_lifecycle p={info,load,unload,NULL,{0}};return p;
}
int main(void)
{
 assert(COBRA_LOAD_VSH_PLUGIN==0x1ee7&&COBRA_UNLOAD_VSH_PLUGIN==0x364f);
 unsigned slot;int reused;plugin_lifecycle p=reset(1);
 /* Re-enabling or changing preference must reuse the occupied helper slot. */
 for(int i=0;i<3;i++)assert(!helper_start(&p,0,&slot,&reused)&&slot==2&&reused&&!loaded&&!unloaded);
 assert(!strcmp(names[1],"WWWD")); /* Preserve webMAN. */
 p=reset(0);assert(!helper_start(&p,0,&slot,&reused)&&slot==2&&!reused&&loaded==1&&!unloaded);
 p=reset(1);assert(!helper_start(&p,1,&slot,&reused)&&slot==2&&!reused&&loaded==1&&unloaded==1);
 p=reset(1);fail_unload=1;assert(helper_start(&p,1,&slot,&reused)==-1&&!loaded&&unloaded==1);
 p=reset(1);sticky=1;assert(helper_start(&p,1,&slot,&reused)==-1&&!loaded&&unloaded==1);
 p=reset(0);fail_load=1;assert(helper_start(&p,0,&slot,&reused)==-1&&loaded==1);
 p=reset(1);fail_info=3;assert(helper_start(&p,0,&slot,&reused)==-1&&!loaded&&!unloaded);
 p=reset(1);names[3]="vsh_240p";paths[3]="duplicate";assert(helper_start(&p,0,&slot,&reused)==-1&&!loaded&&!unloaded);
 p=reset(0);for(int i=2;i<=6;i++){names[i]="other";paths[i]="occupied";}
 assert(helper_start(&p,0,&slot,&reused)==-1&&!loaded&&!unloaded);
 p=reset(1);assert(!helper_stop(&p)&&unloaded==1&&!names[2]&&names[1]);
 assert(!helper_stop(&p)&&unloaded==1);
 puts("PASS: repeated enable reuses resident helper; checked replacement/unload, occupied slots, inspection errors and unterminated names");return 0;
}
