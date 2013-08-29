#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "cart.h"

#include "display.h"
#include "serial.h"
#include "timer.h"
#include "cpu.h"
#include "audio.h"
#include "input.h"



///////////////////////////////////////////////////////////////////////////////
// io dispatch

typedef uint8_t (*io_read8_fn)(void* device, uint8_t addr);
typedef void (*io_write8_fn)(void* device, uint8_t addr, uint8_t data);

static struct {
  int device_id;
  uint8_t (*read8)(void* device, uint8_t addr);
  void (*write8)(void* device, uint8_t addr, uint8_t data);
} io_register_functions[] = {
  {DEVICE_INPUT,   (io_read8_fn)read_input_register, (io_write8_fn)write_input_register}, // 0x00
  {DEVICE_SERIAL,  (io_read8_fn)read_serial_data, (io_write8_fn)write_serial_data}, // 0x01
  {DEVICE_SERIAL,  (io_read8_fn)read_serial_control, (io_write8_fn)write_serial_control}, // 0x02
  {-1,             NULL, NULL}, // 0x03
  {DEVICE_TIMER,   (io_read8_fn)read_divider, (io_write8_fn)write_divider}, // 0x04
  {DEVICE_TIMER,   (io_read8_fn)read_timer, (io_write8_fn)write_timer}, // 0x05
  {DEVICE_TIMER,   (io_read8_fn)read_timer_mod, (io_write8_fn)write_timer_mod}, // 0x06
  {DEVICE_TIMER,   (io_read8_fn)read_timer_control, (io_write8_fn)write_timer_control}, // 0x07
  {-1,             NULL, NULL}, // 0x08
  {-1,             NULL, NULL}, // 0x09
  {-1,             NULL, NULL}, // 0x0A
  {-1,             NULL, NULL}, // 0x0B
  {-1,             NULL, NULL}, // 0x0C
  {-1,             NULL, NULL}, // 0x0D
  {-1,             NULL, NULL}, // 0x0E
  {DEVICE_CPU,     (io_read8_fn)read_interrupt_flag, (io_write8_fn)write_interrupt_flag}, // 0x0F
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x10
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x11
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x12
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x13
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x14
  {-1,             NULL, NULL}, // 0x15
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x16
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x17
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x18
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x19
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x1A
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x1B
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x1C
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x1D
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x1E
  {-1,             NULL, NULL}, // 0x1F
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x20
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x21
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x22
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x23
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x24
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x25
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x26
  {-1,             NULL, NULL}, // 0x27
  {-1,             NULL, NULL}, // 0x28
  {-1,             NULL, NULL}, // 0x29
  {-1,             NULL, NULL}, // 0x2A
  {-1,             NULL, NULL}, // 0x2B
  {-1,             NULL, NULL}, // 0x2C
  {-1,             NULL, NULL}, // 0x2D
  {-1,             NULL, NULL}, // 0x2E
  {-1,             NULL, NULL}, // 0x2F
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x30
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x31
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x32
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x33
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x34
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x35
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x36
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x37
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x38
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x39
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x3A
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x3B
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x3C
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x3D
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x3E
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x3F
  {DEVICE_DISPLAY, (io_read8_fn)read_lcd_reg, (io_write8_fn)write_lcd_reg}, // 0x40
  {DEVICE_DISPLAY, (io_read8_fn)read_lcd_reg, (io_write8_fn)write_lcd_reg}, // 0x41
  {DEVICE_DISPLAY, (io_read8_fn)read_lcd_reg, (io_write8_fn)write_lcd_reg}, // 0x42
  {DEVICE_DISPLAY, (io_read8_fn)read_lcd_reg, (io_write8_fn)write_lcd_reg}, // 0x43
  {DEVICE_DISPLAY, (io_read8_fn)read_lcd_reg, (io_write8_fn)write_lcd_reg}, // 0x44
  {DEVICE_DISPLAY, (io_read8_fn)read_lcd_reg, (io_write8_fn)write_lcd_reg}, // 0x45
  {DEVICE_DISPLAY, (io_read8_fn)read_lcd_reg, (io_write8_fn)write_lcd_reg}, // 0x46
  {DEVICE_DISPLAY, (io_read8_fn)read_lcd_reg, (io_write8_fn)write_lcd_reg}, // 0x47
  {DEVICE_DISPLAY, (io_read8_fn)read_lcd_reg, (io_write8_fn)write_lcd_reg}, // 0x48
  {DEVICE_DISPLAY, (io_read8_fn)read_lcd_reg, (io_write8_fn)write_lcd_reg}, // 0x49
  {DEVICE_DISPLAY, (io_read8_fn)read_lcd_reg, (io_write8_fn)write_lcd_reg}, // 0x4A
  {DEVICE_DISPLAY, (io_read8_fn)read_lcd_reg, (io_write8_fn)write_lcd_reg}, // 0x4B
  {-1,             NULL, NULL}, // 0x4C
  {DEVICE_CPU,     (io_read8_fn)read_speed_switch, (io_write8_fn)write_speed_switch}, // 0x4D
  {-1,             NULL, NULL}, // 0x4E
  {-1,             NULL, NULL}, // 0x4F
  {-1,             NULL, NULL}, // 0x50
  {-1,             NULL, NULL}, // 0x51
  {-1,             NULL, NULL}, // 0x52
  {-1,             NULL, NULL}, // 0x53
  {-1,             NULL, NULL}, // 0x54
  {-1,             NULL, NULL}, // 0x55
  {-1,             NULL, NULL}, // 0x56
  {-1,             NULL, NULL}, // 0x57
  {-1,             NULL, NULL}, // 0x58
  {-1,             NULL, NULL}, // 0x59
  {-1,             NULL, NULL}, // 0x5A
  {-1,             NULL, NULL}, // 0x5B
  {-1,             NULL, NULL}, // 0x5C
  {-1,             NULL, NULL}, // 0x5D
  {-1,             NULL, NULL}, // 0x5E
  {-1,             NULL, NULL}, // 0x5F
  {-1,             NULL, NULL}, // 0x60
  {-1,             NULL, NULL}, // 0x61
  {-1,             NULL, NULL}, // 0x62
  {-1,             NULL, NULL}, // 0x63
  {-1,             NULL, NULL}, // 0x64
  {-1,             NULL, NULL}, // 0x65
  {-1,             NULL, NULL}, // 0x66
  {-1,             NULL, NULL}, // 0x67
  {-1,             NULL, NULL}, // 0x68
  {-1,             NULL, NULL}, // 0x69
  {-1,             NULL, NULL}, // 0x6A
  {-1,             NULL, NULL}, // 0x6B
  {-1,             NULL, NULL}, // 0x6C
  {-1,             NULL, NULL}, // 0x6D
  {-1,             NULL, NULL}, // 0x6E
  {-1,             NULL, NULL}, // 0x6F
  {-1,             NULL, NULL}, // 0x70
  {-1,             NULL, NULL}, // 0x71
  {-1,             NULL, NULL}, // 0x72
  {-1,             NULL, NULL}, // 0x73
  {-1,             NULL, NULL}, // 0x74
  {-1,             NULL, NULL}, // 0x75
  {-1,             NULL, NULL}, // 0x76
  {-1,             NULL, NULL}, // 0x77
  {-1,             NULL, NULL}, // 0x78
  {-1,             NULL, NULL}, // 0x79
  {-1,             NULL, NULL}, // 0x7A
  {-1,             NULL, NULL}, // 0x7B
  {-1,             NULL, NULL}, // 0x7C
  {-1,             NULL, NULL}, // 0x7D
  {-1,             NULL, NULL}, // 0x7E
  {-1,             NULL, NULL}, // 0x7F
  {-1,             NULL, NULL}, // 0x80
  {-1,             NULL, NULL}, // 0x81
  {-1,             NULL, NULL}, // 0x82
  {-1,             NULL, NULL}, // 0x83
  {-1,             NULL, NULL}, // 0x84
  {-1,             NULL, NULL}, // 0x85
  {-1,             NULL, NULL}, // 0x86
  {-1,             NULL, NULL}, // 0x87
  {-1,             NULL, NULL}, // 0x88
  {-1,             NULL, NULL}, // 0x89
  {-1,             NULL, NULL}, // 0x8A
  {-1,             NULL, NULL}, // 0x8B
  {-1,             NULL, NULL}, // 0x8C
  {-1,             NULL, NULL}, // 0x8D
  {-1,             NULL, NULL}, // 0x8E
  {-1,             NULL, NULL}, // 0x8F
  {-1,             NULL, NULL}, // 0x90
  {-1,             NULL, NULL}, // 0x91
  {-1,             NULL, NULL}, // 0x92
  {-1,             NULL, NULL}, // 0x93
  {-1,             NULL, NULL}, // 0x94
  {-1,             NULL, NULL}, // 0x95
  {-1,             NULL, NULL}, // 0x96
  {-1,             NULL, NULL}, // 0x97
  {-1,             NULL, NULL}, // 0x98
  {-1,             NULL, NULL}, // 0x99
  {-1,             NULL, NULL}, // 0x9A
  {-1,             NULL, NULL}, // 0x9B
  {-1,             NULL, NULL}, // 0x9C
  {-1,             NULL, NULL}, // 0x9D
  {-1,             NULL, NULL}, // 0x9E
  {-1,             NULL, NULL}, // 0x9F
  {-1,             NULL, NULL}, // 0xA0
  {-1,             NULL, NULL}, // 0xA1
  {-1,             NULL, NULL}, // 0xA2
  {-1,             NULL, NULL}, // 0xA3
  {-1,             NULL, NULL}, // 0xA4
  {-1,             NULL, NULL}, // 0xA5
  {-1,             NULL, NULL}, // 0xA6
  {-1,             NULL, NULL}, // 0xA7
  {-1,             NULL, NULL}, // 0xA8
  {-1,             NULL, NULL}, // 0xA9
  {-1,             NULL, NULL}, // 0xAA
  {-1,             NULL, NULL}, // 0xAB
  {-1,             NULL, NULL}, // 0xAC
  {-1,             NULL, NULL}, // 0xAD
  {-1,             NULL, NULL}, // 0xAE
  {-1,             NULL, NULL}, // 0xAF
  {-1,             NULL, NULL}, // 0xB0
  {-1,             NULL, NULL}, // 0xB1
  {-1,             NULL, NULL}, // 0xB2
  {-1,             NULL, NULL}, // 0xB3
  {-1,             NULL, NULL}, // 0xB4
  {-1,             NULL, NULL}, // 0xB5
  {-1,             NULL, NULL}, // 0xB6
  {-1,             NULL, NULL}, // 0xB7
  {-1,             NULL, NULL}, // 0xB8
  {-1,             NULL, NULL}, // 0xB9
  {-1,             NULL, NULL}, // 0xBA
  {-1,             NULL, NULL}, // 0xBB
  {-1,             NULL, NULL}, // 0xBC
  {-1,             NULL, NULL}, // 0xBD
  {-1,             NULL, NULL}, // 0xBE
  {-1,             NULL, NULL}, // 0xBF
  {-1,             NULL, NULL}, // 0xC0
  {-1,             NULL, NULL}, // 0xC1
  {-1,             NULL, NULL}, // 0xC2
  {-1,             NULL, NULL}, // 0xC3
  {-1,             NULL, NULL}, // 0xC4
  {-1,             NULL, NULL}, // 0xC5
  {-1,             NULL, NULL}, // 0xC6
  {-1,             NULL, NULL}, // 0xC7
  {-1,             NULL, NULL}, // 0xC8
  {-1,             NULL, NULL}, // 0xC9
  {-1,             NULL, NULL}, // 0xCA
  {-1,             NULL, NULL}, // 0xCB
  {-1,             NULL, NULL}, // 0xCC
  {-1,             NULL, NULL}, // 0xCD
  {-1,             NULL, NULL}, // 0xCE
  {-1,             NULL, NULL}, // 0xCF
  {-1,             NULL, NULL}, // 0xD0
  {-1,             NULL, NULL}, // 0xD1
  {-1,             NULL, NULL}, // 0xD2
  {-1,             NULL, NULL}, // 0xD3
  {-1,             NULL, NULL}, // 0xD4
  {-1,             NULL, NULL}, // 0xD5
  {-1,             NULL, NULL}, // 0xD6
  {-1,             NULL, NULL}, // 0xD7
  {-1,             NULL, NULL}, // 0xD8
  {-1,             NULL, NULL}, // 0xD9
  {-1,             NULL, NULL}, // 0xDA
  {-1,             NULL, NULL}, // 0xDB
  {-1,             NULL, NULL}, // 0xDC
  {-1,             NULL, NULL}, // 0xDD
  {-1,             NULL, NULL}, // 0xDE
  {-1,             NULL, NULL}, // 0xDF
  {-1,             NULL, NULL}, // 0xE0
  {-1,             NULL, NULL}, // 0xE1
  {-1,             NULL, NULL}, // 0xE2
  {-1,             NULL, NULL}, // 0xE3
  {-1,             NULL, NULL}, // 0xE4
  {-1,             NULL, NULL}, // 0xE5
  {-1,             NULL, NULL}, // 0xE6
  {-1,             NULL, NULL}, // 0xE7
  {-1,             NULL, NULL}, // 0xE8
  {-1,             NULL, NULL}, // 0xE9
  {-1,             NULL, NULL}, // 0xEA
  {-1,             NULL, NULL}, // 0xEB
  {-1,             NULL, NULL}, // 0xEC
  {-1,             NULL, NULL}, // 0xED
  {-1,             NULL, NULL}, // 0xEE
  {-1,             NULL, NULL}, // 0xEF
  {-1,             NULL, NULL}, // 0xF0
  {-1,             NULL, NULL}, // 0xF1
  {-1,             NULL, NULL}, // 0xF2
  {-1,             NULL, NULL}, // 0xF3
  {-1,             NULL, NULL}, // 0xF4
  {-1,             NULL, NULL}, // 0xF5
  {-1,             NULL, NULL}, // 0xF6
  {-1,             NULL, NULL}, // 0xF7
  {-1,             NULL, NULL}, // 0xF8
  {-1,             NULL, NULL}, // 0xF9
  {-1,             NULL, NULL}, // 0xFA
  {-1,             NULL, NULL}, // 0xFB
  {-1,             NULL, NULL}, // 0xFC
  {-1,             NULL, NULL}, // 0xFD
  {-1,             NULL, NULL}, // 0xFE
  {DEVICE_CPU,     (io_read8_fn)read_interrupt_enable, (io_write8_fn)write_interrupt_enable}, // 0xFF
};

