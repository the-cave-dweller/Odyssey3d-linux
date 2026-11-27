/* prueba_dlss_interruptor.cpp — el interruptor de la profundidad de DLSS al momento y el modo rendimiento sin DLSS
   (v1.18). Sin ventana ni tarjeta: la red en la CPU (ODYSSEYVK_GPU=-1) y la capa de mentira (tools/simula-capa.py) con
   su propia memoria compartida, asi que se puede correr con una sesion 3D abierta. Lo que dice el lector (si late o
   no), en el registro de simula-capa: «el visor lee» / «el visor dejo de leer».
     g++ -O2 -std=gnu++17 -fopenmp -DHAVE_ORT=1 -DHAVE_OPENCV=1 -Isrc -Icapa -I/usr/include/onnxruntime \
         -isystem /usr/include/opencv5 tools/prueba_dlss_interruptor.cpp build/CMakeFiles/odysseyvk.dir/src/{depth,profjuego,util}.cpp.o \
         /usr/lib/libonnxruntime.so -lopencv_dnn -lopencv_imgproc -lopencv_core -lopencv_photo -ldl -lpthread -o /tmp/interruptor
     export ODYSSEYVK_PROFUNDIDAD_SHM=/odysseyvk-profundidad-prueba
     python3 tools/simula-capa.py profundidad.r32f 1920 1080 --segundos 30 &
     ODYSSEYVK_GPU=-1 /tmp/interruptor ~/.local/share/odysseyvk/models/depth_anything_v2_small_fp16.onnx */
#include "depth.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

static int fallos = 0;
static void mira(bool ok, const char *que) {
  std::printf("%s  %s\n", ok ? "BIEN " : "FALLO", que);
  std::fflush(stdout);
  if (!ok) fallos++;
}

static double ahora() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

/* unas pasadas seguidas (la red en la CPU tarda): lo que quede en usandoJuego() es lo del ultimo cuadro */
static bool pasadas(DepthNet &net, const std::vector<uint8_t> &img, int W, int H, int n, std::vector<uint8_t> &mapa) {
  std::string err;
  for (int i = 0; i < n; i++)
    if (!net.estimate(img.data(), W, H, mapa, &err)) {
      std::printf("FALLO  estimate: %s\n", err.c_str());
      fallos++;
      return false;
    }
  return true;
}

static double diferencia(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b) {
  if (a.size() != b.size() || a.empty()) return -1.0;
  double s = 0;
  for (size_t i = 0; i < a.size(); i++) s += std::abs((int)a[i] - (int)b[i]);
  return s / (double)a.size();
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::fprintf(stderr, "prueba MODELO\n");
    return 2;
  }
  const int W = 640, H = 360;
  std::vector<uint8_t> img((size_t)W * H * 4, 255);
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      uint8_t *p = &img[((size_t)y * W + x) * 4];
      p[0] = (uint8_t)(40 + 180 * x / W), p[1] = (uint8_t)(60 + 120 * y / H), p[2] = (uint8_t)(200 - 150 * y / H);
      if ((x - 320) * (x - 320) + (y - 200) * (y - 200) < 80 * 80) p[0] = 230, p[1] = 60, p[2] = 50;
    }

  std::string err;
  std::vector<uint8_t> juego, red, otra;
  {
    DepthNet net;
    if (!net.init(argv[1], &err)) {
      std::printf("FALLO  init: %s\n", err.c_str());
      return 1;
    }
    std::printf("[prueba] red %s en %s\n", argv[1], net.device().c_str());
    net.setPermitirJuego(true);

    std::printf("[prueba] %.1f  1. interruptor de fabrica (sin orden): la de DLSS\n", ahora());
    pasadas(net, img, W, H, 2, juego);
    mira(net.usandoJuego(), "con la capa escribiendo, se usa la profundidad del juego");

    std::printf("[prueba] %.1f  2. orden profundidad_dlss = no\n", ahora());
    net.setUsarJuego(false);
    pasadas(net, img, W, H, 2, red);
    mira(!net.usandoJuego(), "en el cuadro siguiente ya no se usa la del juego");
    const double d1 = diferencia(juego, red);
    std::printf("[prueba] diferencia media juego/red: %.1f niveles de 255\n", d1);
    mira(d1 > 5.0, "el mapa es otro (el de la red, no el del juego)");
    int hw = 0, hh = 0;
    mira(!net.mascaraHud(otra, hw, hh), "sin la del juego tampoco se toma la mascara del HUD");
    std::printf("[prueba] %.1f  esperando 3 s sin leer: la capa tiene que ver que el visor dejo de leer\n", ahora());
    std::this_thread::sleep_for(std::chrono::seconds(3));
    pasadas(net, img, W, H, 1, red);   /* otra pasada con la red: tampoco late */
    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::printf("[prueba] %.1f  3. orden profundidad_dlss = si\n", ahora());
    net.setUsarJuego(true);
    pasadas(net, img, W, H, 1, otra);
    mira(net.usandoJuego(), "vuelve la del juego sin reiniciar nada");
    int k = 1;
    double dk = diferencia(juego, otra);
    std::printf("[prueba] tras %d cuadro: diferencia con el primer mapa del juego %.2f\n", k, dk);
    while (dk >= 1.0 && k < 40) {
      pasadas(net, img, W, H, 1, otra);
      dk = diferencia(juego, otra), k++;
      if (k <= 5 || k % 5 == 0 || dk < 1.0) std::printf("[prueba] tras %d cuadros: %.2f\n", k, dk);
    }
    mira(dk < 1.0, "y el mapa vuelve a ser el del juego (tras el suavizado entre cuadros)");
  }
  {
    std::printf("[prueba] %.1f  4. modo rendimiento (ODYSSEYVK_SIN_RED=imagen), con la capa escribiendo\n", ahora());
    ::setenv("ODYSSEYVK_SIN_RED", "imagen", 1);
    DepthNet net;
    if (!net.init(argv[1], &err)) {
      std::printf("FALLO  init sin red: %s\n", err.c_str());
      return 1;
    }
    net.setPermitirJuego(true);
    net.setUsarJuego(true);   /* aunque el interruptor diga que si */
    std::vector<uint8_t> sinRed;
    pasadas(net, img, W, H, 3, sinRed);
    mira(net.sinRed() != 0, "es el modo rendimiento");
    mira(!net.usandoJuego(), "el modo rendimiento no usa la profundidad de DLSS");
    mira(diferencia(juego, sinRed) > 5.0, "el mapa es el de la imagen, no el del juego");
    std::printf("[prueba] %.1f  esperando 3 s: tampoco tiene que latir\n", ahora());
    std::this_thread::sleep_for(std::chrono::seconds(3));
    pasadas(net, img, W, H, 2, sinRed);
    std::this_thread::sleep_for(std::chrono::seconds(3));
  }
  std::printf("[prueba] %.1f  fin · %d fallos\n", ahora(), fallos);
  return fallos ? 1 : 0;
}
