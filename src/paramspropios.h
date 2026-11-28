/* paramspropios.h — (v1.18) los parametros que el usuario añade a Integraciones.
   Rodolfo: «añade tambien un boton de deteccion de integraciones que note si algun programa extra añade parametros y deja
   que el usuario lo añada de un listado, y si no les agrada que los quiten de esa lista, que solo puedan quitar los que
   ellos añadan, tambien que deje añadir personalizados».
   Cada uno es una pieza `VARIABLE=valor`. La lista (los que ha añadido, de los detectados o escritos a mano) vive en
   ~/.config/odysseyvk/integraciones-propias, una por renglon. Encendido, la pieza va en «Otros parametros» (`otros`) de
   lo de todos o de un juego, que es lo que ya sabe escribir y leer la linea de Steam: no hay claves nuevas en
   integraciones.conf ni nada que la linea no supiera llevar. Aqui, lo que no toca ni Steam ni el panel, para probarlo
   solo: tools/prueba_params.cpp */
#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace paramspropios {

/* las piezas de un «otros», partidas por espacios (lo de entre comillas dobles no se parte) */
inline std::vector<std::string> piezas(const std::string &otros) {
  std::vector<std::string> v;
  std::string p;
  bool comillas = false;
  for (char c : otros) {
    if (c == '"') comillas = !comillas;
    if (!comillas && std::isspace((unsigned char)c)) {
      if (!p.empty()) v.push_back(p), p.clear();
    } else {
      p.push_back(c);
    }
  }
  if (!p.empty()) v.push_back(p);
  return v;
}

/* la variable de una pieza: lo de antes del = */
inline std::string variableDe(const std::string &pieza) { return pieza.substr(0, pieza.find('=')); }

/* una pieza que se puede llevar: VARIABLE=valor, la variable con letras, cifras y _ (sin empezar por cifra) y el valor sin
   espacios ni nada que la linea de Steam no sepa leer (; | & < > $ ` comillas, parentesis). Lo que no pasa, no se añade */
inline bool valida(const std::string &pieza) {
  const size_t ig = pieza.find('=');
  if (ig == std::string::npos || ig == 0 || ig + 1 >= pieza.size() || pieza.size() > 200) return false;
  if (std::isdigit((unsigned char)pieza[0])) return false;
  for (size_t i = 0; i < ig; i++)
    if (!(std::isalnum((unsigned char)pieza[i]) || pieza[i] == '_')) return false;
  for (size_t i = ig + 1; i < pieza.size(); i++) {
    const unsigned char c = (unsigned char)pieza[i];
    if (!(std::isalnum(c) || std::string("_.,:/@%+=-").find((char)c) != std::string::npos)) return false;
  }
  return true;
}

/* esta puesta en ese «otros» (la pieza entera, no solo su variable) */
inline bool puesta(const std::string &otros, const std::string &pieza) {
  const std::vector<std::string> v = piezas(otros);
  return std::find(v.begin(), v.end(), pieza) != v.end();
}

/* el «otros» con la pieza puesta o quitada. Al ponerla quita antes cualquier otra de la misma variable (FOO=1 y FOO=2 no
   van juntas); lo demas queda en su orden */
inline std::string con(const std::string &otros, const std::string &pieza, bool si) {
  std::string r;
  const std::string var = variableDe(pieza);
  for (const std::string &p : piezas(otros)) {
    if (p == pieza || (si && variableDe(p) == var)) continue;
    r += (r.empty() ? "" : " ") + p;
  }
  if (si) r += (r.empty() ? "" : " ") + pieza;
  return r;
}

/* la lista del usuario, de su texto (un renglon por pieza; lo que no vale o se repite, fuera) y a su texto */
inline std::vector<std::string> deTexto(const std::string &t) {
  std::vector<std::string> v;
  size_t i = 0;
  while (i <= t.size()) {
    const size_t f = std::min(t.find('\n', i), t.size());
    std::string l = t.substr(i, f - i);
    while (!l.empty() && std::isspace((unsigned char)l.back())) l.pop_back();
    while (!l.empty() && std::isspace((unsigned char)l.front())) l.erase(l.begin());
    if (!l.empty() && l[0] != '#' && valida(l) && std::find(v.begin(), v.end(), l) == v.end()) v.push_back(l);
    i = f + 1;
  }
  return v;
}
inline std::string aTexto(const std::vector<std::string> &v) {
  std::string t = "# OdysseyVK: los parametros que añadiste en Integraciones (una pieza VARIABLE=valor por renglon)\n";
  for (const std::string &p : v) t += p + "\n";
  return t;
}

}  // namespace paramspropios
