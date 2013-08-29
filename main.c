#include <stdio.h>
#include <string.h>

#include "cart.h"
#include "cpu.h"

#include "display.h"
#include "serial.h"
#include "timer.h"
#include "audio.h"
#include "input.h"

int main(int argc, char* argv[]) {

  if (argc < 2) {
    printf("usage: %s rom_file_name [options]\n", argv[0]);
    return -1;
  }

  const char* rom_file_name = NULL;
  int debug = 0, do_disassemble = 0, use_debug_cart = 0;
  int32_t breakpoint_addr = -1, watchpoint_addr = -1, write_breakpoint_addr = -1, memory_watchpoint_addr = -1;
  int x;
  for (x = 1; x < argc; x++) {
    if (argv[x][0] == '-') {
      if (argv[x][1] == 'd')
        debug = 1;
      else if (argv[x][1] == 'b')
        sscanf(&argv[x][2], "%x", &breakpoint_addr);
      else if (argv[x][1] == 'w')
        sscanf(&argv[x][2], "%x", &watchpoint_addr);
      else if (argv[x][1] == 'm')
        sscanf(&argv[x][2], "%x", &write_breakpoint_addr);
      else if (argv[x][1] == 'M')
        sscanf(&argv[x][2], "%x", &memory_watchpoint_addr);
      else if (!strncmp(argv[x], "--disassemble", 13) || !strncmp(argv[x], "--dasm", 6))
        do_disassemble = 1;
      else if (!strncmp(argv[x], "--debug-cart", 12))
        use_debug_cart = 1;
    } else {
      rom_file_name = argv[x];
    }
  }

  union cart_data* cart;
  if (use_debug_cart) {
    printf("creating debug cart\n");
    cart = debug_cart();
  } else {
    printf("loading %s\n", rom_file_name);
    cart = load_cart_from_file(rom_file_name);
    if (!cart) {
      printf("  failed\n");
      return -1;
    }
  }

  print_cart_info(cart);
  if (!verify_cart(cart)) {
    printf("cart is corrupt or invalid\n");
    return -1;
  }

  if (do_disassemble) {
    int size = rom_size_for_rom_size_code(cart->header.rom_size);
    disassemble(NULL, cart, size, 0, size);
    return 0;
  }

  struct memory* m = create_memory(cart);
  if (!m) {
    printf("failed to create memory\n");
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
    printf("failed to create cpu\n");
    delete_memory(m);
    delete_cart(cart);
    return -2;
  }
  r->debug = debug;
  r->ddx = memory_watchpoint_addr;

  // bind devices together
  ser.cpu = r;
  lcd.cpu = r;
  tim.cpu = r;
  inp.cpu = r;

  // bind devices to memory manager
  add_device(m, DEVICE_DISPLAY, &lcd);
  add_device(m, DEVICE_SERIAL, &ser);
  add_device(m, DEVICE_TIMER, &tim);
  add_device(m, DEVICE_AUDIO, &aud);
  add_device(m, DEVICE_CPU, r);
  add_device(m, DEVICE_INPUT, &inp);

  // go go gadget
  while (r->pc != breakpoint_addr) {
    run_cycle(r, m);
    if (r->pc == watchpoint_addr)
      getchar();
  }

  while (getchar())
    run_cycle(r, m);

  delete_memory(m);
  delete_cart(cart);
  return 0;
}
