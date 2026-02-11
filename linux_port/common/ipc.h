#pragma once
#include "types.h"

// IPC Message types

// ipc/pipe
#define MAX_IPC_MSG_SIZE   256
#define MAX_APP_MSG_SIZE   50000
#define PIPE_TIMEOUT       500

////////////////////////////////////////////////////////////////////////
//                    may be used by all
////////////////////////////////////////////////////////////////////////

// for ipc with interface
#define SEND_SHM            "Tx_Shm"
#define SEND_MUTEX          "Tx_Mutex"
#define SEND_SEM_POST       "Tx_SemPost"
#define SEND_SEM_ACK        "Tx_SemAck"

#define SEND_HST_SHM        "HstTx_Shm"
#define SEND_HST_MUTEX      "HstTx_Mutex"
#define SEND_HST_SEM_POST   "HstTx_SemPost"
#define SEND_HST_SEM_ACK    "HstTx_SemAck"

#define RECV_SHM            "Rx_Shm"
#define RECV_MUTEX          "Rx_Mutex"
#define RECV_SEM_POST       "Rx_SemPost"
#define RECV_SEM_ACK        "Rx_SemAck"

// for ipc with app
#define APP_SHM             "App_Msg_Shm"
#define APP_MUTEX           "App_Msg_Mutex"
#define APP_SEM_POST        "App_Msg_SemPost"
#define APP_SEM_ACK         "App_Msg_SemAck"

// for ipc with drop manager
#define DRPMGR_SHM          "DM_Msg_Shm"
#define DRPMGR_MUTEX        "DM_Msg_Mutex"
#define DRPMGR_SEM_POST     "DM_Msg_SemPost"
#define DRPMGR_SEM_ACK      "DM_Msg_SemAck"

// for ipc with intersystems
#define ISYS_SHM            "Isys_Msg_Shm"
#define ISYS_MUTEX          "Isys_Msg_Mutex"
#define ISYS_SEM_POST       "Isys_Msg_SemPost"
#define ISYS_SEM_ACK        "Isys_Msg_SemAck"

// general mailbox structure
typedef struct {
    LONG    Ack;        // Server acknowledge flag
    CHAR    Buffer[MAX_IPC_MSG_SIZE];
} IPCMSG, *PIPCMSG;

// shorter ipc messages with interface/intersystems
typedef struct {
    LONG    Ack;        // Server acknowledge flag
    LONG    LineId;
    LONG    Len;
    CHAR    Buffer[MAX_IPC_MSG_SIZE];
} MSGSTR, *PMSGSTR;

// longer ipc messages with interface/app
typedef struct {
    LONG    Ack;        // Server acknowledge flag
    LONG    LineId;
    LONG    Len;
    CHAR    Buffer[MAX_APP_MSG_SIZE];
} APPIPCMSG, *PAPPIPCMSG;

// Message header, anything related to network messages
typedef struct{
    ULONG    cs;
    int      destLineId; // checksum starts here and includes all data after the header
    int      srcLineId;
    int      cmd;
    int      flags;
    int      len;    // length of data after the header
    UINT     seq_num;
} mhdr;

// Verify mhdr matches the Go API and Windows ABI (28 bytes)
static_assert(sizeof(mhdr) == 28, "mhdr must be 28 bytes for network protocol compatibility");

// !!!!IMPORTANT!!!!!!!!!!!IMPORTANT!!!!!!!!!!IMPORTANT!!!!!!!!!!!IMPORTANT!!!!!!

// Below are descriptions for ipc messages at the top of overhead.cpp.
// It translates the message id into a description.

// Used with the description strings (isys_msg_desc,dmgr_msg,drp_state_desc,dist_mode_desc,...)
#define MAX_DBG_DESC        16

// If any messages are added to the enum section 
// below, remember to change related text!!!!!!!!!

extern char isys_msg_desc  [] [MAX_DBG_DESC];
extern char app_msg        [] [MAX_DBG_DESC];
extern char dmgr_msg       [] [MAX_DBG_DESC];
extern char drp_state_desc [] [MAX_DBG_DESC];

