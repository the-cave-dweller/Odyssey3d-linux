#include "track.h"

#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect/face.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

/* --------------------------------------------------------------------------
   Transformada camara -> panel (SR+CALET), copiada de DisplayTransform.swift
   del proyecto original. Devuelve centimetros en el espacio del panel: x a la
   derecha, y hacia arriba, z hacia el espectador.
   -------------------------------------------------------------------------- */
static bool panelTransform(const TrackerTransform &tr, const double point[3], double out[3]) {
  const double radians = 0.017453292519943295;
  const double yAngle = tr.rotationDegrees[1] * radians;
  const double xAngle = tr.rotationDegrees[0] * radians;
  const double zAngle = M_PI + tr.rotationDegrees[2] * radians;
  const double sy = std::sin(yAngle), cy = std::cos(yAngle);
  const double sx = std::sin(xAngle), cx = std::cos(xAngle);
  const double sz = std::sin(zAngle), cz = std::cos(zAngle);
  if (std::fabs(cy) <= 1e-12 || std::fabs(cx) <= 1e-12) return false;
  const double y = tr.translationCentimetres[1] / cy + point[1];
  const double z = point[2];
  const double z1 = z * cy + sy * y;
  const double inverseX = 1.0 / cx;
  const double tilt = std::atan(sx * inverseX * cy);
  const double st = std::sin(tilt), ct = std::cos(tilt);
  const double x1 = point[0] + inverseX * tr.translationCentimetres[0];
  const double x2 = -st * z1 + ct * x1;
  const double y1 = -z * sy + cy * y;
  out[0] = x2 * cz + sz * y1;
  out[1] = cz * y1 - x2 * sz;
  out[2] = ct * z1 + tr.translationCentimetres[2] + st * x1;
  for (int i = 0; i < 3; i++)
    if (!std::isfinite(out[i])) return false;
  return true;
}

/* Triangulacion identica a odyssey_stereo_triangulate() del proyecto original:
   P0 = [I|0], P1 = [R|t], puntos desdistorsionados, y despues se mide el error
   de reproyeccion de verdad contra los dos cameras. */
/* matrices de las dos vistas a partir de la calibracion de fabrica */
static void stereoMatrices(const CameraCalibration &calib, cv::Mat &K0, cv::Mat &D0, cv::Mat &K1,
                           cv::Mat &D1, cv::Mat &R, cv::Mat &t) {
  double k0[9] = {calib.leftIntrinsics[0], 0, calib.leftIntrinsics[2], 0,
                  calib.leftIntrinsics[1], calib.leftIntrinsics[3], 0, 0, 1};
  double k1[9] = {calib.rightIntrinsics[0], 0, calib.rightIntrinsics[2], 0,
                  calib.rightIntrinsics[1], calib.rightIntrinsics[3], 0, 0, 1};
  double d0[8], d1[8], rv[3], tt[3];
  for (int i = 0; i < 8; i++) {
    d0[i] = calib.leftDistortion[i];
    d1[i] = calib.rightDistortion[i];
  }
  for (int i = 0; i < 3; i++) {
    rv[i] = calib.rotationVector[i];
    tt[i] = calib.translation[i];
  }
  K0 = cv::Mat(3, 3, CV_64F, k0).clone();
  K1 = cv::Mat(3, 3, CV_64F, k1).clone();
  D0 = cv::Mat(1, 8, CV_64F, d0).clone();
  D1 = cv::Mat(1, 8, CV_64F, d1).clone();
  t = cv::Mat(3, 1, CV_64F, tt).clone();
  cv::Mat rvec(3, 1, CV_64F, rv);
  cv::Rodrigues(rvec, R);
}

