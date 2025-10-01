#include "trace.hpp"

TraceLog& global_trace(){
    static TraceLog g;
    return g;
}
