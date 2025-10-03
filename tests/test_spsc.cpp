#include "emu/spsc.hpp"
#include <thread>
#include <atomic>
#include <cassert>
#include <iostream>

int main(){
    SPSCQueue<int, 1024> q;
    std::atomic<bool> done{false};
    std::atomic<int> produced{0}, consumed{0};

    std::thread prod([&]{
        for(int i=0;i<200000;i++){
            while(!q.push(i)){}
            produced++;
        }
        done = true;
    });
    std::thread cons([&]{
        int v;
        while(!done.load() || !q.empty()){
            if(q.pop(v)) consumed++;
        }
    });

    prod.join(); cons.join();
    std::cout << "[spsc-test] produced="<<produced<<" consumed="<<consumed<<"\n";
    assert(consumed==produced);
    return 0;
}
