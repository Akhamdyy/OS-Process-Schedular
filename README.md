# Operating System Simulator (CPU & Memory)

A modular OS kernel simulation in C demonstrating **CPU Scheduling** and **Memory Management**. It utilizes System V IPC (Message Queues, Shared Memory, Signals) for process synchronization.

## 🚀 Key Features

- **CPU Scheduling:** Supports **HPF** (Highest Priority First), **SRTN** (Shortest Remaining Time Next), and **Round Robin** (RR).
- **Memory Management (MMU):** Simulates a paging system with **32 Physical Frames** and **Virtual Memory**.
- **Algorithms:** Implements **Demand Paging** and the **Second Chance (Clock)** page replacement algorithm.
- **Simulation:** Features realistic **Disk I/O latency** (10 cycles) during Page Faults.

## 📂 Structure

- **`process_generator.c`**: Orchestrates the system and initializes IPC.
- **`scheduler.c`**: Core logic for CPU scheduling and MMU operations.
- **`clk.c`**: System clock emulator using shared memory.
- **`process.c`**: Worker process simulation.
- **`test_generator.c`**: Generates processes and memory trace files.

## 🛠️ Build

```bash
git clone <your-repo-link>
make build
# To clean up logs and object files later: make clean
💻 How to Run
1. Generate Data Create the process list and memory access traces.

Bash

./test_generator.out
# Enter number of processes (e.g., 20 for stress testing)
2. Start Simulation Run the main program (starts Clock and Scheduler automatically).

Bash

./process_generator.out
3. Configure Follow the prompts:

Algorithm: Choose 3 (Round Robin) to enable Phase 2 MMU features.

Quantum: Enter 2 or 3 for high concurrency testing.

📊 Output Files
scheduler.log: Detailed process state transitions.

memory.log: MMU events (Allocations, Page Faults, Swapping).

scheduler.perf: Metrics (CPU Utilization, Avg Wait, Avg WTA).

🧠 Architecture
The system uses Message Queues for process creation, Shared Memory for time synchronization, and Signals (SIGSTOP/SIGCONT) to manage context switching and page fault blocking.
```
