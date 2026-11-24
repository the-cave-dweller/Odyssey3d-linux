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
  /* -1 = mirar el dispositivo y decidir; 0 = fuerza las dos vistas de la camara
     del monitor; 1 = fuerza la via de una sola vista (cualquier webcam). */
  int fuerzaMono = -1;
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
  bool mono = false;             /* se va por la cara en una sola vista (webcam normal) */
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
  /* Reabre el dispositivo con el ultimo modo que funciono. Unica forma decente de levantar una
     camara que deja de mandar cuadros: NUNCA se manda un reset de USB, que a la 04e8:20d7 del
     monitor la deja muerta hasta apagar y encender el monitor. */
  bool reabrirCamara(std::string *err);

  TrackFeedOptions opts_;
  Camera cam_;
  std::thread th_;
  mutable std::mutex mu_;
  TrackFeedState st_;
  std::atomic<bool> stop_{false};
  bool running_ = false;
  double startedAt_ = 0.0, acceptedAt_ = -1.0;
  bool mono_ = false;
  int modoW_ = 0, modoH_ = 0, modoFps_ = 0;   /* el modo que eligio start() */
  std::string modoCuatro_;
};
