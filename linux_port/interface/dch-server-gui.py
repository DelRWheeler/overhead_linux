#!/usr/bin/env python3
"""
DCH Server GUI Console - GTK3 + VTE terminal wrapper

Spawns the Interface process inside a VTE terminal widget so all
ANSI escape codes (colors, cursor positioning, screen clearing)
render natively. The Interface process in turn spawns the Overhead
backend via fork+exec.

Closing the window sends SIGTERM to the interface process, which
triggers its clean shutdown handler (stopping overhead first).
"""

import os
import sys
import signal
import struct
import subprocess
from datetime import datetime

import gi
gi.require_version('Gtk', '3.0')
gi.require_version('Vte', '2.91')
from gi.repository import Gtk, Vte, GLib, Gdk, Pango

INTERFACE_DIR = os.path.dirname(os.path.abspath(__file__))
INTERFACE_BIN = os.path.expanduser('~/dchservices/bin/interface')
LOG_DIR = os.path.expanduser('~/dchservices/logs')
PIDFILE = os.path.expanduser('~/dchservices/dch-server.pid')

# Fall back to build directory if dchservices copy doesn't exist
if not os.path.isfile(INTERFACE_BIN):
    INTERFACE_BIN = os.path.join(INTERFACE_DIR, 'build', 'interface')

# Settings file paths (match overheadconst.h)
SETTINGS_DIR = os.path.expanduser('~/dchservices/data/settings')
DATA_DIR = os.path.expanduser('~/dchservices/data')

SETTINGS_FILES = {
    'System Settings':   os.path.join(SETTINGS_DIR, 'system.bin'),
    'Scale Settings':    os.path.join(SETTINGS_DIR, 'scale.bin'),
    'Shackle Tares':     os.path.join(SETTINGS_DIR, 'tares.bin'),
    'Schedule Settings': os.path.join(SETTINGS_DIR, 'schedule.bin'),
    'Totals':            os.path.join(DATA_DIR, 'totals.dat'),
    'Drop Record Info':  os.path.join(DATA_DIR, 'droprecs', 'drecinfo.dat'),
}

# Constants from overheadconst.h
MAXSCALES = 2
MAXSYNCS = 16
MAXDROPS = 32
MAXGRADES = 5
MAXPENDANT = 2000
MAXIISYSLINES = 4
MAXIISYSNAME = 64
MAXLOADCELLS = 2


def kill_existing_instance():
    """If a previous DCH Server instance is running, stop it and its children."""
    import time
    import glob
    import socket

    my_pid = os.getpid()
    killed_something = False

    # Method 1: Kill by pidfile
    if os.path.isfile(PIDFILE):
        try:
            with open(PIDFILE) as f:
                old_pid = int(f.read().strip())
            if old_pid != my_pid:
                os.kill(old_pid, signal.SIGTERM)
                print(f"Stopping existing DCH Server (PID {old_pid})...")
                killed_something = True
        except (ValueError, OSError, ProcessLookupError):
            pass
        try:
            os.remove(PIDFILE)
        except OSError:
            pass

    # Method 2: Find any other dch-server-gui.py processes by scanning /proc
    for entry in os.listdir('/proc'):
        if not entry.isdigit():
            continue
        pid = int(entry)
        if pid == my_pid:
            continue
        try:
            with open(f'/proc/{pid}/cmdline', 'rb') as f:
                cmdline = f.read().decode('utf-8', errors='replace')
            if 'dch-server-gui.py' in cmdline:
                print(f"Killing old DCH Server process (PID {pid})...")
                os.kill(pid, signal.SIGTERM)
                killed_something = True
        except (OSError, ProcessLookupError):
            continue

    if killed_something:
        # Wait for SIGTERM to take effect
        time.sleep(2)

    # Kill any leftover interface/overhead processes (SIGTERM first, then SIGKILL)
    subprocess.run(['killall', '-q', 'interface', 'overhead'], stderr=subprocess.DEVNULL)
    time.sleep(1)
    # Force kill anything that didn't respond to SIGTERM
    subprocess.run(['killall', '-q', '-9', 'interface', 'overhead'], stderr=subprocess.DEVNULL)
    time.sleep(0.5)

    # Clean ALL shared memory (semaphores and shared memory segments)
    shm_patterns = [
        'sem.mtx_*', 'sem.App_*', 'sem.DM_*', 'sem.HstTx_*',
        'sem.Isys_*', 'sem.Rx_*', 'sem.Tx_*',
        'App_Msg_Shm', 'DM_Msg_Shm', 'HstTx_Shm', 'Isys_Msg_Shm',
        'Rx_Shm', 'Tx_Shm', 'SharedMemory', 'TelnetTermSMem', 'TraceMemory',
    ]
    for pattern in shm_patterns:
        for f in glob.glob(f'/dev/shm/{pattern}'):
            try:
                os.remove(f)
            except OSError:
                pass

    # Wait for TCP port 5000 to be free (up to 10 seconds)
    for i in range(20):
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind(('0.0.0.0', 5000))
            s.close()
            break  # Port is free
        except OSError:
            if i == 0:
                print("Waiting for port 5000 to be released...")
            time.sleep(0.5)
            if i == 19:
                print("WARNING: Port 5000 still in use after 10s - startup may fail")

    if killed_something:
        print("Previous instance stopped.")


