// DropManager.cpp: implementation of the DropManager class.
//
//////////////////////////////////////////////////////////////////////

#include "types.h"

#undef  _FILE_
#define _FILE_      "DropManager.cpp"

//
// Local data.
//

TDrpMgrMsgQEntry    DrpCtlWrtQ [ISYS_MAXWQENTRYS];
TIsysBatchState		IsysBatchState[MAXDROPS];

PIPCMSG pDrpMgrMsg;
// intersystems mailbox
HANDLE              hDmIsysShm, hDmIsysMutex,  hDmIsysSemPost,  hDmIsysSemAck;
PIPCMSG             pDmIsysMsg;

LONG                ldmInitialCount   = 0;
LONG                ldmMaximumCount   = 1;
LONG                ldmReleaseCount   = 1;

char                dm_dt_str [MAXDATETIMESTR];     // place to build a date/time string
char                dm_err_buf[MAXERRMBUFSIZE];

//////////////////////////////////////////////////////////////////////
// Local Defines
//////////////////////////////////////////////////////////////////////
#define		NODATA			-1
#define		_MAXSTALLCOUNT_	1000
#define		_STALLCOUNT_DEFAULT_ (10 * 2)			// 10 seconds

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

DropManager::DropManager()
{
    initialize();
}

DropManager::~DropManager()
{

}

LONG __stdcall DropManager::dmUnhandledExceptionFilter(EXCEPTION_POINTERS* pExPtrs)
{
    RtPrintf("*** Drop Manager Exception! ***\nExpCode:\t0x%8.8X\nExpFlags:\t%d\nExpAddress:\t0x%8.8X\n",
      pExPtrs->ExceptionRecord->ExceptionCode,
      pExPtrs->ExceptionRecord->ExceptionFlags,
      pExPtrs->ExceptionRecord->ExceptionAddress);

    EXCEPTION_SHUTDOWN

   return EXCEPTION_CONTINUE_SEARCH;
}

//--------------------------------------------------------
//   initialize
//--------------------------------------------------------

void DropManager::initialize()
{
	int Index = 0;

   if ( OpenIsysMailbox() != NO_ERRORS )
   {
      sprintf(dm_err_buf,"Can't open mailbox %s line %d\n", _FILE_, __LINE__);
      app->GenError(critical, dm_err_buf);
      return;
   }

   if ( CreateDropManagerMbxThread() != NO_ERRORS )
   {
      sprintf(dm_err_buf,"Can't create thread %s line %d\n", _FILE_, __LINE__);
      app->GenError(critical, dm_err_buf);
      return;
   }

//----- Create/Open Mutex for trace buffer

    trc[DMMBXBUFID].mutex = RtCreateMutex( NULL, FALSE, "dmgr_main_trace");

    if ( trc[DMMBXBUFID].mutex == NULL )
    {
        sprintf(dm_err_buf,"Can't create mutex %s line %d\n", _FILE_, __LINE__);
        app->GenError(critical, dm_err_buf);
    }

	
	// Start the drop manager watchdog
    if ((hDropTimer = Create_Drop_Manager_Timer()) == NULL)
    {
        sprintf(dm_err_buf,"Can't create DropMgr timer %s line %d\n", _FILE_, __LINE__);
        app->GenError(warning, dm_err_buf);
    }

	// Initialize the BatchState records
	for (Index = 0; Index < MAXDROPS; Index++)
	{
		IsysBatchState[Index].TotalBatchCount = NODATA;
		IsysBatchState[Index].LineIndex = NODATA;
		IsysBatchState[Index].TotalBatchWeight = NODATA;
		IsysBatchState[Index].StallCount = 0;
	}


}

//--------------------------------------------------------
//   Drop_Manager_Timer - 
//--------------------------------------------------------

void __stdcall DropManager::Drop_Manager_Timer (PVOID addr)
{
	addr;	// RED - Added to eliminate waring for unreferenced formal parameter
    int      DropIndex			= 0;
	int		 Batched			= 0;
	int		 Suspended			= 0;
	int		 DropCount			= 0;
	int		 NotBatchedIndex	= 0;
	int		 LineIndex			= 0;
//   int      TotalPPMCount		= 0;		// RED - Removed because it was initializaed but not referenced
//    int      TotalBchActCnt		= 0;		// RED - Removed because it was initializaed but not referenced
//	bool	 bNotLoopMode		= false;		// RED - Removed because it was initializaed but not referenced
	static  TDrpMgrMsgQEntry	qmsg;	// Message to send to individual lines
	int		drop_plus1;
	int		LineUpdate = 0;	// RED - Initialized to avoid warning of possible use without initialization
	int		StallCount			= _STALLCOUNT_DEFAULT_;
    
	//----- Check if this line is master and if any drops batch mode that are stuck
    if (ISYS_ENABLE && ISYS_MASTER) 
	{
		if ((app->pShm->sys_set.OverRideDelay > 0) && 
			(app->pShm->sys_set.OverRideDelay <= _MAXSTALLCOUNT_))
		{
			// We are using a .5 sec timer so multiply by 2 to equate to seconds
			// from the frontend interface.
			StallCount = app->pShm->sys_set.OverRideDelay * 2; 
		}
		else
		{
			StallCount = _STALLCOUNT_DEFAULT_;
		}
		// Now find a drop that is intersystem
		for (DropIndex = 0; DropIndex < app->pShm->sys_set.NumDrops; DropIndex++)
		{
			DropCount = 0;
			Batched = 0;
			Suspended = 0;

			// Is this an ISYS Drop?
			if ((app->pShm->sys_set.IsysLineSettings.active[app->this_lineid-1] &&
				app->pShm->sys_set.IsysDropSettings[DropIndex].active[app->this_lineid-1])
				)
			{
				// Only interested in Batch modes if ISYS
				switch (app->pShm->Schedule[DropIndex].DistMode)
				{

				case mode_3_batch:
				case mode_5_batch_rate:
				case mode_6_batch_alt_rate:
				case mode_7_batch_alt_rate:
			
					// Get the total number of batched drops for this ISYS grouping
					for (LineIndex = 0; LineIndex < MAXIISYSLINES; LineIndex++)
					{
						if ( app->pShm->sys_set.IsysLineSettings.active[LineIndex] &&
							app->pShm->sys_set.IsysDropSettings[DropIndex].active[LineIndex] )
						{
							// Get the number of lines dropping here
							DropCount++;

							// Check to see if line is Suspended
							if ((app->pShm->IsysDropStatus[DropIndex].flags[LineIndex] & ISYS_DROP_SUSPENDED))
							{
								Suspended++;
							}
							else
							{
								// Now check if the line has batched out for this drop
								if ( (app->pShm->IsysDropStatus[DropIndex].flags[LineIndex] & ISYS_DROP_BATCHED) != 0)
								{
									Batched++;
								}
								else
								{
									// Only interested in the case were one line is holding up the drop
									NotBatchedIndex = LineIndex;
								}
							}
						}
					}

					// If one drop in this ISYS grouping is not batched, 
					// check for a stalled condition
					// GLC 12/7/04 added (DropCount > 1) to avoid running this code if only a single drop is active
					// RED 04-30-2010 added bypass of this logic if there is a suspended drop in the mix of drops
					if ( (Suspended == 0) && (DropCount > 1) && (Batched == (DropCount - 1)) ) 
					{

						// Check if the stalled condition was already determined and skip
						// to the next drop
						if (IsysBatchState[DropIndex].StallCount >= StallCount)
						{
							DebugTrace(_DEBUG_, "Already in over-ride condition for drop %d\n", DropIndex + 1);
							break; 
						}

						// If the batch count/weight is not changing over time, the drop is stalled
						if ((IsysBatchState[DropIndex].LineIndex == NotBatchedIndex) &&
							((app->pShm->Schedule[DropIndex].BchCntSp > 0) &&
							(IsysBatchState[DropIndex].TotalBatchCount == 
							(int)app->pShm->IsysDropStatus[DropIndex].BCount[NotBatchedIndex])) ||
							((app->pShm->Schedule[DropIndex].BchWtSp > 0) &&
							(IsysBatchState[DropIndex].TotalBatchWeight == 
							app->pShm->IsysDropStatus[DropIndex].BWeight[NotBatchedIndex]))
							)
						{

							IsysBatchState[DropIndex].StallCount++;

							// if the count reaches ? we have been stalled long enough, open up
							// one of the batched drops
							if (IsysBatchState[DropIndex].StallCount >= StallCount) // Try 2 seconds
							{

								//RtPrintf("Drop_Manager_Timer: Line %d"
								//				" Drop %d BatchCnt %d BatchWt %el StallCnt %d\n",
								//		IsysBatchState[DropIndex].LineIndex + 1, DropIndex + 1, 
								//		IsysBatchState[DropIndex].TotalBatchCount,
								//		IsysBatchState[DropIndex].TotalBatchWeight,
								//		IsysBatchState[DropIndex].StallCount);

								drop_plus1 = DropIndex + 1;

								// Find
								for (int LineIndex = 0; LineIndex < MAXIISYSLINES; LineIndex++)
								{   
									if ( app->pShm->sys_set.IsysDropSettings[DropIndex].active[LineIndex] )
									{
										if ( (app->pShm->IsysDropStatus[DropIndex].flags[LineIndex] & ISYS_DROP_BATCHED) != 0)
										{
											LineUpdate = LineIndex;
											break;
										}
									}
								}

								// Unbatch the batched line and set is override flag to ignore cutoffs
								app->pShm->IsysDropStatus[DropIndex].flags[LineUpdate] &= ~ISYS_DROP_BATCHED;
								app->pShm->IsysDropStatus[DropIndex].flags[LineUpdate] |= ISYS_DROP_OVERRIDE;

								//	Set this line as the new fastest
								app->isys_fastest_idx[DropIndex] = LineUpdate;

								// If override line is me, unbatch myself
								if (LineUpdate == (app->this_lineid - 1))
								{
									app->pShm->sys_stat.DropStatus[DropIndex].Batched  = 0;
								}
								else
								{
									//	Send message to line to set its flags and unbatch itself
									qmsg.cmd    = ISYS_DROP_MAINT;
									qmsg.action = isys_drop_set_flags;
									qmsg.drop_num = app->pShm->sys_set.IsysDropSettings[DropIndex].drop[LineUpdate]; 
									qmsg.lineId   = app->pShm->sys_set.IsysLineSettings.line_ids[LineUpdate];
									qmsg.flags1   = app->pShm->IsysDropStatus[DropIndex].flags[LineUpdate];
									MaintainMsgQ( ISYS_ADDQ, (BYTE*) &qmsg );
								}

								// Batch out the stalled line
								app->pShm->IsysDropStatus[DropIndex].flags[NotBatchedIndex] |= ISYS_DROP_BATCHED;
								app->pShm->IsysDropStatus[DropIndex].flags[NotBatchedIndex] &= ~ISYS_DROP_OVERRIDE;

								// If stalled line is me, batch myself
								if (NotBatchedIndex == (app->this_lineid - 1))
								{
									app->pShm->sys_stat.DropStatus[DropIndex].Batched  = 1;
								}
								else
								{
									//	Send message to line to set its flags and batch itself
									qmsg.cmd    = ISYS_DROP_MAINT;
									qmsg.action = isys_drop_set_flags;
									qmsg.drop_num = app->pShm->sys_set.IsysDropSettings[DropIndex].drop[NotBatchedIndex]; 
									qmsg.lineId   = app->pShm->sys_set.IsysLineSettings.line_ids[NotBatchedIndex];
									qmsg.flags1   = app->pShm->IsysDropStatus[DropIndex].flags[NotBatchedIndex];
									MaintainMsgQ( ISYS_ADDQ, (BYTE*) &qmsg );
								}
							}
						}
						else
						{
							// Keep a record of NotBatchedIndex, Total batch count
							IsysBatchState[DropIndex].TotalBatchCount = (int)app->pShm->IsysDropStatus[DropIndex].BCount[NotBatchedIndex];
							IsysBatchState[DropIndex].TotalBatchWeight = app->pShm->IsysDropStatus[DropIndex].BWeight[NotBatchedIndex];
							IsysBatchState[DropIndex].LineIndex = NotBatchedIndex;
							IsysBatchState[DropIndex].StallCount = 0;
						}
					}
					else
					{
						// Have not found the condition, clear out all the info for this drop
						IsysBatchState[DropIndex].TotalBatchCount = NODATA;
						IsysBatchState[DropIndex].TotalBatchWeight = NODATA;
						IsysBatchState[DropIndex].LineIndex = NODATA;
						IsysBatchState[DropIndex].StallCount = 0;
					}

					break;

				default:

					// Continue to next valid drop

					break;
				}
			}
		}
	}
}

