// Vsbc6_io.cpp: Linux stub implementation of the io_vsbc6 class.
//
//////////////////////////////////////////////////////////////////////

#include "types.h"

#undef  _FILE_
#define _FILE_  "Vsbc6_io.cpp"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

io_vsbc6::io_vsbc6()
{
}

io_vsbc6::~io_vsbc6()
{
}

/************************************************************************
** Initialize the I/O board
*************************************************************************/

bool io_vsbc6::initialize()
{
    // Stub - no VSBC6 hardware on Linux
    return true;
}

//--------------------------------------------------------
// readInputs: Routine to Read inputs
//--------------------------------------------------------

void io_vsbc6::readInputs(void)
{
    // Stub - no hardware to read from on Linux
}
