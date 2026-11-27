/* integraciones.cpp — ver integraciones.h */
#include "integraciones.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>

#include "util.h"

namespace integ {
namespace {

/* las variables con su interruptor, en el orden en que van en la linea */
struct Variable {
  const char *clave, *nombre;
};
const Variable kVariables[] = {{"odysseyvk", "ODYSSEYVK"},
                               {"mangohud", "MANGOHUD"},
                               {"vkdaedalus", "ENABLE_VKDAEDALUS"},
                               {"neural_forge", "NEURAL_FORGE_ENABLE"},
                               {"vkbasalt", "ENABLE_VKBASALT"}};

bool esAppid(const std::string &s) {
  return !s.empty() && s.size() < 12 && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
}
bool esNumero(const std::string &s) { return esAppid(s) && s.size() < 6; }

std::string unir(const std::vector<std::string> &v) {
  std::string s;
  for (const std::string &x : v) {
    if (x.empty()) continue;
    if (!s.empty()) s += ' ';
    s += x;
  }
  return s;
}

/* «2048x1536» → 2048 y 1536 */
bool medida(const std::string &s, std::string &w, std::string &h) {
  const size_t x = s.find('x');
  if (x == std::string::npos) return false;
  w = s.substr(0, x), h = s.substr(x + 1);
  return esNumero(w) && esNumero(h);
}

/* un valor que la linea de Steam lleva tal cual o entre comillas dobles */
bool sinComillas(const std::string &v) {
  if (v.empty()) return false;
  for (unsigned char c : v)
    if (!(std::isalnum(c) || std::strchr("._,+-/:@%=^~", c) || c >= 0x80)) return false;
  return true;
}
std::string citar(const std::string &v) {
  if (sinComillas(v)) return v;
  std::string s = "\"";
  for (char c : v) {
    if (c == '"' || c == '\\' || c == '$' || c == '`') s += '\\';
    s += c;
  }
  return s + "\"";
}

/* ---- la linea de lanzamiento, partida como la parte un shell ---- */
struct Pieza {
  std::string texto;   /* sin comillas */
  size_t ini = 0, fin = 0;   /* donde esta en la linea, con sus comillas */
};
/* falso si la linea lleva algo que no se modela: `;`, `&`, `|`, redirecciones, `$(...)`, comillas sin cerrar */
bool partir(const std::string &s, std::vector<Pieza> &out) {
  out.clear();
  size_t i = 0;
  while (i < s.size()) {
    while (i < s.size() && std::isspace((unsigned char)s[i])) i++;
    if (i >= s.size()) break;
    Pieza p;
    p.ini = i;
    if (s[i] == '#') return false;   /* un comentario */
    while (i < s.size() && !std::isspace((unsigned char)s[i])) {
      const char c = s[i];
      if (c == '\'') {
        const size_t q = s.find('\'', i + 1);
        if (q == std::string::npos) return false;
        p.texto += s.substr(i + 1, q - i - 1);
        i = q + 1;
      } else if (c == '"') {
        i++;
        while (i < s.size() && s[i] != '"') {
          if (s[i] == '\\' && i + 1 < s.size() && std::strchr("\"\\$`", s[i + 1])) {
            p.texto += s[i + 1];
            i += 2;
            continue;
          }
          if (s[i] == '`' || (s[i] == '$' && i + 1 < s.size() && s[i + 1] == '(')) return false;
          p.texto += s[i++];
        }
        if (i >= s.size()) return false;
        i++;
      } else if (c == '\\') {
        if (i + 1 < s.size()) p.texto += s[i + 1];
        i += 2;
      } else if (std::strchr(";&|<>()`", c) || (c == '$' && i + 1 < s.size() && s[i + 1] == '(')) {
        return false;
      } else {
        p.texto += c;
        i++;
      }
    }
    p.fin = std::min(i, s.size());
    out.push_back(p);
  }
  return true;
}

bool esAsignacion(const std::string &x) {
  const size_t ig = x.find('=');
  if (ig == std::string::npos || ig == 0) return false;
  if (!(std::isalpha((unsigned char)x[0]) || x[0] == '_')) return false;
  for (size_t k = 1; k < ig; k++)
    if (!(std::isalnum((unsigned char)x[k]) || x[k] == '_')) return false;
  return true;
}

std::string nombreBase(const std::string &s) {
  const size_t b = s.rfind('/');
  return b == std::string::npos ? s : s.substr(b + 1);
}

/* ---- VDF (los ficheros de texto de Steam), con donde esta cada cosa para poder cambiarla ---- */
struct NodoVdf {
  std::string clave;
  bool objeto = false;
  std::string valor;               /* sin escapes */
  size_t valIni = 0, valFin = 0;   /* el valor entre sus comillas */
  size_t abre = 0, cierra = 0;     /* las llaves */
  std::vector<NodoVdf> hijos;
  const NodoVdf *hijo(const std::string &k) const {
    for (const NodoVdf &h : hijos)
      if (strLower(h.clave) == strLower(k)) return &h;
    return nullptr;
  }
};

struct LectorVdf {
  const std::string &t;
  size_t i = 0;
  enum Tipo { Fin, Texto, Abre, Cierra };
  struct Ficha {
    Tipo tipo = Fin;
    std::string texto;
    size_t ini = 0, fin = 0;
  };
  explicit LectorVdf(const std::string &txt) : t(txt) {}
  Ficha siguiente() {
    for (;;) {
      while (i < t.size() && std::isspace((unsigned char)t[i])) i++;
      if (i + 1 < t.size() && t[i] == '/' && t[i + 1] == '/') {
        while (i < t.size() && t[i] != '\n') i++;
        continue;
      }
      break;
    }
    Ficha f;
    f.ini = i;
    if (i >= t.size()) return f;
    if (t[i] == '{' || t[i] == '}') {
      f.tipo = t[i] == '{' ? Abre : Cierra;
      f.fin = ++i;
      return f;
    }
    f.tipo = Texto;
    if (t[i] == '"') {
      i++;
      while (i < t.size() && t[i] != '"') {
        if (t[i] == '\\' && i + 1 < t.size()) {
          const char e = t[i + 1];
          f.texto += e == 'n' ? '\n' : (e == 't' ? '\t' : e);
          i += 2;
          continue;
        }
        f.texto += t[i++];
      }
      if (i < t.size()) i++;
    } else {
      while (i < t.size() && !std::isspace((unsigned char)t[i]) && t[i] != '{' && t[i] != '}' && t[i] != '"') f.texto += t[i++];
    }
    f.fin = i;
    return f;
  }
  /* los pares de un objeto, hasta su llave de cierre (o el final) */
  bool objeto(NodoVdf &n, bool raiz, std::string *err) {
    for (;;) {
      Ficha k = siguiente();
      if (k.tipo == Fin) {
        if (raiz) return true;
        if (err) *err = "el fichero se corta a medio objeto";
        return false;
      }
      if (k.tipo == Cierra) {
        if (raiz) {
          if (err) *err = "una llave de cierre de mas";
          return false;
        }
        n.cierra = k.ini;
        return true;
      }
      if (k.tipo != Texto) {
        if (err) *err = "una llave donde iba una clave";
        return false;
      }
      NodoVdf h;
      h.clave = k.texto;
      Ficha v = siguiente();
      if (v.tipo == Abre) {
        h.objeto = true;
        h.abre = v.ini;
        if (!objeto(h, false, err)) return false;
      } else if (v.tipo == Texto) {
        h.valor = v.texto;
        h.valIni = v.ini, h.valFin = v.fin;
        /* una condicion de plataforma detras del valor ([$WIN32]): se salta */
        const size_t guarda = i;
        Ficha c = siguiente();
        if (!(c.tipo == Texto && !c.texto.empty() && c.texto[0] == '[')) i = guarda;
      } else {
        if (err) *err = "una clave sin valor";
        return false;
      }
      n.hijos.push_back(std::move(h));
    }
  }
};

bool parsearVdf(const std::string &txt, NodoVdf &raiz, std::string *err) {
  raiz = NodoVdf{};
  raiz.objeto = true;
  LectorVdf l(txt);
  return l.objeto(raiz, true, err);
}

std::string escaparVdf(const std::string &s) {
  std::string o;
  for (char c : s) {
    if (c == '"' || c == '\\') o += '\\';
    o += c;
  }
  return o;
}

/* el objeto apps de localconfig.vdf: UserLocalConfigStore → Software → Valve → Steam → apps */
const NodoVdf *appsDe(const NodoVdf &raiz) {
  const NodoVdf *n = raiz.hijo("UserLocalConfigStore");
  for (const char *k : {"Software", "Valve", "Steam", "apps"}) {
    if (!n) return nullptr;
    n = n->hijo(k);
  }
  return n && n->objeto ? n : nullptr;
}

/* la sangria de la linea donde esta `pos` (los tabuladores del principio) */
std::string sangriaDe(const std::string &t, size_t pos) {
  size_t ini = t.rfind('\n', pos == 0 ? 0 : pos - 1);
  ini = ini == std::string::npos ? 0 : ini + 1;
  size_t k = ini;
  while (k < t.size() && (t[k] == '\t' || t[k] == ' ')) k++;
  return t.substr(ini, k - ini);
}
size_t inicioDeLinea(const std::string &t, size_t pos) {
  const size_t p = t.rfind('\n', pos == 0 ? 0 : pos - 1);
  return p == std::string::npos ? 0 : p + 1;
}

bool leerFichero(const std::string &ruta, std::string &txt, struct stat *sb = nullptr) {
  if (sb && ::stat(ruta.c_str(), sb) != 0) return false;
  return readSmallFile(ruta, txt);
}

/* escribe aparte y cambia el fichero de golpe, con los permisos que tenia */
bool escribirDeGolpe(const std::string &ruta, const std::string &txt, mode_t modo, std::string *err) {
  const std::string tmp = ruta + ".odysseyvk-nuevo";
  const int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, modo);
  if (fd < 0) {
    if (err) *err = tmp + ": " + std::strerror(errno);
    return false;
  }
  size_t hecho = 0;
  while (hecho < txt.size()) {
    const ssize_t n = ::write(fd, txt.data() + hecho, txt.size() - hecho);
    if (n <= 0) {
      if (err) *err = tmp + ": " + std::strerror(errno);
      ::close(fd);
      ::unlink(tmp.c_str());
      return false;
    }
    hecho += (size_t)n;
  }
  ::fchmod(fd, modo);
  ::fsync(fd);
  ::close(fd);
  if (::rename(tmp.c_str(), ruta.c_str()) != 0) {
    if (err) *err = ruta + ": " + std::strerror(errno);
    ::unlink(tmp.c_str());
    return false;
  }
  return true;
}

std::string rutaMarca() { return rutaEstadoOdyssey("integraciones-pendiente"); }

}  // namespace

