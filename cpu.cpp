#include "cpu.hpp"
#include "mem.hpp"
#include <cstdint>

// -------- helpers -----------------------------------------------------------

static inline uint32_t get_bits(uint32_t v, int pos, int len) {
    return (v >> pos) & ((1u << len) - 1u);
}

static inline int32_t sign_extend(uint32_t v, int bits) {
    uint32_t m = 1u << (bits - 1);
    return (int32_t)((v ^ m) - m);
}

// -------- fetch → decode → execute -----------------------------------------

bool CPU::step(Memory& mem) {
    // FETCH
    uint32_t inst = mem.load32(pc);

    // Common fields
    uint32_t opcode = get_bits(inst, 0, 7);
    uint32_t rd     = get_bits(inst, 7, 5);
    uint32_t funct3 = get_bits(inst, 12, 3);
    uint32_t rs1    = get_bits(inst, 15, 5);

    // EXECUTE
    if (opcode == 0x13) {
        // I-type: OP-IMM  (ADDI)
        if (funct3 == 0b000) {
            int32_t imm = sign_extend(get_bits(inst, 20, 12), 12);
            uint32_t res = (uint32_t)((int32_t)x[rs1] + imm);
            if (rd != 0) x[rd] = res;
            pc += 4;
        } else {
            return false;
        }

    } else if (opcode == 0x33) {
        // R-type: ADD / SUB
        uint32_t rs2    = get_bits(inst, 20, 5);
        uint32_t funct7 = get_bits(inst, 25, 7);

        if (funct3 == 0b000 && funct7 == 0b0000000) {        // ADD
            uint32_t res = x[rs1] + x[rs2];
            if (rd != 0) x[rd] = res;
            pc += 4;
        } else if (funct3 == 0b000 && funct7 == 0b0100000) { // SUB
            uint32_t res = x[rs1] - x[rs2];
            if (rd != 0) x[rd] = res;
            pc += 4;
        } else {
            return false;
        }

    } else if (opcode == 0x37) {
        // U-type: LUI
        uint32_t imm20 = get_bits(inst, 12, 20);
        uint32_t value = imm20 << 12;
        if (rd != 0) x[rd] = value;
        pc += 4;

    } else if (opcode == 0x63) {
        // B-type: BEQ
        uint32_t rs2   = get_bits(inst, 20, 5);
        uint32_t i12   = get_bits(inst, 31, 1);
        uint32_t i10_5 = get_bits(inst, 25, 6);
        uint32_t i4_1  = get_bits(inst, 8, 4);
        uint32_t i11   = get_bits(inst, 7, 1);
        uint32_t imm13 = (i12 << 12) | (i11 << 11) | (i10_5 << 5) | (i4_1 << 1);
        int32_t  off   = sign_extend(imm13, 13);  // byte offset (LSB is 0)

        if (funct3 == 0b000) { // BEQ
            if (x[rs1] == x[rs2]) pc = pc + off;  // taken (PC-relative)
            else                  pc += 4;        // not taken
        } else {
            return false;
        }

    } else if (opcode == 0x03) {
        // Loads: LW (I-type)
        if (funct3 == 0b010) {
            int32_t imm = sign_extend(get_bits(inst, 20, 12), 12);
            uint32_t ea  = x[rs1] + (uint32_t)imm;
            uint32_t val = mem.load32(ea);
            if (rd != 0) x[rd] = val;
            pc += 4;
        } else {
            return false;
        }

    } else if (opcode == 0x23) {
        // Stores: SW (S-type)
        uint32_t imm11_5 = get_bits(inst, 25, 7);
        uint32_t imm4_0  = get_bits(inst, 7, 5);
        int32_t  imm     = sign_extend((imm11_5 << 5) | imm4_0, 12);
        uint32_t rs2     = get_bits(inst, 20, 5);

        if (funct3 == 0b010) {
            uint32_t ea = x[rs1] + (uint32_t)imm;
            mem.store32(ea, x[rs2]);
            pc += 4;
        } else {
            return false;
        }

    } else if (opcode == 0x6F) {
        // J-type: JAL (jump and link)
        uint32_t i20     = get_bits(inst, 31, 1);
        uint32_t i10_1   = get_bits(inst, 21, 10);
        uint32_t i11     = get_bits(inst, 20, 1);
        uint32_t i19_12  = get_bits(inst, 12, 8);
        uint32_t imm21   = (i20 << 20) | (i19_12 << 12) | (i11 << 11) | (i10_1 << 1);
        int32_t  off     = sign_extend(imm21, 21);

        uint32_t ret = pc + 4;     // return address
        pc = pc + off;             // jump target (PC-relative)
        if (rd != 0) x[rd] = ret;

    } else if (opcode == 0x67) {
        // I-type: JALR (jump and link register)  funct3=000
        if (funct3 != 0b000) return false;
        int32_t imm   = sign_extend(get_bits(inst, 20, 12), 12);
        uint32_t ret  = pc + 4;
        uint32_t tgt  = (x[rs1] + (uint32_t)imm) & ~1u;  // force alignment
        pc = tgt;
        if (rd != 0) x[rd] = ret;

    } else {
        return false; // unsupported for now
    }

    x[0] = 0; // x0 hard-wired to zero
    return true;
}
