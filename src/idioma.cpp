/* idioma.cpp — la tabla de traducciones y el idioma puesto (v1.07). */
#include "idioma.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace idioma {
namespace {

struct Fila {
  const char *es;
  const char *t[6];   /* en, fr, de, zh, ja, ru */
};

const Fila kTabla[] = {
#include "idioma_tabla.inc"
};

const char *const kCodigos[kLenguas] = {"en", "es", "fr", "de", "zh", "ja", "ru"};
const char *const kNombres[kLenguas] = {"English", "Español", "Français", "Deutsch", "中文", "日本語", "Русский"};

int g_actual = EN;

const std::unordered_map<std::string_view, const Fila *> &indice() {
  static const std::unordered_map<std::string_view, const Fila *> m = [] {
    std::unordered_map<std::string_view, const Fila *> r;
    for (const Fila &f : kTabla) r.emplace(f.es, &f);
    return r;
  }();
  return m;
}

/* columna de la tabla para cada idioma; -1 = el propio texto (español) */
int columna(int l) {
  switch (l) {
    case EN: return 0;
    case FR: return 1;
    case DE: return 2;
    case ZH: return 3;
    case JA: return 4;
    case RU: return 5;
    default: return -1;
  }
}

std::string recorta(const std::string &s) {
  size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
  return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

void anadeSignos(const char *s, std::vector<unsigned> &v) {
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    unsigned cp;
    if (*p < 0x80) { p++; continue; }
    if ((*p & 0xE0) == 0xC0 && p[1]) { cp = ((unsigned)(*p & 0x1F) << 6) | (unsigned)(p[1] & 0x3F); p += 2; }
    else if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) {
      cp = ((unsigned)(*p & 0x0F) << 12) | ((unsigned)(p[1] & 0x3F) << 6) | (unsigned)(p[2] & 0x3F);
      p += 3;
    } else { p++; continue; }
    if (cp >= 256) v.push_back(cp);
  }
}

}  // namespace

const char *codigo(int l) { return l >= 0 && l < kLenguas ? kCodigos[l] : "en"; }
const char *nombre(int l) { return l >= 0 && l < kLenguas ? kNombres[l] : "English"; }

int deCodigo(const std::string &c0) {
  std::string c;
  for (char ch : recorta(c0)) c += (char)std::tolower((unsigned char)ch);
  if (c.size() > 2 && (c[2] == '_' || c[2] == '-' || c[2] == '.')) c.resize(2);   /* es_MX.UTF-8 -> es */
  for (int l = 0; l < kLenguas; l++)
    if (c == kCodigos[l]) return l;
  if (c == "english" || c == "ingles") return EN;
  if (c == "español" || c == "espanol" || c == "spanish") return ES;
  return -1;
}

int actual() { return g_actual; }
void poner(int l) { g_actual = (l >= 0 && l < kLenguas) ? l : EN; }

int leerDeConfig() {
  const char *home = std::getenv("HOME");
  if (!home) return -1;
  std::ifstream f(std::string(home) + "/.config/odysseyvk/serve.conf");
  std::string linea;
  int r = -1;
  while (std::getline(f, linea)) {
    std::string l = recorta(linea);
    if (l.empty() || l[0] == '#') continue;
    size_t eq = l.find('=');
    if (eq == std::string::npos) continue;
    std::string k = recorta(l.substr(0, eq));
    for (char &ch : k) ch = (char)std::tolower((unsigned char)ch);
    if (k == "idioma" || k == "language") r = deCodigo(l.substr(eq + 1));
  }
  return r;
}

int cargar() {
  int l = -1;
  if (const char *e = std::getenv("ODYSSEYVK_IDIOMA"); e && *e) l = deCodigo(e);
  if (l < 0) l = leerDeConfig();
  poner(l < 0 ? EN : l);
  return g_actual;
}

const char *tr(const char *es) {
  const int col = columna(g_actual);
  if (col < 0 || !es) return es;
  auto it = indice().find(std::string_view(es));
  if (it == indice().end()) return es;
  return it->second->t[col];
}

std::string trf(const char *es, std::initializer_list<std::string> datos) {
  const std::string patron = tr(es);
  std::string r;
  r.reserve(patron.size() + 32);
  auto it = datos.begin();
  for (size_t i = 0; i < patron.size(); i++) {
    if (patron[i] == '{' && i + 1 < patron.size() && patron[i + 1] == '}') {
      if (it != datos.end()) r += *it++;
      i++;
      continue;
    }
    r += patron[i];
  }
  return r;
}

std::vector<unsigned> signosDe(int l) {
  std::vector<unsigned> v;
  for (int k = 0; k < kLenguas; k++) anadeSignos(kNombres[k], v);
  const int col = columna(l);
  for (const Fila &f : kTabla) anadeSignos(col < 0 ? f.es : f.t[col], v);
  std::sort(v.begin(), v.end());
  v.erase(std::unique(v.begin(), v.end()), v.end());
  return v;
}

bool esCjk(int l) { return l == ZH || l == JA; }

char decimal() { return (g_actual == ES || g_actual == FR || g_actual == DE || g_actual == RU) ? ',' : '.'; }

}  // namespace idioma
