/* pantallas.h — (v1.18) las pantallas conectadas y cual de ellas hace 3D.
   Rodolfo: «nuevo major change, sistema de displays, que en el tope vkodyssey detecte y muestre enlistados los display,
   que si no son 3d (la mayoria) deshabilite en ellos cualquier cosa relacionada al 3d y que funcione como gestor
   independiente de shader y juegos, que en casos donde no halla un monitor 3d sea la pestaña por defecto, tambien analiza
   si monitores 3d como el acer spatialabs o el de sony pueden tener su modo 3d».
   Se leen del nucleo (/sys/class/drm/cardN-CONECTOR: `status` y `edid`), sin el compositor: vale en el panel, en el
   servicio y en la linea de ordenes. De cada EDID, el fabricante (las tres letras PNP), el nombre del monitor (el
   descriptor 0xFC) y su resolucion nativa (el primer detalle de tiempos).
   Las clases:
     Con3D       el Odyssey 3D (SAM · «Odyssey G90XF»): lo que OdysseyVK sabe poner en 3D;
     Candidata   un monitor 3D sin gafas que todavia no (Acer SpatialLabs, Sony Spatial Reality Display): se dice, y se
                 trata como una pantalla normal;
     Normal      las demas: sin nada del 3D.
   MEDIDO en el equipo de Rodolfo el 28-nov: card2-DP-4 = SAM 7860 «Odyssey G90XF» 3840x2160 (EDID de 384 bytes) y
   card0-DP-7 = SUN «X7149A» 1600x1200; y por USB la placa de la lente es 354b:0116 «Dimenco BV SR Lens Switch Board»:
   el Odyssey 3D lleva la plataforma SR de Dimenco, la misma de los Acer SpatialLabs (ver CAMBIOS.md).
   Para probarlo sin monitores: ODYSSEYVK_PANTALLAS_PRUEBA="DP-4|SAM|Odyssey G90XF|3840x2160;HDMI-A-1|GSM|LG ULTRAGEAR|2560x1440"
   (o vacio: ninguna), y tools/prueba_pantallas.cpp */
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace pantallas {

enum Clase { Normal = 0, Con3D = 1, Candidata = 2 };

struct Pantalla {
  std::string conector;     /* DP-4 */
  std::string fabricante;   /* SAM */
  std::string nombre;       /* Odyssey G90XF */
  int ancho = 0, alto = 0;  /* la resolucion nativa */
  Clase clase = Normal;
  std::string rotulo() const {
    return conector + (nombre.empty() ? (fabricante.empty() ? std::string() : " · " + fabricante) : " · " + nombre);
  }
};

inline std::string minusculas(std::string s) {
  for (char &c : s) c = (char)std::tolower((unsigned char)c);
  return s;
}

/* que clase de pantalla es, por su fabricante y su nombre. El Odyssey 3D es el G90XF (los demas Odyssey no llevan lente);
   «3D» en el nombre de un Samsung vale tambien para el que venga despues */
inline Clase clasifica(const std::string &fabricante, const std::string &nombre) {
  const std::string f = minusculas(fabricante), n = minusculas(nombre);
  auto tiene = [&](const char *t) { return n.find(t) != std::string::npos; };
  if (tiene("g90xf") || (f == "sam" && tiene("odyssey") && tiene("3d"))) return Con3D;
  if (tiene("spatiallabs") || tiene("asv15") || tiene("asv27") || tiene("psv27")) return Candidata;   /* Acer SpatialLabs View */
  if (tiene("elf-sr")) return Candidata;                                                             /* Sony Spatial Reality Display */
  return Normal;
}

/* una pantalla, de su EDID (el bloque base de 128 bytes; lo demas no hace falta) */
inline Pantalla deEdid(const std::string &conector, const std::string &edid) {
  Pantalla p;
  p.conector = conector;
  if (edid.size() < 128) return p;
  const unsigned char *e = (const unsigned char *)edid.data();
  const unsigned m = ((unsigned)e[8] << 8) | e[9];
  for (int s : {10, 5, 0}) {
    const char c = (char)(((m >> s) & 31) + 64);
    if (c >= 'A' && c <= 'Z') p.fabricante.push_back(c);
  }
  for (int o : {54, 72, 90, 108}) {
    const unsigned char *b = e + o;
    if (b[0] == 0 && b[1] == 0) {
      if (b[3] != 0xFC) continue;
      std::string n((const char *)b + 5, 13);
      n = n.substr(0, n.find('\n'));
      while (!n.empty() && (n.back() == ' ' || n.back() == '\0')) n.pop_back();
      p.nombre = n;
    } else if (p.ancho == 0) {
      p.ancho = b[2] | ((b[4] & 0xF0) << 4);
      p.alto = b[5] | ((b[7] & 0xF0) << 4);
    }
  }
  p.clase = clasifica(p.fabricante, p.nombre);
  return p;
}

/* las de la prueba: «CONECTOR|FAB|NOMBRE|ANCHOxALTO» separadas por «;» */
inline std::vector<Pantalla> deTexto(const std::string &t) {
  std::vector<Pantalla> v;
  size_t a = 0;
  while (a < t.size()) {
    size_t b = t.find(';', a);
    if (b == std::string::npos) b = t.size();
    const std::string una = t.substr(a, b - a);
    a = b + 1;
    std::vector<std::string> c;
    size_t i = 0;
    while (i <= una.size()) {
      size_t j = una.find('|', i);
      if (j == std::string::npos) j = una.size();
      c.push_back(una.substr(i, j - i));
      i = j + 1;
    }
    if (c.empty() || c[0].empty()) continue;
    Pantalla p;
    p.conector = c[0];
    if (c.size() > 1) p.fabricante = c[1];
    if (c.size() > 2) p.nombre = c[2];
    if (c.size() > 3) std::sscanf(c[3].c_str(), "%dx%d", &p.ancho, &p.alto);
    p.clase = clasifica(p.fabricante, p.nombre);
    v.push_back(p);
  }
  return v;
}

/* las conectadas, la de 3D primero y las demas por su conector. `raiz`: /sys/class/drm (otra, para probarlo) */
inline std::vector<Pantalla> lista(const std::string &raiz = "/sys/class/drm") {
  if (const char *p = std::getenv("ODYSSEYVK_PANTALLAS_PRUEBA")) return deTexto(p);
  namespace fs = std::filesystem;
  std::vector<Pantalla> v;
  std::error_code ec;
  for (const fs::directory_entry &c : fs::directory_iterator(raiz, ec)) {
    const std::string n = c.path().filename().string();
    const size_t g = n.find('-');
    if (n.rfind("card", 0) != 0 || g == std::string::npos) continue;
    std::ifstream st(c.path() / "status");
    std::string estado;
    std::getline(st, estado);
    if (estado != "connected") continue;
    std::ifstream ed(c.path() / "edid", std::ios::binary);
    const std::string edid((std::istreambuf_iterator<char>(ed)), std::istreambuf_iterator<char>());
    v.push_back(deEdid(n.substr(g + 1), edid));
  }
  std::stable_sort(v.begin(), v.end(), [](const Pantalla &a, const Pantalla &b) {
    if ((a.clase == Con3D) != (b.clase == Con3D)) return a.clase == Con3D;
    return a.conector < b.conector;
  });
  return v;
}

inline bool hay3D(const std::vector<Pantalla> &v) {
  return std::any_of(v.begin(), v.end(), [](const Pantalla &p) { return p.clase == Con3D; });
}

}  // namespace pantallas
