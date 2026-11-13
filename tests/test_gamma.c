#include "gamma.h"
#include "ui_layout.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ui_layout layout;
static void observe(void *u,int x,int y,int w,int h,int item,const char *s)
{(void)u;(void)item;(void)s;assert(x>=layout.left&&y>=layout.top&&x+w<=layout.right&&y+h<=layout.bottom);}
int main(void)
{
 const char *bad[]={"", "nan", "inf", "2.200", "2.2abc", "0", "0.49", "4.01", "-2.2", ".8", "2.", "22", "2,2"};
 int value=123;
 for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++){assert(crt_gamma_parse(bad[i],&value));assert(value==123);}
 assert(!crt_gamma_parse(" 2.2\r\n",&value)&&value==220);
 assert(!crt_gamma_parse("1",&value)&&value==100);
 uint8_t ramp[256],identity[256];char text[5],shader[2048],tiny[4];
 assert(!crt_gamma_ramp(100,identity));for(int i=0;i<256;i++)assert(identity[i]==i);
 for(int g=50;g<=400;g++){
  crt_gamma_format(g,text);assert(!crt_gamma_parse(text,&value)&&value==g);
  assert(!crt_gamma_ramp(g,ramp));assert(ramp[0]==0&&ramp[255]==255);
  for(int i=1;i<256;i++)assert(ramp[i]>=ramp[i-1]);
 }
 assert(crt_gamma_ramp(0,ramp));assert(crt_gamma_shader(220,tiny,sizeof(tiny))<0);
 assert(!crt_gamma_ramp(220,ramp));assert(ramp[128]>=186&&ramp[128]<=187&&ramp[16]>16);
 assert(crt_gamma_shader(220,shader,sizeof(shader))>0);
 assert(strstr(shader,"1.0/2.20")&&strstr(shader,"main_vertex")&&strstr(shader,"main_fragment"));
 const char *path="build/tests/gamma-test.txt";remove(path);
 assert(!crt_gamma_save(path,220));value=0;assert(!crt_gamma_load(path,&value)&&value==220);
 assert(!crt_gamma_save(path,180));assert(!crt_gamma_load(path,&value)&&value==180);
 assert(!crt_gamma_save(path,240));assert(!crt_gamma_load(path,&value)&&value==240);
 assert(crt_gamma_save(path,401));assert(!crt_gamma_load(path,&value)&&value==240);
 FILE *old=fopen("build/tests/gamma-test.txt.old","rb");assert(!old);
 FILE *f=fopen(path,"wb");assert(f);fputs("2.20\0junk",f);fwrite("\0junk",1,5,f);fclose(f);
 value=100;assert(crt_gamma_load(path,&value)&&value==100);remove(path);
 int sizes[][2]={{720,480},{720,576},{1280,720},{1920,1080}};
 for(int s=0;s<4;s++)for(int wide=0;wide<2;wide++)for(int interlaced=0;interlaced<2;interlaced++){
  int w=sizes[s][0],h=sizes[s][1];uint32_t *p=malloc(((size_t)w*h+2)*sizeof(*p));assert(p);
  p[0]=p[w*h+1]=0x12345678;ui_canvas c={p+1,w,observe,NULL};ui_result r;
  ui_layout_init(&layout,w,h,wide,interlaced);
  assert(!ui_render_gamma(&layout,&c,&r,220,1,ramp,0));
  assert(!ui_render_gamma(&layout,&c,&r,220,1,ramp,220));
  assert(p[0]==0x12345678&&p[w*h+1]==0x12345678);
  free(p);
 }
 puts("PASS: gamma values, identity, brightening direction, monotonic ramps, file validation, shader and overscan-safe preview");
 return 0;
}
