//////////////////////////////////////////////////////////////////////
// hbm_sim.cpp: Virtual HBM FIT7 digital load cell simulator
//
// Simulates the HBM FIT7 serial protocol so the controller
// exercises the full HBM load cell code path:
//   init_adc() → STR, IDN?, BDR?, COF, CSM, MSV?0
//   HBMCheckSettings() → reads all 21 dig_lc_settings
//   SerialRead() → parses 4-byte binary measurement packets
//
// The simulator:
//   1. Receives commands via HBMSim_Write (from SerialWrite)
//   2. Parses commands and queues text responses into FIFO
//   3. When continuous mode active, a background thread pushes
//      4-byte measurement packets into the FIFO at ~600 Hz
//   4. HBMSim_Read / HBMSim_Available provide data to SerialRead
//
//////////////////////////////////////////////////////////////////////

#include "types.h"
#include "hbm_sim.h"
#include <stdlib.h>

#undef  _FILE_
#define _FILE_  "hbm_sim.cpp"

// Global simulator states (one per load cell)
HBMSimState g_hbmSim[HBM_SIM_MAX_LC];

// Bird-present flag and shackle sequence counter.
// These are now defined in ispd_sim.cpp as g_lc_bird_present / g_lc_shackle_seq
// and aliased via #defines in hbm_sim.h for backwards compatibility.
// (The actual globals live in ispd_sim.cpp to avoid duplicate definitions.)

// Simulated weight table (pounds) - 2 to 6 lb range for testing schedules/grading
static const double hbm_sim_weights[HBM_SIM_NUM_WEIGHTS] =
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
    if (port_num >= 0 && port_num < HBM_SIM_MAX_LC)
        return port_num;
    return -1;
}

//--------------------------------------------------------
// FIFO operations (all require fifo_mutex held)
//--------------------------------------------------------

static int fifo_count(HBMSimState* s)
{
    return (s->fifo_head - s->fifo_tail) & HBM_SIM_FIFO_MASK;
}

static int fifo_free(HBMSimState* s)
{
    return HBM_SIM_FIFO_SIZE - 1 - fifo_count(s);
}

static void fifo_push(HBMSimState* s, unsigned char byte)
{
    if (fifo_free(s) > 0)
    {
        s->fifo[s->fifo_head] = byte;
        s->fifo_head = (s->fifo_head + 1) & HBM_SIM_FIFO_MASK;
    }
}

static void fifo_push_bytes(HBMSimState* s, const unsigned char* data, int len)
{
    int i;
    for (i = 0; i < len && fifo_free(s) > 0; i++)
    {
        s->fifo[s->fifo_head] = data[i];
        s->fifo_head = (s->fifo_head + 1) & HBM_SIM_FIFO_MASK;
    }
}

static void fifo_push_string(HBMSimState* s, const char* str)
{
    while (*str && fifo_free(s) > 0)
    {
        s->fifo[s->fifo_head] = (unsigned char)*str;
        s->fifo_head = (s->fifo_head + 1) & HBM_SIM_FIFO_MASK;
        str++;
    }
}

static unsigned char fifo_pop(HBMSimState* s)
{
    unsigned char byte;
    if (fifo_count(s) == 0)
        return 0;
    byte = s->fifo[s->fifo_tail];
    s->fifo_tail = (s->fifo_tail + 1) & HBM_SIM_FIFO_MASK;
    return byte;
}

//--------------------------------------------------------
// Push a text response with CRLF terminator into FIFO
//--------------------------------------------------------
static void push_response(HBMSimState* s, const char* response)
{
    fifo_push_string(s, response);
    fifo_push(s, 0x0D);  // CR
    fifo_push(s, 0x0A);  // LF
}

//--------------------------------------------------------
// Generate a 4-byte binary measurement packet
//
// Format (COF=8, MSB→LSB, checksum):
//   byte[0] = MSB of 24-bit value
//   byte[1] = middle byte
//   byte[2] = LSB of 24-bit value
//   byte[3] = XOR checksum of bytes 0-2
//
// The value is raw ADC counts: weight_lbs * CNTS
//--------------------------------------------------------
static void push_measurement(HBMSimState* s, int adc_counts)
{
    unsigned char pkt[4];

    // Handle negative values (24-bit two's complement)
    unsigned int val;
    if (adc_counts < 0)
        val = (unsigned int)(adc_counts + 0x1000000) & 0xFFFFFF;
    else
        val = (unsigned int)adc_counts & 0xFFFFFF;

    pkt[0] = (unsigned char)((val >> 16) & 0xFF);  // MSB
    pkt[1] = (unsigned char)((val >> 8)  & 0xFF);   // mid
    pkt[2] = (unsigned char)((val)       & 0xFF);   // LSB
    pkt[3] = pkt[0] ^ pkt[1] ^ pkt[2];              // checksum

    fifo_push_bytes(s, pkt, 4);
}

