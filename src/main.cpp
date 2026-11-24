/* odyssey3d — visor/reproductor 3D sin gafas para el Samsung Odyssey 3D (G90XF)
   en Wayland. Parte de control de hardware: lente lenticular, scaler MediaTek,
   iluminador IR y modo del panel. */
#include "ircam.h"
#include "lens.h"
#include "calib.h"
#include "camera.h"
#include "track.h"
#include "panel.h"
#include "mpris.h"
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
#include "weave.h"

#include <GLFW/glfw3.h>

#include <poll.h>

#include <csignal>
#include <map>
#include <set>
#include <sys/stat.h>
#include <sys/wait.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
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
    "  odyssey3d ctl calib  [--out calib]   leer calibracion de fabrica (camara IR y transformada)\n"
    "  odyssey3d ctl camera [--dev N] [--size 1280x480] [--format MJPG]   probar la camara\n"
    "  odyssey3d ctl track  [--segundos 20] [--dump foto.jpg]   rastreo de cabeza (cara y ojos)\n"
    "                     [--foto IMAGEN] revisa la deteccion en un archivo; [--sintetico] prueba la geometria\n"
    "  odyssey3d ctl panel  [status|apply|restore] [--output DP-4] [--hz 120] [--dry]\n"
    "  odyssey3d ctl windows [--watch]\n"
    "  odyssey3d ctl ventana [fullscreen|normal|maximizar|minimizar|aparecer|sobre|debajo|libre|salida NOMBRE|cerrar] [--uuid ID]\n"
    "                                        como esta la ventana del video; `salida DP-4` la devuelve al Odyssey\n"
    "  odyssey3d ctl aparece   la ventana del visor otra vez a la vista (para atarla a la tecla PB)\n"
    "  odyssey3d ctl salidas   cada pantalla con su posicion y tamano logicos en el espacio global\n"
    "  odyssey3d ctl apilamiento  el orden arriba/abajo que ve el compositor (el ultimo esta arriba)\n"
    "  odyssey3d ctl capture [--uuid X] [--out F] [--segundos N]\n"
    "  odyssey3d ctl capture-gl [--uuid X] [--out F] [--segundos N]\n"
    "  odyssey3d ctl vkprobe [--gpu N] [--solamente-gpu] [--verboso] [--uuid X] [--out F]\n"
      "                                        muestra y guarda un fotograma leido en Vulkan\n"
    "  odyssey3d ctl notify [--wait N]\n"
    "  odyssey3d ctl flujo [--uuid ID] [--segundos N]  medir si el compositor manda fotogramas nuevos\n"
    "  odyssey3d ctl video  [status|pause|play|toggle]   pausar y reanudar el video sin quitar la capa\n"
    "  odyssey3d ctl overlay [--segundos 12] [--dump F.png] [--titulo T] [--cuerpo T]\n"
    "                                        poner la tarjeta del hub por encima de todo (capa superpuesta)\n"
    "  odyssey3d capture3d [--uuid X]   capturar una ventana a pantalla completa y tramarla en 3D\n"
    "  odyssey3d test [--windowed]      patron de prueba con la interfaz (raton y teclas)\n"
    "  odyssey3d serve [--dry]          servicio: avisa cuando algo se pone a pantalla completa y lo pone en 3D\n"
    "\n"
    "opciones:\n"
    "  --output NOMBRE   salida del panel (por defecto, la que tenga modo 3840x2160)\n"
    "  --hz N            frecuencia objetivo del modo (por defecto 120)\n"
    "  --windowed        ventana en lugar de pantalla completa\n"
    "  --no-panel        no tocar el modo / la escala / la rotacion del panel\n"
    "  --sin-pasar-raton el visor se traga todos los clics (como antes); por defecto el\n"
    "                    raton atraviesa la ventana fuera de la pastilla\n"
    "  --no-lens         no tocar la lente lenticular\n"
    "  --no-scaler       no tocar el modo 3D del scaler del monitor\n"
    "  --2d              arrancar en pase 2D (sin tramar)\n"
    "  --tb              video arriba/abajo (por defecto: lado a lado)\n"
    "  --hwdec M         decodificacion de mpv: auto, nvdec, no (prueba `no` si hay artefactos)\n"
    "  --eye-x/--eye-y/--eye-z CM   posicion del ojo (por defecto 0, 8.39, 65.11 cm)\n"
    "  --track                      rastrear la cabeza con la camara IR del monitor\n"
    "  --track-dev DEV  --track-model ONNX  --track-calib-dir DIR   (se solos por separado)\n"
    "  --track-offset-x/y/z CM      corregir lo medido · --track-flip-x/-y invertir un eje\n"
    "  --track-score 0.6            umbral de cara · --track-suavizado 0.35 · --track-fixed\n"
    "  --align CICLOS    desfase de fase inicial   --pitch F  escala de la rampa de fase\n"
    "  --calib-dir DIR   mapas de correccion del panel (3DStackCorrection_A/B.png)\n"
    "  --solid / --no-corr / --out ARCHIVO   solo `weave`: fuente plana, sin correcciones, salida\n"
    "  --sin-interfaz    no dibujar la superposicion del hub (solo Vulkan)\n"
    "  --ui-escala X     tamano de la pastilla: 1 pequena, 2 grande (por defecto x2 en 4K)\n"
    "  --sin-borde       ventana sin bordes maximada en vez de pantalla completa\n"
    "  --siempre         representar a ritmo de panel aunque no llegue video nuevo\n"
    "  --pos X,Y         colocar la ventana en una esquina de la salida capturada\n"
    "  --gl              usar el camino OpenGL de siempre (por defecto todo se dibuja con Vulkan)\n"
    "\n"
    "teclas: ESC salir · 3 2D/3D · F invertir profundidad · flechas fase/paso/crosstalk · , . filtro\n"
    "        9/0 convergencia · 8 reiniciar ajuste · espacio pausa · - = saltar 5 s · [ ] cuadro\n"
    "        con Vulkan: raton sobre la pastilla · Ctrl+Shift 1..6 como el hub oficial · 7/8 tamano de la pastilla\n";

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

static bool saveBlobFile(const std::string &dir, const char *name, const std::vector<uint8_t> &blob) {
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  const std::string path = dir + "/" + name;
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    std::fprintf(stderr, "no se pudo escribir %s\n", path.c_str());
    return false;
  }
  f.write((const char *)blob.data(), (std::streamsize)blob.size());
  f.close();
  std::printf("  %-14s %6zu bytes -> %s\n", name, blob.size(), path.c_str());
  return true;
}

static int cmdCalib(int argc, char **argv) {
  std::string port = flag(argc, argv, "--port");
  std::string out = flag(argc, argv, "--out");
  if (out.empty()) out = "calib";
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
  if (!lens.authenticated() && !lens.authenticate()) {
    std::fprintf(stderr, "hace falta autenticar el tablero para leer la calibracion: %s\n",
                 lens.lastError().c_str());
    return 1;
  }
  struct Pedido {
    const char *cmd;
    size_t tam;
    int ms;
    const char *archivo;
  };
  const Pedido pedidos[] = {
      {"SR+CHIP", 32, 2000, "chip.bin"},
      {"SR+CALCAM", 256, 2000, "SR+CALCAM.bin"},
      {"SR+CALET", 256, 2000, "SR+CALET.bin"},
      {"SR+CAL3D", 0x20000, 10000, "SR+CAL3D.bin"},
  };
  int fallos = 0;
  std::vector<uint8_t> camBlob, letBlob;
  for (const auto &p : pedidos) {
    std::vector<uint8_t> blob;
    if (!lens.readFixed(p.cmd, p.tam, p.ms, blob)) {
      std::fprintf(stderr, "%s: %s\n", p.cmd, lens.lastError().c_str());
      ++fallos;
      continue;
    }
    saveBlobFile(out, p.archivo, blob);
    if (std::string(p.cmd) == "SR+CALCAM") camBlob = blob;
    if (std::string(p.cmd) == "SR+CALET") letBlob = blob;
  }
  CameraCalibration cam;
  if (cam.fromBlob(camBlob)) {
    for (int v = 0; v < 2; v++) {
      const double *k = v ? cam.rightIntrinsics : cam.leftIntrinsics;
      std::printf("  camara %-9s fx=%.3f fy=%.3f cx=%.3f cy=%.3f\n", v ? "derecha" : "izquierda", k[0],
                  k[1], k[2], k[3]);
    }
    std::printf("  distorsion izquierda:");
    for (int i = 0; i < 8; i++) std::printf(" %.6f", cam.leftDistortion[i]);
    std::printf("\n  distorsion derecha :");
    for (int i = 0; i < 8; i++) std::printf(" %.6f", cam.rightDistortion[i]);
    std::printf("\n  rotacion vector: %.6f %.6f %.6f\n", cam.rotationVector[0], cam.rotationVector[1],
                cam.rotationVector[2]);
    std::printf("  traslacion     : %.4f %.4f %.4f\n", cam.translation[0], cam.translation[1],
                cam.translation[2]);
  } else if (!camBlob.empty()) {
    std::fprintf(stderr, "SR+CALCAM no tiene la forma esperada\n");
    ++fallos;
  }
  TrackerTransform tr;
  if (tr.fromBlob(letBlob)) {
    std::printf("  camara->panel: X=%.4f Y=%.4f Z=%.4f cm   giros X=%.4f Y=%.4f Z=%.4f grados\n",
                tr.translationCentimetres[0], tr.translationCentimetres[1],
                tr.translationCentimetres[2], tr.rotationDegrees[0], tr.rotationDegrees[1],
                tr.rotationDegrees[2]);
  } else if (!letBlob.empty()) {
    std::fprintf(stderr, "SR+CALET no tiene la forma esperada\n");
    ++fallos;
  }
  return fallos ? 1 : 0;
}

/* ruta del modelo de cara, buscada junto al programa y en los sitios de siempre */
static std::string defaultModelPath() {
  std::vector<std::string> bases;
  bases.push_back(".");
  bases.push_back("..");
  char link[4096];
  ssize_t n = readlink("/proc/self/exe", link, sizeof link - 1);
  if (n > 0) {
    link[n] = 0;
    std::string dir = link;
    size_t slash = dir.find_last_of('/');
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    bases.push_back(dir);
    bases.push_back(dir + "/..");
    bases.push_back(dir + "/../share/odyssey3d");
    bases.push_back("/usr/local/share/odyssey3d");
    bases.push_back("/usr/share/odyssey3d");
  }
  for (const auto &b : bases) {
    const std::string p = b + "/modelos/face_detection_yunet_2023mar.onnx";
    if (fileExists(p)) return p;
  }
  return std::string();
}

