# Verification report — 13 October 2026

This report distinguishes executed local tests from checks requiring the ESP32. The initial build occurred without a board connected. After the user connected the device and explicitly requested flashing, the following hardware checks ran on COM5.

## Connected-board follow-up

* CP210x USB-to-UART device identified on **COM5**. Esptool detected **ESP32-D0WD-V3 revision v3.1**, dual core, 40 MHz crystal and **4 MB physical flash**.
* Dual-worker firmware uploaded successfully at 115200 baud, with esptool verifying the written image hashes. No full-chip erase was performed; flashing erased only the programmed image sectors.
* Upload inspection exposed PlatformIO's standard `boot_app0.bin` write at 0xe000 overlapping the original NVS allocation. The final layout limits NVS to 0x9000–0xdfff and reserves 0xe000–0xffff for boot metadata. The corrected layout was compiled and reflashed successfully. Both packaged variants now contain matching partition tables and boot_app0 images.
* A controlled restart and 15-second serial capture showed `SHA reference, optimized and mbedTLS self-tests PASS`, a responsive `stats` command and setup AP startup. The OLED absence/failure message was not emitted; visual display readability still requires user confirmation.
* On-device startup benchmark: **38.47 KH/s optimized software**, **13.58 KH/s mbedTLS hardware-assisted backend**, 1,024 real hashes per backend on one core. These are startup microbenchmarks, not sustained dual-worker mining throughput.
* The device is waiting for initial setup, so zero mining hashes/shares is expected. `stats` reported free heap 146,680 bytes and minimum heap 146,416 bytes during this capture. This is one startup observation, not a long-duration memory test.
* The SDK logged that no flash core-dump partition is configured. It did not prevent startup; crash diagnostics remain available over USB serial rather than persisted flash dumps.
* Evidence is in `docs/hardware-boot.log` (generated locally and ignored by source control). Wi-Fi credentials and a receiving address have not been supplied; persistence, real jobs, live authorization, accepted shares and sustained recovery testing remain pending.

## Initial local verification (before hardware follow-up)

* PlatformIO release build for classic ESP32 DevKit V1, `esp32-oled`: success. Final sizes are recorded in `build-manifest.json`; application partition capacity is 3,145,728 bytes, configured flash 4 MB. Actual board flash remains unmeasured.
* Native test runner: **19 tests passed**, using the actual `lib/MiningCore/src` C++ files and independent Python oracles. The tests include hundreds of subcases.
* SHA-256/SHA-256d: empty input, abc, padding-boundary lengths and long input, against Python hashlib.
* Optimized SHA-256d: the Bitcoin genesis header, six nonce extremes and 256 deterministic random 80-byte headers against hashlib. Full digest tested, including hashes that do not satisfy the upstream sixteen-bit prefilter.
* Exact difficulty targets: 108 values, including fractional pool difficulties, minimum/maximum supported values and 100 randomized binary64 values against Python Fraction integer division. Wire decimal conversion also checked, avoiding ArduinoJson exponent-rounding errors. Supported difficulty interval: 2^-64 through 2^256; targets above uint256 saturate, and invalid/out-of-range difficulty causes a reconnect.
* Compact nBits decoding: Bitcoin difficulty-one, low exponents, uint256 edge, negative/sign, zero and overflow cases.
* Full 256-bit little-endian comparison: target−1, target, target+1, maximum hash and opposing high/low bytes. Best difficulty calculation checked independently.
* Header/coinbase/merkle construction: nontrivial previous-hash bytes, two merkle branches and a 2,400-byte coinbase that exceeds the inspected upstream fixed buffer; byte-for-byte comparison with independent Python construction.
* Stratum parsing: representative valid notifications, malformed/missing/type-invalid fields, odd/nonhex strings, excessive branches and extranonce sizes 1/2/4/8/16. These are protocol fixtures, not claims of accepted mainnet work.
* Share formatting: all five parameters, JSON escaping, exact 8-digit nonce hex, newline termination and nonce extremes.
* Responses: acceptance requires boolean true with no error; false, missing result, string true and explicit errors fail. ID correlation and bounded pending slots were code reviewed in the network task; live acceptance has not run.
* Connection policy: capped retry sequence, reset behavior, ready/unready timeout conditions and uint32 timer wrap.
* Address checksums: published mainnet Base58Check/Bech32/Bech32m vectors, corruption, mixed-case and testnet rejection. These fixture addresses are not payout defaults.
* Live read-only endpoint probe: `public-pool.io:21496` responded successfully to `mining.subscribe` at 10:35:36 UTC / 12:35:36 SAST on 13 October 2026, with extranonce1 size 4 and extranonce2 size 8. Evidence: [pool-probe.json](pool-probe.json). No authorization or submission occurred.
* Source/config inspection: SSD1306 128×64, 0x3C, explicit GPIO21/22, 1,000 ms refresh, no BOOT handler, no automatic upload target or OTA endpoint, distinct task/socket ownership, protected shared statistics, bounded 16-share and 16-pending structures. Startup hash self-tests and benchmark are compiled but have not executed on the ESP32.

