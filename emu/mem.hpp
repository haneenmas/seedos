#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <algorithm>

class Memory {
public:
    explicit Memory(std::size_t n)
    : bytes(n, 0)
    , text_end(0x1000)
    , heap_brk(0x2000)
    , heap_base(heap_brk)
    {}

    // ------------ loads/stores (little-endian) ------------
    uint32_t load32(uint32_t addr) const {
        if (addr + 3 >= bytes.size()) throw std::out_of_range("load32 OOB");
        return (uint32_t)bytes[addr]
             | ((uint32_t)bytes[addr+1] << 8)
             | ((uint32_t)bytes[addr+2] << 16)
             | ((uint32_t)bytes[addr+3] << 24);
    }
    void store32(uint32_t addr, uint32_t v) {
        if (addr + 3 >= bytes.size()) throw std::out_of_range("store32 OOB");
        bytes[addr]   = (uint8_t)(v & 0xFF);
        bytes[addr+1] = (uint8_t)((v >> 8) & 0xFF);
        bytes[addr+2] = (uint8_t)((v >> 16) & 0xFF);
        bytes[addr+3] = (uint8_t)((v >> 24) & 0xFF);
    }
    void store8(uint32_t addr, uint8_t v){
        if (addr >= bytes.size()) throw std::out_of_range("store8 OOB");
        bytes[addr] = v;
    }
    uint8_t load8(uint32_t addr) const{
        if (addr >= bytes.size()) throw std::out_of_range("load8 OOB");
        return bytes[addr];
    }

    // ------------ sbrk (program break) ------------
    uint32_t sbrk(int32_t delta){
        uint32_t old = heap_brk;
        int64_t target = (int64_t)heap_brk + (int64_t)delta;
        target = std::max<int64_t>(target, (int64_t)text_end);
        target = std::min<int64_t>(target, (int64_t)bytes.size());
        heap_brk = (uint32_t)target;
        return old;
    }
    uint32_t brk()   const { return heap_brk; }
    uint32_t hbase() const { return heap_base; }
    std::size_t size() const { return bytes.size(); }

    // ------------ tiny malloc/free (first-fit) ------------
    // Blocks are tracked in a sorted vector by start address.
    uint32_t malloc32(uint32_t nbytes){
        if (nbytes == 0) return 0;
        const uint32_t ALIGN = 8;
        auto align_up = [&](uint32_t v){ return (v + (ALIGN-1)) & ~(ALIGN-1); };
        uint32_t need = align_up(nbytes);

        // first-fit search
        for (size_t i=0;i<blocks.size();++i){
            auto& b = blocks[i];
            if (b.free && b.size >= need){
                // split if enough slack for another block
                uint32_t remain = b.size - need;
                uint32_t start  = b.start;
                b.free = false;
                b.size = need;
                if (remain >= ALIGN){
                    Block nb{start+need, remain, true};
                    blocks.insert(blocks.begin()+i+1, nb);
                }
                return start;
            }
        }
        // no block big enough → grow heap (at least one page or 'need')
        uint32_t grow = std::max<uint32_t>(need, 4096);
        uint32_t old  = sbrk(grow);
        Block nb{old, grow, true};
        // keep sorted order (old is the current top → append)
        blocks.push_back(nb);
        // retry once (will succeed)
        return malloc32(need);
    }

    void free32(uint32_t ptr){
        if (ptr < heap_base || ptr >= heap_brk) return; // invalid; ignore for now
        // find the exact block
        for (size_t i=0;i<blocks.size();++i){
            auto& b = blocks[i];
            if (b.start == ptr){
                b.free = true;
                // coalesce with next
                if (i+1 < blocks.size() && blocks[i+1].free){
                    b.size += blocks[i+1].size;
                    blocks.erase(blocks.begin()+i+1);
                }
                // coalesce with prev
                if (i>0 && blocks[i-1].free){
                    blocks[i-1].size += b.size;
                    blocks.erase(blocks.begin()+i);
                }
                return;
            }
        }
        // not found → ignore (toy safety)
    }

private:
    struct Block{
        uint32_t start;
        uint32_t size;
        bool     free;
    };

    std::vector<uint8_t> bytes;
    uint32_t text_end;
    uint32_t heap_brk;
    uint32_t heap_base;

    std::vector<Block> blocks; // sorted by 'start'
};
