/* panel_ajustes.cpp — el panel de ajustes (`odysseyvk ajustes`), aparte del resto (v1.16): asi lo puede llevar
   tambien `odysseyvk-panel`, un ejecutable sin las bibliotecas del visor (onnxruntime, OpenCV, mpv...), que el
   panel no usa y que tardaban ~45 ms en cargarse cada vez que se abria (Rodolfo: «optimize the opening and
   closing of the layer to your best»; «Yes, do it»). */
#include "panel_ajustes.h"

#include "ircam.h"
#include "lens.h"
#include "calib.h"
#include "camera.h"
#include "track.h"
#include "atajos.h"
#include "rendimiento.h"
#include "panel.h"
#include "mpris.h"
#include "ajuste.h"
#include "barra.h"
#include "preferencias.h"
#include "integraciones.h"
#include "bandeja.h"
#include "idioma.h"
#include "wm.h"
#ifdef HAVE_WAYLAND_WM
#include "overlay.h"
#endif
#include "capture.h"
#include "glimport.h"
#include "vkdev.h"
#include "vkprobe.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "notify.h"
#include "scaler.h"
#include "session.h"
#include "testsrc.h"
#include "vkui.h"
#include "util.h"
#include "viewer.h"
#include "depth.h"
#include "weave.h"
#include <GLFW/glfw3.h>
#include <poll.h>
#include <csignal>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <sys/stat.h>
#include <sys/wait.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

/* ---------------- ajustes: el panel flotante (v0.84) ----------------------------------------
   Los ajustes de todos los dias y la biblioteca de juegos, en una tarjeta translucida de esquinas
   redondas que flota en el centro de la pantalla, encima de lo que haya: el escritorio (se abre
   con el icono del sistema) o el video en 3D (con el engranaje de la pastilla; ahi se dibuja al
   doble porque se ve a traves de la lente). Es la misma capa superpuesta de la tarjeta de la
   orilla. Abrirlo cuando ya esta abierto lo cierra. `--juegos` lo abre en esa pestana y
   `--dump F.png` deja su dibujo en un archivo. */
static volatile sig_atomic_t g_cierraPanel = 0;
static void alCerrarPanel(int) { g_cierraPanel = 1; }

