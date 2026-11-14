/* SPDX-License-Identifier: MIT. Four public getter imports, compact PS3 ABI.
 * This avoids the installed SDK's monolithic sysutil object, which otherwise
 * imports setters along with the getters. No firmware code or VSH offsets.
 */
 .section .rodata.sceResident,"a"
hd_video_library:
 .asciz "cellSysutil"
 .align 2

 .section .lib.stub,"a"
 .byte 44,0
 .short 1,9,4,0,0
 .long 0
 .long hd_video_library,hd_video_nids,hd_video_stubs,0,0,0,0

 .section .rodata.sceFNID,"a"
hd_video_nids:
 .long 0x887572d5,0x15b0b0cd,0x75bbb672,0x1e930eef

 .section .data.sceFStub,"aw"
hd_video_stubs:
 .long cellVideoOutGetState,cellVideoOutGetConfiguration
 .long cellVideoOutGetNumberOfDevice,cellVideoOutGetDeviceInfo

 .macro HD_GETTER name,index
 .section .opd,"aw"
 .align 2
 .global \name
 .type \name,@function
\name:
 .long .\name,.TOC.
 .size \name,8

 .text
 .align 2
 .global .\name
 .type .\name,@function
.\name:
 mflr 0
 stw 0,24(1)
 stw 2,40(1)
 lis 12,hd_video_stubs@ha
 lwz 12,hd_video_stubs@l+\index*4(12)
 lwz 0,0(12)
 lwz 2,4(12)
 mtctr 0
 bctrl
 lwz 2,40(1)
 lwz 0,24(1)
 mtlr 0
 blr
 .size .\name,.-.\name
 .endm

 HD_GETTER cellVideoOutGetState,0
 HD_GETTER cellVideoOutGetConfiguration,1
 HD_GETTER cellVideoOutGetNumberOfDevice,2
 HD_GETTER cellVideoOutGetDeviceInfo,3