/* ------------------------------------------------------------------------------------------------ */

const std::vector<std::string> &claves() {
  static const std::vector<std::string> k = {"odysseyvk", "mangohud",   "vkdaedalus", "neural_forge", "vkbasalt",
                                             "otros",     "vkdaedalus_run", "comando", "gamescope",  "reescalado",
                                             "filtro",    "render",     "salida",     "escala",       "pantalla_completa",
                                             "cursor",    "gs_otros",   "opciones"};
  return k;
}

const Ajustes &deFabrica() {
  static const Ajustes a = {{"odysseyvk", "0"},       {"mangohud", "0"},        {"vkdaedalus", "0"},
                            {"neural_forge", "0"},    {"vkbasalt", "0"},        {"otros", ""},
                            {"vkdaedalus_run", "0"},  {"comando", ""},          {"gamescope", "0"},
                            {"reescalado", "0"},      {"filtro", "fsr"},        {"render", "2048x1536"},
                            {"salida", "3840x2160"},  {"escala", "stretch"},    {"pantalla_completa", "1"},
                            {"cursor", "1"},          {"gs_otros", ""},         {"opciones", ""}};
  return a;
}

bool esInterruptor(const std::string &c) {
  return c == "odysseyvk" || c == "mangohud" || c == "vkdaedalus" || c == "neural_forge" || c == "vkbasalt" ||
         c == "vkdaedalus_run" || c == "gamescope" || c == "reescalado" || c == "pantalla_completa" || c == "cursor";
}

const std::vector<Resolucion> &resoluciones() {
  static const std::vector<Resolucion> r = {{"HD", "1280x720"},   {"HD+", "1600x900"},  {"FHD", "1920x1080"},
                                            {"QXGA", "2048x1536"}, {"QHD", "2560x1440"}, {"4K", "3840x2160"}};
  return r;
}

