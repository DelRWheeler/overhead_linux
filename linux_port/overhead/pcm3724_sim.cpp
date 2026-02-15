//////////////////////////////////////////////////////////////////////
// pcm3724_sim.cpp: Virtual PCM-3724 I/O board simulator
//
// Generates physically accurate sensor patterns at the hardware
// register level so the controller exercises the full I/O code path:
//   readInputs() -> RtReadPortUchar() -> virtual registers -> bit shuffling
//
// Physical model:
//   - Trolleys on 6" centers, 1.125" wide
//   - Counter sensor (higher) fires on every trolley
//   - Zero sensor (lower, offset 1.5" horizontally) fires on every trolley
//   - Zero sensor also sees the zero flag (bolted between two trolleys)
//   - When the flag passes, the zero sensor stays ON for one full cycle
//     (6" of travel) instead of just the trolley width (1.125")
//   - The controller detects the extended zero pulse = zeroing event
//   - One flag on the chain travels through all sensors in sequence:
//     grade -> scale 1 -> scale 2 -> drop 1 -> drop 2 -> ... -> drop 6
//
// The simulation thread runs at 1ms intervals (same as the RTX timer).
//
//////////////////////////////////////////////////////////////////////

#include "types.h"
#include "pcm3724_sim.h"
#ifdef _HBM_SIM_
#include "hbm_sim.h"
#endif

#undef  _FILE_
#define _FILE_  "pcm3724_sim.cpp"

// Global instance
PCM3724SimState g_pcmSim;

// Access to overhead app (declared in overheadext.h as 'overhead* app')
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
//   Port A0 even bits (0,2,4,6) -> sync_in[0] bits 0-3
//   Port A0 odd bits  (1,3,5,7) -> sync_zero[0] bits 0-3
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
        // Even bits = syncs (bits 0,2,4,6 -> sync_in bits 0,1,2,3)
        if (sync_lo & SimMask[i])
            active |= SimMask[i * 2];

        // Odd bits = zeros (bits 1,3,5,7 -> sync_zero bits 0,1,2,3)
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
//   Port B0 even bits (0,2,4,6) -> sync_in[0] bits 4-7
//   Port B0 odd bits  (1,3,5,7) -> sync_zero[0] bits 4-7
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
// Port C0 -> switch_in[0] directly (no bit shuffling)
// Just active-low inversion.
//--------------------------------------------------------
static unsigned char BuildPortC0(unsigned char switch_val)
{
    return (unsigned char)(~switch_val);
}

//--------------------------------------------------------
// Calculate timing parameters from current SPM
//
// All timing derived from physical constants:
//   line_speed = SPM * trolley_spacing / 60  (inches/sec)
//   counter_on = trolley_width / line_speed   (seconds)
//   zero_delay = sensor_offset / line_speed   (seconds)
//   cycle_time = 60 / SPM                     (seconds)
//--------------------------------------------------------
static void RecalcTiming(double spm)
{
    if (spm < 1.0) spm = PCM_SIM_DEFAULT_SPM;

    double line_speed = spm * PCM_SIM_TROLLEY_SPACING / 60.0;  // inches/sec

    // How long a trolley blocks a sensor (counter or zero)
    double trolley_on_sec = PCM_SIM_TROLLEY_WIDTH / line_speed;

    // Delay from zero sensor to counter sensor (zero fires first, counter is offset)
    double counter_delay_sec = PCM_SIM_SENSOR_OFFSET / line_speed;

    // Total cycle time per shackle
    double cycle_sec = 60.0 / spm;

    // Flag holds zero ON for its physical length
    double flag_on_sec = PCM_SIM_FLAG_LENGTH / line_speed;

    // Convert to ticks (1 tick = 1ms)
    g_pcmSim.zero_on_ticks       = (unsigned int)(trolley_on_sec * 1000.0 + 0.5);
    g_pcmSim.counter_on_ticks    = (unsigned int)(trolley_on_sec * 1000.0 + 0.5);
    g_pcmSim.counter_start_tick  = (unsigned int)(counter_delay_sec * 1000.0 + 0.5);
    g_pcmSim.cycle_ticks         = (unsigned int)(cycle_sec * 1000.0 + 0.5);
    g_pcmSim.zero_flag_ticks     = (unsigned int)(flag_on_sec * 1000.0 + 0.5);

    // Sanity clamps
    if (g_pcmSim.counter_on_ticks < 1)    g_pcmSim.counter_on_ticks = 1;
    if (g_pcmSim.zero_on_ticks < 1)       g_pcmSim.zero_on_ticks = 1;
    if (g_pcmSim.counter_start_tick < 1)   g_pcmSim.counter_start_tick = 1;
    if (g_pcmSim.cycle_ticks < 10)         g_pcmSim.cycle_ticks = 10;
}

