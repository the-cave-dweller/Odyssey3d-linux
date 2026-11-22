/* testsrc.h — patrón de prueba side-by-side para alinear la trama y comprobar profundidad. */
#ifndef ODYSSEY_TESTSRC_H
#define ODYSSEY_TESTSRC_H

#include "glutil.h"

#include <string>

class TestSrc {
public:
  bool init(std::string *err);
  void destroy();
  void createFbo(int w, int h);
  /* Dibuja el patrón en el FBO y devuelve su textura (SBS w x h, cada mitad w/2 x h). */
  GLuint render(double t, int xoffPx, int yoffPx);
  GLuint texture() const { return fbo_.tex; }
  GLuint fboId() const { return fbo_.fbo; }

private:
  GLuint prog_ = 0, vao_ = 0;
  GLint uRes_ = -1, uT_ = -1, uOff_ = -1;
  Fbo fbo_{};
};

#endif
