//////////////////////////////////////////////////////////////////////
// SPDLoadCell.h: interface for the SPDLoadCell class.
//
// Group Four Transducers SPD (LDB151/LDB152) load cell driver.
// Replacement for HBM FIT7 - uses ASCII serial protocol instead of
// HBM's binary 4-byte packet format.
//
// SPD Manual Reference: "SPD USER MANUAL Rev 08"
// Key differences from HBM FIT7:
//   - ASCII output format (e.g., "S+0125785\r\n") vs binary 4-byte packets
//   - 2-letter commands with CR/LF terminator vs semicolon-terminated
//   - Default 115200 baud vs 38400
//   - No checksum on serial output
//   - Continuous ADC sample output via "SX" command (vs "MSV?0;")
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "types.h"

// Measurement queue length - same as HBM for compatibility
#define SPD_MAX_ADC_READS       ADCREADSMAX
#define SPD_MEASQ_MAX_ITEMS     SPD_MAX_ADC_READS

#define SPD_NUM_CH              1
#define SPD_LOADCELL_1          0
#define SPD_LOADCELL_2          1
#define SPD_LOADCELL_3          2
#define SPD_LOADCELL_4          3

#define SPD_BUFFER_SIZE         RING_BUFFER_SIZE
#define SPD_MAX_MEAS_SAMPLES    5000

// ASCII line buffer for incoming measurement lines
// SPD measurement format: "S+0125785\r\n" = max ~14 chars per reading
#define SPD_LINE_BUFFER_SIZE    64

// SPD default baud rate
#define SPD_DEFAULT_BAUD_RATE   COM_BAUD_RATE_115200

class SPDLoadCell: public LoadCell
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
		SPDLoadCell *	pSPDLoadCell;
		HANDLE *		phSerMtx;
	} SPDinit;

	SPDLoadCell();
	virtual ~SPDLoadCell();
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
	void	initialize(SPDinit* serial);
	__int64	SampleLCReadQ[SPD_MAX_MEAS_SAMPLES];
	int		WriteLCReadsQ();
	bool	FirstTimeThru;

private:

	HANDLE		hSPDLc;
	HANDLE		hSerMtx;
	HANDLE		hMeasQ;
    HANDLE		hSerResponse;

	Serial*		serialObj;
	SPDinit*	SPDInitializeData;

	// Load cell threads - one per physical load cell
	static int	RTFCNDCL SPDLoadCellThread1(PVOID pObj);
	static int	RTFCNDCL SPDLoadCellThread2(PVOID pObj);
	static int	RTFCNDCL SPDLoadCellThread3(PVOID pObj);
	static int	RTFCNDCL SPDLoadCellThread4(PVOID pObj);

	// Private methods for SPD communication
    void		KillTime(int iTime);
    int			CreateSPDLoadCellThread(SPDinit* serial);
	int			FlushComInBuffer(void);
	int			StartContinuousOutput();
	int			StopContinuousOutput();
	void		SerialRead(Serial* serial);
	int			SerialWrite(char* txdata);
	int			SendCommand(char* cmd, DWORD response_time);
	bool		CheckConfigValue(char* cmd, char* value, DWORD response_time);
	int			MeasureQ(__int64* measArr, int items);
	int			SampleLCReadMeasureQ(__int64* measArr);

	// Parse an ASCII measurement line from SPD
	// Returns the parsed ADC count value, or 0 on parse error
	// Handles formats: "S+0125785", "G+001.234", "N+001.234"
	__int64		ParseMeasurementLine(char* line, int len);

	// LoadCell settings
    int			num_adc_reads[SPD_NUM_CH];
    UCHAR		left_adc_pn[ADCINFOMAX];
    UCHAR		left_adc_version[ADCINFOMAX];
    int			adc_mode[SPD_NUM_CH];
    int			adc_offset[SPD_NUM_CH];

    // Arrays for error reporting
	char		lc_err_buf[MAXERRMBUFSIZE];
    bool		error_sent[SPD_NUM_CH];

    // Rx message buffer (for command responses)
	BYTE		rxmsg[SPD_BUFFER_SIZE];

	// ASCII line accumulation buffer (for continuous measurement parsing)
	char		lineBuf[SPD_LINE_BUFFER_SIZE];
	int			lineIdx;

    // Continuous measurement output state
	bool		ContMeasOut;
	bool		blnOutputStopped;
    bool		blnMeas;
    bool		blnCmdSent;
    bool		blnResponseReceived;
	bool		bFirstCheck;

	// Measurement queue control
	MeasQ_Item		MeasQ[SPD_MEASQ_MAX_ITEMS];
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
