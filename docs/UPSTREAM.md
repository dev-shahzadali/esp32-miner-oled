# Source provenance and engineering decisions

Inspected on **13 October 2026**:

| Project | Inspected revision | License / use |
| --- | --- | --- |
| [BitMaker-hub/NerdMiner_v2](https://github.com/BitMaker-hub/NerdMiner_v2) | `e3a04b7d9f0b6e60ca6e02f7433c94d78d9dded4`, main | MIT, Copyright 2023 Bitmaker. Optimized SHA source copied; job construction adapted. |
| [paelzer OLED fork](https://github.com/paelzer/NerdMiner_v2_paelzer/tree/support-SSD1306-I2C-display) | `5a75c6a2220956e74c88268059cae2975183f518` | MIT. Inspected driver and board configuration; no source copied from this fork. |
| [Public Pool server](https://github.com/benjamin-wilson/public-pool) | `96a9202c11de2c6fc8d41155e2e779912a476dc7` | GPL-3.0. Protocol/output behavior inspected only; no source linked or copied into firmware. |
| [Blockstream Jade](https://github.com/Blockstream/Jade) | NerdMiner's SHA comments identify Jade as its original basis | MIT, Copyright 2021 Blockstream Corporation Inc. Attribution retained, additional notice included. |

The exact requested branch `ESP32_devkit_C_SSD1306` is absent from the fork's current remote heads. Available branches include `SSD1306` and `support-SSD1306-I2C-display`; the latter was inspected rather than claiming the missing branch was fetched. Its driver already selects 128×64 and 0x3C, but uses default Wire pins and an infinite halt on display initialization failure. This project explicitly selects GPIO21/22 and continues without a display.

Direct reuse of the upstream application would not meet this request: the inspected Stratum authorization routine prints its full outgoing payload including password, job fields have weak validation and borrow JSON array lifetimes, coinbase construction uses a small fixed buffer, and upstream behavior filters most SHA outputs on sixteen zero bits. That filter complicates arbitrary easy-target support and exact best-hash statistics. UI/cloud/settings code also brings dependencies and behavior not needed here.

Copied files: `lib/MiningCore/src/nerdSHA256plus.cpp` and `.h`, preserving Bitmaker/Jade/LarryBitcoin attribution. Changes: conditional Arduino attributes for native tests; unused ESP includes removed; NDEBUG guarded; both early sixteen-bit filters removed to produce complete digests for arbitrary targets. No hashing constants or baked-round calculations were altered. Native randomized differential tests verify the changed optimized path against Python hashlib; boot self-tests also compare it with the separate reference implementation.

The coinbase double hash, iterative merkle fold, word-swapped previous hash and little-endian numeric header fields follow the inspected upstream `utils.cpp` construction and match Public Pool's `MiningJob.ts`. New code validates bounds and hex, builds a complete 80-byte header and performs full integer target comparison. The original upstream target helpers and lifetime-dependent job structs are not used.

The remaining network/session, security, display, NVS, tasks and portable reference modules are new code written for this adaptation. The resulting implementation has a larger custom Stratum layer than initially preferred, because retaining the inspected protocol routines would retain credential logging and input/response correctness defects. Those routines provided a behavior reference; the actual parsing/formatting helpers are tested locally.

Protocol evidence: [Public Pool MiningJob.ts](https://github.com/benjamin-wilson/public-pool/blob/96a9202c11de2c6fc8d41155e2e779912a476dc7/src/models/MiningJob.ts), [StratumV1Client.ts](https://github.com/benjamin-wilson/public-pool/blob/96a9202c11de2c6fc8d41155e2e779912a476dc7/src/models/StratumV1Client.ts), and [BIP350](https://github.com/bitcoin/bips/blob/master/bip-0350.mediawiki). Public Pool supports P2PKH, P2SH, P2WPKH, P2WSH and P2TR payouts in the inspected server. Deployment version is unknown; successful subscription does not establish the live server's revision or your address's authorization.

## Pinned dependencies

| Dependency | Version | License |
| --- | --- | --- |
| PlatformIO Espressif32 platform | 6.6.0 | Apache-2.0 platform tooling |
| Arduino-ESP32 framework | 2.0.14 (`3.20014.231204` package) | LGPL-2.1 Arduino components and included third-party licenses |
| ESP-IDF included SDK | 4.4.6 | Apache-2.0 plus SDK third-party components |
| Xtensa ESP32 GCC | 8.4.0 + 2021r2-patch5 | GCC/toolchain licensing |
| ArduinoJson | 6.21.5 | MIT |
| Adafruit SSD1306 | 2.5.13 | BSD |
| Adafruit GFX | 1.11.11 | BSD |
| Adafruit BusIO | 1.17.4 | MIT |
| Native-test-only Zig | 0.13.0 | MIT compiler plus bundled toolchain licenses |

NerdMiner's MIT notice is in the root LICENSE; independent library notices are retained under `docs/licenses/`. Framework package notices remain with the installed SDK. Preserve the framework's LGPL and included third-party obligations when distributing binaries; the editable application source and build instructions are included here. Public Pool GPL source is research-only and not a firmware dependency.
