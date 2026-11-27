/* capasajenas.cpp — ver capasajenas.h */
#include "capasajenas.h"

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <string>

#include "util.h"

namespace ajenas {
namespace {

double ahora() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

/* el arranque de un proceso (el campo 22 de /proc/<pid>/stat), como lo cuenta vkDaedalus (prefix_holder.hpp) */
bool arranque(long pid, uint64_t &ticks) {
  std::string st;
  if (!readSmallFile("/proc/" + std::to_string(pid) + "/stat", st)) return false;
  const size_t p = st.rfind(')');
  if (p == std::string::npos) return false;
  const char *s = st.c_str() + p + 1;
  for (int campo = 3; campo <= 22; campo++) {
    while (*s == ' ') s++;
    if (!*s) return false;
    if (campo == 22) {
      char *fin = nullptr;
      ticks = std::strtoull(s, &fin, 10);
      return fin != s;
    }
    while (*s && *s != ' ') s++;
  }
  return false;
}

/* el menu de vkDaedalus abierto en algun juego (ver capasajenas.h) */
bool menuAbierto() {
  const char *rt = std::getenv("ODYSSEYVK_AJENAS_DIR");   /* para las pruebas: otra carpeta que $XDG_RUNTIME_DIR */
  if (!rt || !*rt) rt = std::getenv("XDG_RUNTIME_DIR");
  if (!rt || !*rt) return false;
  DIR *d = ::opendir(rt);
  if (!d) return false;
  bool abierto = false;
  while (dirent *e = ::readdir(d)) {
    if (std::strncmp(e->d_name, "vkdaedalus-input-", 17) != 0) continue;
    const std::string r = std::string(rt) + "/" + e->d_name;
    const int fd = ::open(r.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) continue;
    uint64_t quien = 0;
    const bool leido = ::pread(fd, &quien, sizeof quien, 0) == (ssize_t)sizeof quien;
    ::close(fd);
    if (!leido || quien == 0) continue;
    const long pid = (long)(int32_t)(quien & 0xFFFFFFFFu);
    uint64_t t = 0;
    if (pid > 0 && arranque(pid, t) && ((t << 32) | (quien & 0xFFFFFFFFu)) == quien) {
      abierto = true;
      break;
    }
  }
  ::closedir(d);
  return abierto;
}

/* `clave = valor` de un fichero de ajustes (el ultimo que haya) */
std::string valorConf(const std::string &txt, const std::string &clave) {
  std::string v;
  size_t i = 0;
  while (i < txt.size()) {
    const size_t f = std::min(txt.find('\n', i), txt.size());
    std::string l = strTrim(txt.substr(i, f - i));
    i = f + 1;
    const size_t ig = l.find('=');
    if (l.empty() || l[0] == '#' || ig == std::string::npos) continue;
    if (strTrim(l.substr(0, ig)) == clave) v = strTrim(l.substr(ig + 1));
  }
  return v;
}

/* lo de MANGOHUD_CONFIG («fps,position=top-right,no_display»): la clave y su valor ("" si va sola) */
std::map<std::string, std::string> opcionesMango(const std::string &s) {
  std::map<std::string, std::string> o;
  size_t i = 0;
  while (i <= s.size()) {
    const size_t f = std::min(s.find(',', i), s.size());
    const std::string t = strTrim(s.substr(i, f - i));
    i = f + 1;
    if (t.empty()) continue;
    const size_t ig = t.find('=');
    o[strTrim(t.substr(0, ig))] = ig == std::string::npos ? "" : strTrim(t.substr(ig + 1));
  }
  return o;
}

/* la esquina de un HUD de ancho `w` y alto `h` (de 0 a 1) segun su `position` de MangoHud */
Zona esquina(const std::string &pos, float w, float h) {
  const bool der = pos.find("right") != std::string::npos, centroX = pos.find("center") != std::string::npos;
  const bool abajo = pos.find("bottom") != std::string::npos, medio = pos.find("middle") != std::string::npos;
  const float x0 = der ? 1.0f - w : (centroX ? 0.5f - w * 0.5f : 0.0f);
  const float y0 = abajo ? 1.0f - h : (medio ? 0.5f - h * 0.5f : 0.0f);
  return {x0, y0, x0 + w, y0 + h};
}

struct Lento {
  pid_t pid = -1;
  double cuando = -1e9;
  bool mango = false, vkd = false, hudVkd = false, fpsVkd = false;
  std::string posMango = "top-left";
};

void mirarJuego(pid_t pid, Lento &l) {
  l = Lento{};
  l.pid = pid;
  if (pid <= 0) return;
  const std::string base = "/proc/" + std::to_string((long)pid);
  /* sus bibliotecas: la de MangoHud y la de vkDaedalus (libvkdaedalus.so; su gancho de entrada es otra) */
  std::ifstream maps(base + "/maps");
  std::string linea;
  while (std::getline(maps, linea)) {
    if (!l.mango && linea.find("libMangoHud") != std::string::npos) l.mango = true;
    if (!l.vkd && linea.find("/libvkdaedalus.so") != std::string::npos) l.vkd = true;
    if (l.mango && l.vkd) break;
  }
  /* su entorno: MANGOHUD_CONFIG y el fichero de MangoHud que use */
  std::string env;
  {
    std::ifstream f(base + "/environ", std::ios::binary);
    env.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  }
  std::map<std::string, std::string> vars;
  for (size_t i = 0; i < env.size();) {
    const size_t f = std::min(env.find('\0', i), env.size());
    const std::string kv = env.substr(i, f - i);
    i = f + 1;
    const size_t ig = kv.find('=');
    if (ig != std::string::npos) vars[kv.substr(0, ig)] = kv.substr(ig + 1);
  }
  if (l.mango) {
    std::map<std::string, std::string> op;
    if (vars.count("MANGOHUD_CONFIG")) op = opcionesMango(vars["MANGOHUD_CONFIG"]);
    std::string fichero = vars.count("MANGOHUD_CONFIGFILE") ? vars["MANGOHUD_CONFIGFILE"] : std::string();
    if (fichero.empty()) {
      const char *xc = std::getenv("XDG_CONFIG_HOME");
      const char *home = std::getenv("HOME");
      fichero = (xc && *xc ? std::string(xc) : std::string(home ? home : "") + "/.config") + "/MangoHud/MangoHud.conf";
    }
    std::string txt;
    if (readSmallFile(fichero, txt)) {
      if (!op.count("position") && !valorConf(txt, "position").empty()) op["position"] = valorConf(txt, "position");
      if (!op.count("no_display") && txt.find("\nno_display") != std::string::npos) op["no_display"] = "";
    }
    if (op.count("no_display")) l.mango = false;   /* cargado pero escondido */
    if (op.count("position") && !op["position"].empty()) l.posMango = op["position"];
  }
  if (l.vkd) {
    const char *xc = std::getenv("XDG_CONFIG_HOME");
    const char *home = std::getenv("HOME");
    const std::string conf = (xc && *xc ? std::string(xc) : std::string(home ? home : "") + "/.config") + "/vkdaedalus/settings.conf";
    std::string txt;
    if (readSmallFile(conf, txt)) {
      l.hudVkd = strLower(valorConf(txt, "hud")) == "true";
      const std::string items = "," + valorConf(txt, "hudItems") + ",";
      l.fpsVkd = l.hudVkd && items.find(",fps,") != std::string::npos;
    }
  }
}

}  // namespace

Estado mirar(pid_t pid) {
  static Lento l;
  const double t = ahora();
  if (pid != l.pid || t - l.cuando > 3.0) {
    mirarJuego(pid, l);
    l.cuando = t;
  }
  Estado e;
  e.menuVkd = menuAbierto();
  e.mangohud = l.mango;
  e.hudVkd = l.hudVkd;
  e.fpsVkd = l.fpsVkd;
  if (e.menuVkd) {
    e.planas.push_back({0.0f, 0.0f, 1.0f, 1.0f});
    return e;
  }
  /* MEDIDO a ojo, con holgura: MangoHud de fabrica (FPS, tiempos, CPU, GPU) mide unos 400 x 300 en 1080p; el HUD
     de vkDaedalus, de uno a tres renglones arriba a la izquierda */
  if (e.mangohud) e.planas.push_back(esquina(l.posMango, 0.22f, 0.30f));
  if (e.hudVkd) e.planas.push_back({0.0f, 0.0f, 0.20f, 0.12f});
  return e;
}

}  // namespace ajenas