static bool triangulate(const CameraCalibration &calib, const double pixels[4], double minDepth,
                        double maxError, double point[3], double *error, std::string *err) {
  try {
    cv::Mat K0, D0, K1, D1, R, t;
    stereoMatrices(calib, K0, D0, K1, D1, R, t);
    cv::Mat identity = cv::Mat::eye(3, 3, CV_64F), zero = cv::Mat::zeros(3, 1, CV_64F);
    cv::Mat P0, P1;
    cv::hconcat(identity, zero, P0);
    cv::hconcat(R, t, P1);
    std::vector<cv::Point2d> left{{pixels[0], pixels[1]}}, right{{pixels[2], pixels[3]}};
    cv::undistortPoints(left, left, K0, D0);
    cv::undistortPoints(right, right, K1, D1);
    cv::Mat hom;
    cv::triangulatePoints(P0, P1, left, right, hom);
    const double inverse = 1.0 / hom.at<double>(3, 0);
    for (int axis = 0; axis < 3; axis++) point[axis] = inverse * hom.at<double>(axis, 0);
    for (int axis = 0; axis < 3; axis++)
      if (!std::isfinite(point[axis])) return false;

    std::vector<cv::Point3d> object{{point[0], point[1], point[2]}};
    std::vector<cv::Point2d> back0, back1;
    cv::Mat rv0, rv1;
    cv::Rodrigues(identity, rv0);
    cv::Rodrigues(R, rv1);
    cv::projectPoints(object, rv0, zero, K0, D0, back0);
    cv::projectPoints(object, rv1, t, K1, D1, back1);
    const double e0 = std::hypot(pixels[0] - back0[0].x, pixels[1] - back0[0].y);
    const double e1 = std::hypot(pixels[2] - back1[0].x, pixels[3] - back1[0].y);
    *error = std::max(e0, e1);
    if (*error > maxError || point[2] < minDepth) {
      char buf[160];
      std::snprintf(buf, sizeof buf, "triangulacion descartada (error %.2f px, z %.1f mm)", *error,
                    point[2]);
      if (err) *err = buf;
      return false;
    }
    return true;
  } catch (const cv::Exception &e) {
    if (err) *err = std::string("OpenCV: ") + e.what();
    return false;
  }
}

static double nowSeconds() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool HeadTracker::init(const CameraCalibration &calib, const TrackerTransform &xform,
                       const TrackSettings &set, std::string *err) {
  if (!set.mono && !calib.valid) {
    if (err) *err = "hace falta la calibracion de fabrica (ctl calib)";
    return false;
  }
  if (set.mono && (set.hFovDegrees < 20.0 || set.hFovDegrees > 170.0)) {
    if (err) *err = "el campo horizontal de la webcam ha de estar entre 20 y 170 grados";
    return false;
  }
  if (set.mono && set.faceWidthMillimetres < 80.0) {
    if (err) *err = "el ancho de cara ha de ser de al menos 80 mm";
    return false;
  }
  set_ = set;
  calib_ = calib;
  xform_ = xform;
  if (set_.model.empty()) {
    if (err) *err = "falta el modelo de cara (--track-model)";
    return false;
  }
  if (set_.detectWidth % 32 || set_.detectHeight % 32) {
    if (err) *err = "el tamano de deteccion ha de ser multiplo de 32";
    return false;
  }
  ready_ = true;
  haveLast_ = false;
  misses_ = 0;
  err_.clear();
  return true;
}

/* busca la cara mas grande y saca los dos ojos (YuNet: 4-5 ojo derecho, 6-7 ojo
   izquierdo, en coordenadas de la imagen de entrada). */
