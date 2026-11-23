/* prueba_dibr.cpp — comprobar la conversi6n 2D a 3D sin pantalla de por medio: toma una imagen,
   calcula la profundidad y aplica el mismo desplazamiento por ojo que hace el shader de la trama,
   y escribe la vista izquierda, la derecha y un anaglifo (rojo/cian) para poder mirarlo. */
#include "depth.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static bool leePpm(const char *ruta, std::vector<uint8_t> &rgba, int &w, int &h, std::string *err) {
  FILE *f = std::fopen(ruta, "rb");
  if (!f) {
    *err = std::string("no se pudo abrir ") + ruta;
    return false;
  }
  char m[3] = {0, 0, 0};
  if (std::fread(m, 1, 2, f) != 2 || m[0] != 'P' || m[1] != '6') {
    *err = "se necesita un PPM binario (P6)";
    std::fclose(f);
    return false;
  }
  int vals[3] = {0, 0, 0};
  for (int i = 0; i < 3; i++) {
    int c = std::fgetc(f);
    while (c == '#' || c == '\n' || c == ' ' || c == '\t' || c == '\r') {
      while (c != '\n' && c != EOF) c = std::fgetc(f);
      c = std::fgetc(f);
    }
    while (c >= '0' && c <= '9') {
      vals[i] = vals[i] * 10 + (c - '0');
      c = std::fgetc(f);
    }
  }
  w = vals[0], h = vals[1];   /* el bucle de arriba ya se comio el salto despues del maximo */
  std::vector<uint8_t> rgb((size_t)w * (size_t)h * 3);
  if (std::fread(rgb.data(), 1, rgb.size(), f) != rgb.size()) {
    *err = "el PPM esta incompleto";
    std::fclose(f);
    return false;
  }
  std::fclose(f);
  rgba.resize((size_t)w * (size_t)h * 4);
  for (size_t i = 0; i < (size_t)w * (size_t)h; i++) {
    rgba[i * 4] = rgb[i * 3];
    rgba[i * 4 + 1] = rgb[i * 3 + 1];
    rgba[i * 4 + 2] = rgb[i * 3 + 2];
    rgba[i * 4 + 3] = 255;
  }
  return true;
}

static void escribePpm(const char *ruta, const std::vector<uint8_t> &rgb, int w, int h) {
  FILE *f = std::fopen(ruta, "wb");
  if (!f) return;
  std::fprintf(f, "P6\n%d %d\n255\n", w, h);
  std::fwrite(rgb.data(), 1, rgb.size(), f);
  std::fclose(f);
}

/* bilinear sobre la imagen, con los bordes estirados (como hace el tramado de la pantalla) */
static void muestrea(const std::vector<uint8_t> &img, int w, int h, float x, float y, uint8_t *out) {
  if (x < 0.0f) x = 0.0f;
  if (y < 0.0f) y = 0.0f;
  if (x > (float)(w - 1)) x = (float)(w - 1);
  if (y > (float)(h - 1)) y = (float)(h - 1);
  int x0 = (int)x, y0 = (int)y;
  int x1 = x0 + 1 < w ? x0 + 1 : x0;
  int y1 = y0 + 1 < h ? y0 + 1 : y0;
  float fx = x - (float)x0, fy = y - (float)y0;
  for (int c = 0; c < 3; c++) {
    float a = img[((size_t)y0 * (size_t)w + (size_t)x0) * 4 + c] * (1.0f - fx) +
              img[((size_t)y0 * (size_t)w + (size_t)x1) * 4 + c] * fx;
    float b = img[((size_t)y1 * (size_t)w + (size_t)x0) * 4 + c] * (1.0f - fx) +
              img[((size_t)y1 * (size_t)w + (size_t)x1) * 4 + c] * fx;
    out[c] = (uint8_t)(a * (1.0f - fy) + b * fy + 0.5f);
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::printf("uso: %s imagen.ppm [cuanto_se_separan 0.09] [plano_quieto 0.5]\n", argv[0]);
    return 1;
  }
  std::vector<uint8_t> img;
  int w = 0, h = 0;
  std::string err;
  if (!leePpm(argv[1], img, w, h, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  float ganancia = argc > 2 ? (float)std::atof(argv[2]) : 0.09f;
  float referencia = argc > 3 ? (float)std::atof(argv[3]) : 0.5f;

  const char *redg = std::getenv("RED");
  DepthNet red;
  if (!red.init("", &err, redg ? std::atoi(redg) : 0)) {
    std::fprintf(stderr, "[conversor] %s\n", err.c_str());
    return 1;
  }
  std::vector<uint8_t> mapa;
  if (!red.estimate(img.data(), w, h, mapa, &err)) {
    std::fprintf(stderr, "[conversor] %s\n", err.c_str());
    return 1;
  }
  const int msz = red.size();
  std::printf("[prueba] imagen %dx%d, mapa %dx%d en %.1f ms (%s)\n", w, h, msz, msz, red.lastMs(),
              red.device().c_str());

  std::vector<uint8_t> izq((size_t)w * (size_t)h * 3), der((size_t)w * (size_t)h * 3), ana((size_t)w * (size_t)h * 3);
  double difAcum = 0.0;
  double shiftPelota = 0.0, shiftFondo = 0.0;
  int nPelota = 0, nFondo = 0;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      /* el mapa viene a la resolucion del modelo: lo estiramos igual que en la pantalla */
      float u = (float)x / (float)w, v = (float)y / (float)h;
      int mx = (int)(u * (float)msz), my = (int)(v * (float)msz);
      if (mx >= msz) mx = msz - 1;
      if (my >= msz) my = msz - 1;
      float cerca = (float)mapa[(size_t)my * (size_t)msz + (size_t)mx] / 255.0f;
      float s = (cerca - referencia) * ganancia * (float)w;   /* desplazamiento en pixeles */
      uint8_t a[3], b[3];
      muestrea(img, w, h, (float)x - s, (float)y, a);   /* ojo izquierdo: el contenido va a la derecha */
      muestrea(img, w, h, (float)x + s, (float)y, b);   /* ojo derecho: hacia la izquierda */
      size_t o = ((size_t)y * (size_t)w + (size_t)x) * 3;
      izq[o] = a[0]; izq[o + 1] = a[1]; izq[o + 2] = a[2];
      der[o] = b[0]; der[o + 1] = b[1]; der[o + 2] = b[2];
      ana[o] = a[0]; ana[o + 1] = b[1]; ana[o + 2] = b[2];
      difAcum += std::abs((int)a[0] - (int)b[0]) + std::abs((int)a[1] - (int)b[1]) + std::abs((int)a[2] - (int)b[2]);
      if (cerca > 0.9f) { shiftPelota += s; nPelota++; }
      if (cerca < 0.1f) { shiftFondo += s; nFondo++; }
    }
  }
  escribePpm("/tmp/ojo_izq.ppm", izq, w, h);
  escribePpm("/tmp/ojo_der.ppm", der, w, h);
  escribePpm("/tmp/anaglifo.ppm", ana, w, h);
  std::printf("[prueba] diferencia media entre ojos %.2f por canal\n", difAcum / ((double)w * (double)h * 3.0));
  std::printf("[prueba] desplazamiento (positivo = sale de la pantalla): cerca %.1f px (%d puntos), fondo %.1f px (%d puntos)\n",
              nPelota ? shiftPelota / nPelota : 0.0, nPelota, nFondo ? shiftFondo / nFondo : 0.0, nFondo);
  std::printf("[prueba] escritos /tmp/ojo_izq.ppm /tmp/ojo_der.ppm /tmp/anaglifo.ppm\n");
  return 0;
}
