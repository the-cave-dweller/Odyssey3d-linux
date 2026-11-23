/* odyssey3d — visor/reproductor 3D sin gafas para el Samsung Odyssey 3D (G90XF)
   en Wayland. Parte de control de hardware: lente lenticular, scaler MediaTek,
   iluminador IR y modo del panel. */
#include "ircam.h"
#include "lens.h"
#include "panel.h"
#include "wm.h"
#include "capture.h"
#include "glimport.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "notify.h"
#include "scaler.h"
#include "testsrc.h"
#include "util.h"
#include "viewer.h"
#include "weave.h"

#include <GLFW/glfw3.h>

#include <poll.h>

#include <csignal>
#include <map>
#include <set>
#include <sys/wait.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

static const char *rotW(int r) {
  const char *w = Panel::rotationWord(r);
  return w ? w : "desconocida";
}

static const char *kUsage =
    "odyssey3d — 3D sin gafas en el Samsung Odyssey 3D (G90XF) bajo Wayland\n"
    "\n"
    "  odyssey3d play ARCHIVO        reproducir un video side-by-side en 3D sin gafas\n"
    "  odyssey3d test                patron de calibracion (disparidades, barras RGB) sobre el panel\n"
    "  odyssey3d doctor              diagnostico: lente, scaler, camara IR, panel, permisos\n"
    "  odyssey3d weave               escribe un PNG con la trama calculada (sin tocar el panel)\n"
    "  odyssey3d attrs               atributos de trama calculados (para validar el modelo)\n"
    "  odyssey3d ctl lens   [info|auth|on|off|state] [--port /dev/ttyACM0]\n"
    "  odyssey3d ctl scaler [on|off] [--dev /dev/sg1]\n"
    "  odyssey3d ctl ir     [on|off] [--dev /dev/video2]\n"
    "  odyssey3d ctl panel  [status|apply|restore] [--output DP-4] [--hz 120] [--dry]\n"
    "  odyssey3d ctl windows [--watch]\n"
    "  odyssey3d ctl capture [--uuid X] [--out F] [--segundos N]\n"
    "  odyssey3d ctl capture-gl [--uuid X] [--out F] [--segundos N]\n"
    "  odyssey3d ctl notify [--wait N]\n"
    "  odyssey3d capture3d [--uuid X]   capturar una ventana a pantalla completa y tramarla en 3D\n"
    "  odyssey3d serve [--dry]          servicio: avisa cuando algo se pone a pantalla completa y lo pone en 3D\n"
    "\n"
    "opciones:\n"
    "  --output NOMBRE   salida del panel (por defecto, la que tenga modo 3840x2160)\n"
    "  --hz N            frecuencia objetivo del modo (por defecto 120)\n"
    "  --windowed        ventana en lugar de pantalla completa\n"
    "  --no-panel        no tocar el modo / la escala / la rotacion del panel\n"
    "  --no-lens         no tocar la lente lenticular\n"
    "  --no-scaler       no tocar el modo 3D del scaler del monitor\n"
    "  --2d              arrancar en pase 2D (sin tramar)\n"
    "  --tb              video arriba/abajo (por defecto: lado a lado)\n"
    "  --hwdec M         decodificacion de mpv: auto, nvdec, no (prueba `no` si hay artefactos)\n"
    "  --eye-x/--eye-y/--eye-z CM   posicion del ojo (por defecto 0, 8.39, 65.11 cm)\n"
    "  --align CICLOS    desfase de fase inicial   --pitch F  escala de la rampa de fase\n"
    "  --calib-dir DIR   mapas de correccion del panel (3DStackCorrection_A/B.png)\n"
    "  --solid / --no-corr / --out ARCHIVO   solo `weave`: fuente plana, sin correcciones, salida\n"
    "\n"
    "teclas: ESC salir · 3 2D/3D · F invertir profundidad · flechas fase/paso/crosstalk · , . filtro\n"
    "        9/0 convergencia · 8 reiniciar ajuste · espacio pausa · - = saltar 5 s · [ ] cuadro\n";

static std::string flag(int argc, char **argv, const char *name) {
  for (int i = 0; i < argc; i++)
    if (std::string(argv[i]) == name && i + 1 < argc) return argv[i + 1];
  return {};
}

static bool hasFlag(int argc, char **argv, const char *name) {
  for (int i = 0; i < argc; i++)
    if (std::string(argv[i]) == name) return true;
  return false;
}

static int cmdLens(int argc, char **argv) {
  std::string what = argc > 0 ? argv[0] : "info";
  std::string port = flag(argc, argv, "--port");
  if (port.empty()) {
    port = Lens::findPort();
    if (port.empty()) {
      std::fprintf(stderr, "no se encontro el puerto de la lente (buscado por USB 354b:0116)\n");
      return 1;
    }
  }
  Lens lens;
  if (!lens.open(port)) {
    std::fprintf(stderr, "%s\n", lens.lastError().c_str());
    return 1;
  }
  std::printf("puerto: %s\n", port.c_str());
  if (what == "info") {
    for (auto &kv : lens.info())
      std::printf("  %-10s: %s\n", kv.first.c_str(), kv.second.c_str());
    std::printf("  auth     : %s\n", lens.authenticated() ? "si" : "no");
    return 0;
  }
  if (what == "auth") {
    bool ok = lens.authenticate();
    std::printf("authenticate: %s\n", ok ? "ok" : "fallo");
    if (!ok) std::fprintf(stderr, "%s\n", lens.lastError().c_str());
    std::printf("  auth     : %s\n", lens.authenticated() ? "si" : "no");
    return ok ? 0 : 1;
  }
  if (what == "on" || what == "off") {
    bool on = what == "on";
    if (!lens.authenticate()) std::fprintf(stderr, "aviso: auth fallo: %s\n", lens.lastError().c_str());
    bool ok = on ? lens.on() : lens.off();
    std::printf("lente %s: %s\n", on ? "on" : "off", ok ? "ok" : "fallo");
    if (!ok) std::fprintf(stderr, "%s\n", lens.lastError().c_str());
    std::printf("  estado   : %s\n", lens.state() ? "on" : "off");
    return ok ? 0 : 1;
  }
  if (what == "state") {
    std::printf("  estado   : %s\n", lens.state() ? "on" : "off");
    std::printf("  auth     : %s\n", lens.authenticated() ? "si" : "no");
    return 0;
  }
  std::fprintf(stderr, "subcomando de lens desconocido: %s\n", what.c_str());
  return 2;
}

