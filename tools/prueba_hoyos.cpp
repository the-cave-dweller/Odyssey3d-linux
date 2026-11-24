/* prueba_hoyos.cpp — medir el relleno de huecos sin mirar el panel.
   Monta un mapa sinteticos con un canto duro (un objeto cercano delante de un fondo), lo pasa por
   el relleno de vistas y compara cuanto se cruza de banda el shader al ir a buscar el color.

   El color siempre se lee de la imagen en crudo; lo que cambia es DESDE DONDE se lee. Si el punto
   de destino esta en el objeto pero la lectura cae del lado del fondo, ahi sale un fantasma: el
   canto babbeado. Eso es lo que cuenta esta herramienta.

   compilation y uso (fuera del panel):
     g++ -O2 -fopenmp -Isrc tools/prueba_hoyos.cpp src/hoyos.cpp -o /tmp/hoyos
     /tmp/hoyos [ancho] [ganancia] [plano]
*/
#include "hoyos.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

/* escena sintetica: fondo a 120, un patron de barras (para tener varios cantos) a 220,
   y un circulo cercano a 255. Todos los cantos son duros, que es donde se ven los problemas. */
void escena(std::vector<uint8_t> *m, int n) {
  m->assign((size_t)n * (size_t)n, 0);
  for (int y = 0; y < n; y++) {
    for (int x = 0; x < n; x++) {
      uint8_t v = 120;
      if ((x / (n / 8)) % 2 == 1) v = 220;                       /* barras verticales */
      const float dx = (float)x - 0.5f * (float)n, dy = (float)y - 0.5f * (float)n;
      if (dx * dx + dy * dy < (0.12f * (float)n) * (0.12f * (float)n)) v = 255;  /* esfera */
      (*m)[(size_t)y * (size_t)n + (size_t)x] = v;
    }
  }
}

/* Cuantos puntos de destino leen al otro lado de un salto de profundidad.
   ojo = 0 izquierdo (la lectura va hacia la izquierda), 1 derecho.
   mapaLectura: el mapa del que sale el COLOR (siempre el crudo).
   mapaDestino: el mapa que usa el shader para decidir cuanto moverse. */
int cruces(const std::vector<uint8_t> &mapaLectura, const std::vector<uint8_t> &mapaDestino, int n,
           float gan, float plano, int ojo, double umbral) {
  int malos = 0;
  for (int y = 0; y < n; y++) {
    for (int x = 0; x < n; x++) {
      const size_t p = (size_t)y * (size_t)n + (size_t)x;
      const float s = ((float)mapaDestino[p] / 255.0f - plano) * gan;
      const float src = (float)x + (ojo ? s : -s);
      if (src < 0.0f || src >= (float)(n - 1)) continue;
      const int a = (int)std::floor(src);
      const float t = src - (float)a;
      const float dlect = (1.0f - t) * (float)mapaLectura[(size_t)y * (size_t)n + (size_t)a] +
                          t * (float)mapaLectura[(size_t)y * (size_t)n + (size_t)(a + 1)];
      if (std::fabs((double)dlect - (double)mapaDestino[p]) > umbral) malos++;
    }
  }
  return malos;
}

/* El punto de destino que si tiene algo detras: esparcir el mapa con la regla del mas cercano y
   quedarse solo con las casillas cubiertas. Eso es lo que tendria que salir (referencia), huecos
   aparte. */
void esparcir(const std::vector<uint8_t> &mapa, int n, float gan, float plano, int ojo,
              std::vector<uint8_t> *val, std::vector<uint8_t> *cubierta) {
  val->assign((size_t)n * n, 0);
  cubierta->assign((size_t)n * n, 0);
  for (int y = 0; y < n; y++) {
    for (int x = 0; x < n; x++) {
      const size_t p = (size_t)y * (size_t)n + (size_t)x;
      const float d = (float)mapa[p];
      float s = (d / 255.0f - plano) * gan;
      const float tope = 0.0035f * (float)n + 1.0f;
      if (s > tope) s = tope;
      if (s < -tope) s = -tope;
      const int x0 = (int)std::floor((float)x + (ojo ? -s : s));
      for (int k = 0; k < 2; k++) {
        const int xx = x0 + k;
        if (xx < 0 || xx >= n) continue;
        const size_t idx = (size_t)y * (size_t)n + (size_t)xx;
        if (!(*cubierta)[idx] || d > (*val)[idx]) { (*val)[idx] = (uint8_t)d; (*cubierta)[idx] = 1; }
      }
    }
  }
}

/* Cuanto se equivoca el shader yendo a buscar el color al destino (mapa) que le damos, comparado
   con la referencia de arriba. Solo cuentan las casillas que si tienen referencia. */
