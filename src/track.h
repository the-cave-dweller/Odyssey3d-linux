/* Rastreo de cabeza estereo: cara + ojos con YuNet (OpenCV DNN), triangulacion
   con la calibracion de fabrica leida por SR+CALCAM y paso al espacio del panel
   con la transformada de SR+CALET. Todo lo de OpenCV se queda en track.cpp para
   que el resto del programa se pueda compilar sin OpenCV. */
#pragma once

#include "calib.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

/* Filtro One-Euro (Casiez, Roussel y Vogel, CHI 2012) para UNA coordenada (v0.60).
   Es un paso bajo de primer orden cuyo corte sube con la velocidad: quieto corta muy bajo y se come
   el temblor; en marcha abre el corte y no se queda atras. Hace falta porque la trama es muy
   sensible a la pose: a 60 cm un centimetro de x mueve la fase 5,239/60 = 0,087 ciclos, y el ojo
   que debe ver negro solo tiene +-0,04 de margen antes de que se cuele el subpixel rojo o el azul
   (zona negra +-0,225, subpixeles a +-0,185). Con el exponencial de antes (alfa 0,35 por cuadro,
   constante de tiempo de ~2 cuadros) el temblor de la deteccion pasaba casi entero.
   corte = minCorte + beta * |velocidad filtrada|; minCorte y corteDeriv en Hz, beta en 1/cm. */
struct FiltroEuro {
  double minCorte = 0.6, beta = 0.01, corteDeriv = 1.0;
  bool listo = false;
  double x = 0.0, dx = 0.0, t = 0.0;
  static double alfa(double corte, double dt) {
    const double tau = 1.0 / (2.0 * M_PI * corte);
    return 1.0 / (1.0 + tau / dt);
  }
  double paso(double valor, double ahora) {
    if (!listo) {
      listo = true;
      x = valor;
      dx = 0.0;
      t = ahora;
      return x;
    }
    double dt = ahora - t;
    if (dt < 1e-4) dt = 1e-4;   /* dos muestras en el mismo instante: que no divida por cero */
    if (dt > 0.5) dt = 0.5;     /* tras un hueco largo no se fia de una velocidad enorme */
    t = ahora;
    dx += alfa(corteDeriv, dt) * ((valor - x) / dt - dx);
    x += alfa(minCorte + beta * std::fabs(dx), dt) * (valor - x);
    return x;
  }
  void olvidar() { listo = false; }
};

struct TrackSettings {
  std::string model;                /* .onnx de YuNet */
  float score = 0.5f;               /* umbral de deteccion de cara; 0,5 como el cliente de Mac */
  double maxReprojectionPx = 10.0;  /* error de reproyeccion admitido (antes 3 px: descartaba
                                      triangulaciones buenas; el cliente de Mac admite 40 px) */
  bool realce = true;               /* reintentar con CLAHE cuando no sale ninguna cara */
  double minDepthMillimetres = 200; /* distancia minima a la camara */
  double maxJumpCentimetres = 12;   /* salto máximo entre cuadros aceptados */
  double smoothing = 0.35;          /* alfa del filtro exponencial (0 = sin filtro); solo cuenta
                                       con el One-Euro apagado (euroMinCorte <= 0) */
  /* One-Euro (v0.60, ver FiltroEuro): corte minimo en Hz (<= 0 vuelve al exponencial de antes),
     cuanto abre por cm/s y corte de la velocidad. ODYSSEYVK_EURO=0 lo apaga; "0,6;0,01" los cambia.
     Elegidos con tools/prueba_euro.cpp (ruido medido: z +-3 cm, x +-0,8 cm por cuadro): la fase tiembla
     0,015 ciclos en el centro y 0,033 en el borde (antes 0,034 y 0,066), dentro del margen de +-0,04, a
     cambio de 2,7 cm de retraso a 20 cm/s (antes 1,7). Con beta 0,25 el ruido de la deteccion
     se lee como velocidad, abre el corte y no suaviza nada: por eso beta va tan bajo. */
  double euroMinCorte = 0.6, euroBeta = 0.01, euroCorteDeriv = 1.0;
  int holdFrames = 40;              /* cuadros que se aguanta sin cara */
  /* Re-enganche (v0.59): cuadros SEGUIDOS rechazados por salto despues de los cuales se da por buena
     la pose nueva. Sin esto, una pose mala que se colara una vez (medido en el panel: x -22,6 cm con la
     cara al centro) dejaba todas las buenas "demasiado lejos" y el rastreo se quedaba clavado en ella
     para siempre: la deteccion pone `misses_` a 0 en cada cuadro y `hold` reenvia la vieja como fresca.
     15 cuadros son ~0,25 s a 60 fps. */
  int reengancheCuadros = 15;
  int detectWidth = 448, detectHeight = 320;  /* como el hub: 0.7 de la vista 640x480 */

