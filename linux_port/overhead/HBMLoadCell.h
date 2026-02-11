// HBMLoadCell.h: interface for the HBMLoadCell class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "types.h"

// Measurement queue length
#define HBM_MAX_ADC_READS       ADCREADSMAX
#define	MEASQ_MAX_ITEMS 		HBM_MAX_ADC_READS

#define HBM_NUM_CH              1
#define	LOADCELL_1				0
#define	LOADCELL_2				1
#define	LOADCELL_3				2
#define	LOADCELL_4				3


//#define RX_BUFFER_SIZE  RING_BUFFER_SIZE
#define HBM_BUFFER_SIZE RING_BUFFER_SIZE

#define MAX_MEAS_SAMPLES	5000

// Declare COF (Output Format) constants
const int COF_MODE_BINARY = 0;
const int COF_MODE_ASCII = 1;
const int COF_EOC_NO_CRLF = 0;
const int COF_EOC_CRLF = 1;
const int COF_BYTEORDER_MSB_LSB = 0;
const int COF_BYTEORDER_LSB_MSB = 1;
const int COF_STATUS_NONE = 0;
const int COF_STATUS_LSB = 1;
const int COF_STATUS_PRM3 = 2;
const int COF_ADDR_NONE = 0;
const int COF_ADDR_PRM2 = 1;

class HBMLoadCell: public LoadCell
{
public:

	//Define MeasQ_Item type
	typedef struct
	{
		__int64		meas;
		bool		state;
	} MeasQ_Item;

	//Define the Output Format type
	typedef struct
	{
		int Mode;		// 0=Binary, 1=ASCII
		int Bytes;		// For binary mode - 2 or 4
		int ByteOrder;	// 0=MSB->LSB, 1=LSB->MSB
		int Status;		// 0=Off, 1=On
		int EOC;		// End of command 0=No CRLF, 1=Append CRLF
		int Addr;		// Address display for ASCII mode
	} Output_Format;

	typedef struct
	{
		Serial*			pSerial;
		HBMLoadCell *	pHBMLoadCell;
		HANDLE *		phSerMtx;
	} HBMinit;

	HBMLoadCell();
	virtual ~HBMLoadCell();
    int     NumCH;
	int		LoadCellNum;

	// Public methods that are common between Loadcell types
    int     read_adc_num_reads(int channel);
    int     init_adc(int mode, int num_adc_reads);
    void    write_adc(int channel,unsigned char outbyte);
    __int64 read_load_cell(int channel);
    void    set_adc_offset(int channel,int offset);
    void    set_adc_num_reads(int channel, int reads);
    void    set_adc_mode(int channel, int mode);
    void    read_adc_version(void);
    void    read_adc_pn(void);
    int     read_adc_mode(int channel);
    char    read_adc(int channel);
	void	initialize(HBMinit* serial);
	__int64	SampleLCReadQ[MAX_MEAS_SAMPLES];
	int		WriteLCReadsQ();
	bool	FirstTimeThru;

private:

	HANDLE		hHBMLc;
	HANDLE		hSerMtx;
	HANDLE		hMeasQ;
    HANDLE		hSerResponse;

	Serial*		serialObj;
	HBMinit*	HBMInitializeData;

	// Private Loadcell threads that handles configuration
	// and reading loadcell serial output
	static int	RTFCNDCL HBMLoadCellThread1(PVOID pObj);
	static int	RTFCNDCL HBMLoadCellThread2(PVOID pObj);
	static int	RTFCNDCL HBMLoadCellThread3(PVOID pObj);
	static int	RTFCNDCL HBMLoadCellThread4(PVOID pObj);

	// Private methods unique to the digitial HBM loadcell.
 	int			SetOutputFormat(int COF);
    void		KillTime(int iTime);
    bool		FindInIntArray(int tgt, int * arr, int intArrSize);
    int			CreateHBMLoadCellThread(  HBMinit* serial );
	int			Meas_Mode_From_COF(int COF, Output_Format * mmode);
	int			FlushComInBuffer(void);
	bool		CheckConfigIntValue(char * cmd, int value, int item, DWORD response_time);
	int			HBMSetTrigger();
	int			HBMCalibrateLoadCell();
	int			HBMChangeIntSetting(
						char * cmd, int * oldSetting,
						int * newSetting, int item,
						bool passwordRequired,
						bool saveToNVMem,
						DWORD response_time
						);
	int			HBMCheckSettings();
    int			StartContinuousOutput();
	int			StopContinuousOutput();
	void		SerialRead(Serial* serial);
	int			SerialWrite(char * txdata);
	int			SendCommand(char * cmd, DWORD response_time);
	bool		CheckConfigValue(char * cmd, char * value, DWORD response_time);
	int			MeasureQ(__int64 * measArr, int items);
	int			SampleLCReadMeasureQ(__int64 * measArr);

	// TG Added Loopback Test Function
	int			PerformLoopbackTest();

	// LoadCell settings
    int			num_adc_reads[HBM_NUM_CH];
    UCHAR		left_adc_pn[ADCINFOMAX];
    UCHAR		left_adc_version[ADCINFOMAX];
    int			adc_mode[HBM_NUM_CH];
    int			adc_offset[HBM_NUM_CH];

    // Arrays for error reporting
	char		lc_err_buf[MAXERRMBUFSIZE];
    bool		error_sent[HBM_NUM_CH];

    // Rx message char array
	BYTE    rxmsg[HBM_BUFFER_SIZE];

    // Set continuous meas output flag, meas mode
	bool ContMeasOut;
	bool blnOutputStopped;
    bool blnMeas;
    bool blnCmdSent;
    bool blnResponseReceived;
	bool bFirstCheck;
	Output_Format meas_mode;

	// Measurement queue control
	MeasQ_Item		MeasQ[MEASQ_MAX_ITEMS];
	MeasQ_Item*		pInMeasQ;
	MeasQ_Item*		pOutMeasQ;
	bool			bMeasQWrapped;
	int				InvalidMeasureCnt;
	int				MeasQ_Idx;
	int				LCReadsCaptured;
	int				LCReadCounter;
	bool			StartLCReads;
	int				CurrentRead;

	// Used for debug of serial performance
	long	cPrintCnt;
	__int64 CheckSumError;
	__int64 ExtraReadCnt;
};
