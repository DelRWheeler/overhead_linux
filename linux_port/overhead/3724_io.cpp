// 3724_io.cpp: Implementation of the io_3724 class.
//
// Restored from RTX source. Uses RtReadPortUchar/RtWritePortUchar
// which route to either real hardware (inb/outb) or the virtual
// PCM-3724 simulator depending on build configuration.
//
//////////////////////////////////////////////////////////////////////

#include "types.h"

#undef  _FILE_
#define _FILE_  "3724_io.cpp"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

io_3724::io_3724()
{
}

io_3724::~io_3724()
{
}

/************************************************************************
** Initialize the I/O board
*************************************************************************/

bool io_3724::initialize()
{
//----- Initialize the PCM_3724 ports

    if(!RtEnablePortIo(PCM_3724_1_BASE_ADDR, PCM_3724_1_BASE_BYTES))
        return false;

    RtWritePortUchar(PCM_3724_1_GATE_CNTRL, PCM_3724_1_DISABLE);    // Disable all gates
    RtWritePortUchar(PCM_3724_1_CFG_REG_0,  PCM_3724_1_INPUT_SEL);  // Set port 0 to all inputs
    RtWritePortUchar(PCM_3724_1_CFG_REG_1,  PCM_3724_1_OUTPUT_SEL); // Set port 10 to all outputs
    RtWritePortUchar(PCM_3724_1_BUFFER_DIR, PCM_3724_1_OUTPUT_DIR); // Set BUFFER DIR
    RtWritePortUchar(PCM_3724_1_GATE_CNTRL, PCM_3724_1_ENABLE);     // Re-enable all gates

    // Clear outputs
    RtWritePortUchar(PCM_3724_1_PORT_A1, (unsigned char)(~app->output_byte[OUTPUT_0]));
    RtWritePortUchar(PCM_3724_1_PORT_B1, (unsigned char)(~app->output_byte[OUTPUT_1]));
    RtWritePortUchar(PCM_3724_1_PORT_C1, (unsigned char)(~app->output_byte[OUTPUT_2]));

    return true;
}

//--------------------------------------------------------
// readInputs: Routine to Read inputs and set Sync,Zero
//             and switch_in bits
//--------------------------------------------------------

void io_3724::readInputs(void)
{
    UCHAR atemp, btemp;
    UCHAR raw_a0, raw_b0;
    int i, dest_bit;

    // Read the OPTO 22 Input ports for sensor state checking
    raw_a0 = RtReadPortUchar(PCM_3724_1_PORT_A0);
    raw_b0 = RtReadPortUchar(PCM_3724_1_PORT_B0);
    atemp = (unsigned char)(~raw_a0);
    btemp = (unsigned char)(~raw_b0);

    // Separate into sync_in and sync_zero bytes
    // Port A0/B0 use interleaved bits:
    //   Even bits (0,2,4,6) = Sync sensors
    //   Odd bits  (1,3,5,7) = Zero sensors
    for (i = 0; i <= 7; i++)
    {
        dest_bit = i/2;
        // Odds are Zeros
         if (i & 1)
         {
            if BITSET(atemp, i)
            {
                SETBIT(app->sync_zero[ZERO_0_LO], dest_bit);
            }
            else
            {
                CLRBIT(app->sync_zero[ZERO_0_LO], dest_bit);
            }

            if BITSET(btemp, i)
            {
                SETBIT(app->sync_zero[ZERO_0_HI], dest_bit + 4);
            }
            else
            {
                CLRBIT(app->sync_zero[ZERO_0_HI], dest_bit + 4);
            }
         }
        // Evens are Syncs
        else
        {
            if BITSET(atemp, i)
            {
                SETBIT(app->sync_in[SYNC_0_LO], dest_bit);
            }
            else
            {
                CLRBIT(app->sync_in[SYNC_0_LO], dest_bit);
            }

            if BITSET(btemp, i)
            {
                SETBIT(app->sync_in[SYNC_0_HI], dest_bit + 4);
            }
            else
            {
                CLRBIT(app->sync_in[SYNC_0_HI], dest_bit + 4);
            }
        }
    }
    // switch_in Bits
    app->switch_in[SWITCH_0] = (unsigned char)(~RtReadPortUchar(PCM_3724_1_PORT_C0));
}
