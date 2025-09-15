#include "disasm.hpp"
#include <sstream>
#include <iomanip>
#include <string>
#include <cstdint>

static inline uint32_t get_bits(uint32_t v, int pos, int len){ return (v>>pos)&((1u<<len)-1u); }
static inline int32_t  sign_extend(uint32_t v,int bits){ uint32_t m=1u<<(bits-1); return (int32_t)((v^m)-m); }

std::string disasm(uint32_t inst){
    uint32_t opcode = get_bits(inst,0,7);
    std::ostringstream ss;

    if(opcode==0x13){ // OP-IMM (ADDI)
        uint32_t rd=get_bits(inst,7,5), f3=get_bits(inst,12,3), rs1=get_bits(inst,15,5);
        if(f3==0b000){ int32_t imm=sign_extend(get_bits(inst,20,12),12);
            ss<<"addi x"<<rd<<", x"<<rs1<<", "<<imm;
        } else ss<<"op-imm(?)";

    } else if(opcode==0x33){ // R-type (ADD/SUB)
        uint32_t rd=get_bits(inst,7,5), f3=get_bits(inst,12,3), rs1=get_bits(inst,15,5), rs2=get_bits(inst,20,5), f7=get_bits(inst,25,7);
        if(f3==0b000 && f7==0b0000000) ss<<"add x"<<rd<<", x"<<rs1<<", x"<<rs2;
        else if(f3==0b000 && f7==0b0100000) ss<<"sub x"<<rd<<", x"<<rs1<<", x"<<rs2;
        else ss<<"r-type(?)";

    } else if(opcode==0x37){ // LUI
        uint32_t rd=get_bits(inst,7,5), imm20=get_bits(inst,12,20);
        ss<<"lui x"<<rd<<", 0x"<<std::hex<<imm20<<std::dec;

    } else if(opcode==0x63){ // BEQ
        uint32_t rs1=get_bits(inst,15,5), rs2=get_bits(inst,20,5), f3=get_bits(inst,12,3);
        uint32_t i12=get_bits(inst,31,1), i10_5=get_bits(inst,25,6), i4_1=get_bits(inst,8,4), i11=get_bits(inst,7,1);
        int32_t off=sign_extend((i12<<12)|(i11<<11)|(i10_5<<5)|(i4_1<<1),13);
        if(f3==0b000){ std::ostringstream d; d<<std::showpos<<off<<std::noshowpos; ss<<"beq x"<<rs1<<", x"<<rs2<<", "<<d.str(); }
        else ss<<"branch(?)";

    } else if(opcode==0x03){ // loads (LW)
        uint32_t rd=get_bits(inst,7,5), f3=get_bits(inst,12,3), rs1=get_bits(inst,15,5);
        if(f3==0b010){ int32_t imm=sign_extend(get_bits(inst,20,12),12);
            ss<<"lw x"<<rd<<", "<<imm<<"(x"<<rs1<<")";
        } else ss<<"load(?)";

    } else if(opcode==0x23){ // stores (SW)
        uint32_t f3=get_bits(inst,12,3), rs1=get_bits(inst,15,5), rs2=get_bits(inst,20,5), i11_5=get_bits(inst,25,7), i4_0=get_bits(inst,7,5);
        int32_t imm=sign_extend((i11_5<<5)|i4_0,12);
        if(f3==0b010) ss<<"sw x"<<rs2<<", "<<imm<<"(x"<<rs1<<")";
        else ss<<"store(?)";

    } else if(opcode==0x6F){ // JAL
        uint32_t rd=get_bits(inst,7,5);
        uint32_t i20=get_bits(inst,31,1), i10_1=get_bits(inst,21,10), i11=get_bits(inst,20,1), i19_12=get_bits(inst,12,8);
        int32_t off=sign_extend((i20<<20)|(i19_12<<12)|(i11<<11)|(i10_1<<1),21);
        std::ostringstream d; d<<std::showpos<<off<<std::noshowpos;
        ss<<"jal x"<<rd<<", "<<d.str();

    } else if(opcode==0x67){ // JALR
        uint32_t rd=get_bits(inst,7,5), rs1=get_bits(inst,15,5), f3=get_bits(inst,12,3);
        if(f3!=0b000) ss<<"jalr(?)";
        else { int32_t imm=sign_extend(get_bits(inst,20,12),12);
            ss<<"jalr x"<<rd<<", x"<<rs1<<", "<<imm;
        }

    } else {
        ss<<"unknown(0x"<<std::hex<<inst<<std::dec<<")";
    }
    return ss.str();
}
