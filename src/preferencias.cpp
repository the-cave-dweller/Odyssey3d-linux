/* preferencias.cpp — ver preferencias.h. */
#include "preferencias.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <system_error>

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ajuste.h"
#include "util.h"

namespace prefs {
namespace {

bool leeTexto(const std::string &ruta, std::string &sal) {
  FILE *f = std::fopen(ruta.c_str(), "re");
  if (!f) return false;
  char buf[8192];
  size_t n;
  sal.clear();
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) sal.append(buf, n);
  std::fclose(f);
  return true;
}

/* el valor de `"clave"   "valor"` en un fichero de Steam (VDF): la primera vez que sale la clave */
std::string valorVdf(const std::string &txt, const std::string &clave) {
  const std::string aguja = "\"" + clave + "\"";
  size_t p = txt.find(aguja);
  if (p == std::string::npos) return {};
  p = txt.find('"', p + aguja.size());
  if (p == std::string::npos) return {};
  std::string v;
  for (size_t i = p + 1; i < txt.size() && txt[i] != '"'; i++) {
    if (txt[i] == '\\' && i + 1 < txt.size()) i++;
    v += txt[i];
  }
  return v;
}

std::vector<std::string> todosVdf(const std::string &txt, const std::string &clave) {
  std::vector<std::string> sal;
  const std::string aguja = "\"" + clave + "\"";
  size_t p = 0;
  while ((p = txt.find(aguja, p)) != std::string::npos) {
    size_t q = txt.find('"', p + aguja.size());
    if (q == std::string::npos) break;
    std::string v;
    size_t i = q + 1;
    for (; i < txt.size() && txt[i] != '"'; i++) {
      if (txt[i] == '\\' && i + 1 < txt.size()) i++;
      v += txt[i];
    }
    sal.push_back(v);
    p = i;
  }
  return sal;
}

bool esHerramienta(const std::string &nombre) {
  static const char *kPrefijos[] = {"Proton ", "Steam Linux Runtime", "Steamworks Common", "Steam Controller"};
  for (const char *p : kPrefijos)
    if (nombre.rfind(p, 0) == 0) return true;
  return nombre == "Proton" || nombre.empty();
}

bool si(const std::string &v, bool porDefecto) {
  const std::string s = strLower(v);
  if (s.empty()) return porDefecto;
  return !(s == "no" || s == "0" || s == "off" || s == "apagado" || s == "quitado" || s == "false");
}

double numero(const std::string &v, double porDefecto) {
  if (v.empty()) return porDefecto;
  std::string s = v;
  for (char &c : s)
    if (c == ',') c = '.';
  char *fin = nullptr;
  const double d = std::strtod(s.c_str(), &fin);
  return fin == s.c_str() ? porDefecto : d;
}

std::string conComa(double v, int decimales) {
  char buf[48];
  std::snprintf(buf, sizeof buf, "%.*f", decimales, v);
  std::string s = buf;
  for (char &c : s)
    if (c == '.') c = ',';
  return s;
}

const char *nombreModo(const std::string &m) {
  if (m == "conv") return "vídeo normal";
  if (m == "sbs" || m == "suave") return "vídeo dividido";
  if (m == "tb") return "arriba y abajo";
  if (m == "flat") return "sin 3D";
  return m.c_str();
}

/* avisar al visor abierto (si lo hay): una orden suelta por el canal de ajuste */
void avisarVisor(const std::map<std::string, double> &numeros, const std::map<std::string, std::string> &palabras) {
  std::string e;
  ajuste::escribir(numeros, std::string(), palabras, &e);
}

}  // namespace

