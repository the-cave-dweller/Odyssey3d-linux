/* depth.h — calcular la profundidad de un cuadro normal (2D) para poder darle relieve en el
   panel del Odyssey. Usa un modelo de profundidad (Depth Anything V2 pequeno) con ONNX Runtime:
   si hay tarjeta NVIDIA se usa ella (unos 7 milisegundos por cuadro); si no, la CPU. */
#ifndef ODYSSEY_DEPTH_H
#define ODYSSEY_DEPTH_H

#include <cstdint>
#include <atomic>
#include <mutex>
#include <cstdlib>
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
/* Memoria del apoyo entre un mapa y el siguiente (v0.74). Sin ella cada mapa decide de cero, celda
   por celda y con umbrales duros, y sobre una textura que se mueve la misma celda sale texto en un
   mapa y no en el siguiente: la zona salta a la pantalla y vuelve varias veces por segundo. Medido
   a 6 mapas por segundo sobre 20 s de video a pulso de un papel impreso: de cada mapa al siguiente
   cambiaba el 2,4 % de la rejilla, con el 4,3 % apoyado. Con memoria hacen falta dos cosas mas:
     - QUIETO: un rotulo es un dibujo pegado a la pantalla y sus trazos no se mueven de un mapa al
       siguiente; el texto y la textura de la escena (un cartel, las ventanas de un edificio) si,
       en cuanto la camara se mueve. La celda solo cuenta si sus trazos estan donde estaban.
     - SEGUIDO: la celda entra cuando lleva dos mapas viendose asi y sale cuando lleva dos sin
       verse (en la banda de abajo, la de los subtitulos, entra al primero).
   Un subtitulo nuevo tarda un mapa mas en apoyarse (0,17 s a 6 por segundo). */
struct MemoriaTexto {
  std::vector<float> lum;         /* la luminancia del mapa anterior, para ver que se quedo quieto */
  int w = 0, h = 0;
  std::vector<float> confianza;   /* 0..1: cuanto lleva viendose cada celda como rotulo */
  std::vector<uint8_t> puesta;    /* la celda esta apoyada */
  std::vector<float> veto;        /* (v1.12) cuanto hace que la celda cayo en un bloque que no es texto (cuerdas) */
  int cw = 0, ch = 0;
  void olvidar() { cw = ch = w = h = 0; veto.clear(); }
};

/* `juego` (v1.13): la ventana es un juego. Ahi el centro de abajo es del personaje en tercera persona
   y los subtitulos son frases: en las bandas solo vale un renglon largo (ver anclarTexto) */
float anclarTexto(const uint8_t *rgba, int w, int h, std::vector<uint8_t> &mapa, float plano,
                  int celdaPx, std::vector<uint8_t> *mascara, int *cw, int *ch,
                  MemoriaTexto *memoria = nullptr, bool juego = false);

/* (v1.13, fase 3c) El HUD del juego al plano de la pantalla, como los rotulos de anclarTexto pero con su
   mascara exacta: el alfa de la imagen donde el juego dibuja el HUD (de la capa, `alfa` de aw x ah, el cuadro
   entero). Se engorda un poco (el HUD se mece y la mascara puede ser de un cuadro antes o despues que la
   imagen capturada) y se suaviza el borde. `mapa` es el de lado x lado (el cuadro aplastado a cuadrado).
   Devuelve la parte del mapa que queda en el plano. */
float aplicarHud(std::vector<uint8_t> &mapa, int lado, float plano, const std::vector<uint8_t> &alfa, int aw, int ah);

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

class ProfundidadJuego;   /* profjuego.h: la profundidad que recibe DLSS, de la capa (v1.13) */

class DepthNet {
public:
  ~DepthNet();
  /* ruta del modelo .onnx; vacio = ~/.local/share/odysseyvk/models/depth_anything_v2_small_fp16.onnx */
  bool init(const std::string &model, std::string *err, int gpu = 0);   /* gpu: tarjeta NVIDIA a usar */

