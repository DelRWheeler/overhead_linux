/*******************************************************************************/
/* KpyM Telnet Server                                                          */
/* Copyright (C) 2002-2004  Kroum Grigorov                                     */
/* e-mail:  kpym@yahoo.com                                                     */
/* web:     http://kpym.sourceforge.net                                        */
/*                                                                             */
/*---doxygen follows-----------------------------------------------------------*/
//! \file telnetsrv.h
//! \brief Common struct declarations and #defines
//! <pre>
//!
//! version  changes
//! -------  -----------------------------------------------------------------
//! 1.07     o telnet_opt
//!
//! 1.06     o term_io keyinsertmode
//!
//! 1.05     o clientwidth clientheight defines added
//!
//! 1.04     o minor changes
//!
//! 1.03     o screen buffers are CHAR_INFO now cause of the colours
//!          o USECOLOURS define added
//!
//! 1.02     o i have moved the functoins in this file to other files
//!          o minor changes
//!
//! 1.01     o welcome message changed
//!          o GetFileParam  changed
//!          o idle timeout added
//!          o MAXSPAWNCMD deleted
//!
//! 1.00     o initial release
//! </pre>
/*******************************************************************************/
#pragma once
#include "types.h"

#define OK							1			//!< Returned on success
#define ERR							0			//!< Returned on error

#define MAXTERMIN					1024		//!< Max term input
#define MAXTERMOUT					1024		//!< Max term out


#define MAXTELNETD					3					//!< Max telnet daemon
#define PORT						23					//!< Default port

#define NOFREEID		"max connections reached\r\ntry again later\r\n"	//!< No free id message

#define LOGIN			"\nlogin:  "					//!< Login message
#define PASS			"\npassword:   "				//!< Pass message
#define LOGGEDIN		"\n\n"							//!< Logged in message
#define PASSTIMEOUT		"\n\npassword timeout \n\n"		//!< Pass timeout message
#define IDLETIMEOUT		"\n\nidle timeout \n\n"			//!< Idle timeout message
#define REFRESHDELAY	100							//!< Refresh delay

#define SERVICE_CONTROL_RESTART		128				//!< Restart service control message

/*******************************************************************************/
/* structure for shared memory                                                 */
/*---doxygen follows-----------------------------------------------------------*/
//! \brief Shared memory structure
/*******************************************************************************/
typedef struct SHAREDMEMORY{
	void *pSharedMemory;	//!< shared memory pointer
	int iSize;              //!< shared memory size
	char acName[256];       //!< mapped file name
	HANDLE hMapping;        //!< handle to the mapped file
}SHAREDMEMORY;/* SHAREDMEMORY */

/*******************************************************************************/
/* communication structure btw telnetd.exe and term.exe                        */
/*---doxygen follows-----------------------------------------------------------*/
//! \brief Communication structure btw telnetd.exe and term.exe
/*******************************************************************************/
typedef struct TERM_IO{
	char acIn[MAXTERMIN]; 					//!< Telnet input
	int bCanChangeIn;						//!< Can change input flag
	char acOut[MAXTERMOUT];					//!< Telnet output
	int bCanChangeOut;						//!< Can change output flag
	int bIsActive;							//!< Is term active flag
	//char acTermState[MAXTERMSTATE];			//!< ?? not used
}TERM_IO;/*TERM_IO*/

/*******************************************************************************/
/* telnetd inner structure                                                     */
/*---doxygen follows-----------------------------------------------------------*/
//! \brief Telnetd inner structure
/*******************************************************************************/
typedef struct TELNETD{
	SOCKET sClientSock;						//!< Client socket
	TERM_IO stioTerm;						//!< Communication structure btw telnetd.exe and term.exe
	SHAREDMEMORY ssmMShare;					//!< Shared memory structure
	int	bIsUsed;							//!< Is used flag
	//PROCESS_INFORMATION piTerm;				//!< Terminal process information
	int	iID;								//!< Id
}TELNETD;/*TELNETD*/


/*******************************************************************************/
/* telnet protocol options state                                               */
/*---doxygen follows-----------------------------------------------------------*/
//! \brief Telnet options structure
/*******************************************************************************/
// Undefine ECHO from termios.h to avoid conflict with struct member
#ifdef ECHO
#undef ECHO
#endif
typedef struct TELNET_OPT{
	int ECHO;
	int SGO;
	int NAWS;
}TELNET_OPT;
