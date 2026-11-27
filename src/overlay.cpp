/* overlay.cpp — cliente de wlr-layer-shell que pinta la tarjeta de aviso del hub por
   encima de cualquier ventana, incluidas las de pantalla completa.

   Dos cosas la hacen posible:
     · la capa OVERLAY del protocolo wlr-layer-shell, que en KWin va por encima de las
       ventanas a pantalla completa (una ventana normal siempre las tapa, y el centro de
       notificaciones de KDE ni se ve);
     · el teclado NO se intercepta, así que el reproductor sigue recibiendo espacio,
       flechas, etc. mientras la tarjeta está puesta.

   El dibujo se hace en la CPU: se reutiliza la misma geometría, el mismo atlas de
   fuentes y los mismos colores que la interfaz Vulkan (vkui.cpp), así que la tarjeta es
   idéntica a la que se ve dentro del visor. Cada cuadro de la interfaz es un rectángulo
   alineado que multiplica su color por una máscara del atlas, así que el rasterizador
   que hace falta son cuatro líneas. */
#include "overlay.h"

#include <chrono>
#include <thread>

#include "glutil.h"
#include "vkui.h"
#include "util.h"

#include "xdg-output-unstable-v1-client-protocol.h"

#ifdef HAVE_FRACTIONAL_SCALE
#include "fractional-scale-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#endif
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#ifdef HAVE_KWIN_BLUR
#include "blur-client-protocol.h"
#endif

#include <wayland-client.h>
#ifdef HAVE_XKBCOMMON
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-compose.h>
#endif

#include <linux/input-event-codes.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
#ifdef HAVE_WAYLAND_CURSOR
#include <wayland-cursor.h>
#endif

/* ---------------- implementación ---------------- */

#ifdef HAVE_XKBCOMMON
/* (v1.15) una tecla pulsada: su combinacion, escrita como los atajos ("Ctrl+Shift+K"; vacia si la tecla solo escribe,
   como la ñ), y lo que escribe (UTF-8, con Mayus, AltGr y las teclas muertas; vacio si no escribe). Falso si no da
   nada (un modificador solo, la primera de una tecla muerta) */
static bool leeTeclaXkb(xkb_keymap *mapa, xkb_state *estado, xkb_compose_state *compose, xkb_keycode_t kc, std::string &combo,
                        std::string &texto) {
  const xkb_keysym_t *syms = nullptr;
  /* la tecla en su nivel de base: Mayus+1 se apunta como Shift+1, no como «!» */
  const int n = xkb_keymap_key_get_syms_by_level(mapa, kc, xkb_state_key_get_layout(estado, kc), 0, &syms);
  if (n < 1) return false;
  const xkb_keysym_t k = syms[0];
  /* (v1.15) lo que escribe, con el estado de verdad (Mayus, AltGr) y las teclas muertas */
  texto.clear();
  bool compuesta = false;
  if (compose) {
    const xkb_keysym_t sym = xkb_state_key_get_one_sym(estado, kc);
    if (sym != XKB_KEY_NoSymbol && xkb_compose_state_feed(compose, sym) == XKB_COMPOSE_FEED_ACCEPTED) {
      switch (xkb_compose_state_get_status(compose)) {
        case XKB_COMPOSE_COMPOSING:
          compuesta = true;   /* una tecla muerta: se espera a la siguiente */
          break;
        case XKB_COMPOSE_COMPOSED: {
          char b[64] = {0};
          xkb_compose_state_get_utf8(compose, b, sizeof b);
          texto = b;
          compuesta = true;
          xkb_compose_state_reset(compose);
          break;
        }
        case XKB_COMPOSE_CANCELLED:
          compuesta = true;
          xkb_compose_state_reset(compose);
          break;
        default:
          break;
      }
    }
  }
  if (!compuesta) {
    char b[64] = {0};
    if (xkb_state_key_get_utf8(estado, kc, b, sizeof b) > 0) texto = b;
  }
  if (!texto.empty() && ((unsigned char)texto[0] < 0x20 || texto == "\x7f")) texto.clear();   /* Ctrl+letra, Retroceso, Intro... */
  std::string nombre;
  if (k >= XKB_KEY_a && k <= XKB_KEY_z) nombre = std::string(1, (char)('A' + (k - XKB_KEY_a)));
  else if (k >= XKB_KEY_A && k <= XKB_KEY_Z) nombre = std::string(1, (char)k);
  else if (k >= XKB_KEY_0 && k <= XKB_KEY_9) nombre = std::string(1, (char)k);
  else if (k >= XKB_KEY_F1 && k <= XKB_KEY_F24) nombre = "F" + std::to_string(k - XKB_KEY_F1 + 1);   /* F13..F24 desde la 1.12 */
  else if (k == XKB_KEY_Left) nombre = "Left";
  else if (k == XKB_KEY_Right) nombre = "Right";
  else if (k == XKB_KEY_Up) nombre = "Up";
  else if (k == XKB_KEY_Down) nombre = "Down";
  else if (k == XKB_KEY_Prior) nombre = "PgUp";
  else if (k == XKB_KEY_Next) nombre = "PgDown";
  else if (k == XKB_KEY_Home) nombre = "Home";
  else if (k == XKB_KEY_End) nombre = "End";
  else if (k == XKB_KEY_Insert) nombre = "Insert";
  else if (k == XKB_KEY_Delete) nombre = "Delete";
  else if (k == XKB_KEY_space) nombre = "Space";
  else if (k == XKB_KEY_plus) nombre = "Plus";
  /* v1.12: Pausa/Inter (la PB), Impr Pant, Bloq Despl y Menu; antes se ignoraban y no habia modo de ponerlas */
  else if (k == XKB_KEY_Pause || k == XKB_KEY_Break) nombre = "Pause";
  else if (k == XKB_KEY_Print || k == XKB_KEY_Sys_Req) nombre = "Print";
  else if (k == XKB_KEY_Scroll_Lock) nombre = "ScrollLock";
  else if (k == XKB_KEY_Menu) nombre = "Menu";
  else if (k == XKB_KEY_BackSpace) nombre = "Backspace";   /* (v1.15) para los campos de texto */
  else if (k == XKB_KEY_Return || k == XKB_KEY_KP_Enter) nombre = "Return";
  else if (k == XKB_KEY_Tab || k == XKB_KEY_ISO_Left_Tab) nombre = "Tab";
  else if (k >= 0x21 && k <= 0x7e) nombre = std::string(1, (char)k);
  else if (texto.empty()) return false;   /* los modificadores solos, Bloq Mayus, las teclas raras: se espera a la de verdad */
  auto activo = [&](const char *m) {
    return xkb_state_mod_name_is_active(estado, m, XKB_STATE_MODS_EFFECTIVE) > 0;
  };
  combo.clear();
  if (!nombre.empty()) {
    if (activo(XKB_MOD_NAME_CTRL)) combo += "Ctrl+";
    if (activo(XKB_MOD_NAME_ALT)) combo += "Alt+";
    if (activo(XKB_MOD_NAME_SHIFT)) combo += "Shift+";
    if (activo(XKB_MOD_NAME_LOGO)) combo += "Meta+";
    combo += nombre;
  }
  /* con Ctrl, Alt o Meta no se escribe (Ctrl+V pega, no escribe una v) */
  if (activo(XKB_MOD_NAME_CTRL) || activo(XKB_MOD_NAME_ALT) || activo(XKB_MOD_NAME_LOGO)) texto.clear();
  return !(combo.empty() && texto.empty());
}
#endif

struct Overlay::Impl {
  /* protocolos */
  wl_display *dpy = nullptr;
  wl_registry *registry = nullptr;
  wl_compositor *comp = nullptr;
  wl_shm *shm = nullptr;
  zwlr_layer_shell_v1 *shell = nullptr;
  xdg_wm_base *wmBase = nullptr;         /* la ventana normal de la entrada en la barra de tareas (v1.04) */
  wl_surface *barraSurf = nullptr;
  xdg_surface *barraXdg = nullptr;
  xdg_toplevel *barraTop = nullptr;
  bool barraCerrada = false;
  zxdg_output_manager_v1 *xdgMgr = nullptr;
  wl_seat *seat = nullptr;
  wl_pointer *pointer = nullptr;
  /* el teclado (v1.09): el panel de ajustes se lo queda entero mientras esta abierto (el juego de
     debajo no recibe teclas) y asi puede capturar los atajos nuevos */
  wl_keyboard *keyboard = nullptr;
  /* 0 = sin teclado · 1 = exclusivo (el panel abierto) · 2 = a pedido, solo cuando se pica la capa (el
     panel fijado encima, rama experimental; layer-shell v4, si no se queda en 0) */
  int tecladoPedido = 0;
  uint32_t capaVersion = 1;
  std::vector<std::string> teclas;   /* combinaciones pulsadas: "Ctrl+Shift+K", "Escape" */
  /* (v1.15) lo que escribe cada pulsacion, a la par de `teclas` (UTF-8; vacio si no escribe nada), para los
     campos de texto de la pestana Integraciones. Una tecla que solo escribe (la ñ, una letra con acento) va con
     la combinacion vacia */
  std::vector<std::string> textos;
  /* la repeticion de la tecla que se deja pulsada (lo que pide el compositor: tantas por segundo, tras tanta espera) */
  int32_t repRitmo = 25, repEspera = 600;
  uint32_t repTecla = 0;   /* la que se repite (0 = ninguna) */
  std::string repCombo, repTexto;
  double repSiguiente = 0.0;
#ifdef HAVE_XKBCOMMON
  xkb_context *xkbCtx = nullptr;
  xkb_keymap *xkbMapa = nullptr;
  xkb_state *xkbEstado = nullptr;
  /* las teclas muertas (´ + a = á; en el teclado espanol, la ~ y el ^ tambien lo son) */
  xkb_compose_table *composeTabla = nullptr;
  xkb_compose_state *compose = nullptr;
#endif
#ifdef HAVE_FRACTIONAL_SCALE
  wp_fractional_scale_manager_v1 *escalaMgr = nullptr;
#ifdef HAVE_KWIN_BLUR
  org_kde_kwin_blur_manager *blurMgr = nullptr;   /* KWin desenfoca lo que hay detras de la region que se le diga */
  org_kde_kwin_blur *blur = nullptr;
#endif
  wp_viewporter *viewporter = nullptr;
  wp_viewport *viewport = nullptr;
  wp_fractional_scale_v1 *escalaSuperf = nullptr;
#endif

  /* monitores que anuncia el compositor */
  struct Monitor {
    wl_output *obj = nullptr;
    uint32_t global = 0;                      /* nombre del global: así se sabe si desaparece */
    int32_t x = 0, y = 0, ancho = 0, alto = 0;   /* en píxeles lógicos */
    int anchoFis = 0, altoFis = 0;               /* en píxeles reales */
    int anchoLog = 0, altoLog = 0;               /* tamaño lógico anunciado por xdg_output */
    zxdg_output_v1 *xdg = nullptr;
    int escala = 1;
    uint32_t transformacion = 0;
    std::string nombre, modelo;
    bool vivo = true;
  };
  std::vector<Monitor *> monitores;
  Monitor *elegido = nullptr;
  wl_output *capaEn = nullptr;   /* monitor para el que se creó la capa actual */
  int ofrecidoAncho = 0;         /* area util que ofrecio el compositor: delata donde cayo */
  int ofrecidoAlto = 0;
  int intentosColocacion = 0;    /* reintentos hechos para colocar la capa bien */
  bool avisoColocacion = false;  /* hay aviso pendiente para quien lleve el servicio */
  bool reconocido = false;       /* el monitor elegido es claramente el panel del Odyssey */
  int anchoE = 0, altoE = 0;      /* tamaño real del monitor elegido, en píxeles */
  double escala = 1.0;            /* 1.0, 1.5, 2.0 ... */

  /* la superficie de la tarjeta */
  wl_surface *surf = nullptr;
  zwlr_layer_surface_v1 *layer = nullptr;
  bool hayConfigure = false, cerrada = false;
  int supAncho = 0, supAlto = 0;  /* tamaño lógico que nos asigna el compositor */
  int bufAncho = 0, bufAlto = 0;  /* mapa en píxeles reales */
  int margenDer = 0, margenAb = 0;
  double ox = 0.0, oy = 0.0;      /* esquina de la tarjeta en píxeles del monitor */
  double escalaGeom = 0.0;        /* escala con la que se calculo el recorte actual */
  /* y la escala del MONITOR de entonces. El panel al doble lleva su tamano fijo, asi que `hud.scale()`
     no cambia cuando el compositor anuncia su escala de verdad (aqui pasa de 1,50 a 1,80), el recorte
     no se rehacia y el raton se media con una escala y la ventana con otra: los golpes caian un 20 %
     corridos (el aspa no cerraba y el deslizador saltaba al tope; lo vio Rodolfo con la 0.84). */
  double escalaMonGeom = 0.0;
  bool sucio = false;             /* el panel pide pintarse: se pinta UNA vez por tanda de eventos */
  /* el velo (rama experimental): con el panel abierto sobre un juego, una capa transparente que cubre
     la pantalla entera por debajo del panel. Se queda con el raton: el juego no ve ni movimiento ni
     clics, su cursor encerrado se suelta, y un clic en el velo cierra el panel (el juego recupera el
     control). Se crea antes que la capa del panel para quedar debajo de ella. */
  wl_surface *veloSurf = nullptr;
  zwlr_layer_surface_v1 *veloLayer = nullptr;
  bool veloConfigurado = false, ratonEnVelo = false;
  int veloAncho = 0, veloAlto = 0;
#ifdef HAVE_FRACTIONAL_SCALE
  wp_viewport *veloViewport = nullptr;
#endif
  /* la flecha del tema: KWin no la pone sola, y sobre un juego que esconde su cursor no se veia nada */
#ifdef HAVE_WAYLAND_CURSOR
  wl_cursor_theme *temaCursor = nullptr;
  int temaEscala = 0;
#endif
  wl_surface *cursorSurf = nullptr;
  bool conexionCaida = false;     /* el compositor cerró la conexión de la capa */

