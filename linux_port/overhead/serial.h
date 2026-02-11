#pragma once
//////////////////////////////////////////////////////////////////////
// serial.h - Linux port of the RTX serial driver
//
// On RTX, serial was done via direct UART register access and ISRs.
// On Linux, serial uses /dev/ttyS* with termios (to be implemented).
// For now, provides the same interface with stub implementations.
//////////////////////////////////////////////////////////////////////

#include <termios.h>

//------------------------------------------------------------
// Data definitions - kept from original for compatibility
//------------------------------------------------------------

#define  RING_BUFFER_SIZE              0x2000

#define  COM1                          0
#define  COM2                          1
#define  COM3                          2
#define  COM4                          3

// Port addresses (x86 specific - not used on Linux, kept for struct compat)
#define  COM1_BADDR                    0x3f8
#define  COM2_BADDR                    0x2f8
#define  COM3_BADDR                    0x3e8
#define  COM4_BADDR                    0x2e8

#define  COM_IO_RANGE                  8
#define  COM_MAX_PORTS                 4

// IRQs (x86 specific - not used on Linux)
#define  COM1_IRQ                      0x4
#define  COM2_IRQ                      0x10
#define  COM3_IRQ                      0x3
#define  COM4_IRQ                      0x11

#define  RS232                         0x00
#define  RS422                         0x01

//-------------------------------------------------
//  define COM port states
//-------------------------------------------------

#define COM_STATE_IDLE                  0
#define COM_STATE_OPEN                  1

//-------------------------------------------------
// Register offsets (kept for compatibility, not used on Linux)
//-------------------------------------------------

#define  UART_XMIT_BUFFER             0
#define  UART_RCVR_BUFFER             0
#define  UART_DIV_LATCH_LOW           0
#define  UART_IER                     1
#define  UART_DIV_LATCH_HIGH          1
#define  UART_IIR                     2
#define  UART_FCR                     2
#define  UART_LCR                     3
#define  UART_MCR                     4
#define  UART_LSR                     5
#define  UART_MSR                     6
#define  UART_SCRATCH                 7
#define  UART_ADDRESS_SPACE           8

//---------------------------------------------------
// UART interrupt types
//---------------------------------------------------

#define  IIR_MODEM_STATUS                 0x00
#define  IIR_NO_INT_PENDING               0x01
#define  IIR_XMIT_HOLDING_EMPTY           0x02
#define  IIR_RECEIVED_DATA_READY          0x04
#define  IIR_RECEIVER_LINE_STATUS         0x06
#define  IIR_RECEIVE_DATA_TIMEOUT         0x0c
#define  IIR_MASK                         0xff

// These don't exist on Linux - stub them
#define  DISABLE_INTERRUPTS
#define  ENABLE_INTERRUPTS

//-----------------------------------------------------
// LSR bit masks
//-----------------------------------------------------

#define LSR_DATA_READY                    0x01
#define LSR_OVERRUN_ERROR                 0x02
#define LSR_PARITY_ERROR                  0x04
#define LSR_FRAMING_ERROR                 0x08
#define LSR_BREAK                         0x10
#define LSR_EMPTY_XMIT_HOLDING_REG        0x20
#define LSR_TRANSMITTER_EMPTY             0x40
#define LSR_FIFO_ERROR                    0x80

//-----------------------------------------------------
// LCR bit masks
//-----------------------------------------------------

#define  LCR_PARITY_NONE               0x00
#define  LCR_PARITY_ODD                0x08
#define  LCR_PARITY_EVEN               0x18
#define  LCR_PARITY_HIGH               0x28
#define  LCR_PARITY_LOW                0x38
#define  LCR_STOPBITS_1                0x00
#define  LCR_STOPBITS_2                0x04
#define  LCR_WORDSIZE_5                0x00
#define  LCR_WORDSIZE_6                0x01
#define  LCR_WORDSIZE_7                0x02
#define  LCR_WORDSIZE_8                0x03
#define  LCR_DIVISOR_LATCH             0x80

//--------------------------------------------------------
// IER bit masks
//--------------------------------------------------------