static bool detectEyes(const uint8_t *gray, int w, int h, const TrackSettings &set,
                       std::vector<cv::Point2f> &eyes, float &score, int &faces, std::string &why,
                       cv::Rect *caja = nullptr) {
  /* la red se carga una sola vez: crearla por cuadro costaria decenas de ms */
  static cv::Ptr<cv::FaceDetectorYN> detector;
  static std::string usedModel;
  static int usedW = 0, usedH = 0;
  static float usedScore = -1;
  if (!detector || usedModel != set.model || usedW != set.detectWidth || usedH != set.detectHeight ||
      usedScore != set.score) {
    detector = cv::FaceDetectorYN::create(set.model, "", cv::Size(set.detectWidth, set.detectHeight),
                                          set.score, 0.3f, 5000);
    if (!detector) {
      why = "no se pudo cargar el modelo YuNet";
      return false;
    }
    usedModel = set.model;
    usedW = set.detectWidth;
    usedH = set.detectHeight;
    usedScore = set.score;
  }
  cv::Mat image(h, w, CV_8UC1, (void *)gray);
  cv::Mat bgr, small;
  cv::cvtColor(image, bgr, cv::COLOR_GRAY2BGR);
  /* la red quiere exactamente su tamano fijo: se encoge la vista y luego se
     devuelven las coordenadas al tamano original */
  cv::resize(bgr, small, cv::Size(set.detectWidth, set.detectHeight));
  cv::Mat out;
  try {
    detector->detect(small, out);
  } catch (const cv::Exception &e) {
    why = std::string("deteccion: ") + e.what();
    return false;
  }
  const double sx = (double)w / (double)set.detectWidth, sy = (double)h / (double)set.detectHeight;
  if (!out.empty()) {
    for (int row = 0; row < out.rows; row++) {
      float *v = out.ptr<float>(row);
      for (int col = 0; col < 11; col += 2) {
        v[col] = (float)(v[col] * sx);
        v[col + 1] = (float)(v[col + 1] * sy);
      }
    }
  }
  faces = out.rows;
  if (out.rows == 0) {
    why = "sin cara";
    return false;
  }
  /* la cara con mas puntaje; si empatan, la mas grande. ademas hay que
     comprobar que los dos ojos caen dentro de la caja y separados lo que
     corresponde: descarta detecciones falsas con landmarks locos */
  int best = -1;
  double bestKey = -1;
  for (int row = 0; row < out.rows; row++) {
    const float *v = out.ptr<float>(row);
    const double area = (double)v[2] * (double)v[3];
    const double clave = (double)v[14] * 1e6 + (area > 1e6 ? 1e6 : area);
    const double dx = std::fabs((double)v[4] - (double)v[6]), dy = std::fabs((double)v[5] - (double)v[7]);
    const double separacion = std::hypot(dx, dy);
    if (separacion < 0.08 * area / (v[3] > 0 ? v[3] : 1) || separacion > 0.75 * (double)v[2]) continue;
    const bool dentro = v[4] >= v[0] && v[4] <= v[0] + v[2] && v[5] >= v[1] && v[5] <= v[1] + v[3] &&
                        v[6] >= v[0] && v[6] <= v[0] + v[2] && v[7] >= v[1] && v[7] <= v[1] + v[3];
    if (!dentro) continue;
    if (clave > bestKey) {
      bestKey = clave;
      best = row;
    }
  }
  if (best < 0) {
    why = "cara descartada por los ojos";
    return false;
  }
  const float *v = out.ptr<float>(best);
  score = v[14];
  eyes = {cv::Point2f(v[4], v[5]), cv::Point2f(v[6], v[7])};
  if (caja) *caja = cv::Rect((int)v[0], (int)v[1], (int)v[2], (int)v[3]);
  return true;
}

