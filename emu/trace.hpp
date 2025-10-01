
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <cstdio>

struct TraceRec {
    uint32_t tid;
    uint32_t pc;
    uint32_t opcode;
    uint32_t cycles_after;
    uint64_t instret_after;
};

class TraceLog {
public:
    void enable(bool on){ enabled = on; }
    bool is_enabled() const { return enabled; }

    void push(uint32_t tid, uint32_t pc, uint32_t opcode,
              uint32_t cycles_after, uint64_t instret_after)
    {
        if(!enabled) return;
        if (records.size() < max_keep) {
            records.push_back(TraceRec{tid,pc,opcode,cycles_after,instret_after});
        } else {
            records[idx % max_keep] = TraceRec{tid,pc,opcode,cycles_after,instret_after};
            idx++;
        }
    }

    bool write_ndjson(const std::string& path) const {
        FILE* f = std::fopen(path.c_str(), "wb");
        if(!f) return false;
        auto dump = [&](const TraceRec& r){
            std::fprintf(f,
              "{\"tid\":%u,\"pc\":%u,\"opcode\":%u,\"cycles\":%u,\"instret\":%llu}\n",
              r.tid, r.pc, r.opcode, r.cycles_after, (unsigned long long)r.instret_after);
        };
        if (idx==0) {
            for (auto& r: records) dump(r);
        } else {
            size_t base = idx % max_keep;
            for(size_t k=0;k<max_keep;k++){
                size_t i = (base + k) % max_keep;
                dump(records[i]);
            }
        }
        std::fclose(f);
        return true;
    }

    void set_capacity(size_t n){
        max_keep = n;
        records.clear();
        records.reserve(max_keep);
        idx = 0;
    }

private:
    bool enabled = false;
    size_t max_keep = 200000;
    size_t idx = 0;
    std::vector<TraceRec> records;
};

// global accessor
TraceLog& global_trace();
