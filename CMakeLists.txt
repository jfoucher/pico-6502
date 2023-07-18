cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(6502_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Creates a pico-sdk subdirectory in our project for the libraries
pico_sdk_init()

if (TARGET tinyusb_device)
    add_executable(6502emu
    6502emu.c
    )

    # Pull in our pico_stdlib which aggregates commonly used features
    target_link_libraries(6502emu pico_stdlib hardware_timer hardware_vreg)

    # enable usb output, disable uart output
    pico_enable_stdio_usb(6502emu 1)
    pico_enable_stdio_uart(6502emu 0)

    # create map/bin/hex/uf2 file etc.
    pico_add_extra_outputs(6502emu)

    # add url via pico_set_program_url
    #example_auto_set_url(6502emu)
elseif(PICO_ON_DEVICE)
    message(WARNING "not building hello_usb because TinyUSB submodule is not initialized in the SDK")
endif()
