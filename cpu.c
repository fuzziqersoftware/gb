#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "cpu.h"
#include "debug.h"
#include "terminal.h"



///////////////////////////////////////////////////////////////////////////////
// register state

int get_flag_value(struct regs* r, int flag) {
  return (r->f >> flag) & 1;
}

void set_flag_value(struct regs* r, int flag, int value) {
  r->f = (r->f & ~(1 << flag)) | (value << flag);
}

int get_flag(struct regs* r, int flag_id) {
  if (flag_id == 0) // NZ
    return !get_flag_value(r, FLAG_Z);
  else if (flag_id == 1) // Z
    return get_flag_value(r, FLAG_Z);
  else if (flag_id == 2) // NC
    return !get_flag_value(r, FLAG_C);
  else if (flag_id == 3) // C
    return get_flag_value(r, FLAG_C);
  return 0;
}

static inline uint8_t make_flags_reg(int z, int n, int h, int c) {
  return (z << 7) | (n << 6) | (h << 5) | (c << 4);
}

static inline uint16_t* get_r_ptr(struct regs* r, int r_) {
  return &r->bc + r_;
}

static inline uint16_t read_r_value(struct regs* r, int r_) {
  return *get_r_ptr(r, r_);
}

static inline void write_r_value(struct regs* r, int r_, uint16_t value) {
  *get_r_ptr(r, r_) = value;
}

static inline uint8_t read_x_value(struct regs* r, struct memory* m, int x) {
  if (x == 0)
    return r->b;
  if (x == 1)
    return r->c;
  if (x == 2)
    return r->d;
  if (x == 3)
    return r->e;
  if (x == 4)
    return r->h;
  if (x == 5)
    return r->l;
  if (x == 6)
    return read8(m, r->hl);
  if (x == 7)
    return r->a;
  return 0; // TODO: should probably fail here in some way
}

static inline void write_x_value(struct regs* r, struct memory* m, int x, uint8_t value) {
  if (x == 0)
    r->b = value;
  else if (x == 1)
    r->c = value;
  else if (x == 2)
    r->d = value;
  else if (x == 3)
    r->e = value;
  else if (x == 4)
    r->h = value;
  else if (x == 5)
    r->l = value;
  else if (x == 6)
    write8(m, r->hl, value);
  else if (x == 7)
    r->a = value;
}

static inline uint8_t read_e_value(struct regs* r, struct memory* m, int e) {
  if (e == 0)
    return read8(m, r->bc);
  if (e == 1)
    return read8(m, r->de);
  if (e == 2)
    return read8(m, r->hl++);
  if (e == 3)
    return read8(m, r->hl--);
  return 0; // TODO: should probably fail here in some way
}

static inline void write_e_value(struct regs* r, struct memory* m, int e, uint8_t value) {
  if (e == 0)
    write8(m, r->bc, value);
  else if (e == 1)
    write8(m, r->de, value);
  else if (e == 2)
    write8(m, r->hl++, value);
  else if (e == 3)
    write8(m, r->hl--, value);
}

static inline uint8_t ifetch(struct regs* r, struct memory* m) {
  return read8(m, r->pc++);
}

static inline uint16_t ifetch_word(struct regs* r, struct memory* m) {
  register int v = read16(m, r->pc);
  r->pc += 2;
  return v;
}

static inline void stack_push(struct regs* r, struct memory* m, uint16_t value) {
  r->sp -= 2;
  write16(m, r->sp, value);
}

static inline uint16_t stack_pop(struct regs* r, struct memory* m) {
  register uint16_t v = read16(m, r->sp);
  r->sp += 2;
  return v;
}



///////////////////////////////////////////////////////////////////////////////
// opcode decoding

static inline int get_r_field(uint8_t op) {
  return (op >> 4) & 3;
}

static inline int get_x_field(uint8_t op) {
  return (op >> 3) & 7;
}

static inline int get_bit_field(uint8_t op) {
  return (op >> 3) & 7;
}

static inline int get_x2_field(uint8_t op) {
  return op & 7;
}

static inline int get_e_field(uint8_t op) {
  return (op >> 4) & 3;
}

static inline int get_flag_field(uint8_t op) {
  return (op >> 3) & 3;
}

static inline int get_z_field(uint8_t op) {
  return (op >> 3) & 7;
}

static inline int16_t sign_extend(uint8_t x) {
  return (x & 0x80) ? (0xFF00 | x) : x;
}



///////////////////////////////////////////////////////////////////////////////
// normal opcodes

void run_op_nop(struct regs* r, struct memory* m, uint8_t op) { }

void run_op_ld_a16_sp(struct regs* r, struct memory* m, uint8_t op) {
  write16(m, ifetch_word(r, m), r->sp);
}

void run_op_stop(struct regs* r, struct memory* m, uint8_t op) {
  r->stop = 1;
  if (!r->ime)
    printf("warning: stopped with ime disabled\n");

  if (r->speed_switch & 0x01) {
    r->speed_switch = (r->speed_switch ^ 0x80) & 0x80;
    r->stop = 0;
    printf("speed switched to %s; skipping stop instruction\n", is_double_speed_mode(r) ? "double" : "normal");
  }
}

void run_op_jr_r8(struct regs* r, struct memory* m, uint8_t op) {
  uint8_t v = ifetch(r, m);
  if (v == 0xFE) {
    printf("warning: jr -02 opcode halted system\n");
    r->stop = 1;
  }
  r->pc += sign_extend(v);
}

void run_op_jr_flag_r8(struct regs* r, struct memory* m, uint8_t op) {
  int16_t offset = sign_extend(ifetch(r, m));
  if (get_flag(r, get_flag_field(op)))
    r->pc += offset;
}

void run_op_rlca(struct regs* r, struct memory* m, uint8_t op) {
  r->a = (r->a << 1) | ((r->a >> 7) & 0x01);
  r->f = make_flags_reg(0, 0, 0, r->a & 1);
}

void run_op_rrca(struct regs* r, struct memory* m, uint8_t op) {
  // TODO verify
  r->a = ((r->a >> 1) & 0x7F) | (r->a << 7);
  r->f = make_flags_reg(0, 0, 0, !!(r->a & 0x80));
}

void run_op_rla(struct regs* r, struct memory* m, uint8_t op) {
  uint8_t new_flags = make_flags_reg(0, 0, 0, (r->a >> 7) & 1);
  r->a = (r->a << 1) | get_flag_value(r, FLAG_C);
  r->f = new_flags;
}

void run_op_rra(struct regs* r, struct memory* m, uint8_t op) {
  uint8_t new_flags = make_flags_reg(0, 0, 0, r->a & 1);
  r->a = (r->a >> 1) | (get_flag_value(r, FLAG_C) << 7);
  r->f = new_flags;
}

void run_op_daa(struct regs* r, struct memory* m, uint8_t op) {
  uint16_t value = r->a;
  if (!get_flag_value(r, FLAG_N)) {
    if (get_flag_value(r, FLAG_H) || (value & 0x0F) > 9)
      value += 6;
    if (get_flag_value(r, FLAG_C) || (value > 0x9F))
      value += 0x60;
  } else {
    if (get_flag_value(r, FLAG_H)) {
      value -= 6;
      if (!get_flag_value(r, FLAG_C))
        value &= 0xFF;
    }
    if (get_flag_value(r, FLAG_C))
        value -= 0x60;
  }
  //uint16_t orig_af = r->af;
  r->a = value & 0xFF;
  r->f = make_flags_reg(r->a == 0, get_flag_value(r, FLAG_N), 0, (value & 0x0100) || get_flag_value(r, FLAG_C));
  //printf("daa: %04X -> %04X\n", orig_af, r->af);
}

void run_op_cpl(struct regs* r, struct memory* m, uint8_t op) {
  r->a = ~r->a;
  set_flag_value(r, FLAG_N, 1);
  set_flag_value(r, FLAG_H, 1);
}

void run_op_scf(struct regs* r, struct memory* m, uint8_t op) {
  set_flag_value(r, FLAG_N, 0);
  set_flag_value(r, FLAG_H, 0);
  set_flag_value(r, FLAG_C, 1);
}

void run_op_ccf(struct regs* r, struct memory* m, uint8_t op) {
  set_flag_value(r, FLAG_N, 0);
  set_flag_value(r, FLAG_H, 0);
  set_flag_value(r, FLAG_C, get_flag_value(r, FLAG_C) ^ 1);
}

void run_op_ld_r_d16(struct regs* r, struct memory* m, uint8_t op) {
  write_r_value(r, get_r_field(op), ifetch_word(r, m));
}

void run_op_ld_e_a(struct regs* r, struct memory* m, uint8_t op) {
  write_e_value(r, m, get_e_field(op), r->a);
}

