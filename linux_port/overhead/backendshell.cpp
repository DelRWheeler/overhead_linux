/*******************************************************************************
01234567890123456789012345678901234567890123456789012345678901234567890123456789
00000000001111111111222222222233333333334444444444555555555566666666667777777777
*******************************************************************************/
#undef  _FILE_
#define _FILE_      "backendshell.c"

/*******************************************************************************/
/* MODULE DESCRIPTION                                                          */
/*******************************************************************************/
/*
** This module is the command shell used to accept input from a telnet session,
** parse the input, and output a response. Shared memory between the interface
** and backend is used to relay the input received through the telnet connection
** that is managed by the interface process.
**
*/


/*******************************************************************************/
/* INCLUDES                                                                    */
/*******************************************************************************/

#include "types.h"
#include "backendshell.h"
#include "telnetsrv.h"


/*******************************************************************************/
/* EXTERNS                                                                     */
/*******************************************************************************/

extern TELNETD atdTelnetd[];

/*******************************************************************************/
/* GLOBAL VARIABLES                                                            */
/*******************************************************************************/
int TakeLoadCellReadsFlag;
int	ReadsToSample;


/*******************************************************************************/
/* LOCAL FUNCTION PROTOTYPES                                                   */
/*******************************************************************************/

static int ShowMemory(int Argc, char** ppArgv);

static int SetTrace(int Argc, char** ppArgv);

static int ReadLoadCell(int Argc, char** ppArgv);

static int CmdHelp(int Argc, char** ppArgv);

static tCmdEntry* FindCmd (char *	pCmd);

static int RunCmd (tCmdEntry* pCmdEntry, char* pBuf, int Len);

int ParseCmd (char*	pBuf, int Len);

void TermInput (char* pInput);

/*******************************************************************************/
/* LOCAL VARIABLES                                                            */
/*******************************************************************************/

static BYTE CmdIn[_CMDLINE_SIZE];
static UINT InIdx;

static const tCmdEntry MainCmdTable[] =
{
	{"showmem"	,		NULL,		ShowMemory,			"Show Memory"			},
	{"settrace"	,		NULL,		SetTrace,			"Setup Tracing"			},
	{"readloadcell",	NULL,		ReadLoadCell,		"Take N loadcell reads"	},
	{"help"	,			NULL,		CmdHelp,			"List Commands"			},
	{NULL		,		NULL,		NULL,				""						}
};

static const tCmdEntry* pCmdLists = &MainCmdTable[0];

const char pPrompt[] = "debug> ";

/*******************************************************************************/
/* CODE START                                                                  */
/*******************************************************************************/

