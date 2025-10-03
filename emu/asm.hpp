// emu/asm.hpp
#pragma once
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <cctype>
#include <cstdint>
#include <stdexcept>
#include "mem.hpp"

// ---- small helpers ----
inline uint32_t encI(uint8_t rd,uint8_t rs1,int32_t imm12,uint8_t f3=0){
    uint32_t u = (uint32_t)(imm12 & 0xFFF);
    return (u<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|0x13;
}
inline uint32_t encR(uint8_t rd,uint8_t rs1,uint8_t rs2,uint8_t f3,uint8_t f7){
    return (f7<<25)|(rs2<<20)|(rs1<<15)|(f3<<12)|(rd<<7)|0x33;
}
inline uint32_t encLUI(uint8_t rd,uint32_t imm20){ return ((imm20&0xFFFFF)<<12)|(rd<<7)|0x37; }
inline uint32_t encLW (uint8_t rd,uint8_t rs1,int32_t imm){ return ((imm&0xFFF)<<20)|(rs1<<15)|(2<<12)|(rd<<7)|0x03; }
inline uint32_t encSW (uint8_t rs2,uint8_t rs1,int32_t imm){
    uint32_t imm11_5=((imm>>5)&0x7F), imm4_0=(imm&0x1F);
    return (imm11_5<<25)|(rs2<<20)|(rs1<<15)|(2<<12)|(imm4_0<<7)|0x23;
}
inline uint32_t encB  (uint8_t rs1,uint8_t rs2,int32_t off,uint8_t f3){
    // off must be 13-bit signed and even
    uint32_t u=(uint32_t)off;
    uint32_t imm = ((u>>12)&1)<<31 | ((u>>5)&0x3F)<<25 | ((u>>1)&0xF)<<8 | ((u>>11)&1)<<7;
    return imm | (rs2<<20)|(rs1<<15)|(f3<<12)|0x63;
}
inline uint32_t encJAL(uint8_t rd,int32_t off){
    uint32_t u=(uint32_t)off;
    uint32_t imm = ((u>>20)&1)<<31 | ((u>>1)&0x3FF)<<21 | ((u>>11)&1)<<20 | ((u>>12)&0xFF)<<12;
    return imm | (rd<<7) | 0x6F;
}
inline uint32_t encJALR(uint8_t rd,uint8_t rs1,int32_t imm){ return ((imm&0xFFF)<<20)|(rs1<<15)|(0<<12)|(rd<<7)|0x67; }
inline uint32_t ECALL(){ return 0x00000073u; }
inline uint32_t EBREAK(){ return 0x00100073u; }

inline int32_t parse_int(const std::string& s){
    int base=10; size_t i=0; bool neg=false;
    if(s.size()>1 && s[0]=='-'){ neg=true; i=1; }
    if(i+1<s.size() && s[i]=='0' && (s[i+1]=='x'||s[i+1]=='X')) { base=16; i+=2; }
    int64_t v = std::stoll(s.substr(i), nullptr, base);
    return (int32_t)(neg ? -v : v);
}
inline std::string trim(const std::string& s){
    size_t a=0,b=s.size();
    while(a<b && std::isspace((unsigned char)s[a])) ++a;
    while(b>a && std::isspace((unsigned char)s[b-1])) --b;
    return s.substr(a,b-a);
}
inline std::vector<std::string> split_tokens(const std::string& line){
    std::vector<std::string> out; std::string cur;
    auto flush=[&]{ if(!cur.empty()){ out.push_back(cur); cur.clear(); } };
    for(char c: line){
        if(c==','||std::isspace((unsigned char)c)){ flush(); }
        else cur.push_back(c);
    }
    flush(); return out;
}
inline uint8_t reg_of(const std::string& r){
    if(!r.empty() && r[0]=='x'){ return (uint8_t)std::stoi(r.substr(1)); }
    static const std::unordered_map<std::string,int> abi = {
        {"zero",0},{"ra",1},{"sp",2},{"gp",3},{"tp",4},
        {"t0",5},{"t1",6},{"t2",7},
        {"s0",8},{"fp",8},{"s1",9},
        {"a0",10},{"a1",11},{"a2",12},{"a3",13},{"a4",14},{"a5",15},{"a6",16},{"a7",17},
        {"s2",18},{"s3",19},{"s4",20},{"s5",21},{"s6",22},{"s7",23},{"s8",24},{"s9",25},{"s10",26},{"s11",27},
        {"t3",28},{"t4",29},{"t5",30},{"t6",31}
    };
    auto it = abi.find(r);
    if(it!=abi.end()) return (uint8_t)it->second;
    throw std::runtime_error("bad register: "+r);
}

// Assemble `src` into memory at `base`. Returns number of emitted instructions.
inline int assemble_to_memory(const std::string& src, Memory& mem, uint32_t base, std::string* err=nullptr){
    std::vector<std::string> lines;
    {   // split lines, strip comments ';' or '#'
        std::istringstream is(src); std::string L;
        while(std::getline(is, L)){
            auto pos = L.find_first_of(";#");
            if(pos!=std::string::npos) L = L.substr(0,pos);
            lines.push_back(trim(L));
        }
    }

    // pass 1: collect labels
    std::unordered_map<std::string,uint32_t> label;
    uint32_t pc = base;
    for(auto& L : lines){
        if(L.empty()) continue;
        if(L.back()==':'){ label[trim(L.substr(0,L.size()-1))]=pc; continue; }
        // non-label line -> one instruction
        pc += 4;
    }

    // pass 2: encode
    pc = base; int emitted=0;
    try{
        for(auto& L : lines){
            if(L.empty()) continue;
            if(L.back()==':') continue; // label def
            auto toks = split_tokens(L);
            if(toks.empty()) continue;
            std::string op = toks[0];

            auto at = [&](size_t i)->std::string{
                if(i>=toks.size()) throw std::runtime_error("missing operand: "+op);
                return toks[i];
            };

            uint32_t word = 0;

            if(op=="addi"){
                uint8_t rd=reg_of(at(1)), rs1=reg_of(at(2));
                int32_t imm=parse_int(at(3));
                word = encI(rd,rs1,imm,0);
            } else if(op=="add"||op=="sub"){
                uint8_t rd=reg_of(at(1)), rs1=reg_of(at(2)), rs2=reg_of(at(3));
                word = encR(rd,rs1,rs2,0, op=="add"?0:0b0100000);
            } else if(op=="lui"){
                uint8_t rd=reg_of(at(1));
                uint32_t imm = (uint32_t)parse_int(at(2));
                word = encLUI(rd, imm);
            } else if(op=="lw"){
                // lw rd, imm(rs1)
                uint8_t rd=reg_of(at(1));
                auto memop = at(2);
                auto l = memop.find('('), r = memop.find(')');
                int32_t imm = parse_int(memop.substr(0,l));
                uint8_t rs1 = reg_of(memop.substr(l+1, r-l-1));
                word = encLW(rd,rs1,imm);
            } else if(op=="sw"){
                // sw rs2, imm(rs1)
                uint8_t rs2=reg_of(at(1));
                auto memop = at(2);
                auto l = memop.find('('), r = memop.find(')');
                int32_t imm = parse_int(memop.substr(0,l));
                uint8_t rs1 = reg_of(memop.substr(l+1, r-l-1));
                word = encSW(rs2,rs1,imm);
            } else if(op=="beq"||op=="bne"||op=="blt"||op=="bge"){
                uint8_t rs1=reg_of(at(1)), rs2=reg_of(at(2));
                std::string tgt = at(3);
                int32_t off;
                if(label.count(tgt)){
                    off = (int32_t)label[tgt] - (int32_t)pc;
                } else {
                    off = parse_int(tgt);
                }
                uint8_t f3 = (op=="beq")?0 : (op=="bne")?1 : (op=="blt")?4 : 5;
                word = encB(rs1,rs2,off,f3);
            } else if(op=="jal"){
                // jal rd,label  OR jal label (rd=ra)
                uint8_t rd=1; size_t idx=1;
                if(toks.size()==3){ rd=reg_of(at(1)); idx=2; }
                std::string tgt = at(idx);
                int32_t off;
                if(label.count(tgt)){
                    off = (int32_t)label[tgt] - (int32_t)pc;
                } else {
                    off = parse_int(tgt);
                }
                word = encJAL(rd, off);
            } else if(op=="jalr"){
                // jalr rd, rs1, imm
                uint8_t rd=reg_of(at(1)), rs1=reg_of(at(2));
                int32_t imm = parse_int(at(3));
                word = encJALR(rd, rs1, imm);
            } else if(op=="ebreak"){
                word = EBREAK();
            } else if(op=="ecall"){
                word = ECALL();
            } else {
                throw std::runtime_error("unknown op: "+op);
            }

            mem.store32(pc, word);
            pc += 4; emitted++;
        }
    } catch(const std::exception& e){
        if(err) *err = e.what();
        throw;
    }
    return emitted;
}
