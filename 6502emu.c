/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/time.h"

#include "6502.c"
#include "65C02_test.h"



uint8_t mem[0x10000];
absolute_time_t start;


uint8_t read6502(uint16_t address) {
    // printf("read %04X\n", address);
    // if (address == 0xf004) {
        // int16_t ch = getchar_timeout_us(100);
        // if (ch == PICO_ERROR_TIMEOUT) {
        //    return 0;
        // }
        // return (uint8_t) ch & 0xFF;
    // }
    return mem[address];
}

void write6502(uint16_t address, uint8_t value) {
    //
    // if (address == 0xf001) {
        //printf("write %04X %02X\n", address, value);
        // printf("%c", value);
    // } else {
        mem[address] = value;
    // }

    if (address == 0x202) {
        printf("next test is %d\n", value);
    }
}

uint16_t old_pc = 0;

void callback() {
    //if (clockticks6502 % 100 == 0) {
        printf("pc %04X opcode: %02X test: %d \n", old_pc, opcode, mem[0x202]);

        old_pc = pc;
        // absolute_time_t now = get_absolute_time();
        // int64_t elapsed = absolute_time_diff_us(start, now);

        // float khz = (float)clockticks6502 / (float)(elapsed/1000.0);

        // printf("kHz %.2f\n", khz);
    //}

    
}

int main() {
    stdio_init_all();
    for(int i = 0; i < 0x10000; i++) {
        // if (i >= 0x8000) {
        //     mem[i] = _Users_jonathan_Documents_Planck_Software_forth_taliforth_py65mon_bin[i-0x8000];
        // }

        mem[i] = __65C02_extended_opcodes_test_bin[i];
        
    }
    for(uint8_t i = 6; i > 0; i--) {
        printf("Starting in %d \n", i);
        sleep_ms(1000);
    }
    
    // mem[0xFFFA] = 0x00;
    // mem[0xFFFB] = 0xff;
    // mem[0xFFFC] = 0x00;
    // mem[0xFFFD] = 0xff;

    // mem[0xFF10] = 0xA9;
    hookexternal(callback);
    reset6502();
    pc = 0X400;
    start = get_absolute_time();

    while (true) {
        exec6502(100);
        sleep_ms(10);
    }
    return 0;
}
