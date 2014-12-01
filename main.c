#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "cart.h"
#include "cpu.h"

#include "display.h"
#include "serial.h"
#include "timer.h"
#include "audio.h"
#include "input.h"
#include "terminal.h"
#include "opengl.h"

#ifdef MACOSX
#include "OpenGL/gl.h"
#include "OpenGL/glu.h"
#include "GLUT/glut.h"
#else
#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glut.h"
#endif

struct hardware {
  struct display lcd;
  struct serial ser;
  struct timer tim;
  struct audio aud;
  struct input inp;
  struct regs* cpu;
  struct memory* mem;
  union cart_data* cart;
} hw;

static void opengl_display_cb() {
  //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  // TODO display
  //glutSwapBuffers();
}

static void opengl_key_press_cb(uint8_t key, int x, int y) {
  if (key == 0x1B)
    exit(0);
  if (key == '\t')
    input_key_press(&hw.inp, KEY_B);
  if (key == ' ')
    input_key_press(&hw.inp, KEY_A);
  if (key == '\r')
    input_key_press(&hw.inp, KEY_START);
  if (key == 'Z' || key == 'z')
    input_key_press(&hw.inp, KEY_SELECT);
}

static void opengl_special_key_press_cb(int key, int x, int y) {
  if (key == GLUT_KEY_LEFT)
    input_key_press(&hw.inp, KEY_LEFT);
  if (key == GLUT_KEY_RIGHT)
    input_key_press(&hw.inp, KEY_RIGHT);
  if (key == GLUT_KEY_UP)
    input_key_press(&hw.inp, KEY_UP);
  if (key == GLUT_KEY_DOWN)
    input_key_press(&hw.inp, KEY_DOWN);
}

static void opengl_key_release_cb(uint8_t key, int x, int y) {
  if (key == '\t')
    input_key_release(&hw.inp, KEY_B);
  if (key == ' ')
    input_key_release(&hw.inp, KEY_A);
  if (key == '\r')
    input_key_release(&hw.inp, KEY_START);
  if (key == 'Z' || key == 'z')
    input_key_release(&hw.inp, KEY_SELECT);
}

static void opengl_special_key_release_cb(int key, int x, int y) {
  if (key == GLUT_KEY_LEFT)
    input_key_release(&hw.inp, KEY_LEFT);
  if (key == GLUT_KEY_RIGHT)
    input_key_release(&hw.inp, KEY_RIGHT);
  if (key == GLUT_KEY_UP)
    input_key_release(&hw.inp, KEY_UP);
  if (key == GLUT_KEY_DOWN)
    input_key_release(&hw.inp, KEY_DOWN);
}

static void opengl_idle_cb() {
  run_cycles(hw.cpu, hw.mem, LCD_CYCLES_PER_FRAME);
  glutPostRedisplay();
}

int main(int argc, char* argv[]) {

  if (argc < 2) {
    fprintf(stderr, "usage: %s rom_file_name [options]\n", argv[0]);
    return -1;
  }

  const char* rom_file_name = NULL;
  int debug = 0, do_disassemble = 0, use_debug_cart = 0, fd = 0,
      wait_vblank = 0, render_freq = 1, opengl_scale = 1;
  int32_t breakpoint_addr = -1, watchpoint_addr = -1, write_breakpoint_addr = -1, memory_watchpoint_addr = -1;
  uint64_t stop_after_cycles = 0;
  int x;
  for (x = 1; x < argc; x++) {
    if (argv[x][0] == '-') {
      if (argv[x][1] == 'd')
        debug = 1;
      else if (argv[x][1] == 'b')
        sscanf(&argv[x][2], "%x", &breakpoint_addr);
      else if (argv[x][1] == 'r')
        sscanf(&argv[x][2], "%d", &render_freq);
      else if (argv[x][1] == 'w')
        sscanf(&argv[x][2], "%x", &watchpoint_addr);
      else if (argv[x][1] == 'm')
        sscanf(&argv[x][2], "%x", &write_breakpoint_addr);
      else if (argv[x][1] == 'M')
        sscanf(&argv[x][2], "%x", &memory_watchpoint_addr);
      else if (!strcmp(argv[x], "--disassemble") || !strcmp(argv[x], "--dasm"))
        do_disassemble = 1;
      else if (!strcmp(argv[x], "--debug-cart"))
        use_debug_cart = 1;
      else if (!strcmp(argv[x], "--wait-vblank"))
        wait_vblank = 1;
      else if (!strcmp(argv[x], "--disable-input"))
        fd = -1;
      else if (!strncmp(argv[x], "--opengl-scale=", 15))
        sscanf(&argv[x][15], "%d", &opengl_scale);
      else if (!strncmp(argv[x], "--stop-cycles=", 14))
        sscanf(&argv[x][14], "%016llX", &stop_after_cycles);
    } else {
      rom_file_name = argv[x];
    }
  }

  if (use_debug_cart) {
    fprintf(stderr, "creating debug cart\n");
    hw.cart = debug_cart();
  } else {
    fprintf(stderr, "loading %s\n", rom_file_name);
    hw.cart = load_cart_from_file(rom_file_name);
    if (!hw.cart) {
      fprintf(stderr, "  failed\n");
      return -1;
    }
  }

  print_cart_info(hw.cart);
  if (!verify_cart(hw.cart)) {
    fprintf(stderr, "cart is corrupt or invalid\n");
    return -1;
  }

  if (do_disassemble) {
    int size = rom_size_for_rom_size_code(hw.cart->header.rom_size);
    disassemble(NULL, hw.cart, size, 0, size);
    return 0;
  }

  hw.mem = create_memory(hw.cart);
  if (!hw.mem) {
    fprintf(stderr, "failed to create memory\n");
    delete_cart(hw.cart);
    return -2;
  }
  hw.mem->write_breakpoint_addr = write_breakpoint_addr;

  // create devices
  hw.cpu = create_cpu();
  if (!hw.cpu) {
    fprintf(stderr, "failed to create cpu\n");
    delete_memory(hw.mem);
    delete_cart(hw.cart);
    return -2;
  }

  // initialize devices
  hw.cpu->debug = debug;
  hw.cpu->ddx = memory_watchpoint_addr;
  display_init(&hw.lcd, hw.cpu, hw.mem, render_freq);
  serial_init(&hw.ser, hw.cpu);
  timer_init(&hw.tim, hw.cpu);
  audio_init(&hw.aud, hw.cpu);
  input_init(&hw.inp, hw.cpu);
  hw.lcd.wait_vblank = wait_vblank;

  // bind devices to memory manager
  add_device(hw.mem, DEVICE_DISPLAY, &hw.lcd);
  add_device(hw.mem, DEVICE_SERIAL, &hw.ser);
  add_device(hw.mem, DEVICE_TIMER, &hw.tim);
  add_device(hw.mem, DEVICE_AUDIO, &hw.aud);
  add_device(hw.mem, DEVICE_CPU, hw.cpu);
  add_device(hw.mem, DEVICE_INPUT, &hw.inp);

  // setup opengl
  opengl_init();
  opengl_create_window(160 * opengl_scale, 144 * opengl_scale, "gb");

  glutDisplayFunc(opengl_display_cb);
  glutKeyboardFunc(opengl_key_press_cb);
  glutKeyboardUpFunc(opengl_key_release_cb);
  glutSpecialFunc(opengl_special_key_press_cb);
  glutSpecialUpFunc(opengl_special_key_release_cb);
  glutIdleFunc(opengl_idle_cb);

  glutMainLoop();

  // clean up
  delete_memory(hw.mem);
  delete_cart(hw.cart);
  return 0;
}
