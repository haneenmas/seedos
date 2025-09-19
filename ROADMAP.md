# Roadmap — seedos (tiny RV32I systems lab)

- [x] **ALU & branches**: ADD/SUB, SLL/SRL/SRA, SLT/SLTU, BEQ/BNE, BLT/BGE, BLTU/BGEU, JAL/JALR, LUI.
- [ ] **Syscalls & traps**: ECALL/EBREAK + tiny syscall table (a7 = id, a0/a1 args).
- [ ] **Counters**: cycles & instret; print at end to compare algorithms.
- [ ] **Allocator**: `sbrk` + first-fit free list; heap stats.
- [ ] **Scheduler (toy)**: timer “interrupt” that switches between two threads (save/restore regs).
- [ ] **Caches/Perf**: direct-mapped I/D cache with miss counts OR Sv32 + TLB.
- [ ] **Algorithms in guest**: quicksort/mergesort/BFS/Dijkstra; compare cycles & misses.
- [ ] **ELF loader**: run real RV32I binaries (static).
- [ ] **Test harness**: host asserts for known programs; CI runs them.

**Status note:** Step 1 is done (you can explain signed/unsigned compares, shifts, PC-relative branches, and JAL/JALR).
