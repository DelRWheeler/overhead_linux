//////////////////////////////////////////////////////////////////////
//
// Serial.cpp: Linux implementation of the Serial class.
//
// Three modes (compile-time selection via overheadconst.h):
//
// 1. _HBM_SIM_ defined: routes serial I/O through the in-process
//    HBM FIT7 load cell simulator (FIFO-based, no real serial port).
//
// 2. _HBM_SIM_ NOT defined: uses real Linux serial ports via termios.
//    COM1 = /dev/ttyS0, COM2 = /dev/ttyS1.
//    Sets SandCat FPGA registers for RS-485 transceiver mode.
//
//////////////////////////////////////////////////////////////////////

#include "types.h"

#ifdef _HBM_SIM_
#include "hbm_sim.h"
#else
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#endif

#undef  _FILE_
#define _FILE_ "Serial.cpp"

extern Serial* pser1;
extern Serial* pser2;
extern Serial* pser3;
extern Serial* pser4;

extern ser_typ::BAUDRATE baudRateTbl[7];
extern ser_typ::FIFO     fifoTbl[5];

//------------------------------------------------
// Table of baud rate values.
//-------------------------------------------------

ser_typ::BAUDRATE baudRateTbl[7] =
{
   {2400,0x00,0x30},
   {4800,0x00,0x18},
   {9600,0x00,0x0c},
   {19200,0x00,0x06},
   {38400,0x00,0x03},
   {57600,0x00,0x02},
   {115200,0x00,0x01}
};

//-------------------------------
// table for Fifo size masks
//-------------------------------

ser_typ::FIFO  fifoTbl[5] =
{
   {0,FCR_FIFO_DISABLED},
   {1,FCR_TRIGGER_1},
   {4,FCR_TRIGGER_4},
   {8,FCR_TRIGGER_8},
   {14,FCR_TRIGGER_14}
};

#ifndef _HBM_SIM_
//--------------------------------------------------------
// Real serial I/O support (Linux termios)
//--------------------------------------------------------

// File descriptors for each COM port (-1 = not open)
static int g_comFd[4] = { -1, -1, -1, -1 };

// COM port to Linux device path mapping
static const char* g_comDevPath[4] = {
    "/dev/ttyS0",   // COM1
    "/dev/ttyS1",   // COM2
    "/dev/ttyS2",   // COM3
    "/dev/ttyS3"    // COM4
};

// SandCat VersaLogic FPGA register addresses for RS-485 mode
#define FPGA_XCVRMODE   0xCA9   // Transceiver mode
#define FPGA_UARTMODE1  0xCB6   // UART mode 1

//--------------------------------------------------------
// Set SandCat FPGA registers for RS-485 transceiver mode
// via /dev/port direct I/O access.
//--------------------------------------------------------
static void SetFPGA_RS485(void)
{
    int fd = open("/dev/port", O_RDWR | O_SYNC);
    if (fd < 0)
    {
        RtPrintf("Serial: Warning: could not open /dev/port for FPGA setup (%d)\n", errno);
        RtPrintf("Serial: Continuing anyway (may already be set, or not running on SandCat)\n");
        return;
    }

    unsigned char val;

    // XCVRMODE: 0x03 = both COM1+COM2 in RS-485 mode
    lseek(fd, FPGA_XCVRMODE, SEEK_SET);
    val = 0x03;
    write(fd, &val, 1);

    // UARTMODE1: 0x33 = auto TX direction + UART enable for both
    lseek(fd, FPGA_UARTMODE1, SEEK_SET);
    val = 0x33;
    write(fd, &val, 1);

    close(fd);
    RtPrintf("Serial: FPGA RS-485 registers set (XCVRMODE=0x03, UARTMODE1=0x33)\n");
}

//--------------------------------------------------------
// Open and configure a Linux serial port with termios
// Returns file descriptor or -1 on error.
//--------------------------------------------------------
static int OpenLinuxSerial(int port_num)
{
    if (port_num < 0 || port_num > 3)
        return -1;

    const char* path = g_comDevPath[port_num];

    int fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0)
    {
        RtPrintf("Serial: Failed to open %s: errno=%d\n", path, errno);
        return -1;
    }

    // Clear O_NONBLOCK so reads can be used with VTIME/VMIN
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    // Configure termios: 38400 baud, 8N1, raw mode
    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    // Input: no break, no CR->NL, no parity check, no strip, no flow ctrl
    tty.c_iflag = 0;

    // Output: raw
    tty.c_oflag = 0;

    // Control: 8-bit, enable receiver, local mode (no modem signals)
    tty.c_cflag = CS8 | CREAD | CLOCAL;

    // Local: completely raw
    tty.c_lflag = 0;

    // VMIN=1: block until at least 1 byte available
    // VTIME=1: 100ms inter-character timeout
    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 1;

    // Baud rate
    cfsetispeed(&tty, B38400);
    cfsetospeed(&tty, B38400);

    if (tcsetattr(fd, TCSANOW, &tty) != 0)
    {
        RtPrintf("Serial: tcsetattr failed for %s: errno=%d\n", path, errno);
        close(fd);
        return -1;
    }

    // Flush any stale data
    tcflush(fd, TCIOFLUSH);

    RtPrintf("Serial: Opened %s (38400 8N1 RS-485) fd=%d\n", path, fd);
    return fd;
}
#endif // !_HBM_SIM_

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Serial::Serial(UCB setup)
{
    ucbObj = setup;
    receiveRingBuffer = &RxBuffer;
    transmitRingBuffer = &TxBuffer;
}

