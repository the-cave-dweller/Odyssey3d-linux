#include "mpris.h"
#include "bususuario.h"

#ifdef HAVE_SDBUS
#include <systemd/sd-bus.h>

#include <cstring>
#include <vector>
#endif

#include <cstdio>

namespace {

const char kPlayerIface[] = "org.mpris.MediaPlayer2.Player";

#ifdef HAVE_SDBUS
/* Una propiedad de texto de un reproductor ("" si no se pudo leer). */
std::string propiedad(sd_bus *bus, const char *nombre, const char *iface, const char *prop) {
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *reply = nullptr;
  std::string out;
  if (sd_bus_call_method(bus, nombre, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get", &error,
                         &reply, "ss", iface, prop) >= 0) {
    const char *v = nullptr;
    if (sd_bus_message_read(reply, "v", "s", &v) >= 0 && v) out = v;
    sd_bus_message_unref(reply);
  }
  sd_bus_error_free(&error);
  return out;
}

std::string minusculas(std::string s) {
  for (char &c : s) {
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (c == '-' || c == '_') c = ' ';
  }
  return s;
}

/* Puntos de un reproductor. Antes se quedaba con el primero cuyo NOMBRE tuviera "chrom", "firefox"...
   y no lo soltaba: las apps Electron (la de Claude, Discord, VS Code) publican
   org.mpris.MediaPlayer2.chromium.instanceN y casi siempre dicen Stopped, asi que con el video en
   Firefox el vigilante leia "Stopped" de la app equivocada y a los 4 s cerraba la sesion 3D (v0.60,
   25-nov 07:01). Ahora se mira la Identity (la de Claude es "Claude", la de Firefox "Mozilla firefox")
   y el estado, y manda la pista de la ventana del video si la hay. */
int puntos(sd_bus *bus, const char *nombre, const std::string &pista) {
  const std::string ident = minusculas(propiedad(bus, nombre, "org.mpris.MediaPlayer2", "Identity"));
  const std::string estado = propiedad(bus, nombre, "org.mpris.MediaPlayer2.Player", "PlaybackStatus");
  const std::string n = minusculas(nombre);
  int p = 0;
  if (!pista.empty() && (ident.find(pista) != std::string::npos || n.find(pista) != std::string::npos)) p += 8;
  static const char *navegadores[] = {"firefox", "zen", "chrome", "chromium", "brave", "epiphany",
                                      "midori",  "vivaldi", "opera", "edge"};
  for (const char *b : navegadores)
    if (ident.find(b) != std::string::npos) { p += 4; break; }
  if (estado == "Playing") p += 2;
  else if (estado == "Paused") p += 1;
  return p;
}
#endif

}  // namespace

Mpris::~Mpris() {
#ifdef HAVE_SDBUS
  if (bus_) sd_bus_flush_close_unref(bus_);
#endif
}

bool Mpris::open(std::string *err) {
#ifdef HAVE_SDBUS
  if (bus_) return refresh(err);
  int r = abrirBusUsuario(&bus_);
  if (r < 0) {
    if (err) *err = "no hay bus del usuario";
    bus_ = nullptr;
    return false;
  }
  return refresh(err);
#else
  (void)err;
  return false;
#endif
}

bool Mpris::refresh(std::string *err) {
#ifdef HAVE_SDBUS
  if (!bus_) {
    if (err) *err = "sin conexion al bus";
    return false;
  }
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *reply = nullptr;
  int r = sd_bus_call_method(bus_, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                             "ListNames", &error, &reply, nullptr);
  if (r < 0) {
    if (err) *err = error.message ? error.message : "no se pudo listar los nombres del bus";
    sd_bus_error_free(&error);
    return false;
  }
  sd_bus_error_free(&error);
  std::vector<std::string> nombres;
  if (sd_bus_message_enter_container(reply, 'a', "s") >= 0) {
    const char *n = nullptr;
    while (sd_bus_message_read(reply, "s", &n) > 0)
      if (n && std::strncmp(n, "org.mpris.MediaPlayer2.", 23) == 0) nombres.push_back(n);
    sd_bus_message_exit_container(reply);
  }
  sd_bus_message_unref(reply);
  const std::string pista = minusculas(pista_);
  std::string elegido;
  int mejor = -1;
  for (const std::string &n : nombres) {
    /* a igualdad de puntos se queda el que ya teniamos, para no saltar de uno a otro */
    const int p = puntos(bus_, n.c_str(), pista) * 2 + (n == name_ ? 1 : 0);
    if (p > mejor) {
      mejor = p;
      elegido = n;
    }
  }
  name_ = elegido;
  if (name_.empty() && err) *err = "no hay ningun reproductor MPRIS en sesion";
  return !name_.empty();
#else
  (void)err;
  return false;
#endif
}

bool Mpris::status(std::string *state, std::string *err) {
#ifdef HAVE_SDBUS
  if (state) state->clear();
  if (!bus_ || name_.empty()) {
    if (err) *err = "no hay reproductor MPRIS";
    return false;
  }
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *reply = nullptr;
  int r = sd_bus_call_method(bus_, name_.c_str(), "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties",
                             "Get", &error, &reply, "ss", kPlayerIface, "PlaybackStatus");
  if (r < 0) {
    if (err) *err = error.message ? error.message : "no se pudo leer el estado del reproductor";
    sd_bus_error_free(&error);
    return false;
  }
  sd_bus_error_free(&error);
  const char *v = nullptr;
  if (sd_bus_message_read(reply, "v", "s", &v) >= 0 && v && state) *state = v;
  sd_bus_message_unref(reply);
  return !state || !state->empty();
#else
  (void)state;
  (void)err;
  return false;
#endif
}

bool Mpris::playPause(std::string *err) {
#ifdef HAVE_SDBUS
  if (!bus_ || name_.empty()) {
    if (err) *err = "no hay reproductor MPRIS";
    return false;
  }
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *reply = nullptr;
  int r = sd_bus_call_method(bus_, name_.c_str(), "/org/mpris/MediaPlayer2", kPlayerIface, "PlayPause", &error, &reply,
                             nullptr);
  sd_bus_message_unref(reply);
  if (r >= 0) {
    sd_bus_error_free(&error);
    return true;
  }
  sd_bus_error_free(&error);
  if (err) *err = error.message ? error.message : "el reproductor no acepta pausa/reproduccion";
  return false;
#else
  if (err) *err = "compilado sin soporte MPRIS";
  return false;
#endif
}

bool Mpris::resumeIfNeeded(std::string *err) {
  std::string st;
  if (!status(&st, err)) return false;
  if (st == "Playing") return false;             /* ya estaba en marcha */
#ifdef HAVE_SDBUS
  /* Algunos reproductores no implementan "Play"; si falla, lo intentamos con PlayPause. */
  sd_bus_error error = SD_BUS_ERROR_NULL;
  sd_bus_message *reply = nullptr;
  int r = sd_bus_call_method(bus_, name_.c_str(), "/org/mpris/MediaPlayer2", kPlayerIface, "Play", &error, &reply,
                             nullptr);
  sd_bus_message_unref(reply);
  sd_bus_error_free(&error);
  if (r >= 0) return true;
#endif
  return playPause(err);
}
