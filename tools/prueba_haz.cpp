/* prueba_haz.cpp — (v1.18) ShaderBeam sin tarjeta: las cuentas de src/haz.h y la de haz() de shaders/weave.frag, pasadas
   a C++ tal cual, para muchos refrescos por cuadro y muchos grises. Mide:
     - que la luz de un ciclo entero del CRT (en lineal) es la ganancia por la de siempre, para cada gris y cada altura
       de la pantalla: si no, salen franjas (lo que el original llama «seamless»);
     - cuanto se enciende cada refresco como mucho (el pico) y en cuantos refrescos se reparte un blanco;
     - cuando se toma el cuadro nuevo del juego (fase().cicloNuevo) y el desliz del LCD con un numero par.
   MEDIDO (27-nov): desde el gris 32/255 se aparta menos de un 1 %; por debajo del 16/255 se pierde luz (el 8/255 a gamma 2,2
   da un 27 % menos de muy poco). Es del original: su paso a sRGB con clamp (pensado para gamma 2,4) aplasta los casi negros.
   No se toca: es lo de ShaderBeam, y son sombras que apenas se ven.
   La medida con el sombreador de verdad es la del volcado (vkview.cpp, medirHaz).

   compilacion y uso (fuera del panel):
     g++ -O2 -std=c++17 -Isrc tools/prueba_haz.cpp -o /tmp/prueba_haz && /tmp/prueba_haz
*/
#include "haz.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

static double aLineal(double c, double g) { return c > 0.04045 ? std::pow(c / 1.055 + 0.055 / 1.055, g) : c / 12.92; }
static double aSrgb(double c, double g) { return std::min(std::max(0.0031308 * 12.92, c * 12.92), std::pow(c, 1.0 / g) * 1.055 - 0.055); }
/* haz() de weave.frag, un canal */
static double haz1(double c, double tubo, double N, double raster, double ganancia, double g) {
  const double L = aLineal(c, g) * N * ganancia;
  if (L <= 0.0) return 0.0;
  const double tc = tubo * N, fIni = raster * N, fFin = fIni + 1.0;
  double r = 0.0;
  for (int k = -1; k <= 1; k++) {
    const double ini = tc + k * N;
    r += std::max(0.0, std::min(ini + L, fFin) - std::max(ini, fIni));
  }
  return std::clamp(aSrgb(r, g), 0.0, 1.0);
}

int main() {
  const double g = 2.2, ganancia = 0.5;
  int fallos = 0;
  std::printf("refrescos por cuadro · la luz media de un ciclo / la de siempre (tiene que dar %.2f) · lo que mas se aparta · pico\n", ganancia);
  for (double hzCrt : {55.0, 60.0, 72.0, 82.5, 100.0}) {
    const float N = haz::refrescosPorCrt(165.0, hzCrt);
    /* ciclos que acaban justo en un refresco (con el desliz no hay: se toman muchos) */
    int ciclos = 1;
    double mejor = 1e9;
    for (int c = 1; c <= 4000; c++) {
      const double d = std::fabs(c * (double)N - std::floor(c * (double)N + 0.5));
      if (d < mejor - 1e-9) mejor = d, ciclos = c;
      if (d < 1e-6) break;
    }
    const int K = (int)std::floor(ciclos * (double)N + 0.5);
    double peor = 0.0, pico = 0.0;
    for (int gris = 32; gris <= 256; gris += 32)
      for (int fila = 0; fila < 64; fila++) {
        const double c = std::min(gris, 255) / 255.0, tubo = (fila + 0.5) / 64.0;
        double suma = 0.0;
        for (int k = 0; k < K; k++) {
          const double v = aLineal(haz1(c, tubo, N, haz::fase((uint64_t)k, N).raster, ganancia, g), g);
          suma += v;
          pico = std::max(pico, v / std::max(1e-9, aLineal(c, g)));
        }
        const double rel = suma / K / aLineal(c, g);
        peor = std::max(peor, std::fabs(rel - ganancia));
      }
    /* el cuadro nuevo: al empezar cada ciclo del CRT */
    int nuevos = 0;
    for (int k = 0; k < K; k++) nuevos += haz::fase((uint64_t)k, N).cicloNuevo ? 1 : 0;
    const bool bien = peor < 0.01 && nuevos == ciclos;
    fallos += bien ? 0 : 1;
    std::printf("  %5.1f Hz: %.3f refrescos%s · %d refrescos = %d ciclos, %d cuadros nuevos · se aparta %.4f · pico %.2f %s\n", hzCrt,
                (double)N, std::fabs(165.0 / hzCrt - N) > 1e-6 ? " (con el desliz del LCD)" : "", K, ciclos, nuevos, peor, pico,
                bien ? "bien" : "MAL");
  }
  /* los casi negros, solo para verlos: lo que se aparta a 60 Hz */
  {
    const float N = haz::refrescosPorCrt(165.0, 60.0);
    std::printf("los casi negros a 60 Hz (del original, no cuenta como fallo):");
    for (int gris : {4, 8, 16, 24}) {
      double peor = 0.0;
      for (int fila = 0; fila < 64; fila++) {
        const double c = gris / 255.0, tubo = (fila + 0.5) / 64.0;
        double suma = 0.0;
        for (int k = 0; k < 11; k++) suma += aLineal(haz1(c, tubo, N, haz::fase((uint64_t)k, N).raster, ganancia, g), g);
        peor = std::max(peor, std::fabs(suma / 11 / aLineal(c, g) - ganancia));
      }
      std::printf("  %d/255: %.3f", gris, peor);
    }
    std::printf("\n");
  }
  /* un blanco a 60: en cuantos refrescos seguidos se enciende un mismo punto (lo que reparte el fosforo) */
  {
    const float N = haz::refrescosPorCrt(165.0, 60.0);
    int encendidos = 0;
    for (int k = 0; k < 11; k++) encendidos += haz1(1.0, 0.5, N, haz::fase((uint64_t)k, N).raster, ganancia, g) > 0.0 ? 1 : 0;
    std::printf("un blanco a media altura, a 60 Hz: encendido en %d de 11 refrescos (4 ciclos)\n", encendidos);
  }
  std::printf("%s\n", fallos ? "HAY FALLOS" : "todo bien");
  return fallos ? 1 : 0;
}