def write_pidfile():
    """Write our PID to the pidfile."""
    os.makedirs(os.path.dirname(PIDFILE), exist_ok=True)
    with open(PIDFILE, 'w') as f:
        f.write(str(os.getpid()))


def remove_pidfile():
    """Remove the pidfile on exit."""
    try:
        os.remove(PIDFILE)
    except OSError:
        pass


def read_settings_file(path):
    """Read a settings file, validate checksum, return (data, error)."""
    with open(path, 'rb') as f:
        raw = f.read()
    if len(raw) < 4:
        return None, "File too small"
    data = raw[:-4]
    stored_cs = struct.unpack_from('<I', raw, len(raw) - 4)[0]
    calc_cs = sum(data) & 0xFFFFFFFF
    if calc_cs != stored_cs:
        return data, f"WARNING: Checksum mismatch (calc={calc_cs:#010x} stored={stored_cs:#010x})"
    return data, None


def _wt(raw):
    """Format a raw weight value: show raw and lbs (raw/10000)."""
    if raw == 0:
        return "0"
    return f"{raw} ({raw / 10000.0:.4f} lbs)"


def decode_system_settings(data):
    """Decode system.bin (sys_in struct, 2424 bytes on aarch64)."""
    lines = []
    lines.append(f"Data size: {len(data)} bytes (expected 2424)")
    lines.append("")

    shackles = struct.unpack_from('<i', data, 0)[0]
    tare_times = struct.unpack_from('<i', data, 4)[0]
    gib_min = struct.unpack_from('<i', data, 8)[0]
    min_bird = struct.unpack_from('<q', data, 16)[0]
    last_drop = struct.unpack_from('<i', data, 24)[0]
    num_drops = struct.unpack_from('<i', data, 28)[0]
    gib_drop = struct.unpack_from('<i', data, 32)[0]
    drop_on = struct.unpack_from('<i', data, 36)[0]
    sync_on = struct.unpack_from('<i', data, 40)[0]
    skip_trollies = struct.unpack_from('<i', data, 44)[0]
    mb_enable = struct.unpack_from('<2i', data, 48)
    mb_sync = struct.unpack_from('<2i', data, 56)
    mb_offset = struct.unpack_from('<2i', data, 64)

    lines.append(f"Shackles:          {shackles}")
    lines.append(f"TareTimes:         {tare_times}")
    lines.append(f"GibMin:            {gib_min}")
    lines.append(f"MinBird:           {_wt(min_bird)}")
    lines.append(f"LastDrop:          {last_drop}")
    lines.append(f"NumDrops:          {num_drops}")
    lines.append(f"GibDrop:           {gib_drop}")
    lines.append(f"DropOn:            {drop_on}")
    lines.append(f"SyncOn:            {sync_on}")
    lines.append(f"SkipTrollies:      {skip_trollies}")
    lines.append(f"MissedBirdEnable:  [{mb_enable[0]}, {mb_enable[1]}]")
    lines.append(f"MBSync:            [{mb_sync[0]}, {mb_sync[1]}]")
    lines.append(f"MBOffset:          [{mb_offset[0]}, {mb_offset[1]}]")

    # SyncSettings[16] at offset 72, each TSyncSettings = 8 bytes (2 ints)
    lines.append("")
    lines.append("--- Sync Settings [index]: first..last ---")
    for i in range(MAXSYNCS):
        first, last = struct.unpack_from('<2i', data, 72 + i * 8)
        if first != 0 or last != 0:
            lines.append(f"  [{i:2d}]  first={first}  last={last}")

    # DropSettings[32] at offset 328, each TDropSettings = 32 bytes
    lines.append("")
    lines.append("--- Drop Settings ---")
    n = min(num_drops, MAXDROPS) if num_drops > 0 else MAXDROPS
    for i in range(n):
        base = 328 + i * 32
        active = struct.unpack_from('<B', data, base)[0]
        extgrade = struct.unpack_from('<B', data, base + 1)[0]
        isys_bch, isys_bchw, isys_bpm = struct.unpack_from('<3i', data, base + 4)
        sync, offset, drop_mode, switch_led = struct.unpack_from('<4i', data, base + 16)
        mode_names = {1: 'Normal', 2: 'RemoteReset', 3: 'Reassign'}
        mode_str = mode_names.get(drop_mode, str(drop_mode))
        if active:
            lines.append(f"  Drop {i+1:2d}: Active  Sync={sync}  Offset={offset}  Mode={mode_str}  SwitchLED={switch_led}")
        else:
            lines.append(f"  Drop {i+1:2d}: Inactive")

    # Grading at offset 1352
    grading = struct.unpack_from('<B', data, 1352)[0]
    reset_grade, reset_s1_grade, reset_s1_empty = struct.unpack_from('<3B', data, 1353)
    lines.append("")
    lines.append(f"Grading:           {'ON' if grading else 'OFF'}")
    lines.append(f"ResetGrade:        {reset_grade}  ResetS1Grade={reset_s1_grade}  ResetS1Empty={reset_s1_empty}")

    # GradeArea[5] at offset 1356, each TGradeSettings = 12 bytes
    lines.append("")
    lines.append("--- Grade Areas ---")
    grade_labels = ['A', 'B', 'C', 'D', 'Gib']
    for i in range(MAXGRADES):
        base = 1356 + i * 12
        grade_char = data[base:base+1].decode('ascii', errors='replace')
        if grade_char == '\x00':
            grade_char = '-'
        grade_offset = struct.unpack_from('<i', data, base + 4)[0]
        grade_sync2 = struct.unpack_from('<i', data, base + 8)[0]
        lines.append(f"  {grade_labels[i]}: char='{grade_char}'  offset={grade_offset}  GradeSync2={grade_sync2}")

    # MiscFeatures at offset 2408
    misc = struct.unpack_from('<4B', data, 2408)
    override_delay = struct.unpack_from('<i', data, 2412)[0]
    reassign_timer = struct.unpack_from('<i', data, 2416)[0]
    lines.append("")
    lines.append("--- Misc Features ---")
    lines.append(f"  EnableGradeSync2:      {'ON' if misc[0] else 'OFF'}")
    lines.append(f"  EnableBoazMode:        {'ON' if misc[1] else 'OFF'}")
    lines.append(f"  RequireZeroBeforeSync: {'ON' if misc[2] else 'OFF'}")
    lines.append(f"  OverRideDelay:         {override_delay}")
    lines.append(f"  ReassignTimerMax:      {reassign_timer}")

    return '\n'.join(lines)


