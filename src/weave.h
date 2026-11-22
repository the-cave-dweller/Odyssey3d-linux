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
  float filterSlope = 10.0f;   /* pendiente del filtro triangular entre vistas */
  float xtalkFac = 0.0128535f; /* crosstalk estático (pre-distorsión) */
  float xtalkDyn = 0.0f;       /* crosstalk dependiente de la posición de la pupila */
  float contrast = 1.0f;
  float corrAScale = 1.0f;
  float conv = 0.0f;           /* convergencia: desplazamiento horizontal de una vista */
  float align = 0.0f;          /* desfase de fase global (ciclos) para alinear con la lente física */
  float pitch = 1.0f;          /* escala de la rampa de fase (pitch efectivo de la lente) */
  bool weave = true;
  bool topBottom = false;      /* video arriba/abajo en lugar de lado a lado */
  bool swapLR = false;         /* profundidad invertida -> alternar */
};

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
  struct CornerAttrs {
    float v2[2];
    float v3[2];
    float v4[3];
  };

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
      uPitch_ = -1, uWeave_ = -1, uSwap_ = -1, uHud_ = -1, uLayout_ = -1, uFlipY_ = -1, uCorrA_ = -1, uCorrB_ = -1, uSrc_ = -1;
  int W_ = 3840, H_ = 2160, hud_ = 0, srcFlipY_ = 1;
  bool corrReal_ = false;
  EyePose eye_{};
  WeaveTuning tune_{};
  LensField field_{};
};

#endif
