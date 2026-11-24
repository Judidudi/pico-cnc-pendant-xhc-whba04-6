# 3.14 is the floor because of FetchContent_MakeAvailable() below.
cmake_minimum_required(VERSION 3.14)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

# THE one place the version is defined. cmake/version.h.in turns it into a
# header; nothing else may carry a version number of its own.
project(cnc_pendant VERSION 0.9.0 LANGUAGES C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

configure_file(
        ${CMAKE_CURRENT_LIST_DIR}/cmake/version.h.in
        ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h
        @ONLY)

# ---------------------------------------------------------------------
# LVGL is fetched at configure time instead of being vendored, so no
# third-party code lives in this repository.
#
# The revision is PINNED TO A COMMIT, not to a branch and not to a tag. It is
# the exact tree this firmware was tested with on hardware: the head of
# LVGL's release/v8.3 branch, which is AHEAD of the v8.3.11 tag even though
# lvgl.h still reports 8.3.11. Building against the tag instead produces a
# measurably different binary.
#
# v8.3 is a deliberate choice, not a default: v9 replaces the whole display
# driver registration model for no gain here. Do not move this pin without
# reading inc/lv_conf.h and src/lvgl_port.c, and do not move it without
# re-testing on hardware - the UI leans on LVGL input internals.
#
# The configuration stays outside the library, in inc/lv_conf.h, and has to
# be set BEFORE the library is made available.
#
# Offline builds: drop an LVGL 8.3.11 checkout into lib/lvgl and configure
# with -DFETCHCONTENT_SOURCE_DIR_LVGL=${CMAKE_SOURCE_DIR}/lib/lvgl, and
# nothing is downloaded.
# ---------------------------------------------------------------------
include(FetchContent)

set(LV_CONF_PATH ${CMAKE_CURRENT_LIST_DIR}/inc/lv_conf.h CACHE STRING "" FORCE)

FetchContent_Declare(lvgl
        GIT_REPOSITORY https://github.com/lvgl/lvgl.git
        GIT_TAG        fbb73d4b3ffe086a7218bd1a8e51f9e7a260c4b9  # release/v8.3, reports 8.3.11
        # No GIT_SHALLOW: a shallow clone can only fetch a branch or tag, not
        # an arbitrary commit. The price is LVGL's full history on the first
        # configure, a few minutes. A GitHub archive URL with URL_HASH would
        # take seconds, but GitHub does not guarantee its generated tarballs
        # stay byte-identical, and a changed checksum breaks the build for
        # everyone. Slow and reliable wins here.
)
FetchContent_MakeAvailable(lvgl)

# LVGL declares example and demo targets this project does not use. Take them
# out of the default build, or they are rebuilt on every "ninja". Guarded,
# because whether they exist at all depends on LVGL's own options.
foreach(unused_target lvgl_examples lvgl_demos)
    if(TARGET ${unused_target})
        set_target_properties(${unused_target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
    endif()
endforeach()

add_executable(cnc_pendant
        src/main.c
        src/pendant_state.c
        src/pendant_settings.c
        src/encoder.c
        src/buttons.c
        src/virtual_key.c
        src/rotary_switch.c
        src/settings_store.c
        src/heartbeat.c
        src/whb04b_usb.c
        src/usb_descriptors.c
        src/ili9341_driver.c
        src/xpt2046_touch.c
        src/lvgl_port.c
        src/ui.c
        src/ui_theme.c
        src/ui_main.c
        src/ui_menu.c
        src/ui_settings.c
        src/ui_reset.c
        src/ui_buttons.c
        src/ui_function_pick.c
        src/ui_grid.c
        src/ui_functions.c
        src/ui_calibration.c
        src/ui_debug.c
        )

pico_set_program_name(cnc_pendant "cnc-pendant")
pico_set_program_version(cnc_pendant "${PROJECT_VERSION}")

# The USB port is reserved EXCLUSIVELY for the WHB04B-6 HID emulation. Never
# set pico_enable_stdio_usb to 1 - it collides with our own TinyUSB HID
# descriptor. printf goes out over GP0/GP1 at 115200 baud.
pico_enable_stdio_uart(cnc_pendant 1)
pico_enable_stdio_usb(cnc_pendant 0)

target_include_directories(cnc_pendant PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/inc
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)

# Warnings are enabled on THIS target only. The SDK and LVGL are third-party
# code and are not ours to fix; raising the level globally would bury our own
# warnings in theirs.
target_compile_options(cnc_pendant PRIVATE -Wall -Wextra)

# The SDK default is a 2 KB stack. LVGL needs considerably more for layout
# and drawing - an overflow ends silently in a hard fault. 4 KB is the
# maximum without a linker script of our own, because the core 0 stack lives
# in SCRATCH_Y and that is exactly 4 KB.
target_compile_definitions(cnc_pendant PRIVATE
        PICO_STACK_SIZE=0x1000
)

target_link_libraries(cnc_pendant
        pico_stdlib
        tinyusb_device
        hardware_adc
        hardware_dma
        hardware_flash
        hardware_gpio
        hardware_spi
        hardware_sync
        lvgl
        )

pico_add_extra_outputs(cnc_pendant)
