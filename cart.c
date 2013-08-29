#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "cart.h"

#define op(name, id, class_id, prefix) {id, CART_CLASS_##class_id, #name},
static const struct cart_type_info cart_types[] = {
  CART_TYPES(op)
};
#undef op

const struct cart_type_info* type_info_for_cart_type(uint8_t id) {
  int x;
  for (x = 0; x < sizeof(cart_types) / sizeof(cart_types[0]); x++)
    if (cart_types[x].id == id)
      return &cart_types[x];
  return NULL;
}

uint8_t header_checksum(const struct cart_header* h) {
  const uint8_t *addr;
  uint8_t x = 0;
  for (addr = (uint8_t*)&h->title[0]; addr < &h->header_checksum; addr++)
    x = x - *addr - 1;
  return x;
}

uint16_t global_checksum(const union cart_data* c) {
  /* TODO: this is wrong; fix it
  int num_words = rom_size_for_rom_size_code(c->header.rom_size);
  uint16_t sum = 0;
  uint16_t* addr = (uint16_t*)c;
  while (num_words--) {
    if (addr != &c->header.global_checksum)
      sum += byteswap(*addr);
    addr++;
  }
  return byteswap(sum); */
  return 0;
}

int verify_logo(uint8_t* logo) {
  static const uint8_t correct_logo[] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
    0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
    0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
  };

  return !memcmp(logo, correct_logo, sizeof(correct_logo));
}

inline int rom_size_for_rom_size_code(uint8_t code) {
  return 0x8000 << code;
}

inline int ram_size_for_ram_size_code(uint8_t code) {
  // TODO: figure out why super mario land writes to eram when it presumably doesn't have any eram
  return code ? (0x200 << (code << 1)) : 0;
}

union cart_data* load_cart_from_file(const char* filename) {
  FILE* f = fopen(filename, "rb");
  if (!f)
    return NULL;

  fseek(f, 0, SEEK_END);
  int fsize = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (fsize & 0x7FFF) {
    fclose(f);
    return NULL;
  }

  union cart_data* c = (union cart_data*)malloc(fsize);
  if (c)
    fread(c, 1, fsize, f);

  fclose(f);
  return c;
}

void delete_cart(union cart_data* c) {
  free(c);
}

int verify_cart(union cart_data* c) {
  return (c->header.header_checksum == header_checksum(&c->header)) &&
      verify_logo(c->header.logo);
}

void print_cart_info(union cart_data* c) {
  printf("entry point code  : %02X %02X %02X %02X\n", c->header.entry_code[0], c->header.entry_code[1], c->header.entry_code[2], c->header.entry_code[3]);
  printf("logo              : %s\n", verify_logo(c->header.logo) ? "ok" : "incorrect");
  printf("title             : %.11s\n", c->header.title);
  printf("manufacturer code : %02X %02X %02X %02X\n", c->header.manufacturer_code[0], c->header.manufacturer_code[1], c->header.manufacturer_code[2], c->header.manufacturer_code[3]);
  printf("cgb flag          : %02X\n", c->header.cgb_flag);
  printf("new maker code    : %02X %02X\n", c->header.new_maker_code[0], c->header.new_maker_code[1]);
  printf("sgb flag          : %02X\n", c->header.sgb_flag);

  const struct cart_type_info* type_info = type_info_for_cart_type(c->header.cart_type);
  printf("cart type         : %02X (%s)\n", c->header.cart_type, type_info ? type_info->name : "unknown");

  printf("rom size          : %02X (%d bytes)\n", c->header.rom_size, rom_size_for_rom_size_code(c->header.rom_size));
  printf("ram size          : %02X (%d bytes)\n", c->header.ram_size, ram_size_for_ram_size_code(c->header.ram_size));

  printf("dest code         : %02X\n", c->header.dest_code);
  printf("maker code        : %02X\n", c->header.maker_code);
  printf("mask rom version  : %02X\n", c->header.mask_rom_version);
  printf("header checksum   : %02X (%s)\n", c->header.header_checksum, header_checksum(&c->header) == c->header.header_checksum ? "ok" : "incorrect");
  printf("global checksum   : %04X (%s)\n", c->header.global_checksum, "unverified");
}
