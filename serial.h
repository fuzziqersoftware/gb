#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

#include "cpu.h"

struct serial {
  struct regs* cpu;
  uint8_t data;
  uint8_t control;
};

void serial_init(struct serial* s, struct regs* cpu);

void serial_update(struct serial* s, int cycles);
uint8_t read_serial_data(struct serial* s, uint8_t addr);
void write_serial_data(struct serial* s, uint8_t addr, uint8_t value);
uint8_t read_serial_control(struct serial* s, uint8_t addr);
void write_serial_control(struct serial* s, uint8_t addr, uint8_t value);

#endif // SERIAL_H
