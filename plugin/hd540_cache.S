/* SPDX-License-Identifier: MIT. Self-authored PPC64 ELFv1 wrapper.
 * 4.93 configure callback: same r2/arguments/return, no MMIO stores.
 * Fixed addresses are audited in docs/HD540_DEV15.md; no Sony code copied.
 */
.section .text
.balign 8
.global hd540_cache_payload
hd540_cache_payload:
    mflr 0
    std 0,16(1)
    stdu 1,-144(1)

    /* Record entry in module RW page padding, outside ELF p_memsz. */
    lis 11,0xc000
    ori 11,11,0xde00
    clrldi 11,11,32
    lwz 12,0(11)
    addi 12,12,1
    stw 12,0(11)
    li 12,1
    stw 12,4(11)

    /* Flush all four modified instruction lines in THIS domain/CPU. */
    lis 11,0x8000
    ori 11,11,0x2f48
    clrldi 11,11,32
    dcbst 0,11
    addi 11,11,0x110
    dcbst 0,11
    addi 11,11,0xa8
    dcbst 0,11
    addi 11,11,0x5c8
    dcbst 0,11
    sync
    icbi 0,11
    addi 11,11,-0x5c8
    icbi 0,11
    addi 11,11,-0xa8
    icbi 0,11
    addi 11,11,-0x110
    icbi 0,11
    sync
    isync

    /* Original callback has five register arguments; preserve r3..r10. */
    lis 12,0x8000
    ori 12,12,0xd8ec
    clrldi 12,12,32
    mtctr 12
    bctrl
    std 3,128(1)

    /* Existing native display context -> its verified mapped MMIO base.
     * Only read controls that native code already reads in RMW operations. */
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
    lwz 12,0x814(10)
    stw 12,16(11)
    lwz 12,0x818(10)
    stw 12,20(11)
    /* Native placement writes this vertical step; inspect its readback. */
    lwz 12,0x89c(10)
    stw 12,28(11)
    lwz 12,0x820(9)
    stw 12,32(11)
    lwz 12,0x210(9)
    stw 12,36(11)
    ld 3,128(1)
    stw 3,24(11)
    li 12,0
    sync
    stw 12,4(11)

    addi 1,1,144
    ld 0,16(1)
    mtlr 0
    blr
    .balign 8
