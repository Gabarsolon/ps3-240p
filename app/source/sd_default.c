#include "sd_default.h"
#include <stdio.h>
#include <string.h>
#define REG_PATH "/xmb.ps3$xregistry(/setting/display/tvSystem)"
static int parse_key(const char *reply,const char *key,unsigned *value)
{
 char marker[128];snprintf(marker,sizeof(marker),"xregistry(%s) => ",key);
 const char *p=strstr(reply,marker);unsigned decimal,hex;
 if(!p||sscanf(p+strlen(marker),"%u (0x%x)",&decimal,&hex)!=2||hex!=decimal)return -1;
 *value=decimal;return 0;
}
int sd_default_parse(const char *reply)
{unsigned value;return parse_key(reply,"/setting/display/tvSystem",&value)||value>1?-1:(int)value;}
static int read_value(sd_request_fn request,void *u)
{char reply[4096]={0};return request(u,REG_PATH,reply,sizeof(reply))?-1:sd_default_parse(reply);}
static int write_value(sd_request_fn request,void *u,int value)
{
 char path[128],reply[4096]={0};snprintf(path,sizeof(path),REG_PATH "=%d",value);
 /* Setters echo a bare decimal on the owner's webMAN build; getters add
  * the hex value. An independent getter is the authority, including when
  * a setter reply is lost after the server has already applied the write. */
 (void)request(u,path,reply,sizeof(reply));
 return read_value(request,u)==value?0:-1;
}
int sd_default_save(sd_request_fn request,void *u,unsigned resolution)
{
 if(resolution!=4&&resolution!=5)return -1;
 int old=read_value(request,u),desired=resolution==5?1:0;
 if(old<0)return -1;
 /* Builds without DEBUG_XREGISTRY interpret a named key as numeric ID 0.
  * Two different named SD keys must return distinct values before a write
  * is allowed. Otherwise this endpoint could modify an unrelated ID 0. */
 char probe[4096]={0};unsigned mode;
 if(request(u,"/xmb.ps3$xregistry(/setting/display/0/modeid)",probe,sizeof(probe))||
    parse_key(probe,"/setting/display/0/modeid",&mode)||mode==(unsigned)old)return -1;
 if(old==desired||!write_value(request,u,desired))return 0;
 return write_value(request,u,old)?-2:-1;
}