void run_op_ld_a_e(struct regs* r, struct memory* m, uint8_t op) {
  r->a = read_e_value(r, m, get_e_field(op));
}

void run_op_inc_r(struct regs* r, struct memory* m, uint8_t op) {
  // TODO: see comment in run_op_dec_r
  int field_value = get_r_field(op);
  write_r_value(r, field_value, read_r_value(r, field_value) + 1);
}

void run_op_dec_r(struct regs* r, struct memory* m, uint8_t op) {
  // TODO: apparently there's a bug in inc/dec where it trashes sprites if the high byte is 0xFE before the operation
  // do we want to implement this? (see mame source: http://mamedev.org/source/src/emu/cpu/lr35902/opc_main.h)
  int field_value = get_r_field(op);
  write_r_value(r, field_value, read_r_value(r, field_value) - 1);
}

void run_op_inc_x(struct regs* r, struct memory* m, uint8_t op) {
  int x = get_x_field(op);
  uint8_t v = read_x_value(r, m, x) + 1;
  write_x_value(r, m, x, v);
  r->f = make_flags_reg(v == 0, 0, (v & 0x0F) == 0, get_flag_value(r, FLAG_C));
}

void run_op_dec_x(struct regs* r, struct memory* m, uint8_t op) {
  int x = get_x_field(op);
  uint8_t v = read_x_value(r, m, x) - 1;
  write_x_value(r, m, x, v);
  r->f = make_flags_reg(v == 0, 1, (v & 0x0F) == 0x0F, get_flag_value(r, FLAG_C));
}

void run_op_ld_x_d8(struct regs* r, struct memory* m, uint8_t op) {
  write_x_value(r, m, get_x_field(op), ifetch(r, m));
}

void run_op_add_hl_r(struct regs* r, struct memory* m, uint8_t op) {
  register uint16_t add_value = read_r_value(r, get_r_field(op));
  register uint16_t half_test = (r->hl & 0x0FFF) + (add_value & 0x0FFF);
  register uint16_t new_value = r->hl + add_value;
  r->f = make_flags_reg(get_flag_value(r, FLAG_Z), 0, (half_test & 0xF000) != 0, (new_value < add_value) || (new_value < r->hl));
  r->hl = new_value;
}

void run_op_ld_x_x(struct regs* r, struct memory* m, uint8_t op) {
  write_x_value(r, m, get_x_field(op), read_x_value(r, m, get_x2_field(op)));
}

void run_op_halt(struct regs* r, struct memory* m, uint8_t op) {
  r->wait_for_interrupt = 1;
  if (!r->ime)
    printf("warning: halted with ime disabled\n");
}

void run_op_add_a_x(struct regs* r, struct memory* m, uint8_t op) {
  register uint8_t add_value = read_x_value(r, m, get_x2_field(op));
  register uint8_t half_test = (r->a & 0x0F) + (add_value & 0x0F);
  register uint8_t new_value = r->a + add_value;
  r->f = make_flags_reg(new_value == 0, 0, (half_test & 0xF0) != 0, (new_value < add_value) || (new_value < r->a));
  r->a = new_value;
}

void run_op_adc_a_x(struct regs* r, struct memory* m, uint8_t op) {
  register uint8_t add_value = read_x_value(r, m, get_x2_field(op));
  register uint8_t half_test = (r->a & 0x0F) + (add_value & 0x0F) + get_flag_value(r, FLAG_C);
  register uint8_t new_value = r->a + add_value + get_flag_value(r, FLAG_C);
  r->f = make_flags_reg(new_value == 0, 0, (half_test & 0xF0) != 0, (new_value < (add_value + get_flag_value(r, FLAG_C))) || (new_value < r->a));
  r->a = new_value;
}

void run_op_sub_a_x(struct regs* r, struct memory* m, uint8_t op) {
  register uint8_t sub_value = read_x_value(r, m, get_x2_field(op));
  register uint8_t half_test = (r->a & 0x0F) - (sub_value & 0x0F);
  register uint8_t new_value = r->a - sub_value;
  r->f = make_flags_reg(new_value == 0, 1, (half_test & 0xF0) != 0, new_value > r->a);
  r->a = new_value;
}

void run_op_subc_a_x(struct regs* r, struct memory* m, uint8_t op) {
  register uint8_t sub_value = read_x_value(r, m, get_x2_field(op));
  register uint8_t half_test = (r->a & 0x0F) - (sub_value & 0x0F) - get_flag_value(r, FLAG_C);
  register uint8_t new_value = r->a - sub_value - get_flag_value(r, FLAG_C);
  r->f = make_flags_reg(new_value == 0, 1, (half_test & 0xF0) != 0, (new_value > r->a) || (get_flag_value(r, FLAG_C) && r->a == new_value));
  r->a = new_value;
}

void run_op_and_a_x(struct regs* r, struct memory* m, uint8_t op) {
  r->a &= read_x_value(r, m, get_x2_field(op));
  r->f = make_flags_reg(r->a == 0, 0, 1, 0);
}

void run_op_xor_a_x(struct regs* r, struct memory* m, uint8_t op) {
  r->a ^= read_x_value(r, m, get_x2_field(op));
  r->f = make_flags_reg(r->a == 0, 0, 0, 0);
}

void run_op_or_a_x(struct regs* r, struct memory* m, uint8_t op) {
  r->a |= read_x_value(r, m, get_x2_field(op));
  r->f = make_flags_reg(r->a == 0, 0, 0, 0);
}

void run_op_cp_a_x(struct regs* r, struct memory* m, uint8_t op) {
  register uint8_t sub_value = read_x_value(r, m, get_x2_field(op));
  register uint8_t half_test = (r->a & 0x0F) - (sub_value & 0x0F);
  register uint8_t new_value = r->a - sub_value;
  r->f = make_flags_reg(new_value == 0, 1, (half_test & 0xF0) != 0, new_value > r->a);
}

void run_op_ret_y(struct regs* r, struct memory* m, uint8_t op) {
  if (get_flag(r, get_flag_field(op)))
    r->pc = stack_pop(r, m);
}

void run_op_ldh_ff00_a8_a(struct regs* r, struct memory* m, uint8_t op) {
  write8(m, 0xFF00 + ifetch(r, m), r->a);
}

void run_op_add_sp_r8(struct regs* r, struct memory* m, uint8_t op) {
  register uint16_t add_value = sign_extend(ifetch(r, m));
  register uint16_t half_test, carry_test;

  if (add_value < 0x8000) {
    half_test = ((r->sp & 0x0F) + (add_value & 0x0F)) & 0xF0;
    carry_test = ((r->sp & 0xFF) + add_value) & 0xFF00;
    r->sp += add_value;
  } else {
    uint16_t new_sp = r->sp + add_value;
    half_test = (new_sp & 0x0F) <= (r->sp & 0x0F);
    carry_test = (new_sp & 0xFF) <= (r->sp & 0xFF);
    r->sp = new_sp;
  }

  r->f = make_flags_reg(0, 0, half_test != 0, carry_test != 0);
}

void run_op_ldh_a_ff00_a8(struct regs* r, struct memory* m, uint8_t op) {
  r->a = read8(m, 0xFF00 + ifetch(r, m));
}

void run_op_ld_hl_sp_r8(struct regs* r, struct memory* m, uint8_t op) {
  register uint16_t add_value = sign_extend(ifetch(r, m));
  register uint16_t half_test, carry_test;

  if (add_value < 0x8000) {
    r->hl = r->sp + add_value;
    half_test = ((r->sp & 0x0F) + (add_value & 0x0F)) & 0xF0;
    carry_test = ((r->sp & 0xFF) + add_value) & 0xFF00;
  } else {
    r->hl = r->sp + add_value;
    half_test = (r->hl & 0x0F) <= (r->sp & 0x0F);
    carry_test = (r->hl & 0xFF) <= (r->sp & 0xFF);
  }

  r->f = make_flags_reg(0, 0, half_test != 0, carry_test != 0);
}

void run_op_pop_r(struct regs* r, struct memory* m, uint8_t op) {
  int r_ = get_r_field(op);
  if (r_ == 3)
    r->af = stack_pop(r, m) & 0xFFF0; // af instead of sp (you can't pop sp)
  else
    write_r_value(r, r_, stack_pop(r, m));
}

void run_op_ret(struct regs* r, struct memory* m, uint8_t op) {
  r->pc = stack_pop(r, m);
}

void run_op_reti(struct regs* r, struct memory* m, uint8_t op) {
  r->pc = stack_pop(r, m);
  r->ime = 1;
}

