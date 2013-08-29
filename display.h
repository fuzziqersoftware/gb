#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

struct display {
  struct regs* cpu; // for interrupts

  uint8_t control;    // FF40
  uint8_t status;     // FF41
  uint8_t scy;        // FF42
  uint8_t scx;        // FF43
  uint8_t ly;         // FF44
  uint8_t lyc;        // FF45
  uint8_t dma;        // FF46
  uint8_t bg_palette; // FF47
  uint8_t palette0;   // FF48
  uint8_t palette1;   // FF49
  uint8_t wy;         // FF4A
  uint8_t wx;         // FF4B
};

void display_update(struct display* d, int cycles);
uint8_t read_lcd_reg(struct display* d, uint8_t addr);
void write_lcd_reg(struct display* d, uint8_t addr, uint8_t value);

#endif // DISPLAY_H
