/* puerto3d.h — (v1.18) el informe para pedir el 3D de un monitor que todavia no lo tiene.
   Rodolfo: «add a 3d port button near the bug one on 3d known monitors that offers to gather device info and upload it as
   a feature request to see wether support can be added, consider to also log the way this monitors handle 3d so we can
   bring new support as they are usually not 3d lens free 3d».
   Lo que junta, de un monitor (pantallas.h): quien es, como declara su 3D en el EDID (estereoDeEdid), su EDID entero SIN
   el numero de serie, sus modos, y los aparatos USB que pueden ser suyos (las camaras, la placa de la lente de Dimenco y
   los de su misma marca), sin sus numeros de serie. No sube nada: arma la direccion de «New issue» de GitHub con el texto
   puesto y el navegador la abre; quien la manda es el usuario, despues de leerla. Queda una copia en
   ~/.local/state/odysseyvk/portar-3d.md. Para probarlo solo: tools/prueba_pantallas.cpp */
#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "informe.h"
#include "pantallas.h"

namespace puerto3d {

/* el EDID sin lo que identifica a la unidad: el numero de serie del bloque base (bytes 12 a 15) y el descriptor de serie
   (0xFF), con la suma del bloque rehecha para que siga siendo un EDID valido */
inline std::string edidSinSerie(std::string e) {
  if (e.size() < 128) return e;
  for (int i = 12; i < 16; i++) e[(size_t)i] = 0;
  for (int o : {54, 72, 90, 108})
    if (e[(size_t)o] == 0 && e[(size_t)o + 1] == 0 && (unsigned char)e[(size_t)o + 3] == 0xFF)
      for (int i = 5; i < 18; i++) e[(size_t)(o + i)] = i == 5 ? '\n' : ' ';
  unsigned suma = 0;
  for (int i = 0; i < 127; i++) suma += (unsigned char)e[(size_t)i];
  e[127] = (char)((256 - (suma & 255)) & 255);
  return e;
}

inline std::string hex(const std::string &b) {
  static const char *h = "0123456789abcdef";
  std::string t;
  for (size_t i = 0; i < b.size(); i++) {
    t.push_back(h[(unsigned char)b[i] >> 4]), t.push_back(h[(unsigned char)b[i] & 15]);
    if (i % 32 == 31) t.push_back('\n');
  }
  if (!t.empty() && t.back() != '\n') t.push_back('\n');
  return t;
}

/* los aparatos USB que pueden ser del monitor: fabricante:producto y su nombre, sin serie. `raiz`: /sys/bus/usb/devices */
inline std::vector<std::string> usbDelMonitor(const std::string &fabricantePnp, const std::string &raiz = "/sys/bus/usb/devices") {
  static const std::pair<const char *, const char *> marcas[] = {{"SAM", "04e8"}, {"ACR", "0502"}, {"SNY", "054c"}, {"LEN", "17ef"},
                                                                {"AUS", "0b05"}, {"GSM", "043e"}, {"DEL", "413c"}};
  std::string suya;
  for (const auto &m : marcas)
    if (fabricantePnp == m.first) suya = m.second;
  auto lee = [](const std::filesystem::path &p) {
    std::ifstream f(p);
    std::string l;
    std::getline(f, l);
    while (!l.empty() && (l.back() == ' ' || l.back() == '\n')) l.pop_back();
    return l;
  };
  std::vector<std::string> v;
  std::error_code ec;
  for (const auto &d : std::filesystem::directory_iterator(raiz, ec)) {
    const std::string vid = lee(d.path() / "idVendor"), pid = lee(d.path() / "idProduct");
    if (vid.empty() || pid.empty()) continue;
    bool camara = false;
    for (const auto &i : std::filesystem::directory_iterator(d.path(), ec))
      if (lee(i.path() / "bInterfaceClass") == "0e") camara = true;
    const bool dimenco = vid == "354b";
    if (!(camara || dimenco || (!suya.empty() && vid == suya))) continue;
    v.push_back(vid + ":" + pid + " " + lee(d.path() / "manufacturer") + " " + lee(d.path() / "product") +
                (dimenco ? " (placa de lente SR de Dimenco)" : camara ? " (camara)" : ""));
  }
  std::sort(v.begin(), v.end());
  return v;
}

inline std::string titulo(const pantallas::Pantalla &p) {
  return "Portar el 3D: " + p.fabricante + " " + (p.nombre.empty() ? std::string("(sin nombre)") : p.nombre);
}

/* el texto del pedido. `modos`: los de su conector (de /sys/class/drm/…/modes), `usb`: usbDelMonitor, `equipo`: lo basico
   del equipo (informe::diagnostico) */
inline std::string cuerpo(const pantallas::Pantalla &p, const std::vector<std::string> &modos, const std::vector<std::string> &usb,
                          const std::string &equipo) {
  char prod[16];
  std::snprintf(prod, sizeof prod, "%04x", p.producto);
  std::string t = "## Monitor\n";
  t += "- Fabricante (PNP): " + p.fabricante + " · producto " + prod + "\n";
  t += "- Nombre: " + (p.nombre.empty() ? std::string("(el EDID no lo trae)") : p.nombre) + "\n";
  t += "- Conector: " + p.conector + " · nativa " + std::to_string(p.ancho) + "x" + std::to_string(p.alto) + "\n";
  if (p.forzada) t += "- OdysseyVK lo reconoce como monitor 3D, pero se puso «Monitor 3D sin soporte» en Ajustes\n";
  t += "\n## Cómo declara su 3D\n";
  if (p.estereo.empty()) t += "- Nada en el EDID (ni en el detalle de tiempos, ni en el bloque de HDMI, ni en DisplayID)\n";
  for (const std::string &m : p.estereo) t += "- " + m + "\n";
  t += "\n## Aparatos USB que pueden ser suyos\n";
  if (usb.empty()) t += "- Ninguno (ni cámaras, ni placa de lente de Dimenco, ni de su marca)\n";
  for (const std::string &u : usb) t += "- " + u + "\n";
  t += "\n## Modos\n";
  std::string ms;
  for (size_t k = 0; k < modos.size() && k < 14; k++) ms += (k ? " · " : "") + modos[k];
  t += (ms.empty() ? std::string("(no se pudieron leer)") : ms) + "\n";
  t += "\n## EDID (sin el número de serie)\n```\n" + hex(edidSinSerie(p.edid)) + "```\n";
  t += "\n## Equipo\n```\n" + equipo + "```\n";
  t += "\n## Lo que sé de cómo se usa su 3D en otros sistemas\n(escribe aquí: qué programa lo pone en 3D, si lleva gafas, si tiene cámara de ojos…)\n";
  return t;
}

inline std::string url(const std::string &repo, const pantallas::Pantalla &p, const std::string &texto) {
  return repo + "/issues/new?labels=enhancement&title=" + informe::codificar(titulo(p)) + "&body=" + informe::codificar(texto);
}

}  // namespace puerto3d
