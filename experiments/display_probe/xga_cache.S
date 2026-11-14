/* SPDX-License-Identifier: MIT. Self-authored PPC64 ELFv1 wrapper.
 * Reuses the checked callback/arena lifecycle from hd540_cache.c. Flush all
 * seven XGA geometry/policy instruction lines in sysmgr's own CPU/domain. Only
 * read native display registers after the original callback; no MMIO stores.
 * An explicitly armed, exact-pattern source correction changes only temporary
 * request RAM, and restores its original fields immediately after the call.
 */
.section .text
.balign 8
.global xga_cache_payload
xga_cache_payload:
    mflr 0
    std 0,16(1)
    stdu 1,-192(1)
    li 12,0
    stw 12,152(1)
    lis 11,0xc000
    ori 11,11,0xde00
    clrldi 11,11,32
    lwz 12,0(11)
    addi 12,12,1
    stw 12,0(11)
    li 12,1
    stw 12,4(11)

    /* Read only the first bounded video record. Native callback length is
     * r6, packet pointer r4; no caller argument is changed. */
    cmplwi 7,6,52
    blt 7,1f
    lhz 12,8(4)
    cmpwi 7,12,0
    beq 7,1f
    stw 12,48(11)
    stw 6,52(11)
    lhz 12,18(4)
    cmplwi 7,12,32
    blt 7,1f
    lwz 12,24(4)
    stw 12,56(11)
    lwz 12,32(4)
    stw 12,60(11)
    lwz 12,36(4)
    stw 12,64(11)
    lwz 12,40(4)
    stw 12,68(11)
    lwz 12,44(4)
    stw 12,72(11)
    lwz 12,48(4)
    stw 12,76(11)
    li 12,0
    stw 12,80(11)
    stw 12,84(11)
    /* Only native-XGA control may normalize the observed inconsistent
     * head0 / VID13 / ARGB /1280x720 /4096-byte source tuple. */
    lwz 12,96(11)
    xoris 12,12,0x5847
    cmpwi 7,12,0x4139
    bne 7,1f
    lwz 12,100(11)
    cmpwi 7,12,4096
    bne 7,1f
    lhz 12,8(4)
    cmpwi 7,12,1
    bne 7,1f
    lwz 12,24(4)
    cmpwi 7,12,0
    bne 7,1f
    lwz 12,32(4)
    cmpwi 7,12,13
    bne 7,1f
    lwz 12,44(4)
    cmpwi 7,12,4096
    bne 7,1f
    lwz 12,48(4)
    cmpwi 7,12,0
    bne 7,1f
    lwz 12,36(4)
    cmpwi 7,12,1280
    bne 7,1f
    stw 12,144(1)
    lwz 12,40(4)
    cmpwi 7,12,720
    bne 7,1f
    stw 12,148(1)
    std 4,136(1)
    li 12,1
    stw 12,152(1)
    stw 12,80(11)
    li 12,1024
    stw 12,36(4)
    li 12,768
    stw 12,40(4)
    sync
1:

    lis 11,0x8000
    ori 11,11,0x2d00
    clrldi 11,11,32
    dcbst 0,11
    addi 11,11,0x100
    dcbst 0,11
    addi 11,11,0x180
    dcbst 0,11
    addi 11,11,0x80
    dcbst 0,11
    addi 11,11,0x80
    dcbst 0,11
    addi 11,11,0x80
    dcbst 0,11
    lis 11,0x8002
    ori 11,11,0x5f80
    clrldi 11,11,32
    dcbst 0,11
    sync
    icbi 0,11
    lis 11,0x8000
    ori 11,11,0x3100
    clrldi 11,11,32
    icbi 0,11
    addi 11,11,-0x80
    icbi 0,11
    addi 11,11,-0x80
    icbi 0,11
    addi 11,11,-0x80
    icbi 0,11
    addi 11,11,-0x180
    icbi 0,11
    addi 11,11,-0x100
    icbi 0,11
    sync
    isync

    /* Preserve r3..r10 and the original callback's TOC. */
    lis 12,0x8000
    ori 12,12,0xd8ec
    clrldi 12,12,32
    mtctr 12
    bctrl
    std 3,128(1)

    /* The native path consumes/copies the video record synchronously.
     * Return the caller's packet fields exactly as they arrived. */
    lwz 12,152(1)
    cmpwi 7,12,0
    beq 7,2f
    ld 11,136(1)
    lwz 12,144(1)
    stw 12,36(11)
    lwz 12,148(1)
    stw 12,40(11)
    sync
    lwz 12,36(11)
    lwz 0,144(1)
    cmpw 7,12,0
    bne 7,2f
    lwz 12,40(11)
    lwz 0,148(1)
    cmpw 7,12,0
    bne 7,2f
    lis 11,0xc000
    ori 11,11,0xde00
    clrldi 11,11,32
    li 12,1
    stw 12,84(11)
2:

    lis 11,0xc000
    ori 11,11,0xb440
    clrldi 11,11,32
    ld 11,0(11)
    ld 11,0(11)
    ld 10,32(11)
    ld 9,24(11)
    addis 9,9,0x60
    addis 10,10,0x68
    lis 11,0xc000
    ori 11,11,0xde00
    clrldi 11,11,32
    lwz 12,0x848(10)
    stw 12,8(11)
    lwz 12,0x880(10)
    stw 12,12(11)
    lwz 12,0x834(10)
    stw 12,16(11)
    lwz 12,0x838(10)
    stw 12,20(11)
    lwz 12,0x89c(10)
    stw 12,28(11)
    lwz 12,0x814(10)
    stw 12,32(11)
    lwz 12,0x818(10)
    stw 12,36(11)
    lwz 12,0x898(10)
    stw 12,40(11)
    lwz 12,0x200(9)
    stw 12,44(11)
    ld 3,128(1)
    stw 3,24(11)
    li 12,0
    sync
    stw 12,4(11)
    addi 1,1,192
    ld 0,16(1)
    mtlr 0
    blr
    .balign 8
