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
// DON
#include "hardware/gpio.h"
#define PIN_0 0
#define PIN_1 1
#define PIN_2 2
#define PIN_3 3
#define PIN_4 4
#define PIN_5 5
#define PIN_6 6
#define PIN_7 7

#define PIN_8 8
#define PIN_9 9
#define PIN_10 10
#define PIN_11 11
#define PIN_12 12
#define PIN_13 13
#define PIN_14 14
#define PIN_15 15

uint8_t via_latch = 0;
// DON
// #define VIA_BASE_ADDRESS UINT16_C(0xFF90) // Don this address interferes with Krusader
// #define VIA_BASE_ADDRESS UINT16_C(0x0200)
// DON TRY THE FOLLOWING
#define VIA_BASE_ADDRESS UINT16_C(0x7000)

// If this is active, then an overclock will be applied
#define OVERCLOCK
// Comment this to run your own ROM
// #define TESTING

// Delay startup by so many seconds
#define START_DELAY 6
// The address at which to put your ROM file
#define ROM_START 0x8000
// Size of your ROM
#define ROM_SIZE 0x8000
// Your rom file in C array format
// #define ROM_FILE "forth.h"
// #define ROM_FILE "wozmon.h"
#define ROM_FILE "krusader.h"
// Variable in which your rom data is stored
// #define ROM_VAR taliforth_pico_bin
// #define ROM_VAR wozmon
#define ROM_VAR krusader

#ifdef VIA_BASE_ADDRESS
m6522_t via;
uint32_t gpio_dirs;
uint32_t gpio_outs;
#define GPIO_PORTA_MASK 0xFF     // PORTB of via is translated to GPIO pins 0 to 7
#define GPIO_PORTB_MASK 0x7F8000 // PORTB of via is translated to GPIO pins 8 to 15
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

void via_update()
{
    // uint8_t pa = M6522_GET_PA(via_pins);
    // uint8_t pb = M6522_GET_PB(via_pins);

    // //
    // // Turn on led when first bit of PB is set
    // gpio_put(PICO_DEFAULT_LED_PIN, pb & 1);

    // printf("pins  %lx\n", (uint32_t)(via_pins & 0XFFFFFFFF));
    // printf("irq   %lx\n", (uint32_t)(M6522_IRQ & 0XFFFFFFFF));
    if ((uint32_t)(via_pins & 0XFFFFFFFF) & (uint32_t)(M6522_IRQ & 0XFFFFFFFF))
    {
        // printf("via irq\n");
        irq6502();
    }
}

