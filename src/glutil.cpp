#include "glutil.h"

#include <setjmp.h>

#include <cstdio>
#include <cstring>
#include <vector>

#ifdef HAVE_PNG
#include <png.h>
#endif

void glCheckErr(const char *where) {
  GLenum e = glGetError();
  if (e != GL_NO_ERROR)
    std::fprintf(stderr, "[gl] error 0x%04x en %s\n", (unsigned)e, where);
}

GLuint glCompileShaderSrc(GLenum type, const char *src, std::string *err) {
  GLuint sh = glCreateShader(type);
  glShaderSource(sh, 1, &src, nullptr);
  glCompileShader(sh);
  GLint ok = 0;
  glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[4096];
    glGetShaderInfoLog(sh, sizeof log, nullptr, log);
    if (err) *err = std::string("shader ") + (type == GL_VERTEX_SHADER ? "de vértice" : "de fragmento") + ": " + log;
    glDeleteShader(sh);
    return 0;
  }
  return sh;
}

GLuint glLinkProgram(const char *vs, const char *fs, std::string *err) {
  GLuint v = glCompileShaderSrc(GL_VERTEX_SHADER, vs, err);
  if (!v) return 0;
  GLuint f = glCompileShaderSrc(GL_FRAGMENT_SHADER, fs, err);
  if (!f) {
    glDeleteShader(v);
    return 0;
  }
  GLuint p = glCreateProgram();
  glAttachShader(p, v);
  glAttachShader(p, f);
  glLinkProgram(p);
  glDeleteShader(v);
  glDeleteShader(f);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[4096];
    glGetProgramInfoLog(p, sizeof log, nullptr, log);
    if (err) *err = std::string("enlace de programa: ") + log;
    glDeleteProgram(p);
    return 0;
  }
  return p;
}

GLuint glTexFromRGBA(int w, int h, const uint8_t *rgba, bool linear) {
  GLuint t = 0;
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  GLint f = linear ? GL_LINEAR : GL_NEAREST;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return t;
}

GLuint glTexFromR8(int w, int h, const uint8_t *gray, bool linear) {
  GLuint t = 0;
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, gray);
  GLint f = linear ? GL_LINEAR : GL_NEAREST;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return t;
}

bool pngLoadRGBA(const std::string &path, int &w, int &h, std::vector<uint8_t> &rgba) {
#ifdef HAVE_PNG
  FILE *fp = fopen(path.c_str(), "rb");
  if (!fp) return false;
  png_structp r = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  png_infop info = r ? png_create_info_struct(r) : nullptr;
  if (!r || !info || setjmp(png_jmpbuf(r))) {
    fclose(fp);
    return false;
  }
  png_init_io(r, fp);
  png_read_info(r, info);
  w = (int)png_get_image_width(r, info);
  h = (int)png_get_image_height(r, info);
  png_byte ct = png_get_color_type(r, info);
  png_byte bd = png_get_bit_depth(r, info);
  if (bd == 16) png_set_strip_16(r);
  if (ct == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(r);
  if (ct == PNG_COLOR_TYPE_GRAY && bd < 8) png_set_expand_gray_1_2_4_to_8(r);
  if (png_get_valid(r, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(r);
  if (ct == PNG_COLOR_TYPE_GRAY || ct == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(r);
  if (!(ct & PNG_COLOR_MASK_ALPHA)) png_set_filler(r, 0xff, PNG_FILLER_AFTER);
  png_read_update_info(r, info);
  rgba.assign((size_t)w * h * 4, 0);
  std::vector<png_bytep> rows((size_t)h);
  for (int y = 0; y < h; y++) rows[y] = rgba.data() + (size_t)y * w * 4;
  png_read_image(r, rows.data());
  png_destroy_read_struct(&r, &info, nullptr);
  fclose(fp);
  return true;
#else
  (void)path; (void)w; (void)h; (void)rgba;
  return false;
#endif
}

void Fbo::create(int width, int height) {
  destroy();
  w = width;
  h = height;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glCheckErr("Fbo::create");
}

void Fbo::destroy() {
  if (fbo) {
    glDeleteFramebuffers(1, &fbo);
    fbo = 0;
  }
  if (tex) {
    glDeleteTextures(1, &tex);
    tex = 0;
  }
}

