/* prueba_red.cpp — comprobar la clase DepthNet sola: fabrica una escena sencilla (una pelota
   delante de un muro) y escribe la foto y el mapa de profundidad como PPM para verlos a ojo.
   g++ -O2 -DHAVE_ORT -Isrc -I/usr/include/onnxruntime tools/prueba_red.cpp src/depth.cpp \
       -lonnxruntime -ldl -o /tmp/red  &&  /tmp/red /tmp/escena.ppm /tmp/profundidad.pgm       */
#include "depth.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static void writePpm(const char *path, int w, int h, const std::vector<uint8_t> &rgb) {
  FILE *f = std::fopen(path, "wb");
  if (!f) return;
  std::fprintf(f, "P6\n%d %d\n255\n", w, h);
  std::fwrite(rgb.data(), 1, rgb.size(), f);
  std::fclose(f);
}

static void writePgm(const char *path, int w, int h, const std::vector<uint8_t> &g) {
  FILE *f = std::fopen(path, "wb");
  if (!f) return;
  std::fprintf(f, "P5\n%d %d\n255\n", w, h);
  std::fwrite(g.data(), 1, g.size(), f);
  std::fclose(f);
}

int main(int argc, char **argv) {
  const int W = 640, H = 360;
  std::vector<uint8_t> img((size_t)W * H * 4, 0);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      uint8_t *p = &img[((size_t)y * W + x) * 4];
      p[0] = (uint8_t)(60 + 120 * y / H);       /* muro con degradado */
      p[1] = (uint8_t)(70 + 100 * y / H);
      p[2] = (uint8_t)(90 + 80 * y / H);
      p[3] = 255;
    }
  /* mesa: banda clara en la parte de abajo */
  for (int y = (int)(H * 0.72); y < H; y++)
    for (int x = 0; x < W; x++) {
      uint8_t *p = &img[((size_t)y * W + x) * 4];
      const float t = (float)(y - H * 0.72) / (float)(H * 0.28);
      p[0] = (uint8_t)(120 + 90 * t);
      p[1] = (uint8_t)(100 + 70 * t);
      p[2] = (uint8_t)(70 + 40 * t);
    }
  /* pelota delante de todo */
  const int cx = W / 2, cy = H / 2, r = 95;
  for (int y = cy - r; y <= cy + r; y++)
    for (int x = cx - r; x <= cx + r; x++) {
      const float d = std::sqrt((float)(x - cx) * (x - cx) + (float)(y - cy) * (y - cy));
      if (d > (float)r) continue;
      const float sh = 1.0f - 0.5f * (d / (float)r);
      uint8_t *p = &img[((size_t)y * W + (size_t)x) * 4];
      p[0] = (uint8_t)(220 * sh);
      p[1] = (uint8_t)(70 * sh);
      p[2] = (uint8_t)(60 * sh);
    }

  DepthNet net;
  std::string err;
  if (!net.init(argc > 3 ? argv[3] : "", &err)) {
    std::fprintf(stderr, "[conversor] %s\n", err.c_str());
    return 1;
  }
  std::vector<uint8_t> depth;
  const int veces = argc > 4 ? std::atoi(argv[4]) : 3;   /* con ODYSSEY3D_MEDIR=1 ensena el reparto */
  for (int i = 0; i < veces; i++)
    if (!net.estimate(img.data(), W, H, depth, &err)) {
      std::fprintf(stderr, "[conversor] fallo: %s\n", err.c_str());
      return 1;
    }
  std::printf("[prueba] mapa %dx%d en %.1f ms (%s)\n", net.size(), net.size(), net.lastMs(), net.device().c_str());

  /* centro de la pelota y de la esquina superior izquierda: la pelota debe estar mas cerca */
  const int s = net.size();
  const int ballX = (int)((float)cx / W * s), ballY = (int)((float)cy / H * s);
  std::printf("[prueba] pelota %.2f  muro %.2f  mesa %.2f  (mas cerca = mas cerca de ti)\n",
              depth[(size_t)ballY * s + ballX] / 255.0, depth[20 * s + 20] / 255.0,
              depth[(size_t)(s - 10) * s + s / 2] / 255.0);

  writePpm(argc > 1 ? argv[1] : "/tmp/escena.ppm", W, H, img);
  writePgm(argc > 2 ? argv[2] : "/tmp/profundidad.pgm", s, s, depth);
  return 0;
}
