"""Read mining counters over USB without requesting a device reset or secrets."""
import argparse
import sys
import time
import serial

sys.stdout.reconfigure(encoding='utf-8', errors='replace')
parser = argparse.ArgumentParser()
parser.add_argument('--port', required=True)
parser.add_argument('--seconds', type=float, default=12)
args = parser.parse_args()
device = serial.Serial(port=None, baudrate=115200, timeout=0.2)
device.dtr = False
device.rts = False
device.port = args.port
with device:
    device.reset_input_buffer()
    start = time.monotonic()
    sent = 0
    skipped = 0
    while time.monotonic() - start < args.seconds:
        elapsed = time.monotonic() - start
        if (sent == 0 and elapsed > 1) or (sent == 1 and elapsed > args.seconds - 2):
            device.write(b'stats\n')
            sent += 1
        line = device.readline(1024).decode('utf-8', errors='replace')
        if line.startswith(('hashes=', 'payout_verified=', 'SECURITY:')) or 'Pool accepted share' in line or 'Pool rejected share' in line:
            print(line, end='', flush=True)
        elif line:
            skipped += 1
    print(f'Status capture complete; skipped {skipped} non-status/partial lines.')
