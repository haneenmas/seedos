#include "cpu.hpp"
#include "mem.hpp"
#include <cstdint>

// ---------- helpers ---------------------------------------------------------

// Extract 'len' bits starting at bit 'pos' (0 = least-significant bit).
static inline uint32_t get_bits(uint32_t v, int pos, int len) {
    return (v >> pos) & ((1u << len) - 1u);
}

// Sign-extend a value that is currently 'bits' wide to 32-bit two's complement.
static inline int32_t sign_extend(uint32_t v, int bits) {
    uint32_t m = 1u << (bits - 1);
    return (int32_t)((v ^ m) - m);
}

// ---------- CPU: fetch -> decode -> execute one instruction -----------------

bool CPU::step(Memory& mem) {
    // 1) FETCH (fixed 32-bit instruction)
    uint32_t inst = mem.load32(pc);

    // 2) DECODE fields common across formats
    uint32_t opcode = get_bits(inst, 0, 7);   // 6..0
    uint32_t rd     = get_bits(inst, 7, 5);   // 11..7
    uint32_t funct3 = get_bits(inst, 12, 3);  // 14..12
    uint32_t rs1    = get_bits(inst, 15, 5);  // 19..15

    // 3) EXECUTE by opcode
    if (opcode == 0x13) {
        // I-type: OP-IMM ---> ADDI
        if (funct3 == 0b000) {                // ADDI
            int32_t imm = sign_extend(get_bits(inst, 20, 12), 12); // 31..20
            uint32_t res = (uint32_t)((int32_t)x[rs1] + imm);
            if (rd != 0) x[rd] = res;
            pc += 4;
        } else {
            return false; // other OP-IMM later
        }

    } else if (opcode == 0x33) {
        // R-type: ADD/SUB (selected by funct7)
        uint32_t rs2    = get_bits(inst, 20, 5);
        uint32_t funct7 = get_bits(inst, 25, 7);

        if (funct3 == 0b000 && funct7 == 0b0000000) {       // ADD
            uint32_t res = x[rs1] + x[rs2];
            if (rd != 0) x[rd] = res;
            pc += 4;

        } else if (funct3 == 0b000 && funct7 == 0b0100000) { // SUB
            uint32_t res = x[rs1] - x[rs2];
            if (rd != 0) x[rd] = res;
            pc += 4;

        } else {
            return false; // other R-type later
        }

    } else if (opcode == 0x37) {
        // U-type: LUI
        uint32_t imm20 = get_bits(inst, 12, 20);
        uint32_t value = imm20 << 12;
        if (rd != 0) x[rd] = value;
        pc += 4;

    } else if (opcode == 0x63) {
        // B-type: BEQ (branch if equal)
        uint32_t rs2     = get_bits(inst, 20, 5);
        uint32_t funct3b = funct3;

        // Rebuild split immediate: [12|10:5|4:1|11] then <<1 (LSB always 0)
        uint32_t i12   = get_bits(inst, 31, 1);
        uint32_t i10_5 = get_bits(inst, 25, 6);
        uint32_t i4_1  = get_bits(inst, 8, 4);
        uint32_t i11   = get_bits(inst, 7, 1);
        uint32_t imm13 = (i12 << 12) | (i11 << 11) | (i10_5 << 5) | (i4_1 << 1);
        int32_t  off   = sign_extend(imm13, 13); // signed byte offset

        if (funct3b == 0b000) { // BEQ
            if (x[rs1] == x[rs2]) {
                pc = pc + off;  // taken: PC-relative
            } else {
                pc += 4;        // not taken
            }
        } else {
            return false; // other branches later
        }

    } else if (opcode == 0x03) {
        // I-type loads: LW
        if (funct3 == 0b010) { // LW
            int32_t imm = sign_extend(get_bits(inst, 20, 12), 12);
            uint32_t ea = x[rs1] + (uint32_t)imm; // effective address
            uint32_t val = mem.load32(ea);
            if (rd != 0) x[rd] = val;
            pc += 4;
        } else {
            return false; // other loads later
        }

    } else if (opcode == 0x23) {
        // S-type stores: SW
        uint32_t imm11_5 = get_bits(inst, 25, 7);
        uint32_t imm4_0  = get_bits(inst, 7, 5);
        int32_t  imm     = sign_extend((imm11_5 << 5) | imm4_0, 12);
        uint32_t rs2     = get_bits(inst, 20, 5);

        if (funct3 == 0b010) { // SW
            uint32_t ea = x[rs1] + (uint32_t)imm;
            mem.store32(ea, x[rs2]);
            pc += 4;
        } else {
            return false; // other stores later
        }

    } else {
        return false; // unsupported opcode (for now)
    }

    x[0] = 0; // enforce hard-wired zero register
    return true;
}
