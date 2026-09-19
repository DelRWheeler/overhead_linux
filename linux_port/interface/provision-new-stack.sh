#!/bin/bash
# provision-new-stack.sh — take a new/cloned SandCat CPU stack to fleet standard
# in ONE command, from the dev VM, and refuse to pass until it verifies.
#
# Run this on EVERY new board stack before it ships or goes into a line, and
# again after pushing new binaries. It is idempotent: re-running a good box
# changes nothing and exits 0.
#
#   ./provision-new-stack.sh --ip 192.168.100.13 --ntp 192.168.100.102 \
#        --timezone America/New_York [--restart]
#
# It does three things, in this order:
#   1. automatic updates  -> remediate-appliance.sh --apt-only (the overhead repo
#      owns that policy; this only drives it). apt-daily has restarted the whole
#      stack unattended at 07:00 and has been implicated in a controller being
#      watchdog-killed mid-run, so it goes FIRST -- before anything reboots.
#   2. commissioning      -> commission-stack.sh (kiosk geometry, NTP, hostname,
#      /etc/hosts, Restart=always, timezone)
#   3. verification       -> commission-stack.sh --check, whose non-zero exit is
#      this script's exit. A stack that does not verify does not ship.
#
# --restart bounces the controller to make the display change live. Without it
# the geometry sits on disk until the next GUI start, which is the right default
# on a line that is weighing.
set -u

IP=""; NTP=""; TZWANT=""; RESTART=""; OVERHEAD_REPO=""
EXPECT_OVERHEAD=""; EXPECT_INTERFACE=""
PASS=dchservice
while [ $# -gt 0 ]; do
  case "$1" in
    --ip) IP="$2"; shift 2;;
    --ntp) NTP="$2"; shift 2;;
    --timezone) TZWANT="$2"; shift 2;;
    --overhead-repo) OVERHEAD_REPO="$2"; shift 2;;
    --expect-overhead) EXPECT_OVERHEAD="$2"; shift 2;;
    --expect-interface) EXPECT_INTERFACE="$2"; shift 2;;
    --restart) RESTART="--restart"; shift;;
    *) echo "unknown option: $1" >&2; exit 2;;
  esac
done
[ -n "$IP" ]  || { echo "ERROR: --ip is required" >&2; exit 2; }
[ -n "$NTP" ] || { echo "ERROR: --ntp is required (VIP 192.168.100.102 at an HA plant, the Pi .103 on the office rig)" >&2; exit 2; }

HERE="$(cd "$(dirname "$0")" && pwd)"
# PubkeyAuthentication=no is not optional: the agent offers too many keys first
# and the board rejects with "Too many authentication failures". UserKnownHosts
# to /dev/null because a re-imaged board changes host key and that reads exactly
# like a blocked SSH.
SSHOPTS="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
 -o PreferredAuthentications=password -o PubkeyAuthentication=no -o ConnectTimeout=20"
SSHC() { sshpass -p "$PASS" ssh $SSHOPTS "dchservice@$IP" "$@"; }
SCPC() { sshpass -p "$PASS" scp $SSHOPTS "$@"; }

echo "=== provisioning SandCat stack at $IP ==="
SSHC 'echo "  reachable: $(hostname) — uptime $(uptime -p)"' || { echo "ERROR: cannot reach $IP" >&2; exit 1; }

# --- 1. automatic updates --------------------------------------------------
# Locate the overhead repo for the apt payload. Sibling checkout by default.
if [ -z "$OVERHEAD_REPO" ]; then
  for cand in "$HERE/../../../overhead" "$HOME/Documents/projects/overhead"; do
    [ -d "$cand/provisioning/scripts" ] && { OVERHEAD_REPO="$(cd "$cand" && pwd)"; break; }
  done
fi
echo
echo "--- 1/3 automatic updates ---"
if [ -z "$OVERHEAD_REPO" ]; then
  echo "  ⚠️  SKIPPED — overhead repo not found; pass --overhead-repo DIR."
  echo "      apt will be reported as a FAILURE by the verify step below."
else
  tar -C "$OVERHEAD_REPO/provisioning" -czf - scripts/remediate-appliance.sh \
      payload/config/apt-no-auto-upgrades.conf \
    | SSHC 'rm -rf /tmp/ovh-remediate && mkdir -p /tmp/ovh-remediate \
        && tar -C /tmp/ovh-remediate -xzf - \
        && echo dchservice | sudo -S /tmp/ovh-remediate/scripts/remediate-appliance.sh --apt-only' \
    2>&1 | grep -vE "^\[sudo\]|Warning: Permanently" | sed -n '/---- CHANGED ----/,/---- SKIPPED ----/p' | sed 's/^/  /'
fi

# --- 2. commissioning ------------------------------------------------------
echo
echo "--- 2/3 commissioning ---"
SCPC "$HERE/commission-stack.sh" "dchservice@$IP:/tmp/" 2>/dev/null
ARGS="--ntp $NTP"
[ -n "$TZWANT" ] && ARGS="$ARGS --timezone $TZWANT"
SSHC "bash /tmp/commission-stack.sh $ARGS $RESTART" 2>/dev/null | sed 's/^/  /'

# --- 3. verification (this script's exit status) ---------------------------
echo
echo "--- 3/3 verification ---"
VARGS="--check --ntp $NTP"
[ -n "$TZWANT" ]          && VARGS="$VARGS --timezone $TZWANT"
[ -n "$EXPECT_OVERHEAD" ] && VARGS="$VARGS --expect-overhead $EXPECT_OVERHEAD"
[ -n "$EXPECT_INTERFACE" ]&& VARGS="$VARGS --expect-interface $EXPECT_INTERFACE"
SSHC "bash /tmp/commission-stack.sh $VARGS" 2>/dev/null | sed 's/^/  /'
RC=${PIPESTATUS[0]}

echo
if [ "$RC" -eq 0 ]; then
  echo "✅ $IP is at fleet standard."
  [ -n "$RESTART" ] || echo "   (display geometry applies at the next GUI start; --restart makes it live and BOUNCES the controller)"
else
  echo "❌ $IP did NOT verify — see the items above. Do not ship this stack."
fi
exit "$RC"
