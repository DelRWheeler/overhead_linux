#!/bin/bash
# commission-stack.sh — bring a new/cloned SandCat controller to fleet standard.
#
# New SandCats arrive either stock from the vendor or cloned from another box.
# Both carry the same handful of defects every single time, and each one has
# cost a live session at least once. This script applies all of them, is safe to
# re-run, and prints what it changed.
#
#   Run ON the controller:
#     scp commission-stack.sh dchservice@<ip>:/tmp/
#     ssh dchservice@<ip> 'bash /tmp/commission-stack.sh --ntp 192.168.100.102'
#
#   Options:
#     --ntp <ip>    time source. At an HA plant this is the VIP (192.168.100.102)
#                   so NTP follows failover. On the office rig it is the Pi
#                   (192.168.100.103). REQUIRED — there is no safe default.
#     --hostname <name>   override the auto-derived dchserverN
#     --restart     restart dch-server-gui to apply the display change.
#                   ⚠️ THIS BOUNCES THE CONTROLLER — weighing stops and the load
#                   cell re-inits. Only with the line down and permission given.
#     --check       report only, change nothing.
#
# What it does NOT do: deploy binaries. Check the reported versions against the
# fleet and deploy separately if they are stale.

set -u
KIOSK=/home/dchservice/dchservices/bin/start-kiosk.sh
UNIT=/etc/systemd/system/dch-server-gui.service
SUDO() { echo dchservice | sudo -S "$@" 2>/dev/null; }   # these boards want a piped password

NTP=""; HOSTNAME_OVERRIDE=""; DO_RESTART=0; CHECK_ONLY=0
while [ $# -gt 0 ]; do
  case "$1" in
    --ntp) NTP="$2"; shift 2;;
    --hostname) HOSTNAME_OVERRIDE="$2"; shift 2;;
    --restart) DO_RESTART=1; shift;;
    --check) CHECK_ONLY=1; shift;;
    *) echo "unknown option: $1" >&2; exit 2;;
  esac
done

CHANGED=0; NEEDS_RESTART=0
say() { printf '  %-22s %s\n' "$1" "$2"; }
changed() { CHANGED=1; printf '  \033[1m%-22s %s\033[0m\n' "$1" "$2"; }

MYIP=$(ip -4 -o addr show | awk '$2!="lo"{split($4,a,"/"); if (a[1] ~ /^192\.168\.100\./) print a[1]}' | head -1)
echo "=== commission-stack.sh on $(hostname) (${MYIP:-unknown ip}) ==="

# --- 1. kiosk display geometry ------------------------------------------------
# The 10" panel's EDID over-reports 1920x1200; left alone the GUI overscans off
# the glass. The canonical launcher forces the panel's true 1280x800.
if [ ! -f "$KIOSK" ]; then
  say "display" "SKIP — $KIOSK not found"
elif grep -q DCH_PANEL_MODE "$KIOSK"; then
  say "display" "already current"
else
  if [ "$CHECK_ONLY" = 1 ]; then
    changed "display" "NEEDS FIX — stock launcher, will overscan"
  else
    cp -p "$KIOSK" "$KIOSK.bak-$(date +%Y%m%d-%H%M%S)"
    # write in place: $KIOSK is hardlinked to /home/del/dchservices/bin/, and
    # mv/install would break the link and leave one path stale.
    cat > "$KIOSK" <<'KIOSKEOF'
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
KIOSKEOF
    chmod 775 "$KIOSK"
    changed "display" "launcher updated (1280x800) — needs GUI restart"
    NEEDS_RESTART=1
  fi
fi

# --- 2. NTP source ------------------------------------------------------------
# A cloned stack points at whatever its source pointed at. 192.168.100.103 is
# the Pi on the OFFICE RIG but the STANDBY PC at every HA plant, so a clone
# silently aims its time client at the standby and never follows failover.
CONF=/etc/systemd/timesyncd.conf.d/overhead-interface.conf
CUR_NTP=$(grep -hoP '^NTP=\K.*' "$CONF" 2>/dev/null)
if [ -z "$NTP" ]; then
  say "ntp" "${CUR_NTP:-unset} (no --ntp given, left alone)"
elif [ "$CUR_NTP" = "$NTP" ]; then
  say "ntp" "$CUR_NTP already correct"
elif [ "$CHECK_ONLY" = 1 ]; then
  changed "ntp" "NEEDS FIX — ${CUR_NTP:-unset} should be $NTP"
