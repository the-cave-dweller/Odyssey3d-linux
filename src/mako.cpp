/* (v1.18) la generacion de cuadros de MAKO: su conf.toml (ver mako.h) */
#include "mako.h"

#include "util.h"

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace mako {
namespace {

/* una linea sin su comentario (un # fuera de comillas) y sin espacios a los lados */
std::string sinComentario(const std::string &l) {
  char comilla = 0;
  for (size_t i = 0; i < l.size(); i++) {
    const char c = l[i];
    if (comilla) {
      if (c == comilla) comilla = 0;
    } else if (c == '\'' || c == '"') {
      comilla = c;
    } else if (c == '#') {
      return strTrim(l.substr(0, i));
    }
  }
  return strTrim(l);
}

/* la clave de una linea «clave = valor» (vacio si no es eso) y su valor */
bool parClave(const std::string &linea, std::string &clave, std::string &valor) {
  const std::string l = sinComentario(linea);
  const size_t ig = l.find('=');
  if (l.empty() || l[0] == '[' || ig == std::string::npos) return false;
  clave = strTrim(l.substr(0, ig));
  valor = strTrim(l.substr(ig + 1));
  return !clave.empty();
}

/* las cadenas entre comillas de un valor («'a'», «['a', "b"]») */
std::vector<std::string> cadenas(const std::string &v) {
  std::vector<std::string> r;
  for (size_t i = 0; i < v.size(); i++) {
    const char c = v[i];
    if (c != '\'' && c != '"') continue;
    const size_t f = v.find(c, i + 1);
    if (f == std::string::npos) break;
    r.push_back(v.substr(i + 1, f - i - 1));
    i = f;
  }
  return r;
}

std::string sinComillas(const std::string &v) {
  const std::vector<std::string> c = cadenas(v);
  return c.empty() ? v : c.front();
}

/* un [[profile]]: de la linea `ini` (la cabecera) a `fin` (sin incluir) */
struct Bloque {
  size_t ini = 0, fin = 0;
  std::vector<std::string> juegos;   /* su active_in */
};

std::vector<Bloque> bloques(const std::vector<std::string> &lineas) {
  std::vector<Bloque> r;
  for (size_t i = 0; i < lineas.size(); i++) {
    if (sinComentario(lineas[i]) != "[[profile]]") continue;
    Bloque b;
    b.ini = i;
    size_t j = i + 1;
    while (j < lineas.size()) {
      const std::string l = sinComentario(lineas[j]);
      if (!l.empty() && l[0] == '[' && (l.rfind("[[", 0) == 0 || l.find('=') == std::string::npos)) break;   /* otra tabla */
      std::string k, v;
      if (parClave(lineas[j], k, v) && k == "active_in") {
        std::string todo = v;
        /* una lista que sigue en las lineas de abajo, hasta su «]» */
        if (!v.empty() && v[0] == '[' && v.find(']') == std::string::npos)
          while (j + 1 < lineas.size()) {
            j++;
            const std::string m = sinComentario(lineas[j]);
            todo += " " + m;
            if (m.find(']') != std::string::npos) break;
          }
        b.juegos = cadenas(todo);
      }
      j++;
    }
    b.fin = j;
    r.push_back(b);
    i = j - 1;
  }
  return r;
}

bool nombra(const Bloque &b, const std::string &exe) {
  for (const std::string &j : b.juegos)
    if (j == exe || strLower(j) == strLower(exe)) return true;
  return false;
}

bool leerLineas(std::vector<std::string> &lineas) {
  std::ifstream f(rutaConf());
  if (!f) return false;
  std::string l;
  while (std::getline(f, l)) lineas.push_back(l);
  return true;
}

std::string citar(const std::string &s) {
  if (s.find('\'') == std::string::npos) return "'" + s + "'";
  std::string r = "\"";
  for (char c : s) r += (c == '"' || c == '\\') ? std::string("\\") + c : std::string(1, c);
  return r + "\"";
}

/* `mako-cli validate -c`: cierto si valida o si no esta mako-cli (entonces no se puede mirar) */
bool valida(const std::string &ruta, std::string *err) {
  std::string orden = "timeout 15 mako-cli validate -c '";
  for (char c : ruta) orden += c == '\'' ? std::string("'\\''") : std::string(1, c);
  orden += "' 2>&1";
  std::string sal;
  const int rc = runCapture(orden, sal);
  if (rc == 127 || sal.find("not found") != std::string::npos) return true;   /* sin mako-cli */
  if (rc == 0 && sal.find("success") != std::string::npos) return true;
  if (err) *err = strTrim(sal.empty() ? std::string("mako-cli no la dio por buena") : sal);
  return false;
}

/* la copia de antes, una vez por proceso (y la primera de todas se queda) */
void respaldar(const std::string &ruta) {
  static bool hecho = false;
  if (hecho || !fileExists(ruta)) return;
  hecho = true;
  const std::string dir = rutaEstadoOdyssey("mako");
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  char fecha[32];
  const std::time_t t = std::time(nullptr);
  std::strftime(fecha, sizeof fecha, "%Y%m%d-%H%M%S", std::localtime(&t));
  std::filesystem::copy_file(ruta, dir + "/conf-" + fecha + ".toml", std::filesystem::copy_options::overwrite_existing, ec);
  if (!fileExists(dir + "/conf-antes-de-odysseyvk.toml"))
    std::filesystem::copy_file(ruta, dir + "/conf-antes-de-odysseyvk.toml", ec);
}

}  // namespace

bool instalado() {
  static const bool hay = [] {
    std::vector<std::string> dirs = {"/usr/share/vulkan/implicit_layer.d", "/usr/local/share/vulkan/implicit_layer.d",
                                     "/etc/vulkan/implicit_layer.d"};
    if (const char *h = std::getenv("HOME")) dirs.push_back(std::string(h) + "/.local/share/vulkan/implicit_layer.d");
    for (const std::string &d : dirs)
      if (fileExists(d + "/VkLayer_MAKO_render.json")) return true;
    return false;
  }();
  return hay;
}

std::string rutaConf() {
  if (const char *c = std::getenv("MAKO_CONFIG"); c && *c) return c;
  const char *xc = std::getenv("XDG_CONFIG_HOME");
  const char *h = std::getenv("HOME");
  return (xc && *xc ? std::string(xc) : std::string(h ? h : "") + "/.config") + "/mako-render/conf.toml";
}

Perfil leer(const std::string &exe) {
  Perfil p;
  std::vector<std::string> lineas;
  if (exe.empty() || !leerLineas(lineas)) return p;
  for (const Bloque &b : bloques(lineas)) {
    if (!nombra(b, exe)) continue;
    p.existe = true;
    for (size_t i = b.ini + 1; i < b.fin; i++) {
      std::string k, v;
      if (!parClave(lineas[i], k, v)) continue;
      if (k == "name") p.nombre = sinComillas(v);
      else if (k == "frame_generation_enabled") p.puesta = v == "true";
      else if (k == "multiplier") p.multiplicador = std::atoi(v.c_str());
      else if (k == "flow_scale") p.flujo = std::atof(v.c_str());
      else if (k == "performance_mode") p.rendimiento = v == "true";
      else if (k == "gpu") p.tarjeta = sinComillas(v);
    }
    break;
  }
  return p;
}

bool fijar(const std::string &exe, const std::map<std::string, std::string> &claves, std::string *err) {
  if (exe.empty()) {
    if (err) *err = "no se sabe el ejecutable del juego";
    return false;
  }
  const std::string ruta = rutaConf();
  std::vector<std::string> lineas;
  const bool habia = leerLineas(lineas);
  if (!habia) lineas = {"version = 2", ""};
  const std::vector<Bloque> bs = bloques(lineas);
  const Bloque *b = nullptr;
  for (const Bloque &x : bs)
    if (nombra(x, exe)) {
      b = &x;
      break;
    }
  if (b) {
    /* el perfil que ya nombra al juego: cada clave en su linea, o detras de `name` (o de la cabecera) */
    size_t fin = b->fin, tras = b->ini + 1;
    for (size_t i = b->ini + 1; i < fin; i++) {
      std::string k, v;
      if (parClave(lineas[i], k, v) && k == "name") tras = i + 1;
    }
    for (const auto &[clave, valor] : claves) {
      bool puesta = false;
      for (size_t i = b->ini + 1; i < fin; i++) {
        std::string k, v;
        if (parClave(lineas[i], k, v) && k == clave) {
          if (valor.empty()) {
            lineas.erase(lineas.begin() + (long)i);   /* quitarla */
            fin--;
            if (tras > i) tras--;
          } else {
            lineas[i] = clave + " = " + valor;
          }
          puesta = true;
          break;
        }
      }
      if (!puesta && !valor.empty()) {
        lineas.insert(lineas.begin() + (long)tras, clave + " = " + valor);
        fin++, tras++;
      }
    }
  } else {
    /* uno nuevo, al final */
    if (!lineas.empty() && !strTrim(lineas.back()).empty()) lineas.push_back("");
    lineas.push_back("[[profile]]");
    lineas.push_back("name = " + citar("OdysseyVK · " + exe));
    lineas.push_back("active_in = " + citar(exe));
    if (!claves.count("frame_generation_provisioned")) lineas.push_back("frame_generation_provisioned = true");
    for (const auto &[clave, valor] : claves)
      if (!valor.empty()) lineas.push_back(clave + " = " + valor);
  }
  std::string texto;
  for (const std::string &l : lineas) texto += l + "\n";
  /* aparte, comprobado, y despues de golpe */
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(ruta).parent_path(), ec);
  const std::string tmp = ruta + ".odysseyvk-nuevo";
  {
    std::ofstream o(tmp, std::ios::trunc);
    if (!o) {
      if (err) *err = "no se puede escribir " + tmp + ": " + std::strerror(errno);
      return false;
    }
    o << texto;
  }
  if (!valida(tmp, err)) {
    std::remove(tmp.c_str());
    return false;
  }
  struct stat sb {};
  if (habia && ::stat(ruta.c_str(), &sb) == 0) ::chmod(tmp.c_str(), sb.st_mode & 07777);
  else ::chmod(tmp.c_str(), 0600);
  respaldar(ruta);
  if (std::rename(tmp.c_str(), ruta.c_str()) != 0) {
    if (err) *err = std::string("no se pudo cambiar el fichero: ") + std::strerror(errno);
    std::remove(tmp.c_str());
    return false;
  }
  return true;
}

std::vector<std::string> tarjetas() {
  namespace fs = std::filesystem;
  std::vector<std::pair<std::string, std::string>> v;   /* direccion PCI, nombre */
  std::error_code ec;
  for (const fs::directory_entry &d : fs::directory_iterator("/sys/bus/pci/devices", ec)) {
    std::string fab, clase, modelo;
    if (!readSmallFile(d.path().string() + "/vendor", fab) || strLower(strTrim(fab)) != "0x10de") continue;
    if (!readSmallFile(d.path().string() + "/class", clase) || strTrim(clase).rfind("0x03", 0) != 0) continue;
    readSmallFile(d.path().string() + "/device", modelo);
    modelo = strLower(strTrim(modelo));
    if (modelo.rfind("0x", 0) == 0) modelo = modelo.substr(2);
    std::string n = nombrePciIds("10de", modelo);   /* «GeForce RTX 5080» */
    if (n.empty() || n.find('/') != std::string::npos) continue;   /* sin un nombre de una sola tarjeta, no */
    v.push_back({d.path().filename().string(), "NVIDIA " + n});
  }
  std::sort(v.begin(), v.end());
  std::vector<std::string> r;
  for (const auto &x : v) r.push_back(x.second);
  return r;
}

}  // namespace mako
