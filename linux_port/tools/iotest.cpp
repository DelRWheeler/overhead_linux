/*=============================================================================
 * iotest.cpp — standalone PCM-3724 digital OUTPUT test utility
 *
 * Purpose
 *   Exercise the PCM-3724 digital outputs with ZERO controller software in the
 *   loop, so a card/driver/wiring fault can be conclusively separated from an
 *   application fault. Nothing here shares state with the overhead controller:
 *   it initializes the card itself and drives the output latches directly.
 *
 * Hardware (mirrors linux_port/overhead/3724_io.h — board 1)
 *   Base address 0x300, two independent 8255 groups.
 *     Group 0 (INPUTS,  0x300-0x303) — sensors live here. NOT TOUCHED by this
 *                                      tool (see --init-inputs for the one
 *                                      opt-in exception, which only ever writes
 *                                      the all-inputs control word 0x9B).
 *     Group 1 (OUTPUTS, 0x304-0x307) — PORT_A1 0x304, PORT_B1 0x305,
 *                                      PORT_C1 0x306, CFG_REG_1 0x307
 *     BUFFER_DIR 0x308, GATE_CNTRL 0x309 — board-wide.
 *
 *   Outputs are ACTIVE LOW: 0xFF in a latch = all 8 channels OFF; clearing a
 *   bit ENERGIZES that channel. This matches the controller, which always
 *   writes (unsigned char)~output_byte[n] (overhead.cpp SetOutput()).
 *
 *   Output N (1..24) -> byte = (N-1)/8, bit = (N-1)&7, mask = 1<<bit
 *   (identical to overhead.cpp SetOutput() + Mask[bit & 0x7]).
 *   e.g. output 11 -> byte 1 (port 0x305), bit 2, mask 0x04, latch 0xFB.
 *
 * Build (static, no deps):
 *     g++ -O2 -static -o iotest tools/iotest.cpp
 *
 * Run: needs root for iopl(3):  sudo ./iotest --all
 *
 * Single threaded on purpose: iopl() privilege is per-thread on Linux, so a
 * background thread's outb() would be silently lost (see the note in
 * overhead.cpp ~3969). Everything here runs on the main thread.
 *===========================================================================*/

#if !defined(__i386__) && !defined(__x86_64__)
#error "iotest targets x86 port I/O (iopl/outb/inb) only"
#endif

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define IOTEST_VERSION "1.0"

/*---------------------------------------------------------------------------
 * Register map — values copied verbatim from 3724_io.h (board 1)
 *-------------------------------------------------------------------------*/
#define PCM_BASE        0x300

#define PORT_A0         (PCM_BASE + 0)   /* 0x300 input  — never written     */
#define PORT_B0         (PCM_BASE + 1)   /* 0x301 input  — never written     */
#define PORT_C0         (PCM_BASE + 2)   /* 0x302 input  — never written     */
#define CFG_REG_0       (PCM_BASE + 3)   /* 0x303 group-0 control            */
#define PORT_A1         (PCM_BASE + 4)   /* 0x304 output byte 0 (out  1.. 8) */
#define PORT_B1         (PCM_BASE + 5)   /* 0x305 output byte 1 (out  9..16) */
#define PORT_C1         (PCM_BASE + 6)   /* 0x306 output byte 2 (out 17..24) */
#define CFG_REG_1       (PCM_BASE + 7)   /* 0x307 group-1 control            */
#define BUFFER_DIR      (PCM_BASE + 8)   /* 0x308 buffer direction           */
#define GATE_CNTRL      (PCM_BASE + 9)   /* 0x309 buffer gate enable         */

#define CFG_INPUT_SEL   0x9B             /* PCM_3724_1_INPUT_SEL             */
#define CFG_OUTPUT_SEL  0x80             /* PCM_3724_1_OUTPUT_SEL            */
#define BUF_OUTPUT_DIR  0x38             /* PCM_3724_1_OUTPUT_DIR            */
#define GATE_DISABLE    0x00             /* PCM_3724_1_DISABLE               */
#define GATE_ENABLE     0xFF             /* PCM_3724_1_ENABLE                */

#define ALL_OFF         0xFF             /* active low: every channel OFF    */

#define NUM_OUT_PORTS   3
#define MAX_OUTPUT      24               /* 3 ports x 8 bits                 */

static const unsigned short g_out_port[NUM_OUT_PORTS] = { PORT_A1, PORT_B1, PORT_C1 };

