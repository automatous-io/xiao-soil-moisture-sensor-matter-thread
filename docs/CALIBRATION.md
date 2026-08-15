**[README](../README.md)** > **Calibration Guide** · [Report an issue](../../../issues/new)

# Calibration Guide

The probe measures capacitance, and every batch of potting mix, probe, and enclosure differs a little. The firmware ships with defaults taken from the [stock firmware](https://github.com/Seeed-Studio/xiao-esphome-projects)'s behavior, about 2750 mV dry and 1200 mV wet at the probe output. Those are close enough for basic monitoring, and a 30-second calibration makes the 0-100% scale accurate for your specific probe and soil.

Calibrate when readings sit oddly high or low (for example, never above 70% in soaked soil), when the sensor moves to a very different soil type, or when you are contributing field data and want it comparable.

## The procedure

You need the sensor powered on, dry air, and a glass of water.

1. Triple-press the button.
2. The red LED blinks for 10 seconds. Hold the probe in dry air, out of soil and wiped clean. At the end of the countdown the firmware measures the dry reference, which takes about a second.
3. After a 3-second pause the green LED blinks for 10 seconds. Put the probe in the glass of water up to the enclosure line, keeping the electronics dry. The firmware measures the wet reference.
4. Five green blinks mean the calibration was accepted, saved to flash, and a fresh reading reports immediately. Five red blinks mean it was rejected: the dry reading must exceed the wet reading by at least 200 mV, and a rejection usually means the probe was in soil during the dry phase or missed the water. Nothing changes on a rejection; triple-press to try again.

Calibration persists in NVS.

## Sanity checking

Single-press the button for an instant reading with an LED verdict: red below 23%, yellow from 23 to 57%, green at 58% and above, the same classification the stock firmware uses. The exact percentage reaches Home Assistant a moment later.

## Fleet builds

When flashing several sensors, the defaults are build-time configurable under `idf.py menuconfig`, in the Soil Sensor Configuration menu, as `SOIL_CAL_DEFAULT_DRY_MV` and `SOIL_CAL_DEFAULT_WET_MV`. That lets you bake in a calibration measured on one unit. Per-device button calibration still overrides the baked defaults.

## Related documentation

- [README](../README.md) — project overview and quick start
- [Flashing Guide](FLASHING.md) — flashing over USB-C and back to stock
- [Commissioning Guide](COMMISSIONING.md) — pairing with Home Assistant and other ecosystems
- [Updating Guide](UPDATING.md) — Matter OTA and USB updates that keep commissioning
- [Power & Battery](POWER.md) — power design decisions and field data
- [Hardware Notes](HARDWARE.md) — pin map, probe drive, battery sensing, antenna
- [Building from Source](BUILDING.md) — dev container, ESP-IDF, release artifacts
- [Troubleshooting](TROUBLESHOOTING.md) — LED reference and common issues
- [Roadmap](ROADMAP.md) — known limitations and planned work
- [Contributing](CONTRIBUTING.md) — commit, branch, and release conventions
- [Changelog](../CHANGELOG.md) — release history by version
