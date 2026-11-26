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
#include "ajuste.h"
#include "barra.h"
#include "mpris.h"
#include "atajos.h"
#include "util.h"
#ifdef HAVE_ORT
#include "depth.h"
#include "hoyos.h"
#include <condition_variable>
#endif
#include "idioma.h"
#include "rendimiento.h"
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
#include <ctime>
#include <map>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <poll.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

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
  /* los atajos puestos (v1.09): combinacion -> accion ("Ctrl+Shift+1" -> "3d"), de serve.conf */
  std::map<std::string, std::string> atajosPuestos;
  WeaveTuning tune{};
  Hud hud;
  RegionEntrada region;
  bool quit = false;
  float convBase = 0.0f;  /* la convergencia que mueven las teclas; la del deslizador se suma */
  double mx = -1.0, my = -1.0;
  Hud::Hit dragging = Hud::HitNone;
  /* La pose del ojo que usa la trama, de un cuadro al siguiente. La trae la camara
     cuando el rastreo esta puesto y entrega una pose fresca; si no, es la nominal de
     siempre (el angulo a mano de la pastilla se quito en v0.34). */
  EyePose ojoBase{};
  bool ojoBasePuesto = false;
  int anguloImpreso = 0;     /* el ultimo angulo del que dimos cuenta en el journal */
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
  std::string extraInfo;     /* el aviso del momento, bajo la pastilla: se quita solo */
  std::string estadoVideo;   /* " · video en pausa": ya no se pinta (lo dice el propio video), queda para depurar */
  bool convInfo = false;     /* contar en la frase de estado que el conversor 2D/3D esta puesto */
  bool convWant = false;     /* la tarjeta del hub pidio "video normal": encender, no alternar */
  bool convFuera = false;    /* el boton de video dividido de la pastilla: apagar, no alternar */
  int modoPedido = 0;        /* los botones de icono: 1 normal · 2 dividido · 3 dividido suavizado (0 = nada) */
  int preajustePedido = -1;  /* un preajuste del modo rendimiento (pastilla u orden `preajuste`): 0..2 */
  int focoPedido = 0;        /* el menu del chip «AI»: 1 = Auto, 2 = Manual (lo atiende el bucle) */
  /* el cursor de los juegos (rama experimental). Con un juego debajo, la pastilla puesta a mano le quita
     el cursor (`robando`): el visor se queda con todo el raton y se hace la ventana activa, que es lo que
     suelta el cursor encerrado del juego (KWin solo lo deja encerrado mientras el juego es la activa). Al
     ocultarla, o con un clic fuera de la pastilla, se le devuelve a `devolverA`. `esJuego`: -1 = sin
     saber todavia. Al entrar al 3D de un juego la pastilla se ensena unos segundos (los atajos) y se va. */
#ifdef HAVE_WAYLAND_WM
  WindowMonitor wm;
#endif
  bool wmProbado = false, wmAbierto = false;
  int esJuego = -1;
  bool robando = false;
  std::string devolverA;
  double robadoMirar = -1.0;
  bool presentacionHecha = false;
  bool suavizado = true;     /* el boton de dividido lo pone suavizado (los ajustes del programa lo quitan) */
  bool suavizadoAuto = false; /* ...o lo decide solo el medidor de separacion (v0.88) */
  std::string modoElegido;   /* "conv" / "sbs" / "tb": con que tipo de 3D se guarda el «no volver a preguntar» */
  /* el boton de la pastilla y las teclas 1 y 2: 1 = la red de Samsung (mde_*.onnx), 2 = la nativa
     (Depth Anything). redPedida es un encargo pendiente que el bucle atiende con la red parada. */
  bool textoToggle = false;  /* la tecla R: rotulos apoyados en la pantalla o no */
  int redPedida = 0;
  /* (v1.12) el interruptor Neural | Rendimiento de la pastilla: 1 = pasar al modo sin red, 2 = a la red.
     Lo atiende el bucle con el hilo de la red parado, como redPedida. */
  int modoRedPedido = 0;
  std::string rutaRed[3];    /* [1] y [2]: ruta .onnx de cada red; vacia = esa red no esta disponible */
  double extraHasta = 0.0;   /* la frase extra se quita sola a los pocos segundos */
  /* La ventana de ajustes (un proceso APARTE) y `odysseyvk ctl ajuste` escriben numeros y una ruta
     .onnx en ~/.local/state/odysseyvk/ajuste; el bucle los lee de ahi. redRutaPedida es el cambio de
     modelo encargado, que se atiende con la red parada, igual que las teclas 1 y 2, y se queda
     encargado hasta que el conversor este encendido. */
  std::string redRutaPedida;
  bool avisoRedLejos = false;   /* dar cuenta una sola vez de "el conversor esta apagado" */
};

/* senal de cierre (systemctl stop, kill, o el propio serve): la sesion tiene que salir por la
   puerta de adelante para devolver el panel a su modo, apagar la lente y apagar el scaler. Si se
   moria por SIGTERM a secas, el monitor se quedaba en modo 3D y la lente encendida. */
static volatile std::sig_atomic_t gTerm = 0;
static void onTermSignal(int) { gTerm = 1; }

void printTuning(const VkUi &ui, const char *what) {
  std::printf("[%-11s] weave=%d swap=%d fs=%5.2f align=%+.4f pitch=%.5f xtdyn=%+.4f conv=%+.5f"
              " rampa %+.6f %+.6f prof=%d rel=%d\n",
              what, (int)ui.tune.weave, (int)ui.tune.swapLR, ui.tune.filterSlope, ui.tune.align, ui.tune.pitch,
              ui.tune.xtalkDyn, ui.tune.conv, ui.tune.rampaDX, ui.tune.rampaDY,
              ui.hud.state().depth, ui.hud.state().popout);
  std::fflush(stdout);
}

/* ---- el canal de ajuste vivo -------------------------------------------------------------
   El visor se queda el teclado de la pantalla del panel, asi que los numeros solo se podian mover
   con las teclas de esa ventana. La ventana de ajustes (otro proceso) y `odysseyvk ctl ajuste` los
   mueven por ficheros: ver src/ajuste.h. Aqui esta la otra mitad: la foto de lo que el visor usa de
   verdad (el fichero `estado`) y la cuenta de lo que va cambiando (`preferencias.log`). */
/* El panel de ajustes, por encima del video. Es otro proceso (la capa superpuesta de la tarjeta de
   la orilla); se suelta del todo para que viva por su cuenta, y pulsar el engranaje otra vez lo
   cierra. Lo abre el engranaje de la pastilla y, desde v0.94, el propio visor cuando se le pide el
   conversor y no hay red de profundidad que cargar. */
static void abrirPanelDeAjustes() {
  char propio[512] = {0};
  const ssize_t n = ::readlink("/proc/self/exe", propio, sizeof propio - 1);
  const std::string exe = n > 0 ? std::string(propio, (size_t)n) : std::string("odysseyvk");
  const pid_t pid = ::fork();
  if (pid == 0) {
    ::setsid();
    if (::fork() != 0) ::_exit(0);
    ::execl(exe.c_str(), exe.c_str(), "ajustes", (char *)nullptr);
    ::_exit(127);
  }
  if (pid > 0) {
    int stHijo = 0;
    ::waitpid(pid, &stHijo, 0);
  }
}
static bool g_cantosFinos = true;   /* v0.89: el mapa va tal cual y el sombreador busca la vista de cada ojo */
static double g_fpsVisto = 0.0, g_mapaMs = 0.0;   /* para el `estado`: lo ensena la pestana de rendimiento */
static std::string g_modeloEnUso;   /* el .onnx que tiene abierto el conversor, para los avisos */
static std::string g_tarjetaEnUso;    /* "NVIDIA" o "CPU": lo que reporta ONNX Runtime */
/* La casilla de Previsualizacion de la ventana de ajustes (`vista = 1`): mientras esta pedida, la copia
   del cuadro que guardamos en `camara.jpg` sale ocho veces por segundo en vez de una cada dos segundos.
   El rastreo ya esta leyendo la camara a su tasa, asi que esto no abre nada ni le arranca nada a la
   camara del monitor: solo cambia cada cuanto guardamos una foto de lo que ya teniamos en la mano. */
static bool g_vistaRapida = false;
static int g_barraGuardar = 0;   /* copias chicas que faltan por guardar (`ctl barra guardar`) */
/* Si esta sesion quiere la camara (`--track`, lo decide el hub): se apunta desde el arranque, aunque la
   apertura falle. Es la senal que mira `ctl vista` para cederla: la vista previa de la ventana de
   ajustes sabe que el visor la quiere y la suelta en medio segundo, y el visor la vuelve a abrir. */
static bool g_camaraPedida = false;

static std::string nombreCorto(const std::string &ruta) {
  const size_t b = ruta.find_last_of('/');
  return b == std::string::npos ? ruta : ruta.substr(b + 1);
}

static std::string fmtNumero(double v) {
  char b[40];
  std::snprintf(b, sizeof b, "%.6f", v);
  std::string s(b);
  while (s.size() > 1 && s.back() == '0') s.pop_back();
  if (!s.empty() && s.back() == '.') s.pop_back();
  for (char &c : s)
    if (c == '.') c = ',';
  return s;
}

/* Los mismos nombres que usan serve.conf y la ventana de ajustes: una sola palabra para cada
   numero, aqui y alli. `rampa_x` / `rampa_y` son la base que trae la configuracion, y
   `rampa_x_teclas` / `rampa_y_teclas` lo que le sumaron las teclas [ ] y ; ' encima. */
static std::map<std::string, double> ajusteActual(const VkUi &ui, const VkWeave &wv) {
  const Hud::State &st = ui.hud.state();
  const WeaveTuning &t = ui.tune;
  std::map<std::string, double> v;
  v["fase"] = t.align;
  v["paso"] = t.pitch;
  v["filtro"] = t.filterSlope;
  v["crosstalk"] = t.xtalkDyn;
  v["crosstalk_base"] = t.xtalkFac;
  v["conv"] = t.conv;
  v["rampa_x"] = wv.rampaBase(true);
  v["rampa_y"] = wv.rampaBase(false);
  v["rampa_x_teclas"] = t.rampaDX;
  v["rampa_y_teclas"] = t.rampaDY;
  v["fondo"] = t.ganFondo;
  v["frente"] = t.ganFrente;
  v["empuje_fondo"] = t.empFondo;
  v["empuje_frente"] = t.empFrente;
  v["planos"] = t.planos;
  v["muestras"] = t.muestras;
  v["temblor"] = wv.temblor();
  v["canto"] = wv.canto();
  v["fase_temblor"] = wv.faseTemblor();
  v["relieve"] = st.popout;
  v["apoyo"] = st.depth;
  return v;
}

/* Aplica UN numero pedido desde fuera. Pasa por los mismos campos que las teclas, asi que no hay
   dos versiones de la verdad, y deja linea en el registro y en preferencias.log. */
static void aplicarAjuste(VkUi &ui, VkWeave &wv, const std::string &k, double v) {
  WeaveTuning &t = ui.tune;
  Hud::State &st = ui.hud.state();
  /* en modo de legado la trama usa los numeros de fabrica y nada mas: ahi el video ya llega en dos
     vistas, asi que los unicos dos numeros que se mueven son los de la pastilla (relieve y apoyo).
     La pestaña de edicion avanzada de la ventana queda apagada mientras dura el modo. */
  if (st.legado && k != "relieve" && k != "apoyo") {
    const double ahora = ajusteActual(ui, wv)[k];
    if (std::fabs(ahora - v) > 1e-9)
      std::printf("[legado] `%s` no se mueve en modo de legado (esta en %s): para eso hay que apagar el modo\n",
                  k.c_str(), fmtNumero(ahora).c_str());
    std::fflush(stdout);
    return;
  }
  std::string antes;
  if (k == "fase") { antes = fmtNumero(t.align); t.align = (float)v; }
  else if (k == "paso") { antes = fmtNumero(t.pitch); t.pitch = (float)v; }
  else if (k == "filtro") { antes = fmtNumero(t.filterSlope); t.filterSlope = (float)v; }
  else if (k == "crosstalk") { antes = fmtNumero(t.xtalkDyn); t.xtalkDyn = (float)v; }
  else if (k == "crosstalk_base") { antes = fmtNumero(t.xtalkFac); t.xtalkFac = (float)v; }
  else if (k == "conv") { antes = fmtNumero(t.conv); t.conv = (float)v; }
  else if (k == "rampa_x") { antes = fmtNumero(wv.rampaBase(true)); wv.setRampaBase(true, (float)v); }
  else if (k == "rampa_y") { antes = fmtNumero(wv.rampaBase(false)); wv.setRampaBase(false, (float)v); }
  else if (k == "rampa_x_teclas") { antes = fmtNumero(t.rampaDX); t.rampaDX = (float)v; }
  else if (k == "rampa_y_teclas") { antes = fmtNumero(t.rampaDY); t.rampaDY = (float)v; }
  else if (k == "fondo") { antes = fmtNumero(t.ganFondo); t.ganFondo = (float)v; }
  else if (k == "frente") { antes = fmtNumero(t.ganFrente); t.ganFrente = (float)v; }
  else if (k == "empuje_fondo") { antes = fmtNumero(t.empFondo); t.empFondo = (float)v; }
  else if (k == "empuje_frente") { antes = fmtNumero(t.empFrente); t.empFrente = (float)v; }
  else if (k == "planos") { antes = fmtNumero(t.planos); t.planos = (float)v; }
  else if (k == "muestras") { antes = fmtNumero(t.muestras); t.muestras = (int)(v + 0.5); }
  else if (k == "temblor") { antes = fmtNumero(wv.temblor()); wv.setTemblor((float)v); }
  else if (k == "canto") { antes = fmtNumero(wv.canto()); wv.setCanto((float)v); }
  else if (k == "fase_temblor") { antes = fmtNumero(wv.faseTemblor()); wv.setFaseTemblor((float)v); }
  else if (k == "relieve") { antes = fmtNumero(st.popout); st.popout = (int)(v + 0.5); }
  else if (k == "apoyo") { antes = fmtNumero(st.depth); st.depth = (int)(v + 0.5); }
  else {
    std::fprintf(stderr, "[ajuste] no conocemos ese numero: %s\n", k.c_str());
    std::fflush(stderr);
    return;
  }
  const std::string ahora = fmtNumero(ajusteActual(ui, wv)[k]);
  if (ahora == antes) return;   /* el fichero arrastra los demas numeros: no los volvemos a contar */
  std::printf("[ajuste] %s %s -> %s (lo pidio la ventana de ajustes)\n", k.c_str(), antes.c_str(), ahora.c_str());
  std::fflush(stdout);
  ajuste::registrar(k, antes, ahora, nombreCorto(g_modeloEnUso), "la ventana de ajustes");
  printTuning(ui, "ajuste");
}

/* Numero que habia en la trama antes de entrar en el modo de legado, para poder volver atras. */
static std::map<std::string, double> previoLegado;
static bool previoLegadoPuesto = false;

/* El modo de legado cambia COMO se arma la imagen, no la sesion: el video, la pantalla, la lente y
   el ON/OFF se quedan donde estaban. Lo que hace es dejar la trama con los numeros de fabrica (que
   son los que le corresponden a un video que ya llega en dos vistas), cerrar la puerta al conversor
   que saca dos vistas de una, quitar el boton de la red de la pastilla y recortarla a lo justo. Al
   apagarlo vuelven los numeros que uno estaba usando. */
static void cambioLegado(VkUi &ui, VkWeave &wv, bool querido, const char *origen) {
  Hud::State &st = ui.hud.state();
  if (st.legado == querido) return;
  if (querido) {
    previoLegado = ajusteActual(ui, wv);
    previoLegadoPuesto = true;
    st.legado = true;   /* a partir de aqui `aplicarAjuste` ya no deja mover los numeros finos */
    for (const auto &kv : ajuste::valoresFabrica())
      if (kv.first != "relieve" && kv.first != "apoyo") aplicarAjuste(ui, wv, kv.first, kv.second);
    std::printf("[legado] modo de legado ACTIVADO (%s): trama con los numeros de fabrica, sin conversor"
                " de video normal y pastilla sencilla\n", origen);
  } else {
    st.legado = false;
    if (previoLegadoPuesto)
      for (const auto &kv : previoLegado) aplicarAjuste(ui, wv, kv.first, kv.second);
    previoLegadoPuesto = false;
    std::printf("[legado] modo de legado APAGADO (%s): vuelven los numeros que estabas usando\n", origen);
  }
  std::fflush(stdout);
  printTuning(ui, "legado");
}

/* Lo que el visor esta usando de verdad, escrito cada medio segundo: la ventana de ajustes lee de
   ahi para no tener que adivinar. Son claves `clave = valor`, con coma decimal como en serve.conf. */
static std::string textoEstado(const VkUi &ui, const VkWeave &wv) {
  char hora[32] = {0};
  const std::time_t qt = std::time(nullptr);
  std::tm tm{};
  localtime_r(&qt, &tm);
  std::strftime(hora, sizeof hora, "%Y-%m-%d %H:%M:%S", &tm);
  std::string s;
  s += "# estado real del visor: lo escribe el propio visor, lo lee la ventana de ajustes\n";
  s += std::string("hora = ") + hora + "\n";
  s += std::string("visor = ") + ODYSSEYVK_VERSION + "\n";
  {
    char num[64];
    std::snprintf(num, sizeof num, "%.1f", g_fpsVisto);
    s += std::string("fps = ") + num + "\n";
    std::snprintf(num, sizeof num, "%.1f", g_mapaMs);
    s += std::string("mapa_ms = ") + num + "\n";
  }
  s += std::string("trama = ") + (ui.tune.weave && ui.hud.state().weaveOn ? "activa" : "apagada") + "\n";
  /* si la pastilla esta a la vista o ocultada (Ctrl+Shift+F1 / `ctl capa`): lo lee `ctl capa estado`
     y la ventana de ajustes. Con la pastilla oculta la trama sigue igual, solo se quita la interfaz. */
  s += std::string("capa = ") + (ui.hud.overlayVisible() ? "puesta" : "oculta") + "\n";
  /* el modo de legado: trama con numeros de fabrica, sin conversor y con la pastilla recortada */
  s += std::string("legado = ") + (ui.hud.state().legado ? "si" : "no") + "\n";
#ifdef HAVE_ORT
  s += std::string("conversor = ") + (wv.converterOn() ? "puesto" : "apagado") + "\n";
  s += std::string("modelo = ") + (g_modeloEnUso.empty() ? "(ninguno)" : g_modeloEnUso) + "\n";
  s += std::string("red = ") + std::to_string(ui.hud.state().red) + "\n";
  s += std::string("tarjeta = ") + (g_tarjetaEnUso.empty() ? "-" : g_tarjetaEnUso) + "\n";
#else
  s += "conversor = no compilado\n";
#endif
  for (const auto &kv : ajusteActual(ui, wv)) s += kv.first + " = " + fmtNumero(kv.second) + "\n";
  /* el Facetracking: con que camara se esta siguiendo la cara y que esta viendo. La ventana de
     ajustes lo lee de aqui y de ahi sale tambien su pestaña de camara. */
  {
    const Hud::State &hs = ui.hud.state();
    s += std::string("facetracking = ") + (hs.rastreoVivo ? "abierto" : "sin abrir") + "\n";
    s += std::string("camara = ") + (hs.rastreoCamara.empty() ? "-" : hs.rastreoCamara) + "\n";
    if (hs.rastreoVivo)
      s += std::string("camara_tipo = ") + (hs.rastreoEstudio ? "3D Studio" : "externa") + "\n";
    s += std::string("camara_cara = ") + (hs.rastreoCara ? "si" : "no") + "\n";
    s += std::string("camara_nota = ") + (hs.rastreoNota.empty() ? "-" : hs.rastreoNota) + "\n";
    /* si esta sesion quiere la camara, aunque ahora mismo no la tenga abierta (la encontro ocupada y
       esta reintentando): `ctl vista` lo mira para cederla enseguida, y la ventana de ajustes para
       saber si la vista previa tiene que salir del medio o puede seguir con la camara suelta. */
    s += std::string("camara_pedida = ") + (g_camaraPedida ? "si" : "no") + "\n";
    /* cada cuanto guardamos la copia en `camara.jpg`: `rapida` mientras la casilla de Previsualizacion
       la pide. La ventana lo lee para confirmar que el visor acepto la orden. */
    s += std::string("camara_vista = ") + (g_vistaRapida ? "rapida" : "normal") + "\n";
  }
  return s;
}

/* frase breve bajo la pastilla: qué se está viendo y en qué estado está el hardware */
/* La linea de abajo a la izquierda (v0.78): solo lo que hay que saber. Antes era un parte
   entero ("ventana capturada 3022x1303 con trama · lente 3D · scaler 3D · conversor...") que se
   quedaba puesto encima del video. Ahora dice lo que FALLA (la lente, el scaler o la camara no
   responden) y el aviso del momento (pausa, cambio de red...), que se quita solo. Con todo en
   orden no pinta nada. */
std::string infoLine(const Hud::State &st, const std::string &extra) {
  std::string out;
  auto suma = [&](const std::string &s) {
    if (s.empty()) return;
    if (!out.empty()) out += " · ";
    out += s;
  };
  if (st.weaveOn && !(st.hud & 1)) suma(idioma::tr("la lente 3D no respondió"));
  if (st.weaveOn && !st.legado && !(st.hud & 2)) suma(idioma::tr("el scaler del monitor no respondió"));
  if (st.controlOjo && !st.rastreoVivo) suma(idioma::tr("sin cámara: la trama va con la posición de fábrica"));
  size_t i = 0;
  while (i < extra.size() && (extra[i] == ' ' || (unsigned char)extra[i] == 0xC2 || (unsigned char)extra[i] == 0xB7)) i++;
  suma(extra.substr(i));   /* las frases viejas venian con su " · " delante */
  return out;
}

/* slotRed() vive en util.h: es la misma regla para el hub y para el visor. */
const char *nombreRed(int slot) { return slot == 1 ? "la de Samsung" : "la nativa"; }

/* ocultar o mostrar la pastilla. Mandan ahi la tecla Ctrl+Shift+F1, el `odysseyvk ctl capa` (que
   llega por fichero) y el rincon pinchable del que habla Hud::huecoDeLaPastilla. Lo unico que se quita
   es la interfaz: la trama sigue tejiendose y el rastreo sigue mirando los ojos, que es lo que uno
   quiere mientras ve algo a pantalla completa. La region de entrada se rehace con `inputChanged`,
   porque sin pastilla no hay nada que picar salvo el rincon de arriba a la derecha. */
/* ---- el tipo de video, detectado en los primeros segundos (rama experimental) ----
   En un video dividido las dos mitades son casi la misma imagen, corrida unos puntos en horizontal (el
   paralaje); en uno normal no se parecen. Se miran dos cosas, sobre el cuadro chico que ya se lee para
   la red (reducido a la mitad): el parecido de la imagen y el del detalle (la imagen menos su version
   borrosa), con la mitad derecha corrida hasta 6 puntos. Con la imagen sola, un paisaje (cielo arriba y
   suelo abajo en las dos mitades) parecia dividido; con el detalle solo, una pelicula oscura se acercaba
   a un 3D de detalle suave. MEDIDO 26-nov con tres videos 3D lado a lado y tres normales de YouTube
   (ventanas de 3 s, 5 muestras por segundo): los 3D dan imagen 0,77-0,99 y detalle 0,27-0,83; los
   normales, imagen hasta 0,86 con detalle hasta 0,03 (paisaje) o detalle hasta 0,21 con imagen hasta
   0,50 (pelicula). Regla: dividido si imagen > 0,65 Y detalle > 0,12 (18 de 18 ventanas en los 3D, 0
   de 18 en los normales). Arriba y abajo, igual con las mitades de arriba y de abajo (sin ejemplos
   medidos todavia). Se decide con 3 s que tengan imagen (no negro); como mucho a los 8 s. */
