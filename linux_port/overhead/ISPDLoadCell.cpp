//////////////////////////////////////////////////////////////////////
// ISPDLoadCell.cpp: implementation of the ISPDLoadCell class.
//
// H&B (Hauch & Bach) ISPD-20KG digital load cell amplifier driver.
// Drop-in replacement for HBMLoadCell - same LoadCell base class,
// same measurement queue interface, same thread structure.
//
// Protocol differences from HBM FIT7:
//   HBM: Binary 4-byte packets (3 data + 1 XOR checksum), 38400 baud
//   ISPD: ASCII text lines (e.g., "S+015641\r"), 115200 baud
//
//   HBM commands: "CMD?;" or "CMD value;" (semicolon terminated)
//   ISPD commands: "CMD\r" or "CMD value\r" (CR terminated)
//
//   HBM continuous start: "MSV?0;"  HBM stop: "STP;"
//   ISPD continuous start: "SX\r"   ISPD stop: any new command stops stream
//
// ISPD measurement output format (SX command = raw ADC samples):
//   "S+015641\r"  or  "S-000500\r"
//   Byte 0: 'S' (sample prefix)
//   Byte 1: '+' or '-' (sign)
//   Bytes 2-7: 6-digit integer value (ADC counts)
//   Byte 8: CR
//
//////////////////////////////////////////////////////////////////////

#include "types.h"
#undef  _FILE_
#define _FILE_      "ISPDLoadCell.cpp"

extern ISPDLoadCell::ISPDinit *	pISPDLoadCellInit1;
extern ISPDLoadCell::ISPDinit *	pISPDLoadCellInit2;
extern ISPDLoadCell::ISPDinit *	pISPDLoadCellInit3;
extern ISPDLoadCell::ISPDinit *	pISPDLoadCellInit4;
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

ISPDLoadCell::ISPDLoadCell()
{
	int i = 0;

    this->NumCH = 1;

	this->ContMeasOut = false;
	this->blnMeas     = false;
	this->bFirstCheck = true;
	this->num_adc_reads[0] = 5;

	// Initialize line accumulation buffer
	this->lineIdx = 0;
	memset(this->lineBuf, 0, ISPD_LINE_BUFFER_SIZE);

	// Initialize queue indices
	this->MeasQ_Idx = -1;

    for (i = 0; i < ISPD_MEASQ_MAX_ITEMS; i++)
	{
		this->MeasQ[i].meas = 0;
		this->MeasQ[i].state = false;
	}
}

ISPDLoadCell::~ISPDLoadCell()
{
}

