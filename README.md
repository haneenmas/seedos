# seedos — Tiny RV32I Emulator with Debugger & MMIO  ![ci](https://img.shields.io/github/actions/workflow/status/haneenmas/seedos/ci.yml?branch=main)


## Architecture (bird’s-eye)



      +-------------------+
      |    seedos (app)   |
      |  demos + REPL UI  |
      +---------+---------+
                |
                v
        +---------------+        +-----------+
        |  CPU (RV32I)  |<------>|  Disasm   |
        +-------+-------+        +-----------+
                |                 (mirrors decoder)
                v
        +---------------+
        |   Memory      |——— MMIO 0x4000_0000 → UART (host stdout)
        +-------+-------+
                |
           sbrk/heap


**One-shot pitch.** A compact, readable RISC-V RV32I emulator you can **step**, **break**, and **poke** like real hardware. It blends computer architecture (decoder/disasm), OS ideas (syscalls, scheduler, traps), and tooling (Xcode/CMake, tests, CI). The goal: make every bit easy to explain to a recruiter or hiring manager.

![demo](docs/demo.gif)

---

## Highlights 
- **Devices:** Memory-mapped UART at `0x4000_0000` — storing a byte prints to host console.
- **Traps:** Illegal / misaligned / access fault; **EBREAK** software breakpoints (INT3-style).
- **Debugger REPL:** `c`(continue), `s`(step), `b`(toggle breakpoint), `r`(regs), `m`(mem), `d`(disasm).
- **Scheduling:** Preemptive **round-robin** with instruction-count time slices.
- **Syscalls:** Minimal ECALL shim (puti / putch / sbrk / cycles / exit).
- **Tests:** Unit tests for ADDI/SUB/branches and `sbrk`.
- **Tooling:** CMake + Xcode project generation, GitHub Actions CI.

---

## Quick start
```bash
git clone https://github.com/haneenmas/seedos
cd seedos
mkdir build && cd build
cmake -G Xcode -DCMAKE_OSX_DEPLOYMENT_TARGET=12.2 ..
# Either open in Xcode:
open seedos.xcodeproj
# Or build from terminal:
cmake --build . --config Debug