/* ---- integraciones.conf ---- */
std::string rutaConf() { return rutaConfigOdyssey("integraciones.conf"); }

Estado cargar() {
  Estado e;
  e.todos = deFabrica();
  std::string txt;
  if (!readSmallFile(rutaConf(), txt)) return e;
  std::string seccion;
  size_t p = 0;
  while (p < txt.size()) {
    size_t q = txt.find('\n', p);
    if (q == std::string::npos) q = txt.size();
    const std::string t = strTrim(txt.substr(p, q - p));
    p = q + 1;
    if (t.empty() || t[0] == '#') continue;
    if (t.front() == '[' && t.back() == ']') {
      seccion = strTrim(t.substr(1, t.size() - 2));
      continue;
    }
    const size_t ig = t.find('=');
    if (ig == std::string::npos) continue;
    const std::string k = strTrim(t.substr(0, ig)), v = strTrim(t.substr(ig + 1));
    if (seccion == "todos") {
      if (deFabrica().count(k)) e.todos[k] = v;
    } else if (esAppid(seccion)) {
      Juego &j = e.juegos[seccion];
      if (k == ".linea_steam") j.lineaSteam = v, j.visto = true;
      else if (k == ".propia") j.propia = v == "1";
      else if (k == ".nf_exe") j.nfExe = v;
      else if (deFabrica().count(k)) j.propios[k] = v;
    }
  }
  return e;
}

bool guardar(const Estado &e, std::string *err) {
  std::string t =
      "# OdysseyVK · Integraciones (la pestaña del panel): lo que va en las opciones de lanzamiento de Steam.\n"
      "# [todos] vale para cada juego; cada [appid] lleva lo suyo (lo que tiene distinto de todos) y, con un punto\n"
      "# delante, lo último que se vio en su línea de Steam. Se puede tocar a mano con el panel cerrado.\n\n[todos]\n";
  for (const std::string &k : claves()) {
    const auto it = e.todos.find(k);
    t += k + " = " + (it == e.todos.end() ? deFabrica().at(k) : it->second) + "\n";
  }
  for (const auto &[id, j] : e.juegos) {
    if (!j.visto && j.propios.empty()) continue;
    t += "\n[" + id + "]\n";
    for (const std::string &k : claves())
      if (const auto it = j.propios.find(k); it != j.propios.end()) t += k + " = " + it->second + "\n";
    if (j.visto) t += ".linea_steam = " + j.lineaSteam + "\n";
    if (j.propia) t += ".propia = 1\n";
    if (!j.nfExe.empty()) t += ".nf_exe = " + j.nfExe + "\n";
  }
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(rutaConf()).parent_path(), ec);
  return escribirDeGolpe(rutaConf(), t, 0644, err);
}

Ajustes efectivo(const Estado &e, const std::string &appid) {
  Ajustes a = deFabrica();
  for (const auto &[k, v] : e.todos) a[k] = v;
  if (!appid.empty())
    if (const auto it = e.juegos.find(appid); it != e.juegos.end())
      for (const auto &[k, v] : it->second.propios) a[k] = v;
  return a;
}

void fijar(Estado &e, const std::string &appid, const std::string &clave, const std::string &valor) {
  auto pon = [&](const std::string &k, const std::string &v) {
    if (appid.empty()) {
      e.todos[k] = v;
      return;
    }
    Juego &j = e.juegos[appid];
    const auto t = e.todos.find(k);
    if ((t != e.todos.end() ? t->second : deFabrica().at(k)) == v) j.propios.erase(k);
    else j.propios[k] = v;
  };
  pon(clave, valor);
  if (valor == "1")
    if (const char *otra = excluyente(clave); otra[0]) pon(otra, "0");   /* uno u otro */
  if (clave == "reescalado" && valor == "1") pon("gamescope", "1");
  if (clave == "gamescope" && valor == "0") pon("reescalado", "0");
}

void comoTodos(Estado &e, const std::string &appid) {
  Juego &j = e.juegos[appid];
  j.propios.clear();
  j.propia = false;
}

/* ---- la linea ---- */
std::string linea(const Ajustes &a, const std::string &nfExe) {
  auto si = [&](const char *k) {
    const auto it = a.find(k);
    return it != a.end() && it->second == "1";
  };
  auto val = [&](const char *k) -> std::string {
    const auto it = a.find(k);
    return it == a.end() ? std::string() : strTrim(it->second);
  };
  std::vector<std::string> partes;
  const bool gs = si("gamescope");
  if (gs) {
    partes.push_back("gamescope");
    std::string w, h;
    if (medida(val("render"), w, h)) partes.insert(partes.end(), {"-w", w, "-h", h});
    if (medida(val("salida"), w, h)) partes.insert(partes.end(), {"-W", w, "-H", h});
    if (!val("escala").empty()) partes.insert(partes.end(), {"-S", val("escala")});
    if (si("reescalado")) partes.insert(partes.end(), {"-F", val("filtro").empty() ? std::string("fsr") : val("filtro")});
    if (si("pantalla_completa")) partes.push_back("-f");
    if (si("cursor")) partes.push_back("--force-grab-cursor");
    if (!val("gs_otros").empty()) partes.push_back(val("gs_otros"));
    partes.push_back("--");
  }
  std::vector<std::string> vars;
  for (const Variable &v : kVariables) {
    if (!si(v.clave)) continue;
    vars.push_back(std::string(v.nombre) + "=1");
    if (std::strcmp(v.clave, "neural_forge") == 0 && !strTrim(nfExe).empty())
      vars.push_back("NEURAL_FORGE_TARGET_EXE=" + citar(strTrim(nfExe)));
  }
  if (!val("otros").empty()) vars.push_back(val("otros"));
  std::vector<std::string> delante;
  if (si("vkdaedalus_run")) delante.push_back("vkdaedalus-run");
  if (!val("comando").empty()) delante.push_back(val("comando"));
  const std::string opc = val("opciones");
  if (!gs && vars.empty() && delante.empty() && opc.empty()) return "";
  /* el formato de Rodolfo: «gamescope ... -- env (los parametros) (un comando) %command% (un flag)» */
  if (gs && !vars.empty()) partes.push_back("env");
  partes.insert(partes.end(), vars.begin(), vars.end());
  partes.insert(partes.end(), delante.begin(), delante.end());
  partes.push_back("%command%");
  if (!opc.empty()) partes.push_back(opc);
  return unir(partes);
}