uint8_t io_read8(struct memory* m, uint8_t addr) {
  if (io_register_functions[addr].read8) {
    void* device = m->devices[io_register_functions[addr].device_id];
    if (!device) {
      printf("> read from unattached device type %d\n", io_register_functions[addr].device_id);
      return 0;
    }
    return io_register_functions[addr].read8(device, addr);
  }
  printf("> unhandled io_read8  : %02X\n", addr);
  return 0;
}

void io_write8(struct memory* m, uint8_t addr, uint8_t data) {
  if (io_register_functions[addr].write8) {
    void* device = m->devices[io_register_functions[addr].device_id];
    io_register_functions[addr].write8(device, addr, data);
  } else
    printf("> unhandled io_write8 : %02X = %02X\n", addr, data);
}



///////////////////////////////////////////////////////////////////////////////
// exported calls

// TODO: we only implement MBC1 for now; implement more MBCs later.

void* ptr(struct memory* m, uint16_t addr) {
  if (addr == m->write_breakpoint_addr)
    printf("warning: memory breakpoint\n");

  if (addr >= 0xE000 && addr < 0xFE00)
    addr -= 0x2000;

  if (addr < 0x4000)
    return &m->cart->data[addr]; // rom bank 0
  if (addr < 0x8000)
    return &m->cart->data[addr - 0x4000 + m->cart_rom_bank_num * 0x4000]; // rom bank 01-7F
  if (addr < 0xA000)
    return &m->vram[addr - 0x8000 + m->vram_bank_num * 0x2000];
  if (addr < 0xC000)
    return m->eram ? &m->eram[addr - 0xA000 + m->eram_bank_num * 0x2000] : NULL;
  if (addr < 0xD000)
    return &m->wram[addr - 0xC000];
  if (addr < 0xE000)
    return &m->wram[addr - 0xD000 + m->wram_bank_num * 0x1000];
  if (addr < 0xFE00)
    return NULL; // unsupported for now
  if (addr < 0xFEA0)
    return &m->sprite_table[addr - 0xFE00];
  if (addr < 0xFF00)
    return NULL; // nothing there
  if (addr < 0xFF80)
    return NULL; // IO regs; handled specially by read/write functions
  return &m->hram[addr - 0xFF80];
}