//--------------------------------------------------------
//  Create Drop Manager Timer
//--------------------------------------------------------

HANDLE DropManager::Create_Drop_Manager_Timer()
{
    LARGE_INTEGER  pExp, pInt;

    hDropTimer = RtCreateTimer(
                         NULL,              // Security - NULL is none
                         0,                 // Stack size - 0 is use default
                         Drop_Manager_Timer,// Timer handler
                         (PVOID) 0,         // NULL context (argument to handler)
                         RT_PRIORITY_MIN+52,// Priority
                         CLOCK_FASTEST);    // RTX HAL Timer

    //------------- Start Timer
    // 100ns * 2,000,000 = .5 sec intervals
    pExp.QuadPart = 5000000;
    pInt.QuadPart = 5000000;

     if (!RtSetTimerRelative( hDropTimer, &pExp, &pInt))
     {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        RtCancelTimer(hDropTimer, NULL);
        hDropTimer = NULL;
     }
    else
    {

#ifdef _SHOW_HANDLES_
        RtPrintf("Hnd %.2X Drop Manager Timer\n", hDropTimer);
#endif

    }
    return (hDropTimer);
}


//--------------------------------------------------------
//   OpenIsysMailbox
//--------------------------------------------------------

int DropManager::OpenIsysMailbox()
{

    hDmIsysMutex = RtOpenMutex( SYNCHRONIZE, FALSE, ISYS_MUTEX);
    if (hDmIsysMutex==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    hDmIsysShm = RtOpenSharedMemory( PAGE_READWRITE, FALSE, ISYS_SHM, (LPVOID*) &pDmIsysMsg);

    if (hDmIsysShm==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        RtCloseHandle(hDmIsysMutex);
        return ERROR_OCCURED;
    }

    hDmIsysSemPost = RtOpenSemaphore( SYNCHRONIZE, FALSE, ISYS_SEM_POST);

    if (hDmIsysSemPost==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        RtCloseHandle(hDmIsysShm);
        RtCloseHandle(hDmIsysMutex);
        return ERROR_OCCURED;
    }

    hDmIsysSemAck = RtOpenSemaphore( SYNCHRONIZE, FALSE, ISYS_SEM_ACK);
    if (hDmIsysSemAck==NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        RtCloseHandle(hDmIsysShm);
        RtCloseHandle(hDmIsysMutex);
        RtCloseHandle(hDmIsysSemPost);
        return ERROR_OCCURED;
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
//  CreateDropManagerMbxThread
//--------------------------------------------------------

int DropManager::CreateDropManagerMbxThread()
{
    DWORD            threadId;

//----- Debugger

    hDrpMgr = CreateThread(
                            NULL,            
                            0,               //default
                            (LPTHREAD_START_ROUTINE) Mbx_Server, //function
                            NULL,            //parameters
                            0,
                            (LPDWORD) &threadId
                            );

    if(hDrpMgr == NULL)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    if(!RtSetThreadPriority( hDrpMgr, RT_PRIORITY_MIN+50))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        TerminateThread( hDrpMgr, 1);
        return ERROR_OCCURED;
    }

#ifdef _SHOW_HANDLES_
    RtPrintf("Hnd %.2X DrpMgr thread\n", threadId);
#endif

    return NO_ERRORS;
}

//--------------------------------------------------------
//   Mbx_Server
//--------------------------------------------------------

int RTFCNDCL DropManager::Mbx_Server(PVOID unused)
{
	unused;	// RED - Added to eliminate warning for unused formal parameter
    static HANDLE  hDrpMgrShm, hDrpMgrMutex, hDrpMgrSemPost, hDrpMgrSemAck;
    static BOOL    bInit = FALSE;
    int            event;

    if (!bInit)
    {

        SetUnhandledExceptionFilter(dmUnhandledExceptionFilter);

        hDrpMgrMutex = RtOpenMutex( SYNCHRONIZE, FALSE, DRPMGR_MUTEX);

        if (hDrpMgrMutex==NULL)
        {
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hDrpMgrShm = RtOpenSharedMemory( PAGE_READWRITE, FALSE, DRPMGR_SHM, (LPVOID*) &pDrpMgrMsg);

        if (hDrpMgrShm==NULL)
        {
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            RtCloseHandle(hDrpMgrMutex);
            return ERROR_OCCURED;
        }

        hDrpMgrSemPost = RtOpenSemaphore( SYNCHRONIZE, FALSE, DRPMGR_SEM_POST);

        if (hDrpMgrSemPost==NULL)
        {
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            RtCloseHandle(hDrpMgrShm);
            RtCloseHandle(hDrpMgrMutex);
            return ERROR_OCCURED;
        }

        hDrpMgrSemAck = RtOpenSemaphore( SYNCHRONIZE, FALSE, DRPMGR_SEM_ACK);
        if (hDrpMgrSemAck==NULL)
        {
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            RtCloseHandle(hDrpMgrShm);
            RtCloseHandle(hDrpMgrMutex);
            RtCloseHandle(hDrpMgrSemPost);
            return ERROR_OCCURED;
        }

        bInit = TRUE;
    }

    // indicate this thread is ready
    app->pShm->IsysLineStatus.app_threads |= RTSS_DM_MBX_SVR_RDY;

#ifdef _SHOW_HANDLES_
    RtPrintf("Hnd %.2X Drop Manager Mbx_Server\n", hDrpMgr);
#endif

    for (;;)
    {
       // Guard: skip pShm access if invalid (prevents SIGSEGV)
       if (!isPShmValid())
       {
           Sleep(100);
           continue;
       }

       // check for everything ok
       if(RtWaitForSingleObject(hInterfaceThreadsOk,INFINITE)==WAIT_FAILED)
           RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
       else
       {
            event = RtWaitForSingleObject( hDrpMgrSemPost, WAIT50MS);

            switch(event)
            {
                case WAIT_OBJECT_0:

                    drpman->ProcessMbxMsg();
                    pDrpMgrMsg->Ack = 1;

                    if(!RtReleaseSemaphore( hDrpMgrSemAck, ldmReleaseCount, NULL))
                        RtPrintf("RtReleaseSemaphore failed.\n");
                    break;

                case WAIT_TIMEOUT:

                    // ok to send drop manager mailbox thread trace
                    if ( trc[DMMBXBUFID].send_OK )
                    {
                        // Request Mutex for send
                        if(RtWaitForSingleObject(trc[DMMBXBUFID].mutex, WAIT50MS) != WAIT_OBJECT_0)
                            RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);
                        else
                        {
// Send debug to self
#ifdef _REDIRECT_DEBUG_
                            drpman->SendLineMsg( DEBUG_MSG, app->this_lineid, NULL, 
                                                 (BYTE*) &trc_buf[DMMBXBUFID], 
                                                 trc[DMMBXBUFID].len);
#endif

#ifdef _LOCAL_DEBUG_
							if (UDP_ENABLE & app->pShm->dbg_set.UdpTrace)
							{
#ifdef	_TELNET_UDP_ENABLED_
								UdpTraceNBuf((char*) &trc_buf[DMMBXBUFID], trc[DMMBXBUFID].len);
#else
								PrintNBuf((char*) &trc_buf[DMMBXBUFID], trc[DMMBXBUFID].len);
#endif
							}
							else
							{
								PrintNBuf((char*) &trc_buf[DMMBXBUFID], trc[DMMBXBUFID].len);
							}
#endif

#ifdef	_HOST_DEBUG_
							drpman->SendHostMsg( DEBUG_MSG, NULL, 
                                             (BYTE*) &trc_buf[DMMBXBUFID], 
                                             trc[DMMBXBUFID].len);
#endif




                            if(!RtReleaseMutex(trc[DMMBXBUFID].mutex))
                                RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__);

                        }

                        // clear old message.
                        memset(&trc_buf[DMMBXBUFID], 0, sizeof(trcbuf));
                        sprintf( (char*) &trc_buf[DMMBXBUFID],"L%d:%s\n",
                                 app->this_lineid,
                                 &trc_buf_desc[DMMBXBUFID]);

                        trc[DMMBXBUFID].send_OK  = false;
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
//  SendIsysMsg
//--------------------------------------------------------

int DropManager::SendIsysMsg( int cmd, BYTE *data, int len)
{
    TIsysIpcMsg isys_msg;
    CHAR*       pisys_msg;
    int i;

    // Due to the volume of IPC messages, these will not go to host
    DebugTrace(_IPC_,"DMgr->Isys\t[%s]\n", isys_msg_desc[cmd - ISYS_MSG_START]);

    memset(&isys_msg,0,sizeof(isys_msg));

    // Build message for intersystems

    switch(cmd)
    {
        case ISYS_DROPMSG:
            {
                int* drop_num          = (int*) data;
                isys_msg.imsg.cmd      = cmd;
                isys_msg.imsg.drop_num = *drop_num;
            }
            break;

        case ISYS_DROP_MAINT:
            memcpy(&isys_msg.imsg,data,len); // matches perfectly
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
    if(RtWaitForSingleObject(hDmIsysMutex, WAIT50MS) != WAIT_OBJECT_0)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    //
    // Copy the message string
    //
    pisys_msg = (CHAR*) &isys_msg;
    for (i = 0; i < sizeof(TIsysIpcMsg); i++ )
        pDmIsysMsg->Buffer[i] = pisys_msg[i];

    //
    // Release the Post Semaphore to the server and wait on the Acknowledge.
    // Confirm that the acknowledge is not left-over from a previous client.
    //
    // NOTE: Extremely small hole here -- if the client is terminated between
    //       clearing the acknowledge and releasing the semaphore.
    //
    pDmIsysMsg->Ack = 0;

    if(!RtReleaseSemaphore(hDmIsysSemPost, ldmReleaseCount, NULL))
    {
        RtReleaseMutex( hDmIsysMutex);
        RtPrintf("Error file %s, line %d hnd %u\n",_FILE_, __LINE__,(int) hDmIsysSemPost);
        return ERROR_OCCURED;
    }

    for (;;)
    {

        if(RtWaitForSingleObject(hDmIsysSemAck, INFINITE)==WAIT_FAILED)
        {
            RtReleaseMutex( hDmIsysMutex);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }
        else
        {
            if (pDmIsysMsg->Ack != (LONG) 0)
                break;
        }
     }

    //
    // Release the Mutex for other client's use.
    //
    if(!RtReleaseMutex( hDmIsysMutex))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    DebugTrace(_IPC_, "DMgr->Isys\t[%s]\tOk\n", isys_msg_desc[cmd - ISYS_MSG_START]);

    return NO_ERRORS;
}

//
// Function to post a message string to the message server.
//
// NOTE: This function is structured so that any client can abort at any time, including
//       the middle of posting a message, and the messaging protocol and server will
//       continue to operate properly.
//
int DropManager::SendLineMsg( int cmd, int line, int var, BYTE *data, int len)
{
    static HANDLE      hDmLineShm, hDmLineMutex, hDmLineSemPost, hDmLineSemAck;
    static BOOL        bDmLineInit = FALSE;
    static PAPPIPCMSG  pDmLMsg;
    TAppIpcMsg*        pDmLineMsg;
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
    if (!bDmLineInit)
    {
        hDmLineMutex = RtOpenMutex( SYNCHRONIZE, FALSE, SEND_MUTEX);

        if (hDmLineMutex==NULL)
        {
            RtPrintf("Error file %s, DmLine %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hDmLineShm = RtOpenSharedMemory( PAGE_READWRITE, FALSE, SEND_SHM, (LPVOID*) &pDmLMsg);

        if (hDmLineShm==NULL)
        {
            RtCloseHandle(hDmLineMutex);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hDmLineSemPost = RtOpenSemaphore( SYNCHRONIZE, FALSE, SEND_SEM_POST);

        if (hDmLineSemPost==NULL)
        {
            RtCloseHandle(hDmLineShm);
            RtCloseHandle(hDmLineMutex);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hDmLineSemAck = RtOpenSemaphore( SYNCHRONIZE, FALSE, SEND_SEM_ACK);

        if (hDmLineSemAck==NULL)
        {
            RtCloseHandle(hDmLineShm);
            RtCloseHandle(hDmLineMutex);
            RtCloseHandle(hDmLineSemPost);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        bDmLineInit = TRUE;
    }

    // Don't be recursive here. Don't use TRACE. 
    //RtPrintf("App->DmLine\t[%s]\n",app_msg[cmd - APP_MSG_START]);

    //
    // Acquire the serialization mutex.  Confirm that the msg buffer is really free
    // as it may have been abandoned by another client in the middle of a post.
    //
    if(RtWaitForSingleObject(hDmLineMutex, WAIT50MS) != WAIT_OBJECT_0)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    memset(&pDmLMsg->Buffer, 0, sizeof(mhdr) + len + 1);

    // set a pointer to fill data
    pDmLineMsg                     = (TAppIpcMsg*) &pDmLMsg->Buffer;
    pDmLineMsg->gen.hdr.cmd        = cmd;
    pDmLineMsg->gen.hdr.destLineId = line;
    pDmLineMsg->gen.hdr.srcLineId  = app->this_lineid;

    // miscellaneous stuff
    switch(cmd)
    {
        case ERROR_MSG:

            // copy data
            for (i = 0; i < len; i++ )
                pDmLineMsg->errMsg.text[i] = data[i];

            // make sure it is null terminated
            pDmLineMsg->errMsg.text[len] = '\0';
            len++;

            pDmLineMsg->errMsg.severity  = var;
            len += sizeof(var);          // add in the severity length
            pDmLineMsg->errMsg.hdr.len   = len;
            break;

        case DEBUG_MSG:

            for (i = 0; i < len; i++ )
                pDmLineMsg->gen.data[i]  = data[i];

            // make sure it is null terminated
            pDmLineMsg->gen.data[len] = '\0';
            len++;

            pDmLineMsg->gen.hdr.len      = len;
            // set drop rec recovery bit or other flag bit
            pDmLineMsg->gen.hdr.flags    = var;
            break;

        default:
            RtPrintf("Error file %s, line %d \n", _FILE_, __LINE__);
            RtReleaseMutex( hDmLineMutex);
            return ERROR_OCCURED;
            break;
    }

    // tell net client where to send and how much
    pDmLMsg->LineId = line;
    pDmLMsg->Len    = sizeof(mhdr) + len; // add header length

    //
    // Release the Post Semaphore to the server and wait on the Acknowledge.
    // Confirm that the acknowledge is not left-over from a previous client.
    //
    // NOTE: Extremely small hole here -- if the client is terminated between
    //       clearing the acknowledge and releasing the semaphore.
    //
    pDmLMsg->Ack = 0;

    if(!RtReleaseSemaphore(hDmLineSemPost, ldmReleaseCount, NULL))
    {
        RtReleaseMutex( hDmLineMutex );
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    for (;;)
    {
        if(RtWaitForSingleObject( hDmLineSemAck, INFINITE)==WAIT_FAILED)
        {
            RtReleaseMutex( hDmLineMutex );
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        if (pDmLMsg->Ack != (LONG) 0)
            break;
    }

    //
    // Release the Mutex for other client's use.
    //
    if(!RtReleaseMutex( hDmLineMutex ))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
//  SendHostMsg
//--------------------------------------------------------

int DropManager::SendHostMsg( int cmd, int var, BYTE *data, int len)
{
    static HANDLE      hDmShShm, hDmShMutex, hDmShSemPost, hDmShSemAck;
    static PAPPIPCMSG  pMsg;
    static BOOL        bDmShInit = FALSE;
    TAppIpcMsg*        pappMsg;
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
    if (!bDmShInit)
    {
        hDmShMutex = RtOpenMutex( SYNCHRONIZE, FALSE, SEND_HST_MUTEX);

        if (hDmShMutex==NULL)
        {
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hDmShShm = RtOpenSharedMemory( PAGE_READWRITE, FALSE, SEND_HST_SHM, (LPVOID*) &pMsg);

        if (hDmShShm==NULL)
        {
            RtCloseHandle(hDmShMutex);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hDmShSemPost = RtOpenSemaphore( SYNCHRONIZE, FALSE, SEND_HST_SEM_POST);

        if (hDmShSemPost==NULL)
        {
            RtCloseHandle(hDmShShm);
            RtCloseHandle(hDmShMutex);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        hDmShSemAck = RtOpenSemaphore( SYNCHRONIZE, FALSE, SEND_HST_SEM_ACK);

        if (hDmShSemAck==NULL)
        {
            RtCloseHandle(hDmShShm);
            RtCloseHandle(hDmShMutex);
            RtCloseHandle(hDmShSemPost);
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        bDmShInit = TRUE;
    }

    //
    // Acquire the serialization mutex.  Confirm that the msg buffer is really free
    // as it may have been abandoned by another client in the middle of a post.
    //
    if(RtWaitForSingleObject( hDmShMutex, WAIT50MS) != WAIT_OBJECT_0)
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }


    // Don't be recursive here. Don't use TRACE. 
    //RtPrintf("DMgr->Hst\t[%s]\n",app_msg[cmd - APP_MSG_START]);

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
            for (i = 0; i < len; i++ )
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

    if(!RtReleaseSemaphore( hDmShSemPost, ldmReleaseCount, NULL))
    {
        RtReleaseMutex( hDmShMutex );
        RtPrintf("Error file %s, line %d hnd %u\n",_FILE_, __LINE__,(int) hDmShSemPost);
        return ERROR_OCCURED;
    }

    for (;;)
    {

        if(RtWaitForSingleObject( hDmShSemAck, INFINITE)==WAIT_FAILED)
        {
            RtReleaseMutex( hDmShMutex );
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        if (pMsg->Ack != (LONG) 0)
            break;
    }

    //
    // Release the Mutex for other client's use.
    //
    if(!RtReleaseMutex( hDmShMutex ))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
//  CheckDropCutoffs
//
//  This routine will check the count setpoints for the 
//  various drop modes and sends stop commands to the
//  remote intersystem drops.
//
//--------------------------------------------------------

bool DropManager::CheckDropCutoffs(int action, int drop, __int64 actCount, bool& StopAll )
{
    bool    local_stop		= false;
    bool    stop_all		= false;
    char    stop_str[10]	= "StopSlw";
    int     drop_master		= 0;
    int     i,flag = 0;		// RED - Added initialization to flag to avoid warning about possible use without initialization
    __int64		IsysCutoff = 0;		// RED - Added initialization to avoid warning about possible use without initialization
    __int64		IsysSetpoint = 0;	// RED - Added initialization to avoid warning about possible use without initialization
    static  TDrpMgrMsgQEntry qmsg;
//	int		drop_plus1		= drop + 1;		// RED - Removed because it was initializaed but not referenced
//	int		drp_index		= 0;		// RED - Removed because it was initializaed but not referenced
//    int		start_drp_index = drop;		// RED - Removed because it was initializaed but not referenced
//    int		loops           = 0;		// RED - Removed because it was initializaed but not referenced
//	int		drp_index_plus1 = 0;		// RED - Removed because it was initializaed but not referenced
//	int		start_index		= 0;		// RED - Removed because it was initializaed but not referenced

	StopAll = false;

	GET_DROP_MASTER(drop, app->pShm);

    switch(action)
    {
        case isys_chk_batch_cnt:

            // Calculate the high water mark
            IsysSetpoint = app->pShm->Schedule[drop].BchCntSp;

            // If very small batches, like with drop loop or alternating
			// drop loop, the slowdown count may be larger than the setpoint.
			// Just subtract one from the setpoint.
            if (IsysSetpoint > app->isys_cnt_slwdn[drop])
			{
                IsysCutoff = IsysSetpoint - app->isys_cnt_slwdn[drop];
			}
            else
			{
                IsysCutoff = IsysSetpoint - 1;
			}
			//DebugTrace(_DEBUG_,"CheckDropCutoffs:cutoff for drop %d flag 0x%x Cutoff %el slwdn %d\n", drop,
			//		app->pShm->IsysDropStatus[drop].flags[app->this_lineid - 1],
			//		IsysCutoff, app->isys_cnt_slwdn[drop]);

            flag = ISYS_DROP_BATCHED;

            break;

        case isys_chk_batch_wt:

            // Calculate the high water mark
            IsysSetpoint = app->pShm->Schedule[drop].BchWtSp;

            // If very small batches, like with drop loop or alternating
			// drop loop, the slowdown weight may be larger than the setpoint.
			// Just subtract one avg birds weight from the setpoint.
            if (IsysSetpoint > app->isys_wt_slwdn[drop])
			{
                IsysCutoff = IsysSetpoint - app->isys_wt_slwdn[drop];
			}
            else
			{
                IsysCutoff = IsysSetpoint - (app->isys_wt_slwdn[drop] / app->isys_cnt_slwdn[drop]);
			}
			//DebugTrace(_DEBUG_,"CheckDropCutoffs:cutoff for drop %d flag 0x%x Cutoff %el slwdn %d\n", drop,
			//		app->pShm->IsysDropStatus[drop].flags[app->this_lineid - 1],
			//		IsysCutoff, app->isys_cnt_slwdn[drop]);

            flag = ISYS_DROP_BATCHED;

            break;

        case isys_chk_bpm_cnt:

			//	If this drop is BPM / Trickle mode
			if (app->pShm->Schedule[drop].BpmMode == bpm_trickle)
			{
				IsysCutoff = app->target_trickle_count[drop];
				IsysSetpoint = app->target_trickle_count[drop];
			}
			else
			{	//	This drop is straight BPM mode
				// Calculate the high water mark
				IsysSetpoint = app->pShm->Schedule[drop].BPMSetpoint;

				// If very small batches, as in testing, the slowdown count
				// may be larger than the setpoint. Just subtract one from
				// the setpoint.
				if ((IsysSetpoint > app->pShm->sys_set.DropSettings[0].IsysBchSlowdown) &&
					(app->pShm->sys_set.DropSettings[0].IsysBchSlowdown <= 10) &&
					(app->pShm->sys_set.DropSettings[0].IsysBchSlowdown >= 0))
				{
				   // IsysCutoff = IsysSetpoint - app->isys_cnt_slwdn[drop];
					IsysCutoff = IsysSetpoint - app->pShm->sys_set.DropSettings[0].IsysBchSlowdown;
                																	// front end input is used to determine slow down   
                																	// this will keep the remote lines active longer to provide
                																	// birds from the remote lines until the bpm is satisfied. DRW 7/1/2005 2:30PM
				}
				else
				{
					IsysCutoff = IsysSetpoint - 1;
				}
			}

			flag = ISYS_DROP_SUSPENDED;

            break;

        default:
            RtPrintf("Error file %s, line %d \n", _FILE_, __LINE__);
            break;
    }

//----- Nothing to stop

    if (actCount < IsysCutoff)
	{
		return false;
	}

//----- Stop all

    if (actCount >= IsysSetpoint)
    {
        stop_all = true;
		StopAll = true;
        sprintf(stop_str,"%s","StopAll");
		//DebugTrace(_DEBUG_, "CheckDropCutoffs:stop_all\tdrop\t%d\n", drop);
    }

    // start building message to other line(s)
    qmsg.cmd    = ISYS_DROP_MAINT;
    qmsg.action = isys_drop_set_flags;

//----- Find the first line sharing this drop (drop master)

    GET_DROP_MASTER(drop, app->pShm)

//----- Send stop commands to remotes

    if ( app->this_lineid == drop_master )
    {
		//	Check to see if there is only one line open
		{
			int drop_count = 0;
			int batch_count = 0;
			for (int i = 0; i < MAXIISYSLINES; i++)
			{
				if (app->pShm->sys_set.IsysDropSettings[drop].active[i])
					drop_count++;
				if (app->pShm->IsysDropStatus[drop].flags[i] & ISYS_DROP_BATCHED)
					batch_count++;
			}
			if ((drop_count - batch_count) < 2)
				return	stop_all ? true : false;
		}

//----- Show counts

        if DBG_DROP(drop, app->pShm)
        {
            if( (!trc[DMMBXBUFID].buffer_full) && (TraceMask & _FDROPS_ ) )
            {
                sprintf((char*) &tmp_trc_buf[DMMBXBUFID],"\nChkCt\tdrop\t%u\t%s\tfstidx\t%d\tactCnt\t%u\tIoffCnt\t%u\tTOffCnt\t%u\n", 
                              drop+1, stop_str, 
                              app->isys_fastest_idx[drop], 
                              (UINT) actCount, (UINT) IsysCutoff, (UINT) IsysSetpoint);
                strcat((char*) &trc_buf[DMMBXBUFID],(char*) &tmp_trc_buf[DMMBXBUFID]);
            }
        }

        for (i = 0; i < MAXIISYSLINES; i++)
        {
            // Stop any drop which needs stopping
			if ((app->pShm->IsysDropStatus[drop].flags[i] & ISYS_DROP_OVERRIDE) == 0)	// REDTODO: Look at this test
			{
				if ( app->pShm->sys_set.IsysDropSettings[drop].active[i] )
				{
					if DBG_DROP(drop, app->pShm)
					{
						if( (!trc[DMMBXBUFID].buffer_full) && (TraceMask & _FDROPS_) )
						{
							sprintf((char*) &tmp_trc_buf[DMMBXBUFID],"\tidx\t%d\tidrop\t%d\tflgs\t%x\n", 
										  i, app->pShm->sys_set.IsysDropSettings[drop].drop[i],
										  app->pShm->IsysDropStatus[drop].flags[i]);
							strcat((char*) &trc_buf[DMMBXBUFID],(char*) &tmp_trc_buf[DMMBXBUFID]);
						}
					}

					if ( (stop_all || ((i != app->isys_fastest_idx[drop]))) && 
						 ((app->pShm->IsysDropStatus[drop].flags[i] & flag) != flag) )
					{
						//DebugTrace(_DEBUG_,
						//	"Send stop\tidx\t%d\tidrop\t%d\taction\t%d\tdrp_flg\t%d\tflg\t%d\n", 
						//		 i, app->pShm->sys_set.IsysDropSettings[drop].drop[i], action,
						//		 app->pShm->IsysDropStatus[drop].flags[i], flag);

						qmsg.drop_num = app->pShm->sys_set.IsysDropSettings[drop].drop[i]; 
						qmsg.lineId   = app->pShm->sys_set.IsysLineSettings.line_ids[i];
						// cut down on messages by writing flag here
						app->pShm->IsysDropStatus[drop].flags[i] |= flag;
						qmsg.flags1   = app->pShm->IsysDropStatus[drop].flags[i];

						// send stop message to all remotes
						if (i != app->this_lineid - 1) //&& (app->pShm->IsysLineStatus.connected[i] != 0))
						{
							MaintainMsgQ( ISYS_ADDQ, (BYTE*) &qmsg );
						}
						else
						{
							local_stop = true;
						}

						if DBG_DROP(drop, app->pShm)
						{
							if( (!trc[DMMBXBUFID].buffer_full) && (TraceMask & _FDROPS_ ) )
							{
								sprintf( (char*) &tmp_trc_buf[DMMBXBUFID],"Send stop\tidx\t%d\tidrop\t%d\tflag\t%X\n", 
												i, app->pShm->sys_set.IsysDropSettings[drop].drop[i],
												app->pShm->IsysDropStatus[drop].flags[i]);
								strcat ( (char*) &trc_buf[DMMBXBUFID], (char*) &tmp_trc_buf[DMMBXBUFID]);
							}	// End if( (!trc[DMMBXBUFID].buffer_full) && (TraceMask & _FDROPS_ ) )
						}	// End if DBG_DROP(drop, app->pShm)
					}	// End if ( (stop_all || ((!stop_all) && (i != app->isys_fastest_idx[drop]))) && ...
				}	// End if ( app->pShm->sys_set.IsysDropSettings[drop].active[i] )
			}	// End if ((app->pShm->IsysDropStatus[drop].flags[i] & ISYS_DROP_OVERRIDE) == 0)
		}	// End for (i = 0; i < MAXIISYSLINES; i++)
    }	// End if ( app->this_lineid == drop_master )

//----- safety feature, stop local drop

    // Sometimes the bpm reset is just a little slow arriving, Let it go.
    if (stop_all)
    {
		if ((app->this_lineid != drop_master ) &&
             (action == isys_chk_bpm_cnt) )
            return false;
        else
            return true;
    }

    if (local_stop)
        return true;

    return false;
}

//--------------------------------------------------------
//  CheckDropMode
//--------------------------------------------------------

void DropManager::CheckDropMode(int check_request)
{
    app_type::TScheduleSettings *pSch;
    app_type::TScheduleStatus   *pSchStat;
    app_type::TShackleStatus    *pShkStat;
    TDrpMgrIpcMsg               *cmsg;
    int      i, drop, drop_plus1, shk, scl;
    int      send_update    = false;
    int      drop_master    = 0;
    int      TotalPPMCount  = 0;
    int      TotalBchActCnt = 0;
    __int64  TotalActWeight = 0;
    int      this_line_idx  = app->this_lineid - 1;
    char     loc_rem        = (check_request == CHECK_DROP_LOCAL ? 'L' : 'R');
	static  TDrpMgrMsgQEntry qmsg;
	bool	StopAll;
	bool	ByCount;
    
//----- Get the local drop number from the message

    cmsg       = (TDrpMgrIpcMsg*) &pDrpMgrMsg->Buffer;
    drop       = cmsg->ck_drp.drop_num;
    shk        = cmsg->ck_drp.shackle;
    scl        = cmsg->ck_drp.scale;
    pSchStat   = &app->pShm->sys_stat.ScheduleStatus[drop];
    pSch       = &app->pShm->Schedule[drop];
    pShkStat   = &app->pShm->ShackleStatus[shk];
    drop_plus1 = drop + 1;

//----- Update counts for local drop

    if (check_request == CHECK_DROP_LOCAL)
    {
        app->pShm->IsysDropStatus[drop].PPMCount[this_line_idx] = pSchStat->PPMCount;
        app->pShm->IsysDropStatus[drop].BCount  [this_line_idx] = pSchStat->BchActCnt;
        app->pShm->IsysDropStatus[drop].BWeight [this_line_idx] = pSchStat->BchActWt;
    }

    if DBG_DROP(drop, app->pShm)
    {
        if( (!trc[DMMBXBUFID].buffer_full) && (TraceMask & _FDROPS_ ) )
        {
            sprintf((char*) &tmp_trc_buf[DMMBXBUFID],"\n*CkDrp%c\tdrop\t%d\tidxL\t%d\n", 
                          loc_rem, drop_plus1, this_line_idx);
            strcat((char*) &trc_buf[DMMBXBUFID], (char*) &tmp_trc_buf[DMMBXBUFID]);
        }
    }

//----- Find the first line sharing this drop (drop master)

    GET_DROP_MASTER(drop, app->pShm)

//----- Loop through the isys drops, adding the totals and checking for
//      for a faster bpm. Local was already done just add remotes

    for (i = 0; i < MAXIISYSLINES; i++)
    {   
        if ( app->pShm->sys_set.IsysDropSettings[drop].active[i] )
        {
            if ( (i != app->this_lineid - 1) && (check_request == CHECK_DROP_LOCAL) )
                send_update = true;

            // Add the totals from the isys lines

            TotalPPMCount  += app->pShm->IsysDropStatus[drop].PPMCount[i];
            TotalBchActCnt += (int) app->pShm->IsysDropStatus[drop].BCount[i];
            TotalActWeight += app->pShm->IsysDropStatus[drop].BWeight[i];

            if DBG_DROP(drop, app->pShm)
            {
                if( (!trc[DMMBXBUFID].buffer_full) && (TraceMask & _FDROPS_ ) )
                {
                    sprintf((char*) &tmp_trc_buf[DMMBXBUFID],"\tidx\t%u\tdrop\t%u\tBPMCt\t%u\tBCt\t%u\tBWt\t%u\n",
                                  i,
                                  app->pShm->sys_set.IsysDropSettings[drop].drop[i],
                                  app->pShm->IsysDropStatus[drop].PPMCount[i],
                                  (UINT) app->pShm->IsysDropStatus[drop].BCount[i], 
                                  (UINT) app->pShm->IsysDropStatus[drop].BWeight[i]);
                    strcat((char*) &trc_buf[DMMBXBUFID],(char*) &tmp_trc_buf[DMMBXBUFID]);
                }
            }

        }
    }

//----- Stop dropping when:

    // 1. Batch(BchCnt) reached
    // 2. Batch(BchWtSp) reached
    // 3. BPM reached

    switch(pSch->DistMode)
    {
        case mode_3_batch:
        case mode_5_batch_rate:
        case mode_6_batch_alt_rate:
        case mode_7_batch_alt_rate:

			ByCount = (pSch->BchCntSp > 0);
            // if CheckDropCutoffs returns true to stop based on total or bpm
            if ( CheckDropCutoffs( ByCount ? isys_chk_batch_cnt : isys_chk_batch_wt, drop, ByCount ? (__int64)TotalBchActCnt : TotalActWeight, StopAll) )
            {
                if ( app->pShm->sys_stat.DropStatus[drop].Batched == 0 )
                {
                    app->pShm->sys_stat.DropStatus[drop].Batched  = 1;
					// RED 2013-06-06 - Added following line to set Intersystem flag to batched so drop is batched no matter what flag is looked at
					app->pShm->IsysDropStatus[drop].flags[app->this_lineid - 1] |= ISYS_DROP_BATCHED;
                    if (check_request == CHECK_DROP_LOCAL)
                    {
                        if (StopAll || is_isys_batch_complete(drop, pSch->DistMode) )
                        {
							//RtPrintf("CHECK_DROP_LOCAL:IsysBatch Complete for drop %d flag 0x%x\n", 
							//	drop + 1,
							//	app->pShm->IsysDropStatus[drop].flags[app->this_lineid - 1]); 
                            app->pShm->sys_stat.DropStatus[drop].last_in_batch = true;
	                        pShkStat->last_in_batch[scl] = true;

							if ((pSch->DistMode == mode_6_batch_alt_rate) || 
								(pSch->DistMode == mode_7_batch_alt_rate))
							{

								if ((SearchLoopUtility(search_locals, drop))  == ERROR_OCCURED)
								{
									if (((SearchLoopUtility(search_remotes, drop)) == ERROR_OCCURED) ||
										(app->this_lineid != drop_master))
									{
										SearchLoopUtility(clear_drops, drop);

										// Send the reset to each line involved in the isys loop.
										// We must insert directly into the queue because we
										// are currently in the message handler.
										for ( int LineIndex = 0; LineIndex < MAXIISYSLINES; LineIndex++ )
										{
											if (LineIndex != app->this_lineid - 1)
											{
												qmsg.lineId = LineIndex + 1;

												// verify line id
												if ( (qmsg.lineId < 1) && (qmsg.lineId > MAXIISYSLINES) ) 
												{
													break;
												}

												if (  app->pShm->sys_set.IsysLineSettings.active[LineIndex] &&
													  app->pShm->IsysLineStatus.connected[LineIndex] )
												{
													// start building message to other line(s)
													qmsg.cmd    = ISYS_DROP_MAINT;
													qmsg.action = isys_drop_batch_loop_reset;
													qmsg.drop_num = app->pShm->sys_set.IsysDropSettings[drop].drop[LineIndex]; 
													qmsg.lineId   = app->pShm->sys_set.IsysLineSettings.line_ids[LineIndex];
													// cut down on messages by writing flag here
													qmsg.flags1   = app->pShm->IsysDropStatus[drop].flags[LineIndex];
													MaintainMsgQ( ISYS_ADDQ, (BYTE*) &qmsg );
												}
											}
										}
									}
								}
							}
                        }

                        if DBG_DROP(drop, app->pShm)
                        {
                            if( (!trc[DMMBXBUFID].buffer_full) && (TraceMask & ( _FDROPS_ | _LABELS_ )) )
                            {
                               sprintf((char*) &tmp_trc_buf[DMMBXBUFID],"Btch'd*\tdrop\t%d\tBch%s\t%u\tT%s\t%u\tBchNo\t%u\tLst\t%d\n",
																		 drop_plus1,
																		 ByCount ? "Cnt" : "Wt",
																		 ByCount ? (UINT) pSch->BchCntSp : (UINT) pSch->BchWtSp,
																		 ByCount ? "Cnt" : "Wt",
																		 ByCount ? (UINT) TotalBchActCnt : (UINT) TotalActWeight,
																		 app->pShm->sys_stat.DropStatus[drop].batch_number & BATCH_MASK,
																		 app->pShm->sys_stat.DropStatus[drop].last_in_batch & 1);
                                strcat((char*) &trc_buf[DMMBXBUFID],(char*) &tmp_trc_buf[DMMBXBUFID]);
                            }
                        }
                    }
                    // status flag changed, update remotes
                    else
					{
						if ((pSch->DistMode == mode_6_batch_alt_rate) || 
							(pSch->DistMode == mode_7_batch_alt_rate))
						{

							if (is_isys_batch_complete(drop, pSch->DistMode) )
							{
								//RtPrintf("CHECK_DROP_REMOTE:IsysBatch Complete for drop %d flag 0x%x\n", 
 								//	drop + 1,
								//	app->pShm->IsysDropStatus[drop].flags[app->this_lineid - 1]); 
								//app->pShm->sys_stat.DropStatus[drop].last_in_batch = true;
								//pShkStat->last_in_batch[scl] = true;

								if ((SearchLoopUtility(search_locals, drop))  == ERROR_OCCURED)
								{
									if (((SearchLoopUtility(search_remotes, drop)) == ERROR_OCCURED) ||
										(app->this_lineid != drop_master))
									{
										SearchLoopUtility(clear_drops, drop);

										// Send the reset to each line involved in the isys loop.
										// We must insert directly into the queue because we
										// are currently in the message handler.
										for ( int LineIndex = 0; LineIndex < MAXIISYSLINES; LineIndex++ )
										{
											if (LineIndex != app->this_lineid - 1)
											{
												qmsg.lineId = LineIndex + 1;

												// verify line id
												if ( (qmsg.lineId < 1) && (qmsg.lineId > MAXIISYSLINES) ) 
												{
													break;
												}

												if (  app->pShm->sys_set.IsysLineSettings.active[LineIndex] &&
													  app->pShm->IsysLineStatus.connected[LineIndex] )
												{
													// start building message to other line(s)
													qmsg.cmd    = ISYS_DROP_MAINT;
													qmsg.action = isys_drop_batch_loop_reset;
													qmsg.drop_num = app->pShm->sys_set.IsysDropSettings[drop].drop[LineIndex]; 
													qmsg.lineId   = app->pShm->sys_set.IsysLineSettings.line_ids[LineIndex];
													// cut down on messages by writing flag here
													qmsg.flags1   = app->pShm->IsysDropStatus[drop].flags[LineIndex];
													MaintainMsgQ( ISYS_ADDQ, (BYTE*) &qmsg );
												}
											}
										}
									}
								}
							}
						}

						send_update = true;
					}
                }
            }
            break;

        default:
            break;
    }

    // 2. Check BPM count

    switch(pSch->DistMode)
    {
        case mode_4_rate:
        case mode_5_batch_rate:
        case mode_6_batch_alt_rate:
        case mode_7_batch_alt_rate:

            if (pSch->BPMSetpoint > 0)
            {
                // if CheckDropCutoffs returns true to stop based on total or bpm
                if ( CheckDropCutoffs( isys_chk_bpm_cnt, drop, (__int64)TotalPPMCount, StopAll ) )
                {
                    if (!app->pShm->sys_stat.DropStatus[drop].Batched && (app->pShm->sys_stat.DropStatus[drop].Suspended == 0))
                    {
                        app->pShm->sys_stat.DropStatus[drop].Suspended  = true;

                        if (check_request == CHECK_DROP_LOCAL)
                        {
                            if (DBG_DROP(drop, app->pShm))
                            {
                                if( (!trc[DMMBXBUFID].buffer_full) && (TraceMask & _FDROPS_) )
                                {
                                    sprintf((char*) &tmp_trc_buf[DMMBXBUFID],"Susp'd*\tdrop\t%d\tBPMSp\t%u\tTCnt\t%u\n",
                                            drop_plus1, (UINT) pSch->BPMSetpoint, (UINT) TotalPPMCount);
                                    strcat((char*) &trc_buf[DMMBXBUFID],(char*) &tmp_trc_buf[DMMBXBUFID]);
                                }
                            }
                        }
                        // status flag changed, update remotes
                        else 
						{
							send_update = true;
						}
                    }
                }
            }
            break;

        default:
            break;
    }

//----- Update remotes if there is an intersystems
//      drop for this local drop.

    if ( send_update ) 
	{
		SendIsysMsg(ISYS_DROPMSG, (BYTE*) &drop_plus1, sizeof(int));
	}

}

//--------------------------------------------------------
//  is_isys_batch_complete
//--------------------------------------------------------

int DropManager::is_isys_batch_complete(int drop, int mode ) 
{
    int  lp;
    int  batched    = 0;
    int  drp_cnt    = 0;
    switch(mode)
    {
        case mode_3_batch:
        case mode_5_batch_rate:
        case mode_6_batch_alt_rate:
        case mode_7_batch_alt_rate:

            for (lp = 0; lp < MAXIISYSLINES; lp++)
            {
                if ( app->pShm->sys_set.IsysLineSettings.active[lp] &&
                     app->pShm->sys_set.IsysDropSettings[drop].active[lp] &&
                     (lp != app->this_lineid - 1) )
                 {
                     drp_cnt++; \
                     if ( (app->pShm->IsysDropStatus[drop].flags[lp] & ISYS_DROP_BATCHED) != 0)
                        batched++;
                 }
            }

            if (batched == drp_cnt)
                return true;

            break;

        default:
            break;
    }
return false;
}

//--------------------------------------------------------
//  ExecuteMessage
//
//  Used for maintence commands only. This routine is 
//  Called when the intersystems task receives a 
//  maintenance command. Only intersystem slaves will 
//  receive these messages.
//--------------------------------------------------------

DropManager::ExecuteMessage(int drop, int action, int lineId, int flags, int batch)
{
	batch;	//	RED - Added to eliminate unreferenced formal parameter warning
    int i;
    int drop_minus1;

    //RtPrintf("<<< ExCmd\t[Maint Cmd    ]\tlineId\t%d\tdrop\t%d\taction\t[%13s]\tflags\t%d <<<\n",
    //                  lineId, drop, drp_state_desc[action], flags);

    if( (!trc[ISYSMBXBUFID].buffer_full) && (TraceMask & _COMBASIC_ ) )
    {
        sprintf((char*) &tmp_trc_buf[ISYSMBXBUFID],"<<< ExCmd\t[Maint Cmd    ]\tlineId\t%d\tdrop\t%d\taction\t[%13s]\tflags\t%d <<<\n",
                      lineId, drop, drp_state_desc[action], flags);
        strcat((char*) &trc_buf[ISYSMBXBUFID], (char*) &tmp_trc_buf[ISYSMBXBUFID]);
    }

    drop_minus1 = drop - 1;

    switch(action)
    {
        // MAXIISYSLINES - 1 always refers to this line

        case isys_drop_set_flags:

            if (drop != isys_send_all_drops)
            {
				app->pShm->sys_stat.DropStatus[drop_minus1].Batched   = ((flags & ISYS_DROP_BATCHED)   ? 1 : 0);
                app->pShm->sys_stat.DropStatus[drop_minus1].Suspended = ((flags & ISYS_DROP_SUSPENDED) ? 1 : 0);
                app->pShm->IsysDropStatus[drop_minus1].flags[app->this_lineid - 1] = flags;
				if (!(flags & ISYS_DROP_BATCHED) && (flags & ISYS_DROP_OVERRIDE))
					app->isys_fastest_idx[drop_minus1] = app->this_lineid - 1;

                //DebugTrace(_DEBUG_, "DMExec\tsetflg\tdrp\t%d\tbch'd\t%d\tsusp'd\t%d\n",
                //         drop,
                //         app->pShm->sys_stat.DropStatus[drop_minus1].Batched, 
                //         app->pShm->sys_stat.DropStatus[drop_minus1].Suspended);

                if( (!trc[ISYSMBXBUFID].buffer_full) && (TraceMask & (_COMBASIC_ | _FDROPS_)) )
                {
                    sprintf( (char*) &tmp_trc_buf[ISYSMBXBUFID],"DMExec\tsetflg\tdrp\t%d\tflgs\t0x%x\tbch'd\t%d\tsusp'd\t%d\n",
                             drop, app->pShm->IsysDropStatus[drop_minus1].flags[app->this_lineid - 1],
                             app->pShm->sys_stat.DropStatus[drop_minus1].Batched, 
                             app->pShm->sys_stat.DropStatus[drop_minus1].Suspended);
                    strcat ( (char*) &trc_buf[ISYSMBXBUFID], (char*) &tmp_trc_buf[ISYSMBXBUFID]);
                }
             }
            else
            {
                for ( i = 0; i < MAXDROPS; i++ )
                {
					app->pShm->sys_stat.DropStatus[i].Batched   = ((flags & ISYS_DROP_BATCHED)   ? 1 : 0);
                    app->pShm->sys_stat.DropStatus[i].Suspended = ((flags & ISYS_DROP_SUSPENDED) ? 1 : 0); 
                    app->pShm->IsysDropStatus[i].flags[app->this_lineid - 1] = flags;
               }
            }
            break;

        case isys_drop_bpm_reset:

            app->isys_reset_bpm = true;

            break;

        case isys_drop_batch_loop_reset:

            if (drop != isys_send_all_drops)
            {
                int start = drop_minus1;
                int idx   = drop_minus1;

                //RtPrintf("DMExec\tbchrst\tdrp\t%d\n",drop);

                if( (!trc[ISYSMBXBUFID].buffer_full) && (TraceMask & (_COMBASIC_ | _FDROPS_)) )
                {
                    sprintf( (char*) &tmp_trc_buf[ISYSMBXBUFID],"DMExec\tbchrst\tdrp\t%d\n",drop);
                    strcat ( (char*) &trc_buf[ISYSMBXBUFID], (char*) &tmp_trc_buf[ISYSMBXBUFID]);
                }


                for(;;)
                {
                    if ( (idx >= 0) && (idx < app->pShm->sys_set.NumDrops) )
                    {
                        CLEAR_DROP_BATCH(idx, app->pShm)
                        CLEAR_ISYS_DROP_BATCH(idx, app->pShm)

                        idx = app->pShm->Schedule[idx].NextEntry - 1;

                        if (idx == start) idx = -1;
                    }
                    if (idx == -1) break;
                }
            }
            else
            {
                for ( i = 0; i < MAXDROPS; i++ )
                {
                    CLEAR_DROP_BATCH(i, app->pShm)
                    CLEAR_ISYS_DROP_BATCH(i, app->pShm)
                }
            }

            break;

        default:
             RtPrintf("error actn %d file %s, line %d \n",action, _FILE_, __LINE__);
           break;
    }
    return 0;
}

//--------------------------------------------------------
//  RemoveFailedMsg
//--------------------------------------------------------

DropManager::RemoveFailedMsg()
{
    int i;

    TDrpMgrIpcMsg* cmsg;

    if( (!trc[DMMBXBUFID].buffer_full) && ( TraceMask & (_DRPMGRQ_ | _ISCOMS_)) )
    {
        sprintf((char*) &tmp_trc_buf[DMMBXBUFID],"RemoveFailedMsg\n");
        strcat((char*) &trc_buf[DMMBXBUFID],(char*) &tmp_trc_buf[DMMBXBUFID]);
    }

    cmsg = (TDrpMgrIpcMsg*) &pDrpMgrMsg->Buffer;


    for (i = 0; i < ISYS_MAXWQENTRYS;i++)
    {
        // Message failed, remove it
        if (DrpCtlWrtQ[i].seq_num == cmsg->rem_msg.seq_num)
        {
            if( (!trc[DMMBXBUFID].buffer_full) && ( TraceMask & _DRPMGRQ_ ) )
            {
                sprintf((char*) &tmp_trc_buf[DMMBXBUFID],"RemoveFailedMsg\tdrop\t%d\tlineId\t%d\taction\t%13.13s\n",
                               DrpCtlWrtQ[i].drop_num,
                               DrpCtlWrtQ[i].lineId,
                               drp_state_desc[DrpCtlWrtQ[i].action]);
                strcat((char*) &trc_buf[DMMBXBUFID], (char*) &tmp_trc_buf[DMMBXBUFID]);
            }

            MaintainMsgQ(ISYS_DELQ, (void*) &i);
        }
    }
    return 0;
}

//--------------------------------------------------------
//  RemoveMsg
//
//  Whatever is acknowledged is assumed to have been
//  executed. Only the master will send maintenance 
//  commands to slaves. Updates to the operational states of 
//  the remote drops are done here.
//--------------------------------------------------------

DropManager::RemoveMsg()
{
    int i;

    TDrpMgrIpcMsg* cmsg;

    cmsg = (TDrpMgrIpcMsg*) &pDrpMgrMsg->Buffer;

    for (i = 0; i < ISYS_MAXWQENTRYS;i++)
    {

        // Update status of intersystem drops and remove it
        if ( (cmsg->rem_msg.seq_num > 0) &&
             (DrpCtlWrtQ[i].seq_num == cmsg->rem_msg.seq_num) )
        {
            if( (!trc[DMMBXBUFID].buffer_full) && (TraceMask & _DRPMGRQ_ ) )
            {
                sprintf((char*) &tmp_trc_buf[DMMBXBUFID],"RemoveMsg\tdrop\t%d\tlineId\t%d\taction\t%13.13s\n",
                               DrpCtlWrtQ[i].drop_num,
                               DrpCtlWrtQ[i].lineId,
                               drp_state_desc[DrpCtlWrtQ[i].action]);
                strcat((char*) &trc_buf[DMMBXBUFID],(char*) &tmp_trc_buf[DMMBXBUFID]);
            }

            switch(DrpCtlWrtQ[i].action)
            {
                case isys_drop_bpm_reset:
                case isys_drop_set_flags:
                case isys_drop_batch_loop_reset:
                case isys_drop_batch_req:
                   break;

                default:
                     RtPrintf("error actn %d file %s, line %d \n", DrpCtlWrtQ[i].action, _FILE_, __LINE__);
                   break;
            }

            MaintainMsgQ(ISYS_DELQ, (void*) &i);
        }
    }
    return 0;
}

//--------------------------------------------------------
//  MaintainMsgQ
//
//  Used for maintence commands only. This finds a slot 
//  to add a command or removes it by the index.
//--------------------------------------------------------

int DropManager::MaintainMsgQ(int action, void* msg)
{
    static UINT dq_wrt_seq_num = 1;
    int i;
    int index;
    TDrpMgrMsgQEntry* qmsg = (TDrpMgrMsgQEntry*) msg;
	static int MsgHighWaterMark = ISYS_MAXWQENTRYS/2;

    for (i = 0; i < ISYS_MAXWQENTRYS;i++)
    {
        switch(action)
        {
            case ISYS_ADDQ:

                // if unflags1;used message queue slot
                if (DrpCtlWrtQ[i].flags == ISYS_UNUSEDQ)
                {
					// Warning that our message queue is filling up
					if (i > MsgHighWaterMark)
					{
						RtPrintf("MsgQAdd overflow %d:cmd %d, lineId %d, drp_# %d actn %d seq %d\n",
							i, qmsg->cmd, qmsg->lineId, qmsg->drop_num, qmsg->action, dq_wrt_seq_num);
						MsgHighWaterMark = i;
					} 

                    if ( (qmsg->lineId <= 0) || (qmsg->lineId > MAXIISYSLINES) )
					{
						RtPrintf("Error i %d file %s, line %d \n", i, _FILE_, __LINE__);
                        return ERROR_OCCURED;
					}

                    // can't send to this line
                    if ( (!app->pShm->sys_set.IsysLineSettings.active[qmsg->lineId - 1]) ||
                         (!app->pShm->IsysLineStatus.connected[qmsg->lineId - 1]) )
					{
						RtPrintf("Error i %d file %s, line %d \n", i, _FILE_, __LINE__);
                        return ERROR_OCCURED;
					}

                    if (dq_wrt_seq_num >= 0xfffffff0)
                        dq_wrt_seq_num = 1;

                    memcpy(&DrpCtlWrtQ[i], msg, sizeof(TDrpMgrMsgQEntry));

                    DrpCtlWrtQ[i].seq_num = dq_wrt_seq_num++;
                    DrpCtlWrtQ[i].flags   = ISYS_ACTIVEQ;

                    if( (!trc[DMMBXBUFID].buffer_full) && (TraceMask & _DRPMGRQ_ ) )
                    {
                        sprintf((char*) &tmp_trc_buf[DMMBXBUFID],"MsgQAdd\tdrop\t%d\tlineId\t%d\taction\t[%13s]\tflags1\t0x%x\tseq\t%d\tbch\t%u\n",
                                       DrpCtlWrtQ[i].drop_num, 
                                       DrpCtlWrtQ[i].lineId, 
                                       drp_state_desc[DrpCtlWrtQ[i].action],
                                       DrpCtlWrtQ[i].flags1,
                                       DrpCtlWrtQ[i].seq_num,
                                       DrpCtlWrtQ[i].batch);

                        strcat((char*) &trc_buf[DMMBXBUFID],(char*) &tmp_trc_buf[DMMBXBUFID]);
                    }

                    if (SendIsysMsg(ISYS_DROP_MAINT, (BYTE*) &DrpCtlWrtQ[i].cmd, sizeof(int)*7) == NO_ERRORS)
                        return NO_ERRORS;
                }
            break;

            case ISYS_DELQ:

                // Remove from message queue
                index = *(int*) msg;
                DrpCtlWrtQ[index].flags = ISYS_UNUSEDQ;
                return NO_ERRORS;

            break;

        default:
            RtPrintf("Error i %d file %s, line %d \n", i, _FILE_, __LINE__);
            return ERROR_OCCURED;
            break;
        }
    }
    
    switch(action)
    {
        case ISYS_ADDQ:

            if( (!trc[DMMBXBUFID].buffer_full) && (TraceMask & _DRPMGRQ_ ) )
            {
                sprintf((char*) &tmp_trc_buf[DMMBXBUFID],"ErrQAdd\tdrop\t%d\tlineId\t%d\taction\t[%13s]\tseq\t%d \n",
                               qmsg->drop_num, 
                               qmsg->lineId, 
                               drp_state_desc[qmsg->action], 
                               qmsg->seq_num);

                strcat((char*) &trc_buf[DMMBXBUFID],(char*) &tmp_trc_buf[DMMBXBUFID]);
            }

			// If we got here the queue is full, empty it and see if the condition fixes itself
			for (i = 0; i < ISYS_MAXWQENTRYS;i++)
			{
                DrpCtlWrtQ[i].flags = ISYS_UNUSEDQ;
			}
			RtPrintf("ErrQAdd:Clearing MaintainMsgQ\n");
			MsgHighWaterMark = ISYS_MAXWQENTRYS/2;

            break;

        case ISYS_DELQ:

            index = *(int*) msg;

            if( (!trc[DMMBXBUFID].buffer_full) && (TraceMask & _DRPMGRQ_ ) )
            {
                sprintf((char*) &tmp_trc_buf[DMMBXBUFID],"ErrQDel\tseq_num\t%d\n",index);
                strcat((char*) &trc_buf[DMMBXBUFID],(char*) &tmp_trc_buf[DMMBXBUFID]);
            }

            break;

        default:
            RtPrintf("Error file %s, line %d \n", _FILE_, __LINE__);
            break;
    }
    return ERROR_OCCURED;
}

//--------------------------------------------------------
//  AddMsgQ
//
//  Used for maintence commands only. This routine fills
//  in the structure, requests mutex and calls the routine
//  to add the command.
//--------------------------------------------------------

DropManager::AddMsgQ()
{
    int i, idx;
//    int len = sizeof(TDrpMgrIpcMsg); // currently only copies drop # and action		// RED - Removed because it was initializaed but not referenced

    static TDrpMgrMsgQEntry qmsg;
           TDrpMgrIpcMsg*   cmsg;

    //RtPrintf("AddMsgQ\n");

    cmsg = (TDrpMgrIpcMsg*) &pDrpMgrMsg->Buffer;

    // Build message to send other line
    qmsg.cmd    = cmsg->maint.cmd;
    qmsg.action = cmsg->maint.action;
    qmsg.batch  = cmsg->maint.batch;

    switch(cmsg->maint.extent)
    {
        case isys_send_single:

            // verify index
            idx = cmsg->maint.slave_indx;
            if ( (idx < 0) || (idx >= MAXIISYSLINES) ) 
                return ERROR_OCCURED;
            qmsg.lineId   = idx + 1;
            // verify line id
            if ( (qmsg.lineId < 1) || (qmsg.lineId > MAXIISYSLINES) ) 
                return ERROR_OCCURED;

             if( app->pShm->sys_set.IsysLineSettings.active[idx] &&
                 app->pShm->IsysLineStatus.connected[idx] )
			{
                qmsg.drop_num = cmsg->maint.drop_num; // Remote drop number
                qmsg.flags1   = cmsg->maint.flags;

                if( (!trc[DMMBXBUFID].buffer_full) && (TraceMask & _DRPMGRQ_ ) )
                {
                    sprintf((char*) &tmp_trc_buf[DMMBXBUFID],"AddMsgQ (s)\tidrop\t%d\tlineId\t%d\taction\t[%13s]\n",
                                   qmsg.drop_num, qmsg.lineId, drp_state_desc[qmsg.action]);
                    strcat((char*) &trc_buf[DMMBXBUFID],(char*) &tmp_trc_buf[DMMBXBUFID]);
                }

                MaintainMsgQ( ISYS_ADDQ, (BYTE*) &qmsg );
            }
            else 
                return ERROR_OCCURED;

            break;

        case isys_send_common:

            for ( i = 0; i < MAXIISYSLINES; i++ )
            {
                if (i != app->this_lineid - 1)
                {
                    qmsg.lineId = i + 1;

                    // verify line id
                    if ( (qmsg.lineId < 1) && (qmsg.lineId > MAXIISYSLINES) ) 
                        return ERROR_OCCURED;

                    if (  app->pShm->sys_set.IsysLineSettings.active[i] &&
                          app->pShm->IsysLineStatus.connected[i] )
                    {
                        qmsg.drop_num = app->pShm->sys_set.IsysDropSettings[cmsg->maint.drop_num - 1].drop[i]; 

						qmsg.flags = cmsg->maint.flags;	//RCB - 8-06
						qmsg.flags1 = cmsg->maint.flags;	//RCB - 8-06

                        if( (!trc[DMMBXBUFID].buffer_full) && (TraceMask & _DRPMGRQ_ ) )
                        {
                            sprintf((char*) &tmp_trc_buf[DMMBXBUFID],"AddMsgQ (b)\tdrop\t%d\tidrop\t%d\tflags1\t0x%x\tlineId\t%d\taction\t[%13s]\n",
                                           cmsg->maint.drop_num, qmsg.drop_num, qmsg.flags1,
                                           qmsg.lineId, drp_state_desc[qmsg.action]);
                            strcat((char*) &trc_buf[DMMBXBUFID],(char*) &tmp_trc_buf[DMMBXBUFID]);
                        }

                        MaintainMsgQ( ISYS_ADDQ, (BYTE*) &qmsg );
                    }
                }
            }
            break;

        case isys_send_all:

            // If all drops were going to be affected only send out one
            // command to each line. Just don't put a drop number and 
            // make sure the ExecuteMessage routine does the right thing.

            for ( i = 0; i < MAXIISYSLINES; i++ )
            {
                if (i != app->this_lineid - 1)
                {
                    qmsg.lineId = i + 1;

                    // verify line id
                    if ( (qmsg.lineId < 1) && (qmsg.lineId > MAXIISYSLINES) ) 
                        return ERROR_OCCURED;

                    if ( app->pShm->sys_set.IsysLineSettings.active[i] &&
                         app->pShm->IsysLineStatus.connected[i] )
                    {
                        qmsg.drop_num = isys_send_all_drops;

                        //RtPrintf("AddMsgQ (a)\tdrop\t%d\tidrop\t%d\tlineId\t%d\taction\t[%13s]\n",
                        //                   cmsg->maint.drop_num, qmsg.drop_num, 
                        //                   qmsg.lineId, drp_state_desc[qmsg.action]);

                        if( (!trc[DMMBXBUFID].buffer_full) && (TraceMask & _DRPMGRQ_ ) )
                        {
                            sprintf((char*) &tmp_trc_buf[DMMBXBUFID],"AddMsgQ (a)\tdrop\t%d\tidrop\t%d\tlineId\t%d\taction\t[%13s]\n",
                                           cmsg->maint.drop_num, qmsg.drop_num, 
                                           qmsg.lineId, drp_state_desc[qmsg.action]);
                            strcat((char*) &trc_buf[DMMBXBUFID],(char*) &tmp_trc_buf[DMMBXBUFID]);
                        }

                        MaintainMsgQ( ISYS_ADDQ, (BYTE*) &qmsg );
                    }
                }
            }
            break;

        default:
            break;
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
//  ProcessMbxMsg
//
//  Used for maintence commands only. This routine fills
//  in the structure, requests mutex and calls the routine
//  to add the command.
//--------------------------------------------------------

void DropManager::ProcessMbxMsg()
{
    TDrpMgrIpcMsg* dmsg;
    dmsg = (TDrpMgrIpcMsg*) &pDrpMgrMsg->Buffer;

    // validate message
    if ( (dmsg->maint.cmd <= DRPMGR_MSG_START) || 
         (dmsg->maint.cmd >= LAST_DRPMGR_MSG) )
        return;

    // Due to the volume of IPC messages, these will not go to host
    DebugTrace(_IPC_, "DMgr\tMbx\t[%s]\n", 
					dmgr_msg[dmsg->maint.cmd - DRPMGR_MSG_START]);

    switch(dmsg->maint.cmd)
    {
        case DROP_MAINT:
            dmsg->maint.cmd = ISYS_DROP_MAINT;
            AddMsgQ();
            break;

        case DROP_MAINT_RESP:
            RemoveMsg();
            break;

        case DROP_MAINT_FAIL:
            RemoveFailedMsg();
            break;

        case CHECK_DROP_LOCAL:
            CheckDropMode(CHECK_DROP_LOCAL);
            break;

        case CHECK_DROP_REMOTE:
            CheckDropMode(CHECK_DROP_REMOTE);
            break;

        default:
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            break;
    }

	DebugTrace(_IPC_, "DMgr\tMbx\t[%s]\tOk\n", 
					dmgr_msg[dmsg->maint.cmd - DRPMGR_MSG_START]);

}


//--------------------------------------------------------
//  SearchLoopUtility
//--------------------------------------------------------

int DropManager::SearchLoopUtility(int cmd, int drp_index)
{
    int start_drp_index = drp_index;
    int loops           = 0;
    int i;
//	int LineCnt			= 0;		// RED - Removed because it was initializaed but not referenced
	int	TotalPPMCount	= 0;
	int TotalBchActCnt	= 0;
    __int64 TotalActWeight	= 0;
//	int drop_plus1 = drp_index + 1;		// RED - Removed because it was initializaed but not referenced
	//int start_index = drp_index;

	switch(cmd)
    {
        case clear_drops:
    
            for (;;)
            {
                //if GOOD_INDEX(scale,drp_index)
                //{
                    //RtPrintf("clrdrps drop %d\n", drp_index+1);
                    if (!app->pShm->sys_set.DropSettings[drp_index].Active)
                    { 
                        // Do not clear the batch counts for inactive batched drops
                        switch (app->pShm->Schedule[drp_index].DistMode)
                        {
                            case mode_1:
                            case mode_2:
                            case mode_4_rate:
                                CLEAR_DROP_BATCH(drp_index, app->pShm)
                                CLEAR_ISYS_DROP_BATCH(drp_index, app->pShm)
                                break;
                        }
                    
                    }
                    else
                    {
                        CLEAR_DROP_BATCH(drp_index, app->pShm)
                        CLEAR_ISYS_DROP_BATCH(drp_index, app->pShm)
                    }

                    if DBG_DROP(drp_index, app->pShm)
                    {
                        if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _FDROPS_ ) )
                        { 
                            sprintf((char*) &tmp_trc_buf[MAINBUFID],"Cleared\tdrop\t%d\n",drp_index + 1);
                            strcat((char*) &trc_buf[MAINBUFID], (char*) &tmp_trc_buf[MAINBUFID] );
                        } 
                    }

                    drp_index = app->pShm->Schedule[drp_index].NextEntry - 1;
                //}
                //else 
                //    return ERROR_OCCURED;

                if FINISHED_LOOP(loops)
                    return ERROR_OCCURED;
            }
            break;

        case search_locals:
    
            for (;;)
            {
				if ( DROP_OK(drp_index) )
				{
					for (i = 0; i < MAXIISYSLINES; i++)
					{   
						if ( app->pShm->sys_set.IsysDropSettings[drp_index].active[i] )
						{
							// Add the totals from the isys lines

							TotalPPMCount  += app->pShm->IsysDropStatus[drp_index].PPMCount[i];
							TotalBchActCnt += (int) app->pShm->IsysDropStatus[drp_index].BCount[i];
							TotalActWeight += app->pShm->IsysDropStatus[drp_index].BWeight[i];
						}
					}

					//if ( CheckDropCutoffs( isys_chk_batch_cnt, drop, TotalBchActCnt) )
					if (((app->pShm->Schedule[drp_index].BchCntSp != 0) && (TotalBchActCnt < app->pShm->Schedule[drp_index].BchCntSp)) ||
						((app->pShm->Schedule[drp_index].BchWtSp != 0) && (TotalActWeight < app->pShm->Schedule[drp_index].BchWtSp)))
					{
						// Skip my current drop. I'm here because this drop has batched
						if (drp_index != start_drp_index)
						{
							//RtPrintf("DrpMgr:Search loc found a drop index %d tot %d set %d\n",
							//	drp_index, TotalBchActCnt, app->pShm->Schedule[drp_index].BchCntSp);
							return (drp_index);
						}
					}
				}

				drp_index = app->pShm->Schedule[drp_index].NextEntry - 1;

				// Set back to zero for next drop
				TotalPPMCount = 0;
				TotalBchActCnt = 0;
				TotalActWeight = 0;

                if FINISHED_LOOP(loops)
				{
					//RtPrintf("DrpMgr:Search loc found nothing\n");
                    return ERROR_OCCURED;
				}
            }
            break;

        case search_remotes:
    
            // Mode 6 goes through all the drops above and if all batched or suspended, we arrive here.
            // If remotes haven't batched, this will be a wait. If Mode 6 wasn't batched above, the 
            // suspended flag will clear and continue dropping using search_locals.
            // Mode 7 should be batched completely by this time.

			//DebugTrace(_DEBUG_, "SearchLoopUtility:search_remotes:drp_index %d\n", drp_index);
             
            for (;;)
            {
	            if ( !DROP_OK(drp_index) )
				{
                   // Let search_locals take care of it when suspend goes false
                   //if ( DROP_USABLE(drp_index)   && 
                   //    (app->pShm->sys_stat.DropStatus[drp_index].Suspended != 0) )
                   //    return wait_on_drop;

                   for (i = 0; i < MAXIISYSLINES; i++)
                   {   // Line & drop active, flags not bached, suspended and not this line
                       //RtPrintf("srchrem0 drop %d line %d flgs %x\n",
                       //              drp_index+1, i+1, pShm->IsysDropStatus[drp_index].flags[i]);
					   // RED 05-05-10 added test for not batched but suspended
                       if ( (app->pShm->sys_set.IsysLineSettings.active[i]            != 0) && 
                            (app->pShm->sys_set.IsysDropSettings[drp_index].active[i] != 0) &&
                           (((app->pShm->IsysDropStatus[drp_index].flags[i] & ISYS_DROP_BATCHED)   == 0 ) ||
                           (((app->pShm->IsysDropStatus[drp_index].flags[i] & ISYS_DROP_BATCHED)   == 0 ) &&
                            ((app->pShm->IsysDropStatus[drp_index].flags[i] & ISYS_DROP_SUSPENDED) != 0 ))) &&
                            (app->isys_comm_fail[i] == 0) &&
							 (app->this_lineid - 1 != i) ) 
                       {
                           //RtPrintf("Rem drop %d waiting line %d drop %d flgs %x\n",
                           //         drp_index+1,i+1,
                           //         app->pShm->sys_set.IsysDropSettings[drp_index].drop[i],
                           //         app->pShm->IsysDropStatus[drp_index].flags[i]);
                           return wait_on_drop;
                       }
                   }
				}
				else
				{
					// Skip my current drop. I'm here because this drop has batched
					if (drp_index != start_drp_index)
					{
						//RtPrintf("DrpMgr:Search rem found a drop index %d\n", drp_index);
						return wait_on_drop;
					}
				}
				drp_index = app->pShm->Schedule[drp_index].NextEntry - 1;

                if FINISHED_LOOP(loops)
				{
					//RtPrintf("DrpMgr:Search rem found nothing\n");
                    return ERROR_OCCURED;
				}
            }
            break;

        default:
            RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
            break;
    
			}
    return ERROR_OCCURED;
}
