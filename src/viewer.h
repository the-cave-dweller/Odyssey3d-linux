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
  /* --sin-despejar: no mover las demas salidas cuando el panel pasa a escala 1. Por defecto si se
     mueven, porque al crecer el area logica del panel se monta sobre el monitor de al lado y esa
     franja se ve duplicada en las dos pantallas. */
  bool noLayout = false;
  /* el raton atraviesa la ventana del visor salvo sobre la pastilla: asi se puede dar a pausa en
     el video, avanzar la barra o manejar el navegador sin quitar el 3D. Con --sin-pasar-raton el
     visor vuelve a tragarse todos los clics, que era lo que pasaba siempre. */
  bool noPassthrough = false;
  bool no3d = false;       /* arrancar en pase 2D */
  /* conversor de video normal (2D) a 3D: la red de profundidad dice cuanto desplazar cada ojo.
     vacio = el modelo que esta en ~/.local/share/odysseyvk/models */
  bool conver = false;
  /* `--detectar` (rama experimental): el tipo de video lo decide el visor mirando los primeros segundos
     (normal, lado a lado o arriba y abajo) y luego deja de mirar */
  bool detectar = false;
  std::string modelPath;
  /* la otra red de profundidad, la del boton 1 / 2 de la pastilla (--alterno RUTA) */
  std::string altModel;
  /* (v1.16) la red solo para videos (--modelo-video RUTA): el servicio la manda solo cuando lo de debajo no es un juego */
  std::string modeloVideo;
  int convEach = 3;          /* cada cuantos cuadros se vuelve a calcular la profundidad */
  int convGpu = 0;           /* 0 = primera tarjeta NVIDIA, 1 = la otra (la primera puede estar llena) */
  bool redAuto = false;      /* (v1.18) `--red auto`: la tarjeta de la red se mide (ver EvalTarjeta en vkview.cpp) */
  /* mapa de profundidad fijo para medir en el taller (--mapa-fijo F.pgm): el relieve se prueba con un
     mapa que ya conocemos, sin red y sin sentarse delante del panel. vacio = lo de siempre. */
  std::string mapaFijo;
  /* plano de apoyo forzado (--apoyo 0..1): es exactamente Ctrl+Shift+3/4, pero escrito en la linea de
     mando para poder repetir una medicion igual. -1 = que lo decida la red, que es lo de siempre. */
  double apoyo = -1.0;
  /* "3D Salida" forzado (--relieve 0..100): sin relieve el mapa de profundidad no mueve nada, y para
     medir los dos planos hace falta ponerlo a mano. -1 = lo de siempre (0 sin conversor, 50 con el). */
  int relieve = -1;
  /* «No volver a preguntar» dentro del video (--recordar APP:MODO, y --recordado si ya esta
     apuntada): lo pasa el servicio, que es quien guarda la eleccion por aplicacion. Con esto la
     pastilla ensena la casilla; vacio = no hay casilla (visor lanzado a mano). */
  std::string recordarApp, recordarModo;
  bool recordado = false;
  /* Video dividido suavizado (--suave, v0.83): la ventana trae dos imagenes lado a lado, pero en vez
     de tejerlas tal cual se toma solo la izquierda y el relieve lo pone el conversor de video
     normal. Es para los videos cuyo relieve propio se sale de lo que aguanta este panel. */
  bool suave = false;
  /* Que hace el boton de «video dividido» de la pastilla (v0.84): de normal lo pone SUAVIZADO; con
     `--sin-suavizado` (suavizado = no en los ajustes) lo teje tal cual, con el relieve que trae. */
  bool suavizado = true;
  /* `--suavizado-auto` (dividido_suavizado = auto, v0.88): el video dividido se teje con su propio
     relieve mientras quepa en lo que aguanta el panel, y se pasa solo al suavizado mientras algo se
     salga (lo dice el medidor de separacion). */
  bool suavizadoAuto = false;
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
  double filtro = 10.0;    /* pendiente del filtro de la trama (weave.h): la de Samsung */
  /* Dureza con la que el mapa de profundidad se aplasta hacia los dos unicos valores que hay en el
     cuadro (fondo y primeros planos): la imagen queda en dos laminas con su propio desplazamiento,
     como en el Nintendo 3DS. 0 = el relieve continuo de siempre, y es lo que arranca. */
  double planos = 0.0;
  /* Muestras intermedias entre las dos laminas (--muestras 1): el tramo del centro deja de ser una
     rampa y recibe un nivel propio, que se ve solido en vez de degradado. 0 = como siempre. */
  int muestras = 0;
  /* rastreo de cabeza con la camara infrarroja del monitor (necesita OpenCV) */
  bool track = false;
  std::string trackDevice, trackModel, trackCalibDir;
  double trackOffsetX = 0.0, trackOffsetY = 0.0, trackOffsetZ = 0.0;  /* cm */
  bool trackFlipX = false, trackFlipY = false;
  bool trackFixed = false;   /* ignora lo que mide la camara (para comparar) */
  float trackScore = 0.6f;
  double trackSmoothing = 0.35;
  /* respaldo con cualquier webcam: -1 = mirar el dispositivo y decidir, 0 = forzar
     las dos vistas de la camara del monitor, 1 = forzar la via de una sola vista */
  int trackMono = -1;
  double trackCampo = 65.0;  /* campo horizontal de la webcam, en grados */
  double trackCara = 140.0;  /* ancho de cara de oreja a oreja, en milimetros (Samsung usa 140) */

  /* Modo de legado (--legado, `legado = si` en serve.conf): la forma de siempre de ver un video que
     YA viene en dos vistas. Sin conversor 2D a 3D, sin los numeros finos de la trama (se quedan en
     los de fabrica), con la camara del monitor y nada de webcam, y con la pastilla reducida a lo que
     ese camino puede tocar: el 3D, la salida y el plano de apoyo. Si la camara no ve la cara, la
     imagen vuelve a 2D y la lente se apaga, en vez de seguir tejiendo con una pose prestada. */
  bool legado = false;

  /* rotulos y subtitulos: 1 los apoya en el plano de la pantalla (asi no se deslizan con la
     cabeza), 0 los deja con la profundidad que les da la red, -1 = apoyados (lo de fabrica) */
  int textoPanel = -1;

  std::string corrA, corrB, corrDir;
  std::string file;
  std::string captureUuid;   /* ventana a capturar (SourceKind::Capture) */
  std::string outPath;     /* salida de `weave` */
  std::string dumpPath;    /* guardar fotogramas PNG (diagnostico) y salir */
  bool dumpKeep = false;   /* ... pero seguir: el hub lo usa cuando `volcado` esta en serve.conf */
  int segundos = 0;      /* salir solos tras N segundos (por secuencias y pruebas) */
};

enum class SourceKind { Test, Mpv, Capture };

int runViewer(const ViewerOpts &opts, SourceKind kind);      /* camino OpenGL  */
int runViewerVk(const ViewerOpts &opts, SourceKind kind);    /* camino Vulkan  */

#endif
