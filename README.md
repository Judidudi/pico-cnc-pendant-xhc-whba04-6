# Universal CNC Pendant (XHC WHB04B-6 compatible)

This repository contains the software and hardware for a complete CNC pendant that emulates the USB protocol of the original XHC WHB04B-6 pendant. It is designed to work with LinuxCNC, Beamicon2, Mach3 and Mach4 without any driver changes.

<p align="center">
  <img src="docs/images/pendant.jpg" alt="CNC pendant" width="600">
</p>

## Features

- **Drop-in WHB04B-6 emulation** – recognized by LinuxCNC's stock `xhc-whb04b-6` driver
- **60 mm handwheel**, rotary switches for axis selection and feed override
- **8 mechanical keys** (Kailh Choc V2) with configurable mapping
- **2.8" TFT touch display** (LVGL) showing machine position and status
- **Hardwired E-stop** – routed as a physical contact to the machine, completely independent of USB
- **Connection watchdog** – the pendant detects a lost host connection and can trigger the E-stop

## Compatibility

| Controller | Status |
|------------|--------|
| LinuxCNC   | To be tested |
| Beamicon2  | WIP |
| Mach3      | WIP |
| Mach4      | WIP |

## Repository structure

```
firmware/        Pico 2 W firmware (C, Pico SDK, TinyUSB, LVGL)
hardware/
  ├─ pcb/        KiCad projects (work in progress)
  ├─ enclosure/  Link to the printable parts on Printables
  └─ bom/        Bill of materials
docs/            Assembly guide, wiring, images
```

## Quickstart

1. Download the latest `.uf2` file from the [Releases](../../releases) page.
2. Hold the **BOOTSEL** button on the Pico while plugging in the USB cable. The Pico shows up as a USB drive.
3. Copy the `.uf2` file onto the drive. The Pico restarts automatically and the pendant is ready to use.

## Safety

- The **RESET** key toggles the machine on/off – it is **not** an emergency stop.
- The pendant's E-stop only protects you if it is wired into your machine's **hardware safety chain**. Never rely on a software-only E-stop.
- This is a hobby project. Build and use it at your own risk.

## To do

- Test and optimize the default button layout and add symbols to the keycaps
- Test with Beamicon2, Mach3 and Mach4
- Finish the hardware watchdog
- Design a custom PCB with an integrated RP2350 and all peripherals

## License

The original work in this repository is licensed under **TODO**, see [LICENSE](LICENSE).

Third-party components keep their own licenses:

| Component | License |
|-----------|---------|
| [LVGL](https://github.com/lvgl/lvgl) v8.3 | MIT |
| [TinyUSB](https://github.com/hathach/tinyusb) (via Pico SDK) | MIT |
| [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) | BSD-3-Clause |

Full license texts: [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md)

## Acknowledgements

- The LinuxCNC project and the authors of the `xhc-whb04b-6` component
- The LVGL and TinyUSB communities
