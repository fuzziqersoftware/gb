#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "display.h"
#include "cpu.h"
#include "mmu.h"



static uint64_t display_gettime_us() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000000) + tv.tv_usec;
}

void display_pause(struct display* d) {
  d->pause_time = display_gettime_us();
}

void display_resume(struct display* d) {
  d->last_vblank_time += (display_gettime_us() - d->pause_time);
}

void display_init(struct display* d, struct regs* cpu, struct memory* m,
    uint64_t render_freq) {

  memset(d, 0, sizeof(*d));
  d->cpu = cpu;
  d->mem = m;
  d->render_freq = render_freq;
  d->wait_vblank = 1;
  d->last_vblank_time = display_gettime_us();
}

static void decode_tile(uint16_t* tile, uint8_t out[8][8]) {
  int x, y;
  for (y = 0; y < 8; y++) {
    for (x = 0; x < 8; x++) {
      int low_bit = ((tile[y] >> 8) >> (7 - x)) & 1;
      int high_bit = (tile[y] >> (7 - x)) & 1;
      int overall = (high_bit << 1) | low_bit;
      out[y][x] = overall;
    }
  }
}

static void display_generate_image(const struct display* d,
    uint8_t overall_image[256][256]) {

  int unsigned_tile_ids = d->control & LCD_CONTROL_BG_WINDOW_TILE_SELECT;
  uint16_t* tile_data_map = (uint16_t*)(unsigned_tile_ids ?
      ptr(d->mem, 0x8000) : ptr(d->mem, 0x9000));
  uint8_t* tile_id_map = (uint8_t*)((d->control & LCD_CONTROL_BG_TILEMAP_SELECT) ?
      ptr(d->mem, 0x9C00) : ptr(d->mem, 0x9800));

  int tile_x, tile_y, x, y;
  for (y = 0; y < 32; y++) {
    for (x = 0; x < 32; x++) {

      int tile_id = tile_id_map[y * 32 + x];
      if (!unsigned_tile_ids && tile_id > 127)
        tile_id -= 256;
      uint16_t* this_tile_data = tile_data_map + (8 * tile_id);

      uint8_t tile_data[8][8];
      decode_tile(this_tile_data, tile_data);

      for (tile_y = 0; tile_y < 8; tile_y++) {
        for (tile_x = 0; tile_x < 8; tile_x++) {
          overall_image[y * 8 + tile_y][x * 8 + tile_x] = tile_data[tile_y][tile_x];
        }
      }
    }
  }
}

static void display_render(FILE* f, uint8_t overall_image[256][256],
    const char* terminal_palette) {

  char output_buffer[264];
  int x, y;
  for (y = 0; y < 256; y++) {
    int line_offset = 0;
    for (x = 0; x < 256; x++)
      output_buffer[line_offset++] = terminal_palette[overall_image[y][x]];
    output_buffer[line_offset++] = '\n';
    fputs(output_buffer, f);
  }
  fputc('\n', f);
}

static void display_render_window(FILE* f, const struct display* d,
    uint8_t overall_image[256][256], const char* terminal_palette) {

  char output_buffer[168];
  int x, y;
  for (y = 0; y < 144; y++) {
    int line_offset = 0;
    for (x = 0; x < 160; x++)
      output_buffer[line_offset++] = terminal_palette[overall_image[(y + d->scy) % 256][(x + d->scx) % 256]];
    output_buffer[line_offset++] = '\n';
    fputs(output_buffer, f);
  }
  fputc('\n', f);
}

