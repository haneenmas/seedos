// tests/test_util.hpp
#pragma once
#include <iostream>
#include <string>
#include <cstdlib>

struct TestState {
    int passed = 0, failed = 0;
    void ok(bool cond, const char* expr, const char* file, int line){
        if(cond) ++passed;
        else {
            ++failed;
            std::cerr << "[FAIL] " << file << ":" << line << "  " << expr << "\n";
        }
    }
    int summary() const {
        std::cout << "[tests] passed=" << passed << " failed=" << failed << "\n";
        return failed ? 1 : 0;
    }
};

#define EXPECT_TRUE(ts, expr)  (ts).ok((expr), #expr, __FILE__, __LINE__)
#define EXPECT_EQ(ts, a, b)    (ts).ok(((a)==(b)), #a " == " #b, __FILE__, __LINE__)

