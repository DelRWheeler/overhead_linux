#pragma once
//////////////////////////////////////////////////////////////////////
// types.h - Linux port of Overhead types.h
//
// Replaces Windows.h, rtapi.h with platform.h
// Replaces direct serial/I/O includes with Linux stubs
//////////////////////////////////////////////////////////////////////

//---------------------------------------------------
// useful defines (from original types.h)
//---------------------------------------------------

#ifndef  TRUE
#define  TRUE  1
#define  FALSE 0
#endif

#define  ON    1
#define  OFF   0

// Build configuration (defines _HBM_SIM_, etc.)
// MUST come before platform.h so preprocessor conditionals work.
#include "../common/overheadconst.h"

// Platform compatibility layer (replaces Windows.h + rtapi.h)
#include "../common/platform.h"

// Console output (shared with Interface)
#include "../interface/console.h"

// Serial port support (Linux version)
#include "serial.h"
#include "serialtypes.h"
#include "overheadmacros.h"
#include "../common/overheadtypes.h"

// Hardware abstraction classes
#include "LoadCell.h"
#include "1510Loadcell.h"
#include "HBMLoadCell.h"
#include "ISPDLoadCell.h"

// Application class
#include "overhead.h"

// Utilities
#include "utils.h"

// I/O card abstraction
#include "io.h"
#include "3724_io.h"
#include "3724_io_2.h"
#include "3724_io_3.h"
#include "3724_io_4.h"
#include "Vsbc6_io.h"

// Subsystems
#include "InterSystems.h"
#include "DropManager.h"
#include "Simulator.h"

// IPC
#include "../common/ipc.h"

// External variables
#include "overheadext.h"
