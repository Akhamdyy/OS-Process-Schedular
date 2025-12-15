# Operating System Scheduler & Memory Management Simulator

A modular simulation of an Operating System kernel written in C. This project simulates core OS components including a **Process Generator**, **CPU Scheduler**, **Memory Management Unit (MMU)**, and a **System Clock**.

It demonstrates Inter-Process Communication (IPC) using **System V Message Queues**, **Shared Memory**, and **Unix Signals** to manage process synchronization and resource allocation.

## 🚀 Features

### Phase 1: CPU Scheduling

Implementation of three classic process scheduling algorithms:

- **HPF (Highest Priority First):** Preemptive priority-based scheduling using a Min-Heap.
- **SRTN (Shortest Remaining Time Next):** Preemptive scheduling based on remaining burst time.
- **RR (Round Robin):** Time-slice based scheduling with context switching.

### Phase 2: Memory Management (MMU)

A complete simulation of a paging system integrated with the Round Robin scheduler:

- **Virtual Memory:** 10-bit addressing space simulation.
- **Physical Memory:** 32 Frames (512 Bytes total RAM, 16 Bytes per page).
- **Demand Paging:** Pages are loaded from "disk" only when requested by the process.
- **Page Replacement:** Implements the **Second Chance (Clock)** algorithm to swap out pages when memory is full.
- **Disk I/O Simulation:** Processes are blocked for 10 clock cycles during Page Faults to simulate disk access latency.

## 📂 Project Structure

- **`process_generator.c`**: The system orchestrator. Reads `processes.txt`, initializes the IPC channels, and spawns the Scheduler and Clock.
- **`scheduler.c`**: The core component. It acts as both the CPU Scheduler and the MMU. It manages the Ready Queue, handles Page Faults, and logs system performance.
- **`clk.c`**: Emulates the system clock by updating a shared memory integer every second.
- **`process.c`**: Represents a user program. It consumes CPU time and generates signals upon completion.
- **`test_generator.c`**: Utility to generate random test data (processes and memory access traces).
- **`headers.h`**: Shared data structures (PCB, Page Table, Frame Table) and IPC keys.

## 🛠️ Build & Installation

Prerequisites: `gcc` and `make`.

1.  **Clone the repository:**

    ```bash
    git clone <your-repo-link>
    cd <repo-name>
    ```

2.  **Compile the project:**
    Use the provided Makefile to compile all modules.

    ```bash
    make build
    ```

3.  **Clean up (Optional):**
    To remove object files, logs, and generated input files:
    ```bash
    make clean
    ```

## 💻 Usage

### 1. Generate Test Data

Before running the simulation, generate the input files (`processes.txt` and `request_*.txt`).

```bash
./test_generator.out
Enter the number of processes you want to simulate (e.g., 20 for a stress test).

2. Run the Simulation
Start the Process Generator. It will automatically start the Clock and Scheduler.

Bash

./process_generator.out
3. runtime Options
Follow the on-screen prompts:

Choose Algorithm: Select 3 for Round Robin.

Enter Quantum: Recommended 2 or 3 for heavy load testing.
```
##📊 Output Logs
The system generates three log files for analysis:

**`scheduler.log:`** Tracks every state transition (Started, Stopped, Resumed, Finished) for every process.

**`memory.log:`** Tracks MMU events including Memory Allocations, Page Faults, and Swapping operations.

```bash
#Swapping out page 3 to disk
At time 190 page 2 for process 10 is loaded into memory page 3.
```

**`scheduler.perf:`** Calculates final system metrics:
```bash
CPU Utilization

Average Weighted Turnaround Time (WTA)

Average Waiting Time

Standard Deviation of WTA
```

##🧠 System Architecture


<img width="2816" height="1536" alt="OS Project Block Diagram" src="https://github.com/user-attachments/assets/e0229ab2-cf7f-448e-8512-b7db70c451a4" />

The system uses a Message Queue to pass process definitions from the Generator to the Scheduler. The Clock updates a timestamp in Shared Memory. The Scheduler/MMU manages Child Processes via fork/exec and controls their execution flow using SIGSTOP and SIGCONT signals.