/*******************************************************************************/
/*                                                                             */
/* ShowMemory - Shell command to dump shared memory to telnet console          */
/*                                                                             */
/*******************************************************************************/
static int ShowMemory(

	int Argc,
	char** ppArgv)
{
    int   k;
    int*  pDbg_int   = 0;
    __int64* pDbg_real  = 0;
    char* pDbg_char;
    int   dbg_id     = 0;
    int   dbg_idx    = 0;

    if (Argc != 3)
        {
			SendToClient("\r\nUsage: ShowMem <Id> {1-89} <Index> {0-#}.\r\n");
			return(-1);
        }
    else
        {
			dbg_id = atol(ppArgv[1]);
			dbg_idx = atol(ppArgv[2]);
			if((dbg_id < 1) || (dbg_id > 89))
			{
				SendToClient("\r\nDebug bad Id %d\r\n", dbg_id);
				return(-1);
			}
        }

	SendToClient("\r\nLooking up Id = %d Index = %d\r\n", dbg_id, dbg_idx);

	//Index into array begins at zero
	dbg_id--;

    if (dbg_idx >= app->shm_tbl[dbg_id].num_members)
	{
        SendToClient("\r\nDebug bad index %d\r\n", dbg_idx);
		return(-1);
	}
    else
    {
		if (dbg_idx >= app->shm_tbl[dbg_id].num_members)
		{
			SendToClient("\tDebug bad index %d\r\n", dbg_idx);
		}
		else
		{
			switch (app->shm_tbl[dbg_id].data_type)
	        {
				 case _DBOOL:
	             case _char:

	                 pDbg_char = (char*) app->shm_tbl[dbg_id].data;
	                 pDbg_char += dbg_idx;

	                 if (isalnum(*pDbg_char))
	                 {
	                     SendToClient("\t%1c\r\n", *pDbg_char );
	                 }
	                 else
	                 {
	                     SendToClient("\t%1x\r\n", *pDbg_char);
	                 }

	                 break;

	             case _uint:

	                 pDbg_int = (int*) app->shm_tbl[dbg_id].data;
	                 pDbg_int += dbg_idx;
	                 SendToClient("\t%u\r\n", *pDbg_int);
	                 break;

	             case _int:

	                 pDbg_int = (int*) app->shm_tbl[dbg_id].data;
	                 pDbg_int += dbg_idx;
	                 SendToClient("\t%d\r\n", *pDbg_int);
	                 break;

	             case ___int64:

	                 pDbg_real = (__int64*) app->shm_tbl[dbg_id].data;
	                 pDbg_real += dbg_idx;
	                 SendToClient("\t%d\r\n", (int) *pDbg_real);
	                 break;

	             case _TShackleTares:

	                     SendToClient("index\t%d\tGrdIdx\t%d\ttare[0]\t%d\ttare[1]\t%d\r\n",
	                             dbg_idx, app->pShm->ShackleStatus[dbg_idx].GradeIndex[0],
	                             dbg_idx, app->pShm->ShackleStatus[dbg_idx].GradeIndex[1],
	                             app->pShm->ShackleTares[dbg_idx].tare[0],
	                             app->pShm->ShackleTares[dbg_idx].tare[1]);
	                 break;

	             case _TSyncSettings:

	                     SendToClient("index\t%d\tfirst\t%d\tlast\t%d\r\n", dbg_idx,
	                                   app->pShm->sys_set.SyncSettings[dbg_idx].first,
	                                   app->pShm->sys_set.SyncSettings[dbg_idx].last);
	                 break;

	             case _TScheduleSettings:

	                     SendToClient("index\t%d\tMinWt\t%d\tMaxWt\t%d\tNxtEnt\t%d\tGrade\t%4.4s\tExgrd\t%4.4s\r\n", dbg_idx,
	                          (int)app->pShm->Schedule[dbg_idx].MinWeight,
	                          (int)app->pShm->Schedule[dbg_idx].MaxWeight,
	                          (int)app->pShm->Schedule[dbg_idx].NextEntry,
	                          (char*)&app->pShm->Schedule[dbg_idx].Grade[0],
	                          (char*)&app->pShm->Schedule[dbg_idx].Extgrade[0]);

	                     SendToClient("ExMnWt\t%d\tExMxWt\t%d\tEx2MnWt\t%d\tEx2MxWt\t%d\tShrkg\t%u\r\n",
	                          (int)app->pShm->Schedule[dbg_idx].ExtMinWeight,
	                          (int)app->pShm->Schedule[dbg_idx].ExtMaxWeight,
	                          (int)app->pShm->Schedule[dbg_idx].Ext2MinWeight,
	                          (int)app->pShm->Schedule[dbg_idx].Ext2MaxWeight,
	                          app->pShm->Schedule[dbg_idx].Shrinkage);

	                     SendToClient("BchCnt\t%u\tBchWtSp\t%l\tBPMSp\t%u\tDistMd\t%d\tBPMmode\t%u\tLpOrd\t%d\r\n",
	                          app->pShm->Schedule[dbg_idx].BchCntSp,
	                          (int) app->pShm->Schedule[dbg_idx].BchWtSp,
	                          app->pShm->Schedule[dbg_idx].BPMSetpoint,
	                          app->pShm->Schedule[dbg_idx].DistMode,
	                          app->pShm->Schedule[dbg_idx].BpmMode,
	                          app->pShm->Schedule[dbg_idx].LoopOrder);

	                 break;

	             case _TOrder:

	                     SendToClient("index\t%d\t%d\r\n", dbg_idx, app->pShm->sys_set.Order[dbg_idx]);
	                 break;

	             case _TDropSettings:

	                     SendToClient("index\t%d\tAct\t%1x\tSync\t%u\tOffst\t%d\tExtgrd\t%1x\r\n", dbg_idx,
	                         app->pShm->sys_set.DropSettings[dbg_idx].Active,
	                         app->pShm->sys_set.DropSettings[dbg_idx].Sync,
	                         app->pShm->sys_set.DropSettings[dbg_idx].Offset,
	                         app->pShm->sys_set.DropSettings[dbg_idx].Extgrade);
	                 break;

	             case _TGradeSettings:

	                     SendToClient("index\t%d\tgrade\t%1c\toffset\t%d\tgrdsync\t%d\r\n", dbg_idx,
	                         app->pShm->sys_set.GradeArea[dbg_idx].grade,
	                         app->pShm->sys_set.GradeArea[dbg_idx].offset,
						 app->pShm->sys_set.GradeArea[dbg_idx].GradeSyncUsed);

	                 break;

	             case _TIsysDropSettings:

	                     SendToClient("index\t%d\r\n", dbg_idx);

	                     for (k = 0; k < MAXIISYSLINES; k++)
	                     {
	                         SendToClient("active[%d]\t%d\tdrop\t%d\r\n", k,
	                             app->pShm->sys_set.IsysDropSettings[dbg_idx].active[k] & 0x1,
	                             app->pShm->sys_set.IsysDropSettings[dbg_idx].drop[k]);
	                     }
	                 break;

	             case _TDebugRec:

	                     SendToClient("DbgRec\tid_count\t%u\r\n",
	                         app->pShm->dbg_set.debug_id.id_count);

	                     for (k = 0; k < MAXDBGIDS; k++)
	                     {
	                         SendToClient("dbgid[%d]\t%u\tdbgindex\t%u\r\n", k,
	                                 app->pShm->dbg_set.debug_id.ids[k].id,
	                                 app->pShm->dbg_set.debug_id.ids[k].index);
	                     }
	                 break;

	             case _TIsysLineSettings:

	                     SendToClient("\tiena\t%d\timst\t%d\tlocid\t%d",
	                              app->pShm->sys_set.IsysLineSettings.isys_enable,
	                              app->pShm->sys_set.IsysLineSettings.isys_master,
	                              app->this_lineid);

	                     {
	                         bool path_valid;

	                         for (k = 0; k <= MAXIISYSLINES; k++)
	                         {
	                             if ( (app->pShm->sys_set.IsysLineSettings.pipe_path[k][0] == 0x5c) &&
	                                  (app->pShm->sys_set.IsysLineSettings.pipe_path[k][1] == 0x5c) )
	                                  path_valid = true;
	                             else
	                                  path_valid = false;

	                             if (k < MAXIISYSLINES)
	                             {
	                                 SendToClient("\r\nRemId[%d]\t%u\tactive\t%1.1x\tpath\t%s\r\n",k,
	                                         app->pShm->sys_set.IsysLineSettings.line_ids[k],
	                                         app->pShm->sys_set.IsysLineSettings.active[k] & 0x1,
	                                         (path_valid ? app->pShm->sys_set.IsysLineSettings.pipe_path[k] : "No Path") );
	                             }
	                             else
	                             {
	                                 SendToClient("\r\n\t\t\t\thost\tpath\t%s\r\n",
	                                         (path_valid ? app->pShm->sys_set.IsysLineSettings.pipe_path[k] : "No Path") );
	                             }
	                         }
	                     }

	                 break;

	             case _TDropStats:

	                     SendToClient("index\t%d\tBWeight\t%u\tBCount\t%u\r\n", dbg_idx,
	                          (int) app->pShm->sys_stat.MissedDropInfo[dbg_idx].BWeight,
	                          (int) app->pShm->sys_stat.MissedDropInfo[dbg_idx].BCount);

	                     for (k = 0; k < MAXGRADES; k++)
	                     {
	                         SendToClient("\t\tGrd\t%c\tGrdWt\t%u\tGrdCnt\t%u\r\n",
	                                    app->pShm->sys_set.GradeArea[k].grade,
	                              (int) app->pShm->sys_stat.MissedDropInfo[dbg_idx].GrdWeight[k],
	                              (int) app->pShm->sys_stat.MissedDropInfo[dbg_idx].GrdCount[k]);
	                     }

	                 break;

	             case _TDropStats1:

	                     SendToClient("index\t%d\tBWeight\t%u\tBCount\t%u\r\n", dbg_idx,
	                          (int) app->pShm->sys_stat.DropInfo[dbg_idx].BWeight,
	                          (int) app->pShm->sys_stat.DropInfo[dbg_idx].BCount);

	                     for (k = 0; k <  MAXGRADES; k++)
	                     {
	                         SendToClient("\t\tGrd\t%c\tGrdWt\t%u\tGrdCnt\t%u\r\n",
	                                        app->pShm->sys_set.GradeArea[k].grade,
	                                  (int) app->pShm->sys_stat.DropInfo[dbg_idx].GrdWeight[k],
	                                  (int) app->pShm->sys_stat.DropInfo[dbg_idx].GrdCount[k]);
	                     }
	                     break;

	             case _THostDropRec:

	                     SendToClient("index\t%d\tflags\t%x\tshkl\t%d\tgrade\t%1.1x\tdrpd[0]\t%d\tweight\t%d\tdrop\t%d\r\n", dbg_idx,
	                         app->pShm->HostDropRec[dbg_idx].flags,
	                         app->pShm->HostDropRec[dbg_idx].shackle,
	                         app->pShm->HostDropRec[dbg_idx].grade[0],
	                         app->pShm->HostDropRec[dbg_idx].dropped[0],
	                         app->pShm->HostDropRec[dbg_idx].weight[0],
	                         app->pShm->HostDropRec[dbg_idx].drop[0]);


						 RtPrintf(" THostDropRec: Grade[0] = %c\n",app->pShm->HostDropRec[dbg_idx].grade[0]);


	                     SendToClient("\t\t\t\t\t\t\t\tgrade\t%1.1x\tdrpd[1]\t%d\tweight\t%d\tdrop\t%d\r\n",
	                         app->pShm->HostDropRec[dbg_idx].grade[1],
	                         app->pShm->HostDropRec[dbg_idx].dropped[1],
	                         app->pShm->HostDropRec[dbg_idx].weight[1],
	                         app->pShm->HostDropRec[dbg_idx].drop[1]);

						 RtPrintf(" THostDropRec: Grade[1] = %c\n",app->pShm->HostDropRec[dbg_idx].grade[1]);
	                 break;

	             case _TIsysDropStatus:

	                     SendToClient("index\t%d\t\r\n", dbg_idx);

	                     for (k = 0; k < MAXIISYSLINES; k++)
	                     {
	                         SendToClient("flags[%d]\t%1.1x\tPPMCnt\t%d\tPPMPrev\t%d\tBCnt\t%u\tBWt\t%u\r\n", k,
	                                 app->pShm->IsysDropStatus[dbg_idx].flags[k],
	                                 app->pShm->IsysDropStatus[dbg_idx].PPMCount[k],
	                                 app->pShm->IsysDropStatus[dbg_idx].PPMPrevious[k],
	                                 (unsigned int) app->pShm->IsysDropStatus[dbg_idx].BCount[k],
	                                 (unsigned int) app->pShm->IsysDropStatus[dbg_idx].BWeight[k]);
	                     }
	                  break;

	             case _TScheduleStatus:

	                     SendToClient("index\t%d\t\r\n", dbg_idx);

	                     SendToClient("BaActCt\t%u\tBaActWt\t%u\tPPMCnt\t%u\tSpare\t%u\tBPMPrv\t%u\t\r\n",
	                          app->pShm->sys_stat.ScheduleStatus[dbg_idx].BchActCnt,
	                          (int) app->pShm->sys_stat.ScheduleStatus[dbg_idx].BchActWt,
	                          app->pShm->sys_stat.ScheduleStatus[dbg_idx].PPMCount,
	                          app->pShm->sys_stat.ScheduleStatus[dbg_idx].spare,
	                          app->pShm->sys_stat.ScheduleStatus[dbg_idx].PPMPrevious);

	                     SendToClient("TklCnt\t%u\tBPM1Qok\t%u\tBPM2Qok\t%u\tBPM3Qok\t%u\tBPM4Qok\t%u\r\n",
	                          app->pShm->sys_stat.ScheduleStatus[dbg_idx].BPMTrickleCount,
	                          app->pShm->sys_stat.ScheduleStatus[dbg_idx].BPM_1stQ_ok,
	                          app->pShm->sys_stat.ScheduleStatus[dbg_idx].BPM_2ndQ_ok,
	                          app->pShm->sys_stat.ScheduleStatus[dbg_idx].BPM_3rdQ_ok,
	                          app->pShm->sys_stat.ScheduleStatus[dbg_idx].BPM_4thQ_ok);
	                 break;

	             case _TSyncStatus:

	                     SendToClient("index\t%d\tshkl\t%u\tzero'd\t%1x\r\n", dbg_idx,
	                         app->pShm->SyncStatus[dbg_idx].shackleno,
	                         app->pShm->SyncStatus[dbg_idx].zeroed);
	                 break;

	             case _TDropStatus:

	                     SendToClient("index\t%d\tBatch\t%u\tSusp\t%1x\tBCnt\t%u\tBWt\t%u\r\n", dbg_idx,
	                         app->pShm->sys_stat.DropStatus[dbg_idx].Batched,
	                         app->pShm->sys_stat.DropStatus[dbg_idx].Suspended,
	                         (int) app->pShm->sys_stat.DropStatus[dbg_idx].BCount,
	                         (int) app->pShm->sys_stat.DropStatus[dbg_idx].BWeight);
	                 break;

	             case _TDisplayShackle:

	                     SendToClient("grdidx[0]\t%u\tdrop[0]\t%d\tWt[0]\t%u\tgrdidx[1]\t%u\tdrop[1]\t%d\tWt[1]\t%u\r\n",
	                         app->pShm->sys_stat.DispShackle.GradeIndex[0],
	                         app->pShm->sys_stat.DispShackle.drop[0],
	                         app->pShm->sys_stat.DispShackle.weight[0],
						 app->pShm->sys_stat.DispShackle.GradeIndex[1],
	                         app->pShm->sys_stat.DispShackle.drop[1],
	                         app->pShm->sys_stat.DispShackle.weight[1]);
	                 break;

	             case _TShackleStatus:

	                     for (k = 0; k < MAXSCALES; k++)
	                     {
							 SendToClient("index\t%d\tGrdIdx\t%d\r\n",
									dbg_idx, app->pShm->ShackleStatus[dbg_idx].GradeIndex[0],
									dbg_idx, app->pShm->ShackleStatus[dbg_idx].GradeIndex[1]);

	                         SendToClient("drop\t%d\tweight\t%d\ttare\t%d\r\n",
	                                       app->pShm->ShackleStatus[dbg_idx].drop[k],
	                                       app->pShm->ShackleStatus[dbg_idx].weight[k],
	                                       app->pShm->ShackleTares[dbg_idx].tare[k]);
	                     }
	                 break;

	             case _TIsysLineStatus:

	                     SendToClient("pksent\t%d\tpkrec\t%d\tthrds\t%X\r\n",
	                               app->pShm->IsysLineStatus.pps_sent,
	                               app->pShm->IsysLineStatus.pps_recvd,
	                               app->pShm->IsysLineStatus.app_threads);

	                     for (k = 0; k < MAXIISYSLINES; k++)
	                     {
	                         SendToClient("flags[%d]\t%d\tconnected\t%1.1X\r\n", k,
	                                       app->pShm->IsysLineStatus.flags[k],
	                                       app->pShm->IsysLineStatus.connected[k]);
	                     }
	                 break;

	             case _DigLCSet:
			        SendToClient("\nindex\t%d\tmmode\t%d\tASF\t%d\r\n"
				        "FMD\t%d\tICR\t%d\tLDW\t%d\r\n"
				        "LWT\t%d\tNOV\t%d\tRSN\t%d\r\n"
				        "MTD\t%d\tLIC0\t%d\tLIC1\t%d\r\n"
				        "LIC2\t%d\tLIC3\t%d\tZTR\t%d\r\n"
				        "ZSE\t%d\tTRC1\t%d\tTRC2\t%d\r\n"
						"TRC3\t%d\tTRC4\t%d\tTRC5\t%d\r\n\r\n",
				        dbg_idx,
				        app->pShm->scl_set.DigLCSet[dbg_idx].measure_mode,
				        app->pShm->scl_set.DigLCSet[dbg_idx].ASF,
				        app->pShm->scl_set.DigLCSet[dbg_idx].FMD,
				        app->pShm->scl_set.DigLCSet[dbg_idx].ICR,
				        app->pShm->scl_set.DigLCSet[dbg_idx].LDW,
				        app->pShm->scl_set.DigLCSet[dbg_idx].LWT,
				        app->pShm->scl_set.DigLCSet[dbg_idx].NOV,
				        app->pShm->scl_set.DigLCSet[dbg_idx].RSN,
				        app->pShm->scl_set.DigLCSet[dbg_idx].MTD,
				        app->pShm->scl_set.DigLCSet[dbg_idx].LIC0,
				        app->pShm->scl_set.DigLCSet[dbg_idx].LIC1,
				        app->pShm->scl_set.DigLCSet[dbg_idx].LIC2,
				        app->pShm->scl_set.DigLCSet[dbg_idx].LIC3,
				        app->pShm->scl_set.DigLCSet[dbg_idx].ZTR,
				        app->pShm->scl_set.DigLCSet[dbg_idx].ZSE,
				        app->pShm->scl_set.DigLCSet[dbg_idx].TRC1,
				        app->pShm->scl_set.DigLCSet[dbg_idx].TRC2,
				        app->pShm->scl_set.DigLCSet[dbg_idx].TRC3,
				        app->pShm->scl_set.DigLCSet[dbg_idx].TRC4,
				        app->pShm->scl_set.DigLCSet[dbg_idx].TRC5);


	                     break;
	             case _lc_sample_adj:
			        SendToClient("\r\nindex\t%d\tsample_start\t%d\tsample_end\t%d\r\n",
				        dbg_idx,
				        app->pShm->scl_set.LC_Sample_Adj[dbg_idx].start,
				        app->pShm->scl_set.LC_Sample_Adj[dbg_idx].end);
	                 break;

			case _TMiscFeatures:
					SendToClient("EnableGradSync2\t0x%x\tEnableBoazMode\t0x%x\tRequireZeroBeforeSync\t0x%x\tSpare\t0x%x\n",
						app->pShm->sys_set.MiscFeatures.EnableGradeSync2,
						app->pShm->sys_set.MiscFeatures.EnableBoazMode,
						app->pShm->sys_set.MiscFeatures.RequireZeroBeforeSync,
						app->pShm->sys_set.MiscFeatures.spare);
				break;

	             default:
	                 SendToClient(" Not functional!\r\n");
	                 break;
		}
		}
	}
 	return 0;
}

