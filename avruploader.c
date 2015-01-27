#include <stdint.h>

typedef void (*irqhandler)(void);
extern void __StackTop();
extern void start();
typedef void (*static_init)();

// page 34 datasheet
#define RCC_BLOCK (0x40021000)
#define GPIOA_BLOCK (0x48000000)
#define GPIOB_BLOCK (0x48000400)
#define GPIOC_BLOCK (0x48000800)
#define GPIOD_BLOCK (0x48000C00)
#define IWDG_BLOCK (0x40003000)
#define SPI1_BLOCK (0x40013000)

enum PinMode
{
    PIN_MODE_INPUT = 0,
    PIN_MODE_OUTPUT = 1,
    PIN_MODE_ALTERNATE = 2,
    PIN_MODE_ANALOG = 3,
};

void SetGPIOMode(int bank, int pin, enum PinMode mode)
{
    volatile uint32_t *reg = (volatile uint32_t *)(GPIOA_BLOCK + 0x400 * bank + 0x00);
    int shift = pin << 1;
    *reg = (*reg &~ (3 << shift)) | (mode << shift);
}

void SetGPIOAltMode(int bank, int pin, int altfunc)
{
    volatile uint32_t *reg = (volatile uint32_t *)(GPIOA_BLOCK + 0x400 * bank + 0x00);
    int shift = pin << 1;
    *reg = (*reg &~ (3 << shift)) | (PIN_MODE_ALTERNATE << shift);
    
    reg += 0x20;
    shift = (pin & 7) << 4;
    // use AFRH instead of AFRL for pins 8..15
    reg += (pin & 8) >> 1;
    *reg = (*reg &~ (15 << shift)) | (altfunc << shift);
}

void SetGPIOPin(int bank, int pin, int value)
{
    volatile uint32_t *reg = (volatile uint32_t *)(GPIOA_BLOCK + 0x400 * bank + 0x18);
    *reg |= 1 << ((value ? 0 : 16) + pin);
}

void SetResetPin(int reset_pin_state)
{
    SetGPIOPin(1, 1, reset_pin_state);
}

void SetSlaveSelect(int pin_state)
{
    SetGPIOPin(0, 4, pin_state);
}

void SetBeeper(int pin_state)
{
    SetGPIOPin(0, 0, pin_state);
}

void SetRedLED(int pin_state)
{
    SetGPIOPin(0, 2, pin_state);
}

void SetGreenLED(int pin_state)
{
    SetGPIOPin(0, 3, pin_state);
}

uint16_t SPIGetStatus()
{
    return *(uint16_t volatile *)(SPI1_BLOCK + 0x8);
}

uint32_t SPISendRecvByte(uint8_t value)
{
    // wait for busy flag to disappear
    while (SPIGetStatus() & 128)
        ;
    // wait for space
    while (!(SPIGetStatus() & 2))
        ;
    *(volatile uint8_t *)(SPI1_BLOCK + 0x0C) = value;
    while (SPIGetStatus() & 128)
        ;
    // wait for data
    while (!(SPIGetStatus() & 1))
        ;
    return *(volatile uint8_t *)(SPI1_BLOCK + 0x0C);
}



void InitSPI()
{
    //SetGPIOMode(0, 4, PIN_MODE_OUTPUT); // slave select
    
    SetGPIOAltMode(0, 5, 0); // SCK
    SetGPIOAltMode(0, 6, 0); // MISO
    SetGPIOAltMode(0, 7, 0); // MOSI
    
    SetGPIOMode(1, 1, PIN_MODE_OUTPUT); // reset
    
    SetSlaveSelect(1);
    SetResetPin(0); // hold the chip in reset

    int speed = 7;
    *(uint16_t volatile *)(SPI1_BLOCK + 0x0) = (1 << 2) | (speed << 3) | (1 << 6) | (1 << 9) | (1 << 8); // master enable, fclk/16, enable; cpol=cpha=0, software slave select, slave not selected
    *(uint16_t volatile *)(SPI1_BLOCK + 0x4) = 1 << 12; // 8-bit mode
}

void InitLED()
{
    SetGPIOMode(0, 0, PIN_MODE_OUTPUT); // beeper
    SetGPIOMode(0, 2, PIN_MODE_OUTPUT);
    SetGPIOMode(0, 3, PIN_MODE_OUTPUT);
}

void DelayMs(int ms)
{
    for(volatile int i = 0; i < 400 * ms; i++)
        ;
}

uint32_t AtmegaCmd(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4)
{
    uint32_t res = SPISendRecvByte(byte1);
    res |= SPISendRecvByte(byte2) << 8;
    res |= SPISendRecvByte(byte3) << 16;
    res |= SPISendRecvByte(byte4) << 24;
    return res;
}

uint8_t AtmegaQuery(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4)
{
    SPISendRecvByte(byte1);
    SPISendRecvByte(byte2);
    SPISendRecvByte(byte3);
    return SPISendRecvByte(byte4);
}

static const uint8_t bootloader1[] = {
    #include "arduino-bootloader.inc"
};

static const uint8_t blinky[] = {
    #include "arduino-blinky.inc"
};

void WaitForFlash(int delay)
{
    while(AtmegaQuery(0xF0, 0, 0, 0) & 1) {
        //*(uint32_t *)(IWDG_BLOCK) = 0xAAAA;
        SetGreenLED(1);
        DelayMs(delay);
        SetGreenLED(0);
        DelayMs(delay);
    }
}

