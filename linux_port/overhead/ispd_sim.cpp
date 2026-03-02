//////////////////////////////////////////////////////////////////////
// ispd_sim.cpp: Virtual H&B ISPD-20KG digital load cell simulator
//
// Simulates the H&B ISPD-20KG serial protocol so the controller
// exercises the full ISPD load cell code path:
//   init_adc() → ID, IV, IS, FL, FM, UR, GS (continuous polling)
//   SerialRead() → parses ASCII "S±NNNNNN\r" measurement lines
//
// The simulator:
//   1. Receives commands via ISPDSim_Write (from SerialWrite)
//   2. Parses commands and queues text responses into FIFO
//   3. When continuous mode active, a background thread pushes
//      ASCII measurement lines into the FIFO at ~586 Hz
//   4. ISPDSim_Read / ISPDSim_Available provide data to SerialRead
//
// H&B Protocol:
//   Commands terminated with CR (0x0D)
//   Responses terminated with CR (0x0D)
//   Measurement format: "S+015641\r" or "S-000500\r"
//
//////////////////////////////////////////////////////////////////////

#include "types.h"
#include "ispd_sim.h"
#include <stdlib.h>

#undef  _FILE_
#define _FILE_  "ispd_sim.cpp"

// Global simulator states (one per load cell)
ISPDSimState g_ispdSim[ISPD_SIM_MAX_LC];

// Bird-present flag and shackle sequence counter.
// Shared with HBM sim — set by sync processing code in overhead.cpp.
// When _LC_SIM_ is defined, overhead.cpp sets these instead of
// the old g_hbm_bird_present / g_hbm_shackle_seq globals.
volatile bool g_lc_bird_present = false;
volatile int  g_lc_shackle_seq = 0;

// Simulated weight table (pounds) - 2 to 6 lb range for testing schedules/grading
static const double ispd_sim_weights[ISPD_SIM_NUM_WEIGHTS] =
{
    2.10, 3.40, 4.80, 2.60, 5.20, 3.90, 4.30, 2.30,
    5.50, 3.10, 4.60, 5.80, 2.80, 4.10, 3.60, 5.40,
    2.50, 4.90, 3.30, 5.10, 2.90, 4.40, 3.70, 5.70,
    3.50, 2.40, 5.30, 4.20, 3.80, 5.90, 2.70, 4.50
};

//--------------------------------------------------------
// Map COM port number to simulator index
// COM1=0, COM2=1, COM3=2, COM4=3
//--------------------------------------------------------
static int PortToIndex(int port_num)
{
    if (port_num >= 0 && port_num < ISPD_SIM_MAX_LC)
        return port_num;
    return -1;
}

//--------------------------------------------------------
// FIFO operations (all require fifo_mutex held)
//--------------------------------------------------------

static int fifo_count(ISPDSimState* s)
{
    return (s->fifo_head - s->fifo_tail) & ISPD_SIM_FIFO_MASK;
}

static int fifo_free(ISPDSimState* s)
{
    return ISPD_SIM_FIFO_SIZE - 1 - fifo_count(s);
}

static void fifo_push(ISPDSimState* s, unsigned char byte)
{
    if (fifo_free(s) > 0)
    {
        s->fifo[s->fifo_head] = byte;
        s->fifo_head = (s->fifo_head + 1) & ISPD_SIM_FIFO_MASK;
    }
}

static void fifo_push_string(ISPDSimState* s, const char* str)
{
    while (*str && fifo_free(s) > 0)
    {
        s->fifo[s->fifo_head] = (unsigned char)*str;
        s->fifo_head = (s->fifo_head + 1) & ISPD_SIM_FIFO_MASK;
        str++;
    }
}

static unsigned char fifo_pop(ISPDSimState* s)
{
    unsigned char byte;
    if (fifo_count(s) == 0)
        return 0;
    byte = s->fifo[s->fifo_tail];
    s->fifo_tail = (s->fifo_tail + 1) & ISPD_SIM_FIFO_MASK;
    return byte;
}

//--------------------------------------------------------
// Push a text response with CR terminator into FIFO
// H&B uses CR-only line endings (not CRLF)
//--------------------------------------------------------
static void push_response(ISPDSimState* s, const char* response)
{
    fifo_push_string(s, response);
    fifo_push(s, 0x0D);  // CR only
}

