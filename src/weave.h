/* weave.h — la trama ("weave") real del panel: genera por columna la vista que le toca a cada ojo.
   Es el mismo modelo que usa el firmware del Odyssey 3D (DimencoWeaving): por cada esquina de la
   pantalla se calculan v2 (coeficientes de fase), v3 (tangente de dirección ojo-pantalla) y v4 (rampa
   de fase por canal) y el interpolador del GPU reconstruye el campo, que es afín. */
#ifndef ODYSSEY_WEAVE_H
#define ODYSSEY_WEAVE_H

#include "glutil.h"

#include <string>

struct EyePose {
  double x = 0.0;        /* cm, marco de pantalla (0 = centro horizontal) */
  double y = 8.3916;     /* cm hacia abajo desde el centro de la pantalla */
  double z = 65.1113;    /* cm de distancia a la pantalla */
};

struct WeaveTuning {
  /* pendiente del filtro triangular entre vistas: el paso de una vista a la otra ocupa 1/pendiente del
     ciclo. La de Samsung es 10 (casi un escalon). El 26-nov se probo 4 de fabrica para que al mover la
     cabeza la imagen del otro ojo entrara poco a poco, y Rodolfo vio las cuerdas de una guitarra
     dobles («duplica cuerdas»); probando en marcha entre el 10 y el 100 % se quedo con el 10. En el panel:
     «Suavizar entre las vistas» (`filtro` en serve.conf). */
  float filterSlope = 10.0f;
  float xtalkFac = 0.0128535f; /* crosstalk estático (pre-distorsión) */
  float xtalkDyn = 0.0f;       /* crosstalk dependiente de la posición de la pupila */
  float contrast = 1.0f;
  float corrAScale = 1.0f;
  float conv = 0.0f;           /* convergencia: desplazamiento horizontal de una vista */
  float align = 0.0f;          /* desfase de fase global (ciclos) para alinear con la lente física */
  float pitch = 1.0f;          /* escala de la rampa de fase (pitch efectivo de la lente) */
  /* Rampa extra de fase, en ciclos por pixel de panel, que SUMA a la de la configuracion
     (`rampa_x` / `rampa_y` en serve.conf). Mueve solo el paso de la rejilla escrita, sin tocar
     los desfases entre canales: si el paso escrito no coincide con el de la lente del panel, las
     dos tramas baten y salen franjas gruesas por toda la imagen. `pitch` no sirve para eso porque
     escala la rampa de la lente entera y afloja la inclinacion (medido, v0.42).
     Teclas: `[` y `]` en x, `;` y `'` en y; `8` las pone a cero. */
  float rampaDX = 0.0f;
  float rampaDY = 0.0f;
  /* Capas por profundidad: cuanto se estira el relieve a CADA LADO del plano de apoyo.
     `ganFondo` manda sobre lo que queda detras de la pantalla (lo empuja hacia dentro) y
     `ganFrente` sobre lo que sale fuera de ella. Los dos en 1 es el comportamiento de siempre;
     subiendo `ganFondo` se atrasa el fondo sin tocar los primeros planos, que es lo que hace
     el conversor de Windows con las bandas de `scene_dspr` (near_diff_scale -0,5 ·
     far_diff_scale 1,0) y con los signos de `dspr_scale(-)` / `dspr_scale(+)`.
     El tope oficial por ojo (+12,67 puntos hacia fuera y 9,98 hacia dentro sobre 3840) se
     aplica DESPUES de este escalado, asi que por mucho que se suba no se sale del presupuesto
     de Samsung. Teclas: `j`/`k` el fondo, `u`/`i` el frente, de 0,05 en 0,05. */
  float ganFondo = 1.0f;
  float ganFrente = 1.0f;
  /* Empuje por capa: ademas de estirar, cada banda puede recibir un desplazamiento FIJO, en
     puntos de panel, que se suma al que ya le toca por su profundidad. Sirve para llevar una
     banda entera a un plano concreto (subtitulos, ventanas, el fondo del plano) sin depender de
     donde caiga el plano de apoyo. Es el `far_diff_popup -1,0` de las bandas de `scene_dspr` y el
     `depth_offset` de la capa `overlay` de `degraph` en la config de Windows. Positivo = hacia
     fuera de la pantalla. 0 es como estaba. Teclas: `h`/`l` el fondo y `y`/`o` el frente, de 0,5
     puntos en 0,5. */
  float empFondo = 0.0f;
  float empFrente = 0.0f;
  /* Dos planos (paralaje por laminas, como en el Nintendo 3DS). Cuanto se aplasta el mapa de
     profundidad hacia los DOS unicos valores que `DepthNet` encuentra en el cuadro: 0 deja el
     relieve continuo de siempre (lo que arranca el programa y lo que estaba hasta ahora) y 1 deja
     dos laminas planas, cada una con su desplazamiento, que luego se colocan con `j`/`k` (atras),
     `u`/`i` (delante), `h`/`l` y `y`/`o` (empuje fijo de cada lamina). No es un numero de la trama:
     se aplica en el mapa, en la CPU, y por eso no entra en el bloque de la trama ni gasta ancho de
     banda. Teclas: `g` sube la dureza y `v` la baja, de 0,1 en 0,1. */
  float planos = 0.0f;
  /* Muestras intermedias entre las dos laminas (v0.50). Con las dos laminas y dureza baja el tramo
     del centro es una rampa: se nota el degradado entre una capa y la otra. Cada muestra agrega un
     nivel a mitad de camino con su propia meseta, asi el centro se ve solido y el paso entre niveles
     sigue siendo suave (por eso el canto no se corta ni parpadea). 0 = la rampa continua de siempre,
     1 = atras / medio / delante, hasta 3. Tampoco es un numero de la trama: vive en el mapa.
     Teclas: `n` agrega una muestra y `m` la quita. */
  int muestras = 0;
  bool capaTocada = false;   /* al menos una tecla de capas pulsada: manda lo del visor */
  bool weave = true;
  bool topBottom = false;      /* video arriba/abajo en lugar de lado a lado */
  bool swapLR = false;         /* profundidad invertida -> alternar */
  /* Patron de un solo ojo (v0.59): 0 = imagen normal, 1 = vista izquierda blanca y derecha
     negra, 2 = al reves. Sirve para medir el paso contra la LENTE, no contra una foto de Windows:
     con un ojo cerrado, si el paso escrito es el de la lente el ojo abierto ve la pantalla entera
     de un solo tono; si no, ve bandas diagonales blanco/negro, y cuanto mas anchas, mas cerca.
     Tecla `b` (da la vuelta 0 -> 1 -> 2 -> 0); `8` lo apaga. Solo en el visor Vulkan. */
  int unOjo = 0;
};

