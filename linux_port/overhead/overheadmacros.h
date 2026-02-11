#pragma once

// save drop records, clear outputs and shutdown
#define EXCEPTION_SHUTDOWN \
app->saveDrpRecs = true; \
Sleep(500); \
app->ClearOutputs(); \
app->pShm->AppFlags = 0;

#define SYNC_OK     (pSyncStat->zeroed) && (trolly_counters[i] == syncOffset) //jdc

// Verify if the current shackle number is within range.......
#define SHACKLE_OK(shk)  ((shk >= 0) && (shk <= pShm->sys_set.Shackles))

#define DBG_DROP(drp, pShm) ((drp == pShm->dbg_set.dbg_extra) || (pShm->dbg_set.dbg_extra == 99))

//----- If a drop is suspended or batched, show it (except for inactive drop).

#define SHOW_DROP_STATUS(idx,md) \
\
if ((pShm->sys_set.DropSettings[idx].Active) && DBG_DROP(idx, pShm))\
{ \
    if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _FDROPS_ ) ) \
    { \
        sprintf((char*) &tmp_trc_buf[MAINBUFID],"DMode [%13s]\tdrop\t%d\tSync\t%d\tNxtEnt\t%d\n\t\t\tsusp\t%d\tbchd\t%d\n", \
                      dist_mode_desc[md], \
                      idx + 1, \
                      pShm->sys_set.DropSettings[idx].Sync, \
                      pShm->Schedule [idx].NextEntry,  \
                      pShm->sys_stat.DropStatus[idx].Suspended, \
                      pShm->sys_stat.DropStatus[idx].Batched); \
        strcat((char*) &trc_buf[MAINBUFID], (char*) &tmp_trc_buf[MAINBUFID]); \
    } \
}

//----- Determines if the drop is ok to use
//           ACTIVE, NOT SUSPENDED, NOT BATCHED, and VALID SYNC
#define  DROP_OK(idx) ( (app->pShm->sys_set.DropSettings [idx].Sync != bad_sync)  && \
                        (app->pShm->sys_set.DropSettings [idx].Active)      &&  \
                        (!app->pShm->sys_stat.DropStatus[idx].Suspended) &&  \
                        (!app->pShm->sys_stat.DropStatus[idx].Batched) )

//----- Determines if the drop is ok to use
//           ACTIVE, and VALID SYNC
#define  DROP_USABLE(idx) ((app->pShm->sys_set.DropSettings [idx].Sync != bad_sync)  && \
                           (app->pShm->sys_set.DropSettings [idx].Active))

//			This checks to see if the current drop is valid
//			Is it a SCALE 1 drop or a SCALE 2 drop
#define  GOOD_INDEX(scl,idx) \
            (((scl == 0) && (idx >= 0) && (idx < pShm->sys_set.LastDrop)) || \
             ((scl == 1) && (idx >= 0) && (idx >= pShm->sys_set.LastDrop) && (idx <  pShm->sys_set.NumDrops)) )

// This checks to see if the loop is finished.
//#define  FINISHED_LOOP ((loops++ >= app->pShm->sys_set.LastDrop) || (drp_index == start_drp_index))
#define  FINISHED_LOOP(count) ((count++ >= app->pShm->sys_set.NumDrops) || (drp_index == start_drp_index))



#define  CLEAR_DROP_BATCH(drp, pShm) \
pShm->sys_stat.ScheduleStatus[drp].BchActCnt = 0; \
pShm->sys_stat.ScheduleStatus[drp].BchActWt  = 0; \
pShm->sys_stat.DropStatus[drp].Batched       = 0; \
pShm->sys_stat.DropStatus[drp].batch_number  = 0; \
pShm->sys_stat.DropStatus[drp].last_in_batch = false; \
app->last_in_batch_dropped[drp] = false;