bool leerLinea(const std::string &lin, Leida &l) {
  l = Leida{};
  std::vector<Pieza> t;
  if (!partir(lin, t)) return false;
  Ajustes &a = l.ajustes;
  for (const Variable &v : kVariables) a[v.clave] = "0";
  a["otros"] = "", a["vkdaedalus_run"] = "0", a["comando"] = "", a["gamescope"] = "0", a["opciones"] = "";
  if (t.empty()) return true;
  auto crudoDe = [&](const Pieza &p) { return lin.substr(p.ini, p.fin - p.ini); };
  auto crudo = [&](size_t de, size_t a0) {
    return de >= a0 ? std::string() : strTrim(lin.substr(t[de].ini, t[a0 - 1].fin - t[de].ini));
  };
  long cmd = -1;
  for (size_t i = 0; i < t.size(); i++) {
    if (t[i].texto.find("%command%") == std::string::npos) continue;
    if (cmd >= 0 || t[i].texto != "%command%" || crudoDe(t[i]) != "%command%") return false;
    cmd = (long)i;
  }
  if (cmd < 0) {
    /* sin %command%, Steam pone el juego delante: todo son opciones del juego */
    a["opciones"] = strTrim(lin);
    return true;
  }
  const size_t c = (size_t)cmd;
  a["opciones"] = crudo(c + 1, t.size());
  size_t i = 0;
  if (c > 0 && nombreBase(t[0].texto) == "gamescope") {
    a["gamescope"] = "1", a["reescalado"] = "0", a["pantalla_completa"] = "0", a["cursor"] = "0";
    std::string w, h, W, H, escala, filtro;
    std::vector<std::string> otros;
    bool fin = false;
    i = 1;
    while (i < c) {
      const std::string &x = t[i].texto;
      if (x == "--") {
        fin = true;
        i++;
        break;
      }
      auto conValor = [&](const char *corta, const char *larga, std::string &dest) {
        if (x == corta || x == larga) {
          if (i + 1 < c && t[i + 1].texto != "--") {
            dest = t[i + 1].texto;
            i += 2;
            return true;
          }
          return false;
        }
        const std::string pre = std::string(larga) + "=";
        if (x.rfind(pre, 0) == 0) {
          dest = x.substr(pre.size());
          i++;
          return true;
        }
        return false;
      };
      if (conValor("-w", "--nested-width", w) || conValor("-h", "--nested-height", h) ||
          conValor("-W", "--output-width", W) || conValor("-H", "--output-height", H) ||
          conValor("-S", "--scaler", escala) || conValor("-F", "--filter", filtro))
        continue;
      if (x == "-f" || x == "--fullscreen") a["pantalla_completa"] = "1";
      else if (x == "--force-grab-cursor") a["cursor"] = "1";
      else if (x == "-U") filtro = "fsr";   /* los de antes del -F */
      else if (x == "-Y") filtro = "nis";
      else otros.push_back(crudoDe(t[i]));
      i++;
    }
    if (!fin) return false;   /* sin `--` no se sabe donde empieza el juego */
    auto par = [&](const std::string &x, const std::string &y, const char *cx, const char *cy, const char *clave) {
      if (esNumero(x) && esNumero(y)) {
        a[clave] = x + "x" + y;
        return;
      }
      a[clave] = "";
      if (!x.empty()) otros.push_back(std::string(cx) + " " + citar(x));
      if (!y.empty()) otros.push_back(std::string(cy) + " " + citar(y));
    };
    par(w, h, "-w", "-h", "render");
    par(W, H, "-W", "-H", "salida");
    a["escala"] = escala;
    if (filtro == "fsr" || filtro == "nis") a["reescalado"] = "1", a["filtro"] = filtro;
    else if (!filtro.empty()) otros.push_back("-F " + citar(filtro));
    a["gs_otros"] = unir(otros);
  }
  /* lo de delante del juego: [env] VARIABLES... [comando...] */
  size_t r = i;
  if (r < c && t[r].texto == "env") {
    r++;
    if (r < c && !t[r].texto.empty() && t[r].texto[0] == '-') return false;   /* env -u ...: no se modela */
  }
  std::vector<std::string> otrosVars;
  std::string exe;
  for (; r < c && esAsignacion(t[r].texto); r++) {
    const std::string &x = t[r].texto;
    const size_t ig = x.find('=');
    const std::string nombre = x.substr(0, ig), valor = x.substr(ig + 1);
    bool nuestra = false;
    for (const Variable &v : kVariables)
      if (nombre == v.nombre && valor == "1" && a[v.clave] == "0") {
        a[v.clave] = "1";
        nuestra = true;
        break;
      }
    if (!nuestra && nombre == "NEURAL_FORGE_TARGET_EXE" && exe.empty()) {
      exe = valor;
      nuestra = true;
    }
    if (!nuestra) otrosVars.push_back(crudoDe(t[r]));
  }
  /* NEURAL_FORGE_TARGET_EXE sin NEURAL_FORGE_ENABLE=1 no es nuestro: se queda con las demas */
  if (!exe.empty() && a["neural_forge"] != "1") otrosVars.push_back("NEURAL_FORGE_TARGET_EXE=" + citar(exe)), exe.clear();
  l.nfExe = exe;
  a["otros"] = unir(otrosVars);
  if (r < c && nombreBase(t[r].texto) == "vkdaedalus-run") {
    a["vkdaedalus_run"] = "1";
    r++;
  }
  a["comando"] = crudo(r, c);
  return true;
}

/* ---- Steam ---- */
std::string raizSteam() {
  if (const char *r = std::getenv("ODYSSEYVK_STEAM_RAIZ")) return r;   /* para las pruebas */
  const char *home = std::getenv("HOME");
  if (!home) return {};
  for (const char *rel : {"/.local/share/Steam", "/.steam/steam", "/.var/app/com.valvesoftware.Steam/.local/share/Steam"}) {
    const std::string b = std::string(home) + rel;
    std::error_code ec;
    if (std::filesystem::is_directory(b + "/userdata", ec) || std::filesystem::is_directory(b + "/steamapps", ec)) return b;
  }
  return {};
}