void display_print(FILE* f, struct display* d) {
  fprintf(f, ">>> display\n");
  fprintf(f, "40_control  = %02X    41_status     = %02X\n", d->control, d->status);
  fprintf(f, "42_scy      = %02X    43_scx        = %02X\n", d->scy, d->scx);
  fprintf(f, "44_ly       = %02X    45_lyc        = %02X\n", d->ly, d->lyc);
  fprintf(f, "46_dma      = %02X    47_bg_palette = %02X\n", d->dma, d->bg_palette);
  fprintf(f, "48_palette0 = %02X    49_palette1   = %02X\n", d->palette0, d->palette1);
  fprintf(f, "4A_wy       = %02X    4B_wx         = %02X\n", d->wy, d->wx);
  fprintf(f, "wait_vblank = %d\n", d->wait_vblank);
  fprintf(f, "last_vblank_time = %016llX\n", d->last_vblank_time);
  fprintf(f, "pause_time       = %016llX\n", d->pause_time);

  const char* terminal_palette = "_!*@";
  uint8_t tile_data[8][8];
  int x, y, z;
  for (z = 0; z < 192; z++) {
    fprintf(f, "\n>>> tile dump %d\n", z);
    uint16_t* this_tile_data = (uint16_t*)d->mem->vram + (z * 8);
    decode_tile(this_tile_data, tile_data);
    for (y = 0; y < 8; y++) {
      for (x = 0; x < 8; x++)
        fputc(terminal_palette[tile_data[y][x]], f);
      fputc('\n', f);
    }
  }

  fprintf(f, "\n>>> full rendered image\n");

  uint8_t overall_image[256][256];
  display_generate_image(d, overall_image);
  display_render(f, overall_image, terminal_palette);

  fprintf(f, "\n>>> screen image (at %d %d)\n", d->scx, d->scy);
  display_render_window(f, d, overall_image, terminal_palette);
}

void display_update(struct display* d, uint64_t cycles) {

  int prev_ly = d->ly;

  uint64_t current_period_cycles = (d->control & 0x80) ? (cycles % 70224) : 70000;
  d->ly = current_period_cycles / 456;

  int mode;
  if (d->ly < 144) {
    int current_hblank_cycles = current_period_cycles % 456;
    if (current_hblank_cycles < 204)
      mode = 2; // reading oam
    else if (current_hblank_cycles < 284)
      mode = 3; // reading oam & vram
    else
      mode = 0; // hblank
  } else
    mode = 1; // vblank or display disabled

  if ((prev_ly > 0) && (d->ly == 0)) {
    if (d->render_freq && ((cycles / 70224) % d->render_freq) == 0) {
      const char* terminal_palette = "_!*@";
      uint8_t overall_image[256][256];
      display_generate_image(d, overall_image);
      display_render_window(stdout, d, overall_image, terminal_palette);
    }

    if (d->wait_vblank) {
      // 16750 us/frame = 59.7 frames/sec
      d->last_vblank_time += 16750;
      uint64_t now = display_gettime_us();
      if (now < d->last_vblank_time)
        usleep(d->last_vblank_time - now);
      else
        fprintf(stderr, "display: emulation is too slow! %llu missing usecs\n",
            now - d->last_vblank_time);
    }
  }

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
  uint8_t value = (&d->control)[addr - 0x40];
  return value;
}

void write_lcd_reg(struct display* d, uint8_t addr, uint8_t value) {

  switch (addr) {
    case 0x40:
      d->control = value;
      fprintf(stderr, "lcd control: %02X (%s)\n", value,
          (value & LCD_CONTROL_ENABLE) ? "enabled" : "disabled");
      break;

    case 0x41:
      // can't write the lower 3 bits
      d->status = (d->status & 7) | (value & 0x78);
      if (d->status & 0x20)
        fprintf(stderr, "warning: lcd oam interrupt enabled but not implemented\n");
      if (d->status & 0x08)
        fprintf(stderr, "warning: lcd hblank interrupt enabled but not implemented\n");
      break;

    case 0x42:
      d->scy = value;
      fprintf(stderr, "lcd yscroll: %02X\n", value);
      break;

    case 0x43:
      d->scx = value;
      fprintf(stderr, "lcd xscroll: %02X\n", value);
      break;

    case 0x4A:
      d->wy = value;
      fprintf(stderr, "lcd ywindow: %02X\n", value);
      break;

    case 0x4B:
      d->wx = value;
      fprintf(stderr, "lcd xwindow: %02X (actual %d)\n", value, value - 7);
      break;

    case 0x44:
      fprintf(stderr, "lcd: attempted to write ly %02X (skipped)\n", value);
      break;

    case 0x46:
      // TODO: DMA that shit
      break;

    default:
      fprintf(stderr, "lcd reg write: %02X %02X\n", addr, value);
      (&d->control)[addr - 0x40] = value;
  }
}