  /* Cambiar de red con el visor en marcha: primero se suelta la sesion vieja (que es lo que
     devuelve la memoria de la grafica) y despues se abre la nueva. Si la nueva no abre, se
     vuelve a abrir la vieja y el conversor sigue como estaba. */
  /* forzar (v1.12): reabrir aunque sea la misma ruta, para pasar de la red al modo sin red y al reves
     (`ODYSSEYVK_SIN_RED` se lee en init) */
  bool cambiar(const std::string &model, std::string *err, bool forzar = false);

  /* Cierta si ONNX Runtime ya revinto una sesion dentro de este proceso (un .onnx que no abrio con
     CUDA). Despues de eso no se puede volver a abrir ninguna red sin arriesgarse a un abort: hay que
     cerrar el visor y abrirlo otra vez. */
  bool quemada() const { return quemada_; }
  const std::string &modelPath() const { return ruta_; }   /* la .onnx que esta abierta ahora */
  bool ready() const { return ready_; }
  /* 0 = la red; 1 = sin red, suelo; 2 = sin red, imagen (rama experimental, modo rendimiento) */
  int sinRed() const { return sinRed_; }
  float mezcla() const { return mezcla_.load(); }
  void setMezcla(float m) { mezcla_.store(m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m)); }
  /* la saliencia elige el sujeto del plano automatico; el Foco automatico de la pastilla la pone en
     marcha (rama experimental). De fabrica, lo que diga ODYSSEYVK_SALIENCIA (si no esta: puesta). */
  bool saliencia() const { return saliencia_.load(); }
  /* los cuantiles 2 y 98 % del ultimo mapa (0..1), para la Salida automatica (rama experimental) */
  float cuantilBajo() const { return cuantilBajo_.load(); }
  float cuantilAlto() const { return cuantilAlto_.load(); }
  /* el plano que la trama usa como pantalla (0..1; el visor lo dice cada cuadro): ahi se dejan las
     bandas negras de las peliculas (rama experimental). Negativo = el automatico. */
  void setPlanoPantalla(float p) { planoPantalla_.store(p); }
  void setSaliencia(bool s) { saliencia_.store(s); }
  /* La vista de depuracion (v1.12, Ctrl+Mayus+9): el peso de la saliencia en cada punto de su rejilla
     (una celda cada 8 puntos del mapa, lado x lado, 255 = el punto que mas pesa en esta estimacion,
     0 = nada o banda negra). La deja `estimate` y se lee desde el mismo hilo, justo despues; vacia
     sin la saliencia puesta. */
  void rejillaSaliencia(std::vector<uint8_t> &fuera, int &lado) const { fuera = salRejilla_, lado = salLado_; }
  int size() const { return sz_; }                    /* el mapa que sale es de sz_ x sz_ */
  /* la entrada de la red (rama experimental): de fabrica 518 x 518 (la imagen 16:9 aplastada a un
     cuadrado); con ODYSSEYVK_RED_ENTRADA=WxH la red ve el cuadro a esa medida (su proporcion, y mas
     grande si se quiere) y la salida se remuestrea al mapa de sz_. ODYSSEYVK_MAPA cambia sz_, que es
     cosa aparte. */
  int entradaAncho() const { return inW_; }
  int entradaAlto() const { return inH_; }
  const std::string &device() const { return dev_; }  /* "NVIDIA", "AMD" (v1.16) o "CPU" */
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
  /* El plano que se queda en la pantalla. Con `pivote` 0 es el del sujeto; con pivote p queda p de
     camino entre el sujeto y el fondo, asi que el sujeto SALE de la pantalla y el fondo se hunde (v0.66). */
  float focusPlane() const { return focus_ * (1.0f - pivote_); }
  float sujeto() const { return focus_; }
  /* Paso de tiempo entre estimaciones para los suavizados (v0.69). Con <= 0 (lo normal) se mide con el
     reloj; las herramientas que pasan fotos a toda prisa (`odysseyvk mapa`) lo fijan en kPasoRef para
     que una rafaga de cuadros se suavice como en un video y no como si durara milisegundos. */
  static constexpr double kPasoRef = 1.0 / 6.0;
  void setPasoFijo(double s) { pasoFijo_ = s; }
  /* Olvida lo que arrastra de un cuadro al siguiente (mapa anterior, escala, plano de apoyo, laminas):
     el cuadro que venga se trata como el primero. Para medir fotos sueltas con `mapa --sueltas`: sin
     esto, pasar varias fotos sin relacion en una sola llamada mezcla cada mapa con el anterior (el
     suavizado en el tiempo) y las medidas salen contaminadas (paso el 25-nov, ver CAMBIOS v0.66). */
  void olvidar() {
    tUltima_ = -1.0;
    prev_.clear();
    haveScale_ = false;
    focoListo_ = false;
    cuadrosDesdeCorte_ = 1000;
    pFondo_ = pFrente_ = -1.0f;
  }
  bool lastCut() const { return corte_; }    /* hubo cambio de plano en la ultima estimacion */
  long cutCount() const { return cortes_; }  /* cambios de plano contados desde el arranque */

  /* limpieza del mapa (filtro guiado + dilatar). Se apaga con ODYSSEYVK_LIMPIEZA=0 para comparar en
     el panel la misma red con el mapa en crudo. */
  void setCleaning(bool on) { limpieza_ = on; }
  bool cleaning() const { return limpieza_; }
  /* radio del dilatado de lo cercano (0 = sin dilatar). Ver `dilatar` en depth.cpp. */
  void setDilatar(int r) { dilata_ = r < 0 ? 0 : (r > 6 ? 6 : r); }
  int radioDilatar() const { return dilata_; }

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

  /* (v1.13, fase 2) La profundidad del juego, la que recibe DLSS, en lugar de la red: la escribe la
     capa de OdysseyVK dentro del juego (`ODYSSEYVK=1`, capa/) en una memoria compartida. El visor la
     permite cuando la ventana del 3D es un juego; si no llega fresca, se usa la red como siempre.
     ODYSSEYVK_PROFUNDIDAD_JUEGO=0 la apaga. */
  void setPermitirJuego(bool p) { permitirJuego_.store(p); }
  bool usandoJuego() const { return usandoJuego_.load(); }
  /* (v1.13, fase 3c) la mascara del HUD del juego, de la capa (ver ProfundidadJuego::leerHud); en el hilo del
     mapa, despues de estimate. `fuenteW/H`: el tamano de la imagen del HUD del juego, para el registro */
  bool mascaraHud(std::vector<uint8_t> &alfa, int &w, int &h, int *fuenteW = nullptr, int *fuenteH = nullptr);
  /* (v1.13) para el cuadrito de la pastilla y la ventana de ajustes, desde el hilo que dibuja: los FPS del juego
     y de la capa (0 = sin profundidad del juego ahora), lo que se lee («1920x1080 D32_SFLOAT, Z invertida») y la
     parte del mapa que el HUD del juego deja en el plano (< 0 = sin mascara) */
  float fpsJuego() const { return fpsJuego_.load(); }
  float fpsCapa() const { return fpsCapa_.load(); }
  std::string descJuego() const {
    std::lock_guard<std::mutex> g(infoMx_);
    return descJuego_;
  }
  float hudFrac() const { return hudFrac_.load(); }
  void setHudFrac(float f) { hudFrac_.store(f); }

