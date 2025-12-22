#pragma once
#include "headers.h"

// Initialize MMU shared memory and link scheduler's process list
void init_mmu(PCB *process_list_ptr);

// Cleanup MMU shared memory resources
void destroy_mmu();

// Handle memory access. Returns 0 for Hit, 1 for Page Fault 
int mmu_request(PCB *pcb, int logical_addr, char type, int current_time);

// Load page into memory after Disk I/O completes 
void resolve_page_fault(PCB *pcb, int current_time);

// Allocate a frame, swapping out a victim if full 
int allocate_frame(int pid, int page_num, bool is_page_table);