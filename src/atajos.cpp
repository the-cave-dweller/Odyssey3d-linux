#include "atajos.h"
#include "bususuario.h"
#include "ajuste.h"
#include "idioma.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

#ifdef HAVE_SDBUS
#include <systemd/sd-bus.h>
#endif
#ifdef HAVE_XKBCOMMON
#include <xkbcommon/xkbcommon.h>
#endif

namespace {

const char *kServicio = "org.kde.kglobalaccel";
const char *kRuta = "/kglobalaccel";
const char *kIface = "org.kde.KGlobalAccel";
const char *kComponente = "odysseyvk";
const char *kComponenteVisible = "OdysseyVK";

/* codigos de tecla de Qt: lo que entiende kglobalaccel */
const int32_t kCtrl = 0x04000000, kMayus = 0x02000000, kAlt = 0x08000000, kMeta = 0x10000000, kF1 = 0x01000030;

struct Accion {
  const char *nombre;
  const char *visible;
};
/* el orden es el de las teclas de siempre (1..8 y F1): el visor y `ctl atajos` lo usan asi */
const Accion kAcciones[] = {
    {"3d", "3D encendido o apagado"},
    {"capa2", "Ocultar o mostrar la pastilla (tecla de siempre)"},
    {"prof-", "Bajar el plano de apoyo (3D Profundidad)"},
    {"prof+", "Subir el plano de apoyo (3D Profundidad)"},
    {"relieve-", "Bajar el relieve (3D Salida)"},
    {"relieve+", "Subir el relieve (3D Salida)"},
    {"pastilla-", "Achicar la pastilla"},
    {"pastilla+", "Agrandar la pastilla"},
    {"capa", "Ocultar o mostrar la pastilla"},
};

/* las que se pueden cambiar en el panel (v1.09). `capa2` (Ctrl+Shift+2) no: es la vieja de ocultar
   la pastilla y se queda fija, salvo que otra accion tome esa combinacion. */
const std::vector<atajos::Def> kDefs = {
    {"3d", "3d", "3D encendido o apagado", "Ctrl+Shift+1"},
    {"prof-", "profundidad_menos", "Menos profundidad", "Ctrl+Shift+3"},
    {"prof+", "profundidad_mas", "Más profundidad", "Ctrl+Shift+4"},
    {"relieve-", "salida_menos", "Menos salida", "Ctrl+Shift+5"},
    {"relieve+", "salida_mas", "Más salida", "Ctrl+Shift+6"},
    {"pastilla-", "pastilla_menos", "Pastilla más chica", "Ctrl+Shift+7"},
    {"pastilla+", "pastilla_mas", "Pastilla más grande", "Ctrl+Shift+8"},
    {"capa", "pastilla", "Ocultar o mostrar la pastilla", "Ctrl+Shift+F1"},
};
const char *kCapa2 = "Ctrl+Shift+2";

struct Nombrada {
  const char *nombre;
  int32_t qt;
};
const Nombrada kEspeciales[] = {
    {"Left", 0x01000012}, {"Up", 0x01000013}, {"Right", 0x01000014}, {"Down", 0x01000015},
    {"PgUp", 0x01000016}, {"PgDown", 0x01000017}, {"Home", 0x01000010}, {"End", 0x01000011},
    {"Insert", 0x01000006}, {"Delete", 0x01000007}, {"Space", 0x20}, {"Plus", '+'},
};

std::string minusculas(std::string s) {
  for (char &c : s) c = (char)std::tolower((unsigned char)c);
  return s;
}

/* la tecla sola, escrita como se guarda; vacia si no vale */
std::string nombreDeTecla(const std::string &t0) {
  const std::string t = minusculas(t0);
  if (t.size() == 1) {
    const char c = t[0];
    if (c >= 'a' && c <= 'z') return std::string(1, (char)(c - 32));
    if (c == '+') return "Plus";
    if (c >= 0x21 && c <= 0x7e) return t;
    return "";
  }
  if (t.size() >= 2 && t[0] == 'f' && std::isdigit((unsigned char)t[1])) {
    const int n = std::atoi(t.c_str() + 1);
    if (n >= 1 && n <= 12) return "F" + std::to_string(n);
  }
  if (t == "pageup" || t == "prior" || t == "repag") return "PgUp";
  if (t == "pagedown" || t == "next" || t == "avpag") return "PgDown";
  if (t == "del" || t == "supr") return "Delete";
  if (t == "ins") return "Insert";
  if (t == "espacio") return "Space";
  for (const Nombrada &n : kEspeciales)
    if (t == minusculas(n.nombre)) return n.nombre;
  return "";
}

/* "Ctrl+Shift+K" -> modificadores de Qt y la tecla */
bool partir(const std::string &combo, int32_t *mods, std::string *tecla) {
  *mods = 0;
  tecla->clear();
  std::string trozo;
  std::vector<std::string> trozos;
  for (size_t i = 0; i <= combo.size(); i++) {
    if (i == combo.size() || combo[i] == '+') {
      /* «Ctrl++»: el ultimo trozo vacio tras un + es la tecla + */
      if (i < combo.size() && trozo.empty() && i + 1 == combo.size()) { trozos.push_back("+"); i++; break; }
      trozos.push_back(trozo);
      trozo.clear();
    } else if (combo[i] != ' ') {
      trozo += combo[i];
    }
  }
  if (trozos.empty()) return false;
  for (size_t i = 0; i + 1 < trozos.size(); i++) {
    const std::string m = minusculas(trozos[i]);
    if (m == "ctrl" || m == "control" || m == "strg") *mods |= kCtrl;
    else if (m == "shift" || m == "mayus" || m == "mayús" || m == "umschalt" || m == "maj") *mods |= kMayus;
    else if (m == "alt") *mods |= kAlt;
    else if (m == "meta" || m == "super" || m == "win") *mods |= kMeta;
    else return false;
  }
  *tecla = nombreDeTecla(trozos.back());
  return !tecla->empty();
}

int32_t qtDeTecla(const std::string &t) {
  if (t.size() == 1) return (int32_t)(unsigned char)t[0];
  if (t[0] == 'F' && t.size() >= 2 && std::isdigit((unsigned char)t[1])) return kF1 + std::atoi(t.c_str() + 1) - 1;
  for (const Nombrada &n : kEspeciales)
    if (t == n.nombre) return n.qt;
  return 0;
}

#ifdef HAVE_SDBUS

std::string recorta(std::string s) {
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
  size_t i = 0;
  while (i < s.size() && s[i] == ' ') i++;
  return s.substr(i);
}

/* La distribucion del teclado que usa KWin: la de ~/.config/kxkbrc si el usuario eligio una, y si no
   la del sistema (org.freedesktop.locale1, lo mismo que dice `localectl`). */
void distribucion(std::string &modelo, std::string &capa, std::string &variante) {
  const char *home = std::getenv("HOME");
  if (home) {
    std::ifstream f(std::string(home) + "/.config/kxkbrc");
    std::string linea, lista, vars, mod;
    bool enLayout = false, usar = false;
    while (std::getline(f, linea)) {
      linea = recorta(linea);
      if (!linea.empty() && linea[0] == '[') { enLayout = linea == "[Layout]"; continue; }
      if (!enLayout) continue;
      const size_t eq = linea.find('=');
      if (eq == std::string::npos) continue;
      const std::string k = linea.substr(0, eq), v = linea.substr(eq + 1);
      if (k == "LayoutList") lista = v;
      else if (k == "VariantList") vars = v;
      else if (k == "Model") mod = v;
      else if (k == "Use") usar = v == "true";
    }
    if (usar && !lista.empty()) {
      capa = lista.substr(0, lista.find(','));
      variante = vars.substr(0, vars.find(','));
      modelo = mod;
      return;
    }
  }
  sd_bus *sis = nullptr;
  if (sd_bus_open_system(&sis) >= 0) {
    auto prop = [&](const char *nombre) {
      char *v = nullptr;
      std::string out;
      if (sd_bus_get_property_string(sis, "org.freedesktop.locale1", "/org/freedesktop/locale1",
                                     "org.freedesktop.locale1", nombre, nullptr, &v) >= 0 && v) {
        out = v;
        std::free(v);
      }
      return out;
    };
    capa = prop("X11Layout");
    variante = prop("X11Variant");
    modelo = prop("X11Model");
    capa = capa.substr(0, capa.find(','));
    variante = variante.substr(0, variante.find(','));
    sd_bus_flush_close_unref(sis);
  }
}

/* El simbolo que sale con Mayus en la tecla del numero `d` (0..9), como codigo de Qt; 0 si no se sabe. */
int32_t simboloConMayus(int d, std::string *capaUsada) {
#ifdef HAVE_XKBCOMMON
  static bool hecho = false;
  static int32_t tabla[10] = {0};
  static std::string nombreCapa;
  if (!hecho) {
    hecho = true;
    std::string modelo, capa, variante;
    distribucion(modelo, capa, variante);
    nombreCapa = capa.empty() ? "la del sistema" : capa + (variante.empty() ? "" : " (" + variante + ")");
    xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (ctx) {
      xkb_rule_names n{};
      n.model = modelo.empty() ? nullptr : modelo.c_str();
      n.layout = capa.empty() ? nullptr : capa.c_str();
      n.variant = variante.empty() ? nullptr : variante.c_str();
      xkb_keymap *km = xkb_keymap_new_from_names(ctx, &n, XKB_KEYMAP_COMPILE_NO_FLAGS);
      if (km) {
        for (int k = 0; k <= 9; k++) {
          const xkb_keysym_t *syms = nullptr;
          /* evdev KEY_1 = 2 ... KEY_9 = 10, KEY_0 = 11; el codigo de xkb es ese mas 8. Nivel 1 = con Mayus. */
          const int evdev = k == 0 ? 11 : k + 1;
          const int cuantos = xkb_keymap_key_get_syms_by_level(km, (xkb_keycode_t)(evdev + 8), 0, 1, &syms);
          if (cuantos >= 1 && syms[0] >= 0x21 && syms[0] <= 0xff) {
            int32_t q = (int32_t)syms[0];
            if (q >= 'a' && q <= 'z') q -= 32;
            tabla[k] = q;
          }
        }
        xkb_keymap_unref(km);
      }
      xkb_context_unref(ctx);
    }
  }
  if (capaUsada) *capaUsada = nombreCapa;
  return (d >= 0 && d <= 9) ? tabla[d] : 0;
#else
  (void)d;
  if (capaUsada) *capaUsada = "sin xkbcommon";
  return 0;
#endif
}

/* Las secuencias que se registran para una combinacion. Con Mayus y un numero hay que poner tambien
   el simbolo que da esa tecla con Mayus: en Wayland KWin quita la Mayus cuando ya se gasto en
   cambiar el simbolo, y lo que ve es Ctrl+«!». */
std::vector<int32_t> teclasDe(const std::string &combo) {
  std::vector<int32_t> t;
  int32_t mods = 0;
  std::string tecla;
  if (!partir(combo, &mods, &tecla)) return t;
  const int32_t q = qtDeTecla(tecla);
  if (!q) return t;
  const bool digito = tecla.size() == 1 && tecla[0] >= '0' && tecla[0] <= '9';
  const int32_t sim = (digito && (mods & kMayus)) ? simboloConMayus(tecla[0] - '0', nullptr) : 0;
  if (sim) t.push_back((mods & ~kMayus) | sim);
  t.push_back(mods | q);
  if (sim) t.push_back(mods | sim);
  return t;
}

/* la combinacion que lleva cada accion de kglobalaccel ahora mismo */
std::string comboDeAccion(const std::map<std::string, std::string> &c, const Accion &a) {
  if (std::string(a.nombre) == "capa2") {
    for (const auto &par : c)
      if (par.second == kCapa2) return "";   /* otra accion se quedo con Ctrl+Shift+2 */
    return kCapa2;
  }
  const auto it = c.find(a.nombre);
  return it == c.end() ? std::string() : it->second;
}

std::string nombreTecla(int32_t t) {
  std::string s;
  if (t & kCtrl) s += "Ctrl+";
  if (t & kAlt) s += "Alt+";
  if (t & kMayus) s += "Mayus+";
  if (t & kMeta) s += "Meta+";
  const int32_t k = t & 0x01ffffff;
  if (k >= kF1 && k < kF1 + 12) s += "F" + std::to_string(k - kF1 + 1);
  else if (k >= 0x21 && k < 0x7f) s += (char)k;
  else if (k == 0xb7) s += "·";
  else { char b[16]; std::snprintf(b, sizeof b, "0x%x", (unsigned)k); s += b; }
  return s;
}

int llamar(sd_bus *bus, const char *metodo, const Accion &a, const std::vector<int32_t> *teclas, uint32_t banderas,
           std::vector<int32_t> *quedaron) {
  sd_bus_message *m = nullptr, *resp = nullptr;
  sd_bus_error e = SD_BUS_ERROR_NULL;
  int r = sd_bus_message_new_method_call(bus, &m, kServicio, kRuta, kIface, metodo);
  if (r < 0) return r;
  const char *id[] = {kComponente, a.nombre, kComponenteVisible, a.visible, nullptr};
  r = sd_bus_message_append_strv(m, (char **)id);
  if (r >= 0 && teclas) {
    /* a(ai): cada secuencia es una estructura con CUATRO enteros (las que sobran, a cero) */
    r = sd_bus_message_open_container(m, 'a', "(ai)");
    for (size_t i = 0; r >= 0 && i < teclas->size(); i++) {
      const int32_t cuatro[4] = {(*teclas)[i], 0, 0, 0};
      r = sd_bus_message_open_container(m, 'r', "ai");
      if (r >= 0) r = sd_bus_message_append_array(m, 'i', cuatro, sizeof cuatro);
      if (r >= 0) r = sd_bus_message_close_container(m);
    }
    if (r >= 0) r = sd_bus_message_close_container(m);
    if (r >= 0) r = sd_bus_message_append(m, "u", banderas);
  }
  if (r >= 0) r = sd_bus_call(bus, m, 2000000, &e, &resp);
  if (r >= 0 && resp && quedaron) {
    quedaron->clear();
    if (sd_bus_message_enter_container(resp, 'a', "(ai)") >= 0) {
      while (sd_bus_message_enter_container(resp, 'r', "ai") > 0) {
        const void *datos = nullptr;
        size_t bytes = 0;
        if (sd_bus_message_read_array(resp, 'i', &datos, &bytes) >= 0 && bytes >= sizeof(int32_t)) {
          const int32_t primera = *(const int32_t *)datos;
          if (primera) quedaron->push_back(primera);
        }
        sd_bus_message_exit_container(resp);
      }
      sd_bus_message_exit_container(resp);
    }
  }
  sd_bus_error_free(&e);
  sd_bus_message_unref(m);
  sd_bus_message_unref(resp);
  return r;
}

int alPulsar(sd_bus_message *m, void *dato, sd_bus_error *) {
  const char *comp = nullptr, *nombre = nullptr;
  int64_t reloj = 0;
  if (sd_bus_message_read(m, "ssx", &comp, &nombre, &reloj) >= 0 && comp && nombre &&
      std::strcmp(comp, kComponente) == 0)
    ((AtajosGlobales *)dato)->anotar(nombre);
  return 0;
}

#endif   // HAVE_SDBUS

}  // namespace

