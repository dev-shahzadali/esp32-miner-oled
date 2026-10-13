# Built images

The dual-worker image was flashed to the connected ESP32 on COM5 and passed startup SHA self-tests. Live mining remains pending Wi-Fi/payout configuration. `esp32-oled` uses two software hash workers; `esp32-single` uses one for a controlled throughput comparison and has only been compiled. Both target a classic ESP32 DevKit V1 with at least 4 MB flash and the same OLED wiring.

Use the PlatformIO upload instructions in the root README. For manual tools the matching image offsets are bootloader **0x1000**, partitions **0x8000**, boot_app0 **0xe000**, application **0x10000**. Keep each variant's files together. This partition table has a factory app and NVS, without an OTA update service. A reserved boot-metadata partition accommodates PlatformIO's standard boot_app0 write without overlapping NVS.

Build sizes and SHA-256 checksums: `docs/build-manifest.json`. Recreate this folder after building both environments with `python tools/package_firmware.py`. This packaging command never uploads firmware or opens a serial port.
