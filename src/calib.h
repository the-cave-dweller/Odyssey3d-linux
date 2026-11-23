/* calib.h — calibracion de fabrica del subsistema 3D del monitor.
   El tablero de lente (SR Lens Switch Board) guarda dos blobs leibles por el
   protocolo SR+: SR+CALCAM con la calibracion estereo de la camara IR y
   SR+CALET con la transformada entre la camara y el centro del panel.
   Los numeros son dobles en little-endian, con cabecera {tamano, version}. */
#ifndef ODYSSEY_CALIB_H
#define ODYSSEY_CALIB_H

#include <cstdint>
#include <string>
#include <vector>

/* SR+CALCAM: 248 bytes. Intrinsecas fx, fy, cx, cy por vista; distorsion de
   ocho terminos; rotacion en vector y traslacion de la camara derecha. */
struct CameraCalibration {
  double leftIntrinsics[4]{};
  double rightIntrinsics[4]{};
  double leftDistortion[8]{};
  double rightDistortion[8]{};
  double rotationVector[3]{};
  double translation[3]{};
  bool valid = false;

  bool fromBlob(const std::vector<uint8_t> &blob);
};

/* SR+CALET: 56 bytes. Desplazamiento en cm y giro en grados de la camara
   respecto al sistema de coordenadas del panel (el que usa la trama). */
struct TrackerTransform {
  double translationCentimetres[3]{};
  double rotationDegrees[3]{};
  bool valid = false;

  bool fromBlob(const std::vector<uint8_t> &blob);
};

/* Carga SR+CALCAM.bin y SR+CALET.bin desde DIR. Deja de lado lo que falte. */
bool loadFactoryCalib(const std::string &dir, CameraCalibration *cam, TrackerTransform *track,
                      std::string *err);

/* Ruta del directorio de calibracion (busca en el arbol del ejecutable y en
   /usr/share/odyssey3d/calib). Vacio si no hay nada. */
std::string defaultCalibDir();

#endif
