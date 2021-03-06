#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "terminal.h"

int write_change_color(char* fmt, int color, ...) {
  fmt[0] = '\033';
  int fmt_len = 1;

  va_list va;
  va_start(va, color);
  do {
    fmt[fmt_len] = (fmt_len == 1) ? '[' : ';';
    fmt_len++;
    fmt_len += sprintf(&fmt[fmt_len], "%d", color);
    color = va_arg(va, int);
  } while (color != FORMAT_END);
  va_end(va);

  fmt[fmt_len] = 'm';
  fmt[fmt_len + 1] = 0;
  return fmt_len + 1;
}

void change_color(int color, ...) {

  char fmt[0x40] = "\033";
  int fmt_len = strlen(fmt);

  va_list va;
  va_start(va, color);
  do {
    fmt[fmt_len] = (fmt_len == 1) ? '[' : ';';
    fmt_len++;
    fmt_len += sprintf(&fmt[fmt_len], "%d", color);
    color = va_arg(va, int);
  } while (color != FORMAT_END);
  va_end(va);

  fmt[fmt_len] = 'm';
  fmt[fmt_len + 1] = 0;

  printf("%s", fmt);
}

void print_data(FILE* out, uint64_t address, const void* _data, uint64_t data_size) {

  if (data_size == 0)
    return;

  // if color is disabled or no diff source is given, disable diffing
  const uint8_t* data = (const uint8_t*)_data;

  char data_ascii[20];

  // start_offset is how many blank spaces to print before the first byte
  int start_offset = address & 0x0F;
  address &= ~0x0F;
  data_size += start_offset;

  // if nonzero, print the address here (the loop won't do it for the 1st line)
  if (start_offset)
    fprintf(out, "%016llX | ", address);

  // print initial spaces, if any
  unsigned long long x, y;
  for (x = 0; x < start_offset; x++) {
    fprintf(out, "   ");
    data_ascii[x] = ' ';
  }

  // print the data
  for (; x < data_size; x++) {

    int line_offset = x & 0x0F;
    int data_offset = x - start_offset;
    data_ascii[line_offset] = data[data_offset];

    // first byte on the line? then print the address
    if ((x & 0x0F) == 0)
      fprintf(out, "%016llX | ", address + x);

    // print the byte itself
    fprintf(out, "%02X ", data[data_offset]);

    // last byte on the line? then print the ascii view and a \n
    if ((x & 0x0F) == 0x0F) {
      fprintf(out, "| ");
      for (y = 0; y < 16; y++) {
        if (data_ascii[y] < 0x20 || data_ascii[y] == 0x7F)
          putc('?', out);
        else
          putc(data_ascii[y], out);
      }

      fprintf(out, "\n");
    }
  }

  // if the last line is a partial line, print the remaining ascii chars
  if (x & 0x0F) {
    for (y = x; y & 0x0F; y++)
      fprintf(out, "   ");
    fprintf(out, "| ");
    for (y = 0; y < (x & 0x0F); y++) {
      if (data_ascii[y] < 0x20 || data_ascii[y] == 0x7F)
        putc('?', out);
      else
        putc(data_ascii[y], out);
    }
    fprintf(out, "\n");
  }
}
