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
  ~DepthNet();
  /* ruta del modelo .onnx; vacio = ~/.local/share/odyssey3d/modelos/depth_anything_v2_small.onnx */
  bool init(const std::string &model, std::string *err, int gpu = 0);   /* gpu: tarjeta NVIDIA a usar */

  /* Cambiar de red con el visor en marcha: primero se suelta la sesion vieja (que es lo que
     devuelve la memoria de la grafica) y despues se abre la nueva. Si la nueva no abre, se
     vuelve a abrir la vieja y el conversor sigue como estaba. */
  bool cambiar(const std::string &model, std::string *err);
  const std::string &modelPath() const { return ruta_; }   /* la .onnx que esta abierta ahora */
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

  /* plano de enfoque: donde deberia apoyarse el plano de la pantalla (0 = fondo, 1 = lo mas cerca).
     Sale del histograma del mapa ya limpio, ponderando el centro del cuadro y los cantos, para que
     el sujeto quede apoyado en la pantalla y el relieve se abra detras. Se mueve despacio para que
     el apoyo no baile, y de golpe en cada cambio de plano. */
  float focusPlane() const { return focus_; }
  bool lastCut() const { return corte_; }    /* hubo cambio de plano en la ultima estimacion */
  long cutCount() const { return cortes_; }  /* cambios de plano contados desde el arranque */

  /* limpieza del mapa (filtro guiado + dilatar). Se apaga con ODYSSEY3D_LIMPIEZA=0 para comparar en
     el panel la misma red con el mapa en crudo. */
  void setCleaning(bool on) { limpieza_ = on; }
  bool cleaning() const { return limpieza_; }

private:
  void cierra();          /* suelta la sesion de ONNX Runtime */

  bool ready_ = false;
  std::string ruta_;      /* la ruta de la red abierta ahora */
  int sz_ = 518;
  int gpu_ = 0;
  std::string dev_ = "nada";
  double ms_ = 0.0;
  float smooth_ = 0.60f;
  void *impl_ = nullptr;                 /* ocultar ONNX Runtime de este fichero de cabecera */
  std::vector<uint8_t> prev_;            /* mapa anterior, para suavizar */
  float loS_ = 0.0f, hiS_ = 1.0f;        /* escala en el tiempo (min/max suavizados) */
  bool haveScale_ = false;
  float focus_ = 0.5f;                  /* plano de enfoque seguido automaticamente */
  bool corte_ = false;                  /* cambio de plano en la ultima estimacion */
  long cortes_ = 0;
  int cuadrosDesdeCorte_ = 1000;        /* descanso entre reinicios (15 cuadros en el programa de Samsung) */
  std::vector<float> guia_;             /* luminancia de la foto: la que guia la limpieza del mapa */
  bool limpieza_ = true;                /* filtro guiado + dilatar activados */
};

#endif
