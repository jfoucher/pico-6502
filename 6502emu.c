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
#define CHIPS_IMPL
#include "6502.c"
#include "6522.h"

#define VIA_BASE_ADDRESS UINT16_C(0xFF90)

// If this is active, then an overclock will be applied
#define OVERCLOCK
// Comment this to run your own ROM
//#define TESTING

// Delay startup by so many seconds
#define START_DELAY 6
// The address at which to put your ROM file
#define ROM_START 0x8000
//Size of your ROM
#define ROM_SIZE 0x8000
// Your rom file in C array format
#define ROM_FILE "forth.h"
// Variable in which your rom data is stored
#define ROM_VAR taliforth_pico_bin

#ifdef VIA_BASE_ADDRESS
m6522_t via;
uint32_t gpio_dirs;
uint32_t gpio_outs;
#define GPIO_PORTA_MASK 0xFF  // PORTB of via is translated to GPIO pins 0 to 7
#define GPIO_PORTB_MASK 0x7F8000  // PORTB of via is translated to GPIO pins 8 to 15
#define GPIO_PORTA_BASE_PIN 0
#define GPIO_PORTB_BASE_PIN 15
#endif

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
uint32_t old_ticks = 0;


#endif

uint8_t mem[0x10000];
absolute_time_t start;
bool running = true;

uint64_t via_pins = 0;

void via_update() {
    // uint8_t pa = M6522_GET_PA(via_pins);
    // uint8_t pb = M6522_GET_PB(via_pins);

    // //
    // // Turn on led when first bit of PB is set
    // gpio_put(PICO_DEFAULT_LED_PIN, pb & 1);

    //printf("pins  %lx\n", (uint32_t)(via_pins & 0XFFFFFFFF));
    //printf("irq   %lx\n", (uint32_t)(M6522_IRQ & 0XFFFFFFFF));
    if ((uint32_t)(via_pins & 0XFFFFFFFF) & (uint32_t)(M6522_IRQ & 0XFFFFFFFF)) {
        //printf("via irq\n");
        irq6502(); 
    }
}

uint8_t read6502(uint16_t address) {
#ifndef TESTING
    if (address == 0xf004) {
        int16_t ch = getchar_timeout_us(100);
        if (ch == PICO_ERROR_TIMEOUT) {
           return 0;
        }
        return (uint8_t) ch & 0xFF;
#ifdef VIA_BASE_ADDRESS
    } else if ((address & 0xFFF0) == VIA_BASE_ADDRESS) {
        
        via_pins &= ~(M6522_RS_PINS | M6522_CS2); // clear RS pins - set CS2 low
        // Set via   RW high   set selected  set RS pins
        via_pins |= (M6522_RW | M6522_CS1 | ((uint16_t)M6522_RS_PINS & address));

        via_pins = m6522_tick(&via, via_pins);
        // Via trigrred IRQ
        uint8_t vdata = M6522_GET_DATA(via_pins);
        //printf("reading from VIA: %04X %02X \n", address, vdata);
        via_update();
        //old_ticks > 0 ? old_ticks-- : 0;
        return vdata;
#endif
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
#ifdef VIA_BASE_ADDRESS
    } else if ((address & 0xFFF0) == VIA_BASE_ADDRESS) {
        //printf("writing to VIA %04X val: %02X\n", address, value);
        via_pins &= ~(M6522_RW | M6522_RS_PINS | M6522_CS2); // SET RW pin low to write - clear data pins - clear RS pins
        // Set via selected      set RS pins                 set data pins
        via_pins |= (M6522_CS1 | ((uint16_t)M6522_RS_PINS & address));
        M6522_SET_DATA(via_pins, value);
        


        if (((uint16_t)M6522_RS_PINS & address) == M6522_REG_DDRB) {
            // Setting DDRB / Set pins to in/output
            gpio_dirs &= ~((uint32_t)GPIO_PORTB_MASK);
            gpio_dirs |= (uint32_t)(value << GPIO_PORTB_BASE_PIN) & (uint32_t)GPIO_PORTB_MASK;
            gpio_set_dir_all_bits(gpio_dirs);
        }

        if (((uint16_t)M6522_RS_PINS & address) == M6522_REG_RB) {
            // Setting DDRB / Set pins to in/output
            gpio_outs &= ~((uint32_t)GPIO_PORTB_MASK);
            gpio_outs |= (uint32_t)(value << GPIO_PORTB_BASE_PIN) & (uint32_t)GPIO_PORTB_MASK;
            gpio_put_masked(gpio_dirs, gpio_outs);
        }

        if (((uint16_t)M6522_RS_PINS & address) == M6522_REG_DDRA) {
            // Setting DDRB / Set pins to in/output
            gpio_dirs &= ~((uint32_t)GPIO_PORTA_MASK);
            gpio_dirs |= (uint32_t)(value << GPIO_PORTA_BASE_PIN) & (uint32_t)GPIO_PORTA_MASK;
            gpio_set_dir_all_bits(gpio_dirs);
        }

        if (((uint16_t)M6522_RS_PINS & address) == M6522_REG_RA) {
            // Setting DDRB / Set pins to in/output
            gpio_outs &= ~((uint32_t)GPIO_PORTA_MASK);
            gpio_outs |= (uint32_t)(value << GPIO_PORTA_BASE_PIN) & (uint32_t)GPIO_PORTA_MASK;
            gpio_put_masked(gpio_dirs, gpio_outs);
        }
        
        via_pins = m6522_tick(&via, via_pins);

        via_update();
        //old_ticks > 0 ? old_ticks-- : 0;
#endif
    } else {
        mem[address] = value;
    }
#endif
}




