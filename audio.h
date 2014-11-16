#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

#include "cpu.h"

struct audio {
  uint8_t ch1_sweep;             // FF10
  uint8_t ch1_pattern_length;    // FF11
  uint8_t ch1_volume;            // FF12
  uint8_t ch1_freq_low;          // FF13
  uint8_t ch1_freq_high_control; // FF14
  uint8_t unused1;               // FF15; not readable, not writable
  uint8_t ch2_pattern_length;    // FF16
  uint8_t ch2_volume;            // FF17
  uint8_t ch2_freq_low;          // FF18
  uint8_t ch2_freq_high_control; // FF19
  uint8_t ch3_enable;            // FF1A
  uint8_t ch3_length;            // FF1B
  uint8_t ch3_volume;            // FF1C
  uint8_t ch3_freq_low;          // FF1D
  uint8_t ch3_freq_high_control; // FF1E
  uint8_t unused2;               // FF1F; not readable, not writable
  uint8_t ch4_length;            // FF20
  uint8_t ch4_volume;            // FF21
  uint8_t ch4_poly_counter;      // FF22
  uint8_t ch4_counter_consec;    // FF23
  uint8_t channel_control;       // FF24
  uint8_t channel_terminal;      // FF25
  uint8_t control;               // FF26
  uint8_t unused3[0x09];         // FF27-FF2F; not readable, not writable
  uint8_t ch3_wave_data[0x10];   // FF30-FF3F

  struct regs* cpu;
};

void audio_init(struct audio* a, struct regs* cpu);

void audio_update(struct audio* a, int cycles);
uint8_t read_audio_register(struct audio* a, uint8_t addr);
void write_audio_register(struct audio* a, uint8_t addr, uint8_t value);

#endif // AUDIO_H
