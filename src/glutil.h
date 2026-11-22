/* glutil.h — carga de OpenGL 3.3 core, programas y texturas (sin GLEW). */
#ifndef ODYSSEY_GLUTIL_H
#define ODYSSEY_GLUTIL_H

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>

#include <cstdint>
#include <string>
#include <vector>

/* Comprueba errores de compilación/enlazado de shaders. */
GLuint glCompileShaderSrc(GLenum type, const char *src, std::string *err);
GLuint glLinkProgram(const char *vs, const char *fs, std::string *err);
void glCheckErr(const char *where);

/* Crea una textura 2D RGBA8 a partir de píxeles (origin-abajo, como OpenGL). */
GLuint glTexFromRGBA(int w, int h, const uint8_t *rgba, bool linear);
/* Crea una textura 2D R8 a partir de un mapa de grayscale. */
GLuint glTexFromR8(int w, int h, const uint8_t *gray, bool linear);

/* Escribe un PNG RGBA (píxeles con la primera fila arriba). Necesita libpng. */
bool glPngSave(const std::string &path, int w, int h, const uint8_t *rgbaTopFirst);

/* Carga un PNG en 8 bits devolviendo RGBA (origin-arriba). Necesita libpng; sin ella devuelve vacío. */
bool pngLoadRGBA(const std::string &path, int &w, int &h, std::vector<uint8_t> &rgba);

struct Fbo {
  GLuint fbo = 0, tex = 0;
  int w = 0, h = 0;
  void create(int width, int height);
  void bind() { glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, w, h); }
  void unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
  void destroy();
};

#endif
