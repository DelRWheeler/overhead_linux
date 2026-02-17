//////////////////////////////////////////////////////////////////////
// SPDLoadCell.cpp: implementation of the SPDLoadCell class.
//
// Group Four Transducers SPD (LDB151/LDB152) load cell driver.
// Drop-in replacement for HBMLoadCell - same LoadCell base class,
// same measurement queue interface, same thread structure.
//
// Protocol differences from HBM FIT7:
//   HBM: Binary 4-byte packets (3 data + 1 XOR checksum), 38400 baud
//   SPD: ASCII text lines (e.g., "S+0125785\r\n"), 115200 baud
//
//   HBM commands: "CMD?;" or "CMD value;" (semicolon terminated)
//   SPD commands: "CMD\r\n" or "CMD value\r\n" (CR/LF terminated)
//
//   HBM continuous start: "MSV?0;"  HBM stop: "STP;"
//   SPD continuous start: "SX\r\n"  SPD stop: any new command stops stream
//
// SPD measurement output format (SX command = raw ADC samples):
//   "S+0125785\r\n"  or  "S-0000500\r\n"
//   Byte 0: 'S' (sample prefix)
//   Byte 1: '+' or '-' (sign)
//   Bytes 2-8: 7-digit integer value (ADC counts)
//   Bytes 9-10: CR LF
//
//////////////////////////////////////////////////////////////////////

#include "types.h"
#undef  _FILE_
#define _FILE_      "SPDLoadCell.cpp"

extern SPDLoadCell::SPDinit *	pSPDLoadCellInit1;
extern SPDLoadCell::SPDinit *	pSPDLoadCellInit2;
extern SPDLoadCell::SPDinit *	pSPDLoadCellInit3;
extern SPDLoadCell::SPDinit *	pSPDLoadCellInit4;
extern char app_err_buf[];

#define MEASURE_MODE_SLEEP	2
#define CMD_MODE_SLEEP		2
#define MISC_MODE_SLEEP		100
#define LOOP_CNT_MAX		0xFFFFFFFF
#define RETRY_CNT_MAX		0x000003E8

extern	int	TakeLoadCellReadsFlag;
extern	int ReadsToSample;
extern	int WriteLCReadsToFile;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SPDLoadCell::SPDLoadCell()
{
	int i = 0;

    this->NumCH = 1;

	this->ContMeasOut = false;
	this->blnMeas     = false;
	this->bFirstCheck = true;
	this->num_adc_reads[0] = 5;

	// Initialize line accumulation buffer
	this->lineIdx = 0;
	memset(this->lineBuf, 0, SPD_LINE_BUFFER_SIZE);

	// Initialize queue indices
	this->MeasQ_Idx = -1;

    for (i = 0; i < SPD_MEASQ_MAX_ITEMS; i++)
	{
		this->MeasQ[i].meas = 0;
		this->MeasQ[i].state = false;
	}
}

SPDLoadCell::~SPDLoadCell()
{
}

void SPDLoadCell::initialize(SPDinit* pSPDInit)
{
	char	sBuffer[80];

	this->serialObj = pSPDInit->pSerial;

	sprintf(sBuffer, "Ser_Port%d", this->serialObj->ucbObj.port);
	DebugTrace(_HBMLDCELL_, "SPDLoadCell::initialize - Using %s\n", (char*)&sBuffer[0]);

	// Create serial port Mutex
	this->hSerMtx = RtCreateMutex(NULL, FALSE, (LPCTSTR)&sBuffer[0]);

	if (this->hSerMtx == NULL)
	{
		RtPrintf("RtCreateMutex for serial port failed file %s, line %d \n", _FILE_, __LINE__);
	}

	RtPrintf("SPDLoadCell::initializing::LoadCellNum = %d\n", this->LoadCellNum);

	switch (this->LoadCellNum)
	{
	case SPD_LOADCELL_1:
		pSPDLoadCellInit1->phSerMtx = (HANDLE*)this->hSerMtx;
		if ((this->CreateSPDLoadCellThread(pSPDInit) != NO_ERRORS))
			return;
		break;

	case SPD_LOADCELL_2:
		pSPDLoadCellInit2->phSerMtx = (HANDLE*)this->hSerMtx;
		if ((this->CreateSPDLoadCellThread(pSPDInit) != NO_ERRORS))
			return;
		break;

	case SPD_LOADCELL_3:
		pSPDLoadCellInit3->phSerMtx = (HANDLE*)this->hSerMtx;
		if ((this->CreateSPDLoadCellThread(pSPDInit) != NO_ERRORS))
			return;
		break;

	case SPD_LOADCELL_4:
		pSPDLoadCellInit4->phSerMtx = (HANDLE*)this->hSerMtx;
		if ((this->CreateSPDLoadCellThread(pSPDInit) != NO_ERRORS))
			return;
		break;

	default:
		return;
	}

	// Create Measurement Queue Mutex
	this->hMeasQ = RtCreateMutex(NULL, FALSE, (LPCTSTR)"LC_Meas");
	if (this->hMeasQ == NULL)
	{
		RtPrintf("RtCreateMutex for measurement queue file %s, line %d \n", _FILE_, __LINE__);
	}
}