#define  CLEAR_ISYS_DROP_BATCH(drp, pShm) \
	{\
		for (int ln = 0; ln < MAXIISYSLINES; ln++) \
		{ \
			pShm->IsysDropStatus[drp].BCount[ln]       = 0; \
			pShm->IsysDropStatus[drp].BWeight[ln]      = 0; \
			pShm->IsysDropStatus[drp].batch_number[ln] = 0; \
			pShm->IsysDropStatus[drp].flags[ln]       &= ~ISYS_DROP_BATCHED; \
			pShm->IsysDropStatus[drp].flags[ln]       &= ~ISYS_DROP_OVERRIDE; \
		}\
		app->SetIsysFastestIndices(drp, true); \
	}

#define  TRICKLE_INFO(str,drp) \
    \
if (DBG_DROP(drp, pShm)) \
{ \
    if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _TRICKLE_ ) ) \
    { \
        sprintf((char*) &tmp_trc_buf[MAINBUFID],"%-10s\tdrop\t%d\ttarget\t%d\tshort\t%d\tact\t%d\n", \
            str, drp, \
            target_trickle_count[drp], \
            trickle_short_cnt   [drp], \
            pShm->sys_stat.ScheduleStatus[drp].BPMTrickleCount); \
        strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID] ); \
    } \
}

//----- Shack...WeighSha.. (too long)
#define  WEIGH_SHACKLE(scale, pShm)  pShm->ShackleStatus[pShm->WeighShackle[scale]]
#define  TARE_SHACKLE(scale, pShm)   pShm->ShackleTares[pShm->WeighShackle[scale]].tare[scale]


//----- Check for valid grade

#define VALID_GRADE(byt) (((byt >= 'A')  &&  \
                           (byt <= 'D'))  || \
                           (byt == 'N'))

//----- Multipurpose print for debugging checkrange

#define CHK_RANGE_OK(str, pShm) \
  \
if (DBG_DROP(drop, pShm))  \
{ \
    if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _CHKRNG_ ) ) \
    { \
        sprintf((char*) &tmp_trc_buf[MAINBUFID],"CheckRange\tGood\tdrop\t%d\t[%13s]\tshkl\t%d\tWeight\t%u\tgrade\t%1c\n", \
                  drop+1, str, pShm->WeighShackle[scale], weight, grade);  \
        strcat((char*) &trc_buf[MAINBUFID], (char*) &tmp_trc_buf[MAINBUFID] ); \
    } \
}

//----- Shorter way to call checkrange

#define CHK_RANGE(drp_chk, scale, pShk) ((chkrng_ret_cd = CheckRange(drp_chk, scale, pShk)) == drop_it)

//----- Check mode and assign a new batch number. Limit to F423F 999999 (6 digits)
//      add line number in upper nibble

#define APPLY_BATCH_NUMBER(drp, drop_master) \
{ \
	UINT line_mult; \
	if ( ((drop_master == this_lineid) || (drop_master == NOT_AN_ISYS_DROP)) && \
		 (pShm->sys_stat.DropStatus[drp].batch_number == 0) ) \
	{ \
		line_mult = this_lineid << LINE_SHIFT; \
		switch(pShm->Schedule[drp].DistMode) \
		{ \
			case mode_3_batch: \
			case mode_5_batch_rate: \
			case mode_6_batch_alt_rate: \
			case mode_7_batch_alt_rate: \
				pShm->sys_stat.DropStatus[drp].batch_number = sav_drp_rec_file_info.nxt_lbl_seqnum + line_mult; \
				if (HOST_OK) {LABEL_INFO(drp)} \
				if(++sav_drp_rec_file_info.nxt_lbl_seqnum >= MAXBCHLBLNUM) \
				   sav_drp_rec_file_info.nxt_lbl_seqnum = 1; \
				break; \
			default: \
				break; \
		} \
	} \
}

//----- If drop is a slave drop and no batch number, assign a temporary
//      batch number and request a batch number from the drop master.
//      The validate routine will search the shackle data for temporary
//      batch numbers and replace it with the real one.

