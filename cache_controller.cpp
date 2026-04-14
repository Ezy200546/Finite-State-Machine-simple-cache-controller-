#include <stdio.h>
#include "cache_controller.h"
#include "cpu.h"

// ---- Helper: get index and tag from address --------------------------------

int getIndex(int address) {
    return address % NUM_LINES;
}

int getTag(int address) {
    return address / NUM_LINES;
}

// ---- Helper: return state name as string -----------------------------------

const char* stateName(int state) {
    if (state == STATE_IDLE)        return "IDLE       ";
    if (state == STATE_COMPARE_TAG) return "COMPARE_TAG";
    if (state == STATE_WRITE_BACK)  return "WRITE_BACK ";
    if (state == STATE_ALLOCATE)    return "ALLOCATE   ";
    return "UNKNOWN";
}

// ---- Initialize the cache controller ---------------------------------------

void controllerInit(CacheController* ctrl) {
    int i;
    ctrl->state         = STATE_IDLE;
    ctrl->latchedOp     = 0;
    ctrl->latchedAddress= 0;
    ctrl->latchedData   = 0;

    for (i = 0; i < NUM_LINES; i++) {
        ctrl->lines[i].valid = 0;
        ctrl->lines[i].dirty = 0;
        ctrl->lines[i].tag   = -1;
        ctrl->lines[i].data  = 0;
    }

    ctrl->signals.cache_ready = 0;
    ctrl->signals.mem_read    = 0;
    ctrl->signals.mem_write   = 0;
    ctrl->signals.valid_bit   = 0;
    ctrl->signals.dirty_bit   = 0;
    ctrl->signals.stall_cpu   = 0;
}

// ---- Print all signals for this cycle --------------------------------------

void printSignals(Signals* s) {
    printf("  Signals: cache_ready=%d  mem_read=%d  mem_write=%d  valid=%d  dirty=%d  stall_cpu=%d\n",
        s->cache_ready, s->mem_read, s->mem_write,
        s->valid_bit, s->dirty_bit, s->stall_cpu);
}

// ---- Print cache contents --------------------------------------------------

void printCache(CacheController* ctrl) {
    int i;
    printf("\n  +------+-------+-------+---------+-------+\n");
    printf("  | Line | Valid | Dirty | Tag     | Data  |\n");
    printf("  +------+-------+-------+---------+-------+\n");
    for (i = 0; i < NUM_LINES; i++) {
        CacheLine* l = &ctrl->lines[i];
        if (l->tag == -1) {
            printf("  |  %d   |   %d   |   %d   |   ---   |  ---  |\n",
                i, l->valid, l->dirty);
        } else {
            printf("  |  %d   |   %d   |   %d   |  0x%03X  |  %3d  |\n",
                i, l->valid, l->dirty, l->tag, l->data);
        }
    }
    printf("  +------+-------+-------+---------+-------+\n\n");
}

// ---- Main FSM tick ---------------------------------------------------------
// Called once per clock cycle.
// Takes the current CPU request and memory module.
// Returns 1 when the request is fully served (cache_ready asserted).

