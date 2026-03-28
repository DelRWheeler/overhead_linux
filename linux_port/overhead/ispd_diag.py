#!/usr/bin/env python3
"""ISPD-20KG diagnostic - query all settings and test streaming.
Run standalone: python3 ispd_diag.py /dev/ttyS0"""

import serial
import sys
import time

PORT = sys.argv[1] if len(sys.argv) > 1 else '/dev/ttyS0'
BAUD = 115200

def send(ser, cmd_bytes, label, wait=0.5):
    ser.reset_input_buffer()
    ser.write(cmd_bytes)
    time.sleep(wait)
    resp = ser.read(ser.in_waiting or 200)
    resp_str = resp.decode('ascii', errors='ignore').strip()
    print(f"  {label:30s} -> [{resp_str}]")
    return resp_str

ser = serial.Serial(PORT, BAUD, timeout=1)
ser.reset_input_buffer()

print("=== ISPD-20KG Diagnostic ===\n")

print("--- Open Device ---")
send(ser, b'OP 1\r', 'OP 1 (open device)')

print("\n--- Device Info ---")
send(ser, b'ID\r', 'ID (device identity)')
send(ser, b'IV\r', 'IV (firmware version)')
send(ser, b'IS\r', 'IS (device status)')

print("\n--- Network Settings (NS) ---")
send(ser, b'NS 0 0\r', 'NS 0 0 (device ID)')
send(ser, b'NS 0 1\r', 'NS 0 1 (baud rate)')
send(ser, b'NS 0 2\r', 'NS 0 2 (address)')
send(ser, b'NS 0 3\r', 'NS 0 3 (serial mode)')
send(ser, b'NS 0 4\r', 'NS 0 4 (TX delay)')

print("\n--- Filter & Rate Settings ---")
send(ser, b'FL\r', 'FL (filter level)')
send(ser, b'FM\r', 'FM (filter mode)')
send(ser, b'UR\r', 'UR (update rate)')
send(ser, b'BR\r', 'BR (baud rate)')
send(ser, b'DX\r', 'DX (duplex mode)')
send(ser, b'AT\r', 'AT (auto-transmit)')
send(ser, b'DP\r', 'DP (decimal position)')

print("\n--- Try Setting UR (critical for streaming at 115200) ---")
# Try various formats
for cmd in [b'UR1\r', b'UR 1\r', b'UR_1\r']:
    resp = send(ser, cmd, f'Set UR: {cmd}')
    if 'OK' in resp:
        print(f"  *** UR set successfully with: {cmd}")
        break
    time.sleep(0.2)

# Query UR again to confirm
send(ser, b'UR\r', 'UR (verify)')

print("\n--- Try Setting FL, FM ---")
send(ser, b'FL3\r', 'FL3 (filter level 3)')
send(ser, b'FM0\r', 'FM0 (IIR filter)')

print("\n--- Save and Test Stream ---")
send(ser, b'WP\r', 'WP (save to EEPROM)', wait=0.8)

# Test SX streaming for 5 seconds
print("\n--- SX Stream Test (5 seconds) ---")
ser.reset_input_buffer()
ser.write(b'SX\r')
print("  SX sent, reading for 5 seconds...")

start = time.time()
count = 0
buf = b''
while time.time() - start < 5.0:
    chunk = ser.read(ser.in_waiting or 1)
    if chunk:
        buf += chunk
        while b'\r' in buf:
            line, buf = buf.split(b'\r', 1)
            if buf.startswith(b'\n'):
                buf = buf[1:]
            text = line.decode('ascii', errors='ignore').strip()
            if text and text[0] in ('S', 's', 'G', 'g', 'N', 'n'):
                count += 1
                if count <= 3 or count % 500 == 0:
                    elapsed = time.time() - start
                    print(f"  sample #{count} at {elapsed:.2f}s: {text}")

elapsed = time.time() - start
print(f"\n  RESULT: {count} samples in {elapsed:.1f}s = {count/elapsed:.0f} samples/sec")
if count < 100:
    print("  *** STREAMING FAILED - stream died early")
elif count/elapsed > 650:
    print("  *** WARNING: rate > 600/sec, may have streaming conflict")
else:
    print("  *** STREAMING OK")

# Stop streaming
ser.write(b'GS\r')
time.sleep(0.2)
ser.read(ser.in_waiting or 100)
print("\nDone.")
ser.close()