struct DetectorTipo {
  bool activo = false, decidido = false;
  double desde = -1.0;
  std::vector<float> lumLR, detLR, lumTB, detTB;
  static float ncc(const std::vector<float> &a, const std::vector<float> &b, int w, int ax, int bx, int ay, int by, int ancho,
                   int alto) {
    double sa = 0, sb = 0, saa = 0, sbb = 0, sab = 0;
    const int n = ancho * alto;
    for (int y = 0; y < alto; y++)
      for (int x = 0; x < ancho; x++) {
        const double va = a[(size_t)(ay + y) * w + ax + x], vb = b[(size_t)(by + y) * w + bx + x];
        sa += va, sb += vb, saa += va * va, sbb += vb * vb, sab += va * vb;
      }
    const double ca = saa - sa * sa / n, cb = sbb - sb * sb / n, cab = sab - sa * sb / n;
    return (ca > 1e-9 && cb > 1e-9) ? (float)(cab / std::sqrt(ca * cb)) : 0.0f;
  }
  /* el mejor parecido entre dos rectangulos de `img` (ancho `w`) del mismo tamano, con el segundo corrido
     en horizontal hasta `d` */
  static float mejor(const std::vector<float> &img, int w, int ax, int ay, int bx, int by, int ancho, int alto, int d) {
    float m = -1.0f;
    for (int dx = -d; dx <= d; dx++) {
      const int a0 = dx >= 0 ? ax + dx : ax, b0 = dx >= 0 ? bx : bx - dx, an = ancho - std::abs(dx);
      m = std::max(m, ncc(img, img, w, a0, b0, ay, by, an, alto));
    }
    return m;
  }
  /* una muestra; true si ya hay decision en `tipo` (0 normal, 2 lado a lado, 4 arriba y abajo) */
  bool muestra(const uint8_t *rgba, int sz, double ahora, int &tipo) {
    if (!activo || decidido) return false;
    if (desde < 0.0) desde = ahora;
    const int n = sz / 2;
    std::vector<float> g((size_t)n * n), b((size_t)n * n), t((size_t)n * n), h((size_t)n * n);
    for (int y = 0; y < n; y++)
      for (int x = 0; x < n; x++) {
        float s = 0.0f;
        for (int k = 0; k < 4; k++) {
          const uint8_t *p = rgba + ((size_t)(2 * y + (k >> 1)) * sz + (size_t)(2 * x + (k & 1))) * 4;
          s += 0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2];
        }
        g[(size_t)y * n + x] = s / (4.0f * 255.0f);
      }
    /* el borroso: [1 4 6 4 1] / 16 en las dos direcciones, dos veces */
    auto pasa = [&](const std::vector<float> &de, std::vector<float> &a, bool horizontal) {
      static const float k[5] = {1 / 16.f, 4 / 16.f, 6 / 16.f, 4 / 16.f, 1 / 16.f};
      for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
          float s = 0.0f;
          for (int j = -2; j <= 2; j++) {
            const int xx = horizontal ? std::max(0, std::min(n - 1, x + j)) : x, yy = horizontal ? y : std::max(0, std::min(n - 1, y + j));
            s += k[j + 2] * de[(size_t)yy * n + xx];
          }
          a[(size_t)y * n + x] = s;
        }
    };
    pasa(g, t, true), pasa(t, b, false), pasa(b, t, true), pasa(t, b, false);
    for (size_t i = 0; i < h.size(); i++) h[i] = g[i] - b[i];
    /* con poca imagen (negro, un fundido) la muestra no cuenta */
    auto desv = [&](int x0, int ancho) {
      double s = 0, ss = 0;
      for (int y = 0; y < n; y++)
        for (int x = x0; x < x0 + ancho; x++) s += g[(size_t)y * n + x], ss += (double)g[(size_t)y * n + x] * g[(size_t)y * n + x];
      const double m = s / (ancho * n);
      return std::sqrt(std::max(0.0, ss / (ancho * n) - m * m));
    };
    const int mitad = n / 2;
    if (std::min(desv(0, mitad), desv(mitad, mitad)) > 0.03) {
      lumLR.push_back(mejor(g, n, 0, 0, mitad, 0, mitad, n, 6));
      detLR.push_back(mejor(h, n, 0, 0, mitad, 0, mitad, n, 6));
      lumTB.push_back(mejor(g, n, 0, 0, 0, mitad, n, mitad, 6));
      detTB.push_back(mejor(h, n, 0, 0, 0, mitad, n, mitad, 6));
    }
    const bool bastan = ahora - desde >= 3.0 && lumLR.size() >= 5;
    if (!bastan && ahora - desde < 8.0) return false;
    decidido = true;
    auto mediana = [](std::vector<float> v) {
      if (v.empty()) return 0.0f;
      std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
      return v[v.size() / 2];
    };
    const float iLR = mediana(lumLR), dLR = mediana(detLR), iTB = mediana(lumTB), dTB = mediana(detTB);
    tipo = (iLR > 0.65f && dLR > 0.12f) ? 2 : ((iTB > 0.65f && dTB > 0.12f) ? 4 : 0);
    std::printf("[tipo] %s en %.1f s (%zu muestras con imagen): lado a lado imagen %.2f detalle %.2f · arriba/abajo imagen %.2f "
                "detalle %.2f\n",
                tipo == 2 ? "video dividido lado a lado" : tipo == 4 ? "video dividido arriba y abajo" : "video normal",
                ahora - desde, lumLR.size(), iLR, dLR, iTB, dTB);
    std::fflush(stdout);
    return true;
  }
};

/* ---- el cursor de los juegos (rama experimental) ---- */
static bool monitorVentanas(VkUi &ui) {
#ifdef HAVE_WAYLAND_WM
  if (!ui.wmProbado) {
    ui.wmProbado = true;
    std::string e;
    ui.wmAbierto = ui.wm.open(&e);
    if (!ui.wmAbierto) std::printf("[raton] sin el gestor de ventanas de KWin: la pastilla no puede quitarle el cursor a un juego (%s)\n", e.c_str());
    std::fflush(stdout);
  }
  if (ui.wmAbierto) ui.wm.waitAndDispatch(0);
  return ui.wmAbierto;
#else
  (void)ui;
  return false;
#endif
}
/* la ventana capturada es un juego de Steam (o gamescope): la misma regla que el servicio */
static bool sobreJuego(VkUi &ui) {
  if (ui.esJuego >= 0) return ui.esJuego == 1;
  /* ODYSSEYVK_JUEGO=1|0 lo fija a mano: para probarlo en el compositor aparte, que no tiene el de KWin */
  if (const char *f = entornoOdyssey("ODYSSEYVK_JUEGO")) {
    ui.esJuego = f[0] == '1' ? 1 : 0;
    return ui.esJuego == 1;
  }
#ifdef HAVE_WAYLAND_WM
  if (!ui.opts || ui.opts->captureUuid.empty() || !monitorVentanas(ui)) return false;
  WindowInfo w{};
  if (!ui.wm.find(ui.opts->captureUuid, w) || w.appId.empty()) return false;   /* todavia sin datos */
  const std::string a = strLower(w.appId);
  ui.esJuego = (a.rfind("steam_app_", 0) == 0 || a == "gamescope") ? 1 : 0;
  std::printf("[raton] la ventana del 3D es %s (%s)\n", w.appId.c_str(), ui.esJuego ? "un juego" : "no es un juego");
  std::fflush(stdout);
  return ui.esJuego == 1;
#else
  return false;
#endif
}
static void robarCursor(VkUi &ui, const char *fuente) {
  if (ui.robando || !sobreJuego(ui)) return;
  ui.robando = true;
  ui.inputChanged = true;   /* la region de entrada pasa a la ventana entera */
  std::string como = "sin el gestor de ventanas de KWin: solo se queda con el raton, sin hacerse la ventana activa";
#ifdef HAVE_WAYLAND_WM
  if (monitorVentanas(ui)) {
    std::string propia, activa;
    for (const WindowInfo &w : ui.wm.windows()) {
      if (w.pid == (uint32_t)::getpid() && propia.empty()) propia = w.uuid;
      if (w.active()) activa = w.uuid;
    }
    ui.devolverA = (!activa.empty() && activa != propia) ? activa : (ui.opts ? ui.opts->captureUuid : std::string());
    std::string e;
    const bool pedido = !propia.empty() && ui.wm.setState(propia, WindowInfo::Active, true, &e);
    ui.wm.flush();
    ui.robadoMirar = glfwGetTime() + 0.6;
    como = pedido ? "el visor pide ser la ventana activa"
                  : "no se pudo pedir ser la ventana activa: " + (propia.empty() ? std::string("sin la ventana del visor") : e);
  }
#endif
  std::printf("[raton] %s: la pastilla le quita el cursor al juego (%s)\n", fuente, como.c_str());
  std::fflush(stdout);
}
static void devolverCursor(VkUi &ui, const char *fuente) {
  if (!ui.robando) return;
  ui.robando = false;
  ui.inputChanged = true;
  ui.robadoMirar = -1.0;
  std::string e = "sin el gestor de ventanas de KWin";
  bool ok = false;
#ifdef HAVE_WAYLAND_WM
  if (ui.wmAbierto && !ui.devolverA.empty()) {
    ok = ui.wm.setState(ui.devolverA, WindowInfo::Active, true, &e);
    ui.wm.flush();
  }
#endif
  std::printf("[raton] %s: el cursor vuelve al juego%s%s%s\n", fuente, ok ? "" : " (sin activarlo: ", ok ? "" : e.c_str(),
              ok ? "" : ")");
  std::fflush(stdout);
}

/* un clic con el cursor quitado al juego: fuera de la pastilla (y de su menu) se lo devuelve. La
   pastilla se va, el panel de ajustes tambien si estaba abierto, y el juego vuelve a ser la activa.
   Dice si el clic se quedo en eso (true) o hay que atenderlo como siempre. */
static void cerrarPanelDeAjustes();
static void cambioCapa(VkUi &ui, bool puesta, const char *fuente);
static bool clicFuera(VkUi &ui, Hud::Hit hit, double x, double y) {
  if (!ui.robando || hit != Hud::HitNone) return false;
  float px = 0, py = 0, pw = 0, ph = 0;
  if (ui.hud.pillBounds(&px, &py, &pw, &ph) && x >= px && x <= px + pw && y >= py && y <= py + ph) return false;
  if (!panelFijado()) cerrarPanelDeAjustes();   /* fijado encima (el pin del panel), se queda */
  cambioCapa(ui, false, "clic en el juego");
  return true;
}

static void cambioCapa(VkUi &ui, bool puesta, const char *fuente) {
  const bool antes = ui.hud.overlayVisible();
  if (puesta == antes) {
    std::printf("[capa] la pastilla ya estaba %s (%s)\n", puesta ? "a la vista" : "ocultada", fuente);
    std::fflush(stdout);
    return;
  }
  ui.hud.state().overlayOn = puesta;
  ui.inputChanged = true;
  ui.presentacionHecha = true;   /* tocada a mano (o ya ida): la presentacion no la vuelve a quitar */
  if (!puesta) ui.hud.state().menuRendimiento = ui.hud.state().menuNormal = false;
  ajuste::registrar("capa", antes ? "puesta" : "oculta", puesta ? "puesta" : "oculta",
                    nombreCorto(g_modeloEnUso), fuente);
  std::printf("[capa] la pastilla %s (%s)\n", puesta ? "a la vista" : "ocultada", fuente);
  std::fflush(stdout);
  /* con un juego debajo, la pastilla puesta a mano le quita el cursor y al irse se lo devuelve */
  if (puesta) robarCursor(ui, fuente);
  else devolverCursor(ui, fuente);
}

/* apagar o encender el 3D desde fuera: es el interruptor ON/OFF de la pastilla mandado por fichero
   (`odysseyvk ctl 3d`), lo mismo que picar la pista con el raton o pulsar Ctrl+Shift+1. Con el 3D
   apagado la ventana sigue ahí y el panel vuelve a 2D; al encenderlo vuelve la trama. */
static void cambio3D(VkUi &ui, bool puesto, const char *fuente) {
  const bool antes = ui.hud.state().weaveOn;
  if (puesto == antes) {
    std::printf("[3d] la trama ya estaba %s (%s)\n", puesto ? "activa" : "apagada", fuente);
    std::fflush(stdout);
    return;
  }
  ui.hud.toggle3D();
  ajuste::registrar("trama", antes ? "activa" : "apagada", puesto ? "activa" : "apagada",
                    nombreCorto(g_modeloEnUso), fuente);
  std::printf("[3d] trama %s (%s)\n", puesto ? "activa" : "apagada", fuente);
  std::fflush(stdout);
  printTuning(ui, fuente);
}

void applyHit(VkUi &ui, Hud::Hit hit) {
  Hud::State &st = ui.hud.state();
  WeaveTuning &t = ui.tune;
  switch (hit) {
    case Hud::Hit3D:
      ui.hud.toggle3D();
      printTuning(ui, "3D/2D");
      break;
    /* ya no hay boton de capa: la pastilla se oculta con Ctrl+Shift+F1 o con `ctl capa` */
    case Hud::HitDepth:
    case Hud::HitPopout:
      ui.dragging = hit;
      break;
    case Hud::HitModoRed:
      ui.modoRedPedido = st.rendimiento ? 2 : 1;   /* el interruptor Neural | Rendimiento pasa al otro extremo */
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
      /* cerrar sin elegir es quedarse en 2D: la tesela de «2D plano» ya no existe (v0.78) */
      st.ask = false;
      st.weaveOn = false;
      printTuning(ui, "2D plano");
      break;
    case Hud::HitPreajuste1:
    case Hud::HitPreajuste2:
    case Hud::HitPreajuste3:
      ui.preajustePedido = (int)hit - (int)Hud::HitPreajuste1;   /* lo aplica el bucle: tiene la red y la trama */
      break;
    case Hud::HitFocoAuto:
      st.menuNormal = !st.menuNormal;   /* el boton del chip abre y cierra su menu */
      break;
    case Hud::HitNormalAuto:
    case Hud::HitNormalManual:
      ui.focoPedido = hit == Hud::HitNormalAuto ? 1 : 2;   /* lo atiende el bucle: tiene la red y los valores */
      break;
    case Hud::HitRendimientoMenu:
      /* el boton del rayo: abre y cierra el menu del modo rendimiento */
      st.menuRendimiento = !st.menuRendimiento;
      std::printf("[rendimiento] menu del rayo %s\n", st.menuRendimiento ? "abierto" : "cerrado");
      std::fflush(stdout);
      break;
    case Hud::HitRendMezcla:
    case Hud::HitRendFuerza:
    case Hud::HitRendAlcance:
    case Hud::HitRendParalaje:
      /* un deslizador del menu del rayo: el clic ya pone el valor, y arrastrando lo sigue */
      ui.dragging = hit;
      ui.hud.dragTo(hit, (float)ui.mx);
      break;
    case Hud::HitModoNormal:
    case Hud::HitModoDividido:
    case Hud::HitModoSuave: {
      /* los botones de icono de la pastilla: cambiar de tipo de 3D sin salir del video. El cambio
         de verdad (encender o apagar el conversor, darle la imagen entera o solo la izquierda) lo
         hace el bucle del visor, que es quien tiene la red. */
      /* el boton de video dividido lo pone suavizado, salvo que los ajustes del programa digan otra
         cosa (v0.84). HitModoSuave ya no tiene boton: queda para la prueba por nombre. */
      const int modo = hit == Hud::HitModoNormal ? 1
                       : (hit == Hud::HitModoSuave ? 3 : (ui.suavizadoAuto ? 2 : (ui.suavizado ? 3 : 2)));
      if (st.modo3d == modo && st.weaveOn) break;   /* ya estaba en ese */
      st.ask = false;
      st.weaveOn = true;
      t.topBottom = false;
      ui.modoPedido = modo;
      if (modo == 2) {
        /* los dos deslizadores vuelven a lo de un video que ya trae sus dos imagenes: sin empuje */
        st.depth = 50;
        st.popout = 0;
      }
      /* el dividido se guarda siempre como `sbs`: que salga suavizado o no lo deciden los ajustes */
      static const char *kClave[] = {"", "conv", "sbs", "sbs"};
      static const char *kNombre[] = {"", "video normal (2D a 3D)", "video dividido (lado a lado, su propio relieve)",
                                      "video dividido suavizado (una imagen y el relieve del conversor)"};
      static const char *kAviso[] = {"", "vídeo normal", "vídeo dividido: el relieve del propio vídeo",
                                     "vídeo dividido suavizado: el relieve lo pone el conversor"};
      ui.modoElegido = kClave[modo];
      ui.extraInfo = idioma::tr(kAviso[modo]);
      ui.extraHasta = glfwGetTime() + 4.0;
      std::printf("[formato] desde la pastilla: %s\n", kNombre[modo]);
      std::fflush(stdout);
      /* si esta aplicacion estaba apuntada para no preguntar, se queda apuntada con el tipo nuevo */
      if (st.recordar && ui.opts && !ui.opts->recordarApp.empty()) {
        std::string e;
        if (ajuste::pedirRecordar(ui.opts->recordarApp, ui.modoElegido, &e))
          std::printf("[recordar] %s: %s\n", ui.opts->recordarApp.c_str(), ui.modoElegido.c_str());
        else
          std::fprintf(stderr, "[recordar] %s\n", e.c_str());
      }
      break;
    }
    case Hud::HitEngranaje: {
      /* el engranaje de la pastilla: el panel de ajustes, por encima del video */
      abrirPanelDeAjustes();
      std::printf("[pastilla] engranaje: el panel de ajustes\n");
      std::fflush(stdout);
      break;
    }
    case Hud::HitRecordar: {
      /* la casilla de «no volver a preguntar», dentro del video: el recado lo recoge el servicio */
      if (!ui.opts || ui.opts->recordarApp.empty()) break;
      const std::string &app = ui.opts->recordarApp;
      const bool quiere = !st.recordar;
      std::string e;
      if (!ajuste::pedirRecordar(app, quiere ? ui.modoElegido : std::string("no"), &e)) {
        std::fprintf(stderr, "[recordar] %s\n", e.c_str());
        ui.extraInfo = idioma::tr("no se pudo guardar la elección");
      } else {
        st.recordar = quiere;
        ui.extraInfo = idioma::trf(quiere ? "{} pasa directo al 3D, sin preguntar" : "la próxima vez se vuelve a preguntar por {}", {app});
        std::printf("[recordar] %s: %s\n", app.c_str(), quiere ? ui.modoElegido.c_str() : "se vuelve a preguntar");
        std::fflush(stdout);
      }
      ui.extraHasta = glfwGetTime() + 6.0;
      break;
    }
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

/* Los atajos del hub (de fabrica Ctrl+Mayus+1 3D · 3/4 plano de apoyo · 5/6 relieve · 7/8 tamano de
   la pastilla · F1, y 2 por costumbre, ocultar o mostrar la pastilla; desde v1.09 se cambian en el
   panel). Llegan por dos caminos: la tecla, cuando el visor tiene el teclado, y el atajo global del
   escritorio (v0.75, atajos.h), cuando lo tiene el video o el juego. `accion` es su nombre en
   kglobalaccel ("3d", "prof+", ...). */
static void atajoHub(VkUi &ui, const std::string &accion, const char *origen) {
  const char *tag = nullptr;
  if (accion == "3d") { ui.hud.toggle3D(); tag = "3D/2D"; }
  else if (accion == "capa" || accion == "capa2") cambioCapa(ui, !ui.hud.overlayVisible(), origen);
  /* en el modo rendimiento no hay Profundidad ni Salida: la separacion la pone el menu del rayo */
  else if (ui.hud.state().rendimiento && (accion == "prof-" || accion == "prof+" || accion == "relieve-" || accion == "relieve+")) {
    std::printf("[rendimiento] %s no hace nada en el modo rendimiento (%s): la separacion la pone el menu del rayo\n",
                accion.c_str(), origen);
    std::fflush(stdout);
    return;
  }
  /* en Auto la Profundidad y la Salida no estan: las calcula el visor (plano de la saliencia y salida) */
  else if (ui.hud.state().focoVisible && ui.hud.state().focoAuto &&
           (accion == "prof-" || accion == "prof+" || accion == "relieve-" || accion == "relieve+")) {
    std::printf("[foco] %s no hace nada en Auto (%s): el plano y la salida se calculan solos\n", accion.c_str(), origen);
    std::fflush(stdout);
    return;
  }
  else if (accion == "prof-") { ui.hud.nudgeDepth(-5); tag = "profundidad -"; }
  else if (accion == "prof+") { ui.hud.nudgeDepth(+5); tag = "profundidad +"; }
  else if (accion == "relieve-") { ui.hud.nudgePopout(-5); tag = "relieve -"; }
  else if (accion == "relieve+") { ui.hud.nudgePopout(+5); tag = "relieve +"; }
  /* tamano de la pastilla. El panel cambia de escala al pasar al modo 3D (se queda en 1) y a cada
     quien le viene bien un tamano distinto, asi que se puede mover en marcha. */
  else if (accion == "pastilla-" || accion == "pastilla+") {
    const float sc = ui.hud.scale() + (accion == "pastilla+" ? 0.25f : -0.25f);
    const float fija = sc < 0.5f ? 0.5f : (sc > 3.0f ? 3.0f : sc);
    ui.hud.setScale(fija);
    std::printf("[pastilla] tamano x%.2f (%s)\n", (double)fija, origen);
  } else {
    return;
  }
  ui.inputChanged = true;
  if (tag) printTuning(ui, tag);
  std::fflush(stdout);
}

/* lee los atajos de serve.conf (v1.09): el mapa de la tecla local y el renglon de la pastilla */
static void cargaAtajos(VkUi &ui) {
  ui.atajosPuestos.clear();
  for (const auto &par : atajos::combos()) ui.atajosPuestos[par.second] = par.first;
  if (!ui.atajosPuestos.count("Ctrl+Shift+2")) ui.atajosPuestos["Ctrl+Shift+2"] = "capa2";
  ui.hud.state().teclas = atajos::renglonPastilla(ui.hud.state().legado);
  ui.hud.state().teclasRendimiento = atajos::renglonPastilla(ui.hud.state().legado, true);
}

/* la tecla de GLFW escrita como los atajos ("Ctrl+Shift+K"); vacia si no es de las que valen */
static std::string comboDeGlfw(int key, int mods) {
  std::string t;
  if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) t = std::string(1, (char)('A' + key - GLFW_KEY_A));
  else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) t = std::string(1, (char)('0' + key - GLFW_KEY_0));
  else if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12) t = "F" + std::to_string(key - GLFW_KEY_F1 + 1);
  else if (key == GLFW_KEY_LEFT) t = "Left";
  else if (key == GLFW_KEY_RIGHT) t = "Right";
  else if (key == GLFW_KEY_UP) t = "Up";
  else if (key == GLFW_KEY_DOWN) t = "Down";
  else if (key == GLFW_KEY_PAGE_UP) t = "PgUp";
  else if (key == GLFW_KEY_PAGE_DOWN) t = "PgDown";
  else if (key == GLFW_KEY_HOME) t = "Home";
  else if (key == GLFW_KEY_END) t = "End";
  else if (key == GLFW_KEY_INSERT) t = "Insert";
  else if (key == GLFW_KEY_DELETE) t = "Delete";
  else if (key == GLFW_KEY_SPACE) t = "Space";
  else if (key >= 0x21 && key <= 0x7e) t = std::string(1, (char)key);   /* los signos: su posicion de EE. UU. */
  else return "";
  std::string c;
  if (mods & GLFW_MOD_CONTROL) c += "Ctrl+";
  if (mods & GLFW_MOD_ALT) c += "Alt+";
  if (mods & GLFW_MOD_SHIFT) c += "Shift+";
  if (mods & GLFW_MOD_SUPER) c += "Meta+";
  return c + t;
}

/* Cierra el panel de ajustes (`odysseyvk ajustes`) si esta abierto: su proceso deja su numero en un
   fichero. Lo llama Esc y la salida del visor, para que el panel no se quede solo en pantalla. */
static void cerrarPanelDeAjustes() {
  FILE *f = std::fopen(ajuste::rutaPanel().c_str(), "re");
  if (!f) return;
  long pid = 0;
  if (std::fscanf(f, "%ld", &pid) != 1) pid = 0;
  std::fclose(f);
  if (pid <= 1) return;
  char comm[64] = {0};
  if (FILE *c = std::fopen(("/proc/" + std::to_string(pid) + "/comm").c_str(), "re")) {
    if (!std::fgets(comm, sizeof comm, c)) comm[0] = 0;
    std::fclose(c);
  }
  if (std::strncmp(comm, "odysseyvk", 9) == 0) ::kill((pid_t)pid, SIGTERM);
}

static void sacarDePantallaCompleta(const std::string &uuid) {
#ifdef HAVE_WAYLAND_WM
  WindowMonitor wm;
  std::string err;
  if (!wm.open(&err)) return;
  wm.settle();
  if (wm.setState(uuid, WindowInfo::Fullscreen, false, &err)) {
    wm.flush();
    wm.waitAndDispatch(60);
    std::printf("[tecla] ESC: el video sale tambien de pantalla completa\n");
  } else {
    std::printf("[tecla] ESC: no se pudo sacar al video de pantalla completa: %s\n", err.c_str());
  }
  std::fflush(stdout);
#else
  (void)uuid;
#endif
}

