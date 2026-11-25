/* depth.h — calcular la profundidad de un cuadro normal (2D) para poder darle relieve en el
   panel del Odyssey. Usa un modelo de profundidad (Depth Anything V2 pequeno) con ONNX Runtime:
   si hay tarjeta NVIDIA se usa ella (unos 7 milisegundos por cuadro); si no, la CPU. */
#ifndef ODYSSEY_DEPTH_H
#define ODYSSEY_DEPTH_H

#include <cstdint>
#include <string>
#include <vector>

/* --- rotulos y subtitulos apoyados en el plano de la pantalla --------------------
   Un rotulo al que el conversor le da profundidad se desliza de lado a lado cuando mueves la
   cabeza: es la palanca de la disparidad (un punto a 2 cm del panel barre unos 30 px en la
   pantalla con la cabeza movida 15 cm). Los subtitulos tienen que estar en el plano de la
   pantalla, que es lo que hace el programa de Samsung con esa banda. Esta funcion busca en la
   foto las zonas que son texto y, en esas zonas del mapa de profundidad, pone el valor del plano
   de apoyo: sin desplazamiento, el texto queda soldado a la pantalla y no se mueve.

   La deteccion trabaja a la escala del mapa (518 x 518), en celdas de `celdaPx` puntos, y pide
   las cuatro senales que solo el texto reune a la vez: mucho contraste dentro de la celda, los
   bordes ocupando una parte amplia de la celda (los trazos finos alternan claro y oscuro muchas
   veces), detalle FINO (el segundo orden es una fraccion grande del contraste; una pared de
   ladrillo tiene contraste y densidad, pero el detalle es grueso), y el trazo ocupando una parte
   pequena de la celda porque lo demas es fondo. En la banda de abajo y en la de arriba se bajan
   un poco los umbrales, que es donde viven los subtitulos y los Cintillo.

   `mapa` se modifica en el sitio (w x h, un byte por punto, el mismo tamano que la foto).
   Devuelve la fraccion de celdas tocadas (0..1). Si se pasa `mascara` se rellena la rejilla de
   deteccion, con el peso aplicado en cada celda (0..255), y `cw`/`ch` devuelven su tamano. */
float anclarTexto(const uint8_t *rgba, int w, int h, std::vector<uint8_t> &mapa, float plano,
                  int celdaPx, std::vector<uint8_t> *mascara, int *cw, int *ch);

/* --- los dos planos ------------------------------------------------------------
   El conversor da una profundidad distinta a cada punto, y con eso todo el cuadro se
   trata igual: un sujeto y el fondo detras solo se separan si caen a lados opuestos del
   plano de apoyo. Aqui se busca si el cuadro de verdad tiene DOS grupos (un fondo y unos
   primeros planos) y, si los hay, se les puede dar un valor unico a cada grupo: la imagen
   queda en dos laminas con paralaje propio, que es como lo resuelve el Nintendo 3DS (el
   contenido pone dos capas y la pantalla solo escala, con un presupuesto fijo).

   `buscarPlanos` mira el histograma del propio mapa (0 = el fondo, 1 = lo mas cerca) y
   corta donde mas se separan los dos grupos, que es la prueba de Otsu de toda la vida:
   sin umbrales escritos a mano y sin aprender nada del cuadro. Devuelve valido = false
   cuando el cuadro no tiene dos grupos claros (un plano fijo, un pasillo, niebla), y
   entonces no hay que tocar nada.

   `aplicarPlanos` acerca cada punto al valor de su lamina con la fuerza `dureza` (0 = no
   tocar nada, 1 = laminas planas). En el borde entre las dos no se corta de golpe: se
   cruza en una franja suave, porque un corte duro se ve como un tajo y ademas parpadea.

   Con `muestras` >= 1 el tramo del centro deja de ser una rampa y recibe sus propias
   muestras intermedias (1 = fondo, medio, frente). Cada muestra tiene su meseta, que es lo
   que se ve solido, y el paso entre muestras sigue siendo un suave angosto, que es lo que
   mantiene el canto sin tajo y sin parpadeo. Con 0 es la rampa continua de siempre. */