Serial::~Serial()
{
}

WORD Serial::RtOpenComPort(WORD baudRate, BYTE wordSize, BYTE stopBits, BYTE parity, BYTE flowControl)
{
    UCB *thisUcb;

    thisUcb = &this->ucbObj;

    if (thisUcb->state)
    {
        return COM_DEVICE_OPEN;
    }

    if (parity)
        thisUcb->parity = parity;
    if (stopBits)
        thisUcb->stopBits = stopBits;
    if (baudRate)
        thisUcb->baudRate = (BYTE)baudRate;
    if (wordSize)
        thisUcb->wordSize = wordSize;
    if (flowControl)
        thisUcb->flowControl = flowControl;

    thisUcb->inBuffer  = this->receiveRingBuffer;
    thisUcb->outBuffer = this->transmitRingBuffer;

    RtPrintf("RtOpenComPort::thisUcb->port=%d\n", thisUcb->port);

    if (thisUcb->port == COM1)
        pser1 = this;
    else if (thisUcb->port == COM2)
        pser2 = this;
    else if (thisUcb->port == COM3)
        pser3 = this;
    else if (thisUcb->port == COM4)
        pser4 = this;

#ifdef _HBM_SIM_
    // Initialize HBM FIT7 simulator for this port
    HBMSim_Init(thisUcb->port);
#else
    // Set FPGA registers for RS-485 mode (idempotent, safe to call multiple times)
    SetFPGA_RS485();

    // Open real Linux serial port
    int fd = OpenLinuxSerial(thisUcb->port);
    if (fd >= 0)
    {
        g_comFd[thisUcb->port] = fd;
    }
    else
    {
        RtPrintf("RtOpenComPort: WARNING - could not open COM%d serial device\n", thisUcb->port + 1);
    }
#endif

    thisUcb->state = COM_STATE_OPEN;

    return COM_NORMAL;
}

WORD Serial::RtReadComPort(BYTE *buffer, WORD bytesToRead, WORD *bytesRead)
{
#ifdef _HBM_SIM_
    *bytesRead = (WORD)HBMSim_Read(this->ucbObj.port, buffer, bytesToRead);
#else
    int fd = g_comFd[this->ucbObj.port];
    if (fd < 0)
    {
        *bytesRead = 0;
        return COM_NORMAL;
    }

    // Non-blocking check: see if data is available before reading
    int avail = 0;
    ioctl(fd, FIONREAD, &avail);
    if (avail <= 0)
    {
        *bytesRead = 0;
        return COM_NORMAL;
    }

    int toRead = (avail < bytesToRead) ? avail : bytesToRead;
    ssize_t n = read(fd, buffer, toRead);
    if (n < 0)
    {
        *bytesRead = 0;
        return COM_NORMAL;
    }
    *bytesRead = (WORD)n;
#endif
    return COM_NORMAL;
}

WORD Serial::RtWriteComPort(BYTE *buffer, WORD bytesToWrite, WORD *bytesWritten)
{
#ifdef _HBM_SIM_
    *bytesWritten = (WORD)HBMSim_Write(this->ucbObj.port, buffer, bytesToWrite);
#else
    int fd = g_comFd[this->ucbObj.port];
    if (fd < 0)
    {
        *bytesWritten = bytesToWrite;  // Pretend written if no device
        return COM_NORMAL;
    }

    ssize_t n = write(fd, buffer, bytesToWrite);
    if (n < 0)
    {
        *bytesWritten = 0;
        return COM_NORMAL;
    }
    *bytesWritten = (WORD)n;
#endif
    return COM_NORMAL;
}

WORD Serial::RtGetComStatus(CSB *csb)
{
    UCB *ucb;

    ucb = &this->ucbObj;

    if (!ucb->state)
    {
        return COM_PORT_NOT_OPEN;
    }
    csb->state      = ucb->state;
    csb->lastError  = ucb->lastError;
    csb->errorCount = ucb->errorCount;
    return COM_NORMAL;
}

WORD Serial::RtGetComBufferCount(WORD* count)
{
#ifdef _HBM_SIM_
    *count = (WORD)HBMSim_Available(this->ucbObj.port);
#else
    int fd = g_comFd[this->ucbObj.port];
    if (fd < 0)
    {
        *count = 0;
        return NO_ERRORS;
    }

    int avail = 0;
    if (ioctl(fd, FIONREAD, &avail) < 0)
        avail = 0;
    *count = (WORD)avail;
#endif
    return NO_ERRORS;
}

