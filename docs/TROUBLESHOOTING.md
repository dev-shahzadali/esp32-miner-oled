# Troubleshooting

| Symptom | Check/action |
| --- | --- |
| No CP2102 COM port | USB data cable, another USB port, Device Manager and the CP210x driver from Silicon Labs. COM3 Intel AMT is unrelated. |
| Upload stalls at Connecting | Close other serial programs; hold BOOT during initial connection, release once upload starts; use 115200 baud. |
| Blank OLED | Check 3V3/GND, GPIO21/22, address 0x3C and SSD1306 controller. `OLED absent at 0x3C` means address ACK or initialization failed. Enter `setup-key` to continue setup over USB. |
| SHA self-test failure | Firmware deliberately stops mining; preserve serial diagnostics and rebuild this pinned environment before attempting mining. |
| Wi-Fi does not connect | Use 2.4 GHz SSID, correct credentials and adequate USB power. After 60 seconds join ESP32-LotteryMiner and save corrected settings. WPA2 personal or open networks are supported; enterprise authentication is not configured. |
| Cannot open portal | Join the miner AP, disable VPN/cellular auto-routing temporarily if needed, open http://192.168.4.1/ and use admin plus the displayed one-time key. The key changes after reboot. |
| Invalid receiving address | Copy native mainnet Bitcoin receive address from Phantom. Other chains, testnet, mixed-case Bech32 and incorrect checksums are rejected. Never enter a seed phrase. |
| Pool authorization refused | Check public receiving address, worker and pool password. Try the selected pool's supported native Bitcoin address type. Network reachability alone does not prove authorization. Use `setup` to change settings. |
| Invalid pool message | Malformed JSON, excessive sizes, invalid job parameters or unsupported extranonce update. Device abandons work and reconnects. Confirm a Bitcoin SHA-256d Stratum V1 endpoint. |
| BLOCKED: payout mismatch | The reward transaction is malformed or pays a positive amount outside the saved address. Firmware 1.1 refuses those jobs, including pool fees. Check the saved address and pool policy; no fee bypass is enabled. |
| Pool session timeout | Handshake over 20 s, no successful receive for 90 s, or no new job for 180 s. Firmware reconnects with capped backoff. |
| No accepted shares yet | Low-power mining is probabilistic. Check actual hash count, recent jobs and pool difficulty. Pool shares require qualifying hashes and can be infrequent at excessive difficulty. No invented submissions are sent. |
| Public Pool assigns difficulty 1 | Observed on the connected board and reported in [Public Pool issue #152](https://github.com/benjamin-wilson/public-pool/issues/152). A suggestion is not binding on the pool. At about 31 KH/s, difficulty-1 shares are infrequent; the miner must continue comparing against the assigned target. |
| Hashrate zero | Setup/no Wi-Fi/no authorized job, disconnection or task failure. Use `stats` and serial status. An absent OLED does not itself stop hashing. |
| Dropped shares | Queue or 16 outstanding responses saturated, usually an unusually easy target or unresponsive pool. No unbounded queue is allocated. |
| Stale count increases | A job superseded a queued candidate or connection recovery abandoned an unresolved submission. This count is separate from pool rejections. |
| Reboot loop | Check stable power, startup SHA errors, watchdog messages and heap. Use a data cable and log 115200 output. Do not assume a firmware bug from a supply brownout. |
| Need fresh setup | USB command `reset-config`, Enter. It clears only this application's NVS namespace. No full flash erase is needed. |

Routine logs do not print Wi-Fi/pool passwords or receiving addresses. The explicit local USB command `setup-key` reveals only the temporary portal key; treat any recording of that response as private until the next restart.
