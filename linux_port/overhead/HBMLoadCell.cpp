//////////////////////////////////////////////////////////////////////
// HBMLoadCell.cpp: implementation of the HBMLoadCell class.
//////////////////////////////////////////////////////////////////////

#include "types.h"
#undef  _FILE_
#define _FILE_      "HBMLoadCell.cpp"

extern HBMLoadCell::HBMinit *	pHBMLoadCellInit1;
extern HBMLoadCell::HBMinit *	pHBMLoadCellInit2;
extern HBMLoadCell::HBMinit *	pHBMLoadCellInit3;
extern HBMLoadCell::HBMinit *	pHBMLoadCellInit4;
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

HBMLoadCell::HBMLoadCell()
{
	int i = 0;

    this->NumCH = 1;

    //Set continuous meas output flag, meas mode
	this->ContMeasOut =	false;
	this->blnMeas     =	false;
	this->bFirstCheck = true;
	this->num_adc_reads[0]    = 5;
	this->num_adc_reads[1]    = 5;

	//Initialize queue indices to -1
	this->MeasQ_Idx = -1;

    for (i = 0; i < MEASQ_MAX_ITEMS; i++)
	{
		this->MeasQ[i].meas = 0;
		this->MeasQ[i].state = false;
	}

    //initialize();
}

HBMLoadCell::~HBMLoadCell()
{

}

void HBMLoadCell::initialize( HBMinit* pHBMInit )
{

	char	sBuffer[80];

	this->serialObj = pHBMInit->pSerial;
	
	sprintf( sBuffer, "Ser_Port%d", this->serialObj->ucbObj.port);
	DebugTrace(_HBMLDCELL_, "HBMLoadCell::initialize - Using %s\n", (char*) &sBuffer[0]);
	
	// Create serial port Mutex
	this->hSerMtx = RtCreateMutex( NULL, FALSE, (LPCTSTR) &sBuffer[0]);

	if (this->hSerMtx == NULL)
	{
		RtPrintf("RtCreateMutex for serial port failed file %s, line %d \n", _FILE_, __LINE__);
	}

	RtPrintf("initializing::LoadCellNum = %d\n", this->LoadCellNum);
	//switch (this->serialObj->ucbObj.port)
	switch (this->LoadCellNum)
	{
	case	LOADCELL_1:
		
		pHBMLoadCellInit1->phSerMtx = (HANDLE *)this->hSerMtx;
	
		//Create Load Cell thread
		if ((this->CreateHBMLoadCellThread( pHBMInit /*pHBMLoadCellInit1*/) != NO_ERRORS))
			{
			return;
			}
		break;

	case	LOADCELL_2:
		
		pHBMLoadCellInit2->phSerMtx = (HANDLE *)this->hSerMtx;
	
		//Create Load Cell thread
		if ((this->CreateHBMLoadCellThread( pHBMInit /* pHBMLoadCellInit2 */) != NO_ERRORS))
			{
			return;
			}
		break;

	case	LOADCELL_3:
		
		pHBMLoadCellInit3->phSerMtx = (HANDLE *)this->hSerMtx;
	
		//Create Load Cell thread
		if ((this->CreateHBMLoadCellThread(pHBMInit) != NO_ERRORS))
			{
			return;
			}
		break;

	case	LOADCELL_4:
		
		pHBMLoadCellInit4->phSerMtx = (HANDLE *)this->hSerMtx;
	
		//Create Load Cell thread
		if ((this->CreateHBMLoadCellThread(pHBMInit) != NO_ERRORS))
			{
			return;
			}
		break;

	default:
    
		return;
    }
	
    // Create Measurement Queue Mutex
	this->hMeasQ = RtCreateMutex( NULL, FALSE,(LPCTSTR)"LC_Meas");
	if (this->hMeasQ == NULL)
	{
		RtPrintf("RtCreateMutex for measurement queue file %s, line %d \n", _FILE_, __LINE__);
	}
}

//--------------------------------------------------------
//   CreateHBMLoadCellThread
//--------------------------------------------------------

int HBMLoadCell::CreateHBMLoadCellThread( HBMinit* pHBMInit)
{

    DWORD            threadId;
    DWORD            dwStackSize = 0x4000;
 //   ULONG            stackSize   = 0;			RED - Removed because not referenced
//    ULONG            sleepTime   = 30000;		RED - Removed because not referenced
    DWORD            dwExitCode  = 0;
	
	hHBMLc = NULL;
    
	switch(pHBMInit->pHBMLoadCell->LoadCellNum)
	{
	case LOADCELL_1:
		hHBMLc = CreateThread(
		NULL,            
		dwStackSize,    //default
		(LPTHREAD_START_ROUTINE) HBMLoadCellThread1, //function
		(PVOID) pHBMInit,            //parameters
		0,
		(LPDWORD) &threadId
		);
		break;


	case LOADCELL_2:
		hHBMLc = CreateThread(
		NULL,            
		dwStackSize,    //default
		(LPTHREAD_START_ROUTINE) HBMLoadCellThread2, //function
		(PVOID) pHBMInit,            //parameters
		0,
		(LPDWORD) &threadId
		);
		break;

	case LOADCELL_3:
		hHBMLc = CreateThread(
		NULL,            
		dwStackSize,    //default
		(LPTHREAD_START_ROUTINE) HBMLoadCellThread3, //function
		(PVOID) pHBMInit,            //parameters
		0,
		(LPDWORD) &threadId
		);
		break;

	case LOADCELL_4:
		hHBMLc = CreateThread(
		NULL,            
		dwStackSize,    //default
		(LPTHREAD_START_ROUTINE) HBMLoadCellThread4, //function
		(PVOID) pHBMInit,            //parameters
		0,
		(LPDWORD) &threadId
		);
		break;
	}

    if(hHBMLc == NULL)
    {
        RtPrintf("Error file %s, line %d \n", _FILE_, __LINE__ );
        return ERROR_OCCURED;
    }

    if(!RtSetThreadPriority( hHBMLc, RT_PRIORITY_MIN+50))
    {
        RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
        TerminateThread( hHBMLc, dwExitCode);
        return ERROR_OCCURED;
    }

    return NO_ERRORS;
  }

//--------------------------------------------------------
//  HBMLoadCell thread
//--------------------------------------------------------

