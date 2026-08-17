**[README](README.md)** > **Changelog** · [Report an issue](../../issues/new)

# Changelog

All notable changes to this firmware. Versions follow [semver](https://semver.org/); the Matter software version integer for each release is noted since OTA uses it.

## v0.3.0 - Identify fix & toolchain refresh

*Software version `0x00000300`*

- **Fixed:** the Identify button. The soil sensor endpoint advertised its identify type as None; controllers hid or ignored the button even though the LED blink logic was present. It now advertises the status LED as a visible indicator
- **Changed:** esp_matter moved from 1.5.1 to 1.6.0. The 1.6.0 component ships the Soil Sensor endpoint helper and the `SetSoilMoisture` glue, so the `soil_measurement_compat` shim that mirrored them is deleted
- **Changed:** builds pinned to ESP-IDF v5.5.5, up from v5.5.2, in both the dev container and the release workflow
- No change to sampling, power management, button handling, or battery reporting

## v0.2.0 - Battery health & diagnostics

*Software version `0x00000200`*

- **Fixed:** battery operation. On battery the device appeared to die after a couple of button presses. Cause: peripheral power-down during light sleep disabled the button's GPIO wake and the device slept through presses (USB masked the bug because the console blocks light sleep). The peripheral domain now stays powered in light sleep
- **Added:** battery health via sag measurement. The firmware loads the cell with its own LEDs (~daily) and uses voltage sag as an internal-resistance proxy; drives charge level Warning/Critical and `BatReplacementNeeded`
- **Added:** brownout diagnostics. Red ×5 blink at boot after a brownout reset, plus a lifetime counter in NVS printed on the serial console
- **Added:** Thread TX power capped at +10 dBm (configurable) to cut TX battery peaks ~3×

## v0.1.0 - Initial release

*Software version `0x00000100`*

- Matter over Thread firmware for the Seeed Studio XIAO Soil Moisture Sensor (XIAO ESP32-C6)
- Soil Sensor device type (Matter 1.5 SoilMeasurement cluster), sampled locally every 15 min, reported on ≥1% change
- Battery percentage via the Power Source cluster
- Sleepy end device (Thread MTD, LIT ICD), BLE commissioning, multi-fabric, Matter OTA (A/B partitions)
- Button: single press = sample + LED moisture verdict, triple press = LED-guided dry/wet calibration (persisted to NVS), 10 s hold = factory reset

## Related documentation

- [README](README.md) — project overview and quick start
- [Flashing Guide](docs/FLASHING.md) — flashing over USB-C and back to stock
- [Commissioning Guide](docs/COMMISSIONING.md) — pairing with Home Assistant and other ecosystems
- [Calibration Guide](docs/CALIBRATION.md) — LED-guided dry and wet calibration
- [Updating Guide](docs/UPDATING.md) — Matter OTA and USB updates that keep commissioning
- [Power & Battery](docs/POWER.md) — power design decisions and field data
- [Hardware Notes](docs/HARDWARE.md) — pin map, probe drive, battery sensing, antenna
- [Building from Source](docs/BUILDING.md) — dev container, ESP-IDF, release artifacts
- [Troubleshooting](docs/TROUBLESHOOTING.md) — LED reference and common issues
- [Roadmap](docs/ROADMAP.md) — known limitations and planned work
- [Contributing](docs/CONTRIBUTING.md) — commit, branch, and release conventions
