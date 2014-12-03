#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "cpu.h"

void audio_init(struct audio* a, struct regs* cpu) {
  memset(a, 0, sizeof(*a));
  a->cpu = cpu;
}

void audio_update(struct audio* a, int cycles) {
  // TODO
}

uint8_t read_audio_register(struct audio* a, uint8_t addr) {
  if (addr < 0x10 || addr > 0x3F)
    signal_debug_interrupt(a->cpu, "invalid audio reg read");
  return ((uint8_t*)a)[addr - 0x10];
}

void write_audio_register(struct audio* a, uint8_t addr, uint8_t value) {
  if (addr < 0x10 || addr > 0x3F)
    signal_debug_interrupt(a->cpu, "invalid audio reg write");
  ((uint8_t*)a)[addr - 0x10] = value;
}