/*******************************************************************************/
/*                                                                             */
/* SetTrace:	Shell command to set the trace level                           */
/*                                                                             */
/*******************************************************************************/

static int SetTrace(int Argc, char** ppArgv)
{
	int SetTraceMask = 0x0;
	int UdpEnable = 0;

    if (Argc != 3)
        {
			SendToClient("\r\nUsage: settrace <TraceMsk> {0x0 - 0xFFFFFFFF}"
						"<UDP enable> {0,1}.\r\n");
			SendToClient("\t_CHKRNG_ \t0x00000001" _CrLf
					"\t_FDROPS_      0x00000002" _CrLf
					"\t_DROPS_       0x00000004" _CrLf
					"\t_HOST_COMS_   0x00000010" _CrLf
					"\t_WEIGHT_      0x00000020" _CrLf
					"\t_GRADE_       0x00000040" _CrLf
					"\t_SYNCS_       0x00000080" _CrLf
					"\t_ZEROS_       0x00000100" _CrLf
					"\t_TRICKLE_     0x00000200" _CrLf
					"\t_IPC_         0x00000400" _CrLf
					"\t_COMBASIC_    0x00000800" _CrLf
					"\t_COMQUEUE_    0x00001000" _CrLf
					"\t_DRPMGRQ_     0x00002000" _CrLf
					"\t_ISCOMS_      0x00004000" _CrLf
					"\t_WTAVG_       0x00008000" _CrLf
					"\t_HBMLDCELL_   0x00100000" _CrLf
					"\t_DEBUG_       0x00800000" _CrLf
					"\t_ALWAYS_ON_   0x80000000" _CrLf);
			SendToClient("Current TraceMask is set to 0x%8.8x\r\n", TraceMask);

			return(0);
        }
    else
        {
			SetTraceMask = strtoul(ppArgv[1], NULL, 16);
			UdpEnable = atol(ppArgv[2]);
			if((SetTraceMask < 0xFFFFFFFF) && ((UdpEnable == 0) || (UdpEnable == 1)))
			{
				SendToClient("\r\nDebug setting TraceMask to 0x%8.8x\r\n", SetTraceMask);
				TraceMask = SetTraceMask;
				app->pShm->dbg_set.dbg_trace = SetTraceMask;
				if (UdpEnable)
				{
					app->pShm->dbg_set.UdpTrace = UDP_ENABLE;
				}
				else
				{
					app->pShm->dbg_set.UdpTrace = 0x0;
				}
			}
			else
			{
				SendToClient("\r\nDebug bad TraceMask 0x%8.8x\r\n", SetTraceMask);
			}
        }

	return 0;
}


