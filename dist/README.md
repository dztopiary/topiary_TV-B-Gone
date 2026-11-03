# TV-B-Gone - Ready-to-flash package

Everything you need to flash this firmware onto an ESP32-S3 SuperMini, without
installing PlatformIO or VS Code.

## What's here

| File | Purpose |
|------|---------|
| `esp-flash-tool/Flash-TV-B-Gone.bat` | One-click Windows script that flashes the board for you |
| `flash-config.json` | Config for the official ESP Flash Download Tool (pre-wired addresses) |
| `firmware/tv-b-gone-v1.0.bin` | The compiled app (~341 KB) |
| `firmware/bootloader.bin` | 2nd-stage bootloader (~19 KB) |
| `firmware/partitions.bin` | Partition table (~3 KB) |

## Easiest path (Windows, one click)

1. Download the **ESP Flash Download Tool** from Espressif's site:
   <https://www.espressif.com/en/support/download/other-tools> — pick "Flash download tool" for Windows.
2. Double-click `esp-flash-tool\Flash-TV-B-Gone.bat`.
3. Put your ESP32-S3 into flashing mode (hold BOOT, tap RESET once) when it asks.
4. The official flasher opens pre-loaded with the right addresses and files — hit **Start**.

Done. Power-cycle the board to run it.

## Alternative: just use esptool.py

If you'd rather not install the GUI tool, any Python 3 environment will do:

```bash
pip install esptool
python -m esptool --chip esp32s3 write_flash @flash-config.json
```

(Put the board in flashing mode first.)

## Alternative: official ESP Flash Download Tool manually

1. Install the tool from Espressif's site.
2. Open it → **File → Import** → pick `flash-config.json`.
3. Hit **Start**.

That's it — no PlatformIO, no VS Code, no compiling.
