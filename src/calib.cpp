#include "calib.h"
#include "util.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

static uint32_t leWord(const std::vector<uint8_t> &b, size_t off) {
  return (uint32_t)b[off] | ((uint32_t)b[off + 1] << 8) | ((uint32_t)b[off + 2] << 16) |
         ((uint32_t)b[off + 3] << 24);
}

static double leDouble(const std::vector<uint8_t> &b, size_t off) {
  uint64_t bits = 0;
  for (int i = 7; i >= 0; i--) bits = (bits << 8) | b[off + (size_t)i];
  double v = 0;
  std::memcpy(&v, &bits, sizeof v);
  return v;
}

static void readDoubles(const std::vector<uint8_t> &b, size_t off, size_t count, double *dst) {
  for (size_t i = 0; i < count; i++) dst[i] = leDouble(b, off + i * 8);
}

static bool allFinite(const double *v, size_t count) {
  for (size_t i = 0; i < count; i++)
    if (!std::isfinite(v[i])) return false;
  return true;
}

bool CameraCalibration::fromBlob(const std::vector<uint8_t> &blob) {
  valid = false;
  if (blob.size() < 248) return false;
  if (leWord(blob, 0) != 248 || leWord(blob, 4) != 1) return false;
  readDoubles(blob, 8, 4, leftIntrinsics);
  readDoubles(blob, 40, 4, rightIntrinsics);
  readDoubles(blob, 72, 8, leftDistortion);
  readDoubles(blob, 136, 8, rightDistortion);
  readDoubles(blob, 200, 3, rotationVector);
  readDoubles(blob, 224, 3, translation);
  valid = allFinite(leftIntrinsics, 4) && allFinite(rightIntrinsics, 4) &&
          allFinite(leftDistortion, 8) && allFinite(rightDistortion, 8) &&
          allFinite(rotationVector, 3) && allFinite(translation, 3);
  return valid;
}

bool TrackerTransform::fromBlob(const std::vector<uint8_t> &blob) {
  valid = false;
  if (blob.size() < 56) return false;
  if (leWord(blob, 0) != 56 || leWord(blob, 4) != 1) return false;
  readDoubles(blob, 8, 3, translationCentimetres);
  readDoubles(blob, 32, 3, rotationDegrees);
  valid = allFinite(translationCentimetres, 3) && allFinite(rotationDegrees, 3);
  return valid;
}

static std::string exeDirForCalib() {
  std::error_code ec;
  fs::path p = fs::read_symlink("/proc/self/exe", ec);
  if (ec || p.empty()) return {};
  return p.parent_path().string();
}

static std::vector<std::string> calibBases() {
  std::vector<std::string> bases;
  /* SR+CALCAM y SR+CALET son de fabrica POR UNIDAD: el dueno los saca de su propio tablero con
     `ctl calib` y se guardan en ~/.config/odysseyvk/calib. Ahi tenian que buscarse tambien, y hasta
     ahora no se buscaba: solo se miraba junto al binario y en rutas RELATIVAS al directorio actual.
     Lanzado desde la unidad de systemd el directorio actual es el home, asi que esas rutas relativas
     no llevaban a ningun sitio y el rastreo se quedaba sin calibracion. Medido el 2026-11-25: una
     sesion de 95 s lanzada por el servicio no recibio NI UN cuadro de la camara (0/0, nota
     "sin SR+CALCAM.bin en ."), mientras 3.768 lineas de sesiones arrancadas desde el repo si
     traian cuadros (hasta 59,8 fps). Desde el repo funcionaba por `./calib`, que es relativo. */
  if (const char *h = std::getenv("HOME"); h && *h) {
    bases.push_back(std::string(h) + "/.config/odysseyvk/calib");
    bases.push_back(std::string(h) + "/.config/odyssey3d/calib");
  }
  for (const char *b : {".", "./calib", "../calib"}) bases.push_back(b);
  std::string ed = exeDirForCalib();
  if (!ed.empty()) {
    bases.push_back(ed + "/calib");
    bases.push_back(ed + "/../share/odysseyvk/calib");
    bases.push_back(ed + "/../calib");
  }
  bases.push_back("/usr/share/odysseyvk/calib");
  bases.push_back("/usr/local/share/odysseyvk/calib");
  return bases;
}

bool loadFactoryCalib(const std::string &dir, CameraCalibration *cam, TrackerTransform *track,
                      std::string *err) {
  std::vector<std::string> bases;
  if (!dir.empty()) bases.push_back(dir);
  for (const std::string &b : calibBases()) bases.push_back(b);
  bool camOk = false, trackOk = false;
  for (const std::string &b : bases) {
    if (!camOk && cam) {
      std::vector<uint8_t> blob;
      if (readBinaryFile(b + "/SR+CALCAM.bin", blob) && cam->fromBlob(blob)) {
        camOk = true;
        if (err) err->clear();
      }
    }
    if (!trackOk && track) {
      std::vector<uint8_t> blob;
      if (readBinaryFile(b + "/SR+CALET.bin", blob) && track->fromBlob(blob)) {
        trackOk = true;
      }
    }
    if ((!cam || camOk) && (!track || trackOk)) break;
  }
  /* El aviso acaba en el estado del visor (`camara_nota`) y en el rastreo: decir solo el primer
     directorio hacia creer que no se busco en los demas. Se listan todos los probados. */
  if (err && ((!camOk && cam) || (!trackOk && track))) {
    std::string donde;
    for (size_t i = 0; i < bases.size(); ++i) {
      if (i) donde += ", ";
      donde += bases[i];
    }
    *err = std::string("sin ") + ((!camOk && cam) ? "SR+CALCAM.bin" : "SR+CALET.bin") +
           " (buscado en " + donde + ")";
  }
  return camOk || trackOk;
}

std::string defaultCalibDir() {
  for (const std::string &b : calibBases())
    if (fileExists(b + "/SR+CALCAM.bin") || fileExists(b + "/SR+CALET.bin")) return b;
  return {};
}
