/* Rastreo de cabeza estereo: cara + ojos con YuNet (OpenCV DNN), triangulacion
   con la calibracion de fabrica leida por SR+CALCAM y paso al espacio del panel
   con la transformada de SR+CALET. Todo lo de OpenCV se queda en track.cpp para
   que el resto del programa se pueda compilar sin OpenCV. */
#pragma once

#include "calib.h"

#include <cstdint>
#include <string>
#include <vector>

struct TrackSettings {
  std::string model;                /* .onnx de YuNet */
  float score = 0.6f;               /* umbral de deteccion de cara */
  double maxReprojectionPx = 3.0;   /* error de reproyeccion admitido */
  double minDepthMillimetres = 200; /* distancia minima a la camara */
  double maxJumpCentimetres = 12;   /* salto máximo entre cuadros aceptados */
  double smoothing = 0.35;          /* alfa del filtro exponencial (0 = sin filtro) */
  int holdFrames = 40;              /* cuadros que se aguanta sin cara */
  int detectWidth = 448, detectHeight = 320;  /* como el hub: 0.7 de la vista 640x480 */
};

struct TrackSample {
  bool detected = false;    /* cara en las dos vistas */
  bool accepted = false;    /* pose fiable y entregada */
  bool predicted = false;   /* se conserva la ultima pose, no es medida */
  double eyeMillimetres[3] = {0, 0, 0};  /* punto medio de los ojos, camara izquierda */
  double panelCentimetres[3] = {0, 0, 0}; /* la posicion que usa la trama */
  double rawPanelCentimetres[3] = {0, 0, 0};
  double ipdMillimetres = 0;
  double reprojectionPx = 0;
  int faces = 0;
  int misses = 0;
  double latencyMs = 0;
  std::string reason;       /* por que no se acepto, si acaso */
};

class HeadTracker {
public:
  bool init(const CameraCalibration &calib, const TrackerTransform &xform, const TrackSettings &set,
            std::string *err = nullptr);

  /* Pasa la luminancia de cada vista (w x h, sin separacion de fila). */
  bool process(const uint8_t *left, const uint8_t *right, int w, int h, TrackSample &out,
               std::string *err = nullptr);

  /* Decodifica el cuadro tal como lo entrega la camara (MJPG o YUYV, 1280x480
     con las dos vistas pegadas) y saca la luminancia de cada mitad. */
  static bool decodeFrame(const uint8_t *data, size_t size, int width, int height, bool mjpeg,
                          std::vector<uint8_t> &grayLeft, std::vector<uint8_t> &grayRight, int &viewW,
                          int &viewH, std::string *err = nullptr);

  /* Un solo ojo descrito por sus pixeles en las dos vistas (x,y izquierda y
     x,y derecha) y toda la cadena detras: triangulacion, transformada al panel
     y suavizado. Es la via del simulacro, para mirar la geometria sin camara. */
  bool submitEyes(const double pixels[4], TrackSample &out, std::string *err = nullptr);

  /* Proyecta un punto 3D conocido (milimetros, camara izquierda) en las dos
     vistas con la calibracion de fabrica: para el simulacro. */
  bool projectPoint(const double point[3], double pixels[4], std::string *err = nullptr);

  /* Olvida la pose guardada: util entre pruebas sueltas. */
  void reset() {
    haveLast_ = false;
    misses_ = 0;
    for (int i = 0; i < 3; i++) history_[i].clear();
  }

  /* Comprueba deteccion y geometria sobre un archivo (PNG/JPG) con las dos
     vistas pegadas, sin tocar la camara. */
  static bool describeStill(const std::string &path, const TrackSettings &set,
                            const std::string &dump, HeadTracker &tracker, TrackSample *sample,
                            std::string *info, std::string *err = nullptr);

  /* Guarda un JPEG con las caras y los ojos marcados, para revisar la deteccion. */
  static bool saveAnnotated(const uint8_t *left, const uint8_t *right, int w, int h,
                            const TrackSettings &set, const std::string &path,
                            std::string *err = nullptr);

  const std::string &lastError() const { return err_; }

private:
  struct Eye {
    bool ok = false;
    double pixelLeft[2] = {0, 0}, pixelRight[2] = {0, 0};
  };

  void hold(TrackSample &out);
  bool publish(const double raw[3], TrackSample &out);

  TrackSettings set_;
  CameraCalibration calib_;
  TrackerTransform xform_;
  bool ready_ = false, haveLast_ = false;
  double last_[3] = {0, 0, 0};
  double lastTime_ = 0;
  int misses_ = 0;
  std::vector<double> history_[3];
  std::string err_;
};
