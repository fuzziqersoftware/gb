#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "cpu.h"
#include "debug.h"
#include "display.h"
#include "terminal.h"

extern int terminal_fd;

void debug_main(struct regs* r, struct memory* m) {

  display_pause((struct display*)m->devices[DEVICE_DISPLAY]);
  if (terminal_fd != -1 && isatty(terminal_fd))
      reset_terminal_settings();

  char filename[L_tmpnam];
  tmpnam(filename);
  FILE* f = fopen(filename, "w");

  fprintf(f, "debug interrupt: %s\n\n", r->debug_interrupt_reason);

  print_regs_debug(f, r);

  fprintf(f, "\n>>> disassembly around pc\n");
  uint32_t dasm_start_offset = r->pc;
  uint32_t dasm_end_offset = r->pc;
  dasm_start_offset = (dasm_start_offset < 0x80) ? 0 : (dasm_start_offset - 0x80);
  dasm_end_offset = (dasm_end_offset > 0xFF80) ? 0x10000 : (dasm_end_offset + 0x80);
  disassemble_memory(f, m, dasm_start_offset, dasm_end_offset - dasm_start_offset);

  fprintf(f, "\n");

  print_memory_debug(f, m);
  display_print(f, (struct display*)m->devices[DEVICE_DISPLAY]);

  fclose(f);

  char cmd_buffer[L_tmpnam + 12];
  sprintf(cmd_buffer, "less \"%s\"", filename);
  system(cmd_buffer);

  unlink(filename);

  if (terminal_fd != -1 && isatty(terminal_fd))
      set_terminal_raw();
  display_resume((struct display*)m->devices[DEVICE_DISPLAY]);
}
