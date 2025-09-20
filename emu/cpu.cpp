#include "cpu.hpp"
#include "mem.hpp"
#include <cstdint>
#include <iostream>

static inline uint32_t get_bits(uint32_t v,int pos,int len){ return (v>>pos)&((1u<<len)-1u); }
static inline int32_t  sign_extend(uint32_t v,int bits){ uint32_t m=1u<<(bits-1); return (int32_t)((v^m)-m); }

bool CPU::step(Memory& mem){
    if (halted) return false;

    uint32_t inst   = mem.load32(pc);
    uint32_t opcode = get_bits(inst,0,7);
    uint32_t rd     = get_bits(inst,7,5);
    uint32_t funct3 = get_bits(inst,12,3);
    uint32_t rs1    = get_bits(inst,15,5);

    uint32_t cost = 1; // base CPI=1

    if(opcode==0x13){ // OP-IMM (ADDI only here)
        if(funct3!=0b000) return false;
        int32_t imm = sign_extend(get_bits(inst,20,12),12);
        if(rd!=0) x[rd] = (uint32_t)((int32_t)x[rs1] + imm);
        pc += 4;

    } else if(opcode==0x33){ // R-type ALU subset
        uint32_t rs2    = get_bits(inst,20,5);
        uint32_t funct7 = get_bits(inst,25,7);
        if(funct3==0b000 && funct7==0)               { if(rd!=0) x[rd]=x[rs1]+x[rs2];             pc+=4; }
        else if(funct3==0b000 && funct7==0b0100000)  { if(rd!=0) x[rd]=x[rs1]-x[rs2];             pc+=4; }
        else if(funct3==0b001 && funct7==0)          { if(rd!=0) x[rd]=x[rs1]<<(x[rs2]&31u);      pc+=4; }
        else if(funct3==0b101 && funct7==0)          { if(rd!=0) x[rd]=x[rs1]>>(x[rs2]&31u);      pc+=4; }
        else if(funct3==0b101 && funct7==0b0100000)  { if(rd!=0) x[rd]=(uint32_t)((int32_t)x[rs1]>>(int)(x[rs2]&31u)); pc+=4; }
        else if(funct3==0b010 && funct7==0)          { if(rd!=0) x[rd]=((int32_t)x[rs1]<(int32_t)x[rs2])?1u:0u; pc+=4; }
        else if(funct3==0b011 && funct7==0)          { if(rd!=0) x[rd]=(x[rs1]<x[rs2])?1u:0u;     pc+=4; }
        else return false;

    } else if(opcode==0x37){ // LUI
        if(rd!=0) x[rd] = get_bits(inst,12,20)<<12; pc+=4;

    } else if(opcode==0x63){ // branches
        uint32_t rs2   = get_bits(inst,20,5);
        uint32_t i12   = get_bits(inst,31,1);
        uint32_t i10_5 = get_bits(inst,25,6);
        uint32_t i4_1  = get_bits(inst,8,4);
        uint32_t i11   = get_bits(inst,7,1);
        int32_t  off   = sign_extend((i12<<12)|(i11<<11)|(i10_5<<5)|(i4_1<<1),13);
        if     (funct3==0b000) pc = (x[rs1]==x[rs2]) ? pc+off : pc+4;
        else if(funct3==0b001) pc = (x[rs1]!=x[rs2]) ? pc+off : pc+4;
        else if(funct3==0b100) pc = ((int32_t)x[rs1] <  (int32_t)x[rs2]) ? pc+off : pc+4;
        else if(funct3==0b101) pc = ((int32_t)x[rs1] >= (int32_t)x[rs2]) ? pc+off : pc+4;
        else if(funct3==0b110) pc = (x[rs1] <  x[rs2]) ? pc+off : pc+4;
        else if(funct3==0b111) pc = (x[rs1] >= x[rs2]) ? pc+off : pc+4;
        else return false;

    } else if(opcode==0x03){ // LW
        if(funct3!=0b010) return false;
        int32_t imm = sign_extend(get_bits(inst,20,12),12);
        if(rd!=0) x[rd] = mem.load32(x[rs1] + (uint32_t)imm);
        pc+=4; cost+=2;

    } else if(opcode==0x23){ // SW
        uint32_t imm11_5 = get_bits(inst,25,7);
        uint32_t imm4_0  = get_bits(inst,7,5);
        int32_t  imm     = sign_extend((imm11_5<<5)|imm4_0,12);
        uint32_t rs2     = get_bits(inst,20,5);
        if(funct3!=0b010) return false;
        mem.store32(x[rs1] + (uint32_t)imm, x[rs2]);
        pc+=4; cost+=2;

    } else if(opcode==0x6F){ // JAL
        uint32_t i20=get_bits(inst,31,1), i10_1=get_bits(inst,21,10), i11=get_bits(inst,20,1), i19_12=get_bits(inst,12,8);
        int32_t  off=sign_extend((i20<<20)|(i19_12<<12)|(i11<<11)|(i10_1<<1),21);
        uint32_t ret=pc+4; pc=pc+off; if(rd!=0) x[rd]=ret; cost+=1;

    } else if(opcode==0x67){ // JALR
        if(funct3!=0b000) return false;
        int32_t imm=sign_extend(get_bits(inst,20,12),12);
        uint32_t ret=pc+4; uint32_t tgt=(x[rs1]+(uint32_t)imm)&~1u;
        pc=tgt; if(rd!=0) x[rd]=ret; cost+=1;

    } else if(opcode==0x73){ // SYSTEM
        uint32_t imm12 = get_bits(inst,20,12);
        if(funct3==0 && imm12==0){              // ECALL
            uint32_t id = x[17];                // a7
            uint32_t a0 = x[10];                // arg0/result
            uint32_t a1 = x[11];                // arg1
            switch(id){
                case 0: // exit(a0)
                    exit_code = a0; halted = true; break;
                case 1: // print_u32(a0)
                    std::cout << a0 << "\n"; break;
                case 2: // putchar(a0)
                    std::cout << (char)(a0 & 0xFF) << std::flush; break;
                case 3: { // sbrk(Δ)
                    int32_t delta = (int32_t)a0;
                    uint32_t old = mem.sbrk(delta);
                    x[10] = old;                // return old break in a0
                    break;
                }
                case 4: { // write_str(ptr,len)
                    uint32_t ptr = a0, len = a1;
                    for(uint32_t i=0;i<len;i++) std::cout << (char)mem.load8(ptr+i);
                    std::cout.flush();
                    break;
                }
                    
                    
                    
                    
                    
                case 5: { // malloc(bytes) -> a0 = ptr (or 0)
                    uint32_t bytes = a0;
                    uint32_t ptr = mem.malloc32(bytes);
                    x[10] = ptr;
                    break;
                }
                case 6: { // free(ptr)
                    uint32_t ptr = a0;
                    mem.free32(ptr);
                    break;
                }

                default:
                    std::cerr << "[ecall] unsupported id " << id << "\n";
                    halted = true; exit_code = (uint32_t)-1; break;
            }
            pc += 4;
        } else if(funct3==0 && imm12==1){       // EBREAK
            halted = true; pc += 4;
        } else return false;

    } else return false;

    x[0]=0;
    instret += 1;
    cycles  += cost;
    return true;
}
