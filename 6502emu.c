/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"

#include "6502.c"

// If this is active, then an overclock will be applied
#define OVERCLOCK
// Comment this to run your own ROM
// #define TESTING

// Delay startup by so many seconds
#define START_DELAY 6
// The address at which to put your ROM file
#define ROM_START 0x8000
//Size of your ROM
#define ROM_SIZE 0x8000
// Your rom file in C array format
#define ROM_FILE "forth.h"
// Variable in which your rom data is stored
#define ROM_VAR taliforth_bin


#ifdef TESTING
#include "65C02_test.h"
#define R_VAR __65C02_extended_opcodes_test_bin
#define R_START 0
#define R_SIZE 0x10000

uint16_t old_pc = 0;
uint16_t old_pc1 = 0;
uint16_t old_pc2 = 0;
uint16_t old_pc3 = 0;
uint16_t old_pc4 = 0;

#else
#include ROM_FILE
#define R_VAR ROM_VAR
#define R_START ROM_START
#define R_SIZE ROM_SIZE
#endif

uint8_t mem[0x10000];
absolute_time_t start;
bool running = true;

uint8_t read6502(uint16_t address) {
    #ifndef TESTING
    if (address == 0xf004) {
        int16_t ch = getchar_timeout_us(100);
        if (ch == PICO_ERROR_TIMEOUT) {
           return 0;
        }
        return (uint8_t) ch & 0xFF;
    }
    #endif
    return mem[address];
}

void write6502(uint16_t address, uint8_t value) {

#ifdef TESTING
    mem[address] = value;
    if (address == 0x202) {
        printf("next test is %d\n", value);
    }
#else
    if (address == 0xf001) {
        printf("%c", value);
    } else {
        mem[address] = value;
    }
#endif
}




void callback() {
    #ifdef TESTING
        if ((pc == old_pc) && (old_pc == old_pc1)) {
            running= false;
            if (old_pc == 0x24F1) {
                printf("65C02 test suite passed sucessfully!\n\n");

                absolute_time_t now = get_absolute_time();
                int64_t elapsed = absolute_time_diff_us(start, now);

                float khz = (float)clockticks6502 / (float)(elapsed);

                printf("Average emulated speed was %.3f MHz\n", khz);
            } else {
                printf("65C02 test suite failed\n");
                printf("pc %04X opcode: %02X test: %d status: %02X \n", old_pc, opcode, mem[0x202], status);
                printf("a %02X x: %02X y: %02X value: %02X \n\n", a, x, y, value);
            }
            
            
        }
        old_pc4 = old_pc3;
        old_pc3 = old_pc2;
        old_pc2 = old_pc1;
        old_pc1 = old_pc;
        old_pc = pc;
    #endif
    //if (clockticks6502 % 100 == 0) {
        // absolute_time_t now = get_absolute_time();
        // int64_t elapsed = absolute_time_diff_us(start, now);

        // float khz = (float)clockticks6502 / (float)(elapsed/1000.0);

        // printf("kHz %.2f\n", khz);
    //}

    
}

int main() {
#ifdef OVERCLOCK
    vreg_set_voltage(VREG_VOLTAGE_1_15);
    set_sys_clock_khz(280000, true);
#endif
    stdio_init_all();

    for(uint8_t i = START_DELAY; i > 0; i--) {
        printf("Starting in %d \n", i);
        sleep_ms(1000);
    }

    printf("Starting\n");

    if (R_START + R_SIZE > 0x10000) {
        printf("Your rom will not fit. Either adjust ROM_START or ROM_SIZE\n");
    }
    for (int i = R_START; i < R_SIZE; i++) {
        mem[i] = R_VAR[i];
    }

    printf("ROM filled\n");

    hookexternal(callback);
    reset6502();
#ifdef TESTING
    pc = 0X400;
#endif
    start = get_absolute_time();

    while (running) {
        step6502();
    }
    return 0;
}
