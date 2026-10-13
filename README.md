# ESP32 Bitcoin Lottery Miner

Real Bitcoin mainnet SHA-256d mining for an ESP32-D0WD / DOIT DevKit V1 and an external SSD1306 OLED. This is a hobby lottery miner: accepted pool shares are proof of work sent to a pool, **not Bitcoin block discoveries or earnings**. No sample wallet is configured.

**Payout protection (firmware 1.1):** every job's coinbase is parsed and all positive-value outputs must pay the saved Bitcoin address. Redirected rewards, any pool/developer fee and malformed coinbases are refused before hashing. Zero-value outputs such as witness commitments are allowed. `PAY:OK` on the OLED and `payout_verified=1` in USB statistics identify verified jobs. This check protects reward destinations; plaintext Stratum still lacks authenticated server identity, and the device cannot force a pool to relay a winning block or validate all transactions from a Stratum V1 job. See the [payout and live-mining audit](docs/SECURITY-AUDIT.md).

The complete firmware builds with PlatformIO. Local cryptographic and protocol tests pass, and a read-only subscription to `public-pool.io:21496` succeeded on 13 October 2026. The user subsequently authorized flashing the connected board on **COM5**: 4 MB flash was verified, upload succeeded and startup SHA self-tests passed. **NVS persistence, Wi-Fi recovery, sustained mining hashrate and accepted shares still require setup and further physical verification.** See [test report](docs/TESTING.md).

## Hardware and wiring

| OLED pin | ESP32 connection |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

SSD1306: **128 × 64**, I²C **0x3C**, no dedicated reset pin. No wiring changes. See the [OLED README](docs/oled/README.md) for the 0.96-inch module, driver settings, dashboard and troubleshooting, or [wiring details](docs/WIRING.md) for the connections.

## Windows / VS Code build and upload

Install the PlatformIO IDE extension in VS Code and open this folder. The project uses pinned Espressif32 6.6.0 / Arduino-ESP32 2.0.14 and the `esp32doit-devkit-v1` profile at its standard 240 MHz. It requires 4 MB or larger flash and uses a 3 MB factory application partition, without OTA.

In VS Code's PlatformIO terminal:

```powershell
pio run -e esp32-oled
pio device list
```

If `pio` is unavailable in an ordinary PowerShell terminal, use the installed executable:

```powershell
Set-Location 'C:\Users\sania\Desktop\Projects\Nerdminer'
$minerPio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
& $minerPio run -e esp32-oled
& $minerPio device list
```

Connect the board with a USB data cable. Locate its **CP2102** COM port in Device Manager or `device list`. COM1/COM2 legacy ports and Intel AMT COM3 are not this board. Replace `COM7` below with the identified port. Close Arduino Serial Monitor before uploading.

First verify flash capacity with the following read-only query. It may reset the board into its bootloader but does not erase or write flash:

```powershell
$minerPython = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
$minerEsptool = Join-Path $env:USERPROFILE '.platformio\packages\tool-esptoolpy\esptool.py'
& $minerPython $minerEsptool --chip esp32 --port COM7 --baud 115200 flash_id
```

Proceed only if at least 4 MB is detected. The firmware also reports the physical chip capacity at startup and disables mining if it is below 4 MB. This project's 4 MB layout remains usable on larger flash chips.

When you choose to flash the board:

```powershell
& $minerPio run -e esp32-oled -t upload --upload-port COM7
& $minerPio device monitor --port COM7 --baud 115200
```

PlatformIO uploads the bootloader, partition table, boot metadata and application. A first upload replaces your existing Arduino firmware. The coding agent uploaded the dual-worker firmware to COM5 after explicit user authorization, without a full-chip erase. No separate erase command is required. If upload stalls at `Connecting...`, hold BOOT while connecting/uploading, then release when writing begins. BOOT is used only for the ESP32's ROM bootloader; it has **no configuration-reset behavior in this firmware**.

Built application: `.pio/build/esp32-oled/firmware.bin`. Use PlatformIO upload so the correct bootloader and partition table accompany it.