int RTFCNDCL HBMLoadCell::HBMLoadCellThread1(PVOID pHBMInit)
{
	int  rc = FALSE;
	ULONG	LoopCnt = 0;
//	HANDLE	hInit;

//----- Create comm objects
	
	HBMLoadCell::HBMinit*	pHBMInitObj;

	Serial* serObj;
	HBMLoadCell * pHBMcObj;
	HANDLE	hSerialQ;

	pHBMInitObj = (HBMLoadCell::HBMinit *)pHBMInit;

	serObj = pHBMInitObj->pSerial;
	pHBMcObj = pHBMInitObj->pHBMLoadCell;
	hSerialQ = (HANDLE *)pHBMInitObj->phSerMtx;

	pHBMcObj->cPrintCnt			= 0;
	pHBMcObj->CheckSumError		= 0;
	pHBMcObj->ExtraReadCnt		= 0;

	pHBMcObj->LCReadCounter	= 0;
	pHBMcObj->StartLCReads	= false;
	pHBMcObj->CurrentRead	= 0;
	pHBMcObj->FirstTimeThru = true;

	//hInit = RtOpenEvent(NULL, NULL, "Load Cell 1 Initialized"); //GLC added 2/19/04
	hLoadCellInitialized[LOADCELL_1] = RtOpenEvent(NULL, NULL, "Load Cell 1 Initialized"); //GLC added 2/19/04
	
	if (hLoadCellInitialized[LOADCELL_1] == NULL)//(hInit == NULL)
	{
		RtPrintf("Failed to open event -Load Cell 1 Initialized- \n");
	}

//----- Open communications port

	DebugTrace(_HBMLDCELL_, "Opening COMPORT%d . . .\n", serObj->ucbObj.port + 1 );

	if (RtWaitForSingleObject(hSerialQ, 100) != WAIT_OBJECT_0)
	{
		RtPrintf("hSerMtx RtWaitForSingleObject failed in file %s, line %d \n", _FILE_, __LINE__);
	}
	else
	{
		if ((rc = serObj->RtOpenComPort(COM_BAUD_RATE_38400, COM_WORDSIZE_8, // TG - COM_BAUD_RATE_38400
                         COM_STOPBITS_1, COM_PARITY_NONE, COM_FLOW_CONTROL_HARDWARE)) != NO_ERRORS)
        {
            RtPrintf("Error COM%d Open rc = %d file %s, line %d \n", serObj->ucbObj.port + 1,
					rc, _FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        //Release the mutex
		if (!RtReleaseMutex(hSerialQ))
		{
			RtPrintf("Release failed in file %s, line %d \n", _FILE_, __LINE__);
		}
    }
    
	if (pHBMcObj->init_adc(AVG, 2) != TRUE)
	{
		// Load cell not responding — do not enter main loop
		// Event is never signaled, overhead will report init failure
		RtPrintf("HBM Load Cell thread exiting - init failed.\n");
		for(;;) { Sleep(5000); } // Stay alive but idle
	}

	if (!RtSetEvent(hLoadCellInitialized[LOADCELL_1])) //hInit)) // GLC added 2/19/04
    {
        RtPrintf("Error %u file %s, line %d \n",GetLastError(), _FILE_, __LINE__);
    }
///////////////////////////////////////////////////////////////////////////////
//                               Main Loop
///////////////////////////////////////////////////////////////////////////////

	for(;;)
    {
		if (LoopCnt%2500 == 0)
		{
			pHBMcObj->HBMCheckSettings(); //GLC
			LoopCnt = 0;
		}

		LoopCnt++;
		
        
		//---- Read measurements ----
        if (pHBMcObj->ContMeasOut)
		{
            pHBMcObj->SerialRead(serObj);
			Sleep (MEASURE_MODE_SLEEP);
		}
		else
		{
			Sleep(MISC_MODE_SLEEP);
		}
    }
}

int RTFCNDCL HBMLoadCell::HBMLoadCellThread2(PVOID pHBMInit)
{

    WORD  rc = FALSE;
	ULONG	LoopCnt = 0;

//----- Create comm objects
	
	HBMinit*	pHBMInitObj;

	Serial* serObj;
	HBMLoadCell * pHBMcObj;
	HANDLE	hSerialQ;

	pHBMInitObj = (HBMinit *)pHBMInit;

	serObj = pHBMInitObj->pSerial;
	pHBMcObj = pHBMInitObj->pHBMLoadCell;
	hSerialQ = (HANDLE *)pHBMInitObj->phSerMtx;

	pHBMcObj->cPrintCnt			= 0;
	pHBMcObj->CheckSumError		= 0;
	pHBMcObj->ExtraReadCnt		= 0;
	
	pHBMcObj->LCReadCounter	= 0;
	pHBMcObj->StartLCReads	= false;
	pHBMcObj->CurrentRead	= 0;
	pHBMcObj->FirstTimeThru = true;
	
	hLoadCellInitialized[LOADCELL_2] = RtOpenEvent(NULL, NULL, "Load Cell 2 Initialized"); //GLC added 2/19/04

	if (hLoadCellInitialized[LOADCELL_2] == NULL)
	{
		RtPrintf("Failed to open event -Load Cell 2 Initialized- \n");
	}

//----- Open communications port

	DebugTrace(_HBMLDCELL_, "Opening COMPORT%d . . .\n", serObj->ucbObj.port + 1 );

	if (RtWaitForSingleObject(hSerialQ, 100) != WAIT_OBJECT_0)
	{
		RtPrintf("hSerMtx RtWaitForSingleObject failed in file %s, line %d \n", _FILE_, __LINE__);
	}
	else
	{
		if ((rc = serObj->RtOpenComPort(COM_BAUD_RATE_38400, COM_WORDSIZE_8, // TG - Commented out COM_BAUD_RATE_38400
                         COM_STOPBITS_1, COM_PARITY_NONE, COM_FLOW_CONTROL_HARDWARE)) != NO_ERRORS)
        {
            RtPrintf("Error COM%d Open rc = %d file %s, line %d \n", serObj->ucbObj.port + 1,
					rc, _FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        //Release the mutex
		if (!RtReleaseMutex(hSerialQ))
		{
			RtPrintf("Release failed in file %s, line %d \n", _FILE_, __LINE__);
		}
    }

 	if (pHBMcObj->init_adc(AVG, 2) != TRUE)
	{
		RtPrintf("HBM Load Cell 2 thread exiting - init failed.\n");
		for(;;) { Sleep(5000); }
	}

	if (!RtSetEvent(hLoadCellInitialized[LOADCELL_2])) // GLC added 2/19/04
    {
        RtPrintf("Error %u file %s, line %d \n",GetLastError(), _FILE_, __LINE__);
    }
///////////////////////////////////////////////////////////////////////////////
//                               Main Loop
///////////////////////////////////////////////////////////////////////////////

    for(;;)
    {   
		if (LoopCnt%2500 == 0)
		{
			pHBMcObj->HBMCheckSettings(); //GLC
			LoopCnt = 0;
		}

		LoopCnt++;
		
        //---- Read measurements ----
        if (pHBMcObj->ContMeasOut)
		{
            pHBMcObj->SerialRead(serObj);
			Sleep (MEASURE_MODE_SLEEP);
		}
		else
		{
			Sleep(MISC_MODE_SLEEP);
		}
    }
}

int RTFCNDCL HBMLoadCell::HBMLoadCellThread3(PVOID pHBMInit)
{

    WORD  rc = FALSE;
	ULONG	LoopCnt = 0;

//----- Create comm objects
	
	HBMinit*	pHBMInitObj;

	Serial* serObj;
	HBMLoadCell * pHBMcObj;
	HANDLE	hSerialQ;

	pHBMInitObj = (HBMinit *)pHBMInit;

	serObj = pHBMInitObj->pSerial;
	pHBMcObj = pHBMInitObj->pHBMLoadCell;
	hSerialQ = (HANDLE *)pHBMInitObj->phSerMtx;
	
	pHBMcObj->cPrintCnt			= 0;
	pHBMcObj->CheckSumError		= 0;
	pHBMcObj->ExtraReadCnt		= 0;

	pHBMcObj->LCReadCounter	= 0;
	pHBMcObj->StartLCReads	= false;
	pHBMcObj->CurrentRead	= 0;
	pHBMcObj->FirstTimeThru = true;
    
	hLoadCellInitialized[LOADCELL_3] = RtOpenEvent(NULL, NULL, "Load Cell 3 Initialized"); //GLC added 2/19/04

//----- Open communications port

	DebugTrace(_HBMLDCELL_, "Opening COMPORT%d . . .\n", serObj->ucbObj.port + 1 );

	if (RtWaitForSingleObject(hSerialQ, 100) != WAIT_OBJECT_0)
	{
		RtPrintf("hSerMtx RtWaitForSingleObject failed in file %s, line %d \n", _FILE_, __LINE__);
	}
	else
	{
		if ((rc = serObj->RtOpenComPort(COM_BAUD_RATE_38400, COM_WORDSIZE_8, // TG - Commented out COM_BAUD_RATE_38400
                        COM_STOPBITS_1, COM_PARITY_NONE, COM_FLOW_CONTROL_HARDWARE)) != NO_ERRORS)
        {
            RtPrintf("Error COM%d Open rc = %d file %s, line %d \n", serObj->ucbObj.port + 1,
					rc, _FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        //Release the mutex
		if (!RtReleaseMutex(hSerialQ))
		{
			RtPrintf("Release failed in file %s, line %d \n", _FILE_, __LINE__);
		}
    }
	
	if (pHBMcObj->init_adc(AVG, 2) != TRUE)
	{
		RtPrintf("HBM Load Cell 3 thread exiting - init failed.\n");
		for(;;) { Sleep(5000); }
	}

    if (!RtSetEvent(hLoadCellInitialized[LOADCELL_3])) // GLC added 2/19/04
    {
        RtPrintf("Error %u file %s, line %d \n",GetLastError(), _FILE_, __LINE__);
    }
///////////////////////////////////////////////////////////////////////////////
//                               Main Loop
///////////////////////////////////////////////////////////////////////////////

    for(;;)
    {   
		if (LoopCnt%2500 == 0)
		{
			pHBMcObj->HBMCheckSettings(); //GLC
			LoopCnt = 0;
		}

		LoopCnt++;
		
        //---- Read measurements ----
        if (pHBMcObj->ContMeasOut)
		{
            pHBMcObj->SerialRead(serObj);
			Sleep (MEASURE_MODE_SLEEP);
		}
		else
		{
			Sleep(MISC_MODE_SLEEP);
		}
    }
}

int RTFCNDCL HBMLoadCell::HBMLoadCellThread4(PVOID pHBMInit)
{

    WORD  rc = FALSE;
	ULONG	LoopCnt = 0;

//----- Create comm objects
	
	HBMinit*	pHBMInitObj;

	Serial* serObj;
	HBMLoadCell * pHBMcObj;
	HANDLE	hSerialQ;

	pHBMInitObj = (HBMinit *)pHBMInit;

	serObj = pHBMInitObj->pSerial;
	pHBMcObj = pHBMInitObj->pHBMLoadCell;
	hSerialQ = (HANDLE *)pHBMInitObj->phSerMtx;

	pHBMcObj->cPrintCnt			= 0;
	pHBMcObj->CheckSumError		= 0;
	pHBMcObj->ExtraReadCnt		= 0;

	pHBMcObj->LCReadCounter	= 0;
	pHBMcObj->StartLCReads	= false;
	pHBMcObj->CurrentRead	= 0;
	pHBMcObj->FirstTimeThru = true;
    
	hLoadCellInitialized[LOADCELL_4] = RtOpenEvent(NULL, NULL, "Load Cell 4 Initialized"); //GLC added 2/19/04

//----- Open communications port

	DebugTrace(_HBMLDCELL_, "Opening COMPORT%d . . .\n", serObj->ucbObj.port + 1 );

	if (RtWaitForSingleObject(hSerialQ, 100) != WAIT_OBJECT_0)
	{
		RtPrintf("hSerMtx RtWaitForSingleObject failed in file %s, line %d \n", _FILE_, __LINE__);
	}
	else
	{
		if ((rc = serObj->RtOpenComPort(COM_BAUD_RATE_38400, COM_WORDSIZE_8, // TG - Commented out COM_BAUD_RATE_38400
                         COM_STOPBITS_1, COM_PARITY_NONE, COM_FLOW_CONTROL_HARDWARE)) != NO_ERRORS)
        {
            RtPrintf("Error COM%d Open rc = %d file %s, line %d \n", serObj->ucbObj.port + 1,
					rc, _FILE_, __LINE__);
            return ERROR_OCCURED;
        }

        //Release the mutex
		if (!RtReleaseMutex(hSerialQ))
		{
			RtPrintf("Release failed in file %s, line %d \n", _FILE_, __LINE__);
		}
    }

	if (pHBMcObj->init_adc(AVG, 2) != TRUE)
	{
		RtPrintf("HBM Load Cell 4 thread exiting - init failed.\n");
		for(;;) { Sleep(5000); }
	}

    if (!RtSetEvent(hLoadCellInitialized[3])) // GLC added 2/19/04
    {
        RtPrintf("Error %u file %s, line %d \n",GetLastError(), _FILE_, __LINE__);
    }
///////////////////////////////////////////////////////////////////////////////
//                               Main Loop
///////////////////////////////////////////////////////////////////////////////

    for(;;)
    {   
		if (LoopCnt%2500 == 0)
		{
			pHBMcObj->HBMCheckSettings(); //GLC
			LoopCnt = 0;
		}

		LoopCnt++;
		
        //---- Read measurements ----
        if (pHBMcObj->ContMeasOut)
		{
            pHBMcObj->SerialRead(serObj);
			Sleep (MEASURE_MODE_SLEEP);
		}
		else
		{
			Sleep(MISC_MODE_SLEEP);
		}
    }
}
//---------------------------------------------------------------------------
// HBMLoadCell::Meas_Mode_From_COF - Determine output characteristics from COF
//---------------------------------------------------------------------------
int HBMLoadCell::Meas_Mode_From_COF(int COF, Output_Format * mmode)
{

	bool blnFound = false;

	//COF values for binary output
	const int BIN_ARRAY_SIZE = 18;
	int IntBinArray[BIN_ARRAY_SIZE] = {0,2,4,6,8,12,16,18,20,22,24,28,32,34,36,38,40,44}; 

//	//COF values for ASCII output
//	const int ASC_ARRAY_SIZE = 18;
//	int IntASCArray[ASC_ARRAY_SIZE] = {1,3,5,7,9,11,17,19,21,23,25,27,33,35,37,39,41,43};    
	
	//COF values for ASCII address output
	const int ADDR_ARRAY_SIZE = 9;
	int IntADDRArray[ADDR_ARRAY_SIZE] = {1,5,9,17,21,25,33,37,41};    

	//COF values for 2 byte binary output
	const int TWO_BYTE_ARRAY_SIZE = 6;
    int Int2ByteArray[TWO_BYTE_ARRAY_SIZE] = {2,6,18,24,34,38};
    
//	//COF values for 4 byte binary output
//	const int FOUR_BYTE_ARRAY_SIZE = 12;
//	int Int4ByteArray[FOUR_BYTE_ARRAY_SIZE] = {0,4,8,12,16,20,24,28,32,36,40,44};

	//COF values for MSB_LSB
	const int MSBLSB_ARRAY_SIZE = 9;
	int IntMSBLSBArray[MSBLSB_ARRAY_SIZE] = {0,2,8,16,18,24,32,34,40}; 
    
//	//COF values for LSB_MSB
//	const int LSBMSB_ARRAY_SIZE = 9;
//	int IntLSBMSBArray[LSBMSB_ARRAY_SIZE] = {4,6,12,20,22,28,36,38,44};
	
	//COF values for Status
	const int BIN_STATUS_ARRAY_SIZE = 6;
	int IntBinStatusArray[BIN_STATUS_ARRAY_SIZE] = {8,12,24,28,40,44}; 

	const int ASC_STATUS_ARRAY_SIZE = 6;
	int IntASCStatusArray[ASC_STATUS_ARRAY_SIZE] = {9,11,25,27,41,43}; 
    
	//----- Determine output mode - Binary or ASCII ----
	if ((blnFound = FindInIntArray(COF, IntBinArray, BIN_ARRAY_SIZE)) != NULL)
	// Binary
	{
		DebugTrace(_HBMLDCELL_,"Mode Binary, ");
		mmode->Mode = COF_MODE_BINARY;
	
		//----- Set address to none ----
		mmode->Addr = COF_ADDR_NONE;
		DebugTrace(_HBMLDCELL_,"Address None, ");

		//----- Determine bytes ----
		if ((blnFound = FindInIntArray(COF, Int2ByteArray, TWO_BYTE_ARRAY_SIZE)) != NULL) {
			mmode->Bytes = 2;
            DebugTrace(_HBMLDCELL_,"Bytes 2, ");
		}
		else {
			mmode->Bytes = 4;
			DebugTrace(_HBMLDCELL_,"Bytes 4, ");
		}

		//----- Determine byte order ----
		if ((blnFound = FindInIntArray(COF, IntMSBLSBArray, MSBLSB_ARRAY_SIZE)) != NULL) {
			mmode->ByteOrder = COF_BYTEORDER_MSB_LSB;
			DebugTrace(_HBMLDCELL_,"Order MSB->LSB, ");
		} 
		else {
			mmode->ByteOrder = COF_BYTEORDER_LSB_MSB;
			DebugTrace(_HBMLDCELL_,"Order LSB->MSB, ");
		}
		

		//----- Determine if status is in output ----
		if ((blnFound = FindInIntArray(COF, IntBinStatusArray, BIN_STATUS_ARRAY_SIZE)) != NULL) {
			mmode->Status = COF_STATUS_LSB;
			DebugTrace(_HBMLDCELL_,"Status LSB, ");
		}
		else {
			mmode->Status = COF_STATUS_NONE;
			DebugTrace(_HBMLDCELL_,"Status None, ");
		}

	}
	else
	// ASCII
	{
		mmode->Mode = COF_MODE_ASCII;
		DebugTrace(_HBMLDCELL_,"Mode ASCII, ");

		//----- Set bytes to none ----
		mmode->Bytes = 0;

		// Determine if address is in the output
		if((blnFound = FindInIntArray(COF, IntADDRArray, ADDR_ARRAY_SIZE)) != NULL) {
			mmode->Addr = COF_ADDR_PRM2;
			DebugTrace(_HBMLDCELL_,"Address PRM2, ");
		}
		else {
			mmode->Addr = COF_ADDR_NONE;
			DebugTrace(_HBMLDCELL_,"Address None, ");
		}
	
		// Determine if status is in output 
		if ((blnFound = FindInIntArray(COF, IntASCStatusArray, ASC_STATUS_ARRAY_SIZE)) != NULL) {
			mmode->Status = COF_STATUS_PRM3;
			DebugTrace(_HBMLDCELL_,"Status PRM3, ");
		}
		else {
			mmode->Status = COF_STATUS_NONE;
			DebugTrace(_HBMLDCELL_,"Status None, ");
		}
	}


	//----- Determine EOC terminator ----
	if (COF>=0 && COF<=12) {
		mmode->EOC = COF_EOC_CRLF;
		DebugTrace(_HBMLDCELL_,"EOC CRLF \n");
	}
	else {
		mmode->EOC = COF_EOC_NO_CRLF;
		DebugTrace(_HBMLDCELL_,"EOC None \n");
	}

	return NO_ERRORS;
}


//---------------------------------------------------------------------------
// HBMLoadCell::FlushComInBuffer - Flush all characters waiting at Com input 
//---------------------------------------------------------------------------
int HBMLoadCell::FlushComInBuffer()
{
    WORD    rc, bytes_read;
    WORD    buf_bytes = 0;

	if (RtWaitForSingleObject(hSerMtx, 100) != WAIT_OBJECT_0)
	{
		RtPrintf("FlushComInBuffer:hSerMtx RtWaitForSingleObject failed in file %s, line %d \n", _FILE_, __LINE__);
	}
	else
	{
	    rc = this->serialObj->RtGetComBufferCount(&buf_bytes);
	    
        DebugTrace(_HBMLDCELL_,"FlushComInBuffer Count: %d \n", buf_bytes); //GLC
        if (buf_bytes > 0)
        {
	        rc = this->serialObj->RtReadComPort((BYTE*) &this->rxmsg[0], buf_bytes, &bytes_read);
            DebugTrace(_HBMLDCELL_,"Rx Buffer Flushed %d\n", bytes_read);
        }
        
		memset((char*)&this->rxmsg[0], 0, HBM_BUFFER_SIZE );

        //Release the mutex
		if (!RtReleaseMutex(this->hSerMtx))
		{
			RtPrintf("Release failed in file %s, line %d \n", _FILE_, __LINE__);
		}
    }
    return NO_ERRORS;
}

//---------------------------------------------------------------------------
// HBMLoadCell::SetOutputFormat - Interface to COF command 
//								  (sets measurement output format)
//---------------------------------------------------------------------------
int HBMLoadCell::SetOutputFormat(int COF)
{
	int i;
	char txbuffer[30];
    this->blnOutputStopped = false;

    if (this->ContMeasOut)
    {
        this->StopContinuousOutput();
    }

    // Copy the command with parameter to write buffer
	i = sprintf(txbuffer, "COF %d;", COF);

	// Send write buffer to Tx queue
	i = this->SendCommand(txbuffer, 3000);
	if (i != NO_ERRORS)
	{
		RtPrintf("Load cell write failed in file %s, line %d \n", _FILE_, __LINE__);
		return ERROR_OCCURED;
	}
    
	// Populate measurement info based on COF
	i = this->Meas_Mode_From_COF(COF, &meas_mode);
	DebugTrace(_HBMLDCELL_,"Meas Mode: Mode %d, Bytes %d, ByteOrder %d, Status %d, EOC %d, Addr %d \n", 
		meas_mode.Mode, meas_mode.Bytes, meas_mode.ByteOrder, meas_mode.Status, 
		meas_mode.EOC, meas_mode.Addr);
	
	if (this->blnOutputStopped)
    {
        this->StartContinuousOutput();
    }

    return i;

}

// ------------------------------------------------
// Find an integer value in an array
// ------------------------------------------------
bool HBMLoadCell::FindInIntArray(int tgt, int arr[], int intArrSize)
{
	
	bool	blnFound	= false;
	int		i			= 0;

	for (i=0; i < intArrSize; i++)
	{
		if (arr[i] == tgt)
		{
			blnFound = true;
			break;		
		}
	}
	
	return blnFound;
}


//---------------------------------------------------------------------------
// HBMLoadCell::MeasureQ - Add or remove a measurement from the meas queue
//---------------------------------------------------------------------------
int HBMLoadCell::MeasureQ(__int64 * measArr, int items)
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

	if (((this->cPrintCnt%10000) == 0) && (this->cPrintCnt != 0))
	{
		RtPrintf("MeasureQ for Loadcell = %d\n", this->LoadCellNum);
	}
	
	// Request Receive Queue Mutex
	if ( RtWaitForSingleObject( this->hMeasQ, 100) != WAIT_OBJECT_0)
	{
		RtPrintf("RtWaitForSingleObject failed in file %s, line %d Loadcell %d\n", _FILE_, __LINE__, this->LoadCellNum);
	}
	else
	{
		switch (items)
		{
		case 0:

            // Add the data to the Q
			this->pInMeasQ->meas = measArr[0];
			this->pInMeasQ->state = true;

			// Update so we know we are getting readings
			this->InvalidMeasureCnt = 0;

			// Set the pointer for reading from the queue
			// to the location of the latest value added to the queue
			this->pOutMeasQ = this->pInMeasQ;

			// Point to the next location in the queue
			this->pInMeasQ++;

			// If we made it to the end of the queue
			// start back at the front of the queue
			if (this->pInMeasQ == &this->MeasQ[MEASQ_MAX_ITEMS])
			{
				//DebugTrace(_HBMLDCELL_, "Wrapped around in MeasQ ");
				//DebugTrace(_HBMLDCELL_, "pInMeasQ=0x%08x pOutMeasQ=0x%08x\n", pInMeasQ, pOutMeasQ);
				this->pInMeasQ = &this->MeasQ[0];
				this->bMeasQWrapped = true;
			}
			break;

		default:
            // RtPrintf("Q-"); //GLC
            // Read one or more measurements
			pTmpPointer = this->pOutMeasQ;
			for (i=0; i < items; i++)
			{
#ifndef _MEASUREQ_ALLOW_MULTI_READ_
				// Check for a valid reading
				if (this->pOutMeasQ->state == true)
				{
#endif
					measArr[i] = this->pOutMeasQ->meas;
					this->pOutMeasQ->state = false;

					// Make sure we are not at the beginning of the queue
					if (this->pOutMeasQ != &this->MeasQ[0])
					{
						// Point to the next position if the queue
						this->pOutMeasQ--;
					}
					// Give this a try later RCB 12-18-03
					else if (this->bMeasQWrapped == true)
					{
						this->pOutMeasQ = &this->MeasQ[MEASQ_MAX_ITEMS - 1];
						//DebugTrace(_HBMLDCELL_, "Reading from MeasQ wrapped to end\n");
					}
					else
					{
						//DebugTrace(_HBMLDCELL_, "No new measurements to read\n");
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
						// Increament the counter here, once it reaches RETRY_CNT_MAX +1 it will peg,
						// It will only reset if MeasureQ is called for putting in a new reading.
						// For now just use the latest reading
						this->InvalidMeasureCnt++;
						measArr[i] = this->pOutMeasQ->meas;
					}
#endif
				}
			}
			this->pOutMeasQ = pTmpPointer;
			break;
		}
		
		//Release the mutex
		if (!RtReleaseMutex(this->hMeasQ))
		{
			RtPrintf("Release failed in file %s, line %d \n", _FILE_, __LINE__);
		}

	}

	return NO_ERRORS;

}

/*******************************************************************************/
/*                                                                             */
/* WriteLCReadsQ:	Write weight samples to LCReads.txt file                   */
/*                                                                             */
/*******************************************************************************/

int HBMLoadCell::WriteLCReadsQ()
{
    static      HANDLE hFile;
    ULONG       rw_cnt;
    int         status = NO_ERRORS;
	char		Buffer[80];
	char		FileName[30];
	int			Cnt;

	sprintf((char *)&FileName[0], "%s%d.txt", LCREADS_PATH, this->LoadCellNum);

    hFile = CreateFile( (char *)&FileName[0],
                        GENERIC_READ |
                        GENERIC_WRITE,
                        0, NULL, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL,
                        NULL);

    if (hFile != INVALID_HANDLE_VALUE)
    {
		for (int i = 0; i < this->LCReadsCaptured; i++)
		{
			Cnt = sprintf(&Buffer[0], "%ld\r\n",
				this->SampleLCReadQ[i]);

			WriteFile(hFile, &Buffer[0], Cnt, &rw_cnt, NULL);
		}
     }
    else
    {
        RtPrintf("Write file error %u file %s\n", GetLastError(), (char *)&FileName[0]);
        status = ERROR_OCCURED;
    }

    CloseHandle(hFile);

	RtPrintf("File %s written\n", (char *)&FileName[0]);

	return 0;
}

//---------------------------------------------------------------------------
// HBMLoadCell::SampleLCReadMeasureQ - Add a loadcell read to the sample queue
//---------------------------------------------------------------------------
int HBMLoadCell::SampleLCReadMeasureQ(__int64 * measArr)
{

	if ((this->LCReadCounter == 0) || this->StartLCReads)
	{
		this->StartLCReads = true;

		if (this->LCReadCounter == 0)
			RtPrintf("Started taking %d reads from HBM loadcell %d\n", ReadsToSample, this->LoadCellNum);

		// Point to the next location in the queue
		this->SampleLCReadQ[LCReadCounter] = *measArr;
		this->LCReadCounter++;

		if ((this->LCReadCounter == MAX_MEAS_SAMPLES) || (this->LCReadCounter == ReadsToSample))
		{
			this->LCReadsCaptured = this->LCReadCounter;
			this->LCReadCounter = 0;
			TakeLoadCellReadsFlag = TakeLoadCellReadsFlag - 1;
			this->StartLCReads = false;
			app->WriteLCReadsToFile = app->WriteLCReadsToFile - 1;
			RtPrintf("Done taking %d reads from HBM loadcell %d\n", this->LCReadsCaptured, this->LoadCellNum);
		}
	}
	return NO_ERRORS;
}

int HBMLoadCell::init_adc(int mode, int num_reads)
{
	int i;
    int cntFlush = 0;
	int cntSendCmd = 0;
	WORD buf_bytes = 0;
	//char Cmd[8];
	bool Config = true;

    this->blnOutputStopped = false;

	// First verify load cell is responding with IDN? command
	// Retry up to 3 times before giving up
	{
		bool lc_responding = false;
		for (int retry = 0; retry < 3; retry++)
		{
			RtPrintf("HBM Load Cell: Sending IDN? (attempt %d of 3)...\n", retry + 1);
			int rc = this->SendCommand("IDN?;", 3000);
			if (rc == NO_ERRORS && this->rxmsg[0] != 0)
			{
				RtPrintf("HBM Load Cell: Responded - %s\n", (char*)&this->rxmsg[0]);
				lc_responding = true;
				break;
			}
			RtPrintf("HBM Load Cell: No response.\n");
			Sleep(1000);
		}
		if (!lc_responding)
		{
			RtPrintf("\n");
			RtPrintf("*** HBM Load Cell is NOT responding. ***\n");
			RtPrintf("*** Check power and serial connections. ***\n");
			RtPrintf("\n");
			app->GenError(warning, (char*)"HBM Load Cell not responding - check power and connections.\n");
			return(FALSE);
		}
	}

//DRW This is no longer used
//	this->SendCommand("RES;", 0);

	// Switch in the 500 ohm terminating resistor - RCB - 485
	if (this->serialObj->ucbObj.mode == RS422) //Need to add 232/422 mode option
	{
		this->SendCommand("STR 1;", 3000); // DRW Changed from 3000 to 1000 as a test
	}
	else
	{
		this->SendCommand("STR 0;", 3000);
	}

	this->serialObj->RtGetComBufferCount(&buf_bytes);

	while ((buf_bytes != 0) && (cntSendCmd++ <= 5))
	{

		while ((cntFlush++ <= 5) && (buf_bytes != 0))
		{
				
			this->serialObj->RtGetComBufferCount(&buf_bytes);
			this->FlushComInBuffer();
			Sleep(10);
		}

		if (buf_bytes == 0)
		{
			break;
		}
		else
		{
	//DRW No Longer Used		
	//		this->SendCommand("RES;", 0); 
			cntFlush = 0;
		}
	}

    for(i = 0; i < HBM_NUM_CH; i++)
    {
		//RtPrintf("\nHBM::init_adc - num_reads =%d\n", num_reads);
        this->adc_offset   [i]    = ADCOFFSET;
        this->num_adc_reads[i]    = num_reads;
        this->adc_mode     [i]    = mode;
        this->error_sent[i]       = false;

        this->set_adc_num_reads(i, num_adc_reads[i]);
        this->set_adc_offset   (i, adc_offset   [i]);
        this->set_adc_mode     (i, adc_mode     [i]);
    }

    if (this->ContMeasOut)
    {
        this->StopContinuousOutput();
    }

    this->read_adc_pn();
    this->read_adc_version();


//------------- copy part and version number info

    for(i = 0; i < ADCINFOMAX; i++)
    {
        app->pShm->sys_stat.adc_part[i]    = this->left_adc_pn[i];
        app->pShm->sys_stat.adc_version[i] = this->left_adc_version[i];
    }

	// Check the BAUD rate setting
	if (this->CheckConfigValue("BDR?;", "38400,0\r\n", 500) == false)
	{
		Config = false;
		RtPrintf("BDR value not set correctly. file %s, line %d\n",_FILE_, __LINE__);
	}

	// Set the output format to 4 byte value with checksum
	this->SetOutputFormat(8);
	if (this->CheckConfigIntValue("COF?;", 8, 0, 3000) == false)
	{
		Config = false;
		RtPrintf("COF value not set correctly. file %s, line %d\n",_FILE_, __LINE__);
	}

	// Enable checksum
	this->SendCommand("CSM 1;", 3000); //RCB enable CheckSum
	if (this->CheckConfigIntValue("CSM?;", 1, 0, 3000) == false)
	{
		Config = false;
		RtPrintf("CSM value not set correctly. file %s, line %d\n",_FILE_, __LINE__);
	}

	// Set to continuous read measurement mode
    this->StartContinuousOutput(); //GLC
	
    if (Config == true)
	{
		return(TRUE);
	}
	else
	{
		// Send error text to the front-end
		sprintf(app_err_buf,"HBMLoadCell::init_adc - Error in HBMLoadCell Configuration\n");
	    app->GenError(warning, app_err_buf);
		return(FALSE);
	}
}

void HBMLoadCell::set_adc_num_reads(int channel, int reads)
{
    DebugTrace(_HBMLDCELL_,"Set HBM channel %d reads %d\n",channel, reads);
    if ( (reads > 0) && 
         (reads < HBM_MAX_ADC_READS) )
    {
        this->num_adc_reads[channel] = reads;
    }
    else
	{
        RtPrintf("Error set adc reads %d chan %d file %s, line %d \n",
                  reads, channel, _FILE_, __LINE__);
	}
}

int HBMLoadCell::read_adc_num_reads(int channel)
{
    return(this->num_adc_reads[channel]);
}

void HBMLoadCell::KillTime(int iTime)
{
    int iCounter;
    for (iCounter=0;iCounter<iTime;iCounter++){}
}

void HBMLoadCell::set_adc_mode(int channel,int mode)
{
    DebugTrace(_HBMLDCELL_,"Set channel %d mode %d\n",channel, mode);
    if ( (mode >= INDIVID) && (mode <=  AVG) )
    {
        this->adc_mode[channel] = mode;
    }
    else
        RtPrintf("Error set adc mode %d chan %d file %s, line %d \n",
                  mode, channel, _FILE_, __LINE__);
}

int HBMLoadCell::read_adc_mode(int channel)
{

    return(this->adc_mode[channel]);
}

void HBMLoadCell::read_adc_version(void)
{
    char*		ptr;
	char*		pVersion;
    
    ptr = (char*)&this->left_adc_version[0];

	// We need to fill in the info
    this->SendCommand("IDN?;", 3000);
	pVersion = strtok((char*)&this->rxmsg[0],",");
	pVersion = strtok(NULL,",");
	if (pVersion != NULL)
	{
		strncpy(ptr, pVersion, ADCINFOMAX);
	}

}

void HBMLoadCell::set_adc_offset(int channel,int offset) {

    bool good_offset = false;

    DebugTrace(_HBMLDCELL_,"Set channel %d offset %d\n",channel, offset);

  good_offset = true;

  if (good_offset)
  {
  }
  else
      RtPrintf("Error Offset %d file %s, line %d \n", offset, _FILE_, __LINE__);

}

void HBMLoadCell::read_adc_pn()
{
    char*		ptr;
   	char*		pPartNum;


    ptr = (char *)&this->left_adc_pn[0];
	//DRW
//	strcpy((char*)&this->rxmsg[0],"not working");
	// We need to fill in the info
    this->SendCommand("IDN?;", 3000);
	pPartNum = strtok((char*)&this->rxmsg[0],",");
	//DRW
//DRW	RtPrintf("read_adc_pn: %s\n",&this->rxmsg[0]);
	pPartNum = strtok(NULL,",");
	pPartNum = strtok(NULL,",");
	if (pPartNum != NULL)
	{
		strncpy(ptr, pPartNum, ADCINFOMAX);
	}
}

//---------------------------------------------------------------------------
// HBMLoadCell::read_load_cell - Simulate old method of load cell reads
//---------------------------------------------------------------------------
__int64 HBMLoadCell::read_load_cell(int channel)
{
    int   k, rcnt;
    __int64 rdg, cum_rdg;

    rcnt    = 0;
    cum_rdg = 0;

    if (this->num_adc_reads[channel] > 0)
    {
        this->MeasureQ(app->individual_wts, this->num_adc_reads[channel]); //GLC TEMP REMOVE

        for(k = 0; k <this->num_adc_reads[channel]; k++)
        {

            rdg = app->individual_wts[k];
            //RtPrintf("Reading %d = %d\n", k, app->individual_wts[k]); //GLC
            switch(this->adc_mode[channel])
            {
                case AVG:

                    LC_ERR_CLR
                    return(rdg);
                    break;

                case INDIVID:

                    if ( (this->num_adc_reads[channel] > 0) && 
                         (this->num_adc_reads[channel] < MEASQ_MAX_ITEMS) )
                    {
                        //app->individual_wts[k] = rdg;
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
                    // bad num reads
                    else 
                    {
                        //RtPrintf("Bad Num Reads: %d\n", num_adc_reads[channel]); //TEMP GLC
                        LC_ERR_SET
                    }
                    break;

                // bad mode
                default:
                    //RtPrintf("Bad Mode: %d\n", adc_mode[channel]); //TEMP GLC
                    LC_ERR_SET
                    break;
            }
        }
    }
    else
    {
        RtPrintf("Cannot read load cell. ADC Reads set to %d\n", num_adc_reads[channel]);
    }

    return(0);
}


void HBMLoadCell::write_adc(int channel,unsigned char outbyte)  /* write command or data byte to adc */
{
	channel; // RED - Added to eliminate waring for unreferenced formal parameter
	outbyte; // RED - Added to eliminate waring for unreferenced formal parameter

}

char HBMLoadCell::read_adc(int channel)
{
	channel; // RED - Added to eliminate waring for unreferenced formal parameter
    int ctr;
    ctr = 0;

    //RtReadPortUchar(adc_command_port[channel]);
/*
    while (!(RtReadPortUchar(adc_status_port[channel]) & (unsigned char)0x80)) 
    {
        if(ctr++ > 1000) 
        {
            RtPrintf("Error chan %d file %s, line %d \n",channel, _FILE_, __LINE__);
            return(0);
        }
    }

    /KillTime(10);
    return(RtReadPortUchar(adc_command_port[channel]));
*/
  return(0);
}


//---------------------------------------------------------------------------
// HBMLoadCell::SendCommand - Send a command and wait for a response
//---------------------------------------------------------------------------
int HBMLoadCell::SendCommand(char *cmd, DWORD response_time)
{
	int rc = ERROR_OCCURED;
//    int cntFlush = 0;		RED - Removed becuase it is not referenced
//	int cntSendCmd = 0;		RED - Removed becuase it is not referenced
//	WORD buf_bytes = 0;		RED - Removed becuase it is not referenced
	//	RED - Line below changed to unsigned to match value it is being compared against
	unsigned int ResponseCnt = 0; // Start at 0 so we always enter the wait loop. Was 1000 which broke BDR check (response_time=500).
    
	if (this->blnCmdSent == true)
	{
		return(ERROR_OCCURED);
	}

    //DRW shut this off
//	RtPrintf("SendCommand: %s\n", cmd);//GLC
    if (this->ContMeasOut)
    {
        this->StopContinuousOutput();
    }

	if (strcmp(cmd, "STP;") == 0) return(0); //GLC exit on stop command
    
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
			ResponseCnt += RETRY_CNT_MAX; //Add in delay from SerialRead
			
			if ((ResponseCnt >= response_time) && (this->blnResponseReceived != true))
			{
				this->blnCmdSent =    false;
				RtPrintf("Timeout in serial response for command %s\n", cmd); 
				break;
			}
	
		}

		if (this->blnResponseReceived == true)
		{
			rc = NO_ERRORS;
			// DRW Just added this inside SendCommand
//			RtPrintf("Port %d SendCommand::received->%s\n", 
//				this->serialObj->ucbObj.port, (char *)&this->rxmsg[0]);
	
//DRW Removed this for test
			DebugTrace(_HBMLDCELL_,"Port %d SendCommand::received->%s\n", 
				this->serialObj->ucbObj.port, (char *)&this->rxmsg[0]);
		}

    }
	else
	{
		if (strcmp(cmd, "RES;") == 0)
		{
			Sleep(3000); // DRW Changed from 3000 to 2000 and it just failed on all
 			memset( (char *)&this->rxmsg[0], 0, HBM_BUFFER_SIZE);
		}
	}

    this->blnCmdSent =    false;

    //Look for continuous output request or stop request
	if ((strcmp(cmd, "MSV?0;") == 0) || (strcmp(cmd, "MSV? 0;") == 0))
	{
		this->ContMeasOut = true;
		this->blnMeas = true;
		DebugTrace(_HBMLDCELL_, "Continuous Output Mode Enabled \n");
	}
	else if (strstr(cmd, "MSV") != NULL)
	{
		this->ContMeasOut = false;
		this->blnMeas = true;
	}
	else if (strcmp(cmd, "MAV?;") == 0)
	{
		this->ContMeasOut = false;
		this->blnMeas = true;
	}
	else
	{
		this->blnMeas = false;
	}

    return rc;
}


bool HBMLoadCell::CheckConfigValue(char * cmd, char * value, DWORD response_time)
{
	bool rc = false;
	//  DRW Added this for testing need to remove when finished
//			RtPrintf("Starting CheckConfigValue here ");
    // Send the command
	if (this->SendCommand(cmd, response_time) == NO_ERRORS)
	{
			//DRW Need to remove this when Im done
//			RtPrintf("Port %d CheckConfigValue:return false received->%s\n",
//				this->serialObj->ucbObj.port,
//				(char *)&this->rxmsg[0]);
//	DRW Put this back 	
			DebugTrace(_HBMLDCELL_,"Port %d CheckConfigValue::value->%s\n",
			this->serialObj->ucbObj.port, (char *)&value[0]);
		if((strcmp((char *)&this->rxmsg[0], (char *)&value[0])) == 0)
		{
			rc = true;
			//DRW Need to remove this when Im done
		//	RtPrintf("Port %d CheckConfigValue:return false received->%s\n",
		//		this->serialObj->ucbObj.port,
		//		(char *)&this->rxmsg[0]);

//		DRW Put this back when Im done	
			DebugTrace(_HBMLDCELL_,
				"Port% d CheckConfigValue:return true received->%s\n", 
				this->serialObj->ucbObj.port,
				(char *)&this->rxmsg[0]);
		}
		else
		{
			//DRW Take this out when Im done
//			RtPrintf("Port %d CheckConfigValue:return false received->%s\n",
//				this->serialObj->ucbObj.port,
//				(char *)&this->rxmsg[0]);

			rc = false;
// DRW Put This back when Im done
			DebugTrace(_HBMLDCELL_,
				"Port %d CheckConfigValue:return false received->%s\n",
				this->serialObj->ucbObj.port,
				(char *)&this->rxmsg[0]);
		}
	}

	//  DRW Added this for testing need to remove when finished
//			RtPrintf("Ending CheckConfigValue here ");
	return rc;

}

//---------------------------------------------------------------------------
// HBMLoadCell::SerialWrite - Write data to the serial port
//---------------------------------------------------------------------------
int HBMLoadCell::SerialWrite(char *txdata)
{

	WORD rc = NO_ERRORS;
	WORD bytes_wrtn;

	if (RtWaitForSingleObject(this->hSerMtx, 100) != WAIT_OBJECT_0)
	{
		RtPrintf("hSerMtx RtWaitForSingleObject failed in file %s, line %d \n", _FILE_, __LINE__);
	}
	else
	{
        // TBA Verify data is valid to write GLC

	    //RtPrintf("Com writing %d Bytes. \n", strlen(txdata));

	    //Write characters to com port
        DebugTrace(_HBMLDCELL_,"Port %d Out: %s\n", this->serialObj->ucbObj.port, txdata);
	    rc = this->serialObj->RtWriteComPort((BYTE*)txdata, (unsigned short)(strlen(txdata)), &bytes_wrtn);

	    if (rc != NO_ERRORS)
		    RtPrintf("Error rc = %d file %s, line %d \n", rc, _FILE_, __LINE__);

	    //RtPrintf("Com wrote %d Bytes. \n", bytes_wrtn); //GLC

        //Release the mutex
		if (!RtReleaseMutex(this->hSerMtx))
		{
			RtPrintf("Release failed in file %s, line %d \n", _FILE_, __LINE__);
		}
    }
    return 1; //TEMP
}

//---------------------------------------------------------------------------
// HBMLoadCell::SerialRead - Send a command and wait for a response
//---------------------------------------------------------------------------
void HBMLoadCell::SerialRead( Serial* serial)
{
	serial; // RED - Added to eliminate waring for unreferenced formal parameter
    WORD    rc, bytes_read;
    WORD    buf_bytes           = 0;
	WORD    NumBytes            = 0;
    int     rxbuffidx           = 0;
	int     RetryCnt            = 0;
	int		ReadCnt			 	= 0;
	unsigned char tmpBuffer;
	
	this->blnResponseReceived = false;

	if (RtWaitForSingleObject(this->hSerMtx, 100) != WAIT_OBJECT_0)
	{
		RtPrintf("hSerMtx RtWaitForSingleObject failed in file %s, line %d \n", _FILE_, __LINE__);
	}
	else
	{
	    rc = this->serialObj->RtGetComBufferCount(&buf_bytes);

        memset( (char *)&this->rxmsg[0], 0, HBM_BUFFER_SIZE);

        if (this->blnCmdSent && !blnMeas)
        {
			//DebugTrace(_HBMLDCELL_,"Port %d In: ", this->serialObj->ucbObj.port);
			while ((!this->blnResponseReceived) && (RetryCnt < RETRY_CNT_MAX))
	        {
				RetryCnt++;
                //---- See if any characters are in serial buffer ----
                if (buf_bytes > 0)
                {
                    //---- Read one character ----
                    rc = this->serialObj->RtReadComPort((BYTE*) &this->rxmsg[rxbuffidx], 1, &bytes_read);
					if(this->rxmsg[rxbuffidx] == 0)
					{
						rc = this->serialObj->RtGetComBufferCount(&buf_bytes);
						Sleep (1);
						continue;
					}
					rxbuffidx++;
					//DebugTrace(_HBMLDCELL_, "%x ",rxmsg[rxbuffidx - 1]);
				}
        
                // ---- Look for CRLF ----
                if (rxbuffidx > 1)
                {
                    if ((this->rxmsg[rxbuffidx - 1] == 0xA) && (this->rxmsg[rxbuffidx - 2] == 0xD))
                    {
                        this->blnResponseReceived = true;
                        //DebugTrace(_HBMLDCELL_, "\n");
                    }
                }

                rc = this->serialObj->RtGetComBufferCount(&buf_bytes);
                Sleep (1);
                
				if (rxbuffidx >= HBM_BUFFER_SIZE)
                {
                    RtPrintf("Serial Read Rx Buffer Full\n");
					break;
                }
	        }
		}
	    else if ((this->blnMeas) && (buf_bytes >= 4)) //&& (buf_bytes%meas_mode.Bytes == 0))
	    // Handle measurement output
	    {
		    __int64 tmpMeas = 0;
			BYTE CheckSum = 0;
			WORD* pToValue;

		    if (meas_mode.Mode == COF_MODE_BINARY) 
            {

				for(ReadCnt = 0; ReadCnt < buf_bytes; ReadCnt += meas_mode.Bytes)
				{
					this->serialObj->RtGetComBufferCount(&NumBytes);

					// If the number of bytes to read is less than 4 we can
					// break out of this and wait until the number of bytes
					// in the buffer has reached a full 4 byte value.
					if (NumBytes < 4)
					{
						break;
					}
				    rc = this->serialObj->RtReadComPort((BYTE*) &this->rxmsg[ReadCnt], (unsigned short)(meas_mode.Bytes), &bytes_read);
				    
	                if (bytes_read == meas_mode.Bytes)
	                {
	                    switch (meas_mode.Bytes)
	                    {
	                    case 2:
	                        if (meas_mode.ByteOrder == COF_BYTEORDER_MSB_LSB)
	                        {
	                            tmpMeas = (this->rxmsg[0] * 0xFF) + (this->rxmsg[1]);
	                        }
	                        else
	                        {
	                            tmpMeas = (this->rxmsg[0]) + (this->rxmsg[1] * 0xFF);
	                        }
	                        if (tmpMeas > 30000)
	                            tmpMeas -= 0xFFFF;
	                        break;
	                    case 4:
	                        if (meas_mode.ByteOrder == COF_BYTEORDER_MSB_LSB)
	                        {
	                            tmpMeas = (this->rxmsg[0] * 0xFFFF) + 
	                                (this->rxmsg[1] * 0xFF) + 
	                                (this->rxmsg[2]);
	                        }
	                        else
	                        {
	                            tmpMeas = (this->rxmsg[1]) + 
	                                (this->rxmsg[2] * 0xFF) + 
	                                (this->rxmsg[3] * 0xFFFF);
	                        }
	                        if (tmpMeas > 0x7FFE81)
	                            tmpMeas -= 0xFFFFFF;
	                        break;
	                    }
	
						// Need to do a check sum and compare to 4th byte
						CheckSum = (unsigned char)(this->rxmsg[0] ^ this->rxmsg[1] ^ this->rxmsg[2]);
	
						if (CheckSum != this->rxmsg[3])
						{
							tmpMeas = 0x0;			//we have a problem
							this->CheckSumError++;
							this->serialObj->RtGetComBufferCount(&NumBytes);
							if (NumBytes > 0)
							{
								// Try to change our allignment by one byte
								this->serialObj->RtReadComPort((BYTE*) &tmpBuffer, 1, &rc);
								this->ExtraReadCnt++;
								//ReadCnt++;
							}
						}
							
						// This just used for debug to check on our status periodically
						if( _HBMLDCELL_ & TraceMask )
						{
							this->cPrintCnt++;
							if ((this->cPrintCnt%10000) == 0)
							{
								this->serialObj->RtGetComBufferCount(&NumBytes);
								pToValue = (WORD *)&tmpMeas;
								DebugTrace(_HBMLDCELL_, 
									"SerialRead bytes in buffer = %d CheckSumError = %d Port = %d\n",
									NumBytes, this->CheckSumError, 
									this->serialObj->ucbObj.port);
							}
						}
						//Write to the measurement queue
						this->MeasureQ(&tmpMeas, 0);

						// Set flag from debugshell
						if (TakeLoadCellReadsFlag > 0)
						{
							this->SampleLCReadMeasureQ(&tmpMeas);
						}
					}
				}
		    }
	    }
        //Release the mutex
		if (!RtReleaseMutex(this->hSerMtx))
		{
			RtPrintf("Release failed in file %s, line %d \n", _FILE_, __LINE__);
		}
    }
}

int HBMLoadCell::StopContinuousOutput()
{
    // Stop continuous output
    this->SerialWrite("STP;");
	Sleep(10);
	this->ContMeasOut = false;
	this->blnMeas = false;
	this->FlushComInBuffer();
	this->blnOutputStopped = true;
	DebugTrace(_HBMLDCELL_, "Continuous Output Mode Disabled \n");
    return NO_ERRORS;
}

int HBMLoadCell::StartContinuousOutput()
{
    // Start continuous output
    this->SerialWrite("MSV?0;");
  	this->ContMeasOut = true;
	this->blnMeas = true;
	this->blnOutputStopped = false;

	DebugTrace(_HBMLDCELL_, "Continuous Output Mode Enabled \n");
    return NO_ERRORS;

}

//---------------------------------------------------------------------------
// HBMLoadCell::HBMCheckSettings - Check for a change in the load cell settings
//---------------------------------------------------------------------------
int HBMLoadCell::HBMCheckSettings()
{
   	char*		pSegment;

    this->blnOutputStopped = false;

    if (this->bFirstCheck)
	{
		this->bFirstCheck = false;
        
        // Stop continuous output if active
        if (this->ContMeasOut)
        {
            this->StopContinuousOutput();
        }
        
		// Read all settings from load cell the first time through
		RtPrintf("--------------------------------\n");
		RtPrintf("HBM Load Cell %d Initial Settings\n", this->LoadCellNum + 1);
		RtPrintf("--------------------------------\n");

        // ASF - Read lowpass filter setting
        if (this->SendCommand("ASF?;", 3000) == NO_ERRORS)
        {
            app->LastDigLCSet[this->LoadCellNum].ASF = atoi((char *)&this->rxmsg[0]);
            RtPrintf("ASF %d\n", app->LastDigLCSet[this->LoadCellNum].ASF); // Response: 05
        }
        
        // FMD - Read filter mode
        if (this->SendCommand("FMD?;", 3000) == NO_ERRORS)
        {
    	    app->LastDigLCSet[this->LoadCellNum].FMD = atoi((char *)&this->rxmsg[0]);
            RtPrintf("FMD %d\n", app->LastDigLCSet[this->LoadCellNum].FMD); // Response: 1
        }
        
        // ICR - Read internal conversion rate
        if (this->SendCommand("ICR?;", 3000) == NO_ERRORS)
        {
    	    app->LastDigLCSet[this->LoadCellNum].ICR = atoi((char *)&this->rxmsg[0]);
            RtPrintf("ICR %d\n", app->LastDigLCSet[this->LoadCellNum].ICR); // Response: 04
        }
        
        // CWT - Read calibration weight
        if (this->SendCommand("CWT?;", 3000) == NO_ERRORS)
        {

	        pSegment = strtok((char*)&this->rxmsg[0],",");
	        if (pSegment != NULL)
	        {
                app->LastDigLCSet[this->LoadCellNum].CWT = atoi(pSegment);
	        }
            RtPrintf("CWT %d\n", app->LastDigLCSet[this->LoadCellNum].CWT); // Response: 1000000, 1000000
        }

        // LDW - Read load cell dead load weight
        if (this->SendCommand("LDW?;", 3000) == NO_ERRORS)
        {
    	    app->LastDigLCSet[this->LoadCellNum].LDW = atoi((char *)&this->rxmsg[0]);
            RtPrintf("LDW %d\n", app->LastDigLCSet[this->LoadCellNum].LDW); // Response: 0000000
        }
        
        // LWT - Read load cell live weight
        if (this->SendCommand("LWT?;", 3000) == NO_ERRORS)
        {
    	    app->LastDigLCSet[this->LoadCellNum].LWT = atoi((char *)&this->rxmsg[0]);
            RtPrintf("LWT %d\n", app->LastDigLCSet[this->LoadCellNum].LWT); // Response: 1000000
        }
        
        // NOV - Read load cell nominal value
        if (this->SendCommand("NOV?;", 3000) == NO_ERRORS)
        {
    	    app->LastDigLCSet[this->LoadCellNum].NOV = atoi((char *)&this->rxmsg[0]);
            RtPrintf("NOV %d\n", app->LastDigLCSet[this->LoadCellNum].NOV); // Response: 0000000
        }
        
        // RSN - Read load cell resolution
        if (this->SendCommand("RSN?;", 3000) == NO_ERRORS)
        {
    	    app->LastDigLCSet[this->LoadCellNum].RSN = atoi((char *)&this->rxmsg[0]);
            RtPrintf("RSN %d\n", app->LastDigLCSet[this->LoadCellNum].RSN); // Response: 001
        }
        
        // MTD - Read load cell motion detection setting
        if (this->SendCommand("MTD?;", 3000) == NO_ERRORS)
        {
    	    app->LastDigLCSet[this->LoadCellNum].MTD = atoi((char *)&this->rxmsg[0]);
            RtPrintf("MTD %d\n", app->LastDigLCSet[this->LoadCellNum].MTD); // Response: 00
        }
        
        // Read load cell linearization coefficients
        if (this->SendCommand("LIC?;", 3000) == NO_ERRORS) // Response: 0000000, 1000000, 0000000, 0000000
        {
	        pSegment = strtok((char*)&this->rxmsg[0],",");
	        if (pSegment != NULL)
	        {
                app->LastDigLCSet[this->LoadCellNum].LIC0 = atoi(pSegment);
            	pSegment = strtok(NULL,",");
                if (pSegment != NULL)
                    app->LastDigLCSet[this->LoadCellNum].LIC1 = atoi(pSegment);
            	pSegment = strtok(NULL,",");
                if (pSegment != NULL)
                    app->LastDigLCSet[this->LoadCellNum].LIC2 = atoi(pSegment);
            	pSegment = strtok(NULL,",");
                if (pSegment != NULL)
                    app->LastDigLCSet[this->LoadCellNum].LIC3 = atoi(pSegment);
	        }
            RtPrintf("LIC %d, %d, %d, %d\n", 
                app->LastDigLCSet[this->LoadCellNum].LIC0,
                app->LastDigLCSet[this->LoadCellNum].LIC1,
                app->LastDigLCSet[this->LoadCellNum].LIC2,
                app->LastDigLCSet[this->LoadCellNum].LIC3); // Response: 0000000, 1000000, 0000000, 0000000
        }
        
        // ZTR - Read load cell zero tracking setting
        if (this->SendCommand("ZTR?;", 3000) == NO_ERRORS)
        {
	        pSegment = strtok((char*)&this->rxmsg[0],",");
	        if (pSegment != NULL)
	        {
                app->LastDigLCSet[this->LoadCellNum].ZTR = atoi(pSegment);
	        }
            RtPrintf("ZTR %d\n", app->LastDigLCSet[this->LoadCellNum].ZTR); // Response: 0
        }
        
        // ZSE - Read load cell zero setting
        if (this->SendCommand("ZSE?;", 3000) == NO_ERRORS)
        {
    	    app->LastDigLCSet[this->LoadCellNum].ZSE = atoi((char *)&this->rxmsg[0]);
            RtPrintf("ZSE %d\n", app->LastDigLCSet[this->LoadCellNum].ZSE); // Response: 00
        }

        // TRC - Read load cell trigger settings
        if (this->SendCommand("TRC?;", 3000) == NO_ERRORS)
        {
	        pSegment = strtok((char*)&this->rxmsg[0],",");
	        if (pSegment != NULL)
	        {
                app->LastDigLCSet[this->LoadCellNum].TRC1 = atoi(pSegment);
            	pSegment = strtok(NULL,",");
                if (pSegment != NULL)
                    app->LastDigLCSet[this->LoadCellNum].TRC2 = atoi(pSegment);
            	pSegment = strtok(NULL,",");
                if (pSegment != NULL)
                    app->LastDigLCSet[this->LoadCellNum].TRC3 = atoi(pSegment);
            	pSegment = strtok(NULL,",");
                if (pSegment != NULL)
                    app->LastDigLCSet[this->LoadCellNum].TRC4 = atoi(pSegment);
            	pSegment = strtok(NULL,",");
                if (pSegment != NULL)
                    app->LastDigLCSet[this->LoadCellNum].TRC5 = atoi(pSegment);
	        }
            RtPrintf("TRC %d, %d, %d, %d, %d\n", 
                app->LastDigLCSet[this->LoadCellNum].TRC1,
                app->LastDigLCSet[this->LoadCellNum].TRC2,
                app->LastDigLCSet[this->LoadCellNum].TRC3,
                app->LastDigLCSet[this->LoadCellNum].TRC4,
                app->LastDigLCSet[this->LoadCellNum].TRC5); // Response: 0,0, 0000000,00,00
        }

		RtPrintf("------------------------------\n");
    }
       
    // Each time through, change any settings required

    // ASF
    if (app->pShm->scl_set.DigLCSet[this->LoadCellNum].ASF >= 2)
	{
		this->HBMChangeIntSetting("ASF", &app->LastDigLCSet[this->LoadCellNum].ASF,
							&app->pShm->scl_set.DigLCSet[this->LoadCellNum].ASF, 
							0, false, true, 3000);
	}
	else
	{
		sprintf(app_err_buf, "Attempted to change ASF from %d to %d\n", 
			app->LastDigLCSet[this->LoadCellNum].ASF, 
			app->pShm->scl_set.DigLCSet[this->LoadCellNum].ASF); //GLC added 10/19/04
		app->GenError(informational, app_err_buf); //GLC added 10/19/04
		
		if (app->LastDigLCSet[this->LoadCellNum].ASF > 2)
		{
			// Set shared memory to last setting since frontend is trying to write an invalid value
			app->pShm->scl_set.DigLCSet[this->LoadCellNum].ASF = app->LastDigLCSet[this->LoadCellNum].ASF;
		}
		else
		{
			// Set shared memory to default since frontend is trying to write an invalid value
			app->pShm->scl_set.DigLCSet[this->LoadCellNum].ASF = app->DefaultDigLCSet[this->LoadCellNum].ASF;
		}
	}

    // FMD
    this->HBMChangeIntSetting("FMD", &app->LastDigLCSet[this->LoadCellNum].FMD,
						&app->DefaultDigLCSet[this->LoadCellNum].FMD,                
						//&app->pShm->scl_set.DigLCSet[this->LoadCellNum].FMD, 
                        0, false, true, 3000);
	
    // ICR
    this->HBMChangeIntSetting("ICR", &app->LastDigLCSet[this->LoadCellNum].ICR,
                        &app->DefaultDigLCSet[this->LoadCellNum].ICR, 
                        //&app->pShm->scl_set.DigLCSet[this->LoadCellNum].ICR, 
                        0, false, true, 3000);

    // CWT
    this->HBMChangeIntSetting("CWT", &app->LastDigLCSet[this->LoadCellNum].CWT,
                        &app->DefaultDigLCSet[this->LoadCellNum].CWT, 
                        //&app->pShm->scl_set.DigLCSet[this->LoadCellNum].CWT, 
                        1, true, true, 3000);
	
    // LDW, LWT
    this->HBMCalibrateLoadCell();

    // NOV
    this->HBMChangeIntSetting("NOV", &app->LastDigLCSet[this->LoadCellNum].NOV,
                        &app->DefaultDigLCSet[this->LoadCellNum].NOV, 
                        //&app->pShm->scl_set.DigLCSet[this->LoadCellNum].NOV, 
                        0, false, true, 3000);

    // RSN
    this->HBMChangeIntSetting("RSN", &app->LastDigLCSet[this->LoadCellNum].RSN,
                        &app->DefaultDigLCSet[this->LoadCellNum].RSN, 
                        //&app->pShm->scl_set.DigLCSet[this->LoadCellNum].RSN, 
                        0, false, false, 3000);

    // MTD
    this->HBMChangeIntSetting("MTD", &app->LastDigLCSet[this->LoadCellNum].MTD,
                        &app->DefaultDigLCSet[this->LoadCellNum].MTD, 
                        //&app->pShm->scl_set.DigLCSet[this->LoadCellNum].MTD, 
                        0, false, true, 3000);

    // LIC0
    this->HBMChangeIntSetting("LIC0,", &app->LastDigLCSet[this->LoadCellNum].LIC0,
                        &app->DefaultDigLCSet[this->LoadCellNum].LIC0, 
                        //&app->pShm->scl_set.DigLCSet[this->LoadCellNum].LIC0, 
                        1, true, true, 3000);

    // LIC1
    this->HBMChangeIntSetting("LIC1,", &app->LastDigLCSet[this->LoadCellNum].LIC1,
                        &app->DefaultDigLCSet[this->LoadCellNum].LIC1, 
                        //&app->pShm->scl_set.DigLCSet[this->LoadCellNum].LIC1, 
                        2, true, true, 3000);

    // LIC2
    this->HBMChangeIntSetting("LIC2,", &app->LastDigLCSet[this->LoadCellNum].LIC2,
                        &app->DefaultDigLCSet[this->LoadCellNum].LIC2, 
                        //&app->pShm->scl_set.DigLCSet[this->LoadCellNum].LIC2, 
                        3, true, true, 3000);

    // LIC3
    this->HBMChangeIntSetting("LIC3,", &app->LastDigLCSet[this->LoadCellNum].LIC3,
                        &app->DefaultDigLCSet[this->LoadCellNum].LIC3, 
                        //&app->pShm->scl_set.DigLCSet[this->LoadCellNum].LIC3, 
                        4, true, true, 3000);
    
    // ZTR
    this->HBMChangeIntSetting("ZTR", &app->LastDigLCSet[this->LoadCellNum].ZTR,
                        &app->DefaultDigLCSet[this->LoadCellNum].ZTR, 
                        //&app->pShm->scl_set.DigLCSet[this->LoadCellNum].ZTR, 
                        0, false, true, 3000);

    // ZSE
    this->HBMChangeIntSetting("ZSE", &app->LastDigLCSet[this->LoadCellNum].ZSE,
                        &app->DefaultDigLCSet[this->LoadCellNum].ZSE, 
                        //&app->pShm->scl_set.DigLCSet[this->LoadCellNum].ZSE, 
                        0, false, true, 3000);

    // TRC
    this->HBMSetTrigger();


    // Restart continuous output if stopped before
    if (this->blnOutputStopped)
    {
        this->StartContinuousOutput();
    }
	
	return (0);

}


//---------------------------------------------------------------------------
// HBMLoadCell::HBMChangeIntSetting - Check for a change in an integer
//                           load cell setting, send change and verify.
//---------------------------------------------------------------------------
int HBMLoadCell::HBMChangeIntSetting(char * cmd, int * oldSetting, 
                                     int * newSetting, int item,
                                     bool passwordRequired, 
                                     bool saveToNVMem, DWORD response_time)
{
    char command[40];
	char commandQuery[40];
    int i;
    
    // Check the shared memory for a change in setting
	if (!(*newSetting == *oldSetting))
    {
        // Make sure continuous measurements are stopped
        if (this->ContMeasOut)
        {
            this->StopContinuousOutput();
        }
        
        // Debug trace
        //DebugTrace(_HBMLDCELL_, "LC %d %s changing from %d to %d\n", 
        //    this->LoadCellNum + 1, cmd, *oldSetting, *newSetting);
		sprintf(app_err_buf, "LC %d %s changing from %d to %d\n", 
			this->LoadCellNum + 1, cmd, *oldSetting, *newSetting); //GLC added 10/19/04
		app->GenError(logonly, app_err_buf); //GLC added 10/19/04

        // Send password if required
	    if (passwordRequired)
        {
            this->SendCommand("SPW\"AED\";", 3000);	
        }

        // Construct the command
        i = sprintf(command, "%s %d;", cmd, *newSetting);
        
        // Send the command on serial port
        this->SendCommand((char *)&command, response_time);

        // Construct the query
        if (strncmp("LIC", cmd, 3) == 0)
        {
            i = sprintf(commandQuery, "LIC?;");
        }
        else
        {
            i = sprintf(commandQuery, "%s?;", cmd);
        }

        // Send the query and check the return value
        if (!(this->CheckConfigIntValue(commandQuery, *newSetting, item, response_time)))
        {
	        //RtPrintf("LC %d %s value not set correctly. file %s, line %d\n", 
			//	this->LoadCellNum + 1, cmd, _FILE_, __LINE__);
	        sprintf(app_err_buf,"LC %d %s value not set correctly. file %s, line %d\n", 
				this->LoadCellNum + 1, cmd, _FILE_, __LINE__);
	        app->GenError(warning, app_err_buf);
            *newSetting = *oldSetting;
        }
        else 
        {
            // Save to NVRAM if required
            if (saveToNVMem)
            {
                this->SendCommand("TDD1;", 3000);	
            }
            
            // Display debug trace
            //DebugTrace(_HBMLDCELL_, "LC %d %s changed from %d to %d\n", 
			//	this->LoadCellNum + 1, cmd, *oldSetting, *newSetting);
			sprintf(app_err_buf, "LC %d %s changed from %d to %d\n", 
				this->LoadCellNum + 1, cmd, *oldSetting, *newSetting); //GLC added 10/19/04
			app->GenError(logonly, app_err_buf); //GLC added 10/19/04

            // Reassign the new value to the temp variable
            *oldSetting = *newSetting;
        }

    }

    return (0);

}

//-------------------------------------------------------------------------------------
// HBMLoadCell::HBMCalibrateLoadCell()
// LDW/LWT - these calibration settings are a special case that must be sent to the load 
// cell as a command sequence in the order of LDW, LWT.
//-------------------------------------------------------------------------------------
int HBMLoadCell::HBMCalibrateLoadCell()
{
	char fltCmd[40];
    int i;

    // Check to see if any calibration settings have changed
    //if ((app->pShm->scl_set.DigLCSet[this->LoadCellNum].LDW != app->LastDigLCSet[this->LoadCellNum].LDW)
    //    || (app->pShm->scl_set.DigLCSet[this->LoadCellNum].LWT != app->LastDigLCSet[this->LoadCellNum].LWT))
    if ((app->DefaultDigLCSet[this->LoadCellNum].LDW != app->LastDigLCSet[this->LoadCellNum].LDW)
        || (app->DefaultDigLCSet[this->LoadCellNum].LWT != app->LastDigLCSet[this->LoadCellNum].LWT))
    {
        // Stop continuous output if active
        if (this->ContMeasOut)
        {
            this->StopContinuousOutput();
        }

        // Debug traces
        //DebugTrace(_HBMLDCELL_, "LDW changing from %d to %d\n", 
        //    app->LastDigLCSet[this->LoadCellNum].LDW, 
        //    app->DefaultDigLCSet[this->LoadCellNum].LDW);
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].LDW);
		sprintf(app_err_buf, "LC %d LDW changing from %d to %d\n", 
			this->LoadCellNum + 1,
			app->LastDigLCSet[this->LoadCellNum].LDW, 
			app->DefaultDigLCSet[this->LoadCellNum].LDW); //GLC added 10/25/04
		app->GenError(logonly, app_err_buf); //GLC added 10/25/04

        //DebugTrace(_HBMLDCELL_, "LWT changing from %d to %d\n", 
        //    app->LastDigLCSet[this->LoadCellNum].LWT, 
        //    app->DefaultDigLCSet[this->LoadCellNum].LWT);
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].LWT);
		sprintf(app_err_buf, "LC %d LWT changing from %d to %d\n", 
			this->LoadCellNum + 1,
			app->LastDigLCSet[this->LoadCellNum].LWT, 
			app->DefaultDigLCSet[this->LoadCellNum].LWT); //GLC added 10/25/04
		app->GenError(logonly, app_err_buf); //GLC added 10/25/04

	    // Send password before setting LDW parameter
        if (this->SendCommand("SPW\"AED\";", 3000) != NO_ERRORS)
        {
	        //RtPrintf("Load cell %d password check not completed in file %s, line %d\n", 
			//	this->LoadCellNum + 1, _FILE_, __LINE__);
	        sprintf(app_err_buf,"Load cell %d password check not completed in file %s, line %d\n", 
				this->LoadCellNum + 1, _FILE_, __LINE__);
	        app->GenError(warning, app_err_buf);
        }   
        else
        {
        
            // Construct the command
            //i = sprintf(fltCmd, "LDW %d;", app->pShm->scl_set.DigLCSet[this->LoadCellNum].LDW);
            i = sprintf(fltCmd, "LDW %d;", app->DefaultDigLCSet[this->LoadCellNum].LDW);

            // Send the command and check the return value for 0
            if (!(this->CheckConfigIntValue(fltCmd, 0, 0, 4000)))
            {
	            //RtPrintf("LC %d LDW value not set correctly. file %s, line %d\n", 
				//	this->LoadCellNum + 1, _FILE_, __LINE__);
	            sprintf(app_err_buf,"LC %d LDW value not set correctly. file %s, line %d\n", 
					this->LoadCellNum + 1, _FILE_, __LINE__);
	            app->GenError(warning, app_err_buf);
            }
            else
            {
                // Send the LWT setting to complete the calibration process

                // Construct the command
                //i = sprintf(fltCmd, "LWT %d;", app->pShm->scl_set.DigLCSet[this->LoadCellNum].LWT);
                i = sprintf(fltCmd, "LWT %d;", app->DefaultDigLCSet[this->LoadCellNum].LWT);

                // Send the command and check the return value for 0
                if (!(this->CheckConfigIntValue(fltCmd, 0, 0, 4000)))
                {
	                //RtPrintf("LC %d LWT value not set correctly. file %s, line %d\n", 
					//	this->LoadCellNum + 1, _FILE_, __LINE__);
	                sprintf(app_err_buf,"LC %d LWT value not set correctly. file %s, line %d\n",
						this->LoadCellNum + 1, _FILE_, __LINE__);
	                app->GenError(warning, app_err_buf);
                }
                else
                {
   
                    // Verify LDW changes
                    //if (!(this->CheckConfigIntValue("LDW?;", app->pShm->scl_set.DigLCSet[this->LoadCellNum].LDW, 0, 3000)))
                    if (!(this->CheckConfigIntValue("LDW?;", app->DefaultDigLCSet[this->LoadCellNum].LDW, 0, 3000)))
                    {
	                    //RtPrintf("LC %d LDW value not set correctly. file %s, line %d\n",
						//	this->LoadCellNum + 1, _FILE_, __LINE__);
	                    sprintf(app_err_buf,"LC %d LDW value not set correctly. file %s, line %d\n",
							this->LoadCellNum + 1, _FILE_, __LINE__);
	                    app->GenError(warning, app_err_buf);
                    }
                    else
                    {
                        // Debug trace
                        //DebugTrace(_HBMLDCELL_, "LDW changed from %d to %d\n",
                        //    app->LastDigLCSet[this->LoadCellNum].LDW, 
                        //    //app->pShm->scl_set.DigLCSet[this->LoadCellNum].LDW);
                        //    app->DefaultDigLCSet[this->LoadCellNum].LDW);
						sprintf(app_err_buf, "LC %d LDW changed from %d to %d\n", 
							this->LoadCellNum + 1,
							app->LastDigLCSet[this->LoadCellNum].LDW, 
							app->DefaultDigLCSet[this->LoadCellNum].LDW); //GLC added 10/25/04
						app->GenError(logonly, app_err_buf); //GLC added 10/25/04

                        // Reassign the new value to the temp variable
                        //app->LastDigLCSet[this->LoadCellNum].LDW = app->pShm->scl_set.DigLCSet[this->LoadCellNum].LDW;
                        app->LastDigLCSet[this->LoadCellNum].LDW = app->DefaultDigLCSet[this->LoadCellNum].LDW;
                    }

                    // Verify LWT changes
                    //if (!(this->CheckConfigIntValue("LWT?;", app->pShm->scl_set.DigLCSet[this->LoadCellNum].LWT, 0, 3000)))
                    if (!(this->CheckConfigIntValue("LWT?;", app->DefaultDigLCSet[this->LoadCellNum].LWT, 0, 3000)))
                    {
	                    //RtPrintf("LC %d LWT value not set correctly. file %s, line %d\n",
						//	this->LoadCellNum + 1, _FILE_, __LINE__);
	                    sprintf(app_err_buf,"LC %d LWT value not set correctly. file %s, line %d\n",
							this->LoadCellNum + 1, _FILE_, __LINE__);
	                    app->GenError(warning, app_err_buf);
                    }
                    else
                    {
                        // Debug trace
                        //DebugTrace(_HBMLDCELL_, "LWT changed from %d to %d\n",
                        //    app->LastDigLCSet[this->LoadCellNum].LWT, 
                        //    //app->pShm->scl_set.DigLCSet[this->LoadCellNum].LWT);
                        //    app->DefaultDigLCSet[this->LoadCellNum].LWT);
						sprintf(app_err_buf, "LC %d LWT changed from %d to %d\n", 
							this->LoadCellNum + 1,
							app->LastDigLCSet[this->LoadCellNum].LWT, 
							app->DefaultDigLCSet[this->LoadCellNum].LWT); //GLC added 10/25/04
						app->GenError(logonly, app_err_buf); //GLC added 10/25/04

                        // Reassign the new value to the temp variable
                        //app->LastDigLCSet[this->LoadCellNum].LWT = app->pShm->scl_set.DigLCSet[this->LoadCellNum].LWT;
                        app->LastDigLCSet[this->LoadCellNum].LWT = app->DefaultDigLCSet[this->LoadCellNum].LWT;
                    }
                }
            }
        }
    }
    return (0);
}

