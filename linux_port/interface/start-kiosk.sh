#!/bin/bash
# DCH Server Kiosk Launcher
# Starts X11 with the DCH Server GUI in kiosk mode.
# If the GUI exits, X exits, and systemd restarts everything.

# Clean shared memory from any previous run
rm -f /dev/shm/sem.* /dev/shm/shm_*

# Disable screen blanking / power saving
xset s off
xset -dpms
xset s noblank

# Hide the mouse cursor after 3 seconds of inactivity (if unclutter is installed)
command -v unclutter >/dev/null 2>&1 && unclutter -idle 3 &

# --- 10" touch-panel geometry (this panel's EDID over-reports the usable area,
# so a full-frame window at 0,0 spills off the left and top). Nudge the window
# right/down and shrink it to sit inside the visible area. Tune these four to
# fit the panel; the GUI reads them as DCH_GUI_X/Y/WIDTH/HEIGHT.
export DCH_GUI_X=300
export DCH_GUI_Y=50
export DCH_GUI_WIDTH=1600
export DCH_GUI_HEIGHT=1100

# Launch the DCH Server GUI
exec /usr/bin/python3 /home/dchservice/dchservices/bin/dch-server-gui.py
