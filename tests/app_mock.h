/* Host declarations for the real app entry point and video/GCM paths.
 * Native states intentionally differ from the legacy compatibility macros. */
#ifndef APP_MOCK_H
#define APP_MOCK_H
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#define SYS_PROCESS_PARAM(a,b)
#define VIDEO_STATE_ENABLED 1
#define VIDEO_STATE_DISABLED 0
#define VIDEO_STATE_BUSY 3
enum {CELL_VIDEO_OUT_OUTPUT_STATE_ENABLED=0,CELL_VIDEO_OUT_OUTPUT_STATE_DISABLED=1,CELL_VIDEO_OUT_OUTPUT_STATE_PREPARING=2};
enum {VIDEO_ASPECT_4_3=1,VIDEO_ASPECT_16_9=2,VIDEO_SCANMODE_INTERLACE=0,VIDEO_BUFFER_FORMAT_XRGB=0};
enum {VIDEO_PORT_COMPOSITE=0x81,VIDEO_PORT_D=0x82,VIDEO_PORT_COMPONENT=0x83,VIDEO_PORT_RGB=0x84,VIDEO_PORT_SCART=0x85};
enum {CELL_SYSUTIL_REQUEST_EXITGAME=0x101,CELL_SYSUTIL_DRAWING_BEGIN=0x121,CELL_SYSUTIL_DRAWING_END=0x122,CELL_SYSUTIL_SYSTEM_MENU_OPEN=0x131,CELL_SYSUTIL_SYSTEM_MENU_CLOSE=0x132};
enum {SYS_O_WRONLY=1,SYS_O_CREAT=2,SYS_O_TRUNC=4,GCM_FLIP_VSYNC=0};
typedef struct {uint8_t resolution,scanMode,conversion,aspect,padding[2];uint16_t refreshRates;} videoDisplayMode;
typedef struct {uint8_t state,colorSpace,padding[6];videoDisplayMode displayMode;} videoState;
typedef struct {uint16_t width,height;} videoResolution;
typedef struct {uint8_t resolution,format,aspect,padding[9];uint32_t pitch;} videoConfiguration;
typedef struct {uint8_t portType;} videoDeviceInfo;
typedef struct {int unused;} CellGcmContextData;
extern CellGcmContextData mock_context;
int mock_uart_permission(uint64_t,uint64_t,uint64_t);
#define CELL_GCM_CURRENT (&mock_context)
typedef struct {unsigned char status[7];} padInfo;
typedef struct {int len,BTN_UP,BTN_DOWN,BTN_CROSS,BTN_CIRCLE,BTN_LEFT,BTN_RIGHT;} padData;
void *memalign(size_t,size_t);
void *rsxMemalign(size_t,size_t);
int usleep(unsigned);
uint64_t sys_time_get_system_time(void);
void sysProcessExit(int);
int sysLv2FsMkdir(const char *,int);
int sysLv2FsOpen(const char *,int,int *,int,void *,int);
int sysLv2FsWrite(int,const void *,uint64_t,uint64_t *);
int sysLv2FsFsync(int);
int sysLv2FsClose(int);
int cellSysutilRegisterCallback(int,void (*)(uint64_t,uint64_t,void *),void *);
int cellSysutilUnregisterCallback(int);
int cellSysutilCheckCallback(void);
int cellGcmInit(unsigned,unsigned,void *);
unsigned cellGcmGetFlipStatus(void);
void cellGcmResetFlipStatus(void);
void *cellGcmGetLabelAddress(unsigned);
void cellGcmSetWriteBackEndLabel(CellGcmContextData *,unsigned,uint32_t);
void cellGcmFlush(CellGcmContextData *);
int cellGcmAddressToOffset(void *,uint32_t *);
int cellGcmSetDisplayBuffer(uint8_t,uint32_t,uint32_t,uint32_t,uint32_t);
void cellGcmSetFlipMode(int);
int cellGcmSetFlip(CellGcmContextData *,uint8_t);
void cellGcmSetWaitFlip(CellGcmContextData *);
int videoGetState(int,int,videoState *);
int videoGetResolution(int,videoResolution *);
int videoGetDeviceInfo(int,int,videoDeviceInfo *);
int videoGetResolutionAvailability(int,unsigned,int,int);
int videoConfigure(int,videoConfiguration *,void *,int);
int ioPadInit(int);
int ioPadEnd(void);
int ioPadGetInfo(padInfo *);
int ioPadGetData(int,padData *);
#endif