//--------------------------------------------------------
// Push an ASCII measurement line into FIFO
//
// Format: "S±NNNNNN\r"
//   S = sample prefix
//   + or - = sign
//   6-digit zero-padded ADC count
//   CR terminator
//
// ISPD counts per pound: 13340 (vs HBM's 116500)
//--------------------------------------------------------
static void push_measurement_ascii(ISPDSimState* s, int adc_counts)
{
    char buf[16];
    char sign = '+';
    int abs_val = adc_counts;

    if (adc_counts < 0)
    {
        sign = '-';
        abs_val = -adc_counts;
    }

    sprintf(buf, "S%c%06d", sign, abs_val);
    fifo_push_string(s, buf);
    fifo_push(s, 0x0D);  // CR terminator
}

//--------------------------------------------------------
// Process a complete command from the controller
//
// H&B commands end with CR and are 2-letter ASCII.
// Query commands: "CMD\r" returns current value
// Set commands: "CMD N\r" sets value
//--------------------------------------------------------
static void process_command(ISPDSimState* s, const char* cmd)
{
    char response[128];

    // Flush stale responses (same rationale as HBM sim)
    s->fifo_head = s->fifo_tail;

    //RtPrintf("ISPD Sim[%d] cmd: %s\n", s->port_num, cmd);

    // ---- Device Identity ----
    if (strcmp(cmd, "ID") == 0)
    {
        push_response(s, "D:1520");
        return;
    }

    // ---- Firmware Version ----
    if (strcmp(cmd, "IV") == 0)
    {
        push_response(s, "V:0146");
        return;
    }

    // ---- Device Status ----
    if (strcmp(cmd, "IS") == 0)
    {
        push_response(s, "S:001000");
        return;
    }

    // ---- Get Raw ADC Sample (single shot) ----
    if (strcmp(cmd, "GS") == 0)
    {
        // Return a single measurement as ASCII
        bool bird = g_lc_bird_present;
        int adc_counts;

        if (bird)
        {
            double wt = ispd_sim_weights[s->weight_idx] * ISPD_CNTS;
            adc_counts = (int)wt;
        }
        else
        {
            adc_counts = 50;  // empty shackle baseline
        }

        adc_counts += (rand() % 21) - 10;  // ±10 noise

        sprintf(response, "S%c%06d", adc_counts >= 0 ? '+' : '-',
                adc_counts >= 0 ? adc_counts : -adc_counts);
        push_response(s, response);
        return;
    }

    // ---- Get Long Weight with Checksum ----
    if (strcmp(cmd, "GW") == 0)
    {
        bool bird = g_lc_bird_present;
        int adc_counts;

        if (bird)
        {
            double wt = ispd_sim_weights[s->weight_idx] * ISPD_CNTS;
            adc_counts = (int)wt;
        }
        else
        {
            adc_counts = 50;
        }

        adc_counts += (rand() % 21) - 10;

        // W±NNNNNN±NNNNNNSSCC format (weight, tare, status, checksum)
        // For simplicity, tare=0, status=00, checksum=00
        sprintf(response, "W%c%06d+00000000%02X",
                adc_counts >= 0 ? '+' : '-',
                adc_counts >= 0 ? adc_counts : -adc_counts,
                0);
        push_response(s, response);
        return;
    }

    // ---- Start Continuous Output (SX) ----
    if (strcmp(cmd, "SX") == 0)
    {
        s->continuous = true;
        // No text response - switches to measurement stream
        return;
    }

    // ---- Filter Level ----
    if (strcmp(cmd, "FL") == 0)
    {
        // Query: return current value
        sprintf(response, "%d", s->FL);
        push_response(s, response);
        return;
    }
    if (strncmp(cmd, "FL ", 3) == 0)
    {
        s->FL = atoi(cmd + 3);
        push_response(s, "OK");
        return;
    }

    // ---- Filter Mode ----
    if (strcmp(cmd, "FM") == 0)
    {
        sprintf(response, "%d", s->FM);
        push_response(s, response);
        return;
    }
    if (strncmp(cmd, "FM ", 3) == 0)
    {
        s->FM = atoi(cmd + 3);
        push_response(s, "OK");
        return;
    }

    // ---- Update Rate ----
    if (strcmp(cmd, "UR") == 0)
    {
        sprintf(response, "%d", s->UR);
        push_response(s, response);
        return;
    }
    if (strncmp(cmd, "UR ", 3) == 0)
    {
        s->UR = atoi(cmd + 3);
        push_response(s, "OK");
        return;
    }

    // ---- Decimal Point ----
    if (strcmp(cmd, "DP") == 0)
    {
        sprintf(response, "%d", s->DP);
        push_response(s, response);
        return;
    }
    if (strncmp(cmd, "DP ", 3) == 0)
    {
        s->DP = atoi(cmd + 3);
        push_response(s, "OK");
        return;
    }

    // ---- Set Zero ----
    if (strcmp(cmd, "SZ") == 0)
    {
        push_response(s, "OK");
        return;
    }

    // ---- Set Tare ----
    if (strcmp(cmd, "ST") == 0)
    {
        push_response(s, "OK");
        return;
    }

    // ---- Write Parameters to EEPROM ----
    if (strcmp(cmd, "WP") == 0)
    {
        push_response(s, "OK");
        return;
    }

    // Unknown command - echo "OK"
    //RtPrintf("ISPD Sim[%d] unknown cmd: %s\n", s->port_num, cmd);
    push_response(s, "OK");
}

