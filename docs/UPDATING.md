**[README](../README.md)** > **Updating Guide** · [Report an issue](../../../issues/new)

# Updating Guide

There are two update paths, and both keep your commissioning, Thread credentials, and soil calibration. Those live in NVS, which neither path touches.

## USB update

Reflash the new merged image over USB-C:

```sh
esptool.py --chip esp32c6 write_flash 0x0 automatous-io-xiao-soil-moisture-sensor-vX.Y.Z.bin
```

From source, `idf.py flash` does the same. Do not run `erase_flash` between versions of this firmware; that is the step that wipes commissioning. After reboot the device rejoins Thread with its existing credentials and resumes reporting.

## Matter OTA

The firmware runs a Matter OTA requestor with A/B app partitions. A new image is transferred over Thread to the inactive slot, verified, and swapped at reboot, with no cable involved. Each release ships a `.ota` asset for this purpose, and Home Assistant's Matter Server acts as the OTA provider: you place the image where the server can read it, and it offers the image to any commissioned device whose vendor and product ID match.

The steps below are verified on the [Shelly Gen4 Matter over Thread project](https://github.com/automatous-io/shelly-1-gen4-matter-thread/blob/main/docs/UPDATING.md) with Matter Server add-on version 9.0.2, using the same test credentials this firmware ships. This device has not run one end to end yet; its first OTA is planned for the next release, and a [report](../../../issues) from anyone who beats us to it is welcome.

1. **Enable Test Net DCL.** The Matter Server only serves uncertified OTA images with it on. Go to Settings, then Apps, then Matter Server, then Configuration, turn on **Enable test-net DCL usage**, and restart the add-on.
2. **Get the image.** Download the `.ota` from the [latest release](../../../releases/latest) or [build it yourself](BUILDING.md#release-builds). Its version must be higher than what the device is running.
3. **Place the image.** Copy it into `/addon_configs/core_matter_server/updates/` (the Samba Share add-on is the simplest way in; create the `updates` folder if it does not exist).
4. **Restart the add-on to import.** The server scans the folder at startup. On success the log shows `Imported OTA file: …` and the `.ota` disappears from the folder, which is expected; the server copied it into its own storage.
5. **Install.** The update appears on the device page in Home Assistant and on the node in the Matter Server add-on. Trigger it from either. The device downloads over Thread, writes the inactive slot, verifies, and reboots into the new firmware.

A sleepy device downloads slowly by design, and checks for updates on its own timer. Pressing the button wakes it if you want the exchange to start sooner. If an OTA attempt fails, the device keeps running the active slot; the update either completes and verifies or does not happen. One imported image updates every matching device on the network, meaning both of your sensors pick it up from a single upload.

## Version numbering

Firmware versions are semver and surface in Matter as both a version string and an integer, `0x00000200` for 0.2.0. Controllers use the integer to decide whether an OTA image is an upgrade, and releases always increment it.

## Related documentation

- [README](../README.md) — project overview and quick start
- [Flashing Guide](FLASHING.md) — flashing over USB-C and back to stock
- [Commissioning Guide](COMMISSIONING.md) — pairing with Home Assistant and other ecosystems
- [Calibration Guide](CALIBRATION.md) — LED-guided dry and wet calibration
- [Power & Battery](POWER.md) — power design decisions and field data
- [Hardware Notes](HARDWARE.md) — pin map, probe drive, battery sensing, antenna
- [Building from Source](BUILDING.md) — dev container, ESP-IDF, release artifacts
- [Troubleshooting](TROUBLESHOOTING.md) — LED reference and common issues
- [Roadmap](ROADMAP.md) — known limitations and planned work
- [Contributing](CONTRIBUTING.md) — commit, branch, and release conventions
- [Changelog](../CHANGELOG.md) — release history by version
