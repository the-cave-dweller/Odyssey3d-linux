/* trackfeed.cpp — el hilo del rastreo. Ver trackfeed.h. */
#include "trackfeed.h"

#include "calib.h"
#include "camera.h"
#include "util.h"

#include <chrono>

static double reloj() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void TrackFeed::note(const std::string &text) {
  std::lock_guard<std::mutex> guard(mu_);
  st_.note = text;
}

bool TrackFeed::start(const TrackFeedOptions &opts, std::string *err) {
  opts_ = opts;
  stop_ = false;
  st_ = TrackFeedState{};
  if (opts_.model.empty() || !fileExists(opts_.model)) {
    if (err) *err = "no se encontro el modelo de cara (" + opts_.model + "): bajaolo con tools/descargar-modelo.sh";
    return false;
  }
  std::string local;
  if (!Camera::pickDevice(opts_.device, &local)) {
    if (err) *err = local;
    return false;
  }
  st_.camera = opts_.device;
  /* Que camara es queda apuntado para la pastilla y para la ventana de ajustes:
     la integrada del monitor es "3D Studio", cualquiera otra es la de reserva. */
  {
    Camera::Info info;
    if (Camera::describe(opts_.device, &info)) {
      st_.nombre = info.nombre;
      st_.esMonitor = info.esMonitor;
    }
  }
  /* Que clase de camara es decide todo lo demas. La del monitor (04e8:20d7)
     entrega un cuadro de 1280x480 con las dos vistas pegadas y viene calibrada
     de fabrica: ahi se triangulan los dos ojos. Cualquier otra webcam que se
     enchufe es una sola vista sin calibrar, y ahi el rastreo va por el ancho de
     la cara; es menos fino en la distancia, pero sigue moviendo la trama. */
  mono_ = opts_.fuerzaMono >= 0 ? opts_.fuerzaMono == 1 : !Camera::esEstereoDelMonitor(opts_.device);
  opts_.set.mono = mono_;
  st_.mono = mono_;
  if (!cam_.open(opts_.device, &local)) {
    if (err) *err = "abrir " + opts_.device + ": " + local;
    return false;
  }
  /* como StereoCamera.swift: 1280x480 MJPG a la tasa mas alta que se pueda */
  struct Intento {
    int w, h, fps;
    const char *fourcc;
  };
  Intento listaEstereo[] = {{1280, 480, 60, "MJPG"}, {1280, 480, 30, "MJPG"},
                            {1280, 480, 15, "YUYV"}, {640, 480, 60, "MJPG"},
                            {640, 480, 30, "YUYV"}};
  /* Una webcam normal: primero lo que casi todas aceptan sin quejas. El MJPG
     deja subir de resolucion sin partir el USB; la YUYV de 640x480 es el suelo
     seguro. Mas pixeles = la caja de la cara se mide mejor = distancia mas finos. */
  Intento listaMono[] = {{1280, 720, 30, "MJPG"}, {640, 480, 30, "MJPG"}, {640, 480, 60, "MJPG"},
                         {1280, 720, 15, "MJPG"}, {640, 480, 30, "YUYV"}, {320, 240, 30, "YUYV"}};
  const Intento *intentos = mono_ ? listaMono : listaEstereo;
  const size_t cuentaIntentos = mono_ ? sizeof listaMono / sizeof listaMono[0]
                                      : sizeof listaEstereo / sizeof listaEstereo[0];
  bool abierto = false;
  for (size_t numero = 0; numero < cuentaIntentos && !abierto; numero++) {
    const Intento &intento = intentos[numero];
    if (!cam_.setFormat(intento.w, intento.h, intento.fourcc, intento.fps, &local)) continue;
    if (!cam_.start(&local)) continue;
    char texto[80];
    std::snprintf(texto, sizeof texto, "%s %dx%d @ %d%s", intento.fourcc, cam_.width(), cam_.height(),
                  intento.fps, mono_ ? " (una vista, por la cara)" : " (dos vistas, calibrada)");
    st_.mode = texto;
    st_.jpeg = cam_.mjpeg();
    abierto = true;
    modoW_ = cam_.width();
    modoH_ = cam_.height();
    modoFps_ = intento.fps;
    modoCuatro_ = intento.fourcc;
    break;
  }
  if (!abierto) {
    cam_.close();
    if (err) *err = "la camara " + opts_.device + " no acepto ningun modo (ultimo fallo: " + local + ")";
    return false;
  }
  startedAt_ = reloj();
  th_ = std::thread(&TrackFeed::run, this);
  running_ = true;
  return true;
}

bool TrackFeed::reabrirCamara(std::string *err) {
  if (modoCuatro_.empty()) return false;
  cam_.stop();
  cam_.close();
  std::this_thread::sleep_for(std::chrono::milliseconds(400));
  std::string local;
  if (!cam_.open(opts_.device, &local)) {
    if (err) *err = "abrir: " + local;
    return false;
  }
  if (!cam_.setFormat(modoW_, modoH_, modoCuatro_, modoFps_, &local)) {
    cam_.close();
    if (err) *err = "formato: " + local;
    return false;
  }
  if (!cam_.start(&local)) {
    cam_.close();
    if (err) *err = "arranque del flujo: " + local;
    return false;
  }
  if (err) err->clear();
  return true;
}