namespace {
std::string buscarLocalconfig(const std::string &raiz);
/* la fecha y el tamano de un fichero, para ver si cambio ("" si no esta) */
std::string sello(const std::string &ruta) {
  struct stat sb {};
  if (ruta.empty() || ::stat(ruta.c_str(), &sb) != 0) return "-";
  return std::to_string((long long)sb.st_mtim.tv_sec) + "." + std::to_string((long)sb.st_mtim.tv_nsec) + ":" +
         std::to_string((long long)sb.st_size);
}
}  // namespace

std::string rutaLocalconfig() {
  if (const char *r = std::getenv("ODYSSEYVK_STEAM_LOCALCONFIG")) return r;   /* para las pruebas */
  const std::string raiz = raizSteam();
  if (raiz.empty()) return {};
  /* la de la ultima vez mientras loginusers.vdf siga igual (se mira cada dos segundos con el panel abierto) */
  static std::string clave, ruta;
  const std::string c = raiz + "|" + sello(raiz + "/config/loginusers.vdf");
  if (c != clave || !fileExists(ruta)) clave = c, ruta = buscarLocalconfig(raiz);
  return ruta;
}

namespace {
std::string buscarLocalconfig(const std::string &raiz) {
  /* la cuenta de la ultima vez: MostRecent = 1, o la del inicio automatico, o la de la marca mas nueva */
  std::string txt;
  NodoVdf n;
  if (readSmallFile(raiz + "/config/loginusers.vdf", txt) && parsearVdf(txt, n, nullptr)) {
    if (const NodoVdf *us = n.hijo("users")) {
      std::string mejor;
      long long marcaMejor = -1;
      int puntos = -1;
      for (const NodoVdf &u : us->hijos) {
        if (!u.objeto || u.clave.size() < 16 ||
            !std::all_of(u.clave.begin(), u.clave.end(), [](unsigned char ch) { return std::isdigit(ch); }))
          continue;
        auto val = [&](const char *k) {
          const NodoVdf *h = u.hijo(k);
          return h ? h->valor : std::string();
        };
        const int p = val("MostRecent") == "1" ? 2 : (val("AutoLogin") == "1" || val("AllowAutoLogin") == "1" ? 1 : 0);
        const long long m = std::atoll((val("Timestamp").empty() ? val("timestamp") : val("Timestamp")).c_str());
        if (p > puntos || (p == puntos && m > marcaMejor)) mejor = u.clave, puntos = p, marcaMejor = m;
      }
      if (!mejor.empty()) {
        const unsigned long long id64 = std::strtoull(mejor.c_str(), nullptr, 10);
        if (id64 > 76561197960265728ULL) {
          const std::string r = raiz + "/userdata/" + std::to_string(id64 - 76561197960265728ULL) + "/config/localconfig.vdf";
          if (fileExists(r)) return r;
        }
      }
    }
  }
  std::string mejor;
  long long marca = -1;
  std::error_code ec;
  for (const auto &d : std::filesystem::directory_iterator(raiz + "/userdata", ec)) {
    const std::string r = d.path().string() + "/config/localconfig.vdf";
    struct stat sb {};
    if (::stat(r.c_str(), &sb) == 0 && (long long)sb.st_mtime > marca) mejor = r, marca = (long long)sb.st_mtime;
  }
  return mejor;
}

/* el proceso `pid` es Steam de este usuario */
bool esSteam(long pid) {
  if (pid <= 0) return false;
  const std::string base = "/proc/" + std::to_string(pid);
  struct stat sb {};
  if (::stat(base.c_str(), &sb) != 0 || sb.st_uid != ::getuid()) return false;
  std::string comm;
  if (!readSmallFile(base + "/comm", comm)) return false;
  comm = strTrim(comm);
  return comm == "steam" || comm == "steamwebhelper";
}

/* un Steam de este usuario en todo /proc (unos 600 procesos): para cuando no hay steam.pid y antes de escribir */
bool recorrerProc(long *pid) {
  DIR *d = ::opendir("/proc");
  if (!d) return false;
  bool hay = false;
  while (dirent *e = ::readdir(d)) {
    if (!std::isdigit((unsigned char)e->d_name[0])) continue;
    const long p = std::atol(e->d_name);
    if (esSteam(p)) {
      if (pid) *pid = p;
      hay = true;
      break;
    }
  }
  ::closedir(d);
  return hay;
}

/* antes de escribir, la mirada entera: el steam.pid podria mentir */
bool steamAbiertoSeguro() {
  if (std::getenv("ODYSSEYVK_STEAM_ABIERTO")) return steamAbierto();
  return steamAbierto() || recorrerProc(nullptr);
}
}  // namespace

bool steamAbierto() {
  if (const char *f = std::getenv("ODYSSEYVK_STEAM_ABIERTO")) return f[0] == '1';   /* para las pruebas */
  /* el ultimo que se vio, si sigue ahi: una mirada */
  static long visto = 0;
  if (esSteam(visto)) return true;
  visto = 0;
  /* el de ~/.steam/steam.pid, que Steam escribe al arrancar: si ese ya no esta, Steam esta cerrado (es lo que mira
     steam.sh). En Flatpak el numero es el de su caja, asi que alli no vale y se recorre /proc */
  const std::string raiz = raizSteam();
  const char *home = std::getenv("HOME");
  if (home && raiz.find("/.var/app/") == std::string::npos) {
    std::string t;
    if (readSmallFile(std::string(home) + "/.steam/steam.pid", t)) {
      const long pid = std::atol(strTrim(t).c_str());
      if (esSteam(pid)) visto = pid;
      return visto != 0;
    }
  }
  return recorrerProc(&visto);
}

bool leerSteam(std::map<std::string, std::string> &opciones, std::string *err) {
  opciones.clear();
  const std::string ruta = rutaLocalconfig();
  std::string txt;
  if (ruta.empty() || !readSmallFile(ruta, txt)) {
    if (err) *err = "no encontré el localconfig.vdf de Steam";
    return false;
  }
  NodoVdf raiz;
  std::string e;
  if (!parsearVdf(txt, raiz, &e)) {
    if (err) *err = ruta + ": " + e;
    return false;
  }
  const NodoVdf *apps = appsDe(raiz);
  if (!apps) return true;   /* sin opciones de nadie todavia */
  for (const NodoVdf &a : apps->hijos)
    if (a.objeto)
      if (const NodoVdf *lo = a.hijo("LaunchOptions"); lo && !lo->objeto) opciones[a.clave] = lo->valor;
  return true;
}

