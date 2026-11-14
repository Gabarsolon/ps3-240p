/* SPDX-License-Identifier: MIT. Only allowlisted syscall-8 queries and getters. */
#include "diagnostic.h"
#ifndef HD_PUBLIC_ONLY
#include <ppu-lv2.h>
#endif
#include <sysutil/video.h>

/* Kept out of line so the compiled transport has one auditable sc-0 site. */
#ifndef HD_PUBLIC_ONLY
uint64_t __attribute__((noinline)) hd_native_raw(void *user,uint64_t op,uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e)
{
 (void)user;
 lv2syscall6(8,op,a,b,c,d,e);return_to_user_prog(uint64_t);
}
#endif
static hd_mode copy_mode(const videoDisplayMode *m)
{
 hd_mode out={m->resolution,m->scanMode,m->conversion,m->aspect,m->refreshRates};return out;
}
void hd_native_video(void *user,hd_video *out)
{
 (void)user;
 videoState state={0};videoConfiguration cfg={0};videoDeviceInfo info={0};
 out->state_rc=videoGetState(0,0,&state);
 if(!out->state_rc){out->state=state.state;out->color_space=state.colorSpace;out->current=copy_mode(&state.displayMode);}
 out->configuration_rc=videoGetConfiguration(0,&cfg,NULL);
 if(!out->configuration_rc){out->configuration_resolution=cfg.resolution;out->configuration_format=cfg.format;out->configuration_aspect=cfg.aspect;out->pitch=cfg.pitch;}
 out->device_count=videoGetNumberOfDevice(0);
 if(out->device_count<=0)return;
 out->device_rc=videoGetDeviceInfo(0,0,&info);
 if(out->device_rc)return;
 out->port=info.portType;out->device_state=info.state;out->device_color_space=info.colorSpace;out->rgb_range=info.rgbOutputRange;
 out->reported_modes=info.availableModeCount;out->mode_count=info.availableModeCount<32?info.availableModeCount:32;
 for(unsigned i=0;i<out->mode_count;i++)out->modes[i]=copy_mode(&info.availableModes[i]);
 /* No KSV/HDCP identifiers, EDID bytes, account data or device IDs logged. */
}
