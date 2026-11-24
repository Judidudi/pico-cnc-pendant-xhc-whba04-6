# Third-party notices

This project is licensed under the GNU General Public License version 3
(see `LICENSE`). It uses the following third-party components. All of them
carry permissive licences that may be combined into a GPL-3.0 work; their
own terms continue to apply to their own files.

## LVGL 8.3.11

- Licence: MIT
- Copyright (c) 2021 LVGL Kft
- Source: https://github.com/lvgl/lvgl
- Where: not vendored. CMake fetches a pinned revision at configure time
  into `build/_deps/lvgl-src/`, where its own `LICENCE.txt` ships with it.

The project configuration lives outside the library, in `inc/lv_conf.h`, so
the library itself stays upgradable.

## Raspberry Pi Pico SDK 2.3.1

- Licence: BSD-3-Clause
- Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
- Source: https://github.com/raspberrypi/pico-sdk
- Where: not vendored. The SDK is located at build time through
  `PICO_SDK_PATH`. The full licence text ships with the SDK as
  `LICENSE.TXT`.

`pico_sdk_import.cmake` is the SDK's own import script, copied unmodified as
the SDK intends. It is the only third-party file in this repository.

## TinyUSB

- Licence: MIT
- Copyright (c) 2018 hathach (tinyusb.org)
- Source: https://github.com/hathach/tinyusb
- Where: not vendored; it ships inside the Pico SDK and is pulled in through
  the `tinyusb_device` library.

## XHC WHB04B-6 protocol and the LinuxCNC xhc-whb04b-6 component

No code from LinuxCNC is used, copied, linked or distributed here.

The USB protocol this firmware speaks was reconstructed by reading the
LinuxCNC HAL component `xhc-whb04b-6` (GPL-2.0), which is the host-side
counterpart. A handful of lines of that component are quoted inside source
comments, as documentation of the interface being implemented - for example
the slot normalisation in `inc/whb04b_codes.h`, which is the reason
an Fn combination can be sent as a single report.

The USB vendor and product identifiers (`0x10CE` / `0xEB93`) and the three
USB descriptor strings are those of the original XHC device. They are what
existing drivers match on, and they are reproduced so that this device is
recognised without a driver of its own. "XHC" and "WHB04B" are the marks of
their respective owner; this project is not affiliated with or endorsed by
them.
