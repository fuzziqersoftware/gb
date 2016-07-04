#ifndef __GLTEXT_H
#define __GLTEXT_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void draw_text(float x, float y, float r, float g, float b, float a,
    float aspect_ratio, float char_size, int centered, const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // __GLTEXT_H
