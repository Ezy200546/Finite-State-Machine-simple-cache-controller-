# FSM-Based Cache Controller Simulation (C++)

Simulates the **finite state machine cache controller** from Patterson & Hennessy *Computer Organization and Design* (Figure 5.38).

## Model

- **Cache**: Direct-mapped, 4 lines, write-back + write-allocate policy
- **Memory**: Always available, 2-cycle latency for reads and writes
- **FSM States**: `IDLE → COMPARE_TAG → WRITE_BACK → ALLOCATE`

## Build & Run

```bash
# Compile
make

# Run all test scenarios
./cache_sim

# Or in one step
make run

# Clean build
make clean
```

Requires g++ with C++17 support (`g++ --version` to check).

## Files

| File | Description |
|------|-------------|
| `main.cpp` | Simulation entry point; defines all test scenarios |
| `cache_controller.h/cpp` | The FSM — all state transition logic |
| `cache_line.h` | Struct for one cache line (valid, dirty, tag, data) |
| `memory.h` | Simple memory model with configurable latency |
| `Makefile` | Build system |

## FSM Paths Demonstrated

| Scenario | FSM Path |
|----------|----------|
| Cold READ | IDLE → COMPARE_TAG → ALLOCATE → COMPARE_TAG → IDLE |
| Cache hit (READ) | IDLE → COMPARE_TAG → IDLE |
| Cache hit (WRITE) | IDLE → COMPARE_TAG → IDLE (dirty=1) |
| Dirty eviction | IDLE → COMPARE_TAG → WRITE_BACK → ALLOCATE → COMPARE_TAG → IDLE |
| Write miss | IDLE → COMPARE_TAG → ALLOCATE → COMPARE_TAG → IDLE |
