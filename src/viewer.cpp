#include "viewer.h"

#include "glutil.h"
#include "lens.h"
#include "mpvsrc.h"
#include "panel.h"
#include "scaler.h"
#include "testsrc.h"
#include "util.h"

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

static const char *rotW(int r) {
  const char *w = Panel::rotationWord(r);
  return w ? w : "desconocida";
}

static void glfwError(int code, const char *desc) { std::fprintf(stderr, "[glfw %d] %s\n", code, desc); }

/* ---------------- puesta en marcha / apagado del hardware ---------------- */

struct Rig {
  Lens lens;
  Scaler scaler;
  PanelState saved{};
  bool panelApplied = false;
  bool scalerOn = false;
  bool lensOk = false;
  int hud() const { return (lensOk ? 1 : 0) + (scalerOn ? 2 : 0); }
  std::string output;

  void bringUp(const ViewerOpts &o) {
    /* 1) modo del panel: la trama exige píxel a píxel (escala 1, rotación normal, 3840x2160). */
    if (!o.noPanel) {
      if (!Panel::available()) {
        std::fprintf(stderr,
                     "[aviso] kscreen-doctor no esta disponible: deja el panel a 3840x2160, escala 1 y rotacion normal\n");
      } else {
        std::string name = Panel::resolveOutput(o.output);
        PanelState st{};
        std::string err;
        if (name.empty() || !Panel::query(name, st, &err)) {
          std::fprintf(stderr, "[aviso] no se pudo leer el estado del panel (%s): no se cambia el modo\n",
                       err.empty() ? "sin salidas" : err.c_str());
        } else {
          output = name;
          int idx = Panel::pickMode(st, o.hz);
          if (idx < 0) {
            std::fprintf(stderr, "[aviso] %s no tiene modo 3840x2160: no se cambia el modo\n", name.c_str());
          } else {
            /* Aviso si una sesion anterior se quedo sin restaurar (no tocamos nada por nuestra
               cuenta: puede que el usuario haya cambiado la pantalla despues). */
            PanelState stale{};
            if (Panel::loadSnapshot(stale) && stale.name == name &&
                (stale.modeIndex != st.modeIndex || stale.scale != st.scale || stale.rotation != st.rotation))
              std::fprintf(stderr,
                           "[aviso] hay un estado del panel sin restaurar de otra sesion (modo %d %s, escala %g, "
                           "rotacion %s). Si quieres recuperarlo: odyssey3d ctl panel restore\n",
                           stale.modeIndex, stale.modeRes.c_str(), stale.scale, rotW(stale.rotation));
            std::string snapPath;
            if (!Panel::saveSnapshot(st, &snapPath, &err))
              std::fprintf(stderr, "[aviso] %s\n", err.c_str());
            if (Panel::apply(name, idx, 1.0, RotNone, true, &err, false)) {
                saved = st;
              panelApplied = true;
              std::printf("[panel] %s: modo %d %s, escala 1, rotacion normal, HDR apagado\n", name.c_str(), idx,
                          Panel::modeName(st, idx).c_str());
              std::printf("[panel] al salir se restaura el modo %d %s con escala %g y rotacion %s\n", saved.modeIndex,
                          saved.modeRes.c_str(), saved.scale, rotW(saved.rotation));
              usleep(400000); /* que KWin asiente antes de crear la ventana */
            } else {
              std::fprintf(stderr, "[aviso] no se pudo cambiar el modo: %s\n", err.c_str());
              Panel::clearSnapshot();
            }
          }
          if (!st.vrr.empty() && st.vrr != "Never")
            std::fprintf(stderr, "[aviso] sincronizacion adaptativa en %s: apagala en Ajustustes > Pantalla si ves parpadeo\n",
                         st.vrr.c_str());
        }
      }
    }

    /* 2) lente lenticular: autenticacion HMAC-SHA3 y conmutacion a 3D. */
    if (!o.noLens) {
      std::string port = Lens::findPort();
      if (port.empty()) {
        std::fprintf(stderr, "[aviso] no se encontro la lente (USB 354b:0116): ¿falta el cable USB-B del monitor?\n");
      } else if (!lens.open(port)) {
        std::fprintf(stderr, "[aviso] lente: %s\n", lens.lastError().c_str());
      } else {
        std::printf("[lente] %s\n", port.c_str());
        if (!lens.authenticate()) std::fprintf(stderr, "[aviso] autenticación de lente falló: %s\n", lens.lastError().c_str());
        if (!lens.on()) std::fprintf(stderr, "[aviso] no se pudo encender la lente: %s\n", lens.lastError().c_str());
        lensOk = lens.state();
        std::printf("[lente] estado: %s\n", lensOk ? "3D" : "2D");
      }
    }

    /* 3) scaler MediaTek: modo 3D de la tarjeta del monitor. */
    if (!o.noScaler) {
      std::string dev = Scaler::findDevice();
      if (dev.empty()) {
        std::fprintf(stderr, "[aviso] no se encontro el scaler (dispositivo SCSI con vendor GCREADER)\n");
      } else if (!scaler.open(dev)) {
        std::fprintf(stderr, "[aviso] scaler: %s\n", scaler.lastError().c_str());
      } else if (scaler.set3d(true)) {
        scalerOn = true;
        std::printf("[scaler] %s: modo 3D activado\n", dev.c_str());
      } else {
        std::fprintf(stderr, "[aviso] no se pudo activar el modo 3D: %s\n", scaler.lastError().c_str());
      }
    }
  }