def decode_scale_settings(data):
    """Decode scale.bin (scl_in struct, 272 bytes on aarch64)."""
    lines = []
    lines.append(f"Data size: {len(data)} bytes (expected 272)")
    lines.append("")

    num_scales = struct.unpack_from('<i', data, 0)[0]
    zero_number = struct.unpack_from('<i', data, 4)[0]
    auto_bias_limit = struct.unpack_from('<i', data, 8)[0]
    num_adc_reads = struct.unpack_from('<2i', data, 12)
    adc_mode = struct.unpack_from('<2i', data, 20)
    offset_adc = struct.unpack_from('<2i', data, 28)
    # Spare[2] at offset 40 (after 4 bytes padding)
    span_bias = struct.unpack_from('<2q', data, 56)
    load_cell_type = struct.unpack_from('<i', data, 72)[0]

    lc_types = {0: '1510 (Analog)', 1: 'HBM (Digital)'}
    lines.append(f"NumScales:      {num_scales}")
    lines.append(f"ZeroNumber:     {zero_number}")
    lines.append(f"AutoBiasLimit:  {auto_bias_limit}")
    lines.append(f"LoadCellType:   {load_cell_type} ({lc_types.get(load_cell_type, 'Unknown')})")
    lines.append("")

    for s in range(MAXLOADCELLS):
        lines.append(f"--- Load Cell {s+1} ---")
        lines.append(f"  NumAdcReads:  {num_adc_reads[s]}")
        lines.append(f"  AdcMode:      {adc_mode[s]}")
        lines.append(f"  OffsetAdc:    {offset_adc[s]}")
        lines.append(f"  SpanBias:     {span_bias[s]}")

    # DigLCSet[2] at offset 76, each dig_lc_settings = 21 ints = 84 bytes
    dig_lc_names = [
        'measure_mode', 'ASF', 'FMD', 'ICR', 'CWT', 'LDW', 'LWT',
        'NOV', 'RSN', 'MTD', 'LIC0', 'LIC1', 'LIC2', 'LIC3',
        'ZTR', 'ZSE', 'TRC1', 'TRC2', 'TRC3', 'TRC4', 'TRC5',
    ]
    for s in range(MAXLOADCELLS):
        lines.append("")
        lines.append(f"--- Digital LC Settings {s+1} ---")
        base = 76 + s * 84
        vals = struct.unpack_from('<21i', data, base)
        for j, name in enumerate(dig_lc_names):
            lines.append(f"  {name:15s} = {vals[j]}")

    # LC_Sample_Adj[2] at offset 244, each lc_sample_adj = 8 bytes
    lines.append("")
    for s in range(MAXSCALES):
        start, end = struct.unpack_from('<2i', data, 244 + s * 8)
        lines.append(f"LC_Sample_Adj[{s}]:  start={start}  end={end}")

    return '\n'.join(lines)


