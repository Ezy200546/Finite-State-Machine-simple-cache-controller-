#include <stdio.h>
#include "memory.h"
#include "cpu.h"

// ---- Memory implementations ------------------------------------------------

void memoryInit(Memory* mem) {
    int i;
    for (i = 0; i < MEM_SIZE; i++) {
        mem->data[i] = 0;
    }
    mem->busy        = 0;
    mem->countdown   = 0;
    mem->pendingAddr = 0;
    mem->pendingData = 0;
    mem->pendingWrite= 0;
}

void memoryRequestRead(Memory* mem, int address) {
    mem->busy        = 1;
    mem->countdown   = MEM_LATENCY;
    mem->pendingAddr = address;
    mem->pendingWrite= 0;
}

void memoryRequestWrite(Memory* mem, int address, int value) {
    mem->busy        = 1;
    mem->countdown   = MEM_LATENCY;
    mem->pendingAddr = address;
    mem->pendingData = value;
    mem->pendingWrite= 1;
}

// Advance memory one cycle. Returns 1 when the operation is done.
int memoryTick(Memory* mem) {
    if (mem->busy == 0) return 0;
    mem->countdown--;
    if (mem->countdown <= 0) {
        if (mem->pendingWrite == 1) {
            mem->data[mem->pendingAddr % MEM_SIZE] = mem->pendingData;
        }
        mem->busy = 0;
        return 1;
    }
    return 0;
}

int memoryRead(Memory* mem, int address) {
    return mem->data[address % MEM_SIZE];
}

// ---- CPU implementations ---------------------------------------------------

void cpuInit(CPU* cpu) {
    cpu->head    = 0;
    cpu->tail    = 0;
    cpu->count   = 0;
    cpu->stalled = 0;
}

void cpuEnqueue(CPU* cpu, int op, int address, int data) {
    if (cpu->count >= QUEUE_MAX) {
        printf("  [CPU] Queue full!\n");
        return;
    }
    cpu->queue[cpu->tail].op      = op;
    cpu->queue[cpu->tail].address = address;
    cpu->queue[cpu->tail].data    = data;
    cpu->tail  = (cpu->tail + 1) % QUEUE_MAX;
    cpu->count++;
}

int cpuHasRequest(CPU* cpu) {
    return cpu->count > 0 && cpu->stalled == 0;
}

Request cpuCurrentRequest(CPU* cpu) {
    return cpu->queue[cpu->head];
}

void cpuAcknowledge(CPU* cpu) {
    if (cpu->count > 0) {
        cpu->head    = (cpu->head + 1) % QUEUE_MAX;
        cpu->count--;
        cpu->stalled = 0;
    }
}

void cpuStall(CPU* cpu) {
    cpu->stalled = 1;
}

int cpuQueueSize(CPU* cpu) {
    return cpu->count;
}
