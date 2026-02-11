//////////////////////////////////////////////////////////////////////
//
// Serial.cpp: Linux implementation of the Serial class.
//
// When _HBM_SIM_ is defined, the serial read/write/count functions
// route through the HBM FIT7 load cell simulator instead of being
// no-ops. This exercises the full HBM serial code path.
//
//////////////////////////////////////////////////////////////////////

#include "types.h"

#ifdef _HBM_SIM_
#include "hbm_sim.h"
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
#endif

    thisUcb->state = COM_STATE_OPEN;

    return COM_NORMAL;
}

WORD Serial::RtReadComPort(BYTE *buffer, WORD bytesToRead, WORD *bytesRead)
{
#ifdef _HBM_SIM_
    *bytesRead = (WORD)HBMSim_Read(this->ucbObj.port, buffer, bytesToRead);
#else
    (void)buffer;
    (void)bytesToRead;
    *bytesRead = 0;
#endif
    return COM_NORMAL;
}

WORD Serial::RtWriteComPort(BYTE *buffer, WORD bytesToWrite, WORD *bytesWritten)
{
#ifdef _HBM_SIM_
    *bytesWritten = (WORD)HBMSim_Write(this->ucbObj.port, buffer, bytesToWrite);
#else
    (void)buffer;
    *bytesWritten = bytesToWrite;
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
    *count = 0;
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
