# Wiring

Use the already tested wiring: SSD1306 VCC to ESP32 **3V3**, GND to GND, SDA to **GPIO 21**, SCL to **GPIO 22**. Display address **0x3C**, resolution **128×64**, controller SSD1306, no reset GPIO. The configured bus is 400 kHz with a 50 ms I²C timeout.

The Adafruit driver calls `begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false)` after `Wire.begin(21,22)`. The final `false` prevents library code from replacing the chosen bus configuration. An address ACK is required before initialization; missing devices do not halt mining. A compatible SH1106 screen is not interchangeable with SSD1306 without a driver change.

Disconnect power before rewiring. Use a stable USB power source and short I²C wires. Never connect OLED VCC to 5 V for this project. Check module silkscreen rather than assuming pin order. No mining reset action is attached to GPIO 0 / BOOT, and no status LED is assigned to GPIO 21 or 22.

The board is configured as DOIT DevKit V1, classic dual-core ESP32. A chip/flash capacity log is emitted at startup. Esptool's `flash_id` query on COM5 verified ESP32-D0WD-V3 revision v3.1 and **4 MB physical flash** on the connected board before the authorized upload.
