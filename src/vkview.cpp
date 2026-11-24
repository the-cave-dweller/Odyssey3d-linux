/* vkview.cpp — el visor por Vulkan (ver viewer.h). GLFW sólo pone la ventana, la entrada y el
   cursor; todo lo demás se dibuja con Vulkan: la trama a pantalla completa y encima la interfaz
   estilo Odyssey 3D Hub. El fotograma del compositor se muestrea desde su DMA-BUF, sin copia por
   la CPU. */
#define GLFW_INCLUDE_VULKAN 1  /* antes de cualquier cabecera que traiga GLFW */

#include "viewer.h"

#ifdef HAVE_CAPTURE
#include "capture.h"
#include "wm.h"
#endif
#include "glutil.h"
#ifdef HAVE_OPENCV
#include "trackfeed.h"
#endif
#include "session.h"
#include "mpris.h"
#include "util.h"
#ifdef HAVE_ORT
#include "depth.h"
#endif
#include "vkui.h"
#include "vkweave.h"

#include <GLFW/glfw3.h>
#ifdef HAVE_WAYLAND_WM
/* la ventana del visor la pone GLFW, pero la region de entrada la pide el compositor: para eso
   necesitamos el wl_surface que GLFW creo por detras */
#define GLFW_EXPOSE_NATIVE_WAYLAND 1
#include <GLFW/glfw3native.h>
#include <wayland-client.h>
#else
struct wl_compositor;
struct wl_surface;
struct wl_region;
#endif

#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <unistd.h>

static void vkGlfwError(int code, const char *desc) { std::fprintf(stderr, "[glfw %d] %s\n", code, desc); }