private:
  void cierra();          /* suelta la sesion de ONNX Runtime */

  bool ready_ = false;
  std::string ruta_;      /* la ruta de la red abierta ahora */
  int sz_ = 518;
  int inW_ = 518, inH_ = 518;           /* la entrada de la red (ver entradaAncho) */
  /* Sin red (rama experimental, modo rendimiento): ODYSSEYVK_SIN_RED=suelo|imagen. La profundidad sale
     de la propia imagen en la CPU (la altura; con «imagen», tambien la nitidez, el color y el centro)
     y pasa por todo lo demas igual que la de la red. 0 = la red, 1 = suelo, 2 = imagen. */
  int sinRed_ = 0;
  /* la mezcla sin red: 0 = solo la altura (suelo), 1 = solo las pistas de la imagen (nitidez, color,
     centro); ODYSSEYVK_SIN_RED_MEZCLA. Sin ella: suelo = 0, imagen = 0,35 (lo mas perceptible). */
  std::atomic<float> mezcla_{0.35f};   /* se cambia en marcha (los preajustes de la pastilla) */
  std::atomic<float> cuantilBajo_{0.0f}, cuantilAlto_{1.0f};
  std::atomic<float> planoPantalla_{-1.0f};
  /* las bandas negras, en puntos del mapa desde cada borde (arriba, abajo, izquierda, derecha), y el
     candidato que tiene que aguantar para crecer */
  int bArr_ = 0, bAb_ = 0, bIz_ = 0, bDe_ = 0;
  int bCand_[4] = {0, 0, 0, 0}, bCuenta_[4] = {0, 0, 0, 0};
  bool bVistas_ = false;   /* ya hubo un cuadro valido (el primero las toma sin esperar) */
  void detectarBandas();
  bool enBanda(int x, int y) const { return y < bArr_ || y >= sz_ - bAb_ || x < bIz_ || x >= sz_ - bDe_; }
  ProfundidadJuego *juego_ = nullptr;   /* se crea al primer uso y se borra en ~DepthNet (depth.cpp) */
  std::atomic<bool> permitirJuego_{false}, usandoJuego_{false};
  std::atomic<float> fpsJuego_{0.0f}, fpsCapa_{0.0f}, hudFrac_{-1.0f};
  mutable std::mutex infoMx_;
  std::string descJuego_;
  std::atomic<bool> saliencia_{[] {
    const char *e = std::getenv("ODYSSEYVK_SALIENCIA");
    if (!e) e = std::getenv("ODYSSEY3D_SALIENCIA");
    return !e || !(e[0] == '0' || e[0] == 'n');
  }()};
  void profundidadSinRed(std::vector<float> &d);
  bool topeArrastre_ = false;           /* rama experimental: tope de 1/16 al arrastre del suavizado */
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
  bool focoListo_ = false;              /* ya hubo una estimacion: el plano se sigue, no se salta */
  float pivote_ = 0.0f;                 /* cuanto se retrasa el plano de la pantalla respecto al sujeto (0..0,9) */
  double tUltima_ = -1.0;               /* reloj de la estimacion anterior, en segundos (< 0 = no hubo) */
  double pasoFijo_ = 0.0;               /* ver setPasoFijo */
  bool corte_ = false;                  /* cambio de plano en la ultima estimacion */
  long cortes_ = 0;
  int cuadrosDesdeCorte_ = 1000;        /* descanso entre reinicios (15 cuadros en el programa de Samsung) */
  std::vector<float> guia_;             /* luminancia de la foto: la que guia la limpieza del mapa */
  std::vector<float> guiaAntes_;        /* la de la estimacion anterior (el tope de arrastre mira que se movio) */
  std::vector<float> opRG_, opBY_;      /* color oponente (R-G, B-Y) para la saliencia (v0.65) */
  std::vector<uint8_t> salRejilla_;     /* ver rejillaSaliencia */
  int salLado_ = 0;
  bool limpieza_ = true;                /* filtro guiado + dilatar activados */
  int dilata_ = 3;                      /* radio del dilatado de lo cercano, en puntos del mapa */
  bool quemada_ = false;                /* ONNX Runtime ya reviento aqui: no intentar otra sesion */
  float dureza_ = 0.0f;                 /* fuerza de la separacion en dos planos (0 = apagado) */
  int muestras_ = 0;                    /* muestras intermedias entre las laminas (0 = rampa) */
  float pFondo_ = -1.0f, pFrente_ = -1.0f;   /* posiciones de las dos laminas, seguidas en el tiempo */
  float fracFrente_ = 0.0f;             /* parte del cuadro en la lamina delantera */
};

#endif