  /* mapas de memoria para los cuadros (doble buffer: uno en pantalla, otro dibujando) */
  struct Bloque {
    wl_shm_pool *pozo = nullptr;
    wl_buffer *buf = nullptr;
    uint8_t *mapa = nullptr;
    size_t tam = 0;
    int fd = -1;
    int ancho = 0, alto = 0;
    bool libre = false;
  };
  std::vector<Bloque *> bloques;
  Bloque *barraBloque = nullptr;   /* el pixel de la entrada en la barra de tareas (v1.04) */
  Bloque *veloBloque = nullptr;    /* el lienzo transparente del velo (rama experimental) */
  std::vector<float> acum;        /* RGBA premultiplicado, para mezclar como hace la GPU */

  Hud hud;
  bool atlasListo = false;
  bool tarjetaPuesta = false;
  int golpeVisto = Hud::HitNone;
  /* el panel de ajustes: el deslizador que se esta arrastrando, lo que habia bajo el raton la
     ultima vez (para no repintar de mas) y los golpes que esperan a quien lo ensena */
  /* (v1.16, PR #1) el deslizador que se arrastra vive en el estado de la interfaz: hud.state().arrastrando */
  /* mover el panel: se agarra por la cabecera y la capa sigue al raton cambiando sus margenes */
  bool moviendo = false, panelMontado = false;
  /* cambiar el tamano del panel con su esquina (rama experimental): el tamano del que se partio, en
     puntos del lienzo, y donde se agarro; y la flecha de la esquina puesta o no */
  bool redimensionando = false, cursorEsquina = false;
  float agarreAncho = 0.0f, agarreAlto = 0.0f;
  uint32_t serialPuntero = 0;
  bool esperaMov = false;
  double tMov = 0.0;
  static void cuadroMov(void *data, wl_callback *cb, uint32_t) {
    static_cast<Impl *>(data)->esperaMov = false;
    wl_callback_destroy(cb);
  }
  static const struct wl_callback_listener cuadroMovListener;
  double agarreX = 0.0, agarreY = 0.0, supX = 0.0, supY = 0.0;
  int margenIzq = 0, margenArr = 0, logAncho = 0, logAlto = 0;
  long firmaVista = -1;
  std::vector<Overlay::Golpe> golpes;
  double ratonX = -1.0, ratonY = -1.0;
  bool ratonDentro = false;
  std::string accion;
  std::string titulo, cuerpo;
  std::string aviso;
  std::string pistaMonitor;

  ~Impl() {
    quitaVelo();
#ifdef HAVE_WAYLAND_CURSOR
    if (temaCursor) wl_cursor_theme_destroy(temaCursor);
#endif
    if (cursorSurf) wl_surface_destroy(cursorSurf);
    quitaBarra();
    if (wmBase) xdg_wm_base_destroy(wmBase);
    limpiaTarjeta();
    liberaBloques(true);
#ifdef HAVE_FRACTIONAL_SCALE
    if (escalaSuperf) wp_fractional_scale_v1_destroy(escalaSuperf);
    if (viewport) wp_viewport_destroy(viewport);
    if (viewporter) wp_viewporter_destroy(viewporter);
    if (escalaMgr) wp_fractional_scale_manager_v1_destroy(escalaMgr);
#ifdef HAVE_KWIN_BLUR
    if (blur) org_kde_kwin_blur_release(blur);
    if (blurMgr) org_kde_kwin_blur_manager_destroy(blurMgr);
#endif
#endif
    if (surf) wl_surface_destroy(surf);
    if (pointer) wl_pointer_destroy(pointer);
    if (keyboard) wl_keyboard_destroy(keyboard);
#ifdef HAVE_XKBCOMMON
    if (xkbEstado) xkb_state_unref(xkbEstado);
    if (xkbMapa) xkb_keymap_unref(xkbMapa);
    if (xkbCtx) xkb_context_unref(xkbCtx);
#endif
    if (seat) wl_seat_destroy(seat);
    for (Monitor *m : monitores)
      if (m->obj) wl_output_destroy(m->obj);   /* los zxdg_output los destruye el compositor */
    if (shell) zwlr_layer_shell_v1_destroy(shell);
    if (shm) wl_shm_destroy(shm);
    if (comp) wl_compositor_destroy(comp);
    if (registry) wl_registry_destroy(registry);
    if (dpy) wl_display_disconnect(dpy);
    for (Monitor *m : monitores) delete m;
    monitores.clear();
  }

  /* ---------- la entrada en la barra de tareas (v1.04) ---------- */
  static void wmPing(void *, xdg_wm_base *b, uint32_t serie) { xdg_wm_base_pong(b, serie); }
  static void barraConfigura(void *data, xdg_surface *xs, uint32_t serie) {
    Impl *s = static_cast<Impl *>(data);
    xdg_surface_ack_configure(xs, serie);
    if (s->barraBloque && s->barraSurf) {
      wl_surface_attach(s->barraSurf, s->barraBloque->buf, 0, 0);
      wl_surface_damage(s->barraSurf, 0, 0, 1, 1);
      wl_surface_commit(s->barraSurf);
    }
  }
  static void topConfigura(void *, xdg_toplevel *, int32_t, int32_t, wl_array *) {}
  static void topCierra(void *data, xdg_toplevel *) { static_cast<Impl *>(data)->barraCerrada = true; }
  static void topLimites(void *, xdg_toplevel *, int32_t, int32_t) {}
  static void topCapacidades(void *, xdg_toplevel *, wl_array *) {}
  void ponBarra(const std::string &titulo) {
    if (barraTop) {   /* ya esta: solo el titulo nuevo (v1.07: al cambiar de idioma) */
      xdg_toplevel_set_title(barraTop, titulo.c_str());
      wl_display_flush(dpy);
      return;
    }
    if (!wmBase || !comp || !shm) return;
    static const xdg_wm_base_listener wl = {wmPing};
    static const xdg_surface_listener sl = {barraConfigura};
    static const xdg_toplevel_listener tl = {topConfigura, topCierra, topLimites, topCapacidades};
    xdg_wm_base_add_listener(wmBase, &wl, this);
    barraSurf = wl_compositor_create_surface(comp);
    std::string e;
    barraBloque = nuevoBloque(shm, 1, 1, &e);   /* un pixel transparente: la ventana no se ve */
    wl_region *nada = wl_compositor_create_region(comp);
    wl_surface_set_input_region(barraSurf, nada);   /* y el raton la atraviesa */
    wl_region_destroy(nada);
    barraXdg = xdg_wm_base_get_xdg_surface(wmBase, barraSurf);
    xdg_surface_add_listener(barraXdg, &sl, this);
    barraTop = xdg_surface_get_toplevel(barraXdg);
    xdg_toplevel_add_listener(barraTop, &tl, this);
    xdg_toplevel_set_title(barraTop, titulo.c_str());
    xdg_toplevel_set_app_id(barraTop, "odysseyvk");   /* el icono y el nombre del lanzador del menu */
    wl_surface_commit(barraSurf);
    wl_display_flush(dpy);
  }
  void quitaBarra() {
    if (barraTop) xdg_toplevel_destroy(barraTop);
    if (barraXdg) xdg_surface_destroy(barraXdg);
    if (barraSurf) wl_surface_destroy(barraSurf);
    barraTop = nullptr, barraXdg = nullptr, barraSurf = nullptr;
    if (barraBloque) {
      wl_buffer_destroy(barraBloque->buf);
      wl_shm_pool_destroy(barraBloque->pozo);
      ::munmap(barraBloque->mapa, barraBloque->tam);
      ::close(barraBloque->fd);
      delete barraBloque;
      barraBloque = nullptr;
    }
    if (dpy) wl_display_flush(dpy);
  }