uint8_t read8(struct memory* m, uint16_t addr) {
  if ((addr >= 0xFF00 && addr < 0xFF80) || addr == 0xFFFF)
    return io_read8(m, addr & 0xFF);
  return m->read8(m, addr);
}

uint16_t read16(struct memory* m, uint16_t addr) {
  return m->read16(m, addr);
}

void write8(struct memory* m, uint16_t addr, uint8_t data) {
  if ((addr >= 0xFF00 && addr < 0xFF80) || addr == 0xFFFF)
    io_write8(m, addr & 0xFF, data);
  else
    m->write8(m, addr, data);
}

void write16(struct memory* m, uint16_t addr, uint16_t data) {
  m->write16(m, addr, data);
}



///////////////////////////////////////////////////////////////////////////////
// Default behavior (for no MBC or MBCs with simple behaviors like MBC1)

uint8_t default_mbc_read8(struct memory* m, uint16_t addr) {
  return *(uint8_t*)ptr(m, addr);
}

uint16_t default_mbc_read16(struct memory* m, uint16_t addr) {
  return *(uint16_t*)ptr(m, addr);
}

void default_mbc_write8(struct memory* m, uint16_t addr, uint8_t data) {
  // <0x8000 isn't writable
  if (addr < 0x8000)
    return;

  uint8_t* x = (uint8_t*)ptr(m, addr);
  if (x)
    *x = data;
}

