// General Purpose Utilities
#include "types.h"
#include <pthread.h>
#include <sys/stat.h>

#undef  _FILE_
#define _FILE_      "Utils.cpp"

// Mutex to protect global file_buf from concurrent access by multiple threads
static pthread_mutex_t file_buf_mutex = PTHREAD_MUTEX_INITIALIZER;

//--------------------------------------------------------
//  CheckSum
//--------------------------------------------------------

ULONG CheckSum(BYTE *pbuf, ULONG len)
{
   ULONG i;
   ULONG cs = 0;

   for (i = 0; i < len; i++)
   {
      cs += (unsigned char)(*pbuf++);
   }
   return (cs);
}

//--------------------------------------------------------
//  DumpData
//--------------------------------------------------------

void DumpData(char* msg, BYTE *pbuf, ULONG len)
{
   ULONG i;
   int   cnt = 0;

   BYTE cs = 0;

   RtPrintf("Dump %s\n",msg);
   for (i = 0; i < len; i++)
   {
      RtPrintf("%2.2x ",*pbuf);

      cs = BYTE(cs + *pbuf++);
      cnt++;
      if (!(cnt & 0xf) && (cnt != 0)) RtPrintf("\n");

   }
   RtPrintf("\n");
}

//--------------------------------------------------------
//   GetDateStr:
//--------------------------------------------------------

void GetDateStr(char* str)
{
    time_t long_time;
    struct tm *ptime;
    char   am_pm[] = "AM";

    time( &long_time );
    ptime = localtime( &long_time );

    if( ptime->tm_hour > 12 ) strcpy( am_pm, "PM" );

    sprintf(str,"%.19s %s ", asctime( ptime ), am_pm );
}

//--------------------------------------------------------
//   FileExists: Check if a file exists
//--------------------------------------------------------

bool FileExists(const char* fname)
{
    if (!fname) {
        RtPrintf("ERROR: FileExists called with NULL filename!\n");
        return false;
    }
    struct stat st;
    return (stat(fname, &st) == 0);
}

//--------------------------------------------------------
//   Read_File:
//--------------------------------------------------------

int Read_File(ULONG mode, char* fname, BYTE* data_ptr,  UINT* len, UINT method)
{
    static      HANDLE hFile;
    ULONG       rw_cnt;
    ULONG       csum;
    ULONG*      psum;
    int         status      = NO_ERRORS;

    if (*len > MAXFILEBUFFER) return ERROR_OCCURED;

    pthread_mutex_lock(&file_buf_mutex);

    hFile = CreateFile( fname,
                        GENERIC_READ,
                        0, NULL, mode,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);

    if (hFile != INVALID_HANDLE_VALUE)
    {
        if (method == LEN_UNKNOWN)
        {
            ReadFile(hFile, file_buf, 0xFFFF, &rw_cnt, NULL);
            // Validate: file must contain at least a checksum
            if (rw_cnt < sizeof(csum))
            {
                RtPrintf("Read_File: file too small (%u bytes), file %s\n", rw_cnt, fname);
                CloseHandle(hFile);
                pthread_mutex_unlock(&file_buf_mutex);
                return ERROR_OCCURED;
            }
            *len = rw_cnt - sizeof(csum);
        }
        else
            ReadFile(hFile, file_buf, *len + sizeof(csum), &rw_cnt, NULL);

        // check read length
        if (rw_cnt == *len + sizeof(csum))
        {
            csum = CheckSum((BYTE*) file_buf, *len);

            psum = (ULONG*) &file_buf[*len];

            if (csum == *psum)
            {
                memcpy(data_ptr, file_buf, *len);
            }
            else
            {
                RtPrintf("Error read %s file %s, line %d cs %x fcs %x\n",
                          fname, _FILE_, __LINE__, csum, (int) file_buf[*len]);
                status = ERROR_OCCURED;
            }

        }
        else
        {
            RtPrintf("Error file %s, line %d r_cnt %u len %u\n",
                      _FILE_, __LINE__,rw_cnt,*len + sizeof(csum));
            status = ERROR_OCCURED;
        }
    }
    else
    {
        DWORD err = GetLastError();
        // Error 2 (file not found) with OPEN_EXISTING is normal on first boot - don't log as error
        if (err != 2 || mode != OPEN_EXISTING)
            RtPrintf("Read file error %u file %s\n", err, fname);
        status = ERROR_OCCURED;
    }

    CloseHandle(hFile);
    pthread_mutex_unlock(&file_buf_mutex);
    return status;
}

