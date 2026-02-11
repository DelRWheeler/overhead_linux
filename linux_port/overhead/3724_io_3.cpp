// 3724_io_3.cpp: Implementation of the io_3724_3 class.
//
// Restored from RTX source.
//
//////////////////////////////////////////////////////////////////////

#include "types.h"

#undef  _FILE_
#define _FILE_  "3724_io_3.cpp"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

io_3724_3::io_3724_3()
{
}

io_3724_3::~io_3724_3()
{
}

/************************************************************************
** Initialize the I/O board
*************************************************************************/

bool io_3724_3::initialize()
{
//----- Initialize the PCM_3724 ports

    if(!RtEnablePortIo(PCM_3724_3_BASE_ADDR, PCM_3724_3_BASE_BYTES))
        return false;

//----- Setup the Input/Output Port configuration
	unsigned char uCfg_Reg_0 = 0x80;		//	Set initial value to all outputs
	unsigned char uCfg_Reg_1 = 0x80;		//	Set initial value to all outputs
	unsigned char uCfg_Direction = 0x3F;	//	Set initial value to all inputs

	#ifdef VSBC6_LOWIN_HIOUT
		//	Set A0 to input
		uCfg_Reg_0 |= 0x10;
		uCfg_Direction |= 0x3B;
	#endif

    RtWritePortUchar(PCM_3724_3_GATE_CNTRL, PCM_3724_2_DISABLE);    // Disable all gates
    RtWritePortUchar(PCM_3724_3_CFG_REG_0,  uCfg_Reg_0);
    RtWritePortUchar(PCM_3724_3_CFG_REG_1,  uCfg_Reg_1);
    RtWritePortUchar(PCM_3724_3_BUFFER_DIR, uCfg_Direction);
    RtWritePortUchar(PCM_3724_3_GATE_CNTRL, PCM_3724_2_ENABLE);     // Re-enable all gates

    // Clear outputs
	#ifdef VSBC6_LOWOUT_HIOUT
		RtWritePortUchar(PCM_3724_3_PORT_A0, (unsigned char) 0xff);
	#endif
	RtWritePortUchar(PCM_3724_3_PORT_A0, (unsigned char) 0xff);

	return true;
}

//--------------------------------------------------------
// readInputs: Routine to Read inputs
//--------------------------------------------------------

void io_3724_3::readInputs(void)
{
	#ifdef VSBC6_LOWIN_HIOUT
		app->switch_in[SWITCH_1] = (unsigned char) (RtReadPortUchar(PCM_3724_3_PORT_A0));
	#endif
}

//--------------------------------------------------------
// writeOutput: Routine to write output registers
//--------------------------------------------------------

void io_3724_3::writeOutput(int byte)
{
	switch(byte)
	{
	case OUTPUT_3:
		RtWritePortUchar(PCM_3724_3_PORT_A0, (unsigned char)(~app->output_byte[byte]));
		break;
	case OUTPUT_4:
		RtWritePortUchar(PCM_3724_3_PORT_B0, (unsigned char)(~app->output_byte[byte]));
		break;
	}
}
