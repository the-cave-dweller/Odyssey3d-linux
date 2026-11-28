/* informe.h — (v1.18) el informe de un error para GitHub.
   Rodolfo: «añade un boton de bug al fondo a la izquierda de la pantalla, que al presionarlo abra y mande los errores a
   nuestro github para arreglar si algo salio mal, que pida lo que deba pedir en descripcion, recuerda solo recolectar y
   guardar logs basicos e importantes».
   Como va: el boton abre una pagina del panel que pregunta que paso, que se esperaba y como repetirlo, enseña lo que se
   adjunta y abre el navegador en «New issue» del repositorio con todo ya escrito. Lo manda el usuario con su cuenta,
   despues de leerlo: OdysseyVK no envia nada por su cuenta ni guarda claves de GitHub.
   Lo que se adjunta, y nada mas:
     - la version de OdysseyVK, el sistema (distribucion, nucleo, escritorio y tipo de sesion) y las tarjetas graficas;
     - si estan el monitor 3D, su lente, su scaler y su camara (si o no, por sus identificadores USB);
     - lo que dice el vigilante ahora (el fichero `hub`: sesion, modo, pantalla, red);
     - del registro del servicio de este arranque, solo los renglones que cuentan algo (fallos, avisos, sesiones que
       empiezan y por que se cierran), los 30 ultimos, cortados a 200 letras, sin el titulo de la ventana y sin repetir
       los que salen seguidos;
     - las caidas de OdysseyVK o de gamescope de los dos ultimos dias (cuando y que programa, sin el volcado).
   Antes de salir, todo pasa por limpiar(): la carpeta personal queda en «~», y el nombre de usuario y el del equipo, fuera.
   No van: serve.conf, las lineas de Steam, titulos de ventanas, capturas ni volcados.
   Una copia de cada informe abierto queda en ~/.local/state/odysseyvk/informes (los cinco ultimos).
   Lo que no lee el equipo se prueba aparte: tools/prueba_informe.cpp */
#pragma once

#include "monitor.h"
#include "util.h"

#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

namespace informe {

struct Datos {
  std::string que, esperaba, pasos;   /* lo que escribe el usuario */
};

/* ---- lo que no lee el equipo ---- */

inline std::string reemplazar(std::string s, const std::string &de, const std::string &a) {
  if (de.empty()) return s;
  for (size_t i = s.find(de); i != std::string::npos; i = s.find(de, i + a.size())) s.replace(i, de.size(), a);
  return s;
}

/* sin la carpeta personal, el usuario ni el nombre del equipo (un nombre de menos de 3 letras no se toca: saldria de
   dentro de otras palabras) */
inline std::string limpiar(std::string s, const std::string &casa, const std::string &usuario, const std::string &equipo) {
  if (casa.size() > 1) s = reemplazar(s, casa, "~");
  /* el equipo antes que el usuario: si su nombre lleva el del usuario dentro («torre-de-ana»), al reves quedaba a medias */
  if (equipo.size() >= 3) s = reemplazar(s, equipo, "equipo");
  if (usuario.size() >= 3) s = reemplazar(s, "/home/" + usuario, "~"), s = reemplazar(s, usuario, "usuario");
  return s;
}

/* del registro del servicio, los renglones que cuentan algo: como mucho `tope`, los ultimos, cada uno cortado a 200 */
inline std::vector<std::string> filtrarRegistro(const std::string &registro, size_t tope = 30) {
  static const char *kCuenta[] = {"error", "Error", "fall", "no se pudo", "No se pudo", "[serve] lanzando", "[serve] la sesion",
                                  "cerramos la sesion", "Xid", "DEVICE_LOST", "perdio la pantalla completa",
                                  "tarjeta de aviso", "no encontr", "se corto", "NO:"};
  std::vector<std::string> v;
  size_t i = 0;
  while (i < registro.size()) {
    const size_t f = std::min(registro.find('\n', i), registro.size());
    std::string l = registro.substr(i, f - i);
    i = f + 1;
    bool cuenta = false;
    for (const char *k : kCuenta) cuenta = cuenta || l.find(k) != std::string::npos;
    /* lo de cada segundo no es de un informe aunque diga «error» (el del rastreo es su medida, en pixeles) */
    if (!cuenta || l.find("[rastreo]") != std::string::npos || l.find("[fps]") != std::string::npos) continue;
    /* MEDIDO con el registro de Rodolfo: «[serve] lanzando 3D por zen | <el titulo del video> — Zen Browser» traia lo que
       estaba viendo. El titulo de la ventana no sale: se corta en la barra */
    if (l.find("[serve] lanzando 3D") != std::string::npos)
      if (const size_t barra = l.find(" | "); barra != std::string::npos) l = l.substr(0, barra) + " | (título quitado)";
    /* lo mismo seguido (una sesion que entra y sale en bucle) se cuenta, no se repite */
    if (!v.empty() && v.back().rfind(l, 0) == 0) {
      const size_t x = v.back().rfind(" (x");
      const int n = x != std::string::npos && x >= l.size() ? std::atoi(v.back().c_str() + x + 3) : 1;
      v.back() = l + " (x" + std::to_string(n + 1) + ")";
      continue;
    }
    if (l.size() > 200) {
      size_t c = 200;
      while (c > 0 && ((unsigned char)l[c] & 0xC0) == 0x80) c--;   /* sin partir una letra */
      l = l.substr(0, c) + "…";
    }
    v.push_back(l);
  }
  if (v.size() > tope) v.erase(v.begin(), v.end() - (ptrdiff_t)tope);
  return v;
}

/* el texto de la incidencia: las tres preguntas con lo escrito y, debajo, lo recogido */
inline std::string cuerpo(const Datos &d, const std::string &diagnostico) {
  auto o = [](const std::string &t) { return t.empty() ? std::string("_(sin escribir)_") : t; };
  return "### Qué pasó\n" + o(d.que) + "\n\n### Qué esperabas\n" + o(d.esperaba) + "\n\n### Cómo repetirlo\n" + o(d.pasos) +
         "\n\n### Lo que adjunta OdysseyVK\n```\n" + diagnostico + "```\n";
}

inline std::string titulo(const Datos &d) {
  std::string t = d.que.substr(0, d.que.find('\n'));
  if (t.size() > 80) {
    size_t c = 80;
    while (c > 0 && ((unsigned char)t[c] & 0xC0) == 0x80) c--;
    t = t.substr(0, c) + "…";
  }
  return t.empty() ? std::string("Informe de un error") : t;
}

inline std::string codificar(const std::string &s) {
  static const char *hex = "0123456789ABCDEF";
  std::string r;
  for (unsigned char c : s) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') r.push_back((char)c);
    else r.push_back('%'), r.push_back(hex[c >> 4]), r.push_back(hex[c & 15]);
  }
  return r;
}