//--------------------------------------------------------
//   Write_File:
//--------------------------------------------------------

int Write_File(ULONG mode, char* fname, BYTE* data_ptr, UINT len)
{
    static      HANDLE hFile;
    ULONG       rw_cnt;
    ULONG       csum;
    ULONG*      psum;
    int         status = NO_ERRORS;

    if (len > MAXFILEBUFFER) return ERROR_OCCURED;

    pthread_mutex_lock(&file_buf_mutex);

    hFile = CreateFile( fname,
                        GENERIC_READ |
                        GENERIC_WRITE,
                        0, NULL, mode,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);

    if (hFile != INVALID_HANDLE_VALUE)
    {
        if (mode == OPEN_EXISTING)
        {
            ReadFile( hFile, file_buf, 0xFFFFFFFF, &rw_cnt, NULL);

            // Validate: existing file must have at least a checksum
            if (rw_cnt < sizeof(csum))
            {
                // File empty or corrupt - treat as new file (just write data)
                RtPrintf("Write_File: existing file too small (%u bytes), writing fresh, file %s\n", rw_cnt, fname);
                memcpy(file_buf, data_ptr, len);
            }
            else
            {
                ULONG existing_data_len = rw_cnt - sizeof(csum);

                // Bounds check: existing data + new data must fit in buffer
                if (existing_data_len + len > MAXFILEBUFFER)
                {
                    RtPrintf("Write_File: append would overflow (%u + %u > %u), file %s\n",
                             existing_data_len, len, (ULONG)MAXFILEBUFFER, fname);
                    CloseHandle(hFile);
                    pthread_mutex_unlock(&file_buf_mutex);
                    return ERROR_OCCURED;
                }

                memcpy(&file_buf[existing_data_len], data_ptr, len);
                len += existing_data_len;
            }
            SetFilePointer( hFile,0,0,FILE_BEGIN);
        }
        else
            memcpy(file_buf, data_ptr, len);

        csum     = CheckSum((BYTE*) file_buf, len);
        psum     = (ULONG*) &file_buf[len];
        *psum    = csum;

        WriteFile(hFile, file_buf, len + sizeof(csum), &rw_cnt, NULL);
    }
    else
    {
        RtPrintf("Write file error %u file %s\n", GetLastError(), fname);
        status = ERROR_OCCURED;
    }

    CloseHandle(hFile);
    pthread_mutex_unlock(&file_buf_mutex);
    return status;
}

//---------------------------------------------------------------
//  DebugTrace -
//				Debug Tracing based on Trace setting from Front-End
//				Formats string and prints to UDP port not to
//				exceed MAXERRMBUFSIZE bytes
//---------------------------------------------------------------
void DebugTrace(UINT TraceLvl, char* pTraceBuffer, ...)
{
	va_list ap;
	char* pBuffer;
	int Rc = 0;

	if( TraceLvl & TraceMask )
	{

		if (UDP_ENABLE & app->pShm->dbg_set.UdpTrace)
		{
			if (pTraceBuffer != NULL)
			{
				pBuffer = (char*)RtAllocateLockedMemory(MAXERRMBUFSIZE);
				memset((void *)pBuffer, 0, MAXERRMBUFSIZE);
				va_start(ap, pTraceBuffer);
				Rc = vsprintf(pBuffer, pTraceBuffer, ap);
				va_end(ap);

				if (Rc > 0)
				{
#ifdef	_TELNET_UDP_ENABLED_
					UdpTraceBuf(pBuffer);
#else
					RtPrintf(pBuffer);
#endif
				}
				RtFreeLockedMemory(pBuffer);
			}
		}
		else
		{
			if (pTraceBuffer != NULL)
			{
				pBuffer = (char*)RtAllocateLockedMemory(MAXERRMBUFSIZE);
				va_start(ap, pTraceBuffer);
				Rc = vsprintf(pBuffer, pTraceBuffer, ap);
				va_end(ap);

				if (Rc > 0)
				{
					RtPrintf(pBuffer);
				}
				RtFreeLockedMemory(pBuffer);
			}
		}
	}

}

