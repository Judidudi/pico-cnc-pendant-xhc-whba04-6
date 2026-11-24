# Architecture

Rules that are not visible in the code but that the code depends on. If you
change one of them, something below will break quietly rather than loudly.

## Cooperative superloop, local state machines

There is no overarching state machine. `src/main.c` holds a plain
`while (true)` loop that taps every module once per pass through its
`_update()` function.

- **No module blocks.** No `sleep_ms()` inside a module update.
- A module gets a state machine only where it genuinely needs one, not on
  principle.
- **Modules never call each other.** They communicate through the shared
  `pendant_state_t` / `button_mapping_t` only.
- **No dynamic allocation in time-critical paths.**

Everything runs on core 0. Moving LVGL to core 1 was considered and
deliberately rejected: the safety heartbeat must stop when any part of the
loop hangs, and on a separate core it would keep running. If core 1 is ever
needed, the shared state needs a `critical_section_t` **and** the heartbeat
concept has to be rethought.

## The order of calls in the superloop is load-bearing

Two places in `main.c` are not free to move:

1. USB is serviced twice per pass, before and after drawing. An LVGL pass
   can block on SPI for several milliseconds, and the host's display data
   piles up meanwhile - the coordinate readout then visibly lags.
2. `virtual_key_apply()` runs after `buttons_update()` (which fills the
   report's two key slots from the physical keys), after `lvgl_port_task()`
   (where touch events fire, so a tap from the same pass can start
   immediately), and before `whb04b_usb_send()`.

The heartbeat runs last, so it measures exactly what it protects: one
complete pass through all modules.

## The safety heartbeat is not negotiable

GP10 toggles every 50 ms. The watchdog is a retriggerable monostable in
hardware, driven by those edges, and it opens the same safety circuit as the
mushroom button.

- `heartbeat_update()` is called **directly from the superloop**, never from
  a timer interrupt. An interrupt-driven heartbeat would keep running while
  another module blocked the loop - which is the exact failure it exists to
  catch. This property must survive any refactoring.
- Missed edges are **not** made up for after a long pass.
- **Nothing about the safety function is settable from the user interface**,
  and nothing may be added that is.
- A connection-loss watchdog must never run over the USB/HID data channel: a
  watchdog that can hang the same way as the thing it watches is not a
  watchdog.

## USB is reserved for the HID emulation

`pico_enable_stdio_usb` must stay at **0**. Setting it to 1 collides with
this project's own TinyUSB HID descriptor. `printf` goes out over the UART
on GP0/GP1 at 115200 baud, and that is the only debug channel.

Debug output is kept short on purpose: stdio over UART blocks once the
32-byte TX FIFO is full, and a full FIFO costs about 2.8 ms at 115200 baud.

## Pins and timing constants live in one file

`inc/pendant_config.h`. New pins are assigned **only** there.

GP23, GP24, GP25 and GP29 are used internally by the CYW43 WiFi chip on the
Pico 2 W and must never appear.

Two pairs have to be changed together:

- A rotary switch's pin and its ADC index (`PIN_ROTARY_*` and
  `ADC_INPUT_ROTARY_*`). Mismatched, one pin is switched to analogue mode
  and a different one is measured - no error, just a switch that reads
  nonsense forever.
- Display orientation: `MADCTL_VALUE`, `ILI9341_WIDTH`/`HEIGHT`, and a fresh
  touch calibration.

## External libraries are fetched, never committed

No third-party code lives in this repository. CMake fetches LVGL at configure
time from a **pinned** revision in `CMakeLists.txt`, so the build is
reproducible without carrying 99 MB of library in the history.

LVGL is pinned to **v8.3**, not v9: v9 replaces the whole driver registration
model for no gain here. Do not change that without discussion, and do not
move the pin without re-testing on hardware - the UI leans on LVGL input
internals (see "Firing only on release" in the code comments).

LVGL's configuration therefore lives outside the library, in
`inc/lv_conf.h`.

## The user interface sets no colours of its own

All colours and measurements come from `inc/ui_theme.h` and
`src/ui_theme.c`, as shared `lv_style_t`. A theme change rewrites
the styles and calls `lv_obj_report_style_change(NULL)`.

The rule in one sentence: if a widget calls `lv_obj_set_style_*_color()`
directly, it will not follow a theme change. This is checkable mechanically -
outside `ui_theme.c`, a grep for `lv_color_hex`, `lv_palette_` and
`lv_obj_set_style_*_color` must come up empty.

Two traps:

- `lv_label_set_recolor()` with a colour in the text is hardcoding too.
- `lv_obj_add_style()` **appends**. A state change that re-applies a style
  grows the object's style list, so every state style needs a counterpart
  (`ui_apply_selected()` / `ui_clear_selected()`). `lv_obj_remove_style()`
  with NULL is not a substitute - it strips the theme styles as well.

## Flash persistence never erases while running

A sector erase runs from RAM with interrupts off and freezes the superloop,
heartbeat included - **measured at 112 to 160 ms on this device**. A
monostable dimensioned above that would leave a real firmware hang
undetected for just as long.

So: two sectors at the end of flash, 16 slots each; a save programs the next
free slot (one page program, a few milliseconds, no erase); the stale sector
is erased **at boot**, in `settings_store_boot_maintenance()`, before
`heartbeat_init()`, where a stall is harmless because the machine cannot be
enabled yet. Each record carries a sequence number and the highest valid one
wins.

**A changed default is a reason to raise `SETTINGS_VERSION`** - otherwise the
old value survives the update unnoticed. Be aware that raising it discards
the whole record, including the touch calibration, so the device starts up in
the calibration screen.

## Display flush completes in the DMA interrupt

`ili9341_flush_cb()` returns immediately and the transfer is finished in the
DMA interrupt, not from the superloop. This is mandatory, not a preference:
with a single draw buffer `lv_refr.c` waits inside `lv_timer_handler()`, so
control does not return to the loop during a redraw and a poll placed there
would never run.

Three things hang together:

1. The interrupt must wait for `spi_is_busy()` before releasing chip select.
   The DMA is done when the last byte reaches the FIFO; letting go earlier
   truncates the last pixels of the band.
2. The DMA path has to repeat the cleanup `spi_write_blocking()` does -
   drain the RX FIFO and clear the overrun flag. The PL022 pushes one byte
   into RX for every byte sent; nobody reads those, the FIFO overruns and the
   flag latches. The next user of the bus is the touch controller, which then
   reads that leftover instead of its own answer.
3. The shared bus needs a gate. `read_channel()` in the touch driver changes
   the SPI clock and would destroy a band in flight, so it calls
   `ili9341_bus_wait()` first. Same for `ili9341_set_rotated()`.

## Comments and display text are English

Comments explain **why**, not what. Display strings are ASCII only: LVGL's
Montserrat fonts carry ASCII plus a thin symbol list, and anything else
renders as a placeholder box. U+00B7 is missing, U+2022 is present - check
the font's cmap before using a special character rather than guessing.

## Formatting

`.clang-format` at the repository root describes the existing style; it was
read off the code, not chosen. Two initialiser tables are aligned by hand and
fenced with `// clang-format off`, because no clang-format setting reproduces
them.

Format our own sources only - and not `inc/lv_conf.h`, which is derived from
LVGL's own template:

```bash
clang-format -i src/*.c inc/pendant_*.h inc/ui*.h
```
