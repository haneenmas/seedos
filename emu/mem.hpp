#pragma once
#include <cstdint>
#include <vector>

// Flat little-endian memory. Byte-addressable, safe bounds via .at()
struct Memory {
    std::vector<uint8_t> bytes;
    explicit Memory(size_t size) : bytes(size, 0) {}

    uint8_t  load8 (uint32_t a) const { return bytes.at(a); }
    uint32_t load32(uint32_t a) const {
        return  (uint32_t)bytes.at(a)
            | ((uint32_t)bytes.at(a+1) << 8)
            | ((uint32_t)bytes.at(a+2) << 16)
            | ((uint32_t)bytes.at(a+3) << 24);
    }
    void store8 (uint32_t a, uint8_t v)  { bytes.at(a) = v; }
    void store32(uint32_t a, uint32_t v) {
        bytes.at(a)   = (v      ) & 0xFF;
        bytes.at(a+1) = (v >> 8 ) & 0xFF;
        bytes.at(a+2) = (v >> 16) & 0xFF;
        bytes.at(a+3) = (v >> 24) & 0xFF;
    }
};