std::vector<Juego> juegosSteam() {
  std::vector<Juego> sal;
  const char *home = std::getenv("HOME");
  if (!home) return sal;
  std::vector<std::string> raices;
  for (const char *rel : {"/.local/share/Steam", "/.steam/steam", "/.var/app/com.valvesoftware.Steam/.local/share/Steam"}) {
    const std::string base = std::string(home) + rel;
    std::string vdf;
    if (!leeTexto(base + "/steamapps/libraryfolders.vdf", vdf)) continue;
    raices.push_back(base);
    for (const std::string &p : todosVdf(vdf, "path")) raices.push_back(p);
  }
  std::vector<std::string> vistas, ids;
  for (const std::string &r : raices) {
    std::error_code ec;
    const std::filesystem::path real = std::filesystem::weakly_canonical(r, ec);
    const std::string clave = ec ? r : real.string();
    if (std::find(vistas.begin(), vistas.end(), clave) != vistas.end()) continue;
    vistas.push_back(clave);
    for (const auto &e : std::filesystem::directory_iterator(clave + "/steamapps", ec)) {
      const std::string n = e.path().filename().string();
      if (n.rfind("appmanifest_", 0) != 0 || n.size() < 17 || n.substr(n.size() - 4) != ".acf") continue;
      std::string txt;
      if (!leeTexto(e.path().string(), txt)) continue;
      Juego j;
      j.id = valorVdf(txt, "appid");
      j.nombre = valorVdf(txt, "name");
      j.ultimaVez = std::atoll(valorVdf(txt, "LastPlayed").c_str());
      /* StateFlags 4 = instalado del todo; a medio bajar no se ofrece */
      if (j.id.empty() || esHerramienta(j.nombre) || !(std::atoi(valorVdf(txt, "StateFlags").c_str()) & 4)) continue;
      if (std::find(ids.begin(), ids.end(), j.id) != ids.end()) continue;
      ids.push_back(j.id);
      sal.push_back(j);
    }
  }
  std::sort(sal.begin(), sal.end(), [](const Juego &a, const Juego &b) {
    if (a.ultimaVez != b.ultimaVez) return a.ultimaVez > b.ultimaVez;
    return a.nombre < b.nombre;
  });
  return sal;
}

bool lanzarJuego(const std::string &id, std::string *err) {
  for (char c : id)
    if (c < '0' || c > '9') {
      if (err) *err = "ese no es un juego de Steam";
      return false;
    }
  const std::string url = "steam://rungameid/" + id;
  const pid_t pid = ::fork();
  if (pid < 0) {
    if (err) *err = std::string("no se pudo arrancar: ") + std::strerror(errno);
    return false;
  }
  if (pid == 0) {
    /* suelto del todo: sesion propia y sin nuestros descriptores, para que siga vivo al cerrar el panel */
    ::setsid();
    if (::fork() != 0) ::_exit(0);
    const int nulo = ::open("/dev/null", O_RDWR);
    if (nulo >= 0) {
      ::dup2(nulo, 0);
      ::dup2(nulo, 1);
      ::dup2(nulo, 2);
    }
    ::execlp("steam", "steam", url.c_str(), (char *)nullptr);
    ::execlp("xdg-open", "xdg-open", url.c_str(), (char *)nullptr);
    ::_exit(127);
  }
  int st = 0;
  ::waitpid(pid, &st, 0);
  return true;
}

Valores leer() {
  Valores v;
  const std::map<std::string, std::string> c = ajuste::leerServeConf();
  auto de = [&](const char *k) {
    const auto it = c.find(k);
    return it == c.end() ? std::string() : it->second;
  };
  const std::string modo = strLower(de("modo"));
  v.modo = (modo == "conv" || modo == "sbs" || modo == "suave") ? (modo == "suave" ? "sbs" : modo) : "ask";
  v.divididoSuave = si(de("dividido_suavizado"), true);
  v.rastreo = si(de("rastreo"), false);
  v.hdr = si(de("hdr_en_3d"), false);
  v.atajos = si(de("atajos_globales"), true);
  v.fantasma = std::max(0.0, std::min(0.10, numero(de("crosstalk_base"), 0.0128535)));
  v.relieve = std::max(0.5, std::min(2.0, numero(de("relieve"), 1.0)));
  /* las lineas `auto = app:modo, otra:modo` pueden ser varias: se lee el fichero entero */
  std::string txt;
  if (leeTexto(ajuste::rutaServeConf(), txt)) {
    size_t ini = 0;
    while (ini < txt.size()) {
      size_t fin = txt.find('\n', ini);
      if (fin == std::string::npos) fin = txt.size();
      std::string l = strTrim(txt.substr(ini, fin - ini));
      ini = fin + 1;
      const size_t eq = l.find('=');
      if (l.empty() || l[0] == '#' || eq == std::string::npos || strLower(strTrim(l.substr(0, eq))) != "auto") continue;
      std::string par;
      for (char ch : l.substr(eq + 1) + ",") {
        if (ch != ',' && ch != ' ' && ch != '\t') {
          par += ch;
          continue;
        }
        const size_t dd = par.find(':');
        if (dd != std::string::npos && dd > 0) {
          const std::string app = strLower(par.substr(0, dd)), md = strLower(par.substr(dd + 1));
          bool estaba = false;
          for (auto &r : v.recordadas)
            if (r.first == app) r.second = md, estaba = true;
          if (!estaba) v.recordadas.emplace_back(app, md);
        }
        par.clear();
      }
    }
  }
  return v;
}

