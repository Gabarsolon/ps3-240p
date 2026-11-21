#include "boot_plugins.h"
#include <string.h>
static int line_equals(const char *line,size_t n,const char *path,size_t plen)
{
 while(n&&(line[0]==' '||line[0]=='\t'||line[0]=='\r')){line++;n--;}
 while(n&&(line[n-1]==' '||line[n-1]=='\t'||line[n-1]=='\r'))n--;
 return n==plen&&!memcmp(line,path,plen);
}
int bp_find(const char *text,size_t len,const char *path)
{
 size_t plen=strlen(path),start=0;
 for(size_t i=0;i<=len;i++){
  if(i==len||text[i]=='\n'){
   if(i>start&&line_equals(text+start,i-start,path,plen))return 1;
   start=i+1;
  }
 }
 return 0;
}
size_t bp_add(char *text,size_t len,size_t capacity,const char *path)
{
 if(bp_find(text,len,path))return len;
 size_t plen=strlen(path);
 int crlf=0;
 for(size_t i=0;i+1<len;i++)if(text[i]=='\r'&&text[i+1]=='\n'){crlf=1;break;}
 size_t eol=crlf?2:1,need=plen+eol;
 int add_break=len&&text[len-1]!='\n';
 if(add_break)need+=eol;
 if(len+need+1>capacity)return 0;
 if(add_break){if(crlf)text[len++]='\r';text[len++]='\n';}
 memcpy(text+len,path,plen);len+=plen;
 if(crlf)text[len++]='\r';
 text[len++]='\n';text[len]=0;
 return len;
}
size_t bp_remove(char *text,size_t len,const char *path)
{
 size_t plen=strlen(path),out=0,start=0;
 for(size_t i=0;i<=len;i++){
  if(i==len||text[i]=='\n'){
   size_t n=i-start;
   int last_unterminated=(i==len);
   if(!(n&&line_equals(text+start,n,path,plen))){
    if(n||!last_unterminated){memmove(text+out,text+start,n+(last_unterminated?0:1));out+=n+(last_unterminated?0:1);}
   }
   start=i+1;
  }
 }
 text[out]=0;return out;
}