/*******************************************************************************/
/*                                                                             */
/* ReadLoadCell:	Shell command to log N load cell readings                  */
/*                                                                             */
/*******************************************************************************/

static int ReadLoadCell(int Argc, char** ppArgv)
{
	int tmpReads = 1;

    if (Argc != 2)
        {
			SendToClient("\r\nUsage: readloadcell <NumReads> {1 - 5000}\r\n");
			SendToClient("Current number of reads is set to %d\r\n", ReadsToSample);
			return(0);
        }
    else
		{
			tmpReads = atol(ppArgv[1]);
			if ((tmpReads > 0) && (tmpReads <= 5000))
			{
				ReadsToSample = tmpReads;
				SendToClient("Current reads to sample is %d\r\n",
					ReadsToSample);
				TakeLoadCellReadsFlag = app->pShm->scl_set.NumScales;
			}
			else
			{
				SendToClient("\r\nUsage: readloadcell <NumReads> {1 - 5000}\r\n");
			}
		}

	return 0;
}

/*******************************************************************************/
/*                                                                             */
/* RunCmd:		Runs the command after building the argument list              */
/*                                                                             */
/*******************************************************************************/

static int RunCmd (

	tCmdEntry*	pCmdEntry,
	char*		pBuf,
	int			Len)

	{
	int		Argc;
	char*	pArgv[_MAXARGS];
	int		Idx;

	/* first argument - the command name - pArgv[0]. */
	Argc = 0;
	pArgv[Argc++] = pCmdEntry->pCmd;

	/* the rest of the arguments. */
	if (*pBuf && Len > 0)
		{
		/* Strip leading space */
		Idx = 0;
		while( ' ' == pBuf[Idx])
			{
			Idx++;
			}

		/* Set the argc and argv's. */
		pArgv[Argc++] = &pBuf[Idx];
		for (; Idx < Len; Idx++)
			{
			if (pBuf[Idx] == ' ')
				{
				/* Null terminate this argument. */
				pBuf[Idx++] = '\0';

				/* Skip the spaces. */
				while (pBuf[Idx] == ' ')
					{
					Idx++;
					}

				/* Next arg starts here. */
				if (pBuf[Idx] != '\0')
					{
	     			pArgv[Argc++] = pBuf + Idx;
					}
				}
			}

		/* NULL terminate the end of the argument list */
		pArgv[Argc] = NULL;
		}

	/* Run the command. */
	return (pCmdEntry->pCmdFunction(Argc, pArgv));
	}