  void shutDown() {
    if (scalerOn) {
      scaler.set3d(false);
      std::printf("[scaler] modo 3D apagado\n");
      scalerOn = false;
    }
    if (lens.isOpen()) {
      lens.off();
      std::printf("[lente] devuelta a 2D\n");
      lens.close();
    }
    if (panelApplied) {
      std::string err;
      if (Panel::restore(saved, &err)) {
        std::printf("[panel] restaurado: modo %d %s, escala %g, rotacion %s\n", saved.modeIndex,
                    saved.modeRes.c_str(), saved.scale, rotW(saved.rotation));
        Panel::clearSnapshot();
      } else {
        std::fprintf(stderr, "[aviso] no se pudo restaurar el panel: %s\n", err.c_str());
        std::fprintf(stderr, "[aviso] el estado guardado sigue en ~/.local/state/odyssey3d/panel.state: "
                             "recuperelo con `odyssey3d ctl panel restore`\n");
      }
      panelApplied = false;
    }
  }
};

/* ---------------- teclas ---------------- */

struct Ui {
  WeaveTuning tune{};
  MpvSrc *mpv = nullptr;
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
  WeaveTuning &t = ui->tune;
  const char *tag = nullptr;
  switch (key) {
    case GLFW_KEY_ESCAPE:
      ui->quit = true;
      glfwSetWindowShouldClose(win, GLFW_TRUE);
      return;
    case GLFW_KEY_3:
      t.weave = !t.weave;
      tag = "3D/2D";
      break;
    case GLFW_KEY_F:
      t.swapLR = !t.swapLR;
      tag = "invertir";
      break;
    case GLFW_KEY_LEFT:
      t.align -= 0.002f;
      tag = "fase -";
      break;
    case GLFW_KEY_RIGHT:
      t.align += 0.002f;
      tag = "fase +";
      break;
    case GLFW_KEY_PAGE_UP:
      t.pitch += 0.0002f;
      tag = "paso +";
      break;
    case GLFW_KEY_PAGE_DOWN:
      t.pitch -= 0.0002f;
      tag = "paso -";
      break;
    case GLFW_KEY_UP:
      t.xtalkDyn += 0.002f;
      tag = "crosstalk +";
      break;
    case GLFW_KEY_DOWN:
      t.xtalkDyn -= 0.002f;
      tag = "crosstalk -";
      break;
    case GLFW_KEY_COMMA:
      t.filterSlope = t.filterSlope < 0.5f ? 0.0f : t.filterSlope - 0.5f;
      tag = "filtro -";
      break;
    case GLFW_KEY_PERIOD:
      t.filterSlope += 0.5f;
      tag = "filtro +";
      break;
    case GLFW_KEY_9:
      t.conv -= 0.0005f;
      tag = "conv -";
      break;
    case GLFW_KEY_0:
      t.conv += 0.0005f;
      tag = "conv +";
      break;
    case GLFW_KEY_8:
      t.conv = 0.0f;
      t.align = 0.0f;
      t.pitch = 1.0f;
      t.xtalkDyn = 0.0f;
      tag = "reiniciar";
      break;
    default:
      break;
  }
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

static GLFWmonitor *pickMonitor(const std::string &output) {
  int n = 0;
  GLFWmonitor **ms = glfwGetMonitors(&n);
  for (int i = 0; i < n; i++)
    if (!output.empty() && output == glfwGetMonitorName(ms[i])) return ms[i];
  for (int i = 0; i < n; i++) { /* si no coincide el nombre, el panel con modo 4K */
    int nm = 0;
    const GLFWvidmode *modes = glfwGetVideoModes(ms[i], &nm);
    for (int j = 0; j < nm; j++)
      if (modes[j].width == 3840 && modes[j].height == 2160) return ms[i];
  }
  return glfwGetPrimaryMonitor();
}

static int refreshOf(GLFWmonitor *mon, double wantHz) {
  int n = 0;
  const GLFWvidmode *modes = glfwGetVideoModes(mon, &n);
  int best = 0;
  double bestd = 1e9;
  for (int i = 0; i < n; i++) {
    double d = std::fabs((double)modes[i].refreshRate - wantHz);
    if (d < bestd) {
      bestd = d;
      best = modes[i].refreshRate;
    }
  }
  const GLFWvidmode *cur = glfwGetVideoMode(mon);
  return best > 0 ? best : (cur ? cur->refreshRate : 0);
}

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
  glfwSetWindowUserPointer(win, &ui);
  glfwSetKeyCallback(win, onKey);