//--------------------------------------------------------
// Background thread: generates continuous measurements
//
// When s->continuous is true, pushes ASCII measurement
// lines into the FIFO at approximately the real ISPD rate.
//--------------------------------------------------------
static void* ISPDSimMeasThread(void* arg)
{
    ISPDSimState* s = (ISPDSimState*)arg;
    struct timespec ts;
    double wt;
    int adc_counts;

    RtPrintf("ISPD-20KG Sim[%d] measurement thread started\n", s->port_num);

    while (s->running)
    {
        if (s->continuous)
        {
            bool bird = g_lc_bird_present;

            if (bird)
            {
                // Advance weight on each new shackle using sequence counter
                int seq = g_lc_shackle_seq;
                if (seq != s->prev_seq)
                {
                    s->weight_idx = (s->weight_idx + 1) % ISPD_SIM_NUM_WEIGHTS;
                    s->prev_seq = seq;
                }

                wt = ispd_sim_weights[s->weight_idx] * ISPD_CNTS;
                adc_counts = (int)wt;
            }
            else
            {
                adc_counts = 50;  // empty shackle baseline
            }

            // Add small random noise (±10 ADC counts)
            adc_counts += (rand() % 21) - 10;

            // Push ASCII measurement line into FIFO
            pthread_mutex_lock(&s->fifo_mutex);
            push_measurement_ascii(s, adc_counts);
            pthread_mutex_unlock(&s->fifo_mutex);

            // Sleep at measurement rate (~586 Hz)
            ts.tv_sec = 0;
            ts.tv_nsec = ISPD_SIM_MEAS_INTERVAL_US * 1000;
            clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
        }
        else
        {
            // Not in continuous mode - sleep longer
            ts.tv_sec = 0;
            ts.tv_nsec = 10000000;  // 10ms
            clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
        }
    }

    RtPrintf("ISPD-20KG Sim[%d] measurement thread stopped\n", s->port_num);
    return NULL;
}

//--------------------------------------------------------
// ISPDSim_Init: Initialize a load cell simulator instance
//--------------------------------------------------------
void ISPDSim_Init(int port_num)
{
    int idx = PortToIndex(port_num);
    if (idx < 0) return;

    ISPDSimState* s = &g_ispdSim[idx];

    if (s->initialized)
        return;

    memset(s, 0, sizeof(ISPDSimState));
    pthread_mutex_init(&s->fifo_mutex, NULL);

    s->port_num = port_num;
    s->fifo_head = 0;
    s->fifo_tail = 0;
    s->cmd_idx = 0;

    // Default H&B settings
    s->FL = 3;      // filter level
    s->FM = 0;      // filter mode (IIR)
    s->UR = 1;      // update rate divider
    s->DP = 0;      // decimal point

    s->continuous = false;
    s->weight_idx = 0;

    // Start measurement thread
    s->running = true;
    if (pthread_create(&s->meas_thread, NULL, ISPDSimMeasThread, s) != 0)
    {
        RtPrintf("ISPD-20KG Sim[%d] thread creation failed\n", port_num);
        s->running = false;
    }

    s->initialized = true;

    RtPrintf("ISPD-20KG Load Cell Simulator initialized on COM%d\n", port_num + 1);
    RtPrintf("  Settings: FL=%d FM=%d UR=%d  Counts/lb=%d\n",
        s->FL, s->FM, s->UR, ISPD_CNTS);
}