#define SET_ASSIGN_OK(assign_ok, drop_master)\
{ \
	int master_drop = 0; \
	assign_ok = false; \
	switch(pShm->Schedule[drp_chk].DistMode) \
	{ \
		case mode_3_batch: \
		case mode_5_batch_rate: \
		case mode_6_batch_alt_rate: \
		case mode_7_batch_alt_rate: \
			if (pShm->sys_stat.DropStatus[drp_chk].batch_number != 0) \
				assign_ok  = true; \
			else \
			{  \
				if( (this_lineid != drop_master) && (drop_master != NOT_AN_ISYS_DROP) )  \
				{ \
					pShm->sys_stat.DropStatus[drp_chk].batch_number = TEMP_BATCH_NUM; \
					assign_ok = true; \
					master_drop = pShm->sys_set.IsysDropSettings[drp_chk].drop[drop_master - 1]; \
					if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _LABELS_) ) \
					{ \
						sprintf((char*) &tmp_trc_buf[MAINBUFID],"Requesting batch# from line %d drop %d\n",drop_master,master_drop); \
						strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID] ); \
					} \
					SendDrpManager( DROP_MAINT, master_drop, drop_master - 1, isys_drop_batch_req, isys_send_single, NULL ); \
				} \
			} \
			break; \
		default: \
			assign_ok  = true; \
			break; \
	} \
}

//----- Show checkrange fail reason

#define CHK_RANGE_STATUS(drp_chk, pShm, scale)  \
  \
if (DBG_DROP(drp_chk, pShm)) \
{ \
    if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _FDROPS_ ) ) \
    { \
        sprintf((char*) &tmp_trc_buf[MAINBUFID],"FindDrops\tdrop\t%d\tFail\t[%13s]\tshkl\t%d\tweight\t%d", \
                      drp_chk + 1, chk_rng_desc[chkrng_ret_cd], pShm->WeighShackle[scale], \
					  pShm->ShackleStatus[0].weight); \
        strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID] ); \
        if (chkrng_ret_cd == weight_out_of_range) \
        { \
            sprintf((char*) &tmp_trc_buf[MAINBUFID],"\twt\t%d\n",WEIGH_SHACKLE(scale, pShm).weight[scale]); \
            strcat((char*) &trc_buf[MAINBUFID], (char*) &tmp_trc_buf[MAINBUFID] ); \
        } \
        else \
        { \
            sprintf((char*) &tmp_trc_buf[MAINBUFID],"\tgrd\t%c\n",pShm->sys_set.GradeArea[WEIGH_SHACKLE(scale, pShm).GradeIndex[scale]].grade); \
            strcat((char*) &trc_buf[MAINBUFID], (char*) &tmp_trc_buf[MAINBUFID] ); \
        } \
    } \
}

//----- Compare weight to max/min values

#define WEIGHT_CHECK(weight, min,max) ((weight != 0) && (weight >= min) && (weight <= max))


//----- Same as above, but for extended grade

#define CHECK_EXT_GRADE \
if (pShm->sys_set.DropSettings[drop].Extgrade) \
{\
    for (i = 0; i <= MAXGRADES - 1; i++)\
    {\
        if (psched->Extgrade[i] == grade)\
        {\
            grade_ext_OK  = true;\
            break;\
        }\
    }\
}

//----- Adds all the BPM totals for a drop, doesn't do much if not intersystems

#define GET_BPM_TOTAL(drop) \
\
TotalPPMCount = psched_stat->PPMCount;\
\
if ISYS_ENABLE \
{\
    for (int i = 0; i < MAXIISYSLINES; i++)\
    {\
        if ( (pShm->sys_set.IsysLineSettings.active[i]) && \
             (pShm->sys_set.IsysDropSettings[drop].active[i]) && \
             (i != this_lineid - 1) ) \
            TotalPPMCount += pShm->IsysDropStatus[drop].PPMCount[i];\
    }\
}

#define CNV_WT(num) ((double) num * CNTS)

//----- Intersystems macros

#define ISYS_ENABLE      (app->pShm->sys_set.IsysLineSettings.isys_enable > 0)
#define ISYS_MASTER      (app->pShm->sys_set.IsysLineSettings.isys_master > 0)

