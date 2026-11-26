/* vkui.h — la interfaz del hub: la pastilla gris de arriba a la derecha con el interruptor de
   3D (ON/OFF), el boton de la red y los dos controles (profundidad y salida), mas el aviso abajo a
   la derecha que pregunta como ver la ventana a pantalla completa. Todo se dibuja con
   cuadros sobre Vulkan (atlas R8 con fuentes y mascaras) y se maneja con el raton y los
   atajos de la aplicacion oficial. No toca Vulkan: solo geometria, texto y entradas.

   Desde v0.56 la pastilla es RECTA: sin esquinas redondeadas en el panel, los botones, las cajas
   del Facetracking ni las tarjetas del aviso. Unicas curvas: el propio interruptor ON/OFF (asi lo
   pidio Rodolfo, con su bola) y las cabecitas de los deslizadores. La fila de ocultar la pastilla
   ya no existe como boton: se oculta y se muestra con Ctrl+Shift+F1 (o con `odysseyvk ctl capa`,
   que es la misma orden por fichero y sirve cuando el teclado lo tiene otra ventana).

   Desde v0.78 la pastilla es UNA fila (interruptor, dos deslizadores y la casilla de «no volver a
   preguntar») con las teclas en un renglon, y la tarjeta de la orilla se parece a la de la
   aplicacion oficial: redonda, con dos teselas ilustradas. La pastilla sigue recta. */
#ifndef ODYSSEY_VKUI_H
#define ODYSSEY_VKUI_H

