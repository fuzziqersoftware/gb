#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "timer.h"
#include "cpu.h"


static int timer_freq[4] = {
	4096,
	262144,
	65536,
	16384,
};

void timer_update(struct timer* t, int cycles) {

  uint64_t cycles_per_increment = CPU_CYCLES_PER_SEC / (is_double_speed_mode(t->cpu) ? 32768 : 16384);
  if (cycles - t->divider_last_incremented_at_cycles > cycles_per_increment) {
    t->divider_last_incremented_at_cycles += cycles_per_increment;
    t->divider++;
  }

  if (t->control & 0x04) {
    cycles_per_increment = CPU_CYCLES_PER_SEC / (is_double_speed_mode(t->cpu) ? (2 * timer_freq[t->control & 3]) : timer_freq[t->control & 3]);
    if (cycles - t->timer_last_incremented_at_cycles > cycles_per_increment) {
      t->timer_last_incremented_at_cycles += cycles_per_increment;
      if (t->timer == 0xFF) {
        signal_interrupt(t->cpu, INTERRUPT_TIMER, 1);
        t->timer = t->timer_mod;
      } else
        t->timer++;
    }
  }
}

inline uint8_t read_divider(struct timer* t, uint8_t addr) {
  return t->divider;
}

inline void write_divider(struct timer* t, uint8_t addr, uint8_t value) {
  t->divider = 0;
}

inline uint8_t read_timer(struct timer* t, uint8_t addr) {
  return t->timer;
}

inline void write_timer(struct timer* t, uint8_t addr, uint8_t value) {
  t->timer = value;
}

inline uint8_t read_timer_mod(struct timer* t, uint8_t addr) {
  return t->timer_mod;
}

inline void write_timer_mod(struct timer* t, uint8_t addr, uint8_t value) {
  t->timer_mod = value;
}

inline uint8_t read_timer_control(struct timer* t, uint8_t addr) {
  return t->control;
}

inline void write_timer_control(struct timer* t, uint8_t addr, uint8_t value) {
  t->control = value;
  printf("timer control reset: %dHz, %s\n", timer_freq[t->control & 3], (t->control & 4) ? "enabled" : "disabled");
}