/*---------------------------------------------------------------------------
 * Global state touched by signal handlers — keep it trivially safe
 *-------------------------------------------------------------------------*/
static volatile sig_atomic_t g_io_ready      = 0;  /* iopl(3) succeeded      */
static volatile sig_atomic_t g_outputs_armed = 0;  /* we own the output side */
static volatile sig_atomic_t g_released      = 0;  /* all-off already done   */

static unsigned char g_latch[NUM_OUT_PORTS] = { ALL_OFF, ALL_OFF, ALL_OFF };

static int    g_verify_errors  = 0;
static int    g_outputs_fired  = 0;
static int    g_readback       = 0;

/*---------------------------------------------------------------------------
 * Timestamped logging
 *-------------------------------------------------------------------------*/
static void logline(const char *fmt, ...)
{
    struct timespec now;
    struct tm       tmv;
    char            stamp[32];

    clock_gettime(CLOCK_REALTIME, &now);
    localtime_r(&now.tv_sec, &tmv);
    snprintf(stamp, sizeof(stamp), "%02d:%02d:%02d.%03ld",
             tmv.tm_hour, tmv.tm_min, tmv.tm_sec, now.tv_nsec / 1000000L);

    printf("[%s] ", stamp);

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    printf("\n");
    fflush(stdout);
}

/*---------------------------------------------------------------------------
 * Raw port helpers
 *-------------------------------------------------------------------------*/
static inline void wr(unsigned short port, unsigned char val) { outb(val, port); }
static inline unsigned char rd(unsigned short port)           { return inb(port); }

/* Write a latch from the shadow copy, optionally verifying the read-back.
 * An 8255 port programmed as an output returns its latch when read, so a
 * mismatch means the write did not stick (missing/dead card, wrong base
 * address, gates/config wrong, or a second writer on the bus). */
static void write_latch(int idx, int force_verify)
{
    wr(g_out_port[idx], g_latch[idx]);

    if (g_readback || force_verify) {
        unsigned char got = rd(g_out_port[idx]);
        if (got == g_latch[idx]) {
            logline("  readback latch 0x%03X = %02X (verify OK)",
                    g_out_port[idx], got);
        } else {
            g_verify_errors++;
            logline("  *** READBACK MISMATCH latch 0x%03X: wrote %02X, read %02X ***",
                    g_out_port[idx], g_latch[idx], got);
            logline("  *** card may be absent/failed, base addr wrong, or another writer is active ***");
        }
    }
}

/*---------------------------------------------------------------------------
 * Release everything — the one routine every exit path funnels through.
 * Must be safe to call from a signal handler: outb is a single instruction and
 * write(2) is async-signal-safe. No printf here.
 *-------------------------------------------------------------------------*/
static void release_all_outputs(void)
{
    if (!g_io_ready || !g_outputs_armed) return;

    g_latch[0] = g_latch[1] = g_latch[2] = ALL_OFF;
    outb(ALL_OFF, PORT_A1);
    outb(ALL_OFF, PORT_B1);
    outb(ALL_OFF, PORT_C1);
    g_released = 1;
}

static void atexit_release(void)
{
    release_all_outputs();
}

static void signal_release(int sig)
{
    static const char msg[] = "\n*** signal caught: all outputs released (FF/FF/FF) ***\n";
    release_all_outputs();
    ssize_t ignored = write(STDERR_FILENO, msg, sizeof(msg) - 1);
    (void)ignored;
    (void)sig;
    _exit(130);
}

static void install_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_release;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;                 /* no SA_RESTART: interrupt the dwell   */

    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP,  &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    /* Fatal faults too — a crash must not leave a drop solenoid energized. */
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS,  &sa, NULL);
    sigaction(SIGFPE,  &sa, NULL);
    sigaction(SIGILL,  &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
}

/*---------------------------------------------------------------------------
 * Sleep helper (float seconds, EINTR-tolerant)
 *-------------------------------------------------------------------------*/
static void sleep_secs(double secs)
{
    if (secs <= 0.0) return;

    struct timespec req;
    req.tv_sec  = (time_t)secs;
    req.tv_nsec = (long)((secs - (double)req.tv_sec) * 1e9);
    if (req.tv_nsec < 0)          req.tv_nsec = 0;
    if (req.tv_nsec > 999999999L) req.tv_nsec = 999999999L;

    while (nanosleep(&req, &req) == -1 && errno == EINTR)
        ;   /* signal handlers _exit(), so this only resumes on benign wakeups */
}