def decode_tares(data):
    """Decode tares.bin (TShackleTares[2000], 32000 bytes)."""
    lines = []
    lines.append(f"Data size: {len(data)} bytes (expected 32000)")
    lines.append("")

    non_zero = []
    for i in range(MAXPENDANT):
        base = i * 16
        t0, t1 = struct.unpack_from('<2q', data, base)
        if t0 != 0 or t1 != 0:
            non_zero.append((i + 1, t0, t1))

    lines.append(f"Non-zero tare entries: {len(non_zero)} of {MAXPENDANT}")
    lines.append("")

    if non_zero:
        lines.append(f"{'Shackle':>8s}  {'Scale 1 Tare':>16s}  {'Scale 2 Tare':>16s}")
        lines.append("-" * 46)
        shown = 0
        for shk, t0, t1 in non_zero:
            lines.append(f"{shk:8d}  {t0:16d}  {t1:16d}")
            shown += 1
            if shown >= 50:
                remaining = len(non_zero) - shown
                if remaining > 0:
                    lines.append(f"  ... and {remaining} more non-zero entries")
                break
    else:
        lines.append("All tares are zero.")

    return '\n'.join(lines)


def decode_schedule_settings(data):
    """Decode schedule.bin (TScheduleSettings[32], 3328 bytes)."""
    lines = []
    lines.append(f"Data size: {len(data)} bytes (expected 3328)")
    lines.append("")

    dist_modes = {
        1: 'Weight+Grade', 2: 'Grading', 3: 'Batch Count',
        4: 'BPM', 5: 'BPM+Batch', 6: 'Loop+Batch', 7: 'Loop+Batch+BPM',
    }

    for i in range(MAXDROPS):
        base = i * 104
        extgrade = data[base:base+8].rstrip(b'\x00').decode('ascii', errors='replace')
        grade = data[base+8:base+16].rstrip(b'\x00').decode('ascii', errors='replace')
        dist_mode = struct.unpack_from('<i', data, base + 16)[0]
        bpm_mode = struct.unpack_from('<i', data, base + 20)[0]
        loop_order = struct.unpack_from('<i', data, base + 24)[0]
        next_entry = struct.unpack_from('<i', data, base + 28)[0]
        min_wt = struct.unpack_from('<q', data, base + 32)[0]
        max_wt = struct.unpack_from('<q', data, base + 40)[0]
        shrinkage = struct.unpack_from('<i', data, base + 80)[0]
        bch_cnt = struct.unpack_from('<i', data, base + 84)[0]
        bch_wt = struct.unpack_from('<q', data, base + 88)[0]
        bpm_sp = struct.unpack_from('<i', data, base + 96)[0]

        # Skip empty drops (all zeros)
        if dist_mode == 0 and min_wt == 0 and max_wt == 0 and bch_cnt == 0:
            continue

        lines.append(f"--- Drop {i+1} ---")
        lines.append(f"  Grade:      '{grade}'  ExtGrade: '{extgrade}'")
        lines.append(f"  DistMode:   {dist_mode} ({dist_modes.get(dist_mode, '?')})")
        lines.append(f"  MinWeight:  {_wt(min_wt)}")
        lines.append(f"  MaxWeight:  {_wt(max_wt)}")
        lines.append(f"  BchCntSp:   {bch_cnt}")
        lines.append(f"  BchWtSp:    {_wt(bch_wt)}")
        lines.append(f"  BPMSetpoint:{bpm_sp}  BpmMode:{bpm_mode}")
        lines.append(f"  Shrinkage:  {shrinkage}%  LoopOrder:{loop_order}  NextEntry:{next_entry}")
        lines.append("")

    if not any(line.startswith("--- Drop") for line in lines):
        lines.append("No active schedule entries.")

    return '\n'.join(lines)


