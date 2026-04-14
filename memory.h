#ifndef MEMORY_H
#define MEMORY_H

#include <unordered_map>
#include <string>
#include <cstdio>

// Simple main memory model.
// Never misses, but takes MEM_LATENCY cycles to complete any operation.
// Internally stores data as a map from address -> value.

const int MEM_LATENCY = 2;  // cycles to complete a read or write

class Memory {
public:
    Memory() : busy(false), countdown(0), pendingAddr(-1), pendingData(0), pendingWrite(false) {}

    // Request a read. Returns false if memory is busy (not ready yet).
    bool requestRead(int address) {
        if (!busy) {
            busy        = true;
            countdown   = MEM_LATENCY;
            pendingAddr = address;
            pendingWrite= false;
            return false; // not ready this cycle
        }
        return false;
    }

    // Request a write.
    bool requestWrite(int address, int data) {
        if (!busy) {
            busy        = true;
            countdown   = MEM_LATENCY;
            pendingAddr = address;
            pendingData = data;
            pendingWrite= true;
            return false;
        }
        return false;
    }

    // Call once per cycle. Returns true when the pending operation finishes.
    bool tick() {
        if (!busy) return false;
        countdown--;
        if (countdown <= 0) {
            if (pendingWrite) {
                store[pendingAddr] = pendingData;
            }
            busy = false;
            return true; // memory ready
        }
        return false;
    }

    // Directly read a value (used after memory signals ready on a read).
    int read(int address) {
        auto it = store.find(address);
        if (it != store.end()) return it->second;
        return 0xDEAD; // uninitialized memory sentinel
    }

    bool isBusy() const { return busy; }
    int  getCycles() const { return MEM_LATENCY - countdown; }

    void reset() {
        busy = false;
        countdown = 0;
        pendingAddr = -1;
        pendingData = 0;
        pendingWrite = false;
        // keep store intact across resets (memory persists)
    }

private:
    std::unordered_map<int,int> store;
    bool busy;
    int  countdown;
    int  pendingAddr;
    int  pendingData;
    bool pendingWrite;
};

#endif