bool escribirSteam(const std::map<std::string, std::string> &lineas, std::string *respaldo, std::string *err) {
  if (lineas.empty()) return true;
  if (steamAbiertoSeguro()) {
    if (err) *err = "Steam está abierto";
    return false;
  }
  const std::string ruta = rutaLocalconfig();
  std::string txt;
  struct stat sb {};
  if (ruta.empty() || !leerFichero(ruta, txt, &sb)) {
    if (err) *err = "no encontré el localconfig.vdf de Steam";
    return false;
  }
  NodoVdf raiz;
  std::string e;
  if (!parsearVdf(txt, raiz, &e)) {
    if (err) *err = ruta + ": " + e;
    return false;
  }
  const NodoVdf *apps = appsDe(raiz);
  if (!apps) {
    if (err) *err = "localconfig.vdf no tiene la lista de juegos (apps)";
    return false;
  }
  /* los cambios, de atras hacia delante para que las posiciones sigan valiendo */
  struct Cambio {
    size_t pos, largo;
    std::string texto;
  };
  std::vector<Cambio> cambios;
  std::string nuevosJuegos;
  const std::string sangriaApps = sangriaDe(txt, apps->cierra);
  for (const auto &[id, lin] : lineas) {
    const std::string valor = "\"" + escaparVdf(lin) + "\"";
    const NodoVdf *app = nullptr;
    for (const NodoVdf &h : apps->hijos)
      if (h.objeto && h.clave == id) app = &h;
    if (!app) {
      const std::string s = sangriaApps + "\t";
      nuevosJuegos += s + "\"" + id + "\"\n" + s + "{\n" + s + "\t\"LaunchOptions\"\t\t" + valor + "\n" + s + "}\n";
      continue;
    }
    if (const NodoVdf *lo = app->hijo("LaunchOptions"); lo && !lo->objeto) {
      cambios.push_back({lo->valIni, lo->valFin - lo->valIni, valor});
    } else {
      const size_t en = inicioDeLinea(txt, app->cierra);
      cambios.push_back({en, 0, sangriaDe(txt, app->cierra) + "\t\"LaunchOptions\"\t\t" + valor + "\n"});
    }
  }
  if (!nuevosJuegos.empty()) cambios.push_back({inicioDeLinea(txt, apps->cierra), 0, nuevosJuegos});
  std::sort(cambios.begin(), cambios.end(), [](const Cambio &a, const Cambio &b) { return a.pos > b.pos; });
  std::string nuevo = txt;
  for (const Cambio &c : cambios) nuevo.replace(c.pos, c.largo, c.texto);
  /* antes de tocar nada: que lo nuevo se lea y diga justo lo que se quiere */
  {
    NodoVdf r2;
    std::map<std::string, std::string> leidas;
    const NodoVdf *a2 = parsearVdf(nuevo, r2, &e) ? appsDe(r2) : nullptr;
    if (a2)
      for (const NodoVdf &a : a2->hijos)
        if (a.objeto)
          if (const NodoVdf *lo = a.hijo("LaunchOptions"); lo && !lo->objeto) leidas[a.clave] = lo->valor;
    for (const auto &[id, lin] : lineas)
      if (!a2 || leidas[id] != lin) {
        if (err) *err = "lo escrito no se leía bien de vuelta (" + id + "): no se tocó nada";
        return false;
      }
  }
  /* la copia: la primera de todas se queda; de las demas, las ocho ultimas */
  const std::string dir = rutaEstadoOdyssey("steam");
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  const std::string original = dir + "/localconfig-antes-de-odysseyvk.vdf";
  if (!fileExists(original) && !escribirDeGolpe(original, txt, 0600, err)) return false;
  char cuando[32] = {0};
  const time_t ahora = ::time(nullptr);
  std::strftime(cuando, sizeof cuando, "%Y%m%d-%H%M%S", std::localtime(&ahora));
  const std::string copia = dir + "/localconfig-" + cuando + ".vdf";
  if (!escribirDeGolpe(copia, txt, 0600, err)) return false;
  if (respaldo) *respaldo = copia;
  {
    std::vector<std::string> copias;
    for (const auto &d : std::filesystem::directory_iterator(dir, ec)) {
      const std::string n = d.path().filename().string();
      if (n.rfind("localconfig-2", 0) == 0) copias.push_back(d.path().string());
    }
    std::sort(copias.begin(), copias.end());
    for (size_t k = 0; k + 8 < copias.size(); k++) std::filesystem::remove(copias[k], ec);
  }
  /* si Steam (u otro) lo cambio mientras tanto, no se pisa */
  struct stat sb2 {};
  if (::stat(ruta.c_str(), &sb2) != 0 || sb2.st_size != sb.st_size || sb2.st_mtim.tv_sec != sb.st_mtim.tv_sec ||
      sb2.st_mtim.tv_nsec != sb.st_mtim.tv_nsec) {
    if (err) *err = "localconfig.vdf cambió mientras tanto: no se tocó";
    return false;
  }
  if (steamAbiertoSeguro()) {
    if (err) *err = "Steam está abierto";
    return false;
  }
  return escribirDeGolpe(ruta, nuevo, sb.st_mode & 07777, err);
}

