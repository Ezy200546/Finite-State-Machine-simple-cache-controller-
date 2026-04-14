#ifndef CPU_H
#define CPU_H

#include <stdio.h>

#define QUEUE_MAX 20

#define OP_READ  0
#define OP_WRITE 1

struct Request {
    int op;
    int address;
    int data;
};

struct CPU {
    Request queue[QUEUE_MAX];
    int     head;
    int     tail;
    int     count;
    int     stalled;
};

void    cpuInit(CPU* cpu);
void    cpuEnqueue(CPU* cpu, int op, int address, int data);
int     cpuHasRequest(CPU* cpu);
Request cpuCurrentRequest(CPU* cpu);
void    cpuAcknowledge(CPU* cpu);
void    cpuStall(CPU* cpu);
int     cpuQueueSize(CPU* cpu);

#endif