Packaged copies are under [firmware/esp32-oled](firmware/esp32-oled) and [firmware/esp32-single](firmware/esp32-single). Firmware 1.1 dual-worker build: **871,953 bytes linked flash**, **45,712 bytes static RAM**, **878,256-byte application binary**. Sizes exclude task stacks and dynamic allocations. SHA-256 checksums and companion image offsets are in [build-manifest.json](docs/build-manifest.json).

## Initial setup

1. On first boot, connect your phone/computer to **ESP32-LotteryMiner**. Use the fresh 16-character password shown on the OLED. If the display is absent, send `setup-key` followed by Enter over the local USB serial monitor to explicitly reveal that temporary key.
2. Browse to **http://192.168.4.1/**. Use HTTP username **admin** and the same temporary key as password. The portal binds to the AP address and accepts only AP-subnet clients. It has no remote cloud dependency.
3. Enter your 2.4 GHz Wi-Fi SSID/password, pool hostname/port/password, **public Bitcoin mainnet receiving address**, and worker name. Default pool: `public-pool.io:21496`; password `x`; worker `esp32`.
4. Save. The validated settings are written as one versioned NVS record and the board restarts. The AP shuts down after restart while normal Wi-Fi connects.
5. Watch USB serial for SHA self-test success, measured startup benchmark, pool connection and mining status. Run `stats` to inspect counters. An actual pool authorization response confirms support for your address at that endpoint.

In Phantom, select Bitcoin mainnet and copy its **native Bitcoin receive address**. Never enter a recovery phrase, private key, Ethereum address or wrapped-BTC token address. The firmware validates Base58Check mainnet P2PKH/P2SH, Bech32 v0 P2WPKH/P2WSH and Bech32m v1 Taproot checksums and witness sizes. Public Pool's inspected server implementation supports these output types; the deployed pool must still authorize your particular receiving address. Source contains only published address test vectors, never your receiving address.

The local setup AP uses WPA2 with a random key generated on each boot/portal activation, HTTP authentication and a random per-session form token. Wi-Fi and pool passwords are never logged or rendered back in forms. HTTP runs inside this local protected AP; NVS is not encrypted against physical flash extraction. Do not forward ports or expose the AP service to the Internet. The temporary key is printed only if you explicitly request `setup-key` via USB.

## Recovery and controls

Send commands followed by Enter at 115200 baud:

| Command | Effect |
| --- | --- |
| `stats` | Actual hash count/rates, accepted/rejected/stale/dropped shares, best difficulty, uptime, connection ages, free heap, payout verification, blocked jobs, assigned share difficulty and network-target candidates |
| `setup` | Start the local protected AP to replace configuration; existing mining may continue until saved |
| `setup-key` | Explicitly reveal the one-time setup key locally over USB |
| `reset-config` | Clear only the `lottery` NVS namespace, then restart into setup |

Wi-Fi retries every 15 seconds. After 60 seconds disconnected, recovery setup becomes available automatically and Wi-Fi retries continue. Once opened, the portal remains available until a saved configuration causes restart. Pool failures use retry delays of 1, 2, 4, 8, 16, 32 and 60 seconds. Read [troubleshooting](docs/TROUBLESHOOTING.md) for failure diagnostics.

## Display and real statistics

The 6×8 bitmap font shows two pages that rotate every 8 seconds. Page 1: miner name, instantaneous and 30-sample average KH/s, Wi-Fi/pool state, accepted/rejected shares and best hash difficulty. Page 2: uptime, stale/dropped counts, RSSI, payout status, free heap and status. Setup replaces these with connection instructions. Refresh is once per second using a buffered frame; no physical clear-screen command is issued between frames. An absent OLED disables display updates while mining continues.

Hashrate comes from completed nonce hashes divided by elapsed monotonic time. Startup test and benchmark hashes are excluded from mining statistics. Best difficulty is computed from **every** actual job hash; it is not limited to accepted shares. Statistics reset at reboot. `stale` includes locally obsolete candidates and unresolved submissions abandoned on reconnect; `dropped` counts bounded-queue/backpressure losses. A rejected response is counted only for an outstanding submission ID. `network_candidates` counts independently rehashed headers that meet the advertised nBits network target; it does not claim blockchain acceptance or earnings. Such candidates are eligible for submission even if a pool assigns an unusually harder share target.

