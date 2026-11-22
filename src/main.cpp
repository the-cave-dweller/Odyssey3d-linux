/* odyssey3d — visor/reproductor 3D sin gafas para el Samsung Odyssey 3D (G90XF)
   en Wayland. Parte de control de hardware: lente lenticular, scaler MediaTek,
   iluminador IR y modo del panel. */
#include "ircam.h"
#include "lens.h"
#include "panel.h"
#include "scaler.h"
#include "testsrc.h"
#include "util.h"
#include "viewer.h"
#include "weave.h"

#include <GLFW/glfw3.h>

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
