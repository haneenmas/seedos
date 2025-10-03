
# seedos — tiny RV32I emulator (educational)
## Architecture (bird’s eye)

![demo](docs/demo.gif)
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


A minimal, readable **RISC-V RV32I** emulator you can step through like a textbook.  
It ties together **computer architecture, OS concepts, data structures, algorithms, and compiler/ABI fundamentals**:

- **Instruction set implemented:** `ADDI`, `ADD`, `SUB`, `LUI`, `LW`, `SW`, `BEQ`, `BNE`, `BLT`, `BGE`, `JAL`, `JALR`.
- **PC-relative control flow:** correct split-immediate decoding + sign-extension for B/J formats.
- **Registers:** 32 general purpose (`x0..x31`), with `x0` hard-wired to 0.
- **Memory:** simple byte-addressable RAM (little-endian, 32-bit `load32`/`store32`).
- **Disassembler:** mirrors the decoder so every step prints a readable instruction trace.
- **Calling convention demo:** real stack frame using `sp (x2)`, `ra (x1)`, `s0/fp (x8)`, plus a local variable on the stack.

> Elevator pitch: “I implemented a tiny RV32I emulator with correct bitfield decoding, PC-relative jumps, a proper callee prologue/epilogue, and a mirrored disassembler so I can explain each state transition.”

---

## Directory layout


cd ~/code/seedos
git add emu/main.cpp
git commit -m "feat(stack): real callee prologue/epilogue; local variable at 4(sp); result=(a0+a1)+3"
git push
