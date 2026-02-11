// 3724_io_2.cpp: Implementation of the io_3724_2 class.
//
// Restored from RTX source.
//
//////////////////////////////////////////////////////////////////////

#include "types.h"

#undef  _FILE_
#define _FILE_  "3724_io_2.cpp"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

io_3724_2::io_3724_2()
{
}

io_3724_2::~io_3724_2()
{
}

/************************************************************************
** Initialize the I/O board
*************************************************************************/

bool io_3724_2::initialize()
{
//----- Initialize the PCM_3724 ports

    if(!RtEnablePortIo(PCM_3724_2_BASE_ADDR, PCM_3724_2_BASE_BYTES))
        return false;

//----- Setup the Input/Output Port configuration
	unsigned char uCfg_Reg_0 = 0x80;		//	Set initial value to all outputs
	unsigned char uCfg_Reg_1 = 0x80;		//	Set initial value to all outputs
	unsigned char uCfg_Direction = 0x00;	//	Set initial value to all inputs

	//	Modify the register configuration based on number of input registers required
	unsigned char uInput_Mask[] = {0x10, 0x12, 0x1B};
	unsigned char uDirection_Mask[] = {0x3B, 0x39, 0x38};

	//	If InputRegisters are less than 1 or more than 3 we have a configuration problem
    if((app->Second_3724_InputRegisters < 1) ||
	   (app->Second_3724_InputRegisters > 3))
        return false;

	//	Or proper mask values for input registers required
	uCfg_Reg_0 |= uInput_Mask[app->Second_3724_InputRegisters - 1];

	//	Or proper mask values for direction of input registers needed
	uCfg_Direction |= uDirection_Mask[app->Second_3724_InputRegisters - 1];

    RtWritePortUchar(PCM_3724_2_GATE_CNTRL, PCM_3724_2_DISABLE);    // Disable all gates
    RtWritePortUchar(PCM_3724_2_CFG_REG_0,  uCfg_Reg_0);			// Set port 0 to calculated number of inputs
    RtWritePortUchar(PCM_3724_2_CFG_REG_1,  uCfg_Reg_1);			// Set port 10 to all outputs
    RtWritePortUchar(PCM_3724_2_BUFFER_DIR, uCfg_Direction);		// Set direction of ports
    RtWritePortUchar(PCM_3724_2_GATE_CNTRL, PCM_3724_2_ENABLE);     // Re-enable all gates

    // Clear outputs
	switch (app->Second_3724_InputRegisters)
	{
	case 1:			// If only 1 clear B0, C0, A1, B1 & C1
		RtWritePortUchar(PCM_3724_2_PORT_B0, (unsigned char) 0xff);
	case 2:			// If 2 clear only C0, A1, B1 & C1
		RtWritePortUchar(PCM_3724_2_PORT_C0, (unsigned char) 0xff);
	case 3:			// If 3 clear only A1, B1 & C1
		RtWritePortUchar(PCM_3724_2_PORT_A1, (unsigned char) 0xff);
		RtWritePortUchar(PCM_3724_2_PORT_B1, (unsigned char) 0xff);
		RtWritePortUchar(PCM_3724_2_PORT_C1, (unsigned char) 0xff);
		break;
	default:
		break;
	}

   return true;
}

//--------------------------------------------------------
// readInputs: Routine to Read inputs
//--------------------------------------------------------

void io_3724_2::readInputs(void)
{
    UCHAR temp[3];
    int i;

    // Read the OPTO 22 Input ports for inputs
	switch(app->Second_3724_InputRegisters)
	{
	case 3:
		temp[2] = (unsigned char)(~RtReadPortUchar(PCM_3724_2_PORT_C0));
	case 2:
		temp[1] = (unsigned char)(~RtReadPortUchar(PCM_3724_2_PORT_B0));
	case 1:
		temp[0] = (unsigned char)(~RtReadPortUchar(PCM_3724_2_PORT_A0));
	}

	// Convert bits in registers to an array of bools
	for (i = 0; i < app->Second_3724_No_of_Inputs; i++)
	{
		app->Second_3724_Input[i] = temp[i/8] % 2 ? true : false;
		temp[i/8] >>= 1;
	}
}

//--------------------------------------------------------
// writeOutputs: Routine to convert array of bools to
//				 registers and write to card
//--------------------------------------------------------

void io_3724_2::writeOutputs(void)
{
    UCHAR temp[3];
	int i;

	if (app->Second_3724_WriteOutput)
	{
		app->Second_3724_WriteOutput = false;

		// Clear temps to zero
		for (i = 0; i < 3; i++)
			temp[i] = 0;

		// Convert array of bools to bits in registers
		for (i = app->Second_3724_No_of_Outputs - 1; i >= 0; i--)
		{
			temp[i/8] <<= 1;
			if (app->Second_3724_Output[i])
				temp[i/8]++;
		}

		// Write the registers to the OPTO 22 Output ports
		switch(app->Second_3724_InputRegisters)
		{
		case 3:
			RtWritePortUchar(PCM_3724_2_PORT_A1, (unsigned char)(~temp[0]));
			RtWritePortUchar(PCM_3724_2_PORT_B1, (unsigned char)(~temp[1]));
			RtWritePortUchar(PCM_3724_2_PORT_C1, (unsigned char)(~temp[2]));
			break;
		case 2:
			RtWritePortUchar(PCM_3724_2_PORT_C0, (unsigned char)(~temp[0]));
			RtWritePortUchar(PCM_3724_2_PORT_A1, (unsigned char)(~temp[1]));
			RtWritePortUchar(PCM_3724_2_PORT_B1, (unsigned char)(~temp[2]));
			break;
		case 1:
			RtWritePortUchar(PCM_3724_2_PORT_B0, (unsigned char)(~temp[0]));
			RtWritePortUchar(PCM_3724_2_PORT_C0, (unsigned char)(~temp[1]));
			RtWritePortUchar(PCM_3724_2_PORT_A1, (unsigned char)(~temp[2]));
			break;
		default:
			break;
		}
	}
}
