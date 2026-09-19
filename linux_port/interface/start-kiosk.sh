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

# --- 10" kiosk panel geometry -------------------------------------------------
# The 10" Lilliput panel is truly 1280x800, but its EDID over-reports 1920x1200.
# Left alone, X picks 1920x1200 and the GUI overscans off the glass. Force the
# panel to its real mode and size the window to match it exactly.
#
# To retune for a different panel, change DCH_PANEL_MODE below. Setting it to
# an empty string skips the xrandr call and leaves X on whatever it picked.
DCH_PANEL_MODE="${DCH_PANEL_MODE-1280x800}"

if [ -n "$DCH_PANEL_MODE" ]; then
    # Apply to whichever output is actually connected rather than hardcoding
    # DP-1, so a panel moved to VGA-1 still comes up right.
    for out in $(xrandr | awk '/ connected/ {print $1}'); do
        xrandr --output "$out" --mode "$DCH_PANEL_MODE" 2>/dev/null && break
    done
fi

export DCH_GUI_WIDTH="${DCH_PANEL_MODE%x*}"
export DCH_GUI_HEIGHT="${DCH_PANEL_MODE#*x}"
export DCH_GUI_MENU_FONT_SIZE=20
# export DCH_GUI_FONT_SIZE=24     # message/terminal font (default 24; leave as-is)

# Launch the DCH Server GUI
exec /usr/bin/python3 /home/dchservice/dchservices/bin/dch-server-gui.py
