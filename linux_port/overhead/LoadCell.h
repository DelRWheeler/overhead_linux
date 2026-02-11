// LoadCell.h: interface for the LoadCell class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_LOADCELL_H__E36381B3_B10D_11D6_B740_0020E067B7E4__INCLUDED_)
#define AFX_LOADCELL_H__E36381B3_B10D_11D6_B740_0020E067B7E4__INCLUDED_

#pragma once

class LoadCell
{
private:
	virtual void    KillTime(int iTime);
	int     num_adc_reads[1];

public:
	LoadCell();
	virtual ~LoadCell();
	virtual int     read_adc_num_reads(int channel);
	virtual int     init_adc(int mode, int num_reads);
	virtual void    write_adc(int channel,unsigned char outbyte);
	virtual __int64 read_load_cell(int channel);
	virtual void    set_adc_offset(int channel,int offset);
	virtual void    set_adc_num_reads(int channel, int num_reads);
	virtual void    set_adc_mode(int channel, int mode);
	virtual void    read_adc_version(void);
	virtual void    read_adc_pn(void);
	virtual int     read_adc_mode(int channel);
	virtual char    read_adc(int channel);

    int NumCH;

};

#endif // !defined(AFX_LOADCELL_H__E36381B3_B10D_11D6_B740_0020E067B7E4__INCLUDED_)
