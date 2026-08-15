**[README](../README.md)** > **Contributing** · [Report an issue](../../../issues/new)

# Contributing

Field reports, docs fixes, and firmware PRs are all welcome. This is a small repo with one firmware for one device, and there is no heavy process.

The most useful contribution needs no code: battery and compatibility data from your own unit. Open an [issue](../../../issues/new) with what you ran and what happened; [POWER.md](POWER.md#file-a-field-report) lists what to include.

For code, build in the [dev container](BUILDING.md) and note what hardware testing was done: bench USB, battery, or in soil. Changes that can affect power or sleep need testing on battery with USB disconnected, because an active USB console blocks light sleep and masks battery-path bugs.

Commits follow [Conventional Commits](https://www.conventionalcommits.org/) with a lowercase imperative subject, using `feat`, `fix`, `docs`, `chore`, and `build`. Branches are named `type/short-kebab-description`. Releases are tagged `vX.Y.Z`, with assets built by the scripts in `scripts/` ([BUILDING.md](BUILDING.md#release-builds)) and a matching [CHANGELOG.md](../CHANGELOG.md) entry.

Contributions are accepted under [Apache 2.0](../LICENSE).

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
- [Roadmap](ROADMAP.md) — known limitations and planned work
- [Changelog](../CHANGELOG.md) — release history by version
