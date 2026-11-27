#include "viewer.h"

#include "glutil.h"
#include "lens.h"
#include "mpvsrc.h"
#ifdef HAVE_CAPTURE
#include "capture.h"
#include "glimport.h"
#endif
#include "panel.h"
#include "scaler.h"
#include "session.h"
#include "testsrc.h"
#include "util.h"

#include <GLFW/glfw3.h>

#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

static void glfwError(int code, const char *desc) { std::fprintf(stderr, "[glfw %d] %s\n", code, desc); }

/* ---------------- teclas ---------------- */

struct Ui {
  WeaveTuning tune{};
  MpvSrc *mpv = nullptr;
  bool warned = false;
  bool sizeWarned = false;
  bool quit = false;
};

static void printTuning(const Ui &ui, const char *what) {
  std::printf("[%-11s] weave=%d swap=%d fs=%5.2f align=%+.4f pitch=%.5f xtdyn=%+.4f conv=%+.5f\n", what,
              (int)ui.tune.weave, (int)ui.tune.swapLR, ui.tune.filterSlope, ui.tune.align, ui.tune.pitch,
              ui.tune.xtalkDyn, ui.tune.conv);
  std::fflush(stdout);
}

static void onKey(GLFWwindow *win, int key, int, int action, int) {
  if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
  Ui *ui = (Ui *)glfwGetWindowUserPointer(win);
  if (!ui) return;
  const char *tag = nullptr;
  if (key == GLFW_KEY_ESCAPE) {
    ui->quit = true;
    glfwSetWindowShouldClose(win, GLFW_TRUE);
    return;
  }
  tuningKey(ui->tune, key, &tag);
  if (ui->mpv) {
    switch (key) {
      case GLFW_KEY_SPACE:
        ui->mpv->togglePause();
        tag = "pausa";
        break;
      case GLFW_KEY_MINUS:
        ui->mpv->seek(ui->mpv->time() - 5.0);
        tag = "-5 s";
        break;
      case GLFW_KEY_EQUAL:
        ui->mpv->seek(ui->mpv->time() + 5.0);
        tag = "+5 s";
        break;
      case GLFW_KEY_LEFT_BRACKET:
        ui->mpv->frameStep(-1);
        tag = "cuadro -";
        break;
      case GLFW_KEY_RIGHT_BRACKET:
        ui->mpv->frameStep(1);
        tag = "cuadro +";
        break;
      default:
        break;
    }
  }
  if (tag) printTuning(*ui, tag);
}

/* ---------------- ventana ---------------- */

static void saveRgbaFlipped(const std::string &path, int w, int h, const uint8_t *bottomFirst) {
  std::vector<uint8_t> out((size_t)w * (size_t)h * 4u);
  for (int y = 0; y < h; y++)
    std::memcpy(&out[(size_t)y * (size_t)w * 4u], bottomFirst + (size_t)(h - 1 - y) * (size_t)w * 4u,
                (size_t)w * 4u);
  if (glPngSave(path, w, h, out.data()))
    std::printf("[volcado] %s (%dx%d)\n", path.c_str(), w, h);
  else
    std::fprintf(stderr, "[aviso] no se pudo escribir %s\n", path.c_str());
}

/* una senal de cierre (systemctl stop, kill) tiene que salir por la puerta de adelante: solo asi se
   devuelve el panel a su modo y se apagan la lente y el scaler. */
static volatile std::sig_atomic_t gTermGL = 0;
static void onTermSignalGL(int) { gTermGL = 1; }

