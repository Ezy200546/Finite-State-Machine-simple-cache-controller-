#include "cache_controller.h"
#include <cstdio>

#define COL_RESET  "\033[0m"
#define COL_IDLE   "\033[37m"
#define COL_CMP    "\033[34m"
#define COL_WB     "\033[31m"
#define COL_ALLOC  "\033[32m"
#define COL_HIT    "\033[1;32m"
#define COL_MISS   "\033[1;31m"
#define COL_SIG    "\033[33m"

std::string CacheController::stateName() const {
    switch (state) {
        case State::IDLE:        return "IDLE";
        case State::COMPARE_TAG: return "COMPARE_TAG";
        case State::WRITE_BACK:  return "WRITE_BACK";
        case State::ALLOCATE:    return "ALLOCATE";
    }
    return "UNKNOWN";
}

void CacheController::logTransition(const std::string& msg) const {
    const char* col = COL_IDLE;
    switch (state) {
        case State::IDLE:        col = COL_IDLE;  break;
        case State::COMPARE_TAG: col = COL_CMP;   break;
        case State::WRITE_BACK:  col = COL_WB;    break;
        case State::ALLOCATE:    col = COL_ALLOC; break;
    }
    printf("%s[Cycle %3d | %-12s]%s %s\n", col, cycle, stateName().c_str(), COL_RESET, msg.c_str());
}

bool CacheController::tick(const CPURequest& req) {
    cycle++;
    signals.reset();

    char buf[256];

    switch (state) {

    case State::IDLE: {
        latchedAddress   = req.address;
        latchedWriteData = req.writeData;
        latchedOp        = req.op;

        snprintf(buf, sizeof(buf),
            "Valid CPU request received: %s addr=0x%X  →  moving to COMPARE_TAG",
            (req.op == OpType::READ ? "READ" : "WRITE"), req.address);
        state = State::COMPARE_TAG;
        signals.stall_cpu = true;
        logTransition(buf);
        return false;
    }

    case State::COMPARE_TAG: {
        int idx  = getIndex(latchedAddress);
        int tag  = getTag  (latchedAddress);
        CacheLine& line = cache[idx];

        signals.valid_bit = line.valid;
        signals.dirty_bit = line.dirty;

        bool isHit = line.valid && (line.tag == tag);

        if (isHit) {
            signals.cache_ready = true;
            signals.stall_cpu   = false;

            if (latchedOp == OpType::WRITE) {
                line.valid = true;
                line.tag   = tag;
                line.dirty = true;
                line.data  = latchedWriteData;
                signals.dirty_bit = true;

                snprintf(buf, sizeof(buf),
                    COL_HIT "WRITE HIT" COL_RESET
                    " | index=%d tag=0x%X | data updated to %d, dirty=1",
                    idx, tag, latchedWriteData);
            } else {
                snprintf(buf, sizeof(buf),
                    COL_HIT "READ HIT" COL_RESET
                    " | index=%d tag=0x%X | data=%d served to CPU",
                    idx, tag, line.data);
            }
            logTransition(buf);
            state = State::IDLE;
            return true;

        } else {
            signals.stall_cpu = true;

            line.tag   = tag;
            line.valid = true;

            if (line.dirty && line.valid) {
                snprintf(buf, sizeof(buf),
                    COL_MISS "MISS (dirty eviction)" COL_RESET
                    " | index=%d | old tag=0x%X is dirty, must write back first",
                    idx, line.tag);
                logTransition(buf);
                state = State::WRITE_BACK;
                memory.requestWrite(latchedAddress, line.data);
            } else {
                snprintf(buf, sizeof(buf),
                    COL_MISS "MISS (clean)" COL_RESET
                    " | index=%d tag=0x%X | no writeback needed, fetching from memory",
                    idx, tag);
                logTransition(buf);
                state = State::ALLOCATE;
                memory.requestRead(latchedAddress);
            }
            return false;
        }
    }

    case State::WRITE_BACK: {
        signals.mem_write = true;
        signals.stall_cpu = true;

        bool memReady = memory.tick();

        if (memReady) {
            int idx = getIndex(latchedAddress);
            cache[idx].dirty = false;

            snprintf(buf, sizeof(buf),
                "Write-Back complete (mem ready). Dirty block flushed. "
                "→ moving to ALLOCATE");
            logTransition(buf);
            state = State::ALLOCATE;
            memory.requestRead(latchedAddress);
        } else {
            snprintf(buf, sizeof(buf),
                "Write-Back: waiting for memory... (%d/%d cycles)",
                memory.getCycles(), MEM_LATENCY);
            logTransition(buf);
        }
        return false;
    }

    case State::ALLOCATE: {
        signals.mem_read  = true;
        signals.stall_cpu = true;

        bool memReady = memory.tick();

        if (memReady) {
            int idx  = getIndex(latchedAddress);
            int tag  = getTag  (latchedAddress);
            int data = memory.read(latchedAddress);

            cache[idx].valid = true;
            cache[idx].tag   = tag;
            cache[idx].dirty = false;
            cache[idx].data  = data;

            snprintf(buf, sizeof(buf),
                "Allocate complete (mem ready). Block loaded: index=%d tag=0x%X data=%d"
                "  →  returning to COMPARE_TAG to re-serve request",
                idx, tag, data);
            logTransition(buf);
            state = State::COMPARE_TAG;
        } else {
            snprintf(buf, sizeof(buf),
                "Allocate: waiting for memory... (%d/%d cycles)",
                memory.getCycles(), MEM_LATENCY);
            logTransition(buf);
        }
        return false;
    }

    }
    return false;
}

void CacheController::printCache() const {
    printf("\n  %-6s %-6s %-6s %-10s %-10s\n", "Line", "Valid", "Dirty", "Tag", "Data");
    printf("  %s\n", std::string(46, '-').c_str());
    for (int i = 0; i < NUM_LINES; i++) {
        const CacheLine& l = cache[i];
        char tagBuf[16], dataBuf[16];
        if (l.tag == -1) snprintf(tagBuf,  sizeof(tagBuf),  "---");
        else             snprintf(tagBuf,  sizeof(tagBuf),  "0x%X", l.tag);
        if (!l.valid)    snprintf(dataBuf, sizeof(dataBuf), "---");
        else             snprintf(dataBuf, sizeof(dataBuf), "%d", l.data);

        printf("  %-6d %-6s %-6s %-10s %-10s\n",
            i,
            l.valid ? "1" : "0",
            l.dirty ? "1" : "0",
            tagBuf,
            dataBuf);
    }
    printf("\n");
}

void CacheController::printSignals() const {
    printf(COL_SIG "  Signals: %s\n" COL_RESET, signals.toString().c_str());
}
