// 3724_io_4.cpp: Implementation of the io_3724_4 class (VL-EPM-19).
//
// Restored from RTX source.
//
//////////////////////////////////////////////////////////////////////

#include "types.h"
#include "3724_io_4.h"

#undef  _FILE_
#define _FILE_  "3724_io_4.cpp"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

io_3724_4::io_3724_4()
{
}

io_3724_4::~io_3724_4()
{
}

/************************************************************************
** Initialize the I/O board
*************************************************************************/

bool io_3724_4::initialize()
{
    // Initialize the PCM_3724 ports for VL-EPM-19
    if (!RtEnablePortIo(PCM_3724_4_BASE_ADDR, PCM_3724_4_BASE_BYTES))
        return false;

    // Setup Input/Output Port configuration
    unsigned char uCfg_Reg_0 = PCM_3724_4_OUTPUT_SEL;
    unsigned char uCfg_Reg_1 = PCM_3724_4_OUTPUT_SEL;
    unsigned char uCfg_Direction = PCM_3724_4_OUTPUT_DIR;

    #ifdef VSBC6_LOWIN_HIOUT
        uCfg_Reg_0 |= 0x10;
        uCfg_Direction |= 0x3B;
    #endif

    RtWritePortUchar(PCM_3724_4_GATE_CNTRL, PCM_3724_4_DISABLE);
    RtWritePortUchar(PCM_3724_4_CFG_REG_0, uCfg_Reg_0);
    RtWritePortUchar(PCM_3724_4_CFG_REG_1, uCfg_Reg_1);
    RtWritePortUchar(PCM_3724_4_BUFFER_DIR, uCfg_Direction);
    RtWritePortUchar(PCM_3724_4_GATE_CNTRL, PCM_3724_4_ENABLE);

    // Clear outputs
    #ifdef VSBC6_LOWOUT_HIOUT
        RtWritePortUchar(PCM_3724_4_PORT_A0, (unsigned char)0xFF);
    #endif
    RtWritePortUchar(PCM_3724_4_PORT_B0, (unsigned char)0xFF);

    return true;
}

//--------------------------------------------------------
// readInputs: Routine to Read inputs
//--------------------------------------------------------

void io_3724_4::readInputs(void)
{
    #ifdef VSBC6_LOWIN_HIOUT
        app->switch_in[SWITCH_1] = (unsigned char)(RtReadPortUchar(PCM_3724_4_PORT_A0));
    #endif
}

//--------------------------------------------------------
// writeOutput: Routine to write output registers
//--------------------------------------------------------

void io_3724_4::writeOutput(int byte)
{
    switch (byte)
    {
    case OUTPUT_3:
        RtWritePortUchar(PCM_3724_4_PORT_A0, (unsigned char)(~app->output_byte[byte]));
        break;
    case OUTPUT_4:
        RtWritePortUchar(PCM_3724_4_PORT_B0, (unsigned char)(~app->output_byte[byte]));
        break;
    default:
        break;
    }
}