  TestSrc test;
  MpvSrc mpv;
  bool haveTest = false, haveMpv = false;
  if (kind == SourceKind::Test) {
    haveTest = test.init(&err);
    if (!haveTest) std::fprintf(stderr, "[error] patron de prueba: %s\n", err.c_str());
    test.createFbo(W, H);
  } else {
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
  }

  std::printf("\n  teclas  ESC salir · 3 2D/3D · F invertir profundidad · ←/→ fase · RePag/SavPg paso de lente\n"
              "          ↑/↓ crosstalk · , . pendiente de filtro · 9/0 convergencia · 8 reiniciar ajuste\n");
  if (kind == SourceKind::Mpv) std::printf("          espacio pausa · - = saltar 5 s · [ ] cuadro a cuadro\n\n");
  std::fflush(stdout);

  double t0 = glfwGetTime(), lastPoll = 0.0, clock = t0;
  int frames = 0;
  while (!glfwWindowShouldClose(win) && !ui.quit) {
    glfwPollEvents();
    weave.setTuning(ui.tune);
    weave.setHud(rig.hud());
    double now = glfwGetTime();
    GLuint tex = 0;
    if (kind == SourceKind::Test && haveTest) {
      tex = test.render(now - t0, 0, 0);
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
      GLuint srcFbo = (kind == SourceKind::Test) ? test.fboId() : mpv.fboId();
      glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo);
      glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
      glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
      glPixelStorei(GL_PACK_ALIGNMENT, 4);
      saveRgbaFlipped(opts.dumpPath + "-origen.png", W, H, buf.data());
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
  weave.destroy();
  glfwDestroyWindow(win);
  rig.shutDown();
  glfwTerminate();
  return 0;
}