void onKeyVK(GLFWwindow *win, int key, int, int action, int mods) {
  if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
  VkUi *ui = (VkUi *)glfwGetWindowUserPointer(win);
  if (!ui) return;
  ui->inputChanged = true;
  if (key == GLFW_KEY_ESCAPE) {
    cerrarPanelDeAjustes();   /* Esc sale de todo: tambien del panel de ajustes si esta abierto */
    /* ESC saca tambien al video de pantalla completa (v0.77, lo pidio Rodolfo): antes, con el
       teclado en el visor, solo cerraba el 3D y el video seguia a pantalla completa. */
    if (ui->opts && !ui->opts->captureUuid.empty()) sacarDePantallaCompleta(ui->opts->captureUuid);
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
  /* atajos del hub oficial: Ctrl+Shift+1 3D · 3/4 profundidad · 5/6 relieve y 7/8, que son
     nuestros: achicar o agrandar la pastilla. Ocultar o mostrar la pastilla es Ctrl+Shift+F1;
     Ctrl+Shift+2 era la tecla del boton que ya no esta y sigue valiendo por costumbre.
     Ninguna de las dos choca con el escritorio: lo que tiene KWin asignado es Ctrl+F1 (cambiar de
     escritorio) y Meta+F1, no Ctrl+Shift+F1. F1 con Ctrl+Shift tampoco la pide el navegador. */
  if (mods & (GLFW_MOD_CONTROL | GLFW_MOD_ALT | GLFW_MOD_SUPER)) {
    const auto it = ui->atajosPuestos.find(comboDeGlfw(key, mods));
    if (it != ui->atajosPuestos.end()) {
      atajoHub(*ui, it->second, "tecla");
      return;
    }
  }
  if ((mods & GLFW_MOD_CONTROL) && (mods & GLFW_MOD_SHIFT)) return;   /* Ctrl+Shift sin atajo: nada */
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
    ui->estadoVideo = pausado ? " · video en pausa" : " · video en marcha";
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
    if (ui->hud.state().legado) {
      std::printf("[legado] en modo de legado no se sacan dos vistas de una: no hay conversor que"
                  " encender ni apagar\n");
      std::fflush(stdout);
      return;
    }
    ui->convToggle = true;   /* el bucle del visor enciende o apaga el conversor */
    return;
  }
  /* R: los rotulos y subtitulos se apoyan (o no) en el plano de la pantalla. Es para comparar en
     el panel sin recompilar: con el apoyo puesto el texto queda quieto al mover la cabeza. */
  if (key == GLFW_KEY_R) {
    ui->textoToggle = true;
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
     marcha: el bucle cambia la red cuando el hilo de la estimacion esta libre.
     En modo de legado no hay red que elegir: ahi el video ya llega con sus dos vistas. */
  if (key == GLFW_KEY_1 || key == GLFW_KEY_2) {
    if (ui->hud.state().legado) {
      std::printf("[legado] en modo de legado no hay red de profundidad que cambiar\n");
      std::fflush(stdout);
      return;
    }
    ui->redPedida = key == GLFW_KEY_1 ? 1 : 2;
    return;
  }
  /* La tecla T se quito (v0.54): el rastreo no se ofrece a apagar. Si la camara
     esta abierta, manda lo que mide; si no la encuentra, la trama se queda en la
     pose de fabrica y la pastilla lo dice. */
  const char *tag = nullptr;
  if (ui->hud.state().legado) {
    /* las teclas finas de la trama (fase, paso, crosstalk, conv, muestras...) no van en el modo de
       legado: alli la trama trabaja con los numeros de fabrica. Se avisa una sola vez por sesion. */
    static bool avisado = false;
    if (tuningKey(ui->tune, key, &tag, mods) && tag && !avisado) {
      avisado = true;
      std::printf("[legado] la tecla de `%s` no se usa en modo de legado: ahi la trama lleva los numeros"
                  " de fabrica (se apaga el modo en la ventana de ajustes, pestana General)\n", tag);
      std::fflush(stdout);
    }
    return;
  }
  if (tuningKey(ui->tune, key, &tag, mods)) {
    if (key == GLFW_KEY_0 || key == GLFW_KEY_9) ui->convBase = ui->tune.conv; /* la tecla gana sobre el deslizador */
    if (tag) printTuning(*ui, tag);
  }
}

/* Cuanto separar la vista izquierda de la derecha segun el deslizador "3D Salida".
   El recorrido nuevo es el presupuesto del programa de Samsung, medido de su config:
   +12,67 px por ojo hacia afuera y 9,98 px hacia adentro (fraccion del ancho), o sea
   ~22,6 px de separacion entre ojos con el deslizador a 50 y a 3840 px de ancho. El
   recorrido viejo llegaba a ~65 px, 2,9 veces mas; ODYSSEYVK_RELIEVE=2.9 lo devuelve. */
bool topesPorOjoActivos() {
  const char *e = entornoOdyssey("ODYSSEYVK_TOPE");
  return !e || e[0] != '0';
}

float gananciaRelieve(int popout) {
  static const float escala = [] {
    const char *e = entornoOdyssey("ODYSSEYVK_RELIEVE");
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
                                  : ui->robando ? "la pastilla le quito el cursor al juego: el raton entero es del visor"
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
  /* con el cursor quitado al juego, un clic fuera de la pastilla (y de su menu) se lo devuelve: la
     pastilla se va, el panel de ajustes tambien si estaba abierto, y el juego vuelve a ser la activa */
  if (clicFuera(*ui, hit, ui->mx, ui->my)) return;
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
  /* El mapa A es una FASE (255 = una vuelta entera de la lente, el shader hace fract), y en la
     fabrica los valores apenas por debajo de 0 se dieron la vuelta: la unidad de Rodolfo trae 311
     puntos en 254-255 pegados a 0-1 en el borde izquierdo (x 0..135, y 1552..1903 del panel). Con
     lectura nearest eso daria igual (255 = 0 tras fract), pero el sampler interpola: en el borde
     entre 0 y 255 sale ~0,5 vueltas, media fase al reves, y se ve una mancha de bordes raros con el
     patron de un ojo (v0.59). Fuera de ahi el mapa no pasa de ~60, asi que todo lo que este en la
     mitad de arriba (>= 192) es un negativo dado la vuelta y se lee como 0: el error es de 2/255
     de vuelta como mucho. El mapa B no se toca: ese esta centrado en 127 y no es una fase. */
  size_t vueltas = 0;
  for (uint8_t &v : a)
    if (v >= 192) { v = 0; vueltas++; }
  if (vueltas)
    std::printf("[correccion] mapa A: %zu puntos dados la vuelta (>= 192) leidos como 0\n", vueltas);
  return true;
#else
  (void)opts; (void)a; (void)aw; (void)ah; (void)b; (void)bw; (void)bh;
  return false;
#endif
}

/* Leer un mapa de profundidad guardado (P5, el que escribe el propio visor con --dump). Solo lo usa
   el taller, para medir el relieve con un mapa que ya conocemos en vez de esperar a la red. */
bool leerPgm(const std::string &ruta, int &w, int &h, std::vector<uint8_t> &gr, std::string *err) {
  FILE *f = std::fopen(ruta.c_str(), "rb");
  if (!f) {
    if (err) *err = "no se puede abrir " + ruta;
    return false;
  }
  char magic[3] = {0, 0, 0};
  if (std::fread(magic, 1, 2, f) != 2 || magic[0] != 'P' || magic[1] != '5') {
    std::fclose(f);
    if (err) *err = ruta + " no es un PGM binario (P5)";
    return false;
  }
  int val[3] = {0, 0, 0};   /* ancho, alto, nivel maximo */
  int n = 0;
  while (n < 3) {
    int c = std::fgetc(f);
    if (c == EOF) break;
    if (c == '#') { while (c != '\n' && c != EOF) c = std::fgetc(f); continue; }
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      if (val[n] != 0 || n > 0) n++;      /* un numero ya estaba abierto: se da por cerrado */
      continue;
    }
    if (c < '0' || c > '9') continue;
    val[n] = val[n] * 10 + (c - '0');
  }
  const int maxv = n >= 3 ? val[2] : 255;
  w = n >= 1 ? val[0] : 0;
  h = n >= 2 ? val[1] : 0;
  if (w <= 0 || h <= 0 || (maxv != 255 && maxv != 65535)) {
    std::fclose(f);
    if (err) *err = "cabecera rara en " + ruta;
    return false;
  }
  const size_t n_pix = (size_t)w * (size_t)h;
  gr.assign(n_pix, 0);
  size_t leidos = 0;
  if (maxv == 255) {
    leidos = std::fread(gr.data(), 1, n_pix, f);
  } else {
    /* 16 bits: nos sobra con el byte alto, que es el que usa el visor (mapa R8) */
    std::vector<uint8_t> crudos(n_pix * 2);
    leidos = std::fread(crudos.data(), 1, crudos.size(), f) / 2;
    for (size_t i = 0; i < leidos; i++) gr[i] = crudos[i * 2];
  }
  std::fclose(f);
  if (leidos != n_pix) {
    if (err) *err = "leidos " + std::to_string(leidos) + " puntos de " + std::to_string(n_pix) + " en " + ruta;
    return false;
  }
  return true;
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

static bool pareceReproductor(const std::string &estado) {
  static const char *nombres[] = {"mpv", "vlc", "haruna", "celluloid", "totem", "smplayer", "kodi", "clapper",
                                  "dragon", "elisa", "mplayer", "stremio", "jellyfin", "plex"};
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
  marcaArranque("hasta el visor");
  std::string sock = ensureWaylandEnv();
  if (sock.empty())
    std::fprintf(stderr, "[aviso] no se detecta sesion Wayland (WAYLAND_DISPLAY vacio): glfw puede fallar\n");
  else
    std::printf("[sesion] Wayland %s\n", sock.c_str());

  if (kind == SourceKind::Mpv) {
    std::fprintf(stderr,
                 "[error] el reproductor mpv sigue por OpenGL. Reproducemelo con Vulkan asi:\n"
                 "           mpv --fullscreen <video>   y deja que el servicio te pregunte\n"
                 "         (o usa el visor de siempre: odysseyvk weave ... --gl)\n");
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
#ifdef HAVE_CAPTURE
  /* (v1.12) La captura de la ventana se pide ya, recien puesto el panel, y un hilo la bombea mientras se
     hace lo demas (la ventana, Vulkan, la red, la camara). Antes se pedia casi al final y el visor se
     quedaba ~0,8 s esperando el primer cuadro del compositor (MEDIDO en su panel: 770-806 ms, el paso
     mas largo de los ~2,2 s del arranque). El hilo se para y se junta antes del bucle, que la bombea
     desde ahi; si el visor sale antes por un error, se para solo. */
  WindowCapture cap;
  bool capPedida = false;
  std::string capErr;
  struct BombaCaptura {
    std::atomic<bool> parar{false};
    std::thread hilo;
    void arranca(WindowCapture &c) {
      hilo = std::thread([this, &c] {
        while (!parar.load()) {
          c.pump();
          pollfd p{c.fd(), POLLIN, 0};
          if (p.fd >= 0) ::poll(&p, 1, 5);
          else std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
      });
    }
    void para() {
      parar = true;
      if (hilo.joinable()) hilo.join();
    }
    ~BombaCaptura() { para(); }
  } bomba;
  if (kind == SourceKind::Capture) {
    capPedida = cap.start(opts.captureUuid, &capErr);
    if (capPedida) {
      marcaArranque("pedir la captura");
      bomba.arranca(cap);
    }
  }
#endif

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

  /* el app_id, dicho por nosotros (v1.11): sin esto KWin lo saca de /proc/<pid>/exe, y con
     `setcap` en el binario el proceso no se deja leer (no volcable). MEDIDO 26-nov: el servicio no
     encontraba la ventana del visor, no bajaba el juego y el juego quedaba encima del 3D. */
  glfwWindowHintString(GLFW_WAYLAND_APP_ID, "odysseyvk");
  GLFWwindow *win = glfwCreateWindow(W, H, NOMBRE_APP, (opts.windowed || opts.borderless) ? nullptr : mon, nullptr);
  marcaArranque("ventana");
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
  /* lo que importa es que el LIENZO mida lo que el modo del panel; la escala del escritorio da igual
     (v0.76): con 1,5 la ventana son 2560x1440 logicos y el lienzo los 3840x2160 puntos del panel */
  if (fbW != W || fbH != H)
    std::fprintf(stderr,
                 "[aviso] el lienzo de la ventana es %dx%d (escala de contenido %.2f) y el modo %dx%d: la trama "
                 "necesita pixel a pixel.\n",
                 fbW, fbH, sx, W, H);

  if (opts.windowed && !(opts.w > 0) && fbW > W && sx > 1.01f) {
    /* En el escritorio el panel esta a escala de contenido 1,5 mientras no lo tomemos, y la ventana
       del tamano del modo sale 1,5 veces MAS GRANDE que la pantalla: solo se ve una esquina y de la
       escalera de disparidad del patron desaparecen los cuadros de la derecha (eso es lo que se veia
       como «se ven solo los ultimos cuadros»). Pedimos el tamano del modo en puntos LOGICOS, que al
       multiplicarse por la escala vuelven a dar pixel a pixel. Solo en la ventana de taller: en la
       sesion de verdad el panel queda a escala 1 y aqui no se entra. */
    const int lw = (int)std::lround((float)W / sx), lh = (int)std::lround((float)H / sy);
    glfwSetWindowSize(win, lw, lh);
    glfwPollEvents();
    glfwGetFramebufferSize(win, &fbW, &fbH);
    std::printf("[ventana] ventana de taller ajustada a %dx%d logicos: el lienzo ahora %dx%d (escala %.2f)\n",
                lw, lh, fbW, fbH, sx);
  }

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
  ui.tune.filterSlope = (float)opts.filtro;
  ui.tune.planos = (float)opts.planos;   /* dos planos: 0 = el relieve continuo de siempre */
  ui.tune.muestras = opts.muestras;      /* muestras intermedias entre las dos laminas */
  if (opts.apoyo >= 0.0) {
    /* el apoyo escrito en la linea de mando (--apoyo 0..1) se pone en la palanca del panel: ahi el
       propio bucle lo ve cambiado, avisa una vez y lo usa como plano de referencia */
    ui.hud.state().depth = (int)std::lround(opts.apoyo * 100.0);
    std::printf("[taller] plano de apoyo forzado a %.2f (la palanca 3D Depth queda en %d)\n", opts.apoyo,
                (int)std::lround(opts.apoyo * 100.0));
  }
  if (opts.relieve >= 0) {
    /* "3D Salida" escrito en la linea de mando: sin relieve el mapa de profundidad no mueve nada, y
       para medir con el mapa fijo del taller hace falta poder ponerlo a mano (lo mismo que 5/6) */
    ui.hud.state().popout = opts.relieve;
    std::printf("[taller] relieve forzado a %d (3D Salida; 50 es lo que trae el programa de Samsung)\n",
                opts.relieve);
  }
  if (!opts.mapaFijo.empty() && opts.apoyo < 0.0)
    std::printf("[taller] con --mapa-fijo deberias escribir --apoyo 0..1: sin red nadie sabe donde va "
                "el plano de apoyo y se pone a la mitad del rango (0,50)\n");
  /* La compensacion pareja de la diafonia (`crosstalk_base` en serve.conf, v0.83). De fabrica es
     0,0128535: se resta el 1,3 % de la imagen del otro ojo. Si la lente deja pasar mas que eso, lo que
     sobra se ve como un fantasma separado lo que el video separe ahi sus dos imagenes. */
  if (const char *e = entornoOdyssey("ODYSSEYVK_CROSSTALK_BASE")) {
    std::string num = e;
    for (char &c : num)
      if (c == ',') c = '.';
    const double v = std::atof(num.c_str());
    if (v >= 0.0 && v <= 0.25) {
      ui.tune.xtalkFac = (float)v;
      std::printf("[trama] compensacion pareja de la diafonia: %.4f (de fabrica 0,0129)\n", v);
    }
  }
  ui.hud.state().weaveOn = ui.tune.weave;
  /* La tarjeta de "que formato tiene el video" solo sirve si nadie lo eligio antes: si el hub
     o la linea de ordenes ya lo dijeron (--sbs, --tb, --2d), no la volvemos a pintar encima
     del video. */
  ui.hud.state().ask = (kind == SourceKind::Capture) && !opts.noHud && !opts.fmtGiven;
  ui.hud.state().overlayOn = !opts.noHud;
  /* la casilla de «no volver a preguntar» sale solo si el servicio dijo de que aplicacion es el video */
  ui.hud.state().recordarVisible = !opts.recordarApp.empty() && !opts.recordarModo.empty();
  ui.modoElegido = !opts.recordarModo.empty() ? opts.recordarModo
                   : (opts.suave ? "suave" : (opts.conver ? "conv" : (opts.topBottom ? "tb" : "sbs")));
  ui.hud.state().recordar = opts.recordado;
  ui.suavizado = opts.suavizado;
  ui.suavizadoAuto = opts.suavizadoAuto;
  ui.hud.state().engranaje = !opts.noHud;
  ui.hud.state().suavizado = opts.suavizado;
  ui.hud.state().ratonLibre = !opts.noPassthrough;
  /* tamano de la pastilla pedido a mano (--ui-escala); en marcha se ajusta con Ctrl+Shift+7 / 8 */
  if (opts.uiScale > 0.25f) {
    ui.hud.setScale(opts.uiScale);
    std::printf("[pastilla] tamano fijo x%.2f (Ctrl+Shift+7 mas pequena, Ctrl+Shift+8 mas grande)\n",
                opts.uiScale);
  }
  /* el modo de legado puede venir del arranque (`--legado`, lo pasa el hub con `legado = si` en
     serve.conf) o mandarse en marcha con `ctl legado`. En ambos casos la pastilla se encoge. */
  ui.hud.state().legado = opts.legado;
  if (opts.legado) {
    std::printf("[legado] modo de legado desde el arranque: la trama lleva los numeros de fabrica, no"
                " hay conversor de video normal y la pastilla sale sencilla\n");
    std::fflush(stdout);
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
  std::printf("[vulkan] %s · %s · cola de prioridad %s%s%s\n", dev.deviceName().c_str(),
              dev.hasDmaBufImport() ? "dma-buf disponible" : "sin dma-buf", dev.prioridad().c_str(),
              dev.prioridadNota().empty() ? "" : " · ", dev.prioridadNota().c_str());

  VkWeave wv;
  if (!wv.init(dev, surface, fbW, fbH, &err)) {
    std::fprintf(stderr, "[error] trazador Vulkan: %s\n", err.c_str());
    vkDestroySurfaceKHR(dev.instance(), surface, nullptr);
    glfwDestroyWindow(win);
    dev.destroy();
    rig.shutDown();
    glfwTerminate();
    return 1;
  }  marcaArranque("vulkan");

  wv.setPanelPixels(fbW, fbH);
  wv.setField(LensField{}, EyePose{opts.eyeX, opts.eyeY, opts.eyeZ});

  /* conversor de video normal a 3D: la red de profundidad corre en este mismo hilo, pero sólo
     cada varios cuadros. Medido: unos 7 milisegundos con la tarjeta NVIDIA, y el presupuesto por
     cuadro en el modo 3D es de 8, así que hacerla en cada cuadro hundiría la tasa de imagen. */
#ifdef HAVE_ORT
  DepthNet red;
  /* el modo rendimiento (rama experimental): cuanto se exageran, sin red, la ganancia del relieve, los
     topes por ojo y el paralaje de la cabeza. De fabrica el preajuste «medio» (rendimiento.h); los
     preajustes de la pastilla los cambian en marcha. */
  auto numEntorno = [](const char *nombre, float defecto, float lo, float hi) {
    const char *e = entornoOdyssey(nombre);
    std::string t = e ? e : "";
    for (char &c : t)
      if (c == ',') c = '.';
    const float v = t.empty() ? defecto : (float)std::atof(t.c_str());
    return (v >= lo && v <= hi) ? v : defecto;
  };
  const rendimiento::Preajuste &pf = rendimiento::kPreajustes[rendimiento::kDeFabrica];
  float exGanancia = numEntorno("ODYSSEYVK_SIN_RED_RELIEVE", pf.ganancia, 0.5f, 2.0f);
  float exTopes = numEntorno("ODYSSEYVK_SIN_RED_TOPES", pf.topes, 1.0f, 4.0f);
  /* con la red (rama experimental): los topes por ojo multiplicados (`exp_topes_red`). MEDIDO 26-nov
     con los mapas de la red: con Profundidad 100 y `relieve = 2,00`, a x1 el 85 % del cuadro se pega
     al tope de dentro y entre los ojos solo hay ~11 px de relieve; a x2,75 el 58 % y ~47 px; a x4 el
     32 % y ~71 px. Con el plano automatico, x2,75 deja un 2 % pegado y ~81 px. */
  float exTopesRed = numEntorno("ODYSSEYVK_TOPES_RED", 1.0f, 1.0f, 4.0f);
  /* solo el taller lo fija (ver el alcance, abajo); sin la variable lo calcula el visor */
  static const bool kTopesRedFijos = entornoOdyssey("ODYSSEYVK_TOPES_RED") != nullptr;
  double exParalaje = numEntorno("ODYSSEYVK_SIN_RED_PARALAJE", pf.paralaje, 1.0f, 4.0f);
  std::vector<uint8_t> chico, mapa;
  bool convActivo = opts.conver;
  /* video dividido suavizado desde el arranque: el conversor ve solo la imagen izquierda */
  wv.setMediaFuente(opts.suave && opts.conver);
  /* cantos finos (v0.89), de fabrica. `cantos = viejo` en serve.conf (ODYSSEYVK_CANTOS=viejo) vuelve
     al reparto del mapa por ojo de antes, para comparar en el panel. */
  if (const char *e = entornoOdyssey("ODYSSEYVK_CANTOS")) g_cantosFinos = !(e[0] == 'v' || e[0] == 'V' || e[0] == '0');
  wv.setCantosFinos(g_cantosFinos);
  /* Con los cantos finos el mapa no se ensancha: el dilatado de 3 puntos (22 pixeles) estaba para que
     el objeto no dejara atras su orilla, y a cambio se llevaba pegada una franja de fondo. Ahora el
     canto lo pone el sombreador en su sitio, pixel a pixel. ODYSSEYVK_DILATAR lo fuerza (taller). */
  if (g_cantosFinos && !entornoOdyssey("ODYSSEYVK_DILATAR")) red.setDilatar(0);
  std::printf("[conversor] cantos: %s\n",
              g_cantosFinos ? "finos (el canto del mapa se afina con el color de la imagen y cada ojo busca su vista)"
                            : "los de antes (el mapa ensanchado y repartido por ojo; `cantos = viejo`)");
  /* rotulos y subtitulos apoyados en el plano de la pantalla (ODYSSEYVK_TEXTO=0 para apagarlo) */
  bool textoAncla = opts.textoPanel != 0;
  if (const char *e = entornoOdyssey("ODYSSEYVK_TEXTO"))
    textoAncla = !(e[0] == '0' || e[0] == 'n' || e[0] == 'N');
  /* Capas por profundidad: `fondo` y `frente` de serve.conf llegan como entorno y son el punto de
     arranque de las teclas `j`/`k` (lo de atras) y `u`/`i` (lo de delante), para que el primer
     golpe de tecla continue desde lo ajustado y no desde 1,0. */
  const auto ganArranque = [](const char *nombre, float &destino) {
    const char *e = std::getenv(nombre);
    if (!e) return;
    const float v = (float)std::atof(e);
    if (v >= 0.2f && v <= 2.0f) {
      destino = v;
      std::printf("[conversor] capa %s = %.2f\n", strstr(nombre, "FONDO") ? "fondo" : "frente", (double)v);
    }
  };
  ganArranque("ODYSSEYVK_GAN_FONDO", ui.tune.ganFondo);
  ganArranque("ODYSSEYVK_GAN_FRENTE", ui.tune.ganFrente);
  /* Empuje por capa (`empuje_fondo` / `empuje_frente` en serve.conf), en puntos de panel: mismo
     arreglo, punto de arranque de las teclas `h`/`l` (lo de atras) y `y`/`o` (lo de delante). */
  const auto empArranque = [](const char *nombre, float &destino) {
    const char *e = std::getenv(nombre);
    if (!e) return;
    const float v = (float)std::atof(e);
    if (v >= -15.0f && v <= 15.0f) {
      destino = v;
      std::printf("[conversor] empuje %s = %+.1f puntos\n",
                  strstr(nombre, "FONDO") ? "fondo" : "frente", (double)v);
    }
  };
  empArranque("ODYSSEYVK_EMP_FONDO", ui.tune.empFondo);
  empArranque("ODYSSEYVK_EMP_FRENTE", ui.tune.empFrente);
  int convSalto = 0;
  long long convHechas = 0, convFallos = 0;
  double convMs = 0.0, ultimaEstim = 0.0;
  /* medicion del bucle (ODYSSEYVK_MEDIR): tiempos de leer la copia pequena y de subir el mapa, y
     cuadros que pierden un refresco */
  double medLeeMs = 0.0, medSubeMs = 0.0, medUltimo = 0.0;
  long long medLeeN = 0, medSubeN = 0, medLentos = 0;
  /* Los tirones (v1.11), cada 5 s en la linea [fps]: si el que se atasca es el
     juego (huecos entre sus cuadros) o el visor (vueltas cuyo trabajo no cabe en un refresco,
     presentaciones que esperan a la tarjeta, leer la copia chica o subir el mapa). */
  double medLeeMax = 0.0, medSubeMax = 0.0;
  double tirFuenteUlt = 0.0, tirFuenteMax = 0.0, tirTrabajoMax = 0.0, tirPresentaMax = 0.0, tirPresentaUlt = 0.0,
         tirPresentaHueco = 0.0, tirTrabajo0 = 0.0, tirRedMs = 0.0;
  long long tirFuenteN = 0, tirFuenteHuecos = 0, tirTrabajoLargos = 0, tirPresentaLargas = 0, tirRedN = 0;
  int cadaConv = opts.convEach;
  bool apoyoAuto = true;      /* el plano de apoyo lo sigue la red; Ctrl+Shift+3/4 lo vuelve manual */
  int profundidadVista = ui.hud.state().depth;
   /* La red puede tardar cientos de ms si le toca la CPU (cuando las dos tarjetas estan llenas).
      Para no congelar el cuadro, la estimacion corre en su propio hilo: aqui solo se reduce la
      imagen en la tarjeta y se le manda la copia; el mapa vuelve cuando esta terminado y se sube
      en este hilo, que es el dueno de Vulkan. */
   /* El hilo de la red. Desde v0.69 es UN hilo que vive toda la sesion y espera trabajo: antes se creaba
      uno por estimacion, y con el su cuadrilla de OpenMP (el filtro guiado y el relleno de huecos van con
      `omp parallel`), que se montaba y se tiraba veinte veces por segundo. */
   struct HiloRed {
     std::thread hilo;
     std::mutex candado;
     std::condition_variable aviso, libre;
     struct Trabajo {
       DepthNet *red = nullptr;
       std::vector<uint8_t> copia;
       int sz = 0;
       bool apoyar = false;
       float ganancia = 0.0f, apoyoFijo = -1.0f;
     } trabajo;
     bool hayTrabajo = false, enCurso = false, salir = false;
     std::vector<uint8_t> mapa;   /* mapa terminado y esperando subirse */
     std::vector<uint8_t> mapaRG; /* el mismo, ya repartido por ojo y con los huecos rellenos (v0.69) */
     std::vector<uint8_t> mascara; /* rejilla de rotulos detectados, para poder verla */
     MemoriaTexto memoriaTexto;    /* el apoyo de los rotulos de un mapa al siguiente (v0.74); solo la toca el hilo */
     int mw = 0, mh = 0;
     float textoFrac = 0.0f;      /* parte del mapa que se apoyo en la pantalla */
     std::string error;
     float ms = 0.0f;
     bool terminado = false;
     bool ocupado = false;        /* solo lo toca el hilo del visor */
     ~HiloRed() {
       {
         std::lock_guard<std::mutex> g(candado);
         salir = true;
       }
       aviso.notify_all();
       if (hilo.joinable())
         hilo.join();
     }
     /* quedarse quieto hasta que termine el mapa en el aire: hace falta antes de cambiar de red */
     void esperar() {
       std::unique_lock<std::mutex> g(candado);
       libre.wait(g, [this] { return !hayTrabajo && !enCurso; });
       terminado = false;
       ocupado = false;
     }
     void bucle() {
       for (;;) {
         Trabajo t;
         {
           std::unique_lock<std::mutex> g(candado);
           aviso.wait(g, [this] { return hayTrabajo || salir; });
           if (salir) return;
           t = std::move(trabajo);
           hayTrabajo = false;
           enCurso = true;
         }
         std::vector<uint8_t> m;
         std::string e;
         const bool ok = t.red->estimate(t.copia.data(), t.sz, t.sz, m, &e);
         float frac = 0.0f;
         std::vector<uint8_t> mk;
         int gw = 0, gh = 0;
         if (ok && t.apoyar) {
           /* los subtitulos y cualquier texto: van al plano de la pantalla para que no se deslicen
              con la cabeza. Se hace aqui, con la misma foto que genero este mapa. La memoria no
              se tira en los cambios de plano: un subtitulo que cruza el corte sigue quieto y sigue
              apoyado, y lo demas deja de estar quieto y sale solo en dos mapas. */
           frac = anclarTexto(t.copia.data(), t.sz, t.sz, m, t.red->focusPlane(), 8, &mk, &gw, &gh, &memoriaTexto);
         }
         /* y el reparto por ojo con los huecos rellenos (antes lo hacia el hilo que dibuja) */
         std::vector<uint8_t> rg;
         if (ok) {
           const int n = t.red->size();
           const size_t plano = (size_t)n * (size_t)n;
           if (m.size() == plano && g_cantosFinos && t.copia.size() >= plano * 4) {
             /* cantos finos (v0.89): el mapa tal cual y, con el, el color de la copia que vio la red
                (cuatro bytes por punto). El sombreador compara el pixel de ahora con ESTOS colores
                para decidir de que lado del canto cae: ver afinar() en weave.frag. */
             rg.resize(plano * 4);
             for (size_t i = 0; i < plano; i++) {
               rg[4 * i] = m[i];
               rg[4 * i + 1] = t.copia[4 * i];
               rg[4 * i + 2] = t.copia[4 * i + 1];
               rg[4 * i + 3] = t.copia[4 * i + 2];
             }
           } else if (m.size() == plano) {
             rg.resize(plano * 2);
             if (hoyosActivo() && !g_cantosFinos) {
               rellenarVistas(m.data(), n, t.ganancia * (float)n,
                              t.apoyoFijo >= 0.0f ? t.apoyoFijo : t.red->focusPlane(), rg.data(), nullptr);
             } else {
               for (size_t i = 0; i < plano; i++) rg[2 * i] = rg[2 * i + 1] = m[i];
             }
           }
         }
         {
           std::lock_guard<std::mutex> g(candado);
           mascara = std::move(mk), mw = gw, mh = gh, textoFrac = frac;
           mapaRG = std::move(rg);
           mapa = ok ? std::move(m) : std::vector<uint8_t>{};
           error = ok ? std::string() : e;
           ms = t.red->lastMs();
           terminado = true;
           enCurso = false;
         }
         libre.notify_all();
       }
     }
     /* `ganancia` es la del conversor (unidades de pantalla por unidad de mapa) y `apoyoFijo` el plano
        de apoyo cuando el usuario lo puso a mano (< 0 = el que saque la red de este mismo cuadro): con
        eso el relleno de huecos por ojo se hace aqui, fuera del hilo que dibuja (v0.69). */
     void arrancar(DepthNet *red, const uint8_t *chico, int sz, bool apoyar, float ganancia, float apoyoFijo) {
       {
         std::lock_guard<std::mutex> g(candado);
         trabajo.red = red;
         trabajo.copia.assign(chico, chico + (size_t)sz * (size_t)sz * 4);
         trabajo.sz = sz;
         trabajo.apoyar = apoyar;
         trabajo.ganancia = ganancia;
         trabajo.apoyoFijo = apoyoFijo;
         hayTrabajo = true;
         terminado = false;
         error.clear();
       }
       ocupado = true;
       if (!hilo.joinable()) hilo = std::thread([this] { bucle(); });
       aviso.notify_one();
     }
     /* 0 = sigue trabajando, 1 = mapa listo (en `fuera`), -1 = fallo (en `errFuera`) */
     int recoger(std::vector<uint8_t> &fuera, float &msFuera, std::string &errFuera,
                 std::vector<uint8_t> *masc = nullptr, int *mw = nullptr, int *mh = nullptr,
                 float *frac = nullptr, std::vector<uint8_t> *rgFuera = nullptr) {
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
       if (rgFuera)
         rgFuera->swap(mapaRG);
       if (masc)
         masc->swap(mascara);
       if (mw) *mw = this->mw, *mh = this->mh, *frac = textoFrac;
       return fuera.empty() ? -1 : 1;
     }
   } hiloRed;
  /* Las dos redes del boton 1 / 2: la que mande el servicio (--modelo) y la otra (--alterno). Lo
     que no venga por bandera se rellena por convencion si el archivo esta en su carpeta. */
  auto anotarRed = [&](const std::string &r) {
    if (!r.empty())
      ui.rutaRed[slotRed(r)] = r;
  };
  anotarRed(opts.modelPath);
  anotarRed(opts.altModel);
  if (ui.rutaRed[2].empty() && fileExists(rutaRedNuestra()))
    ui.rutaRed[2] = rutaRedNuestra();
  /* "opts" es de solo lectura, asi que la red con la que arrancamos vive aqui. Al arrancar se usa
     SIEMPRE la de Samsung (boton 1), que es la que calzo su programa y la que da el relieve que el
     panel espera; si no esta, la nuestra. Lo que se elige con 1 / 2 se recuerda en el archivo pero ya
     no manda al arrancar. */
  std::string modeloActivo = opts.modelPath;
  if (!ui.rutaRed[1].empty())
    modeloActivo = ui.rutaRed[1];
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
    /* Y si la red que pide el servicio simplemente no arranca (la de Samsung solo va en la grafica:
       por CPU se cuelga y aborta), probamos la nuestra, que si va por CPU. Mejor un conversor mas
       lento que ningún conversor, y asi el visor no se queda a dos dimensiones por un capricho. */
    if (!ui.rutaRed[2].empty() && ui.rutaRed[2] != modeloActivo && fileExists(ui.rutaRed[2])) {
      std::fprintf(stderr, "[conversor] %s\n[conversor] se prueba con la nuestra (%s)\n", e->c_str(),
                   ui.rutaRed[2].c_str());
      std::fflush(stderr);
      if (red.init(ui.rutaRed[2], e, gpu)) {
        modeloActivo = ui.rutaRed[2];
        ui.hud.state().red = 2;
        return true;
      }
    }
    return false;
  };
  if (convActivo && (modeloActivo.empty() || !fileExists(modeloActivo)) &&
      (ui.rutaRed[2].empty() || !fileExists(ui.rutaRed[2]))) {
    /* se pidio el conversor y no hay ninguna red que cargar: el video se vera plano, y el panel de
       ajustes se abre solo, en el menu, con el aviso y el boton de descarga (v0.94) */
    std::fprintf(stderr, "[conversor] no hay ninguna red de profundidad (%s): el video se ve plano; se abre el "
                         "panel de ajustes para descargarla\n", rutaRedNuestra().c_str());
    std::fflush(stderr);
    abrirPanelDeAjustes();
  }
  if (convActivo) {
    std::string rerr;
    const bool redLista = arrancarRed(convGpu, &rerr);
    marcaArranque("red");
    if (!redLista) {
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
      std::printf("[conversor] limpieza del mapa: filtro guiado (radio 8) + %s + corte de plano "
                  "(umbral 0,12, descanso 15 cuadros); apoyo por saliencia\n",
                  red.radioDilatar() > 0 ? "dilate 3" : "sin dilatar (el canto lo afina el sombreador)");
      if (ui.tune.planos > 0.0f) {
        char bufLam[80];
        std::printf("[conversor] dos planos: dureza %.1f%s (las laminas las decide el propio cuadro; "
                    "`g`/`v` las endurecen o las suavizan, 0 = relieve continuo)\n", ui.tune.planos,
                    ui.tune.muestras >= 1 ? (std::snprintf(bufLam, sizeof bufLam, " · %d muestra%s "
                                                           "intermedia%s (`n`/`m`)", ui.tune.muestras,
                                            ui.tune.muestras > 1 ? "s" : "", ui.tune.muestras > 1 ? "s" : ""),
                                             bufLam) : "");
      }
      ui.convInfo = true;
      ui.extraInfo.clear();   /* el recordatorio de las teclas ya va en el renglon de la pastilla */
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
  TrackFeedOptions fo;   /* queda a la vista del bucle: si la camara estaba ocupada, se reintenta con esto */
  bool rastreoVivo = false;
  int intentosCamara = 0;
  double ultimoIntentoCamara = 0.0;
  g_camaraPedida = opts.track;   /* se apunta aunque la apertura falle: es la senal para que `ctl vista` ceda */
  if (opts.track) {
    fo.device = opts.trackDevice;
    fo.model = opts.trackModel;
    fo.calibDir = opts.trackCalibDir;
    fo.set.model = opts.trackModel;
    fo.set.score = opts.trackScore;
    fo.set.smoothing = opts.trackSmoothing;
    /* ODYSSEYVK_EURO: "0" apaga el One-Euro (vuelve el exponencial de `suavizado`); "MIN;BETA" con
       coma o punto decimal cambia el corte minimo (Hz) y cuanto abre por cm/s. */
    if (const char *e = entornoOdyssey("ODYSSEYVK_EURO")) {
      std::string s(e);
      for (char &c : s)
        if (c == ',') c = '.';
      double a = 0.0, b = fo.set.euroBeta;
      if (std::sscanf(s.c_str(), "%lf;%lf", &a, &b) >= 1) {
        fo.set.euroMinCorte = fo.set.euroMinCorteFino = a;   /* puesto a mano: vale para las dos vias */
        fo.set.euroBeta = fo.set.euroBetaFino = b;
      }
      std::printf("[rastreo] One-Euro %s (corte minimo %.2f Hz, beta %.2f)\n",
                  fo.set.euroMinCorte > 0.0 ? "puesto" : "apagado: exponencial de antes", fo.set.euroMinCorte,
                  fo.set.euroBeta);
    }
    /* ODYSSEYVK_RASTREO_FINO=0 (rastreo_fino = no en serve.conf) vuelve al rastreo de antes de la
       v0.74: los ojos que da YuNet en cada vista, restados. Para comparar en el panel. */
    if (const char *e = entornoOdyssey("ODYSSEYVK_RASTREO_FINO")) fo.set.fino = !(e[0] == '0' || e[0] == 'n');
    std::printf("[rastreo] %s\n", fo.set.fino
                    ? "fino: la distancia sale de comparar la imagen de los ojos entre las dos vistas"
                    : "de antes (rastreo_fino = no): los ojos de YuNet en cada vista, restados");
    fo.fuerzaMono = opts.trackMono;
    fo.set.hFovDegrees = opts.trackCampo;
    fo.set.faceWidthMillimetres = opts.trackCara;
    if (opts.legado) {
      /* en modo de legado la cara se mide siempre con las dos vistas de la camara del monitor y con
         el margen de error de reproyeccion ancho (40 px en vez de 10): asi la pose sigue sirviendo
         aunque la cara salga movida o torcida, que es lo que uno espera de este modo. Con las dos
         vistas no hace falta la calibracion ni el tamano de cara a medida, asi que el hub no las pasa. */
      fo.fuerzaMono = 0;   /* 0 = las dos vistas de la camara del monitor, siempre */
      fo.set.maxReprojectionPx = 40.0;
    }
    std::string trk;
    const bool camaraLista = feed.start(fo, &trk);
    marcaArranque("camara");
    if (!camaraLista)
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
      /* Los mapas de correccion son de fabrica POR UNIDAD y cargar los de otro panel pasa
         desapercibido (mismo tamano, mismo aspecto), asi que decimos de donde salieron y como
         son: el rango y la media cambian mucho de una unidad a otra. */
      auto estad = [](const std::vector<uint8_t> &m, const char *que) {
        int lo = 255, hi = 0;
        double suma = 0.0;
        for (uint8_t v : m) { if (v < lo) lo = v; if (v > hi) hi = v; suma += (double)v; }
        std::printf("[correccion] mapa %s: %d..%d media %.1f\n", que, lo, hi,
                    m.empty() ? 0.0 : suma / (double)m.size());
      };
      std::printf("[correccion] mapas %dx%d y %dx%d\n", aw, ah, bw, bh);
      estad(a, "A");
      estad(b, "B");
    }
  }

#ifdef HAVE_CAPTURE
  uint64_t lastFrameCount = 0;
  double ultimoNuevo = 0.0;   /* momento del último fotograma nuevo del compositor */
  int avisosCorte = 0;
  int avisosFlujo = 0;
  double ultimoPresente = 0.0;
  bool haveCap = false;
  bool sizeWarned = false, importWarned = false;
  int fallosImport = 0;                /* importaciones falladas seguidas (memoria de video agotada) */
  bool avisoMemoriaHecho = false;
  Mpris media;                       /* pausar/reanudar el video del navegador sin quitar la capa */
  double nextMediaCheck = 0.0;
  double reanudaHasta = -1.0;        /* hasta cuando se reanuda solo un video parado (al volver de Tab) */
  int reanudados = 0;
  if (kind == SourceKind::Capture) {
    bomba.para();   /* (v1.12) desde aqui la bombea el bucle */
    if (!capPedida) {
      err = capErr;
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
    /* El aviso del formato solo tiene sentido cuando la imagen trae dos vistas. Con el conversor
       ponia "lado a lado" igual, y hacia pensar que la sesion estaba en el formato equivocado. */
    if (opts.conver) {
      std::printf("[formato] video normal a 3D: la red de profundidad decide cuanto desplaza cada ojo."
                  " Tecla 3 para el 2D plano y 1 / 2 para cambiar de red\n");
      std::fflush(stdout);
    } else if (opts.topBottom || (opts.fmtGiven && !opts.no3d)) {
      std::printf("[formato] %s. Si el video no tiene dos imagenes asi, se vera doble y movido:"
                  " tecla 3 para 2D plano o Ctrl+Shift+1 para apagar el 3D\n",
                  opts.topBottom ? "arriba y abajo (una imagen sobre otra)"
                                 : "lado a lado (una imagen al lado de otra)");
      std::fflush(stdout);
      ui.fmtInfo = std::string(" · ") + (opts.topBottom ? "arriba y abajo" : "lado a lado");
    }
    std::string merr;
    /* El reproductor solo se toca si la ventana capturada es de un navegador o de un reproductor
       (v0.75). Medido el 25-nov 10:52: con un juego delante (Stray) el visor cogio el reproductor
       del navegador, que estaba en pausa detras, y lo puso a sonar. Y se le da la pista de la
       aplicacion de la ventana, para que con dos navegadores abiertos gane el del video. */
    bool vivaIni = true;
    const std::string estIni = estadoVentana(opts.captureUuid, &vivaIni);
    const std::string appIni = estIni.substr(0, estIni.find(" "));
    const bool sinSaber = estIni.rfind("sin acceso", 0) == 0;   /* no se pudo preguntar: como antes */
    if (!sinSaber && !pareceNavegador(estIni) && !pareceReproductor(estIni)) {
      std::printf("[video] la ventana es de %s, que no es un navegador ni un reproductor: no se toca"
                  " ningun reproductor (MPRIS)\n", appIni.empty() ? "otra aplicacion" : appIni.c_str());
    } else {
      if (!sinSaber) media.pista(appIni);
      if (media.open(&merr))
        std::printf("[video] mando el video por MPRIS (%s): P lo pausa y lo reanuda\n", media.playerName().c_str());
      else
        std::printf("[video] sin reproductor MPRIS (%s): el video habra que pausarlo a mano\n", merr.c_str());
    }
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
  /* Atajos globales del hub (v0.75, atajos.h): para que Ctrl+Mayus+1..8 y F1 lleguen aunque el
     teclado lo tenga el video o el juego. Solo en sesiones de verdad (no en el taller sin panel),
     y se sueltan al salir. ODYSSEYVK_ATAJOS=0 (`atajos_globales = no`) no los registra. */
  AtajosGlobales atajos;
  cargaAtajos(ui);
  {
    const char *e = entornoOdyssey("ODYSSEYVK_ATAJOS");
    const bool quiere = !(e && (e[0] == '0' || e[0] == 'n'));
    const bool aLaFuerza = e && e[0] == '1';   /* en el taller sin panel, solo si se pide: para probarlos */
    if (quiere && (!opts.noPanel || aLaFuerza)) {
      std::string eA;
      if (atajos.abrir(&eA))
        std::printf("[atajos] globales del escritorio puestos (valen con el teclado en el video o en el juego): %s\n",
                    atajos.resumen().c_str());
      else
        std::printf("[atajos] sin atajos globales (%s): Ctrl+Mayus+1..8 solo valen con el visor enfocado\n",
                    eA.c_str());
      std::fflush(stdout);
    }
  }
  double ultimoCuadroVisto = 0.0;   /* cuando volcamos por ultima vez la vista de la camara */
  /* modo de legado: ahi la camara manda. Si deja de ver la cara se suelta la trama (el video se ve
     tal cual, sin relieve) y medio segundo despues se apaga la lente; al recuperar la cara vuelven
     las dos. Son los unicos dos segundos que este modo decide por su cuenta. */
  double ultimoPoseFresca = -1.0;
  bool tramaCortada = false;
  bool lenteViva = true;
  int frames = 0, errStreak = 0, reintentos = 0;
  bool dumpHecho = false;
  /* Vigia de la ventana (el bloque esta mas abajo en el bucle): si el compositor nos esconde,
     dejamos de trazar; al volver, pedimos otra vez la pantalla completa en nuestra salida y
     rehacemos el intercambio y la captura, que eran del tamano y la salida anteriores. */
  bool apartado = false;      /* el compositor nos tiene fuera de en medio */
  bool panelApagado = false;  /* ya devolvimos el panel a 2D mientras estabamos aparte */
  bool elHubDiceOculto = false;
  double desdeApartado = 0.0, intentoSitio = 0.0, ultimoChequeoVentana = 0.0;
  std::signal(SIGTERM, onTermSignal);
  std::signal(SIGINT, onTermSignal);
  rig.esperar();   /* (v1.12) la lente y el scaler, puestos en su hilo, tienen que estar antes del primer cuadro */
  marcaArranque("lente y scaler listos");
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
      /* su propio contador: `frames` vuelve a cero con cada linea de [fps] y los golpes de despues no llegaban */
      static long cuadrosGolpe = 0;
      ++cuadrosGolpe;
      if (hechos < (int)goles.size() && cuadrosGolpe >= 4 + hechos * 12) {
        const std::string tok = strLower(goles[hechos]);
        float cx = 0.0f, cy = 0.0f;
        bool tiene = false;
        if (tok == "capa") {   /* lo mismo que Ctrl+Shift+F1: ocultar o poner la pastilla */
          atajoHub(ui, "capa", "golpe de prueba");
        } else if (tok == "red") {
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
        Hud::Hit hit = tiene ? ui.hud.hit(cx, cy) : Hud::HitNone;
        /* por nombre, sin pasar por la geometria: los dos botones de icono de la pastilla */
        if (tok == "normal") hit = Hud::HitModoNormal, tiene = true;
        if (tok == "dividido") hit = Hud::HitModoDividido, tiene = true;
        if (tok == "suave") hit = Hud::HitModoSuave, tiene = true;
        if (tok == "rayo") hit = Hud::HitRendimientoMenu, tiene = true;   /* el boton del rayo (rama experimental) */
        if (tok == "modo") hit = Hud::HitModoRed, tiene = true;   /* (v1.12) el interruptor Neural | Rendimiento */
        float ax = 0, ay = 0, aw = 0, ah = 0;
        ui.hud.pillBounds(&ax, &ay, &aw, &ah);
        std::printf("[golpe] %s -> lienzo (%.0f,%.0f) -> golpe %d · pastilla %.0f,%.0f %.0fx%.0f escala %.2f\n",
                    tok.c_str(), (double)cx, (double)cy, (int)hit, (double)ax, (double)ay, (double)aw, (double)ah,
                    (double)ui.hud.scale());
        std::fflush(stdout);
        if (tiene && !clicFuera(ui, hit, cx, cy)) applyHit(ui, hit);
        /* un golpe es un clic entero: si cayo en un deslizador, el valor es el de ese punto y se suelta */
        if (tiene && ui.dragging != Hud::HitNone) {
          ui.hud.dragTo(ui.dragging, cx);
          ui.dragging = Hud::HitNone;
        }
        hechos++;
      }
    }
    glfwPollEvents();
    /* los atajos globales que haya pulsado el usuario desde el cuadro anterior */
    if (atajos.abierto())
      for (const std::string &cual : atajos.pulsados()) atajoHub(ui, cual, "atajo global");
    /* ---- el canal de ajuste vivo: la ventana de ajustes (proceso aparte) y `ctl ajuste` ----
       Una vez cada decima de segundo se mira el fichero ~/.local/state/odysseyvk/ajuste y, solo si
       CAMBIO, se aplican los numeros pedidos. Es importante el "solo si cambio": los numeros escritos
       son una ORDEN, no la verdad, y si se volvieran a aplicar cuadro tras cuadro una tecla pulsada
       despues se anularia sola. Ademas el visor deja en `estado` lo que esta usando de verdad (la
       ventana pinta de ahi, no de lo que ella cree) y en `preferencias.log` cada numero que se movio,
       venga de la tecla, de la pastilla o de la ventana, con la red que estaba puesta: asi se puede
       repasar despues que se probo y con que. */
    {
      static ajuste::Lector lector;
      static std::map<std::string, double> foto;   /* los numeros del vistazo anterior */
      static bool fotoPuesta = false;
      static double ultimoVistazo = 0.0, ultimoEstado = 0.0;
      const double momento = glfwGetTime();
      if (momento - ultimoVistazo >= 0.1) {
        ultimoVistazo = momento;
#ifdef HAVE_ORT
        if (red.ready()) { g_modeloEnUso = red.modelPath(); g_tarjetaEnUso = red.device(); }
#endif
        const std::map<std::string, double> actual = ajusteActual(ui, wv);
        if (fotoPuesta)
          for (const auto &kv : actual) {
            const auto antes = foto.find(kv.first);
            if (antes == foto.end() || antes->second == kv.second) continue;
            ajuste::registrar(kv.first, fmtNumero(antes->second), fmtNumero(kv.second),
                              nombreCorto(g_modeloEnUso), "la tecla o la pastilla");
          }
        foto = actual;
        fotoPuesta = true;
        std::map<std::string, double> pedidos;
        std::map<std::string, std::string> palabras;   /* las ordenes de palabra: `capa`, `camara`, `mono` */
        std::string modeloPedido;
        if (lector.cambio(pedidos, modeloPedido, &palabras)) {
          for (const auto &kv : pedidos) {
            if (ajuste::esOrden(kv.first)) continue;   /* `overlay` lo atiende el vigilante, no nosotros */
            aplicarAjuste(ui, wv, kv.first, kv.second);
          }
          /* `capa = ocultar|mostrar|alternar`: la misma tecla mandada por fichero. Sirve para el caso
             en que el teclado de la pantalla se lo quedo otra ventana (una pagina a pantalla completa
             que se apilo encima del visor, un juego que hace grab del teclado): ahi la tecla no nos
             llega, pero el fichero lo leemos igual cada decima de segundo. */
          const auto ordenCapa = palabras.find("capa");
          if (ordenCapa != palabras.end()) {
            const std::string v = strLower(ordenCapa->second);
            const bool puesta = ui.hud.overlayVisible();
            bool querida = puesta;
            if (v == "ocultar" || v == "quitar" || v == "0" || v == "no" || v == "off")
              querida = false;
            else if (v == "mostrar" || v == "poner" || v == "1" || v == "si" || v == "on")
              querida = true;
            else if (v == "alternar" || v == "toggle")
              querida = !puesta;
            else
              std::printf("[capa] no entiendo `capa = %s`: vale ocultar, mostrar o alternar\n", v.c_str());
            cambioCapa(ui, querida, "ctl capa");
          }
          const auto orden3D = palabras.find("3d");
          if (orden3D != palabras.end()) {
            const std::string v = strLower(orden3D->second);
            bool puesto = ui.hud.state().weaveOn;
            if (v == "apagar" || v == "quitar" || v == "0" || v == "no" || v == "off")
              puesto = false;
            else if (v == "encender" || v == "poner" || v == "1" || v == "si" || v == "on")
              puesto = true;
            else if (v == "alternar" || v == "toggle")
              puesto = !puesto;
            else
              std::printf("[3d] no entiendo `3d = %s`: vale encender, apagar o alternar\n", v.c_str());
            cambio3D(ui, puesto, "ctl 3d");
          }
          /* `legado = encender|apagar|alternar`: el modo de legado, por fichero. Es la misma puerta que
             el interruptor de la ventana de ajustes; el hub lo recuerda en serve.conf y lo pasa al
             arrancar, asi que se puede cambiar sin cerrar la sesion. */
          const auto ordenLegado = palabras.find("legado");
          if (ordenLegado != palabras.end()) {
            const std::string v = strLower(ordenLegado->second);
            bool querido = ui.hud.state().legado;
            if (v == "apagar" || v == "quitar" || v == "0" || v == "no" || v == "off")
              querido = false;
            else if (v == "encender" || v == "poner" || v == "1" || v == "si" || v == "on")
              querido = true;
            else if (v == "alternar" || v == "toggle")
              querido = !querido;
            else
              std::printf("[legado] no entiendo `legado = %s`: vale encender, apagar o alternar\n", v.c_str());
            cambioLegado(ui, wv, querido, "ctl legado");
          }
          /* `vista = 1|0`: la casilla de Previsualizacion de la hoja de Camara pidiendo la camara en
             vivo. Con `vista = 1` la copia del cuadro que guardamos en `camara.jpg` sale ocho veces por
             segundo en vez de una cada dos, y la ventana la va refrescando; la camara no se abre dos
             veces ni se le pide nada nuevo, porque el hilo del rastreo ya la esta leyendo a su tasa. Con
             `vista = 0` se vuelve a la cadencia tranquila, que es la que no gasta disco de mas. */
          /* `barra = guardar` o `barra = 12`: guardar copias chicas del cuadro para afinar el detector
             de la barra del reproductor (ver el bloque del video dividido, mas abajo) */
          const auto ordenBarra = palabras.find("barra");
          if (ordenBarra != palabras.end()) {
            /* la orden lleva su hora (`16@1764100000`): el fichero de ajuste se queda escrito y el visor
               lo lee tambien al arrancar, asi que sin la hora cada sesion nueva volveria a guardar */
            const std::string &v = ordenBarra->second;
            const int n = std::atoi(v.c_str());
            const size_t arroba = v.find('@');
            const long long hora = arroba == std::string::npos ? 0 : std::atoll(v.c_str() + arroba + 1);
            const long long edad = (long long)std::time(nullptr) - hora;
            if (n > 0 && edad >= 0 && edad <= 300) {
              g_barraGuardar = std::min(n, 60);
              std::printf("[barra] se guardaran %d copias chicas del cuadro en %s\n", g_barraGuardar,
                          ajuste::rutaBarra().c_str());
              std::fflush(stdout);
            }
          }
          /* `cargar_red = si` (v0.94): el panel de ajustes acaba de bajar la red abierta. Si el conversor
             estaba pedido y sin red, se carga ahora y el video pasa a 3D sin cerrar nada. (`red = N`
             es otra orden, la de cambiar de red con el conversor ya en marcha.) */
          /* `idioma = fr` (v1.07): el panel de ajustes cambio el idioma. La pastilla y sus avisos
             salen ya en el nuevo: se rehace el atlas de letras (con las chinas o japonesas si toca). */
          /* `preajuste = 0|1|2` (rama experimental): el panel eligio un preajuste del modo rendimiento */
          if (const auto op = palabras.find("preajuste"); op != palabras.end()) {
            const int k = std::atoi(op->second.c_str());
            if (k >= 0 && k <= 2) ui.preajustePedido = k;
          }
          const auto ordenIdioma = palabras.find("idioma");
          if (ordenIdioma != palabras.end()) {
            const int l = idioma::deCodigo(ordenIdioma->second);
            if (l >= 0 && l != idioma::actual()) {
              idioma::poner(l);
              std::string herr2;
              if (ui.hud.buildAtlas(&herr2))
                wv.setAtlas(ui.hud.atlasPixels(), ui.hud.atlasW(), ui.hud.atlasH());
              else
                std::fprintf(stderr, "[interfaz] atlas: %s\n", herr2.c_str());
              std::printf("[interfaz] idioma: %s\n", idioma::codigo(l));
              std::fflush(stdout);
            }
          }
          /* `atajos = recargar` (v1.09): se cambio un atajo en el panel. Se leen otra vez y los
             globales se registran de nuevo con las combinaciones nuevas. */
          if (palabras.count("atajos")) {
            cargaAtajos(ui);
            if (atajos.abierto()) {
              atajos.cerrar();
              std::string eA;
              if (atajos.abrir(&eA)) std::printf("[atajos] de nuevo: %s\n", atajos.resumen().c_str());
              else std::printf("[atajos] no se pudieron registrar otra vez: %s\n", eA.c_str());
            } else {
              std::printf("[atajos] recargados (solo con el visor enfocado)\n");
            }
            std::fflush(stdout);
          }
          const auto ordenRed = palabras.find("cargar_red");
          if (ordenRed != palabras.end()) {
            if (ui.rutaRed[2].empty() && fileExists(rutaRedNuestra())) ui.rutaRed[2] = rutaRedNuestra();
            /* bajo la ligera (fp16) con la de siempre puesta: se cambia a ella en marcha (rama experimental) */
            if (ui.rutaRed[2] == rutaRedVieja() && rutaRedNuestra() == rutaRedLigera()) {
              ui.rutaRed[2] = rutaRedLigera();
              if (modeloActivo == rutaRedVieja() && convActivo) ui.redRutaPedida = rutaRedLigera();
              else if (modeloActivo == rutaRedVieja()) modeloActivo = rutaRedLigera();
            }
            if (modeloActivo.empty() || !fileExists(modeloActivo)) modeloActivo = ui.rutaRed[2];
            if (!convActivo && !modeloActivo.empty() && fileExists(modeloActivo)) {
              std::printf("[conversor] la red de profundidad ya esta (%s): se carga ahora\n", modeloActivo.c_str());
              std::fflush(stdout);
              ui.convWant = true;
            }
          }
          /* `dividido_suavizado = si|no`: lo mandan los ajustes del programa. Cambia lo que hace el
             boton de video dividido y, si ahora mismo se esta viendo un dividido, lo cambia al momento. */
          const auto ordenSuave = palabras.find("dividido_suavizado");
          if (ordenSuave != palabras.end()) {
            const std::string v = strLower(ordenSuave->second);
            const bool autoQ = (v == "auto" || v == "automatico");
            const bool quiere = autoQ || !(v == "no" || v == "0" || v == "off" || v == "apagar" || v == "quitar");
            if (quiere != ui.suavizado || autoQ != ui.suavizadoAuto) {
              ui.suavizado = quiere;
              ui.suavizadoAuto = autoQ;
              ui.hud.state().suavizado = quiere;
              std::printf("[formato] los ajustes dejan el suavizado del video dividido en: %s\n",
                          autoQ ? "solo si hace falta" : (quiere ? "siempre" : "nunca"));
              std::fflush(stdout);
              if (!autoQ && ui.hud.state().modo3d >= 2 && ui.hud.state().weaveOn) ui.modoPedido = quiere ? 3 : 2;
            }
          }
          const auto ordenVista = palabras.find("vista");
          if (ordenVista != palabras.end()) {
            const std::string v = strLower(ordenVista->second);
            if (v == "apagar" || v == "quitar" || v == "0" || v == "no" || v == "off")
              g_vistaRapida = false;
            else if (v == "encender" || v == "poner" || v == "1" || v == "si" || v == "on")
              g_vistaRapida = true;
            else
              std::printf("[vista] no entiendo `vista = %s`: vale 1 o 0\n", v.c_str());
          }
          if (!modeloPedido.empty()) {
            ui.redRutaPedida = modeloPedido;   /* se atiende mas abajo, con la red parada */
            ui.avisoRedLejos = false;
          }
          foto = ajusteActual(ui, wv);         /* lo de fuera ya quedo contado al aplicarlo */
        }
        if (momento - ultimoEstado >= 0.5) {
          ultimoEstado = momento;
          ajuste::escribirEstado(textoEstado(ui, wv));
        }
      }
    }
#ifdef HAVE_ORT
    /* la tecla C: encender o apagar el conversor de video normal a 3D */
    if (ui.textoToggle) {
      if (hiloRed.ocupado) {
        /* el hilo de la estimacion esta dentro de la red: limpiarla a la mitad rompe el mapa en
           curso. Se atiende en la vuelta siguiente del bucle, cuando ya termino. */
      } else {
        ui.textoToggle = false;
        textoAncla = !textoAncla;
        hiloRed.esperar();
        red.reset();   /* el mapa viejo ya traia el apoyo dentro: no lo mezcles con el nuevo */
        std::printf("[rotulos] %s\n", textoAncla ? "apoyados en el plano de la pantalla (R los suelta)"
                                                 : "soltados: llevan la profundidad que le da la red (R los apoya)");
        std::fflush(stdout);
      }
    }
    if (ui.modoPedido) {
      /* Los botones de icono de la pastilla. Pasar de «imagen entera» a «solo la izquierda» (o al
         reves) le cambia a la red la imagen de debajo: se espera a su hilo, se tira lo que traiga
         a medias, se olvida su memoria (escala, apoyo, rotulos) y se tira el mapa puesto, que era
         de otra imagen. Hasta que llegue el mapa nuevo se ve la imagen plana (un par de cuadros). */
      const int modo = ui.modoPedido;
      ui.modoPedido = 0;
      const bool media = (modo == 3);
      if (media != wv.mediaFuente()) {
        if (hiloRed.ocupado) {
          hiloRed.esperar();
          std::vector<uint8_t> tiro;
          float msTiro = 0.0f;
          std::string eTiro;
          hiloRed.recoger(tiro, msTiro, eTiro);
        }
        red.reset();
        hiloRed.memoriaTexto.olvidar();
        wv.setMediaFuente(media);
        wv.dropDepthMap();
      }
      if (modo == 2) ui.convFuera = true;
      else ui.convWant = true;
    }
    if (ui.convFuera) {
      /* el boton de video dividido: apagar el conversor si estaba puesto (y nada mas si no) */
      ui.convFuera = false;
      if (convActivo) ui.convToggle = true;
    }
    if (ui.convToggle || ui.convWant) {
      const bool pidiendo = ui.convWant;
      ui.convToggle = false, ui.convWant = false;
      std::string cerr0;
      if (!convActivo) {
        if (!red.ready() && !arrancarRed(convGpu, &cerr0)) {
          std::fprintf(stderr, "[conversor] %s\n", cerr0.c_str());
          ui.extraInfo = idioma::tr("el conversor de vídeo normal no abrió"), ui.extraHasta = glfwGetTime() + 8.0;
        } else if ((!wv.converterReady() || wv.converterSize() != red.size()) && !wv.prepareConverter(red.size(), &cerr0)) {
          std::fprintf(stderr, "[conversor] %s\n", cerr0.c_str());
          ui.extraInfo = idioma::tr("el conversor de vídeo normal no abrió"), ui.extraHasta = glfwGetTime() + 8.0;
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
          ui.extraInfo.clear();   /* el recordatorio de las teclas ya va en el renglon de la pastilla */
          ui.extraHasta = glfwGetTime() + 9.0;
        }
      } else if (!pidiendo) {
        convActivo = false;
        std::printf("[conversor] video normal a 3D apagado\n");
        ui.convInfo = false;
      }
      std::fflush(stdout);
    }
    /* el boton 1 / 2 de la pastilla: pasar de la red de Samsung a la nativa sin cerrar el visor.
       Se atiende aqui, en el hilo del visor y con el hilo de la estimacion parado: mover la sesion
       de ONNX mientras ese hilo trabaja reventaria el cuadro en curso. La red nueva se abre antes
       de soltar la vieja? No: DepthNet::cambiar suelta la vieja primero para devolver la memoria a
       la tarjeta, y si la nueva no abre vuelve a cargar la anterior. */
    /* (v1.12) Neural | Rendimiento desde la pastilla, en marcha: el conversor se reabre con la misma ruta
       y `ODYSSEYVK_SIN_RED` puesto o quitado (lo lee DepthNet::init). Se apunta en serve.conf
       (`exp_sin_red`), para que el panel y la proxima sesion digan lo mismo. Si la red no abre, se
       vuelve a como estaba. */
    if (ui.modoRedPedido && !hiloRed.ocupado) {
      const bool aSinRed = ui.modoRedPedido == 1;
      ui.modoRedPedido = 0;
      std::string e2;
      if (!red.ready()) {
        ajuste::fijarEnServeConf("exp_sin_red", aSinRed ? "si" : "no", &e2);
        ui.extraInfo = idioma::tr("se aplica desde el siguiente vídeo");
        ui.extraHasta = glfwGetTime() + 6.0;
      } else if ((red.sinRed() != 0) != aSinRed) {
        hiloRed.esperar();
        const char *antesE = std::getenv("ODYSSEYVK_SIN_RED");
        const std::string antes = antesE ? antesE : "";
        if (aSinRed) ::setenv("ODYSSEYVK_SIN_RED", "imagen", 1);
        else ::unsetenv("ODYSSEYVK_SIN_RED");
        std::string ruta = red.modelPath();
        if (!aSinRed && (ruta.empty() || !fileExists(ruta))) ruta = ui.rutaRed[2];
        std::string cerr1;
        const auto t0 = std::chrono::steady_clock::now();
        if (red.cambiar(ruta, &cerr1, true)) {
          if (wv.converterSize() != red.size() && !wv.prepareConverter(red.size(), &cerr1))
            std::fprintf(stderr, "[conversor] %s\n", cerr1.c_str());
          ajuste::fijarEnServeConf("exp_sin_red", aSinRed ? "si" : "no", &e2);
          std::printf("[conversor] desde la pastilla: %s en %.0f ms (%s)\n",
                      aSinRed ? "modo rendimiento, sin red" : "Neural, con la red",
                      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(),
                      red.device().c_str());
          std::fflush(stdout);
        } else {
          if (antes.empty()) ::unsetenv("ODYSSEYVK_SIN_RED");
          else ::setenv("ODYSSEYVK_SIN_RED", antes.c_str(), 1);
          std::string cerr2;
          red.cambiar(red.modelPath().empty() ? ruta : red.modelPath(), &cerr2, true);
          std::fprintf(stderr, "[conversor] no se pudo cambiar de modo: %s\n", cerr1.c_str());
          std::fflush(stderr);
          ui.extraInfo = idioma::tr("no se pudo cambiar de modo: seguimos como estábamos");
          ui.extraHasta = glfwGetTime() + 9.0;
        }
      }
    }
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
          ui.extraInfo = std::string("red ") + (quiero == 1 ? "1 (Samsung) · " : "2 (nativa) · ") + red.device();
          ui.extraHasta = glfwGetTime() + 6.0;
        } else {
          std::fprintf(stderr, "[conversor] %s\n", cerr1.c_str());
          ui.hud.state().red = slotRed(red.modelPath());
          std::fflush(stderr);
          ui.extraInfo = idioma::tr("la otra red no abrió: seguimos con la anterior");
          ui.extraHasta = glfwGetTime() + 9.0;
        }
      }
    }
    /* La ventana de ajustes (o `ctl ajuste modelo=RUTA`) pide una red concreta por su ruta, no solo
       el boton 1 o 2 de la pastilla: asi se pueden probar los .onnx que el visor no ofrece, que es
       para lo que esta esa ventana. Se atiende aqui, con el hilo de la estimacion parado, igual que
       las teclas 1 y 2. Si el conversor esta apagado el encargo SE QUEDA pendiente: al encenderlo con
       C se abre la red pedida, en la misma vuelta del bucle. */
    if (!ui.redRutaPedida.empty()) {
      if (red.quemada()) {
        /* Un .onnx que no abrio deja ONNX Runtime inservible en este proceso (v0.33). No insistimos:
           el encargo sigue escrito en ~/.local/state/odysseyvk/ajuste y la proxima sesion arranca con
           esa red, que es lo que se puede hacer sin arriesgarse a que el visor reviente sin devolver
           el panel. */
        std::printf("[conversor] en esta sesion ya no se puede abrir otra red (%s): la pedida %s queda "
                    "apuntada en el fichero de ajuste y se abrira al reabrir el visor\n",
                    ui.redRutaPedida.c_str(), nombreCorto(ui.redRutaPedida).c_str());
        std::fflush(stdout);
        ui.extraInfo = idioma::tr("cambiar de red toca cerrar y abrir el visor");
        ui.extraHasta = glfwGetTime() + 9.0;
        ui.redRutaPedida.clear();
      } else if (!red.ready()) {
        if (!ui.avisoRedLejos) {
          ui.avisoRedLejos = true;
          std::printf("[conversor] pediste la red %s pero el conversor esta apagado: queda apuntada, "
                      "enciendelo con C\n", ui.redRutaPedida.c_str());
          std::fflush(stdout);
        }
      } else if (hiloRed.ocupado) {
        /* espera unos milisegundos a que termine el mapa que esta en el aire */
      } else if (ui.redRutaPedida == red.modelPath()) {
        ui.redRutaPedida.clear();                 /* ya estaba puesta: nada que hacer */
      } else {
        const std::string pedida = ui.redRutaPedida;
        const std::string anterior = red.modelPath();
        ui.redRutaPedida.clear();
        if (!fileExists(pedida)) {
          std::fprintf(stderr, "[conversor] la red pedida no existe: %s\n", pedida.c_str());
          std::fflush(stderr);
          ui.extraInfo = idioma::trf("esa red no existe: {}", {nombreCorto(pedida)});
          ui.extraHasta = glfwGetTime() + 9.0;
        } else {
          hiloRed.esperar();
          std::string cerr2;
          if (red.cambiar(pedida, &cerr2)) {
            if (wv.converterSize() != red.size() && !wv.prepareConverter(red.size(), &cerr2))
              std::fprintf(stderr, "[conversor] %s\n", cerr2.c_str());
            ui.hud.state().red = slotRed(pedida);
            grabarRedPreferida(slotRed(pedida));
            g_modeloEnUso = red.modelPath();
            g_tarjetaEnUso = red.device();
            ajuste::registrar("modelo", nombreCorto(anterior), nombreCorto(pedida), nombreCorto(pedida),
                              "la ventana de ajustes");
            std::printf("[conversor] red cambiada a %s en %s · mapa %d x %d\n", pedida.c_str(),
                        red.device().c_str(), red.size(), red.size());
            std::fflush(stdout);
            ui.extraInfo = "red " + nombreCorto(pedida) + " · " + red.device();
            ui.extraHasta = glfwGetTime() + 6.0;
          } else {
            std::fprintf(stderr, "[conversor] la red %s no abrio: %s\n", pedida.c_str(), cerr2.c_str());
            std::fflush(stderr);
            ui.hud.state().red = slotRed(red.modelPath());
            ajuste::registrar("modelo", nombreCorto(anterior), nombreCorto(pedida) + " (no abrio)",
                              nombreCorto(red.modelPath()), "la ventana de ajustes");
            ui.extraInfo = idioma::tr("la red pedida no abrió: seguimos con la anterior");
            ui.extraHasta = glfwGetTime() + 9.0;
          }
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

    /* Vigia de la ventana. En Wayland el compositor NO avisa al cliente de que lo minimizaron:
       simplemente deja de mandarnos sincronismo, y el bucle se queda presentando cuadros contra un
       intercambio que ya no tiene superficie -> el panel muestra trozos sueltos y ya no hay forma de
       responder a las teclas. El hub, que si ve el estado de la ventana por el protocolo de ventanas
       de KWin, escribe ~/.local/state/odysseyvk/ventana con "oculta" o "visible" y su reloj: mientras
       estemos ocultos no trazamos nada (y si dura, devolvemos el panel a 2D), y al volver pedimos
       otra vez la pantalla completa en NUESTRA salida, porque KWin devuelve la ventana donde le place
       y sin pixel a pixel la trama sale rota. */
    {
      bool marcaFresca = false;
      if (now - ultimoChequeoVentana >= 0.2) {
        ultimoChequeoVentana = now;
        std::string marca;
        char estado[32] = {0};
        double reloj = 0.0;
        if (readSmallFile(rutaEstadoOdyssey("ventana"), marca) &&
            std::sscanf(marca.c_str(), "%31s %lf", estado, &reloj) >= 1 && now - reloj < 5.0) {
          marcaFresca = true;
          elHubDiceOculto = !std::strcmp(estado, "oculta");
        } else {
          elHubDiceOculto = false;
        }
      }
      /* sin marca del hub (visor suelto, sin servicio) nos fiamos de GLFW */
      const bool escondida = elHubDiceOculto || (!marcaFresca && glfwGetWindowAttrib(win, GLFW_ICONIFIED) != 0);
      if (escondida) {
        if (!apartado) {
          apartado = true;
          desdeApartado = now;
          std::printf("[ventana] el compositor nos quito de en medio: dejamos de trazar hasta volver\n");
          std::fflush(stdout);
        }
        /* Mientras la marca este fresca el hub esta vivo y nos va a devolver la ventana en un par
           de segundos: si aqui apagamos el panel, el monitor hace 3D->2D->3D en cada minimizado.
           El panel solo se libera cuando la marca ya es vieja (hub muerto) y aun seguimos
           minimizados, que es el caso en que nadie nos va a devolver. */
        if (!panelApagado && !elHubDiceOculto && now - desdeApartado > 2.0) {
          panelApagado = true;
          rig.shutDown(); /* sin hub y sin ventana propia: el panel, a 2D */
        }
        glfwWaitEventsTimeout(0.15);
        continue;
      }
      if (apartado) {
        apartado = false;
        glfwRestoreWindow(win);
        if (!opts.windowed && !opts.borderless && mon) glfwSetWindowMonitor(win, mon, 0, 0, W, H, hz);
        glfwFocusWindow(win);
        glfwPollEvents();
        int nW = 0, nH = 0;
        glfwGetFramebufferSize(win, &nW, &nH);
        if (nW > 0 && nH > 0 && (nW != fbW || nH != fbH)) {
          fbW = nW;
          fbH = nH;
          ui.fbW = fbW;
          ui.fbH = fbH;
        }
        if (!wv.resize(fbW, fbH))
          std::fprintf(stderr, "[vulkan] al volver de minimizada el intercambio no se rehizo\n");
#ifdef HAVE_CAPTURE
        if (haveCap) {
          cap.stop();
          lastFrameCount = 0;
          ultimoNuevo = 0.0;
          reintentos = 0;
          std::string e2;
          if (!cap.start(opts.captureUuid, &e2)) std::fprintf(stderr, "[captura] al volver: %s\n", e2.c_str());
          reanudaHasta = glfwGetTime() + 8.0;   /* el cambio de modo de la vuelta tambien lo puede parar */
        }
#endif
        if (panelApagado) {
          panelApagado = false;
          rig.bringUp(opts);
        }
        std::printf("[ventana] de vuelta: lienzo %dx%d en %s, intercambio y captura rehechos\n", fbW, fbH,
                    mon ? glfwGetMonitorName(mon) : "la ventana");
        std::fflush(stdout);
        continue; /* una vuelta mas, esperando que el compositor vuelva a mandarnos fotograma */
      }
      /* Y si el compositor nos deja achicados o en otra salida, lo pedimos otra vez: el lienzo tiene
         que medir exactamente lo del modo, si no la red no cae sobre los subpixeles del panel. */
      if (!opts.windowed && !opts.borderless && mon && now - intentoSitio >= 3.0) {
        int nW = 0, nH = 0;
        glfwGetFramebufferSize(win, &nW, &nH);
        if (nW > 0 && nH > 0 && (nW != W || nH != H)) {
          intentoSitio = now;
          std::printf("[ventana] el compositor dejo la ventana a %dx%d (el modo es %dx%d): pedimos otra"
                      " vez la pantalla completa en %s\n", nW, nH, W, H, glfwGetMonitorName(mon));
          std::fflush(stdout);
          glfwSetWindowMonitor(win, mon, 0, 0, W, H, hz);
          glfwPollEvents();
          int mW = 0, mH = 0;
          glfwGetFramebufferSize(win, &mW, &mH);
          if (mW > 0 && mH > 0 && (mW != fbW || mH != fbH)) {
            fbW = mW;
            fbH = mH;
            ui.fbW = fbW;
            ui.fbH = fbH;
            if (!wv.resize(fbW, fbH))
              std::fprintf(stderr, "[vulkan] el intercambio no siguio el nuevo tamano del lienzo\n");
          }
        }
      }
    }

    /* la fuente de este cuadro */
    bool newSource = false;
    if (kind == SourceKind::Test) {
      /* ODYSSEYVK_FUENTE_PNG (taller, v0.62): un PNG del tamano de la pantalla en lugar del patron.
         Sirve para pasar un cuadro real (p. ej. el -trama-cruda.png de un volcado) por el sombreador
         de verdad con --mapa-fijo y --dump, sin video, sin red y sin panel. */
      static const std::string kFuentePng = [] {
        const char *e = entornoOdyssey("ODYSSEYVK_FUENTE_PNG");
        return e ? std::string(e) : std::string();
      }();
      static bool fuentePngLista = false;
      if (!kFuentePng.empty() && !fuentePngLista) {
        fuentePngLista = true;
#ifdef HAVE_PNG
        int pw = 0, ph = 0;
        std::vector<uint8_t> px;
        if (pngLoadRGBA(kFuentePng, pw, ph, px) && pw == fbW && ph == fbH) {
          testPx.swap(px);
          newSource = wv.sourceFromRgba(testPx.data(), fbW, fbH, true, &err);
          haveTest = true;
          testAnim = false;
          std::printf("[taller] fuente fija %s (%dx%d)\n", kFuentePng.c_str(), pw, ph);
        } else {
          std::fprintf(stderr, "[taller] ODYSSEYVK_FUENTE_PNG: %s no se pudo leer o no mide %dx%d\n",
                       kFuentePng.c_str(), fbW, fbH);
        }
#endif
      }
      /* ODYSSEYVK_FUENTE_VIVA=1 (taller, v0.69): da la fuente por nueva en cada cuadro, sin volver a
         subirla. El conversor trabaja entonces con la cadencia de un video (una estimacion cada tres
         cuadros) y se puede medir el coste del bucle sin captura, sin navegador y sin panel. */
      static const bool kFuenteViva = entornoOdyssey("ODYSSEYVK_FUENTE_VIVA") != nullptr;
      if (kFuenteViva && haveTest) newSource = true;
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
        if (lastFrameCount == 0) marcaArranque("primer cuadro capturado");
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
          if (!wv.sourceFromDma(gf, &err)) {
            if (!importWarned) {
              importWarned = true;
              std::fprintf(stderr,
                           "[captura] no se pudo importar el fotograma: %dx%d paso %d offset %d fourcc %c%c%c%c "
                           "modificador 0x%llx descriptor %d\n",
                           gf.w, gf.h, gf.stride, gf.offset,
                           char(gf.fourcc & 0xff), char((gf.fourcc >> 8) & 0xff), char((gf.fourcc >> 16) & 0xff),
                           char((gf.fourcc >> 24) & 0xff), (unsigned long long)gf.modifier, gf.fd);
              std::fprintf(stderr, "[captura] %s\n", err.c_str());
            }
            /* Y NO marcamos la fuente como nueva: no hay cuadro importado que tramar. Presentando el
               ultimo que si lo estaba se llenaba la pantalla de trozos sueltos. */
            if (++fallosImport == 12 && !avisoMemoriaHecho) {
              avisoMemoriaHecho = true;
              std::fprintf(stderr, "[captura] %d fotogramas seguidos sin poder importarse: a la grafica le falta"
                                   " memoria de video. Cierra ventanas o pestanas con aceleracion y el 3D se "
                                   "reanuda solo\n", fallosImport);
              std::fflush(stderr);
            }
          } else {
            fallosImport = 0;
            newSource = true;
          }
        }
      }
      /* El navegador suele dejar el video en pausa cuando cambiamos el modo del monitor o
         cuando la ventana queda tapada por la nuestra. Como el flujo sigue llegando, el visor
         no se da cuenta: miramos el estado MPRIS y lo reanudamos, salvo que el usuario lo
         pausara el con P. */
      /* v0.75: solo en los primeros segundos de la sesion (y al volver de Tab), que es cuando el
         cambio de modo lo deja parado. Antes valia toda la sesion: medido el 25-nov, cada pausa que
         ponia Rodolfo en el navegador se deshacia a los 1-3 s («se reanuda solo», hasta seis veces). */
      if (media.playerFound() && now >= nextMediaCheck && ((now - t0 < 8.0 && !rig.panelSinGeometria) || now < reanudaHasta)) {
        /* (v0.86) Al arrancar solo se reanuda si el 3D cambio el modo o la escala del monitor, que
           es lo que hacia que el navegador parara el video. Desde la v0.76 no se cambia nada, y
           esta reanudacion le daba play a las pausas de Rodolfo en los primeros 8 s de cada sesion
           (MEDIDO 25/11 18:08: Paused a los 4 y a los 6 s, reanudado las dos veces). */
        nextMediaCheck = now + 3.0;
        std::string mst, merr;
        if (media.status(&mst, &merr) && mst != "Playing" && !ui.mediaPausaManual && reanudados < 6) {
          ++reanudados;
          if (media.resumeIfNeeded(&merr)) {
            std::printf("[video] el navegador habia parado el video (%s): se reanuda solo\n", mst.c_str());
            ui.estadoVideo = " · video reanudado";
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
          /* sin flujo y sin ventana del video no hay nada que esperar: nos vamos nosotros, que es
             como se devuelve el panel a 2D y se apaga la lente. Sin esto la sesion se quedaba viva
             mostrando el ultimo cuadro, con la pastilla detras de la ventana activa. */
          bool viva = true;
          estadoVentana(opts.captureUuid, &viva);
          if (!viva) {
            std::printf("[captura] la ventana del video cerro sin dejar flujo: se cierra la sesion"
                        " y se devuelve el panel a 2D\n");
            std::fflush(stdout);
            ui.quit = true;
          }
        } else if ((lastFrameCount == 0 && now - t0 > 5.0 && reintentos < 12) ||
                   (ultimoNuevo > 0.0 && now - ultimoNuevo > 2.0)) {
          /* dos casos: el nodo se creo pero no llega nada (el portal tarda o el cambio de modo
             del panel invalida la sesion), o el flujo se paro a mitad de la sesion. En ambos
             hay que pedir el flujo otra vez, y decir por dónde vino el corte. */
          const bool cortado = ultimoNuevo > 0.0;
          if (cortado && avisosCorte < 4) {
            ++avisosCorte;
            ++avisosFlujo;
            const std::string flujo = cap.stateText();
            std::printf("[captura] %d s sin fotogramas nuevos del compositor (flujo: %s)\n",
                        (int)(now - ultimoNuevo), flujo.c_str());
            bool viva = true;
            const std::string est = estadoVentana(opts.captureUuid, &viva);
            if (!est.empty()) std::printf("[captura] ventana capturada: %s\n", est.c_str());
            /* el consejo del navegador solo aplica si nuestro flujo sigue vivo: si el estado no es
               "streaming" el roto es nuestro (el flujo nuevo nunca se conecto), y mandarnos a
               about:config era ruido. Medido el 23-nov: con el contador clavado,
               `ctl flujo --uuid <video> --segundos 8` seguia dando 65,6 fps. */
            if (pareceNavegador(est) && avisosFlujo == 1 && flujo.rfind("streaming", 0) == 0) {
              std::printf("[captura] el navegador dejo de pintar porque el compositor ya no le manda el sincronismo"
                          " (la tapamos por completo).\n"
                          "            primero descartar lo nuestro: odysseyvk ctl flujo --uuid <id> --segundos 5\n"
                          "            (si ahi si llegan fotogramas, el flujo del visor es el que no arranca)\n"
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
    tirTrabajo0 = glfwGetTime();
    if (newSource) {   /* los huecos entre cuadros del juego (o del video): si son grandes, el que tira es el */
      if (tirFuenteUlt > 0.0) {
        const double g = tirTrabajo0 - tirFuenteUlt;
        tirFuenteMax = std::max(tirFuenteMax, g);
        if (g > 0.050) tirFuenteHuecos++;
      }
      tirFuenteUlt = tirTrabajo0;
      tirFuenteN++;
    }

    /* la interfaz se rehace en CPU cada cuadro (son pocos triángulos) y se sube su geometría */
    WeaveTuning &t = wv.tuning();
    /* Vigía de los numeros de la trama. En v0.45 el almacen en CPU del bloque de la trama se quedo
       a 288 bytes mientras el bloque paso a 320: cada cuadro se escribian 32 bytes fuera, dentro de
       la pila, y esos 32 bytes cayeron encima de los numeros de la trama del visor. El contraste se
       fue a 0 y el sombreador con contraste 0 pinta medio gris en todos los pixeles: pantalla GRIS
       pareja sin imagen, con la red trabajando y la captura bien. El aviso era mudo.
       Aqui miramos de cuadro a cuadro los unicos numeros que ninguna tecla toca (contraste, escala
       de la correccion A y crosstalk dinamico): si cambian solos, algo los esta pisando. Los demas
       (paso, filtro, fase, crosstalk fijo, conversor, rampas, las capas y la dureza de los dos
       planos) los mueven las teclas a proposito, asi
       que aqui no se comparan; en su lugar se vigila que ninguno se vuelva un numero loco. */
    static WeaveTuning vezAnterior{};
    static bool avisoPisoton = false;
    static bool primeraVez = true;
    if (!primeraVez && !avisoPisoton) {
      const struct { const char *nombre; float a, b; } fijos[] = {
          {"el contraste", vezAnterior.contrast, ui.tune.contrast},
          {"la escala de la correccion A", vezAnterior.corrAScale, ui.tune.corrAScale},
          {"el crosstalk que sigue a la pantalla", vezAnterior.xtalkDyn, ui.tune.xtalkDyn}};
      for (const auto &c : fijos) {
        if (c.a == c.b) continue;
        avisoPisoton = true;
        std::fprintf(stderr, "[aviso] algo pise los numeros de la trama entre cuadros: %s valia %.6f y ahora vale %.6f "
                             "(con el contraste o el filtro a 0 el cuadro sale gris o sin trama)\n",
                     c.nombre, c.a, c.b);
        break;
      }
      const float todos[] = {ui.tune.filterSlope, ui.tune.xtalkFac, ui.tune.contrast, ui.tune.corrAScale,
                             ui.tune.pitch, ui.tune.align, ui.tune.conv, ui.tune.rampaDX, ui.tune.rampaDY,
                             ui.tune.ganFondo, ui.tune.ganFrente, ui.tune.empFondo, ui.tune.empFrente,
                             ui.tune.planos};
      for (float v : todos) {
        if (std::isfinite(v)) continue;
        avisoPisoton = true;
        std::fprintf(stderr, "[aviso] un numero de la trama se volvio loco (%g): el sombreador con eso no dibuja nada "
                             "util y el cuadro sale gris\n", v);
        break;
      }
    }
    primeraVez = false;
    /* --mapa-fijo: medir el relieve con un mapa que ya conocemos (el que escribe --dump, o el que sale
       de `odysseyvk mapa foto.png`), sin red y sin sentarse delante del panel. Se sube una sola vez y
       se deja quieto: asi la diferencia entre dos pasadas de la misma orden es solo lo que se cambi. */
    if (!opts.mapaFijo.empty()) {
      static std::vector<uint8_t> mfMapa;
      static int mfW = 0, mfH = 0;
      static bool mfIntentado = false;
      if (!mfIntentado) {
        mfIntentado = true;
        std::string errmf;
        if (leerPgm(opts.mapaFijo, mfW, mfH, mfMapa, &errmf)) {
          unsigned hist[256] = {0};
          unsigned niveles = 0;
          double suma = 0.0;
          for (uint8_t v : mfMapa) { hist[v]++; suma += (double)v; }
          for (unsigned q : hist) if (q) niveles++;
          std::printf("[mapa fijo] %s · %d x %d · %u niveles · media %.1f (sin red)\n",
                      opts.mapaFijo.c_str(), mfW, mfH, niveles, suma / (double)mfMapa.size());
        } else {
          std::fprintf(stderr, "[mapa fijo] %s\n", errmf.c_str());
        }
      }
      if (!mfMapa.empty()) {
        std::string errmf;
        /* ODYSSEYVK_GUIA_PNG (taller, v0.89): con los cantos finos, la imagen de la que se supone que
           salio este mapa. Sus colores viajan con el mapa, como en una sesion de verdad (ver afinar()
           en weave.frag); dando una imagen distinta de la que se dibuja se prueba lo que pasa tras
           un cambio de plano, cuando el mapa todavia es del plano anterior. */
        std::vector<uint8_t> conGuia;
#ifdef HAVE_PNG
        if (const char *gp = entornoOdyssey("ODYSSEYVK_GUIA_PNG")) {
          int gw = 0, gh = 0;
          std::vector<uint8_t> gpx;
          if (g_cantosFinos && pngLoadRGBA(gp, gw, gh, gpx) && gw > 0 && gh > 0) {
            conGuia.resize((size_t)mfW * (size_t)mfH * 4);
            for (int y = 0; y < mfH; y++)
              for (int x = 0; x < mfW; x++) {
                const int sx = std::min(gw - 1, (int)(((double)x + 0.5) * gw / mfW));
                const int sy = std::min(gh - 1, (int)(((double)y + 0.5) * gh / mfH));
                const uint8_t *s = &gpx[((size_t)sy * (size_t)gw + (size_t)sx) * 4];
                uint8_t *d = &conGuia[((size_t)y * (size_t)mfW + (size_t)x) * 4];
                d[0] = mfMapa[(size_t)y * (size_t)mfW + (size_t)x];
                d[1] = s[0], d[2] = s[1], d[3] = s[2];
              }
            std::printf("[mapa fijo] con los colores de %s (%d x %d)\n", gp, gw, gh);
          } else {
            std::fprintf(stderr, "[mapa fijo] ODYSSEYVK_GUIA_PNG: %s no se pudo leer (o los cantos no son los finos)\n", gp);
          }
        }
#endif
        const bool subido = conGuia.empty() ? wv.setDepthMap(mfMapa.data(), mfW, mfH, &errmf)
                                            : wv.setDepthMapGuia(conGuia.data(), mfW, mfH, &errmf);
        if (subido) mfMapa.clear();
        else std::fprintf(stderr, "[mapa fijo] no se pudo subir: %s\n", errmf.c_str());
      }
    }
    /* en modo de legado, si la camara perdio la cara se suelta la trama y el video se ve tal cual:
       sin alguien mirando, el relieve no sirve de nada y lo que molesta es la trama encima. */
    const bool caraPerdida = opts.legado && ui.hud.state().legado && tramaCortada;
    t.weave = ui.tune.weave && ui.hud.state().weaveOn && !caraPerdida;
    t.swapLR = ui.tune.swapLR;
    t.topBottom = ui.tune.topBottom;
    t.filterSlope = ui.tune.filterSlope;
    t.align = ui.tune.align;
    t.pitch = ui.tune.pitch;
    t.xtalkDyn = ui.tune.xtalkDyn;
    t.contrast = ui.tune.contrast;
    t.corrAScale = ui.tune.corrAScale;
    /* rampas de las teclas `[` `]` `;` `'` y patron de un ojo (`b`). Hasta v0.59 estas tres no se
       copiaban: las teclas cambiaban el numero de `estado` pero la trama Vulkan seguia con la rampa
       de serve.conf sola, y `b` no hacia nada. (El volcado con ODYSSEYVK_PRUEBA no lo vio porque la
       sonda entra por el entorno, no por esta copia.) */
    t.rampaDX = ui.tune.rampaDX;
    t.rampaDY = ui.tune.rampaDY;
    t.unOjo = ui.tune.unOjo;
    /* capas: cuanto se estira el relieve a cada lado del plano de apoyo (teclas j/k y u/i) y
       cuanto se desplaza esa banda entera de su plano (h/l y y/o) */
    t.ganFondo = ui.tune.ganFondo;
    t.ganFrente = ui.tune.ganFrente;
    t.empFondo = ui.tune.empFondo;
    t.empFrente = ui.tune.empFrente;
    t.capaTocada = ui.tune.capaTocada;
    const Hud::State &st = ui.hud.state();
    t.conv = ui.convBase + (float)st.popout / 50.0f * 0.004f;
    vezAnterior = t;
    wv.setDepthShift(((float)st.depth - 50.0f) / 50.0f * 0.004f);
    /* ---- video dividido: la barra del reproductor, de una pieza (v0.81; ver barra.h) ----
       Cada seis cuadros nuevos (o cada 0,2 s si el video esta quieto) se baja la copia chica del
       cuadro (la misma del conversor) y se buscan las filas donde las dos mitades no casan: ahi el
       reproductor pinto su barra a lo ancho y esas filas se tejen en 2D con el cuadro entero.
       `barra_2d = no` en serve.conf lo apaga; `barra_2d = entera` deja esas filas tal cual vienen (la
       franja de la v0.81 a la v0.89, con las dos mitades del video detras de la barra). */
    {
      static const bool kBarra = [] {
        const char *e = entornoOdyssey("ODYSSEYVK_BARRA_2D");
        return !(e && (e[0] == '0' || e[0] == 'n' || e[0] == 'N'));
      }();
      static const bool kFranjaEntera = [] {
        const char *e = entornoOdyssey("ODYSSEYVK_BARRA_2D");
        return e && (e[0] == 'e' || e[0] == 'E');
      }();
      wv.setFranjaEntera(kFranjaEntera);
      static DetectorBarra detector;
      static std::vector<uint8_t> chica;
      static int cuentaBarra = 0, guardados = 0;
      static double ultimoAnalisis = -1.0, ultimoGuardado = -1.0, parteDesde = -1.0;
      static bool barraFallo = false, avisoNoDividido = false, habiaBarra = false;
      static int fallosBarra = 0;
      /* el parte de cada 20 s: sin el no se sabe, leyendo el registro, si el detector no vio nada o
         si no habia nada que ver */
      static int pAnalisis = 0, pConBanda = 0, pNoDividido = 0;
      static float pCentroMin = 1e9f, pCentroMax = 0.0f, pMaxAbajo = 0.0f, pMaxArriba = 0.0f;
      /* y cuanto separa el propio video sus dos imagenes (una medida por segundo, v0.83) */
      static std::vector<float> sepNormal, sepCerca, sepLejos;
      static double ultimaSeparacion = -1.0;
      /* la ventana trae dos imagenes lado a lado: o se tejen tal cual (sin conversor) o el
         conversor trabaja con la izquierda sola (el suavizado). En el suavizado vale tambien con el 3D
         apagado: ahi el 2D es la imagen izquierda estirada y la barra saldria a medias. */
      const bool fuenteDividida = wv.mediaFuente() || (!ui.convInfo && !wv.converterOn());
      const bool dividido = kBarra && (st.weaveOn || wv.mediaFuente()) && !st.ask && fuenteDividida &&
                            !ui.tune.topBottom && opts.mapaFijo.empty();
      if (!dividido) {
        if (habiaBarra) {
          habiaBarra = false;
          std::printf("[barra] ya no es video dividido: se quitan las filas en 2D\n");
          std::fflush(stdout);
        }
        detector.olvidar();
        cuentaBarra = 0;
        parteDesde = -1.0;
        wv.setFilas2D(0.0f, 0.0f);
      } else if (!barraFallo && wv.hasSource() &&
                 ((newSource && ++cuentaBarra >= 6) || now - ultimoAnalisis >= 0.2)) {
        /* (con `hasSource`: el reloj de 0,2 s no espera a un cuadro nuevo, y antes del primer cuadro
           no hay nada que bajar) */
        cuentaBarra = 0;
        ultimoAnalisis = now;
        std::string eb;
        if ((wv.converterReady() || wv.prepareConverter(518, &eb)) && wv.readSmall(chica, &eb, true)) {
          const int lado = wv.converterSize();
          fallosBarra = 0;
          DetectorBarra::Medida med;
          const Filas2D f = detector.analizar(chica.data(), lado, lado, now, &med);
          wv.setFilas2D(f.arriba, f.abajo);
          const bool hay = f.arriba > 0.0f || f.abajo > 0.0f;
          const bool cambio = hay != habiaBarra;
          if (cambio) {
            habiaBarra = hay;
            if (hay)
              std::printf("[barra] el reproductor pinto encima del video dividido: en 2D el %.1f %% de abajo y el "
                          "%.1f %% de arriba (filas %d+%d / %d+%d, residuo %.0f / %.0f, umbral %.0f, centro %.1f)\n",
                          100.0 * f.abajo, 100.0 * f.arriba, med.filasAbajo, med.fuertesAbajo, med.filasArriba,
                          med.fuertesArriba, med.maxAbajo, med.maxArriba, med.umbral, med.centro);
            else
              std::printf("[barra] se fue: todo el cuadro vuelve a 3D\n");
            std::fflush(stdout);
          }
          if (!med.dividido && !avisoNoDividido) {
            avisoNoDividido = true;
            std::printf("[barra] las dos mitades no casan ni en el centro del cuadro (residuo %.0f): este video no "
                        "parece dividido, no se toca ninguna fila\n", med.centro);
            std::fflush(stdout);
          }
          /* el parte */
          if (parteDesde < 0.0) parteDesde = now;
          pAnalisis++;
          if (hay) pConBanda++;
          if (!med.dividido) pNoDividido++;
          pCentroMin = std::min(pCentroMin, med.centro), pCentroMax = std::max(pCentroMax, med.centro);
          pMaxAbajo = std::max(pMaxAbajo, med.maxAbajo), pMaxArriba = std::max(pMaxArriba, med.maxArriba);
          if (med.dividido && now - ultimaSeparacion >= (ui.suavizadoAuto ? 0.25 : 1.0)) {
            ultimaSeparacion = now;
            const Separacion sp = medirSeparacion(chica.data(), lado, lado);
            if (sp.valida) {
              sepNormal.push_back(sp.normal);
              sepCerca.push_back(sp.cerca);
              sepLejos.push_back(sp.lejos);
            }
            /* Suavizado automatico (v0.88). El video dividido se teje con su propio relieve mientras
               quepa; en cuanto lo mas cercano o lo mas lejano pasa de 8 mm de separacion (el doble de
               lo que saca el conversor de este panel) se pasa al suavizado, y se vuelve cuando lleva
               4 s seguidos por debajo de 5 mm. Los dos umbrales y la espera son para que no baile. */
            if (ui.suavizadoAuto && sp.valida && ui.hud.state().modo3d >= 2 && ui.hud.state().weaveOn && !ui.modoPedido) {
              static double cabeDesde = -1.0, cambioEn = -100.0;
              const float peor = std::max(sp.cerca, -sp.lejos);
              Hud::State &hs = ui.hud.state();
              if (peor > 8.0f / 598.0f) {
                cabeDesde = -1.0;
                if (hs.modo3d == 2 && now - cambioEn >= 1.0) {
                  ui.modoPedido = 3;
                  cambioEn = now;
                  std::printf("[relieve] el video separa %.0f mm: se pasa solo al suavizado\n", 598.0 * peor);
                  std::fflush(stdout);
                }
              } else if (peor < 5.0f / 598.0f) {
                if (cabeDesde < 0.0) cabeDesde = now;
                if (hs.modo3d == 3 && now - cabeDesde >= 4.0 && now - cambioEn >= 4.0) {
                  ui.modoPedido = 2;
                  hs.depth = 50, hs.popout = 0;   /* sin el empuje del conversor */
                  cambioEn = now;
                  std::printf("[relieve] el video vuelve a caber (%.0f mm): otra vez con su propio relieve\n", 598.0 * peor);
                  std::fflush(stdout);
                }
              } else {
                cabeDesde = -1.0;
              }
            }
          }
          if (now - parteDesde >= 20.0) {
            std::printf("[barra] 20 s: %d analisis · centro %.1f..%.1f · residuo maximo abajo %.0f, arriba %.0f · con "
                        "banda %d %% del tiempo · no dividido %d %%\n", pAnalisis, pCentroMin, pCentroMax, pMaxAbajo,
                        pMaxArriba, 100 * pConBanda / std::max(1, pAnalisis), 100 * pNoDividido / std::max(1, pAnalisis));
            if (sepNormal.size() >= 3) {
              /* en milimetros de ESTE panel (27 pulgadas, 598 mm de ancho): lo normal es la mediana
                 de las medidas, y de lo mas cercano y lo mas lejano se toma la segunda medida por cada
                 punta, para que un solo segundo raro no mande */
              const double mm = 598.0;
              std::sort(sepNormal.begin(), sepNormal.end());
              std::sort(sepCerca.begin(), sepCerca.end());
              std::sort(sepLejos.begin(), sepLejos.end());
              std::printf("[relieve] el video dividido separa sus dos imagenes: lo normal %+.1f mm · lo mas cercano "
                          "%+.1f mm · lo mas lejano %+.1f mm (%d medidas; el conversor de este panel no pasa de 4 mm)%s\n",
                          mm * sepNormal[sepNormal.size() / 2], mm * sepCerca[sepCerca.size() - 2], mm * sepLejos[1],
                          (int)sepNormal.size(), wv.mediaFuente() ? " · ahora en suavizado" : "");
            }
            sepNormal.clear(), sepCerca.clear(), sepLejos.clear();
            std::fflush(stdout);
            parteDesde = now;
            pAnalisis = pConBanda = pNoDividido = 0;
            pCentroMin = 1e9f, pCentroMax = pMaxAbajo = pMaxArriba = 0.0f;
          }
          /* `odysseyvk ctl barra guardar [N]`: deja en ~/.local/state/odysseyvk/barra las proximas N
             copias chicas (cada vez que la banda se pone o se quita, y si no una por segundo), para
             afinar el detector con cuadros del reproductor de verdad. */
          if (g_barraGuardar > 0 && (cambio || now - ultimoGuardado >= 1.0)) {
            ultimoGuardado = now;
            const std::string dir = ajuste::rutaBarra();
            ::mkdir(dir.c_str(), 0700);
            char nombre[96];
            std::snprintf(nombre, sizeof nombre, "/%03d-%s.ppm", guardados++,
                          cambio ? (hay ? "se-pone" : "se-quita") : (hay ? "con-banda" : "sin-banda"));
            if (FILE *fp = std::fopen((dir + nombre).c_str(), "wbe")) {
              std::fprintf(fp, "P6\n%d %d\n255\n", lado, lado);
              for (size_t p = 0; p < (size_t)lado * (size_t)lado; p++) std::fwrite(chica.data() + p * 4u, 1, 3, fp);
              std::fclose(fp);
              std::printf("[barra] guardada %s%s (quedan %d)\n", dir.c_str(), nombre, g_barraGuardar - 1);
              std::fflush(stdout);
            }
            g_barraGuardar--;
          }
        } else {
          /* un fallo suelto no apaga el detector; cinco seguidos si, y se dice una vez */
          if (++fallosBarra >= 5) {
            barraFallo = true;
            std::fprintf(stderr, "[barra] no se pudo leer la copia chica del cuadro (5 veces seguidas): %s · el "
                                 "detector de la barra se apaga en esta sesion\n", eb.c_str());
          }
        }
      }
    }
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
      /* Los cuadros se cuentan desde el LANZAMIENTO anterior (v0.69). Antes solo se contaban con el
         hilo libre, o sea que a lo que tardaba la red se le sumaban tres cuadros de espera y salian
         ~13 mapas por segundo en vez de los 30-40 que caben. */
      if (newSource && convSalto < 1000) ++convSalto;
      /* Tope de mapas por segundo (v0.70). Hasta la v0.68 salian ~6 por segundo sin que nadie lo
         hubiera elegido: el hilo que dibuja se paraba 86 ms en cada estimacion. Al quitar esas paradas
         (v0.69) pasaron a ser 20 con video de 60 cuadros, y Rodolfo vio «una sacudida leve de lado a
         lado»: cada mapa trae algo de ruido de la red, y con el triple de mapas ese ruido deja de ser
         una deriva lenta y se vuelve temblor. MEDIDO con video real (tools/mide_temblor.py): los
         vaivenes del mapa pasan de 0,34 a 0,86 px por segundo (nave) y de 0,32 a 0,76 (globo) al ir de
         6 a 25 mapas por segundo. Se vuelve a 6 por defecto, ahora elegido y cambiable:
         ODYSSEYVK_MAPAS_SEG / `mapas_por_segundo` en serve.conf (2..60). El dibujo sigue a 120. */
      static const double kIntervaloMapas = [] {
        const char *e = entornoOdyssey("ODYSSEYVK_MAPAS_SEG");
        std::string s = e ? e : "";
        for (char &c : s)
          if (c == ',') c = '.';
        const double v = s.empty() ? 6.0 : std::atof(s.c_str());
        return 1.0 / ((v >= 2.0 && v <= 60.0) ? v : 6.0);
      }();
      const bool tocaMapa = now - ultimaEstim >= kIntervaloMapas * 0.97;   /* 3 % de holgura por el reloj */
      if (!hiloRed.ocupado && tocaMapa && ((newSource && convSalto >= cadaConv) || now - ultimaEstim > 0.5) &&
          wv.hasSource()) {
        convSalto = 0;
        ultimaEstim = now;
        std::string cerr;
        const double tLee0 = glfwGetTime();
        const bool leido = wv.readSmall(chico, &cerr);
        medLeeMs += (glfwGetTime() - tLee0) * 1000.0;
        medLeeMax = std::max(medLeeMax, (glfwGetTime() - tLee0) * 1000.0);
        medLeeN++;
        if (leido) {
          /* el tipo de video, en los primeros segundos (rama experimental): si sale dividido, se cambia
             como con el boton de la pastilla; despues no se vuelve a mirar */
          static DetectorTipo detector;
          if (opts.detectar && !detector.decidido) {
            detector.activo = true;
            int tipo = 0;
            if (detector.muestra(chico.data(), wv.converterSize(), now, tipo) && tipo != 0) {
              applyHit(ui, Hud::HitModoDividido);
              if (tipo == 4) ui.tune.topBottom = true;
            }
          }
          /* La dureza de los dos planos se puede subir o bajar con `g`/`v` en mitad de la pelicula:
             se pasa a la red justo antes de lanzar la pasada, que es el unico momento en que su hilo
             esta libre y no hay dos sitios tocando el mismo numero. */
          red.setPlanos(ui.tune.planos);
          red.setMuestras(ui.tune.muestras);
          hiloRed.arrancar(&red, chico.data(), wv.converterSize(), textoAncla, wv.converterGain(),
                           apoyoAuto ? -1.0f : wv.converterRef());
        } else if (++convFallos == 1 || convFallos % 100 == 0) {
          std::fprintf(stderr, "[conversor] no se pudo leer el cuadro pequeno (%d): %s\n", (int)convFallos,
                       cerr.c_str());
        }
      }
      float msHecho = 0.0f;
      std::string cerr, errHecho;
      std::vector<uint8_t> mascara;
      int masW = 0, masH = 0;
      float fracTexto = 0.0f;
      std::vector<uint8_t> mapaRG;
      const int estado = hiloRed.recoger(mapa, msHecho, errHecho, &mascara, &masW, &masH, &fracTexto, &mapaRG);
      if (estado == 1) {
        convMs = msHecho;
        g_mapaMs = msHecho;
        const double tSube0 = glfwGetTime();
        const bool subido = mapaRG.size() == mapa.size() * 4
                                ? wv.setDepthMapGuia(mapaRG.data(), red.size(), red.size(), &cerr)
                                : mapaRG.size() == mapa.size() * 2
                                      ? wv.setDepthMapRG(mapaRG.data(), red.size(), red.size(), &cerr)
                                      : wv.setDepthMap(mapa.data(), red.size(), red.size(), &cerr);
        medSubeMs += (glfwGetTime() - tSube0) * 1000.0;
        medSubeMax = std::max(medSubeMax, (glfwGetTime() - tSube0) * 1000.0);
        medSubeN++;
        tirRedMs += msHecho;
        tirRedN++;
        if (subido) {
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
                /* y la MISMA trama, del mismo cuadro, con el releve apagado del todo. Sin esa
                   segunda imagen no hay forma de separar una franja que pone la trama de una que
                   pone el mapa: en el panel las dos se ven iguales. Se apaga y se vuelve a dejar
                   como estaba, sin tocar el panel (es un cuadro fuera de pantalla). */
                const float gan = wv.converterGain(), ref = wv.converterRef();
                wv.setConverter(gan, ref, false);
                std::vector<uint8_t> plano;
                if (wv.grab(plano, &cerr)) {
                  const std::string pp = opts.dumpPath + "-trama-sin-releve.png";
                  if (glPngSave(pp, fbW, fbH, plano.data()))
                    std::printf("[volcado] %s (%dx%d el mismo cuadro sin releve, para comparar)\n",
                                pp.c_str(), fbW, fbH);
                  else
                    std::fprintf(stderr, "[volcado] no se pudo escribir %s\n", pp.c_str());
                } else
                  std::fprintf(stderr, "[volcado] trama sin releve: %s\n", cerr.c_str());
                wv.setConverter(gan, ref, true);
              } else
                std::fprintf(stderr, "[volcado] trama con mapa: %s\n", cerr.c_str());
            }
          }
          /* cuanto del mapa se apoyo: si el numero sube de la cuenta esta comiendo textura, y con
             --volcado guardamos la rejilla para ver exactamente que consider6 texto */
          static float fracAnterior = -1.0f;
          if (textoAncla && (convHechas == 0 || std::fabs(fracTexto - fracAnterior) > 0.03f)) {
            fracAnterior = fracTexto;
            std::printf("[rotulos] apoyados en la pantalla: %.1f%% del mapa %d x %d\n", fracTexto * 100.0f,
                        masW, masH);
            if (!opts.dumpPath.empty() && masW > 0 && masH > 0) {
              const std::string mp2 = opts.dumpPath + "-rotulos.pgm";
              FILE *mf = std::fopen(mp2.c_str(), "wb");
              if (mf) {
                const int es = 8;
                std::fprintf(mf, "P5\n%d %d\n255\n", masW * es, masH * es);
                std::vector<uint8_t> fila((size_t)masW * es);
                for (int yy = 0; yy < masH; yy++) {
                  for (int xx = 0; xx < masW; xx++)
                    for (int k = 0; k < es; k++)
                      fila[(size_t)xx * es + k] = mascara[(size_t)yy * masW + xx];
                  std::fwrite(fila.data(), 1, fila.size(), mf);
                }
                std::fclose(mf);
                std::printf("[volcado] %s (lo que el visor considera texto)\n", mp2.c_str());
              }
            }
          }
          if (red.lastCut())
            std::printf("[conversor] cambio de plano (%ld): se reinicia la escala de profundidad y el apoyo\n",
                        red.cutCount());
          if (convHechas == 0) marcaArranque("primer mapa");
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
      /* modo rendimiento sin red (rama experimental): el relieve entre los ojos, exagerado por
         ODYSSEYVK_SIN_RED_RELIEVE (x1..x3), y los topes por ojo agrandados lo mismo */
      /* un preajuste pedido (la pastilla o la orden `preajuste` del panel): se pone en marcha y se
         guarda en serve.conf, asi el panel lo ensena y la sesion siguiente arranca con el */
      {
        Hud::State &hs = ui.hud.state();
        /* guarda los cuatro valores puestos en serve.conf: el panel y la sesion siguiente arrancan con ellos */
        auto guardaRend = [&](const char *que) {
          auto conComaV = [](double v) {
            char b[32];
            std::snprintf(b, sizeof b, "%.2f", v);
            for (char *q = b; *q; q++)
              if (*q == '.') *q = ',';
            return std::string(b);
          };
          std::string e;
          ajuste::fijarEnServeConf("exp_sin_red_mezcla", conComaV(red.mezcla()), &e);
          ajuste::fijarEnServeConf("exp_sin_red_relieve", conComaV(exGanancia), &e);
          ajuste::fijarEnServeConf("exp_sin_red_topes", conComaV(exTopes), &e);
          ajuste::fijarEnServeConf("exp_sin_red_paralaje", conComaV(exParalaje), &e);
          std::printf("[rendimiento] %s: mezcla %.0f %% · relieve x%.2f · topes x%.2f (+%.1f / -%.1f px) · paralaje x%.1f%s%s\n",
                      que, 100.0 * red.mezcla(), (double)exGanancia, (double)exTopes, 12.67 * exTopes, 9.98 * exTopes,
                      exParalaje, e.empty() ? "" : " · no se guardo: ", e.c_str());
          std::fflush(stdout);
        };
        /* el menu del rayo: lo que se movio en sus cuatro deslizadores se pone en marcha al momento y,
           con el raton suelto y quieto medio segundo, se guarda */
        static bool sembrado = false;
        static double rendCambio = -1.0;
        const float vivo[4] = {red.mezcla(), exGanancia, exTopes, (float)exParalaje};
        if (!sembrado) {
          for (int k = 0; k < 4; k++) hs.rendValor[k] = vivo[k];
          sembrado = true;
        }
        bool movido = false;
        for (int k = 0; k < 4; k++)
          if (std::fabs(hs.rendValor[k] - vivo[k]) > 1e-4f) movido = true;
        if (movido && ui.preajustePedido < 0) {
          red.setMezcla(hs.rendValor[0]);
          exGanancia = hs.rendValor[1], exTopes = hs.rendValor[2], exParalaje = hs.rendValor[3];
          rendCambio = now;
        }
        if (rendCambio >= 0.0 && ui.dragging == Hud::HitNone && now - rendCambio >= 0.5) {
          guardaRend("a mano");
          rendCambio = -1.0;
        }
        /* un preajuste pedido (el menu del rayo o la orden `preajuste`): se pone en marcha y se guarda */
        if (ui.preajustePedido >= 0 && ui.preajustePedido < 3) {
          const rendimiento::Preajuste &p = rendimiento::kPreajustes[ui.preajustePedido];
          exGanancia = p.ganancia, exTopes = p.topes, exParalaje = p.paralaje;
          red.setMezcla(p.mezcla);
          hs.rendValor[0] = p.mezcla, hs.rendValor[1] = p.ganancia, hs.rendValor[2] = p.topes, hs.rendValor[3] = p.paralaje;
          static const char *kNombre[3] = {"preajuste suave", "preajuste medio", "preajuste intenso"};
          guardaRend(kNombre[ui.preajustePedido]);
          rendCambio = -1.0;
          ui.preajustePedido = -1;
        }
      }
      const float exRel = red.sinRed() ? exGanancia : 1.0f;
      /* El alcance del relieve con la red, en Auto y en Manual (v1.12), con la distancia de los ojos (la
         del rastreo; sin camara, la pose nominal de 65,1 cm) y el paso de este panel (598 mm / 3840 px):
         el tope de fuera queda en 0,3 grados de paralaje entre los ojos, alcance = d * tan(0,3 grados) /
         (2 * 12,67 px * 0,156 mm), y nunca por debajo de los topes de fabrica (x1). Hasta 75 cm sale x1; a
         80 cm, x1,06; a 100 cm, x1,33. Antes fue 0,4 grados (x1,15 a 65 cm) y medio grado (x1,44): con eso
         Rodolfo aun veia rayas junto a los cantos y los trastes de una guitarra dobles («le bajas mas al
         rango de alcance?», 26-nov). Y primero el grado entero (la «regla del grado», la zona comoda de la vista: x2,88 a 65 cm)
         y era demasiado («alcance de relieve is the issue, you might be too aggressive with it», Rodolfo,
         26-nov): todo error del mapa salia 2,88 veces mas grande y el fantasma de la lente 2,88 veces mas
         lejos, y se veian cantos torcidos, subtitulos doblados y rectas que bailaban. El grado es lo que
         la vista aguanta, no lo que esta lente y una red de una sola imagen dan limpio. Manual tenia el
         suyo en la pestana Experimental (`exp_topes_red`), que se fue en la 1.12; ODYSSEYVK_TOPES_RED lo
         fija a mano para el taller. */
      static float tAlcance = -1.0f;
      static double tAlcanceT = -1.0;
      if (!red.sinRed()) {
        if (kTopesRedFijos) {
          tAlcance = exTopesRed;
        } else {
          const double distMm = std::max(300.0, std::min(1500.0, wv.eye().z * 10.0));
          const float tComodo = (float)std::max(1.0, std::min(4.0, distMm * std::tan(0.3 * M_PI / 180.0) /
                                                                         (2.0 * 12.67 * (598.0 / 3840.0))));
          if (tAlcance < 0.0f) tAlcance = tComodo;
          else tAlcance += (tComodo - tAlcance) * (float)std::min(1.0, (tAlcanceT < 0.0 ? 1.0 : now - tAlcanceT) / 1.0);
          tAlcanceT = now;
        }
      }
      wv.setTopeEscala(red.sinRed() ? exTopes : tAlcance);
      {
        static bool dicho = false;
        if (!red.sinRed() && kTopesRedFijos && !dicho) {
          dicho = true;
          std::printf("[conversor] alcance del relieve fijado por ODYSSEYVK_TOPES_RED: x%.2f (topes por ojo +%.1f / -%.1f px)\n",
                      (double)exTopesRed, 12.67 * exTopesRed, 9.98 * exTopesRed);
          std::fflush(stdout);
        }
      }
      {
        static bool dicho = false;
        if (red.sinRed() && !dicho) {
          dicho = true;
          std::printf("[conversor] sin red: relieve x%.2f · topes x%.2f (+%.1f / -%.1f px por ojo) · paralaje de la cabeza "
                      "x%.1f · mezcla %.0f %%\n",
                      (double)exGanancia, (double)exTopes, 12.67 * exTopes, 9.98 * exTopes, exParalaje, 100.0 * red.mezcla());
          std::fflush(stdout);
        }
      }
      /* sin red (modo rendimiento) la base es la de los preajustes medidos: ni la Salida ni el `relieve`
         global cuentan, y el plano de la pantalla es el automatico (rendimiento.h) */
      float ganancia = (red.sinRed() ? rendimiento::kGananciaBase : gananciaRelieve(st.popout)) * exRel;
      /* la Salida automatica (rama experimental): en Auto, con el plano de la saliencia en el sujeto, la
         mayor ganancia que deja como mucho un 2 % del mapa pegado a cada tope por ojo (cuantiles 2 y
         98 % del mapa). MEDIDO 26-nov (5 cuadros, con ruido y con barrido): con los topes de fabrica
         queda un 0,9 % pegado y ~28 px de relieve entre los ojos (la Salida 50 con `relieve = 2,00`:
         61 % pegado); con el alcance x2,75, ~77 px y 0,9 % (la fija: 66 px y 31 %). Varia un 1,2 % con el
         ruido y salta como mucho un 2,6 % en un barrido; se suaviza (abajo) y salta en los cortes.
         Tope: lo que daria la Salida 50 con `relieve = 2,00` y el mismo alcance, para que una escena
         casi plana no infle el ruido de la red. */
      if (!red.sinRed() && st.focoVisible && apoyoAuto) {
        static float gAuto = -1.0f;
        static double tAuto = -1.0;
        /* el alcance (arriba): medio grado de paralaje a la distancia de los ojos */
        ui.hud.state().focoAlcance = (int)std::lround(tAlcance * 100.0f);
        const float T = tAlcance, p = red.focusPlane();
        const float lo = red.cuantilBajo(), hi = red.cuantilAlto();
        float g = std::min(0.0033f * T / std::max(hi - p, 0.02f), 0.0026f * T / std::max(p - lo, 0.02f));
        g = std::max(gananciaRelieve(0) * 0.5f, std::min(g, (0.00104f + 0.5f * 0.00972f) * 2.0f * T));
        const double dt = tAuto < 0.0 ? 1.0 : now - tAuto;
        tAuto = now;
        /* baja en ~0,2 s (la escena se hizo mas honda: que no se aplaste) y sube en ~2 s (que no bombee).
           MEDIDO con tres videos de YouTube: frente a un suavizado parejo de 0,6 s, lo pegado al tope baja
           del 2,2-3,8 % al 1,4-1,5 % (y en los peores cuadros, del 4-15 % al 2,4-6,2 %), con un 3-8 % menos
           de relieve y el mismo cambio por cuadro */
        if (gAuto < 0.0f || red.lastCut()) gAuto = g;
        else gAuto += (g - gAuto) * (float)std::min(1.0, dt / (g < gAuto ? 0.2 : 2.0));
        ganancia = gAuto;
        /* lo que ensena el menu: el plano y la Salida equivalente del deslizador */
        const float esc = gananciaRelieve(0) / 0.00104f;
        ui.hud.state().focoPlano = (int)std::lround(p * 100.0f);
        ui.hud.state().focoSalida = (int)std::lround(((gAuto / esc) - 0.00104f) / 0.00972f * 100.0f);
        /* en el menu se ensena el relieve entre los ojos, no la Salida: la automatica puede pedir mas de lo
           que da el deslizador (en una pelicula, el equivalente de Salida 225) */
        ui.hud.state().focoRelieve =
            (int)std::lround(2.0 * std::min((double)(gAuto * 3840.0f * (hi - lo)), (12.67 + 9.98) * (double)T));
        static double dicho = -10.0;
        if (now - dicho >= 5.0) {
          dicho = now;
          std::printf("[foco] auto: plano %.2f · salida %.1f px por unidad (Salida %d) · cuantiles %.2f..%.2f · alcance x%.2f "
                      "(ojos a %.0f cm: +%.1f / -%.1f px por ojo)\n",
                      (double)p, (double)gAuto * 3840.0, ui.hud.state().focoSalida, (double)lo, (double)hi, (double)T,
                      wv.eye().z, 12.67 * T, 9.98 * T);
          std::fflush(stdout);
        }
      }
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
      const float planoTrama = (apoyoAuto || red.sinRed()) ? red.focusPlane() : (float)st.depth / 100.0f;
      red.setPlanoPantalla(planoTrama);   /* ahi se dejan las bandas negras de las peliculas */
      wv.setConverter(ganancia, planoTrama, wv.hasDepthMap());
      wv.setSujeto(red.sujeto());   /* el realce de detalle va al sujeto, este donde este el plano (v0.68) */
    }
    /* Apagado el conversor, la trama tiene que enterarse (v0.80). `setConverter` solo se llamaba
       con el conversor encendido, asi que al apagarlo (tecla C, o el boton de video dividido de la
       pastilla) la trama se quedaba con el ultimo mapa de profundidad puesto: el registro decia
       "apagado" y en pantalla seguia el video normal con relieve, no las dos mitades. MEDIDO fuera
       del panel: tras el boton, la trama se parecia a la del conversor (17,98 % de pixeles distintos
       de la de lado a lado; el conversor, 18,52 %). */
    if (!convActivo && wv.converterOn() && opts.mapaFijo.empty())
      wv.setConverter(wv.converterGain(), wv.converterRef(), false);
    /* Con el mapa venido de un archivo (--mapa-fijo) no hay red que mande el relieve, y sin relieve
       el mapa no mueve nada: se manda por el mismo camino de siempre, con el apoyo que diga --apoyo
       (por eso esa bandera pide el apoyo escrito: no hay red que lo saque de la escena). */
    if (!opts.mapaFijo.empty() && wv.hasDepthMap())
      wv.setConverter(gananciaRelieve(st.popout), (float)(opts.apoyo >= 0.0 ? opts.apoyo : 0.5), true);
#endif
    ui.hud.state().hud = rig.hud();
    /* la frase extra del video se quita sola, salvo si estamos en pausa a proposito */
    if (!ui.extraInfo.empty() && ui.extraHasta > 0.0 && now >= ui.extraHasta) {
      ui.extraInfo.clear();
      ui.extraHasta = 0.0;
    }
    ui.hud.setInfo(infoLine(ui.hud.state(), ui.extraInfo));
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
    /* la fila de Facetracking solo existe en el visor: el hub no rastrea. No es un
       boton: dice con que camara se esta siguiendo la cara y que esta viendo. */
    /* los botones de icono: cual de los dos tipos de 3D esta puesto de verdad (lo dice el conversor) */
    ui.hud.state().modoVisible = (kind == SourceKind::Capture) && !ui.hud.state().legado;
    ui.hud.state().modo3d = ui.convInfo ? (wv.mediaFuente() ? 3 : 1) : 2;
    /* la pastilla de rendimiento: con el conversor puesto y sin red */
    ui.hud.state().rendimiento = convActivo && red.sinRed();
    if (!ui.hud.state().rendimiento) ui.hud.state().menuRendimiento = false;
    /* el cursor de los juegos (rama experimental): el gestor de ventanas se lee cada cuadro (si no, sus
       mensajes se amontonan en el enchufe); al entrar al 3D de un juego la pastilla se ensena unos
       segundos, para que se vean los atajos, y se va sola sin tocar el raton; y tras quitarle el cursor
       al juego, el registro dice si KWin hizo caso */
    {
      static double desde = -1.0;
      if (desde < 0.0) desde = now;
#ifdef HAVE_WAYLAND_WM
      if (ui.wmAbierto) ui.wm.waitAndDispatch(0);
#endif
      if (!ui.presentacionHecha && now - desde >= 6.0) {
        const bool juego = sobreJuego(ui);
        if (juego && ui.hud.overlayVisible() && !ui.robando && !ui.hud.state().menuRendimiento) {
          cambioCapa(ui, false, "se ensenaron los atajos al entrar al juego");
        } else if (ui.esJuego >= 0 || now - desde >= 12.0) {
          ui.presentacionHecha = true;
        }
      }
#ifdef HAVE_WAYLAND_WM
      if (ui.robando && ui.robadoMirar > 0.0 && now >= ui.robadoMirar) {
        ui.robadoMirar = -1.0;
        bool visorActivo = false, juegoActivo = false;
        for (const WindowInfo &w : ui.wm.windows()) {
          if (w.pid == (uint32_t)::getpid() && w.active()) visorActivo = true;
          if (w.uuid == ui.devolverA && w.active()) juegoActivo = true;
        }
        std::printf("[raton] %s\n", visorActivo ? "el visor es la ventana activa: el juego solto el cursor"
                                    : juegoActivo ? "KWin no hizo caso: el juego sigue siendo la ventana activa"
                                                  : "el juego ya no es la ventana activa");
        std::fflush(stdout);
      }
#endif
    }
    ui.hud.state().preajuste = rendimiento::cual(red.mezcla(), exGanancia, exTopes, (float)exParalaje);
    /* Profundidad y Salida por modo (v1.10). De fabrica: el conversor y el dividido suavizado
       arrancan en Profundidad 75 (plano fijo en 0,75, lo que se ve al moverla a 75) y Salida 50; el
       dividido con su propio relieve, neutro (50 y 0). En cuanto se mueve uno de los dos y se deja
       quieto un segundo, se guarda ESE valor para ESE modo en serve.conf (`profundidad_conv`,
       `salida_conv`, `_suave`, `_sbs`) y desde entonces se arranca con el. `profundidad_conv = auto`
       vuelve al plano automatico (la saliencia elige el plano de cada escena). No se toca con los
       valores forzados por la linea de mando ni en el modo de legado. */
    static const bool valoresEnTaller = entornoOdyssey("ODYSSEYVK_VALORES") != nullptr;   /* para probarlo sin video */
    ui.hud.state().focoVisible = false;
    ui.hud.state().focoAuto = apoyoAuto;
    if (opts.apoyo < 0.0 && opts.relieve < 0 && !ui.hud.state().legado &&
        (kind == SourceKind::Capture || valoresEnTaller)) {
      static int modoValores = -1;
      static int baseDepth = 0, basePop = 0;
      static double cambioDepth = -1.0, cambioPop = -1.0;
      static std::map<std::string, std::string> conf = ajuste::leerServeConf();
      Hud::State &hs = ui.hud.state();
      const int modo = hs.modo3d;
      static const char *kSufijo[] = {"", "conv", "sbs", "suave"};
      const std::string suf = (modo >= 1 && modo <= 3) ? kSufijo[modo] : "";
      if (!suf.empty() && modo != modoValores) {
        modoValores = modo;
        const bool conConversor = modo != 2;
        auto valor = [&](const std::string &k) {
          const auto it = conf.find(k);
          return it == conf.end() ? std::string() : it->second;
        };
        const std::string vd = valor("profundidad_" + suf), vp = valor("salida_" + suf);
        if (conConversor && (vd == "auto" || vd == "automatico")) {
          apoyoAuto = true;
          hs.depth = 55;
        } else {
          hs.depth = vd.empty() ? (conConversor ? 75 : 50) : std::max(0, std::min(100, std::atoi(vd.c_str())));
          if (conConversor) apoyoAuto = false;
        }
        hs.popout = vp.empty() ? (conConversor ? 50 : 0) : std::max(-50, std::min(50, std::atoi(vp.c_str())));
        profundidadVista = hs.depth;
        baseDepth = hs.depth, basePop = hs.popout;
        cambioDepth = cambioPop = -1.0;
        std::printf("[valores] %s: profundidad %d%s · salida %d (%s)\n", suf.c_str(), hs.depth,
                    conConversor ? (apoyoAuto ? " (plano automatico)" : " (plano fijo)") : "", hs.popout,
                    vd.empty() && vp.empty() ? "de fabrica" : "recordados");
        std::fflush(stdout);
      } else if (!suf.empty() && !ui.modoPedido && !ui.convWant && !ui.convToggle && !ui.convFuera) {
        /* lo movio el usuario: se espera a que lo deje quieto un segundo y se guarda solo ese */
        auto vigila = [&](int ahora, int &base, double &desde, const char *clave) {
          if (ahora == base) { desde = -1.0; return; }
          static std::map<std::string, int> visto;
          const std::string k = std::string(clave) + "_" + suf;
          if (desde < 0.0 || visto[k] != ahora) { desde = now; visto[k] = ahora; return; }
          if (now - desde < 1.0) return;
          std::string e;
          if (ajuste::fijarEnServeConf(k, std::to_string(ahora), &e)) {
            conf[k] = std::to_string(ahora);
            std::printf("[valores] %s = %d recordado\n", k.c_str(), ahora);
          } else {
            std::fprintf(stderr, "[valores] no se pudo guardar %s: %s\n", k.c_str(), e.c_str());
          }
          std::fflush(stdout);
          base = ahora;
          desde = -1.0;
        };
        vigila(hs.depth, baseDepth, cambioDepth, "profundidad");
        vigila(hs.popout, basePop, cambioPop, "salida");
      }
      /* el Foco automatico (rama experimental): el chip «AI» de la pastilla, con el conversor y la red.
         Puesto, el plano de la pantalla es el automatico con la saliencia y se guarda
         `profundidad_<modo> = auto`; quitado, la Profundidad vuelve con el valor donde estaba el sujeto en
         ese momento (la imagen no salta) y se guarda ese numero. */
      const bool conConv = modo == 1 || modo == 3;
      hs.focoVisible = !suf.empty() && conConv && convActivo && !red.sinRed();
      /* (v1.12) el menu del boton Auto/Manual vale tambien sin conversor (solo Manual): se cierra en Rendimiento, que tiene el suyo */
      if (convActivo && red.sinRed()) hs.menuNormal = false;
      if (ui.focoPedido) {
        const bool quiereAuto = ui.focoPedido == 1;
        ui.focoPedido = 0;
        if (hs.focoVisible && quiereAuto != apoyoAuto) {
          const std::string k = "profundidad_" + suf;
          std::string e, valor;
          if (!apoyoAuto) {
            apoyoAuto = true;
            hs.depth = 55;
            valor = "auto";
            if (!red.saliencia()) {
              red.setSaliencia(true);
              ajuste::fijarEnServeConf("saliencia", "si", &e);
            }
          } else {
            /* a Manual: la Profundidad donde estaba el sujeto y la Salida que se estaba calculando (si
               cabe en el deslizador), para que la imagen no salte */
            apoyoAuto = false;
            hs.depth = std::max(0, std::min(100, (int)std::lround(red.focusPlane() * 100.0f)));
            valor = std::to_string(hs.depth);
            hs.popout = std::max(-50, std::min(50, hs.focoSalida));
            basePop = hs.popout;
            cambioPop = -1.0;
            std::string e2;
            if (ajuste::fijarEnServeConf("salida_" + suf, std::to_string(hs.popout), &e2)) conf["salida_" + suf] = std::to_string(hs.popout);
          }
          profundidadVista = hs.depth;
          baseDepth = hs.depth;
          cambioDepth = -1.0;
          const bool guardado = ajuste::fijarEnServeConf(k, valor, &e);
          if (guardado) conf[k] = valor;
          std::printf("[foco] %s · %s = %s%s%s\n", apoyoAuto ? "Auto: el plano lo elige la saliencia y la salida se calcula"
                                                            : "Manual: la Profundidad vuelve donde estaba el sujeto y la Salida la que se calculaba",
                      k.c_str(), valor.c_str(), guardado ? "" : " (no se guardo: ", guardado ? "" : (e + ")").c_str());
          std::fflush(stdout);
          ui.inputChanged = true;
        }
      }
      hs.focoAuto = apoyoAuto;
    }
    ui.hud.state().controlOjo = opts.track;   /* desde v0.78 solo dice si se PIDIO rastreo: la fila ya no se pinta */
#ifdef HAVE_OPENCV
    {
      Hud::State &hs = ui.hud.state();
      const TrackFeedState fs = feed.state();
      hs.rastreoVivo = rastreoVivo;
      hs.rastreoEstudio = fs.esMonitor;
      hs.rastreoCara = fs.cara;
      hs.rastreoCamara = fs.nombre.empty() ? fs.camera : fs.nombre;
      if (!rastreoVivo)
        hs.rastreoNota = "camara sin abrir: la trama va con la pose de fabrica";
      else if (!fs.note.empty())
        hs.rastreoNota = fs.note;
      else
        hs.rastreoNota = fs.mode + (fs.cara ? " · cara a la vista" : " · sin cara");
    }
#else
    ui.hud.state().rastreoVivo = false;
    ui.hud.state().rastreoNota = "sin rastreo (compilado sin OpenCV)";
#endif
    ui.hud.begin(fbW, fbH);   /* escala por pixeles fisicos: ver el comentario del lienzo arriba */
    /* la pastilla cambio de sitio o se oculto: la caja que deja pasar el raton se actualiza aqui */
    /* quitandole el cursor a un juego, el visor se queda con todo el raton */
    aplicarRegionEntrada(win, &ui.region, &ui, !opts.noPassthrough && !ui.robando);
    wv.setHud(ui.hud.overlayVisible() ? ui.hud.vertices() : std::vector<HudVertex>{});
    wv.setHudState(rig.hud());

    /* ---- la pose del ojo que ve la trama --------------------------------------
       La base es la nominal del arranque; con el rastreo puesto (boton de la
       pastilla o tecla T) la camara la va moviendo, y si se aparta vuelve la
       nominal. Nada mas: el angulo que se podia girar a mano con el deslizador se
       quito en v0.34 porque hacia duplicado con lo que mide la camara. */
    if (!ui.ojoBasePuesto) {
      ui.ojoBasePuesto = true;
      ui.ojoBase = EyePose{opts.eyeX, opts.eyeY, opts.eyeZ};
    }
    bool fresco = false;
#ifdef HAVE_OPENCV
    /* y si al arrancar la sesion la camara estaba ocupada, se reintenta. Quien suele tenerla es la
       vista previa de la ventana de ajustes (`ctl vista`), que al ver que esta sesion quiere la camara
       la suelta en medio segundo: asi el rastreo se suma a media sesion en vez de quedarse fuera toda
       la sesion. Pocos intentos y espaciados, para no machacar el USB si de verdad hay otra cosa
       usando la camara. */
    if (opts.track && !rastreoVivo && intentosCamara < 6 && now - ultimoIntentoCamara >= 2.0) {
      ultimoIntentoCamara = now;
      intentosCamara++;
      std::string trk;
      if (feed.start(fo, &trk)) {
        rastreoVivo = true;
        const TrackFeedState arr = feed.state();
        std::printf("[rastreo] la camara estaba ocupada (la vista previa de la ventana de ajustes?): "
                    "ya abrio, %s en %s\n", arr.camera.c_str(), arr.mode.c_str());
        std::fflush(stdout);
      } else if (intentosCamara == 6) {
        std::fprintf(stderr, "[rastreo] la camara sigue ocupada tras %d intentos: %s\n", intentosCamara,
                     trk.c_str());
        std::fflush(stderr);
      }
    }
    if (rastreoVivo) {
      const TrackFeedState fs = feed.state();
      const double altoCm = (double)fbH * wv.field().pxPcm;
      const bool dentro = fs.sample.panelCentimetres[1] >= -50.0 && fs.sample.panelCentimetres[1] <= 50.0 &&
                          fs.sample.panelCentimetres[2] >= 50.0 && fs.sample.panelCentimetres[2] <= 130.0;
      fresco = fs.seen && fs.age >= 0.0 && fs.age <= 0.15 && dentro;
      if (opts.legado) {
        if (fresco) ultimoPoseFresca = now;
        const double sinCara = ultimoPoseFresca < 0.0 ? 1e9 : now - ultimoPoseFresca;
        const bool corte = sinCara > 0.5;   /* medio segundo sin cara: el video se ve tal cual */
        if (corte != tramaCortada) {
          tramaCortada = corte;
          std::printf("[legado] %s\n", corte ? "la camara ya no ve la cara: el video se ve tal cual, sin trama"
                                             : "cara recuperada: vuelve la trama");
          std::fflush(stdout);
        }
        /* la lente se apaga tres cuartos de segundo despues (un poco despues que la trama, para que
           no se note el salto) y se vuelve a poner en cuanto la cara esta otra vez a la vista */
        if (lenteViva && sinCara > 0.75 && rig.lensOk) {
          if (rig.lens.off()) {
            lenteViva = false;
            std::printf("[legado] lente apagada hasta volver a ver la cara\n");
            std::fflush(stdout);
          }
        } else if (!lenteViva && fresco && rig.lensOk) {
          if (rig.lens.on()) {
            lenteViva = true;
            std::printf("[legado] lente encendida de nuevo\n");
            std::fflush(stdout);
          }
        }
      }
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
        /* Anticipacion de la cabeza (rama experimental). El tejedor de Leia calcula la trama para donde
           estaran los ojos cuando el cuadro llegue al panel; Any2Odyssey lo probo a ojo y le fue mejor
           con 25 ms a 120 Hz. Aqui: la velocidad de la pose (media movil sobre las muestras aceptadas)
           por ODYSSEYVK_ANTICIPACION milisegundos, topada a 3 cm. 0 = apagado (lo de siempre). */
        {
          static const double kAnticipacion = [] {
            const char *a = entornoOdyssey("ODYSSEYVK_ANTICIPACION");
            std::string t = a ? a : "";
            for (char &c : t)
              if (c == ',') c = '.';
            const double v = t.empty() ? 0.0 : std::atof(t.c_str());
            return (v > 0.0 && v <= 60.0) ? v : 0.0;
          }();
          static EyePose antes{};
          static double tAntes = -1.0, vx = 0.0, vy = 0.0, vz = 0.0;
          static long avisos = 0;
          if (kAnticipacion > 0.0) {
            if (tAntes >= 0.0) {
              const double dt = now - tAntes;
              if (dt > 0.001 && dt < 0.3) {
                const double a = 0.5;   /* media movil corta: el temblor del rastreo no se anticipa */
                vx += ((e.x - antes.x) / dt - vx) * a;
                vy += ((e.y - antes.y) / dt - vy) * a;
                vz += ((e.z - antes.z) / dt - vz) * a;
              } else if (dt >= 0.3) {
                vx = vy = vz = 0.0;   /* un hueco largo sin cara: la velocidad vieja no vale */
              }
            }
            antes = e;
            tAntes = now;
            const double sAnt = kAnticipacion / 1000.0;
            auto tope = [](double d) { return d < -3.0 ? -3.0 : (d > 3.0 ? 3.0 : d); };
            e.x += tope(vx * sAnt);
            e.y += tope(vy * sAnt);
            e.z += tope(vz * sAnt);
            if (avisos++ == 0) {
              std::printf("[rastreo] anticipacion de %.0f ms (rama experimental)\n", kAnticipacion);
              std::fflush(stdout);
            }
          }
        }
        ui.ojoBase = e;
      }
      /* Paralaje de movimiento (v0.67): al mover la cabeza, lo cercano se desliza respecto a lo lejano.
         Es la senal de profundidad mas fuerte que hay despues de la de los dos ojos, y hasta ahora el
         rastreo solo servia para apuntar cada vista a su ojo: el contenido no cambiaba.
         - Se mide contra una posicion «de reposo» que sigue a la cabeza muy despacio (6 s): asi da
           igual donde te sientes, y si te quedas quieto en otro sitio la imagen vuelve a centrarse.
         - Una media distancia entre ojos (3,25 cm) de movimiento es una vista entera de mas; se topa
           a +-1 para que el estirado de los cantos no pase del doble.
         - Un paso bajo corto (60 ms) encima del One-Euro: el temblor que quede del rastreo movería la
           imagen entera (0,17 cm de temblor serian 0,05 vistas = 0,6 px en lo mas cercano).
         ODYSSEYVK_PARALAJE / `paralaje` en serve.conf: ganancia (0 = apagado, 1 = geometria real,
         negativo = al reves por si el eje de la camara estuviera invertido). */
      {
        static const double kParalaje = [] {
          const char *e = entornoOdyssey("ODYSSEYVK_PARALAJE");
          std::string s = e ? e : "";
          for (char &c : s)
            if (c == ',') c = '.';
          const double v = s.empty() ? 0.0 : std::atof(s.c_str());
          return (v >= -2.0 && v <= 2.0) ? v : 0.0;
        }();
        static double xReposo = 0.0, hSuave = 0.0, tParalaje = 0.0;
        static bool paralajeListo = false;
        if (kParalaje != 0.0) {
          double dtp = paralajeListo ? now - tParalaje : 0.0;
          dtp = dtp < 0.0 ? 0.0 : (dtp > 0.25 ? 0.25 : dtp);
          tParalaje = now;
          double objetivo = 0.0;   /* sin cara a la vista vuelve a cero despacio */
          if (fresco && !opts.trackFixed) {
            const double x = ui.ojoBase.x;
            if (!paralajeListo) xReposo = x;
            xReposo += (x - xReposo) * (1.0 - std::exp(-dtp / 6.0));
            /* Zona muerta suave (v0.72). Sentado y quieto, la cabeza (y su medida) deriva de 1 a 4 mm
               por segundo (medido en el registro de nueve sesiones: mediana de 0,08 a 0,44 cm entre
               muestras de un segundo). Sin zona muerta eso movia lo cercano hasta medio pixel contra
               el fondo, sin ritmo: «una sacudida leve de lado a lado, no pareja» (Rodolfo, 25-nov).
               El factor d^2 / (d^2 + z^2) deja en nada lo de milimetros (0,3 cm -> 20 %) y casi entero
               un movimiento de verdad (3 cm -> 96 %), sin escalon. */
            const double d = x - xReposo, zona = 0.6;
            /* modo rendimiento sin red (rama experimental): el paralaje de la cabeza, exagerado por
               ODYSSEYVK_SIN_RED_PARALAJE (x1..x4); el tope de una vista sube con el, hasta dos */
            const double ex = (convActivo && red.sinRed()) ? exParalaje : 1.0;
            const double topeVistas = std::min(2.0, ex);
            objetivo = d * (d * d / (d * d + zona * zona)) / 3.25 * kParalaje * ex;
            objetivo = objetivo < -topeVistas ? -topeVistas : (objetivo > topeVistas ? topeVistas : objetivo);
          }
          paralajeListo = true;
          /* paso bajo de 120 ms (antes 60): el paralaje es una ayuda, importa mas que vaya liso */
          hSuave += (objetivo - hSuave) * (1.0 - std::exp(-dtp / 0.12));
          wv.setParalaje((float)hSuave);
        }
      }
      if (now - ultimoRastreo >= 1.0) {
        ultimoRastreo = now;
        const TrackSample &m = fs.sample;
        std::printf("[rastreo] camara (%6.1f, %6.1f, %6.1f) mm · panel (%6.2f, %6.2f, %6.2f) cm · ipd %.1f mm · "
                    "%lld/%lld cuadros (%.1f fps) · error %.2f px · hace %.2f s · %s · fino %lld (%.2f/%.2f)\n",
                    m.eyeMillimetres[0], m.eyeMillimetres[1], m.eyeMillimetres[2], m.panelCentimetres[0],
                    m.panelCentimetres[1], m.panelCentimetres[2], m.ipdMillimetres, fs.frames, fs.good,
                    fs.cameraFps, m.reprojectionPx, fs.age,
                    fresco ? "siguiendo" : (fs.note.empty() ? "sin pose valida" : fs.note.c_str()), fs.finos,
                    m.puntajeFranja, m.puntajeDisparidad);
        std::fflush(stdout);
      }
      /* la vista de la camara: el cuadro crudo tal cual lo soltio, para que la ventana de ajustes la
         ensene sin abrir el dispositivo (la camara del monitor no se puede tener abierta dos veces).
         La cadencia depende de lo que pida la ventana: una cada dos segundos por defecto, y ocho por
         segundo mientras la casilla de Previsualizacion esta marcada (`vista = 1`), que es cuando esa
         imagen se esta mirando de verdad. Si el rastreo no llego a abrir la camara, no hay nada que
         volcar y la ventana lo dice con lo que hay en `estado`. */
      if (now - ultimoCuadroVisto >= (g_vistaRapida ? 0.12 : 2.0)) {
        ultimoCuadroVisto = now;
        std::vector<uint8_t> crudo;
        if (feed.ultimoCuadro(&crudo)) {
          std::string err;
          if (!ajuste::escribirCamaraVista(crudo, &err))
            std::fprintf(stderr, "[camara] %s\n", err.c_str());
        }
      }
    }
#endif
    {
      const EyePose puesto = ui.ojoBase;
      if (std::fabs(puesto.x - wv.eye().x) > 1e-4 || std::fabs(puesto.y - wv.eye().y) > 1e-4 ||
          std::fabs(puesto.z - wv.eye().z) > 1e-4)
        wv.setEye(puesto);
    }

    ultimoPresente = now;
    ui.inputChanged = false;
    const double tirPresenta0 = glfwGetTime();
    {
      const double periodo = 1.0 / (hz > 0 ? (double)hz : 120.0);
      const double trabajo = tirPresenta0 - tirTrabajo0;
      tirTrabajoMax = std::max(tirTrabajoMax, trabajo);
      if (trabajo > periodo) tirTrabajoLargos++;
    }
    const bool frameBien = wv.frame(&err);
    /* (v1.12) la linea del arranque: en el primer cuadro con relieve (o con imagen, sin conversor) */
    {
      static bool arranqueDicho = false;
      if (!arranqueDicho && frameBien && (convActivo ? convHechas >= 1 : lastFrameCount > 0)) {
        arranqueDicho = true;
        marcaArranque("primer cuadro en 3D");
        std::printf("[arranque] %s\n", resumenArranque().c_str());
        std::fflush(stdout);
      }
    }
    {   /* cuanto espero la presentacion a la tarjeta y al refresco, y el hueco entre presentaciones */
      const double t1 = glfwGetTime(), periodo = 1.0 / (hz > 0 ? (double)hz : 120.0);
      tirPresentaMax = std::max(tirPresentaMax, t1 - tirPresenta0);
      if (t1 - tirPresenta0 > 2.0 * periodo) tirPresentaLargas++;
      if (tirPresentaUlt > 0.0) tirPresentaHueco = std::max(tirPresentaHueco, t1 - tirPresentaUlt);
      tirPresentaUlt = t1;
    }
    if (!frameBien) {
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
    /* ODYSSEYVK_VOLCADO_CUADRO=N (taller, v0.80): el volcado espera al cuadro N en vez del 4, para
       poder sacar la trama DESPUES de un cambio hecho en marcha (ODYSSEY_GOLPE) y compararla. */
    static const int cuadroVolcado = [] {
      const char *e = std::getenv("ODYSSEYVK_VOLCADO_CUADRO");
      return (e && std::atoi(e) > 4) ? std::atoi(e) : 4;
    }();
    if (!opts.dumpPath.empty() && !dumpHecho &&
        ((fuenteLista && frames >= cuadroVolcado) || frames >= std::max(150, cuadroVolcado + 146))) {
      std::vector<uint8_t> rgba;
      if (wv.grab(rgba, &err)) {
        std::string a = opts.dumpPath + "-trama-vulkan.png";
        if (glPngSave(a, fbW, fbH, rgba.data()))
          std::printf("[volcado] %s (%dx%d leido de Vulkan)\n", a.c_str(), fbW, fbH);
        else
          std::fprintf(stderr, "[volcado] no se pudo escribir %s\n", a.c_str());
        /* Sonda del cuadro que sale de Vulkan: media y que fraction de puntos esta a +-1,5 de la
           media. Si casi todo el cuadro esta en un solo color, el visor esta pintando una lama
           pareja (el "fondo gris, nada de imagen") y con la copia SIN tramar del mismo cuadro se
           ve si ese color venia de la imagen que entra o de los calculos de la trama. */
        const auto sonda = [](const std::vector<uint8_t> &p) {
          const long total = (long)p.size() / 4;
          double s = 0.0;
          long n = 0;
          for (long q = 0; q < total; q += 7) {
            s += ((double)p[(size_t)q * 4] + p[(size_t)q * 4 + 1] + p[(size_t)q * 4 + 2]) / 3.0;
            n++;
          }
          const double m = n ? s / (double)n : 0.0;
          long pareja = 0;
          for (long q = 0; q < total; q += 7) {
            const double l = ((double)p[(size_t)q * 4] + p[(size_t)q * 4 + 1] + p[(size_t)q * 4 + 2]) / 3.0;
            if (std::fabs(l - m) <= 1.5) pareja++;
          }
          return std::make_pair(m, n ? 100.0 * (double)pareja / (double)n : 0.0);
        };
        const auto trama = sonda(rgba);
        std::vector<uint8_t> cruda;
        std::string errCruda;
        if (wv.grab(cruda, &errCruda, true)) {
          const std::string b = opts.dumpPath + "-trama-cruda.png";
          if (glPngSave(b, fbW, fbH, cruda.data())) {
            const auto copia = sonda(cruda);
            std::printf("[volcado] %s (la misma imagen sin tramar, para comparar)\n", b.c_str());
            std::printf("[sonda] trama media %.1f · parejo %.1f%%   copia media %.1f · parejo %.1f%%\n",
                        trama.first, trama.second, copia.first, copia.second);
          }
        } else {
          std::printf("[sonda] trama media %.1f · parejo %.1f%%   sin copia (%s)\n", trama.first,
                      trama.second, errCruda.c_str());
        }
        if (trama.second > 90.0)
          std::fprintf(stderr,
                       "[aviso] el tramado pintó una imagen plana: %.0f%% de los puntos estan en "
                       "%.1f (de 0 a 255) · cuadro %dx%d · miren %s y el volcado sin tramar\n",
                       trama.second, trama.first, fbW, fbH, a.c_str());
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
         el 3D en el panel y sale al cumplirse el tiempo. Con --volcado-seguir pasa lo mismo
         para siempre: es lo que usa el hub cuando `volcado` esta en serve.conf, para poder
         mirar la pantalla mientras se guardan la trama y el mapa de profundidad. */
      if (opts.segundos <= 0 && !opts.dumpKeep) ui.quit = true;
    }
#endif
    frames++;
    {
      /* cuadros que tardan mas de un refresco y medio: cada uno es un refresco perdido */
      const double ahoraM = glfwGetTime();
      if (medUltimo > 0.0 && ahoraM - medUltimo > 1.5 / (hz > 0 ? (double)hz : 120.0)) medLentos++;
      medUltimo = ahoraM;
    }
    if (now - clock > 5.0) {
      g_fpsVisto = frames / (now - clock);
      /* la linea de siempre y, detras, lo de los tirones (ver tirFuente*): fuente = cuadros nuevos
         del juego o del video por segundo y su hueco mas largo; vuelta = el trabajo del visor antes
         de presentar (si pasa de un refresco, el visor pierde su turno); presentar = lo que espera
         a la tarjeta y al refresco; leer/subir = la copia chica para la red y el mapa de vuelta */
      {
        const double seg = now - clock;
        const double periodoMs = 1000.0 / (hz > 0 ? (double)hz : 120.0);
        std::printf("[fps] %.1f · fuente %.1f/s hueco max %.0f ms (%lld de mas de 50) · vuelta max %.1f ms (%lld de "
                    "mas de %.1f) · presentar max %.1f ms (%lld de mas de %.1f), hueco max %.0f ms · leer %.1f/%.1f ms · "
                    "subir %.1f/%.1f ms · red %.0f ms\n",
                    g_fpsVisto, seg > 0.0 ? (double)tirFuenteN / seg : 0.0, tirFuenteMax * 1000.0, tirFuenteHuecos,
                    tirTrabajoMax * 1000.0, tirTrabajoLargos, periodoMs, tirPresentaMax * 1000.0, tirPresentaLargas,
                    2.0 * periodoMs, tirPresentaHueco * 1000.0, medLeeN ? medLeeMs / (double)medLeeN : 0.0, medLeeMax,
                    medSubeN ? medSubeMs / (double)medSubeN : 0.0, medSubeMax, tirRedN ? tirRedMs / (double)tirRedN : 0.0);
        /* y dentro de presentar: valla = esperar a que la tarjeta acabe el cuadro anterior de ese hueco,
           adquirir = esperar una imagen libre (el refresco), entregar; tarjeta = lo que tarda nuestro
           cuadro en la tarjeta, de principio a fin (con el juego compartiendola, incluye esperar turno) */
        const VkWeave::TiemposCuadro tc = wv.tomaTiempos();
        std::printf("[tirones] valla max %.1f · adquirir max %.1f · entregar max %.1f ms · tarjeta %.2f media / %.1f max "
                    "ms (%ld cuadros) · cola %s\n",
                    tc.vallaMax, tc.adquirirMax, tc.presentarMax, tc.gpuMedia, tc.gpuMax, tc.gpuN,
                    dev.prioridad().c_str());
        tirFuenteMax = tirTrabajoMax = tirPresentaMax = tirPresentaHueco = 0.0;
        tirFuenteN = tirFuenteHuecos = tirTrabajoLargos = tirPresentaLargas = 0;
        medLeeMax = medSubeMax = 0.0;
        tirRedMs = 0.0;
        tirRedN = 0;
      }
      /* Cada 20 s, lo que ha hecho el pase de afinado de los cantos finos (v0.89): asi el registro de
         una sesion dice si estuvo puesto y cuantas veces tuvo que repetirse. */
      {
        static int vueltaCantos = 0;
        static long hechosAntes = 0, ahorradosAntes = 0;
        if (g_cantosFinos && wv.converterOn() && ++vueltaCantos % 4 == 1) {
          long hechos = 0, ahorrados = 0;
          wv.cuentaAfinado(&hechos, &ahorrados);
          const long dh = hechos - hechosAntes, da = ahorrados - ahorradosAntes;
          hechosAntes = hechos, ahorradosAntes = ahorrados;
          if (dh + da > 0)
            std::printf("[cantos] finos: el mapa se afino en %ld de %ld cuadros (en los demas no habia cambiado ni la "
                        "imagen ni el mapa)\n", dh, dh + da);
        }
      }
      /* ODYSSEYVK_MEDIR: donde se va el cuadro. `leer` es bajar la copia pequena para la red y
         `subir` es mandar el mapa nuevo a la tarjeta; las dos corren en el hilo que dibuja. */
      static const bool kMedir = entornoOdyssey("ODYSSEYVK_MEDIR") != nullptr;
      if (kMedir)
        std::printf("[medir] cuadros lentos %.1f%% (%lld de %lld) · leer %.2f ms x %lld · subir %.2f ms x %lld\n",
                    frames ? 100.0 * (double)medLentos / (double)frames : 0.0, medLentos, (long long)frames,
                    medLeeN ? medLeeMs / (double)medLeeN : 0.0, medLeeN,
                    medSubeN ? medSubeMs / (double)medSubeN : 0.0, medSubeN);
      medLentos = 0; medLeeMs = medSubeMs = 0.0; medLeeN = medSubeN = 0;
      std::fflush(stdout);
      frames = 0;
      clock = now;
    }
  }

  devolverCursor(ui, "el visor se cierra");   /* si se lo habia quitado, el juego recupera el cursor */
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