void llenar(Hud::State &st, const Valores &v, const std::vector<Juego> &juegos, std::vector<std::string> &ids) {
  st.pestanas = {"Ajustes", "Juegos"};
  st.items.clear();
  ids.clear();
  auto pon = [&](Hud::ItemPanel it, const std::string &id) {
    st.items.push_back(std::move(it));
    ids.push_back(id);
  };
  if (st.pestana == 0) {
    Hud::ItemPanel s;
    s.tipo = Hud::ItemPanel::Seccion, s.rotulo = "VÍDEO";
    pon(s, "");
    Hud::ItemPanel m;
    m.tipo = Hud::ItemPanel::Opciones, m.rotulo = "Al ver un vídeo a pantalla completa";
    m.opciones = {"Preguntar", "Vídeo normal", "Vídeo dividido"};
    m.elegido = v.modo == "conv" ? 1 : (v.modo == "sbs" ? 2 : 0);
    pon(m, "modo");
    Hud::ItemPanel d;
    d.tipo = Hud::ItemPanel::Interruptor, d.rotulo = "Suavizar los vídeos divididos";
    d.nota = "Usa una de sus dos imágenes y el relieve del conversor: nada sale doble";
    d.valor = v.divididoSuave;
    pon(d, "dividido_suavizado");
    Hud::ItemPanel r;
    r.tipo = Hud::ItemPanel::Deslizador, r.rotulo = "Fuerza del relieve";
    r.nota = "Para el vídeo normal y el dividido suavizado · desde el siguiente vídeo";
    r.frac = (float)((v.relieve - 0.5) / 1.5), r.valorTexto = conComa(v.relieve, 2);
    pon(r, "relieve");

    s.rotulo = "PANTALLA";
    pon(s, "");
    Hud::ItemPanel f;
    f.tipo = Hud::ItemPanel::Deslizador, f.rotulo = "Reducir el fantasma";
    f.nota = "Cuánta imagen del otro ojo se resta · se nota al momento";
    f.frac = (float)(v.fantasma / 0.10), f.valorTexto = conComa(v.fantasma * 100.0, 1) + " %";
    pon(f, "crosstalk_base");
    Hud::ItemPanel c;
    c.tipo = Hud::ItemPanel::Interruptor, c.rotulo = "Seguir la cabeza con la cámara";
    c.nota = "Desde el siguiente vídeo", c.valor = v.rastreo;
    pon(c, "rastreo");
    Hud::ItemPanel h;
    h.tipo = Hud::ItemPanel::Interruptor, h.rotulo = "Mantener el HDR durante el 3D";
    h.nota = "Desde el siguiente vídeo", h.valor = v.hdr;
    pon(h, "hdr_en_3d");
    Hud::ItemPanel a;
    a.tipo = Hud::ItemPanel::Interruptor, a.rotulo = "Atajos de teclado en todo el escritorio";
    a.nota = "Ctrl+Shift+1…8 y F1 valen aunque el teclado lo tenga el vídeo o el juego", a.valor = v.atajos;
    pon(a, "atajos_globales");

    s.rotulo = "APLICACIONES QUE NO PREGUNTAN";
    pon(s, "");
    if (v.recordadas.empty()) {
      Hud::ItemPanel n;
      n.tipo = Hud::ItemPanel::Nota, n.rotulo = "Ninguna: se pregunta siempre (o se sigue lo de arriba).";
      pon(n, "");
    }
    for (const auto &par : v.recordadas) {
      Hud::ItemPanel fila;
      fila.tipo = Hud::ItemPanel::Fila, fila.rotulo = par.first, fila.valorTexto = nombreModo(par.second);
      fila.quitar = true;
      pon(fila, "auto:" + par.first);
    }
  } else {
    Hud::ItemPanel s;
    s.tipo = Hud::ItemPanel::Seccion, s.rotulo = "STEAM";
    pon(s, "");
    if (juegos.empty()) {
      Hud::ItemPanel n;
      n.tipo = Hud::ItemPanel::Nota, n.rotulo = "No encontré juegos instalados en las bibliotecas de Steam.";
      pon(n, "");
    }
    for (const Juego &j : juegos) {
      Hud::ItemPanel fila;
      fila.tipo = Hud::ItemPanel::Juego, fila.rotulo = j.nombre;
      fila.valorTexto = j.ultimaVez > 0 ? "" : "sin estrenar";
      pon(fila, "juego:" + j.id);
    }
    Hud::ItemPanel n;
    n.tipo = Hud::ItemPanel::Nota;
    n.rotulo = "Al ponerse el juego a pantalla completa, OdysseyVK ofrece el 3D como con un vídeo.";
    pon(n, "");
  }
}

