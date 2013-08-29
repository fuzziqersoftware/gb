#ifndef MMU_H
#define MMU_H

#include <stdint.h>

#define DEVICE_DISPLAY   0
#define DEVICE_SERIAL    1
#define DEVICE_TIMER     2
#define DEVICE_CPU       3
#define DEVICE_AUDIO     4
#define DEVICE_INPUT     5
#define NUM_DEVICE_TYPES 6

struct memory {
  union cart_data* cart;

  uint8_t cart_rom_bank_num;
  uint8_t vram_bank_num;
  uint8_t eram_bank_num;
  uint8_t wram_bank_num;

  uint8_t* vram; // video ram (2 banks of 0x2000 each)
  uint8_t* eram; // external ram (# banks determined by cart)
  uint8_t* wram; // work ram (8 banks of 0x1000 each)
  uint8_t* sprite_table; // 0xA0 bytes
  uint8_t* hram; // high ram (0x80 bytes; byte 0x7F is the interrupt flag register)

  void* devices[NUM_DEVICE_TYPES];

  uint32_t write_breakpoint_addr;

  void* mbc_data;
  uint8_t (*read8)(struct memory* m, uint16_t addr);
  uint16_t (*read16)(struct memory* m, uint16_t addr);
  void (*write8)(struct memory* m, uint16_t addr, uint8_t data);
  void (*write16)(struct memory* m, uint16_t addr, uint16_t data);
};

void* ptr(struct memory* m, uint16_t addr);
uint8_t read8(struct memory* m, uint16_t addr);
uint16_t read16(struct memory* m, uint16_t addr);
void write8(struct memory* m, uint16_t addr, uint8_t data);
void write16(struct memory* m, uint16_t addr, uint16_t data);

struct memory* create_memory(union cart_data* cart);
void delete_memory(struct memory* m);

void add_device(struct memory* m, int device_type, void* device);
void update_devices(struct memory* m, uint64_t cycles);

#endif // MMU_H
