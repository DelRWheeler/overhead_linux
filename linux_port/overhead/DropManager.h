// DropManager.h: interface for the DropManager class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DROPMANAGER_H__E8519FC3_3828_11D6_B48E_020000000001__INCLUDED_)
#define AFX_DROPMANAGER_H__E8519FC3_3828_11D6_B48E_020000000001__INCLUDED_

#pragma once

//HANDLE          hDropTimer = NULL;
typedef struct TIsysBatchState
{
	int LineIndex;			// The index number of line that is not batched
	int TotalBatchCount;	// Count in single drop of ISYS drop
	int StallCount;			// Number of times the timer fired and the count
							//                 for this drop has not changed
	__int64	TotalBatchWeight;
} TIsysBatchState;

class DropManager
{

private:

    int     CreateDropManagerMbxThread();
    bool    CheckDropCutoffs(int action, int drop, __int64 actCount, bool& StopAll);
    void    CheckDropMode(int check_request);
    int     is_isys_batch_complete(int drop, int mode);
    int     OpenIsysMailbox();
    int     RemoveFailedMsg();
    int     RemoveMsg();
    static int     MaintainMsgQ(int action, void* msg);
    int     AddMsgQ();
    void    ProcessMbxMsg();
    int     SendLineMsg( int cmd, int line, int var, BYTE *data, int len);
    int     SendHostMsg( int cmd, int var, BYTE *data, int len);
    static int     SendIsysMsg(int cmd, BYTE* data, int len);
	HANDLE  Create_Drop_Manager_Timer();
	static void __stdcall Drop_Manager_Timer(PVOID addr);
	HANDLE  hDropTimer;
	int SearchLoopUtility(int cmd, int drp_index);


public:

            DropManager();
    virtual ~DropManager();
    void    initialize();
    int     ExecuteMessage(int drop, int action, int lineId, int flags, int batch);

    static int RTFCNDCL Mbx_Server(PVOID unused);
    static LONG __stdcall dmUnhandledExceptionFilter(EXCEPTION_POINTERS* pExPtrs);

};

#endif // !defined(AFX_DROPMANAGER_H__E8519FC3_3828_11D6_B48E_020000000001__INCLUDED_)