bool HeadTracker::process(const uint8_t *left, const uint8_t *right, int w, int h, TrackSample &out,
                          std::string *err) {
  out = TrackSample{};
  if (!ready_) {
    if (err) *err = "el rastreador no esta preparado";
    return false;
  }
  const double t0 = nowSeconds();
  std::vector<cv::Point2f> eyesL, eyesR;
  float scoreL = 0, scoreR = 0;
  std::string whyL, whyR;
  const bool okL = detectEyes(left, w, h, set_, eyesL, scoreL, out.faces, whyL);
  int facesR = 0;
  const bool okR = detectEyes(right, w, h, set_, eyesR, scoreR, facesR, whyR);
  out.detected = okL && okR;
  if (!out.detected) {
    out.reason = okL ? whyR : whyL;
    hold(out);
    out.latencyMs = (nowSeconds() - t0) * 1000.0;
    return true;
  }
  misses_ = 0;

  /* ojo derecho y ojo izquierdo, cada uno con su pareja en la otra vista */
  double mid[3] = {0, 0, 0};
  double worstError = 0;
  double eyePoint[2][3];
  int good = 0;
  for (int e = 0; e < 2; e++) {
    const double pixels[4] = {eyesL[e].x, eyesL[e].y, eyesR[e].x, eyesR[e].y};
    double point[3], error = 0;
    std::string why;
    if (!triangulate(calib_, pixels, set_.minDepthMillimetres, set_.maxReprojectionPx, point, &error,
                     &why)) {
      out.reason = why;
      continue;
    }
    eyePoint[good][0] = point[0];
    eyePoint[good][1] = point[1];
    eyePoint[good][2] = point[2];
    good++;
    worstError = std::max(worstError, error);
  }
  if (good < 2) {
    if (out.reason.empty()) out.reason = "no se pudieron triangular los dos ojos";
    hold(out);
    out.latencyMs = (nowSeconds() - t0) * 1000.0;
    return true;
  }
  for (int i = 0; i < 3; i++) mid[i] = 0.5 * (eyePoint[0][i] + eyePoint[1][i]);
  out.eyeMillimetres[0] = mid[0];
  out.eyeMillimetres[1] = mid[1];
  out.eyeMillimetres[2] = mid[2];
  out.ipdMillimetres = std::hypot(std::hypot(eyePoint[1][0] - eyePoint[0][0], eyePoint[1][1] - eyePoint[0][1]),
                                  eyePoint[1][2] - eyePoint[0][2]);
  out.reprojectionPx = worstError;

  double raw[3] = {0, 0, 0};
  const double centimetres[3] = {mid[0] * 0.1, mid[1] * 0.1, mid[2] * 0.1};
  if (!panelTransform(xform_, centimetres, raw)) {
    out.reason = "la transformada al panel fallo";
    hold(out);
    out.latencyMs = (nowSeconds() - t0) * 1000.0;
    return true;
  }
  for (int i = 0; i < 3; i++) out.rawPanelCentimetres[i] = raw[i];
  publish(raw, out);
  out.latencyMs = (nowSeconds() - t0) * 1000.0;
  return true;
}

void HeadTracker::hold(TrackSample &out) {
  out.misses = ++misses_;
  if (haveLast_ && misses_ <= set_.holdFrames) {
    out.accepted = true;
    out.predicted = true;
    for (int i = 0; i < 3; i++) out.panelCentimetres[i] = last_[i];
  }
}

/* mediana de tres muestras (como el hub) + guarda de saltos + filtro exponencial */
bool HeadTracker::publish(const double raw[3], TrackSample &out) {
  double observation[3];
  for (int i = 0; i < 3; i++) {
    history_[i].push_back(raw[i]);
    if (history_[i].size() > 3) history_[i].erase(history_[i].begin());
    std::vector<double> orden = history_[i];
    std::sort(orden.begin(), orden.end());
    observation[i] = orden[orden.size() / 2];
  }
  const double distancia = std::hypot(std::hypot(observation[0] - last_[0], observation[1] - last_[1]),
                                      observation[2] - last_[2]);
  if (haveLast_ && distancia > set_.maxJumpCentimetres) {
    out.reason = "salto demasiado grande, se descarta el cuadro";
    hold(out);
    return false;
  }
  const double alpha = set_.smoothing <= 0 ? 1.0 : set_.smoothing;
  for (int i = 0; i < 3; i++)
    last_[i] = haveLast_ ? last_[i] + (observation[i] - last_[i]) * alpha : observation[i];
  haveLast_ = true;
  lastTime_ = nowSeconds();
  out.accepted = true;
  for (int i = 0; i < 3; i++) out.panelCentimetres[i] = last_[i];
  return true;
}

/* ---------- la via de una sola vista: cualquier webcam -------------------------
   La camara infrarroja del monitor trae dos vistas calibradas y se puede
   triangular. Una webcam normal no: tiene una sola imagen y ninguna
   calibracion, asi que la pose sale de la caja de la cara. La focal se deduce
   del campo horizontal que se le supone al bicho (--track-campo); con eso, una
   cara de `caraMm` de ancho que en la imagen ocupa `caraPx` pixeles esta a
   `f * caraMm / caraPx`. El numero de distancia es grueso (la caja de YuNet
   cambia con la barba, las gafas o el pelo) pero el centro de la cara y el lado
   son buenos, y para afinar la distancia ya esta el deslizador de la pastilla. */
