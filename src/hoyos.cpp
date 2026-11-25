/* hoyos.cpp — ver hoyos.h. El relleno es en el mapa de destino de cada ojo, no en el color.
   Sale barato porque el desplazamiento de la lente va por columnas: los huecos son horizontales,
   asi que esparcir es una fila a la vez y rellenar tambien. Con eso no hace falta una piramide
   push-pull de dos dimensiones (que es lo que hace el inpaint.push/pull de Samsung) ni dos
   texturas mas en la grafica. */
#include "hoyos.h"
#include "util.h"   /* entornoOdyssey: la variable de entorno vale con el nombre nuevo y con el viejo */

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

bool hoyosActivo() {
  static const bool activo = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_HOYOS");
    return !e || e[0] != '0';
  }();
  return activo;
}

namespace {

uint8_t aByte(float v) {
  if (v <= 0.0f) return 0;
  if (v >= 255.0f) return 255;
  return (uint8_t)(v + 0.5f);
}

/* Rellenar los huecos de una fila: entre dos puntos validos se interpola pesando por la distancia
   (un hueco de dos puntos se queda casi en el valor del vecino mas cercano), y donde solo hay un
   lado valido se replica. Es el equivalente, recortado a una dimension, de inpaint.push/pull. */
void rellenarFila(float *v, uint8_t *ok, int n) {
  int i = 0;
  while (i < n) {
    if (ok[i]) { i++; continue; }
    int b = i;
    while (b < n && !ok[b]) b++;           /* primer valido a la derecha */
    const int a = i - 1;                   /* ultimo valido a la izquierda */
    if (a >= 0 && b < n) {
      const float va = v[a], vb = v[b];
      const float span = (float)(b - a);
      for (int k = a + 1; k < b; k++) {
        const float t = (float)(k - a) / span;
        v[k] = va * (1.0f - t) + vb * t;
        ok[k] = 2;
      }
    } else if (a >= 0) {
      for (int k = a + 1; k < b; k++) { v[k] = v[a]; ok[k] = 2; }
    } else if (b < n) {
      for (int k = i; k < b; k++) { v[k] = v[b]; ok[k] = 2; }
    }
    /* la fila entera sin ningun punto valido se queda como estaba: no hay nada que inventar */
    i = b > i ? b : i + 1;
  }
}

}  // namespace

int rellenarVistas(const uint8_t *mapa, int n, float ganMapPx, float plano, uint8_t *rg, int *hoyos) {
  if (!mapa || !rg || n < 2) return 0;
  const size_t plane = (size_t)n * (size_t)n;
  /* El desplazamiento de un ojo no pasa del tope oficial (+12,67 puntos de pantalla sobre 3840),
     que en un mapa de 518 son 1,7 puntos: con dos celdas por punto esparcido sobra. El +1 de
     holgura es para mapas de otro tamano. */
  const float tope = 0.0035f * (float)n + 1.0f;
  /* Los tres tampones se quedan entre cuadros: separarlos cada cuadro costaba un reparto de
     megabyte y medio por subida de mapa. Si cambia el tamano del mapa, se vuelven a tomar. */
  static std::vector<float> val;
  static std::vector<uint8_t> ok, salida;
  if (val.size() != plane) { val.resize(plane); ok.resize(plane); salida.resize(plane); }
  int rellenos = 0;

  for (int ojo = 0; ojo < 2; ojo++) {
    const float signo = ojo ? -1.0f : 1.0f;   /* ojo izquierdo: el contenido va hacia la derecha */
    std::fill(val.begin(), val.end(), 0.0f);
    std::fill(ok.begin(), ok.end(), (uint8_t)0);

    /* Esparcir el mapa en su sitio de destino. Como el desplazamiento es solo horizontal, cada
       punto escribe en su propia fila: se puede ir en hebras por filas sin que dos hebras se
       pisen la misma casilla. */
#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int y = 0; y < n; y++) {
      for (int x = 0; x < n; x++) {
        const size_t p = (size_t)y * (size_t)n + (size_t)x;
        const float d = (float)mapa[p];
        float s = (d / 255.0f - plano) * ganMapPx;
        if (s > tope) s = tope;
        if (s < -tope) s = -tope;
        const int x0 = (int)std::floor((float)x + signo * s);
        for (int k = 0; k < 2; k++) {
          const int xx = x0 + k;
          if (xx < 0 || xx >= n) continue;
          const size_t idx = (size_t)y * (size_t)n + (size_t)xx;
          /* Gana la superficie mas cercana: si el canto de un objeto y el fondo caen en el mismo
             sitio del destino, el objeto manda. Ahi es donde sin esta guarda el fondo se subia
             por encima del canto y el borde salia babbeado. */
          if (!ok[idx] || d > val[idx]) { val[idx] = d; ok[idx] = 1; }
        }
      }
    }

    int faltan = 0;
#ifdef _OPENMP
#pragma omp parallel for schedule(static) reduction(+:faltan)
#endif
    for (int y = 0; y < n; y++) {
      float *v = val.data() + (size_t)y * (size_t)n;
      uint8_t *o = ok.data() + (size_t)y * (size_t)n;
      uint8_t *q = salida.data() + (size_t)y * (size_t)n;
      for (int x = 0; x < n; x++) {
        if (!o[x]) faltan++;
        else q[x] = aByte(v[x]);
      }
      rellenarFila(v, o, n);
      for (int x = 0; x < n; x++)
        if (o[x] == 2) q[x] = aByte(v[x]);
    }
    for (size_t i = 0; i < plane; i++) rg[2 * i + (size_t)ojo] = salida[i];
    rellenos += faltan;
  }
  if (hoyos) *hoyos = rellenos;
  return rellenos;
}
