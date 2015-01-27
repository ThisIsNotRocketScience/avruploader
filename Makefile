all: avruploader.bin

GCCARMBIN=/opt/sat/bin
CC=$(GCCARMBIN)/arm-none-eabi-gcc
CFLAGS=-mcpu=cortex-m0 -mthumb -mlittle-endian -O2 -std=gnu99 
LDFLAGS=

clean:
	rm -f *.o *.axf avruploader.bin arduino-bootloader.inc arduino-blinky.inc avruploader.map

avruploader.o: Makefile arduino-bootloader.inc arduino-blinky.inc

avruploader.elf: avruploader.o avruploader.ld Makefile
	$(CC) avruploader.o $(CFLAGS) -nostdlib -T avruploader.ld -Wl,--gc-sections -Wl,-Map=avruploader.map -o avruploader.elf

avruploader.bin: avruploader.elf
	$(GCCARMBIN)/arm-none-eabi-objcopy avruploader.elf -O binary avruploader.bin

flash: avruploader.elf
	sudo openocd -f /usr/local/share/openocd/scripts/board/32f0308discovery.cfg -c 'init' -c "program avruploader.elf verify reset"

gdb:
	arm-none-eabi-gdb avruploader.elf

arduino-bootloader.inc: arduino-bootloader.bin
	xxd -i < arduino-bootloader.bin >arduino-bootloader.inc
arduino-blinky.inc: arduino-bootloader.bin
	xxd -i < arduino-blinky.bin >arduino-blinky.inc