//--------------------------------------------------------
//   CreateSPDLoadCellThread
//--------------------------------------------------------

int SPDLoadCell::CreateSPDLoadCellThread(SPDinit* pSPDInit)
{
    DWORD threadId;
    DWORD dwStackSize = 0x4000;
    DWORD dwExitCode  = 0;

	hSPDLc = NULL;

	switch(pSPDInit->pSPDLoadCell->LoadCellNum)
	{
	case SPD_LOADCELL_1:
		hSPDLc = CreateThread(NULL, dwStackSize,
			(LPTHREAD_START_ROUTINE) SPDLoadCellThread1,
			(PVOID) pSPDInit, 0, (LPDWORD) &threadId);
		break;

	case SPD_LOADCELL_2:
		hSPDLc = CreateThread(NULL, dwStackSize,
			(LPTHREAD_START_ROUTINE) SPDLoadCellThread2,
			(PVOID) pSPDInit, 0, (LPDWORD) &threadId);
		break;

	case SPD_LOADCELL_3:
		hSPDLc = CreateThread(NULL, dwStackSize,
			(LPTHREAD_START_ROUTINE) SPDLoadCellThread3,
			(PVOID) pSPDInit, 0, (LPDWORD) &threadId);
		break;

	case SPD_LOADCELL_4:
		hSPDLc = CreateThread(NULL, dwStackSize,
			(LPTHREAD_START_ROUTINE) SPDLoadCellThread4,
			(PVOID) pSPDInit, 0, (LPDWORD) &threadId);
		break;
	}

    if(hSPDLc == NULL)
    {
        RtPrintf("Error file %s, line %d \n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    if(!RtSetThreadPriority(hSPDLc, RT_PRIORITY_MIN+50))
    {
        RtPrintf("Error file %s, line %d \n", _FILE_, __LINE__);
        TerminateThread(hSPDLc, dwExitCode);
        return ERROR_OCCURED;
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
//  SPD Load Cell Thread - common body
//  Each thread opens COM port at 115200, initializes SPD,
//  then loops reading ASCII measurement lines.
//--------------------------------------------------------

static void SPDLoadCellThreadBody(SPDLoadCell::SPDinit* pSPDInit, int loadCellIdx, const char* eventName)
{
	WORD rc = FALSE;
	ULONG LoopCnt = 0;

	Serial* serObj;
	SPDLoadCell* pSPDcObj;
	HANDLE hSerialQ;

	serObj   = pSPDInit->pSerial;
	pSPDcObj = pSPDInit->pSPDLoadCell;
	hSerialQ = (HANDLE*)pSPDInit->phSerMtx;

	pSPDcObj->cPrintCnt      = 0;
	pSPDcObj->ParseErrorCount = 0;

	pSPDcObj->LCReadCounter  = 0;
	pSPDcObj->StartLCReads   = false;
	pSPDcObj->CurrentRead    = 0;
	pSPDcObj->FirstTimeThru  = true;

	hLoadCellInitialized[loadCellIdx] = RtOpenEvent(NULL, NULL, eventName);

	if (hLoadCellInitialized[loadCellIdx] == NULL)
	{
		RtPrintf("Failed to open event -%s- \n", eventName);
	}

	//----- Open communications port at 115200 baud -----
	DebugTrace(_HBMLDCELL_, "SPD: Opening COMPORT%d at 115200 baud . . .\n", serObj->ucbObj.port + 1);

	if (RtWaitForSingleObject(hSerialQ, 100) != WAIT_OBJECT_0)
	{
		RtPrintf("hSerMtx RtWaitForSingleObject failed in file %s, line %d \n", _FILE_, __LINE__);
	}
	else
	{
		// SPD default baud rate is 115200 (vs HBM's 38400)
		if ((rc = serObj->RtOpenComPort(SPD_DEFAULT_BAUD_RATE, COM_WORDSIZE_8,
                         COM_STOPBITS_1, COM_PARITY_NONE, COM_FLOW_CONTROL_HARDWARE)) != NO_ERRORS)
        {
            RtPrintf("Error COM%d Open rc = %d file %s, line %d \n", serObj->ucbObj.port + 1,
					rc, _FILE_, __LINE__);
            return;
        }

		if (!RtReleaseMutex(hSerialQ))
		{
			RtPrintf("Release failed in file %s, line %d \n", _FILE_, __LINE__);
		}
    }

	pSPDcObj->init_adc(AVG, 2);

	if (!RtSetEvent(hLoadCellInitialized[loadCellIdx]))
    {
        RtPrintf("Error %u file %s, line %d \n", GetLastError(), _FILE_, __LINE__);
    }

	///////////////////////////////////////////////////////////////////////////
	//                           Main Loop
	///////////////////////////////////////////////////////////////////////////

	for(;;)
    {
		// SPD doesn't have the 21-setting check that HBM does.
		// The SPD settings are much simpler and don't need periodic polling.
		// If we need to add periodic setting verification later, it goes here.

		LoopCnt++;

        // Read measurements
        if (pSPDcObj->ContMeasOut)
		{
            pSPDcObj->SerialRead(serObj);
			Sleep(MEASURE_MODE_SLEEP);
		}
		else
		{
			Sleep(MISC_MODE_SLEEP);
		}
    }
}

int RTFCNDCL SPDLoadCell::SPDLoadCellThread1(PVOID pSPDInit)
{
	SPDLoadCellThreadBody((SPDLoadCell::SPDinit*)pSPDInit, SPD_LOADCELL_1, "Load Cell 1 Initialized");
	return 0;
}

int RTFCNDCL SPDLoadCell::SPDLoadCellThread2(PVOID pSPDInit)
{
	SPDLoadCellThreadBody((SPDLoadCell::SPDinit*)pSPDInit, SPD_LOADCELL_2, "Load Cell 2 Initialized");
	return 0;
}

int RTFCNDCL SPDLoadCell::SPDLoadCellThread3(PVOID pSPDInit)
{
	SPDLoadCellThreadBody((SPDLoadCell::SPDinit*)pSPDInit, SPD_LOADCELL_3, "Load Cell 3 Initialized");
	return 0;
}

int RTFCNDCL SPDLoadCell::SPDLoadCellThread4(PVOID pSPDInit)
{
	SPDLoadCellThreadBody((SPDLoadCell::SPDinit*)pSPDInit, SPD_LOADCELL_4, "Load Cell 4 Initialized");
	return 0;
}

//===========================================================================
//
//  init_adc - Initialize SPD load cell
//
//  SPD initialization is much simpler than HBM:
//    1. Flush any stale data
//    2. Query identity (ID command)
//    3. Configure filter settings (FL, FM, UR)
//    4. Start continuous ADC sample output (SX command)
//
//  No need for COF/CSM/STR commands - SPD uses ASCII natively.
//
//===========================================================================

int SPDLoadCell::init_adc(int mode, int num_reads)
{
	int i;
    int cntFlush = 0;
	bool Config = true;

    this->blnOutputStopped = false;

	// Flush any stale data in the receive buffer
	this->serialObj->RtGetComBufferCount((WORD*)&i);

	while ((i != 0) && (cntFlush++ <= 5))
	{
		this->FlushComInBuffer();
		Sleep(10);
		this->serialObj->RtGetComBufferCount((WORD*)&i);
	}

	// Initialize ADC settings - same pattern as HBM
    for(i = 0; i < SPD_NUM_CH; i++)
    {
        this->adc_offset   [i] = ADCOFFSET;
        this->num_adc_reads[i] = num_reads;
        this->adc_mode     [i] = mode;
        this->error_sent[i]    = false;

        this->set_adc_num_reads(i, num_adc_reads[i]);
        this->set_adc_offset   (i, adc_offset   [i]);
        this->set_adc_mode     (i, adc_mode     [i]);
    }

    if (this->ContMeasOut)
    {
        this->StopContinuousOutput();
    }

	// Read device identification
    this->read_adc_pn();
    this->read_adc_version();

	// Copy part and version number info to shared memory
    for(i = 0; i < ADCINFOMAX; i++)
    {
        app->pShm->sys_stat.adc_part[i]    = this->left_adc_pn[i];
        app->pShm->sys_stat.adc_version[i] = this->left_adc_version[i];
    }

	// Configure SPD filter settings
	// FL 3 = Filter cutoff level 3 (default, good general purpose)
	// FM 0 = IIR filter mode (faster response than FIR)
	// UR 1 = Update rate divider 1 (output rate = 1172/2^1 = 586 Hz, close to HBM's ~600 Hz)
	this->SendCommand("FL 3\r\n", 3000);
	this->SendCommand("FM 0\r\n", 3000);
	this->SendCommand("UR 1\r\n", 3000);

	// Save settings to SPD EEPROM so they persist across power cycles
	this->SendCommand("WP\r\n", 3000);

	// Start continuous ADC sample output
	// SX command streams raw ADC counts: "S+0125785\r\n"
    this->StartContinuousOutput();

    if (Config == true)
	{
		RtPrintf("SPD LoadCell %d initialized successfully\n", this->LoadCellNum);
		return(TRUE);
	}
	else
	{
		sprintf(app_err_buf, "SPDLoadCell::init_adc - Error in SPD LoadCell Configuration\n");
	    app->GenError(warning, app_err_buf);
		return(FALSE);
	}
}

//===========================================================================
//
//  ParseMeasurementLine - Parse an ASCII measurement line from the SPD
//
//  SX output format: "S+0125785\r\n" (raw ADC sample)
//    Byte 0: type prefix ('S'=sample, 'G'=gross, 'N'=net)
//    Byte 1: sign ('+' or '-')
//    Bytes 2+: integer digits
//
//  Returns parsed value as __int64, or 0 on parse error.
//
//===========================================================================

__int64 SPDLoadCell::ParseMeasurementLine(char* line, int len)
{
	__int64 value = 0;
	int sign = 1;
	int startIdx = 0;

	if (len < 3)
	{
		this->ParseErrorCount++;
		return 0;
	}

	// First character is the type prefix (S, G, N, etc.)
	// Skip it - we just want the numeric value
	char prefix = line[0];
	if (prefix == 'S' || prefix == 'G' || prefix == 'N' ||
	    prefix == 's' || prefix == 'g' || prefix == 'n')
	{
		startIdx = 1;
	}

	// Check for sign
	if (startIdx < len)
	{
		if (line[startIdx] == '-')
		{
			sign = -1;
			startIdx++;
		}
		else if (line[startIdx] == '+')
		{
			startIdx++;
		}
	}

	// Parse digits - for SX mode these are pure integer ADC counts
	// For SG/SN mode there may be a decimal point (calibrated weight)
	// We parse as integer regardless - the controller works in ADC counts
	for (int i = startIdx; i < len; i++)
	{
		char c = line[i];
		if (c >= '0' && c <= '9')
		{
			value = value * 10 + (c - '0');
		}
		else if (c == '.')
		{
			// Skip decimal point - we only care about the integer part
			// For SX mode (raw ADC), there is no decimal point
			// For SG/SN mode, truncating at the decimal is fine
			// since the controller works in raw ADC counts anyway
			break;
		}
		else if (c == '\r' || c == '\n')
		{
			// End of value
			break;
		}
		else
		{
			// Unexpected character - parse error
			this->ParseErrorCount++;
			return 0;
		}
	}

	return value * sign;
}

//===========================================================================
//
//  SerialRead - Read data from serial port
//
//  In command mode: read character-by-character until CR/LF (same as HBM)
//  In measurement mode: accumulate ASCII lines, parse each complete line
//    as an ADC sample value and push to measurement queue
//
//  This replaces the HBM's binary 4-byte packet parser with an ASCII
//  line parser. The measurement queue interface is identical.
//
//===========================================================================

void SPDLoadCell::SerialRead(Serial* serial)
{
	serial; // suppress unused parameter warning
    WORD    rc, bytes_read;
    WORD    buf_bytes   = 0;
    int     rxbuffidx   = 0;
	int     RetryCnt    = 0;

	this->blnResponseReceived = false;

	if (RtWaitForSingleObject(this->hSerMtx, 100) != WAIT_OBJECT_0)
	{
		RtPrintf("hSerMtx RtWaitForSingleObject failed in file %s, line %d \n", _FILE_, __LINE__);
	}
	else
	{
	    rc = this->serialObj->RtGetComBufferCount(&buf_bytes);

        memset((char*)&this->rxmsg[0], 0, SPD_BUFFER_SIZE);

        if (this->blnCmdSent && !blnMeas)
        {
			//----------------------------------------------------------
			// Command response mode - read until CR/LF
			// This is the same approach as HBM - SPD also terminates
			// responses with CR/LF
			//----------------------------------------------------------
			while ((!this->blnResponseReceived) && (RetryCnt < RETRY_CNT_MAX))
	        {
				RetryCnt++;

                if (buf_bytes > 0)
                {
                    rc = this->serialObj->RtReadComPort((BYTE*)&this->rxmsg[rxbuffidx], 1, &bytes_read);
					if(this->rxmsg[rxbuffidx] == 0)
					{
						rc = this->serialObj->RtGetComBufferCount(&buf_bytes);
						Sleep(1);
						continue;
					}
					rxbuffidx++;
				}

                // Look for CRLF terminator
                if (rxbuffidx > 1)
                {
                    if ((this->rxmsg[rxbuffidx - 1] == 0x0A) && (this->rxmsg[rxbuffidx - 2] == 0x0D))
                    {
                        this->blnResponseReceived = true;
                    }
                }

                rc = this->serialObj->RtGetComBufferCount(&buf_bytes);
                Sleep(1);

				if (rxbuffidx >= SPD_BUFFER_SIZE)
                {
                    RtPrintf("SPD Serial Read Rx Buffer Full\n");
					break;
                }
	        }
		}
	    else if (this->blnMeas && (buf_bytes > 0))
	    {
			//----------------------------------------------------------
			// Measurement mode - ASCII line accumulation
			//
			// SPD sends continuous ASCII lines like "S+0125785\r\n"
			// We accumulate characters into lineBuf until we see CR/LF,
			// then parse the complete line and push to measurement queue.
			//
			// This replaces the HBM's 4-byte binary packet parsing.
			// The measurement queue (MeasureQ) interface is identical.
			//----------------------------------------------------------
			__int64 tmpMeas = 0;
			BYTE tmpByte;
			WORD bytesAvail = buf_bytes;

			while (bytesAvail > 0)
			{
				// Read one byte at a time for line accumulation
				rc = this->serialObj->RtReadComPort(&tmpByte, 1, &bytes_read);
				if (bytes_read != 1) break;
				bytesAvail--;

				if (tmpByte == 0x0A) // LF = end of line
				{
					// We have a complete measurement line
					if (this->lineIdx > 2) // minimum valid: "S+0" = 3 chars
					{
						// Null-terminate for parsing
						this->lineBuf[this->lineIdx] = '\0';

						// Parse the ASCII measurement value
						tmpMeas = this->ParseMeasurementLine(this->lineBuf, this->lineIdx);

						// Write to the measurement queue (same interface as HBM)
						this->MeasureQ(&tmpMeas, 0);

						// Support load cell read sampling (debug feature)
						if (TakeLoadCellReadsFlag > 0)
						{
							this->SampleLCReadMeasureQ(&tmpMeas);
						}
					}

					// Reset line buffer for next measurement
					this->lineIdx = 0;
				}
				else if (tmpByte == 0x0D) // CR = skip (part of CRLF)
				{
					// Don't add CR to the line buffer, just skip it
				}
				else
				{
					// Accumulate character into line buffer
					if (this->lineIdx < SPD_LINE_BUFFER_SIZE - 1)
					{
						this->lineBuf[this->lineIdx++] = (char)tmpByte;
					}
					else
					{
						// Line buffer overflow - something is wrong, reset
						RtPrintf("SPD line buffer overflow, resetting\n");
						this->lineIdx = 0;
					}
				}

				// Check if more bytes are available
				if (bytesAvail == 0)
				{
					this->serialObj->RtGetComBufferCount(&bytesAvail);
				}
			}

			// Periodic debug output
			if (_HBMLDCELL_ & TraceMask)
			{
				this->cPrintCnt++;
				if ((this->cPrintCnt % 10000) == 0)
				{
					this->serialObj->RtGetComBufferCount(&bytesAvail);
					DebugTrace(_HBMLDCELL_,
						"SPD SerialRead bytes in buffer = %d ParseErrors = %d Port = %d\n",
						bytesAvail, this->ParseErrorCount,
						this->serialObj->ucbObj.port);
				}
			}
	    }

		// Release the mutex
		if (!RtReleaseMutex(this->hSerMtx))
		{
			RtPrintf("Release failed in file %s, line %d \n", _FILE_, __LINE__);
		}
    }
}

//===========================================================================
//
//  SerialWrite - Write command to the SPD
//
//  SPD commands are 2-letter commands terminated with CR/LF.
//  The caller must include "\r\n" in the command string.
//
//===========================================================================

int SPDLoadCell::SerialWrite(char* txdata)
{
	WORD rc = NO_ERRORS;
	WORD bytes_wrtn;

	if (RtWaitForSingleObject(this->hSerMtx, 100) != WAIT_OBJECT_0)
	{
		RtPrintf("hSerMtx RtWaitForSingleObject failed in file %s, line %d \n", _FILE_, __LINE__);
	}
	else
	{
        DebugTrace(_HBMLDCELL_, "SPD Port %d Out: %s", this->serialObj->ucbObj.port, txdata);
	    rc = this->serialObj->RtWriteComPort((BYTE*)txdata, (unsigned short)(strlen(txdata)), &bytes_wrtn);

	    if (rc != NO_ERRORS)
		    RtPrintf("SPD Error rc = %d file %s, line %d \n", rc, _FILE_, __LINE__);

		if (!RtReleaseMutex(this->hSerMtx))
		{
			RtPrintf("Release failed in file %s, line %d \n", _FILE_, __LINE__);
		}
    }
    return 1;
}

//===========================================================================
//
//  SendCommand - Send a command and wait for a response
//
//  Stops continuous output if active, sends the command, waits for
//  the CR/LF-terminated response.
//
//===========================================================================

int SPDLoadCell::SendCommand(char* cmd, DWORD response_time)
{
	int rc = ERROR_OCCURED;
	unsigned int ResponseCnt = 0;

	if (this->blnCmdSent == true)
	{
		return(ERROR_OCCURED);
	}

    if (this->ContMeasOut)
    {
        this->StopContinuousOutput();
    }

    // Send the command
    this->SerialWrite(cmd);
    this->blnCmdSent = true;
	this->blnResponseReceived = false;

    // Wait for a response if response_time > 0
    if (response_time > 0)
    {
		while ((ResponseCnt < response_time) && (this->blnResponseReceived != true))
		{
			this->SerialRead(this->serialObj);
			ResponseCnt += RETRY_CNT_MAX;

			if ((ResponseCnt >= response_time) && (this->blnResponseReceived != true))
			{
				this->blnCmdSent = false;
				RtPrintf("SPD Timeout in serial response for command %s", cmd);
				break;
			}
		}

		if (this->blnResponseReceived == true)
		{
			rc = NO_ERRORS;
			DebugTrace(_HBMLDCELL_, "SPD Port %d SendCommand::received->%s\n",
				this->serialObj->ucbObj.port, (char*)&this->rxmsg[0]);
		}
    }

    this->blnCmdSent = false;

    // Check if this was a continuous output command
	// SPD: "SX\r\n" starts continuous ADC sample output
	// Any other command sent while streaming will stop it
	if (strncmp(cmd, "SX", 2) == 0 || strncmp(cmd, "SG", 2) == 0 || strncmp(cmd, "SN", 2) == 0)
	{
		this->ContMeasOut = true;
		this->blnMeas = true;
		DebugTrace(_HBMLDCELL_, "SPD Continuous Output Mode Enabled\n");
	}
	else
	{
		this->blnMeas = false;
	}

    return rc;
}

//===========================================================================
//
//  StartContinuousOutput / StopContinuousOutput
//
//  SPD: "SX\r\n" starts streaming raw ADC samples
//  Any new command sent will stop the stream (no explicit stop command needed)
//  We use "GS\r\n" (get single sample) as a benign command to stop streaming
//
//===========================================================================

int SPDLoadCell::StopContinuousOutput()
{
	// Send a single-shot command to stop the continuous stream
	// GS = Get Sample (single reading) - this stops the stream and returns one value
	this->SerialWrite("GS\r\n");
	Sleep(10);
	this->ContMeasOut = false;
	this->blnMeas = false;
	this->FlushComInBuffer();
	this->blnOutputStopped = true;

	// Reset line accumulation buffer
	this->lineIdx = 0;

	DebugTrace(_HBMLDCELL_, "SPD Continuous Output Mode Disabled\n");
    return NO_ERRORS;
}

int SPDLoadCell::StartContinuousOutput()
{
	// Reset line accumulation buffer before starting
	this->lineIdx = 0;

	// SX = continuous raw ADC sample output
	// Format: "S+0125785\r\n" per reading
    this->SerialWrite("SX\r\n");
  	this->ContMeasOut = true;
	this->blnMeas = true;
	this->blnOutputStopped = false;

	DebugTrace(_HBMLDCELL_, "SPD Continuous Output Mode Enabled\n");
    return NO_ERRORS;
}

//===========================================================================
//
//  FlushComInBuffer - Flush all characters waiting at serial input
//
//===========================================================================

int SPDLoadCell::FlushComInBuffer()
{
    WORD    rc, bytes_read;
    WORD    buf_bytes = 0;

	if (RtWaitForSingleObject(hSerMtx, 100) != WAIT_OBJECT_0)
	{
		RtPrintf("SPD FlushComInBuffer: RtWaitForSingleObject failed file %s, line %d \n", _FILE_, __LINE__);
	}
	else
	{
	    rc = this->serialObj->RtGetComBufferCount(&buf_bytes);

        DebugTrace(_HBMLDCELL_, "SPD FlushComInBuffer Count: %d\n", buf_bytes);
        if (buf_bytes > 0)
        {
	        rc = this->serialObj->RtReadComPort((BYTE*)&this->rxmsg[0], buf_bytes, &bytes_read);
            DebugTrace(_HBMLDCELL_, "SPD Rx Buffer Flushed %d\n", bytes_read);
        }

		memset((char*)&this->rxmsg[0], 0, SPD_BUFFER_SIZE);

		// Also reset line accumulation buffer
		this->lineIdx = 0;

		if (!RtReleaseMutex(this->hSerMtx))
		{
			RtPrintf("Release failed in file %s, line %d \n", _FILE_, __LINE__);
		}
    }
    return NO_ERRORS;
}

//===========================================================================
//
//  CheckConfigValue - Send a query command and verify the response
//
//===========================================================================

bool SPDLoadCell::CheckConfigValue(char* cmd, char* value, DWORD response_time)
{
	bool rc = false;

	if (this->SendCommand(cmd, response_time) == NO_ERRORS)
	{
		DebugTrace(_HBMLDCELL_, "SPD Port %d CheckConfigValue::value->%s\n",
			this->serialObj->ucbObj.port, (char*)&value[0]);

		if ((strcmp((char*)&this->rxmsg[0], (char*)&value[0])) == 0)
		{
			rc = true;
			DebugTrace(_HBMLDCELL_,
				"SPD Port %d CheckConfigValue: return true received->%s\n",
				this->serialObj->ucbObj.port, (char*)&this->rxmsg[0]);
		}
		else
		{
			rc = false;
			DebugTrace(_HBMLDCELL_,
				"SPD Port %d CheckConfigValue: return false received->%s\n",
				this->serialObj->ucbObj.port, (char*)&this->rxmsg[0]);
		}
	}

	return rc;
}

//===========================================================================
//
//  MeasureQ - Measurement queue (circular buffer)
//
//  This is identical to the HBM implementation. The queue stores raw
//  ADC count values regardless of the serial protocol used to obtain them.
//
//===========================================================================

int SPDLoadCell::MeasureQ(__int64* measArr, int items)
{
	int i;
	MeasQ_Item* pTmpPointer;

#ifdef _MEASUREQ_ALLOW_MULTI_READ_
	if (this->FirstTimeThru)
	{
#endif
		this->pInMeasQ = &this->MeasQ[0];
		this->pOutMeasQ = &this->MeasQ[0];
		this->bMeasQWrapped = false;
		this->InvalidMeasureCnt = 0;
#ifdef _MEASUREQ_ALLOW_MULTI_READ_
		this->FirstTimeThru = false;
	}
#endif

	if (((this->cPrintCnt % 10000) == 0) && (this->cPrintCnt != 0))
	{
		RtPrintf("SPD MeasureQ for Loadcell = %d\n", this->LoadCellNum);
	}

	// Request Receive Queue Mutex
	if (RtWaitForSingleObject(this->hMeasQ, 100) != WAIT_OBJECT_0)
	{
		RtPrintf("RtWaitForSingleObject failed in file %s, line %d Loadcell %d\n", _FILE_, __LINE__, this->LoadCellNum);
	}
	else
	{
		switch (items)
		{
		case 0:
			// Add data to the queue
			this->pInMeasQ->meas = measArr[0];
			this->pInMeasQ->state = true;

			this->InvalidMeasureCnt = 0;

			this->pOutMeasQ = this->pInMeasQ;
			this->pInMeasQ++;

			if (this->pInMeasQ == &this->MeasQ[SPD_MEASQ_MAX_ITEMS])
			{
				this->pInMeasQ = &this->MeasQ[0];
				this->bMeasQWrapped = true;
			}
			break;

		default:
			// Read one or more measurements
			pTmpPointer = this->pOutMeasQ;
			for (i = 0; i < items; i++)
			{
#ifndef _MEASUREQ_ALLOW_MULTI_READ_
				if (this->pOutMeasQ->state == true)
				{
#endif
					measArr[i] = this->pOutMeasQ->meas;
					this->pOutMeasQ->state = false;

					if (this->pOutMeasQ != &this->MeasQ[0])
					{
						this->pOutMeasQ--;
					}
					else if (this->bMeasQWrapped == true)
					{
						this->pOutMeasQ = &this->MeasQ[SPD_MEASQ_MAX_ITEMS - 1];
					}
#ifndef _MEASUREQ_ALLOW_MULTI_READ_
				}
				else
#else
				if (this->pOutMeasQ->state == false)
#endif
				{
					if (this->InvalidMeasureCnt > RETRY_CNT_MAX)
					{
						measArr[i] = 0;
					}
#ifndef _MEASUREQ_ALLOW_MULTI_READ_
					else
					{
						this->InvalidMeasureCnt++;
						measArr[i] = this->pOutMeasQ->meas;
					}
#endif
				}
			}
			this->pOutMeasQ = pTmpPointer;
			break;
		}

		if (!RtReleaseMutex(this->hMeasQ))
		{
			RtPrintf("Release failed in file %s, line %d \n", _FILE_, __LINE__);
		}
	}

	return NO_ERRORS;
}

//===========================================================================
//
//  read_load_cell - Read averaged weight from measurement queue
//
//  Identical to HBM implementation - the queue contains ADC counts
//  regardless of whether they came from binary or ASCII parsing.
//
//===========================================================================

__int64 SPDLoadCell::read_load_cell(int channel)
{
    int   k, rcnt;
    __int64 rdg, cum_rdg;

    rcnt    = 0;
    cum_rdg = 0;

    if (this->num_adc_reads[channel] > 0)
    {
        this->MeasureQ(app->individual_wts, this->num_adc_reads[channel]);

        for(k = 0; k < this->num_adc_reads[channel]; k++)
        {
            rdg = app->individual_wts[k];

            switch(this->adc_mode[channel])
            {
                case AVG:
                    LC_ERR_CLR
                    return(rdg);
                    break;

                case INDIVID:
                    if ((this->num_adc_reads[channel] > 0) &&
                        (this->num_adc_reads[channel] < SPD_MEASQ_MAX_ITEMS))
                    {
                        rcnt++;
                        cum_rdg += rdg;

                        if (rcnt == this->num_adc_reads[channel])
                        {
                            if (this->num_adc_reads[channel] > 0)
							{
								cum_rdg = cum_rdg / this->num_adc_reads[channel];
								return(cum_rdg);
							}
                        }
                        LC_ERR_CLR
                    }
                    else
                    {
                        LC_ERR_SET
                    }
                    break;

                default:
                    LC_ERR_SET
                    break;
            }
        }
    }
    else
    {
        RtPrintf("SPD Cannot read load cell. ADC Reads set to %d\n", num_adc_reads[channel]);
    }

    return(0);
}

//===========================================================================
//
//  Accessor methods - identical to HBM
//
//===========================================================================

void SPDLoadCell::set_adc_num_reads(int channel, int reads)
{
    DebugTrace(_HBMLDCELL_, "SPD Set channel %d reads %d\n", channel, reads);
    if ((reads > 0) && (reads < SPD_MAX_ADC_READS))
    {
        this->num_adc_reads[channel] = reads;
    }
    else
	{
        RtPrintf("SPD Error set adc reads %d chan %d file %s, line %d \n",
                  reads, channel, _FILE_, __LINE__);
	}
}

int SPDLoadCell::read_adc_num_reads(int channel)
{
    return(this->num_adc_reads[channel]);
}

void SPDLoadCell::KillTime(int iTime)
{
    int iCounter;
    for (iCounter = 0; iCounter < iTime; iCounter++) {}
}

void SPDLoadCell::set_adc_mode(int channel, int mode)
{
    DebugTrace(_HBMLDCELL_, "SPD Set channel %d mode %d\n", channel, mode);
    if ((mode >= INDIVID) && (mode <= AVG))
    {
        this->adc_mode[channel] = mode;
    }
    else
        RtPrintf("SPD Error set adc mode %d chan %d file %s, line %d \n",
                  mode, channel, _FILE_, __LINE__);
}

int SPDLoadCell::read_adc_mode(int channel)
{
    return(this->adc_mode[channel]);
}

void SPDLoadCell::read_adc_version(void)
{
    char* ptr;

    ptr = (char*)&this->left_adc_version[0];

	// SPD: "IV\r\n" returns firmware version
	// Response format: "IV major.minor\r\n" (e.g., "IV 2.03\r\n")
    this->SendCommand("IV\r\n", 3000);

	// Copy response (up to ADCINFOMAX chars)
	if (this->blnResponseReceived)
	{
		// Strip any "IV " prefix if present
		char* pVersion = (char*)&this->rxmsg[0];
		if (strncmp(pVersion, "IV", 2) == 0)
		{
			pVersion += 2;
			while (*pVersion == ' ') pVersion++; // skip spaces
		}
		strncpy(ptr, pVersion, ADCINFOMAX);
		// Remove trailing CR/LF
		for (int i = 0; i < ADCINFOMAX; i++)
		{
			if (ptr[i] == '\r' || ptr[i] == '\n')
			{
				ptr[i] = '\0';
				break;
			}
		}
	}
}

void SPDLoadCell::set_adc_offset(int channel, int offset)
{
    DebugTrace(_HBMLDCELL_, "SPD Set channel %d offset %d\n", channel, offset);
}

void SPDLoadCell::read_adc_pn()
{
    char* ptr;

    ptr = (char*)&this->left_adc_pn[0];

	// SPD: "ID\r\n" returns device identification
	// Response format varies - typically device model info
    this->SendCommand("ID\r\n", 3000);

	if (this->blnResponseReceived)
	{
		// Copy the identification string
		char* pPartNum = (char*)&this->rxmsg[0];
		strncpy(ptr, pPartNum, ADCINFOMAX);
		// Remove trailing CR/LF
		for (int i = 0; i < ADCINFOMAX; i++)
		{
			if (ptr[i] == '\r' || ptr[i] == '\n')
			{
				ptr[i] = '\0';
				break;
			}
		}
	}
}

void SPDLoadCell::write_adc(int channel, unsigned char outbyte)
{
	channel; // unused
	outbyte; // unused
}

char SPDLoadCell::read_adc(int channel)
{
	channel; // unused
	return(0);
}

//===========================================================================
//
//  WriteLCReadsQ - Write weight samples to file (debug feature)
//
//===========================================================================

int SPDLoadCell::WriteLCReadsQ()
{
    static HANDLE hFile;
    ULONG  rw_cnt;
    int    status = NO_ERRORS;
	char   Buffer[80];
	char   FileName[30];
	int    Cnt;

	sprintf((char*)&FileName[0], "%s%d.txt", LCREADS_PATH, this->LoadCellNum);

    hFile = CreateFile((char*)&FileName[0],
                        GENERIC_READ | GENERIC_WRITE,
                        0, NULL, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile != INVALID_HANDLE_VALUE)
    {
		for (int i = 0; i < this->LCReadsCaptured; i++)
		{
			Cnt = sprintf(&Buffer[0], "%ld\r\n", this->SampleLCReadQ[i]);
			WriteFile(hFile, &Buffer[0], Cnt, &rw_cnt, NULL);
		}
    }
    else
    {
        RtPrintf("SPD Write file error %u file %s\n", GetLastError(), (char*)&FileName[0]);
        status = ERROR_OCCURED;
    }

    CloseHandle(hFile);
	RtPrintf("SPD File %s written\n", (char*)&FileName[0]);

	return 0;
}

//===========================================================================
//
//  SampleLCReadMeasureQ - Add a loadcell read to the sample queue (debug)
//
//===========================================================================

int SPDLoadCell::SampleLCReadMeasureQ(__int64* measArr)
{
	if ((this->LCReadCounter == 0) || this->StartLCReads)
	{
		this->StartLCReads = true;

		if (this->LCReadCounter == 0)
			RtPrintf("SPD Started taking %d reads from loadcell %d\n", ReadsToSample, this->LoadCellNum);

		this->SampleLCReadQ[LCReadCounter] = *measArr;
		this->LCReadCounter++;

		if ((this->LCReadCounter == SPD_MAX_MEAS_SAMPLES) || (this->LCReadCounter == ReadsToSample))
		{
			this->LCReadsCaptured = this->LCReadCounter;
			this->LCReadCounter = 0;
			TakeLoadCellReadsFlag = TakeLoadCellReadsFlag - 1;
			this->StartLCReads = false;
			app->WriteLCReadsToFile = app->WriteLCReadsToFile - 1;
			RtPrintf("SPD Done taking %d reads from loadcell %d\n", this->LCReadsCaptured, this->LoadCellNum);
		}
	}
	return NO_ERRORS;
}
