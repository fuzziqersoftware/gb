#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "serial.h"

void serial_update(struct serial* s, int cycles) { } // TODO

inline uint8_t read_serial_data(struct serial* s, uint8_t addr) {
  return s->data;
}

inline void write_serial_data(struct serial* s, uint8_t addr, uint8_t value) {
  s->data = value;
}

inline uint8_t read_serial_control(struct serial* s, uint8_t addr) {
  return s->control;
}

inline void write_serial_control(struct serial* s, uint8_t addr, uint8_t value) {
  s->control = value;
  if (s->control & 0x80) {
    if (s->data >= 0x20 && s->data <= 0x7F)
      printf("serial out: %02X \'%c\'\n", s->data, s->data);
    else
      printf("serial out: %02X\n", s->data);
    s->data = 0x00;
    signal_interrupt(s->cpu, INTERRUPT_SERIAL, 1);
  }
}
