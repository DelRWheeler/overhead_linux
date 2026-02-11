//////////////////////////////////////////////////////////////////////
// InterSystems.cpp: implementation of the InterSystems class.
//
//////////////////////////////////////////////////////////////////////

#include "types.h"

#undef  _FILE_
#define _FILE_      "InterSystems.cpp"

// read/write queue mutex
HANDLE      hWQ;

// drop manager mailbox
HANDLE      hIDrpMgrShm, hIDrpMgrMutex, hIDrpMgrSemPost, hIDrpMgrSemAck;
PIPCMSG     pIDrpMgrMsg;
PIPCMSG     pIsysMsg;

LONG        lInitialCount   = 0;
LONG        lMaximumCount   = 1;
LONG        lReleaseCount   = 1;

// Message Queue array
uRQentry    rdQ              [ISYS_MAXRQENTRYS];
uWQentry    wrtQ             [ISYS_MAXWQENTRYS];
int         write_fail_count [MAXIISYSLINES];

#ifdef _ISYS_TIMING_TEST_
    static LARGE_INTEGER StartTime, EndTime;
    static UINT iseq_num = 0;
#endif

// The lengths of messages to transmit. Some are 0 because they aren't
// transmitted over the network

const int uRxLen[NUM_ISYS_CMDS] = {  // see enum in overheadconst.h
     0,                             // 0 DUMMY
     sizeof(tMaintRespMsg),         // 1 ISYS_ACK (whatever the longest response is)
     sizeof(tPchkReqMsg),           // 2 ISYS_PORTCHK
     sizeof(tMaintReqMsg),          // 3 ISYS_DROP_MAINT
     sizeof(tDrpReqMsg),            // 6 ISYS_DROPMSG
};

char    isys_dt_str         [MAXDATETIMESTR];   // place to build a date/time string
char    isys_err_buf        [MAXERRMBUFSIZE];

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

InterSystems::InterSystems()
{
    initialize();
}

InterSystems::~InterSystems()
{

}

LONG __stdcall InterSystems::IsysMainUnhandledExceptionFilter(EXCEPTION_POINTERS* pExPtrs)
{
    RtPrintf("*** IsysMain Exception! ***\nExpCode:\t0x%8.8X\nExpFlags:\t%d\nExpAddress:\t0x%8.8X\n",
      pExPtrs->ExceptionRecord->ExceptionCode,
      pExPtrs->ExceptionRecord->ExceptionFlags,
      pExPtrs->ExceptionRecord->ExceptionAddress);

    EXCEPTION_SHUTDOWN

   return EXCEPTION_CONTINUE_SEARCH;
}

LONG __stdcall InterSystems::IsysMbxUnhandledExceptionFilter(EXCEPTION_POINTERS* pExPtrs)
{
    RtPrintf("*** IsysMbx Exception! ***\nExpCode:\t0x%8.8X\nExpFlags:\t%d\nExpAddress:\t0x%8.8X\n",
      pExPtrs->ExceptionRecord->ExceptionCode,
      pExPtrs->ExceptionRecord->ExceptionFlags,
      pExPtrs->ExceptionRecord->ExceptionAddress);

    EXCEPTION_SHUTDOWN

  return EXCEPTION_CONTINUE_SEARCH;
}

LONG __stdcall InterSystems::IsysRxUnhandledExceptionFilter(EXCEPTION_POINTERS* pExPtrs)
{
    RtPrintf("*** IsysRxServer Exception! ***\nExpCode:\t0x%8.8X\nExpFlags:\t%d\nExpAddress:\t0x%8.8X\n",
      pExPtrs->ExceptionRecord->ExceptionCode,
      pExPtrs->ExceptionRecord->ExceptionFlags,
      pExPtrs->ExceptionRecord->ExceptionAddress);

    EXCEPTION_SHUTDOWN

   return EXCEPTION_CONTINUE_SEARCH;
}

//--------------------------------------------------------
//  initialize: starts everything 
//--------------------------------------------------------

void InterSystems::initialize()
{

    if ((OpenDrpMgrMailbox() != NO_ERRORS))
    {
        sprintf(isys_err_buf,"Can't open mailbox %s line %d\n", _FILE_, __LINE__);
        app->GenError(critical, isys_err_buf);
        return;
    }

    if ((CreateISysThread() != NO_ERRORS))
    {
        sprintf(isys_err_buf,"Can't create thread %s line %d\n", _FILE_, __LINE__);
        app->GenError(critical, isys_err_buf);
        return;
    }

    if ((CreateMbxServer() != NO_ERRORS))
    {
        sprintf(isys_err_buf,"Can't create thread %s line %d\n", _FILE_, __LINE__);
        app->GenError(critical, isys_err_buf);
        return;
    }

    if ((CreateRxServerThread() != NO_ERRORS))
    {
        sprintf(isys_err_buf,"Can't create thread %s line %d\n", _FILE_, __LINE__);
        app->GenError(critical, isys_err_buf);
        return;
    }


//----- Create/Open Mutex for access to write message Q

    hWQ = RtCreateMutex( NULL, FALSE, "wQ_post");

    if ( hWQ == NULL )
    {
        sprintf(isys_err_buf,"Can't create mutex %s line %d\n", _FILE_, __LINE__);
        app->GenError(critical, isys_err_buf);
    }

//----- Create/Open Mutex for trace buffer

    trc[ISYSMAINBUFID].mutex = RtCreateMutex( NULL, FALSE, "is_trace");

    if ( trc[ISYSMAINBUFID].mutex == NULL )
    {
        sprintf(isys_err_buf,"Can't create mutex %s line %d\n", _FILE_, __LINE__);
        app->GenError(critical, isys_err_buf);
    }

    trc[ISYSMBXBUFID].mutex = RtCreateMutex( NULL, FALSE, "is_mbx_trace");

    if ( trc[ISYSMBXBUFID].mutex == NULL )
    {
        sprintf(isys_err_buf,"Can't create mutex %s line %d\n", _FILE_, __LINE__);
        app->GenError(critical, isys_err_buf);
    }
}

//--------------------------------------------------------
//   OpenDrpMgrMailbox
//--------------------------------------------------------

