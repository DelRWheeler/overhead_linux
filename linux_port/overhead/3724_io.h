#pragma once

//////////////////////////////////////////////////////////////////////////////////////////
// IO ranges definitions

//	Board 1

#define PCM_3724_1_BASE_ADDR ((PUCHAR)0x300)
#define PCM_3724_1_BASE_BYTES ((ULONG)0x10)

// PCM_3724 Board 1register definitions
#define PCM_3724_1_PORT_A0        (PCM_3724_1_BASE_ADDR + 0)
#define PCM_3724_1_PORT_B0        (PCM_3724_1_BASE_ADDR + 1)
#define PCM_3724_1_PORT_C0        (PCM_3724_1_BASE_ADDR + 2)
#define PCM_3724_1_CFG_REG_0      (PCM_3724_1_BASE_ADDR + 3)
#define PCM_3724_1_PORT_A1        (PCM_3724_1_BASE_ADDR + 4)
#define PCM_3724_1_PORT_B1        (PCM_3724_1_BASE_ADDR + 5)
#define PCM_3724_1_PORT_C1        (PCM_3724_1_BASE_ADDR + 6)
#define PCM_3724_1_CFG_REG_1      (PCM_3724_1_BASE_ADDR + 7)
#define PCM_3724_1_BUFFER_DIR     (PCM_3724_1_BASE_ADDR + 8)
#define PCM_3724_1_GATE_CNTRL     (PCM_3724_1_BASE_ADDR + 9)

// Port Parameters
#define PCM_3724_1_INPUT_SEL        ((UCHAR)0x9B)    // CFG REG control value - All inputs
#define PCM_3724_1_OUTPUT_SEL       ((UCHAR)0x80)    // CFG REG control value - All outputs
#define PCM_3724_1_OUTPUT_DIR       ((UCHAR)0x38)    // BUFFER DIR Ports A0, B0, C0 inputs, Ports A1, B1, C1 outputs
#define PCM_3724_1_DISABLE          ((UCHAR)0x0)     // Disable all gates
#define PCM_3724_1_ENABLE           ((UCHAR)0xFF)    // Enable all gates

/////////////////////////////////////////////////////////////////////////////////////////
//                    End Definitions

class io_3724 : public io
{
public:
	io_3724();
	virtual ~io_3724();

    bool initialize();
    void readInputs(void);

};