//----- The first intersystems line sharing drops is the drop master.
//      Example lines 2 & 3 share isys drop x, line two is the drop master.

#define GET_DROP_MASTER(drp_idx, pShm) \
{ \
    drop_master = NOT_AN_ISYS_DROP; \
	if (pShm->sys_set.IsysDropSettings[drp_idx].active[app->this_lineid - 1]) \
	{ \
		for (int dx = 0; dx < app->this_lineid; dx++) \
		{ \
			if ( pShm->sys_set.IsysDropSettings[drp_idx].active[dx] )\
			{ \
				if (app->isys_comm_fail[dx] == 0) \
				{ \
				drop_master = dx + 1; \
				break; \
				} \
			} \
		} \
	} \
}

//----- Populate data for batch labels

#define LABEL_INFO(drp) \
{ \
    int     lp; \
    time_t  long_time; \
    struct  tm *ptime; \
    time( &long_time ); \
    ptime = localtime( &long_time ); \
    for (lp = 0; lp < MAXBCHLABELS; lp++) \
    { \
        if (app->batch_label.info[lp].seq_num == 0) \
        { \
            memcpy(&app->batch_label.info[lp].time, ptime, sizeof(tm)); \
            app->batch_label.info[lp].label_step    = 0; \
            app->batch_label.info[lp].drop          = drp+1; \
            app->batch_label.info[lp].cnt           = 0; \
            app->batch_label.info[lp].wt            = 0; \
            app->batch_label.info[lp].seq_num       = app->pShm->sys_stat.DropStatus[drp].batch_number; \
            if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _LABELS_) ) \
            { \
                sprintf((char*) &tmp_trc_buf[MAINBUFID],"New\tRemReq\tbch#\t%d\tdrop\t%d\tindx\t%d\n", \
                         app->batch_label.info[lp].seq_num & BATCH_MASK, drp+1, lp); \
                strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID] ); \
            } \
            break; \
        } \
    } \
}

#define ISYS_DROP(drp, pShm) (pShm->sys_set.IsysLineSettings.active[app->this_lineid-1] && pShm->sys_set.IsysDropSettings[drp].active[app->this_lineid-1])

//----- MissedBirdCheck macro

#define MB_ENABLED(MBEcnt) (MBEcnt == ((pShm->sys_set.MissedBirdEnable[0] ? 1 : 0) + (pShm->sys_set.MissedBirdEnable[1] ? 1 : 0)))

//DRW Added below for reseting grade on empty shackles only 06/03/2015 \

#define UNDROP_SCALE(idx, pShm, shk) \
if (drop[idx]) \
{ \
    if (pShm->sys_set.ResetGrading.ResetScale1Grade > 0)\
    SubtractBird(shk->GradeIndex[idx], shk->drop[idx], shk->weight[idx]); \
    else\
    SubtractBird(shk->GradeIndex[0], shk->drop[idx], shk->weight[idx]); \
   if DBG_DROP(shk->drop[idx] - 1, pShm) \
    { \
        if( (!trc[MAINBUFID].buffer_full) && (TraceMask & _FDROPS_ ) ) \
        { \
            sprintf((char*) &tmp_trc_buf[MAINBUFID],"MBUndrp\tmb\t%d\tdrop\t%d\tshkl\t%d\twt\t%d\n", \
                          curr_mb, shk->drop[idx], shackle, shk->weight[idx]); \
            strcat((char*) &trc_buf[MAINBUFID],(char*) &tmp_trc_buf[MAINBUFID] ); \
        } \
    } \
}

#define MISSED(idx, shk) \
if (pShm->sys_set.ResetGrading.ResetScale1Grade > 0)\
    AddMissedBird(shk->GradeIndex[idx], shk->drop[idx], shk->weight[idx]);\
else\
    AddMissedBird(shk->GradeIndex[0], shk->drop[idx], shk->weight[idx]);\
