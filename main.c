#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <GLFW/glfw3.h>

#include "cart.h"
#include "cpu.h"

#include "display.h"
#include "serial.h"
#include "timer.h"
#include "audio.h"
#include "input.h"
#include "terminal.h"
#include "opengl.h"
#include "util.h"

struct hardware {
  struct display lcd;
  struct serial ser;
  struct timer tim;
  struct audio aud;
  struct input inp;
  struct regs* cpu;
  struct memory* mem;
  union cart_data* cart;

  int paused;
} hw;



static void glfw_error_cb(int error, const char* description) {
  fprintf(stderr, "[GLFW %d] %s\n", error, description);
}

static void glfw_key_cb(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (action == GLFW_PRESS) {
    if (key == GLFW_KEY_E)
      glfwSetWindowShouldClose(window, 1);

    else if (key == GLFW_KEY_ESCAPE) {
      hw.paused = !hw.paused;
      if (hw.paused)
        display_pause(&hw.lcd);
      else
        display_resume(&hw.lcd);

    } else if (key == GLFW_KEY_TAB)
      input_key_press(&hw.inp, KEY_B);
    else if (key == GLFW_KEY_SPACE)
      input_key_press(&hw.inp, KEY_A);
    else if (key == GLFW_KEY_ENTER)
      input_key_press(&hw.inp, KEY_START);
    else if (key == GLFW_KEY_Z)
      input_key_press(&hw.inp, KEY_SELECT);
    else if (key == GLFW_KEY_LEFT)
      input_key_press(&hw.inp, KEY_LEFT);
    else if (key == GLFW_KEY_RIGHT)
      input_key_press(&hw.inp, KEY_RIGHT);
    else if (key == GLFW_KEY_UP)
      input_key_press(&hw.inp, KEY_UP);
    else if (key == GLFW_KEY_DOWN)
      input_key_press(&hw.inp, KEY_DOWN);

  } else if (action == GLFW_RELEASE) {
    if (key == GLFW_KEY_TAB)
      input_key_release(&hw.inp, KEY_B);
    else if (key == GLFW_KEY_SPACE)
      input_key_release(&hw.inp, KEY_A);
    else if (key == GLFW_KEY_ENTER)
      input_key_release(&hw.inp, KEY_START);
    else if (key == GLFW_KEY_Z)
      input_key_release(&hw.inp, KEY_SELECT);
    else if (key == GLFW_KEY_LEFT)
      input_key_release(&hw.inp, KEY_LEFT);
    else if (key == GLFW_KEY_RIGHT)
      input_key_release(&hw.inp, KEY_RIGHT);
    else if (key == GLFW_KEY_UP)
      input_key_release(&hw.inp, KEY_UP);
    else if (key == GLFW_KEY_DOWN)
      input_key_release(&hw.inp, KEY_DOWN);
  }
}

static void display_render_cb(struct display* d, void* arg) {
  display_render_window_opengl(d);
  glfwSwapBuffers((GLFWwindow*)arg);
}



int main(int argc, char* argv[]) {

  if (argc < 2) {
    fprintf(stderr, "usage: %s rom_file_name [options]\n", argv[0]);
    return -1;
  }

  const char* rom_file_name = NULL;
  int debug = 0, do_disassemble = 0, use_debug_cart = 0, wait_vblank = 0,
      render_freq = 1, opengl_scale = 1, highlight_sprites = 0;
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
      else if (!strcmp(argv[x], "--highlight-sprites"))
        highlight_sprites = 1;
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

  if (!glfwInit()) {
    fprintf(stderr, "failed to initialize GLFW\n");
    return -3;
  }
  glfwSetErrorCallback(glfw_error_cb);

  GLFWwindow* window = glfwCreateWindow(160 * opengl_scale, 144 * opengl_scale,
      "gb", NULL, NULL);
  glfwSetKeyCallback(window, glfw_key_cb);

  glfwMakeContextCurrent(window);

  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);
  glLineWidth(3);
  glPointSize(12);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // create devices
  hw.mem = create_memory(hw.cart);
  if (!hw.mem) {
    fprintf(stderr, "failed to create memory\n");
    delete_cart(hw.cart);
    return -2;
  }
  hw.mem->write_breakpoint_addr = write_breakpoint_addr;

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
  display_init(&hw.lcd, hw.cpu, hw.mem, render_freq, display_render_cb, window);
  serial_init(&hw.ser, hw.cpu);
  timer_init(&hw.tim, hw.cpu);
  audio_init(&hw.aud, hw.cpu);
  input_init(&hw.inp, hw.cpu);
  hw.lcd.wait_vblank = wait_vblank;
  hw.lcd.highlight_sprites = highlight_sprites;

  // bind devices to memory manager
  add_device(hw.mem, DEVICE_DISPLAY, &hw.lcd);
  add_device(hw.mem, DEVICE_SERIAL, &hw.ser);
  add_device(hw.mem, DEVICE_TIMER, &hw.tim);
  add_device(hw.mem, DEVICE_AUDIO, &hw.aud);
  add_device(hw.mem, DEVICE_CPU, hw.cpu);
  add_device(hw.mem, DEVICE_INPUT, &hw.inp);

  hw.paused = 0;

  while (!glfwWindowShouldClose(window)) {
    if (!hw.paused)
      run_cycles(hw.cpu, hw.mem, LCD_CYCLES_PER_FRAME);

    else {
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      display_render_window_opengl(&hw.lcd);

      glBegin(GL_QUADS);
      glColor4f(0.0f, 0.0f, 0.0f, 0.2f);
      glVertex3f(-1.0f, -1.0f, 1.0f);
      glVertex3f(1.0f, -1.0f, 1.0f);
      glVertex3f(1.0f, 1.0f, 1.0f);
      glVertex3f(-1.0f, 1.0f, 1.0f);

      float xpos;
      for (xpos = -1.5f - 0.2 +
            (float)(now() % 3000000) / 3000000 * 2 * 0.2;
           xpos < 3.5f;
           xpos += (2 * 0.2)) {
        glVertex2f(xpos, 1);
        glVertex2f(xpos + 0.2, 1);
        glVertex2f(xpos - 2 + 0.2, -1);
        glVertex2f(xpos - 2, -1);
      }
      glEnd();
      glfwSwapBuffers(window);
    }

    glfwPollEvents();
  }

  // clean up
  delete_memory(hw.mem);
  delete_cart(hw.cart);
  return 0;
}
