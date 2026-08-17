**[README](../README.md)** > **Flashing Guide** · [Report an issue](../../../issues/new)

# Flashing Guide

The XIAO ESP32-C6 has native USB-C with a built-in bootloader. Flashing needs no UART adapter, no soldering, and no disassembly beyond opening the enclosure. It is also reversible: [going back to stock](#flashing-back-to-stock) is a browser operation that takes a few minutes.

## What you need

You need the sensor, a USB-C data cable (not a charge-only cable), and a release binary from [Releases](../../../releases/latest) or your own build per [BUILDING.md](BUILDING.md). Flashing a release binary uses `esptool` (`pip install esptool`). If you built from source, `idf.py flash` handles everything and esptool is not needed separately.

## Flash a release binary

1. Connect the sensor over USB-C. A serial port should appear (`/dev/ttyACM0`, `/dev/cu.usbmodem*`, or a COM port). If it does not, hold the BOOT button while plugging in.
2. Coming from the stock firmware, start clean:

   ```sh
   esptool.py --chip esp32c6 erase_flash
   ```

   `erase_flash` wipes everything, including stock WiFi credentials and any stored calibration. Skipping it is not enough to keep commissioning between versions of this firmware, because the merged image in the next step clears NVS on its own. To update while keeping your pairing and calibration, use one of the paths in [UPDATING.md](UPDATING.md) instead.

3. Write the firmware:

   ```sh
   esptool.py --chip esp32c6 write_flash 0x0 automatous-io-xiao-soil-moisture-sensor-vX.Y.Z.bin
   ```

4. Open a serial monitor at 115200 baud (`idf.py monitor`, `screen`, or any terminal app). On boot the device prints its QR code URL and manual pairing code, which you will need for [commissioning](COMMISSIONING.md).

## Flash from source

With a local ESP-IDF install:

```sh
cd source
idf.py build flash monitor
```

Building in the dev container instead? The container cannot reach USB on macOS or Windows; [BUILDING.md](BUILDING.md#flashing-what-you-built) covers building in the container and flashing from the host.

## After flashing

With no fabric configured, the device advertises for BLE commissioning automatically; go straight to [COMMISSIONING.md](COMMISSIONING.md). Reassemble the enclosure before the probe goes back into soil, and confirm the external U.FL antenna is connected. The firmware routes RF to the external antenna, matching Seeed's hardware design for this kit, and a disconnected pigtail means poor range.

## Flashing back to stock

Seeed publishes the stock ESPHome firmware ([source](https://github.com/Seeed-Studio/xiao-esphome-projects)) with a browser-based flasher at [gadgets.seeed.cc](https://gadgets.seeed.cc/) (use a Chromium-based browser for WebSerial). Run `esptool.py --chip esp32c6 erase_flash` first, then flash the stock image, and the sensor is back to factory WiFi behavior. Remove the device from your Matter controllers afterwards.

## Related documentation

- [README](../README.md) — project overview and quick start
- [Commissioning Guide](COMMISSIONING.md) — pairing with Home Assistant and other ecosystems
- [Calibration Guide](CALIBRATION.md) — LED-guided dry and wet calibration
- [Updating Guide](UPDATING.md) — Matter OTA and USB updates that keep commissioning
- [Power & Battery](POWER.md) — power design decisions and field data
- [Hardware Notes](HARDWARE.md) — pin map, probe drive, battery sensing, antenna
- [Building from Source](BUILDING.md) — dev container, ESP-IDF, release artifacts
- [Troubleshooting](TROUBLESHOOTING.md) — LED reference and common issues
- [Roadmap](ROADMAP.md) — known limitations and planned work
- [Contributing](CONTRIBUTING.md) — commit, branch, and release conventions
- [Changelog](../CHANGELOG.md) — release history by version
