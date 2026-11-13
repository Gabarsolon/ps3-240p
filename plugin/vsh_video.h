/* Native VSH display interfaces, resolved by export NID. */
#ifndef VSH_VIDEO_H
#define VSH_VIDEO_H
#include <stdint.h>
typedef struct {
 uint8_t raw[16];
 uint32_t flags,mode,format,pitch;
 int state;
} vsh_video_state;
int vsh_video_bind(void);
int vsh_video_query(vsh_video_state *);
int vsh_video_map(unsigned resolution,unsigned aspect,uint32_t *flags,uint32_t *mode);
int vsh_video_configure(unsigned resolution,unsigned aspect);
#endif
