#pragma once
#include <atomic>

struct SpinLock {
    std::atomic_flag f = ATOMIC_FLAG_INIT;
    void lock()   { while (f.test_and_set(std::memory_order_acquire)) {} }
    void unlock() {        f.clear(std::memory_order_release); }
};

struct LockedCounter {
    std::atomic<int> value{0};
    SpinLock lock;

    void add(int n){
        lock.lock();
        value.store(value.load(std::memory_order_relaxed) + n,
                    std::memory_order_relaxed);
        lock.unlock();
    }
};

