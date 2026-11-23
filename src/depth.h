/* depth.h — calcular la profundidad de un cuadro normal (2D) para poder darle relieve en el
   panel del Odyssey. Usa un modelo de profundidad (Depth Anything V2 pequeno) con ONNX Runtime:
   si hay tarjeta NVIDIA se usa ella (unos 7 milisegundos por cuadro); si no, la CPU. */
#ifndef ODYSSEY_DEPTH_H
#define ODYSSEY_DEPTH_H

#include <cstdint>
#include <string>
#include <vector>

class DepthNet {
public:
  /* ruta del modelo .onnx; vacio = ~/.local/share/odyssey3d/modelos/depth_anything_v2_small.onnx */
  bool init(const std::string &model, std::string *err, int gpu = 0);   /* gpu: tarjeta NVIDIA a usar */
  bool ready() const { return ready_; }
  int size() const { return sz_; }                    /* el modelo trabaja a sz_ x sz_ */
  const std::string &device() const { return dev_; }  /* "NVIDIA" o "CPU" */
  double lastMs() const { return ms_; }               /* lo que tardo la ultima estimacion */

  /* rgba con la primera fila arriba, w x h. Escribe un byte por punto, del tamano del modelo
     (255 = lo mas cerca de la pantalla, 0 = el fondo). */
  bool estimate(const uint8_t *rgba, int w, int h, std::vector<uint8_t> &out, std::string *err);

  /* suavizado en el tiempo: 0 deja cada cuadro tal cual, 0.7 deja el mapa casi quieto.
     Sirve para que el relieve no parpadee cuadro a cuadro. */
  void setSmoothing(float a) { smooth_ = a < 0.0f ? 0.0f : (a > 0.95f ? 0.95f : a); }
  float smoothing() const { return smooth_; }

private:
  bool ready_ = false;
  int sz_ = 518;
  int gpu_ = 0;
  std::string dev_ = "nada";
  double ms_ = 0.0;
  float smooth_ = 0.60f;
  void *impl_ = nullptr;                 /* ocultar ONNX Runtime de este fichero de cabecera */
  std::vector<uint8_t> prev_;            /* mapa anterior, para suavizar */
  float loS_ = 0.0f, hiS_ = 1.0f;        /* escala en el tiempo (min/max suavizados) */
  bool haveScale_ = false;
};

#endif
