# TV-B-Gone (Minimal ESP32-S3)

A battery-powered, one-shot **TV-B-Gone** for the [ESP32-S3 SuperMini](https://www.espressif.com/en/products/modules/esp32-s3). Flip a power switch and it blasts every known universal *power-off* infrared code at TVs and projectors around you — no buttons, no screen, nothing to press. When the ~6 second burst finishes the chip deep-sleeps until you cut the power again.

Inspired by [Bruce's TV-B-Gone](https://github.com/btaylor1/TV-B-GONE) and the [ir_esphack](https://github.com/kevinmcaleer/infrared) reference firmware.

## What it does

- On power-up it transmits, in order:
  1. **EU power-off codes** (~142) — tuned for Europe / Poland.
  2. **Universal parsed codes** (50) — Samsung, LG, NEC, RCA, Pioneer plus projector variants.
  3. **Universal raw codes** (13) — generic "most TVs", Xiaomi and other raw bursts.
- Each code is followed by a **20.5 ms gap** (same timing as Bruce), so the full blast is **~5–7 seconds**.
- Projectors are covered in the same pass, so there's one mode for everything.
- The North-America code bank exists in the database but is **not transmitted**, since it only matters outside Europe and would just add dead air.

After `setup()` finishes blasting, `loop()` calls `esp_deep_sleep_start()`, dropping current draw to ~0 µA-scale so a small battery lasts. There is no infinite loop — power OFF then ON re-runs the burst.

## Hardware (minimal)

| Part | Notes |
|------|-------|
| [ESP32-S3 SuperMini](https://www.espressif.com/en/products/modules/esp32-s3) | Main board |
| 940 nm high-power IR LED module (1 W / 3 W), 3-pin (5V / GND / Signal) | The emitter. Any ~940 nm IR LED array works. |
| Battery + SPST on/off switch | Powers the whole thing; the switch is your "button". |

**Wiring** — see [docs/schematic.svg](docs/schematic.svg):

```
ESP32-S3 SuperMini        IR LED module
  5V  (VIN)      <------->  5V
  GND            <-------->  GND
  GPIO4 / D4     <-------->  Signal (IN)
```

That's the entire circuit: three wires to the emitter, plus a battery and switch on the power rail. The SuperMini's regulator supplies 5 V from the battery/USB, so no separate supply is needed for a ~1 W LED. For a 3 W module keep the burst short or use a slightly larger cell (it already is).

## Why no screen / button / SD card?

The whole point is minimalism and one-shot operation:

- **No button** — the power switch *is* the trigger.
- **No display** — the emitter's faint glow while transmitting is your "active" indicator; when it goes dark you're done. (If you want an explicit status LED, add one to any spare GPIO.)
- **No SD card** — the code database lives in flash and there's no logging or user data, so removable storage would only add size for nothing.

## Repository layout

```
topiaryTVBGONE/
├─ README.md            # this file
├─ .gitignore
├─ docs/
│  └─ schematic.svg     # wiring diagram (viewable on GitHub)
└─ firmware/            # PlatformIO project
   ├─ platformio.ini    # env: esp32-s3-SuperMini, dep: IRremoteESP8266
   └─ src/
      ├─ main.cpp       # one-shot blast + deep sleep
      └─ WORLD_IR_CODES.h  # the IR code database (~150 KB)
```

The `firmware/.pio` and generated `.vscode` files are gitignored.

## Build & flash

Requires [PlatformIO](https://platformio.org/) (CLI, or the PlatformIO IDE / VS Code extension). The dependency (`IRremoteESP8266`) is pulled automatically from library registry on first build.

```bash
cd firmware
pio run -t upload      # compile + flash (use pio device to pick the port)
# or, if `pio` isn't on PATH:
python -m platformio run --disable-auto-clean     # compile only
python -m platformio run -t upload                # compile + flash
```

- Target environment: `[env:esp32-s3-SuperMini]`, board `esp32-s3-devkitc-1` (pin-compatible with the SuperMini).
- The IR signal pin is **GPIO4 (D4)** — change `GPIO_IR` in [firmware/src/main.cpp](firmware/src/main.cpp) if you wire it elsewhere.

Typical usage after flashing: flip the battery switch on, hold the device pointed at your target(s), wait ~6 seconds, done. Flip off/on to repeat.

## Notes & tuning

- **Blast length** is set by `num_*codes` (how many codes) × (`code time + CODE_GAP_US`). Add or remove entries in [firmware/src/WORLD_IR_CODES.h](firmware/src/WORLD_IR_CODES.h), or change the order/count in `runAll()` in main.cpp.
- **Gap** between codes: `CODE_GAP_US` (20500 µs). Smaller = faster blast but less margin for slow IR receivers.
- **Frequency / duty cycle**: parsed codes carry their own carrier frequency; raw codes use the stored times directly. This matches Bruce's decode path (`read_bits(bitcompression) * 2` indexes into `times[]`, values ×10 → µs, sent via `irsend.sendRaw(...)`).

## License

Firmware: MIT (see [firmware/LICENSE](firmware/LICENSE)).
The IR code database is adapted from public TV-B-Gone projects; see the original sources for their terms.