/*******************************************************************************/
/*                                                                             */
/* ParseCmd:	Parses the command line from input buffer                      */
/*                                                                             */
/*******************************************************************************/
int ParseCmd (

	char*	pBuf,
	int		Len)

{
	tCmdEntry*	pCmdEntry;
	int			Idx;
	int			StartIdx;

	/* Get the first argument which is the command to run. */
	for (StartIdx = 0, Idx = 0; Idx < Len; Idx++)
		{
		if ((pBuf[Idx] == ' ') && (Idx == 0))
			{
			/* Skip the leading spaces. */
			while (pBuf[Idx] == ' ')
				{
				Idx++;
				StartIdx++;
				}
			}
		if ((pBuf[Idx] == ' '))
			{
			/* Skip the trailing spaces. */
			break;
			}
		}

	/* Terminate end of command */
	pBuf[Idx++] = '\0';

	/* Look up the command and run it if there is one. */
	if ((pCmdEntry = FindCmd(&pBuf[StartIdx])) != NULL)
		{
		if(RunCmd(pCmdEntry, pBuf + Idx,
							 Len - Idx) == -1)
			{
			SendToClient("invalid parameter\n\r");
			return 0;
			}
		}
	else
		{
		SendToClient("unknown command\n\r");
		}
	return 0;
}