//--------------------------------------------------------
// Process a complete command from the controller
//
// Commands end with ';' and are text-based.
// Query commands have '?' before the ';'
// Set commands have a space and parameter before ';'
//--------------------------------------------------------
static void process_command(HBMSimState* s, const char* cmd)
{
    char response[128];

    // Flush any stale unconsumed responses from the FIFO.
    // This prevents cascading misalignment when the controller's
    // SendCommand doesn't read a response (e.g., BDR? with
    // response_time=500 but ResponseCnt=1000, so the wait loop
    // condition 1000<500 is false and the response is never consumed).
    // Without this flush, stale responses shift all subsequent
    // command/response pairs by one, causing init_adc to fail.
    s->fifo_head = s->fifo_tail;

    //RtPrintf("HBM Sim[%d] cmd: %s\n", s->port_num, cmd);

    // ---- Identity ----
    if (strcmp(cmd, "IDN?;") == 0)
    {
        push_response(s, "HBM,FIT7i,SIM,1.0");
        return;
    }

    // ---- Baud rate ----
    if (strcmp(cmd, "BDR?;") == 0)
    {
        sprintf(response, "%d,0", s->BDR);
        push_response(s, response);
        return;
    }

    // ---- Output format ----
    if (strcmp(cmd, "COF?;") == 0)
    {
        sprintf(response, "%d", s->COF);
        push_response(s, response);
        return;
    }
    if (strncmp(cmd, "COF ", 4) == 0)
    {
        s->COF = atoi(cmd + 4);
        // COF set commands echo "0" on success
        push_response(s, "0");
        return;
    }

    // ---- Checksum ----
    if (strcmp(cmd, "CSM?;") == 0)
    {
        sprintf(response, "%d", s->CSM);
        push_response(s, response);
        return;
    }
    if (strncmp(cmd, "CSM ", 4) == 0)
    {
        s->CSM = atoi(cmd + 4);
        push_response(s, "0");
        return;
    }

    // ---- Terminating resistor ----
    if (strncmp(cmd, "STR ", 4) == 0)
    {
        s->STR = atoi(cmd + 4);
        push_response(s, "0");
        return;
    }
    if (strcmp(cmd, "STR?;") == 0)
    {
        sprintf(response, "%d", s->STR);
        push_response(s, response);
        return;
    }

    // ---- Password (always accept) ----
    if (strncmp(cmd, "SPW", 3) == 0)
    {
        push_response(s, "0");
        return;
    }

    // ---- Save to NVRAM (accept) ----
    if (strcmp(cmd, "TDD1;") == 0)
    {
        push_response(s, "0");
        return;
    }

    // ---- Reset (accept) ----
    if (strcmp(cmd, "RES;") == 0)
    {
        // No response for reset
        return;
    }

    // ---- Stop continuous output ----
    if (strcmp(cmd, "STP;") == 0)
    {
        s->continuous = false;
        // STP has no response
        return;
    }

    // ---- Start continuous output ----
    if (strcmp(cmd, "MSV?0;") == 0 || strcmp(cmd, "MSV? 0;") == 0)
    {
        s->continuous = true;
        // No text response - switches to binary measurement stream
        return;
    }

    // ---- ASF (lowpass filter) ----
    if (strcmp(cmd, "ASF?;") == 0)
    {
        sprintf(response, "%02d", s->ASF);
        push_response(s, response);
        return;
    }
    if (strncmp(cmd, "ASF ", 4) == 0)
    {
        s->ASF = atoi(cmd + 4);
        push_response(s, "0");
        return;
    }

    // ---- FMD (filter mode) ----
    if (strcmp(cmd, "FMD?;") == 0)
    {
        sprintf(response, "%d", s->FMD);
        push_response(s, response);
        return;
    }
    if (strncmp(cmd, "FMD ", 4) == 0)
    {
        s->FMD = atoi(cmd + 4);
        push_response(s, "0");
        return;
    }

    // ---- ICR (internal conversion rate) ----
    if (strcmp(cmd, "ICR?;") == 0)
    {
        sprintf(response, "%02d", s->ICR);
        push_response(s, response);
        return;
    }
    if (strncmp(cmd, "ICR ", 4) == 0)
    {
        s->ICR = atoi(cmd + 4);
        push_response(s, "0");
        return;
    }

    // ---- CWT (calibration weight) ----
    if (strcmp(cmd, "CWT?;") == 0)
    {
        sprintf(response, "%d,%d", s->CWT, s->CWT);
        push_response(s, response);
        return;
    }
    if (strncmp(cmd, "CWT ", 4) == 0)
    {
        s->CWT = atoi(cmd + 4);
        push_response(s, "0");
        return;
    }

    // ---- LDW (dead load weight) ----
    if (strcmp(cmd, "LDW?;") == 0)
    {
        sprintf(response, "%07d", s->LDW);
        push_response(s, response);
        return;
    }
    if (strncmp(cmd, "LDW ", 4) == 0)
    {
        s->LDW = atoi(cmd + 4);
        push_response(s, "0");
        return;
    }

    // ---- LWT (live weight) ----
    if (strcmp(cmd, "LWT?;") == 0)
    {
        sprintf(response, "%07d", s->LWT);
        push_response(s, response);
        return;
    }
    if (strncmp(cmd, "LWT ", 4) == 0)
    {
        s->LWT = atoi(cmd + 4);
        push_response(s, "0");
        return;
    }

    // ---- NOV (nominal value) ----
    if (strcmp(cmd, "NOV?;") == 0)
    {
        sprintf(response, "%07d", s->NOV);
        push_response(s, response);
        return;
    }
    if (strncmp(cmd, "NOV ", 4) == 0)
    {
        s->NOV = atoi(cmd + 4);
        push_response(s, "0");
        return;
    }

    // ---- RSN (resolution) ----
    if (strcmp(cmd, "RSN?;") == 0)
    {
        sprintf(response, "%03d", s->RSN);
        push_response(s, response);
        return;
    }
    if (strncmp(cmd, "RSN ", 4) == 0)
    {
        s->RSN = atoi(cmd + 4);
        push_response(s, "0");
        return;
    }

    // ---- MTD (motion detection) ----
    if (strcmp(cmd, "MTD?;") == 0)
    {
        sprintf(response, "%02d", s->MTD);
        push_response(s, response);
        return;
    }
    if (strncmp(cmd, "MTD ", 4) == 0)
    {
        s->MTD = atoi(cmd + 4);
        push_response(s, "0");
        return;
    }

    // ---- LIC (linearization coefficients) ----
    if (strcmp(cmd, "LIC?;") == 0)
    {
        sprintf(response, "%07d,%07d,%07d,%07d", s->LIC0, s->LIC1, s->LIC2, s->LIC3);
        push_response(s, response);
        return;
    }
    // Individual LIC set commands: "LIC0, value;", "LIC1, value;", etc.
    if (strncmp(cmd, "LIC0,", 5) == 0)
    {
        s->LIC0 = atoi(cmd + 5);
        push_response(s, "0");
        return;
    }
    if (strncmp(cmd, "LIC1,", 5) == 0)
    {
        s->LIC1 = atoi(cmd + 5);
        push_response(s, "0");
        return;
    }
    if (strncmp(cmd, "LIC2,", 5) == 0)
    {
        s->LIC2 = atoi(cmd + 5);
        push_response(s, "0");
        return;
    }
    if (strncmp(cmd, "LIC3,", 5) == 0)
    {
        s->LIC3 = atoi(cmd + 5);
        push_response(s, "0");
        return;
    }

    // ---- ZTR (zero tracking) ----
    if (strcmp(cmd, "ZTR?;") == 0)
    {
        sprintf(response, "%d", s->ZTR);
        push_response(s, response);
        return;
    }
    if (strncmp(cmd, "ZTR ", 4) == 0)
    {
        s->ZTR = atoi(cmd + 4);
        push_response(s, "0");
        return;
    }

    // ---- ZSE (zero setting) ----
    if (strcmp(cmd, "ZSE?;") == 0)
    {
        sprintf(response, "%02d", s->ZSE);
        push_response(s, response);
        return;
    }
    if (strncmp(cmd, "ZSE ", 4) == 0)
    {
        s->ZSE = atoi(cmd + 4);
        push_response(s, "0");
        return;
    }

    // ---- TRC (trigger settings) ----
    if (strcmp(cmd, "TRC?;") == 0)
    {
        sprintf(response, "%.1d,%.1d, %.7d,%.2d,%.2d",
            s->TRC1, s->TRC2, s->TRC3, s->TRC4, s->TRC5);
        push_response(s, response);
        return;
    }
    if (strncmp(cmd, "TRC ", 4) == 0)
    {
        // TRC set: "TRC val1,val2,val3,val4,val5;"
        char tmp[128];
        strncpy(tmp, cmd + 4, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        // Remove trailing ';'
        char* semi = strchr(tmp, ';');
        if (semi) *semi = '\0';

        char* p = strtok(tmp, ",");
        if (p) { s->TRC1 = atoi(p); p = strtok(NULL, ","); }
        if (p) { s->TRC2 = atoi(p); p = strtok(NULL, ","); }
        if (p) { s->TRC3 = atoi(p); p = strtok(NULL, ","); }
        if (p) { s->TRC4 = atoi(p); p = strtok(NULL, ","); }
        if (p) { s->TRC5 = atoi(p); }
        push_response(s, "0");
        return;
    }

    // ---- MAV (single measurement - not continuous) ----
    if (strcmp(cmd, "MAV?;") == 0)
    {
        // Return a single measurement as ASCII
        double wt = hbm_sim_weights[s->weight_idx] * CNTS;
        s->weight_idx = (s->weight_idx + 1) % HBM_SIM_NUM_WEIGHTS;
        sprintf(response, "%d", (int)wt);
        push_response(s, response);
        return;
    }

    // ---- MSV with non-zero parameter (single measurement) ----
    if (strncmp(cmd, "MSV", 3) == 0 && strcmp(cmd, "MSV?0;") != 0 && strcmp(cmd, "MSV? 0;") != 0)
    {
        double wt = hbm_sim_weights[s->weight_idx] * CNTS;
        s->weight_idx = (s->weight_idx + 1) % HBM_SIM_NUM_WEIGHTS;
        sprintf(response, "%d", (int)wt);
        push_response(s, response);
        return;
    }

    // Unknown command - echo "0" (success)
    //RtPrintf("HBM Sim[%d] unknown cmd: %s\n", s->port_num, cmd);
    push_response(s, "0");
}

//--------------------------------------------------------
// Background thread: generates continuous measurements
//
// When s->continuous is true, pushes 4-byte binary packets
// into the FIFO at approximately the real FIT7 rate.
//--------------------------------------------------------
static void* HBMSimMeasThread(void* arg)
{
    HBMSimState* s = (HBMSimState*)arg;
    struct timespec ts;
    double wt;
    int adc_counts;

    RtPrintf("HBM FIT7 Sim[%d] measurement thread started\n", s->port_num);

    while (s->running)
    {
        if (s->continuous)
        {
            bool bird = g_hbm_bird_present;

            if (bird)
            {
                // Advance weight on each new shackle using sequence counter
                // (rising-edge on bool was too fast for 600Hz thread to catch)
                int seq = g_hbm_shackle_seq;
                if (seq != s->prev_seq)
                {
                    s->weight_idx = (s->weight_idx + 1) % HBM_SIM_NUM_WEIGHTS;
                    s->prev_seq = seq;
                }

                wt = hbm_sim_weights[s->weight_idx] * CNTS;
                adc_counts = (int)wt;
            }
            else
            {
                adc_counts = 500;
            }

            // Add small random noise (±10 ADC counts) to simulate real
            // load cell behavior. Without noise, the controller's stuck
            // load cell detector fires (50 consecutive identical readings).
            adc_counts += (rand() % 21) - 10;

            // Push 4-byte packet into FIFO
            pthread_mutex_lock(&s->fifo_mutex);
            push_measurement(s, adc_counts);
            pthread_mutex_unlock(&s->fifo_mutex);

            // Sleep at measurement rate (~600 Hz = ~1667 us)
            ts.tv_sec = 0;
            ts.tv_nsec = HBM_SIM_MEAS_INTERVAL_US * 1000;
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

    RtPrintf("HBM FIT7 Sim[%d] measurement thread stopped\n", s->port_num);
    return NULL;
}

//--------------------------------------------------------
// HBMSim_Init: Initialize a load cell simulator instance
//--------------------------------------------------------
void HBMSim_Init(int port_num)
{
    int idx = PortToIndex(port_num);
    if (idx < 0) return;

    HBMSimState* s = &g_hbmSim[idx];

    if (s->initialized)
        return;

    memset(s, 0, sizeof(HBMSimState));
    pthread_mutex_init(&s->fifo_mutex, NULL);

    s->port_num = port_num;
    s->fifo_head = 0;
    s->fifo_tail = 0;
    s->cmd_idx = 0;

    // Default settings (matching overhead.cpp DefaultDigLCSet)
    s->ASF  = 5;
    s->FMD  = 1;
    s->ICR  = 4;        // Note: init_adc checks ICR?; expects 04
    s->CWT  = 500000;
    s->LDW  = 0;
    s->LWT  = 500000;
    s->NOV  = 0;
    s->RSN  = 1;
    s->MTD  = 0;
    s->LIC0 = 0;
    s->LIC1 = 500000;
    s->LIC2 = 0;
    s->LIC3 = 0;
    s->ZTR  = 0;
    s->ZSE  = 0;
    s->TRC1 = 0;
    s->TRC2 = 0;
    s->TRC3 = 0;
    s->TRC4 = 0;
    s->TRC5 = 0;

    // Protocol settings
    s->BDR = 38400;
    s->COF = 8;         // 4-byte binary, MSB→LSB, checksum
    s->CSM = 1;         // checksum enabled
    s->STR = 0;

    s->continuous = false;
    s->weight_idx = 0;

    // Start measurement thread
    s->running = true;
    if (pthread_create(&s->meas_thread, NULL, HBMSimMeasThread, s) != 0)
    {
        RtPrintf("HBM FIT7 Sim[%d] thread creation failed\n", port_num);
        s->running = false;
    }

    s->initialized = true;

    RtPrintf("HBM FIT7 Load Cell Simulator initialized on COM%d\n", port_num + 1);
    RtPrintf("  Default settings: ASF=%d FMD=%d ICR=%d COF=%d CSM=%d\n",
        s->ASF, s->FMD, s->ICR, s->COF, s->CSM);
}

//--------------------------------------------------------
// HBMSim_Write: Receive data from controller (commands)
//
// Buffers incoming bytes until ';' is found, then
// processes the complete command.
//--------------------------------------------------------
int HBMSim_Write(int port_num, const unsigned char* data, int len)
{
    int idx = PortToIndex(port_num);
    if (idx < 0) return 0;

    HBMSimState* s = &g_hbmSim[idx];
    int i;

    for (i = 0; i < len; i++)
    {
        char ch = (char)data[i];

        // Accumulate command bytes
        if (s->cmd_idx < HBM_SIM_CMD_MAX - 1)
        {
            s->cmd_buf[s->cmd_idx++] = ch;
        }

        // ';' marks end of command
        if (ch == ';')
        {
            s->cmd_buf[s->cmd_idx] = '\0';

            // Process the command (puts response in FIFO)
            pthread_mutex_lock(&s->fifo_mutex);
            process_command(s, s->cmd_buf);
            pthread_mutex_unlock(&s->fifo_mutex);

            // Reset command buffer
            s->cmd_idx = 0;
        }
    }

    return len;
}

//--------------------------------------------------------
// HBMSim_Read: Read data from load cell (responses/measurements)
//
// Returns data from the FIFO ring buffer.
//--------------------------------------------------------
int HBMSim_Read(int port_num, unsigned char* buffer, int max_len)
{
    int idx = PortToIndex(port_num);
    if (idx < 0) return 0;

    HBMSimState* s = &g_hbmSim[idx];
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
// HBMSim_Available: Get number of bytes ready to read
//--------------------------------------------------------
int HBMSim_Available(int port_num)
{
    int idx = PortToIndex(port_num);
    if (idx < 0) return 0;

    HBMSimState* s = &g_hbmSim[idx];
    int count;

    pthread_mutex_lock(&s->fifo_mutex);
    count = fifo_count(s);
    pthread_mutex_unlock(&s->fifo_mutex);

    return count;
}

//--------------------------------------------------------
// HBMSim_Cleanup: Stop and clean up a simulator instance
//--------------------------------------------------------
void HBMSim_Cleanup(int port_num)
{
    int idx = PortToIndex(port_num);
    if (idx < 0) return;

    HBMSimState* s = &g_hbmSim[idx];

    if (!s->initialized)
        return;

    s->running = false;
    s->continuous = false;
    pthread_join(s->meas_thread, NULL);
    pthread_mutex_destroy(&s->fifo_mutex);

    s->initialized = false;

    RtPrintf("HBM FIT7 Sim[%d] cleaned up\n", port_num);
}

//--------------------------------------------------------
// HBMSim_CleanupAll: Clean up all instances
//--------------------------------------------------------
void HBMSim_CleanupAll(void)
{
    int i;
    for (i = 0; i < HBM_SIM_MAX_LC; i++)
    {
        if (g_hbmSim[i].initialized)
            HBMSim_Cleanup(i);
    }
}