/*---------------------------------------------------------------------------
 * Controller-running guard — native equivalent of `pgrep -x overhead`
 * (no fork/exec, so it works on a minimal box with no procps installed).
 *-------------------------------------------------------------------------*/
static pid_t find_controller(void)
{
    DIR *d = opendir("/proc");
    if (!d) return 0;

    pid_t self = getpid();
    pid_t hit  = 0;
    struct dirent *e;

    while ((e = readdir(d)) != NULL) {
        const char *p = e->d_name;
        int digits = 1;
        for (const char *q = p; *q; q++) {
            if (!isdigit((unsigned char)*q)) { digits = 0; break; }
        }
        if (!digits) continue;

        pid_t pid = (pid_t)atoi(p);
        if (pid <= 0 || pid == self) continue;

        char path[64], comm[64];
        snprintf(path, sizeof(path), "/proc/%d/comm", (int)pid);

        FILE *f = fopen(path, "r");
        if (f) {
            if (fgets(comm, sizeof(comm), f)) {
                char *nl = strchr(comm, '\n');
                if (nl) *nl = '\0';
                if (strcmp(comm, "overhead") == 0) { fclose(f); hit = pid; break; }
            }
            fclose(f);
        }

        /* Fallback: renamed/wrapped process — check the exe basename. */
        snprintf(path, sizeof(path), "/proc/%d/exe", (int)pid);
        char target[PATH_MAX];
        ssize_t n = readlink(path, target, sizeof(target) - 1);
        if (n > 0) {
            target[n] = '\0';
            const char *base = strrchr(target, '/');
            base = base ? base + 1 : target;
            if (strcmp(base, "overhead") == 0) { hit = pid; break; }
        }
    }

    closedir(d);
    return hit;
}

/*---------------------------------------------------------------------------
 * Card initialization
 *
 * Production (3724_io.cpp io_3724::initialize()) does, in order:
 *     GATE_CNTRL  <= 0x00   disable all gates
 *     CFG_REG_0   <= 0x9B   group 0 all inputs
 *     CFG_REG_1   <= 0x80   group 1 all outputs
 *     BUFFER_DIR  <= 0x38   A0/B0/C0 in, A1/B1/C1 out
 *     GATE_CNTRL  <= 0xFF   re-enable all gates
 *     PORT_A1/B1/C1 <= ~output_byte[] (0xFF at startup = all off)
 *
 * DELIBERATE DEVIATION (safety, as briefed): the 8255 mode-set write to
 * CFG_REG_1 CLEARS the output latches to 0x00, which on this active-low wiring
 * means "all 24 channels energized". Production then opens the gates BEFORE
 * writing 0xFF, so the field sees an all-on glitch for the duration of those
 * two I/O writes. This tool writes the all-off pattern to the data latches
 * FIRST and enables the gates LAST, so nothing is ever driven except the
 * intended channel.
 *-------------------------------------------------------------------------*/
static void init_card(int init_inputs)
{
    logline("init: GATE_CNTRL  (0x%03X) <= 0x%02X  (all buffer gates DISABLED — field isolated)",
            GATE_CNTRL, GATE_DISABLE);
    wr(GATE_CNTRL, GATE_DISABLE);

    if (init_inputs) {
        logline("init: CFG_REG_0   (0x%03X) <= 0x%02X  (group 0 all INPUTS — opt-in, --init-inputs)",
                CFG_REG_0, CFG_INPUT_SEL);
        wr(CFG_REG_0, CFG_INPUT_SEL);
    } else {
        logline("init: CFG_REG_0   (0x%03X) skipped — first 8255 group (inputs) left untouched",
                CFG_REG_0);
    }

    logline("init: CFG_REG_1   (0x%03X) <= 0x%02X  (group 1 all OUTPUTS; mode-set zeroes latches)",
            CFG_REG_1, CFG_OUTPUT_SEL);
    wr(CFG_REG_1, CFG_OUTPUT_SEL);

    logline("init: BUFFER_DIR  (0x%03X) <= 0x%02X  (A0/B0/C0 in, A1/B1/C1 out)",
            BUFFER_DIR, BUF_OUTPUT_DIR);
    wr(BUFFER_DIR, BUF_OUTPUT_DIR);

    /* SAFE ORDER: data latches to all-off BEFORE the gates open. */
    g_latch[0] = g_latch[1] = g_latch[2] = ALL_OFF;
    wr(PORT_A1, ALL_OFF);
    wr(PORT_B1, ALL_OFF);
    wr(PORT_C1, ALL_OFF);
    g_outputs_armed = 1;
    logline("init: latches 0x%03X/0x%03X/0x%03X <= FF/FF/FF (all OFF) — written BEFORE gate enable",
            PORT_A1, PORT_B1, PORT_C1);

    logline("init: GATE_CNTRL  (0x%03X) <= 0x%02X  (gates ENABLED, driving all-off pattern)",
            GATE_CNTRL, GATE_ENABLE);
    wr(GATE_CNTRL, GATE_ENABLE);

    /* Belt and braces: re-assert all-off now that the buffers are live. */
    wr(PORT_A1, ALL_OFF);
    wr(PORT_B1, ALL_OFF);
    wr(PORT_C1, ALL_OFF);

    logline("init: complete — card ready, all 24 outputs OFF");
}

