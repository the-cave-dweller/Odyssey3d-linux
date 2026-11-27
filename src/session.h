/* session.h — lo que comparten el visor OpenGL y el visor Vulkan: la puesta en marcha del
   hardware del monitor (lente, scaler, modo del panel), la elección de pantalla y las teclas de
   ajuste de la trama. Está aquí para que ambos caminos usen exactamente los mismos números. */
#ifndef ODYSSEY_SESSION_H
#define ODYSSEY_SESSION_H

#include "lens.h"
#include "panel.h"
#include "scaler.h"
#include "viewer.h"
#include "weave.h"

#include <GLFW/glfw3.h>

#include <cstdint>
#include <string>
#include <thread>
#include <vector>

/* Lente + scaler + modo del panel. bringUp() guarda el estado previo y shutDown() lo devuelve. */
struct Rig {
  Lens lens;
  Scaler scaler;
  PanelState saved{};
  bool panelApplied = false;
  bool panelSinGeometria = false;   /* el 3D no cambio modo, escala ni sitios: al salir tampoco se tocan (v0.76) */
  bool scalerOn = false;
  bool lensOk = false;
  /* Sitios de origen de las demas salidas que movimos al agrandar el panel: shutDown() las devuelve. */
  std::vector<Placement> otras;
  int hud() const { return (lensOk ? 1 : 0) + (scalerOn ? 2 : 0); }
  std::string output;

  /* bringUp() pone el panel y deja la lente y el scaler en un hilo aparte (v1.12: 215 + 180 ms que
     corren mientras se crean la ventana, Vulkan y la red); esperar() lo junta. shutDown() y el
     destructor esperan solos. */
  void bringUp(const ViewerOpts &o);
  bool esperar();   /* falso si se envio la orden al scaler y no se confirmo */
  void shutDown();
  Rig() = default;
  Rig(const Rig &) = delete;
  Rig &operator=(const Rig &) = delete;
  ~Rig() { esperar(); }

private:
  bool scalerIncierto_ = false;
  std::thread hilo_;
};

/* Elige la salida del panel por nombre (DP-4) o buscando el modo 4K. */
GLFWmonitor *pickMonitor(const std::string &output);
/* Frecuencia del modo más cercano a wantHz. */
int refreshOf(GLFWmonitor *mon, double wantHz);

/* Teclas de ajuste de la trama (los dos visores responden igual). Devuelve la etiqueta de la
   acción en *tag si la tecla se consumió. `mods` son los modificadores de GLFW: con Mayus las
   rampas van a paso fino (v0.59); el visor GL no los pasa y se queda con el paso grueso. */
bool tuningKey(WeaveTuning &t, int key, const char **tag, int mods = 0);

/* Patrón de prueba side-by-side en RGBA por CPU (fila de arriba primero). Cada ojo ve el mismo
   dibujo desplazado `parallax` píxeles: con la lente puesta eso sí da profundidad real. */
void makeTestSbs(std::vector<uint8_t> &rgba, int w, int h, double t, double parallax);

#endif
