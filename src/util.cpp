#include "util.h"

#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>

std::string strTrim(const std::string &s) {
  size_t a = 0, b = s.size();
  while (a < b && (unsigned char)s[a] <= ' ') a++;
  while (b > a && (unsigned char)s[b - 1] <= ' ') b--;
  return s.substr(a, b - a);
}

std::string strLower(std::string s) {
  for (char &c : s)
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
  return s;
}

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::vector<uint8_t> unhex(const std::string &s) {
  std::vector<uint8_t> out;
  int hi = -1;
  for (char c : s) {
    int v = hexVal(c);
    if (v < 0) continue; /* ignora separadores */
    if (hi < 0) {
      hi = v;
    } else {
      out.push_back((uint8_t)((hi << 4) | v));
      hi = -1;
    }
  }
  return out;
}

std::string toHexUpper(const uint8_t *p, size_t n) {
  static const char *D = "0123456789ABCDEF";
  std::string s;
  s.reserve(2 * n);
  for (size_t i = 0; i < n; i++) {
    s.push_back(D[p[i] >> 4]);
    s.push_back(D[p[i] & 15]);
  }
  return s;
}

std::string toHexUpper(const std::vector<uint8_t> &v) {
  return toHexUpper(v.data(), v.size());
}

bool readSmallFile(const std::string &path, std::string &out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

/* ------ del nombre viejo (odyssey3d) al nuevo (odysseyvk) ------ */

/* Copia `de` dentro de `a` sin pisar lo que ya esta. Solo archivos: los directorios se crean. */
static int copiarArbol(const std::filesystem::path &de, const std::filesystem::path &a) {
  std::error_code ec;
  if (!std::filesystem::is_directory(de, ec)) return 0;
  int n = 0;
  for (std::filesystem::recursive_directory_iterator it(de, std::filesystem::directory_options::skip_permission_denied, ec), fin;
       it != fin; it.increment(ec)) {
    if (ec) break;
    const std::filesystem::path &orig = it->path();
    if (!it->is_regular_file(ec) || ec) continue;
    std::filesystem::path destino = a / std::filesystem::relative(orig, de, ec);
    if (ec) continue;
    if (std::filesystem::exists(destino, ec)) continue;   /* lo nuevo manda: no se pisa nada */
    std::filesystem::create_directories(destino.parent_path(), ec);
    std::filesystem::copy_file(orig, destino, std::filesystem::copy_options::skip_existing, ec);
    if (!ec) n++;
  }
  return n;
}

int migrarDatosDeViejoNombre() {
  static int hecho = -1;
  if (hecho >= 0) return hecho;
  const char *home = std::getenv("HOME");
  const std::string h = home ? home : "/home";
  const char *xdgState = std::getenv("XDG_STATE_HOME");
  struct Pareja { std::string viejo, nuevo; };
  const Pareja carpetas[] = {
      {h + "/.config/odyssey3d", h + "/.config/odysseyvk"},
      {(xdgState && *xdgState ? std::string(xdgState) + "/odyssey3d" : h + "/.local/state/odyssey3d"),
       (xdgState && *xdgState ? std::string(xdgState) + "/odysseyvk" : h + "/.local/state/odysseyvk")},
      {h + "/.local/share/odyssey3d", h + "/.local/share/odysseyvk"},
  };
  int tot = 0;
  for (const Pareja &c : carpetas) {
    std::error_code ec;
    if (!std::filesystem::is_directory(c.viejo, ec)) continue;
    const int n = copiarArbol(c.viejo, c.nuevo);
    if (n > 0) {
      std::printf("[nombre] copiados %d archivos de %s a %s (lo del nombre viejo se deja intacto)\n", n,
                  c.viejo.c_str(), c.nuevo.c_str());
      tot += n;
    }
  }
  hecho = tot;
  return hecho;
}

const char *entornoOdyssey(const char *nombre) {
  if (!nombre || !*nombre) return nullptr;
  if (const char *v = std::getenv(nombre)) return v;
  /* el nombre viejo es el mismo con ODYSSEY3D_ en lugar de ODYSSEYVK_ */
  static const std::string pref = "ODYSSEYVK_", viejo = "ODYSSEY3D_";
  if (std::string(nombre).compare(0, pref.size(), pref) != 0) return nullptr;
  static std::string altern;   /* getenv necesita que la cadena siga viva */
  altern = viejo + (nombre + pref.size());
  return std::getenv(altern.c_str());
}

std::string rutaConfigOdyssey(const char *nombre) {
  const char *home = std::getenv("HOME");
  return std::string(home ? home : "/home") + "/.config/odysseyvk/" + nombre;
}

std::string rutaEstadoOdyssey(const char *nombre) {
  const char *xdg = std::getenv("XDG_STATE_HOME");
  std::string dir;
  if (xdg && *xdg)
    dir = std::string(xdg) + "/odysseyvk";
  else {
    const char *home = std::getenv("HOME");
    dir = std::string(home ? home : "/home") + "/.local/state/odysseyvk";
  }
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(dir), ec);
  return dir + "/" + nombre;
}