void run_op_jp_hl(struct regs* r, struct memory* m, uint8_t op) {
  r->pc = r->hl;
}

void run_op_ld_sp_hl(struct regs* r, struct memory* m, uint8_t op) {
  r->sp = r->hl;
}

void run_op_jp_y_a16(struct regs* r, struct memory* m, uint8_t op) {
  register int new_pc = ifetch_word(r, m);
  if (get_flag(r, get_flag_field(op)))
    r->pc = new_pc;
}

void run_op_ld_ff00_c_a(struct regs* r, struct memory* m, uint8_t op) {
  write8(m, 0xFF00 + r->c, r->a);
}

void run_op_ld_a16_a(struct regs* r, struct memory* m, uint8_t op) {
  write8(m, ifetch_word(r, m), r->a);
}

void run_op_ld_a_ff00_c(struct regs* r, struct memory* m, uint8_t op) {
  r->a = read8(m, 0xFF00 + r->c);
}

void run_op_ld_a_a16(struct regs* r, struct memory* m, uint8_t op) {
  r->a = read8(m, ifetch_word(r, m));
}

void run_op_jp_a16(struct regs* r, struct memory* m, uint8_t op) {
  r->pc = read16(m, r->pc);
}

void run_op_di(struct regs* r, struct memory* m, uint8_t op) {
  r->ime = 0;
}

void run_op_ei(struct regs* r, struct memory* m, uint8_t op) {
  r->ime = 1;
}

void run_op_call_y_a16(struct regs* r, struct memory* m, uint8_t op) {
  register int new_pc = ifetch_word(r, m);
  if (get_flag(r, get_flag_field(op))) {
    stack_push(r, m, r->pc);
    r->pc = new_pc;
  }
}

void run_op_call_a16(struct regs* r, struct memory* m, uint8_t op) {
  register int new_pc = ifetch_word(r, m);
  stack_push(r, m, r->pc);
  r->pc = new_pc;
}

void run_op_push_r(struct regs* r, struct memory* m, uint8_t op) {
  int r_ = get_r_field(op);
  if (r_ == 3)
    stack_push(r, m, r->af); // af instead of sp (you can't push sp)
  else
    stack_push(r, m, read_r_value(r, r_));
}

void run_op_add_a_d8(struct regs* r, struct memory* m, uint8_t op) {
  register uint8_t add_value = ifetch(r, m);
  register uint8_t half_test = (r->a & 0x0F) + (add_value & 0x0F);
  register uint8_t new_value = r->a + add_value;
  r->f = make_flags_reg(new_value == 0, 0, (half_test & 0xF0) != 0, (new_value < add_value) || (new_value < r->a));
  r->a = new_value;
}

void run_op_adc_a_d8(struct regs* r, struct memory* m, uint8_t op) {
  register uint8_t add_value = ifetch(r, m);
  register uint8_t half_test = (r->a & 0x0F) + (add_value & 0x0F) + get_flag_value(r, FLAG_C);
  register uint8_t new_value = r->a + add_value + get_flag_value(r, FLAG_C);
  r->f = make_flags_reg(new_value == 0, 0, (half_test & 0xF0) != 0, (new_value < (add_value + get_flag_value(r, FLAG_C))) || (new_value < r->a));
  r->a = new_value;
}

void run_op_sub_a_d8(struct regs* r, struct memory* m, uint8_t op) {
  register uint8_t sub_value = ifetch(r, m);
  register uint8_t half_test = (r->a & 0x0F) - (sub_value & 0x0F);
  register uint8_t new_value = r->a - sub_value;
  r->f = make_flags_reg(new_value == 0, 1, (half_test & 0xF0) != 0, new_value > r->a);
  r->a = new_value;
}

void run_op_subc_a_d8(struct regs* r, struct memory* m, uint8_t op) {
  register uint8_t sub_value = ifetch(r, m);
  register uint8_t half_test = (r->a & 0x0F) - (sub_value & 0x0F) - get_flag_value(r, FLAG_C);
  register uint8_t new_value = r->a - sub_value - get_flag_value(r, FLAG_C);
  r->f = make_flags_reg(new_value == 0, 1, (half_test & 0xF0) != 0, (new_value > r->a) || (get_flag_value(r, FLAG_C) && r->a == new_value));
  r->a = new_value;
}

void run_op_and_a_d8(struct regs* r, struct memory* m, uint8_t op) {
  r->a &= ifetch(r, m);
  r->f = make_flags_reg(r->a == 0, 0, 1, 0);
}

void run_op_xor_a_d8(struct regs* r, struct memory* m, uint8_t op) {
  r->a ^= ifetch(r, m);
  r->f = make_flags_reg(r->a == 0, 0, 0, 0);
}

void run_op_or_a_d8(struct regs* r, struct memory* m, uint8_t op) {
  r->a |= ifetch(r, m);
  r->f = make_flags_reg(r->a == 0, 0, 0, 0);
}

void run_op_cp_a_d8(struct regs* r, struct memory* m, uint8_t op) {
  register uint8_t sub_value = ifetch(r, m);
  register uint8_t half_test = (r->a & 0x0F) - (sub_value & 0x0F);
  register uint8_t new_value = r->a - sub_value;
  r->f = make_flags_reg(new_value == 0, 1, (half_test & 0xF0) != 0, new_value > r->a);
}

void run_op_rst(struct regs* r, struct memory* m, uint8_t op) {
  stack_push(r, m, r->pc);
  r->pc = get_z_field(op) * 8;
}



///////////////////////////////////////////////////////////////////////////////
// CB opcodes

void run_op_rlc(struct regs* r, struct memory* m, uint8_t op) {
  uint8_t x2 = get_x2_field(op);
  uint8_t v = read_x_value(r, m, x2);
  v = (v << 1) | ((v >> 7) & 0x01);
  write_x_value(r, m, x2, v);
  r->f = make_flags_reg(v == 0, 0, 0, v & 1);
}

void run_op_rrc(struct regs* r, struct memory* m, uint8_t op) {
  uint8_t x2 = get_x2_field(op);
  uint8_t v = read_x_value(r, m, x2);
  v = ((v >> 1) & 0x7F) | (v << 7);
  write_x_value(r, m, x2, v);
  r->f = make_flags_reg(v == 0, 0, 0, (v & 0x80) == 0x80);
}

void run_op_rl(struct regs* r, struct memory* m, uint8_t op) {
  uint8_t x2 = get_x2_field(op);
  uint8_t v = read_x_value(r, m, x2);
  int new_carry = v >> 7;
  v = (v << 1) | get_flag_value(r, FLAG_C);
  write_x_value(r, m, x2, v);
  r->f = make_flags_reg(v == 0, 0, 0, new_carry);
}

void run_op_rr(struct regs* r, struct memory* m, uint8_t op) {
  uint8_t x2 = get_x2_field(op);
  uint8_t v = read_x_value(r, m, x2);
  int new_carry = (v & 1);
  v = (v >> 1) | (get_flag_value(r, FLAG_C) << 7);
  write_x_value(r, m, x2, v);
  r->f = make_flags_reg(v == 0, 0, 0, new_carry);
}

void run_op_sla(struct regs* r, struct memory* m, uint8_t op) {
  uint8_t x2 = get_x2_field(op);
  uint8_t v = read_x_value(r, m, x2);
  int new_carry = (v & 0x80) == 0x80;
  v <<= 1;
  write_x_value(r, m, x2, v);
  r->f = make_flags_reg(v == 0, 0, 0, new_carry);
}

void run_op_sra(struct regs* r, struct memory* m, uint8_t op) {
  uint8_t x2 = get_x2_field(op);
  uint8_t v = read_x_value(r, m, x2);
  int new_carry = v & 1;
  v = (v >> 1) | (v & 0x80);
  write_x_value(r, m, x2, v);
  r->f = make_flags_reg(v == 0, 0, 0, new_carry);
}

void run_op_swap(struct regs* r, struct memory* m, uint8_t op) {
  uint8_t x2 = get_x2_field(op);
  uint8_t v = read_x_value(r, m, x2);
  v = ((v >> 4) & 0x0F) | ((v << 4) & 0xF0);
  write_x_value(r, m, x2, v);
  r->f = make_flags_reg(v == 0, 0, 0, 0);
}

void run_op_srl(struct regs* r, struct memory* m, uint8_t op) {
  uint8_t x2 = get_x2_field(op);
  uint8_t v = read_x_value(r, m, x2);
  int new_c = v & 1;
  v = (v >> 1) & 0x7F;
  write_x_value(r, m, x2, v);
  r->f = make_flags_reg(v == 0, 0, 0, new_c);
}