void errorContraReferencia(const std::vector<uint8_t> &color, const std::vector<uint8_t> &mapaShader,
                           const std::vector<uint8_t> &ref, const std::vector<uint8_t> &cubierta,
                           int n, float gan, float plano, int ojo, long *suma, int *gruesos, long *tot) {
  *suma = 0; *gruesos = 0; *tot = 0;
  for (int y = 0; y < n; y++) {
    for (int x = 0; x < n; x++) {
      const size_t p = (size_t)y * (size_t)n + (size_t)x;
      if (!cubierta[p]) continue;
      const float s = ((float)mapaShader[p] / 255.0f - plano) * gan;
      const float src = (float)x + (ojo ? s : -s);
      if (src < 0.0f || src >= (float)(n - 1)) continue;
      const int a = (int)std::floor(src);
      const float t = src - (float)a;
      const float lec = (1.0f - t) * (float)color[(size_t)y * (size_t)n + (size_t)a] +
                        t * (float)color[(size_t)y * (size_t)n + (size_t)(a + 1)];
      const long e = (long)std::fabs((double)lec - (double)ref[p]);
      *suma += e;
      if (e > 40) (*gruesos)++;
      (*tot)++;
    }
  }
}

}  // namespace

int main(int argc, char **argv) {
  const int n = argc > 1 ? std::atoi(argv[1]) : 518;
  const float gan = argc > 2 ? (float)std::atof(argv[2]) : 3.06f;   /* convGain_ x ancho del mapa */
  const float plano = argc > 3 ? (float)std::atof(argv[3]) : 0.5f;
  const double umbral = argc > 4 ? std::atof(argv[4]) : 40.0;

  std::vector<uint8_t> mapa, rg((size_t)2 * n * n), planoViejo((size_t)n * n);
  escena(&mapa, n);
  for (size_t i = 0; i < mapa.size(); i++) planoViejo[i] = mapa[i];

  const int puntos = n * n;
  const std::vector<uint8_t> color = mapa;   /* el color del cuadro sinteticos es el propio mapa */
  const auto t0 = std::chrono::steady_clock::now();
  int huecos = 0;
  for (int p = 0; p < 20; p++) huecos = rellenarVistas(mapa.data(), n, gan, plano, rg.data(), nullptr);
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 20.0;
  std::vector<uint8_t> izq((size_t)n * n), der((size_t)n * n);
  for (size_t i = 0; i < mapa.size(); i++) { izq[i] = rg[2 * i]; der[i] = rg[2 * i + 1]; }

  std::printf("mapa %d x %d · ganancia %.2f puntos de mapa · plano %.2f\n", n, n, gan, plano);
  std::printf("huecos que hizo falta rellenar: %d de %d puntos (%.2f%%) · %.2f ms por cuadro\n", huecos,
              2 * puntos, 100.0 * (double)huecos / (double)(2 * puntos), ms);
  for (int ojo = 0; ojo < 2; ojo++) {
    std::vector<uint8_t> ref, cub;
    esparcir(mapa, n, gan, plano, ojo, &ref, &cub);
    const std::vector<uint8_t> &nuevo = ojo ? der : izq;
    long s1 = 0, s2 = 0, tot = 0; int g1 = 0, g2 = 0;
    errorContraReferencia(color, planoViejo, ref, cub, n, gan, plano, ojo, &s1, &g1, &tot);
    errorContraReferencia(color, nuevo, ref, cub, n, gan, plano, ojo, &s2, &g2, &tot);
    std::printf("ojo %s (%ld puntos con referencia): error medio %.3f -> %.3f · gordos %d -> %d\n",
                ojo ? "derecho" : "izquierdo", tot, (double)s1 / (double)tot, (double)s2 / (double)tot, g1, g2);
  }
  const int crucesViejo = cruces(mapa, planoViejo, n, gan, plano, 0, umbral) +
                          cruces(mapa, planoViejo, n, gan, plano, 1, umbral);
  const int crucesNuevo = cruces(mapa, izq, n, gan, plano, 0, umbral) +
                          cruces(mapa, der, n, gan, plano, 1, umbral);
  std::printf("lecturas al otro lado de un salto: %d -> %d\n", crucesViejo, crucesNuevo);

  /* una fila por la mitad, alrededor de un canto, para ver la forma con los ojos */
  const int y = n / 2;
  int x0 = 0;
  while (x0 < n && mapa[(size_t)y * (size_t)n + (size_t)x0] == 120) x0++;
  x0 = x0 > 6 ? x0 - 6 : 0;
  std::printf("\nfila %d alrededor del canto de una barra (crudo / ojo izq / ojo der)\n", y);
  for (int x = x0; x < x0 + 16 && x < n; x++) {
    const size_t p = (size_t)y * (size_t)n + (size_t)x;
    std::printf("x=%3d  crudo %3d  izq %3d  der %3d\n", x, mapa[p], izq[p], der[p]);
  }
  return 0;
}
