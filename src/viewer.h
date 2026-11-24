/* viewer.h — ventana GL, puesta en marcha del hardware y bucle de trazado. */
#ifndef ODYSSEY_VIEWER_H
#define ODYSSEY_VIEWER_H

#include "weave.h"

#include <string>

struct ViewerOpts {
  std::string output;      /* nombre de la salida del panel (DP-4); vacío = autodetectar */
  double hz = 120.0;       /* frecuencia objetivo para el modo del panel */
  int w = 0, h = 0;        /* 0 = modo nativo del panel */
  bool windowed = false;
  /* ventana sin bordes y maximizada en vez de pantalla completa: cubre la salida entera
     sin entrar en el estado "fullscreen" del compositor (así el vídeo de abajo no deja de
     repintarse). */
  bool borderless = false;
  int posX = 0, posY = 0;   /* colocacion de la ventana (con --sin-borde) */
  bool alwaysRender = false;  /* --siempre: presentar a ritmo de panel aunque no llegue nada nuevo */
  bool noLens = false;
  bool noScaler = false;
  bool noPanel = false;
  /* el raton atraviesa la ventana del visor salvo sobre la pastilla: asi se puede dar a pausa en
     el video, avanzar la barra o manejar el navegador sin quitar el 3D. Con --sin-pasar-raton el
     visor vuelve a tragarse todos los clics, que era lo que pasaba siempre. */
  bool noPassthrough = false;
  bool no3d = false;       /* arrancar en pase 2D */
  /* conversor de video normal (2D) a 3D: la red de profundidad dice cuanto desplazar cada ojo.
     vacio = el modelo que esta en ~/.local/share/odyssey3d/modelos */
  bool conver = false;
  std::string modelPath;
  /* la otra red de profundidad, la del boton 1 / 2 de la pastilla (--alterno RUTA) */
  std::string altModel;
  int convEach = 3;          /* cada cuantos cuadros se vuelve a calcular la profundidad */
  int convGpu = 0;           /* 0 = primera tarjeta NVIDIA, 1 = la otra (la primera puede estar llena) */
  bool noCorr = false;     /* sin mapas de correccion del panel */
  bool solid = false;      /* fuente plana (para medir la trama) */
  bool topBottom = false;  /* video arriba/abajo */
  bool fmtGiven = false;   /* el formato ya lo eligio el hub: no hace falta volver a preguntar */
  float uiScale = 0.0f;    /* tamano de la pastilla a mano (0 = el automatico del panel) */
  bool noHud = false;
  bool cpuPattern = false;   /* patron de prueba generado en la CPU (para comparar GL y Vulkan) */
  double eyeX = 0.0;       /* cm */
  double eyeY = 8.391599655;  /* cm hacia abajo desde el centro de la pantalla */
  double eyeZ = 65.11128998; /* cm de distancia */
  double align = 0.0;      /* desfase de fase inicial (ciclos) */
  double pitch = 1.0;      /* escala de la rampa de fase */
  /* rastreo de cabeza con la camara infrarroja del monitor (necesita OpenCV) */
  bool track = false;
  std::string trackDevice, trackModel, trackCalibDir;
  double trackOffsetX = 0.0, trackOffsetY = 0.0, trackOffsetZ = 0.0;  /* cm */
  bool trackFlipX = false, trackFlipY = false;
  bool trackFixed = false;   /* ignora lo que mide la camara (para comparar) */
  float trackScore = 0.6f;
  double trackSmoothing = 0.35;

  std::string corrA, corrB, corrDir;
  std::string file;
  std::string captureUuid;   /* ventana a capturar (SourceKind::Capture) */
  std::string outPath;     /* salida de `weave` */
  std::string dumpPath;    /* guardar fotogramas PNG (diagnostico) y salir */
  int segundos = 0;      /* salir solos tras N segundos (por secuencias y pruebas) */
};

enum class SourceKind { Test, Mpv, Capture };

int runViewer(const ViewerOpts &opts, SourceKind kind);      /* camino OpenGL  */
int runViewerVk(const ViewerOpts &opts, SourceKind kind);    /* camino Vulkan  */

#endif
