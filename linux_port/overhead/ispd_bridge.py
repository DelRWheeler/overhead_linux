#!/usr/bin/env python3
"""ISPD-20KG serial bridge - polls GS and writes measurements to stdout.
The controller reads from this process's stdout via popen().
Uses pyserial which is proven to work reliably on SandCat RS-422."""

import serial
import sys
import time

PORT = sys.argv[1] if len(sys.argv) > 1 else '/dev/ttyS0'
BAUD = 115200

try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    ser.reset_input_buffer()

    # Open device at address 1
    ser.write(b'OP 1\r')
    time.sleep(0.3)
    resp = ser.read(ser.in_waiting or 100)
    sys.stderr.write(f"ISPD bridge: OP 1 -> {resp}\n")

    # Verify ID
    ser.write(b'ID\r')
    time.sleep(0.3)
    resp = ser.read(ser.in_waiting or 100)
    sys.stderr.write(f"ISPD bridge: ID -> {resp}\n")

    # SG = Send Gross Value continuously (~580 Hz ASCII stream)
    # Format: G±NN.NNNN\r (gross weight in kg with decimal)
    # Controller needs raw ADC counts, so we convert by removing decimal
    # and multiplying to match GS scale (counts_per_pound = 13340)
    ser.write(b'SG\r')
    sys.stderr.write("ISPD bridge: SG continuous streaming started\n")
    sys.stderr.flush()

    buf = b''
    while True:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            buf += chunk
            while b'\r' in buf:
                line_bytes, buf = buf.split(b'\r', 1)
                line = line_bytes.decode('ascii', errors='ignore').strip()
                if line.startswith('G') and len(line) >= 3:
                    try:
                        # Parse G±NN.NNNN -> float -> raw counts
                        # Remove 'G' prefix, parse as float
                        val_kg = float(line[1:])
                        # Convert to raw counts: multiply by 10000 to remove decimal
                        # (matches GS scale where counts = weight * counts_per_unit)
                        val_counts = int(val_kg * 10000)
                        sys.stdout.write(f"{val_counts}\n")
                        sys.stdout.flush()
                    except (ValueError, IndexError):
                        pass

except Exception as e:
    sys.stderr.write(f"ISPD bridge error: {e}\n")
    sys.exit(1)