void default_mbc_write16(struct memory* m, uint16_t addr, uint16_t data) {
  // <0x8000 isn't writable
  if (addr < 0x8000)
    return;

  uint16_t* x = (uint16_t*)ptr(m, addr);
  if (x)
    *x = data;
}



///////////////////////////////////////////////////////////////////////////////
// MBC1

struct mbc1_data {
  uint8_t ram_enable; // not enforced
  uint8_t rom_bank_num_low; // 5 bits
  uint8_t rom_bank_num_high; // 2 bits; also used as ram bank num
  uint8_t rom_ram_mode_select;
};

#define MBC1_REGS(m) ((struct mbc1_data*)m->mbc_data)

void mbc1_set_bank_numbers(struct memory* m) {
  if (MBC1_REGS(m)->rom_ram_mode_select) {
    m->cart_rom_bank_num = MBC1_REGS(m)->rom_bank_num_low;
    m->wram_bank_num = MBC1_REGS(m)->rom_bank_num_high;
  } else {
    m->cart_rom_bank_num = MBC1_REGS(m)->rom_bank_num_low | (MBC1_REGS(m)->rom_bank_num_high << 5);
    m->wram_bank_num = 1;
  }
}

#define mbc1_read8 default_mbc_read8
#define mbc1_read16 default_mbc_read16