def decode_totals(data):
    """Decode totals.dat (ftot_info struct, 6256 bytes)."""
    lines = []
    lines.append(f"Data size: {len(data)} bytes (expected 6256)")
    lines.append("")

    tcnt = struct.unpack_from('<q', data, 0)[0]
    twt = struct.unpack_from('<q', data, 8)[0]

    lines.append(f"TotalCount:   {tcnt}")
    lines.append(f"TotalWeight:  {_wt(twt)}")

    # MdrpInfo[33] at offset 16, each TDropStats = 96 bytes
    # Index 0 = grand total across all drops, Index 1-32 = per-grade totals
    lines.append("")
    lines.append("--- Missed Bird Detector Grand Totals (MdrpInfo[0]) ---")
    base = 16
    bwt, bcnt = struct.unpack_from('<2q', data, base)
    lines.append(f"  Count: {bcnt}  Weight: {_wt(bwt)}")
    grd_cnt = struct.unpack_from(f'<{MAXGRADES}q', data, base + 16)
    grd_wt = struct.unpack_from(f'<{MAXGRADES}q', data, base + 56)
    for g in range(MAXGRADES):
        if grd_cnt[g] != 0 or grd_wt[g] != 0:
            lbl = ['A', 'B', 'C', 'D', 'Gib'][g]
            lines.append(f"    Grade {lbl}: count={grd_cnt[g]}  weight={_wt(grd_wt[g])}")

    # DrpInfo[32] at offset 16 + 33*96 = 3184, each TDropStats = 96 bytes
    lines.append("")
    lines.append("--- Per-Drop Totals ---")
    for i in range(MAXDROPS):
        base = 3184 + i * 96
        bwt, bcnt = struct.unpack_from('<2q', data, base)
        if bcnt == 0 and bwt == 0:
            continue
        lines.append(f"  Drop {i+1:2d}: count={bcnt}  weight={_wt(bwt)}")
        grd_cnt = struct.unpack_from(f'<{MAXGRADES}q', data, base + 16)
        grd_wt = struct.unpack_from(f'<{MAXGRADES}q', data, base + 56)
        for g in range(MAXGRADES):
            if grd_cnt[g] != 0:
                lbl = ['A', 'B', 'C', 'D', 'Gib'][g]
                lines.append(f"         Grade {lbl}: count={grd_cnt[g]}  weight={_wt(grd_wt[g])}")

    return '\n'.join(lines)