static int cmdScaler(int argc, char **argv) {
  std::string what = argc > 0 ? argv[0] : "off";
  std::string dev = flag(argc, argv, "--dev");
  if (dev.empty()) {
    dev = Scaler::findDevice();
    if (dev.empty()) {
      std::fprintf(stderr, "no se encontro el dispositivo scaler (buscado por vendor GCREADER)\n");
      return 1;
    }
  }
  Scaler sc;
  if (!sc.open(dev)) {
    std::fprintf(stderr, "%s\n", sc.lastError().c_str());
    return 1;
  }
  bool on = what == "on";
  int status = -1, host = -1;
  bool ok = sc.set3d(on, &status, &host);
  std::printf("scaler %s sobre %s: %s (status=%d host=%d)\n", on ? "on" : "off", dev.c_str(),
              ok ? "ok" : "fallo", status, host);
  if (!ok) std::fprintf(stderr, "%s\n", sc.lastError().c_str());
  return ok ? 0 : 1;
}

static int cmdIr(int argc, char **argv) {
  std::string what = argc > 0 ? argv[0] : "on";
  std::string dev = flag(argc, argv, "--dev");
  if (dev.empty()) {
    auto devs = IrCam::findDevices();
    if (devs.empty()) {
      std::fprintf(stderr, "no se encontro la camara 3D Stereo WebCam (04e8:20d7)\n");
      return 1;
    }
    dev = devs.front();
    std::fprintf(stderr, "usando %s\n", dev.c_str());
  }
  /* El endpoint de control UVC de esta camara solo responde tras autenticar la lente. */
  {
    Lens lens;
    std::string port = Lens::findPort();
    if (!port.empty() && lens.open(port)) {
      if (!lens.authenticated() && !lens.authenticate())
        std::fprintf(stderr, "aviso: auth de lente fallo (%s); el IR puede no responder\n",
                     lens.lastError().c_str());
    }
  }
  IrCam cam;
  if (!cam.open(dev)) {
    std::fprintf(stderr, "%s\n", cam.lastError().c_str());
    return 1;
  }
  bool ok = what == "off" ? cam.irOff() : cam.irOn();
  std::printf("ir %s: %s\n", what.c_str(), ok ? "ok" : "fallo");
  if (!ok) std::fprintf(stderr, "%s\n", cam.lastError().c_str());
  return ok ? 0 : 1;
}

