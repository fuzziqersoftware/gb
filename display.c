#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef MACOSX
#include "OpenGL/gl.h"
#include "OpenGL/glu.h"
#include "GLUT/glut.h"
#else
#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glut.h"
#endif

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

  int x;
  for (x = 0; x < 0x20; x++)
    d->bg_colors[x] = 0x7FFF;
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

struct sprite_info {
  uint8_t y;
  uint8_t x;
  uint8_t tile_id;
  uint8_t flags;
};

#define SPRITE_FLAG_BEHIND_BG      0x80
#define SPRITE_FLAG_YFLIP          0x40
#define SPRITE_FLAG_XFLIP          0x20
#define SPRITE_FLAG_USE_PALETTE1   0x10 // Non-CGB only
#define SPRITE_FLAG_VRAM_BANK1     0x08 // CGB only

static void display_update_line(struct display* d, int y) {

  // if disabled, don't draw anything
  if (!(d->control & 0x80))
    return;

  static float colors[][3] = {
    {1.0f, 1.0f, 1.0f},
    {0.3f, 0.3f, 0.3f},
    {0.7f, 0.7f, 0.7f},
    {0.0f, 0.0f, 0.0f},
  };

  int unsigned_tile_ids = d->control & LCD_CONTROL_BG_WINDOW_TILE_SELECT;
  uint16_t* tile_data_map = (uint16_t*)(unsigned_tile_ids ?
      ptr(d->mem, 0x8000) : ptr(d->mem, 0x9000));
  uint8_t* tile_id_map = (uint8_t*)((d->control & LCD_CONTROL_BG_TILEMAP_SELECT) ?
      ptr(d->mem, 0x9C00) : ptr(d->mem, 0x9800));

  // draw background
  int x, z;
  int tile_y = ((y + d->scy) & 0xFF) / 8;
  int tile_pixel_y = (y + d->scy) % 8;
  int decoded_tile_id = -1;
  uint8_t tile_data[8][8];

  for (x = 0; x < 160; x++) {
    int tile_x = ((x + d->scx) & 0xFF) / 8;
    int tile_pixel_x = (x + d->scx) % 8;
    int tile_id = tile_id_map[tile_y * 32 + tile_x];
    if (!unsigned_tile_ids && tile_id > 127)
      tile_id -= 256;

    if (decoded_tile_id != tile_id) {
      uint16_t* this_tile_data = tile_data_map + (8 * tile_id);
      decode_tile(this_tile_data, tile_data);
      decoded_tile_id = tile_id;
    }

    int color_id = tile_data[tile_pixel_y][tile_pixel_x];
    d->image_color_ids[y][x] = color_id;
    d->image[y][x][0] = colors[color_id][0];
    d->image[y][x][1] = colors[color_id][1];
    d->image[y][x][2] = colors[color_id][2];
  }

  // draw window
  if (d->control & 0x20) {
    // TODO
  }

  // draw sprites
  if (d->control & 0x02) {
    int sprite_ysize = (d->control & 0x04) ? 16 : 8;
    uint16_t* tile_data_map = (uint16_t*)ptr(d->mem, 0x8000);

    const struct sprite_info* sprites = (struct sprite_info*)ptr(d->mem, 0xFE00);
    for (z = 0; z < 40; z++) {
      const struct sprite_info* sprite = &sprites[z];

      int sprite_x = sprite->x - 8;
      int sprite_y = sprite->y - 16;
      int line_id = y - sprite_y;

      // skip if not visible
      if (line_id < 0 ||
          line_id >= sprite_ysize ||
          sprite_x < -7 ||
          sprite_x >= 160)
        continue;

      uint8_t tile_data[16][8];
      if (sprite_ysize == 16) {
        uint16_t* this_tile_data = tile_data_map + (8 * (sprite->tile_id & 0xFE));
        decode_tile(this_tile_data, &tile_data[0]);
        this_tile_data = tile_data_map + (8 * (sprite->tile_id | 0x01));
        decode_tile(this_tile_data, &tile_data[8]);
      } else {
        uint16_t* this_tile_data = tile_data_map + (8 * sprite->tile_id);
        decode_tile(this_tile_data, &tile_data[0]);
      }

      if (sprite->flags & SPRITE_FLAG_USE_PALETTE1) {
        fprintf(stderr, "lcd: warning: rendering sprite (%02X %02X %02X %02X) using palette1 not implemented\n",
            sprite->y, sprite->x, sprite->tile_id, sprite->flags);
      }
      if (sprite->flags & SPRITE_FLAG_VRAM_BANK1) {
        fprintf(stderr, "lcd: warning: rendering sprite (%02X %02X %02X %02X) from vram1 not implemented\n",
            sprite->y, sprite->x, sprite->tile_id, sprite->flags);
      }

      if (sprite->flags & SPRITE_FLAG_YFLIP)
        line_id = sprite_ysize - 1 - line_id;
      if (sprite->flags & SPRITE_FLAG_XFLIP) {
        for (x = 0; x < 4; x++) {
          uint8_t t = tile_data[line_id][x];
          tile_data[line_id][x] = tile_data[line_id][7 - x];
          tile_data[line_id][7 - x] = t;
        }
      }

      for (x = 0; x < 8; x++) {
        int target_x = sprite_x + x;
        if (target_x < 0 || target_x >= 160)
          continue;

        if ((sprite->flags & SPRITE_FLAG_BEHIND_BG) && d->image_color_ids[y][target_x])
          continue;
        if (!tile_data[line_id][x])
          continue;
        int color_id = tile_data[line_id][x];
        d->image_color_ids[y][target_x] = color_id;
        d->image[y][target_x][0] = colors[color_id][0];
        d->image[y][target_x][1] = colors[color_id][1];
        d->image[y][target_x][2] = colors[color_id][2];
      }
    }
  }
}

