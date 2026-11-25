/* prueba_euro.cpp — el filtro del rastreo sin camara (v0.60).
   Simula una cabeza a 60 cm con el ruido de la deteccion y la pasa por lo mismo que hace
   HeadTracker::publish: mediana de tres y luego el filtro (el exponencial de antes, alfa 0,35 por
   cuadro, o el One-Euro de ahora). Mide dos cosas:
     - quieto: cuanto tiembla la pose filtrada, y eso traducido a ciclos de fase de la trama
       (a 60 cm: 5,239/60 = 0,087 ciclos por cm de x en el centro; 5,239*30/60^2 = 0,044 ciclos por cm
       de z en el borde, a 30 cm del centro). El ojo que debe ver negro tiene +-0,04 de margen.
     - en marcha: cuanto se queda atras la pose filtrada al mover la cabeza a 20 cm/s.
   El ruido: medido en el panel el 25-nov con la cara quieta a 60 cm, la camara da z con +-30 mm de
   dispersion cuadro a cuadro; x tiembla menos, pero la transformada al panel le pasa parte de z.
   Aqui x con 0,8 cm y z con 3 cm de desviacion (gaussiano), mas un 3 % de cuadros con un tiron de
   hasta 6 cm.

   compilacion y uso (fuera del panel):
     g++ -O2 -std=c++17 -Isrc tools/prueba_euro.cpp -o /tmp/euro && /tmp/euro [minCorte] [beta]
*/
#include "track.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

struct Resultado {
  double sdX = 0, sdZ = 0, faseCentro = 0, faseBorde = 0, retraso = 0;
};

static Resultado correr(bool euro, double minCorte, double beta) {
  std::mt19937 gen(12345);   /* la misma secuencia para los dos filtros */
  std::normal_distribution<double> rx(0.0, 0.8), rz(0.0, 3.0);
  std::uniform_real_distribution<double> u(0.0, 1.0);
  const double fps = 60.0, dt = 1.0 / fps;
  FiltroEuro f[2];
  for (FiltroEuro &e : f) { e.minCorte = minCorte; e.beta = beta; }
  std::vector<double> hx, hz;
  double lx = 0, lz = 0;
  bool listo = false;
  std::vector<double> qx, qz;
  double retraso = 0;
  /* 0-5 s quieto en (0, 60); 5-6 s se mueve en x a 20 cm/s; 6-8 s quieto en (20, 60) */
  for (int n = 0; n < (int)(8 * fps); n++) {
    const double t = n * dt;
    const double verdadX = t < 5 ? 0 : (t < 6 ? 20 * (t - 5) : 20);
    const double verdadZ = 60;
    double ox = verdadX + rx(gen), oz = verdadZ + rz(gen);
    if (u(gen) < 0.03) { ox += (u(gen) - 0.5) * 12; oz += (u(gen) - 0.5) * 12; }
    hx.push_back(ox);
    hz.push_back(oz);
    if (hx.size() > 3) { hx.erase(hx.begin()); hz.erase(hz.begin()); }
    std::vector<double> sx = hx, sz = hz;
    std::sort(sx.begin(), sx.end());
    std::sort(sz.begin(), sz.end());
    const double mx = sx[sx.size() / 2], mz = sz[sz.size() / 2];
    if (euro) {
      lx = f[0].paso(mx, t);
      lz = f[1].paso(mz, t);
    } else {
      lx = listo ? lx + (mx - lx) * 0.35 : mx;
      lz = listo ? lz + (mz - lz) * 0.35 : mz;
    }
    listo = true;
    if (t > 1 && t < 5) { qx.push_back(lx); qz.push_back(lz); }
    if (t >= 5 && t < 6.5) retraso = std::max(retraso, std::fabs(verdadX - lx));
  }
  auto sd = [](const std::vector<double> &v) {
    double m = 0, s = 0;
    for (double a : v) m += a;
    m /= v.size();
    for (double a : v) s += (a - m) * (a - m);
    return std::sqrt(s / v.size());
  };
  Resultado r;
  r.sdX = sd(qx);
  r.sdZ = sd(qz);
  r.faseCentro = r.sdX * 5.239 / 60.0;
  r.faseBorde = std::hypot(r.sdX * 5.239 / 60.0, r.sdZ * 5.239 * 30.0 / 3600.0);
  r.retraso = retraso;
  return r;
}

int main(int argc, char **argv) {
  const double minCorte = argc > 1 ? std::atof(argv[1]) : 0.6;
  const double beta = argc > 2 ? std::atof(argv[2]) : 0.01;
  const Resultado a = correr(false, minCorte, beta), b = correr(true, minCorte, beta);
  std::printf("                        temblor quieto (desv.)     fase que tiembla (ciclos)   retraso max a 20 cm/s\n");
  std::printf("exponencial alfa 0,35   x %.2f cm   z %.2f cm      centro %.3f  borde %.3f      %.1f cm\n", a.sdX, a.sdZ,
              a.faseCentro, a.faseBorde, a.retraso);
  std::printf("One-Euro %.2f / %.2f     x %.2f cm   z %.2f cm      centro %.3f  borde %.3f      %.1f cm\n", minCorte, beta,
              b.sdX, b.sdZ, b.faseCentro, b.faseBorde, b.retraso);
  std::printf("margen del ojo que debe ver negro: +-0,04 ciclos\n");
  return 0;
}
