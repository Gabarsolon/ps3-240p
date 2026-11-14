/* Native VSH display interfaces, resolved by export NID. */
#ifndef VSH_VIDEO_H
#define VSH_VIDEO_H
#include <stdint.h>
/* Resolve an existing VSH export descriptor; zero on validation failure. */
uint32_t vsh_video_export(const char *library,uint32_t nid);
int vsh_video_copy(uint32_t address,void *out,unsigned length);
typedef struct {
 uint8_t raw[16];
 uint32_t flags,mode,format,pitch;
 int state;
} vsh_video_state;
int vsh_video_bind(void);
int vsh_video_query(vsh_video_state *);
int vsh_video_map(unsigned resolution,unsigned aspect,uint32_t *flags,uint32_t *mode);
int vsh_video_configure(unsigned resolution,unsigned aspect);
/* Experimental HD calls are separate from the stable SD corrector. */
int vsh_video_hd720(uint32_t *,uint32_t *);
int vsh_video_hd_submit(const vsh_video_state *,uint32_t,uint32_t);
#endif