//--------------------------------------------------------
// ISPDSim_Write: Receive data from controller (commands)
//
// Buffers incoming bytes until CR (0x0D) is found, then
// processes the complete command.
// H&B uses CR-only termination (NOT semicolon like HBM).
//--------------------------------------------------------
int ISPDSim_Write(int port_num, const unsigned char* data, int len)
{
    int idx = PortToIndex(port_num);
    if (idx < 0) return 0;

    ISPDSimState* s = &g_ispdSim[idx];
    int i;

    for (i = 0; i < len; i++)
    {
        char ch = (char)data[i];

        // Skip LF bytes (in case controller sends CRLF)
        if (ch == 0x0A)
            continue;

        // CR marks end of command
        if (ch == 0x0D)
        {
            s->cmd_buf[s->cmd_idx] = '\0';

            if (s->cmd_idx > 0)
            {
                // Any command sent while in continuous mode stops it
                // (except SX which starts it)
                if (s->continuous && strcmp(s->cmd_buf, "SX") != 0)
                {
                    s->continuous = false;
                }

                // Process the command (puts response in FIFO)
                pthread_mutex_lock(&s->fifo_mutex);
                process_command(s, s->cmd_buf);
                pthread_mutex_unlock(&s->fifo_mutex);
            }

            // Reset command buffer
            s->cmd_idx = 0;
        }
        else
        {
            // Accumulate command bytes
            if (s->cmd_idx < ISPD_SIM_CMD_MAX - 1)
            {
                s->cmd_buf[s->cmd_idx++] = ch;
            }
        }
    }

    return len;
}

//--------------------------------------------------------
// ISPDSim_Read: Read data from load cell (responses/measurements)
//
// Returns data from the FIFO ring buffer.
//--------------------------------------------------------
int ISPDSim_Read(int port_num, unsigned char* buffer, int max_len)
{
    int idx = PortToIndex(port_num);
    if (idx < 0) return 0;

    ISPDSimState* s = &g_ispdSim[idx];
    int count, i;

    pthread_mutex_lock(&s->fifo_mutex);

    count = fifo_count(s);
    if (count > max_len)
        count = max_len;

    for (i = 0; i < count; i++)
    {
        buffer[i] = fifo_pop(s);
    }

    pthread_mutex_unlock(&s->fifo_mutex);

    return count;
}

//--------------------------------------------------------
// ISPDSim_Available: Get number of bytes ready to read
//--------------------------------------------------------
int ISPDSim_Available(int port_num)
{
    int idx = PortToIndex(port_num);
    if (idx < 0) return 0;

    ISPDSimState* s = &g_ispdSim[idx];
    int count;

    pthread_mutex_lock(&s->fifo_mutex);
    count = fifo_count(s);
    pthread_mutex_unlock(&s->fifo_mutex);

    return count;
}

//--------------------------------------------------------
// ISPDSim_Cleanup: Stop and clean up a simulator instance
//--------------------------------------------------------
void ISPDSim_Cleanup(int port_num)
{
    int idx = PortToIndex(port_num);
    if (idx < 0) return;

    ISPDSimState* s = &g_ispdSim[idx];

    if (!s->initialized)
        return;

    s->running = false;
    s->continuous = false;
    pthread_join(s->meas_thread, NULL);
    pthread_mutex_destroy(&s->fifo_mutex);

    s->initialized = false;

    RtPrintf("ISPD-20KG Sim[%d] cleaned up\n", port_num);
}

//--------------------------------------------------------
// ISPDSim_CleanupAll: Clean up all instances
//--------------------------------------------------------
void ISPDSim_CleanupAll(void)
{
    int i;
    for (i = 0; i < ISPD_SIM_MAX_LC; i++)
    {
        if (g_ispdSim[i].initialized)
            ISPDSim_Cleanup(i);
    }
}