//--------------------------------------------------------
// Build the sync mask and zero flag entry table from
// current controller configuration
//--------------------------------------------------------
static void ReconfigSyncs(void)
{
    g_pcmSim.sync_mask = 0;
    g_pcmSim.num_zero_entries = 0;

    if (!app || !app->pShm)
    {
        // Fallback: scale 1 only
        g_pcmSim.sync_mask = SimMask[0];
        return;
    }

    // Scale 1 sync is always active (index 0)
    g_pcmSim.sync_mask |= SimMask[0];

    // Scale 2 sync only if dual scale (index 1)
    if (app->pShm->scl_set.NumScales >= 2)
        g_pcmSim.sync_mask |= SimMask[1];

    // Drop syncs: indices 2-7 (only if they have drops assigned)
    for (int i = 2; i < 8; i++)
    {
        if (app->pShm->sys_set.SyncSettings[i].first > 0)
            g_pcmSim.sync_mask |= SimMask[i];
    }

    // Shackles per revolution
    g_pcmSim.shackles_per_rev = app->pShm->sys_set.Shackles;
    if (g_pcmSim.shackles_per_rev < 100)
        g_pcmSim.shackles_per_rev = 800;  // Sane default

    // Build zero flag arrival table
    // The flag is a single physical marker that travels through sensors in order:
    //   grade -> scale 1 -> (scale 2) -> drop 1 -> drop 2 -> ... -> drop 6
    //
    // Flag arrives at grade after PCM_SIM_FLAG_TO_GRADE shackles from start.
    // Then each subsequent sensor is spaced along the chain.

    int entry = 0;
    int arrival = PCM_SIM_FLAG_TO_GRADE;

    // Grade sync (always present)
    g_pcmSim.zero_entries[entry].sync_index = 0;
    g_pcmSim.zero_entries[entry].is_grade = true;
    g_pcmSim.zero_entries[entry].flag_arrival = arrival;
    g_pcmSim.zero_entries[entry].initial_zeroed = false;
    entry++;

    // Scale 1 (sync index 0)
    arrival += PCM_SIM_GRADE_TO_SCALE1;
    g_pcmSim.zero_entries[entry].sync_index = 0;
    g_pcmSim.zero_entries[entry].is_grade = false;
    g_pcmSim.zero_entries[entry].flag_arrival = arrival;
    g_pcmSim.zero_entries[entry].initial_zeroed = false;
    entry++;

    // Scale 2 (sync index 1) - only if configured
    if (app->pShm->scl_set.NumScales >= 2)
    {
        // Scale 2 is physically close to scale 1 (use same drop-to-drop spacing)
        arrival += PCM_SIM_DROP_TO_DROP;
        g_pcmSim.zero_entries[entry].sync_index = 1;
        g_pcmSim.zero_entries[entry].is_grade = false;
        g_pcmSim.zero_entries[entry].flag_arrival = arrival;
        g_pcmSim.zero_entries[entry].initial_zeroed = false;
        entry++;
    }

    // Drop syncs (indices 2-7)
    arrival += PCM_SIM_SCALE1_TO_DROP1;
    for (int i = 2; i < 8; i++)
    {
        if (!(g_pcmSim.sync_mask & SimMask[i]))
            continue;

        g_pcmSim.zero_entries[entry].sync_index = i;
        g_pcmSim.zero_entries[entry].is_grade = false;
        g_pcmSim.zero_entries[entry].flag_arrival = arrival;
        g_pcmSim.zero_entries[entry].initial_zeroed = false;
        entry++;

        arrival += PCM_SIM_DROP_TO_DROP;
    }

    g_pcmSim.num_zero_entries = entry;
}

