// emu/spsc.hpp
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

// Single-Producer / Single-Consumer ring buffer.
// N must be a power of two for mask-fast modulo.
template <typename T, std::size_t N>
class SPSCQueue {
    static_assert((N & (N-1)) == 0, "N must be a power of two");
public:
    SPSCQueue() : head_(0), tail_(0) {}

    // Producer thread only
    bool push(const T& v) {
        const std::size_t t = tail_.load(std::memory_order_relaxed);
        const std::size_t next = (t + 1) & (N-1);
        if (next == head_.load(std::memory_order_acquire)) {
            return false; // full
        }
        buf_[t] = v;
        tail_.store(next, std::memory_order_release);
        return true;
    }

    // Consumer thread only
    bool pop(T& out) {
        const std::size_t h = head_.load(std::memory_order_relaxed);
        if (h == tail_.load(std::memory_order_acquire)) {
            return false; // empty
        }
        out = buf_[h];
        head_.store((h + 1) & (N-1), std::memory_order_release);
        return true;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }
    bool full() const {
        const std::size_t t = tail_.load(std::memory_order_acquire);
        const std::size_t next = (t + 1) & (N-1);
        return next == head_.load(std::memory_order_acquire);
    }

private:
    alignas(64) T buf_[N];
    alignas(64) std::atomic<std::size_t> head_;
    alignas(64) std::atomic<std::size_t> tail_;
};
