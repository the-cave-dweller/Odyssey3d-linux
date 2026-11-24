/* vkui.h — la interfaz del hub: la pastilla gris de arriba a la derecha con el botón de
   3D, el de superposición y los dos controles (profundidad y salida), más el aviso abajo a
   la derecha que pregunta cómo ver la ventana a pantalla completa. Todo se dibuja con
   cuadros sobre Vulkan (atlas R8 con fuentes y máscaras) y se maneja con el ratón y los
   atajos de la aplicación oficial. No toca Vulkan: sólo geometría, texto y entradas. */
#ifndef ODYSSEY_VKUI_H
#define ODYSSEY_VKUI_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct HudVertex {
  float x, y;   /* píxeles, (0,0) arriba a la izquierda */
  float u, v;   /* coordenada dentro del atlas */
  float r, g, b, a;
};

class Hud {
public:
  struct State {
    bool overlayOn = true;
    bool weaveOn = true;
    int depth = 50;    /* "3D Depth" oficial: 0..100 · 50 = sin cambio */
    int popout = 0;    /* "3D Popout" oficial: -50..+50 */
    bool ask = false;  /* aviso de conversión (abajo a la derecha) */
    bool remember = false;
    int hud = 0;       /* 3 = lente y scaler listos · 0 = ninguno */
    int red = 2;       /* que red de profundidad esta puesta: 1 = la de Samsung, 2 = la nuestra */
    bool ratonLibre = false;  /* el raton atraviesa la ventana fuera de la pastilla */
    bool redViva = false;  /* hay conversor abierto (si no, el boton de la red se apaga) */
  };

  enum Hit {
    HitNone = 0,
    Hit3D,
    HitOverlay,
    HitDepth,
    HitPopout,
    HitRed,
    HitAskSbs,
    HitAskTop,
    HitAskFlat,
    HitAskConv,
    HitAskDont,
    HitAskClose
  };

  /* ---- atlas (se construye una vez en CPU y se sube como R8) ---- */
  bool buildAtlas(std::string *err);
  const std::vector<uint8_t> &atlasPixels() const { return atlas_; }
  int atlasW() const { return aw_; }
  int atlasH() const { return ah_; }
  bool hasText() const { return hasText_; }

  /* ---- estado visible ---- */
  State &state() { return st_; }
  const State &state() const { return st_; }
  void setInfo(const std::string &s) { info_ = s; }
  /* texto del aviso: por defecto el de la aplicacion oficial; el servicio pone aqui
     el titulo de la pelicula que esta dando pantalla completa */
  void setAskText(const std::string &titulo, const std::string &cuerpo) {
    askTitulo_ = titulo;
    askCuerpo_ = cuerpo;
  }

  /* ---- cuadro de la superposición y pruebas de golpe de ratón ---- */
  /* altoLogico: alto de la pantalla en puntos logicos (los que ve el escritorio). Lo usa la
     tarjeta del hub, que vive en el escritorio: con el panel al 150% son 1440 y no los 2160
     fisicos. El visor NO lo pasa: cuando el panel entra en el modo 3D se queda con escala 1 y
     alli la pastilla se dibuja en pixeles fisicos, asi que mide lo mismo con el panel en 2 o en 3.
     setScale deja el tamano fijo a mano (Ctrl+Shift+7 / 8 en el visor, --ui-escala al arrancar). */
  void begin(int w, int h, float altoLogico = 0.0f);  /* limpia y calcula la disposición */
  void setScale(float s) { scUser_ = s; }             /* 0 = tamano automatico */
  float scale() const { return sc_; }
  const std::vector<HudVertex> &vertices() const { return v_; }
  int vertexCount() const { return (int)v_.size(); }
  Hit hit(float mx, float my) const;
  void dragTo(Hit which, float mx);    /* arrastre sobre un control deslizante */
  void toggle3D();
  void toggleOverlay();
  /* la pastilla a la vista sin más (PB, o el raton en el rincon de arriba a la derecha) */
  void showOverlay() { st_.overlayOn = true; }
  void nudgeDepth(int delta);
  void nudgePopout(int delta);
  /* el raton llega en pixeles reales de la ventana y se pasa al espacio virtual */
  void setMouse(float x, float y) { mx_ = x / sc_, my_ = y / sc_; }
  bool interactive() const { return st_.overlayOn || st_.ask; }

