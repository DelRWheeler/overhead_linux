#pragma once

#include "types.h"

// General Purpose Utilities
ULONG   CheckSum (BYTE* pbuf, ULONG len);
void    DumpData (char* msg,   BYTE* pbuf, ULONG len);
void    GetDateStr(char* str);
int     Write_File(ULONG mode, char* fname, BYTE* data_ptr, UINT  len);
int     Read_File (ULONG mode, char* fname, BYTE* data_ptr, UINT* len, UINT method);
bool    FileExists(const char* fname);
void	UdpTraceBuf(char* pTraceBuffer);
void	UdpTraceNBuf(char* pTraceBuffer, int BufLen);
void	UdpPrintf(char* pTraceBuffer, ...);
void	DebugTrace(UINT TraceLvl, char* pTraceBuffer, ...);
void	PrintNBuf(char* pTraceBuffer, int BufLen);