def decode_drecinfo(data):
    """Decode drecinfo.dat (fdrec_info struct, 20 bytes)."""
    lines = []
    lines.append(f"Data size: {len(data)} bytes (expected 20)")
    lines.append("")

    cur_file, nxt2_send, cur_cnt, nxt_seqnum, nxt_lbl_seqnum = struct.unpack_from('<5I', data, 0)

    lines.append(f"cur_file:       {cur_file}  (current drop record file number 0-99)")
    lines.append(f"nxt2_send:      {nxt2_send}  (starting file index for recovery)")
    lines.append(f"cur_cnt:        {cur_cnt}  (records in current file)")
    lines.append(f"nxt_seqnum:     {nxt_seqnum}  (next sequence number)")
    lines.append(f"nxt_lbl_seqnum: {nxt_lbl_seqnum}  (next label sequence number)")

    return '\n'.join(lines)


# Map menu labels to decoder functions
DECODERS = {
    'System Settings':   decode_system_settings,
    'Scale Settings':    decode_scale_settings,
    'Shackle Tares':     decode_tares,
    'Schedule Settings': decode_schedule_settings,
    'Totals':            decode_totals,
    'Drop Record Info':  decode_drecinfo,
}


class SettingsViewerWindow(Gtk.Window):
    """Independent window for displaying decoded settings file content."""

    def __init__(self, title, content, parent=None):
        super().__init__(title=title)
        self.set_default_size(700, 500)
        if parent:
            self.set_transient_for(parent)

        scrolled = Gtk.ScrolledWindow()
        scrolled.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)
        self.add(scrolled)

        textview = Gtk.TextView()
        textview.set_editable(False)
        textview.set_cursor_visible(False)
        textview.set_monospace(True)
        textview.set_left_margin(8)
        textview.set_top_margin(8)
        textview.get_buffer().set_text(content)
        scrolled.add(textview)

        self.show_all()


