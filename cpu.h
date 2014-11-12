#ifndef CPU_H
#define CPU_H

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "mmu.h"
#include "cart.h"



#define CPU_CYCLES_PER_SEC  4194304

#define INTERRUPT_VBLANK   0
#define INTERRUPT_LCDSTAT  1
#define INTERRUPT_TIMER    2
#define INTERRUPT_SERIAL   3
#define INTERRUPT_JOYPAD   4
#define INTERRUPT_MAX      4

#define FLAG_C 4
#define FLAG_H 5
#define FLAG_N 6
#define FLAG_Z 7

// x comes after y here since the host system is little-endian
#define DECLARE_COMPOSITE_REG(x, y) \
  union { \
    uint16_t x##y; \
    struct { \
      uint8_t y; \
      uint8_t x; \
    }; \
  }

struct regs {
  DECLARE_COMPOSITE_REG(a, f);
  DECLARE_COMPOSITE_REG(b, c);
  DECLARE_COMPOSITE_REG(d, e);
  DECLARE_COMPOSITE_REG(h, l);
  uint16_t sp;
  uint16_t pc;

  uint8_t ime;
  uint8_t interrupt_flag;
  uint8_t interrupt_enable;
  uint8_t debug_interrupt_requested;

  uint8_t speed_switch;

  uint64_t cycles;

  uint8_t wait_for_interrupt;
  uint8_t stop;

  uint8_t debug;
  uint16_t ddx;
};

int run_cycle(struct regs* r, const struct regs* prev, struct memory* m);
int is_double_speed_mode(struct regs* r);

void signal_interrupt(struct regs* r, int int_id, int signal);
void signal_debug_interrupt(struct regs* r);

uint8_t read_interrupt_flag(struct regs* r, uint8_t addr);
void write_interrupt_flag(struct regs* r, uint8_t addr, uint8_t value);
uint8_t read_interrupt_enable(struct regs* r, uint8_t addr);
void write_interrupt_enable(struct regs* r, uint8_t addr, uint8_t value);
uint8_t read_speed_switch(struct regs* r, uint8_t addr);
void write_speed_switch(struct regs* r, uint8_t addr, uint8_t value);

struct regs* create_cpu();
void delete_cpu();
void print_regs(const struct regs* r, const struct regs* prev, struct memory* m);
void print_regs_debug(FILE* f, const struct regs* r);
void disassemble(FILE* output_stream, void* data, uint32_t size, uint32_t offset, uint32_t dasm_size);
void disassemble_memory(FILE* output_stream, struct memory* m, uint16_t addr, uint16_t size);

union cart_data* debug_cart();

#endif // CPU_H