uint8_t read6502(uint16_t address)
{
#ifndef TESTING
    // if (address == 0xf004) { // Don, update to work easily with Krusader
    if (address == 0xe004)
    {
        int16_t ch = getchar_timeout_us(100);
        if (ch == PICO_ERROR_TIMEOUT)
        {
            return 0;
        }
        return (uint8_t)ch & 0xFF;
#ifdef VIA_BASE_ADDRESS
    }
    // else if ((address & 0xFF00) == VIA_BASE_ADDRESS)
    else if ((address & 0xFFF0) == VIA_BASE_ADDRESS) 
        {
        via_latch = 0;
        // MODIFICATION PORTA
        if (address == VIA_BASE_ADDRESS + 0x01) // PORTA
            // MODIFICATION PORTA
            {
            uint8_t pin_0_value, pin_1_value, pin_2_value, pin_3_value, pin_4_value, pin_5_value, pin_6_value, pin_7_value;
            uint8_t pins[] = {PIN_0, PIN_1, PIN_2, PIN_3, PIN_4, PIN_5, PIN_6, PIN_7};
            via_latch = (gpio_get(PIN_7) << 7) | (gpio_get(PIN_6) << 6) | (gpio_get(PIN_5) << 5) | (gpio_get(PIN_4) << 4) | (gpio_get(PIN_3) << 3) | (gpio_get(PIN_2) << 2) | (gpio_get(PIN_1) << 1) | gpio_get(PIN_0);
            }
        // MODIFICATION PORTB
        if (address == VIA_BASE_ADDRESS) // PORTB
            {
            uint8_t pin_8_value, pin_9_value, pin_10_value, pin_11_value, pin_12_value, pin_13_value, pin_14_value, pin_15_value;
            uint8_t pins[] = {PIN_8, PIN_9, PIN_10, PIN_11, PIN_12, PIN_13, PIN_14, PIN_15};
            via_latch = (pin_8_value << 7) | (pin_9_value << 6) | (pin_10_value << 5) | (pin_11_value << 4) | (pin_12_value << 3) | (pin_13_value << 2) | (pin_14_value << 1) | pin_15_value;
            }
        if (address == VIA_BASE_ADDRESS + 2) // PORTB DDRB
            {
            // Access the value stored at M6522_REG_DDRB
            via_latch = via.pb.ddr;
            }   
        if (address == VIA_BASE_ADDRESS + 3) // PORTA DDRA
            {
            // Access the value stored at M6522_REG_DDRA
            via_latch = via.pa.ddr;
            }   
        if (address == VIA_BASE_ADDRESS + 4) // T1C-L 
            {
            via_latch = via.t1.counter & 0xff;
            }     
        if (address == VIA_BASE_ADDRESS + 5) // T1C-H 
            {
            via_latch = via.t1.counter >> 8;
            } 
        if (address == VIA_BASE_ADDRESS + 6) // T1L-L 
            {
            via_latch = ((uint16_t) (via.t1.latch & 0xff));
            }   
        if (address == VIA_BASE_ADDRESS + 7) // T1L-H 
            {
            via_latch = ((uint16_t) (via.t1.latch >> 8));
            }  
        if (address == VIA_BASE_ADDRESS + 8) // T2C-L 
            {
            via_latch = (uint8_t) (via.t2.counter & 0xff);
            } 
        if (address == VIA_BASE_ADDRESS + 9) // T2C-H  
            {
            via_latch = (uint8_t) (via.t2.counter >> 8);
            }   
        if (address == VIA_BASE_ADDRESS + 0xa) // SR   
            {
            via_latch = 0; // FIX ME
            }  
        if (address == VIA_BASE_ADDRESS + 0xb) // ACR   
            {
            via_latch = via.acr; 
            } 
        if (address == VIA_BASE_ADDRESS + 0xc) // PCR    
            {
            via_latch = via.pcr;
            }         
        if (address == VIA_BASE_ADDRESS + 0xd) // IFR    
            {
            via_latch  = via.intr.ifr;
            }      
        if (address == VIA_BASE_ADDRESS + 0xe) // IER    
            {
            via_latch  =  via.intr.ier;
            }     
        if (address == VIA_BASE_ADDRESS + 0xf) // ORA/IRA Reg 1 but No handshake    
            {
            via_latch = (gpio_get(PIN_7) << 7) | (gpio_get(PIN_6) << 6) | (gpio_get(PIN_5) << 5) | (gpio_get(PIN_4) << 4) | (gpio_get(PIN_3) << 3) | (gpio_get(PIN_2) << 2) | (gpio_get(PIN_1) << 1) | gpio_get(PIN_0);
            }  
        // And in any case
        //if ((address == VIA_BASE_ADDRESS + 0x01) || (address == VIA_BASE_ADDRESS))
        //    {
        via_pins &= ~(M6522_RS_PINS | M6522_CS2); // clear RS pins - set CS2 low
        // Set via   RW high   set selected  set RS pins
        via_pins |= (M6522_RW | M6522_CS1 | ((uint16_t)M6522_RS_PINS & address));
        via_pins = m6522_tick(&via, via_pins);

        // MODIFICATION VDATA 
        //  Via trigrred IRQ
        // uint8_t vdata = M6522_GET_DATA(via_pins);
        uint8_t vdata = via_latch;
        // printf("reading from VIA: %04X %02X \n", address, vdata);
        //    }
        via_update();
        // old_ticks > 0 ? old_ticks-- : 0;
        return vdata;
        //}
#endif
        }
#endif
    return mem[address];
}

