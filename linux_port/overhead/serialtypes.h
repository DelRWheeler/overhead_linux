#ifndef _SER_TYPES_
#define _SER_TYPES_

#define  XFER_BUFFER_SIZE              1024

//---------------------------------------
// Serial types - Linux port
// Original used direct UART register access.
// Linux version will use /dev/ttyS* with termios.
//---------------------------------------

class ser_typ
{
public:

typedef  struct
{
   BYTE        fifoSize;
   BYTE        parity;
   BYTE        stopBits;
   BYTE        wordSize;
   BYTE        baudRate;
   BYTE        flowControl;
}COM_DCB;

typedef struct
{
   WORD  state;
   WORD  lastError;
   WORD  errorCount;
}CSB;

typedef  struct
{
   BYTE  buffer[RING_BUFFER_SIZE];
   WORD  count;
   WORD  nextSlotOut;
   WORD  nextSlotIn;
}RINGBUFFER;

typedef  struct
{
   DWORD bytesRead;
   DWORD bytesWritten;
   DWORD readRequests;
   DWORD writeRequests;
   DWORD rcvInts;
   DWORD xmtInts;
   DWORD secondaryInts;
}COMSTATS;

typedef  struct
{
   WORD        port;
   PUCHAR      baseAddress;
   HANDLE      isrHandle;
   HANDLE      isrVectorHandle;
   WORD        irq;
   RINGBUFFER  *inBuffer;
   RINGBUFFER  *outBuffer;
   BYTE        fifoMask;
   BYTE        fifoSize;
   BYTE        parity;
   BYTE        stopBits;
   BYTE        receiveISRActive:1;
   BYTE        transmitISRActive:1;
   BYTE        timeDelimitedRcv:1;
   BYTE        statsActive:1;
   BYTE        spare:4;
   BYTE        state;
   BYTE        wordSize;
   BYTE        baudRate;
   BYTE        flowControl;
   DWORD       rcvTimer;
   WORD        lastError;
   WORD        errorCount;
   WORD        mode;  // RS232/RS422
}UCB;

typedef struct
{
   DWORD    rate;
   BYTE     DLHigh;
   BYTE     DLLow;
}BAUDRATE;

typedef struct
{
   BYTE  size;
   BYTE  mask;
}FIFO;

typedef  struct
{
   BYTE     command;
   DWORD    parmBlock[8];
   HANDLE   lock;
   BYTE     xferBuffer[XFER_BUFFER_SIZE];
   WORD     status;
   WORD     returnCode;
}DTA;

typedef struct
{
   HANDLE   cmdEvent;
   HANDLE   ackEvent;
   BOOL     active;
   BOOL     IOPending;
}DTA_CONTROL;

};

class Serial:ser_typ
{
private:

COMSTATS    csb;
RINGBUFFER* receiveRingBuffer;
RINGBUFFER* transmitRingBuffer;
RINGBUFFER  RxBuffer;
RINGBUFFER  TxBuffer;
PVOID       vAddress;

public:
    UCB       ucbObj;
    Serial(UCB setup);
    virtual ~Serial();

    WORD     InitializePort( UCB *ucb );
    WORD     RtOpenComPort(WORD baudRate, BYTE wordLength, BYTE stopBits, BYTE parity, BYTE flowControl );
    WORD     RtCloseComPort();
    WORD     RtReadComPort (BYTE *buffer, WORD bytesToRead, WORD *bytesRead );
    WORD     RtWriteComPort (BYTE *buffer, WORD bytesToWrite, WORD *bytesWritten );
    WORD     RtWaitForComData (BYTE *buffer, WORD bytesToRead, WORD *bytesRead );
    WORD     RtGetComBufferCount (WORD* count);
    WORD     RtGetComStatus (CSB * csb );
    WORD     RtConfigComPort (COM_DCB * dcb );
    WORD     RtSetComTimer(DWORD quantum );
    WORD     RtClearComTimer();
    WORD     RtComStats(BYTE command );
    void     DumpUcb(char *s, UCB *ucb );
    VOID     ReadPic( void );
    void     isr( UCB *ucb );
    void     SendNextFIFO ( UCB *ucb );
    void     EmptyFIFO ( UCB *ucb );
    static   void __stdcall COM1Isr( void *junk);
    static   void __stdcall COM2Isr( void *junk);
    static   void __stdcall COM3Isr( void *junk);
    static   void __stdcall COM4Isr( void *junk);
};

#endif
