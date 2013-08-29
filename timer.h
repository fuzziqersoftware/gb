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
  uint64_t last_incremented_at_cycles;
};

void timer_update(struct timer* t, int cycles);
inline uint8_t read_divider(struct timer* t, uint8_t addr);
inline void write_divider(struct timer* t, uint8_t addr, uint8_t value);
inline uint8_t read_timer(struct timer* t, uint8_t addr);
inline void write_timer(struct timer* t, uint8_t addr, uint8_t value);
inline uint8_t read_timer_mod(struct timer* t, uint8_t addr);
inline void write_timer_mod(struct timer* t, uint8_t addr, uint8_t value);
inline uint8_t read_timer_control(struct timer* t, uint8_t addr);
inline void write_timer_control(struct timer* t, uint8_t addr, uint8_t value);

#endif // TIMER_H
