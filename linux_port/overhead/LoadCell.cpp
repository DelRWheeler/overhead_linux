// LoadCell.cpp: Linux stub implementation of the LoadCell base class.
//
//////////////////////////////////////////////////////////////////////

#include "types.h"

#undef  _FILE_
#define _FILE_ "LoadCell.cpp"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

LoadCell::LoadCell()
{
}

LoadCell::~LoadCell()
{
}

int LoadCell::init_adc(int mode, int num_reads)
{
	(void)mode;
	(void)num_reads;
    RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
    return(false);
}

void LoadCell::set_adc_num_reads(int channel, int num_adc_reads)
{
	(void)channel;
	(void)num_adc_reads;
    RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
}

int LoadCell::read_adc_num_reads(int channel)
{
	(void)channel;
    RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
    return(0);
}

void LoadCell::KillTime(int iTime)
{
	(void)iTime;
    // Empty stub - no busy-wait needed on Linux
}

void LoadCell::set_adc_mode(int channel, int mode)
{
	(void)channel;
	(void)mode;
    RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
}

int LoadCell::read_adc_mode(int channel)
{
	(void)channel;
    RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
    return(0);
}

void LoadCell::read_adc_version(void)
{
    RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
}

void LoadCell::set_adc_offset(int channel, int offset)
{
	(void)channel;
	(void)offset;
    RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
}

void LoadCell::read_adc_pn()
{
    RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
}

__int64 LoadCell::read_load_cell(int channel)
{
	(void)channel;
    RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
    return(0);
}

void LoadCell::write_adc(int channel, unsigned char outbyte)
{
	(void)channel;
	(void)outbyte;
    RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
}

char LoadCell::read_adc(int channel)
{
	(void)channel;
    RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
    return(0);
}
