/* SPDX-License-Identifier: MIT. No MMIO, firmware calls or other-app edits. */
#include "gamma.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int crt_gamma_parse(const char *s,int *value)
{
 if(!s||!value)return -1;
 while(*s==' '||*s=='\t'||*s=='\r'||*s=='\n')s++;
 if(*s<'0'||*s>'4')return -1;
 int n=(*s++-'0')*100;
 if(*s=='.'){
  s++;if(*s<'0'||*s>'9')return -1;n+=(*s++-'0')*10;
  if(*s>='0'&&*s<='9')n+=*s++-'0';
 }
 while(*s==' '||*s=='\t'||*s=='\r'||*s=='\n')s++;
 if(*s||n<CRT_GAMMA_MIN||n>CRT_GAMMA_MAX)return -1;
 *value=n;return 0;
}
void crt_gamma_format(int value,char out[5])
{
 if(value<CRT_GAMMA_MIN||value>CRT_GAMMA_MAX)value=CRT_GAMMA_NEUTRAL;
 out[0]=(char)('0'+value/100);out[1]='.';
 out[2]=(char)('0'+value/10%10);out[3]=(char)('0'+value%10);out[4]=0;
}
int crt_gamma_ramp(int value,uint8_t ramp[256])
{
 if(!ramp||value<CRT_GAMMA_MIN||value>CRT_GAMMA_MAX)return -1;
 for(int i=0;i<256;i++)ramp[i]=value==100?(uint8_t)i:
  (uint8_t)(255.0*pow(i/255.0,100.0/value)+0.5);
 return 0;
}
int crt_gamma_load(const char *path,int *value)
{
 if(!path||!value)return -1;
 FILE *f=fopen(path,"rb");if(!f)return -1;
 char text[32];size_t n=fread(text,1,sizeof(text)-1,f);
 int error=ferror(f),extra=fgetc(f);text[n]=0;
 if(fclose(f)||error||extra!=EOF||memchr(text,0,n))return -1;
 return crt_gamma_parse(text,value);
}
int crt_gamma_save(const char *path,int value)
{
 if(!path||value<CRT_GAMMA_MIN||value>CRT_GAMMA_MAX)return -1;
 char tmp[512],backup[512],text[5];int n=snprintf(tmp,sizeof(tmp),"%s.tmp",path);
 if(n<0||(size_t)n>=sizeof(tmp))return -1;
 n=snprintf(backup,sizeof(backup),"%s.old",path);
 if(n<0||(size_t)n>=sizeof(backup))return -1;
 crt_gamma_format(value,text);FILE *f=fopen(tmp,"wb");if(!f)return -1;
 int failed=fwrite(text,1,4,f)!=4;
 if(fflush(f))failed=1;if(fclose(f))failed=1;
 int check=0;if(failed||crt_gamma_load(tmp,&check)||check!=value){remove(tmp);return -1;}
 int backed_up=0;
 if(rename(tmp,path)){
  /* LV2 (and Windows) rename refuses an existing destination. Preserve the
   * previous preference while giving the verified new file its final name. */
  FILE *previous=fopen(path,"rb");if(!previous){remove(tmp);return -1;}
  if(fclose(previous)){remove(tmp);return -1;}
  if(remove(backup)&&errno!=ENOENT){remove(tmp);return -1;}
  if(rename(path,backup)){remove(tmp);return -1;}backed_up=1;
  if(rename(tmp,path)){(void)rename(backup,path);remove(tmp);return -1;}
 }
 if(crt_gamma_load(path,&check)||check!=value){
  if(backed_up){(void)remove(path);(void)rename(backup,path);}return -1;
 }
 if(backed_up)(void)remove(backup);
 return 0;
}
int crt_gamma_shader(int value,char *out,size_t capacity)
{
 if(!out||!capacity||value<CRT_GAMMA_MIN||value>CRT_GAMMA_MAX)return -1;
 char number[5];crt_gamma_format(value,number);
 int n=snprintf(out,capacity,
  "// SPDX-License-Identifier: MIT\n"
  "// CRT Toolset correction %s; 1.00 neutral, higher brightens.\n"
  "// Applies to RetroArch content only. No scanlines or scaling.\n"
  "struct vertex_output { float4 position : POSITION; float2 tex : TEXCOORD0; };\n"
  "vertex_output main_vertex(float4 position : POSITION, float2 tex : TEXCOORD0,\n"
  " uniform float4x4 modelViewProj) {\n"
  " vertex_output o; o.position=mul(modelViewProj,position); o.tex=tex; return o;\n}\n"
  "float4 main_fragment(vertex_output i, uniform sampler2D decal : TEXUNIT0) : COLOR {\n"
  " float4 c=tex2D(decal,i.tex); c.rgb=pow(saturate(c.rgb),float3(1.0/%s,1.0/%s,1.0/%s)); return c;\n}\n",
  number,number,number,number);
 return n<0||(size_t)n>=capacity?-1:n;
}