class DCHServerWindow(Gtk.Window):
    def __init__(self):
        super().__init__(title="DCH Server")
        self.set_default_size(900, 550)
        self.child_pid = -1

        # Main vertical box
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.add(vbox)

        # Menu bar
        menubar = self._build_menubar()
        vbox.pack_start(menubar, False, False, 0)

        # VTE terminal widget
        self.terminal = Vte.Terminal()
        self.terminal.set_scrollback_lines(10000)

        # Font
        font_desc = Pango.FontDescription("Monospace 11")
        self.terminal.set_font(font_desc)

        # Colors: dark background, light foreground
        bg = Gdk.RGBA()
        bg.parse("#0a0a1a")
        fg = Gdk.RGBA()
        fg.parse("#e0e0e0")
        self.terminal.set_color_background(bg)
        self.terminal.set_color_foreground(fg)

        # Scroll bar
        scrolled = Gtk.ScrolledWindow()
        scrolled.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        scrolled.add(self.terminal)
        vbox.pack_start(scrolled, True, True, 0)

        # Signals
        self.terminal.connect("child-exited", self.on_child_exited)
        self.connect("delete-event", self.on_close)

        # Spawn the interface process
        self.spawn_interface()

    def _build_menubar(self):
        """Build the menu bar with File menu."""
        menubar = Gtk.MenuBar()

        # File menu
        file_menu = Gtk.Menu()
        file_item = Gtk.MenuItem(label="File")
        file_item.set_submenu(file_menu)

        # Save Log
        save_item = Gtk.MenuItem(label="Save Log")
        save_item.connect("activate", self.on_save_log)
        file_menu.append(save_item)

        # Clear Screen
        clear_item = Gtk.MenuItem(label="Clear Screen")
        clear_item.connect("activate", self.on_clear_screen)
        file_menu.append(clear_item)

        file_menu.append(Gtk.SeparatorMenuItem())

        # Exit
        exit_item = Gtk.MenuItem(label="Exit")
        exit_item.connect("activate", lambda _: self.close())
        file_menu.append(exit_item)

        menubar.append(file_item)

        # Diagnostics menu
        diag_menu = Gtk.Menu()
        diag_item = Gtk.MenuItem(label="Diagnostics")
        diag_item.set_submenu(diag_menu)

        for label in SETTINGS_FILES:
            item = Gtk.MenuItem(label=label)
            item.connect("activate", self.on_view_settings, label)
            diag_menu.append(item)

        menubar.append(diag_item)
        return menubar

    def on_view_settings(self, widget, label):
        """Open a settings file viewer window."""
        path = SETTINGS_FILES[label]
        if not os.path.isfile(path):
            dialog = Gtk.MessageDialog(
                transient_for=self,
                message_type=Gtk.MessageType.ERROR,
                buttons=Gtk.ButtonsType.OK,
                text=f"File not found:\n{path}",
            )
            dialog.run()
            dialog.destroy()
            return

        try:
            data, err = read_settings_file(path)
            header = f"=== {label} ===\n"
            header += f"File: {path}\n"
            header += f"Size: {os.path.getsize(path)} bytes\n"
            if err:
                header += f"{err}\n"
            header += "\n"

            if data is not None:
                decoder = DECODERS[label]
                content = header + decoder(data)
            else:
                content = header + f"ERROR: {err}"
        except Exception as e:
            content = f"Failed to decode {path}:\n{e}"

        SettingsViewerWindow(label, content, parent=self)

    def on_save_log(self, widget):
        """Save terminal content to a log file."""
        # Extract text from terminal (get_text returns (text, attr_array))
        text_data = self.terminal.get_text()
        if text_data is None:
            return

        # VTE get_text returns a tuple: (text, attributes)
        text = text_data[0] if isinstance(text_data, tuple) else text_data

        if not text or not text.strip():
            dialog = Gtk.MessageDialog(
                transient_for=self,
                message_type=Gtk.MessageType.INFO,
                buttons=Gtk.ButtonsType.OK,
                text="Nothing to save - terminal is empty.",
            )
            dialog.run()
            dialog.destroy()
            return

        # Ensure log directory exists
        os.makedirs(LOG_DIR, exist_ok=True)

        # Generate filename with timestamp
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        default_name = f"dch_server_{timestamp}.log"
        default_path = os.path.join(LOG_DIR, default_name)

        # File chooser dialog
        chooser = Gtk.FileChooserDialog(
            title="Save Log File",
            parent=self,
            action=Gtk.FileChooserAction.SAVE,
        )
        chooser.add_buttons(
            Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
            Gtk.STOCK_SAVE, Gtk.ResponseType.ACCEPT,
        )
        chooser.set_do_overwrite_confirmation(True)
        chooser.set_current_folder(LOG_DIR)
        chooser.set_current_name(default_name)

        # Add filter for log files
        filter_log = Gtk.FileFilter()
        filter_log.set_name("Log files")
        filter_log.add_pattern("*.log")
        filter_log.add_pattern("*.txt")
        chooser.add_filter(filter_log)

        filter_all = Gtk.FileFilter()
        filter_all.set_name("All files")
        filter_all.add_pattern("*")
        chooser.add_filter(filter_all)

        response = chooser.run()
        if response == Gtk.ResponseType.ACCEPT:
            filepath = chooser.get_filename()
            try:
                with open(filepath, 'w') as f:
                    f.write(f"DCH Server Log - Saved {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                    f.write("=" * 60 + "\n\n")
                    f.write(text)

                dialog = Gtk.MessageDialog(
                    transient_for=self,
                    message_type=Gtk.MessageType.INFO,
                    buttons=Gtk.ButtonsType.OK,
                    text=f"Log saved to:\n{filepath}",
                )
                dialog.run()
                dialog.destroy()
            except Exception as e:
                dialog = Gtk.MessageDialog(
                    transient_for=self,
                    message_type=Gtk.MessageType.ERROR,
                    buttons=Gtk.ButtonsType.OK,
                    text=f"Failed to save log:\n{e}",
                )
                dialog.run()
                dialog.destroy()

        chooser.destroy()

    def on_clear_screen(self, widget):
        """Clear the terminal screen."""
        # Reset terminal and clear scrollback
        self.terminal.reset(True, True)

    def spawn_interface(self):
        env = os.environ.copy()
        env['TERM'] = 'xterm-256color'
        envv = [f"{k}={v}" for k, v in env.items()]

        working_dir = os.path.dirname(os.path.abspath(INTERFACE_BIN))

        self.terminal.spawn_async(
            Vte.PtyFlags.DEFAULT,
            working_dir,
            [os.path.abspath(INTERFACE_BIN)],
            envv,
            GLib.SpawnFlags.DEFAULT,
            None, None,   # child_setup, child_setup_data
            -1,           # timeout (-1 = default)
            None,         # cancellable
            self.on_spawn_complete,
        )

    def on_spawn_complete(self, terminal, pid, error):
        if error:
            self.terminal.feed(f"\r\n*** Failed to start interface: {error.message} ***\r\n".encode())
            self.child_pid = -1
        else:
            self.child_pid = pid

    def on_child_exited(self, terminal, status):
        exit_code = os.waitstatus_to_exitcode(status) if hasattr(os, 'waitstatus_to_exitcode') else status
        terminal.feed(f"\r\n*** Interface process exited (status {exit_code}) ***\r\n".encode())
        self.child_pid = -1

    def on_close(self, widget, event):
        if self.child_pid > 0:
            try:
                os.kill(self.child_pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            # Give it a moment to clean up
            GLib.timeout_add(500, Gtk.main_quit)
            return True  # Prevent immediate close
        return False  # Allow close


def main():
    # Stop any existing instance first
    kill_existing_instance()

    # Write our PID so future launches can find us
    write_pidfile()

    # Clean up pidfile on exit
    import atexit
    atexit.register(remove_pidfile)

    # Ignore SIGINT in the GUI process (let the terminal child handle it)
    signal.signal(signal.SIGINT, signal.SIG_IGN)

    win = DCHServerWindow()
    win.show_all()
    Gtk.main()


if __name__ == '__main__':
    main()