struct Planos {
  bool valido = false;
  float fondo = 0.0f;      /* valor del plano de atras (0 fondo .. 1 cerca de la pantalla) */
  float frente = 0.0f;     /* valor del plano de delante */
  float fraccion = 0.0f;   /* parte del cuadro que cae en el plano delantero */
};

Planos buscarPlanos(const float *mapa, int n);
float aplicarPlanos(float *mapa, int n, const Planos &p, float dureza, int muestras = 0);

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
  double lastMsRed() const { return msRed_; }          /* solo la pasada de la red */
  double lastMsPost() const { return msPost_; }        /* limpiar, dilatar, apoyo, subir la textura */

  /* rgba con la primera fila arriba, w x h. Escribe un byte por punto, del tamano del modelo
     (255 = lo mas cerca de la pantalla, 0 = el fondo). */
  bool estimate(const uint8_t *rgba, int w, int h, std::vector<uint8_t> &out, std::string *err);

  /* suavizado en el tiempo: 0 deja cada cuadro tal cual, 0.7 deja el mapa casi quieto.
     Sirve para que el relieve no parpadee cuadro a cuadro. */
  void setSmoothing(float a) { smooth_ = a < 0.0f ? 0.0f : (a > 0.95f ? 0.95f : a); }
  float smoothing() const { return smooth_; }

  /* Olvidar el mapa anterior y la escala seguida. Hace falta cuando cambia algo que entra dentro
     del mapa (por ejemplo al quitar el apoyo de los rotulos), para que el cuadro nuevo no salga
     mezclado con el mapa viejo. */
  void reset() { prev_.clear(); haveScale_ = false; }

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

  /* Los dos planos (ver Planos mas arriba). La `dureza` va de 0 a 1: 0 deja el relieve continuo
     de siempre (y no cuesta nada porque el codigo se salta entero), 1 deja cada lamina plana. Las
     posiciones de las dos laminas se siguen en el tiempo para que no bailen cuadro a cuadro, y de
     golpe en cada cambio de plano. */
  void setPlanos(float dureza) {
    /* al reves de lo normal: se compara contra 0 para que un numero que no es numero (NaN) caiga
       directo en apagado en vez de colarse dentro del mapa */
    dureza_ = dureza > 0.0f ? (dureza > 1.0f ? 1.0f : dureza) : 0.0f;
  }
  float planosDureza() const { return dureza_; }
  /* Muestras intermedias entre las dos laminas (0 = rampa continua, 1 = fondo/medio/frente).
     No cambia donde estan las laminas: solo como se cruza el tramo del centro. */
  void setMuestras(int n) { muestras_ = n >= 1 ? (n > 3 ? 3 : n) : 0; }
  int muestras() const { return muestras_; }
  float planoAtras() const { return pFondo_; }    /* -1 = todavia no se han visto dos planos */
  float planoDelante() const { return pFrente_; }
  float planoFraccion() const { return fracFrente_; }   /* parte del cuadro en la lamina delantera */

private:
  void cierra();          /* suelta la sesion de ONNX Runtime */

  bool ready_ = false;
  std::string ruta_;      /* la ruta de la red abierta ahora */
  int sz_ = 518;
  int gpu_ = 0;
  std::string dev_ = "nada";
  double ms_ = 0.0;
  double msRed_ = 0.0, msPost_ = 0.0;   /* reparto del tiempo: red por un lado, lo demas por otro */
  double msIir_ = 0.0, msFilt_ = 0.0, msPlano_ = 0.0, msSubida_ = 0.0;
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
  bool quemada_ = false;                /* ONNX Runtime ya reviento aqui: no intentar otra sesion */
  float dureza_ = 0.0f;                 /* fuerza de la separacion en dos planos (0 = apagado) */
  int muestras_ = 0;                    /* muestras intermedias entre las laminas (0 = rampa) */
  float pFondo_ = -1.0f, pFrente_ = -1.0f;   /* posiciones de las dos laminas, seguidas en el tiempo */
  float fracFrente_ = 0.0f;             /* parte del cuadro en la lamina delantera */
};

#endif
