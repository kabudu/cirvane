/*
 * SPDX-FileCopyrightText: 2026 kabudu
 * SPDX-License-Identifier: MIT
 */

    .section .text.start, "ax", @progbits
    .align 4
    .globl _start
    .type _start, @function
_start:
    la sp, _stack_top
    la t0, _sbss
    la t1, _ebss
1:
    bgeu t0, t1, 2f
    sw zero, 0(t0)
    addi t0, t0, 4
    j 1b
2:
    la t0, _ebss
    la t1, _stack_top
    addi t1, t1, -256
    li t2, 0xA5A5A5A5
3:
    bgeu t0, t1, 4f
    sw t2, 0(t0)
    addi t0, t0, 4
    j 3b
4:
    la t0, cirvane_trap_entry
    andi t0, t0, -64
    ori t0, t0, 3
    csrw mtvec, t0
    call kernel_main
5:
    wfi
    j 5b
    .size _start, .-_start

    .align 6
    .globl cirvane_trap_entry
    .type cirvane_trap_entry, @function
cirvane_trap_entry:
    addi sp, sp, -16
    sw ra, 8(sp)
    sw t0, 4(sp)
    sw t1, 0(sp)
    csrr a0, mcause
    csrr a1, mepc
    call cirvane_trap
    csrw mepc, a0
    lw t1, 0(sp)
    lw t0, 4(sp)
    lw ra, 8(sp)
    addi sp, sp, 16
    mret
    .size cirvane_trap_entry, .-cirvane_trap_entry
