**[README](../README.md)** > **Roadmap** · [Report an issue](../../../issues/new)

# Roadmap

What is planned and what is known to be missing. This is a spare-time project; the order is firm, the dates are not.

## Next release (v0.4.0)

Verified and ready to tag: cell voltage over Matter. The Power Source cluster now carries `BatVoltage`, the resting measurement in millivolts, because the battery percentage is a linear voltage map that pins at 100% on a lithium cell, and the raw millivolts previously reached only the serial console, which a running battery unit cannot use. Confirmed on hardware in Home Assistant and delivered to a deployed unit over Matter OTA from v0.3.0. See the [changelog](../CHANGELOG.md) for what changed.

Endurance on a cell is still open, and it is tracked below rather than here, since it does not gate the release.

## Open investigations

- AA battery life. A cell lasts weeks rather than months, and the radio duty cycle does not account for it. That leaves the chip's light sleep floor and the board's boost converter as the two candidates, and telling them apart needs a current measurement taken on battery with USB disconnected, since connecting USB removes both. The data and the open questions live in [POWER.md](POWER.md#the-aa-problem), and field reports are the fastest way to move this.
- A chemistry-aware battery percentage. The mapping is linear in voltage and assumes an alkaline cell, so it overstates the early decline on alkaline and pins at 100% on a lithium primary ([POWER.md](POWER.md#battery-telemetry-and-health)). The replacement should be a piecewise curve fitted from the voltage the field units now report, rather than from a datasheet.
- Alternative power. Lithium AA is under test on two units as of August 2026. The XIAO's LiPo pads bypass the boost converter entirely and remain untested.

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