static bool pickCameraDevice(std::string &dev, std::string *err) {
  if (!dev.empty()) return true;
  auto devs = IrCam::findDevices();
  if (!devs.empty()) {
    dev = devs.front();
    return true;
  }
  for (int i = 0; i < 10; i++) {
    const std::string p = "/dev/video" + std::to_string(i);
    auto modes = Camera::modes(p);
    if (!modes.empty()) {
      dev = p;
      return true;
    }
  }
  if (err) *err = "no se encontro ninguna camara de video (/dev/video*)";
  return false;
}

static int cmdCamera(int argc, char **argv) {
  std::string dev = flag(argc, argv, "--dev");
  std::string err;
  if (!pickCameraDevice(dev, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  std::printf("camara: %s\n", dev.c_str());
  for (const auto &m : Camera::modes(dev))
    std::printf("  modo   %s %dx%d @ %d fps\n", m.fourcc.c_str(), m.width, m.height, m.fps);
  if (hasFlag(argc, argv, "--solo-modos")) return 0;

  const std::string sizeS = flag(argc, argv, "--size");
  const std::string format = flag(argc, argv, "--format").empty() ? "MJPG" : flag(argc, argv, "--format");
  int fps = atoi(flag(argc, argv, "--fps").c_str());
  if (fps <= 0) fps = 60;
  int wanted = atoi(flag(argc, argv, "--frames").c_str());
  if (wanted <= 0) wanted = 5;
  int width = 1280, height = 480;
  if (!sizeS.empty()) {
    if (std::sscanf(sizeS.c_str(), "%dx%d", &width, &height) != 2) {
      std::fprintf(stderr, "--size quiere decir WxH\n");
      return 2;
    }
  }
  Camera cam;
  if (!cam.open(dev, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  if (!cam.setFormat(width, height, format, fps, &err)) {
    std::fprintf(stderr, "ajuste de formato: %s\n", err.c_str());
    return 1;
  }
  std::printf("formato  %dx%d %s @ %d fps\n", cam.width(), cam.height(), format.c_str(), fps);
  if (!cam.start(&err)) {
    std::fprintf(stderr, "arranque del flujo: %s\n", err.c_str());
    return 1;
  }
  const std::string out = flag(argc, argv, "--out");
  int ok = 0;
  for (int i = 0; i < wanted; i++) {
    const uint8_t *data = nullptr;
    size_t size = 0;
    std::string why;
    if (!cam.read(&data, &size, 4000, &why)) {
      std::fprintf(stderr, "fotograma %d: %s\n", i + 1, why.c_str());
      break;
    }
    ok++;
    std::printf("fotograma %d: %zu bytes\n", i + 1, size);
    if (!out.empty()) {
      const std::string path = out + "." + (cam.mjpeg() ? "jpg" : "yuyv");
      FILE *f = std::fopen(path.c_str(), "wb");
      if (f) {
        std::fwrite(data, 1, size, f);
        std::fclose(f);
        std::printf("         guardado en %s\n", path.c_str());
      }
    }
    cam.release();
  }
  cam.stop();
  std::printf("%d de %d fotogramas\n", ok, wanted);
  return ok > 0 ? 0 : 1;
}

#ifdef HAVE_OPENCV
static int cmdTrack(int argc, char **argv) {
  TrackSettings set;
  set.model = flag(argc, argv, "--track-model");
  if (set.model.empty()) set.model = defaultModelPath();
  if (set.model.empty()) {
    std::fprintf(stderr, "falta el modelo de cara: descarga face_detection_yunet_2023mar.onnx en "
                         "modelos/ o usa --track-model RUTA\n");
    return 1;
  }
  if (!flag(argc, argv, "--score").empty()) set.score = (float)atof(flag(argc, argv, "--score").c_str());
  if (!flag(argc, argv, "--suavizado").empty()) set.smoothing = atof(flag(argc, argv, "--suavizado").c_str());
  if (!flag(argc, argv, "--salto").empty()) set.maxJumpCentimetres = atof(flag(argc, argv, "--salto").c_str());

  std::string err;
  CameraCalibration calib;
  TrackerTransform xform;
  const std::string dir = flag(argc, argv, "--calib-dir");
  if (!loadFactoryCalib(dir, &calib, &xform, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  std::printf("calibracion  %s\n", dir.empty() ? "calib/" : dir.c_str());
  std::printf("modelo       %s\n", set.model.c_str());

  const std::string dump = flag(argc, argv, "--dump");
  const std::string foto = flag(argc, argv, "--foto");
  if (!foto.empty()) {
    HeadTracker tracker;
    if (!tracker.init(calib, xform, set, &err)) {
      std::fprintf(stderr, "rastreador: %s\n", err.c_str());
      return 1;
    }
    TrackSample sample;
    std::string info;
    if (!HeadTracker::describeStill(foto, set, dump, tracker, &sample, &info, &err)) {
      std::fprintf(stderr, "%s\n", err.c_str());
      return 1;
    }
    std::printf("%s\n", info.c_str());
    if (!dump.empty()) std::printf("anotado      %s\n", dump.c_str());
    return sample.accepted ? 0 : 1;
  }

  HeadTracker tracker;
  if (!tracker.init(calib, xform, set, &err)) {
    std::fprintf(stderr, "rastreador: %s\n", err.c_str());
    return 1;
  }
  if (hasFlag(argc, argv, "--sintetico")) {
    /* Se comprime un punto 3D conocido en las dos vistas y se devuelve por toda
       la cadena: asi se ve si la geometria y los signos cuadran sin camara. */
    const double puntos[][3] = {{0, 0, 600},  {0, 0, 500},   {0, 0, 700},  {30, 0, 600},
                                {-30, 0, 600}, {0, 30, 600}, {0, -30, 600}, {60, 0, 600},
                                {-60, 0, 600}, {0, 60, 600}, {0, -60, 600}, {0, 0, 900}};
    std::printf("punto pedido (mm)        pixeles izq/der                recuperado (mm)        panel (cm)\n");
    for (const auto &p : puntos) {
      double px[4];
      if (!tracker.projectPoint(p, px, &err)) {
        std::fprintf(stderr, "proyeccion: %s\n", err.c_str());
        return 1;
      }
      tracker.reset();
      TrackSample sample;
      tracker.submitEyes(px, sample, &err);
      std::printf("(%7.1f %7.1f %7.1f)  (%7.1f,%7.1f) (%7.1f,%7.1f)  (%7.1f %7.1f %7.1f)  (%6.2f %6.2f %6.2f)  %.3f px %s\n",
                  p[0], p[1], p[2], px[0], px[1], px[2], px[3], sample.eyeMillimetres[0],
                  sample.eyeMillimetres[1], sample.eyeMillimetres[2], sample.panelCentimetres[0],
                  sample.panelCentimetres[1], sample.panelCentimetres[2], sample.reprojectionPx,
                  sample.accepted ? "" : (sample.reason.empty() ? "descartada" : sample.reason.c_str()));
    }
    return 0;
  }

  std::string dev = flag(argc, argv, "--dev");
  if (!pickCameraDevice(dev, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  Camera cam;
  if (!cam.open(dev, &err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  struct Intento {
    int w, h, fps;
    const char *fourcc;
  };
  const Intento intentos[] = {{1280, 480, 60, "MJPG"}, {1280, 480, 15, "YUYV"}, {640, 480, 30, "YUYV"}};
  bool listo = false;
  for (const auto &it : intentos) {
    if (cam.setFormat(it.w, it.h, it.fourcc, it.fps, &err)) {
      std::printf("camara       %s %dx%d %s @ %d\n", dev.c_str(), it.w, it.h, it.fourcc, it.fps);
      listo = true;
      break;
    }
    std::fprintf(stderr, "aviso: %dx%d %s no fue posible (%s)\n", it.w, it.h, it.fourcc, err.c_str());
  }
  if (!listo) return 1;
  if (!cam.start(&err)) {
    std::fprintf(stderr, "arranque del flujo: %s\n", err.c_str());
    return 1;
  }
  const int segundos = atoi(flag(argc, argv, "--segundos").c_str());
  const int segundosLimite = segundos > 0 ? segundos : 20;
  const int mostrarCada = std::max(1, atoi(flag(argc, argv, "--cada").c_str()) ? atoi(flag(argc, argv, "--cada").c_str()) : 15);
  auto t0 = std::chrono::steady_clock::now();
  int leidos = 0, aceptados = 0;
  bool dumpHecho = false;
  while (true) {
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<double>(now - t0).count() >= (double)segundosLimite) break;
    const uint8_t *data = nullptr;
    size_t size = 0;
    std::string why;
    if (!cam.read(&data, &size, 2000, &why)) {
      std::fprintf(stderr, "lectura: %s\n", why.c_str());
      break;
    }
    leidos++;
    std::vector<uint8_t> left, right;
    int vw = 0, vh = 0;
    if (!HeadTracker::decodeFrame(data, size, cam.width(), cam.height(), cam.mjpeg(), left, right, vw,
                                  vh, &why)) {
      cam.release();
      std::fprintf(stderr, "decodificacion: %s\n", why.c_str());
      continue;
    }
    const uint8_t *lp = left.data(), *rp = right.data();
    TrackSample sample;
    tracker.process(lp, rp, vw, vh, sample, &why);
    if (sample.accepted && !sample.predicted) aceptados++;
    if (!dump.empty() && !dumpHecho && sample.detected) {
      std::string d2;
      if (HeadTracker::saveAnnotated(lp, rp, vw, vh, set, dump, &d2))
        std::printf("anotado      %s\n", dump.c_str());
      else
        std::fprintf(stderr, "anotado: %s\n", d2.c_str());
      dumpHecho = true;
    }
    cam.release();
    if (leidos % mostrarCada == 0) {
      std::printf("cara %s  panel %7.2f %7.2f %7.2f cm  ipd %5.1f  error %4.2f px  %5.1f ms  %s\n",
                  sample.detected ? "si " : "no ", sample.panelCentimetres[0],
                  sample.panelCentimetres[1], sample.panelCentimetres[2], sample.ipdMillimetres,
                  sample.reprojectionPx, sample.latencyMs,
                  sample.accepted ? (sample.predicted ? "mantenida" : "valida")
                                  : (sample.reason.empty() ? "sin usar" : sample.reason.c_str()));
      std::fflush(stdout);
    }
  }
  cam.stop();
  std::printf("%d fotogramas leidos, %d poses validas\n", leidos, aceptados);
  return aceptados > 0 ? 0 : 1;
}
#endif

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
  /* El iluminador IR no necesita ninguna orden: la camara lo enciende sola cuando
     manda video. El control que probamos (unidad 3, selector 2 del XU) hace que el
     endpoint de control UVC deje de responder (timepo -110) y luego S_FMT /
     STREAMON devuelven EIO hasta desenchufar el USB del monitor. Por eso solo se
     manda si se pide de forma explicita con --unit y --sel. */
  const std::string unitS = flag(argc, argv, "--unit"), selS = flag(argc, argv, "--sel"),
                      bytesS = flag(argc, argv, "--bytes");
  if (unitS.empty() || selS.empty()) {
    std::printf("ir %s: no enviado a proposito\n", what.c_str());
    std::printf("  la unidad 3 no responde a GET_LEN y el puerto de control se bloquea; la camara\n");
    std::printf("  ya enciende el IR sola al mandar video.\n");
    std::printf("  para experimentar: ctl ir %s --unit N --sel N [--bytes 01]\n", what.c_str());
    return 0;
  }
  const int unit = atoi(unitS.c_str()), sel = atoi(selS.c_str());
  std::vector<uint8_t> payload = bytesS.empty() ? std::vector<uint8_t>{(uint8_t)(what == "off" ? 0 : 1)}
                                                : unhex(bytesS);
  if (payload.empty()) {
    std::fprintf(stderr, "--bytes vacio\n");
    return 2;
  }
  const bool ok = cam.setCur(unit, sel, payload);
  std::printf("ir %s (unidad %d selector %d): %s\n", what.c_str(), unit, sel, ok ? "ok" : "fallo");
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
      {"sbs", "3D lado a lado"}, {"tb", "3D arriba/abajo"}, {"conv", "Video normal a 3D"},
      {"flat", "Solo lente"}, {"no", "No"}};
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
  std::printf("%-34s  %-20s  %-30s  %-24s  %-17s  %s\n", "uuid", "app_id", "titulo", "estado", "posicion y tamano",
              "pid");
  for (const auto &w : wm.windows()) {
    std::string t = w.title;
    if (t.size() > 28) t = t.substr(0, 27) + "\xe2\x80\xa6";
    char geo[32];
    std::snprintf(geo, sizeof geo, "%5d,%-4d %ux%u", w.x, w.y, w.w, w.h);
    std::printf("%-34s  %-20s  %-30s  %-24s  %-17s  %u\n", w.uuid.c_str(), w.appId.c_str(), t.c_str(),
                WindowInfo::stateList(w.states).c_str(), geo, w.pid);
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

/* ---------------- pruebas Vulkan ---------------- */
/* Comprueba, sin abrir ventana, si esta GPU puede importar el DMA-BUF que entrega KWin
   y muestrearlo. Es el paso que decide cómo se porta el visor a Vulkan. */
static int cmdVkProbe(int argc, char **argv) {
  std::string err;
  bool verboso = hasFlag(argc, argv, "--verboso");
  std::string out = flag(argc, argv, "--out");
  bool soloGpu = hasFlag(argc, argv, "--solamente-gpu");
  int secs = 4;
  if (std::string s = flag(argc, argv, "--segundos"); !s.empty()) secs = std::atoi(s.c_str());

  VkDev dev;
  if (!dev.init({}, &err)) {
    std::fprintf(stderr, "[vulkan] no se pudo crear la instancia: %s\n", err.c_str());
    return 1;
  }
  int gpuElegida = -1;
  if (std::string g = flag(argc, argv, "--gpu"); !g.empty()) gpuElegida = std::atoi(g.c_str());
  if (!dev.createDevice({}, VK_NULL_HANDLE, &err, gpuElegida)) {
    std::fprintf(stderr, "[vulkan] %s\n", err.c_str());
    return 1;
  }
  std::printf("[vulkan] gpu: %s · importación DMA-BUF: %s\n", dev.deviceName().c_str(),
              dev.hasDmaBufImport() ? "sí" : "no");
  std::fflush(stdout);
  if (verboso) {
    std::printf("[vulkan] %s\n", dev.describeModifiers(VK_FORMAT_B8G8R8A8_UNORM).c_str());
    std::printf("[vulkan] %s\n", dev.describeModifiers(VK_FORMAT_R8G8B8A8_UNORM).c_str());
    std::printf("[vulkan] %s\n", dev.describeExternal(VK_FORMAT_B8G8R8A8_UNORM).c_str());
    std::printf("[vulkan] %s\n", dev.describeExternal(VK_FORMAT_R8G8B8A8_UNORM).c_str());
    std::fflush(stdout);
  }
  if (soloGpu) return 0;

#ifndef HAVE_CAPTURE
  (void)out; (void)secs;
  std::fprintf(stderr, "compilado sin soporte de captura: usa --gpu para mirar sólo la GPU\n");
  return 1;
#else
  std::string uuid;
  WindowInfo win;
  bool haveWin = false;
  if (!pickWindow(argc, argv, uuid, win, haveWin)) return 1;

  WindowCapture cap;
  std::string cls = flag(argc, argv, "--clase");
  if (!cls.empty()) cap.setStreamClass(cls);
  if (!cap.start(uuid, &err)) {
    std::fprintf(stderr, "[vulkan] %s\n", err.c_str());
    return 1;
  }
  std::printf("[vulkan] pidiendo el flujo de la ventana a KWin...\n");
  std::fflush(stdout);
  for (int i = 0; i < secs * 2; ++i) {
    struct pollfd pfd{cap.fd(), POLLIN, 0};
    poll(&pfd, 1, 500);
    cap.pump();
    if (cap.framesFlowing()) break;
    if (i % 2 == 1) std::printf("%s\n", cap.describe().c_str());
    std::fflush(stdout);
  }
  CapturedFrame cf;
  if (!cap.lastFrame(cf)) {
    std::fprintf(stderr, "[vulkan] no llegó ningún fotograma: %s\n", cap.lastError().c_str());
    cap.stop();
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

  std::string info;
  std::vector<uint8_t> rgba;
  bool ok = vkProbeFrame(dev, gf, rgba, info, &err);
  cap.stop();
  std::printf("%s", info.c_str());
  if (!ok) {
    std::fprintf(stderr, "[vulkan] %s\n", err.c_str());
    return 1;
  }
  /* resumen para saber si de verdad hay imagen (no todo negro) */
  unsigned long sumR = 0, sumG = 0, sumB = 0;
  int muestras = 0;
  for (size_t i = 0; i + 3 < rgba.size(); i += 4 * 97) {
    sumR += rgba[i];
    sumG += rgba[i + 1];
    sumB += rgba[i + 2];
    muestras++;
  }
  if (muestras)
    std::printf("promedio muestreado: R %lu G %lu B %lu sobre %d puntos\n", sumR / (unsigned long)muestras,
                sumG / (unsigned long)muestras, sumB / (unsigned long)muestras, muestras);
  if (out.empty()) out = "captura-vk.png";
  if (!glPngSave(out, gf.w, gf.h, rgba.data())) {
    std::fprintf(stderr, "[vulkan] no se pudo escribir %s\n", out.c_str());
    return 1;
  }
  std::printf("guardado %s (%llu fotogramas recibidos)\n", out.c_str(),
              static_cast<unsigned long long>(cap.frameCount()));
  return 0;
#endif
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

/* Busca un archivo de datos junto al binario, en el directorio actual o en /usr/share. */
static std::string findDataFile(const std::string &name, const std::string &calibDir) {
  std::vector<std::string> bases;
  if (!calibDir.empty()) bases.push_back(calibDir);
  for (const char *b : {".", "./calib", "../calib"}) bases.push_back(b);
  std::string ed = exeDir();
  if (!ed.empty()) {
    bases.push_back(ed + "/calib");
    bases.push_back(ed + "/../share/odyssey3d/calib");
    bases.push_back(ed + "/../calib");
  }
  bases.push_back("/usr/share/odyssey3d/calib");
  bases.push_back("/usr/local/share/odyssey3d/calib");
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
    else if (a == "--sin-pasar-raton") o.noPassthrough = true;
    else if (a == "--no-lens") o.noLens = true;
    else if (a == "--no-scaler") o.noScaler = true;
    else if (a == "--2d") { o.no3d = true; o.fmtGiven = true; }
    else if (a == "--tb" || a == "--top-bottom") { o.topBottom = true; o.fmtGiven = true; }
    else if (a == "--sbs" || a == "--side-by-side") o.fmtGiven = true;
    else if (a == "--conver" || a == "--convert") {
      o.conver = true;
      o.fmtGiven = true;   /* el formato ya esta elegido: no hace falta volver a preguntar */
    }
    else if (a == "--modelo" || a == "--model") {
      if (i + 1 >= argc) { std::fprintf(stderr, "%s necesita una ruta\n", a.c_str()); return 2; }
      o.modelPath = argv[++i];
      o.conver = true;
    } else if (a == "--alterno" || a == "--alternativa") {
      if (i + 1 >= argc) { std::fprintf(stderr, "%s necesita una ruta\n", a.c_str()); return 2; }
      o.altModel = argv[++i];
      o.conver = true;   /* la otra red del boton 1 / 2 del visor */
    }
    else if (a == "--red" || a == "--red-gpu") {
      if (i + 1 >= argc) { std::fprintf(stderr, "%s necesita un numero\n", a.c_str()); return 2; }
      o.convGpu = std::max(0, std::atoi(argv[++i]));
    } else if (a == "--cada") {
      if (i + 1 >= argc) { std::fprintf(stderr, "%s necesita un numero\n", a.c_str()); return 2; }
      o.convEach = std::max(1, std::atoi(argv[++i]));
    }
    else if (a == "--ui-escala" || a == "--ui-scale") {
      if (!need("ui-escala")) return false;
      o.uiScale = (float)std::atof(argv[++i]);
    }
    else if (a == "--sin-interfaz") o.noHud = true;
    else if (a == "--sin-borde") o.borderless = true;
    else if (a == "--siempre") o.alwaysRender = true;
    else if (a == "--pos") {
      if (!need("pos")) return false;
      const char *v = argv[++i];
      const char *c = std::strchr(v, ',');
      if (c) { o.posX = std::atoi(v); o.posY = std::atoi(c + 1); }
    }
    else if (a == "--cpu-patron") o.cpuPattern = true;
    else if (a == "--no-corr") o.noCorr = true;
    else if (a == "--solid") o.solid = true;
    else if (a == "--out") { if (!need("out")) return false; o.outPath = argv[++i]; }
    else if (a == "--dump") { if (!need("dump")) return false; o.dumpPath = argv[++i]; }
    else if (a == "--hwdec") { if (!need("hwdec")) return false; setenv("ODYSSEY_HWDEC", argv[++i], 1); }
    else if (a == "--eye-x") { if (!need("eye-x")) return false; o.eyeX = atof(argv[++i]); }
    else if (a == "--eye-y") { if (!need("eye-y")) return false; o.eyeY = atof(argv[++i]); }
    else if (a == "--eye-z") { if (!need("eye-z")) return false; o.eyeZ = atof(argv[++i]); }
    else if (a == "--track") o.track = true;
    else if (a == "--track-fixed") o.trackFixed = true;
    else if (a == "--track-flip-x") o.trackFlipX = true;
    else if (a == "--track-flip-y") o.trackFlipY = true;
    else if (a == "--track-dev") { if (!need("track-dev")) return false; o.trackDevice = argv[++i]; }
    else if (a == "--track-model") { if (!need("track-model")) return false; o.trackModel = argv[++i]; }
    else if (a == "--track-calib-dir") { if (!need("track-calib-dir")) return false; o.trackCalibDir = argv[++i]; }
    else if (a == "--track-offset-x") { if (!need("track-offset-x")) return false; o.trackOffsetX = atof(argv[++i]); }
    else if (a == "--track-offset-y") { if (!need("track-offset-y")) return false; o.trackOffsetY = atof(argv[++i]); }
    else if (a == "--track-offset-z") { if (!need("track-offset-z")) return false; o.trackOffsetZ = atof(argv[++i]); }
    else if (a == "--track-score") { if (!need("track-score")) return false; o.trackScore = (float)atof(argv[++i]); }
    else if (a == "--track-suavizado") { if (!need("track-suavizado")) return false; o.trackSmoothing = atof(argv[++i]); }
    else if (a == "--align") { if (!need("align")) return false; o.align = atof(argv[++i]); }
    else if (a == "--pitch") { if (!need("pitch")) return false; o.pitch = atof(argv[++i]); }
    else if (a == "--calib-dir") { if (!need("calib-dir")) return false; o.corrDir = argv[++i]; }
    else if (a == "--uuid") { if (!need("uuid")) return false; o.captureUuid = argv[++i]; }
    else if (a == "--segundos") { if (!need("segundos")) return false; o.segundos = atoi(argv[++i]); }
    else if (a == "--help" || a == "-h") { std::fputs(kUsage, stdout); exit(0); }
    else if (!a.empty() && a[0] == '-') { std::fprintf(stderr, "opcion desconocida: %s\n", a.c_str()); return false; }
    else if (o.file.empty()) o.file = a;
    else { std::fprintf(stderr, "solo se admite un archivo; ignorando %s\n", a.c_str()); }
  }
  if (o.track && o.trackModel.empty()) o.trackModel = defaultModelPath();
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
    CornerAttrs a = w.fill(ndc[i][0], ndc[i][1]);
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
  std::vector<uint8_t> cpu;
  if (o.solid) {   /* mitad izquierda del archivo roja, derecha verde: la trama debe alternarlas */
    const uint8_t px[16] = {255, 0, 0, 255, 0, 255, 0, 255, 255, 0, 0, 255, 0, 255, 0, 255};
    tex = glTexFromRGBA(2, 2, px, false);
  } else if (o.cpuPattern) {   /* mismo patron que usa el camino Vulkan: la salida debe coincidir */
    makeTestSbs(cpu, W, H, 0.0, 34.0);
    tex = glTexFromRGBA(W, H, cpu.data(), true);
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

/* ---------------- prueba sin pantalla de la interfaz ---------------- */

static int cmdHudSelfTest(int argc, char **argv) {
  ViewerOpts o{};
  if (!parseViewerFlags(argc, argv, o)) return 2;
  Hud hud;
  std::string err;
  if (!hud.buildAtlas(&err)) {
    std::fprintf(stderr, "[interfaz] no se pudo armar el atlas: %s\n", err.c_str());
    return 1;
  }
  const int W = o.w > 0 ? o.w : 1920, H = o.h > 0 ? o.h : 1080;
  hud.state().ask = true;
  hud.setInfo("prueba de la interfaz");
  hud.begin(W, H);
  std::printf("[interfaz] ventana %dx%d · atlas %dx%d · texto %s · %d vertices\n", W, H, hud.atlasW(), hud.atlasH(),
              hud.hasText() ? "si" : "no", hud.vertexCount());
  static const char *nom[] = {"nada",              "boton 3D",   "boton capa",   "deslizador profundidad",
                              "deslizador salida", "boton red",  "tesela SxS",   "tesela arriba/abajo",
                              "tesela plana",     "boton video normal", "casilla no preguntar", "cerrar el aviso"};
  int cuenta[12] = {0}, px0[12] = {0}, py0[12] = {0};
  for (int y = 0; y < H; y += 2)
    for (int x = 0; x < W; x += 2) {
      int h = (int)hud.hit((float)x, (float)y);
      if (h < 0 || h > 11) continue;
      if (!cuenta[h]) {
        px0[h] = x;
        py0[h] = y;
      }
      cuenta[h]++;
    }
  for (int h = 1; h <= 11; h++)
    std::printf("  %-24s %6d celdas de raton · primer golpe (%d,%d)\n", nom[h], cuenta[h], px0[h], py0[h]);

  /* Rotulos de la pastilla, con la caja real donde se dibujan (ancho del boton menos el aire a cada
     lado). Si la letra mide mas que su caja se sale del recuadro, que es lo que pasaba antes. */
  struct Medida {
    const char *texto;
    float px, caja;
  };
  static const Medida med[] = {{"Apagar 3D", 15.0f, 126.0f},        {"Encender 3D", 15.0f, 126.0f},
                               {"Ocultar capa", 15.0f, 136.0f},     {"Mostrar capa", 15.0f, 136.0f},
                               {"Red 1 · Samsung", 14.0f, 136.0f},   {"Red 2 · nuestra", 14.0f, 136.0f},
                               {"Red sin conversor", 14.0f, 136.0f},      {"1 / 2  cambia de red", 11.0f, 160.0f},
                               {"teclas 1 / 2", 11.0f, 160.0f},           {"Tab  entrega la pantalla", 11.0f, 272.0f},
                               {"Ctrl+Shift+1", 11.0f, 150.0f},              {"Ctrl+Shift+3 / 4", 11.0f, 120.0f},
                               {"Ctrl+Shift+5 / 6", 11.0f, 120.0f},
                              {"clic fuera de la pastilla  manda sobre el video de atras", 11.0f, 650.0f}};
  for (const Medida &m : med) {
    const float w = hud.mideTexto(m.texto, m.px);
    std::printf("  letra  %-24s %4.0fpx %6.1f px de %5.0f  %s\n", m.texto, (double)m.px, (double)w, (double)m.caja,
                w <= m.caja ? "cabe" : "SE SALE");
  }

  const int d0 = hud.state().depth, p0 = hud.state().popout;
  hud.dragTo(Hud::HitDepth, (float)(W - 1));
  hud.dragTo(Hud::HitPopout, (float)(W - 1));
  std::printf("  arrastre a la derecha: profundidad %d -> %d · salida %d -> %d\n", d0, hud.state().depth, p0,
              hud.state().popout);
  hud.dragTo(Hud::HitDepth, 0.0f);
  hud.dragTo(Hud::HitPopout, 0.0f);
  std::printf("  arrastre a la izquierda: profundidad %d · salida %d\n", hud.state().depth, hud.state().popout);
  const bool w0 = hud.state().weaveOn;
  hud.toggle3D();
  std::printf("  boton 3D: tramado %s -> %s\n", w0 ? "activo" : "apagado", hud.state().weaveOn ? "activo" : "apagado");
  const bool o0 = hud.state().overlayOn;
  hud.toggleOverlay();
  std::printf("  boton capa: superposicion %s -> %s\n", o0 ? "visible" : "oculta",
              hud.state().overlayOn ? "visible" : "oculta");
  /* con la capa ocultada tiene que quedar un rincon pinchable en su sitio de siempre, si no, el que
     la oculto desde el video ya no tiene forma de traerla */
  float hx = 0.0f, hy = 0.0f, hw = 0.0f, hh = 0.0f;
  const bool rincon = hud.huecoDeLaPastilla(&hx, &hy, &hw, &hh);
  std::printf("  rincon para traer la pastilla de vuelta: %.0f,%.0f %.0fx%.0f  %s\n", (double)hx, (double)hy,
              (double)hw, (double)hh, (rincon && hw >= 40.0f && hh >= 20.0f) ? "vale" : "NO VALE");
  hud.showOverlay();
  std::printf("  la pastilla vuelve con la tecla PB: %s\n", hud.state().overlayOn ? "si" : "no");
  return 0;
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
  std::string pantalla;           /* dónde poner la tarjeta (DP-4); vacio = reconocer solo */
  std::string modelo;             /* .onnx de profundidad; vacio = la red de siempre */
  std::string nuestra;            /* la otra red (.onnx), la del boton 2 del visor */
  std::string modo = "ask";   /* ask = preguntar; sbs/tb/flat/conv = arrancar sin preguntar */
  int red = 0;                /* GPU donde vive la red que convierte video normal a 3D */
  int cada = 3;               /* cada cuantos cuadros se calcula la profundidad */
  std::map<std::string, std::string> porApp;   /* app_id -> modo, para no volver a preguntar */
};

/* Se añade al final del archivo, así se conservan los comentarios de arriba. */
static void guardarAutoEnDisco(const std::string &appId, const std::string &modo) {
  const char *home = std::getenv("HOME");
  if (!home || appId.empty()) return;
  const std::string dir = std::string(home) + "/.config/odyssey3d";
  ::mkdir(dir.c_str(), 0700);
  FILE *f = std::fopen((dir + "/serve.conf").c_str(), "ae");
  if (!f) return;
  std::fseek(f, 0, SEEK_END);
  std::fprintf(f, "auto = %s:%s\n", appId.c_str(), modo.c_str());
  std::fclose(f);
}

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

/* "~" al principio de una ruta de modelo, para poder escribirlo a mano en la configuracion. */
static std::string expansionRuta(std::string r) {
  const char *h = std::getenv("HOME");
  const std::string casa = h ? h : "";
  if (!casa.empty() && (r == "~" || (r.size() > 1 && r.compare(0, 2, "~/") == 0)))
    r = casa + r.substr(1);
  return r;
}

/* ~/.config/odyssey3d/serve.conf: lineas clave=valor (ignore=, espera=, modo=, pantalla=,
   modelo= la red del boton 1, nuestra= la del boton 2). */
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
    else if (k == "red") cfg.red = std::max(0, std::atoi(v.c_str()));
    else if (k == "cada") cfg.cada = std::max(1, std::atoi(v.c_str()));
    else if (k == "modo") cfg.modo = strLower(v);
    else if (k == "pantalla") cfg.pantalla = v;
    else if (k == "modelo") cfg.modelo = expansionRuta(v);
    else if (k == "nuestra") cfg.nuestra = expansionRuta(v);
    else if (k == "auto") {
      /* auto = mpv:sbs, haruna:tb  -> esas ventanas pasan directas, sin preguntar */
      std::vector<std::string> pares;
      splitList(v, pares);
      for (const std::string &par : pares) {
        size_t dd = par.find(':');
        if (dd == std::string::npos) continue;
        std::string app = strLower(strTrim(par.substr(0, dd))), md = strLower(strTrim(par.substr(dd + 1)));
        if (!app.empty() && (md == "sbs" || md == "tb" || md == "flat" || md == "conv")) cfg.porApp[app] = md;
      }
    }
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

static pid_t spawnViewer(const std::string &exe, const std::string &uuid, const std::string &modo, int red = 0,
                        int cada = 3, const std::string &modelo = std::string(),
                        const std::string &pantalla = std::string(),
                        const std::string &alterno = std::string()) {
  pid_t pid = ::fork();
  if (pid != 0) return pid;
  std::vector<std::string> args = {exe, "capture3d", "--uuid", uuid};
  if (!pantalla.empty()) { args.push_back("--output"); args.push_back(pantalla); }
  if (modo == "sbs") args.push_back("--sbs");
  if (modo == "tb") args.push_back("--tb");
  if (modo == "flat") args.push_back("--2d");
  if (modo == "conv") {
    /* video normal: la red de profundidad decide cuanto desplazar cada ojo */
    args.push_back("--conver");
    args.push_back("--cada");
    args.push_back(std::to_string(cada));
    args.push_back("--red");
    args.push_back(std::to_string(red));
    if (!modelo.empty()) {
      args.push_back("--modelo");
      args.push_back(modelo);
    }
    if (!alterno.empty()) {   /* la otra red, para el boton 1 / 2 del visor */
      args.push_back("--alterno");
      args.push_back(alterno);
    }
  }
  std::vector<char *> av;
  for (std::string &a : args) av.push_back(const_cast<char *>(a.c_str()));
  av.push_back(nullptr);
  ::setenv("GLFW_APPLICATION_ID", "odyssey3d", 1);
  ::execv(exe.c_str(), av.data());
  ::_exit(127);
}

#ifdef HAVE_WAYLAND_WM
/* Deja la tarjeta del hub encima de cualquier ventana, incluidas las de pantalla
   completa, y la deja visible un rato. Con --dump guarda el lienzo tal cual se pintó,
   así se puede revisar el dibujo sin mirar la pantalla. */
/* Mide a qué ritmo manda el compositor fotogramas nuevos de una ventana, sin abrir
   ninguna ventana nuestra. Sirve para separar dos cosas distintas: que el flujo del
   compositor se corte, frente a que la imagen se congele dentro de nuestra ventana. */
/* Orden para el compositor sobre una ventana del escritorio. */
static int cmdVentana(int argc, char **argv) {
  const char *act = argc > 0 ? argv[0] : "";
  std::string uuid = flag(argc, argv, "--uuid");
  WindowMonitor wm;
  std::string err;
  if (!wm.open(&err)) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  wm.settle();
  WindowInfo wi;
  std::string u = uuid;
  if (u.empty()) {
    if (!wm.fullscreenCandidate(wi, {})) {
      std::fprintf(stderr, "no hay ventana a pantalla completa (usa --uuid con `ctl windows`)\n");
      return 1;
    }
    u = wi.uuid;
  } else if (!wm.find(u, wi)) {
    std::fprintf(stderr, "no se encontró la ventana %s\n", u.c_str());
    return 1;
  }
  std::printf("ventana: %s | %s | %s uuid=%s\n", wi.appId.c_str(), wi.title.c_str(),
              WindowInfo::stateList(wi.states).c_str(), u.c_str());
  bool ok = false;
  if (!std::strcmp(act, "fullscreen")) ok = wm.setState(u, WindowInfo::Fullscreen, true, &err);
  else if (!std::strcmp(act, "normal")) ok = wm.setState(u, WindowInfo::Fullscreen, false, &err);
  else if (!std::strcmp(act, "maximizar")) ok = wm.setState(u, WindowInfo::Maximized, true, &err);
  /* aparecer: quitar la minimizada. Es lo que hace `ctl aparece` sobre la ventana del visor, y
     sirve para atar la tecla PB a un atajo global del escritorio. */
  else if (!std::strcmp(act, "aparecer")) ok = wm.setState(u, WindowInfo::Minimized, false, &err);
  else if (!std::strcmp(act, "minimizar")) ok = wm.setState(u, WindowInfo::Minimized, true, &err);
  /* sobre / debajo: el compositor no tiene orden para "traer al frente", pero si para dejar la
     ventana siempre por encima (KeepAbove). Es lo que evita que el visor vuelva DETRAS del video
     despues de que el gestor de ventanas lo minimizara. */
  else if (!std::strcmp(act, "sobre")) ok = wm.setState(u, WindowInfo::KeepAbove, true, &err);
  else if (!std::strcmp(act, "debajo")) {
    wm.setState(u, WindowInfo::KeepAbove, false, &err);
    ok = wm.setState(u, WindowInfo::KeepBelow, true, &err);
  } else if (!std::strcmp(act, "libre")) {
    wm.setState(u, WindowInfo::KeepAbove, false, &err);
    ok = wm.setState(u, WindowInfo::KeepBelow, false, &err);
  }
  /* salida DP-4: devolver la ventana a LA salida del Odyssey. KWin la coloca donde le place y la
     trama necesita pixel a pixel la salida del panel. */
  else if (!std::strcmp(act, "salida")) {
    std::string nom = flag(argc, argv, "--salida");
    if (nom.empty() && argc > 1) nom = argv[1];
    if (nom.empty()) {
      std::fprintf(stderr, "falta el nombre de la salida: ctl ventana salida DP-4 [--uuid ID]\n");
      for (const std::string &p : wm.outputNames()) std::fprintf(stderr, "  salida: %s\n", p.c_str());
      return 2;
    }
    ok = wm.sendToOutput(u, nom, &err);
  }
  else if (!std::strcmp(act, "cerrar")) ok = wm.close(u, &err);
  else {
    std::fprintf(stderr,
                 "uso: ctl ventana [fullscreen|normal|maximizar|minimizar|aparecer|sobre|debajo|libre|salida"
                 " NOMBRE|cerrar] [--uuid ID]\n");
    return 2;
  }
  if (!ok) {
    std::fprintf(stderr, "%s\n", err.c_str());
    return 1;
  }
  wm.settle(120, 12);
  WindowInfo desp;
  if (wm.find(u, desp))
    std::printf("estado ahora: %s\n", WindowInfo::stateList(desp.states).c_str());
  else
    std::printf("la ventana ya no está\n");
  return 0;
}

/* ctl aparece: la ventana del visor otra vez a la vista, tenga el foco quien lo tenga. Es la
   respuesta a la tecla PB cuando el que manda es el video: ninguna tecla nuestra le llega al
   visor, pero el hub si puede ordenarle al compositor que devuelva la ventana. Se puede atar a un
   atajo global del escritorio (Ajustes del sistema -> Atajos de teclado -> accion nueva) con el
   comando `odyssey3d ctl aparece`. */
static int cmdAparece(int, char **) {
  WindowMonitor wm;
  std::string err;
  if (!wm.open(&err)) {
    std::fprintf(stderr, "[aparece] %s\n", err.c_str());
    return 1;
  }
  wm.settle();
  WindowInfo nuestra{};
  bool encontrada = false;
  for (const WindowInfo &w : wm.windows()) {
    if (w.appId != "odyssey3d" || w.title != "Odyssey 3D") continue;  /* el visor, no la tarjeta */
    nuestra = w;
    encontrada = true;
  }
  if (!encontrada) {
    std::fprintf(stderr, "[aparece] no hay ninguna sesion del visor abierta\n");
    return 1;
  }
  if (!nuestra.minimized()) {
    std::printf("[aparece] la ventana del visor ya estaba a la vista (%s)\n",
                WindowInfo::stateList(nuestra.states).c_str());
    return 0;
  }
  if (!wm.setState(nuestra.uuid, WindowInfo::Minimized, false, &err)) {
    std::fprintf(stderr, "[aparece] %s\n", err.c_str());
    return 1;
  }
  /* KWin no activa la ventana al sacarla de minimizada, así que se queda detras del video: la
     ponemos por encima. (Este protocolo no trae orden de activar ventanas.) */
  wm.setState(nuestra.uuid, WindowInfo::KeepAbove, true, &err);
  /* Ademas la devolvemos a SU salida y a pantalla completa: KWin la coloca donde le place, y si
     cae en otra pantalla o le quita el estado de pantalla completa la trama deja de ser pixel a
     pixel y el panel sale con trozos sueltos. */
  ServeCfg cfg;
  loadServeCfg(cfg);
  if (!nuestra.fullscreen()) wm.setState(nuestra.uuid, WindowInfo::Fullscreen, true, &err);
  if (!cfg.pantalla.empty()) {
    if (!wm.sendToOutput(nuestra.uuid, cfg.pantalla, &err))
      std::fprintf(stderr, "[aparece] %s\n", err.c_str());
  }
  wm.settle(120, 12);
  WindowInfo desp;
  if (wm.find(nuestra.uuid, desp) && desp.minimized()) {
    std::fprintf(stderr, "[aparece] el compositor no quiso devolver la ventana: prueba con la barra de tareas\n");
    return 1;
  }
  std::printf("[aparece] la ventana del visor vuelve a la vista\n");
  return 0;
}

/* ctl apilamiento: orden de apilamiento que tiene el compositor, de abajo hacia arriba (o al
   reves: se comprueba con la ventana que uno sabe que esta al frente). Es la unica forma de
   comprobar de verdad si el visor quedo por encima del video. */
static int cmdApilamiento(int, char **) {
  WindowMonitor wm;
  std::string err;
  if (!wm.open(&err)) {
    std::fprintf(stderr, "[apilamiento] %s\n", err.c_str());
    return 1;
  }
  wm.settle();
  std::vector<std::string> orden;
  if (!wm.stackingOrder(&orden, &err)) {
    std::fprintf(stderr, "[apilamiento] %s\n", err.c_str());
    return 1;
  }
  /* Medido en esta maquina con KWin 6.8: la lista va de ABAJO hacia ARRIBA. Con `ctl ventana sobre`
     la ventana salta al ultimo puesto y con `libre` vuelve a su hueco, y el fondo de escritorio es
     siempre el primero. */
  std::printf("[apilamiento] %zu ventanas, de abajo (1) hacia arriba (%zu), en el orden que manda el compositor\n",
              orden.size(), orden.size());
  for (size_t i = 0; i < orden.size(); ++i) {
    WindowInfo w{};
    const bool sab = wm.find(orden[i], w);
    std::printf("  %2zu  %-34s  %-20s  %-24s  %s\n", i + 1, orden[i].c_str(),
                sab ? w.appId.c_str() : "?", sab ? WindowInfo::stateList(w.states).c_str() : "?",
                sab ? w.title.c_str() : "?");
  }
  return 0;
}

/* ctl salidas: nombres y geometricas de las salidas, para saber donde esta cada monitor en el
   espacio global del compositor (la posicion que despues reports las ventanas). */
static int cmdSalidas(int, char **) {
  WindowMonitor wm;
  std::string err;
  if (!wm.open(&err)) {
    std::fprintf(stderr, "[salidas] %s\n", err.c_str());
    return 1;
  }
  wm.settle();
  std::vector<WindowMonitor::Salida> salidas = wm.outputs();
  std::printf("[salidas] %zu salidas (posicion y tamano logicos en el espacio global)\n", salidas.size());
  for (const WindowMonitor::Salida &s : salidas)
    std::printf("  %-10s %d,%d  %dx%d\n", s.name.c_str(), s.x, s.y, s.w, s.h);
  return 0;
}

/* ctl video [status|pause|play|toggle] · pausar y reanudar el video sin quitar la capa 3D.
   Zen, Firefox y Chrome publican un reproductor MPRIS en la sesion, y ese es el unico camino
   que queda mientras el visor se queda con el teclado y el raton de la pantalla. */
static int cmdVideo(int argc, char **argv) {
  std::string accion = "status";
  if (argc > 0 && argv[0] && argv[0][0] != '-') accion = argv[0];
  Mpris m;
  std::string err;
  if (!m.open(&err)) {
    std::fprintf(stderr, "[video] %s\n", err.c_str());
    return 1;
  }
  std::string st;
  if (accion == "status") {
    if (!m.status(&st, &err)) {
      std::fprintf(stderr, "[video] %s\n", err.c_str());
      return 1;
    }
    std::printf("[video] %s · %s\n", m.playerName().c_str(), st.c_str());
    return 0;
  }
  if (accion == "toggle") {
    if (!m.playPause(&err)) {
      std::fprintf(stderr, "[video] %s\n", err.c_str());
      return 1;
    }
    m.status(&st);
    std::printf("[video] %s · %s\n", m.playerName().c_str(), st.empty() ? "cambiado" : st.c_str());
    return 0;
  }
  if (accion == "play" || accion == "reanudar") {
    if (!m.status(&st, &err)) {
      std::fprintf(stderr, "[video] %s\n", err.c_str());
      return 1;
    }
    if (st == "Playing") {
      std::printf("[video] ya estaba en marcha (%s)\n", m.playerName().c_str());
      return 0;
    }
    if (!m.resumeIfNeeded(&err)) {
      std::fprintf(stderr, "[video] %s\n", err.c_str());
      return 1;
    }
    std::printf("[video] en marcha (%s)\n", m.playerName().c_str());
    return 0;
  }
  if (accion == "pause" || accion == "pausa") {
    if (m.status(&st) && st != "Playing") {
      std::printf("[video] ya estaba parado (%s)\n", st.c_str());
      return 0;
    }
    if (!m.playPause(&err)) {
      std::fprintf(stderr, "[video] %s\n", err.c_str());
      return 1;
    }
    std::printf("[video] en pausa (%s)\n", m.playerName().c_str());
    return 0;
  }
  std::fprintf(stderr, "accion desconocida: %s (status|pause|play|toggle)\n", accion.c_str());
  return 2;
}

static int cmdFlujo(int argc, char **argv) {
#ifndef HAVE_CAPTURE
  (void)argc; (void)argv;
  std::fprintf(stderr, "compilado sin soporte de captura (hace falta libpipewire-0.3 y plasma-wayland-protocols)\n");
  return 1;
#else
  int secs = 12;
  if (std::string v = flag(argc, argv, "--segundos"); !v.empty()) secs = std::max(2, std::atoi(v.c_str()));
  std::string uuid;
  WindowInfo win;
  bool haveWin = false;
  if (!pickWindow(argc, argv, uuid, win, haveWin)) return 1;

  std::signal(SIGTERM, onServeSignal);
  std::signal(SIGINT, onServeSignal);

  std::string err;
  WindowCapture cap;
  std::string cls = flag(argc, argv, "--clase");
  if (!cls.empty()) cap.setStreamClass(cls);
  if (!cap.start(uuid, &err)) {
    std::fprintf(stderr, "[flujo] %s\n", err.c_str());
    return 1;
  }
  std::printf("[flujo] midiendo %s | %s\n", win.appId.c_str(), win.title.c_str());
  std::fflush(stdout);

  auto reloj = [] {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
  };
  const double inicio = reloj();
  double ultimo = inicio;
  uint64_t previo = 0;
  while (!g_stopServe && reloj() - inicio < (double)secs) {
    struct pollfd pfd{cap.fd(), POLLIN, 0};
    ::poll(&pfd, 1, 100);
    cap.pump();
    double ahora = reloj();
    if (ahora - ultimo >= 1.0) {
      const uint64_t n = cap.frameCount();
      std::printf("[flujo] %5.1f s  acumulado %6llu  ultimo segundo %4llu  flujo %s\n", ahora - inicio,
                  (unsigned long long)n, (unsigned long long)(n - previo), cap.stateText().c_str());
      if (!cap.streamCreated()) std::printf("[flujo] still: %s\n", cap.describe().c_str());
      std::fflush(stdout);
      previo = n;
      ultimo = ahora;
    }
  }
  const uint64_t total = cap.frameCount();
  const double dur = reloj() - inicio;
  std::printf("[flujo] %llu fotogramas en %.1f s = %.1f fotogramas por segundo\n", (unsigned long long)total, dur,
              dur > 0 ? (double)total / dur : 0.0);
  cap.stop();
  return total > 0 ? 0 : 1;
#endif
}

static int cmdOverlay(int argc, char **argv) {
  int segundos = 12;
  if (std::string v = flag(argc, argv, "--segundos"); !v.empty()) segundos = std::max(1, std::atoi(v.c_str()));
  const std::string volcado = flag(argc, argv, "--dump");
  std::string titulo = "¿Convertir el vídeo actual a 3D?";
  std::string cuerpo = "Elige el formato de la imagen que estás viendo.";
  if (std::string v = flag(argc, argv, "--titulo"); !v.empty()) titulo = v;
  if (std::string v = flag(argc, argv, "--cuerpo"); !v.empty()) cuerpo = v;

  std::signal(SIGTERM, onServeSignal);
  std::signal(SIGINT, onServeSignal);

  const std::string pista = flag(argc, argv, "--pantalla");
  Overlay ov;
  if (!pista.empty()) ov.setPantalla(pista);
  std::string err;
  if (!ov.open(&err)) {
    std::fprintf(stderr, "[capa] %s\n", err.c_str());
    return 1;
  }
  std::printf("[capa] %s · escala %.2f%s\n", ov.outputName().c_str(), ov.scale(),
              ov.pantallaReconocida() ? "" : "  (no reconocido como el Odyssey)");
  for (const std::string &p : ov.pantallas()) std::printf("[capa] pantalla anunciada: %s\n", p.c_str());
  std::fflush(stdout);
  ov.showAsk(titulo, cuerpo);
  if (!ov.askVisible()) {
    std::fprintf(stderr, "[capa] el compositor no puso la tarjeta: %s\n", ov.lastError().c_str());
    return 1;
  }
  std::printf("[capa] tarjeta puesta %d s · mueve el ratón sobre los botones\n", segundos);
  std::fflush(stdout);

  auto reloj = [] {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
  };
  const double inicio = reloj();
  bool volcadoHecho = false;
  while (!g_stopServe && reloj() - inicio < (double)segundos) {
    ov.waitAndDispatch(100);
    if (ov.corrigeColocacion() && ov.hayAvisoColocacion())
      std::printf("[capa] %s\n", ov.lastError().c_str());
    if (std::string a = ov.takeAction(); !a.empty()) {
      std::printf("[capa] elegido: %s\n", a.c_str());
      std::fflush(stdout);
      if (a != "close") break;
    }
    if (!volcadoHecho && !volcado.empty() && reloj() - inicio > 1.0) {
      if (ov.dumpPng(volcado, &err))
        std::printf("[capa] lienzo guardado en %s\n", volcado.c_str());
      else
        std::fprintf(stderr, "[capa] %s\n", err.c_str());
      volcadoHecho = true;
    }
  }
  ov.hideAsk();
  return 0;
}
#endif

/* El visor no puede ver lo que hace el gestor de ventanas con su propia ventana: ese protocolo solo
   lo admite un cliente a la vez y lo tiene el hub. Se lo contamos por un archivo en
   ~/.local/state/odyssey3d con la hora del reloj monotono; caduca, asi que si el hub se va el visor
   vuelve a dibujar por su cuenta. Con "oculta" el visor deja de dibujar y de pedir fotogramas, y al
   pasar a "visible" reengancha la captura, que es lo que quita los trozos mal colocados. */
static void escribirMarcaVentana(const char *estado, double ahora) {
  std::ofstream os(rutaEstadoOdyssey("ventana"), std::ios::trunc);
  if (os) os << estado << ' ' << ahora << "\n";
}
static void borrarMarcaVentana() {
  std::error_code ec;
  std::filesystem::remove(rutaEstadoOdyssey("ventana"), ec);
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
  if (std::string v = flag(argc, argv, "--red"); !v.empty()) cfg.red = std::max(0, std::atoi(v.c_str()));
  if (std::string v = flag(argc, argv, "--cada"); !v.empty()) cfg.cada = std::max(1, std::atoi(v.c_str()));
  if (std::string v = flag(argc, argv, "--modo"); !v.empty()) cfg.modo = strLower(v);
  if (std::string v = flag(argc, argv, "--pantalla"); !v.empty()) cfg.pantalla = v;
  if (std::string v = flag(argc, argv, "--modelo"); !v.empty()) cfg.modelo = expansionRuta(v);
  if (std::string v = flag(argc, argv, "--nuestra"); !v.empty()) cfg.nuestra = expansionRuta(v);
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
  /* La tarjeta propia va por encima del video a pantalla completa; el portal de
     avisos de KDE queda debajo, así que sólo lo usamos de respaldo. */
  /* La tarjeta vive en su propia conexion de Wayland. Si el compositor la cierra (paso un
     momento malo con el tamano del lienzo), la tarjeta deja de salir para siempre: se vuelve
     a abrir sola para que el aviso de pantalla completa no se pierda. */
  std::unique_ptr<Overlay> ov;
  auto abrirTarjeta = [&]() {
    std::unique_ptr<Overlay> nueva(new Overlay());
    if (!cfg.pantalla.empty()) nueva->setPantalla(cfg.pantalla);
    std::string e2;
    if (!nueva->open(&e2)) {
      std::fprintf(stderr, "[serve] sin capa superpuesta: %s\n", e2.c_str());
      std::fflush(stderr);
      return false;
    }
    ov = std::move(nueva);
    return true;
  };
  bool hayTarjeta = abrirTarjeta();
  if (hayTarjeta) {
    std::printf("[serve] tarjeta superpuesta en %s (escala %.2f)\n", ov->outputName().c_str(), ov->scale());
    if (!ov->pantallaReconocida())
      std::printf("[serve] OJO: esa no parece la pantalla del Odyssey. Fija cual es con "
                  "pantalla=DP-4 en ~/.config/odyssey3d/serve.conf\n");
  }
  else
    std::fprintf(stderr, "[serve] sin capa superpuesta: %s\n", err.c_str());
  Notifier nt;
  bool hayAvisos = hayTarjeta ? false : nt.open(&err);
  if (!hayTarjeta && !hayAvisos) std::fprintf(stderr, "[serve] sin portal de avisos: %s\n", err.c_str());
  if (cfg.modo == "ask" && !hayAvisos && !hayTarjeta) {
    std::fprintf(stderr, "[serve] para preguntar hace falta la capa superpuesta o las notificaciones; usa --modo sbs|tb|flat\n");
    return 1;
  }

  const std::string exe = selfPath();
  std::map<std::string, std::string> idUuid;   /* id del aviso -> uuid de la ventana */
  std::set<std::string> preguntadas;           /* ventanas por las que ya preguntamos o actuamos */
  std::string avisoId, avisoUuid, accion;      /* aviso pendiente y respuesta del usuario */
  double avisoT = 0.0;
  std::string tarjetaUuid, tarjetaApp;         /* ventana a la que le pusimos la tarjeta */
  double tarjetaT = 0.0;
  pid_t hijo = -1;
  std::string hijoUuid;
  double intentoAparecer = 0.0;   /* cuando intentamos por ultima vez devolver la ventana minimizada */
  bool avisoAparecerHecho = false;
  double intentoEncima = 0.0;     /* cuando reforzamos el "encima de todo" del visor */
  bool avisoEncimaHecho = false;
  double intentoVideo = 0.0;      /* cuando devolvimos el video a su sitio por ultima vez */

  nt.setOnAction([&](const std::string &id, const std::string &action) {
    auto it = idUuid.find(id);
    if (it == idUuid.end()) return;
    accion = strLower(action);
    avisoUuid = it->second;
    std::printf("[serve] respuesta %s -> %s\n", action.c_str(), it->second.c_str());
    std::fflush(stdout);
  });

  /* Las dos redes del boton 1 / 2 del visor, cada una en su boton segun el nombre del archivo:
     1 = las que trajo el programa de Samsung (mde_*, Disp_*), 2 = la nuestra (Depth Anything).
     Si en modelo= se puso la nuestra se reconoce y se mueve sola al boton 2. La eleccion que hizo
     el usuario la guarda el visor en ~/.config/odyssey3d/red-elegida y aca se lee para arrancar con esa. */
  auto disponible = [](const std::string &r) { return !r.empty() && fileExists(r); };
  std::string rSamsung = cfg.modelo, rNuestra = cfg.nuestra.empty() ? rutaRedNuestra() : cfg.nuestra;
  if (!rSamsung.empty() && slotRed(rSamsung) == 2) {
    if (rSamsung == rNuestra || !disponible(rNuestra)) {
      rNuestra = rSamsung;
      rSamsung.clear();
    } else
      std::swap(rSamsung, rNuestra);
  }
  const std::string ruta1 = disponible(rSamsung) ? rSamsung : std::string();
  const std::string ruta2 = disponible(rNuestra) ? rNuestra : std::string();
  int redElegida = leerRedPreferida();
  if (!redElegida) redElegida = ruta1.empty() ? 2 : 1;   /* sin preferencia: la de Samsung si esta */
  if (redElegida == 1 && ruta1.empty()) redElegida = 2;
  if (redElegida == 2 && ruta2.empty()) redElegida = 1;
  const std::string redActiva = redElegida == 1 ? ruta1 : ruta2;
  const std::string redOtra = redElegida == 1 ? ruta2 : ruta1;
  std::printf("[serve] redes de profundidad: boton 1 = %s · boton 2 = %s\n",
              ruta1.empty() ? "no hay" : ruta1.c_str(), ruta2.empty() ? "no hay" : ruta2.c_str());
  std::printf("[serve]   arrancamos con la %d (%s); en el visor se cambia con las teclas 1 y 2 o con el boton\n",
              redElegida, redElegida == 1 ? "Samsung" : "nuestra");
  std::printf("[serve] vigilando ventanas a pantalla completa (%s) · modo=%s · espera=%d s\n",
              dry ? "sin ejecutar, --dry" : "ejecutando", cfg.modo.c_str(), cfg.esperaSeg);
  std::fflush(stdout);

  while (!g_stopServe) {
    if (hayTarjeta) {
      /* dos conexiones de Wayland abiertas: ninguna puede bloquear a la otra */
      struct pollfd ojea[2] = {{wm.fd(), POLLIN, 0}, {ov->fd(), POLLIN, 0}};
      ::poll(ojea, 2, 200);
      wm.waitAndDispatch(0);
      ov->waitAndDispatch(0);
      /* el compositor a veces coloca la capa en otra pantalla: se comprueba y se rehace */
      if (ov->corrigeColocacion() && ov->hayAvisoColocacion()) {
        std::printf("[serve] %s\n", ov->lastError().c_str());
        std::printf("[serve] la tarjeta vuelve a intentarse en %s\n", ov->outputName().c_str());
        std::fflush(stdout);
      }
      if (ov->conexionCaida()) {
        std::printf("[serve] %s\n", ov->lastError().c_str());
        ov.reset();                       /* se suelta la conexion muerta */
        hayTarjeta = abrirTarjeta();
        if (hayTarjeta)
          std::printf("[serve] capa superpuesta reabierta en %s\n", ov->outputName().c_str());
        std::fflush(stdout);
      }
    } else {
      wm.waitAndDispatch(500);
    }
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

    /* Si el gestor de ventanas minimiza la ventana del visor (Meta+F9, un clic en la barra de
       tareas, un gesto del touchpad) la trama desaparece, pero el monitor se queda en modo 3D y ya
       nadie puede mandar sobre la pastilla: la devolvemos nosotros. Al devolverla KWin no la
       activa (se queda DETRAS del video) y la coloca en la salida que le place: por eso pedimos
       tambien el "encima de todo", la pantalla completa y la salida del panel. La unica
       minimizacion que dejamos en paz es la nuestra, la de Tab, que el visor avisa escribiendo
       ~/.local/state/odyssey3d/entregada con su numero de proceso. */
    if (hijo > 0) {
      const std::string panelSalida =
          !cfg.pantalla.empty() ? cfg.pantalla : (hayTarjeta && ov ? ov->outputName() : std::string());
      WindowInfo visor{};
      bool hayVisor = false;
      for (const WindowInfo &w : wm.windows()) {
        if (w.appId != "odyssey3d" || w.title != "Odyssey 3D") continue;   /* el visor, no la tarjeta */
        visor = w;
        hayVisor = true;
      }
      if (hayVisor) {
        std::string marca;
        bool nuestra = readSmallFile(rutaEstadoOdyssey("entregada"), marca);
        if (nuestra) {
          /* el aviso solo vale mientras el proceso que lo escribio siga vivo: un portazo a mitad
             no puede dejar al hub con la ventana escondida para siempre */
          const long pid = std::atol(marca.c_str());
          nuestra = pid > 0 && ::kill((pid_t)pid, 0) == 0;
        }
        if (visor.minimized() && !nuestra) {
          escribirMarcaVentana("oculta", now);   /* el visor deja de dibujar mientras este oculto */
          if (now - intentoAparecer >= 2.0) {
            intentoAparecer = now;
            std::string e3;
            const bool pudo = wm.setState(visor.uuid, WindowInfo::Minimized, false, &e3);
            /* sin activarla (no esta en este protocolo) KWin la coloca detras del video: encima fijo */
            wm.setState(visor.uuid, WindowInfo::KeepAbove, true, &e3);
            /* y de vuelta a SU salida, a pantalla completa: si la deja en el otro monitor la trama
               no casa pixel a pixel y el panel sale con graficos rotos */
            if (!visor.fullscreen()) wm.setState(visor.uuid, WindowInfo::Fullscreen, true, &e3);
            if (!panelSalida.empty() && !wm.sendToOutput(visor.uuid, panelSalida, &e3) && !avisoEncimaHecho) {
              avisoEncimaHecho = true;
              std::printf("[ventana] %s\n", e3.c_str());
            }
            if (!avisoAparecerHecho) {
              avisoAparecerHecho = true;
              std::printf("[ventana] %s\n", pudo
                  ? "el gestor de ventanas minimizo el visor: la devolvemos por encima del video y en la"
                    " salida del panel. Para estar aparte usa Tab (y ESC para salir del todo)"
                  : ("el visor esta minimizado y el compositor no lo devolvio: " + e3).c_str());
              std::fflush(stdout);
            }
          }
        } else {
          escribirMarcaVentana("visible", now);
          avisoAparecerHecho = false;
          /* por si el compositor perdio el "encima" o el "pantalla completa" al recolocar la ventana */
          if (!visor.minimized() && now - intentoEncima >= 5.0 &&
              (!(visor.states & WindowInfo::KeepAbove) || !visor.fullscreen())) {
            intentoEncima = now;
            std::string e3;
            if (!visor.fullscreen()) wm.setState(visor.uuid, WindowInfo::Fullscreen, true, &e3);
            if (!wm.setState(visor.uuid, WindowInfo::KeepAbove, true, &e3) && !avisoEncimaHecho) {
              avisoEncimaHecho = true;
              std::printf("[ventana] el compositor no quiso poner el visor por encima: %s\n", e3.c_str());
              std::fflush(stdout);
            }
          }
        }
      }
      /* El otro lado de la trama es el video: si el navegador pierde la pantalla completa o KWin lo
         recoloca en otra salida, el fotograma deja de medir lo mismo que el lienzo y el 3D sale con
         trozos sueltos. Lo devolvemos a su sitio y avisamos. */
      if (!hijoUuid.empty()) {
        WindowInfo video{};
        if (wm.find(hijoUuid, video) && video.mapped && !video.minimized()) {
          bool fuera = !video.fullscreen();
          if (!fuera && !panelSalida.empty()) {
            for (const WindowMonitor::Salida &s : wm.outputs()) {
              if (s.name != panelSalida || s.w <= 0 || s.h <= 0) continue;
              const int cx = video.x + (int)video.w / 2, cy = video.y + (int)video.h / 2;
              fuera = (cx < s.x || cx >= s.x + s.w || cy < s.y || cy >= s.y + s.h);
              break;
            }
          }
          if (fuera && now - intentoVideo >= 5.0) {
            intentoVideo = now;
            std::string e3;
            wm.setState(hijoUuid, WindowInfo::Fullscreen, true, &e3);
            if (!panelSalida.empty()) wm.sendToOutput(hijoUuid, panelSalida, &e3);
            std::printf("[ventana] el video (%s) esta en %d,%d %ux%u con %s: lo devolvemos a pantalla"
                        " completa en %s\n", hijoUuid.c_str(), video.x, video.y, video.w, video.h,
                        WindowInfo::stateList(video.states).c_str(),
                        panelSalida.empty() ? "la salida del panel" : panelSalida.c_str());
            std::fflush(stdout);
          }
        }
      }
    } else {
      borrarMarcaVentana();
    }

    /* el aviso caduca: si nadie pulsa, se quita y no se vuelve a preguntar por esa ventana */
    /* lo mismo con la tarjeta: si nadie pulsa, se retira */
    if (!tarjetaUuid.empty()) {
      /* La tarjeta se retira solo si ESA ventana en concreto dejó de estar a pantalla
         completa. Antes se volvía a preguntar por "la mejor candidata": en cuanto la
         tarjeta aparecía el navegador dejaba de ser la ventana activa un instante, la
         candidatura cambiaba de ventana, la tarjeta se retiraba sola a los pocos
         segundos y volvía a salir (una oscilación que no dejaba tiempo de pulsar). */
      WindowInfo actual{};
      bool sigue = wm.find(tarjetaUuid, actual) && actual.usable();
      if (!sigue) {
        std::printf("[serve] %s ya no está a pantalla completa (%s), se quita la tarjeta\n",
                    tarjetaUuid.c_str(),
                    actual.uuid.empty() ? "desapareció" : WindowInfo::stateList(actual.states).c_str());
        std::fflush(stdout);
        ov->hideAsk();
        preguntadas.erase(tarjetaUuid);   /* si vuelve a pantalla completa, se vuelve a ofrecer */
        tarjetaUuid.clear();
        tarjetaApp.clear();
      }
    }

    if (!tarjetaUuid.empty() && now - tarjetaT > (double)cfg.esperaSeg) {
      std::printf("[serve] tarjeta sin respuesta, se retira (%s)\n", tarjetaUuid.c_str());
      std::fflush(stdout);
      ov->hideAsk();
      tarjetaUuid.clear();
    }

    if (hayTarjeta) {
      if (std::string acto = ov->takeAction(); !acto.empty()) {
        ov->hideAsk();
        if (acto == "close") {
          std::printf("[serve] tarjeta cerrada sin elegir (%s)\n", tarjetaUuid.c_str());
        } else {
          accion = acto;
          avisoUuid = tarjetaUuid;
          std::printf("[serve] tarjeta -> %s (%s)\n", acto.c_str(), avisoUuid.c_str());
          if (ov->remember() && !tarjetaApp.empty() && acto != "flat") {
            cfg.porApp[tarjetaApp] = acto;
            guardarAutoEnDisco(tarjetaApp, acto);
            std::printf("[serve] apuntado: %s pasa directo a %s sin preguntar\n", tarjetaApp.c_str(), acto.c_str());
          }
        }
        tarjetaApp.clear();
        ov->setRemember(false);
        tarjetaUuid.clear();
        std::fflush(stdout);
      }
    }

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
      if (act == "flat") {
        /* 2D plano NO toca el panel, la lente ni el scaler: si había una sesión 3D abierta,
           se cierra para devolver el monitor a 2D; si no, no se hace nada. */
        if (hijo > 0) {
          std::printf("[serve] 2D plano: cerrando la sesion 3D en curso (%s)\n", hijoUuid.c_str());
          ::kill(hijo, SIGTERM);
        } else {
          std::printf("[serve] 2D plano: se deja el video normal, sin lente ni scaler ni cambio de modo\n");
        }
        std::fflush(stdout);
      } else if (act != "no" && act != "cancel" && act != "close" && hijo < 0) {
        if (dry) {
          std::printf("[serve] (dry) habria lanzado capture3d --uuid %s modo=%s\n", avisoUuid.c_str(), act.c_str());
          std::fflush(stdout);
          if (unaVez) break;
        } else {
          hijo = spawnViewer(exe, avisoUuid, act, cfg.red, cfg.cada, redActiva, cfg.pantalla, redOtra);
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
        const std::string app = strLower(w.appId);
        auto fija = cfg.porApp.find(app);
        if (fija != cfg.porApp.end() && fija->second == "flat") {
          std::printf("[serve] %s esta marcada como 2D plano: no se hace nada\n", app.c_str());
          std::fflush(stdout);
        } else if (fija != cfg.porApp.end()) {
          if (dry) {
            std::printf("[serve] (dry) %s ya está ajustada a %s, no se pregunta\n", app.c_str(), fija->second.c_str());
            std::fflush(stdout);
            if (unaVez) break;
          } else {
            hijo = spawnViewer(exe, w.uuid, fija->second, cfg.red, cfg.cada, redActiva, cfg.pantalla, redOtra);
            hijoUuid = w.uuid;
            std::printf("[serve] lanzando 3D (%s) sin preguntar por %s\n", fija->second.c_str(), app.c_str());
            std::fflush(stdout);
            if (hijo > 0 && unaVez) break;
          }
        } else if (cfg.modo == "ask" && hayTarjeta) {
          std::string titulo = "¿Convertir el vídeo actual a 3D?";
          std::string cuerpo = w.title.empty() ? w.appId : w.title;
          cuerpo += "  ·  " + std::to_string(w.w) + "x" + std::to_string(w.h);
          ov->showAsk(titulo, cuerpo);
          tarjetaUuid = w.uuid;
          tarjetaApp = app;
          tarjetaT = now;
          std::printf("[serve] tarjeta puesta por %s | %s (%s) · pantalla %s\n", w.appId.c_str(),
                      w.title.c_str(), w.uuid.c_str(), ov->outputName().c_str());
          if (!ov->askVisible())
            std::printf("[serve] el compositor no llego a poner la tarjeta: %s\n", ov->lastError().c_str());
          std::fflush(stdout);
        } else if (cfg.modo == "ask") {
          std::string id = "odyssey3d-" + w.uuid.substr(0, 8);
          std::vector<std::pair<std::string, std::string>> botones = {
              {"sbs", "3D lado a lado"}, {"tb", "3D arriba y abajo"}, {"conv", "Vídeo normal a 3D"},
              {"flat", "2D plano"}};
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
        } else if (cfg.modo == "flat") {
          std::printf("[serve] 2D plano: sin lente, scaler ni cambio de modo (%s)\n", w.appId.c_str());
          std::fflush(stdout);
          if (unaVez) break;
        } else if (dry) {
          std::printf("[serve] (dry) habria lanzado modo %s por %s | %s\n", cfg.modo.c_str(), w.appId.c_str(),
                      w.title.c_str());
          std::fflush(stdout);
          if (unaVez) break;
        } else {
          hijo = spawnViewer(exe, w.uuid, cfg.modo, cfg.red, cfg.cada, redActiva, cfg.pantalla, redOtra);
          hijoUuid = w.uuid;
          std::printf("[serve] lanzando 3D (%s) por %s | %s\n", cfg.modo.c_str(), w.appId.c_str(), w.title.c_str());
          std::fflush(stdout);
          if (hijo > 0 && unaVez) break;
        }
      }
    }
  }

  if (!avisoId.empty()) nt.remove(avisoId);
  if (hayTarjeta) ov->hideAsk();
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
    if (sub == "calib") return cmdCalib(argc - 3, argv + 3);
    if (sub == "camera") return cmdCamera(argc - 3, argv + 3);
#ifdef HAVE_OPENCV
    if (sub == "track") return cmdTrack(argc - 3, argv + 3);
#endif
    if (sub == "panel") return cmdPanel(argc - 3, argv + 3);
    if (sub == "windows") return cmdWindows(argc - 3, argv + 3);
    if (sub == "notify") return cmdNotify(argc - 3, argv + 3);
#ifdef HAVE_WAYLAND_WM
    if (sub == "video") return cmdVideo(argc - 3, argv + 3);
    if (sub == "flujo") return cmdFlujo(argc - 3, argv + 3);
    if (sub == "ventana") return cmdVentana(argc - 3, argv + 3);
    if (sub == "aparece") return cmdAparece(argc - 3, argv + 3);
    if (sub == "apilamiento") return cmdApilamiento(argc - 3, argv + 3);
    if (sub == "salidas") return cmdSalidas(argc - 3, argv + 3);
    if (sub == "overlay") return cmdOverlay(argc - 3, argv + 3);
#endif
    if (sub == "capture") return cmdCapture(argc - 3, argv + 3);
    if (sub == "capture-gl") return cmdCaptureGl(argc - 3, argv + 3);
    if (sub == "vkprobe") return cmdVkProbe(argc - 3, argv + 3);
    if (sub == "hud") return cmdHudSelfTest(argc - 3, argv + 3);
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
    return hasFlag(argc - 2, argv + 2, "--gl") ? runViewer(o, SourceKind::Capture)
                                               : runViewerVk(o, SourceKind::Capture);
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
    return hasFlag(argc - 2, argv + 2, "--gl") ? runViewer(o, SourceKind::Test)
                                               : runViewerVk(o, SourceKind::Test);
  }
  std::fputs(kUsage, stderr);
  return 2;
}
