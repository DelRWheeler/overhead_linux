#pragma once
//////////////////////////////////////////////////////////////////////
// pcm3724_sim.h: Virtual PCM-3724 I/O board simulator
//
// Simulates the PCM-3724 parallel I/O card so the controller
// exercises the full I/O code path (readInputs -> port reads ->
// bit shuffling) without real hardware.
//
// Generates physically accurate sensor patterns:
//   - Counter sensors fire on every trolley (all syncs simultaneously)
//   - Zero sensors fire on every trolley (offset by 1.5" from counter)
//   - A single zero flag travels through all syncs in sequence,
//     extending the zero pulse to one full cycle to trigger zeroing
//   - Grade sync + grade value bits on Port C0
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

// Physical constants (inches)
#define PCM_SIM_TROLLEY_SPACING   6.0     // Center-to-center spacing
#define PCM_SIM_TROLLEY_WIDTH     1.125   // Width of trolley body
#define PCM_SIM_SENSOR_OFFSET     1.5     // Zero-to-counter sensor horizontal offset (zero is first)
#define PCM_SIM_FLAG_LENGTH       6.0     // Zero flag length (inches of travel that holds zero active)
#define PCM_SIM_DEFAULT_SPM       200.0   // Default shackles per minute

// Zero flag zeroing sequence: shackle offsets from line start
// These represent the physical position of the zero flag on the chain
// relative to each sensor location
#define PCM_SIM_FLAG_TO_GRADE     10      // Flag reaches grade sync after 10 shackles
#define PCM_SIM_GRADE_TO_SCALE1   100     // Grade sync to scale 1: 100 shackles
#define PCM_SIM_SCALE1_TO_DROP1   75      // Scale 1 to first drop sync: 75 shackles
#define PCM_SIM_DROP_TO_DROP      75      // Between consecutive drop syncs: 75 shackles

// Maximum number of sync channels we track for zero flag
// grade(1) + scales(2) + drops(6) = 9
#define PCM_SIM_MAX_ZERO_ENTRIES  9

struct PCM3724SimZeroEntry
{
    int sync_index;         // Which sync channel (0-7 for scale/drops, -1 for grade)
    bool is_grade;          // True if this is the grade sync zero
    int flag_arrival;       // Shackle count when flag first reaches this sensor
    bool initial_zeroed;    // Has the initial zero been completed for this sync?
};

struct PCM3724SimState
{
    // Virtual port registers for each board
    // Index: offset from board base address (0-9)
    unsigned char regs[PCM_SIM_NUM_BOARDS][PCM_SIM_REGS_PER_BOARD];

    // Simulation thread
    pthread_t     sim_thread;
    volatile bool running;
    volatile bool started;          // Line is producing (after auto-start delay)

    // Mutex for register access
    pthread_mutex_t mutex;

    // Tick-based timing (1ms per tick)
    unsigned int tick;              // Current tick within shackle cycle
    unsigned int cycle_ticks;       // Total ticks per shackle cycle
    unsigned int counter_on_ticks;    // Duration counter sensor is ON (trolley width)
    unsigned int zero_on_ticks;       // Duration zero sensor is ON (trolley width)
    unsigned int counter_start_tick;  // Tick when counter sensor turns ON (zero fires first, counter delayed by sensor offset)
    unsigned int zero_flag_ticks;     // Duration zero sensor ON during flag

    // Shackle tracking
    int shackle_count;              // Global shackle count since start

    // Configured sync mask (which syncs are active)
    unsigned char sync_mask;        // Bits 0-7: which syncs fire

    // Zero flag tracking
    PCM3724SimZeroEntry zero_entries[PCM_SIM_MAX_ZERO_ENTRIES];
    int num_zero_entries;           // How many entries are configured
    int shackles_per_rev;           // Revolution count (from config)

    // Grade cycling
    int grade_idx;

    // Auto-start tracking
    bool waiting_for_config;
    unsigned int config_ok_tick;    // Tick when config_Ok was first seen
    unsigned int autostart_delay;   // Ticks to wait after config_Ok (30 seconds)
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
