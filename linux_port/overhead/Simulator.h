// Simulator.h: interface for the Simulator class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SIMULATOR_H__33958EE3_4575_11D6_B4A0_020000000001__INCLUDED_)
#define AFX_SIMULATOR_H__33958EE3_4575_11D6_B4A0_020000000001__INCLUDED_

#pragma once

class Simulator
{

public:

    Simulator();
    virtual ~Simulator();

    void    initialize();
    int     SimWeight();
    void    RunSim();
    void    Create_Sim_Timer();
    static  void __stdcall Sim_Timer_Main(PVOID addr);
    void    CycleTimes();

};

#endif // !defined(AFX_SIMULATOR_H__33958EE3_4575_11D6_B4A0_020000000001__INCLUDED_)
