/* versiones.h — (v1.18) las versiones anteriores guardadas al instalar (tools/versiones.sh), vistas desde el programa:
   `odysseyvk volver` y la pagina del informe de un error las listan con esto. Rodolfo: «implementa un sistema de fallback
   que permita regresar hasta dos versiones si el programa funcionaba previo al error».
   El guion vive en PREFIJO/lib/odysseyvk/versiones.sh y lo guardado en PREFIJO/lib/odysseyvk/anteriores. El prefijo es el
   del binario que corre (/usr/local), o ODYSSEYVK_PREFIJO en las pruebas. */
#pragma once

#include "util.h"

#include <unistd.h>

#include <cstdlib>
#include <string>
#include <vector>

namespace versiones {

struct Guardada {
  std::string carpeta, version, fecha;
  bool funcionaba = false;   /* con ella hubo un 3D de un minuto (lo apunta el servicio) */
};

inline std::string prefijo() {
  if (const char *p = std::getenv("ODYSSEYVK_PREFIJO"); p && *p) return p;
  char propio[512] = {0};
  const ssize_t n = ::readlink("/proc/self/exe", propio, sizeof propio - 1);
  const std::string bin = n > 0 ? std::string(propio, (size_t)n) : std::string("/usr/local/bin/odysseyvk");
  const std::string bindir = bin.substr(0, bin.rfind('/'));
  return bindir.substr(0, bindir.rfind('/'));
}
inline std::string guion() { return prefijo() + "/lib/odysseyvk/versiones.sh"; }
inline std::string comillas(const std::string &s) {
  std::string r = "'";
  for (char c : s) r += c == '\'' ? std::string("'\\''") : std::string(1, c);
  return r + "'";
}

/* las guardadas, la mas nueva primero (dos como mucho) */
inline std::vector<Guardada> lista() {
  std::vector<Guardada> v;
  std::string t;
  if (::access(guion().c_str(), R_OK) != 0) return v;
  runCapture("sh " + comillas(guion()) + " lista " + comillas(prefijo()) + " 2>/dev/null", t);
  size_t i = 0;
  while (i < t.size()) {
    const size_t f = std::min(t.find('\n', i), t.size());
    const std::string l = t.substr(i, f - i);
    i = f + 1;
    const size_t a = l.find('|'), b = a == std::string::npos ? a : l.find('|', a + 1), c = b == std::string::npos ? b : l.find('|', b + 1);
    if (c == std::string::npos) continue;
    v.push_back({l.substr(0, a), l.substr(a + 1, b - a - 1), l.substr(b + 1, c - b - 1), l.substr(c + 1) == "si"});
  }
  return v;
}

/* Apunta que el binario instalado funciono (el servicio, tras un 3D de un minuto): un renglon por binario en `ruta`, con
   su suma (`cksum`, la misma que mira install.sh). La suma y no el numero de version: de la 1.18 salen muchos binarios,
   y que uno funcionara no dice nada del siguiente. Se quedan los 20 ultimos renglones */
inline void apuntarFuncionaba(const std::string &ruta) {
  std::string suma;
  runCapture("cksum < " + comillas(prefijo() + "/bin/odysseyvk") + " 2>/dev/null", suma);
  while (!suma.empty() && (suma.back() == '\n' || suma.back() == ' ')) suma.pop_back();
  if (suma.empty()) return;
  std::string t;
  readSmallFile(ruta, t);
  if (("\n" + t).find("\n" + suma + "\n") != std::string::npos) return;
  t += suma + "\n";
  size_t renglones = 0;
  for (char c : t) renglones += c == '\n';
  for (; renglones > 20; renglones--) t.erase(0, t.find('\n') + 1);
  if (FILE *f = std::fopen(ruta.c_str(), "we")) {
    std::fwrite(t.data(), 1, t.size(), f);
    std::fclose(f);
  }
}

}  // namespace versiones