/*---------------------------------------------------------------------------
 * Fire one output for the dwell time, then release it
 *-------------------------------------------------------------------------*/
static void fire_output(int n, double dwell)
{
    int idx  = (n - 1) / 8;
    int bit  = (n - 1) & 7;
    unsigned char mask = (unsigned char)(1u << bit);

    g_latch[idx] = (unsigned char)(g_latch[idx] & ~mask);
    logline("output %2d ON  (port 0x%03X bit %d, mask %02X, latch <= %02X)",
            n, g_out_port[idx], bit, mask, g_latch[idx]);
    write_latch(idx, 0);
    g_outputs_fired++;

    sleep_secs(dwell);

    g_latch[idx] = (unsigned char)(g_latch[idx] | mask);
    logline("output %2d OFF (port 0x%03X bit %d, latch <= %02X)",
            n, g_out_port[idx], bit, g_latch[idx]);
    write_latch(idx, 0);
}

/*---------------------------------------------------------------------------
 * --status: read-only. Never writes anything, so it is the only mode that is
 * safe to run while the controller owns the card.
 *-------------------------------------------------------------------------*/
static void print_status(void)
{
    unsigned char a = rd(PORT_A1);
    unsigned char b = rd(PORT_B1);
    unsigned char c = rd(PORT_C1);

    logline("status: latch 0x%03X = %02X", PORT_A1, a);
    logline("status: latch 0x%03X = %02X", PORT_B1, b);
    logline("status: latch 0x%03X = %02X", PORT_C1, c);

    unsigned char v[NUM_OUT_PORTS] = { a, b, c };
    char list[256];
    int  len = 0, energized = 0;
    list[0] = '\0';

    for (int n = 1; n <= MAX_OUTPUT; n++) {
        int idx = (n - 1) / 8;
        int bit = (n - 1) & 7;
        if (!(v[idx] & (1u << bit))) {          /* active low: 0 = energized */
            energized++;
            len += snprintf(list + len, sizeof(list) - (size_t)len,
                            "%s%d", (energized > 1 ? "," : ""), n);
            if (len >= (int)sizeof(list) - 8) break;
        }
    }

    if (energized == 0)
        logline("status: no outputs energized (idle) — %02X/%02X/%02X", a, b, c);
    else
        logline("status: %d output(s) ENERGIZED: %s", energized, list);

    logline("status: read-only mode — nothing was written");
}

/*---------------------------------------------------------------------------
 * Usage
 *-------------------------------------------------------------------------*/
