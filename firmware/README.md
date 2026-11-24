# CNC Handwheel Pendant

A handwheel pendant for CNC machines, built on a Raspberry Pi Pico 2 W. It
presents itself over USB as an XHC WHB04B-6, so existing controllers
recognise it without a driver of its own, and it adds a hardware watchdog
that the original device does not have.

Compatible with XHC WHB04B-6.

## Features

- Handwheel (quadrature encoder) with detent division, 8 function keys, two
  rotary switches for axis and step size
- 320x240 touch display: coordinate readout, menu, settings, key assignment,
  a page for triggering functions by touch, and a diagnostic page showing
  the raw USB traffic in both directions
- Freely assignable keys, 32 assignable actions (16 protocol codes and 15 Fn
  macros), stored in flash
- Heartbeat output for a hardware watchdog relay: if the firmware stops, the
  edges stop and the safety circuit opens
- Two colour themes, measured touch calibration, both persisted in flash
- Settings are written without ever erasing a flash sector while running, so
  the superloop is never frozen long enough to matter

## Hardware

- Raspberry Pi Pico 2 W (RP2350)
- ILI9341 320x240 display with XPT2046 resistive touch, on a shared SPI bus
- Quadrature handwheel encoder with push button
- 8 function keys
- 2 rotary switches (4 and 6 positions) read as resistor ladders on the ADC
- Watchdog relay driven by a retriggerable monostable, in series with a
  16 mm mushroom button in one safety circuit

Pinout, schematic and build instructions are in the manual. This repository
holds the firmware only; the KiCad project and the manual are not part of it
yet.

## Supported controllers and test status

| Controller | Status |
|---|---|
| LinuxCNC (`xhc-whb04b-6` HAL component) | Standalone test with `xhc-whb04b-6 -ue` passed |
| Mach3 | Not tested |
| Mach4 | Not tested |
| UCCNC | Not tested |
| Beamicon2 | Not tested |

Note for LinuxCNC: the host component starts in MPG mode, where the jog
scale is zero and the handwheel does not move the machine. Assign a key to
"Mode continuous" or "Mode step" and press it once. If the handwheel changes
the feed override instead of jogging, remove `-f` from the `loadusr` line.
See the manual for the full explanation.

## Building

Requires the Raspberry Pi Pico SDK 2.3.1, CMake 3.14 or newer, Ninja and the
ARM toolchain. The Raspberry Pi Pico VS Code extension installs all of them
and builds without further arguments.

LVGL is **not** part of this repository. CMake fetches the pinned revision at
configure time. The first configure therefore needs a network connection and
takes a few minutes, because it clones LVGL's history; later configures reuse
what is already in `build/_deps`. Nothing else is downloaded.

From the command line:

```bash
cmake -S . -B build -G Ninja -DPICO_SDK_PATH=/path/to/pico-sdk
ninja -C build
```

This produces `build/cnc_pendant.uf2`.

To build offline, put an LVGL checkout of the pinned version in `lib/lvgl`
and point CMake at it:

```bash
cmake -S . -B build -G Ninja -DPICO_SDK_PATH=/path/to/pico-sdk -DFETCHCONTENT_SOURCE_DIR_LVGL=$PWD/lib/lvgl
```

On macOS, CMake may fail to find Python because `/usr/bin/python3` triggers
the Xcode licence prompt. Either accept it once with `sudo xcodebuild
-license`, or point CMake at the Command Line Tools copy:

```bash
cmake -S . -B build -G Ninja -DPICO_SDK_PATH=/path/to/pico-sdk -DPython3_EXECUTABLE=/Library/Developer/CommandLineTools/usr/bin/python3
```

## Flashing

Hold BOOTSEL while plugging the Pico in, then copy the `.uf2` onto the
`RPI-RP2` drive that appears. The board reboots into the firmware by itself.

## Documentation

- `ARCHITECTURE.md` - the design rules the code depends on
- `CHANGELOG.md` - what changed per release

The user manual and build guide (`main.pdf`, LaTeX sources) and the KiCad
project live outside this repository for now.

## Safety

**The machine's own emergency stop remains mandatory.** The pendant's
watchdog contact supplements it and does not replace it. Under the Machinery
Directive (Annex I, 1.2.4.3) an emergency stop may only ever supplement
other protective measures.

The safety circuit uses the closed-circuit principle: it is energised during
normal operation, so a broken cable, an unplugged connector or a loss of
supply opens it through the wiring alone, with no logic involved. For the
subtler case - firmware hung, power present, cable intact - the firmware
emits a periodic heartbeat rather than a static level, and a retriggerable
monostable on the board opens the circuit when the edges stop.

One consequence worth knowing: the heartbeat also stops when USB is not
enumerated. That is the fail-safe direction, but it means the machine is not
enabled while no host is connected.

Nothing about the safety function can be changed from the user interface.

## Development

This project was developed with AI assistance (Claude Code). The author
reviewed, tested and is responsible for the code. [AUTHOR: adjust to the
rules of your institution.]

## Licence

GNU General Public License version 3 - see `LICENSE`.

Third-party components and their licences are listed in
`THIRD_PARTY_NOTICES.md`. The USB identifiers and descriptor strings of the
original XHC device are reproduced so that existing drivers recognise this
one; this project is not affiliated with or endorsed by XHC.
