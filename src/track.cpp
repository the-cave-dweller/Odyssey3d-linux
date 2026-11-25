#include "track.h"

#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect/face.hpp>
#include <opencv2/photo.hpp>

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

double HeadTracker::ahora() const { return reloj_ >= 0.0 ? reloj_ : nowSeconds(); }

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
  saltos_ = 0;
  err_.clear();
  return true;
}

/* busca la cara mas grande y saca los dos ojos (YuNet: 4-5 ojo derecho, 6-7 ojo
   izquierdo, en coordenadas de la imagen de entrada). */
static bool detectEyes(const uint8_t *gray, int w, int h, const TrackSettings &set,
                       std::vector<cv::Point2f> &eyes, float &score, int &faces, std::string &why,
                       cv::Rect *caja = nullptr, bool *realce = nullptr) {
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

  /* letterbox: la vista se encoje conservando la proporcion y se rellena con 114 hasta el
     tamano fijo que quiere la red, igual que hace el cliente de Mac (FaceProcessing.cpp).
     Antes se aplastaba con un resize a secas: la cara quedaba deformada y cuando estaba
     pegada a un borde YuNet simplemente no la veia. */
  const double escala = std::min((double)set.detectWidth / (double)w,
                                 (double)set.detectHeight / (double)h);
  const int rw = (int)std::lround((double)w * escala), rh = (int)std::lround((double)h * escala);
  const double factor = 1.0 / escala;

  auto lienzoDe = [&](const cv::Mat &gris) {
    cv::Mat bgr, lienzo(set.detectHeight, set.detectWidth, CV_8UC3, cv::Scalar(114, 114, 114));
    cv::cvtColor(gris, bgr, cv::COLOR_GRAY2BGR);
    cv::resize(bgr, lienzo(cv::Rect(0, 0, rw, rh)), cv::Size(rw, rh));
    return lienzo;
  };
  auto correr = [&](const cv::Mat &lienzo, cv::Mat &salida) {
    try {
      detector->detect(lienzo, salida);
      return true;
    } catch (const cv::Exception &e) {
      why = std::string("deteccion: ") + e.what();
      return false;
    }
  };

  cv::Mat out;
  if (!correr(lienzoDe(image), out))
    return false;
  if (out.rows == 0 && set.realce) {
    /* las vistas vienen del sensor en infrarrojo del monitor con un contraste muy plano:
       el cliente de Mac vuelve a buscar la cara con el contraste estirado cuando la
       primera pasada no encontro nada (FaceProcessing.cpp: clipLimit 40, tile 8x8).
       El reintento solo cuesta en los cuadros en los que no habia cara. */
    static cv::Ptr<cv::CLAHE> realce_ = cv::createCLAHE(40.0, cv::Size(8, 8));
    cv::Mat igualado;
    realce_->apply(image, igualado);
    if (!correr(lienzoDe(igualado), out))
      return false;
    if (out.rows > 0 && realce)
      *realce = true;
  }
  if (!out.empty()) {
    for (int row = 0; row < out.rows; row++) {
      float *v = out.ptr<float>(row);
      for (int col = 0; col < 11; col += 2) {
        v[col] = (float)(v[col] * factor);
        v[col + 1] = (float)(v[col + 1] * factor);
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

/* --------------------------------------------------------------------------
   Rastreo fino (v0.74): correlacion de imagen en vez de restar puntos de YuNet.
   -------------------------------------------------------------------------- */
/* Dos parches, en separaciones de ojos. La plantilla que se sigue de cuadro a cuadro es ancha (de
   sien a sien): cuanto mas dibujo, mas firme. El que se compara entre las dos vistas es solo lo
   que hay ENTRE los ojos: con la cabeza girada, por el lado de la sien se cuela el fondo, que esta
   a otra distancia, y la pareja saltaba entre dos sitios (medido: 53,1 -> 48,8 -> 53,1 px durante
   19 cuadros con el parche ancho; 54,2 / 53,9 / 53,2 con el estrecho). */
static const double kFranjaAncho = 1.6, kFranjaAlto = 0.7;
static const double kParejaAncho = 1.0, kParejaAlto = 0.7;
static const double kSepMin = 16.0, kSepMax = 160.0;         /* separaciones de ojos creibles (px) */
static const int kFranjaRadio = 12;                          /* busqueda de un cuadro al siguiente (px) */
static const double kPuntajePerdido = 0.5;      /* por debajo, la plantilla no esta en el cuadro */
/* por debajo un rato, la cara cambio de postura: plantilla nueva. Con 0,7 la plantilla se quedaba
   vieja y el maximo saltaba 3 px en cuadros sueltos (8 saltos en 20 s; con 0,8, uno). */
static const double kPuntajeRefresco = 0.8;
static const double kPuntajeDisparidad = 0.6;   /* minimo para fiarse de la pareja entre vistas */
static const double kTauAncla = 10.0;           /* segundos que tarda el ancla en seguir a YuNet */
static const double kDispCaduca = 0.25;         /* segundos que vale la disparidad anterior */
/* Tope del error de reproyeccion de la pareja fina. Una pareja buena lo tiene clavado (medido:
   0,82 px de mediana y 0,97 de maximo en 1178 cuadros de frente; 1,01 y 1,16 de p90 con la cara de
   lado en el borde del cuadro: es el desajuste fijo de la calibracion de fabrica); una pareja falsa
   se va de 2,5 px para arriba, porque casa la franja con otra altura. */
static const double kErrorFino = 2.0;

/* Busca `plantilla` en `imagen` alrededor de la esquina entera (x0, y0), hasta +-rx y +-ry pixeles
   (menos si la imagen se acaba antes). Devuelve la esquina hallada con decimales (parabola por el
   maximo y sus vecinos) y el parecido (correlacion normalizada de media cero, 1 = identicas). Falso
   si no cabe o si el maximo cae en el borde de la busqueda: ahi no se puede afinar ni fiar. */
static bool buscarPlantilla(const cv::Mat &imagen, const cv::Mat &plantilla, int x0, int y0, int rx, int ry,
                            double *hx, double *hy, double *parecido) {
  const int w = plantilla.cols, h = plantilla.rows;
  rx = std::min({rx, x0, imagen.cols - (x0 + w)});
  ry = std::min({ry, y0, imagen.rows - (y0 + h)});
  if (rx < 2 || ry < 2) return false;
  cv::Mat res;
  try {
    cv::matchTemplate(imagen(cv::Rect(x0 - rx, y0 - ry, w + 2 * rx, h + 2 * ry)), plantilla, res,
                      cv::TM_CCOEFF_NORMED);
  } catch (const cv::Exception &) {
    return false;
  }
  double maximo = 0;
  cv::Point donde;
  cv::minMaxLoc(res, nullptr, &maximo, nullptr, &donde);
  if (!std::isfinite(maximo) || donde.x == 0 || donde.y == 0 || donde.x == res.cols - 1 ||
      donde.y == res.rows - 1)
    return false;
  auto decimal = [](double a, double b, double c) {
    const double den = a - 2.0 * b + c;
    return std::fabs(den) < 1e-12 ? 0.0 : std::clamp(0.5 * (a - c) / den, -0.5, 0.5);
  };
  const double fx = decimal(res.at<float>(donde.y, donde.x - 1), maximo, res.at<float>(donde.y, donde.x + 1));
  const double fy = decimal(res.at<float>(donde.y - 1, donde.x), maximo, res.at<float>(donde.y + 1, donde.x));
  *hx = (double)(x0 - rx + donde.x) + fx;
  *hy = (double)(y0 - ry + donde.y) + fy;
  *parecido = maximo;
  return true;
}

/* Corta la plantilla de la franja de los ojos centrada en (cx, cy). El punto medio de los ojos no
   cambia: lo que sobre al redondear la esquina se lo queda el desfase. */
bool HeadTracker::cortarFranja(const uint8_t *left, int w, int h, double cx, double cy, double sep) {
  const int fw = std::max(21, (int)std::lround(kFranjaAncho * sep) | 1);
  const int fh = std::max(11, (int)std::lround(kFranjaAlto * sep) | 1);
  const int x0 = (int)std::lround(cx - 0.5 * fw), y0 = (int)std::lround(cy - 0.5 * fh);
  if (x0 < 0 || y0 < 0 || x0 + fw > w || y0 + fh > h) return false;
  franja_.resize((size_t)fw * (size_t)fh);
  for (int y = 0; y < fh; y++)
    std::memcpy(&franja_[(size_t)y * (size_t)fw], left + (size_t)(y0 + y) * (size_t)w + (size_t)x0, (size_t)fw);
  franjaW_ = fw;
  franjaH_ = fh;
  franjaPos_[0] = x0;
  franjaPos_[1] = y0;
  franjaOff_[0] = cx - ((double)x0 + 0.5 * fw);
  franjaOff_[1] = cy - ((double)y0 + 0.5 * fh);
  franjaSep_ = sep;
  franjaBajos_ = 0;
  return true;
}

bool HeadTracker::afinar(const uint8_t *left, const uint8_t *right, int w, int h, const double ojos[8],
                         double t, double par[4], TrackSample &out) {
  const cv::Mat L(h, w, CV_8UC1, (void *)left), R(h, w, CV_8UC1, (void *)right);
  /* lo que dice YuNet: punto medio y separacion en la vista izquierda, disparidad media de los dos ojos */
  const double yunX = 0.5 * (ojos[0] + ojos[2]), yunY = 0.5 * (ojos[1] + ojos[3]);
  const double sep = std::hypot(ojos[2] - ojos[0], ojos[3] - ojos[1]);
  const double dYunX = 0.5 * ((ojos[4] - ojos[0]) + (ojos[6] - ojos[2]));
  const double dYunY = 0.5 * ((ojos[5] - ojos[1]) + (ojos[7] - ojos[3]));
  const bool sepVale = sep >= kSepMin && sep <= kSepMax;
  double dt = franjaT_ >= 0.0 ? t - franjaT_ : 1.0 / 60.0;
  dt = std::clamp(dt, 1.0 / 240.0, 0.5);
  franjaT_ = t;

  /* 1. donde esta el punto medio de los ojos en la vista izquierda */
  double midX = 0, midY = 0;
  bool hayMedio = false;
  bool cuadra = false;   /* YuNet ve una cara del tamano de la que seguimos */
  if (franjaW_ > 0) {
    const cv::Mat plantilla(franjaH_, franjaW_, CV_8UC1, franja_.data());
    cuadra = sepVale && std::fabs(sep - franjaSep_) < 0.25 * franjaSep_;
    double hx = 0, hy = 0, s = 0;
    if (!buscarPlantilla(L, plantilla, (int)std::lround(franjaPos_[0]), (int)std::lround(franjaPos_[1]),
                         kFranjaRadio, kFranjaRadio, &hx, &hy, &s) ||
        s < kPuntajePerdido) {
      if (++franjaPerdidos_ > 5) franjaW_ = 0;   /* se fue: a empezar de YuNet */
    } else {
      franjaPerdidos_ = 0;
      franjaPos_[0] = hx;
      franjaPos_[1] = hy;
      const double cx = hx + 0.5 * franjaW_, cy = hy + 0.5 * franjaH_;
      if (cuadra) {
        /* el ancla: al principio la media de lo visto (1/n), luego un paso bajo de kTauAncla */
        franjaN_++;
        const double a = std::max(1.0 / (double)franjaN_, dt / kTauAncla);
        double ix = (yunX - cx) - franjaOff_[0], iy = (yunY - cy) - franjaOff_[1];
        if (franjaN_ > 30) {   /* ya asentada, un punto loco de YuNet no la mueve mas que uno normal */
          const double tope = 0.1 * franjaSep_;
          ix = std::clamp(ix, -tope, tope);
          iy = std::clamp(iy, -tope, tope);
        }
        franjaOff_[0] += a * ix;
        franjaOff_[1] += a * iy;
      }
      midX = cx + franjaOff_[0];
      midY = cy + franjaOff_[1];
      hayMedio = true;
      out.puntajeFranja = s;
      /* la plantilla lejos de YuNet un buen rato = se quedo pegada a otra cosa: se rehace */
      franjaLejos_ = (cuadra && std::hypot(yunX - midX, yunY - midY) > 0.25 * franjaSep_) ? franjaLejos_ + 1 : 0;
      if (franjaLejos_ > 12) {
        franjaW_ = 0;
        hayMedio = false;
      } else {
        franjaBajos_ = s < kPuntajeRefresco ? franjaBajos_ + 1 : 0;
        const bool otraEscala = cuadra && std::fabs(sep - franjaSep_) > 0.06 * franjaSep_;
        if (franjaBajos_ > 6 || (otraEscala && franjaBajos_ > 0))
          cortarFranja(left, w, h, midX, midY, cuadra ? sep : franjaSep_);
      }
    }
  }
  if (franjaW_ == 0 && sepVale && cortarFranja(left, w, h, yunX, yunY, sep)) {
    franjaN_ = 1;
    franjaPerdidos_ = 0;
    franjaLejos_ = 0;
    midX = yunX;
    midY = yunY;
    hayMedio = true;
    cuadra = true;
    out.puntajeFranja = 1.0;
  }
  if (!hayMedio) return false;

  /* 2. su pareja en la vista derecha: la franja de AHORA, buscada cerca de la disparidad anterior
        y, si no aparece, en una ventana ancha alrededor de la que dice YuNet */
  const int pw = std::max(21, (int)std::lround(kParejaAncho * franjaSep_) | 1);
  const int ph = std::max(11, (int)std::lround(kParejaAlto * franjaSep_) | 1);
  const int x0 = (int)std::lround(midX - 0.5 * pw), y0 = (int)std::lround(midY - 0.5 * ph);
  const bool previa = t >= dispT_ && t - dispT_ <= kDispCaduca;
  bool medida = false;
  if (x0 >= 0 && y0 >= 0 && x0 + pw <= w && y0 + ph <= h) {
    const cv::Mat parche = L(cv::Rect(x0, y0, pw, ph));
    double rx = 0, ry = 0, s = 0;
    bool ok = previa && buscarPlantilla(R, parche, x0 + (int)std::lround(disp_[0]), y0 + (int)std::lround(disp_[1]),
                                        4, 3, &rx, &ry, &s) && s >= kPuntajeDisparidad;
    if (!ok && cuadra)
      ok = buscarPlantilla(R, parche, x0 + (int)std::lround(dYunX), y0 + (int)std::lround(dYunY), 20, 6, &rx, &ry,
                           &s) && s >= kPuntajeDisparidad;
    /* con YuNet cuerdo, la pareja no puede quedar a mas de un tercio de cara de la suya: asi no
       se casa un ojo con el otro */
    if (ok && cuadra && std::fabs((rx - x0) - dYunX) > std::max(0.35 * franjaSep_, 8.0)) ok = false;
    if (ok) {
      disp_[0] = rx - x0;
      disp_[1] = ry - y0;
      dispT_ = t;
      medida = true;
      out.puntajeDisparidad = s;
    }
  }
  if (!medida && !previa) return false;   /* ni medida ni una reciente que aguantar */
  par[0] = midX;
  par[1] = midY;
  par[2] = midX + disp_[0];
  par[3] = midY + disp_[1];
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
  bool realceL = false, realceR = false;
  const bool okL = detectEyes(left, w, h, set_, eyesL, scoreL, out.facesLeft, whyL, nullptr, &realceL);
  const bool okR = detectEyes(right, w, h, set_, eyesR, scoreR, out.facesRight, whyR, nullptr, &realceR);
  out.faces = out.facesLeft + out.facesRight;
  out.enhanced = realceL || realceR;
  out.eyesRejected = whyL == "cara descartada por los ojos" || whyR == "cara descartada por los ojos";
  out.detected = okL && okR;
  if (!out.detected) {
    out.reason = okL ? whyR : whyL;
    hold(out);
    out.latencyMs = (nowSeconds() - t0) * 1000.0;
    return true;
  }
  misses_ = 0;

  double mid[3] = {0, 0, 0};
  double worstError = 0;
  double eyePoint[2][3];
  int good = 0;
  /* via fina (v0.74): el punto medio seguido con la plantilla y su pareja por correlacion. Para el
     ipd y el diagnostico, cada ojo es su punto de YuNet en la izquierda con la disparidad comun. */
  if (set_.fino) {
    const double ojos[8] = {eyesL[0].x, eyesL[0].y, eyesL[1].x, eyesL[1].y,
                            eyesR[0].x, eyesR[0].y, eyesR[1].x, eyesR[1].y};
    double par[4], punto[3], error = 0;
    std::string why;
    bool finoVale = afinar(left, right, w, h, ojos, ahora(), par, out) &&
                    triangulate(calib_, par, set_.minDepthMillimetres, set_.maxReprojectionPx, punto, &error, &why);
    if (finoVale && error > kErrorFino) {
      finoVale = false;
      dispT_ = -1e9;   /* esa disparidad no se guarda para el cuadro que viene */
    }
    if (finoVale) {
      out.fino = true;
      for (int k = 0; k < 4; k++) out.medioPx[k] = par[k];
      for (int e = 0; e < 2; e++) {
        const double pixels[4] = {ojos[2 * e], ojos[2 * e + 1], ojos[2 * e] + (par[2] - par[0]),
                                  ojos[2 * e + 1] + (par[3] - par[1])};
        double p3[3] = {punto[0], punto[1], punto[2]}, e3 = 0;
        triangulate(calib_, pixels, 0.0, 1e9, p3, &e3, nullptr);
        for (int k = 0; k < 4; k++) out.ojoPx[e][k] = pixels[k];
        for (int k = 0; k < 3; k++) out.ojoMm[e][k] = eyePoint[e][k] = p3[k];
      }
      /* el punto que se entrega es el medio triangulado, no la media de los dos ojos */
      for (int k = 0; k < 3; k++) mid[k] = punto[k];
      worstError = error;
      good = 3;
      finoT_ = ahora();
    } else if (ahora() >= finoT_ && ahora() - finoT_ <= kDispCaduca) {
      /* Un cuadro suelto sin via fina (la plantilla no aparecio, la pareja no convence): se aguanta
         la pose un cuarto de segundo antes de caer a la via de antes, que mide otra distancia
         (+-4 cm de sesgo) y metia un tiron. Medido en la segunda grabacion, con la cara de lado y
         en el borde del cuadro: la via de antes daba de 66 cm a 114 m. */
      out.reason = "rastreo fino sin medida: se aguanta la pose";
      hold(out);
      out.latencyMs = (nowSeconds() - t0) * 1000.0;
      return true;
    }
  }
  /* la via de siempre: ojo derecho y ojo izquierdo, cada uno con su pareja de YuNet en la otra vista */
  for (int e = 0; e < 2 && good < 3; e++) {
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
    for (int k = 0; k < 4; k++) out.ojoPx[e][k] = pixels[k];
    for (int k = 0; k < 3; k++) out.ojoMm[e][k] = point[k];
    good++;
    worstError = std::max(worstError, error);
  }
  if (good < 2) {
    if (out.reason.empty()) out.reason = "no se pudieron triangular los dos ojos";
    hold(out);
    out.latencyMs = (nowSeconds() - t0) * 1000.0;
    return true;
  }
  if (good < 3) {
    for (int i = 0; i < 3; i++) mid[i] = 0.5 * (eyePoint[0][i] + eyePoint[1][i]);
    out.reason.clear();
  }
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
  /* Primer enganche: no con UNA muestra suelta, que es cuando mas facil se cuela una mala (al
     arrancar, la cara entra a medias). Con tres la mediana ya trabaja. */
  if (!haveLast_ && history_[0].size() < 3) {
    out.reason = "enganchando: faltan muestras para la mediana";
    hold(out);
    return false;
  }
  const double distancia = std::hypot(std::hypot(observation[0] - last_[0], observation[1] - last_[1]),
                                      observation[2] - last_[2]);
  if (haveLast_ && distancia > set_.maxJumpCentimetres) {
    if (++saltos_ < set_.reengancheCuadros) {
      out.reason = "salto demasiado grande, se descarta el cuadro";
      hold(out);
      return false;
    }
    /* Muchos saltos seguidos: la equivocada era la pose guardada, no las nuevas. Se adopta la
       observacion (ya es mediana de tres) sin filtro, como un primer enganche. */
    out.reason = "re-enganche: la pose guardada era la que estaba mal";
    haveLast_ = false;
  }
  saltos_ = 0;
  if (set_.euroMinCorte > 0.0) {
    const double ahora = this->ahora();
    for (int i = 0; i < 3; i++) {
      FiltroEuro &f = euro_[i];
      f.minCorte = out.fino ? set_.euroMinCorteFino : set_.euroMinCorte;
      f.beta = out.fino ? set_.euroBetaFino : set_.euroBeta;
      f.corteDeriv = set_.euroCorteDeriv;
      if (!haveLast_) f.olvidar();   /* primer enganche o re-enganche: sin historia que arrastrar */
      last_[i] = f.paso(observation[i], ahora);
    }
  } else {
    const double alpha = set_.smoothing <= 0 ? 1.0 : set_.smoothing;
    for (int i = 0; i < 3; i++)
      last_[i] = haveLast_ ? last_[i] + (observation[i] - last_[i]) * alpha : observation[i];
  }
  haveLast_ = true;
  lastTime_ = this->ahora();
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
    /* OJO: laRegion del cuadro NO es contigua (el cuadro mide el doble de ancho), asi que hay que
       clonarla antes de coparla a byte plano. Sin el clone se copiaban filas de la otra vista y el
       detector recibia un cuadro mezcla: nunca encontraba la cara. */
    const cv::Mat left = frame(cv::Rect(0, 0, half, frame.rows)).clone();
    const cv::Mat right = frame(cv::Rect(frame.cols - half, 0, half, frame.rows)).clone();
    grayLeft.assign(left.data, left.data + left.total() * left.elemSize());
    grayRight.assign(right.data, right.data + right.total() * right.elemSize());
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

void HeadTracker::brilloDeVista(const uint8_t *gray, int w, int h, double *media, double *quemados) {
  if (!gray || w <= 0 || h <= 0) { *media = 0; *quemados = 0; return; }
  const cv::Mat vista(h, w, CV_8UC1, (void *)gray);
  *media = cv::mean(vista)[0];
  *quemados = 100.0 * (double)cv::countNonZero(vista > 250) / (double)vista.total();
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
