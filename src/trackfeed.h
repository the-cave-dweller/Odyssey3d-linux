/* trackfeed.h — un hilo que mira la camara infrarroja del monitor y deja lista
   la ultima posicion de la cabeza. Mirar la cara cuesta decenas de milisegundos
   y el panel va a 120 fps, asi que el rastreo va aparte del bucle de dibujado:
   el dibujado pregunta y usa lo ultimo que llego. */
#pragma once

#include "camera.h"
#include "track.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

struct TrackFeedOptions {
  std::string device;    /* vacio = buscar la camara sola */
  std::string model;     /* .onnx de YuNet */
  std::string calibDir;  /* vacio = buscar la calibracion de fabrica */
  TrackSettings set;
};

struct TrackFeedState {
  bool seen = false;             /* alguna vez hubo una pose valida */
  TrackSample sample;            /* ultima pose entregada (medida o aguantada) */
  double age = -1.0;             /* segundos desde esa pose (-1 = nunca) */
  long long frames = 0;          /* cuadros leidos de la camara */
  long long good = 0;            /* cuadros con pose aceptada */
  double cameraFps = 0.0;        /* cuadros por segundo leidos */
  std::string camera;            /* dispositivo en uso */
  std::string mode;              /* modo de la camara, ej. MJPG 1280x480 @ 60 */
  std::string note;              /* ultimo problema: sin cara, sin camara... */
};

class TrackFeed {
public:
  ~TrackFeed() { stop(); }

  /* Arranca el hilo. Devuelve falso si no hay camara, calibracion o modelo; el
     motivo queda en `err` (y en state().note). */
  bool start(const TrackFeedOptions &opts, std::string *err = nullptr);
  void stop();
  bool running() const { return running_; }

  /* Copia corta del estado mas reciente; se puede llamar cada cuadro. */
  TrackFeedState state() const;

private:
  void run();
  void note(const std::string &text);

  TrackFeedOptions opts_;
  Camera cam_;
  std::thread th_;
  mutable std::mutex mu_;
  TrackFeedState st_;
  std::atomic<bool> stop_{false};
  bool running_ = false;
  double startedAt_ = 0.0, acceptedAt_ = -1.0;
};
