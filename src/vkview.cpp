/* vkview.cpp — el visor por Vulkan (ver viewer.h). GLFW sólo pone la ventana, la entrada y el
   cursor; todo lo demás se dibuja con Vulkan: la trama a pantalla completa y encima la interfaz
   estilo Odyssey 3D Hub. El fotograma del compositor se muestrea desde su DMA-BUF, sin copia por
   la CPU. */
#define GLFW_INCLUDE_VULKAN 1  /* antes de cualquier cabecera que traiga GLFW */

#include "viewer.h"

#ifdef HAVE_CAPTURE
#include "capture.h"
#endif
#include "glutil.h"
#include "session.h"
#include "util.h"
#include "vkui.h"
#include "vkweave.h"

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static void vkGlfwError(int code, const char *desc) { std::fprintf(stderr, "[glfw %d] %s\n", code, desc); }

namespace {

struct VkUi {
  WeaveTuning tune{};
  Hud hud;
  bool quit = false;
  float convBase = 0.0f;  /* la convergencia que mueven las teclas; la del deslizador se suma */
  double mx = -1.0, my = -1.0;
  Hud::Hit dragging = Hud::HitNone;
  int fbW = 0, fbH = 0;
};

void printTuning(const VkUi &ui, const char *what) {
  std::printf("[%-11s] weave=%d swap=%d fs=%5.2f align=%+.4f pitch=%.5f xtdyn=%+.4f conv=%+.5f prof=%d rel=%d\n",
              what, (int)ui.tune.weave, (int)ui.tune.swapLR, ui.tune.filterSlope, ui.tune.align, ui.tune.pitch,
              ui.tune.xtalkDyn, ui.tune.conv, ui.hud.state().depth, ui.hud.state().popout);
  std::fflush(stdout);
}

/* frase breve bajo la pastilla: qué se está viendo y en qué estado está el hardware */
std::string infoLine(SourceKind kind, const Rig &rig, const Hud::State &st, int w, int h) {
  char buf[192];
  std::snprintf(buf, sizeof buf, "%dx%d  %s  %s  %s", w, h, kind == SourceKind::Capture ? "ventana" : "prueba",
                st.weaveOn ? (st.ask ? "" : "") : "", "");
  std::string out = buf;
  out = kind == SourceKind::Capture ? "ventana capturada " : "patron de prueba ";
  out += st.weaveOn ? (st.ask ? "sin elegir" : (st.hud >= 0 ? "" : "")) : "2D plano";
  if (st.weaveOn) out += " · tramado ";
  out += " · lente ";
  out += (rig.lensOk ? "3D" : "2D");
  out += " · scaler ";
  out += (rig.scalerOn ? "3D" : "apagado");
  return out;
}

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

void onKeyVK(GLFWwindow *win, int key, int, int action, int mods) {
  if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
  VkUi *ui = (VkUi *)glfwGetWindowUserPointer(win);
  if (!ui) return;
  if (key == GLFW_KEY_ESCAPE) {
    ui->quit = true;
    glfwSetWindowShouldClose(win, GLFW_TRUE);
    return;
  }
  /* atajos del hub oficial: Ctrl+Shift+1 3D · 2 superposición · 3/4 profundidad · 5/6 relieve */
  if ((mods & GLFW_MOD_CONTROL) && (mods & GLFW_MOD_SHIFT)) {
    const char *tag = nullptr;
    switch (key) {
      case GLFW_KEY_1: ui->hud.toggle3D(); tag = "3D/2D"; break;
      case GLFW_KEY_2: ui->hud.toggleOverlay(); tag = "superposicion"; break;
      case GLFW_KEY_3: ui->hud.nudgeDepth(-5); tag = "profundidad -"; break;
      case GLFW_KEY_4: ui->hud.nudgeDepth(+5); tag = "profundidad +"; break;
      case GLFW_KEY_5: ui->hud.nudgePopout(-5); tag = "relieve -"; break;
      case GLFW_KEY_6: ui->hud.nudgePopout(+5); tag = "relieve +"; break;
      default: break;
    }
    if (tag) printTuning(*ui, tag);
    return;
  }
  const char *tag = nullptr;
  if (tuningKey(ui->tune, key, &tag)) {
    if (key == GLFW_KEY_0 || key == GLFW_KEY_9) ui->convBase = ui->tune.conv; /* la tecla gana sobre el deslizador */
    if (tag) printTuning(*ui, tag);
  }
}

void onCursorVK(GLFWwindow *win, double x, double y) {
  VkUi *ui = (VkUi *)glfwGetWindowUserPointer(win);
  if (!ui) return;
  ui->mx = x;
  ui->my = y;
  ui->hud.setMouse((float)x, (float)y);
  if (ui->dragging != Hud::HitNone) ui->hud.dragTo(ui->dragging, (float)x);
}

void onButtonVK(GLFWwindow *win, int button, int action, int) {
  VkUi *ui = (VkUi *)glfwGetWindowUserPointer(win);
  if (!ui || button != GLFW_MOUSE_BUTTON_LEFT) return;
  if (action == GLFW_RELEASE) {
    ui->dragging = Hud::HitNone;
    return;
  }
  Hud::Hit hit = ui->hud.hit((float)ui->mx, (float)ui->my);
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

  GLFWwindow *win = glfwCreateWindow(W, H, "Odyssey 3D", opts.windowed ? nullptr : mon, nullptr);
  if (!win) {
    const char *desc = nullptr;
    int code = glfwGetError(&desc);
    std::fprintf(stderr, "[error] no se pudo crear la ventana (glfw %d): %s\n", code, desc ? desc : "?");
    rig.shutDown();
    glfwTerminate();
    return 1;
  }
  if (!opts.windowed) glfwSetWindowMonitor(win, mon, 0, 0, W, H, hz);
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
  ui.tune.weave = !opts.no3d;
  ui.tune.topBottom = opts.topBottom;
  ui.tune.align = (float)opts.align;
  ui.tune.pitch = (float)opts.pitch;
  ui.hud.state().weaveOn = ui.tune.weave;
  ui.hud.state().ask = (kind == SourceKind::Capture) && !opts.noHud;
  ui.hud.state().overlayOn = !opts.noHud;
  glfwSetWindowUserPointer(win, &ui);
  glfwSetKeyCallback(win, onKeyVK);
  glfwSetCursorPosCallback(win, onCursorVK);
  glfwSetMouseButtonCallback(win, onButtonVK);
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
  bool haveCap = false;
  bool sizeWarned = false, importWarned = false;
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
  }
#endif

