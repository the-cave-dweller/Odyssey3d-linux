/* pngsave.cpp — glPngSave, aparte de glutil.cpp (v1.16): no usa OpenGL, y asi lo puede llevar el panel ligero
   (odysseyvk-panel) sin enlazar GL */
#include "glutil.h"

#include <setjmp.h>

#include <cstdio>
#include <cstring>
#include <vector>

#ifdef HAVE_PNG
#include <png.h>
#endif

bool glPngSave(const std::string &path, int w, int h, const uint8_t *rgbaTopFirst) {
#ifdef HAVE_PNG
  png_structp p = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!p) return false;
  png_infop info = png_create_info_struct(p);
  if (!info) {
    png_destroy_write_struct(&p, nullptr);
    return false;
  }
  if (setjmp(png_jmpbuf(p))) {
    png_destroy_write_struct(&p, &info);
    return false;
  }
  FILE *f = std::fopen(path.c_str(), "wb");
  if (!f) {
    png_destroy_write_struct(&p, &info);
    return false;
  }
  png_init_io(p, f);
  png_set_IHDR(p, info, (png_uint_32)w, (png_uint_32)h, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  std::vector<png_bytep> rows((size_t)h);
  for (int y = 0; y < h; y++) rows[(size_t)y] = const_cast<png_bytep>(rgbaTopFirst + (size_t)y * (size_t)w * 4u);
  png_set_rows(p, info, rows.data());
  png_write_png(p, info, PNG_TRANSFORM_IDENTITY, nullptr);
  png_destroy_write_struct(&p, &info);
  std::fclose(f);
  return true;
#else
  (void)path; (void)w; (void)h; (void)rgbaTopFirst;
  return false;
#endif
}