void ISPDLoadCell::initialize(ISPDinit* pISPDInit)
{
	char	sBuffer[80];

	this->serialObj = pISPDInit->pSerial;

	sprintf(sBuffer, "Ser_Port%d", this->serialObj->ucbObj.port);
	DebugTrace(_HBMLDCELL_, "ISPDLoadCell::initialize - Using %s\n", (char*)&sBuffer[0]);

	// Create serial port Mutex
	this->hSerMtx = RtCreateMutex(NULL, FALSE, (LPCTSTR)&sBuffer[0]);

	if (this->hSerMtx == NULL)
	{
		RtPrintf("RtCreateMutex for serial port failed file %s, line %d \n", _FILE_, __LINE__);
	}

	RtPrintf("ISPDLoadCell::initializing::LoadCellNum = %d\n", this->LoadCellNum);

	switch (this->LoadCellNum)
	{
	case ISPD_LOADCELL_1:
		pISPDLoadCellInit1->phSerMtx = (HANDLE*)this->hSerMtx;
		if ((this->CreateISPDLoadCellThread(pISPDInit) != NO_ERRORS))
			return;
		break;

	case ISPD_LOADCELL_2:
		pISPDLoadCellInit2->phSerMtx = (HANDLE*)this->hSerMtx;
		if ((this->CreateISPDLoadCellThread(pISPDInit) != NO_ERRORS))
			return;
		break;

	case ISPD_LOADCELL_3:
		pISPDLoadCellInit3->phSerMtx = (HANDLE*)this->hSerMtx;
		if ((this->CreateISPDLoadCellThread(pISPDInit) != NO_ERRORS))
			return;
		break;

	case ISPD_LOADCELL_4:
		pISPDLoadCellInit4->phSerMtx = (HANDLE*)this->hSerMtx;
		if ((this->CreateISPDLoadCellThread(pISPDInit) != NO_ERRORS))
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
//   CreateISPDLoadCellThread
//--------------------------------------------------------

int ISPDLoadCell::CreateISPDLoadCellThread(ISPDinit* pISPDInit)
{
    DWORD threadId;
    DWORD dwStackSize = 0x4000;
    DWORD dwExitCode  = 0;

	hISPDLc = NULL;

	switch(pISPDInit->pISPDLoadCell->LoadCellNum)
	{
	case ISPD_LOADCELL_1:
		hISPDLc = CreateThread(NULL, dwStackSize,
			(LPTHREAD_START_ROUTINE) ISPDLoadCellThread1,
			(PVOID) pISPDInit, 0, (LPDWORD) &threadId);
		break;

	case ISPD_LOADCELL_2:
		hISPDLc = CreateThread(NULL, dwStackSize,
			(LPTHREAD_START_ROUTINE) ISPDLoadCellThread2,
			(PVOID) pISPDInit, 0, (LPDWORD) &threadId);
		break;

	case ISPD_LOADCELL_3:
		hISPDLc = CreateThread(NULL, dwStackSize,
			(LPTHREAD_START_ROUTINE) ISPDLoadCellThread3,
			(PVOID) pISPDInit, 0, (LPDWORD) &threadId);
		break;

	case ISPD_LOADCELL_4:
		hISPDLc = CreateThread(NULL, dwStackSize,
			(LPTHREAD_START_ROUTINE) ISPDLoadCellThread4,
			(PVOID) pISPDInit, 0, (LPDWORD) &threadId);
		break;
	}

    if(hISPDLc == NULL)
    {
        RtPrintf("Error file %s, line %d \n", _FILE_, __LINE__);
        return ERROR_OCCURED;
    }

    if(!RtSetThreadPriority(hISPDLc, RT_PRIORITY_MIN+50))
    {
        RtPrintf("Error file %s, line %d \n", _FILE_, __LINE__);
        TerminateThread(hISPDLc, dwExitCode);
        return ERROR_OCCURED;
    }

    return NO_ERRORS;
}

//--------------------------------------------------------
//  ISPD Load Cell Thread - common body
//  Each thread opens COM port at 115200, initializes ISPD,
//  then loops reading ASCII measurement lines.
//--------------------------------------------------------

static void ISPDLoadCellThreadBody(ISPDLoadCell::ISPDinit* pISPDInit, int loadCellIdx, const char* eventName)
{
	WORD rc = FALSE;
	ULONG LoopCnt = 0;

	Serial* serObj;
	ISPDLoadCell* pISPDcObj;
	HANDLE hSerialQ;

	serObj   = pISPDInit->pSerial;
	pISPDcObj = pISPDInit->pISPDLoadCell;
	hSerialQ = (HANDLE*)pISPDInit->phSerMtx;

	pISPDcObj->cPrintCnt      = 0;
	pISPDcObj->ParseErrorCount = 0;

	pISPDcObj->LCReadCounter  = 0;
	pISPDcObj->StartLCReads   = false;
	pISPDcObj->CurrentRead    = 0;
	pISPDcObj->FirstTimeThru  = true;

	hLoadCellInitialized[loadCellIdx] = RtOpenEvent(NULL, NULL, eventName);

	if (hLoadCellInitialized[loadCellIdx] == NULL)
	{
		RtPrintf("Failed to open event -%s- \n", eventName);
	}

	//----- Open communications port at 115200 baud -----
	DebugTrace(_HBMLDCELL_, "ISPD: Opening COMPORT%d at 115200 baud . . .\n", serObj->ucbObj.port + 1);

	if (RtWaitForSingleObject(hSerialQ, 100) != WAIT_OBJECT_0)
	{
		RtPrintf("hSerMtx RtWaitForSingleObject failed in file %s, line %d \n", _FILE_, __LINE__);
	}
	else
	{
		// ISPD default baud rate is 115200 (same as SPD)
		if ((rc = serObj->RtOpenComPort(ISPD_DEFAULT_BAUD_RATE, COM_WORDSIZE_8,
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

	pISPDcObj->init_adc(AVG, 2);

	if (!RtSetEvent(hLoadCellInitialized[loadCellIdx]))
    {
        RtPrintf("Error %u file %s, line %d \n", GetLastError(), _FILE_, __LINE__);
    }

	///////////////////////////////////////////////////////////////////////////
	//                           Main Loop
	///////////////////////////////////////////////////////////////////////////

	for(;;)
    {
		LoopCnt++;

        // Read measurements
        if (pISPDcObj->ContMeasOut)
		{
            pISPDcObj->SerialRead(serObj);
			Sleep(MEASURE_MODE_SLEEP);
		}
		else
		{
			Sleep(MISC_MODE_SLEEP);
		}
    }
}

int RTFCNDCL ISPDLoadCell::ISPDLoadCellThread1(PVOID pISPDInit)
{
	ISPDLoadCellThreadBody((ISPDLoadCell::ISPDinit*)pISPDInit, ISPD_LOADCELL_1, "Load Cell 1 Initialized");
	return 0;
}

int RTFCNDCL ISPDLoadCell::ISPDLoadCellThread2(PVOID pISPDInit)
{
	ISPDLoadCellThreadBody((ISPDLoadCell::ISPDinit*)pISPDInit, ISPD_LOADCELL_2, "Load Cell 2 Initialized");
	return 0;
}

int RTFCNDCL ISPDLoadCell::ISPDLoadCellThread3(PVOID pISPDInit)
{
	ISPDLoadCellThreadBody((ISPDLoadCell::ISPDinit*)pISPDInit, ISPD_LOADCELL_3, "Load Cell 3 Initialized");
	return 0;
}

int RTFCNDCL ISPDLoadCell::ISPDLoadCellThread4(PVOID pISPDInit)
{
	ISPDLoadCellThreadBody((ISPDLoadCell::ISPDinit*)pISPDInit, ISPD_LOADCELL_4, "Load Cell 4 Initialized");
	return 0;
}

//===========================================================================
//
//  init_adc - Initialize ISPD load cell
//
//  ISPD initialization:
//    1. Flush any stale data
//    2. Query identity (ID command)
//    3. Query firmware version (IV command)
//    4. Configure filter settings (FL, FM, UR)
//    5. Save settings to EEPROM (WP)
//    6. Start continuous ADC sample output (SX command)
//
//  H&B commands use CR-only termination (NOT CRLF).
//
//===========================================================================

int ISPDLoadCell::init_adc(int mode, int num_reads)
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
    for(i = 0; i < ISPD_NUM_CH; i++)
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

	// Configure ISPD filter settings (CR-only termination)
	// FL 3 = Filter cutoff level 3
	// FM 0 = IIR filter mode
	// UR 1 = Update rate divider 1 (~586 Hz output)
	this->SendCommand("FL 3\r", 3000);
	this->SendCommand("FM 0\r", 3000);
	this->SendCommand("UR 1\r", 3000);

	// Save settings to ISPD EEPROM
	this->SendCommand("WP\r", 3000);

	// Start continuous ADC sample output
	// SX command streams raw ADC counts: "S+015641\r"
    this->StartContinuousOutput();

    if (Config == true)
	{
		RtPrintf("ISPD LoadCell %d initialized successfully\n", this->LoadCellNum);
		return(TRUE);
	}
	else
	{
		sprintf(app_err_buf, "ISPDLoadCell::init_adc - Error in ISPD LoadCell Configuration\n");
	    app->GenError(warning, app_err_buf);
		return(FALSE);
	}
}

//===========================================================================
//
//  ParseMeasurementLine - Parse an ASCII measurement line from the ISPD
//
//  SX output format: "S+015641\r" (raw ADC sample)
//    Byte 0: type prefix ('S'=sample, 'G'=gross, 'N'=net)
//    Byte 1: sign ('+' or '-')
//    Bytes 2+: integer digits
//
//  Returns parsed value as __int64, or 0 on parse error.
//
//===========================================================================

__int64 ISPDLoadCell::ParseMeasurementLine(char* line, int len)
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
//  In command mode: read character-by-character until CR (H&B uses CR-only)
//  In measurement mode: accumulate ASCII lines, parse each complete line
//    as an ADC sample value and push to measurement queue
//
//===========================================================================

void ISPDLoadCell::SerialRead(Serial* serial)
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

        memset((char*)&this->rxmsg[0], 0, ISPD_BUFFER_SIZE);

        if (this->blnCmdSent && !blnMeas)
        {
			//----------------------------------------------------------
			// Command response mode - read until CR
			// H&B ISPD uses CR-only line termination
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

                // Look for CR terminator (H&B uses CR-only, not CRLF)
                if (rxbuffidx > 0)
                {
                    if (this->rxmsg[rxbuffidx - 1] == 0x0D)
                    {
                        // Null-terminate at CR position for string ops
                        this->rxmsg[rxbuffidx - 1] = '\0';
                        this->blnResponseReceived = true;
                    }
                }

                rc = this->serialObj->RtGetComBufferCount(&buf_bytes);
                Sleep(1);

				if (rxbuffidx >= ISPD_BUFFER_SIZE)
                {
                    RtPrintf("ISPD Serial Read Rx Buffer Full\n");
					break;
                }
	        }
		}
	    else if (this->blnMeas && (buf_bytes > 0))
	    {
			//----------------------------------------------------------
			// Measurement mode - ASCII line accumulation
			//
			// ISPD sends continuous ASCII lines like "S+015641\r"
			// We accumulate characters into lineBuf until we see CR,
			// then parse the complete line and push to measurement queue.
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

				if (tmpByte == 0x0D) // CR = end of line (H&B uses CR-only)
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
				else if (tmpByte == 0x0A) // LF = skip (shouldn't be present but handle gracefully)
				{
					// Don't add LF to the line buffer, just skip it
				}
				else
				{
					// Accumulate character into line buffer
					if (this->lineIdx < ISPD_LINE_BUFFER_SIZE - 1)
					{
						this->lineBuf[this->lineIdx++] = (char)tmpByte;
					}
					else
					{
						// Line buffer overflow - something is wrong, reset
						RtPrintf("ISPD line buffer overflow, resetting\n");
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
						"ISPD SerialRead bytes in buffer = %d ParseErrors = %d Port = %d\n",
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
//  SerialWrite - Write command to the ISPD
//
//  ISPD commands terminated with CR (0x0D).
//  The caller must include "\r" in the command string.
//
//===========================================================================

int ISPDLoadCell::SerialWrite(char* txdata)
{
	WORD rc = NO_ERRORS;
	WORD bytes_wrtn;

	if (RtWaitForSingleObject(this->hSerMtx, 100) != WAIT_OBJECT_0)
	{
		RtPrintf("hSerMtx RtWaitForSingleObject failed in file %s, line %d \n", _FILE_, __LINE__);
	}
	else
	{
        DebugTrace(_HBMLDCELL_, "ISPD Port %d Out: %s", this->serialObj->ucbObj.port, txdata);
	    rc = this->serialObj->RtWriteComPort((BYTE*)txdata, (unsigned short)(strlen(txdata)), &bytes_wrtn);

	    if (rc != NO_ERRORS)
		    RtPrintf("ISPD Error rc = %d file %s, line %d \n", rc, _FILE_, __LINE__);

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
//  the CR-terminated response.
//
//===========================================================================

int ISPDLoadCell::SendCommand(char* cmd, DWORD response_time)
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
				RtPrintf("ISPD Timeout in serial response for command %s", cmd);
				break;
			}
		}

		if (this->blnResponseReceived == true)
		{
			rc = NO_ERRORS;
			RtPrintf("ISPD cmd '%s' -> response '%s'\n", cmd, (char*)&this->rxmsg[0]);
		}
    }

    this->blnCmdSent = false;

    // Check if this was a continuous output command
	// ISPD: "SX\r" starts continuous ADC sample output
	// Any other command sent while streaming will stop it
	if (strncmp(cmd, "SX", 2) == 0)
	{
		this->ContMeasOut = true;
		this->blnMeas = true;
		DebugTrace(_HBMLDCELL_, "ISPD Continuous Output Mode Enabled\n");
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
//  ISPD: "SX\r" starts streaming raw ADC samples
//  Any new command sent will stop the stream
//  We use "GS\r" (get single sample) as a benign command to stop streaming
//
//===========================================================================

int ISPDLoadCell::StopContinuousOutput()
{
	// Send a single-shot command to stop the continuous stream
	// GS = Get Sample (single reading) - this stops the stream and returns one value
	this->SerialWrite("GS\r");
	Sleep(10);
	this->ContMeasOut = false;
	this->blnMeas = false;
	this->FlushComInBuffer();
	this->blnOutputStopped = true;

	// Reset line accumulation buffer
	this->lineIdx = 0;

	DebugTrace(_HBMLDCELL_, "ISPD Continuous Output Mode Disabled\n");
    return NO_ERRORS;
}

int ISPDLoadCell::StartContinuousOutput()
{
	// Reset line accumulation buffer before starting
	this->lineIdx = 0;

	// SX = continuous raw ADC sample output
	// Format: "S+015641\r" per reading
    this->SerialWrite("SX\r");
  	this->ContMeasOut = true;
	this->blnMeas = true;
	this->blnOutputStopped = false;

	DebugTrace(_HBMLDCELL_, "ISPD Continuous Output Mode Enabled\n");
    return NO_ERRORS;
}

//===========================================================================
//
//  FlushComInBuffer - Flush all characters waiting at serial input
//
//===========================================================================

int ISPDLoadCell::FlushComInBuffer()
{
    WORD    rc, bytes_read;
    WORD    buf_bytes = 0;

	if (RtWaitForSingleObject(hSerMtx, 100) != WAIT_OBJECT_0)
	{
		RtPrintf("ISPD FlushComInBuffer: RtWaitForSingleObject failed file %s, line %d \n", _FILE_, __LINE__);
	}
	else
	{
	    rc = this->serialObj->RtGetComBufferCount(&buf_bytes);

        DebugTrace(_HBMLDCELL_, "ISPD FlushComInBuffer Count: %d\n", buf_bytes);
        if (buf_bytes > 0)
        {
	        rc = this->serialObj->RtReadComPort((BYTE*)&this->rxmsg[0], buf_bytes, &bytes_read);
            DebugTrace(_HBMLDCELL_, "ISPD Rx Buffer Flushed %d\n", bytes_read);
        }

		memset((char*)&this->rxmsg[0], 0, ISPD_BUFFER_SIZE);

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

bool ISPDLoadCell::CheckConfigValue(char* cmd, char* value, DWORD response_time)
{
	bool rc = false;

	if (this->SendCommand(cmd, response_time) == NO_ERRORS)
	{
		DebugTrace(_HBMLDCELL_, "ISPD Port %d CheckConfigValue::value->%s\n",
			this->serialObj->ucbObj.port, (char*)&value[0]);

		if ((strcmp((char*)&this->rxmsg[0], (char*)&value[0])) == 0)
		{
			rc = true;
			DebugTrace(_HBMLDCELL_,
				"ISPD Port %d CheckConfigValue: return true received->%s\n",
				this->serialObj->ucbObj.port, (char*)&this->rxmsg[0]);
		}
		else
		{
			rc = false;
			DebugTrace(_HBMLDCELL_,
				"ISPD Port %d CheckConfigValue: return false received->%s\n",
				this->serialObj->ucbObj.port, (char*)&this->rxmsg[0]);
		}
	}

	return rc;
}

//===========================================================================
//
//  MeasureQ - Measurement queue (circular buffer)
//
//  Identical to HBM implementation. The queue stores raw ADC count values.
//
//===========================================================================

int ISPDLoadCell::MeasureQ(__int64* measArr, int items)
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
		RtPrintf("ISPD MeasureQ for Loadcell = %d\n", this->LoadCellNum);
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

			if (this->pInMeasQ == &this->MeasQ[ISPD_MEASQ_MAX_ITEMS])
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
						this->pOutMeasQ = &this->MeasQ[ISPD_MEASQ_MAX_ITEMS - 1];
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

__int64 ISPDLoadCell::read_load_cell(int channel)
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
                        (this->num_adc_reads[channel] < ISPD_MEASQ_MAX_ITEMS))
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
        RtPrintf("ISPD Cannot read load cell. ADC Reads set to %d\n", num_adc_reads[channel]);
    }

    return(0);
}

//===========================================================================
//
//  Accessor methods - identical to HBM
//
//===========================================================================

void ISPDLoadCell::set_adc_num_reads(int channel, int reads)
{
    DebugTrace(_HBMLDCELL_, "ISPD Set channel %d reads %d\n", channel, reads);
    if ((reads > 0) && (reads < ISPD_MAX_ADC_READS))
    {
        this->num_adc_reads[channel] = reads;
    }
    else
	{
        RtPrintf("ISPD Error set adc reads %d chan %d file %s, line %d \n",
                  reads, channel, _FILE_, __LINE__);
	}
}

int ISPDLoadCell::read_adc_num_reads(int channel)
{
    return(this->num_adc_reads[channel]);
}

void ISPDLoadCell::KillTime(int iTime)
{
    int iCounter;
    for (iCounter = 0; iCounter < iTime; iCounter++) {}
}

void ISPDLoadCell::set_adc_mode(int channel, int mode)
{
    DebugTrace(_HBMLDCELL_, "ISPD Set channel %d mode %d\n", channel, mode);
    if ((mode >= INDIVID) && (mode <= AVG))
    {
        this->adc_mode[channel] = mode;
    }
    else
        RtPrintf("ISPD Error set adc mode %d chan %d file %s, line %d \n",
                  mode, channel, _FILE_, __LINE__);
}

int ISPDLoadCell::read_adc_mode(int channel)
{
    return(this->adc_mode[channel]);
}

void ISPDLoadCell::read_adc_version(void)
{
    char* ptr;

    ptr = (char*)&this->left_adc_version[0];

	// ISPD: "IV\r" returns firmware version
	// Response format: "V:0146" (from ISPD-20KG D:1520)
    this->SendCommand("IV\r", 3000);

	// Copy response (up to ADCINFOMAX chars)
	if (this->blnResponseReceived)
	{
		char* pVersion = (char*)&this->rxmsg[0];
		// Strip "V:" prefix if present
		if (strncmp(pVersion, "V:", 2) == 0)
		{
			pVersion += 2;
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

void ISPDLoadCell::set_adc_offset(int channel, int offset)
{
    DebugTrace(_HBMLDCELL_, "ISPD Set channel %d offset %d\n", channel, offset);
}

void ISPDLoadCell::read_adc_pn()
{
    char* ptr;

    ptr = (char*)&this->left_adc_pn[0];

	// ISPD: "ID\r" returns device identification
	// Response format: "D:1520" (for ISPD-20KG)
    this->SendCommand("ID\r", 3000);

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

void ISPDLoadCell::write_adc(int channel, unsigned char outbyte)
{
	channel; // unused
	outbyte; // unused
}

char ISPDLoadCell::read_adc(int channel)
{
	channel; // unused
	return(0);
}

//===========================================================================
//
//  WriteLCReadsQ - Write weight samples to file (debug feature)
//
//===========================================================================

int ISPDLoadCell::WriteLCReadsQ()
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
        RtPrintf("ISPD Write file error %u file %s\n", GetLastError(), (char*)&FileName[0]);
        status = ERROR_OCCURED;
    }

    CloseHandle(hFile);
	RtPrintf("ISPD File %s written\n", (char*)&FileName[0]);

	return 0;
}

//===========================================================================
//
//  SampleLCReadMeasureQ - Add a loadcell read to the sample queue (debug)
//
//===========================================================================

int ISPDLoadCell::SampleLCReadMeasureQ(__int64* measArr)
{
	if ((this->LCReadCounter == 0) || this->StartLCReads)
	{
		this->StartLCReads = true;

		if (this->LCReadCounter == 0)
			RtPrintf("ISPD Started taking %d reads from loadcell %d\n", ReadsToSample, this->LoadCellNum);

		this->SampleLCReadQ[LCReadCounter] = *measArr;
		this->LCReadCounter++;

		if ((this->LCReadCounter == ISPD_MAX_MEAS_SAMPLES) || (this->LCReadCounter == ReadsToSample))
		{
			this->LCReadsCaptured = this->LCReadCounter;
			this->LCReadCounter = 0;
			TakeLoadCellReadsFlag = TakeLoadCellReadsFlag - 1;
			this->StartLCReads = false;
			app->WriteLCReadsToFile = app->WriteLCReadsToFile - 1;
			RtPrintf("ISPD Done taking %d reads from loadcell %d\n", this->LCReadsCaptured, this->LoadCellNum);
		}
	}
	return NO_ERRORS;
}
