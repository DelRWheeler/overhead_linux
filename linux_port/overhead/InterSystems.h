// InterSystems.h: interface for the InterSystems class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_INTERSYSTEMS_H__622F8021_1CC3_11D6_B444_020000000001__INCLUDED_)
#define AFX_INTERSYSTEMS_H__622F8021_1CC3_11D6_B444_020000000001__INCLUDED_

#pragma once

class InterSystems
{

public:

private:
    int    CleanUpWrite(int idx, int action);
    int    CreateISysThread();
    int    CreateMbxServer();
    int    CreateRxServerThread();
    void   initialize();
    int    MessageQueue(int trcbuf, BYTE* msg, int cmd);
    int    OpenDrpMgrMailbox();
    int    postMessage(BYTE* Msg);
    void   ProcessMbxMsg();
    int    SendDrpManager(int cmd, int arg0);
    int    SendLineMsg( int cmd, int line, int var, BYTE *data, int len);
    int    SendHostMsg( int cmd, int var, BYTE *data, int len);
    int    UpdateCommStatus(int status, int index);
    void   UpdateDropInfo(BYTE* drp_msg, int port);

public:
    InterSystems();
    virtual ~InterSystems();

    static int  __stdcall ISysThread(PVOID unused);
    static int RTFCNDCL   Mbx_Server(PVOID unused);
    static int RTFCNDCL   RxServer(PVOID unused);
    static LONG __stdcall IsysMainUnhandledExceptionFilter(EXCEPTION_POINTERS* pExPtrs);
    static LONG __stdcall IsysMbxUnhandledExceptionFilter(EXCEPTION_POINTERS* pExPtrs);
    static LONG __stdcall IsysRxUnhandledExceptionFilter(EXCEPTION_POINTERS* pExPtrs);
    int    ProcessWrites();
    void   ProcessRx(int rdq_indx);
    int    ISysSendDropInfo(int drop_no);
    int    ISysSendCommand(int cmd, int port, BYTE *data, int len);

};

#endif // !defined(AFX_INTERSYSTEMS_H__622F8021_1CC3_11D6_B444_020000000001__INCLUDED_)
