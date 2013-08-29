#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

#include "cpu.h"

#define KEY_DOWN    0x80
#define KEY_UP      0x40
#define KEY_LEFT    0x20
#define KEY_RIGHT   0x10
#define KEY_START   0x08
#define KEY_SELECT  0x04
#define KEY_B       0x02
#define KEY_A       0x01

#define SELECTED_NOTHING     0x00
#define SELECTED_DIRECTIONS  0x01
#define SELECTED_KEYS        0x02

struct input {
  int keys_pressed;
  int selected;
  struct regs* cpu;
};

void input_update(struct input* i, int cycles);
uint8_t read_input_register(struct input* i, uint8_t addr);
void write_input_register(struct input* i, uint8_t addr, uint8_t value);

#endif // INPUT_H
