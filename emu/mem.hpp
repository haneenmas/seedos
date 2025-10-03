#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <unordered_map>
#include <iostream>

class Memory {
public:
    explicit Memory(std::size_t bytes)
    : bytes_(bytes, 0), text_end_(0), heap_brk_(8192) {}  // keep your defaults

    // ------- loads/stores (host little-endian) -------
    uint32_t load32(uint32_t addr) const {
        if (addr & 3) throw std::runtime_error("load32 misaligned");
        if (addr + 3 >= size()) throw std::out_of_range("load32 OOB");
        // MMIO reads
        if (is_mmio(addr)) return mmio_read32(addr);
        return  (uint32_t)bytes_[addr]
              | (uint32_t)bytes_[addr+1] << 8
              | (uint32_t)bytes_[addr+2] << 16
              | (uint32_t)bytes_[addr+3] << 24;
    }

    void store32(uint32_t addr, uint32_t value) {
        if (addr & 3) throw std::runtime_error("store32 misaligned");
        if (addr + 3 >= size()) throw std::out_of_range("store32 OOB");
        // MMIO writes
        if (is_mmio(addr)) { mmio_write32(addr, value); return; }
        bytes_[addr]   = (uint8_t)(value & 0xFF);
        bytes_[addr+1] = (uint8_t)((value >> 8) & 0xFF);
        bytes_[addr+2] = (uint8_t)((value >> 16) & 0xFF);
        bytes_[addr+3] = (uint8_t)((value >> 24) & 0xFF);
    }

    uint8_t load8(uint32_t addr) const {
        if (addr >= size()) throw std::out_of_range("load8 OOB");
        // MMIO reads
        if (is_mmio(addr)) return (uint8_t)mmio_read8(addr);
        return bytes_[addr];
    }
    void store8(uint32_t addr, uint8_t v) {
        if (addr >= size()) throw std::out_of_range("store8 OOB");
        // MMIO writes
        if (is_mmio(addr)) { mmio_write8(addr, v); return; }
        bytes_[addr] = v;
    }

    // ------- heap (bump pointer) -------
    // sbrk(delta): move program break; return old break
    uint32_t sbrk(int32_t delta){
        uint32_t old = heap_brk_;
        int64_t  nb  = (int64_t)heap_brk_ + delta;
        if (nb < (int64_t)text_end_) nb = text_end_;
        if (nb < 0 || nb >= (int64_t)size()) throw std::out_of_range("sbrk OOB");
        heap_brk_ = (uint32_t)nb;
        return old;
    }

    // record where your .text ends (optional, used by sbrk guard)
    void set_text_end(uint32_t a){ text_end_ = a; }

    uint32_t size() const { return (uint32_t)bytes_.size(); }

    // ------- software breakpoints (EBREAK patch) -------
    // Toggle a breakpoint at aligned addr. Returns true if now enabled.
    bool toggle_break(uint32_t addr){
        if (addr & 3) throw std::runtime_error("breakpoint addr must be 4-byte aligned");
        if (addr + 3 >= size()) throw std::out_of_range("breakpoint OOB");
        auto it = bp_original_.find(addr);
        if (it == bp_original_.end()){
            uint32_t w = load32(addr);
            bp_original_[addr] = w;
            // EBREAK = 0x00100073
            store32(addr, 0x00100073u);
            return true;  // enabled
        } else {
            store32(addr, it->second);
            bp_original_.erase(it);
            return false; // disabled
        }
    }
    bool has_break(uint32_t addr) const {
        return bp_original_.count(addr) != 0;
    }

private:
    // ---------------- MMIO ----------------
    // UART TX @ 0x4000'0000 (one byte)
    static constexpr uint32_t UART_TX = 0x40000000u;

    bool is_mmio(uint32_t addr) const {
        return addr == UART_TX;
    }
    // writes: printing to host console
    void mmio_write8(uint32_t addr, uint8_t v) const {
        if (addr == UART_TX) {
            std::cout.put((char)v);
            std::cout.flush();
            return;
        }
    }
    void mmio_write32(uint32_t addr, uint32_t v) const {
        if (addr == UART_TX) {
            std::cout.put((char)(v & 0xFF));
            std::cout.flush();
            return;
        }
    }
    uint32_t mmio_read32(uint32_t /*addr*/) const { return 0; }
    uint32_t mmio_read8 (uint32_t /*addr*/) const { return 0; }

private:
    std::vector<uint8_t> bytes_;
    uint32_t text_end_;
    uint32_t heap_brk_;
    // breakpoint original words
    std::unordered_map<uint32_t,uint32_t> bp_original_;
};
