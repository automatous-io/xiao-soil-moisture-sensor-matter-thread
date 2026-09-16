**[README](../README.md)** > **Roadmap** · [Report an issue](../../../issues/new)

# Roadmap

What is planned and what is known to be missing. This is a spare-time project; the order is firm, the dates are not.

## Next release

v0.4.0 is the current release and shipped cell voltage over Matter; the [changelog](../CHANGELOG.md) has the details. The next release is expected to be a power release, and its contents depend on what the battery investigation below turns up. The sleepy end device, LIT, delta reporting, and capped TX power are already in place, and the measured draw says the remaining loss is not in the radio schedule. The work in progress is therefore measurement and hardware path testing before firmware: the module regulator check, a cell on the module's battery pads, and a bench current profile. A few firmware settings that trim awake time, such as letting the CPU clock scale down when idle, are lined up behind those results. Each one gets tested on battery against a known floor rather than guessed at.

## Open investigations

- AA battery life. A cell lasts weeks rather than months, and the radio duty cycle does not account for it. The lithium runs put the average draw at roughly 4.5 mA, tens of times what a device that sleeps between samples should pull. The schematics point at the XIAO module's buck regulator, which the sensor board back-feeds through its output on AA power, with the chip's own 180 µA light-sleep floor second and the AA boost converter's light-load efficiency multiplying both ([HARDWARE.md](HARDWARE.md#power-path)). Confirming it needs a voltage reading on the module's BAT+ pad, which sits on the back-fed rail whenever USB is absent, and a current measurement at the cell, both on battery with USB disconnected, since connecting USB removes the whole path. The data and the open questions live in [POWER.md](POWER.md#the-aa-problem), and a bench measurement is the fastest way to move this.
- A chemistry-aware battery percentage. The mapping is linear in voltage and assumes an alkaline cell, so it overstates the early decline on alkaline and pins at 100% on a lithium primary ([POWER.md](POWER.md#battery-telemetry-and-health)). The field data now includes one complete lithium discharge, and it showed the gauge at 97% while the cell was brownout-looping. The replacement should be a piecewise curve fitted from the voltage the field units report, rather than from a datasheet, and it needs to be honest that a Li-FeS2 cell gives about a day of voltage warning however it is mapped.
- Alternative power. Energizer Ultimate Lithium AA ran 21 days in Standard Mode and is past 28 days in Battery Saver Mode as of September 2026, against a 27-day projection for alkaline. A modest gain, not a multiple ([POWER.md](POWER.md#field-data)). A cell on the XIAO's battery pads bypasses the boost converter and feeds the module regulator in its normal input range, which by the schematics removes the two largest identified losses at once. Untested.
- Peripheral power-down in light sleep. The ESP32-C6 datasheet lists light sleep at 180 µA with peripherals powered and 35 µA with them powered down. The firmware keeps them powered because the v0.2.0 bug showed that powering them down kills the button's GPIO wake ([POWER.md](POWER.md#what-the-firmware-does-to-save-power)). The button sits on GPIO2, which is one of the chip's low-power GPIOs, so a wake through the low-power domain instead of the digital GPIO peripheral might keep the button working with the peripherals off. That would be the largest firmware-side saving identified so far, it keeps RAM and the Thread and Matter state intact, and it is exactly the class of change that must be verified on battery with USB disconnected.

## Considered

- A browser-based flasher, removing the esptool requirement for release installs.
- Deep sleep between samples. The chip's deep-sleep floor is 7 µA against 180 µA in light sleep, but deep sleep is a reboot. ESP-IDF ships an OpenThread deep-sleep sleepy-device example for the C6 that saves the network and parent information to flash and re-attaches on every wake, and its README recommends light sleep unless the device sleeps for more than about 30 minutes at a stretch; this firmware samples every 15 minutes. Matter adds the session and subscription state on top, which would have to be re-established on every wake, and esp-matter has no deep-sleep path for a Thread device. Worth a proper comparison once the regulator question is settled and the peripheral power-down option above has been tried, since both are cheaper.

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
