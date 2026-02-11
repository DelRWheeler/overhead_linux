/*
 * types.h - Linux port of Interface types.h
 *
 * Replaces the Windows-centric types.h with Linux/POSIX equivalents.
 * All application types and structures remain identical.
 */

#pragma once

//---------------------------------------------------
// Platform abstraction (provides Windows types + POSIX wrappers)
//---------------------------------------------------
#include "../common/platform.h"

//---------------------------------------------------
// Application headers (unchanged from original)
//---------------------------------------------------
#include "../common/overheadconst.h"
#include "../common/overheadtypes.h"
#include "../common/ipc.h"
#include "interface.h"