  std::vector<uint8_t> testPx;
  bool haveTest = false;
  bool testAnim = (size_t)fbW * (size_t)fbH <= (size_t)2000000; /* en 4K el patrón se dibuja una vez */
  double animNext = 0.0;

  std::printf("\n  teclas  ESC salir · 3 2D/3D · F invertir profundidad · ←/→ fase · RePag/SavPg paso de lente\n"
              "          ↑/↓ crosstalk · , . pendiente de filtro · 9/0 convergencia · 8 reiniciar ajuste\n");
  std::printf("          raton  botones y deslizadores de la pastilla · Ctrl+Shift 1..6 como el hub oficial\n\n");
  std::fflush(stdout);

  double t0 = glfwGetTime(), clock = t0, lastPoll = 0.0, ultimoAviso = 0.0;
  int frames = 0, errStreak = 0, reintentos = 0;
  bool dumpHecho = false;
  while (!glfwWindowShouldClose(win) && !ui.quit) {
    glfwPollEvents();
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
            std::fprintf(stderr, "[captura] %s\n", err.c_str());
          }
          newSource = true;
        }
      }
      if (now - lastPoll > 1.0) {
        lastPoll = now;
        if (!cap.streamCreated()) {
          std::fprintf(stderr, "[captura] %s\n", cap.describe().c_str());
        } else if (lastFrameCount == 0 && now - t0 > 5.0 && reintentos < 3) {
          /* el nodo se creo pero no llega nada: cambia el modo del panel y KWin invalida la
             sesion, o el portal tarda. Tira del flujo y pidelo otra vez. */
          ++reintentos;
          std::printf("[captura] el flujo no arranca, pidiendolo otra vez (intento %d)\n", reintentos);
          cap.stop();
          std::string e2;
          if (!cap.start(opts.captureUuid, &e2)) std::fprintf(stderr, "[captura] reintentar: %s\n", e2.c_str());
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
    (void)newSource;

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
    ui.hud.state().hud = rig.hud();
    ui.hud.setInfo(infoLine(kind, rig, ui.hud.state(), fbW, fbH));
    ui.hud.begin(fbW, fbH);
    wv.setHud(ui.hud.overlayVisible() ? ui.hud.vertices() : std::vector<HudVertex>{});
    wv.setHudState(rig.hud());

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
  rig.shutDown();
  glfwTerminate();
  return 0;
}