## Implementation and performance

The project adapts NerdMiner's MIT-licensed optimized `nerdSHA256plus` hashing implementation and its coinbase/merkle/header construction conventions. The requested fork branch no longer exists; the available `support-SSD1306-I2C-display` branch was inspected. Its OLED initialization can halt indefinitely on failure. Both upstream miners were considered, and direct transplantation was rejected because this request requires bounded input handling, exact targets, protected configuration and NVS persistence. See [upstream attribution and modifications](docs/UPSTREAM.md).

The firmware constructs coinbase hashes from both pool coinbase segments plus extranonce1 and full-width extranonce2, folds merkle branches with SHA-256d, word-swaps the Stratum previous hash, and serializes version/nTime/nBits/nonce little endian. It compares the full little-endian 256-bit hash with an integer share target. Difficulty is converted with exact integer division of the received binary64 value. Every qualifying candidate is independently rehashed before submission.

Network work owns the TCP socket and protocol state on core 0. Software SHA workers on cores 0 and 1 use nonoverlapping nonce sequences, immutable local templates, a generation counter, a protected template and a 16-entry fixed share queue. On each new job, older work is abandoned conservatively even when `clean_jobs` is false. Extranonce2 increments for new templates and on nonce exhaustion. `mining.set_difficulty` applies to the next job. Changed extranonce notifications cause reconnect/resubscribe. Frames, coinbases, branch counts and pending submissions have explicit bounds. The device accepts compatible Bitcoin SHA-256d Stratum V1 pools, not arbitrary altcoin or Stratum V2 services.

Every task is registered with the watchdog and yields regularly. Display I²C runs outside mining locks. Metrics and templates use separate mutexes; only the network task writes the pool socket. Heap and minimum heap are visible through `stats`. No overclocking, OTA endpoint, unsupported temperature/voltage values or external market-data service is included.

The pinned ESP32 SDK enables hardware SHA in mbedTLS. Startup checks it against reference SHA, then benchmarks 1,024 real SHA-256d hashes per backend. The default mining loop uses NerdMiner's optimized software midstate and baked rounds, allowing independent work on both cores without serializing on the single hardware engine. On the connected board, the startup benchmark measured **38.47 KH/s optimized software** and **13.58 KH/s mbedTLS** on one core. For a controlled sustained comparison, build/upload `esp32-single` and `esp32-oled` separately and compare five minutes of rolling rates at the same pool difficulty and Wi-Fi conditions. Startup benchmark results exclude network/display/yield overhead and are not displayed as sustained mining hashrate.

## Local tests

```powershell
python tools/run_tests.py
python tools/probe_pool.py
```

The 30-test suite compiles the actual portable C++ modules with a native compiler and uses Python `hashlib` and exact rational arithmetic as independent oracles. It also tests address-to-script decoding, full coinbase parsing, payout redirection/fee refusal, truncation and parser bounds. On this workspace a local Zig 0.13 compiler is installed under `tools/host`; on another machine install `g++` or recreate the local compiler with:

```powershell
python -m venv tools/host
& .\tools\host\Scripts\python.exe -m pip install ziglang==0.13.0
```

Build `esp32-oled` first so the pinned ArduinoJson headers are available. The pool probe subscribes only; it never supplies a wallet or submits a share. Native tests validate protocol helpers and connection policy, not physical Wi-Fi/NVS/FreeRTOS operation. Full verification steps and remaining acceptance checks are in [TESTING.md](docs/TESTING.md).

## Files

`src/`: main, workers, network, OLED, Wi-Fi portal, settings and statistics. `lib/MiningCore/src/`: portable SHA, header, target, address and protocol code. `tests/`: native C++ harness and independent Python tests. `tools/`: test runner and live subscription probe. `docs/`: attribution, wiring, troubleshooting and test evidence. `upstream/` is an ignored research checkout; the firmware builds without it.

Original source is MIT licensed, with third-party notices retained under [docs/licenses](docs/licenses). Linked framework/library licenses also apply when distributing compiled firmware.
