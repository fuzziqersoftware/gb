#ifndef CART_H
#define CART_H

#include <stdlib.h>
#include <stdint.h>

#include "mmu.h"

#define CART_TYPES(op) \
  op(CART_TYPE_SIMPLE,                  0x00, SIMPLE,        default_mbc) \
  op(CART_TYPE_MBC1,                    0x01, MBC1,          mbc1) \
  op(CART_TYPE_MBC1_RAM,                0x02, MBC1,          mbc1) \
  op(CART_TYPE_MBC1_RAM_BATTERY,        0x03, MBC1,          mbc1) \
  op(CART_TYPE_MBC2,                    0x05, MBC2,          mbc2) \
  op(CART_TYPE_MBC2_BATTERY,            0x06, MBC2,          mbc2) \
  op(CART_TYPE_SIMPLE_RAM,              0x08, SIMPLE,        default_mbc) \
  op(CART_TYPE_SIMPLE_RAM_BATTERY,      0x09, SIMPLE,        default_mbc) \
  op(CART_TYPE_MMM01,                   0x0B, MMM01,         mmm01) \
  op(CART_TYPE_MMM01_RAM,               0x0C, MMM01,         mmm01) \
  op(CART_TYPE_MMM01_RAM_BATTERY,       0x0D, MMM01,         mmm01) \
  op(CART_TYPE_MBC3_TIMER_BATTERY,      0x0F, MBC3,          mbc3) \
  op(CART_TYPE_MBC3_TIMER_RAM_BATTERY,  0x10, MBC3,          mbc3) \
  op(CART_TYPE_MBC3,                    0x11, MBC3,          mbc3) \
  op(CART_TYPE_MBC3_RAM,                0x12, MBC3,          mbc3) \
  op(CART_TYPE_MBC3_RAM_BATTERY,        0x13, MBC3,          mbc3) \
  op(CART_TYPE_MBC4,                    0x15, MBC4,          mbc4) \
  op(CART_TYPE_MBC4_RAM,                0x16, MBC4,          mbc4) \
  op(CART_TYPE_MBC4_RAM_BATTERY,        0x17, MBC4,          mbc4) \
  op(CART_TYPE_MBC5,                    0x19, MBC5,          mbc5) \
  op(CART_TYPE_MBC5_RAM,                0x1A, MBC5,          mbc5) \
  op(CART_TYPE_MBC5_RAM_BATTERY,        0x1B, MBC5,          mbc5) \
  op(CART_TYPE_MBC5_RUMBLE,             0x1C, MBC5,          mbc5) \
  op(CART_TYPE_MBC5_RUMBLE_RAM,         0x1D, MBC5,          mbc5) \
  op(CART_TYPE_MBC5_RUMBLE_RAM_BATTERY, 0x1E, MBC5,          mbc5) \
  op(CART_TYPE_POCKET_CAMERA,           0xFC, POCKET_CAMERA, pocket_camera) \
  op(CART_TYPE_BANDAI_TAMA5,            0xFD, BANDAI_TAMA5,  bandai_tama5) \
  op(CART_TYPE_HuC3,                    0xFE, HUC3,          huc3) \
  op(CART_TYPE_HuC1_RAM_BATTERY,        0xFF, HUC1,          huc1)

#define CART_CLASS_SIMPLE        0
#define CART_CLASS_MBC1          1
#define CART_CLASS_MBC2          2
#define CART_CLASS_MMM01         3
#define CART_CLASS_MBC3          4
#define CART_CLASS_MBC4          5
#define CART_CLASS_MBC5          6
#define CART_CLASS_POCKET_CAMERA 7
#define CART_CLASS_BANDAI_TAMA5  8
#define CART_CLASS_HUC3          9
#define CART_CLASS_HUC1          10

struct cart_type_info {
  uint8_t id;
  uint8_t class_id;
  const char* name;
};

struct cart_header {
  uint8_t entry_code[4];
  uint8_t logo[0x30];
  char title[11];
  uint8_t manufacturer_code[4];
  uint8_t cgb_flag; // 80 = cgb but backwards compatible; C0 = cgb only
  uint8_t new_maker_code[2];
  uint8_t sgb_flag; // 03 = supports sgb functions
  uint8_t cart_type; // see CART_TYPE_*
  uint8_t rom_size; // actual size == 32K << rom_size
  uint8_t ram_size; // 00 = none, 01 = 2K, 02 = 8K, 03 = 32K (4 banks)
  uint8_t dest_code; // 00 = japanese, 01 = other
  uint8_t maker_code; // 33 = sgb functions (use new maker code instead)
  uint8_t mask_rom_version;
  uint8_t header_checksum; // see header_checksum()
  uint16_t global_checksum; // see global_checksum()
};

union cart_data {
  uint8_t data[0];
  struct {
    uint8_t rst_vectors[8][8];
    uint8_t interrupt_vectors[5][8];
    uint8_t unused[19][8];
    struct cart_header header;
  };
};


const struct cart_type_info* type_info_for_cart_type(uint8_t id);

uint8_t header_checksum(const struct cart_header* h);
uint16_t global_checksum(const union cart_data* c);
int verify_logo(uint8_t* logo);

int rom_size_for_rom_size_code(uint8_t code);
int ram_size_for_ram_size_code(uint8_t code);

union cart_data* load_cart_from_file(const char* filename);
void delete_cart(union cart_data* c);
int verify_cart(union cart_data* c);
void print_cart_info(union cart_data* c);

#endif // CART_H