  /* Via de respaldo: una webcam normal, que es UNA sola vista y no esta
     calibrada. La distancia sale del ancho de la caja de la cara (de oreja a
     oreja) y el lado del campo horizontal que se le supone a la camara. */
  bool mono = false;
  double faceWidthMillimetres = 140.0;  /* oreja a oreja; el rastreador de Samsung usa 140
                                           (tracker-ft_user.ini: FaceWidth_mm=140). Se cambia sin
                                           recompilar con --track-cara. */
  double hFovDegrees = 65.0;
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
  bool mono = false;          /* pose sacada de una sola vista (webcam normal) */
  double faceWidthPx = 0;     /* ancho de la caja de la cara, en la via mono */
  int faces = 0;
  int facesLeft = 0, facesRight = 0;  /* caras que vio YuNet en cada vista */
  bool eyesRejected = false;      /* hubo cara pero la tiro la comprobacion de los ojos */
  bool enhanced = false;          /* el cuadro lo saco el reintento con CLAHE */
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

  /* Una sola vista (la webcam que sea): de la caja de la cara salen el lado,
     arriba/abajo y la distancia, sin calibracion y sin triangulacion. */
  bool processMono(const uint8_t *gray, int w, int h, TrackSample &out, std::string *err = nullptr);

  /* Decodifica el cuadro de una webcam normal (MJPG o YUYV, una sola vista). */
  static bool decodeMono(const uint8_t *data, size_t size, int width, int height, bool mjpeg,
                         std::vector<uint8_t> &gray, std::string *err = nullptr);

  /* La cuenta de la via mono, para comprobarla sin camara: cuantos pixeles mide
     una cara de `caraMm` a `distanciaMm` con el campo `hFovDegrees` en una fila
     de `anchoPx` pixeles, y la inversa. */
  static void monoSimular(double hFovDegrees, int anchoPx, double caraMm, double distanciaMm,
                          double *pixeles, double *profundidadMm);

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
    saltos_ = 0;
    for (int i = 0; i < 3; i++) {
      history_[i].clear();
      euro_[i].olvidar();
    }
  }

  /* Comprueba deteccion y geometria sobre un archivo (PNG/JPG) con las dos
     vistas pegadas, sin tocar la camara. */
  static bool describeStill(const std::string &path, const TrackSettings &set,
                            const std::string &dump, HeadTracker &tracker, TrackSample *sample,
                            std::string *info, std::string *err = nullptr);

  /* Guarda un JPEG con las caras y los ojos marcados, para revisar la deteccion. */
  /* brillo medio y porcentaje de pixels quemados de una vista en grises */
  static void brilloDeVista(const uint8_t *gray, int w, int h, double *media, double *quemados);

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
  int saltos_ = 0;   /* cuadros seguidos rechazados por salto (ver reengancheCuadros) */
  FiltroEuro euro_[3];
  std::vector<double> history_[3];
  std::string err_;
};
