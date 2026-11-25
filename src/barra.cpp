/* barra.cpp — ver barra.h. */
#include "barra.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
/* las zonas donde un reproductor pinta sus mandos: el cuarto de abajo y la franja de arriba */
constexpr float kZonaAbajo = 0.74f;    /* de aqui para abajo */
constexpr float kZonaArriba = 0.16f;   /* de aqui para arriba */
constexpr float kCentroIni = 0.32f, kCentroFin = 0.68f;
constexpr float kUmbralMin = 12.0f;    /* niveles de 255. MEDIDO: 22 escenas limpias dan hasta 11; la barra, de 26 para arriba */
constexpr float kNoDividido = 22.0f;   /* si el centro mide mas que esto, no hay dos imagenes */
constexpr int kFilasMin = 4;           /* una fila suelta no es una barra */
/* Aire sobre la primera fila pintada. Abajo es generoso a proposito: la barra de progreso va justo
   encima de los botones, y cuando esta casi llena (o casi vacia) sus dos mitades son iguales y no se
   detecta por si misma (MEDIDO con la barra al 97 %: la banda salia de 4,9 % y la barra quedaba
   fuera, a 5,5 % del borde). */
constexpr float kMargenAbajo = 0.03f, kMargenArriba = 0.018f;
constexpr double kSoltar = 0.45;       /* segundos sin verla para quitar la banda */
constexpr int kRadio = 5;              /* media caja del suavizado de cada fila */

/* Una fila de luminancia suavizada de la mitad que empieza en x0. La caja es ancha a proposito
   (2 * kRadio + 1 puntos de la copia chica, unos 80 pixeles de pantalla): la copia chica toma un
   punto de cada siete sin promediar, asi que el detalle fino (un subtitulo que ya viene en el video)
   sale distinto en cada mitad segun donde caiga la muestra. MEDIDO con caja de 5: un cuadro limpio
   con dos renglones de subtitulos daba 9..14 niveles en 18 filas, lo mismo que una barra. Lo que
   pinta el reproductor son grupos de botones de 40..55 puntos de ancho que estan en una mitad y en
   la otra no: eso sobrevive a la caja; el detalle fino, no. */
void filaSuave(const uint8_t *fila, int x0, int n, std::vector<float> &sal) {
  std::vector<float> lum((size_t)n);
  for (int x = 0; x < n; x++) {
    const uint8_t *p = fila + (size_t)(x0 + x) * 4u;
    lum[(size_t)x] = 0.25f * (float)p[0] + 0.5f * (float)p[1] + 0.25f * (float)p[2];
  }
  sal.resize((size_t)n);
  for (int x = 0; x < n; x++) {
    float s = 0.0f;
    int c = 0;
    for (int k = -kRadio; k <= kRadio; k++) {
      const int q = x + k;
      if (q < 0 || q >= n) continue;
      s += lum[(size_t)q];
      c++;
    }
    sal[(size_t)x] = s / (float)c;
  }
}

/* El residuo de una fila: cuanto se diferencian las dos mitades una vez casadas POR TROZOS. Cada
   trozo (un octavo de la mitad) busca su propio corrimiento, porque en un video dividido de verdad
   el relieve no es el mismo a lo largo de la fila: lo de delante se corre mas que el fondo. Con un
   solo corrimiento para la fila entera, una escena con algo cerca abajo (MEDIDO: un cuadro con un
   microfono en primer plano) daba 19 niveles en 18 filas y se tomaba por la barra del reproductor.
   Lo que pinta el reproductor no casa con ningun corrimiento: esta en una mitad y en la otra no. */
float residuoFila(const uint8_t *fila, int w) {
  const int mitad = w / 2;
  /* el relieve corre una mitad contra la otra: hasta un 3 % del ancho de la mitad */
  const int D = std::max(3, mitad * 3 / 100);
  const int trozo = std::max(12, mitad / 8);
  static thread_local std::vector<float> izq, der;
  filaSuave(fila, 0, mitad, izq);
  filaSuave(fila, mitad, mitad, der);
  float total = 0.0f;
  int cuenta = 0;
  for (int x0 = D; x0 < mitad - D; x0 += trozo) {
    const int x1 = std::min(mitad - D, x0 + trozo);
    float mejor = 1e9f;
    for (int d = -D; d <= D; d++) {
      float s = 0.0f;
      for (int x = x0; x < x1; x++) s += std::fabs(izq[(size_t)x] - der[(size_t)(x + d)]);
      mejor = std::min(mejor, s);
    }
    total += mejor;
    cuenta += x1 - x0;
  }
  return total / (float)std::max(1, cuenta);
}
}  // namespace