/* la direccion de «New issue» con el titulo y el cuerpo. GitHub corta las direcciones largas: si no cabe en `tope`, del
   diagnostico se quitan renglones del final del registro (lo primero es lo del equipo) y se dice */
inline std::string url(const std::string &repo, const Datos &d, std::string diagnostico, size_t tope = 7000) {
  const std::string base = repo + "/issues/new?title=" + codificar(titulo(d)) + "&body=";
  std::string u = base + codificar(cuerpo(d, diagnostico));
  bool cortado = false;
  while (u.size() > tope && !diagnostico.empty()) {
    const size_t fin = diagnostico.rfind('\n', diagnostico.size() >= 2 ? diagnostico.size() - 2 : 0);
    if (fin == std::string::npos) break;
    diagnostico.erase(fin + 1), cortado = true;
    u = base + codificar(cuerpo(d, diagnostico + "(recortado: el informe entero quedó copiado en el portapapeles)\n"));
  }
  (void)cortado;
  return u;
}

/* ---- lo que lee el equipo ---- */

inline bool hayUsb(const char *fabricante, const char *producto) {
  std::error_code ec;
  for (const auto &e : std::filesystem::directory_iterator("/sys/bus/usb/devices", ec)) {
    std::string v, p;
    if (readSmallFile((e.path() / "idVendor").string(), v) && readSmallFile((e.path() / "idProduct").string(), p) &&
        strTrim(v) == fabricante && strTrim(p) == producto)
      return true;
  }
  return false;
}

