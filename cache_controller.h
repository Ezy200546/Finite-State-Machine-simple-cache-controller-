#ifndef CACHE_CONTROLLER_H
#define CACHE_CONTROLLER_H

#include "cache_line.h"
#include "memory.h"
#include <string>
#include <iostream>
#include <iomanip>

const int NUM_LINES = 4;

enum class State {
    IDLE,
    COMPARE_TAG,
    WRITE_BACK,
    ALLOCATE
};

enum class OpType { READ, WRITE };

struct CPURequest {
    OpType op;
    int    address;
    int    writeData;
    CPURequest(OpType o, int a, int d = 0) : op(o), address(a), writeData(d) {}
};

struct Signals {
    bool cache_ready = false;
    bool mem_read    = false;
    bool mem_write   = false;
    bool valid_bit   = false;
    bool dirty_bit   = false;
    bool stall_cpu   = false;

    void reset() {
        cache_ready = mem_read = mem_write = false;
        valid_bit   = dirty_bit = stall_cpu = false;
    }

    std::string toString() const {
        auto b = [](bool v){ return v ? "1" : "0"; };
        return std::string("cache_ready=") + b(cache_ready) +
               "  mem_read="  + b(mem_read)    +
               "  mem_write=" + b(mem_write)   +
               "  valid="     + b(valid_bit)   +
               "  dirty="     + b(dirty_bit)   +
               "  stall_cpu=" + b(stall_cpu);
    }
};

class CacheController {
public:
    CacheController(Memory& mem) : memory(mem), state(State::IDLE), cycle(0) {}

    bool tick(const CPURequest& req);

    void printCache() const;

    void printSignals() const;

    Signals signals;

    std::string stateName() const;

    int getCycle() const { return cycle; }

    void reset() {
        state = State::IDLE;
        cycle = 0;
        signals.reset();
        for (auto& line : cache) line.reset();
        memory.reset();
    }

private:
    CacheLine cache[NUM_LINES];
    Memory&   memory;
    State     state;
    int       cycle;

    int latchedAddress  = 0;
    int latchedWriteData= 0;
    OpType latchedOp    = OpType::READ;

    int getIndex(int address) const { return address % NUM_LINES; }
    int getTag  (int address) const { return address / NUM_LINES; }

    void logTransition(const std::string& msg) const;
};

#endif