int InterSystems::OpenDrpMgrMailbox()
{

    //----- Create/Open Mutex for sending messages to Drop Manager

    hIDrpMgrMutex = RtOpenMutex( SYNCHRONIZE, FALSE, DRPMGR_MUTEX);

    if (hIDrpMgrMutex==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    hIDrpMgrShm = RtOpenSharedMemory( PAGE_READWRITE, FALSE, DRPMGR_SHM, (LPVOID*) &pIDrpMgrMsg);

    if (hIDrpMgrShm==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        RtCloseHandle(hIDrpMgrMutex);
        return ERROR_OCCURED;
    }

    hIDrpMgrSemPost = RtOpenSemaphore( SYNCHRONIZE, FALSE, DRPMGR_SEM_POST);

    if (hIDrpMgrSemPost==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        RtCloseHandle(hIDrpMgrShm);
        RtCloseHandle(hIDrpMgrMutex);
        return ERROR_OCCURED;
    }

    hIDrpMgrSemAck = RtOpenSemaphore( SYNCHRONIZE, FALSE, DRPMGR_SEM_ACK);
    if (hIDrpMgrSemAck==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        RtCloseHandle(hIDrpMgrShm);
        RtCloseHandle(hIDrpMgrMutex);
        RtCloseHandle(hIDrpMgrSemPost);
        return ERROR_OCCURED;
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
//   CreateISysThread
//--------------------------------------------------------

int InterSystems::CreateISysThread()
{

    DWORD            threadId;
    
    hIsys = NULL;
    
    hIsys = CreateThread(
                        NULL,            
                        0,               //default
                        (LPTHREAD_START_ROUTINE) ISysThread, //function
                        NULL,            //parameters
                        0,
                        (LPDWORD) &threadId
                        );

    if(hIsys == NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    if(!RtSetThreadPriority( hIsys, RT_PRIORITY_MIN+50)) //RCB - Was 50
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        TerminateThread( hIsys, 1);
        return ERROR_OCCURED;
    }

#ifdef _SHOW_HANDLES_
    RtPrintf("Hnd %.2X IntSys thread\n", threadId);
#endif

    return NO_ERRORS;
  }

//--------------------------------------------------------
//  InterSys thread
//--------------------------------------------------------

int __stdcall InterSystems::ISysThread(PVOID unused)
{
	unused; // RED - Added to eliminate waring for unreferenced formal parameter
    static int last_rx = 0;
    int  i;
    bool found;

    SetUnhandledExceptionFilter(IsysMainUnhandledExceptionFilter);

    // wait till all associated threads indicate they are ready
    RtWaitForSingleObject(hInterfaceThreadsOk,INFINITE);

#ifdef _SHOW_HANDLES_
    RtPrintf("Hnd %.2X ISysThread\n", hIsys);
#endif

//----- Clear message queue

    for ( i = 0; i < ISYS_MAXRQENTRYS; i++ )
        memset( &rdQ[i], 0, sizeof(uRQentry) );

    for ( i = 0; i < ISYS_MAXWQENTRYS; i++ )
        memset( &wrtQ[i], 0, sizeof(uWQentry) );

    for ( i = 0; i < MAXIISYSLINES; i++ )
        write_fail_count[i] = 3;

///////////////////////////////////////////////////////////////////////////////
//                               Main Loop
///////////////////////////////////////////////////////////////////////////////

	bool forever = true; //	RED - Added to eliminate unreachable code error

    while(forever)	//	RED - Changed to eliminate unreachable code error

    {

//----- process rx Q

        found = false;
        for (i = 0; i < ISYS_MAXRQENTRYS; i++)
        {
            // look for a rx message to process
            if (rdQ[last_rx].gen.flags == ISYS_USEDQ)
            {
                Isys->ProcessRx(last_rx);
                rdQ[last_rx].gen.flags = ISYS_UNUSEDQ;
                found = true;
            }

            if (++last_rx >= ISYS_MAXRQENTRYS)
                last_rx = 0;

            //if (found) break;
        }

        Sleep (1);

//----- process tx Q

        Isys->ProcessWrites();

        // ok to send intersystems thread trace
        if ( trc[ISYSMAINBUFID].send_OK )
        {
            // Request Mutex for send
            if(RtWaitForSingleObject(trc[ISYSMAINBUFID].mutex, WAIT50MS) != WAIT_OBJECT_0)
                RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
            else
            {
// Send debug to self
#ifdef _REDIRECT_DEBUG_
                Isys->SendLineMsg( DEBUG_MSG,app->this_lineid, NULL, 
                                   (BYTE*) &trc_buf[ISYSMAINBUFID], 
                                   trc[ISYSMAINBUFID].len);
#endif

#ifdef _LOCAL_DEBUG_
						if (UDP_ENABLE & app->pShm->dbg_set.UdpTrace)
						{
#ifdef	_TELNET_UDP_ENABLED_
							UdpTraceNBuf((char*) &trc_buf[ISYSMAINBUFID], trc[ISYSMAINBUFID].len);
#else
							PrintNBuf((char*) &trc_buf[ISYSMAINBUFID], trc[ISYSMAINBUFID].len);
#endif
						}
						else
						{
							PrintNBuf((char*) &trc_buf[ISYSMAINBUFID], trc[ISYSMAINBUFID].len);
						}
#endif

#ifdef	_HOST_DEBUG_
                if HOST_OK
                    Isys->SendHostMsg( DEBUG_MSG, NULL, 
                                       (BYTE*) &trc_buf[ISYSMAINBUFID], 
                                       trc[ISYSMAINBUFID].len);
#endif
                if(!RtReleaseMutex(trc[ISYSMAINBUFID].mutex))
                    RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
            }

            // clear old message.
            memset(&trc_buf[ISYSMAINBUFID], 0, sizeof(trcbuf));
            sprintf( (char*) &trc_buf[ISYSMAINBUFID],"L%d:%s\n",
                     app->this_lineid,
                     &trc_buf_desc[ISYSMAINBUFID]);

            trc[ISYSMAINBUFID].send_OK = false;
        }
  		forever = forever ? 1 : 0;	//	RED - Added to eliminate unreachable code error
  }
    return 0;
}

//--------------------------------------------------------
//   CreateRxServerThread
//--------------------------------------------------------

int InterSystems::CreateRxServerThread()
{

    DWORD            threadId;
    
    hIsysRx = NULL;
    
    hIsysRx = CreateThread(
                        NULL,            
                        0,               //default
                        (LPTHREAD_START_ROUTINE) RxServer, //function
                        NULL,            //parameters
                        0,
                        (LPDWORD) &threadId
                        );


    if(hIsysRx == NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    if(!RtSetThreadPriority( hIsysRx, RT_PRIORITY_MIN+50))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        TerminateThread( hIsysRx, 1);
        return ERROR_OCCURED;
    }

#ifdef _SHOW_HANDLES_
    RtPrintf("hIsysRx\tthread\t%x \n",threadId);
#endif

    return NO_ERRORS;
  }

//--------------------------------------------------------
//   CreateMbxServer
//--------------------------------------------------------

int InterSystems::CreateMbxServer()
{

    DWORD            threadId;
    
    hIsysMbx = NULL;
    
    hIsysMbx = CreateThread(
                        NULL,            
                        0,               //default
                        (LPTHREAD_START_ROUTINE) Mbx_Server, //function
                        NULL,            //parameters
                        0,
                        (LPDWORD) &threadId
                        );


    if(hIsysMbx == NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    if(!RtSetThreadPriority( hIsysMbx, RT_PRIORITY_MIN+50)) 
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        TerminateThread( hIsysMbx, 1);
        return ERROR_OCCURED;
    }

#ifdef _SHOW_HANDLES_
    RtPrintf("Hnd %.2X IsysMbx thread\n", threadId);
#endif

    return NO_ERRORS;
  }

//--------------------------------------------------------
//   Mbx_Server
//--------------------------------------------------------

int RTFCNDCL InterSystems::Mbx_Server(PVOID unused)
{
	unused; // RED - Added to eliminate waring for unreferenced formal parameter
    static HANDLE   hIsysShm,    hIsysMutex,    hIsysSemPost,    hIsysSemAck;
    static BOOL     bInit = FALSE;
    int             event;

    //
    // Open the required IPC objects upon first call.
    //
    // NOTE: All IPC objects are closed by the system when this client process exits.
    //
    if (!bInit)
    {

        SetUnhandledExceptionFilter(IsysMbxUnhandledExceptionFilter);

        hIsysMutex = RtOpenMutex( SYNCHRONIZE, FALSE, ISYS_MUTEX);
        if (hIsysMutex==NULL)
        {
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hIsysShm = RtOpenSharedMemory( PAGE_READWRITE, FALSE, ISYS_SHM, (LPVOID*) &pIsysMsg);

        if (hIsysShm==NULL)
        {
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            RtCloseHandle(hIsysMutex);
            return ERROR_OCCURED;
        }

        hIsysSemPost = RtOpenSemaphore( SYNCHRONIZE, FALSE, ISYS_SEM_POST);

        if (hIsysSemPost==NULL)
        {
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            RtCloseHandle(hIsysShm);
            RtCloseHandle(hIsysMutex);
            return ERROR_OCCURED;
        }

        hIsysSemAck = RtOpenSemaphore( SYNCHRONIZE, FALSE, ISYS_SEM_ACK);
        if (hIsysSemAck==NULL)
        {
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            RtCloseHandle(hIsysShm);
            RtCloseHandle(hIsysMutex);
            RtCloseHandle(hIsysSemPost);
            return ERROR_OCCURED;
        }

        bInit = true;
    }

    // indicate this thread is ready
    app->pShm->IsysLineStatus.app_threads |= RTSS_IS_MBX_SVR_RDY;

#ifdef _SHOW_HANDLES_
    RtPrintf("Hnd %.2X isys Mbx_Server\n", hIsysMbx);
#endif

    for (;;)
    {
       // check for everything ok
       if(RtWaitForSingleObject(hInterfaceThreadsOk,INFINITE)==WAIT_FAILED)
           RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
       else
       {
            event = RtWaitForSingleObject( hIsysSemPost, WAIT200MS);

            switch(event)
            {
                case WAIT_OBJECT_0:

                    Isys->ProcessMbxMsg();
                    pIsysMsg->Ack = 1;

                    if(!RtReleaseSemaphore( hIsysSemAck, lReleaseCount, NULL))
                        RtPrintf("RtReleaseSemaphore failed.\n");
                    break;

                case WAIT_TIMEOUT:

                    // ok to send intersystems mailbox thread trace
                    if ( trc[ISYSMBXBUFID].send_OK )
                    {
                        // Request Mutex for send
                        if(RtWaitForSingleObject(trc[ISYSMBXBUFID].mutex, WAIT50MS) != WAIT_OBJECT_0)
                            RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                        else
                        {
// Send debug to self
#ifdef _REDIRECT_DEBUG_
                            Isys->SendLineMsg( DEBUG_MSG, app->this_lineid, NULL, 
                                               (BYTE*) &trc_buf[ISYSMBXBUFID], 
                                               trc[ISYSMBXBUFID].len);
#endif

#ifdef _LOCAL_DEBUG_
						if (UDP_ENABLE & app->pShm->dbg_set.UdpTrace)
						{
#ifdef	_TELNET_UDP_ENABLED_
							UdpTraceNBuf((char*) &trc_buf[ISYSMBXBUFID], trc[ISYSMBXBUFID].len);
#else
							PrintNBuf((char*) &trc_buf[ISYSMBXBUFID], trc[ISYSMBXBUFID].len);
#endif
						}
						else
						{
							PrintNBuf((char*) &trc_buf[ISYSMBXBUFID], trc[ISYSMBXBUFID].len);
						}
#endif

#ifdef	_HOST_DEBUG_
                            if HOST_OK
                                Isys->SendHostMsg( DEBUG_MSG, NULL, 
                                                   (BYTE*) &trc_buf[ISYSMBXBUFID], 
                                                   trc[ISYSMBXBUFID].len);
#endif
                            if(!RtReleaseMutex(trc[ISYSMBXBUFID].mutex))
                                RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                        }

                        // clear old message.
                        memset(&trc_buf[ISYSMBXBUFID], 0, sizeof(trcbuf));
                        sprintf( (char*) &trc_buf[ISYSMBXBUFID],"L%d:%s\n",
                                 app->this_lineid,
                                 &trc_buf_desc[ISYSMBXBUFID]);

                        trc[ISYSMBXBUFID].send_OK = false;
                    }
                    break;

                default:
                    RtPrintf("Error %u file %s, line %d \n", GetLastError(), _FILE_, __LINE__);
                    break;

            }
       }
    }
}

//--------------------------------------------------------
//  ProcessMbxMsg
//--------------------------------------------------------

void InterSystems::ProcessMbxMsg()
{
    TIsysIpcMsg* iMsg;


    iMsg = (TIsysIpcMsg*) &pIsysMsg->Buffer;

    // validate message
    if ( (iMsg->imsg.cmd <= ISYS_MSG_START) || 
         (iMsg->imsg.cmd >= LAST_ISYS_MSG) )
        return;

    // Due to the volume of IPC messages, these will not go to host
	DebugTrace(_IPC_, "Isys\tMbx\t[%s]\n",
					isys_msg_desc[iMsg->imsg.cmd - ISYS_MSG_START]);

    switch(iMsg->imsg.cmd)
    {
        case ISYS_DROPMSG:
            ISysSendDropInfo(iMsg->imsg.drop_num);
            break;

        case ISYS_DROP_MAINT:
            ISysSendCommand(iMsg->imsg.cmd, iMsg->imsg.lineId, (BYTE *) &iMsg->imsg.drop_num, sizeof(tMaintReqMsg));
            break;

        case ISYS_PORTCHK:
            ISysSendCommand(iMsg->imsg.cmd, iMsg->imsg.lineId, NULL, NULL);
            break;

        default:
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            break;
    }

	DebugTrace(_IPC_, "Isys\tMbx\t[%s]\tOk\n",isys_msg_desc[iMsg->imsg.cmd - ISYS_MSG_START]);
}

//--------------------------------------------------------
//  RxServer
//--------------------------------------------------------

int RTFCNDCL InterSystems::RxServer(PVOID unused)
{
	unused; // RED - Added to eliminate waring for unreferenced formal parameter
    static HANDLE   hRxShm, hRxMutex, hRxSemPost, hRxSemAck;
    static PMSGSTR  pRxMsg;
    static BOOL     bInit = FALSE;
    mhdr *pmsg;

    //----- Create/Open Mutex for receiving messages from interface

    //
    // Open the required IPC objects upon first call.
    //
    // NOTE: All IPC objects are closed by the system when this client process exits.
    //
    if (!bInit)
    {
        hRxMutex = RtOpenMutex( SYNCHRONIZE, FALSE, RECV_MUTEX);

        if (hRxMutex==NULL)
        {
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hRxShm = RtOpenSharedMemory( PAGE_READWRITE, FALSE, RECV_SHM, (LPVOID*) &pRxMsg);

        if (hRxShm==NULL)
        {
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            RtCloseHandle(hRxMutex);
            return ERROR_OCCURED;
        }

        hRxSemPost = RtOpenSemaphore( SYNCHRONIZE, FALSE, RECV_SEM_POST);

        if (hRxSemPost==NULL)
        {
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            RtCloseHandle(hRxShm);
            RtCloseHandle(hRxMutex);
            return ERROR_OCCURED;
        }

        hRxSemAck = RtOpenSemaphore( SYNCHRONIZE, FALSE, RECV_SEM_ACK);
        if (hRxSemAck==NULL)
        {
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            RtCloseHandle(hRxShm);
            RtCloseHandle(hRxMutex);
            RtCloseHandle(hRxSemPost);
            return ERROR_OCCURED;
        }

        bInit = TRUE;
    }

    // indicate this thread is ready
    app->pShm->IsysLineStatus.app_threads |= RTSS_IS_RX_SVR_RDY;

#ifdef _SHOW_HANDLES_
    RtPrintf("Hnd %.2X isys RxServer\n", hIsysRx);
#endif

    for (;;)
    {
       //RtPrintf("\nRxServer, waiting on message\n");

       // check for everything ok
       if(RtWaitForSingleObject(hInterfaceThreadsOk,INFINITE)==WAIT_FAILED)
           RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
       else
       {
            if(RtWaitForSingleObject( hRxSemPost, INFINITE)==WAIT_FAILED)
                RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            else
            {
                pmsg = (mhdr*) pRxMsg->Buffer;
                //RtPrintf("RxServer received msg [%s]\n",isys_msg_desc[pmsg->cmd - ISYS_MSG_START]);
                //DumpData("pRxMsg->Buffer", (BYTE *) pRxMsg->Buffer, pRxMsg->Len);

                for (int i = 0; i < ISYS_MAXRQENTRYS; i++)
                {
                    if (rdQ[i].gen.flags == ISYS_UNUSEDQ)
                    {
                        memcpy(&rdQ[i].gen.mh,pRxMsg->Buffer, sizeof(uRx));
                        rdQ[i].gen.flags = ISYS_USEDQ;
                        //DumpData("rdQ[i]", (BYTE *) &rdQ[i], sizeof(uRQentry));
                        break;
                    }
                }

                pRxMsg->Ack = 1;

                if(!RtReleaseSemaphore( hRxSemAck, lReleaseCount, NULL))
                    RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            }
       }
    }
}

//--------------------------------------------------------
//   UpdateCommStatus
//--------------------------------------------------------

InterSystems::UpdateCommStatus(int status, int index)
{
//	RED - Two lines below removed because error_code was not referenced	
//    const int error_code[MAXIISYSLINES*2] =  {com1_fail,   com2_fail,   com3_fail,   com4_fail,
//                                              com1_normal, com2_normal, com3_normal, com4_normal};
    switch (status)
    {

//----- Set comm fail bit if fails 3 times. Right now the port# is ok, because
//      this routine is called with status = COM? only for fails

        case ISYS_BAD:

            if ( write_fail_count[index] > 0 )
                write_fail_count[index]--;
            else
            {
                // set flags for host
                app->isys_comm_fail[index] = true;
                //RtPrintf("Comm fail idx %d \n", index);
                sprintf(isys_err_buf,"Commfail localId %d to remoteId %d\n",
                                 app->this_lineid,
                                 app->pShm->sys_set.IsysLineSettings.line_ids[index]);
                app->GenError(warning, isys_err_buf);
            }
            break;

        case ISYS_GOOD:

            write_fail_count[index] = 1;
            app->isys_comm_fail[index] = false;
            //RtPrintf("Comm restored idx %d \n", index);
            sprintf(isys_err_buf,"Comm normal localId %d to remoteId %d\n",
                             app->this_lineid,
                             app->pShm->sys_set.IsysLineSettings.line_ids[index]);
            app->GenError(informational, isys_err_buf);
            break;

        default:
            RtPrintf("Error status %d file %s, line %d \n", status, _FILE_, __LINE__);
           break;
    }
    return 0;
}

//--------------------------------------------------------
//   ProcessWrites
//--------------------------------------------------------

InterSystems::ProcessWrites()
{
    static int last_tx = 0;
    int    i;
    int    rc;
    bool   wrote = false;

    for (i = 0; i < ISYS_MAXWQENTRYS;i++)
    {
//----- If active

        if (wrtQ[last_tx].gen.qh.flags & ISYS_ACTIVEQ)
        {

//----- Hasn't been sent

            if (!(wrtQ[last_tx].gen.qh.flags & ISYS_WROTEQ))
            {

//----- Write it

                if( (!trc[ISYSMAINBUFID].buffer_full) && (TraceMask & _COMBASIC_ ) )
                {
                    sprintf((char*) &tmp_trc_buf[ISYSMAINBUFID],">>> Post\t[%13s]\tlineId\t%d\tseq\t%d\n", 
                                     isys_msg_desc[wrtQ[last_tx].gen.mh.cmd - ISYS_MSG_START], 
                                     wrtQ[last_tx].gen.mh.destLineId, 
                                     wrtQ[last_tx].gen.mh.seq_num);
                    strcat((char*) &trc_buf[ISYSMAINBUFID],(char*) &tmp_trc_buf[ISYSMAINBUFID]);
                }

#ifdef _ISYS_TIMING_TEST_
				//if ( (iseq_num == 0) && (wrtQ[last_tx].gen.mh.cmd == ISYS_DROP_MAINT) )
                if ( (iseq_num == 0) && (wrtQ[last_tx].gen.mh.cmd == ISYS_PORTCHK) )
                {
                    RtGetClockTime(CLOCK_FASTEST,&StartTime);
                    iseq_num = wrtQ[last_tx].gen.mh.seq_num;
                }
#endif
                rc = postMessage((BYTE*) &wrtQ[last_tx].gen.mh);
                wrtQ[last_tx].gen.qh.flags   |= ISYS_WROTEQ;
                wrtQ[last_tx].gen.qh.cmd_tmr  = ISYS_MSGTMO;
                wrote = true;

//----- No error on write

                if (rc == NO_ERRORS)
                {

//----- If we just sent an ack or no ack needed, remove it from the queue

                   if ( (wrtQ[last_tx].gen.mh.cmd   == ISYS_ACK) ||
                       ((wrtQ[last_tx].gen.mh.flags &  ISYS_ACK_REQ) != ISYS_ACK_REQ))
                   {

                        // show it was an ack
                        if (wrtQ[last_tx].gen.mh.cmd == ISYS_ACK)
                        {
                            if( (!trc[ISYSMAINBUFID].buffer_full) && (TraceMask & _COMBASIC_ ) )
                            {
                                sprintf((char*) &tmp_trc_buf[ISYSMAINBUFID],"Send was\tack cmd\t[%13s]\tseq\t%d\n", 
                                                isys_msg_desc[wrtQ[last_tx].gen.mh.cmd-ISYS_MSG_START], 
                                                wrtQ[last_tx].gen.mh.seq_num);
                                strcat((char*) &trc_buf[ISYSMAINBUFID], (char*) &tmp_trc_buf[ISYSMAINBUFID]);
                            }
                        }
                        // show no ack required
                        else
                        {
                            if( (!trc[ISYSMAINBUFID].buffer_full) && (TraceMask & _COMBASIC_ ) )
                            {
                                sprintf((char*) &tmp_trc_buf[ISYSMAINBUFID],"No Ack\tReq cmd\t[%13s]\tseq\t%d\n", 
                                                isys_msg_desc[wrtQ[last_tx].gen.mh.cmd-ISYS_MSG_START], 
                                                wrtQ[last_tx].gen.mh.seq_num);
                                strcat((char*) &trc_buf[ISYSMAINBUFID], (char*) &tmp_trc_buf[ISYSMAINBUFID]);
                            }
                        }
                        // do cleanup
                        CleanUpWrite(last_tx, NO_ERRORS);
                   }

                }
            }

//----- Was previously sent, start timing

            else
            {
                if (wrtQ[last_tx].gen.qh.cmd_tmr > 0)
                    wrtQ[last_tx].gen.qh.cmd_tmr--;

//----- Timeout, flag a comm fail and remove message

                else
                {
#ifdef _ISYS_TIMING_TEST_
                if (iseq_num == wrtQ[last_tx].gen.mh.seq_num)
                    iseq_num = 0;
#endif
                    if (wrtQ[last_tx].gen.qh.retries > 0)
                    {
                        wrtQ[last_tx].gen.qh.retries--;
                        wrtQ[last_tx].gen.qh.flags    &= ~ISYS_WROTEQ; // will cause a new send

                        if( (!trc[ISYSMAINBUFID].buffer_full) && (TraceMask & _COMBASIC_ ) )
                        {
                            sprintf((char*) &tmp_trc_buf[ISYSMAINBUFID],"Msg retry cmd  [%13s] lineId %d seq %d file %s, line %d \n",
                                            isys_msg_desc[wrtQ[last_tx].gen.mh.cmd - ISYS_MSG_START], 
                                            wrtQ[last_tx].gen.mh.destLineId, 
                                            wrtQ[last_tx].gen.mh.seq_num,
                                            _FILE_, __LINE__);
                            strcat((char*) &trc_buf[ISYSMAINBUFID],(char*) &tmp_trc_buf[ISYSMAINBUFID]);
                        }
                        else
						{
                            RtPrintf("Msg retry cmd  [%13s] lineId %d seq %d file %s, line %d \n",
                                            isys_msg_desc[wrtQ[last_tx].gen.mh.cmd - ISYS_MSG_START], 
                                            wrtQ[last_tx].gen.mh.destLineId, 
                                            wrtQ[last_tx].gen.mh.seq_num,
                                            _FILE_, __LINE__);
						}
                    }
                    else
                    {

//----- Ok, give it up.

                        if( (!trc[ISYSMAINBUFID].buffer_full) && (TraceMask & _COMBASIC_ ) )
                        {
                            sprintf((char*) &tmp_trc_buf[ISYSMAINBUFID],"Msg timeout cmd  [%13s] lineId %d seq %d file %s, line %d \n",
                                            isys_msg_desc[wrtQ[last_tx].gen.mh.cmd - ISYS_MSG_START], 
                                            wrtQ[last_tx].gen.mh.destLineId, 
                                            wrtQ[last_tx].gen.mh.seq_num,
                                            _FILE_, __LINE__);
                            strcat((char*) &trc_buf[ISYSMAINBUFID],(char*) &tmp_trc_buf[ISYSMAINBUFID]);
                        }
                        else
                            RtPrintf("Msg timeout cmd  [%13s] lineId %d seq %d file %s, line %d \n",
                                            isys_msg_desc[wrtQ[last_tx].gen.mh.cmd - ISYS_MSG_START], 
                                            wrtQ[last_tx].gen.mh.destLineId, 
                                            wrtQ[last_tx].gen.mh.seq_num,
                                            _FILE_, __LINE__);


                        // do cleanup
                        CleanUpWrite(last_tx, ERROR_OCCURED);
                    }
                }
            }
        }

        if (++last_tx >= ISYS_MAXWQENTRYS)
            last_tx = 0;

        //if (wrote) break; // just do one at a time
    }
    return 0;
}

//--------------------------------------------------------
//   CleanUpWrite
//--------------------------------------------------------

InterSystems::CleanUpWrite(int idx, int action)
{

    switch(action)
    {
        case NO_ERRORS:

            switch(wrtQ[idx].gen.mh.cmd)
            {
                case ISYS_DROP_MAINT:
                    SendDrpManager( DROP_MAINT_RESP, wrtQ[idx].maint.msgdata.seq_num);
                    break;

                default:
                    break;

            }
            break;
                    
        case ERROR_OCCURED:

            // Update communications status
            if (app->isys_comm_fail[wrtQ[idx].gen.mh.destLineId - 1] == false)
                UpdateCommStatus(ISYS_BAD, wrtQ[idx].gen.mh.destLineId - 1);

//----- If command was ISYS_DROP_MAINT, tell drop manager to remove it

            switch(wrtQ[idx].gen.mh.cmd)
            {
                case ISYS_DROP_MAINT:
                    SendDrpManager( DROP_MAINT_FAIL, wrtQ[idx].maint.msgdata.seq_num );
                    break;

                default:
                    break;
            }
            break;

        default:
            break;
    }

    //----- Request Mutex for access to message Q

    // remove it from isys Q
    if(RtWaitForSingleObject(hWQ, WAIT50MS) != WAIT_OBJECT_0)
        RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
    else
	{
        MessageQueue(ISYSMAINBUFID, (BYTE*) &idx, ISYS_DELQ);

		if(!RtReleaseMutex(hWQ))
			RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
	}
    return 0;
}

//--------------------------------------------------------
//   ProcessRx
//--------------------------------------------------------

void InterSystems::ProcessRx(int rdq_indx)
{
    uRx*   prxmsg;  // what was received
    uResp  txmsg;   // what to send back
    uResp* packmsg; // what comes back in ack (same as txmsg, but using ptr)
    int    len, lineId;


    prxmsg = (uRx*) &rdQ[rdq_indx].gen.mh.cs;
    len    = prxmsg->gen.mh.len + sizeof(mhdr);
    lineId = prxmsg->gen.mh.srcLineId;

    // check for good length and lineId
    if ( (len > sizeof(uRx)) || (lineId < 1) || (lineId > MAXIISYSLINES) )
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        //DumpData("prxmsg",(BYTE*) prxmsg, sizeof(uRx));
        return;
    }

//----- Process messages

    if( (!trc[ISYSMAINBUFID].buffer_full) && (TraceMask & _COMBASIC_ ) )
    {
        sprintf((char*) &tmp_trc_buf[ISYSMAINBUFID],"<<< Recv Cmd\t[%13s]\tlineId\t%d\tseq\t%d\n",
                        isys_msg_desc[prxmsg->gen.mh.cmd-ISYS_MSG_START], 
                        lineId,
                        prxmsg->gen.mh.seq_num);
        strcat((char*) &trc_buf[ISYSMAINBUFID],(char*) &tmp_trc_buf[ISYSMAINBUFID]);
    }

    // process a command
    switch(prxmsg->gen.mh.cmd)
    {
        
//----- Process ACK, there could be some good stuff returned, but for now they
//      basically return the same thing.

        case ISYS_ACK:

//----- Request Mutex for access to message Q, Clear message which was acked.

            //if(RtWaitForSingleObject(hWQ, WAIT200MS) != WAIT_OBJECT_0)
            //    RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
            //else
            //    {

                packmsg = (uResp*) &prxmsg->gen.msgdata; 
            
                switch(packmsg->drp.cmd)
                {
                    case ISYS_DROPMSG:
			            if(RtWaitForSingleObject(hWQ, WAIT50MS) != WAIT_OBJECT_0)
						    RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
						else
						{
							MessageQueue( ISYSMAINBUFID, (BYTE*) &packmsg->drp.seq_num, ISYS_DELQ1 );
				            if(!RtReleaseMutex(hWQ))
								RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
						}
                        break;

                    case ISYS_DROP_MAINT:
                        // This is a response to a get batch number from the drop master
                        if (packmsg->maint.action == isys_drop_batch_req)
                        {
                            int drop_master = prxmsg->gen.mh.srcLineId;
                            int local_drop  = -1;

                            for (int ldrp = 0; ldrp < app->pShm->sys_set.NumDrops; ldrp++)
                            {
                                if (app->pShm->sys_set.IsysDropSettings[ldrp].drop[drop_master - 1] == packmsg->maint.drop_num)
                                    local_drop = ldrp;
                            }

                            if (local_drop != -1)
                            {
                                app->pShm->sys_stat.DropStatus[local_drop].batch_number = packmsg->maint.batch;
								{
									//	Chase shackles to update temp batch numbers in them
									for (int shk_idx = 0; shk_idx < app->pShm->sys_set.Shackles; shk_idx++)
									{
										for (int scl_idx = 0; scl_idx < app->pShm->scl_set.NumScales; scl_idx++)
										{
											//	If shackle status has temporary batch number and is for drop receiving this command
											if ((app->pShm->ShackleStatus[shk_idx].batch_number[scl_idx] == TEMP_BATCH_NUM) &&
												(app->pShm->ShackleStatus[shk_idx].drop[scl_idx] == packmsg->maint.drop_num))
											{
												//	Reset the temporarny batch number to the real batch number
												app->pShm->ShackleStatus[shk_idx].batch_number[scl_idx] = packmsg->maint.batch;
											}
										}
									}
								}
                                if( (!trc[ISYSMAINBUFID].buffer_full) && (TraceMask & _LABELS_) )
                                {
                                    sprintf((char*) &tmp_trc_buf[ISYSMAINBUFID],"Recv'd batch# %d for drop %d \n", packmsg->maint.batch & BATCH_MASK, local_drop + 1);
                                    strcat((char*) &trc_buf[ISYSMAINBUFID],(char*) &tmp_trc_buf[ISYSMAINBUFID] );
                                }
                            }
                            else 
                                RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
                        }
						/**********
                        #ifdef _ISYS_TIMING_TEST_
                            if (iseq_num == packmsg->ptck.seq_num)
                            {
                                RtGetClockTime(CLOCK_FASTEST,&EndTime);
		                        RtPrintf("ISYS\tresponse\ttime\t%d ms\n",
                                         (int) ((EndTime.QuadPart - StartTime.QuadPart)/10000));
                                iseq_num = 0;
                            }
                        #endif
						***********/

                        SendDrpManager( DROP_MAINT_RESP, packmsg->maint.dm_seq_num);

			            if(RtWaitForSingleObject(hWQ, WAIT50MS) != WAIT_OBJECT_0)
						    RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
						else
						{
							MessageQueue( ISYSMAINBUFID, (BYTE*) &packmsg->maint.seq_num, ISYS_DELQ1 );
				            if(!RtReleaseMutex(hWQ))
								RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
						}

                        break;

                    case ISYS_PORTCHK:

                        #ifdef _ISYS_TIMING_TEST_
                            if (iseq_num == packmsg->ptck.seq_num)
                            {
                                RtGetClockTime(CLOCK_FASTEST,&EndTime);
		                        RtPrintf("ISYS\tresponse\ttime\t%d ms\n",
                                         (int) ((EndTime.QuadPart - StartTime.QuadPart)/10000));
                                iseq_num = 0;
                            }
                        #endif

                        // Update communications status
                        if (app->isys_comm_fail[lineId - 1])
                            UpdateCommStatus(ISYS_GOOD, lineId - 1); 
						if(RtWaitForSingleObject(hWQ, WAIT50MS) != WAIT_OBJECT_0)
							RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
						else
						{
							MessageQueue( ISYSMAINBUFID, (BYTE*) &packmsg->ptck.seq_num, ISYS_DELQ1 );
				            if(!RtReleaseMutex(hWQ))
								RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
						}
                        break;

                    default:
                        RtPrintf("Error cmd %d file %s, line %d \n", packmsg->drp.cmd, _FILE_, __LINE__);
                        //DumpData("prxmsg",(BYTE*) &prxmsg, sizeof(uRx));
                        break;
                }
            //}

            //if(!RtReleaseMutex(hWQ))
            //    RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
        break;

//----- Process other messages

        case ISYS_DROPMSG:

            UpdateDropInfo((BYTE*) &prxmsg->drp, lineId); 
            if (prxmsg->drp.mh.flags & ISYS_ACK_REQ)
            {
                txmsg.drp.cmd     = prxmsg->drp.mh.cmd;
                txmsg.drp.seq_num = prxmsg->drp.mh.seq_num;
                ISysSendCommand(ISYS_ACK, lineId, (BYTE*) &txmsg, sizeof(tDrpRespMsg) );
            }

            break;

        case ISYS_PORTCHK:

            if (prxmsg->ptck.mh.flags & ISYS_ACK_REQ)
            {
                txmsg.ptck.cmd     = prxmsg->ptck.mh.cmd;
                txmsg.ptck.seq_num = prxmsg->ptck.mh.seq_num;
                ISysSendCommand(ISYS_ACK, lineId, (BYTE*) &txmsg.ptck, sizeof(tPchkRespMsg) );
            }

            break;

        case ISYS_DROP_MAINT:

            if (prxmsg->maint.mh.flags & ISYS_ACK_REQ)
            {
                txmsg.maint.cmd        = prxmsg->maint.mh.cmd;
                txmsg.maint.seq_num    = prxmsg->maint.mh.seq_num;
                txmsg.maint.dm_seq_num = prxmsg->maint.msgdata.seq_num;
                txmsg.maint.action     = prxmsg->maint.msgdata.action;
                txmsg.maint.drop_num   = prxmsg->maint.msgdata.drop_num;
                txmsg.maint.batch      = prxmsg->maint.msgdata.batch;

                if (prxmsg->maint.msgdata.action == isys_drop_batch_req)
                {
                    //	pGET_BATCH_NUMBER(prxmsg->maint.msgdata.drop_num - 1)
					UINT line_mult;
					line_mult = app->this_lineid << LINE_SHIFT;
					if (app->pShm->sys_stat.DropStatus[prxmsg->maint.msgdata.drop_num - 1].batch_number == 0)
					{
						app->pShm->sys_stat.DropStatus[prxmsg->maint.msgdata.drop_num - 1].batch_number = app->sav_drp_rec_file_info.nxt_lbl_seqnum + line_mult;
						if (HOST_OK) 
						{
							LABEL_INFO(prxmsg->maint.msgdata.drop_num - 1)
						}
						if(++app->sav_drp_rec_file_info.nxt_lbl_seqnum >= MAXBCHLBLNUM)
						   app->sav_drp_rec_file_info.nxt_lbl_seqnum = 1;
					}
					txmsg.maint.batch = app->pShm->sys_stat.DropStatus[prxmsg->maint.msgdata.drop_num - 1].batch_number;

                    if( (!trc[ISYSMAINBUFID].buffer_full) && (TraceMask & _LABELS_) )
                    {
                        sprintf((char*) &tmp_trc_buf[ISYSMAINBUFID],"Req for batch#, drop %d batch# is %u\n", 
                                  txmsg.maint.drop_num,
                                  txmsg.maint.batch  & BATCH_MASK);
                        strcat((char*) &trc_buf[ISYSMAINBUFID],(char*) &tmp_trc_buf[ISYSMAINBUFID] );
                    }
                }

                ISysSendCommand(ISYS_ACK, lineId, (BYTE*) &txmsg, sizeof(tMaintRespMsg) );
            }

            if (prxmsg->maint.msgdata.action != isys_drop_batch_req)
                drpman->ExecuteMessage(prxmsg->maint.msgdata.drop_num, 
                                       prxmsg->maint.msgdata.action,
                                       lineId,
                                       prxmsg->maint.msgdata.flags,
                                       prxmsg->maint.msgdata.batch);
 
            break;

        default:

            RtPrintf("Error Rec'vd msg %d data %d\n",
                      prxmsg->gen.mh.cmd, prxmsg->gen.mh.cmd);
            //DumpData("prxmsg",(BYTE*) &prxmsg, sizeof(uRx));
            break;

    }
}

//--------------------------------------------------------
//  SendDrpManager
//--------------------------------------------------------

int InterSystems::SendDrpManager( int cmd, int arg0)
{
    TDrpMgrIpcMsg dm_msg;
    CHAR*         pdm_msg;
    int i;

    // Due to the volume of IPC messages, these will not go to host
    DebugTrace(_IPC_, "Isys->DrpMgr\t[%s]\n", dmgr_msg[cmd - DRPMGR_MSG_START]);

    memset(&dm_msg,0,sizeof(dm_msg));

    // Build message for drop manager

    switch(cmd)
    {
        case CHECK_DROP_REMOTE:
            dm_msg.ck_drp.cmd         = cmd;
            dm_msg.ck_drp.drop_num    = arg0;
            break;

        case DROP_MAINT_RESP:
        case DROP_MAINT_FAIL:
            dm_msg.rem_msg.cmd        = cmd;
            dm_msg.rem_msg.seq_num    = arg0;
            break;

        default:
            RtPrintf("Error file %s, line %d \n", _FILE_, __LINE__);
            return ERROR_OCCURED;
            break;
    }

    //
    // Acquire the serialization mutex.  Confirm that the msg buffer is really free
    // as it may have been abandoned by another client in the middle of a post.
    //
    if(RtWaitForSingleObject(hIDrpMgrMutex, WAIT50MS) != WAIT_OBJECT_0)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    //
    // Copy the message string.
    //
    pdm_msg = (CHAR*) &dm_msg;
    for (i = 0; i < sizeof(TDrpMgrIpcMsg); i++ )
        pIDrpMgrMsg->Buffer[i] = pdm_msg[i];

    //
    // Release the Post Semaphore to the server and wait on the Acknowledge.
    // Confirm that the acknowledge is not left-over from a previous client.
    //
    // NOTE: Extremely small hole here -- if the client is terminated between
    //       clearing the acknowledge and releasing the semaphore.
    //
    pIDrpMgrMsg->Ack = 0;

    if(!RtReleaseSemaphore(hIDrpMgrSemPost, lReleaseCount, NULL))
    {
        RtReleaseMutex( hIDrpMgrMutex);
        RtPrintf("Error file %s, line %d hnd %u\n",_FILE_, __LINE__,(int) hIDrpMgrSemPost);
        return ERROR_OCCURED;
    }

    for (;;)
    {

        if(RtWaitForSingleObject( hIDrpMgrSemAck, INFINITE)==WAIT_FAILED)
        {
            RtReleaseMutex( hIDrpMgrMutex);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }
        else
        {
            if (pIDrpMgrMsg->Ack != (LONG) 0)
                break;
        }
    }

    //
    // Release the Mutex for other client's use.
    //
    if(!RtReleaseMutex( hIDrpMgrMutex))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    DebugTrace(_IPC_, "Isys->DrpMgr\t[%s]\tOk\n", dmgr_msg[cmd - DRPMGR_MSG_START]);
    return NO_ERRORS;
}

//
// Function to post a message string to the message server.
//
// NOTE: This function is structured so that any client can abort at any time, including
//       the middle of posting a message, and the messaging protocol and server will
//       continue to operate properly.
//
int InterSystems::SendLineMsg( int cmd, int line, int var, BYTE *data, int len)
{
    static HANDLE      hIsLineShm, hIsLineMutex, hIsLineSemPost, hIsLineSemAck;
    static BOOL        bIsLineInit = FALSE;
    static PAPPIPCMSG  pIsLMsg;
    TAppIpcMsg*        pIsLineMsg;
    LONG   i;

    // validate message
    if ( (cmd <= APP_MSG_START) || (cmd >= LAST_APP_MSG) )
        return ERROR_OCCURED;

    if(RtWaitForSingleObject(hInterfaceThreadsOk,WAIT50MS)==WAIT_FAILED)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    //
    // Open the required IPC objects upon first call.
    //
    // NOTE: All IPC objects are closed by the system when this client process exits.
    //
    if (!bIsLineInit)
    {
        hIsLineMutex = RtOpenMutex( SYNCHRONIZE, FALSE, SEND_MUTEX);

        if (hIsLineMutex==NULL)
        {
            RtPrintf("Error file %s, IsLine %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hIsLineShm = RtOpenSharedMemory( PAGE_READWRITE, FALSE, SEND_SHM, (LPVOID*) &pIsLMsg);

        if (hIsLineShm==NULL)
        {
            RtCloseHandle(hIsLineMutex);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hIsLineSemPost = RtOpenSemaphore( SYNCHRONIZE, FALSE, SEND_SEM_POST);

        if (hIsLineSemPost==NULL)
        {
            RtCloseHandle(hIsLineShm);
            RtCloseHandle(hIsLineMutex);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hIsLineSemAck = RtOpenSemaphore( SYNCHRONIZE, FALSE, SEND_SEM_ACK);

        if (hIsLineSemAck==NULL)
        {
            RtCloseHandle(hIsLineShm);
            RtCloseHandle(hIsLineMutex);
            RtCloseHandle(hIsLineSemPost);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        bIsLineInit = TRUE;
    }

    // Don't be recursive here. Don't use TRACE. 
    //RtPrintf("App->IsLine\t[%s]\n",app_msg[cmd - APP_MSG_START]);

    //
    // Acquire the serialization mutex.  Confirm that the msg buffer is really free
    // as it may have been abandoned by another client in the middle of a post.
    //
    if(RtWaitForSingleObject(hIsLineMutex, WAIT50MS) != WAIT_OBJECT_0)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    memset(&pIsLMsg->Buffer, 0, sizeof(mhdr) + len + 1);

    // set a pointer to fill data
    pIsLineMsg                     = (TAppIpcMsg*) &pIsLMsg->Buffer;
    pIsLineMsg->gen.hdr.cmd        = cmd;
    pIsLineMsg->gen.hdr.destLineId = line;
    pIsLineMsg->gen.hdr.srcLineId  = app->this_lineid;

    // miscellaneous stuff
    switch(cmd)
    {
        case ERROR_MSG:

            // copy data
            for (i = 0; i < len; i++ )
                pIsLineMsg->errMsg.text[i] = data[i];

            // make sure it is null terminated
            pIsLineMsg->errMsg.text[len] = '\0';
            len++;

            pIsLineMsg->errMsg.severity    = var;
            len += sizeof(var);          // add in the severity length

            pIsLineMsg->errMsg.hdr.len     = len;
            break;

        case DEBUG_MSG:

            // copy data
            for (i = 0; i < len; i++ )
                pIsLineMsg->gen.data[i] = data[i];

            // make sure it is null terminated
            pIsLineMsg->gen.data[len] = '\0';
            len++;

            pIsLineMsg->gen.hdr.len     = len;

            // set drop rec recovery bit or other flag bit
            pIsLineMsg->gen.hdr.flags   = var;
            break;

        default:
            RtPrintf("Error file %s, line %d \n", _FILE_, __LINE__);
            RtReleaseMutex( hIsLineMutex);
            return ERROR_OCCURED;
            break;
    }

    // tell net client where to send and how much
    pIsLMsg->LineId = line;
    pIsLMsg->Len    = sizeof(mhdr) + len; // add header length

    //
    // Release the Post Semaphore to the server and wait on the Acknowledge.
    // Confirm that the acknowledge is not left-over from a previous client.
    //
    // NOTE: Extremely small hole here -- if the client is terminated between
    //       clearing the acknowledge and releasing the semaphore.
    //
    pIsLMsg->Ack = 0;

    if(!RtReleaseSemaphore(hIsLineSemPost, lReleaseCount, NULL))
    {
        RtReleaseMutex( hIsLineMutex );
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    for (;;)
    {
        if(RtWaitForSingleObject( hIsLineSemAck, INFINITE)==WAIT_FAILED)
        {
            RtReleaseMutex( hIsLineMutex );
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        if (pIsLMsg->Ack != (LONG) 0)
            break;
    }

    //
    // Release the Mutex for other client's use.
    //
    if(!RtReleaseMutex( hIsLineMutex ))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
//  SendHostMsg
//--------------------------------------------------------

int InterSystems::SendHostMsg( int cmd, int var, BYTE *data, int len)
{
    static HANDLE      hShShm, hShMutex, hShSemPost, hShSemAck;
    static BOOL        bShInit = FALSE;
    static PAPPIPCMSG  pMsg;
    TAppIpcMsg*  pappMsg;
    int i;

    // validate message
    if ( (cmd <= APP_MSG_START) || (cmd >= LAST_APP_MSG) )
        return ERROR_OCCURED;

    if(RtWaitForSingleObject(hInterfaceThreadsOk,WAIT50MS)==WAIT_FAILED)
        return ERROR_OCCURED;

    //
    // Open the required IPC objects upon first call.
    //
    // NOTE: All IPC objects are closed by the system when this client process exits.
    //
    if (!bShInit)
    {
        hShMutex = RtOpenMutex( SYNCHRONIZE, FALSE, SEND_HST_MUTEX);

        if (hShMutex==NULL)
        {
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hShShm = RtOpenSharedMemory( PAGE_READWRITE, FALSE, SEND_HST_SHM, (LPVOID*) &pMsg);

        if (hShShm==NULL)
        {
            RtCloseHandle(hShMutex);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hShSemPost = RtOpenSemaphore( SYNCHRONIZE, FALSE, SEND_HST_SEM_POST);

        if (hShSemPost==NULL)
        {
            RtCloseHandle(hShShm);
            RtCloseHandle(hShMutex);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hShSemAck = RtOpenSemaphore( SYNCHRONIZE, FALSE, SEND_HST_SEM_ACK);

        if (hShSemAck==NULL)
        {
            RtCloseHandle(hShShm);
            RtCloseHandle(hShMutex);
            RtCloseHandle(hShSemPost);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        bShInit = TRUE;
    }

    //
    // Acquire the serialization mutex.  Confirm that the msg buffer is really free
    // as it may have been abandoned by another client in the middle of a post.
    //
    if(RtWaitForSingleObject(hShMutex, WAIT50MS) != WAIT_OBJECT_0)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    // Don't be recursive here. Don't use TRACE. 
    //RtPrintf("Isys->Hst\t[%s]\n",app_msg[cmd - APP_MSG_START]);

    memset(&pMsg->Buffer, 0, sizeof(mhdr) + len + 1);

    // set a pointer to fill data
    pappMsg                     = (TAppIpcMsg*) &pMsg->Buffer;
    pappMsg->gen.hdr.cmd        = cmd;
    pappMsg->gen.hdr.destLineId = HOST_ID;
    pappMsg->gen.hdr.srcLineId  = app->this_lineid;

    // miscellaneous stuff
    switch(cmd)
    {
        case ERROR_MSG:

            // copy data
            for (i = 0; i < len+1; i++ )
                pappMsg->errMsg.text[i] = data[i];

            // make sure it is null terminated
            pappMsg->errMsg.text[len]   = '\0';
            len++;

            pappMsg->errMsg.severity = var;
            len += sizeof(var);             // add in the severity

            pappMsg->errMsg.hdr.len  = len;
            break;

        case DEBUG_MSG:

            for (i = 0; i < len; i++ )
                pappMsg->gen.data[i] = data[i];

            // make sure it is null terminated
            pappMsg->gen.data[len]   = '\0';
            len++;

            pappMsg->gen.hdr.len = len;
            break;

        default:
            RtPrintf("Error file %s, line %d \n", _FILE_, __LINE__);
            return ERROR_OCCURED;
            break;
    }

    // tell net client where to send and how much
    pMsg->LineId = HOST_ID;
    pMsg->Len    = sizeof(mhdr) + len; // add header length

    //
    // Release the Post Semaphore to the server and wait on the Acknowledge.
    // Confirm that the acknowledge is not left-over from a previous client.
    //
    // NOTE: Extremely small hole here -- if the client is terminated between
    //       clearing the acknowledge and releasing the semaphore.
    //
    pMsg->Ack = 0;

    if(!RtReleaseSemaphore(hShSemPost, lReleaseCount, NULL))
    {
        RtReleaseMutex( hShMutex);
        RtPrintf("Error file %s, line %d hnd %u\n",_FILE_, __LINE__,(int) hShSemPost);
        return ERROR_OCCURED;
    }

    for (;;)
    {

        if(RtWaitForSingleObject( hShSemAck, INFINITE)==WAIT_FAILED)
        {
            RtReleaseMutex( hShMutex);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        if (pMsg->Ack != (LONG) 0)
            break;
    }

    //
    // Release the Mutex for other client's use.
    //
    if(!RtReleaseMutex( hShMutex))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
//  MessageQueue
// 
//  Add/remove by Q pointer
//--------------------------------------------------------

int InterSystems::MessageQueue(int trcbuf, BYTE* msg, int cmd)
{
    int i;
    static UINT wrt_seq_num = 1;
    UINT*  seqNum;

    for (i = 0; i < ISYS_MAXWQENTRYS;i++)
    {
        //RtPrintf("Q i %d %x \n", i, wrtQ[i].flags);

        switch(cmd)
        {
            case ISYS_ADDQ:

                // Add to message queue
                if (wrtQ[i].gen.qh.flags == ISYS_UNUSEDQ)
                {
                    memcpy(&wrtQ[i], msg, sizeof(uWQentry));
                    wrtQ[i].gen.qh.flags |= ISYS_USEDQ;

                    if (wrt_seq_num >= 0xfffffff0)
                        wrt_seq_num = 1;

                    wrtQ[i].gen.mh.seq_num = wrt_seq_num++;
                    wrtQ[i].gen.mh.cs      = 0;                 // not used anymore
                    wrtQ[i].gen.qh.flags   |= ISYS_ACTIVEQ;
                    //DumpData("wrtQ[i].gen.mh",(BYTE*) &wrtQ[i].gen.mh, sizeof(uRx));

                    if( (!trc[trcbuf].buffer_full) && (TraceMask & _COMQUEUE_ ) )
                    {
                        sprintf((char*) &tmp_trc_buf[trcbuf],"WQ add 1\ti\t%d\tseq\t%d\tcmd [%13s]\tlineId\t%d\tidata[0]\t%d\tcs\t%d\n",
                                       i, wrtQ[i].gen.mh.seq_num, 
                                       isys_msg_desc[wrtQ[i].gen.mh.cmd-ISYS_MSG_START], 
                                       wrtQ[i].gen.mh.destLineId, 
                                       wrtQ[i].gen.msgdata.idata1, wrtQ[i].gen.mh.cs);
                        strcat((char*) &trc_buf[trcbuf],(char*) &tmp_trc_buf[trcbuf]);
                    }

                    return NO_ERRORS;
                }
            break;

            case ISYS_DELQ:

                // Remove from message queue by index
                if (i == (int) *msg)
                {
                    if( (!trc[trcbuf].buffer_full) && (TraceMask & _COMQUEUE_ ) )
                    {
                        sprintf((char*) &tmp_trc_buf[trcbuf],"WQ remove 0\ti\t%d\tseq\t%d\n", i, wrtQ[i].gen.mh.seq_num);
                        strcat((char*) &trc_buf[trcbuf],(char*) &tmp_trc_buf[trcbuf]);
                    }
                    wrtQ[i].gen.qh.flags = ISYS_UNUSEDQ;
                    return NO_ERRORS;
                }
            break;

            case ISYS_DELQ1:

                seqNum = (UINT*) msg;

                // Remove from message queue by sequence number
                if (wrtQ[i].gen.mh.seq_num == (UINT) *seqNum)
                {
                    if( (!trc[trcbuf].buffer_full) && (TraceMask & _COMQUEUE_ ) )
                    {
                        sprintf((char*) &tmp_trc_buf[trcbuf],"WQ remove 1\ti\t%d\tseq\t%d \n", i, wrtQ[i].gen.mh.seq_num);
                        strcat((char*) &trc_buf[trcbuf],(char*) &tmp_trc_buf[trcbuf]);
                    }
                    wrtQ[i].gen.qh.flags = ISYS_UNUSEDQ;
                    return NO_ERRORS;
                }
            break;

        default:
            RtPrintf("Error cmd %d var %d file %s, line %d \n", cmd, (UINT) *msg, _FILE_, __LINE__);
            return ERROR_OCCURED;
            break;
        }
    }
    

    RtPrintf("Error cmd %d var %d file %s, line %d \n", cmd, (UINT) *msg, _FILE_, __LINE__);
    return ERROR_OCCURED;
}


//--------------------------------------------------------
//  ISysSendIsysDropInfo
//
//  When a drop occurs, this routine is called to update
//  the other lines. The routine will send to all lines if
//  it is the master.
//--------------------------------------------------------

InterSystems::ISysSendDropInfo(int drop_no)
{
    int i, drop_minus1;
    static uWQentry q;

    
    drop_minus1 = drop_no - 1;

    memset(&q, 0, sizeof(uWQentry));

//----- Build message to send other line

    q.drp.mh.cmd        = ISYS_DROPMSG;
    q.drp.mh.len        = uRxLen[ISYS_DROPMSG - ISYS_MSG_START];
    q.drp.qh.retries    = 1;
    //q.drp.mh.flags     |= ISYS_ACK_REQ; // ack required

//----- Or in some flag bits to show how its going with the drop

    q.drp.msgdata.flags = 0;

    if (app->pShm->sys_stat.DropStatus[drop_minus1].Suspended)
        q.drp.msgdata.flags |= ISYS_DROP_SUSPENDED;
    if (app->pShm->sys_stat.DropStatus[drop_minus1].Batched)
        q.drp.msgdata.flags |= ISYS_DROP_BATCHED;


//----- Schedule status info

    q.drp.msgdata.PPMCount     = app->pShm->sys_stat.ScheduleStatus[drop_minus1].PPMCount;
    q.drp.msgdata.PPMPrevious  = app->pShm->sys_stat.ScheduleStatus[drop_minus1].PPMPrevious;
    q.drp.msgdata.BCount       = app->pShm->sys_stat.ScheduleStatus[drop_minus1].BchActCnt;
    q.drp.msgdata.BWeight      = app->pShm->sys_stat.ScheduleStatus[drop_minus1].BchActWt;
    q.drp.msgdata.batch_number = app->pShm->sys_stat.DropStatus    [drop_minus1].batch_number;

    if( (!trc[ISYSMBXBUFID].buffer_full) && (TraceMask & _ISCOMS_ ) )
    {
        sprintf((char*) &tmp_trc_buf[ISYSMBXBUFID],">>> SndDrpInf\tdrop\t%d\tBcnt\t%u\tBwt\t%u\tBPMCnt\t%u\tBPMPrv\t%u\n", 
                      drop_no,
                      (int) q.drp.msgdata.BCount, 
                      (int) q.drp.msgdata.BWeight, 
                      q.drp.msgdata.PPMCount,
                      q.drp.msgdata.PPMPrevious);
        strcat( (char*) &trc_buf[ISYSMBXBUFID], (char*) &tmp_trc_buf[ISYSMBXBUFID] );
    }

    for (i = 0; i < MAXIISYSLINES; i++)
    {
        if (i != app->this_lineid - 1)
        {
            if ( app->pShm->sys_set.IsysDropSettings[drop_minus1].active[i] &&
                (app->pShm->sys_set.IsysLineSettings.line_ids[i] > 0 )   &&
                 app->pShm->sys_set.IsysLineSettings.active[i]           &&
                 app->pShm->IsysLineStatus.connected[i]                  &&
                (!(app->isys_comm_fail[i])) )
            {

				// Only pass this flag if line is the master. Don't want slaves passing
				// this info back to the master line.
				if( app->pShm->sys_set.IsysLineSettings.isys_master )
				{
					if (app->pShm->IsysDropStatus[drop_minus1].flags[i] & ISYS_DROP_OVERRIDE ) //RCB 8-10
					{
						//DebugTrace(_DEBUG_, "Sending OVERRIDE flag to line %d from line %d\n",
						//	q.drp.mh.destLineId,
						//	app->this_lineid - 1);
						q.drp.msgdata.flags |= ISYS_DROP_OVERRIDE;
					}
				}

				q.drp.mh.destLineId    = app->pShm->sys_set.IsysLineSettings.line_ids[i];
				q.drp.mh.srcLineId     = app->this_lineid;
				q.drp.msgdata.drop_num = app->pShm->sys_set.IsysDropSettings[drop_minus1].drop[i];

				if(RtWaitForSingleObject(hWQ, WAIT50MS) != WAIT_OBJECT_0)
                //if(RtWaitForSingleObject(hWQ, WAIT200MS) != WAIT_OBJECT_0)
                    RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                else
                {

                    MessageQueue( ISYSMBXBUFID, (BYTE*) &q, ISYS_ADDQ);

                    if( (!trc[ISYSMBXBUFID].buffer_full) && (TraceMask & _ISCOMS_ ) )
                    {
                        sprintf((char*) &tmp_trc_buf[ISYSMBXBUFID],">>>\t\tIdrop\t%d\tlineId\t%d\n", 
                                      q.drp.msgdata.drop_num, 
                                      q.drp.mh.destLineId);
                        strcat( (char*) &trc_buf[ISYSMBXBUFID], (char*) &tmp_trc_buf[ISYSMBXBUFID] );
                    }
                }

                if(!RtReleaseMutex(hWQ))
                    RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
            }
        }
    }
    return 0;
}

//--------------------------------------------------------
//  ISysSendCommand
//--------------------------------------------------------

int InterSystems::ISysSendCommand(int cmd, int lineId, BYTE *data, int len)
{
    int rc = NO_ERROR;
    static uWQentry q;

    memset(&q, 0, sizeof(q));

    // Build message to send other line
    q.drp.mh.srcLineId  = app->this_lineid;
    q.drp.mh.destLineId = lineId;
    q.drp.mh.cmd        = cmd;
    q.drp.mh.len        = uRxLen[cmd - ISYS_MSG_START];
    q.drp.qh.retries    = 1;
    q.drp.mh.flags     |= ISYS_ACK_REQ; // ack required


    switch(cmd)
    {
        case ISYS_ACK:
            if ( (len != NULL)       && 
                 (len <= sizeof(uWQentry)) &&
                 (data != NULL) )
                memcpy(&q.maint.msgdata, data, len);

            q.drp.qh.retries = 0;
            q.drp.mh.flags   = 0; // no ack required

            break;

        case ISYS_DROP_MAINT:
            
            if ( (len != NULL)       && 
                 (len <= sizeof(uWQentry)) &&
                 (data != NULL) )
                memcpy(&q.maint.msgdata, data, len);
//            {
//               tMaintReqMsg* mrq = (tMaintReqMsg*) &q.maint.msgdata;
//                if (mrq->action == isys_drop_batch_req)
//                    RtPrintf("Isend req batch line %d drop %d\n",q.maint.mh.destLineId, mrq->drop_num);
//            }

            break;

        case ISYS_PORTCHK:
            break;

        default:
            RtPrintf("Error cmd %d file %s, line %d \n", cmd, _FILE_, __LINE__);
            return ERROR_OCCURED;
            break;
    }

   if(RtWaitForSingleObject(hWQ, WAIT50MS) != WAIT_OBJECT_0)
        RtPrintf("Error file %s, line %d err %u\n",_FILE_, __LINE__, GetLastError());
   else
   {      
        if (MessageQueue(ISYSMBXBUFID, (BYTE*) &q, ISYS_ADDQ) == ERROR_OCCURED)
            rc = ERROR_OCCURED;
   }

   if(!RtReleaseMutex(hWQ))
        RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);

   if (cmd == ISYS_DROP_MAINT)
   {
        if( (!trc[ISYSMBXBUFID].buffer_full) && (TraceMask & _ISCOMS_ ) )
        {
            sprintf((char*) &tmp_trc_buf[ISYSMBXBUFID],">>> SndCmd\t[%13s]\tlineId\t%d\tdrop\t%d\taction\t[%13s] >>>\n", 
                  isys_msg_desc[cmd-ISYS_MSG_START], 
                  lineId, 
                  q.maint.msgdata.drop_num, 
                  drp_state_desc[q.maint.msgdata.action]);
            strcat( (char*) &trc_buf[ISYSMBXBUFID], (char*) &tmp_trc_buf[ISYSMBXBUFID] );
        }
   }
   else
   {
        if( (!trc[ISYSMBXBUFID].buffer_full) && (TraceMask & _ISCOMS_ ) )
        {
            sprintf((char*) &tmp_trc_buf[ISYSMBXBUFID],">>> SndCmd\t[%13s]\tlineId\t%d\n", isys_msg_desc[cmd-ISYS_MSG_START], lineId);
            strcat( (char*) &trc_buf[ISYSMBXBUFID], (char*) &tmp_trc_buf[ISYSMBXBUFID] );
        }
   }

    return rc;
}

//--------------------------------------------------------
//  UpdateIsysDropInfo
//--------------------------------------------------------

void InterSystems::UpdateDropInfo(BYTE* drp_msg, int lineId)
{
    int  drop_minus1;
    int  lineid_minus1;
    rx_drp_msg* rx;

    rx            = (rx_drp_msg*) drp_msg;
    drop_minus1   = rx->msgdata.drop_num - 1;
    lineid_minus1 = lineId - 1;

    if ( (rx->msgdata.drop_num <= 0) && 
         (rx->msgdata.drop_num > MAXIISYSLINES) )
    {
        RtPrintf("Error lineId %d drop %d file %s, line %d \n",
                        lineId, rx->msgdata.drop_num,
                        _FILE_, __LINE__);
        return;
    }

//----- Update the remote drops data
    
    // Clear old flags and or with the new
    app->pShm->IsysDropStatus[drop_minus1].flags       [lineid_minus1] &= (ISYS_LOCAL_MASK|ISYS_DROP_OVERRIDE);

	// Don't allow the slave line to clear flags on the master line - RCB 
	if (ISYS_ENABLE && ISYS_MASTER) 
	{
		if ((app->pShm->IsysDropStatus[drop_minus1].flags[lineid_minus1] & ISYS_DROP_OVERRIDE) == 1)
		{
			app->pShm->IsysDropStatus[drop_minus1].flags       [lineid_minus1] |= rx->msgdata.flags;
			app->pShm->IsysDropStatus[drop_minus1].flags       [lineid_minus1] |= ISYS_DROP_OVERRIDE;
		}
		else
		{
			app->pShm->IsysDropStatus[drop_minus1].flags       [lineid_minus1] |= rx->msgdata.flags;
		}
	}
	else
	{
		app->pShm->IsysDropStatus[drop_minus1].flags       [lineid_minus1] |= rx->msgdata.flags;
	}
	//DebugTrace(_DEBUG_, "UpdateDropInfo:drop %d flag 0x%x line %d\n", drop_minus1 + 1, rx->msgdata.flags, lineid_minus1 +1);

    app->pShm->IsysDropStatus[drop_minus1].PPMCount    [lineid_minus1] = rx->msgdata.PPMCount;
    app->pShm->IsysDropStatus[drop_minus1].PPMPrevious [lineid_minus1] = rx->msgdata.PPMPrevious;
    app->pShm->IsysDropStatus[drop_minus1].BCount      [lineid_minus1] = rx->msgdata.BCount;
    app->pShm->IsysDropStatus[drop_minus1].BWeight     [lineid_minus1] = rx->msgdata.BWeight;
    app->pShm->IsysDropStatus[drop_minus1].batch_number[lineid_minus1] = rx->msgdata.batch_number;

    if( (!trc[ISYSMAINBUFID].buffer_full) && (TraceMask & _ISCOMS_ ) )
    {
        sprintf((char*) &tmp_trc_buf[ISYSMAINBUFID],"<<< UpdDrp\tdrop\t%u\tlineId\t%u\tflgs\t0x%x\tBCnt\t%u\tBPMCnt\t%u\tBWt\t%u\n",
                rx->msgdata.drop_num, lineId,
                      app->pShm->IsysDropStatus[drop_minus1].flags[lineid_minus1],
                (int) app->pShm->IsysDropStatus[drop_minus1].BCount  [lineid_minus1],
                      app->pShm->IsysDropStatus[drop_minus1].PPMCount[lineid_minus1],
                (int) app->pShm->IsysDropStatus[drop_minus1].BWeight [lineid_minus1]);
        strcat((char*) &trc_buf[ISYSMAINBUFID],(char*) &tmp_trc_buf[ISYSMAINBUFID]);
    }

//----- Check to see if any totals have been exceeded
    
     SendDrpManager( CHECK_DROP_REMOTE, drop_minus1 /* -1 for CHECK_DROP_REMOTE */ );
}

//
// Function to post a message string to the message server.
//
// NOTE: This function is structured so that any client can abort at any time, including
//       the middle of posting a message, and the messaging protocol and server will
//       continue to operate properly.
//
int InterSystems::postMessage(BYTE* Msg)
{
    static HANDLE   hPmShm, hPmMutex, hPmSemPost, hPmSemAck;
    static BOOL     bPmInit = FALSE;
    static PMSGSTR  pMsg;
    LONG   i;
    uRx*   pTx;


    //
    // Open the required IPC objects upon first call.
    //
    // NOTE: All IPC objects are closed by the system when this client process exits.
    //
    if (!bPmInit)
    {
        hPmMutex = RtOpenMutex( SYNCHRONIZE, FALSE, SEND_MUTEX);

        if (hPmMutex==NULL)
        {
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hPmShm = RtOpenSharedMemory( PAGE_READWRITE, FALSE, SEND_SHM, (LPVOID*) &pMsg);

        if (hPmShm==NULL)
        {
            RtCloseHandle(hPmMutex);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hPmSemPost = RtOpenSemaphore( SYNCHRONIZE, FALSE, SEND_SEM_POST);

        if (hPmSemPost==NULL)
        {
            RtCloseHandle(hPmShm);
            RtCloseHandle(hPmMutex);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hPmSemAck = RtOpenSemaphore( SYNCHRONIZE, FALSE, SEND_SEM_ACK);

        if (hPmSemAck==NULL)
        {
            RtCloseHandle(hPmShm);
            RtCloseHandle(hPmMutex);
            RtCloseHandle(hPmSemPost);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        bPmInit = TRUE;
    }

    //
    // Acquire the serialization mutex.  Confirm that the msg buffer is really free
    // as it may have been abandoned by another client in the middle of a post.
    //
    if(RtWaitForSingleObject(hPmMutex, WAIT50MS) != WAIT_OBJECT_0)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    // Just copy sizeof largest rx
    pTx          = (uRx*) Msg; 
    pMsg->LineId = pTx->gen.mh.destLineId;
    pMsg->Len    = uRxLen[pTx->gen.mh.cmd - ISYS_MSG_START] + sizeof(mhdr);
        
    //RtPrintf("PostMessage: len %d\n", pMsg->Len);
    //DumpData("Msg",(BYTE*) Msg, pMsg->Len);

    for (i = 0; i < pMsg->Len;i++ )
        pMsg->Buffer[i] = Msg[i];

    //
    // Release the Post Semaphore to the server and wait on the Acknowledge.
    // Confirm that the acknowledge is not left-over from a previous client.
    //
    // NOTE: Extremely small hole here -- if the client is terminated between
    //       clearing the acknowledge and releasing the semaphore.
    //
    pMsg->Ack = 0;

    if(!RtReleaseSemaphore(hPmSemPost, lReleaseCount, NULL))
    {
        RtReleaseMutex( hPmMutex );
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    for (;;)
    {
        if(RtWaitForSingleObject( hPmSemAck, INFINITE)==WAIT_FAILED)
        {
            RtReleaseMutex( hPmMutex );
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        if (pMsg->Ack != (LONG) 0)
            break;
    }

    //
    // Release the Mutex for other client's use.
    //
    if(!RtReleaseMutex( hPmMutex ))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    return NO_ERRORS;
}
