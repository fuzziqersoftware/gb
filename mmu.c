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
#include "debug.h"
#include "terminal.h"



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
  {DEVICE_INVALID, NULL, NULL}, // 0x03
  {DEVICE_TIMER,   (io_read8_fn)read_divider, (io_write8_fn)write_divider}, // 0x04
  {DEVICE_TIMER,   (io_read8_fn)read_timer, (io_write8_fn)write_timer}, // 0x05
  {DEVICE_TIMER,   (io_read8_fn)read_timer_mod, (io_write8_fn)write_timer_mod}, // 0x06
  {DEVICE_TIMER,   (io_read8_fn)read_timer_control, (io_write8_fn)write_timer_control}, // 0x07
  {DEVICE_INVALID, NULL, NULL}, // 0x08
  {DEVICE_INVALID, NULL, NULL}, // 0x09
  {DEVICE_INVALID, NULL, NULL}, // 0x0A
  {DEVICE_INVALID, NULL, NULL}, // 0x0B
  {DEVICE_INVALID, NULL, NULL}, // 0x0C
  {DEVICE_INVALID, NULL, NULL}, // 0x0D
  {DEVICE_INVALID, NULL, NULL}, // 0x0E
  {DEVICE_CPU,     (io_read8_fn)read_interrupt_flag, (io_write8_fn)write_interrupt_flag}, // 0x0F
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x10
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x11
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x12
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x13
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x14
  {DEVICE_INVALID, NULL, NULL}, // 0x15
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x16
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x17
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x18
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x19
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x1A
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x1B
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x1C
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x1D
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x1E
  {DEVICE_INVALID, NULL, NULL}, // 0x1F
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x20
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x21
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x22
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x23
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x24
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x25
  {DEVICE_AUDIO,   (io_read8_fn)read_audio_register, (io_write8_fn)write_audio_register}, // 0x26
  {DEVICE_INVALID, NULL, NULL}, // 0x27
  {DEVICE_INVALID, NULL, NULL}, // 0x28
  {DEVICE_INVALID, NULL, NULL}, // 0x29
  {DEVICE_INVALID, NULL, NULL}, // 0x2A
  {DEVICE_INVALID, NULL, NULL}, // 0x2B
  {DEVICE_INVALID, NULL, NULL}, // 0x2C
  {DEVICE_INVALID, NULL, NULL}, // 0x2D
  {DEVICE_INVALID, NULL, NULL}, // 0x2E
  {DEVICE_INVALID, NULL, NULL}, // 0x2F
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
  {DEVICE_INVALID, NULL, NULL}, // 0x4C
  {DEVICE_CPU,     (io_read8_fn)read_speed_switch, (io_write8_fn)write_speed_switch}, // 0x4D
  {DEVICE_INVALID, NULL, NULL}, // 0x4E
  {-1,             NULL, NULL}, // 0x4F
  {DEVICE_INVALID, NULL, NULL}, // 0x50
  {-1,             NULL, NULL}, // 0x51
  {-1,             NULL, NULL}, // 0x52
  {-1,             NULL, NULL}, // 0x53
  {-1,             NULL, NULL}, // 0x54
  {-1,             NULL, NULL}, // 0x55
  {-1,             NULL, NULL}, // 0x56
  {DEVICE_INVALID, NULL, NULL}, // 0x57
  {DEVICE_INVALID, NULL, NULL}, // 0x58
  {DEVICE_INVALID, NULL, NULL}, // 0x59
  {DEVICE_INVALID, NULL, NULL}, // 0x5A
  {DEVICE_INVALID, NULL, NULL}, // 0x5B
  {DEVICE_INVALID, NULL, NULL}, // 0x5C
  {DEVICE_INVALID, NULL, NULL}, // 0x5D
  {DEVICE_INVALID, NULL, NULL}, // 0x5E
  {DEVICE_INVALID, NULL, NULL}, // 0x5F
  {DEVICE_INVALID, NULL, NULL}, // 0x60
  {DEVICE_INVALID, NULL, NULL}, // 0x61
  {DEVICE_INVALID, NULL, NULL}, // 0x62
  {DEVICE_INVALID, NULL, NULL}, // 0x63
  {DEVICE_INVALID, NULL, NULL}, // 0x64
  {DEVICE_INVALID, NULL, NULL}, // 0x65
  {DEVICE_INVALID, NULL, NULL}, // 0x66
  {DEVICE_INVALID, NULL, NULL}, // 0x67
  {-1,             NULL, NULL}, // 0x68
  {-1,             NULL, NULL}, // 0x69
  {-1,             NULL, NULL}, // 0x6A
  {-1,             NULL, NULL}, // 0x6B
  {-1,             NULL, NULL}, // 0x6C
  {DEVICE_INVALID, NULL, NULL}, // 0x6D
  {DEVICE_INVALID, NULL, NULL}, // 0x6E
  {DEVICE_INVALID, NULL, NULL}, // 0x6F
  {-1,             NULL, NULL}, // 0x70
  {DEVICE_INVALID, NULL, NULL}, // 0x71
  {-1,             NULL, NULL}, // 0x72
  {-1,             NULL, NULL}, // 0x73
  {-1,             NULL, NULL}, // 0x74
  {-1,             NULL, NULL}, // 0x75
  {-1,             NULL, NULL}, // 0x76
  {-1,             NULL, NULL}, // 0x77
  {DEVICE_INVALID, NULL, NULL}, // 0x78
  {DEVICE_INVALID, NULL, NULL}, // 0x79
  {DEVICE_INVALID, NULL, NULL}, // 0x7A
  {DEVICE_INVALID, NULL, NULL}, // 0x7B
  {DEVICE_INVALID, NULL, NULL}, // 0x7C
  {DEVICE_INVALID, NULL, NULL}, // 0x7D
  {DEVICE_INVALID, NULL, NULL}, // 0x7E
  {DEVICE_INVALID, NULL, NULL}, // 0x7F
};