void run_op_bit(struct regs* r, struct memory* m, uint8_t op) {
  register int bit = (read_x_value(r, m, get_x2_field(op)) >> get_bit_field(op)) & 1;
  r->f = make_flags_reg(!bit, 0, 1, get_flag_value(r, FLAG_C));
}

void run_op_res(struct regs* r, struct memory* m, uint8_t op) {
  int x2 = get_x2_field(op);
  write_x_value(r, m, x2, read_x_value(r, m, x2) & ~(1 << get_bit_field(op)));
}

void run_op_set(struct regs* r, struct memory* m, uint8_t op) {
  int x2 = get_x2_field(op);
  write_x_value(r, m, x2, read_x_value(r, m, x2) | (1 << get_bit_field(op)));
}



///////////////////////////////////////////////////////////////////////////////
// opcode dispatchers

typedef struct {
  int op_num;
  const char* name;
  int size;
  int min_cycles;
  int max_cycles;
  int arg1_type;
  int arg2_type;
  void (*run)(struct regs* r, struct memory* m, uint8_t op);
} opcode_def;

#define ARG_NONE 0
#define ARG_R8   1
#define ARG_FLAG 2
#define ARG_R    3
#define ARG_D8   4
#define ARG_D16  5
#define ARG_E    6
#define ARG_X    7
#define ARG_A8IO 8
#define ARG_A16  9
#define ARG_X2   10
#define ARG_Z    11
#define ARG_BIT  12
#define ARG_A    13
#define ARG_S    14
#define ARG_A16M 15
#define ARG_CIO  16
#define ARG_SPR8 17
#define ARG_SP   18
#define ARG_HLM  19
#define ARG_HL   20
#define ARG_A16S 21