int controllerTick(CacheController* ctrl, Memory* mem, Request req, int cycle) {
    int idx, tag, isHit;
    CacheLine* line;

    // Reset all signals at the start of each cycle
    ctrl->signals.cache_ready = 0;
    ctrl->signals.mem_read    = 0;
    ctrl->signals.mem_write   = 0;
    ctrl->signals.valid_bit   = 0;
    ctrl->signals.dirty_bit   = 0;
    ctrl->signals.stall_cpu   = 0;

    // -----------------------------------------------------------------------
    // STATE: IDLE
    // A valid request has arrived from the CPU queue.
    // Latch it and move to COMPARE_TAG.
    // -----------------------------------------------------------------------
    if (ctrl->state == STATE_IDLE) {
        ctrl->latchedOp      = req.op;
        ctrl->latchedAddress = req.address;
        ctrl->latchedData    = req.data;

        ctrl->signals.stall_cpu = 1;
        ctrl->state = STATE_COMPARE_TAG;

        printf("  [Cycle %3d | %s] Request received: %s addr=0x%X\n",
            cycle,
            stateName(ctrl->state),
            (req.op == OP_READ ? "READ " : "WRITE"),
            req.address);
        printSignals(&ctrl->signals);
        return 0;
    }

    // -----------------------------------------------------------------------
    // STATE: COMPARE_TAG
    // Check if the address is in cache (hit or miss).
    // Hit  -> serve CPU, return to IDLE.
    // Miss (clean) -> go to ALLOCATE.
    // Miss (dirty) -> go to WRITE_BACK first.
    // -----------------------------------------------------------------------
    if (ctrl->state == STATE_COMPARE_TAG) {
        idx  = getIndex(ctrl->latchedAddress);
        tag  = getTag(ctrl->latchedAddress);
        line = &ctrl->lines[idx];

        ctrl->signals.valid_bit = line->valid;
        ctrl->signals.dirty_bit = line->dirty;

        isHit = (line->valid == 1) && (line->tag == tag);

        if (isHit == 1) {
            // -- HIT --
            ctrl->signals.cache_ready = 1;
            ctrl->signals.stall_cpu   = 0;

            if (ctrl->latchedOp == OP_WRITE) {
                // Write hit: update data, set dirty, set valid+tag
                line->valid = 1;
                line->tag   = tag;
                line->dirty = 1;
                line->data  = ctrl->latchedData;
                ctrl->signals.dirty_bit = 1;
                printf("  [Cycle %3d | %s] WRITE HIT  index=%d tag=0x%X  data=%d written, dirty=1\n",
                    cycle, stateName(ctrl->state), idx, tag, ctrl->latchedData);
            } else {
                printf("  [Cycle %3d | %s] READ HIT   index=%d tag=0x%X  data=%d served to CPU\n",
                    cycle, stateName(ctrl->state), idx, tag, line->data);
            }

            ctrl->state = STATE_IDLE;
            printSignals(&ctrl->signals);
            return 1;  // request done

        } else {
            // -- MISS --
            ctrl->signals.stall_cpu = 1;

            if (line->valid == 1 && line->dirty == 1) {
                // Dirty miss: must write old block back first
                printf("  [Cycle %3d | %s] MISS (dirty) index=%d old_tag=0x%X must write back\n",
                    cycle, stateName(ctrl->state), idx, line->tag);
                ctrl->state = STATE_WRITE_BACK;
                memoryRequestWrite(mem, line->tag * NUM_LINES + idx, line->data);
            } else {
                // Clean miss: fetch new block directly
                printf("  [Cycle %3d | %s] MISS (clean) index=%d tag=0x%X fetching from memory\n",
                    cycle, stateName(ctrl->state), idx, tag);
                ctrl->state = STATE_ALLOCATE;
                memoryRequestRead(mem, ctrl->latchedAddress);
            }

            // Update tag now (book says tag is updated on miss)
            line->tag   = tag;
            line->valid = 1;

            printSignals(&ctrl->signals);
            return 0;
        }
    }

    // -----------------------------------------------------------------------
    // STATE: WRITE_BACK
    // Write the dirty block to memory. Wait until memory is ready.
    // Then move to ALLOCATE to fetch the new block.
    // -----------------------------------------------------------------------
    if (ctrl->state == STATE_WRITE_BACK) {
        int memDone;
        ctrl->signals.mem_write = 1;
        ctrl->signals.stall_cpu = 1;

        memDone = memoryTick(mem);

        if (memDone == 1) {
            int idx2 = getIndex(ctrl->latchedAddress);
            ctrl->lines[idx2].dirty = 0;  // block is now clean in memory

            printf("  [Cycle %3d | %s] Write-Back done, dirty block flushed\n",
                cycle, stateName(ctrl->state));

            ctrl->state = STATE_ALLOCATE;
            memoryRequestRead(mem, ctrl->latchedAddress);
        } else {
            printf("  [Cycle %3d | %s] Writing dirty block to memory...\n",
                cycle, stateName(ctrl->state));
        }

        printSignals(&ctrl->signals);
        return 0;
    }

    // -----------------------------------------------------------------------
    // STATE: ALLOCATE
    // Fetch the new block from memory. Wait until memory is ready.
    // Then go back to COMPARE_TAG to re-serve the original request.
    // -----------------------------------------------------------------------
    if (ctrl->state == STATE_ALLOCATE) {
        int memDone;
        ctrl->signals.mem_read  = 1;
        ctrl->signals.stall_cpu = 1;

        memDone = memoryTick(mem);

        if (memDone == 1) {
            int idx2  = getIndex(ctrl->latchedAddress);
            int tag2  = getTag(ctrl->latchedAddress);
            int value = memoryRead(mem, ctrl->latchedAddress);

            ctrl->lines[idx2].valid = 1;
            ctrl->lines[idx2].tag   = tag2;
            ctrl->lines[idx2].dirty = 0;
            ctrl->lines[idx2].data  = value;

            printf("  [Cycle %3d | %s] Block loaded from memory: index=%d tag=0x%X data=%d\n",
                cycle, stateName(ctrl->state), idx2, tag2, value);
            printf("              Returning to COMPARE_TAG to re-serve the request\n");

            ctrl->state = STATE_COMPARE_TAG;
        } else {
            printf("  [Cycle %3d | %s] Fetching block from memory...\n",
                cycle, stateName(ctrl->state));
        }

        printSignals(&ctrl->signals);
        return 0;
    }

    return 0;
}
