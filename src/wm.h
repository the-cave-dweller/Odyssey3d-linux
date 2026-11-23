/* Vigía de ventanas del compositor KDE (org_kde_plasma_window_management).
   Nos da la lista de ventanas con app_id, título, pid y estados (incluido
   `fullscreen`) y el uuid que luego usa la captura de ventana. Wayland no permite
   mirar los píxeles de otra app, pero sí preguntar por el estado de las ventanas
   si el compositor ofrece este protocolo (KWin lo ofrece). */
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct WindowInfo {
  uint32_t id = 0;
  std::string uuid;
  std::string appId;
  std::string title;
  uint32_t states = 0;
  uint32_t pid = 0;
  int x = 0, y = 0;
  uint32_t w = 0, h = 0;
  bool mapped = true;

  enum State {
    Active = 0x1,
    Minimized = 0x2,
    Maximized = 0x4,
    Fullscreen = 0x8,
    KeepAbove = 0x10,
    KeepBelow = 0x20,
    OnAllDesktops = 0x40,
    DemandsAttention = 0x80,
    SkipTaskbar = 0x1000,
    ExcludeFromCapture = 0x200000
  };

  bool fullscreen() const { return (states & Fullscreen) != 0; }
  bool active() const { return (states & Active) != 0; }
  bool minimized() const { return (states & Minimized) != 0; }
  bool usable() const { return mapped && fullscreen() && !minimized(); }
  static const char *stateName(uint32_t s);
  /* Lista legible de los estados activos, para `ctl windows`. */
  static std::string stateList(uint32_t s);
};

class WindowMonitor {
public:
  WindowMonitor();
  ~WindowMonitor();
  WindowMonitor(const WindowMonitor &) = delete;
  WindowMonitor &operator=(const WindowMonitor &) = delete;

  /* Conecta con el compositor y enlaza el gestor de ventanas. Devuelve false si el
     compositor no ofrece el protocolo (todo lo demás del servicio puede seguir usando
     `odyssey3d play`). */
  bool open(std::string *err = nullptr);
  bool available() const;

  /* Descriptor a vigilar con poll() y forma de mover la máquina de eventos. */
  int fd() const;
  int dispatchPending();
  /* Espera un poco a que lleguen eventos (para el modo de una sola pasada). */
  int waitAndDispatch(int timeoutMs);
  void flush() const;
  /* Bombea eventos hasta que el compositor se calle (los detalles de cada ventana
     llegan en rondas sucesivas: sin esto la lista sale vacía). */
  void settle(int quietMs = 150, int maxRounds = 30);

  std::vector<WindowInfo> windows() const;
  bool find(const std::string &uuid, WindowInfo &out) const;
  /* Candidata a pantalla completa que no sea una ventana nuestra. */
  bool fullscreenCandidate(WindowInfo &out, const std::vector<std::string> &ignoreAppIds) const;

  void setOnChange(std::function<void()> fn);

  const std::string &lastError() const { return err_; }

private:
  struct Impl;
  std::unique_ptr<Impl> d;
  std::string err_;
};
