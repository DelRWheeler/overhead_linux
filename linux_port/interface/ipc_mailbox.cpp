//////////////////////////////////////////////////////////////////////
// ipc_mailbox.cpp: Linux port of mailbox creation (from client.cpp)
//                  and PostAppRxMessage/PostIsysRxMessage (from server.cpp)
//
// These functions use POSIX shared memory and named semaphores to
// provide the same IPC mechanism as the RTX version. All message
// formats and routing logic are unchanged.
//////////////////////////////////////////////////////////////////////

#include "types.h"

#undef  _FILE_
#define _FILE_      "ipc_mailbox.cpp"

// External declarations
extern SOCKET sock;
extern SOCKADDR_IN saUdpServ;

// Global variables for mailbox creation
DWORD   dwMaximumSizeHigh  = 0;
LONG    lInitialCount      = 0;
LONG    lMaximumCount      = 1;
LONG    lReleaseCount      = 1;

//--------------------------------------------------------
//   CreateTxMailbox
//--------------------------------------------------------

int CreateTxMailbox()
{
    hTxShm = RtCreateSharedMemory(PAGE_READWRITE, dwMaximumSizeHigh,
                                   MAX_APP_MSG_SIZE, SEND_SHM, (LPVOID*)&pTxMsg);

    if (GetLastError() == ERROR_ALREADY_EXISTS)
        RtPrintf("Warning!\nTx shared memory already exists.\nThe MsgServer may already be running.\n");

    if (hTxShm == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    pTxMsg->Ack = 1;

    hTxSemPost = RtCreateSemaphore(NULL, lInitialCount, lMaximumCount, SEND_SEM_POST);

    if (hTxSemPost == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    hTxSemAck = RtCreateSemaphore(NULL, lInitialCount, lMaximumCount, SEND_SEM_ACK);

    if (hTxSemAck == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    hTxMutex = RtCreateMutex(NULL, FALSE, SEND_MUTEX);

    if (hTxMutex == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    RtPrintf("Tx mailbox created\n");
    return NO_ERRORS;
}

//--------------------------------------------------------
//   CreateHstTxMailbox
//--------------------------------------------------------

int CreateHstTxMailbox()
{
    hHstTxShm = RtCreateSharedMemory(PAGE_READWRITE, dwMaximumSizeHigh,
                                      MAX_APP_MSG_SIZE, SEND_HST_SHM, (LPVOID*)&pHstTxMsg);

    if (GetLastError() == ERROR_ALREADY_EXISTS)
        RtPrintf("Warning!\nHstTx shared memory already exists.\n");

    if (hHstTxShm == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    pHstTxMsg->Ack = 1;

    hHstTxSemPost = RtCreateSemaphore(NULL, lInitialCount, lMaximumCount, SEND_HST_SEM_POST);

    if (hHstTxSemPost == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    hHstTxSemAck = RtCreateSemaphore(NULL, lInitialCount, lMaximumCount, SEND_HST_SEM_ACK);

    if (hHstTxSemAck == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    hHstTxMutex = RtCreateMutex(NULL, FALSE, SEND_HST_MUTEX);

    if (hHstTxMutex == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    RtPrintf("Host Tx mailbox created\n");
    return NO_ERRORS;
}

//--------------------------------------------------------
//   CreateRxMailbox
//--------------------------------------------------------

int CreateRxMailbox()
{
    hRxShm = RtCreateSharedMemory(PAGE_READWRITE, dwMaximumSizeHigh,
                                   MAX_APP_MSG_SIZE, RECV_SHM, (LPVOID*)&pRxMsg);

    if (GetLastError() == ERROR_ALREADY_EXISTS)
        RtPrintf("Warning!\nRx shared memory already exists.\n");

    if (hRxShm == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    pRxMsg->Ack = 1;

    hRxSemPost = RtCreateSemaphore(NULL, lInitialCount, lMaximumCount, RECV_SEM_POST);

    if (hRxSemPost == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    hRxSemAck = RtCreateSemaphore(NULL, lInitialCount, lMaximumCount, RECV_SEM_ACK);

    if (hRxSemAck == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    hRxMutex = RtCreateMutex(NULL, FALSE, RECV_MUTEX);

    if (hRxMutex == NULL)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    RtPrintf("Rx mailbox created\n");
    return NO_ERRORS;
}

//--------------------------------------------------------
//   PostIsysRxMessage (unchanged logic)
//--------------------------------------------------------

int PostIsysRxMessage(PCHAR Message, int bytes)
{
    static  HANDLE      hShm, hMutex, hSemPost, hSemAck;
    static  BOOL        bInit = FALSE;
    static  PAPPIPCMSG  pMsg;
    LONG    i;
    LONG    abandoned     = 0;
    LONG    lReleaseCount = 1;
    int     len;
    mhdr*   phdr;

    if (!bInit)
    {
        hMutex = RtOpenMutex(SYNCHRONIZE, FALSE, RECV_MUTEX);

        if (hMutex == NULL)
        {
            RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hShm = RtOpenSharedMemory(PAGE_READWRITE, FALSE, RECV_SHM, (LPVOID*)&pMsg);

        if (hShm == NULL)
        {
            RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
            RtCloseHandle(hMutex);
            return ERROR_OCCURED;
        }

        hSemPost = RtOpenSemaphore(SYNCHRONIZE, FALSE, RECV_SEM_POST);

        if (hSemPost == NULL)
        {
            RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
            RtCloseHandle(hShm);
            RtCloseHandle(hMutex);
            return ERROR_OCCURED;
        }

        hSemAck = RtOpenSemaphore(SYNCHRONIZE, FALSE, RECV_SEM_ACK);

        if (hSemAck == NULL)
        {
            RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
            RtCloseHandle(hShm);
            RtCloseHandle(hMutex);
            RtCloseHandle(hSemPost);
            return ERROR_OCCURED;
        }

        bInit = TRUE;
    }

    if (RtWaitForSingleObject(hMutex, WAIT100MS) != WAIT_OBJECT_0)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    phdr = (mhdr*)Message;
    len = phdr->len + sizeof(mhdr);

    for (i = 0; i < len; i++)
        pMsg->Buffer[i] = Message[i];

    pMsg->Len    = len;
    pMsg->LineId = phdr->srcLineId;

    if (len == bytes)
    {
        pMsg->Ack = 0;

        if (!RtReleaseSemaphore(hSemPost, lReleaseCount, NULL))
        {
            RtReleaseMutex(hMutex);
            RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        for (;;)
        {
            if (RtWaitForSingleObject(hSemAck, INFINITE) == WAIT_FAILED)
            {
                RtReleaseMutex(hMutex);
                RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
                return ERROR_OCCURED;
            }

            if (pMsg->Ack != abandoned)
                break;
        }
    }

    if (!RtReleaseMutex(hMutex))
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
//   PostAppRxMessage (unchanged logic)
//--------------------------------------------------------

int PostAppRxMessage(PCHAR Message, int bytes)
{
    static  HANDLE      hAppShm, hAppMutex, hAppSemPost, hAppSemAck;
    static  BOOL        bInit = FALSE;
    static  PAPPIPCMSG  pAppMsg;
    LONG    i;
    LONG    abandoned     = 0;
    LONG    lReleaseCount = 1;
    int     len;
    mhdr*   phdr;

    (void)bytes;

    if (!bInit)
    {
        hAppMutex = RtOpenMutex(SYNCHRONIZE, FALSE, APP_MUTEX);

        if (hAppMutex == NULL)
        {
            RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hAppShm = RtOpenSharedMemory(PAGE_READWRITE, FALSE, APP_SHM, (LPVOID*)&pAppMsg);

        if (hAppShm == NULL)
        {
            RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
            RtCloseHandle(hAppMutex);
            return ERROR_OCCURED;
        }

        hAppSemPost = RtOpenSemaphore(SYNCHRONIZE, FALSE, APP_SEM_POST);

        if (hAppSemPost == NULL)
        {
            RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
            RtCloseHandle(hAppShm);
            RtCloseHandle(hAppMutex);
            return ERROR_OCCURED;
        }

        hAppSemAck = RtOpenSemaphore(SYNCHRONIZE, FALSE, APP_SEM_ACK);

        if (hAppSemAck == NULL)
        {
            RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
            RtCloseHandle(hAppShm);
            RtCloseHandle(hAppMutex);
            RtCloseHandle(hAppSemPost);
            return ERROR_OCCURED;
        }

        bInit = TRUE;
    }

    if (RtWaitForSingleObject(hAppMutex, WAIT100MS) != WAIT_OBJECT_0)
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    phdr = (mhdr*)Message;
    len = phdr->len + sizeof(mhdr);

    for (i = 0; i < len; i++)
        pAppMsg->Buffer[i] = Message[i];

    pAppMsg->Len    = len;
    pAppMsg->LineId = phdr->srcLineId;

    pAppMsg->Ack = 0;

    if (!RtReleaseSemaphore(hAppSemPost, lReleaseCount, NULL))
    {
        RtReleaseMutex(hAppMutex);
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    for (;;)
    {
        if (RtWaitForSingleObject(hAppSemAck, INFINITE) == WAIT_FAILED)
        {
            RtReleaseMutex(hAppMutex);
            RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
            return ERROR_OCCURED;
        }
        if (pAppMsg->Ack != abandoned)
            break;
    }

    if (!RtReleaseMutex(hAppMutex))
    {
        RtPrintf("Error file %s, line %d\n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
// UdpTraceThread (unchanged logic)
//--------------------------------------------------------

int RTFCNDCL UdpTraceThread(PVOID unused)
{
    static int Indx = 0;
    static int TraceId = 0;
    static int NothingNewCnt = 0;
    char    TraceString[MAXERRMBUFSIZE];
    int     err;

    (void)unused;

    if (pTraceMemory != NULL)
    {
        memset((void*)pTraceMemory, 0, sizeof(app_type::TRACE_MEMORY));

        for (;;)
        {
            if (pTraceMemory->UdpTraceData[Indx].TraceId != 0)
            {
                NothingNewCnt = 0;
                TraceId = pTraceMemory->UdpTraceData[Indx].TraceId;
                strncpy(&TraceString[0],
                         pTraceMemory->UdpTraceData[Indx].TraceString,
                         MAXERRMBUFSIZE);
                pTraceMemory->UdpTraceData[Indx].TraceId = 0;

                err = sendto(sock,
                             TraceString,
                             strlen(&TraceString[0]),
                             0,
                             (SOCKADDR*)&saUdpServ,
                             sizeof(SOCKADDR_IN));

                if (err < 0 && errno != ENETUNREACH)
                {
                    RtPrintf("DoUdpClient:sendto error %d\n", errno);
                }
                memset((void*)pTraceMemory->UdpTraceData[Indx].TraceString,
                       0, MAXERRMBUFSIZE);
                Indx++;
                if (Indx > MAXTRCMBUFSIZE)
                {
                    Indx = 0;
                }
            }
            else
            {
                NothingNewCnt++;
                if (NothingNewCnt > 100)
                {
                    Indx = pTraceMemory->LatestEntry;
                }
            }
            Sleep(30);
        }
    }

    return NO_ERRORS;
}