bool aplicar(const std::string &id, int sub, float frac, bool soltado, const std::vector<Juego> &juegos,
             std::string *aviso, bool *cerrar) {
  std::string e;
  auto fija = [&](const std::string &k, const std::string &val) {
    if (ajuste::fijarEnServeConf(k, val, &e)) return true;
    if (aviso) *aviso = "no se pudo guardar: " + e;
    return false;
  };
  const Valores v = leer();
  if (id == "modo") {
    static const char *kModo[] = {"ask", "conv", "sbs"};
    if (sub < 0 || sub > 2 || !fija("modo", kModo[sub])) return false;
    if (aviso) *aviso = sub == 0 ? "se preguntará con la tarjeta" : "desde el siguiente vídeo, sin preguntar";
    return true;
  }
  if (id == "dividido_suavizado") {
    const bool nuevo = !v.divididoSuave;
    if (!fija(id, nuevo ? "si" : "no")) return false;
    avisarVisor({}, {{"dividido_suavizado", nuevo ? "si" : "no"}});
    if (aviso) *aviso = nuevo ? "los vídeos divididos se suavizan" : "los vídeos divididos se ven con su propio relieve";
    return true;
  }
  if (id == "rastreo" || id == "hdr_en_3d" || id == "atajos_globales") {
    const bool ahora = id == "rastreo" ? v.rastreo : (id == "hdr_en_3d" ? v.hdr : v.atajos);
    if (!fija(id, ahora ? "no" : "si")) return false;
    if (aviso) *aviso = "se aplica desde el siguiente vídeo";
    return true;
  }
  if (id == "crosstalk_base") {
    const double val = std::round(std::max(0.0f, std::min(1.0f, frac)) * 0.10 / 0.002) * 0.002;
    avisarVisor({{"crosstalk_base", val}}, {});   /* se ve al momento, tambien mientras se arrastra */
    if (!soltado) return false;
    if (!fija(id, conComa(val, 4))) return false;
    if (aviso) *aviso = "se resta el " + conComa(val * 100.0, 1) + " % de la imagen del otro ojo";
    return true;
  }
  if (id == "relieve") {
    if (!soltado) return false;
    const double val = std::round((0.5 + std::max(0.0f, std::min(1.0f, frac)) * 1.5) / 0.05) * 0.05;
    if (!fija(id, conComa(val, 2))) return false;
    if (aviso) *aviso = "fuerza del relieve " + conComa(val, 2) + " · desde el siguiente vídeo";
    return true;
  }
  if (id.rfind("auto:", 0) == 0) {
    if (!ajuste::fijarAutoEnServeConf(id.substr(5), std::string(), &e)) {
      if (aviso) *aviso = "no se pudo guardar: " + e;
      return false;
    }
    if (aviso) *aviso = "se volverá a preguntar por " + id.substr(5);
    return true;
  }
  if (id.rfind("juego:", 0) == 0) {
    const std::string ap = id.substr(6);
    std::string nombre = ap;
    for (const Juego &j : juegos)
      if (j.id == ap) nombre = j.nombre;
    if (!lanzarJuego(ap, &e)) {
      if (aviso) *aviso = e;
      return false;
    }
    if (aviso) *aviso = "arrancando " + nombre + "…";
    if (cerrar) *cerrar = true;
    return true;
  }
  return false;
}

}  /* namespace prefs */
