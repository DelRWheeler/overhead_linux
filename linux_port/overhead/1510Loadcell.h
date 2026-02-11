#pragma once

#include "types.h"

#define SET_DAC_REG         0xB0
#define READ_LOAD_CELL      0x88
#define SET_NUM_ADC_READS   0x70
#define SET_ADC_MODE        0x71
#define READ_ADC_PN         0x72
#define READ_ADC_VERSION    0x73
#define READ_ADC_MODE       0x75
#define READ_ADC_NUM_READS  0x76

#define MAX_ADC_READS_1510  15
#define NUM_CH_1510         2
#define SET                 1
#define RESET               0

#define INDIVID             0
#define AVG                 1

#define CH_A                0
#define CH_B                1

#define Port1 (PUCHAR)0x110

class LoadCell_1510 : public LoadCell
{
private:

    unsigned char left_adc_pn[8];
    unsigned char left_adc_version[8];
    int           adc_mode[NUM_CH_1510];
//    int           num_adc_reads[NUM_CH_1510];
    int           adc_offset[NUM_CH_1510];
    char          lc_err_buf[MAXERRMBUFSIZE];
    bool          error_sent[NUM_CH_1510];

    void    KillTime(int iTime);

public:
    LoadCell_1510();
    virtual ~LoadCell_1510();

    int     num_adc_reads[NUM_CH_1510];
    int     read_adc_num_reads(int channel);
    int     init_adc(int mode, int num_reads);
    void    write_adc(int channel,unsigned char outbyte);
    __int64 read_load_cell(int channel);
    void    set_adc_offset(int channel,int offset);
    void    set_adc_num_reads(int channel, int reads);
    void    set_adc_mode(int channel, int mode);
    void    read_adc_version(void);
    void    read_adc_pn(void);
    int     read_adc_mode(int channel);
    char    read_adc(int channel);

    int NumCH;
};

#define LC_ERR_CLR \
if (error_sent[adc_mode[channel]] == true) \
{ \
    error_sent[adc_mode[channel]] = false; \
    RtPrintf("Loadcell reads %d mode %d OK\n", \
             num_adc_reads[channel], adc_mode[channel]); \
    sprintf(lc_err_buf,"Loadcell reads %d mode %d OK\n", \
             num_adc_reads[channel], adc_mode[channel]); \
    app->GenError(informational, lc_err_buf); \
}

#define LC_ERR_SET \
if (error_sent[adc_mode[channel]] == false) \
{ \
    error_sent[adc_mode[channel]] = true; \
    RtPrintf("Error adc reads %d mode %d file %s, line %d \n", \
              num_adc_reads[channel], \
              adc_mode[channel], _FILE_, __LINE__); \
    sprintf(lc_err_buf,"Error load cell reads %d mode %d file %s, line %d \n", \
              num_adc_reads[channel], \
              adc_mode[channel], _FILE_, __LINE__); \
    app->GenError(warning, lc_err_buf); \
} \
return(0);
