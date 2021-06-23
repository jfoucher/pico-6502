# PICO-6502

This is a 6502 emulator for the [Raspberry Pi Pico](https://www.raspberrypi.org/products/raspberry-pi-pico/)

You can interact with it via its USB serial connection.

It emulates a 65C02 processor so that [Taliforth](https://github.com/scotws/TaliForth2) can run on it.

Most of the 6502 emulation code is from [this codegolf answer](https://codegolf.stackexchange.com/a/13020) with some additions to add 65C02 instructions and adressing modes.

The interaction with your 6502 programs is extremely simple: any write to address `$F001` will appear on the serial console, and you can read from `$F004` to see if a character is available from serial. This means that obviously your own programs must not tough these two addresses for anything other than input/output.