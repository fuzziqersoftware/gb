#include <stdint.h>

#include "input.h"
#include "cpu.h"



void input_update(struct input* i, int cycles) {
	// TODO: read keys from stdin or something I dunno
	// apparently the interrupt doesn't work with CGB hardware; should we even implement it? lol
}

uint8_t read_input_register(struct input* i, uint8_t addr) {
	if (i->selected == SELECTED_KEYS)
		return i->keys_pressed & 0x0F;
	if (i->selected == SELECTED_DIRECTIONS)
		return (i->keys_pressed >> 4) & 0x0F;
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