int runViewer(const ViewerOpts &opts, SourceKind kind) {
  std::string sock = ensureWaylandEnv();
  if (sock.empty())
    std::fprintf(stderr, "[aviso] no se detecta sesion Wayland (WAYLAND_DISPLAY vacio): glfw puede fallar\n");
  else
    std::printf("[sesion] Wayland %s\n", sock.c_str());

  glfwSetErrorCallback(glfwError);
  if (!glfwInit()) {
    std::fprintf(stderr, "[error] glfwInit fallo\n");
    return 1;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

  Rig rig;
  rig.bringUp(opts);
  rig.esperar();
  if (rig.fallo()) {   /* (v1.16, de la PR #2 de Noa3) el scaler no confirmo el 3D: no se sigue */
    rig.shutDown();
    glfwTerminate();
    return 1;
  }
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

  GLFWwindow *win = glfwCreateWindow(W, H, NOMBRE_APP, opts.windowed ? nullptr : mon, nullptr);
  if (!win) {
    const char *desc = nullptr;
    int code = glfwGetError(&desc);
    std::fprintf(stderr, "[error] no se pudo crear la ventana (glfw %d): %s\n", code, desc ? desc : "?");
    rig.shutDown();
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(win);
  if (!opts.windowed) glfwSetWindowMonitor(win, mon, 0, 0, W, H, hz);
  glfwSwapInterval(1);
  std::printf("[gl] %s / %s\n", (const char *)glGetString(GL_VERSION), (const char *)glGetString(GL_RENDERER));

  int fbW = 0, fbH = 0;
  glfwGetFramebufferSize(win, &fbW, &fbH);
  float sx = 1.0f, sy = 1.0f;
  glfwGetWindowContentScale(win, &sx, &sy);
  if (fbW != W || fbH != H || std::fabs(sx - 1.0f) > 0.01f)
    std::fprintf(stderr,
                 "[aviso] la ventana es %dx%d (escala de contenido %.2f) y el modo %dx%d: la trama necesita pixel a "
                 "pixel. Deja la escala del panel en 1 y la rotacion normal.\n",
                 fbW, fbH, sx, W, H);

  std::string err;
  Weave weave;
  if (!weave.init(&err)) {
    std::fprintf(stderr, "[error] shader de trama: %s\n", err.c_str());
    glfwDestroyWindow(win);
    rig.shutDown();
    glfwTerminate();
    return 1;
  }
  weave.setPanelSize(fbW, fbH);
  weave.setEye(EyePose{opts.eyeX, opts.eyeY, opts.eyeZ});
  if (!opts.corrA.empty() && !opts.corrB.empty()) weave.setCorrectionsFromPng(opts.corrA, opts.corrB);

  Ui ui{};
  ui.tune.weave = !opts.no3d;
  ui.tune.topBottom = opts.topBottom;
  ui.tune.align = (float)opts.align;
  ui.tune.pitch = (float)opts.pitch;
  ui.tune.filterSlope = (float)opts.filtro;
  glfwSetWindowUserPointer(win, &ui);
  glfwSetKeyCallback(win, onKey);

  TestSrc test;
  MpvSrc mpv;
  bool haveTest = false, haveMpv = false;
#ifdef HAVE_CAPTURE
  WindowCapture cap;
  GpuImporter imp;
  uint64_t lastFrameCount = 0;
  bool haveCap = false;
#endif
  if (kind == SourceKind::Test) {
    haveTest = test.init(&err);
    if (!haveTest) std::fprintf(stderr, "[error] patron de prueba: %s\n", err.c_str());
    test.createFbo(W, H);
  } else if (kind == SourceKind::Mpv) {
    haveMpv = mpv.init(&err);
    if (!haveMpv) {
      std::fprintf(stderr, "[error] mpv: %s\n", err.c_str());
      weave.destroy();
      glfwDestroyWindow(win);
      rig.shutDown();
      glfwTerminate();
      return 1;
    }
    mpv.createFbo(W, H);
    ui.mpv = &mpv;
    if (!mpv.load(opts.file, &err)) std::fprintf(stderr, "[error] %s\n", err.c_str());
    std::printf("[mpv] %s\n", opts.file.c_str());
#ifdef HAVE_CAPTURE
  } else if (kind == SourceKind::Capture) {
    if (!imp.init(&err)) {
      std::fprintf(stderr, "[error] importación DMA-BUF: %s\n", err.c_str());
      weave.destroy();
      glfwDestroyWindow(win);
      rig.shutDown();
      glfwTerminate();
      return 1;
    }
    if (!cap.start(opts.captureUuid, &err)) {
      std::fprintf(stderr, "[error] captura: %s\n", err.c_str());
      weave.destroy();
      glfwDestroyWindow(win);
      rig.shutDown();
      glfwTerminate();
      return 1;
    }
    haveCap = true;
    /* el fotograma del compositor viene con la fila 0 arriba (convención Wayland), al revés
       que la textura de mpv */
    weave.setSrcFlipY(0);
    std::printf("[captura] pidiendo el flujo de la ventana %s\n", opts.captureUuid.c_str());
#endif
  }

  std::printf("\n  teclas  ESC salir · 3 2D/3D · F invertir profundidad · ←/→ fase · RePag/SavPg paso de lente\n"
              "          ↑/↓ crosstalk · , . pendiente de filtro · 9/0 convergencia · 8 reiniciar ajuste\n");
  if (kind == SourceKind::Mpv) std::printf("          espacio pausa · - = saltar 5 s · [ ] cuadro a cuadro\n\n");
#ifdef HAVE_CAPTURE
  if (kind == SourceKind::Capture)
    std::printf("          ventana capturada: si la cierras o pierdes el foco, ESC para volver al escritorio\n\n");
#endif
  std::fflush(stdout);

  double t0 = glfwGetTime(), lastPoll = 0.0, clock = t0;
  int frames = 0;
  std::signal(SIGTERM, onTermSignalGL);
  std::signal(SIGINT, onTermSignalGL);
  while (!glfwWindowShouldClose(win) && !ui.quit) {
    if (gTermGL && !ui.quit) {
      std::printf("[sesion] llego una senal de cierre: se sale devolviendo el panel, la lente y el scaler\n");
      std::fflush(stdout);
      ui.quit = true;
    }
    glfwPollEvents();
    weave.setTuning(ui.tune);
    weave.setHud(rig.hud());
    double now = glfwGetTime();
    GLuint tex = 0;
    if (kind == SourceKind::Test && haveTest) {
      tex = test.render(now - t0, 0, 0);
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
          if (!ui.sizeWarned && (cf.w != fbW || cf.h != fbH)) {
            ui.sizeWarned = true;
            std::fprintf(stderr,
                         "[captura] el fotograma del compositor es %dx%d y el panel esta en %dx%d: la trama "
                         "necesita pixel a pixel (deja la ventana a pantalla completa en esta salida)\n",
                         cf.w, cf.h, fbW, fbH);
          }
          std::string e2;
          tex = imp.upload(gf, &e2);
          if (!tex && !ui.warned) {
            ui.warned = true;
            std::fprintf(stderr, "[captura] %s\n", e2.c_str());
          }
        }
      }
      if (!tex) tex = imp.lastTexture(); /* sin fotograma nuevo: repetir el último, no parpadear */
      if (now - lastPoll > 1.0) {
        lastPoll = now;
        if (!cap.streamCreated()) std::fprintf(stderr, "[captura] %s\n", cap.describe().c_str());
      }
#endif
    } else if (kind == SourceKind::Mpv && haveMpv) {
      tex = mpv.render();
      if (now - lastPoll > 0.25) {
        lastPoll = now;
        mpv.poll();
      }
    }
    if (tex) {
      weave.refreshCorners();
      weave.draw(tex);
    }
    if (tex && !opts.dumpPath.empty() && frames == 30) {
      GLint pw = 0, ph = 0;
      glfwGetFramebufferSize(win, &pw, &ph);
      std::vector<uint8_t> buf((size_t)pw * (size_t)ph * 4u);
      glPixelStorei(GL_PACK_ALIGNMENT, 1);
      glReadPixels(0, 0, pw, ph, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
      saveRgbaFlipped(opts.dumpPath + "-trama.png", pw, ph, buf.data());
      if (kind != SourceKind::Capture) {
        GLuint srcFbo = (kind == SourceKind::Test) ? test.fboId() : mpv.fboId();
        glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo);
        glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        saveRgbaFlipped(opts.dumpPath + "-origen.png", W, H, buf.data());
      }
      ui.quit = true;
    }
    glfwSwapBuffers(win);
    frames++;
    if (now - clock > 5.0) {
      std::printf("[fps] %.1f\n", frames / (now - clock));
      std::fflush(stdout);
      frames = 0;
      clock = now;
    }
  }

  if (kind == SourceKind::Test) test.destroy();
  if (kind == SourceKind::Mpv) mpv.destroy();
#ifdef HAVE_CAPTURE
  if (haveCap) {
    cap.stop();
    imp.destroy();
  }
#endif
  weave.destroy();
  glfwDestroyWindow(win);
  rig.shutDown();
  glfwTerminate();
  return 0;
}
