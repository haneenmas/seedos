    .option norvc            # RV32I only (no compressed)
    .text
    .globl _start
_start:
    addi x5, x0, 5           # x5 = 5
    addi x4, x0, 12          # x4 = 12
    add  x7, x4, x5          # x7 = 17  (visible in your dumps)
    li   a7, 0               # our emulator: ECALL 0 = exit
    ecall