const std::vector<std::string> &AtajosGlobales::nombres() {
  static const std::vector<std::string> n = [] {
    std::vector<std::string> v;
    for (const Accion &a : kAcciones) v.push_back(a.nombre);
    return v;
  }();
  return n;
}

void AtajosGlobales::anotar(const char *nombre) { cola_.push_back(nombre); }

AtajosGlobales::~AtajosGlobales() { cerrar(); }

bool AtajosGlobales::abrir(std::string *err) {
#ifdef HAVE_SDBUS
  if (bus_) return true;
  if (abrirBusUsuario(&bus_) < 0) {
    bus_ = nullptr;
    if (err) *err = "no hay bus del usuario";
    return false;
  }
  std::string capa;
  simboloConMayus(1, &capa);
  resumen_.clear();
  int hechas = 0;
  const std::map<std::string, std::string> puestas = atajos::combos();
  std::map<std::string, std::string> fabrica;
  for (const atajos::Def &d : kDefs) fabrica[d.nombre] = d.defecto;
  for (const Accion &a : kAcciones) {
    const std::vector<int32_t> teclas = teclasDe(comboDeAccion(puestas, a));
    const std::vector<int32_t> deFabrica = teclasDe(comboDeAccion(fabrica, a));
    std::vector<int32_t> quedaron;
    if (llamar(bus_, "doRegister", a, nullptr, 0, nullptr) < 0) continue;
    /* 8 = son las de fabrica. Despues las puestas, con 2 (la accion esta presente) y 4
       (NoAutoloading): desde v1.09 los atajos se eligen en el panel de OdysseyVK y mandan los de
       serve.conf, no los que KDE recuerde de antes. */
    llamar(bus_, "setShortcutKeys", a, &deFabrica, 8u, nullptr);
    if (llamar(bus_, "setShortcutKeys", a, &teclas, 2u | 4u, &quedaron) < 0) continue;
    hechas++;
    if (!resumen_.empty()) resumen_ += " · ";
    resumen_ += std::string(a.nombre) + " = ";
    if (quedaron.empty()) resumen_ += "(sin tecla: la tiene otro programa)";
    for (size_t i = 0; i < quedaron.size(); i++) resumen_ += (i ? ", " : "") + nombreTecla(quedaron[i]);
  }
  if (!hechas) {
    if (err) *err = "el escritorio no ofrece org.kde.kglobalaccel (no es KDE, o no contesto)";
    sd_bus_flush_close_unref(bus_);
    bus_ = nullptr;
    return false;
  }
  resumen_ += " · teclado " + capa;
  /* el componente y sus senales: pulsada, y repetida mientras se deja apretada */
  sd_bus_message *resp = nullptr;
  sd_bus_error e = SD_BUS_ERROR_NULL;
  const char *ruta = nullptr;
  if (sd_bus_call_method(bus_, kServicio, kRuta, kIface, "getComponent", &e, &resp, "s", kComponente) >= 0 &&
      sd_bus_message_read(resp, "o", &ruta) >= 0 && ruta) {
    const int a1 = sd_bus_match_signal(bus_, nullptr, kServicio, ruta, "org.kde.kglobalaccel.Component",
                                       "globalShortcutPressed", alPulsar, this);
    const int a2 = sd_bus_match_signal(bus_, nullptr, kServicio, ruta, "org.kde.kglobalaccel.Component",
                                       "globalShortcutRepeated", alPulsar, this);
    escucha_ = a1 >= 0 && a2 >= 0;
    ruta_ = ruta;
  }
  sd_bus_error_free(&e);
  sd_bus_message_unref(resp);
  if (!escucha_) {
    if (err) *err = "se registraron los atajos pero no se pudo escuchar su senal";
    cerrar();
    return false;
  }
  return true;
#else
  if (err) *err = "compilado sin libsystemd (sd-bus)";
  return false;
#endif
}

