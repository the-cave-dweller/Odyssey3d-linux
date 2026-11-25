/* prueba_cambio.cpp — reproduce el cambio de red con el boton 1/2 del visor:
   abre la nuestra en la grafica y pasa a la de Samsung, que con la tarjeta justa
   se va a la CPU. Es el camino del cuelgue del 24-nov.
   g++ -O2 -DHAVE_ORT -Isrc -I/usr/include/onnxruntime tools/prueba_cambio.cpp src/depth.cpp \
       src/util.cpp -lonnxruntime -ldl -o /tmp/cambio && /tmp/cambio RUTA_NUESTRA RUTA_SAMSUNG   */
#include "depth.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char **argv) {
  const int W = 640, H = 360;
  std::vector<uint8_t> img((size_t)W * H * 4, 0);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      uint8_t *p = &img[((size_t)y * W + x) * 4];
      p[0] = (uint8_t)(60 + 120 * y / H);
      p[1] = (uint8_t)(70 + 100 * y / H);
      p[2] = (uint8_t)(90 + 80 * y / H);
      p[3] = 255;
    }
  DepthNet net;
  std::string err;
  if (!net.init(argc > 1 ? argv[1] : "", &err)) {
    std::fprintf(stderr, "[paso 1] %s\n", err.c_str());
    return 1;
  }
  std::vector<uint8_t> depth;
  if (!net.estimate(img.data(), W, H, depth, &err)) {
    std::fprintf(stderr, "[paso 1] estimacion: %s\n", err.c_str());
    return 1;
  }
  std::printf("[paso 1] abierta %s en %s, estimacion %.1f ms\n", net.modelPath().c_str(),
              net.device().c_str(), net.lastMs());
  std::fflush(stdout);
  if (argc > 3)
    setenv("ODYSSEYVK_GPU", "-1", 1);   /* a partir de aqui: que la nueva red vaya por CPU */
  std::printf("[paso 2] cambio a %s (ODYSSEYVK_GPU=%s)\n", argv[2] ? argv[2] : "?",
              getenv("ODYSSEYVK_GPU") ? getenv("ODYSSEYVK_GPU") : "sin");
  std::fflush(stdout);
  if (!net.cambiar(argv[2] ? argv[2] : "", &err)) {
    std::fprintf(stderr, "[paso 2] %s\n", err.c_str());
    std::printf("[paso 2] seguimos con %s en %s, viva=%d\n", net.modelPath().c_str(),
                net.device().c_str(), (int)net.ready());
    return 2;
  }
  std::printf("[paso 2] abierta %s en %s\n", net.modelPath().c_str(), net.device().c_str());
  if (!net.estimate(img.data(), W, H, depth, &err)) {
    std::fprintf(stderr, "[paso 3] estimacion: %s\n", err.c_str());
    return 3;
  }
  std::printf("[paso 3] estimacion %.1f ms en %s\n", net.lastMs(), net.device().c_str());
  return 0;
}