void mbc1_write8(struct memory* m, uint16_t addr, uint8_t data) {

  if (addr < 0x2000)
    MBC1_REGS(m)->ram_enable = data;

  else if (addr < 0x4000) {
    if (!data)
      data = 1;
    MBC1_REGS(m)->rom_bank_num_low = data & 0x1F;
    mbc1_set_bank_numbers(m);

  } else if (addr < 0x6000) {
    MBC1_REGS(m)->rom_bank_num_high = data & 0x03;
    mbc1_set_bank_numbers(m);

  } else if (addr < 0x8000) {
    MBC1_REGS(m)->rom_ram_mode_select = data & 1;
    mbc1_set_bank_numbers(m);

  } else
    default_mbc_write8(m, addr, data);
}

void mbc1_write16(struct memory* m, uint16_t addr, uint16_t data) {
  // all the MBC1 regs are <8 bits so we can just truncate the data if we're
  // write16'ing an MBC1 reg
  if (addr < 0x8000)
    mbc1_write8(m, addr, data);
  else
    default_mbc_write16(m, addr, data);
}



///////////////////////////////////////////////////////////////////////////////
// global management functions

struct memory* create_memory(union cart_data* cart) {

  int eram_size = ram_size_for_ram_size_code(cart->header.ram_size);
  const struct cart_type_info* type_info = type_info_for_cart_type(cart->header.cart_type);