std::vector<std::string> AtajosGlobales::pulsados() {
  std::vector<std::string> out;
#ifdef HAVE_SDBUS
  if (!bus_) return out;
  for (int i = 0; i < 64; i++)
    if (sd_bus_process(bus_, nullptr) <= 0) break;
  out.swap(cola_);
#endif
  return out;
}

bool AtajosGlobales::invocar(const std::string &nombre) {
#ifdef HAVE_SDBUS
  if (!bus_ || ruta_.empty()) return false;
  sd_bus_error e = SD_BUS_ERROR_NULL;
  const int r = sd_bus_call_method(bus_, kServicio, ruta_.c_str(), "org.kde.kglobalaccel.Component",
                                   "invokeShortcut", &e, nullptr, "s", nombre.c_str());
  sd_bus_error_free(&e);
  return r >= 0;
#else
  (void)nombre;
  return false;
#endif
}

void AtajosGlobales::cerrar() {
#ifdef HAVE_SDBUS
  if (!bus_) return;
  for (const Accion &a : kAcciones) llamar(bus_, "setInactive", a, nullptr, 0, nullptr);
  sd_bus_flush_close_unref(bus_);
  bus_ = nullptr;
  escucha_ = false;
#endif
}

void AtajosGlobales::desactivar() {
#ifdef HAVE_SDBUS
  sd_bus *bus = nullptr;
  if (abrirBusUsuario(&bus) < 0) return;
  for (const Accion &a : kAcciones) llamar(bus, "setInactive", a, nullptr, 0, nullptr);
  sd_bus_flush_close_unref(bus);
#endif
}

