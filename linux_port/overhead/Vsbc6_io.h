#pragma once

#include "types.h"

//////////////////////////////////////////////////////////////
// VSBC6 register definitions
//////////////////////////////////////////////////////////////

#define VSBC6_IO_BASE_ADDR      ((PUCHAR)0xE6)
#define VSBC6_IO_BYTES          ((ULONG)0x2)

#define VSBC6_CFG_DCAS          ((PUCHAR)0xE2)
#define VSBC6_DCAS_BYTES        ((ULONG)0x1)

// The define below configures the VSBC6 I/O

// LO (0x1) inputs, HI (0x2) outputs
#ifdef VSBC6_LOWIN_HIOUT

    #define VSBC6_IO_SEL            ((UCHAR)0x02)
    #define VSBC6_INPUT_PORT_LO     (VSBC6_IO_BASE_ADDR + 0)
    #define VSBC6_OUTPUT_PORT_HI    (VSBC6_IO_BASE_ADDR + 1)

#endif

// LO (0x1) outputs, HI (0x2) outputs
#ifdef VSBC6_LOWOUT_HIOUT

    #define VSBC6_IO_SEL            ((UCHAR)0x03)
    #define VSBC6_OUTPUT_PORT_LO    (VSBC6_IO_BASE_ADDR + 0)
    #define VSBC6_OUTPUT_PORT_HI    (VSBC6_IO_BASE_ADDR + 1)

#endif

//----- Used in SetOutput routine to simplify swapping bytes for VSB6 IO

#define VSBC6_LO 0
#define VSBC6_HI 1

#define VSBC6_SWAP_AND_WRITE(byte,port) \
{ \
    BYTE swapped_bits; \
\
     for (int i = 0; i <= 7; i++)\
     {\
        if BITSET(byte,i)\
        {\
           SETBIT(swapped_bits, 7 - i);\
        }\
        else\
        {\
           CLRBIT(swapped_bits, 7 - i);\
        }\
    }\
\
 if (port == VSBC6_LO)\
    RtWritePortUchar(VSBC6_OUTPUT_PORT_LO, UCHAR(~swapped_bits));\
 else\
    RtWritePortUchar(VSBC6_OUTPUT_PORT_HI, UCHAR(~swapped_bits));\
}

/////////////////////////////////////////////////////////////////////////////////////////
//                    End Definitions

class io_vsbc6 : public io
{
public:
	io_vsbc6();
	virtual ~io_vsbc6();

    bool initialize();
    void readInputs(void);

};
