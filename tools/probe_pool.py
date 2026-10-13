"""Read-only Stratum subscribe probe. Never authorizes or submits a share."""
import argparse
import json
import socket
import time
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--host', default='public-pool.io')
parser.add_argument('--port', type=int, default=21496)
args = parser.parse_args()
report = {'host': args.host, 'port': args.port, 'time_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), 'authorized': False, 'submitted': False}
try:
    with socket.create_connection((args.host, args.port), timeout=10) as sock:
        sock.settimeout(10)
        sock.sendall(b'{"id":1,"method":"mining.subscribe","params":["ESP32LotteryMiner/1.0"]}\n')
        reader = sock.makefile('rb')
        for _ in range(8):
            line = reader.readline(16385)
            if not line or len(line) > 16384:
                raise RuntimeError('Connection closed or oversized message')
            message = json.loads(line)
            if message.get('id') == 1:
                result = message.get('result')
                if message.get('error') or not isinstance(result, list) or len(result) != 3:
                    raise RuntimeError('Invalid subscription response')
                report['subscribe_ok'] = True
                report['extranonce1_bytes'] = len(bytes.fromhex(result[1]))
                report['extranonce2_bytes'] = result[2]
                break
except Exception as exc:
    report['subscribe_ok'] = False
    report['error'] = str(exc)
Path('docs/pool-probe.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