static void usage(const char *argv0)
{
    printf(
"iotest %s — standalone PCM-3724 digital output test (base 0x%03X, outputs 1..%d)\n"
"\n"
"Exercises the PCM-3724 outputs with no controller software involved, to split\n"
"card/driver/wiring faults from application faults. Needs root (iopl).\n"
"\n"
"USAGE\n"
"  sudo %s --all [options]\n"
"  sudo %s --drop N [options]\n"
"  sudo %s --status\n"
"\n"
"MODES\n"
"  -a, --all            Sweep outputs 1..%d in order, dwelling on each.\n"
"  -d, --drop N         Fire a single output N (1..%d).\n"
"      --status         Read and print the three output latches, then exit.\n"
"                       Read-only: writes nothing, safe with controller running.\n"
"\n"
"OPTIONS\n"
"  -t, --time SECS      Hold/dwell time per output, float OK (default 2.0).\n"
"  -r, --repeat K       Repeat the fire/sweep K times (default 1).\n"
"      --readback       Read each port back after every write and verify the\n"
"                       latch. Mismatches are flagged and set exit code 2.\n"
"      --gap SECS       Off-time between outputs/repeats (default 0.3).\n"
"      --force          Run even if the 'overhead' controller process is alive.\n"
"                       DANGEROUS: two writers on one card.\n"
"      --init-inputs    Also write CFG_REG_0 (0x%03X) <= 0x%02X, i.e. force the\n"
"                       first 8255 group to all-inputs exactly as the controller\n"
"                       does. Off by default so the input group is not touched.\n"
"  -h, --help           This help.\n"
"\n"
"SAFETY\n"
"  * Refuses to run (except --status) while 'overhead' is running.\n"
"  * Outputs are ACTIVE LOW: latch FF = all off, a cleared bit energizes.\n"
"  * Every exit path — normal, error, SIGINT/SIGTERM, crash — writes FF to\n"
"    0x%03X/0x%03X/0x%03X before exiting.\n"
"  * The first 8255 group (0x300-0x303, inputs) is never written unless you\n"
"    pass --init-inputs, and even then only the all-inputs value 0x%02X.\n"
"\n"
"EXAMPLES\n"
"  sudo ./iotest -a                       sweep all 24, 2 s each\n"
"  sudo ./iotest -d 11 -t 5 --readback    hold output 11 (0x305 bit 2) 5 s, verify\n"
"  sudo ./iotest -a -t 0.5 -r 3           three fast sweeps\n"
"  sudo ./iotest --status                 what is energized right now\n"
"\n"
"EXIT CODES\n"
"  0 OK   1 usage/permission/refused   2 readback mismatch\n",
    IOTEST_VERSION, PCM_BASE, MAX_OUTPUT,
    argv0, argv0, argv0,
    MAX_OUTPUT, MAX_OUTPUT,
    CFG_REG_0, CFG_INPUT_SEL,
    PORT_A1, PORT_B1, PORT_C1,
    CFG_INPUT_SEL);
}

