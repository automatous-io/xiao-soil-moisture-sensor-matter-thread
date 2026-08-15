**[README](../README.md)** > **Roadmap** · [Report an issue](../../../issues/new)

# Roadmap

What is planned and what is known to be missing. This is a spare-time project; the order is firm, the dates are not.

## Next release (v0.3.0)

- Move to the esp_matter 1.6 component, which ships the Soil Sensor endpoint natively, and delete the `soil_measurement_compat` shim ([BUILDING.md](BUILDING.md#whats-in-the-build)).
- Fix Identify. v0.2.0 advertises its identify type as None, and controllers hide or ignore the Identify button even though the LED blink logic is present ([TROUBLESHOOTING.md](TROUBLESHOOTING.md#common-issues)). The fix advertises the LED as a visible indicator.
- Bump the dev container to ESP-IDF v5.5.5, the version the 1.6 component recommends.
- Deliver the release over Matter OTA from v0.2.0, as the first end-to-end verification of the OTA path ([UPDATING.md](UPDATING.md#matter-ota)).

## Open investigations

- AA battery life. A cell lasts weeks rather than months, and the evidence so far points at the board's power path rather than the radio. The data and the open questions live in [POWER.md](POWER.md#the-aa-problem), and field reports are the fastest way to move this.
- Alternative power. Lithium AA cells behave differently under pulse loads, and the XIAO's LiPo pads bypass the boost converter entirely. Both untested.

## Considered

- A browser-based flasher, removing the esptool requirement for release installs.

## Watching

- Apple Home and Google Home support for the Soil Sensor device type. The sensor commissions there but shows nothing today ([COMMISSIONING.md](COMMISSIONING.md#apple-home-and-google-home)). When either app catches up, the [compatibility table](../README.md#compatibility) wants reports.

## Related documentation

- [README](../README.md) — project overview and quick start
- [Flashing Guide](FLASHING.md) — flashing over USB-C and back to stock
- [Commissioning Guide](COMMISSIONING.md) — pairing with Home Assistant and other ecosystems
- [Calibration Guide](CALIBRATION.md) — LED-guided dry and wet calibration
- [Updating Guide](UPDATING.md) — Matter OTA and USB updates that keep commissioning
- [Power & Battery](POWER.md) — power design decisions and field data
- [Hardware Notes](HARDWARE.md) — pin map, probe drive, battery sensing, antenna
- [Building from Source](BUILDING.md) — dev container, ESP-IDF, release artifacts
- [Troubleshooting](TROUBLESHOOTING.md) — LED reference and common issues
- [Contributing](CONTRIBUTING.md) — commit, branch, and release conventions
- [Changelog](../CHANGELOG.md) — release history by version
