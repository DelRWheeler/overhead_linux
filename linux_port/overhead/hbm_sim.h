//////////////////////////////////////////////////////////////////////
// hbm_sim.h: Virtual HBM FIT7 digital load cell simulator
//
// Simulates the HBM FIT7 serial protocol at the Serial layer level
// so the controller exercises the full HBM load cell code path:
//   SerialWrite() → command parser → response generator
//   SerialRead()  → FIFO ring buffer → measurement packets
//
// Enable with #define _HBM_SIM_ in overheadconst.h
// Replaces _WEIGHT_SIMULATION_MODE_ (which bypasses the entire
// HBM serial path with SimWeight())
//////////////////////////////////////////////////////////////////////

#pragma once

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

// FIFO ring buffer size (must be power of 2 for fast masking)
#define HBM_SIM_FIFO_SIZE  4096
#define HBM_SIM_FIFO_MASK  (HBM_SIM_FIFO_SIZE - 1)

// Maximum command length from controller
#define HBM_SIM_CMD_MAX    128

// Number of simulated weight values to cycle through
#define HBM_SIM_NUM_WEIGHTS 32

// Simulated measurement interval in microseconds (matching real FIT7 rate)
// At ICR=4, the FIT7 outputs ~600 measurements/sec
#define HBM_SIM_MEAS_INTERVAL_US  1667  // ~600 Hz

//--------------------------------------------------------
// Per-load-cell simulator state
//--------------------------------------------------------
typedef struct
{
    // FIFO ring buffer (load cell → controller)
    unsigned char   fifo[HBM_SIM_FIFO_SIZE];
    int             fifo_head;      // write index
    int             fifo_tail;      // read index
    pthread_mutex_t fifo_mutex;

    // Command receive buffer (controller → load cell)
    char            cmd_buf[HBM_SIM_CMD_MAX];
    int             cmd_idx;

    // Settings (matching dig_lc_settings struct)
    int             ASF;        // lowpass filter (default 5)
    int             FMD;        // filter mode (default 1)
    int             ICR;        // internal conversion rate (default 4)
    int             CWT;        // calibration weight (default 500000)
    int             LDW;        // dead load weight (default 0)
    int             LWT;        // live weight (default 500000)
    int             NOV;        // nominal value (default 0)
    int             RSN;        // resolution (default 1)
    int             MTD;        // motion detection (default 0)
    int             LIC0;       // linearization coeff 0 (default 0)
    int             LIC1;       // linearization coeff 1 (default 500000)
    int             LIC2;       // linearization coeff 2 (default 0)
    int             LIC3;       // linearization coeff 3 (default 0)
    int             ZTR;        // zero tracking (default 0)
    int             ZSE;        // zero setting (default 0)
    int             TRC1;       // trigger setting 1 (default 0)
    int             TRC2;       // trigger setting 2 (default 0)
    int             TRC3;       // trigger setting 3 (default 0)
    int             TRC4;       // trigger setting 4 (default 0)
    int             TRC5;       // trigger setting 5 (default 0)

    // Protocol settings
    int             BDR;        // baud rate (38400)
    int             COF;        // output format (default 8)
    int             CSM;        // checksum enabled (default 1)
    int             STR;        // terminating resistor (default 0)

    // Continuous measurement state
    bool            continuous;     // MSV?0 started
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
} HBMSimState;

// Maximum 4 load cells (matching MAXLOADCELLS)
#define HBM_SIM_MAX_LC  4

// Global array of simulator states
extern HBMSimState g_hbmSim[HBM_SIM_MAX_LC];

//--------------------------------------------------------
// API functions called from Serial.cpp
//--------------------------------------------------------

// Initialize simulator for a specific COM port
void HBMSim_Init(int port_num);

// Write data to simulator (controller → load cell command)
// Returns number of bytes consumed
int HBMSim_Write(int port_num, const unsigned char* data, int len);

// Read data from simulator (load cell → controller response/measurements)
// Returns number of bytes read
int HBMSim_Read(int port_num, unsigned char* buffer, int max_len);

// Get number of bytes available to read
int HBMSim_Available(int port_num);

// Cleanup
void HBMSim_Cleanup(int port_num);
void HBMSim_CleanupAll(void);

//--------------------------------------------------------
// Bird-present signaling (set by PCM3724 sim)
//
// When true, the HBM sim outputs bird weights from the table.
// When false, it outputs ~0 (empty shackle baseline).
// This is how the sim models birds physically being on the scale.
//--------------------------------------------------------
extern volatile bool g_hbm_bird_present;
extern volatile int  g_hbm_shackle_seq;
