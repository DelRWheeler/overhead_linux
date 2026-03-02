//////////////////////////////////////////////////////////////////////
// ispd_sim.h: Virtual H&B ISPD-20KG digital load cell simulator
//
// Simulates the H&B (Hauch & Bach) ISPD-20KG serial protocol at the
// Serial layer level so the controller exercises the full ISPD load
// cell code path:
//   SerialWrite() → command parser → response generator
//   SerialRead()  → FIFO ring buffer → measurement lines
//
// Protocol: ASCII commands terminated with CR (0x0D)
// Responses: ASCII text terminated with CR
// Measurement output: "S±NNNNNN\r" (S prefix, sign, 6-digit count)
//
// Enable with #define _LC_SIM_ in overheadconst.h
// Serial.cpp routes to this simulator when g_simType[port] == 1 (ISPD)
//////////////////////////////////////////////////////////////////////

#pragma once

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

// FIFO ring buffer size (must be power of 2 for fast masking)
#define ISPD_SIM_FIFO_SIZE  4096
#define ISPD_SIM_FIFO_MASK  (ISPD_SIM_FIFO_SIZE - 1)

// Maximum command length from controller
#define ISPD_SIM_CMD_MAX    128

// Number of simulated weight values to cycle through
#define ISPD_SIM_NUM_WEIGHTS 32

// Simulated measurement interval in microseconds
// ISPD-20KG at UR=1: ~586 Hz output rate (close to HBM's ~600 Hz)
#define ISPD_SIM_MEAS_INTERVAL_US  1707  // ~586 Hz

// ISPD counts per pound (measured from real ISPD-20KG hardware)
#define ISPD_CNTS  13340

//--------------------------------------------------------
// Per-load-cell simulator state
//--------------------------------------------------------
typedef struct
{
    // FIFO ring buffer (load cell → controller)
    unsigned char   fifo[ISPD_SIM_FIFO_SIZE];
    int             fifo_head;      // write index
    int             fifo_tail;      // read index
    pthread_mutex_t fifo_mutex;

    // Command receive buffer (controller → load cell)
    char            cmd_buf[ISPD_SIM_CMD_MAX];
    int             cmd_idx;

    // H&B settings
    int             FL;         // filter level (default 3)
    int             FM;         // filter mode (default 0 = IIR)
    int             UR;         // update rate divider (default 1)
    int             DP;         // decimal point position (default 0)

    // Continuous measurement state
    bool            continuous;     // GS polling or continuous mode active
    bool            running;        // thread running
    pthread_t       meas_thread;    // measurement generator thread

    // Weight generation
    int             weight_idx;     // index into weight table
    int             port_num;       // COM port number for this instance

    // Shackle tracking (for advancing weight table)
    bool            prev_bird;      // previous bird-present state
    int             prev_seq;       // last seen shackle sequence counter

    // Identification
    bool            initialized;
} ISPDSimState;

// Maximum 4 load cells (matching MAXLOADCELLS)
#define ISPD_SIM_MAX_LC  4

// Global array of simulator states
extern ISPDSimState g_ispdSim[ISPD_SIM_MAX_LC];

//--------------------------------------------------------
// API functions called from Serial.cpp
//--------------------------------------------------------

// Initialize simulator for a specific COM port
void ISPDSim_Init(int port_num);

// Write data to simulator (controller → load cell command)
// Returns number of bytes consumed
int ISPDSim_Write(int port_num, const unsigned char* data, int len);

// Read data from simulator (load cell → controller response/measurements)
// Returns number of bytes read
int ISPDSim_Read(int port_num, unsigned char* buffer, int max_len);

// Get number of bytes available to read
int ISPDSim_Available(int port_num);

// Cleanup
void ISPDSim_Cleanup(int port_num);
void ISPDSim_CleanupAll(void);

//--------------------------------------------------------
// Bird-present signaling (shared with HBM sim)
//
// These globals are set by the sync processing code in
// overhead.cpp when _LC_SIM_ is defined. Both HBM and
// ISPD simulators use the same bird-present signal.
//--------------------------------------------------------
extern volatile bool g_lc_bird_present;
extern volatile int  g_lc_shackle_seq;
