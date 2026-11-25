#include "atajos.h"

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
const int32_t kCtrl = 0x04000000, kMayus = 0x02000000, kF1 = 0x01000030;

struct Accion {
  const char *nombre;
  const char *visible;
  int digito;   /* 1..8, o 0 para F1 */
};
const Accion kAcciones[] = {
    {"3d", "3D encendido o apagado", 1},
    {"capa2", "Ocultar o mostrar la pastilla (tecla de siempre)", 2},
    {"prof-", "Bajar el plano de apoyo (3D Profundidad)", 3},
    {"prof+", "Subir el plano de apoyo (3D Profundidad)", 4},
    {"relieve-", "Bajar el relieve (3D Salida)", 5},
    {"relieve+", "Subir el relieve (3D Salida)", 6},
    {"pastilla-", "Achicar la pastilla", 7},
    {"pastilla+", "Agrandar la pastilla", 8},
    {"capa", "Ocultar o mostrar la pastilla", 0},
};

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

/* El simbolo que sale con Mayus en la tecla del numero `d` (1..8), como codigo de Qt; 0 si no se sabe. */
int32_t simboloConMayus(int d, std::string *capaUsada) {
#ifdef HAVE_XKBCOMMON
  static bool hecho = false;
  static int32_t tabla[9] = {0};
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
        for (int k = 1; k <= 8; k++) {
          const xkb_keysym_t *syms = nullptr;
          /* evdev KEY_1 = 2 ... KEY_8 = 9; el codigo de xkb es ese mas 8. Nivel 1 = con Mayus. */
          const int cuantos = xkb_keymap_key_get_syms_by_level(km, (xkb_keycode_t)(k + 1 + 8), 0, 1, &syms);
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
  return (d >= 1 && d <= 8) ? tabla[d] : 0;
#else
  (void)d;
  if (capaUsada) *capaUsada = "sin xkbcommon";
  return 0;
#endif
}

std::vector<int32_t> teclasDe(const Accion &a) {
  std::vector<int32_t> t;
  if (a.digito == 0) {
    t.push_back(kCtrl | kMayus | kF1);
    return t;
  }
  const int32_t sim = simboloConMayus(a.digito, nullptr);
  /* en Wayland KWin quita la Mayus cuando ya se gasto en cambiar el simbolo: lo que ve es Ctrl+«!» */
  if (sim) t.push_back(kCtrl | sim);
  t.push_back(kCtrl | kMayus | ('0' + a.digito));
  if (sim) t.push_back(kCtrl | kMayus | sim);
  return t;
}

std::string nombreTecla(int32_t t) {
  std::string s;
  if (t & kCtrl) s += "Ctrl+";
  if (t & kMayus) s += "Mayus+";
  const int32_t k = t & 0x01ffffff;
  if (k == kF1) s += "F1";
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
  if (sd_bus_open_user(&bus_) < 0) {
    bus_ = nullptr;
    if (err) *err = "no hay bus del usuario";
    return false;
  }
  std::string capa;
  simboloConMayus(1, &capa);
  resumen_.clear();
  int hechas = 0;
  for (const Accion &a : kAcciones) {
    const std::vector<int32_t> teclas = teclasDe(a);
    std::vector<int32_t> quedaron;
    if (llamar(bus_, "doRegister", a, nullptr, 0, nullptr) < 0) continue;
    /* 8 = son las de fabrica; 2 = la accion esta presente (activa). Sin el 4 (NoAutoloading), si
       el usuario las cambio en Ajustes del sistema mandan las suyas. */
    llamar(bus_, "setShortcutKeys", a, &teclas, 8u, nullptr);
    if (llamar(bus_, "setShortcutKeys", a, &teclas, 2u, &quedaron) < 0) continue;
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
  if (sd_bus_open_user(&bus) < 0) return;
  for (const Accion &a : kAcciones) llamar(bus, "setInactive", a, nullptr, 0, nullptr);
  sd_bus_flush_close_unref(bus);
#endif
}