/*******************************************************************************/
/*                                                                             */
/* FindCmd:	Find the command from the table pointer.                           */
/*                                                                             */
/*******************************************************************************/

static tCmdEntry* FindCmd (

	char *		pCmd)

{
	int			Idx;
	const tCmdEntry*	pCmdList;

	pCmdList = pCmdLists;
	for (Idx = 0; pCmdList[Idx].pCmd != NULL; Idx++)
		{
		if (strcmp(pCmd, pCmdList[Idx].pCmd) == 0)
			{
			return (tCmdEntry *)&pCmdList[Idx];
			}
		}

	return NULL;
}

/*******************************************************************************/
/*                                                                             */
/* CmdHelp:	Command for a list of available commands                           */
/*                                                                             */
/*******************************************************************************/

static int CmdHelp (

	int			Argc,
	char**		ppArgv)

{
	int	Idx;
	const tCmdEntry*	pList;
	(void)Argc;
	(void)ppArgv;

	SendToClient("\r\n           Command List                 \r\n");
	SendToClient("--------------------------------------------\r\n");

	pList = pCmdLists;

	for (Idx = 0; pList[Idx].pCmd != NULL; Idx++)
		{
		SendToClient("%-10s %s\r\n", pList[Idx].pCmd,
			pList[Idx].pDescription);
		}

	SendToClient("--------------------------------------------\r\n");

	return 0;
}