// intersystems messages between machines
enum {
    NUM_ISYS_CMDS  = 5,   // used to indicate the number of commands + dummy
    ISYS_MSG_START = 100, // used to sub from msg# for desc strings and valid message check
    ISYS_ACK,             // actual message ids
    ISYS_PORTCHK,
    ISYS_DROP_MAINT,
    ISYS_DROPMSG,
    LAST_ISYS_MSG         // used for valid message check
};
// messages for drop manager
enum {
    DRPMGR_MSG_START = 200,
    DROP_MAINT,
    DROP_MAINT_RESP,
    DROP_MAINT_FAIL,
    CHECK_DROP_LOCAL,
    CHECK_DROP_REMOTE,
    LAST_DRPMGR_MSG
};
// messages for overhead app
enum {
    APP_MSG_START = 300,
    SHM_READ,   // if initiated by remote, host responds with SHM_WRITE.
    SHM_WRITE,
    DREC_RESP,
    BLK_DREC_REQ,
    BLK_DREC_RESP,
    ERROR_MSG,
    DEBUG_MSG,
    DREC_MSG,
    ACK_MSG,
    PRN_BCH_REQ,
    PRN_BCH_RESP,
    SET_DATE_TIME,
    RESTART_APP,
    SHUTDOWN_NT,
    RESTART_NT,
    RESET_BCH_NUMS,
    IND_MODE_WT_LIM,
    AUTO_BIAS_MODE,
    CLEAR_BATCH_RECS,
    PRN_BCH_PRE_REQ,
    PRN_BCH_PRE_RSP,
    CLEAR_TOTS,
    CLEAR_MAN_BATCH,
    HOST_HEARTBEAT,
    SLAVE_PRE_LABEL,
    SET_LC_CAPTURE,
    LC_CAPTURE_INFO,
	MASTER_STOPPED,				//Master line has stopped, going inactive
	MASTER_STARTED,				//Master line is running again, retake mastership
	LAST_BIRD_CLEARED,			//	Sent between lines when they share an InterSystem drop and the last bird for a batch arrives at the drop
	REMOTE_BATCH_RESET,			//	Sent between lines when they share an InterSystem drop and the remote batch reset button is pressed on one of them
    LAST_APP_MSG
};

// events for overhead app
enum {
    EVT_SHM_READ = 0,
    EVT_DREC_RESP,
    // the rest for debugging
    EVT_SND_DREC_RESP,
    NUM_EVT
};

// !!!!IMPORTANT!!!!!!!!!!!IMPORTANT!!!!!!!!!!IMPORTANT!!!!!!!!!!!IMPORTANT!!!!!!

// trace buffers
typedef char     trcbuf        [MAXTRCMBUFSIZE];
typedef char     tmptrcbuf     [MAXTRCMBUFSIZE/2];
// controls sending of trace buffers
typedef struct
{
    bool         send_OK;     // ok to send
    bool         buffer_full; // ok to send
    UINT         len;         // length of message
    HANDLE       mutex;       // critical section mutex handle array
} trace_ctrl;

////////////////////////////////////////////////////////////////////////
//                    Overhead
////////////////////////////////////////////////////////////////////////

typedef struct{      // generic 
    mhdr       hdr;
    char       data[1];
} app_gen_rw_msg;

typedef struct{
    mhdr       hdr;
    int        shm_id;
    char       data[1]; // variable length
} host_rw_shm_id;

typedef struct{
    mhdr       hdr;
    int        severity;
    char       text[1]; // variable length
} app_error_msg;

typedef struct{
    UINT       seq_num;
    app_type::THostDropRec d_rec;
} app_drp_rec;

typedef struct{
    mhdr        hdr;
    app_drp_rec drec[1]; // variable length
} app_drp_msg;

typedef struct{ 
    mhdr       hdr;
    UINT       seq_num[1]; // variable length
} host_drp_resp;

// format to event handler
typedef struct{ 
    int        cnt;
    UINT       seq_num[1]; // variable length
} ev_app_drp_resp;

union TAppIpcMsg
{
    app_gen_rw_msg gen;
    host_rw_shm_id rw_shm;
    app_error_msg  errMsg;
    app_drp_msg    drpMsg;
    host_drp_resp  drpResp;
};

////////////////////////////////////////////////////////////////////////
//                    Drop Manager
////////////////////////////////////////////////////////////////////////

typedef struct{
    int        cmd;
    int        drop_num;
    int        flags;
    int        action;
    int        slave_indx; // the index for line id
    int        extent;     // effect on drops
    UINT       batch;
} maint_msg;

typedef struct{
    int        cmd;
    int        drop_num;
    int        nothing;   // In overhead::SendDrpManager uses maint msg format, but flags
    int        shackle;   // is not used. Use action for shackle and slave_indx for scale
    int        scale;
} chk_drp_msg;

typedef struct{
    int        cmd;
    UINT       seq_num;
} remove_msg;

union TDrpMgrIpcMsg
{
    maint_msg      maint;
    remove_msg     rem_msg;
    chk_drp_msg    ck_drp;
};