//-------------------------------------------------------------------------------------
// HBMLoadCell::HBMSetTrigger()
// Check shared memory for trigger setting changes and apply them to the load cell
//-------------------------------------------------------------------------------------
int HBMLoadCell::HBMSetTrigger()
{
	char fltCmd[50];
    int i;
   	char*	pSegment;
    
    // Check to see if any trigger settings have changed
    if ((app->DefaultDigLCSet[this->LoadCellNum].TRC1 != app->LastDigLCSet[this->LoadCellNum].TRC1) ||
        (app->DefaultDigLCSet[this->LoadCellNum].TRC2 != app->LastDigLCSet[this->LoadCellNum].TRC2) ||
        (app->DefaultDigLCSet[this->LoadCellNum].TRC3 != app->LastDigLCSet[this->LoadCellNum].TRC3) ||
        (app->DefaultDigLCSet[this->LoadCellNum].TRC4 != app->LastDigLCSet[this->LoadCellNum].TRC4) ||
        (app->DefaultDigLCSet[this->LoadCellNum].TRC5 != app->LastDigLCSet[this->LoadCellNum].TRC5))
    //if ((app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC1 != app->LastDigLCSet[this->LoadCellNum].TRC1) ||
    //    (app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC2 != app->LastDigLCSet[this->LoadCellNum].TRC2) ||
    //    (app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC3 != app->LastDigLCSet[this->LoadCellNum].TRC3) ||
    //    (app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC4 != app->LastDigLCSet[this->LoadCellNum].TRC4) ||
    //    (app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC5 != app->LastDigLCSet[this->LoadCellNum].TRC5))
    {
        // Stop continuous output if active
        if (this->ContMeasOut)
        {
            this->StopContinuousOutput();
        }

        // If setting has changed, send the new value
        /*
		DebugTrace(_HBMLDCELL_, "TRC changing from %d, %d, %d, %d, %d to %d, %d, %d, %d, %d\n", 
            app->LastDigLCSet[this->LoadCellNum].TRC1, 
            app->LastDigLCSet[this->LoadCellNum].TRC2, 
            app->LastDigLCSet[this->LoadCellNum].TRC3, 
            app->LastDigLCSet[this->LoadCellNum].TRC4, 
            app->LastDigLCSet[this->LoadCellNum].TRC5, 
            app->DefaultDigLCSet[this->LoadCellNum].TRC1,
            app->DefaultDigLCSet[this->LoadCellNum].TRC2,
            app->DefaultDigLCSet[this->LoadCellNum].TRC3,
            app->DefaultDigLCSet[this->LoadCellNum].TRC4,
            app->DefaultDigLCSet[this->LoadCellNum].TRC5);
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC1,
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC2,
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC3,
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC4,
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC5);
		*/
		sprintf(app_err_buf, "LC %d TRC changing from %d, %d, %d, %d, %d to %d, %d, %d, %d, %d\n", 
            this->LoadCellNum + 1,
			app->LastDigLCSet[this->LoadCellNum].TRC1, 
            app->LastDigLCSet[this->LoadCellNum].TRC2, 
            app->LastDigLCSet[this->LoadCellNum].TRC3, 
            app->LastDigLCSet[this->LoadCellNum].TRC4, 
            app->LastDigLCSet[this->LoadCellNum].TRC5, 
            app->DefaultDigLCSet[this->LoadCellNum].TRC1,
            app->DefaultDigLCSet[this->LoadCellNum].TRC2,
            app->DefaultDigLCSet[this->LoadCellNum].TRC3,
            app->DefaultDigLCSet[this->LoadCellNum].TRC4,
            app->DefaultDigLCSet[this->LoadCellNum].TRC5);
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC1,
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC2,
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC3,
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC4,
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC5);
		app->GenError(logonly, app_err_buf); //GLC added 10/25/04

        // Construct the command
        i = sprintf(fltCmd, "TRC %d,%d,%d,%d,%d;", 
            app->DefaultDigLCSet[this->LoadCellNum].TRC1,
            app->DefaultDigLCSet[this->LoadCellNum].TRC2,
            app->DefaultDigLCSet[this->LoadCellNum].TRC3,
            app->DefaultDigLCSet[this->LoadCellNum].TRC4,
            app->DefaultDigLCSet[this->LoadCellNum].TRC5);
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC1,
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC2,
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC3,
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC4,
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC5);

        // Send the command and check the return value for 0
        if (!(this->CheckConfigIntValue(fltCmd, 0, 0, 4000)))
        {
	        //RtPrintf("LC %d TRC not set correctly. file %s, line %d\n", 
			//	this->LoadCellNum + 1, _FILE_, __LINE__);
	        sprintf(app_err_buf,"LC %d TRC not set correctly. file %s, line %d\n", 
				this->LoadCellNum + 1, _FILE_, __LINE__);
	        app->GenError(warning, app_err_buf);
        }

        // Construct expected response
        sprintf(fltCmd, "%.1d,%.1d, %.7d,%.2d,%.2d\r\n",
            app->DefaultDigLCSet[this->LoadCellNum].TRC1,
            app->DefaultDigLCSet[this->LoadCellNum].TRC2,
            app->DefaultDigLCSet[this->LoadCellNum].TRC3,
            app->DefaultDigLCSet[this->LoadCellNum].TRC4,
            app->DefaultDigLCSet[this->LoadCellNum].TRC5);
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC1,
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC2,
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC3,
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC4,
            //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC5);
        
        // Verify changes - "0,0, 0000000,00,00"
        if (!(this->CheckConfigValue("TRC?;", fltCmd, 1000)))
        {
	        //RtPrintf("LC %d TRC not set correctly. file %s, line %d\n",
			//	this->LoadCellNum + 1, _FILE_, __LINE__);
	        sprintf(app_err_buf,"LC %d TRC not set correctly. file %s, line %d\n",
				this->LoadCellNum + 1, _FILE_, __LINE__);
	        app->GenError(warning, app_err_buf);

            pSegment = strtok((char*)&this->rxmsg[0],",");
	        if (pSegment != NULL)
	        {
                app->LastDigLCSet[this->LoadCellNum].TRC1 = atoi(pSegment);
            	pSegment = strtok(NULL,",");
                if (pSegment != NULL)
                    app->LastDigLCSet[this->LoadCellNum].TRC2 = atoi(pSegment);
            	pSegment = strtok(NULL,",");
                if (pSegment != NULL)
                    app->LastDigLCSet[this->LoadCellNum].TRC3 = atoi(pSegment);
            	pSegment = strtok(NULL,",");
                if (pSegment != NULL)
                    app->LastDigLCSet[this->LoadCellNum].TRC4 = atoi(pSegment);
            	pSegment = strtok(NULL,",");
                if (pSegment != NULL)
                    app->LastDigLCSet[this->LoadCellNum].TRC5 = atoi(pSegment);

                // Reassign the old value to the shm variable
                //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC1 = app->LastDigLCSet[this->LoadCellNum].TRC1;
                //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC2 = app->LastDigLCSet[this->LoadCellNum].TRC2;
                //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC3 = app->LastDigLCSet[this->LoadCellNum].TRC3;
                //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC4 = app->LastDigLCSet[this->LoadCellNum].TRC4;
                //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC5 = app->LastDigLCSet[this->LoadCellNum].TRC5;
            }

        
        }
        else
        {
            /*
			DebugTrace(_HBMLDCELL_, "TRC changed from %d, %d, %d, %d, %d to %d, %d, %d, %d, %d\n", 
                app->LastDigLCSet[this->LoadCellNum].TRC1, 
                app->LastDigLCSet[this->LoadCellNum].TRC2, 
                app->LastDigLCSet[this->LoadCellNum].TRC3, 
                app->LastDigLCSet[this->LoadCellNum].TRC4, 
                app->LastDigLCSet[this->LoadCellNum].TRC5, 
                app->DefaultDigLCSet[this->LoadCellNum].TRC1,
                app->DefaultDigLCSet[this->LoadCellNum].TRC2,
                app->DefaultDigLCSet[this->LoadCellNum].TRC3,
                app->DefaultDigLCSet[this->LoadCellNum].TRC4,
                app->DefaultDigLCSet[this->LoadCellNum].TRC5);
                //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC1,
                //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC2,
                //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC3,
                //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC4,
                //app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC5);
			*/
			sprintf(app_err_buf, "LC %d TRC changed from %d, %d, %d, %d, %d to %d, %d, %d, %d, %d\n", 
				this->LoadCellNum + 1,
				app->LastDigLCSet[this->LoadCellNum].TRC1, 
				app->LastDigLCSet[this->LoadCellNum].TRC2, 
				app->LastDigLCSet[this->LoadCellNum].TRC3, 
				app->LastDigLCSet[this->LoadCellNum].TRC4, 
				app->LastDigLCSet[this->LoadCellNum].TRC5, 
				app->DefaultDigLCSet[this->LoadCellNum].TRC1,
				app->DefaultDigLCSet[this->LoadCellNum].TRC2,
				app->DefaultDigLCSet[this->LoadCellNum].TRC3,
				app->DefaultDigLCSet[this->LoadCellNum].TRC4,
				app->DefaultDigLCSet[this->LoadCellNum].TRC5);
				//app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC1,
				//app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC2,
				//app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC3,
				//app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC4,
				//app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC5);
			app->GenError(logonly, app_err_buf); //GLC added 10/25/04

            // Reassign the new value to the temp variable
            app->LastDigLCSet[this->LoadCellNum].TRC1 = app->DefaultDigLCSet[this->LoadCellNum].TRC1;
            app->LastDigLCSet[this->LoadCellNum].TRC2 = app->DefaultDigLCSet[this->LoadCellNum].TRC2;
            app->LastDigLCSet[this->LoadCellNum].TRC3 = app->DefaultDigLCSet[this->LoadCellNum].TRC3;
            app->LastDigLCSet[this->LoadCellNum].TRC4 = app->DefaultDigLCSet[this->LoadCellNum].TRC4;
            app->LastDigLCSet[this->LoadCellNum].TRC5 = app->DefaultDigLCSet[this->LoadCellNum].TRC5;
            //app->LastDigLCSet[this->LoadCellNum].TRC1 = app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC1;
            //app->LastDigLCSet[this->LoadCellNum].TRC2 = app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC2;
            //app->LastDigLCSet[this->LoadCellNum].TRC3 = app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC3;
            //app->LastDigLCSet[this->LoadCellNum].TRC4 = app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC4;
            //app->LastDigLCSet[this->LoadCellNum].TRC5 = app->pShm->scl_set.DigLCSet[this->LoadCellNum].TRC5;

            // Store to NVRAM
            this->SendCommand("TDD1;", 3000);	
        }
    }

    return (0);

}

