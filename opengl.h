#ifndef OPENGL_H
#define OPENGL_H

inline int opengl_pixel_x(int win_x, float x) {
  return ((x + 1) / 2) * win_x;
}

inline int opengl_pixel_y(int win_y, float y) {
  return ((y + 1) / 2) * win_y;
}

inline float opengl_window_x(int win_x, int x) {
  return (2 * (float)x / win_x) - 1;
}

inline float opengl_window_y(int win_y, int y) {
  return (2 * (float)y / win_y) - 1;
}

#endif // OPENGL_H
