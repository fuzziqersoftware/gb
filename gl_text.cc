#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef MACOSX
#include "OpenGL/gl.h"
#include "OpenGL/glu.h"
#include "GLUT/glut.h"
#else
#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glut.h"
#endif

#include "gl_text_font.hh"

const float cell_size = 1;
const float cell_division_size = 0;
const float char_space_size = 0.5;

extern "C" void draw_text(float x, float y, float r, float g, float b, float a,
    float aspect_ratio, float char_size, int centered, const char* fmt, ...) {

  char* s = NULL;
  va_list va;
  va_start(va, fmt);
  int len = vasprintf(&s, fmt, va);
  va_end(va);
  if (!s) {
    return;
  }

  if (len == 0) {
    return;
  }

  if (centered) {
    int num_spaces = len - 1;
    float chars_width = 0.0;
    for (const char* t = s; *t; t++) {
      const vector<bool>& bitmap = font[(unsigned char)*t];
      size_t this_char_width = bitmap.size() / 9;
      chars_width += this_char_width * cell_size + (this_char_width - 1) * cell_division_size;
    }
    float totalWidth = (chars_width * char_size + char_space_size * char_size * num_spaces) / aspect_ratio;

    // note: we treat total height as 7 instead of 9 because the bottom two
    // cells are below the base line (for letters like p and q)
    float totalHeight = (7 * cell_size + 6 * cell_division_size) * char_size;
    x -= totalWidth / 2;
    y += totalHeight / 2;
  }

  glBegin(GL_QUADS);
  glColor4f(r, g, b, a);

  double currentX = x, currentY = y;
  for (; *s; s++) {
    const vector<bool>& bitmap = font[(unsigned char)*s];
    size_t char_width = bitmap.size() / 9;
    for (int y = 0; y < 9; y++) {
      for (int x = 0; x < char_width; x++) {
        if (!bitmap[y * char_width + x])
          continue;
        glVertex2f(currentX + ((cell_size + cell_division_size) * x) * char_size / aspect_ratio,
                   currentY - ((cell_size + cell_division_size) * y) * char_size);
        glVertex2f(currentX + ((cell_size + cell_division_size) * x + cell_size) * char_size / aspect_ratio,
                   currentY - ((cell_size + cell_division_size) * y) * char_size);
        glVertex2f(currentX + ((cell_size + cell_division_size) * x + cell_size) * char_size / aspect_ratio,
                   currentY - ((cell_size + cell_division_size) * y + cell_size) * char_size);
        glVertex2f(currentX + ((cell_size + cell_division_size) * x) * char_size / aspect_ratio,
                   currentY - ((cell_size + cell_division_size) * y + cell_size) * char_size);
      }
    }

    double total_size = char_width * cell_size + (char_width - 1) * cell_division_size + char_space_size;
    currentX += (total_size * char_size) / aspect_ratio;
  }
  glEnd();
}