bool HeadTracker::processMono(const uint8_t *gray, int w, int h, TrackSample &out, std::string *err) {
  out = TrackSample{};
  out.mono = true;
  if (!ready_) {
    if (err) *err = "el rastreador no esta preparado";
    return false;
  }
  const double t0 = nowSeconds();
  std::vector<cv::Point2f> ojos;
  float puntaje = 0.0f;
  int caras = 0;
  std::string porQue;
  cv::Rect caja;
  if (!detectEyes(gray, w, h, set_, ojos, puntaje, caras, porQue, &caja)) {
    out.detected = false;
    out.faces = caras;
    out.reason = porQue;
    out.latencyMs = (nowSeconds() - t0) * 1000.0;
    hold(out);
    return true;
  }
  const double fov = set_.hFovDegrees;
  const double focal = (0.5 * (double)w) / std::tan(0.5 * fov * CV_PI / 180.0);
  /* la cara de frente mide unos 150 mm de oreja a oreja; de ahi la distancia.
     Si la caja sale absurda (pegada a la camara o fuera de la imagen), se planta en el
     rango que la trama puede seguir. */
  const double caraPx = caja.width > 1 ? (double)caja.width : 1.0;
  double z = focal * set_.faceWidthMillimetres / caraPx;
  if (z < set_.minDepthMillimetres) z = set_.minDepthMillimetres;
  if (z > 3000.0) z = 3000.0;
  const double x = ((double)caja.x + 0.5 * (double)caja.width - 0.5 * (double)w) * z / focal;
  const double y = (0.5 * (double)h - ((double)caja.y + 0.5 * (double)caja.height)) * z / focal;
  out.detected = true;
  out.faces = caras;
  out.faceWidthPx = caraPx;
  out.eyeMillimetres[0] = x;
  out.eyeMillimetres[1] = y;
  out.eyeMillimetres[2] = z;
  out.latencyMs = (nowSeconds() - t0) * 1000.0;
  /* Sin calibracion el paso a la pantalla es directo: la webcam esta pegada al
     borde de arriba del monitor, mas o menos centrada, y el panel mide en
     centimetros. `y` hacia arriba, que es como lo espera el visor. */
  const double cm[3] = {x * 0.1, y * 0.1, z * 0.1};
  return publish(cm, out);
}

void HeadTracker::monoSimular(double hFovDegrees, int anchoPx, double caraMm, double distanciaMm,
                              double *pixeles, double *profundidadMm) {
  const double focal = (0.5 * (double)anchoPx) / std::tan(0.5 * hFovDegrees * CV_PI / 180.0);
  const double px = focal * caraMm / (distanciaMm > 1.0 ? distanciaMm : 1.0);
  if (pixeles) *pixeles = px;
  if (profundidadMm) *profundidadMm = focal * caraMm / (px > 1.0 ? px : 1.0);
}

bool HeadTracker::submitEyes(const double pixels[4], TrackSample &out, std::string *err) {
  out = TrackSample{};
  if (!ready_) {
    if (err) *err = "el rastreador no esta preparado";
    return false;
  }
  double point[3], error = 0;
  std::string why;
  if (!triangulate(calib_, pixels, set_.minDepthMillimetres, set_.maxReprojectionPx, point, &error,
                   &why)) {
    out.reason = why;
    hold(out);
    return true;
  }
  out.detected = true;
  out.reprojectionPx = error;
  for (int i = 0; i < 3; i++) out.eyeMillimetres[i] = point[i];
  const double centimetres[3] = {point[0] * 0.1, point[1] * 0.1, point[2] * 0.1};
  double raw[3];
  if (!panelTransform(xform_, centimetres, raw)) {
    out.reason = "la transformada al panel fallo";
    hold(out);
    return true;
  }
  for (int i = 0; i < 3; i++) out.rawPanelCentimetres[i] = raw[i];
  publish(raw, out);
  return true;
}

