#include <stdio.h>
#include "cpu.h"
#include "memory.h"
#include "cache_controller.h"

// Forward declaration of the FSM tick function
int controllerTick(CacheController* ctrl, Memory* mem, Request req, int cycle);
void controllerInit(CacheController* ctrl);
void printCache(CacheController* ctrl);

// ---- Print a section header ------------------------------------------------

void printSection(const char* title) {
    printf("\n==============================================================\n");
    printf("  %s\n", title);
    printf("==============================================================\n\n");
}

// ---- Print the CPU queue contents ------------------------------------------

void printQueue(CPU* cpu) {
    int i, idx;
    printf("  CPU Queue (%d request(s)):\n", cpu->count);
    if (cpu->count == 0) {
        printf("    (empty)\n");
        return;
    }
    for (i = 0; i < cpu->count; i++) {
        idx = (cpu->head + i) % QUEUE_MAX;
        printf("    [%d] %s addr=0x%X",
            i,
            cpu->queue[idx].op == OP_READ ? "READ " : "WRITE",
            cpu->queue[idx].address);
        if (cpu->queue[idx].op == OP_WRITE) {
            printf(" data=%d", cpu->queue[idx].data);
        }
        if (i == 0) printf("  <-- next to process");
        printf("\n");
    }
    printf("\n");
}

// ---- Run the simulation: unified clock loop --------------------------------
// The CPU feeds requests from its queue one at a time.
// The cache controller FSM processes each request over possibly many cycles.
// The CPU waits (stalls) until cache_ready is asserted, then sends the next.

void runSimulation(CPU* cpu, CacheController* ctrl, Memory* mem) {
    int cycle    = 0;
    int maxCycles= 200;  // safety limit

    printf("  Starting simulation...\n");
    printQueue(cpu);

    while (cpu->count > 0 && cycle < maxCycles) {
        int done;
        Request req;

        cycle++;

        // CPU sends the front request from the queue
        req = cpuCurrentRequest(cpu);

        // Tick the FSM with the current request
        done = controllerTick(ctrl, mem, req, cycle);

        if (done == 1) {
            // cache_ready = 1: CPU can move to next request
            printf("  [Cycle %3d] CPU: cache_ready received, popping request from queue.\n\n", cycle);
            cpuAcknowledge(cpu);

            // Print cache state after each completed request
            printf("  Cache state after this request:\n");
            printCache(ctrl);

            if (cpu->count > 0) {
                printf("  Next in queue:\n");
                printQueue(cpu);
            }
        } else {
            // stall_cpu = 1: CPU waits
            cpuStall(cpu);
        }
    }

    if (cycle >= maxCycles) {
        printf("  [Warning] Simulation hit cycle limit.\n");
    }

    printf("  Simulation complete. Total cycles used: %d\n", cycle);
}

// ============================================================
// MAIN: load test scenarios into the CPU queue and run
// ============================================================

int main() {
    CPU             cpu;
    Memory          mem;
    CacheController ctrl;

    printf("\n");
    printf("##############################################################\n");
    printf("##   FSM Cache Controller Simulation                        ##\n");
    printf("##   Based on Patterson & Hennessy Figure 5.38              ##\n");
    printf("##   Direct-mapped | Write-back | Write-allocate            ##\n");
    printf("##   Cache lines: %d  |  Memory latency: %d cycles           ##\n",
           NUM_LINES, MEM_LATENCY);
    printf("##############################################################\n");

    // ----------------------------------------------------------
    // SCENARIO 1: Basic hit and miss
    //   - READ 0x100: cold miss (clean) -> Allocate -> hit
    //   - READ 0x100: now a hit
    //   - WRITE 0x100: write hit, sets dirty
    // ----------------------------------------------------------
    printSection("SCENARIO 1: Cold Miss, then Hit, then Write Hit");

    cpuInit(&cpu);
    memoryInit(&mem);
    controllerInit(&ctrl);

    cpuEnqueue(&cpu, OP_READ,  0x100, 0);
    cpuEnqueue(&cpu, OP_READ,  0x100, 0);
    cpuEnqueue(&cpu, OP_WRITE, 0x100, 999);

    runSimulation(&cpu, &ctrl, &mem);

    // ----------------------------------------------------------
    // SCENARIO 2: Dirty eviction (Write-Back path)
    //   - WRITE 0x100: cold miss, allocate, then write (dirty=1)
    //   - READ  0x500: same index as 0x100, triggers write-back
    //     Path: IDLE->COMPARE_TAG->WRITE_BACK->ALLOCATE->COMPARE_TAG->IDLE
    // ----------------------------------------------------------
    printSection("SCENARIO 2: Dirty Eviction (Full Write-Back Path)");

    cpuInit(&cpu);
    memoryInit(&mem);
    controllerInit(&ctrl);

    cpuEnqueue(&cpu, OP_WRITE, 0x100, 42);   // will make line dirty
    cpuEnqueue(&cpu, OP_READ,  0x500, 0);    // same index, forces eviction

    runSimulation(&cpu, &ctrl, &mem);

    // ----------------------------------------------------------
    // SCENARIO 3: Write miss (write-allocate policy)
    //   - WRITE to a cold address: FSM fetches block first, then writes
    //   - READ same address: should be a hit
    // ----------------------------------------------------------
    printSection("SCENARIO 3: Write Miss (Write-Allocate Policy)");

    cpuInit(&cpu);
    memoryInit(&mem);
    controllerInit(&ctrl);

    cpuEnqueue(&cpu, OP_WRITE, 0x204, 77);  // write miss: allocate first
    cpuEnqueue(&cpu, OP_READ,  0x204, 0);   // should hit now

    runSimulation(&cpu, &ctrl, &mem);

    // ----------------------------------------------------------
    // SCENARIO 4: Realistic mixed workload
    //   A longer sequence showing all FSM paths in one run
    // ----------------------------------------------------------
    printSection("SCENARIO 4: Mixed Workload (All Paths)");

    cpuInit(&cpu);
    memoryInit(&mem);
    controllerInit(&ctrl);

    cpuEnqueue(&cpu, OP_READ,  0x010, 0);    // cold miss
    cpuEnqueue(&cpu, OP_READ,  0x010, 0);    // hit
    cpuEnqueue(&cpu, OP_WRITE, 0x010, 55);   // write hit -> dirty
    cpuEnqueue(&cpu, OP_READ,  0x010, 0);    // read hit (dirty line)
    cpuEnqueue(&cpu, OP_READ,  0x110, 0);    // same index, dirty eviction
    cpuEnqueue(&cpu, OP_WRITE, 0x020, 88);   // write miss (write-allocate)
    cpuEnqueue(&cpu, OP_READ,  0x020, 0);    // hit

    runSimulation(&cpu, &ctrl, &mem);

    printf("\n##############################################################\n");
    printf("##  All scenarios done. FSM paths covered:                  ##\n");
    printf("##  [1] IDLE -> COMPARE_TAG -> IDLE             (hit)       ##\n");
    printf("##  [2] IDLE -> COMPARE_TAG -> ALLOCATE                     ##\n");
    printf("##          -> COMPARE_TAG -> IDLE          (clean miss)    ##\n");
    printf("##  [3] IDLE -> COMPARE_TAG -> WRITE_BACK                   ##\n");
    printf("##          -> ALLOCATE -> COMPARE_TAG -> IDLE (dirty miss) ##\n");
    printf("##############################################################\n\n");

    return 0;
}
