# Payout and mining audit — 13 October 2026

## Firmware 1.1 hardening

Following this audit, firmware 1.1 adds a bounded stripped-coinbase parser and standard mainnet address-to-script conversion. Every positive-valued coinbase output must match the saved wallet script, with a nonzero reward. Jobs with redirected payouts, any fee, malformed transaction data, noncanonical lengths, amount overflow, non-coinbase input, witness-inclusive serialization or trailing bytes are refused before publication to hashing workers. Zero-value commitment outputs are permitted. Thirty native tests passed, including eleven payout/parser test groups and truncation/mutation subcases.

`payout_verified`, `blocked_jobs`, assigned `pool_difficulty` and locally verified `network_candidates` are now exposed by the `stats` command. The OLED shows `PAY:OK` for verified jobs. A network-target candidate is not represented as a confirmed block or earnings. Firmware updates preserve the existing version-1 settings record; this update writes only the application partition.

Remaining limitations: the guard compares against the saved address, so the user still must ensure it is their intended Phantom receiving address. It does not provide TLS, secure boot, flash encryption, full-block validation or a guarantee that the pool will publish winning work. The original findings below describe firmware 1.0 and explain why this update was needed.

### Verified on the connected board

Firmware 1.1 was flashed to COM5 with an **application-only write at 0x10000**; esptool verified the written data hash. Before upload, the device's partition table was read and matched against the local table. NVS at 0x9000–0xdfff was outside the write/erase range and no configuration reset or schema change occurred.

After restart, the version banner identified `1.1 payout-guard`, startup SHA tests passed, saved Wi-Fi and pool authorization succeeded, and a real pool job passed `payout_verified=1`. Subsequent live samples showed hashes advancing from 1,967,712 to 2,344,800, about 31 KH/s, `blocked_jobs=0`, assigned `pool_difficulty=1`, and zero network candidates or accepted/rejected shares. This establishes successful settings reload and actual payout-checked mining, not a Bitcoin payout or confirmed block.

The packaged/flashed dual-worker application SHA-256 is `ff88da19befc0ecbc12ff3835f65a8f421f0afa5b853e54a6207d06674ecdca5`. Both variants compile and the 30-test suite passes. The single-worker variant was rebuilt but was not flashed.

The hosted pool continued to assign difficulty 1 despite a 0.0001 suggestion. [Public Pool issue #152](https://github.com/benjamin-wilson/public-pool/issues/152) reports the same behavior with official NerdMiner clients. That report supports this as a deployed-service limitation; it is not proof of the precise server configuration. At around 31,000 H/s, difficulty-1 shares average roughly 38 hours each under independent uniform hashing, with potentially much longer waits. This is a share probability estimate, not an earnings estimate or block-discovery prediction. The configured pool was left unchanged.

## Original audit result (firmware 1.0)

The flashed application contains no hardcoded receiving wallet, developer donation destination, automatic wallet transaction code, or private-key/seed handling. Authorization and submissions use the public receiving address saved in NVS plus the configured worker name. Only the configured Stratum host receives mining requests.

**Payout protection is incomplete:** the firmware hashes the coinbase supplied by the pool without decoding its output scripts and checking that they pay the configured receiving address. Supplying an address in `mining.authorize` does not itself enforce the reward destination. A malicious pool or a network attacker able to replace plaintext Stratum traffic can supply valid work paying another address. This concerns the potential reward from new mining work; the application cannot sign a transfer from the user's existing wallet because it has no private key.

The actual deployed pool's current coinbase destinations were not verified in this audit. The firmware does not currently expose either those outputs or the configured public address through its serial diagnostics. No NVS flash dump was taken, to avoid unnecessarily extracting Wi-Fi/pool passwords. A full payout audit needs the assembled coinbase for the exact job and comparison against the user's intended receiving address.

## Live device evidence

Read-only USB diagnostics on COM5, without requesting reset:

| Measurement | First sample | Second sample |
| --- | --- | --- |
| Actual completed job hashes | 6,142,336 | 6,514,112 |
| Uptime | 203 s | 214 s |
| Instantaneous KH/s | 30.799 | 31.195 |
| Rolling KH/s | 31.069 | 31.046 |
| Wi-Fi connected | Yes | Yes |
| Pool authorized | Yes | Yes |
| Accepted / rejected | 0 / 0 | 0 / 0 |
| Stale / dropped | 0 / 0 | 0 / 0 |
| Best computed hash difficulty | 0.0011801561 | 0.0011801561 |
| Free / minimum heap | 144,516 / 125,140 bytes | 144,516 / 125,140 bytes |

These counters demonstrate advancing real hashing on pool-supplied jobs. Startup optimized/reference/m­bedTLS SHA checks also passed earlier on this board. **No accepted share, valid Bitcoin block discovery, or payout is established by these samples.** Pool authorization and increasing hashrate alone do not independently prove reward destinations, mainnet job validity, or pool honesty. The requested share difficulty is 0.0001, but the flashed diagnostics do not expose the actual job target assigned by the pool, so accepted-share timing cannot be inferred reliably from that request.

The local build and packaged firmware match SHA-256 `b185b079f62a9ff45da475cb186050752e3cc36e92bb5d32b0372ed2dda95d8e`. Esptool verified writes during the authorized flash. This check is not secure boot or a fresh complete flash readback.

## Controls inspected and remaining risks

* Public-address checksum validation supports standard Bitcoin mainnet output types. It validates format, not ownership. The user must compare the saved address with Phantom's receiving address.
* Setup uses a random WPA2 AP key, HTTP authentication and a random form token, binds to the AP IP, and checks the client subnet. Wi-Fi and pool passwords are not printed by routine application logs or returned in configuration forms. The explicit USB `setup-key` command reveals only the temporary provisioning key.
* NVS stores credentials without flash encryption. Physical access can permit extraction or firmware replacement. Secure boot and signed updates are not configured.
* Stratum uses plain TCP without TLS or authenticated server identity. The pool controls coinbase and block-template contents. There is no automatic remote-update endpoint or cloud telemetry service in this application.
* Bounds exist for frames, coinbases, merkle branches, fixed candidate queue and outstanding submissions. The inherited Arduino HTTP parser can allocate request data before application-handler validation; the setup AP is intended for trusted local access. This was a source audit, not a complete penetration test.
* A qualifying share is independently rehashed before being submitted. Acceptance is counted only when an outstanding submission receives boolean true without an error. Acceptance still is not a Bitcoin block discovery.

## Original hardening recommendation (implemented in 1.1)

Add a bounded Bitcoin coinbase-transaction parser, decode the configured receiving address to its scriptPubKey, compare every pool job's positive-value outputs against an explicit payout policy, and refuse mining when that policy is violated. Report the destination check, assigned pool difficulty and local network-target candidate status separately from pool acceptance. Any pool fee must be visible and explicitly allowed rather than silently accepted. Such a check protects against redirected jobs, but cannot force a pool to relay a winning block or verify all block transactions from a Stratum V1 header/merkle branch alone.

References: [Bitcoin mining guide](https://developer.bitcoin.org/devguide/mining.html), [inspected Public Pool payout construction](https://github.com/benjamin-wilson/public-pool/blob/96a9202c11de2c6fc8d41155e2e779912a476dc7/src/models/StratumV1Client.ts). The inspected pool source can include a configurable 1.5% developer-fee output under some conditions. Its deployment configuration/version and this user's live job outputs have not been independently established.