shk->dropped[idx] = 0;

#define DUMP_SAVED_DRECS(len)  \
{ \
for (UINT i = 0; i < (len/sizeof(app_drp_rec)); i++) \
RtPrintf("DrecSavBuf\t%d//%d//%d %d:%d seq\t%x\tshkl\t%d\n",  \
                  sav_rec_buf[i].d_rec.time.tm_mon, \
                  sav_rec_buf[i].d_rec.time.tm_wday, \
                  sav_rec_buf[i].d_rec.time.tm_year, \
                  sav_rec_buf[i].d_rec.time.tm_hour, \
                  sav_rec_buf[i].d_rec.time.tm_min, \
                  sav_rec_buf[i].seq_num, \
                  sav_rec_buf[i].d_rec.shackle); \
}

#define BITSET(byt,bit)  (( byt & Mask[bit & 0x7] ) != 0)
#define SETBIT(byt,bit)     byt |=  Mask[bit & 0x7];
#define CLRBIT(byt,bit)     byt &= ~Mask[bit & 0x7];

#define HOST_OK (app->pShm->IsysLineStatus.connected[HOST_INDEX] != 0)

#define CLEAR_TOTALS \
pShm->sys_stat.TotalCount       = 0; \
pShm->sys_stat.TotalWeight      = 0; \
memset(&pShm->sys_stat.MissedDropInfo, 0, sizeof(pShm->sys_stat.MissedDropInfo)); \
memset(&pShm->sys_stat.DropInfo,       0, sizeof(pShm->sys_stat.DropInfo)); \
memset(&pShm->sys_stat.GradeCount,     0, sizeof(pShm->sys_stat.GradeCount)); \
memset(&pShm->sys_stat.DropStatus,     0, sizeof(pShm->sys_stat.DropStatus)); \
memset(&pShm->sys_stat.ScheduleStatus, 0, sizeof(pShm->sys_stat.ScheduleStatus)); \
memset(&pShm->IsysDropStatus,          0, sizeof(pShm->IsysDropStatus)); \
memset(&app->Second_3724_Output,       0, sizeof(app->Second_3724_Output)); \
app->Second_3724_WriteOutput = true; \
{ \
	for (int x = 0; x < MAXDROPS; x++) \
		app->last_in_batch_dropped[x] = false; \
} \
pShm->sys_set.IsysLineSettings.reset_totals[this_lineid-1] = 0;

 // If next to send is not the current file, check cur_file, reset to 0 if invalid ( > MAXDSAVFILES)
// If next to send is not the current file, inc next to send
// If next to send is same file as current, just clear count
// save file.
#define UPDATE_FILE_INFO \
if (sav_drp_rec_file_info.nxt2_send != sav_drp_rec_file_info.cur_file) \
{ \
if (sav_drp_rec_file_info.cur_file >= MAXDSAVFILES) \
{ \
sav_drp_rec_file_info.cur_file  = 0; \
sav_drp_rec_file_info.cur_cnt   = 0; \
sav_drp_rec_file_info.nxt2_send = 0; \
} \
else \
{ \
if (++sav_drp_rec_file_info.nxt2_send >= MAXDSAVFILES) \
sav_drp_rec_file_info.nxt2_send = 0; \
} \
} \
else \
sav_drp_rec_file_info.cur_cnt = 0; \
updateDrecInfo = true;

// clear the record, if both finished
#define CLEAR_BCH_INFO(idx) \
if ( (batch_label.info[idx].label_step     == 1) &&  \
     (batch_label.info[idx].pre_label_step == 2) ) \
{ \
    if( (!trc[EVTBUFID].buffer_full) && (TraceMask & _LABELS_) ) \
    { \
        sprintf((char*) &tmp_trc_buf[EVTBUFID],"Mbx\tremv\tbch#\t%d\tindx\t%d\n", \
                batch_label.info[idx].seq_num & BATCH_MASK, idx); \
        strcat((char*) &trc_buf[EVTBUFID],(char*) &tmp_trc_buf[EVTBUFID] ); \
    } \
    memset ( &batch_label.info[idx].label_step, 0, sizeof(print_info)); \
}

