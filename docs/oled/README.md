# SSD1306 OLED Display

This project uses an external **0.96-inch, 128 x 64 SSD1306 OLED with a four-pin I2C interface** on a classic ESP32 DevKit V1-compatible board. The display was previously tested by the project owner with the Adafruit SSD1306 and Adafruit GFX libraries.

## Display specifications

| Setting | Configuration |
| --- | --- |
| Screen size | 0.96 inch |
| Controller | SSD1306 |
| Resolution | 128 x 64 pixels |
| Graphics | Monochrome bitmap |
| Interface | I2C |
| I2C address | `0x3C` |
| Supply | ESP32 `3V3` |
| SDA | GPIO 21 |
| SCL | GPIO 22 |
| Dedicated reset GPIO | None (`-1` in the driver) |
| Bus clock | 400 kHz |
| I2C timeout | 50 ms |
| Refresh interval | Approximately 1 second |
| Dashboard page interval | 8 seconds |

The module's manufacturer and exact product number have not been recorded. Use the controller, resolution, interface and address above to identify a matching module.

## Wiring

Disconnect power before changing any connections. Read the labels on your OLED module; pin order can differ between modules.

| OLED pin | ESP32 pin |
| --- | --- |
| VCC | **3V3** |
| GND | **GND** |
| SDA | **GPIO 21** |
| SCL | **GPIO 22** |

Use the existing tested wiring. This project powers the OLED from **3.3 V**, with a shared ground and short I2C wires. GPIO 21 and 22 are reserved for the display bus; the firmware does not use them for a status LED or a reset button.

See [the wiring document](../WIRING.md) for board details.

## Driver and initialization

The display task is implemented in [src/oled.cpp](../../src/oled.cpp). The project pins these libraries in [platformio.ini](../../platformio.ini):

- Adafruit SSD1306 `2.5.13`
- Adafruit GFX Library `1.11.11`
- Adafruit BusIO `1.17.4`

The relevant initialization from the firmware is:

```cpp
Wire.begin(21, 22);
Wire.setClock(400000);
Wire.setTimeOut(50);
Wire.beginTransmission(0x3C);

Adafruit_SSD1306 oled(128, 64, &Wire, -1, 400000, 400000);
bool present = Wire.endTransmission() == 0 &&
               oled.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false);
```

The firmware first checks for an I2C acknowledgement at `0x3C`. The last argument to `begin()` is `false` so the library uses the already configured Wire bus. Text uses Adafruit GFX's built-in small bitmap font at size 1, with wrapping disabled.

A dedicated FreeRTOS task handles drawing. It copies the shared statistics under a mutex, releases that mutex, then updates the display. I2C transfers therefore run outside the mining statistics lock. The screen is drawn into a RAM buffer before `display()` sends the frame; clearing that buffer does not issue a separate physical clear-screen command between frames.

## Dashboard

The title is **NERDMINER ESP32**. Two dashboard pages rotate every eight seconds.

| Page | Fields |
| --- | --- |
| Mining | Measured KH/s, rolling average KH/s, Wi-Fi and pool status, accepted/rejected shares, best computed hash difficulty |
| Diagnostics | Uptime, stale/dropped shares, Wi-Fi RSSI, payout-check status, free heap, current miner status |

These are illustrative layouts, not promised performance or actual earnings:

```text
NERDMINER ESP32
---------------------
HASH   31.00 KH/s
AVG    30.95 KH/s
WIFI:ON POOL:ON
SHARES 0 / 0
BEST 0.00119
```

```text
NERDMINER ESP32
---------------------
UP 0:01:16
STALE 0 DROP 0
RSSI -55 PAY:OK
HEAP 141 KB
Mining
```

- `HASH` and `AVG` come from completed job hashes and elapsed time. Startup test and benchmark hashes are excluded.
- `SHARES` shows **accepted / rejected pool submissions**. An accepted share is not a Bitcoin block discovery.
- `BEST` is the highest difficulty calculated from an actual job hash; it is not an earnings value.
- `PAY:OK` means the current job passed the payout check against the **saved receiving address**. All positive-value reward outputs must pay that address. `PAY:WAIT` means no currently verified job is available.
- `BLOCKED: payout mismatch` means the firmware refused a malformed or redirected reward transaction. The small display may clip the end of a long status message; USB serial provides the full message.

Firmware 1.1 also reports payout verification, blocked jobs, assigned pool difficulty and network-target candidates through the USB `stats` command. See [the main README](../../README.md) for their meanings.

## Initial setup screen

When setup or recovery mode is active, the dashboard displays:

- Wi-Fi access point: **ESP32-LotteryMiner**
- Configuration page: **http://192.168.4.1/**
- A temporary 16-character setup key
- An instruction to save the configuration and restart

Use that key to join the access point and to log into the page with username **admin**. The key changes after restart and should be kept private while the portal is active. The screen returns to the mining dashboard after you save settings and the device restarts.

## Missing display and troubleshooting

The miner continues operating if no OLED acknowledges `0x3C` or driver initialization fails. USB serial reports:

```text
OLED absent at 0x3C; mining continues
```

Display detection occurs at startup. Power down before repairing connections, then reboot so the firmware checks the OLED again.

| Symptom | Check |
| --- | --- |
| Completely blank screen | Verify 3V3, common ground, SDA on GPIO 21, SCL on GPIO 22, and the module's pin labels. |
| OLED absent message | Confirm the module uses address `0x3C` and an I2C interface; check for loose connections. |
| Image shifted, clipped or scrambled | Confirm the controller is SSD1306 and the panel is 128 x 64. An SH1106 panel or 128 x 32 display needs different driver configuration. |
| Intermittent display updates | Check short wires and a stable USB power source. Inspect USB serial for resets or I2C-related symptoms. |
| Display works but hashrate is zero | Check Wi-Fi, pool authorization, recent jobs and payout status. The display does not generate mining work. |
| Setup needed without a working OLED | Open USB serial at 115200 baud, send `setup`, then `setup-key` to explicitly retrieve the temporary key locally. |

See [the troubleshooting guide](../TROUBLESHOOTING.md) and [verification report](../TESTING.md) for firmware and hardware checks. Visual readability and screen behavior require observation of the physical panel; serial diagnostics alone do not prove what is visible on it.
