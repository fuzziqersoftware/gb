#ifndef TERMINAL_H
#define TERMINAL_H

#define FORMAT_END         (-1)
#define FORMAT_NORMAL      0
#define FORMAT_BOLD        1
#define FORMAT_UNDERLINE   4
#define FORMAT_BLINK       5
#define FORMAT_INVERSE     7
#define FORMAT_FG_BLACK    30
#define FORMAT_FG_RED      31
#define FORMAT_FG_GREEN    32
#define FORMAT_FG_YELLOW   33
#define FORMAT_FG_BLUE     34
#define FORMAT_FG_MAGENTA  35
#define FORMAT_FG_CYAN     36
#define FORMAT_FG_GRAY     37
#define FORMAT_FG_WHITE    38
#define FORMAT_BG_BLACK    40
#define FORMAT_BG_RED      41
#define FORMAT_BG_GREEN    42
#define FORMAT_BG_YELLOW   43
#define FORMAT_BG_BLUE     44
#define FORMAT_BG_MAGENTA  45
#define FORMAT_BG_CYAN     46
#define FORMAT_BG_GRAY     47
#define FORMAT_BG_WHITE    48

int write_change_color(char* fmt, int color, ...);
void change_color(int color, ...);

void print_data(FILE* out, uint64_t address, const void* _data, uint64_t data_size);

#endif // TERMINAL_H
