/* mpvsrc.h — decodificación/reproducción de vídeo SBS con libmpv (render API, con NVDEC). */
#ifndef ODYSSEY_MPVSRC_H
#define ODYSSEY_MPVSRC_H

#include "glutil.h"

#include <string>

class MpvSrc {
public:
  MpvSrc();
  ~MpvSrc();

  /* Necesita un contexto GL actual. */
  bool init(std::string *err);
  void destroy();

  bool load(const std::string &path, std::string *err);
  void createFbo(int w, int h);
  /* Dibuja el fotograma actual en el FBO y devuelve su textura. */
  GLuint render();
  GLuint texture() const { return fbo_.tex; }
  GLuint fboId() const { return fbo_.fbo; }

  void togglePause();
  bool paused() const { return paused_; }
  void seek(double seconds);
  void frameStep(int dir);
  void setSpeed(double s);
  double time() const { return time_; }
  double duration() const { return dur_; }
  void poll();          /* lee propiedades (tiempo, duración, fin) */
  bool eof() const { return eof_; }

private:
  void *handle_ = nullptr;    /* mpv_handle* */
  void *renderCtx_ = nullptr; /* mpv_render_context* */
  Fbo fbo_{};
  bool paused_ = false;
  bool eof_ = false;
  double time_ = 0.0, dur_ = 0.0;
};

#endif