void* device_for_addr(struct memory* m, uint8_t addr) {
  if (addr >= 0x80) {
    fprintf(stderr, "> internal error! device_for_addr called with addr=%02X\n", addr);
    return NULL;
  }

  int device_id = io_register_functions[addr].device_id;
  if (device_id == DEVICE_INVALID)
    return NULL; // silly rom, there's no device here
  if (device_id < 0 || device_id > NUM_DEVICE_TYPES) {
    fprintf(stderr, "> access of unimplemented device type %d at address FF%02X\n", device_id, addr);
    return NULL;
  }

  if (!m->devices[device_id])
    fprintf(stderr, "> access of unattached device type %d\n", device_id);
  return m->devices[device_id];
}

uint8_t io_read8(struct memory* m, uint8_t addr) {
  void* device = device_for_addr(m, addr);
  return device ? io_register_functions[addr].read8(device, addr) : 0;
}

void io_write8(struct memory* m, uint8_t addr, uint8_t data) {
  void* device = device_for_addr(m, addr);
  if (device)
    io_register_functions[addr].write8(device, addr, data);
}



///////////////////////////////////////////////////////////////////////////////
// exported calls

// TODO: we only implement MBC1 for now; implement more MBCs later.

int valid_ptr(struct memory* m, uint16_t addr) {
  if (addr >= 0xE000 && addr < 0xFE00)
    addr -= 0x2000;

  if (addr < 0xA000)
    return 1;
  if (addr < 0xC000)
    return m->eram ? 1 : 0;
  if (addr < 0xE000)
    return 1;
  if (addr < 0xFE00)
    return 0; // unsupported for now
  if (addr < 0xFEA0)
    return 1;
  if (addr < 0xFF00)
    return 0; // nothing there
  return 1;
}

void* ptr(struct memory* m, uint16_t addr) {
  if (addr == m->write_breakpoint_addr)
    fprintf(stderr, "warning: memory breakpoint\n");

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
  if (!valid_ptr(m, addr)) {
    fprintf(stderr, "mmu: warning: read8\'ing bad address: %04X\n", addr);
    return 0;
  }
  if (addr >= 0xFF00 && addr < 0xFF80)
    return io_read8(m, addr & 0xFF);
  if (addr == 0xFFFF)
    return read_interrupt_enable((struct regs*)m->devices[DEVICE_CPU], addr);
  return m->read8(m, addr);
}

uint16_t read16(struct memory* m, uint16_t addr) {
  if (!valid_ptr(m, addr)) {
    fprintf(stderr, "mmu: warning: read16\'ing bad address: %04X\n", addr);
    return 0;
  }
  return m->read16(m, addr);
}

void write8(struct memory* m, uint16_t addr, uint8_t data) {
  if (!valid_ptr(m, addr)) {
    fprintf(stderr, "mmu: warning: write8\'ing bad address: %04X = %02X\n",
        addr, data);
    return;
  }
  if (addr >= 0xFF00 && addr < 0xFF80)
    io_write8(m, addr & 0xFF, data);
  else if (addr == 0xFFFF)
    write_interrupt_enable((struct regs*)m->devices[DEVICE_CPU], addr, data);
  else
    m->write8(m, addr, data);
}

void write16(struct memory* m, uint16_t addr, uint16_t data) {
  if (!valid_ptr(m, addr)) {
    fprintf(stderr, "mmu: warning: write16\'ing bad address: %04X = %04X\n",
        addr, data);
    return;
  }
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

void print_memory_debug(FILE* out, struct memory* m) {
  fprintf(out, ">>> rom 0\n");
  print_data(out, 0x0000, m->cart->data, 0x4000);

  fprintf(out, "\n>>> rom %d\n", m->cart_rom_bank_num);
  print_data(out, 0x4000, &m->cart->data[m->cart_rom_bank_num * 0x4000], 0x4000);

  fprintf(out, "\n>>> vram\n");
  print_data(out, 0x8000, m->vram, 0x2000);

  if (m->eram) {
    fprintf(out, "\n>>> eram %d\n", m->eram_bank_num);
    print_data(out, 0xA000, &m->eram[m->eram_bank_num * 0x2000], 0x2000);
  }

  fprintf(out, "\n>>> wram 0\n");
  print_data(out, 0xC000, m->wram, 0x1000);

  fprintf(out, "\n>>> wram %d\n", m->wram_bank_num);
  print_data(out, 0xD000, &m->wram[m->wram_bank_num * 0x1000], 0x1000);

  fprintf(out, "\n>>> sprite table\n");
  print_data(out, 0xFE00, m->sprite_table, 0xA0);

  fprintf(out, "\n>>> hram\n");
  print_data(out, 0xFF80, m->hram, 0x80);

  fprintf(out, "\n");
}
