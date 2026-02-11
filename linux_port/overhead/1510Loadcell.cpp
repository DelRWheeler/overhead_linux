//////////////////////////////////////////////////////////////////////
// 1510LoadCell.cpp: Linux stub implementation of the LoadCell_1510 class.
//////////////////////////////////////////////////////////////////////

#include "types.h"

#undef  _FILE_
#define _FILE_      "1510LoadCell.cpp"

PUCHAR adc_command_port[2];
PUCHAR adc_status_port[2];

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

LoadCell_1510::LoadCell_1510()
{
    NumCH = 2;
}

LoadCell_1510::~LoadCell_1510()
{
}

int LoadCell_1510::init_adc(int mode, int num_reads)
{
    (void)mode;
    (void)num_reads;

    int i;

    adc_command_port[CH_A]  = (PUCHAR)0x110;
    adc_command_port[CH_B]  = (PUCHAR)0x112;
    adc_status_port [CH_A]  = (PUCHAR)0x111;
    adc_status_port [CH_B]  = (PUCHAR)0x113;

    for(i = 0; i < NUM_CH_1510; i++)
    {
        adc_offset   [i]    = ADCOFFSET;
        num_adc_reads[i]    = num_reads;
        adc_mode     [i]    = mode;
        error_sent[i]       = false;
    }

    // Skip hardware I/O on Linux - just initialize data structures
    RtPrintf("1510 LoadCell init_adc stub (mode=%d, reads=%d)\n", mode, num_reads);

    return(TRUE);
}

void LoadCell_1510::set_adc_num_reads(int channel, int reads)
{
    if (reads < 1) {reads = 10;}
    if (reads > MAX_ADC_READS_1510) {reads = 10;}
    if ( (reads > 0) && (reads < MAX_ADC_READS_1510) )
    {
        num_adc_reads[channel] = reads;
        // Skip hardware write on Linux
    }
}

int LoadCell_1510::read_adc_num_reads(int channel)
{
    (void)channel;
    return 0;
}

void LoadCell_1510::KillTime(int iTime)
{
    (void)iTime;
    // Empty stub - no busy-wait needed on Linux
}

void LoadCell_1510::set_adc_mode(int channel, int mode)
{
    if ( (mode >= INDIVID) && (mode <= AVG) )
    {
        adc_mode[channel] = mode;
        // Skip hardware write on Linux
    }
}

int LoadCell_1510::read_adc_mode(int channel)
{
    (void)channel;
    return 0;
}

void LoadCell_1510::read_adc_version(void)
{
    // Stub - no hardware to read from on Linux
}

void LoadCell_1510::set_adc_offset(int channel, int offset)
{
    (void)channel;
    (void)offset;
    // Stub - no hardware to write to on Linux
}

void LoadCell_1510::read_adc_pn()
{
    // Stub - no hardware to read from on Linux
}

__int64 LoadCell_1510::read_load_cell(int channel)
{
    (void)channel;
    return 0;
}

void LoadCell_1510::write_adc(int channel, unsigned char outbyte)
{
    (void)channel;
    (void)outbyte;
    // Stub - no hardware to write to on Linux
}

char LoadCell_1510::read_adc(int channel)
{
    (void)channel;
    return 0;
}
