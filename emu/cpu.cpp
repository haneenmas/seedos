#include <cstdint>
#include <stdexcept>
#include "cpu.hpp"
#include "mem.hpp"
#include "disasm.hpp"
#include "syscall.hpp"

static inline uint32_t get_bits(uint32_t v, int pos, int len){
    return (v >> pos) & ((1u << len) - 1u);
}
static inline int32_t sign_extend(uint32_t v, int bits){
    uint32_t m = 1u << (bits - 1);
    return (int32_t)((v ^ m) - m);
}

bool CPU::step(Memory& mem)
{
    if (halted) return false;

    uint32_t inst = 0;
    try {
        inst = mem.load32(pc);
    } catch (const std::runtime_error&) {   // misaligned
        halted = true; exit_code = -1; last_trap = Trap::MisalignedLoad; return false;
    } catch (const std::out_of_range&) {    // access fault
        halted = true; exit_code = -1; last_trap = Trap::AccessFault;    return false;
    }

    const uint32_t opcode = inst & 0x7F;
    uint32_t next_pc = pc + 4;

    auto rd     = get_bits(inst, 7, 5);
    auto funct3 = get_bits(inst, 12, 3);
    auto rs1    = get_bits(inst, 15, 5);
    auto rs2    = get_bits(inst, 20, 5);
    auto funct7 = get_bits(inst, 25, 7);

    switch (opcode)
    {
    case 0x13: {  // OP-IMM
        int32_t imm = sign_extend(inst >> 20, 12);
        switch (funct3) {
        case 0b000: // ADDI
            if (rd != 0) x[rd] = (uint32_t)((int32_t)x[rs1] + imm);
            break;
        default:
            halted = true; exit_code = -1; last_trap = Trap::Illegal; break;
        }
        break;
    }
    case 0x33: {  // OP
        switch (funct3) {
        case 0b000:
            if (funct7 == 0x00) {
                if (rd != 0) x[rd] = x[rs1] + x[rs2];           // ADD
            } else if (funct7 == 0x20) {
                if (rd != 0) x[rd] = (uint32_t)((int32_t)x[rs1] - (int32_t)x[rs2]); // SUB
            } else {
                halted = true; exit_code = -1; last_trap = Trap::Illegal;
            }
            break;
        default:
            halted = true; exit_code = -1; last_trap = Trap::Illegal; break;
        }
        break;
    }
    case 0x37: {  // LUI
        uint32_t imm20 = inst & 0xFFFFF000u;
        if (rd != 0) x[rd] = imm20;
        break;
    }
    case 0x03: {  // LOAD
        int32_t imm = sign_extend(inst >> 20, 12);
        uint32_t addr = x[rs1] + (uint32_t)imm;
        if (funct3 == 0b010) { // LW
            try {
                uint32_t v = mem.load32(addr);
                if (rd != 0) x[rd] = v;
            } catch (const std::runtime_error&) { halted = true; exit_code = -1; last_trap = Trap::MisalignedLoad; }
              catch (const std::out_of_range&)  { halted = true; exit_code = -1; last_trap = Trap::AccessFault; }
        } else {
            halted = true; exit_code = -1; last_trap = Trap::Illegal;
        }
        break;
    }
    case 0x23: {  // STORE
        int32_t imm = (int32_t)(
            (get_bits(inst, 7, 5)) |
            (get_bits(inst, 25, 7) << 5)
        );
        imm = sign_extend((uint32_t)imm, 12);
        uint32_t addr = x[rs1] + (uint32_t)imm;
        if (funct3 == 0b010) { // SW
            try {
                mem.store32(addr, x[rs2]);
            } catch (const std::runtime_error&) { halted = true; exit_code = -1; last_trap = Trap::MisalignedStore; }
              catch (const std::out_of_range&)  { halted = true; exit_code = -1; last_trap = Trap::AccessFault; }
        } else {
            halted = true; exit_code = -1; last_trap = Trap::Illegal;
        }
        break;
    }
    case 0x63: {  // BRANCH
        uint32_t bimm =
            ((inst >> 7)  & 0x1)      << 11 |
            ((inst >> 25) & 0x3F)     << 5  |
            ((inst >> 8)  & 0xF)      << 1  |
            ((inst >> 31) & 0x1)      << 12;
        int32_t off = sign_extend(bimm, 13);

        bool take = false;
        switch (funct3) {
        case 0b000: take = (x[rs1] == x[rs2]); break; // BEQ
        case 0b001: take = (x[rs1] != x[rs2]); break; // BNE
        case 0b100: take = ((int32_t)x[rs1] <  (int32_t)x[rs2]); break; // BLT
        case 0b101: take = ((int32_t)x[rs1] >= (int32_t)x[rs2]); break; // BGE
        default: halted = true; exit_code = -1; last_trap = Trap::Illegal; break;
        }
        if (take) next_pc = pc + (uint32_t)off;
        break;
    }
    case 0x6F: {  // JAL
        uint32_t imm =
            ((inst >> 21) & 0x3FF) << 1  |
            ((inst >> 20) & 0x1)   << 11 |
            ((inst >> 12) & 0xFF)  << 12 |
            ((inst >> 31) & 0x1)   << 20;
        int32_t off = sign_extend(imm, 21);
        uint32_t ret = pc + 4;
        if (rd != 0) x[rd] = ret;
        next_pc = pc + (uint32_t)off;
        break;
    }
    case 0x67: {  // JALR
        int32_t imm = sign_extend(inst >> 20, 12);
        uint32_t ret = pc + 4;
        uint32_t t   = (x[rs1] + (uint32_t)imm) & ~1u;
        if (rd != 0) x[rd] = ret;
        next_pc = t;
        break;
    }
    case 0x73: {  // SYSTEM
        if (inst == 0x00000073u) {          // ECALL
            handle_ecall(*this, mem);
        } else if (inst == 0x00100073u) {   // EBREAK
            halted = true; exit_code = -1; last_trap = Trap::Breakpoint;
        } else {
            halted = true; exit_code = -1; last_trap = Trap::Illegal;
        }
        break;
    }
    default:
        halted = true; exit_code = -1; last_trap = Trap::Illegal;
        break;
    }

    x[0] = 0;
    pc = next_pc;

    instret++;
    cycles++;
    if (quantum > 0) --quantum;

    return !halted;
}
