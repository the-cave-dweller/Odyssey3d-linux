/* Control del vídeo del navegador por MPRIS.
   Zen y Firefox publican un reproductor MPRIS (org.mpris.MediaPlayer2.firefox...), así que
   podemos pausar y reanudar el vídeo sin quitar la capa 3D, y reanudar solo el vídeo que
   el cambio de modo del monitor suele dejar parado. Hablamos por sd-bus, la misma librería
   que ya usan los avisos de escritorio; si no hay libsystemd, la clase se compila pero
   siempre dice que no está disponible.                                                      */
#pragma once

#include <string>

class Mpris {
public:
  Mpris() = default;
  ~Mpris();
  Mpris(const Mpris &) = delete;
  Mpris &operator=(const Mpris &) = delete;

  /* Abre el bus del usuario y busca un reproductor. Devuelve false si no hay bus. */
  bool open(std::string *err = nullptr);
  bool available() const { return bus_ != nullptr; }
  bool playerFound() const { return !name_.empty(); }
  const std::string &playerName() const { return name_; }

  /* App de la ventana del video (su app_id, p. ej. "firefox"): el reproductor que coincida gana.
     Sin pista se elige el que sea un navegador por su Identity y este reproduciendo. */
  void pista(const std::string &appId) { pista_ = appId; }

  /* Vuelve a mirar por si el navegador se abrió despues de empezar la sesion. */
  bool refresh(std::string *err = nullptr);

  /* "Playing" | "Paused" | "Stopped" | "" si no se pudo leer. */
  bool status(std::string *state, std::string *err = nullptr);

  /* Alterna la reproducción. */
  bool playPause(std::string *err = nullptr);

  /* Reanuda si estaba parado o en pausa. Devuelve true si mandó la orden. */
  bool resumeIfNeeded(std::string *err = nullptr);

private:
  struct sd_bus *bus_ = nullptr;
  std::string name_;
  std::string pista_;
};