opcode_def opcodes[0x100] = {
  {0x00, "nop",              1,  4,  4, ARG_NONE,  ARG_NONE, run_op_nop},
  {0x01, "ld",               3, 12, 12, ARG_R,     ARG_D16,  run_op_ld_r_d16},
  {0x02, "ld",               1,  8,  8, ARG_E,     ARG_A,    run_op_ld_e_a},
  {0x03, "inc",              1,  8,  8, ARG_R,     ARG_NONE, run_op_inc_r},
  {0x04, "inc",              1,  4,  4, ARG_X,     ARG_NONE, run_op_inc_x},
  {0x05, "dec",              1,  4,  4, ARG_X,     ARG_NONE, run_op_dec_x},
  {0x06, "ld",               2,  8,  8, ARG_X,     ARG_D8,   run_op_ld_x_d8},
  {0x07, "rlca",             1,  4,  4, ARG_NONE,  ARG_NONE, run_op_rlca},
  {0x08, "ld",               3, 20, 20, ARG_A16M,  ARG_SP,   run_op_ld_a16_sp},
  {0x09, "add",              1,  8,  8, ARG_HL,    ARG_R,    run_op_add_hl_r},
  {0x0A, "ld",               1,  8,  8, ARG_A,     ARG_E,    run_op_ld_a_e},
  {0x0B, "dec",              1,  8,  8, ARG_R,     ARG_NONE, run_op_dec_r},
  {0x0C, "inc",              1,  4,  4, ARG_X,     ARG_NONE, run_op_inc_x},
  {0x0D, "dec",              1,  4,  4, ARG_X,     ARG_NONE, run_op_dec_x},
  {0x0E, "ld",               2,  8,  8, ARG_X,     ARG_D8,   run_op_ld_x_d8},
  {0x0F, "rrca",             1,  4,  4, ARG_NONE,  ARG_NONE, run_op_rrca},

  {0x10, "stop",             2,  4,  4, ARG_NONE,  ARG_NONE, run_op_stop},
  {0x11, "ld",               3, 12, 12, ARG_R,     ARG_D16,  run_op_ld_r_d16},
  {0x12, "ld",               1,  8,  8, ARG_E,     ARG_A,    run_op_ld_e_a},
  {0x13, "inc",              1,  8,  8, ARG_R,     ARG_NONE, run_op_inc_r},
  {0x14, "inc",              1,  4,  4, ARG_X,     ARG_NONE, run_op_inc_x},
  {0x15, "dec",              1,  4,  4, ARG_X,     ARG_NONE, run_op_dec_x},
  {0x16, "ld",               2,  8,  8, ARG_X,     ARG_D8,   run_op_ld_x_d8},
  {0x17, "rla",              1,  4,  4, ARG_NONE,  ARG_NONE, run_op_rla},
  {0x18, "jr",               2, 12, 12, ARG_R8,    ARG_NONE, run_op_jr_r8},
  {0x19, "add",              1,  8,  8, ARG_R,     ARG_NONE, run_op_add_hl_r},
  {0x1A, "ld",               1,  8,  8, ARG_A,     ARG_E,    run_op_ld_a_e},
  {0x1B, "dec",              1,  8,  8, ARG_R,     ARG_NONE, run_op_dec_r},
  {0x1C, "inc",              1,  4,  4, ARG_X,     ARG_NONE, run_op_inc_x},
  {0x1D, "dec",              1,  4,  4, ARG_X,     ARG_NONE, run_op_dec_x},
  {0x1E, "ld",               2,  8,  8, ARG_X,     ARG_D8,   run_op_ld_x_d8},
  {0x1F, "rra",              1,  4,  4, ARG_NONE,  ARG_NONE, run_op_rra},

  {0x20, "jr",               2,  8, 12, ARG_FLAG,  ARG_R8,   run_op_jr_flag_r8},
  {0x21, "ld",               3, 12, 12, ARG_R,     ARG_D16,  run_op_ld_r_d16},
  {0x22, "ld",               1,  8,  8, ARG_E,     ARG_A,    run_op_ld_e_a},
  {0x23, "inc",              1,  8,  8, ARG_R,     ARG_NONE, run_op_inc_r},
  {0x24, "inc",              1,  4,  4, ARG_X,     ARG_NONE, run_op_inc_x},
  {0x25, "dec",              1,  4,  4, ARG_X,     ARG_NONE, run_op_dec_x},
  {0x26, "ld",               2,  8,  8, ARG_X,     ARG_D8,   run_op_ld_x_d8},
  {0x27, "daa",              1,  4,  4, ARG_NONE,  ARG_NONE, run_op_daa},
  {0x28, "jr",               2,  8, 12, ARG_FLAG,  ARG_R8,   run_op_jr_flag_r8},
  {0x29, "add",              1,  8,  8, ARG_R,     ARG_NONE, run_op_add_hl_r},
  {0x2A, "ld",               1,  8,  8, ARG_A,     ARG_E,    run_op_ld_a_e},
  {0x2B, "dec",              1,  8,  8, ARG_R,     ARG_NONE, run_op_dec_r},
  {0x2C, "inc",              1,  4,  4, ARG_X,     ARG_NONE, run_op_inc_x},
  {0x2D, "dec",              1,  4,  4, ARG_X,     ARG_NONE, run_op_dec_x},
  {0x2E, "ld",               2,  8,  8, ARG_X,     ARG_D8,   run_op_ld_x_d8},
  {0x2F, "cpl",              1,  4,  4, ARG_NONE,  ARG_NONE, run_op_cpl},

  {0x30, "jr",               2,  8, 12, ARG_FLAG,  ARG_R8,   run_op_jr_flag_r8},
  {0x31, "ld",               3, 12, 12, ARG_R,     ARG_D16,  run_op_ld_r_d16},
  {0x32, "ld",               1,  8,  8, ARG_E,     ARG_A,    run_op_ld_e_a},
  {0x33, "inc",              1,  8,  8, ARG_R,     ARG_NONE, run_op_inc_r},
  {0x34, "inc",              1, 12, 12, ARG_X,     ARG_NONE, run_op_inc_x},
  {0x35, "dec",              1, 12, 12, ARG_X,     ARG_NONE, run_op_dec_x},
  {0x36, "ld",               2, 12, 12, ARG_X,     ARG_D8,   run_op_ld_x_d8},
  {0x37, "scf",              1,  4,  4, ARG_NONE,  ARG_NONE, run_op_scf},
  {0x38, "jr",               2,  8, 12, ARG_FLAG,  ARG_R8,   run_op_jr_flag_r8},
  {0x39, "add",              1,  8,  8, ARG_HL,    ARG_R,    run_op_add_hl_r},
  {0x3A, "ld",               1,  8,  8, ARG_A,     ARG_E,    run_op_ld_a_e},
  {0x3B, "dec",              1,  8,  8, ARG_R,     ARG_NONE, run_op_dec_r},
  {0x3C, "inc",              1,  4,  4, ARG_X,     ARG_NONE, run_op_inc_x},
  {0x3D, "dec",              1,  4,  4, ARG_X,     ARG_NONE, run_op_dec_x},
  {0x3E, "ld",               2,  8,  8, ARG_X,     ARG_D8,   run_op_ld_x_d8},
  {0x3F, "ccf",              1,  4,  4, ARG_NONE,  ARG_NONE, run_op_ccf},

  {0x40, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x41, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x42, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x43, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x44, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x45, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x46, "ld",               1,  8,  8, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x47, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x48, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x49, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x4A, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x4B, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x4C, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x4D, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x4E, "ld",               1,  8,  8, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x4F, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},

  {0x50, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x51, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x52, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x53, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x54, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x55, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x56, "ld",               1,  8,  8, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x57, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x58, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x59, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x5A, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x5B, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x5C, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x5D, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x5E, "ld",               1,  8,  8, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x5F, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},

  {0x60, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x61, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x62, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x63, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x64, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x65, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x66, "ld",               1,  8,  8, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x67, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x68, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x69, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x6A, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x6B, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x6C, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x6D, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x6E, "ld",               1,  8,  8, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x6F, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},

  {0x70, "ld",               1,  8,  8, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x71, "ld",               1,  8,  8, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x72, "ld",               1,  8,  8, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x73, "ld",               1,  8,  8, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x74, "ld",               1,  8,  8, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x75, "ld",               1,  8,  8, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x76, "halt",             1,  4,  4, ARG_NONE,  ARG_NONE, run_op_halt},
  {0x77, "ld",               1,  8,  8, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x78, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x79, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x7A, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x7B, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x7C, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x7D, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x7E, "ld",               1,  8,  8, ARG_X,     ARG_X2,   run_op_ld_x_x},
  {0x7F, "ld",               1,  4,  4, ARG_X,     ARG_X2,   run_op_ld_x_x},

  {0x80, "add",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_add_a_x},
  {0x81, "add",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_add_a_x},
  {0x82, "add",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_add_a_x},
  {0x83, "add",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_add_a_x},
  {0x84, "add",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_add_a_x},
  {0x85, "add",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_add_a_x},
  {0x86, "add",              1,  8,  8, ARG_X2,    ARG_NONE, run_op_add_a_x},
  {0x87, "add",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_add_a_x},
  {0x88, "adc",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_adc_a_x},
  {0x89, "adc",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_adc_a_x},
  {0x8A, "adc",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_adc_a_x},
  {0x8B, "adc",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_adc_a_x},
  {0x8C, "adc",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_adc_a_x},
  {0x8D, "adc",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_adc_a_x},
  {0x8E, "adc",              1,  8,  8, ARG_X2,    ARG_NONE, run_op_adc_a_x},
  {0x8F, "adc",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_adc_a_x},

  {0x90, "sub",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_sub_a_x},
  {0x91, "sub",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_sub_a_x},
  {0x92, "sub",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_sub_a_x},
  {0x93, "sub",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_sub_a_x},
  {0x94, "sub",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_sub_a_x},
  {0x95, "sub",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_sub_a_x},
  {0x96, "sub",              1,  8,  8, ARG_X2,    ARG_NONE, run_op_sub_a_x},
  {0x97, "sub",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_sub_a_x},
  {0x98, "subc",             1,  4,  4, ARG_X2,    ARG_NONE, run_op_subc_a_x},
  {0x99, "subc",             1,  4,  4, ARG_X2,    ARG_NONE, run_op_subc_a_x},
  {0x9A, "subc",             1,  4,  4, ARG_X2,    ARG_NONE, run_op_subc_a_x},
  {0x9B, "subc",             1,  4,  4, ARG_X2,    ARG_NONE, run_op_subc_a_x},
  {0x9C, "subc",             1,  4,  4, ARG_X2,    ARG_NONE, run_op_subc_a_x},
  {0x9D, "subc",             1,  4,  4, ARG_X2,    ARG_NONE, run_op_subc_a_x},
  {0x9E, "subc",             1,  8,  8, ARG_X2,    ARG_NONE, run_op_subc_a_x},
  {0x9F, "subc",             1,  4,  4, ARG_X2,    ARG_NONE, run_op_subc_a_x},

  {0xA0, "and",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_and_a_x},
  {0xA1, "and",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_and_a_x},
  {0xA2, "and",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_and_a_x},
  {0xA3, "and",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_and_a_x},
  {0xA4, "and",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_and_a_x},
  {0xA5, "and",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_and_a_x},
  {0xA6, "and",              1,  8,  8, ARG_X2,    ARG_NONE, run_op_and_a_x},
  {0xA7, "and",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_and_a_x},
  {0xA8, "xor",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_xor_a_x},
  {0xA9, "xor",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_xor_a_x},
  {0xAA, "xor",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_xor_a_x},
  {0xAB, "xor",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_xor_a_x},
  {0xAC, "xor",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_xor_a_x},
  {0xAD, "xor",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_xor_a_x},
  {0xAE, "xor",              1,  8,  8, ARG_X2,    ARG_NONE, run_op_xor_a_x},
  {0xAF, "xor",              1,  4,  4, ARG_X2,    ARG_NONE, run_op_xor_a_x},

  {0xB0, "or",               1,  4,  4, ARG_X2,    ARG_NONE, run_op_or_a_x},
  {0xB1, "or",               1,  4,  4, ARG_X2,    ARG_NONE, run_op_or_a_x},
  {0xB2, "or",               1,  4,  4, ARG_X2,    ARG_NONE, run_op_or_a_x},
  {0xB3, "or",               1,  4,  4, ARG_X2,    ARG_NONE, run_op_or_a_x},
  {0xB4, "or",               1,  4,  4, ARG_X2,    ARG_NONE, run_op_or_a_x},
  {0xB5, "or",               1,  4,  4, ARG_X2,    ARG_NONE, run_op_or_a_x},
  {0xB6, "or",               1,  8,  8, ARG_X2,    ARG_NONE, run_op_or_a_x},
  {0xB7, "or",               1,  4,  4, ARG_X2,    ARG_NONE, run_op_or_a_x},
  {0xB8, "cp",               1,  4,  4, ARG_X2,    ARG_NONE, run_op_cp_a_x},
  {0xB9, "cp",               1,  4,  4, ARG_X2,    ARG_NONE, run_op_cp_a_x},
  {0xBA, "cp",               1,  4,  4, ARG_X2,    ARG_NONE, run_op_cp_a_x},
  {0xBB, "cp",               1,  4,  4, ARG_X2,    ARG_NONE, run_op_cp_a_x},
  {0xBC, "cp",               1,  4,  4, ARG_X2,    ARG_NONE, run_op_cp_a_x},
  {0xBD, "cp",               1,  4,  4, ARG_X2,    ARG_NONE, run_op_cp_a_x},
  {0xBE, "cp",               1,  8,  8, ARG_X2,    ARG_NONE, run_op_cp_a_x},
  {0xBF, "cp",               1,  4,  4, ARG_X2,    ARG_NONE, run_op_cp_a_x},

  {0xC0, "ret",              1,  8, 20, ARG_FLAG,  ARG_NONE, run_op_ret_y},
  {0xC1, "pop",              1, 12, 12, ARG_S,     ARG_NONE, run_op_pop_r},
  {0xC2, "jp",               3, 12, 16, ARG_FLAG,  ARG_A16,  run_op_jp_y_a16},
  {0xC3, "jp",               3, 16, 16, ARG_A16,   ARG_NONE, run_op_jp_a16},
  {0xC4, "call",             3, 12, 24, ARG_FLAG,  ARG_A16,  run_op_call_y_a16},
  {0xC5, "push",             1, 16, 16, ARG_S,     ARG_NONE, run_op_push_r},
  {0xC6, "add",              2,  8,  8, ARG_D8,    ARG_NONE, run_op_add_a_d8},
  {0xC7, "rst",              1, 16, 16, ARG_Z,     ARG_NONE, run_op_rst},
  {0xC8, "ret",              1,  8, 20, ARG_FLAG,  ARG_NONE, run_op_ret_y},
  {0xC9, "ret",              1, 16, 16, ARG_NONE,  ARG_NONE, run_op_ret},
  {0xCA, "jp",               3, 12, 16, ARG_FLAG,  ARG_A16,  run_op_jp_y_a16},
  {0xCB, NULL,               2,  0,  0, ARG_NONE,  ARG_NONE, NULL}, // special-cased in run_cycle
  {0xCC, "call",             3, 12, 24, ARG_FLAG,  ARG_A16,  run_op_call_y_a16},
  {0xCD, "call",             3, 24, 24, ARG_A16,   ARG_NONE, run_op_call_a16},
  {0xCE, "adc",              2,  8,  8, ARG_D8,    ARG_NONE, run_op_adc_a_d8},
  {0xCF, "rst",              1, 16, 16, ARG_Z,     ARG_NONE, run_op_rst},

  {0xD0, "ret",              1,  8, 20, ARG_FLAG,  ARG_NONE, run_op_ret_y},
  {0xD1, "pop",              1, 12, 12, ARG_S,     ARG_NONE, run_op_pop_r},
  {0xD2, "jp",               3, 12, 16, ARG_FLAG,  ARG_A16,  run_op_jp_y_a16},
  {0xD3, NULL,               1,  0,  0, ARG_NONE,  ARG_NONE, NULL},
  {0xD4, "call",             3, 12, 24, ARG_FLAG,  ARG_A16,  run_op_call_y_a16},
  {0xD5, "push",             1, 16, 16, ARG_S,     ARG_NONE, run_op_push_r},
  {0xD6, "sub",              2,  8,  8, ARG_D8,    ARG_NONE, run_op_sub_a_d8},
  {0xD7, "rst",              1, 16, 16, ARG_Z,     ARG_NONE, run_op_rst},
  {0xD8, "ret",              1,  8, 20, ARG_FLAG,  ARG_NONE, run_op_ret_y},
  {0xD9, "reti",             1, 16, 16, ARG_NONE,  ARG_NONE, run_op_reti},
  {0xDA, "jp",               3, 12, 16, ARG_FLAG,  ARG_A16,  run_op_jp_y_a16},
  {0xDB, NULL,               1,  0,  0, ARG_NONE,  ARG_NONE, NULL},
  {0xDC, "call",             3, 12, 24, ARG_FLAG,  ARG_A16,  run_op_call_y_a16},
  {0xDD, NULL,               1,  0,  0, ARG_NONE,  ARG_NONE, NULL},
  {0xDE, "subc",             2,  8,  8, ARG_D8,    ARG_NONE, run_op_subc_a_d8},
  {0xDF, "rst",              1, 16, 16, ARG_Z,     ARG_NONE, run_op_rst},

  {0xE0, "ldh",              2, 12, 12, ARG_A8IO,  ARG_A,    run_op_ldh_ff00_a8_a},
  {0xE1, "pop",              1, 12, 12, ARG_S,     ARG_NONE, run_op_pop_r},
  {0xE2, "ld",               2,  8,  8, ARG_CIO,   ARG_A,    run_op_ld_ff00_c_a},
  {0xE3, NULL,               1,  0,  0, ARG_NONE,  ARG_NONE, NULL},
  {0xE4, NULL,               1,  0,  0, ARG_NONE,  ARG_NONE, NULL},
  {0xE5, "push",             1, 16, 16, ARG_S,     ARG_NONE, run_op_push_r},
  {0xE6, "and",              2,  8,  8, ARG_D8,    ARG_NONE, run_op_and_a_d8},
  {0xE7, "rst",              1, 16, 16, ARG_Z,     ARG_NONE, run_op_rst},
  {0xE8, "add",              2, 16, 16, ARG_SP,    ARG_R8,   run_op_add_sp_r8},
  {0xE9, "jp",               1,  4,  4, ARG_HLM,   ARG_NONE, run_op_jp_hl},
  {0xEA, "ld",               3, 16, 16, ARG_A16S,  ARG_A,    run_op_ld_a16_a},
  {0xEB, NULL,               1,  0,  0, ARG_NONE,  ARG_NONE, NULL},
  {0xEC, NULL,               1,  0,  0, ARG_NONE,  ARG_NONE, NULL},
  {0xED, NULL,               1,  0,  0, ARG_NONE,  ARG_NONE, NULL},
  {0xEE, "xor",              2,  8,  8, ARG_D8,    ARG_NONE, run_op_xor_a_d8},
  {0xEF, "rst",              1, 16, 16, ARG_Z,     ARG_NONE, run_op_rst},

  {0xF0, "ldh",              2, 12, 12, ARG_A,     ARG_A8IO, run_op_ldh_a_ff00_a8},
  {0xF1, "pop",              1, 12, 12, ARG_S,     ARG_NONE, run_op_pop_r},
  {0xF2, "ld",               2,  8,  8, ARG_A,     ARG_CIO,  run_op_ld_a_ff00_c},
  {0xF3, "di",               1,  4,  4, ARG_NONE,  ARG_NONE, run_op_di},
  {0xF4, NULL,               1,  0,  0, ARG_NONE,  ARG_NONE, NULL},
  {0xF5, "push",             1, 16, 16, ARG_S,     ARG_NONE, run_op_push_r},
  {0xF6, "or",               2,  8,  8, ARG_D8,    ARG_NONE, run_op_or_a_d8},
  {0xF7, "rst",              1, 16, 16, ARG_Z,     ARG_NONE, run_op_rst},
  {0xF8, "ld",               2, 12, 12, ARG_HL,    ARG_SPR8, run_op_ld_hl_sp_r8},
  {0xF9, "ld",               1,  8,  8, ARG_SP,    ARG_HL,   run_op_ld_sp_hl},
  {0xFA, "ld",               2,  8, 16, ARG_A,     ARG_A16S, run_op_ld_a_a16},
  {0xFB, "ei",               1,  4,  4, ARG_NONE,  ARG_NONE, run_op_ei},
  {0xFC, NULL,               1,  0,  0, ARG_NONE,  ARG_NONE, NULL},
  {0xFD, NULL,               1,  0,  0, ARG_NONE,  ARG_NONE, NULL},
  {0xFE, "cp",               2,  8,  8, ARG_D8,    ARG_NONE, run_op_cp_a_d8},
  {0xFF, "rst",              1, 16, 16, ARG_Z,     ARG_NONE, run_op_rst},
};