#define CAPTURE_WT  capt_wt.capture_weights[capt_wt.curr_buf][capt_wt.curr_index]

#define LC_CAPTURE(scl) \
{ \
    __int64 wt; \
    if (weight_simulation_mode) \
        CAPTURE_WT = sim->SimWeight(); \
    else \
    { \
        if(RtWaitForSingleObject(load_cell_mutex[MAINBUFID], WAIT50MS) != WAIT_OBJECT_0) \
            RtPrintf("Wait failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__); \
        else \
        { \
			if (app->pShm->scl_set.LoadCellType == LOADCELL_TYPE_1510 ) \
			{ \
				wt = app->ld_cel[0]->read_load_cell(scl); \
			} \
			else \
			{ \
				wt = app->ld_cel[scl]->read_load_cell(0); \
			} \
            INDIV_READS(scl, FUNC_CAPT, wt) \
 \
            if(!RtReleaseMutex(load_cell_mutex[MAINBUFID])) \
                RtPrintf("Release failed. GetLastError = %u file %s, line %d\n", GetLastError(), _FILE_, __LINE__); \
        } \
    } \
    capt_wt.curr_index++; \
}

// Set send flag, length and swap buffers so capure can contunue while sending.
#define LC_SEND(len) \
capt_wt.send_capture[capt_wt.curr_buf] = true; \
capt_wt.send_length [capt_wt.curr_buf] = len;  \
capt_wt.curr_buf   = nxt_cap_buf[capt_wt.curr_buf];


// (re) Initialize capture variables
#define RESET_PRODUCT_CAPTURE \
{capt_wt.capture_weights[capt_wt.curr_buf][0] = -1; \
 capt_wt.capture_weights[capt_wt.curr_buf][1] = -1; \
 capt_wt.curr_index = READ_INDEXES; \
 capture_period     = -1;}

// The fun variable identifies the calling function. The result will be
// placed in a different location based on it.
#define INDIV_READS(s,fun,val) \
if (app->pShm->scl_set.AdcMode[s] == INDIVID) \
{ \
    int     i; \
    __int64 tmp_cnt = 0; \
    __int64 tmp_wt  = 0; \
    for (i = 0; i < app->pShm->scl_set.NumAdcReads[s]; i++) \
    { \
       if ( ( individual_wts[i] <= val + individual_lim) && \
            ( individual_wts[i] >= val - individual_lim) ) \
       { \
             tmp_wt += individual_wts[i]; \
             tmp_cnt++; \
       } \
/*       else  \
           RtPrintf("Ind mode shk %d avg %d ignore wt %d\n", \
                     pShm->WeighShackle[s], (int) raw_wts[s][1], individual_wts[i]); \
*/    } \
\
    if (tmp_cnt > 0) val = (tmp_wt / tmp_cnt); \
} \
switch(fun) \
{ \
    case FUNC_RAW: \
        app->pShm->sys_stat.raw_weight = val; \
        break; \
    case FUNC_AVG: \
        w_avg[s].weights[w_avg[s].curr_index] = val; \
        break; \
    case FUNC_CAPT: \
        CAPTURE_WT = val; \
        break; \
    default: \
        break; \
} \
/*    RtPrintf("GW reads %d tmpcnt %d\n", pShm->scl_set.NumAdcReads[pShm->raw_scale],tmp_cnt); \
RtPrintf("indiv read wt %d", pShm->sys_stat.raw_weight); \
for (int i = 0; i < pShm->scl_set.NumAdcReads[pShm->raw_scale]; i++) \
   RtPrintf("\t%d", individual_wts[i]); \
RtPrintf("\n"); \
*/

// Macro for weight average start/end ticks
// myStart and myEnd are a percentage; 0 - 100 percent
#define AVG_START   shk2shk_ticks - (shk2shk_ticks/3)
#define AVG_END     (AVG_START)/2
