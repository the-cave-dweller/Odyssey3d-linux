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
  unsigned producto = 0;    /* el codigo de producto del EDID */
  Clase clase = Normal;
  /* (v1.18) como declara su 3D en el EDID (ver estereoDeEdid), y el EDID entero, para el informe de «portar el 3D» */
  std::vector<std::string> estereo;
  std::string edid;
  bool forzada = false;     /* hace 3D, pero se pidio tratarla como una normal (`sin_3d = si`) */
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

/* (v1.18) Como dice un monitor que hace 3D, leido de su EDID (Rodolfo: «consider to also log the way this monitors handle
   3d so we can bring new support as they are usually not 3d lens free 3d»). Casi ninguno es de lente como el Odyssey: los
   de gafas lo declaran en tres sitios:
     - el detalle de tiempos del bloque base (byte 17, bits 6, 5 y 0): campo secuencial, entrelazado o lado a lado;
     - el bloque de HDMI de la extension CTA-861 (OUI 00-0C-03): `3D_present` y los formatos (empaquetado de cuadros,
       arriba y abajo, lado a lado a medias);
     - el bloque «Stereo Display Interface» de DisplayID (0x10 en la 1.x, 0x27 en la 2.0), con su metodo.
   El Odyssey 3D no declara nada de eso (MEDIDO: su extension CTA trae FreeSync y su DisplayID los tiempos): su 3D va por
   USB (la placa de la lente de Dimenco), y eso se mira aparte */
inline std::vector<std::string> estereoDeEdid(const std::string &edid) {
  std::vector<std::string> v;
  auto pon = [&](const std::string &t) {
    if (std::find(v.begin(), v.end(), t) == v.end()) v.push_back(t);
  };
  if (edid.size() < 128) return v;
  const unsigned char *e = (const unsigned char *)edid.data();
  for (int o : {54, 72, 90, 108}) {
    const unsigned char *b = e + o;
    if (!(b[0] | b[1])) continue;
    const int modo = ((b[17] >> 5) & 3) * 2 + (b[17] & 1);   /* bits 6, 5 y 0 */
    static const char *nombres[8] = {nullptr, nullptr, "EDID: campo secuencial, derecha con la sincronia", "EDID: entrelazado de 2 vias, derecha en las pares",
                                     "EDID: campo secuencial, izquierda con la sincronia", "EDID: entrelazado de 2 vias, izquierda en las pares",
                                     "EDID: entrelazado de 4 vias", "EDID: lado a lado entrelazado"};
    if (nombres[modo]) pon(nombres[modo]);
  }
  for (size_t k = 1; k * 128 + 128 <= edid.size(); k++) {
    const unsigned char *b = e + k * 128;
    if (b[0] == 0x02) {   /* CTA-861 */
      const int fin = std::min<int>(b[2], 127);
      for (int i = 4; i < fin;) {
        const int tipo = b[i] >> 5, n = b[i] & 31;
        if (i + 1 + n > 127) break;
        if (tipo == 3 && n >= 8 && b[i + 1] == 0x03 && b[i + 2] == 0x0C && b[i + 3] == 0x00) {   /* el bloque de HDMI */
          const unsigned char *h = b + i + 1;   /* h[0..2] OUI, h[3..4] direccion, h[5] banderas, h[6] TMDS, h[7] latencias y video */
          int j = 8;
          if (n > 7 && (h[7] & 0x20)) {   /* HDMI_Video_present */
            if (h[7] & 0x80) j += 2;
            if (h[7] & 0x40) j += 2;
            if (j < n && (h[j] & 0x80)) {
              pon("HDMI: 3D presente (3D_present)");
              const int multi = (h[j] >> 5) & 3;
              const int vics = j + 1 < n ? h[j + 1] >> 5 : 0;
              const int q = j + 2 + vics;
              if ((multi == 1 || multi == 2) && q + 1 < n) {
                const unsigned todos = ((unsigned)h[q] << 8) | h[q + 1];
                if (todos & 0x0001) pon("HDMI: empaquetado de cuadros (frame packing)");
                if (todos & 0x0040) pon("HDMI: arriba y abajo (top-and-bottom)");
                if (todos & 0x0100) pon("HDMI: lado a lado a medias (side-by-side half)");
              }
            }
          }
        }
        i += n + 1;
      }
    } else if (b[0] == 0x70) {   /* DisplayID */
      const int largo = std::min<int>(b[2], 121);
      for (int i = 5; i + 3 <= 5 + largo;) {
        const int tag = b[i], n = b[i + 2];
        if (tag == 0 && n == 0) break;
        if ((tag == 0x10 || tag == 0x27) && n >= 1 && i + 3 + n <= 127) {
          static const char *m[6] = {"campo secuencial", "lado a lado", "entrelazado de pixeles", "dos interfaces", "multivista", "cuadros apilados"};
          const int metodo = b[i + 3 + (tag == 0x10 ? 0 : 0)] & 0xFF;
          pon(std::string("DisplayID: interfaz estereo, ") + (metodo < 6 ? m[metodo] : "metodo " + std::to_string(metodo)));
        }
        i += 3 + n;
      }
    }
  }
  return v;
}

/* (v1.18) `sin_3d = si` en serve.conf (el interruptor «Monitor 3D sin soporte» de Ajustes): la pantalla de 3D se trata
   como una normal, y todo pasa a ShaderHub. Se lee aqui, sin mas dependencias, porque lo miran el panel, el servicio y la
   linea de ordenes */
inline bool forzado2D() {
  if (const char *e = std::getenv("ODYSSEYVK_SIN_3D")) return e[0] == '1';
  const char *x = std::getenv("XDG_CONFIG_HOME"), *h = std::getenv("HOME");
  const std::string ruta = (x && *x ? std::string(x) : std::string(h ? h : "") + "/.config") + "/odysseyvk/serve.conf";
  std::ifstream f(ruta);
  std::string l;
  bool si = false;
  while (std::getline(f, l)) {
    const size_t a = l.find_first_not_of(" \t");
    if (a == std::string::npos || l.compare(a, 6, "sin_3d") != 0) continue;
    const size_t ig = l.find('=', a);
    if (ig == std::string::npos) continue;
    std::string v = minusculas(l.substr(ig + 1));
    v = v.substr(0, v.find('#'));
    si = v.find("si") != std::string::npos || v.find('1') != std::string::npos || v.find("on") != std::string::npos;
  }
  return si;
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
  p.producto = (unsigned)e[10] | ((unsigned)e[11] << 8);
  p.edid = edid;
  p.estereo = estereoDeEdid(edid);
  p.clase = clasifica(p.fabricante, p.nombre);
  /* el que declara un 3D en su EDID y no es el Odyssey: un monitor 3D que todavia no */
  if (p.clase == Normal && !p.estereo.empty()) p.clase = Candidata;
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
  if (const char *p = std::getenv("ODYSSEYVK_PANTALLAS_PRUEBA")) {
    std::vector<Pantalla> t = deTexto(p);
    if (forzado2D())
      for (Pantalla &q : t)
        if (q.clase == Con3D) q.clase = Candidata, q.forzada = true;
    return t;
  }
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
  if (forzado2D())
    for (Pantalla &p : v)
      if (p.clase == Con3D) p.clase = Candidata, p.forzada = true;
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