/*******************************************************************************/
/*                                                                             */
/* TermInput: Handles keyboard input from telnet client                        */
/*                                                                             */
/*******************************************************************************/

void TermInput (

	char* pInput)

{
	UINT	Idx;
	char	Ch;
	int		Len = 0;
	int		i	= 0;
	char	EchoBuf[2];

	Len = strlen(pInput);
	if((Len < _CMDLINE_SIZE) & (Len > 0))
	{
		for(i = 0; i < Len; i++)
		{
			Ch = pInput[i];

			switch (Ch)
				{
				case _Cr:
				case _Lf:
					if (InIdx > 0)
						{
						CmdIn[InIdx] = Ch;
						for (Idx = 0; Idx < _CMDLINE_SIZE; Idx++)
							{
							if (CmdIn[Idx] == _Cr || CmdIn[Idx] == _Lf)
								{
								CmdIn[Idx] = '\0';
								SendToClient(_CrLf);
								ParseCmd((char *)CmdIn, Idx);

								break;
								}
							}

						InIdx = 0;
						SendToClient("%s", (char *)pPrompt);
						}
					else
						{
						SendToClient(_CrLf);
						SendToClient("%s",  (char *)pPrompt);
						}

					break;

				case _Bs:
					if (InIdx > 0)
						{
						CmdIn[InIdx--] = '\0';
						SendToClient(&Ch);
						Ch = _Sp;
						SendToClient(&Ch);
						Ch = _Bs;
						SendToClient(&Ch);
						}
					break;

				default:
		            if( _CMDLINE_SIZE <= InIdx )
						{
						CmdIn[InIdx] = '\0';
						SendToClient(_CrLf);
						InIdx = 0;
						SendToClient("%s", (char *)pPrompt);
						memset((void *)&CmdIn[0], 0, _CMDLINE_SIZE);
						break;
						}
					else
						{
						CmdIn[InIdx++] = Ch;
						EchoBuf[0] = Ch;
						EchoBuf[1] = '\0';
						SendToClient(&EchoBuf[0]);
						break;
						}
				}
		}
	}
}

