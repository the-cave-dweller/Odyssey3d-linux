/* rendimiento.h — los tres preajustes del modo rendimiento (rama experimental, 26-nov).
   MEDIDO con `odysseyvk mapa` sin red sobre tres cuadros (persona, nave, globo), con la Salida en 50 y
   `relieve = 2,00`: con ganancia y topes multiplicados por lo mismo, el 43-57 % del cuadro quedaba
   aplastado contra los topes (relieve de recortes planos). Con los topes por encima de la ganancia
   casi no se aplasta nada y la separacion entre los ojos crece:
     suave:   ganancia x0,75 · topes x2    -> ~3 % aplastado, 28 px (p95) / 32 px (max) entre los ojos
     medio:   ganancia x1    · topes x2,75 -> ~3 % aplastado, 38 / 43 px
     intenso: ganancia x1,5  · topes x2,75 -> ~20 % aplastado, 42-46 / 47-52 px
   Todos por debajo de los ~98 px que ya se midieron como molestos (v0.6x). La mezcla sube con el
   preajuste: mas imagen = algo mas de forma (correlacion con la red 0,06 -> 0,09) y poco mas de
   parpadeo (0,41 -> 0,46 de 255, la red 1,43). */
#pragma once

#include <cmath>

namespace rendimiento {

struct Preajuste {
  float mezcla;     /* 0 = suelo .. 1 = pistas de la imagen */
  float ganancia;   /* multiplica la ganancia del relieve */
  float topes;      /* multiplica los topes por ojo (+12,67 / -9,98 px) */
  float paralaje;   /* multiplica el paralaje de la cabeza */
};

inline constexpr Preajuste kPreajustes[3] = {
    {0.25f, 0.75f, 2.0f, 1.5f},    /* suave */
    {0.35f, 1.0f, 2.75f, 2.0f},    /* medio: el de fabrica */
    {0.50f, 1.5f, 2.75f, 3.0f},    /* intenso */
};
inline constexpr int kDeFabrica = 1;

/* En el modo rendimiento la pastilla no lleva Profundidad ni Salida, y el `relieve` global tampoco
   cuenta: la separacion la ponen solo estos cuatro valores. La base es la ganancia con la que se
   midieron los preajustes (Salida 50 con `relieve = 2,00`), y el plano de la pantalla es el
   automatico (el sujeto, el pico del mapa), el mismo que dio `odysseyvk mapa` al medirlos. */
inline constexpr float kGananciaBase = (0.00104f + 0.5f * 0.00972f) * 2.0f;

/* los cuatro deslizadores del menu del rayo: de donde a donde y a que paso se redondean */
struct Rango {
  float lo, hi, paso;
};
inline constexpr Rango kRangos[4] = {
    {0.0f, 1.0f, 0.01f},    /* mezcla: 0 % suelo .. 100 % imagen */
    {0.5f, 2.0f, 0.05f},    /* fuerza del relieve */
    {1.0f, 4.0f, 0.05f},    /* alcance del relieve (los topes) */
    {1.0f, 4.0f, 0.1f},     /* paralaje de la cabeza */
};
inline float ajusta(int k, float v) {
  const Rango &r = kRangos[k];
  v = std::round(v / r.paso) * r.paso;
  return v < r.lo ? r.lo : (v > r.hi ? r.hi : v);
}

/* cual de los tres es (por sus cuatro numeros); -1 si son valores a mano */
inline int cual(float mezcla, float ganancia, float topes, float paralaje) {
  for (int k = 0; k < 3; k++) {
    const Preajuste &p = kPreajustes[k];
    if (std::fabs(mezcla - p.mezcla) < 0.02f && std::fabs(ganancia - p.ganancia) < 0.02f &&
        std::fabs(topes - p.topes) < 0.02f && std::fabs(paralaje - p.paralaje) < 0.02f)
      return k;
  }
  return -1;
}

}  // namespace rendimiento
