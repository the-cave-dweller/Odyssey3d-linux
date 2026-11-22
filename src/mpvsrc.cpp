#include "mpvsrc.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef HAVE_MPV
#include <GLFW/glfw3.h>
#include <mpv/client.h>
#include <mpv/render_gl.h>

static void *getProcAddress(void *, const char *name) { return (void *)glfwGetProcAddress(name); }
static void renderUpdate(void *) {}

static bool setStr(mpv_handle *h, const char *opt, const char *val, std::string *err) {
  if (mpv_set_option_string(h, opt, val) < 0) {
    if (err) *err = std::string("mpv: no se pudo fijar la opción ") + opt;
    return false;
  }
  return true;
}
#endif

MpvSrc::MpvSrc() {}
MpvSrc::~MpvSrc() { destroy(); }

bool MpvSrc::init(std::string *err) {
#ifndef HAVE_MPV
  (void)err;
  if (err) *err = "compilado sin libmpv: instala mpv/libs (mpv-devel) y vuelve a compilar";
  return false;
#else
  mpv_handle *h = mpv_create();
  if (!h) {
    if (err) *err = "mpv: no se pudo crear la instancia";
    return false;
  }
  setStr(h, "config", "no", nullptr);
  setStr(h, "terminal", "no", nullptr);
  setStr(h, "vo", "libmpv", nullptr);
  setStr(h, "idle", "yes", nullptr);
  setStr(h, "keepaspect", "no", nullptr); /* las dos mitades SBS ocupan media pantalla cada una */
  const char *hw = getenv("ODYSSEY_HWDEC");
  setStr(h, "hwdec", hw && *hw ? hw : "auto", nullptr);
  setStr(h, "profile", "gpu", nullptr);
  if (mpv_initialize(h) < 0) {
    mpv_terminate_destroy(h);
    if (err) *err = "mpv: fallo al inicializar";
    return false;
  }
  mpv_opengl_init_params glInit{};
  glInit.get_proc_address = getProcAddress;
  glInit.get_proc_address_ctx = nullptr;
  mpv_render_param params[] = {{MPV_RENDER_PARAM_API_TYPE, (void *)"opengl"},
                               {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInit},
                               {MPV_RENDER_PARAM_INVALID, nullptr}};
  mpv_render_context *rc = nullptr;
  if (mpv_render_context_create(&rc, h, params) < 0 || !rc) {
    mpv_terminate_destroy(h);
    if (err) *err = "mpv: no se pudo crear el contexto de render (¿contexto GL actual?)";
    return false;
  }
  mpv_render_context_set_update_callback(rc, renderUpdate, nullptr);
  handle_ = h;
  renderCtx_ = rc;
  return true;
#endif
}

void MpvSrc::destroy() {
#ifdef HAVE_MPV
  if (renderCtx_) {
    mpv_render_context_free((mpv_render_context *)renderCtx_);
    renderCtx_ = nullptr;
  }
  if (handle_) {
    mpv_terminate_destroy((mpv_handle *)handle_);
    handle_ = nullptr;
  }
#endif
  fbo_.destroy();
}

void MpvSrc::createFbo(int w, int h) { fbo_.create(w, h); }

bool MpvSrc::load(const std::string &path, std::string *err) {
#ifdef HAVE_MPV
  if (!handle_) {
    if (err) *err = "mpv no inicializado";
    return false;
  }
  const char *cmd[] = {"loadfile", path.c_str(), "replace", nullptr};
  if (mpv_command_async((mpv_handle *)handle_, 1, cmd) < 0) {
    if (err) *err = "mpv: no se pudo cargar " + path;
    return false;
  }
  const char *no = "no";
  mpv_set_property_string((mpv_handle *)handle_, "pause", no);
  return true;
#else
  (void)path;
  if (err) *err = "compilado sin libmpv";
  return false;
#endif
}

GLuint MpvSrc::render() {
#ifdef HAVE_MPV
  if (!renderCtx_ || !fbo_.fbo) return 0;
  mpv_render_context_update((mpv_render_context *)renderCtx_);
  fbo_.bind();
  mpv_opengl_fbo mfbo{};
  mfbo.fbo = (int)fbo_.fbo;
  mfbo.w = fbo_.w;
  mfbo.h = fbo_.h;
  mfbo.internal_format = GL_RGBA8;
  int flipY = 1; /* el quad de la trama usa uv.y=0 arriba: mpv debe volcar la imagen al revés */
  mpv_render_param params[] = {{MPV_RENDER_PARAM_OPENGL_FBO, &mfbo},
                               {MPV_RENDER_PARAM_FLIP_Y, &flipY},
                               {MPV_RENDER_PARAM_INVALID, nullptr}};
  mpv_render_context_render((mpv_render_context *)renderCtx_, params);
  fbo_.unbind();
  return fbo_.tex;
#else
  return 0;
#endif
}

void MpvSrc::togglePause() {
#ifdef HAVE_MPV
  if (!handle_) return;
  const char *cmd[] = {"cycle", "pause", nullptr};
  mpv_command((mpv_handle *)handle_, cmd);
  paused_ = !paused_;
#endif
}

void MpvSrc::seek(double seconds) {
#ifdef HAVE_MPV
  if (!handle_) return;
  std::string s = std::to_string(seconds);
  const char *cmd[] = {"seek", s.c_str(), "absolute", nullptr};
  mpv_command_async((mpv_handle *)handle_, 2, cmd);
#endif
}

void MpvSrc::frameStep(int dir) {
#ifdef HAVE_MPV
  if (!handle_) return;
  mpv_set_property_string((mpv_handle *)handle_, "pause", "yes");
  std::string d = dir >= 0 ? "up" : "down";
  const char *cmd[] = {"frame-step", d.c_str(), nullptr};
  mpv_command_async((mpv_handle *)handle_, 3, cmd);
  paused_ = true;
#endif
}

void MpvSrc::setSpeed(double s) {
#ifdef HAVE_MPV
  if (!handle_) return;
  double v = s;
  mpv_set_property((mpv_handle *)handle_, "speed", MPV_FORMAT_DOUBLE, &v);
#endif
}

void MpvSrc::poll() {
#ifdef HAVE_MPV
  if (!handle_) return;
  double t = 0.0, d = 0.0;
  if (mpv_get_property((mpv_handle *)handle_, "playback-time", MPV_FORMAT_DOUBLE, &t) == 0) time_ = t;
  if (mpv_get_property((mpv_handle *)handle_, "duration", MPV_FORMAT_DOUBLE, &d) == 0) dur_ = d;
  int eof = 0;
  mpv_get_property((mpv_handle *)handle_, "eof-reached", MPV_FORMAT_FLAG, &eof);
  eof_ = eof == 1;
  int p = 0;
  mpv_get_property((mpv_handle *)handle_, "pause", MPV_FORMAT_FLAG, &p);
  paused_ = p == 1;
#endif
}
