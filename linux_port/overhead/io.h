// io.h: interface for the io class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_IO_H__E36381B6_B10D_11D6_B740_0020E067B7E4__INCLUDED_)
#define AFX_IO_H__E36381B6_B10D_11D6_B740_0020E067B7E4__INCLUDED_

#pragma once

class io
{
public:
	io();
	virtual ~io();

    virtual bool initialize();
    virtual void readInputs(void);
	virtual void writeOutputs(void);
	virtual void writeOutput(int byte);
};

#endif // !defined(AFX_IO_H__E36381B6_B10D_11D6_B740_0020E067B7E4__INCLUDED_)
