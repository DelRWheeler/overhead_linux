// Dispatch.h: interface for the Dispatch class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DISPATCH_H__20EC0A01_1CC7_11D6_B444_020000000001__INCLUDED_)
#define AFX_DISPATCH_H__20EC0A01_1CC7_11D6_B444_020000000001__INCLUDED_

#pragma once

class Dispatch
{

private:

    void  initialize();
    int   CreateDispatchThread();
    WORD  DispatchInit();
          ProcessOpen();
          ProcessRead();
          ProcessWrite();
          ProcessGetBufferCount();
          ProcessClose();
          ProcessConfig();
          ProcessGetStatus();
          ProcessComStats();

public:

    Dispatch();
    virtual ~Dispatch();
    static int __stdcall CommDispatchThread(PVOID unused);

};

#endif // !defined(AFX_DISPATCH_H__20EC0A01_1CC7_11D6_B444_020000000001__INCLUDED_)