int cmdAjustes(int argc, char **argv) {
  const std::string rutaPid = ajuste::rutaPanel();
  {
    /* ya hay uno abierto: se le pide que se cierre (el engranaje y el icono hacen de interruptor) */
    FILE *f = std::fopen(rutaPid.c_str(), "re");
    long viejo = 0;
    if (f) {
      if (std::fscanf(f, "%ld", &viejo) != 1) viejo = 0;
      std::fclose(f);
    }
    if (viejo > 1 && ::kill((pid_t)viejo, 0) == 0) {
      char destino[512] = {0};
      const std::string enlace = "/proc/" + std::to_string(viejo) + "/exe";
      const ssize_t n = ::readlink(enlace.c_str(), destino, sizeof destino - 1);
      if (n > 0 && std::string(destino, (size_t)n).find("odysseyvk") != std::string::npos) {
        ::kill((pid_t)viejo, SIGTERM);
        return 0;
      }
    }
  }
  bool enJuegos = false, forzarGrande = false, desdePastilla = false;
  std::string volcado, pista;
  for (int i = 0; i < argc; i++) {
    const std::string a = argv[i];
    if (a == "--juegos") enJuegos = true;
    else if (a == "--grande") forzarGrande = true;   /* para probar el tamano del 3D sin video */
    else if (a == "--desde-pastilla") desdePastilla = true;   /* (v1.16) lo abrio el engranaje de la pastilla del visor */
    else if (a == "--dump" && i + 1 < argc) volcado = argv[++i];
    else if (a == "--pantalla" && i + 1 < argc) pista = argv[++i];
  }
  std::signal(SIGTERM, alCerrarPanel);
  std::signal(SIGINT, alCerrarPanel);
  /* (v1.16) lo que tarda en abrirse, paso a paso, en el registro (Rodolfo: «optimize the opening and closing of the
     layer»): desde que arranco el proceso (su arranque en /proc, en centesimas) y desde aqui */
  auto relojA = [] {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
  };
  const double tA0 = relojA();
  double msArranque = -1.0;
  {
    std::string st;
    struct timespec tb{};
    if (readSmallFile("/proc/self/stat", st) && clock_gettime(CLOCK_BOOTTIME, &tb) == 0) {
      const size_t p = st.rfind(')');
      std::istringstream in(p == std::string::npos ? std::string() : st.substr(p + 2));
      std::string campo;
      for (int k = 3; k <= 22 && (in >> campo); k++)
        if (k == 22) msArranque = ((double)tb.tv_sec + 1e-9 * (double)tb.tv_nsec - std::atof(campo.c_str()) / (double)sysconf(_SC_CLK_TCK)) * 1000.0;
    }
  }
  std::vector<std::pair<const char *, double>> pasos;
  auto paso = [&](const char *que) { pasos.push_back({que, relojA()}); };
  Overlay ov;
  if (pista.empty()) {
    const auto conf = ajuste::leerServeConf();
    if (const auto it = conf.find("pantalla"); it != conf.end()) pista = it->second;
  }
  if (!pista.empty()) ov.setPantalla(pista);
  std::string err;
  if (!ov.open(&err)) {
    std::fprintf(stderr, "[ajustes] no se pudo abrir la capa superpuesta: %s\n", err.c_str());
    return 1;
  }
  paso("capa");
  if (FILE *f = std::fopen(rutaPid.c_str(), "we")) {
    std::fprintf(f, "%ld\n", (long)::getpid());
    std::fclose(f);
  }
  /* con un video en 3D debajo el panel se ve a traves de la lente: al doble, como la pastilla */
  const auto hub = ajuste::leerEstadoHub();
  const auto ses = hub.find("sesion");
  const bool haySesion = ses != hub.end() && !ses->second.empty() && ses->second != "ninguna";
  /* (v1.16) con el marco de vkDaedalus, tambien en el escritorio: a la misma escala que sobre un juego (Rodolfo: «desktop
     app size does not match the overlay, adjust»). MEDIDO en el G90XF: en el escritorio dibujaba a 1,50 (la escala del
     monitor) y sobre el juego a 2,00, asi que salia un 25 % mas chico. Se decide abajo, cuando ya se sabe si hay marco */
  bool grande = haySesion || forzarGrande;
  /* (v1.16) las caratulas se leen al entrar en Juegos (o ya, si se abre en Juegos): para lo demas basta la lista */
  std::vector<prefs::Juego> juegos = prefs::juegosSteam(enJuegos);
  bool conCaratulas = enJuegos;
  auto ponCaratulas = [&] {
    if (conCaratulas) return;
    juegos = prefs::juegosSteam(true);
    conCaratulas = true;
  };
  paso("juegos");
  std::vector<std::string> ids;
  std::vector<std::string> menuClaves;   /* (v1.17) lo de cada opcion del menu de la tesela del cuadrito */
  Hud::State &st = ov.hud().state();
  st.pestana = enJuegos ? 1 : 0;
  st.version = std::string("OdysseyVK ") + ODYSSEYVK_VERSION;
  st.desdePastilla = desdePastilla || entornoOdyssey("ODYSSEYVK_DESDE_PASTILLA");   /* (v1.16) la flecha de volver */
  /* (v1.16) al estilo de vkDaedalus, a pantalla completa; ODYSSEYVK_PANEL_VIEJO=1 deja el panel flotante de antes */
  st.marco = !(entornoOdyssey("ODYSSEYVK_PANEL_VIEJO") && entornoOdyssey("ODYSSEYVK_PANEL_VIEJO")[0] == '1');
  st.aparicion = st.marco ? 0.0f : 1.0f;
  if (st.marco) grande = true;
  /* (v1.16) abierto con la vista de depuracion del visor ya puesta (Ctrl+Mayus+9, o se cerro el panel con ella): en la
     columna de la izquierda y sin velo hasta que se quite. La casilla refleja lo del visor */
  if (st.marco && haySesion) {
    const auto e = ajuste::leerEstado();
    const auto vp = e.find("ver_profundidad");
    st.depuracion = vp != e.end() && vp->second == "si";
  }
  /* (v1.16) dentro de un juego (o con una sesion 3D) las pestanas se ven, pero Integraciones va en gris y en Juegos
     solo sale en color el juego de ahora (Rodolfo: «si muestra el boton de juegos e integraciones, pero integraciones
     en gris, que juegos deje entrar pero solo se vea de color el juego actual»). Antes, con una sesion 3D, la de
     Juegos no salia. Las demas caratulas, en gris y quietas (no se arranca otro juego desde dentro de uno), y ninguna
     con el engranaje que lleva a Integraciones. `juegoActual` es su appid («» = ninguno: un video) */
  bool sinIntegraciones = haySesion;
  std::string juegoActual;
  auto rellenar = [&] {
    /* (v1.16) el desplegable abierto se vuelve a buscar por la cabeza de su id («lista:red»): al rehacerse, las filas
       pueden cambiar de sitio (y sus opciones, de orden: una red bajada) */
    std::string cabezaDesp;
    if (st.desplegado >= 0 && (size_t)st.desplegado < ids.size())
      cabezaDesp = ids[(size_t)st.desplegado].substr(0, ids[(size_t)st.desplegado].find('\x1f'));
    prefs::llenar(st, prefs::leer(), juegos, ids);
    st.desplegado = -1;
    if (!cabezaDesp.empty())
      for (size_t k = 0; k < ids.size(); k++)
        if (ids[k].substr(0, ids[k].find('\x1f')) == cabezaDesp) st.desplegado = (int)k;
    st.pestanaGris.assign(st.pestanas.size(), false);
    if (sinIntegraciones) {
      if (st.pestanaGris.size() > 2) st.pestanaGris[2] = true;
      if (st.pestana == 2) st.pestana = 0;
      /* (v1.16) sin juego a la vista (un video en 3D) tampoco Juegos (Rodolfo: «si no se esta viendo un juego que la
         pestaña juegos tambien sea gris, por ejemplo en un video») */
      if (juegoActual.empty() && st.pestanaGris.size() > 1) st.pestanaGris[1] = true;
      if (juegoActual.empty() && st.pestana == 1) st.pestana = 0;
      st.pieParametro.clear(), st.pieInterruptor = false;   /* (v1.16) la casilla de ODYSSEYVK es de Integraciones: en el escritorio */
      for (size_t k = 0; k < st.items.size() && k < ids.size(); k++)
        if (st.items[k].tipo == Hud::ItemPanel::Juego) {
          st.items[k].ayudas.clear();
          st.items[k].gris = ids[k] != "juego:" + juegoActual || juegoActual.empty();
        }
    }
  };
  /* al abrirse, una mirada a GitHub (v0.99): asi la pestana Programa y el boton junto al aspa estan
     al dia sin esperar a la mirada del servicio. Es un curl aparte; si no hay red, no pasa nada. */
  if (prefs::leer().avisarActualizaciones) prefs::comprobarActualizacion(nullptr);
  paso("github");
  rellenar();
  paso("filas");
  /* el cursor de los juegos (rama experimental): si la ventana activa es un juego, el panel le quita
     el raton mientras esta abierto (el velo) y al cerrarse se lo devuelve, volviendo a hacerla la
     ventana activa. KWin solo deja el cursor encerrado en el juego mientras el juego es la activa:
     el teclado del panel (exclusivo) se lo quita. */
  WindowMonitor wmPanel;
  std::string juegoUuid, juegoApp;
  if (wmPanel.open(nullptr)) {
    /* (v1.16) una vuelta con el compositor en vez de settle(60, 10): los datos de cada ventana llegan en esa vuelta.
       MEDIDO en el escritorio de Rodolfo: las mismas 8 ventanas, con su app y la activa, en 0,2 ms en vez de 60 */
    wmPanel.sincroniza();
    for (const WindowInfo &w : wmPanel.windows()) {
      if (!w.active() || w.pid == (uint32_t)::getpid()) continue;
      const std::string a = strLower(w.appId);
      /* ODYSSEYVK_JUEGO=1 toma por juego la ventana activa que sea: para probarlo en un KWin aparte */
      const char *fingido = entornoOdyssey("ODYSSEYVK_JUEGO");
      if (a.rfind("steam_app_", 0) == 0 || a == "gamescope" || (fingido && fingido[0] == '1'))
        juegoUuid = w.uuid, juegoApp = w.appId;
      break;
    }
  }
  /* (v1.16) el juego de ahora: el de la ventana activa o, con una sesion 3D, el de la ventana que se ve en 3D (el hub
     apunta su uuid en `ventana`) */
  {
    auto appDe = [](const std::string &app) {
      const std::string a = strLower(app);
      return a.rfind("steam_app_", 0) == 0 ? a.substr(10) : std::string();
    };
    juegoActual = appDe(juegoApp);
    const auto ven = hub.find("ventana");
    if (juegoActual.empty() && haySesion && ven != hub.end() && ven->second != "-")
      for (const WindowInfo &w : wmPanel.windows())
        if (w.uuid == ven->second) juegoActual = appDe(w.appId);
    if (!juegoActual.empty()) std::printf("[ajustes] dentro del juego %s: Integraciones en gris y en Juegos solo el suyo\n", juegoActual.c_str());
  }
  if (!juegoUuid.empty() || haySesion) {
    sinIntegraciones = true;   /* (v1.16) la ventana activa es un juego, o hay una sesion 3D */
    rellenar();
  }
  /* el pin (rama experimental): fijado encima, sin velo y con el teclado a pedido */
  st.fijado = st.marco ? false : panelFijado();   /* (v1.16) a pantalla completa no hay pin */
  /* (v1.16) a pantalla completa tambien en el escritorio (Rodolfo: «make the desktop version fullscreen again by
     default»); ODYSSEYVK_PANEL_VENTANA=1 la deja en la ventana que se mueve por su barra */
  st.marcoVentana = st.marco && !haySesion && juegoUuid.empty() && entornoOdyssey("ODYSSEYVK_PANEL_VENTANA");
  /* y el acento del panel, el color de resaltado de KDE */
  colorResaltadoKde(st.acentoPanel);
  const bool quiereVelo = !juegoUuid.empty() || entornoOdyssey("ODYSSEYVK_VELO") != nullptr;
  /* ODYSSEYVK_VELO=1 lo pone sin juego: para probarlo en un KWin aparte */
  paso("ventanas");
  ov.showPanel(grande, quiereVelo && !st.fijado && !st.depuracion);
  paso("puesto");
  {
    std::string t;
    double antes = tA0;
    for (const auto &[que, cuando] : pasos) {
      char b[48];
      std::snprintf(b, sizeof b, "%s%s %.0f", t.empty() ? "" : " · ", que, (cuando - antes) * 1000.0);
      t += b;
      antes = cuando;
    }
    std::printf("[ajustes] abierto en %.0f ms desde aqui%s: %s\n", (relojA() - tA0) * 1000.0,
                msArranque >= 0.0 ? (std::string(" (") + std::to_string((int)msArranque) + " desde que arranco el proceso)").c_str() : "",
                t.c_str());
    std::fflush(stdout);
  }
  if (!ov.askVisible()) {
    std::fprintf(stderr, "[ajustes] el compositor no puso el panel: %s\n", ov.lastError().c_str());
    ::unlink(rutaPid.c_str());
    return 1;
  }
  /* el teclado entero para el panel mientras esta abierto (v1.09): el juego o el video de debajo no
     reciben teclas, Esc lo cierra y los atajos se capturan aqui */
  ov.modoTeclado(st.fijado ? 2 : 1);
  if (!juegoUuid.empty() && !st.fijado) {
    /* medido, no supuesto: el registro dice si el juego dejo de ser la ventana activa */
    for (int i = 0; i < 6; i++) {
      ov.waitAndDispatch(30);
      wmPanel.waitAndDispatch(30);
    }
    WindowInfo j{};
    const bool sigue = wmPanel.find(juegoUuid, j) && j.active();
    std::printf("[ajustes] sobre el juego %s: %s\n", juegoApp.c_str(),
                sigue ? "sigue siendo la ventana activa (su cursor puede seguir encerrado)"
                      : "deja de ser la ventana activa: su cursor queda libre y el raton es del panel");
    std::fflush(stdout);
  }
  /* la entrada en la barra de tareas mientras el panel esta abierto (v1.04), si no se apago */
  if (prefs::leer().enBarraTareas) ov.ponEntradaBarra(idioma::tr("OdysseyVK — Ajustes"));
  std::printf("[ajustes] panel puesto en %s (%s) · %d juegos de Steam\n", ov.outputName().c_str(),
              haySesion ? "al doble: hay una sesion 3D" : grande ? "al doble: el marco, como sobre un juego" : "tamano de escritorio", (int)juegos.size());
  std::fflush(stdout);
  if (!volcado.empty()) {
    ov.waitAndDispatch(150);
    std::printf("[ajustes] volcado a %s: %s\n", volcado.c_str(), ov.dumpPng(volcado, &err) ? "hecho" : err.c_str());
  }
  auto reloj = [] {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
  };
  double cerrarEn = -1.0, miradaHub = 0.0, miradaRend = 0.0;
  double copiadoHasta = -1.0;   /* (v1.14) hasta cuando el boton Copiar del pie dice «Copiado» */
  /* (v1.15) ODYSSEYVK_PANEL_GUION, para probar en un KWin aparte (sin raton ni teclado): pasos «SEGUNDOS ACCION ARG»
     separados por «;». `golpe ID [SUB]` pica la fila de ese id; `pestana N`; `escribe TEXTO` y `tecla COMBO` son
     pulsaciones; `volcado RUTA.png`; `fin` cierra el panel */
  struct PasoGuion {
    double t;
    std::string accion, arg;
  };
  std::vector<PasoGuion> guion;
  if (const char *gu = entornoOdyssey("ODYSSEYVK_PANEL_GUION")) {
    std::string todo = gu;
    size_t a = 0;
    while (a < todo.size()) {
      size_t b = todo.find(';', a);
      if (b == std::string::npos) b = todo.size();
      const std::string paso = strTrim(todo.substr(a, b - a));
      a = b + 1;
      const size_t e1 = paso.find(' ');
      if (e1 == std::string::npos) continue;
      const size_t e2 = paso.find(' ', e1 + 1);
      PasoGuion p{std::atof(paso.substr(0, e1).c_str()), paso.substr(e1 + 1, e2 == std::string::npos ? std::string::npos : e2 - e1 - 1),
                  e2 == std::string::npos ? std::string() : paso.substr(e2 + 1)};
      guion.push_back(p);
    }
  }
  std::deque<Overlay::Golpe> golpesGuion;
  std::deque<std::pair<std::string, std::string>> teclasGuion;
  std::string volcadoGuion;
  const double t0Guion = reloj();
  int idiomaDelAtlas = idioma::actual();
  /* (v1.16) la aparicion: la interfaz sale de la pastilla y se hace opaca (ver abajo) */
  const double t0Aparicion = reloj();
  while (!g_cierraPanel && !ov.conexionCaida() && !ov.entradaCerrada()) {
    ov.waitAndDispatch(st.aparicion < 1.0f ? 8 : (guion.empty() ? 100 : 20));
    Overlay::Golpe g;
    bool repintar = false;
    if (st.aparicion < 1.0f) {
      /* (v1.16) en 0,16 s y saliendo deprisa (1 - (1 - t)^3): enseguida se ve casi entera */
      const double t = std::min(1.0, (reloj() - t0Aparicion) / 0.16);
      st.aparicion = (float)(1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t));
      repintar = true;
    }
    while (!guion.empty() && reloj() - t0Guion >= guion.front().t) {
      const PasoGuion p = guion.front();
      guion.erase(guion.begin());
      std::printf("[ajustes] guion: %s %s\n", p.accion.c_str(), p.arg.c_str());
      std::fflush(stdout);
      if (p.accion == "golpe") {
        const size_t sp = p.arg.find(' ');
        const std::string id = p.arg.substr(0, sp);
        Overlay::Golpe gg;
        gg.tipo = (int)Hud::HitPanelItem;
        gg.sub = sp == std::string::npos ? 0 : std::atoi(p.arg.c_str() + sp + 1);
        for (size_t k = 0; k < ids.size(); k++)
          if (ids[k] == id || ids[k].rfind(id + '\x1f', 0) == 0) gg.item = (int)k;   /* (v1.16) un desplegable, por su cabeza */
        if (gg.item < 0) std::printf("[ajustes] guion: no hay fila «%s»\n", id.c_str());
        else golpesGuion.push_back(gg);
      } else if (p.accion == "pestana") {
        Overlay::Golpe gg;
        gg.tipo = (int)Hud::HitPanelPestana, gg.sub = std::atoi(p.arg.c_str());
        golpesGuion.push_back(gg);
      } else if (p.accion == "clic") {   /* (v1.16) un clic en x y del lienzo, como el del raton (pasa por la capa) */
        float cx = 0.0f, cy = 0.0f;
        if (std::sscanf(p.arg.c_str(), "%f %f", &cx, &cy) == 2) ov.clicPrueba(cx, cy);
      } else if (p.accion == "corre") {   /* (v1.16) la lista corrida N puntos, como con la rueda */
        st.corrida = (float)std::atof(p.arg.c_str());
        repintar = true;
      } else if (p.accion == "aprieta" || p.accion == "mueve" || p.accion == "suelta") {   /* (v1.16) arrastrar, a mano */
        float cx = 0.0f, cy = 0.0f;
        std::sscanf(p.arg.c_str(), "%f %f", &cx, &cy);
        ov.ratonPrueba(p.accion == "aprieta" ? 1 : (p.accion == "mueve" ? 0 : -1), cx, cy);
      } else if (p.accion == "tesela") {   /* (v1.16) una tesela chica: su numero (0..5) */
        Overlay::Golpe gg;
        gg.tipo = (int)Hud::HitPanelTesela, gg.sub = std::atoi(p.arg.c_str());
        golpesGuion.push_back(gg);
      } else if (p.accion == "escribe") {
        teclasGuion.push_back({"", p.arg});
      } else if (p.accion == "tecla") {
        teclasGuion.push_back({p.arg, ""});
      } else if (p.accion == "volcado") {
        volcadoGuion = p.arg;
        repintar = true;
      } else if (p.accion == "fin") {
        g_cierraPanel = 1;
      }
    }
    auto tomaGolpe = [&](Overlay::Golpe &gg) {
      if (golpesGuion.empty()) return ov.takeGolpe(gg);
      gg = golpesGuion.front();
      golpesGuion.pop_front();
      return true;
    };
    /* ODYSSEYVK_PIN_TRAS=S (para probar en un KWin aparte, donde no hay raton): a los S segundos se pica el pin */
    {
      static const char *pinTras = entornoOdyssey("ODYSSEYVK_PIN_TRAS");
      static const double t0 = reloj();
      static bool hecho = false;
      if (pinTras && !hecho && reloj() - t0 >= std::atof(pinTras)) {
        hecho = true;
        Overlay::Golpe gp;
        gp.tipo = (int)Hud::HitPanelFijar;
        g = gp;
        std::printf("[ajustes] prueba: se pica el pin\n");
      }
    }
    for (bool primero = true; (primero && g.tipo == (int)Hud::HitPanelFijar) || tomaGolpe(g); primero = false) {
      /* un clic en el velo, fuera del panel: el panel se cierra y el juego recupera el control */
      if (g.tipo == Overlay::kClicFuera) {
        std::printf("[ajustes] clic fuera del panel: se cierra y el juego recupera el control\n");
        std::fflush(stdout);
        g_cierraPanel = 1;
        break;
      }
      /* (v1.15) escribiendo en un campo de Integraciones: un clic en otra cosa guarda lo escrito */
      if (prefs::editandoTexto() && g.tipo != (int)Hud::HitPanelTitulo) {
        const bool mismo = g.tipo == (int)Hud::HitPanelItem && g.item >= 0 && (size_t)g.item < ids.size() &&
                           ids[(size_t)g.item] == prefs::idEditando();
        if (mismo) continue;
        /* guardar puede quitar o poner filas (el campo de otras opciones de gamescope, vaciado): el clic se vuelve a
           buscar por su id en la lista nueva */
        const std::string idClic = g.tipo == (int)Hud::HitPanelItem && g.item >= 0 && (size_t)g.item < ids.size() ? ids[(size_t)g.item] : "";
        std::string aviso;
        prefs::terminarTexto(juegos, &aviso);
        if (!aviso.empty()) st.pie = aviso;
        rellenar();
        repintar = true;
        if (g.tipo == (int)Hud::HitPanel) continue;
        if (!idClic.empty()) {
          g.item = -1;
          for (size_t k = 0; k < ids.size(); k++)
            if (ids[k] == idClic) g.item = (int)k;
          if (g.item < 0) continue;
        }
      }
      /* esperando un atajo: un clic en otra cosa que no sea la tecla de un atajo deja de esperar */
      if (!prefs::capturando().empty() && g.tipo != (int)Hud::HitPanel && g.tipo != (int)Hud::HitPanelTitulo) {
        const bool enAtajo = g.tipo == (int)Hud::HitPanelItem && g.item >= 0 && (size_t)g.item < ids.size() &&
                             ids[(size_t)g.item].rfind("atajo:", 0) == 0;
        if (!enAtajo) {
          prefs::cancelarCaptura();
          st.pie = idioma::tr("sin cambios");
          rellenar();
          repintar = true;
        }
      }
      /* (v1.16) una tesela chica del panel al estilo de vkDaedalus: la del idioma y la de la version hacen lo de sus
         botones de antes; las demas son interruptores, que se aplican como su fila */
      /* (v1.17) el menu de la tesela del cuadrito de los FPS: su flechita lo abre o lo cierra; una opcion se marca o se
         quita y el menu se queda abierto; cualquier otro clic lo cierra */
      if (g.tipo == (int)Hud::HitPanelTesela && g.sub >= 2000) {
        const size_t k = (size_t)(g.sub - 2000);
        if (k < menuClaves.size()) {
          std::string aviso;
          if (prefs::alternarMonitor(menuClaves[k], &aviso) && k < st.menuMarcadas.size()) st.menuMarcadas[k] = !st.menuMarcadas[k];
          st.pie = aviso;
          std::printf("[ajustes] monitor %s · %s\n", menuClaves[k].c_str(), aviso.c_str());
          std::fflush(stdout);
          rellenar();
        }
        repintar = true;
        continue;
      }
      if (g.tipo == (int)Hud::HitPanelTesela && g.sub >= 1000) {
        const int k = g.sub - 1000;
        if (st.teselaMenu == k) {
          st.teselaMenu = -1;
        } else {
          prefs::opcionesMonitor(st.menuOpciones, st.menuMarcadas, menuClaves);
          st.teselaMenu = k;
        }
        std::printf("[ajustes] menu del cuadrito %s (%zu opciones)\n", st.teselaMenu >= 0 ? "abierto" : "cerrado", menuClaves.size());
        std::fflush(stdout);
        repintar = true;
        continue;
      }
      if (st.teselaMenu >= 0) {
        st.teselaMenu = -1;
        repintar = true;
      }
      std::string idTesela;
      if (g.tipo == (int)Hud::HitPanelTesela && g.sub >= 0 && (size_t)g.sub < st.teselas.size()) {
        idTesela = st.teselas[(size_t)g.sub].id;
        if (idTesela == "menu_idioma") g.tipo = (int)Hud::HitPanelIdioma;
        else if (idTesela == "buscar_actualizacion") g.tipo = (int)Hud::HitPanelActualizar;
      }
      /* la lista de idiomas se cierra con cualquier clic que no sea en ella o en su boton */
      const bool clicIdioma = g.tipo == (int)Hud::HitPanelIdioma || g.tipo == (int)Hud::HitPanelIdiomaOpcion;
      if (st.menuIdioma && !clicIdioma) {
        st.menuIdioma = false;
        repintar = true;
      }
      /* (v1.16) y el desplegable de redes, con cualquiera que no sea en su caja o en sus opciones */
      if (st.desplegado >= 0 && !(g.tipo == (int)Hud::HitPanelItem && g.item == st.desplegado)) {
        st.desplegado = -1;
        repintar = true;
      }
      if (g.tipo == (int)Hud::HitPanelCerrar) {
        g_cierraPanel = 1;
      } else if (g.tipo == (int)Hud::HitPanelVolver) {
        /* (v1.16) volver a la pastilla: el panel se cierra y el visor la vuelve a poner (lo de `ctl capa mostrar`) */
        std::string e;
        if (!ajuste::escribir(std::map<std::string, double>(), std::string(), {{"capa", "mostrar"}}, &e))
          std::fprintf(stderr, "[ajustes] volver a la pastilla: %s\n", e.c_str());
        std::printf("[ajustes] volver a la pastilla\n");
        std::fflush(stdout);
        g_cierraPanel = 1;
      } else if (g.tipo == (int)Hud::HitPanelTesela && !idTesela.empty()) {
        std::string aviso;
        bool cerrar = false;
        if (prefs::aplicar(idTesela, 0, 0.0f, true, juegos, &aviso, &cerrar)) {
          std::printf("[ajustes] tesela %s%s%s\n", idTesela.c_str(), aviso.empty() ? "" : " · ", aviso.c_str());
          std::fflush(stdout);
          rellenar();
        }
        if (!aviso.empty()) st.pie = aviso;
        repintar = true;
      } else if (g.tipo == (int)Hud::HitPanelModoRed) {
        /* el interruptor AI | Rendimiento de la cabecera: el video normal con la red o sin ella */
        std::string aviso;
        bool cerrar = false;
        if ((g.sub == 1) != st.sinRedPanel && prefs::aplicar("exp_sin_red", g.sub, 0.0f, true, juegos, &aviso, &cerrar)) {
          st.pie = aviso;
          rellenar();
          std::printf("[ajustes] %s\n", g.sub == 1 ? "modo rendimiento (sin red)" : "AI (con la red)");
          std::fflush(stdout);
        }
        repintar = true;
      } else if (g.tipo == (int)Hud::HitPanelFijar) {
        /* el pin: fijado, el velo se va y el teclado pasa a pedido (un clic en el juego se lo lleva y
           el panel se queda); suelto, vuelve el velo (encima del juego) y el teclado exclusivo */
        st.fijado = !st.fijado;
        fijarPanel(st.fijado);
        if (st.fijado) ov.quitarVelo();
        else if (quiereVelo) ov.showPanel(grande, true);
        std::printf("[ajustes] panel %s\n", st.fijado ? "fijado encima: un clic en el juego ya no lo cierra"
                                                      : "suelto: un clic fuera lo cierra");
        std::fflush(stdout);
        repintar = true;
      } else if (g.tipo == (int)Hud::HitPanelIdioma) {
        st.menuIdioma = !st.menuIdioma;
        repintar = true;
      } else if (g.tipo == (int)Hud::HitPanelIdiomaOpcion) {
        st.menuIdioma = false;
        std::string aviso;
        bool cerrar = false;
        if (prefs::aplicar("idioma", g.sub, 0.0f, true, juegos, &aviso, &cerrar)) st.pie = aviso;
        repintar = true;
      } else if (g.tipo == (int)Hud::HitPanelCopiar && st.pieInterruptor) {
        /* (v1.16) la casilla del pie de Juegos: ODYSSEYVK=1 para todos (lo de cada juego se queda) */
        std::string aviso;
        prefs::alternarOdysseyTodos(juegos, &aviso);
        std::printf("[ajustes] ODYSSEYVK=1 en todos los juegos: %s · %s\n", st.pieValor ? "se quita" : "se pone", aviso.c_str());
        std::fflush(stdout);
        st.pie = aviso;
        rellenar();
        repintar = true;
      } else if (g.tipo == (int)Hud::HitPanelCopiar) {
        /* (v1.14) el parametro de Steam al portapapeles: el boton dice «Copiado» un momento; si no se puede, el pie */
        std::string e;
        if (prefs::copiarParametroSteam(&e)) {
          st.pieCopiado = true;
          copiadoHasta = reloj() + 2.5;
        } else {
          st.pie = idioma::trf("no se pudo copiar ({}): escríbelo a mano", {e});
        }
        std::printf("[ajustes] copiar el parametro de Steam: %s\n", e.empty() ? "copiado" : e.c_str());   /* (v1.16) */
        std::fflush(stdout);
        repintar = true;
      } else if (g.tipo == (int)Hud::HitPanelActualizar) {
        /* el boton junto al aspa (v0.95): con una version nueva, se instala; si no, se vuelve a mirar */
        std::string e;
        if (!st.actualizacion.empty()) {
          st.pie = prefs::lanzarActualizacion(&e) ? idioma::trf("actualizando a OdysseyVK {}: se compila, se pide tu clave y el servicio se reinicia", {st.actualizacion}) : e;
          if (e.empty()) cerrarEn = reloj() + 2.5;
        } else {
          st.pie = prefs::comprobarActualizacion(&e) ? std::string(idioma::tr("mirando la versión de GitHub…")) : e;
        }
        repintar = true;
      } else if (g.tipo == (int)Hud::HitPanelPestana && (size_t)std::max(0, g.sub) < st.pestanaGris.size() && st.pestanaGris[(size_t)g.sub]) {
        std::printf("[ajustes] la pestana %d esta en gris: no se abre\n", g.sub);   /* (v1.16) Integraciones dentro de un juego */
        std::fflush(stdout);
      } else if (g.tipo == (int)Hud::HitPanelPestana) {
        st.pestana = (g.sub >= 0 && (size_t)g.sub < st.pestanas.size()) ? g.sub : 0;   /* (v1.18) antes <= 3: FG (la 4) abria la de 3D */
        if (st.pestana == 1) ponCaratulas();   /* (v1.16) la primera vez que se entra */
        if (st.pestana == 2) prefs::integracionesDe("");   /* (v1.15) la pildora: lo de todos los juegos */
        st.corrida = 0.0f;
        st.pie.clear();
        rellenar();
        repintar = true;
      } else if (g.tipo == (int)Hud::HitPanelItem && g.item >= 0 && (size_t)g.item < ids.size() && ids[(size_t)g.item] == "depuracion") {
        /* (v1.16) la vista de depuracion: el visor ensena su mapa de profundidad (la orden `ver_profundidad`, lo de
           Ctrl+Mayus+9) y el panel deja solo las dos listas de redes en una columna a la izquierda, sin velo y con el
           teclado a pedido, para probarlas con el video a la vista. Quitada, vuelve todo */
        st.depuracion = !st.depuracion;
        st.desplegado = -1;
        st.corrida = 0.0f;
        std::string e;
        if (!ajuste::escribir(std::map<std::string, double>(), std::string(), {{"ver_profundidad", st.depuracion ? "1" : "0"}}, &e))
          std::fprintf(stderr, "[ajustes] vista de depuracion: %s\n", e.c_str());
        if (st.depuracion) ov.quitarVelo();
        else if (quiereVelo && !st.fijado) ov.showPanel(grande, true);
        st.pie = idioma::tr(st.depuracion ? "vista de depuración: prueba las redes con el vídeo a la vista" : "vista de depuración quitada");
        std::printf("[ajustes] vista de depuracion %s\n", st.depuracion ? "puesta" : "quitada");
        std::fflush(stdout);
        miradaRend = reloj();
        rellenar();
        repintar = true;
      } else if (g.tipo == (int)Hud::HitPanelItem && g.item >= 0 && (size_t)g.item < ids.size()) {
        std::string aviso;
        bool cerrar = false;
        std::string id = ids[(size_t)g.item];
        int subA = g.sub;
        /* (v1.16) un desplegable de redes: su caja lo abre o lo cierra; una opcion (sub 100 + k) hace lo de su casilla,
           cuya orden va detras de la cabeza del id (la k-esima, separadas por \x1f). La que ya esta puesta, nada */
        bool seguir = true;
        if (id.rfind("lista:", 0) == 0) {
          repintar = true;
          if (g.sub < 100) {
            st.desplegado = st.desplegado == g.item ? -1 : g.item;
            std::printf("[ajustes] %s %s\n", id.substr(0, id.find('\x1f')).c_str(), st.desplegado >= 0 ? "abierto" : "cerrado");
            std::fflush(stdout);
            seguir = false;
          } else {
            const int k = g.sub - 100;
            st.desplegado = -1;
            std::vector<std::string> partes;
            for (size_t a = 0; a <= id.size();) {
              size_t b = id.find('\x1f', a);
              if (b == std::string::npos) b = id.size();
              partes.push_back(id.substr(a, b - a));
              a = b + 1;
            }
            if (k == st.items[(size_t)g.item].elegido || k < 0 || (size_t)k + 1 >= partes.size()) seguir = false;
            else id = partes[(size_t)k + 1], subA = 0;
          }
        }
        if (seguir) {
        const bool cambio = prefs::aplicar(id, subA, g.frac, g.soltado, juegos, &aviso, &cerrar);
        /* (v1.15) el engranaje de una caratula o un juego de la lista de Integraciones: a esa pestana, arriba */
        if (const int p = prefs::tomarPestana(); p >= 0 && !(p == 2 && sinIntegraciones)) {
          st.pestana = p;
          st.corrida = 0.0f;
        }
        if (!aviso.empty()) st.pie = aviso;
        if (cambio) {
          std::printf("[ajustes] %s%s%s\n", id.c_str(), aviso.empty() ? "" : " · ", aviso.c_str());
          std::fflush(stdout);
          rellenar();
        }
        if (cambio || !aviso.empty()) repintar = true;
        if (cerrar) cerrarEn = reloj() + 1.2;   /* que de tiempo a leer «arrancando ...» */
        }
      }
    }
    /* fijado, el teclado va a pedido; esperando un atajo sin el «a pedido» del compositor, exclusivo */
    ov.modoTeclado(!(st.fijado || st.depuracion) ? 1 : (((!prefs::capturando().empty() || prefs::editandoTexto()) && !ov.tecladoAPedido()) ? 1 : 2));
    /* el teclado: con un atajo esperando, la combinacion va a el; si no, Esc cierra el panel (y las
       demas teclas se quedan aqui: el juego de debajo no las recibe) */
    {
      std::string combo, texto;
      auto tomaTecla = [&] {
        if (teclasGuion.empty()) return ov.takeTecla(combo, texto);
        combo = teclasGuion.front().first, texto = teclasGuion.front().second;
        teclasGuion.pop_front();
        return true;
      };
      while (tomaTecla()) {
        /* (v1.15) escribiendo en un campo de Integraciones, las teclas van a el (Esc deja de escribir, no cierra) */
        if (prefs::editandoTexto()) {
          std::string aviso;
          if (prefs::teclaTexto(combo, texto, juegos, &aviso)) {
            if (!aviso.empty()) st.pie = aviso;
            rellenar();
            repintar = true;
          }
          continue;
        }
        /* las que solo valen para escribir (una letra con acento, Retroceso, Intro, Tab) no son atajos */
        if (combo.empty() || combo == "Backspace" || combo == "Return" || combo == "Tab") continue;
        if (!prefs::capturando().empty()) {
          std::string aviso;
          if (prefs::teclaCapturada(combo, &aviso)) {
            if (!aviso.empty()) st.pie = aviso;
            std::printf("[ajustes] atajo: %s · %s\n", combo.c_str(), aviso.c_str());
            std::fflush(stdout);
            rellenar();
            repintar = true;
          }
        } else if (combo == "Escape") {
          if (st.menuIdioma || st.desplegado >= 0 || st.teselaMenu >= 0) {
            st.menuIdioma = false;
            st.desplegado = -1;
            st.teselaMenu = -1;
            repintar = true;
          } else {
            g_cierraPanel = 1;
          }
        }
      }
    }
    /* el idioma cambio (por el globo o por la fila de Programa): el atlas de letras se rehace con
       las del idioma nuevo (el chino y el japones traen las suyas) y las filas se vuelven a escribir */
    if (idioma::actual() != idiomaDelAtlas) {
      idiomaDelAtlas = idioma::actual();
      std::string e;
      if (!ov.hud().buildAtlas(&e)) std::fprintf(stderr, "[ajustes] atlas: %s\n", e.c_str());
      if (prefs::leer().enBarraTareas) ov.ponEntradaBarra(idioma::tr("OdysseyVK — Ajustes"));
      std::printf("[ajustes] idioma: %s\n", idioma::codigo(idiomaDelAtlas));
      std::fflush(stdout);
      rellenar();
      repintar = true;
    }
    /* la pestana de rendimiento se refresca sola, una vez por segundo */
    if ((st.pestana == 3 || st.pestana == 4 || st.depuracion) && reloj() - miradaRend >= 1.0) {   /* (v1.18) y FG */   /* Programa lleva el rendimiento; (v1.16) la vista de depuracion, lo de la red */
      miradaRend = reloj();
      rellenar();
      repintar = true;
    }
    /* (v1.15) Integraciones, cada dos segundos: si Steam se abrio o se cerro, lo que falta escribir, el «Copiado».
       Solo se rehace si cambio algo (integ::firma: unas fechas de ficheros y el steam.pid); rehacerla entera es leer
       y analizar el localconfig.vdf de Steam */
    if (st.pestana == 2 && reloj() - miradaRend >= 2.0) {
      static std::string firmaInteg;
      miradaRend = reloj();
      std::string f = prefs::firmaIntegraciones();
      if (f != firmaInteg) {
        firmaInteg = std::move(f);
        rellenar();
        repintar = true;
      }
    }
    /* la comprobacion de la version (v0.95): al acabar se repinta con lo que haya dicho GitHub */
    if (prefs::comprobacionEnMarcha() && prefs::vigilarComprobacion()) {
      const prefs::Actualizacion a = prefs::estadoActualizacion();
      st.pie = a.remota.empty() ? std::string(idioma::tr("no se pudo mirar GitHub (¿hay conexión?)"))
                                : idioma::trf(a.pendiente ? "hay una versión nueva: OdysseyVK {}" : "estás al día (OdysseyVK {})", {a.remota});
      rellenar();
      repintar = true;
    }
    /* la descarga de la red abierta (v0.92): su avance se ensena segun llega */
    if (prefs::descargaEnMarcha() && prefs::vigilarDescarga()) {
      rellenar();
      repintar = true;
      if (!prefs::descargaEnMarcha()) {
        std::printf("[ajustes] red abierta: %s\n", prefs::textoDescarga().c_str());
        std::fflush(stdout);
        const std::string bajada = prefs::recogerRedBajada();
        if (!bajada.empty()) {
          /* (v1.16) bajada y entera: queda puesta (se pico su casilla para eso). Sin ninguna red antes, el visor que haya
             abierto la carga ya (orden `cargar_red`), el servicio la ve en su proxima mirada y este panel, que se abrio
             para eso, se cierra solo (v0.94); con otra ya puesta, se cambia a esta y el panel se queda */
          std::string e, aviso;
          if (esRedDeVideo(bajada)) {   /* (v1.16) la de video: queda puesta para los videos, y nada mas */
            prefs::ponerRedVideo(bajada, &aviso);
            st.pie = aviso;
          } else {
            prefs::ponerRed(bajada, &aviso);
            if (!prefs::descargaHabiaRed()) {
              ajuste::escribir(std::map<std::string, double>(), bajada, {{"cargar_red", "si"}}, &e);
              st.pie = idioma::tr("red descargada: el 3D de vídeo normal ya puede calcular la profundidad");
              cerrarEn = reloj() + 2.5;
            } else {
              st.pie = aviso;
            }
          }
          rellenar();
        } else {
          st.pie = prefs::textoDescarga();
        }
      }
    }
    if (copiadoHasta > 0.0 && reloj() >= copiadoHasta) {
      copiadoHasta = -1.0;
      if (st.pieCopiado) {
        st.pieCopiado = false;
        repintar = true;
      }
    }
    if (repintar) ov.repaint();
    if (!volcadoGuion.empty()) {
      std::string e;
      std::printf("[ajustes] guion: volcado a %s: %s · pie: %s\n", volcadoGuion.c_str(), ov.dumpPng(volcadoGuion, &e) ? "hecho" : e.c_str(),
                  st.pie.c_str());
      std::fflush(stdout);
      volcadoGuion.clear();
    }
    if (cerrarEn > 0.0 && reloj() >= cerrarEn) break;
    /* abierto sobre un video en 3D: cuando ese video se acaba, el panel se va con el (si no, se
       quedaba al doble de grande en medio del escritorio) */
    if (haySesion && reloj() - miradaHub >= 1.0) {
      miradaHub = reloj();
      const auto h2 = ajuste::leerEstadoHub();
      const auto s2 = h2.find("sesion");
      if (s2 == h2.end() || s2->second.empty() || s2->second == "ninguna") break;
    }
  }
  /* (v1.16) al cerrarse, de golpe (Rodolfo: «closing is still slow from a video»). Hubo una animacion de 0,10 s al
     reves de la de abrirse: MEDIDO en 4K, cada cuadro de la pantalla entera cuesta ~50 ms en la CPU, asi que eran
     150-165 ms con 3 cuadros a trompicones. Ahora la capa se quita en cuanto se pide */
  const double t0Cierre = reloj();
  const int cuadrosCierre = 0;
  const double tFinAnim = reloj();
  /* (v1.15) se cierra a medio escribir en un campo de Integraciones: lo escrito se guarda */
  if (prefs::editandoTexto()) {
    std::string aviso;
    prefs::terminarTexto(juegos, &aviso);
    std::printf("[ajustes] integraciones: %s\n", aviso.c_str());
    std::fflush(stdout);
  }
  ov.tomarTeclado(false);
  ov.hideAsk();
  /* el juego vuelve a ser la ventana activa: recupera el teclado y, si lo encerraba, el cursor */
  if (!juegoUuid.empty()) {
    WindowInfo j{};
    wmPanel.waitAndDispatch(0);
    std::string e;
    const bool ok = wmPanel.find(juegoUuid, j) && wmPanel.setState(juegoUuid, WindowInfo::Active, true, &e);
    wmPanel.flush();
    std::printf("[ajustes] panel cerrado: %s\n", ok ? ("el control vuelve a " + juegoApp).c_str()
                                                     : ("no se pudo devolver el control al juego: " + e).c_str());
    std::fflush(stdout);
  }
  ::unlink(rutaPid.c_str());
  /* (v1.16) lo que tarda en cerrarse, como lo de abrirse */
  std::printf("[ajustes] cerrado en %.0f ms: animacion %.0f ms (%d cuadros), lo demas %.0f ms\n", (reloj() - t0Cierre) * 1000.0,
              (tFinAnim - t0Cierre) * 1000.0, cuadrosCierre, (reloj() - tFinAnim) * 1000.0);
  std::fflush(stdout);
  return 0;
}