#include <algorithm>
#include <cstdint>
#include <memory>
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
  /* Una fila del panel de ajustes (v0.84). El panel no sabe de ajustes: pinta las filas que le den y
     dice cual se pico; que hace cada una lo decide quien lo ensena (src/preferencias.cpp). */
  struct ItemPanel {
    /* Boton (v0.92): rotulo y nota a la izquierda, un boton a la derecha con `valorTexto`.
       Info (v0.92): una «i» en un anillo, el rotulo y, debajo, la nota. */
    /* Atajo (v1.09): el rotulo y, a la derecha, la combinacion en una tecla; `valor` = esperando la
       combinacion nueva; `quitar` = no es la de fabrica (sale la flecha de volver a ella). */
    enum Tipo { Seccion, Interruptor, Opciones, Deslizador, Fila, Juego, Nota, Boton, Info, Atajo };
    Tipo tipo = Nota;
    std::string rotulo, nota;
    std::string valorTexto;          /* a la derecha: el numero del deslizador, el modo de una fila... */
    bool valor = false;              /* interruptor */
    int elegido = 0;                 /* opciones */
    std::vector<std::string> opciones;
    float frac = 0.0f;               /* deslizador, 0..1 */
    bool quitar = false;             /* fila con aspa para quitarla */
    bool peligro = false;            /* boton en rojo suave: desinstalar (v1.06) */
    /* la ayuda al pasar el raton (rama experimental): `ayuda` para la fila entera y, en las opciones,
       `ayudas[k]` para cada una (si la hay, manda sobre `ayuda`) */
    std::string ayuda;
    std::vector<std::string> ayudas;
    /* la caratula de un juego (RGBA, imgW x imgH). El atlas de la interfaz es de un solo canal, asi
       que las imagenes van aparte y solo las pinta el pintado de CPU (el del panel). */
    std::shared_ptr<const std::vector<uint8_t>> img;
    int imgW = 0, imgH = 0;
  };

  struct State {
    bool overlayOn = true;
    bool weaveOn = true;
    int depth = 50;    /* "3D Depth" oficial: 0..100 · 50 = sin cambio */
    int popout = 0;    /* "3D Popout" oficial: -50..+50 */
    bool ask = false;  /* aviso de conversión (abajo a la derecha) */
    /* con esto puesto, la tarjeta de la orilla no pregunta por el video: avisa de que hay esta
       version nueva en GitHub, con Actualizar, Luego y «no avisar» (v0.95) */
    std::string askActualizacion;
    /* con esto puesto, la tarjeta es un aviso de texto con un boton «Entendido» (v1.00): el titulo
       y hasta tres renglones */
    std::string askAviso;
    std::vector<std::string> askAvisoLineas;
    bool remember = false;
    /* la misma casilla, pero dentro del video (en la pastilla): `recordarVisible` la ensena y
       `recordar` dice si esta aplicacion pasa directa al 3D sin preguntar. La pone el visor cuando
       lo lanzo el servicio, que es quien guarda la eleccion en serve.conf. */
    bool recordarVisible = false;
    bool recordar = false;
    /* los botones de icono de la pastilla (v0.79): que tipo de 3D se esta haciendo y poder
       cambiarlo en marcha. 1 = video normal (el conversor), 2 = video dividido (lado a lado, con el
       relieve que trae), 3 = video dividido suavizado (v0.83: una de sus dos imagenes y el relieve
       del conversor). */
    bool modoVisible = false;
    int modo3d = 1;
    /* v0.84: la pastilla lleva solo DOS botones, video normal y video dividido. El dividido es el
       suavizado salvo que en los ajustes del programa se haya quitado: `suavizado` dice cual de los
       dos dibujos lleva ese boton (la imagen con la raya a trazos, o las dos imagenes separadas). */
    bool suavizado = true;
    /* el engranaje de la pastilla, que abre el panel de ajustes (solo lo pone el visor) */
    bool engranaje = false;
    /* ---- el panel de ajustes flotante (v0.84) ---- */
    bool panel = false;
    int pestana = 0;
    std::vector<std::string> pestanas;
    std::vector<ItemPanel> items;
    float corrida = 0.0f;            /* cuanto se ha corrido la lista hacia arriba, en puntos del lienzo */
    std::string pie;                 /* la frase del pie: lo ultimo que se cambio */
    std::string version;             /* "OdysseyVK 0.84", abajo a la derecha */
    /* la actualizacion (v0.95): `actualizacion` = version nueva que hay en GitHub (vacia = ninguna);
       `alDia` = ya se comprobo y es la ultima. Pintan el boton junto al aspa: amarillo con la
       flecha si hay una pendiente, flecha verde si esta al dia, gris si no se ha mirado. */
    std::string actualizacion;
    bool alDia = false;
    /* el boton del idioma (v1.07), a la izquierda del de la actualizacion: abierto, cuelga debajo la
       lista de los siete idiomas */
    bool menuIdioma = false;
    /* el renglon de las teclas de la pastilla cuando hay atajos cambiados (v1.09); vacio = el de fabrica */
    std::string teclas;
    /* la pastilla del modo rendimiento (rama experimental): el conversor va sin red. Lleva el acento
       ambar y, en vez de Profundidad, Salida y los botones de tipo de video, un boton con el rayo que
       despliega el menu del modo: los tres preajustes y los cuatro deslizadores finos.
       `preajuste` = el puesto (0 suave, 1 medio, 2 intenso; -1 = valores a mano); `rendValor` =
       mezcla, fuerza, alcance y paralaje (los rangos, en rendimiento.h); `menuRendimiento` = abierto.
       `teclasRendimiento` es el renglon de las teclas con atajos cambiados (vacio = el de fabrica). */
    bool rendimiento = false;
    int preajuste = -1;
    bool menuRendimiento = false;
    float rendValor[4] = {0.35f, 1.0f, 2.75f, 2.0f};
    std::string teclasRendimiento;
    int hud = 0;       /* 3 = lente y scaler listos · 0 = ninguno */
    int red = 2;       /* que red de profundidad esta puesta: 1 = la de Samsung, 2 = la nativa */
    bool ratonLibre = false;  /* el raton atraviesa la ventana fuera de la pastilla */
    bool redViva = false;  /* hay conversor abierto (si no, el boton de la red se apaga) */
    /* Modo de legado: la pastilla se queda solo con lo que ese camino puede tocar (el 3D, la salida
       y el plano de apoyo). Se cae el boton de la red, el deslizador de la profundidad del conversor y
       los avisos de las teclas que alli no existen. La pastilla mide 648 px en vez de 824. */
    bool legado = false;
    /* La fila de abajo de la pastilla: Facetracking. Solo la enseña el visor (el hub no
       rastrea), asi que la pastilla del hub no crece. No es un boton: el rastreo no se ofrece
       a encender ni a apagar, la pastilla dice con que camara se esta siguiendo la trama. */
    bool controlOjo = false;
    bool rastreoVivo = false;    /* hay hilo de camara abierto (si no, la fila sale apagada) */
    bool rastreoEstudio = false; /* la camara es la integrada del monitor: "3D Studio" */
    bool rastreoCara = false;    /* en el ultimo cuadro habia una cara delante */
    std::string rastreoNota;     /* lo que la camara reporta: "sin cara", "sin pose valida"... */
    std::string rastreoCamara;   /* lo que anuncia el conductor: "3D Stereo WebCam" */
  };

  enum Hit {
    HitNone = 0,
    Hit3D,             /* el interruptor ON/OFF del 3D */
    HitDepth,
    HitPopout,
    HitRed,
    HitAskSbs,
    HitAskTop,
    HitAskFlat,
    HitAskConv,
    HitAskDont,
    HitAskClose,
    HitAskAyuda,       /* el "?" de la tarjeta: no manda nada, solo cambia el texto al pasar por encima */
    HitAskActualizar,  /* la tarjeta de la actualizacion (v0.95): instalar, luego, no avisar mas */
    HitAskLuego,
    HitAskNoAvisar,
    HitRecordar,       /* la casilla de «no volver a preguntar» de la pastilla */
    HitModoNormal,     /* los dos botones de icono de la pastilla: video normal ... */
    HitModoDividido,   /* ... video dividido ... */
    HitModoSuave,      /* ... y video dividido suavizado (v0.83) */
    HitEngranaje,      /* el engranaje de la pastilla: abre el panel de ajustes */
    HitPreajuste1,     /* los tres preajustes del menu del rayo (rama experimental) */
    HitPreajuste2,
    HitPreajuste3,
    HitRendimientoMenu,   /* el boton del rayo: abre y cierra su menu */
    HitRendMezcla,        /* los cuatro deslizadores del menu del rayo */
    HitRendFuerza,
    HitRendAlcance,
    HitRendParalaje,
    HitRendFondo,         /* dentro del menu del rayo, sobre nada que se pique */
    HitPanel,          /* dentro del panel de ajustes, sobre nada que se pique */
    HitPanelCerrar,
    HitPanelActualizar,   /* el boton de la actualizacion, junto al aspa (v0.95) */
    HitPanelIdioma,       /* el boton del idioma (v1.07): abre y cierra la lista */
    HitPanelIdiomaOpcion, /* un idioma de la lista: `sub` = cual (idioma::Lengua) */
    HitPanelPestana,   /* `sub` = cual */
    HitPanelTitulo,    /* la cabecera: arrastrandola se mueve el panel (v0.86) */
    HitPanelItem       /* `item` = que fila; `sub` = que opcion (o 1 = el aspa de quitar); `frac` = deslizador */
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
  /* de cuanto es la pastilla que se dibujo en el ultimo `begin`: 824 de normal, 648 en modo de legado
     (o menos si la ventana es angosta). Lo mide la prueba de `ctl hud`. */
  float panelW() const { return panelW_; }
  float panelH() const { return panelH_; }
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
  /* lo mismo para el panel de ajustes, y `flotanteBounds` da el que este puesto de los dos (el panel
     manda): es la caja que la capa de Wayland recorta */
  bool panelBounds(float *x, float *y, float *w, float *h) const;
  bool flotanteBounds(float *x, float *y, float *w, float *h) const {
    return st_.panel ? panelBounds(x, y, w, h) : askBounds(x, y, w, h);
  }
  /* El golpe con sus senas: que fila del panel, que parte de ella y por donde va el deslizador. */
  Hit hitDetalle(float mx, float my, int *item, int *sub, float *frac) const;
  /* arrastrando el deslizador de la fila `item`: la fraccion que corresponde a esa x del raton */
  float fracDeslizador(int item, float mx) const;
  /* la rueda del raton sobre la lista del panel (positivo = bajar) */
  void correr(float cuanto);
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
  /* caja de la casilla de la pastilla, en pixeles reales (para la prueba y para ODYSSEY_GOLPE) */
  bool recordarBounds(float *x, float *y, float *w, float *h) const {
    if (chkRecordar_.w <= 0.0f) return false;
    if (x) *x = chkRecordar_.x * sc_;
    if (y) *y = chkRecordar_.y * sc_;
    if (w) *w = chkRecordar_.w * sc_;
    if (h) *h = chkRecordar_.h * sc_;
    return true;
  }
  /* Los vertices del ultimo `begin` pintados en la CPU sobre un lienzo de ancho x alto cuyo origen
     esta en (ox, oy) de la ventana. Sale RGBA en coma flotante, color ya multiplicado por opacidad. */
  void pintarCpu(float ox, float oy, int ancho, int alto, std::vector<float> &acum) const;
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
  void icono(Rect r, const Rect &m, float cr, float cg, float cb, float ca, float fx0 = 0.0f, float fy0 = 0.0f,
             float fx1 = 1.0f, float fy1 = 1.0f);
  float muestra(float u, float v) const;
  /* el paisaje de las teselas y de los botones de la pastilla: `apretado` lo estrecha (cada mitad
     del video dividido), `luz` lo apaga (el boton que no esta puesto) y `radio` > 0 le redondea las
     esquinas tapandolas con el color `fondo` */
  void paisaje(Rect r, float apretado, float luz, float radio, const float *fondo);
  void dibujaPanel(int vw, int vh);
  /* recorte de todo lo que se pinte mientras este puesto (la lista del panel, que corre) */
  Rect recorte_;
  bool hayRecorte_ = false;
  struct ImagenCpu {
    Rect caja, recorte;      /* en puntos del lienzo virtual */
    const uint8_t *px = nullptr;
    int w = 0, h = 0;
    float radio = 0.0f;
  };
  std::vector<ImagenCpu> imagenes_;
  float text(const std::string &s, float x, float y, float size, float cr, float cg, float cb, float ca,
             bool center = false);
  float textWidth(const std::string &s, float size) const;
  /* como text(), pero sin pasar de `maxAncho`: primero achica la letra hasta un 80 % y, si aun no
     cabe, corta con «…» (v1.07: el frances y el aleman son mas largos que el español) */
  float textoCabe(const std::string &s, float x, float y, float size, float maxAncho, float cr, float cg, float cb,
                  float ca, bool center = false);
  /* un texto partido en renglones de hasta `maxAncho` (las palabras sin espacios, como el chino o el
     japones, se parten por letras): lo usan las cajas de ayuda */
  std::vector<std::string> renglones(const std::string &s, float size, float maxAncho) const;
  /* la caja de ayuda con su texto ya partido, con la esquina de arriba a la izquierda en (x, y) */
  void cajaAyuda(const std::vector<std::string> &lineas, float x, float y, float size);

  /* máscaras del atlas: 0 píxel blanco, 1 píldora, 2 tarjeta */
  Rect white_, pill_, card_;
  /* los dibujos de la tarjeta: aspa, palomita, anillo del "?", el `hueco` que redondea esquinas y
     las dos siluetas del paisaje (todas en pixeles del atlas) */
  Rect icoCerrar_, icoPalomita_, icoAnillo_, hueco_, monteLejos_, monteCerca_, icoEngranaje_, icoFlecha_, icoGlobo_,
      icoRayo_, icoBajar_;
  /* el radio que trae dibujada cada mascara, en píxeles de la mascara: mask9 saca de ahi la
     porcion de esquina y la estira hasta el radio que se pida en pantalla. Con la píldora es la
     mitad del cuadrado (48 de 96), o sea un cuarto de circulo entero; de ahi sale la pista y la
     bola del interruptor. Antes se usaba el radio de pantalla para recortar la mascara, y salian
     puntas de lente en vez de esquinas redondas. */
  float maskRad_[2] = {48.0f, 24.0f};   /* {píldora, tarjeta} */
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

  /* sw3D_ es la pista del interruptor ON/OFF y swKnob_ la bola blanca de dentro (la bola es lo unico
     redondo de la pastilla: asi lo queria Rodolfo). btnOv_ ya no esta: ocultar la pastilla es tecla. */
  Rect pillPanel_, sw3D_, swKnob_, btnRed_, depTrack_, popTrack_, depKnob_, popKnob_;
  float panelW_ = 0.0f, panelH_ = 0.0f;   /* de cuanto salio la pastilla en el ultimo begin */
  Rect askPanel_, askSbs_, askTop_, askFlat_, askConv_, askDont_, askClose_, askAyuda_;
  Rect askActualizar_, askLuego_, askNoAvisar_;
  Rect chkRecordar_, btnNormal_, btnDividido_, btnSuave_, btnEngranaje_;
  Rect btnPre_[3];   /* los preajustes del menu del rayo */
  /* el boton del rayo de la pastilla, su menu desplegado y las pistas de sus cuatro deslizadores */
  Rect btnRayo_, menuRend_, rendTrack_[4], rendFila_[4];
  /* el panel de ajustes: su caja, el aspa, las pestanas, el cuerpo que corre y la caja de cada fila */
  struct ZonaItem {
    Rect caja;
    std::vector<Rect> subs;   /* las opciones de un segmentado, o el aspa de una fila */
    Rect pista;               /* la pista de un deslizador */
  };
  Rect panelCaja_, panelActualizar_, panelCerrar_, panelCuerpo_, panelIdioma_;
  std::vector<Rect> panelIdiomaOpc_;   /* las filas de la lista de idiomas, si esta abierta */
  std::vector<Rect> panelPestanas_;
  std::vector<ZonaItem> zonas_;
  float contenidoAlto_ = 0.0f;
};

#endif