/* Esquina del campo: lo mismo que Weave::fill pero sin tocar el contexto GL, para que
   el trazador Vulkan use exactamente los mismos números. */
struct CornerAttrs {
  float v2[2];
  float v3[2];
  float v4[3];
};

struct LensField;
CornerAttrs lensCorner(const LensField &f, const EyePose &e, int W, int H, double ndcX, double ndcY);

/* Modelo analítico del campo de la lente. Valores capturados de un G90XF (ver README). */
struct LensField {
  double kx = 0.5539365133;      /* ciclos de fase por píxel en x */
  double ky = 0.1600504557;      /* ciclos de fase por píxel en y (pendiente de la lente) */
  double c00[3] = {0.23046875, 0.4150390625, 0.599609375}; /* fase en la esquina superior izquierda */
  double pxPcm = 0.01554;        /* paso de píxel, cm */
  double slant = 0.28893;        /* pendiente lenticular */
  double ezEffRef = 86.6526;     /* profundidad efectiva de la pila a la distancia nominal */
  double optZ = 65.11129;        /* distancia de trabajo nominal, cm */
  double fxOverFy = 5.23903;     /* razón altura_pila/paso_lente (constante geométrica) */
};

class Weave {
public:
  bool init(std::string *err);
  void destroy();

  void setPanelSize(int w, int h);
  void setEye(const EyePose &e) { eye_ = e; }
  const EyePose &eye() const { return eye_; }
  void setTuning(const WeaveTuning &t) { tune_ = t; }
  /* 3 = lente y scaler listos · 1 o 2 = solo uno · 0 = ninguno (indicador en la esquina). */
  void setHud(int st) { hud_ = st; }
  /* v=0 arriba (0) o v=1 arriba (1, lo que produce mpv con FLIP_Y=1 y el patron de prueba). */
  void setSrcFlipY(int f) { srcFlipY_ = f; }
  const WeaveTuning &tuning() const { return tune_; }
  void setField(const LensField &f) { field_ = f; }
  const LensField &field() const { return field_; }

  /* Mapas de corrección del panel (grises). Sin ficheros se usan neutros (A=0, B=128). */
  void setCorrectionsFromPng(const std::string &a, const std::string &b);
  bool correctionsLoaded() const { return corrReal_; }

  /* Calcula los atributos de las 4 esquinas para el ojo actual y sube el VBO. */
  void refreshCorners();
  CornerAttrs fill(double ndcX, double ndcY) const;

  /* Dibuja la trama usando una textura SBS (mitad izquierda = vista izquierda del fichero). */
  void draw(GLuint srcTex);

private:
  GLuint prog_ = 0, vao_ = 0, vbo_ = 0, corrA_ = 0, corrB_ = 0;
  GLint uRes_ = -1, uFS_ = -1, uXTF_ = -1, uXTD_ = -1, uContrast_ = -1, uCAS_ = -1, uConv_ = -1, uAlign_ = -1,
      uPitch_ = -1, uRampaX_ = -1, uRampaY_ = -1, uWeave_ = -1, uSwap_ = -1, uHud_ = -1, uLayout_ = -1, uFlipY_ = -1, uCorrA_ = -1, uCorrB_ = -1, uSrc_ = -1;
  int W_ = 3840, H_ = 2160, hud_ = 0, srcFlipY_ = 1;
  bool corrReal_ = false;
  EyePose eye_{};
  WeaveTuning tune_{};
  LensField field_{};
};

#endif
