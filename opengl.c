#include <pthread.h>
#include <stdlib.h>

#ifdef MACOSX
#include "OpenGL/gl.h"
#include "OpenGL/glu.h"
#include "GLUT/glut.h"
#else
#include "GL/gl.h"
#include "GL/glu.h"
#include "GL/glut.h"
#endif


void opengl_init() {
  // nice try, glut. you leave my cli alone
  int argc = 1;
  char* argv[] = {"glut", NULL};
  glutInit(&argc, argv);
}

void opengl_create_window(int w, int h, const char* title) {
  glutInitWindowSize(w, h);
  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
  glutCreateWindow(title);

  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);
  glLineWidth(3);
  glPointSize(12);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void* opengl_run_loop_async_thread(void* _unused) {
  glutMainLoop();
  return NULL;
}

int opengl_run_loop_async() {
  pthread_t t;
  int err = pthread_create(&t, NULL, opengl_run_loop_async_thread, NULL);
  if (err)
    return err;
  return pthread_detach(t);
}
