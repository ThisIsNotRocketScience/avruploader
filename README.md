AVR uploader
============

This is a very simple but perhaps useful firmware example for an STM32F030
microcontroller. The firmware runs on an STM32 chip with a few extra
components, and can program ATmega328 chip, either new or used, with an Arduino
(or other) bootloader and the blinky (or other) example. Example uses:

- checking if your ATmega328 chip is still alive after having an accident 
making your own circuit

- programming bootloader into the replacement ATmega quickly if the old one
turns out to be dead

- preparing a fresh-from-factory ATmega chip for your own Arduino-based circuit,
so that you can keep your Arduino for future projects and experiments

- flooding the market with cheap Arduino clones (just kidding, it's not
economically viable really)

Usage
=====

Insert the chip into the ZIF socket and press the reset button.

If the red LED is lit continuously and you hear a long beep, the chip hasn't
been detected, might be a burned chip or simply bad soldering.

If the red LED is lit continuously and you hear two short beeps, the chip
signature is wrong - most likely a wrong chip type or maybe a faulty chip.

Both red and green LED indicate programming failure. Try again, maybe it will
work.

Three short beeps and green LED indicate success. Now if you put that
chip in your Arduino's socket, it should blink an LED.

Requirements
============

Due to licensing, I haven't included the Arduino bootloader and blinky binaries
in the repository. However, if you put the correct binaries with the correct
names (arduino-bootloader.bin and arduino-blinky.bin) in the project directory,
it will use them during compilation. The bootloader you can safely copy from
the Arduino distribution. The version I used for my device was the one in:

hardware/arduino/bootloaders/optiboot/optiboot_atmega328.hex

The blinky binary is simply the blinky example compiled to binary, feel free
to substitute with any other *very small* binary you wish, as long as it
fits in the microcontroller flash with the rest of the project.

Hardware
========

2x 1K resistors
10K resistor
2N3904 transistor
TMB12A buzzer
2x 100nF capacitors
STM32F030F4P6
16 MHz crystal
2x 22p capacitors
red LED
green LED
push button
ZIF28 socket ("narrow" type, cheaply available on ebay)

Circuit diagram and/or circuit board example coming soon, if anyone requests it.

MCU port pins:

A0 - TMB12A piezo beeper (must be connected via an NPN transistor like 2N3904
in common emitter configuration, in order to keep the pin current low)
A2 - red LED (through a 1K resistor)
A3 - green LED (through a 1K resistor)
RESET - push button to GND

B1 - AVR reset
A4 - AVR slave select
A5 - AVR SCK
A6 - AVR MISO
A7 - AVR MOSI

The AVR socket needs to have the clock crystal (16 MHz) and load capacitors (22pF ish)
connected to it.

Also, put a 100nF bypass capacitor between Vcc and GND of both AVR and STM32.
