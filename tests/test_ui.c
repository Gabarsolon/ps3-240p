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
 const char *items[]={"Test 240p 60Hz (15-second recovery)","Enable 240p 60Hz + background enforcer","Test PAL 288p 50Hz (15-second recovery)","Enable 288p 50Hz + background enforcer","Stock 480i 60Hz + save boot default","Stock 576i 50Hz + save boot default","Output / diagnostics","Geometry pattern (measure crop)","Gamma / shadow correction","Recheck timing memory","Exit to XMB"};
 const int item_count=(int)(sizeof(items)/sizeof(items[0]));
 int sizes[][2]={{720,240},{720,288},{720,480},{720,576},{1280,720},{1920,1080}};
 for(int n=0;n<6;n++)for(int wide=0;wide<2;wide++){
  int w=sizes[n][0],h=sizes[n][1];uint32_t *p=malloc(((size_t)w*h+2)*4);assert(p);
  p[0]=p[w*h+1]=0x12345678;
  ui_canvas canvas={p+1,w,observe,NULL};ui_result result;
  ui_layout_init(&l,w,h,wide,n>=2&&n<4);
  ui_view v={0};v.title="CRT Toolset / v1.4.3\nPAL: candidate  NTSC: candidate";v.items=items;v.count=item_count;v.page=-1;
  for(int i=0;i<item_count;i++){v.selected=i;assert(!ui_render(&l,&v,&canvas,&result));assert(result.selection_visible);}
  v.selected=0;assert(!ui_render(&l,&v,&canvas,&result));
  if(argc>1&&n==1&&!wide)save_ppm(argv[1],p+1,w,h);
  v.items=NULL;v.count=0;v.title="Picture test";
  v.text="PAL 288p candidate loaded into RAM.\n\nIs the picture visible?\nCross: keep   Circle: restore\nRestore in 15 seconds.\n\nCheck the TV/scaler: native progressive output remains unverified. If it still says 576i, choose restore.";
  v.page=0;assert(!ui_render(&l,&v,&canvas,&result));
  for(int i=0;i<result.pages;i++){v.page=i;assert(!ui_render(&l,&v,&canvas,&result));}
  assert(p[0]==0x12345678&&p[w*h+1]==0x12345678);free(p);
 }
 /* Geometry pattern: equal bars, centred square, safe box, magenta band. */
 {
  int w=720,h=480,content=240;uint32_t *p=malloc((size_t)w*h*4);assert(p);
  ui_canvas canvas={p,w,NULL,NULL};
  assert(!ui_render_pattern(&canvas,w,h,content));
  /* Alternating bars: even bars white, odd bars black, equal widths. */
  for(int i=0;i<16;i++)assert(p[4*w+i*(w/16)+4]==((i&1)?0xff000000u:0xffffffffu));
  /* Magenta band below the content marks what a cropped raster would hide. */
  assert(p[content*w+w/2]==0xffff00ffu);
  assert(p[(h-1)*w+w/2]==0xffff00ffu);
  /* Centred square outline is cyan on the bar field. */
  int side=content/2,sx=(w-side)/2,sy=(content-side)/2;
  assert(p[sy*w+sx]==0xff40ffffu);
  assert(p[(sy+side-1)*w+sx+side-1]==0xff40ffffu);
  /* Safe box stays inside the content area. */
  int mx=(w*11+99)/100,my=(content*12+99)/100;
  assert(mx>0&&my>0&&2*mx<w&&2*my<content);
  assert(p[my*w+mx]==0xff40ff40u);
  /* The whole content area is painted, no background leak. */
  int painted=0;for(int i=0;i<content;i++)for(int j=0;j<w;j++)if(p[i*w+j]!=UI_BACKGROUND)painted++;
  assert(painted==content*w);
  if(argc>2)save_ppm(argv[2],p,w,h);
  free(p);
  /* A pure 240-line pattern fills its canvas exactly. */
  uint32_t *q=malloc((size_t)720*240*4);assert(q);
  ui_canvas c2={q,720,NULL,NULL};
  assert(!ui_render_pattern(&c2,720,240,240));
  assert(q[239*720+360]!=0xffff00ffu);
  if(argc>3)save_ppm(argv[3],q,720,240);
  free(q);
 }
 puts("PASS: menu selection and wrapped text inside overscan margins at SD/HD, 4:3/16:9; framebuffer guards intact");
 puts("PASS: geometry pattern renders equal bars, centred square, safe box and magenta crop band");
 return 0;
}