bool HeadTracker::projectPoint(const double point[3], double pixels[4], std::string *err) {
  try {
    cv::Mat K0, D0, K1, D1, R, t;
    stereoMatrices(calib_, K0, D0, K1, D1, R, t);
    cv::Mat identity = cv::Mat::eye(3, 3, CV_64F), zero = cv::Mat::zeros(3, 1, CV_64F);
    cv::Mat rv0, rv1;
    cv::Rodrigues(identity, rv0);
    cv::Rodrigues(R, rv1);
    std::vector<cv::Point3d> obj{{point[0], point[1], point[2]}};
    std::vector<cv::Point2d> a, b;
    cv::projectPoints(obj, rv0, zero, K0, D0, a);
    cv::projectPoints(obj, rv1, t, K1, D1, b);
    pixels[0] = a[0].x;
    pixels[1] = a[0].y;
    pixels[2] = b[0].x;
    pixels[3] = b[0].y;
    return true;
  } catch (const cv::Exception &e) {
    if (err) *err = std::string("OpenCV: ") + e.what();
    return false;
  }
}

bool HeadTracker::describeStill(const std::string &path, const TrackSettings &set,
                                const std::string &dump, HeadTracker &tracker, TrackSample *sample,
                                std::string *info, std::string *err) {
  try {
    cv::Mat image = cv::imread(path, cv::IMREAD_GRAYSCALE);
    if (image.empty()) {
      if (err) *err = "no se pudo leer " + path;
      return false;
    }
    const int half = image.cols / 2;
    cv::Mat left = image(cv::Rect(0, 0, half, image.rows)).clone();
    cv::Mat right = image(cv::Rect(image.cols - half, 0, half, image.rows)).clone();
    for (int view = 0; view < 2; view++) {
      cv::Mat &m = view ? right : left;
      std::vector<cv::Point2f> eyes;
      float score = 0;
      int faces = 0;
      std::string why;
      char texto[420];
      if (!detectEyes(m.data, m.cols, m.rows, set, eyes, score, faces, why)) {
        std::snprintf(texto, sizeof texto, "vista %s: %s", view ? "derecha" : "izquierda", why.c_str());
      } else {
        std::snprintf(texto, sizeof texto,
                      "vista %s: %d cara(s), %d ojos, puntaje %.2f, ojo der (%.1f, %.1f), ojo izq (%.1f, %.1f)",
                      view ? "derecha" : "izquierda", faces, (int)eyes.size(), score, eyes[0].x,
                      eyes[0].y, eyes[1].x, eyes[1].y);
      }
      if (info) {
        if (!info->empty()) *info += "\n";
        *info += texto;
      }
    }
    if (!dump.empty()) {
      std::string d;
      if (!saveAnnotated(left.data, right.data, left.cols, left.rows, set, dump, &d) && err) *err = d;
    }
    if (sample) {
      std::string porque;
      tracker.process(left.data, right.data, left.cols, left.rows, *sample, &porque);
      char texto[240];
      std::snprintf(texto, sizeof texto,
                    "geometria: camara (%.1f, %.1f, %.1f) mm, panel (%.2f, %.2f, %.2f) cm, ipd %.1f mm, error %.2f px, %s%s",
                    sample->eyeMillimetres[0], sample->eyeMillimetres[1], sample->eyeMillimetres[2],
                    sample->panelCentimetres[0], sample->panelCentimetres[1], sample->panelCentimetres[2],
                    sample->ipdMillimetres, sample->reprojectionPx,
                    sample->accepted ? (sample->predicted ? "mantenida" : "valida") : "descartada",
                    (porque.empty() ? sample->reason : porque).c_str());
      if (info) {
        if (!info->empty()) *info += "\n";
        *info += texto;
      }
    }
    return true;
  } catch (const cv::Exception &e) {
    if (err) *err = std::string("OpenCV: ") + e.what();
    return false;
  }
}