  struct memory* m = calloc(1, sizeof(struct memory));
  m->cart = cart;
  m->cart_rom_bank_num = 1;
  m->wram_bank_num = 1;

  m->vram = (uint8_t*)malloc(0x4000);
  m->eram = eram_size ? (uint8_t*)malloc(eram_size) : NULL;
  m->wram = (uint8_t*)malloc(0x8000);
  m->sprite_table = (uint8_t*)malloc(0xA0);
  m->hram = (uint8_t*)malloc(0x80);

  m->write_breakpoint_addr = 0x10000;

  if (type_info->class_id == CART_CLASS_SIMPLE) {
    m->read8 = default_mbc_read8;
    m->read16 = default_mbc_read16;
    m->write8 = default_mbc_write8;
    m->write16 = default_mbc_write16;
  } else if (type_info->class_id == CART_CLASS_MBC1) {
    m->mbc_data = malloc(sizeof(struct mbc1_data));
    m->read8 = mbc1_read8;
    m->read16 = mbc1_read16;
    m->write8 = mbc1_write8;
    m->write16 = mbc1_write16;
  }

  return m;
}

void delete_memory(struct memory* m) {

  if (m) {
    if (m->vram)
      free(m->vram);
    if (m->eram)
      free(m->eram);
    if (m->wram)
      free(m->wram);
    if (m->sprite_table)
      free(m->sprite_table);
    if (m->hram)
      free(m->hram);
    if (m->mbc_data)
      free(m->mbc_data);

    free(m);
  }
}

void add_device(struct memory* m, int device_id, void* device) {
  m->devices[device_id] = device;
}

void update_devices(struct memory* m, uint64_t cycles) {
  if (m->devices[DEVICE_DISPLAY])
    display_update(m->devices[DEVICE_DISPLAY], cycles);
  if (m->devices[DEVICE_TIMER])
    timer_update(m->devices[DEVICE_TIMER], cycles);
  if (m->devices[DEVICE_INPUT])
    input_update(m->devices[DEVICE_INPUT], cycles);
}