const std::map<std::string, std::string> &exesNeuralForge() {
  static std::map<std::string, std::string> exes;
  static bool mirado = false;
  if (mirado) return exes;
  mirado = true;
  std::string cli;
  if (const char *c = std::getenv("ODYSSEYVK_NEURAL_FORGE_CLI")) cli = c;
  else if (const char *home = std::getenv("HOME"); home && fileExists(std::string(home) + "/.local/share/neural-forge/bin/neural-forge-cli"))
    cli = std::string(home) + "/.local/share/neural-forge/bin/neural-forge-cli";
  else if (const char *path = std::getenv("PATH")) {
    std::string p = path;
    size_t a = 0;
    while (a <= p.size() && cli.empty()) {
      size_t b = p.find(':', a);
      if (b == std::string::npos) b = p.size();
      const std::string r = p.substr(a, b - a) + "/neural-forge-cli";
      if (b > a && ::access(r.c_str(), X_OK) == 0) cli = r;
      a = b + 1;
    }
  }
  if (cli.empty() || cli == "-") return exes;
  std::string orden = "timeout 15 '";
  for (char ch : cli) orden += ch == '\'' ? std::string("'\\''") : std::string(1, ch);
  orden += "' games 2>/dev/null";
  std::string sal;
  runCapture(orden, sal);
  /* «   appid  on   name  (library)  target executable», p. ej.
     « 1091500  no   Cyberpunk 2077  (/run/media/.../SteamLibrary)  Cyberpunk2077.exe (chosen by hand)  [tested: ...]» */
  size_t p = 0;
  while (p < sal.size()) {
    size_t q = sal.find('\n', p);
    if (q == std::string::npos) q = sal.size();
    const std::string l = strTrim(sal.substr(p, q - p));
    p = q + 1;
    size_t k = 0;
    while (k < l.size() && std::isdigit((unsigned char)l[k])) k++;
    if (k == 0 || k >= l.size() || !std::isspace((unsigned char)l[k])) continue;
    const std::string id = l.substr(0, k);
    const size_t lib = l.find("  (", k);
    if (lib == std::string::npos) continue;
    const size_t finLib = l.find(")  ", lib);
    if (finLib == std::string::npos) continue;
    std::string exe = l.substr(finLib + 3);
    if (const size_t cor = exe.find("  ["); cor != std::string::npos) exe = exe.substr(0, cor);
    exe = strTrim(exe);
    if (!exe.empty() && exe.back() == ')') {
      const size_t par = exe.rfind(" (");
      if (par != std::string::npos) exe = strTrim(exe.substr(0, par));
    }
    if (!exe.empty() && exe != "-" && exe != "?") exes[id] = exe;
  }
  return exes;
}

std::string exeDe(const Estado &e, const std::string &appid) {
  const auto &nf = exesNeuralForge();
  if (const auto it = nf.find(appid); it != nf.end()) return it->second;
  if (const auto it = e.juegos.find(appid); it != e.juegos.end()) return it->second.nfExe;
  return {};
}

/* ---- poner al dia y escribir ---- */
bool sincronizar(Estado &e, const std::vector<std::string> &appids, const std::map<std::string, std::string> &steam) {
  bool cambio = false;
  for (const std::string &id : appids) {
    const auto s = steam.find(id);
    const std::string cur = s == steam.end() ? std::string() : s->second;
    Juego &j = e.juegos[id];
    if (j.visto && strTrim(cur) == strTrim(j.lineaSteam)) continue;
    cambio = true;
    Leida nueva;
    if (!leerLinea(cur, nueva)) {
      /* no se sabe leer: se deja como esta */
      j.propia = true, j.visto = true, j.lineaSteam = cur;
      continue;
    }
    Leida vieja;
    const bool hayVieja = j.visto && !j.propia && leerLinea(j.lineaSteam, vieja);
    for (const auto &[k, v] : nueva.ajustes) {
      /* solo lo que cambio por fuera: lo demas sigue como estaba (lo de todos, o lo pendiente de escribir) */
      if (hayVieja)
        if (const auto it = vieja.ajustes.find(k); it != vieja.ajustes.end() && it->second == v) continue;
      const auto t = e.todos.find(k);
      if ((t != e.todos.end() ? t->second : deFabrica().at(k)) == v) j.propios.erase(k);
      else j.propios[k] = v;
    }
    if (!nueva.nfExe.empty()) j.nfExe = nueva.nfExe;
    j.propia = false, j.visto = true, j.lineaSteam = cur;
  }
  return cambio;
}

std::map<std::string, std::string> porEscribir(const Estado &e, const std::vector<std::string> &appids,
                                               const std::map<std::string, std::string> &steam) {
  std::map<std::string, std::string> sal;
  for (const std::string &id : appids) {
    if (const auto it = e.juegos.find(id); it != e.juegos.end() && it->second.propia) continue;
    const auto s = steam.find(id);
    const std::string cur = s == steam.end() ? std::string() : s->second;
    const std::string quiero = linea(efectivo(e, id), exeDe(e, id));
    Leida l;
    if (!leerLinea(cur, l)) {
      /* una que no se sabe leer solo se pisa si ya se vio y se pidio llevarla (`comoTodos`, «Usar la de OdysseyVK») */
      const auto it = e.juegos.find(id);
      if (it != e.juegos.end() && it->second.visto && !it->second.propia && strTrim(cur) != quiero) sal[id] = quiero;
      continue;
    }
    /* lo que dice la de ahora, con lo que no decide tomado de lo de todos: si sale la misma linea, no se toca
       (aunque la de Steam este en otro orden o lleve `env` de mas) */
    Ajustes comoEsta = efectivo(e, "");
    for (const auto &[k, v] : l.ajustes) comoEsta[k] = v;
    if (linea(comoEsta, l.nfExe) == quiero) continue;
    sal[id] = quiero;
  }
  return sal;
}

Resultado aplicar(const std::vector<std::string> &appids, bool escribir) {
  Resultado r;
  std::map<std::string, std::string> steam;
  if (!leerSteam(steam, &r.error)) {
    r.ok = false;
    return r;
  }
  Estado e = cargar();
  if (sincronizar(e, appids, steam)) guardar(e, nullptr);
  const std::map<std::string, std::string> pend = porEscribir(e, appids, steam);
  for (const auto &par : pend) r.cuales.push_back(par.first);
  r.pendientes = (int)pend.size();
  r.steamAbierto = escribir ? steamAbiertoSeguro() : steamAbierto();
  if (pend.empty()) {
    quitarMarca();
    return r;
  }
  if (!escribir) return r;
  if (r.steamAbierto) {
    ponerMarca();
    return r;
  }
  if (!escribirSteam(pend, &r.respaldo, &r.error)) {
    r.ok = false;
    ponerMarca();   /* que lo vuelva a intentar el servicio */
    return r;
  }
  for (const auto &[id, lin] : pend) {
    Juego &j = e.juegos[id];
    j.lineaSteam = lin, j.visto = true, j.propia = false;
  }
  guardar(e, nullptr);
  r.escritos = r.pendientes;
  r.pendientes = 0;
  quitarMarca();
  return r;
}

