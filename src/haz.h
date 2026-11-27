#pragma once
/* haz.h — (v1.18) ShaderBeam en el visor 3D: el «Blur Busters CRT Beam Simulator» a cada refresco del panel.
   Rodolfo, señalando https://github.com/mausimus/ShaderBeam: «create this one too».

   ShaderBeam (mausimus, MIT) es un programa de Windows que captura el escritorio y lo vuelve a pintar a pantalla completa
   a cada refresco del monitor con el simulador del haz de un CRT de Mark Rejhon (Blur Busters) y Timothy Lottes (MIT
   2024), o con un BFI simple (un refresco con imagen y los demas en negro). Un panel LCD u OLED deja cada cuadro quieto
   todo su tiempo (muestra y retencion) y el ojo, que sigue lo que se mueve, lo ve corrido: a 60 cuadros, 16,7 ms de
   estela. El simulador enciende en cada refresco solo la franja por donde pasaria el haz y reparte lo que no cabe (los
   pixeles claros) en los refrescos siguientes: mucha menos estela, a cambio de brillo.

   No es un efecto de ReShade: los efectos corren una vez por cuadro del juego y esto tiene que correr una vez por
   refresco del panel (2,75 refrescos por cuadro con un juego a 60 en el G90XF a 165 Hz). Por eso va en la trama
   (shaders/weave.frag, haz()), que el visor ya presenta al ritmo del panel (FIFO), y no en la cadena de efectos.
   Del ShaderBeam se trae: los dos modos, la ganancia contra el desenfoque, la gamma, la direccion del barrido, el desliz
   contra la retencion de los LCD (solo cuando los refrescos por cuadro son un entero par, como hace su «Auto") y el
   cambio de cuadro solo al empezar cada refresco del CRT (su NewInputRequired). Su modo «frame-ahead»: los tres cuadros
   que lee el sombreador son el mismo, el ultimo. No se trae su captura (aqui la imagen ya esta en el visor), ni la
   camara lenta, ni el banco de pruebas, ni el uso de una segunda tarjeta.

   Los ajustes van con los de los efectos (efectos.h: all.conf o el del juego), con estas claves:
     shaderbeam = si|no   shaderbeam.modo = crt|bfi   shaderbeam.hz = 60   shaderbeam.ganancia = 0.5
     shaderbeam.gamma = 2.2   shaderbeam.barrido = 1 (0 ninguno, 1 arriba→abajo, 2 abajo→arriba, 3 izq→der, 4 der→izq) */
#include <cmath>
#include <cstdint>

namespace haz {

enum Modo { Apagado = 0, Crt = 1, BfiSimple = 2 };

struct Ajustes {
  bool puesto = false;      /* su pastilla en la pestana Efectos */
  int modo = Crt;           /* Crt o BfiSimple */
  float hz = 60.0f;         /* los Hz del CRT que se simula: los cuadros por segundo del juego */
  float ganancia = 0.5f;    /* «Gain vs Blur» de ShaderBeam: 1 = el brillo de siempre (los claros sin menos estela) */
  float gamma = 2.2f;       /* la del panel: si no cuadra, salen franjas horizontales que se mueven */
  int barrido = 1;          /* de arriba abajo */
  bool operator==(const Ajustes &o) const {
    return puesto == o.puesto && modo == o.modo && hz == o.hz && ganancia == o.ganancia && gamma == o.gamma &&
           barrido == o.barrido;
  }
  bool operator!=(const Ajustes &o) const { return !(*this == o); }
};

/* los topes de los deslizadores y de lo que se lee del archivo */
constexpr float kHzMin = 30.0f, kHzMax = 120.0f, kGananciaMin = 0.1f, kGananciaMax = 1.0f, kGammaMin = 1.5f, kGammaMax = 3.0f;
inline float topar(float v, float a, float b) { return std::isfinite(v) ? (v < a ? a : (v > b ? b : v)) : a; }

/* refrescos del panel por cada refresco del CRT (el FRAMES_PER_HZ del sombreador; no hace falta que sea entero). En un LCD
   (el G90XF lo es) con un numero entero PAR, el encendido coincide siempre con la misma polaridad de la inversion del
   panel y deja la imagen marcada: ShaderBeam le suma 0,001 («LCD Anti-retention», Auto) y el patron se va corriendo
   despacio. Con 165/60 = 2,75 no hace falta; con 82,5 Hz (2 refrescos) si */
inline float refrescosPorCrt(double hzPanel, double hzCrt) {
  if (!(hzPanel > 0.0) || !(hzCrt > 0.0)) return 1.0f;
  double n = hzPanel / hzCrt;
  if (n < 1.0) n = 1.0;
  const double r = std::floor(n + 0.5);
  if (std::fabs(n - r) < 1e-4 && ((long long)r % 2) == 0) n = r + 0.001;
  return (float)n;
}

/* lo que el sombreador necesita para el refresco numero `k` del panel (contados desde que se encendio), como lo calcula
   ShaderBeam en CRTBeamSimulatorShader::Render: en dobles, porque con floats fmod y floor fallan en algun cuadro suelto
   (su nota: «frame 3910») */
struct Fase {
  float raster = 0.0f;      /* donde va el haz, de 0 (arriba) a 1 */
  double ciclo = 0.0;       /* el refresco del CRT en que va (crtHzCounter) */
  bool cicloNuevo = true;   /* este refresco empieza uno del CRT: es cuando se toma el cuadro nuevo del juego */
};
inline Fase fase(uint64_t k, float n) {
  Fase f;
  const double N = n > 0.0f ? (double)n : 1.0;
  const double x = (double)k;
  f.raster = (float)(std::fmod(x, N) / N);
  f.ciclo = std::floor(x / N);
  f.cicloNuevo = k == 0 || std::floor((x - 1.0) / N) != f.ciclo;
  return f;
}

}  // namespace haz