opcode_def cb_opcodes[0x100] = {

  {0x00, "rlc",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_rlc},
  {0x01, "rlc",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_rlc},
  {0x02, "rlc",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_rlc},
  {0x03, "rlc",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_rlc},
  {0x04, "rlc",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_rlc},
  {0x05, "rlc",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_rlc},
  {0x06, "rlc",              2, 16, 16, ARG_X2,    ARG_NONE, run_op_rlc},
  {0x07, "rlc",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_rlc},
  {0x08, "rrc",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_rrc},
  {0x09, "rrc",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_rrc},
  {0x0A, "rrc",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_rrc},
  {0x0B, "rrc",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_rrc},
  {0x0C, "rrc",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_rrc},
  {0x0D, "rrc",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_rrc},
  {0x0E, "rrc",              2, 16, 16, ARG_X2,    ARG_NONE, run_op_rrc},
  {0x0F, "rrc",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_rrc},

  {0x10, "rl",               2,  8,  8, ARG_X2,    ARG_NONE, run_op_rl},
  {0x11, "rl",               2,  8,  8, ARG_X2,    ARG_NONE, run_op_rl},
  {0x12, "rl",               2,  8,  8, ARG_X2,    ARG_NONE, run_op_rl},
  {0x13, "rl",               2,  8,  8, ARG_X2,    ARG_NONE, run_op_rl},
  {0x14, "rl",               2,  8,  8, ARG_X2,    ARG_NONE, run_op_rl},
  {0x15, "rl",               2,  8,  8, ARG_X2,    ARG_NONE, run_op_rl},
  {0x16, "rl",               2, 16, 16, ARG_X2,    ARG_NONE, run_op_rl},
  {0x17, "rl",               2,  8,  8, ARG_X2,    ARG_NONE, run_op_rl},
  {0x18, "rr",               2,  8,  8, ARG_X2,    ARG_NONE, run_op_rr},
  {0x19, "rr",               2,  8,  8, ARG_X2,    ARG_NONE, run_op_rr},
  {0x1A, "rr",               2,  8,  8, ARG_X2,    ARG_NONE, run_op_rr},
  {0x1B, "rr",               2,  8,  8, ARG_X2,    ARG_NONE, run_op_rr},
  {0x1C, "rr",               2,  8,  8, ARG_X2,    ARG_NONE, run_op_rr},
  {0x1D, "rr",               2,  8,  8, ARG_X2,    ARG_NONE, run_op_rr},
  {0x1E, "rr",               2, 16, 16, ARG_X2,    ARG_NONE, run_op_rr},
  {0x1F, "rr",               2,  8,  8, ARG_X2,    ARG_NONE, run_op_rr},

  {0x20, "sla",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_sla},
  {0x21, "sla",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_sla},
  {0x22, "sla",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_sla},
  {0x23, "sla",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_sla},
  {0x24, "sla",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_sla},
  {0x25, "sla",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_sla},
  {0x26, "sla",              2, 16, 16, ARG_X2,    ARG_NONE, run_op_sla},
  {0x27, "sla",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_sla},
  {0x28, "sra",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_sra},
  {0x29, "sra",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_sra},
  {0x2A, "sra",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_sra},
  {0x2B, "sra",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_sra},
  {0x2C, "sra",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_sra},
  {0x2D, "sra",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_sra},
  {0x2E, "sra",              2, 16, 16, ARG_X2,    ARG_NONE, run_op_sra},
  {0x2F, "sra",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_sra},

  {0x30, "swap",             2,  8,  8, ARG_X2,    ARG_NONE, run_op_swap},
  {0x31, "swap",             2,  8,  8, ARG_X2,    ARG_NONE, run_op_swap},
  {0x32, "swap",             2,  8,  8, ARG_X2,    ARG_NONE, run_op_swap},
  {0x33, "swap",             2,  8,  8, ARG_X2,    ARG_NONE, run_op_swap},
  {0x34, "swap",             2,  8,  8, ARG_X2,    ARG_NONE, run_op_swap},
  {0x35, "swap",             2,  8,  8, ARG_X2,    ARG_NONE, run_op_swap},
  {0x36, "swap",             2, 16, 16, ARG_X2,    ARG_NONE, run_op_swap},
  {0x37, "swap",             2,  8,  8, ARG_X2,    ARG_NONE, run_op_swap},
  {0x38, "srl",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_srl},
  {0x39, "srl",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_srl},
  {0x3A, "srl",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_srl},
  {0x3B, "srl",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_srl},
  {0x3C, "srl",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_srl},
  {0x3D, "srl",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_srl},
  {0x3E, "srl",              2, 16, 16, ARG_X2,    ARG_NONE, run_op_srl},
  {0x3F, "srl",              2,  8,  8, ARG_X2,    ARG_NONE, run_op_srl},

  {0x40, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x41, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x42, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x43, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x44, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x45, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x46, "bit",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x47, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x48, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x49, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x4A, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x4B, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x4C, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x4D, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x4E, "bit",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x4F, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x50, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x51, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x52, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x53, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x54, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x55, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x56, "bit",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x57, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x58, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x59, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x5A, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x5B, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x5C, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x5D, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x5E, "bit",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x5F, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x60, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x61, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x62, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x63, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x64, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x65, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x66, "bit",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x67, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x68, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x69, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x6A, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x6B, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x6C, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x6D, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x6E, "bit",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x6F, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x70, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x71, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x72, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x73, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x74, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x75, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x76, "bit",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x77, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x78, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x79, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x7A, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x7B, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x7C, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x7D, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x7E, "bit",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_bit},
  {0x7F, "bit",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_bit},

  {0x80, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x81, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x82, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x83, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x84, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x85, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x86, "res",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_res},
  {0x87, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x88, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x89, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x8A, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x8B, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x8C, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x8D, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x8E, "res",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_res},
  {0x8F, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x90, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x91, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x92, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x93, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x94, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x95, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x96, "res",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_res},
  {0x97, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x98, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x99, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x9A, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x9B, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x9C, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x9D, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0x9E, "res",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_res},
  {0x9F, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xA0, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xA1, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xA2, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xA3, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xA4, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xA5, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xA6, "res",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_res},
  {0xA7, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xA8, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xA9, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xAA, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xAB, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xAC, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xAD, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xAE, "res",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_res},
  {0xAF, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xB0, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xB1, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xB2, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xB3, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xB4, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xB5, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xB6, "res",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_res},
  {0xB7, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xB8, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xB9, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xBA, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xBB, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xBC, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xBD, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},
  {0xBE, "res",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_res},
  {0xBF, "res",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_res},

  {0xC0, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xC1, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xC2, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xC3, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xC4, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xC5, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xC6, "set",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_set},
  {0xC7, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xC8, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xC9, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xCA, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xCB, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xCC, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xCD, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xCE, "set",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_set},
  {0xCF, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xD0, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xD1, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xD2, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xD3, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xD4, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xD5, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xD6, "set",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_set},
  {0xD7, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xD8, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xD9, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xDA, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xDB, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xDC, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xDD, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xDE, "set",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_set},
  {0xDF, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xE0, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xE1, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xE2, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xE3, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xE4, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xE5, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xE6, "set",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_set},
  {0xE7, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xE8, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xE9, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xEA, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xEB, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xEC, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xED, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xEE, "set",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_set},
  {0xEF, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xF0, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xF1, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xF2, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xF3, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xF4, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xF5, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xF6, "set",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_set},
  {0xF7, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xF8, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xF9, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xFA, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xFB, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xFC, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xFD, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
  {0xFE, "set",              2, 16, 16, ARG_X2,    ARG_BIT,  run_op_set},
  {0xFF, "set",              2,  8,  8, ARG_X2,    ARG_BIT,  run_op_set},
};

int run_cycle(struct regs* r, const struct regs* prev, struct memory* m) {

  // check for debug interrupt
  if (r->debug_interrupt_reason) {
    debug_main(r, m);
    r->debug_interrupt_reason = NULL;
  }

  // check for interrupts
  uint8_t irq = r->interrupt_enable & r->interrupt_flag;
  if ((r->ime || r->wait_for_interrupt) && irq) {
    int x;
    for (x = 0; x <= INTERRUPT_MAX; x++) {
      if (irq & (1 << x)) {
        if (r->wait_for_interrupt) {
          r->wait_for_interrupt = 0;
          // TODO: may need some cycles adjustment here
        }
        if (r->ime) {
          // execute interrupt
          printf("> interrupt! %d\n", x);
          r->ime = 0;
          r->interrupt_flag &= ~(1 << x);
          stack_push(r, m, r->pc);
          r->pc = 0x40 + 8 * x;
          break;
        }
      }
    }
  }

  if (r->stop || r->wait_for_interrupt) {
    r->cycles += 4;
    update_devices(m, r->cycles);
    return 0;
  }

  uint8_t op = ifetch(r, m);

  opcode_def* table = opcodes;
  if (op == 0xCB) {
    op = ifetch(r, m);
    table = cb_opcodes;
  }

  if (table[op].run == NULL)
    return -1;
  table[op].run(r, m, op);
  r->cycles += table[op].min_cycles;

  update_devices(m, r->cycles);

  if (r->debug)
    print_regs(r, prev, m);

  return 0;
}

int is_double_speed_mode(struct regs* r) {
  return !!(r->speed_switch & 0x80);
}

void signal_interrupt(struct regs* r, int int_id, int signal) {
  int mask = (1 << int_id);
  if (signal)
    r->interrupt_flag |= mask;
  else
    r->interrupt_flag &= ~mask;
}

void signal_debug_interrupt(struct regs* r, const char* reason) {
  r->debug_interrupt_reason = reason;
}

inline uint8_t read_interrupt_flag(struct regs* r, uint8_t addr) {
  return r->interrupt_flag;
}

inline void write_interrupt_flag(struct regs* r, uint8_t addr, uint8_t value) {
  r->interrupt_flag = value;
}

inline uint8_t read_interrupt_enable(struct regs* r, uint8_t addr) {
  return r->interrupt_enable;
}

inline void write_interrupt_enable(struct regs* r, uint8_t addr, uint8_t value) {
  r->interrupt_enable = value;
}

uint8_t read_speed_switch(struct regs* r, uint8_t addr) {
  return r->speed_switch;
}

void write_speed_switch(struct regs* r, uint8_t addr, uint8_t value) {
  r->speed_switch = (r->speed_switch & 0x80) | (value & 0x01);
}

struct regs* create_cpu() {
  struct regs* r = (struct regs*)malloc(sizeof(struct regs));
  if (!r)
    return NULL;
  r->af = 0x01B0;
  r->bc = 0x0013;
  r->de = 0x00D8;
  r->hl = 0x014D;
  r->sp = 0xFFFE;
  r->pc = 0x0100;
  r->ime = 1;
  r->cycles = 0;
  r->wait_for_interrupt = 0;
  r->stop = 0;
  return r;
}

void delete_cpu(struct regs* r) {
  free(r);
}

const char* r_field_names[] = {"bc", "de", "hl", "sp"};
const char* x_field_names[] = {"b", "c", "d", "e", "h", "l", "(hl)", "a"};
const char* e_field_names[] = {"(bc)", "(de)", "(hl+)", "(hl-)"};
const char* flag_field_names[] = {"nz", "z", "nc", "c"};
const char* s_field_names[] = {"bc", "de", "hl", "af"};

static int print_opcode(FILE* f, const uint8_t* opcode_data, struct memory* m) {

  if (!f)
    f = stdout;

  uint32_t opcode_offset = 0;
  uint8_t op = opcode_data[opcode_offset++];
  opcode_def* table = opcodes;
  if (op == 0xCB) {
    op = opcode_data[opcode_offset++];
    table = cb_opcodes;
  }
  opcode_def* def = &table[op];

  fprintf(f, "%s", def->name);
  int x;
  uint8_t off;
  uint16_t addr;
  for (x = 0; x < 2; x++) {
    switch ((&def->arg1_type)[x]) {
      case ARG_NONE:
        break;
      case ARG_R8:
        off = opcode_data[opcode_offset++];
        fprintf(f, " %c%02X", (off & 0x80) ? '-' : '+', (off & 0x80) ? (-off & 0xFF) : off);
        break;
      case ARG_FLAG:
        fprintf(f, " %s", flag_field_names[get_flag_field(op)]);
        break;
      case ARG_R:
        fprintf(f, " %s", r_field_names[get_r_field(op)]);
        break;
      case ARG_D8:
        fprintf(f, " %02X", opcode_data[opcode_offset++]);
        break;
      case ARG_D16:
        fprintf(f, " %04X", *(uint16_t*)&opcode_data[opcode_offset]);
        opcode_offset += 2;
        break;
      case ARG_E:
        fprintf(f, " %s", e_field_names[get_e_field(op)]);
        break;
      case ARG_X:
        fprintf(f, " %s", x_field_names[get_x_field(op)]);
        break;
      case ARG_A8IO:
        fprintf(f, " (FF%02X)", opcode_data[opcode_offset]);
        if (m)
          fprintf(f, " [%02X]", read8(m, 0xFF00 | opcode_data[opcode_offset]));
        opcode_offset++;
        break;
      case ARG_A16:
        fprintf(f, " %04X", *(uint16_t*)&opcode_data[opcode_offset]);
        opcode_offset += 2;
        break;
      case ARG_A16M:
        addr = *(uint16_t*)&opcode_data[opcode_offset];
        fprintf(f, " (%04X)", addr);
        if (m)
          fprintf(f, " [%02X %02X]", read8(m, addr), read8(m, addr + 1));
        opcode_offset += 2;
        break;
      case ARG_A16S:
        addr = *(uint16_t*)&opcode_data[opcode_offset];
        fprintf(f, " (%04X)", addr);
        if (m)
          fprintf(f, " [%02X]", read8(m, addr));
        opcode_offset += 2;
        break;
      case ARG_X2:
        fprintf(f, " %s", x_field_names[get_x2_field(op)]);
        break;
      case ARG_Z:
        fprintf(f, " %02X", get_z_field(op) << 3);
        break;
      case ARG_BIT:
        fprintf(f, " %d", get_bit_field(op));
        break;
      case ARG_A:
        fprintf(f, " a");
        break;
      case ARG_S:
        fprintf(f, " %s", s_field_names[get_r_field(op)]);
        break;
      case ARG_CIO:
        fprintf(f, " (FF00+c)");
        break;
      case ARG_SPR8:
        off = opcode_data[opcode_offset++];
        fprintf(f, " sp%c%02X", (off & 0x80) ? '-' : '+', (off & 0x80) ? (-off & 0xFF) : off);
        break;
      case ARG_SP:
        fprintf(f, " sp");
        break;
      case ARG_HLM:
        fprintf(f, " (hl)");
        break;
      case ARG_HL:
        fprintf(f, " hl");
        break;
      default:
        fprintf(f, " unknown_argtype");
    }
  }
  return opcode_offset;
}

#define setup_color_if_true(cond) \
  do { \
    if (cond) \
      change_color(FORMAT_FG_MAGENTA, FORMAT_BOLD, FORMAT_END); \
    else \
      change_color(FORMAT_NORMAL, FORMAT_END); \
  } while (0)

#define print_reg_value(regname, format) \
  do { \
    setup_color_if_true(prev && (r->regname != prev->regname)); \
    printf(" " #regname "=" format, r->regname); \
  } while (0)

void print_regs(const struct regs* r, const struct regs* prev, struct memory* m) {

  uint8_t code_data[4];
  code_data[0] = read8(m, r->pc);
  code_data[1] = read8(m, r->pc + 1);
  code_data[2] = read8(m, r->pc + 2);
  code_data[3] = 0;
  printf("regs:");
  print_reg_value(cycles, "%016llX");
  print_reg_value(ddx, "%04X");
  print_reg_value(af, "%04X");
  print_reg_value(bc, "%04X");
  print_reg_value(de, "%04X");
  print_reg_value(hl, "%04X");
  print_reg_value(sp, "%04X");
  print_reg_value(ime, "%d");
  print_reg_value(interrupt_enable, "%02X");
  print_reg_value(interrupt_flag, "%02X");
  print_reg_value(pc, "%04X");
  change_color(FORMAT_NORMAL, FORMAT_END);
  printf(" code=%02X%02X%02X", code_data[0], code_data[1], code_data[2]);

  int x;
  for (x = 0; x < 3; x++)
    if (code_data[x] < 0x20 || code_data[x] > 0x7E)
      code_data[x] = '?';
  printf(" \'%s\'   ", code_data);

  uint8_t* opcode_data = ptr(m, r->pc);
  print_opcode(stdout, opcode_data, m);
  printf("\n");
}

void print_regs_debug(FILE* f, const struct regs* r) {
  fprintf(f, ">>> registers\n");
  fprintf(f, "AF = %04X    (A = %02X, F = %02X)\n", r->af, r->a, r->f);
  fprintf(f, "BC = %04X    (B = %02X, C = %02X)\n", r->bc, r->b, r->c);
  fprintf(f, "DE = %04X    (D = %02X, E = %02X)\n", r->de, r->d, r->e);
  fprintf(f, "HL = %04X    (H = %02X, L = %02X)\n", r->hl, r->h, r->l);
  fprintf(f, "SP = %04X    PC = %04X    IME = %02X\n", r->sp, r->pc, r->ime);
  fprintf(f, "interrupt_flag = %02X        interrupt_enable = %02X\n",
      r->interrupt_flag, r->interrupt_enable);
  fprintf(f, "wait_for_interrupt = %02X    stop = %02X\n",
      r->wait_for_interrupt, r->stop);
  fprintf(f, "speed_switch = %02X          cycles = %016llX\n", r->speed_switch,
      r->cycles);
  fprintf(f, "debug = %02X                 memory_watchpoint = %02X\n",
      r->debug, r->ddx);
}

void disassemble(FILE* output_stream, void* data, uint32_t size, uint32_t offset, uint32_t dasm_size) {

  if (!output_stream)
    output_stream = stdout;

  uint32_t dasm_end = offset + dasm_size;
  while (offset < dasm_end) {
    fprintf(output_stream, "%04X ", offset);
    offset += print_opcode(output_stream, (uint8_t*)data + offset, NULL);
    fprintf(output_stream, "\n");
  }
}

void disassemble_memory(FILE* output_stream, struct memory* m, uint16_t addr, uint16_t size) {

  if (!output_stream)
    output_stream = stdout;

  uint16_t dasm_end = addr + size;
  while (addr < dasm_end) {
    uint8_t opcode_data[3];
    if (!valid_ptr(m, addr)) {
      fprintf(output_stream, "%04X << not valid address >>\n", addr);
      addr++;
    } else {
      opcode_data[0] = read8(m, addr);
      if (opcode_data[0] == 0xCB)
        opcode_data[1] = read8(m, addr + 1);
      fprintf(output_stream, "%04X ", addr);
      addr += print_opcode(output_stream, opcode_data, NULL);
      fprintf(output_stream, "\n");
    }
  }
}

union cart_data* debug_cart() {
  union cart_data* c = (union cart_data*)calloc(1, 0x8000);
  if (!c)
    return NULL;

  c->header.entry_code[0] = 0xC3; // jp 0200
  c->header.entry_code[2] = 0x02;

  static const uint8_t correct_logo[0x30] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
    0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
    0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
  };
  memcpy(c->header.logo, correct_logo, 0x30);

  strcpy(c->header.title, "DEBUG_CART");
  c->header.cgb_flag = 0x80;
  c->header.rom_size = 1;
  c->header.dest_code = 1;
  c->header.header_checksum = header_checksum(&c->header);

  int x, y;
  for (x = 0; x < 8; x++)
    c->rst_vectors[x][0] = 0xD8; // ret
  for (x = 0; x < 5; x++)
    c->interrupt_vectors[x][0] = 0xD9; // reti

  int code_offset = 0x200;
  for (x = 0; x < 0x100; x++) {
    for (y = 0; y < opcodes[x].size; y++) {
      c->data[code_offset++] = x;
    }
  }

  return c;
}