void write6502(uint16_t address, uint8_t value)
{
#ifdef TESTING
    mem[address] = value;
    if (address == 0x202)
    {
        printf("next test is %d\n", value);
    }
#else
    // if (address == 0xf001) { // Don, update to work easily with Krusader
    if (address == 0xe001)
    {
        printf("%c", value);
#ifdef VIA_BASE_ADDRESS
    }
    else if ((address & 0xFFF0) == VIA_BASE_ADDRESS)
    // else if ((address & 0xDFF0) == VIA_BASE_ADDRESS)
    {
        // printf("writing to VIA %04X val: %02X\n", address, value);
        via_pins &= ~(M6522_RW | M6522_RS_PINS | M6522_CS2); // SET RW pin low to write - clear data pins - clear RS pins
        // Set via selected      set RS pins                 set data pins
        via_pins |= (M6522_CS1 | ((uint16_t)M6522_RS_PINS & address));
        M6522_SET_DATA(via_pins, value);

        if (((uint16_t)M6522_RS_PINS & address) == M6522_REG_DDRB)
        {
            // Setting DDRB / Set pins to in/output
            gpio_dirs &= ~((uint32_t)GPIO_PORTB_MASK);
            gpio_dirs |= (uint32_t)(value << GPIO_PORTB_BASE_PIN) & (uint32_t)GPIO_PORTB_MASK;
            gpio_set_dir_all_bits(gpio_dirs);
            // TO DO
            // sleep_ms(1); //Found delay neccessary after updating the DDR for a 1602A display, 
            // else it will miss first couple of characters. Instead of modifying this C routine,
            // adding a delay in the 65C02 assembly code, to count down from #128 is enough upon
            // DDR change. It is not clear why, for example code with another device, specifically
            // an SD card which also performs DDR changes seems to work fine.
        }

        if (((uint16_t)M6522_RS_PINS & address) == M6522_REG_RB)
        {
            // Setting DDRB / Set pins to in/output
            gpio_outs &= ~((uint32_t)GPIO_PORTB_MASK);
            gpio_outs |= (uint32_t)(value << GPIO_PORTB_BASE_PIN) & (uint32_t)GPIO_PORTB_MASK;
            gpio_put_masked(gpio_dirs, gpio_outs);
        }

        if (((uint16_t)M6522_RS_PINS & address) == M6522_REG_DDRA)
        {
            // Setting DDRB / Set pins to in/output
            gpio_dirs &= ~((uint32_t)GPIO_PORTA_MASK);
            gpio_dirs |= (uint32_t)(value << GPIO_PORTA_BASE_PIN) & (uint32_t)GPIO_PORTA_MASK;
            gpio_set_dir_all_bits(gpio_dirs);
        }

        if (((uint16_t)M6522_RS_PINS & address) == M6522_REG_RA)
        {
            // Setting DDRB / Set pins to in/output
            gpio_outs &= ~((uint32_t)GPIO_PORTA_MASK);
            gpio_outs |= (uint32_t)(value << GPIO_PORTA_BASE_PIN) & (uint32_t)GPIO_PORTA_MASK;
            gpio_put_masked(gpio_dirs, gpio_outs);
        }

        via_pins = m6522_tick(&via, via_pins);

        via_update();
        // old_ticks > 0 ? old_ticks-- : 0;
#endif
    }
    else
    {
        mem[address] = value;
    }
#endif
}

void callback()
{
#ifdef TESTING
    if ((pc == old_pc) && (old_pc == old_pc1))
    {

        if (clockticks6502 % 1000000 == 0)
        {
            absolute_time_t now = get_absolute_time();
            int64_t elapsed = absolute_time_diff_us(start, now);

            float khz = (double)clockticks6502 / (double)(elapsed);

            printf("Average emulated speed was %.3f MHz\n", khz);
        }

        if (old_pc == 0x24F1)
        {
            printf("65C02 test suite passed sucessfully!\n\n");

            absolute_time_t now = get_absolute_time();
            int64_t elapsed = absolute_time_diff_us(start, now);

            float khz = (double)clockticks6502 / (double)(elapsed);

            printf("Average emulated speed was %.3f MHz\n", khz);
            printf("Average emulated speed was %.3f MHz\n", khz);
        }
        else
        {
            printf("65C02 test suite failed\n");
            printf("pc %04X opcode: %02X test: %d status: %02X \n", old_pc, opcode, mem[0x202], status);
            printf("a %02X x: %02X y: %02X value: %02X \n\n", a, x, y, value);
        }

        running = false;
    }
    old_pc4 = old_pc3;
    old_pc3 = old_pc2;
    old_pc2 = old_pc1;
    old_pc1 = old_pc;
    old_pc = pc;
#endif
    // if (clockticks6502 % 100 == 0) {
    //  absolute_time_t now = get_absolute_time();
    //  int64_t elapsed = absolute_time_diff_us(start, now);

    // float khz = (float)clockticks6502 / (float)(elapsed/1000.0);

    // printf("kHz %.2f\n", khz);
    //}

#ifdef VIA_BASE_ADDRESS
    // one tick for each clock to keep accurate time
    for (uint16_t i = 0; i < clockticks6502 - old_ticks; i++)
    {
        via_pins = m6522_tick(&via, via_pins);
    }

    via_update();

    old_ticks = clockticks6502;
#endif
}

int main()
{

#ifdef OVERCLOCK
    vreg_set_voltage(VREG_VOLTAGE_1_15);
    set_sys_clock_khz(280000, true);
#endif
    stdio_init_all();

    for (uint8_t i = START_DELAY; i > 0; i--)
    {
        printf("Starting in %d \n", i);
        sleep_ms(1000);
    }

    printf("Starting\n");

    if (R_START + R_SIZE > 0x10000)
    {
        printf("Your rom will not fit. Either adjust ROM_START or ROM_SIZE\n");
        while (1)
        {
        }
    }
    for (int i = R_START; i < R_SIZE + R_START; i++)
    {
        mem[i] = R_VAR[i - R_START];
    }

    hookexternal(callback);
    reset6502();
#ifdef VIA_BASE_ADDRESS
    // setup VIA
    m6522_init(&via);
    m6522_reset(&via);
    // gpio_dirs = 0; // GPIO_PORTB_MASK | GPIO_PORTA_MASK;
    gpio_dirs = GPIO_PORTB_MASK | GPIO_PORTA_MASK;
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

    while (running)
    {
        step6502();
    }
    return 0;
}
