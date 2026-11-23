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
  if (!cam_.open(opts_.device, &local)) {
    if (err) *err = "abrir " + opts_.device + ": " + local;
    return false;
  }
  /* como StereoCamera.swift: 1280x480 MJPG a la tasa mas alta que se pueda */
  struct Intento {
    int w, h, fps;
    const char *fourcc;
  };
  const Intento intentos[] = {{1280, 480, 60, "MJPG"}, {1280, 480, 30, "MJPG"},
                              {1280, 480, 15, "YUYV"}, {640, 480, 60, "MJPG"},
                              {640, 480, 30, "YUYV"}};
  bool abierto = false;
  for (const auto &intento : intentos) {
    if (!cam_.setFormat(intento.w, intento.h, intento.fourcc, intento.fps, &local)) continue;
    if (!cam_.start(&local)) continue;
    char texto[64];
    std::snprintf(texto, sizeof texto, "%s %dx%d @ %d", intento.fourcc, cam_.width(), cam_.height(),
                  intento.fps);
    st_.mode = texto;
    abierto = true;
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

void TrackFeed::run() {
  CameraCalibration calib;
  TrackerTransform xform;
  std::string err;
  if (!loadFactoryCalib(opts_.calibDir, &calib, &xform, &err)) {
    note("calibracion: " + err);
    return;
  }
  HeadTracker tracker;
  if (!tracker.init(calib, xform, opts_.set, &err)) {
    note("rastreador: " + err);
    return;
  }
  std::vector<uint8_t> izquierda, derecha;
  int vw = 0, vh = 0;
  int sinCuadro = 0, malDecodificados = 0;
  while (!stop_) {
    const uint8_t *datos = nullptr;
    size_t bytes = 0;
    if (!cam_.read(&datos, &bytes, 250, &err)) {
      if (++sinCuadro % 20 == 1) note("camara: " + err);
      continue;
    }
    const bool decodificado = HeadTracker::decodeFrame(datos, bytes, cam_.width(), cam_.height(),
                                                        cam_.mjpeg(), izquierda, derecha, vw, vh, &err);
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
    tracker.process(izquierda.data(), derecha.data(), vw, vh, muestra, &err);
    const double t = reloj();
    std::lock_guard<std::mutex> guard(mu_);
    if (muestra.accepted) {
      st_.sample = muestra;
      st_.seen = true;
      st_.good++;
      acceptedAt_ = t;
      st_.note.clear();
    } else if (!muestra.reason.empty()) {
      st_.note = muestra.reason;
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

TrackFeedState TrackFeed::state() const {
  std::lock_guard<std::mutex> guard(mu_);
  TrackFeedState copia = st_;
  if (acceptedAt_ >= 0.0) copia.age = reloj() - acceptedAt_;
  return copia;
}
