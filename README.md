# Paw Power Tracer 🐾⚡

A local solar-charge-controller dashboard for an EPEVER Tracer, running on an ESP32-S3 with RS485. No Home Assistant, Raspberry Pi, or cloud account is required.

I wanted a more useful way to see what my Tracer was doing. Paw Power reads the controller, serves its own web page, and keeps daily solar-generation totals and peaks on the ESP32. There is also Paw Ball, because watching the electrons should come with a game against Donut.

**Status: early DIY version, tested by the project owner with a Tracer10415AN and an industrial ESP32-S3 board with onboard RS485/CAN.** Other controller models and boards are not verified. CAN is unused in this project. This is separate from the Junctek-to-EG4 battery bridge in [PawPower](https://github.com/MadMeower/PawPower).

## What it does

- Local web dashboard: solar watts/volts/amps, battery voltage/current, calculated battery power, and temperatures in Fahrenheit.
- Direct read-only polling: approximately six seconds per complete cycle with normal replies.
- Optional passive listening alongside an EPEVER Wi-Fi adapter, without competing for the RS485 bus.
- Individual reading ages and stale-data indications.
- 31 calendar days of solar kWh, coverage, and daily peaks, retained in ESP32 flash.
- CSV history download.
- Browser-session graphs: solar power, battery voltage/current, and controller temperature; zoom buttons, wheel zoom, and drag-to-pan.
- Paw Ball against Donut, including optional browser-generated sound.
- Saved connection-mode selection, restored after a restart.

**Storage distinction:** daily totals and peaks live on the ESP32 and checkpoint every five minutes. Detailed graphs hold up to two hours only while that browser page remains open; reloading clears them. This board does not have the touchscreen project's SD-card history or ZIP backup.

## Hardware

Tested setup:

- EPEVER Tracer10415AN solar charge controller.
- Industrial ESP32-S3 control board with integrated RS485 transceiver: [board used](https://www.amazon.com/dp/B0FNJX8VGZ).
- USB power for the ESP32.
- RJ45 cable/breakout for the Tracer RS485 port.
- Optional EPEVER-WiFi-2.4G-RJ45-D dongle for passive mode.

A bare ESP32 UART cannot connect directly to RS485 A/B. This board already includes the transceiver. Another board needs its own verified RS485 interface and pin assignments.

## Wiring

The tested board uses UART TX GPIO17, RX GPIO18, and active-high driver-enable GPIO21. Controller communication is 115200 baud, 8 data bits, no parity, one stop bit, address 1.

For an RJ45 cable terminated **T568B**:

| Tracer RJ45 pin | T568B conductor | ESP32 board terminal |
| --- | --- | --- |
| 4 | Blue | RS485 B |
| 5 | White/blue | RS485 A |
| 8 | Brown | GND |

EPEVER assigns pins 3/4 to B, 5/6 to A, and 7/8 to ground. Verify the actual cable termination; color alone is not a pin definition. Leave all unused wires insulated. **Pins 1/2 provide +5V: do not connect them to ESP32 GPIO.** Power the board through USB. No CAN wiring is required.

## Installation

Uses **ESPHome 2026.10.2**, the version tested during development. Start with that version or a compatible newer version. Home Assistant is not required. [ESPHome installation](https://esphome.io/install/).

1. Download the complete repository with **Code → Download ZIP** and extract it.
2. Copy its contents into your ESPHome Device Builder configuration directory. The main YAML and both supporting folders must be siblings. **Importing only the YAML does not bring the supporting files with it.**
3. For a new setup, copy `secrets.yaml.example` to `secrets.yaml` and fill in your Wi-Fi and web credentials. Run `Generate-API-Key.bat` on Windows to generate a private API encryption key. If `secrets.yaml` already exists, add the required entries to it rather than replacing it.
4. Edit the hostname/IP and board pins as needed. The supplied YAML currently uses **172.16.0.113**, gateway **172.16.0.1**, subnet **255.255.255.0**. Change that for your network, or remove the `manual_ip` block to use DHCP.
5. Refresh Builder, open `PawPower-Tracer.yaml`, compile, and upload. First installation may require USB; later updates can use OTA.
6. Open the device's IP address in your browser and sign in with your web credentials.
7. With the Wi-Fi dongle and other RS485 polling devices disconnected, select **Poll - no dongle** and Apply. Allow five seconds for the preference to save.
8. Check voltage, power, and temperature readings against the controller before relying on the log.

Expected layout:

```text
ESPHome configuration directory/
  PawPower-Tracer.yaml
  secrets.yaml                 # yours; never committed
  secrets.yaml.example
  Generate-API-Key.bat
  pawpower_common/
  tracer_shared/
  tests/
```

Required secrets: `wifi_ssid`, `wifi_password`, `tracer_api_key`, `paw_web_username`, `paw_web_password`. The example API key is a public placeholder, not a private key. Each device can use its own named key in one secrets file. Keep the key when updating a device.

## Connection modes

**Poll - no dongle:** Paw Power is the RS485 master. It sends read-input-register requests only; it does not write battery settings. Unplug any dongle or external display that also polls this bus. Two independent masters caused communication errors in testing.

**Listen - Solar Guardian:** Paw Power does not transmit. It decodes the dongle's requests and the controller's replies. This preserves Solar Guardian logging, but readings arrive at the dongle's polling cadence, which was sometimes over a minute in testing. No automatic polling takeover occurs when the dongle is silent.

First boot defaults to Listen. Later boots restore the chosen mode. Disconnect the dongle before selecting or restarting in Poll mode.

## Data limitations

- Controller SOC proved inaccurate on the owner's LiFePO4 bank, so it appears only in diagnostic details. It is not a shunt measurement and is not used for charge control.
- Battery temperature may be the controller's default 25°C / 77°F if no probe is installed.
- Signed battery current requires both registers 0x331B/0x331C. A dongle that requests only the low word cannot provide a verified signed value; that partial value is diagnostic only and is excluded from peaks.
- Battery power is calculated from separately sampled voltage and current.
- Solar kWh integrates observed PV input watts; it is not battery discharge or whole-house consumption. No earlier days are reconstructed.
- Logging requires synchronized time. Startup, time changes, or sample gaps over 20 seconds create partial coverage. Passive mode can therefore miss substantial energy when the dongle polls slowly.
- Sudden power loss can lose roughly the last five minutes of uncheckpointed totals/peaks. Saved daily history survives normal restarts.
- Daily maxima are independent observations and may occur at different times.
- The Modbus decoder validates CRC and matches request/function/byte count. Modbus has no transaction ID; the bus must have one polling master.

## Security

The web interface uses HTTP with Digest authentication. Native API encryption uses a separate key. Do not forward the ESP32 web port directly to the internet. A VPN such as Tailscale on an existing PC/subnet router can provide remote access without exposing the device publicly.

`secrets.yaml` is excluded by `.gitignore`. The ZIP contains only a fake example. Review any local changes before publishing.

## Source layout

- `PawPower-Tracer.yaml`: hardware, networking, sensors, persisted stores and periodic work.
- `tracer_shared/tracer_bus.h`: read-only polling and passive Modbus decoding.
- `tracer_shared/solar_peaks.h`: Tracer-specific daily maxima.
- `tracer_shared/dashboard.js`: Tracer presentation, live graphs and controls.
- `pawpower_common/energy_tracker.h`: shared energy integration and day storage.
- `pawpower_common/web_assets.h`: shared Paw Ball asset, served locally.

Each standalone release includes the common folder. Keep common-file versions aligned if installing several Paw Power projects in one Builder directory. See [COMMON-CODE.md](COMMON-CODE.md).

## Validation

Host tests cover captured Modbus traffic, CRC rejection, missing/late replies, mode changes, energy integration/restore, daily maxima, graph gaps and dashboard reconnect logic. The owner confirmed live direct-poll readings, the dashboard and mode restoration on hardware. Newer changes should still be compiled and tested on a device before release; host tests are not an ESPHome cross-compile.

Run the included Node/C++ tests with Node.js and a C++17 compiler. They do not require real credentials or controller hardware.

## References

- [ESPHome UART](https://esphome.io/components/uart/)
- [ESPHome web API](https://esphome.io/web-api/)
- [EPEVER Tracer manual](https://www.epever.com/wp-content/uploads/2021/05/Tracer-AN50-100A-Manual-EN-V4.0.pdf)
- [EPEVER register-map implementation reference](https://github.com/rosswarren/epevermodbus)

Paw Power is an independent DIY project, not affiliated with EPEVER or EG4.
