/* glimport.cpp — implementación de la importación DMA-BUF → textura OpenGL. */
#include "glimport.h"
#include "glutil.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glext.h>

#include <dlfcn.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

/* glext.h de este sistema no trae el alias antiguo de GL_OES_EGL_image */
#ifndef GL_OES_EGL_image
#ifndef GLeglImageOES
typedef void *GLeglImageOES;
#endif
typedef void(APIENTRYP PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)(GLenum target, GLeglImageOES image);
#endif

#ifndef EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT
#define EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT 0x3443
#endif
#ifndef EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT
#define EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT 0x3444
#endif

bool GpuImporter::init(std::string *err) {
  EGLDisplay dpy = eglGetCurrentDisplay();
  if (dpy == EGL_NO_DISPLAY) {
    if (err) *err = "no hay contexto EGL activo (hace falta la ventana GLFW)";
    return false;
  }
  dpy_ = dpy;
  const char *exts = eglQueryString(dpy, EGL_EXTENSIONS);
  if (!exts || !std::strstr(exts, "EGL_EXT_image_dma_buf_import")) {
    if (err) *err = "este controlador no ofrece EGL_EXT_image_dma_buf_import";
    return false;
  }
  createImageFn_ = reinterpret_cast<void *>(eglGetProcAddress("eglCreateImageKHR"));
  if (!createImageFn_) {
    if (err) *err = "falta eglCreateImageKHR";
    return false;
  }

  /* en un perfil core glGetString(GL_EXTENSIONS) viene vacío: hay que recorrer glGetStringi */
  std::string glext;
  GLint nExt = 0;
  glGetIntegerv(GL_NUM_EXTENSIONS, &nExt);
  for (GLint i = 0; i < nExt; ++i) {
    const char *e = reinterpret_cast<const char *>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)));
    if (e) glext += std::string(e) + " ";
  }
  if (glext.find("GL_EXT_EGL_image_storage") != std::string::npos)
    targetStorageFn_ = reinterpret_cast<void *>(eglGetProcAddress("glEGLImageTargetTextureStorageEXT"));
  if (glext.find("GL_OES_EGL_image") != std::string::npos)
    target2dFn_ = reinterpret_cast<void *>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
  if (!targetStorageFn_ && target2dFn_) { /* la vía antigua sólo vale si el OpenGL la anuncia */
  }
  if (!targetStorageFn_ && !target2dFn_) {
    if (err) *err = "el OpenGL no permite enlazar EGLImage (GL_EXT_EGL_image_storage / GL_OES_EGL_image)";
    return false;
  }
  method_ = targetStorageFn_ ? "GL_EXT_EGL_image_storage" : "GL_OES_EGL_image";
  ok_ = true;
  return true;
}

void GpuImporter::destroy() {
  if (dpy_ && createImageFn_) {
    auto destroyImage = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
    if (destroyImage)
      for (Entry &e : cache_)
        if (e.image) destroyImage(static_cast<EGLDisplay>(dpy_), static_cast<EGLImageKHR>(e.image));
  }
  for (Entry &e : cache_)
    if (e.tex) glDeleteTextures(1, &e.tex);
  cache_.clear();
  lastTex_ = 0;
  ok_ = false;
  dpy_ = nullptr;
  createImageFn_ = targetStorageFn_ = target2dFn_ = nullptr;
}