/*---------------------------------------------------------------------------
 * main
 *-------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
    int    do_all      = 0;
    int    do_status   = 0;
    int    drop        = 0;
    int    repeat      = 1;
    int    force       = 0;
    int    init_inputs = 0;
    double dwell       = 2.0;
    double gap         = 0.3;

    static struct option longopts[] = {
        { "all",         no_argument,       NULL, 'a' },
        { "drop",        required_argument, NULL, 'd' },
        { "time",        required_argument, NULL, 't' },
        { "repeat",      required_argument, NULL, 'r' },
        { "readback",    no_argument,       NULL, 1000 },
        { "status",      no_argument,       NULL, 1001 },
        { "force",       no_argument,       NULL, 1002 },
        { "gap",         required_argument, NULL, 1003 },
        { "init-inputs", no_argument,       NULL, 1004 },
        { "help",        no_argument,       NULL, 'h' },
        { NULL, 0, NULL, 0 }
    };

    int c;
    while ((c = getopt_long(argc, argv, "ad:t:r:h", longopts, NULL)) != -1) {
        switch (c) {
        case 'a':  do_all = 1; break;
        case 'd':
            drop = atoi(optarg);
            if (drop < 1 || drop > MAX_OUTPUT) {
                fprintf(stderr, "iotest: --drop must be 1..%d (got '%s')\n", MAX_OUTPUT, optarg);
                return 1;
            }
            break;
        case 't':
            dwell = atof(optarg);
            if (dwell <= 0.0 || dwell > 600.0) {
                fprintf(stderr, "iotest: --time must be > 0 and <= 600 seconds (got '%s')\n", optarg);
                return 1;
            }
            break;
        case 'r':
            repeat = atoi(optarg);
            if (repeat < 1 || repeat > 10000) {
                fprintf(stderr, "iotest: --repeat must be 1..10000 (got '%s')\n", optarg);
                return 1;
            }
            break;
        case 1000: g_readback  = 1; break;
        case 1001: do_status   = 1; break;
        case 1002: force       = 1; break;
        case 1003:
            gap = atof(optarg);
            if (gap < 0.0 || gap > 60.0) {
                fprintf(stderr, "iotest: --gap must be 0..60 seconds (got '%s')\n", optarg);
                return 1;
            }
            break;
        case 1004: init_inputs = 1; break;
        case 'h':  usage(argv[0]); return 0;
        default:   usage(argv[0]); return 1;
        }
    }

    if (do_status && (do_all || drop)) {
        fprintf(stderr, "iotest: --status cannot be combined with --all/--drop\n");
        return 1;
    }
    if (do_all && drop) {
        fprintf(stderr, "iotest: choose --all or --drop, not both\n");
        return 1;
    }
    if (!do_all && !drop && !do_status) {
        usage(argv[0]);
        return 1;
    }

    logline("iotest %s — PCM-3724 output test, base 0x%03X, outputs 1..%d (ACTIVE LOW)",
            IOTEST_VERSION, PCM_BASE, MAX_OUTPUT);

    if (geteuid() != 0) {
        fprintf(stderr, "iotest: must run as root for port I/O — try: sudo %s ...\n", argv[0]);
        return 1;
    }

    /*-- SAFETY GATE: two writers on one card corrupts both -----------------*/
    pid_t ctrl = find_controller();
    if (ctrl != 0) {
        if (do_status) {
            logline("NOTE: controller 'overhead' is RUNNING (pid %d) — --status is read-only, continuing",
                    (int)ctrl);
        } else if (!force) {
            logline("REFUSING TO RUN: controller 'overhead' is RUNNING (pid %d).",
                    (int)ctrl);
            logline("  Two writers on one PCM-3724 fight over the same output latches:");
            logline("  this tool's writes would be overwritten by the controller's output");
            logline("  scan, and this tool would clobber live drop firing. Stop the");
            logline("  controller first, then re-run.  Override with --force if you");
            logline("  really mean it. Use --status for a safe read-only look.");
            return 1;
        } else {
            logline("WARNING: --force given while controller 'overhead' is RUNNING (pid %d).",
                    (int)ctrl);
            logline("WARNING: both processes are now writing the same card. Drops may fire.");
        }
    } else {
        logline("safety: no 'overhead' controller process running — OK to drive outputs");
    }

    /*-- Port privilege ------------------------------------------------------
     * iopl(3) rather than ioperm(): matches the controller (platform.h
     * RtEnablePortIo) and survives exec. Per-thread, hence single threaded. */
    if (iopl(3) != 0) {
        fprintf(stderr, "iotest: iopl(3) failed: %s (root? kernel CONFIG_X86_IOPL_IOPERM?)\n",
                strerror(errno));
        return 1;
    }
    g_io_ready = 1;
    logline("port I/O access enabled (iopl 3)");

    /* Handlers + atexit installed BEFORE the first output write. */
    install_handlers();
    atexit(atexit_release);

    if (do_status) {
        print_status();
        return 0;                    /* read-only: nothing armed, nothing written */
    }

    init_card(init_inputs);

    if (do_all) {
        logline("MODE: sweep outputs 1..%d, dwell %.3f s, gap %.3f s, %d pass(es)%s",
                MAX_OUTPUT, dwell, gap, repeat, g_readback ? ", readback ON" : "");
        for (int pass = 1; pass <= repeat; pass++) {
            if (repeat > 1) logline("--- pass %d of %d ---", pass, repeat);
            for (int n = 1; n <= MAX_OUTPUT; n++) {
                fire_output(n, dwell);
                if (n < MAX_OUTPUT || pass < repeat) sleep_secs(gap);
            }
        }
    } else {
        logline("MODE: single output %d, dwell %.3f s, %d repeat(s)%s",
                drop, dwell, repeat, g_readback ? ", readback ON" : "");
        for (int pass = 1; pass <= repeat; pass++) {
            if (repeat > 1) logline("--- repeat %d of %d ---", pass, repeat);
            fire_output(drop, dwell);
            if (pass < repeat) sleep_secs(gap);
        }
    }

    /*-- Release + verify idle ---------------------------------------------*/
    release_all_outputs();

    unsigned char a = rd(PORT_A1), b = rd(PORT_B1), cc = rd(PORT_C1);
    int idle = (a == ALL_OFF && b == ALL_OFF && cc == ALL_OFF);

    logline("SUMMARY: %d output%s exercised, all released, latches idle %02X/%02X/%02X%s",
            g_outputs_fired, (g_outputs_fired == 1 ? "" : "s"), a, b, cc,
            idle ? "" : "   *** NOT IDLE — CHECK CARD ***");

    if (!idle) {
        logline("*** WARNING: latches did not read back FF/FF/FF after release ***");
        g_verify_errors++;
    }

    if (g_verify_errors > 0) {
        logline("RESULT: FAIL — %d readback/verify error(s)", g_verify_errors);
        return 2;
    }

    logline("RESULT: PASS — no verify errors");
    return 0;
}
