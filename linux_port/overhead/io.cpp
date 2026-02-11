// io.cpp: Linux stub implementation of the io base class.
//
//////////////////////////////////////////////////////////////////////

#include "types.h"

#undef  _FILE_
#define _FILE_  "io.cpp"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

io::io()
{
}

io::~io()
{
}

/************************************************************************
** Initialize the I/O board
*************************************************************************/

bool io::initialize()
{
    RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
    return true;
}

//--------------------------------------------------------
// readInputs: Routine to Read inputs and set Sync,Zero
//             and switch_in bits
//--------------------------------------------------------

void io::readInputs(void)
{
    RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
}

//--------------------------------------------------------
// writeOutputs: Routine to Write outputs and set Sync,Zero
//               and switch_in bits
//--------------------------------------------------------

void io::writeOutputs(void)
{
    RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
}

//--------------------------------------------------------
// writeOutput: Routine to Write single output byte
//--------------------------------------------------------

void io::writeOutput(int byte)
{
	(void)byte;
    RtPrintf("Error file %s, line %d \n",_FILE_, __LINE__);
}
