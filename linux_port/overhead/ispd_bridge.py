#!/usr/bin/env python3
"""ISPD-20KG serial bridge - streams raw ADC samples to stdout.
The controller reads from this process's stdout via popen().

Uses SX continuous streaming (~586 samples/sec).
After sending SX, sets serial break condition to hold TX line LOW,
preventing idle noise that ISPD interprets as a stop command."""

import serial
import sys
import time

PORT = sys.argv[1] if len(sys.argv) > 1 else '/dev/ttyS0'
BAUD = 115200

def log(msg):
    sys.stderr.write(f"ISPD bridge: {msg}\n")
    sys.stderr.flush()

def send_cmd(ser, cmd, label, wait=0.3):
    """Send a command and read the response."""
    ser.reset_input_buffer()
    ser.write(cmd)
    time.sleep(wait)
    resp = ser.read(ser.in_waiting or 100)
    resp_str = resp.decode('ascii', errors='ignore').strip()
    log(f"{label} -> {resp_str}")
    return resp_str

def restore_fpga():
    """Restore UARTMODE1 to 0x33 in case a previous run changed it."""
    try:
        with open('/dev/port', 'wb') as f:
            f.seek(0xCB6)
            f.write(bytes([0x33]))
        log("UARTMODE1 restored to 0x33")
    except:
        pass  # Might not have permission, that's OK

try:
    # Restore FPGA in case previous run broke it
    restore_fpga()
    time.sleep(0.1)

    ser = serial.Serial(PORT, BAUD, timeout=0.1)
    ser.reset_input_buffer()

    # Open device at address 1 (must be first command)
    send_cmd(ser, b'OP 1\r', 'OP 1')

    # Verify identity
    send_cmd(ser, b'ID\r', 'ID')

    # Query current settings
    send_cmd(ser, b'FL\r', 'FL?')
    send_cmd(ser, b'FM\r', 'FM?')
    send_cmd(ser, b'UR\r', 'UR?')

    # Configure filter settings
    send_cmd(ser, b'FL3\r', 'FL3')
    send_cmd(ser, b'FM0\r', 'FM0')
    # UR may need different format - try with and without space
    resp = send_cmd(ser, b'UR1\r', 'UR1')
    if 'ERR' in resp.upper() or not resp:
        send_cmd(ser, b'UR 1\r', 'UR 1')

    # Save settings to EEPROM
    send_cmd(ser, b'WP\r', 'WP', wait=0.5)

    # Start continuous raw ADC sample output
    ser.reset_input_buffer()
    ser.write(b'SX\r')
    time.sleep(0.01)

    # Hold TX line in break state (continuous LOW)
    # Break is a special UART condition - not interpreted as data by ISPD
    # Prevents idle TX noise from being seen as a command
    ser.break_condition = True
    log("SX sent, TX break enabled")

    buf = b''
    sample_count = 0
    total_samples = 0
    last_status = time.time()

    while True:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            buf += chunk
            while b'\r' in buf:
                line_bytes, buf = buf.split(b'\r', 1)
                if buf.startswith(b'\n'):
                    buf = buf[1:]
                line = line_bytes.decode('ascii', errors='ignore').strip()
                if len(line) >= 3 and line[0] in ('S', 's'):
                    try:
                        val_counts = int(line[1:])
                        sys.stdout.write(f"{val_counts}\n")
                        sys.stdout.flush()
                        sample_count += 1
                        total_samples += 1
                        if total_samples <= 3:
                            log(f"sample #{total_samples} = {val_counts}")
                    except (ValueError, IndexError):
                        pass

        # Periodic status
        now = time.time()
        if now - last_status >= 10.0:
            rate = sample_count / 10.0
            log(f"status: {rate:.0f} samples/sec ({total_samples} total)")
            sample_count = 0
            last_status = now

except Exception as e:
    log(f"FATAL: {e}")
    import traceback
    traceback.print_exc(file=sys.stderr)
    sys.exit(1)
