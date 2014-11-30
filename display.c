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
#include "terminal.h"



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
    uint64_t render_freq, int render_terminal, int render_opengl,
    int use_terminal_graphics) {

  memset(d, 0, sizeof(*d));
  d->cpu = cpu;
  d->mem = m;
  d->render_freq = render_freq;
  d->render_terminal = render_terminal;
  d->render_opengl = render_opengl;
  d->use_terminal_graphics = use_terminal_graphics;
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

void display_render_window_ascii(FILE* f, const struct display* d,
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

void display_render_window_color(FILE* f, const struct display* d,
    uint8_t overall_image[256][256]) {

  char output_buffer[4096];
  int x, y;
  for (y = 0; y < 72; y++) {

    uint8_t top_color, current_top_color = 255;
    uint8_t bottom_color, current_bottom_color = 255;
    int line_offset = 0;
    for (x = 0; x < 160; x++) {
      int pixel_x = (x + d->scx) & 0xFF;
      int pixel_y = ((y << 1) + d->scy);
      bottom_color = overall_image[pixel_y & 0xFF][pixel_x];
      top_color = overall_image[(pixel_y + 1) & 0xFF][pixel_x];

      if (top_color != current_top_color || bottom_color != current_bottom_color) {
        current_top_color = top_color;
        current_bottom_color = bottom_color;
        line_offset += write_change_color(&output_buffer[line_offset],
            FORMAT_FG_BLACK + current_top_color,
            FORMAT_BG_BLACK + current_bottom_color,
            FORMAT_END);
      }
      output_buffer[line_offset++] = 0xE2;
      output_buffer[line_offset++] = 0x96;
      output_buffer[line_offset++] = 0x84;
    }
    line_offset += write_change_color(&output_buffer[line_offset],
        FORMAT_NORMAL, FORMAT_END);
    output_buffer[line_offset++] = '\n';
    output_buffer[line_offset++] = 0;
    fputs(output_buffer, f);
  }
  change_color(FORMAT_NORMAL, FORMAT_END);
  fputc('\n', f);
  fflush(f);
}

void display_render_window_opengl(const struct display* d,
    uint8_t overall_image[256][256]) {

  static float colors[][3] = {
    {1.0f, 1.0f, 1.0f},
    {0.3f, 0.3f, 0.3f},
    {0.7f, 0.7f, 0.7f},
    {0.0f, 0.0f, 0.0f},
  };

  int x, y;
  float xf, yf;
  float xfstep = (2.0f / 160.0f), yfstep = -(2.0f / 144.0f);
  glBegin(GL_QUADS);
  for (y = 0, yf = 1; y < 144; y++, yf += yfstep) {
    for (x = 0, xf = -1; x < 160; x++, xf += xfstep) {
      int pixel_x = (x + d->scx) & 0xFF;
      int pixel_y = (y + d->scy) & 0xFF;

      int color_id = (d->bg_palette >> (overall_image[pixel_y][pixel_x] << 1)) & 3;

      glColor3f(colors[color_id][0], colors[color_id][1], colors[color_id][2]);
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

  fprintf(f, "\n>>> full rendered image\n");

  uint8_t overall_image[256][256];
  display_generate_image(d, overall_image);
  display_render(f, overall_image, terminal_palette);

  fprintf(f, "\n>>> screen image (at %d %d)\n", d->scx, d->scy);
  display_render_window_ascii(f, d, overall_image, terminal_palette);
}

void display_update(struct display* d, uint64_t cycles) {

  int prev_ly = d->ly;

  uint64_t current_period_cycles = (d->control & 0x80) ? (cycles % LCD_CYCLES_PER_FRAME) : 70000;
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
    int frame_num = cycles / LCD_CYCLES_PER_FRAME;
    if (d->render_freq && (frame_num % d->render_freq) == 0) {
      if (frame_num == 1)
        fprintf(stdout, "\033[s");
      uint8_t overall_image[256][256];
      display_generate_image(d, overall_image);

      if (d->render_terminal) {
        if (d->use_terminal_graphics)
          printf("\e[H");
        display_render_window_color(stdout, d, overall_image);
      }
      if (d->render_opengl)
        display_render_window_opengl(d, overall_image);
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

  if (prev_ly != d->ly) {
    if (d->ly == d->lyc) {
      d->status |= 0x04;
      if (d->status & 0x40)
        signal_interrupt(d->cpu, INTERRUPT_LCDSTAT, 1);
    } else
      d->status &= ~0x04;

    if ((d->status & 0x10) && (d->ly == 144))
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

    case 0x46:
      signal_debug_interrupt(d->cpu, "lcd oam dma transfer unimplemented");
      // TODO: DMA that shit
      break;

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
