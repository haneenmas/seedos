#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

class Memory {
public:
    // Create a zero-initialized byte-addressable RAM of `bytes` size.
    explicit Memory(std::size_t bytes);

    // Load/store a 32-bit little-endian word at byte address `addr`.
    // Both functions throw std::out_of_range if the access would exceed RAM.
    uint32_t load32(uint32_t addr) const;
    void     store32(uint32_t addr, uint32_t value);

    // Small helper (not required, but handy for checks/tests).
    std::size_t size() const noexcept { return data.size(); }

private:
    // Backing store: contiguous bytes (little-endian interpretation done in .cpp).
    std::vector<uint8_t> data;
};