//--------------------------------------------------------
// Check if the zero flag is currently at a given sync,
// causing an extended zero pulse.
//
// The flag arrives at each sync at a specific shackle count
// (flag_arrival). After initial zeroing, it comes back every
// shackles_per_rev counts. When present, the zero stays ON
// for one full cycle (6" of travel).
//
// Returns true if the flag is at this sync for the current
// shackle_count.
//--------------------------------------------------------
static bool IsFlagAtSync(int entry_idx)
{
    if (entry_idx < 0 || entry_idx >= g_pcmSim.num_zero_entries)
        return false;

    PCM3724SimZeroEntry* e = &g_pcmSim.zero_entries[entry_idx];
    int count = g_pcmSim.shackle_count;
    int rev = g_pcmSim.shackles_per_rev;

    // Initial zeroing: flag arrives at flag_arrival shackle count
    if (!e->initial_zeroed)
    {
        if (count == e->flag_arrival)
        {
            // Flag is at this sync for exactly 1 shackle cycle
            return true;
        }
        if (count > e->flag_arrival)
        {
            // Flag has passed, mark initial zero as done
            e->initial_zeroed = true;
        }
        return false;
    }

    // Steady-state: flag returns every shackles_per_rev after initial arrival
    if (rev > 0)
    {
        int since_initial = count - e->flag_arrival;
        if (since_initial > 0 && (since_initial % rev) == 0)
            return true;  // Flag is here for exactly 1 cycle
    }

    return false;
}

