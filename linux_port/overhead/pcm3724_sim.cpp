//////////////////////////////////////////////////////////////////////
// pcm3724_sim.cpp: Virtual PCM-3724 I/O board simulator
//
// Generates simulated sensor patterns at the hardware register level
// so the controller exercises the full I/O code path:
//   readInputs() → RtReadPortUchar() → virtual registers → bit shuffling
//
// The simulation thread runs at 1ms intervals (same as the RTX timer)
// and generates:
//   - Sync pulses on Port A0/B0 (interleaved even bits)
//   - Zero pulses on Port A0/B0 (interleaved odd bits)
//   - Grade sync + grade zone bits on Port C0
//   - All in active-low format (0 = active, matching real PCM-3724)
//
//////////////////////////////////////////////////////////////////////

#include "types.h"
#include "pcm3724_sim.h"

#undef  _FILE_
#define _FILE_  "pcm3724_sim.cpp"

// Global instance
PCM3724SimState g_pcmSim;

// Access to overhead app (declared in overheadext.h as 'overhead* app')
// overhead inherits from app_type, so we can access all app_type members
extern overhead* app;

// Bit manipulation (matching overhead Mask[] array)
static const unsigned char SimMask[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

// Grade bit sequence (matching Simulator.cpp sim_grades[])
static const int sim_grade_bits[5] = {GRADE4_BIT, GRADE3_BIT, GRADE2_BIT, GRADE1_BIT, GRADE0_BIT};

//--------------------------------------------------------
// Map a port address to board index and register offset
// Returns -1 if address doesn't match any board
//--------------------------------------------------------
static int AddrToBoard(uintptr_t addr, int* reg_offset)
{
    if (addr >= PCM_SIM_BOARD1_BASE && addr < PCM_SIM_BOARD1_BASE + PCM_SIM_REGS_PER_BOARD)
    {
        *reg_offset = (int)(addr - PCM_SIM_BOARD1_BASE);
        return 0;
    }
    if (addr >= PCM_SIM_BOARD2_BASE && addr < PCM_SIM_BOARD2_BASE + PCM_SIM_REGS_PER_BOARD)
    {
        *reg_offset = (int)(addr - PCM_SIM_BOARD2_BASE);
        return 1;
    }
    if (addr >= PCM_SIM_BOARD3_BASE && addr < PCM_SIM_BOARD3_BASE + PCM_SIM_REGS_PER_BOARD)
    {
        *reg_offset = (int)(addr - PCM_SIM_BOARD3_BASE);
        return 2;
    }
    if (addr >= PCM_SIM_BOARD4_BASE && addr < PCM_SIM_BOARD4_BASE + PCM_SIM_REGS_PER_BOARD)
    {
        *reg_offset = (int)(addr - PCM_SIM_BOARD4_BASE);
        return 3;
    }
    return -1;
}

//--------------------------------------------------------
// Build Port A0 register value from desired sync/zero state
//
// The readInputs() bit-shuffling in 3724_io.cpp works like this:
//   Port A0 even bits (0,2,4,6) → sync_in[0] bits 0-3
//   Port A0 odd bits  (1,3,5,7) → sync_zero[0] bits 0-3
//
// We work in "active-high" (what the controller sees AFTER
// the ~ inversion), then invert to active-low for the register.
//--------------------------------------------------------
static unsigned char BuildPortA0(unsigned char sync_lo, unsigned char zero_lo)
{
    unsigned char active = 0;
    int i;

    for (i = 0; i < 4; i++)
    {
        // Even bits = syncs (bits 0,2,4,6 → sync_in bits 0,1,2,3)
        if (sync_lo & SimMask[i])
            active |= SimMask[i * 2];

        // Odd bits = zeros (bits 1,3,5,7 → sync_zero bits 0,1,2,3)
        if (zero_lo & SimMask[i])
            active |= SimMask[i * 2 + 1];
    }

    // Return active-low (readInputs inverts with ~)
    return (unsigned char)(~active);
}

//--------------------------------------------------------
// Build Port B0 register value from desired sync/zero state
//
// Same interleaving as Port A0, but maps to bits 4-7:
//   Port B0 even bits (0,2,4,6) → sync_in[0] bits 4-7
//   Port B0 odd bits  (1,3,5,7) → sync_zero[0] bits 4-7
//--------------------------------------------------------
static unsigned char BuildPortB0(unsigned char sync_hi, unsigned char zero_hi)
{
    unsigned char active = 0;
    int i;

    for (i = 0; i < 4; i++)
    {
        // sync_hi bits 4-7 map to even port bits
        if (sync_hi & SimMask[i + 4])
            active |= SimMask[i * 2];

        // zero_hi bits 4-7 map to odd port bits
        if (zero_hi & SimMask[i + 4])
            active |= SimMask[i * 2 + 1];
    }

    return (unsigned char)(~active);
}

//--------------------------------------------------------
// Build Port C0 register value from desired switch state
//
// Port C0 → switch_in[0] directly (no bit shuffling)
// Just active-low inversion.
//--------------------------------------------------------
static unsigned char BuildPortC0(unsigned char switch_val)
{
    return (unsigned char)(~switch_val);
}

//--------------------------------------------------------
// RunSimStep: One tick of the simulation state machine
//
// Same logic as Simulator::RunSim() but writes to virtual
// port registers instead of directly to sync_in[]/switch_in[].
//
// Called every 1ms by the simulation thread.
//--------------------------------------------------------
static void RunSimStep(void)
{
    unsigned char sync_bits  = 0;
    unsigned char zero_bits  = 0;
    unsigned char switch_bits = 0;
    int i, byte;

    switch (g_pcmSim.step)
    {
    case 0:
        // Reset PPM timer shackle count tracking
        if (app && app->pShm && app->pShm->sys_stat.PPMTimer == 1 && g_pcmSim.shk_cnt != 0)
            g_pcmSim.shk_cnt = 0;

        // Start with everything OFF
        g_pcmSim.sync_on_ticks = 0;
        g_pcmSim.sync_off_ticks = 0;

        // All ports inactive (active-low = 0xFF)
        pthread_mutex_lock(&g_pcmSim.mutex);
        g_pcmSim.regs[0][0] = 0xFF;  // Port A0
        g_pcmSim.regs[0][1] = 0xFF;  // Port B0
        g_pcmSim.regs[0][2] = 0xFF;  // Port C0
        pthread_mutex_unlock(&g_pcmSim.mutex);

        g_pcmSim.step = 1;
        break;

    case 1:
        // Wait off_time milliseconds (syncs inactive)
        g_pcmSim.sync_off_ticks++;
        if (g_pcmSim.sync_off_ticks < (unsigned int)g_pcmSim.off_time)
            return;

        g_pcmSim.step = 2;
        break;

    case 2:
        // Activate syncs and set grade signals
        g_pcmSim.shk_cnt++;

        // All syncs ON (bits 0-7)
        sync_bits = 0xFF;

        // Check each sync for zero pulse
        zero_bits = 0;
        if (app && app->pShm)
        {
            for (i = 0; i < MAXSYNCS && i < 8; i++)
            {
                app_type::TSyncStatus* pSync = &app->pShm->SyncStatus[i];
                if (pSync->shackleno == app->pShm->sys_set.Shackles)
                {
                    zero_bits |= SimMask[i];
                }
            }
        }

        // Grade cycling
        if (g_pcmSim.grade_idx++ >= 4)
            g_pcmSim.grade_idx = 0;

        // Build switch_in: grade sync bit + current grade bit
        switch_bits = (unsigned char)(SimMask[GRADESYNCBIT] | SimMask[sim_grade_bits[g_pcmSim.grade_idx]]);

        // Check for grade zero
        if (app && app->pShm)
        {
            if (app->pShm->grade_shackle[0] == app->pShm->sys_set.Shackles)
            {
                switch_bits |= SimMask[GRADEZEROBIT];
            }

            // Grade sync 2 support
            if (app->pShm->sys_set.MiscFeatures.EnableGradeSync2)
            {
                switch_bits = (unsigned char)(SimMask[GRADESYNC2BIT] | SimMask[sim_grade_bits[g_pcmSim.grade_idx]]);

                if (app->pShm->grade_shackle[MAXGRADESYNCS - 1] == app->pShm->sys_set.Shackles)
                {
                    switch_bits |= SimMask[GRADEZERO2BIT];
                }
            }
        }

        // Convert to port register values (active-low with bit interleaving)
        pthread_mutex_lock(&g_pcmSim.mutex);
        g_pcmSim.regs[0][0] = BuildPortA0(sync_bits, zero_bits);       // Port A0
        g_pcmSim.regs[0][1] = BuildPortB0(sync_bits, zero_bits);       // Port B0
        g_pcmSim.regs[0][2] = BuildPortC0(switch_bits);                // Port C0
        pthread_mutex_unlock(&g_pcmSim.mutex);

        g_pcmSim.step = 3;
        break;

    case 3:
        // Keep syncs active for on_time milliseconds
        g_pcmSim.sync_on_ticks++;
        if (g_pcmSim.sync_on_ticks < (unsigned int)g_pcmSim.on_time)
            return;

        g_pcmSim.step = 0;
        break;

    default:
        g_pcmSim.step = 0;
        break;
    }
}

//--------------------------------------------------------
// Simulation thread entry point
// Runs at 1ms intervals matching the RTX timer
//--------------------------------------------------------
static void* SimThread(void* arg)
{
    (void)arg;
    struct timespec ts;

    RtPrintf("PCM-3724 I/O simulator thread started\n");

    while (g_pcmSim.running)
    {
        RunSimStep();

        // Sleep 1ms (matching RTX timer interval)
        ts.tv_sec = 0;
        ts.tv_nsec = 1000000;  // 1ms
        clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
    }

    RtPrintf("PCM-3724 I/O simulator thread stopped\n");
    return NULL;
}

//--------------------------------------------------------
// PCM3724Sim_Init: Initialize virtual boards
//--------------------------------------------------------
void PCM3724Sim_Init(void)
{
    int i, j;

    memset(&g_pcmSim, 0, sizeof(g_pcmSim));
    pthread_mutex_init(&g_pcmSim.mutex, NULL);

    // All input ports default to 0xFF (active-low = nothing active)
    for (i = 0; i < PCM_SIM_NUM_BOARDS; i++)
    {
        for (j = 0; j < PCM_SIM_REGS_PER_BOARD; j++)
        {
            g_pcmSim.regs[i][j] = 0xFF;
        }
    }

    // Calculate timing (same as Simulator::initialize)
    g_pcmSim.bpm_setpoint = 200.0;
    double bps = g_pcmSim.bpm_setpoint / 60.0;
    double cycle_time_ms = (1.0 / bps) * 1000.0;
    double on_time_mult = 0.180;
    g_pcmSim.on_time = on_time_mult * cycle_time_ms;
    g_pcmSim.off_time = cycle_time_ms - g_pcmSim.on_time;
    g_pcmSim.grade_idx = 0;
    g_pcmSim.shk_cnt = 0;
    g_pcmSim.step = 0;

    RtPrintf("PCM-3724 I/O Simulator initialized\n");
    RtPrintf("  Line Speed   %3d Shk/min.\n", (int)g_pcmSim.bpm_setpoint);
    RtPrintf("  Sync On      %3d msec.\n", (int)g_pcmSim.on_time);
    RtPrintf("  Sync Off     %3d msec.\n", (int)g_pcmSim.off_time);
}

//--------------------------------------------------------
// PCM3724Sim_Start: Start the simulation thread
//--------------------------------------------------------
void PCM3724Sim_Start(void)
{
    if (g_pcmSim.running)
        return;

    g_pcmSim.running = true;

    if (pthread_create(&g_pcmSim.sim_thread, NULL, SimThread, NULL) != 0)
    {
        RtPrintf("Error: PCM-3724 sim thread creation failed\n");
        g_pcmSim.running = false;
        return;
    }

    RtPrintf("PCM-3724 I/O Simulator started\n");
}

//--------------------------------------------------------
// PCM3724Sim_Stop: Stop the simulation thread
//--------------------------------------------------------
void PCM3724Sim_Stop(void)
{
    if (!g_pcmSim.running)
        return;

    g_pcmSim.running = false;
    pthread_join(g_pcmSim.sim_thread, NULL);

    RtPrintf("PCM-3724 I/O Simulator stopped\n");
}

//--------------------------------------------------------
// PCM3724Sim_Cleanup: Free resources
//--------------------------------------------------------
void PCM3724Sim_Cleanup(void)
{
    PCM3724Sim_Stop();
    pthread_mutex_destroy(&g_pcmSim.mutex);
}

//--------------------------------------------------------
// PCM3724Sim_Read: Read a virtual port register
// Called from RtReadPortUchar in platform.h
//--------------------------------------------------------
unsigned char PCM3724Sim_Read(uintptr_t addr)
{
    int board, reg_offset;
    unsigned char val;

    board = AddrToBoard(addr, &reg_offset);
    if (board < 0)
        return 0xFF;  // Unknown address = inactive

    pthread_mutex_lock(&g_pcmSim.mutex);
    val = g_pcmSim.regs[board][reg_offset];
    pthread_mutex_unlock(&g_pcmSim.mutex);

    return val;
}

//--------------------------------------------------------
// PCM3724Sim_Write: Write a virtual port register
// Called from RtWritePortUchar in platform.h
// Stores output values (solenoid states) for inspection
//--------------------------------------------------------
void PCM3724Sim_Write(uintptr_t addr, unsigned char value)
{
    int board, reg_offset;

    board = AddrToBoard(addr, &reg_offset);
    if (board < 0)
        return;  // Unknown address, ignore

    pthread_mutex_lock(&g_pcmSim.mutex);
    g_pcmSim.regs[board][reg_offset] = value;
    pthread_mutex_unlock(&g_pcmSim.mutex);
}
