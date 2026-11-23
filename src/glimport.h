/* glimport.h — convierte un fotograma DMA-BUF del compositor en una textura OpenGL.
   KWin entrega la ventana como DMA-BUF con el modificador del fabricante de la GPU, así que
   copiar los bytes con la CPU no funciona (NVIDIA los deja en memoria bloque-lineal): hay que
   importarlos con eglCreateImageKHR y usarlos directamente en la GPU, sin copia. */
#ifndef ODYSSEY_GLIMPORT_H
#define ODYSSEY_GLIMPORT_H

#include "frame.h"  /* GpuFrame: el fotograma DMA-BUF del compositor */
#include "glutil.h" /* define GL_GLEXT_PROTOTYPES antes que GL/gl.h */

#include <cstdint>
#include <string>
#include <vector>

class GpuImporter {
public:
  /* Requiere un contexto OpenGL actual (ventana GLFW creada). */
  bool init(std::string *err = nullptr);
  void destroy();
  bool ok() const { return ok_; }

  /* Devuelve la textura que contiene ese fotograma. Los búferes del compositor se reutilizan,
     así que cada fd se importa una sola vez y después sólo se cambia de textura. */
  GLuint upload(const GpuFrame &f, std::string *err = nullptr);

  /* Diagnóstico. */
  const std::string &method() const { return method_; }
  const std::string &lastError() const { return err_; }
  int imported() const { return static_cast<int>(cache_.size()); }
  /* última textura entregada: sirve para repetir el fotograma cuando no llega ninguno nuevo */
  GLuint lastTexture() const { return lastTex_; }

private:
  struct Entry {
    int fd = -1;
    uint64_t modifier = 0;
    int stride = 0, w = 0, h = 0;
    void *image = nullptr; /* EGLImageKHR */
    GLuint tex = 0;
  };

  void *createImage(const GpuFrame &f, std::string *err);

  std::vector<Entry> cache_;
  GLuint lastTex_ = 0;
  void *dpy_ = nullptr;  /* EGLDisplay */
  void *createImageFn_ = nullptr;
  void *targetStorageFn_ = nullptr; /* glEGLImageTargetTextureStorageEXT */
  void *target2dFn_ = nullptr;      /* glEGLImageTargetTexture2DOES (alternativa) */
  bool ok_ = false;
  std::string method_, err_;
};

/* Pasos de un cuadrado y programa que sólo muestrea una textura: sirve para volcar a PNG
   el contenido real de una textura importada (comprobación en la línea de órdenes). */
GLuint glImportBlitProgram();
bool glImportReadback(GLuint srcTex, int w, int h, std::vector<uint8_t> &rgbaTopFirst, std::string *err);

#endif
