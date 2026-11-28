/* marca.h — (v1.18) el nombre que se ve: SpatialHub con el 3D y ShaderHub sin el.
   Rodolfo: «que la version no 3d se llame ShaderHUB y la 3D que sea SpatialHUB, la interfaz no 3d debe ser roja, haz todo
   el rebrand».
   El programa es uno (la orden, el servicio y sus carpetas siguen llamandose `odysseyvk`: cambiarlas dejaria sin ajustes ni
   calibracion a quien ya lo tiene); lo que cambia es como se presenta. Los textos siguen escritos con «OdysseyVK» (en los
   seis idiomas) y el nombre de ahora se pone al pintarlos (Hud::text) y al avisar por el escritorio: asi una sola tabla
   vale para las dos caras.
     SpatialHub  la pantalla elegida hace 3D (el Odyssey): todo lo de siempre;
     ShaderHub   una pantalla normal: juegos, efectos (sombreadores) y generacion de cuadros, en rojo. */
#pragma once

#include <string>
#include <vector>

#include "pantallas.h"

namespace marca {

inline const char *nombre(bool sin3D) { return sin3D ? "ShaderHub" : "SpatialHub"; }

/* el rojo de ShaderHub (el acento de su interfaz) */
inline const float *rojo() {
  static const float c[3] = {0.90f, 0.27f, 0.30f};
  return c;
}

/* el texto con el nombre de ahora donde dice «OdysseyVK» */
inline std::string pon(const std::string &s, bool sin3D) {
  static const std::string viejo = "OdysseyVK";
  size_t i = s.find(viejo);
  if (i == std::string::npos) return s;
  std::string r = s;
  const std::string nuevo = nombre(sin3D);
  while (i != std::string::npos) {
    r.replace(i, viejo.size(), nuevo);
    i = r.find(viejo, i + nuevo.size());
  }
  return r;
}

/* La cara del equipo, para lo que no tiene un panel delante (los avisos del escritorio, la bandeja): ShaderHub si se sabe
   de alguna pantalla conectada y ninguna hace 3D. Se mira una vez por proceso */
inline bool equipoSin3D() {
  static const bool sin = [] {
    const std::vector<pantallas::Pantalla> v = pantallas::lista();
    return !v.empty() && !pantallas::hay3D(v);
  }();
  return sin;
}

}  // namespace marca
