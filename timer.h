#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

#include "cpu.h"

struct timer {
  uint8_t divider;
  uint8_t timer;
  uint8_t timer_mod;
  uint8_t control;

  struct regs* cpu;
  uint64_t divider_last_incremented_at_cycles;
  uint64_t timer_last_incremented_at_cycles;
};

void timer_init(struct timer* t, struct regs* cpu);

void timer_update(struct timer* t, uint64_t cycles);
uint8_t read_divider(struct timer* t, uint8_t addr);
void write_divider(struct timer* t, uint8_t addr, uint8_t value);
uint8_t read_timer(struct timer* t, uint8_t addr);
void write_timer(struct timer* t, uint8_t addr, uint8_t value);
uint8_t read_timer_mod(struct timer* t, uint8_t addr);
void write_timer_mod(struct timer* t, uint8_t addr, uint8_t value);
uint8_t read_timer_control(struct timer* t, uint8_t addr);
void write_timer_control(struct timer* t, uint8_t addr, uint8_t value);

#endif // TIMER_H
