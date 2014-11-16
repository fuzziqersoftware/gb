#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

#include "input.h"
#include "cpu.h"



void input_init(struct input* i, struct regs* cpu) {
  memset(i, 0, sizeof(*i));
  i->cpu = cpu;
}

int input_read_byte(int fd) {
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(fd, &fds);

  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  select(fd + 1, &fds, NULL, NULL, &tv);

  if (!FD_ISSET(fd, &fds)) {
    //fprintf(stderr, "input: no data to read\n");
    return -1;
  }

  int data = 0;
  if (read(fd, &data, 1) == 0)
    data = 4;  // ctrl+d
  return data;
}

void input_update(struct input* i, uint64_t cycles) {
  if (cycles % i->update_frequency)
    return;

  i->keys_pressed = 0;

  if (i->fd < 0)
    return;

  int x;
  int data[16];
  for (x = 0; x < 15; x++)
    data[x] = input_read_byte(i->fd);
  data[15] = -1;

  for (x = 0; data[x] != -1; x++) {
    fprintf(stderr, "data: %02X (%c)\n", data[x], data[x]);
    if (data[x] == 4) // ctrl+d
      signal_debug_interrupt(i->cpu, "requested by user");
    if ((x < 13) && (data[x] == 0x1B) && (data[x + 1] == 0x5B)) {
      if (data[x + 2] == 'A')
        i->keys_pressed |= KEY_UP;
      if (data[x + 2] == 'B')
        i->keys_pressed |= KEY_DOWN;
      if (data[x + 2] == 'C')
        i->keys_pressed |= KEY_RIGHT;
      if (data[x + 2] == 'D')
        i->keys_pressed |= KEY_LEFT;
      x += 2;
    } else {
    	if (data[x] == ' ')
    		i->keys_pressed |= KEY_A;
    	if (data[x] == '\n')
    		i->keys_pressed |= KEY_START;
    	if (data[x] == '\t')
    		i->keys_pressed |= KEY_B;
    	if (data[x] == 'Z')
    		i->keys_pressed |= KEY_SELECT;
    }
  }
  if (i->keys_pressed)
    fprintf(stderr, "keys: %02X\n", i->keys_pressed);
  // apparently the interrupt doesn't work with CGB hardware; should we even implement it? lol
}

uint8_t read_input_register(struct input* i, uint8_t addr) {
  if (i->selected == SELECTED_KEYS)
    return (~i->keys_pressed) & 0x0F;
  if (i->selected == SELECTED_DIRECTIONS)
    return ((~i->keys_pressed) >> 4) & 0x0F;
  return 0;
}

void write_input_register(struct input* i, uint8_t addr, uint8_t value) {
  switch (value & 0x30) {
    case 0x30:
    case 0x00: // not valid: you can't select both
      i->selected = SELECTED_NOTHING;
      break;
    case 0x10:
      i->selected = SELECTED_KEYS;
      break;
    case 0x20:
      i->selected = SELECTED_DIRECTIONS;
  }
}
