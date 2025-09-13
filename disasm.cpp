#include "disasm.hpp"
#include <sstream>
#include <iomanip>
#include <cstdint>

static inline uint32_t get_bits(uint32_t v,int pos,int len){ return (v>>pos) & ((1u<<len)-1u); }
static inline int32_t  sign_extend(uint32_t v,int bits){ uint32_t m=1u<<(bits-1); return (int32_t)((v^m)-m); }

std::string disasm(uint32_t inst){
    uint32_t opcode = get_bits(inst,0,7);
    std::ostringstream ss;

    if(opcode==0x13){ // OP-IMM
        uint32_t rd = get_bits(inst,7,5);
        uint32_t funct3 = get_bits(inst,12,3);
        uint32_t rs1 = get_bits(inst,15,5);
        if(funct3==0b000){ // ADDI
            int32_t imm = sign_extend(get_bits(inst,20,12),12);
            ss<<"addi x"<<rd<<", x"<<rs1<<", "<<imm;
        } else {
            ss<<"op-imm(?)";
        }
    } else if(opcode==0x33){ // R-type
        uint32_t rd   = get_bits(inst,7,5);
        uint32_t funct3 = get_bits(inst,12,3);
        uint32_t rs1  = get_bits(inst,15,5);
        uint32_t rs2  = get_bits(inst,20,5);
        uint32_t funct7 = get_bits(inst,25,7);
        if(funct3==0b000 && funct7==0b0000000){
            ss<<"add x"<<rd<<", x"<<rs1<<", x"<<rs2;
        } else {
            ss<<"r-type(?)";
        }
    } else if(opcode==0x37){ // LUI
        uint32_t rd = get_bits(inst,7,5);
        uint32_t imm20 = get_bits(inst,12,20);
        ss<<"lui x"<<rd<<", 0x"<<std::hex<<imm20<<std::dec;
    } else {
        ss<<"unknown(0x"<<std::hex<<inst<<std::dec<<")";
    }
    return ss.str();
}