#ifdef HAVE_CAPTURE
/* Elige la ventana a capturar: la del --uuid o, si no se dio, la primera a pantalla completa. */
static bool pickWindow(int argc, char **argv, std::string &uuid, WindowInfo &win, bool &haveWin) {
  uuid = flag(argc, argv, "--uuid");
  haveWin = false;
  if (!uuid.empty()) return true;
  WindowMonitor wm;
  std::string err;
  if (!wm.open(&err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return false;
  }
  wm.settle();
  if (!wm.fullscreenCandidate(win, {})) {
    std::fprintf(stderr, "no hay ventana a pantalla completa (usa --uuid con `ctl windows`)\n");
    return false;
  }
  uuid = win.uuid;
  haveWin = true;
  std::printf("ventana: %s | %s | %ux%u uuid=%s\n", win.appId.c_str(), win.title.c_str(), win.w, win.h,
              uuid.c_str());
  return true;
}
#endif

static int cmdCapture(int argc, char **argv) {
#ifndef HAVE_CAPTURE
  (void)argc; (void)argv;
  std::fprintf(stderr, "compilado sin soporte de captura (hace falta libpipewire-0.3 y plasma-wayland-protocols)\n");
  return 1;
#else
  std::string out = flag(argc, argv, "--out");
  if (out.empty()) out = "captura.png";
  int secs = 4;
  std::string sd = flag(argc, argv, "--segundos");
  if (!sd.empty()) secs = std::atoi(sd.c_str());

  std::string uuid;
  WindowInfo win;
  bool haveWin = false;
  if (!pickWindow(argc, argv, uuid, win, haveWin)) return 1;

  std::string err;
  WindowCapture cap;
  std::string cls = flag(argc, argv, "--clase");
  if (!cls.empty()) cap.setStreamClass(cls);
  if (!cap.start(uuid, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  std::printf("pidiendo el flujo de la ventana...\n");
  for (int i = 0; i < secs * 2; ++i) {
    struct pollfd pfd{cap.fd(), POLLIN, 0};
    poll(&pfd, 1, 500);
    cap.pump();
    std::printf("%s\n", cap.describe().c_str());
    std::fflush(stdout);
    if (cap.framesFlowing()) break;
  }
  std::vector<uint8_t> px;
  int w = 0, h = 0, stride = 0;
  if (!cap.copyFrame(px, w, h, stride)) {
    std::fprintf(stderr, "han llegado %llu fotogramas pero no se pueden leer con la CPU: %s\n",
                  static_cast<unsigned long long>(cap.frameCount()), cap.lastError().c_str());
    std::fprintf(stderr, "prueba `ctl capture-gl`, que usa el fotograma directamente en la GPU\n");
    cap.stop();
    return 1;
  }
  std::printf("fotograma: %dx%d paso %d (%zu bytes)\n", w, h, stride, px.size());
  if (haveWin && win.w && w && (static_cast<int>(win.w) != w || static_cast<int>(win.h) != h))
    std::printf("(ojo: la ventana mide %ux%u)\n", win.w, win.h);
#ifndef HAVE_PNG
  std::fprintf(stderr, "sin libpng: no se puede guardar el PNG\n");
  cap.stop();
  return 0;
#else
  std::vector<uint8_t> rgba(px.size());
  for (size_t i = 0; i + 3 < px.size(); i += 4) {
    rgba[i] = px[i + 2];
    rgba[i + 1] = px[i + 1];
    rgba[i + 2] = px[i];
    rgba[i + 3] = px[i + 3];
  }
  if (!glPngSave(out, w, h, rgba.data())) {
    std::fprintf(stderr, "no se pudo escribir %s\n", out.c_str());
    cap.stop();
    return 1;
  }
  std::printf("guardado %s\n", out.c_str());
#endif
  cap.stop();
  return 0;
#endif
}

#ifdef HAVE_CAPTURE
/* Prueba de la ruta de la GPU: captura la ventana, importa el DMA-BUF como textura OpenGL y
   lo vuelca a PNG. Sirve para comprobar que el compositor y el controlador cooperan. */
static int cmdCaptureGl(int argc, char **argv) {
  std::string uuid;
  WindowInfo win;
  bool haveWin = false;
  if (!pickWindow(argc, argv, uuid, win, haveWin)) return 1;
  std::string out = flag(argc, argv, "--out");
  if (out.empty()) out = "captura-gl.png";
  int secs = 4;
  std::string sd = flag(argc, argv, "--segundos");
  if (!sd.empty()) secs = std::atoi(sd.c_str());

  if (!glfwInit()) {
    std::fprintf(stderr, "glfwInit fallo\n");
    return 1;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  GLFWwindow *winGl = glfwCreateWindow(320, 240, "odyssey3d-gl", nullptr, nullptr);
  if (!winGl) {
    std::fprintf(stderr, "no se pudo crear el contexto OpenGL\n");
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(winGl);
  std::printf("GL: %s | %s\n", reinterpret_cast<const char *>(glGetString(GL_VENDOR)),
              reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
  if (flag(argc, argv, "--verboso") == "1" || hasFlag(argc, argv, "--verboso")) {
    std::string glx;
    GLint nExt = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &nExt);
    for (GLint i = 0; i < nExt; ++i) {
      const char *e = reinterpret_cast<const char *>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)));
      if (e) glx += std::string(e) + " ";
    }
    std::printf("extensiones GL (%d): %s\n", nExt, glx.c_str());
    for (const char *needle : {"GL_OES_EGL_image", "GL_EXT_EGL_image_storage", "GL_OES_EGL_image_external"})
      std::printf("  %s: %s\n", needle, glx.find(needle) != std::string::npos ? "sí" : "no");
    const char *ee = eglQueryString(eglGetCurrentDisplay(), EGL_EXTENSIONS);
    std::string egx = ee ? ee : "";
    for (const char *needle : {"EGL_EXT_image_dma_buf_import", "EGL_EXT_image_dma_buf_import_modifiers",
                               "EGL_EXT_gl_colorspace", "EGL_KHR_image_base"})
      std::printf("  %s: %s\n", needle, egx.find(needle) != std::string::npos ? "sí" : "no");
    std::fflush(stdout);
  }

  GpuImporter imp;
  std::string err;
  if (!imp.init(&err)) {
    std::fprintf(stderr, "importación DMA-BUF no disponible: %s\n", err.c_str());
    glfwDestroyWindow(winGl);
    glfwTerminate();
    return 1;
  }
  std::printf("ruta de importación: %s\n", imp.method().c_str());

  WindowCapture cap;
  std::string cls = flag(argc, argv, "--clase");
  if (!cls.empty()) cap.setStreamClass(cls);
  if (!cap.start(uuid, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    glfwDestroyWindow(winGl);
    glfwTerminate();
    return 1;
  }
  std::printf("pidiendo el flujo de la ventana...\n");
  for (int i = 0; i < secs * 2; ++i) {
    struct pollfd pfd{cap.fd(), POLLIN, 0};
    poll(&pfd, 1, 500);
    cap.pump();
    glfwPollEvents();
    if (cap.framesFlowing()) break;
    if (i % 2 == 1) std::printf("%s\n", cap.describe().c_str());
    std::fflush(stdout);
  }
  CapturedFrame cf;
  if (!cap.lastFrame(cf)) {
    std::fprintf(stderr, "no llegó ningún fotograma (%s)\n", cap.lastError().c_str());
    cap.stop();
    glfwDestroyWindow(winGl);
    glfwTerminate();
    return 1;
  }
  GpuFrame gf;
  gf.fd = cf.fd;
  gf.w = cf.w;
  gf.h = cf.h;
  gf.stride = cf.stride;
  gf.offset = cf.offset;
  gf.modifier = cf.modifier;
  gf.fourcc = cf.fourcc;
  gf.dmabuf = cf.dmabuf;
  std::printf("fotograma: %dx%d paso %d fd %d modificador 0x%llx%s\n", gf.w, gf.h, gf.stride, gf.fd,
              static_cast<unsigned long long>(gf.modifier), gf.dmabuf ? "" : " (memoria compartida)");

  GLuint tex = imp.upload(gf, &err);
  if (!tex) {
    std::fprintf(stderr, "no se pudo importar: %s\n", err.c_str());
    cap.stop();
    glfwDestroyWindow(winGl);
    glfwTerminate();
    return 1;
  }
  std::printf("textura OpenGL %u importada\n", tex);
  std::vector<uint8_t> rgba;
  if (!glImportReadback(tex, gf.w, gf.h, rgba, &err)) {
    std::fprintf(stderr, "no se pudo leer la textura: %s\n", err.c_str());
    cap.stop();
    glfwDestroyWindow(winGl);
    glfwTerminate();
    return 1;
  }
#ifdef HAVE_PNG
  if (!glPngSave(out, gf.w, gf.h, rgba.data())) {
    std::fprintf(stderr, "no se pudo escribir %s\n", out.c_str());
    cap.stop();
    glfwDestroyWindow(winGl);
    glfwTerminate();
    return 1;
  }
  std::printf("guardado %s (%d fotogramas recibidos)\n", out.c_str(), static_cast<int>(cap.frameCount()));
#else
  (void)out;
  std::printf("leídos %zu bytes de la textura (sin libpng no se guarda el PNG)\n", rgba.size());
#endif
  cap.stop();
  glfwDestroyWindow(winGl);
  glfwTerminate();
  return 0;
}
#endif

static int cmdNotify(int argc, char **argv) {
#ifndef HAVE_SDBUS
  (void)argc; (void)argv;
  std::fprintf(stderr, "compilado sin libsystemd (sd-bus): hace falta el paquete systemd-libs\n");
  return 1;
#else
  int waitSecs = 30;
  std::string w = flag(argc, argv, "--wait");
  if (!w.empty()) waitSecs = std::atoi(w.c_str());
  std::string err;
  Notifier n;
  if (!n.open(&err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  std::printf("portal de avisos: version %u\n", n.version());
  std::vector<std::pair<std::string, std::string>> botones = {
      {"sbs", "3D lado a lado"}, {"tb", "3D arriba/abajo"}, {"flat", "Solo lente"}, {"no", "No"}};
  if (!n.send("odyssey3d-prueba", "Video a pantalla completa detectado",
              "Elige como quieres verlo en el Odyssey 3D", botones, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  std::printf("aviso enviado; esperando la pulsacion (%d s, Ctrl+C para salir)\n", waitSecs);
  fflush(stdout);
  bool picked = false;
  n.setOnAction([&](const std::string &id, const std::string &action) {
    std::printf("accion: id=%s accion=%s\n", id.c_str(), action.c_str());
    fflush(stdout);
    picked = true;
  });
  for (int i = 0; !picked && i < waitSecs * 4; ++i) {
    struct pollfd pfd{n.fd(), POLLIN, 0};
    poll(&pfd, 1, 250);
    if (pfd.revents & POLLIN) n.process();
  }
  n.remove("odyssey3d-prueba");
  if (!picked) std::printf("no pulso nadie ningun boton\n");
  return picked ? 0 : 3;
#endif
}

static int cmdWindows(int argc, char **argv) {
#ifndef HAVE_WAYLAND_WM
  (void)argc; (void)argv;
  std::fprintf(stderr, "compilado sin soporte de ventanas (hace falta wayland-client y plasma-wayland-protocols)\n");
  return 1;
#else
  bool watch = hasFlag(argc, argv, "--watch");
  std::string err;
  WindowMonitor wm;
  if (!wm.open(&err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  wm.settle();
  std::printf("%-34s  %-20s  %-30s  %-24s  %-11s  %s\n", "uuid", "app_id", "titulo", "estado", "tamano", "pid");
  for (const auto &w : wm.windows()) {
    std::string t = w.title;
    if (t.size() > 28) t = t.substr(0, 27) + "\xe2\x80\xa6";
    std::printf("%-34s  %-20s  %-30s  %-24s  %4ux%-6u  %u\n", w.uuid.c_str(), w.appId.c_str(), t.c_str(),
                WindowInfo::stateList(w.states).c_str(), w.w, w.h, w.pid);
  }
  WindowInfo cand;
  if (wm.fullscreenCandidate(cand, {}))
    std::printf("\ncandidata a pantalla completa: %s (%s) uuid=%s\n", cand.appId.c_str(), cand.title.c_str(),
                cand.uuid.c_str());
  else
    std::printf("\nno hay ninguna ventana a pantalla completa\n");
  if (!watch) return 0;

  std::printf("\n(vigilando cambios, Ctrl+C para salir)\n");
  std::string prev;
  wm.setOnChange([&]() {
    WindowInfo c;
    std::string now = wm.fullscreenCandidate(c, {}) ? c.uuid : std::string();
    if (now == prev) return;
    prev = now;
    if (now.empty())
      std::printf("[%zu ventanas] ya no hay pantalla completa\n", wm.windows().size());
    else
      std::printf("[%zu ventanas] pantalla completa: %s | %s | uuid=%s\n", wm.windows().size(), c.appId.c_str(),
                  c.title.c_str(), c.uuid.c_str());
    fflush(stdout);
  });
  for (;;) wm.waitAndDispatch(1000);
#endif
}

static int cmdPanel(int argc, char **argv) {
  std::string what = argc > 0 ? argv[0] : "status";
  std::string out = flag(argc, argv, "--output");
  double hz = atof(flag(argc, argv, "--hz").c_str());
  if (hz <= 0) hz = 120.0;
  if (!Panel::available()) {
    std::fprintf(stderr, "kscreen-doctor no esta disponible (hace falta KDE/kscreen)\n");
    return 1;
  }
  if (out.empty()) {
    std::string err;
    auto names = Panel::outputs(&err);
    if (names.empty()) {
      std::fprintf(stderr, "%s\n", err.c_str());
      return 1;
    }
    out = names.front();
    if (names.size() > 1)
      std::fprintf(stderr, "usando output %s\n", out.c_str());
  }
  PanelState st;
  std::string err;
  if (!Panel::query(out, st, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  if (what == "status") {
    std::printf("output   : %s\n", st.name.c_str());
    std::printf("  modo   : %d (%s)\n", st.modeIndex, st.modeRes.c_str());
    std::printf("  escala : %g\n", st.scale);
    std::printf("  rotacion: %d (%s)\n", st.rotation, rotW(st.rotation));
    std::printf("  vrr    : %s\n", st.vrr.empty() ? "?" : st.vrr.c_str());
    std::printf("  hdr    : %s\n", st.hdr ? "on" : "off");
    std::printf("  modos  : ");
    for (auto &m : st.modes)
      if (m.second.rfind("3840x2160@", 0) == 0) std::printf("%d:%s  ", m.first, m.second.c_str());
    std::printf("\n");
    std::printf("  objetivo: modo %d, escala 1, rotacion none\n", Panel::pickMode(st, hz));
    return 0;
  }
  if (what == "apply") {
    bool dry = hasFlag(argc, argv, "--dry");
    int mode = Panel::pickMode(st, hz);
    if (!dry) {
      std::string snapPath;
      if (Panel::saveSnapshot(st, &snapPath, &err))
        std::printf("  estado actual guardado en %s (para `ctl panel restore`)\n", snapPath.c_str());
    }
    if (!Panel::apply(out, mode, 1.0, RotNone, true, &err, dry)) {
      std::fprintf(stderr, "%s\n", err.c_str());
      return 1;
    }
    std::printf("%s\n", dry ? err.c_str() : "panel aplicado");
    if (!dry) {
      PanelState after;
      if (Panel::query(out, after))
        std::printf("  ahora: %s escala %g rotacion %d (%s)\n", after.modeRes.c_str(), after.scale, after.rotation, rotW(after.rotation));
    }
    return 0;
  }
  if (what == "snapshot") {
    std::string snapPath;
    if (!Panel::saveSnapshot(st, &snapPath, &err)) {
      std::fprintf(stderr, "%s\n", err.c_str());
      return 1;
    }
    std::printf("estado guardado en %s: modo %d %s, escala %g, rotacion %d (%s)\n", snapPath.c_str(), st.modeIndex,
                st.modeRes.c_str(), st.scale, st.rotation, rotW(st.rotation));
    return 0;
  }
  if (what == "restore") {
    PanelState snap{};
    std::string snapPath;
    if (!Panel::loadSnapshot(snap, &snapPath, &err)) {
      std::fprintf(stderr, "%s\n", err.c_str());
      return 1;
    }
    std::printf("  instantanea: %s  modo %d %s, escala %g, rotacion %d (%s)\n", snapPath.c_str(), snap.modeIndex,
                snap.modeRes.c_str(), snap.scale, snap.rotation, rotW(snap.rotation));
    if (hasFlag(argc, argv, "--dry")) {
      std::printf("  ejecutaría: kscreen-doctor output.%s.mode.%d output.%s.scale.%g", snap.name.c_str(),
                  snap.modeIndex, snap.name.c_str(), snap.scale);
      if (const char *w = Panel::rotationWord(snap.rotation))
        std::printf(" output.%s.rotation.%s", snap.name.c_str(), w);
      std::printf("\n");
      return 0;
    }
    if (!Panel::restore(snap, &err)) {
      std::fprintf(stderr, "%s\n", err.c_str());
      return 1;
    }
    Panel::clearSnapshot();
    std::printf("panel restaurado\n");
    return 0;
  }
  std::fprintf(stderr, "subcomando de panel desconocido: %s\n", what.c_str());
  return 2;
}

/* ---------------- ayudas de opciones y datos ---------------- */

static std::string exeDir() {
  char buf[4096];
  ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
  if (n <= 0) return {};
  buf[n] = 0;
  std::string p = buf;
  size_t s = p.find_last_of('/');
  return s == std::string::npos ? std::string(".") : p.substr(0, s);
}

static bool fileExists(const std::string &p) { return !p.empty() && access(p.c_str(), R_OK) == 0; }

/* Busca un archivo de datos junto al binario, en el directorio actual o en /usr/share. */
static std::string findDataFile(const std::string &name, const std::string &calibDir) {
  std::vector<std::string> bases;
  if (!calibDir.empty()) bases.push_back(calibDir);
  for (const char *b : {".", "./calib", "../calib"}) bases.push_back(b);
  std::string ed = exeDir();
  if (!ed.empty()) {
    bases.push_back(ed + "/calib");
    bases.push_back(ed + "/../share/odyssey3d/calib");
  }
  bases.push_back("/usr/share/odyssey3d/calib");
  for (const std::string &b : bases) {
    std::string p = b + "/" + name;
    if (fileExists(p)) return p;
  }
  return {};
}

static bool parseViewerFlags(int argc, char **argv, ViewerOpts &o) {
  for (int i = 0; i < argc; i++) {
    std::string a = argv[i];
    auto need = [&](const char *what) {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "falta el valor de %s\n", a.c_str());
        return false;
      }
      return true;
    };
    if (a == "--output") { if (!need("output")) return false; o.output = argv[++i]; }
    else if (a == "--hz") { if (!need("hz")) return false; o.hz = atof(argv[++i]); }
    else if (a == "--w") { if (!need("w")) return false; o.w = atoi(argv[++i]); }
    else if (a == "--h") { if (!need("h")) return false; o.h = atoi(argv[++i]); }
    else if (a == "--windowed") o.windowed = true;
    else if (a == "--no-panel") o.noPanel = true;
    else if (a == "--no-lens") o.noLens = true;
    else if (a == "--no-scaler") o.noScaler = true;
    else if (a == "--2d") o.no3d = true;
    else if (a == "--tb" || a == "--top-bottom") o.topBottom = true;
    else if (a == "--no-corr") o.noCorr = true;
    else if (a == "--solid") o.solid = true;
    else if (a == "--out") { if (!need("out")) return false; o.outPath = argv[++i]; }
    else if (a == "--dump") { if (!need("dump")) return false; o.dumpPath = argv[++i]; }
    else if (a == "--hwdec") { if (!need("hwdec")) return false; setenv("ODYSSEY_HWDEC", argv[++i], 1); }
    else if (a == "--eye-x") { if (!need("eye-x")) return false; o.eyeX = atof(argv[++i]); }
    else if (a == "--eye-y") { if (!need("eye-y")) return false; o.eyeY = atof(argv[++i]); }
    else if (a == "--eye-z") { if (!need("eye-z")) return false; o.eyeZ = atof(argv[++i]); }
    else if (a == "--align") { if (!need("align")) return false; o.align = atof(argv[++i]); }
    else if (a == "--pitch") { if (!need("pitch")) return false; o.pitch = atof(argv[++i]); }
    else if (a == "--calib-dir") { if (!need("calib-dir")) return false; o.corrDir = argv[++i]; }
    else if (a == "--help" || a == "-h") { std::fputs(kUsage, stdout); exit(0); }
    else if (!a.empty() && a[0] == '-') { std::fprintf(stderr, "opcion desconocida: %s\n", a.c_str()); return false; }
    else if (o.file.empty()) o.file = a;
    else { std::fprintf(stderr, "solo se admite un archivo; ignorando %s\n", a.c_str()); }
  }
  if (!o.corrDir.empty()) {
    std::string dir = o.corrDir;
    o.corrA = dir + "/3DStackCorrection_A.png";
    o.corrB = dir + "/3DStackCorrection_B.png";
    if (!fileExists(o.corrA)) { std::fprintf(stderr, "no existe %s\n", o.corrA.c_str()); return false; }
  } else {
    o.corrA = findDataFile("3DStackCorrection_A.png", {});
    o.corrB = findDataFile("3DStackCorrection_B.png", {});
    if (!fileExists(o.corrA) || !fileExists(o.corrB)) {
      std::fprintf(stderr, "aviso: no se encontro el mapa de correccion del panel; se usara correccion neutra\n");
      o.corrA.clear(); o.corrB.clear();
    }
  }
  return true;
}

/* ---------------- atributos de la trama (para validar el modelo) ---------------- */

static int cmdAttrs(const ViewerOpts &o) {
  Weave w; /* fill() no necesita contexto GL */
  w.setPanelSize(o.w > 0 ? o.w : 3840, o.h > 0 ? o.h : 2160);
  w.setEye(EyePose{o.eyeX, o.eyeY, o.eyeZ});
  const LensField f{};
  static const double ndc[4][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
  static const char *name[4] = {"TL", "TR", "BL", "BR"};
  std::printf("ojo = %.4f %.4f %.4f cm, panel %dx%d\n", o.eyeX, o.eyeY, o.eyeZ, o.w > 0 ? o.w : 3840,
              o.h > 0 ? o.h : 2160);
  std::printf("kx=%.10g ky=%.10g c00=%.10g,%.10g,%.10g px=%.10g slant=%.10g ezEffRef=%.10g fxOverFy=%.10g\n", f.kx,
              f.ky, f.c00[0], f.c00[1], f.c00[2], f.pxPcm, f.slant, f.ezEffRef, f.fxOverFy);
  for (int i = 0; i < 4; i++) {
    Weave::CornerAttrs a = w.fill(ndc[i][0], ndc[i][1]);
    std::printf("%s v2=%.10g,%.10g v3=%.10g,%.10g v4=%.10g,%.10g,%.10g\n", name[i], a.v2[0], a.v2[1], a.v3[0],
                a.v3[1], a.v4[0], a.v4[1], a.v4[2]);
  }
  return 0;
}

/* ---------------- tramo calculado en fuera de pantalla (sin tocar el panel) ---------------- */

static int cmdWeavePng(const ViewerOpts &o) {
  int W = o.w > 0 ? o.w : 3840, H = o.h > 0 ? o.h : 2160;
  if (!glfwInit()) {
    std::fprintf(stderr, "[error] glfwInit fallo\n");
    return 1;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  GLFWwindow *win = glfwCreateWindow(64, 64, "odyssey3d-weave", nullptr, nullptr);
  if (!win) {
    std::fprintf(stderr, "[error] no se pudo crear un contexto GL\n");
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(win);
  std::printf("[gl] %s / %s\n", (const char *)glGetString(GL_VERSION), (const char *)glGetString(GL_RENDERER));

  std::string err;
  Weave weave;
  if (!weave.init(&err)) {
    std::fprintf(stderr, "[error] shader de trama: %s\n", err.c_str());
    glfwDestroyWindow(win);
    glfwTerminate();
    return 1;
  }
  weave.setPanelSize(W, H);
  weave.setEye(EyePose{o.eyeX, o.eyeY, o.eyeZ});
  WeaveTuning t{};
  t.align = o.align;
  t.pitch = o.pitch;
  t.weave = !o.no3d;
  t.topBottom = o.topBottom;
  weave.setTuning(t);
  if (!o.noCorr) {
    std::string a = o.corrA.empty() ? findDataFile("3DStackCorrection_A.png", o.corrDir) : o.corrA;
    std::string b = o.corrB.empty() ? findDataFile("3DStackCorrection_B.png", o.corrDir) : o.corrB;
    if (!a.empty() && !b.empty())
      weave.setCorrectionsFromPng(a, b);
    else
      std::printf("[aviso] sin mapas de correccion del panel (se usan neutros)\n");
  } else {
    std::printf("[aviso] mapas de correccion desactivados (neutros)\n");
  }

  GLuint tex = 0;
  TestSrc pat;
  bool havePat = false;
  if (o.solid) {   /* mitad izquierda del archivo roja, derecha verde: la trama debe alternarlas */
    const uint8_t px[16] = {255, 0, 0, 255, 0, 255, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255};
    tex = glTexFromRGBA(2, 2, px, false);
  } else if (!pat.init(&err)) {
    std::fprintf(stderr, "[error] patron: %s\n", err.c_str());
    weave.destroy();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 1;
  } else {
    havePat = true;
    pat.createFbo(W, H);
    tex = pat.render(0.0, 0, 0);
  }
  Fbo dst;
  dst.create(W, H);
  dst.bind();
  weave.refreshCorners();
  weave.draw(tex);
  glCheckErr("weave.draw");
  std::vector<uint8_t> buf((size_t)W * (size_t)H * 4u);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
  glPixelStorei(GL_PACK_ALIGNMENT, 4);
  std::vector<uint8_t> top((size_t)W * (size_t)H * 4u);
  for (int y = 0; y < H; y++)
    std::memcpy(&top[(size_t)y * W * 4u], &buf[(size_t)(H - 1 - y) * W * 4u], (size_t)W * 4u);
  std::string out = o.outPath.empty() ? "trama.png" : o.outPath;
  bool ok = glPngSave(out, W, H, top.data());
  std::printf("[trama] %s (%dx%d)%s\n", out.c_str(), W, H, ok ? "" : "  <-- fallo al escribir");
  dst.destroy();
  if (havePat) pat.destroy();
  weave.destroy();
  glfwDestroyWindow(win);
  glfwTerminate();
  return ok ? 0 : 1;
}

/* ---------------- diagnostico ---------------- */

static int cmdDoctor(const ViewerOpts &o, const std::string &waylandSock) {
  int problems = 0;
  std::string s;
  std::printf("== sesion ==\n");
  std::printf("  WAYLAND_DISPLAY : %s\n", getenv("WAYLAND_DISPLAY") ? getenv("WAYLAND_DISPLAY") : "(sin definir)");
  if (!getenv("WAYLAND_DISPLAY") && !waylandSock.empty()) std::printf("  (detectado: %s)\n", waylandSock.c_str());
  std::printf("  XDG_RUNTIME_DIR : %s\n", getenv("XDG_RUNTIME_DIR") ? getenv("XDG_RUNTIME_DIR") : "(sin definir)");
  std::printf("  XDG_SESSION_TYPE: %s\n", getenv("XDG_SESSION_TYPE") ? getenv("XDG_SESSION_TYPE") : "(sin definir)");
  if (!getenv("WAYLAND_DISPLAY")) { std::printf("  ! estas fuera de una sesion Wayland\n"); problems++; }

  std::printf("== lente lenticular ==\n");
  std::string port = Lens::findPort();
  if (port.empty()) {
    std::printf("  ! no aparece USB 354b:0116 (¿cable USB-B del monitor desconectado?)\n");
    problems++;
  } else {
    std::printf("  puerto : %s  (accesible: %s)\n", port.c_str(), access(port.c_str(), R_OK | W_OK) == 0 ? "si" : "NO");
    Lens lens;
    if (!lens.open(port)) {
      std::printf("  ! %s\n", lens.lastError().c_str());
      problems++;
    } else {
      for (auto &kv : lens.info()) std::printf("  %-10s: %s\n", kv.first.c_str(), kv.second.c_str());
      std::printf("  %-10s: %s\n", "auth", lens.authenticated() ? "si" : "no");
      std::printf("  %-10s: %s\n", "estado", lens.state() ? "3D" : "2D");
    }
  }

  std::printf("== scaler del monitor ==\n");
  std::string dev = Scaler::findDevice();
  if (dev.empty()) {
    std::printf("  ! no aparece el disco SCSI vendor GCREADER\n");
    problems++;
  } else {
    std::printf("  dispositivo : %s  (accesible: %s)\n", dev.c_str(), access(dev.c_str(), R_OK | W_OK) == 0 ? "si" : "NO");
  }

  std::printf("== camara de seguimiento ==\n");
  std::vector<std::string> cams = IrCam::findDevices();
  if (cams.empty()) {
    std::printf("  ! no aparece la camara 04e8:20d7 (3D Stereo WebCam)\n");
    problems++;
  } else {
    for (const std::string &c : cams)
      std::printf("  %s  (accesible: %s)\n", c.c_str(), access(c.c_str(), R_OK | W_OK) == 0 ? "si" : "NO");
  }

  std::printf("== panel ==\n");
  if (!Panel::available()) {
    std::printf("  ! kscreen-doctor no esta instalado (kmlload/kscreen): no se puede cambiar el modo\n");
  } else {
    std::string name = Panel::resolveOutput(o.output);
    PanelState st{};
    std::string err;
    if (name.empty() || !Panel::query(name, st, &err)) {
      std::printf("  ! no se pudo leer el estado del panel\n");
      problems++;
    } else {
      std::printf("  salida        : %s\n", st.name.c_str());
      std::printf("  modo activo   : %d %s\n", st.modeIndex, st.modeRes.c_str());
      std::printf("  escala        : %g  (la trama necesita 1)\n", st.scale);
      std::printf("  rotacion      : %s  (la trama necesita normal)\n", rotW(st.rotation));
      std::printf("  HDR           : %s\n", st.hdr ? "activado (se apagara al reproducir)" : "apagado");
      std::printf("  sincron. adap.: %s\n", st.vrr.empty() ? "(sin dato)" : st.vrr.c_str());
      int idx = Panel::pickMode(st, o.hz);
      std::printf("  modo elegido  : %s\n", idx < 0 ? "(ninguno 3840x2160)" : Panel::modeName(st, idx).c_str());
      if (st.scale != 1.0 || !Panel::isLandscape(st.rotation)) { std::printf("  ! el panel no esta a pixel a pixel: usa `odyssey3d ctl panel apply`\n"); }
    }
  }

  std::printf("== datos ==\n");
  std::string a = findDataFile("3DStackCorrection_A.png", o.corrDir);
  std::string b = findDataFile("3DStackCorrection_B.png", o.corrDir);
  std::printf("  correccion A  : %s\n", a.empty() ? "(no encontrada: correccion neutra)" : a.c_str());
  std::printf("  correccion B  : %s\n", b.empty() ? "(no encontrada: correccion neutra)" : b.c_str());
  std::printf("\n%s\n", problems ? "hay problemas: revisa las lineas con !" : "todo lo esencial esta presente");
  return problems ? 1 : 0;
}

/* ---------------- servicio en segundo plano ---------------- */

static volatile sig_atomic_t g_stopServe = 0;
static void onServeSignal(int) { g_stopServe = 1; }

struct ServeCfg {
  std::vector<std::string> ignore;
  int esperaSeg = 40;
  std::string modo = "ask";   /* ask = preguntar con un aviso; sbs/tb/flat = arrancar sin preguntar */
};

static void splitList(const std::string &text, std::vector<std::string> &out) {
  std::string item;
  for (char c : text) {
    if (c == ',' || c == ' ' || c == '\t') {
      if (!item.empty()) out.push_back(strLower(strTrim(item)));
      item.clear();
    } else {
      item += c;
    }
  }
  if (!item.empty()) out.push_back(strLower(strTrim(item)));
}

/* ~/.config/odyssey3d/serve.conf: lineas clave=valor (ignore=, espera=, modo=). */
static void loadServeCfg(ServeCfg &cfg) {
  const char *home = std::getenv("HOME");
  if (!home) return;
  std::string txt;
  if (!readSmallFile(std::string(home) + "/.config/odyssey3d/serve.conf", txt)) return;
  std::string line;
  auto eat = [&](bool last) {
    std::string l = strTrim(line);
    line.clear();
    if (l.empty() || l[0] == '#') return;
    size_t eq = l.find('=');
    if (eq == std::string::npos) return;
    std::string k = strLower(strTrim(l.substr(0, eq))), v = strTrim(l.substr(eq + 1));
    if (k == "ignore") splitList(v, cfg.ignore);
    else if (k == "espera") cfg.esperaSeg = std::max(5, std::atoi(v.c_str()));
    else if (k == "modo") cfg.modo = strLower(v);
    (void)last;
  };
  for (char c : txt) {
    if (c == '\n') eat(false);
    else line += c;
  }
  eat(true);
}

static std::string selfPath() {
  char buf[4096];
  ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0) return "odyssey3d";
  buf[n] = 0;
  return buf;
}

static bool ignoredApp(const WindowInfo &w, const std::vector<std::string> &list) {
  std::string app = strLower(w.appId), title = strLower(w.title);
  if (title.find("odyssey 3d") != std::string::npos) return true;
  for (const std::string &ig : list) {
    if (ig.empty()) continue;
    if (app == ig || app.find(ig) != std::string::npos) return true;
  }
  return false;
}

static pid_t spawnViewer(const std::string &exe, const std::string &uuid, const std::string &modo) {
  pid_t pid = ::fork();
  if (pid != 0) return pid;
  std::vector<std::string> args = {exe, "capture3d", "--uuid", uuid};
  if (modo == "tb") args.push_back("--tb");
  if (modo == "flat") args.push_back("--2d");
  std::vector<char *> av;
  for (std::string &a : args) av.push_back(const_cast<char *>(a.c_str()));
  av.push_back(nullptr);
  ::setenv("GLFW_APPLICATION_ID", "odyssey3d", 1);
  ::execv(exe.c_str(), av.data());
  ::_exit(127);
}

static int cmdServe(int argc, char **argv) {
#ifndef HAVE_WAYLAND_WM
  (void)argc; (void)argv;
  std::fprintf(stderr, "compilado sin soporte de ventanas Wayland (hace falta wayland-client)\n");
  return 1;
#else
  ServeCfg cfg;
  cfg.ignore = {"odyssey3d", "plasmashell", "plasma", "kscreen", "polkit", "xwayland", "krunner", "dolphin"};
  loadServeCfg(cfg);
  bool dry = hasFlag(argc, argv, "--dry");
  bool unaVez = hasFlag(argc, argv, "--once");
  if (std::string v = flag(argc, argv, "--espera"); !v.empty()) cfg.esperaSeg = std::max(5, std::atoi(v.c_str()));
  if (std::string v = flag(argc, argv, "--modo"); !v.empty()) cfg.modo = strLower(v);
  if (std::string v = flag(argc, argv, "--ignore"); !v.empty()) splitList(v, cfg.ignore);
  if (cfg.modo == "preguntar") cfg.modo = "ask";

  ensureWaylandEnv();
  std::signal(SIGTERM, onServeSignal);
  std::signal(SIGINT, onServeSignal);
  std::signal(SIGPIPE, SIG_IGN);

  WindowMonitor wm;
  std::string err;
  if (!wm.open(&err)) {
    std::fprintf(stderr, "[serve] %s\n", err.c_str());
    return 1;
  }
  Notifier nt;
  bool hayAvisos = nt.open(&err);
  if (!hayAvisos) std::fprintf(stderr, "[serve] sin portal de avisos: %s\n", err.c_str());
  if (cfg.modo == "ask" && !hayAvisos) {
    std::fprintf(stderr, "[serve] para preguntar hacen falta las notificaciones; usa --modo sbs|tb|flat\n");
    return 1;
  }

  const std::string exe = selfPath();
  std::map<std::string, std::string> idUuid;   /* id del aviso -> uuid de la ventana */
  std::set<std::string> preguntadas;           /* ventanas por las que ya preguntamos o actuamos */
  std::string avisoId, avisoUuid, accion;      /* aviso pendiente y respuesta del usuario */
  double avisoT = 0.0;
  pid_t hijo = -1;
  std::string hijoUuid;

  nt.setOnAction([&](const std::string &id, const std::string &action) {
    auto it = idUuid.find(id);
    if (it == idUuid.end()) return;
    accion = strLower(action);
    avisoUuid = it->second;
    std::printf("[serve] respuesta %s -> %s\n", action.c_str(), it->second.c_str());
    std::fflush(stdout);
  });

  std::printf("[serve] vigilando ventanas a pantalla completa (%s) · modo=%s · espera=%d s\n",
              dry ? "sin ejecutar, --dry" : "ejecutando", cfg.modo.c_str(), cfg.esperaSeg);
  std::fflush(stdout);

  while (!g_stopServe) {
    wm.waitAndDispatch(500);
    if (hayAvisos) {
      nt.process();
      nt.flush();
    }
    double now = 0.0;
    {
      struct timespec ts;
      clock_gettime(CLOCK_MONOTONIC, &ts);
      now = (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
    }

    if (hijo > 0) {
      int st = 0;
      pid_t r = ::waitpid(hijo, &st, WNOHANG);
      if (r == hijo) {
        std::printf("[serve] sesion 3D terminada (%s)\n", hijoUuid.c_str());
        std::fflush(stdout);
        hijo = -1;
        hijoUuid.clear();
      }
    }

    /* el aviso caduca: si nadie pulsa, se quita y no se vuelve a preguntar por esa ventana */
    if (!avisoId.empty() && now - avisoT > (double)cfg.esperaSeg) {
      std::printf("[serve] aviso sin respuesta, se retira (%s)\n", avisoUuid.c_str());
      std::fflush(stdout);
      nt.remove(avisoId);
      idUuid.erase(avisoId);
      avisoId.clear();
      avisoUuid.clear();
    }

    if (!accion.empty()) {
      std::string act = accion;
      accion.clear();
      if (!avisoId.empty()) {
        nt.remove(avisoId);
        idUuid.erase(avisoId);
        avisoId.clear();
      }
      if (act != "no" && act != "cancel" && act != "close" && hijo < 0) {
        if (dry) {
          std::printf("[serve] (dry) habria lanzado capture3d --uuid %s modo=%s\n", avisoUuid.c_str(), act.c_str());
          std::fflush(stdout);
          if (unaVez) break;
        } else {
          hijo = spawnViewer(exe, avisoUuid, act);
          hijoUuid = avisoUuid;
          std::printf("[serve] lanzando 3D (%s) para %s\n", act.c_str(), hijoUuid.c_str());
          std::fflush(stdout);
          if (hijo > 0 && unaVez) break;
        }
      }
      avisoUuid.clear();
    }

    /* podar: si la ventana dejó de estar a pantalla completa o se cerró, se olvida */
    if (!preguntadas.empty()) {
      std::set<std::string> vivas;
      for (const WindowInfo &w : wm.windows())
        if (w.usable()) vivas.insert(w.uuid);
      for (auto it = preguntadas.begin(); it != preguntadas.end();) {
        if (vivas.count(*it) == 0) it = preguntadas.erase(it);
        else ++it;
      }
    }

    if (hijo < 0 && avisoId.empty()) {
      WindowInfo w{};
      if (wm.fullscreenCandidate(w, {}) && !ignoredApp(w, cfg.ignore) && !preguntadas.count(w.uuid)) {
        preguntadas.insert(w.uuid);
        if (cfg.modo == "ask") {
          std::string id = "odyssey3d-" + w.uuid.substr(0, 8);
          std::vector<std::pair<std::string, std::string>> botones = {
              {"sbs", "3D lado a lado"}, {"tb", "3D arriba y abajo"}, {"flat", "2D plano"}};
          std::string titulo = "¿Poner en 3D?";
          std::string cuerpo = w.title.empty() ? w.appId : w.title;
          cuerpo += "  ·  " + std::to_string(w.w) + "x" + std::to_string(w.h);
          if (nt.send(id, titulo, cuerpo, botones, &err)) {
            idUuid[id] = w.uuid;
            avisoId = id;
            avisoUuid = w.uuid;
            avisoT = now;
            std::printf("[serve] avisando por %s | %s (%s)\n", w.appId.c_str(), w.title.c_str(), w.uuid.c_str());
            std::fflush(stdout);
          } else {
            std::fprintf(stderr, "[serve] aviso rechazado: %s\n", err.c_str());
            std::fflush(stderr);
          }
        } else if (dry) {
          std::printf("[serve] (dry) habria lanzado modo %s por %s | %s\n", cfg.modo.c_str(), w.appId.c_str(),
                      w.title.c_str());
          std::fflush(stdout);
          if (unaVez) break;
        } else {
          hijo = spawnViewer(exe, w.uuid, cfg.modo);
          hijoUuid = w.uuid;
          std::printf("[serve] lanzando 3D (%s) por %s | %s\n", cfg.modo.c_str(), w.appId.c_str(), w.title.c_str());
          std::fflush(stdout);
          if (hijo > 0 && unaVez) break;
        }
      }
    }
  }

  if (!avisoId.empty()) nt.remove(avisoId);
  if (hijo > 0) {
    ::kill(hijo, SIGTERM);
    int st = 0;
    ::waitpid(hijo, &st, 0);
  }
  std::printf("[serve] detenido\n");
  std::fflush(stdout);
  return 0;
#endif
}

int main(int argc, char **argv) {
  if (argc < 2) {
    std::fputs(kUsage, stdout);
    return 2;
  }
  /* Las órdenes de panel hablán con KWin por Wayland: aseguramos el socket en el entorno. */
  const std::string waylandSocket = ensureWaylandEnv();

  std::string cmd = argv[1];
  if (cmd == "-h" || cmd == "--help" || cmd == "help") {
    std::fputs(kUsage, stdout);
    return 0;
  }
  if (cmd == "ctl") {
    if (argc < 3) {
      std::fputs(kUsage, stdout);
      return 2;
    }
    std::string sub = argv[2];
    if (sub == "lens") return cmdLens(argc - 3, argv + 3);
    if (sub == "scaler") return cmdScaler(argc - 3, argv + 3);
    if (sub == "ir") return cmdIr(argc - 3, argv + 3);
    if (sub == "panel") return cmdPanel(argc - 3, argv + 3);
    if (sub == "windows") return cmdWindows(argc - 3, argv + 3);
    if (sub == "notify") return cmdNotify(argc - 3, argv + 3);
    if (sub == "capture") return cmdCapture(argc - 3, argv + 3);
    if (sub == "capture-gl") return cmdCaptureGl(argc - 3, argv + 3);
    std::fprintf(stderr, "subcomando desconocido: %s\n", sub.c_str());
    return 2;
  }
  if (cmd == "attrs") {
    ViewerOpts o{};
    if (!parseViewerFlags(argc - 2, argv + 2, o)) return 2;
    return cmdAttrs(o);
  }
  if (cmd == "weave") {
    ViewerOpts o{};
    if (!parseViewerFlags(argc - 2, argv + 2, o)) return 2;
    return cmdWeavePng(o);
  }
  if (cmd == "--version" || cmd == "-v" || cmd == "version") {
    std::printf("odyssey3d %s\n", ODYSSEY3D_VERSION);
    return 0;
  }
  if (cmd == "serve") return cmdServe(argc - 1, argv + 1);
  if (cmd == "capture3d") {
#ifndef HAVE_CAPTURE
    (void)argc; (void)argv;
    std::fprintf(stderr, "compilado sin soporte de captura (hace falta libpipewire-0.3 y plasma-wayland-protocols)\n");
    return 1;
#else
    ViewerOpts o{};
    if (!parseViewerFlags(argc - 2, argv + 2, o)) return 2;
    std::string uuid;
    WindowInfo wi{};
    bool haveWin = false;
    if (!pickWindow(argc - 2, argv + 2, uuid, wi, haveWin)) return 1;
    o.captureUuid = uuid;
    return runViewer(o, SourceKind::Capture);
#endif
  }
  if (cmd == "test" || cmd == "play" || cmd == "doctor") {
    ViewerOpts o{};
    if (!parseViewerFlags(argc - 2, argv + 2, o)) return 2;
    if (cmd == "doctor") return cmdDoctor(o, waylandSocket);
    if (cmd == "play") {
      if (o.file.empty()) {
        std::fprintf(stderr, "falta el archivo: odyssey3d play ARCHIVO\n");
        return 2;
      }
      return runViewer(o, SourceKind::Mpv);
    }
    return runViewer(o, SourceKind::Test);
  }
  std::fputs(kUsage, stderr);
  return 2;
}