void display_render_window_opengl(const struct display* d) {

  static const int x_pixels = 160, y_pixels = 144;
  static const float xfstep = (2.0f / x_pixels), yfstep = -(2.0f / y_pixels);

  int x, y;
  float xf, yf;
  glBegin(GL_QUADS);
  for (y = 0, yf = 1; y < y_pixels; y++, yf += yfstep) {
    for (x = 0, xf = -1; x < x_pixels; x++, xf += xfstep) {
      glColor3f(d->image[y][x][0], d->image[y][x][1], d->image[y][x][2]);
      glVertex3f(xf, yf, 1.0f);
      glVertex3f(xf + xfstep, yf, 1.0f);
      glVertex3f(xf + xfstep, yf + yfstep, 1.0f);
      glVertex3f(xf, yf + yfstep, 1.0f);
    }
  }
  glEnd();

  glutSwapBuffers();
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
}

void display_update(struct display* d, uint64_t cycles) {

  int prev_ly = d->ly;

  uint64_t current_period_cycles = (d->control & 0x80) ? (cycles % LCD_CYCLES_PER_FRAME) : 70000;
  d->ly = current_period_cycles / 456;

  int mode, prev_mode = d->status & 0x03;
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
  d->status = (d->status & ~3) | mode;

  if ((prev_ly > 0) && (d->ly == 0)) {
    int frame_num = cycles / LCD_CYCLES_PER_FRAME;
    if (d->render_freq && (frame_num % d->render_freq) == 0)
      display_render_window_opengl(d);

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

  // check for interrupts
  if (prev_mode != mode) {
    if (mode == 0) // on hblank, draw a line
      display_update_line(d, d->ly);
    if ((mode == 0) && (d->status & 0x08)) // h-blank interrupt
      signal_interrupt(d->cpu, INTERRUPT_LCDSTAT, 1);
    if ((mode == 1) && (d->status & 0x10)) // v-blank interrupt
      signal_interrupt(d->cpu, INTERRUPT_LCDSTAT, 1);
    if ((mode == 2) && (d->status & 0x20)) // oam interrupt
      signal_interrupt(d->cpu, INTERRUPT_LCDSTAT, 1);
  }

  if (prev_ly != d->ly) {
    if (d->ly == d->lyc) {
      d->status |= 0x04;
      if (d->status & 0x40)
        signal_interrupt(d->cpu, INTERRUPT_LCDSTAT, 1);
    } else
      d->status &= ~0x04;

    // TODO: does the vblank interrupt always happen, or is it controlled by
    // d->status & 0x10? if the latter, does d->status & 0x10 cause LCDSTAT or
    // VBLANK?
    if (d->ly == 144)
      signal_interrupt(d->cpu, INTERRUPT_VBLANK, 1);
  }
}

uint8_t read_lcd_reg(struct display* d, uint8_t addr) {
  switch (addr) {
    case 0x68:
      return d->bg_color_palette_index;

    case 0x69:
      return d->bg_color_palette[d->bg_color_palette_index & 0x3F];

    case 0x6A:
      return d->sprite_color_palette_index;

    case 0x6B:
      return d->sprite_color_palette[d->sprite_color_palette_index & 0x3F];

    default:
      if (addr < 0x40 || addr > 0x4B) {
        signal_debug_interrupt(d->cpu, "read from unimplemented display register");
        return 0;
      }
      return (&d->control)[addr - 0x40];
  }
}

void write_lcd_reg(struct display* d, uint8_t addr, uint8_t value) {

  switch (addr) {
    case 0x40:
      d->control = value;
      fprintf(stderr, "lcd control: %02X\n", value);
      break;

    case 0x41:
      // can't write the lower 3 bits
      d->status = (d->status & 7) | (value & 0x78);
      fprintf(stderr, "lcd status: %02X\n", value);
      break;

    case 0x42:
      d->scy = value;
      break;

    case 0x43:
      d->scx = value;
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

    case 0x45:
      d->lyc = value;
      fprintf(stderr, "lcd lyc: %02X\n", d->lyc);
      break;

    case 0x46: {
      //fprintf(stderr, "lcd: sprite table dma from %02X00\n", value);
      int x, addr = (value << 8);
      for (x = 0; x < 0xA0; x++)
        write8(d->mem, 0xFE00 + x, read8(d->mem, addr + x));
      break;
    }

    case 0x47: {
      d->bg_palette = value;

      int x;
      fprintf(stderr, "lcd: bg palette set to [");
      for (x = 0; x < 4; x++)
        fprintf(stderr, "%d", (d->bg_palette >> (x << 1)) & 3);
      fprintf(stderr, "]\n");
      break;
    }

    case 0x48: {
      d->palette0 = value;

      int x;
      fprintf(stderr, "lcd: object palette 0 set to [");
      for (x = 0; x < 4; x++)
        fprintf(stderr, "%d", (d->palette0 >> (x << 1)) & 3);
      fprintf(stderr, "]\n");
      break;
    }

    case 0x49: {
      d->palette1 = value;

      int x;
      fprintf(stderr, "lcd: object palette 1 set to [");
      for (x = 0; x < 4; x++)
        fprintf(stderr, "%d", (d->palette1 >> (x << 1)) & 3);
      fprintf(stderr, "]\n");
      break;
    }

    case 0x68:
      d->bg_color_palette_index = value & 0xBF;
      break;
    case 0x69: {
      d->bg_color_palette[d->bg_color_palette_index & 0x3F] = value;
      int color_index = (d->bg_color_palette_index >> 1) & 0x1F;
      fprintf(stderr, "lcd: bg palette %d set to %04hX\n", color_index,
          d->bg_colors[color_index]);
      if (d->bg_color_palette_index & 0x80)
        d->bg_color_palette_index = (d->bg_color_palette_index + 1) & 0xBF;
      break;
    }

    case 0x6A:
      d->sprite_color_palette_index = value & 0xBF;
      break;
    case 0x6B: {
      d->sprite_color_palette[d->sprite_color_palette_index & 0x3F] = value;
      int color_index = (d->sprite_color_palette_index >> 1) & 0x1F;
      fprintf(stderr, "lcd: sprite palette %d set to %04hX\n", color_index,
          d->sprite_colors[color_index]);
      if (d->sprite_color_palette_index & 0x80)
        d->sprite_color_palette_index = (d->sprite_color_palette_index + 1) & 0xBF;
      break;
    }

    default:
      fprintf(stderr, "lcd reg write: %02X %02X\n", addr, value);
      (&d->control)[addr - 0x40] = value;
  }
}