//-------------------------------------------------------------------------------------
// HBMLoadCell::CheckConfigIntValue()
// Send an integer setting and verify the change
//-------------------------------------------------------------------------------------
bool HBMLoadCell::CheckConfigIntValue(char * cmd, int value, int item, DWORD response_time)
{
	bool    rc      =   false;
    int     retVal  =   0;
    int     i;
   	char*	pSegment;
	char    RetStr[HBM_BUFFER_SIZE];

    // Send Debug Command - TG
	RtPrintf("Setting Loadcell Command = %s\n", cmd);

	if (this->SendCommand(cmd, response_time) == NO_ERRORS)
	{
		strcpy((char *)&RetStr[0], (char *)&this->rxmsg[0]);
        DebugTrace(_HBMLDCELL_,"CheckConfigIntValue::value->%d\n", value);
		if (item == 0)
        {
            retVal = atoi((char *)&RetStr[0]);
        }
        else
        {
            for (i=0; i<item; i++)
            {
	            if (i==0)
                {
                    pSegment = strtok((char*)&RetStr[0],",");
                }
                else
                {
                    pSegment = strtok(NULL,",");
                }
	            
                if (pSegment != NULL)
                {
                    retVal = atoi(pSegment);
                    //RtPrintf("pSegment = %s, retVal = %d\n", pSegment, retVal);
                }
	        }
        }

        if(retVal == value)
		{
			rc = true;
			DebugTrace(_HBMLDCELL_,"CheckConfigIntValue:return true received->%s\n", (char *)&this->rxmsg[0]);
		}
		else
		{
			rc = false;
			DebugTrace(_HBMLDCELL_,"CheckConfigIntValue:return false received->%s\n", (char *)&this->rxmsg[0]);
			RtPrintf("CheckConfigIntValue:return false received->%s\n", (char *)&this->rxmsg[0]);
		}
	}

	return rc;


}
