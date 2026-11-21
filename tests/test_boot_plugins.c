#include "boot_plugins.h"
#include <stdio.h>
#include <string.h>
#define P "/dev_hdd0/plugins/disc_rgb_fix.sprx"
static int fails;
#define CHECK(c) do{if(!(c)){printf("FAIL line %d: %s\n",__LINE__,#c);fails++;}}while(0)
static size_t load(char *b,const char *s){strcpy(b,s);return strlen(s);}
int main(void)
{
 char b[512];size_t n;
 n=load(b,"");CHECK(!bp_find(b,n,P));n=bp_add(b,n,sizeof(b),P);CHECK(!strcmp(b,P "\n"));
 n=bp_add(b,n,sizeof(b),P);CHECK(!strcmp(b,P "\n"));            /* idempotent */
 n=bp_remove(b,n,P);CHECK(n==0&&!b[0]);
 n=load(b,"/dev_hdd0/plugins/a.sprx");n=bp_add(b,n,sizeof(b),P); /* no trailing newline */
 CHECK(!strcmp(b,"/dev_hdd0/plugins/a.sprx\n" P "\n"));
 n=bp_remove(b,n,P);CHECK(!strcmp(b,"/dev_hdd0/plugins/a.sprx\n"));
 n=load(b,"/dev_hdd0/plugins/a.sprx\r\n/dev_hdd0/plugins/b.sprx\r\n");n=bp_add(b,n,sizeof(b),P);
 CHECK(!strcmp(b,"/dev_hdd0/plugins/a.sprx\r\n/dev_hdd0/plugins/b.sprx\r\n" P "\r\n"));
 CHECK(bp_find(b,n,P));
 n=bp_remove(b,n,P);CHECK(!strcmp(b,"/dev_hdd0/plugins/a.sprx\r\n/dev_hdd0/plugins/b.sprx\r\n"));
 n=load(b,"/dev_hdd0/plugins/a.sprx\n  " P " \n/dev_hdd0/plugins/b.sprx");CHECK(bp_find(b,n,P));
 n=bp_remove(b,n,P);CHECK(!strcmp(b,"/dev_hdd0/plugins/a.sprx\n/dev_hdd0/plugins/b.sprx"));
 n=load(b,P "x\n");CHECK(!bp_find(b,n,P));                      /* prefix is not a match */
 n=load(b,"/dev_hdd0/plugins/a.sprx\n");CHECK(bp_add(b,n,n+5,P)==0&&!strcmp(b,"/dev_hdd0/plugins/a.sprx\n")); /* no room */
 n=load(b,P);n=bp_remove(b,n,P);CHECK(n==0);                    /* only line, no newline */
 printf(fails?"boot_plugins FAILED\n":"boot_plugins ok\n");return fails!=0;
}
