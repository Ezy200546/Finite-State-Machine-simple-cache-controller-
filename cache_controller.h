#ifndef CACHE_CONTROLLER_H
#define CACHE_CONTROLLER_H

#include "cache_line.h"
#include "memory.h"

#define NUM_LINES 4   // number of cache lines (direct-mapped)

// FSM states (Figure 5.38, Patterson & Hennessy)
#define STATE_IDLE        0
#define STATE_COMPARE_TAG 1
#define STATE_WRITE_BACK  2
#define STATE_ALLOCATE    3

// Interface signals the FSM asserts each cycle
struct Signals {
    int cache_ready;  // 1 = data is ready for the CPU
    int mem_read;     // 1 = requesting a block from memory
    int mem_write;    // 1 = writing a dirty block to memory
    int valid_bit;    // current cache line's valid bit
    int dirty_bit;    // current cache line's dirty bit
    int stall_cpu;    // 1 = CPU must wait
};

// The cache controller
struct CacheController {
    CacheLine lines[NUM_LINES];  // the cache
    Signals   signals;           // current output signals
    int       state;             // current FSM state

    // Latched values of the request being processed
    int latchedOp;
    int latchedAddress;
    int latchedData;
};

#endif