//--------------------------------------------------------
// RunSimStep: One tick of the simulation
//
// Called every 1ms by the simulation thread.
//
// Within each shackle cycle (cycle_ticks ms):
//   t=0                  : counter bits ON (all configured syncs)
//   t=counter_on_ticks   : counter bits OFF
//   t=zero_start_tick    : zero bits ON (all configured syncs)
//   t=zero_start_tick+N  : zero bits OFF
//     where N = zero_on_ticks (normal trolley)
//           N = zero_flag_ticks (when flag is present at that sync)
//   t=cycle_ticks        : next shackle, increment count
//
// Grade signals (Port C0) follow the same counter/zero pattern
// but also include grade value bits.
//--------------------------------------------------------
static void RunSimStep(void)
{
    // Check for auto-start: wait for config_Ok then 10 second delay
    if (!g_pcmSim.started)
    {
        if (g_pcmSim.waiting_for_config)
        {
            if (app && app->config_Ok)
            {
                g_pcmSim.waiting_for_config = false;
                g_pcmSim.config_ok_tick = 0;
                RtPrintf("PCM-3724 Sim: config_Ok detected, waiting 10s to start line\n");

                // Now that config is loaded, set up timing and syncs
                // Always use default SPM - don't read from sys_stat which
                // is the measured value and may be stale/zero at startup
                double spm = PCM_SIM_DEFAULT_SPM;
                RecalcTiming(spm);
                ReconfigSyncs();

                RtPrintf("PCM-3724 Sim: SPM=%.0f cycle=%dms zero_on=%dms counter_start=%dms counter_on=%dms flag_on=%dms\n",
                    spm, g_pcmSim.cycle_ticks, g_pcmSim.zero_on_ticks,
                    g_pcmSim.counter_start_tick, g_pcmSim.counter_on_ticks, g_pcmSim.zero_flag_ticks);
                RtPrintf("PCM-3724 Sim: sync_mask=0x%02X shackles_per_rev=%d zero_entries=%d\n",
                    g_pcmSim.sync_mask, g_pcmSim.shackles_per_rev, g_pcmSim.num_zero_entries);

                for (int i = 0; i < g_pcmSim.num_zero_entries; i++)
                {
                    PCM3724SimZeroEntry* e = &g_pcmSim.zero_entries[i];
                    RtPrintf("  Zero entry %d: %s sync=%d arrives@shackle=%d\n",
                        i, e->is_grade ? "GRADE" : "SYNC", e->sync_index, e->flag_arrival);
                }
            }
            return;  // Still waiting for config
        }

        // Counting down the 30-second delay
        g_pcmSim.config_ok_tick++;
        if (g_pcmSim.config_ok_tick < g_pcmSim.autostart_delay)
            return;

        // Auto-start!
        g_pcmSim.started = true;
        g_pcmSim.tick = 0;
        g_pcmSim.shackle_count = 0;
        RtPrintf("PCM-3724 Sim: Line auto-started after 10s delay\n");
    }

    // --- Active simulation ---

    unsigned char sync_bits  = 0;   // Counter sensor state (bits 0-7)
    unsigned char zero_bits  = 0;   // Zero sensor state (bits 0-7)
    unsigned char switch_bits = 0;  // Port C0 (grade signals)
    bool grade_counter_on = false;
    bool grade_zero_on = false;

    unsigned int t = g_pcmSim.tick;

    // --- Zero bits: ON from t=0 (zero sensor fires FIRST) ---
    // Normal trolley: ON for zero_on_ticks
    // Flag present: ON for zero_flag_ticks (full 6" flag length)
    for (int i = 0; i < 8; i++)
    {
        if (!(g_pcmSim.sync_mask & SimMask[i]))
            continue;

        // Check if the zero flag is at this sync
        bool flag_here = false;
        for (int e = 0; e < g_pcmSim.num_zero_entries; e++)
        {
            if (!g_pcmSim.zero_entries[e].is_grade &&
                g_pcmSim.zero_entries[e].sync_index == i)
            {
                flag_here = IsFlagAtSync(e);
                break;
            }
        }

        unsigned int duration = flag_here ? g_pcmSim.zero_flag_ticks : g_pcmSim.zero_on_ticks;
        if (t < duration)
            zero_bits |= SimMask[i];
    }

    // --- Grade zero: same logic, check grade entry ---
    {
        bool grade_flag = false;
        for (int e = 0; e < g_pcmSim.num_zero_entries; e++)
        {
            if (g_pcmSim.zero_entries[e].is_grade)
            {
                grade_flag = IsFlagAtSync(e);
                break;
            }
        }

        unsigned int grade_duration = grade_flag ? g_pcmSim.zero_flag_ticks : g_pcmSim.zero_on_ticks;
        if (t < grade_duration)
            grade_zero_on = true;
    }

    // --- Counter bits: ON from t=counter_start_tick (counter sensor fires SECOND) ---
    if (t >= g_pcmSim.counter_start_tick && t < (g_pcmSim.counter_start_tick + g_pcmSim.counter_on_ticks))
    {
        sync_bits = g_pcmSim.sync_mask;
        grade_counter_on = true;
    }

#ifdef _HBM_SIM_
    // --- Bird-present signaling to HBM load cell simulator ---
    // When counter fires and the controller has zeroed at least one scale,
    // signal that a bird is on the scale. This makes the HBM sim output
    // bird-level weights during the weighing window, and near-zero
    // (empty shackle) the rest of the time.
    //
    // Before zeroing: WeighZero is false → bird_present stays false →
    //   HBM outputs ~0 → tare captures ~0
    // After zeroing: WeighZero is true → bird_present pulses true →
    //   HBM outputs bird weight → net = bird - tare(~0) = correct weight
    if (t == g_pcmSim.counter_start_tick)
    {
        // Check if any scale has zeroed (production mode)
        if (app && app->pShm)
        {
            bool any_zeroed = false;
            for (int s = 0; s < MAXSCALES; s++)
            {
                if (app->pShm->WeighZero[s])
                {
                    any_zeroed = true;
                    break;
                }
            }
            if (any_zeroed)
            {
                g_hbm_bird_present = true;
                // Bird-present signaling is working; suppress periodic logging
            }
        }
    }
    // Clear bird-present at 75% of cycle (bird has left the scale,
    // well after the weighing window closes)
    if (t == (g_pcmSim.cycle_ticks * 3 / 4))
    {
        g_hbm_bird_present = false;
    }
#endif

    // --- Grade signals on Port C0 ---
    if (grade_counter_on)
    {
        // Grade sync pulse + current grade value bit
        switch_bits |= SimMask[GRADESYNCBIT];
        switch_bits |= SimMask[sim_grade_bits[g_pcmSim.grade_idx]];
    }
    if (grade_zero_on)
    {
        switch_bits |= SimMask[GRADEZEROBIT];
    }

    // --- Write to virtual port registers ---
    {
        unsigned char pa0 = BuildPortA0(sync_bits, zero_bits);
        unsigned char pb0 = BuildPortB0(sync_bits, zero_bits);
        unsigned char pc0 = BuildPortC0(switch_bits);

        pthread_mutex_lock(&g_pcmSim.mutex);
        g_pcmSim.regs[0][0] = pa0;
        g_pcmSim.regs[0][1] = pb0;
        g_pcmSim.regs[0][2] = pc0;
        pthread_mutex_unlock(&g_pcmSim.mutex);
    }

    // --- Advance tick, handle cycle rollover ---
    g_pcmSim.tick++;
    if (g_pcmSim.tick >= g_pcmSim.cycle_ticks)
    {
        g_pcmSim.tick = 0;
        g_pcmSim.shackle_count++;

        // Cycle grade on each new shackle
        g_pcmSim.grade_idx++;
        if (g_pcmSim.grade_idx >= 5)
            g_pcmSim.grade_idx = 0;

        // NOTE: Do NOT read SPM from sys_stat.ShacklesPerMinute here.
        // That field is the MEASURED value from BpmStats (updated once/min).
        // Reading it creates a destructive feedback loop: if zeroing takes
        // most of the first 60s window, BpmStats reports low SPM, sim slows
        // down, next BpmStats reports even lower, etc.
        // The real conveyor runs at a fixed speed set by the VFD - it never
        // adjusts based on the controller's reported SPM.
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

    // Set initial timing from defaults (will be recalculated when config loads)
    RecalcTiming(PCM_SIM_DEFAULT_SPM);

    g_pcmSim.grade_idx = 0;
    g_pcmSim.shackle_count = 0;
    g_pcmSim.tick = 0;
    g_pcmSim.started = false;
    g_pcmSim.waiting_for_config = true;
    g_pcmSim.config_ok_tick = 0;
    g_pcmSim.autostart_delay = 10000;  // 10 seconds in ticks (1ms each)

    RtPrintf("PCM-3724 I/O Simulator initialized\n");
    RtPrintf("  Default SPM  %3d\n", (int)PCM_SIM_DEFAULT_SPM);
    RtPrintf("  Cycle time   %3d ms\n", g_pcmSim.cycle_ticks);
    RtPrintf("  Zero ON      %3d ms (trolley width %.3f\", fires first)\n",
        g_pcmSim.zero_on_ticks, PCM_SIM_TROLLEY_WIDTH);
    RtPrintf("  Counter ON   %3d ms (trolley width, fires at +%dms)\n",
        g_pcmSim.counter_on_ticks, g_pcmSim.counter_start_tick);
    RtPrintf("  Zero flag ON %3d ms (%.1f\" flag length)\n", g_pcmSim.zero_flag_ticks, PCM_SIM_FLAG_LENGTH);
    RtPrintf("  Waiting for config_Ok then 10s auto-start delay\n");
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

    RtPrintf("PCM-3724 I/O Simulator thread running (waiting for config_Ok)\n");
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
