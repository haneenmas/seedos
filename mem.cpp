#include "mem.hpp"
#include <vector>
#include <stdexcept>
#include <cstdint>

Memory::Memory(std::size_t bytes) : data(bytes, 0) {}

uint32_t Memory::load32(uint32_t addr) const {
    if (addr + 3 >= data.size()) throw std::out_of_range("load32 OOB");
    // little-endian
    return  (uint32_t)data[addr]
          | ((uint32_t)data[addr+1] << 8)
          | ((uint32_t)data[addr+2] << 16)
          | ((uint32_t)data[addr+3] << 24);
}

void Memory::store32(uint32_t addr, uint32_t value) {
    if (addr + 3 >= data.size()) throw std::out_of_range("store32 OOB");
    // little-endian
    data[addr]   = (uint8_t)(value & 0xFF);
    data[addr+1] = (uint8_t)((value >> 8)  & 0xFF);
    data[addr+2] = (uint8_t)((value >> 16) & 0xFF);
    data[addr+3] = (uint8_t)((value >> 24) & 0xFF);
}