WORD Serial::RtCloseComPort()
{
    UCB *ucb;

    ucb = &this->ucbObj;

    if (!ucb->state)
    {
        return COM_PORT_NOT_OPEN;
    }

#ifndef _HBM_SIM_
    // Close the real serial port
    int fd = g_comFd[ucb->port];
    if (fd >= 0)
    {
        close(fd);
        g_comFd[ucb->port] = -1;
        RtPrintf("Serial: Closed COM%d (fd=%d)\n", ucb->port + 1, fd);
    }
#endif

    ucb->state = COM_STATE_IDLE;

    RINGBUFFER *rb;

    rb = ucb->inBuffer;
    if (rb)
    {
        rb->count = 0;
        rb->nextSlotIn = 0;
        rb->nextSlotOut = 0;
    }

    rb = ucb->outBuffer;
    if (rb)
    {
        rb->count = 0;
        rb->nextSlotIn = 0;
        rb->nextSlotOut = 0;
    }

    ucb->stopBits = DEFAULT_STOP_BITS;
    ucb->parity = DEFAULT_PARITY;
    ucb->baudRate = DEFAULT_BAUD_RATE;
    ucb->wordSize = DEFAULT_WORDSIZE;
    ucb->fifoSize = DEFAULT_FIFO_SIZE;
    ucb->fifoMask = DEFAULT_FIFO_MASK;
    ucb->receiveISRActive = 0;
    ucb->transmitISRActive = 0;
    ucb->statsActive = 0;
    ucb->lastError = 0;
    ucb->errorCount = 0;

    return COM_NORMAL;
}

WORD Serial::InitializePort(UCB *ucb)
{
    (void)ucb;
    return COM_NORMAL;
}

WORD Serial::RtConfigComPort(COM_DCB *dcb)
{
    UCB *ucb;

    ucb = &this->ucbObj;

    if (!ucb->state)
    {
        return COM_PORT_NOT_OPEN;
    }

    if (dcb->fifoSize > COM_FIFO_14)
    {
        return COM_INVALID_PARAMETER;
    }
    else
    {
        ucb->fifoSize = fifoTbl[dcb->fifoSize].size;
        ucb->fifoMask = fifoTbl[dcb->fifoSize].mask;
    }

    ucb->parity      = dcb->parity;
    ucb->stopBits    = dcb->stopBits;
    ucb->wordSize    = dcb->wordSize;
    ucb->baudRate    = dcb->baudRate;
    ucb->flowControl = dcb->flowControl;

    return COM_NORMAL;
}

WORD Serial::RtComStats(BYTE command)
{
    UCB      *thisUcb;
    COMSTATS *thisCsb;

    thisUcb = &this->ucbObj;
    thisCsb = &this->csb;

    switch (command)
    {
        case COM_STATS_START:
            memset(thisCsb, 0, sizeof(COMSTATS));
            thisUcb->statsActive = TRUE;
            break;
        case COM_STATS_STOP:
            memset(thisCsb, 0, sizeof(COMSTATS));
            thisUcb->statsActive = FALSE;
            break;
        case COM_STATS_PRINT:
            RtPrintf(" ***** Com Stats (stub) *****\n");
            break;
        default:
            return COM_INVALID_COMMAND;
    }
    return COM_NORMAL;
}

void Serial::DumpUcb(char *s, UCB *ucb)
{
    RtPrintf("***  UCB Dump from %s  ***\n\n", s);
    RtPrintf(" port:         %d\n", ucb->port);
    RtPrintf(" baseAddress:  %lx\n", (unsigned long)ucb->baseAddress);
    RtPrintf(" baudRate:     %d\n", ucb->baudRate);
    RtPrintf(" state:        %d\n", ucb->state);
}

VOID Serial::ReadPic(void)
{
    // No-op on Linux - no PIC hardware access
}

void __stdcall Serial::COM1Isr(void *junk)
{
    (void)junk;
    // No-op on Linux - no hardware ISR
}

void __stdcall Serial::COM2Isr(void *junk)
{
    (void)junk;
    // No-op on Linux - no hardware ISR
}

void __stdcall Serial::COM3Isr(void *junk)
{
    (void)junk;
    // No-op on Linux - no hardware ISR
}

void __stdcall Serial::COM4Isr(void *junk)
{
    (void)junk;
    // No-op on Linux - no hardware ISR
}

void Serial::isr(ser_typ::UCB *ucb)
{
    (void)ucb;
    // No-op on Linux - no hardware ISR
}

VOID Serial::SendNextFIFO(ser_typ::UCB *ucb)
{
    (void)ucb;
    // No-op on Linux - no hardware FIFO
}

VOID Serial::EmptyFIFO(ser_typ::UCB *ucb)
{
    (void)ucb;
    // No-op on Linux - no hardware FIFO
}