namespace {

/* ---- que el raton no se quede en el visor ---------------------------------------------
   La ventana del visor tapa la pantalla entera y, si no le decimos nada al compositor, se traga
   todos los clics y toda la rueda: no se puede dar a pausa en el video ni mover la barra de
   avance porque lo que se pica es el visor. Le marcamos una region de entrada con la pastilla (y
   con el aviso, si esta puesto): fuera de esas cajas el raton atraviesa el visor y llega a la
   ventana de atras. Las teclas siguen yendo a la ventana que tenga el foco: un clic en el video
   se lo queda y un clic en la pastilla devuelve el mando al visor. --sin-pasar-raton lo apaga. */
struct RegionEntrada {
  struct wl_compositor *comp = nullptr;  /* quien fabrica las cajas de la region */
  struct wl_surface *surf = nullptr;     /* la superficie de la ventana del visor */
  std::string firma = "?";              /* lo mandado la ultima vez, para no repetir cada cuadro.
                                       Empieza con un valor imposible: la region vacia (sin pastilla)
                                       tambien tiene una firma vacia y hay que mandarla igual. */
  bool avisoImpreso = false;               /* la explicacion del teclado solo se dice una vez */
  bool activo = false;
  /* El rincon que dejamos pinchable cuando la pastilla esta ocultada, en pixeles de la ventana.
     Sin ninguna caja en la region no llega ni un movimiento del puntero y la pastilla ya no
     vuelve sola: con este hueco el puntero puede entrarlo y la pastilla reaparece. */
  float hogar[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  bool hayHogar = false;
};

struct VkUi {
  WeaveTuning tune{};
  Hud hud;
  RegionEntrada region;
  bool quit = false;
  float convBase = 0.0f;  /* la convergencia que mueven las teclas; la del deslizador se suma */
  double mx = -1.0, my = -1.0;
  Hud::Hit dragging = Hud::HitNone;
  bool inputChanged = true;  /* forzar cuadro cuando el raton o el teclado se movieron */
  int fbW = 0, fbH = 0;
  int logH = 0;            /* alto logico de la ventana (el que ve el escritorio), para la interfaz */
  /* Entregar la pantalla: con Tab nos minimizamos y apagamos el 3D del panel, y el navegador
     vuelve a estar accesible con el raton. Al recuperar el foco (Meta+Alt+Tab o la barra de tareas)
     volvemos a poner el 3D. entregada=verdadero mientras estamos aparte. */
  bool entregar = false, entregada = false, recuperar = false;
  GLFWwindow *win = nullptr;
  Rig *rig = nullptr;
  const ViewerOpts *opts = nullptr;
  /* El visor se queda el teclado y el raton de la pantalla (por eso el navegador deja de
     responder): para poder pausar el video sin quitar la capa hablamos con el reproductor
     MPRIS del navegador. mediaPausaManual evita reanudar un video que el usuario pauso a proposito. */
  Mpris *media = nullptr;
  bool mediaPausaManual = false;
  std::string fmtInfo;       /* " · lado a lado": recuerda que formato se esta entrelazando */
  bool convToggle = false;   /* la tecla C: el bucle del visor enciende o apaga el conversor */
  std::string extraInfo;     /* " · video en pausa" y cosas asi, detras de la frase de estado */
  bool convInfo = false;     /* contar en la frase de estado que el conversor 2D/3D esta puesto */
  bool convWant = false;     /* la tarjeta del hub pidio "video normal": encender, no alternar */
  /* el boton de la pastilla y las teclas 1 y 2: 1 = la red de Samsung (mde_*.onnx), 2 = la nuestra
     (Depth Anything). redPedida es un encargo pendiente que el bucle atiende con la red parada. */
  int redPedida = 0;
  std::string rutaRed[3];    /* [1] y [2]: ruta .onnx de cada red; vacia = esa red no esta disponible */
  double extraHasta = 0.0;   /* la frase extra se quita sola a los pocos segundos */
};

/* senal de cierre (systemctl stop, kill, o el propio serve): la sesion tiene que salir por la
   puerta de adelante para devolver el panel a su modo, apagar la lente y apagar el scaler. Si se
   moria por SIGTERM a secas, el monitor se quedaba en modo 3D y la lente encendida. */
static volatile std::sig_atomic_t gTerm = 0;
static void onTermSignal(int) { gTerm = 1; }

void printTuning(const VkUi &ui, const char *what) {
  std::printf("[%-11s] weave=%d swap=%d fs=%5.2f align=%+.4f pitch=%.5f xtdyn=%+.4f conv=%+.5f prof=%d rel=%d\n",
              what, (int)ui.tune.weave, (int)ui.tune.swapLR, ui.tune.filterSlope, ui.tune.align, ui.tune.pitch,
              ui.tune.xtalkDyn, ui.tune.conv, ui.hud.state().depth, ui.hud.state().popout);
  std::fflush(stdout);
}

/* frase breve bajo la pastilla: qué se está viendo y en qué estado está el hardware */
std::string infoLine(SourceKind kind, const Rig &rig, const Hud::State &st, int w, int h,
                     const std::string &extra = "", const std::string &fmt = "", bool convOn = false) {
  std::string out = kind == SourceKind::Capture ? "ventana capturada " : "patron de prueba ";
  out += std::to_string(w) + "x" + std::to_string(h);
  out += st.weaveOn ? (st.ask ? " sin formato elegido" : " con trama") : " 2D plano";
  if (st.weaveOn) out += fmt;   /* el formato elegido en la tarjeta: lado a lado / arriba y abajo */
  out += " · lente ";
  out += (rig.lensOk ? "3D" : "2D");
  out += " · scaler ";
  out += (rig.scalerOn ? "3D" : "apagado");
  /* el aviso que faltaba: la trama solo da profundidad si el video trae dos imagenes. Con un video
     normal lo que sale es una imagen doble y movida, y es lo que se ve "regular" en el monitor. */
  if (convOn)
    out += " · conversor de video normal (C lo apaga)";
  else if (st.weaveOn && !fmt.empty())
    out += " · si el video es normal (una sola imagen) se vera doble: tecla 3 o 2D plano";
  out += extra;
  return out;
}

/* slotRed() vive en util.h: es la misma regla para el hub y para el visor. */
const char *nombreRed(int slot) { return slot == 1 ? "la de Samsung" : "la nuestra"; }

void applyHit(VkUi &ui, Hud::Hit hit) {
  Hud::State &st = ui.hud.state();
  WeaveTuning &t = ui.tune;
  switch (hit) {
    case Hud::Hit3D:
      ui.hud.toggle3D();
      printTuning(ui, "3D/2D");
      break;
    case Hud::HitOverlay:
      ui.hud.toggleOverlay();
      break;
    case Hud::HitDepth:
    case Hud::HitPopout:
      ui.dragging = hit;
      break;
    case Hud::HitRed:
      ui.redPedida = (st.red == 1) ? 2 : 1;   /* el boton pasa de una red a la otra */
      break;
    case Hud::HitAskSbs:
      st.ask = false;
      t.topBottom = false;
      st.weaveOn = true;
      printTuning(ui, "lado a lado");
      break;
    case Hud::HitAskTop:
      st.ask = false;
      t.topBottom = true;
      st.weaveOn = true;
      printTuning(ui, "arriba/abajo");
      break;
    case Hud::HitAskFlat:
      st.ask = false;
      st.weaveOn = false;
      printTuning(ui, "2D plano");
      break;
    case Hud::HitAskConv:
      st.ask = false;
      ui.convWant = true;   /* el bucle del visor enciende el conversor (necesita la red ONNX) */
      printTuning(ui, "video normal a 3D");
      break;
    case Hud::HitAskDont:
      st.remember = !st.remember;
      break;
    case Hud::HitAskClose:
      st.ask = false;
      break;
    default:
      break;
  }
}

/* El compositor nos avisa cuando esta ventana vuelve a tener el foco. Si nos fuimos con Tab,
   aqui es donde hay que volver a encender la lente, el scaler y el modo 3D del panel. */
void onFocusVK(GLFWwindow *win, int enfocado) {
  VkUi *ui = (VkUi *)glfwGetWindowUserPointer(win);
  if (!ui) return;
  ui->inputChanged = true;
  if (enfocado && ui->entregada) ui->recuperar = true;
}

/* Le avisamos al hub de que la minimizacion es nuestra (Tab), para que no nos devuelva la ventana.
   El hub mira ademas que este proceso siga vivo, asi que un portazo a mitad no deja el aviso
   eternamente. Mientras no exista ese archivo, una ventana del visor minimizada es un accidente. */
void escribirEntregada() {
  std::ofstream os(rutaEstadoOdyssey("entregada"), std::ios::trunc);
  if (os) os << (long)::getpid() << "\n";
}
void borrarEntregada() {
  std::error_code ec;
  std::filesystem::remove(rutaEstadoOdyssey("entregada"), ec);
}

void onKeyVK(GLFWwindow *win, int key, int, int action, int mods) {
  if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
  VkUi *ui = (VkUi *)glfwGetWindowUserPointer(win);
  if (!ui) return;
  ui->inputChanged = true;
  if (key == GLFW_KEY_ESCAPE) {
    ui->quit = true;
    glfwSetWindowShouldClose(win, GLFW_TRUE);
    return;
  }
  /* Tab: entregar la pantalla. Somos una ventana a pantalla completa por encima de todo, asi
     que el unico modo de usar el raton sobre el navegador es irnos nosotros: nos minimizamos y
     apagamos el 3D del panel. Al volver a mirar esta ventana (Meta+Alt+Tab o la barra de tareas) el
     bucle vuelve a poner el 3D. ESC sigue siendo la salida definitiva. */
  if (key == GLFW_KEY_TAB) {
    if (!ui->entregada && !ui->entregar) ui->entregar = true;
    return;
  }
  /* PB (Pausa / Alto): la pastilla siempre a la vista. Si esta ocultada vuelve, y si el gestor de
     ventanas nos minimizo nos devolvemos nosotros. Cuando el foco lo tiene el video esta tecla no
     nos llega a nosotros: ahi la responde el hub con `ctl aparece`, que se puede atar a la misma
     tecla como atajo global del escritorio. */
  if (key == GLFW_KEY_PAUSE) {
    const bool estabaCiega = !ui->hud.overlayVisible();
    ui->hud.showOverlay();
    if (glfwGetWindowAttrib(win, GLFW_ICONIFIED)) glfwRestoreWindow(win);
    glfwFocusWindow(win);
    ui->inputChanged = true;
    std::printf("[tecla] PB: %s\n", estabaCiega ? "la pastilla vuelve a la vista"
                                                : "la pastilla ya estaba a la vista, ventana al frente");
    std::fflush(stdout);
    return;
  }
  /* atajos del hub oficial: Ctrl+Shift+1 3D · 2 superposición · 3/4 profundidad · 5/6 relieve
     y 7/8, que son nuestros: achicar o agrandar la pastilla */
  if ((mods & GLFW_MOD_CONTROL) && (mods & GLFW_MOD_SHIFT)) {
    const char *tag = nullptr;
    switch (key) {
      case GLFW_KEY_1: ui->hud.toggle3D(); tag = "3D/2D"; break;
      case GLFW_KEY_2: ui->hud.toggleOverlay(); tag = "superposicion"; break;
      case GLFW_KEY_3: ui->hud.nudgeDepth(-5); tag = "profundidad -"; break;
      case GLFW_KEY_4: ui->hud.nudgeDepth(+5); tag = "profundidad +"; break;
      case GLFW_KEY_5: ui->hud.nudgePopout(-5); tag = "relieve -"; break;
      case GLFW_KEY_6: ui->hud.nudgePopout(+5); tag = "relieve +"; break;
      /* 7 / 8: tamano de la pastilla. El panel cambia de escala al pasar al modo 3D (se queda en
         1) y a cada quien le viene bien un tamano distinto, asi que se puede mover en marcha. */
      case GLFW_KEY_7:
      case GLFW_KEY_8: {
        const float s = ui->hud.scale() + (key == GLFW_KEY_8 ? 0.25f : -0.25f);
        const float fija = s < 0.5f ? 0.5f : (s > 3.0f ? 3.0f : s);
        ui->hud.setScale(fija);
        std::printf("[pastilla] tamano x%.2f\n", (double)fija);
        break;
      }
      default: break;
    }
    if (tag) printTuning(*ui, tag);
    return;
  }
  /* P o la tecla de reproduccion: pausar y reanudar el video del navegador sin quitar la
     capa. Hace falta porque el visor se queda el teclado y el raton de la pantalla. */
  if (key == GLFW_KEY_P || key == GLFW_KEY_SPACE) {
    if (!ui->media || !ui->media->playerFound()) {
      std::printf("[video] no hay ningun reproductor MPRIS en sesion: el navegador tendra que"
                  " pausarse a mano, o pulsar Tab para quedarse con el raton y el navegador)\n");
      return;
    }
    std::string antes, despues, e;
    ui->media->status(&antes);
    if (!ui->media->playPause(&e)) {
      std::printf("[video] el reproductor no acepto la pausa: %s\n", e.c_str());
      return;
    }
    ui->media->status(&despues);
    const bool pausado = despues.empty() ? (antes != "Paused") : (despues == "Paused" || despues == "Stopped");
    ui->mediaPausaManual = pausado;
    ui->extraInfo = pausado ? " · video en pausa" : " · video en marcha";
    ui->extraHasta = glfwGetTime() + 8.0;
    std::printf("[video] %s (%s) · otra vez P para %s\n", pausado ? "pausado" : "en marcha",
                ui->media->playerName().c_str(), pausado ? "reanudar" : "pausar");
    std::fflush(stdout);
    return;
  }
  /* M: cambiar el formato del video sin salir de la sesion. Es la tecla que salva cuando se eligio
     "lado a lado" sobre un video que en realidad no tiene dos imagenes: cada ojo queda viendo media
     pantalla ampliada al doble, o sea una imagen enorme y cortada por los bordes. Los tres estados
     del ciclo son lado a lado, arriba y abajo, y 2D plano (el video tal cual). */
  if (key == GLFW_KEY_C) {
    ui->convToggle = true;   /* el bucle del visor enciende o apaga el conversor */
    return;
  }
  if (key == GLFW_KEY_M) {
    Hud::State &st = ui->hud.state();
    if (!st.weaveOn) {
      st.weaveOn = true;
      ui->tune.topBottom = false;
      ui->fmtInfo = " · lado a lado";
      std::printf("[formato] lado a lado (dos imagenes una al lado de otra)\n");
    } else if (!ui->tune.topBottom) {
      ui->tune.topBottom = true;
      ui->fmtInfo = " · arriba y abajo";
      std::printf("[formato] arriba y abajo (dos imagenes una sobre otra)\n");
    } else {
      st.weaveOn = false;
      std::printf("[formato] 2D plano: el video se ve tal cual, sin relieve\n");
    }
    std::fflush(stdout);
    printTuning(*ui, "formato");
    return;
  }
  /* 1 y 2 sin modificar: cambiar la red de profundidad del conversor. La 1 es la de Samsung
     (mde_large.onnx, la que usa su programa) y la 2 la nuestra (Depth Anything). Se puede hacer en
     marcha: el bucle cambia la red cuando el hilo de la estimacion esta libre. */
  if (key == GLFW_KEY_1 || key == GLFW_KEY_2) {
    ui->redPedida = key == GLFW_KEY_1 ? 1 : 2;
    return;
  }
  const char *tag = nullptr;
  if (tuningKey(ui->tune, key, &tag)) {
    if (key == GLFW_KEY_0 || key == GLFW_KEY_9) ui->convBase = ui->tune.conv; /* la tecla gana sobre el deslizador */
    if (tag) printTuning(*ui, tag);
  }
}

/* Cuanto separar la vista izquierda de la derecha segun el deslizador "3D Salida".
   El recorrido nuevo es el presupuesto del programa de Samsung, medido de su config:
   +12,67 px por ojo hacia afuera y 9,98 px hacia adentro (fraccion del ancho), o sea
   ~22,6 px de separacion entre ojos con el deslizador a 50 y a 3840 px de ancho. El
   recorrido viejo llegaba a ~65 px, 2,9 veces mas; ODYSSEY3D_RELIEVE=2.9 lo devuelve. */
bool topesPorOjoActivos() {
  const char *e = std::getenv("ODYSSEY3D_TOPE");
  return !e || e[0] != '0';
}

float gananciaRelieve(int popout) {
  static const float escala = [] {
    const char *e = std::getenv("ODYSSEY3D_RELIEVE");
    return e ? (float)std::atof(e) : 1.0f;
  }();
  return (0.00104f + (float)popout / 100.0f * 0.00972f) * escala;
}

/* GLFW da la posicion del raton en los puntos logicos de la ventana, pero la interfaz se dibuja
   sobre el lienzo de pixeles reales del panel. Con el panel en modo normal (escala 1,5) son
   medidas distintas: sin este paso los clics caen por encima de lo que se ve. */
void pointerScale(GLFWwindow *win, const VkUi *ui, float &kx, float &ky) {
  int lw = 0, lh = 0;
  glfwGetWindowSize(win, &lw, &lh);
  kx = lw > 0 && ui->fbW > 0 ? (float)ui->fbW / (float)lw : 1.0f;
  ky = lh > 0 && ui->fbH > 0 ? (float)ui->fbH / (float)lh : 1.0f;
}

#ifdef HAVE_WAYLAND_WM
/* el compositor hace falta para crear la region. GLFW ya esta conectado al compositor, asi que le
   pedimos el gestor a esa misma conexion; el registro se suelta enseguida y el gestor sigue valido. */
void alRegistrar(void *dat, struct wl_registry *reg, uint32_t id, const char *iface, uint32_t ver) {
  RegionEntrada *re = (RegionEntrada *)dat;
  if (!re->comp && !std::strcmp(iface, "wl_compositor"))
    re->comp = (struct wl_compositor *)wl_registry_bind(reg, id, &wl_compositor_interface, ver > 1u ? 1u : ver);
}
void alQuitarRegistro(void *, struct wl_registry *, uint32_t) {}
const struct wl_registry_listener oyenteDelRegistro = {alRegistrar, alQuitarRegistro};
#endif

void abrirRegionEntrada(GLFWwindow *win, RegionEntrada *re) {
#ifdef HAVE_WAYLAND_WM
  if (glfwGetPlatform() != GLFW_PLATFORM_WAYLAND) return;  /* visor sobre X11: ahi no hay regiones */
  re->surf = glfwGetWaylandWindow(win);
  struct wl_display *dpy = glfwGetWaylandDisplay();
  if (!re->surf || !dpy) return;
  struct wl_registry *reg = wl_display_get_registry(dpy);
  if (!reg) return;
  wl_registry_add_listener(reg, &oyenteDelRegistro, re);
  wl_display_roundtrip(dpy);
  wl_registry_destroy(reg);
  re->activo = re->comp != nullptr;
  if (!re->activo)
    std::fprintf(stderr, "[raton] el compositor no dio wl_compositor: el visor seguira tragando los clics\n");
#else
  (void)win;
  (void)re;
#endif
}

/* la caja esta en pixeles del lienzo; la region de entrada se mide en el espacio logico de la
   ventana (el que ve el escritorio y el raton), que es el lienzo partido por la escala */
void cajaEnLogico(float kx, float ky, float px, float py, float pw, float ph, int out[4]) {
  out[0] = (int)std::floor(px / kx);
  out[1] = (int)std::floor(py / ky);
  out[2] = (int)std::ceil(pw / kx);
  out[3] = (int)std::ceil(ph / ky);
}

void aplicarRegionEntrada(GLFWwindow *win, RegionEntrada *re, VkUi *ui, bool dejarPasar) {
#ifdef HAVE_WAYLAND_WM
  if (!re->activo) return;
  /* arrastrando un deslizador hace falta quedarnos el raton entero, si no el puntero se sale de la
     caja a medio arrastre y el deslizador se queda plantado */
  const bool pleno = !dejarPasar || ui->dragging != Hud::HitNone;
  float kx = 1.0f, ky = 1.0f;
  pointerScale(win, ui, kx, ky);
  int cajas[2][4];
  int n = 0;
  const Hud::State &st = ui->hud.state();
  float px = 0.0f, py = 0.0f, pw = 0.0f, ph = 0.0f;
  if (!pleno && ui->hud.pillBounds(&px, &py, &pw, &ph)) {
    cajaEnLogico(kx, ky, px, py, pw, ph, cajas[n]);
    n++;
  }
  if (!pleno && st.ask && n < 2 && ui->hud.askBounds(&px, &py, &pw, &ph)) {
    cajaEnLogico(kx, ky, px, py, pw, ph, cajas[n]);
    n++;
  }
  re->hayHogar = false;
  /* Con la pastilla ocultada (Ctrl+Shift+2) no quedaba ninguna caja en la region: sin region no
     llega ni un movimiento del puntero, y el que la oculto desde el video (que tiene el foco) ya
     no tenia forma de traerla. Dejamos pinchada la punta derecha de su sitio de siempre: el puntero
     que entra ahi la devuelve, y el clic ademas le devuelve el teclado al visor. */
  if (!pleno && n == 0 && ui->hud.huecoDeLaPastilla(&px, &py, &pw, &ph)) {
    re->hogar[0] = px;
    re->hogar[1] = py;
    re->hogar[2] = pw;
    re->hogar[3] = ph;
    re->hayHogar = true;
    cajaEnLogico(kx, ky, px, py, pw, ph, cajas[n]);
    n++;
  }
  std::string firma;
  char pedazo[48];
  if (pleno) firma = "la ventana entera";
  for (int i = 0; i < n; i++) {
    std::snprintf(pedazo, sizeof pedazo, " %d,%d %dx%d", cajas[i][0], cajas[i][1], cajas[i][2], cajas[i][3]);
    firma += pedazo;
  }
  if (firma == re->firma) return;  /* sin cambio de tamano no se le habla al compositor cada cuadro */
  if (pleno) {
    wl_surface_set_input_region(re->surf, nullptr);  /* region sin fronteras: la ventana oye todo */
  } else {
    struct wl_region *r = wl_compositor_create_region(re->comp);
    if (!r) return;
    for (int i = 0; i < n; i++) wl_region_add(r, cajas[i][0], cajas[i][1], cajas[i][2], cajas[i][3]);
    wl_surface_set_input_region(re->surf, r);  /* caja vacia = aqui no se pica nada: pasa todo */
    wl_region_destroy(r);
  }
  wl_surface_commit(re->surf);  /* la region se aplica con el siguiente envío de la superficie */
  re->firma = firma;
  std::printf("[raton] %s · region de entrada:%s\n",
              pleno ? (dejarPasar ? "arrastrando: el raton se queda en el visor"
                                  : "el visor se queda todos los clics (--sin-pasar-raton)")
                    : (re->hayHogar ? "pastilla oculta: solo se pica su rincon de arriba a la derecha"
                                    : (n ? "el raton atraviesa el visor fuera de la pastilla"
                                         : "sin pastilla ni aviso: el raton pasa entero")),
              firma.empty() ? " ninguna" : (" " + firma).c_str());
  if (!re->avisoImpreso) {
    re->avisoImpreso = true;
    std::printf("          el teclado se queda con la ventana activa: clic en el video y manda el video, clic\n"
                "          en la pastilla y el mando vuelve al visor\n");
  }
  std::fflush(stdout);
#else
  (void)win;
  (void)re;
  (void)ui;
  (void)dejarPasar;
#endif
}

void onCursorVK(GLFWwindow *win, double x, double y) {
  VkUi *ui = (VkUi *)glfwGetWindowUserPointer(win);
  if (!ui) return;
  float kx = 1.0f, ky = 1.0f;
  pointerScale(win, ui, kx, ky);
  x *= kx;
  y *= ky;
  bool dentro = x >= 0 && y >= 0 && x < (double)ui->fbW && y < (double)ui->fbH;
  if (std::getenv("ODYSSEY_CONV")) {
    static double ultimo = 0.0;
    double t = glfwGetTime();
    if (t - ultimo > 0.2) {
      ultimo = t;
      std::printf("[tarjeta] puntero %.0f,%.0f\n", x, y);
      std::fflush(stdout);
    }
  }
  (void)dentro;
  ui->mx = x;
  ui->my = y;
  ui->inputChanged = true;
  ui->hud.setMouse((float)x, (float)y);
  if (ui->dragging != Hud::HitNone) ui->hud.dragTo(ui->dragging, (float)x);
}

void onEnterVK(GLFWwindow *win, int entered) {
  VkUi *ui = (VkUi *)glfwGetWindowUserPointer(win);
  if (!ui) return;
  /* con la region de entrada el puntero "sale" de la ventana en cuanto se va de la pastilla: si no
     limpiamos la posicion, el boton se queda resaltado aunque el raton este manejando el video */
  if (!entered && ui->dragging == Hud::HitNone) {
    ui->mx = ui->my = -1.0;
    ui->hud.setMouse(-1.0f, -1.0f);
    ui->inputChanged = true;
  }
  if (!std::getenv("ODYSSEY_CONV")) return;
  std::printf("[tarjeta] el puntero %s la ventana (ahora %.0f,%.0f)\n", entered ? "entra en" : "sale de",
              ui ? ui->mx : -1.0, ui ? ui->my : -1.0);
  std::fflush(stdout);
}

void onButtonVK(GLFWwindow *win, int button, int action, int) {
  VkUi *ui = (VkUi *)glfwGetWindowUserPointer(win);
  if (!ui || button != GLFW_MOUSE_BUTTON_LEFT) return;
  ui->inputChanged = true;
  if (action == GLFW_RELEASE) {
    ui->dragging = Hud::HitNone;
    return;
  }
  Hud::Hit hit = ui->hud.hit((float)ui->mx, (float)ui->my);
  if (std::getenv("ODYSSEY_CONV")) {   /* mira donde cae el puntero: depuracion de la tarjeta */
    float bx = 0, by = 0, bw = 0, bh = 0;
    ui->hud.askBounds(&bx, &by, &bw, &bh);
    std::printf("[tarjeta] puntero %.0f,%.0f escala %.2f golpe %d panel %.0f,%.0f %.0fx%.0f\n", ui->mx, ui->my,
                ui->hud.scale(), (int)hit, bx, by, bw, bh);
    std::fflush(stdout);
  }
  applyHit(*ui, hit);
}

bool loadCorrections(const ViewerOpts &opts, std::vector<uint8_t> &a, int &aw, int &ah, std::vector<uint8_t> &b,
                     int &bw, int &bh) {
#ifdef HAVE_PNG
  if (opts.corrA.empty() || opts.corrB.empty()) return false;
  std::vector<uint8_t> ra, rb;
  if (!pngLoadRGBA(opts.corrA, aw, ah, ra) || !pngLoadRGBA(opts.corrB, bw, bh, rb)) {
    std::fprintf(stderr, "[aviso] no se pudieron leer los mapas de corrección\n");
    return false;
  }
  a.assign((size_t)aw * (size_t)ah, 0);
  b.assign((size_t)bw * (size_t)bh, 0);
  for (size_t i = 0; i < a.size(); i++) a[i] = ra[i * 4];    /* canal rojo de un PNG en escala de grises */
  for (size_t i = 0; i < b.size(); i++) b[i] = rb[i * 4];
  return true;
#else
  (void)opts; (void)a; (void)aw; (void)ah; (void)b; (void)bw; (void)bh;
  return false;
#endif
}

}  // namespace

#ifdef HAVE_CAPTURE
/* Estado de la ventana capturada tal y como lo ve el compositor: sirve para distinguir
   dos fallos distintos cuando la imagen se congela (la ventana dejó de estar visible,
   frente al flujo de PipeWire que se quedó parado). */
/* Un navegador (Gecko o Chromium) pinta al ritmo del sincronismo que le da el compositor: si
   lo tapamos por completo deja de recibirlo y se para. Los reproductores normales no dependen
   de ese aviso, asi que solo tiene sentido avisar para navegadores. */
static bool pareceNavegador(const std::string &estado) {
  static const char *nombres[] = {"firefox", "zen", "chrom", "brave", "opera", "edge", "vivaldi", "thunderbird"};
  for (const char *n : nombres)
    if (estado.find(n) != std::string::npos) return true;
  return false;
}

static std::string estadoVentana(const std::string &uuid, bool *viva) {
  if (viva) *viva = true;
#ifdef HAVE_WAYLAND_WM
  WindowMonitor wm;
  std::string err;
  if (!wm.open(&err)) return "sin acceso al compositor: " + err;
  wm.settle();
  WindowInfo w;
  if (!wm.find(uuid, w)) {
    if (viva) *viva = false;
    return "la ventana ya no existe";
  }
  return w.appId + " · " + WindowInfo::stateList(w.states) + " · " + std::to_string(w.w) + "x" + std::to_string(w.h);
#else
  (void)uuid;
  return {};
#endif
}
#endif

int runViewerVk(const ViewerOpts &opts, SourceKind kind) {
  std::string sock = ensureWaylandEnv();
  if (sock.empty())
    std::fprintf(stderr, "[aviso] no se detecta sesion Wayland (WAYLAND_DISPLAY vacio): glfw puede fallar\n");
  else
    std::printf("[sesion] Wayland %s\n", sock.c_str());

  if (kind == SourceKind::Mpv) {
    std::fprintf(stderr,
                 "[error] el reproductor mpv sigue por OpenGL. Reproducemelo con Vulkan asi:\n"
                 "           mpv --fullscreen <video>   y deja que el servicio te pregunte\n"
                 "         (o usa el visor de siempre: odyssey3d weave ... --gl)\n");
    return 2;
  }

  glfwSetErrorCallback(vkGlfwError);
  if (!glfwInit()) {
    std::fprintf(stderr, "[error] glfwInit fallo\n");
    return 1;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
  if (opts.borderless) {
    /* KWin 6.8 deja de repintar una ventana a pantalla completa cuando OTRA ventana también
       a pantalla completa la tapa por completo: con un navegador (YouTube) el flujo de
       fotogramas se seca a los pocos segundos y la trama queda congelada. Para no provocar
       eso, aquí la ventana se crea sin bordes y maximizada: ocupa la salida entera sin el
       estado fullscreen del compositor, y `floating` (KeepAbove) para quedar por encima. */
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
  }

  Rig rig;
  rig.bringUp(opts);
  glfwPollEvents(); /* que GLFW vea el cambio de modo del panel */

  GLFWmonitor *mon = pickMonitor(opts.output.empty() ? rig.output : opts.output);
  if (!mon) {
    std::fprintf(stderr, "[error] no hay ninguna salida de pantalla\n");
    rig.shutDown();
    glfwTerminate();
    return 1;
  }
  const GLFWvidmode *vm = glfwGetVideoMode(mon);
  int W = opts.w > 0 ? opts.w : (vm ? vm->width : 3840);
  int H = opts.h > 0 ? opts.h : (vm ? vm->height : 2160);
  int hz = refreshOf(mon, opts.hz);
  std::printf("[pantalla] %s %dx%d@%d Hz\n", glfwGetMonitorName(mon), W, H, hz);

  GLFWwindow *win = glfwCreateWindow(W, H, "Odyssey 3D", (opts.windowed || opts.borderless) ? nullptr : mon, nullptr);
  if (!win) {
    const char *desc = nullptr;
    int code = glfwGetError(&desc);
    std::fprintf(stderr, "[error] no se pudo crear la ventana (glfw %d): %s\n", code, desc ? desc : "?");
    rig.shutDown();
    glfwTerminate();
    return 1;
  }
  if (!opts.windowed && !opts.borderless) glfwSetWindowMonitor(win, mon, 0, 0, W, H, hz);
  if (opts.borderless) {
    /* maximizada no vale: el panel de Plasma se guarda una franja y la ventana quedaría
       42 px mas baja, así que la ponemos en el origen de la salida con el tamano del modo. */
    int mx = 0, my = 0, mw = 0, mh = 0;
    glfwGetMonitorWorkarea(mon, &mx, &my, &mw, &mh);
    if (opts.posX || opts.posY) { mx = opts.posX; my = opts.posY; }
    glfwSetWindowPos(win, mx, my);
    std::printf("[ventana] sin bordes en (%d,%d), area util de la salida %dx%d\n", mx, my, mw, mh);
  }
  int fbW = 0, fbH = 0;
  glfwGetFramebufferSize(win, &fbW, &fbH);
  float sx = 1.0f, sy = 1.0f;
  glfwGetWindowContentScale(win, &sx, &sy);
  if (fbW != W || fbH != H || std::fabs(sx - 1.0f) > 0.01f)
    std::fprintf(stderr,
                 "[aviso] la ventana es %dx%d (escala de contenido %.2f) y el modo %dx%d: la trama necesita pixel a "
                 "pixel. Deja la escala del panel en 1 y la rotacion normal.\n",
                 fbW, fbH, sx, W, H);

  VkUi ui;
  ui.fbW = fbW;
  ui.fbH = fbH;
  /* La interfaz del visor se mide en pixeles FISICOS del panel, no en los puntos logicos del
     escritorio. Al poner el modo 3D el panel se queda con escala 1 (la escala del 150% del
     escritorio se va con el modo 2), y si la escala de la interfaz sale del tamano logico en el
     3D sale a 1,0: la pastilla se ve a media altura de lo comodo. Midiendola en pixeles fisicos
     la pastilla mide siempre lo mismo, con el panel en modo 2 o en modo 3. La tarjeta del hub si
     se mide en puntos logicos, porque esa vive en el escritorio y no encima del panel en 3D. */
  {
    int lw = 0, lh = 0;
    glfwGetWindowSize(win, &lw, &lh);
    if (lh <= 0) lh = (int)std::lround((float)fbH / std::max(0.25f, sx));
    ui.logH = lh;
    std::printf("[ventana] lienzo %dx%d fisicos, ventana logica %dx%d: la interfaz se dibuja a %.2f veces\n",
                fbW, fbH, lw, lh, std::max(1.0, std::min(2.0, (double)fbH / 1080.0)));
  }
  ui.win = win;
  ui.rig = &rig;
  ui.opts = &opts;
  ui.tune.weave = !opts.no3d;
  ui.tune.topBottom = opts.topBottom;
  ui.tune.align = (float)opts.align;
  ui.tune.pitch = (float)opts.pitch;
  ui.hud.state().weaveOn = ui.tune.weave;
  /* La tarjeta de "que formato tiene el video" solo sirve si nadie lo eligio antes: si el hub
     o la linea de ordenes ya lo dijeron (--sbs, --tb, --2d), no la volvemos a pintar encima
     del video. */
  ui.hud.state().ask = (kind == SourceKind::Capture) && !opts.noHud && !opts.fmtGiven;
  ui.hud.state().overlayOn = !opts.noHud;
  ui.hud.state().ratonLibre = !opts.noPassthrough;
  /* tamano de la pastilla pedido a mano (--ui-escala); en marcha se ajusta con Ctrl+Shift+7 / 8 */
  if (opts.uiScale > 0.25f) {
    ui.hud.setScale(opts.uiScale);
    std::printf("[pastilla] tamano fijo x%.2f (Ctrl+Shift+7 mas pequena, Ctrl+Shift+8 mas grande)\n",
                opts.uiScale);
  }
  glfwSetWindowUserPointer(win, &ui);
  glfwSetKeyCallback(win, onKeyVK);
  glfwSetCursorPosCallback(win, onCursorVK);
  glfwSetCursorEnterCallback(win, onEnterVK);
  glfwSetMouseButtonCallback(win, onButtonVK);
  glfwSetWindowFocusCallback(win, onFocusVK);
  abrirRegionEntrada(win, &ui.region);
  /* cursor visible, como en el hub oficial */
  GLFWcursor *arrow = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
  if (arrow) glfwSetCursor(win, arrow);

  /* ---- Vulkan: instancia con las extensiones que pide GLFW, superficie, dispositivo, intercambio ---- */
  std::string err;
  uint32_t nec = 0;
  const char **glfwExts = glfwGetRequiredInstanceExtensions(&nec);
  std::vector<const char *> instExts;
  for (uint32_t i = 0; i < nec; i++) instExts.push_back(glfwExts[i]);
  instExts.push_back("VK_KHR_get_surface_capabilities2");

  VkDev dev;
  if (!dev.init(instExts, &err)) {
    std::fprintf(stderr, "[error] Vulkan: %s\n", err.c_str());
    glfwDestroyWindow(win);
    rig.shutDown();
    glfwTerminate();
    return 1;
  }
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  if (glfwCreateWindowSurface(dev.instance(), win, nullptr, &surface) != VK_SUCCESS) {
    std::fprintf(stderr, "[error] GLFW no pudo crear la superficie Vulkan de la ventana\n");
    glfwDestroyWindow(win);
    rig.shutDown();
    glfwTerminate();
    return 1;
  }
  if (!dev.createDevice({"VK_KHR_swapchain"}, surface, &err)) {
    std::fprintf(stderr, "[error] dispositivo Vulkan: %s\n", err.c_str());
    vkDestroySurfaceKHR(dev.instance(), surface, nullptr);
    glfwDestroyWindow(win);
    rig.shutDown();
    glfwTerminate();
    return 1;
  }
  std::printf("[vulkan] %s · %s\n", dev.deviceName().c_str(), dev.hasDmaBufImport() ? "dma-buf disponible" : "sin dma-buf");

  VkWeave wv;
  if (!wv.init(dev, surface, fbW, fbH, &err)) {
    std::fprintf(stderr, "[error] trazador Vulkan: %s\n", err.c_str());
    vkDestroySurfaceKHR(dev.instance(), surface, nullptr);
    glfwDestroyWindow(win);
    dev.destroy();
    rig.shutDown();
    glfwTerminate();
    return 1;
  }
  wv.setPanelPixels(fbW, fbH);
  wv.setField(LensField{}, EyePose{opts.eyeX, opts.eyeY, opts.eyeZ});

  /* conversor de video normal a 3D: la red de profundidad corre en este mismo hilo, pero sólo
     cada varios cuadros. Medido: unos 7 milisegundos con la tarjeta NVIDIA, y el presupuesto por
     cuadro en el modo 3D es de 8, así que hacerla en cada cuadro hundiría la tasa de imagen. */
#ifdef HAVE_ORT
  DepthNet red;
  std::vector<uint8_t> chico, mapa;
  bool convActivo = opts.conver;
  int convSalto = 0;
  long long convHechas = 0, convFallos = 0;
  double convMs = 0.0, ultimaEstim = 0.0;
  int cadaConv = opts.convEach;
  bool apoyoAuto = true;      /* el plano de apoyo lo sigue la red; Ctrl+Shift+3/4 lo vuelve manual */
  int profundidadVista = ui.hud.state().depth;
   /* La red puede tardar cientos de ms si le toca la CPU (cuando las dos tarjetas estan llenas).
      Para no congelar el cuadro, la estimacion corre en su propio hilo: aqui solo se reduce la
      imagen en la tarjeta y se le manda la copia; el mapa vuelve cuando esta terminado y se sube
      en este hilo, que es el dueno de Vulkan. */
   struct HiloRed {
     std::thread hilo;
     std::mutex candado;
     std::vector<uint8_t> mapa;   /* mapa terminado y esperando subirse */
     std::string error;
     float ms = 0.0f;
     bool terminado = false;
     bool ocupado = false;        /* solo lo toca el hilo del visor */
     ~HiloRed() {
       if (hilo.joinable())
         hilo.join();
     }
     /* quedarse quieto hasta que termine el mapa en el aire: hace falta antes de cambiar de red */
     void esperar() {
       if (hilo.joinable())
         hilo.join();
       ocupado = false;
     }
     void arrancar(DepthNet *red, const uint8_t *chico, int sz) {
       if (hilo.joinable())
         hilo.join();
       std::vector<uint8_t> copia(chico, chico + (size_t)sz * (size_t)sz * 4);
       ocupado = true, terminado = false, error.clear();
       hilo = std::thread([this, red, copia = std::move(copia), sz]() {
         std::vector<uint8_t> m;
         std::string e;
         const bool ok = red->estimate(copia.data(), sz, sz, m, &e);
         std::lock_guard<std::mutex> g(candado);
         mapa = ok ? std::move(m) : std::vector<uint8_t>{};
         error = ok ? std::string() : e;
         ms = red->lastMs();
         terminado = true;
       });
     }
     /* 0 = sigue trabajando, 1 = mapa listo (en `fuera`), -1 = fallo (en `errFuera`) */
     int recoger(std::vector<uint8_t> &fuera, float &msFuera, std::string &errFuera) {
       if (!ocupado)
         return 0;
       std::lock_guard<std::mutex> g(candado);
       if (!terminado)
         return 0;
       ocupado = false, terminado = false;
       msFuera = ms;
       if (!error.empty()) {
         errFuera.swap(error);
         return -1;
       }
       fuera.swap(mapa);
       return fuera.empty() ? -1 : 1;
     }
   } hiloRed;
  /* Las dos redes del boton 1 / 2: la que mande el servicio (--modelo) y la otra (--alterno). Lo
     que no venga por bandera se rellena por convencion si el archivo esta en su carpeta, y si el
     usuario ya eligio una vez (el boton la guarda) esa manda al arrancar. */
  auto anotarRed = [&](const std::string &r) {
    if (!r.empty())
      ui.rutaRed[slotRed(r)] = r;
  };
  anotarRed(opts.modelPath);
  anotarRed(opts.altModel);
  if (ui.rutaRed[2].empty() && fileExists(rutaRedNuestra()))
    ui.rutaRed[2] = rutaRedNuestra();
  /* "opts" es de solo lectura, asi que la red con la que arrancamos vive aqui: la preferencia
     guardada manda sobre lo que venga por bandera, y si no vino ninguna se usa la nuestra. */
  std::string modeloActivo = opts.modelPath;
  if (const int pref = leerRedPreferida(); pref && !ui.rutaRed[pref].empty())
    modeloActivo = ui.rutaRed[pref];
  else if (modeloActivo.empty())
    modeloActivo = ui.rutaRed[2];
  ui.hud.state().red = modeloActivo.empty() ? 2 : slotRed(modeloActivo);
  ui.hud.state().redViva = false;
  int convGpu = opts.convGpu;   /* se sube solo a la 1 si la 0 esta llena (strata, juegos, ...) */
  auto arrancarRed = [&](int &gpu, std::string *e) {
    if (red.init(modeloActivo, e, gpu)) return true;
    if (gpu == 0) {   /* la GPU de la pantalla suele estar ocupada: probar la siguiente tarjeta */
      std::fprintf(stderr, "[conversor] %s\n[conversor] se prueba en la grafica 1 (se fuerza con --red N)\n",
                   e->c_str());
      std::fflush(stderr);
      if (red.init(modeloActivo, e, 1)) {
        gpu = 1;
        return true;
      }
    }
    return false;
  };
  if (convActivo) {
    std::string rerr;
    if (!arrancarRed(convGpu, &rerr)) {
      std::fprintf(stderr, "[conversor] %s\n", rerr.c_str());
      convActivo = false;
    } else if (!wv.prepareConverter(red.size(), &rerr)) {
      std::fprintf(stderr, "[conversor] %s\n", rerr.c_str());
      convActivo = false;
    } else {
      if (red.device() == "CPU" && opts.convEach < 10) {
        cadaConv = 10;   /* la red por CPU tarda ~150 ms: no tiene sentido pedirla cada 3 cuadros */
        std::printf("[conversor] la red va por CPU: la profundidad se recalcula cada %d cuadros "
                    "(se cambia con --cada N)\n", cadaConv);
      }
      /* sin desplazamiento no hay relieve: arrancar en el termino medio de la curva nueva, que
         es el presupuesto del programa de Samsung (unos 22,6 px de separacion entre ojos a 3840
         de ancho, repartidos 12,67 px por ojo hacia afuera y 9,98 px hacia adentro). */
      ui.hud.state().popout = 50;
      if (ui.hud.state().depth == 50) ui.hud.state().depth = 55;   /* algo por delante, sin pasarse */
      apoyoAuto = true;
      profundidadVista = ui.hud.state().depth;
      std::printf("[conversor] video normal a 3D con %s, cada %d cuadros (tecla C para apagar)\n",
                  red.device().c_str(), cadaConv);
      std::printf("[conversor] arranque: relieve %d (%.0f px de separacion entre ojos a %d px de ancho, "
                  "%s), plano de apoyo automatico -- Ctrl+Shift+6 sube el relieve, "
                  "Ctrl+Shift+5 lo baja\n",
                  ui.hud.state().popout, gananciaRelieve(ui.hud.state().popout) * 3840.0, 3840,
                  topesPorOjoActivos() ? "tope por ojo +12,67 / -9,98 px" : "sin tope por ojo");
      std::printf("[conversor] limpieza del mapa: filtro guiado (radio 8) + dilate 3 + corte de plano "
                  "(umbral 0,12, descanso 15 cuadros); apoyo por saliencia\n");
      ui.convInfo = true;
      ui.extraInfo = "relieve 2D a 3D: Ctrl+Shift+6 subelo, Ctrl+Shift+5 lo baja";
      ui.extraHasta = glfwGetTime() + 9.0;
      std::fflush(stdout);
    }
  }
#else
  if (opts.conver)
    std::fprintf(stderr, "[conversor] este programa se compiló sin ONNX Runtime: no puede convertir "
                         "video normal a 3D (paquete onnxruntime-cuda)\n");
#endif

  /* rastreo de cabeza: un hilo mira la camara IR y aqui se lee lo ultimo que vio */
#ifdef HAVE_OPENCV
  TrackFeed feed;
  bool rastreoVivo = false;
  if (opts.track) {
    TrackFeedOptions fo;
    fo.device = opts.trackDevice;
    fo.model = opts.trackModel;
    fo.calibDir = opts.trackCalibDir;
    fo.set.model = opts.trackModel;
    fo.set.score = opts.trackScore;
    fo.set.smoothing = opts.trackSmoothing;
    std::string trk;
    if (!feed.start(fo, &trk))
      std::fprintf(stderr, "[rastreo] no arranco: %s\n", trk.c_str());
    else {
      rastreoVivo = true;
      const TrackFeedState fs = feed.state();
      std::printf("[rastreo] %s en %s, buscando la cara\n", fs.camera.c_str(), fs.mode.c_str());
    }
  }
#else
  if (opts.track)
    std::fprintf(stderr, "[aviso] este binario se compilo sin OpenCV: no hay rastreo de cabeza\n");
#endif

  /* atlas de texto de la interfaz (FreeType), subido una sola vez */
  std::string herr;
  if (ui.hud.buildAtlas(&herr))
    wv.setAtlas(ui.hud.atlasPixels(), ui.hud.atlasW(), ui.hud.atlasH());
  else
    std::fprintf(stderr, "[aviso] interfaz sin texto: %s\n", herr.c_str());

  if (!opts.noCorr) {
    std::vector<uint8_t> a, b;
    int aw = 0, ah = 0, bw = 0, bh = 0;
    if (loadCorrections(opts, a, aw, ah, b, bw, bh)) {
      wv.setCorrections(a.data(), aw, ah, b.data(), bw, bh);
      std::printf("[correccion] mapas %dx%d y %dx%d\n", aw, ah, bw, bh);
    }
  }

#ifdef HAVE_CAPTURE
  WindowCapture cap;
  uint64_t lastFrameCount = 0;
  double ultimoNuevo = 0.0;   /* momento del último fotograma nuevo del compositor */
  int avisosCorte = 0;
  int avisosFlujo = 0;
  double ultimoPresente = 0.0;
  bool haveCap = false;
  bool sizeWarned = false, importWarned = false;
  Mpris media;                       /* pausar/reanudar el video del navegador sin quitar la capa */
  double nextMediaCheck = 0.0;
  int reanudados = 0;
  if (kind == SourceKind::Capture) {
    if (!cap.start(opts.captureUuid, &err)) {
      std::fprintf(stderr, "[error] captura: %s\n", err.c_str());
      wv.destroy();
      vkDestroySurfaceKHR(dev.instance(), surface, nullptr);
      glfwDestroyWindow(win);
      dev.destroy();
      rig.shutDown();
      glfwTerminate();
      return 1;
    }
    haveCap = true;
    std::printf("[captura] pidiendo el flujo de la ventana %s\n", opts.captureUuid.c_str());
    /* el formato lo eligio el usuario en la tarjeta; si se equivoco, la imagen se ve doble
       y desplazada, que es la queja tipica: decir como se deshace sin cerrar la sesion */
    std::printf("[formato] %s. Si el video no tiene dos imagenes asi, se vera doble y movido:"
                " tecla 3 para 2D plano o Ctrl+Shift+1 para apagar el 3D\n",
                opts.topBottom ? "arriba y abajo (una imagen sobre otra)" : "lado a lado (una imagen al lado de otra)");
    std::fflush(stdout);
    ui.fmtInfo = std::string(" · ") + (opts.topBottom ? "arriba y abajo" : "lado a lado");
    std::string merr;
    if (media.open(&merr))
      std::printf("[video] mando el video por MPRIS (%s): P lo pausa y lo reanuda\n", media.playerName().c_str());
    else
      std::printf("[video] sin reproductor MPRIS (%s): el video habra que pausarlo a mano\n", merr.c_str());
    ui.media = &media;
    std::fflush(stdout);
  }
#endif

  std::vector<uint8_t> testPx;
  bool haveTest = false;
  bool testAnim = (size_t)fbW * (size_t)fbH <= (size_t)2000000; /* en 4K el patrón se dibuja una vez */
  double animNext = 0.0;

  std::printf("\n  teclas  ESC salir · 3 2D/3D · F invertir profundidad · ←/→ fase · RePag/SavPg paso de lente\n"
              "          ↑/↓ crosstalk · , . pendiente de filtro · 9/0 convergencia · 8 reiniciar ajuste\n");
  std::printf("          raton  botones y deslizadores de la pastilla · Ctrl+Shift 1..6 como el hub oficial\n");
  std::printf("                 fuera de la pastilla el raton atraviesa el visor: pausar el video, mover la\n");
  std::printf("                 barra o manejar el navegador sin quitar el 3D (--sin-pasar-raton lo apaga)\n");
  std::printf("                 Ctrl+Shift+7 achica la pastilla · Ctrl+Shift+8 la agranda\n");
  std::printf("          P o espacio  pausa o reanuda el video · Tab entrega la pantalla al navegador (nos\n");
  std::printf("          minimizamos y el panel vuelve a 2D; Meta+Alt+Tab o un clic en la barra de tareas la\n          "
              "devuelve y el 3D se reanuda solo\n");
  std::printf("          teclas sueltas  3 sin tramado (2D) · F invierte los ojos · M cambia el formato · 1 y 2 cambian la red\n"
              "                            (lado a lado, arriba y abajo, 2D plano) · C conversor de video normal a 3D\n\n");
  std::fflush(stdout);

  double t0 = glfwGetTime(), clock = t0, lastPoll = 0.0, ultimoAviso = 0.0, ultimoRastreo = 0.0;
  int frames = 0, errStreak = 0, reintentos = 0;
  bool dumpHecho = false;
  std::signal(SIGTERM, onTermSignal);
  std::signal(SIGINT, onTermSignal);
  while (!glfwWindowShouldClose(win) && !ui.quit) {
    if (gTerm && !ui.quit) {
      std::printf("[sesion] llego una senal de cierre: se sale devolviendo el panel, la lente y el scaler\n");
      std::fflush(stdout);
      ui.quit = true;
    }
    /* depuracion de la tarjeta del hub y de la pastilla: ODYSSEY_GOLPE simula clics sin tocar el
       raton de verdad. Acepta coordenadas logicas "x,y" o el nombre de un boton ("red" = el boton
       de la red de profundidad), y varios golpes separados por ";", aplicados uno cada pocos
       cuadros (hace falta al menos un cuadro trazado para saber donde cae cada control). */
    if (const char *g = std::getenv("ODYSSEY_GOLPE")) {
      static std::vector<std::string> goles;
      static int hechos = 0;
      if (goles.empty()) {
        std::string tok;
        for (char c : std::string(g)) {
          if (c == ';') {
            if (!tok.empty()) { goles.push_back(tok); tok.clear(); }
          } else if (c != ' ')
            tok += c;
        }
        if (!tok.empty()) goles.push_back(tok);
      }
      if (hechos < (int)goles.size() && frames >= 4 + hechos * 12) {
        const std::string tok = strLower(goles[hechos]);
        float cx = 0.0f, cy = 0.0f;
        bool tiene = false;
        if (tok == "red") {
          float rx = 0, ry = 0, rw = 0, rh = 0;
          if (ui.hud.redBounds(&rx, &ry, &rw, &rh)) {
            /* hit() trabaja en pixeles reales de la ventana y la caja viene en el espacio virtual */
            cx = (rx + rw * 0.5f) * ui.hud.scale(), cy = (ry + rh * 0.5f) * ui.hud.scale();
            tiene = true;
          }
        } else {
          float kx = 1.0f, ky = 1.0f;
          pointerScale(win, &ui, kx, ky);
          int gx = 0, gy = 0;
          if (std::sscanf(tok.c_str(), "%d,%d", &gx, &gy) == 2) {
            cx = (float)gx * kx, cy = (float)gy * ky;
            tiene = true;
          }
        }
        const Hud::Hit hit = tiene ? ui.hud.hit(cx, cy) : Hud::HitNone;
        float ax = 0, ay = 0, aw = 0, ah = 0;
        ui.hud.pillBounds(&ax, &ay, &aw, &ah);
        std::printf("[golpe] %s -> lienzo (%.0f,%.0f) -> golpe %d · pastilla %.0f,%.0f %.0fx%.0f escala %.2f\n",
                    tok.c_str(), (double)cx, (double)cy, (int)hit, (double)ax, (double)ay, (double)aw, (double)ah,
                    (double)ui.hud.scale());
        std::fflush(stdout);
        if (tiene) applyHit(ui, hit);
        hechos++;
      }
    }
    glfwPollEvents();
#ifdef HAVE_ORT
    /* la tecla C: encender o apagar el conversor de video normal a 3D */
    if (ui.convToggle || ui.convWant) {
      const bool pidiendo = ui.convWant;
      ui.convToggle = false, ui.convWant = false;
      std::string cerr0;
      if (!convActivo) {
        if (!red.ready() && !arrancarRed(convGpu, &cerr0)) {
          std::fprintf(stderr, "[conversor] %s\n", cerr0.c_str());
        } else if (!wv.converterReady() && !wv.prepareConverter(red.size(), &cerr0)) {
          std::fprintf(stderr, "[conversor] %s\n", cerr0.c_str());
        } else {
          convActivo = true;
          ui.hud.state().weaveOn = true;   /* hace falta la trama: el relieve sale de ahi */
          ui.tune.topBottom = false;
          if (ui.hud.state().popout < 50) ui.hud.state().popout = 50;   /* plano = sin relieve */
          if (ui.hud.state().depth == 50) ui.hud.state().depth = 55;
          apoyoAuto = true;
          profundidadVista = ui.hud.state().depth;
          std::printf("[conversor] video normal a 3D encendido con %s (tecla C para apagar): "
                      "relieve %d, apoyo automatico -- Ctrl+Shift+6 sube, Ctrl+Shift+5 baja\n",
                      red.device().c_str(), ui.hud.state().popout);
          ui.convInfo = true;
          ui.extraInfo = "relieve 2D a 3D: Ctrl+Shift+6 subelo, Ctrl+Shift+5 lo baja";
          ui.extraHasta = glfwGetTime() + 9.0;
        }
      } else if (!pidiendo) {
        convActivo = false;
        std::printf("[conversor] video normal a 3D apagado\n");
        ui.convInfo = false;
      }
      std::fflush(stdout);
    }
    /* el boton 1 / 2 de la pastilla: pasar de la red de Samsung a la nuestra sin cerrar el visor.
       Se atiende aqui, en el hilo del visor y con el hilo de la estimacion parado: mover la sesion
       de ONNX mientras ese hilo trabaja reventaria el cuadro en curso. La red nueva se abre antes
       de soltar la vieja? No: DepthNet::cambiar suelta la vieja primero para devolver la memoria a
       la tarjeta, y si la nueva no abre vuelve a cargar la anterior. */
    if (ui.redPedida) {
      const int quiero = ui.redPedida;
      const int ahora = slotRed(red.modelPath());
      if (quiero != 1 && quiero != 2) {
        ui.redPedida = 0;
      } else if (red.ready() && quiero == ahora) {
        ui.redPedida = 0;                      /* ya estamos en esa red: nada que hacer */
      } else if (ui.rutaRed[quiero].empty()) {
        ui.redPedida = 0;
        std::fprintf(stderr, "[conversor] no sabemos donde esta la red %d (%s): pasele --alterno RUTA\n",
                     quiero, nombreRed(quiero));
        std::fflush(stderr);
      } else if (!red.ready()) {
        ui.redPedida = 0;                      /* sin conversor no hay nada que cambiar: se apunta */
        ui.hud.state().red = quiero;
        grabarRedPreferida(quiero);
        std::printf("[conversor] apuntada la red %d (%s): se abrira al encender el conversor (tecla C)\n",
                    quiero, nombreRed(quiero));
        std::fflush(stdout);
      } else if (hiloRed.ocupado) {
        /* espera unos milisegundos a que termine el mapa que esta en el aire */
      } else {
        ui.redPedida = 0;
        hiloRed.esperar();
        std::string cerr1;
        if (red.cambiar(ui.rutaRed[quiero], &cerr1)) {
          if (wv.converterSize() != red.size() && !wv.prepareConverter(red.size(), &cerr1))
            std::fprintf(stderr, "[conversor] %s\n", cerr1.c_str());
          ui.hud.state().red = quiero;
          grabarRedPreferida(quiero);
          std::printf("[conversor] cambiada a la red %d (%s) en %s · mapa %d x %d\n", quiero,
                      nombreRed(quiero), red.device().c_str(), red.size(), red.size());
          std::fflush(stdout);
          ui.extraInfo = std::string("red ") + (quiero == 1 ? "1 (Samsung) · " : "2 (nuestra) · ") + red.device();
          ui.extraHasta = glfwGetTime() + 6.0;
        } else {
          std::fprintf(stderr, "[conversor] %s\n", cerr1.c_str());
          ui.hud.state().red = slotRed(red.modelPath());
          std::fflush(stderr);
          ui.extraInfo = "la otra red no abrio: seguimos con la anterior";
          ui.extraHasta = glfwGetTime() + 9.0;
        }
      }
    }
    ui.hud.state().redViva = red.ready();
#endif
    /* Entregar y recuperar la pantalla (Tab). Somos una ventana a pantalla completa por encima
       del navegador, asi que la unica forma de que el raton llegue al video es irnos nosotros:
       apagamos el 3D del panel y nos minimizamos. Mientras estamos aparte no tiene sentido
       seguir trazando cuadros, y al volver a recibir el foco se enciende todo otra vez. */
    if (ui.entregar) {
      ui.entregar = false;
      ui.entregada = true;
      escribirEntregada();
      rig.shutDown();
      glfwIconifyWindow(win);
      std::printf("[pantalla] entregada: el panel vuelve a su modo normal y esta ventana se minimiza."
                  "  Vuelve a ella con Meta+Alt+Tab (el cambio de ventanas que tambien muestra las minimizadas)"
                  " o con un clic en la barra de tareas, y el 3D se pone solo\n");
      std::fflush(stdout);
      continue;
    }
    if (ui.recuperar) {
      ui.recuperar = false;
      if (!ui.entregada) continue;
      ui.entregada = false;
      borrarEntregada();
      rig.bringUp(opts);
      glfwGetFramebufferSize(win, &fbW, &fbH);
      ui.fbW = fbW;
      ui.fbH = fbH;
      if (!wv.resize(fbW, fbH))
        std::fprintf(stderr, "[vulkan] al recuperar la pantalla el intercambio no se rehizo\n");
      std::printf("[pantalla] recuperada: lente, scaler y modo 3D otra vez puestos\n");
      std::fflush(stdout);
      continue;
    }
    if (ui.entregada) {
      glfwWaitEventsTimeout(0.12); /* esperando que alguien nos devuelva el foco */
      continue;
    }
    double now = glfwGetTime();
    if (opts.segundos > 0 && now - t0 >= (double)opts.segundos) {
      std::printf("[sesion] %d segundos cumplidos, saliendo con la secuencia normal\n", opts.segundos);
      break;
    }

    /* la fuente de este cuadro */
    bool newSource = false;
    if (kind == SourceKind::Test) {
      if (!haveTest || (testAnim && now >= animNext)) {
        makeTestSbs(testPx, fbW, fbH, now - t0, 34.0);
        newSource = wv.sourceFromRgba(testPx.data(), fbW, fbH, true, &err);
        if (!newSource && !importWarned) {
          importWarned = true;
          std::fprintf(stderr, "[prueba] %s\n", err.c_str());
        }
        animNext = now + 0.12;
        haveTest = true;
      }
#ifdef HAVE_CAPTURE
    } else if (kind == SourceKind::Capture && haveCap) {
      cap.pump();
      if (cap.frameCount() != lastFrameCount) {
        lastFrameCount = cap.frameCount();
        ultimoNuevo = now;
        reintentos = 0;  /* el flujo revive: presupuesto nuevo de reintentos */
        CapturedFrame cf;
        if (cap.lastFrame(cf)) {
          GpuFrame gf;
          gf.fd = cf.fd;
          gf.w = cf.w;
          gf.h = cf.h;
          gf.stride = cf.stride;
          gf.offset = cf.offset;
          gf.modifier = cf.modifier;
          gf.fourcc = cf.fourcc;
          gf.dmabuf = cf.dmabuf;
          if (!sizeWarned && (cf.w != fbW || cf.h != fbH)) {
            sizeWarned = true;
            std::fprintf(stderr,
                         "[captura] el fotograma del compositor es %dx%d y el panel esta en %dx%d: la trama "
                         "necesita pixel a pixel (deja la ventana a pantalla completa en esta salida)\n",
                         cf.w, cf.h, fbW, fbH);
          }
          if (!wv.sourceFromDma(gf, &err) && !importWarned) {
            importWarned = true;
            std::fprintf(stderr,
                         "[captura] no se pudo importar el fotograma: %dx%d paso %d offset %d fourcc %c%c%c%c "
                         "modificador 0x%llx descriptor %d\n",
                         gf.w, gf.h, gf.stride, gf.offset,
                         char(gf.fourcc & 0xff), char((gf.fourcc >> 8) & 0xff), char((gf.fourcc >> 16) & 0xff),
                         char((gf.fourcc >> 24) & 0xff), (unsigned long long)gf.modifier, gf.fd);
            std::fprintf(stderr, "[captura] %s\n", err.c_str());
          }
          newSource = true;
        }
      }
      /* El navegador suele dejar el video en pausa cuando cambiamos el modo del monitor o
         cuando la ventana queda tapada por la nuestra. Como el flujo sigue llegando, el visor
         no se da cuenta: miramos el estado MPRIS y lo reanudamos, salvo que el usuario lo
         pausara el con P. */
      if (media.playerFound() && now >= nextMediaCheck) {
        nextMediaCheck = now + 3.0;
        std::string mst, merr;
        if (media.status(&mst, &merr) && mst != "Playing" && !ui.mediaPausaManual && reanudados < 6) {
          ++reanudados;
          if (media.resumeIfNeeded(&merr)) {
            std::printf("[video] el navegador habia parado el video (%s): se reanuda solo\n", mst.c_str());
            ui.extraInfo = " · video reanudado";
            ui.extraHasta = now + 8.0;
          } else {
            std::printf("[video] el video estaba parado (%s) y no se pudo reanudar: %s\n", mst.c_str(),
                        merr.c_str());
          }
          std::fflush(stdout);
        }
      }
      if (now - lastPoll > 1.0) {
        lastPoll = now;
        if (!cap.streamCreated()) {
          std::fprintf(stderr, "[captura] %s\n", cap.describe().c_str());
        } else if ((lastFrameCount == 0 && now - t0 > 5.0 && reintentos < 12) ||
                   (ultimoNuevo > 0.0 && now - ultimoNuevo > 2.0)) {
          /* dos casos: el nodo se creo pero no llega nada (el portal tarda o el cambio de modo
             del panel invalida la sesion), o el flujo se paro a mitad de la sesion. En ambos
             hay que pedir el flujo otra vez, y decir por dónde vino el corte. */
          const bool cortado = ultimoNuevo > 0.0;
          if (cortado && avisosCorte < 4) {
            ++avisosCorte;
            ++avisosFlujo;
            std::printf("[captura] %d s sin fotogramas nuevos del compositor (flujo: %s)\n",
                        (int)(now - ultimoNuevo), cap.stateText().c_str());
            bool viva = true;
            const std::string est = estadoVentana(opts.captureUuid, &viva);
            if (!est.empty()) std::printf("[captura] ventana capturada: %s\n", est.c_str());
            if (pareceNavegador(est) && avisosFlujo == 1) {
              std::printf("[captura] el navegador dejo de pintar porque el compositor ya no le manda el sincronismo"
                          " (la tapamos por completo).\n"
                          "            Firefox o Zen: about:config -> gfx.vsync.compositor.unobserve-count = 0"
                          " y reiniciar el navegador\n"
                          "            Chrome o Chromium: arrancarlos con --disable-gpu-vsync\n"
                          "            mpv, VLC y reproductores parecidos no necesitan nada\n");
            }
            if (!viva) {
              std::printf("[captura] la ventana del video cerro: se cierra la sesion y se devuelve el panel a 2D\n");
              std::fflush(stdout);
              ui.quit = true;
            }
            std::fflush(stdout);
          }
          if (!ui.quit) {
            ++reintentos;
            /* el navegador puede tardar en volver a pintar: no nos rendimos a los cuatro intentos,
               pero tampoco imprimimos cada uno */
            if (reintentos <= 3 || reintentos % 5 == 0)
              std::printf("[captura] %s, pidiendo el flujo otra vez (intento %d)\n",
                          cortado ? "el flujo se corto" : "el flujo no arranca", reintentos);
            std::fflush(stdout);
            cap.stop();
            lastFrameCount = 0;
            ultimoNuevo = 0.0;
            std::string e2;
            if (!cap.start(opts.captureUuid, &e2)) std::fprintf(stderr, "[captura] reintentar: %s\n", e2.c_str());
          }
        }
      }
      if (now - ultimoAviso > 5.0 && lastFrameCount > 0) {
        ultimoAviso = now;
        CapturedFrame cf;
        cap.lastFrame(cf);
        std::printf("[captura] %llu fotogramas del compositor, ultimo %dx%d %s\n",
                    (unsigned long long)lastFrameCount, cf.w, cf.h, cf.dmabuf ? "en DMA-BUF" : "en memoria");
      }
#endif
    }
    /* Si el compositor no mando fotograma nuevo y aqui no cambio nada, presentar otra vez lo
       mismo solo consume GPU: con un video 4K60 en el navegador esa GPU extra era justo la que
       faltaba para que el flujo se atragantara. Sin --siempre presentamos a 30 por segundo
       como techo cuando el cuadro no cambia (el panel ya muestra lo mismo). */
    bool algoNuevo = newSource || ui.inputChanged;
#ifdef HAVE_OPENCV
    if (rastreoVivo) algoNuevo = true;
#endif
    if (!opts.alwaysRender && kind == SourceKind::Capture && !algoNuevo &&
        now - ultimoPresente < 1.0 / 30.0)
      continue;

    /* la interfaz se rehace en CPU cada cuadro (son pocos triángulos) y se sube su geometría */
    WeaveTuning &t = wv.tuning();
    t.weave = ui.tune.weave && ui.hud.state().weaveOn;
    t.swapLR = ui.tune.swapLR;
    t.topBottom = ui.tune.topBottom;
    t.filterSlope = ui.tune.filterSlope;
    t.align = ui.tune.align;
    t.pitch = ui.tune.pitch;
    t.xtalkDyn = ui.tune.xtalkDyn;
    t.contrast = ui.tune.contrast;
    t.corrAScale = ui.tune.corrAScale;
    const Hud::State &st = ui.hud.state();
    t.conv = ui.convBase + (float)st.popout / 50.0f * 0.004f;
    wv.setDepthShift(((float)st.depth - 50.0f) / 50.0f * 0.004f);
#ifdef HAVE_ORT
    /* conversor de video normal a 3D: reducir la imagen en la tarjeta, bajarla (1 MB), pasar la
       red y volver a subir el mapa. Se hace cada opts.convEach cuadros y solo cuando el
       compositor mando un cuadro nuevo: la red tarda ~7 ms y el presupuesto por cuadro en el
       modo 3D anda por los 8 ms. */
    if (convActivo && red.ready() && wv.converterReady()) {
      if (std::getenv("ODYSSEY_CONV")) {
        static long long dep = 0;
        if (++dep % 200 == 0)
          std::printf("[conv-debug] t=%.2f ultima=%.2f salto=%d cada=%d fuente=%d nuevo=%d hechas=%lld fallos=%lld\n",
                      now, ultimaEstim, convSalto, opts.convEach, wv.hasSource() ? 1 : 0, newSource ? 1 : 0,
                      convHechas, convFallos);
      }
      /* con el video quieto no hace falta repetir la red, pero tampoco queremos un mapa
         congelado si el cuadro no cambia: como mucho una vez cada medio segundo */
      /* con la red en su hilo solo lanzamos una estimacion nueva cuando la anterior ya se subio */
      if (!hiloRed.ocupado && (newSource || now - ultimaEstim > 0.5) && wv.hasSource() &&
          ++convSalto >= cadaConv) {
        convSalto = 0;
        ultimaEstim = now;
        std::string cerr;
        if (wv.readSmall(chico, &cerr)) {
          hiloRed.arrancar(&red, chico.data(), wv.converterSize());
        } else if (++convFallos == 1 || convFallos % 100 == 0) {
          std::fprintf(stderr, "[conversor] no se pudo leer el cuadro pequeno (%d): %s\n", (int)convFallos,
                       cerr.c_str());
        }
      }
      float msHecho = 0.0f;
      std::string cerr, errHecho;
      const int estado = hiloRed.recoger(mapa, msHecho, errHecho);
      if (estado == 1) {
        convMs = msHecho;
        if (wv.setDepthMap(mapa.data(), red.size(), red.size(), &cerr)) {
          /* con --dump guardamos tambien el mapa del primer cuadro: sirve para ver si la red
             esta entendiendo bien la escena (claro = cerca, oscuro = lejos) */
          if (!opts.dumpPath.empty() && (convHechas == 0 || convHechas == 8)) {
            /* guardamos tambien lo que realmente le llega a la red: si aqui no esta la imagen de
               la ventana capturada, el problema esta en la lectura pequena y no en el modelo */
            const std::string cp = opts.dumpPath + "-chico.ppm";
            const int cs = wv.converterSize();
            FILE *cf = std::fopen(cp.c_str(), "wb");
            if (cf) {
              std::fprintf(cf, "P6\n%d %d\n255\n", cs, cs);
              for (int yy = 0; yy < cs; yy++)
                for (int xx = 0; xx < cs; xx++)
                  std::fwrite(chico.data() + (((size_t)yy * (size_t)cs + (size_t)xx) * 4), 1, 3, cf);
              std::fclose(cf);
              std::printf("[volcado] %s (%dx%d lo que ve la red, estimacion %lld)\n", cp.c_str(), cs, cs,
                          convHechas + 1);
            }
            const std::string mp = opts.dumpPath + "-profundidad.pgm";
            FILE *pf = std::fopen(mp.c_str(), "wb");
            if (pf) {
              std::fprintf(pf, "P5\n%d %d\n255\n", red.size(), red.size());
              std::fwrite(mapa.data(), 1, mapa.size(), pf);
              std::fclose(pf);
              std::printf("[volcado] %s (%dx%d, estimacion %lld)\n", mp.c_str(), red.size(), red.size(),
                          convHechas + 1);
            }
            /* y la trama YA con el mapa puesto: el volcado del principio sale antes de la primera
               estimacion, asi que ahi el desplazamiento por ojo todavia no existe. Esta es la unica
               imagen que deja ver el relieve aplicado y como se aguantan los cantos. */
            if (convHechas == 8) {
              std::vector<uint8_t> trama;
              if (wv.grab(trama, &cerr)) {
                const std::string tp = opts.dumpPath + "-trama-con-mapa.png";
                if (glPngSave(tp, fbW, fbH, trama.data()))
                  std::printf("[volcado] %s (%dx%d con el mapa de profundidad puesto)\n",
                              tp.c_str(), fbW, fbH);
                else
                  std::fprintf(stderr, "[volcado] no se pudo escribir %s\n", tp.c_str());
              } else
                std::fprintf(stderr, "[volcado] trama con mapa: %s\n", cerr.c_str());
            }
          }
          if (red.lastCut())
            std::printf("[conversor] cambio de plano (%ld): se reinicia la escala de profundidad y el apoyo\n",
                        red.cutCount());
          if (++convHechas == 1 || convHechas % 150 == 0)
            std::printf("[conversor] profundidad en %.1f ms con %s (cuadro %lld)\n", convMs,
                        red.device().c_str(), (long long)convHechas);
          if (convHechas == 2 || convHechas % 60 == 0)
            std::printf("[conversor] apoyo %d%% del rango de profundidad (cortes %ld, mapa %d x %d)\n",
                        (int)(red.focusPlane() * 100.0f), red.cutCount(), wv.converterSize(), wv.converterSize());
          std::fflush(stdout);
        } else {
          /* si falla, decirlo en vez de quedarse mudo: casi siempre es el cuadro que no llego a
             importar, y conviene verlo en el registro */
          if (++convFallos == 1 || convFallos % 100 == 0)
            std::fprintf(stderr, "[conversor] fallo (%d): %s\n", (int)convFallos, cerr.c_str());
          if (!convHechas) {
            std::fprintf(stderr, "[conversor] %s\n", cerr.c_str());
            convActivo = false;   /* no insistir cuadro tras cuadro con el mismo fallo */
          }
        }
      } else if (estado == -1) {
        if (++convFallos == 1 || convFallos % 100 == 0)
          std::fprintf(stderr, "[conversor] fallo (%d): %s\n", (int)convFallos, errHecho.c_str());
        if (!convHechas) {
          std::fprintf(stderr, "[conversor] %s\n", errHecho.c_str());
          convActivo = false;   /* la red no dio una sola: no seguir intentandolo cada cuadro */
        }
      }
      /* los dos deslizadores de siempre mandan aqui: "3D Depth" pone el plano que se queda
         quieto y "3D Popout" cuanto se separan la vista izquierda y la derecha */
      /* "3D Salida" (relieve) manda aqui: 0 deja la imagen plana y con 50 las dos vistas se
         separan lo que separa el programa de Samsung (ver gananciaRelieve). Antes de cambiar la
         curva: con 50 px de separacion no se notaba y con 98 molestaba a los ojos. */
      const float ganancia = gananciaRelieve(st.popout);
      /* El plano de apoyo (lo que se queda quieto en la pantalla) lo sigue la red: toma el pico del
         histograma del mapa ya limpio, o sea el sujeto. Con el sujeto apoyado en la pantalla el
         relieve se abre hacia atras y cansa menos la vista con el mismo relieve. Si el usuario mueve
         Ctrl+Shift+3/4 se le deja el suyo; apagar y volver a encender el conversor (tecla C) lo
         devuelve al automatico. */
      if (st.depth != profundidadVista) {
        profundidadVista = st.depth;
        if (apoyoAuto) {
          apoyoAuto = false;
          std::printf("[conversor] plano de referencia manual %d (tecla C dos veces lo devuelve a automatico)\n",
                      st.depth);
          std::fflush(stdout);
        }
      }
      wv.setConverter(ganancia, apoyoAuto ? red.focusPlane() : (float)st.depth / 100.0f,
                      wv.hasDepthMap());
    }
#endif
    ui.hud.state().hud = rig.hud();
    /* la frase extra del video se quita sola, salvo si estamos en pausa a proposito */
    if (!ui.extraInfo.empty() && ui.extraHasta > 0.0 && now >= ui.extraHasta && !ui.mediaPausaManual) {
      ui.extraInfo.clear();
      ui.extraHasta = 0.0;
    }
    ui.hud.setInfo(infoLine(kind, rig, ui.hud.state(), fbW, fbH, ui.extraInfo, ui.fmtInfo, ui.convInfo));
    /* La pastilla ocultada vuelve sola en cuanto el puntero entra en el rincon que dejamos
       pinchable. Hace falta porque el que la oculto suele estar manejando el video: el foco lo
       tiene el navegador y ninguna tecla nuestra le llega, asi que el raton es la unica puerta. */
    if (ui.region.hayHogar && !ui.hud.overlayVisible() && ui.mx >= 0.0 &&
        ui.mx >= ui.region.hogar[0] && ui.mx <= ui.region.hogar[0] + ui.region.hogar[2] &&
        ui.my >= ui.region.hogar[1] && ui.my <= ui.region.hogar[1] + ui.region.hogar[3]) {
      ui.hud.showOverlay();
      std::printf("[pastilla] el puntero llego al rincon de arriba a la derecha: la pastilla vuelve"
                  "  (Ctrl+Shift+2 la oculta otra vez)\n");
      std::fflush(stdout);
    }
    ui.hud.begin(fbW, fbH);   /* escala por pixeles fisicos: ver el comentario del lienzo arriba */
    /* la pastilla cambio de sitio o se oculto: la caja que deja pasar el raton se actualiza aqui */
    aplicarRegionEntrada(win, &ui.region, &ui, !opts.noPassthrough);
    wv.setHud(ui.hud.overlayVisible() ? ui.hud.vertices() : std::vector<HudVertex>{});
    wv.setHudState(rig.hud());

#ifdef HAVE_OPENCV
    if (rastreoVivo) {
      const TrackFeedState fs = feed.state();
      const double altoCm = (double)fbH * wv.field().pxPcm;
      const bool dentro = fs.sample.panelCentimetres[1] >= -50.0 && fs.sample.panelCentimetres[1] <= 50.0 &&
                          fs.sample.panelCentimetres[2] >= 50.0 && fs.sample.panelCentimetres[2] <= 130.0;
      const bool fresco = fs.seen && fs.age >= 0.0 && fs.age <= 0.15 && dentro;
      if (fresco && !opts.trackFixed) {
        const TrackSample &m = fs.sample;
        double x = m.panelCentimetres[0], y = m.panelCentimetres[1], z = m.panelCentimetres[2];
        if (opts.trackFlipX) x = -x;
        if (opts.trackFlipY) y = -y;
        EyePose e;
        e.x = x + opts.trackOffsetX;
        /* la trama nuestra mide y hacia abajo desde el borde de arriba, y la
           transformada del hub la da hacia arriba desde el centro del panel: los
           dos marcos caen iguales justo en la pose nominal (8.3916 cm) */
        e.y = altoCm * 0.5 - y + opts.trackOffsetY;
        e.z = z + opts.trackOffsetZ;
        wv.setEye(e);
      }
      if (now - ultimoRastreo >= 1.0) {
        ultimoRastreo = now;
        const TrackSample &m = fs.sample;
        std::printf("[rastreo] camara (%6.1f, %6.1f, %6.1f) mm · panel (%6.2f, %6.2f, %6.2f) cm · ipd %.1f mm · "
                    "%lld/%lld cuadros (%.1f fps) · error %.2f px · hace %.2f s · %s\n",
                    m.eyeMillimetres[0], m.eyeMillimetres[1], m.eyeMillimetres[2], m.panelCentimetres[0],
                    m.panelCentimetres[1], m.panelCentimetres[2], m.ipdMillimetres, fs.frames, fs.good,
                    fs.cameraFps, m.reprojectionPx, fs.age,
                    fresco ? "siguiendo" : (fs.note.empty() ? "sin pose valida" : fs.note.c_str()));
        std::fflush(stdout);
      }
    }
#endif

    ultimoPresente = now;
    ui.inputChanged = false;
    if (!wv.frame(&err)) {
      if (wv.lost()) {
        glfwGetFramebufferSize(win, &fbW, &fbH);
        if (!wv.resize(fbW, fbH)) {
          std::fprintf(stderr, "[vulkan] el trazador no sobrevivió al cambio de tamaño\n");
          break;
        }
        ui.fbW = fbW;
        ui.fbH = fbH;
      } else if (++errStreak > 30) {
        std::fprintf(stderr, "[vulkan] cuadro: %s\n", err.c_str());
        break;
      }
    } else {
      errStreak = 0;
    }
#ifdef HAVE_PNG
    /* con captura hay que esperar al primer fotograma del compositor: el portal tarda alrededor
       de un segundo en crear el flujo, y si no llegan fotogramas el volcado sale en negro */
    bool fuenteLista = true;
#ifdef HAVE_CAPTURE
    if (kind == SourceKind::Capture) fuenteLista = lastFrameCount > 0;
#endif
    if (!opts.dumpPath.empty() && !dumpHecho && ((fuenteLista && frames >= 4) || frames >= 150)) {
      std::vector<uint8_t> rgba;
      if (wv.grab(rgba, &err)) {
        std::string a = opts.dumpPath + "-trama-vulkan.png";
        if (glPngSave(a, fbW, fbH, rgba.data()))
          std::printf("[volcado] %s (%dx%d leido de Vulkan)\n", a.c_str(), fbW, fbH);
        else
          std::fprintf(stderr, "[volcado] no se pudo escribir %s\n", a.c_str());
        if (kind == SourceKind::Test && !testPx.empty())
          glPngSave(opts.dumpPath + "-origen.png", fbW, fbH, testPx.data());
#ifdef HAVE_CAPTURE
        if (kind == SourceKind::Capture)
          std::printf("[captura] fotogramas recibidos del compositor: %llu · %s\n",
                      (unsigned long long)lastFrameCount, cap.describe().c_str());
#endif
      } else {
        std::fprintf(stderr, "[volcado] %s\n", err.c_str());
      }
      dumpHecho = true;
      /* si ademas se pidio un tiempo (--segundos), el volcado no corta la sesion: deja ver
         el 3D en el panel y sale al cumplirse el tiempo */
      if (opts.segundos <= 0) ui.quit = true;
    }
#endif
    frames++;
    if (now - clock > 5.0) {
      std::printf("[fps] %.1f\n", frames / (now - clock));
      std::fflush(stdout);
      frames = 0;
      clock = now;
    }
  }

#ifdef HAVE_CAPTURE
  if (haveCap) cap.stop();
#endif
  wv.destroy();
  vkDestroySurfaceKHR(dev.instance(), surface, nullptr);
  dev.destroy();
  if (arrow) glfwDestroyCursor(arrow);
  glfwDestroyWindow(win);
  borrarEntregada();   /* si nos fuimos minimizados, el aviso ya no vale para nada */
  rig.shutDown();
  glfwTerminate();
  return 0;
}
