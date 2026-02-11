// Simulator.cpp: Linux stub implementation of the Simulator class.
//
//////////////////////////////////////////////////////////////////////

#include "types.h"

#undef  _FILE_
#define _FILE_      "Simulator.cpp"

// weights will become a file soon

#define MAXSIMWEIGHTS 32

const double  sim_weight[MAXSIMWEIGHTS] =
                        {
    2.00, 2.10, 2.20, 2.30, 2.40, 2.50, 2.70, 2.80,
    3.20, 3.40, 3.30, 3.40, 3.50, 3.60, 3.80, 3.80,
    2.80, 2.60, 2.70, 2.80, 2.40, 2.30, 2.20, 2.10,
    3.50, 3.10, 3.80, 3.90, 3.50, 3.40, 3.30, 3.00
};

const int    sim_grades[5] =  {GRADE4_BIT, GRADE3_BIT, GRADE2_BIT, GRADE1_BIT, GRADE0_BIT};

unsigned sync_on_ticks;
unsigned sync_off_ticks;
double   bpm_setpoint;
double   bps;
double   cycle_time;
double   new_cycle_time;
double   on_time_mult;
double   on_time;
double   off_time;
int      grade_idx;

char    err_buf[MAXERRMBUFSIZE];

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Simulator::Simulator()
{
    initialize();
}

Simulator::~Simulator()
{
}

//--------------------------------------------------------
//  initialize
//--------------------------------------------------------

void Simulator::initialize()
{
//----- variables/settings for calculating the on/off times of
//      simulated photoeyes for a given shackle per minute setting.

    bpm_setpoint   = 200;
    bps            = bpm_setpoint / 60;             // convert to seconds
    cycle_time     = 1;                             // time for 1 cycle
    new_cycle_time = (cycle_time / bps) * 1000;     // new cycle time based on x bpm
    on_time_mult   = .180;                          // percent on time
    on_time        = on_time_mult * new_cycle_time; // new on time
    off_time       = new_cycle_time - on_time;      // new off time

    Create_Sim_Timer();
}

//--------------------------------------------------------
//  Create_Sim_Timer
//--------------------------------------------------------

void Simulator::Create_Sim_Timer()
{
    LARGE_INTEGER pExp, pInt;

    hSimTimer = RtCreateTimer(
                         NULL,               // Security - NULL is none
                         0,                  // Stack size - 0 is use default
                         Sim_Timer_Main,     // Timer handler
                         (PVOID) 0,          // NULL context (argument to handler)
                         RT_PRIORITY_MIN+50, // Priority
                         CLOCK_FASTEST);     // RTX HAL Timer

    //------------- Start Timer
    // 100ns * 10,000 = .001 sec intervals
    pExp.QuadPart = 10000;
    pInt.QuadPart = 10000;

    if (!RtSetTimerRelative( hSimTimer, &pExp, &pInt))
    {
        RtCancelTimer(hSimTimer, NULL);
        sprintf(err_buf,"Sim timer fail %s line %d\n", _FILE_, __LINE__);
        app->GenError(warning, err_buf);
        return;
    }

#ifdef _SHOW_HANDLES_
    RtPrintf("Hnd %.2X Simulator\n",hSimTimer);
#endif

    RtPrintf("NOTE: System is running in simulation mode.\n", (int) bpm_setpoint);
    RtPrintf("Line Speed   %3.3d Shk/min.\n", (int) bpm_setpoint);
    RtPrintf("Sync On      %3.3d msec.\n",    (int) on_time);
    RtPrintf("Sync Off     %3.3d msec.\n",    (int) off_time);
}

//--------------------------------------------------------
//  SimWeight
//--------------------------------------------------------

int Simulator::SimWeight()
{
    static int wt_idx = 0;
    double wt;

    wt = CNV_WT(sim_weight[wt_idx]);
    wt_idx++;

    if (wt_idx == MAXSIMWEIGHTS)
        wt_idx = 0;

    return (int) wt;
}

//--------------------------------------------------------
//  CycleTimes
//--------------------------------------------------------

void Simulator::CycleTimes()
{
    static bool start_flag = false;
    static int  on_ticks,  off_ticks;

    if ((!start_flag) && (app->sync_in[0] & 1))
    {
        start_flag = true;
    }

    if ((start_flag) && (!(app->sync_in[0] & 1)) )
    {
        start_flag = false;
        on_ticks  = 0;
        off_ticks = 0;
    }

    if (app->sync_in[0] & 1)
        on_ticks++;
    else
        off_ticks++;
}