void TrackFeed::run() {
  CameraCalibration calib;
  TrackerTransform xform;
  std::string err;
  if (!mono_ && !loadFactoryCalib(opts_.calibDir, &calib, &xform, &err)) {
    note("calibracion: " + err);
    /* Importante: si nos vamos sin leer, la camara se queda con el flujo montado y nadie se lo
       come. La 04e8:20d7 no lo lleva bien (deja de responder al S_FMT y hay que apagar el
       monitor para levantarla), asi que soltamos el flujo antes de salir. */
    cam_.stop();
    cam_.close();
    return;
  }
  if (mono_)
    note("webcam normal: la distancia sale del ancho de la cara (aproximada)");
  HeadTracker tracker;
  if (!tracker.init(calib, xform, opts_.set, &err)) {
    note("rastreador: " + err);
    cam_.stop();
    cam_.close();
    return;
  }
  std::vector<uint8_t> izquierda, derecha;
  int vw = 0, vh = 0;
  int sinCuadro = 0, malDecodificados = 0, contados = 0;
  while (!stop_) {
    const uint8_t *datos = nullptr;
    size_t bytes = 0;
    if (!cam_.read(&datos, &bytes, 250, &err)) {
      if (++sinCuadro % 20 == 1) note("camara: " + err);
      /* Tres segundos sin un solo cuadro: la camara se colgo del flujo. La unica recuperacion
         limpia es cerrar el dispositivo y volver a montarlo; no tocar el bus USB (un reset de
         control deja la camara del monitor muerta hasta apagar y encender el monitor). Despues
         del primer intento, se reintenta cada 40 cuadros de espera. */
      if (sinCuadro == 12 || (sinCuadro > 12 && sinCuadro % 40 == 0)) {
        std::string rerr;
        if (reabrirCamara(&rerr)) {
          sinCuadro = 0;
          note("camara: flujo reabierto");
        } else {
          note("camara: reapertura fallida, " + rerr);
          std::this_thread::sleep_for(std::chrono::seconds(2));
        }
      }
      continue;
    }
    sinCuadro = 0;
    bool decodificado = false;
    if (mono_) {
      decodificado = HeadTracker::decodeMono(datos, bytes, cam_.width(), cam_.height(), cam_.mjpeg(),
                                             izquierda, &err);
      vw = cam_.width();
      vh = cam_.height();
    } else {
      decodificado = HeadTracker::decodeFrame(datos, bytes, cam_.width(), cam_.height(), cam_.mjpeg(),
                                              izquierda, derecha, vw, vh, &err);
    }
    /* Cada 20 cuadros (3 o 4 por segundo) nos quedamos con el cuadro crudo tal como lo
       solto la camara: es un memcpy de unos 100 KB y solo lo pide el bucle de dibujado,
       que lo vuelca a ~/.local/state/odyssey3d/camara.jpg. Asi la ventana de ajustes
       ensena lo que se esta viendo sin abrir ella el dispositivo, que estaria ocupado. */
    if (cam_.mjpeg() && ++contados % 20 == 0) {
      std::lock_guard<std::mutex> guard(mu_);
      ultimo_.assign(datos, datos + bytes);
      ultimoNuevo_ = true;
    }
    cam_.release();
    {
      std::lock_guard<std::mutex> guard(mu_);
      st_.frames++;
    }
    if (!decodificado) {
      if (++malDecodificados % 30 == 1) note("cuadro: " + err);
      continue;
    }
    TrackSample muestra;
    if (mono_)
      tracker.processMono(izquierda.data(), vw, vh, muestra, &err);
    else
      tracker.process(izquierda.data(), derecha.data(), vw, vh, muestra, &err);
    const double t = reloj();
    std::lock_guard<std::mutex> guard(mu_);
    st_.cara = muestra.detected;
    if (muestra.accepted) {
      st_.sample = muestra;
      st_.seen = true;
      st_.good++;
      acceptedAt_ = t;
      st_.note.clear();
    } else if (!muestra.reason.empty()) {
      st_.note = muestra.reason;
      /* Cinco segundos de camara y ni una cara: casi siempre es que la cara no entra en el
         campo, no que la camara este mal. Las dos vistas del monitor miran rectas al frente, asi
         que hay que estar centrado bajo ellas. Se comprueba sin montar una sesion con
         `ctl track`, y la foto que ve la camara sale con
         `ffmpeg -f v4l2 -input_format mjpeg -video_size 1280x480 -i /dev/video0 -frames:v 1 /tmp/ir.png`. */
      if (st_.good == 0 && st_.frames > 240)
        st_.note += " (la cara no entra en el campo de la camara: date un centro bajo la camara del monitor)";
    }
    st_.age = acceptedAt_ < 0.0 ? -1.0 : t - acceptedAt_;
    if (t > startedAt_) st_.cameraFps = (double)st_.frames / (t - startedAt_);
  }
}

void TrackFeed::stop() {
  if (!running_) return;
  stop_ = true;
  cam_.stop();
  if (th_.joinable()) th_.join();
  cam_.close();
  running_ = false;
}

bool TrackFeed::ultimoCuadro(std::vector<uint8_t> *out) {
  std::lock_guard<std::mutex> guard(mu_);
  if (!ultimoNuevo_ || ultimo_.empty()) return false;
  ultimoNuevo_ = false;
  if (out) *out = ultimo_;
  return true;
}

TrackFeedState TrackFeed::state() const {
  std::lock_guard<std::mutex> guard(mu_);
  TrackFeedState copia = st_;
  if (acceptedAt_ >= 0.0) copia.age = reloj() - acceptedAt_;
  return copia;
}
