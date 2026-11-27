/* panel_main.cpp — `odysseyvk-panel` (v1.16): el panel de ajustes solo, sin las bibliotecas del visor (onnxruntime,
   OpenCV, mpv, ffmpeg...), que el panel no usa y que tardaban ~45 ms en cargarse cada vez que se abria. Hace lo
   mismo que `odysseyvk ajustes`: los mismos argumentos y el mismo arranque (los datos del nombre viejo, el idioma y
   la sesion de Wayland). Quien abre el panel lo prefiere si esta junto a `odysseyvk` (ver rutaPanelLigero). */
#include "idioma.h"
#include "panel_ajustes.h"
#include "util.h"

int main(int argc, char **argv) {
  migrarDatosDeViejoNombre();
  idioma::cargar();
  ensureWaylandEnv();
  return cmdAjustes(argc - 1, argv + 1);
}