/* ---------------- los atajos personalizables (v1.09) ---------------- */

namespace atajos {

const std::vector<Def> &lista() { return kDefs; }

std::string normaliza(const std::string &combo) {
  int32_t mods = 0;
  std::string tecla;
  if (!partir(combo, &mods, &tecla)) return "";
  std::string s;
  if (mods & kCtrl) s += "Ctrl+";
  if (mods & kAlt) s += "Alt+";
  if (mods & kMayus) s += "Shift+";
  if (mods & kMeta) s += "Meta+";
  return s + tecla;
}

bool tieneModificador(const std::string &combo) {
  int32_t mods = 0;
  std::string tecla;
  return partir(combo, &mods, &tecla) && (mods & (kCtrl | kAlt | kMeta));
}

std::map<std::string, std::string> combos() {
  std::map<std::string, std::string> c;
  const std::map<std::string, std::string> conf = ajuste::leerServeConf();
  for (const Def &d : kDefs) {
    std::string v;
    const auto it = conf.find(std::string("atajo_") + d.clave);
    if (it != conf.end()) v = normaliza(it->second);
    c[d.nombre] = v.empty() ? d.defecto : v;
  }
  return c;
}

std::string renglonPastilla(bool legado, bool rendimiento, bool foco) {
  const std::map<std::string, std::string> c = combos();
  bool todasDeFabrica = true;
  for (const Def &d : kDefs)
    if (c.at(d.nombre) != d.defecto) todasDeFabrica = false;
  if (todasDeFabrica) return "";
  auto de = [&](const char *n) { return c.at(n); };
  /* un par con los mismos modificadores se escribe junto: «Ctrl+Shift+3/4» */
  auto par = [&](const char *a, const char *b) {
    const std::string x = de(a), y = de(b);
    const size_t px = x.rfind('+'), py = y.rfind('+');
    if (px != std::string::npos && py != std::string::npos && x.compare(0, px + 1, y, 0, py + 1) == 0)
      return x + "/" + y.substr(py + 1);
    return x + " / " + y;
  };
  if (rendimiento) return de("3d") + " 3D · " + de("capa") + " " + idioma::tr("oculta") + " · Esc " + idioma::tr("sale");
  /* con el Foco automatico puesto la Profundidad no esta (rama experimental) */
  if (foco)
    return de("3d") + " 3D · " + par("relieve-", "relieve+") + " " + idioma::tr("salida") + " · " + de("capa") + " " +
           idioma::tr("oculta") + " · Esc " + idioma::tr("sale");
  return de("3d") + " 3D · " + par("prof-", "prof+") + " " + idioma::tr(legado ? "plano" : "profundidad") + " · " +
         par("relieve-", "relieve+") + " " + idioma::tr("salida") + " · " + de("capa") + " " + idioma::tr("oculta") +
         " · Esc " + idioma::tr("sale");
}

}  // namespace atajos