bool vigilar(Vigia &v, const std::vector<std::string> &(*appids)(), Resultado &r) {
  r = Resultado{};
  if (!hayMarca()) {
    v = Vigia{};
    return false;
  }
  if (steamAbierto()) {
    v.sinSteam = 0;
    return false;
  }
  if (++v.sinSteam < 2) return false;
  v.sinSteam = 0;
  r = aplicar(appids(), true);
  if (r.ok) {
    v.fallos = 0;
  } else if (++v.fallos >= 5) {
    quitarMarca();
    v.fallos = 0;
  }
  return true;
}

bool hayMarca() { return fileExists(rutaMarca()); }
void ponerMarca() {
  if (FILE *f = std::fopen(rutaMarca().c_str(), "we")) {
    std::fprintf(f, "%ld\n", (long)::time(nullptr));
    std::fclose(f);
  }
}
void quitarMarca() { ::unlink(rutaMarca().c_str()); }

/* ---- lo instalado ---- */
namespace {
/* donde busca el cargador de Vulkan las capas implicitas (las que se encienden con una variable), y las carpetas
   del PATH: si alguna cambia, se vuelve a mirar */
std::vector<std::string> carpetasCapas() {
  std::vector<std::string> v;
  auto lista = [&](const char *var, const char *defecto, const char *sufijo) {
    const char *e = std::getenv(var);
    std::string t = e && *e ? e : defecto;
    size_t i = 0;
    while (i <= t.size()) {
      const size_t j = std::min(t.find(':', i), t.size());
      if (j > i) v.push_back(t.substr(i, j - i) + sufijo);
      i = j + 1;
    }
  };
  const char *home = std::getenv("HOME");
  const std::string h = home ? home : "";
  const char *xc = std::getenv("XDG_CONFIG_HOME");
  v.push_back((xc && *xc ? std::string(xc) : h + "/.config") + "/vulkan/implicit_layer.d");
  lista("XDG_CONFIG_DIRS", "/etc/xdg", "/vulkan/implicit_layer.d");
  v.push_back("/etc/vulkan/implicit_layer.d");
  const char *xd = std::getenv("XDG_DATA_HOME");
  v.push_back((xd && *xd ? std::string(xd) : h + "/.local/share") + "/vulkan/implicit_layer.d");
  lista("XDG_DATA_DIRS", "/usr/local/share:/usr/share", "/vulkan/implicit_layer.d");
  for (const char *d : {"/usr/local/share/vulkan/implicit_layer.d", "/usr/share/vulkan/implicit_layer.d"})
    if (std::find(v.begin(), v.end(), d) == v.end()) v.push_back(d);
  return v;
}
std::vector<std::string> carpetasPath() {
  std::vector<std::string> v;
  const char *p = std::getenv("PATH");
  const std::string t = p ? p : "";
  size_t i = 0;
  while (i <= t.size()) {
    const size_t j = std::min(t.find(':', i), t.size());
    if (j > i) v.push_back(t.substr(i, j - i));
    i = j + 1;
  }
  /* el servicio puede arrancar con un PATH corto */
  const char *home = std::getenv("HOME");
  for (const std::string d : {std::string(home ? home : "") + "/.local/bin", std::string("/usr/local/bin"), std::string("/usr/bin")})
    if (std::find(v.begin(), v.end(), d) == v.end()) v.push_back(d);
  return v;
}

std::string sellosCarpetas() {
  std::string s;
  for (const std::string &d : carpetasCapas()) s += sello(d) + "|";
  for (const std::string &d : carpetasPath()) s += sello(d) + "|";
  return s;
}

/* las variables con las que se enciende alguna capa implicita: lo de dentro de cada "enable_environment" */
std::vector<std::string> variablesDeCapas() {
  std::vector<std::string> vars;
  for (const std::string &d : carpetasCapas()) {
    std::error_code ec;
    for (const auto &f : std::filesystem::directory_iterator(d, ec)) {
      if (f.path().extension() != ".json") continue;
      std::string t;
      if (!readSmallFile(f.path().string(), t)) continue;
      for (size_t i = t.find("\"enable_environment\""); i != std::string::npos; i = t.find("\"enable_environment\"", i + 1)) {
        const size_t a = t.find('{', i), b = a == std::string::npos ? a : t.find('}', a);
        if (b == std::string::npos) break;
        /* las claves: lo que va entre comillas justo antes de cada ':' */
        size_t k = a;
        while ((k = t.find('"', k + 1)) != std::string::npos && k < b) {
          const size_t fin = t.find('"', k + 1);
          if (fin == std::string::npos || fin > b) break;
          size_t m = fin + 1;
          while (m < b && std::isspace((unsigned char)t[m])) m++;
          if (m < b && t[m] == ':') vars.push_back(t.substr(k + 1, fin - k - 1));
          k = fin;
        }
      }
    }
  }
  return vars;
}

bool hayComando(const std::string &nombre) {
  for (const std::string &d : carpetasPath()) {
    const std::string r = d + "/" + nombre;
    struct stat sb {};
    if (::stat(r.c_str(), &sb) == 0 && S_ISREG(sb.st_mode) && ::access(r.c_str(), X_OK) == 0) return true;
  }
  return false;
}
}  // namespace

bool instalado(const std::string &clave) {
  static std::string sellos;
  static std::map<std::string, bool> hay;
  const std::string ahora = sellosCarpetas();
  if (ahora != sellos || hay.empty()) {
    sellos = ahora;
    hay.clear();
    const std::vector<std::string> vars = variablesDeCapas();
    for (const Variable &v : kVariables) hay[v.clave] = std::find(vars.begin(), vars.end(), v.nombre) != vars.end();
    hay["vkdaedalus_run"] = hayComando("vkdaedalus-run");
    hay["gamescope"] = hay["reescalado"] = hayComando("gamescope");
  }
  const auto it = hay.find(clave);
  return it == hay.end() || it->second;
}

const char *excluyente(const std::string &clave) {
  if (clave == "vkdaedalus") return "vkdaedalus_run";
  if (clave == "vkdaedalus_run") return "vkdaedalus";
  return "";
}

std::string firma() {
  return sello(rutaLocalconfig()) + "|" + sello(rutaConf()) + "|" + sello(rutaMarca()) + "|" + (steamAbierto() ? "A" : "C") +
         "|" + sellosCarpetas();
}

}  // namespace integ