/*******************************************************************************/
/*                                                                             */
/* SendToClient: Copy string to term shared memory location. The interface     */
/*				 process will poll this memory for new entries and then        */
/*				 send out to the telnet client.                                */
/*                                                                             */
/*******************************************************************************/

void SendToClient(char* pOutBuffer, ...)
{
	TERM_IO *pstioTermIO=NULL;
	va_list ap;
	char Buffer[MAXTERMOUT];
	int Rc = 0;

	if (pOutBuffer != NULL)
	{
		va_start(ap, pOutBuffer);
		Rc = vsprintf(&Buffer[0], pOutBuffer, ap);
		va_end(ap);
		pstioTermIO=(TERM_IO *)atdTelnetd[0].ssmMShare.pSharedMemory;
		if (Rc > 0)
		{
			strcpy(pstioTermIO->acOut, &Buffer[0]);
			pstioTermIO->acOut[Rc] = '\0';
			pstioTermIO->bCanChangeOut=0;
			Sleep(100);
		}
		else
		{
			strcpy(pstioTermIO->acOut, "vsprintf error formatting string\n");
			pstioTermIO->acOut[Rc] = '\0';
			pstioTermIO->bCanChangeOut=0;
			Sleep(100);
		}
	}
}

/*******************************************************************************/
/*                                                                             */
/* TerminalThread: Polls shared memory for new input.                          */
/*                                                                             */
/*******************************************************************************/
int __stdcall TerminalThread(PVOID unused)
{
	(void)unused;
	TERM_IO *pstioTermIO=NULL;
	int iID;
	int iLoopCnt=0;

	iID=0;

	pstioTermIO=(TERM_IO *)atdTelnetd[iID].ssmMShare.pSharedMemory;

	if(pstioTermIO != NULL)
	{
		strcpy(pstioTermIO->acOut, TELNET_WELCOME);
		pstioTermIO->bCanChangeOut=0;
	}
	else
	{
		return(ERR);
	}

	iLoopCnt=0;
	int forever = 1;
	while(forever)
	{
		if(pstioTermIO->bCanChangeIn==0)
		{
			TermInput (&pstioTermIO->acIn[0]);
			pstioTermIO->bCanChangeIn = 1;
		}
		Sleep(REFRESHDELAY);
		forever = forever ? 1 : 0;
	}
	return (ERR);
}/* TerminalThread */
