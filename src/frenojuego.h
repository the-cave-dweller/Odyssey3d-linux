/* frenojuego.h — (v1.18) el freno de un juego que pierde la pantalla completa al entrar el 3D.
   MEDIDO en el registro de Rodolfo del 27-nov (Cyberpunk, 18:26:53-18:27:47 y 18:31:47-18:33:02): el visor se queda
   con el foco, el juego se minimiza, al sacarlo vuelve en ventana (2554x1408) 1,1 s y el servicio cerraba el 3D por
   «salio de pantalla completa», que no cuenta como muerte: a los 4 s de volver el juego a pantalla completa lo
   lanzaba otra vez. Siete vueltas en 54 s y otras cinco despues, con el juego rehaciendo su intercambio en cada una.
   La regla: dos cierres asi seguidos en la misma ventana, con sesiones de menos de 15 s, y a esa ventana el 3D no se
   le lanza solo durante un minuto; cada cierre corto mas, cinco minutos. Una sesion que aguanta 15 s lo borra, y la
   ventana que deja de existir se olvida (la que solo sale de pantalla completa lo conserva: de eso va el bucle).
   Aparte del servicio para poder probarlo con el reloj en la mano: tools/prueba_freno.cpp */
#pragma once

#include <iterator>
#include <map>
#include <set>
#include <string>

struct FrenoJuego {
  static constexpr double kCorta = 15.0;      /* una sesion de menos de esto «se cerro apenas empezo» */
  static constexpr double kPrimera = 60.0;    /* la espera al segundo cierre corto seguido */
  static constexpr double kDespues = 300.0;   /* y la de cada uno mas */

  std::string cortasUuid;                 /* la ventana de los cierres cortos que se van contando */
  int cortasSeguidas = 0;
  std::map<std::string, double> hasta;    /* por ventana: hasta cuando no se le lanza el 3D solo */

  /* Se recogio una sesion sobre `uuid` que duro `duro` segundos. `porPantallaCompleta`: la cerro la salida de
     pantalla completa de la ventana; `juego`: la ventana es de un juego. Devuelve la espera que acaba de ponerle a
     esa ventana (0 si ninguna) */
  double cierre(const std::string &uuid, double duro, bool porPantallaCompleta, bool juego, double now) {
    if (uuid.empty()) return 0.0;
    if (duro >= kCorta) {   /* aguanto: ni freno ni cierres apuntados */
      if (cortasUuid == uuid) cortasUuid.clear(), cortasSeguidas = 0;
      hasta.erase(uuid);
      return 0.0;
    }
    if (!porPantallaCompleta || !juego) return 0.0;
    if (cortasUuid != uuid) cortasUuid = uuid, cortasSeguidas = 0;
    if (++cortasSeguidas < 2) return 0.0;
    const double espera = cortasSeguidas == 2 ? kPrimera : kDespues;
    hasta[uuid] = now + espera;
    return espera;
  }
  /* a esta ventana no se le lanza el 3D solo todavia */
  bool frenada(const std::string &uuid, double now) const {
    const auto it = hasta.find(uuid);
    return it != hasta.end() && now < it->second;
  }
  /* `estan`: las ventanas que existen ahora, a pantalla completa o no */
  void podar(const std::set<std::string> &estan) {
    for (auto it = hasta.begin(); it != hasta.end();) it = estan.count(it->first) ? std::next(it) : hasta.erase(it);
    if (!cortasUuid.empty() && !estan.count(cortasUuid)) cortasUuid.clear(), cortasSeguidas = 0;
  }
};
