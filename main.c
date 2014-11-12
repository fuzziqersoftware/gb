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

extern int terminal_fd;

int main(int argc, char* argv[]) {

  if (argc < 2) {
    fprintf(stderr, "usage: %s rom_file_name [options]\n", argv[0]);
    return -1;
  }

  const char* rom_file_name = NULL;
  int debug = 0, do_disassemble = 0, use_debug_cart = 0, fd = 0,
      wait_vblank = 0, modify_terminal_settings = 0, render_freq = 0;
  int32_t breakpoint_addr = -1, watchpoint_addr = -1, write_breakpoint_addr = -1, memory_watchpoint_addr = -1;
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
      else if (!strcmp(argv[x], "--term-settings"))
        modify_terminal_settings = 1;
    } else {
      rom_file_name = argv[x];
    }
  }

  union cart_data* cart;
  if (use_debug_cart) {
    fprintf(stderr, "creating debug cart\n");
    cart = debug_cart();
  } else {
    fprintf(stderr, "loading %s\n", rom_file_name);
    cart = load_cart_from_file(rom_file_name);
    if (!cart) {
      fprintf(stderr, "  failed\n");
      return -1;
    }
  }

  print_cart_info(cart);
  if (!verify_cart(cart)) {
    fprintf(stderr, "cart is corrupt or invalid\n");
    return -1;
  }

  if (do_disassemble) {
    int size = rom_size_for_rom_size_code(cart->header.rom_size);
    disassemble(NULL, cart, size, 0, size);
    return 0;
  }

  struct memory* m = create_memory(cart);
  if (!m) {
    fprintf(stderr, "failed to create memory\n");
    delete_cart(cart);
    return -2;
  }
  m->write_breakpoint_addr = write_breakpoint_addr;

  // create devices
  struct display lcd;
  struct serial ser;
  struct timer tim;
  struct audio aud;
  struct input inp;
  struct regs* r = create_cpu();
  if (!r) {
    fprintf(stderr, "failed to create cpu\n");
    delete_memory(m);
    delete_cart(cart);
    return -2;
  }

  // initialize devices
  r->debug = debug;
  r->ddx = memory_watchpoint_addr;
  display_init(&lcd, r, m, render_freq);
  serial_init(&ser, r);
  timer_init(&tim, r);
  audio_init(&aud, r);
  input_init(&inp, r);
  lcd.wait_vblank = wait_vblank;

  // bind devices to memory manager
  add_device(m, DEVICE_DISPLAY, &lcd);
  add_device(m, DEVICE_SERIAL, &ser);
  add_device(m, DEVICE_TIMER, &tim);
  add_device(m, DEVICE_AUDIO, &aud);
  add_device(m, DEVICE_CPU, r);
  add_device(m, DEVICE_INPUT, &inp);

  // set input fd
  inp.update_frequency = 10000;
  inp.fd = fd;
  if (fd >= 0) {
    if (!isatty(fd))
      fprintf(stderr, "warning: input device is not a terminal\n");
    else if (modify_terminal_settings) {
      terminal_fd = fd;
      set_terminal_raw();
    }
  } else
    fprintf(stderr, "warning: input is disabled\n");

  // go go gadget
  struct regs prev;
  memset(&prev, 0, sizeof(prev));
  while (r->pc != breakpoint_addr) {
    run_cycle(r, &prev, m);
    if (r->pc == watchpoint_addr)
      getchar();
    memcpy(&prev, r, sizeof(prev));
  }

  while (getchar()) {
    run_cycle(r, &prev, m);
    memcpy(&prev, r, sizeof(prev));
  }

  delete_memory(m);
  delete_cart(cart);
  return 0;
}
