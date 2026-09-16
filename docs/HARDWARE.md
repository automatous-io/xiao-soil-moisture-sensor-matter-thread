**[README](../README.md)** > **Hardware Notes** · [Report an issue](../../../issues/new)

# Hardware Notes

The [Seeed Studio XIAO Soil Moisture Sensor](https://www.seeedstudio.com/XIAO-Soil-Sensor-p-6452.html) ([wiki](https://wiki.seeedstudio.com/xiao_soil_moisture_sensor/)) is a XIAO ESP32-C6 carrier with a capacitive soil probe, three status LEDs, a user button, a AA battery holder with a boost converter, and an external U.FL antenna, all in a garden-tolerant enclosure. This page records what the firmware knows about the board.

## Pin map

From [app_priv.h](../source/main/app_priv.h):

| GPIO | XIAO pin | Function |
|---|---|---|
| 0 | D0 | Battery voltage ADC (100 kΩ series resistor, reads the cell directly) |
| 1 | D1 | Soil probe output ADC |
| 2 | D2 | User button (active low) |
| 3 | - | RF switch enable (driven low to enable) |
| 14 | - | Antenna select (driven high for the external U.FL) |
| 18 | D10 | Yellow LED |
| 19 | D8 | Green LED |
| 20 | D9 | Red LED |
| 21 | D3 | Soil probe excitation, 200 kHz PWM |

## Soil probe

The capacitive probe is driven by a 200 kHz excitation signal at 68% duty, the same parameters the [stock ESPHome firmware](https://github.com/Seeed-Studio/xiao-esphome-projects) uses, and readings stay comparable between the two. A measurement turns the PWM on, waits 300 ms for the probe's RC filter to settle, averages 10 ADC reads over about half a second, and turns the PWM off. Higher moisture produces lower voltage, and the dry and wet calibration points ([CALIBRATION.md](CALIBRATION.md)) map millivolts to a 0-100% scale.

The ADC runs at 12 dB attenuation with ESP-IDF curve-fitting calibration, which makes millivolt values meaningful across units.

## Battery sensing

The AA cell voltage arrives at GPIO0 through a 100 kΩ series resistor with a capacitor to ground, so the pin sees the cell voltage directly, where 1000 mV maps to 0% and 1500 mV to 100%, matching the stock firmware. Beyond resting voltage, the firmware measures sag under load by lighting all three LEDs for about 150 ms as a known load and comparing the loaded and resting readings. The sag is an internal-resistance proxy for cell health; thresholds and details are in [POWER.md](POWER.md#battery-telemetry-and-health).

On USB power the cell is out of the load path and the battery percentage is not meaningful; it typically pins at 0%. The history below is from a unit that ran its first three days on AA and then moved to USB power, where the reading flatlines at zero while the device keeps reporting soil data normally.

<p align="center">
  <img src="images/ha-battery-history-usb.png" alt="Home Assistant history graph showing battery percentage declining on AA for three days, then reading zero after the unit moved to USB power" width="820">
</p>

## Antenna

The C6's RF output routes through a switch to an external U.FL antenna, which is Seeed's design for this kit. The firmware enables the switch and selects the external antenna at boot, and holds the selection through light sleep. A disconnected U.FL pigtail causes very poor range. Check it first when debugging weak Thread connectivity.

## Power path

A single AA cell feeds a TI [TPS61021A](https://www.ti.com/lit/ds/symlink/tps61021a.pdf) boost converter that supplies the 3.3 V rail. The part is identified from the [schematic](https://files.seeedstudio.com/wiki/XIAO_Soil_Moisture_Sensor/res/SCH.pdf) on the Seeed wiki. It starts at 0.9 V and operates down to 0.5 V input. Alkaline, NiMH, and lithium primary cells all work in the holder. A NiMH cell runs fine but its 1.2 V resting voltage reads low on the battery gauge, which maps 1.0 V to 0% and 1.5 V to 100%.

Do not put a 3.7 V lithium-ion cell in the holder. The schematic names the battery connector CN_BAT_14500, but 14500 is the mechanical size code for an AA-format cell, not a lithium-ion endorsement. Above its 3.3 V setpoint the TPS61021A passes the input straight through to the output, which would put the cell voltage on the ESP32-C6's rail, past its 3.6 V absolute maximum. A lithium-ion or LiPo cell belongs on the XIAO module's battery pads instead, which feed the module's regulator at a voltage it is designed for. The paragraphs below say why that path should do far better than the boost. It has not been tested with the kit's enclosure.

The converter's enable pin is gated by USB 5 V presence. Plugging in USB shuts the boost down and takes the cell out of the load path entirely. That is the mechanism behind the battery gauge pinning at 0% on USB power ([battery sensing](#battery-sensing) above).

The module's side of the rail is the more likely problem. The XIAO ESP32-C6 [schematic](https://files.seeedstudio.com/wiki/SeeedStudio-XIAO-ESP32C6/XIAO_ESP32_C6_v1.0_SCH_260114.pdf) shows its 3.3 V coming from an SGM6029 switching buck regulator, fed from an internal 5 V rail that USB supplies through a Schottky diode and the battery pads supply through a MOSFET. The sensor board drives 3.3 V from the boost converter into the module's 3V3 pin, which is that buck regulator's output. On AA power the regulator's input rail has no source. In a synchronous buck the high-side FET's body diode points from the switch node to the input, so the 3.3 V on the output would be expected to pull the input up to about 2.5 V. The enable pin is tied to the input, which would leave the regulator awake at its own output, below its setpoint, in a state its datasheet does not describe. That is a reading of the schematic, not a measurement. A [measurement on the same module](https://tomasmcguinness.com/2025/01/06/lowering-power-consumption-in-esp32-c6/) with a Power Profiler Kit II found a sleep floor of 299 µA with the supply at 3.3 V and under 11 µA at 3.8 V. The author did not say which pin he fed, and 3.8 V is a LiPo resting voltage, which suggests the battery pads or the 5 V pin. At 3.3 V on its input the buck is at or below its setpoint, which is the same situation our back-feed puts it in by a different route, and at 3.8 V it is regulating normally. Both of his figures are deep sleep, and this firmware light-sleeps, so the 11 µA is not a floor this device would reach. What transfers is the difference: same chip, same sleep, only the supply changed, and about 290 µA disappeared. That is the regulator's share. Through a 1.5 V boost at light load, 300 µA on the rail becomes roughly a milliamp at the cell. It is the largest identified piece of the short AA runtime, and it is why a cell on the module's battery pads should do far better than the same capacity in the AA holder: it removes the boost converter and puts the buck back in its normal input range at the same time. With the holder empty the boost converter's enable pin, which is pulled up to the cell, sits at 0 V, and the TPS61021A datasheet specifies true input-to-output disconnection in shutdown with 1 to 2 µA of leakage into a held-up output. On paper the converter is inert in that state. It has not been checked on a bench. The firmware would also need to know, because with the holder empty the gauge reads 0% and the health logic reports a dead cell.

Two smaller loads on the module are worth knowing about. The RF switch is powered whenever its enable GPIO is low, which the firmware holds through light sleep on purpose ([antenna](#antenna)). The module's yellow user LED hangs on GPIO15 through 1.5 kΩ, and the firmware does not configure that pin.

The module's internal 5 V rail has no test point of its own, but with USB absent the battery-path MOSFET is on and connects that rail to the BAT+ pad on the module's underside. With the unit on the AA, USB unplugged, and nothing on the battery pads, the BAT+ pad reads near zero if the buck is dormant and around 2.5 V if the output is holding the input up through the body diode. The data and the open questions are in [the AA problem](POWER.md#the-aa-problem).

## Brownout behavior

When the supply sags below the ESP32-C6's brownout threshold mid-operation, typically a worn cell meeting a TX peak, the chip resets. The firmware makes this visible with five red blinks at boot after a brownout and a lifetime counter in NVS that prints on the serial console at every boot. A climbing brownout counter means the power source is dying or undersized.

## Related documentation

- [README](../README.md) — project overview and quick start
- [Flashing Guide](FLASHING.md) — flashing over USB-C and back to stock
- [Commissioning Guide](COMMISSIONING.md) — pairing with Home Assistant and other ecosystems
- [Calibration Guide](CALIBRATION.md) — LED-guided dry and wet calibration
- [Updating Guide](UPDATING.md) — Matter OTA and USB updates that keep commissioning
- [Power & Battery](POWER.md) — power design decisions and field data
- [Building from Source](BUILDING.md) — dev container, ESP-IDF, release artifacts
- [Troubleshooting](TROUBLESHOOTING.md) — LED reference and common issues
- [Roadmap](ROADMAP.md) — known limitations and planned work
- [Contributing](CONTRIBUTING.md) — commit, branch, and release conventions
- [Changelog](../CHANGELOG.md) — release history by version