bool HeadTracker::decodeFrame(const uint8_t *data, size_t size, int width, int height, bool mjpeg,
                              std::vector<uint8_t> &grayLeft, std::vector<uint8_t> &grayRight,
                              int &viewW, int &viewH, std::string *err) {
  try {
    cv::Mat frame;
    if (mjpeg) {
      frame = cv::imdecode(cv::Mat(1, (int)size, CV_8UC1, (void *)data), cv::IMREAD_GRAYSCALE);
    } else {
      /* YUYV 4:2:2 encajonado, tal como lo entrega el dispositivo */
      cv::Mat yuyv(height, width, CV_8UC2, (void *)data);
      cv::cvtColor(yuyv, frame, cv::COLOR_YUV2GRAY_YUYV);
    }
    if (frame.empty() || frame.cols != width || frame.rows != height) {
      if (err)
        *err = "el cuadro decodificado no cuadra (" + std::to_string(frame.cols) + "x" +
               std::to_string(frame.rows) + ")";
      return false;
    }
    const int half = frame.cols / 2;
    cv::Mat left = frame(cv::Rect(0, 0, half, frame.rows));
    cv::Mat right = frame(cv::Rect(frame.cols - half, 0, half, frame.rows));
    grayLeft.assign(left.data, left.data + (size_t)left.total() * left.elemSize());
    grayRight.assign(right.data, right.data + (size_t)right.total() * right.elemSize());
    viewW = half;
    viewH = frame.rows;
    return true;
  } catch (const cv::Exception &e) {
    if (err) *err = std::string("OpenCV al decodificar: ") + e.what();
    return false;
  }
}

bool HeadTracker::decodeMono(const uint8_t *data, size_t size, int width, int height, bool mjpeg,
                             std::vector<uint8_t> &gray, std::string *err) {
  try {
    cv::Mat frame;
    if (mjpeg) {
      frame = cv::imdecode(cv::Mat(1, (int)size, CV_8UC1, (void *)data), cv::IMREAD_GRAYSCALE);
    } else {
      cv::Mat yuyv(height, width, CV_8UC2, (void *)data);
      cv::cvtColor(yuyv, frame, cv::COLOR_YUV2GRAY_YUYV);
    }
    if (frame.empty() || frame.cols != width || frame.rows != height) {
      if (err)
        *err = "el cuadro decodificado no cuadra (" + std::to_string(frame.cols) + "x" +
               std::to_string(frame.rows) + ")";
      return false;
    }
    if (frame.type() != CV_8UC1) {
      cv::Mat g;
      cv::cvtColor(frame, g, cv::COLOR_BGR2GRAY);
      frame = g;
    }
    gray.assign(frame.data, frame.data + (size_t)frame.total() * frame.elemSize());
    return true;
  } catch (const cv::Exception &e) {
    if (err) *err = std::string("OpenCV al decodificar: ") + e.what();
    return false;
  }
}

bool HeadTracker::saveAnnotated(const uint8_t *left, const uint8_t *right, int w, int h,
                                const TrackSettings &set, const std::string &path,
                                std::string *err) {
  try {
    cv::Mat combo(h, w * 2, CV_8UC1);
    memcpy(combo.data, left, (size_t)w * h);
    memcpy(combo.data + (size_t)w * h, right, (size_t)w * h);
    cv::Mat color;
    cv::cvtColor(combo, color, cv::COLOR_GRAY2BGR);
    for (int view = 0; view < 2; view++) {
      std::vector<cv::Point2f> eyes;
      float score = 0;
      int faces = 0;
      std::string why;
      const uint8_t *base = view ? right : left;
      if (!detectEyes(base, w, h, set, eyes, score, faces, why)) continue;
      for (size_t e = 0; e < eyes.size(); e++) {
        const cv::Point2f p(eyes[e].x + (float)view * (float)w, eyes[e].y);
        cv::circle(color, p, 4, view ? cv::Scalar(0, 255, 255) : cv::Scalar(0, 128, 255), 1);
      }
      char text[64];
      std::snprintf(text, sizeof text, "%s %d caras %.2f", view ? "D" : "I", faces, score);
      cv::putText(color, text, cv::Point(8 + view * w, 20), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                  cv::Scalar(255, 255, 255), 1);
    }
    if (!cv::imwrite(path, color)) {
      if (err) *err = "no se pudo escribir " + path;
      return false;
    }
    return true;
  } catch (const cv::Exception &e) {
    if (err) *err = std::string("OpenCV al anotar: ") + e.what();
    return false;
  }
}
