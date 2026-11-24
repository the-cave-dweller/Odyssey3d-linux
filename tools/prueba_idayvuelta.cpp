/* prueba_idayvuelta.cpp — ida y vuelta de las dos redes, sin tocar el panel: abre la red de
   Samsung, pasa a la nativa, y vuelve a la de Samsung (que es el paso que no sale en el visor).
   En cada paso imprime cuanta memoria libre anuncia cada tarjeta ANTES y DESPUES, para ver donde
   se va el sitio y si de verdad hace falta el colchon de 450 MB que pedimos.
   g++ -O2 -DHAVE_ORT -Isrc -I/usr/include/onnxruntime tools/prueba_idayvuelta.cpp src/depth.cpp \
       src/util.cpp -lonnxruntime -ldl -o /tmp/idayvuelta && \
   /tmp/idayvuelta RUTA_SAMSUNG RUTA_NATIVA [gpu]                                          */
#include "depth.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

/* Cuanta memoria libre anuncia cada tarjeta. Es lo mismo que mira el conversor por dentro. */
static void memoria(const char *cuando) {
  FILE *p = popen("nvidia-smi --query-gpu=index,memory.free --format=csv,noheader,nounits "
                       "2>/dev/null", "r");
  if (!p) {
    std::printf("[mem] %s: sin nvidia-smi\n", cuando);
    std::fflush(stdout);
    return;
  }
  char linea[128];
  std::printf("[mem] %s:", cuando);
  while (std::fgets(linea, sizeof(linea), p)) {
    int idx = -1, lib = -1;
    if (std::sscanf(linea, "%d, %d", &idx, &lib) == 2)
      std::printf(" gpu%d libre %d MB", idx, lib);
  }
  pclose(p);
  std::printf("\n");
  std::fflush(stdout);
}

int main(int argc, char **argv) {
  const int W = 640, H = 360;
  std::vector<uint8_t> img((size_t)W * H * 4, 0);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      uint8_t *q = &img[((size_t)y * W + x) * 4];
      q[0] = (uint8_t)(60 + 120 * y / H);
      q[1] = (uint8_t)(70 + 100 * y / H);
      q[2] = (uint8_t)(90 + 80 * y / H);
      q[3] = 255;
    }
  const std::string samsung = argc > 1 ? argv[1] : "";
  const std::string nativa = argc > 2 ? argv[2] : "";
  const int gpu = argc > 3 ? std::atoi(argv[3]) : 0;

  DepthNet net;
  std::string err;
  std::vector<uint8_t> mapa;

  memoria("arranque");
  if (!net.init(samsung, &err, gpu)) {
    std::fprintf(stderr, "[paso 1] %s\n", err.c_str());
    return 1;
  }
  std::printf("[paso 1] Samsung abierta en %s\n", net.device().c_str());
  memoria("con Samsung abierta");
  if (!net.estimate(img.data(), W, H, mapa, &err)) {
    std::fprintf(stderr, "[paso 1] estimacion: %s\n", err.c_str());
    return 1;
  }
  std::printf("[paso 1] estimacion %.1f ms\n", net.lastMs());
  std::fflush(stdout);

  memoria("antes de pasar a la nativa");
  if (!net.cambiar(nativa, &err)) {
    std::fprintf(stderr, "[paso 2] %s\n", err.c_str());
    return 2;
  }
  std::printf("[paso 2] nativa abierta en %s\n", net.device().c_str());
  memoria("con la nativa abierta");
  if (!net.estimate(img.data(), W, H, mapa, &err)) {
    std::fprintf(stderr, "[paso 2] estimacion: %s\n", err.c_str());
    return 2;
  }
  std::printf("[paso 2] estimacion %.1f ms\n", net.lastMs());
  std::fflush(stdout);

  memoria("antes de volver a Samsung");
  const bool ok = net.cambiar(samsung, &err);
  memoria("despues del intento de volver a Samsung");
  if (!ok) {
    std::fprintf(stderr, "[paso 3] NO VOLVIO: %s\n", err.c_str());
    std::printf("[paso 3] seguimos con %s en %s\n", net.modelPath().c_str(), net.device().c_str());
    return 3;
  }
  std::printf("[paso 3] Samsung abierta otra vez en %s\n", net.device().c_str());
  if (!net.estimate(img.data(), W, H, mapa, &err)) {
    std::fprintf(stderr, "[paso 3] estimacion: %s\n", err.c_str());
    return 3;
  }
  std::printf("[paso 3] estimacion %.1f ms\n", net.lastMs());
  return 0;
}
