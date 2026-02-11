#pragma once

//////////////////////////////////////////////////////////////////////////////////////////
// IO ranges definitions
#define PCM_3724_4_BASE_ADDR ((PUCHAR)0xC800) // Updated base address for VL-EPM-19
#define PCM_3724_4_BASE_BYTES ((ULONG)0x10)  // Number of I/O bytes reserved

// PCM_3724 register definitions
#define PCM_3724_4_PORT_A0        (PCM_3724_4_BASE_ADDR + 0x00) // Port A0 register
#define PCM_3724_4_PORT_B0        (PCM_3724_4_BASE_ADDR + 0x01) // Port B0 register
#define PCM_3724_4_PORT_C0        (PCM_3724_4_BASE_ADDR + 0x02) // Port C0 register
#define PCM_3724_4_CFG_REG_0      (PCM_3724_4_BASE_ADDR + 0x03) // Configuration Register 0
#define PCM_3724_4_PORT_A1        (PCM_3724_4_BASE_ADDR + 0x04) // Port A1 register
#define PCM_3724_4_PORT_B1        (PCM_3724_4_BASE_ADDR + 0x05) // Port B1 register
#define PCM_3724_4_PORT_C1        (PCM_3724_4_BASE_ADDR + 0x06) // Port C1 register
#define PCM_3724_4_CFG_REG_1      (PCM_3724_4_BASE_ADDR + 0x07) // Configuration Register 1
#define PCM_3724_4_BUFFER_DIR     (PCM_3724_4_BASE_ADDR + 0x08) // Buffer direction control
#define PCM_3724_4_GATE_CNTRL     (PCM_3724_4_BASE_ADDR + 0x09) // Gate control register

// Port Parameters
#define PCM_3724_4_INPUT_SEL        ((UCHAR)0x9B)    // CFG REG control value - All inputs
#define PCM_3724_4_OUTPUT_SEL       ((UCHAR)0x80)    // CFG REG control value - All outputs
#define PCM_3724_4_OUTPUT_DIR       ((UCHAR)0x38)    // BUFFER DIR Ports A0, B0, C0 outputs, Ports A1, B1, C1 inputs
#define PCM_3724_4_DISABLE          ((UCHAR)0x00)    // Disable all gates
#define PCM_3724_4_ENABLE           ((UCHAR)0xFF)    // Enable all gates

/////////////////////////////////////////////////////////////////////////////////////////
//                    End Definitions

class io_3724_4 : public io {
public:
    io_3724_4();
    virtual ~io_3724_4();

    bool initialize();        // Initialize the I/O board
    void readInputs(void);    // Read input ports
    void writeOutput(int byte); // Write output to ports
};