void callback() {
    #ifdef TESTING
        if ((pc == old_pc) && (old_pc == old_pc1)) {

            if (clockticks6502%1000000 == 0) {
                absolute_time_t now = get_absolute_time();
                int64_t elapsed = absolute_time_diff_us(start, now);

                float khz = (double)clockticks6502 / (double)(elapsed);

                printf("Average emulated speed was %.3f MHz\n", khz);
            }
            
            if (old_pc == 0x24F1) {
                printf("65C02 test suite passed sucessfully!\n\n");

                absolute_time_t now = get_absolute_time();
                int64_t elapsed = absolute_time_diff_us(start, now);

                float khz = (double)clockticks6502 / (double)(elapsed);

                printf("Average emulated speed was %.3f MHz\n", khz);
                printf("Average emulated speed was %.3f MHz\n", khz);
                
            } else {
                printf("65C02 test suite failed\n");
                printf("pc %04X opcode: %02X test: %d status: %02X \n", old_pc, opcode, mem[0x202], status);
                printf("a %02X x: %02X y: %02X value: %02X \n\n", a, x, y, value);
            }

            running= false;
            
            
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

#ifdef VIA_BASE_ADDRESS
    // one tick for each clock to keep accurate time
    for (uint16_t i = 0; i < clockticks6502-old_ticks; i++) {
        via_pins = m6522_tick(&via, via_pins);
    }

    via_update();
    
    old_ticks = clockticks6502;
#endif
    
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
        while(1) {}
    }
    for (int i = R_START; i < R_SIZE + R_START; i++) {
        mem[i] = R_VAR[i-R_START];
    }

    hookexternal(callback);
    reset6502();
#ifdef VIA_BASE_ADDRESS
    // setup VIA
    m6522_init(&via);
    m6522_reset(&via);
    gpio_dirs = 0; //GPIO_PORTB_MASK | GPIO_PORTA_MASK;
    gpio_outs = 0;
    // Init GPIO
    // Set pins 0 to 7 as output as well as the LED, the others as input
    gpio_init_mask(gpio_dirs);
    gpio_set_dir_all_bits(gpio_dirs);

#endif



#ifdef TESTING
    pc = 0X400;
#endif
    start = get_absolute_time();

    while (running) {
        step6502();
    }
    return 0;
}