//--------------------------------------------------------
//  RunSim
//--------------------------------------------------------

void Simulator::RunSim()
{
    int i, byte;
    static int  step = 0;
    static int  lst_grd_indx;
    static int  shk_cnt = 0;

    app_type::TSyncStatus* pSync;

// Timer is running at .001 seconds

    switch(step)
    {

    case 0:

        if ((app->pShm->sys_stat.PPMTimer == 1) && (shk_cnt != 0) )
            shk_cnt = 0;

        // start with shackle, shackle count
        sync_on_ticks  = 0;
        sync_off_ticks = 0;

        for (i = 0; i < (MAXSYNCS / 8); i++)
        {
            app->sync_in[i]   = 0;
            app->sync_zero[i] = 0;
        }
        app->switch_in[0]     = 0; // grade syncs

        step++;
        break;

    case 1:

        // wait off time
        sync_off_ticks++;

        if (sync_off_ticks < (unsigned) off_time) return;

        step++;
        break;

    case 2:

//----- set syncs

        shk_cnt++;

        for (i = 0; i < (MAXSYNCS / 8); i++)
        {
            app->sync_in[i]   = (unsigned char) 0xff;
        }

//----- watch the shackle number and fake a zero flag whenever the
//      signal should occur.

        for (i = 0; i < MAXSYNCS; i++)
        {
             byte = i / 8;
             pSync = &app->pShm->SyncStatus[i];

             if ( (!(BITSET(app->sync_zero[byte], i)) ) &&
                  (pSync->shackleno == app->pShm->sys_set.Shackles) )
             {
                 SETBIT(app->sync_zero[byte], i);
             }

             else
             {
                if ((BITSET(app->sync_zero[byte], i) ) &&
                     (pSync->shackleno != app->pShm->sys_set.Shackles) )
                {
                    CLRBIT(app->sync_zero[byte], i);
                }
             }

        }

//----- Do the same as above for grading
        if (grade_idx++ >= 4)
            grade_idx = 0;

        if (grade_idx < 0)
            RtPrintf("******* SIM grade problem **** idx %d\n",grade_idx );

        app->switch_in[0]     = (char)(Mask[GRADESYNCBIT] | Mask[sim_grades[grade_idx]]);

        // if time for a zero, doit
        if ( (!(BITSET(app->switch_in[0], GRADEZEROBIT))) &&
             (app->pShm->grade_shackle[0] == app->pShm->sys_set.Shackles) )
        {
            SETBIT(app->switch_in[0], GRADEZEROBIT);
        }
        else
        {
            if ( (BITSET(app->switch_in[0], GRADEZEROBIT)) &&
                 (app->pShm->grade_shackle[0] != app->pShm->sys_set.Shackles) )
            {
                CLRBIT(app->switch_in[0], GRADEZEROBIT);
            }
        }

		if(app->pShm->sys_set.MiscFeatures.EnableGradeSync2)
		{
			app->switch_in[0]     = (char)(Mask[GRADESYNC2BIT] | Mask[sim_grades[grade_idx]]);

			// if time for a zero, doit
			if ( (!(BITSET(app->switch_in[MAXGRADESYNCS - 1], GRADEZERO2BIT))) &&
				 (app->pShm->grade_shackle[MAXGRADESYNCS - 1] == app->pShm->sys_set.Shackles) )
			{
				SETBIT(app->switch_in[0], GRADEZERO2BIT);
			}
			else
			{
				if ( (BITSET(app->switch_in[MAXGRADESYNCS - 1], GRADEZERO2BIT)) &&
					 (app->pShm->grade_shackle[MAXGRADESYNCS - 1] != app->pShm->sys_set.Shackles) )
				{
					CLRBIT(app->switch_in[0], GRADEZERO2BIT);
				}
			}
		}

        step++;
        break;

    case 3:

        // turn on inputs and wait on time
        sync_on_ticks++;
        if (sync_on_ticks < (unsigned) on_time) return;
        step = 0;
        break;

    default:
        step = 0;
        break;
    }
}

//--------------------------------------------------------
//   Sim_Timer_Main: Called by RtTimer, this is a
//   timer to run the simulation engine
//--------------------------------------------------------

void __stdcall Simulator::Sim_Timer_Main(PVOID addr)
{
	(void)addr;
    sim->RunSim();
    Sleep(0);
}