## Remaining hardware acceptance checklist

Firmware 1.1 update: the native suite now has **30 passing tests**. Eleven new payout/parser groups check mainnet address-to-script conversion, all-reward payment, full/partial redirection, even a one-satoshi third-party fee, wallet bytes hidden in inputs, same-wallet split outputs, zero rewards, every truncated prefix, trailing bytes, non-coinbase inputs, witness serialization, noncanonical CompactSize, overflow, output bounds, extended script lengths and deterministic mutation invariants. The existing 19 cryptographic/protocol tests also pass.

The update was uploaded on COM5 using only the application region after verifying the existing partition table. The saved configuration reloaded successfully: Wi-Fi/pool connected without setup, on-device SHA tests passed, `payout_verified=1`, no blocked jobs, and real hashes advanced at about 31 KH/s. Assigned difficulty was 1. No accepted/rejected shares or network-target candidates were observed during these samples. See [the updated audit](SECURITY-AUDIT.md) for evidence and remaining limitations.

| Check | Procedure / pass evidence |
| --- | --- |
| Flash capacity / board identity | **Passed on COM5:** classic ESP32-D0WD-V3 and 4 MB flash. |
| Startup cryptography | **Passed:** reference, optimized and mbedTLS self-tests; both startup benchmark rates captured above. |
| OLED | Confirm startup status, setup key, both dashboard pages, all counters and no visible clearing/flashing. Disconnect OLED with power removed, reboot, confirm absent-device log and continued setup/mining. |
| Portal security | Connect only to protected AP, verify unauthenticated HTTP prompts, wrong password rejected, wrong form token rejected, and page inaccessible from normal STA address. |
| Configuration save / reload | **Reload passed after application-only update and reset:** existing Wi-Fi/pool/public-address settings worked, with verified live payout and no setup prompt. Power-loss persistence and intentional reset-command testing remain separate checks. |
| Invalid configuration | Corrupted/invalid addresses rejected before NVS write. Invalid Wi-Fi leads to 15 s retries and recovery AP after 60 s. Correct settings and confirm recovery. |
| Reset without erase | Send `reset-config`, observe restart and fresh setup. Confirm no full flash erase and BOOT has no settings-reset behavior. |
| Pool authorization / real jobs | **Passed over USB diagnostics:** authorized pool, recent jobs, advancing actual hashes, about 31 KH/s and payout-verified templates. Pool dashboard and visual OLED confirmation remain optional user checks. |
| Real accepted/rejected shares | Wait for a naturally qualifying share, preserve submission response and pool dashboard evidence. Change to a controlled LAN test server for intentional rejection tests; do not manufacture mainnet submissions. |
| Wi-Fi recovery | Disable router for 90 s; ensure worker stops, recovery AP appears, watchdog remains healthy; restore router and confirm new subscription/job and advancing hashes. |
| Pool recovery | Stop a LAN pool or temporarily block the configured endpoint. Observe capped backoff, paused work, restart after restoration and clearing of pending state. Test disconnect during handshake too. |
| Malformed network / limits | LAN fixture server sends invalid JSON, oversized frames, wrong extranonce size, bad hex, set_extranonce and refused authorization. Verify safe reconnect, no hash fabrication or heap drift. |
| New jobs / stale work | Controlled pool issues jobs during active hashing; verify workers change generation and no locally obsolete queued share is transmitted. Already submitted response IDs still update the correct counters. |
| Synchronization and memory | 24-hour run with `stats` sampled every minute, stable free/minimum heap, no watchdog/brownout/restart, no growing pending/queue memory. Hardware stack high-water inspection may be added if needed. |
| Performance | Compare five-minute rolling rates for `esp32-single` and `esp32-oled`, same pool/network. Record startup software and hardware-assisted benchmarks separately. No expected performance value is asserted. |

## Limitations and completion boundary

All local crypto/protocol checks and compilation are complete. Flashing, physical board identification, startup SHA self-tests and the single-core startup benchmarks also ran on the connected device. Physical persistence, Wi-Fi/portal recovery, sustained task scheduling, visual OLED behavior, live authorization and live accepted-share behavior remain acceptance checks. No sustained mining hashrate, real share or block discovery is claimed for your board.

The transport is plaintext Stratum V1, as used by the configured endpoint. The recovery portal relies on WPA2/local AP access and HTTP authentication, not HTTPS. NVS is not flash-encrypted. The bundled framework's HTTP request parser is trusted for local setup and can allocate request data before handler validation; keep the AP key private. Hardware stress testing is needed before describing the device as proven for unattended operation.

## Reproduce

`pio run -e esp32-oled -e esp32-single` builds both variants. `python tools/run_tests.py` rebuilds the native harness and runs the suite. `python tools/probe_pool.py` repeats subscription-only endpoint verification; availability may change. The README contains exact Windows upload instructions and local serial commands.
