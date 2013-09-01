#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "display.h"
#include "cpu.h"



void display_update(struct display* d, int cycles) {
  d->ly = (cycles / (CPU_CYCLES_PER_SEC / (60 * 154))) % 154;

  if (d->ly == d->lyc) {
    d->status |= 0x04;
    if (d->status & 0x40)
      signal_interrupt(d->cpu, INTERRUPT_LCDSTAT, 1);
  } else
    d->status &= ~0x04;

  if ((d->status & 0x10) && (d->ly == 144))
    signal_interrupt(d->cpu, INTERRUPT_VBLANK, 1);
}

uint8_t read_lcd_reg(struct display* d, uint8_t addr) {
  return (&d->control)[addr - 0x40];
}

void write_lcd_reg(struct display* d, uint8_t addr, uint8_t value) {

  switch (addr) {
    case 0x41:
      // can't write the lower 3 bits
      d->status = (d->status & 7) | (value & 0x78);
      if (d->status & 0x20)
        printf("warning: lcd oam interrupt enabled but not implemented\n");
      if (d->status & 0x08)
        printf("warning: lcd hblank interrupt enabled but not implemented\n");
      break;
    case 0x46:
      // TODO: DMA that shit
      break;
    default:
      (&d->control)[addr - 0x40] = value;
  }
}