void *GpuImporter::createImage(const GpuFrame &f, std::string *err) {
  EGLint attrs[26];
  int n = 0;
  attrs[n++] = EGL_WIDTH; attrs[n++] = static_cast<EGLint>(f.w);
  attrs[n++] = EGL_HEIGHT; attrs[n++] = static_cast<EGLint>(f.h);
  attrs[n++] = EGL_LINUX_DRM_FOURCC_EXT; attrs[n++] = static_cast<EGLint>(f.fourcc);
  attrs[n++] = EGL_DMA_BUF_PLANE0_FD_EXT; attrs[n++] = static_cast<EGLint>(f.fd);
  attrs[n++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT; attrs[n++] = static_cast<EGLint>(f.offset);
  attrs[n++] = EGL_DMA_BUF_PLANE0_PITCH_EXT; attrs[n++] = static_cast<EGLint>(f.stride);
  if (f.modifier) {
    attrs[n++] = EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT;
    attrs[n++] = static_cast<EGLint>(f.modifier & 0xffffffffu);
    attrs[n++] = EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT;
    attrs[n++] = static_cast<EGLint>((f.modifier >> 32) & 0xffffffffu);
  }
  attrs[n++] = EGL_NONE;

  auto create = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(createImageFn_);
  EGLImageKHR img = create(static_cast<EGLDisplay>(dpy_), EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attrs);
  if (img == EGL_NO_IMAGE_KHR) {
    char buf[256];
    std::snprintf(buf, sizeof buf,
                  "eglCreateImageKHR falló (0x%04x): fourcc %c%c%c%c %dx%d paso %d offset %d modificador 0x%llx",
                  eglGetError(), static_cast<char>(f.fourcc & 0xff), static_cast<char>((f.fourcc >> 8) & 0xff),
                  static_cast<char>((f.fourcc >> 16) & 0xff), static_cast<char>((f.fourcc >> 24) & 0xff), f.w, f.h,
                  f.stride, f.offset, static_cast<unsigned long long>(f.modifier));
    if (err) *err = buf;
    return nullptr;
  }
  return img;
}

GLuint GpuImporter::upload(const GpuFrame &f, std::string *err) {
  if (!ok_ && !init(err)) return 0;
  if (!f.valid()) {
    if (err) *err = "el fotograma no trae un DMA-BUF utilizable";
    return 0;
  }
  for (Entry &e : cache_)
    if (e.fd == f.fd && e.modifier == f.modifier && e.stride == f.stride && e.w == f.w && e.h == f.h)
      return lastTex_ = e.tex;

  std::string e2;
  void *img = createImage(f, &e2);
  if (!img) {
    if (err) *err = e2;
    return 0;
  }
  Entry en;
  en.fd = f.fd;
  en.modifier = f.modifier;
  en.stride = f.stride;
  en.w = f.w;
  en.h = f.h;
  en.image = img;
  glGenTextures(1, &en.tex);
  glBindTexture(GL_TEXTURE_2D, en.tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  if (targetStorageFn_) {
    auto fn = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURESTORAGEEXTPROC>(targetStorageFn_);
    fn(en.tex, static_cast<EGLImageKHR>(img), nullptr);
  } else {
    auto fn = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(target2dFn_);
    fn(GL_TEXTURE_2D, static_cast<EGLImageKHR>(img));
  }
  glBindTexture(GL_TEXTURE_2D, 0);
  const GLenum glErr = glGetError();
  if (glErr != GL_NO_ERROR) {
    glDeleteTextures(1, &en.tex);
    char buf[128];
    std::snprintf(buf, sizeof buf, "el OpenGL rechazó la textura importada (error 0x%04x)", glErr);
    if (err) *err = buf;
    return 0;
  }
  cache_.push_back(en);
  return lastTex_ = en.tex;
}

/* ------------ volcado de prueba (para comprobar en la línea de órdenes) ------------ */

GLuint glImportBlitProgram() {
  static const char *vs =
      "#version 330 core\n"
      "out vec2 vUv;\n"
      "void main(){\n"
      "  vec2 p = vec2(gl_VertexID == 1 ? 3.0 : -1.0, gl_VertexID == 2 ? 3.0 : -1.0);\n"
      "  vUv = (p + 1.0) * 0.5;\n"
      "  gl_Position = vec4(p, 0.0, 1.0);\n"
      "}\n";
  static const char *fs =
      "#version 330 core\n"
      "in vec2 vUv; out vec4 o; uniform sampler2D t;\n"
      "void main(){ o = texture(t, vec2(vUv.x, 1.0 - vUv.y)); }\n"; /* el DMA-BUF trae la fila 0 arriba */
  return glLinkProgram(vs, fs, nullptr);
}

bool glImportReadback(GLuint srcTex, int w, int h, std::vector<uint8_t> &rgbaTopFirst, std::string *err) {
  if (!srcTex || w <= 0 || h <= 0) return false;
  GLuint prog = glImportBlitProgram();
  if (!prog) {
    if (err) *err = "no se pudo compilar el programa de volcado";
    return false;
  }
  GLuint fbo = 0, dst = 0, vao = 0;
  glGenFramebuffers(1, &fbo);
  glGenTextures(1, &dst);
  glGenVertexArrays(1, &vao);
  glBindTexture(GL_TEXTURE_2D, dst);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst, 0);
  bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
  if (!ok && err) *err = "el búfer de volcado no está completo";
  if (ok) {
    glBindVertexArray(vao);
    glUseProgram(prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcTex);
    glUniform1i(glGetUniformLocation(prog, "t"), 0);
    glViewport(0, 0, w, h);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    std::vector<uint8_t> rows(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rows.data());
    const GLenum glErr = glGetError();
    if (glErr != GL_NO_ERROR && err) {
      char buf[96];
      std::snprintf(buf, sizeof buf, "error de OpenGL en el volcado (0x%04x)", glErr);
      *err = buf;
    }
    rgbaTopFirst.assign(rows.size(), 0);
    for (int y = 0; y < h; ++y) /* glReadPixels da la fila 0 abajo */
      std::memcpy(&rgbaTopFirst[static_cast<size_t>(y) * w * 4],
                  &rows[static_cast<size_t>(h - 1 - y) * w * 4], static_cast<size_t>(w) * 4);
    ok = true;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDeleteVertexArrays(1, &vao);
  glDeleteTextures(1, &dst);
  glDeleteFramebuffers(1, &fbo);
  glDeleteProgram(prog);
  return ok;
}
