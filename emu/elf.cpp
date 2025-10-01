#include "elf.hpp"
#include <fstream>
#include <cstring>
#include <iostream>

// Minimal ELF32 structures (only fields we use)
struct Elf32_Ehdr {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct Elf32_Phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

#ifndef EM_RISCV
#define EM_RISCV 243
#endif
#ifndef PT_LOAD
#define PT_LOAD 1
#endif

std::vector<uint8_t> read_file(const std::string& path){
    std::ifstream f(path, std::ios::binary);
    if(!f) throw std::runtime_error("open failed: "+path);
    f.seekg(0, std::ios::end);
    std::streamsize n = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf((size_t)std::max<long long>(0,n));
    if(n>0) f.read((char*)buf.data(), n);
    return buf;
}

static uint16_t u16le(const void* p){
    const uint8_t* b=(const uint8_t*)p; return (uint16_t)(b[0] | (b[1]<<8));
}
static uint32_t u32le(const void* p){
    const uint8_t* b=(const uint8_t*)p; return (uint32_t)(b[0] | (b[1]<<8) | (b[2]<<16) | (b[3]<<24));
}

uint32_t load_elf32_into_memory(const std::string& path, Memory& mem){
    auto file = read_file(path);
    if(file.size() < sizeof(Elf32_Ehdr)) throw std::runtime_error("ELF too small");

    const Elf32_Ehdr* eh = (const Elf32_Ehdr*)file.data();

    // Validate ELF ident
    const unsigned char* id = eh->e_ident;
    if(!(id[0]==0x7F && id[1]=='E' && id[2]=='L' && id[3]=='F'))
        throw std::runtime_error("bad ELF magic");
    if(id[4] != 1) throw std::runtime_error("not ELF32");
    if(id[5] != 1) throw std::runtime_error("not little-endian");
    if(id[6] != 1) throw std::runtime_error("bad version");

    uint16_t e_machine   = u16le(&eh->e_machine);
    uint32_t e_entry     = u32le(&eh->e_entry);
    uint32_t e_phoff     = u32le(&eh->e_phoff);
    uint16_t e_phentsize = u16le(&eh->e_phentsize);
    uint16_t e_phnum     = u16le(&eh->e_phnum);

    if(e_phoff + (uint32_t)e_phnum * e_phentsize > file.size())
        throw std::runtime_error("program headers out of range");

    if(e_machine != EM_RISCV){
        std::cerr << "[elf] warning: e_machine="<< e_machine <<" (expect " << EM_RISCV << ")\n";
    }

    // Map PT_LOAD segments
    for(uint16_t i=0;i<e_phnum;i++){
        const uint8_t* ph_ptr = file.data() + e_phoff + i*e_phentsize;
        uint32_t p_type   = u32le(ph_ptr+0);
        uint32_t p_offset = u32le(ph_ptr+4);
        uint32_t p_vaddr  = u32le(ph_ptr+8);
        uint32_t p_filesz = u32le(ph_ptr+16);
        uint32_t p_memsz  = u32le(ph_ptr+20);

        if(p_type != PT_LOAD) continue;
        if(p_offset + p_filesz > file.size())
            throw std::runtime_error("segment exceeds file size");

        // Copy file bytes
        for(uint32_t k=0;k<p_filesz;k++){
            mem.store8(p_vaddr + k, file[p_offset + k]);
        }
        // Zero BSS
        for(uint32_t k=p_filesz; k<p_memsz; k++){
            mem.store8(p_vaddr + k, 0);
        }
    }

    return e_entry;
}