typedef struct{
    unsigned flags;
    int      cmd_tmr;
    int      cmd;         // Send from here down to intersystems.
    int      lineId;      // Note: sizeof(int)*7 in MaintainMsgQ
    int      drop_num;    // should match tMaintReqMsg from flags down
    int      flags1;
    int      action;
    UINT     seq_num;
    UINT     batch;
} TDrpMgrMsgQEntry;

////////////////////////////////////////////////////////////////////////
//                    Intersystems
////////////////////////////////////////////////////////////////////////

typedef struct{
    int      cmd;
    int      lineId;
    int      drop_num;  // Send from here down to intersystems.
    int      flags;     // Note: sizeof(int)*7 in MaintainMsgQ
    int      action;    // Beware!
    UINT     seq_num;
    UINT     batch;
} isys_msg;

union TIsysIpcMsg
{
    isys_msg   imsg;
};

//----- Queue maintenience

typedef struct{
    int      flags;
    int      cmd_tmr;
    int      retries;
} q_wrt_hdr;

//----- Maintenience message details

typedef struct{
    int      drop_num;
    int      flags;
    int      action;
    UINT     seq_num;
    UINT     batch;
} tMaintReqMsg;

typedef struct{
    int      cmd;
    UINT     seq_num;
    UINT     dm_seq_num;
    int      drop_num;
    UINT     batch;
    int      action;
} tMaintRespMsg;

// Maintenance message Q entry

typedef struct{
    q_wrt_hdr qh;
    mhdr      mh;
    tMaintReqMsg msgdata;
} qmaint_msg;

typedef struct{
    UINT      flags;
    mhdr      mh;
    tMaintReqMsg msgdata;
} rxq_maint_msg;

// What we will receive

typedef struct{
    mhdr      mh;
    tMaintReqMsg msgdata;
} rx_maint_msg;

//----- Generic message, used where message type is undetermined

typedef struct{
    int      idata1;
    int      idata2;
    int      idata3;
    int      data4;
    __int64  ddata1;
    __int64  ddata2;
} tGenericMsg;

typedef struct{
    mhdr         mh;
    tGenericMsg  msgdata;
} rx_gen_msg;

typedef struct{
    UINT         flags;
    mhdr         mh;
    tGenericMsg  msgdata;
} rxq_gen_msg;

typedef struct{
    q_wrt_hdr    qh;
    mhdr         mh;
    tGenericMsg  msgdata;
} qgen_msg;

//----- Drop message details

typedef struct{
    int      drop_num;
    int      flags;
    int      PPMCount;
    int      PPMPrevious;
    __int64  BCount;
    __int64  BWeight;
    UINT     batch_number;
} tDrpReqMsg;

// What to respond with

typedef struct{
    int         cmd;
    UINT        seq_num;  // always 1st
} tDrpRespMsg;

// Maintenance message Q entry

typedef struct{
    q_wrt_hdr   qh;
    mhdr        mh;
    tDrpReqMsg  msgdata;
} qdrp_msg;

typedef struct{
    UINT        flags;
    mhdr        mh;
    tDrpReqMsg  msgdata;
} rxq_drp_msg;

// What we will receive

typedef struct{
    mhdr        mh;
    tDrpReqMsg  msgdata;
} rx_drp_msg;

//----- Port Check message details

typedef struct{
    int         chkid;
} tPchkReqMsg;

typedef struct{
    int         cmd;
    UINT        seq_num;  // always 1st
} tPchkRespMsg;

// Port check message Q entry

typedef struct{
    q_wrt_hdr   qh;
    mhdr        mh;
    tPchkReqMsg msgdata;
} qpchk_msg;

typedef struct{
    UINT        flags;
    mhdr        mh;
    tPchkReqMsg msgdata;
} rxq_pchk_msg;

// What we will receive

typedef struct{
    mhdr        mh;
    tPchkReqMsg msgdata;
} rx_pchk_msg;

//----- UNIONS

// The queue entries

union uWQentry {
     qgen_msg    gen;
     qdrp_msg    drp;
     qmaint_msg  maint;
     qpchk_msg   ptck;
};

// The received messages

union uRx {
     rx_gen_msg    gen;
     rx_drp_msg    drp;
     rx_maint_msg  maint;
     rx_pchk_msg   ptck;
};

union uRQentry {
     rxq_gen_msg    gen;
     rxq_drp_msg    drp;
     rxq_maint_msg  maint;
     rxq_pchk_msg   ptck;
};

// The response messages

union uResp {
     tDrpRespMsg    drp;
     tMaintRespMsg  maint;
     tPchkRespMsg   ptck;
};