#define  IER_RECEIVE_DATA              0x01
#define  IER_TRANSMIT_DATA             0x02
#define  IER_LINE_STATUS               0x04
#define  IER_MODEM_STATUS              0x08

//-------------------------------------------------------
// FCR bit masks
//-------------------------------------------------------

#define FCR_TRIGGER_1                     0x07
#define FCR_TRIGGER_4                     0x47
#define FCR_TRIGGER_8                     0x87
#define FCR_TRIGGER_14                    0xC7
#define FCR_FIFO_DISABLED                 0x00

//-------------------------------------------------------
//  MCR bit mask
//-------------------------------------------------------
#define MCR_DTR                             0x01
#define MCR_RTS                             0x02
#define MCR_OUT1                            0x04
#define MCR_OUT2                            0x08
#define MCR_LOOPBACK                        0x10

//--------------------------------------------------------
// UART defaults
//--------------------------------------------------------

#define  DEFAULT_BAUD_RATE                COM_BAUD_RATE_9600
#define  DEFAULT_PARITY                   LCR_PARITY_NONE
#define  DEFAULT_WORDSIZE                 LCR_WORDSIZE_8
#define  DEFAULT_STOP_BITS                LCR_STOPBITS_1
#define  DEFAULT_FIFO_MASK                FCR_TRIGGER_14
#define  DEFAULT_FIFO_SIZE                14

//-----------------------------------------------------
// COM parameter definitions
//-----------------------------------------------------

#define  COM_PARITY_NONE               0x00
#define  COM_PARITY_ODD                0x08
#define  COM_PARITY_EVEN               0x18
#define  COM_PARITY_HIGH               0x28
#define  COM_PARITY_LOW                0x38

#define  COM_FIFO_DISABLED             0
#define  COM_FIFO_1                    1
#define  COM_FIFO_4                    2
#define  COM_FIFO_8                    3
#define  COM_FIFO_14                   4

#define  COM_STOPBITS_1                0x00
#define  COM_STOPBITS_2                0x04

#define  COM_WORDSIZE_5                0x00
#define  COM_WORDSIZE_6                0x01
#define  COM_WORDSIZE_7                0x02
#define  COM_WORDSIZE_8                0x03

#define  COM_BAUD_RATE_2400            0
#define  COM_BAUD_RATE_4800            1
#define  COM_BAUD_RATE_9600            2
#define  COM_BAUD_RATE_19200           3
#define  COM_BAUD_RATE_38400           4
#define  COM_BAUD_RATE_57600           5
#define  COM_BAUD_RATE_115200          6

#define  COM_FLOW_CONTROL_NONE         0
#define  COM_FLOW_CONTROL_HARDWARE     1

//-------------------------------------------
// ComStats function codes
//-------------------------------------------

#define  COM_STATS_START               0
#define  COM_STATS_STOP                1
#define  COM_STATS_PRINT               2

//--------------------------------------------
// API return codes
//--------------------------------------------

#define  COM_NORMAL                       0
#define  COM_RECEIVE_BUFFER_OVERFLOW      1
#define  COM_TRANSMIT_BUFFER_OVERFLOW     2
#define  COM_DEVICE_OPEN                  3
#define  COM_DEVICE_TIMEOUT               4
#define  COM_RECEIVE_TIMEOUT              5
#define  COM_INVALID_DEVICE               6
#define  COM_PORT_IO_FAILURE              7
#define  COM_CONNECT_INTERRUPT_FAILED     8
#define  COM_PORT_NOT_OPEN                9
#define  COM_PORT_UNSUPPORTED_INTERRUPT   10
#define  COM_PORT_UNSOLICITED_INTERRUPT   11
#define  COM_DEVICE_BUSY                  12
#define  COM_INVALID_COMMAND              13
#define  COM_DRIVER_UNAVAILABLE           14
#define  COM_INVALID_PARAMETER            15
#define  COM_TIMER_ACTIVE                 16
#define  COM_CANNOT_CREATE_TIMER          17

#define  WAIT500MS                      500
#define  WAIT100MS                      100