std::vector<float> residuoPorFila(const uint8_t *rgba, int w, int h, int cadaCentro) {
  std::vector<float> r((size_t)std::max(0, h), -1.0f);
  if (!rgba || w < 32 || h < 32) return r;
  const int finArriba = (int)std::lround(kZonaArriba * (float)h), iniAbajo = (int)std::lround(kZonaAbajo * (float)h);
  const int cIni = (int)std::lround(kCentroIni * (float)h), cFin = (int)std::lround(kCentroFin * (float)h);
  for (int y = 0; y < h; y++) {
    const bool zona = y < finArriba || y >= iniAbajo;
    const bool centro = y >= cIni && y < cFin && (cadaCentro <= 1 || (y - cIni) % cadaCentro == 0);
    if (!zona && !centro) continue;
    r[(size_t)y] = residuoFila(rgba + (size_t)y * (size_t)w * 4u, w);
  }
  return r;
}

void DetectorBarra::olvidar() {
  abajo_ = Zona{};
  arriba_ = Zona{};
  puesta_ = Filas2D{};
}

Filas2D DetectorBarra::analizar(const uint8_t *rgba, int w, int h, double ahora, Medida *m) {
  Medida med;
  const std::vector<float> r = residuoPorFila(rgba, w, h, 4);
  if (r.empty()) {
    if (m) *m = med;
    return puesta_;
  }
  const int finArriba = (int)std::lround(kZonaArriba * (float)h), iniAbajo = (int)std::lround(kZonaAbajo * (float)h);
  const int cIni = (int)std::lround(kCentroIni * (float)h), cFin = (int)std::lround(kCentroFin * (float)h);
  std::vector<float> centro;
  for (int y = cIni; y < cFin; y++)
    if (r[(size_t)y] >= 0.0f) centro.push_back(r[(size_t)y]);
  if (!centro.empty()) {
    std::nth_element(centro.begin(), centro.begin() + (long)(centro.size() / 2), centro.end());
    med.centro = centro[centro.size() / 2];
  }
  med.umbral = std::max(kUmbralMin, 3.0f * med.centro + 3.0f);
  med.dividido = med.centro <= kNoDividido;

  /* la primera fila pintada de cada zona: tiene que ir acompanada (dos de las cuatro siguientes) */
  auto acompanada = [&](int y, int paso) {
    int n = 0;
    for (int k = 0; k < 4; k++) {
      const int q = y + k * paso;
      if (q < 0 || q >= h) break;
      if (r[(size_t)q] > med.umbral) n++;
    }
    return n >= 2;
  };
  float extAbajo = 0.0f, extArriba = 0.0f;
  if (med.dividido) {
    int primera = -1;
    for (int y = iniAbajo; y < h; y++) {
      med.maxAbajo = std::max(med.maxAbajo, r[(size_t)y]);
      if (r[(size_t)y] <= med.umbral) continue;
      med.filasAbajo++;
      if (primera < 0 && acompanada(y, 1)) primera = y;
    }
    if (med.filasAbajo >= kFilasMin && primera >= 0)
      extAbajo = std::min(1.0f - kZonaAbajo, (float)(h - primera) / (float)h + kMargenAbajo);
    int ultima = -1;
    for (int y = finArriba - 1; y >= 0; y--) {
      med.maxArriba = std::max(med.maxArriba, r[(size_t)y]);
      if (r[(size_t)y] <= med.umbral) continue;
      med.filasArriba++;
      if (ultima < 0 && acompanada(y, -1)) ultima = y;
    }
    if (med.filasArriba >= kFilasMin && ultima >= 0)
      extArriba = std::min(kZonaArriba, (float)(ultima + 1) / (float)h + kMargenArriba);
  }

  auto llevar = [&](Zona &z, float ext) {
    if (ext > 0.0f) {
      z.seguidas++;
      z.ultima = ahora;
      if (z.seguidas >= 2) z.puesta = true;
      /* mientras dure se queda con la mayor: si no, el borde de la banda baila con cada analisis */
      z.extension = std::max(z.extension, ext);
    } else {
      z.seguidas = 0;
      if (z.ultima < 0.0 || ahora - z.ultima > kSoltar) {
        z.puesta = false;
        z.extension = 0.0f;
      }
    }
    return z.puesta ? z.extension : 0.0f;
  };
  puesta_.abajo = llevar(abajo_, extAbajo);
  puesta_.arriba = llevar(arriba_, extArriba);
  if (m) *m = med;
  return puesta_;
}
