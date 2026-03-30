//////////////////////////////////////////////////////////////////////
// ISPDLoadCell.h: interface for the ISPDLoadCell class.
//
// H&B (Hauch & Bach) ISPD-20KG digital load cell amplifier driver.
// Model D:1520, firmware V:0146.
//
// Protocol: ASCII commands terminated with CR (0x0D)
//   - 2-letter commands: ID, IV, IS, GS, GW, FL, FM, UR, SZ, ST, WP, SX
//   - Responses terminated with CR
//   - Continuous output via SX command: "S±NNNNNN\r" per reading
//   - Default 115200 baud, no checksum
//   - Counts per pound: 13340 (measured from real hardware)
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "types.h"

// Measurement queue length - same as HBM for compatibility
#define ISPD_MAX_ADC_READS       ADCREADSMAX
#define ISPD_MEASQ_MAX_ITEMS     ISPD_MAX_ADC_READS

#define ISPD_NUM_CH              1
#define ISPD_LOADCELL_1          0
#define ISPD_LOADCELL_2          1
#define ISPD_LOADCELL_3          2
#define ISPD_LOADCELL_4          3

#define ISPD_BUFFER_SIZE         RING_BUFFER_SIZE
#define ISPD_MAX_MEAS_SAMPLES    5000

// ASCII line buffer for incoming measurement lines
// ISPD measurement format: "S+015641\r" = max ~12 chars per reading
#define ISPD_LINE_BUFFER_SIZE    64

// ISPD default baud rate
#define ISPD_DEFAULT_BAUD_RATE   COM_BAUD_RATE_115200

class ISPDLoadCell: public LoadCell
{
public:

	// Measurement queue item - same structure as HBM for compatibility
	typedef struct
	{
		__int64		meas;
		bool		state;
	} MeasQ_Item;

	// Initialization data structure - same pattern as HBM
	typedef struct
	{
		Serial*			pSerial;
		ISPDLoadCell *	pISPDLoadCell;
		HANDLE *		phSerMtx;
	} ISPDinit;

	ISPDLoadCell();
	virtual ~ISPDLoadCell();
    int     NumCH;
	int		LoadCellNum;

	// Public methods - same interface as HBMLoadCell for drop-in compatibility
    int     read_adc_num_reads(int channel);
    int     init_adc(int mode, int num_adc_reads);
    void    write_adc(int channel, unsigned char outbyte);
    __int64 read_load_cell(int channel);
    void    set_adc_offset(int channel, int offset);
    void    set_adc_num_reads(int channel, int reads);
    void    set_adc_mode(int channel, int mode);
    void    read_adc_version(void);
    void    read_adc_pn(void);
    int     read_adc_mode(int channel);
    char    read_adc(int channel);
	void	initialize(ISPDinit* serial);
	__int64	SampleLCReadQ[ISPD_MAX_MEAS_SAMPLES];
	int		WriteLCReadsQ();
	bool	FirstTimeThru;

	HANDLE		hISPDLc;
	HANDLE		hSerMtx;
	HANDLE		hMeasQ;
    HANDLE		hSerResponse;

	Serial*		serialObj;
	ISPDinit*	ISPDInitializeData;

	// Load cell threads - one per physical load cell
	static int	RTFCNDCL ISPDLoadCellThread1(PVOID pObj);
	static int	RTFCNDCL ISPDLoadCellThread2(PVOID pObj);
	static int	RTFCNDCL ISPDLoadCellThread3(PVOID pObj);
	static int	RTFCNDCL ISPDLoadCellThread4(PVOID pObj);

	// Private methods for ISPD communication
    void		KillTime(int iTime);
    int			CreateISPDLoadCellThread(ISPDinit* serial);
	int			FlushComInBuffer(void);
	int			StartContinuousOutput();
	int			StopContinuousOutput();
	void		SerialRead(Serial* serial);
	int			SerialWrite(char* txdata);
	int			SendCommand(char* cmd, DWORD response_time);
	bool		CheckConfigValue(char* cmd, char* value, DWORD response_time);
	int			MeasureQ(__int64* measArr, int items);
	int			SampleLCReadMeasureQ(__int64* measArr);

	// Parse an ASCII measurement line from ISPD
	// Returns the parsed ADC count value, or 0 on parse error
	// Handles formats: "S+015641", "G+001.234", "N+001.234"
	__int64		ParseMeasurementLine(char* line, int len);

	// LoadCell settings
    int			num_adc_reads[ISPD_NUM_CH];
    UCHAR		left_adc_pn[ADCINFOMAX];
    UCHAR		left_adc_version[ADCINFOMAX];
    int			adc_mode[ISPD_NUM_CH];
    int			adc_offset[ISPD_NUM_CH];

    // Arrays for error reporting
	char		lc_err_buf[MAXERRMBUFSIZE];
    bool		error_sent[ISPD_NUM_CH];

    // Rx message buffer (for command responses)
	BYTE		rxmsg[ISPD_BUFFER_SIZE];

	// ASCII line accumulation buffer (for continuous measurement parsing)
	char		lineBuf[ISPD_LINE_BUFFER_SIZE];
	int			lineIdx;

    // Continuous measurement output state
	bool		ContMeasOut;
	bool		blnOutputStopped;
    bool		blnMeas;
    bool		blnCmdSent;
    bool		blnResponseReceived;
	bool		bFirstCheck;

	// Simple shared value - bypasses MeasureQ for debugging
	volatile __int64	latestMeas;

	// Measurement queue control
	MeasQ_Item		MeasQ[ISPD_MEASQ_MAX_ITEMS];
	MeasQ_Item*		pInMeasQ;
	MeasQ_Item*		pOutMeasQ;
	bool			bMeasQWrapped;
	int				InvalidMeasureCnt;
	int				MeasQ_Idx;
	int				LCReadsCaptured;
	int				LCReadCounter;
	bool			StartLCReads;
	int				CurrentRead;

	// Debug counters
	long		cPrintCnt;
	__int64		ParseErrorCount;
};