  /* ---------- el velo y la flecha (rama experimental) ---------- */
  static void veloConfigure(void *data, zwlr_layer_surface_v1 *ls, uint32_t serial, uint32_t w, uint32_t h) {
    Impl *s = static_cast<Impl *>(data);
    zwlr_layer_surface_v1_ack_configure(ls, serial);
    if (w == 0 || h == 0 || (s->veloConfigurado && (int)w == s->veloAncho && (int)h == s->veloAlto)) return;
    s->veloAncho = (int)w, s->veloAlto = (int)h;
    std::string e;
    /* transparente: con el recorte de la ventana basta un pixel estirado a la pantalla; sin el, un
       lienzo del tamano logico (ceros = transparente) */
    bool estirado = false;
#ifdef HAVE_FRACTIONAL_SCALE
    if (s->viewporter) {
      if (!s->veloViewport) s->veloViewport = wp_viewporter_get_viewport(s->viewporter, s->veloSurf);
      wp_viewport_set_destination(s->veloViewport, (int)w, (int)h);
      estirado = s->veloViewport != nullptr;
    }
#endif
    if (s->veloBloque) {
      wl_buffer_destroy(s->veloBloque->buf);
      wl_shm_pool_destroy(s->veloBloque->pozo);
      ::munmap(s->veloBloque->mapa, s->veloBloque->tam);
      ::close(s->veloBloque->fd);
      delete s->veloBloque;
      s->veloBloque = nullptr;
    }
    s->veloBloque = nuevoBloque(s->shm, estirado ? 1 : (int)w, estirado ? 1 : (int)h, &e);
    if (!s->veloBloque) {
      std::fprintf(stderr, "[velo] sin lienzo: %s\n", e.c_str());
      return;
    }
    wl_surface_attach(s->veloSurf, s->veloBloque->buf, 0, 0);
    wl_surface_damage(s->veloSurf, 0, 0, (int)w, (int)h);
    wl_surface_commit(s->veloSurf);
    s->veloConfigurado = true;
  }
  static void veloCerrado(void *data, zwlr_layer_surface_v1 *) { static_cast<Impl *>(data)->quitaVelo(); }
  static const struct zwlr_layer_surface_v1_listener veloListener;
  void ponVelo() {
    if (veloSurf || !comp || !shell || !shm) return;
    veloSurf = wl_compositor_create_surface(comp);
    veloLayer = zwlr_layer_shell_v1_get_layer_surface(shell, veloSurf, elegido ? elegido->obj : nullptr,
                                                      ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "odysseyvk-velo");
    if (!veloLayer) {
      wl_surface_destroy(veloSurf);
      veloSurf = nullptr;
      return;
    }
    zwlr_layer_surface_v1_add_listener(veloLayer, &veloListener, this);
    zwlr_layer_surface_v1_set_anchor(veloLayer, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                                                    ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_size(veloLayer, 0, 0);
    zwlr_layer_surface_v1_set_exclusive_zone(veloLayer, -1);
    zwlr_layer_surface_v1_set_keyboard_interactivity(veloLayer, 0);
    wl_surface_commit(veloSurf);
    for (int intento = 0; intento < 40 && !veloConfigurado && veloSurf; ++intento)
      if (esperar(50) < 0) break;
    std::printf("[velo] %s\n", veloConfigurado ? "puesto: el raton es del panel mientras este abierto"
                                                : "el compositor no lo configuro");
    std::fflush(stdout);
  }
  void quitaVelo() {
#ifdef HAVE_FRACTIONAL_SCALE
    if (veloViewport) wp_viewport_destroy(veloViewport);
    veloViewport = nullptr;
#endif
    if (veloLayer) zwlr_layer_surface_v1_destroy(veloLayer);
    if (veloSurf) wl_surface_destroy(veloSurf);
    veloLayer = nullptr, veloSurf = nullptr;
    veloConfigurado = false, ratonEnVelo = false;
    if (veloBloque) {
      wl_buffer_destroy(veloBloque->buf);
      wl_shm_pool_destroy(veloBloque->pozo);
      ::munmap(veloBloque->mapa, veloBloque->tam);
      ::close(veloBloque->fd);
      delete veloBloque;
      veloBloque = nullptr;
    }
    if (dpy) wl_display_flush(dpy);
  }
  /* la flecha del tema (XCURSOR_THEME / XCURSOR_SIZE), al doble o al triple si la pantalla va escalada */
  void ponCursor(uint32_t serial, const char *forma = nullptr) {
#ifdef HAVE_WAYLAND_CURSOR
    if (!pointer || !shm || !comp) return;
    const char *tam = std::getenv("XCURSOR_SIZE");
    int base = tam ? std::atoi(tam) : 24;
    if (base <= 0) base = 24;
    auto flecha = [&](int esc) -> wl_cursor_image * {
      if (!temaCursor || temaEscala != esc) {
        if (temaCursor) wl_cursor_theme_destroy(temaCursor);
        temaCursor = wl_cursor_theme_load(std::getenv("XCURSOR_THEME"), base * esc, shm);
        temaEscala = esc;
      }
      if (!temaCursor) return nullptr;
      wl_cursor *c = forma ? wl_cursor_theme_get_cursor(temaCursor, forma) : nullptr;
      if (!c && forma) c = wl_cursor_theme_get_cursor(temaCursor, "size_fdiag");
      if (!c) c = wl_cursor_theme_get_cursor(temaCursor, "default");
      if (!c) c = wl_cursor_theme_get_cursor(temaCursor, "left_ptr");
      if (!c || c->image_count == 0) return nullptr;
      wl_cursor_image *im = c->images[0];
      /* el lienzo de una superficie escalada tiene que ser multiplo de su escala */
      return ((int)im->width % esc == 0 && (int)im->height % esc == 0) ? im : nullptr;
    };
    int esc = std::max(1, (int)std::ceil(escala - 0.01));
    wl_cursor_image *im = flecha(esc);
    if (!im && esc > 1) im = flecha(esc = 1);
    if (!im) return;
    wl_buffer *b = wl_cursor_image_get_buffer(im);
    if (!b) return;
    if (!cursorSurf) cursorSurf = wl_compositor_create_surface(comp);
    wl_surface_set_buffer_scale(cursorSurf, esc);
    wl_surface_attach(cursorSurf, b, 0, 0);
    wl_surface_damage(cursorSurf, 0, 0, (int)im->width, (int)im->height);
    wl_surface_commit(cursorSurf);
    wl_pointer_set_cursor(pointer, serial, cursorSurf, (int)im->hotspot_x / esc, (int)im->hotspot_y / esc);
#else
    (void)serial, (void)forma;
#endif
  }
  /* el sitio y el tamano del panel, para la proxima vez (rama experimental): margenes logicos y tamano
     en puntos del lienzo, en ~/.local/state/odysseyvk/panel-ventana */
  void guardaVentana() {
    if (FILE *f = std::fopen(rutaEstadoOdyssey("panel-ventana").c_str(), "we")) {
      std::fprintf(f, "%d %d %.0f %.0f\n", margenIzq, margenArr, (double)hud.panelAnchoVisto(), (double)hud.panelAltoVisto());
      std::fclose(f);
    }
  }
  bool leeVentana(int &izq, int &arr, float &ancho, float &alto) const {
    FILE *f = std::fopen(rutaEstadoOdyssey("panel-ventana").c_str(), "re");
    if (!f) return false;
    const bool ok = std::fscanf(f, "%d %d %f %f", &izq, &arr, &ancho, &alto) == 4;
    std::fclose(f);
    return ok && ancho > 0.0f && alto > 0.0f;
  }

  /* ---------- memoria compartida ---------- */

  static Bloque *nuevoBloque(wl_shm *shm, int w, int h, std::string *err) {
    const int stride = w * 4;
    const size_t tam = (size_t)stride * (size_t)h;
    /* (v1.13) el nombre se borra en cuanto se crea: el compositor solo necesita el descriptor. Antes se
       quedaba en /dev/shm para siempre (en RAM): MEDIDO el 26-nov, 1183 archivos y 7,8 GB de un dia de
       uso, uno por cada lienzo de la pastilla, el panel o las tarjetas */
    char plantilla[] = "/dev/shm/odysseyvk-XXXXXX";
    int fd = ::mkstemp(plantilla);
    if (fd >= 0) ::unlink(plantilla);
    if (fd < 0) {
      char plantilla2[] = "/tmp/odysseyvk-XXXXXX";
      fd = ::mkstemp(plantilla2);
      if (fd >= 0) ::unlink(plantilla2);
      if (fd < 0) {
        if (err) *err = "no se pudo crear el mapa de memoria";
        return nullptr;
      }
    }
    if (::ftruncate(fd, (off_t)tam) != 0) {
      ::close(fd);
      if (err) *err = "no se pudo dar tamaño al mapa de memoria";
      return nullptr;
    }
    void *p = ::mmap(nullptr, tam, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
      ::close(fd);
      if (err) *err = "no se pudo mapear la memoria compartida";
      return nullptr;
    }
    wl_shm_pool *pozo = wl_shm_create_pool(shm, fd, (int32_t)tam);
    if (!pozo) {
      ::munmap(p, tam);
      ::close(fd);
      if (err) *err = "el compositor rechazó el charco de memoria compartida";
      return nullptr;
    }
    wl_buffer *b = wl_shm_pool_create_buffer(pozo, 0, w, h, stride, WL_SHM_FORMAT_ARGB8888);
    if (!b) {
      wl_shm_pool_destroy(pozo);
      ::munmap(p, tam);
      ::close(fd);
      if (err) *err = "el compositor rechazó el lienzo";
      return nullptr;
    }
    Bloque *bl = new Bloque();
    bl->pozo = pozo;
    bl->buf = b;
    bl->mapa = static_cast<uint8_t *>(p);
    bl->tam = tam;
    bl->fd = fd;
    bl->ancho = w;
    bl->alto = h;
    std::memset(bl->mapa, 0, tam);
    wl_buffer_add_listener(b, &bufferListener, bl);
    return bl;
  }

  static void bufferReleased(void *data, struct wl_buffer *buf) {
    Bloque *bl = static_cast<Bloque *>(data);
    if (bl) bl->libre = true;   /* ya la puede reutilizar el siguiente cuadro */
    (void)buf;
  }

  static const struct wl_buffer_listener bufferListener;

  void liberaBloques(bool todos) {
    bool guardado = false;
    for (size_t i = 0; i < bloques.size();) {
      Bloque *bl = bloques[i];
      if (!todos && (!bl->libre || (bl->ancho == bufAncho && bl->alto == bufAlto && !guardado))) {
        if (bl->libre) guardado = true;   /* uno libre del tamano bueno se queda para el cuadro que viene */
        ++i;
        continue;
      }
      if (bl->buf) wl_buffer_destroy(bl->buf);
      if (bl->pozo) wl_shm_pool_destroy(bl->pozo);
      if (bl->mapa) ::munmap(bl->mapa, bl->tam);
      if (bl->fd >= 0) ::close(bl->fd);
      delete bl;
      bloques.erase(bloques.begin() + (long)i);
    }
  }

  /* ---------- dibujo de la tarjeta ---------- */

  /* Alto del escritorio en puntos logicos. El monitor es 3840x2160 fisicos pero con la escala
     del escritorio al 150% el escritorio mide 1440 de alto: la tarjeta se mide ahi, si no sale
     un 50% mas grande que cualquier ventana normal. */
  int regionCuadrada[4] = {-1, -1, -1, -1};  /* la ultima caja de entrada mandada a la capa */

  float altoLogico() const { return (float)altoE / (escala > 0.25 ? escala : 1.0); }

  /* El lienzo de la capa llega hasta el borde de la pantalla y lo que no es tarjeta es
     transparente, pero sin decirle nada al compositor se pica igual: se tragaba los clics y la
     rueda que caian encima. La region de entrada deja la capa pinchable solo sobre la tarjeta; el
     resto del lienzo deja pasar el raton a la ventana de atras. Las coordenadas son logicas del
     lienzo (pixeles reales partido por la escala que anuncio el compositor). */
  void poneRegionEntrada() {
    if (!surf || !comp) return;
    float ax = 0.0f, ay = 0.0f, aw = 0.0f, ah = 0.0f;
    const double e = escala > 0.25 ? escala : 1.0;
    int caja[4] = {0, 0, 0, 0};
    if (tarjetaPuesta && hud.flotanteBounds(&ax, &ay, &aw, &ah)) {
      caja[0] = (int)std::floor((ax - (float)ox) / e);
      caja[1] = (int)std::floor((ay - (float)oy) / e);
      caja[2] = (int)std::ceil(aw / e);
      caja[3] = (int)std::ceil(ah / e);
    }
    if (caja[0] == regionCuadrada[0] && caja[1] == regionCuadrada[1] && caja[2] == regionCuadrada[2] &&
        caja[3] == regionCuadrada[3])
      return;  /* la misma caja que el cuadro anterior: no hay nada que mandar */
    struct wl_region *r = wl_compositor_create_region(comp);
    if (!r) return;
    if (caja[2] > 0 && caja[3] > 0) wl_region_add(r, caja[0], caja[1], caja[2], caja[3]);
    wl_surface_set_input_region(surf, r);  /* caja vacia = aqui no se pica nada: pasa todo */
#ifdef HAVE_KWIN_BLUR
    /* y la misma caja, desenfocada por detras (v0.91): la pastilla y el panel son translucidos y el
       video se ve a traves; con el desenfoque de KWin se leen mejor y parecen de cristal. Se manda
       con el siguiente commit de la superficie. */
    if (blurMgr) {
      if (!blur) blur = org_kde_kwin_blur_manager_create(blurMgr, surf);
      if (blur) {
        org_kde_kwin_blur_set_region(blur, (caja[2] > 0 && caja[3] > 0) ? r : nullptr);
        org_kde_kwin_blur_commit(blur);
      }
    }
#endif
    wl_region_destroy(r);
    std::printf("[capa] region de entrada: %s\n",
                (caja[2] > 0 && caja[3] > 0) ? "solo la tarjeta" : "ninguna (el aviso no esta puesto)");
    std::fflush(stdout);
    regionCuadrada[0] = caja[0]; regionCuadrada[1] = caja[1];
    regionCuadrada[2] = caja[2]; regionCuadrada[3] = caja[3];
  }

  void pintar() {
    if (bufAncho <= 0 || bufAlto <= 0) return;
    if (supAncho <= 0) supAncho = bufAncho;
    if (supAlto <= 0) supAlto = bufAlto;
    liberaBloques(false);
    /* la disposición es determinista: misma ventana, misma tarjeta, mismos píxeles */
    hud.begin(anchoE, altoE, altoLogico());
    if (tarjetaPuesta && (std::fabs(hud.scale() - escalaGeom) > 0.01 || std::fabs(escala - escalaMonGeom) > 0.001)) {
      /* el compositor anuncio su escala despues de montar la ventana: la tarjeta se dibuja
         de otro tamano y hay que volver a recortar, si no la letra se sale de las cajas */
      std::printf("[capa] la escala del compositor paso de %.2f a %.2f: la tarjeta se reajusta\n",
                  escalaGeom, hud.scale());
      ajustaGeometria();
    }
    if (entornoOdyssey("ODYSSEYVK_DEBUG_UI"))
      std::printf("[depurar capa] al dibujar: hud %.2f | recorte ox=%.0f buf=%dx%d\n",
                  hud.scale(), ox, bufAncho, bufAlto);
    const size_t npx = (size_t)bufAncho * (size_t)bufAlto;
    static const bool medirPintado = entornoOdyssey("ODYSSEYVK_DEBUG_UI") != nullptr;
    const auto tP0 = std::chrono::steady_clock::now();
    hud.pintarCpu((float)ox, (float)oy, bufAncho, bufAlto, acum);   /* el mismo pintado que `ctl hud --png` */
    const auto tP1 = std::chrono::steady_clock::now();

    /* ARGB8888 sin premultiplicar (así lo pide wl_shm) */
    Bloque *bl = nullptr;
    std::string err;
    for (Bloque *candidato : bloques)
      if (candidato->libre && candidato->ancho == bufAncho && candidato->alto == bufAlto) {
        bl = candidato;
        break;
      }
    if (!bl) {
      /* si cambió el tamaño de la tarjeta, los lienzos viejos ya no sirven */
      liberaBloques(false);   /* sueltos pero del tamaño viejo: fuera */
      bl = nuevoBloque(shm, bufAncho, bufAlto, &err);
      if (!bl) {
        aviso = err;
        return;
      }
      bloques.push_back(bl);
    }
    uint32_t *pinta = reinterpret_cast<uint32_t *>(bl->mapa);
    const float *fuente = acum.data();
    /* (v1.16) mas deprisa y en varios hilos: MEDIDO en 4K a pantalla completa, esta vuelta costaba 50-55 ms por cuadro
       (cuatro std::lround y cuatro productos por punto, y con el velo todos los puntos pasan por aqui), y era lo que
       hacia lenta la animacion del panel sobre un video. El redondeo (x + 0,5 truncado) da lo mismo que lround para
       valores positivos */
    auto convierte = [pinta, fuente](size_t desde, size_t hasta) {
      for (size_t p = desde; p < hasta; p++) {
        const float A = fuente[p * 4u + 3u];
        if (A <= 0.0001f) {
          pinta[p] = 0u;
          continue;
        }
        const float k = 255.0f / A;
        auto canal = [k](float c) -> uint32_t {
          const float v = c * k;
          return v <= 0.0f ? 0u : (v >= 255.0f ? 255u : (uint32_t)(v + 0.5f));
        };
        const uint32_t aa = A >= 1.0f ? 255u : (uint32_t)(A * 255.0f + 0.5f);
        pinta[p] = (aa << 24) | (canal(fuente[p * 4u]) << 16) | (canal(fuente[p * 4u + 1u]) << 8) | canal(fuente[p * 4u + 2u]);
      }
    };
    const unsigned hilos = npx >= 1000000u ? std::max(1u, std::min(8u, std::thread::hardware_concurrency())) : 1u;
    if (hilos <= 1) {
      convierte(0, npx);
    } else {
      std::vector<std::thread> trabajo;
      const size_t trozo = (npx + hilos - 1) / hilos;
      for (unsigned h = 1; h < hilos; h++) trabajo.emplace_back(convierte, std::min(npx, h * trozo), std::min(npx, (h + 1) * trozo));
      convierte(0, std::min(npx, trozo));
      for (std::thread &t : trabajo) t.join();
    }

    if (surf && !(layer && !hayConfigure)) {
      /* enganchar un lienzo antes de la primera confirmacion de tamano es un error de
         protocolo: el compositor cierra la conexion de la capa y desde ahi la tarjeta ya no
         vuelve a aparecer nunca. Pasaba al rehacer la capa, que es lo que dejaba el aviso
         "tarjeta puesta" en el registro pero nada en la pantalla. */
      poneRegionEntrada();  /* va en el mismo envio que el lienzo */
      wl_surface_attach(surf, bl->buf, 0, 0);
      wl_surface_damage(surf, 0, 0, supAncho, supAlto);
      wl_surface_commit(surf);
    }
    if (medirPintado) {
      const auto tP2 = std::chrono::steady_clock::now();
      std::printf("[depurar capa] cuadro de %dx%d: pintado %.1f ms, conversion y envio %.1f ms\n", bufAncho, bufAlto,
                  std::chrono::duration<double, std::milli>(tP1 - tP0).count(), std::chrono::duration<double, std::milli>(tP2 - tP1).count());
    }
  }

  /* ---------- colocación de la tarjeta ---------- */

  /* Recorte del lienzo y tamano de la ventana de la tarjeta, sacados de la disposicion que
     calculo el hud. Se vuelve a llamar al dibujar: el compositor anuncia su escala despues de
     montar la ventana (aqui paso de 1,50 a 1,80) y con la escala nueva la tarjeta se dibuja
     mas grande; si el recorte se queda con la escala vieja, la letra sale cortada por la
     izquierda y se sale de sus cajas. */
  bool ajustaGeometria() {
    float ax = 0, ay = 0, aw = 0, ah = 0;
    if (!hud.flotanteBounds(&ax, &ay, &aw, &ah)) return false;
    const float holgura = 4.0f * (float)std::max(1.0, escala);   /* para el borde suave */
    ax -= holgura;
    ay -= holgura;
    aw += 2.0f * holgura;
    ah += 2.0f * holgura;
    if (ax < 0.0f) {
      aw += ax;
      ax = 0.0f;
    }
    if (ay < 0.0f) {
      ah += ay;
      ay = 0.0f;
    }
    aw = std::min(aw, (float)anchoE - ax);
    ah = std::min(ah, (float)altoE - ay);
    /* El lienzo llega hasta el borde de la pantalla y la separacion con el borde se deja
       DENTRO del lienzo, en zona transparente. Con `set_margin` el compositor multiplica el
       margen por su escala, que no siempre coincide con la nuestra, y el sobrante se lo comia
       a la tarjeta: salia encogida y con la letra fuera de las cajas. */
    /* El panel de ajustes va en el centro y su lienzo es justo su caja; la tarjeta va en la esquina
       y su lienzo llega hasta el borde de la pantalla (lo de aqui abajo). */
    const bool esPanel = hud.state().panel;
    const int nAncho = (int)std::lround(esPanel ? aw : (float)anchoE - ax);
    const int nAlto = (int)std::lround(esPanel ? ah : (float)altoE - ay);
    if (nAncho < 16 || nAlto < 16) return false;
    const bool cambio = (nAncho != bufAncho || nAlto != bufAlto);
    ox = ax;
    oy = ay;
    bufAncho = nAncho;
    bufAlto = nAlto;
    escalaGeom = hud.scale();
    escalaMonGeom = escala;
    if (esPanel)
      std::printf("[capa] panel: lienzo %dx%d px · escala del monitor %.2f · dibujo a %.2f\n", nAncho, nAlto, escala,
                  hud.scale());
    margenDer = margenAb = 0;
    supAncho = aQuiere((double)bufAncho);
    supAlto = aQuiere((double)bufAlto);
    if (layer && cambio) {
      zwlr_layer_surface_v1_set_size(layer, (uint32_t)supAncho, (uint32_t)supAlto);
#ifdef HAVE_FRACTIONAL_SCALE
      if (viewport) wp_viewport_set_destination(viewport, supAncho, supAlto);
#endif
    }
    return true;
  }

  int32_t aQuiere(double pixelesReales) const {
    return (int32_t)std::lround(pixelesReales / std::max(0.25, escala));
  }

  void quitaEscuchaLayer() {
    if (layer) {
      zwlr_layer_surface_v1_destroy(layer);
      layer = nullptr;
      capaEn = nullptr;
    }
    ofrecidoAncho = ofrecidoAlto = 0;
    hayConfigure = false;   /* la capa nueva tiene que esperar su propia confirmacion */
  }

  void limpiaTarjeta() {
    quitaEscuchaLayer();
    if (surf) {
      wl_surface_attach(surf, nullptr, 0, 0);
      wl_surface_commit(surf);
    }
    tarjetaPuesta = false;
  }

  static std::string minusc(std::string v) {
    for (char &c : v) c = (char)std::tolower(c);
    return v;
  }
  /* El panel del Odyssey es el único que puede hacer 3D, así que la tarjeta tiene que
     caer SIEMPRE ahí. Bastaba "el monitor de más píxeles" mientras solo había uno, pero
     con dos pantallas conectadas (o cuando el compositor vuelve a anunciar la salida
     después de cambiar el modo de la lente) eso ponía la tarjeta en el monitor
     equivocado. Estas palabras reconocen el panel; si no cuadran, se arregla con
     pantalla=DP-4 en ~/.config/odysseyvk/serve.conf o con ODYSSEYVK_PANTALLA=DP-4. */
  static bool esPanelOdyssey(const Monitor &m) {
    const std::string n = minusc(m.nombre), d = minusc(m.modelo);
    for (const char *aguja : {"odyssey", "syncmaster", "g90xf", "g90xc", "g90"})
      if (d.find(aguja) != std::string::npos || n.find(aguja) != std::string::npos) return true;
    return false;
  }
  static long areaDe(const Monitor &m) {
    const long a = m.anchoFis > 0 ? m.anchoFis : m.ancho;
    const long b = m.altoFis > 0 ? m.altoFis : m.alto;
    return a * b;
  }

  void eligeMonitor() {
    if (pistaMonitor.empty()) {
      if (const char *e = entornoOdyssey("ODYSSEYVK_PANTALLA"); e && e[0]) pistaMonitor = e;
    }
    Monitor *porPista = nullptr, *porPanel = nullptr, *elGrande = nullptr;
    for (Monitor *m : monitores) {
      if (!m->vivo || !m->obj) continue;
      if (!elGrande || areaDe(*m) > areaDe(*elGrande)) elGrande = m;
      if (esPanelOdyssey(*m) && (!porPanel || areaDe(*m) > areaDe(*porPanel))) porPanel = m;
      if (!pistaMonitor.empty()) {
        const std::string bus = minusc(m->nombre), aguja = minusc(pistaMonitor);
        if (bus.find(aguja) != std::string::npos || minusc(m->modelo).find(aguja) != std::string::npos)
          if (!porPista || areaDe(*m) > areaDe(*porPista)) porPista = m;
      }
    }
    /* la pista manda; si no coincide con nada (la pantalla se acaba de anunciar y todavia
       no trae nombre) se sigue con el panel Odyssey o con el de mas pixeles, y nunca con el
       primero de la lista: asi era como la tarjeta se colaba en el otro monitor */
    elegido = porPista ? porPista : (porPanel ? porPanel : elGrande);
    reconocido = porPista || porPanel;
    if (!elegido && !monitores.empty()) {
      for (Monitor *m : monitores)
        if (m->obj) { elegido = m; break; }
    }
    if (elegido) {
      anchoE = elegido->anchoFis > 0 ? elegido->anchoFis : elegido->ancho;
      altoE = elegido->altoFis > 0 ? elegido->altoFis : elegido->alto;
      /* La escala real es píxeles reales entre píxeles lógicos: con el escritorio al
         150 % el wl_output anuncia la entera de arriba (2) y todo saldría descuadrado. */
      escala = 0.0;
      if (elegido->anchoLog > 0 && elegido->anchoFis > 0)
        escala = (double)elegido->anchoFis / (double)elegido->anchoLog;
      if (escala <= 0.25) escala = (double)std::max(1, elegido->escala);
    }
  }

  /* un error de protocolo (tamano o lienzo mal puestos) cierra la conexion del compositor:
     sin avisar de esto la tarjeta "se pone" segun el registro pero no vuelve a aparecer
     jamas, que es exactamente lo que pasaba cuando la tarjeta dejaba de salir */
  void marcaConexionCaida() {
    if (conexionCaida) return;
    conexionCaida = true;
    aviso = "el compositor cerró la conexión de la capa superpuesta";
  }

  int esperar(int ms) {
    if (!dpy) return -1;
    if (wl_display_get_error(dpy) != 0) {
      marcaConexionCaida();
      return -1;
    }
    while (wl_display_prepare_read(dpy) != 0)
      if (wl_display_dispatch_pending(dpy) < 0) {
        marcaConexionCaida();
        return -1;
      }
    wl_display_flush(dpy);
    struct pollfd p{wl_display_get_fd(dpy), POLLIN, 0};
    const int n = ::poll(&p, 1, ms);
    if (n > 0) {
      wl_display_read_events(dpy);
      return wl_display_dispatch_pending(dpy);
    }
    wl_display_cancel_read(dpy);
    return 0;
  }

  /* ---------- escuchas ---------- */

  static void registryGlobal(void *data, wl_registry *reg, uint32_t name, const char *iface,
                             uint32_t version) {
    Impl *s = static_cast<Impl *>(data);
    if (!std::strcmp(iface, wl_compositor_interface.name))
      s->comp = static_cast<wl_compositor *>(
          wl_registry_bind(reg, name, &wl_compositor_interface, std::min<uint32_t>(version, 4)));
    else if (!std::strcmp(iface, wl_shm_interface.name))
      s->shm = static_cast<wl_shm *>(wl_registry_bind(reg, name, &wl_shm_interface, std::min<uint32_t>(version, 1)));
    else if (!std::strcmp(iface, xdg_wm_base_interface.name))
      s->wmBase = static_cast<xdg_wm_base *>(wl_registry_bind(reg, name, &xdg_wm_base_interface, std::min<uint32_t>(version, 2)));
    else if (!std::strcmp(iface, zwlr_layer_shell_v1_interface.name)) {
      s->shell = static_cast<zwlr_layer_shell_v1 *>(
          wl_registry_bind(reg, name, &zwlr_layer_shell_v1_interface, std::min<uint32_t>(version, 4)));
      s->capaVersion = std::min<uint32_t>(version, 4);   /* la v4 trae el teclado «a pedido» (panel fijado) */
    }
    else if (!std::strcmp(iface, wl_seat_interface.name)) {
      s->seat = static_cast<wl_seat *>(wl_registry_bind(reg, name, &wl_seat_interface, std::min<uint32_t>(version, 6)));
      if (s->seat) {
        s->pointer = wl_seat_get_pointer(s->seat);
        if (s->pointer) wl_pointer_add_listener(s->pointer, &s->pointerListener, s);
        s->keyboard = wl_seat_get_keyboard(s->seat);
        if (s->keyboard) wl_keyboard_add_listener(s->keyboard, &s->keyboardListener, s);
      }
    } else if (!std::strcmp(iface, zxdg_output_manager_v1_interface.name)) {
      s->xdgMgr = static_cast<zxdg_output_manager_v1 *>(
          wl_registry_bind(reg, name, &zxdg_output_manager_v1_interface, std::min<uint32_t>(version, 2)));
    } else if (!std::strcmp(iface, wl_output_interface.name)) {
      Monitor *m = new Monitor();
      m->global = name;
      m->obj = static_cast<wl_output *>(
          wl_registry_bind(reg, name, &wl_output_interface, std::min<uint32_t>(version, 4)));
      if (m->obj) wl_output_add_listener(m->obj, &s->outputListener, m);
      /* una pantalla anunciada tardé (otra conectada, un cambio de modo de la lente):
         su tamaño lógico se pide ahora mismo, si no la tarjeta saldría a la escala mala */
      if (m->obj && s->xdgMgr && !m->xdg) {
        m->xdg = zxdg_output_manager_v1_get_xdg_output(s->xdgMgr, m->obj);
        if (m->xdg) zxdg_output_v1_add_listener(m->xdg, &s->xdgListener, m);
      }
      s->monitores.push_back(m);
    }
#ifdef HAVE_KWIN_BLUR
    else if (!std::strcmp(iface, org_kde_kwin_blur_manager_interface.name))
      s->blurMgr = static_cast<org_kde_kwin_blur_manager *>(
          wl_registry_bind(reg, name, &org_kde_kwin_blur_manager_interface, std::min<uint32_t>(version, 1)));
#endif
#ifdef HAVE_FRACTIONAL_SCALE
    else if (!std::strcmp(iface, wp_fractional_scale_manager_v1_interface.name))
      s->escalaMgr = static_cast<wp_fractional_scale_manager_v1 *>(wl_registry_bind(
          reg, name, &wp_fractional_scale_manager_v1_interface, std::min<uint32_t>(version, 1)));
    else if (!std::strcmp(iface, wp_viewporter_interface.name))
      s->viewporter = static_cast<wp_viewporter *>(
          wl_registry_bind(reg, name, &wp_viewporter_interface, std::min<uint32_t>(version, 1)));
#endif
  }
  static void registryGlobalRemove(void *data, wl_registry *, uint32_t name) {
    Impl *s = static_cast<Impl *>(data);
    for (Monitor *m : s->monitores) {
      if (m->global != name) continue;
      /* el compositor ya destruyó el objeto del lado del servidor: el cliente tiene que
         soltar el suyo y dejar de contar esa pantalla */
      m->vivo = false;
      const bool capaAqui = s->capaEn && m->obj && s->capaEn == m->obj;
      if (m->obj) { wl_output_destroy(m->obj); m->obj = nullptr; }
      if (s->elegido == m) s->elegido = nullptr;
      if (capaAqui) {
        /* la capa estaba puesta en esa pantalla: se quita y en el siguiente aviso se
           pondrá en la que siga en pie */
        s->quitaEscuchaLayer();
        if (s->layer) { zwlr_layer_surface_v1_destroy(s->layer); s->layer = nullptr; }
        s->capaEn = nullptr;
        s->tarjetaPuesta = false;
      }
      return;
    }
  }

  static const struct wl_registry_listener registryListener;

  static void outGeometry(void *data, wl_output *, int32_t x, int32_t y, int32_t, int32_t, int32_t,
                          const char *, const char *, int32_t transform) {
    Monitor *m = static_cast<Monitor *>(data);
    m->x = x;
    m->y = y;
    m->transformacion = (uint32_t)transform;
  }
  static void outMode(void *data, wl_output *, uint32_t, int32_t w, int32_t h, int32_t) {
    Monitor *m = static_cast<Monitor *>(data);
    if ((long)w * (long)h > (long)m->anchoFis * (long)m->altoFis) {
      m->anchoFis = (int)w;
      m->altoFis = (int)h;
    }
  }
  static void outDone(void *data, wl_output *) {
    Monitor *m = static_cast<Monitor *>(data);
    m->vivo = true;
  }
  static void outScale(void *data, wl_output *, int32_t f) {
    Monitor *m = static_cast<Monitor *>(data);
    m->escala = f > 0 ? (int)f : 1;
  }
  static void outName(void *data, wl_output *, const char *nombre) {
    Monitor *m = static_cast<Monitor *>(data);
    if (nombre) m->nombre = nombre;
  }
  static void outDescription(void *data, wl_output *, const char *desc) {
    Monitor *m = static_cast<Monitor *>(data);
    if (desc) m->modelo = desc;
  }

  static void xdgPosicion(void *data, zxdg_output_v1 *, int32_t x, int32_t y) {
    Monitor *m = static_cast<Monitor *>(data);
    m->x = x;
    m->y = y;
  }
  static void xdgTamano(void *data, zxdg_output_v1 *, int32_t w, int32_t h) {
    Monitor *m = static_cast<Monitor *>(data);
    m->anchoLog = (int)w;
    m->altoLog = (int)h;
  }
  static void xdgNombre(void *data, zxdg_output_v1 *, const char *nombre) {
    Monitor *m = static_cast<Monitor *>(data);
    if (nombre && m->nombre.empty()) m->nombre = nombre;
  }
  static void xdgDescripcion(void *data, zxdg_output_v1 *, const char *desc) {
    Monitor *m = static_cast<Monitor *>(data);
    if (desc && m->modelo.empty()) m->modelo = desc;
  }
  static void xdgTermina(void *data, zxdg_output_v1 *) {
    /* ya llegaron todos los datos de este monitor */
    Monitor *m = static_cast<Monitor *>(data);
    (void)m;
  }

  static const struct zxdg_output_v1_listener xdgListener;
  static const struct wl_output_listener outputListener;

  static void layerConfigure(void *data, zwlr_layer_surface_v1 *, uint32_t serial, uint32_t w, uint32_t h) {
    Impl *s = static_cast<Impl *>(data);
    s->ofrecidoAncho = (int)w;
    s->ofrecidoAlto = (int)h;
    /* Lo que ofrece el compositor es el area util de la pantalla donde puso la capa, no el
       tamano de la tarjeta, y ademas lo da en SU escala (aqui ofrecio 668x224 para una
       ventana nuestra de 800x267: los mismos pixeles contados con escala 1,80 en vez de
       1,50). Recortarse a esa cifra encogia la tarjeta y la letra salia fuera de sus cajas,
       asi que solo se guarda como huella de la pantalla y no se toca el tamano pedido. */
    (void)w;
    (void)h;
    s->hayConfigure = true;
    zwlr_layer_surface_v1_ack_configure(s->layer, serial);
#ifdef HAVE_FRACTIONAL_SCALE
    if (s->viewport && s->supAncho > 0 && s->supAlto > 0)
      wp_viewport_set_destination(s->viewport, s->supAncho, s->supAlto);
#endif
  }
  static void layerClosed(void *data, zwlr_layer_surface_v1 *) {
    Impl *s = static_cast<Impl *>(data);
    s->layer = nullptr;   /* el compositor ya lo destruyó */
    s->tarjetaPuesta = false;
    s->cerrada = true;
  }

  static const struct zwlr_layer_surface_v1_listener layerListener;

#ifdef HAVE_FRACTIONAL_SCALE
  static void escalaSuperficie(void *data, wp_fractional_scale_v1 *, uint32_t nueva) {
    Impl *s = static_cast<Impl *>(data);
    /* el protocolo la manda en 120 avos (150 % = 180), no en centesimos: con /100 salia 1,80 en un monitor al 150 % y el
       raton del panel caia desfasado, mas cuanto mas abajo a la derecha (Rodolfo: «mouse desfasado en el escritorio») */
    s->escala = (double)nueva / 120.0;
  }
  static const struct wp_fractional_scale_v1_listener escalaListener;
#endif

  static void ptrEnter(void *data, wl_pointer *, uint32_t serial, wl_surface *sobre, wl_fixed_t sx, wl_fixed_t sy) {
    Impl *s = static_cast<Impl *>(data);
    s->serialPuntero = serial;
    s->cursorEsquina = false;
    s->ponCursor(serial);
    if (sobre && sobre == s->veloSurf) {   /* el velo: fuera del panel, nada se resalta */
      s->ratonEnVelo = true;
      return;
    }
    s->ratonEnVelo = false;
    s->ratonDentro = true;
    s->ratonX = s->ox + wl_fixed_to_double(sx) * s->escala;
    s->ratonY = s->oy + wl_fixed_to_double(sy) * s->escala;
    s->hud.setMouse((float)s->ratonX, (float)s->ratonY);
  }
  static void ptrLeave(void *data, wl_pointer *, uint32_t, wl_surface *sobre) {
    Impl *s = static_cast<Impl *>(data);
    if (sobre && sobre == s->veloSurf) {
      s->ratonEnVelo = false;
      return;
    }
    s->ratonDentro = false;
    s->ratonX = s->ratonY = -1.0;
    s->hud.setMouse(-1.0f, -1.0f);
    if (s->tarjetaPuesta && s->hud.state().panel && s->hud.state().arrastrando < 0) {
      s->firmaVista = -1;
      s->pintar();   /* se apaga el realce de lo que habia bajo el raton */
      return;
    }
    if (s->tarjetaPuesta && s->golpeVisto != Hud::HitNone) {
      s->golpeVisto = Hud::HitNone;
      s->pintar();
    }
  }
  void panelMovimiento() {
    /* cambiar el tamano: la capa no se mueve (sus margenes se quedan), asi que el raton se mide siempre
       contra la misma esquina de arriba a la izquierda; el panel crece hacia la derecha y hacia abajo */
    if (redimensionando && layer) {
      struct timespec ts;
      clock_gettime(CLOCK_MONOTONIC, &ts);
      const double ahora = (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
      if (esperaMov && ahora - tMov < 0.08) return;
      const double k = escala / std::max(0.25, (double)hud.scale());   /* puntos logicos -> puntos del lienzo */
      const float w = agarreAncho + (float)((supX - agarreX) * k), h = agarreAlto + (float)((supY - agarreY) * k);
      if (std::fabs(w - hud.state().panelAncho) < 2.0f && std::fabs(h - hud.state().panelAlto) < 2.0f) return;
      hud.state().panelAncho = w, hud.state().panelAlto = h;
      esperaMov = true;
      tMov = ahora;
      if (wl_callback *cb = wl_surface_frame(surf)) wl_callback_add_listener(cb, &cuadroMovListener, this);
      hud.begin(anchoE, altoE, altoLogico());
      if (ajustaGeometria()) pintar();
      return;
    }
    if (moviendo && layer) {
      /* la capa se mueve con el raton, asi que la posicion del raton DENTRO de ella no cambia: lo
         que se haya movido respecto al agarre es justo lo que hay que correr.
         Pero el compositor tarda un cuadro en mover la capa, y los movimientos del raton que llegan
         mientras tanto vienen medidos contra el sitio VIEJO: sumarlos otra vez corria el panel dos
         veces y daba un tiron (lo vio Rodolfo con la 0.86). Despues de cada corrimiento se espera a
         que el compositor pinte ese cuadro (o 80 ms, por si no avisa) antes de mirar otro. */
      struct timespec ts;
      clock_gettime(CLOCK_MONOTONIC, &ts);
      const double ahora = (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
      if (esperaMov && ahora - tMov < 0.08) return;
      if (std::lround(supX - agarreX) == 0 && std::lround(supY - agarreY) == 0) return;
      esperaMov = true;
      tMov = ahora;
      if (wl_callback *cb = wl_surface_frame(surf)) wl_callback_add_listener(cb, &cuadroMovListener, this);
      margenIzq = std::max(0, std::min(std::max(0, logAncho - 80), margenIzq + (int)std::lround(supX - agarreX)));
      margenArr = std::max(0, std::min(std::max(0, logAlto - 60), margenArr + (int)std::lround(supY - agarreY)));
      zwlr_layer_surface_v1_set_margin(layer, margenArr, 0, 0, margenIzq);
      wl_surface_commit(surf);
      return;
    }
    const int arrastrando = hud.state().arrastrando;
    if (arrastrando >= 0) {
      const float f = hud.fracDeslizador(arrastrando, (float)ratonX);
      if ((size_t)arrastrando < hud.state().items.size()) hud.state().items[(size_t)arrastrando].frac = f;
      golpes.push_back({(int)Hud::HitPanelItem, arrastrando, 0, f, false});
      sucio = true;
      return;
    }
    int item = -1, sub = 0;
    float fr = 0.0f;
    const Hud::Hit h = hud.hitDetalle((float)ratonX, (float)ratonY, &item, &sub, &fr);
    const long firma = (long)h * 1000000L + (long)(item + 1) * 1000L + sub;
    if (firma != firmaVista) {   /* solo se repinta si cambia lo que hay bajo el raton */
      firmaVista = firma;
      sucio = true;
    }
  }
  void panelBoton(bool apretado) {
    int &arrastrando = hud.state().arrastrando;
    if (!apretado) {
      if (moviendo || redimensionando) guardaVentana();
      moviendo = redimensionando = false;
      if (arrastrando >= 0 && (size_t)arrastrando < hud.state().items.size()) {
        /* (v1.16, PR #1) se guarda donde se ve la bola (la lista pudo rehacerse mientras tanto) */
        golpes.push_back({(int)Hud::HitPanelItem, arrastrando, 0, hud.state().items[(size_t)arrastrando].frac, true});
        sucio = true;
      }
      arrastrando = -1;
      return;
    }
    int item = -1, sub = 0;
    float fr = 0.0f;
    const Hud::Hit h = hud.hitDetalle((float)ratonX, (float)ratonY, &item, &sub, &fr);
    if (h == Hud::HitPanelItem && sub == 0 && item >= 0 && (size_t)item < hud.state().items.size() &&
        hud.state().items[(size_t)item].tipo == Hud::ItemPanel::Deslizador) {   /* (v1.16) sub 1: su flechita */
      arrastrando = item;
      hud.state().items[(size_t)item].frac = fr;
      golpes.push_back({(int)h, item, 0, fr, false});
      sucio = true;
      return;
    }
    if (h == Hud::HitPanelTitulo) {
      moviendo = true;
      agarreX = supX, agarreY = supY;
      return;
    }
    if (h == Hud::HitPanelRedimensionar) {   /* la esquina: cambiar el tamano (rama experimental) */
      redimensionando = true;
      agarreX = supX, agarreY = supY;
      agarreAncho = hud.panelAnchoVisto(), agarreAlto = hud.panelAltoVisto();
      return;
    }
    /* los botones de la cabecera tambien (el globo, su lista y la flecha de actualizar no llegaban nunca
       al bucle del panel; el pin es de la rama experimental), y cualquier otro clic con la lista de
       idiomas abierta, que la cierra */
    if (h == Hud::HitPanelCerrar || h == Hud::HitPanelPestana || h == Hud::HitPanelItem || h == Hud::HitPanelIdioma ||
        h == Hud::HitPanelIdiomaOpcion || h == Hud::HitPanelActualizar || h == Hud::HitPanelFijar ||
        h == Hud::HitPanelModoRed || h == Hud::HitPanelTesela ||   /* (v1.16) las teselas chicas del panel nuevo */
        h == Hud::HitPanelCopiar ||   /* (v1.16) el Copiar del pie de Juegos: desde la 1.14 no llegaba */
        h == Hud::HitPanelVolver ||   /* (v1.16) la flecha de volver a la pastilla */
        (hud.state().menuIdioma && (h == Hud::HitPanel || h == Hud::HitPanelTitulo)))
      golpes.push_back({(int)h, item, sub, fr, true});
  }
  static void ptrMotion(void *data, wl_pointer *, uint32_t, wl_fixed_t sx, wl_fixed_t sy) {
    Impl *s = static_cast<Impl *>(data);
    if (!s->tarjetaPuesta || s->ratonEnVelo) return;
    s->ratonX = s->ox + wl_fixed_to_double(sx) * s->escala;
    s->ratonY = s->oy + wl_fixed_to_double(sy) * s->escala;
    s->hud.setMouse((float)s->ratonX, (float)s->ratonY);
    s->supX = wl_fixed_to_double(sx), s->supY = wl_fixed_to_double(sy);
    if (s->hud.state().panel) {
      const bool esquina = s->redimensionando || s->hud.hit((float)s->ratonX, (float)s->ratonY) == Hud::HitPanelRedimensionar;
      if (esquina != s->cursorEsquina) {
        s->cursorEsquina = esquina;
        s->ponCursor(s->serialPuntero, esquina ? "se-resize" : nullptr);
      }
      s->panelMovimiento();
      return;
    }
    /* sólo se redibuja si cambia el botón que hay bajo el ratón */
    const Hud::Hit h = s->hud.hit((float)s->ratonX, (float)s->ratonY);
    if (h != s->golpeVisto) {
      s->golpeVisto = h;
      s->pintar();
    }
  }
  static void ptrButton(void *data, wl_pointer *, uint32_t, uint32_t, uint32_t boton, uint32_t estado) {
    Impl *s = static_cast<Impl *>(data);
    if (s->ratonEnVelo) {   /* un clic fuera del panel: quien lo ensena lo cierra */
      if (estado == WL_POINTER_BUTTON_STATE_PRESSED) s->golpes.push_back({Overlay::kClicFuera, -1, 0, 0.0f, true});
      return;
    }
    if (s->tarjetaPuesta && s->hud.state().panel) {
      if (boton == BTN_LEFT) s->panelBoton(estado == WL_POINTER_BUTTON_STATE_PRESSED);
      return;
    }
    if (boton != BTN_LEFT || estado != WL_POINTER_BUTTON_STATE_PRESSED || !s->tarjetaPuesta) return;
    const Hud::Hit h = s->hud.hit((float)s->ratonX, (float)s->ratonY);
    switch (h) {
      case Hud::HitAskSbs: s->accion = "sbs"; break;
      case Hud::HitAskTop: s->accion = "tb"; break;
      case Hud::HitAskFlat: s->accion = "flat"; break;
      case Hud::HitAskConv: s->accion = "conv"; break;
      case Hud::HitAskClose: s->accion = "close"; break;
      case Hud::HitAskActualizar: s->accion = "actualizar"; break;   /* la tarjeta de la actualizacion (v0.95) */
      case Hud::HitAskLuego: s->accion = "luego"; break;
      case Hud::HitAskNoAvisar: s->accion = "noavisar"; break;
      case Hud::HitAskDont:
        s->hud.state().remember = !s->hud.state().remember;
        s->pintar();
        break;
      default: break;
    }
  }
  static void ptrAxis(void *data, wl_pointer *, uint32_t, uint32_t eje, wl_fixed_t v) {
    /* la rueda corre la lista del panel de ajustes (eje 0 = vertical) */
    Impl *s = static_cast<Impl *>(data);
    if (!s->tarjetaPuesta || !s->hud.state().panel || eje != 0 || s->ratonEnVelo) return;
    s->hud.correr((float)wl_fixed_to_double(v) * 2.5f);
    s->firmaVista = -1;
    s->sucio = true;
  }
  static void ptrFrame(void *, wl_pointer *) {}
  static void ptrAxisSource(void *, wl_pointer *, uint32_t) {}
  static void ptrAxisStop(void *, wl_pointer *, uint32_t, uint32_t) {}
  static void ptrAxisDiscrete(void *, wl_pointer *, uint32_t, int32_t) {}

  static const struct wl_pointer_listener pointerListener;

  /* ---- teclado ---- */
  static void kbKeymap(void *data, wl_keyboard *, uint32_t formato, int32_t fd, uint32_t tam) {
#ifdef HAVE_XKBCOMMON
    Impl *s = static_cast<Impl *>(data);
    if (formato == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
      void *m = mmap(nullptr, tam, PROT_READ, MAP_PRIVATE, fd, 0);
      if (m != MAP_FAILED) {
        if (!s->xkbCtx) s->xkbCtx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        xkb_keymap *km = s->xkbCtx ? xkb_keymap_new_from_string(s->xkbCtx, (const char *)m, XKB_KEYMAP_FORMAT_TEXT_V1,
                                                                XKB_KEYMAP_COMPILE_NO_FLAGS)
                                   : nullptr;
        munmap(m, tam);
        if (km) {
          if (s->xkbEstado) xkb_state_unref(s->xkbEstado);
          if (s->xkbMapa) xkb_keymap_unref(s->xkbMapa);
          s->xkbMapa = km;
          s->xkbEstado = xkb_state_new(km);
          if (!s->composeTabla) {
            const char *loc = std::getenv("LC_ALL");
            if (!loc || !*loc) loc = std::getenv("LC_CTYPE");
            if (!loc || !*loc) loc = std::getenv("LANG");
            s->composeTabla = xkb_compose_table_new_from_locale(s->xkbCtx, loc && *loc ? loc : "C", XKB_COMPOSE_COMPILE_NO_FLAGS);
            if (s->composeTabla) s->compose = xkb_compose_state_new(s->composeTabla, XKB_COMPOSE_STATE_NO_FLAGS);
          }
        }
      }
    }
#else
    (void)data, (void)formato, (void)tam;
#endif
    close(fd);
  }
  static void kbEnter(void *, wl_keyboard *, uint32_t, wl_surface *, wl_array *) {}
  static void kbLeave(void *data, wl_keyboard *, uint32_t, wl_surface *) { static_cast<Impl *>(data)->repTecla = 0; }
  static double ahoraSeg() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
  }
  void ponTecla(const std::string &combo, const std::string &texto) {
    teclas.push_back(combo);
    textos.push_back(texto);
  }
  static void kbKey(void *data, wl_keyboard *, uint32_t, uint32_t, uint32_t tecla, uint32_t estado) {
    Impl *s = static_cast<Impl *>(data);
    if (estado != WL_KEYBOARD_KEY_STATE_PRESSED) {
      if (tecla == s->repTecla) s->repTecla = 0;
      return;
    }
    if (s->tecladoPedido == 0) return;
    s->repTecla = 0;
    if (tecla == KEY_ESC) {   /* sin xkb tambien: Esc cierra o cancela */
      s->ponTecla("Escape", "");
      return;
    }
#ifdef HAVE_XKBCOMMON
    if (!s->xkbMapa || !s->xkbEstado) return;
    const xkb_keycode_t kc = tecla + 8;
    std::string combo, texto;
    if (!leeTeclaXkb(s->xkbMapa, s->xkbEstado, s->compose, kc, combo, texto)) return;
    s->ponTecla(combo, texto);
    /* la que se deja pulsada se repite (lo que diga el compositor) */
    if (s->repRitmo > 0 && xkb_keymap_key_repeats(s->xkbMapa, kc)) {
      s->repTecla = tecla, s->repCombo = combo, s->repTexto = texto;
      s->repSiguiente = ahoraSeg() + (double)s->repEspera / 1000.0;
    }
#endif
  }
  static void kbModifiers(void *data, wl_keyboard *, uint32_t, uint32_t dep, uint32_t lat, uint32_t blo, uint32_t grupo) {
#ifdef HAVE_XKBCOMMON
    Impl *s = static_cast<Impl *>(data);
    if (s->xkbEstado) xkb_state_update_mask(s->xkbEstado, dep, lat, blo, 0, 0, grupo);
#else
    (void)data, (void)dep, (void)lat, (void)blo, (void)grupo;
#endif
  }
  static void kbRepeat(void *data, wl_keyboard *, int32_t ritmo, int32_t espera) {
    Impl *s = static_cast<Impl *>(data);
    s->repRitmo = std::max(0, ritmo), s->repEspera = std::max(0, espera);
  }
  /* (v1.15) la repeticion que toca: una pulsacion mas de la tecla que sigue abajo */
  void repetir() {
    if (repTecla == 0 || repRitmo <= 0 || tecladoPedido == 0) return;
    const double t = ahoraSeg();
    if (t < repSiguiente) return;
    ponTecla(repCombo, repTexto);
    repSiguiente += 1.0 / (double)repRitmo;
    if (repSiguiente < t) repSiguiente = t + 1.0 / (double)repRitmo;   /* muy atrasada: no se encadenan de golpe */
  }
  static const struct wl_keyboard_listener keyboardListener;
  /* pedir o soltar el teclado: la capa lo toma en exclusiva (layer-shell v1: 1 = si) */
  /* el valor que entiende el compositor: «a pedido» (2) solo existe desde la version 4 del protocolo;
     el pedido es el mismo de la v1 con otro numero, asi que basta con haber enlazado la v4 */
  uint32_t modoTecladoCapa() const {
    if (tecladoPedido == 2) return capaVersion >= 4 ? 2u : 0u;
    return tecladoPedido == 1 ? 1u : 0u;
  }
  void aplicaTeclado() {
    if (!layer) return;
    zwlr_layer_surface_v1_set_keyboard_interactivity(layer, modoTecladoCapa());
    wl_surface_commit(surf);
    wl_display_flush(dpy);
  }
};

const struct wl_registry_listener Overlay::Impl::registryListener = {
    Impl::registryGlobal, Impl::registryGlobalRemove};

const struct zxdg_output_v1_listener Overlay::Impl::xdgListener = {
    Impl::xdgPosicion, Impl::xdgTamano, Impl::xdgTermina, Impl::xdgNombre, Impl::xdgDescripcion};

const struct wl_output_listener Overlay::Impl::outputListener = {
    Impl::outGeometry, Impl::outMode, Impl::outDone, Impl::outScale, Impl::outName, Impl::outDescription};

const struct zwlr_layer_surface_v1_listener Overlay::Impl::layerListener = {Impl::layerConfigure,
                                                                            Impl::layerClosed};
const struct zwlr_layer_surface_v1_listener Overlay::Impl::veloListener = {Impl::veloConfigure, Impl::veloCerrado};

const struct wl_keyboard_listener Overlay::Impl::keyboardListener = {
    Overlay::Impl::kbKeymap, Overlay::Impl::kbEnter,     Overlay::Impl::kbLeave,
    Overlay::Impl::kbKey,    Overlay::Impl::kbModifiers, Overlay::Impl::kbRepeat};

const struct wl_pointer_listener Overlay::Impl::pointerListener = {
    Impl::ptrEnter, Impl::ptrLeave, Impl::ptrMotion, Impl::ptrButton, Impl::ptrAxis,
    Impl::ptrFrame, Impl::ptrAxisSource, Impl::ptrAxisStop, Impl::ptrAxisDiscrete};

const struct wl_buffer_listener Overlay::Impl::bufferListener = {Impl::bufferReleased};
const struct wl_callback_listener Overlay::Impl::cuadroMovListener = {Impl::cuadroMov};

#ifdef HAVE_FRACTIONAL_SCALE
const struct wp_fractional_scale_v1_listener Overlay::Impl::escalaListener = {Impl::escalaSuperficie};
#endif

/* ---------------- API pública ---------------- */

Overlay::Overlay() : d(new Impl()) {}

Overlay::~Overlay() = default;

bool Overlay::open(std::string *err) {
  d->dpy = wl_display_connect(nullptr);
  if (!d->dpy) {
    if (err) *err = "no hay sesión de Wayland a la que conectarse (WAYLAND_DISPLAY)";
    return false;
  }
  d->registry = wl_display_get_registry(d->dpy);
  wl_registry_add_listener(d->registry, &Impl::registryListener, d.get());
  wl_display_roundtrip(d->dpy);
  /* los monitores ya están enlazados: pedimos su tamaño lógico ahora, así no depende
     del orden en que el compositor anuncia los protocolos */
  if (d->xdgMgr) {
    for (Impl::Monitor *m : d->monitores)
      if (m->obj && !m->xdg) {
        m->xdg = zxdg_output_manager_v1_get_xdg_output(d->xdgMgr, m->obj);
        if (m->xdg) zxdg_output_v1_add_listener(m->xdg, &Impl::xdgListener, m);
      }
  }
  wl_display_roundtrip(d->dpy);
  wl_display_roundtrip(d->dpy);   /* los detalles de cada monitor llegan en varias rondas */
  d->eligeMonitor();

  if (!d->shell || !d->comp || !d->shm) {
    if (err) {
      std::string falta;
      if (!d->shell) falta += " wlr-layer-shell";
      if (!d->comp) falta += " compositor";
      if (!d->shm) falta += " memoria compartida";
      *err = "el compositor no ofrece:" + falta;
    }
    return false;
  }
  if (d->anchoE <= 0 || d->altoE <= 0) {
    if (err) *err = "no se conoció el tamaño de ningún monitor";
    return false;
  }
  d->surf = wl_compositor_create_surface(d->comp);
#ifdef HAVE_FRACTIONAL_SCALE
  if (d->escalaMgr) {
    d->escalaSuperf = wp_fractional_scale_manager_v1_get_fractional_scale(d->escalaMgr, d->surf);
    if (d->escalaSuperf) wp_fractional_scale_v1_add_listener(d->escalaSuperf, &Impl::escalaListener, d.get());
  }
  if (d->viewporter) d->viewport = wp_viewporter_get_viewport(d->viewporter, d->surf);
  wl_display_roundtrip(d->dpy);
#endif
  std::string e;
  d->atlasListo = d->hud.buildAtlas(&e);
  if (!d->atlasListo && err) *err = "la interfaz saldrá sin texto: " + e;
  return true;
}

void Overlay::setPantalla(const std::string &pista) {
  if (!d) return;
  d->pistaMonitor = pista;
  d->eligeMonitor();
}

bool Overlay::pantallaReconocida() const { return d && d->reconocido; }

bool Overlay::available() const { return d && d->shell && d->comp && d->shm && d->surf; }

int Overlay::fd() const { return d->dpy ? wl_display_get_fd(d->dpy) : -1; }

int Overlay::dispatchPending() {
  if (!d->dpy) return -1;
  const int r = wl_display_dispatch_pending(d->dpy);
  if (wl_display_get_error(d->dpy) != 0) d->marcaConexionCaida();
  return r;
}

int Overlay::waitAndDispatch(int ms) {
  if (!d->dpy) return -1;
  /* con la conexion rota, `prepare_read` no deja de fallar y `dispatch_pending` tampoco: el bucle de
     antes se quedaba dando vueltas para siempre (el panel no se cerraba ni con SIGTERM; visto en un
     KWin aparte al rehacer la capa con el lienzo aun puesto) */
  if (wl_display_get_error(d->dpy) != 0) {
    d->marcaConexionCaida();
    return -1;
  }
  while (wl_display_prepare_read(d->dpy) != 0)
    if (wl_display_dispatch_pending(d->dpy) < 0) {
      d->marcaConexionCaida();
      return -1;
    }
  wl_display_flush(d->dpy);
  /* con una tecla abajo, se despierta a tiempo para su repeticion (v1.15) */
  if (d->repTecla != 0 && d->repRitmo > 0 && d->tecladoPedido != 0)
    ms = std::max(0, std::min(ms, (int)std::ceil((d->repSiguiente - Impl::ahoraSeg()) * 1000.0)));
  struct pollfd p{wl_display_get_fd(d->dpy), POLLIN, 0};
  int n = ::poll(&p, 1, ms);
  if (n > 0) {
    wl_display_read_events(d->dpy);
    const int r = wl_display_dispatch_pending(d->dpy);
    if (wl_display_get_error(d->dpy) != 0) d->marcaConexionCaida();
    if (d->sucio && d->tarjetaPuesta) {
      d->sucio = false;
      d->pintar();
    }
    d->repetir();
    return r;
  }
  wl_display_cancel_read(d->dpy);
  d->repetir();
  return 0;
}

void Overlay::showAsk(const std::string &titulo, const std::string &cuerpo) {
  if (!available()) return;
  Impl &s = *d;
  s.titulo = titulo;
  s.cuerpo = cuerpo;
  s.hud.state().ask = true;
  s.hud.state().overlayOn = false;
  s.hud.setAskText(s.titulo, s.cuerpo);
  s.hud.begin(s.anchoE, s.altoE, s.altoLogico());

  /* El escritorio puede haber cambiado desde que arrancó el servicio (otra pantalla
     conectada, el modo 3D de la lente, una suspensión): se vuelve a elegir el monitor
     y, si la capa estaba puesta en otro, se rehace para que la tarjeta no acabe en la
     pantalla equivocada. */
  s.eligeMonitor();
  for (int esperarNombre = 0; esperarNombre < 4 && s.elegido &&
                               (s.elegido->nombre.empty() || s.elegido->anchoFis <= 0); ++esperarNombre) {
    /* la pantalla se acaba de anunciar y todavia no trae nombre ni modos: sin esos datos la
       eleccion sale a ciegas y puede caer en el monitor equivocado */
    s.esperar(60);
    s.eligeMonitor();
  }
  if (s.layer && s.capaEn != (s.elegido ? s.elegido->obj : nullptr))
    s.quitaEscuchaLayer();   /* destruye la capa: se volvera a crear sobre el monitor bueno */

  s.hud.begin(s.anchoE, s.altoE, s.altoLogico());   /* por si el monitor cambio de tamano */
  if (!s.ajustaGeometria()) return;
  if (entornoOdyssey("ODYSSEYVK_DEBUG_UI"))
    std::printf("[depurar capa] lienzo %dx%d escala %.2f hud %.2f | recorte ox=%.0f oy=%.0f buf=%dx%d | ventana %dx%d\n",
                s.anchoE, s.altoE, s.escala, s.hud.scale(), s.ox, s.oy, s.bufAncho, s.bufAlto,
                s.supAncho, s.supAlto);

  if (!s.layer) {
    s.layer = zwlr_layer_shell_v1_get_layer_surface(s.shell, s.surf, s.elegido ? s.elegido->obj : nullptr,
                                                    ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "odysseyvk");
    if (!s.layer) {
      d->aviso = "el compositor rechazó la capa superpuesta";
      return;
    }
    zwlr_layer_surface_v1_add_listener(s.layer, &Impl::layerListener, d.get());
    s.capaEn = s.elegido ? s.elegido->obj : nullptr;
  }
  zwlr_layer_surface_v1_set_anchor(s.layer, ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                                                ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
  zwlr_layer_surface_v1_set_margin(s.layer, 0, 0, 0, 0);   /* la separacion vive dentro del lienzo */
  zwlr_layer_surface_v1_set_exclusive_zone(s.layer, -1);
  zwlr_layer_surface_v1_set_size(s.layer, (uint32_t)s.supAncho, (uint32_t)s.supAlto);
#ifdef HAVE_FRACTIONAL_SCALE
  if (s.viewport) wp_viewport_set_destination(s.viewport, s.supAncho, s.supAlto);
#endif
  s.hayConfigure = false;
  wl_surface_commit(s.surf);

  /* el compositor tiene que confirmar el tamaño antes de poner el lienzo encima */
  for (int intento = 0; intento < 100 && !s.hayConfigure && !s.cerrada; ++intento)
    if (s.esperar(50) < 0) break;
  s.tarjetaPuesta = true;
  s.cerrada = false;
  s.pintar();
}

Hud &Overlay::hud() { return d->hud; }

void Overlay::repaint() {
  if (d && d->tarjetaPuesta) d->pintar();
}

bool Overlay::takeGolpe(Golpe &g) {
  if (!d || d->golpes.empty()) return false;
  g = d->golpes.front();
  d->golpes.erase(d->golpes.begin());
  return true;
}

/* El panel de ajustes: el mismo montaje que la tarjeta, pero sin anclas (centrado) y con el lienzo
   justo de su caja. */
void Overlay::showPanel(bool grande, bool velo) {
  if (!available()) return;
  Impl &s = *d;
  /* el tamano y el sitio de la ultima vez (rama experimental) */
  int izqGuardado = -1, arrGuardado = -1;
  if (!s.panelMontado) {
    float an = 0.0f, al = 0.0f;
    if (s.leeVentana(izqGuardado, arrGuardado, an, al)) s.hud.state().panelAncho = an, s.hud.state().panelAlto = al;
    else izqGuardado = arrGuardado = -1;
  }
  s.hud.state().panel = true;
  s.hud.state().ask = false;
  s.hud.state().overlayOn = false;
  /* (v1.16) al doble, pero sin pasarse de lo que cabe del diseno de 1280x720: en el 4K de la lente es 2,00, y en una
     pantalla mas chica el marco se encogia (a 0,5 en 1280x720) sin que lo de dentro de la tarjeta lo siguiera */
  auto escalaGrande = [&s] {
    if (s.anchoE <= 0 || s.altoE <= 0) return 2.0f;
    return std::max(1.0f, std::min(2.0f, std::min((float)s.anchoE / 1280.0f, (float)s.altoE / 720.0f)));
  };
  s.hud.setScale(grande ? escalaGrande() : 0.0f);
  s.hud.begin(s.anchoE, s.altoE, s.altoLogico());

  /* El escritorio puede haber cambiado desde que arrancó el servicio (otra pantalla
     conectada, el modo 3D de la lente, una suspensión): se vuelve a elegir el monitor
     y, si la capa estaba puesta en otro, se rehace para que la tarjeta no acabe en la
     pantalla equivocada. */
  s.eligeMonitor();
  for (int esperarNombre = 0; esperarNombre < 4 && s.elegido &&
                               (s.elegido->nombre.empty() || s.elegido->anchoFis <= 0); ++esperarNombre) {
    /* la pantalla se acaba de anunciar y todavia no trae nombre ni modos: sin esos datos la
       eleccion sale a ciegas y puede caer en el monitor equivocado */
    s.esperar(60);
    s.eligeMonitor();
  }
  if (s.layer && s.capaEn != (s.elegido ? s.elegido->obj : nullptr))
    s.quitaEscuchaLayer();   /* destruye la capa: se volvera a crear sobre el monitor bueno */

  if (grande) s.hud.setScale(escalaGrande());   /* con el monitor ya elegido */
  s.hud.begin(s.anchoE, s.altoE, s.altoLogico());   /* por si el monitor cambio de tamano */
  if (!s.ajustaGeometria()) return;
  if (entornoOdyssey("ODYSSEYVK_DEBUG_UI"))
    std::printf("[depurar capa] lienzo %dx%d escala %.2f hud %.2f | recorte ox=%.0f oy=%.0f buf=%dx%d | ventana %dx%d\n",
                s.anchoE, s.altoE, s.escala, s.hud.scale(), s.ox, s.oy, s.bufAncho, s.bufAlto,
                s.supAncho, s.supAlto);

  /* el velo va antes que la capa del panel: en la misma capa del compositor, la ultima puesta va encima */
  if (velo) {
    /* la del panel se rehace encima del velo; antes se suelta su lienzo, que una capa nueva no puede
       traer uno puesto antes de su primera configuracion (error de protocolo, medido en un KWin aparte) */
    if (s.layer && !s.veloSurf) s.limpiaTarjeta();
    s.ponVelo();
  }
  if (!s.layer) {
    s.layer = zwlr_layer_shell_v1_get_layer_surface(s.shell, s.surf, s.elegido ? s.elegido->obj : nullptr,
                                                    ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "odysseyvk");
    if (!s.layer) {
      d->aviso = "el compositor rechazó la capa superpuesta";
      return;
    }
    zwlr_layer_surface_v1_add_listener(s.layer, &Impl::layerListener, d.get());
    s.capaEn = s.elegido ? s.elegido->obj : nullptr;
  }
  /* Anclada arriba a la izquierda y colocada con margenes: asi se puede mover arrastrando su
     titulo (v0.86). Empieza en el centro. */
  zwlr_layer_surface_v1_set_anchor(s.layer, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
  s.logAncho = (s.elegido && s.elegido->anchoLog > 0) ? s.elegido->anchoLog : (int)(s.anchoE / std::max(0.25, s.escala));
  s.logAlto = (s.elegido && s.elegido->altoLog > 0) ? s.elegido->altoLog : (int)(s.altoE / std::max(0.25, s.escala));
  /* ya montado (se rehace su capa encima del velo al quitarle el pin): se queda donde estaba */
  if (!s.panelMontado) {
    if (izqGuardado >= 0) {   /* donde se dejo, sin salirse de la pantalla */
      s.margenIzq = std::max(0, std::min(izqGuardado, s.logAncho - s.supAncho));
      s.margenArr = std::max(0, std::min(arrGuardado, s.logAlto - s.supAlto));
    } else {
      s.margenIzq = std::max(0, (s.logAncho - s.supAncho) / 2);
      s.margenArr = std::max(0, (s.logAlto - s.supAlto) / 2);
    }
  }
  s.panelMontado = true;
  zwlr_layer_surface_v1_set_margin(s.layer, s.margenArr, 0, 0, s.margenIzq);
  zwlr_layer_surface_v1_set_exclusive_zone(s.layer, -1);
  zwlr_layer_surface_v1_set_size(s.layer, (uint32_t)s.supAncho, (uint32_t)s.supAlto);
  zwlr_layer_surface_v1_set_keyboard_interactivity(s.layer, s.modoTecladoCapa());
#ifdef HAVE_FRACTIONAL_SCALE
  if (s.viewport) wp_viewport_set_destination(s.viewport, s.supAncho, s.supAlto);
#endif
  s.hayConfigure = false;
  wl_surface_commit(s.surf);

  /* el compositor tiene que confirmar el tamaño antes de poner el lienzo encima */
  for (int intento = 0; intento < 100 && !s.hayConfigure && !s.cerrada; ++intento)
    if (s.esperar(50) < 0) break;
  s.tarjetaPuesta = true;
  s.cerrada = false;
  s.pintar();
}


void Overlay::clicPrueba(float x, float y) {
  if (!d || !d->hud.state().panel) return;
  d->ratonX = x * d->hud.scale();
  d->ratonY = y * d->hud.scale();
  d->panelBoton(true);
  d->panelBoton(false);
}

void Overlay::ratonPrueba(int que, float x, float y) {
  if (!d || !d->hud.state().panel) return;
  if (que >= 0) {
    d->ratonX = x * d->hud.scale();
    d->ratonY = y * d->hud.scale();
    d->hud.setMouse((float)d->ratonX, (float)d->ratonY);
  }
  if (que == 1) d->panelBoton(true);
  else if (que == 0) d->panelMovimiento();
  else d->panelBoton(false);
}

void Overlay::tomarTeclado(bool si) { modoTeclado(si ? 1 : 0); }

void Overlay::modoTeclado(int modo) {
  if (!d || d->tecladoPedido == modo) return;
  d->tecladoPedido = modo;
  if (modo == 0) d->teclas.clear();
  d->aplicaTeclado();
}

int Overlay::pruebaTeclado(const char *distribucion) {
#ifdef HAVE_XKBCOMMON
  /* (v1.15) sin compositor: el teclado `distribucion` con las reglas de siempre y unas pulsaciones, para ver que
     combinacion y que texto da cada una (las teclas muertas, AltGr, la ñ) */
  xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  xkb_rule_names nombres{};
  nombres.layout = distribucion;
  xkb_keymap *mapa = ctx ? xkb_keymap_new_from_names(ctx, &nombres, XKB_KEYMAP_COMPILE_NO_FLAGS) : nullptr;
  if (!mapa) {
    std::fprintf(stderr, "no se pudo armar el teclado «%s»\n", distribucion);
    return 1;
  }
  xkb_state *estado = xkb_state_new(mapa);
  const char *loc = std::getenv("LC_ALL");
  if (!loc || !*loc) loc = std::getenv("LC_CTYPE");
  if (!loc || !*loc) loc = std::getenv("LANG");
  xkb_compose_table *tabla = xkb_compose_table_new_from_locale(ctx, loc && *loc ? loc : "C", XKB_COMPOSE_COMPILE_NO_FLAGS);
  xkb_compose_state *compose = tabla ? xkb_compose_state_new(tabla, XKB_COMPOSE_STATE_NO_FLAGS) : nullptr;
  std::printf("teclado «%s», composicion de «%s»: %s\n", distribucion, loc ? loc : "C", compose ? "si" : "no");
  struct Paso {
    const char *que;
    std::vector<uint32_t> mods, teclas;   /* codigos evdev */
  };
  const Paso pasos[] = {
      {"a", {}, {KEY_A}},
      {"Mayus+a", {KEY_LEFTSHIFT}, {KEY_A}},
      {"´ y a", {}, {KEY_APOSTROPHE, KEY_A}},
      {"¨ y u", {KEY_LEFTSHIFT}, {KEY_APOSTROPHE}},
      {"(sigue) u", {}, {KEY_U}},
      {"AltGr+2", {KEY_RIGHTALT}, {KEY_2}},
      {"AltGr+4", {KEY_RIGHTALT}, {KEY_4}},
      {"ñ", {}, {KEY_SEMICOLON}},
      {"Mayus+1", {KEY_LEFTSHIFT}, {KEY_1}},
      {"Mayus+7", {KEY_LEFTSHIFT}, {KEY_7}},
      {"tecla de -", {}, {KEY_SLASH}},
      {"Mayus+tecla de -", {KEY_LEFTSHIFT}, {KEY_SLASH}},
      {"` y espacio", {}, {KEY_LEFTBRACE, KEY_SPACE}},
      {"espacio", {}, {KEY_SPACE}},
      {"Ctrl+V", {KEY_LEFTCTRL}, {KEY_V}},
      {"Ctrl+A", {KEY_LEFTCTRL}, {KEY_A}},
      {"Retroceso", {}, {KEY_BACKSPACE}},
      {"Intro", {}, {KEY_ENTER}},
      {"Supr", {}, {KEY_DELETE}},
      {"flecha izquierda", {}, {KEY_LEFT}},
      {"Mayus solo", {}, {KEY_LEFTSHIFT}},
  };
  for (const Paso &p : pasos) {
    for (uint32_t m : p.mods) xkb_state_update_key(estado, m + 8, XKB_KEY_DOWN);
    std::string dice;
    for (uint32_t t : p.teclas) {
      std::string combo, texto;
      const bool hay = leeTeclaXkb(mapa, estado, compose, t + 8, combo, texto);
      dice += hay ? "[" + combo + "|" + texto + "] " : std::string("[nada] ");
    }
    for (uint32_t m : p.mods) xkb_state_update_key(estado, m + 8, XKB_KEY_UP);
    std::printf("  %-18s %s\n", p.que, dice.c_str());
  }
  if (compose) xkb_compose_state_unref(compose);
  if (tabla) xkb_compose_table_unref(tabla);
  xkb_state_unref(estado);
  xkb_keymap_unref(mapa);
  xkb_context_unref(ctx);
  return 0;
#else
  (void)distribucion;
  std::fprintf(stderr, "compilado sin xkbcommon\n");
  return 1;
#endif
}

bool Overlay::tecladoAPedido() const { return d && d->capaVersion >= 4; }

void Overlay::quitarVelo() {
  if (d) d->quitaVelo();
}

bool Overlay::takeTecla(std::string &combo) {
  std::string texto;
  return takeTecla(combo, texto);
}

bool Overlay::takeTecla(std::string &combo, std::string &texto) {
  if (!d || d->teclas.empty()) return false;
  combo = d->teclas.front();
  d->teclas.erase(d->teclas.begin());
  texto.clear();
  if (!d->textos.empty()) {
    texto = d->textos.front();
    d->textos.erase(d->textos.begin());
  }
  return true;
}

void Overlay::hideAsk() {
  if (!d) return;
  d->limpiaTarjeta();
}

bool Overlay::askVisible() const { return d->tarjetaPuesta; }

bool Overlay::conexionCaida() const { return d && d->conexionCaida; }
void Overlay::ponEntradaBarra(const std::string &titulo) { if (d) d->ponBarra(titulo); }
void Overlay::quitaEntradaBarra() { if (d) d->quitaBarra(); }
bool Overlay::entradaCerrada() const { return d && d->barraCerrada; }

bool Overlay::remember() const { return d && d->hud.state().remember; }

void Overlay::setRemember(bool v) {
  if (!d) return;
  d->hud.state().remember = v;
  if (d->tarjetaPuesta) d->pintar();
}

std::string Overlay::takeAction() {
  std::string out = d->accion;
  d->accion.clear();
  return out;
}

bool Overlay::dumpPng(const std::string &ruta, std::string *err) {
  if (!d->tarjetaPuesta || d->bufAncho <= 0 || d->bufAlto <= 0) {
    if (err) *err = "no hay nada dibujado que volcar";
    return false;
  }
  const uint32_t *pinta = nullptr;
  for (Impl::Bloque *b : d->bloques)
    if (b->buf) {
      pinta = reinterpret_cast<const uint32_t *>(b->mapa);
      break;
    }
  if (!pinta) {
    if (err) *err = "no hay lienzo que volcar";
    return false;
  }
  std::vector<uint8_t> rgba((size_t)d->bufAncho * (size_t)d->bufAlto * 4u);
  for (size_t p = 0; p < rgba.size() / 4u; p++) {
    const uint32_t v = pinta[p];
    rgba[p * 4u + 0u] = (uint8_t)((v >> 16) & 0xFF);
    rgba[p * 4u + 1u] = (uint8_t)((v >> 8) & 0xFF);
    rgba[p * 4u + 2u] = (uint8_t)(v & 0xFF);
    rgba[p * 4u + 3u] = (uint8_t)((v >> 24) & 0xFF);
  }
  if (!glPngSave(ruta, d->bufAncho, d->bufAlto, rgba.data())) {
    if (err) *err = "no se pudo escribir " + ruta;
    return false;
  }
  return true;
}

std::vector<std::string> Overlay::pantallas() const {
  std::vector<std::string> out;
  if (!d) return out;
  for (const Impl::Monitor *m : d->monitores) {
    if (!m->vivo || !m->obj) continue;
    std::string linea = m->nombre.empty() ? "(sin nombre)" : m->nombre;
    if (!m->modelo.empty()) linea += " (" + m->modelo + ")";
    linea += " ";
    linea += std::to_string(m->anchoFis > 0 ? m->anchoFis : m->ancho);
    linea += "x";
    linea += std::to_string(m->altoFis > 0 ? m->altoFis : m->alto);
    out.push_back(linea);
  }
  return out;
}

/* El compositor manda en el configure el area util de la pantalla donde puso la capa. Si esa
   area es claramente MAS grande que la ventana que pedimos, la capa esta en otra pantalla: se
   rehace sobre la pantalla pedida (dos intentos como mucho) y se guarda el motivo. */
bool Overlay::corrigeColocacion() {
  if (!d || !d->layer || !d->tarjetaPuesta || d->intentosColocacion >= 2 || !d->elegido) return false;
  if (d->ofrecidoAncho <= 0 || d->ofrecidoAlto <= 0 || d->ofrecidoAncho > 8192) return false;
  /* Una oferta MAS GRANDE que la tarjeta pedida es la huella del area util de otra pantalla.
     Una mas pequena es el compositor ajustando margenes o contando en su propia escala: eso
     no significa que la capa este en otro monitor. */
  if (d->ofrecidoAncho <= d->supAncho + 8 && d->ofrecidoAlto <= d->supAlto + 8) return false;
  const int intentos = ++d->intentosColocacion;
  char texto[240];
  std::snprintf(texto, sizeof texto,
                "el compositor puso la capa en otra pantalla: ofrecio %dx%d y la tarjeta pide "
                "%dx%d para %s; se intenta de nuevo (%d de 2)",
                d->ofrecidoAncho, d->ofrecidoAlto, d->supAncho, d->supAlto,
                d->elegido->nombre.c_str(), intentos);
  d->aviso = texto;
  err_ = texto;
  d->avisoColocacion = true;
  const std::string titulo = d->titulo, cuerpo = d->cuerpo;
  d->quitaEscuchaLayer();
  showAsk(titulo, cuerpo);
  d->intentosColocacion = intentos;
  return true;
}

bool Overlay::hayAvisoColocacion() {
  if (!d || !d->avisoColocacion) return false;
  d->avisoColocacion = false;
  return true;
}

std::string Overlay::outputName() const {
  if (!d->elegido) return "ninguno";
  std::string out = d->elegido->nombre;
  if (!d->elegido->modelo.empty()) out += " (" + d->elegido->modelo + ")";
  out += " " + std::to_string(d->anchoE) + "x" + std::to_string(d->altoE);
  return out;
}

double Overlay::scale() const { return d ? d->escala : 1.0; }
