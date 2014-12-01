#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#include "mmu.h"

#define LCD_CONTROL_ENABLE                  0x80  // 1 = on
#define LCD_CONTROL_TILEMAP_SELECT          0x40  // 0=9800-9BFF, 1=9C00-9FFF
#define LCD_CONTROL_WINDOW_ENABLE           0x20  // 1 = on
#define LCD_CONTROL_BG_WINDOW_TILE_SELECT   0x10  // 0=8800-97FF, 1=8000-8FFF
#define LCD_CONTROL_BG_TILEMAP_SELECT       0x08  // 0=9800-9BFF, 1=9C00-9FFF
#define LCD_CONTROL_LARGE_SPRITES           0x04  // 0=8x8, 1=8x16
#define LCD_CONTROL_SPRITES_ENABLE          0x02  // 1 = on
#define LCD_CONTROL_BG_DISPLAY              0x01  // 1 = on

#define LCD_CYCLES_PER_FRAME   70224

struct display {
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
  uint8_t bg_color_palette_index; // FF68
  uint8_t sprite_color_palette_index; // FF6A

  union {
    uint8_t bg_color_palette[0x40];
    uint16_t bg_colors[0x20];
  };
  union {
    uint8_t sprite_color_palette[0x40];
    uint16_t sprite_colors[0x20];
  };

  struct regs* cpu; // for interrupts
  struct memory* mem; // for tile data & rendering

  int wait_vblank;
  uint64_t last_vblank_time;
  uint64_t pause_time;
  uint64_t render_freq;

  uint16_t image_color_ids[144][160];
  float image[144][160][3];
};

void display_init(struct display* d, struct regs* cpu, struct memory* m,
    uint64_t render_freq);
void display_print(FILE* f, struct display* d);

void display_pause(struct display* d);
void display_resume(struct display* d);

void display_update(struct display* d, uint64_t cycles);
uint8_t read_lcd_reg(struct display* d, uint8_t addr);
void write_lcd_reg(struct display* d, uint8_t addr, uint8_t value);

#endif // DISPLAY_H