inline std::string diagnostico() {
  std::string t;
  auto renglon = [&](const std::string &k, const std::string &v) { t += k + ": " + v + "\n"; };
#ifdef ODYSSEYVK_VERSION
  renglon("OdysseyVK", ODYSSEYVK_VERSION);
#endif
  {
    std::string os, nombre = "?";
    if (readSmallFile("/etc/os-release", os))
      if (const size_t i = os.find("PRETTY_NAME=\""); i != std::string::npos) nombre = os.substr(i + 13, os.find('"', i + 13) - i - 13);
    struct utsname un{};
    ::uname(&un);
    const char *esc = std::getenv("XDG_CURRENT_DESKTOP"), *ses = std::getenv("XDG_SESSION_TYPE");
    renglon("Sistema", nombre + " · " + un.release + " · " + (esc ? esc : "?") + " · " + (ses ? ses : "?"));
  }
  {
    std::string g;
    for (const monitor::Tarjeta &ta : monitor::leer().tarjetas) g += (g.empty() ? "" : " · ") + ta.nombre;
    std::string nv;
    if (readSmallFile("/proc/driver/nvidia/version", nv)) {   /* «... Kernel Module for x86_64  620.73.04  Release Build» */
      size_t i = nv.find("x86_64");
      if (i != std::string::npos) {
        i = nv.find_first_of("0123456789", i + 6);
        if (i != std::string::npos) g += " · NVIDIA " + nv.substr(i, nv.find_first_not_of("0123456789.", i) - i);
      }
    }
    renglon("Tarjetas", g.empty() ? "?" : g);
  }
  {
    bool scaler = false;
    std::error_code ec;
    for (const auto &e : std::filesystem::directory_iterator("/sys/class/scsi_generic", ec)) {
      std::string v;
      if (readSmallFile((e.path() / "device" / "vendor").string(), v) && v.find("GCREADER") != std::string::npos) scaler = true;
    }
    auto sn = [](bool b) { return std::string(b ? "sí" : "no"); };
    renglon("Monitor 3D", "lente " + sn(hayUsb("354b", "0116")) + " · scaler " + sn(scaler) + " · cámara " + sn(hayUsb("04e8", "20d7")));
  }
  {
    std::string hub;
    if (readSmallFile(rutaEstadoOdyssey("hub"), hub)) {
      t += "Vigilante:\n";
      size_t i = 0;
      while (i < hub.size()) {
        const size_t f = std::min(hub.find('\n', i), hub.size());
        const std::string l = hub.substr(i, f - i);
        if (!l.empty() && l[0] != '#') t += "  " + l + "\n";
        i = f + 1;
      }
    }
  }
  {
    std::string reg;
    runCapture("journalctl --user -u odysseyvk-serve -b --no-pager -o cat -n 1500 2>/dev/null", reg);
    const std::vector<std::string> v = filtrarRegistro(reg);
    t += "Registro del servicio (" + std::to_string(v.size()) + " renglones):\n";
    for (const std::string &l : v) t += "  " + l + "\n";
  }
  {
    std::string cd;
    runCapture("coredumpctl list --since -2d --no-pager -q 2>/dev/null | grep -E 'odysseyvk|gamescope' | tail -5 | awk '{print $2, $3, $(NF-3), $(NF-1)}'", cd);
    if (!strTrim(cd).empty()) {
      t += "Caídas de los dos últimos días:\n";
      size_t i = 0;
      while (i < cd.size()) {
        const size_t f = std::min(cd.find('\n', i), cd.size());
        if (f > i) t += "  " + cd.substr(i, f - i) + "\n";
        i = f + 1;
      }
    }
  }
  const char *casa = std::getenv("HOME"), *usu = std::getenv("USER");
  char equipo[256] = "";
  ::gethostname(equipo, sizeof equipo - 1);
  return limpiar(t, casa ? casa : "", usu ? usu : "", equipo);
}

/* una copia en ~/.local/state/odysseyvk/informes (se quedan los cinco ultimos); devuelve su ruta, o vacio */
inline std::string guardar(const std::string &texto) {
  const std::string dir = rutaEstadoOdyssey("informes");
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  char cuando[32];
  const time_t ahora = std::time(nullptr);
  std::strftime(cuando, sizeof cuando, "%Y%m%d-%H%M%S", std::localtime(&ahora));
  const std::string ruta = dir + "/informe-" + cuando + ".md";
  FILE *f = std::fopen(ruta.c_str(), "we");
  if (!f) return {};
  std::fwrite(texto.data(), 1, texto.size(), f);
  std::fclose(f);
  std::vector<std::string> todos;
  for (const auto &e : std::filesystem::directory_iterator(dir, ec))
    if (e.path().filename().string().rfind("informe-", 0) == 0) todos.push_back(e.path().string());
  std::sort(todos.begin(), todos.end());
  for (size_t k = 0; k + 5 < todos.size(); k++) std::filesystem::remove(todos[k], ec);
  return ruta;
}

}  // namespace informe
