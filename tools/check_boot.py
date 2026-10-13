"""Reset the connected ESP32, capture startup diagnostics, then close its port."""
import argparse
from pathlib import Path
import time
import serial
import sys

sys.stdout.reconfigure(encoding='utf-8', errors='replace')

parser = argparse.ArgumentParser()
parser.add_argument('--port', required=True)
parser.add_argument('--seconds', type=float, default=15)
args = parser.parse_args()
lines = []
device = serial.Serial(port=None, baudrate=115200, timeout=0.2)
device.dtr = False  # Keep GPIO0 out of ROM download mode.
device.rts = False
device.port = args.port
with device:
    device.rts = True
    time.sleep(0.1)
    device.reset_input_buffer()
    device.rts = False
    start = time.monotonic()
    sent = False
    while time.monotonic() - start < args.seconds:
        text = device.readline().decode('utf-8', errors='replace')
        if text:
            print(text, end='', flush=True)
            lines.append(text)
        if not sent and time.monotonic() - start > 5:
            device.write(b'stats\n')
            sent = True
Path('docs/hardware-boot.log').write_text(''.join(lines), encoding='utf-8')
