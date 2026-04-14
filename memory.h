#ifndef MEMORY_H
#define MEMORY_H

#define MEM_SIZE    256
#define MEM_LATENCY 2

struct Memory {
    int  data[MEM_SIZE];
    int  busy;
    int  countdown;
    int  pendingAddr;
    int  pendingData;
    int  pendingWrite;
};

void memoryInit(Memory* mem);
void memoryRequestRead(Memory* mem, int address);
void memoryRequestWrite(Memory* mem, int address, int value);
int  memoryTick(Memory* mem);
int  memoryRead(Memory* mem, int address);

#endif