  /* Área útil de la ventana: lo que hay debajo de la pastilla sigue dibujando vídeo. */
  bool overlayVisible() const { return st_.overlayOn; }

  /* Cuadro ocupado por el aviso / la pastilla en la disposicion actual, en pixeles
     reales de la ventana (los mismos que llevan los vertices). Lo usa la superposicion
     de Wayland para recortar su ventana justo a esa esquina. */
  bool askBounds(float *x, float *y, float *w, float *h) const;
  /* Cuanto mide una etiqueta con la fuente de la interfaz: para comprobar que el texto de un boton
     cabe en su caja sin mirar la pantalla (con la letra mal medida los rotulos se salian del recuadro). */
  float mideTexto(const std::string &s, float px) const { return textWidth(s, px); }
  /* caja del boton de la red: la usa la depuracion (ODYSSEY_GOLPE=red) para pulsarlo sin raton */
  bool redBounds(float *x, float *y, float *w, float *h) const {
    if (btnRed_.w <= 0.0f) return false;
    if (x) *x = btnRed_.x;
    if (y) *y = btnRed_.y;
    if (w) *w = btnRed_.w;
    if (h) *h = btnRed_.h;
    return true;
  }
  bool pillBounds(float *x, float *y, float *w, float *h) const;
  /* La punta derecha de la caja que ocuparia la pastilla, aun estando ocultada. Es el rincon que
     el visor deja pinchable para que el raton pueda devolver la pastilla sin usar el teclado. */
  bool huecoDeLaPastilla(float *x, float *y, float *w, float *h) const;

private:
  struct Glyph {
    float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
    int w = 0, h = 0, bx = 0, by = 0, adv = 0;
    bool ok = false;
  };
  struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    bool inside(float mx, float my) const { return mx >= x && mx <= x + w && my >= y && my <= y + h; }
  };

  void quad(Rect r, Rect uv, float cr, float cg, float cb, float ca);
  void solid(float x, float y, float w, float h, float cr, float cg, float cb, float ca);
  void mask9(Rect r, int which, float radius, float cr, float cg, float cb, float ca);
  float text(const std::string &s, float x, float y, float size, float cr, float cg, float cb, float ca,
             bool center = false);
  float textWidth(const std::string &s, float size) const;

  /* máscaras del atlas: 0 píxel blanco, 1 píldora, 2 tarjeta */
  Rect white_, pill_, card_;
  Glyph glyphs_[256]{};
  /* unos pocos signos fuera del rango latino-1 (euro, raya, comillas tipograficas, puntos
     suspensivos): sin ellos el titulo de la tarjeta salta a "?" en mitad de la frase */
  std::vector<std::pair<unsigned, Glyph>> extra_;
  const Glyph *glifo(unsigned cp) const;
  float asc_ = 17.0f;   /* ascenso de la fuente en píxeles del atlas */
  bool hasText_ = false;
  std::vector<uint8_t> atlas_;
  int aw_ = 0, ah_ = 0;
  int shelfX_ = 0, shelfY_ = 0, shelfH_ = 0;

  State st_{};
  float mx_ = -1.0f, my_ = -1.0f;
  std::string info_;
  std::string askTitulo_, askCuerpo_;   /* vacio = texto oficial */
  std::vector<HudVertex> v_;
  int W_ = 0, H_ = 0;      /* tamano real de la ventana */
  float sc_ = 1.0f;      /* escala de la interfaz: 1 en 1080, hasta 2 en 2160 */
  float scUser_ = 0.0f;  /* tamano fijado a mano (--ui-escala, Ctrl+Shift+7/8); 0 = automatico */

  Rect pillPanel_, btn3D_, btnOv_, btnRed_, depTrack_, popTrack_, depKnob_, popKnob_;
  Rect askPanel_, askSbs_, askTop_, askFlat_, askConv_, askDont_, askClose_;
};

#endif
