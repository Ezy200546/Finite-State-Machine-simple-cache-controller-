#include <iostream>
#include <vector>
#include <string>
#include <cstdio>
#include "cache_controller.h"

void runRequest(CacheController& ctrl, Memory& mem,
                const CPURequest& req, const std::string& description)
{
    printf("\n==========================================================\n");
    printf("  %-55s\n", description.c_str());
    printf("  %s addr=0x%X%s%-44s\n",
           req.op == OpType::READ ? "READ " : "WRITE",
           req.address,
           req.op == OpType::WRITE ? (" data=" + std::to_string(req.writeData) + " ").c_str() : "  ",
           "");

    printf("==========================================================\n");

    bool done = false;
    int safeguard = 0;

    while (!done && safeguard < 20) {
        done = ctrl.tick(req);
        ctrl.printSignals();
        safeguard++;
    }

    printf("  ✓ Request complete in %d cycles.\n", safeguard);
    printf("  Cache state after request:\n");
    ctrl.printCache();
}

void section(const std::string& title) {
    printf("\n==========================================================\n");
    printf("=  %-56s=\n", title.c_str());
    printf("==========================================================\n\n");
}

int main() {
    printf("\n==========================================================\n");
    printf("      FSM-Based Cache Controller Simulation                 \n");
    printf("      Based on Patterson & Hennessy Figure 5.38             \n");
    printf("      Direct-mapped, Write-back, Write-allocate             \n");
    printf("      Cache lines: %d | Memory latency: %d cycles            \n",
           NUM_LINES, MEM_LATENCY);
    printf("==========================================================\n");

    Memory memory;
    CacheController ctrl(memory);

    section("SCENARIO 1: Cold READ (Compulsory Miss, Clean)");
    runRequest(ctrl, memory,
        CPURequest(OpType::READ, 0x100),
        "READ 0x100 — cold start, cache is empty");

    section("SCENARIO 2: Repeated READ (Cache Hit)");
    runRequest(ctrl, memory,
        CPURequest(OpType::READ, 0x100),
        "READ 0x100 again — should be a cache hit");

    section("SCENARIO 3: WRITE to Cached Address (Write Hit)");
    runRequest(ctrl, memory,
        CPURequest(OpType::WRITE, 0x100, 0xABCD),
        "WRITE 0x100 = 0xABCD — hit, sets dirty bit");

    section("SCENARIO 4: READ Causing Dirty Eviction (Write-Back Path)");
    runRequest(ctrl, memory,
        CPURequest(OpType::READ, 0x500),
        "READ 0x500 — same index as 0x100, dirty eviction triggered");

    section("SCENARIO 5: WRITE Miss (Write-Allocate Policy)");
    runRequest(ctrl, memory,
        CPURequest(OpType::WRITE, 0x204, 0x1234),
        "WRITE 0x204 = 0x1234 — cold write, allocate-then-write");

    section("SCENARIO 6: Mixed Access Sequence (Realistic Workload)");

    ctrl.reset();
    printf("  (Controller and cache reset for fresh start)\n\n");

    std::vector<std::pair<CPURequest, std::string>> sequence = {
        { CPURequest(OpType::READ,  0x010),       "READ  0x010  (cold miss)" },
        { CPURequest(OpType::READ,  0x020),       "READ  0x020  (cold miss)" },
        { CPURequest(OpType::WRITE, 0x010, 99),    "WRITE 0x010 = 99  (write hit)" },
        { CPURequest(OpType::READ,  0x010),       "READ  0x010  (read hit)" },
        { CPURequest(OpType::READ,  0x110),       "READ  0x110  (miss, evicts 0x010 dirty)" },
        { CPURequest(OpType::WRITE, 0x030, 42),    "WRITE 0x030 = 42  (write miss)" },
        { CPURequest(OpType::READ,  0x030),       "READ  0x030  (read hit)" },
    };

    for (auto& item : sequence) {
        runRequest(ctrl, memory, item.first, item.second);
    }

    printf("\n==========================================================\n");
    printf("  Simulation complete. All FSM paths demonstrated:\n\n");
    printf("  IDLE -> COMPARE_TAG -> IDLE              (hit)\n");
    printf("  IDLE -> COMPARE_TAG -> ALLOCATE\n");
    printf("       -> COMPARE_TAG -> IDLE             (clean miss)\n");
    printf("  IDLE -> COMPARE_TAG -> WRITE_BACK\n");
    printf("       -> ALLOCATE -> COMPARE_TAG -> IDLE (dirty miss)\n");
    printf("==========================================================\n\n");

    return 0;
}