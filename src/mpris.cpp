#include "mpris.h"

#ifdef HAVE_SDBUS
#include <systemd/sd-bus.h>

#include <cstring>
#endif

#include <cstdio>

namespace {

const char kPlayerIface[] = "org.mpris.MediaPlayer2.Player";

#ifdef HAVE_SDBUS
/* Queda con el primer reproductor que parezca un navegador; si no hay ninguno,
   con el primero que aparezca. Así el control no falla cuando hay otro reproductor abierto. */
bool mejorNombre(const char *actual, const char *candidato) {
  if (!candidato || !*candidato) return false;
  static const char *preferidos[] = {"firefox", "zen", "chrom", "epiphany", "midori", "brave"};
  auto preferido = [](const char *n) {
    for (const char *p : preferidos)
      if (std::strstr(n, p)) return true;
    return false;
  };
  if (actual && *actual) {
    for (const char *p : preferidos)
      if (std::strstr(actual, p)) return false;      /* ya tenemos un navegador */
    return preferido(candidato);
  }
  return true;
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
  int r = sd_bus_open_user(&bus_);
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
  std::string elegido = name_;
  if (sd_bus_message_enter_container(reply, 'a', "s") >= 0) {
    const char *n = nullptr;
    while (sd_bus_message_read(reply, "s", &n) > 0) {
      if (!n || std::strncmp(n, "org.mpris.MediaPlayer2.", 23) != 0) continue;
      if (mejorNombre(elegido.c_str(), n)) elegido = n;
    }
    sd_bus_message_exit_container(reply);
  }
  sd_bus_message_unref(reply);
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
