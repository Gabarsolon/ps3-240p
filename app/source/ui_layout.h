/* SPDX-License-Identifier: MIT. Shared PS3/PC layout and software renderer. */
#ifndef BB_UI_LAYOUT_H
#define BB_UI_LAYOUT_H
#include <stdint.h>
#define UI_BACKGROUND 0xff142334u
typedef struct {
 int width,height,wide,interlaced;
 int left,top,right,bottom,scale,title_scale,advance,line,gap;
 int body_top,body_bottom,columns,lines;
} ui_layout;
typedef struct {
 const char *title,*text;
 const char *const *items;
 int count,selected,page,question,yes;
} ui_view;
/* Called for every actual text line, also used by host geometry assertions. */
typedef void (*ui_observe)(void *,int,int,int,int,int,const char *);
typedef struct {
 uint32_t *pixels; int stride;
 ui_observe observe; void *user;
} ui_canvas;
typedef struct {int pages,page,first,last,selection_visible,text_lines,body_top,body_lines;} ui_result;
void ui_layout_init(ui_layout *,int width,int height,int wide,int interlaced);
void ui_filter_interlaced(ui_canvas *,int width,int height);
int ui_render(const ui_layout *,const ui_view *,ui_canvas *,ui_result *);
/* Geometry pattern: content_height rows of pattern, magenta below it. */
int ui_render_pattern(ui_canvas *,int width,int height,int content_height);
/* App-local correction preview. Text stays neutral; upper ramp is original. */
int ui_render_gamma(const ui_layout *,ui_canvas *,ui_result *,int value,int digit,const uint8_t ramp[256],int native_value);
#endif