else
  SUDO mkdir -p /etc/systemd/timesyncd.conf.d
  printf '[Time]\nNTP=%s\n' "$NTP" | SUDO tee "$CONF" >/dev/null
  SUDO systemctl restart systemd-timesyncd
  changed "ntp" "${CUR_NTP:-unset} -> $NTP"
fi

# --- 3. hostname --------------------------------------------------------------
# Clones all answer to their source's name; three boxes called dchserver2 makes
# every later diagnostic ambiguous.
WANT_HOST="$HOSTNAME_OVERRIDE"
if [ -z "$WANT_HOST" ] && [ -n "$MYIP" ]; then
  WANT_HOST="dchserver$(( ${MYIP##*.} - 10 ))"   # .11 -> dchserver1
fi
if [ -z "$WANT_HOST" ]; then
  say "hostname" "$(hostname) (could not derive; pass --hostname)"
elif [ "$(hostname)" = "$WANT_HOST" ]; then
  say "hostname" "$WANT_HOST already correct"
elif [ "$CHECK_ONLY" = 1 ]; then
  changed "hostname" "NEEDS FIX — $(hostname) should be $WANT_HOST"
else
  OLD=$(hostname)
  SUDO hostnamectl set-hostname "$WANT_HOST"
  SUDO sed -i "s/\b$OLD\b/$WANT_HOST/g" /etc/hosts
  changed "hostname" "$OLD -> $WANT_HOST"
fi

# --- 4. GUI restart policy ----------------------------------------------------
# Restart=on-failure never catches a CLEAN GUI exit, which leaves the box
# pingable with the controller dead. Must be Restart=always.
if [ ! -f "$UNIT" ]; then
  say "restart-policy" "SKIP — $UNIT not found"
else
  POL=$(grep -oP '^Restart=\K.*' "$UNIT"); LIM=$(grep -oP '^StartLimitIntervalSec=\K.*' "$UNIT")
  if [ "$POL" = always ] && [ "$LIM" = 0 ]; then
    say "restart-policy" "Restart=always, StartLimitIntervalSec=0 — correct"
  elif [ "$CHECK_ONLY" = 1 ]; then
    changed "restart-policy" "NEEDS FIX — Restart=$POL StartLimitIntervalSec=${LIM:-unset}"
  else
    # Gate the daemon-reload: reload/mask has segfaulted systemd and frozen PID 1
    # on boxes with a huge accumulated restart count. Reboot such a box first.
    NR=$(systemctl show dch-server-gui -p NRestarts --value)
    if [ "${NR:-0}" -gt 100000 ]; then
      changed "restart-policy" "REFUSED — NRestarts=$NR too high for daemon-reload; REBOOT FIRST"
    else
      SUDO cp -p "$UNIT" "$UNIT.bak-$(date +%Y%m%d-%H%M%S)"
      SUDO sed -i 's/^Restart=.*/Restart=always/' "$UNIT"
      grep -q '^StartLimitIntervalSec=' "$UNIT" \
        || SUDO sed -i '0,/^\[Unit\]/s//[Unit]\nStartLimitIntervalSec=0/' "$UNIT"
      SUDO systemctl daemon-reload
      changed "restart-policy" "Restart=$POL -> always (NRestarts=$NR)"
    fi
  fi
fi

# --- 5. report binary versions (deploy separately if stale) -------------------
for b in overhead interface; do
  f=/home/dchservice/dchservices/bin/$b
  [ -f "$f" ] && say "$b" "md5=$(md5sum "$f" | cut -c1-8) size=$(stat -c%s "$f")"
done

# --- 6. apply the display change ---------------------------------------------
echo
if [ "$CHECK_ONLY" = 1 ]; then
  [ "$CHANGED" = 1 ] && echo "  => items need fixing (re-run without --check)" || echo "  => box is at fleet standard"
elif [ "$NEEDS_RESTART" = 1 ] && [ "$DO_RESTART" = 1 ]; then
  echo "  restarting dch-server-gui (controller bounces)..."
  SUDO systemctl --no-block restart dch-server-gui   # a blocking start hangs the SSH session
  echo "  => restart issued; verify screen, window and controller before leaving"
elif [ "$NEEDS_RESTART" = 1 ]; then
  echo "  => display change is on disk but NOT live."
  echo "     Restart dch-server-gui when the line is down: this bounces the controller."
else
  [ "$CHANGED" = 1 ] && echo "  => done" || echo "  => box already at fleet standard, nothing changed"
fi