std::string rutaRedNuestra() {
  const char *home = std::getenv("HOME");
  return std::string(home ? home : "/home") + "/.local/share/odysseyvk/modelos/depth_anything_v2_small.onnx";
}

int slotRed(const std::string &ruta) {
  const std::string s = strLower(ruta);
  return (s.find("mde_") != std::string::npos || s.find("disp_") != std::string::npos) ? 1 : 2;
}

int leerRedPreferida() {
  std::string txt;
  if (!readSmallFile(rutaConfigOdyssey("red-elegida"), txt)) return 0;
  txt = strTrim(txt);
  return txt == "1" ? 1 : (txt == "2" ? 2 : 0);
}

void grabarRedPreferida(int red) {
  if (red != 1 && red != 2) return;
  const std::string ruta = rutaConfigOdyssey("red-elegida");
  if (FILE *f = std::fopen(ruta.c_str(), "we")) {   /* 'e': sin permiso de ejecucion */
    std::fprintf(f, "%d\n", red);
    std::fclose(f);
  }
}

bool fileExists(const std::string &path) {
  return !path.empty() && access(path.c_str(), R_OK) == 0;
}

bool readBinaryFile(const std::string &path, std::vector<uint8_t> &out) {
  out.clear();
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  f.seekg(0, std::ios::end);
  const std::streamoff n = f.tellg();
  if (n <= 0 || n > (std::streamoff)64 * 1024 * 1024) return false;
  f.seekg(0, std::ios::beg);
  out.resize((size_t)n);
  f.read((char *)out.data(), n);
  if (!f) {
    out.clear();
    return false;
  }
  return true;
}

int runCapture(const std::string &cmd, std::string &out) {
  std::array<char, 4096> buf{};
  FILE *p = popen(cmd.c_str(), "r");
  if (!p) return -1;
  while (fgets(buf.data(), (int)buf.size(), p)) out += buf.data();
  int rc = pclose(p);
  return WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
}

std::string usbIdToHex(uint16_t v) {
  char b[8];
  snprintf(b, sizeof b, "%04x", v);
  return b;
}

#include <cstdlib>
#include <filesystem>
#include <unistd.h>

std::string ensureWaylandEnv() {
  const char *wd = getenv("WAYLAND_DISPLAY");
  if (wd && *wd) return wd;
  const char *xdg = getenv("XDG_RUNTIME_DIR");
  std::string dir = (xdg && *xdg) ? xdg : ("/run/user/" + std::to_string((long long)getuid()));
  std::error_code ec;
  if (!std::filesystem::is_directory(dir, ec)) return {};
  std::vector<std::string> found;
  for (const auto &e : std::filesystem::directory_iterator(dir, ec)) {
    std::string n = e.path().filename().string();
    if (n.rfind("wayland-", 0) == 0 && std::filesystem::is_socket(e.path(), ec)) found.push_back(n);
  }
  if (found.empty()) return {};
  std::sort(found.begin(), found.end());
  setenv("WAYLAND_DISPLAY", found[0].c_str(), 1);
  setenv("XDG_RUNTIME_DIR", dir.c_str(), 1);
  return found[0];
}