//---------------------------------------------------------------
//  UdpPrintf - Formats string and prints to UDP port
//			      not to exceed MAXERRMBUFSIZE bytes
//---------------------------------------------------------------
void UdpPrintf(char* pTraceBuffer, ...)
{
	va_list ap;
	char* pBuffer;
	int Rc = 0;

	if (pTraceBuffer != NULL)
	{
		pBuffer = (char*)RtAllocateLockedMemory(MAXERRMBUFSIZE);
		memset((void *)pBuffer, 0, MAXERRMBUFSIZE);
		va_start(ap, pTraceBuffer);
		Rc = vsprintf(pBuffer, pTraceBuffer, ap);
		va_end(ap);
		if (Rc > 0)
		{
			UdpTraceBuf(pBuffer);
		}
		RtFreeLockedMemory(pBuffer);
	}

}

//---------------------------------------------------------------
//  UdpTraceBuf - Print out to UDP port buffer of variable length
//			      not to exceed MAXERRMBUFSIZE bytes
//---------------------------------------------------------------
void UdpTraceBuf(char* pTraceBuffer)
{
	static int TraceCnt = 0;
	static int TraceId = 0;

	// Write to the Trace memory area to be sent over UDP
	strcpy(app->pTraceMemory->UdpTraceData[TraceCnt].TraceString, pTraceBuffer);
	app->pTraceMemory->UdpTraceData[TraceCnt].TraceId = TraceCnt + 1;
	app->pTraceMemory->LatestEntry = TraceCnt;
	TraceCnt++;
	TraceId++;

	if (TraceCnt >= (MAXTRCMBUFSIZE -1))
	{
		TraceCnt = 0;
	}

}


//---------------------------------------------------------------
//  UdpNTraceBuf - Print out to UDP port buffer of length BufLen
//---------------------------------------------------------------
void UdpTraceNBuf(char* pTraceBuffer, int BufLen)
{
	static int TraceCnt = 0;
	static int TraceId = 0;
	int i = 0;
	int Width = 0;
	char Buffer[MAXERRMBUFSIZE];

	for (i = 0; i < BufLen; i += Width)
	{
		Width = BufLen - i;
		if (Width > MAXERRMBUFSIZE)
		{
			Width = MAXERRMBUFSIZE - 1;
		}
		memset(&Buffer[0], 0, MAXERRMBUFSIZE);
		strncpy(&Buffer[0],	(char *)&pTraceBuffer[i], Width);
		UdpTraceBuf(&Buffer[0]);
	}
}


//---------------------------------------------------------------
//  PrintNBuf - Print out to console buffer of length BufLen
//---------------------------------------------------------------
void PrintNBuf(char* pTraceBuffer, int BufLen)
{
	int i = 0;
	int Width = 0;
	char Buffer[MAXERRMBUFSIZE];

	for (i = 0; i < BufLen; i += Width)
	{
		Width = BufLen - i;
		if (Width > MAXERRMBUFSIZE)
			{
			Width = MAXERRMBUFSIZE - 1;
			}
		memset(&Buffer[0], 0, MAXERRMBUFSIZE);
		strncpy(&Buffer[0], (char *)&pTraceBuffer[i], Width);
		RtPrintf(&Buffer[0]);
	}

}
