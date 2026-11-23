/* Captura de una ventana concreta usando el protocolo de KWin
   (zkde_screencast_unstable-v1) y PipeWire. Wayland no deja mirar los píxeles de otra
   aplicación: sólo el compositor puede darlos, y KWin lo hace a través de este
   protocolo (especifico de KDE, inestable: puede cambiar con cada versión de Plasma). */
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/* Metadatos del último fotograma. Cuando el compositor lo entrega en dmabuf hay que
   importarlo en la GPU (eglCreateImageKHR); mapearlo con la CPU no funciona con NVIDIA. */
struct CapturedFrame {
  bool valid = false;
  bool dmabuf = false;
  int w = 0, h = 0;
  int stride = 0;         /* bytes por línea */
  int fd = -1;            /* válido mientras siga viva la captura */
  int offset = 0;
  uint64_t modifier = 0;  /* modificador DRM que usa el compositor */
  uint32_t fourcc = 0;    /* formato DRM (fourcc) */
};

class WindowCapture {
public:
  WindowCapture();
  ~WindowCapture();
  WindowCapture(const WindowCapture &) = delete;
  WindowCapture &operator=(const WindowCapture &) = delete;

  /* Clase del flujo en PipeWire; si el compositor no conecta, se puede probar otra. */
  void setStreamClass(const std::string &pwClass);

  /* Pide al compositor un flujo de la ventana con ese uuid (uuid de `ctl windows`). */
  bool start(const std::string &uuid, std::string *err = nullptr);
  void stop();
  bool running() const;

  /* El flujo vive en el socket de Wayland: hay que bombearlo con el bucle del servicio. */
  int fd() const;
  void pump();

  bool streamCreated() const;   /* el compositor ya creó el nodo de PipeWire */
  bool framesFlowing() const;   /* llegó al menos un fotograma */
  uint64_t frameCount() const;  /* cuántos fotogramas han llegado */
  bool dmabufOnly() const;      /* el flujo viene en dmabuf (aún no sabemos usarlo) */

  /* Estado de nuestro lado del flujo de PipeWire (error, paused, streaming...). */
  std::string stateText() const;

  /* Último fotograma en memoria, como bytes BGRA (o lo que dé el compositor). */
  bool copyFrame(std::vector<uint8_t> &out, int &w, int &h, int &stride);
  /* Metadatos del último fotograma (para importarlo en OpenGL si viene en dmabuf). */
  bool lastFrame(CapturedFrame &out) const;

  /* Diagnóstico para la línea de órdenes. */
  std::string describe() const;
  const std::string &lastError() const { return err_; }

private:
  struct Impl;
  std::unique_ptr<Impl> d;
  std::string err_;
};
