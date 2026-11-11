#include "ui_layout.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static ui_layout l;
static void observe(void *u,int x,int y,int w,int h,int item,const char *s)
{(void)u;(void)item;(void)s;assert(x>=l.left&&y>=l.top&&x+w<=l.right&&y+h<=l.bottom);}
static void save_ppm(const char *path,const uint32_t *p,int w,int h)
{
 FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",w,h);
 for(int i=0;i<w*h;i++){unsigned char rgb[3]={(unsigned char)(p[i]>>16),(unsigned char)(p[i]>>8),(unsigned char)p[i]};assert(fwrite(rgb,1,3,f)==3);}
 fclose(f);
}
int main(int argc,char **argv)
{
 const char *items[]={"PAL 288p (test unavailable)","NTSC 240p (test unavailable)","Stock timing / reboot info","Output / diagnostics","Recheck timing memory","Exit to XMB"};
 int sizes[][2]={{720,240},{720,288},{720,480},{720,576},{1280,720},{1920,1080}};
 for(int n=0;n<6;n++)for(int wide=0;wide<2;wide++){
  int w=sizes[n][0],h=sizes[n][1];uint32_t *p=malloc(((size_t)w*h+2)*4);assert(p);
  p[0]=p[w*h+1]=0x12345678;
  ui_canvas canvas={p+1,w,observe,NULL};ui_result result;
  ui_layout_init(&l,w,h,wide,n<2);
  ui_view v={0};v.title="Analog SD timing / v1.1.4\nPAL: unavailable  NTSC: unavailable";v.items=items;v.count=6;v.page=-1;
  for(int i=0;i<6;i++){v.selected=i;assert(!ui_render(&l,&v,&canvas,&result));assert(result.selection_visible);}
  v.selected=0;assert(!ui_render(&l,&v,&canvas,&result));
  if(argc>1&&n==1&&!wide)save_ppm(argv[1],p+1,w,h);
  v.items=NULL;v.count=0;v.title="Picture test";
  v.text="PAL 288p candidate loaded into RAM.\n\nIs the picture visible?\nCross: keep   Circle: restore\nRestore in 15 seconds.\n\nCheck the TV/scaler: native progressive output remains unverified. If it still says 576i, choose restore.";
  v.page=0;assert(!ui_render(&l,&v,&canvas,&result));
  for(int i=0;i<result.pages;i++){v.page=i;assert(!ui_render(&l,&v,&canvas,&result));}
  assert(p[0]==0x12345678&&p[w*h+1]==0x12345678);free(p);
 }
 puts("PASS: menu selection and wrapped text inside overscan margins at SD/HD, 4:3/16:9; framebuffer guards intact");return 0;
}