void ProgramRange(uint32_t address, const uint8_t *data, uint32_t len)
{
    uint32_t end_addr = address + len;
    uint32_t orig_len = len;
    while(len > 0)
    {
        uint32_t addr = end_addr - len;
        AtmegaCmd((addr & 1) ? 0x48 : 0x40, 0, (addr >> 1) & 63, *data++);
        if ((addr & 63) == 63)
        {
            // last byte of the page
            uint32_t page_addr = addr & ~63;
            AtmegaCmd(0x4C, page_addr >> 9, page_addr >> 1, 0);
            WaitForFlash(10);
        }
        len--;
    }
    if (end_addr & 63)
    {
        uint32_t page_addr = end_addr & ~63;
        AtmegaCmd(0x4C, page_addr >> 9, page_addr >> 1, 0);
        WaitForFlash(10);
    }
    for(uint32_t i = 0; i < len; i++)
    {
        uint32_t addr = address + i;
        if (data[i] != AtmegaQuery((addr & 1) ? 0x28 : 0x20, (addr >> 9), (addr >> 1), 0))
        {
            SetRedLED(1);
            SetGreenLED(1);
            while(1) ;
        }
        
    }
}

void beep(int ton, int toff)
{
    SetBeeper(1);
    DelayMs(ton);
    SetBeeper(0);
    DelayMs(toff);
}

void hang()
{
    while(1)
        __asm ("wfe");
}

int main()
{
    *(volatile uint32_t *)(RCC_BLOCK + 0x8) = 0; // interrupt disable
    *(volatile uint32_t *)(RCC_BLOCK + 0x14) |= (1 << 2) | (1 << 17) | (1 << 18); // Enable SRAM, IOA, IOB
    *(volatile uint32_t *)(RCC_BLOCK + 0x18) |= (1 << 12);

    InitLED();
    InitSPI();
    
    int found = 0;
    int ntry = 0;
    while(1)
    {
        ntry++;
        SetRedLED(1);
        DelayMs(10);
        SetResetPin(0);
        DelayMs(90);
        SetRedLED(0);
        
        uint32_t ack = AtmegaCmd(0xAC, 0x53, 0, 0);
        if (ack == (0x53 << 16))
        {
            found = 1;
            break;
        }
        SetResetPin(1);
        SetSlaveSelect(1);
        DelayMs(100);
        if (ntry == 3)
        {
            SetRedLED(1);
            beep(500, 0);
            hang();
        }
    }
    SetRedLED(1);
    uint8_t lock = AtmegaQuery(0x58, 0, 0, 0);
    uint8_t signature1 = AtmegaQuery(0x30, 0, 0, 0);
    uint8_t signature2 = AtmegaQuery(0x30, 0, 1, 0);
    uint8_t signature3 = AtmegaQuery(0x30, 0, 2, 0);
    uint8_t signature4 = AtmegaQuery(0x30, 0, 3, 0);
    uint8_t lfuse = AtmegaQuery(0x50, 0, 0, 0);
    uint8_t hfuse = AtmegaQuery(0x58, 8, 0, 0);
    uint8_t efuse = AtmegaQuery(0x50, 8, 0, 0);
    uint8_t calibration = AtmegaQuery(0x38, 0, 0, 0);
    // require atmega328p or atmega328
    if (signature1 != 0x1e || signature2 != 0x95 || (signature3 != 0x14 && signature3 != 0x0F))
    {
        SetBeeper(1);
        DelayMs(100);
        SetBeeper(0);
        DelayMs(100);
        SetBeeper(1);
        DelayMs(100);
        SetBeeper(0);
        __asm ("bkpt");
        hang();
    }
    
    SetRedLED(0);
    AtmegaCmd(0xAC, 0x80, 0, 0);
    WaitForFlash(200);
    ProgramRange(0x7e00, bootloader1, sizeof(bootloader1));
    ProgramRange(0x0000, blinky, sizeof(blinky));
    // set lock bits (protect bootloader)
    AtmegaCmd(0xAC, 0xE0, 0, 0x0F);
    AtmegaCmd(0xAC, 0xA0, 0, 0xFF); // fuse
    AtmegaCmd(0xAC, 0xA8, 0, 0xDE); // high fuse (reset not disabled, enable debugwire, enable SPI programming, watchdog timer not enabled, EEPROM not preserved on chip erase, boot size 3, reset vector unprogrammed)
    AtmegaCmd(0xAC, 0xA4, 0, 0x05); // ext fuse (brown out detector trigger level)
    
    SetGreenLED(1);
    beep(10, 100);
    beep(10, 100);
    beep(10, 0);
    hang();
    return 0;
}

void HardFault()
{
    while(1) ;
}

void NMI()
{
    while(1) ;
}

extern uint32_t __bss_start__;	// zero out uninitialized ram, for the sake of c/c++-libs
extern uint32_t __bss_end__;

extern uint32_t __etext;
extern uint32_t __data_start__;	// copy flash -> initialized ram
extern uint32_t __data_end__;

void crt0()
{
    // *(uint32_t *)(RCC_BLOCK + 0x14) |= (1 << 2); // SRAM
    uint32_t *t, *f;
    for (t = &__data_start__, f = &__etext; t != &__data_end__; t++)
        *t = *f++;
    for (t = &__bss_start__; t != &__bss_end__; t++)
        *t = 0;
    main();
}

__attribute__ ((section(".isr_vector")))
irqhandler irqhandlers[48] = {
  &__StackTop,		// Stack top defined by linker-script
  (irqhandler)crt0,	// The reset handler
  (irqhandler)NMI,
  (irqhandler)HardFault,
  0			
};
