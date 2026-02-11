#pragma once
//////////////////////////////////////////////////////////////////////
// pcm3724_sim.h: Virtual PCM-3724 I/O board simulator
//
// Simulates the PCM-3724 parallel I/O card so the controller
// exercises the full I/O code path (readInputs → port reads →
// bit shuffling) without real hardware.
//
// Enable with #define _PCM3724_SIM_ in overheadconst.h
// Requires _SIMULATION_MODE_ to be DISABLED (so readInputs runs)
// Can coexist with _WEIGHT_SIMULATION_MODE_ (simulated weights)
//////////////////////////////////////////////////////////////////////

#include <pthread.h>
#include <stdint.h>

// Port address ranges for the 4 boards
// Board 1: 0x300-0x309 (primary syncs + switches)
// Board 2: 0x310-0x319 (extended I/O)
// Board 3: 0x320-0x329 (VSBC6 secondary)
// Board 4: 0xC800-0xC809 (VL-EPM-19)

#define PCM_SIM_NUM_BOARDS    4
#define PCM_SIM_REGS_PER_BOARD 10

// Board base addresses (must match 3724_io*.h)
#define PCM_SIM_BOARD1_BASE   0x300
#define PCM_SIM_BOARD2_BASE   0x310
#define PCM_SIM_BOARD3_BASE   0x320
#define PCM_SIM_BOARD4_BASE   0xC800

struct PCM3724SimState
{
    // Virtual port registers for each board
    // Index: offset from board base address (0-9)
    unsigned char regs[PCM_SIM_NUM_BOARDS][PCM_SIM_REGS_PER_BOARD];

    // Simulation thread
    pthread_t     sim_thread;
    volatile bool running;

    // Mutex for register access
    pthread_mutex_t mutex;

    // Simulation state machine
    int  step;
    unsigned int sync_on_ticks;
    unsigned int sync_off_ticks;

    // Timing parameters (same as Simulator.cpp)
    double bpm_setpoint;
    double on_time;      // ms
    double off_time;     // ms

    // Grade cycling
    int grade_idx;

    // Shackle counting for zero pulse generation
    int shk_cnt;
};

// Global simulator instance
extern PCM3724SimState g_pcmSim;

// Initialize the virtual PCM-3724 boards (call once at startup)
void PCM3724Sim_Init(void);

// Start the simulation thread (generates sensor patterns)
void PCM3724Sim_Start(void);

// Stop the simulation thread
void PCM3724Sim_Stop(void);

// Cleanup resources
void PCM3724Sim_Cleanup(void);

// Read a virtual port register (called from RtReadPortUchar)
unsigned char PCM3724Sim_Read(uintptr_t addr);

// Write a virtual port register (called from RtWritePortUchar)
void PCM3724Sim_Write(uintptr_t addr, unsigned char value);
